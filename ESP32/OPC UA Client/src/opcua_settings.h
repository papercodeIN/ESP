/**
 * @file opcua_settings.h
 * @brief Persistent configuration stored in NVS (flash).
 */
#pragma once

#include <Arduino.h>

#include "opcua_config.h"

namespace settings {

/* One configurable tag slot. Identifier is either numeric or a string. */
struct TagSlot {
    bool enabled;
    uint16_t ns;
    bool stringId;
    uint32_t identNum;
    char identStr[48];
    char name[40];
};

struct Settings {
    char wifiSsid[33];
    char wifiPass[65];
    char hostname[33];
    bool dhcp;
    char ip[16];
    char gateway[16];
    char netmask[16];
    char apPass[17];
    char serverUrl[128];
    char username[49];
    char password[49];
    uint32_t pollMs;
    TagSlot tags[OPC_TAG_SLOTS];
};

void load();
void save();
void reset();

Settings &get();

} /* namespace settings */
