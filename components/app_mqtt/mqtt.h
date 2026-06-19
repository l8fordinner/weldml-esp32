#pragma once

#include <stdbool.h>

/**
 * @brief Initialise and connect the MQTT client.
 *
 * @param broker_url  Full broker URL, e.g. "mqtt://broker.example.com:1883"
 *                    or "mqtts://broker.example.com:8883".
 */
void mqtt_start(const char *broker_url);

/** @brief Stop the MQTT client and free resources. */
void mqtt_stop(void);

/**
 * @brief Publish a message to a topic.
 *
 * @param topic    Topic string.
 * @param payload  Payload bytes.
 * @param len      Payload length, or 0 to use strlen(payload).
 * @param qos      QoS level (0, 1, or 2).
 * @param retain   Retain flag.
 * @return         Message ID on success, -1 on failure.
 */
int mqtt_publish(const char *topic, const char *payload, int len,
                 int qos, bool retain);

/**
 * @brief Subscribe to a topic.
 *
 * @param topic  Topic filter (supports wildcards).
 * @param qos    QoS level.
 * @return       Message ID on success, -1 on failure.
 */
int mqtt_subscribe(const char *topic, int qos);

/** @brief Return true if the client is currently connected. */
bool mqtt_is_connected(void);
