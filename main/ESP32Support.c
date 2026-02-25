//
//  ESP32Support.c
//  
//
//  Created by Chris Galzerano on 2/7/26.
//

#include "ESP32Support.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "FilesApp.h"
#include "ClockApp.h"
#include "PaintApp.h"

wl_handle_t wl_handle = WL_INVALID_HANDLE;

// Pin Definitions SPI LCD

#define PIN_NUM_MOSI        11
#define PIN_NUM_CLK         12
#define PIN_NUM_CS          10
#define PIN_NUM_DC           9
#define PIN_NUM_RST          8
#define PIN_NUM_BK_LIGHT     -1 // Use a real GPIO if you have a backlight control pin

// --- NEW, CORRECTED PIN CONFIGURATION ---
#define I2C_HOST           I2C_NUM_0
#define PIN_NUM_TOUCH_RST  4   // Your RST pin
#define PIN_NUM_TOUCH_INT  5   // Your INT pin
#define PIN_NUM_I2C_SDA    6   // Your SDA pin
#define PIN_NUM_I2C_SCL    7   // Your SCL pin




// --- Configuration for ILI9488 ---
#if defined(USE_ILI9488)
#define LCD_PIXEL_CLOCK_HZ (30 * 1000 * 1000)
#define LCD_H_RES          320
#define LCD_V_RES          480
#define LCD_COLOR_SPACE    ESP_LCD_COLOR_SPACE_RGB // ILI9488 is typically RGB
#define LCD_DRIVER_FN()    esp_lcd_new_panel_ili9488(io_handle, &panel_config, &panel_handle)

//RGB Display Pinout

#define LCD_COLOR_BYTES_PER_PIXEL 2 // Using 16-bit RGB565 for simpler pinout
#define LCD_INIT_BUFFER_SIZE 2048 // 2KB is usually more than enough for init commands
#define FRAME_BUFFER_SIZE (LCD_H_RES * LCD_V_RES * 2)

// --- GPIO PIN DEFINITIONS ---

// 1. Pins Freed for I2S/SDIO (DO NOT USE THESE BELOW)
// I2S: GPIO 40, 41, 42 | SDIO: GPIO 1, 5, 6, 7, 8, 9
// We will define them later, but they are reserved here.

// --- 1. I2S Audio Pin Definitions (FINAL MOVE) ---
#define I2S_PIN_NUM_BCLK      1
#define I2S_PIN_NUM_LRCK      2
#define I2S_PIN_NUM_DOUT      46      // Moved to 46 to free up 3 and 4

// --- 2. SD Card (SDIO 4-bit) Pin Definitions ---
#define SDIO_PIN_NUM_CLK      5
#define SDIO_PIN_NUM_CMD      6
#define SDIO_PIN_NUM_D0       7
#define SDIO_PIN_NUM_D1       8
#define SDIO_PIN_NUM_D2       9
#define SDIO_PIN_NUM_D3       10

// --- 3. SPI Pins for ILI9488 Initialization ---
#define LCD_PIN_NUM_SPI_CS    11
#define LCD_PIN_NUM_SPI_SCLK  12
#define LCD_PIN_NUM_SPI_MOSI  13
#define LCD_PIN_NUM_SPI_DC    10      // Safest choice for Data/Command

// --- 4. General Control Pin Definitions ---
#define LCD_PIN_NUM_RST       14
#define LCD_PIN_NUM_BK_LIGHT  15

// --- 5. RGB Interface Pins for DISPLAY (SYNC LINES) ---
#define LCD_PIN_NUM_PCLK      1
#define LCD_PIN_NUM_HSYNC     0
#define LCD_PIN_NUM_VSYNC     45
#define LCD_PIN_NUM_DE        47

// --- 6. RGB Data Bus (D0-D15) - FINAL, VERIFIED PINS ---
// Low Byte (D0-D7)
#define LCD_PIN_NUM_D0        20
#define LCD_PIN_NUM_D1        21
#define LCD_PIN_NUM_D2        16      // Remapped from 23
#define LCD_PIN_NUM_D3        17      // Remapped from 25
#define LCD_PIN_NUM_D4        18      // Remapped from 26
#define LCD_PIN_NUM_D5        19      // Remapped from 27
#define LCD_PIN_NUM_D6        4       // Remapped from 33 (New location!)
#define LCD_PIN_NUM_D7        3       // Remapped from 34 (New location!)

// High Byte (D8-D15)
#define LCD_PIN_NUM_D8        7
#define LCD_PIN_NUM_D9        8
#define LCD_PIN_NUM_D10       9
#define LCD_PIN_NUM_D11       38
#define LCD_PIN_NUM_D12       39
#define LCD_PIN_NUM_D13       40
#define LCD_PIN_NUM_D14       41
#define LCD_PIN_NUM_D15       42
// --- 6. RGB Data Bus (D16, D17) - NEW 18-BIT LINES ---
#define LCD_PIN_NUM_D16       5       // LCD Pin: D16
#define LCD_PIN_NUM_D17       6       // LCD Pin: D17

// NOTE: Please carefully check your specific ESP32-S3 module's pinout.
// GPIOs 22, 24, and 28-32 are often not available. The example above is
// adjusted to skip these unavailable pins, creating the 16-pin bus.

// IMPORTANT: LCD timing parameters for ILI9488
// These are typical values, but you MUST verify them with your datasheet.
#define LCD_PCLK_HZ          (6 * 1000 * 1000) // 6MHz for 480x320
// Increase the timing margins (Porches)
#define LCD_HSYNC_PULSE_WIDTH       5
#define LCD_HSYNC_BACK_PORCH        20 // Increased from 8
#define LCD_HSYNC_FRONT_PORCH       20 // Increased from 10

#define LCD_VSYNC_PULSE_WIDTH       5
#define LCD_VSYNC_BACK_PORCH        20 // Increased from 8
#define LCD_VSYNC_FRONT_PORCH       20 // Increased from 10

// --- Global Variable Definitions ---

int32_t g_last_x = 0;
int32_t g_last_y = 0;
lv_indev_state_t g_last_state = LV_INDEV_STATE_REL; // Start as Released
SemaphoreHandle_t g_touch_mutex = NULL; // Initialize to NULL or create in init()

const int DISPLAY_HORIZONTAL_PIXELS = 320;
const int DISPLAY_VERTICAL_PIXELS = 480;
const int DISPLAY_COMMAND_BITS = 8;
const int DISPLAY_PARAMETER_BITS = 8;
const unsigned int DISPLAY_REFRESH_HZ = 40000000;
const int DISPLAY_SPI_QUEUE_LEN = 10;
const int SPI_MAX_TRANSFER_SIZE = 32768;

// Note: You can switch this logic based on your build flags as before
const lcd_rgb_element_order_t TFT_COLOR_MODE = COLOR_RGB_ELEMENT_ORDER_RGB;

// Default to 25 lines of color data
const size_t LV_BUFFER_SIZE = 320 * 25; // DISPLAY_HORIZONTAL_PIXELS * 25
const int LVGL_UPDATE_PERIOD_MS = 5;

/*
const ledc_mode_t BACKLIGHT_LEDC_MODE = LEDC_LOW_SPEED_MODE;
const ledc_channel_t BACKLIGHT_LEDC_CHANNEL = LEDC_CHANNEL_0;
const ledc_timer_t BACKLIGHT_LEDC_TIMER = LEDC_TIMER_1;
const ledc_timer_bit_t BACKLIGHT_LEDC_TIMER_RESOLUTION = LEDC_TIMER_10_BIT;
const uint32_t BACKLIGHT_LEDC_FRQUENCY = 5000;
*/

esp_lcd_panel_io_handle_t lcd_io_handle = NULL;
esp_lcd_panel_handle_t lcd_handle = NULL;
lv_draw_buf_t lv_disp_buf;
lv_display_t *lv_display = NULL;
lv_color_t *lv_buf_1 = NULL;
lv_color_t *lv_buf_2 = NULL;
lv_obj_t *meter = NULL;
lv_style_t style_screen;


// Place this outside of app_main
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    // Ensure DC is LOW for command (0)
    gpio_set_level(LCD_PIN_NUM_SPI_DC, 0);
    
    // Create the transaction configuration
    spi_transaction_t t = {
        .length = 8, // 8 bits long
        .tx_buffer = &cmd,
    };
    // Send the command
    spi_device_transmit(spi, &t);
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    if (len == 0) return;
    // Ensure DC is HIGH for data (1)
    gpio_set_level(LCD_PIN_NUM_SPI_DC, 1);
    
    // Create the transaction configuration
    spi_transaction_t t = {
        .length = len * 8, // length is in bits
        .tx_buffer = data,
    };
    // Send the data
    spi_device_transmit(spi, &t);
}

// Function to send a single command followed by a single parameter
void ili9488_set_colmod(spi_device_handle_t spi)
{
    const uint8_t cmd_colmod = 0x3A; // Command to set pixel format
    const uint8_t param_rgb565 = 0x55; // Parameter for 16-bit (RGB565)

    // 1. Send the command (DC=0)
    lcd_cmd(spi, cmd_colmod);
    
    // 2. Send the parameter (DC=1)
    lcd_data(spi, &param_rgb565, 1);
}

// --- ILI9488 Command Definitions (MUST be visible to the array) ---

#define ILI9488_SWRESET      0x01
#define ILI9488_SLPOUT       0x11
#define ILI9488_DISPON       0x29
#define ILI9488_PIXFMT       0x3A

#define ILI9488_FRMCTR1      0xB1
#define ILI9488_INVCTR       0xB4
#define ILI9488_DFUNCTR      0xB6

#define ILI9488_PWCTR1       0xC0
#define ILI9488_PWCTR2       0xC1
#define ILI9488_VMCTR1       0xC5

#define ILI9488_GMCTRP1      0xE0
#define ILI9488_GMCTRN1      0xE1
#define ILI9488_IMGFUNCT     0xE9

#define ILI9488_ADJCTR3      0xF7

// Commands used in your array (from the STM32 source):
#define ILI9488_GMCTRP1      0xE0
#define ILI9488_GMCTRN1      0xE1
#define ILI9488_PWCTR1       0xC0
#define ILI9488_PWCTR2       0xC1
#define ILI9488_VMCTR1       0xC5
#define ILI9488_PIXFMT       0x3A
#define ILI9488_FRMCTR1      0xB1
#define ILI9488_INVCTR       0xB4
#define ILI9488_DFUNCTR      0xB6
#define ILI9488_IMGFUNCT     0xE9
#define ILI9488_ADJCTR3      0xF7
#define ILI9488_SLPOUT       0x11
#define ILI9488_DISPON       0x29
#define ILI9488_SWRESET      0x01

