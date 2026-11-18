/**
 * @file bacnet_server.cpp
 * @brief BACnet/IP (Annex J) server implementation.
 */
#include "bacnet_server.h"

#include <math.h>
#include <string.h>

#include "bacnet_config.h"
#include "bacnet_objects.h"
#include "bacnet_settings.h"

namespace bacnet {
namespace {

#if BACNET_ENABLE_SERIAL
#define BACNET_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define BACNET_LOG(...) \
    do {                \
    } while (0)
#endif

constexpr uint8_t kPduConfirmedRequest = 0x00;
constexpr uint8_t kPduUnconfirmedRequest = 0x10;
constexpr uint8_t kPduSimpleAck = 0x20;
constexpr uint8_t kPduComplexAck = 0x30;
constexpr uint8_t kPduError = 0x50;
constexpr uint8_t kPduReject = 0x60;
constexpr uint8_t kPduAbort = 0x70;

#if BACNET_ENABLE_SERIAL
/* Wire-level trace of APDUs so client interactions (e.g. Yabe's property
 * grid) can be diagnosed from the serial console. */
void logFrame(const char *direction, const uint8_t *data, size_t length)
{
    char hex[3 * 96 + 1] = {0};
    static const char digits[] = "0123456789ABCDEF";
    const size_t shown = length < 96 ? length : 96;
    for (size_t i = 0; i < shown; ++i) {
        hex[i * 3] = digits[data[i] >> 4];
        hex[i * 3 + 1] = digits[data[i] & 0x0F];
        hex[i * 3 + 2] = ' ';
    }
    hex[shown * 3 - 1] = '\0';
    Serial.printf("[BACnet] %s %uB %s%s\n", direction,
                  static_cast<unsigned>(length), hex,
                  length > shown ? "..." : "");
}
#else
void logFrame(const char *, const uint8_t *, size_t)
{
}
#endif

bool readContextUnsigned(ByteReader &reader,
                         uint8_t expectedTag,
                         uint32_t &value)
{
    uint8_t tag = 0;
    bool context = false;
    bool constructed = false;
    bool opening = false;
    bool closing = false;
    uint32_t length = 0;
    if (!reader.readTag(
            tag, context, constructed, opening, closing, length)) {
        return false;
    }
    if (!context || constructed || opening || closing || tag != expectedTag) {
        return false;
    }
    if (length < 1 || length > 4) {
        return false;
    }
    uint32_t result = 0;
    for (uint32_t i = 0; i < length; ++i) {
        uint8_t byte = 0;
        if (!reader.readU8(byte)) {
            return false;
        }
        result = (result << 8) | byte;
    }
    value = result;
    return true;
}

bool readContextObjectId(ByteReader &reader,
                         uint8_t expectedTag,
                         uint16_t &objectType,
                         uint32_t &instance)
{
    uint8_t tag = 0;
    bool context = false;
    bool constructed = false;
    bool opening = false;
    bool closing = false;
    uint32_t length = 0;
    if (!reader.readTag(
            tag, context, constructed, opening, closing, length)) {
        return false;
    }
    if (!context || constructed || opening || closing || tag != expectedTag ||
        length != 4) {
        return false;
    }
    uint32_t raw = 0;
    if (!reader.readU32(raw)) {
        return false;
    }
    splitObjectId(raw, objectType, instance);
    return true;
}

/* A context-tagged boolean carries its value in a content octet - unlike
 * the application-tagged boolean, which encodes it in the tag length. */
bool readContextBoolean(ByteReader &reader,
                        uint8_t expectedTag,
                        bool &value)
{
    uint8_t tag = 0;
    bool context = false;
    bool constructed = false;
    bool opening = false;
    bool closing = false;
    uint32_t length = 0;
    if (!reader.readTag(
            tag, context, constructed, opening, closing, length)) {
        return false;
    }
    if (!context || constructed || opening || closing || tag != expectedTag ||
        length != 1) {
        return false;
    }
    uint8_t raw = 0;
    if (!reader.readU8(raw)) {
        return false;
    }
    value = raw != 0;
    return true;
}

bool expectOpeningTag(ByteReader &reader, uint8_t expectedTag)
{
    uint8_t tag = 0;
    bool context = false;
    bool constructed = false;
    bool opening = false;
    bool closing = false;
    uint32_t length = 0;
    if (!reader.readTag(
            tag, context, constructed, opening, closing, length)) {
        return false;
    }
    return context && constructed && opening && !closing &&
           tag == expectedTag;
}

bool expectClosingTag(ByteReader &reader, uint8_t expectedTag)
{
    uint8_t tag = 0;
    bool context = false;
    bool constructed = false;
    bool opening = false;
    bool closing = false;
    uint32_t length = 0;
    if (!reader.readTag(
            tag, context, constructed, opening, closing, length)) {
        return false;
    }
    return context && constructed && closing && !opening &&
           tag == expectedTag;
}

bool readOptionalContextTag(ByteReader &reader, uint8_t expectedTag,
                            uint32_t &value)
{
    uint8_t tag = 0;
    bool context = false;
    bool constructed = false;
    bool opening = false;
    bool closing = false;
    uint32_t length = 0;
    if (!reader.peekTag(
            tag, context, constructed, opening, closing, length)) {
        return false;
    }
    if (!context || constructed || opening || closing || tag != expectedTag) {
        return false;
    }
    return readContextUnsigned(reader, expectedTag, value);
}

bool mapReadError(ReadStatus status, uint16_t &errorClass, uint32_t &errorCode)
{
    switch (status) {
        case ReadStatus::UnknownObject:
            errorClass = kErrorClassObject;
            errorCode = kErrorCodeUnknownObject;
            return true;
        case ReadStatus::UnknownProperty:
            errorClass = kErrorClassProperty;
            errorCode = kErrorCodeUnknownProperty;
            return true;
        case ReadStatus::InvalidArrayIndex:
            errorClass = kErrorClassProperty;
            errorCode = kErrorCodeInvalidArrayIndex;
            return true;
        case ReadStatus::PropertyIsNotArray:
            errorClass = kErrorClassProperty;
            errorCode = kErrorCodePropertyIsNotAnArray;
            return true;
        default:
            return false;
    }
}

void encodePropertyError(ByteWriter &writer,
                         uint16_t errorClass,
                         uint32_t errorCode)
{
    writer.openingTag(5);
    writer.appEnumerated(errorClass);
    writer.appEnumerated(errorCode);
    writer.closingTag(5);
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */
bool Server::begin(uint16_t port)
{
    port_ = port;
    udp_.stop();
    for (auto &subscription : cov_) {
        subscription.active = false;
    }
    if (!udp_.begin(port_)) {
        BACNET_LOG("[BACnet] Failed to bind UDP port %u\n",
                   static_cast<unsigned>(port_));
        return false;
    }
    startupMs_ = millis();
    lastStartupIAmMs_ = 0;
    BACNET_LOG("[BACnet] BACnet/IP listening on UDP %u\n",
               static_cast<unsigned>(port_));
    return true;
}

void Server::stop()
{
    udp_.stop();
    for (auto &subscription : cov_) {
        subscription.active = false;
    }
}

size_t Server::activeCovSubscriptions() const
{
    size_t count = 0;
    for (const auto &subscription : cov_) {
        if (subscription.active) {
            ++count;
        }
    }
    return count;
}

void Server::run()
{
    const int available = udp_.parsePacket();
    if (available > 0) {
        ++rxPackets_;
        const IPAddress remoteIp = udp_.remoteIP();
        const uint16_t remotePort = udp_.remotePort();
        const int length = udp_.read(rx_, sizeof(rx_));
        if (length > 0) {
            handleDatagram(rx_, static_cast<size_t>(length), remoteIp,
                           remotePort);
        }
    }

    serviceCovSubscriptions();

    /* A few unsolicited I-Am frames after boot help clients that start
     * before the device finished connecting to Wi-Fi. */
    if (startupMs_ != 0) {
        const uint32_t now = millis();
        if ((now - startupMs_) < 4000 &&
            (now - lastStartupIAmMs_) >= 900) {
            lastStartupIAmMs_ = now;
            sendIAmFrame(IPAddress(255, 255, 255, 255), port_, true);
        }
    }
}

void Server::sendIAm()
{
    sendIAmFrame(IPAddress(255, 255, 255, 255), port_, true);
}

/* ------------------------------------------------------------------------- */
/* Datagram / NPDU / APDU dispatch                                           */
/* ------------------------------------------------------------------------- */
void Server::handleDatagram(const uint8_t *data,
                            size_t length,
                            IPAddress remoteIp,
                            uint16_t remotePort)
{
    if (length < 6 || length > sizeof(rx_) || data[0] != 0x81) {
        ++droppedPackets_;
        return;
    }
    const uint8_t function = data[1];
    if (function != 0x0A && function != 0x0B) {
        /* Only Original-Unicast-NPDU and Original-Broadcast-NPDU. */
        return;
    }
    const uint16_t bvlcLength =
        static_cast<uint16_t>((data[2] << 8) | data[3]);
    if (bvlcLength < 6 || bvlcLength != length ||
        bvlcLength > kMaxApdu + 6) {
        ++droppedPackets_;
        return;
    }
    handleNpdu(data + 4, bvlcLength - 4, remoteIp, remotePort);
}

void Server::handleNpdu(const uint8_t *data,
                        size_t length,
                        IPAddress remoteIp,
                        uint16_t remotePort)
{
    if (length < 2 || data[0] != 0x01) {
        return;
    }
    const uint8_t control = data[1];
    if (control & 0x80) {
        /* Network layer message (routing), not handled by this device. */
        return;
    }
    if (control & (0x20 | 0x08)) {
        /* Destination or source specifier present: routed NPDU. This
         * device is a plain BACnet/IP endpoint and does not route. */
        return;
    }
    handleApdu(data + 2, length - 2, remoteIp, remotePort);
}

void Server::handleApdu(const uint8_t *data,
                        size_t length,
                        IPAddress remoteIp,
                        uint16_t remotePort)
{
    if (length < 1) {
        return;
    }
    const uint8_t pduType = static_cast<uint8_t>(data[0] & 0xF0);

    if (pduType == kPduUnconfirmedRequest) {
        if (length >= 2 && data[1] == kServiceWhoIs) {
            handleWhoIs(data + 2, length - 2, remoteIp, remotePort);
        }
        return;
    }

    if (pduType != kPduConfirmedRequest || length < 4) {
        return;
    }

    const bool segmented = (data[0] & 0x08) != 0;
    const uint8_t invokeId = data[2];
    const uint8_t service = data[3];

    if (segmented) {
        sendAbort(invokeId, true, kAbortSegmentationNotSupported, remoteIp,
                  remotePort);
        return;
    }

    if (pduType == kPduConfirmedRequest) {
        logFrame("RX", data, length);
    }

    switch (service) {
        case kServiceSubscribeCov:
            handleSubscribeCov(data + 4, length - 4, invokeId, remoteIp,
                               remotePort);
            break;
        case kServiceReadProperty:
            handleReadProperty(data + 4, length - 4, invokeId, remoteIp,
                               remotePort);
            break;
        case kServiceReadPropertyMultiple:
            handleReadPropertyMultiple(data + 4, length - 4, invokeId,
                                       remoteIp, remotePort);
            break;
        case kServiceWriteProperty:
            handleWriteProperty(data + 4, length - 4, invokeId, remoteIp,
                                remotePort);
            break;
        default:
            sendReject(invokeId, kRejectUnrecognizedService, remoteIp,
                       remotePort);
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Who-Is / I-Am                                                             */
/* ------------------------------------------------------------------------- */
void Server::handleWhoIs(const uint8_t *data,
                         size_t length,
                         IPAddress remoteIp,
                         uint16_t remotePort)
{
    int32_t lowLimit = -1;
    int32_t highLimit = -1;

    if (length > 0) {
        ByteReader reader(data, length);
        uint32_t low = 0;
        uint32_t high = 0;
        if (!readContextUnsigned(reader, 0, low) ||
            !readContextUnsigned(reader, 1, high) || reader.remaining() != 0) {
            return;
        }
        if (high < low) {
            return;
        }
        lowLimit = static_cast<int32_t>(low);
        highLimit = static_cast<int32_t>(high);
    }

    const uint32_t instance = settings::get().deviceInstance;
    if (lowLimit >= 0 &&
        (instance < static_cast<uint32_t>(lowLimit) ||
         instance > static_cast<uint32_t>(highLimit))) {
        return;
    }

    /* Unicast reply so a client using an ephemeral source port also sees
     * the device, plus a broadcast for normal BACnet/IP discovery. */
    sendIAmFrame(remoteIp, remotePort, false);
    sendIAmFrame(IPAddress(255, 255, 255, 255), port_, true);
    BACNET_LOG("[BACnet] Who-Is -> I-Am (device %u)\n",
               static_cast<unsigned>(instance));
}

/* ------------------------------------------------------------------------- */
/* Change-of-value subscriptions (SubscribeCOV, service 5)                    */
/* ------------------------------------------------------------------------- */
CovSubscription *Server::findCovSubscription(uint32_t processId,
                                             uint16_t objectType,
                                             uint32_t objectInstance)
{
    for (size_t i = 0; i < kMaxCovSubscriptions; ++i) {
        CovSubscription &sub = cov_[i];
        if (sub.active && sub.processId == processId &&
            sub.objectType == objectType &&
            sub.objectInstance == objectInstance) {
            return &sub;
        }
    }
    return nullptr;
}

CovSubscription *Server::reserveCovSubscription()
{
    for (size_t i = 0; i < kMaxCovSubscriptions; ++i) {
        if (!cov_[i].active) {
            return &cov_[i];
        }
    }
    /* Table full: recycle the subscription that expires soonest. Slots
     * without a lifetime (0) count as infinite and are taken last. */
    CovSubscription *candidate = nullptr;
    uint32_t bestRemaining = 0xFFFFFFFFu;
    const uint32_t now = millis();
    for (size_t i = 0; i < kMaxCovSubscriptions; ++i) {
        CovSubscription &sub = cov_[i];
        const uint32_t remaining =
            sub.expiresAtMs == 0
                ? 0xFFFFFFFFu
                : (sub.expiresAtMs > now ? sub.expiresAtMs - now : 0);
        if (candidate == nullptr || remaining < bestRemaining) {
            candidate = &sub;
            bestRemaining = remaining;
        }
    }
    return candidate;
}

void Server::handleSubscribeCov(const uint8_t *data,
                                size_t length,
                                uint8_t invokeId,
                                IPAddress remoteIp,
                                uint16_t remotePort)
{
    ByteReader reader(data, length);
    uint32_t processId = 0;
    uint16_t objectType = 0;
    uint32_t instance = 0;
    bool confirmed = false;
    uint32_t lifetime = 0;
    bool hasLifetime = false;

    if (!readContextUnsigned(reader, 0, processId) ||
        !readContextObjectId(reader, 1, objectType, instance)) {
        sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
        return;
    }

    /* Optional [2] issueConfirmedNotifications and [3] lifetime. */
    uint8_t tag = 0;
    bool context = false;
    bool constructed = false;
    bool opening = false;
    bool closing = false;
    uint32_t tagLength = 0;
    if (reader.remaining() > 0 &&
        reader.peekTag(tag, context, constructed, opening, closing,
                       tagLength) &&
        context && !constructed && !opening && !closing && tag == 2) {
        if (!readContextBoolean(reader, 2, confirmed)) {
            sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
            return;
        }
    }
    if (reader.remaining() > 0 &&
        reader.peekTag(tag, context, constructed, opening, closing,
                       tagLength) &&
        context && !constructed && !opening && !closing && tag == 3) {
        if (!readContextUnsigned(reader, 3, lifetime)) {
            sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
            return;
        }
        hasLifetime = true;
    }
    if (reader.remaining() != 0) {
        sendReject(invokeId, kRejectTooManyArguments, remoteIp, remotePort);
        return;
    }

    if (!findObject(objectType, instance)) {
        sendError(invokeId, kServiceSubscribeCov, kErrorClassObject,
                  kErrorCodeUnknownObject, remoteIp, remotePort);
        return;
    }

    /* Lifetime 0 cancels an existing subscription. */
    if (hasLifetime && lifetime == 0) {
        CovSubscription *existing =
            findCovSubscription(processId, objectType, instance);
        if (existing) {
            existing->active = false;
        }
        sendSimpleAck(invokeId, kServiceSubscribeCov, remoteIp, remotePort);
        return;
    }

    CovSubscription *sub =
        findCovSubscription(processId, objectType, instance);
    if (!sub) {
        sub = reserveCovSubscription();
        if (!sub) {
            sendError(invokeId, kServiceSubscribeCov, kErrorClassResources,
                      kErrorCodeOther, remoteIp, remotePort);
            return;
        }
        sub->processId = processId;
        sub->objectType = objectType;
        sub->objectInstance = instance;
        sub->hasLastValue = false;
        sub->lastNotifyMs = 0;
    }
    sub->active = true;
    sub->address = remoteIp;
    sub->port = remotePort;
    sub->confirmed = confirmed;
    sub->expiresAtMs = hasLifetime ? millis() + lifetime * 1000u : 0;

    sendSimpleAck(invokeId, kServiceSubscribeCov, remoteIp, remotePort);
    /* Initial notification: the subscriber immediately receives the
     * current value, then further notifications whenever it changes. */
    sendCovNotification(*sub);
}

void Server::sendCovNotification(CovSubscription &subscription)
{
    const Object *object =
        findObject(subscription.objectType, subscription.objectInstance);
    if (!object) {
        subscription.active = false;
        return;
    }

    ByteWriter writer(apdu_, sizeof(apdu_));
    if (subscription.confirmed) {
        invokeId_ = static_cast<uint8_t>((invokeId_ % 255) + 1);
        writer.u8(kPduConfirmedRequest);
        writer.u8(0x05); /* max segments 0, max APDU 1476 */
        writer.u8(invokeId_);
        writer.u8(kServiceConfirmedCovNotification);
    } else {
        writer.u8(kPduUnconfirmedRequest);
        writer.u8(kServiceUnconfirmedCovNotification);
    }
    writer.ctxUnsigned(0, subscription.processId);
    writer.ctxObjectId(1, kObjectDevice, settings::get().deviceInstance);
    writer.ctxObjectId(2, subscription.objectType,
                       subscription.objectInstance);
    const uint32_t now = millis();
    const uint32_t remaining =
        subscription.expiresAtMs == 0
            ? 0
            : (subscription.expiresAtMs > now
                   ? (subscription.expiresAtMs - now) / 1000u
                   : 0);
    writer.ctxUnsigned(3, remaining);
    writer.openingTag(4);
    /* present-value */
    writer.ctxEnumerated(0, kPropPresentValue);
    writer.openingTag(2);
    if (object->isAnalog()) {
        writer.appReal(object->presentValue);
    } else {
        writer.appEnumerated(object->binaryPresentValue);
    }
    writer.closingTag(2);
    /* status-flags */
    writer.ctxEnumerated(0, kPropStatusFlags);
    writer.openingTag(2);
    uint8_t flags = object->outOfService ? 0x10 : 0x00;
    writer.appBitString(4, &flags);
    writer.closingTag(2);
    writer.closingTag(4);
    if (writer.overflow()) {
        return;
    }
    sendApdu(subscription.address, subscription.port, writer.length(),
             false);

    subscription.lastRealValue = object->presentValue;
    subscription.lastBinaryValue = object->binaryPresentValue;
    subscription.hasLastValue = true;
    subscription.lastNotifyMs = millis();
}

void Server::serviceCovSubscriptions()
{
    const uint32_t now = millis();
    for (size_t i = 0; i < kMaxCovSubscriptions; ++i) {
        CovSubscription &sub = cov_[i];
        if (!sub.active) {
            continue;
        }
        if (sub.expiresAtMs != 0 &&
            static_cast<int32_t>(now - sub.expiresAtMs) >= 0) {
            sub.active = false;
            continue;
        }
        const Object *object =
            findObject(sub.objectType, sub.objectInstance);
        if (!object) {
            sub.active = false;
            continue;
        }
        bool changed = false;
        if (object->isAnalog()) {
            changed = !sub.hasLastValue ||
                      fabsf(object->presentValue - sub.lastRealValue) >=
                          object->covIncrement;
        } else {
            changed = !sub.hasLastValue ||
                      object->binaryPresentValue != sub.lastBinaryValue;
        }
        if (!changed) {
            continue;
        }
        /* Rate limit: at most one notification per subscriber per second. */
        if (sub.lastNotifyMs != 0 && (now - sub.lastNotifyMs) < 1000u) {
            continue;
        }
        sendCovNotification(sub);
    }
}

void Server::sendIAmFrame(IPAddress destination,
                          uint16_t destinationPort,
                          bool broadcast)
{
    ByteWriter writer(apdu_, sizeof(apdu_));
    writer.u8(kPduUnconfirmedRequest);
    writer.u8(kServiceIAm);
    writer.appObjectId(kObjectDevice, settings::get().deviceInstance);
    writer.appUnsigned(kMaxApdu);
    /* 3 = NO_SEGMENTATION */
    writer.appEnumerated(3);
    writer.appUnsigned(settings::get().vendorId);
    if (writer.overflow()) {
        return;
    }
    sendApdu(destination, destinationPort, writer.length(), broadcast);
}

/* ------------------------------------------------------------------------- */
/* ReadProperty                                                              */
/* ------------------------------------------------------------------------- */
void Server::handleReadProperty(const uint8_t *data,
                                size_t length,
                                uint8_t invokeId,
                                IPAddress remoteIp,
                                uint16_t remotePort)
{
    ByteReader reader(data, length);
    uint16_t objectType = 0;
    uint32_t instance = 0;
    uint32_t property = 0;
    uint32_t arrayIndex = kArrayAll;

    if (!readContextObjectId(reader, 0, objectType, instance) ||
        !readContextUnsigned(reader, 1, property)) {
        sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
        return;
    }
    if (reader.remaining() > 0 &&
        !readContextUnsigned(reader, 2, arrayIndex)) {
        sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
        return;
    }
    if (reader.remaining() != 0) {
        sendReject(invokeId, kRejectTooManyArguments, remoteIp, remotePort);
        return;
    }

    Object *object = findObject(objectType, instance);
    if (!object) {
        sendError(invokeId, kServiceReadProperty, kErrorClassObject,
                  kErrorCodeUnknownObject, remoteIp, remotePort);
        return;
    }

    ByteWriter writer(apdu_, sizeof(apdu_));
    writer.u8(kPduComplexAck);
    writer.u8(invokeId);
    writer.u8(kServiceReadProperty);
    writer.ctxObjectId(0, objectType, instance);
    writer.ctxEnumerated(1, property);
    if (arrayIndex != kArrayAll) {
        writer.ctxUnsigned(2, arrayIndex);
    }
    writer.openingTag(3);

    const ReadStatus status =
        readProperty(object, property, arrayIndex, writer);
    if (status != ReadStatus::Ok) {
        sendReadError(status, invokeId, kServiceReadProperty, remoteIp,
                      remotePort);
        return;
    }
    writer.closingTag(3);

    if (writer.overflow() || writer.length() > kMaxApdu) {
        sendAbort(invokeId, true, kAbortBufferOverflow, remoteIp, remotePort);
        return;
    }
    sendApdu(remoteIp, remotePort, writer.length(), false);
}

/* ------------------------------------------------------------------------- */
/* WriteProperty                                                             */
/* ------------------------------------------------------------------------- */
void Server::handleWriteProperty(const uint8_t *data,
                                 size_t length,
                                 uint8_t invokeId,
                                 IPAddress remoteIp,
                                 uint16_t remotePort)
{
    ByteReader reader(data, length);
    uint16_t objectType = 0;
    uint32_t instance = 0;
    uint32_t property = 0;
    bool hasArrayIndex = false;
    uint32_t arrayIndex = 0;

    if (!readContextObjectId(reader, 0, objectType, instance) ||
        !readContextUnsigned(reader, 1, property)) {
        sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
        return;
    }

    if (reader.remaining() > 0) {
        uint8_t tag = 0;
        bool context = false;
        bool constructed = false;
        bool opening = false;
        bool closing = false;
        uint32_t tagLength = 0;
        if (!reader.peekTag(tag, context, constructed, opening, closing,
                            tagLength)) {
            sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
            return;
        }
        if (context && !constructed && !opening && !closing && tag == 2) {
            if (!readContextUnsigned(reader, 2, arrayIndex)) {
                sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
                return;
            }
            hasArrayIndex = true;
        }
    }

    if (!expectOpeningTag(reader, 3)) {
        sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
        return;
    }

    Value value;
    if (!decodeApplicationValue(reader, value)) {
        sendReject(invokeId, kRejectInvalidParameterDataType, remoteIp,
                   remotePort);
        return;
    }

    if (!expectClosingTag(reader, 3)) {
        sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
        return;
    }

    uint8_t priority = 0;
    if (reader.remaining() > 0) {
        uint8_t tag = 0;
        bool context = false;
        bool constructed = false;
        bool opening = false;
        bool closing = false;
        uint32_t tagLength = 0;
        if (!reader.peekTag(tag, context, constructed, opening, closing,
                            tagLength)) {
            sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
            return;
        }
        if (context && !constructed && !opening && !closing && tag == 4) {
            uint32_t priorityValue = 0;
            if (!readContextUnsigned(reader, 4, priorityValue)) {
                sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
                return;
            }
            if (priorityValue < 1 || priorityValue > kMaxPriority) {
                sendError(invokeId, kServiceWriteProperty,
                          kErrorClassProperty, kErrorCodeValueOutOfRange,
                          remoteIp, remotePort);
                return;
            }
            priority = static_cast<uint8_t>(priorityValue);
        }
    }

    if (reader.remaining() != 0) {
        sendReject(invokeId, kRejectTooManyArguments, remoteIp, remotePort);
        return;
    }

    Object *object = findObject(objectType, instance);
    if (!object) {
        sendError(invokeId, kServiceWriteProperty, kErrorClassObject,
                  kErrorCodeUnknownObject, remoteIp, remotePort);
        return;
    }

    uint16_t errorClass = 0;
    uint32_t errorCode = 0;
    if (!writeProperty(object, property, hasArrayIndex, arrayIndex, value,
                       priority, errorClass, errorCode)) {
        sendError(invokeId, kServiceWriteProperty, errorClass, errorCode,
                  remoteIp, remotePort);
        return;
    }

    sendSimpleAck(invokeId, kServiceWriteProperty, remoteIp, remotePort);
}

/* ------------------------------------------------------------------------- */
/* ReadPropertyMultiple                                                      */
/* ------------------------------------------------------------------------- */
void Server::handleReadPropertyMultiple(const uint8_t *data,
                                        size_t length,
                                        uint8_t invokeId,
                                        IPAddress remoteIp,
                                        uint16_t remotePort)
{
    ByteReader reader(data, length);
    ByteWriter writer(apdu_, sizeof(apdu_));
    writer.u8(kPduComplexAck);
    writer.u8(invokeId);
    writer.u8(kServiceReadPropertyMultiple);

    bool sawObject = false;

    while (reader.remaining() > 0) {
        uint16_t objectType = 0;
        uint32_t instance = 0;
        if (!readContextObjectId(reader, 0, objectType, instance)) {
            sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
            return;
        }
        if (!expectOpeningTag(reader, 1)) {
            sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
            return;
        }

        Object *object = findObject(objectType, instance);
        writer.ctxObjectId(0, objectType, instance);
        writer.openingTag(1);
        sawObject = true;

        for (;;) {
            uint8_t tag = 0;
            bool context = false;
            bool constructed = false;
            bool opening = false;
            bool closing = false;
            uint32_t tagLength = 0;
            if (!reader.readTag(tag, context, constructed, opening, closing,
                                tagLength)) {
                sendReject(invokeId, kRejectInvalidTag, remoteIp,
                           remotePort);
                return;
            }
            if (context && constructed && closing && !opening && tag == 1) {
                break;
            }
            if (!context || constructed || opening || closing || tag != 0) {
                sendReject(invokeId, kRejectInvalidTag, remoteIp, remotePort);
                return;
            }
            if (tagLength < 1 || tagLength > 4) {
                sendReject(invokeId, kRejectInvalidTag, remoteIp,
                           remotePort);
                return;
            }

            uint32_t property = 0;
            for (uint32_t i = 0; i < tagLength; ++i) {
                uint8_t byte = 0;
                if (!reader.readU8(byte)) {
                    sendReject(invokeId, kRejectInvalidTag, remoteIp,
                               remotePort);
                    return;
                }
                property = (property << 8) | byte;
            }

            bool hasArrayIndex = false;
            uint32_t arrayIndex = kArrayAll;
            uint32_t optionalIndex = 0;
            if (readOptionalContextTag(reader, 1, optionalIndex)) {
                hasArrayIndex = true;
                arrayIndex = optionalIndex;
            }

            /* ASHRAE 135 property identifier all(8) is valid inside
             * ReadPropertyMultiple and shall return every readable
             * property of the object; Yabe's property grid uses it. */
            if (property == kPropAll) {
                if (!object) {
                    writer.ctxEnumerated(2, property);
                    encodePropertyError(writer, kErrorClassObject,
                                        kErrorCodeUnknownObject);
                } else {
                    size_t count = 0;
                    const uint32_t *list =
                        objectPropertyList(object->type, &count);
                    for (size_t i = 0; list && i < count; ++i) {
                        writer.ctxEnumerated(2, list[i]);
                        ByteWriter scratch(valueScratch_,
                                           sizeof(valueScratch_));
                        const ReadStatus status =
                            readProperty(object, list[i], kArrayAll, scratch);
                        if (status == ReadStatus::Ok) {
                            writer.openingTag(4);
                            writer.bytes(scratch.data(), scratch.length());
                            writer.closingTag(4);
                        } else {
                            uint16_t errorClass = 0;
                            uint32_t errorCode = 0;
                            mapReadError(status, errorClass, errorCode);
                            encodePropertyError(writer, errorClass,
                                                errorCode);
                        }
                        if (writer.overflow()) {
                            sendAbort(invokeId, true, kAbortBufferOverflow,
                                      remoteIp, remotePort);
                            return;
                        }
                    }
                }
                continue;
            }

            writer.ctxEnumerated(2, property);
            if (hasArrayIndex) {
                writer.ctxUnsigned(3, arrayIndex);
            }

            if (!object) {
                encodePropertyError(writer, kErrorClassObject,
                                    kErrorCodeUnknownObject);
            } else {
                ByteWriter scratch(valueScratch_, sizeof(valueScratch_));
                const ReadStatus status = readProperty(
                    object, property, hasArrayIndex ? arrayIndex : kArrayAll,
                    scratch);
                if (status == ReadStatus::Ok) {
                    writer.openingTag(4);
                    writer.bytes(scratch.data(), scratch.length());
                    writer.closingTag(4);
                } else if (status == ReadStatus::BufferOverflow) {
                    encodePropertyError(writer, kErrorClassResources,
                                        kErrorCodeNoSpaceToWriteProperty);
                } else {
                    uint16_t errorClass = 0;
                    uint32_t errorCode = 0;
                    mapReadError(status, errorClass, errorCode);
                    encodePropertyError(writer, errorClass, errorCode);
                }
            }

            if (writer.overflow()) {
                sendAbort(invokeId, true, kAbortBufferOverflow, remoteIp,
                          remotePort);
                return;
            }
        }

        writer.closingTag(1);
        if (writer.overflow()) {
            sendAbort(invokeId, true, kAbortBufferOverflow, remoteIp,
                      remotePort);
            return;
        }
    }

    if (!sawObject) {
        sendReject(invokeId, kRejectMissingRequiredParameter, remoteIp,
                   remotePort);
        return;
    }
    if (writer.overflow() || writer.length() > kMaxApdu) {
        sendAbort(invokeId, true, kAbortBufferOverflow, remoteIp, remotePort);
        return;
    }
    sendApdu(remoteIp, remotePort, writer.length(), false);
}

/* ------------------------------------------------------------------------- */
/* Response helpers                                                          */
/* ------------------------------------------------------------------------- */
void Server::sendApdu(IPAddress destination,
                      uint16_t destinationPort,
                      size_t apduLength,
                      bool broadcast)
{
    const size_t totalLength = apduLength + 6;
    if (totalLength > sizeof(tx_)) {
        return;
    }
    tx_[0] = 0x81;
    tx_[1] = broadcast ? 0x0B : 0x0A;
    tx_[2] = static_cast<uint8_t>((totalLength >> 8) & 0xFF);
    tx_[3] = static_cast<uint8_t>(totalLength & 0xFF);
    tx_[4] = 0x01; /* NPDU version */
    tx_[5] = 0x00; /* control: no network layer information */
    memcpy(tx_ + 6, apdu_, apduLength);

    logFrame(broadcast ? "TXb" : "TX", tx_ + 6, apduLength);

    if (!udp_.beginPacket(destination, destinationPort) ||
        udp_.write(tx_, totalLength) != totalLength || !udp_.endPacket()) {
        ++txErrors_;
    }
}

void Server::sendSimpleAck(uint8_t invokeId,
                           uint8_t service,
                           IPAddress remoteIp,
                           uint16_t remotePort)
{
    ByteWriter writer(apdu_, sizeof(apdu_));
    writer.u8(kPduSimpleAck);
    writer.u8(invokeId);
    writer.u8(service);
    if (!writer.overflow()) {
        sendApdu(remoteIp, remotePort, writer.length(), false);
    }
}

void Server::sendError(uint8_t invokeId,
                       uint8_t service,
                       uint16_t errorClass,
                       uint32_t errorCode,
                       IPAddress remoteIp,
                       uint16_t remotePort)
{
    ByteWriter writer(apdu_, sizeof(apdu_));
    writer.u8(kPduError);
    writer.u8(invokeId);
    writer.u8(service);
    writer.appEnumerated(errorClass);
    writer.appEnumerated(errorCode);
    if (!writer.overflow()) {
        sendApdu(remoteIp, remotePort, writer.length(), false);
    }
}

void Server::sendReject(uint8_t invokeId,
                        uint8_t reason,
                        IPAddress remoteIp,
                        uint16_t remotePort)
{
    /* Clause 20.1.8: type, invoke ID, reason as a raw octet. */
    ByteWriter writer(apdu_, sizeof(apdu_));
    writer.u8(kPduReject);
    writer.u8(invokeId);
    writer.u8(reason);
    if (!writer.overflow()) {
        sendApdu(remoteIp, remotePort, writer.length(), false);
    }
}

void Server::sendAbort(uint8_t invokeId,
                       bool server,
                       uint8_t reason,
                       IPAddress remoteIp,
                       uint16_t remotePort)
{
    /* Clause 20.1.9: type (bit 0 = sent-by-server), invoke ID, reason as
     * a raw octet. */
    ByteWriter writer(apdu_, sizeof(apdu_));
    writer.u8(kPduAbort | (server ? 0x01 : 0x00));
    writer.u8(invokeId);
    writer.u8(reason);
    if (!writer.overflow()) {
        sendApdu(remoteIp, remotePort, writer.length(), false);
    }
}

void Server::sendReadError(ReadStatus status,
                           uint8_t invokeId,
                           uint8_t service,
                           IPAddress remoteIp,
                           uint16_t remotePort)
{
    if (status == ReadStatus::BufferOverflow) {
        sendAbort(invokeId, true, kAbortBufferOverflow, remoteIp, remotePort);
        return;
    }
    uint16_t errorClass = kErrorClassProperty;
    uint32_t errorCode = kErrorCodeUnknownProperty;
    mapReadError(status, errorClass, errorCode);
    sendError(invokeId, service, errorClass, errorCode, remoteIp, remotePort);
}

} // namespace bacnet
