/**
 * @file opcua_client.cpp
 * @brief ESP32 as an OPC UA client, powered by the vendored open62541 stack.
 */
#include "opcua_client.h"

#include <stdio.h>
#include <string.h>

#include <open62541.h>

#include "opcua_config.h"
#include "opcua_settings.h"

namespace opcua {

namespace {

UA_Client *g_client = nullptr;
State g_state = State::Idle;
char g_lastError[96];
uint32_t g_nextAttemptMs = 0;
uint32_t g_lastPollMs = 0;

uint32_t g_readsOk = 0;
uint32_t g_readsFailed = 0;
uint32_t g_lastReadAt = 0;

TagValue g_cache[TAG_SLOTS];

/* ---------------------------------------------------------------------- */
/* Helpers                                                                */
/* ---------------------------------------------------------------------- */

void setError(const char *text)
{
    strncpy(g_lastError, text, sizeof(g_lastError) - 1);
    g_lastError[sizeof(g_lastError) - 1] = '\0';
    printf("OPC UA client: %s\n", g_lastError);
}

void setErrorCode(UA_StatusCode code, const char *context)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s: %s (0x%08X)",
             context, UA_StatusCode_name(code), code);
    setError(buffer);
}

void formatNodeid(size_t slot, char *out, size_t capacity)
{
    const settings::TagSlot &s = settings::get().tags[slot];
    if (s.stringId) {
        snprintf(out, capacity, "ns=%u;s=%s", s.ns, s.identStr);
    } else {
        snprintf(out, capacity, "ns=%u;i=%u", s.ns, s.identNum);
    }
}

void buildNodeId(const settings::TagSlot &s, UA_NodeId *out)
{
    if (s.stringId) {
        *out = UA_NODEID_STRING_ALLOC(s.ns, s.identStr);
    } else {
        *out = UA_NODEID_NUMERIC(s.ns, s.identNum);
    }
}

void formatVariant(const UA_Variant *value, char *out, size_t capacity)
{
    out[0] = '\0';
    if (!value || !value->type) {
        strncpy(out, "(null)", capacity - 1);
        out[capacity - 1] = '\0';
        return;
    }

    if (UA_Variant_isScalar(value)) {
        const void *data = value->data;
        switch (value->type->typeId.identifier.numeric) {
        case UA_NS0ID_BOOLEAN:
            snprintf(out, capacity, "%s", *static_cast<const UA_Boolean *>(data) ? "true" : "false");
            return;
        case UA_NS0ID_SBYTE:
            snprintf(out, capacity, "%d", *static_cast<const UA_SByte *>(data)); return;
        case UA_NS0ID_BYTE:
            snprintf(out, capacity, "%u", *static_cast<const UA_Byte *>(data)); return;
        case UA_NS0ID_INT16:
            snprintf(out, capacity, "%d", *static_cast<const UA_Int16 *>(data)); return;
        case UA_NS0ID_UINT16:
            snprintf(out, capacity, "%u", *static_cast<const UA_UInt16 *>(data)); return;
        case UA_NS0ID_INT32:
            snprintf(out, capacity, "%d", *static_cast<const UA_Int32 *>(data)); return;
        case UA_NS0ID_UINT32:
            snprintf(out, capacity, "%u", *static_cast<const UA_UInt32 *>(data)); return;
        case UA_NS0ID_INT64:
            snprintf(out, capacity, "%lld",
                     static_cast<long long>(*static_cast<const UA_Int64 *>(data))); return;
        case UA_NS0ID_UINT64:
            snprintf(out, capacity, "%llu",
                     static_cast<unsigned long long>(*static_cast<const UA_UInt64 *>(data))); return;
        case UA_NS0ID_FLOAT:
            snprintf(out, capacity, "%.3f", *static_cast<const UA_Float *>(data)); return;
        case UA_NS0ID_DOUBLE:
            snprintf(out, capacity, "%.3f", *static_cast<const UA_Double *>(data)); return;
        case UA_NS0ID_STRING: {
            const UA_String *s = static_cast<const UA_String *>(data);
            size_t n = s->length < capacity - 1 ? s->length : capacity - 1;
            memcpy(out, s->length ? reinterpret_cast<const char *>(s->data) : "", n);
            out[n] = '\0';
            return;
        }
        case UA_NS0ID_DATETIME: {
            UA_DateTime dt = *static_cast<const UA_DateTime *>(data);
            /* 100 ns ticks since 1601-01-01 -> ms since Unix epoch. */
            long long ms = static_cast<long long>(
                (dt - UA_DATETIME_UNIX_EPOCH) / UA_DATETIME_MSEC);
            snprintf(out, capacity, "%lld", ms);
            return;
        }
        default:
            break;
        }
        snprintf(out, capacity, "(%s)", value->type->typeName);
    } else {
        snprintf(out, capacity, "(array %ux%s)",
                 static_cast<unsigned>(value->arrayLength), value->type->typeName);
    }
}

