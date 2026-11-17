/**
 * @file opcua_web.cpp
 * @brief HTTP configuration UI and JSON API.
 */
#include "opcua_web.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "opcua_config.h"
#include "opcua_server.h"
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
    json += ",\"rssi\":" + String(app::mode() == app::Mode::Station ? WiFi.RSSI() : 0);
    json += ",\"uptime\":" + String(millis() / 1000);
    json += ",\"heap\":" + String(ESP.getFreeHeap());
    json += ",\"connected\":" + String(app::mode() == app::Mode::Station && WiFi.status() == WL_CONNECTED ? "true" : "false");
    json += ",\"opcuaRunning\":" + String(opcua::running() ? "true" : "false");
    json += ",\"endpointUrl\":\"" + jsonEscape(opcua::endpointUrl().c_str()) + "\"";
    json += ",\"mdnsUrl\":\"http://" + jsonEscape(s.hostname) + ".local\"";
    json += ",\"deviceName\":\"" + jsonEscape(s.deviceName) + "\"";
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
    json += ",\"port\":" + String(s.port);
    json += ",\"deviceName\":\"" + jsonEscape(s.deviceName) + "\"";
    json += ",\"endpointHost\":\"" + jsonEscape(s.endpointHost) + "\"";
    json += "}";

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
        /* empty = keep stored password */
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
    if (g_server.hasArg("port")) {
        int port = g_server.arg("port").toInt();
        if (port > 0 && port < 65536) {
            s.port = static_cast<uint16_t>(port);
        }
    }
    if (g_server.hasArg("deviceName")) {
        copyText(s.deviceName, sizeof(s.deviceName), g_server.arg("deviceName").c_str());
    }
    if (g_server.hasArg("endpointHost")) {
        copyText(s.endpointHost, sizeof(s.endpointHost), g_server.arg("endpointHost").c_str());
    }

    settings::save();
    g_server.send(200, "application/json",
                  "{\"ok\":true,\"wifiChanged\":" + String(wifiChanged ? "true" : "false") + "}");
    if (wifiChanged) {
        g_restartAtMs = millis() + 1500;   /* apply Wi-Fi changes cleanly */
    }
}

void handleTags()
{
    opcua::refreshCache();
    const opcua::TagValue *tags = opcua::tags();

    String json = "[";
    for (size_t i = 0; i < opcua::TAG_COUNT; ++i) {
        const opcua::TagValue &t = tags[i];
        if (i) {
            json += ",";
        }
        json += "{";
        json += "\"id\":\"" + jsonEscape(t.id) + "\"";
        json += ",\"name\":\"" + jsonEscape(t.name) + "\"";
        json += ",\"type\":\"" + jsonEscape(t.type) + "\"";
        json += ",\"writable\":" + String(t.writable ? "true" : "false");
        json += ",\"valid\":" + String(t.valid ? "true" : "false");
        json += ",\"value\":\"" + jsonEscape(t.value) + "\"";
        json += ",\"changedAt\":" + String(t.changedAtMs);
        json += "}";
    }
    json += "]";

    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", json);
}

void handleTagWrite()
{
    String id = g_server.arg("id");
    String value = g_server.arg("value");

    if (!id.length()) {
        g_server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing id\"}");
        return;
    }
    if (!opcua::writeTag(id.c_str(), value.c_str())) {
        g_server.send(400, "application/json",
                      "{\"ok\":false,\"error\":\"tag unknown or not writable\"}");
        return;
    }
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
    g_server.on("/api/tags", HTTP_GET, handleTags);
    g_server.on("/api/tags/write", HTTP_POST, handleTagWrite);
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
