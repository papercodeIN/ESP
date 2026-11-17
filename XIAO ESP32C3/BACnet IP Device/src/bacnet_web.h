/**
 * @file bacnet_web.h
 * @brief Built-in web interface for configuration and live point values.
 */
#pragma once

namespace webui {

/* Register the HTTP routes and start listening on the web port. */
void begin();

/* Service HTTP clients. Call from the main loop. */
void run();

} // namespace webui