void applyResult(size_t slot, const UA_DataValue *dv)
{
    TagValue &out = g_cache[slot];
    out.updatedAtMs = millis();
    out.status = dv ? dv->status : UA_STATUSCODE_BADINTERNALERROR;

    if (!dv) {
        out.ok = false;
        return;
    }
    if (dv->hasValue && dv->value.type) {
        formatVariant(&dv->value, out.value, sizeof(out.value));
        out.ok = true;
        out.status = dv->hasStatus ? dv->status : UA_STATUSCODE_GOOD;
    } else {
        out.ok = false;
        strncpy(out.value, UA_StatusCode_name(dv->status), sizeof(out.value) - 1);
        out.value[sizeof(out.value) - 1] = '\0';
    }

    out.sourceTsMs = 0;
    if (dv->hasSourceTimestamp) {
        out.sourceTsMs = static_cast<long long>(
            (dv->sourceTimestamp - UA_DATETIME_UNIX_EPOCH) / UA_DATETIME_MSEC);
    }
}

/* ---------------------------------------------------------------------- */
/* Connection + polling                                                   */
/* ---------------------------------------------------------------------- */

void teardownClient()
{
    if (g_client) {
        UA_Client_disconnect(g_client);
        UA_Client_delete(g_client);
        g_client = nullptr;
    }
}

bool doConnect()
{
    teardownClient();

    const settings::Settings &s = settings::get();
    g_client = UA_Client_new();
    if (!g_client) {
        setError("client allocation failed");
        g_state = State::Error;
        g_nextAttemptMs = millis() + OPC_RECONNECT_BACKOFF_MS;
        return false;
    }

    UA_ClientConfig *cfg = UA_Client_getConfig(g_client);
    UA_StatusCode rv = UA_ClientConfig_setDefault(cfg);
    if (rv != UA_STATUSCODE_GOOD) {
        setErrorCode(rv, "client config");
        teardownClient();
        g_state = State::Error;
        g_nextAttemptMs = millis() + OPC_RECONNECT_BACKOFF_MS;
        return false;
    }
    cfg->timeout = OPC_CLIENT_TIMEOUT_MS;
    if (s.username[0]) {
        /* Without encryption the credentials travel unencrypted inside the
         * ActivateSession request; use only on trusted networks. */
        UA_ClientConfig_setAuthenticationUsername(cfg, s.username, s.password);
    }

    printf("OPC UA client: connecting %s\n", s.serverUrl);
    rv = UA_Client_connect(g_client, s.serverUrl);
    if (rv != UA_STATUSCODE_GOOD) {
        setErrorCode(rv, "connect");
        teardownClient();
        g_state = State::Error;
        g_nextAttemptMs = millis() + OPC_RECONNECT_BACKOFF_MS;
        return false;
    }

    g_state = State::Connected;
    g_lastPollMs = 0;   /* poll immediately */
    setError("");
    printf("OPC UA client: connected to %s\n", s.serverUrl);
    return true;
}

