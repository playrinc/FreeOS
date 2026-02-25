/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "es8311.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/i2c_master.h" // <--- NEW DRIVER HEADER
#include "es8311_reg.h"


// Removed: #include "es8311_reg.h" (Assuming definitions are inside es8311.h or local.
// If you have a separate reg header, keep it included!)

// --- Internal Struct for the New Driver ---
typedef struct {
    i2c_master_dev_handle_t i2c_handle; // <--- Uses Device Handle, not Port ID
} es8311_dev_t;


/*
 * Clock coefficient structure
 */
struct _coeff_div {
    uint32_t mclk;        /* mclk frequency */
    uint32_t rate;        /* sample rate */
    uint8_t pre_div;      /* the pre divider with range from 1 to 8 */
    uint8_t pre_multi;    /* the pre multiplier with 0: 1x, 1: 2x, 2: 4x, 3: 8x selection */
    uint8_t adc_div;      /* adcclk divider */
    uint8_t dac_div;      /* dacclk divider */
    uint8_t fs_mode;      /* double speed or single speed, =0, ss, =1, ds */
    uint8_t lrck_h;       /* adclrck divider and daclrck divider */
    uint8_t lrck_l;
    uint8_t bclk_div;     /* sclk divider */
    uint8_t adc_osr;      /* adc osr */
    uint8_t dac_osr;      /* dac osr */
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
    /* 8k */
    {12288000, 8000, 0x06, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 8000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x05, 0xff, 0x18, 0x10, 0x10},
    {16384000, 8000, 0x08, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 8000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 8000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 8000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 8000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1024000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    /* 11.025k */
    {11289600, 11025, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 11025, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 11025, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    /* 12k */
    {12288000, 12000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 12000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 12000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    /* 16k */
    {12288000, 16000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 16000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 16000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 16000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 16000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1024000, 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    /* 44.1k */
    {11289600, 44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 44100, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    /* 48k */
    {12288000, 48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 48000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 48000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
};

static const char *TAG = "ES8311";

// --- NEW WRITE FUNCTION ---
// --- SAFELY UPDATED WRITE FUNCTION ---
// Removed 'static inline' so you can use it in scan.c
esp_err_t es8311_write_reg(es8311_handle_t dev, uint8_t reg_addr, uint8_t data)
{
    es8311_dev_t *es = (es8311_dev_t *) dev;
    const uint8_t write_buf[2] = {reg_addr, data};
    // Change -1 to 1000 (1 second timeout)
    return i2c_master_transmit(es->i2c_handle, write_buf, sizeof(write_buf), 1000);
}

// --- SAFELY UPDATED READ FUNCTION ---
// Removed 'static inline' so you can use it in scan.c
esp_err_t es8311_read_reg(es8311_handle_t dev, uint8_t reg_addr, uint8_t *reg_value)
{
    es8311_dev_t *es = (es8311_dev_t *) dev;
    // Change -1 to 1000 (1 second timeout)
    return i2c_master_transmit_receive(es->i2c_handle, &reg_addr, 1, reg_value, 1, 1000);
}

static int get_coeff(uint32_t mclk, uint32_t rate)
{
    for (int i = 0; i < (sizeof(coeff_div) / sizeof(coeff_div[0])); i++) {
        if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk) {
            return i;
        }
    }
    return -1;
}

esp_err_t es8311_sample_frequency_config(es8311_handle_t dev, int mclk_frequency, int sample_frequency)
{
    uint8_t regv;
    int coeff = get_coeff(mclk_frequency, sample_frequency);

    if (coeff < 0) {
        ESP_LOGE(TAG, "Unable to configure sample rate %dHz with %dHz MCLK", sample_frequency, mclk_frequency);
        return ESP_ERR_INVALID_ARG;
    }

    const struct _coeff_div *const selected_coeff = &coeff_div[coeff];

    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_CLK_MANAGER_REG02, &regv), TAG, "I2C read/write error");
    regv &= 0x07;
    regv |= (selected_coeff->pre_div - 1) << 5;
    regv |= selected_coeff->pre_multi << 3;
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG02, regv), TAG, "I2C read/write error");

    const uint8_t reg03 = (selected_coeff->fs_mode << 6) | selected_coeff->adc_osr;
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG03, reg03), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG04, selected_coeff->dac_osr), TAG, "I2C read/write error");

    const uint8_t reg05 = ((selected_coeff->adc_div - 1) << 4) | (selected_coeff->dac_div - 1);
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG05, reg05), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_CLK_MANAGER_REG06, &regv), TAG, "I2C read/write error");
    regv &= 0xE0;
    if (selected_coeff->bclk_div < 19) {
        regv |= (selected_coeff->bclk_div - 1) << 0;
    } else {
        regv |= (selected_coeff->bclk_div) << 0;
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG06, regv), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_CLK_MANAGER_REG07, &regv), TAG, "I2C read/write error");
    regv &= 0xC0;
    regv |= selected_coeff->lrck_h << 0;
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG07, regv), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG08, selected_coeff->lrck_l), TAG, "I2C read/write error");

    return ESP_OK;
}

