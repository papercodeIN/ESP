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
#define OPC_DEFAULT_HOSTNAME        "esp32-opcua-client"

/* OPC UA server to read from. */
#define OPC_DEFAULT_SERVER_URL      "opc.tcp://192.168.1.50:4840"
#define OPC_DEFAULT_USERNAME        ""
#define OPC_DEFAULT_PASSWORD        ""
#define OPC_DEFAULT_POLL_MS         2000

/* Optional static-IP defaults (empty strings keep DHCP). */
#define OPC_DEFAULT_IP              ""
#define OPC_DEFAULT_GATEWAY         ""
#define OPC_DEFAULT_NETMASK         ""

/* Tag slots configurable in the web UI. */
#define OPC_TAG_SLOTS               8

/* Session sizing. */
#define OPC_CLIENT_TIMEOUT_MS       3000
#define OPC_RECONNECT_BACKOFF_MS    10000

/* Loop cadence. */
#define OPC_POLL_CHECK_MS           50
#define OPC_REFRESH_MS              1000

/* Web UI. */
#define OPC_WEB_PORT                80
