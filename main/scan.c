/* Scan Example
 
 This example code is in the Public Domain (or CC0 licensed, at your option.)
 
 Unless required by applicable law or agreed to in writing, this
 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
 */

/*
 This example shows how to scan for available set of APs.
 */


//Main ESP System Imports
#include <string.h>
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

//C Standard Library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>
#include <setjmp.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <libgen.h>
#include <regex.h>
#include <pthread.h>
//#include <curl.h>
#include <sys/termios.h>
#include <fcntl.h>
#include <sys/select.h>
#include <dirent.h>

//Objective CC
#include <ObjectiveCC.h>
#include <CPUGraphics.h>
#include <cJSON.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H
#include <png.h>
#include <zlib.h>
#include "jpeg_decoder.h"
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

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

static const char *TAG = "scan";
void checkAvailableMemory(void);

FT_Library ft_library;
FT_Face    ft_face;
uint8_t* font_buffer = NULL; // We must keep the font in memory

static const char *MOUNT_POINT = "/spiflash";
static const char *PARTITION_LABEL = "storage"; // MUST match partitions.csv entry

// Global handle for the Wear Leveling driver
wl_handle_t wl_handle = WL_INVALID_HANDLE;

// In the global scope of scan.c
Framebuffer fb;

// Define the commands
typedef enum {
    CMD_DRAW_RECT,       // NEW: Draw a solid rectangle
    CMD_DRAW_TEXT,       // Draws text
    CMD_DRAW_TEXT_BOX,
    CMD_DRAW_TEXT_BOX_CACHED,
    CMD_UPDATE_AREA,      // NEW: Send a part of the PSRAM to the LCD
    CMD_CURSOR_SETUP,      // NEW: Initializes cursor position and saves background
    CMD_CURSOR_DRAW,       // NEW: Draws the cursor (blink ON)
    CMD_CURSOR_RESTORE,   // NEW: Restores background (blink OFF)
    CMD_SCROLL_CONTENT,
    CMD_LOAD_PAGE_1,
    CMD_DRAW_STAR,
    CMD_DRAW_ROUNDED_RECT,
    CMD_DRAW_GRADIENT_RECT,
    CMD_DRAW_GRADIENT_ROUNDED_RECT,
    CMD_DRAW_IMAGE_FILE,
    CMD_DRAW_POLYGON,
    CMD_DRAW_PIXEL_BUFFER,
    CMD_ANIM_SAVE_BG,    // Copy Framebuffer -> Backup Buffer
    CMD_ANIM_RESTORE_BG,
    CMD_UI_SAVE_TO_A,      // Copy Screen -> Buffer A
    CMD_UI_RESTORE_FROM_A, // Copy Buffer A -> Screen
    CMD_UI_SAVE_TO_B,      // Copy Screen -> Buffer B
    CMD_UI_COPY_B_TO_A,     // Copy Buffer B -> Buffer A (To promote new icon to "Active")
    CMD_DRAW_ROUNDED_HAND,
    CMD_DRAW_DAY_NIGHT_OVERLAY
} GraphicsCommandType;

// Define the message struct
typedef struct {
    GraphicsCommandType cmd;
    int x, y, w, h;
    int clipX, clipY, clipW, clipH;
    int radius;
    
    // Text Specifics
    char *text; // Increased size for multi-line text
    ColorRGBA color;
    int fontSize;
    TextFormat textFormat; // Stores alignment, wrap, spacing
    
    // --- NEW POLYGON FIELDS ---
    Vector3* vertices;   // Pointer to array of Vector3 (allocated in Bridge, freed in Task)
    int numVertices;     // Number of points
    
    // Image/Gradient Specifics
    char imagePath[64]; // Path to the file (e.g., "/spiflash/icon.png")
    void* pixelBuffer;
    Gradient* gradientData;
    
    // --- TRANSFORM FIELD (NEW) ---
    Matrix3x3 transform; // Fixed 3x3 matrix (9 floats)
    bool hasTransform;   // Flag to trigger the transform draw function
    
    // General
    bool fill;
    int shadowSize;
} GraphicsCommand;

// The global queue handle
QueueHandle_t g_graphics_queue;

bool setupui = false;
bool touchEnabled = true;

void drawViewHierarchy(void* object, int parentX, int parentY, CCRect currentClip, bool notHierarchy);
CCView* find_subview_at_point(CCView* container, int globalX, int globalY);
// Global Root View
CCView* mainWindowView = NULL;
CCScrollView* g_active_scrollview = NULL;
int g_touch_last_y = 0;
int lastOffY = 0;
CCTextView* myDemoTextView = NULL;
// --- Global Scroll State ---
int g_text_scroll_y = 0;      // How far down we have scrolled
int g_text_total_height = 0;  // Total height of the text (calculated once)
int g_text_view_h = 300;      // Height of the visible box
const char* g_long_text_ptr = NULL; // Pointer to the long string
bool notFirstTouch = false;

//
// =================== UPDATED: FreeType Initialization ===================
//
esp_err_t initialize_freetype()
{
    ESP_LOGI(TAG, "Initializing FreeType...");
    FT_Error error = FT_Init_FreeType(&ft_library);
    if (error) {
        ESP_LOGE(TAG, "Failed to initialize FreeType library!");
        return ESP_FAIL;
    }
    
    // --- THIS IS THE NEW PART ---
    // Read the font file from the mounted /storage partition
    const char* font_path = "/spiflash/proximaNovaRegular.ttf";
    ESP_LOGI(TAG, "Loading font from %s", font_path);
    
    FILE* file = fopen(font_path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open font file. Make sure it's in your 'storage' folder.");
        return ESP_FAIL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long font_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate a buffer for the font.
    // FreeType needs this buffer to exist for as long as the font is used.
    //font_buffer = (uint8_t*)cc_safe_alloc(1, font_size);
    font_buffer = (uint8_t*)heap_caps_malloc(font_size, MALLOC_CAP_SPIRAM);
    if (!font_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for font buffer (%ld bytes)", font_size);
        fclose(file);
        return ESP_FAIL;
    }
    
    // Read the file into the buffer
    fread(font_buffer, 1, font_size, file);
    fclose(file);
    
    // Load the font face from the memory buffer
    error = FT_New_Memory_Face(ft_library,
                               font_buffer, // Font buffer
                               font_size,   // Font size
                               0,           // Face index (0 for first face)
                               &ft_face);   // Output handle
    
    if (error) {
        ESP_LOGE(TAG, "Failed to load font face! FreeType error: 0x%X", error);
        // Free the buffer if we failed
        free(font_buffer);
        font_buffer = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "FreeType initialized and font loaded.");
    return ESP_OK;
}

// --- FreeType Cache Globals ---
FTC_Manager     g_ftc_manager = NULL;
FTC_ImageCache  g_ftc_image_cache = NULL;
FTC_CMapCache   g_ftc_cmap_cache = NULL;

// --- Face Requester Callback ---
// The Cache Manager calls this automatically when it needs to load a font file.
// We use the file path string itself as the 'face_id'.
FT_Error face_requester(FTC_FaceID face_id, FT_Library library, FT_Pointer req_data, FT_Face* aface) {
    const char* font_path = (const char*)face_id;
    // printf("FTC: Loading font from flash: %s\n", font_path);
    return FT_New_Face(library, font_path, 0, aface);
}

// --- Initialize and Preload ---
void init_freetype_cache_system(void) {
    if (g_ftc_manager != NULL) return; // Prevent double init

    printf("Initializing FreeType Cache...\n");

    // 1. Create the Cache Manager
    // Max Faces: 2 (Keep low for embedded)
    // Max Sizes: 4 (e.g., 12, 18, 24, 30)
    // Max Bytes: 200KB (Adjust based on your available SPIRAM)
    FT_Error error = FTC_Manager_New(
                                     ft_library,
        2,              // max_faces
        4,              // max_sizes
        200 * 1024,     // max_bytes (200KB cache)
        face_requester, // Callback to load files
        NULL,           // req_data
        &g_ftc_manager
    );

    if (error) {
        printf("CRITICAL: Failed to create FTC Manager! Error: %d\n", error);
        return;
    }

    // 2. Create the Sub-Caches (Image = Glyphs, CMap = Char-to-Index)
    FTC_ImageCache_New(g_ftc_manager, &g_ftc_image_cache);
    FTC_CMapCache_New(g_ftc_manager, &g_ftc_cmap_cache);
    
    // --- 3. Preload a Specific Size (Warm Up) ---
    // This forces the I/O and parsing to happen NOW.
    const char* font_path = "/spiflash/proximaNovaRegular.ttf";
    int preload_size = 12;

    printf("Preloading Font: %s @ %dpx\n", font_path, preload_size);
    
    FTC_ScalerRec scaler;
    scaler.face_id = (FTC_FaceID)font_path; // Cast path to ID
    scaler.width = 0;
    scaler.height = preload_size;
    scaler.pixel = 1; // 1 = Size is pixels
    scaler.x_res = 0;
    scaler.y_res = 0;

    FT_Size size;
    // Looking up the size forces the manager to load the file and set the scale
    if (FTC_Manager_LookupSize(g_ftc_manager, &scaler, &size) != 0) {
        printf("WARNING: Failed to preload font size.\n");
    } else {
        printf("Font preloaded successfully. Ready for fast rendering.\n");
    }
}


#define MAX_VIEW_STACK 5

// A simple stack to hold pointers to previous views
static CCView* viewStack[MAX_VIEW_STACK];
static int viewStackPointer = 0;

void push_view(CCView* currentView) {
    if (viewStackPointer < MAX_VIEW_STACK) {
        viewStack[viewStackPointer++] = currentView;
        printf("View pushed to stack. Stack size: %d\n", viewStackPointer);
    } else {
        printf("Error: View stack full!\n");
    }
}

CCView* pop_view() {
    if (viewStackPointer > 0) {
        viewStackPointer--;
        printf("View popped from stack. Remaining: %d\n", viewStackPointer);
        return viewStack[viewStackPointer];
    }
    return NULL; // Stack is empty
}

typedef enum {
    CurrentViewHome = 0,
    CurrentViewFiles,    // Default (starts at X)
    CurrentViewSettings,      // Centers text within clipWidth
    CurrentViewText,        // Aligns text to the right edge of clipWidth
    CurrentViewMessages,
    CurrentViewPaint,
    CurrentViewClock,
    CurrentViewPhotos,
    CurrentViewMusic,
    CurrentViewCalculator,
    CurrentViewSearch,
    CurrentViewMaps,
    CurrentViewNetTools,
    CurrentViewAbout,
    CurrentViewLocale,
    CurrentViewCalendarClock,
    CurrentViewWifi,
    CurrentViewBluetooth
} CurrentView;

CurrentView currentView = CurrentViewHome;

void init_anim_buffer(void);
void update_full_ui(void);

bool openedApp = false;
CCArray* files = NULL;
CCArray* settings = NULL;

typedef struct {
    CCView* container;
    CCImageView* icon;
    CCLabel* label;
} CCIconView;

// --- Keyboard Globals ---
CCView* uiKeyboardView = NULL;   // The container for the keyboard
CCLabel* uiTargetLabel = NULL;   // The label currently receiving text
CCString* uiInputBuffer = NULL;  // The actual text string being edited

void teardown_keyboard_data(void) {
    // 1. Free the Input String Buffer (Prevents Memory Leak)
    if (uiInputBuffer) {
        freeCCString(uiInputBuffer);
        uiInputBuffer = NULL;
    }

    // 2. Clear the View Pointer (Prevents "Dangling Pointer" Crashes)
    // Note: We do NOT free(uiKeyboardView) here because freeViewHierarchy
    // will handle the actual memory when it frees the parent window.
    uiKeyboardView = NULL;

    // 3. Clear the Target (Prevents "Ghost Updates")
    uiTargetLabel = NULL;
    
    // 4. Reset Touch State (Prevents Stuck Keys)
    //reset_keyboard_touch_state();
}

void close_current_app(void) {
    // 1. Check if we have anywhere to go back to
    CCView* previousView = pop_view();
    
    if (previousView != NULL) {
        // 2. Clean up the current app (The Files App)
        // We created it with malloc/viewWithFrame, so we should free it
        // to prevent memory leaks now that we are done with it.
        // Assuming you have a function like freeViewHierarchy(CCView* v)
        teardown_keyboard_data();
        
        freeViewHierarchy(mainWindowView);
        
        CurrentView newCurrentView = -999;
        
        if (currentView == CurrentViewFiles) {
            freeElement(files);
        }
        else if (currentView == CurrentViewSettings) {
            freeElement(settings);
        }
        else if (currentView == CurrentViewWifi) {
            newCurrentView = CurrentViewSettings;
        }
        
        if (newCurrentView != -999) {
            currentView = newCurrentView;
        }
        else {
            currentView = CurrentViewHome;
        }
        
        openedApp = false;
        
        // 3. Restore the old view
        mainWindowView = previousView;
        
        // 4. Refresh
        printf("Restored previous view.\n");
        update_full_ui();
        
        
    } else {
        printf("Can't go back, stack empty.\n");
    }
}

/**
 * @brief Returns a CCArray of CCDictionaries containing file metadata.
 * Keys: "Name" (String), "Path" (String), "DateModified" (CCDate), "Size" (Number)
 */
CCArray* get_directory_files_as_array(const char *mount_point) {
    CCArray* fileList = array();
    
    DIR *dir = NULL;
    struct dirent *ent;
    char full_path[256];
    struct stat st;
    
    dir = opendir(mount_point);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", mount_point);
        return fileList;
    }
    
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        
        if (mount_point[strlen(mount_point) - 1] == '/') {
            snprintf(full_path, sizeof(full_path), "%s%s", mount_point, ent->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", mount_point, ent->d_name);
        }
        
        if (stat(full_path, &st) == 0) {
            CCDictionary* fileDict = dictionary();
            
            // 1. Name & Path
            // Correct Order: (Dict, Value, Key)
            dictionarySetObjectForKey(fileDict, ccs(ent->d_name), ccs("Name"));
            dictionarySetObjectForKey(fileDict, ccs(full_path), ccs("Path"));
            
            // 2. Size
            // Correct Order: (Dict, Value, Key)
            dictionarySetObjectForKey(fileDict, numberWithInt((int)st.st_size), ccs("Size"));
            
            // 3. Date Modified
            // Correct Order: (Dict, Value, Key)
            CCDate* fileDate = dateWithTimeInterval((double)st.st_mtime);
            dictionarySetObjectForKey(fileDict, fileDate, ccs("DateModified"));
            
            // 4. Is Directory
            // Correct Order: (Dict, Value, Key)
            bool isDir = S_ISDIR(st.st_mode);
            dictionarySetObjectForKey(fileDict, numberWithInt(isDir ? 1 : 0), ccs("IsDirectory"));
            
            arrayAddObject(fileList, fileDict);
        } else {
            ESP_LOGW(TAG, "Failed to stat: %s", full_path);
        }
    }
    
    closedir(dir);
    return fileList;
}

void showTriangleAnimation(void);
void showRotatingImageAnimation(void);
void hideRotatingImageAnimation(void);
void rotating_image_task(void *pvParameter);
void setup_wifi_ui(void);

// --- WiFi Data Structures ---
#define MAX_WIFI_RESULTS 20

typedef struct {
    char ssid[33]; // 32 chars + null terminator
    int rssi;
    int channel;
    int auth_mode; // Useful later for lock icons
} WifiNetwork;

// Global access to scan results
extern WifiNetwork g_wifi_scan_results[MAX_WIFI_RESULTS];
extern int g_wifi_scan_count;

// Functions
void init_wifi_stack_once(void);
void trigger_wifi_scan(void);

#include <math.h>
// --- Global Animation State ---
// 200x200 pixels * 3 bytes = 120 KB (Must use PSRAM)
#define ANIM_W 200
#define ANIM_H 200
#define ANIM_X 60  // Center X (160) - Half Width (100)
#define ANIM_Y 140 // Center Y (240) - Half Height (100)

uint8_t* g_anim_backup_buffer = NULL;

// In scan.c (Global Scope)
#define MAX_INTERACT_W 80
#define MAX_INTERACT_H 100
#define BUFFER_SIZE (MAX_INTERACT_W * MAX_INTERACT_H * 3)

uint8_t* g_ui_backup_buffer_A = NULL; // Stores background of g_last_touched_icon
uint8_t* g_ui_backup_buffer_B = NULL; // Temp buffer for the new icon being touched

void init_ui_buffers() {
    g_ui_backup_buffer_A = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    g_ui_backup_buffer_B = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    
    if (!g_ui_backup_buffer_A || !g_ui_backup_buffer_B) {
        ESP_LOGE(TAG, "Failed to allocate UI backup buffers!");
    }
}



CCPoint getAbsoluteOrigin(CCView* view) {
    float x = 0;
    float y = 0;
    
    CCView* current = view;
    while (current != NULL) {
        x += current->frame->origin->x;
        y += current->frame->origin->y;
        
        // --- FIX: Cast the pointer ---
        current = (CCView*)current->superview;
        // -----------------------------
    }
    return (CCPoint){ .x = x, .y = y };
}

// Calculates the actual visible rectangle of a view on screen,
// accounting for all parent offsets AND parent clipping masks.
CCRect getAbsoluteVisibleRect(CCView* view) {
    if (!view) return *ccRect(0,0,0,0);
    
    // 1. Start with the view's own bounds relative to itself
    float x = 0;
    float y = 0;
    float w = view->frame->size->width;
    float h = view->frame->size->height;
    
    // 2. Walk up the tree to transform coordinates and apply clips
    CCView* current = view;
    
    // Accumulate offsets to get to screen space
    // We have to do this carefully:
    // The clipping rects of parents are in THEIR coordinate space,
    // so we need to track where 'current' is relative to screen.
    
    // Actually, it's easier to calculate the Absolute Rect of the view first,
    // then walk up and intersect with Absolute Rects of parents.
    
    CCPoint absOrigin = getAbsoluteOrigin(view); // Use your existing helper
    
    float finalX = absOrigin.x;
    float finalY = absOrigin.y;
    float finalW = w;
    float finalH = h;
    
    // Now walk up to check clipping
    current = (CCView*)view->superview;
    while (current != NULL) {
        if (current->layer->masksToBounds) {
            CCPoint parentOrigin = getAbsoluteOrigin(current);
            
            // Parent's Visible Absolute Rect
            float pX = parentOrigin.x;
            float pY = parentOrigin.y;
            float pW = current->frame->size->width;
            float pH = current->frame->size->height;
            
            // Intersect (Math logic expanded here for clarity)
            float interLeft = (finalX > pX) ? finalX : pX;
            float interTop  = (finalY > pY) ? finalY : pY;
            float interRight = (finalX + finalW < pX + pW) ? (finalX + finalW) : (pX + pW);
            float interBottom = (finalY + finalH < pY + pH) ? (finalY + finalH) : (pY + pH);
            
            if (interLeft < interRight && interTop < interBottom) {
                finalX = interLeft;
                finalY = interTop;
                finalW = interRight - interLeft;
                finalH = interBottom - interTop;
            } else {
                // Completely hidden
                return *ccRect(0,0,0,0);
            }
        }
        current = (CCView*)current->superview;
    }
    
    return *ccRect(finalX, finalY, finalW, finalH);
}

// Add this to scan.c

void update_label_safe(CCLabel* label) {
    if (!label || !label->view) return;

    // 1. Get Coordinates
    CCPoint origin = getAbsoluteOrigin(label->view);
    int x = (int)origin.x;
    int y = (int)origin.y;
    int w = (int)label->view->frame->size->width;
    int h = (int)label->view->frame->size->height;
    
    printf("\n  update_label_safe %d %d  \n", w , h);

    // 2. Erase Old Text (Draw Background)
    // We assume the label is opaque (backgroundColor is set).
    GraphicsCommand cmd_bg = {
        .cmd = CMD_DRAW_RECT,
        .x = x, .y = y, .w = w, .h = h,
        .color = convert_cc_color(label->view->backgroundColor),
        // No clipping needed for self-update usually, or clip to self
        .clipX = 0, .clipY = 0, .clipW = 320, .clipH = 480
    };
    xQueueSend(g_graphics_queue, &cmd_bg, portMAX_DELAY);

    // 3. Draw New Text
    //CCLabel* label = (CCLabel*)object;
    ColorRGBA textCol = convert_cc_color(label->textColor);
    
    TextFormat fmt = { 0 };
    if (label->textAlignment == CCTextAlignmentCenter) fmt.alignment = TEXT_ALIGN_CENTER;
    else if (label->textAlignment == CCTextAlignmentRight) fmt.alignment = TEXT_ALIGN_RIGHT;
    else fmt.alignment = TEXT_ALIGN_LEFT;
    
    if (label->textVerticalAlignment == CCTextVerticalAlignmentCenter) fmt.valignment = TEXT_VALIGN_CENTER;
    else if (label->textVerticalAlignment == CCTextVerticalAlignmentTop) fmt.valignment = TEXT_VALIGN_TOP;
    else fmt.valignment = TEXT_VALIGN_BOTTOM;
    
    //ESP_LOGI(TAG, "drawViewHierarchy5 CMD_DRAW_TEXT_BOX valignment %d", fmt.valignment);
    
    if (label->lineBreakMode == CCLineBreakWordWrapping) fmt.wrapMode = TEXT_WRAP_MODE_WHOLE_WORD;
    else fmt.wrapMode = TEXT_WRAP_MODE_TRUNCATE;
    
    fmt.lineSpacing = (int)label->lineSpacing;
    fmt.glyphSpacing = (int)label->glyphSpacing;
    
    GraphicsCommand cmd_text = {
        .cmd = CMD_DRAW_TEXT_BOX_CACHED,
        .x = x, .y = y, .w = w, .h = h,
        .color = textCol,
        .fontSize = (int)label->fontSize,
        .textFormat = fmt,
        .clipX = 0,
        .clipY = 0,
        .clipW = 320,
        .clipH = 480
    };
    if (label->text && label->text->string) {
        // strdup allocates memory on heap and copies the string
        cmd_text.text = strdup(label->text->string);
    } else {
        cmd_text.text = NULL;
    }
    xQueueSend(g_graphics_queue, &cmd_text, portMAX_DELAY);
    
    // 4. Force Flush of ONLY this area (Optimization)
    // If you have a specific flush command, use it.
    // Otherwise, the Graphics Task usually flushes after processing.
    
    GraphicsCommand cmd_flush = {
        .cmd = CMD_UPDATE_AREA,
        .x = 0, .y = 0, .w = 320, .h = 480
    };
    xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
    
}

void update_view_area_via_parent(CCView* view) {
    if (!view || !view->superview) return;
    
    // 1. Calculate Child's Absolute Position (Target for Update)
    CCPoint childAbs = getAbsoluteOrigin(view);
    int targetX = (int)childAbs.x;
    int targetY = (int)childAbs.y;
    int targetW = (int)view->frame->size->width;
    int targetH = (int)view->frame->size->height;
    
    // --- EXPANSION (Optional, keep 0 for debugging) ---
    int expand = 0;
    int drawX = targetX - expand;
    int drawY = targetY - expand;
    int drawW = targetW + (expand * 2);
    int drawH = targetH + (expand * 2);
    
    // 2. Create Clip Rect
    CCRect* clipRectPtr = ccRect(drawX, drawY, drawW, drawH);
    if (!clipRectPtr) return;
    
    // 3. Find the Parent (We draw starting here)
    CCView* parent = (CCView*)view->superview;
    
    // 4. Calculate Parent's Absolute Position
    // We use the SAME helper function to guarantee coordinates match 'targetX/Y'
    CCPoint parentAbs = getAbsoluteOrigin(parent);
    
    // 5. Draw from Parent
    // We pass the Parent's absolute screen coordinates as the starting point.
    // drawViewHierarchy will take this (X,Y) and add the Child's relative (x,y)
    // resulting in exactly (targetX, targetY).
    drawViewHierarchy(parent, (int)parentAbs.x, (int)parentAbs.y, *clipRectPtr, true);
    
    freeCCRect(clipRectPtr);
    
    // 6. Safety Clamp for LCD
    if (drawX < 0) drawX = 0;
    if (drawY < 0) drawY = 0;
    if (drawX + drawW > 320) drawW = 320 - drawX;
    if (drawY + drawH > 480) drawH = 480 - drawY;
    
    // 7. Push Update
    /*GraphicsCommand cmd_flush = {
     .cmd = CMD_UPDATE_AREA,
     .x = drawX, .y = drawY, .w = drawW, .h = drawH
     };*/
    GraphicsCommand cmd_flush = {
        .cmd = CMD_UPDATE_AREA,
        .x = 0, .y = 0, .w = 320, .h = 480
    };
    xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
}


void update_view_only(CCView* view) {
    if (!view) return;
    
    // --- 1. Use the FULL Frame, not the "Visible/Clipped" Rect ---
    // The drawing logic likely generates the full view regardless of clipping.
    // We must ensure the hardware window is large enough to accept ALL that data.
    
    CCPoint absOrigin = getAbsoluteOrigin(view);
    int rawX = (int)absOrigin.x;
    int rawY = (int)absOrigin.y;
    int rawW = (int)view->frame->size->width;
    int rawH = (int)view->frame->size->height;
    
    // --- 5. Define the Clip Rect for the Renderer ---
    // We give the renderer the full padded area so it draws the borders.
    CCRect* clipRectPtr = ccRect(rawX, rawY, rawW, rawH);
    if (!clipRectPtr) return;
    
    // --- 6. Draw ---
    // Calculate parent-relative coordinates for the draw function
    int parentAbsX = rawX - (int)view->frame->origin->x;
    int parentAbsY = rawY - (int)view->frame->origin->y;
    
    // Use the PADDED size for the clip
    drawViewHierarchy(view, parentAbsX, parentAbsY, *clipRectPtr, true);
    
    freeCCRect(clipRectPtr);
    
    // --- 7. Flush the PADDED, ALIGNED Area ---
    GraphicsCommand cmd_flush = {
        .cmd = CMD_UPDATE_AREA,
        .x = 0,
        .y = 0,
        .w = 320,
        .h = 480
    };
    /*GraphicsCommand cmd_flush = {
     .cmd = CMD_UPDATE_AREA,
     .x = rawX,
     .y = rawY,
     .w = rawW,
     .h = rawH
     };*/
    xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
}

void update_full_ui(void) {
    if (!mainWindowView) return;
    touchEnabled = false;
    
    ESP_LOGI(TAG, "Starting UI Update...");
    
    // 1. Clear the screen first (Optional, but prevents ghosting)
    // We use the main window's background color for this
    ColorRGBA bgCol = convert_cc_color(mainWindowView->backgroundColor);
    GraphicsCommand cmd_clear = {
        .cmd = CMD_DRAW_RECT,
        .x = 0, .y = 0,
        .w = 320, .h = 480,
        .color = bgCol,
        .fill = true
    };
    // Use portMAX_DELAY to ensure we don't drop the clear command
    xQueueSend(g_graphics_queue, &cmd_clear, portMAX_DELAY);
    
    CCRect* screenRect = ccRect(0, 0, 320, 480);
    // 2. Walk the tree and generate all draw commands (Shadows, Borders, Views)
    // This calls the recursive function we wrote earlier
    drawViewHierarchy(mainWindowView, 0, 0, *screenRect, false);
    freeCCRect(screenRect);
    
    // 3. Push the pixels to the LCD (The Chunked Update)
    GraphicsCommand cmd_flush = {
        .cmd = CMD_UPDATE_AREA,
        .x = 0, .y = 0,
        .w = 320, .h = 480
    };
    xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
    
    ESP_LOGI(TAG, "UI Update Commands Sent.");
}

void update_full_ui1(void) {
    if (!mainWindowView) return;
    touchEnabled = false;
    
    ESP_LOGI(TAG, "Starting UI Update...");
    
    // 1. Clear the screen first (Optional, but prevents ghosting)
    // We use the main window's background color for this
    ColorRGBA bgCol = convert_cc_color(mainWindowView->backgroundColor);
    GraphicsCommand cmd_clear = {
        .cmd = CMD_DRAW_RECT,
        .x = 0, .y = 0,
        .w = 320, .h = 480,
        .color = bgCol,
        .fill = true
    };
    // Use portMAX_DELAY to ensure we don't drop the clear command
    xQueueSend(g_graphics_queue, &cmd_clear, portMAX_DELAY);
    
    CCRect* screenRect = ccRect(0, 0, 320, 480);
    // 2. Walk the tree and generate all draw commands (Shadows, Borders, Views)
    // This calls the recursive function we wrote earlier
    drawViewHierarchy(mainWindowView, 0, 0, *screenRect, true);
    freeCCRect(screenRect);
    
    // 3. Push the pixels to the LCD (The Chunked Update)
    GraphicsCommand cmd_flush = {
        .cmd = CMD_UPDATE_AREA,
        .x = 0, .y = 0,
        .w = 320, .h = 480
    };
    xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
    
    ESP_LOGI(TAG, "UI Update Commands Sent.");
}