static esp_err_t es8311_clock_config(es8311_handle_t dev, const es8311_clock_config_t *const clk_cfg, es8311_resolution_t res)
{
    uint8_t reg06;
    uint8_t reg01 = 0x3F;
    int mclk_hz;

    if (clk_cfg->mclk_from_mclk_pin) {
        mclk_hz = clk_cfg->mclk_frequency;
    } else {
        mclk_hz = clk_cfg->sample_frequency * (int)res * 2;
        reg01 |= BIT(7);
    }

    if (clk_cfg->mclk_inverted) {
        reg01 |= BIT(6);
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG01, reg01), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_CLK_MANAGER_REG06, &reg06), TAG, "I2C read/write error");
    if (clk_cfg->sclk_inverted) {
        reg06 |= BIT(5);
    } else {
        reg06 &= ~BIT(5);
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG06, reg06), TAG, "I2C read/write error");

    return es8311_sample_frequency_config(dev, mclk_hz, clk_cfg->sample_frequency);
}

static esp_err_t es8311_resolution_config(const es8311_resolution_t res, uint8_t *reg)
{
    switch (res) {
    case ES8311_RESOLUTION_16: *reg |= (3 << 2); break;
    case ES8311_RESOLUTION_18: *reg |= (2 << 2); break;
    case ES8311_RESOLUTION_20: *reg |= (1 << 2); break;
    case ES8311_RESOLUTION_24: *reg |= (0 << 2); break;
    case ES8311_RESOLUTION_32: *reg |= (4 << 2); break;
    default: return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t es8311_fmt_config(es8311_handle_t dev, const es8311_resolution_t res_in, const es8311_resolution_t res_out)
{
    uint8_t reg09 = 0;
    uint8_t reg0a = 0;
    uint8_t reg00;

    ESP_LOGI(TAG, "ES8311 in Slave mode and I2S format");
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_RESET_REG00, &reg00), TAG, "I2C read/write error");
    reg00 &= 0xBF;
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_RESET_REG00, reg00), TAG, "I2C read/write error");

    es8311_resolution_config(res_in, &reg09);
    es8311_resolution_config(res_out, &reg0a);

    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SDPIN_REG09, reg09), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SDPOUT_REG0A, reg0a), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t es8311_microphone_config(es8311_handle_t dev, bool digital_mic)
{
    uint8_t reg14 = 0x1A;
    if (digital_mic) {
        reg14 |= BIT(6);
    }
    es8311_write_reg(dev, ES8311_ADC_REG17, 0xC8);
    return es8311_write_reg(dev, ES8311_SYSTEM_REG14, reg14);
}