// Adapt the command list from the STM32 driver (ili9488_Init function)
static const uint8_t ili9488_rgb_init_sequence[][20] = {
    // Command, Parameter1, Parameter2, ...
    { ILI9488_SWRESET, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0x01: SWRESET (No params, but needs delay)
    { ILI9488_GMCTRP1, 0x00, 0x01, 0x02, 0x04, 0x14, 0x09, 0x3F, 0x57, 0x4D, 0x05, 0x0B, 0x09, 0x1A, 0x1D, 0x0F }, // 0xE0: Positive Gamma Control (15 params)
    { ILI9488_GMCTRN1, 0x00, 0x1D, 0x20, 0x02, 0x0E, 0x03, 0x35, 0x12, 0x47, 0x02, 0x0D, 0x0C, 0x38, 0x39, 0x0F }, // 0xE1: Negative Gamma Control (15 params)
    { ILI9488_PWCTR1, 0x17, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0xC0: Power Control 1 (2 params)
    { ILI9488_PWCTR2, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0xC1: Power Control 2 (1 param)
    { ILI9488_VMCTR1, 0x00, 0x12, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0xC5: VCOM Control (3 params)
    
    // *** CRITICAL CHANGE ***
    // 0x3A: PIXFMT. Must be 0x55 for 16-bit RGB565.
    { ILI9488_PIXFMT, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0x3A: Interface Pixel Format (1 param)

    // 0xB0: Interface Mode Control. We need to set the parallel mode bits, which is NOT 0x80/0x00.
    // The ILI9488 requires different commands for DPI (Parallel RGB) vs. MCU (8080/DPI) mode.
    // We will use 0x02 (which enables the DPI) or 0x03 (for 16-bit DPI). This depends on hardware. Let's try 0x00 (the standard reset state)
    // and rely on the RGB panel to set the mode, but it might need 0x02.
    // Since the driver failed to set this, let's skip it and focus on the power-up sequence.

    { ILI9488_FRMCTR1, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0xB1: Frame rate (1 param)
    { ILI9488_INVCTR, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0xB4: Display Inversion Control (1 param)
    { ILI9488_DFUNCTR, 0x02, 0x02, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0xB6: Display Function Control (3 params)
    { ILI9488_IMGFUNCT, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0xE9: Set Image Function (1 param)
    { ILI9488_ADJCTR3, 0xA9, 0x51, 0x2C, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0xF7: Adjust Control (4 params)
    
    // **Final Wake-up**
    { ILI9488_SLPOUT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0x11: Exit Sleep (Needs 120ms delay)
    { ILI9488_DISPON, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0x29: Display ON (Needs 5ms delay)
};



void initRgbDisplay(void) {
    // --- Part 1: Initialize Backlight ---
        FreeOSLogI(TAG, "Initializing LCD backlight on GPIO %d...", LCD_PIN_NUM_BK_LIGHT);
        // ... (Backlight setup code remains the same)
        gpio_config_t bk_gpio_conf = {
            .pin_bit_mask = 1ULL << LCD_PIN_NUM_BK_LIGHT,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_conf));
        gpio_set_level(LCD_PIN_NUM_BK_LIGHT, 1);

        // --- Part 2: Initialize ILI9488 via temporary SPI bus ---
        FreeOSLogI(TAG, "Installing temporary SPI bus for ILI9488 init...");
        spi_bus_config_t buscfg = {
            .mosi_io_num = LCD_PIN_NUM_SPI_MOSI,
            .sclk_io_num = LCD_PIN_NUM_SPI_SCLK,
            .miso_io_num = -1,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = LCD_H_RES * 10, // Small transfer size for commands
        };
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = LCD_PIN_NUM_SPI_CS,
            .dc_gpio_num = LCD_PIN_NUM_SPI_DC,
            .spi_mode = 0,
            .pclk_hz = 10 * 1000 * 1000,
            .trans_queue_depth = 10,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    FreeOSLogI(TAG, "Installing ILI9488 driver over SPI...");
        esp_lcd_panel_handle_t init_panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_PIN_NUM_RST,
            .bits_per_pixel = 16,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io_handle,
                                                  &panel_config,
                                                  LCD_INIT_BUFFER_SIZE, // <--- NEW ARGUMENT
                                                  &init_panel_handle));

        FreeOSLogI(TAG, "Resetting and Initializing ILI9488...");
        ESP_ERROR_CHECK(esp_lcd_panel_reset(init_panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(init_panel_handle));

        // Optional: Set the ILI9488 to accept RGB565 (16-bit) data
        // Command 0x3A is COLMOD (Pixel Format). 0x55 is 16-bit (RGB565).
        esp_lcd_panel_io_tx_param(io_handle, 0x3A, (uint8_t[]){0x55}, 1);

        // Disable SPI interface
        FreeOSLogI(TAG, "Initialization complete. Deleting SPI handles...");
        ESP_ERROR_CHECK(esp_lcd_panel_del(init_panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_io_del(io_handle));
        ESP_ERROR_CHECK(spi_bus_free(SPI2_HOST));


        // --- Part 3: Install the permanent RGB interface ---
        FreeOSLogI(TAG, "Installing RGB panel driver for ILI9488...");
        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .disp_gpio_num = -1,
            .psram_trans_align = 64,
            .data_width = 16, // Use 16-bit bus
            .bits_per_pixel = 16,
            .de_gpio_num = LCD_PIN_NUM_DE,
            .pclk_gpio_num = LCD_PIN_NUM_PCLK,
            .vsync_gpio_num = LCD_PIN_NUM_VSYNC,
            .hsync_gpio_num = LCD_PIN_NUM_HSYNC,
            .data_gpio_nums = {
                LCD_PIN_NUM_D0, LCD_PIN_NUM_D1, LCD_PIN_NUM_D2, LCD_PIN_NUM_D3,
                LCD_PIN_NUM_D4, LCD_PIN_NUM_D5, LCD_PIN_NUM_D6, LCD_PIN_NUM_D7,
                LCD_PIN_NUM_D8, LCD_PIN_NUM_D9, LCD_PIN_NUM_D10, LCD_PIN_NUM_D11,
                LCD_PIN_NUM_D12, LCD_PIN_NUM_D13, LCD_PIN_NUM_D14, LCD_PIN_NUM_D15,
            },
            .timings = {
                .pclk_hz = LCD_PCLK_HZ,
                .h_res = LCD_H_RES,
                .v_res = LCD_V_RES,
                .flags.pclk_active_neg = true,  // TRY THIS AGAIN! Inverting the clock edge
                .flags.vsync_idle_low = false,  // Standard for VSYNC (try swapping if needed)
                .flags.hsync_idle_low = false,  // Standard for HSYNC (try swapping if needed)
                .hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH,
                .hsync_back_porch = LCD_HSYNC_BACK_PORCH,
                .hsync_front_porch = LCD_HSYNC_FRONT_PORCH,
                .vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH,
                .vsync_back_porch = LCD_VSYNC_BACK_PORCH,
                .vsync_front_porch = LCD_VSYNC_FRONT_PORCH,
            },
            .flags.fb_in_psram = true,
        };
        esp_lcd_panel_handle_t rgb_panel_handle = NULL;
        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &rgb_panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(rgb_panel_handle));
        // Set the ILI9488 to accept RGB565 (16-bit) data
        // Command 0x3A is COLMOD (Pixel Format). 0x55 is 16-bit (RGB565).
        //esp_lcd_panel_io_tx_param(io_handle, 0x3A, (uint8_t[]){0x55}, 1);
        //ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x29, NULL, 0));
    
    // Inside app_main, before calling esp_lcd_new_panel_ili9488:


    /// --- Part 2: Initialize ILI9488 via temporary SPI bus (Corrected Order) ---
    
    // 1. Define the physical bus GPIOs
    spi_bus_config_t buscfg1 = {
        .mosi_io_num = LCD_PIN_NUM_SPI_MOSI,
        .sclk_io_num = LCD_PIN_NUM_SPI_SCLK,
        .miso_io_num = -1, // Not needed for write-only
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024,
    };
    
    // Line 1: Initialize the physical bus and pins (This was missing or misplaced!)
    FreeOSLogI(TAG, "Initializing SPI bus host...");
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg1, SPI_DMA_CH_AUTO));

    // 2. Define the device configuration
    spi_device_handle_t raw_spi_handle = NULL;
    spi_device_interface_config_t devcfg={
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = LCD_PIN_NUM_SPI_CS,
        .queue_size = 7,
        .pre_cb = NULL,
    };
    
    // Line 2: Add the device to the now-initialized bus
    FreeOSLogI(TAG, "Adding ILI9488 device to bus...");
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &raw_spi_handle)); // Your crash line (now fixed)
    
    // 3. Manually send the required commands
    // Ensure the DC pin (GPIO 8) is configured as output before this line!
    gpio_set_direction(LCD_PIN_NUM_SPI_DC, GPIO_MODE_OUTPUT);
    
    FreeOSLogI(TAG, "Manually setting 16-bit color format...");
    ili9488_set_colmod(raw_spi_handle); // This command should now execute!

    // ... continue with esp_lcd_new_panel_ili9488 call, etc.

        // Manually run the hardware reset (already working, but cleaner here)
        // Assuming GPIO 14 is your reset pin
        gpio_set_level(LCD_PIN_NUM_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(LCD_PIN_NUM_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(120));

        FreeOSLogI(TAG, "Starting full ILI9488 init sequence...");

        for (int i = 0; i < sizeof(ili9488_rgb_init_sequence) / sizeof(ili9488_rgb_init_sequence[0]); i++) {
            uint8_t cmd = ili9488_rgb_init_sequence[i][0];
            const uint8_t *data = &ili9488_rgb_init_sequence[i][1];
            // Calculate length based on known commands or hardcoding
            int len = 0;
            if (cmd == ILI9488_GMCTRP1 || cmd == ILI9488_GMCTRN1) len = 15;
            else if (cmd == ILI9488_PWCTR1) len = 2;
            else if (cmd == ILI9488_VMCTR1) len = 3;
            else if (cmd == ILI9488_ADJCTR3) len = 4;
            else if (cmd == ILI9488_DFUNCTR) len = 2; // Adjusted from 3 to 2 based on pattern
            else if (cmd == ILI9488_PWCTR2 || cmd == ILI9488_PIXFMT || cmd == ILI9488_FRMCTR1 || cmd == ILI9488_INVCTR || cmd == ILI9488_IMGFUNCT) len = 1;

            if (cmd == ILI9488_SWRESET) {
                lcd_cmd(raw_spi_handle, cmd);
                vTaskDelay(pdMS_TO_TICKS(5));
            } else if (cmd == ILI9488_SLPOUT) {
                lcd_cmd(raw_spi_handle, cmd);
                vTaskDelay(pdMS_TO_TICKS(120));
            } else if (cmd == ILI9488_DISPON) {
                lcd_cmd(raw_spi_handle, cmd);
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                lcd_cmd(raw_spi_handle, cmd);
                if (len > 0) {
                    lcd_data(raw_spi_handle, data, len);
                }
            }
        }

        FreeOSLogI(TAG, "ILI9488 is now configured.");

    // ... (Proceed to delete SPI handles and initialize esp_lcd_new_panel_rgb) ...
    
        // --- Part 4: Draw to the "live" framebuffer ---
        uint16_t *framebuffer1 = NULL;
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(rgb_panel_handle, 1, (void **)&framebuffer1));

        FreeOSLogI(TAG, "Drawing solid green to the %dx%d framebuffer...", LCD_H_RES, LCD_V_RES);
        uint16_t green_color = 0x07E0; // RGB565: 00000 111111 00000 (Pure Green)
        for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
            framebuffer1[i] = green_color;
        }
        FreeOSLogI(TAG, "Display should be green and refreshed by hardware.");
}

// Add this near the top of your file
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);



/**
 * @brief DMA callback for esp_lcd.
 * This function is called from an ISR when the SPI transfer is done.
 */
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    // user_ctx is the address of the global lv_display pointer (&lv_display)
    lv_display_t **disp_ptr = (lv_display_t **)user_ctx;
    lv_display_t *disp = *disp_ptr;
    
    // Check if LVGL has created the display object yet
    if (disp) {
        // Tell LVGL that the buffer flushing is complete
        lv_display_flush_ready(disp);
    }
    
    // Return false, indicating no high-priority task was woken.
    return false;
}


static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // Change line 165 to this:
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    // At the very end, tell LVGL you are done
    lv_display_flush_ready(disp);
}

static void IRAM_ATTR lvgl_tick_cb(void *param)
{
    lv_tick_inc(LVGL_UPDATE_PERIOD_MS);
}

/*static void display_brightness_init(void)
 {
 const ledc_channel_config_t LCD_backlight_channel =
 {
 .gpio_num = GPIO_NUM_NC,
 .speed_mode = BACKLIGHT_LEDC_MODE,
 .channel = BACKLIGHT_LEDC_CHANNEL,
 .intr_type = LEDC_INTR_DISABLE,
 .timer_sel = BACKLIGHT_LEDC_TIMER,
 .duty = 0,
 .hpoint = 0,
 .flags =
 {
 .output_invert = 0
 }
 };
 const ledc_timer_config_t LCD_backlight_timer =
 {
 .speed_mode = BACKLIGHT_LEDC_MODE,
 .duty_resolution = BACKLIGHT_LEDC_TIMER_RESOLUTION,
 .timer_num = BACKLIGHT_LEDC_TIMER,
 .freq_hz = BACKLIGHT_LEDC_FRQUENCY,
 .clk_cfg = LEDC_AUTO_CLK
 };
 FreeOSLogI(TAG, "Initializing LEDC for backlight pin: %d", GPIO_NUM_NC);
 
 ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));
 ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));
 }
 
 void display_brightness_set(int brightness_percentage)
 {
 if (brightness_percentage > 100)
 {
 brightness_percentage = 100;
 }
 else if (brightness_percentage < 0)
 {
 brightness_percentage = 0;
 }
 FreeOSLogI(TAG, "Setting backlight to %d%%", brightness_percentage);
 
 // LEDC resolution set to 10bits, thus: 100% = 1023
 uint32_t duty_cycle = (1023 * brightness_percentage) / 100;
 ESP_ERROR_CHECK(ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty_cycle));
 ESP_ERROR_CHECK(ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL));
 }*/

void initialize_spi()
{
    FreeOSLogI(TAG, "Initializing SPI bus (MOSI:%d, MISO:%d, CLK:%d)",
             PIN_NUM_MOSI, GPIO_NUM_NC, PIN_NUM_CLK);
    spi_bus_config_t bus =
    {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .data4_io_num = GPIO_NUM_NC,
        .data5_io_num = GPIO_NUM_NC,
        .data6_io_num = GPIO_NUM_NC,
        .data7_io_num = GPIO_NUM_NC,
        .max_transfer_sz = SPI_MAX_TRANSFER_SIZE,
        .flags = SPICOMMON_BUSFLAG_SCLK |
        SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MASTER,
            .intr_flags = ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
}

// The global semaphore flag
SemaphoreHandle_t lcd_flush_ready_sem = NULL;

// The new FreeOS callback to replace the LVGL one
static bool lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    BaseType_t need_yield = pdFALSE;
    if (lcd_flush_ready_sem) {
        xSemaphoreGiveFromISR(lcd_flush_ready_sem, &need_yield);
    }
    return (need_yield == pdTRUE);
}

void initialize_display()
{
    
    if (lcd_flush_ready_sem == NULL) {
            lcd_flush_ready_sem = xSemaphoreCreateBinary();
            xSemaphoreGive(lcd_flush_ready_sem); // Initialize it as "ready"
        }
    
    
    const esp_lcd_panel_io_spi_config_t io_config =
    {
        .cs_gpio_num = PIN_NUM_CS,
        .dc_gpio_num = PIN_NUM_DC,
        .spi_mode = 0,
        .pclk_hz = DISPLAY_REFRESH_HZ,
        .trans_queue_depth = DISPLAY_SPI_QUEUE_LEN,
        .on_color_trans_done = lcd_trans_done_cb,
        .user_ctx = NULL,
        .lcd_cmd_bits = DISPLAY_COMMAND_BITS,
        .lcd_param_bits = DISPLAY_PARAMETER_BITS,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,4,0)
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
#endif
        .flags =
        {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
            .dc_as_cmd_phase = 0,
            .dc_low_on_data = 0,
            .octal_mode = 0,
            .lsb_first = 0
#else
                .dc_low_on_data = 0,
                .octal_mode = 0,
                .sio_mode = 0,
                .lsb_first = 0,
                .cs_high_active = 0
#endif
        }
    };
    
    const esp_lcd_panel_dev_config_t lcd_config =
    {
        .reset_gpio_num = PIN_NUM_RST,
        .color_space = TFT_COLOR_MODE,
        .bits_per_pixel = 18,
        .flags =
        {
            .reset_active_high = 0
        },
            .vendor_config = NULL
    };
    
    ESP_ERROR_CHECK(
                    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &lcd_io_handle));
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(lcd_io_handle, &lcd_config, LV_BUFFER_SIZE, &lcd_handle));
    
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd_handle, 0, 0));
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    ESP_ERROR_CHECK(esp_lcd_panel_disp_off(lcd_handle, false));
#else
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_handle, true));
#endif
}

void initialize_lvgl()
{
    FreeOSLogI(TAG, "Initializing LVGL");
    lv_init();
    
    FreeOSLogI(TAG, "Allocating LVGL draw buffers");
    lv_buf_1 = (lv_color_t *)heap_caps_malloc(LV_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(lv_buf_1);
#if USE_DOUBLE_BUFFERING
    lv_buf_2 = (lv_color_t *)heap_caps_malloc(LV_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(lv_buf_2);
#endif
    
    FreeOSLogI(TAG, "Creating LVGL display");
    // --- THIS IS THE NEW V9 DRIVER SETUP ---
    
    // 1. Create a display
    lv_display = lv_display_create(DISPLAY_HORIZONTAL_PIXELS, DISPLAY_VERTICAL_PIXELS);
    
    lv_display_set_buffers(lv_display,
                           lv_buf_1,
                           lv_buf_2,
                           LV_BUFFER_SIZE,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // 4. Set the flush callback
    lv_display_set_flush_cb(lv_display, lvgl_flush_cb);
    
    // 5. Pass your 'lcd_handle' (panel handle) to the display's user data
    lv_display_set_user_data(lv_display, lcd_handle);
    
    // --- END OF NEW V9 SETUP ---
    
    
    FreeOSLogI(TAG, "Creating LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args =
    {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_UPDATE_PERIOD_MS * 1000));
}

static void lvgl_task_handler(void *pvParameters)
{
    FreeOSLogI(TAG, "Starting LVGL task handler");
    while (1)
    {
        // Run the LVGL timer handler
        // This function will block for the appropriate amount of time
        // based on the timers you have set up in LVGL.
        lv_timer_handler();
        
        // Delay for a short period to yield to other tasks.
        // 10ms is a common value.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- Configuration for ST7789 ---
#elif defined(USE_ST7789)
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000) // You can use 40MHz for ST7789 too
#define LCD_H_RES          320
#define LCD_V_RES          240
#define LCD_COLOR_SPACE    ESP_LCD_COLOR_SPACE_BGR // ST7789 is often BGR
#define LCD_DRIVER_FN()    esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle)

// --- Error if no display is selected ---
#else
#error "No display driver selected. Please define USE_ILI9488 or USE_ST7789."
#endif


#define LCD_COLOR_RED       0xF800 // RGB565 code for Red

// Global panel handle (needed for the drawing function)
esp_lcd_panel_handle_t panel_handle = NULL;

static bool usingSt7789 = false;

//Touch

static esp_lcd_touch_handle_t tp_handle = NULL; // Touch panel handle
static lv_indev_t *lvgl_touch_indev = NULL;     // LVGL input device

/**
 * @brief LVGL input device "read" callback.
 * This function is called by LVGL periodically to get the current touch state.
 */
/*static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
 {
 // Get the touch handle stored in the input device's user_data
 esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
 
 // --- This is your code, now inside the callback ---
 uint16_t touch_x[1];
 uint16_t touch_y[1];
 uint16_t touch_strength[1];
 uint8_t touch_cnt = 0;
 
 // Poll the touch controller
 bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, touch_x, touch_y, touch_strength, &touch_cnt, 1);
 
 if (touchpad_pressed && touch_cnt > 0) {
 // A touch is detected
 data->state = LV_INDEV_STATE_PRESSED;
 data->point.x = touch_x[0];
 data->point.y = touch_y[0];
 } else {
 // No touch is detected
 data->state = LV_INDEV_STATE_RELEASED;
 }
 }*/

// --- Use the ESP32 pins from your code snippet ---
#define I2C_SDA_PIN 6
#define I2C_SCL_PIN 7
#define RST_N_PIN   4
#define INT_N_PIN   5

#define I2C_HOST I2C_NUM_0 // I2C port 0

// Global device handle
static ft6336u_dev_t touch_dev;
static i2c_master_bus_handle_t i2c_bus_handle;

// FreeRTOS queue to handle interrupt events
static QueueHandle_t touch_intr_queue = NULL;

/**
 * @brief Initialize the I2C master bus
 */
esp_err_t i2c_master_init(void) {
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_HOST,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .flags.enable_internal_pullup = true
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        FreeOSLogE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
    }
    return ret;
}


/*void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
 
 // --- LOCK ---
 xSemaphoreTake(g_touch_mutex, QUEUE_MAX_DELAY);
 
 // Read the shared variables
 FreeOSLogI(TAG, "lvgl_touch_read_cb");
 data->point.x = g_last_x;
 data->point.y = g_last_y;
 data->state = g_last_state;
 
 // --- UNLOCK ---
 xSemaphoreGive(g_touch_mutex);
 }*/

/*void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
 
 // Lock the mutex to check the state
 xSemaphoreTake(g_touch_mutex, QUEUE_MAX_DELAY);
 
 // --- This is the new logic ---
 
 // 1. Check if a "press" event is waiting for us
 if (g_last_state == LV_INDEV_STATE_PR) {
 
 // It is. Report the press to LVGL.
 data->point.x = g_last_x;
 data->point.y = g_last_y;
 data->state = LV_INDEV_STATE_PR;
 
 // 2. IMPORTANT: Consume the event
 // We immediately set the global state back to "released".
 // This ensures LVGL only sees "PRESSED" for a single frame.
 g_last_state = LV_INDEV_STATE_REL;
 
 } else {
 // No new press event. Just report "released".
 data->state = LV_INDEV_STATE_REL;
 }
 
 // --- Unlock the mutex ---
 xSemaphoreGive(g_touch_mutex);
 }*/


/*void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
 
 // --- LOCK THE MUTEX ---
 xSemaphoreTake(g_touch_mutex, QUEUE_MAX_DELAY);
 
 // --- THIS IS THE FIX ---
 
 // 1. Check if a "press" event is waiting for us
 if (g_last_state == LV_INDEV_STATE_PR) {
 
 // It is. Report the press to LVGL.
 data->point.x = g_last_x;
 data->point.y = g_last_y;
 data->state = LV_INDEV_STATE_PR;
 
 // 2. IMPORTANT: Consume the event
 // We immediately set the global state back to "released".
 // This ensures LVGL only sees "PRESSED" for a single frame.
 g_last_state = LV_INDEV_STATE_REL;
 
 } else {
 // No new press event. Just report "released".
 data->point.x = g_last_x; // Pass last-known coords
 data->point.y = g_last_y;
 data->state = LV_INDEV_STATE_REL;
 }
 
 // --- UNLOCK THE MUTEX ---
 xSemaphoreGive(g_touch_mutex);
 }*/

void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    
    // Lock the mutex
    xSemaphoreTake(g_touch_mutex, QUEUE_MAX_DELAY);
    
    // --- This is the fix ---
    
    // 1. Check if a "press" event is waiting for us
    if (g_last_state == LV_INDEV_STATE_PR) {
        
        // It is. Report the press to LVGL.
        data->point.x = g_last_x;
        data->point.y = g_last_y;
        data->state = LV_INDEV_STATE_PR;
        
        // 2. IMPORTANT: Consume the event
        // We immediately set the global state back to "released".
        g_last_state = LV_INDEV_STATE_REL;
        
    } else {
        // No new press event. Just report "released".
        data->point.x = g_last_x;
        data->point.y = g_last_y;
        data->state = LV_INDEV_STATE_REL;
    }
    
    // Unlock the mutex
    xSemaphoreGive(g_touch_mutex);
}



/*void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
 
 FreeOSLogI(TAG, "lvgl_touch_read_cb");
 
 FT6336U_TouchPointType touch_data;
 
 // 1. Scan the touch controller
 esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
 if (ret != ESP_OK) {
 FreeOSLogE(TAG, "Failed to scan touch: %s", esp_err_to_name(ret));
 // Keep last state if read fails
 data->state = LV_INDEV_STATE_REL;
 return;
 }
 
 // 2. Update LVGL with the new data
 if (touch_data.touch_count == 0) {
 data->state = LV_INDEV_STATE_REL; // Released
 } else {
 // Use the first touch point
 data->point.x = touch_data.tp[0].x;
 data->point.y = touch_data.tp[0].y;
 data->state = LV_INDEV_STATE_PR; // Pressed
 
 // Optional: Log if you still want to see coordinates
 // FreeOSLogI(TAG, "Touch 0: X=%u, Y=%u", data->point.x, data->point.y);
 }
 }*/

/*void updateTouch(void *pvParameter) {
 while (1) {
 FT6336U_TouchPointType touch_data;
 esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
 
 // --- LOCK ---
 xSemaphoreTake(g_touch_mutex, QUEUE_MAX_DELAY);
 
 if (ret != ESP_OK) {
 g_last_state = LV_INDEV_STATE_REL;
 } else {
 if (touch_data.touch_count == 0) {
 g_last_state = LV_INDEV_STATE_REL;
 } else {
 g_last_x = touch_data.tp[0].x;
 g_last_y = touch_data.tp[0].y;
 g_last_state = LV_INDEV_STATE_PR;
 }
 }
 
 // --- UNLOCK ---
 xSemaphoreGive(g_touch_mutex);
 
 vTaskDelay(pdMS_TO_TICKS(20));
 }
 
 
 uint32_t event;
 FT6336U_TouchPointType touch_data;
 
 while (1) {
 // Wait for an interrupt event from the queue
 if (xQueueReceive(touch_intr_queue, &event, QUEUE_MAX_DELAY)) {
 g_last_state = LV_INDEV_STATE_REL;
 // Interrupt received, scan the touch controller
 esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
 if (ret != ESP_OK) {
 FreeOSLogE(TAG, "Failed to scan touch: %s", esp_err_to_name(ret));
 g_last_state = LV_INDEV_STATE_REL;
 continue;
 }
 
 if (touch_data.touch_count == 0) {
 FreeOSLogI(TAG, "Touch Released");
 
 g_last_state = LV_INDEV_STATE_REL;
 } else {
 FreeOSLogI(TAG, "Touch Pressed");
 for (int i = 0; i < touch_data.touch_count; i++) {
 FreeOSLogI(TAG, "Touch %d: (%s) X=%u, Y=%u",
 i,
 (touch_data.tp[i].status == touch) ? "NEW" : "STREAM",
 touch_data.tp[i].x,
 touch_data.tp[i].y);
 }
 g_last_x = touch_data.tp[0].x;
 g_last_y = touch_data.tp[0].y;
 g_last_state = LV_INDEV_STATE_PR;
 
 }
 }
 }
 }*/



#define MP3_READ_BUF_SIZE 16*1024
#define PCM_BUF_SIZE 1152 * 2

// Volume (0-256)
static int current_volume = 256; // 50%

// --- AUDIO CONFIGURATION ---
#define SAMPLE_RATE     44100
#define WAVE_FREQ_HZ    440.0f
#define PI              3.14159265f

static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;

// --- HELPER: 16-bit Volume Control ---
void write_to_i2s_16bit(i2s_chan_handle_t handle, int16_t *pcm_samples, int sample_count) {
    size_t bytes_written;
    
    // Apply volume in place
    // We cast to int32_t temporarily to avoid overflow during multiplication
    for (int i = 0; i < sample_count; i++) {
        int32_t temp = (int32_t)pcm_samples[i] * current_volume;
        pcm_samples[i] = (int16_t)(temp >> 8); // Divide by 256 and cast back
    }

    // Write DIRECTLY to I2S (No 32-bit conversion needed)
    i2s_channel_write(handle, pcm_samples, sample_count * sizeof(int16_t), &bytes_written, QUEUE_MAX_DELAY);
}

void play_mp3_file(const char *filename, i2s_chan_handle_t tx_handle) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("Failed to open file: %s\n", filename);
        return;
    }

    // --- CHANGE: Allocate in SPIRAM (PSRAM) ---
    // This moves ~4KB + ~4KB out of internal RAM
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(MP3_READ_BUF_SIZE, MALLOC_CAP_SPIRAM);
    int16_t *pcm_buf = (int16_t *)heap_caps_malloc(PCM_BUF_SIZE * sizeof(int16_t), MALLOC_CAP_SPIRAM);

    if (!read_buf || !pcm_buf) {
        printf("Failed to allocate SPIRAM memory! Is PSRAM enabled in menuconfig?\n");
        if (read_buf) free(read_buf);
        if (pcm_buf) free(pcm_buf);
        fclose(f);
        return;
    }

    mp3dec_t mp3d;
    mp3dec_frame_info_t info;
    mp3dec_init(&mp3d);
    
    int bytes_in_buf = 0;
    int buf_offset = 0;

    while (1) {
        if (bytes_in_buf < MP3_READ_BUF_SIZE && !feof(f)) {
            if (bytes_in_buf > 0) {
                memmove(read_buf, read_buf + buf_offset, bytes_in_buf);
            }
            int bytes_read = fread(read_buf + bytes_in_buf, 1, MP3_READ_BUF_SIZE - bytes_in_buf, f);
            bytes_in_buf += bytes_read;
            buf_offset = 0;
            if (bytes_read == 0 && bytes_in_buf == 0) break;
        }

        int samples = mp3dec_decode_frame(&mp3d, read_buf + buf_offset, bytes_in_buf, pcm_buf, &info);
        
        buf_offset += info.frame_bytes;
        bytes_in_buf -= info.frame_bytes;

        if (samples > 0) {
            // Send 16-bit data
            write_to_i2s_16bit(tx_handle, pcm_buf, samples * info.channels);
        } else {
            if (bytes_in_buf > 0 && info.frame_bytes == 0) {
                buf_offset++;
                bytes_in_buf--;
            }
        }
    }

    free(read_buf);
    free(pcm_buf);
    fclose(f);
    printf("Playback finished.\n");
}

