#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t battery_percent;
    bool battery_low;
    bool battery_critical;
    bool wifi_connected;
    bool mqtt_connected;
    uint32_t uptime_seconds;
    uint32_t heap_free;
    uint32_t heap_used;
    float temperature;
} system_status_t;

/**
 * @brief Initialize system monitor
 * @return ESP_OK on success
 */
esp_err_t monitor_init(void);

/**
 * @brief Update system status
 * @return ESP_OK on success
 */
esp_err_t monitor_update(void);

/**
 * @brief Get current system status
 * @param status Pointer to system_status_t structure
 * @return ESP_OK on success
 */
esp_err_t monitor_get_status(system_status_t *status);

/**
 * @brief Get battery level
 * @return Battery percentage (0-100)
 */
uint8_t monitor_get_battery_level(void);

/**
 * @brief Get free heap size
 * @return Free heap in bytes
 */
uint32_t monitor_get_free_heap(void);

/**
 * @brief Get used heap size
 * @return Used heap in bytes
 */
uint32_t monitor_get_used_heap(void);

/**
 * @brief Get system uptime
 * @return Uptime in seconds
 */
uint32_t monitor_get_uptime(void);

/**
 * @brief Check WiFi connection status
 * @return true if connected
 */
bool monitor_is_wifi_connected(void);

/**
 * @brief Start background monitoring task
 * @return ESP_OK on success
 */
esp_err_t monitor_start_task(void);

/**
 * @brief Stop background monitoring task
 * @return ESP_OK on success
 */
esp_err_t monitor_stop_task(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_MONITOR_H */