void drawShapeLayer(CCShapeLayer* shapeLayer, int absX, int absY) {
    // 1. Basic Null Checks
    if (!shapeLayer) return;
    if (!shapeLayer->pointPath) return;
    if (!shapeLayer->pointPath->points) return;
    
    CCArray* points = shapeLayer->pointPath->points;
    int count = points->count;
    
    // 2. Minimum Vertex Check
    if (count < 3) return;
    
    // 3. Allocate Vertices
    Vector3* rawVertices = (Vector3*)cc_safe_alloc(1, sizeof(Vector3) * count);
    if (!rawVertices) {
        ESP_LOGE(TAG, "Failed to allocate vertices for shape layer");
        return;
    }
    
    // 4. Convert Coordinates with SAFETY CHECK
    for (int i = 0; i < count; i++) {
        CCPoint* pt = (CCPoint*)arrayObjectAtIndex(points, i);
        
        // --- SAFETY CHECK START ---
        if (pt == NULL) {
            ESP_LOGE(TAG, "Point at index %d is NULL! Aborting shape draw.", i);
            free(rawVertices); // Prevent memory leak
            return;
        }
        // --- SAFETY CHECK END ---
        
        rawVertices[i].x = (float)(absX + pt->x);
        rawVertices[i].y = (float)(absY + pt->y);
        rawVertices[i].z = 0.0f;
    }
    
    // 5. Handle Gradient or Solid Fill
    Gradient* lowLevelGrad = NULL;
    
    if (shapeLayer->gradient) {
        lowLevelGrad = create_low_level_gradient(shapeLayer->gradient);
    } else {
        // Create "Fake" Gradient for Solid Color
        // Ensure shapeLayer->fillColor is valid
        ColorRGBA solid;
        if (shapeLayer->fillColor) {
            solid = convert_cc_color(shapeLayer->fillColor);
        } else {
            // Default to magenta so you can see the error visibly
            solid = (ColorRGBA){255, 0, 255, 255};
        }
        
        lowLevelGrad = (Gradient*)cc_safe_alloc(1, sizeof(Gradient));
        if (lowLevelGrad) {
            lowLevelGrad->type = GRADIENT_TYPE_LINEAR;
            lowLevelGrad->angle = 0;
            lowLevelGrad->numStops = 2;
            lowLevelGrad->stops = (ColorStop*)cc_safe_alloc(1, sizeof(ColorStop) * 2);
            if (lowLevelGrad->stops) {
                lowLevelGrad->stops[0].color = solid;
                lowLevelGrad->stops[0].position = 0.0f;
                lowLevelGrad->stops[1].color = solid;
                lowLevelGrad->stops[1].position = 1.0f;
            }
        }
    }
    
    // 6. Send Command
    if (lowLevelGrad && lowLevelGrad->stops) {
        GraphicsCommand cmd_poly = {
            .cmd = CMD_DRAW_POLYGON,
            .vertices = rawVertices,
            .numVertices = count,
            .gradientData = lowLevelGrad
        };
        if (xQueueSend(g_graphics_queue, &cmd_poly, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to send polygon command");
            free(rawVertices);
            if(lowLevelGrad->stops) free(lowLevelGrad->stops);
            free(lowLevelGrad);
        }
    } else {
        // Cleanup if gradient creation failed
        free(rawVertices);
        if (lowLevelGrad) free(lowLevelGrad);
    }
}

// Helper to find intersection of two rects (for clipping)
CCRect intersectRects(CCRect r1, CCRect r2) {
    float r1_min_x = r1.origin->x;
    float r1_min_y = r1.origin->y;
    float r1_max_x = r1_min_x + r1.size->width;
    float r1_max_y = r1_min_y + r1.size->height;
    
    float r2_min_x = r2.origin->x;
    float r2_min_y = r2.origin->y;
    float r2_max_x = r2_min_x + r2.size->width;
    float r2_max_y = r2_min_y + r2.size->height;
    
    float inter_min_x = (r1_min_x > r2_min_x) ? r1_min_x : r2_min_x;
    float inter_min_y = (r1_min_y > r2_min_y) ? r1_min_y : r2_min_y;
    float inter_max_x = (r1_max_x < r2_max_x) ? r1_max_x : r2_max_x;
    float inter_max_y = (r1_max_y < r2_max_y) ? r1_max_y : r2_max_y;
    
    if (inter_min_x < inter_max_x && inter_min_y < inter_max_y) {
        return *ccRect(inter_min_x, inter_min_y, inter_max_x - inter_min_x, inter_max_y - inter_min_y);
    }
    // No intersection (empty rect)
    return *ccRect(0, 0, 0, 0);
}

/**
 * @brief Recursively traverses a CCView tree and generates graphics commands.
 * * @param view The current view to render.
 * @param parentX The accumulated X coordinate of the parent view.
 * @param parentY The accumulated Y coordinate of the parent view.
 */
// Function Signature: Accepts void* to handle CCView, CCLabel, and CCImageView
void drawViewHierarchy(void* object, int parentX, int parentY, CCRect currentClip, bool notHierarchy) {
    if (!object) return;
    
    //ESP_LOGI(TAG, "CMD_DRAW_GRADIENT_ROUNDED_RECT %d %d %d %d", (int)currentClip.origin->x, (int)currentClip.origin->y, (int)currentClip.size->width, (int)currentClip.size->height);
    
    // 1. Identify Type
    // We safely cast to CCType* first because 'type' is the first member of ALL your structs.
    CCType type = *((CCType*)object);
    
    // 2. Unwrap the "Base View"
    // This is the common structure shared by all UI elements
    CCView* baseView = NULL;
    
    if (type == CCType_View) {
        baseView = (CCView*)object;
    }
    else if (type == CCType_ImageView) {
        baseView = ((CCImageView*)object)->view;
    }
    else if (type == CCType_Label) {
        baseView = ((CCLabel*)object)->view;
    }
    else {
        return; // Unknown type
    }
    
    // Safety check
    if (!baseView) return;
    
    //ESP_LOGI(TAG, "drawViewHierarchy");
    
    // 3. Calculate Absolute Position
    int absX = parentX + (int)baseView->frame->origin->x;
    int absY = parentY + (int)baseView->frame->origin->y;
    int w = (int)baseView->frame->size->width;
    int h = (int)baseView->frame->size->height;
    
    CCRect myEffectiveClip = currentClip;
    
    if (notHierarchy) {
        // ============================================================
        // USE 'baseView' FOR GEOMETRY, LAYERS, AND BACKGROUNDS
        // ============================================================
        
        
        
        if (baseView->frame->size->width == 80 && baseView->frame->size->height == 100) {
            //printf("DEBUG: drawViewHierarchy | View Abs Pos: X=%d Y=%d | Parent Passed: X=%d Y=%d\n",
            //       absX, absY, parentX, parentY);
        }
        
        // ============================================================
        // FIXED VISIBILITY CULLING (Include Shadows!)
        // ============================================================
        
        // 1. Calculate the "Visual Padding" needed
        int padLeft = 0, padRight = 0, padTop = 0, padBottom = 0;
        
        if (!baseView->layer->masksToBounds) {
            if (baseView->layer->shadowOpacity > 0.0f) {
                int r = (int)baseView->layer->shadowRadius;
                int offX = (int)baseView->layer->shadowOffset->x;
                int offY = (int)baseView->layer->shadowOffset->y;
                
                // Expand to cover the shadow direction + radius
                padLeft   = (offX < 0) ? r + abs(offX) : r;
                padRight  = (offX > 0) ? r + abs(offX) : r;
                padTop    = (offY < 0) ? r + abs(offY) : r;
                padBottom = (offY > 0) ? r + abs(offY) : r;
            }
            if (baseView->layer->borderWidth > 0) {
                int b = (int)baseView->layer->borderWidth;
                padLeft += b; padRight += b; padTop += b; padBottom += b;
            }
        }
        
        // 2. Define Clipping Bounds
        int clipLeft = (int)currentClip.origin->x;
        int clipTop = (int)currentClip.origin->y;
        int clipRight = clipLeft + (int)currentClip.size->width;
        int clipBottom = clipTop + (int)currentClip.size->height;
        
        // 3. Define Visual Bounds
        int visualLeft   = absX - padLeft;
        int visualTop    = absY - padTop;
        int visualRight  = absX + w + padRight;
        int visualBottom = absY + h + padBottom;
        
        // 4. Check Intersection
        bool intersects = (visualLeft < clipRight && visualRight > clipLeft &&
                           visualTop < clipBottom && visualBottom > clipTop);
        
        if (!intersects) {
            // printf("Skipping view outside dirty rect\n");
            return;
        }
        
        // 4. Clipping Logic
        // A. Calculate Clip for THIS VIEW (Background + Shadows)
        // -----------------------------------------------------
        // Start with the PARENT'S clip. This allows shadows to spill outside
        // the view's frame, provided they are still inside the parent.
        CCRect myEffectiveClip = currentClip;
        
        // Only restrict 'myself' to 'my frame' if masksToBounds is strictly TRUE.
        if (baseView->layer->masksToBounds) {
            CCRect* myFrameAbs = ccRect(absX, absY, w, h);
            CCRect intersected = intersectRects(myEffectiveClip, *myFrameAbs);
            myEffectiveClip = intersected;
            freeCCRect(myFrameAbs);
        }
        
        // Optimization: If we can't see the view or its shadow, stop here.
        if (myEffectiveClip.size->width <= 0 || myEffectiveClip.size->height <= 0) return;
        
        //ESP_LOGI(TAG, "drawViewHierarchy1");
    }
    else {
        
    }
    
    
    // 5. Geometry Properties
    int radius = (int)baseView->layer->cornerRadius;
    CCLayer* layer = baseView->layer;
    
    // --- STEP A: SHADOWS (Use baseView/layer) ---
    if (layer->shadowOpacity > 0.0f) {
        int blur = (int)layer->shadowRadius;
        int sh_x = absX + (int)layer->shadowOffset->x - blur;
        int sh_y = absY + (int)layer->shadowOffset->y - blur;
        int sh_w = w + (blur * 2);
        int sh_h = h + (blur * 2);
        int sh_radius = radius + blur;
        
        ColorRGBA baseColor = convert_cc_color(layer->shadowColor);
        ColorRGBA startColor = baseColor;
        startColor.a = (uint8_t)(layer->shadowOpacity * 255.0f);
        ColorRGBA endColor = baseColor;
        endColor.a = 0;
        
        Gradient* shadowGrad = (Gradient*)cc_safe_alloc(1, sizeof(Gradient));
        if (shadowGrad) {
            shadowGrad->type = GRADIENT_TYPE_BOX;
            shadowGrad->angle = 0.0f;
            shadowGrad->numStops = 2;
            shadowGrad->stops = (ColorStop*)cc_safe_alloc(1, sizeof(ColorStop) * 2);
            if (shadowGrad->stops) {
                shadowGrad->stops[0].color = startColor;
                shadowGrad->stops[0].position = 0.0f;
                shadowGrad->stops[1].color = endColor;
                shadowGrad->stops[1].position = 1.0f;
                
                if ((int)sh_radius == 0) {
                    sh_radius = 20.0;
                }
                
                GraphicsCommand cmd_shadow = {
                    .cmd = CMD_DRAW_GRADIENT_ROUNDED_RECT,
                    .x = sh_x, .y = sh_y, .w = sh_w, .h = sh_h,
                    .radius = sh_radius,
                    .gradientData = shadowGrad,
                    .fill = true,
                    .clipX = (int)myEffectiveClip.origin->x,
                    .clipY = (int)myEffectiveClip.origin->y,
                    .clipW = (int)myEffectiveClip.size->width,
                    .clipH = (int)myEffectiveClip.size->height
                };
                
                //ESP_LOGI(TAG, "CMD_DRAW_GRADIENT_ROUNDED_RECT %d %d %d %d %d %d %d %d ", (int)myEffectiveClip.origin->x, (int)myEffectiveClip.origin->y, (int)myEffectiveClip.size->width, (int)myEffectiveClip.size->height, absX, absY, w, h);
                
                xQueueSend(g_graphics_queue, &cmd_shadow, portMAX_DELAY);
            } else { free(shadowGrad); }
        }
    }
    
    //ESP_LOGI(TAG, "drawViewHierarchy2");
    
    // --- STEP B: BORDERS (Use baseView/layer) ---
    if (layer->borderWidth > 0.0f) {
        int b_width = (int)layer->borderWidth;
        ColorRGBA borderCol = convert_cc_color(layer->borderColor);
        
        GraphicsCommand cmd_border = {
            .cmd = CMD_DRAW_ROUNDED_RECT,
            .x = absX - b_width,
            .y = absY - b_width,
            .w = w + (b_width * 2),
            .h = h + (b_width * 2),
            .radius = radius + b_width,
            .color = borderCol,
            .fill = true,
            .clipX = (int)myEffectiveClip.origin->x,
            .clipY = (int)myEffectiveClip.origin->y,
            .clipW = (int)myEffectiveClip.size->width,
            .clipH = (int)myEffectiveClip.size->height
        };
        xQueueSend(g_graphics_queue, &cmd_border, portMAX_DELAY);
    }
    
    //ESP_LOGI(TAG, "drawViewHierarchy3");
    
    // --- STEP C: BACKGROUNDS (Use baseView) ---
    // FIX: Changed 'view->backgroundColor' to 'baseView->backgroundColor'
    if (layer->gradient != NULL) {
        Gradient* lowLevelGrad = create_low_level_gradient(layer->gradient);
        
        // Optimization: Use fast rect if radius is 0 and gradient is linear
        if (radius <= 0 && lowLevelGrad->type == GRADIENT_TYPE_LINEAR) {
            printf("draw gradient rect command %d %d %d %d", absX, absY, w, h);
            GraphicsCommand cmd_grad = {
                .cmd = CMD_DRAW_GRADIENT_RECT,
                .x = absX, .y = absY, .w = w, .h = h,
                .gradientData = lowLevelGrad,
                .fill = true,
                .clipX = (int)myEffectiveClip.origin->x,
                .clipY = (int)myEffectiveClip.origin->y,
                .clipW = (int)myEffectiveClip.size->width,
                .clipH = (int)myEffectiveClip.size->height
            };
            xQueueSend(g_graphics_queue, &cmd_grad, portMAX_DELAY);
        } else {
            printf("draw gradient rounded rect command %d %d %d %d", absX, absY, w, h);
            GraphicsCommand cmd_grad = {
                .cmd = CMD_DRAW_GRADIENT_ROUNDED_RECT,
                .x = absX, .y = absY, .w = w, .h = h,
                .radius = radius,
                .gradientData = lowLevelGrad,
                .fill = true,
                .clipX = (int)myEffectiveClip.origin->x,
                .clipY = (int)myEffectiveClip.origin->y,
                .clipW = (int)myEffectiveClip.size->width,
                .clipH = (int)myEffectiveClip.size->height
            };
            xQueueSend(g_graphics_queue, &cmd_grad, portMAX_DELAY);
        }
    } else {
        ColorRGBA bgCol = convert_cc_color(baseView->backgroundColor);
        if (bgCol.a > 0) {
            GraphicsCommand cmd_bg = {
                .cmd = CMD_DRAW_ROUNDED_RECT,
                .x = absX, .y = absY, .w = w, .h = h,
                .radius = radius,
                .color = bgCol,
                .fill = true,
                .clipX = (int)myEffectiveClip.origin->x,
                .clipY = (int)myEffectiveClip.origin->y,
                .clipW = (int)myEffectiveClip.size->width,
                .clipH = (int)myEffectiveClip.size->height
            };
            xQueueSend(g_graphics_queue, &cmd_bg, portMAX_DELAY);
        }
    }
    
    //ESP_LOGI(TAG, "drawViewHierarchy4");
    
    
    // ============================================================
    // USE 'object' and 'type' FOR CONTENT
    // ============================================================
    
    // --- STEP D: HANDLE CONTENT ---
    
    // 1. Handle Shape Layer (Attached to baseView)
    // FIX: Changed 'view->shapeLayer' to 'baseView->shapeLayer'
    if (baseView->shapeLayer != NULL) {
        drawShapeLayer(baseView->shapeLayer, absX, absY);
    }
    
    //ESP_LOGI(TAG, "drawViewHierarchy5");
    
    // 2. Handle Label Content
    // FIX: Changed 'view->type' to 'type'
    if (type == CCType_Label) {
        CCLabel* label = (CCLabel*)object;
        ColorRGBA textCol = convert_cc_color(label->textColor);
        
        TextFormat fmt = { 0 };
        if (label->textAlignment == CCTextAlignmentCenter) fmt.alignment = TEXT_ALIGN_CENTER;
        else if (label->textAlignment == CCTextAlignmentRight) fmt.alignment = TEXT_ALIGN_RIGHT;
        else fmt.alignment = TEXT_ALIGN_LEFT;
        
        if (label->textVerticalAlignment == CCTextVerticalAlignmentCenter) fmt.valignment = TEXT_VALIGN_CENTER;
        else if (label->textVerticalAlignment == CCTextVerticalAlignmentTop) fmt.valignment = TEXT_VALIGN_TOP;
        else fmt.valignment = TEXT_VALIGN_BOTTOM;
        
        //ESP_LOGI(TAG, "drawViewHierarchy5 CMD_DRAW_TEXT_BOX valignment %d", fmt.valignment);
        
        if (label->lineBreakMode == CCLineBreakWordWrapping) fmt.wrapMode = TEXT_WRAP_MODE_WHOLE_WORD;
        else fmt.wrapMode = TEXT_WRAP_MODE_TRUNCATE;
        
        fmt.lineSpacing = (int)label->lineSpacing;
        fmt.glyphSpacing = (int)label->glyphSpacing;
        
        GraphicsCommand cmd_text = {
            .cmd = CMD_DRAW_TEXT_BOX_CACHED,
            .x = absX, .y = absY, .w = w, .h = h,
            .color = textCol,
            .fontSize = (int)label->fontSize,
            .textFormat = fmt,
            .clipX = (int)myEffectiveClip.origin->x,
            .clipY = (int)myEffectiveClip.origin->y,
            .clipW = (int)myEffectiveClip.size->width,
            .clipH = (int)myEffectiveClip.size->height
        };
        if (label->text && label->text->string) {
            // strdup allocates memory on heap and copies the string
            cmd_text.text = strdup(label->text->string);
        } else {
            cmd_text.text = NULL;
        }
        xQueueSend(g_graphics_queue, &cmd_text, portMAX_DELAY);
        //ESP_LOGI(TAG, "drawViewHierarchy5 CMD_DRAW_TEXT_BOX %d", myEffectiveClip.size->width);
    }
    
    
    
    // 3. Handle Image Content
    // FIX: Changed 'view->type' to 'type'
    // Inside drawViewHierarchy...

    // Inside drawViewHierarchy...

    else if (type == CCType_ImageView) {
        CCImageView* imgView = (CCImageView*)object;
        
        // SAFETY CHAIN:
        // 1. Check if Image exists
        // 2. Check if FilePath object exists (This was the NULL culprit)
        // 3. Check if the actual character buffer exists
        if (imgView->image != NULL &&
            imgView->image->filePath != NULL &&
            imgView->image->filePath->string != NULL) {
            
            GraphicsCommand cmd_img = {
                .cmd = CMD_DRAW_IMAGE_FILE,
                .x = absX, .y = absY, .w = w, .h = h,
                .clipX = (int)myEffectiveClip.origin->x,
                .clipY = (int)myEffectiveClip.origin->y,
                .clipW = (int)myEffectiveClip.size->width,
                .clipH = (int)myEffectiveClip.size->height
            };
            
            // Now it is 100% safe to access the string
            strncpy(cmd_img.imagePath, imgView->image->filePath->string, 63);
            
            xQueueSend(g_graphics_queue, &cmd_img, portMAX_DELAY);
        }
        
        if (imgView->image->imageData) {
            GraphicsCommand cmd = {
                .cmd = CMD_DRAW_PIXEL_BUFFER, // Use the new buffer command
                .x = absX,
                .y = absY,
                .w = imgView->image->size->width,  // Use image size
                .h = imgView->image->size->height,
                .pixelBuffer = imgView->image->imageData // Pass the raw pointer
            };
            xQueueSend(g_graphics_queue, &cmd, 0);
        }
    }
    
    //ESP_LOGI(TAG, "drawViewHierarchy6");
    
    
    // ============================================================
    // RECURSE SUBVIEWS
    // ============================================================
    
    if (notHierarchy) {
        CCRect* myExactFrame = ccRect(absX, absY, w, h);
        CCRect childClip = intersectRects(currentClip, *myExactFrame);
        freeCCRect(myExactFrame);
        
        // Only recurse if the child clip is visible
        if (childClip.size->width > 0 && childClip.size->height > 0) {
            if (baseView->subviews) {
                for (int i = 0; i < baseView->subviews->count; i++) {
                    drawViewHierarchy(baseView->subviews->array[i], absX, absY, childClip, notHierarchy);
                }
            }
        }
    }
    else {
        if (baseView->subviews) {
            for (int i = 0; i < baseView->subviews->count; i++) {
                void* subview = arrayObjectAtIndex(baseView->subviews, i);
                drawViewHierarchy(subview, absX, absY, myEffectiveClip, notHierarchy);
            }
        }
    }
    
}

// Factory function to create one grid item
CCIconView* create_icon_view(CCRect* frame, const char* imgPath, const char* title) {
    CCIconView* item = (CCIconView*)cc_safe_alloc(1, sizeof(CCIconView));
    
    // 1. Container View (80x100)
    item->container = viewWithFrame(frame);
    item->container->backgroundColor = color(0, 0, 0, 0.0); // Transparent default
    layerSetCornerRadius(item->container->layer, 10.0);     // Rounded corners
    // Optional: Add a subtle border or just rely on the click effect
    
    // 2. Icon Image (Top, e.g., 60x60)
    // Center X = (80 - 60) / 2 = 10. Top Y = 10.
    CCRect* imgFrame = ccRect(5, 5, frame->size->width-10, frame->size->width-10);
    item->icon = imageViewWithFrame(imgFrame);
    item->icon->image = imageWithFile(ccs(imgPath));
    viewAddSubview(item->container, item->icon); // Add generic wrapper
    
    // 3. Text Label (Bottom)
    // Y = 75 to leave space under image. Height = 20.
    CCRect* lblFrame = ccRect(0, imgFrame->origin->y+imgFrame->size->height+3, frame->size->width, frame->size->height - imgFrame->origin->y);
    item->label = labelWithFrame(lblFrame);
    labelSetText(item->label, ccs(title));
    item->label->fontSize = 14;
    item->label->textAlignment = CCTextAlignmentCenter;
    item->label->textVerticalAlignment = CCTextVerticalAlignmentTop;
    item->label->textColor = color(1, 1, 1, 1); // White text
    
    // CRITICAL: Add the label object, not the view wrapper
    viewAddSubview(item->container, item->label);
    
    return item;
}

CCArray* g_grid_items_registry = NULL;
// Tracks the specific grid item currently holding the highlight state
CCView* g_last_touched_icon = NULL;

CCArray* create_grid_data_source(void) {
    CCArray* gridData = array();
    
    // Define raw data for the 12 items
    const char* labels[12] = {
        "Files", "Settings", "Text",
        "Message", "Paint", "Clock",
        "Photos", "Music", "Calculator",
        "Search", "Maps", "Net Tools"
    };
    
    // We'll use a few generic icon paths for demonstration
    const char* icons[12] = {
        "/spiflash/files.png", "/spiflash/settings.png", "/spiflash/text.png",
        "/spiflash/messages.png",    "/spiflash/paint.png",  "/spiflash/clock.png",
        "/spiflash/photos.png",    "/spiflash/music.png",  "/spiflash/calculator.png",
        "/spiflash/search.png",     "/spiflash/maps.png",  "/spiflash/net tools.png"
    };
    
    for (int i = 0; i < 12; i++) {
        CCDictionary* itemDict = dictionary();
        dictionarySetObjectForKey(itemDict, ccs(labels[i]), ccs("label"));
        dictionarySetObjectForKey(itemDict, ccs(icons[i]), ccs("image"));
        arrayAddObject(gridData, itemDict);
    }
    
    return gridData;
}

void drawHomeMenu(void) {
    int cols = 3;
    int rows = 4;
    int itemW = 80;
    int itemH = 100;
    int gapX = 10; // Spacing
    int gapY = 10;
    int startX = 30;
    int startY = 40;
    
    if (g_grid_items_registry == NULL) {
        g_grid_items_registry = array();
    }
    
    CCArray* dataItems = create_grid_data_source();
    
    for (int i = 0; i < dataItems->count; i++) {
        // Calculate Grid Position (Row/Col)
        int col = i % cols;
        int row = i / cols;
        
        int x = startX + (col * (itemW + gapX));
        int y = startY + (row * (itemH + gapY));
        
        // Retrieve Data Model
        CCDictionary* itemData = (CCDictionary*)arrayObjectAtIndex(dataItems, i);
        CCString* labelStr = (CCString*)dictionaryObjectForKey(itemData, ccs("label"));
        CCString* imgStr = (CCString*)dictionaryObjectForKey(itemData, ccs("image"));
        
        // Create View using Data
        CCIconView* iconItem = create_icon_view(
                                                ccRect(x, y, itemW, itemH), // Pass by value as expected by your fix
                                                imgStr->string,
                                                labelStr->string
                                                );
        
        // Add to Main Window
        viewAddSubview(mainWindowView, iconItem->container);
        
        arrayAddObject(g_grid_items_registry, iconItem->container);
        
        // Cleanup helper struct
        free(iconItem);
    }
    
}

void drawSampleViews(void) {
    // --- View 1: The "Card" (White, Rounded, Shadow) ---
    CCView* cardView = viewWithFrame(ccRect(40, 100, 240, 150));
    cardView->backgroundColor = color(1.0, 1.0, 1.0, 1.0); // White
    
    // Configure Layer Properties
    layerSetCornerRadius(cardView->layer, 20.0);
    
    // Add Shadow
    cardView->layer->shadowColor = color(0.0, 0.0, 0.0, 1.0); // Black
    cardView->layer->shadowOpacity = 0.5; // 50% transparent shadow
    cardView->layer->shadowOffset = ccPoint(10, 10); // Offset down-right
    
    viewAddSubview(mainWindowView, cardView);
    
    // Create a View to hold the shape
    CCView* triangleView = viewWithFrame(ccRect(100, 100, 100, 100));
    // Make the view transparent so only the shape shows
    triangleView->backgroundColor = color(0,0,0,0);
    
    // Create the Shape Layer
    CCShapeLayer* triangle = shapeLayer();
    
    // Add Points (Relative to the view's 0,0)
    pointPathAddPoint(triangle->pointPath, ccPoint(50, 0));   // Top Middle
    pointPathAddPoint(triangle->pointPath, ccPoint(100, 100)); // Bottom Right
    pointPathAddPoint(triangle->pointPath, ccPoint(0, 100));   // Bottom Left
    
    // Add a Gradient
    CCArray* colors = array();
    arrayAddObject(colors, color(1, 0, 0, 1)); // Red
    arrayAddObject(colors, color(0, 0, 1, 1)); // Blue
    CCArray* locs = array();
    arrayAddObject(locs, numberWithDouble(0.0));
    arrayAddObject(locs, numberWithDouble(1.0));
    
    triangle->gradient = gradientWithColors(colors, locs, M_PI_2); // Vertical gradient
    triangleView->shapeLayer = triangle;
    viewAddSubview(mainWindowView, triangleView);
    
    // Assign to view (You might need to modify CCView struct to hold a ShapeLayer specifically,
    // or just assign it to a generic 'void* content' field if you have one.
    // For now, let's assume you cast it into the 'layer' field if you modify CCLayer to hold shapes).
    
    // Or, simpler: Just call drawShapeLayer directly inside your render loop for testing.
    
    
    
    // Example Image
    CCImageView* myIcon = imageViewWithFrame(ccRect(50, 150, 64, 64));
    // Note: Ensure you have a valid VFS path string in your CCImage
    myIcon->image = imageWithFile(ccs("/spiflash/test.png"));
    viewAddSubview(mainWindowView, myIcon);
    
    
    
    
    // --- View 2: The "Button" (Gradient, Border) ---
    CCView* buttonView = viewWithFrame(ccRect(60, 300, 200, 60));
    
    // Create Gradient: Chrome/Metal style
    CCArray* colors1 = array();
    arrayAddObject(colors1, color(0.8, 0.8, 0.9, 1.0)); // Light Blue-ish Gray
    arrayAddObject(colors1, color(0.2, 0.2, 0.3, 1.0)); // Dark Blue-ish Gray
    
    CCArray* locs1 = array();
    arrayAddObject(locs1, numberWithDouble(0.0));
    arrayAddObject(locs1, numberWithDouble(1.0));
    
    // Assign Gradient to Layer
    buttonView->layer->gradient = gradientWithColors(colors1, locs1, M_PI_2); // Vertical
    
    // Add Border
    buttonView->layer->shadowColor = color(0.0, 0.0, 0.0, 1.0); // Black
    buttonView->layer->shadowOpacity = 0.5; // 50% transparent shadow
    buttonView->layer->shadowOffset = ccPoint(10, 10); // Offset down-right
    //buttonView->layer->borderWidth = 4.0;
    //buttonView->layer->borderColor = color(1.0, 1.0, 1.0, 1.0); // White border
    layerSetCornerRadius(buttonView->layer, 30.0); // Fully rounded caps
    
    viewAddSubview(mainWindowView, buttonView);
    
    // Example Label
    CCLabel* myLabel = labelWithFrame(ccRect(15, 0, buttonView->frame->size->width-30, buttonView->frame->size->height));
    myLabel->text = ccs("This is a long string that will now auto-wrap inside the box!");
    myLabel->fontSize = 12.0;
    myLabel->textAlignment = CCTextAlignmentCenter; // It will center!
    myLabel->textVerticalAlignment = CCTextVerticalAlignmentCenter; // It will center!
    myLabel->lineBreakMode = CCLineBreakWordWrapping; // It will wrap!
    myLabel->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(buttonView, myLabel);
    
    // 1. Create Text View
    // Position: x=20, y=80, w=280, h=300 (Visible Window)
    myDemoTextView = textViewWithFrame(ccRect(20, 80, 280, 300));
    /*
     FTC_Manager     g_ftc_manager = NULL;
     FTC_ImageCache  g_ftc_image_cache = NULL;
     FTC_CMapCache   g_ftc_cmap_cache = NULL;
     */
    myDemoTextView->ftManager    = g_ftc_manager;
    myDemoTextView->ftImageCache = g_ftc_image_cache;
    myDemoTextView->ftCMapCache  = g_ftc_cmap_cache;
    
    // Style the container
    myDemoTextView->scrollView->view->backgroundColor = color(0.95, 0.95, 0.95, 1.0);
    myDemoTextView->scrollView->view->layer->masksToBounds = true;
    layerSetCornerRadius(myDemoTextView->scrollView->view->layer, 10);
    
    
    
    // 2. Set Long Text
    // We assume ccs() creates a CCString
    CCString* longStr = ccs(
                            "Here is a very long string to demonstrate the new CCTextView.\n\n"
                            "This view automatically wraps text using the renderTextBox logic we built earlier.\n\n"
                            "More importantly, it sits inside a CCScrollView. When you drag your finger, "
                            "the touch loop calculates the delta and updates the contentOffset.\n\n"
                            "The 'drawViewHierarchy' function then clips this content to the parent frame, "
                            "creating a seamless scrolling effect.\n\n"
                            "The 'drawViewHierarchy' function then clips this content to the parent frame, "
                            "creating a seamless scrolling effect.\n\n"
                            "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6\nLine 7\n"
                            "End of text."
                            );
    
    // This helper calculates height and resizes the content view automatically
    myDemoTextView->label->fontName = ccs("/spiflash/proximaNovaRegular.ttf");
    textViewSetText(myDemoTextView, longStr);
    scrollViewSetContentSize(myDemoTextView->scrollView, ccSize(280, 1000));
    
    // Style the text
    myDemoTextView->label->fontSize = 12;
    myDemoTextView->label->textColor = color(0.1, 0.1, 0.1, 1.0);
    myDemoTextView->label->lineSpacing = 5;
    
    
    // 3. Add to Main Window
    // Important: Add the scrollView->view, NOT the textView itself
    viewAddSubview(mainWindowView, myDemoTextView->scrollView->view);
    
    CCImageView* myIcon2 = imageViewWithFrame(ccRect(0, 0, 320, 480));
    // Note: Ensure you have a valid VFS path string in your CCImage
    myIcon2->image = imageWithFile(ccs("/spiflash/test2.jpg"));
    //viewAddSubview(mainWindowView, myIcon2);
}

void setup_ui_demo(void) {
    ESP_LOGI(TAG, "setup_ui_demo");
    int64_t start_time = esp_timer_get_time();
    
    if (mainWindowView == NULL) {
        // Initialize the root view filling the screen
        mainWindowView = viewWithFrame(ccRect(0, 0, 320, 480));
        mainWindowView->backgroundColor = color(0.2, 0.2, 0.2, 1.0); // Dark gray background
    }
    
    // A. Create the Color Array
    CCArray* gradColors = array();
    // Deep Blue (0, 50, 150) -> Normalized to 0.0 - 1.0
    arrayAddObject(gradColors, color(0.0f, 50.0f/255.0f, 150.0f/255.0f, 1.0f));
    // Vibrant Aqua (0, 200, 255) -> Normalized
    arrayAddObject(gradColors, color(0.0f, 200.0f/255.0f, 1.0f, 1.0f));
    
    // B. Create the Locations Array
    CCArray* gradLocs = array();
    arrayAddObject(gradLocs, numberWithDouble(0.0));
    arrayAddObject(gradLocs, numberWithDouble(1.0));
    
    // C. Create the Gradient Object (Vertical / 90 degrees)
    // M_PI_2 is 90 degrees (Top to Bottom)
    CCGradient* aquaGradient = gradientWithColors(gradColors, gradLocs, M_PI_2);
    
    // D. Assign it to the layer
    mainWindowView->layer->gradient = aquaGradient;
    
    
    //drawSampleViews();
    
    drawHomeMenu();
    
    checkAvailableMemory();
    
    // 2. Capture End Time
    int64_t end_time = esp_timer_get_time();
    
    // 3. Calculate Duration
    int64_t time_diff = end_time - start_time;
    
    // 4. Log result (Use %lld for int64_t)
    // We log both Microseconds (us) and Milliseconds (ms)
    ESP_LOGI("PROFILE", "Code execution took: %lld us (%lld ms)", time_diff, time_diff / 1000);
    
    // --- Trigger Render ---
    // Assuming you have a function to start the render pass:
    // update_full_ui();
    //update_full_ui();
    update_full_ui1();
}

// --- Global Cursor State ---
#define CURSOR_W 2   // Width of the cursor (e.g., 10 pixels wide)
#define CURSOR_H 18   // Height of the cursor (matching 24pt font)
#define CURSOR_BPP 3  // Bytes per pixel (RGB888)
// Buffer to store the background pixels (Allocated in Internal SRAM)
uint8_t* g_cursor_backup_buffer = NULL;
static bool addedCursor = false;
// Global cursor state (from the background restoration section)
static int g_cursor_x;
static int g_cursor_y;
// Global state to track cursor visibility
static bool g_cursor_visible = false;
TaskHandle_t g_cursor_blink_handle = NULL;

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

/*
 * DISPLAY CONFIGURATION
 * Uncomment one of the following lines to select your display.
 */
#define USE_ILI9488
// #define USE_ST7789


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
        ESP_LOGI(TAG, "Initializing LCD backlight on GPIO %d...", LCD_PIN_NUM_BK_LIGHT);
        // ... (Backlight setup code remains the same)
        gpio_config_t bk_gpio_conf = {
            .pin_bit_mask = 1ULL << LCD_PIN_NUM_BK_LIGHT,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_conf));
        gpio_set_level(LCD_PIN_NUM_BK_LIGHT, 1);

        // --- Part 2: Initialize ILI9488 via temporary SPI bus ---
        ESP_LOGI(TAG, "Installing temporary SPI bus for ILI9488 init...");
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

    ESP_LOGI(TAG, "Installing ILI9488 driver over SPI...");
        esp_lcd_panel_handle_t init_panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_PIN_NUM_RST,
            .bits_per_pixel = 16,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io_handle,
                                                  &panel_config,
                                                  LCD_INIT_BUFFER_SIZE, // <--- NEW ARGUMENT
                                                  &init_panel_handle));

        ESP_LOGI(TAG, "Resetting and Initializing ILI9488...");
        ESP_ERROR_CHECK(esp_lcd_panel_reset(init_panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(init_panel_handle));

        // Optional: Set the ILI9488 to accept RGB565 (16-bit) data
        // Command 0x3A is COLMOD (Pixel Format). 0x55 is 16-bit (RGB565).
        esp_lcd_panel_io_tx_param(io_handle, 0x3A, (uint8_t[]){0x55}, 1);

        // Disable SPI interface
        ESP_LOGI(TAG, "Initialization complete. Deleting SPI handles...");
        ESP_ERROR_CHECK(esp_lcd_panel_del(init_panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_io_del(io_handle));
        ESP_ERROR_CHECK(spi_bus_free(SPI2_HOST));


        // --- Part 3: Install the permanent RGB interface ---
        ESP_LOGI(TAG, "Installing RGB panel driver for ILI9488...");
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
    ESP_LOGI(TAG, "Initializing SPI bus host...");
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
    ESP_LOGI(TAG, "Adding ILI9488 device to bus...");
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &raw_spi_handle)); // Your crash line (now fixed)
    
    // 3. Manually send the required commands
    // Ensure the DC pin (GPIO 8) is configured as output before this line!
    gpio_set_direction(LCD_PIN_NUM_SPI_DC, GPIO_MODE_OUTPUT);
    
    ESP_LOGI(TAG, "Manually setting 16-bit color format...");
    ili9488_set_colmod(raw_spi_handle); // This command should now execute!

    // ... continue with esp_lcd_new_panel_ili9488 call, etc.

        // Manually run the hardware reset (already working, but cleaner here)
        // Assuming GPIO 14 is your reset pin
        gpio_set_level(LCD_PIN_NUM_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(LCD_PIN_NUM_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(120));

        ESP_LOGI(TAG, "Starting full ILI9488 init sequence...");

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

        ESP_LOGI(TAG, "ILI9488 is now configured.");

    // ... (Proceed to delete SPI handles and initialize esp_lcd_new_panel_rgb) ...
    
        // --- Part 4: Draw to the "live" framebuffer ---
        uint16_t *framebuffer1 = NULL;
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(rgb_panel_handle, 1, (void **)&framebuffer1));

        ESP_LOGI(TAG, "Drawing solid green to the %dx%d framebuffer...", LCD_H_RES, LCD_V_RES);
        uint16_t green_color = 0x07E0; // RGB565: 00000 111111 00000 (Pure Green)
        for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
            framebuffer1[i] = green_color;
        }
        ESP_LOGI(TAG, "Display should be green and refreshed by hardware.");
}

static bool cpuGraphics = true;

// Add this near the top of your file
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

static const int DISPLAY_HORIZONTAL_PIXELS = 320;
static const int DISPLAY_VERTICAL_PIXELS = 480;
static const int DISPLAY_COMMAND_BITS = 8;
static const int DISPLAY_PARAMETER_BITS = 8;
static const unsigned int DISPLAY_REFRESH_HZ = 40000000;
static const int DISPLAY_SPI_QUEUE_LEN = 10;
static const int SPI_MAX_TRANSFER_SIZE = 32768;

static const lcd_rgb_element_order_t TFT_COLOR_MODE = COLOR_RGB_ELEMENT_ORDER_RGB;
//if !cpuGraphics
//static const lcd_rgb_element_order_t TFT_COLOR_MODE = COLOR_RGB_ELEMENT_ORDER_BGR;

esp_err_t setup_cursor_buffers() {
    size_t size = CURSOR_W * CURSOR_H * CURSOR_BPP;
    // Allocate buffer in internal, DMA-capable memory (very fast)
    g_cursor_backup_buffer = (uint8_t*) heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!g_cursor_backup_buffer) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for cursor backup!", size);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "setup_cursor_buffers");
    return ESP_OK;
}

void save_cursor_background(Framebuffer *fb, int x, int y) {
    if (!g_cursor_backup_buffer) {
        ESP_LOGI(TAG, "!g_cursor_backup_buffer");
        return;
    }
    
    // Set the new cursor position
    g_cursor_x = x;
    g_cursor_y = y;
    
    uint8_t* psram_ptr;
    uint8_t* backup_ptr = g_cursor_backup_buffer;
    
    ESP_LOGI(TAG, "save_cursor_background");
    
    // Loop through each row of the cursor rect and copy from the PSRAM framebuffer
    for (int i = 0; i < CURSOR_H; i++) {
        // Calculate the starting position of the row in the PSRAM framebuffer
        psram_ptr = &((uint8_t*)fb->pixelData)[((y + i) * fb->displayWidth + x) * CURSOR_BPP];
        // Copy the entire row (CURSOR_W * 3 bytes) to the internal backup buffer
        memcpy(backup_ptr, psram_ptr, CURSOR_W * CURSOR_BPP);
        backup_ptr += CURSOR_W * CURSOR_BPP;
    }
}

void restore_cursor_background(Framebuffer *fb) {
    if (!g_cursor_backup_buffer) return;
    
    // 1. Write the background back into the PSRAM framebuffer
    uint8_t* psram_ptr;
    uint8_t* backup_ptr = g_cursor_backup_buffer;
    
    for (int i = 0; i < CURSOR_H; i++) {
        psram_ptr = &((uint8_t*)fb->pixelData)[((g_cursor_y + i) * fb->displayWidth + g_cursor_x) * CURSOR_BPP];
        // Copy the row from the internal backup buffer back to the PSRAM framebuffer
        memcpy(psram_ptr, backup_ptr, CURSOR_W * CURSOR_BPP);
        backup_ptr += CURSOR_W * CURSOR_BPP;
    }
    
    ESP_LOGI(TAG, "restore_cursor_background");
    
    // 2. Send the restore command to the graphics queue
    GraphicsCommand cmd_update;
    cmd_update.cmd = CMD_UPDATE_AREA;
    cmd_update.x = g_cursor_x;
    cmd_update.y = g_cursor_y;
    cmd_update.w = CURSOR_W;
    cmd_update.h = CURSOR_H;
    
    // Use the queue send for the update
    if (xQueueSend(g_graphics_queue, &cmd_update, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Cursor restore command failed.");
    }
}

void cursor_blink_task(void *pvParameter)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    //ESP_LOGI(TAG, "cursor_blink_task");
    
    while (1) {
        GraphicsCommand cmd_blink;
        
        if (g_cursor_visible) {
            // If visible, send restore command (blink OFF)
            cmd_blink.cmd = CMD_CURSOR_RESTORE;
            g_cursor_visible = false;
        } else {
            // If hidden, send draw command (blink ON)
            cmd_blink.cmd = CMD_CURSOR_DRAW;
            g_cursor_visible = true;
        }
        
        // Send the command with zero wait time
        xQueueSend(g_graphics_queue, &cmd_blink, 0);
        
        // Wait for the blink interval (500ms)
        vTaskDelay(pdMS_TO_TICKS(500));
        
        
    }
}

// Note: To draw the cursor, you would then call drawRectangleCFramebuffer
// to draw a solid block over the area (x, y, CURSOR_W, CURSOR_H).

// Default to 25 lines of color data
static const size_t LV_BUFFER_SIZE = DISPLAY_HORIZONTAL_PIXELS * 25;
static const int LVGL_UPDATE_PERIOD_MS = 5;

/*static const ledc_mode_t BACKLIGHT_LEDC_MODE = LEDC_LOW_SPEED_MODE;
 static const ledc_channel_t BACKLIGHT_LEDC_CHANNEL = LEDC_CHANNEL_0;
 static const ledc_timer_t BACKLIGHT_LEDC_TIMER = LEDC_TIMER_1;
 static const ledc_timer_bit_t BACKLIGHT_LEDC_TIMER_RESOLUTION = LEDC_TIMER_10_BIT;
 static const uint32_t BACKLIGHT_LEDC_FRQUENCY = 5000;*/

static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;
static esp_lcd_panel_handle_t lcd_handle = NULL;
static lv_draw_buf_t lv_disp_buf;
static lv_display_t *lv_display = NULL;
static lv_color_t *lv_buf_1 = NULL;
static lv_color_t *lv_buf_2 = NULL;
static lv_obj_t *meter = NULL;
static lv_style_t style_screen;

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
 ESP_LOGI(TAG, "Initializing LEDC for backlight pin: %d", GPIO_NUM_NC);
 
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
 ESP_LOGI(TAG, "Setting backlight to %d%%", brightness_percentage);
 
 // LEDC resolution set to 10bits, thus: 100% = 1023
 uint32_t duty_cycle = (1023 * brightness_percentage) / 100;
 ESP_ERROR_CHECK(ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty_cycle));
 ESP_ERROR_CHECK(ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL));
 }*/