void pollOnce()
{
    if (!g_client || g_state != State::Connected) {
        return;
    }

    /* Collect enabled slots. */
    settings::Settings &s = settings::get();
    UA_ReadValueId items[TAG_SLOTS];
    size_t count = 0;
    size_t slotOf[TAG_SLOTS];

    for (size_t i = 0; i < TAG_SLOTS; ++i) {
        if (!s.tags[i].enabled || !s.tags[i].identStr[0] && !s.tags[i].identNum) {
            continue;
        }
        UA_ReadValueId_init(&items[count]);
        buildNodeId(s.tags[i], &items[count].nodeId);
        items[count].attributeId = UA_ATTRIBUTEID_VALUE;
        slotOf[count] = i;
        ++count;
    }

    if (count == 0) {
        g_lastReadAt = millis();
        return;
    }

    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    request.nodesToRead = items;
    request.nodesToReadSize = count;
    request.maxAge = 0;
    request.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;

    UA_ReadResponse response = UA_Client_Service_read(g_client, request);

    const UA_StatusCode result = response.responseHeader.serviceResult;
    if (result != UA_STATUSCODE_GOOD) {
        ++g_readsFailed;
        setErrorCode(result, "read");
        /* Connection-level failures trigger a reconnect cycle. */
        switch (result) {
        case UA_STATUSCODE_BADCONNECTIONCLOSED:
        case UA_STATUSCODE_BADSERVERNOTCONNECTED:
        case UA_STATUSCODE_BADSECURECHANNELIDINVALID:
        case UA_STATUSCODE_BADTIMEOUT:
        case UA_STATUSCODE_BADCOMMUNICATIONERROR:
            teardownClient();
            g_state = State::Connecting;
            g_nextAttemptMs = millis() + OPC_RECONNECT_BACKOFF_MS;
            break;
        default:
            break;
        }
    } else {
        ++g_readsOk;
        g_lastReadAt = millis();
        for (size_t i = 0; i < count && i < response.resultsSize; ++i) {
            applyResult(slotOf[i], &response.results[i]);
        }
        setError("");
    }

    for (size_t i = 0; i < count; ++i) {
        UA_NodeId_clear(&items[i].nodeId);
    }
    UA_ReadResponse_clear(&response);
}

} /* namespace */

void start()
{
    memset(g_cache, 0, sizeof(g_cache));
    setError("");
    g_state = State::Connecting;
    g_nextAttemptMs = millis() + 2000;   /* give Wi-Fi a moment */
}

void stop()
{
    teardownClient();
    g_state = State::Idle;
}

void iterate()
{
    uint32_t now = millis();

    /* Connecting: first (or user-forced) attempt. Error: a failed attempt
     * scheduled for another try after the backoff, matching the documented
     * "connection failures retry every 10 s" behaviour. */
    if ((g_state == State::Connecting || g_state == State::Error) &&
        g_nextAttemptMs && now >= g_nextAttemptMs) {
        g_nextAttemptMs = 0;
        doConnect();
    } else if (g_state == State::Connected) {
        uint32_t pollMs = settings::get().pollMs;
        if (pollMs < 250) {
            pollMs = 250;
        }
        if (!g_lastPollMs || now - g_lastPollMs >= pollMs) {
            g_lastPollMs = now;
            pollOnce();
        }
    }
}

const TagValue *tags()
{
    static TagValue snapshot[TAG_SLOTS];
    const settings::Settings &s = settings::get();
    for (size_t i = 0; i < TAG_SLOTS; ++i) {
        TagValue &out = snapshot[i];
        memset(&out, 0, sizeof(out));
        out.enabled = s.tags[i].enabled;
        formatNodeid(i, out.nodeid, sizeof(out.nodeid));
        strncpy(out.name, s.tags[i].name, sizeof(out.name) - 1);
        out.ok = g_cache[i].ok;
        strncpy(out.value, g_cache[i].ok ? g_cache[i].value : "", sizeof(out.value) - 1);
        out.status = g_cache[i].status;
        out.sourceTsMs = g_cache[i].sourceTsMs;
        out.updatedAtMs = g_cache[i].updatedAtMs;
    }
    return snapshot;
}

void connectNow()
{
    g_state = State::Connecting;
    g_nextAttemptMs = millis() + 100;   /* on the next iterate */
}

void disconnectNow()
{
    teardownClient();
    g_state = State::Idle;
    g_nextAttemptMs = 0;
    setError("");
}

State state()
{
    return g_state;
}

String lastError()
{
    return String(g_lastError);
}

uint32_t readsOk()
{
    return g_readsOk;
}

uint32_t readsFailed()
{
    return g_readsFailed;
}

uint32_t lastReadAt()
{
    return g_lastReadAt;
}

} /* namespace opcua */
