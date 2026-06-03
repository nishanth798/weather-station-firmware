#include "data_logger.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = LOG_TAG_LOGGER;
static bool logger_initialized = false;
static uint32_t entry_count = 0;
static logger_stats_t logger_stats = {0};

/* ==================== Helper Functions ==================== */

/**
 * @brief Initialize SPIFFS filesystem
 */
static esp_err_t logger_init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_ESP_SPIFFS_NOT_MOUNTED) {
            ESP_LOGE(TAG, "SPIFFS not mounted");
        } else if (ret == ESP_ERR_ESP_SPIFFS_NOT_FOUND) {
            ESP_LOGE(TAG, "SPIFFS partition not found");
        } else {
            ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    /* Check SPIFFS info */
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPIFFS: Total=%d, Used=%d", total, used);
    return ESP_OK;
}

/**
 * @brief Count existing log entries
 */
static uint32_t logger_count_entries(void) {
    FILE *file = fopen(LOG_FILENAME, "rb");
    if (file == NULL) {
        return 0;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);

    if (file_size <= 0) {
        return 0;
    }

    uint32_t count = file_size / sizeof(weather_data_t);
    ESP_LOGI(TAG, "Found %d existing log entries", count);
    return count;
}

/* ==================== Public Functions ==================== */

esp_err_t logger_init(void) {
    if (logger_initialized) {
        ESP_LOGW(TAG, "Logger already initialized");
        return ESP_OK;
    }

#if USE_SPIFFS
    esp_err_t ret = logger_init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS");
        return ret;
    }
#endif

    /* Count existing entries */
    entry_count = logger_count_entries();
    
    /* Initialize stats */
    logger_stats.total_entries = 0;
    logger_stats.current_index = 0;
    logger_stats.last_write = 0;
    logger_stats.write_count = 0;
    logger_stats.read_count = 0;
    logger_stats.avg_temperature = 0.0f;

    logger_initialized = true;
    ESP_LOGI(TAG, "Logger initialized successfully");
    return ESP_OK;
}

esp_err_t logger_write(const weather_data_t *data) {
    if (!logger_initialized) {
        ESP_LOGE(TAG, "Logger not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(LOG_FILENAME, "ab");  /* Append binary */
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open log file for writing");
        return ESP_FAIL;
    }

    /* Write data entry */
    size_t written = fwrite(data, sizeof(weather_data_t), 1, file);
    fclose(file);

    if (written != 1) {
        ESP_LOGE(TAG, "Failed to write log entry");
        return ESP_FAIL;
    }

    entry_count++;
    logger_stats.total_entries++;
    logger_stats.write_count++;
    logger_stats.last_write = time(NULL);
    logger_stats.current_index = (logger_stats.current_index + 1) % MAX_LOG_ENTRIES;

    /* Update running average temperature */
    if (logger_stats.write_count == 1) {
        logger_stats.avg_temperature = data->temperature;
    } else {
        logger_stats.avg_temperature = 
            (logger_stats.avg_temperature * (logger_stats.write_count - 1) + data->temperature) / 
            logger_stats.write_count;
    }

    ESP_LOGD(TAG, "Log entry written. Total: %d", entry_count);
    return ESP_OK;
}

esp_err_t logger_read(uint32_t index, weather_data_t *data) {
    if (!logger_initialized) {
        ESP_LOGE(TAG, "Logger not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (index >= entry_count) {
        ESP_LOGE(TAG, "Invalid log index: %d (total: %d)", index, entry_count);
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(LOG_FILENAME, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open log file for reading");
        return ESP_FAIL;
    }

    /* Seek to entry */
    if (fseek(file, index * sizeof(weather_data_t), SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek in log file");
        fclose(file);
        return ESP_FAIL;
    }

    /* Read entry */
    size_t read = fread(data, sizeof(weather_data_t), 1, file);
    fclose(file);

    if (read != 1) {
        ESP_LOGE(TAG, "Failed to read log entry");
        return ESP_FAIL;
    }

    logger_stats.read_count++;
    return ESP_OK;
}

uint32_t logger_get_entry_count(void) {
    return entry_count;
}

esp_err_t logger_get_stats(logger_stats_t *stats) {
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &logger_stats, sizeof(logger_stats_t));
    return ESP_OK;
}

esp_err_t logger_clear(void) {
    if (!logger_initialized) {
        ESP_LOGE(TAG, "Logger not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Delete log file */
    if (remove(LOG_FILENAME) != 0) {
        ESP_LOGW(TAG, "Log file does not exist or failed to delete");
    }

    entry_count = 0;
    memset(&logger_stats, 0, sizeof(logger_stats_t));

    ESP_LOGI(TAG, "Log cleared");
    return ESP_OK;
}

int logger_export_json(char *buffer, size_t buffer_size, uint32_t max_entries) {
    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }

    if (entry_count == 0) {
        int written = snprintf(buffer, buffer_size, "[]");
        return written;
    }

    int offset = 0;
    uint32_t entries_to_export = (max_entries > entry_count) ? entry_count : max_entries;
    uint32_t start_index = (entry_count > max_entries) ? (entry_count - max_entries) : 0;

    /* Start JSON array */
    offset += snprintf(buffer + offset, buffer_size - offset, "[");

    for (uint32_t i = 0; i < entries_to_export && offset < buffer_size - 1; i++) {
        weather_data_t data;
        if (logger_read(start_index + i, &data) != ESP_OK) {
            continue;
        }

        if (i > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, ",");
        }

        offset += snprintf(buffer + offset, buffer_size - offset,
            "{\"t\":%ld,\"tmp\":%.2f,\"hum\":%.1f,\"ws\":%.2f,\"wd\":%.0f,\"rf\":%.1f,\"bat\":%d}",
            data.timestamp,
            data.temperature,
            data.humidity,
            data.wind_speed,
            data.wind_direction,
            data.rainfall,
            data.battery_percent
        );
    }

    /* End JSON array */
    if (offset < buffer_size - 1) {
        offset += snprintf(buffer + offset, buffer_size - offset, "]");
    }

    return offset;
}

esp_err_t logger_close(void) {
    if (!logger_initialized) {
        return ESP_OK;
    }

    logger_initialized = false;
    ESP_LOGI(TAG, "Logger closed");
    return ESP_OK;
}

esp_err_t logger_get_storage_info(uint32_t *total, uint32_t *used) {
    if (total == NULL || used == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if USE_SPIFFS
    esp_err_t ret = esp_spiffs_info("storage", (size_t *)total, (size_t *)used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get storage info: %s", esp_err_to_name(ret));
        return ret;
    }
#else
    *total = 0;
    *used = 0;
#endif

    return ESP_OK;
}