void initialize_spi()
{
    ESP_LOGI(TAG, "Initializing SPI bus (MOSI:%d, MISO:%d, CLK:%d)",
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

void initialize_display()
{
    const esp_lcd_panel_io_spi_config_t io_config =
    {
        .cs_gpio_num = PIN_NUM_CS,
        .dc_gpio_num = PIN_NUM_DC,
        .spi_mode = 0,
        .pclk_hz = DISPLAY_REFRESH_HZ,
        .trans_queue_depth = DISPLAY_SPI_QUEUE_LEN,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = &lv_display,
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
    ESP_LOGI(TAG, "Initializing LVGL");
    lv_init();
    
    ESP_LOGI(TAG, "Allocating LVGL draw buffers");
    lv_buf_1 = (lv_color_t *)heap_caps_malloc(LV_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(lv_buf_1);
#if USE_DOUBLE_BUFFERING
    lv_buf_2 = (lv_color_t *)heap_caps_malloc(LV_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(lv_buf_2);
#endif
    
    ESP_LOGI(TAG, "Creating LVGL display");
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
    
    
    ESP_LOGI(TAG, "Creating LVGL tick timer");
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
    ESP_LOGI(TAG, "Starting LVGL task handler");
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
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
    }
    return ret;
}



// --- LVGL BRIDGE ---
// These static variables will store the last known touch state
// They are updated by touch_task and read by lvgl_touch_read_cb
static int32_t g_last_x = 0;
static int32_t g_last_y = 0;
static lv_indev_state_t g_last_state = LV_INDEV_STATE_REL; // Start as Released
static SemaphoreHandle_t g_touch_mutex; // <-- Add this
// --- END LVGL BRIDGE ---


/*void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
 
 // --- LOCK ---
 xSemaphoreTake(g_touch_mutex, portMAX_DELAY);
 
 // Read the shared variables
 ESP_LOGI(TAG, "lvgl_touch_read_cb");
 data->point.x = g_last_x;
 data->point.y = g_last_y;
 data->state = g_last_state;
 
 // --- UNLOCK ---
 xSemaphoreGive(g_touch_mutex);
 }*/

/*void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
 
 // Lock the mutex to check the state
 xSemaphoreTake(g_touch_mutex, portMAX_DELAY);
 
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
 xSemaphoreTake(g_touch_mutex, portMAX_DELAY);
 
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
    xSemaphoreTake(g_touch_mutex, portMAX_DELAY);
    
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
 
 ESP_LOGI(TAG, "lvgl_touch_read_cb");
 
 FT6336U_TouchPointType touch_data;
 
 // 1. Scan the touch controller
 esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
 if (ret != ESP_OK) {
 ESP_LOGE(TAG, "Failed to scan touch: %s", esp_err_to_name(ret));
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
 // ESP_LOGI(TAG, "Touch 0: X=%u, Y=%u", data->point.x, data->point.y);
 }
 }*/

/*void updateTouch(void *pvParameter) {
 while (1) {
 FT6336U_TouchPointType touch_data;
 esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
 
 // --- LOCK ---
 xSemaphoreTake(g_touch_mutex, portMAX_DELAY);
 
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
 if (xQueueReceive(touch_intr_queue, &event, portMAX_DELAY)) {
 g_last_state = LV_INDEV_STATE_REL;
 // Interrupt received, scan the touch controller
 esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
 if (ret != ESP_OK) {
 ESP_LOGE(TAG, "Failed to scan touch: %s", esp_err_to_name(ret));
 g_last_state = LV_INDEV_STATE_REL;
 continue;
 }
 
 if (touch_data.touch_count == 0) {
 ESP_LOGI(TAG, "Touch Released");
 
 g_last_state = LV_INDEV_STATE_REL;
 } else {
 ESP_LOGI(TAG, "Touch Pressed");
 for (int i = 0; i < touch_data.touch_count; i++) {
 ESP_LOGI(TAG, "Touch %d: (%s) X=%u, Y=%u",
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

// --- New Global State for Scrolling UI ---
int g_scroll_offset_y = 0;      // Current scroll position (0 = top is visible)
int g_scroll_total_height = 0;  // Total height of the content (to be calculated)
int g_scroll_viewport_h = 300;  // Visible height of the clipping area (Example)
int g_drag_start_y = 0;         // Tracks initial touch Y when a drag begins
float g_drag_velocity = 0.0f;   // Optional: For kinetic scrolling

#include <math.h>
#include <stdio.h> // Include standard libraries for math and memory

// Assuming Vector3 is defined as:
// typedef struct { float x, y, z; } Vector3;

/**
 * @brief Generates the vertices for a 5-pointed star.
 *
 * @param centerX Center X coordinate.
 * @param centerY Center Y coordinate.
 * @param outerRadius Distance to the outer points.
 * @param innerRadius Distance to the inner points (the notches).
 * @param numVertices Pointer to store the number of vertices generated (will be 10).
 * @return A dynamically allocated array of Vector3 vertices. Caller must free.
 */
Vector3* create_star_vertices(float centerX, float centerY, float outerRadius, float innerRadius, int *numVertices) {
    const int num_points = 5;
    const int total_vertices = num_points * 2;
    *numVertices = total_vertices;
    
    // Allocate 10 vertices in memory
    Vector3* vertices = (Vector3*)cc_safe_alloc(1, total_vertices * sizeof(Vector3));
    if (!vertices) return NULL;
    
    float angle_step = M_PI / num_points; // 36 degrees per point (180/5)
    
    for (int i = 0; i < total_vertices; i++) {
        float radius = (i % 2 == 0) ? outerRadius : innerRadius;
        float angle = i * angle_step - M_PI_2; // Start star pointed upwards (offset by -90 deg)
        
        vertices[i].x = centerX + radius * cosf(angle);
        vertices[i].y = centerY + radius * sinf(angle);
        vertices[i].z = 0.0f; // 2D operation, Z is zero
    }
    
    return vertices;
}



int randNumberTo(int max) {
    return rand() % max;
}

void testGraphics(void);

static bool hasDrawnKeyboard = false;
static int keyboardCursorPosition = 0;

const char* letterForCoordinate(void) {
    return "Q";
}

CCScrollView* findParentScrollView(CCView* view) {
    CCView* current = view;
    while (current != NULL) {
        // Check if this view is the 'contentView' of a ScrollView structure
        // This is tricky because C structs don't have back-pointers to their wrapper.
        // WORKAROUND: We iterate our known scroll views or check a tag.
        
        // BETTER APPROACH:
        // In updateTouch, we look at the touchedView's superview chain.
        // If current->superview's layer has masksToBounds=true and the logic fits...
        
        // FOR THIS EXAMPLE: We will rely on a global pointer for the active scroll view
        // or explicit checking in the touch loop if you track your objects.
        current = (CCView*)current->superview;
    }
    return NULL;
}

// Global to track which icon is currently being pressed
CCView* g_pressed_icon_view = NULL;
ColorRGBA g_color_highlight = {.r=0, .g=0, .b=0, .a=25}; // 0.1 alpha (approx 25/255)
ColorRGBA g_color_transparent = {.r=0, .g=0, .b=0, .a=0};
// Registry to track only the interactive grid items

CCView* find_grid_item_at(int x, int y) {
    // Safety check
    ESP_LOGI(TAG, "find_grid_item_at");
    if (!g_grid_items_registry) return NULL;
    ESP_LOGI(TAG, "find_grid_item_at %d", g_grid_items_registry->count);
    
    // Iterate ONLY the registered grid items
    for (int i = 0; i < g_grid_items_registry->count; i++) {
        CCView* targetView = (CCView*)arrayObjectAtIndex(g_grid_items_registry, i);
        targetView->tag = i;
        
        // Get the frame (relative to parent/screen)
        // Since these are direct children of mainWindowView (0,0),
        // their frame origin is their absolute screen position.
        float vx = targetView->frame->origin->x;
        float vy = targetView->frame->origin->y;
        float vw = targetView->frame->size->width;
        float vh = targetView->frame->size->height;
        
        // Fast Geometry Check
        if (x >= vx && x < (vx + vw) &&
            y >= vy && y < (vy + vh)) {
            return targetView;
        }
    }
    return NULL;
}


// Helper to format bytes into readable KB/MB strings
CCString* formatFileSize(int bytes) {
    char buffer[32];
    if (bytes < 1024) {
        snprintf(buffer, sizeof(buffer), "%d B", bytes);
    } else if (bytes < 1048576) {
        snprintf(buffer, sizeof(buffer), "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(buffer, sizeof(buffer), "%.1f MB", bytes / 1048576.0);
    }
    return stringWithCString(buffer);
}

void setup_files_ui(void) {
    printf("setup_files_ui");
    currentView = CurrentViewFiles;
    
    // 1. Create the new container
    // We create a FRESH view. We do not modify the old one.
    float screenWidth = 320.0f;
    CCView* filesView = viewWithFrame(ccRect(0, 0, screenWidth, 480));
    
    // Style it differently so you know you switched
    filesView->backgroundColor = color(1.0, 1.0, 1.0, 1.0); // Light gray/White
    
    // Create Gradient: Chrome/Metal style
    CCArray* colors1 = array();
    arrayAddObject(colors1, color(0.8, 0.8, 0.9, 1.0)); // Light Blue-ish Gray
    arrayAddObject(colors1, color(0.2, 0.2, 0.3, 1.0)); // Dark Blue-ish Gray
    
    CCArray* locs1 = array();
    arrayAddObject(locs1, numberWithDouble(0.0));
    arrayAddObject(locs1, numberWithDouble(1.0));
    
    // 2. Add a Header
    CCView* header = viewWithFrame(ccRect(0, 30, screenWidth, 60));
    header->backgroundColor = color(0.2, 0.2, 0.2, 1.0);
    layerSetCornerRadius(header->layer, 0.0);
    header->layer->shadowColor = color(0.0, 0.0, 0.0, 1.0); // Black
    header->layer->shadowOpacity = 0.5; // 50% transparent shadow
    header->layer->shadowOffset = ccPoint(0, 10); // Offset down-right
    header->layer->gradient = gradientWithColors(colors1, locs1, M_PI_2); // Vertical
    
    
    // 3. Add a "Back" or "Close" Button
    // IMPORTANT: You need a way to get back!
    CCView* backBtn = viewWithFrame(ccRect(10, 10, 80, 30));
    backBtn->backgroundColor = color(0.8, 0.0, 0.0, 1.0); // Red
    backBtn->tag = 999; // Special tag for "Back"
    //viewAddSubview(header, backBtn);
    
    CCLabel* myLabel = labelWithFrame(ccRect(0, 0, header->frame->size->width, 30));
    myLabel->text = ccs("Files");
    myLabel->fontSize = 24.0;
    myLabel->textAlignment = CCTextAlignmentCenter; // It will center!
    myLabel->textVerticalAlignment = CCTextVerticalAlignmentCenter; // It will center!
    myLabel->lineBreakMode = CCLineBreakWordWrapping; // It will wrap!
    myLabel->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(header, myLabel);
    
    CCView* navLineH = viewWithFrame(ccRect(0, 30, screenWidth, 1.0));
    navLineH->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(header, navLineH);
    
    CCView* navLineV = viewWithFrame(ccRect(0, screenWidth/3.0, 1.0, 70));
    navLineV->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(header, navLineV);
    
    CCView* navLineV2 = viewWithFrame(ccRect(0, (screenWidth/3.0)*2.0, 1.0, 70));
    navLineV2->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(header, navLineV2);
    
    CCView* cardView = viewWithFrame(ccRect(40, 160, 240, 150));
    cardView->backgroundColor = color(0.9, 1.0, 1.0, 1.0); // White
    layerSetCornerRadius(cardView->layer, 20.0);
    cardView->layer->shadowColor = color(0.0, 0.0, 0.0, 1.0); // Black
    cardView->layer->shadowOpacity = 0.5; // 50% transparent shadow
    cardView->layer->shadowOffset = ccPoint(10, 10); // Offset down-right
    //viewAddSubview(filesView, cardView);
    
    // 1. Get the data
    // (Ensure you have mounted the filesystem partition, e.g., "/spiflash" or "/sdcard")
    files = get_directory_files_as_array("/spiflash");
    //printf("%s", cStringOfString(objectDescription(files)));
    
    // 2. Setup Date Formatter
    CCDateFormatter* fmt = dateFormatter();
    fmt->dateStyle = CCDateFormatterStyleShort; // e.g., 12/31/25
    fmt->timeStyle = CCDateFormatterStyleShort; // e.g., 14:30
    
    // 3. Layout Constants
    int startY = 70;   // Start below screen top
    int rowH = 40;     // Height of each row
    int screenW = 320; // Your screen width
    
    // Table Columns:
    // Name: 0 -> 140
    // Date: 140 -> 250
    // Size: 250 -> 320
    
    
    
    // 5. Loop through files (Max 10)
    int count = files->count;
    if (count > 9) count = 9; // Limit to 10 labels as requested
    
    for (int i = 0; i < count; i++) {
        CCDictionary* fileData = (CCDictionary*)arrayObjectAtIndex(files, i);
        
        // --- Extract Data ---
        CCString* nameStr = (CCString*)dictionaryObjectForKey(fileData, ccs("Name"));
        CCDate* dateObj = (CCDate*)dictionaryObjectForKey(fileData, ccs("DateModified"));
        CCNumber* sizeNum = (CCNumber*)dictionaryObjectForKey(fileData, ccs("Size"));
        
        // --- Format Strings ---
        CCString* dateStr = stringFromDate(fmt, dateObj);
        CCString* sizeStr = formatFileSize(numberIntValue(sizeNum));
        
        // --- Create Row Container ---
        // We create a container view for the row so we can add a background or border later
        int currentY = startY + 20.0 + (i * rowH);
        CCView* rowView = viewWithFrame(ccRect(0, currentY, screenW, rowH));
        
        // Alternating background colors for readability (Zebra striping)
        if (i % 2 == 0) {
            rowView->backgroundColor = color(0.2, 0.2, 0.2, 1.0); // Dark Gray
        } else {
            rowView->backgroundColor = color(0.15, 0.15, 0.15, 1.0); // Slightly Darker
        }
        
        // --- 1. Name Label (Left Aligned) ---
        CCLabel* lblName = labelWithFrame(ccRect(5, 0, 155, rowH));
        lblName->text = copyCCString(nameStr);;
        lblName->fontSize = 14;
        lblName->textColor = color(1, 1, 1, 1);
        lblName->textAlignment = CCTextAlignmentLeft;
        lblName->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(rowView, lblName);
        
        // --- 2. Date Label (Center/Left) ---
        CCLabel* lblDate = labelWithFrame(ccRect(165, 0, 85, rowH));
        lblDate->text = dateStr ? dateStr : ccs("--/--/--");
        lblDate->fontSize = 10;
        lblDate->textColor = color(0.8, 0.8, 0.8, 1); // Light gray
        lblDate->textAlignment = CCTextAlignmentLeft;
        lblDate->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(rowView, lblDate);
        
        // --- 3. Size Label (Right Aligned) ---
        CCLabel* lblSize = labelWithFrame(ccRect(255, 0, 60, rowH));
        lblSize->text = sizeStr;
        lblSize->fontSize = 10;
        lblSize->textColor = color(0.5, 0.8, 1.0, 1); // Cyan-ish for numbers
        lblSize->textAlignment = CCTextAlignmentRight; // Numbers look better right-aligned
        lblSize->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(rowView, lblSize);
        
        // Add row to main window
        viewAddSubview(filesView, rowView);
    }
    
    viewAddSubview(filesView, header);
    
    CCImageView* headerArrowLeft = imageViewWithFrame(ccRect(5, 5, 10, 20));
    headerArrowLeft->image = imageWithFile(ccs("/spiflash/leftArrow20.png"));
    viewAddSubview(header, headerArrowLeft);
    
    CCView* highlightHeaderColumnView = viewWithFrame(ccRect(5, 30, 140, 30));
    highlightHeaderColumnView->backgroundColor = color(0.0, 0.0, 0.0, 0.1);
    viewAddSubview(header, highlightHeaderColumnView);
    
    // 4. Draw Header Row (Optional, for context)
    CCLabel* hName = labelWithFrame(ccRect(5, 30, 140, 30));
    hName->text = ccs("Filename");
    hName->textColor = color(1.0, 1.0, 1.0, 0.9); // Gray text
    hName->fontSize = 13;
    hName->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(header, hName);
    
    CCImageView* headerArrow = imageViewWithFrame(ccRect(115, 40, 20, 10));
    headerArrow->image = imageWithFile(ccs("/spiflash/downArrow20.png"));
    viewAddSubview(header, headerArrow);
    
    
    CCLabel* hDate = labelWithFrame(ccRect(165, 30, 100, 30));
    hDate->text = ccs("Date");
    hDate->textColor = color(1.0, 1.0, 1.0, 0.9);
    hDate->fontSize = 13;
    hDate->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(header, hDate);
    
    CCLabel* hSize = labelWithFrame(ccRect(255, 30, 100, 30));
    hSize->text = ccs("Size");
    hSize->textColor = color(1.0, 1.0, 1.0, 0.9);
    hSize->fontSize = 13;
    hSize->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(header, hSize);
    
    
    
    // 4. Update the global pointer
    // NOTE: We don't free the old one here, we assume it was pushed to stack
    mainWindowView = filesView;
}

/*void setup_settings_ui(void) {
    printf("setup_files_ui");
    currentView = CurrentViewSettings;
    
    float screenWidth = 320.0f;
    CCView* filesView = viewWithFrame(ccRect(0, 0, screenWidth, 480));
    
    // Style it differently so you know you switched
    filesView->backgroundColor = color(1.0, 1.0, 1.0, 1.0); // Light gray/White
    
    // Create Gradient: Chrome/Metal style
    CCArray* colors1 = array();
    arrayAddObject(colors1, color(0.8, 0.8, 0.9, 1.0)); // Light Blue-ish Gray
    arrayAddObject(colors1, color(0.2, 0.2, 0.3, 1.0)); // Dark Blue-ish Gray
    
    CCArray* locs1 = array();
    arrayAddObject(locs1, numberWithDouble(0.0));
    arrayAddObject(locs1, numberWithDouble(1.0));
    
    // 2. Add a Header
    CCView* header = viewWithFrame(ccRect(0, 30, screenWidth, 60));
    header->backgroundColor = color(0.2, 0.2, 0.2, 1.0);
    layerSetCornerRadius(header->layer, 0.0);
    header->layer->shadowColor = color(0.0, 0.0, 0.0, 1.0); // Black
    header->layer->shadowOpacity = 0.5; // 50% transparent shadow
    header->layer->shadowOffset = ccPoint(0, 10); // Offset down-right
    header->layer->gradient = gradientWithColors(colors1, locs1, M_PI_2); // Vertical
    
    CCLabel* myLabel = labelWithFrame(ccRect(0, 0, header->frame->size->width, 30));
    myLabel->text = ccs("Settings");
    myLabel->fontSize = 24.0;
    myLabel->textAlignment = CCTextAlignmentCenter; // It will center!
    myLabel->textVerticalAlignment = CCTextVerticalAlignmentCenter; // It will center!
    myLabel->lineBreakMode = CCLineBreakWordWrapping; // It will wrap!
    myLabel->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(header, myLabel);
    
    CCView* navLineH = viewWithFrame(ccRect(0, 30, screenWidth, 1.0));
    navLineH->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(header, navLineH);
    
    CCView* navLineV = viewWithFrame(ccRect(0, screenWidth/3.0, 1.0, 70));
    navLineV->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    //viewAddSubview(header, navLineV);

    
    // 3. Layout Constants
    int startY = 70;   // Start below screen top
    int rowH = 50;     // Height of each row
    int screenW = 320; // Your screen width
    
    settings = arrayWithObjects(ccs("About"), ccs("Locale"), ccs("Calendar/Clock"), ccs("WiFi"), ccs("Bluetooth"), ccs("Style"), ccs("Disk Storage"), ccs("CPU/RAM"), NULL);
    
    // 5. Loop through files (Max 10)
    int count = settings->count;
    if (count > 9) count = 9; // Limit to 10 labels as requested
    
    for (int i = 0; i < count; i++) {
        CCString* nameStr = (CCString*)arrayObjectAtIndex(settings, i);
        
        // --- Create Row Container ---
        // We create a container view for the row so we can add a background or border later
        int currentY = startY + 20.0 + (i * rowH);
        CCView* rowView = viewWithFrame(ccRect(0, currentY, screenW, rowH));
        rowView->tag = i;
        
        // Alternating background colors for readability (Zebra striping)
        if (i % 2 == 0) {
            rowView->backgroundColor = color(0.2, 0.2, 0.2, 1.0); // Dark Gray
        } else {
            rowView->backgroundColor = color(0.15, 0.15, 0.15, 1.0); // Slightly Darker
        }
        
        // --- 1. Name Label (Left Aligned) ---
        CCLabel* lblName = labelWithFrame(ccRect(5, 0, screenW-35, rowH));
        lblName->text = copyCCString(nameStr);
        lblName->fontSize = 24;
        lblName->textColor = color(1, 1, 1, 1);
        lblName->textAlignment = CCTextAlignmentLeft;
        lblName->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(rowView, lblName);
        
        CCImageView* headerArrowRight = imageViewWithFrame(ccRect(screenW-20, 10, 15, 30));
        headerArrowRight->image = imageWithFile(ccs("/spiflash/rightArrow30.png"));
        viewAddSubview(rowView, headerArrowRight);
        
        // Add row to main window
        viewAddSubview(filesView, rowView);
    }
    
    viewAddSubview(filesView, header);
    
    CCImageView* headerArrowLeft = imageViewWithFrame(ccRect(5, 5, 10, 20));
    headerArrowLeft->image = imageWithFile(ccs("/spiflash/leftArrow20.png"));
    viewAddSubview(header, headerArrowLeft);
    
    CCView* highlightHeaderColumnView = viewWithFrame(ccRect(5, 30, 140, 30));
    highlightHeaderColumnView->backgroundColor = color(0.0, 0.0, 0.0, 0.1);
    viewAddSubview(header, highlightHeaderColumnView);
    
    // 4. Draw Header Row (Optional, for context)
    CCLabel* hName = labelWithFrame(ccRect(5, 30, 140, 30));
    hName->text = ccs("Filename");
    hName->textColor = color(1.0, 1.0, 1.0, 0.9); // Gray text
    hName->fontSize = 13;
    hName->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(header, hName);
    

    
    
    
    // 4. Update the global pointer
    // NOTE: We don't free the old one here, we assume it was pushed to stack
    mainWindowView = filesView;
}*/

// --- Settings Globals ---
//static CCArray* settings = NULL;
static int settingsPageIndex = 0;
static const int SETTINGS_PER_PAGE = 6; // How many rows per page
static CCView* uiSettingsContainer = NULL; // Holds the rows + buttons
static CCView* buttonsContainer = NULL;

// Tags to identify buttons
#define TAG_SETTINGS_PREV 2001
#define TAG_SETTINGS_NEXT 2002
#define TAG_SETTINGS_ROW_BASE 3000

void update_settings_list(void) {
    // 1. Clear previous container if it exists
    // We remove it from the main window and free its memory
    if (uiSettingsContainer) {
        if (mainWindowView && mainWindowView->subviews) {
            viewRemoveFromSuperview(uiSettingsContainer);
        }
        freeViewHierarchy(uiSettingsContainer); // CAREFUL: Ensure this unlinks from parent
    }
    
    if (buttonsContainer) {
        if (mainWindowView && mainWindowView->subviews) {
            viewRemoveFromSuperview(buttonsContainer);
        }
        freeViewHierarchy(buttonsContainer);
    }
    
    // 2. Create New Container (Below the header)
    // Header is 90px tall (30 padding + 60 header), so start at Y=90
    uiSettingsContainer = viewWithFrame(ccRect(0, 90, 320, 390));
    uiSettingsContainer->backgroundColor = color(1,1,1,1); // Transparent
    viewAddSubview(mainWindowView, uiSettingsContainer);
    
    // Gradient
    CCArray* colors1 = array();
    arrayAddObject(colors1, color(0.8, 0.8, 0.9, 1.0));
    arrayAddObject(colors1, color(0.2, 0.2, 0.3, 1.0));
    CCArray* locs1 = array();
    arrayAddObject(locs1, numberWithDouble(0.0));
    arrayAddObject(locs1, numberWithDouble(1.0));

    buttonsContainer = viewWithFrame(ccRect(0, 390, 320, 90));
    buttonsContainer->backgroundColor = color(1,1,1,1); // Transparent
    buttonsContainer->layer->gradient = gradientWithColors(colors1, locs1, M_PI_2);
    viewAddSubview(mainWindowView, buttonsContainer);
    
    // 3. Calculate Pagination Bounds
    int totalItems = settings->count;
    int startIndex = settingsPageIndex * SETTINGS_PER_PAGE;
    int endIndex = startIndex + SETTINGS_PER_PAGE;
    if (endIndex > totalItems) endIndex = totalItems;

    int rowH = 50;
    int screenW = 320;
    int currentY = 0; // Relative to container

    // 4. Draw Rows
    for (int i = startIndex; i < endIndex; i++) {
        CCString* nameStr = (CCString*)arrayObjectAtIndex(settings, i);
        
        // Row View
        CCView* rowView = viewWithFrame(ccRect(0, currentY, screenW, rowH));
        rowView->tag = TAG_SETTINGS_ROW_BASE + i; // Tag helps us know which item was clicked
        
        // Zebra Striping
        if (i % 2 == 0) {
            rowView->backgroundColor = color(0.2, 0.2, 0.2, 1.0);
        } else {
            rowView->backgroundColor = color(0.15, 0.15, 0.15, 1.0);
        }

        // Label
        CCLabel* lblName = labelWithFrame(ccRect(15, 0, screenW-50, rowH));
        lblName->text = copyCCString(nameStr);
        lblName->fontSize = 24;
        lblName->textColor = color(1, 1, 1, 1);
        lblName->textAlignment = CCTextAlignmentLeft;
        lblName->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(rowView, lblName);
        
        // Arrow
        CCImageView* arrow = imageViewWithFrame(ccRect(screenW-25, 10, 15, 30));
        arrow->image = imageWithFile(ccs("/spiflash/rightArrow30.png"));
        viewAddSubview(rowView, arrow);

        viewAddSubview(uiSettingsContainer, rowView);
        currentY += rowH;
        
        if (i == endIndex - 1) {
            printf("add shadowOpacity endIndex");
            rowView->layer->shadowOpacity = 0.5;
            rowView->layer->shadowRadius = 10.0;
            rowView->layer->shadowOffset = ccPoint(0, 10);
        }
    }
    
    // Gradient
    /*CCArray* colors2 = array();
    arrayAddObject(colors2, color(0.0, 0.0, 0.0, 1.0));
    arrayAddObject(colors2, color(0.0, 0.0, 0.0, 0.0));
    CCArray* locs2 = array();
    arrayAddObject(locs2, numberWithDouble(0.0));
    arrayAddObject(locs2, numberWithDouble(1.0));

    CCView *buttonsContainerShadow = viewWithFrame(ccRect(0, 390, 320, 15));
    buttonsContainerShadow->backgroundColor = color(1,1,1,1); // Transparent
    buttonsContainerShadow->layer->gradient = gradientWithColors(colors2, locs2, M_PI_2);
    viewAddSubview(mainWindowView, buttonsContainerShadow);*/

    // 5. Draw Pagination Buttons (Bottom of list)
    int btnY = currentY + 10;
    int btnW = 100;
    int btnH = 40;
    
    btnY = 20;

    // PREV BUTTON (Only show if not on first page)
    if (settingsPageIndex > 0) {
        CCView* btnPrev = viewWithFrame(ccRect(20, btnY, btnW, btnH));
        btnPrev->backgroundColor = color(0.3, 0.3, 0.4, 1.0);
        btnPrev->layer->cornerRadius = 20;
        btnPrev->tag = TAG_SETTINGS_PREV;
        btnPrev->layer->shadowOpacity = 0.5;
        btnPrev->layer->shadowOffset = ccPoint(5, 5);
        btnPrev->layer->shadowRadius = 5;
        btnPrev->layer->borderWidth = 2.0;
        btnPrev->layer->borderColor = color(1.0, 1.0, 1.0, 0.7);
        
        CCLabel* lblPrev = labelWithFrame(ccRect(0,0,btnW,btnH));
        lblPrev->text = ccs("<< Back");
        lblPrev->textColor = color(1,1,1,1);
        lblPrev->textAlignment = CCTextAlignmentCenter;
        lblPrev->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(btnPrev, lblPrev);
        
        viewAddSubview(buttonsContainer, btnPrev);
    }

    // NEXT BUTTON (Only show if more items exist)
    if (endIndex < totalItems) {
        CCView* btnNext = viewWithFrame(ccRect(screenW - 20 - btnW, btnY, btnW, btnH));
        btnNext->backgroundColor = color(0.3, 0.3, 0.4, 1.0);
        btnNext->layer->cornerRadius = 20;
        btnNext->tag = TAG_SETTINGS_NEXT;
        btnNext->layer->shadowOpacity = 0.5;
        btnNext->layer->shadowOffset = ccPoint(5, 5);
        btnNext->layer->shadowRadius = 5;
        btnNext->layer->borderWidth = 2.0;
        btnNext->layer->borderColor = color(1.0, 1.0, 1.0, 0.7);

        CCLabel* lblNext = labelWithFrame(ccRect(0,0,btnW,btnH));
        lblNext->text = ccs("Next >>");
        lblNext->textColor = color(1,1,1,1);
        lblNext->textAlignment = CCTextAlignmentCenter;
        lblNext->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(btnNext, lblNext);
        
        viewAddSubview(buttonsContainer, btnNext);
    }
    
    int numSettings = (int)arrayCount(settings);
    
    if (numSettings > SETTINGS_PER_PAGE) {
        CCLabel* pageLbl = labelWithFrame(ccRect(120, btnY, 80, 40));
        char buf[16];
        snprintf(buf, 16, "%d / %d", settingsPageIndex + 1, (numSettings + SETTINGS_PER_PAGE - 1) / SETTINGS_PER_PAGE);
        pageLbl->text = ccs(buf);
        pageLbl->textColor = color(1.0, 1.0, 1.0, 1.0);
        pageLbl->textAlignment = CCTextAlignmentCenter;
        pageLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(buttonsContainer, pageLbl);
    }
}

void setup_settings_ui(void) {
    printf("setup_settings_ui\n");
    currentView = CurrentViewSettings;
    settings = NULL;
    
    // 1. Init Data
    // Note: We create this only once if it's NULL, or re-create it if needed
    if (!settings) {
        settings = arrayWithObjects(
            ccs("About"), ccs("Locale"), ccs("Calendar/Clock"),
            ccs("WiFi"), ccs("Bluetooth"), ccs("Style"),
            ccs("Disk Storage"), ccs("CPU/RAM"), ccs("Power"),
            ccs("Security"), ccs("Updates"), ccs("Developer"), NULL
        );
    }
    settingsPageIndex = 0; // Reset to page 0

    // 2. Main Window Background
    float screenWidth = 320.0f;
    CCView* filesView = viewWithFrame(ccRect(0, 0, screenWidth, 480));
    filesView->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    
    // 3. Header Setup (Same as your code)
    CCView* header = viewWithFrame(ccRect(0, 30, screenWidth, 60));
    header->backgroundColor = color(0.2, 0.2, 0.2, 1.0);
    header->layer->shadowOpacity = 0.5;
    header->layer->shadowOffset = ccPoint(0, 10);
    
    // Gradient
    CCArray* colors1 = array();
    arrayAddObject(colors1, color(0.8, 0.8, 0.9, 1.0));
    arrayAddObject(colors1, color(0.2, 0.2, 0.3, 1.0));
    CCArray* locs1 = array();
    arrayAddObject(locs1, numberWithDouble(0.0));
    arrayAddObject(locs1, numberWithDouble(1.0));
    header->layer->gradient = gradientWithColors(colors1, locs1, M_PI_2);
    
    //filesView->layer->gradient = gradientWithColors(colors1, locs1, M_PI_2);
    
    CCLabel* myLabel = labelWithFrame(ccRect(0, 0, screenWidth, 60));
    myLabel->text = ccs("Settings");
    myLabel->fontSize = 24.0;
    myLabel->textAlignment = CCTextAlignmentCenter;
    myLabel->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    myLabel->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(header, myLabel);
    
    viewAddSubview(filesView, header);
    
    // 4. Set Main View
    mainWindowView = filesView;

    // 5. Draw the List (First Page)
    uiSettingsContainer = NULL; // Reset container pointer
    buttonsContainer = NULL;
    update_settings_list();
}

void handle_settings_touch(int x, int y, int touchState) {
    if (touchState != 1 || !uiSettingsContainer) return; // Only on Touch Down

    // Check if we clicked a button inside the container
    CCView* clickedView = find_subview_at_point(uiSettingsContainer, x, y);
    if (!clickedView) {
        clickedView = find_subview_at_point(buttonsContainer, x, y);
    }
    
    if (clickedView) {
        if (clickedView->tag == TAG_SETTINGS_PREV) {
            printf("Prev Clicked\n");
            if (settingsPageIndex > 0) {
                settingsPageIndex--;
                update_settings_list(); // Redraw
                update_full_ui(); // Flush to screen
            }
        }
        else if (clickedView->tag == TAG_SETTINGS_NEXT) {
            printf("Next Clicked\n");
            // Safety check
            if ((settingsPageIndex + 1) * SETTINGS_PER_PAGE < settings->count) {
                settingsPageIndex++;
                update_settings_list(); // Redraw
                update_full_ui(); // Flush to screen
            }
        }
        else if (clickedView->tag >= TAG_SETTINGS_ROW_BASE) {
            int index = clickedView->tag - TAG_SETTINGS_ROW_BASE;
            //CCString* item = (CCString*)arrayObjectAtIndex(settings, index);
            printf("open row %d", index);
            if (index == 0) {
                
            }
            else if (index == 1) {
                
            }
            else if (index == 2) {
                
            }
            else if (index == 3) {
                if (mainWindowView != NULL) {
                    push_view(mainWindowView);
                }
                showRotatingImageAnimation();
                //showTriangleAnimation();
                setup_wifi_ui();
            }
            else if (index == 4) {
                
            }
        }
    }
}

// --- Calculator State ---
static CCString* calcDisplayStr = NULL;  // The text showing on screen
static double calcStoredValue = 0.0;     // The previous number typed
static char calcCurrentOp = 0;           // The operation: '+', '-', '*', '/'
static bool calcIsNewEntry = true;       // Flag to clear display on next number press

// Tag constants for non-digit buttons to avoid magic numbers
#define TAG_BTN_CLEAR 10
#define TAG_BTN_EQUALS 11
#define TAG_BTN_ADD 12
#define TAG_BTN_SUB 13
#define TAG_BTN_MUL 14
#define TAG_BTN_DIV 15

void update_calculator_label(void); // Forward declaration

void handle_calculator_input(int tag) {
    printf("\nhandle_calculator_input\n");
    // Initialize string if NULL
    if (!calcDisplayStr) calcDisplayStr = ccs("0");

    // --- A. Handle Digits (0-9) ---
    if (tag >= 0 && tag <= 9) {
        if (calcIsNewEntry) {
            // Start fresh (e.g., after pressing + or =)
            // Note: We create a new formatted string
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", tag);
            
            if (calcDisplayStr) freeCCString(calcDisplayStr);
            calcDisplayStr = ccs(buf);
            
            calcIsNewEntry = false;
        } else {
            // Append to existing string
            // We need to manually concatenate C-strings
            char* oldStr = cStringOfString(calcDisplayStr);
            int newLen = strlen(oldStr) + 2; // +1 char + null terminator
            char* newBuf = cc_safe_alloc(1, newLen);
            snprintf(newBuf, newLen, "%s%d", oldStr, tag);
            
            if (calcDisplayStr) freeCCString(calcDisplayStr);
            calcDisplayStr = ccs(newBuf); // ccs uses strdup/malloc internaly
            free(newBuf);
        }
    }
    
    // --- B. Handle Clear (C) ---
    else if (tag == TAG_BTN_CLEAR) {
        if (calcDisplayStr) freeCCString(calcDisplayStr);
        calcDisplayStr = ccs("0");
        calcStoredValue = 0.0;
        calcCurrentOp = 0;
        calcIsNewEntry = true;
    }
    
    // --- C. Handle Operations (+ - * /) ---
    else if (tag >= TAG_BTN_ADD && tag <= TAG_BTN_DIV) {
        // Store the current number
        calcStoredValue = atof(cStringOfString(calcDisplayStr));
        
        // Map tag to char
        if (tag == TAG_BTN_ADD) calcCurrentOp = '+';
        if (tag == TAG_BTN_SUB) calcCurrentOp = '-';
        if (tag == TAG_BTN_MUL) calcCurrentOp = '*';
        if (tag == TAG_BTN_DIV) calcCurrentOp = '/';
        
        calcIsNewEntry = true; // Next digit typed will start a new string
    }
    
    // --- D. Handle Equals (=) ---
    else if (tag == TAG_BTN_EQUALS) {
        double currentValue = atof(cStringOfString(calcDisplayStr));
        double result = currentValue;

        switch (calcCurrentOp) {
            case '+': result = calcStoredValue + currentValue; break;
            case '-': result = calcStoredValue - currentValue; break;
            case '*': result = calcStoredValue * currentValue; break;
            case '/':
                if (currentValue != 0) result = calcStoredValue / currentValue;
                else result = 0; // Error protection
                break;
        }

        // Format Result back to String
        char resBuf[64];
        // Use %.6g to remove trailing zeros automatically
        snprintf(resBuf, sizeof(resBuf), "%.6g", result);
        
        if (calcDisplayStr) freeCCString(calcDisplayStr);
        calcDisplayStr = ccs(resBuf);
        
        // Reset op but keep value in case user hits operation again
        calcStoredValue = result;
        calcCurrentOp = 0;
        calcIsNewEntry = true;
    }
    
    printf("\nupdate_calculator_label\n");

    // Refresh the Screen
    update_calculator_label();
}

CCLabel* uiCalcLabel = NULL; // Global pointer for easy updates

void setup_calculator_ui(void) {
    currentView = CurrentViewCalculator;
    // 1. Reset Root View
    // freeViewHierarchy(mainWindowView); // Ensure you call this before switching apps!
    mainWindowView = viewWithFrame(ccRect(0, 0, 320, 480));
    mainWindowView->backgroundColor = color(0.1, 0.1, 0.1, 1.0); // Dark Background

    // 2. Result Display (Top Rounded Rect)
    CCView* displayContainer = viewWithFrame(ccRect(20, 40, 280, 80));
    layerSetCornerRadius(displayContainer->layer, 15.0);
    displayContainer->backgroundColor = color(0.2, 0.2, 0.2, 1.0); // Darker Gray
    
    // Add Shadow to Display
    displayContainer->layer->shadowOpacity = 0.5;
    displayContainer->layer->shadowRadius = 10;
    displayContainer->layer->shadowOffset = ccPoint(0, 5);
    
    viewAddSubview(mainWindowView, displayContainer);

    // 3. Result Label
    uiCalcLabel = labelWithFrame(ccRect(10, 10, 260, 60)); // Inset inside container
    uiCalcLabel->text = copyCCString(calcDisplayStr ? calcDisplayStr : ccs("0"));
    uiCalcLabel->textColor = color(1, 1, 1, 1);
    uiCalcLabel->fontSize = 32;
    uiCalcLabel->textAlignment = CCTextAlignmentRight; // Standard calc alignment
    uiCalcLabel->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    uiCalcLabel->view->backgroundColor = color(0.2, 0.2, 0.2, 1.0);
    
    viewAddSubview(displayContainer, uiCalcLabel);

    // 4. Button Grid Configuration
    // 4 Columns x 4 Rows
    const char* btnLabels[16] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    
    int btnTags[16] = {
        7, 8, 9, TAG_BTN_DIV,
        4, 5, 6, TAG_BTN_MUL,
        1, 2, 3, TAG_BTN_SUB,
        TAG_BTN_CLEAR, 0, TAG_BTN_EQUALS, TAG_BTN_ADD
    };

    int startY = 140;
    int btnW = 60;
    int btnH = 60;
    int gap = 15;
    int startX = 20;

    for (int i = 0; i < 16; i++) {
        int col = i % 4;
        int row = i / 4;
        
        int x = startX + (col * (btnW + gap));
        int y = startY + (row * (btnH + gap));

        // Create Button Container
        CCView* btn = viewWithFrame(ccRect(x, y, btnW, btnH));
        layerSetCornerRadius(btn->layer, 30.0); // Fully circular/rounded
        btn->tag = btnTags[i]; // Store the Logic ID in the tag!
        
        // Color Logic
        if (btnTags[i] == TAG_BTN_EQUALS) {
             btn->backgroundColor = color(0.0, 0.6, 0.2, 1.0); // Green for Equals
        } else if (btnTags[i] >= 10) {
             btn->backgroundColor = color(1.0, 0.6, 0.0, 1.0); // Orange for Ops
        } else {
             btn->backgroundColor = color(0.3, 0.3, 0.3, 1.0); // Gray for Numbers
        }
        
        // Add Shadow
        btn->layer->shadowOpacity = 0.3;
        btn->layer->shadowRadius = 5;
        btn->layer->shadowOffset = ccPoint(2, 2);

        // Add Label
        CCLabel* lbl = labelWithFrame(ccRect(0, 0, btnW, btnH));
        lbl->text = ccs(btnLabels[i]); // Literals are safe here IF we don't double-free
        lbl->textColor = color(1, 1, 1, 1);
        lbl->fontSize = 24;
        lbl->textAlignment = CCTextAlignmentCenter;
        lbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        
        // Prevent touch events on the label from blocking the button view
        // (Assuming your touch logic checks parents if child doesn't handle)
        
        viewAddSubview(btn, lbl);
        viewAddSubview(mainWindowView, btn);
    }
}

void update_calculator_label(void) {
    if (uiCalcLabel) {
        
        // 1. Clean up old memory (Prevent Leak)
        if (uiCalcLabel->text) {
             freeCCString(uiCalcLabel->text);
        }
        
        // 2. Assign New Text (Deep Copy)
        uiCalcLabel->text = copyCCString(calcDisplayStr);
        
        // 3. OPTIMIZED DRAW:
        // Instead of redrawing the whole screen (slow),
        // we ask the Parent (the gray rounded box) to redraw just this text area.
        //update_view_area_via_parent((CCView*)uiCalcLabel);
        update_label_safe(uiCalcLabel);
        printf("\nupdate_calculator_label update_view_area_via_parent\n");
    }
}

// --- Music Player Globals ---

// UI Pointers for fast updates
static CCLabel* uiMusicTitleLbl = NULL;
static CCLabel* uiMusicArtistLbl = NULL;
static CCView* uiMusicProgressFill = NULL; // We adjust the frame width of this
static CCLabel* uiMusicPlayBtnLbl = NULL;  // To toggle between ▶ and ||

// Tag Constants for Input Handling
#define TAG_MUSIC_PREV 20
#define TAG_MUSIC_PLAY 21
#define TAG_MUSIC_NEXT 22
#define TAG_MUSIC_PROGRESS_BAR 23 // For tapping to seek later

// Layout Constants (320x480 screen)
#define SCREEN_W 320
#define MAIN_PADDING 20
#define ART_SIZE 280

void setup_music_player_ui(void) {
    // 1. Reset Root View
    currentView = CurrentViewMusic;
    //if (mainWindowView) freeViewHierarchy(mainWindowView);
    mainWindowView = viewWithFrame(ccRect(0, 0, SCREEN_W, 480));
    mainWindowView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);

    // --- TIGHTER LAYOUT MATH ---
    int topPadding = 30;
    int artSize = 220; // Reduced from 280
    int currentY = topPadding;

    //================================================================
    // 2. Album Artwork
    //================================================================
    // Centered horizontally: (320 - 220) / 2 = 50
    int artX = (SCREEN_W - artSize) / 2;
    CCView* artView = viewWithFrame(ccRect(artX, currentY, artSize, artSize));
    artView->backgroundColor = color(0.2, 0.25, 0.35, 1.0);
    layerSetCornerRadius(artView->layer, 20.0);
    artView->layer->shadowOpacity = 0.6;
    artView->layer->shadowRadius = 15;
    artView->layer->shadowOffset = ccPoint(0, 10);
    
    viewAddSubview(mainWindowView, artView);
    currentY += artSize + 25; // Gap reduced to 25

    //================================================================
    // 3. Track Information
    //================================================================
    // Song Title
    uiMusicTitleLbl = labelWithFrame(ccRect(20, currentY, 280, 28));
    uiMusicTitleLbl->text = ccs("Song Title Placeholder");
    uiMusicTitleLbl->textColor = color(1, 1, 1, 1);
    uiMusicTitleLbl->fontSize = 22; // Slightly smaller font
    uiMusicTitleLbl->textAlignment = CCTextAlignmentCenter;
    viewAddSubview(mainWindowView, uiMusicTitleLbl);
    currentY += 28;

    // Artist Name
    uiMusicArtistLbl = labelWithFrame(ccRect(20, currentY, 280, 20));
    uiMusicArtistLbl->text = ccs("Artist Name");
    uiMusicArtistLbl->textColor = color(0.7, 0.7, 0.8, 1.0);
    uiMusicArtistLbl->fontSize = 14;
    uiMusicArtistLbl->textAlignment = CCTextAlignmentCenter;
    viewAddSubview(mainWindowView, uiMusicArtistLbl);
    currentY += 20 + 20; // Gap reduced to 20

    //================================================================
    // 4. Progress Section
    //================================================================
    int progressHeight = 6;
    int progressWidth = 280; // 20px padding on each side
    int progressX = 20;
    
    // A. Track
    CCView* progressTrack = viewWithFrame(ccRect(progressX, currentY, progressWidth, progressHeight));
    progressTrack->backgroundColor = color(0.2, 0.2, 0.25, 1.0);
    layerSetCornerRadius(progressTrack->layer, progressHeight / 2.0);
    progressTrack->tag = TAG_MUSIC_PROGRESS_BAR;
    viewAddSubview(mainWindowView, progressTrack);

    // B. Fill
    uiMusicProgressFill = viewWithFrame(ccRect(progressX, currentY, 0, progressHeight)); // Width 0 init
    uiMusicProgressFill->backgroundColor = color(0.0, 0.8, 1.0, 1.0);
    layerSetCornerRadius(uiMusicProgressFill->layer, progressHeight / 2.0);
    // uiMusicProgressFill->userInteractionEnabled = false;
    viewAddSubview(mainWindowView, uiMusicProgressFill);
    currentY += progressHeight + 8;

    // C. Timestamps
    CCLabel* currTimeLbl = labelWithFrame(ccRect(20, currentY, 60, 14));
    currTimeLbl->text = ccs("0:00");
    currTimeLbl->textColor = color(0.6, 0.6, 0.7, 1.0);
    currTimeLbl->fontSize = 10;
    viewAddSubview(mainWindowView, currTimeLbl);

    CCLabel* totalTimeLbl = labelWithFrame(ccRect(SCREEN_W - 80, currentY, 60, 14));
    totalTimeLbl->text = ccs("-:--");
    totalTimeLbl->textColor = color(0.6, 0.6, 0.7, 1.0);
    totalTimeLbl->fontSize = 10;
    totalTimeLbl->textAlignment = CCTextAlignmentRight;
    viewAddSubview(mainWindowView, totalTimeLbl);
    
    currentY += 14 + 20; // Gap reduced to 20

    //================================================================
    // 5. Playback Controls
    //================================================================
    // Current Y should be approx 395px.
    // We have 85px remaining.
    
    int playBtnSize = 64; // Reduced slightly from 74
    int sideBtnSize = 44; // Reduced slightly from 54
    int spacing = 30;
    int centerX = SCREEN_W / 2;
    
    // We want the CENTER of the buttons to be at a specific Y,
    // ensuring they don't hit the bottom edge.
    // Let's place the button Top at currentY.
    // Button Bottom will be 395 + 64 = 459px. (Safe!)
    
    int controlsCenterY = currentY + (playBtnSize / 2);

    // A. Play Button
        CCView* playBtn = viewWithFrame(ccRect(centerX - (playBtnSize/2), currentY, playBtnSize, playBtnSize));
        playBtn->backgroundColor = color(0.0, 0.8, 1.0, 1.0);
        layerSetCornerRadius(playBtn->layer, playBtnSize / 2.0);
        playBtn->tag = TAG_MUSIC_PLAY;
        
        // --- SHADOW FIX ---
        playBtn->layer->shadowOpacity = 0.4;
        playBtn->layer->shadowRadius = 8;
        playBtn->layer->shadowOffset = ccPoint(0, 4); // <--- MUST BE SET to allocate the pointer!
        // ------------------
        
        uiMusicPlayBtnLbl = labelWithFrame(ccRect(0, 0, playBtnSize, playBtnSize));
        uiMusicPlayBtnLbl->text = ccs(">");
        uiMusicPlayBtnLbl->textColor = color(1, 1, 1, 1);
        uiMusicPlayBtnLbl->fontSize = 28;
        uiMusicPlayBtnLbl->textAlignment = CCTextAlignmentCenter;
        uiMusicPlayBtnLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(playBtn, uiMusicPlayBtnLbl);
        viewAddSubview(mainWindowView, playBtn);
    
    

    // B. Prev Button
    // Align centers vertically
    int prevX = centerX - (playBtnSize/2) - spacing - sideBtnSize;
    int sideBtnY = controlsCenterY - (sideBtnSize/2);
    
    CCView* prevBtn = viewWithFrame(ccRect(prevX, sideBtnY, sideBtnSize, sideBtnSize));
    prevBtn->backgroundColor = color(0.25, 0.25, 0.3, 1.0);
    layerSetCornerRadius(prevBtn->layer, sideBtnSize / 2.0);
    prevBtn->tag = TAG_MUSIC_PREV;

    CCLabel* prevLbl = labelWithFrame(ccRect(0, 0, sideBtnSize, sideBtnSize));
    prevLbl->text = ccs("|<");
    prevLbl->textColor = color(0.8, 0.8, 0.9, 1.0);
    prevLbl->fontSize = 14;
    prevLbl->textAlignment = CCTextAlignmentCenter;
    prevLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(prevBtn, prevLbl);
    viewAddSubview(mainWindowView, prevBtn);

    // C. Next Button
    int nextX = centerX + (playBtnSize/2) + spacing;
    
    CCView* nextBtn = viewWithFrame(ccRect(nextX, sideBtnY, sideBtnSize, sideBtnSize));
    nextBtn->backgroundColor = color(0.25, 0.25, 0.3, 1.0);
    layerSetCornerRadius(nextBtn->layer, sideBtnSize / 2.0);
    nextBtn->tag = TAG_MUSIC_NEXT;

    CCLabel* nextLbl = labelWithFrame(ccRect(0, 0, sideBtnSize, sideBtnSize));
    nextLbl->text = ccs(">|");
    nextLbl->textColor = color(0.8, 0.8, 0.9, 1.0);
    nextLbl->fontSize = 14;
    nextLbl->textAlignment = CCTextAlignmentCenter;
    nextLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(nextBtn, nextLbl);
    viewAddSubview(mainWindowView, nextBtn);
}

// percentage is 0.0 to 1.0
void update_music_progress(float percentage) {
    if (uiMusicProgressFill == NULL) return;

    // Clamp percentage safety check
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 1.0f) percentage = 1.0f;

    int maxWidth = SCREEN_W - (MAIN_PADDING * 2);
    int newWidth = (int)(maxWidth * percentage);

    // Update the width of the fill view
    uiMusicProgressFill->frame->size->width = newWidth;

    // Optimized Redraw: Only redraw the progress track area via its parent
    // Assuming the track is the parent, or just update the root if needed.
    // If you implemented update_view_area_via_parent:
    // update_view_area_via_parent(uiMusicProgressFill);
    
    // Fallback if parent optimization isn't ready:
    update_full_ui();
}

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
    i2s_channel_write(handle, pcm_samples, sample_count * sizeof(int16_t), &bytes_written, portMAX_DELAY);
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
    ESP_LOGI("REC", "Opening file for recording...");

    // 1. Open File
    FILE *f = fopen("/sdcard/recording.wav", "wb");
    if (f == NULL) {
        ESP_LOGE("REC", "Failed to open file!");
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
    ESP_LOGI("REC", "Stopping... Finalizing WAV Header.");

    // Update the header with the correct file size
    write_wav_header(f, 44100, 2, total_bytes_recorded);

    fclose(f);
    free(buffer);
    
    ESP_LOGI("REC", "File saved. Task deleting.");
    record_task_handle = NULL; // Reset handle
    vTaskDelete(NULL);
}

void start_recording(void) {
    if (is_recording_active) {
        ESP_LOGW("REC", "Already recording!");
        return;
    }
    
    if (rx_handle == NULL) {
        ESP_LOGE("REC", "I2S RX Handle is NULL! Did you run setupHybridAudio?");
        return;
    }

    is_recording_active = true;
    
    ESP_LOGI("REC", "rec_task");
    
    // Create the task (Pinned to Core 1 is usually best for file I/O)
    xTaskCreatePinnedToCore(record_wav_task, "rec_task", 1024 * 6, NULL, 5, &record_task_handle, 1);
}

void stop_recording(void) {
    if (!is_recording_active) {
        return;
    }

    ESP_LOGI("REC", "Stop requested...");
    // Flip the switch. The task will exit its while() loop and clean up.
    is_recording_active = false;
}

void record_task(void *args) {
    // 1. Open File
    FILE *f = fopen("/sdcard/recording.wav", "wb");
    if (f == NULL) {
        ESP_LOGE("REC", "Failed to open file for writing");
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

    ESP_LOGI("REC", "Start Recording...");

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

    ESP_LOGI("REC", "Recording Complete. Finalizing...");

    // 5. Finalize Header
    // Go back to the top of the file and write the correct lengths
    write_wav_header(f, SAMPLE_RATE, CHANNELS, total_bytes_recorded);

    // 6. Cleanup
    fclose(f);
    free(buffer);
    vTaskDelete(NULL);
}

// --- Gallery Globals ---
static CCArray* galleryImagePaths = NULL; // Array of CCString paths
static int galleryCurrentPage = 0;
static int gallerySelectedIdx = -1; // -1 means no selection (Grid Mode)
static float galleryZoomScale = 1.0f;

// Layout Constants
#define ITEMS_PER_PAGE 12 // 3 cols x 4 rows
#define SCREEN_W 320
#define SCREEN_H 480
#define THUMB_SIZE 90
#define THUMB_GAP 10
#define TOP_MARGIN 50

// Tags for Input
#define TAG_GAL_PREV_PAGE 100
#define TAG_GAL_NEXT_PAGE 101
#define TAG_GAL_BACK      102
#define TAG_GAL_ZOOM_IN   103
#define TAG_GAL_ZOOM_OUT  104
#define TAG_GAL_PHOTO_BASE 1000 // Photo Index = Tag - 1000

void setup_gallery_ui(void); // Forward declaration

void init_gallery_data() {
    // Only load once
    if (galleryImagePaths == NULL) {
        galleryImagePaths = array();
        // Mock Data - In reality, use your get_directory_files_as_array logic here!
        arrayAddObject(galleryImagePaths, ccs("/spiflash/files.png"));
        arrayAddObject(galleryImagePaths, ccs("/spiflash/settings.png"));
        arrayAddObject(galleryImagePaths, ccs("/spiflash/messages.png"));
        // ... add more for testing pagination ...
        //for(int i=4; i<=30; i++) {
        //     char buf[32]; snprintf(buf, 32, "/spiflash/img%d.png", i);
        //     arrayAddObject(galleryImagePaths, ccs(buf));
        //}
    }
}

void handle_gallery_touch(int tag) {
    
    // A. Handle Photo Tap (Enter Detail Mode)
    if (tag >= TAG_GAL_PHOTO_BASE) {
        gallerySelectedIdx = tag - TAG_GAL_PHOTO_BASE;
        galleryZoomScale = 1.0f; // Reset zoom
        setup_gallery_ui(); // Redraw in Detail Mode
    }
    
    // B. Handle Pagination
    else if (tag == TAG_GAL_PREV_PAGE) {
        if (galleryCurrentPage > 0) {
            galleryCurrentPage--;
            setup_gallery_ui();
        }
    }
    else if (tag == TAG_GAL_NEXT_PAGE) {
        int total = galleryImagePaths->count;
        int maxPage = (total - 1) / ITEMS_PER_PAGE;
        if (galleryCurrentPage < maxPage) {
            galleryCurrentPage++;
            setup_gallery_ui();
        }
    }
    
    // C. Handle Detail View Controls
    else if (tag == TAG_GAL_BACK) {
        gallerySelectedIdx = -1; // Return to grid
        setup_gallery_ui();
    }
    else if (tag == TAG_GAL_ZOOM_IN) {
        galleryZoomScale += 0.25f;
        if (galleryZoomScale > 3.0f) galleryZoomScale = 3.0f;
        setup_gallery_ui(); // Re-layout with new scale
    }
    else if (tag == TAG_GAL_ZOOM_OUT) {
        galleryZoomScale -= 0.25f;
        if (galleryZoomScale < 0.5f) galleryZoomScale = 0.5f;
        setup_gallery_ui();
    }
}

// --- Helper: Draw Grid ---
void layout_grid_mode(void) {
    // 1. Header
    CCLabel* title = labelWithFrame(ccRect(0, 0, SCREEN_W, 40));
    char buf[32];
    snprintf(buf, 32, "Gallery - Page %d", galleryCurrentPage + 1);
    title->text = ccs(buf);
    title->textColor = color(1, 1, 1, 1);
    title->textAlignment = CCTextAlignmentCenter;
    title->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(mainWindowView, title);

    // 2. Calculate Range
    int startIdx = galleryCurrentPage * ITEMS_PER_PAGE;
    int endIdx = startIdx + ITEMS_PER_PAGE;
    if (endIdx > galleryImagePaths->count) endIdx = galleryImagePaths->count;

    // 3. Draw Grid
    int col = 0;
    int row = 0;
    
    // Center the grid: (3 * 90) + (2 * 10) = 290px wide. Screen is 320.
    // Start X = (320 - 290) / 2 = 15.
    int startX = 15;
    
    for (int i = startIdx; i < endIdx; i++) {
        int x = startX + (col * (THUMB_SIZE + THUMB_GAP));
        int y = TOP_MARGIN + (row * (THUMB_SIZE + THUMB_GAP));
        
        // Container for Image
        CCView* photoFrame = viewWithFrame(ccRect(x, y, THUMB_SIZE, THUMB_SIZE));
        photoFrame->backgroundColor = color(0.2, 0.2, 0.2, 1.0); // Placeholder color
        photoFrame->tag = TAG_GAL_PHOTO_BASE + i; // Store Index
        layerSetCornerRadius(photoFrame->layer, 8.0);
        
        // Image View
        // (Assuming you have CCImageView. If not, use CCView with image property)
        CCImageView* img = imageViewWithFrame(ccRect(0, 0, THUMB_SIZE, THUMB_SIZE));
        
        // Use DEEP COPY to prevent crash when freeing views vs array
        CCString* path = (CCString*)arrayObjectAtIndex(galleryImagePaths, i);
        img->image = imageWithFile(copyCCString(path)); // Mock implementation
        
        // Just put a Label on top if no real image loader yet
        CCLabel* lbl = labelWithFrame(ccRect(0,0,THUMB_SIZE,THUMB_SIZE));
        lbl->text = copyCCString(path); // Show filename
        lbl->fontSize = 8;
        lbl->textAlignment = CCTextAlignmentCenter;
        viewAddSubview(photoFrame, lbl);
        
        viewAddSubview(photoFrame, img);
        viewAddSubview(mainWindowView, photoFrame);
        
        // Grid Math
        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }

    // 4. Pagination Buttons (Bottom)
    int btnY = SCREEN_H - 50;
    int btnW = 80;
    
    // Prev Button
    if (galleryCurrentPage > 0) {
        CCView* prevBtn = viewWithFrame(ccRect(10, btnY, btnW, 40));
        prevBtn->backgroundColor = color(0.3, 0.3, 0.3, 1.0);
        layerSetCornerRadius(prevBtn->layer, 5);
        prevBtn->tag = TAG_GAL_PREV_PAGE;
        
        CCLabel* l = labelWithFrame(ccRect(0,0,btnW,40));
        l->text = ccs("< Prev");
        l->textColor = color(1,1,1,1);
        l->textAlignment = CCTextAlignmentCenter;
        l->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(prevBtn, l);
        viewAddSubview(mainWindowView, prevBtn);
    }

    // Next Button
    int maxPage = (galleryImagePaths->count - 1) / ITEMS_PER_PAGE;
    if (galleryCurrentPage < maxPage) {
        CCView* nextBtn = viewWithFrame(ccRect(SCREEN_W - 10 - btnW, btnY, btnW, 40));
        nextBtn->backgroundColor = color(0.3, 0.3, 0.3, 1.0);
        layerSetCornerRadius(nextBtn->layer, 5);
        nextBtn->tag = TAG_GAL_NEXT_PAGE;
        
        CCLabel* l = labelWithFrame(ccRect(0,0,btnW,40));
        l->text = ccs("Next >");
        l->textColor = color(1,1,1,1);
        l->textAlignment = CCTextAlignmentCenter;
        l->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(nextBtn, l);
        viewAddSubview(mainWindowView, nextBtn);
    }
}

// --- Helper: Draw Detail ---
void layout_detail_mode(void) {
    // 1. Get Path
    CCString* path = (CCString*)arrayObjectAtIndex(galleryImagePaths, gallerySelectedIdx);

    // 2. Image Container (Full Screen, Black)
    // We use a container to Clip Bounds if zoomed in heavily
    CCView* container = viewWithFrame(ccRect(0, 0, SCREEN_W, SCREEN_H));
    container->backgroundColor = color(0, 0, 0, 1.0);
    container->layer->masksToBounds = true; // Clip zoom
    viewAddSubview(mainWindowView, container);

    // 3. Calculate Zoomed Frame (Aspect Fit Simulation)
    // Base size: 320 width. Assuming square images for now: 320x320.
    float baseW = 320.0f;
    float baseH = 320.0f;
    
    float finalW = baseW * galleryZoomScale;
    float finalH = baseH * galleryZoomScale;
    
    // Center it: (Screen - Final) / 2
    float finalX = (SCREEN_W - finalW) / 2.0f;
    float finalY = (SCREEN_H - finalH) / 2.0f;

    CCImageView* fullImg = imageViewWithFrame(ccRect((int)finalX, (int)finalY, (int)finalW, (int)finalH));
    fullImg->image = imageWithFile(copyCCString(path));
    //fullImg->backgroundColor = color(0.5, 0.5, 0.5, 1.0); // Placeholder
    viewAddSubview(container, fullImg);

    // Debug Label for filename
    CCLabel* fn = labelWithFrame(ccRect(0, 0, (int)finalW, 20));
    fn->text = copyCCString(path);
    //viewAddSubview(fullImg->view, fn);

    // 4. Controls Overlay
    // Back Button (Top Left)
    CCView* backBtn = viewWithFrame(ccRect(10, 30, 60, 40));
    backBtn->backgroundColor = color(0, 0, 0, 0.5); // Semi-transparent
    layerSetCornerRadius(backBtn->layer, 5);
    backBtn->tag = TAG_GAL_BACK;
    
    CCLabel* bl = labelWithFrame(ccRect(0,0,60,40));
    bl->text = ccs("Back");
    bl->textColor = color(1,1,1,1);
    bl->textAlignment = CCTextAlignmentCenter;
    bl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(backBtn, bl);
    viewAddSubview(mainWindowView, backBtn);

    // Zoom Buttons (Bottom Center)
    int zBtnSize = 50;
    int zY = SCREEN_H - 70;
    
    // Zoom Out (-)
    CCView* zOut = viewWithFrame(ccRect((SCREEN_W/2) - 60, zY, zBtnSize, zBtnSize));
    zOut->backgroundColor = color(0,0,0,0.5);
    layerSetCornerRadius(zOut->layer, 25); // Circle
    zOut->tag = TAG_GAL_ZOOM_OUT;
    
    CCLabel* lOut = labelWithFrame(ccRect(0,0,zBtnSize,zBtnSize));
    lOut->text = ccs("-");
    lOut->textColor = color(1,1,1,1);
    lOut->fontSize = 30;
    lOut->textAlignment = CCTextAlignmentCenter;
    lOut->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(zOut, lOut);
    viewAddSubview(mainWindowView, zOut);

    // Zoom In (+)
    CCView* zIn = viewWithFrame(ccRect((SCREEN_W/2) + 10, zY, zBtnSize, zBtnSize));
    zIn->backgroundColor = color(0,0,0,0.5);
    layerSetCornerRadius(zIn->layer, 25);
    zIn->tag = TAG_GAL_ZOOM_IN;

    CCLabel* lIn = labelWithFrame(ccRect(0,0,zBtnSize,zBtnSize));
    lIn->text = ccs("+");
    lIn->textColor = color(1,1,1,1);
    lIn->fontSize = 30;
    lIn->textAlignment = CCTextAlignmentCenter;
    lIn->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(zIn, lIn);
    viewAddSubview(mainWindowView, zIn);
}

// --- Main Layout Entry Point ---
void setup_gallery_ui(void) {
    currentView = CurrentViewPhotos;
    init_gallery_data();
    
    // Clean up previous UI
    // Ensure you have safe logic for this as discussed previously!
    //if (mainWindowView) freeViewHierarchy(mainWindowView);
    
    mainWindowView = viewWithFrame(ccRect(0, 0, SCREEN_W, SCREEN_H));
    mainWindowView->backgroundColor = color(0.1, 0.1, 0.1, 1.0);

    if (gallerySelectedIdx == -1) {
        layout_grid_mode();
    } else {
        layout_detail_mode();
    }
    
    update_full_ui();
}



// Keyboard Modes
typedef enum {
    KB_MODE_ABC_LOWER,
    KB_MODE_ABC_UPPER,
    KB_MODE_NUMBERS,
    KB_MODE_SYMBOLS
} KeyboardMode;

static KeyboardMode kbCurrentMode = KB_MODE_ABC_LOWER;

// Tags for special keys
#define TAG_KB_KEY_BASE 2000 // ASCII char will be added to this
#define TAG_KB_SHIFT    3001
#define TAG_KB_BACK     3002
#define TAG_KB_MODE_123 3003
#define TAG_KB_MODE_ABC 3004
#define TAG_KB_SPACE    3005
#define TAG_KB_RETURN   3006
#define TAG_KB_SYM      3007

// Layout Constants
#define KB_HEIGHT 200
#define KEY_MARGIN 2
#define KEY_HEIGHT 40

CCView* create_key_btn(const char* text, int x, int y, int w, int h, int tag) {
    // Container
    CCView* key = viewWithFrame(ccRect(x, y, w, h));
    key->backgroundColor = color(0.3, 0.3, 0.35, 1.0); // Dark Blue-Gray key
    layerSetCornerRadius(key->layer, 6.0);
    key->tag = tag;
    
    // Shadow for depth
    //key->layer->shadowOpacity = 0.3;
    //key->layer->shadowRadius = 2;
    //key->layer->shadowOffset = ccPoint(0, 2);

    // Label
    CCLabel* lbl = labelWithFrame(ccRect(0, 0, w, h));
    
    // CRITICAL: Use copyCCString to prevent double-free crashes if layout is rebuilt
    CCString* tempStr = ccs(text);
    lbl->text = copyCCString(tempStr);
    freeCCString(tempStr); // Clean up the temp wrapper
    
    lbl->textColor = color(1, 1, 1, 1);
    lbl->fontSize = 18;
    lbl->textAlignment = CCTextAlignmentCenter;
    lbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    
    viewAddSubview(key, lbl);
    return key;
}

void layout_keyboard_keys(void) {
    if (!uiKeyboardView) return;
    
    if (uiKeyboardView && uiKeyboardView->subviews) {
            for (int i = uiKeyboardView->subviews->count - 1; i >= 0; i--) {
                CCView* keyView = (CCView*)arrayObjectAtIndex(uiKeyboardView->subviews, i);
                viewRemoveFromSuperview(keyView);
                // Optionally free memory if your system doesn't auto-handle it
                 freeViewHierarchy(keyView);
            }
        }

    // 1. Clear existing keys (Clean rebuild)
    //freeViewHierarchy(uiKeyboardView);
    // Re-initialize the container logic (since freeViewHierarchy freed the struct)
    // Actually, it's safer to just remove subviews if we want to keep the container.
    // But given your framework, let's assume uiKeyboardView is just a pointer we manage.
    // Let's assume the container is passed in or managed externally.
    // Ideally: remove all subviews.
    // For now, let's assume we are rebuilding the subviews of a fresh container.

    // Row definitions based on Mode
    const char* row1 = "";
    const char* row2 = "";
    const char* row3 = "";
    
    if (kbCurrentMode == KB_MODE_ABC_LOWER) {
        row1 = "qwertyuiop";
        row2 = "asdfghjkl";
        row3 = "zxcvbnm";
    } else if (kbCurrentMode == KB_MODE_ABC_UPPER) {
        row1 = "QWERTYUIOP";
        row2 = "ASDFGHJKL";
        row3 = "ZXCVBNM";
    } else if (kbCurrentMode == KB_MODE_NUMBERS) {
        row1 = "1234567890";
        row2 = "-/:;()$&@";
        row3 = ".,?!'";
    }

    int screenW = 320;
    int keyW = (screenW - (KEY_MARGIN * 11)) / 10; // Approx 30px
    int startY = 10;
    
    // --- ROW 1 ---
    int len1 = strlen(row1);
    int x = KEY_MARGIN;
    for (int i=0; i<len1; i++) {
        char buf[2] = {row1[i], '\0'};
        // Tag is the ASCII value + Base
        viewAddSubview(uiKeyboardView, create_key_btn(buf, x, startY, keyW, KEY_HEIGHT, TAG_KB_KEY_BASE + row1[i]));
        x += keyW + KEY_MARGIN;
    }

    // --- ROW 2 ---
    int len2 = strlen(row2);
    x = KEY_MARGIN + (keyW / 2); // Indent row 2
    for (int i=0; i<len2; i++) {
        char buf[2] = {row2[i], '\0'};
        viewAddSubview(uiKeyboardView, create_key_btn(buf, x, startY + KEY_HEIGHT + KEY_MARGIN, keyW, KEY_HEIGHT, TAG_KB_KEY_BASE + row2[i]));
        x += keyW + KEY_MARGIN;
    }

    // --- ROW 3 (Shift, Letters, Back) ---
    int y3 = startY + (KEY_HEIGHT + KEY_MARGIN) * 2;
    
    // Shift/Symbol Key (Left)
    if (kbCurrentMode == KB_MODE_NUMBERS) {
        // Switch back to ABC
        viewAddSubview(uiKeyboardView, create_key_btn("ABC", KEY_MARGIN, y3, keyW*1.5, KEY_HEIGHT, TAG_KB_MODE_ABC));
    } else {
        // Shift Key
        const char* shiftLbl = (kbCurrentMode == KB_MODE_ABC_UPPER) ? "S" : "s"; // visual indicator
        viewAddSubview(uiKeyboardView, create_key_btn(shiftLbl, KEY_MARGIN, y3, keyW*1.5, KEY_HEIGHT, TAG_KB_SHIFT));
    }

    // Letters
    int len3 = strlen(row3);
    x = KEY_MARGIN + (keyW * 1.5) + KEY_MARGIN;
    for (int i=0; i<len3; i++) {
        char buf[2] = {row3[i], '\0'};
        viewAddSubview(uiKeyboardView, create_key_btn(buf, x, y3, keyW, KEY_HEIGHT, TAG_KB_KEY_BASE + row3[i]));
        x += keyW + KEY_MARGIN;
    }

    // Backspace (Right)
    int backW = screenW - x - KEY_MARGIN;
    viewAddSubview(uiKeyboardView, create_key_btn("<-", x, y3, backW, KEY_HEIGHT, TAG_KB_BACK));

    // --- ROW 4 (123, Space, Return) ---
    int y4 = startY + (KEY_HEIGHT + KEY_MARGIN) * 3;
    
    // 123 Mode
    viewAddSubview(uiKeyboardView, create_key_btn("123", KEY_MARGIN, y4, keyW*2, KEY_HEIGHT, TAG_KB_MODE_123));
    
    // Space
    int spaceW = screenW - (keyW*4) - (KEY_MARGIN*4);
    viewAddSubview(uiKeyboardView, create_key_btn("Space", KEY_MARGIN + keyW*2 + KEY_MARGIN, y4, spaceW, KEY_HEIGHT, TAG_KB_SPACE));
    
    // Return
    viewAddSubview(uiKeyboardView, create_key_btn("Ret", KEY_MARGIN + keyW*2 + KEY_MARGIN + spaceW + KEY_MARGIN, y4, screenW - (KEY_MARGIN + keyW*2 + KEY_MARGIN + spaceW + KEY_MARGIN) - KEY_MARGIN, KEY_HEIGHT, TAG_KB_RETURN));
}

void setup_keyboard_ui(CCLabel* targetLabel) {
    printf("--- Setup Keyboard UI Start ---\n");

    // 1. Set Globals
    uiTargetLabel = targetLabel;
    
    // Setup Input Buffer
    if (uiInputBuffer) freeCCString(uiInputBuffer);
    if (targetLabel && targetLabel->text) {
        uiInputBuffer = copyCCString(targetLabel->text);
    } else {
        uiInputBuffer = ccs("");
    }

    // 2. Clear previous keyboard if it exists
    if (uiKeyboardView) {
        // Since we are about to overwrite the pointer, we should ideally remove the old one
        // But for now, just NULLing it to start fresh is safe enough if freeViewHierarchy handles the rest
        uiKeyboardView = NULL;
    }

    // 3. Create Container
    // Use CC_CALLOC via viewWithFrame.
    // MAKE SURE YOU ARE NOT TYPING "CCView* uiKeyboardView =" here.
    uiKeyboardView = viewWithFrame(ccRect(0, 480 - 200, 320, 200));
    
    // --- DEBUG CHECK 1 ---
    if (uiKeyboardView == NULL) {
        printf("CRITICAL ERROR: uiKeyboardView is NULL. Out of Memory?\n");
        return;
    } else {
        printf("Keyboard View Allocated at %p\n", uiKeyboardView);
    }

    uiKeyboardView->backgroundColor = color(0.15, 0.15, 0.18, 1.0);
    uiKeyboardView->layer->shadowOpacity = 0.5;
    uiKeyboardView->layer->shadowRadius = 10;
    // Fix crash risk: Explicitly set offset
    uiKeyboardView->layer->shadowOffset = ccPoint(0, -5);

    // 4. Populate Keys
    layout_keyboard_keys();
    
    // --- DEBUG CHECK 2 ---
    if (uiKeyboardView->subviews == NULL || uiKeyboardView->subviews->count == 0) {
        printf("WARNING: Keyboard Container created, but Keys are missing!\n");
    } else {
        printf("Keyboard Keys Added: %d keys\n", uiKeyboardView->subviews->count);
    }

    // 5. Add to Main Window
    viewAddSubview(mainWindowView, uiKeyboardView);
    
    // 6. Force Update
    update_full_ui();
    printf("--- Setup Keyboard UI Done ---\n");
}

// Helper to Traverse Subviews and find touched key
CCView* find_key_at_point(int x, int y) {
    if (!uiKeyboardView || !uiKeyboardView->subviews) return NULL;
    
    // Iterate Backwards (Topmost view first)
    for (int i = uiKeyboardView->subviews->count - 1; i >= 0; i--) {
        CCView* key = (CCView*)arrayObjectAtIndex(uiKeyboardView->subviews, i);
        
        // Convert screen X/Y to Keyboard-Relative X/Y
        int localX = x - (int)uiKeyboardView->frame->origin->x;
        int localY = y - (int)uiKeyboardView->frame->origin->y;
        
        // Check intersection
        if (localX >= key->frame->origin->x &&
            localX <= key->frame->origin->x + key->frame->size->width &&
            localY >= key->frame->origin->y &&
            localY <= key->frame->origin->y + key->frame->size->height) {
            
            return key;
        }
    }
    return NULL;
}

void handle_keyboard_touch(int x, int y) {
    // 1. Find the Key
    CCView* key = find_key_at_point(x, y);
    if (!key) return;
    
    int tag = key->tag;
    
    // 2. Handle Logic
    if (tag >= TAG_KB_KEY_BASE && tag < TAG_KB_KEY_BASE + 255) {
        // --- Character Key ---
        char c = (char)(tag - TAG_KB_KEY_BASE);
        
        // Append to buffer
        char* oldStr = cStringOfString(uiInputBuffer);
        int len = strlen(oldStr);
        char* newBuf = calloc(1, len + 2); // +1 char, +1 null
        strcpy(newBuf, oldStr);
        newBuf[len] = c;
        
        freeCCString(uiInputBuffer);
        uiInputBuffer = ccs(newBuf); // ccs uses strdup/malloc
        free(newBuf);
    }
    else if (tag == TAG_KB_SPACE) {
        // Append Space
        char* oldStr = cStringOfString(uiInputBuffer);
        int len = strlen(oldStr);
        char* newBuf = calloc(1, len + 2);
        strcpy(newBuf, oldStr);
        newBuf[len] = ' ';
        
        freeCCString(uiInputBuffer);
        uiInputBuffer = ccs(newBuf);
        free(newBuf);
    }
    else if (tag == TAG_KB_BACK) {
        // Backspace
        char* oldStr = cStringOfString(uiInputBuffer);
        int len = strlen(oldStr);
        if (len > 0) {
            char* newBuf = strdup(oldStr);
            newBuf[len - 1] = '\0'; // Truncate
            
            freeCCString(uiInputBuffer);
            uiInputBuffer = ccs(newBuf);
            free(newBuf);
        }
    }
    else if (tag == TAG_KB_SHIFT) {
        // Toggle Shift
        kbCurrentMode = (kbCurrentMode == KB_MODE_ABC_LOWER) ? KB_MODE_ABC_UPPER : KB_MODE_ABC_LOWER;
        layout_keyboard_keys(); // Rebuild UI
        update_full_ui(); // Redraw
        return; // Don't update text label
    }
    else if (tag == TAG_KB_MODE_123) {
        kbCurrentMode = KB_MODE_NUMBERS;
        layout_keyboard_keys();
        update_full_ui();
        return;
    }
    else if (tag == TAG_KB_MODE_ABC) {
        kbCurrentMode = KB_MODE_ABC_LOWER;
        layout_keyboard_keys();
        update_full_ui();
        return;
    }
    else if (tag == TAG_KB_RETURN) {
        // Done editing
        // Hide keyboard logic here if you want
        printf("Return pressed. Final text: %s\n", cStringOfString(uiInputBuffer));
        return;
    }

    // 3. Update Target Label
    if (uiTargetLabel) {
        if (uiTargetLabel->text) freeCCString(uiTargetLabel->text);
        uiTargetLabel->text = copyCCString(uiInputBuffer);
        
        // Optimized Redraw
        //update_view_area_via_parent((CCView*)uiTargetLabel->view);
        //update_view_only((CCView*)uiTargetLabel->view);
        //update_full_ui();
        update_label_safe(uiTargetLabel);
    }
}

void debug_print_view_hierarchy(CCView* view, int depth) {
    if (!view) {
        printf("View is NULL\n");
        return;
    }

    // Indentation
    for (int i=0; i<depth; i++) printf("  ");

    // Print View Details
    printf("- Type: %d | Frame: %d, %d, %d, %d | Subviews: %d | Addr: %p\n",
           view->type,
           (int)view->frame->origin->x, (int)view->frame->origin->y,
           (int)view->frame->size->width, (int)view->frame->size->height,
           (view->subviews ? view->subviews->count : 0),
           view);

    // Recurse
    if (view->subviews) {
        for (int i=0; i<view->subviews->count; i++) {
            void* child = arrayObjectAtIndex(view->subviews, i);
            // Unwrap if needed (Label, etc) - simplified for View/Container check
            // Assuming child is CCView* or compatible wrapper start
             CCView* childView = (CCView*)child;
             // Check if it's a wrapper to get the real view for frame printing
             // (Logic from viewAddSubview)
             CCType type = *((CCType*)child);
             if (type == CCType_Label) childView = ((CCLabel*)child)->view;
             // ... etc
             
             debug_print_view_hierarchy(childView, depth + 1);
        }
    }
}

void setup_text_ui(void) {
    currentView = CurrentViewText;
    //if (mainWindowView) freeViewHierarchy(mainWindowView);
    mainWindowView = viewWithFrame(ccRect(0, 0, SCREEN_W, 480));
    mainWindowView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
    
    CCLabel* myLabel = labelWithFrame(ccRect(15, 0, 320-30, 120));
    myLabel->text = ccs("This is a long string that will now auto-wrap inside the box!");
    myLabel->fontSize = 12.0;
    myLabel->textAlignment = CCTextAlignmentCenter; // It will center!
    myLabel->textVerticalAlignment = CCTextVerticalAlignmentCenter; // It will center!
    myLabel->lineBreakMode = CCLineBreakWordWrapping; // It will wrap!
    myLabel->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(mainWindowView, myLabel);
    
    myLabel->view->backgroundColor = mainWindowView->backgroundColor;
    
    setup_keyboard_ui(myLabel);
    
    printf("--- UI DEBUG START ---\n");
    debug_print_view_hierarchy(mainWindowView, 0);
    printf("--- UI DEBUG END ---\n");
}

// --- WiFi Layout Globals ---
static CCView* uiWifiContainer = NULL;      // Main container
static CCView* uiWifiListContainer = NULL;  // Scrollable area for networks
static CCView* uiWifiToggleBtn = NULL;      // On/Off Switch
static bool isWifiEnabled = true;           // State Tracker

// Tag Constants
#define TAG_WIFI_TOGGLE      4000
#define TAG_WIFI_NET_BASE    4100 // Network rows start here
#define TAG_WIFI_BACK        4001

// Layout Constants
#define WIFI_ROW_HEIGHT      50
#define WIFI_HEADER_HEIGHT   140


// --- Global Data Storage ---
WifiNetwork g_wifi_scan_results[MAX_WIFI_RESULTS];
int g_wifi_scan_count = 0;
static bool wifi_initialized = false;
// --- Pagination Globals ---
static int g_wifi_page_index = 0;
#define WIFI_ITEMS_PER_PAGE  5

// New Tags for Buttons
#define TAG_WIFI_BTN_PREV    4002
#define TAG_WIFI_BTN_NEXT    4003

CCView* create_wifi_row(const char* ssid, int rssi, int index, int yPos) {
    // 1. Row Container
    CCView* row = viewWithFrame(ccRect(0, yPos, 320, WIFI_ROW_HEIGHT));
    row->backgroundColor = color(0.12, 0.12, 0.16, 1.0); // Slightly lighter background
    row->tag = TAG_WIFI_NET_BASE + index;
    
    // Separator Line (Bottom)
    CCView* line = viewWithFrame(ccRect(15, WIFI_ROW_HEIGHT - 1, 320-15, 1));
    line->backgroundColor = color(0.3, 0.3, 0.35, 1.0);
    viewAddSubview(row, line);

    // 2. SSID Label (Left)
    CCLabel* nameLbl = labelWithFrame(ccRect(15, 0, 200, WIFI_ROW_HEIGHT));
    nameLbl->text = ccs(ssid);
    nameLbl->textColor = color(1, 1, 1, 1);
    nameLbl->fontSize = 18;
    nameLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(row, nameLbl);

    // 3. Signal Strength Label (Right)
    CCLabel* signalLbl = labelWithFrame(ccRect(250, 0, 55, WIFI_ROW_HEIGHT));
    
    // Simple visual RSSI logic
    if (rssi > -60) signalLbl->text = ccs("High");
    else if (rssi > -80) signalLbl->text = ccs("Med");
    else signalLbl->text = ccs("Low");
    
    signalLbl->textColor = color(0.6, 0.6, 0.6, 1.0);
    signalLbl->fontSize = 12;
    signalLbl->textAlignment = CCTextAlignmentRight;
    signalLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(row, signalLbl);

    return row;
}

// Remove the old mockNetworks array and use the external globals

void refresh_wifi_list_ui(void) {
    if (!uiWifiListContainer) return;

    // 1. Clear previous list
    // (Assuming you have a way to clear children, otherwise we rely on rebuilding the container)
    // If you don't have removeAllSubviews, you might need to recreate uiWifiListContainer in setup_wifi_ui
    // For now, let's assume we are rebuilding the list container content.
    
    if (uiWifiListContainer->subviews) {
        // Rudimentary clear: resetting count (Warning: causes memory leak if views aren't freed)
        // Ideally: freeViewHierarchy(uiWifiListContainer); then recreate it.
        // Or loop and remove.
        uiWifiListContainer->subviews = array();
        //uiWifiListContainer->subviews->count = 0;
    }

    if (!isWifiEnabled) {
        CCLabel* msg = labelWithFrame(ccRect(0, 50, 320, 30));
        msg->text = ccs("Wi-Fi is Disabled");
        msg->textColor = color(0.5, 0.5, 0.5, 1.0);
        msg->textAlignment = CCTextAlignmentCenter;
        viewAddSubview(uiWifiListContainer, msg);
        return;
    }

    // 2. Calculate Page Slice
    int start_idx = g_wifi_page_index * WIFI_ITEMS_PER_PAGE;
    int end_idx = start_idx + WIFI_ITEMS_PER_PAGE;
    if (end_idx > g_wifi_scan_count) end_idx = g_wifi_scan_count;

    // 3. Render Rows
    int currentY = 0;
    
    // Add "No Networks" message if empty
    if (g_wifi_scan_count == 0) {
        CCLabel* msg = labelWithFrame(ccRect(0, 50, 320, 30));
        msg->text = ccs("Searching...");
        msg->textColor = color(0.6, 0.6, 0.6, 1.0);
        msg->textAlignment = CCTextAlignmentCenter;
        viewAddSubview(uiWifiListContainer, msg);
    }

    for (int i = start_idx; i < end_idx; i++) {
        // Pass 'i' as the index so the tag matches the global array index
        CCView* row = create_wifi_row(
            g_wifi_scan_results[i].ssid,
            g_wifi_scan_results[i].rssi,
            i,
            currentY
        );
        viewAddSubview(uiWifiListContainer, row);
        currentY += WIFI_ROW_HEIGHT;
    }

    // 4. Render Pagination Controls (Footer)
    // We place this below the rows
    int footerY = WIFI_ITEMS_PER_PAGE * WIFI_ROW_HEIGHT + 10;
    
    // PREV Button
    if (g_wifi_page_index > 0) {
        CCView* btnPrev = viewWithFrame(ccRect(20, footerY, 100, 40));
        btnPrev->backgroundColor = color(0.2, 0.2, 0.25, 1.0);
        layerSetCornerRadius(btnPrev->layer, 8);
        btnPrev->tag = TAG_WIFI_BTN_PREV;
        
        CCLabel* lbl = labelWithFrame(ccRect(0,0,100,40));
        lbl->text = ccs("< Prev");
        lbl->textColor = color(1,1,1,1);
        lbl->textAlignment = CCTextAlignmentCenter;
        lbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(btnPrev, lbl);
        
        viewAddSubview(uiWifiListContainer, btnPrev);
    }

    // NEXT Button
    if (end_idx < g_wifi_scan_count) {
        CCView* btnNext = viewWithFrame(ccRect(200, footerY, 100, 40));
        btnNext->backgroundColor = color(0.2, 0.2, 0.25, 1.0);
        layerSetCornerRadius(btnNext->layer, 8);
        btnNext->tag = TAG_WIFI_BTN_NEXT;
        
        CCLabel* lbl = labelWithFrame(ccRect(0,0,100,40));
        lbl->text = ccs("Next >");
        lbl->textColor = color(1,1,1,1);
        lbl->textAlignment = CCTextAlignmentCenter;
        lbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(btnNext, lbl);
        
        viewAddSubview(uiWifiListContainer, btnNext);
    }
    
    printf("wifi scan count %d", g_wifi_scan_count);
    
    // Page Number Indicator
    if (g_wifi_scan_count > WIFI_ITEMS_PER_PAGE) {
        CCLabel* pageLbl = labelWithFrame(ccRect(120, footerY, 80, 40));
        char buf[16];
        snprintf(buf, 16, "%d / %d", g_wifi_page_index + 1, (g_wifi_scan_count + WIFI_ITEMS_PER_PAGE - 1) / WIFI_ITEMS_PER_PAGE);
        pageLbl->text = ccs(buf);
        pageLbl->textColor = color(0.5, 0.5, 0.5, 1.0);
        pageLbl->textAlignment = CCTextAlignmentCenter;
        pageLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(uiWifiListContainer, pageLbl);
    }
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
    ESP_LOGI("MEM", "Free Internal Heap: %ld bytes", (long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
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

void hideTriangleAnimation(void);
void triangle_animation_task(void *pvParameter);

// Defines the variable (starts as NULL because no task is running yet)
TaskHandle_t g_triangle_task_handle = NULL;
TaskHandle_t g_image_task_handle = NULL;

// Parameters for the image animation
#define IMG_ANIM_X 130  // 160 - 50 (Centered)
#define IMG_ANIM_Y 210  // 240 - 50 (Centered)
#define IMG_ANIM_W 60
#define IMG_ANIM_H 60

void setup_wifi_ui(void) {
    // 1. Reset Root View
    // if (mainWindowView) freeViewHierarchy(mainWindowView);
    currentView = CurrentViewWifi;
    init_wifi_stack_once();//
    trigger_wifi_scan();
    
    
    mainWindowView = viewWithFrame(ccRect(0, 0, 320, 480));
    mainWindowView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);

    //================================================================
    // 2. Header Area
    //================================================================
    CCLabel* title = labelWithFrame(ccRect(0, 40, 320, 30));
    title->text = ccs("Wi-Fi Networks");
    title->textColor = color(1, 1, 1, 1);
    title->fontSize = 24;
    title->textAlignment = CCTextAlignmentCenter;
    viewAddSubview(mainWindowView, title);

    // --- Toggle Switch Area ---
    int switchY = 90;
    
    // Label
    CCLabel* lbl = labelWithFrame(ccRect(20, switchY, 100, 30));
    lbl->text = ccs("Wi-Fi");
    lbl->fontSize = 20;
    lbl->textColor = color(1,1,1,1);
    lbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    viewAddSubview(mainWindowView, lbl);

    // Toggle Button (Container)
    uiWifiToggleBtn = viewWithFrame(ccRect(240, switchY, 60, 30));
    uiWifiToggleBtn->tag = TAG_WIFI_TOGGLE;
    layerSetCornerRadius(uiWifiToggleBtn->layer, 15);
    
    // Color Logic
    if (isWifiEnabled) uiWifiToggleBtn->backgroundColor = color(0.2, 0.8, 0.4, 1.0); // Green
    else               uiWifiToggleBtn->backgroundColor = color(0.4, 0.4, 0.4, 1.0); // Grey

    // Knob (Circle)
    int knobX = isWifiEnabled ? 32 : 2;
    CCView* knob = viewWithFrame(ccRect(knobX, 2, 26, 26));
    knob->backgroundColor = color(1,1,1,1);
    layerSetCornerRadius(knob->layer, 13);
    // knob->userInteractionEnabled = false; // Pass touches through
    viewAddSubview(uiWifiToggleBtn, knob);
    
    viewAddSubview(mainWindowView, uiWifiToggleBtn);

    //================================================================
    // 3. Network List Area
    //================================================================
    uiWifiListContainer = viewWithFrame(ccRect(0, WIFI_HEADER_HEIGHT, 320, 480 - WIFI_HEADER_HEIGHT));
    // uiWifiListContainer->clipsToBounds = true; // Essential for scrolling lists
    viewAddSubview(mainWindowView, uiWifiListContainer);

    
    hideRotatingImageAnimation();
    
    
    refresh_wifi_list_ui();
    
    //hideTriangleAnimation();
    
    
    update_full_ui();
    
    
}





void hideTriangleAnimation(void) {
    if (g_triangle_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping Triangle Animation...");
        
        // 1. Delete the task
        vTaskDelete(g_triangle_task_handle);
        g_triangle_task_handle = NULL;
        
        // 2. Restore the background one last time to clean up the screen
        GraphicsCommand cmd_restore = {
            .cmd = CMD_ANIM_RESTORE_BG,
            .x = ANIM_X, .y = ANIM_Y,
            .w = ANIM_W, .h = ANIM_H
        };
        xQueueSend(g_graphics_queue, &cmd_restore, portMAX_DELAY);
        
        // 3. Flush the clean screen
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = ANIM_X, .y = ANIM_Y,
            .w = ANIM_W, .h = ANIM_H
        };
        xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
        
        // 4. Free the background buffer
        // free_anim_buffer(); // Implement this if your init_anim_buffer uses malloc
    }
}

void showRotatingImageAnimation(void) {
    // 1. Initialize buffer for background save/restore
    init_anim_buffer(); // Ensure this handles the size defined in IMG_ANIM_W/H
    
    // 2. Wait for previous UI to settle
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "CMD_ANIM_SAVE_BG for Image");
    
    // 3. Command: Snapshot the background behind where the image will spin
    GraphicsCommand cmd_save = {
        .cmd = CMD_ANIM_SAVE_BG,
        .x = IMG_ANIM_X-30, .y = IMG_ANIM_Y-30,
        .w = IMG_ANIM_W+60, .h = IMG_ANIM_H+60
    };
    xQueueSend(g_graphics_queue, &cmd_save, portMAX_DELAY);
    
    // 4. Sync: Wait for snapshot to finish
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 5. Start the Animation Task
    xTaskCreatePinnedToCore(
        rotating_image_task,
        "rot_img_task",
        4096,
        NULL,
        4,
        &g_image_task_handle, // Save handle for cancellation
        0
    );
}

void hideRotatingImageAnimation(void) {
    if (g_image_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping Rotating Image Animation...");
        
        // 1. Delete the task
        vTaskDelete(g_image_task_handle);
        g_image_task_handle = NULL;
        
        // 2. Restore background to wipe the spinner
        GraphicsCommand cmd_restore = {
            .cmd = CMD_ANIM_RESTORE_BG,
            .x = IMG_ANIM_X, .y = IMG_ANIM_Y,
            .w = IMG_ANIM_W, .h = IMG_ANIM_H
        };
        xQueueSend(g_graphics_queue, &cmd_restore, portMAX_DELAY);
        
        // 3. Flush
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = IMG_ANIM_X, .y = IMG_ANIM_Y,
            .w = IMG_ANIM_W, .h = IMG_ANIM_H
        };
        xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
        
        // 4. Free buffer
        // free_anim_buffer();
    }
}

void rotating_image_task(void *pvParameter) {
    ESP_LOGI(TAG, "rotating_image_task started");
    
    float angle = 0.0f;
    const float speed = 0.15f;
    
    // Geometry Constants
    float w = (float)IMG_ANIM_W;
    float h = (float)IMG_ANIM_H;
    float cx = IMG_ANIM_X + (w / 2.0f);
    float cy = IMG_ANIM_Y + (h / 2.0f);
    
    // Pre-calculate the "centering" matrix (It never changes)
    // This moves the image's top-left from (0,0) to (-w/2, -h/2)
    // so that (0,0) becomes the center of the image.
    Matrix3x3 mat_center_offset = TranslationMatrix(-w / 2.0f, -h / 2.0f);
    Matrix3x3 mat_screen_position = TranslationMatrix(cx, cy);

    while (1) {
        // --- STEP 1: RESTORE BACKGROUND ---
        GraphicsCommand cmd_restore = {
            .cmd = CMD_ANIM_RESTORE_BG,
            .x = IMG_ANIM_X-30, .y = IMG_ANIM_Y-30,
            .w = IMG_ANIM_W+60, .h = IMG_ANIM_H+60
        };
        xQueueSend(g_graphics_queue, &cmd_restore, 0);

        // --- STEP 2: CALCULATE TRANSFORM MATRIX ---
        
        // A. Get Rotation Matrix for current angle
        Matrix3x3 mat_rot = RotationMatrix(angle);
        
        // B. Combine: Rotate * CenterOffset
        // Order matters! This applies the offset first, then the rotation.
        Matrix3x3 mat_temp = MultiplyMatrix3x3(&mat_rot, &mat_center_offset);
        
        // C. Combine: ScreenPosition * (Rotate * CenterOffset)
        // This moves the rotated shape to the final X,Y on screen.
        Matrix3x3 mat_final = MultiplyMatrix3x3(&mat_screen_position, &mat_temp);
        
        // --- STEP 3: SEND DRAW COMMAND ---
        GraphicsCommand cmd_draw = {
            .cmd = CMD_DRAW_IMAGE_FILE,
            .x = IMG_ANIM_X, .y = IMG_ANIM_Y, // Logical bounds (for updates)
            .w = IMG_ANIM_W, .h = IMG_ANIM_H,
            .hasTransform = true,
            .transform = mat_final
        };
        
        // Set the image path safely
        strncpy(cmd_draw.imagePath, "/spiflash/loading.png", 63);
        
        xQueueSend(g_graphics_queue, &cmd_draw, 0);

        // --- STEP 4: FLUSH ---
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = IMG_ANIM_X-30, .y = IMG_ANIM_Y-30,
            .w = IMG_ANIM_W+60, .h = IMG_ANIM_H+60
        };
        xQueueSend(g_graphics_queue, &cmd_flush, 0);

        // Update angle
        angle += speed;
        if (angle > M_PI * 2) angle -= M_PI * 2;

        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
    }
}

void showTriangleAnimation(void) {
    init_anim_buffer();
    //vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "CMD_ANIM_SAVE_BG");
    
    
    
    // --- CRITICAL STEP: SYNC ---
    // We must wait for the graphics task to finish drawing the gradient
    // before we snapshot it. 500ms is plenty.
    //vTaskDelay(pdMS_TO_TICKS(1000));
    
    // --- 3. NEW: Start Animation Task (Core 0) ---
    xTaskCreatePinnedToCore(
                            triangle_animation_task,
                            "anim_task",
                            4096,
                            NULL,
                            4,    // Slightly lower priority than graphics/touch
                            &g_triangle_task_handle,
                            0     // Run on Core 0 to leave Core 1 free for rendering
                            );
}

CCPoint* getHandTip(int cx, int cy, float radius, float angleRad) {
    // -PI/2 rotates the "0" angle from 3 o'clock to 12 o'clock
    float adj = angleRad - M_PI_2;
    return ccPoint(cx + cosf(adj)*radius, cy + sinf(adj)*radius);
}

// --- CLOCK APP GLOBALS ---
typedef enum {
    TAB_WORLD_CLOCK = 0,
    TAB_CLOCK_FACE,
    TAB_ALARM,
    TAB_TIMER
} ClockTab;

static ClockTab currentClockTab = TAB_CLOCK_FACE;
static bool clock_app_running = false;
static TaskHandle_t clock_anim_task_handle = NULL;

// UI Pointers
static CCView* clockMainContainer = NULL;
static CCView* tabContentView = NULL;
static CCView* tabBar = NULL;

// Layout Constants
#define CLOCK_CENTER_X 160
#define CLOCK_CENTER_Y 210
#define CLOCK_RADIUS   100

// --- WORLD CLOCK DATA ---
typedef struct {
    char* city;
    int offset; // Hours from UTC (e.g., -5 for EST)
} TimeZone;

static TimeZone zones[] = {
    {"New York", -5}, {"London", 0}, {"Paris", 1},
    {"Tokyo", 9}, {"Sydney", 11}, {"Los Angeles", -8},
    {"Dubai", 4}, {"Hong Kong", 8}
};
static int zonesCount = 8;

// --- WORLD CLOCK PAGINATION ---
static int worldClockPage = 0;
static const int CLOCK_ITEMS_PER_PAGE = 3; // 5 items fits nicely in 420px
#define TAG_CLOCK_PREV 701
#define TAG_CLOCK_NEXT 702

// --- WORLD CLOCK GLOBALS ---
static CCView* mapContainerView = NULL;
// The actual map image
static CCImageView* worldMapImageView = NULL;
// The transparent view where we draw the darkness
static CCView* dayNightOverlayView = NULL;

// Constants for map size/position
#define MAP_X 0
#define MAP_Y 80 // Below the city list
#define MAP_W 320
#define MAP_H 120

// --- ALARM DATA ---
static int alarmHour = 7;
static int alarmMinute = 30;
static bool alarmEnabled = false;

// --- TIMER DATA ---
static int timerSecondsTotal = 0; // Set time (e.g. 60s)
static int timerSecondsLeft = 0;  // Countdown value
static bool timerRunning = false;
static uint64_t lastTimerTick = 0;

void switch_clock_tab(ClockTab tab);

void clock_animation_task(void *pvParam) {
    // 1. Wait for UI to settle
    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. Define Dirty Area (The square containing the clock)
    // Add 10px padding for safety
    int dirty_x = CLOCK_CENTER_X - CLOCK_RADIUS - 10;
    int dirty_y = CLOCK_CENTER_Y - CLOCK_RADIUS - 10;
    int dirty_w = (CLOCK_RADIUS * 2) + 20;
    int dirty_h = (CLOCK_RADIUS * 2) + 20;

    // 3. Save the Static Background (Numbers, Black BG)
    GraphicsCommand cmd_save = {
        .cmd = CMD_ANIM_SAVE_BG,
        .x = dirty_x, .y = dirty_y,
        .w = dirty_w, .h = dirty_h
    };
    xQueueSend(g_graphics_queue, &cmd_save, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow save to complete

    while (clock_app_running) {
        
        // --- 1. HANDLE TIMER LOGIC (Always runs) ---
        if (timerRunning && timerSecondsLeft > 0) {
            uint64_t now = esp_timer_get_time() / 1000; // ms
            if (now - lastTimerTick >= 1000) {
                timerSecondsLeft--;
                lastTimerTick = now;
                
                // If we are looking at the timer tab, refresh UI
                if (currentClockTab == TAB_TIMER) {
                    // Quick hack: just redraw the whole tab to update the label
                    // A better way is to update just the label text
                    // But since we are in a task, we need to trigger the main UI thread
                    // For now, assume we can trigger an update:
                    // xQueueSend(ui_event_queue, &UPDATE_MSG, 0);
                }
                
                if (timerSecondsLeft == 0) {
                    timerRunning = false;
                    // TODO: Play Sound!
                    ESP_LOGI(TAG, "TIMER FINISHED!");
                }
            }
        }

        if (currentClockTab == TAB_CLOCK_FACE) {
            
            // A. Get Time
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);

            float sec = timeinfo.tm_sec;
            float min = timeinfo.tm_min;
            float hour = timeinfo.tm_hour % 12;

            // B. Restore Background (Erase old hands)
            GraphicsCommand cmd_restore = {
                .cmd = CMD_ANIM_RESTORE_BG,
                .x = dirty_x, .y = dirty_y,
                .w = dirty_w, .h = dirty_h
            };
            xQueueSend(g_graphics_queue, &cmd_restore, 0);

            // C. Calculate Angles
            float secAng  = (sec / 60.0f) * M_PI * 2;
            float minAng  = ((min + sec/60.0f) / 60.0f) * M_PI * 2;
            float hourAng = ((hour + min/60.0f) / 12.0f) * M_PI * 2;

            // D. Draw Hands (Order: Hour -> Minute -> Second)
            
            // Hour Hand (Short, Thick)
            CCPoint* hourTip = getHandTip(CLOCK_CENTER_X, CLOCK_CENTER_Y, 50, hourAng);
            GraphicsCommand cmd_h = {
                .cmd = CMD_DRAW_ROUNDED_HAND,
                .x = CLOCK_CENTER_X, .y = CLOCK_CENTER_Y,
                .w = (int)hourTip->x, .h = (int)hourTip->y,
                .radius = 6, // 6px Thick
                .color = {200, 200, 200, 255} // Light Gray
            };
            xQueueSend(g_graphics_queue, &cmd_h, 0);

            // Minute Hand (Long, Medium)
            CCPoint* minTip = getHandTip(CLOCK_CENTER_X, CLOCK_CENTER_Y, 80, minAng);
            GraphicsCommand cmd_m = {
                .cmd = CMD_DRAW_ROUNDED_HAND,
                .x = CLOCK_CENTER_X, .y = CLOCK_CENTER_Y,
                .w = (int)minTip->x, .h = (int)minTip->y,
                .radius = 4, // 4px Thick
                .color = {255, 255, 255, 255} // White
            };
            xQueueSend(g_graphics_queue, &cmd_m, 0);

            // Second Hand (Long, Thin, Red)
            CCPoint* secTip = getHandTip(CLOCK_CENTER_X, CLOCK_CENTER_Y, 90, secAng);
            GraphicsCommand cmd_s = {
                .cmd = CMD_DRAW_ROUNDED_HAND,
                .x = CLOCK_CENTER_X, .y = CLOCK_CENTER_Y,
                .w = (int)secTip->x, .h = (int)secTip->y,
                .radius = 2, // 2px Thick
                .color = {0, 0, 255, 255} // Red (BGR format: Blue is 0, Red is 255)
            };
            xQueueSend(g_graphics_queue, &cmd_s, 0);

            // Center Dot (Cover the joints)
            // (Optional: send a CMD_DRAW_CIRCLE_FILLED here if you have one exposed)

            // E. Flush Screen
            GraphicsCommand cmd_flush = {
                .cmd = CMD_UPDATE_AREA,
                .x = dirty_x, .y = dirty_y,
                .w = dirty_w, .h = dirty_h
            };
            xQueueSend(g_graphics_queue, &cmd_flush, 0);
        }
        
        if (currentClockTab == TAB_TIMER && timerRunning) {
             // We can use a GraphicsCommand to overwrite the text area
             // Or simpler: switch_clock_tab(TAB_TIMER) call from main loop?
             // Since we are in a task, we shouldn't touch UI objects directly.
             // Ideally, send a message to main loop.
        }
        
        // 30 FPS update is plenty for a smooth second hand
        vTaskDelay(pdMS_TO_TICKS(33));
    }
    
    vTaskDelete(NULL);
}

// Helper to format time strings (HH:MM)
void formatTimeStr(char* buf, int h, int m, bool seconds) {
    if (seconds) sprintf(buf, "%02d:%02d:%02d", h, m, timerSecondsLeft % 60);
    else sprintf(buf, "%02d:%02d", h, m);
}

void setup_clock_app(void) {
    ESP_LOGI(TAG, "Starting Clock App");
    clock_app_running = true;
    currentView = CurrentViewClock;

    // 1. Create Main Container (Black Background)
    clockMainContainer = viewWithFrame(ccRect(0, 0, 320, 480));
    clockMainContainer->backgroundColor = color(0, 0, 0, 1);
    mainWindowView = clockMainContainer;

    // 2. Create Content Area (Top 420px)
    tabContentView = viewWithFrame(ccRect(0, 0, 320, 420));
    viewAddSubview(clockMainContainer, tabContentView);

    // 3. Create Tab Bar (Bottom 60px)
    tabBar = viewWithFrame(ccRect(0, 420, 320, 60));
    tabBar->backgroundColor = color(0.15, 0.15, 0.15, 1);
    viewAddSubview(clockMainContainer, tabBar);

    // Add Tab Buttons
    const char* titles[] = {"World", "Clock", "Alarm", "Timer"};
    int tabW = 320 / 4;
    
    for (int i=0; i<4; i++) {
        CCView* tab = viewWithFrame(ccRect(i*tabW, 0, tabW, 60));
        
        // Tab Label
        CCLabel* lbl = labelWithFrame(ccRect(0, 35, tabW, 20));
        lbl->text = ccs(titles[i]);
        lbl->fontSize = 12;
        lbl->textColor = (i == TAB_CLOCK_FACE) ? color(1, 0.6, 0, 1) : color(0.6, 0.6, 0.6, 1);
        lbl->textAlignment = CCTextAlignmentCenter;
        viewAddSubview(tab, lbl);
        
        // Simple Circle Icon (Placeholder for real icons)
        CCView* icon = viewWithFrame(ccRect((tabW-24)/2, 8, 24, 24));
        icon->backgroundColor = (i == TAB_CLOCK_FACE) ? color(1, 0.6, 0, 1) : color(0.4, 0.4, 0.4, 1);
        icon->layer->cornerRadius = 12;
        viewAddSubview(tab, icon);

        viewAddSubview(tabBar, tab);
    }

    // 4. Start the Animation Task
    //xTaskCreatePinnedToCore(clock_animation_task, "clock_anim", 4096, NULL, 5, &clock_anim_task_handle, 1);

    // 5. Load Initial Tab
    switch_clock_tab(TAB_CLOCK_FACE);
}

void draw_analog_face_ui(void); // Forward Declaration



void draw_analog_face_ui(void) {
    // Draw the static parts of the clock (Numbers, Ticks)
    // We do this via Views so they persist
    
    // Draw Dial Ticks
    for (int i=0; i<12; i++) {
        float angle = (i / 12.0f) * M_PI * 2;
        CCPoint* start = getHandTip(CLOCK_CENTER_X, CLOCK_CENTER_Y, CLOCK_RADIUS - 10, angle);
        CCPoint* end   = getHandTip(CLOCK_CENTER_X, CLOCK_CENTER_Y, CLOCK_RADIUS, angle);
        
        // Since we don't have a "Line View", we use small thin Views
        // Or simpler: Just rely on the Animation Task to draw the static face too?
        // BETTER: Let's put 4 main labels for 12, 3, 6, 9
    }

    CCLabel* l12 = labelWithFrame(ccRect(CLOCK_CENTER_X-20, CLOCK_CENTER_Y-CLOCK_RADIUS+10, 40, 30));
    l12->text = ccs("12"); l12->fontSize=24; l12->textAlignment=CCTextAlignmentCenter; l12->textColor=color(1,1,1,1);
    viewAddSubview(tabContentView, l12);

    CCLabel* l6 = labelWithFrame(ccRect(CLOCK_CENTER_X-20, CLOCK_CENTER_Y+CLOCK_RADIUS-40, 40, 30));
    l6->text = ccs("6"); l6->fontSize=24; l6->textAlignment=CCTextAlignmentCenter; l6->textColor=color(1,1,1,1);
    viewAddSubview(tabContentView, l6);
    
    CCLabel* l3 = labelWithFrame(ccRect(CLOCK_CENTER_X+CLOCK_RADIUS-40, CLOCK_CENTER_Y-15, 40, 30));
    l3->text = ccs("3"); l3->fontSize=24; l3->textAlignment=CCTextAlignmentCenter; l3->textColor=color(1,1,1,1);
    viewAddSubview(tabContentView, l3);
    
    CCLabel* l9 = labelWithFrame(ccRect(CLOCK_CENTER_X-CLOCK_RADIUS, CLOCK_CENTER_Y-15, 40, 30));
    l9->text = ccs("9"); l9->fontSize=24; l9->textAlignment=CCTextAlignmentCenter; l9->textColor=color(1,1,1,1);
    viewAddSubview(tabContentView, l9);
}

/*void handle_clock_touch(int x, int y, int type) {
    // Only handle "Touch Up" (Release)
    if (type != 1) return;

    // Check Tab Bar Area (Bottom 60px)
    if (y > 420) {
        int tabWidth = 320 / 4;
        int index = x / tabWidth;
        
        if (index >= 0 && index <= 3 && index != currentClockTab) {
            switch_clock_tab((ClockTab)index);
            
            // Re-trigger the BG Save if we went back to Clock Face
            // (Because switching tabs likely destroyed the screen content)
            if (index == TAB_CLOCK_FACE) {
                 // The animation task needs a signal to re-save the background
                 // A simple way is to delete and recreate the task,
                 // or use a flag/semaphore.
                 // For simplicity, recreation is safest:
                 if (clock_anim_task_handle) vTaskDelete(clock_anim_task_handle);
                 xTaskCreatePinnedToCore(clock_animation_task, "clock_anim", 4096, NULL, 5, &clock_anim_task_handle, 1);
            }
        }
    }
    
    CCView* row = find_subview_at_point(mainWindowView, x, y);
    
    // Inside handle_clock_touch...
    if (currentClockTab == TAB_WORLD_CLOCK) {
        if (row->tag == TAG_CLOCK_PREV) {
            if (worldClockPage > 0) {
                worldClockPage--;
                switch_clock_tab(TAB_WORLD_CLOCK); // Refresh
            }
        }
        else if (row->tag == TAG_CLOCK_NEXT) {
            // Safety check to ensure we don't go past end
            if ((worldClockPage + 1) * CLOCK_ITEMS_PER_PAGE < zonesCount) {
                worldClockPage++;
                switch_clock_tab(TAB_WORLD_CLOCK); // Refresh
            }
        }
    }
}*/



void draw_world_clock_ui(void) {
    // 1. Container & Base Image
    // We keep these so the UI system knows the map exists
    mapContainerView = viewWithFrame(ccRect(MAP_X, MAP_Y, MAP_W, MAP_H));
    viewAddSubview(tabContentView, mapContainerView);

    worldMapImageView = imageViewWithFrame(ccRect(0, 0, MAP_W, MAP_H));
    worldMapImageView->image = imageWithFile(ccs("/spiflash/clockmap.png"));
    viewAddSubview(mapContainerView, worldMapImageView);

    // 2. Calculate Day/Night Curve (Run Math ONCE)
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo); // UTC Time

    // A. Time Parameter (0.0 - 1.0)
    float currentMinuteUTC = (timeinfo.tm_hour * 60.0f) + timeinfo.tm_min;
    // Offset by 0.5 to center Noon on the map (assuming Greenwich centered map)
    float time01 = fmodf((currentMinuteUTC / 1440.0f) + 0.5f, 1.0f);

    // B. Season Parameter (-1.0 - 1.0)
    float dayOfYear = timeinfo.tm_yday;
    float seasonStrength = -cosf((dayOfYear / 365.0f) * M_PI * 2.0f);

    // 3. Send Draw Command
    // We queue this to happen *after* the current UI update cycle.
    // This ensures the base map image is drawn first, and we draw the darkness on top.
    
    GraphicsCommand cmd_overlay = {
        .cmd = CMD_DRAW_DAY_NIGHT_OVERLAY,
        .x = MAP_X, .y = MAP_Y, .w = MAP_W, .h = MAP_H,
        .radius = time01,         // Time
        .fontSize = seasonStrength  // Season
    };
    
    // Send to queue.
    // If your UI draws very fast, you might need a small vTaskDelay before sending this,
    // or ensure your graphics_task processes this command AFTER the standard view rendering.
    xQueueSend(g_graphics_queue, &cmd_overlay, 0);
    
    // Force a specific area update for the map just in case
    GraphicsCommand cmd_flush = {
        .cmd = CMD_UPDATE_AREA,
        .x = MAP_X, .y = MAP_Y, .w = MAP_W, .h = MAP_H
    };
    xQueueSend(g_graphics_queue, &cmd_flush, 0);
    
    int startY = MAP_Y+MAP_H;
    int rowH = 60;
    int screenW = 320;

    // 2. Calculate Range
    int totalItems = zonesCount;
    int startIndex = worldClockPage * CLOCK_ITEMS_PER_PAGE;
    int endIndex = startIndex + CLOCK_ITEMS_PER_PAGE;
    if (endIndex > totalItems) endIndex = totalItems;

    // 3. Draw Rows
    int currentY = startY;
    
    for (int i = startIndex; i < endIndex; i++) {
        // Calculate Local Time
        int h = (timeinfo.tm_hour + zones[i].offset);
        if (h < 0) h += 24;
        if (h >= 24) h -= 24;
        
        // Row Container
        CCView* row = viewWithFrame(ccRect(0, currentY, screenW, rowH));
        // Zebra striping
        row->backgroundColor = (i % 2 == 0) ? color(0.12, 0.12, 0.12, 1) : color(0.08, 0.08, 0.08, 1);
        
        // City Name
        CCLabel* lblCity = labelWithFrame(ccRect(20, 0, 200, rowH));
        lblCity->text = ccs(zones[i].city);
        lblCity->fontSize = 20;
        lblCity->textColor = color(1, 1, 1, 1);
        lblCity->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(row, lblCity);
        
        // Time String
        char timeBuf[16];
        sprintf(timeBuf, "%02d:%02d", h, timeinfo.tm_min);
        
        CCLabel* lblTime = labelWithFrame(ccRect(220, 0, 80, rowH));
        lblTime->text = ccs(timeBuf);
        lblTime->fontSize = 24;
        lblTime->textColor = color(0.8, 0.8, 0.8, 1);
        lblTime->textAlignment = CCTextAlignmentRight;
        lblTime->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(row, lblTime);
        
        viewAddSubview(tabContentView, row);
        currentY += rowH;
    }

    // 4. Draw Pagination Buttons (Bottom of list)
    int btnY = currentY + 10;
    int btnW = 100;
    int btnH = 40;

    // PREV BUTTON
    if (worldClockPage > 0) {
        CCView* btnPrev = viewWithFrame(ccRect(20, btnY, btnW, btnH));
        btnPrev->backgroundColor = color(0.3, 0.3, 0.4, 1.0);
        btnPrev->layer->cornerRadius = 5;
        btnPrev->tag = TAG_CLOCK_PREV;
        
        CCLabel* lbl = labelWithFrame(ccRect(0,0,btnW,btnH));
        lbl->text = ccs("<< Back");
        lbl->textColor = color(1,1,1,1);
        lbl->textAlignment = CCTextAlignmentCenter;
        lbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(btnPrev, lbl);
        
        viewAddSubview(tabContentView, btnPrev);
    }

    // NEXT BUTTON
    if (endIndex < totalItems) {
        CCView* btnNext = viewWithFrame(ccRect(screenW - 20 - btnW, btnY, btnW, btnH));
        btnNext->backgroundColor = color(0.2, 0.5, 0.2, 1.0);
        btnNext->layer->cornerRadius = 5;
        btnNext->tag = TAG_CLOCK_NEXT;

        CCLabel* lbl = labelWithFrame(ccRect(0,0,btnW,btnH));
        lbl->text = ccs("Next >>");
        lbl->textColor = color(1,1,1,1);
        lbl->textAlignment = CCTextAlignmentCenter;
        lbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        viewAddSubview(btnNext, lbl);
        
        viewAddSubview(tabContentView, btnNext);
    }
}

// Tags for touch handling
#define TAG_ALARM_H_UP   501
#define TAG_ALARM_H_DOWN 502
#define TAG_ALARM_M_UP   503
#define TAG_ALARM_M_DOWN 504
#define TAG_ALARM_TOGGLE 505

void draw_alarm_ui(void) {
    
    CCLabel* lblTime = labelWithFrame(ccRect(0, 80, 320, 80));
    lblTime->text = ccs("test");
    lblTime->fontSize = 60;
    lblTime->textAlignment = CCTextAlignmentCenter;
    lblTime->textColor = alarmEnabled ? color(1,1,1,1) : color(0.5,0.5,0.5,1);
    viewAddSubview(tabContentView, lblTime);
    
    // 1. Alarm Time Display (Big Center Text)
    /*char buf[16];
    formatTimeStr(buf, alarmHour, alarmMinute, false);
    
    CCLabel* lblTime = labelWithFrame(ccRect(0, 80, 320, 80));
    lblTime->text = ccs(buf);
    lblTime->fontSize = 60;
    lblTime->textAlignment = CCTextAlignmentCenter;
    lblTime->textColor = alarmEnabled ? color(1,1,1,1) : color(0.5,0.5,0.5,1);
    viewAddSubview(tabContentView, lblTime);

    // 2. Control Buttons (Row of 4 buttons)
    int btnY = 200;
    int btnW = 60;
    int gap = 15;
    int startX = (320 - (4*btnW + 3*gap)) / 2;

    // Helper to make button
    void makeBtn(int idx, char* txt, int tag) {
        CCView* btn = viewWithFrame(ccRect(startX + idx*(btnW+gap), btnY, btnW, 50));
        btn->backgroundColor = color(0.2, 0.2, 0.2, 1);
        btn->layer->cornerRadius = 10;
        btn->tag = tag;
        
        CCLabel* l = labelWithFrame(ccRect(0,0,btnW,50));
        l->text = ccs(txt); l->textAlignment = CCTextAlignmentCenter; l->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        l->textColor = color(1,1,1,1);
        viewAddSubview(btn, l);
        viewAddSubview(tabContentView, btn);
    }

    makeBtn(0, "H-", TAG_ALARM_H_DOWN);
    makeBtn(1, "H+", TAG_ALARM_H_UP);
    makeBtn(2, "M-", TAG_ALARM_M_DOWN);
    makeBtn(3, "M+", TAG_ALARM_M_UP);

    // 3. Enable Toggle
    CCView* btnToggle = viewWithFrame(ccRect(100, 300, 120, 50));
    btnToggle->backgroundColor = alarmEnabled ? color(0, 0.6, 0, 1) : color(0.6, 0.2, 0.2, 1);
    btnToggle->layer->cornerRadius = 25;
    btnToggle->tag = TAG_ALARM_TOGGLE;
    
    CCLabel* lTog = labelWithFrame(ccRect(0,0,120,50));
    lTog->text = ccs(alarmEnabled ? "ON" : "OFF");
    lTog->textAlignment = CCTextAlignmentCenter; lTog->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    lTog->textColor = color(1,1,1,1);
    viewAddSubview(btnToggle, lTog);
    viewAddSubview(tabContentView, btnToggle);*/
}

#define TAG_TIMER_START 601
#define TAG_TIMER_RESET 602
#define TAG_TIMER_ADD_1MIN 603

void draw_timer_ui(void) {
    // 1. Countdown Display
    int m = timerSecondsLeft / 60;
    int s = timerSecondsLeft % 60;
    char buf[16];
    sprintf(buf, "%02d:%02d", m, s);

    CCLabel* lblTimer = labelWithFrame(ccRect(0, 80, 320, 80));
    lblTimer->text = ccs(buf);
    lblTimer->fontSize = 60;
    lblTimer->textAlignment = CCTextAlignmentCenter;
    lblTimer->textColor = timerRunning ? color(1, 0.8, 0, 1) : color(1,1,1,1);
    viewAddSubview(tabContentView, lblTimer);

    // 2. Add Time Button (+1 Min)
    CCView* btnAdd = viewWithFrame(ccRect(110, 180, 100, 40));
    btnAdd->backgroundColor = color(0.2, 0.2, 0.2, 1);
    btnAdd->tag = TAG_TIMER_ADD_1MIN;
    CCLabel* lAdd = labelWithFrame(ccRect(0,0,100,40));
    lAdd->text = ccs("+1 Min"); lAdd->textAlignment=CCTextAlignmentCenter; lAdd->textVerticalAlignment=CCTextVerticalAlignmentCenter;
    lAdd->textColor = color(1,1,1,1);
    viewAddSubview(btnAdd, lAdd);
    viewAddSubview(tabContentView, btnAdd);

    // 3. Start/Stop Button
    CCView* btnStart = viewWithFrame(ccRect(40, 260, 100, 60));
    btnStart->backgroundColor = timerRunning ? color(0.6, 0.2, 0, 1) : color(0, 0.6, 0, 1);
    btnStart->tag = TAG_TIMER_START;
    CCLabel* lStart = labelWithFrame(ccRect(0,0,100,60));
    lStart->text = ccs(timerRunning ? "Stop" : "Start"); lStart->textAlignment=CCTextAlignmentCenter; lStart->textVerticalAlignment=CCTextVerticalAlignmentCenter;
    lStart->textColor = color(1,1,1,1);
    viewAddSubview(btnStart, lStart);
    viewAddSubview(tabContentView, btnStart);

    // 4. Reset Button
    CCView* btnReset = viewWithFrame(ccRect(180, 260, 100, 60));
    btnReset->backgroundColor = color(0.3, 0.3, 0.3, 1);
    btnReset->tag = TAG_TIMER_RESET;
    CCLabel* lReset = labelWithFrame(ccRect(0,0,100,60));
    lReset->text = ccs("Reset"); lReset->textAlignment=CCTextAlignmentCenter; lReset->textVerticalAlignment=CCTextVerticalAlignmentCenter;
    lReset->textColor = color(1,1,1,1);
    viewAddSubview(btnReset, lReset);
    viewAddSubview(tabContentView, btnReset);
}

void handle_clock_touch(int x, int y, int type) {
    if (type != 1) return; // Only on Touch Up
    
    printf("handle_clock_touch %d %d", x, y);

    // Tab Bar Logic
    if (y > 420) {
        int tabWidth = 320 / 4;
        int index = x / tabWidth;
        if (index >= 0 && index <= 3 && index != currentClockTab) {
            printf("switch_clock_tab %d", index);
            switch_clock_tab((ClockTab)index);
        }
        return;
    }

    // Tab Specific Logic
    // Helper to find clicked view by tag
    CCView* clicked = find_subview_at_point(tabContentView, x, y);
    if (!clicked) return;

    if (currentClockTab == TAB_ALARM) {
        if (clicked->tag == TAG_ALARM_H_UP) alarmHour = (alarmHour + 1) % 24;
        else if (clicked->tag == TAG_ALARM_H_DOWN) alarmHour = (alarmHour - 1 + 24) % 24;
        else if (clicked->tag == TAG_ALARM_M_UP) alarmMinute = (alarmMinute + 5) % 60;
        else if (clicked->tag == TAG_ALARM_M_DOWN) alarmMinute = (alarmMinute - 5 + 60) % 60;
        else if (clicked->tag == TAG_ALARM_TOGGLE) alarmEnabled = !alarmEnabled;
        
        switch_clock_tab(TAB_ALARM); // Redraw to update text
    }
    else if (currentClockTab == TAB_TIMER) {
        if (clicked->tag == TAG_TIMER_ADD_1MIN) {
            timerSecondsLeft += 60;
            switch_clock_tab(TAB_TIMER);
        }
        else if (clicked->tag == TAG_TIMER_RESET) {
            timerRunning = false;
            timerSecondsLeft = 0;
            switch_clock_tab(TAB_TIMER);
        }
        else if (clicked->tag == TAG_TIMER_START) {
            timerRunning = !timerRunning;
            if (timerRunning) lastTimerTick = esp_timer_get_time() / 1000;
            switch_clock_tab(TAB_TIMER);
        }
    }
    
    if (currentClockTab == TAB_WORLD_CLOCK) {
        if (clicked->tag == TAG_CLOCK_PREV) {
            if (worldClockPage > 0) {
                worldClockPage--;
                switch_clock_tab(TAB_WORLD_CLOCK); // Refresh
            }
        }
        else if (clicked->tag == TAG_CLOCK_NEXT) {
            // Safety check to ensure we don't go past end
            if ((worldClockPage + 1) * CLOCK_ITEMS_PER_PAGE < zonesCount) {
                worldClockPage++;
                switch_clock_tab(TAB_WORLD_CLOCK); // Refresh
            }
        }
    }
}

void switch_clock_tab(ClockTab tab) {
    currentClockTab = tab;
    
    // Clear previous content
    viewRemoveFromSuperview(tabContentView);
    freeViewHierarchy(tabContentView);
    tabContentView = viewWithFrame(ccRect(0, 0, 320, 420));
    viewAddSubview(clockMainContainer, tabContentView);

    // Draw new content
    if (tab == TAB_WORLD_CLOCK) draw_world_clock_ui();
    else if (tab == TAB_CLOCK_FACE) draw_analog_face_ui();
    else if (tab == TAB_ALARM) draw_alarm_ui();
    else if (tab == TAB_TIMER) draw_timer_ui();
    
    // Trigger update
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
        ESP_LOGE(TAG, "Failed to open %s", filename);
        vTaskDelete(NULL);
    }

    // 2. Get File Size for EOF checks
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    ESP_LOGI(TAG, "File Size: %ld", fileSize);

    // 3. Allocate 480x320 Buffers
    ImageTexture *frameBuffer = heap_caps_malloc(sizeof(ImageTexture), MALLOC_CAP_SPIRAM);
    frameBuffer->width = VID_W;
    frameBuffer->height = VID_H;
    
    // 480x320x4 bytes = ~600KB. Ensure your ESP32 has PSRAM enabled.
    frameBuffer->data = heap_caps_malloc(VID_W * VID_H * sizeof(ColorRGBA), MALLOC_CAP_SPIRAM);
    
    if (!frameBuffer->data) {
        ESP_LOGE(TAG, "Failed to allocate video memory! Check PSRAM.");
        fclose(f);
        vTaskDelete(NULL);
    }

    char *work = heap_caps_malloc(4096, MALLOC_CAP_INTERNAL);
    uint8_t *chunkBuf = heap_caps_malloc(READ_CHUNK_SIZE, MALLOC_CAP_INTERNAL);

    JDEC jdec;
    JpegDev dev;
    dev.texture = frameBuffer;

    ESP_LOGI(TAG, "Starting 480x320 Video...");

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
                //ESP_LOGI("GFX", "Sent Pixel Buffer Command");
                GraphicsCommand cmd = {
                    .cmd = CMD_DRAW_PIXEL_BUFFER,
                    .x = 0, .y = 0,
                    .w = VID_W, .h = VID_H, // 480x320
                    .pixelBuffer = frameBuffer->data
                };
                xQueueSend(g_graphics_queue, &cmd, portMAX_DELAY);
                
                GraphicsCommand cmd_flush = {
                    .cmd = CMD_UPDATE_AREA,
                    .x = 0, .y = 0, .w = VID_W, .h = VID_H
                };
                xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
            }
            else {
                //ESP_LOGI("GFX", "Decode Failed");
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
        ESP_LOGE("IMG", "Could not open video: %s", path);
        return NULL;
    }
    
    ESP_LOGI("IMG", "Could open video: %s", path);

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
        ESP_LOGE("IMG", "No frame found in video header");
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
    ESP_LOGI("IMG", "--- PIXEL DATA DUMP ---");

    // 1. Cast the raw bytes to a Pixel Array
    ColorRGBA *pixels = (ColorRGBA *)img->imageData;
    
    // Print First 5 Pixels (Top Left)
    for (int i = 0; i < 5; i++) {
        ColorRGBA p = pixels[i]; // Now this works
        ESP_LOGI("IMG", "Pixel[%d]: R=%d G=%d B=%d A=%d", i, p.r, p.g, p.b, p.a);
    }

    // Print Center Pixel
    int centerIdx = (height/2) * width + (width/2);
    ColorRGBA p = pixels[centerIdx];
    ESP_LOGI("IMG", "Pixel[Center]: R=%d G=%d B=%d A=%d", p.r, p.g, p.b, p.a);

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
        ESP_LOGE("VID", "Cannot open %s", path);
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

    ESP_LOGI("VID", "Starting Animation Loop...");

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
                xQueueSend(g_graphics_queue, &cmd, 0);
                
                //update_view_only(targetView);
                
                GraphicsCommand cmd_flush = {
                    .cmd = CMD_UPDATE_AREA,
                    .x = 0, .y = 0, .w = 320, .h = 480
                };
                xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
                
                
            }
        }

        // --- 5. Advance & Wait ---
        clearerr(f);
        fseek(f, frameStart + 2, SEEK_SET);
        
        // 0.1s Delay (100ms)
        //vTaskDelay(pdMS_TO_TICKS(25));
        
        int64_t t4 = esp_timer_get_time();

        ESP_LOGI(TAG, "nxt frame Time:  %lld us", (t4 - t3));
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
        xQueueSend(g_graphics_queue, &cmd, 0);*/
        
        //update_view_only(targetView);
        
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = 0, .y = 0, .w = 320, .h = 480
        };
        xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
        
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
    
    ESP_LOGI("IMG", "imageWithVideoFrame");
    
    // 3. Add to screen
    viewAddSubview(mainWindowView, videoView);
    
    
}