void mp3_task_wrapper(void *arg) {
    // We can pass the handle via valid logic, or just make tx_handle global
    play_mp3_file("/sdcard/frair.mp3", tx_handle);
    
    // Delete this task when song finishes so we don't crash
    vTaskDelete(NULL);
}

typedef struct {
    char riff_tag[4];       // "RIFF"
    uint32_t riff_len;      // Total file size - 8
    char wave_tag[4];       // "WAVE"
    char fmt_tag[4];        // "fmt "
    uint32_t fmt_len;       // 16 for PCM
    uint16_t audio_format;  // 1 for PCM
    uint16_t num_channels;  // 1 for Mono, 2 for Stereo
    uint32_t sample_rate;   // e.g., 44100
    uint32_t byte_rate;     // sample_rate * channels * (bit_depth/8)
    uint16_t block_align;   // channels * (bit_depth/8)
    uint16_t bits_per_sample; // 16
    char data_tag[4];       // "data"
    uint32_t data_len;      // Total bytes of audio data
} wav_header_t;

// Function to generate the header
void write_wav_header(FILE *f, int sample_rate, int channels, int total_data_len) {
    wav_header_t header;

    // RIFF Chunk
    memcpy(header.riff_tag, "RIFF", 4);
    header.riff_len = total_data_len + 36; // File size - 8
    memcpy(header.wave_tag, "WAVE", 4);

    // fmt Chunk
    memcpy(header.fmt_tag, "fmt ", 4);
    header.fmt_len = 16;
    header.audio_format = 1; // PCM
    header.num_channels = channels;
    header.sample_rate = sample_rate;
    header.bits_per_sample = 16;
    header.byte_rate = sample_rate * channels * (16 / 8);
    header.block_align = channels * (16 / 8);

    // data Chunk
    memcpy(header.data_tag, "data", 4);
    header.data_len = total_data_len;

    // Write header to beginning of file
    fseek(f, 0, SEEK_SET); // Jump to start
    fwrite(&header, sizeof(wav_header_t), 1, f);
    fseek(f, 0, SEEK_END); // Jump back to end
}

#define RECORD_TIME_SEC 10
#define SAMPLE_RATE 44100
#define CHANNELS 2      // We use Stereo mode in I2S, even if mic is mono
#define BIT_DEPTH 16

// Control Flags
static volatile bool is_recording_active = false;
static TaskHandle_t record_task_handle = NULL;

// Make sure your I2S RX Handle is global so the task can see it
// (Define this near your setupHybridAudio function)
extern i2s_chan_handle_t rx_handle;

