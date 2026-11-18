/**
 * @file bacnet_web.cpp
 * @brief HTTP configuration UI and JSON API.
 *
 * Endpoints
 *   GET  /                      single-page interface
 *   GET  /api/status            runtime status
 *   GET  /api/settings          stored configuration
 *   POST /api/settings          update configuration (form encoded)
 *   GET  /api/points            object list with present values
 *   POST /api/points/write      write present-value or release a priority
 *   GET  /api/memory            configurable memory-point definitions
 *   POST /api/memory            save memory-point definitions and restart
 *   POST /api/reboot            restart the device
 *   POST /api/factory-reset     erase settings and restart
 */
#include "bacnet_web.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "bacnet_config.h"
#include "bacnet_objects.h"
#include "bacnet_settings.h"
#include "portal_name.h"
#include "web_page.h"

namespace webui {
namespace {

WebServer g_server(BACNET_WEB_PORT);
bool g_started = false;
uint32_t g_restartAtMs = 0;

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */
void copyText(char *destination, size_t capacity, const char *source)
{
    if (!destination || capacity == 0) {
        return;
    }
    strncpy(destination, source ? source : "", capacity - 1);
    destination[capacity - 1] = '\0';
}

String jsonEscape(const char *text)
{
    String result;
    if (!text) {
        return result;
    }
    for (const char *p = text; *p; ++p) {
        const char c = *p;
        if (c == '"' || c == '\\') {
            result += '\\';
            result += c;
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else if (c == '\t') {
            result += "\\t";
        } else if (static_cast<unsigned char>(c) < 0x20) {
            result += ' ';
        } else {
            result += c;
        }
    }
    return result;
}

const char *kindName(uint16_t type)
{
    switch (type) {
        case bacnet::kObjectAnalogInput:
            return "AI";
        case bacnet::kObjectAnalogOutput:
            return "AO";
        case bacnet::kObjectAnalogValue:
            return "AV";
        case bacnet::kObjectBinaryInput:
            return "BI";
        case bacnet::kObjectBinaryOutput:
            return "BO";
        case bacnet::kObjectBinaryValue:
            return "BV";
        case bacnet::kObjectDevice:
            return "DEV";
        default:
            return "OBJ";
    }
}

bool parseUnsigned(const String &text, uint32_t &value)
{
    if (text.length() == 0) {
        return false;
    }
    char *end = nullptr;
    const unsigned long parsed = strtoul(text.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed > 0xFFFFFFFFul) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool validIPv4(const String &text)
{
    IPAddress address;
    return address.fromString(text) && text.length() <= 15;
}

bool validHostname(const String &text)
{
    if (text.length() == 0 || text.length() >= 33) {
        return false;
    }
    for (size_t i = 0; i < text.length(); ++i) {
        const char c = text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

bool requestWithinLimits()
{
    return g_server.args() <= 40 && g_server.clientContentLength() <= 4096;
}

void sendJson(int code, const String &body)
{
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.sendHeader("X-Content-Type-Options", "nosniff");
    g_server.sendHeader("X-Frame-Options", "DENY");
    g_server.send(code, "application/json", body);
}

bool requireAdmin()
{
    const settings::Config &cfg = settings::get();
    const bool setupMode = (WiFi.getMode() & WIFI_MODE_AP) != 0 &&
                           WiFi.status() != WL_CONNECTED;
    if (cfg.apPassword[0] == '\0') {
        /* Password-free commissioning remains available for first boot. The
         * release checklist requires setting an AP password before deployment. */
        return true;
    }
    if (!g_server.authenticate("admin", cfg.apPassword)) {
        g_server.requestAuthentication(BASIC_AUTH, "BACnet device");
        return false;
    }
    return true;
}

void rebootSoon(uint32_t delayMs)
{
    appPrepareRestart();
    g_restartAtMs = millis() + delayMs;
    if (g_restartAtMs == 0) {
        g_restartAtMs = 1;
    }
}

/* ------------------------------------------------------------------------- */
/* Handlers                                                                  */
/* ------------------------------------------------------------------------- */
void handleRoot()
{
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send_P(200, "text/html", kWebIndexHtml);
}

void handleFavicon()
{
    g_server.send(204, "text/plain", "");
}

void handleNotFound()
{
    g_server.send(404, "text/plain", "Not found");
}

void handleStatus()
{
    settings::Config &cfg = settings::get();
    const bool portal = (WiFi.getMode() & WIFI_MODE_AP) != 0;
    const bool connected = WiFi.status() == WL_CONNECTED;

    String json;
    json.reserve(400);
    json += "{\"name\":\"";
    json += jsonEscape(cfg.deviceName);
    json += "\",\"instance\":";
    json += static_cast<unsigned long>(cfg.deviceInstance);
    json += ",\"mode\":\"";
    json += portal ? "Setup access point" : (connected ? "Station" : "Connecting");
    json += "\",\"portal\":";
    json += portal ? "true" : "false";
    json += ",\"connected\":";
    json += connected ? "true" : "false";
    json += ",\"ssid\":\"";
    json += jsonEscape(portal ? makePortalSsid(cfg.hostname).c_str()
                              : cfg.wifiSsid);
    json += "\",\"ip\":\"";
    json += portal ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    json += "\",\"rssi\":";
    json += connected ? WiFi.RSSI() : 0;
    json += ",\"uptime\":";
    json += static_cast<unsigned long>(millis() / 1000);
    json += ",\"heap\":";
    json += static_cast<unsigned long>(ESP.getFreeHeap());
    json += ",\"port\":";
    json += cfg.bacnetPort;
    json += ",\"firmware\":\"";
    json += jsonEscape(cfg.firmwareRevision);
    json += "\",\"hostname\":\"";
    json += jsonEscape(cfg.hostname);
    json += "\",\"web\":";
    json += (cfg.webEnabled || portal) ? "true" : "false";
    json += "}";
    sendJson(200, json);
}

void handleDiagnostics()
{
    String json;
    json.reserve(700);
    json += "{\"resetReason\":\"";
    json += appResetReason();
    json += "\",\"bootCount\":";
    json += static_cast<unsigned long>(appBootCount());
    json += ",\"freeHeap\":";
    json += static_cast<unsigned long>(ESP.getFreeHeap());
    json += ",\"wifiConnected\":";
    json += WiFi.status() == WL_CONNECTED ? "true" : "false";
    json += ",\"portal\":";
    json += (WiFi.getMode() & WIFI_MODE_AP) ? "true" : "false";
    json += ",\"bacnetRx\":";
    json += static_cast<unsigned long>(appBacnetRxPackets());
    json += ",\"bacnetDropped\":";
    json += static_cast<unsigned long>(appBacnetDroppedPackets());
    json += ",\"bacnetTxErrors\":";
    json += static_cast<unsigned long>(appBacnetTxErrors());
    json += ",\"covSubscriptions\":";
    json += static_cast<unsigned>(appActiveCovSubscriptions());
    json += "}";
    sendJson(200, json);
}

void handleSettingsGet()
{
    if (!requireAdmin()) {
        return;
    }
    settings::Config &cfg = settings::get();

    String json;
    json.reserve(700);
    json += "{\"ssid\":\"";
    json += jsonEscape(cfg.wifiSsid);
    json += "\",\"hasPass\":";
    json += cfg.wifiPassword[0] ? "true" : "false";
    json += ",\"hostname\":\"";
    json += jsonEscape(cfg.hostname);
    json += "\",\"static\":";
    json += cfg.useStaticIp ? "true" : "false";
    json += ",\"ip\":\"";
    json += jsonEscape(cfg.staticIp);
    json += "\",\"gw\":\"";
    json += jsonEscape(cfg.gateway);
    json += "\",\"mask\":\"";
    json += jsonEscape(cfg.subnet);
    json += "\",\"dns\":\"";
    json += jsonEscape(cfg.dns);
    json += "\",\"hasApPass\":";
    json += cfg.apPassword[0] ? "true" : "false";
    json += ",\"instance\":";
    json += static_cast<unsigned long>(cfg.deviceInstance);
    json += ",\"name\":\"";
    json += jsonEscape(cfg.deviceName);
    json += "\",\"desc\":\"";
    json += jsonEscape(cfg.deviceDescription);
    json += "\",\"location\":\"";
    json += jsonEscape(cfg.deviceLocation);
    json += "\",\"vendorId\":";
    json += cfg.vendorId;
    json += ",\"vendorName\":\"";
    json += jsonEscape(cfg.vendorName);
    json += "\",\"model\":\"";
    json += jsonEscape(cfg.modelName);
    json += "\",\"firmware\":\"";
    json += jsonEscape(cfg.firmwareRevision);
    json += "\",\"appVersion\":\"";
    json += jsonEscape(cfg.appSoftwareVersion);
    json += "\",\"port\":";
    json += cfg.bacnetPort;
    json += ",\"web\":";
    json += cfg.webEnabled ? "true" : "false";
    json += "}";
    sendJson(200, json);
}

void handleSettingsPost()
{
    if (!requireAdmin()) {
        return;
    }
    if (!requestWithinLimits()) {
        sendJson(413, "{\"ok\":false,\"error\":\"Request too large\"}");
        return;
    }
    settings::Config &cfg = settings::get();
    bool reboot = false;

    /* --- Wi-Fi --------------------------------------------------------- */
    if (g_server.hasArg("ssid")) {
        const String ssid = g_server.arg("ssid");
        const String pass = g_server.arg("pass");
        const bool ssidChanged = ssid != String(cfg.wifiSsid);
        if (ssidChanged) {
            copyText(cfg.wifiSsid, sizeof(cfg.wifiSsid), ssid.c_str());
            copyText(cfg.wifiPassword, sizeof(cfg.wifiPassword),
                     pass.c_str());
            reboot = true;
        } else if (pass.length() > 0) {
            copyText(cfg.wifiPassword, sizeof(cfg.wifiPassword),
                     pass.c_str());
            reboot = true;
        }
    }
    if (g_server.hasArg("hostname")) {
        const String value = g_server.arg("hostname");
        if (!validHostname(value)) {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid hostname\"}");
            return;
        }
        if (value != String(cfg.hostname)) {
            copyText(cfg.hostname, sizeof(cfg.hostname), value.c_str());
            reboot = true;
        }
    }
    if (g_server.hasArg("static")) {
        const uint8_t value = g_server.arg("static") == "1" ? 1 : 0;
        if (value != cfg.useStaticIp) {
            cfg.useStaticIp = value;
            reboot = true;
        }
    }
    if (g_server.hasArg("ip")) {
        const String value = g_server.arg("ip");
        if (!validIPv4(value)) {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid IP address\"}");
            return;
        }
        if (value != String(cfg.staticIp)) {
            copyText(cfg.staticIp, sizeof(cfg.staticIp), value.c_str());
            reboot = true;
        }
    }
    if (g_server.hasArg("gw")) {
        const String value = g_server.arg("gw");
        if (!validIPv4(value)) {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid gateway\"}");
            return;
        }
        if (value != String(cfg.gateway)) {
            copyText(cfg.gateway, sizeof(cfg.gateway), value.c_str());
            reboot = true;
        }
    }
    if (g_server.hasArg("mask")) {
        const String value = g_server.arg("mask");
        if (!validIPv4(value)) {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid subnet mask\"}");
            return;
        }
        if (value != String(cfg.subnet)) {
            copyText(cfg.subnet, sizeof(cfg.subnet), value.c_str());
            reboot = true;
        }
    }
    if (g_server.hasArg("dns")) {
        const String value = g_server.arg("dns");
        if (!validIPv4(value)) {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid DNS address\"}");
            return;
        }
        if (value != String(cfg.dns)) {
            copyText(cfg.dns, sizeof(cfg.dns), value.c_str());
            reboot = true;
        }
    }
    if (g_server.hasArg("apPass")) {
        copyText(cfg.apPassword, sizeof(cfg.apPassword),
                 g_server.arg("apPass").c_str());
    }

