#include "sensor_parser.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = LOG_TAG_PARSER;

/**
 * @brief Convert two bytes to float (big-endian)
 * @param high High byte
 * @param low Low byte
 * @return Float value
 */
static float bytes_to_float(uint8_t high, uint8_t low) {
    uint16_t value = ((uint16_t)high << 8) | low;
    return (float)value / 100.0f;  /* Assuming 2 decimal places */
}

/**
 * @brief Convert byte to percentage
 * @param value Byte value
 * @return Percentage (0-100)
 */
static uint8_t byte_to_percentage(uint8_t value) {
    return (value > 100) ? 100 : value;
}

/**
 * @brief Parse RS-FSXCS specific frame format
 * Frame format example (adjust based on actual protocol):
 * [HEADER][LENGTH][TYPE][DATA...][CHECKSUM][FOOTER]
 * 
 * DATA section for weather station:
 * Temp (2 bytes), Humidity (2 bytes), Wind Speed (2 bytes),
 * Wind Direction (2 bytes), Rainfall (2 bytes), Battery (1 byte), Status (1 byte)
 */
esp_err_t parser_parse_frame(const rs485_frame_t *frame, weather_data_t *data) {
    if (frame == NULL || data == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (!frame->is_valid || frame->length < 10) {
        ESP_LOGE(TAG, "Invalid frame");
        return ESP_FAIL;
    }

    /* Initialize data structure */
    memset(data, 0, sizeof(weather_data_t));

    /* Extract header (should be FRAME_HEADER) */
    if (frame->data[0] != FRAME_HEADER) {
        ESP_LOGE(TAG, "Invalid frame header");
        return ESP_FAIL;
    }

    /* 
     * Parse payload based on RS-FSXCS protocol
     * Adjust indices based on your actual protocol documentation
     */
    
    uint8_t idx = 1;  /* Start after header */
    
    /* Frame length (1 byte) */
    uint8_t frame_length = frame->data[idx++];
    if (frame_length != (frame->length - 3)) {  /* Subtract header, length, footer */
        ESP_LOGW(TAG, "Frame length mismatch: %d vs %d", frame_length, frame->length - 3);
    }

    /* Device type/ID (1 byte) - optional */
    uint8_t device_id = frame->data[idx++];
    
    /* Temperature (2 bytes, big-endian, in 0.1°C) */
    if (idx + 1 < frame->length) {
        uint16_t temp_raw = ((uint16_t)frame->data[idx] << 8) | frame->data[idx + 1];
        data->temperature = (float)temp_raw / 10.0f;
        idx += 2;
        ESP_LOGD(TAG, "Temperature: %.1f°C", data->temperature);
    }

    /* Humidity (2 bytes, big-endian, in 0.1%) */
    if (idx + 1 < frame->length) {
        uint16_t humidity_raw = ((uint16_t)frame->data[idx] << 8) | frame->data[idx + 1];
        data->humidity = (float)humidity_raw / 10.0f;
        if (data->humidity > 100.0f) {
            data->humidity = 100.0f;
        }
        idx += 2;
        ESP_LOGD(TAG, "Humidity: %.1f%%", data->humidity);
    }

    /* Wind Speed (2 bytes, big-endian, in 0.01 m/s) */
    if (idx + 1 < frame->length) {
        uint16_t wind_raw = ((uint16_t)frame->data[idx] << 8) | frame->data[idx + 1];
        data->wind_speed = (float)wind_raw / 100.0f;
        idx += 2;
        ESP_LOGD(TAG, "Wind Speed: %.2f m/s", data->wind_speed);
    }

    /* Wind Direction (2 bytes, big-endian, in degrees) */
    if (idx + 1 < frame->length) {
        uint16_t direction_raw = ((uint16_t)frame->data[idx] << 8) | frame->data[idx + 1];
        data->wind_direction = (float)direction_raw;
        if (data->wind_direction > 360.0f) {
            data->wind_direction = 0.0f;
        }
        idx += 2;
        ESP_LOGD(TAG, "Wind Direction: %.0f°", data->wind_direction);
    }

    /* Rainfall (2 bytes, big-endian, in 0.1 mm) */
    if (idx + 1 < frame->length) {
        uint16_t rainfall_raw = ((uint16_t)frame->data[idx] << 8) | frame->data[idx + 1];
        data->rainfall = (float)rainfall_raw / 10.0f;
        idx += 2;
        ESP_LOGD(TAG, "Rainfall: %.1f mm", data->rainfall);
    }

    /* Battery Level (1 byte, percentage) */
    if (idx < frame->length) {
        data->battery_percent = byte_to_percentage(frame->data[idx]);
        idx++;
        ESP_LOGD(TAG, "Battery: %d%%", data->battery_percent);
    }

    /* Device Status (1 byte) */
    if (idx < frame->length) {
        data->device_status = frame->data[idx];
        idx++;
        ESP_LOGD(TAG, "Device Status: 0x%02X", data->device_status);
    }

    /* Sensor Status (1 byte) */
    if (idx < frame->length) {
        data->sensor_status = frame->data[idx];
        idx++;
        ESP_LOGD(TAG, "Sensor Status: 0x%02X", data->sensor_status);
    }

    /* Timestamp */
    data->timestamp = time(NULL);

    /* Validate parsed data */
    if (!parser_validate_data(data)) {
        ESP_LOGW(TAG, "Parsed data validation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Frame parsed successfully");
    return ESP_OK;
}

int parser_to_json(const weather_data_t *data, char *json_buffer, size_t buffer_size) {
    if (data == NULL || json_buffer == NULL) {
        return 0;
    }

    int written = snprintf(json_buffer, buffer_size,
        "{"
        "\"timestamp\":%ld,"
        "\"temperature\":%.2f,"
        "\"humidity\":%.1f,"
        "\"wind_speed\":%.2f,"
        "\"wind_direction\":%.0f,"
        "\"rainfall\":%.1f,"
        "\"battery\":%d,"
        "\"device_status\":%d,"
        "\"sensor_status\":%d"
        "}",
        data->timestamp,
        data->temperature,
        data->humidity,
        data->wind_speed,
        data->wind_direction,
        data->rainfall,
        data->battery_percent,
        data->device_status,
        data->sensor_status
    );

    return (written < 0) ? 0 : written;
}

bool parser_validate_data(const weather_data_t *data) {
    if (data == NULL) {
        return false;
    }

    /* Validate temperature range (-50 to 80°C for typical weather station) */
    if (data->temperature < -50.0f || data->temperature > 80.0f) {
        ESP_LOGW(TAG, "Invalid temperature: %.2f", data->temperature);
        return false;
    }

    /* Validate humidity range (0-100%) */
    if (data->humidity < 0.0f || data->humidity > 100.0f) {
        ESP_LOGW(TAG, "Invalid humidity: %.1f", data->humidity);
        return false;
    }

    /* Validate wind speed (should be positive) */
    if (data->wind_speed < 0.0f || data->wind_speed > 100.0f) {
        ESP_LOGW(TAG, "Invalid wind speed: %.2f", data->wind_speed);
        return false;
    }

    /* Validate wind direction (0-360 degrees) */
    if (data->wind_direction < 0.0f || data->wind_direction > 360.0f) {
        ESP_LOGW(TAG, "Invalid wind direction: %.0f", data->wind_direction);
        return false;
    }

    /* Validate rainfall (should be non-negative) */
    if (data->rainfall < 0.0f) {
        ESP_LOGW(TAG, "Invalid rainfall: %.1f", data->rainfall);
        return false;
    }

    /* Validate battery percentage (0-100%) */
    if (data->battery_percent > 100) {
        ESP_LOGW(TAG, "Invalid battery percentage: %d", data->battery_percent);
        return false;
    }

    return true;
}

const char* parser_get_status_string(uint8_t status_code) {
    switch (status_code) {
        case 0x00:
            return "OK";
        case 0x01:
            return "Sensor Error";
        case 0x02:
            return "Communication Error";
        case 0x04:
            return "Battery Low";
        case 0x08:
            return "Hardware Error";
        case 0x10:
            return "Calibration Error";
        case 0xFF:
            return "Unknown Error";
        default:
            return "Unknown Status";
    }
}
