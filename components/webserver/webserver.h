#pragma once

/**
 * @brief Start the HTTP web server.
 *
 * Mounts SPIFFS and registers URI handlers for the web UI pages and the
 * device config/status REST endpoints.  Call after WiFi is initialised
 * (or even before — the server will accept connections on the SoftAP if
 * WiFi provisioning is needed).
 */
void webserver_start(void);

/** @brief Stop the web server and unmount SPIFFS. */
void webserver_stop(void);
