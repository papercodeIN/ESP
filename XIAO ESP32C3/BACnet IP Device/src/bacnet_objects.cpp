/**
 * @file bacnet_objects.cpp
 * @brief Static BACnet object database and point behaviour.
 */
#include "bacnet_objects.h"

#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

#include "bacnet_config.h"
#include "bacnet_settings.h"

namespace bacnet {
namespace {

/* ------------------------------------------------------------------------- */
/* Object table                                                              */
/* ------------------------------------------------------------------------- */
Object g_objects[] = {
    /* 0 */ Object{kObjectDevice, 0},
    /* 1 */ Object{kObjectAnalogInput, 0},
    /* 2 */ Object{kObjectAnalogInput, 1},
    /* 3 */ Object{kObjectAnalogInput, 2},
    /* 4 */ Object{kObjectAnalogInput, 3},
    /* 5 */ Object{kObjectAnalogValue, 0},
    /* 6 */ Object{kObjectAnalogValue, 1},
    /* 7 */ Object{kObjectBinaryInput, 0},
    /* 8 */ Object{kObjectBinaryInput, 1},
    /* 9 */ Object{kObjectBinaryOutput, 0},
    /* 10 */ Object{kObjectBinaryValue, 0},
};

constexpr size_t kObjectCount = sizeof(g_objects) / sizeof(g_objects[0]);

/* ------------------------------------------------------------------------- */
/* Property lists                                                            */
/* ------------------------------------------------------------------------- */
const uint32_t kDeviceProperties[] = {
    kPropObjectIdentifier,
    kPropObjectName,
    kPropObjectType,
    kPropDescription,
    kPropLocation,
    kPropModelName,
    kPropFirmwareRevision,
    kPropApplicationSoftwareVersion,
    kPropVendorName,
    kPropVendorIdentifier,
    kPropProtocolVersion,
    kPropProtocolRevision,
    kPropProtocolObjectTypesSupported,
    kPropProtocolServicesSupported,
    kPropObjectList,
    kPropMaxApduLengthAccepted,
    kPropSegmentationSupported,
    kPropApduTimeout,
    kPropNumberOfApduRetries,
    kPropDeviceAddressBinding,
    kPropDatabaseRevision,
    kPropSystemStatus,
    kPropPropertyList,
};

const uint32_t kAnalogInputProperties[] = {
    kPropObjectIdentifier,
    kPropObjectName,
    kPropObjectType,
    kPropDescription,
    kPropPresentValue,
    kPropStatusFlags,
    kPropEventState,
    kPropReliability,
    kPropOutOfService,
    kPropUnits,
    kPropCovIncrement,
    kPropMinPresValue,
    kPropMaxPresValue,
    kPropResolution,
    kPropTimeDelay,
    kPropEventEnable,
    kPropAckedTransitions,
    kPropNotifyType,
    kPropPropertyList,
};

const uint32_t kAnalogValueProperties[] = {
    kPropObjectIdentifier,
    kPropObjectName,
    kPropObjectType,
    kPropDescription,
    kPropPresentValue,
    kPropStatusFlags,
    kPropEventState,
    kPropReliability,
    kPropOutOfService,
    kPropUnits,
    kPropCovIncrement,
    kPropMinPresValue,
    kPropMaxPresValue,
    kPropResolution,
    kPropPriorityArray,
    kPropRelinquishDefault,
    kPropTimeDelay,
    kPropEventEnable,
    kPropAckedTransitions,
    kPropNotifyType,
    kPropPropertyList,
};

const uint32_t kBinaryInputProperties[] = {
    kPropObjectIdentifier,
    kPropObjectName,
    kPropObjectType,
    kPropDescription,
    kPropPresentValue,
    kPropStatusFlags,
    kPropEventState,
    kPropReliability,
    kPropOutOfService,
    kPropPolarity,
    kPropActiveText,
    kPropInactiveText,
    kPropTimeDelay,
    kPropEventEnable,
    kPropAckedTransitions,
    kPropNotifyType,
    kPropPropertyList,
};

const uint32_t kBinaryOutputProperties[] = {
    kPropObjectIdentifier,
    kPropObjectName,
    kPropObjectType,
    kPropDescription,
    kPropPresentValue,
    kPropStatusFlags,
    kPropEventState,
    kPropReliability,
    kPropOutOfService,
    kPropPolarity,
    kPropActiveText,
    kPropInactiveText,
    kPropPriorityArray,
    kPropRelinquishDefault,
    kPropTimeDelay,
    kPropEventEnable,
    kPropAckedTransitions,
    kPropNotifyType,
    kPropPropertyList,
};

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */
void copyText(char *destination, size_t capacity, const char *source)
{
    if (!source) {
        source = "";
    }
    if (capacity == 0) {
        return;
    }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

bool listContains(const uint32_t *list, size_t count, uint32_t value)
{
    for (size_t i = 0; i < count; ++i) {
        if (list[i] == value) {
            return true;
        }
    }
    return false;
}

bool propertiesFor(uint16_t type,
                   const uint32_t **list,
                   size_t *count)
{
    switch (type) {
        case kObjectDevice:
            *list = kDeviceProperties;
            *count = sizeof(kDeviceProperties) / sizeof(kDeviceProperties[0]);
            return true;
        case kObjectAnalogInput:
            *list = kAnalogInputProperties;
            *count = sizeof(kAnalogInputProperties) /
                     sizeof(kAnalogInputProperties[0]);
            return true;
        case kObjectAnalogValue:
            *list = kAnalogValueProperties;
            *count = sizeof(kAnalogValueProperties) /
                     sizeof(kAnalogValueProperties[0]);
            return true;
        case kObjectBinaryInput:
            *list = kBinaryInputProperties;
            *count = sizeof(kBinaryInputProperties) /
                     sizeof(kBinaryInputProperties[0]);
            return true;
        case kObjectBinaryOutput:
            *list = kBinaryOutputProperties;
            *count = sizeof(kBinaryOutputProperties) /
                     sizeof(kBinaryOutputProperties[0]);
            return true;
        case kObjectBinaryValue:
            *list = kBinaryOutputProperties;
            *count = sizeof(kBinaryOutputProperties) /
                     sizeof(kBinaryOutputProperties[0]);
            return true;
        default:
            return false;
    }
}

bool isKnownProperty(uint16_t type, uint32_t property)
{
    const uint32_t *list = nullptr;
    size_t count = 0;
    if (!propertiesFor(type, &list, &count)) {
        return false;
    }
    return listContains(list, count, property);
}

ReadStatus okOrOverflow(bool ok)
{
    return ok ? ReadStatus::Ok : ReadStatus::BufferOverflow;
}

ReadStatus requireNoIndex(uint32_t arrayIndex)
{
    return (arrayIndex == kArrayAll) ? ReadStatus::Ok
                                     : ReadStatus::PropertyIsNotArray;
}

ReadStatus encodeEnumeratedArray(const uint32_t *items,
                                 size_t count,
                                 uint32_t arrayIndex,
                                 ByteWriter &writer)
{
    /* BACnet arrays: index 0 is the element count, a missing index encodes
     * the whole array as a plain sequence of elements. */
    if (arrayIndex == 0) {
        return okOrOverflow(writer.appUnsigned(count));
    }
    if (arrayIndex == kArrayAll) {
        for (size_t i = 0; i < count; ++i) {
            if (!writer.appEnumerated(items[i])) {
                return ReadStatus::BufferOverflow;
            }
        }
        return ReadStatus::Ok;
    }
    if (arrayIndex > count) {
        return ReadStatus::InvalidArrayIndex;
    }
    return okOrOverflow(writer.appEnumerated(items[arrayIndex - 1]));
}

ReadStatus encodeObjectIdArray(uint32_t arrayIndex, ByteWriter &writer)
{
    if (arrayIndex == 0) {
        return okOrOverflow(writer.appUnsigned(kObjectCount));
    }
    if (arrayIndex == kArrayAll) {
        for (size_t i = 0; i < kObjectCount; ++i) {
            if (!writer.appObjectId(g_objects[i].type,
                                    g_objects[i].instance)) {
                return ReadStatus::BufferOverflow;
            }
        }
        return ReadStatus::Ok;
    }
    if (arrayIndex > kObjectCount) {
        return ReadStatus::InvalidArrayIndex;
    }
    const Object &object = g_objects[arrayIndex - 1];
    return okOrOverflow(
        writer.appObjectId(object.type, object.instance));
}

ReadStatus encodeStatusFlags(const Object *object, ByteWriter &writer)
{
    /* status-flags: in-alarm, fault, overridden, out-of-service */
    uint8_t flags = object->outOfService ? 0x10 : 0x00;
    return okOrOverflow(writer.appBitString(4, &flags));
}

ReadStatus encodeEventsEnabled(ByteWriter &writer)
{
    uint8_t flags = 0;
    return okOrOverflow(writer.appBitString(3, &flags));
}

ReadStatus encodePriorityElement(const Object *object,
                                 size_t priorityIndex,
                                 ByteWriter &writer)
{
    if (object->type == kObjectAnalogValue) {
        if (!object->priorityActive[priorityIndex]) {
            return okOrOverflow(writer.appNull());
        }
        return okOrOverflow(writer.appReal(object->priority[priorityIndex]));
    }
    if (!object->binaryPriorityActive[priorityIndex]) {
        return okOrOverflow(writer.appNull());
    }
    return okOrOverflow(
        writer.appEnumerated(object->binaryPriority[priorityIndex]));
}

ReadStatus encodePriorityArray(const Object *object,
                               uint32_t arrayIndex,
                               ByteWriter &writer)
{
    if (arrayIndex == 0) {
        return okOrOverflow(writer.appUnsigned(kMaxPriority));
    }
    if (arrayIndex == kArrayAll) {
        for (size_t i = 0; i < kMaxPriority; ++i) {
            const ReadStatus status = encodePriorityElement(object, i, writer);
            if (status != ReadStatus::Ok) {
                return status;
            }
        }
        return ReadStatus::Ok;
    }
    if (arrayIndex > kMaxPriority) {
        return ReadStatus::InvalidArrayIndex;
    }
    return encodePriorityElement(object, arrayIndex - 1, writer);
}

ReadStatus encodePropertyList(const Object *object,
                              uint32_t arrayIndex,
                              ByteWriter &writer)
{
    const uint32_t *list = nullptr;
    size_t count = 0;
    if (!propertiesFor(object->type, &list, &count)) {
        return ReadStatus::UnknownProperty;
    }
    return encodeEnumeratedArray(list, count, arrayIndex, writer);
}

/* ------------------------------------------------------------------------- */
/* Commandable property resolution                                           */
/* ------------------------------------------------------------------------- */
void resolveAnalog(Object *object)
{
    for (size_t i = 0; i < kMaxPriority; ++i) {
        if (object->priorityActive[i]) {
            object->presentValue = object->priority[i];
            return;
        }
    }
    object->presentValue = object->relinquishDefault;
}

void resolveBinary(Object *object)
{
    for (size_t i = 0; i < kMaxPriority; ++i) {
        if (object->binaryPriorityActive[i]) {
            object->binaryPresentValue = object->binaryPriority[i];
            return;
        }
    }
    object->binaryPresentValue = object->binaryRelinquishDefault;
}

void setAnalogPriority(Object *object, float value, uint8_t priority)
{
    /* writeProperty() normalizes the priority to 1..16 before calling. */
    object->priority[priority - 1] = value;
    object->priorityActive[priority - 1] = true;
    resolveAnalog(object);
}

void relinquishAnalog(Object *object, uint8_t priority)
{
    object->priorityActive[priority - 1] = false;
    resolveAnalog(object);
}

void setBinaryPriority(Object *object, uint8_t value, uint8_t priority)
{
    object->binaryPriority[priority - 1] = value;
    object->binaryPriorityActive[priority - 1] = true;
    resolveBinary(object);
}

void relinquishBinary(Object *object, uint8_t priority)
{
    object->binaryPriorityActive[priority - 1] = false;
    resolveBinary(object);
}

bool decodeBinaryValue(const Value &value, uint8_t &result)
{
    if (value.type == Value::Boolean) {
        result = value.boolean ? 1 : 0;
        return true;
    }
    if (value.type == Value::Enumerated || value.type == Value::Unsigned) {
        if (value.unsignedValue > 1) {
            return false;
        }
        result = static_cast<uint8_t>(value.unsignedValue);
        return true;
    }
    return false;
}

void applyBinaryOutputs()
{
#if BACNET_BO0_GPIO >= 0
    for (size_t i = 0; i < kObjectCount; ++i) {
        Object &object = g_objects[i];
        if (object.type != kObjectBinaryOutput) {
            continue;
        }
        if (object.instance == 0) {
            const bool active = object.binaryPresentValue != 0;
            digitalWrite(BACNET_BO0_GPIO,
                         (active && !object.outOfService) ? HIGH : LOW);
        }
    }
#endif
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Object database API                                                       */
/* ------------------------------------------------------------------------- */
void applyDeviceIdentity()
{
    settings::Config &config = settings::get();
    Object &device = g_objects[0];
    device.instance = config.deviceInstance;
    copyText(device.name, sizeof(device.name), config.deviceName);
    copyText(device.description, sizeof(device.description),
             config.deviceDescription);
}

void initObjects()
{
    applyDeviceIdentity();

    Object &chipTemperature = g_objects[1];
    copyText(chipTemperature.name, sizeof(chipTemperature.name),
             "Chip Temperature");
    copyText(chipTemperature.description, sizeof(chipTemperature.description),
             "ESP32-C3 internal temperature sensor");
    chipTemperature.units = kUnitsDegreesCelsius;
    chipTemperature.minPresValue = -40.0f;
    chipTemperature.maxPresValue = 125.0f;
    chipTemperature.covIncrement = 0.2f;
    chipTemperature.presentValue = 25.0f;

    Object &wifiRssi = g_objects[2];
    copyText(wifiRssi.name, sizeof(wifiRssi.name), "Wi-Fi RSSI");
    copyText(wifiRssi.description, sizeof(wifiRssi.description),
             "Received signal strength of the Wi-Fi link");
    wifiRssi.units = kUnitsDecibels;
    wifiRssi.minPresValue = -100.0f;
    wifiRssi.maxPresValue = 0.0f;
    wifiRssi.covIncrement = 1.0f;
    wifiRssi.presentValue = -60.0f;

    Object &uptime = g_objects[3];
    copyText(uptime.name, sizeof(uptime.name), "Uptime Seconds");
    copyText(uptime.description, sizeof(uptime.description),
             "Seconds since the device booted");
    uptime.units = kUnitsSeconds;
    uptime.minPresValue = 0.0f;
    uptime.maxPresValue = 100000000.0f;
    uptime.covIncrement = 1.0f;

    Object &demoSine = g_objects[4];
    copyText(demoSine.name, sizeof(demoSine.name), "Demo Sine Wave");
    copyText(demoSine.description, sizeof(demoSine.description),
             "Generated 0..100 percent signal for client testing");
    demoSine.units = kUnitsPercent;
    demoSine.minPresValue = 0.0f;
    demoSine.maxPresValue = 100.0f;
    /* 10 % steps: with a 5 s period the wave would otherwise trigger a
     * COV notification several times per second. */
    demoSine.covIncrement = 10.0f;
    demoSine.presentValue = 50.0f;

    Object &setpoint = g_objects[5];
    copyText(setpoint.name, sizeof(setpoint.name), "Temperature Setpoint");
    copyText(setpoint.description, sizeof(setpoint.description),
             "Commandable temperature setpoint");
    setpoint.units = kUnitsDegreesCelsius;
    setpoint.minPresValue = 5.0f;
    setpoint.maxPresValue = 35.0f;
    setpoint.covIncrement = 0.2f;
    setpoint.relinquishDefault = 21.5f;
    setpoint.presentValue = setpoint.relinquishDefault;

    Object &analogOutput = g_objects[6];
    copyText(analogOutput.name, sizeof(analogOutput.name),
             "Analog Output Value");
    copyText(analogOutput.description, sizeof(analogOutput.description),
             "Commandable 0..100 percent value");
    analogOutput.units = kUnitsPercent;
    analogOutput.minPresValue = 0.0f;
    analogOutput.maxPresValue = 100.0f;
    analogOutput.covIncrement = 0.5f;
    analogOutput.relinquishDefault = 0.0f;
    analogOutput.presentValue = analogOutput.relinquishDefault;

    Object &wifiConnected = g_objects[7];
    copyText(wifiConnected.name, sizeof(wifiConnected.name), "Wi-Fi Connected");
    copyText(wifiConnected.description, sizeof(wifiConnected.description),
             "True while the Wi-Fi station link is up");

    Object &bootButton = g_objects[8];
    copyText(bootButton.name, sizeof(bootButton.name), "Boot Button");
    copyText(bootButton.description, sizeof(bootButton.description),
             "XIAO ESP32C3 BOOT button (GPIO9, active low)");

    Object &relayOutput = g_objects[9];
    copyText(relayOutput.name, sizeof(relayOutput.name), "Relay Output 1");
    copyText(relayOutput.description, sizeof(relayOutput.description),
             "Digital output on D0 / GPIO2");
    relayOutput.activeText = "On";
    relayOutput.inactiveText = "Off";

    Object &simulation = g_objects[10];
    copyText(simulation.name, sizeof(simulation.name), "Simulation Enable");
    copyText(simulation.description, sizeof(simulation.description),
             "Software-only commandable binary value");

#if BACNET_BO0_GPIO >= 0
    pinMode(BACNET_BO0_GPIO, OUTPUT);
    digitalWrite(BACNET_BO0_GPIO, LOW);
#endif
#if BACNET_BI1_GPIO >= 0
    pinMode(BACNET_BI1_GPIO, INPUT_PULLUP);
#endif

    applyBinaryOutputs();
}

size_t objectCount()
{
    return kObjectCount;
}

const Object *objectAt(size_t index)
{
    if (index >= kObjectCount) {
        return nullptr;
    }
    return &g_objects[index];
}

Object *findObject(uint16_t type, uint32_t instance)
{
    for (size_t i = 0; i < kObjectCount; ++i) {
        if (g_objects[i].type == type && g_objects[i].instance == instance) {
            return &g_objects[i];
        }
    }
    return nullptr;
}

const uint32_t *objectPropertyList(uint16_t type, size_t *count)
{
    const uint32_t *list = nullptr;
    if (propertiesFor(type, &list, count)) {
        return list;
    }
    *count = 0;
    return nullptr;
}

/* ------------------------------------------------------------------------- */
/* ReadProperty                                                              */
/* ------------------------------------------------------------------------- */
ReadStatus readProperty(const Object *object,
                        uint32_t property,
                        uint32_t arrayIndex,
                        ByteWriter &writer)
{
    if (!object) {
        return ReadStatus::UnknownObject;
    }
    if (!isKnownProperty(object->type, property)) {
        return ReadStatus::UnknownProperty;
    }

    if (object->type == kObjectDevice) {
        switch (property) {
            case kPropObjectIdentifier:
                if (requireNoIndex(arrayIndex) != ReadStatus::Ok) {
                    return ReadStatus::PropertyIsNotArray;
                }
                return okOrOverflow(writer.appObjectId(object->type,
                                                       object->instance));
            case kPropObjectName:
                return okOrOverflow(writer.appString(object->name));
            case kPropObjectType:
                return okOrOverflow(writer.appEnumerated(object->type));
            case kPropDescription:
                return okOrOverflow(writer.appString(object->description));
            case kPropLocation:
                return okOrOverflow(
                    writer.appString(settings::get().deviceLocation));
            case kPropModelName:
                return okOrOverflow(
                    writer.appString(settings::get().modelName));
            case kPropFirmwareRevision:
                return okOrOverflow(
                    writer.appString(settings::get().firmwareRevision));
            case kPropApplicationSoftwareVersion:
                return okOrOverflow(
                    writer.appString(settings::get().appSoftwareVersion));
            case kPropVendorName:
                return okOrOverflow(
                    writer.appString(settings::get().vendorName));
            case kPropVendorIdentifier:
                return okOrOverflow(
                    writer.appUnsigned(settings::get().vendorId));
            case kPropProtocolVersion:
                return okOrOverflow(writer.appUnsigned(1));
            case kPropProtocolRevision:
                return okOrOverflow(
                    writer.appUnsigned(BACNET_PROTOCOL_REVISION));
            case kPropProtocolObjectTypesSupported: {
                if (arrayIndex != kArrayAll) {
                    return ReadStatus::PropertyIsNotArray;
                }
                /* BIT STRING indexed by object type: AI, AV, BI, BO, BV,
                 * Device. */
                const uint8_t bits[2] = {0xBC, 0x80};
                return okOrOverflow(writer.appBitString(16, bits));
            }
            case kPropProtocolServicesSupported: {
                if (arrayIndex != kArrayAll) {
                    return ReadStatus::PropertyIsNotArray;
                }
                /* BIT STRING indexed by SERVICE_SUPPORTED_* (bacnet-stack
                 * bacenum.h): ConfirmedCOVNotification(1), SubscribeCOV(5),
                 * ReadProperty(12), ReadPropertyMultiple(14),
                 * WriteProperty(15), I-Am(26),
                 * UnconfirmedCOVNotification(28), Who-Is(34).
                 * bit 1 = 0x40, bit 5 = 0x04, bits 12/14/15 = 0x10/0x04/0x02,
                 * bits 26/28 = 0x20/0x08, bit 34 = 0x20. */
                const uint8_t bits[5] = {0x44, 0x16, 0x00, 0x28, 0x20};
                return okOrOverflow(writer.appBitString(35, bits));
            }
            case kPropObjectList:
                return encodeObjectIdArray(arrayIndex, writer);
            case kPropMaxApduLengthAccepted:
                return okOrOverflow(writer.appUnsigned(kMaxApdu));
            case kPropSegmentationSupported:
                /* 3 = NO_SEGMENTATION */
                return okOrOverflow(writer.appEnumerated(3));
            case kPropApduTimeout:
                return okOrOverflow(writer.appUnsigned(3000));
            case kPropNumberOfApduRetries:
                return okOrOverflow(writer.appUnsigned(3));
            case kPropDeviceAddressBinding:
                if (requireNoIndex(arrayIndex) != ReadStatus::Ok) {
                    return ReadStatus::PropertyIsNotArray;
                }
                /* No bindings: an empty list has no encoded elements. */
                return ReadStatus::Ok;
            case kPropDatabaseRevision:
                return okOrOverflow(writer.appUnsigned(1));
            case kPropSystemStatus:
                /* 0 = operational */
                return okOrOverflow(writer.appEnumerated(0));
            case kPropPropertyList:
                return encodePropertyList(object, arrayIndex, writer);
            default:
                return ReadStatus::UnknownProperty;
        }
    }

    switch (property) {
        case kPropObjectIdentifier:
            if (requireNoIndex(arrayIndex) != ReadStatus::Ok) {
                return ReadStatus::PropertyIsNotArray;
            }
            return okOrOverflow(writer.appObjectId(object->type,
                                                   object->instance));
        case kPropObjectName:
            return okOrOverflow(writer.appString(object->name));
        case kPropObjectType:
            return okOrOverflow(writer.appEnumerated(object->type));
        case kPropDescription:
            return okOrOverflow(writer.appString(object->description));
        case kPropStatusFlags:
            return encodeStatusFlags(object, writer);
        case kPropEventState:
            /* 0 = normal */
            return okOrOverflow(writer.appEnumerated(0));
        case kPropReliability:
            /* 0 = no-fault-detected */
            return okOrOverflow(writer.appEnumerated(0));
        case kPropOutOfService:
            return okOrOverflow(writer.appBoolean(object->outOfService));
        case kPropPresentValue:
            if (requireNoIndex(arrayIndex) != ReadStatus::Ok) {
                return ReadStatus::PropertyIsNotArray;
            }
            if (object->isAnalog()) {
                return okOrOverflow(writer.appReal(object->presentValue));
            }
            return okOrOverflow(
                writer.appEnumerated(object->binaryPresentValue));
        case kPropUnits:
            return okOrOverflow(writer.appEnumerated(object->units));
        case kPropCovIncrement:
            return okOrOverflow(writer.appReal(object->covIncrement));
        case kPropMinPresValue:
            return okOrOverflow(writer.appReal(object->minPresValue));
        case kPropMaxPresValue:
            return okOrOverflow(writer.appReal(object->maxPresValue));
        case kPropResolution:
            return okOrOverflow(writer.appReal(0.01f));
        case kPropTimeDelay:
            return okOrOverflow(writer.appUnsigned(0));
        case kPropEventEnable:
            return encodeEventsEnabled(writer);
        case kPropAckedTransitions:
            return encodeEventsEnabled(writer);
        case kPropNotifyType:
            /* 0 = alarm */
            return okOrOverflow(writer.appEnumerated(0));
        case kPropPolarity:
            /* 0 = normal */
            return okOrOverflow(writer.appEnumerated(0));
        case kPropActiveText:
            return okOrOverflow(writer.appString(object->activeText));
        case kPropInactiveText:
            return okOrOverflow(writer.appString(object->inactiveText));
        case kPropPriorityArray:
            if (!object->isCommandable()) {
                return ReadStatus::UnknownProperty;
            }
            return encodePriorityArray(object, arrayIndex, writer);
        case kPropRelinquishDefault:
            if (requireNoIndex(arrayIndex) != ReadStatus::Ok) {
                return ReadStatus::PropertyIsNotArray;
            }
            if (!object->isCommandable()) {
                return ReadStatus::UnknownProperty;
            }
            if (object->isAnalog()) {
                return okOrOverflow(
                    writer.appReal(object->relinquishDefault));
            }
            return okOrOverflow(
                writer.appEnumerated(object->binaryRelinquishDefault));
        case kPropPropertyList:
            return encodePropertyList(object, arrayIndex, writer);
        default:
            return ReadStatus::UnknownProperty;
    }
}

/* ------------------------------------------------------------------------- */
/* WriteProperty                                                             */
/* ------------------------------------------------------------------------- */
bool writeProperty(Object *object,
                   uint32_t property,
                   bool hasArrayIndex,
                   uint32_t arrayIndex,
                   const Value &value,
                   uint8_t priority,
                   uint16_t &errorClass,
                   uint32_t &errorCode)
{
    if (!object) {
        errorClass = kErrorClassObject;
        errorCode = kErrorCodeUnknownObject;
        return false;
    }
    if (!isKnownProperty(object->type, property)) {
        errorClass = kErrorClassProperty;
        errorCode = kErrorCodeUnknownProperty;
        return false;
    }
    /* ASHRAE 135: an omitted priority is assumed to be 16 (the lowest).
     * Priority 6 is reserved for the Minimum On/Off algorithm and shall
     * not be used by WriteProperty. */
    if (priority == 0) {
        priority = kMaxPriority;
    } else if (priority == 6) {
        errorClass = kErrorClassProperty;
        errorCode = kErrorCodeWriteAccessDenied;
        return false;
    }

    if (hasArrayIndex && property != kPropPriorityArray) {
        errorClass = kErrorClassProperty;
        errorCode = kErrorCodePropertyIsNotAnArray;
        return false;
    }
    if (hasArrayIndex && property == kPropPriorityArray) {
        if (arrayIndex < 1 || arrayIndex > kMaxPriority) {
            errorClass = kErrorClassProperty;
            errorCode = kErrorCodeInvalidArrayIndex;
            return false;
        }
    }

    switch (property) {
        case kPropObjectName:
            if (value.type != Value::CharacterString) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeInvalidDataType;
                return false;
            }
            if (value.text[0] == '\0' ||
                strlen(value.text) >= sizeof(object->name)) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeValueOutOfRange;
                return false;
            }
            copyText(object->name, sizeof(object->name), value.text);
            return true;

        case kPropDescription:
            if (value.type != Value::CharacterString) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeInvalidDataType;
                return false;
            }
            copyText(object->description, sizeof(object->description),
                     value.text);
            return true;

        case kPropOutOfService:
            if (value.type != Value::Boolean) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeInvalidDataType;
                return false;
            }
            object->outOfService = value.boolean;
            return true;

