#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

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

/**
 * @brief Register a product-specific URI handler on the running server.
 *
 * Lets other components (e.g. weld results, clear) add endpoints without
 * modifying this generic component's own source. Must be called after
 * webserver_start(). A fixed number of extra slots are reserved beyond the
 * built-in URIs (see max_uri_handlers in webserver.c) — registering beyond
 * that capacity fails.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if the server isn't
 *         running yet, or the underlying httpd_register_uri_handler() error.
 */
esp_err_t webserver_register_uri(const httpd_uri_t *uri);

/**
 * @brief Serve a file from the SPIFFS partition this component mounts (/web),
 * with the given Content-Type header.
 *
 * For product-specific page routes registered via webserver_register_uri()
 * that want to reuse this component's static-file serving instead of
 * duplicating it (e.g. weld_processor's GET /results).
 */
esp_err_t webserver_serve_file(httpd_req_t *req, const char *path,
                                const char *content_type);