void record_wav_task(void *args) {
    FreeOSLogI("REC", "Opening file for recording...");

    // 1. Open File
    FILE *f = fopen("/sdcard/recording.wav", "wb");
    if (f == NULL) {
        FreeOSLogE("REC", "Failed to open file!");
        is_recording_active = false;
        record_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 2. Write Placeholder Header
    wav_header_t dummy_header = {0};
    fwrite(&dummy_header, sizeof(wav_header_t), 1, f);

    // 3. Allocate Buffer
    size_t chunk_size = 4096;
    char *buffer = cc_safe_alloc(1, chunk_size);
    size_t bytes_read = 0;
    size_t total_bytes_recorded = 0;

    // 4. The Loop (Runs while is_recording_active is TRUE)
    while (is_recording_active) {
        // Read audio from I2S
        if (i2s_channel_read(rx_handle, buffer, chunk_size, &bytes_read, 100) == ESP_OK) {
            // Write audio to SD Card
            if (bytes_read > 0) {
                fwrite(buffer, 1, bytes_read, f);
                total_bytes_recorded += bytes_read;
            }
        } else {
            // Optional: Short delay to prevent watchdog starvation if I2S fails
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    // 5. Cleanup (The "Graceful" part)
    FreeOSLogI("REC", "Stopping... Finalizing WAV Header.");

    // Update the header with the correct file size
    write_wav_header(f, 44100, 2, total_bytes_recorded);

    fclose(f);
    free(buffer);
    
    FreeOSLogI("REC", "File saved. Task deleting.");
    record_task_handle = NULL; // Reset handle
    vTaskDelete(NULL);
}

void start_recording(void) {
    if (is_recording_active) {
        ESP_LOGW("REC", "Already recording!");
        return;
    }
    
    if (rx_handle == NULL) {
        FreeOSLogE("REC", "I2S RX Handle is NULL! Did you run setupHybridAudio?");
        return;
    }

    is_recording_active = true;
    
    FreeOSLogI("REC", "rec_task");
    
    // Create the task (Pinned to Core 1 is usually best for file I/O)
    xTaskCreatePinnedToCore(record_wav_task, "rec_task", 1024 * 6, NULL, 5, &record_task_handle, 1);
}

void stop_recording(void) {
    if (!is_recording_active) {
        return;
    }

    FreeOSLogI("REC", "Stop requested...");
    // Flip the switch. The task will exit its while() loop and clean up.
    is_recording_active = false;
}

void record_task(void *args) {
    // 1. Open File
    FILE *f = fopen("/sdcard/recording.wav", "wb");
    if (f == NULL) {
        FreeOSLogE("REC", "Failed to open file for writing");
        vTaskDelete(NULL);
    }

    // 2. Write Dummy Header (We will fix the sizes later)
    wav_header_t dummy_header = {0};
    fwrite(&dummy_header, sizeof(wav_header_t), 1, f);

    // 3. Allocation
    size_t bytes_read = 0;
    // Buffer size: 1024 samples * 2 channels * 2 bytes = 4096 bytes
    size_t chunk_size = 4096;
    char *buffer = cc_safe_alloc(1, chunk_size);
    
    int total_bytes_recorded = 0;
    int expected_bytes = RECORD_TIME_SEC * SAMPLE_RATE * CHANNELS * (BIT_DEPTH/8);

    FreeOSLogI("REC", "Start Recording...");

    // 4. Recording Loop
    while (total_bytes_recorded < expected_bytes) {
        // Read from Microphone (I2S RX Handle)
        // Note: Use 'rx_handle' here, NOT tx_handle!
        if (i2s_channel_read(rx_handle, buffer, chunk_size, &bytes_read, 1000) == ESP_OK) {
            
            // Write Raw Data to SD Card
            fwrite(buffer, 1, bytes_read, f);
            total_bytes_recorded += bytes_read;
        }
    }

    FreeOSLogI("REC", "Recording Complete. Finalizing...");

    // 5. Finalize Header
    // Go back to the top of the file and write the correct lengths
    write_wav_header(f, SAMPLE_RATE, CHANNELS, total_bytes_recorded);

    // 6. Cleanup
    fclose(f);
    free(buffer);
    vTaskDelete(NULL);
}



void turn_off_wifi_and_free_ram(void) {
    ESP_LOGW("WIFI", "Shutting down Wi-Fi to reclaim RAM...");

    // 1. Stop the Wi-Fi Driver (Turns off Radio, saves power)
    // Memory is STILL allocated here.
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW("WIFI", "Wi-Fi was not initialized, nothing to do.");
        return;
    }
    ESP_ERROR_CHECK(err);

    // 2. De-Initialize the Driver (The Magic Step)
    // This effectively "un-mallocs" the 60KB+ of Internal RAM.
    ESP_ERROR_CHECK(esp_wifi_deinit());

    // Optional: Destroy the default pointers if you created them
    // (This cleans up small remaining handles, but deinit does the heavy lifting)
    // esp_netif_destroy_default_wifi(your_netif_handle);

    ESP_LOGW("WIFI", "Wi-Fi De-initialized. RAM should be back.");
    
    // Prove it
    FreeOSLogI("MEM", "Free Internal Heap: %ld bytes", (long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

// 1. One-Time Initialization
// Call this inside app_main() BEFORE you show any UI!
void init_wifi_stack_once(void) {
    checkAvailableMemory();
    if (wifi_initialized) return; // Prevention

    // Initialize NVS (Required for WiFi)
   /* esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);*/

    // Initialize Network Stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    // 1. Slash the RX/TX Buffers (The biggest RAM savers)
    // Default is usually 32. We drop to 4-8.
    // Note: This might reduce max throughput speed, but fixes the crash.
    cfg.dynamic_rx_buf_num = 4;  // Minimum safe value
    cfg.cache_tx_buf_num = 4;    // Minimum safe value

    // 2. Cripple the AMPDU (Aggregation)
    // AMPDU is "High Speed Mode". We don't need 100mbps for audio.
    cfg.ampdu_rx_enable = 0;     // Disable completely if possible, or set extremely low
    cfg.ampdu_tx_enable = 0;     // Disable completely

    // 3. Force "nvs" off (Optional, saves a tiny bit of RAM/Flash ops)
    cfg.nvs_enable = 0;
    
    
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    esp_wifi_set_ps(WIFI_PS_NONE);

    wifi_initialized = true;
    printf("WiFi Stack Initialized.\n");
    checkAvailableMemory();
}

void trigger_wifi_scan(void) {
    if (!wifi_initialized) {
        printf("Error: WiFi not initialized! Call init_wifi_stack_once() first.\n");
        return;
    }
    
    // Reset to first page
    g_wifi_page_index = 0;
    
    printf("Starting WiFi Scan...\n");

    // 1. Start Scan (Blocking)
    // This blocks for ~1.5s. Ensure Watchdog doesn't bite if this task is high priority.
    esp_wifi_scan_start(NULL, true);

    // 2. Get number of results found
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    
    // Safety limit
    if (ap_count > MAX_WIFI_RESULTS) ap_count = MAX_WIFI_RESULTS;

    // 3. FIX: Allocate buffer on HEAP, not Stack
    wifi_ap_record_t *ap_info = (wifi_ap_record_t *)calloc(ap_count, sizeof(wifi_ap_record_t));
    
    if (!ap_info) {
        printf("CRITICAL: Out of memory for WiFi scan!\n");
        return;
    }

    // 4. Get Records into Heap Buffer
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));

    // 5. Update Global Data
    g_wifi_scan_count = ap_count;
    
    for (int i = 0; i < ap_count; i++) {
        // Safe Copy
        strncpy(g_wifi_scan_results[i].ssid, (char *)ap_info[i].ssid, 32);
        g_wifi_scan_results[i].ssid[32] = '\0';
        
        g_wifi_scan_results[i].rssi = ap_info[i].rssi;
        g_wifi_scan_results[i].channel = ap_info[i].primary;
        g_wifi_scan_results[i].auth_mode = ap_info[i].authmode;
        
        // Debug
        // printf("[%d] %s (%d)\n", i, g_wifi_scan_results[i].ssid, g_wifi_scan_results[i].rssi);
    }
    
    // 6. Free the Heap Buffer
    free(ap_info);
    
    printf("Scan Done. Found %d networks.\n", g_wifi_scan_count);
    
    // 7. Refresh UI
    refresh_wifi_list_ui();
    update_full_ui();
}


// Context struct to pass data into the decoder callback
typedef struct {
    ImageTexture *texture; // Your texture struct
    int outputX;           // Where to draw it on the texture
    int outputY;
} JpegDev;

static UINT tjd_input(JDEC* jd, BYTE* buff, UINT nd) {
    FILE *f = (FILE *)jd->device;
    if (buff) {
        size_t read = fread(buff, 1, nd, f);
        // If we hit EOF, log it but return what we got
        if (read < nd && feof(f)) {
            // ESP_LOGW("MJPEG", "Input: EOF hit unexpectedly"); // Comment out if spammy
        }
        return (UINT)read;
    } else {
        if (fseek(f, nd, SEEK_CUR) == 0) return nd;
        return 0;
    }
}

// 2. OUTPUT FUNC: Receives decoded RGB888 blocks and converts to RGBA
static UINT tjd_output(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegDev *dev = (JpegDev *)jd->device;
    ImageTexture *tex = dev->texture;

    // 'bitmap' is an array of RGB bytes (R, G, B, R, G, B...)
    uint8_t *rgb = (uint8_t *)bitmap;
    
    // Loop through the rectangular block provided by the decoder
    for (int y = rect->top; y <= rect->bottom; y++) {
        for (int x = rect->left; x <= rect->right; x++) {
            
            // Safety check: Don't write outside your texture memory
            if (x < tex->width && y < tex->height) {
                
                // Calculate target index in your RGBA array
                int index = y * tex->width + x;
                
                // Convert RGB (3 bytes) to RGBA (4 bytes)
                tex->data[index].r = rgb[0];
                tex->data[index].g = rgb[1];
                tex->data[index].b = rgb[2];
                tex->data[index].a = 255; // Fully opaque
            }
            // Move source pointer forward 3 bytes (R,G,B)
            rgb += 3;
        }
    }
    return 1; // Continue decoding
}

#define READ_CHUNK_SIZE 4096

// Update to match your screen resolution
#define VID_W 480
#define VID_H 320

void video_player_task(void *pvParam) {
    // 1. Correct Path
    const char *filename = "/sdcard/output.mjpeg";
    FILE *f = fopen(filename, "rb");
    
    if (!f) {
        FreeOSLogE(TAG, "Failed to open %s", filename);
        vTaskDelete(NULL);
    }

    // 2. Get File Size for EOF checks
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    FreeOSLogI(TAG, "File Size: %ld", fileSize);

    // 3. Allocate 480x320 Buffers
    ImageTexture *frameBuffer = heap_caps_malloc(sizeof(ImageTexture), MALLOC_CAP_SPIRAM);
    frameBuffer->width = VID_W;
    frameBuffer->height = VID_H;
    
    // 480x320x4 bytes = ~600KB. Ensure your ESP32 has PSRAM enabled.
    frameBuffer->data = heap_caps_malloc(VID_W * VID_H * sizeof(ColorRGBA), MALLOC_CAP_SPIRAM);
    
    if (!frameBuffer->data) {
        FreeOSLogE(TAG, "Failed to allocate video memory! Check PSRAM.");
        fclose(f);
        vTaskDelete(NULL);
    }

    char *work = heap_caps_malloc(4096, MALLOC_CAP_INTERNAL);
    uint8_t *chunkBuf = heap_caps_malloc(READ_CHUNK_SIZE, MALLOC_CAP_INTERNAL);

    JDEC jdec;
    JpegDev dev;
    dev.texture = frameBuffer;

    FreeOSLogI(TAG, "Starting 480x320 Video...");

    while (1) {
        bool foundFrame = false;
        long frameStartOffset = 0;
        
        // --- SCANNING ---
        while (!foundFrame) {
            long currentPos = ftell(f);
            size_t bytesRead = fread(chunkBuf, 1, READ_CHUNK_SIZE, f);
            
            // EOF Check
            if (bytesRead < 2) {
                clearerr(f); fseek(f, 0, SEEK_SET); continue;
            }

            for (int i = 0; i < bytesRead - 1; i++) {
                if (chunkBuf[i] == 0xFF && chunkBuf[i+1] == 0xD8) {
                    foundFrame = true;
                    frameStartOffset = currentPos + i;
                    break;
                }
            }
            if (!foundFrame) vTaskDelay(1);
        }

        // --- DECODE ---
        
        // Safety: If we found a frame very close to the end (e.g. last 1KB), it's likely truncated.
        if (fileSize - frameStartOffset < 1024) {
            ESP_LOGW(TAG, "Skipping truncated frame at end of file.");
            clearerr(f);
            fseek(f, 0, SEEK_SET);
            continue;
        }

        clearerr(f);
        fseek(f, frameStartOffset, SEEK_SET);

        JRESULT res = jd_prepare(&jdec, tjd_input, work, 4096, f);
        
        if (res == JDR_OK) {
            jdec.device = &dev;
            res = jd_decomp(&jdec, tjd_output, 0);
            
            if (res == JDR_OK) {
                // SUCCESS
                //FreeOSLogI("GFX", "Sent Pixel Buffer Command");
                GraphicsCommand cmd = {
                    .cmd = CMD_DRAW_PIXEL_BUFFER,
                    .x = 0, .y = 0,
                    .w = VID_W, .h = VID_H, // 480x320
                    .pixelBuffer = frameBuffer->data
                };
                QueueSend(g_graphics_queue, &cmd, QUEUE_MAX_DELAY);
                
                GraphicsCommand cmd_flush = {
                    .cmd = CMD_UPDATE_AREA,
                    .x = 0, .y = 0, .w = VID_W, .h = VID_H
                };
                QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
            }
            else {
                //FreeOSLogI("GFX", "Decode Failed");
                // Decode Failed (Likely EOF hit unexpectedly)
                // If we fail, assume this frame is bad/truncated.
                // If we are near the end of the file (>90%), just restart.
                if (frameStartOffset > fileSize * 0.9) {
                     ESP_LOGW(TAG, "EOF hit near end. Restarting video.");
                     fseek(f, 0, SEEK_SET);
                     continue;
                }
            }
        }
        else {
             // Prepare failed.
             if (frameStartOffset > fileSize * 0.9) {
                 fseek(f, 0, SEEK_SET);
                 continue;
            }
        }

        // Advance Ptr
        clearerr(f);
        fseek(f, frameStartOffset + 2, SEEK_SET);

        // Frame Pacing (480x320 takes longer to transfer, so 30ms might be optimistic)
        // If it feels slow, decrease this to 10 or 0.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}



typedef struct {
    FILE* f;
    CCImage* img;
} ThumbContext;

static UINT tjd_input_thumb(JDEC* jd, BYTE* buff, UINT nd) {
    ThumbContext *ctx = (ThumbContext *)jd->device;
    if (buff) return (UINT)fread(buff, 1, nd, ctx->f);
    return fseek(ctx->f, nd, SEEK_CUR) == 0 ? nd : 0;
}

static UINT tjd_output_thumb(JDEC* jd, void* bitmap, JRECT* rect) {
    ThumbContext *ctx = (ThumbContext *)jd->device;
    CCImage *img = ctx->img; // Get the image from context
    uint8_t *rgb = (uint8_t *)bitmap;

    for (int y = rect->top; y <= rect->bottom; y++) {
        for (int x = rect->left; x <= rect->right; x++) {
            if (x < img->size->width && y < img->size->height) {
                int index = y * img->size->width + x;
                ColorRGBA *pixel = &((ColorRGBA*)img->imageData)[index];
                pixel->r = rgb[0];
                pixel->g = rgb[1];
                pixel->b = rgb[2];
                pixel->a = 255;
            }
            rgb += 3;
        }
    }
    return 1;
}

static UINT tjd_input_one_frame(JDEC* jd, BYTE* buff, UINT nd) {
    FILE *f = (FILE *)jd->device;
    if (buff) return (UINT)fread(buff, 1, nd, f);
    return fseek(f, nd, SEEK_CUR) == 0 ? nd : 0;
}

// Output: Writes RGBA to CCImage buffer
static UINT tjd_output_one_frame(JDEC* jd, void* bitmap, JRECT* rect) {
    // We pass the CCImage* as the device context
    CCImage *img = (CCImage *)jd->device;
    uint8_t *rgb = (uint8_t *)bitmap;

    for (int y = rect->top; y <= rect->bottom; y++) {
        for (int x = rect->left; x <= rect->right; x++) {
            if (x < img->size->width && y < img->size->height) {
                int index = y * img->size->width + x;
                ColorRGBA *pixel = &((ColorRGBA*)img->imageData)[index];
                
                pixel->r = rgb[0];
                pixel->g = rgb[1];
                pixel->b = rgb[2];
                pixel->a = 255; // Opaque
            }
            rgb += 3;
        }
    }
    return 1;
}




// Corrected Main Function Call
CCImage* imageWithVideoFrame(const char *path, int width, int height) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        FreeOSLogE("IMG", "Could not open video: %s", path);
        return NULL;
    }
    
    FreeOSLogI("IMG", "Could open video: %s", path);

    // 1. Allocate CCImage in PSRAM
    CCImage *img = image();
    img->size->width = width;
    img->size->height = height;
    img->imageData = heap_caps_malloc(width * height * sizeof(ColorRGBA), MALLOC_CAP_SPIRAM);
    
    // 2. Scan for First Frame (FF D8)
    // We scan the first 4KB. If the frame isn't there, it's not a valid MJPEG.
    uint8_t *chunk = cc_safe_alloc(1, 4096);
    size_t read = fread(chunk, 1, 4096, f);
    long headerOffset = -1;

    for (int i = 0; i < read - 1; i++) {
        if (chunk[i] == 0xFF && chunk[i+1] == 0xD8) {
            headerOffset = i;
            break;
        }
    }
    free(chunk);

    if (headerOffset == -1) {
        FreeOSLogE("IMG", "No frame found in video header");
        fclose(f);
        // Return a blank/error image or NULL
        return img;
    }

    // 3. Decode
    fseek(f, headerOffset, SEEK_SET); // Jump to the Start of Image
    
    char *work = cc_safe_alloc(1, 4096);
    JDEC jdec;


    // Context holds both the File and the Target Image
    ThumbContext ctx;
    ctx.f = f;
    ctx.img = img;

    // Use specific helpers for this context
    if (jd_prepare(&jdec, tjd_input_thumb, work, 4096, &ctx) == JDR_OK) {
        jd_decomp(&jdec, tjd_output_thumb, 0);
    }
    
    // --- DEBUG: LOG PIXEL DATA (FIXED) ---
    FreeOSLogI("IMG", "--- PIXEL DATA DUMP ---");

    // 1. Cast the raw bytes to a Pixel Array
    ColorRGBA *pixels = (ColorRGBA *)img->imageData;
    
    // Print First 5 Pixels (Top Left)
    for (int i = 0; i < 5; i++) {
        ColorRGBA p = pixels[i]; // Now this works
        FreeOSLogI("IMG", "Pixel[%d]: R=%d G=%d B=%d A=%d", i, p.r, p.g, p.b, p.a);
    }

    // Print Center Pixel
    int centerIdx = (height/2) * width + (width/2);
    ColorRGBA p = pixels[centerIdx];
    FreeOSLogI("IMG", "Pixel[Center]: R=%d G=%d B=%d A=%d", p.r, p.g, p.b, p.a);

    free(work);
    fclose(f);
    return img;
}

typedef struct {
    FILE *f;
    CCImageView *view;
    uint8_t *chunkBuf; // Buffer for scanning
    char *workBuf;     // Buffer for decoder
    CCImage *imgBuffer; // Reusable image memory
} VideoState;

void next_frame_task(void *pvParam) {
    int64_t t3 = esp_timer_get_time();
    
    CCImageView *targetView = (CCImageView *)pvParam;
    const char *path = "/sdcard/output.mjpeg";

    // 1. Setup State
    FILE *f = fopen(path, "rb");
    if (!f) {
        FreeOSLogE("VID", "Cannot open %s", path);
        vTaskDelete(NULL);
    }

    // Allocate reusable buffers (Don't malloc inside the loop!)
    uint8_t *chunkBuf = heap_caps_malloc(4096, MALLOC_CAP_INTERNAL);
    char *workBuf = heap_caps_malloc(4096, MALLOC_CAP_INTERNAL);
    
    // Create the image buffer ONCE. We will just overwrite its pixels.
    CCImage *img = image();
    img->size->width = 320;
    img->size->height = 480;
    img->imageData = heap_caps_malloc(480 * 320 * sizeof(ColorRGBA), MALLOC_CAP_SPIRAM);
    
    // Assign this image to the view immediately
    targetView->image = img;

    JDEC jdec;
    ThumbContext ctx; // Reuse our context helper from before
    ctx.f = f;
    ctx.img = img;

    FreeOSLogI("VID", "Starting Animation Loop...");

    while (1) {
        bool foundFrame = false;
        long frameStart = 0;

        // --- 2. Scan for Next Frame ---
        while (!foundFrame) {
            long pos = ftell(f);
            size_t read = fread(chunkBuf, 1, 4096, f);
            
            // Loop video at EOF
            if (read < 2) {
                clearerr(f); fseek(f, 0, SEEK_SET); continue;
            }

            for (int i = 0; i < read - 1; i++) {
                if (chunkBuf[i] == 0xFF && chunkBuf[i+1] == 0xD8) {
                    foundFrame = true;
                    frameStart = pos + i;
                    break;
                }
            }
        }

        // --- 3. Decode Frame ---
        clearerr(f);
        fseek(f, frameStart, SEEK_SET);

        if (jd_prepare(&jdec, tjd_input_thumb, workBuf, 4096, &ctx) == JDR_OK) {
            if (jd_decomp(&jdec, tjd_output_thumb, 0) == JDR_OK) {
                
                // --- 4. Trigger Redraw ---
                // We don't send pixel data here. We tell the Graphics Task
                // "The view has changed, please redraw the tree."
                
                // Note: The graphics task reads targetView->image->imageData,
                // which we just overwrote with new pixels.
                
                GraphicsCommand cmd = {
                    .cmd = CMD_DRAW_PIXEL_BUFFER, // Or your custom view redraw command
                    .x = 0, .y = 0, .w = 320, .h = 480,
                    .pixelBuffer = img->imageData
                };
                QueueSend(g_graphics_queue, &cmd, 0);
                
                //update_view_only(targetView);
                
                GraphicsCommand cmd_flush = {
                    .cmd = CMD_UPDATE_AREA,
                    .x = 0, .y = 0, .w = 320, .h = 480
                };
                QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
                
                
            }
        }

        // --- 5. Advance & Wait ---
        clearerr(f);
        fseek(f, frameStart + 2, SEEK_SET);
        
        // 0.1s Delay (100ms)
        //vTaskDelay(pdMS_TO_TICKS(25));
        
        int64_t t4 = esp_timer_get_time();

        FreeOSLogI(TAG, "nxt frame Time:  %lld us", (t4 - t3));
    }
}

int currentFrame = 0;

void next_frame_task1(void *pvParam) {
    
    while (1) {
        
        CCImageView *targetView = (CCImageView *)pvParam;
        CCString* path = stringWithFormat("/sdcard/frames/img_%d.png", currentFrame);
        
        targetView->image = imageWithFile(path);
        
        /*GraphicsCommand cmd = {
            .cmd = CMD_DRAW_PIXEL_BUFFER, // Or your custom view redraw command
            .x = 0, .y = 0, .w = 320, .h = 480,
            .pixelBuffer = targetView->image->imageData
        };
        QueueSend(g_graphics_queue, &cmd, 0);*/
        
        //update_view_only(targetView);
        
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = 0, .y = 0, .w = 320, .h = 480
        };
        QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
        
        currentFrame++;
        
        // 0.1s Delay (100ms)
        vTaskDelay(pdMS_TO_TICKS(50));
        
    }
    
}

CCImageView* videoView = NULL;

void setup_video_preview_ui(void) {
    // 1. Create the Image View
    videoView = imageViewWithFrame(ccRect(0, 0, 320, 480));
    
    // 2. Load JUST the first frame (480x320 resolution)
    // Note: This might take ~200-500ms, blocking the UI briefly.
    videoView->image = imageWithVideoFrame("/sdcard/output.mjpeg", 320, 480);
    
    FreeOSLogI("IMG", "imageWithVideoFrame");
    
    // 3. Add to screen
    viewAddSubview(mainWindowView, videoView);
    
    
}

void setup_video_preview_ui1(void) {
    // 1. Create the Image View
    videoView = imageViewWithFrame(ccRect(0, 0, 320, 480));
    
    // 2. Load JUST the first frame (480x320 resolution)
    // Note: This might take ~200-500ms, blocking the UI briefly.
    videoView->image = imageWithFile(ccs("/sdcard/frames/img_1.png"));
    
    FreeOSLogI("IMG", "imageWithVideoFrame");
    
    // 3. Add to screen
    viewAddSubview(mainWindowView, videoView);
    
    
}


// CONSTANTS FROM FFMPEG SETUP
#define VIDEO_WIDTH  320  // Transposed width
#define VIDEO_HEIGHT 480  // Transposed height

// ... (Keep your Framebuffer struct and yuv2rgb helpers here) ...

typedef struct {
    Framebuffer *fb;
    const char *filepath;
} VideoTaskParams;


void vVideoDecodeTask(void *pvParameters) {
    VideoTaskParams *params = (VideoTaskParams *)pvParameters;
    Framebuffer *fb = params->fb;
    const char *path = params->filepath;

    FreeOSLogI(TAG, "Opening video file: %s", path);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        FreeOSLogE(TAG, "Failed to open file");
        vTaskDelete(NULL);
        return;
    }

    // 1. INITIAL SETUP
    esp_h264_dec_cfg_sw_t cfg = { .pic_type = ESP_H264_RAW_FMT_I420 };
    esp_h264_dec_handle_t dec_handle = NULL;
    esp_h264_dec_sw_new(&cfg, &dec_handle);
    esp_h264_dec_open(dec_handle);

    // 2. ALLOCATE BUFFER
    const int BUFFER_CAPACITY = 1024 * 512;
        
        // Verify we have enough PSRAM
        uint8_t *file_buffer = (uint8_t *)heap_caps_malloc(BUFFER_CAPACITY, MALLOC_CAP_SPIRAM);
        
        if (file_buffer == NULL) {
            FreeOSLogE(TAG, "Failed to allocate 512KB buffer! Trying 256KB...");
            // Fallback if your specific board is low on RAM
            file_buffer = (uint8_t *)heap_caps_malloc(1024 * 256, MALLOC_CAP_SPIRAM);
        }
    
    size_t current_data_len = 0;
    esp_h264_dec_in_frame_t in_frame = { 0 };
    esp_h264_dec_out_frame_t out_frame = { 0 };

    FreeOSLogI(TAG, "Starting decode loop...");

    while (1) {
        // A. READ DATA
        size_t space_available = BUFFER_CAPACITY - current_data_len;
        if (space_available < 1024) {
            current_data_len = 0;
            space_available = BUFFER_CAPACITY;
        }

        size_t bytes_read = fread(file_buffer + current_data_len, 1, space_available, f);
        if (bytes_read == 0) {
            fseek(f, 0, SEEK_SET);
            current_data_len = 0;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        current_data_len += bytes_read;

        // B. DECODE LOOP
        in_frame.raw_data.buffer = file_buffer;
        in_frame.raw_data.len = current_data_len;
        in_frame.dts = 0;
        in_frame.pts = 0;

        while (in_frame.raw_data.len > 0) {
            
            esp_h264_err_t ret = esp_h264_dec_process(dec_handle, &in_frame, &out_frame);

            // --- CASE 1: SUCCESS ---
            if (ret == ESP_H264_ERR_OK) {
                if (out_frame.outbuf != NULL) {
                     // Draw Frame (Optimized YUV->RGB)
                    int width = VIDEO_WIDTH;
                    int height = VIDEO_HEIGHT;
                    uint8_t *y_plane = out_frame.outbuf;
                    uint8_t *u_plane = y_plane + (width * height);
                    uint8_t *v_plane = u_plane + (width * height / 4);
                    int u_stride = width / 2;

                    for (int y = 0; y < height; y += 2) {
                        for (int x = 0; x < width; x += 2) {
                            if (y >= fb->displayHeight || x >= fb->displayWidth) continue;
                            
                            int uv_off = (y>>1)*u_stride + (x>>1);
                            int d = u_plane[uv_off] - 128;
                            int e = v_plane[uv_off] - 128;
                            int rc = (409*e+128);
                            int gc = (-100*d-208*e+128);
                            int bc = (516*d+128);

                            int y_idx[4] = { y*width+x, y*width+x+1, (y+1)*width+x, (y+1)*width+x+1 };
                            int fb_idx[4] = {
                                (y*fb->displayWidth+x)*3, (y*fb->displayWidth+x+1)*3,
                                ((y+1)*fb->displayWidth+x)*3, ((y+1)*fb->displayWidth+x+1)*3
                            };

                            for(int k=0; k<4; k++) {
                                int py = y+(k/2), px = x+(k%2);
                                if(py>=fb->displayHeight || px>=fb->displayWidth) continue;
                                int y_term = 298*(y_plane[y_idx[k]]-16);
                                uint8_t *p = (uint8_t*)fb->pixelData + fb_idx[k];
                                int r = (y_term+rc)>>8; p[0] = (r>255)?255:(r<0?0:r);
                                int g = (y_term+gc)>>8; p[1] = (g>255)?255:(g<0?0:g);
                                int b = (y_term+bc)>>8; p[2] = (b>255)?255:(b<0?0:b);
                            }
                        }
                    }
                }
                
                if (in_frame.consume > 0) {
                    in_frame.raw_data.buffer += in_frame.consume;
                    in_frame.raw_data.len    -= in_frame.consume;
                } else {
                    break; // Need more data
                }
            }
            
            // --- CASE 2: FATAL ERROR (RESTART VIDEO) ---
            else {
                FreeOSLogE(TAG, "Decoder Error %d. RESTARTING VIDEO to restore sync...", ret);
                
                // 1. Destroy Corrupted Decoder
                esp_h264_dec_close(dec_handle);
                esp_h264_dec_del(dec_handle);
                
                // 2. REWIND FILE (The Fix)
                // We must go back to start to read SPS/PPS headers again
                fseek(f, 0, SEEK_SET);
                
                // 3. Clear Buffer
                current_data_len = 0;
                in_frame.raw_data.len = 0; // Forces break from inner loop

                // 4. Create Fresh Decoder
                dec_handle = NULL;
                esp_h264_dec_sw_new(&cfg, &dec_handle);
                esp_h264_dec_open(dec_handle);
                
                ESP_LOGW(TAG, "Video Restarted.");
                break; // Break inner loop to trigger fread from byte 0
            }
        }

        // C. PRESERVE LEFTOVERS
        size_t leftovers = in_frame.raw_data.len;
        if (leftovers > 0) {
            memmove(file_buffer, in_frame.raw_data.buffer, leftovers);
        }
        current_data_len = leftovers;

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_h264_dec_close(dec_handle);
    esp_h264_dec_del(dec_handle);
    free(file_buffer);
    fclose(f);
    vTaskDelete(NULL);
}

void start_video_player(Framebuffer *fb) {
    // You must allocate this structure so it persists after this function returns
    VideoTaskParams *params = cc_safe_alloc(1, sizeof(VideoTaskParams));
    params->fb = fb;
    // Assuming you have mounted SD card to /sdcard
    params->filepath = "/sdcard/output.h264";

    xTaskCreatePinnedToCore(vVideoDecodeTask, "VidTask", 16384, (void*)params, 2, NULL, 1);
}

// --- HELPER: YUV420 to RGB888 ---
// Returns a new buffer that you must free() later
uint8_t* yuv420_to_rgb888(uint8_t *y_plane, uint8_t *u_plane, uint8_t *v_plane, int width, int height) {
    // Allocate RGB Buffer (Width * Height * 3 bytes)
    uint8_t *rgb_buffer = (uint8_t *)heap_caps_malloc(width * height * 3, MALLOC_CAP_SPIRAM);
    if (!rgb_buffer) return NULL;

    int u_stride = width / 2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Get Y (Luma)
            int Y_val = y_plane[y * width + x];
            
            // Get UV (Chroma) - Shared by 2x2 pixels
            int uv_idx = (y >> 1) * u_stride + (x >> 1);
            int U_val = u_plane[uv_idx];
            int V_val = v_plane[uv_idx];

            // Math
            int c = Y_val - 16;
            int d = U_val - 128;
            int e = V_val - 128;

            int r = (298 * c + 409 * e + 128) >> 8;
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int b = (298 * c + 516 * d + 128) >> 8;

            // Clamp
            if (r > 255) r = 255; else if (r < 0) r = 0;
            if (g > 255) g = 255; else if (g < 0) g = 0;
            if (b > 255) b = 255; else if (b < 0) b = 0;

            // Write to buffer
            int idx = (y * width + x) * 3;
            rgb_buffer[idx] = (uint8_t)r;
            rgb_buffer[idx+1] = (uint8_t)g;
            rgb_buffer[idx+2] = (uint8_t)b;
        }
    }
    return rgb_buffer;
}

// --- MAIN FUNCTION: GET FIRST FRAME ---
// Returns: A pointer to RGB888 data (in PSRAM). Returns NULL on failure.
// Note: You must free() the result when done!
uint8_t* get_first_video_frame(const char *filepath, int width, int height) {
    FreeOSLogI("VID", "Extracting first frame from %s...", filepath);

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        FreeOSLogE("VID", "File not found");
        return NULL;
    }

    // 1. Setup Decoder
    esp_h264_dec_cfg_sw_t cfg = { .pic_type = ESP_H264_RAW_FMT_I420 };
    esp_h264_dec_handle_t dec_handle = NULL;
    if (esp_h264_dec_sw_new(&cfg, &dec_handle) != ESP_H264_ERR_OK) {
        fclose(f);
        return NULL;
    }
    esp_h264_dec_open(dec_handle);

    // 2. Allocate Temp Buffer (Large enough for one I-Frame)
    const int CHUNK_SIZE = 1024 * 64;
    uint8_t *file_buffer = (uint8_t *)heap_caps_malloc(CHUNK_SIZE, MALLOC_CAP_SPIRAM);
    
    uint8_t *result_pixels = NULL;
    int decoded = 0;
    
    esp_h264_dec_in_frame_t in_frame = { 0 };
    esp_h264_dec_out_frame_t out_frame = { 0 };

    // 3. Read Loop (Only runs until first frame is found)
    while (!decoded) {
        size_t bytes_read = fread(file_buffer, 1, CHUNK_SIZE, f);
        if (bytes_read == 0) break; // EOF

        in_frame.raw_data.buffer = file_buffer;
        in_frame.raw_data.len = bytes_read;
        
        while (in_frame.raw_data.len > 0) {
            esp_h264_err_t ret = esp_h264_dec_process(dec_handle, &in_frame, &out_frame);
            
            if (ret == ESP_H264_ERR_OK && out_frame.outbuf != NULL) {
                FreeOSLogI("VID", "Frame found! Converting...");
                
                // Pointers for YUV420 Planar
                uint8_t *y = out_frame.outbuf;
                uint8_t *u = y + (width * height);
                uint8_t *v = u + (width * height / 4);

                // Convert to RGB
                result_pixels = yuv420_to_rgb888(y, u, v, width, height);
                
                decoded = 1; // STOP LOOP
                break;
            }

            if (in_frame.consume > 0) {
                in_frame.raw_data.buffer += in_frame.consume;
                in_frame.raw_data.len -= in_frame.consume;
            } else {
                break; // Fetch more data
            }
        }
    }

    // 4. Cleanup
    free(file_buffer);
    esp_h264_dec_close(dec_handle);
    esp_h264_dec_del(dec_handle);
    fclose(f);

    if (result_pixels) {
        FreeOSLogI("VID", "Success. Frame extraction complete.");
    } else {
        FreeOSLogE("VID", "Failed to decode any frame.");
    }

    return result_pixels;
}

