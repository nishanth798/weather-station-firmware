#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t total_entries;
    uint32_t current_index;
    time_t last_write;
    uint32_t write_count;
    uint32_t read_count;
    float avg_temperature;
} logger_stats_t;

/**
 * @brief Initialize data logger
 * @return ESP_OK on success
 */
esp_err_t logger_init(void);

/**
 * @brief Log weather data to storage
 * @param data Pointer to weather_data_t structure
 * @return ESP_OK on success
 */
esp_err_t logger_write(const weather_data_t *data);

/**
 * @brief Read logged data by index
 * @param index Log entry index
 * @param data Pointer to weather_data_t structure to fill
 * @return ESP_OK on success
 */
esp_err_t logger_read(uint32_t index, weather_data_t *data);

/**
 * @brief Get total number of logged entries
 * @return Total entries count
 */
uint32_t logger_get_entry_count(void);

/**
 * @brief Get logger statistics
 * @param stats Pointer to logger_stats_t structure
 * @return ESP_OK on success
 */
esp_err_t logger_get_stats(logger_stats_t *stats);

/**
 * @brief Clear all logged data
 * @return ESP_OK on success
 */
esp_err_t logger_clear(void);

/**
 * @brief Export log to JSON format
 * @param buffer Buffer to store JSON
 * @param buffer_size Size of buffer
 * @param max_entries Maximum entries to export
 * @return Number of bytes written
 */
int logger_export_json(char *buffer, size_t buffer_size, uint32_t max_entries);

/**
 * @brief Close logger
 * @return ESP_OK on success
 */
esp_err_t logger_close(void);

/**
 * @brief Get storage space information
 * @param total Total space in bytes
 * @param used Used space in bytes
 * @return ESP_OK on success
 */
esp_err_t logger_get_storage_info(uint32_t *total, uint32_t *used);

#ifdef __cplusplus
}
#endif

#endif /* DATA_LOGGER_H */
