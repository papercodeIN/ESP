/**
 * @file opcua_settings.cpp
 * @brief Persistent configuration stored in NVS (flash).
 */
#include "opcua_settings.h"

#include <Preferences.h>
#include <string.h>

#include "opcua_config.h"

namespace settings {

namespace {

Preferences prefs;
Settings current;

void setDefaults(Settings &s)
{
    memset(&s, 0, sizeof(s));
    strncpy(s.wifiSsid, OPC_DEFAULT_WIFI_SSID, sizeof(s.wifiSsid) - 1);
    strncpy(s.wifiPass, OPC_DEFAULT_WIFI_PASS, sizeof(s.wifiPass) - 1);
    strncpy(s.hostname, OPC_DEFAULT_HOSTNAME, sizeof(s.hostname) - 1);
    s.dhcp = true;
    strncpy(s.ip, OPC_DEFAULT_IP, sizeof(s.ip) - 1);
    strncpy(s.gateway, OPC_DEFAULT_GATEWAY, sizeof(s.gateway) - 1);
    strncpy(s.netmask, OPC_DEFAULT_NETMASK, sizeof(s.netmask) - 1);
    strncpy(s.apPass, OPC_DEFAULT_AP_PASS, sizeof(s.apPass) - 1);
    s.port = OPC_DEFAULT_PORT;
    strncpy(s.deviceName, OPC_DEFAULT_DEVICE_NAME, sizeof(s.deviceName) - 1);
    s.endpointHost[0] = '\0';
    s.setpoint = 21.5;
}

void readText(Preferences &store, const char *key, char *out, size_t capacity)
{
    String value = store.getString(key, "");
    if (value.length()) {
        strncpy(out, value.c_str(), capacity - 1);
        out[capacity - 1] = '\0';
    }
}

} /* namespace */

void load()
{
    setDefaults(current);
    prefs.begin("opcua", true);

    readText(prefs, "ssid", current.wifiSsid, sizeof(current.wifiSsid));
    readText(prefs, "pass", current.wifiPass, sizeof(current.wifiPass));
    readText(prefs, "host", current.hostname, sizeof(current.hostname));
    current.dhcp = prefs.getBool("dhcp", true);
    readText(prefs, "ip", current.ip, sizeof(current.ip));
    readText(prefs, "gw", current.gateway, sizeof(current.gateway));
    readText(prefs, "mask", current.netmask, sizeof(current.netmask));
    readText(prefs, "appass", current.apPass, sizeof(current.apPass));
    current.port = prefs.getUShort("port", OPC_DEFAULT_PORT);
    readText(prefs, "devname", current.deviceName, sizeof(current.deviceName));
    readText(prefs, "ephost", current.endpointHost, sizeof(current.endpointHost));
    current.setpoint = prefs.getDouble("setpoint", 21.5);

    prefs.end();
}

void save()
{
    prefs.begin("opcua", false);
    prefs.putString("ssid", current.wifiSsid);
    prefs.putString("pass", current.wifiPass);
    prefs.putString("host", current.hostname);
    prefs.putBool("dhcp", current.dhcp);
    prefs.putString("ip", current.ip);
    prefs.putString("gw", current.gateway);
    prefs.putString("mask", current.netmask);
    prefs.putString("appass", current.apPass);
    prefs.putUShort("port", current.port);
    prefs.putString("devname", current.deviceName);
    prefs.putString("ephost", current.endpointHost);
    prefs.putDouble("setpoint", current.setpoint);
    prefs.end();
}

void reset()
{
    prefs.begin("opcua", false);
    prefs.clear();
    prefs.end();
    setDefaults(current);
}

Settings &get()
{
    return current;
}

} /* namespace settings */