// --- MAIN FUNCTION: GET SPECIFIC FRAME ---
// frameNumber: 0-based index (0 = first frame, 100 = 101st frame)
uint8_t* get_video_frame(const char *filepath, int width, int height, int frameNumber) {
    FreeOSLogI("VID", "Seeking Frame %d in %s...", frameNumber, filepath);

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        FreeOSLogE("VID", "File not found");
        return NULL;
    }

    // 1. Setup Decoder
    esp_h264_dec_cfg_sw_t cfg = { .pic_type = ESP_H264_RAW_FMT_I420 };
    esp_h264_dec_handle_t dec_handle = NULL;
    if (esp_h264_dec_sw_new(&cfg, &dec_handle) != ESP_H264_ERR_OK) {
        fclose(f);
        return NULL;
    }
    esp_h264_dec_open(dec_handle);

    // 2. Allocate Buffer (64KB is usually enough for chunks)
    const int BUFFER_CAPACITY = 1024 * 64;
    uint8_t *file_buffer = (uint8_t *)heap_caps_malloc(BUFFER_CAPACITY, MALLOC_CAP_SPIRAM);
    
    uint8_t *result_pixels = NULL;
    int current_frame_index = 0;
    size_t current_data_len = 0;
    
    esp_h264_dec_in_frame_t in_frame = { 0 };
    esp_h264_dec_out_frame_t out_frame = { 0 };

    // 3. Decode Loop
    while (1) {
        // Read data into whatever space is left in buffer
        size_t space_available = BUFFER_CAPACITY - current_data_len;
        
        // Safety: If buffer full, hard reset logic (similar to your player task)
        if (space_available < 1024) {
             current_data_len = 0;
             space_available = BUFFER_CAPACITY;
        }

        size_t bytes_read = fread(file_buffer + current_data_len, 1, space_available, f);
        if (bytes_read == 0) break; // End of file

        current_data_len += bytes_read;
        in_frame.raw_data.buffer = file_buffer;
        in_frame.raw_data.len = current_data_len;
        
        while (in_frame.raw_data.len > 0) {
            esp_h264_err_t ret = esp_h264_dec_process(dec_handle, &in_frame, &out_frame);
            
            // Check if a frame was completed
            if (ret == ESP_H264_ERR_OK && out_frame.outbuf != NULL) {
                
                // Is this the frame we want?
                if (current_frame_index == frameNumber) {
                    FreeOSLogI("VID", "Found Frame %d! Converting...", frameNumber);
                    
                    uint8_t *y = out_frame.outbuf;
                    uint8_t *u = y + (width * height);
                    uint8_t *v = u + (width * height / 4);

                    // Convert ONLY this frame
                    result_pixels = yuv420_to_rgb888(y, u, v, width, height);
                    
                    // Cleanup and Exit immediately
                    goto cleanup;
                }
                
                // Not the right frame yet, keep going
                current_frame_index++;
            }

            // Advance pointers
            if (in_frame.consume > 0) {
                in_frame.raw_data.buffer += in_frame.consume;
                in_frame.raw_data.len -= in_frame.consume;
            } else {
                break; // Fetch more data
            }
        }

        // Preserve leftovers (Move to front)
        if (in_frame.raw_data.len > 0) {
            memmove(file_buffer, in_frame.raw_data.buffer, in_frame.raw_data.len);
        }
        current_data_len = in_frame.raw_data.len;
    }

cleanup:
    free(file_buffer);
    esp_h264_dec_close(dec_handle);
    esp_h264_dec_del(dec_handle);
    fclose(f);

    if (result_pixels) {
        FreeOSLogI("VID", "Frame extraction success.");
    } else {
        FreeOSLogE("VID", "Frame %d not found (Video too short?).", frameNumber);
    }

    return result_pixels;
}

void load_video_poster() {
    int w = 320;
    int h = 480;

    // 1. Get the pixels (Allocates memory in PSRAM)
    uint8_t *pixels = get_first_video_frame("/sdcard/output.h264", w, h);
    //uint8_t *pixels = get_video_frame("/sdcard/output.h264", w, h, 200);

    if (pixels != NULL) {
        // 2. Assign to your Framebuffer
        // Note: You might need to cast to (void*) depending on your struct definition
        fb = (Framebuffer){
            .displayWidth = w,
            .displayHeight = h,
            .pixelData = pixels, // The buffer we just created
            .colorMode = COLOR_MODE_BGR888
        };
        
        // 3. Draw it!
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = 0, .y = 0,
            .w = 320, .h = 480
        };
        QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
        
        printf("load_video_poster...\n");
    }
}

bool keyboardTouchEnabled = true;
bool appTouchEnabled = true;

static int64_t last_draw_time = 0;
const int64_t draw_interval_us = 100000; // 0.1 seconds in microseconds scrolls smoothly
//const int64_t draw_interval_us = 050000; // 0.05 seconds in microseconds has lag from sending too many requests to graphics pipeline

void updateTouch(void *pvParameter) {
    uint32_t event;
    FT6336U_TouchPointType touch_data;
    float newOffY;
    
    // This tracks the *actual* hardware state
    bool is_currently_pressed = false;
    
    while (1) {
        // Wait for an interrupt OR a 50ms timeout
        xQueueReceive(touch_intr_queue, &event, pdMS_TO_TICKS(50));
        
        esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
        if (ret != ESP_OK) {
            touch_data.touch_count = 0; // Treat scan error as a release
        }
        
        // --- ADD THIS LINE ---
        uint16_t current_y = touch_data.touch_count > 0 ? touch_data.tp[0].y : 0;
        
        // --- New State Logic ---
        if (touch_data.touch_count > 0) {
            // --- Finger is DOWN ---
            if (is_currently_pressed == false) {
                // This is a NEW press.
                is_currently_pressed = true; // 1. Set our local state
                g_drag_start_y = current_y; // Anchor the start point
                
                FreeOSLogI(TAG, "Touch Pressed: X=%u, Y=%u", touch_data.tp[0].x, touch_data.tp[0].y);
                
                // 2. Lock and post the "event" for LVGL to consume
                xSemaphoreTake(g_touch_mutex, QUEUE_MAX_DELAY);
                g_last_x = touch_data.tp[0].x;
                g_last_y = touch_data.tp[0].y;
                g_last_state = LV_INDEV_STATE_PR; // Set the "event"
                xSemaphoreGive(g_touch_mutex);
                
                int text_w = 200;
                int text_h = 30;
                
                ColorRGBA blue = {.r = 0, .g = 0, .b = 50, .a = 255};
                
                FreeOSLogI(TAG, "Graphics Command");
                
                int x1 = randNumberTo(320);
                int y1 = randNumberTo(480);
                
                x1 = 100;
                y1 = 410;
                
                FreeOSLogI(TAG, "Graphics Command %d %d", x1, y1);
                
                if (hasDrawnKeyboard == false) {
                    hasDrawnKeyboard = true;
                    
                    // --- 1. Send command to clear the area ---
                    GraphicsCommand cmd_clear;
                    cmd_clear.cmd = CMD_DRAW_RECT;
                    cmd_clear.x = x1;
                    cmd_clear.y = y1;
                    cmd_clear.w = text_w;
                    cmd_clear.h = text_h;
                    cmd_clear.color = blue;
                    //QueueSend(g_graphics_queue, &cmd_clear, 0);
                    
                    ColorRGBA white2 = {.r = 255, .g = 255, .b = 255, .a = 255};
                    
                    // --- 2. Send command to draw the text ---
                    GraphicsCommand cmd_text;
                    cmd_text.cmd = CMD_DRAW_TEXT;
                    cmd_text.x = x1;
                    cmd_text.y = y1; // Your renderText needs to handle Y as baseline
                    cmd_text.text = strdup("Tapped!");
                    //QueueSend(g_graphics_queue, &cmd_text, 0);
                    
                    // --- 2. Send command to draw the text ---
                    GraphicsCommand cmd_text1;
                    cmd_text1.cmd = CMD_DRAW_TEXT;
                    cmd_text1.x = 20;
                    cmd_text1.y = 380; // Your renderText needs to handle Y as baseline
                    cmd_text1.text = strdup(" Q  W  E  R  T  Y  U  I  O  P");
                    cmd_text1.color = white2;
                    QueueSend(g_graphics_queue, &cmd_text1, 0);
                    
                    // --- 2. Send command to draw the text ---
                    GraphicsCommand cmd_text12;
                    cmd_text12.cmd = CMD_DRAW_TEXT;
                    cmd_text12.x = 20;
                    cmd_text12.y = 420; // Your renderText needs to handle Y as baseline
                    cmd_text12.text = strdup("   A  S  D  F  G  H  J  K  L");
                    cmd_text12.color = white2;
                    QueueSend(g_graphics_queue, &cmd_text12, 0);
                    
                    // --- 2. Send command to draw the text ---
                    GraphicsCommand cmd_text13;
                    cmd_text13.cmd = CMD_DRAW_TEXT;
                    cmd_text13.x = 20;
                    cmd_text13.y = 460; // Your renderText needs to handle Y as baseline
                    cmd_text13.text = strdup("     Z  X  C  V  B  N  M");
                    cmd_text13.color = white2;
                    QueueSend(g_graphics_queue, &cmd_text13, 0);
                    
                    const char *long_text = "The quick brown fox jumps over the lazy dog. "
                    "This text needs to demonstrate word-wrapping and line-breaking "
                    "within a confined area for our new scroll view. \n\n" // Explicit newline
                    "This final sentence should appear on a completely new line.";
                    
                    ColorRGBA text_color = {.r = 255, .g = 255, .b = 255, .a = 255}; // White
                    
                    // --- 3. Define and Initialize the TextFormat Struct ---
                    TextFormat text_format = {
                        // Property 1: Center the text horizontally within the clip_w area
                        .alignment = TEXT_ALIGN_CENTER,
                        
                        // Property 2: Never split words mid-line (will move "boundaries" to next line)
                        .wrapMode = TEXT_WRAP_MODE_WHOLE_WORD,
                        
                        // Property 3: Add 10 extra pixels of space between the calculated line height
                        .lineSpacing = -2,
                        
                        // Property 4: Use FreeType's kerning metrics for a polished look
                        .glyphSpacing = -1
                        
                        
                    };
                    
                    ColorRGBA black_alpha = {.r = 0, .g = 0, .b = 0, .a = 255};
                    GraphicsCommand cmd_text2;
                    cmd_text2.cmd = CMD_DRAW_TEXT_BOX;
                    cmd_text2.x = 20;
                    cmd_text2.y = 150; // Your renderText needs to handle Y as baseline
                    cmd_text2.w = 280;
                    cmd_text2.h = 300;
                    cmd_text2.color = text_color;
                    cmd_text2.textFormat = text_format;
                    cmd_text2.fontSize = 12;
                    cmd_text2.text = strdup(long_text);
                    QueueSend(g_graphics_queue, &cmd_text2, 0);
                    
                    GraphicsCommand cmd_star;
                    cmd_star.cmd = CMD_DRAW_STAR;
                    cmd_star.x = 20;
                    cmd_star.y = 460; // Your renderText needs to handle Y as baseline
                    //QueueSend(g_graphics_queue, &cmd_star, 0);
                    
                    GraphicsCommand cmd_update;
                    cmd_update.cmd = CMD_UPDATE_AREA;
                    cmd_update.x = 0;
                    cmd_update.y = 0; // A rough bounding box
                    cmd_update.w = 320;
                    cmd_update.h = 480; // A bit larger to be safe
                    QueueSend(g_graphics_queue, &cmd_update, 0);
                    
                    if (!addedCursor) {
                        addedCursor = true;
                        if (setup_cursor_buffers() != ESP_OK) {
                            FreeOSLogE(TAG, "FATAL: Failed to set up cursor backup buffers. Halting task.");
                            vTaskDelete(NULL);
                            return;
                        }
                        FreeOSLogI(TAG, "Cursor backup buffer allocated.");
                        
                        // 2. Build the CMD_CURSOR_SETUP command
                        GraphicsCommand cmd_cursor_setup;
                        cmd_cursor_setup.cmd = CMD_CURSOR_SETUP;
                        
                        // Set the exact location here:
                        cmd_cursor_setup.x = 100;
                        cmd_cursor_setup.y = 100;
                        
                        // Pass the cursor's fixed size in case the graphics task needs it
                        cmd_cursor_setup.w = CURSOR_W;
                        cmd_cursor_setup.h = CURSOR_H;
                        
                        // 3. Send the command to initiate the blink cycle
                        //QueueSend(g_graphics_queue, &cmd_cursor_setup, 0);
                    }
                    
                }
                else {
                    ColorRGBA black_alpha = {.r = 0, .g = 0, .b = 0, .a = 255};
                    GraphicsCommand cmd_text;
                    cmd_text.cmd = CMD_DRAW_TEXT;
                    cmd_text.x = 30+keyboardCursorPosition*20;
                    cmd_text.y = 53; // Your renderText needs to handle Y as baseline
                    cmd_text.color = black_alpha;
                    cmd_text.text = strdup(letterForCoordinate());
                    
                    keyboardCursorPosition++;
                    
                    GraphicsCommand cmd_update;
                    cmd_update.cmd = CMD_UPDATE_AREA;
                    cmd_update.x = 0;
                    cmd_update.y = 0; // A rough bounding box
                    cmd_update.w = 320;
                    cmd_update.h = 100; // A bit larger to be safe
                    //QueueSend(g_graphics_queue, &cmd_update, 0);
                    
                    if (!setupui) {
                        
                        // 1. Create the high-level View Objects (RAM only)
                        // Make sure setup_ui_demo assigns to the global 'mainWindowView'
                        setup_ui_demo();
                        
                        // 2. Trigger the first render (Generates commands)
                        //update_full_ui();
                        
                        setupui = true;
                    }
                    
                    
                    
                    FreeOSLogI(TAG, "app_main() finished. Tasks are running.");
                }
                
            }
            else {
                FreeOSLogI(TAG, "touch 3");
                int curr_y = touch_data.tp[0].y;
                if (myDemoTextView != NULL) {
                    g_active_scrollview = myDemoTextView->scrollView;
                    
                    if (notFirstTouch == false) {
                        notFirstTouch = true;
                        g_touch_last_y = curr_y;
                        lastOffY = g_active_scrollview->contentOffset->y;
                        FreeOSLogI(TAG, "set lastOffY %d", lastOffY);
                    }
                    
                    if (g_active_scrollview) {
                        
                        int delta = g_touch_last_y - curr_y; // Drag Up = Positive Delta
                        FreeOSLogI(TAG, "g_active_scrollview %d", delta);
                        
                        // Calculate new offset
                        newOffY = lastOffY + (float)delta;
                        
                        float yValue = (float)lastOffY;
                        
                        CCPoint offsetPt;
                        offsetPt.x = 0;
                        offsetPt.y = (float)lastOffY;
                        
                        lastOffY = delta;
                        
                        CCPoint targetPoint = {
                            .x = 0,
                            .y = lastOffY // Correctly assigns the float value
                        };
                        
                        // Apply offset (This function handles clamping and moving the view)
                        //CCPoint offsetPt = {.x = 0, .y = yValue};
                        FreeOSLogI(TAG, "g_active_scrollview1 %f %f %f", lastOffY, newOffY, yValue);
                        scrollViewSetContentOffset(g_active_scrollview, &targetPoint);//ccPoint(0, lastOffY)
                        FreeOSLogI(TAG, "g_active_scrollview contentOffset %f", g_active_scrollview->contentOffset->y);
                        
                        // 2. THROTTLING CHECK: Only draw if 0.2s has passed
                        int64_t now = esp_timer_get_time();
                        
                        if ((now - last_draw_time) > draw_interval_us) {
                            
                            // Send the expensive command
                            update_view_only(g_active_scrollview->view);
                            
                            FreeOSLogI(TAG, "g_active_scrollview contentOffset %f", g_active_scrollview->contentOffset->y);
                            
                            // Reset the timer
                            last_draw_time = now;
                        }
                        
                    }
                }
                
                
                
            }
            // If finger is still down, we do nothing.
            
        } else {
            
            // --- Finger is UP ---
            if (is_currently_pressed == true) {
                // This is a NEW release.
                is_currently_pressed = false; // 1. Set our local state
                notFirstTouch = false;
                if (g_active_scrollview) FreeOSLogI(TAG, "Touch Released");
            }
        }
        
        
        if (touchEnabled) {
            if (touch_data.touch_count > 0 && setupui == true) {
                int touchX = touch_data.tp[0].x;
                int touchY = touch_data.tp[0].y;
                
                is_currently_pressed = true; // 1. Set our local state
                FreeOSLogI(TAG, "Touch DOWN");
                
                // If keyboard is visible (check if uiKeyboardView != NULL)
                if (uiKeyboardView != NULL && keyboardTouchEnabled == true) {
                    if (touchY > uiKeyboardView->frame->origin->y) {
                        keyboardTouchEnabled = false;
                        handle_keyboard_touch(touchX, touchY);
                        printf("handle handle_keyboard_touch");
                        //return; // Swallow touch so nothing underneath gets clicked
                    }
                    
                }
            }
            else {
                keyboardTouchEnabled = true;
            }
            
            if (touch_data.touch_count == 0) {
                is_currently_pressed = false;
                //FreeOSLogI(TAG, "Touch UP");
                
                if (currentView == CurrentViewFiles) {
                    handle_files_touch(0, 0, is_currently_pressed);
                }
                if (currentView == CurrentViewClock) {
                    printf("CurrentViewClock");
                    handle_clock_touch(0, 0, is_currently_pressed);
                }
                if (currentView == CurrentViewPaint) {
                    handle_paint_touch(0, 0, is_currently_pressed);
                }
                
            }
            
            //printf("touch3");
            
            if (currentView == CurrentViewHome) {
                
                if (touch_data.touch_count > 0 && setupui == true) {
                    int curr_x = touch_data.tp[0].x;
                    int curr_y = touch_data.tp[0].y;
                    
                    checkAvailableMemory();
                    
                    CCView* touchedView = find_grid_item_at(curr_x, curr_y);
                    CCPoint parentAbs = getAbsoluteOrigin(touchedView);
                    
                    if (touchedView && touchedView != g_last_touched_icon && openedApp == false) {
                        if (g_last_touched_icon != NULL) {
                            if (g_last_touched_icon->backgroundColor) free(g_last_touched_icon->backgroundColor);
                            g_last_touched_icon->backgroundColor = color(0,0,0,0);
                            update_view_area_via_parent(g_last_touched_icon);
                        }
                        
                        if (touchedView->backgroundColor) free(touchedView->backgroundColor);
                        touchedView->backgroundColor = color(0.0, 0.0, 0.0, 0.2);
                        update_view_only(touchedView);
                        printf("updatevie1w");
                        openHomeMenuItem(touchedView->tag);
                        g_last_touched_icon = touchedView;
                    }
                }
                else {
                    // --- FINGER UP (Release) ---
                    if (g_last_touched_icon != NULL && openedApp == false) {
                        if (g_last_touched_icon->backgroundColor) free(g_last_touched_icon->backgroundColor);
                        g_last_touched_icon->backgroundColor = color(0,0,0,0);
                        update_view_area_via_parent(g_last_touched_icon);
                        g_last_touched_icon = NULL;
                    }
                    is_currently_pressed = false;
                }
            }
            else if (currentView == CurrentViewFiles || currentView == CurrentViewSettings  || currentView == CurrentViewCalculator  || currentView == CurrentViewMusic  || currentView == CurrentViewPhotos || currentView == CurrentViewText || currentView == CurrentViewClock || currentView == CurrentViewPaint) {
                //else if (currentView == CurrentViewFiles || currentView == CurrentViewSettings)
                if (touch_data.touch_count > 0 && setupui == true) {
                    int x = touch_data.tp[0].x;
                    int y = touch_data.tp[0].y;
                    
                    checkAvailableMemory();
                    
                    if (x < 40 && y < 30) {
                        close_current_app();
                    }
                    if (currentView == CurrentViewFiles) {
                        handle_files_touch(x, y, is_currently_pressed);
                    }
                    if (currentView == CurrentViewSettings) {
                        handle_settings_touch(x, y, 1);
                    }
                    if (currentView == CurrentViewPhotos) {
                        CCView* row = find_subview_at_point(mainWindowView, x, y);
                        if (row) {
                            handle_gallery_touch(row->tag);
                        }
                    }
                    if (currentView == CurrentViewCalculator) {
                        CCView* row = find_subview_at_point(mainWindowView, x, y);
                        if (row) {
                            handle_calculator_input(row->tag);
                        }
                    }
                    if (currentView == CurrentViewClock) {
                        printf("CurrentViewClock");
                        handle_clock_touch(x, y, is_currently_pressed);
                    }
                    if (currentView == CurrentViewPaint) {
                        handle_paint_touch(x, y, is_currently_pressed);
                    }
                    
                }
                else {
                    
                }
            }
            else if (currentView == CurrentViewWifi) {
                if (touch_data.touch_count > 0 && setupui == true) {
                    int x = touch_data.tp[0].x;
                    int y = touch_data.tp[0].y;
                    
                    checkAvailableMemory();
                    
                    if (appTouchEnabled == true) {
                        appTouchEnabled = false;
                        handle_wifi_touch(x, y);
                    }
                    
                }
                else {
                    appTouchEnabled = true;
                }
            }
            
            
        }
        

        
        
        
        
        
    }
}



