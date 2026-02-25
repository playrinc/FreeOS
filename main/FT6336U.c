/**************************************************************************/
/*!
    @file     FT6336U.cpp
    Author:   Atsushi Sasaki (https://github.com/aselectroworks)
    License:  MIT (see LICENSE)

    @brief    ESP-IDF C driver for FT6336U CTP controller
    @note     Ported from Arduino C++ library to ESP-IDF C
*/
/**************************************************************************/

#include "ft6336u.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ft6336u";

// Define I2C timeout
#define I2C_MASTER_TIMEOUT_MS 1000

// --- Debug Macros ---
// Uncomment to enable debug messages
// #define FT6336U_DEBUG
#ifdef FT6336U_DEBUG
#define DEBUG_PRINT(...) ESP_LOGD(TAG, __VA_ARGS__)
#define DEBUG_PRINTLN(...) ESP_LOGD(TAG, __VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#endif


// --- Internal I2C Helper Functions ---

/**
 * @brief Read a single byte from a specified register
 */
static esp_err_t ft6336u_read_byte(ft6336u_dev_t *dev, uint8_t reg_addr, uint8_t *data) {
    return i2c_master_transmit_receive(dev->i2c_dev, &reg_addr, 1, data, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

/**
 * @brief Read multiple bytes from a specified register
 */
static esp_err_t ft6336u_read_bytes(ft6336u_dev_t *dev, uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev->i2c_dev, &reg_addr, 1, data, len, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

/**
 * @brief Write a single byte to a specified register
 */
static esp_err_t ft6336u_write_byte(ft6336u_dev_t *dev, uint8_t reg_addr, uint8_t data) {
    uint8_t buffer[2] = {reg_addr, data};
    DEBUG_PRINTLN("writeI2C reg 0x%02X -> 0x%02X", reg_addr, data);
    return i2c_master_transmit(dev->i2c_dev, buffer, 2, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

// --- Public API Functions ---

esp_err_t ft6336u_init(ft6336u_dev_t *dev, i2c_master_bus_handle_t bus_handle, gpio_num_t rst_n, gpio_num_t int_n) {
    if (dev == NULL || bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->rst_n = rst_n;
    dev->int_n = int_n;

    // Add device to I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_7, // Fix: Was I2C_ADDR_LEN_7BIT
        .device_address = I2C_ADDR_FT6336U,
        .scl_speed_hz = 400000, // 400kHz
    };
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev->i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure GPIO pins
    gpio_config_t io_conf = {0};
    // Interrupt pin
    io_conf.pin_bit_mask = (1ULL << int_n);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // Use pull-up
    io_conf.intr_type = GPIO_INTR_DISABLE;  // Interrupt will be configured by user app if needed
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure INT pin: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Reset pin
    io_conf.pin_bit_mask = (1ULL << rst_n);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure RST pin: %s", esp_err_to_name(ret));
        return ret;
    }

    // Perform hardware reset
    gpio_set_level(rst_n, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(rst_n, 1);
    vTaskDelay(pdMS_TO_TICKS(500)); // Wait for chip to boot

    // Check chip ID
    uint8_t chip_id = 0;
    ret = ft6336u_read_chip_id(dev, &chip_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read chip ID: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "FT6336U Chip ID: 0x%02X", chip_id);
    if (chip_id != 0x54) { // 0x54 is a common ID, adjust if needed
        ESP_LOGW(TAG, "Warning: Chip ID is not the expected value.");
    }
    
    // Set to polling mode
    ret = ft6336u_write_g_mode(dev, pollingMode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set polling mode");
        return ret;
    }

    // --- ADD THIS LINE ---
    // Force a fast active rate (e.g., 10 * 10ms = ~100Hz, check datasheet for exact value)
    // A lower value here means a faster report rate. Default is often 14.
    ret = ft6336u_write_byte(dev, FT6336U_ADDR_ACTIVE_MODE_RATE, 10);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set active mode rate");
        return ret;
    }

    return ESP_OK;
}

esp_err_t ft6336u_deinit(ft6336u_dev_t *dev) {
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_bus_rm_device(dev->i2c_dev);
}

// --- Register Read/Write Functions ---

esp_err_t ft6336u_read_device_mode(ft6336u_dev_t *dev, uint8_t *mode) {
    uint8_t temp;
    esp_err_t ret = ft6336u_read_byte(dev, FT6336U_ADDR_DEVICE_MODE, &temp);
    if (ret == ESP_OK) {
        *mode = (temp & 0x70) >> 4;
    }
    return ret;
}

esp_err_t ft6336u_write_device_mode(ft6336u_dev_t *dev, DEVICE_MODE_Enum mode) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_DEVICE_MODE, (mode & 0x07) << 4);
}

esp_err_t ft6336u_read_gesture_id(ft6336u_dev_t *dev, uint8_t *gesture_id) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_GESTURE_ID, gesture_id);
}

