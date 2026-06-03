#ifndef SENSOR_PARSER_H
#define SENSOR_PARSER_H

#include "config.h"
#include "rs485_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse RS485 frame into weather data structure
 * @param frame Raw RS485 frame
 * @param data Pointer to weather_data_t structure to fill
 * @return ESP_OK on success, ESP_FAIL on parse error
 */
esp_err_t parser_parse_frame(const rs485_frame_t *frame, weather_data_t *data);

/**
 * @brief Convert weather data to JSON string
 * @param data Pointer to weather_data_t structure
 * @param json_buffer Buffer to store JSON string
 * @param buffer_size Size of json_buffer
 * @return Length of JSON string written
 */
int parser_to_json(const weather_data_t *data, char *json_buffer, size_t buffer_size);

/**
 * @brief Validate weather data values
 * @param data Pointer to weather_data_t structure
 * @return true if data is valid, false otherwise
 */
bool parser_validate_data(const weather_data_t *data);

/**
 * @brief Get sensor status description
 * @param status_code Status code from sensor
 * @return Status description string
 */
const char* parser_get_status_string(uint8_t status_code);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_PARSER_H */
