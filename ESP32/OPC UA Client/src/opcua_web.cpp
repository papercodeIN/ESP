/**
 * @file opcua_web.cpp
 * @brief HTTP configuration UI and JSON API.
 */
#include "opcua_web.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "opcua_client.h"
#include "opcua_config.h"
#include "opcua_settings.h"
#include "web_page.h"

namespace webui {

namespace {

WebServer g_server(OPC_WEB_PORT);
bool g_started = false;
uint32_t g_restartAtMs = 0;

/* ---------------------------------------------------------------------- */
/* Helpers                                                                */
/* ---------------------------------------------------------------------- */

void copyText(char *destination, size_t capacity, const char *source)
{
    if (!source) {
        source = "";
    }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

String jsonEscape(const char *text)
{
    String out;
    out.reserve(strlen(text) + 8);
    for (const char *p = text; *p; ++p) {
        char c = *p;
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<uint8_t>(c) < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04X", c);
                out += buf;
            } else {
                out += c;
            }
        }
    }
    return out;
}

String masked(const char *secret)
{
    return (secret && secret[0]) ? String("********") : String("");
}

String stateName()
{
    using opcua::State;
    switch (opcua::state()) {
    case State::Idle:      return "Idle";
    case State::Connecting: return "Connecting";
    case State::Connected: return "Connected";
    case State::Error:     return "Error";
    }
    return "Unknown";
}

/* ---------------------------------------------------------------------- */
/* API handlers                                                           */
/* ---------------------------------------------------------------------- */

void handleStatus()
{
    const settings::Settings &s = settings::get();
    bool station = app::mode() == app::Mode::Station;
    String ssidShown = station ? String(s.wifiSsid) : app::apSsid();

    String json = "{";
    json += "\"mode\":\"" + String(station ? "station" : "setup-ap") + "\"";
    json += ",\"ssid\":\"" + jsonEscape(ssidShown.c_str()) + "\"";
    json += ",\"hostname\":\"" + jsonEscape(s.hostname) + "\"";
    json += ",\"ip\":\"" + app::ipAddress() + "\"";
    json += ",\"rssi\":" + String(station && WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
    json += ",\"uptime\":" + String(millis() / 1000);
    json += ",\"heap\":" + String(ESP.getFreeHeap());
    json += ",\"connected\":" + String(station && WiFi.status() == WL_CONNECTED ? "true" : "false");
    json += ",\"clientState\":\"" + stateName() + "\"";
    json += ",\"serverUrl\":\"" + jsonEscape(s.serverUrl) + "\"";
    json += ",\"lastError\":\"" + jsonEscape(opcua::lastError().c_str()) + "\"";
    json += ",\"pollMs\":" + String(s.pollMs);
    json += ",\"readsOk\":" + String(opcua::readsOk());
    json += ",\"readsFailed\":" + String(opcua::readsFailed());
    json += ",\"lastReadAt\":" + String(opcua::lastReadAt());
    json += "}";

    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", json);
}

void handleGetSettings()
{
    const settings::Settings &s = settings::get();
    String json = "{";
    json += "\"wifiSsid\":\"" + jsonEscape(s.wifiSsid) + "\"";
    json += ",\"wifiPass\":\"" + masked(s.wifiPass) + "\"";
    json += ",\"hostname\":\"" + jsonEscape(s.hostname) + "\"";
    json += ",\"dhcp\":" + String(s.dhcp ? "true" : "false");
    json += ",\"ip\":\"" + jsonEscape(s.ip) + "\"";
    json += ",\"gateway\":\"" + jsonEscape(s.gateway) + "\"";
    json += ",\"netmask\":\"" + jsonEscape(s.netmask) + "\"";
    json += ",\"apPass\":\"" + masked(s.apPass) + "\"";
    json += ",\"serverUrl\":\"" + jsonEscape(s.serverUrl) + "\"";
    json += ",\"username\":\"" + jsonEscape(s.username) + "\"";
    json += ",\"password\":\"" + masked(s.password) + "\"";
    json += ",\"pollMs\":" + String(s.pollMs);
    json += ",\"tags\":[";
    for (size_t i = 0; i < opcua::TAG_SLOTS; ++i) {
        const settings::TagSlot &t = s.tags[i];
        if (i) {
            json += ",";
        }
        json += "{";
        json += "\"enabled\":" + String(t.enabled ? "true" : "false");
        json += ",\"ns\":" + String(t.ns);
        json += ",\"stringId\":" + String(t.stringId ? "true" : "false");
        if (t.stringId) {
            json += ",\"ident\":\"" + jsonEscape(t.identStr) + "\"";
        } else {
            json += ",\"ident\":\"" + String(t.identNum) + "\"";
        }
        json += ",\"name\":\"" + jsonEscape(t.name) + "\"";
        json += "}";
    }
    json += "]}";

    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", json);
}

void handlePostSettings()
{
    settings::Settings &s = settings::get();
    bool wifiChanged = false;

    if (g_server.hasArg("wifiSsid")) {
        char ssid[33];
        copyText(ssid, sizeof(ssid), g_server.arg("wifiSsid").c_str());
        wifiChanged = wifiChanged || strcmp(ssid, s.wifiSsid) != 0;
        copyText(s.wifiSsid, sizeof(s.wifiSsid), ssid);
    }
    if (g_server.hasArg("wifiPass") && g_server.arg("wifiPass").length()) {
        char pass[65];
        copyText(pass, sizeof(pass), g_server.arg("wifiPass").c_str());
        wifiChanged = wifiChanged || strcmp(pass, s.wifiPass) != 0;
        copyText(s.wifiPass, sizeof(s.wifiPass), pass);
    }
    if (g_server.hasArg("hostname")) {
        copyText(s.hostname, sizeof(s.hostname), g_server.arg("hostname").c_str());
    }
    s.dhcp = g_server.hasArg("dhcp");
    if (g_server.hasArg("ip")) {
        copyText(s.ip, sizeof(s.ip), g_server.arg("ip").c_str());
    }
    if (g_server.hasArg("gateway")) {
        copyText(s.gateway, sizeof(s.gateway), g_server.arg("gateway").c_str());
    }
    if (g_server.hasArg("netmask")) {
        copyText(s.netmask, sizeof(s.netmask), g_server.arg("netmask").c_str());
    }
    if (g_server.hasArg("apPass") && g_server.arg("apPass").length()) {
        copyText(s.apPass, sizeof(s.apPass), g_server.arg("apPass").c_str());
    }

    settings::save();
    g_server.send(200, "application/json",
                  "{\"ok\":true,\"wifiChanged\":" + String(wifiChanged ? "true" : "false") + "}");
    if (wifiChanged) {
        g_restartAtMs = millis() + 1500;
    }
}

void handlePostConnection()
{
    settings::Settings &s = settings::get();

    if (g_server.hasArg("serverUrl")) {
        copyText(s.serverUrl, sizeof(s.serverUrl), g_server.arg("serverUrl").c_str());
    }
    if (g_server.hasArg("username")) {
        copyText(s.username, sizeof(s.username), g_server.arg("username").c_str());
    }
    if (g_server.hasArg("password") && g_server.arg("password").length()) {
        copyText(s.password, sizeof(s.password), g_server.arg("password").c_str());
    }
    if (g_server.hasArg("pollMs")) {
        uint32_t pollMs = static_cast<uint32_t>(g_server.arg("pollMs").toInt());
        if (pollMs >= 250 && pollMs <= 3600000) {
            s.pollMs = pollMs;
        }
    }

    settings::save();
    opcua::connectNow();   /* reconnect with the new configuration */
    g_server.send(200, "application/json", "{\"ok\":true}");
}

void handleTags()
{
    const opcua::TagValue *tags = opcua::tags();

    String json = "[";
    for (size_t i = 0; i < opcua::TAG_SLOTS; ++i) {
        const opcua::TagValue &t = tags[i];
        if (i) {
            json += ",";
        }
        json += "{";
        json += "\"slot\":" + String(i);
        json += ",\"enabled\":" + String(t.enabled ? "true" : "false");
        json += ",\"nodeid\":\"" + jsonEscape(t.nodeid) + "\"";
        json += ",\"name\":\"" + jsonEscape(t.name) + "\"";
        json += ",\"ok\":" + String(t.ok ? "true" : "false");
        json += ",\"value\":\"" + jsonEscape(t.value) + "\"";
        json += ",\"status\":\"" + String(t.ok ? "Good" : "Bad") + "\"";
        json += ",\"sourceTs\":" + String(static_cast<long>(t.sourceTsMs));
        json += ",\"updatedAt\":" + String(t.updatedAtMs);
        json += "}";
    }
    json += "]";

    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", json);
}

void handleSaveTags()
{
    settings::Settings &s = settings::get();

    for (size_t i = 0; i < opcua::TAG_SLOTS; ++i) {
        String prefix = "tag" + String(i);
        settings::TagSlot &t = s.tags[i];

        t.enabled = g_server.hasArg(prefix + "-enabled");
        t.ns = static_cast<uint16_t>(g_server.arg(prefix + "-ns").toInt());

        String type = g_server.arg(prefix + "-type");
        t.stringId = (type == "string");

        String ident = g_server.arg(prefix + "-ident");
        if (t.stringId) {
            copyText(t.identStr, sizeof(t.identStr), ident.c_str());
        } else {
            t.identNum = static_cast<uint32_t>(strtoul(ident.c_str(), nullptr, 10));
        }
        copyText(t.name, sizeof(t.name), g_server.arg(prefix + "-name").c_str());
    }

    settings::save();
    g_server.send(200, "application/json", "{\"ok\":true}");
}

void handleConnect()
{
    opcua::connectNow();
    g_server.send(200, "application/json", "{\"ok\":true}");
}

void handleDisconnect()
{
    opcua::disconnectNow();
    g_server.send(200, "application/json", "{\"ok\":true}");
}

void handleReboot()
{
    g_server.send(200, "application/json", "{\"ok\":true}");
    g_restartAtMs = millis() + 800;
}

void handleFactoryReset()
{
    settings::reset();
    g_server.send(200, "application/json", "{\"ok\":true}");
    g_restartAtMs = millis() + 800;
}

void handleIndex()
{
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send_P(200, "text/html", kWebIndexHtml);
}

void handleNotFound()
{
    g_server.send(404, "application/json", "{\"error\":\"not found\"}");
}

} /* namespace */

void start()
{
    if (g_started) {
        return;
    }
    g_server.on("/", HTTP_GET, handleIndex);
    g_server.on("/api/status", HTTP_GET, handleStatus);
    g_server.on("/api/settings", HTTP_GET, handleGetSettings);
    g_server.on("/api/settings", HTTP_POST, handlePostSettings);
    g_server.on("/api/connection", HTTP_POST, handlePostConnection);
    g_server.on("/api/tags", HTTP_GET, handleTags);
    g_server.on("/api/tags", HTTP_POST, handleSaveTags);
    g_server.on("/api/connect", HTTP_POST, handleConnect);
    g_server.on("/api/disconnect", HTTP_POST, handleDisconnect);
    g_server.on("/api/reboot", HTTP_POST, handleReboot);
    g_server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);
    g_server.onNotFound(handleNotFound);
    g_server.begin();
    g_started = true;
}

void tick()
{
    if (!g_started) {
        return;
    }
    g_server.handleClient();
    if (g_restartAtMs && millis() >= g_restartAtMs) {
        g_restartAtMs = 0;
        opcua::stop();
        ESP.restart();
    }
}

} /* namespace webui */
