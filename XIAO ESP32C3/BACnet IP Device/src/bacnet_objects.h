/**
 * @file bacnet_objects.h
 * @brief Object database for the XIAO ESP32C3 BACnet/IP device.
 *
 * The database is intentionally static: no dynamic allocation, no key lists,
 * and no hidden filesystem state. Add a point by adding one entry to the
 * table in bacnet_objects.cpp and one initialisation line in initObjects().
 */
#pragma once

#include <stdint.h>

#include "bacnet_encoding.h"
#include "bacnet_settings.h"
#include "bacnet_types.h"

namespace bacnet {

struct Object {
    Object() = default;
    Object(uint16_t objectType, uint32_t objectInstance)
        : type(objectType), instance(objectInstance)
    {
    }

    uint16_t type = 0;
    uint32_t instance = 0;

    char name[64] = {0};
    char description[80] = {0};

    /* Analog points (AI / AV) */
    uint16_t units = kUnitsNoUnits;
    float presentValue = 0.0f;
    float relinquishDefault = 0.0f;
    float priority[kMaxPriority] = {0};
    bool priorityActive[kMaxPriority] = {false};
    float covIncrement = 0.1f;
    float minPresValue = 0.0f;
    float maxPresValue = 100.0f;
    bool outOfService = false;

    /* Binary points (BI / BO / BV) */
    uint8_t binaryPresentValue = 0;
    uint8_t binaryRelinquishDefault = 0;
    uint8_t binaryPriority[kMaxPriority] = {0};
    bool binaryPriorityActive[kMaxPriority] = {false};
    const char *activeText = "Active";
    const char *inactiveText = "Inactive";

    /* GPIO binding: -1 = software-only (not wired to a pin). */
    int8_t gpioPin = -1;

    bool isAnalog() const
    {
        return type == kObjectAnalogInput || type == kObjectAnalogValue;
    }
    bool isBinary() const
    {
        return type == kObjectBinaryInput || type == kObjectBinaryOutput ||
               type == kObjectBinaryValue;
    }
    bool isCommandable() const
    {
        return type == kObjectAnalogValue || type == kObjectBinaryOutput ||
               type == kObjectBinaryValue;
    }
    bool isInput() const
    {
        return type == kObjectAnalogInput || type == kObjectBinaryInput;
    }
};

void initObjects();

/* Re-read device instance, name and description from the stored settings. */
void applyDeviceIdentity();

size_t objectCount();
const Object *objectAt(size_t index);
Object *findObject(uint16_t type, uint32_t instance);

/* Property identifiers readable on this object type (used by property-list
 * and by the all(8) ReadPropertyMultiple expansion). */
const uint32_t *objectPropertyList(uint16_t type, size_t *count);

/* Encode a property value. arrayIndex == kArrayAll means "whole array". */
ReadStatus readProperty(const Object *object,
                        uint32_t property,
                        uint32_t arrayIndex,
                        ByteWriter &writer);

/* Apply a WriteProperty. Returns false and fills errorClass/errorCode on
 * failure. priority == 0 means "priority not supplied". */
bool writeProperty(Object *object,
                   uint32_t property,
                   bool hasArrayIndex,
                   uint32_t arrayIndex,
                   const Value &value,
                   uint8_t priority,
                   uint16_t &errorClass,
                   uint32_t &errorCode);

/* Refresh inputs from hardware and drive outputs. Call from loop(). */
void updateObjects(uint32_t nowMs);

} // namespace bacnet
