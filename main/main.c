#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_sntp.h"

#include "config.h"
#include "rs485_handler.h"
#include "sensor_parser.h"
#include "mqtt_handler.h"
#include "data_logger.h"
#include "system_monitor.h"

static const char *TAG = LOG_TAG_MAIN;

/* ==================== WiFi Configuration ==================== */

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            WIFI_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", WIFI_SSID, WIFI_PASSWORD);
    } else {
        ESP_LOGI(TAG, "UNEXPECTED EVENT");
    }
}

/* ==================== Time Synchronization ==================== */

static void time_sync_notification_cb(timeval_t *tv) {
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    time_t now = time(NULL);
    struct tm timeinfo = *localtime(&now);
    ESP_LOGI(TAG, "The current date/time is: %s", asctime(&timeinfo));
}

static void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

/* ==================== RS485 Data Handler Task ==================== */

static void rs485_task(void *pvParameters) {
    ESP_LOGI(TAG, "RS485 task started");
    rs485_frame_t frame = {0};
    weather_data_t weather_data = {0};

    while (1) {
        /* Try to read a frame from RS485 */
        esp_err_t ret = rs485_read_frame(&frame, DATA_TIMEOUT_MS);
        
        if (ret == ESP_OK && frame.is_valid) {
            ESP_LOGI(TAG, "Received valid RS485 frame");

            /* Parse the frame */
            ret = parser_parse_frame(&frame, &weather_data);
            
            if (ret == ESP_OK) {
                /* Log the data */
                logger_write(&weather_data);

                /* Publish via MQTT if connected */
                if (mqtt_is_connected()) {
                    mqtt_publish_data(&weather_data);
                    ESP_LOGD(TAG, "Data published to MQTT");
                } else {
                    ESP_LOGD(TAG, "MQTT not connected, data logged locally");
                }

                /* Check battery level */
                if (weather_data.battery_percent <= BATTERY_CRITICAL) {
                    ESP_LOGE(TAG, "CRITICAL: Weather station battery critical: %d%%", 
                           weather_data.battery_percent);
                }
            } else {
                ESP_LOGW(TAG, "Failed to parse weather station frame");
            }
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "No data from weather station (timeout)");
        } else {
            ESP_LOGW(TAG, "RS485 read error: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));  /* Wait 1 second before next read */
    }
}

/* ==================== Publish Task ==================== */

static void publish_task(void *pvParameters) {
    ESP_LOGI(TAG, "Publish task started");
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t publish_interval = PUBLISH_INTERVAL_SECONDS * 1000;

    while (1) {
        /* Check if we should publish */
        if (mqtt_is_connected()) {
            uint32_t entry_count = logger_get_entry_count();
            
            if (entry_count > 0) {
                /* Get the latest entry */
                weather_data_t latest_data = {0};
                if (logger_read(entry_count - 1, &latest_data) == ESP_OK) {
                    mqtt_publish_data(&latest_data);
                }
            }

            /* Publish system status */
            system_status_t sys_status = {0};
            if (monitor_get_status(&sys_status) == ESP_OK) {
                char buffer[64];
                snprintf(buffer, sizeof(buffer), 
                    "{\"battery\":%d,\"heap\":%d,\"uptime\":%d}",
                    sys_status.battery_percent,
                    sys_status.heap_free,
                    sys_status.uptime_seconds);
                mqtt_publish("weather/system", buffer);
            }
        }

        xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(publish_interval));
    }
}

/* ==================== System Monitoring Task ==================== */

static void system_task(void *pvParameters) {
    ESP_LOGI(TAG, "System task started");
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        /* Update system status */
        monitor_update();

        /* Log system status */
        system_status_t status = {0};
        if (monitor_get_status(&status) == ESP_OK) {
            ESP_LOGI(TAG, "System Status - Battery: %d%%, Heap: %d bytes, Uptime: %d sec",
                status.battery_percent,
                status.heap_free,
                status.uptime_seconds);
            
            if (status.battery_critical) {
                ESP_LOGE(TAG, "CRITICAL: Battery level critical!");
            } else if (status.battery_low) {
                ESP_LOGW(TAG, "WARNING: Battery level low!");
            }
        }

        /* Log entry count periodically */
        uint32_t entries = logger_get_entry_count();
        if (entries % 10 == 0) {
            ESP_LOGI(TAG, "Total logged entries: %d", entries);
        }

        xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(60000));  /* Every 60 seconds */
    }
}

/* ==================== Main Application ==================== */

void app_main(void) {
    ESP_LOGI(TAG, "===== Weather Station Firmware Started =====");
    ESP_LOGI(TAG, "Build: %s %s", __DATE__, __TIME__);

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize system components */
    ESP_LOGI(TAG, "Initializing system components...");
    
    monitor_init();
    logger_init();
    rs485_init();

    /* Connect to WiFi */
    ESP_LOGI(TAG, "Connecting to WiFi...");
    wifi_init_sta();

    /* Initialize time */
    ESP_LOGI(TAG, "Initializing time synchronization...");
    time_t now = time(NULL);
    struct tm timeinfo = *localtime(&now);
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        initialize_sntp();

        time_t now = time(NULL);
        struct tm timeinfo = *localtime(&now);
        int retry = 0;
        const int retry_count = 15;
        while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
            ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            now = time(NULL);
            timeinfo = *localtime(&now);
        }
        ESP_LOGI(TAG, "System time is set");
    }

    /* Initialize MQTT */
    ESP_LOGI(TAG, "Initializing MQTT...");
    mqtt_init();

    /* Wait a bit for MQTT to connect */
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* Create application tasks */
    ESP_LOGI(TAG, "Creating application tasks...");
    
    xTaskCreate(rs485_task, "rs485_task", TASK_STACK_SIZE * 2, NULL, TASK_PRIORITY_UART, NULL);
    xTaskCreate(publish_task, "publish_task", TASK_STACK_SIZE, NULL, TASK_PRIORITY_MQTT, NULL);
    xTaskCreate(system_task, "system_task", TASK_STACK_SIZE, NULL, TASK_PRIORITY_MONITOR, NULL);

    /* Start background monitoring */
    monitor_start_task();

    ESP_LOGI(TAG, "===== Weather Station Firmware Ready =====");
    ESP_LOGI(TAG, "Waiting for sensor data...");

    /* Application is now running with FreeRTOS tasks */
}