    /* --- BACnet identity ---------------------------------------------- */
    if (g_server.hasArg("instance")) {
        uint32_t value = 0;
        if (!parseUnsigned(g_server.arg("instance"), value) ||
            value > 4194302u) {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid device instance\"}");
            return;
        }
        if (value != cfg.deviceInstance) {
            cfg.deviceInstance = value;
            reboot = true;
        }
    }
    if (g_server.hasArg("name") && g_server.arg("name").length() > 0) {
        copyText(cfg.deviceName, sizeof(cfg.deviceName),
                 g_server.arg("name").c_str());
    }
    if (g_server.hasArg("desc")) {
        copyText(cfg.deviceDescription, sizeof(cfg.deviceDescription),
                 g_server.arg("desc").c_str());
    }
    if (g_server.hasArg("location")) {
        copyText(cfg.deviceLocation, sizeof(cfg.deviceLocation),
                 g_server.arg("location").c_str());
    }
    if (g_server.hasArg("vendorId")) {
        const long value = g_server.arg("vendorId").toInt();
        if (value >= 0 && value <= 65535) {
            cfg.vendorId = static_cast<uint16_t>(value);
        }
    }
    if (g_server.hasArg("vendorName")) {
        copyText(cfg.vendorName, sizeof(cfg.vendorName),
                 g_server.arg("vendorName").c_str());
    }
    if (g_server.hasArg("model")) {
        copyText(cfg.modelName, sizeof(cfg.modelName),
                 g_server.arg("model").c_str());
    }
    if (g_server.hasArg("fw")) {
        copyText(cfg.firmwareRevision, sizeof(cfg.firmwareRevision),
                 g_server.arg("fw").c_str());
    }
    if (g_server.hasArg("app")) {
        copyText(cfg.appSoftwareVersion, sizeof(cfg.appSoftwareVersion),
                 g_server.arg("app").c_str());
    }
    if (g_server.hasArg("port")) {
        const long value = g_server.arg("port").toInt();
        if (value > 0 && value <= 65535 &&
            static_cast<uint16_t>(value) != cfg.bacnetPort) {
            cfg.bacnetPort = static_cast<uint16_t>(value);
            reboot = true;
        }
    }
    if (g_server.hasArg("web")) {
        if (g_server.arg("web") != "0" && g_server.arg("web") != "1") {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid web setting\"}");
            return;
        }
        const uint8_t value = g_server.arg("web") == "1" ? 1 : 0;
        if (cfg.webEnabled != value) {
            cfg.webEnabled = value;
            reboot = true;
        }
    }

    const bool saved = settings::save();
    bacnet::applyDeviceIdentity();

    String json = "{\"ok\":";
    json += saved ? "true" : "false";
    json += ",\"reboot\":";
    json += reboot ? "true" : "false";
    json += "}";
    sendJson(saved ? 200 : 500, json);

    if (reboot) {
        rebootSoon(700);
    }
}