void setup_video_preview_ui1(void) {
    // 1. Create the Image View
    videoView = imageViewWithFrame(ccRect(0, 0, 320, 480));
    
    // 2. Load JUST the first frame (480x320 resolution)
    // Note: This might take ~200-500ms, blocking the UI briefly.
    videoView->image = imageWithFile(ccs("/sdcard/frames/img_1.png"));
    
    ESP_LOGI("IMG", "imageWithVideoFrame");
    
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

    ESP_LOGI(TAG, "Opening video file: %s", path);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file");
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
            ESP_LOGE(TAG, "Failed to allocate 512KB buffer! Trying 256KB...");
            // Fallback if your specific board is low on RAM
            file_buffer = (uint8_t *)heap_caps_malloc(1024 * 256, MALLOC_CAP_SPIRAM);
        }
    
    size_t current_data_len = 0;
    esp_h264_dec_in_frame_t in_frame = { 0 };
    esp_h264_dec_out_frame_t out_frame = { 0 };

    ESP_LOGI(TAG, "Starting decode loop...");

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
                ESP_LOGE(TAG, "Decoder Error %d. RESTARTING VIDEO to restore sync...", ret);
                
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
    ESP_LOGI("VID", "Extracting first frame from %s...", filepath);

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE("VID", "File not found");
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
                ESP_LOGI("VID", "Frame found! Converting...");
                
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
        ESP_LOGI("VID", "Success. Frame extraction complete.");
    } else {
        ESP_LOGE("VID", "Failed to decode any frame.");
    }

    return result_pixels;
}

