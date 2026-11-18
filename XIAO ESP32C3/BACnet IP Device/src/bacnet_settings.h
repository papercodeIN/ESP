/**
 * @file bacnet_settings.h
 * @brief Persistent device configuration stored in NVS (flash).
 *
 * The defaults come from bacnet_config.h. Everything can be changed at
 * runtime from the built-in web interface and survives power cycles.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace settings {

/* ------------------------------------------------------------------------- */
/* Configurable GPIO points                                                  */
/* ------------------------------------------------------------------------- */
/* One row per spare XIAO header pin. Rows are fixed to a pin so a pin can
 * never be assigned twice; a row is simply disabled to free it again. */
constexpr size_t kMaxGpioPoints = 6;

enum GpioPointKind : uint8_t {
    kGpioInput = 0,
    kGpioOutput = 1,
};

/* Software-only BACnet memory points. These rows are appended as AV/BV
 * objects at boot and are disabled by default. */
constexpr size_t kMaxMemoryPoints = 4;

enum MemoryPointKind : uint8_t {
    kMemoryBinary = 0,
    kMemoryAnalog = 1,
};

struct MemoryPoint {
    uint8_t enabled;
    uint8_t kind;       /* MemoryPointKind */
    uint8_t writable;
    uint8_t persistent;
    uint32_t instance;
    uint16_t units;
    uint16_t reserved;
    float defaultValue;
    float retainedValue;
    float minValue;
    float maxValue;
    float covIncrement;
    char name[33];
    char description[65];
};

struct MemoryConfig {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    MemoryPoint points[kMaxMemoryPoints];
};

const MemoryConfig &memory();
void setMemoryDefaults(MemoryConfig &config);
bool saveMemory(const MemoryConfig &config);
bool saveMemoryValue(size_t index, float value);
bool resetMemory();

struct GpioPoint {
    uint8_t enabled;
    uint8_t pin;
    uint8_t kind;      /* GpioPointKind */
    uint8_t activeLow; /* input: contact to GND with pull-up; output: drive
                          low while the point is active */
    uint32_t instance;
    char name[33];
};

struct GpioConfig {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    GpioPoint points[kMaxGpioPoints];
};

/* Header pin -> GPIO number for the rows exposed in the web UI. D0 (GPIO2)
 * and D9 (GPIO9) are reserved for the built-in BO:0 and BI:1 points. */
extern const uint8_t kGpioPointPins[kMaxGpioPoints];

/* Header label ("D1", "D2", ...) for a pin from kGpioPointPins. */
const char *gpioPinLabel(uint8_t pin);

/* Mapping table loaded from flash at begin(). */
const GpioConfig &gpio();

/* Seed a table with the default (all rows disabled) mapping. */
void setGpioDefaults(GpioConfig &config);

/* Persist a mapping table. Caller is responsible for validation. */
bool saveGpio(const GpioConfig &config);

/* Restore and persist the default mapping. */
bool resetGpio();

struct Config {
    /* Wi-Fi */
    char hostname[33];
    char wifiSsid[33];
    char wifiPassword[65];
    char apPassword[65];

    /* Network */
    uint8_t useStaticIp;
    char staticIp[16];
    char gateway[16];
    char subnet[16];
    char dns[16];

    /* BACnet device identity */
    uint32_t deviceInstance;
    char deviceName[64];
    char deviceDescription[80];
    char deviceLocation[32];
    uint16_t vendorId;
    char vendorName[48];
    char modelName[48];
    char firmwareRevision[16];
    char appSoftwareVersion[16];
    uint16_t bacnetPort;

    /* Services */
    uint8_t webEnabled;

    /* Internal storage metadata (do not edit from the web UI) */
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
};

/* Load settings from flash, seeding defaults on first boot. */
void begin();

/* Mutable configuration. Changes are only persisted by save(). */
Config &get();

/* Persist the current configuration. Returns false on a write error. */
bool save();

/* Restore defaults and persist them. */
bool resetToDefaults();

/* True when an SSID has been configured. */
bool wifiConfigured();

} // namespace settings