void handlePoints()
{
    String json;
    json.reserve(2600);
    json += "{\"points\":[";

    bool first = true;
    const size_t count = bacnet::objectCount();
    for (size_t i = 0; i < count; ++i) {
        const bacnet::Object *object = bacnet::objectAt(i);
        if (!object || object->type == bacnet::kObjectDevice) {
            continue;
        }
        if (!first) {
            json += ',';
        }
        first = false;

        json += "{\"t\":";
        json += object->type;
        json += ",\"i\":";
        json += static_cast<unsigned long>(object->instance);
        json += ",\"k\":\"";
        json += kindName(object->type);
        json += "\",\"n\":\"";
        json += jsonEscape(object->name);
        json += "\",\"u\":";
        json += object->isAnalog() ? object->units : bacnet::kUnitsNoUnits;
        json += ",\"c\":";
        json += object->isCommandable() ? 1 : 0;
        json += ",\"v\":";
        if (object->isAnalog()) {
            json += String(object->presentValue, 3);
        } else {
            json += object->binaryPresentValue ? 1 : 0;
        }

        if (object->isCommandable()) {
            json += ",\"rd\":";
            if (object->isAnalog()) {
                json += String(object->relinquishDefault, 3);
            } else {
                json += object->binaryRelinquishDefault ? 1 : 0;
            }
            json += ",\"pa\":[";
            for (size_t p = 0; p < bacnet::kMaxPriority; ++p) {
                if (p) {
                    json += ',';
                }
                if (object->isAnalog()) {
                    if (object->priorityActive[p]) {
                        json += String(object->priority[p], 3);
                    } else {
                        json += "null";
                    }
                } else {
                    if (object->binaryPriorityActive[p]) {
                        json += object->binaryPriority[p] ? 1 : 0;
                    } else {
                        json += "null";
                    }
                }
            }
            json += "]";
        }
        json += "}";
    }

    json += "]}";
    sendJson(200, json);
}