esp_err_t ft6336u_read_td_status(ft6336u_dev_t *dev, uint8_t *td_status) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_TD_STATUS, td_status);
}

esp_err_t ft6336u_read_touch_number(ft6336u_dev_t *dev, uint8_t *touch_num) {
    uint8_t temp;
    esp_err_t ret = ft6336u_read_byte(dev, FT6336U_ADDR_TD_STATUS, &temp);
    if (ret == ESP_OK) {
        *touch_num = temp & 0x0F;
    }
    return ret;
}

// Touch 1
esp_err_t ft6336u_read_touch1_x(ft6336u_dev_t *dev, uint16_t *x) {
    uint8_t read_buf[2];
    esp_err_t ret = ft6336u_read_bytes(dev, FT6336U_ADDR_TOUCH1_X, read_buf, 2);
    if (ret == ESP_OK) {
        *x = ((read_buf[0] & 0x0F) << 8) | read_buf[1];
    }
    return ret;
}

esp_err_t ft6336u_read_touch1_y(ft6336u_dev_t *dev, uint16_t *y) {
    uint8_t read_buf[2];
    esp_err_t ret = ft6336u_read_bytes(dev, FT6336U_ADDR_TOUCH1_Y, read_buf, 2);
    if (ret == ESP_OK) {
        *y = ((read_buf[0] & 0x0F) << 8) | read_buf[1];
    }
    return ret;
}

esp_err_t ft6336u_read_touch1_event(ft6336u_dev_t *dev, uint8_t *event) {
    uint8_t temp;
    esp_err_t ret = ft6336u_read_byte(dev, FT6336U_ADDR_TOUCH1_EVENT, &temp);
    if (ret == ESP_OK) {
        *event = temp >> 6;
    }
    return ret;
}

esp_err_t ft6336u_read_touch1_id(ft6336u_dev_t *dev, uint8_t *id) {
    uint8_t temp;
    esp_err_t ret = ft6336u_read_byte(dev, FT6336U_ADDR_TOUCH1_ID, &temp);
    if (ret == ESP_OK) {
        *id = temp >> 4;
    }
    return ret;
}

esp_err_t ft6336u_read_touch1_weight(ft6336u_dev_t *dev, uint8_t *weight) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_TOUCH1_WEIGHT, weight);
}

esp_err_t ft6336u_read_touch1_misc(ft6336u_dev_t *dev, uint8_t *misc) {
    uint8_t temp;
    esp_err_t ret = ft6336u_read_byte(dev, FT6336U_ADDR_TOUCH1_MISC, &temp);
    if (ret == ESP_OK) {
        *misc = temp >> 4;
    }
    return ret;
}

// Touch 2
esp_err_t ft6336u_read_touch2_x(ft6336u_dev_t *dev, uint16_t *x) {
    uint8_t read_buf[2];
    esp_err_t ret = ft6336u_read_bytes(dev, FT6336U_ADDR_TOUCH2_X, read_buf, 2);
    if (ret == ESP_OK) {
        *x = ((read_buf[0] & 0x0F) << 8) | read_buf[1];
    }
    return ret;
}

esp_err_t ft6336u_read_touch2_y(ft6336u_dev_t *dev, uint16_t *y) {
    uint8_t read_buf[2];
    esp_err_t ret = ft6336u_read_bytes(dev, FT6336U_ADDR_TOUCH2_Y, read_buf, 2);
    if (ret == ESP_OK) {
        *y = ((read_buf[0] & 0x0F) << 8) | read_buf[1];
    }
    return ret;
}

