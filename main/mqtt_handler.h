#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "config.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,
    MQTT_STATUS_CONNECTING = 1,
    MQTT_STATUS_CONNECTED = 2,
    MQTT_STATUS_ERROR = 3
} mqtt_status_t;

/**
 * @brief Initialize MQTT client and connect to broker
 * @return ESP_OK on success
 */
esp_err_t mqtt_init(void);

/**
 * @brief Publish weather data to MQTT topics
 * @param data Pointer to weather_data_t structure
 * @return ESP_OK on success
 */
esp_err_t mqtt_publish_data(const weather_data_t *data);

/**
 * @brief Publish individual sensor reading
 * @param topic MQTT topic
 * @param value String value to publish
 * @return ESP_OK on success
 */
esp_err_t mqtt_publish(const char *topic, const char *value);

/**
 * @brief Subscribe to MQTT topic for commands
 * @param topic Topic to subscribe to
 * @return ESP_OK on success
 */
esp_err_t mqtt_subscribe(const char *topic);

/**
 * @brief Get current MQTT connection status
 * @return Current mqtt_status_t
 */
mqtt_status_t mqtt_get_status(void);

/**
 * @brief Check if connected to MQTT broker
 * @return true if connected, false otherwise
 */
bool mqtt_is_connected(void);

/**
 * @brief Disconnect and cleanup MQTT
 * @return ESP_OK on success
 */
esp_err_t mqtt_stop(void);

/**
 * @brief Get MQTT client handle
 * @return esp_mqtt_client_handle_t
 */
esp_mqtt_client_handle_t mqtt_get_client(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_HANDLER_H */
