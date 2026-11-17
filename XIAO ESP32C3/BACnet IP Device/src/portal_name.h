/**
 * @file portal_name.h
 * @brief Builds the setup access point name from the host name and chip ID.
 */
#pragma once

#include <Arduino.h>

inline String makePortalSsid(const char *hostname)
{
    /* ESP-IDF softAP accepts SSIDs of at most 32 octets; keep room for the
     * "-" separator and the 1..4 hex-digit suffix. */
    String name = (hostname && hostname[0]) ? String(hostname) : String("bacnet");
    if (name.length() > 27) {
        name.remove(27);
    }
    name += "-";
    name += String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFF), HEX);
    return name;
}