// ----------------------------------------------------------------------
// INITIALIZATION FUNCTION
// Call this function from your app_main
// ----------------------------------------------------------------------
/*void initialize_touch()
 {
 FreeOSLogI(TAG, "Initializing I2C Master for touch panel");
 
 // --- 1. Create the new I2C Master Bus ---
 i2c_master_bus_config_t bus_cfg = {
 .i2c_port = I2C_HOST,
 .scl_io_num = PIN_NUM_I2C_SCL,
 .sda_io_num = PIN_NUM_I2C_SDA,
 .clk_source = I2C_CLK_SRC_DEFAULT,
 .glitch_ignore_cnt = 7,
 .flags.enable_internal_pullup = true,
 };
 i2c_master_bus_handle_t bus_handle;
 ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));
 
 // --- 2. Create the I2C Panel IO Handle (THE MISSING STEP) ---
 FreeOSLogI(TAG, "Creating I2C panel IO handle");
 esp_lcd_panel_io_handle_t io_handle = NULL;
 esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();
 
 io_config.scl_speed_hz = 400000;
 
 // This is the line you were missing.
 // It creates the actual IO device handle.
 ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus_handle, &io_config, &io_handle));
 
 // --- 3. Configure and Initialize the Touch Controller ---
 esp_lcd_touch_config_t tp_cfg = {
 .x_max = LCD_H_RES,
 .y_max = LCD_V_RES,
 .rst_gpio_num = -1,//PIN_NUM_TOUCH_RST,
 .int_gpio_num = PIN_NUM_TOUCH_INT,
 .levels = {
 .reset = 0,
 .interrupt = 0,
 },
 .flags = {
 .swap_xy = 0,
 .mirror_x = 0,
 .mirror_y = 0,
 },
 };
 
 esp_lcd_touch_handle_t tp; // <-- Your handle is named 'tp'
 // Now you are passing the *valid* io_handle
 esp_err_t err = esp_lcd_touch_new_i2c_ft6x36(io_handle, &tp_cfg, &tp);
 
 if (err != ESP_OK) {
 FreeOSLogE(TAG, "Touch panel init failed, continuing without touch...");
 } else {
 FreeOSLogI(TAG, "Touch panel initialized successfully");
 
 // --- 4. Register Touch Input with LVGL ---
 FreeOSLogI(TAG, "Registering touch input with LVGL");
 
 lvgl_touch_indev = lv_indev_create();
 lv_indev_set_type(lvgl_touch_indev, LV_INDEV_TYPE_POINTER);
 lv_indev_set_read_cb(lvgl_touch_indev, lvgl_touch_cb);
 
 // *** FIX 2 (Typo) ***
 // Pass the handle you just created, which is 'tp', not 'tp_handle'
 lv_indev_set_user_data(lvgl_touch_indev, tp);
 
 // (Rest of your LVGL code...)
 }
 }*/

//Joystick

// Define the GPIOs and their corresponding ADC Channels
#define JOY_X_PIN  ADC_CHANNEL_3 // GPIO4 corresponds to ADC1 Channel 3
#define JOY_Y_PIN  ADC_CHANNEL_4 // GPIO5 corresponds to ADC1 Channel 4

// ADC handle and configuration structures
adc_oneshot_unit_handle_t adc1_handle;

#define DEFAULT_SCAN_LIST_SIZE CONFIG_EXAMPLE_SCAN_LIST_SIZE

#ifdef CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP
#define USE_CHANNEL_BITMAP 1
#define CHANNEL_LIST_SIZE 3
static uint8_t channel_list[CHANNEL_LIST_SIZE] = {1, 6, 11};
#endif /*CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP*/



static void print_auth_mode(int authmode)
{
    switch (authmode) {
        case WIFI_AUTH_OPEN:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_OPEN");
            break;
        case WIFI_AUTH_OWE:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_OWE");
            break;
        case WIFI_AUTH_WEP:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WEP");
            break;
        case WIFI_AUTH_WPA_PSK:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
            break;
        case WIFI_AUTH_WPA2_PSK:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
            break;
        case WIFI_AUTH_WPA_WPA2_PSK:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
            break;
        case WIFI_AUTH_ENTERPRISE:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
            break;
        case WIFI_AUTH_WPA3_PSK:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
            break;
        case WIFI_AUTH_WPA2_WPA3_PSK:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
            break;
        case WIFI_AUTH_WPA3_ENTERPRISE:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENTERPRISE");
            break;
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_ENTERPRISE");
            break;
        case WIFI_AUTH_WPA3_ENT_192:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
            break;
        default:
            FreeOSLogI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
            break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
        case WIFI_CIPHER_TYPE_NONE:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
            break;
        case WIFI_CIPHER_TYPE_WEP40:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
            break;
        case WIFI_CIPHER_TYPE_WEP104:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
            break;
        case WIFI_CIPHER_TYPE_TKIP:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
            break;
        case WIFI_CIPHER_TYPE_CCMP:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
            break;
        case WIFI_CIPHER_TYPE_TKIP_CCMP:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
            break;
        case WIFI_CIPHER_TYPE_AES_CMAC128:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
            break;
        case WIFI_CIPHER_TYPE_SMS4:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
            break;
        case WIFI_CIPHER_TYPE_GCMP:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
            break;
        case WIFI_CIPHER_TYPE_GCMP256:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
            break;
        default:
            FreeOSLogI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
            break;
    }
    
    switch (group_cipher) {
        case WIFI_CIPHER_TYPE_NONE:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
            break;
        case WIFI_CIPHER_TYPE_WEP40:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
            break;
        case WIFI_CIPHER_TYPE_WEP104:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
            break;
        case WIFI_CIPHER_TYPE_TKIP:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
            break;
        case WIFI_CIPHER_TYPE_CCMP:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
            break;
        case WIFI_CIPHER_TYPE_TKIP_CCMP:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
            break;
        case WIFI_CIPHER_TYPE_SMS4:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
            break;
        case WIFI_CIPHER_TYPE_GCMP:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
            break;
        case WIFI_CIPHER_TYPE_GCMP256:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
            break;
        default:
            FreeOSLogI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
            break;
    }
}

#ifdef USE_CHANNEL_BITMAP
static void array_2_channel_bitmap(const uint8_t channel_list[], const uint8_t channel_list_size, wifi_scan_config_t *scan_config) {
    
    for(uint8_t i = 0; i < channel_list_size; i++) {
        uint8_t channel = channel_list[i];
        scan_config->channel_bitmap.ghz_2_channels |= (1 << channel);
    }
}
#endif /*USE_CHANNEL_BITMAP*/


/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
#ifdef USE_CHANNEL_BITMAP
    wifi_scan_config_t *scan_config = (wifi_scan_config_t *)calloc(1,sizeof(wifi_scan_config_t));
    if (!scan_config) {
        FreeOSLogE(TAG, "Memory Allocation for scan config failed!");
        return;
    }
    array_2_channel_bitmap(channel_list, CHANNEL_LIST_SIZE, scan_config);
    esp_wifi_scan_start(scan_config, true);
    free(scan_config);
    
#else
    esp_wifi_scan_start(NULL, true);
#endif /*USE_CHANNEL_BITMAP*/
    
    FreeOSLogI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    FreeOSLogI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++) {
        FreeOSLogI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        FreeOSLogI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
        FreeOSLogI(TAG, "Channel \t\t%d", ap_info[i].primary);
    }
}



/**
 * @brief Initializes and mounts the FATFS partition using the Wear Leveling driver.
 */
void mount_fatfs(void) {
    // Configuration for the mount (Ensure CONFIG_WL_SECTOR_SIZE is set in menuconfig)
    /*const esp_vfs_fat_mount_config_t mount_config = {
     .max_files = 10000,
     .format_if_mount_failed = false, // Format if initial mount fails (enables writing)
     .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
     };*/
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 100
    };
    
    FreeOSLogI(TAG, "Mounting FATFS partition '%s' at %s...", PARTITION_LABEL, MOUNT_POINT);
    
    // Mount the FAT partition with Read/Write (RW) and Wear Leveling (WL) support
    /*esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(
     MOUNT_POINT,        // Base path (e.g., /fat)
     PARTITION_LABEL,    // Partition Label (e.g., fat_data)
     &mount_config,
     &wl_handle          // Wear Leveling handle (output)
     );*/
    
    esp_err_t ret = esp_vfs_fat_spiflash_mount_ro("/spiflash", "storage", &mount_config);
    
    if (ret != ESP_OK) {
        FreeOSLogE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(ret));
    } else {
        FreeOSLogI(TAG, "FATFS mounted successfully.");
    }
}


void checkAvailableMemory() {
    size_t free_heap_size = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    printf("Free heap size: %zu bytes\n", free_heap_size);
    size_t free_heap_size1 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    printf("Free heap size spiram: %zu bytes\n", free_heap_size1);
    size_t free_heap_size2 = heap_caps_get_free_size(MALLOC_CAP_EXEC);
    printf("Free heap size MALLOC_CAP_EXEC: %zu bytes\n", free_heap_size2);
    // Optionally, you can print other heap info
    size_t free_internal_heap_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    printf("Free internal heap size: %zu bytes\n", free_internal_heap_size);
    printf("Free heap size: %zu bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    
    FreeOSLogI(TAG, "Free heap size: %zu bytes\n", free_heap_size);
    FreeOSLogI(TAG, "Free internal heap size: %zu bytes\n", free_internal_heap_size);
    
    //FreeOSLogI(TAG, "EXECUTABLE 2\n");
    
}

// Function to draw a block of pixels (240x240)
void raw_drawing_task(void *pvParameter)
{
    FreeOSLogI(TAG, "Starting raw drawing demo.");
    
    // Create a pixel buffer for the entire screen (240 * 240 pixels * 2 bytes/pixel)
    // This is a large buffer, ensure your PSRAM is enabled/available.
    // If you don't have enough RAM/PSRAM, reduce the size or use a small tile.
    uint16_t *color_data = (uint16_t *)heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!color_data) {
        FreeOSLogE(TAG, "Failed to allocate display buffer!");
        vTaskDelete(NULL);
        return;
    }
    
    // Fill the buffer with the color red (0xF800)
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        color_data[i] = LCD_COLOR_RED;
    }
    
    // Draw the bitmap once per second
    while (1) {
        // Function: esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, color_data)
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
                                                  panel_handle,
                                                  0,                 // x_start
                                                  0,                 // y_start
                                                  LCD_H_RES,         // x_end (Exclusive, so 240 is 0-239)
                                                  LCD_V_RES,         // y_end (Exclusive, so 240 is 0-239)
                                                  color_data
                                                  ));
        
        FreeOSLogI(TAG, "Drew a red screen.");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    // Note: In a real app, you would free the memory before deleting the task.
}

/*void lvgl_ui_task(void *pvParameter) {
 // *** LVGL UI Creation Code Goes Here ***
 lv_obj_t *label = lv_label_create(lv_screen_active());
 lv_label_set_text(label, "LVGL is Running!");
 lv_obj_center(label);
 
 // LVGL main loop handler
 while(1) {
 lv_timer_handler(); // Processes all LVGL tasks and redrawing
 vTaskDelay(pdMS_TO_TICKS(5)); // Wait 5ms
 }
 }*/


// Style for small, normal weight text (e.g., 18px)
static lv_style_t style_normal;

// Style for large, bold/heavy text (e.g., 36px)
static lv_style_t style_heavy;

// Style for medium, light/thin text (e.g., 24px)
static lv_style_t style_light;

/**
 * @brief Initialize all custom LVGL styles.
 */
static void init_custom_styles()
{
    // --- Normal Style (Montserrat 18) ---
    lv_style_init(&style_normal);
    // Use the built-in Montserrat font with size 18 and a medium/normal weight.
    lv_style_set_text_font(&style_normal, &lv_font_montserrat_8);
    lv_style_set_text_color(&style_normal, lv_color_white());
    
    // --- Heavy Style (Montserrat 36) ---
    lv_style_init(&style_heavy);
    // Use the largest, heaviest weight available (Montserrat 36)
    lv_style_set_text_font(&style_heavy, &lv_font_montserrat_10);
    lv_style_set_text_color(&style_heavy, lv_color_hex(0xFFDD00)); // Yellowish
    
    // --- Light Style (Montserrat 24) ---
    lv_style_init(&style_light);
    // Use Montserrat 24. Since LVGL doesn't have explicit 'light' weight files
    // for all sizes, we use the default and often customize it further if needed.
    // However, 24px is a distinct size from 18 and 36.
    lv_style_set_text_font(&style_light, &lv_font_montserrat_12);
    lv_style_set_text_color(&style_light, lv_color_hex(0xAAAAAA)); // Gray
}


// -----------------------------------------------------------
// 2. UI TASK FUNCTION
// -----------------------------------------------------------

/**
 * @brief LVGL UI creation and handler task.
 */
