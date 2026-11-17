/**
 * @file bacnet_settings.h
 * @brief Persistent device configuration stored in NVS (flash).
 *
 * The defaults come from bacnet_config.h. Everything can be changed at
 * runtime from the built-in web interface and survives power cycles.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace settings {

struct Config {
    /* Wi-Fi */
    char hostname[33];
    char wifiSsid[33];
    char wifiPassword[65];
    char apPassword[65];

    /* Network */
    uint8_t useStaticIp;
    char staticIp[16];
    char gateway[16];
    char subnet[16];
    char dns[16];

    /* BACnet device identity */
    uint32_t deviceInstance;
    char deviceName[64];
    char deviceDescription[80];
    char deviceLocation[32];
    uint16_t vendorId;
    char vendorName[48];
    char modelName[48];
    char firmwareRevision[16];
    char appSoftwareVersion[16];
    uint16_t bacnetPort;

    /* Services */
    uint8_t webEnabled;

    /* Internal storage metadata (do not edit from the web UI) */
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
};

/* Load settings from flash, seeding defaults on first boot. */
void begin();

/* Mutable configuration. Changes are only persisted by save(). */
Config &get();

/* Persist the current configuration. Returns false on a write error. */
bool save();

/* Restore defaults and persist them. */
bool resetToDefaults();

/* True when an SSID has been configured. */
bool wifiConfigured();

} // namespace settings
