# Advanced Features & Customization Guide

## 1. Customizing the Sensor Protocol

The RS-FSXCS protocol parsing is in `sensor_parser.c`. To modify for your weather station:

### Step 1: Identify Frame Structure
Get the RS-FSXCS manual and note:
- Frame header/footer bytes
- Data field offsets and sizes
- Data format (big-endian vs little-endian)
- Conversion factors

### Step 2: Modify parser_parse_frame()

Example: If your station sends temperature differently:

```c
/* Original: 2 bytes, big-endian, in 0.1°C */
uint16_t temp_raw = ((uint16_t)frame->data[idx] << 8) | frame->data[idx + 1];
data->temperature = (float)temp_raw / 10.0f;

/* Modified: 1 byte as signed integer in °C */
data->temperature = (int8_t)frame->data[idx];
```

### Step 3: Update Validation

Modify `parser_validate_data()` with correct ranges:

```c
/* Adjust for your climate */
if (data->temperature < -40.0f || data->temperature > 60.0f) {
    ESP_LOGW(TAG, "Invalid temperature: %.2f", data->temperature);
    return false;
}
```

## 2. Implementing OTA (Over-The-Air) Updates

Add OTA support for remote firmware updates:

```c
#include "esp_ota_ops.h"
#include "esp_http_client.h"

esp_err_t ota_update_begin(const char *url) {
    esp_http_client_config_t config = {
        .url = url,
        .skip_cert_common_name_check = true,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    return esp_https_ota(&ota_config);
}
```

Add to MQTT handler for remote triggering:

```c
if (strncmp(event->topic, "weather/ota", event->topic_len) == 0) {
    char url[256];
    memcpy(url, event->data, event->data_len);
    url[event->data_len] = '\0';
    
    ESP_LOGI(TAG, "Starting OTA update from: %s", url);
    ota_update_begin(url);
}
```

## 3. Implementing Secure MQTT (TLS/SSL)

### Step 1: Get MQTT Broker Certificates

```bash
# Download CA certificate
wget https://cacert.org/certs/ca-bundle.crt
openssl x509 -in ca-bundle.crt -outform PEM -out ca.pem
```

### Step 2: Add to Firmware

Embed certificates in your project:

```c
extern const unsigned char ca_cert_pem_start[] asm("_binary_ca_pem_start");
extern const unsigned char ca_cert_pem_end[]   asm("_binary_ca_pem_end");

esp_mqtt_client_config_t mqtt_cfg = {
    .broker = {
        .address = {
            .uri = "mqtts://your-broker.com:8883",
        },
        .verification = {
            .certificate = (const char *)ca_cert_pem_start,
        },
    },
};
```

## 4. Adding Data Encryption

### AES-256 Example

```c
#include "mbedtls/aes.h"

esp_err_t encrypt_data(const uint8_t *plaintext, size_t length,
                      uint8_t *ciphertext, const uint8_t *key) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, 256);
    
    uint8_t iv[16] = {0};
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, length, iv,
                         plaintext, ciphertext);
    
    mbedtls_aes_free(&ctx);
    return ESP_OK;
}
```

## 5. Offline Data Buffering

Enhanced offline operation with local queue:

```c
typedef struct {
    weather_data_t data;
    bool is_sent;
    uint32_t retry_count;
} queued_entry_t;

esp_err_t mqtt_publish_with_queue(const weather_data_t *data) {
    if (mqtt_is_connected()) {
        /* Send directly */
        return mqtt_publish_data(data);
    } else {
        /* Queue for later */
        queued_entry_t entry = {
            .data = *data,
            .is_sent = false,
            .retry_count = 0
        };
        return logger_write_queue(&entry);
    }
}
```

## 6. Adding Web Dashboard

Minimal HTTP server in main.c:

```c
static const char html_page[] = R"(
<!DOCTYPE html>
<html>
<head>
<title>Weather Station</title>
<style>
  body { font-family: Arial; background: #f0f0f0; }
  .container { max-width: 800px; margin: 50px auto; }
  .card { background: white; padding: 20px; border-radius: 5px; margin: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
  .value { font-size: 24px; font-weight: bold; color: #2196F3; }
</style>
</head>
<body>
<div class="container">
  <h1>Weather Station Dashboard</h1>
  <div class="card">
    <h2>Temperature</h2>
    <div class="value" id="temp">Loading...</div>
  </div>
  <div class="card">
    <h2>Battery</h2>
    <div class="value" id="battery">Loading...</div>
  </div>
</div>
<script>
  setInterval(() => {
    fetch('/api/weather')
      .then(r => r.json())
      .then(d => {
        document.getElementById('temp').textContent = d.temperature + '°C';
        document.getElementById('battery').textContent = d.battery + '%';
      });
  }, 5000);
</script>
</body>
</html>
)";
```

