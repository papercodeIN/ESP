/**
 * @file opcua_web.h
 * @brief HTTP configuration UI and JSON API.
 *
 * Endpoints
 *   GET  /                    single-page interface
 *   GET  /api/status          runtime status
 *   GET  /api/settings        stored configuration
 *   POST /api/settings        update configuration (form encoded)
 *   GET  /api/tags            OPC UA tag snapshot with live values
 *   POST /api/tags/write      write a writable tag
 *   POST /api/reboot          restart the device
 *   POST /api/factory-reset   erase settings and restart
 */
#pragma once

namespace webui {

void start();
void tick();

} /* namespace webui */
