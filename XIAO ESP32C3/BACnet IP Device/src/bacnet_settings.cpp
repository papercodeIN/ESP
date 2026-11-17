/**
 * @file bacnet_settings.cpp
 * @brief Persistent configuration backed by the ESP32 NVS partition.
 */
#include "bacnet_settings.h"

#include <Preferences.h>
#include <string.h>

#include "bacnet_config.h"

namespace settings {
namespace {

constexpr uint32_t kMagic = 0x4241434Fu; /* "BACO" */
constexpr uint16_t kVersion = 1;
constexpr const char *kNamespace = "bacnet";
constexpr const char *kKey = "cfg";

Config g_config;
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

} // namespace

void begin()
{
    if (g_ready) {
        return;
    }
    setDefaults(g_config);

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
    if (openPrefs()) {
        g_prefs.remove(kKey);
    }
    return save();
}

bool wifiConfigured()
{
    return g_config.wifiSsid[0] != '\0';
}

} // namespace settings
