/**
 * @file app.cpp
 * @brief Application lifecycle: Wi-Fi, setup access point, mDNS, OPC UA, web.
 */
#include "app.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <string.h>

#include "opcua_config.h"
#include "opcua_server.h"
#include "opcua_settings.h"
#include "opcua_web.h"
#include "portal_name.h"

namespace app {

namespace {

Mode g_mode = Mode::SetupAp;
String g_apSsid;
uint32_t g_lastRefresh = 0;

bool configuredSsid()
{
    return settings::get().wifiSsid[0] != '\0';
}

void applyStaticIp()
{
    const settings::Settings &s = settings::get();
    if (s.dhcp || !s.ip[0]) {
        return;
    }
    IPAddress ip, gw, mask;
    if (ip.fromString(s.ip) &&
        (!s.gateway[0] || gw.fromString(s.gateway)) &&
        (!s.netmask[0] || mask.fromString(s.netmask))) {
        if (!s.gateway[0]) {
            gw = IPAddress(0, 0, 0, 0);
        }
        if (!s.netmask[0]) {
            mask = IPAddress(255, 255, 255, 0);
        }
        WiFi.config(ip, gw, mask);
    } else {
        printf("APP: invalid static IP settings, staying on DHCP\n");
    }
}

bool connectStation()
{
    applyStaticIp();
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(settings::get().hostname);
    WiFi.begin(settings::get().wifiSsid, settings::get().wifiPass);

    printf("APP: connecting to '%s'", settings::get().wifiSsid);
    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        printf(".");
        if (millis() - started > 25000) {
            printf("\nAPP: Wi-Fi connection failed\n");
            return false;
        }
    }
    printf("\nAPP: Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void startSetupAp()
{
    g_apSsid = makePortalSsid(settings::get().hostname);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(settings::get().hostname);
    WiFi.softAP(g_apSsid.c_str(), settings::get().apPass);
    printf("APP: setup AP '%s' at %s (password %s)\n",
           g_apSsid.c_str(), WiFi.softAPIP().toString().c_str(),
           settings::get().apPass);
}

bool startMdns()
{
    const settings::Settings &s = settings::get();
    if (!MDNS.begin(s.hostname)) {
        printf("APP: mDNS start failed\n");
        return false;
    }
    MDNS.addService("http", "tcp", OPC_WEB_PORT);
    MDNS.addService("opcua-tcp", "tcp", s.port);
    printf("APP: mDNS http://%s.local\n", s.hostname);
    return true;
}

/* Re-try station mode after a failure, then fall back to the setup AP. */
uint32_t g_reconnectCheckAt = 0;
bool g_stationFailed = false;

} /* namespace */

void start()
{
    settings::load();

    pinMode(OPC_BUTTON_PIN, INPUT);
    pinMode(OPC_RELAY_PIN, OUTPUT);
    digitalWrite(OPC_RELAY_PIN, LOW);

    if (configuredSsid() && connectStation()) {
        g_mode = Mode::Station;
    } else {
        startSetupAp();
        g_mode = Mode::SetupAp;
    }

    startMdns();
    webui::start();

    /* OPC UA runs in station mode and in setup-AP mode alike, so the device
     * can be exercised without a configured network (AP address 192.168.4.1). */
    if (!opcua::start(settings::get().port)) {
        printf("APP: OPC UA server failed to start\n");
    }
    opcua::refreshCache();
}

void tick()
{
    webui::tick();
    opcua::iterate();

    if (millis() - g_lastRefresh >= OPC_REFRESH_MS) {
        g_lastRefresh = millis();
        opcua::refreshCache();

        if (g_mode == Mode::Station && WiFi.status() != WL_CONNECTED) {
            printf("APP: Wi-Fi lost, reconnecting\n");
            WiFi.disconnect();
            WiFi.begin(settings::get().wifiSsid, settings::get().wifiPass);
            g_stationFailed = true;
            g_reconnectCheckAt = millis() + 25000;
        } else if (g_stationFailed && g_reconnectCheckAt &&
                   millis() >= g_reconnectCheckAt) {
            g_reconnectCheckAt = 0;
            if (WiFi.status() != WL_CONNECTED) {
                printf("APP: Wi-Fi unreachable, starting setup AP\n");
                startSetupAp();
                g_mode = Mode::SetupAp;
                g_stationFailed = false;
            }
        }
    }
}

Mode mode()
{
    return g_mode;
}

String apSsid()
{
    return g_apSsid;
}

String ipAddress()
{
    if (g_mode == Mode::Station && WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return WiFi.softAPIP().toString();
}

} /* namespace app */
