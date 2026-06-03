#include "system_monitor.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "MONITOR";
static system_status_t current_status = {0};
static bool monitor_initialized = false;
static TaskHandle_t monitor_task_handle = NULL;
static time_t startup_time = 0;

/* ADC calibration data */
static esp_adc_cal_characteristics_t adc_chars;

/* ==================== Helper Functions ==================== */

/**
 * @brief Initialize ADC for battery monitoring
 */
static esp_err_t monitor_init_adc(void) {
    adc_power_on();
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL_3, ADC_ATTEN_DB_11);
    
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    
    ESP_LOGI(TAG, "ADC initialized for battery monitoring");
    return ESP_OK;
}

/**
 * @brief Read battery level from ADC
 * @return Battery percentage (0-100)
 */
static uint8_t monitor_read_battery(void) {
    uint32_t adc_reading = 0;
    
    /* Take multiple readings for stability */
    for (int i = 0; i < 5; i++) {
        adc_reading += adc1_get_raw(ADC_CHANNEL_3);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    adc_reading /= 5;
    
    /* Convert to voltage */
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
    
    /* 
     * Convert voltage to battery percentage
     * Adjust these values based on your battery chemistry
     * For 3.7V Li-Po: 4.2V = 100%, 3.0V = 0%
     */
    uint8_t battery_percent = 0;
    
    if (voltage >= 4200) {
        battery_percent = 100;
    } else if (voltage <= 3000) {
        battery_percent = 0;
    } else {
        battery_percent = (uint8_t)((voltage - 3000) * 100 / (4200 - 3000));
    }
    
    return battery_percent;
}

/**
 * @brief Background monitoring task
 */
static void monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Monitor task started");
    
    while (monitor_initialized) {
        monitor_update();
        vTaskDelay(pdMS_TO_TICKS(30000));  /* Update every 30 seconds */
    }
    
    vTaskDelete(NULL);
}

/* ==================== Public Functions ==================== */

esp_err_t monitor_init(void) {
    if (monitor_initialized) {
        ESP_LOGW(TAG, "Monitor already initialized");
        return ESP_OK;
    }

    /* Initialize ADC */
    esp_err_t ret = monitor_init_adc();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initialize status structure */
    memset(&current_status, 0, sizeof(system_status_t));
    
    startup_time = time(NULL);
    monitor_initialized = true;

    ESP_LOGI(TAG, "System monitor initialized");
    return ESP_OK;
}

esp_err_t monitor_update(void) {
    if (!monitor_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Update battery level */
    current_status.battery_percent = monitor_read_battery();
    current_status.battery_low = (current_status.battery_percent <= BATTERY_LOW_THRESHOLD);
    current_status.battery_critical = (current_status.battery_percent <= BATTERY_CRITICAL);

    /* Update WiFi status */
    wifi_ap_record_t ap_info;
    current_status.wifi_connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);

    /* Update uptime */
    current_status.uptime_seconds = (uint32_t)(time(NULL) - startup_time);

    /* Update heap info */
    current_status.heap_free = esp_get_free_heap_size();
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    current_status.heap_used = info.total_allocated_bytes;

    /* Log warning if battery is low or critical */
    if (current_status.battery_critical) {
        ESP_LOGW(TAG, "CRITICAL: Battery level: %d%%", current_status.battery_percent);
    } else if (current_status.battery_low) {
        ESP_LOGW(TAG, "WARNING: Battery low: %d%%", current_status.battery_percent);
    }

    /* Log heap warning if low */
    if (current_status.heap_free < 20000) {  /* Less than 20KB free */
        ESP_LOGW(TAG, "Low heap memory: %d bytes free", current_status.heap_free);
    }

    ESP_LOGD(TAG, "Status update - Battery: %d%%, WiFi: %s, Uptime: %d sec, Heap: %d/%d bytes",
        current_status.battery_percent,
        current_status.wifi_connected ? "Connected" : "Disconnected",
        current_status.uptime_seconds,
        current_status.heap_free,
        current_status.heap_used);

    return ESP_OK;
}

esp_err_t monitor_get_status(system_status_t *status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!monitor_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(status, &current_status, sizeof(system_status_t));
    return ESP_OK;
}

uint8_t monitor_get_battery_level(void) {
    return current_status.battery_percent;
}

uint32_t monitor_get_free_heap(void) {
    return current_status.heap_free;
}

uint32_t monitor_get_used_heap(void) {
    return current_status.heap_used;
}

uint32_t monitor_get_uptime(void) {
    return current_status.uptime_seconds;
}

bool monitor_is_wifi_connected(void) {
    return current_status.wifi_connected;
}

esp_err_t monitor_start_task(void) {
    if (!monitor_initialized) {
        ESP_LOGE(TAG, "Monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (monitor_task_handle != NULL) {
        ESP_LOGW(TAG, "Monitor task already running");
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(
        monitor_task,
        "monitor_task",
        TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY_MONITOR,
        &monitor_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Monitor task started");
    return ESP_OK;
}

esp_err_t monitor_stop_task(void) {
    if (monitor_task_handle == NULL) {
        return ESP_OK;
    }

    vTaskDelete(monitor_task_handle);
    monitor_task_handle = NULL;

    ESP_LOGI(TAG, "Monitor task stopped");
    return ESP_OK;
}
