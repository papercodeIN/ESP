/**
 * @file opcua_config.h
 * @brief Compile-time defaults. They seed the NVS on first boot and after a
 *        factory reset; afterwards everything is configured in the web UI.
 */
#pragma once

#include <Arduino.h>

/* Wi-Fi station, used when no settings are stored yet. Leave the SSID empty
 * to always start in setup-AP mode on a fresh device. */
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define OPC_DEFAULT_WIFI_SSID       ""
#define OPC_DEFAULT_WIFI_PASS       ""
#endif

/* Setup access point, used when the station cannot connect. */
#define OPC_DEFAULT_AP_PASS         "opcua1234"

/* mDNS / AP host name. */
#define OPC_DEFAULT_HOSTNAME        "esp32-opcua"

/* OPC UA endpoint. */
#define OPC_DEFAULT_PORT            4840
#define OPC_DEFAULT_DEVICE_NAME     "ESP32 OPC UA Server"
#define OPC_APP_URI                 "urn:esp32:opcua:server"
#define OPC_PRODUCT_URI             "urn:esp32:opcua:device"
#define OPC_NAMESPACE_URI           "urn:esp32:opcua:server:namespace"

/* Web UI. */
#define OPC_WEB_PORT                80

/* Optional static-IP defaults (empty strings keep DHCP). */
#define OPC_DEFAULT_IP              ""
#define OPC_DEFAULT_GATEWAY         ""
#define OPC_DEFAULT_NETMASK         ""

/* Hardware pins on a generic ESP32 DevKit. */
#define OPC_RELAY_PIN               2   /* onboard LED on most DevKits */
#define OPC_BUTTON_PIN              0   /* BOOT button, active low */

/* Stack sizing. */
#define OPC_MAX_SESSIONS            4
#define OPC_MAX_SECURECHANNELS      8
#define OPC_TCP_BUFFER_SIZE         8192

/* Loop cadence. */
#define OPC_ITERATE_MS              50
#define OPC_REFRESH_MS              1000