void handleMemoryGet()
{
    if (!requireAdmin()) {
        return;
    }
    const settings::MemoryConfig &cfg = settings::memory();
    String json;
    json.reserve(1800);
    json += "{\"capacity\":";
    json += static_cast<unsigned>(settings::kMaxMemoryPoints);
    json += ",\"points\":[";
    for (size_t i = 0; i < settings::kMaxMemoryPoints; ++i) {
        if (i) {
            json += ',';
        }
        const settings::MemoryPoint &mp = cfg.points[i];
        json += "{\"enabled\":";
        json += mp.enabled ? "true" : "false";
        json += ",\"kind\":";
        json += static_cast<unsigned>(mp.kind);
        json += ",\"writable\":";
        json += mp.writable ? "true" : "false";
        json += ",\"persistent\":";
        json += mp.persistent ? "true" : "false";
        json += ",\"instance\":";
        json += static_cast<unsigned long>(mp.instance);
        json += ",\"units\":";
        json += mp.units;
        json += ",\"defaultValue\":";
        json += String(mp.defaultValue, 3);
        json += ",\"retainedValue\":";
        json += String(mp.retainedValue, 3);
        json += ",\"minValue\":";
        json += String(mp.minValue, 3);
        json += ",\"maxValue\":";
        json += String(mp.maxValue, 3);
        json += ",\"covIncrement\":";
        json += String(mp.covIncrement, 3);
        json += ",\"name\":\"";
        json += jsonEscape(mp.name);
        json += "\",\"description\":\"";
        json += jsonEscape(mp.description);
        json += "\"}";
    }
    json += "]}";
    sendJson(200, json);
}