void lvgl_ui_task(void *pvParameter)
{
    FreeOSLogI(TAG, "LVGL UI Task started.");
    
    // Lock the LVGL mutex before accessing any LVGL objects
    lvgl_port_lock(0);
    
    // 1. Initialize styles
    init_custom_styles();
    
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x002244), 0); // Dark Blue background
    
    // --- LABEL 1: Large & Heavy ---
    lv_obj_t *label1 = lv_label_create(scr);
    lv_label_set_text(label1, "SYSTEM ONLINE");
    lv_obj_add_style(label1, &style_heavy, 0); // Apply the heavy style
    lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 10);
    
    // --- LABEL 2: Medium & Light ---
    lv_obj_t *label2 = lv_label_create(scr);
    lv_label_set_text(label2, "Meet with sales team");
    lv_obj_add_style(label2, &style_light, 0); // Apply the light style
    // Align below Label 1
    lv_obj_align_to(label2, label1, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    // --- LABEL 3: Small & Normal ---
    lv_obj_t *label3 = lv_label_create(scr);
    lv_label_set_text(label3, "Powered by ESP-IDF and LVGL");
    lv_obj_add_style(label3, &style_normal, 0); // Apply the normal style
    // Align below Label 2
    lv_obj_align_to(label3, label2, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    // Unlock the LVGL mutex
    lvgl_port_unlock();
    
    // -----------------------------------------------------------
    // LVGL Main Loop
    // -----------------------------------------------------------
    while(1) {
        // Must protect lv_timer_handler() with lock/unlock
        lvgl_port_lock(0);
        lv_timer_handler(); // Processes all LVGL tasks, redrawing, and animations
        lvgl_port_unlock();
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


// Styles (reusing simple styles, assume default LVGL fonts are available)
static lv_style_t style_main_bg;

/**
 * @brief Initializes base styles for the demo.
 */
static void init_kb_styles()
{
    lv_style_init(&style_main_bg);
    // Set background color for the screen (or container)
    lv_style_set_bg_color(&style_main_bg, lv_color_hex(0xEEEEEE));
    lv_style_set_text_color(&style_main_bg, lv_color_black());
}

// Global variables to store the latest joystick values (access must be thread-safe)
volatile int g_joystick_x = 0;
volatile int g_joystick_y = 0;

// Mutex to protect access to the global variables
SemaphoreHandle_t joystick_mutex;

lv_obj_t *g_cursor_obj;
volatile lv_coord_t g_cursor_x_pos; // Manual cursor position tracker
volatile lv_coord_t g_cursor_y_pos; // Manual cursor position tracker

// Joystick ADC Range
#define JOY_X_MIN 1000
#define JOY_X_MAX 3200
#define JOY_Y_MIN 600
#define JOY_Y_MAX 3600

#define JOY_X_LEFT 1100 //200 to 1100, move left if less than 1100
#define JOY_X_NEUTRAL 1600 //1500 to 2400, do not move cursor on x axis if joystick x between these values
#define JOY_X_RIGHT 2600 //2800 to 3600, move right if greater than 2800
#define JOY_Y_DOWN 1400 //600 to 1200, move down if less than 1200
#define JOY_Y_NEUTRAL 1800 //1700 to 2600, do not move cursor on y axis if joystick y between these values
#define JOY_Y_UP 2800 //2800 to 4000, move up if greater than 2800

// Screen Resolution (Adjust these to your actual display size)
#define SCREEN_WIDTH 240  // Example: 320 pixels wide
#define SCREEN_HEIGHT 320 // Example: 240 pixels high

/**
 * @brief LVGL UI creation and handler task, focusing on the keyboard.
 */
void lvgl_ui_task1(void *pvParameter)
{
    FreeOSLogI(TAG, "LVGL Keyboard Demo Task started.");
    
    //lvgl_port_lock(0);
    lv_lock();
    init_kb_styles();
    
    lv_obj_t *scr = lv_screen_active();
    lv_obj_add_style(scr, &style_main_bg, 0);
    
    // 1. Create a Text Area (Input Field)
    // This is where the user's input will appear.
    lv_obj_t *ta = lv_textarea_create(scr);
    lv_textarea_set_text_selection(ta, true);
    
    // Set the size and position of the text area
    lv_obj_set_width(ta, lv_pct(90));
    lv_obj_set_height(ta, LV_SIZE_CONTENT);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 10);
    
    lv_textarea_set_placeholder_text(ta, "Tap here to type...");
    lv_textarea_set_one_line(ta, true); // Keep it simple
    
    // 2. Create the Keyboard
    // This creates the virtual keyboard widget.
    lv_obj_t *kb = lv_keyboard_create(scr);
    
    // Set the size: full width and align it to the bottom
    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_height(kb, lv_pct(50));
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // 3. Link the Keyboard to the Text Area
    // This tells the keyboard where to send its input.
    lv_keyboard_set_textarea(kb, ta);
    
    // 4. Set Initial Focus
    // Set the text area as the initially focused object so the keyboard is ready.
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    
    // Global handle for the cursor object (declared outside of the function)
    g_cursor_obj = NULL;
    
    // --- Cursor Style ---
    lv_style_t style_cursor;
    lv_style_init(&style_cursor);
    lv_style_set_bg_color(&style_cursor, lv_color_white()); // Small white circle
    lv_style_set_border_width(&style_cursor, 2);
    lv_style_set_border_color(&style_cursor, lv_color_black()); // Black border
    lv_style_set_radius(&style_cursor, LV_RADIUS_CIRCLE); // Makes it a perfect circle
    lv_style_set_pad_all(&style_cursor, 0); // No padding
    
    // --- Cursor Object Creation ---
    g_cursor_obj = lv_obj_create(scr); // Create as a simple object on the screen
    lv_obj_set_size(g_cursor_obj, 10, 10); // Set size (e.g., 10x10 pixels)
    lv_obj_add_style(g_cursor_obj, &style_cursor, 0);
    lv_obj_set_pos(g_cursor_obj, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2); // Start at the center
    lv_obj_move_foreground(g_cursor_obj); // Ensures the cursor is drawn on top
    
    
    //lvgl_port_unlock();
    lv_unlock();
    
    //vTaskDelete(NULL);
    
    // LVGL Main Loop
    while(1) {
        lv_lock();
        lv_timer_handler();
        lv_unlock();
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    // LVGL Main Loop
    // LVGL Main Loop
    //while(1) {
    /*
     // --- 1. Read Joystick Position Safely (RAW VALUES) ---
     int raw_x = g_joystick_x; // Initialize with current global value
     int raw_y = g_joystick_y;
     
     // Safely retrieve the latest global values
     if (xSemaphoreTake(joystick_mutex, QUEUE_MAX_DELAY) == pdTRUE) {
     raw_x = g_joystick_x;
     raw_y = g_joystick_y;
     xSemaphoreGive(joystick_mutex);
     }
     
     // --- 2. Rate Control Logic (Analog to Digital Step) ---
     int move_x = 0;
     int move_y = 0;
     
     // --- X-Axis (Horizontal) ---
     if (raw_x < JOY_X_LEFT) {
     move_x = -1; // Move Left
     } else if (raw_x > JOY_X_RIGHT) {
     move_x = 1;  // Move Right
     }
     
     // --- Y-Axis (Vertical) ---
     // Note: LVGL Y=0 is TOP. Y_UP (high ADC) should move cursor toward Y=0 (move_y = -1).
     if (raw_y > JOY_Y_UP) {
     move_y = -1; // Move UP (towards Y=0)
     } else if (raw_y < JOY_Y_DOWN) {
     move_y = 1;  // Move DOWN (towards Y=HEIGHT)
     }
     
     // --- 3. Acquire the LVGL Lock, Update Position, and Handle Tick ---
     lvgl_port_lock(0);
     
     if (g_cursor_obj != NULL) {
     
     // Check if movement is needed
     if (move_x != 0 || move_y != 0) {
     
     // Update the cursor's internal tracked position
     g_cursor_x_pos += move_x;
     g_cursor_y_pos += move_y;
     
     // Clamp position to screen boundaries (ensures cursor stays visible)
     const int cursor_center_offset = 5; // For a 10x10 cursor
     if (g_cursor_x_pos < cursor_center_offset) g_cursor_x_pos = cursor_center_offset;
     if (g_cursor_x_pos > SCREEN_WIDTH - cursor_center_offset) g_cursor_x_pos = SCREEN_WIDTH - cursor_center_offset;
     if (g_cursor_y_pos < cursor_center_offset) g_cursor_y_pos = cursor_center_offset;
     if (g_cursor_y_pos > SCREEN_HEIGHT - cursor_center_offset) g_cursor_y_pos = SCREEN_HEIGHT - cursor_center_offset;
     
     // Set the LVGL object position
     lv_obj_set_pos(g_cursor_obj, g_cursor_x_pos - cursor_center_offset, g_cursor_y_pos - cursor_center_offset);
     }
     }
     
     lv_timer_handler(); // Handle the LVGL tick
     
     lvgl_port_unlock();
     
     // Control cursor speed by yielding (5ms delay = 200 updates per second)
     vTaskDelay(pdMS_TO_TICKS(5));*/
    
    //lv_timer_handler();
    //vTaskDelay(pdMS_TO_TICKS(5));
    //}
    
}



void joystick_task(void *pvParameter)
{
    // --- 1. ADC Unit Initialization ---
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1, // Use ADC1
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));
    
    // --- 2. ADC Channel Configuration (X-Axis: GPIO4) ---
    adc_oneshot_chan_cfg_t chan_config_x = {
        .atten = ADC_ATTEN_DB_11, // Attenuation for 0V ~ 3.3V range
        .bitwidth = ADC_BITWIDTH_DEFAULT, // Default is 12-bit (0-4095)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, JOY_X_PIN, &chan_config_x));
    
    // --- 3. ADC Channel Configuration (Y-Axis: GPIO5) ---
    adc_oneshot_chan_cfg_t chan_config_y = {
        .atten = ADC_ATTEN_DB_11, // Same attenuation
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, JOY_Y_PIN, &chan_config_y));
    
    joystick_mutex = xSemaphoreCreateMutex();
    
    while (1) {
        int adc_raw_x = 0;
        int adc_raw_y = 0;
        
        // Read the joystick values
        // Note: Check for ESP_OK error return here for production code
        adc_oneshot_read(adc1_handle, JOY_X_PIN, &adc_raw_x);
        adc_oneshot_read(adc1_handle, JOY_Y_PIN, &adc_raw_y);
        
        // =======================================================
        // ✨ NEW LINE ADDED HERE TO GENERATE LOG OUTPUT
        // =======================================================
        FreeOSLogI(TAG, "X-Axis Raw: %d | Y-Axis Raw: %d", adc_raw_x, adc_raw_y);
        // =======================================================
        
        // Safely update global variables
        if (xSemaphoreTake(joystick_mutex, QUEUE_MAX_DELAY) == pdTRUE) {
            g_joystick_x = adc_raw_x;
            g_joystick_y = adc_raw_y;
            xSemaphoreGive(joystick_mutex);
        }
        
        // Delay to prevent the task from consuming too much CPU time
        vTaskDelay(pdMS_TO_TICKS(50)); // Read the joystick every 50ms
    }
}

void initializeUIST7789(void) {
    // 1. Initialize SPI Bus
    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0, // Set to 0 to allow max size transfer
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));
    
    // 2. Initialize LCD Panel I/O (Interface to the ST7789)
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_CS,
        .dc_gpio_num = PIN_NUM_DC,
        .spi_mode = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ, // Corrected clock field name
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));
    
    // 3. Initialize LCD Driver
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .bits_per_pixel = 16,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
    };
    
    const size_t ili9488_buffer_size = 0;
    // Conditional compilation for the driver initialization
#if defined(USE_ILI9488)
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io_handle, &panel_config, ili9488_buffer_size, &panel_handle));
    
#elif defined(USE_ST7789)
    // The ST7789 function does not take the buffer_size argument
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
#endif
    
    // 4. Panel Initialization (Reset and basic setup commands)
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_swap_xy(panel_handle, true); // Often needed for orientation
    esp_lcd_panel_set_gap(panel_handle, 0, 0);
#if defined(USE_ST7789)
    esp_lcd_panel_invert_color(panel_handle, true);
#endif
    esp_lcd_panel_disp_on_off(panel_handle, true);
    
    
#define LVGL_DISP_BUF_SIZE  (LCD_H_RES * LCD_V_RES / 10) // E.g., 1/10th of screen
    
    // 1. Initialize LVGL Core
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    // Use the port init function to set up the LVGL task and tick source
    lvgl_port_init(&lvgl_cfg);
    
    // 2. Register Display with LVGL Porting Layer
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .control_handle = NULL, // Generally unused for basic SPI display
        
        // --- Memory Configuration ---
        // LVGL will allocate the buffer internally based on these fields.
        .buffer_size = LVGL_DISP_BUF_SIZE,
        .double_buffer = true, // We will allocate two buffers of the size above
        .trans_size = 0,       // Optional: Set to 0 if not using a dedicated transaction buffer
        
        // --- Flags (Crucial for Byte Ordering) ---
        .flags.buff_dma = 1,
        // Set this flag to 1 (true) to swap the high and low bytes of the 16-bit color.
        // This often resolves the "fuzziness" and color mixing issues on SPI displays.
        
#if defined(USE_ILI9488)
        .flags.swap_bytes = 0,
#elif defined(USE_ST7789)
        .flags.swap_bytes = 1,
#endif
        
        // --- Display Resolution ---
        .hres = LCD_V_RES,
        .vres = LCD_H_RES,
        .monochrome = false,
        
        // --- LVGL v9 Configuration ---
        .color_format = LV_COLOR_FORMAT_RGB565, // Use RGB565 for 16-bit color
        
        // --- Flags (Crucial for DMA and memory location) ---
        .flags.buff_dma = 1,    // 1: Allocate buffers in DMA-capable memory (DRAM)
        .flags.buff_spiram = 0, // 0: Do NOT allocate in PSRAM
        .flags.sw_rotate = 0,   // 0: Use hardware/driver rotation (faster)
        .flags.full_refresh = 0, // 0: Use partial refresh (more efficient)
        .flags.direct_mode = 0, // 0: Use standard buffered mode
        
        .rotation = {0}, // Use default rotation (0 degrees)
    };
    
    // Use the registration function to wire it up
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    
    
    
    // 3. Start your LVGL UI Task
    xTaskCreate(lvgl_ui_task1, "LVGL_UI_Task", 8192, NULL, 5, NULL);
}

#include "driver/i2c_master.h" // <-- Use this new header

// Make sure these are defined correctly for your board
#define I2C_MASTER_SCL_IO   PIN_NUM_I2C_SCL
#define I2C_MASTER_SDA_IO   PIN_NUM_I2C_SDA
#define I2C_MASTER_NUM      I2C_HOST // Use the same I2C port

/**
 * @brief i2c master scanner (using new "next-gen" driver)
 */
static void i2c_scan_ng(void)
{
    FreeOSLogI(TAG, "Starting I2C scan (NG Driver)...");
    
    // 1. Create the bus configuration
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));
    
    FreeOSLogI(TAG, "Scanning for I2C devices...");
    
    for (uint8_t address = 1; address < 127; address++) {
        // 2. Probe the address. This function sends the address and checks for an ACK.
        // 50ms timeout is more than enough.
        esp_err_t ret = i2c_master_probe(bus_handle, address, 50);
        
        if (ret == ESP_OK) {
            FreeOSLogI(TAG, "I2C device found at address 0x%02X", address);
        } else if (ret != ESP_ERR_TIMEOUT && ret != ESP_ERR_NOT_FOUND) {
            // Log other errors if they occur
            ESP_LOGW(TAG, "Error 0x%X checking address 0x%02X", ret, address);
        }
    }
    
    // 3. Clean up the bus
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    FreeOSLogI(TAG, "I2C scan complete.");
}



/**
 * @brief GPIO Interrupt Service Routine
 * This is called *every* time the INT_N_PIN goes LOW.
 */
static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    if (gpio_num == INT_N_PIN) {
        // Send an event to the touch task
        // A non-zero value is posted to wake up the task
        static uint32_t event = 1;
        QueueSendFromISR(touch_intr_queue, &event, NULL);
    }
}

/**
 * @brief Task to process touch events
 * This task waits for an event from the ISR queue.
 */
static void touch_task(void *arg) {
    uint32_t event;
    FT6336U_TouchPointType touch_data;
    
    while (1) {
        // Wait for an interrupt event from the queue
        if (xQueueReceive(touch_intr_queue, &event, QUEUE_MAX_DELAY)) {
            
            // Interrupt received, scan the touch controller
            esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
            if (ret != ESP_OK) {
                FreeOSLogE(TAG, "Failed to scan touch: %s", esp_err_to_name(ret));
                continue;
            }
            
            // --- LVGL BRIDGE: UPDATE STATE ---
            // Instead of logging, we now update the static variables
            if (touch_data.touch_count == 0) {
                g_last_state = LV_INDEV_STATE_REL; // Released
            } else {
                // Use the first touch point
                g_last_x = touch_data.tp[0].x;
                g_last_y = touch_data.tp[0].y;
                g_last_state = LV_INDEV_STATE_PR; // Pressed
            }
            // --- END LVGL BRIDGE ---
            
            if (touch_data.touch_count == 0) {
                // FreeOSLogI(TAG, "Touch Released");
            } else {
                for (int i = 0; i < touch_data.touch_count; i++) {
                    FreeOSLogI(TAG, "Touch %d: (%s) X=%u, Y=%u",
                             i,
                             (touch_data.tp[i].status == touch) ? "NEW" : "STREAM",
                             touch_data.tp[i].x,
                             touch_data.tp[i].y);
                }
            }
        }
    }
}

/**
 * @brief Initialize the I2C master bus
 */
/*esp_err_t i2c_master_init(void) {
 i2c_master_bus_config_t i2c_bus_config = {
 .i2c_port = I2C_HOST,
 .sda_io_num = I2C_SDA_PIN,
 .scl_io_num = I2C_SCL_PIN,
 .clk_source = I2C_CLK_SRC_DEFAULT,
 .glitch_ignore_cnt = 7, // Corrected: Was .glitch_ignore_cfg.flags.val = 0
 .intr_priority = 0,     // Use 0 for default priority
 .flags.enable_internal_pullup = true // Enable internal pullups
 // Removed non-existent fields:
 // .scl_pulse_period_us
 // .sda_pulse_period_us
 // .intr_flags
 };
 
 esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
 if (ret != ESP_OK) {
 FreeOSLogE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
 }
 return ret;
 }
 */

/**
 * @brief Initializes just the FT6336U chip
 */
esp_err_t ft6336u_driver_init(void) {
    // 1. Initialize I2C Bus
    ESP_ERROR_CHECK(i2c_master_init());
    
    // 2. Initialize FT6336U Driver
    ESP_ERROR_CHECK(ft6336u_init(&touch_dev, i2c_bus_handle, RST_N_PIN, INT_N_PIN));
    
    touch_intr_queue = QueueCreate(10, sizeof(uint32_t));
    
    FreeOSLogI(TAG, "Touch driver initialized successfully.");
    
    // 5. Configure GPIO interrupt for INT_N_PIN
    // We do this *after* the driver is init (which configures the pin)
    // We re-configure to add the interrupt
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << INT_N_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // Trigger on FALLING edge
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // 6. Install GPIO ISR service and add handler
    ESP_ERROR_CHECK(gpio_install_isr_service(0)); // 0 = default flags
    ESP_ERROR_CHECK(gpio_isr_handler_add(INT_N_PIN, gpio_isr_handler, (void *)INT_N_PIN));
    
    FreeOSLogI(TAG, "Interrupt handler installed. Ready for touch.");
    return ESP_OK;
}

