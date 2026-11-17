/**
 * @file opcua_server.h
 * @brief ESP32 as an OPC UA server, powered by the vendored open62541 stack.
 *
 * The server listens on opc.tcp://<ip>:<port> (default 4840) and exposes a
 * small process picture in namespace 1, with string node identifiers:
 *
 *   ns=1;s=DeviceName   String    RO   configured device name
 *   ns=1;s=Temperature  Double    RO   chip temperature, degC
 *   ns=1;s=Uptime       Double    RO   uptime, seconds
 *   ns=1;s=FreeHeap     Double    RO   free heap, bytes
 *   ns=1;s=WiFiRSSI     Double    RO   Wi-Fi signal, dBm
 *   ns=1;s=Sine         Double    RO   simulated process value 0..100
 *   ns=1;s=Setpoint     Double    RW   persisted in NVS
 *   ns=1;s=RelayOutput  Boolean   RW   GPIO2 / onboard LED
 *   ns=1;s=ButtonInput  Boolean   RO   BOOT button (GPIO0, active low)
 *   ns=1;s=RebootDevice Method   -     restarts the ESP32
 */
#pragma once

#include <Arduino.h>

namespace opcua {

/* Maximum stringified tag value the web API needs to carry. */
constexpr size_t VALUE_TEXT_MAX = 48;

struct TagValue {
    char id[32];
    char name[32];
    char type[12];
    bool writable;
    bool valid;
    char value[VALUE_TEXT_MAX];
    uint32_t changedAtMs;
};

constexpr size_t TAG_COUNT = 9;

/* Starts the server; call after Wi-Fi has an address so the endpoint URL
 * can be built. Returns false when the stack could not be brought up. */
bool start(uint16_t port);

/* Stops the server (before Wi-Fi changes / reboot). */
void stop();

/* Non-blocking iterate; call this from loop() every OPC_ITERATE_MS. */
void iterate();

/* Refreshes the cached tag values shown by the web API. */
void refreshCache();

/* Tag snapshot for the web API. */
const TagValue *tags();

/* Writes a tag from the web API. Text input, "true"/"false" for booleans.
 * Returns false when the tag does not exist or is not writable. */
bool writeTag(const char *id, const char *text);

/* Endpoint URL as advertised in GetEndpoints, e.g. opc.tcp://10.0.0.5:4840 */
String endpointUrl();

bool running();

} /* namespace opcua */