esp_err_t ft6336u_read_touch2_event(ft6336u_dev_t *dev, uint8_t *event) {
    uint8_t temp;
    esp_err_t ret = ft6336u_read_byte(dev, FT6336U_ADDR_TOUCH2_EVENT, &temp);
    if (ret == ESP_OK) {
        *event = temp >> 6;
    }
    return ret;
}

esp_err_t ft6336u_read_touch2_id(ft6336u_dev_t *dev, uint8_t *id) {
    uint8_t temp;
    esp_err_t ret = ft6336u_read_byte(dev, FT6336U_ADDR_TOUCH2_ID, &temp);
    if (ret == ESP_OK) {
        *id = temp >> 4;
    }
    return ret;
}

esp_err_t ft6336u_read_touch2_weight(ft6336u_dev_t *dev, uint8_t *weight) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_TOUCH2_WEIGHT, weight);
}

esp_err_t ft6336u_read_touch2_misc(ft6336u_dev_t *dev, uint8_t *misc) {
    uint8_t temp;
    esp_err_t ret = ft6336u_read_byte(dev, FT6336U_ADDR_TOUCH2_MISC, &temp);
    if (ret == ESP_OK) {
        *misc = temp >> 4;
    }
    return ret;
}

// Mode Parameter Register
esp_err_t ft6336u_read_touch_threshold(ft6336u_dev_t *dev, uint8_t *threshold) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_THRESHOLD, threshold);
}

esp_err_t ft6336u_read_filter_coefficient(ft6336u_dev_t *dev, uint8_t *coeff) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_FILTER_COE, coeff);
}

esp_err_t ft6336u_read_ctrl_mode(ft6336u_dev_t *dev, uint8_t *mode) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_CTRL, mode);
}

esp_err_t ft6336u_write_ctrl_mode(ft6336u_dev_t *dev, CTRL_MODE_Enum mode) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_CTRL, mode);
}

esp_err_t ft6336u_read_time_period_enter_monitor(ft6336u_dev_t *dev, uint8_t *time) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_TIME_ENTER_MONITOR, time);
}

esp_err_t ft6336u_read_active_rate(ft6336u_dev_t *dev, uint8_t *rate) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_ACTIVE_MODE_RATE, rate);
}

esp_err_t ft6336u_read_monitor_rate(ft6336u_dev_t *dev, uint8_t *rate) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_MONITOR_MODE_RATE, rate);
}

// Gesture Parameters
esp_err_t ft6336u_read_radian_value(ft6336u_dev_t *dev, uint8_t *val) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_RADIAN_VALUE, val);
}

esp_err_t ft6336u_write_radian_value(ft6336u_dev_t *dev, uint8_t val) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_RADIAN_VALUE, val);
}

esp_err_t ft6336u_read_offset_left_right(ft6336u_dev_t *dev, uint8_t *val) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_OFFSET_LEFT_RIGHT, val);
}

esp_err_t ft6336u_write_offset_left_right(ft6336u_dev_t *dev, uint8_t val) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_OFFSET_LEFT_RIGHT, val);
}

esp_err_t ft6336u_read_offset_up_down(ft6336u_dev_t *dev, uint8_t *val) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_OFFSET_UP_DOWN, val);
}

esp_err_t ft6336u_write_offset_up_down(ft6336u_dev_t *dev, uint8_t val) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_OFFSET_UP_DOWN, val);
}

esp_err_t ft6336u_read_distance_left_right(ft6336u_dev_t *dev, uint8_t *val) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_DISTANCE_LEFT_RIGHT, val);
}

esp_err_t ft6336u_write_distance_left_right(ft6336u_dev_t *dev, uint8_t val) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_DISTANCE_LEFT_RIGHT, val);
}

esp_err_t ft6336u_read_distance_up_down(ft6336u_dev_t *dev, uint8_t *val) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_DISTANCE_UP_DOWN, val);
}

esp_err_t ft6336u_write_distance_up_down(ft6336u_dev_t *dev, uint8_t val) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_DISTANCE_UP_DOWN, val);
}

esp_err_t ft6336u_read_distance_zoom(ft6336u_dev_t *dev, uint8_t *val) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_DISTANCE_ZOOM, val);
}

esp_err_t ft6336u_write_distance_zoom(ft6336u_dev_t *dev, uint8_t val) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_DISTANCE_ZOOM, val);
}

