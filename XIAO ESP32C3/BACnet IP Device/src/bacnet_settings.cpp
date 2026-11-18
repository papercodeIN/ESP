/**
 * @file bacnet_settings.cpp
 * @brief Persistent configuration backed by the ESP32 NVS partition.
 */
#include "bacnet_settings.h"

#include <Preferences.h>
#include <math.h>
#include <string.h>

#include "bacnet_config.h"

namespace settings {
namespace {

constexpr uint32_t kMagic = 0x4241434Fu; /* "BACO" */
constexpr uint16_t kVersion = 1;
constexpr const char *kNamespace = "bacnet";
constexpr const char *kKey = "cfg";

/* GPIO mappings live under their own key so adding them does not invalidate
 * the network/device configuration already stored on deployed devices. */
constexpr uint32_t kGpioMagic = 0x4750494Fu; /* "GPIO" */
constexpr uint16_t kGpioVersion = 1;
constexpr const char *kGpioKey = "gpio";

constexpr uint32_t kMemoryMagic = 0x4D454D4Fu; /* "MEMO" */
constexpr uint16_t kMemoryVersion = 1;
constexpr const char *kMemoryKey = "memory";

Config g_config;
GpioConfig g_gpioConfig;
MemoryConfig g_memoryConfig;
Preferences g_prefs;
bool g_ready = false;
bool g_prefsOpen = false;

bool openPrefs()
{
    if (g_prefsOpen) {
        return true;
    }
    if (!g_prefs.begin(kNamespace, false)) {
        return false;
    }
    g_prefsOpen = true;
    return true;
}

void copyText(char *destination, size_t capacity, const char *source)
{
    if (!destination || capacity == 0) {
        return;
    }
    if (!source) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

void setDefaults(Config &config)
{
    memset(&config, 0, sizeof(config));

    copyText(config.hostname, sizeof(config.hostname),
             BACNET_DEFAULT_HOSTNAME);
    copyText(config.wifiSsid, sizeof(config.wifiSsid),
             BACNET_DEFAULT_WIFI_SSID);
    copyText(config.wifiPassword, sizeof(config.wifiPassword),
             BACNET_DEFAULT_WIFI_PASSWORD);
    copyText(config.apPassword, sizeof(config.apPassword),
             BACNET_DEFAULT_AP_PASSWORD);

    config.useStaticIp = BACNET_DEFAULT_USE_STATIC_IP;
    copyText(config.staticIp, sizeof(config.staticIp),
             BACNET_DEFAULT_STATIC_IP);
    copyText(config.gateway, sizeof(config.gateway),
             BACNET_DEFAULT_GATEWAY_IP);
    copyText(config.subnet, sizeof(config.subnet),
             BACNET_DEFAULT_SUBNET_MASK);
    copyText(config.dns, sizeof(config.dns), BACNET_DEFAULT_DNS_IP);

    config.deviceInstance = BACNET_DEFAULT_DEVICE_INSTANCE;
    copyText(config.deviceName, sizeof(config.deviceName),
             BACNET_DEFAULT_DEVICE_NAME);
    copyText(config.deviceDescription, sizeof(config.deviceDescription),
             BACNET_DEFAULT_DEVICE_DESCRIPTION);
    copyText(config.deviceLocation, sizeof(config.deviceLocation),
             BACNET_DEFAULT_DEVICE_LOCATION);
    config.vendorId = BACNET_DEFAULT_VENDOR_ID;
    copyText(config.vendorName, sizeof(config.vendorName),
             BACNET_DEFAULT_VENDOR_NAME);
    copyText(config.modelName, sizeof(config.modelName),
             BACNET_DEFAULT_MODEL_NAME);
    copyText(config.firmwareRevision, sizeof(config.firmwareRevision),
             BACNET_DEFAULT_FIRMWARE_REVISION);
    copyText(config.appSoftwareVersion, sizeof(config.appSoftwareVersion),
             BACNET_DEFAULT_APP_VERSION);
    config.bacnetPort = BACNET_DEFAULT_PORT;
    config.webEnabled = BACNET_DEFAULT_WEB_ENABLED;

    config.magic = kMagic;
    config.version = kVersion;
    config.reserved = 0;
}

/* Make sure a blob loaded from flash is usable even if it was written by a
 * different firmware version. */
void sanitize(Config &config)
{
    config.hostname[sizeof(config.hostname) - 1] = '\0';
    config.wifiSsid[sizeof(config.wifiSsid) - 1] = '\0';
    config.wifiPassword[sizeof(config.wifiPassword) - 1] = '\0';
    config.apPassword[sizeof(config.apPassword) - 1] = '\0';
    config.staticIp[sizeof(config.staticIp) - 1] = '\0';
    config.gateway[sizeof(config.gateway) - 1] = '\0';
    config.subnet[sizeof(config.subnet) - 1] = '\0';
    config.dns[sizeof(config.dns) - 1] = '\0';
    config.deviceName[sizeof(config.deviceName) - 1] = '\0';
    config.deviceDescription[sizeof(config.deviceDescription) - 1] = '\0';
    config.deviceLocation[sizeof(config.deviceLocation) - 1] = '\0';
    config.vendorName[sizeof(config.vendorName) - 1] = '\0';
    config.modelName[sizeof(config.modelName) - 1] = '\0';
    config.firmwareRevision[sizeof(config.firmwareRevision) - 1] = '\0';
    config.appSoftwareVersion[sizeof(config.appSoftwareVersion) - 1] = '\0';

    if (config.hostname[0] == '\0') {
        copyText(config.hostname, sizeof(config.hostname),
                 BACNET_DEFAULT_HOSTNAME);
    }
    if (config.deviceInstance > 4194302u) {
        config.deviceInstance = 4194302u;
    }
    if (config.bacnetPort == 0) {
        config.bacnetPort = BACNET_DEFAULT_PORT;
    }
    config.useStaticIp = config.useStaticIp ? 1 : 0;
    config.webEnabled = config.webEnabled ? 1 : 0;
}

/* The configurable rows are fixed to the spare header pins, in order. */

void gpioSanitize(GpioConfig &config)
{
    for (size_t i = 0; i < kMaxGpioPoints; ++i) {
        GpioPoint &point = config.points[i];
        point.pin = kGpioPointPins[i];
        point.enabled = point.enabled ? 1 : 0;
        point.activeLow = point.activeLow ? 1 : 0;
        point.kind = (point.kind == kGpioOutput) ? kGpioOutput : kGpioInput;
        point.name[sizeof(point.name) - 1] = '\0';
        if (point.instance > 4194302u ||
            (point.kind == kGpioInput && point.instance <= 1) ||
            (point.kind == kGpioOutput && point.instance == 0)) {
            point.instance = static_cast<uint32_t>(2 + i);
        }
        if (point.enabled == 0) {
            point.name[0] = '\0';
        }
    }
}

void memorySanitize(MemoryConfig &config)
{
    for (size_t i = 0; i < kMaxMemoryPoints; ++i) {
        MemoryPoint &point = config.points[i];
        point.enabled = point.enabled ? 1 : 0;
        point.kind = (point.kind == kMemoryAnalog) ? kMemoryAnalog
                                                    : kMemoryBinary;
        point.writable = point.writable ? 1 : 0;
        point.persistent = point.persistent ? 1 : 0;
        point.name[sizeof(point.name) - 1] = '\0';
        point.description[sizeof(point.description) - 1] = '\0';
        if (point.instance > 4194302u) {
            point.instance = static_cast<uint32_t>(100 + i);
        }
        if (point.kind == kMemoryBinary) {
            point.defaultValue = point.defaultValue >= 0.5f ? 1.0f : 0.0f;
            point.retainedValue = point.retainedValue >= 0.5f ? 1.0f : 0.0f;
            point.minValue = 0.0f;
            point.maxValue = 1.0f;
            point.covIncrement = 1.0f;
        } else {
            if (!isfinite(point.minValue) || !isfinite(point.maxValue) ||
                point.minValue >= point.maxValue) {
                point.minValue = 0.0f;
                point.maxValue = 100.0f;
            }
            if (!isfinite(point.covIncrement) || point.covIncrement <= 0.0f) {
                point.covIncrement = 0.1f;
            }
            if (!isfinite(point.defaultValue)) {
                point.defaultValue = point.minValue;
            }
            if (!isfinite(point.retainedValue)) {
                point.retainedValue = point.defaultValue;
            }
            if (point.defaultValue < point.minValue ||
                point.defaultValue > point.maxValue) {
                point.defaultValue = point.minValue;
            }
            if (point.retainedValue < point.minValue ||
                point.retainedValue > point.maxValue) {
                point.retainedValue = point.defaultValue;
            }
        }
        if (point.enabled == 0) {
            point.name[0] = '\0';
            point.description[0] = '\0';
        }
    }
}

} // namespace

const uint8_t kGpioPointPins[kMaxGpioPoints] = {3, 4, 5, 6, 7, 10};

const char *gpioPinLabel(uint8_t pin)
{
    switch (pin) {
        case 3:
            return "D1";
        case 4:
            return "D2";
        case 5:
            return "D3";
        case 6:
            return "D4";
        case 7:
            return "D5";
        case 10:
            return "D10";
        default:
            return "";
    }
}

void setGpioDefaults(GpioConfig &config)
{
    memset(&config, 0, sizeof(config));
    config.magic = kGpioMagic;
    config.version = kGpioVersion;
    for (size_t i = 0; i < kMaxGpioPoints; ++i) {
        config.points[i].pin = kGpioPointPins[i];
        config.points[i].kind = kGpioInput;
        /* BI:0/1 and BO:0 are built in. Start every configurable row at
         * instance 2 or higher, which is valid for either BI or BO. */
        config.points[i].instance = static_cast<uint32_t>(2 + i);
    }
}

void setMemoryDefaults(MemoryConfig &config)
{
    memset(&config, 0, sizeof(config));
    config.magic = kMemoryMagic;
    config.version = kMemoryVersion;
    for (size_t i = 0; i < kMaxMemoryPoints; ++i) {
        config.points[i].kind = kMemoryBinary;
        config.points[i].instance = static_cast<uint32_t>(100 + i);
        config.points[i].minValue = 0.0f;
        config.points[i].maxValue = 1.0f;
        config.points[i].covIncrement = 1.0f;
    }
}

const GpioConfig &gpio()
{
    return g_gpioConfig;
}

const MemoryConfig &memory()
{
    return g_memoryConfig;
}

void begin()
{
    if (g_ready) {
        return;
    }
    setDefaults(g_config);
    setGpioDefaults(g_gpioConfig);
    setMemoryDefaults(g_memoryConfig);

    if (openPrefs()) {
        Config stored;
        memset(&stored, 0, sizeof(stored));
        const size_t read = g_prefs.getBytesLength(kKey);
        if (read == sizeof(Config)) {
            if (g_prefs.getBytes(kKey, &stored, sizeof(stored)) ==
                    sizeof(stored) &&
                stored.magic == kMagic && stored.version == kVersion) {
                g_config = stored;
            }
        }
    }
    sanitize(g_config);

    /* GPIO mappings: load them from their own key so older devices keep
     * working even though the main Config layout is unchanged. */
    if (openPrefs()) {
        GpioConfig stored;
        memset(&stored, 0, sizeof(stored));
        const size_t read = g_prefs.getBytesLength(kGpioKey);
        if (read == sizeof(GpioConfig)) {
            if (g_prefs.getBytes(kGpioKey, &stored, sizeof(stored)) ==
                    sizeof(stored) &&
                stored.magic == kGpioMagic &&
                stored.version == kGpioVersion) {
                gpioSanitize(stored);
                g_gpioConfig = stored;
            }
        }
    }

    if (openPrefs()) {
        MemoryConfig stored;
        memset(&stored, 0, sizeof(stored));
        const size_t read = g_prefs.getBytesLength(kMemoryKey);
        if (read == sizeof(MemoryConfig)) {
            if (g_prefs.getBytes(kMemoryKey, &stored, sizeof(stored)) ==
                    sizeof(stored) &&
                stored.magic == kMemoryMagic &&
                stored.version == kMemoryVersion) {
                memorySanitize(stored);
                g_memoryConfig = stored;
            }
        }
    }

    g_ready = true;
}

Config &get()
{
    return g_config;
}

bool save()
{
    if (!g_ready) {
        begin();
    }
    g_config.magic = kMagic;
    g_config.version = kVersion;
    if (!openPrefs()) {
        return false;
    }
    const size_t written = g_prefs.putBytes(kKey, &g_config, sizeof(g_config));
    return written == sizeof(g_config);
}

bool resetToDefaults()
{
    setDefaults(g_config);
    setGpioDefaults(g_gpioConfig);
    setMemoryDefaults(g_memoryConfig);
    if (openPrefs()) {
        g_prefs.remove(kKey);
        g_prefs.remove(kGpioKey);
        g_prefs.remove(kMemoryKey);
    }
    return save();
}

bool saveGpio(const GpioConfig &config)
{
    g_gpioConfig = config;
    g_gpioConfig.magic = kGpioMagic;
    g_gpioConfig.version = kGpioVersion;
    gpioSanitize(g_gpioConfig);
    if (!openPrefs()) {
        return false;
    }
    const size_t written =
        g_prefs.putBytes(kGpioKey, &g_gpioConfig, sizeof(g_gpioConfig));
    return written == sizeof(g_gpioConfig);
}

bool resetGpio()
{
    setGpioDefaults(g_gpioConfig);
    if (openPrefs()) {
        g_prefs.remove(kGpioKey);
    }
    return saveGpio(g_gpioConfig);
}

bool saveMemory(const MemoryConfig &config)
{
    g_memoryConfig = config;
    g_memoryConfig.magic = kMemoryMagic;
    g_memoryConfig.version = kMemoryVersion;
    memorySanitize(g_memoryConfig);
    if (!openPrefs()) {
        return false;
    }
    const size_t written =
        g_prefs.putBytes(kMemoryKey, &g_memoryConfig, sizeof(g_memoryConfig));
    return written == sizeof(g_memoryConfig);
}

bool saveMemoryValue(size_t index, float value)
{
    if (index >= kMaxMemoryPoints || !isfinite(value)) {
        return false;
    }
    g_memoryConfig.points[index].retainedValue = value;
    memorySanitize(g_memoryConfig);
    return saveMemory(g_memoryConfig);
}

bool resetMemory()
{
    setMemoryDefaults(g_memoryConfig);
    if (openPrefs()) {
        g_prefs.remove(kMemoryKey);
    }
    return saveMemory(g_memoryConfig);
}

bool wifiConfigured()
{
    return g_config.wifiSsid[0] != '\0';
}

} // namespace settings