// --- MAIN FUNCTION: GET SPECIFIC FRAME ---
// frameNumber: 0-based index (0 = first frame, 100 = 101st frame)
uint8_t* get_video_frame(const char *filepath, int width, int height, int frameNumber) {
    ESP_LOGI("VID", "Seeking Frame %d in %s...", frameNumber, filepath);

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE("VID", "File not found");
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
                    ESP_LOGI("VID", "Found Frame %d! Converting...", frameNumber);
                    
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
        ESP_LOGI("VID", "Frame extraction success.");
    } else {
        ESP_LOGE("VID", "Frame %d not found (Video too short?).", frameNumber);
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
        xQueueSend(g_graphics_queue, &cmd_flush, portMAX_DELAY);
        
        printf("load_video_poster...\n");
    }
}


void openHomeMenuItem(int tag) {
    if (tag == 0) {
        printf("open files");
        
        printf("Opening Files App...\n");
        
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        
        // B. Switch the UI
        // This function creates a new view and assigns it to global mainWindowView
        setup_files_ui();
        
        // C. Refresh the Screen
        // Since we changed the root view, we must redraw everything
        update_full_ui();
        
        //showTriangleAnimation();
        
    }
    else if (tag == 1) {
        printf("Opening Settings App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        setup_settings_ui();
        update_full_ui();
        
    }
    else if (tag == 8) {
        printf("Opening Settings App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        setup_calculator_ui();
        update_full_ui();
        
    }
    else if (tag == 7) {
        printf("Opening Settings App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        setup_music_player_ui();
        update_full_ui();
        
        printf("Starting Playback...\n");
        xTaskCreatePinnedToCore(mp3_task_wrapper, "mp3_task", 32768, NULL, 5, NULL, 0);
        
    }
    else if (tag == 6) {
        printf("Opening Settings App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        setup_gallery_ui();
        stop_recording();
        //update_full_ui();
        
    }
    else if (tag == 2) {
        printf("Opening Settings App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        setup_text_ui();
        update_full_ui();
        
    }
    else if (tag == 5) {
        printf("Opening Settings App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        setup_clock_app();
        update_full_ui();
        
    }
    else if (tag == 4) {
        /*GraphicsCommand cmd_clear = {
            .cmd = CMD_DRAW_RECT,
            .x = 0, .y = 0, .w = 320, .h = 480,
            .color = {0, 0, 0, 255}
        };
        xQueueSend(g_graphics_queue, &cmd_clear, 0);

        // 3. Start the Video Task
        // Stack size 8192 (8KB) is recommended for JPEG decoding
        xTaskCreatePinnedToCore(video_player_task, "video_task", 16384, NULL, 5, NULL, 1);*/
        
        printf("Opening Settings App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        
        currentView = CurrentViewPaint;
        //if (mainWindowView) freeViewHierarchy(mainWindowView);
        mainWindowView = viewWithFrame(ccRect(0, 0, 320, 480));
        mainWindowView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
        
        setup_video_preview_ui();
        
        //start_video_player(&fb);
        
        //load_video_poster();
        
        //setup_video_preview_ui1();
        
        update_full_ui();
        
        xTaskCreatePinnedToCore(next_frame_task, "anim_task", 16384, (void*)videoView, 5, NULL, 1);
        
        //xTaskCreatePinnedToCore(next_frame_task1, "anim_task", 16384, (void*)videoView, 5, NULL, 1);
    }
    else if (tag == 3) {
        printf("Opening Settings App...\n");
        //openedApp = true;
        
        // A. Save the current Home Menu view
        //if (mainWindowView != NULL) {
        //    push_view(mainWindowView);
        //}
        start_recording();
        //update_full_ui();
        
    }
    else if (tag == 10) {
        esp_light_sleep_start();
    }
    
    
}



void handle_wifi_touch(int x, int y) {
    // 1. Check Top-Level Controls (Toggle)
    CCView* toggle = find_subview_at_point(mainWindowView, x, y);
    
    if (x < 40 && y < 60) {
        turn_off_wifi_and_free_ram();
        close_current_app();
        return;
    }
    
    if (toggle && toggle->tag == TAG_WIFI_TOGGLE) {
        // Toggle logic...
        return;
    }

    // 2. Check List Container
    if (uiWifiListContainer) {
        CCView* target = find_subview_at_point(uiWifiListContainer, x, y);
        
        if (target) {
            int tag = target->tag;
            
            // --- PAGINATION LOGIC ---
            if (tag == TAG_WIFI_BTN_PREV) {
                if (g_wifi_page_index > 0) {
                    g_wifi_page_index--;
                    refresh_wifi_list_ui();
                    update_full_ui();
                }
            }
            else if (tag == TAG_WIFI_BTN_NEXT) {
                // Check if next page exists
                int max_pages = (g_wifi_scan_count + WIFI_ITEMS_PER_PAGE - 1) / WIFI_ITEMS_PER_PAGE;
                if (g_wifi_page_index < max_pages - 1) {
                    g_wifi_page_index++;
                    refresh_wifi_list_ui();
                    update_full_ui();
                }
            }
            // --- NETWORK SELECTION LOGIC ---
            else if (tag >= TAG_WIFI_NET_BASE) {
                int idx = tag - TAG_WIFI_NET_BASE;
                if (idx >= 0 && idx < g_wifi_scan_count) {
                    printf("Selected: %s\n", g_wifi_scan_results[idx].ssid);
                    // Open password prompt...
                }
            }
        }
    }
}

// Helper: Finds which subview is under the user's finger
// Returns: The specific child view (key, row, button), or NULL if nothing found.
CCView* find_subview_at_point(CCView* container, int globalX, int globalY) {
    if (!container || !container->subviews) return NULL;

    // 1. Calculate the Container's Global Position
    // We need this because the children's frames are relative to the Container,
    // but your touch is relative to the Screen (0,0).
    
    // (Assuming you have getAbsoluteOrigin available from scan.c,
    //  otherwise we assume container->frame->origin is screen coords if it's a top-level view)
    CCPoint containerOrigin = getAbsoluteOrigin(container);
    int containerAbsX = (int)containerOrigin.x;
    int containerAbsY = (int)containerOrigin.y;

    // 2. Convert Touch to "Container-Local" Coordinates
    int localTouchX = globalX - containerAbsX;
    int localTouchY = globalY - containerAbsY;

    // 3. Check Intersections (Iterate Backwards: Top-most view first)
    for (int i = container->subviews->count - 1; i >= 0; i--) {
        CCView* child = (CCView*)arrayObjectAtIndex(container->subviews, i);
        
        // Simple Rectangle Intersection Check
        if (localTouchX >= child->frame->origin->x &&
            localTouchX <= child->frame->origin->x + child->frame->size->width &&
            localTouchY >= child->frame->origin->y &&
            localTouchY <= child->frame->origin->y + child->frame->size->height) {
            
            return child; // Found it!
        }
    }

    return NULL; // Touched empty space/background
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
                
                ESP_LOGI(TAG, "Touch Pressed: X=%u, Y=%u", touch_data.tp[0].x, touch_data.tp[0].y);
                
                // 2. Lock and post the "event" for LVGL to consume
                xSemaphoreTake(g_touch_mutex, portMAX_DELAY);
                g_last_x = touch_data.tp[0].x;
                g_last_y = touch_data.tp[0].y;
                g_last_state = LV_INDEV_STATE_PR; // Set the "event"
                xSemaphoreGive(g_touch_mutex);
                
                int text_w = 200;
                int text_h = 30;
                
                ColorRGBA blue = {.r = 0, .g = 0, .b = 50, .a = 255};
                
                ESP_LOGI(TAG, "Graphics Command");
                
                int x1 = randNumberTo(320);
                int y1 = randNumberTo(480);
                
                x1 = 100;
                y1 = 410;
                
                ESP_LOGI(TAG, "Graphics Command %d %d", x1, y1);
                
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
                    //xQueueSend(g_graphics_queue, &cmd_clear, 0);
                    
                    ColorRGBA white2 = {.r = 255, .g = 255, .b = 255, .a = 255};
                    
                    // --- 2. Send command to draw the text ---
                    GraphicsCommand cmd_text;
                    cmd_text.cmd = CMD_DRAW_TEXT;
                    cmd_text.x = x1;
                    cmd_text.y = y1; // Your renderText needs to handle Y as baseline
                    cmd_text.text = strdup("Tapped!");
                    //xQueueSend(g_graphics_queue, &cmd_text, 0);
                    
                    // --- 2. Send command to draw the text ---
                    GraphicsCommand cmd_text1;
                    cmd_text1.cmd = CMD_DRAW_TEXT;
                    cmd_text1.x = 20;
                    cmd_text1.y = 380; // Your renderText needs to handle Y as baseline
                    cmd_text1.text = strdup(" Q  W  E  R  T  Y  U  I  O  P");
                    cmd_text1.color = white2;
                    xQueueSend(g_graphics_queue, &cmd_text1, 0);
                    
                    // --- 2. Send command to draw the text ---
                    GraphicsCommand cmd_text12;
                    cmd_text12.cmd = CMD_DRAW_TEXT;
                    cmd_text12.x = 20;
                    cmd_text12.y = 420; // Your renderText needs to handle Y as baseline
                    cmd_text12.text = strdup("   A  S  D  F  G  H  J  K  L");
                    cmd_text12.color = white2;
                    xQueueSend(g_graphics_queue, &cmd_text12, 0);
                    
                    // --- 2. Send command to draw the text ---
                    GraphicsCommand cmd_text13;
                    cmd_text13.cmd = CMD_DRAW_TEXT;
                    cmd_text13.x = 20;
                    cmd_text13.y = 460; // Your renderText needs to handle Y as baseline
                    cmd_text13.text = strdup("     Z  X  C  V  B  N  M");
                    cmd_text13.color = white2;
                    xQueueSend(g_graphics_queue, &cmd_text13, 0);
                    
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
                    xQueueSend(g_graphics_queue, &cmd_text2, 0);
                    
                    GraphicsCommand cmd_star;
                    cmd_star.cmd = CMD_DRAW_STAR;
                    cmd_star.x = 20;
                    cmd_star.y = 460; // Your renderText needs to handle Y as baseline
                    //xQueueSend(g_graphics_queue, &cmd_star, 0);
                    
                    GraphicsCommand cmd_update;
                    cmd_update.cmd = CMD_UPDATE_AREA;
                    cmd_update.x = 0;
                    cmd_update.y = 0; // A rough bounding box
                    cmd_update.w = 320;
                    cmd_update.h = 480; // A bit larger to be safe
                    xQueueSend(g_graphics_queue, &cmd_update, 0);
                    
                    
                    
                    if (!addedCursor) {
                        addedCursor = true;
                        if (setup_cursor_buffers() != ESP_OK) {
                            ESP_LOGE(TAG, "FATAL: Failed to set up cursor backup buffers. Halting task.");
                            vTaskDelete(NULL);
                            return;
                        }
                        ESP_LOGI(TAG, "Cursor backup buffer allocated.");
                        
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
                        //xQueueSend(g_graphics_queue, &cmd_cursor_setup, 0);
                    }
                    
                    
                    
                    /*ESP_LOGI(TAG, "Graphics Command testGraphics");
                     for (int i = 0; i < 10; i++){
                     x1 = 200;
                     y1 = (int)(30 * i);
                     
                     
                     ESP_LOGI(TAG, "Graphics Command %d %d", x1, y1);
                     
                     // --- 1. Send command to clear the area ---
                     GraphicsCommand cmd_clear;
                     cmd_clear.cmd = CMD_DRAW_RECT;
                     cmd_clear.x = x1;
                     cmd_clear.y = y1;
                     cmd_clear.w = text_w;
                     cmd_clear.h = text_h;
                     cmd_clear.color = blue;
                     //xQueueSend(g_graphics_queue, &cmd_clear, 0);
                     
                     // --- 2. Send command to draw the text ---
                     GraphicsCommand cmd_text;
                     cmd_text.cmd = CMD_DRAW_TEXT;
                     cmd_text.x = x1;
                     cmd_text.y = y1; // Your renderText needs to handle Y as baseline
                     strncpy(cmd_text.text, "Tapped!", 50);
                     xQueueSend(g_graphics_queue, &cmd_text, 0);
                     
                     }*/
                    
                    
                    
                    // --- 3. Send command to update the screen ---
                    /*GraphicsCommand cmd_update;
                     cmd_update.cmd = CMD_UPDATE_AREA;
                     cmd_update.x = x1;
                     cmd_update.y = y1 - text_h; // A rough bounding box
                     cmd_update.w = text_w;
                     cmd_update.h = text_h * 2; // A bit larger to be safe
                     xQueueSend(g_graphics_queue, &cmd_update, 0);*/
                    
                    
                }
                else {
                    ColorRGBA black_alpha = {.r = 0, .g = 0, .b = 0, .a = 255};
                    GraphicsCommand cmd_text;
                    cmd_text.cmd = CMD_DRAW_TEXT;
                    cmd_text.x = 30+keyboardCursorPosition*20;
                    cmd_text.y = 53; // Your renderText needs to handle Y as baseline
                    cmd_text.color = black_alpha;
                    cmd_text.text = strdup(letterForCoordinate());
                    //xQueueSend(g_graphics_queue, &cmd_text, 0);
                    
                    keyboardCursorPosition++;
                    
                    /*GraphicsCommand cmd_update;
                     cmd_update.cmd = CMD_UPDATE_AREA;
                     cmd_update.x = cmd_text.x;
                     cmd_update.y = cmd_text.y; // A rough bounding box
                     cmd_update.w = 30;
                     cmd_update.h = 30; // A bit larger to be safe
                     xQueueSend(g_graphics_queue, &cmd_update, 0);*/
                    
                    GraphicsCommand cmd_update;
                    cmd_update.cmd = CMD_UPDATE_AREA;
                    cmd_update.x = 0;
                    cmd_update.y = 0; // A rough bounding box
                    cmd_update.w = 320;
                    cmd_update.h = 100; // A bit larger to be safe
                    //xQueueSend(g_graphics_queue, &cmd_update, 0);
                    
                    if (!setupui) {
                        
                        // 1. Create the high-level View Objects (RAM only)
                        // Make sure setup_ui_demo assigns to the global 'mainWindowView'
                        setup_ui_demo();
                        
                        // 2. Trigger the first render (Generates commands)
                        //update_full_ui();
                        
                        setupui = true;
                    }
                    
                    
                    
                    ESP_LOGI(TAG, "app_main() finished. Tasks are running.");
                }
                
            }
            else {
                ESP_LOGI(TAG, "touch 3");
                int curr_y = touch_data.tp[0].y;
                if (myDemoTextView != NULL) {
                    g_active_scrollview = myDemoTextView->scrollView;
                    
                    if (notFirstTouch == false) {
                        notFirstTouch = true;
                        g_touch_last_y = curr_y;
                        lastOffY = g_active_scrollview->contentOffset->y;
                        ESP_LOGI(TAG, "set lastOffY %d", lastOffY);
                    }
                    
                    if (g_active_scrollview) {
                        
                        int delta = g_touch_last_y - curr_y; // Drag Up = Positive Delta
                        ESP_LOGI(TAG, "g_active_scrollview %d", delta);
                        
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
                        ESP_LOGI(TAG, "g_active_scrollview1 %f %f %f", lastOffY, newOffY, yValue);
                        scrollViewSetContentOffset(g_active_scrollview, &targetPoint);//ccPoint(0, lastOffY)
                        ESP_LOGI(TAG, "g_active_scrollview contentOffset %f", g_active_scrollview->contentOffset->y);
                        
                        // 2. THROTTLING CHECK: Only draw if 0.2s has passed
                        int64_t now = esp_timer_get_time();
                        
                        if ((now - last_draw_time) > draw_interval_us) {
                            
                            // Send the expensive command
                            update_view_only(g_active_scrollview->view);
                            
                            ESP_LOGI(TAG, "g_active_scrollview contentOffset %f", g_active_scrollview->contentOffset->y);
                            
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
                if (g_active_scrollview) ESP_LOGI(TAG, "Touch Released");
                //update_view_only(g_active_scrollview->view);
                /*if (touch_data.tp[0].y < 50 && touch_data.tp[0].y > 0 && g_active_scrollview) {
                 CCPoint offsetPt = {.x = 0, .y = newOffY};
                 scrollViewSetContentOffset(g_active_scrollview, &offsetPt);
                 update_view_only(g_active_scrollview->view);
                 ESP_LOGI(TAG, "g_active_scrollview contentOffset %f", g_active_scrollview->contentOffset->y);
                 }*/
            }
        }
        
       /* if (touch_data.touch_count > 0 && setupui == true) {
            ESP_LOGI(TAG, "g_active_scrollview0");
            int curr_y = touch_data.tp[0].y;
            
            if (myDemoTextView != NULL) {
                g_active_scrollview = myDemoTextView->scrollView;
                ESP_LOGI(TAG, "g_active_scrollview2");
            } else {
                g_active_scrollview = NULL; // Safe default
            }
            
            if (!is_currently_pressed) {
                // --- FINGER DOWN ---
                is_currently_pressed = true;
                g_touch_last_y = curr_y;
                
                // Hit Test logic (Pseudo-code)
                // CCView* touched = hitTest(mainWindowView, x, y);
                // Check if 'touched' is inside our specific 'myDemoTextView->scrollView'
                
                // SAFETY CHECK: Only assign if the object exists
                ESP_LOGI(TAG, "g_active_scrollview1");
                
            }
            else {
                // --- FINGER DRAG ---
                if (g_active_scrollview) {
                    ESP_LOGI(TAG, "g_active_scrollview");
                    int delta = g_touch_last_y - curr_y; // Drag Up = Positive Delta
                    
                    // Calculate new offset
                    float currentOffY = g_active_scrollview->contentOffset->y;
                    float newOffY = currentOffY + delta;
                    
                    // Apply offset (This function handles clamping and moving the view)
                    CCPoint offsetPt = {0, newOffY};
                    scrollViewSetContentOffset(g_active_scrollview, &offsetPt);
                    
                    ESP_LOGI(TAG, "scrollViewSetContentOffset %f", newOffY);
                    
                    // Trigger Screen Refresh
                    update_view_only(g_active_scrollview->view);
                    
                    g_touch_last_y = curr_y;
                }
            }
        } else {
            // --- FINGER UP ---
            is_currently_pressed = false;
            g_active_scrollview = NULL;
        }*/
        
        //is_currently_pressed = false; //changed dec13 11-17a
        
        if (touchEnabled) {
            if (touch_data.touch_count > 0 && setupui == true) {
                int touchX = touch_data.tp[0].x;
                int touchY = touch_data.tp[0].y;
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
            else if (currentView == CurrentViewFiles || currentView == CurrentViewSettings  || currentView == CurrentViewCalculator  || currentView == CurrentViewMusic  || currentView == CurrentViewPhotos || currentView == CurrentViewText || currentView == CurrentViewClock) {
                //else if (currentView == CurrentViewFiles || currentView == CurrentViewSettings)
                if (touch_data.touch_count > 0 && setupui == true) {
                    int x = touch_data.tp[0].x;
                    int y = touch_data.tp[0].y;
                    
                    checkAvailableMemory();
                    
                    if (x < 40 && y < 60) {
                        close_current_app();
                    }
                    
                    if (currentView == CurrentViewSettings) {
                        /*CCView* row = find_subview_at_point(mainWindowView, x, y);
                        if (row) {
                            printf("open row %d", row->tag);
                            if (row->tag == 0) {
                                
                            }
                            else if (row->tag == 1) {
                                
                            }
                            else if (row->tag == 2) {
                                
                            }
                            else if (row->tag == 3) {
                                if (mainWindowView != NULL) {
                                    push_view(mainWindowView);
                                }
                                showTriangleAnimation();
                                setup_wifi_ui();
                            }
                            else if (row->tag == 4) {
                                
                            }
                        }*/
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
                        handle_clock_touch(x, y, 1);
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
 ESP_LOGI(TAG, "Initializing I2C Master for touch panel");
 
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
 ESP_LOGI(TAG, "Creating I2C panel IO handle");
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
 ESP_LOGE(TAG, "Touch panel init failed, continuing without touch...");
 } else {
 ESP_LOGI(TAG, "Touch panel initialized successfully");
 
 // --- 4. Register Touch Input with LVGL ---
 ESP_LOGI(TAG, "Registering touch input with LVGL");
 
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
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
            break;
        case WIFI_AUTH_OWE:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
            break;
        case WIFI_AUTH_WEP:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
            break;
        case WIFI_AUTH_WPA_PSK:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
            break;
        case WIFI_AUTH_WPA2_PSK:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
            break;
        case WIFI_AUTH_WPA_WPA2_PSK:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
            break;
        case WIFI_AUTH_ENTERPRISE:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
            break;
        case WIFI_AUTH_WPA3_PSK:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
            break;
        case WIFI_AUTH_WPA2_WPA3_PSK:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
            break;
        case WIFI_AUTH_WPA3_ENTERPRISE:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENTERPRISE");
            break;
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_ENTERPRISE");
            break;
        case WIFI_AUTH_WPA3_ENT_192:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
            break;
        default:
            ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
            break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
        case WIFI_CIPHER_TYPE_NONE:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
            break;
        case WIFI_CIPHER_TYPE_WEP40:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
            break;
        case WIFI_CIPHER_TYPE_WEP104:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
            break;
        case WIFI_CIPHER_TYPE_TKIP:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
            break;
        case WIFI_CIPHER_TYPE_CCMP:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
            break;
        case WIFI_CIPHER_TYPE_TKIP_CCMP:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
            break;
        case WIFI_CIPHER_TYPE_AES_CMAC128:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
            break;
        case WIFI_CIPHER_TYPE_SMS4:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
            break;
        case WIFI_CIPHER_TYPE_GCMP:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
            break;
        case WIFI_CIPHER_TYPE_GCMP256:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
            break;
        default:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
            break;
    }
    
    switch (group_cipher) {
        case WIFI_CIPHER_TYPE_NONE:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
            break;
        case WIFI_CIPHER_TYPE_WEP40:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
            break;
        case WIFI_CIPHER_TYPE_WEP104:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
            break;
        case WIFI_CIPHER_TYPE_TKIP:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
            break;
        case WIFI_CIPHER_TYPE_CCMP:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
            break;
        case WIFI_CIPHER_TYPE_TKIP_CCMP:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
            break;
        case WIFI_CIPHER_TYPE_SMS4:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
            break;
        case WIFI_CIPHER_TYPE_GCMP:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
            break;
        case WIFI_CIPHER_TYPE_GCMP256:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
            break;
        default:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
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
        ESP_LOGE(TAG, "Memory Allocation for scan config failed!");
        return;
    }
    array_2_channel_bitmap(channel_list, CHANNEL_LIST_SIZE, scan_config);
    esp_wifi_scan_start(scan_config, true);
    free(scan_config);
    
#else
    esp_wifi_scan_start(NULL, true);
#endif /*USE_CHANNEL_BITMAP*/
    
    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
        ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
    }
}




#define TARGET_APP_MAIN_ADDRESS 0x4200aec8

// Function pointer to the loaded program entry
void (*loaded_program_entry)(void);

// This is an *oversimplified* attempt and likely only works if the
// binary is specifically compiled as Position-Independent Code (PIC)
// or if you manually calculate the needed offsets.

void print_heap_info() {
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_EXEC);// |MALLOC_CAP_EXEC|MALLOC_CAP_32BIT
    printf("Largest contiguous block for EXEC/SPIRAM: %zu bytes\n", largest_free_block);
}

long get_file_size(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

void load_and_execute_program(const char* file_path) {
    
    // 1. Determine the size of the file
    struct stat st;
    if (stat(file_path, &st) != 0) {
        printf("Failed to get file size for %s.\n", file_path);
        return;
    }
    size_t file_size = st.st_size;
    printf("File size to load: %zu bytes\n", file_size);
    
    // 2. Allocate memory for the program from Executable PSRAM
    // MALLOC_CAP_SPIRAM: Use External RAM (PSRAM)
    // MALLOC_CAP_EXEC: Required for executing code from this memory
    // MALLOC_CAP_32BIT: Required for aligned 32-bit instructions
    print_heap_info();
    void* program_memory = heap_caps_malloc(file_size,
                                            MALLOC_CAP_SPIRAM |
                                            MALLOC_CAP_EXEC |
                                            MALLOC_CAP_32BIT);
    
    if (!program_memory) {
        printf("Memory allocation failed for program (needed %zu bytes).\n", file_size);
        return;
    }
    printf("Successfully allocated memory at address: %p\n", program_memory);
    
    // 3. Open and Read the binary into allocated memory
    FILE* program_file = fopen(file_path, "rb");
    if (!program_file) {
        printf("Failed to open program file.\n");
        heap_caps_free(program_memory);
        return;
    }
    
    size_t bytes_read = fread(program_memory, 1, file_size, program_file);
    fclose(program_file);
    
    if (bytes_read != file_size) {
        printf("Error reading file! Read %zu of %zu bytes.\n", bytes_read, file_size);
        heap_caps_free(program_memory);
        return;
    }
    
    // 4. Point and Jump to the loaded program (app_main is at the start)
    // NOTE: This only works if 'app_main' is the *very first instruction*
    // in your raw .bin file, which is highly unlikely for a full ESP-IDF bin.
    // The REAL RELOCATION PROBLEM still exists after this step.
    loaded_program_entry = (void (*)(void))program_memory;
    
    printf("Jumping to loaded program...\n");
    loaded_program_entry();
    
    // 5. Clean up
    heap_caps_free(program_memory);
    printf("Returned to main program after running loaded program.\n");
}



/**
 * @brief Lists all files and directories in the given mount point.
 * * @param mount_point The base path where the filesystem is mounted (e.g., "/fat").
 */
void list_directory_contents(const char *mount_point) {
    DIR *dir = NULL;
    struct dirent *ent;
    char full_path[128];
    struct stat st;
    
    ESP_LOGI(TAG, "--- Files in %s partition: ---", mount_point);
    
    // 1. Open the directory stream
    dir = opendir(mount_point);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", mount_point);
        return;
    }
    
    // 2. Read entries one by one
    while ((ent = readdir(dir)) != NULL) {
        // Skip '.' (current dir) and '..' (parent dir) entries which are common in POSIX
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        
        // Construct the full path to get file size
        snprintf(full_path, sizeof(full_path), "%s/%s", mount_point, ent->d_name);
        
        // 3. Get file status (size and type)
        if (stat(full_path, &st) == 0) {
            if (ent->d_type == DT_DIR) {
                // Directories
                printf("  [DIR]  %s\n", ent->d_name);
            } else {
                // Regular Files
                printf("  [FILE] %s (Size: %ld bytes)\n", ent->d_name, st.st_size);
            }
        } else {
            ESP_LOGW(TAG, "Failed to stat file/dir: %s", full_path);
        }
    }
    
    // 4. Close the directory stream
    closedir(dir);
    ESP_LOGI(TAG, "--- Listing Complete ---");
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
    
    ESP_LOGI(TAG, "Mounting FATFS partition '%s' at %s...", PARTITION_LABEL, MOUNT_POINT);
    
    // Mount the FAT partition with Read/Write (RW) and Wear Leveling (WL) support
    /*esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(
     MOUNT_POINT,        // Base path (e.g., /fat)
     PARTITION_LABEL,    // Partition Label (e.g., fat_data)
     &mount_config,
     &wl_handle          // Wear Leveling handle (output)
     );*/
    
    esp_err_t ret = esp_vfs_fat_spiflash_mount_ro("/spiflash", "storage", &mount_config);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "FATFS mounted successfully.");
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
    
    ESP_LOGI(TAG, "Free heap size: %zu bytes\n", free_heap_size);
    ESP_LOGI(TAG, "Free internal heap size: %zu bytes\n", free_internal_heap_size);
    
    //ESP_LOGI(TAG, "EXECUTABLE 2\n");
    
}

// Function to draw a block of pixels (240x240)
void raw_drawing_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting raw drawing demo.");
    
    // Create a pixel buffer for the entire screen (240 * 240 pixels * 2 bytes/pixel)
    // This is a large buffer, ensure your PSRAM is enabled/available.
    // If you don't have enough RAM/PSRAM, reduce the size or use a small tile.
    uint16_t *color_data = (uint16_t *)heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!color_data) {
        ESP_LOGE(TAG, "Failed to allocate display buffer!");
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
        
        ESP_LOGI(TAG, "Drew a red screen.");
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
    ESP_LOGI(TAG, "LVGL UI Task started.");
    
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
    ESP_LOGI(TAG, "LVGL Keyboard Demo Task started.");
    
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
     if (xSemaphoreTake(joystick_mutex, portMAX_DELAY) == pdTRUE) {
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
        ESP_LOGI(TAG, "X-Axis Raw: %d | Y-Axis Raw: %d", adc_raw_x, adc_raw_y);
        // =======================================================
        
        // Safely update global variables
        if (xSemaphoreTake(joystick_mutex, portMAX_DELAY) == pdTRUE) {
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
    ESP_LOGI(TAG, "Starting I2C scan (NG Driver)...");
    
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
    
    ESP_LOGI(TAG, "Scanning for I2C devices...");
    
    for (uint8_t address = 1; address < 127; address++) {
        // 2. Probe the address. This function sends the address and checks for an ACK.
        // 50ms timeout is more than enough.
        esp_err_t ret = i2c_master_probe(bus_handle, address, 50);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at address 0x%02X", address);
        } else if (ret != ESP_ERR_TIMEOUT && ret != ESP_ERR_NOT_FOUND) {
            // Log other errors if they occur
            ESP_LOGW(TAG, "Error 0x%X checking address 0x%02X", ret, address);
        }
    }
    
    // 3. Clean up the bus
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    ESP_LOGI(TAG, "I2C scan complete.");
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
        xQueueSendFromISR(touch_intr_queue, &event, NULL);
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
        if (xQueueReceive(touch_intr_queue, &event, portMAX_DELAY)) {
            
            // Interrupt received, scan the touch controller
            esp_err_t ret = ft6336u_scan(&touch_dev, &touch_data);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to scan touch: %s", esp_err_to_name(ret));
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
                // ESP_LOGI(TAG, "Touch Released");
            } else {
                for (int i = 0; i < touch_data.touch_count; i++) {
                    ESP_LOGI(TAG, "Touch %d: (%s) X=%u, Y=%u",
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
 ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
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
    
    touch_intr_queue = xQueueCreate(10, sizeof(uint32_t));
    
    ESP_LOGI(TAG, "Touch driver initialized successfully.");
    
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
    
    ESP_LOGI(TAG, "Interrupt handler installed. Ready for touch.");
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
    ESP_LOGI(TAG, "Starting FT6336U Touch Example");
    
    // 1. Initialize I2C Bus
    ESP_ERROR_CHECK(i2c_master_init());
    
    // 2. Initialize FT6336U Driver
    // We pass the bus handle, and the driver adds itself as a device
    ESP_ERROR_CHECK(ft6336u_init(&touch_dev, i2c_bus_handle, RST_N_PIN, INT_N_PIN));
    
    ESP_LOGI(TAG, "Touch driver initialized successfully.");
    
    // 3. Create the interrupt queue
    touch_intr_queue = xQueueCreate(10, sizeof(uint32_t));
    
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
    
    ESP_LOGI(TAG, "Interrupt handler installed. Ready for touch.");
}

/*void testCPUGraphicsBenchmark(void) {
    // 1. Allocate Framebuffer in PSRAM (Slow access) or SRAM (Fast access)
    // For benchmarking math, SRAM is better, but for full frame, you likely need PSRAM.
    int width = 320;
    int height = 240;
    size_t fbSize = width * height * 3;
    
    uint8_t *display_buffer = (uint8_t *)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (!display_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return;
    }

    Framebuffer fb = {
        .displayWidth = width,
        .displayHeight = height,
        .pixelData = display_buffer,
        .colorMode = COLOR_MODE_BGR888
    };

    Gradient grad;
        grad.angle = 45.0f * (M_PI / 180.0f);
        grad.type = GRADIENT_TYPE_LINEAR; // Initialize the new enum field
        grad.numStops = 2;

        // 1. Create the storage for the stops (on the stack)
        ColorStop stops_storage[2];

        // 2. Point the struct pointer to this storage
        grad.stops = stops_storage;

        // 3. Now it is safe to assign values
        grad.stops[0].color = (ColorRGBA){255, 0, 0, 255};
        grad.stops[0].position = 0.0f;
        
        grad.stops[1].color = (ColorRGBA){0, 0, 255, 255};
        grad.stops[1].position = 1.0f;

    // --- BENCHMARK 1: ORIGINAL (Float) ---
    // Warm up cache
    fillRectangleWithGradientExtended(&fb, 0, 0, width, height, 0, 0, width, height, &grad, NULL);
    
    int64_t t1 = esp_timer_get_time();
    fillRectangleWithGradientExtended(&fb, 0, 0, width, height, 0, 0, width, height, &grad, NULL);
    int64_t t2 = esp_timer_get_time();
    
    ESP_LOGI(TAG, "Original (Float) Time: %lld us", (t2 - t1));

    // --- BENCHMARK 2: SIMD (Fixed Point) ---
    // Warm up cache
    fillRectangleWithGradientSIMD(&fb, 0, 0, width, height, &grad);

    int64_t t3 = esp_timer_get_time();
    fillRectangleWithGradientSIMD(&fb, 0, 0, width, height, &grad);
    int64_t t4 = esp_timer_get_time();

    ESP_LOGI(TAG, "SIMD (Fixed Pt) Time:  %lld us", (t4 - t3));
    ESP_LOGI(TAG, "Speedup Factor:        %.2fx", (float)(t2 - t1) / (float)(t4 - t3));

    free(display_buffer);
}*/


void testCPUGraphics(void) {
    // 2. Allocate the single framebuffer in PSRAM
    int width = DISPLAY_HORIZONTAL_PIXELS;
    int height = DISPLAY_VERTICAL_PIXELS;
    int num_pixels = width * height;
    
    // Allocate Display buffer (BGR888, 3 bytes/pixel)
    // Size = 320 * 480 * 3 bytes = 460,800 bytes
    uint8_t *display_buffer = (uint8_t *)heap_caps_malloc(num_pixels * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (!display_buffer) {
        ESP_LOGE(TAG, "Failed to allocate display buffer in PSRAM");
        return;
    }
    
    // 3. Initialize the CPUGraphics Framebuffer struct
    Framebuffer fb = {
        .displayWidth = width,
        .displayHeight = height,
        .pixelData = display_buffer,          // Point to our BGR buffer
        .colorMode = COLOR_MODE_BGR888, // Set the direct-write mode
        //.colors = {0}
    };
    
    ESP_LOGI(TAG, "Framebuffer allocated. Starting draw...");
    
    // 4. Perform drawing operations
    
    // "draw a blank framebuffer" (clear to dark blue)
    ColorRGBA blue = {.r = 0, .g = 0, .b = 50, .a = 255};
    clearFramebuffer(&fb, blue);
    
    // "draw a rectangle" (solid red)
    ColorRGBA red = {.r = 255, .g = 0, .b = 0, .a = 255};
    drawRectangleCFramebuffer(&fb, 50, 50, 100, 100, red, true);
    
    // Draw a semi-transparent green rectangle to test blending
    ColorRGBA green_alpha = {.r = 0, .g = 255, .b = 0, .a = 128}; // 50% alpha
    drawRectangleCFramebuffer(&fb, 100, 100, 150, 150, green_alpha, true);
    
    
    ESP_LOGI(TAG, "Drawing complete. Sending to LCD...");
    
    // 5. Send the completed buffer to the LCD
    // No conversion step is needed!
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle, 0, 0, width, height, display_buffer));
    
    ESP_LOGI(TAG, "Buffer sent. Halting in a loop.");
    
    // 6. Clean up (or loop)
    //while (1) {
    //    vTaskDelay(pdMS_TO_TICKS(1000));
    //}
}




/**
 * @brief This task handles all drawing and screen updates.
 *
 * It runs in an infinite loop, drawing to the framebuffer and
 * sending it to the display, then delaying to feed the WDT.
 */
/*void graphics_task(void *arg)
 {
 ESP_LOGI(TAG, "Graphics task started.");
 
 // --- NEW: INITIALIZE FREETYPE *HERE* ---
 esp_err_t ft_ok = initialize_freetype();
 if (ft_ok != ESP_OK) {
 ESP_LOGE(TAG, "Failed to initialize FreeType, deleting task.");
 vTaskDelete(NULL); // Abort this task
 return;
 }
 // --- END NEW PART ---
 
 ESP_LOGI(TAG, "Graphics task started.");
 
 // 2. Allocate the single framebuffer in PSRAM
 int width = DISPLAY_HORIZONTAL_PIXELS;
 int height = DISPLAY_VERTICAL_PIXELS;
 int num_pixels = width * height;
 
 uint8_t *display_buffer = (uint8_t *)heap_caps_malloc(num_pixels * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
 
 if (!display_buffer) {
 ESP_LOGE(TAG, "Failed to allocate display buffer in PSRAM. Deleting task.");
 vTaskDelete(NULL); // Delete this task
 return;
 }
 ESP_LOGI(TAG, "Framebuffer allocated.");
 
 // 3. Initialize the CPUGraphics Framebuffer struct
 Framebuffer fb = {
 .displayWidth = width,
 .displayHeight = height,
 .pixelData = display_buffer,    // Point to our BGR buffer
 .colorMode = COLOR_MODE_BGR888  // Set the direct-write mode
 };
 
 GraphicsCommand cmd;
 
 // This is now our main application loop
 while (1) {
 ESP_LOGI(TAG, "Starting draw...");
 
 // 4. Perform drawing operations
 
 // Clear to dark blue
 ColorRGBA blue = {.r = 0, .g = 0, .b = 50, .a = 255};
 clearFramebuffer(&fb, blue);
 
 // Draw solid red
 ColorRGBA red = {.r = 255, .g = 0, .b = 0, .a = 255};
 drawRectangleCFramebuffer(&fb, 50, 50, 100, 100, red, true);
 
 // Draw blended green
 ColorRGBA green_alpha = {.r = 0, .g = 255, .b = 0, .a = 128}; // 50% alpha
 drawRectangleCFramebuffer(&fb, 100, 100, 150, 150, green_alpha, true);
 
 // 4. --- DRAW TEXT (This code is the same) ---
 ColorRGBA white = {.r = 255, .g = 255, .b = 255, .a = 255};
 renderText(&fb,         // Your framebuffer
 ft_face,     // The font face we loaded
 "Hello, World!", // The text to draw
 50,          // X position
 300,         // Y position
 white,       // Text color
 24,          // Font size in pixels
 NULL);       // No gradient
 
 ESP_LOGI(TAG, "Drawing complete. Sending to LCD...");
 
 esp_err_t err = esp_lcd_panel_draw_bitmap(lcd_handle, 0, 0, width, height, display_buffer);
 if (err != ESP_OK) {
 ESP_LOGE(TAG, "Failed to draw bitmap! Error: %s", esp_err_to_name(err));
 }
 
 // 5. Send the completed buffer to the LCD
 // This is the long-running function
 //ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle, 0, 0, width, height, display_buffer));
 
 ESP_LOGI(TAG, "Buffer sent.");
 
 
 // Wait forever until a command arrives
 if (xQueueReceive(g_graphics_queue, &cmd, portMAX_DELAY) == pdTRUE) {
 
 // --- ADD THIS "RECEIVED" LOG ---
 ESP_LOGI(TAG, "Graphics task received command: %d", cmd.cmd);
 
 switch (cmd.cmd) {
 
 // --- Case 1: Draw a Rectangle ---
 case CMD_DRAW_RECT:
 ESP_LOGI(TAG, "Drawing rect to PSRAM");
 drawRectangleCFramebuffer(&fb, cmd.x, cmd.y, cmd.w, cmd.h, cmd.color, true);
 break;
 
 // --- Case 2: Draw Text ---
 case CMD_DRAW_TEXT:
 ESP_LOGI(TAG, "Drawing text to PSRAM");
 renderText(&fb, ft_face, cmd.text, cmd.x, cmd.y, white, 24, NULL);
 break;
 
 // --- Case 3: Update the LCD ---
 case CMD_UPDATE_AREA:
 {
 ESP_LOGI(TAG, "Pushing update to LCD");
 
 // 1. Create a small, *contiguous* buffer for the update
 uint8_t* temp_buffer = (uint8_t*) heap_caps_malloc(
 cmd.w * cmd.h * 3,
 MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
 if (!temp_buffer) {
 ESP_LOGE(TAG, "Failed to alloc temp_buffer!");
 continue; // Skip this command
 }
 
 // 2. Copy the updated rectangle from PSRAM to the temp_buffer
 uint8_t* psram_ptr;
 uint8_t* temp_ptr = temp_buffer;
 for (int i = 0; i < cmd.h; i++) {
 psram_ptr = &((uint8_t*)fb.pixelData)[((cmd.y + i) * fb.displayWidth + cmd.x) * 3];
 memcpy(temp_ptr, psram_ptr, cmd.w * 3);
 temp_ptr += cmd.w * 3;
 }
 
 // 3. Send *only the small temp_buffer* to the LCD
 ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle,
 cmd.x, cmd.y,           // x_start, y_start
 cmd.x + cmd.w, cmd.y + cmd.h, // x_end, y_end
 temp_buffer));
 
 // 4. Free the temp buffer
 heap_caps_free(temp_buffer);
 break;
 }
 }
 }
 
 
 // 6. *** THE FIX ***
 // Delay for 10 milliseconds.
 // This feeds the Task Watchdog and allows other tasks to run.
 vTaskDelay(pdMS_TO_TICKS(100));
 }
 }*/

static const char *TAG_PNG = "PNG_DECODER";

// --- Global variables to manage PNG decoding (File and Buffer) ---
// Note: We use static variables to hold state during the png_read_data callback
// Since this is a file I/O operation, it is generally okay, but be aware of thread safety
// if you were loading multiple images simultaneously (which requires per-texture state).
static FILE *png_file_handle = NULL;
static uint8_t *png_load_buffer = NULL; // Holds the PSRAM buffer start address
static int png_load_width = 0;
static int png_load_height = 0;


// =================================================================
// PNG DECODING CALLBACKS
// =================================================================

/**
 * @brief PNG file read callback function for libpng.
 * @param png_ptr The internal PNG structure pointer.
 * @param out_data Pointer to the destination buffer (libpng's internal buffer).
 * @param length The number of bytes to read.
 */
static void png_read_data(png_structp png_ptr, png_bytep out_data, png_size_t length) {
    if (png_file_handle) {
        if (fread(out_data, 1, length, png_file_handle) != length) {
            png_error(png_ptr, "Error reading file data");
        }
    } else {
        png_error(png_ptr, "File handle is NULL");
    }
}

/**
 * @brief Custom warning handler for libpng.
 */
static void png_warning_handler(png_structp png_ptr, png_const_charp warning_message) {
    ESP_LOGW(TAG_PNG, "PNG Warning: %s", warning_message);
}

/**
 * @brief Custom error handler for libpng.
 */
static void png_error_handler(png_structp png_ptr, png_const_charp error_message) {
    ESP_LOGE(TAG_PNG, "PNG Error: %s", error_message);
    // Setting longjmp is dangerous in ESP-IDF (may bypass stack unwinding)
    // We rely on the caller checking the texture data for NULL/size.
}

typedef enum {
    IMAGE_TYPE_UNKNOWN = 0,
    IMAGE_TYPE_PNG,
    IMAGE_TYPE_JPEG
} ImageFileType;

ImageFileType get_image_type_from_path(const char* path) {
    if (!path) return IMAGE_TYPE_UNKNOWN;
    
    // Find the last dot '.' in the string
    const char *dot = strrchr(path, '.');
    
    // If no dot, or dot is the first character, it's not a valid extension
    if (!dot || dot == path) return IMAGE_TYPE_UNKNOWN;
    
    // Compare case-insensitive (handles .PNG, .png, .Png)
    if (strcasecmp(dot, ".png") == 0) {
        return IMAGE_TYPE_PNG;
    }
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) {
        return IMAGE_TYPE_JPEG;
    }
    
    return IMAGE_TYPE_UNKNOWN;
}

// =================================================================
// MAIN LOADING FUNCTION
// =================================================================

ImageTexture* load_image_from_file(const char* imgPath) {
    
    // Reset global state for this decode operation
    png_file_handle = NULL;
    png_load_buffer = NULL;
    
    // 1. Open the file via VFS
    png_file_handle = fopen(imgPath, "rb");
    if (!png_file_handle) {
        ESP_LOGE(TAG_PNG, "Failed to open image file: %s", imgPath);
        return NULL;
    }
    
    // --- Check PNG Signature ---
    uint8_t header[8];
    if (fread(header, 1, 8, png_file_handle) != 8 || png_sig_cmp(header, 0, 8)) {
        ESP_LOGE(TAG_PNG, "File is not a valid PNG signature.");
        fclose(png_file_handle);
        return NULL;
    }
    
    // 2. Setup libpng structures
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_error_handler, png_warning_handler);
    if (!png_ptr) {
        ESP_LOGE(TAG_PNG, "Failed to create PNG read structure.");
        fclose(png_file_handle);
        return NULL;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        ESP_LOGE(TAG_PNG, "Failed to create PNG info structure.");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(png_file_handle);
        return NULL;
    }
    
    // 3. Set custom read function and read info
    png_set_read_fn(png_ptr, NULL, png_read_data);
    png_set_sig_bytes(png_ptr, 8); // Already read 8 bytes of header
    png_read_info(png_ptr, info_ptr);
    
    // 4. Get image properties
    png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);
    
    //ESP_LOGI(TAG_PNG, "Image found: %lu x %lu, Depth: %d, Type: %d", width, height, bit_depth, color_type);
    
    // 5. Transform image data for consistent output (32-bit RGBA)
    
    // Convert 16-bit to 8-bit (if needed)
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    // Convert palette to RGB
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    // Convert transparency (tRGB, tALPHA) to full alpha channel
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    // Convert grayscale to RGB (for consistency)
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    // Ensure we have an alpha channel (for transparency and blending)
    if (color_type != PNG_COLOR_TYPE_RGBA && color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
    
    png_set_bgr(png_ptr);
    // Update the image info after all transformations
    png_read_update_info(png_ptr, info_ptr);
    
    // Final check for 4 bytes per pixel (RGBA)
    size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    if (row_bytes != (width * 4)) {
        ESP_LOGE(TAG_PNG, "Final row size mismatch. Expected %lu, got %lu.", width * 4, row_bytes);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(png_file_handle);
        return NULL;
    }
    
    // 6. Allocate buffer in PSRAM (Critical step!)
    png_load_width = width;
    png_load_height = height;
    size_t data_size = width * height * 4; // 4 bytes/pixel (RGBA)
    
    png_load_buffer = (uint8_t*)heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM);
    if (!png_load_buffer) {
        ESP_LOGE(TAG_PNG, "Failed to allocate %u bytes in PSRAM for texture.", data_size);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(png_file_handle);
        return NULL;
    }
    
    // 7. Read image data row by row
    png_bytep* row_pointers = (png_bytep*)cc_safe_alloc(1, height * sizeof(png_bytep));
    if (!row_pointers) {
        ESP_LOGE(TAG_PNG, "Failed to allocate row pointers.");
        heap_caps_free(png_load_buffer);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(png_file_handle);
        return NULL;
    }
    
    // Set pointers to rows within the single PSRAM buffer
    for (int y_pos = 0; y_pos < height; y_pos++) {
        row_pointers[y_pos] = png_load_buffer + y_pos * row_bytes;
    }
    
    png_read_image(png_ptr, row_pointers);
    
    // 8. Cleanup and return ImageTexture
    ImageTexture* new_texture = (ImageTexture*)cc_safe_alloc(1, sizeof(ImageTexture));
    if (new_texture) {
        new_texture->width = width;
        new_texture->height = height;
        // Assign the PSRAM buffer to the ImageTexture struct
        new_texture->data = (ColorRGBA*)png_load_buffer;
    } else {
        heap_caps_free(png_load_buffer);
    }
    
    // Final libpng cleanup
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(png_file_handle);
    
    //ESP_LOGI(TAG_PNG, "PNG file loaded and decoded to PSRAM successfully.");
    return new_texture;
}

static const char *TAG_JPG = "JPG_DECODER";

ImageTexture* load_jpeg_from_file(const char* imgPath) {
    ESP_LOGI(TAG_JPG, "Loading JPEG: %s", imgPath);
    
    // 1. Open File
    FILE *f = fopen(imgPath, "rb");
    if (!f) {
        ESP_LOGE(TAG_JPG, "Failed to open file: %s", imgPath);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    ESP_LOGI(TAG_JPG, "File Size: %d bytes", file_size); // <--- DEBUG 1
    
    if (file_size == 0) {
        ESP_LOGE(TAG_JPG, "File size is 0");
        fclose(f);
        return NULL;
    }
    
    // Allocate Input Buffer in PSRAM
    uint8_t *jpeg_input_buf = (uint8_t *)heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!jpeg_input_buf) {
        ESP_LOGE(TAG_JPG, "Failed to alloc input buffer");
        fclose(f);
        return NULL;
    }
    
    size_t bytes_read = fread(jpeg_input_buf, 1, file_size, f);
    fclose(f);
    
    ESP_LOGI(TAG_JPG, "Bytes Read: %d", bytes_read); // <--- DEBUG 2
    
    // --- DEBUG 3: PRINT HEADER BYTES ---
    if (bytes_read > 4) {
        ESP_LOGI(TAG_JPG, "Header Bytes: %02X %02X %02X %02X",
                 jpeg_input_buf[0], jpeg_input_buf[1], jpeg_input_buf[2], jpeg_input_buf[3]);
    }
    
    // Valid JPEG should start with: FF D8 FF ...
    
    // 2. Configure Decoder
    esp_jpeg_image_cfg_t jpeg_cfg = {
        .indata = jpeg_input_buf,
        .indata_size = bytes_read, // Use actual bytes read
        .out_format = JPEG_IMAGE_FORMAT_RGB888,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = { .swap_color_bytes = 0 }
    };
    
    esp_jpeg_image_output_t info;
    
    // Get width/height without decoding the whole thing yet
    esp_err_t ret = esp_jpeg_get_image_info(&jpeg_cfg, &info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_JPG, "Failed to parse JPEG header");
        heap_caps_free(jpeg_input_buf);
        return NULL;
    }
    
    int w = info.width;
    int h = info.height;
    ESP_LOGI(TAG_JPG, "JPEG Info: %d x %d", w, h);
    
    // --- 3. Allocate Temporary RGB Output Buffer ---
    // The decoder needs a place to dump the 3-byte RGB data
    size_t rgb_buf_size = w * h * 3;
    uint8_t *temp_rgb_buf = (uint8_t *)heap_caps_malloc(rgb_buf_size, MALLOC_CAP_SPIRAM);
    
    if (!temp_rgb_buf) {
        ESP_LOGE(TAG_JPG, "Failed to alloc temp RGB buffer");
        heap_caps_free(jpeg_input_buf);
        return NULL;
    }
    
    // Setup config for full decode
    jpeg_cfg.outbuf = temp_rgb_buf;
    jpeg_cfg.outbuf_size = rgb_buf_size;
    
    // --- 4. Decode ---
    ret = esp_jpeg_decode(&jpeg_cfg, &info);
    
    // We are done with the compressed input file now
    heap_caps_free(jpeg_input_buf);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_JPG, "Failed to decode JPEG");
        heap_caps_free(temp_rgb_buf);
        return NULL;
    }
    
    // --- 5. Convert to ImageTexture (RGBA) ---
    // We need to expand 3-byte RGB to 4-byte RGBA and handle the R/B swap
    // for your display driver logic.
    
    ImageTexture* new_texture = (ImageTexture*)cc_safe_alloc(1, sizeof(ImageTexture));
    if (!new_texture) {
        heap_caps_free(temp_rgb_buf);
        return NULL;
    }
    
    new_texture->width = w;
    new_texture->height = h;
    
    size_t rgba_size = w * h * sizeof(ColorRGBA);
    new_texture->data = (ColorRGBA*)heap_caps_malloc(rgba_size, MALLOC_CAP_SPIRAM);
    
    if (!new_texture->data) {
        free(new_texture);
        heap_caps_free(temp_rgb_buf);
        return NULL;
    }
    
    uint8_t *src = temp_rgb_buf;
    ColorRGBA *dst = new_texture->data;
    int num_pixels = w * h;
    
    for (int i = 0; i < num_pixels; i++) {
        // Read RGB from decoder output
        uint8_t r = *src++;
        uint8_t g = *src++;
        uint8_t b = *src++;
        
        // Write RGBA to texture (Perform R/B swap for your display)
        // Your display expects BGR logic, so we store B in Red, R in Blue
        dst->r = b;
        dst->g = g;
        dst->b = r;
        dst->a = 255; // JPEGs are opaque
        
        dst++;
    }
    
    // Free the temporary 3-byte buffer
    heap_caps_free(temp_rgb_buf);
    
    ESP_LOGI(TAG_JPG, "JPEG Loaded and Converted.");
    return new_texture;
}

void updateArea(Framebuffer fb, GraphicsCommand cmd) {
    ESP_LOGI(TAG, "Pushing update to LCD (chunked)...");
    
    // Define a chunk size (e.g., 10 rows at a time)
    int rows_per_chunk = 10;
    
    // Calculate the size of our small, safe DMA buffer
    size_t chunk_size_bytes = cmd.w * rows_per_chunk * 3;
    
    // 1. Create the small, reusable DMA buffer
    uint8_t* temp_buffer = (uint8_t*) heap_caps_malloc(
                                                       chunk_size_bytes,
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!temp_buffer) {
        ESP_LOGE(TAG, "Failed to alloc temp_buffer for chunking!");
        return; // Skip this command
    }
    
    // 2. Loop over the update area in chunks
    for (int y_chunk = 0; y_chunk < cmd.h; y_chunk += rows_per_chunk) {
        
        // Calculate how many rows to send in this specific chunk
        int rows_to_send = cmd.h - y_chunk;
        if (rows_to_send > rows_per_chunk) {
            rows_to_send = rows_per_chunk;
        }
        
        // Calculate the start/end coordinates for this chunk
        int chunk_y_start = cmd.y + y_chunk;
        int chunk_y_end = chunk_y_start + rows_to_send;
        
        // 3. Copy this chunk from PSRAM to our internal temp_buffer
        uint8_t* psram_ptr;
        uint8_t* temp_ptr = temp_buffer;
        for (int i = 0; i < rows_to_send; i++) {
            psram_ptr = &((uint8_t*)fb.pixelData)[((chunk_y_start + i) * fb.displayWidth + cmd.x) * 3];
            memcpy(temp_ptr, psram_ptr, cmd.w * 3);
            temp_ptr += cmd.w * 3;
        }
        
        // 4. Send *only this small chunk* to the LCD
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle,
                                                  cmd.x, chunk_y_start,   // x_start, y_start
                                                  cmd.x + cmd.w, chunk_y_end, // x_end, y_end
                                                  temp_buffer));
    }
    
    // 5. Free the temp buffer
    heap_caps_free(temp_buffer);
    
    touchEnabled = true;
}

// Validated for: Framebuffer = BGR888 (3-byte), ILI9488 (3-byte SPI)

/*void updateArea(Framebuffer fb, GraphicsCommand cmd) {
 // 1. Configuration
 int bpp = 3; // BGR888 = 3 Bytes Per Pixel
 int rows_per_chunk = 20; // Safe chunk size (adjust if needed)
 
 // 2. Allocate the DMA Buffer ONCE
 // Size = Width * Lines * 3 bytes
 size_t chunk_size = cmd.w * rows_per_chunk * bpp;
 
 // Using INTERNAL | DMA memory is strictly required for ESP32 SPI DMA
 uint8_t* temp_buffer = (uint8_t*) heap_caps_malloc(chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
 
 if (!temp_buffer) {
 ESP_LOGE("LCD", "Failed to allocate DMA buffer for updateArea");
 return;
 }
 
 // 3. Process the Area in Chunks
 for (int y_chunk = 0; y_chunk < cmd.h; y_chunk += rows_per_chunk) {
 
 // Handle the final chunk (might be smaller than rows_per_chunk)
 int rows_to_send = (cmd.h - y_chunk > rows_per_chunk) ? rows_per_chunk : (cmd.h - y_chunk);
 
 // --- A. THE STRIDE COPY (Fixes the "Slice" glitch) ---
 // We pack the non-contiguous framebuffer rows into our contiguous temp_buffer
 for (int i = 0; i < rows_to_send; i++) {
 // Calculate Source Address: (Row * Width + Col) * 3
 // We use (cmd.y + y_chunk + i) to get the absolute row index
 int src_offset = ((cmd.y + y_chunk + i) * fb.displayWidth + cmd.x) * bpp;
 
 // Calculate Destination Address: i * Width * 3
 int dst_offset = (i * cmd.w) * bpp;
 
 memcpy(temp_buffer + dst_offset,
 (uint8_t*)fb.pixelData + src_offset,
 cmd.w * bpp);
 }
 
 // --- B. THE DRAW ---
 // Send the packed buffer to the display
 esp_err_t ret = esp_lcd_panel_draw_bitmap(lcd_handle,
 cmd.x,
 cmd.y + y_chunk,
 cmd.x + cmd.w,
 cmd.y + y_chunk + rows_to_send,
 temp_buffer);
 if (ret != ESP_OK) {
 ESP_LOGE("LCD", "Draw bitmap failed");
 break;
 }
 
 // --- C. THE RACE CONDITION FIX ---
 // We MUST wait for the SPI transaction to finish before we loop back
 // and overwrite 'temp_buffer' with new data.
 
 // Method 1: If you have a 'flush_ready' callback using a Semaphore, wait on it here.
 // Method 2: The "Brute Force" Polling (Simplest for now)
 // Note: Check your specific esp_lcd driver initialization.
 // If you didn't set a callback, verify if the driver blocks automatically.
 // Most ESP-IDF SPI drivers are non-blocking by default.
 
 // A simple delay is crude but proves the fix.
 // Ideally, you use xSemaphoreTake(transfer_done_sem, portMAX_DELAY);
 
 // For testing, this ensures the bus is clear (assuming 40MHz SPI, 20 lines takes <1ms)
 // Better: Application-level wait logic or polling.
 // If your driver has a "wait for done" function, use it.
 
 // *CRITICAL*: If you don't have semaphores set up, the buffer will corrupt.
 // If you are unsure, UNCOMMENT the line below to allocate a NEW buffer every time (slower, but safer)
 // OR add a delay:
 vTaskDelay(pdMS_TO_TICKS(1)); // Give DMA time to breathe
 }
 
 // 4. Cleanup
 heap_caps_free(temp_buffer);
 }
 */


// Example Master Drawing Function
void draw_current_view(Framebuffer *fb, FT_Face ft_face) {
    
    // --- 2. Draw the SCROLLABLE Text Box ---
    const char *scroll_text = "The quick brown fox jumps over the lazy dog. This is the content that needs to scroll smoothly behind the viewport frame.";
    ColorRGBA white = {.r = 255, .g = 255, .b = 255, .a = 255};
    TextFormat format = {.alignment = TEXT_ALIGN_LEFT, .wrapMode = TEXT_WRAP_MODE_WHOLE_WORD, .lineSpacing = 5, .glyphSpacing = 0};
    
    // CALL RENDER TEXT BOX
    renderTextBoxScroll(
                        fb,
                        ft_face,
                        scroll_text,
                        50, // x_start
                        100, // y_start (Top of the viewport)
                        220, // clipWidth
                        g_scroll_viewport_h, // clipHeight (e.g., 300)
                        g_scroll_offset_y, // <--- 🔑 SCROLL OFFSET APPLIED HERE
                        white,
                        16,
                        &format
                        );
    
    // ... (any other static foreground elements) ...
}


#include <math.h>
#include "esp_heap_caps.h" // Ensure we use capable memory for large vertex arrays

// ==========================================================
// HELPER: Generate Gear Vertices
// ==========================================================
/**
 * @brief Generates vertices for a gear-like shape.
 * This creates a star shape with many short teeth, resembling a gear.
 */
Vector3* create_gear_vertices(float centerX, float centerY, float outerRadius, float innerRadius, int numTeeth, int *outNumVertices) {
    int total_vertices = numTeeth * 2;
    *outNumVertices = total_vertices;
    
    // Allocate memory for the vertices.
    // Using heap_caps_malloc to ensure enough general RAM is available for larger shapes.
    Vector3* vertices = (Vector3*)heap_caps_malloc(total_vertices * sizeof(Vector3), MALLOC_CAP_DEFAULT);
    if (!vertices) {
        ESP_LOGE(TAG, "Failed to allocate gear vertices.");
        return NULL;
    }
    
    // The angle step is full circle divided by total points (inner + outer)
    float angle_step = (2.0f * M_PI) / total_vertices;
    
    for (int i = 0; i < total_vertices; i++) {
        // Alternate between outer and inner radii to create teeth
        float r = (i % 2 == 0) ? outerRadius : innerRadius;
        float angle = i * angle_step;
        
        // Calculate vertex position
        vertices[i].x = centerX + r * cosf(angle);
        vertices[i].y = centerY + r * sinf(angle);
        // Assumes Vector3 has a z component, set to 0 for 2D
#ifdef VECTOR3_HAS_Z
        vertices[i].z = 0.0f;
#endif
    }
    
    return vertices;
}

/**
 * @brief Creates a gear with a "spiked" or toothed hole in the center.
 * * @param holeOuterRadius The "tip" of the inner spikes.
 * @param holeInnerRadius The "valley" of the inner spikes.
 */
Vector3* create_spiked_center_gear_vertices(float centerX, float centerY, float outerRadius, float innerRadius, float holeOuterRadius, float holeInnerRadius, int numTeeth, int *outNumVertices) {
    int gear_points = numTeeth * 2;
    int total_vertices = gear_points * 2 + 2;
    
    *outNumVertices = total_vertices;
    
    Vector3* vertices = (Vector3*)heap_caps_malloc(total_vertices * sizeof(Vector3), MALLOC_CAP_DEFAULT);
    if (!vertices) return NULL;
    
    float angle_step = (2.0f * M_PI) / gear_points;
    
    // --- 1. Trace Outer Gear (Clockwise) ---
    int v_index = 0;
    for (int i = 0; i < gear_points; i++) {
        float r = (i % 2 == 0) ? outerRadius : innerRadius;
        float angle = i * angle_step;
        
        vertices[v_index].x = centerX + r * cosf(angle);
        vertices[v_index].y = centerY + r * sinf(angle);
        vertices[v_index].z = 0.0f;
        v_index++;
    }
    
    // --- 2. Cut to Inner Spikes ---
    vertices[v_index] = vertices[v_index - 1];
    v_index++;
    
    // --- 3. Trace Inner Spikes (Counter-Clockwise) ---
    for (int i = gear_points; i >= 0; i--) {
        // Logic for inner teeth:
        // We want the inner spike to align or offset with the outer spike.
        // Using the same modulo logic keeps them aligned.
        float r = (i % 2 == 0) ? holeOuterRadius : holeInnerRadius;
        float angle = i * angle_step;
        
        vertices[v_index].x = centerX + r * cosf(angle);
        vertices[v_index].y = centerY + r * sinf(angle);
        vertices[v_index].z = 0.0f;
        v_index++;
    }
    
    return vertices;
}



// ==========================================================
// MAIN EXAMPLE FUNCTION TO CALL IN GRAPHICS TASK
// ==========================================================
void draw_complex_gear_gradient_example(Framebuffer *fb) {
    ESP_LOGI(TAG, "Starting complex polygon draw...");
    
    // --- 1. Define the Complex Geometry (48-vertex Gear) ---
    int num_vertices = 0;
    // Center the gear at (160, 240)
    // Outer radius 130px, Inner radius 110px (short, stubby teeth)
    // 24 teeth = 48 total vertices
    Vector3* gear_vertices = create_gear_vertices(160.0f, 240.0f, 130.0f, 110.0f, 24, &num_vertices);
    
    if (!gear_vertices) {
        return; // Exit if allocation failed
    }
    ESP_LOGI(TAG, "Generated %d vertices for gear.", num_vertices);
    
    
    // --- 2. Define the 10-Stop "Thermal" Gradient ---
    // We define 10 colors creating a spectrum from cold (blue) to hot (white).
    
    ColorRGBA col0 = {.r = 0,   .g = 0,   .b = 128, .a = 255}; // Deep Blue (0.0)
    ColorRGBA col1 = {.r = 0,   .g = 0,   .b = 255, .a = 255}; // Blue (0.1)
    ColorRGBA col2 = {.r = 0,   .g = 128, .b = 255, .a = 255}; // Light Blue (0.2)
    ColorRGBA col3 = {.r = 0,   .g = 255, .b = 255, .a = 255}; // Cyan (0.3)
    ColorRGBA col4 = {.r = 0,   .g = 255, .b = 0,   .a = 255}; // Green (0.4)
    ColorRGBA col5 = {.r = 255, .g = 255, .b = 0,   .a = 255}; // Yellow (0.5)
    ColorRGBA col6 = {.r = 255, .g = 128, .b = 0,   .a = 255}; // Orange (0.6)
    ColorRGBA col7 = {.r = 255, .g = 0,   .b = 0,   .a = 255}; // Red (0.7)
    ColorRGBA col8 = {.r = 128, .g = 0,   .b = 128, .a = 255}; // Purple (0.8)
    ColorRGBA col9 = {.r = 255, .g = 255, .b = 255, .a = 255}; // White Hot (1.0)
    
    ColorStop stops[10] = {
        {.color = col0, .position = 0.0f},
        {.color = col1, .position = 0.1f},
        {.color = col2, .position = 0.2f},
        {.color = col3, .position = 0.3f},
        {.color = col4, .position = 0.4f},
        {.color = col5, .position = 0.5f},
        {.color = col6, .position = 0.6f},
        {.color = col7, .position = 0.7f},
        {.color = col8, .position = 0.8f},
        {.color = col9, .position = 1.0f}
    };
    
    Gradient complex_gradient = {
        .stops = stops,
        .numStops = 10,
        // Set a 30-degree angle for the thermal sweep across the gear
        .angle = M_PI / 6.0f
    };
    
    
    // --- 3. Draw the Polygon ---
    uint64_t start_time = esp_timer_get_time();
    
    fillPolygonWithGradient(
                            fb,
                            gear_vertices,
                            num_vertices,
                            &complex_gradient,
                            NULL,       // No Transform matrix needed for this test
                            false       // Anti-aliasing off (scanline fill is usually binary)
                            );
    
    uint64_t end_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Complex polygon drawn in %llu microseconds.", (end_time - start_time));
    
    
    // --- 4. Clean up memory ---
    // IMPORTANT: Free the vertex array allocated by the helper function
    free(gear_vertices);
}

void draw_star(Framebuffer *fb) {
    int num_vertices = 0;
    Vector3* star_vertices = create_star_vertices(
                                                  160.0f, // Center X (middle of 320 screen)
                                                  240.0f, // Center Y (middle of 480 screen)
                                                  100.0f, // Outer Radius
                                                  40.0f,  // Inner Radius
                                                  &num_vertices
                                                  );
    
    if (!star_vertices) {
        ESP_LOGE(TAG, "Failed to allocate star vertices.");
        return;
    }
    
    // --- 2. Define the Gradient ---
    
    // Gradient Stop 1: Bright Gold
    ColorRGBA gold = {.r = 255, .g = 215, .b = 0, .a = 255};
    // Gradient Stop 2: Deep Red-Orange
    ColorRGBA fire_red = {.r = 178, .g = 34, .b = 34, .a = 255};
    
    ColorStop stops[2] = {
        {.color = gold, .position = 0.0f},
        {.color = fire_red, .position = 1.0f}
    };
    
    Gradient star_gradient = {
        .stops = stops,
        .numStops = 2,
        .angle = M_PI_4 // 45 degrees diagonal gradient
    };
    
    // --- 3. Draw the Polygon ---
    // No transform or anti-aliasing for this example
    fillPolygonWithGradient(
                            fb,
                            star_vertices,
                            num_vertices,
                            &star_gradient,
                            NULL,       // No Transform
                            false       // No AntiAlias (since the scanline code is complex)
                            );
    
    // 4. Clean up memory
    free(star_vertices);
}

void setup_scroll_text_demo() {
    g_long_text_ptr = "This is the beginning of a very long string.\n"
    "It will demonstrate scrolling text rendering.\n\n"
    "We need to measure the total height of this text "
    "so we know how far we are allowed to scroll.\n"
    "The renderTextBoxScroll function handles the "
    "vertical offset calculation.\n\n"
    "Line 6\nLine 7\nLine 8\nLine 9\nLine 10\n"
    "End of text.";
    
    // Define formatting for measurement
    TextFormat fmt = { .alignment = TEXT_ALIGN_LEFT, .wrapMode = TEXT_WRAP_MODE_WHOLE_WORD, .lineSpacing = 5, .glyphSpacing = 0 };
    
    // Measure total height (using the helper function we wrote earlier)
    // Assuming viewport width is 200
    g_text_total_height = measureTextHeight(ft_face, g_long_text_ptr, 200, 16, &fmt);
    
    ESP_LOGI(TAG, "Total Text Height: %d px", g_text_total_height);
}

// Helper to allocate the buffer (Call this in app_main or graphics_task setup)
void init_anim_buffer() {
    g_anim_backup_buffer = (uint8_t*)heap_caps_malloc(ANIM_W * ANIM_H * 3, MALLOC_CAP_SPIRAM);
    if (!g_anim_backup_buffer) {
        ESP_LOGE(TAG, "Failed to allocate animation backup buffer!");
    } else {
        ESP_LOGI(TAG, "Animation backup buffer allocated (120KB).");
    }
}

void triangle_animation_task(void *pvParameter) {
    
    ESP_LOGI(TAG, "triangle_animation_task");
    
    
    
    // 1. Wait a moment for the main UI (Gradient/Rects) to finish drawing
    //vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 2. Send command to CAPTURE the background (Snapshot the gradient behind us)
    GraphicsCommand cmd_save = { .cmd = CMD_ANIM_SAVE_BG, .x =  ANIM_X, .y = ANIM_Y, .w = ANIM_W, .h = ANIM_H};
    xQueueSend(g_graphics_queue, &cmd_save, portMAX_DELAY);
    
    float phase = 0.0f;
    const float speed = 0.1f;
    int center_x = 160;
    int center_y = 240;
    
    // Background is now handled by Restore, so we don't need a clear color
    
    while (1) {
        
        // --- STEP 1: RESTORE BACKGROUND (Erase previous triangle) ---
        // This copies the saved pixels back to the framebuffer
        GraphicsCommand cmd_restore = { .cmd = CMD_ANIM_RESTORE_BG, .x =  ANIM_X, .y = ANIM_Y, .w = ANIM_W, .h = ANIM_H };
        xQueueSend(g_graphics_queue, &cmd_restore, 0);
        
        
        // --- STEP 2: CALCULATE AND DRAW NEW TRIANGLE ---
        float radius = 50.0f + 30.0f * sinf(phase);
        phase += speed;
        
        Vector3* vertices = (Vector3*)cc_safe_alloc(1, 3 * sizeof(Vector3));
        if (vertices) {
            vertices[0].x = center_x;
            vertices[0].y = center_y - radius;
            vertices[0].z = 0;
            vertices[1].x = center_x + (radius * 0.866f);
            vertices[1].y = center_y + (radius * 0.5f);
            vertices[1].z = 0;
            vertices[2].x = center_x - (radius * 0.866f);
            vertices[2].y = center_y + (radius * 0.5f);
            vertices[2].z = 0;
            
            Gradient* triGrad = (Gradient*)cc_safe_alloc(1, sizeof(Gradient));
            triGrad->type = GRADIENT_TYPE_LINEAR;
            triGrad->angle = M_PI_2;
            triGrad->numStops = 2;
            triGrad->stops = (ColorStop*)cc_safe_alloc(1, sizeof(ColorStop) * 2);
            triGrad->stops[0].color = (ColorRGBA){255, 165, 0, 255};
            triGrad->stops[0].position = 0.0f;
            triGrad->stops[1].color = (ColorRGBA){255, 0, 0, 255};
            triGrad->stops[1].position = 1.0f;
            
            GraphicsCommand cmd_poly = {
                .cmd = CMD_DRAW_POLYGON,
                .vertices = vertices,
                .numVertices = 3,
                .gradientData = triGrad
            };
            xQueueSend(g_graphics_queue, &cmd_poly, 0);
        }
        
        // --- STEP 3: UPDATE SCREEN ---
        // Push the dirty area (Background + Triangle) to the LCD
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = ANIM_X, .y = ANIM_Y,
            .w = ANIM_W, .h = ANIM_H
        };
        xQueueSend(g_graphics_queue, &cmd_flush, 0);
        
        vTaskDelay(pdMS_TO_TICKS(66));
    }
}

/**
 * @brief This task handles all drawing and screen updates.
 * It draws the initial UI *once*, then enters an event loop
 * to process partial updates from a queue.
 */

void graphics_task(void *arg)
{
    // ==========================================================
    // PART 1: INITIALIZATION (Runs ONCE)
    // ==========================================================
    ESP_LOGI(TAG, "Graphics task started.");
    
    // --- INITIALIZE FREETYPE ---
    esp_err_t ft_ok = initialize_freetype();
    if (ft_ok != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize FreeType, deleting task.");
        vTaskDelete(NULL); // Abort this task
        return;
    }
    
    init_freetype_cache_system();
    
    // --- Allocate the single framebuffer in PSRAM ---
    int width = DISPLAY_HORIZONTAL_PIXELS;
    int height = DISPLAY_VERTICAL_PIXELS;
    int num_pixels = width * height;
    
    uint8_t *display_buffer = (uint8_t *)heap_caps_malloc(num_pixels * 3, MALLOC_CAP_SPIRAM);
    
    if (!display_buffer) {
        ESP_LOGE(TAG, "Failed to allocate display buffer in PSRAM. Deleting task.");
        vTaskDelete(NULL); // Delete this task
        return;
    }
    ESP_LOGI(TAG, "Framebuffer allocated.");
    
    // --- Initialize the CPUGraphics Framebuffer struct ---
    fb = (Framebuffer){
        .displayWidth = width,
        .displayHeight = height,
        .pixelData = display_buffer,
        .colorMode = COLOR_MODE_BGR888
    };
    
    // ==========================================================
    // PART 2: DRAW INITIAL SCREEN (Runs ONCE)
    // ==========================================================
    ESP_LOGI(TAG, "Drawing initial UI...");
    
    // --- Define colors ---
    ColorRGBA blue = {.r = 0, .g = 0, .b = 50, .a = 255};
    ColorRGBA red = {.r = 255, .g = 0, .b = 0, .a = 255};
    ColorRGBA green_alpha = {.r = 0, .g = 255, .b = 0, .a = 128};
    ColorRGBA white = {.r = 255, .g = 255, .b = 255, .a = 255};
    
    // --- Draw initial UI to PSRAM buffer ---
    clearFramebuffer(&fb, blue);
    drawRectangleCFramebuffer(&fb, 50, 50, 100, 100, red, true);
    drawRectangleCFramebuffer(&fb, 100, 100, 150, 150, green_alpha, true);
    //renderText(&fb, ft_face, "Hello, World!", 50, 300, white, 24, NULL);
    
    // Start Color: A deep, rich blue
    ColorRGBA deep_blue = {.r = 0, .g = 50, .b = 150, .a = 255};
    // End Color: A bright, vibrant cyan/aqua
    ColorRGBA vibrant_aqua = {.r = 0, .g = 200, .b = 255, .a = 255};
    
    // Define the positions (0.0 to 1.0)
    ColorStop stops1[2] = {
        {.color = deep_blue, .position = 0.0f},
        {.color = vibrant_aqua, .position = 1.0f}
    };
    
    // --- 2. Define the Gradient Structure ---
    Gradient aqua_gradient = {
        .stops = stops1,
        .numStops = 2,
        .angle = M_PI_2 // 90 degrees (vertical gradient, from top to bottom)
        // You could also try 0.0 for horizontal, or M_PI_4 for diagonal
    };
    
    // --- 3. Define the Draw Parameters (Full Screen) ---
    int rect_x1 = 0;
    int rect_y1 = 0;
    int rect_w1 = 320;
    int rect_h1 = 480;
    
    // No transformation needed for a full-screen background
    Matrix3x3 identity_mat1 = IdentityMatrix();
    
    // --- 4. Call the Gradient Drawing Function ---
    fillRectangleWithGradient(
                              &fb,
                              rect_x1,
                              rect_y1,
                              rect_w1,
                              rect_h1,
                              &aqua_gradient,
                              &identity_mat1 // No transformation
                              );
    
    /*const float rotation_angle_deg = 45.0f;
     const float rotation_angle_rad = rotation_angle_deg * (M_PI / 180.0f);
     
     const int startX = 20;
     const int startY = 20;
     const int fontSize = 36;
     
     ColorRGBA yellow = {.r = 255, .g = 255, .b = 0, .a = 255};
     
     // --- 2. Build the Rotation Matrix (R) ---
     Matrix3x3 R = RotationMatrix(rotation_angle_rad);
     
     // --- 3. Build the Full Transform Matrix (T * R * -T) ---
     // The matrix order for pivot rotation is: T(x, y) * R(angle) * T(-x, -y)
     
     // First, translate the origin to the pivot point (startX, startY)
     Matrix3x3 T_pivot = TranslationMatrix((float)startX, (float)startY);
     
     // Second, apply the rotation (R)
     Matrix3x3 T_R = MultiplyMatrix3x3(&T_pivot, &R);
     
     // Third, translate the pivot point back to the origin
     Matrix3x3 T_neg = TranslationMatrix((float)-startX, (float)-startY);
     
     // Final Transform: (T_pivot * R) * T_neg
     Matrix3x3 final_transform = MultiplyMatrix3x3(&T_R, &T_neg);
     
     
     // --- 4. Render the Text ---
     
     // Draw a small square at the pivot point for reference
     drawRectangleCFramebuffer(&fb, startX - 3, startY - 3, 6, 6, yellow, true);
     
     renderTextWithTransform(
     &fb,
     ft_face,
     "Rotated by 45°",
     startX,
     startY,
     yellow,
     fontSize,
     &final_transform, // Pass the composite rotation matrix
     NULL
     );*/
    
    // 1. Load the PNG (and decode it into PSRAM)
    const char* imgPath = "/spiflash/test.png";
    ImageTexture* png_texture = load_image_from_file(imgPath);
    if (!png_texture) {
        return;
    }
    
    // 2. Define the target area for drawing
    int draw_x = 20;
    int draw_y = 80;
    int draw_w = png_texture->width * 2; // Example: draw it scaled up by 2x
    int draw_h = png_texture->height * 2;
    
    // 3. Draw the scaled texture onto the framebuffer
    // This uses your existing drawImageTexture function
    //drawImageTexture(&fb, png_texture, draw_x, draw_y, png_texture->width, png_texture->height);
    
    // 4. Free the temporary PSRAM texture buffer
    heap_caps_free(png_texture->data);
    free(png_texture);
    
    ColorRGBA orange = {.r = 255, .g = 165, .b = 0, .a = 255}; // Opaque Orange
    ColorRGBA aqua_alpha = {.r = 0, .g = 0, .b = 255, .a = 180}; // Semi-transparent Aqua
    ColorRGBA dark_blue = {.r = 0, .g = 0, .b = 50, .a = 255}; // Background blue
    
    /*drawRoundedRectangle_AntiAliasing(
     &fb,
     40,      // X Start
     100,     // Y Start
     200,     // Width (200px)
     150,     // Height (150px)
     orange,  // Color: Opaque Orange
     30,      // Radius (30px corner radius)
     true     // Fill: True (solid rectangle)
     );
     
     // 3. Draw the second Anti-aliased Rounded Rectangle (Semi-transparent Aqua)
     // This will overlap the first, and the alpha blending will occur on the edges
     // and the overlapping area, showing a smooth transition.
     drawRoundedRectangle_AntiAliasing(
     &fb,
     160,     // X Start (overlaps the first rect)
     180,     // Y Start (overlaps the first rect)
     100,     // Width (200px)
     150,     // Height (150px)
     aqua_alpha, // Color: Semi-transparent Aqua (A=180)
     50,         // Radius (50px corner radius)
     true        // Fill: True (solid rectangle)
     );*/
    
    ColorRGBA border_alpha = {.r = 0, .g = 0, .b = 0, .a = 200};
    drawRoundedRectangle_AntiAliasing(
                                      &fb,
                                      17,      // X Start
                                      17,      // Y Start
                                      286,     // Width
                                      56,      // Height
                                      border_alpha,   // Color: White
                                      28,      // Radius
                                      false    // Fill: False (Draws only the border)
                                      );
    drawRoundedRectangle_AntiAliasing(
                                      &fb,
                                      20,      // X Start
                                      20,      // Y Start
                                      280,     // Width
                                      50,      // Height
                                      white,   // Color: White
                                      25,      // Radius
                                      false    // Fill: False (Draws only the border)
                                      );
    
    // --- Define Colors ---
    ColorRGBA white_alpha = {.r = 255, .g = 255, .b = 255, .a = 150}; // Semi-transparent White
    ColorRGBA yellow_opaque = {.r = 255, .g = 255, .b = 0, .a = 255}; // Opaque Yellow
    ColorRGBA magenta_opaque = {.r = 255, .g = 0, .b = 255, .a = 255}; // Opaque Magenta
    ColorRGBA cyan_alpha = {.r = 0, .g = 255, .b = 255, .a = 100}; // Highly transparent Cyan
    
    // Get screen dimensions for cleaner coordinates
    int w = 320;
    int h = 480;
    
    // --- 1. Draw a thin, opaque diagonal line (for precision test) ---
    /*drawLineWithThickness(&fb,
     10, 10,       // Start near top-left
     w - 10, h - 10, // End near bottom-right
     magenta_opaque,
     1); // Thickness: 1 pixel
     
     // --- 2. Draw a thick, opaque horizontal line ---
     drawLineWithThickness(&fb,
     0, 400,     // Start at left edge
     w, 400,     // End at right edge
     yellow_opaque,
     10); // Thickness: 10 pixels
     
     // --- 3. Draw a semi-transparent line over the yellow one (blending test) ---
     // This line should appear orange where it crosses the yellow line.
     drawLineWithThickness(&fb,
     w / 2, 350,   // Start near center
     w / 2, 450,   // End in the yellow line area
     white_alpha,
     15); // Thickness: 15 pixels
     
     // --- 4. Draw a highly transparent overlay (Alpha Blending Test) ---
     // This line will barely change the color of the background/lines it crosses.
     drawLineWithThickness(&fb,
     50, h / 2,
     w - 50, h / 2,
     cyan_alpha,
     5); // Thickness: 5 pixels
     */
    
    // Gradient Stop 1: Starts as opaque dark pink/magenta
    ColorRGBA stop1_color = {.r = 200, .g = 0, .b = 150, .a = 255};
    // Gradient Stop 2: Transitions to opaque yellow/green
    ColorRGBA stop2_color = {.r = 100, .g = 255, .b = 0, .a = 255};
    
    // Define the positions (0.0 to 1.0)
    ColorStop stops[2] = {
        {.color = stop1_color, .position = 0.0f},
        {.color = stop2_color, .position = 1.0f}
    };
    
    // --- 2. Define the Gradient Structure ---
    Gradient my_gradient = {
        .stops = stops,
        .numStops = 2,
        .angle = M_PI_4 // 45 degrees (M_PI_4 is a common constant for PI/4)
    };
    
    // --- 3. Define the Draw Parameters ---
    
    int rect_x = 50;
    int rect_y = 350;
    int rect_w = 250;
    int rect_h = 100;
    int corner_radius = 20;
    
    // Optional: Define a 2D rotation matrix (45 degrees clockwise rotation)
    Matrix3x3 rotation_mat = RotationMatrix(-M_PI_4);
    
    // Note: If you don't want the rectangle rotated, use an Identity Matrix:
    Matrix3x3 identity_mat = IdentityMatrix();
    
    // --- 4. Call the Gradient Drawing Function ---
    
    // This draws a large, rotated, anti-aliased, rounded rectangle
    /*fillRoundedRectangleWithGradient(
     &fb,
     rect_x,
     rect_y,
     rect_w,
     rect_h,
     &my_gradient,
     corner_radius,
     &identity_mat, // Passing the rotation matrix
     true           // AntiAlias: True
     );*/
    
    
    // --- Send the *entire* buffer to the LCD *once* ---
    /*esp_err_t err = esp_lcd_panel_draw_bitmap(lcd_handle, 0, 0, width, height, display_buffer);
     if (err != ESP_OK) {
     ESP_LOGE(TAG, "Failed to draw initial bitmap! Error: %s", esp_err_to_name(err));
     }
     ESP_LOGI(TAG, "Initial UI drawn. GRAPHICS TASK READY FOR COMMANDS.");*/
    
    
    
    GraphicsCommand cmd_update;
    cmd_update.cmd = CMD_UPDATE_AREA;
    cmd_update.x = 0;
    cmd_update.y = 0; // A rough bounding box
    cmd_update.w = 320;
    cmd_update.h = 480; // A bit larger to be safe
    updateArea(fb, cmd_update);
    
    
    
    
    // ==========================================================
    // PART 3: EVENT LOOP (Runs FOREVER)
    // ==========================================================
    GraphicsCommand cmd;
    while (1) {
        // Wait forever until a command arrives
        if (xQueueReceive(g_graphics_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            
            //ESP_LOGI(TAG, "Graphics task received command: %d", cmd.cmd);
            
            switch (cmd.cmd) {
                    
                    // --- Case 1: Draw a Rectangle ---
                case CMD_DRAW_RECT:
                    ESP_LOGI(TAG, "Drawing rect to PSRAM");
                    drawRectangleCFramebuffer(&fb, cmd.x, cmd.y, cmd.w, cmd.h, cmd.color, true);
                    break;
                    
                    // --- Case 2: Draw Text ---
                case CMD_DRAW_TEXT:
                    ESP_LOGI(TAG, "Drawing text to PSRAM");
                    renderText(&fb, ft_face, cmd.text, cmd.x, cmd.y, cmd.color, 12, NULL);
                    break;
                    
                    // --- Case 3: Update the LCD (Chunked) ---
                case CMD_DRAW_TEXT_BOX:
                    // --- Define a large block of text ---
                    /*const char *long_text = "The quick brown fox jumps over the lazy dog. "
                     "This text needs to demonstrate word-wrapping and line-breaking "
                     "within a confined area for our new scroll view. \n\n" // Explicit newline
                     "This final sentence should appear on a completely new line.";
                     
                     ColorRGBA text_color = {.r = 255, .g = 255, .b = 255, .a = 255}; // White
                     
                     // Define the Clipping Area (The Scroll View Window)
                     int clip_x = cmd.x;
                     int clip_y = cmd.y;
                     int clip_w = cmd.w; // Constrain the width to 200 pixels
                     int clip_h = cmd.h; // Constrain the height to 300 pixels
                     
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
                     
                     // --- Render the Multi-Line Text ---
                     renderTextBox(
                     &fb,
                     ft_face,
                     long_text,
                     clip_x,
                     clip_y,
                     clip_w,
                     clip_h,
                     cmd.color,
                     16, // Font Size
                     &text_format
                     );*/
                    ESP_LOGI(TAG, "Drawing Text Box: %s", cmd.text);
                    /*renderTextBoxExtended(
                     &fb, ft_face, cmd.text,
                     cmd.x, cmd.y, cmd.w, cmd.h, // Logical Layout Box
                     // Pass the Clip Rects from the command (which come from View Hierarchy)
                     cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                     0, // ScrollY (Use this param if you have a specific scroll command)
                     cmd.color, cmd.fontSize, &cmd.textFormat
                     );*/
                    renderTextBox(
                                  &fb,
                                  ft_face,
                                  cmd.text,
                                  cmd.x, cmd.y,       // Start X, Y
                                  cmd.w, cmd.h,       // Clip Width, Clip Height
                                  cmd.color,
                                  cmd.fontSize,
                                  &cmd.textFormat     // Pass the format struct
                                  );
                    break;
                case CMD_DRAW_TEXT_BOX_CACHED: {
                    const char* myFont = "/spiflash/proximaNovaRegular.ttf";
                    renderTextBoxExtendedCached(
                                  &fb,
                                  g_ftc_manager,     // The Global Manager
                                  g_ftc_image_cache, // The Global Image Cache
                                  g_ftc_cmap_cache,  // The Global CMap Cache
                                  (FTC_FaceID)myFont, // Pass path as ID
                                  cmd.text,
                                  cmd.x, cmd.y,       // Start X, Y
                                  cmd.w, cmd.h,       // Clip Width, Clip Height
                                  cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                                  0,
                                  cmd.color,
                                  cmd.fontSize,
                                  &cmd.textFormat     // Pass the format struct
                                  );
                    break;
                }
                case CMD_UPDATE_AREA:
                {
                    updateArea(fb, cmd);
                    break;
                }
                case CMD_CURSOR_SETUP:
                {    // 1. Save the background under the new cursor location
                    //    The x/y are passed in the generic cmd struct
                    ESP_LOGI(TAG, "calling function save_cursor_background");
                    save_cursor_background(&fb, cmd.x, cmd.y);
                    
                    // 2. Draw the cursor immediately (first blink ON)
                    drawRectangleCFramebuffer(&fb, cmd.x, cmd.y, CURSOR_W, CURSOR_H, white, true);
                    
                    // 3. Send the small update area to show the cursor
                    GraphicsCommand cmd_update = {.cmd = CMD_UPDATE_AREA, .x = cmd.x, .y = cmd.y, .w = CURSOR_W, .h = CURSOR_H};
                    //xQueueSend(g_graphics_queue, &cmd_update, 0);
                    updateArea(fb, cmd_update);
                    
                    xTaskCreatePinnedToCore(
                                            cursor_blink_task,  // New task function
                                            "cursor_blink",
                                            2048,
                                            NULL,
                                            4,
                                            &g_cursor_blink_handle,  // Capture the handle here
                                            0
                                            );
                    // 4. --- NEW: SEND THE NOTIFICATION TO START THE BLINKER ---
                    if (g_cursor_blink_handle != NULL) {
                        xTaskNotifyGive(g_cursor_blink_handle);
                    }
                    break;
                }
                case CMD_CURSOR_DRAW:
                {    // Draw the cursor (uses its current global x/y location)
                    drawRectangleCFramebuffer(&fb, g_cursor_x, g_cursor_y, CURSOR_W, CURSOR_H, white, true);
                    
                    // Send the update area
                    GraphicsCommand cmd_draw = {.cmd = CMD_UPDATE_AREA, .x = g_cursor_x, .y = g_cursor_y, .w = CURSOR_W, .h = CURSOR_H};
                    //xQueueSend(g_graphics_queue, &cmd_draw, 0);
                    updateArea(fb, cmd_draw);
                    break;
                }
                case CMD_CURSOR_RESTORE:
                {    // Restore the background (which also sends the update to the LCD)
                    restore_cursor_background(&fb);
                    break;
                }
                    // Inside graphics_task's while(1) loop, switch (cmd.cmd):
                case CMD_SCROLL_CONTENT:
                {
                    int delta_y = cmd.y; // The scroll delta sent from updateTouch
                    int max_scroll = g_scroll_total_height - g_scroll_viewport_h;
                    
                    // Ensure max_scroll is not negative (i.e., content fits entirely in viewport)
                    if (max_scroll < 0) max_scroll = 0;
                    
                    // 1. Update the offset
                    g_scroll_offset_y += delta_y;
                    
                    // 2. Clamp the offset (ensure it stays within boundaries)
                    if (g_scroll_offset_y < 0) {
                        g_scroll_offset_y = 0; // Cannot scroll past the top edge
                    } else if (g_scroll_offset_y > max_scroll) {
                        g_scroll_offset_y = max_scroll; // Cannot scroll past the bottom edge
                    }
                    
                    // 3. Trigger a full redraw of the viewport
                    // This uses the existing CMD_LOAD_PAGE_1 logic to redraw the whole scene *once*.
                    GraphicsCommand cmd_redraw_viewport = {.cmd = CMD_LOAD_PAGE_1};
                    xQueueSend(g_graphics_queue, &cmd_redraw_viewport, 0);
                    
                    ESP_LOGI(TAG, "Scroll updated to: %d/%d", g_scroll_offset_y, max_scroll);
                    break;
                }
                    // Inside graphics_task switch (cmd.cmd):
                case CMD_LOAD_PAGE_1:{ // Triggered by CMD_SCROLL_CONTENT
                    ESP_LOGI(TAG, "Redrawing entire view due to scroll event.");
                    
                    // 1. Redraw the entire scene, applying the new g_scroll_offset_y
                    draw_current_view(&fb, ft_face);
                    
                    // 2. Send the full buffer update to the LCD
                    // (This is triggered by the scroll event, so we push the full screen.)
                    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle,
                                                              0, 0,
                                                              fb.displayWidth, fb.displayHeight,
                                                              fb.pixelData));
                    break;}
                case CMD_DRAW_STAR: {
                    draw_star(&fb);
                    
                    draw_complex_gear_gradient_example(&fb);
                    
                    
                    
                    
                }
                    // Inside graphics_task switch ...
                    
                    // Inside graphics_task while loop... switch(cmd.cmd) ...
                    
                case CMD_DRAW_ROUNDED_RECT:
                {
                    // This function handles the AA and the color mode internally
                    /*drawRoundedRectangle_AntiAliasingExtended(
                                                              &fb,
                                                              cmd.x, cmd.y, cmd.w, cmd.h,
                                                              cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                                                              cmd.color,
                                                              cmd.radius,
                                                              cmd.fill
                                                              );
                    
                    int64_t t1 = esp_timer_get_time();
                    
                    drawRoundedRectangle_AntiAliasingExtended(
                                                              &fb,
                                                              cmd.x, cmd.y, cmd.w, cmd.h,
                                                              cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                                                              cmd.color,
                                                              cmd.radius,
                                                              cmd.fill
                                                              );
                    
                    int64_t t2 = esp_timer_get_time();
                    
                    drawRoundedRectangle_AntiAliasingOptimized(
                                                              &fb,
                                                              cmd.x, cmd.y, cmd.w, cmd.h,
                                                              cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                                                              cmd.color,
                                                              cmd.radius,
                                                              cmd.fill
                                                              );
                    
                    int64_t t3 = esp_timer_get_time();*/
                    
                    drawRoundedRectangle_AntiAliasingOptimized(
                                                              &fb,
                                                              cmd.x, cmd.y, cmd.w, cmd.h,
                                                              cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                                                              cmd.color,
                                                              cmd.radius,
                                                              cmd.fill
                                                              );
                    
                    //int64_t t4 = esp_timer_get_time();

                        //ESP_LOGI(TAG, "SIMD (Fixed Pt) Time:  %lld us", (t4 - t3));
                        //ESP_LOGI(TAG, "Speedup Factor:        %.2fx", (float)(t2 - t1) / (float)(t4 - t3));
                    
                    break;
                    
                }
                    
                case CMD_DRAW_GRADIENT_ROUNDED_RECT:
                {
                    if (cmd.gradientData) {
                        fillRoundedRectangleWithGradientExtended(
                                                                 &fb,
                                                                 cmd.x, cmd.y, cmd.w, cmd.h,
                                                                 cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                                                                 cmd.gradientData,
                                                                 cmd.radius,
                                                                 NULL, // No transform for standard views
                                                                 true  // Anti-alias
                                                                 );
                        
                        printf("shadowRadius: %d", (int)cmd.radius);
                        
                        // CRITICAL: Clean up the memory allocated by the main task
                        if (cmd.gradientData->stops) free(cmd.gradientData->stops);
                        free(cmd.gradientData);
                    }
                    break;
                    
                }
                case CMD_DRAW_GRADIENT_RECT:
                {
                    if (cmd.gradientData) {
                        // Use the optimized function for non-rounded rectangles
                        printf("CMD_DRAW_GRADIENT_RECT: %d %d %d %d %d %d %d %d", cmd.x, cmd.y, cmd.w, cmd.h, cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH);
                        Matrix3x3 identity_mat1 = IdentityMatrix();
                        /*fillRectangleWithGradientExtended(
                                                          &fb,
                                                          cmd.x, cmd.y, cmd.w, cmd.h,
                                                          cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                                                          cmd.gradientData,
                                                          &identity_mat1 // No transform for standard views
                                                          );*/
                        
                        //fillRectangleWithGradientSIMD(&fb, cmd.x, cmd.y, cmd.w, cmd.h, cmd.gradientData);
                        
                        fillRectangleWithGradientOptimized(
                                                          &fb,
                                                          cmd.x, cmd.y, cmd.w, cmd.h,
                                                          cmd.clipX, cmd.clipY, cmd.clipW, cmd.clipH,
                                                          cmd.gradientData
                                                          );
                        
                        // Clean up memory
                        if (cmd.gradientData->stops) free(cmd.gradientData->stops);
                        free(cmd.gradientData);
                    }
                    break;
                }
                    // --- NEW: DRAW IMAGE FROM FILE ---
                case CMD_DRAW_IMAGE_FILE:
                {
                    ESP_LOGD(TAG, "Drawing Image: %s", cmd.imagePath);
                    
                    // Inside your Load function
                    ImageTexture* tex = NULL;
                    ImageFileType type = get_image_type_from_path(cmd.imagePath);
                    
                    if (type == IMAGE_TYPE_PNG) {
                        tex = load_image_from_file(cmd.imagePath); // LibPNG
                    } else if (type == IMAGE_TYPE_JPEG) {
                        tex = load_jpeg_from_file(cmd.imagePath);  // ESP-JPEG
                    }
                    
                    
                    if (tex) {
                        // 2. Draw (Scales to the view's width/height)
                        // Note: Your CCView frame is the destination size.
                        if (cmd.hasTransform) {
                            // A. Draw with Affine Transform (Rotation, Scale, Skew)
                            // The x, y, w, h here usually define the base rect before transformation
                            drawImageTextureWithTransform(
                                &fb,
                                tex,
                                cmd.x, cmd.y,
                                cmd.w, cmd.h,
                                &cmd.transform
                            );
                        } else {
                            // B. Standard Draw (Fast Axis-Aligned Blit)
                            drawImageTexture(
                                &fb,
                                tex,
                                cmd.x, cmd.y,
                                cmd.w, cmd.h
                            );
                        }
                        if (tex->data) heap_caps_free(tex->data);
                        free(tex);
                    }
                    break;
                }
                case CMD_DRAW_POLYGON: {
                    if (cmd.vertices && cmd.gradientData) {
                        
                        // Use your existing scanline filler
                        fillPolygonWithGradient(
                                                &fb,
                                                cmd.vertices,
                                                cmd.numVertices,
                                                cmd.gradientData,
                                                NULL,  // No extra transform needed (handled in Bridge)
                                                false  // Anti-aliasing (set to true if you implement AA in polygon fill)
                                                );
                        
                        // --- CRITICAL CLEANUP ---
                        // 1. Free the vertices array allocated in the Bridge
                        free(cmd.vertices);
                        
                        // 2. Free the gradient structure allocated in the Bridge
                        if (cmd.gradientData->stops) free(cmd.gradientData->stops);
                        free(cmd.gradientData);
                    }
                    break;
                }
                /*case CMD_DRAW_PIXEL_BUFFER: {
                    // User's custom struct handling
                    if (cmd.pixelBuffer) {
                        ESP_LOGI("GFX", "Received Pixel Buffer Command");
                        // cmd.pixelBuffer is (ColorRGBA*), fb->pixelData is (ColorRGBA*)
                        // Simple Memcpy because formats match
                        size_t bufferSize = cmd.w * cmd.h * sizeof(ColorRGBA);
                        memcpy(fb.pixelData, cmd.pixelBuffer, bufferSize);
                    }
                    break;
                }*/
                case CMD_DRAW_PIXEL_BUFFER: {
                    if (cmd.pixelBuffer) {
                        ColorRGBA *srcPixels = (ColorRGBA *)cmd.pixelBuffer;
                        
                        if (fb.colorMode == COLOR_MODE_BGR888) {
                            uint8_t *dst = (uint8_t *)fb.pixelData;
                            
                            for (int y = 0; y < cmd.h; y++) {
                                if ((cmd.y + y) >= fb.displayHeight) break;
                                
                                for (int x = 0; x < cmd.w; x++) {
                                    if ((cmd.x + x) >= fb.displayWidth) break;
                                    
                                    ColorRGBA p = srcPixels[y * cmd.w + x];
                                    int dstIdx = ((cmd.y + y) * fb.displayWidth + (cmd.x + x)) * 3;
                                    
                                    // --- VIVID FILTER (Contrast Stretch) ---
                                    // We assume video is 16-235 range. We stretch it to 0-255.
                                    // Formula: (Color - 16) * 1.16
                                    // Using Fast Integer Math: (Color - 16) * 296 / 256
                                    
                                    int r = ((int)p.r - 16) * 296 >> 8;
                                    int g = ((int)p.g - 16) * 296 >> 8;
                                    int b = ((int)p.b - 16) * 296 >> 8;
                                    
                                    // Clamp values to 0-255 (prevent overflow glitches)
                                    if (r < 0) r = 0; else if (r > 255) r = 255;
                                    if (g < 0) g = 0; else if (g > 255) g = 255;
                                    if (b < 0) b = 0; else if (b > 255) b = 255;
                                    
                                    // Write BGR (The order that made the cat Orange)
                                    dst[dstIdx + 0] = (uint8_t)b; // Blue
                                    dst[dstIdx + 1] = (uint8_t)g; // Green
                                    dst[dstIdx + 2] = (uint8_t)r; // Red
                                }
                            }
                        }
                    }
                    break;
                }
                case CMD_ANIM_SAVE_BG:
                {
                    // Capture the current screen state (clean background)
                    if (g_anim_backup_buffer) {
                        anim_save_background(&fb, g_anim_backup_buffer, cmd.x, cmd.y, cmd.w, cmd.h);
                        ESP_LOGI(TAG, "Animation background saved.");
                    }
                    break;
                }
                case CMD_ANIM_RESTORE_BG:
                {
                    // Erase whatever was drawn by putting the clean background back
                    if (g_anim_backup_buffer) {
                        anim_restore_background(&fb, g_anim_backup_buffer, cmd.x, cmd.y, cmd.w, cmd.h);
                    }
                    break;
                }
                case CMD_UI_SAVE_TO_A: {
                    if (g_ui_backup_buffer_A) {
                        anim_save_background(&fb, g_ui_backup_buffer_A, cmd.x, cmd.y, cmd.w, cmd.h);
                    }
                    break;
                }
                case CMD_UI_RESTORE_FROM_A: {
                    if (g_ui_backup_buffer_A) {
                        anim_restore_background(&fb, g_ui_backup_buffer_A, cmd.x, cmd.y, cmd.w, cmd.h);
                    }
                    break;
                }
                case CMD_UI_SAVE_TO_B: {
                    if (g_ui_backup_buffer_B) {
                        anim_save_background(&fb, g_ui_backup_buffer_B, cmd.x, cmd.y, cmd.w, cmd.h);
                    }
                    break;
                }
                case CMD_UI_COPY_B_TO_A: {
                    if (g_ui_backup_buffer_A && g_ui_backup_buffer_B) {
                        // We just promoted the new icon to be the "Active" one.
                        // Copy the data we just saved in B over to A so we can restore it later.
                        memcpy(g_ui_backup_buffer_A, g_ui_backup_buffer_B, BUFFER_SIZE);
                    }
                    break;
                }
                case CMD_DRAW_ROUNDED_HAND: {
                    // x,y = Start Point
                    // w,h = End Point (We reused these fields)
                    // radius = Thickness
                    drawRoundedHand(&fb, cmd.x, cmd.y, cmd.w, cmd.h, cmd.radius, cmd.color);
                    break;
                }
                case CMD_DRAW_DAY_NIGHT_OVERLAY: {
                    drawDayNightOverlay(&fb, cmd.x, cmd.y, cmd.w, cmd.h, cmd.radius, cmd.fontSize);
                    break;
                }
            }
        }
        // There is NO vTaskDelay here.
        // xQueueReceive handles all the waiting.
    }
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
        i2s_channel_write(tx_handle, samples, buffer_len * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
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
        ESP_LOGE(TAG, "CRITICAL: Could not init I2C on Pins 1 & 2. Error: %s", esp_err_to_name(err));
        return;
    }

    // 2. Run the Scan
    int devices_found = 0;
    for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
        // Probe every address
        if (i2c_master_probe(bus_handle, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "  -> SUCCESS: Found Device at 0x%02X", addr);
            devices_found++;
        }
    }

    // 3. Report Results
    if (devices_found == 0) {
        ESP_LOGE(TAG, "FAILURE: No devices found. Pins 1 & 2 are dead or wired incorrectly.");
    } else {
        ESP_LOGI(TAG, "PASSED: Found %d device(s) on Pins 1 & 2.", devices_found);
    }

    // 4. Clean up (Release pins so your real code can use them)
    i2c_del_master_bus(bus_handle);
    ESP_LOGW(TAG, "--- DIAGNOSTIC COMPLETE ---");
}