        case kPropPresentValue: {
            if (object->isInput()) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeWriteAccessDenied;
                return false;
            }

            if (object->isAnalog()) {
                if (value.type == Value::Null) {
                    if (object->outOfService) {
                        return true;
                    }
                    relinquishAnalog(object, priority);
                    return true;
                }
                if (value.type != Value::Real) {
                    errorClass = kErrorClassProperty;
                    errorCode = kErrorCodeInvalidDataType;
                    return false;
                }
                if (value.real < object->minPresValue ||
                    value.real > object->maxPresValue) {
                    errorClass = kErrorClassProperty;
                    errorCode = kErrorCodeValueOutOfRange;
                    return false;
                }
                if (object->outOfService) {
                    object->presentValue = value.real;
                } else {
                    setAnalogPriority(object, value.real, priority);
                }
                return true;
            }

            if (value.type == Value::Null) {
                if (object->outOfService) {
                    return true;
                }
                relinquishBinary(object, priority);
                return true;
            }
            uint8_t binaryValue = 0;
            if (!decodeBinaryValue(value, binaryValue)) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeInvalidDataType;
                return false;
            }
            if (object->outOfService) {
                object->binaryPresentValue = binaryValue;
            } else {
                setBinaryPriority(object, binaryValue, priority);
            }
            applyBinaryOutputs();
            return true;
        }

        case kPropPriorityArray: {
            if (!object->isCommandable()) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeWriteAccessDenied;
                return false;
            }
            const uint32_t slot = hasArrayIndex ? arrayIndex : 0;
            if (slot < 1 || slot > kMaxPriority) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeInvalidArrayIndex;
                return false;
            }
            if (value.type == Value::Null) {
                if (object->isAnalog()) {
                    object->priorityActive[slot - 1] = false;
                    resolveAnalog(object);
                } else {
                    object->binaryPriorityActive[slot - 1] = false;
                    resolveBinary(object);
                    applyBinaryOutputs();
                }
                return true;
            }
            if (object->isAnalog()) {
                if (value.type != Value::Real) {
                    errorClass = kErrorClassProperty;
                    errorCode = kErrorCodeInvalidDataType;
                    return false;
                }
                if (value.real < object->minPresValue ||
                    value.real > object->maxPresValue) {
                    errorClass = kErrorClassProperty;
                    errorCode = kErrorCodeValueOutOfRange;
                    return false;
                }
                object->priority[slot - 1] = value.real;
                object->priorityActive[slot - 1] = true;
                resolveAnalog(object);
            } else {
                uint8_t binaryValue = 0;
                if (!decodeBinaryValue(value, binaryValue)) {
                    errorClass = kErrorClassProperty;
                    errorCode = kErrorCodeInvalidDataType;
                    return false;
                }
                object->binaryPriority[slot - 1] = binaryValue;
                object->binaryPriorityActive[slot - 1] = true;
                resolveBinary(object);
                applyBinaryOutputs();
            }
            return true;
        }

        case kPropRelinquishDefault: {
            if (!object->isCommandable()) {
                errorClass = kErrorClassProperty;
                errorCode = kErrorCodeWriteAccessDenied;
                return false;
            }
            if (object->isAnalog()) {
                if (value.type != Value::Real) {
                    errorClass = kErrorClassProperty;
                    errorCode = kErrorCodeInvalidDataType;
                    return false;
                }
                if (value.real < object->minPresValue ||
                    value.real > object->maxPresValue) {
                    errorClass = kErrorClassProperty;
                    errorCode = kErrorCodeValueOutOfRange;
                    return false;
                }
                object->relinquishDefault = value.real;
                resolveAnalog(object);
            } else {
                uint8_t binaryValue = 0;
                if (!decodeBinaryValue(value, binaryValue)) {
                    errorClass = kErrorClassProperty;
                    errorCode = kErrorCodeInvalidDataType;
                    return false;
                }
                object->binaryRelinquishDefault = binaryValue;
                resolveBinary(object);
                applyBinaryOutputs();
            }
            return true;
        }

        default:
            errorClass = kErrorClassProperty;
            errorCode = kErrorCodeWriteAccessDenied;
            return false;
    }
}