/* * This function should be called ONCE during your setup,
 * right after lv_init() and after your display driver is registered.
 */
void lvgl_touch_driver_init(void) {
    // 1. Create a new input device
    lv_indev_t *indev_touchpad = lv_indev_create();
    
    // 2. Set its type to "Pointer" (e.g., a mouse or touchscreen)
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    
    // 3. THIS IS THE CRITICAL LINE:
    //    Register your C function (lvgl_touch_read_cb) as the
    //    official callback that LVGL will use to read input.
    lv_indev_set_read_cb(indev_touchpad, lvgl_touch_read_cb);
}

void initTouch(void) {
    FreeOSLogI(TAG, "Starting FT6336U Touch Example");
    
    // 1. Initialize I2C Bus
    ESP_ERROR_CHECK(i2c_master_init());
    
    // 2. Initialize FT6336U Driver
    // We pass the bus handle, and the driver adds itself as a device
    ESP_ERROR_CHECK(ft6336u_init(&touch_dev, i2c_bus_handle, RST_N_PIN, INT_N_PIN));
    
    FreeOSLogI(TAG, "Touch driver initialized successfully.");
    
    // 3. Create the interrupt queue
    touch_intr_queue = QueueCreate(10, sizeof(uint32_t));
    
    // 4. Create the touch processing task
    xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, NULL);
    
    // 5. Configure GPIO interrupt for INT_N_PIN
    // We do this *after* the driver is init (which configures the pin)
    // We re-configure to add the interrupt
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << INT_N_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // Trigger on FALLING edge
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // 6. Install GPIO ISR service and add handler
    ESP_ERROR_CHECK(gpio_install_isr_service(0)); // 0 = default flags
    ESP_ERROR_CHECK(gpio_isr_handler_add(INT_N_PIN, gpio_isr_handler, (void *)INT_N_PIN));
    
    FreeOSLogI(TAG, "Interrupt handler installed. Ready for touch.");
}


// --- PIN CONFIGURATION ---
#define I2S_BCK_IO      GPIO_NUM_39
#define I2S_WS_IO       GPIO_NUM_40 // Also known as LRCK
#define I2S_DO_IO       GPIO_NUM_41 // Also known as DIN

void i2s_task(void *args)
{
    // 1. BUFFER SETUP
    // Create a buffer for stereo samples (Left + Right)
    // 16-bit depth * 2 channels * 1024 samples
    size_t buffer_len = 1024;
    int16_t *samples = (int16_t *)calloc(buffer_len, 2 * sizeof(int16_t));
    
    // Fill buffer with a Sine Wave
    // We pre-calculate one buffer and play it repeatedly for this test
    for (int i = 0; i < buffer_len; i++) {
        float t = (float)i / SAMPLE_RATE;
        int16_t val = (int16_t)(3000.0f * sinf(2.0f * PI * WAVE_FREQ_HZ * t));
        
        // Interleaved Stereo: [Left, Right, Left, Right...]
        samples[i * 2]     = val; // Left Channel
        samples[i * 2 + 1] = val; // Right Channel
    }

    size_t bytes_written = 0;

    while (1) {
        // Write the buffer to the I2S peripheral
        // This function blocks until the DMA buffer has space
        i2s_channel_write(tx_handle, samples, buffer_len * 2 * sizeof(int16_t), &bytes_written, QUEUE_MAX_DELAY);
    }
}

void createI2STask() {
    // 2. CHANNEL CONFIGURATION
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

        // 3. STANDARD I2S MODE CONFIGURATION (Philips Format)
        i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED, // We are grounding SCK on the PCM5102, so MCLK is not needed
                .bclk = I2S_BCK_IO,
                .ws = I2S_WS_IO,
                .dout = I2S_DO_IO,
                .din = I2S_GPIO_UNUSED,  // We are only outputting audio
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };

        // 4. INITIALIZE AND ENABLE
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

        printf("I2S initialized on BCK:%d, LRCK:%d, DIN:%d\n", I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO);

        // 5. START TASK
        xTaskCreate(i2s_task, "i2s_task", 4096, NULL, 5, NULL);
}



// Define your pins here
#define PIN_NUM_CLK  14
#define PIN_NUM_CMD  15
#define PIN_NUM_D0   16
#define PIN_NUM_D1   17
#define PIN_NUM_D2   18
#define PIN_NUM_D3   13

/*
 // --- 2. SD Card (SDIO 4-bit) Pin Definitions ---
 #define SDIO_PIN_NUM_CLK      5
 #define SDIO_PIN_NUM_CMD      6
 #define SDIO_PIN_NUM_D0       7
 #define SDIO_PIN_NUM_D1       8
 #define SDIO_PIN_NUM_D2       9
 #define SDIO_PIN_NUM_D3       10
 */

void mount_sd_card()
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Use settings defined above to initialize SD card and mount FATFS.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing production applications.
    
    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";
    
    // By default, SDMMC host frequency is set to 20MHz.
    // If your wires are long (>10cm), consider lowering this to 10MHz or 400kHz.
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    //host.max_freq_khz = 100;

    // This is where we configure the pins
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    //slot_config.width = 4; // Using 4-bit mode
    slot_config.width = 1; // Using 4-bit mode
    
    // MAPPING THE PINS
    slot_config.clk = (gpio_num_t)PIN_NUM_CLK;
    slot_config.cmd = (gpio_num_t)PIN_NUM_CMD;
    slot_config.d0  = (gpio_num_t)PIN_NUM_D0;
    slot_config.d1  = (gpio_num_t)PIN_NUM_D1;
    slot_config.d2  = (gpio_num_t)PIN_NUM_D2;
    slot_config.d3  = (gpio_num_t)PIN_NUM_D3;
    slot_config.gpio_cd = GPIO_NUM_21;

    // CRITICAL: Enable internal pull-ups if your breakout board doesn't have them
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    printf("Mounting filesystem...\n");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount filesystem. \n");
        } else {
            printf("Failed to initialize the card (%s). \n", esp_err_to_name(ret));
        }
        return;
    }

    printf("Filesystem mounted\n");
    sdmmc_card_print_info(stdout, card);
}

static i2c_master_bus_handle_t audio_i2c_bus_handle;

//#include "driver/i2c_master.h"
//#include "esp_log.h"

void check_pins_1_and_2(void) {
    const char *TAG = "I2C_CHECK";
    ESP_LOGW(TAG, "--- DIAGNOSTIC: Checking Pins 1 (SCL) and 2 (SDA) ---");

    // 1. Configure the Bus specifically for these pins
    i2c_master_bus_config_t conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,          // Auto-select port
        .scl_io_num = 1,         // <--- Pin 1
        .sda_io_num = 2,         // <--- Pin 2
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, // Attempt to use internal pullups
    };

    i2c_master_bus_handle_t bus_handle;
    esp_err_t err = i2c_new_master_bus(&conf, &bus_handle);

    if (err != ESP_OK) {
        FreeOSLogE(TAG, "CRITICAL: Could not init I2C on Pins 1 & 2. Error: %s", esp_err_to_name(err));
        return;
    }

    // 2. Run the Scan
    int devices_found = 0;
    for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
        // Probe every address
        if (i2c_master_probe(bus_handle, addr, 50) == ESP_OK) {
            FreeOSLogI(TAG, "  -> SUCCESS: Found Device at 0x%02X", addr);
            devices_found++;
        }
    }

    // 3. Report Results
    if (devices_found == 0) {
        FreeOSLogE(TAG, "FAILURE: No devices found. Pins 1 & 2 are dead or wired incorrectly.");
    } else {
        FreeOSLogI(TAG, "PASSED: Found %d device(s) on Pins 1 & 2.", devices_found);
    }

    // 4. Clean up (Release pins so your real code can use them)
    i2c_del_master_bus(bus_handle);
    ESP_LOGW(TAG, "--- DIAGNOSTIC COMPLETE ---");
}

// Add these defines if not already in your file
#define ES8311_ADDR 0x18

void initEs8311(void) {
    FreeOSLogI("AUDIO", "Initializing ES8311...");

    // 1. Create the I2C Bus Handle (Standard for IDF 5.x)
    // We recreate the bus here because the diagnostic tool closed its temporary one.
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = -1,
        .sda_io_num = 2,  // GPIO 2
        .scl_io_num = 1,  // GPIO 1
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };
    
    // Create the global handle (make sure 'audio_i2c_bus_handle' is defined at top of file)
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &audio_i2c_bus_handle));

    // 2. Create the Device Handle
    es8311_handle_t es_handle = es8311_create(audio_i2c_bus_handle, ES8311_ADDR);

    // 3. Configure Clock (CRITICAL FIX: frequency MUST be set)
    es8311_clock_config_t clk_cfg = {
        .mclk_from_mclk_pin = true,
        .sample_frequency = 44100,
        .mclk_frequency = 11289600 // 44100 * 256
    };

    ESP_ERROR_CHECK(es8311_init(es_handle, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));

    // 4. --- THE SILENCE FIXES ---
    
    // A. Force Mic Bias ON (Powers the microphone element)
    uint8_t reg14;
    es8311_read_reg(es_handle, 0x14, &reg14);
    reg14 |= (1 << 4);
    es8311_write_reg(es_handle, 0x14, reg14);

    // B. Force Max Gain (So we can hear even faint noise)
    es8311_write_reg(es_handle, 0x16, 0xBB); // Analog Gain (+30dB)
    es8311_write_reg(es_handle, 0x17, 0xFF); // Digital Volume (0dB / Max)

    // C. Select the Correct Input Pin (THE USUAL SUSPECT)
    // Most modules use Input 1 (0x00). Some use Input 2 (0x50).
    // We will try Input 1 first. If silent, change this to 0x50.
    //es8311_write_reg(es_handle, 0x44, 0x00);
    // C. Select Input 2 (Common for breakout boards)
    // Change 0x00 to 0x50
    es8311_write_reg(es_handle, 0x44, 0x50);

    // D. Force ADC Unmute (Register 0x15)
    // Bit 6 controls mute. We must clear it to 0.
    uint8_t reg15;
    es8311_read_reg(es_handle, 0x15, &reg15);
    reg15 &= ~(1 << 6); // Clear Bit 6
    es8311_write_reg(es_handle, 0x15, reg15);

    // D. Unmute Everything
    es8311_voice_mute(es_handle, false);
    es8311_microphone_config(es_handle, false); // False = Analog Mic

    FreeOSLogI("AUDIO", "ES8311 Init Complete. Gain set to MAX.");
}

void debug_microphone_task(void *args) {
    ESP_LOGW("MIC_DEBUG", "Starting Microphone Monitor... (Check Serial Plotter!)");

    size_t chunk_size = 1024; // Bytes
    int16_t *buffer = cc_safe_alloc(1, chunk_size);
    size_t bytes_read = 0;

    while (1) {
        // 1. Read Raw Data from I2S
        // Ensure 'rx_handle' is the global handle you created in setupHybridAudio
        if (i2s_channel_read(rx_handle, buffer, chunk_size, &bytes_read, 1000) == ESP_OK) {
            
            // 2. Process the Data (Find Peak Volume)
            int32_t sum_left = 0;
            int32_t max_left = 0;
            int32_t max_right = 0;
            
            int samples = bytes_read / 2; // Total 16-bit samples
            
            // Iterate by 2 because we are in Stereo (Left, Right, Left, Right...)
            for (int i = 0; i < samples; i += 2) {
                int16_t left_val = buffer[i];
                int16_t right_val = buffer[i+1];

                if (abs(left_val) > max_left) max_left = abs(left_val);
                if (abs(right_val) > max_right) max_right = abs(right_val);
            }

            // 3. Print Results (Use Serial Plotter for cool graphs)
            // If these numbers are > 50, your mic is working.
            // If these numbers are 0, your mic is dead.
            FreeOSLogI("MIC_DEBUG", "Left Level: %ld  |  Right Level: %ld", (long)max_left, (long)max_right);
        } else {
            FreeOSLogE("MIC_DEBUG", "I2S Read Failed!");
        }
        
        // Slow down the logs so you can read them
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    free(buffer);
    vTaskDelete(NULL);
}


void initializeI2S(void) {
    // --- STEP 1: INITIALIZE I2S (The "Road") ---
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
        
        // DMA Buffer Tuning for 16-bit
        // 16-bit stereo frame = 4 bytes.
        // 1024 frames * 4 bytes = 4096 bytes (This fits in the DMA limit!)
        chan_cfg.dma_desc_num = 8;
        chan_cfg.dma_frame_num = 1023;

        tx_handle = NULL;
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

        i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
            // --- CHANGE: Set to 16-bit ---
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = GPIO_NUM_42,
                .bclk = GPIO_NUM_39,
                .ws = GPIO_NUM_40,
                .dout = GPIO_NUM_41,
                .din = GPIO_NUM_38,
            },
        };

        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

void testGraphics(void) {
    
}

// --- Configuration ---
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE // S3 only uses Low Speed Mode
#define LEDC_OUTPUT_IO          (47)                // Your requested Pin
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT   // 8192 discrete levels of brightness
#define LEDC_FREQUENCY          (4000)              // 4kHz (Safe for CN5711)

// --- Initialization Function ---
void example_ledc_init(void)
{
    // 1. Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set frequency of PWM signal
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0% initially
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// --- The Dedicated Task ---
void pwm_fade_task(void *pvParameters)
{
    uint32_t max_duty = 8191; // 2^13 - 1
    int fade_step = 100;      // How much to jump per cycle
    int delay_ms = 10;        // Speed of the fade

    FreeOSLogI(TAG, "Starting PWM Fade Loop...");

    while (1) {
        // Fade UP
        for (int duty = 0; duty <= max_duty; duty += fade_step) {
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

        // Fade DOWN
        for (int duty = max_duty; duty >= 0; duty -= fade_step) {
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

        // Wait at bottom (Off) for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// At top of file
volatile float g_battery_level = 0.0f;

// --- CONFIGURATION ---
#define ADC_UNIT          ADC_UNIT_2
#define ADC_CHANNEL       ADC_CHANNEL_7    // GPIO 34 on original ESP32
#define ADC_ATTEN         ADC_ATTEN_DB_12
#define VOLTAGE_DIVIDER   2.0f

static const char *TAGBattery = "BATTERY_TASK";

// Represents a point on the battery curve: {Voltage, Percentage}
typedef struct {
    float voltage;
    int percentage;
} BatteryPoint;

// Standard Li-Ion Discharge Curve (approximate)
// You can tweak these voltages based on your real-world testing
static const BatteryPoint curve[] = {
    {4.20, 100},
    {4.10, 90},
    {4.00, 80},
    {3.90, 70},
    {3.80, 60},
    {3.70, 50}, // Nominal voltage is usually around 50%
    {3.65, 40},
    {3.60, 30}, // Drops quickly after this
    {3.50, 20},
    {3.40, 10},
    {3.20, 0}   // Cutoff
};

#define POINTS_COUNT (sizeof(curve) / sizeof(curve[0]))

uint8_t get_battery_percentage(float voltage)
{
    // 1. Clamp extremely high/low values
    if (voltage >= curve[0].voltage) return 100;
    if (voltage <= curve[POINTS_COUNT - 1].voltage) return 0;

    // 2. Loop through the table to find the range we are in
    for (int i = 0; i < POINTS_COUNT - 1; i++) {
        // Find the two points sandwiching our current voltage
        if (voltage <= curve[i].voltage && voltage > curve[i + 1].voltage) {
            
            float v1 = curve[i].voltage;
            float v2 = curve[i + 1].voltage;
            int p1 = curve[i].percentage;
            int p2 = curve[i + 1].percentage;

            // 3. Linear Interpolation (Math to smooth the gap)
            // This prevents the percentage from jumping 60% -> 50% instantly.
            // It will give you 58%, 57%, etc.
            float percent = p1 + ((voltage - v1) * (p2 - p1) / (v2 - v1));
            
            return (uint8_t)percent;
        }
    }

    return 0; // Should not reach here
}

// 1. The Task Function
// This contains the setup AND the infinite loop for this specific job
void battery_monitor_task(void *pvParameters)
{
    // --- SETUP PHASE (Runs once) ---
    
    // 1. Init ADC Unit (Same as before)
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // 2. Configure Channel (Same as before)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

    // 3. Setup Calibration (CHANGED FOR S3)
    adc_cali_handle_t adc_cali_handle = NULL;
    
    // S3 uses "Curve Fitting", not "Line Fitting"
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    bool do_calibration = false;
    // Note: Function name is also different (create_scheme_curve_fitting)
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle) == ESP_OK) {
        do_calibration = true;
        FreeOSLogI(TAG, "ADC Calibration Initialized");
    } else {
        ESP_LOGW(TAG, "ADC Calibration Failed");
    }

    // --- LOOP PHASE (Runs forever) ---
    while (1) {
        //int adc_raw = 0;
        int voltage_mv_pin = 0;
        float battery_voltage = 0;

        // Read Raw
        //ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw));

        /*if (do_calibration) {
            // Convert to Millivolts
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_mv_pin));
            
            // Apply Divider Math
            battery_voltage = (voltage_mv_pin * VOLTAGE_DIVIDER) / 1000.0f;
            
            FreeOSLogI(TAGBattery, "Battery: %.2f V (%d mV)", battery_voltage, voltage_mv_pin);
        } else {
            ESP_LOGW(TAGBattery, "Raw: %d (No Calib)", adc_raw);
        }*/
        
        uint32_t raw_sum = 0;
        const int SAMPLES = 64; // Take 64 samples to smooth out noise

        // 1. Burst Read
        for (int i = 0; i < SAMPLES; i++) {
            int temp_raw = 0;
            adc_oneshot_read(adc_handle, ADC_CHANNEL, &temp_raw);
            raw_sum += temp_raw;
            // Tiny delay helps separate noise spikes
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 2. Average
        int adc_raw = raw_sum / SAMPLES;

        if (do_calibration) {
            // Convert the AVERAGED raw value to voltage
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_mv_pin));
            
            battery_voltage = (voltage_mv_pin * 2.0f) / 1000.0f;
            uint8_t pct = get_battery_percentage(battery_voltage);
            
            FreeOSLogI(TAG, "Battery (Avg): %.2f V | %d%%", battery_voltage, pct);
        }
        
        // Inside Task loop
        g_battery_level = battery_voltage;

        // Inside any other task
        printf("Current Level: %.2f", g_battery_level);
        
        // 3. Convert to Percentage
        uint8_t pct = get_battery_percentage(battery_voltage);
            
        FreeOSLogI(TAG, "Battery: %.2f V | %d%%", battery_voltage, pct);

        // Delay for 5 seconds (5000ms)
        // This frees up the CPU for other tasks
        vTaskDelay(pdMS_TO_TICKS(100));

        
    }
    
    // Tasks should never return. If they do, they must delete themselves.
    vTaskDelete(NULL);
}


