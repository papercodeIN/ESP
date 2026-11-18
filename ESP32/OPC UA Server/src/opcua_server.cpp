/**
 * @file opcua_server.cpp
 * @brief ESP32 as an OPC UA server, powered by the vendored open62541 stack.
 */
#include "opcua_server.h"

#include <WiFi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <open62541.h>

#include "opcua_config.h"
#include "opcua_settings.h"

namespace opcua {

namespace {

UA_Server *g_server = nullptr;
bool g_running = false;
uint16_t g_port = OPC_DEFAULT_PORT;
String g_endpointUrl;

uint32_t g_bootMs = 0;

/* ---------------------------------------------------------------------- */
/* Tag table                                                              */
/* ---------------------------------------------------------------------- */

enum TagId {
    TAG_DEVICENAME = 0,
    TAG_TEMPERATURE,
    TAG_UPTIME,
    TAG_FREEHEAP,
    TAG_RSSI,
    TAG_SINE,
    TAG_SETPOINT,
    TAG_RELAY,
    TAG_BUTTON,
};

struct TagDef {
    const char *id;
    const char *name;
    const char *type;
    uint16_t uaType;       /* UA_TYPES_... index */
    bool writable;
};

const TagDef k_tags[TAG_COUNT] = {
    { "DeviceName",  "DeviceName",  "String",  UA_TYPES_STRING,  false },
    { "Temperature", "Temperature", "Double",  UA_TYPES_DOUBLE,  false },
    { "Uptime",      "Uptime",      "Double",  UA_TYPES_DOUBLE,  false },
    { "FreeHeap",    "FreeHeap",    "Double",  UA_TYPES_DOUBLE,  false },
    { "WiFiRSSI",    "WiFiRSSI",    "Double",  UA_TYPES_DOUBLE,  false },
    { "Sine",        "Sine",        "Double",  UA_TYPES_DOUBLE,  false },
    { "Setpoint",    "Setpoint",    "Double",  UA_TYPES_DOUBLE,  true  },
    { "RelayOutput", "RelayOutput", "Boolean", UA_TYPES_BOOLEAN, true  },
    { "ButtonInput", "ButtonInput", "Boolean", UA_TYPES_BOOLEAN, false },
};

/* Runtime state of the two writable tags plus a value cache for the web. */
struct TagCache {
    bool valid;
    char value[VALUE_TEXT_MAX];
    uint32_t changedAtMs;
};

TagCache g_cache[TAG_COUNT];
double g_setpoint = 21.5;
bool g_relay = false;
bool g_relayPinReady = false;

int findTag(const char *id)
{
    if (!id) {
        return -1;
    }
    for (size_t i = 0; i < TAG_COUNT; ++i) {
        if (strcmp(k_tags[i].id, id) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const char *typeName(uint16_t uaType)
{
    switch (uaType) {
    case UA_TYPES_STRING:  return "String";
    case UA_TYPES_DOUBLE:  return "Double";
    case UA_TYPES_BOOLEAN: return "Boolean";
    default:               return "BaseDataType";
    }
}

/* ---------------------------------------------------------------------- */
/* Live process values                                                    */
/* ---------------------------------------------------------------------- */

double chipTemperature()
{
    return temperatureRead();
}

double sineValue()
{
    /* 0..100 %, 60 s period, for demo dashboards. */
    double phase = (millis() % 60000) / 60000.0;
    return 50.0 + 50.0 * sin(phase * 2.0 * M_PI);
}

bool bootButtonPressed()
{
    return digitalRead(OPC_BUTTON_PIN) == LOW;
}

void setRelay(bool on)
{
    g_relay = on;
    if (!g_relayPinReady) {
        pinMode(OPC_RELAY_PIN, OUTPUT);
        g_relayPinReady = true;
    }
    digitalWrite(OPC_RELAY_PIN, on ? HIGH : LOW);
}

void storeValue(size_t index, const char *text)
{
    TagCache &slot = g_cache[index];
    uint32_t now = millis();
    if (!slot.valid || strcmp(slot.value, text) != 0) {
        slot.changedAtMs = now;
    }
    strncpy(slot.value, text, sizeof(slot.value) - 1);
    slot.value[sizeof(slot.value) - 1] = '\0';
    slot.valid = true;
}

void formatDouble(char *out, size_t capacity, double value)
{
    snprintf(out, capacity, "%.3f", value);
}

void formatBool(char *out, size_t capacity, bool value)
{
    strncpy(out, value ? "true" : "false", capacity - 1);
    out[capacity - 1] = '\0';
}

void readLiveValue(size_t index, char *out, size_t capacity)
{
    switch (index) {
    case TAG_DEVICENAME:
        strncpy(out, settings::get().deviceName, capacity - 1);
        break;
    case TAG_TEMPERATURE:
        formatDouble(out, capacity, chipTemperature());
        break;
    case TAG_UPTIME:
        formatDouble(out, capacity, (millis() - g_bootMs) / 1000.0);
        break;
    case TAG_FREEHEAP:
        formatDouble(out, capacity, static_cast<double>(ESP.getFreeHeap()));
        break;
    case TAG_RSSI:
        formatDouble(out, capacity, static_cast<double>(WiFi.RSSI()));
        break;
    case TAG_SINE:
        formatDouble(out, capacity, sineValue());
        break;
    case TAG_SETPOINT:
        formatDouble(out, capacity, g_setpoint);
        break;
    case TAG_RELAY:
        formatBool(out, capacity, g_relay);
        break;
    case TAG_BUTTON:
        formatBool(out, capacity, bootButtonPressed());
        break;
    default:
        out[0] = '\0';
        break;
    }
    out[capacity - 1] = '\0';
}

void writeLiveValue(size_t index, const char *text)
{
    switch (index) {
    case TAG_SETPOINT:
        g_setpoint = atof(text);
        settings::get().setpoint = g_setpoint;
        settings::save();
        break;
    case TAG_RELAY:
        setRelay(strcmp(text, "true") == 0 || strcmp(text, "1") == 0 ||
                 strcmp(text, "TRUE") == 0);
        break;
    default:
        break;
    }
}

UA_StatusCode applyVariant(size_t index, UA_DataValue *value, bool includeStamp)
{
    const TagDef &def = k_tags[index];
    char text[VALUE_TEXT_MAX];
    readLiveValue(index, text, sizeof(text));

    UA_StatusCode rv = UA_STATUSCODE_GOOD;
    switch (def.uaType) {
    case UA_TYPES_STRING: {
        UA_String s = UA_STRING(text);
        rv = UA_Variant_setScalarCopy(&value->value, &s, &UA_TYPES[UA_TYPES_STRING]);
        break;
    }
    case UA_TYPES_DOUBLE: {
        UA_Double d = atof(text);
        rv = UA_Variant_setScalarCopy(&value->value, &d, &UA_TYPES[UA_TYPES_DOUBLE]);
        break;
    }
    case UA_TYPES_BOOLEAN: {
        UA_Boolean b = (strcmp(text, "true") == 0);
        rv = UA_Variant_setScalarCopy(&value->value, &b, &UA_TYPES[UA_TYPES_BOOLEAN]);
        break;
    }
    default:
        break;
    }

    if (rv != UA_STATUSCODE_GOOD) {
        return rv;   /* e.g. allocation failure: don't report an empty value */
    }
    value->hasValue = true;
    if (includeStamp) {
        value->hasSourceTimestamp = true;
        value->sourceTimestamp = UA_DateTime_now();
    }
    return UA_STATUSCODE_GOOD;
}

void saveVariantToState(size_t index, const UA_DataValue *value)
{
    const TagDef &def = k_tags[index];
    if (!value->hasValue || !value->value.type) {
        return;
    }
    char text[VALUE_TEXT_MAX];

    if (def.uaType == UA_TYPES_STRING) {
        /* The device name stays authoritative in the settings; string tags
         * are read-only by definition. */
        return;
    }

    if (def.uaType == UA_TYPES_DOUBLE &&
        value->value.type->typeId.identifier.numeric == UA_TYPES_DOUBLE) {
        formatDouble(text, sizeof(text), *static_cast<const UA_Double *>(value->value.data));
    } else if (def.uaType == UA_TYPES_BOOLEAN &&
               value->value.type->typeId.identifier.numeric == UA_TYPES_BOOLEAN) {
        formatBool(text, sizeof(text), *static_cast<const UA_Boolean *>(value->value.data));
    } else {
        return;
    }
    writeLiveValue(index, text);
}

/* ---------------------------------------------------------------------- */
/* open62541 callbacks                                                    */
/* ---------------------------------------------------------------------- */

UA_StatusCode onRead(UA_Server *server, const UA_NodeId *sessionId,
                     void *sessionContext, const UA_NodeId *nodeId,
                     void *nodeContext, UA_Boolean includeSourceTimeStamp,
                     const UA_NumericRange *range, UA_DataValue *value)
{
    (void)server;
    (void)sessionId;
    (void)sessionContext;
    (void)nodeId;

    if (range) {
        return UA_STATUSCODE_BADINDEXRANGEINVALID;
    }
    intptr_t index = reinterpret_cast<intptr_t>(nodeContext);
    if (index < 0 || index >= TAG_COUNT) {
        return UA_STATUSCODE_BADNODEIDUNKNOWN;
    }
    UA_DataValue_init(value);
    return applyVariant(static_cast<size_t>(index), value, includeSourceTimeStamp);
}

UA_StatusCode onWrite(UA_Server *server, const UA_NodeId *sessionId,
                      void *sessionContext, const UA_NodeId *nodeId,
                      void *nodeContext, const UA_NumericRange *range,
                      const UA_DataValue *value)
{
    (void)server;
    (void)sessionId;
    (void)sessionContext;
    (void)nodeId;

    if (range) {
        return UA_STATUSCODE_BADINDEXRANGEINVALID;
    }
    intptr_t index = reinterpret_cast<intptr_t>(nodeContext);
    if (index < 0 || index >= TAG_COUNT) {
        return UA_STATUSCODE_BADNODEIDUNKNOWN;
    }
    if (!k_tags[index].writable) {
        return UA_STATUSCODE_BADWRITENOTSUPPORTED;
    }
    saveVariantToState(static_cast<size_t>(index), value);
    return UA_STATUSCODE_GOOD;
}

/* ---------------------------------------------------------------------- */
/* Node creation                                                          */
/* ---------------------------------------------------------------------- */

void addTagNode(const TagDef &def, size_t index)
{
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en", def.name);
    attr.dataType = UA_TYPES[def.uaType].typeId;
    attr.valueRank = UA_VALUERANK_SCALAR;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ |
                       (def.writable ? UA_ACCESSLEVELMASK_WRITE : 0);
    attr.writeMask = 0;

    /* Seed value so the node is valid before the first read callback. */
    char text[VALUE_TEXT_MAX];
    readLiveValue(index, text, sizeof(text));
    if (def.uaType == UA_TYPES_STRING) {
        UA_String s = UA_STRING(text);
        UA_Variant_setScalarCopy(&attr.value, &s, &UA_TYPES[UA_TYPES_STRING]);
    } else if (def.uaType == UA_TYPES_DOUBLE) {
        UA_Double d = atof(text);
        UA_Variant_setScalarCopy(&attr.value, &d, &UA_TYPES[UA_TYPES_DOUBLE]);
    } else if (def.uaType == UA_TYPES_BOOLEAN) {
        UA_Boolean b = (strcmp(text, "true") == 0);
        UA_Variant_setScalarCopy(&attr.value, &b, &UA_TYPES[UA_TYPES_BOOLEAN]);
    }

    UA_DataSource source;
    source.read = onRead;
    source.write = onWrite;

    UA_NodeId node = UA_NODEID_STRING_ALLOC(1, def.id);
    UA_QualifiedName browse = UA_QUALIFIEDNAME_ALLOC(1, def.id);
    UA_StatusCode rv = UA_Server_addDataSourceVariableNode(
        g_server, node,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        browse,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr, source,
        reinterpret_cast<void *>(static_cast<intptr_t>(index)), nullptr);

    UA_LocalizedText_clear(&attr.displayName);
    UA_Variant_clear(&attr.value);
    UA_NodeId_clear(&node);
    UA_QualifiedName_clear(&browse);

    if (rv != UA_STATUSCODE_GOOD) {
        printf("OPC UA: add node %s failed: 0x%08X\n", def.id, rv);
    }
}

/* Deferred reboot: the method callback only arms the flag; the restart
 * happens in iterate() once the response has been sent. */
volatile bool g_rebootRequested = false;

UA_StatusCode rebootCallback(UA_Server *server, const UA_NodeId *sessionId,
                             void *sessionContext, const UA_NodeId *methodId,
                             void *methodContext, const UA_NodeId *objectId,
                             void *objectContext, size_t inputSize,
                             const UA_Variant *input, size_t outputSize,
                             UA_Variant *output)
{
    (void)server;
    (void)sessionId;
    (void)sessionContext;
    (void)methodId;
    (void)methodContext;
    (void)objectId;
    (void)objectContext;
    (void)inputSize;
    (void)input;
    (void)outputSize;
    (void)output;
    g_rebootRequested = true;
    return UA_STATUSCODE_GOOD;
}

void addRebootMethod()
{
    UA_MethodAttributes attr = UA_MethodAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en", "RebootDevice");
    attr.executable = true;
    attr.userExecutable = true;

    UA_NodeId node = UA_NODEID_STRING_ALLOC(1, "RebootDevice");
    UA_QualifiedName browse = UA_QUALIFIEDNAME_ALLOC(1, "RebootDevice");
    UA_StatusCode rv = UA_Server_addMethodNode(
        g_server, node,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
        browse,
        attr, rebootCallback,
        0, nullptr, 0, nullptr,
        nullptr, nullptr);

    UA_LocalizedText_clear(&attr.displayName);
    UA_NodeId_clear(&node);
    UA_QualifiedName_clear(&browse);

    if (rv == UA_STATUSCODE_GOOD) {
        UA_Server_setMethodNodeCallback(g_server,
            UA_NODEID_STRING(1, "RebootDevice"), rebootCallback);
    } else {
        printf("OPC UA: add method failed: 0x%08X\n", rv);
    }
}

} /* namespace */

/* ---------------------------------------------------------------------- */
/* Public interface                                                       */
/* ---------------------------------------------------------------------- */

bool start(uint16_t port)
{
    if (g_running) {
        return true;
    }

    g_port = port;
    g_bootMs = millis();
    g_setpoint = settings::get().setpoint;
    setRelay(false);
    memset(g_cache, 0, sizeof(g_cache));

    /* Build the endpoint URL from the current network address. */
    {
        String host = settings::get().endpointHost;
        if (!host.length()) {
            if (WiFi.getMode() & WIFI_MODE_STA && WiFi.status() == WL_CONNECTED) {
                host = WiFi.localIP().toString();
            } else {
                host = WiFi.softAPIP().toString();
            }
        }
        g_endpointUrl = String("opc.tcp://") + host + ":" + String(port);
    }

    g_server = UA_Server_new();
    if (!g_server) {
        printf("OPC UA: server allocation failed\n");
        return false;
    }

    UA_ServerConfig *cfg = UA_Server_getConfig(g_server);
    UA_StatusCode rv = UA_ServerConfig_setMinimal(cfg, port, nullptr);
    if (rv != UA_STATUSCODE_GOOD) {
        printf("OPC UA: config failed: 0x%08X\n", rv);
        UA_Server_delete(g_server);
        g_server = nullptr;
        return false;
    }

    /* Right-size for ESP32 RAM. */
    cfg->maxSessions = OPC_MAX_SESSIONS;
    cfg->maxSecureChannels = OPC_MAX_SECURECHANNELS;
    cfg->tcpBufSize = OPC_TCP_BUFFER_SIZE;

    /* Application identity. */
    UA_String_clear(&cfg->applicationDescription.applicationUri);
    cfg->applicationDescription.applicationUri = UA_STRING_ALLOC(OPC_APP_URI);
    UA_String_clear(&cfg->applicationDescription.productUri);
    cfg->applicationDescription.productUri = UA_STRING_ALLOC(OPC_PRODUCT_URI);
    UA_LocalizedText_clear(&cfg->applicationDescription.applicationName);
    cfg->applicationDescription.applicationName =
        UA_LOCALIZEDTEXT_ALLOC("en", settings::get().deviceName);

    for (size_t i = 0; i < cfg->endpointsSize; ++i) {
        UA_String_clear(&cfg->endpoints[i].server.applicationUri);
        cfg->endpoints[i].server.applicationUri = UA_STRING_ALLOC(OPC_APP_URI);
        UA_String_clear(&cfg->endpoints[i].endpointUrl);
        cfg->endpoints[i].endpointUrl = UA_STRING_ALLOC(g_endpointUrl.c_str());
    }

    /* Namespace 1 holds our process tags. */
    UA_Server_addNamespace(g_server, OPC_NAMESPACE_URI);

    for (size_t i = 0; i < TAG_COUNT; ++i) {
        addTagNode(k_tags[i], i);
    }
    addRebootMethod();

    rv = UA_Server_run_startup(g_server);
    if (rv != UA_STATUSCODE_GOOD) {
        printf("OPC UA: run_startup failed: 0x%08X\n", rv);
        UA_Server_delete(g_server);
        g_server = nullptr;
        return false;
    }

    g_running = true;
    printf("OPC UA: server on %s\n", g_endpointUrl.c_str());
    return true;
}

void stop()
{
    if (!g_running || !g_server) {
        return;
    }
    UA_Server_run_shutdown(g_server);
    UA_Server_delete(g_server);
    g_server = nullptr;
    g_running = false;
}

void iterate()
{
    if (g_running && g_server) {
        UA_Server_run_iterate(g_server, OPC_ITERATE_MS);
    }
    if (g_rebootRequested) {
        g_rebootRequested = false;
        stop();
        ESP.restart();
    }
}

void refreshCache()
{
    for (size_t i = 0; i < TAG_COUNT; ++i) {
        char text[VALUE_TEXT_MAX];
        readLiveValue(i, text, sizeof(text));
        storeValue(i, text);
    }
}

const TagValue *tags()
{
    /* Exposed through opcua::tags() as a fresh copy so the web layer can
     * serialize it without touching open62541 types. */
    static TagValue snapshot[TAG_COUNT];
    for (size_t i = 0; i < TAG_COUNT; ++i) {
        TagValue &out = snapshot[i];
        memset(&out, 0, sizeof(out));
        strncpy(out.id, k_tags[i].id, sizeof(out.id) - 1);
        strncpy(out.name, k_tags[i].name, sizeof(out.name) - 1);
        strncpy(out.type, typeName(k_tags[i].uaType), sizeof(out.type) - 1);
        out.writable = k_tags[i].writable;
        out.valid = g_cache[i].valid;
        strncpy(out.value, g_cache[i].value, sizeof(out.value) - 1);
        out.changedAtMs = g_cache[i].changedAtMs;
    }
    return snapshot;
}

bool writeTag(const char *id, const char *text)
{
    int index = findTag(id);
    if (index < 0 || !k_tags[index].writable || !text) {
        return false;
    }
    writeLiveValue(static_cast<size_t>(index), text);

    char check[VALUE_TEXT_MAX];
    readLiveValue(static_cast<size_t>(index), check, sizeof(check));
    storeValue(static_cast<size_t>(index), check);
    return true;
}

String endpointUrl()
{
    return g_endpointUrl;
}

bool running()
{
    return g_running;
}

} /* namespace opcua */
