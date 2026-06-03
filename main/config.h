#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== MQTT Configuration ==================== */
#define MQTT_BROKER_URI         "mqtt://your-mqtt-server.com:1883"
#define MQTT_USERNAME           "your_username"
#define MQTT_PASSWORD           "your_password"
#define MQTT_CLIENT_ID          "weather-station-001"

/* MQTT Topics */
#define MQTT_TOPIC_TEMPERATURE  "weather/temperature"
#define MQTT_TOPIC_HUMIDITY     "weather/humidity"
#define MQTT_TOPIC_WIND_SPEED   "weather/wind_speed"
#define MQTT_TOPIC_WIND_DIR     "weather/wind_direction"
#define MQTT_TOPIC_RAINFALL     "weather/rainfall"
#define MQTT_TOPIC_BATTERY      "weather/battery"
#define MQTT_TOPIC_STATUS       "weather/status"
#define MQTT_TOPIC_LOG          "weather/log"
#define MQTT_TOPIC_HEARTBEAT    "weather/heartbeat"

/* ==================== RS485 UART Configuration ==================== */
#define RS485_UART_PORT         UART_NUM_1
#define RS485_UART_BAUD         9600
#define RS485_UART_TXD          GPIO_NUM_17  /* Adjust to your pin */
#define RS485_UART_RXD          GPIO_NUM_16  /* Adjust to your pin */
#define RS485_UART_RTS          GPIO_NUM_18  /* Adjust to your pin - for RS485 direction */
#define RS485_RX_BUFFER_SIZE    256
#define RS485_TX_BUFFER_SIZE    256

/* ==================== Data Logging Configuration ==================== */
#define LOG_INTERVAL_SECONDS    300         /* Log every 5 minutes */
#define PUBLISH_INTERVAL_SECONDS 60         /* Publish every 1 minute */
#define MAX_LOG_ENTRIES         2880        /* ~2 days at 5min intervals */
#define LOG_FILENAME            "/spiffs/weather_log.bin"
#define USE_SPIFFS              1           /* Set to 1 for SPIFFS, 0 for SD card */

/* ==================== WiFi Configuration ==================== */
#define WIFI_SSID               "your_ssid"
#define WIFI_PASSWORD           "your_password"
#define WIFI_MAX_RETRY          5
#define WIFI_TIMEOUT_MS         10000

/* ==================== System Configuration ==================== */
#define LOG_TAG_MAIN            "WEATHER_MAIN"
#define LOG_TAG_RS485           "RS485"
#define LOG_TAG_MQTT            "MQTT"
#define LOG_TAG_LOGGER          "LOGGER"
#define LOG_TAG_PARSER          "PARSER"

#define TASK_STACK_SIZE         4096
#define TASK_PRIORITY_UART      3
#define TASK_PRIORITY_MQTT      2
#define TASK_PRIORITY_LOGGER    2
#define TASK_PRIORITY_MONITOR   1

/* RS-FSXCS Protocol Frame Structure */
/* Adjust these based on your actual weather station protocol */
#define FRAME_HEADER            0xAA
#define FRAME_FOOTER            0x55
#define FRAME_SIZE              32
#define DATA_TIMEOUT_MS         5000

/* ==================== Battery Monitoring ==================== */
#define ADC_PIN                 GPIO_NUM_4
#define BATTERY_LOW_THRESHOLD   20          /* Percentage */
#define BATTERY_CRITICAL        10          /* Percentage */

/* ==================== Sensor Data Structure ==================== */
typedef struct {
    float temperature;          /* °C */
    float humidity;             /* % RH */
    float wind_speed;           /* m/s */
    float wind_direction;       /* degrees 0-360 */
    float rainfall;             /* mm */
    uint8_t battery_percent;    /* % */
    uint8_t device_status;      /* Device health status */
    uint8_t sensor_status;      /* Individual sensor status */
    time_t timestamp;           /* Unix timestamp */
} weather_data_t;

/* ==================== Function Declarations ==================== */
void config_print(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