// System Information
esp_err_t ft6336u_read_library_version(ft6336u_dev_t *dev, uint16_t *version) {
    uint8_t read_buf[2];
    esp_err_t ret = ft6336u_read_bytes(dev, FT6336U_ADDR_LIBRARY_VERSION_H, read_buf, 2);
    if (ret == ESP_OK) {
        *version = ((read_buf[0] & 0x0F) << 8) | read_buf[1];
    }
    return ret;
}

esp_err_t ft6336u_read_chip_id(ft6336u_dev_t *dev, uint8_t *chip_id) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_CHIP_ID, chip_id);
}

esp_err_t ft6336u_read_g_mode(ft6336u_dev_t *dev, uint8_t *mode) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_G_MODE, mode);
}

esp_err_t ft6336u_write_g_mode(ft6336u_dev_t *dev, G_MODE_Enum mode) {
    return ft6336u_write_byte(dev, FT6336U_ADDR_G_MODE, mode);
}

esp_err_t ft6336u_read_pwrmode(ft6336u_dev_t *dev, uint8_t *mode) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_POWER_MODE, mode);
}

esp_err_t ft6336u_read_firmware_id(ft6336u_dev_t *dev, uint8_t *id) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_FIRMARE_ID, id);
}

esp_err_t ft6336u_read_focaltech_id(ft6336u_dev_t *dev, uint8_t *id) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_FOCALTECH_ID, id);
}

esp_err_t ft6336u_read_release_code_id(ft6336u_dev_t *dev, uint8_t *id) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_RELEASE_CODE_ID, id);
}

esp_err_t ft6336u_read_state(ft6336u_dev_t *dev, uint8_t *state) {
    return ft6336u_read_byte(dev, FT6336U_ADDR_STATE, state);
}

// --- Scan Function ---

esp_err_t ft6336u_scan(ft6336u_dev_t *dev, FT6336U_TouchPointType *touch_data) {
    esp_err_t ret;
    uint8_t touch_count = 0;
    ret = ft6336u_read_td_status(dev, &touch_count);
    if (ret != ESP_OK) return ret;

    // Use internal state `touch_point` to track status
    FT6336U_TouchPointType *tp_state = &dev->touch_point;
    
    tp_state->touch_count = touch_count & 0x0F;

    if (tp_state->touch_count == 0) {
        tp_state->tp[0].status = release;
        tp_state->tp[1].status = release;
    } else if (tp_state->touch_count == 1) {
        uint8_t id1;
        ret = ft6336u_read_touch1_id(dev, &id1);
        if (ret != ESP_OK) return ret;

        tp_state->tp[id1].status = (tp_state->tp[id1].status == release) ? touch : stream;
        ret = ft6336u_read_touch1_x(dev, &tp_state->tp[id1].x);
        if (ret != ESP_OK) return ret;
        ret = ft6336u_read_touch1_y(dev, &tp_state->tp[id1].y);
        if (ret != ESP_OK) return ret;
        
        tp_state->tp[~id1 & 0x01].status = release; // Release other point
    } else {
        // Touch count is 2 or more (device supports 2)
        uint8_t id1, id2;
        ret = ft6336u_read_touch1_id(dev, &id1);
        if (ret != ESP_OK) return ret;
        
        tp_state->tp[id1].status = (tp_state->tp[id1].status == release) ? touch : stream;
        ret = ft6336u_read_touch1_x(dev, &tp_state->tp[id1].x);
        if (ret != ESP_OK) return ret;
        ret = ft6336u_read_touch1_y(dev, &tp_state->tp[id1].y);
        if (ret != ESP_OK) return ret;

        ret = ft6336u_read_touch2_id(dev, &id2);
        if (ret != ESP_OK) return ret;

        tp_state->tp[id2].status = (tp_state->tp[id2].status == release) ? touch : stream;
        ret = ft6336u_read_touch2_x(dev, &tp_state->tp[id2].x);
        if (ret != ESP_OK) return ret;
        ret = ft6336u_read_touch2_y(dev, &tp_state->tp[id2].y);
        if (ret != ESP_OK) return ret;
    }

    // Copy internal state to output
    *touch_data = *tp_state;
    
    return ESP_OK;
}
