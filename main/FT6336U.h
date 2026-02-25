/**************************************************************************/
/*!
    @file     FT6336U.h
    Author:   Atsushi Sasaki(https://github.com/aselectroworks)
    License:  MIT (see LICENSE)

    @brief    ESP-IDF C driver for FT6336U CTP controller
    @note     Ported from Arduino C++ library to ESP-IDF C
*/
/**************************************************************************/

#ifndef _FT6336U_H
#define _FT6336U_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#define I2C_ADDR_FT6336U 0x38

// Touch Parameter
#define FT6336U_PRES_DOWN 0x2
#define FT6336U_COORD_UD  0x1

// Registers
#define FT6336U_ADDR_DEVICE_MODE    0x00
typedef enum {
    working_mode = 0b000,
    factory_mode = 0b100,
} DEVICE_MODE_Enum;
#define FT6336U_ADDR_GESTURE_ID     0x01
#define FT6336U_ADDR_TD_STATUS      0x02

#define FT6336U_ADDR_TOUCH1_EVENT   0x03
#define FT6336U_ADDR_TOUCH1_ID      0x05
#define FT6336U_ADDR_TOUCH1_X       0x03
#define FT6336U_ADDR_TOUCH1_Y       0x05
#define FT6336U_ADDR_TOUCH1_WEIGHT  0x07
#define FT6336U_ADDR_TOUCH1_MISC    0x08

#define FT6336U_ADDR_TOUCH2_EVENT   0x09
#define FT6336U_ADDR_TOUCH2_ID      0x0B
#define FT6336U_ADDR_TOUCH2_X       0x09
#define FT6336U_ADDR_TOUCH2_Y       0x0B
#define FT6336U_ADDR_TOUCH2_WEIGHT  0x0D
#define FT6336U_ADDR_TOUCH2_MISC    0x0E

#define FT6336U_ADDR_THRESHOLD          0x80
#define FT6336U_ADDR_FILTER_COE         0x85
#define FT6336U_ADDR_CTRL               0x86
typedef enum {
    keep_active_mode = 0,
    switch_to_monitor_mode = 1,
} CTRL_MODE_Enum;
#define FT6336U_ADDR_TIME_ENTER_MONITOR 0x87
#define FT6336U_ADDR_ACTIVE_MODE_RATE   0x88
#define FT6336U_ADDR_MONITOR_MODE_RATE  0x89

#define FT6336U_ADDR_RADIAN_VALUE           0x91
#define FT6336U_ADDR_OFFSET_LEFT_RIGHT    0x92
#define FT6336U_ADDR_OFFSET_UP_DOWN       0x93
#define FT6336U_ADDR_DISTANCE_LEFT_RIGHT  0x94
#define FT6336U_ADDR_DISTANCE_UP_DOWN     0x95
#define FT6336U_ADDR_DISTANCE_ZOOM        0x96

#define FT6336U_ADDR_LIBRARY_VERSION_H  0xA1
#define FT6336U_ADDR_LIBRARY_VERSION_L  0xA2
#define FT6336U_ADDR_CHIP_ID            0xA3
#define FT6336U_ADDR_G_MODE             0xA4
typedef enum {
    pollingMode = 0,
    triggerMode = 1,
} G_MODE_Enum;
#define FT6336U_ADDR_POWER_MODE         0xA5
#define FT6336U_ADDR_FIRMARE_ID         0xA6
#define FT6336U_ADDR_FOCALTECH_ID       0xA8
#define FT6336U_ADDR_RELEASE_CODE_ID    0xAF
#define FT6336U_ADDR_STATE              0xBC

// Function Specific Type
typedef enum {
    touch = 0,
    stream,
    release,
} TouchStatusEnum;

typedef struct {
    TouchStatusEnum status;
    uint16_t x;
    uint16_t y;
} TouchPointType;

typedef struct {
    uint8_t touch_count;
    TouchPointType tp[2];
} FT6336U_TouchPointType;

/**
 * @brief FT6336U device handle structure
 * This replaces the C++ class object
 */
typedef struct {
    i2c_master_dev_handle_t i2c_dev;  // I2C device handle
    gpio_num_t int_n;                 // Interrupt pin
    gpio_num_t rst_n;                 // Reset pin
    FT6336U_TouchPointType touch_point; // Internal state for scan
} ft6336u_dev_t;

