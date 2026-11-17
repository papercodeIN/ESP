/**
 * @file opcua_client.h
 * @brief ESP32 as an OPC UA client, powered by the vendored open62541 stack.
 *
 * Connects to any OPC UA server (security policy None, optional
 * username/password login) and polls the configured tags with a batched Read
 * service call. The live values are served through the web API.
 */
#pragma once

#include <Arduino.h>

#include "opcua_config.h"

namespace opcua {

constexpr size_t TAG_SLOTS = OPC_TAG_SLOTS;
constexpr size_t VALUE_TEXT_MAX = 48;

struct TagValue {
    bool enabled;
    char nodeid[80];      /* e.g. "ns=1;s=Temperature" or "ns=0;i=2258" */
    char name[40];
    bool ok;              /* last read returned a value */
    char value[VALUE_TEXT_MAX];
    uint32_t status;      /* OPC UA status code of the last read */
    int64_t sourceTsMs;   /* source timestamp, ms since Unix epoch (0 = none) */
    uint32_t updatedAtMs;
};

enum class State {
    Idle,         /* no client configured / disconnected by user */
    Connecting,   /* waiting to retry or in the middle of a connect */
    Connected,
    Error,
};

/* Builds the client; Wi-Fi must be up. Does not connect yet. */
void start();

/* Shuts the client down (before Wi-Fi changes / reboot). */
void stop();

/* State machine + poll scheduler; call from loop() frequently. */
void iterate();

/* Snapshot of the tag cache for the web API. */
const TagValue *tags();

/* Manual control from the web API. */
void connectNow();
void disconnectNow();

State state();
String lastError();
uint32_t readsOk();
uint32_t readsFailed();
uint32_t lastReadAt();

} /* namespace opcua */