esp_err_t es8311_init(es8311_handle_t dev, const es8311_clock_config_t *const clk_cfg, const es8311_resolution_t res_in, const es8311_resolution_t res_out)
{
    ESP_RETURN_ON_FALSE((clk_cfg->sample_frequency >= 8000) && (clk_cfg->sample_frequency <= 96000), ESP_ERR_INVALID_ARG, TAG, "Frequency Error");
    if (!clk_cfg->mclk_from_mclk_pin) {
        ESP_RETURN_ON_FALSE(res_out == res_in, ESP_ERR_INVALID_ARG, TAG, "Resolution IN/OUT mismatch");
    }

    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_RESET_REG00, 0x1F), TAG, "I2C error");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_RESET_REG00, 0x00), TAG, "I2C error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_RESET_REG00, 0x80), TAG, "I2C error");

    ESP_RETURN_ON_ERROR(es8311_clock_config(dev, clk_cfg, res_out), TAG, "");
    ESP_RETURN_ON_ERROR(es8311_fmt_config(dev, res_in, res_out), TAG, "");

    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG0D, 0x01), TAG, "I2C error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG0E, 0x02), TAG, "I2C error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG12, 0x00), TAG, "I2C error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG13, 0x10), TAG, "I2C error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG1C, 0x6A), TAG, "I2C error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_DAC_REG37, 0x08), TAG, "I2C error");

    return ESP_OK;
}

void es8311_delete(es8311_handle_t dev)
{
    es8311_dev_t *es = (es8311_dev_t *) dev;
    // Note: We do not delete the bus handle here, as it is shared.
    // We only free the specific device handle if the driver provided a way,
    // but IDF 5.x usually manages this via i2c_master_bus_rm_device.
    // For now, just freeing memory is safe enough for simple use.
    if (es->i2c_handle) {
         i2c_master_bus_rm_device(es->i2c_handle);
    }
    free(dev);
}

esp_err_t es8311_voice_volume_set(es8311_handle_t dev, int volume, int *volume_set)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    int reg32 = (volume == 0) ? 0 : ((volume * 256 / 100) - 1);
    if (volume_set != NULL) *volume_set = volume;
    return es8311_write_reg(dev, ES8311_DAC_REG32, reg32);
}

esp_err_t es8311_voice_volume_get(es8311_handle_t dev, int *volume)
{
    uint8_t reg32;
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_DAC_REG32, &reg32), TAG, "I2C error");
    *volume = (reg32 == 0) ? 0 : ((reg32 * 100) / 256) + 1;
    return ESP_OK;
}

esp_err_t es8311_voice_mute(es8311_handle_t dev, bool mute)
{
    uint8_t reg31;
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_DAC_REG31, &reg31), TAG, "I2C error");
    if (mute) reg31 |= BIT(6) | BIT(5);
    else reg31 &= ~(BIT(6) | BIT(5));
    return es8311_write_reg(dev, ES8311_DAC_REG31, reg31);
}

esp_err_t es8311_microphone_gain_set(es8311_handle_t dev, es8311_mic_gain_t gain_db)
{
    return es8311_write_reg(dev, ES8311_ADC_REG16, gain_db);
}

esp_err_t es8311_voice_fade(es8311_handle_t dev, const es8311_fade_t fade)
{
    uint8_t reg37;
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_DAC_REG37, &reg37), TAG, "I2C error");
    reg37 = (reg37 & 0x0F) | (fade << 4);
    return es8311_write_reg(dev, ES8311_DAC_REG37, reg37);
}

esp_err_t es8311_microphone_fade(es8311_handle_t dev, const es8311_fade_t fade)
{
    uint8_t reg15;
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_ADC_REG15, &reg15), TAG, "I2C error");
    reg15 = (reg15 & 0x0F) | (fade << 4);
    return es8311_write_reg(dev, ES8311_ADC_REG15, reg15);
}

void es8311_register_dump(es8311_handle_t dev)
{
    for (int reg = 0; reg < 0x4A; reg++) {
        uint8_t value;
        if (es8311_read_reg(dev, reg, &value) == ESP_OK) {
            printf("REG:%02x: %02x\n", reg, value);
        }
    }
}

// --- NEW CREATE FUNCTION ---
es8311_handle_t es8311_create(i2c_master_bus_handle_t bus_handle, uint16_t dev_addr)
{
    es8311_dev_t *sensor = (es8311_dev_t *) calloc(1, sizeof(es8311_dev_t));
    
    // Configure the Device (Speed, Address)
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 100000,
    };

    // Add this device to the existing Bus Handle
    // This creates the new "Device Handle" needed for transmit/receive
    if (i2c_master_bus_add_device(bus_handle, &dev_cfg, &sensor->i2c_handle) != ESP_OK) {
        free(sensor);
        return NULL;
    }

    return (es8311_handle_t) sensor;
}