void handleMemoryPost()
{
    if (!requireAdmin()) {
        return;
    }
    settings::MemoryConfig next;
    settings::setMemoryDefaults(next);
    const settings::MemoryConfig &current = settings::memory();
    for (size_t i = 0; i < settings::kMaxMemoryPoints; ++i) {
        next.points[i].retainedValue = current.points[i].retainedValue;
    }
    if (!requestWithinLimits()) {
        sendJson(413, "{\"ok\":false,\"error\":\"Request too large\"}");
        return;
    }
    for (size_t i = 0; i < settings::kMaxMemoryPoints; ++i) {
        settings::MemoryPoint &mp = next.points[i];
        const String index = String(i);
        auto key = [&](const char *prefix) {
            return String(prefix) + "_" + index;
        };
        if (g_server.hasArg(key("enabled"))) {
            mp.enabled = g_server.arg(key("enabled")) == "1";
        }
        if (g_server.hasArg(key("kind"))) {
            mp.kind = g_server.arg(key("kind")).toInt() == 1
                          ? settings::kMemoryAnalog
                          : settings::kMemoryBinary;
        }
        if (g_server.hasArg(key("writable"))) {
            mp.writable = g_server.arg(key("writable")) == "1";
        }
        if (g_server.hasArg(key("persistent"))) {
            mp.persistent = g_server.arg(key("persistent")) == "1";
        }
        if (g_server.hasArg(key("instance"))) {
            mp.instance = static_cast<uint32_t>(strtoul(
                g_server.arg(key("instance")).c_str(), nullptr, 10));
        }
        if (g_server.hasArg(key("units"))) {
            mp.units = static_cast<uint16_t>(g_server.arg(key("units")).toInt());
        }
        if (g_server.hasArg(key("default"))) {
            mp.defaultValue = g_server.arg(key("default")).toFloat();
        }
        if (g_server.hasArg(key("min"))) {
            mp.minValue = g_server.arg(key("min")).toFloat();
        }
        if (g_server.hasArg(key("max"))) {
            mp.maxValue = g_server.arg(key("max")).toFloat();
        }
        if (g_server.hasArg(key("cov"))) {
            mp.covIncrement = g_server.arg(key("cov")).toFloat();
        }
        if (g_server.hasArg(key("name"))) {
            copyText(mp.name, sizeof(mp.name), g_server.arg(key("name")).c_str());
        }
        if (g_server.hasArg(key("description"))) {
            copyText(mp.description, sizeof(mp.description),
                     g_server.arg(key("description")).c_str());
        }
    }

    for (size_t i = 0; i < settings::kMaxMemoryPoints; ++i) {
        const settings::MemoryPoint &mp = next.points[i];
        if (!mp.enabled) {
            continue;
        }
        if (mp.instance > 4194302u || mp.name[0] == '\0') {
            sendJson(400, "{\"ok\":false,\"error\":\"Enabled memory points need a valid instance and name\"}");
            return;
        }
        if (mp.kind == settings::kMemoryAnalog &&
            (!isfinite(mp.minValue) || !isfinite(mp.maxValue) ||
             mp.minValue >= mp.maxValue || !isfinite(mp.defaultValue) ||
             mp.defaultValue < mp.minValue || mp.defaultValue > mp.maxValue ||
             !isfinite(mp.covIncrement) || mp.covIncrement <= 0.0f)) {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid analog range or default\"}");
            return;
        }
        const uint16_t type = mp.kind == settings::kMemoryAnalog
                                  ? bacnet::kObjectAnalogValue
                                  : bacnet::kObjectBinaryValue;
        for (size_t j = 0; j < i; ++j) {
            const settings::MemoryPoint &other = next.points[j];
            if (other.enabled && other.kind == mp.kind &&
                other.instance == mp.instance) {
                sendJson(400, "{\"ok\":false,\"error\":\"Duplicate memory instance\"}");
                return;
            }
        }
        for (size_t j = 0; j < bacnet::objectCount(); ++j) {
            const bacnet::Object *object = bacnet::objectAt(j);
            if (object && object->type == type && object->instance == mp.instance &&
                object->memoryIndex < 0) {
                sendJson(400, "{\"ok\":false,\"error\":\"Memory instance conflicts with an existing point\"}");
                return;
            }
        }
    }

    const bool saved = settings::saveMemory(next);
    String json = "{\"ok\":";
    json += saved ? "true" : "false";
    json += ",\"reboot\":true}";
    sendJson(saved ? 200 : 500, json);
    if (saved) {
        rebootSoon(700);
    }
}

