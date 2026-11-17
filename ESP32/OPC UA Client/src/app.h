/**
 * @file app.h
 * @brief Application lifecycle: Wi-Fi, setup access point, mDNS, client, web.
 */
#pragma once

#include <Arduino.h>

namespace app {

enum class Mode {
    Station,
    SetupAp,
};

/* Brings up settings, Wi-Fi (or the setup AP), mDNS, web UI and the client. */
void start();

/* Runs one scheduler tick; call from loop(). */
void tick();

/* Current network mode. */
Mode mode();

/* SSID of the setup access point. */
String apSsid();

/* Active IP address (station or AP). */
String ipAddress();

} /* namespace app */
