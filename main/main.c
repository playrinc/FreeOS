//
//  main.c
//  
//
//  Created by Chris Galzerano on 2/8/26.
//

#include "main.h"

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    
    esp_log_level_set("CPUGraphics", ESP_LOG_VERBOSE);
    
    srand(time(NULL));
    
    //wifi_scan();
    
    CCString* testPath = ccs("/Users/chrisgalzerano/Desktop");
    
    checkAvailableMemory();
    
    mount_fatfs();
    
    list_directory_contents(MOUNT_POINT);
    
    heap_caps_print_heap_info(MALLOC_CAP_EXEC);
    
    // 2. List the contents of the mounted filesystem
    if (wl_handle != WL_INVALID_HANDLE) {
        list_directory_contents(MOUNT_POINT);
        
        // --- Example: Test writing a new file ---
        FILE *f = fopen("/fat/new_file.txt", "w");
        if (f == NULL) {
            FreeOSLogE(TAG, "Failed to open file for writing (Is WL mounted? Check partition size)");
        } else {
            fprintf(f, "This file was created by the ESP32 firmware!");
            fclose(f);
            FreeOSLogI(TAG, "Successfully wrote a test file.");
            
            // List again to show the new file
            list_directory_contents(MOUNT_POINT);
        }
    }
    
    FreeOSLogI(TAG, "AREREREWWERSFRESFRESREW.");
    //load_and_execute_program("/spiflash/test.bin");
    
#if defined(USE_ILI9488)
    FreeOSLogI(TAG, "JYWRUYWERUWTYERUWYERUWY.");
    //display_brightness_init();
    //display_brightness_set(0);
    //////////initialize_spi();
    //////////initialize_display();
    initialize_spi();
    initialize_display();
    //initRgbDisplay();
    g_touch_mutex = xSemaphoreCreateMutex();
    ////////ft6336u_driver_init();
    ft6336u_driver_init();
    //initialize_lvgl();
    //lvgl_touch_driver_init();
    ///////xTaskCreate(updateTouch, "updateTouch", 8192, NULL, 5, NULL);
    //xTaskCreate(updateTouch, "updateTouch", 8192, NULL, 5, NULL);
    
    xTaskCreatePinnedToCore(
        updateTouch,    // Function to run
        "updateTouch",  // Task name
        8192,           // Stack size
        NULL,           // Parameters
        5,              // Priority
        NULL,           // Task handle
        0               // PIN STRICTLY TO CORE 0
    );
    
    /*g_graphics_queue = xQueueCreate(40, sizeof(GraphicsCommand));
    if (g_graphics_queue == NULL) {
        return; // Halt if queue creation fails
    }*/
    
    g_graphics_queue = QueueCreate(40, sizeof(GraphicsCommand));
    if (g_graphics_queue == NULL) {
        FreeOSLogE(TAG, "Failed to create graphics queue!");
    }
    
    FreeOSLogI(TAG, "Graphics queue created.");
    
    // 2. Create the graphics task
    // This creates the task, gives it a 4KB stack, pins it to Core 1
    // (so Core 0 can be used for Wi-Fi/Bluetooth if needed)
   
    
    ///////////////
    xTaskCreatePinnedToCore(
                            graphics_task,      // Function to implement the task
                            "graphics_task",    // Name of the task (for debugging)
                            32768,               // Stack size in words (4096 * 4 bytes)
                            NULL,               // Task parameters (not used)
                            5,                  // Task priority
                            NULL,               // Task handle (not used)
                            1                   // Pin to Core 1
                            );
    
    //i2c_scan_ng();
    //initialize_touch();
    //initTouch();
    
    mount_sd_card();
    
    //createI2STask();
    
    //initEs8311();
    
    //check_pins_1_and_2();
    
    initializeI2S();
    
    // 1. Setup Hardware
    example_ledc_init();

    // 2. Create the Task
    // xTaskCreate( Function, Name, Stack Size, Parameter, Priority, Handle );
    xTaskCreate(pwm_fade_task, "pwm_fade_task", 2048, NULL, 5, NULL);

    // 3. app_main is done.
    // The scheduler will now automatically switch to pwm_fade_task.
    FreeOSLogI(TAG, "Main complete, task created.");
    
    /*xTaskCreate(
            battery_monitor_task,   // Function to call
            "Battery_Monitor",      // Name for debugging
            4096,                   // Stack size (bytes) - 4KB is safe for printf/float
            NULL,                   // Parameter to pass (not used here)
            5,                      // Priority (1 is low, 24 is high)
            NULL                    // Task Handle (not needed unless you want to delete it later)
        );*/
    
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    // Start the debug task
    //xTaskCreatePinnedToCore(debug_microphone_task, "mic_debug", 4096, NULL, 5, NULL, 1);

    //testCPUGraphicsBenchmark();
    
    //printf("Starting Playback...\n");
    //xTaskCreatePinnedToCore(mp3_task_wrapper, "mp3_task", 32768, NULL, 5, NULL, 0);
    //play_mp3_file("/sdcard/frair.mp3", tx_handle);
    
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    /*FreeOSLogI(TAG, "Creating LVGL handler task");
     xTaskCreate(
     lvgl_task_handler, // Task function
     "lvgl_handler",    // Name of the task
     8192,              // Stack size in bytes (8KB)
     NULL,              // Task parameter
     5,                 // Task priority
     NULL               // Task handle (optional)
     );*/
    // 3. Start your LVGL UI Task
    //xTaskCreate(lvgl_ui_task1, "LVGL_UI_Task", 8192, NULL, 5, NULL);
    
#elif defined(USE_ST7789)
    initializeUIST7789();
#endif
    
    
    /*init_anim_buffer();
     vTaskDelay(pdMS_TO_TICKS(1000));
     FreeOSLogI(TAG, "CMD_ANIM_SAVE_BG");
     
     // 5. Send the Save Command
     // This tells graphics_task: "Call anim_save_background(&fb, ...) now!"
     GraphicsCommand cmd_save = {
     .cmd = CMD_ANIM_SAVE_BG,
     .x =  ANIM_X, .y = ANIM_Y, .w = ANIM_W, .h = ANIM_H
     };
     QueueSend(g_graphics_queue, &cmd_save, QUEUE_MAX_DELAY);
     FreeOSLogI(TAG, "Sent command to snapshot background.");
     
     // --- CRITICAL STEP: SYNC ---
     // We must wait for the graphics task to finish drawing the gradient
     // before we snapshot it. 500ms is plenty.
     vTaskDelay(pdMS_TO_TICKS(1000));
     
     // --- 3. NEW: Start Animation Task (Core 0) ---
     xTaskCreatePinnedToCore(
     triangle_animation_task,
     "anim_task",
     4096,
     NULL,
     4,    // Slightly lower priority than graphics/touch
     NULL,
     0     // Run on Core 0 to leave Core 1 free for rendering
     );*/
    
    /*
     // Optional: LVGL task creation is handled inside lvgl_port_init()
     joystick_mutex = xSemaphoreCreateMutex();
     
     // --- 2. Create the Joystick Reading Task ---
     xTaskCreate(
     joystick_task,            // Function that implements the task
     "JoystickReadTask",       // Text name for the task
     4096,                     // Stack size (adjust as needed)
     NULL,                     // Parameter to pass to the task
     5,                        // Priority (5 is a good default for peripherals)
     NULL                      // Task handle (not used here)
     );
     */
    
}

