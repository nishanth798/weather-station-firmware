#ifndef RS485_HANDLER_H
#define RS485_HANDLER_H

#include "driver/uart.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t data[256];
    uint16_t length;
    time_t received_time;
    bool is_valid;
} rs485_frame_t;

/* ==================== Function Declarations ==================== */

/**
 * @brief Initialize RS485 UART communication
 * @return ESP_OK on success
 */
esp_err_t rs485_init(void);

/**
 * @brief Read a frame from RS485
 * @param frame Pointer to frame buffer
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t rs485_read_frame(rs485_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief Send data via RS485
 * @param data Pointer to data buffer
 * @param length Length of data
 * @return ESP_OK on success
 */
esp_err_t rs485_send(const uint8_t *data, uint16_t length);

/**
 * @brief Close RS485 communication
 * @return ESP_OK on success
 */
esp_err_t rs485_close(void);

/**
 * @brief Get RS485 connection status
 * @return true if connected, false otherwise
 */
bool rs485_is_connected(void);

/**
 * @brief Reset RS485 buffers
 * @return ESP_OK on success
 */
esp_err_t rs485_reset_buffers(void);

#ifdef __cplusplus
}
#endif

#endif /* RS485_HANDLER_H */