## 7. Data Compression

Reduce bandwidth with gzip compression:

```c
#include "esp_zlib.h"

esp_err_t compress_data(const uint8_t *data, size_t data_len,
                       uint8_t *compressed, size_t *compressed_len) {
    z_stream stream = {};
    deflateInit(&stream, Z_DEFAULT_COMPRESSION);
    
    stream.avail_in = data_len;
    stream.next_in = (uint8_t *)data;
    stream.avail_out = *compressed_len;
    stream.next_out = compressed;
    
    deflate(&stream, Z_FINISH);
    *compressed_len = stream.total_out;
    deflateEnd(&stream);
    
    return ESP_OK;
}
```

## 8. Remote Configuration Management

Update configuration over MQTT:

```c
esp_err_t update_config(const char *key, const char *value) {
    if (strcmp(key, "log_interval") == 0) {
        uint32_t interval = atoi(value);
        /* Apply new interval */
        return ESP_OK;
    } else if (strcmp(key, "publish_interval") == 0) {
        uint32_t interval = atoi(value);
        /* Apply new interval */
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* In MQTT handler, add: */
if (strncmp(event->topic, "weather/config", event->topic_len) == 0) {
    char key[32], value[32];
    sscanf(event->data, "%s=%s", key, value);
    update_config(key, value);
}
```

## 9. Performance Optimization

### Reduce Memory Usage

```c
/* Use static buffers instead of dynamic */
static uint8_t mqtt_buffer[256];

/* Reduce log buffer sizes */
#define RS485_RX_BUFFER_SIZE    128  /* Was 256 */

/* Optimize stack size */
#define TASK_STACK_SIZE         2048  /* Was 4096 */
```

### Reduce Power Consumption

```c
/* Increase intervals for low-power operation */
#define LOG_INTERVAL_SECONDS    600   /* 10 minutes */
#define PUBLISH_INTERVAL_SECONDS 300  /* 5 minutes */

/* Disable WiFi scanning between connections */
wifi_config_t wifi_cfg = {...};
wifi_cfg.sta.scan_method = WIFI_FAST_SCAN;

/* Use light sleep */
esp_light_sleep_start();
```

## 10. Adding SIM800L LTE Module

Replace WiFi with cellular for remote locations:

```c
#include "sim800.h"

esp_err_t sim800_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_NONE,
        .stop_bits = UART_STOP_BITS_1,
    };
    
    uart_param_config(UART_NUM_2, &uart_config);
    uart_driver_install(UART_NUM_2, 256, 0, 0, NULL);
    
    /* Send AT commands to initialize */
    uart_write_bytes(UART_NUM_2, "ATE0\r", 5);
    /* Check response... */
    
    return ESP_OK;
}
```

## Testing & Debugging

### Unit Testing
```c
/* Add to main/CMakeLists.txt */
idf_component_register(
    ...
    TEST_REQUIRES "unity"
)

/* Create main/test/test_parser.c */
void test_temperature_parsing(void) {
    uint8_t frame[] = {0xAA, 0x08, 0x00, 0xC9, ...};
    weather_data_t data = {};
    rs485_frame_t test_frame = {...};
    
    TEST_ASSERT_EQUAL(ESP_OK, parser_parse_frame(&test_frame, &data));
    TEST_ASSERT_FLOAT_WITHIN(0.1, 20.1, data.temperature);
}
```

### Performance Profiling
```c
/* Add timing analysis */
uint32_t start = esp_timer_get_time();
/* Your code here */
uint32_t elapsed = esp_timer_get_time() - start;
ESP_LOGI(TAG, "Operation took %d microseconds", elapsed);
```

## Production Checklist

- [ ] All configuration values set correctly
- [ ] MQTT authentication enabled (TLS)
- [ ] Battery threshold values appropriate
- [ ] Data validation rules verified
- [ ] Error handling tested
- [ ] Offline operation tested
- [ ] WiFi reconnection tested
- [ ] MQTT reconnection tested
- [ ] Firmware update mechanism implemented
- [ ] Comprehensive logging enabled
- [ ] Performance monitored
- [ ] Memory leaks checked

---

**Ready to enhance?** Pick a feature and start implementing!