/* ------------------------------------------------------------------------- */
/* Periodic hardware refresh                                                 */
/* ------------------------------------------------------------------------- */
void updateObjects(uint32_t nowMs)
{
    static uint32_t lastUpdateMs = 0;
    static bool initialized = false;

    /* Always reflect the commanded output as soon as possible. */
    applyBinaryOutputs();

    if (initialized && (nowMs - lastUpdateMs) < 500) {
        return;
    }
    lastUpdateMs = nowMs;
    initialized = true;

    Object *object = nullptr;

    object = findObject(kObjectAnalogInput, 0);
    if (object && !object->outOfService) {
        const float temperature = temperatureRead();
        if (isfinite(temperature) && temperature > -40.0f &&
            temperature < 150.0f) {
            object->presentValue = temperature;
        }
    }

    object = findObject(kObjectAnalogInput, 1);
    if (object && !object->outOfService &&
        WiFi.status() == WL_CONNECTED) {
        object->presentValue = static_cast<float>(WiFi.RSSI());
    }

    object = findObject(kObjectAnalogInput, 2);
    if (object && !object->outOfService) {
        object->presentValue = static_cast<float>(nowMs) / 1000.0f;
    }

    object = findObject(kObjectAnalogInput, 3);
    if (object && !object->outOfService) {
        const float phase = (static_cast<float>(nowMs) / 5000.0f) * 2.0f * PI;
        object->presentValue = 50.0f + 50.0f * sinf(phase);
    }

    object = findObject(kObjectBinaryInput, 0);
    if (object && !object->outOfService) {
        object->binaryPresentValue =
            (WiFi.status() == WL_CONNECTED) ? 1 : 0;
    }

    object = findObject(kObjectBinaryInput, 1);
    if (object && !object->outOfService) {
#if BACNET_BI1_GPIO >= 0
        object->binaryPresentValue =
            (digitalRead(BACNET_BI1_GPIO) == LOW) ? 1 : 0;
#else
        object->binaryPresentValue = 0;
#endif
    }
}

} // namespace bacnet