// Add these defines if not already in your file
#define ES8311_ADDR 0x18

void initEs8311(void) {
    ESP_LOGI("AUDIO", "Initializing ES8311...");

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

    ESP_LOGI("AUDIO", "ES8311 Init Complete. Gain set to MAX.");
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
            ESP_LOGI("MIC_DEBUG", "Left Level: %ld  |  Right Level: %ld", (long)max_left, (long)max_right);
        } else {
            ESP_LOGE("MIC_DEBUG", "I2S Read Failed!");
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

    ESP_LOGI(TAG, "Starting PWM Fade Loop...");

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
        ESP_LOGI(TAG, "ADC Calibration Initialized");
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
            
            ESP_LOGI(TAGBattery, "Battery: %.2f V (%d mV)", battery_voltage, voltage_mv_pin);
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
            
            ESP_LOGI(TAG, "Battery (Avg): %.2f V | %d%%", battery_voltage, pct);
        }
        
        // Inside Task loop
        g_battery_level = battery_voltage;

        // Inside any other task
        printf("Current Level: %.2f", g_battery_level);
        
        // 3. Convert to Percentage
        uint8_t pct = get_battery_percentage(battery_voltage);
            
        ESP_LOGI(TAG, "Battery: %.2f V | %d%%", battery_voltage, pct);

        // Delay for 5 seconds (5000ms)
        // This frees up the CPU for other tasks
        vTaskDelay(pdMS_TO_TICKS(100));

        
    }
    
    // Tasks should never return. If they do, they must delete themselves.
    vTaskDelete(NULL);
}

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
            ESP_LOGE(TAG, "Failed to open file for writing (Is WL mounted? Check partition size)");
        } else {
            fprintf(f, "This file was created by the ESP32 firmware!");
            fclose(f);
            ESP_LOGI(TAG, "Successfully wrote a test file.");
            
            // List again to show the new file
            list_directory_contents(MOUNT_POINT);
        }
    }
    
    
    //load_and_execute_program("/spiflash/test.bin");
    
