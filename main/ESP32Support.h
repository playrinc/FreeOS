//
//  ESP32Support.h
//  
//
//  Created by Chris Galzerano on 2/7/26.
//

#ifndef ESP32Support_h
#define ESP32Support_h

#include <stdio.h>
#include <main.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/ledc.h"
#include "regex.h"

//ESP Flash File System
#include <stdio.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "esp_heap_caps.h"

//SDIO File System
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

//I2S Audio
#include "driver/i2s_std.h"
#include "es8311.h"

//ESP SPI LCD
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "rom/tjpgd.h"

// ESP-DSP library (must be added as a component)
#include "dsps_fft2r.h"
#include "dsps_fft_tables.h"
#include "dsps_view.h" // For windowing function
#include "dsps_wind.h"

// ESP-ADC
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"



// -- ESP H264 Components --
#include "esp_h264_dec.h"
#include "esp_h264_dec_sw.h"
#include "esp_h264_types.h"

// -- ESP-IDF LCD Components --
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include <esp_lcd_panel_vendor.h>
#include "esp_lcd_panel_st7789.h" // Specific driver for your controller
#include "esp_lcd_ili9488.h"

#include "driver/i2c_master.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft6x36.h"
#include "FT6336U.h"

#include <LogWrapper.h>
#include <QueueWrapper.h>



static const char *MOUNT_POINT = "/spiflash";
static const char *PARTITION_LABEL = "storage"; // MUST match partitions.csv entry

// Global handle for the Wear Leveling driver
extern wl_handle_t wl_handle;

extern SemaphoreHandle_t lcd_flush_ready_sem;

void checkAvailableMemory(void);
void mount_fatfs(void);
void initialize_spi(void);
void initialize_display(void);
esp_err_t ft6336u_driver_init(void);
void mount_sd_card(void);
void initializeI2S(void);
void example_ledc_init(void);
void initializeUIST7789(void);
void updateTouch(void *pvParameter);
void pwm_fade_task(void *pvParameters);
void battery_monitor_task(void *pvParameters);

void initRgbDisplay(void);
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
esp_err_t i2c_master_init(void);
void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
void write_to_i2s_16bit(i2s_chan_handle_t handle, int16_t *pcm_samples, int sample_count);
void play_mp3_file(const char *filename, i2s_chan_handle_t tx_handle);
void mp3_task_wrapper(void *arg);
void write_wav_header(FILE *f, int sample_rate, int channels, int total_data_len);
void record_wav_task(void *args);
void start_recording(void);
void stop_recording(void);
void record_task(void *args);
void turn_off_wifi_and_free_ram(void);
void init_wifi_stack_once(void);
void trigger_wifi_scan(void);
static UINT tjd_input(JDEC* jd, BYTE* buff, UINT nd);
static UINT tjd_output(JDEC* jd, void* bitmap, JRECT* rect);
void video_player_task(void *pvParam);
static UINT tjd_input_thumb(JDEC* jd, BYTE* buff, UINT nd);
static UINT tjd_output_thumb(JDEC* jd, void* bitmap, JRECT* rect);
static UINT tjd_input_one_frame(JDEC* jd, BYTE* buff, UINT nd);
static UINT tjd_output_one_frame(JDEC* jd, void* bitmap, JRECT* rect);
CCImage* imageWithVideoFrame(const char *path, int width, int height);
void next_frame_task(void *pvParam);
void next_frame_task1(void *pvParam);
void setup_video_preview_ui(void);
void setup_video_preview_ui1(void);
void vVideoDecodeTask(void *pvParameters);
void start_video_player(Framebuffer *fb);
uint8_t* yuv420_to_rgb888(uint8_t *y_plane, uint8_t *u_plane, uint8_t *v_plane, int width, int height);
uint8_t* get_first_video_frame(const char *filepath, int width, int height);
uint8_t* get_video_frame(const char *filepath, int width, int height, int frameNumber);
void load_video_poster();
void updateTouch(void *pvParameter);
static void array_2_channel_bitmap(const uint8_t channel_list[], const uint8_t channel_list_size, wifi_scan_config_t *scan_config);
void mount_fatfs(void);
void checkAvailableMemory();
void initializeUIST7789(void);
static void IRAM_ATTR gpio_isr_handler(void *arg);
static void touch_task(void *arg);
esp_err_t ft6336u_driver_init(void);
void lvgl_touch_driver_init(void);
void initTouch(void);
void createI2STask();
void check_pins_1_and_2(void);
void initEs8311(void);
void debug_microphone_task(void *args);
void initializeI2S(void);
void testGraphics(void);

extern int32_t g_last_x;
extern int32_t g_last_y;
extern lv_indev_state_t g_last_state;
extern SemaphoreHandle_t g_touch_mutex;

// Configuration Constants
extern const int DISPLAY_HORIZONTAL_PIXELS;
extern const int DISPLAY_VERTICAL_PIXELS;
extern const int DISPLAY_COMMAND_BITS;
extern const int DISPLAY_PARAMETER_BITS;
extern const unsigned int DISPLAY_REFRESH_HZ;
extern const int DISPLAY_SPI_QUEUE_LEN;
extern const int SPI_MAX_TRANSFER_SIZE;

extern const lcd_rgb_element_order_t TFT_COLOR_MODE;

extern const size_t LV_BUFFER_SIZE;
extern const int LVGL_UPDATE_PERIOD_MS;

/* extern const ledc_mode_t BACKLIGHT_LEDC_MODE;
extern const ledc_channel_t BACKLIGHT_LEDC_CHANNEL;
extern const ledc_timer_t BACKLIGHT_LEDC_TIMER;
extern const ledc_timer_bit_t BACKLIGHT_LEDC_TIMER_RESOLUTION;
extern const uint32_t BACKLIGHT_LEDC_FRQUENCY;
*/

// Handles and Buffers
extern esp_lcd_panel_io_handle_t lcd_io_handle;
extern esp_lcd_panel_handle_t lcd_handle;
extern lv_draw_buf_t lv_disp_buf;
extern lv_display_t *lv_display;
extern lv_color_t *lv_buf_1;
extern lv_color_t *lv_buf_2;
extern lv_obj_t *meter;
extern lv_style_t style_screen;


#endif /* ESP32Support_h */
