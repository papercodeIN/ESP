/**
 * @file opcua_settings.h
 * @brief Persistent configuration stored in NVS (flash).
 */
#pragma once

#include <Arduino.h>

namespace settings {

struct Settings {
    char wifiSsid[33];
    char wifiPass[65];
    char hostname[33];
    bool dhcp;
    char ip[16];
    char gateway[16];
    char netmask[16];
    char apPass[17];
    uint16_t port;
    char deviceName[33];
    char endpointHost[64];   /* optional host override for the endpoint URL */
    double setpoint;         /* demo writable tag, persisted */
};

/* Loads stored settings or the compile-time defaults on first boot. */
void load();

/* Persists the current settings to NVS. */
void save();

/* Erases all stored settings and restores the defaults. */
void reset();

/* Currently active settings (safe to read from anywhere). */
Settings &get();

} /* namespace settings */