/**
 * @brief Initialize the FT6336U device
 *
 * @param dev Pointer to the device handle structure
 * @param bus_handle Handle to the I2C master bus
 * @param rst_n GPIO pin number for Reset
 * @param int_n GPIO pin number for Interrupt
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ft6336u_init(ft6336u_dev_t *dev, i2c_master_bus_handle_t bus_handle, gpio_num_t rst_n, gpio_num_t int_n);

/**
 * @brief De-initialize the FT6336U device
 *
 * @param dev Pointer to the device handle structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ft6336u_deinit(ft6336u_dev_t *dev);

// --- Public API Functions (mirrors C++ class) ---

esp_err_t ft6336u_read_device_mode(ft6336u_dev_t *dev, uint8_t *mode);
esp_err_t ft6336u_write_device_mode(ft6336u_dev_t *dev, DEVICE_MODE_Enum mode);
esp_err_t ft6336u_read_gesture_id(ft6336u_dev_t *dev, uint8_t *gesture_id);
esp_err_t ft6336u_read_td_status(ft6336u_dev_t *dev, uint8_t *td_status);
esp_err_t ft6336u_read_touch_number(ft6336u_dev_t *dev, uint8_t *touch_num);

esp_err_t ft6336u_read_touch1_x(ft6336u_dev_t *dev, uint16_t *x);
esp_err_t ft6336u_read_touch1_y(ft6336u_dev_t *dev, uint16_t *y);
esp_err_t ft6336u_read_touch1_event(ft6336u_dev_t *dev, uint8_t *event);
esp_err_t ft6336u_read_touch1_id(ft6336u_dev_t *dev, uint8_t *id);
esp_err_t ft6336u_read_touch1_weight(ft6336u_dev_t *dev, uint8_t *weight);
esp_err_t ft6336u_read_touch1_misc(ft6336u_dev_t *dev, uint8_t *misc);

esp_err_t ft6336u_read_touch2_x(ft6336u_dev_t *dev, uint16_t *x);
esp_err_t ft6336u_read_touch2_y(ft6336u_dev_t *dev, uint16_t *y);
esp_err_t ft6336u_read_touch2_event(ft6336u_dev_t *dev, uint8_t *event);
esp_err_t ft6336u_read_touch2_id(ft6336u_dev_t *dev, uint8_t *id);
esp_err_t ft6336u_read_touch2_weight(ft6336u_dev_t *dev, uint8_t *weight);
esp_err_t ft6336u_read_touch2_misc(ft6336u_dev_t *dev, uint8_t *misc);

// Mode Parameter Register
esp_err_t ft6336u_read_touch_threshold(ft6336u_dev_t *dev, uint8_t *threshold);
esp_err_t ft6336u_read_filter_coefficient(ft6336u_dev_t *dev, uint8_t *coeff);
esp_err_t ft6336u_read_ctrl_mode(ft6336u_dev_t *dev, uint8_t *mode);
esp_err_t ft6336u_write_ctrl_mode(ft6336u_dev_t *dev, CTRL_MODE_Enum mode);
esp_err_t ft6336u_read_time_period_enter_monitor(ft6336u_dev_t *dev, uint8_t *time);
esp_err_t ft6336u_read_active_rate(ft6336u_dev_t *dev, uint8_t *rate);
esp_err_t ft6336u_read_monitor_rate(ft6336u_dev_t *dev, uint8_t *rate);

// Gesture Parameter Register
esp_err_t ft6336u_read_radian_value(ft6336u_dev_t *dev, uint8_t *val);
esp_err_t ft6336u_write_radian_value(ft6336u_dev_t *dev, uint8_t val);
esp_err_t ft6336u_read_offset_left_right(ft6336u_dev_t *dev, uint8_t *val);
esp_err_t ft6336u_write_offset_left_right(ft6336u_dev_t *dev, uint8_t val);
esp_err_t ft6336u_read_offset_up_down(ft6336u_dev_t *dev, uint8_t *val);
esp_err_t ft6336u_write_offset_up_down(ft6336u_dev_t *dev, uint8_t val);
esp_err_t ft6336u_read_distance_left_right(ft6336u_dev_t *dev, uint8_t *val);
esp_err_t ft6336u_write_distance_left_right(ft6336u_dev_t *dev, uint8_t val);
esp_err_t ft6336u_read_distance_up_down(ft6336u_dev_t *dev, uint8_t *val);
esp_err_t ft6336u_write_distance_up_down(ft6336u_dev_t *dev, uint8_t val);
esp_err_t ft6336u_read_distance_zoom(ft6336u_dev_t *dev, uint8_t *val);
esp_err_t ft6336u_write_distance_zoom(ft6336u_dev_t *dev, uint8_t val);

// System Information
esp_err_t ft6336u_read_library_version(ft6336u_dev_t *dev, uint16_t *version);
esp_err_t ft6336u_read_chip_id(ft6336u_dev_t *dev, uint8_t *chip_id);
esp_err_t ft6336u_read_g_mode(ft6336u_dev_t *dev, uint8_t *mode);
esp_err_t ft6336u_write_g_mode(ft6336u_dev_t *dev, G_MODE_Enum mode);
esp_err_t ft6336u_read_pwrmode(ft6336u_dev_t *dev, uint8_t *mode);
esp_err_t ft6336u_read_firmware_id(ft6336u_dev_t *dev, uint8_t *id);
esp_err_t ft6336u_read_focaltech_id(ft6336u_dev_t *dev, uint8_t *id);
esp_err_t ft6336u_read_release_code_id(ft6336u_dev_t *dev, uint8_t *id);
esp_err_t ft6336u_read_state(ft6336u_dev_t *dev, uint8_t *state);

// Scan Function
/**
 * @brief Scan the touch panel for touch points
 *
 * @param dev Pointer to the device handle structure
 * @param touch_data Pointer to a struct to store the touch data
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ft6336u_scan(ft6336u_dev_t *dev, FT6336U_TouchPointType *touch_data);

#endif
