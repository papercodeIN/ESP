/**
 * @file opcua_web.h
 * @brief HTTP configuration UI and JSON API.
 *
 * Endpoints
 *   GET  /                    single-page interface
 *   GET  /api/status          runtime status
 *   GET  /api/settings        stored configuration
 *   POST /api/settings        update configuration (form encoded)
 *   GET  /api/tags            tag slots with live values
 *   POST /api/tags            save the tag slot table (form encoded)
 *   POST /api/connect         (re)connect to the configured server
 *   POST /api/disconnect      close the session
 *   POST /api/reboot          restart the device
 *   POST /api/factory-reset   erase settings and restart
 */
#pragma once

namespace webui {

void start();
void tick();

} /* namespace webui */
