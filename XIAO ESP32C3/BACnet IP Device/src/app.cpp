/**
 * @file app.cpp
 * @brief Wi-Fi, setup access point, web interface and BACnet lifecycle.
 *
 * Boot sequence
 *   1. Load the stored settings and build the object database.
 *   2. Start the built-in web interface.
 *   3. Join Wi-Fi, or start the setup access point when no credentials are
 *      stored / the network cannot be reached.
 *   4. Start the BACnet/IP server once the network is up.
 */
#include "app.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "bacnet_config.h"
#include "bacnet_objects.h"
#include "bacnet_server.h"
#include "bacnet_settings.h"
#include "bacnet_web.h"
#include "portal_name.h"

namespace {

bacnet::Server g_server;
bool g_serverStarted = false;
uint16_t g_runningPort = 0;

bool g_portalActive = false;
bool g_mdnsStarted = false;
bool g_everConnected = false;
bool g_lastConnected = false;

bool g_wifiAttempted = false;
uint32_t g_lastWifiAttemptMs = 0;
uint32_t g_bootMs = 0;

constexpr uint32_t kWifiRetryIntervalMs = 15000;
constexpr uint32_t kPortalFallbackMs = 25000;

/* ------------------------------------------------------------------------- */
/* Wi-Fi                                                                     */
/* ------------------------------------------------------------------------- */
void attemptWifi()
{
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    const uint32_t now = millis();
    if (g_wifiAttempted && (now - g_lastWifiAttemptMs) < kWifiRetryIntervalMs) {
        return;
    }
    g_wifiAttempted = true;
    g_lastWifiAttemptMs = now;

    settings::Config &config = settings::get();

    WiFi.persistent(false);
    if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
    }
    /* Lock the transmitter at the ESP32-C3 maximum (19.5 dBm); the chip
     * defaults to this, but re-asserting it keeps the setting stable
     * across AP/STA mode switches. */
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    /* Keep the radio awake: the default Wi-Fi modem sleep adds latency
     * spikes that break BACnet request/response timing on marginal links
     * (YABE "Communication Error: Couldn't fetch properties"). */
    WiFi.setSleep(false);
    WiFi.setHostname(config.hostname);

    if (config.useStaticIp) {
        IPAddress ip;
        IPAddress gateway;
        IPAddress subnet;
        IPAddress dns;
        ip.fromString(config.staticIp);
        gateway.fromString(config.gateway);
        subnet.fromString(config.subnet);
        dns.fromString(config.dns);
        WiFi.config(ip, gateway, subnet, dns);
    }

    Serial.printf("[WiFi] Connecting to \"%s\"...\n", config.wifiSsid);
    WiFi.begin(config.wifiSsid,
               config.wifiPassword[0] ? config.wifiPassword : nullptr);
}

void startPortal()
{
    if (g_portalActive) {
        return;
    }

    settings::Config &config = settings::get();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    String apName = makePortalSsid(config.hostname);

    const bool ok = WiFi.softAP(
        apName.c_str(),
        config.apPassword[0] ? config.apPassword : nullptr);
    if (!ok) {
        Serial.println("[WiFi] Failed to start the setup access point");
        return;
    }

    g_portalActive = true;
    Serial.println("[WiFi] No network connection - setup mode active");
    Serial.printf("[WiFi] Join \"%s\" and open http://%s/\n",
                  apName.c_str(), WiFi.softAPIP().toString().c_str());
    if (config.apPassword[0]) {
        Serial.printf("[WiFi] Access point password: %s\n",
                      config.apPassword);
    }
}

/* ------------------------------------------------------------------------- */
/* mDNS + BACnet                                                             */
/* ------------------------------------------------------------------------- */
void startMdns()
{
    if (g_mdnsStarted) {
        return;
    }
    settings::Config &config = settings::get();
    if (MDNS.begin(config.hostname)) {
        MDNS.addService("http", "tcp", BACNET_WEB_PORT);
        Serial.printf("[mDNS] Web UI at http://%s.local/\n", config.hostname);
    } else {
        Serial.println("[mDNS] Could not start the responder");
    }
    g_mdnsStarted = true;
}

void startBacnet()
{
    settings::Config &config = settings::get();

    if (g_serverStarted && g_runningPort != config.bacnetPort) {
        g_serverStarted = false;
    }
    if (g_serverStarted) {
        return;
    }

    if (!g_server.begin(config.bacnetPort)) {
        return;
    }

    g_serverStarted = true;
    g_runningPort = config.bacnetPort;
    g_server.sendIAm();

    Serial.printf("[BACnet] Device %u ready on %s:%u\n",
                  static_cast<unsigned>(config.deviceInstance),
                  WiFi.localIP().toString().c_str(),
                  static_cast<unsigned>(config.bacnetPort));
    Serial.println(
        "[BACnet] Who-Is/I-Am, ReadProperty, ReadPropertyMultiple, "
        "WriteProperty");
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Public entry points                                                       */
/* ------------------------------------------------------------------------- */
void appSetup()
{
#if BACNET_ENABLE_SERIAL
    Serial.begin(BACNET_SERIAL_BAUD);
    delay(300);
#endif

    settings::begin();
    settings::Config &config = settings::get();

#if BACNET_ENABLE_SERIAL
    Serial.println();
    Serial.println("==============================================");
    Serial.println(" XIAO ESP32C3 BACnet/IP Device");
    Serial.println("==============================================");
    Serial.printf(" Device instance : %u\n",
                  static_cast<unsigned>(config.deviceInstance));
    Serial.printf(" BACnet UDP port : %u\n",
                  static_cast<unsigned>(config.bacnetPort));
    Serial.printf(" Vendor ID       : %u\n",
                  static_cast<unsigned>(config.vendorId));
    Serial.println("----------------------------------------------");
#endif

    bacnet::initObjects();

    g_bootMs = millis();

    /* Bring Wi-Fi up first. The web server is NOT started here: it opens
     * lwIP sockets, and sockets created before the Wi-Fi driver has run
     * esp_wifi_start() crash with "assert failed: tcpip_send_msg_wait_sem
     * (Invalid mbox)". webui::begin() therefore runs from appLoop() once
     * the portal is active or the station has connected. */
    if (settings::wifiConfigured()) {
        attemptWifi();
    } else {
        startPortal();
    }
}

void appLoop()
{
    settings::Config &config = settings::get();
    const bool connected = WiFi.status() == WL_CONNECTED;

    /* Start the web server lazily (see appSetup): its sockets need the
     * TCP/IP stack, which is only guaranteed to be up once the setup
     * access point is running or the station has an address. */
    if ((config.webEnabled || g_portalActive) &&
        (connected || g_portalActive)) {
        webui::begin();
        webui::run();
    }

    if (connected) {
        if (!g_lastConnected) {
            g_everConnected = true;
            Serial.printf("[WiFi] Connected: %s (%d dBm)\n",
                          WiFi.localIP().toString().c_str(),
                          WiFi.RSSI());
            /* Rebind the UDP socket after every (re)connection. */
            g_serverStarted = false;
        }
        startMdns();
        startBacnet();
        if (g_serverStarted) {
            g_server.run();
        }
    } else if (!g_everConnected) {
        if (!settings::wifiConfigured()) {
            startPortal();
        } else {
            attemptWifi();
            if (!g_portalActive &&
                (millis() - g_bootMs) > kPortalFallbackMs) {
                startPortal();
            }
        }
    } else if (!g_portalActive) {
        attemptWifi();
    }

    g_lastConnected = connected;

    bacnet::updateObjects(millis());
    delay(1);
}
