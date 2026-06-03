#include "rs485_handler.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "string.h"

static const char *TAG = LOG_TAG_RS485;
static bool rs485_initialized = false;

/* ==================== Helper Functions ==================== */

/**
 * @brief Configure RS485 RTS pin for direction control
 * @param level 1 for transmit, 0 for receive
 */
static void rs485_set_rts(uint8_t level) {
    gpio_set_level(RS485_UART_RTS, level);
}

/**
 * @brief Validate frame header and footer
 * @param frame Pointer to frame data
 * @return true if frame is valid
 */
static bool rs485_validate_frame(const uint8_t *data, uint16_t length) {
    if (length < 4) {
        return false;
    }
    
    /* Check header */
    if (data[0] != FRAME_HEADER) {
        return false;
    }
    
    /* Check footer */
    if (data[length - 1] != FRAME_FOOTER) {
        return false;
    }
    
    return true;
}

/**
 * @brief Calculate simple checksum
 * @param data Pointer to data
 * @param length Data length
 * @return Checksum value
 */
static uint8_t rs485_calculate_checksum(const uint8_t *data, uint16_t length) {
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

/* ==================== Public Functions ==================== */

esp_err_t rs485_init(void) {
    if (rs485_initialized) {
        ESP_LOGW(TAG, "RS485 already initialized");
        return ESP_OK;
    }

    /* Configure UART */
    uart_config_t uart_config = {
        .baud_rate = RS485_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_NONE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    /* Apply UART configuration */
    esp_err_t ret = uart_param_config(RS485_UART_PORT, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set UART pins */
    ret = uart_set_pin(RS485_UART_PORT, RS485_UART_TXD, RS485_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Install UART driver */
    ret = uart_driver_install(RS485_UART_PORT, RS485_RX_BUFFER_SIZE, RS485_TX_BUFFER_SIZE, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configure RTS pin for RS485 direction control */
    gpio_config_t rts_config = {
        .pin_bit_mask = (1ULL << RS485_UART_RTS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ret = gpio_config(&rts_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure RTS GPIO: %s", esp_err_to_name(ret));
        uart_driver_delete(RS485_UART_PORT);
        return ret;
    }

    /* Set RTS to receive mode initially */
    rs485_set_rts(0);

    rs485_initialized = true;
    ESP_LOGI(TAG, "RS485 initialized successfully");
    return ESP_OK;
}

esp_err_t rs485_read_frame(rs485_frame_t *frame, uint32_t timeout_ms) {
    if (!rs485_initialized) {
        ESP_LOGE(TAG, "RS485 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[256];
    uint16_t length = 0;
    time_t start_time = esp_log_timestamp();
    
    /* Ensure we're in receive mode */
    rs485_set_rts(0);

    /* Read data with timeout */
    while ((esp_log_timestamp() - start_time) < timeout_ms) {
        int bytes_read = uart_read_bytes(RS485_UART_PORT, buffer + length, (256 - length), 100 / portTICK_PERIOD_MS);
        
        if (bytes_read > 0) {
            length += bytes_read;
            
            /* Check if we have a complete frame */
            if (length >= FRAME_SIZE && length > 0) {
                /* Look for frame footer */
                for (int i = 0; i < length; i++) {
                    if (buffer[i] == FRAME_FOOTER) {
                        /* Found potential frame end */
                        if (i > 0 && rs485_validate_frame(buffer, i + 1)) {
                            /* Valid frame found */
                            memcpy(frame->data, buffer, i + 1);
                            frame->length = i + 1;
                            frame->received_time = time(NULL);
                            frame->is_valid = true;
                            
                            ESP_LOGI(TAG, "Valid frame received: %d bytes", frame->length);
                            return ESP_OK;
                        }
                    }
                }
            }
        }
    }

    ESP_LOGW(TAG, "Frame read timeout");
    frame->is_valid = false;
    return ESP_ERR_TIMEOUT;
}

esp_err_t rs485_send(const uint8_t *data, uint16_t length) {
    if (!rs485_initialized) {
        ESP_LOGE(TAG, "RS485 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Set to transmit mode */
    rs485_set_rts(1);
    esp_rom_delay_us(10);  /* Small delay for RS485 direction change */

    /* Send data */
    int bytes_written = uart_write_bytes(RS485_UART_PORT, data, length);
    
    /* Wait for transmission to complete */
    uart_wait_tx_done(RS485_UART_PORT, 1000 / portTICK_PERIOD_MS);
    
    esp_rom_delay_us(10);  /* Small delay before switching to receive */
    
    /* Set back to receive mode */
    rs485_set_rts(0);

    if (bytes_written != length) {
        ESP_LOGE(TAG, "Failed to send all data: sent %d/%d bytes", bytes_written, length);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Data sent: %d bytes", bytes_written);
    return ESP_OK;
}

esp_err_t rs485_close(void) {
    if (!rs485_initialized) {
        return ESP_OK;
    }

    uart_driver_delete(RS485_UART_PORT);
    rs485_initialized = false;
    
    ESP_LOGI(TAG, "RS485 closed");
    return ESP_OK;
}

bool rs485_is_connected(void) {
    return rs485_initialized;
}

esp_err_t rs485_reset_buffers(void) {
    if (!rs485_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uart_flush_input(RS485_UART_PORT);
    uart_flush(RS485_UART_PORT);
    
    ESP_LOGI(TAG, "Buffers reset");
    return ESP_OK;
}