void handlePointWrite()
{
    if (!requireAdmin()) {
        return;
    }
    if (!g_server.hasArg("type") || !g_server.hasArg("instance")) {
        sendJson(400, "{\"ok\":false,\"class\":2,\"code\":32}");
        return;
    }

    uint32_t typeValue = 0;
    uint32_t instance = 0;
    if (!parseUnsigned(g_server.arg("type"), typeValue) ||
        !parseUnsigned(g_server.arg("instance"), instance) ||
        typeValue > 1023u || instance > 4194302u) {
        sendJson(400, "{\"ok\":false,\"error\":\"Invalid point identifier\"}");
        return;
    }
    const uint16_t type = static_cast<uint16_t>(typeValue);

    bacnet::Object *object = bacnet::findObject(type, instance);
    if (!object) {
        sendJson(404, "{\"ok\":false,\"class\":1,\"code\":31}");
        return;
    }
    if (!object->isCommandable()) {
        sendJson(200, "{\"ok\":false,\"class\":2,\"code\":40}");
        return;
    }

    bacnet::Value value;
    const bool release =
        g_server.hasArg("rel") && g_server.arg("rel") != "0";

    if (release) {
        value.type = bacnet::Value::Null;
    } else if (object->isAnalog()) {
        if (!g_server.hasArg("value")) {
            sendJson(400, "{\"ok\":false,\"class\":2,\"code\":9}");
            return;
        }
        value.type = bacnet::Value::Real;
        value.real = g_server.arg("value").toFloat();
        if (!isfinite(value.real)) {
            sendJson(400, "{\"ok\":false,\"class\":2,\"code\":37}");
            return;
        }
    } else {
        const String text = g_server.arg("value");
        if (text != "0" && text != "1") {
            sendJson(400, "{\"ok\":false,\"error\":\"Binary value must be 0 or 1\"}");
            return;
        }
        value.type = bacnet::Value::Enumerated;
        value.unsignedValue = text == "1" ? 1 : 0;
    }

    uint8_t priority = 0;
    if (g_server.hasArg("priority")) {
        uint32_t parsedPriority = 0;
        if (!parseUnsigned(g_server.arg("priority"), parsedPriority) ||
            parsedPriority < 1 || parsedPriority > bacnet::kMaxPriority) {
            sendJson(400, "{\"ok\":false,\"error\":\"Invalid priority\"}");
            return;
        }
        priority = static_cast<uint8_t>(parsedPriority);
    }

    uint16_t errorClass = 0;
    uint32_t errorCode = 0;
    const bool ok = bacnet::writeProperty(object, bacnet::kPropPresentValue,
                                          false, 0, value, priority,
                                          errorClass, errorCode);

    String json = "{\"ok\":";
    json += ok ? "true" : "false";
    if (!ok) {
        json += ",\"class\":";
        json += errorClass;
        json += ",\"code\":";
        json += static_cast<unsigned long>(errorCode);
    }
    json += "}";
    sendJson(200, json);
}