#if defined(USE_ILI9488)
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
    xTaskCreate(updateTouch, "updateTouch", 8192, NULL, 5, NULL);
    
    g_graphics_queue = xQueueCreate(40, sizeof(GraphicsCommand));
    if (g_graphics_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create graphics queue!");
        return; // Halt if queue creation fails
    }
    ESP_LOGI(TAG, "Graphics queue created.");
    
    // 2. Create the graphics task
    // This creates the task, gives it a 4KB stack, pins it to Core 1
    // (so Core 0 can be used for Wi-Fi/Bluetooth if needed)
   
    
    ///////////////
    xTaskCreatePinnedToCore(
                            graphics_task,      // Function to implement the task
                            "graphics_task",    // Name of the task (for debugging)
                            8192,               // Stack size in words (4096 * 4 bytes)
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
    ESP_LOGI(TAG, "Main complete, task created.");
    
    xTaskCreate(
            battery_monitor_task,   // Function to call
            "Battery_Monitor",      // Name for debugging
            4096,                   // Stack size (bytes) - 4KB is safe for printf/float
            NULL,                   // Parameter to pass (not used here)
            5,                      // Priority (1 is low, 24 is high)
            NULL                    // Task Handle (not needed unless you want to delete it later)
        );
    
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    // Start the debug task
    //xTaskCreatePinnedToCore(debug_microphone_task, "mic_debug", 4096, NULL, 5, NULL, 1);

    //testCPUGraphicsBenchmark();
    
    //printf("Starting Playback...\n");
    //xTaskCreatePinnedToCore(mp3_task_wrapper, "mp3_task", 32768, NULL, 5, NULL, 0);
    //play_mp3_file("/sdcard/frair.mp3", tx_handle);
    
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    /*ESP_LOGI(TAG, "Creating LVGL handler task");
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
     ESP_LOGI(TAG, "CMD_ANIM_SAVE_BG");
     
     // 5. Send the Save Command
     // This tells graphics_task: "Call anim_save_background(&fb, ...) now!"
     GraphicsCommand cmd_save = {
     .cmd = CMD_ANIM_SAVE_BG,
     .x =  ANIM_X, .y = ANIM_Y, .w = ANIM_W, .h = ANIM_H
     };
     xQueueSend(g_graphics_queue, &cmd_save, portMAX_DELAY);
     ESP_LOGI(TAG, "Sent command to snapshot background.");
     
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

