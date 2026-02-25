


idf_component_register(SRCS "main.c" "cJSON_Utils.c" "cJSON.c" "ObjectiveCC.c" "CPUGraphics.c" "FT6336U.c" "es8311.c" "FreeOS.c" "SystemApps.c" "ESP32Support.c" "QueueWrapper.c" "TaskWrapper.c" "FilesApp.c" "ClockApp.c" "ColorWheel.c" "PaintApp.c"
                    PRIV_REQUIRES esp_wifi nvs_flash fatfs vfs esp_lcd esp_adc esp_timer esp_lcd_touch esp_lcd_ili9488 esp_lcd_touch_ft6x36 freetype espcoredump esp-dsp esp_h264
                    INCLUDE_DIRS "."
REQUIRES "esp_psram" "littlefs" "fatfs" "vfs" "driver" "esp_lcd" "lvgl" "esp_lvgl_port" "esp_adc"  "esp_lcd_touch" "esp_lcd_ili9488" "esp_timer" "esp_lcd_touch_ft6x36" "freetype" "espcoredump" "esp-dsp" "esp_h264"
WHOLE_ARCHIVE)

set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/fatfs_image")
set(INPUT_DATA_DIR "storage")
set(OUTPUT_IMAGE_PATH "${CMAKE_BINARY_DIR}/storage.bin")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=char-subscripts")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=format-truncation")

# Disable warnings/errors related to incomplete struct initialization
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=missing-field-initializers")

# Disable errors related to macro indentation/structure
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=misleading-indentation")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=misleading-indentation")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=char-subscripts)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format-truncation)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-field-initializers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-field-initializers)
target_compile_options(${COMPONENT_LIB} PRIVATE -fPIC)

fatfs_create_rawflash_image(storage ${INPUT_DATA_DIR} FLASH_IN_PROJECT PRESERVE_TIME)