void handleReboot()
{
    if (!requireAdmin()) {
        return;
    }
    sendJson(200, "{\"ok\":true,\"reboot\":true}");
    rebootSoon(400);
}

void handleFactoryReset()
{
    if (!requireAdmin()) {
        return;
    }
    const bool ok = settings::resetToDefaults();
    String json = "{\"ok\":";
    json += ok ? "true" : "false";
    json += ",\"reboot\":";
    json += ok ? "true}" : "false}";
    sendJson(ok ? 200 : 500, json);
    if (ok) {
        rebootSoon(600);
    }
}

/* ---- GPIO mapping API --------------------------------------------------- */

void handleGpioGet()
{
    if (!requireAdmin()) {
        return;
    }
    const settings::GpioConfig &cfg = settings::gpio();

    String json;
    json.reserve(800);
    json += "{\"points\":[";
    for (size_t i = 0; i < settings::kMaxGpioPoints; ++i) {
        const settings::GpioPoint &gp = cfg.points[i];
        if (i) {
            json += ',';
        }
        json += "{\"pin\":";
        json += gp.pin;
        json += ",\"label\":\"";
        json += settings::gpioPinLabel(gp.pin);
        json += "\",\"enabled\":";
        json += gp.enabled ? "true" : "false";
        json += ",\"kind\":";
        json += gp.kind;
        json += ",\"activeLow\":";
        json += gp.activeLow ? "true" : "false";
        json += ",\"instance\":";
        json += static_cast<unsigned long>(gp.instance);
        json += ",\"name\":\"";
        json += jsonEscape(gp.name);
        json += "\"}";
    }
    json += "]}";
    sendJson(200, json);
}

