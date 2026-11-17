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
    strncpy(s.serverUrl, OPC_DEFAULT_SERVER_URL, sizeof(s.serverUrl) - 1);
    strncpy(s.username, OPC_DEFAULT_USERNAME, sizeof(s.username) - 1);
    strncpy(s.password, OPC_DEFAULT_PASSWORD, sizeof(s.password) - 1);
    s.pollMs = OPC_DEFAULT_POLL_MS;

    /* Seed two slots against the companion "OPC UA Server" firmware and the
     * standard server clock; everything else stays disabled. */
    s.tags[0].enabled = true;
    s.tags[0].ns = 1;
    s.tags[0].stringId = true;
    s.tags[0].identNum = 0;
    strncpy(s.tags[0].identStr, "Temperature", sizeof(s.tags[0].identStr) - 1);
    strncpy(s.tags[0].name, "Temperature", sizeof(s.tags[0].name) - 1);

    s.tags[1].enabled = true;
    s.tags[1].ns = 1;
    s.tags[1].stringId = true;
    s.tags[1].identNum = 0;
    strncpy(s.tags[1].identStr, "Setpoint", sizeof(s.tags[1].identStr) - 1);
    strncpy(s.tags[1].name, "Setpoint", sizeof(s.tags[1].name) - 1);

    s.tags[2].enabled = true;
    s.tags[2].ns = 0;
    s.tags[2].stringId = false;
    s.tags[2].identNum = 2258;   /* ns=0;i=2258 Server_ServerStatus_CurrentTime */
    strncpy(s.tags[2].name, "Server time", sizeof(s.tags[2].name) - 1);
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
    prefs.begin("opcc", true);

    readText(prefs, "ssid", current.wifiSsid, sizeof(current.wifiSsid));
    readText(prefs, "pass", current.wifiPass, sizeof(current.wifiPass));
    readText(prefs, "host", current.hostname, sizeof(current.hostname));
    current.dhcp = prefs.getBool("dhcp", true);
    readText(prefs, "ip", current.ip, sizeof(current.ip));
    readText(prefs, "gw", current.gateway, sizeof(current.gateway));
    readText(prefs, "mask", current.netmask, sizeof(current.netmask));
    readText(prefs, "appass", current.apPass, sizeof(current.apPass));
    readText(prefs, "url", current.serverUrl, sizeof(current.serverUrl));
    readText(prefs, "user", current.username, sizeof(current.username));
    readText(prefs, "passw", current.password, sizeof(current.password));
    current.pollMs = prefs.getULong("poll", OPC_DEFAULT_POLL_MS);

    size_t stored = prefs.getBytesLength("tags");
    if (stored == sizeof(current.tags)) {
        prefs.getBytes("tags", current.tags, sizeof(current.tags));
    }

    prefs.end();
}

void save()
{
    prefs.begin("opcc", false);
    prefs.putString("ssid", current.wifiSsid);
    prefs.putString("pass", current.wifiPass);
    prefs.putString("host", current.hostname);
    prefs.putBool("dhcp", current.dhcp);
    prefs.putString("ip", current.ip);
    prefs.putString("gw", current.gateway);
    prefs.putString("mask", current.netmask);
    prefs.putString("appass", current.apPass);
    prefs.putString("url", current.serverUrl);
    prefs.putString("user", current.username);
    prefs.putString("passw", current.password);
    prefs.putULong("poll", current.pollMs);
    prefs.putBytes("tags", current.tags, sizeof(current.tags));
    prefs.end();
}

void reset()
{
    prefs.begin("opcc", false);
    prefs.clear();
    prefs.end();
    setDefaults(current);
}

Settings &get()
{
    return current;
}

} /* namespace settings */
