#include "mqtt_handler.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = LOG_TAG_MQTT;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_status_t mqtt_status = MQTT_STATUS_DISCONNECTED;

/* ==================== MQTT Event Handler ==================== */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_status = MQTT_STATUS_CONNECTED;
            
            /* Subscribe to command topics */
            esp_mqtt_client_subscribe(mqtt_client, "weather/command", 1);
            esp_mqtt_client_subscribe(mqtt_client, "weather/config", 1);
            
            /* Publish connected status */
            esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_HEARTBEAT, "connected", 0, 1, 0);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_status = MQTT_STATUS_DISCONNECTED;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            
            /* Handle incoming commands */
            if (strncmp(event->topic, "weather/command", event->topic_len) == 0) {
                /* Parse and handle command */
                if (event->data_len > 0) {
                    char command[32];
                    memcpy(command, event->data, (event->data_len < 31) ? event->data_len : 31);
                    command[(event->data_len < 31) ? event->data_len : 31] = '\0';
                    
                    ESP_LOGI(TAG, "Received command: %s", command);
                    
                    if (strcmp(command, "reset") == 0) {
                        ESP_LOGI(TAG, "Reset command received");
                        /* Handle reset */
                    } else if (strcmp(command, "sync_time") == 0) {
                        ESP_LOGI(TAG, "Sync time command received");
                        /* Handle time sync */
                    }
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            mqtt_status = MQTT_STATUS_ERROR;
            
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused by broker");
            }
            break;

        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGD(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            mqtt_status = MQTT_STATUS_CONNECTING;
            break;

        case MQTT_EVENT_DELETED:
            ESP_LOGD(TAG, "MQTT_EVENT_DELETED");
            break;

        default:
            ESP_LOGD(TAG, "Other event id:%d", event_id);
            break;
    }
}

/* ==================== Public Functions ==================== */

esp_err_t mqtt_init(void) {
    if (mqtt_client != NULL) {
        ESP_LOGW(TAG, "MQTT already initialized");
        return ESP_OK;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = MQTT_BROKER_URI,
            },
        },
        .credentials = {
            .username = MQTT_USERNAME,
            .authentication = {
                .password = MQTT_PASSWORD,
            },
        },
        .session = {
            .protocol_ver = MQTT_PROTOCOL_V_3_1_1,
            .disable_clean_session = false,
        },
        .network = {
            .reconnect_timeout_ms = 10000,
        },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    /* Register event handler */
    esp_err_t ret = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return ret;
    }

    /* Start MQTT client */
    ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return ret;
    }

    mqtt_status = MQTT_STATUS_CONNECTING;
    ESP_LOGI(TAG, "MQTT client initialized and started");
    return ESP_OK;
}

esp_err_t mqtt_publish_data(const weather_data_t *data) {
    if (!mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char buffer[32];
    int ret;

    /* Publish temperature */
    snprintf(buffer, sizeof(buffer), "%.2f", data->temperature);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_TEMPERATURE, buffer, 0, 1, 0);

    /* Publish humidity */
    snprintf(buffer, sizeof(buffer), "%.1f", data->humidity);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_HUMIDITY, buffer, 0, 1, 0);

    /* Publish wind speed */
    snprintf(buffer, sizeof(buffer), "%.2f", data->wind_speed);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_WIND_SPEED, buffer, 0, 1, 0);

    /* Publish wind direction */
    snprintf(buffer, sizeof(buffer), "%.0f", data->wind_direction);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_WIND_DIR, buffer, 0, 1, 0);

    /* Publish rainfall */
    snprintf(buffer, sizeof(buffer), "%.1f", data->rainfall);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_RAINFALL, buffer, 0, 1, 0);

    /* Publish battery */
    snprintf(buffer, sizeof(buffer), "%d", data->battery_percent);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_BATTERY, buffer, 0, 1, 0);

    /* Publish device status */
    snprintf(buffer, sizeof(buffer), "%d", data->device_status);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATUS, buffer, 0, 1, 0);

    ESP_LOGD(TAG, "Weather data published");
    return ESP_OK;
}

esp_err_t mqtt_publish(const char *topic, const char *value) {
    if (!mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT not connected");
        return ESP_ERR_INVALID_STATE;
    }

    if (topic == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int ret = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to publish to topic %s", topic);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Published to %s: %s", topic, value);
    return ESP_OK;
}

esp_err_t mqtt_subscribe(const char *topic) {
    if (!mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT not connected");
        return ESP_ERR_INVALID_STATE;
    }

    if (topic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int ret = esp_mqtt_client_subscribe(mqtt_client, topic, 1);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic %s", topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Subscribed to topic: %s", topic);
    return ESP_OK;
}

mqtt_status_t mqtt_get_status(void) {
    return mqtt_status;
}

bool mqtt_is_connected(void) {
    return (mqtt_client != NULL) && (mqtt_status == MQTT_STATUS_CONNECTED);
}

esp_err_t mqtt_stop(void) {
    if (mqtt_client == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = esp_mqtt_client_stop(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;
    mqtt_status = MQTT_STATUS_DISCONNECTED;

    ESP_LOGI(TAG, "MQTT client stopped and destroyed");
    return ESP_OK;
}

esp_mqtt_client_handle_t mqtt_get_client(void) {
    return mqtt_client;
}