void handleGpioPost()
{
    if (!requireAdmin()) {
        return;
    }
    if (!requestWithinLimits()) {
        sendJson(413, "{\"ok\":false,\"error\":\"Request too large\"}");
        return;
    }
    /* Parse the incoming table: fields are indexed arrays like
     * enabled_0, kind_0, instance_0, name_0, activeLow_0, ... */
    settings::GpioConfig next;
    settings::setGpioDefaults(next);

    for (size_t i = 0; i < settings::kMaxGpioPoints; ++i) {
        settings::GpioPoint &gp = next.points[i];

        String prefix = String(i);
        String enKey = "enabled_" + prefix;
        String kiKey = "kind_" + prefix;
        String inKey = "instance_" + prefix;
        String nmKey = "name_" + prefix;
        String alKey = "activeLow_" + prefix;

        if (g_server.hasArg(enKey)) {
            gp.enabled = (g_server.arg(enKey) == "1") ? 1 : 0;
        }
        if (g_server.hasArg(kiKey)) {
            gp.kind = (g_server.arg(kiKey) == "1")
                          ? settings::kGpioOutput
                          : settings::kGpioInput;
        }
        if (g_server.hasArg(inKey)) {
            gp.instance = static_cast<uint32_t>(
                strtoul(g_server.arg(inKey).c_str(), nullptr, 10));
        }
        if (g_server.hasArg(nmKey)) {
            copyText(gp.name, sizeof(gp.name),
                     g_server.arg(nmKey).c_str());
        }
        if (g_server.hasArg(alKey)) {
            gp.activeLow = (g_server.arg(alKey) == "1") ? 1 : 0;
        }
    }

    /* Validate: enabled rows must have unique instances that don't overlap
     * with built-in objects. Instances must be <= 4194302. */
    for (size_t i = 0; i < settings::kMaxGpioPoints; ++i) {
        settings::GpioPoint &gp = next.points[i];
        if (!gp.enabled) {
            continue;
        }
        if (gp.instance > 4194302u) {
            String error = "{\"ok\":false,\"error\":\"Row ";
            error += static_cast<unsigned>(i + 1);
            error += " instance is out of range\"}";
            sendJson(400, error);
            return;
        }
        /* Check collisions with built-in objects. */
        uint16_t objType = (gp.kind == settings::kGpioOutput)
                               ? bacnet::kObjectBinaryOutput
                               : bacnet::kObjectBinaryInput;
        /* BI:0, BI:1 and BO:0 are built-in. */
        if ((objType == bacnet::kObjectBinaryInput &&
             (gp.instance == 0 || gp.instance == 1)) ||
            (objType == bacnet::kObjectBinaryOutput && gp.instance == 0)) {
            String error = "{\"ok\":false,\"error\":\"Row ";
            error += static_cast<unsigned>(i + 1);
            error += " uses a reserved built-in instance\"}";
            sendJson(400, error);
            return;
        }
        /* Check for duplicate instances among enabled rows. */
        for (size_t j = 0; j < i; ++j) {
            if (!next.points[j].enabled) {
                continue;
            }
            uint16_t jType = (next.points[j].kind == settings::kGpioOutput)
                                 ? bacnet::kObjectBinaryOutput
                                 : bacnet::kObjectBinaryInput;
            if (jType == objType &&
                next.points[j].instance == gp.instance) {
                sendJson(400,
                         "{\"ok\":false,\"error\":\"Duplicate instance\"}");
                return;
            }
        }
    }

    const bool saved = settings::saveGpio(next);
    String json = "{\"ok\":";
    json += saved ? "true" : "false";
    json += ",\"reboot\":true}";
    sendJson(saved ? 200 : 500, json);
    if (saved) {
        rebootSoon(700);
    }
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */
void begin()
{
    if (g_started) {
        return;
    }

    g_server.on("/", HTTP_GET, handleRoot);
    g_server.on("/index.html", HTTP_GET, handleRoot);
    g_server.on("/favicon.ico", HTTP_GET, handleFavicon);
    g_server.on("/api/status", HTTP_GET, handleStatus);
    g_server.on("/api/diagnostics", HTTP_GET, handleDiagnostics);
    g_server.on("/api/settings", HTTP_GET, handleSettingsGet);
    g_server.on("/api/settings", HTTP_POST, handleSettingsPost);
    g_server.on("/api/points", HTTP_GET, handlePoints);
    g_server.on("/api/points/write", HTTP_POST, handlePointWrite);
    g_server.on("/api/memory", HTTP_GET, handleMemoryGet);
    g_server.on("/api/memory", HTTP_POST, handleMemoryPost);
    g_server.on("/api/reboot", HTTP_POST, handleReboot);
    g_server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);
    g_server.on("/api/gpio", HTTP_GET, handleGpioGet);
    g_server.on("/api/gpio", HTTP_POST, handleGpioPost);
    g_server.onNotFound(handleNotFound);

    g_server.begin();
    g_started = true;

    Serial.printf("[Web] Configuration UI on port %u\n",
                  static_cast<unsigned>(BACNET_WEB_PORT));
}

void run()
{
    if (!g_started) {
        return;
    }
    g_server.handleClient();

    if (g_restartAtMs != 0 &&
        static_cast<int32_t>(millis() - g_restartAtMs) >= 0) {
        Serial.println("[Web] Restarting...");
        delay(50);
        ESP.restart();
    }
}

} // namespace webui
