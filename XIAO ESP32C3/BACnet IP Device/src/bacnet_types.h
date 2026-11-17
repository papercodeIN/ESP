/**
 * @file bacnet_types.h
 * @brief Small subset of BACnet constants and value containers used by this
 *        device implementation.
 *
 * The numeric values are taken from ASHRAE Standard 135 (BACnet) and match
 * the official bacnet-stack enums.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace bacnet {

/* ------------------------------------------------------------------------- */
/* Protocol limits                                                           */
/* ------------------------------------------------------------------------- */
constexpr uint16_t kMaxApdu = 1476;       /* advertised + accepted APDU size */
constexpr size_t kMaxApduBuffer = 1500;   /* internal APDU buffer            */
constexpr uint8_t kMaxPriority = 16;
constexpr uint32_t kArrayAll = 0xFFFFFFFFu;

/* ------------------------------------------------------------------------- */
/* Object types                                                              */
/* ------------------------------------------------------------------------- */
enum ObjectType : uint16_t {
    kObjectAnalogInput = 0,
    kObjectAnalogOutput = 1,
    kObjectAnalogValue = 2,
    kObjectBinaryInput = 3,
    kObjectBinaryOutput = 4,
    kObjectBinaryValue = 5,
    kObjectDevice = 8,
};

/* ------------------------------------------------------------------------- */
/* Property identifiers (only the ones this device implements)               */
/* ------------------------------------------------------------------------- */
enum PropertyId : uint32_t {
    kPropAckedTransitions = 0,
    kPropActiveText = 4,
    kPropAll = 8,   /* RPM-only: requests every readable property (Yabe's
                       property grid uses this) */
    kPropApduTimeout = 11,
    kPropApplicationSoftwareVersion = 12,
    kPropCovIncrement = 22,
    kPropDeadband = 25,
    kPropDescription = 28,
    kPropDeviceAddressBinding = 30,
    kPropEventEnable = 35,
    kPropEventState = 36,
    kPropFirmwareRevision = 44,
    kPropHighLimit = 45,
    kPropInactiveText = 46,
    kPropLimitEnable = 52,
    kPropLocalDate = 56,
    kPropLocalTime = 57,
    kPropLocation = 58,
    kPropLowLimit = 59,
    kPropMaxApduLengthAccepted = 62,
    kPropMaxPresValue = 65,
    kPropMinPresValue = 69,
    kPropModelName = 70,
    kPropNotifyType = 72,
    kPropNumberOfApduRetries = 73,
    kPropObjectIdentifier = 75,
    kPropObjectList = 76,
    kPropObjectName = 77,
    kPropObjectType = 79,
    kPropOutOfService = 81,
    kPropPolarity = 84,
    kPropPresentValue = 85,
    kPropPriorityArray = 87,
    kPropProtocolObjectTypesSupported = 96,
    kPropProtocolServicesSupported = 97,
    kPropProtocolVersion = 98,
    kPropReliability = 103,
    kPropRelinquishDefault = 104,
    kPropResolution = 106,
    kPropSegmentationSupported = 107,
    kPropStatusFlags = 111,
    kPropSystemStatus = 112,
    kPropTimeDelay = 113,
    kPropUnits = 117,
    kPropVendorIdentifier = 120,
    kPropVendorName = 121,
    kPropEventTimeStamps = 130,
    kPropProtocolRevision = 139,
    kPropDatabaseRevision = 155,
    kPropPropertyList = 371,
};

/* ------------------------------------------------------------------------- */
/* Service choices                                                           */
/* ------------------------------------------------------------------------- */
/* Confirmed service choices (ASHRAE 135, clause 15.6): the wire value is
 * the service-choice octet of the APDU. ReadProperty MUST be 12,
 * ReadPropertyMultiple 14 and WriteProperty 15 - clients such as Yabe,
 * BAC0 and the bacnet-stack encode exactly these values. */
enum ConfirmedService : uint8_t {
    kServiceConfirmedCovNotification = 1,
    kServiceSubscribeCov = 5,
    kServiceReadProperty = 12,
    kServiceReadPropertyMultiple = 14,
    kServiceWriteProperty = 15,
    kServiceDeviceCommunicationControl = 17,
    kServiceReinitializeDevice = 20,
};

enum UnconfirmedService : uint8_t {
    kServiceIAm = 0,
    kServiceUnconfirmedCovNotification = 2,
    kServiceWhoIs = 8,
};

/* ------------------------------------------------------------------------- */
/* Error / reject / abort values                                             */
/* ------------------------------------------------------------------------- */
enum ErrorClass : uint16_t {
    kErrorClassDevice = 0,
    kErrorClassObject = 1,
    kErrorClassProperty = 2,
    kErrorClassResources = 3,
    kErrorClassServices = 5,
};

enum ErrorCode : uint32_t {
    kErrorCodeOther = 0,
    kErrorCodeInvalidDataType = 9,
    kErrorCodeNoSpaceToWriteProperty = 20,
    kErrorCodeUnknownObject = 31,
    kErrorCodeUnknownProperty = 32,
    kErrorCodeValueOutOfRange = 37,
    kErrorCodeWriteAccessDenied = 40,
    kErrorCodeInvalidArrayIndex = 42,
    kErrorCodeDatatypeNotSupported = 47,
    kErrorCodePropertyIsNotAnArray = 50,
};

enum RejectReason : uint8_t {
    kRejectOther = 0,
    kRejectBufferOverflow = 1,
    kRejectInconsistentParameters = 2,
    kRejectInvalidParameterDataType = 3,
    kRejectInvalidTag = 4,
    kRejectMissingRequiredParameter = 5,
    kRejectParameterOutOfRange = 6,
    kRejectTooManyArguments = 7,
    kRejectUndefinedEnumeration = 8,
    kRejectUnrecognizedService = 9,
    kRejectInvalidDataEncoding = 10,
};

enum AbortReason : uint8_t {
    kAbortOther = 0,
    kAbortBufferOverflow = 1,
    kAbortInvalidApduInThisState = 2,
    kAbortSegmentationNotSupported = 4,
    kAbortOutOfResources = 9,
    kAbortApduTooLong = 11,
};

/* ------------------------------------------------------------------------- */
/* Engineering units used by the demo points                                 */
/* ------------------------------------------------------------------------- */
enum Units : uint16_t {
    kUnitsDegreesCelsius = 62,
    kUnitsSeconds = 73,
    kUnitsNoUnits = 95,
    kUnitsPercent = 98,
    kUnitsDecibels = 199,
};

/* ------------------------------------------------------------------------- */
/* Decoded application value                                                 */
/* ------------------------------------------------------------------------- */
struct BitString {
    uint8_t bitCount = 0;
    uint8_t bytes[4] = {0, 0, 0, 0};
};

struct Value {
    enum Type : uint8_t {
        Null = 0,
        Boolean,
        Unsigned,
        Signed,
        Real,
        Enumerated,
        CharacterString,
        ObjectIdentifier,
        BitStringValue,
    };

    Type type = Null;
    bool boolean = false;
    uint32_t unsignedValue = 0;
    int32_t signedValue = 0;
    float real = 0.0f;
    uint16_t objectType = 0;
    uint32_t objectInstance = 0;
    char text[96] = {0};
    BitString bitString;
};

/* ------------------------------------------------------------------------- */
/* Read result                                                               */
/* ------------------------------------------------------------------------- */
enum class ReadStatus : int8_t {
    Ok = 0,
    UnknownObject,
    UnknownProperty,
    InvalidArrayIndex,
    PropertyIsNotArray,
    BufferOverflow,
};

} // namespace bacnet
