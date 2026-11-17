/**
 * @file bacnet_config.h
 * @brief Compile-time defaults for the XIAO ESP32C3 BACnet/IP device.
 *
 * These values are only used to seed the settings stored in flash the very
 * first time the device boots (or after a factory reset). Normal operation
 * is configured from the built-in web interface, so you usually do not need
 * to edit this file at all.
 */
#pragma once

/* ------------------------------------------------------------------------- */
/* Default Wi-Fi credentials                                                 */
/* ------------------------------------------------------------------------- */
/* The device joins this network automatically on first boot (and after a
 * factory reset). Leave both empty instead to start in setup mode: join the
 * open "bacnet-...-XXXX" access point and configure Wi-Fi at
 * http://192.168.4.1.
 *
 * Note: values set here are compiled into the firmware binary. */
#define BACNET_DEFAULT_WIFI_SSID "Capgemini_4G"
#define BACNET_DEFAULT_WIFI_PASSWORD "MN704116"

#define BACNET_DEFAULT_HOSTNAME "bacnet-xiao-c3"

/* Password of the temporary setup access point. Empty string = open AP. */
#define BACNET_DEFAULT_AP_PASSWORD ""

/* ------------------------------------------------------------------------- */
/* BACnet device identity                                                    */
/* ------------------------------------------------------------------------- */
/* Must be unique on the BACnet internetwork (0 .. 4194302). */
#define BACNET_DEFAULT_DEVICE_INSTANCE 1234

#define BACNET_DEFAULT_DEVICE_NAME "XIAO-ESP32C3-BACnet"
#define BACNET_DEFAULT_DEVICE_DESCRIPTION "BACnet/IP device on Seeed XIAO ESP32C3"
#define BACNET_DEFAULT_DEVICE_LOCATION "Lab"

/* Vendor ID 999 is the BACnet test/development vendor ID.
 * Request a real vendor ID from ASHRAE before shipping a product. */
#define BACNET_DEFAULT_VENDOR_ID 999
#define BACNET_DEFAULT_VENDOR_NAME "OpenCode Community"
#define BACNET_DEFAULT_MODEL_NAME "Seeed XIAO ESP32C3"
#define BACNET_DEFAULT_FIRMWARE_REVISION "1.0.5"
#define BACNET_DEFAULT_APP_VERSION "1.0.5"

/* BACnet/IP UDP port. 47808 = 0xBAC0, the standard port. */
#define BACNET_DEFAULT_PORT 47808

/* Protocol revision advertised in the Device object. */
#define BACNET_PROTOCOL_REVISION 22

/* ------------------------------------------------------------------------- */
/* Network defaults                                                          */
/* ------------------------------------------------------------------------- */
#define BACNET_DEFAULT_USE_STATIC_IP 0
#define BACNET_DEFAULT_STATIC_IP "192.168.1.50"
#define BACNET_DEFAULT_GATEWAY_IP "192.168.1.1"
#define BACNET_DEFAULT_SUBNET_MASK "255.255.255.0"
#define BACNET_DEFAULT_DNS_IP "8.8.8.8"

/* Built-in web interface (configuration + live points). */
#define BACNET_DEFAULT_WEB_ENABLED 1
#define BACNET_WEB_PORT 80

/* ------------------------------------------------------------------------- */
/* Demo I/O                                                                  */
/* ------------------------------------------------------------------------- */
/* BO 0 is wired to this pin (D0 / GPIO2 on the XIAO ESP32C3).
 * Set to -1 to leave the point software-only. */
#define BACNET_BO0_GPIO 2

/* BI 1 follows the BOOT button (GPIO9, active low on the XIAO ESP32C3).
 * Set to -1 to disable. */
#define BACNET_BI1_GPIO 9

/* ------------------------------------------------------------------------- */
/* Debug output                                                              */
/* ------------------------------------------------------------------------- */
#define BACNET_ENABLE_SERIAL 1
#define BACNET_SERIAL_BAUD 115200
