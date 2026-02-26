//
//  FreeOS.c
//  
//
//  Created by Chris Galzerano on 2/7/26.
//

#include "FreeOS.h"

#include "FilesApp.h"
#include "ClockApp.h"

// --- Global Variable Definitions ---
FT_Library ft_library;
FT_Face    ft_face;
uint8_t* font_buffer = NULL;
Framebuffer fb;
MyQueueHandle_t g_graphics_queue;

bool setupui = false;
bool touchEnabled = true;

// --- Global Root View ---
CCView* mainWindowView = NULL;
CCScrollView* g_active_scrollview = NULL;
int g_touch_last_y = 0;
int lastOffY = 0;
CCTextView* myDemoTextView = NULL;

// --- Global Scroll State ---
int g_text_scroll_y = 0;
int g_text_total_height = 0;
int g_text_view_h = 300;
const char* g_long_text_ptr = NULL;
bool notFirstTouch = false;

// --- FreeType Cache Globals ---
FTC_Manager     g_ftc_manager = NULL;
FTC_ImageCache  g_ftc_image_cache = NULL;
FTC_CMapCache   g_ftc_cmap_cache = NULL;

// --- View Stack ---
CCView* viewStack[MAX_VIEW_STACK];
int viewStackPointer = 0;

CurrentView currentView = CurrentViewHome;

// --- App/System State ---
bool openedApp = false;
CCArray* files = NULL;
CCArray* settings = NULL;

// --- Keyboard Globals ---
CCView* uiKeyboardView = NULL;
CCLabel* uiTargetLabel = NULL;
CCString* uiInputBuffer = NULL;

// --- Animation & Backup Buffers ---
uint8_t* g_anim_backup_buffer = NULL;
uint8_t* g_ui_backup_buffer_A = NULL;
uint8_t* g_ui_backup_buffer_B = NULL;

CCArray* g_grid_items_registry = NULL;
CCView* g_last_touched_icon = NULL;

// --- Global Cursor State ---
uint8_t* g_cursor_backup_buffer = NULL;
bool addedCursor = false;
int g_cursor_x = 0;
int g_cursor_y = 0;
bool g_cursor_visible = false;
TaskHandle_t g_cursor_blink_handle = NULL;

// --- Scrolling UI State ---
int g_scroll_offset_y = 0;
int g_scroll_total_height = 0;
int g_scroll_viewport_h = 300;
int g_drag_start_y = 0;
float g_drag_velocity = 0.0f;

// --- Keyboard Internal State ---
bool hasDrawnKeyboard = false;
int keyboardCursorPosition = 0;

// --- UI Highlighting ---
CCView* g_pressed_icon_view = NULL;
ColorRGBA g_color_highlight = {.r=0, .g=0, .b=0, .a=25};
ColorRGBA g_color_transparent = {.r=0, .g=0, .b=0, .a=0};

KeyboardMode kbCurrentMode = KB_MODE_ABC_LOWER;

// --- Task Handles ---
TaskHandle_t g_triangle_task_handle = NULL;
TaskHandle_t g_image_task_handle = NULL;

// --- Image Decoders ---
const char *TAG_PNG = "PNG_DECODER";
FILE *png_file_handle = NULL;
uint8_t *png_load_buffer = NULL;
int png_load_width = 0;
int png_load_height = 0;
const char *TAG_JPG = "JPG_DECODER";



// 1. Write custom allocator functions forcing PSRAM
static void* ft_alloc(FT_Memory memory, long size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); // FORCE PSRAM!
}

static void ft_free(FT_Memory memory, void* block) {
    heap_caps_free(block);
}

static void* ft_realloc(FT_Memory memory, long cur_size, long new_size, void* block) {
    return heap_caps_realloc(block, new_size, MALLOC_CAP_SPIRAM); // FORCE PSRAM!
}

//
// =================== UPDATED: FreeType Initialization ===================
//
esp_err_t initialize_freetype()
{
    FreeOSLogI(TAG, "Initializing FreeType with PSRAM...");

        // 1. Allocate the custom memory manager struct in PSRAM
        FT_Memory memory = (FT_Memory)heap_caps_malloc(sizeof(*memory), MALLOC_CAP_SPIRAM);
        if (!memory) {
            FreeOSLogE(TAG, "Failed to allocate FreeType memory struct!");
            return ESP_FAIL;
        }
        
        memory->alloc   = ft_alloc;
        memory->free    = ft_free;
        memory->realloc = ft_realloc;
        memory->user    = NULL;

        // 2. Initialize FreeType using our custom memory manager
        // We declare the 'error' variable here to fix the "undeclared" compiler error!
        FT_Error error = FT_New_Library(memory, &ft_library);
        if (error) {
            FreeOSLogE(TAG, "Failed to create FreeType library with custom memory!");
            return ESP_FAIL;
        }
        
        // 3. Load standard FreeType modules (renderers, format parsers)
        FT_Add_Default_Modules(ft_library);
    
    // --- THIS IS THE NEW PART ---
    // Read the font file from the mounted /storage partition
    const char* font_path = "/spiflash/proximaNovaRegular.ttf";
    FreeOSLogI(TAG, "Loading font from %s", font_path);
    
    FILE* file = fopen(font_path, "rb");
    if (!file) {
        FreeOSLogE(TAG, "Failed to open font file. Make sure it's in your 'storage' folder.");
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
        FreeOSLogE(TAG, "Failed to allocate memory for font buffer (%ld bytes)", font_size);
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
        FreeOSLogE(TAG, "Failed to load font face! FreeType error: 0x%X", error);
        // Free the buffer if we failed
        free(font_buffer);
        font_buffer = NULL;
        return ESP_FAIL;
    }
    
    FreeOSLogI(TAG, "FreeType initialized and font loaded.");
    return ESP_OK;
}


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
            freeFilesView();
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
        FreeOSLogE(TAG, "Failed to open directory: %s", mount_point);
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



void init_ui_buffers() {
    g_ui_backup_buffer_A = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    g_ui_backup_buffer_B = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    
    if (!g_ui_backup_buffer_A || !g_ui_backup_buffer_B) {
        FreeOSLogE(TAG, "Failed to allocate UI backup buffers!");
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
    QueueSend(g_graphics_queue, &cmd_bg, QUEUE_MAX_DELAY);

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
    
    //FreeOSLogI(TAG, "drawViewHierarchy5 CMD_DRAW_TEXT_BOX valignment %d", fmt.valignment);
    
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
    QueueSend(g_graphics_queue, &cmd_text, QUEUE_MAX_DELAY);
    
    // 4. Force Flush of ONLY this area (Optimization)
    // If you have a specific flush command, use it.
    // Otherwise, the Graphics Task usually flushes after processing.
    
    GraphicsCommand cmd_flush = {
        .cmd = CMD_UPDATE_AREA,
        .x = 0, .y = 0, .w = 320, .h = 480
    };
    QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
    
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
    QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
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
    QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
}

void update_full_ui(void) {
    if (!mainWindowView) return;
    touchEnabled = false;
    
    FreeOSLogI(TAG, "Starting UI Update...");
    
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
    // Use QUEUE_MAX_DELAY to ensure we don't drop the clear command
    QueueSend(g_graphics_queue, &cmd_clear, QUEUE_MAX_DELAY);
    
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
    QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
    
    FreeOSLogI(TAG, "UI Update Commands Sent.");
}

void update_full_ui1(void) {
    if (!mainWindowView) return;
    touchEnabled = false;
    
    FreeOSLogI(TAG, "Starting UI Update...");
    
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
    // Use QUEUE_MAX_DELAY to ensure we don't drop the clear command
    QueueSend(g_graphics_queue, &cmd_clear, QUEUE_MAX_DELAY);
    
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
    QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
    
    FreeOSLogI(TAG, "UI Update Commands Sent.");
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
        FreeOSLogE(TAG, "Failed to allocate vertices for shape layer");
        return;
    }
    
    // 4. Convert Coordinates with SAFETY CHECK
    for (int i = 0; i < count; i++) {
        CCPoint* pt = (CCPoint*)arrayObjectAtIndex(points, i);
        
        // --- SAFETY CHECK START ---
        if (pt == NULL) {
            FreeOSLogE(TAG, "Point at index %d is NULL! Aborting shape draw.", i);
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
        if (QueueSend(g_graphics_queue, &cmd_poly, QUEUE_MAX_DELAY) != pdTRUE) {
            FreeOSLogE(TAG, "Failed to send polygon command");
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

// Safely reads 1 byte from Flash memory (I-ROM/D-ROM), bypassing hardware alignment exceptions
static inline char safe_flash_read_byte(const char *ptr) {
    // 1. Calculate the nearest 32-bit aligned memory address
    const uint32_t *aligned_ptr = (const uint32_t *)((intptr_t)ptr & ~3);
    
    // 2. Perform a safe 32-bit hardware read
    uint32_t word = *aligned_ptr;
    
    // 3. Find out which of the 4 bytes we actually wanted
    int byte_offset = ((intptr_t)ptr & 3);
    
    // 4. Shift and mask out the target byte (ESP32 is Little Endian)
    return (char)((word >> (byte_offset * 8)) & 0xFF);
}

// Converts a global screen touch (e.g., 480x320) into the local coordinate system of a specific view
CCPoint viewConvertPoint(CCView* targetView, int globalX, int globalY) {
    CCPoint absOrigin = getAbsoluteOrigin(targetView);
    CCPoint localPoint;
    localPoint.x = globalX - absOrigin.x;
    localPoint.y = globalY - absOrigin.y;
    return localPoint;
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
    
    //FreeOSLogI(TAG, "CMD_DRAW_GRADIENT_ROUNDED_RECT %d %d %d %d", (int)currentClip.origin->x, (int)currentClip.origin->y, (int)currentClip.size->width, (int)currentClip.size->height);
    
    // 1. Identify Type
    // We safely cast to CCType* first because 'type' is the first member of ALL your structs.
    CCType type = *((CCType*)object);
    
    FreeOSLogI("drawViewHierarchy", "CCType_FramebufferView %d", (type == CCType_FramebufferView)?1:0);
    
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
    else if (type == CCType_FramebufferView) {
        baseView = ((CCFramebufferView*)object)->view;
    }
    else {
        return; // Unknown type
    }
    
    // Safety check
    if (!baseView) return;
    
    //FreeOSLogI(TAG, "drawViewHierarchy");
    
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
        
        //FreeOSLogI(TAG, "drawViewHierarchy1");
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
                
                //FreeOSLogI(TAG, "CMD_DRAW_GRADIENT_ROUNDED_RECT %d %d %d %d %d %d %d %d ", (int)myEffectiveClip.origin->x, (int)myEffectiveClip.origin->y, (int)myEffectiveClip.size->width, (int)myEffectiveClip.size->height, absX, absY, w, h);
                
                QueueSend(g_graphics_queue, &cmd_shadow, QUEUE_MAX_DELAY);
            } else { free(shadowGrad); }
        }
    }
    
    //FreeOSLogI(TAG, "drawViewHierarchy2");
    
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
        QueueSend(g_graphics_queue, &cmd_border, QUEUE_MAX_DELAY);
    }
    
    //FreeOSLogI(TAG, "drawViewHierarchy3");
    
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
            QueueSend(g_graphics_queue, &cmd_grad, QUEUE_MAX_DELAY);
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
            QueueSend(g_graphics_queue, &cmd_grad, QUEUE_MAX_DELAY);
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
            QueueSend(g_graphics_queue, &cmd_bg, QUEUE_MAX_DELAY);
        }
    }
    
    //FreeOSLogI(TAG, "drawViewHierarchy4");
    
    
    // ============================================================
    // USE 'object' and 'type' FOR CONTENT
    // ============================================================
    
    // --- STEP D: HANDLE CONTENT ---
    
    // 1. Handle Shape Layer (Attached to baseView)
    // FIX: Changed 'view->shapeLayer' to 'baseView->shapeLayer'
    if (baseView->shapeLayer != NULL) {
        drawShapeLayer(baseView->shapeLayer, absX, absY);
    }
    
    //FreeOSLogI(TAG, "drawViewHierarchy5");
    
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
        
        //FreeOSLogI(TAG, "drawViewHierarchy5 CMD_DRAW_TEXT_BOX valignment %d", fmt.valignment);
        
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
                     size_t len = strlen(label->text->string); // strlen in ROM handles alignment automatically
                     cmd_text.text = malloc(len + 1);
                     if (cmd_text.text) {
                         for (size_t i = 0; i <= len; i++) {
                             // Extract byte-by-byte using the hardware-safe alignment math
                             cmd_text.text[i] = safe_flash_read_byte(&label->text->string[i]);
                         }
                     }
                 } else {
                     cmd_text.text = NULL;
                 }
        QueueSend(g_graphics_queue, &cmd_text, QUEUE_MAX_DELAY);
        //FreeOSLogI(TAG, "drawViewHierarchy5 CMD_DRAW_TEXT_BOX %d", myEffectiveClip.size->width);
    }
    
    
    
    // 3. Handle Image Content
    // FIX: Changed 'view->type' to 'type'
    // Inside drawViewHierarchy...

    // Inside drawViewHierarchy...
    
    // Inside your drawViewHierarchy (or subview iteration loop):
        
    else if (type == CCType_FramebufferView) {
        FreeOSLogI("drawViewHierarchy", "CCType_FramebufferView CMD_DRAW_FRAMEBUFFER");
        // Cast the generic object to our specific wrapper
        CCFramebufferView* fbView = (CCFramebufferView*)object;
        
        // 1. Draw the background view normally (handles background color, corner radius, borders)
        //if (fbView->view) {
        //    drawViewHierarchy(fbView->view);
            // Note: If drawViewHierarchy recursively calls this block, ensure you
            // have a check for CCType_View so it doesn't infinite loop!
        //}
        
        // 2. Queue the Framebuffer content to draw on top of the background
        if (fbView->framebuffer) {
            
            GraphicsCommand cmd;
            cmd.cmd = CMD_DRAW_FRAMEBUFFER;
            cmd.x = absX;
            cmd.y = absY;
            cmd.pixelBuffer = (void*)fbView->framebuffer;
            
            FreeOSLogI("drawViewHierarchy", "CMD_DRAW_FRAMEBUFFER");
            
            xQueueSend(g_graphics_queue, &cmd, portMAX_DELAY);
        }
        
        return; // Done rendering this specific component
    }

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
                .alpha = imgView->alpha,
                .x = absX, .y = absY, .w = w, .h = h,
                .clipX = (int)myEffectiveClip.origin->x,
                .clipY = (int)myEffectiveClip.origin->y,
                .clipW = (int)myEffectiveClip.size->width,
                .clipH = (int)myEffectiveClip.size->height
            };
            
            // Now it is 100% safe to access the string
            strncpy(cmd_img.imagePath, imgView->image->filePath->string, 63);
            
            QueueSend(g_graphics_queue, &cmd_img, QUEUE_MAX_DELAY);
        }
        
        if (imgView->image->imageData) {
            GraphicsCommand cmd = {
                .cmd = CMD_DRAW_PIXEL_BUFFER, // Use the new buffer command
                //.alpha = imgView->alpha,
                .x = absX,
                .y = absY,
                .w = imgView->image->size->width,  // Use image size
                .h = imgView->image->size->height,
                .pixelBuffer = imgView->image->imageData // Pass the raw pointer
            };
            QueueSend(g_graphics_queue, &cmd, 0);
        }
    }
    
    //FreeOSLogI(TAG, "drawViewHierarchy6");
    
    
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
    FreeOSLogI(TAG, "setup_ui_demo");
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
    FreeOSLogI("PROFILE", "Code execution took: %lld us (%lld ms)", time_diff, time_diff / 1000);
    
    // --- Trigger Render ---
    // Assuming you have a function to start the render pass:
    // update_full_ui();
    //update_full_ui();
    update_full_ui1();
}

esp_err_t setup_cursor_buffers() {
    size_t size = CURSOR_W * CURSOR_H * CURSOR_BPP;
    // Allocate buffer in internal, DMA-capable memory (very fast)
    g_cursor_backup_buffer = (uint8_t*) heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!g_cursor_backup_buffer) {
        FreeOSLogE(TAG, "Failed to allocate %d bytes for cursor backup!", size);
        return ESP_ERR_NO_MEM;
    }
    FreeOSLogI(TAG, "setup_cursor_buffers");
    return ESP_OK;
}

void save_cursor_background(Framebuffer *fb, int x, int y) {
    if (!g_cursor_backup_buffer) {
        FreeOSLogI(TAG, "!g_cursor_backup_buffer");
        return;
    }
    
    // Set the new cursor position
    g_cursor_x = x;
    g_cursor_y = y;
    
    uint8_t* psram_ptr;
    uint8_t* backup_ptr = g_cursor_backup_buffer;
    
    FreeOSLogI(TAG, "save_cursor_background");
    
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
    
    FreeOSLogI(TAG, "restore_cursor_background");
    
    // 2. Send the restore command to the graphics queue
    GraphicsCommand cmd_update;
    cmd_update.cmd = CMD_UPDATE_AREA;
    cmd_update.x = g_cursor_x;
    cmd_update.y = g_cursor_y;
    cmd_update.w = CURSOR_W;
    cmd_update.h = CURSOR_H;
    
    // Use the queue send for the update
    if (QueueSend(g_graphics_queue, &cmd_update, 0) != pdTRUE) {
        FreeOSLogE(TAG, "Cursor restore command failed.");
    }
}

void cursor_blink_task(void *pvParameter)
{
    ulTaskNotifyTake(pdTRUE, QUEUE_MAX_DELAY);
    
    //FreeOSLogI(TAG, "cursor_blink_task");
    
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
        QueueSend(g_graphics_queue, &cmd_blink, 0);
        
        // Wait for the blink interval (500ms)
        vTaskDelay(pdMS_TO_TICKS(500));
        
        
    }
}

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

CCView* find_grid_item_at(int x, int y) {
    // Safety check
    FreeOSLogI(TAG, "find_grid_item_at");
    if (!g_grid_items_registry) return NULL;
    FreeOSLogI(TAG, "find_grid_item_at %d", g_grid_items_registry->count);
    
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

void hide_keyboard_ui(void) {
    printf("--- Teardown Keyboard UI Start ---\n");

    // 1. Destroy the visual keyboard view
    if (uiKeyboardView) {
        viewRemoveFromSuperview(uiKeyboardView);
        freeViewHierarchy(uiKeyboardView);
        uiKeyboardView = NULL;
        printf("Keyboard View Freed.\n");
    }

    // 2. Clear the input string buffer
    if (uiInputBuffer) {
        freeCCString(uiInputBuffer);
        uiInputBuffer = NULL;
    }

    // 3. Clear the target pointer (we don't free the label, just our link to it!)
    uiTargetLabel = NULL;

    printf("--- Teardown Keyboard UI Done ---\n");
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




void hideTriangleAnimation(void) {
    if (g_triangle_task_handle != NULL) {
        FreeOSLogI(TAG, "Stopping Triangle Animation...");
        
        // 1. Delete the task
        vTaskDelete(g_triangle_task_handle);
        g_triangle_task_handle = NULL;
        
        // 2. Restore the background one last time to clean up the screen
        GraphicsCommand cmd_restore = {
            .cmd = CMD_ANIM_RESTORE_BG,
            .x = ANIM_X, .y = ANIM_Y,
            .w = ANIM_W, .h = ANIM_H
        };
        QueueSend(g_graphics_queue, &cmd_restore, QUEUE_MAX_DELAY);
        
        // 3. Flush the clean screen
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = ANIM_X, .y = ANIM_Y,
            .w = ANIM_W, .h = ANIM_H
        };
        QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
        
        // 4. Free the background buffer
        // free_anim_buffer(); // Implement this if your init_anim_buffer uses malloc
    }
}

void showRotatingImageAnimation(void) {
    // 1. Initialize buffer for background save/restore
    init_anim_buffer(); // Ensure this handles the size defined in IMG_ANIM_W/H
    
    // 2. Wait for previous UI to settle
    vTaskDelay(pdMS_TO_TICKS(500));
    FreeOSLogI(TAG, "CMD_ANIM_SAVE_BG for Image");
    
    // 3. Command: Snapshot the background behind where the image will spin
    GraphicsCommand cmd_save = {
        .cmd = CMD_ANIM_SAVE_BG,
        .x = IMG_ANIM_X-30, .y = IMG_ANIM_Y-30,
        .w = IMG_ANIM_W+60, .h = IMG_ANIM_H+60
    };
    QueueSend(g_graphics_queue, &cmd_save, QUEUE_MAX_DELAY);
    
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
        FreeOSLogI(TAG, "Stopping Rotating Image Animation...");
        
        // 1. Delete the task
        vTaskDelete(g_image_task_handle);
        g_image_task_handle = NULL;
        
        // 2. Restore background to wipe the spinner
        GraphicsCommand cmd_restore = {
            .cmd = CMD_ANIM_RESTORE_BG,
            .x = IMG_ANIM_X, .y = IMG_ANIM_Y,
            .w = IMG_ANIM_W, .h = IMG_ANIM_H
        };
        QueueSend(g_graphics_queue, &cmd_restore, QUEUE_MAX_DELAY);
        
        // 3. Flush
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = IMG_ANIM_X, .y = IMG_ANIM_Y,
            .w = IMG_ANIM_W, .h = IMG_ANIM_H
        };
        QueueSend(g_graphics_queue, &cmd_flush, QUEUE_MAX_DELAY);
        
        // 4. Free buffer
        // free_anim_buffer();
    }
}

void rotating_image_task(void *pvParameter) {
    FreeOSLogI(TAG, "rotating_image_task started");
    
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
        QueueSend(g_graphics_queue, &cmd_restore, 0);

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
            .alpha = 1.0,
            .transform = mat_final
        };
        
        // Set the image path safely
        strncpy(cmd_draw.imagePath, "/spiflash/loading.png", 63);
        
        QueueSend(g_graphics_queue, &cmd_draw, 0);

        // --- STEP 4: FLUSH ---
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = IMG_ANIM_X-30, .y = IMG_ANIM_Y-30,
            .w = IMG_ANIM_W+60, .h = IMG_ANIM_H+60
        };
        QueueSend(g_graphics_queue, &cmd_flush, 0);

        // Update angle
        angle += speed;
        if (angle > M_PI * 2) angle -= M_PI * 2;

        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
    }
}

void showTriangleAnimation(void) {
    init_anim_buffer();
    //vTaskDelay(pdMS_TO_TICKS(1000));
    FreeOSLogI(TAG, "CMD_ANIM_SAVE_BG");
    
    
    
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



// Helper: Finds which subview is under the user's finger
// Returns: The specific child view (key, row, button), or NULL if nothing found.
/*CCView* find_subview_at_point(CCView* container, int globalX, int globalY) {
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
}*/

// Helper: Finds which subview is under the user's finger
// Returns: The specific child view (key, row, button), or NULL if nothing found.
CCView* find_subview_at_point(CCView* container, int globalX, int globalY) {
    // 1. If the container itself ignores touches, stop immediately
    if (!container || !container->subviews || container->ignoreTouch) return NULL;

    // 2. Calculate the Container's Global Position
    // We need this because the children's frames are relative to the Container,
    // but your touch is relative to the Screen (0,0).
    CCPoint containerOrigin = getAbsoluteOrigin(container);
    int containerAbsX = (int)containerOrigin.x;
    int containerAbsY = (int)containerOrigin.y;

    // 3. Convert Touch to "Container-Local" Coordinates
    int localTouchX = globalX - containerAbsX;
    int localTouchY = globalY - containerAbsY;

    // 4. Check Intersections (Iterate Backwards: Top-most view first)
    for (int i = container->subviews->count - 1; i >= 0; i--) {
        CCView* child = (CCView*)arrayObjectAtIndex(container->subviews, i);
        
        // --- THE IGNORE TOUCH FIX ---
        // Skip this child entirely if it is set to ignore touches!
        if (!child || child->ignoreTouch) continue;
        
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

// Helper: Recursively finds the deepest subview under the user's finger
// Returns: The deepest specific child view, or NULL if nothing found.
CCView* find_subview_at_point_recursive(CCView* container, int globalX, int globalY) {
    // 1. Base safety check
    if (!container || container->ignoreTouch) return NULL;

    // 2. Only attempt to search if this container actually has children
    if (container->subviews) {
        
        // Calculate the Container's Global Position once
        CCPoint containerOrigin = getAbsoluteOrigin(container);
        int containerAbsX = (int)containerOrigin.x;
        int containerAbsY = (int)containerOrigin.y;

        // Convert Touch to "Container-Local" Coordinates
        int localTouchX = globalX - containerAbsX;
        int localTouchY = globalY - containerAbsY;

        // Check Intersections (Iterate Backwards: Top-most view first)
        for (int i = container->subviews->count - 1; i >= 0; i--) {
            CCView* child = (CCView*)arrayObjectAtIndex(container->subviews, i);
            
            // Skip this child entirely if it ignores touches
            if (!child || child->ignoreTouch) continue;
            
            // Simple Rectangle Intersection Check using pointers
            if (localTouchX >= child->frame->origin->x &&
                localTouchX <= child->frame->origin->x + child->frame->size->width &&
                localTouchY >= child->frame->origin->y &&
                localTouchY <= child->frame->origin->y + child->frame->size->height) {
                
                // --- THE RECURSIVE MAGIC ---
                // We know the touch landed inside 'child'.
                // Let's ask 'child' if the touch landed on anything inside of IT!
                CCView* deepHit = find_subview_at_point_recursive(child, globalX, globalY);
                
                // If it hit a nested button, return that deepest button
                if (deepHit) {
                    return deepHit;
                }
                
                // Otherwise, it just hit the child wrapper itself (like your toolbar button!)
                return child;
            }
        }
    }

    return NULL; // Touched empty space/background
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
    
    FreeOSLogI(TAG, "--- Files in %s partition: ---", mount_point);
    
    // 1. Open the directory stream
    dir = opendir(mount_point);
    if (!dir) {
        FreeOSLogE(TAG, "Failed to open directory: %s", mount_point);
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
    FreeOSLogI(TAG, "--- Listing Complete ---");
}



/*void testCPUGraphicsBenchmark(void) {
    // 1. Allocate Framebuffer in PSRAM (Slow access) or SRAM (Fast access)
    // For benchmarking math, SRAM is better, but for full frame, you likely need PSRAM.
    int width = 320;
    int height = 240;
    size_t fbSize = width * height * 3;
    
    uint8_t *display_buffer = (uint8_t *)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (!display_buffer) {
        FreeOSLogE(TAG, "Failed to allocate memory");
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
    
    FreeOSLogI(TAG, "Original (Float) Time: %lld us", (t2 - t1));

    // --- BENCHMARK 2: SIMD (Fixed Point) ---
    // Warm up cache
    fillRectangleWithGradientSIMD(&fb, 0, 0, width, height, &grad);

    int64_t t3 = esp_timer_get_time();
    fillRectangleWithGradientSIMD(&fb, 0, 0, width, height, &grad);
    int64_t t4 = esp_timer_get_time();

    FreeOSLogI(TAG, "SIMD (Fixed Pt) Time:  %lld us", (t4 - t3));
    FreeOSLogI(TAG, "Speedup Factor:        %.2fx", (float)(t2 - t1) / (float)(t4 - t3));

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
        FreeOSLogE(TAG, "Failed to allocate display buffer in PSRAM");
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
    
    FreeOSLogI(TAG, "Framebuffer allocated. Starting draw...");
    
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
    
    
    FreeOSLogI(TAG, "Drawing complete. Sending to LCD...");
    
    // 5. Send the completed buffer to the LCD
    // No conversion step is needed!
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle, 0, 0, width, height, display_buffer));
    
    FreeOSLogI(TAG, "Buffer sent. Halting in a loop.");
    
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
 FreeOSLogI(TAG, "Graphics task started.");
 
 // --- NEW: INITIALIZE FREETYPE *HERE* ---
 esp_err_t ft_ok = initialize_freetype();
 if (ft_ok != ESP_OK) {
 FreeOSLogE(TAG, "Failed to initialize FreeType, deleting task.");
 vTaskDelete(NULL); // Abort this task
 return;
 }
 // --- END NEW PART ---
 
 FreeOSLogI(TAG, "Graphics task started.");
 
 // 2. Allocate the single framebuffer in PSRAM
 int width = DISPLAY_HORIZONTAL_PIXELS;
 int height = DISPLAY_VERTICAL_PIXELS;
 int num_pixels = width * height;
 
 uint8_t *display_buffer = (uint8_t *)heap_caps_malloc(num_pixels * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
 
 if (!display_buffer) {
 FreeOSLogE(TAG, "Failed to allocate display buffer in PSRAM. Deleting task.");
 vTaskDelete(NULL); // Delete this task
 return;
 }
 FreeOSLogI(TAG, "Framebuffer allocated.");
 
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
 FreeOSLogI(TAG, "Starting draw...");
 
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
 
 FreeOSLogI(TAG, "Drawing complete. Sending to LCD...");
 
 esp_err_t err = esp_lcd_panel_draw_bitmap(lcd_handle, 0, 0, width, height, display_buffer);
 if (err != ESP_OK) {
 FreeOSLogE(TAG, "Failed to draw bitmap! Error: %s", esp_err_to_name(err));
 }
 
 // 5. Send the completed buffer to the LCD
 // This is the long-running function
 //ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle, 0, 0, width, height, display_buffer));
 
 FreeOSLogI(TAG, "Buffer sent.");
 
 
 // Wait forever until a command arrives
 if (xQueueReceive(g_graphics_queue, &cmd, QUEUE_MAX_DELAY) == pdTRUE) {
 
 // --- ADD THIS "RECEIVED" LOG ---
 FreeOSLogI(TAG, "Graphics task received command: %d", cmd.cmd);
 
 switch (cmd.cmd) {
 
 // --- Case 1: Draw a Rectangle ---
 case CMD_DRAW_RECT:
 FreeOSLogI(TAG, "Drawing rect to PSRAM");
 drawRectangleCFramebuffer(&fb, cmd.x, cmd.y, cmd.w, cmd.h, cmd.color, true);
 break;
 
 // --- Case 2: Draw Text ---
 case CMD_DRAW_TEXT:
 FreeOSLogI(TAG, "Drawing text to PSRAM");
 renderText(&fb, ft_face, cmd.text, cmd.x, cmd.y, white, 24, NULL);
 break;
 
 // --- Case 3: Update the LCD ---
 case CMD_UPDATE_AREA:
 {
 FreeOSLogI(TAG, "Pushing update to LCD");
 
 // 1. Create a small, *contiguous* buffer for the update
 uint8_t* temp_buffer = (uint8_t*) heap_caps_malloc(
 cmd.w * cmd.h * 3,
 MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
 if (!temp_buffer) {
 FreeOSLogE(TAG, "Failed to alloc temp_buffer!");
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
    FreeOSLogE(TAG_PNG, "PNG Error: %s", error_message);
    // Setting longjmp is dangerous in ESP-IDF (may bypass stack unwinding)
    // We rely on the caller checking the texture data for NULL/size.
}



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
        FreeOSLogE(TAG_PNG, "Failed to open image file: %s", imgPath);
        return NULL;
    }
    
    // --- Check PNG Signature ---
    uint8_t header[8];
    if (fread(header, 1, 8, png_file_handle) != 8 || png_sig_cmp(header, 0, 8)) {
        FreeOSLogE(TAG_PNG, "File is not a valid PNG signature.");
        fclose(png_file_handle);
        return NULL;
    }
    
    // 2. Setup libpng structures
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_error_handler, png_warning_handler);
    if (!png_ptr) {
        FreeOSLogE(TAG_PNG, "Failed to create PNG read structure.");
        fclose(png_file_handle);
        return NULL;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        FreeOSLogE(TAG_PNG, "Failed to create PNG info structure.");
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
    
    //FreeOSLogI(TAG_PNG, "Image found: %lu x %lu, Depth: %d, Type: %d", width, height, bit_depth, color_type);
    
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
        FreeOSLogE(TAG_PNG, "Final row size mismatch. Expected %lu, got %lu.", width * 4, row_bytes);
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
        FreeOSLogE(TAG_PNG, "Failed to allocate %u bytes in PSRAM for texture.", data_size);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(png_file_handle);
        return NULL;
    }
    
    // 7. Read image data row by row
    png_bytep* row_pointers = (png_bytep*)cc_safe_alloc(1, height * sizeof(png_bytep));
    if (!row_pointers) {
        FreeOSLogE(TAG_PNG, "Failed to allocate row pointers.");
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
    
    //FreeOSLogI(TAG_PNG, "PNG file loaded and decoded to PSRAM successfully.");
    return new_texture;
}



ImageTexture* load_jpeg_from_file(const char* imgPath) {
    FreeOSLogI(TAG_JPG, "Loading JPEG: %s", imgPath);
    
    // 1. Open File
    FILE *f = fopen(imgPath, "rb");
    if (!f) {
        FreeOSLogE(TAG_JPG, "Failed to open file: %s", imgPath);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    FreeOSLogI(TAG_JPG, "File Size: %d bytes", file_size); // <--- DEBUG 1
    
    if (file_size == 0) {
        FreeOSLogE(TAG_JPG, "File size is 0");
        fclose(f);
        return NULL;
    }
    
    // Allocate Input Buffer in PSRAM
    uint8_t *jpeg_input_buf = (uint8_t *)heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!jpeg_input_buf) {
        FreeOSLogE(TAG_JPG, "Failed to alloc input buffer");
        fclose(f);
        return NULL;
    }
    
    size_t bytes_read = fread(jpeg_input_buf, 1, file_size, f);
    fclose(f);
    
    FreeOSLogI(TAG_JPG, "Bytes Read: %d", bytes_read); // <--- DEBUG 2
    
    // --- DEBUG 3: PRINT HEADER BYTES ---
    if (bytes_read > 4) {
        FreeOSLogI(TAG_JPG, "Header Bytes: %02X %02X %02X %02X",
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
        FreeOSLogE(TAG_JPG, "Failed to parse JPEG header");
        heap_caps_free(jpeg_input_buf);
        return NULL;
    }
    
    int w = info.width;
    int h = info.height;
    FreeOSLogI(TAG_JPG, "JPEG Info: %d x %d", w, h);
    
    // --- 3. Allocate Temporary RGB Output Buffer ---
    // The decoder needs a place to dump the 3-byte RGB data
    size_t rgb_buf_size = w * h * 3;
    uint8_t *temp_rgb_buf = (uint8_t *)heap_caps_malloc(rgb_buf_size, MALLOC_CAP_SPIRAM);
    
    if (!temp_rgb_buf) {
        FreeOSLogE(TAG_JPG, "Failed to alloc temp RGB buffer");
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
        FreeOSLogE(TAG_JPG, "Failed to decode JPEG");
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
    
    FreeOSLogI(TAG_JPG, "JPEG Loaded and Converted.");
    return new_texture;
}

/*void updateArea(Framebuffer fb, GraphicsCommand cmd) {
    FreeOSLogI(TAG, "Pushing update to LCD (chunked)...");
    
    // Define a chunk size (e.g., 10 rows at a time)
    int rows_per_chunk = 10;
    
    // Calculate the size of our small, safe DMA buffer
    size_t chunk_size_bytes = cmd.w * rows_per_chunk * 3;
    
    // 1. Create the small, reusable DMA buffer
    uint8_t* temp_buffer = (uint8_t*) heap_caps_malloc(
                                                       chunk_size_bytes,
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!temp_buffer) {
        FreeOSLogE(TAG, "Failed to alloc temp_buffer for chunking!");
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
}*/

void updateArea(Framebuffer fb, GraphicsCommand cmd) {
    FreeOSLogI(TAG, "Pushing update to LCD (chunked)...");
    
    int rows_per_chunk = 10;
    size_t chunk_size_bytes = cmd.w * rows_per_chunk * 3;
    
    // 1. Create the small, reusable DMA buffer
    uint8_t* temp_buffer = (uint8_t*) heap_caps_malloc(
                                                       chunk_size_bytes,
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!temp_buffer) {
        FreeOSLogE(TAG, "Failed to alloc temp_buffer for chunking!");
        return;
    }
    
    // 2. Loop over the update area in chunks
    for (int y_chunk = 0; y_chunk < cmd.h; y_chunk += rows_per_chunk) {
        
        int rows_to_send = cmd.h - y_chunk;
        if (rows_to_send > rows_per_chunk) {
            rows_to_send = rows_per_chunk;
        }
        
        int chunk_y_start = cmd.y + y_chunk;
        int chunk_y_end = chunk_y_start + rows_to_send;
        
        // 3. Copy this chunk from PSRAM to our internal DMA buffer
        uint8_t* psram_ptr;
        uint8_t* temp_ptr = temp_buffer;
        for (int i = 0; i < rows_to_send; i++) {
            psram_ptr = &((uint8_t*)fb.pixelData)[((chunk_y_start + i) * fb.displayWidth + cmd.x) * 3];
            memcpy(temp_ptr, psram_ptr, cmd.w * 3);
            temp_ptr += cmd.w * 3;
        }
        
        // 4. Send *only this small chunk* to the LCD
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle,
                                                  cmd.x, chunk_y_start,
                                                  cmd.x + cmd.w, chunk_y_end,
                                                  temp_buffer));
                                                  
        // 5. THE DMA FIX: Wait for the SPI hardware to finish sending THIS chunk!
        // This stops the loop from overwriting temp_buffer while DMA is actively reading it.
        if (lcd_flush_ready_sem) {
            xSemaphoreTake(lcd_flush_ready_sem, pdMS_TO_TICKS(100)); // 100ms safety timeout
        }
    }
    
    // 6. Loop is completely finished, 100% safe to free the buffer now
    heap_caps_free(temp_buffer);
    
    touchEnabled = true;
}

//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/queue.h"

// The mailbox that passes coordinates between the cores
QueueHandle_t display_queue = NULL;

// The background task running on Core 1
void display_task(void *pvParameter) {
    GraphicsCommand cmd;
    
    while(1) {
        // 1. Wait here peacefully until the UI thread sends a dirty rectangle
        if (xQueueReceive(display_queue, &cmd, portMAX_DELAY)) {
            
            // 2. We received a command! Do the heavy lifting.
            size_t rect_bytes = cmd.w * cmd.h * 3;
            uint8_t* safe_buffer = (uint8_t*) heap_caps_malloc(rect_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
            
            if (safe_buffer) {
                uint8_t* dest_ptr = safe_buffer;
                for (int y = 0; y < cmd.h; y++) {
                    uint8_t* src_row_ptr = &((uint8_t*)fb.pixelData)[((cmd.y + y) * fb.displayWidth + cmd.x) * 3];
                    memcpy(dest_ptr, src_row_ptr, cmd.w * 3);
                    dest_ptr += cmd.w * 3;
                }
                
                // 3. Fire the DMA SPI transfer
                ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle, cmd.x, cmd.y, cmd.x + cmd.w, cmd.y + cmd.h, safe_buffer));
                
                // 4. Block THIS core until SPI is done.
                // Core 0 is completely unaffected and continues reading touch!
                if (lcd_flush_ready_sem) {
                    xSemaphoreTake(lcd_flush_ready_sem, portMAX_DELAY);
                }
                
                // 5. Free memory and loop back to wait for the next command
                heap_caps_free(safe_buffer);
            }
        }
    }
}

void updateArea1(Framebuffer fb, GraphicsCommand cmd) {
    // 1. First time this is called, set up the Queue and Dual-Core Task!
    if (display_queue == NULL) {
        // Create a queue that can hold 10 pending screen updates
        display_queue = xQueueCreate(10, sizeof(GraphicsCommand));
        
        // Boot up the Display Task and strictly pin it to Core 1
        xTaskCreatePinnedToCore(
            display_task,   // Function to run
            "DisplayTask",  // Task name
            16384,           // Stack size (4KB is plenty)
            NULL,           // Parameters
            5,              // Priority (High)
            NULL,           // Task handle
            1               // PIN TO CORE 1!
        );
        FreeOSLogI("LCD", "Dual-Core Rendering Pipeline Initialized!");
    }
    
    // 2. Send the command to the Queue.
    // The '0' means if the queue is full, just drop the frame and return instantly.
    // This naturally prevents the OS from lagging if you draw too fast!
    xQueueSend(display_queue, &cmd, 0);
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
 FreeOSLogE("LCD", "Failed to allocate DMA buffer for updateArea");
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
 FreeOSLogE("LCD", "Draw bitmap failed");
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
 // Ideally, you use xSemaphoreTake(transfer_done_sem, QUEUE_MAX_DELAY);
 
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
        FreeOSLogE(TAG, "Failed to allocate gear vertices.");
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
    FreeOSLogI(TAG, "Starting complex polygon draw...");
    
    // --- 1. Define the Complex Geometry (48-vertex Gear) ---
    int num_vertices = 0;
    // Center the gear at (160, 240)
    // Outer radius 130px, Inner radius 110px (short, stubby teeth)
    // 24 teeth = 48 total vertices
    Vector3* gear_vertices = create_gear_vertices(160.0f, 240.0f, 130.0f, 110.0f, 24, &num_vertices);
    
    if (!gear_vertices) {
        return; // Exit if allocation failed
    }
    FreeOSLogI(TAG, "Generated %d vertices for gear.", num_vertices);
    
    
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
    FreeOSLogI(TAG, "Complex polygon drawn in %llu microseconds.", (end_time - start_time));
    
    
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
        FreeOSLogE(TAG, "Failed to allocate star vertices.");
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
    
    FreeOSLogI(TAG, "Total Text Height: %d px", g_text_total_height);
}

// Helper to allocate the buffer (Call this in app_main or graphics_task setup)
void init_anim_buffer() {
    g_anim_backup_buffer = (uint8_t*)heap_caps_malloc(ANIM_W * ANIM_H * 3, MALLOC_CAP_SPIRAM);
    if (!g_anim_backup_buffer) {
        FreeOSLogE(TAG, "Failed to allocate animation backup buffer!");
    } else {
        FreeOSLogI(TAG, "Animation backup buffer allocated (120KB).");
    }
}

void triangle_animation_task(void *pvParameter) {
    
    FreeOSLogI(TAG, "triangle_animation_task");
    
    
    
    // 1. Wait a moment for the main UI (Gradient/Rects) to finish drawing
    //vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 2. Send command to CAPTURE the background (Snapshot the gradient behind us)
    GraphicsCommand cmd_save = { .cmd = CMD_ANIM_SAVE_BG, .x =  ANIM_X, .y = ANIM_Y, .w = ANIM_W, .h = ANIM_H};
    QueueSend(g_graphics_queue, &cmd_save, QUEUE_MAX_DELAY);
    
    float phase = 0.0f;
    const float speed = 0.1f;
    int center_x = 160;
    int center_y = 240;
    
    // Background is now handled by Restore, so we don't need a clear color
    
    while (1) {
        
        // --- STEP 1: RESTORE BACKGROUND (Erase previous triangle) ---
        // This copies the saved pixels back to the framebuffer
        GraphicsCommand cmd_restore = { .cmd = CMD_ANIM_RESTORE_BG, .x =  ANIM_X, .y = ANIM_Y, .w = ANIM_W, .h = ANIM_H };
        QueueSend(g_graphics_queue, &cmd_restore, 0);
        
        
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
            QueueSend(g_graphics_queue, &cmd_poly, 0);
        }
        
        // --- STEP 3: UPDATE SCREEN ---
        // Push the dirty area (Background + Triangle) to the LCD
        GraphicsCommand cmd_flush = {
            .cmd = CMD_UPDATE_AREA,
            .x = ANIM_X, .y = ANIM_Y,
            .w = ANIM_W, .h = ANIM_H
        };
        QueueSend(g_graphics_queue, &cmd_flush, 0);
        
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
    FreeOSLogI(TAG, "Graphics task started.");
    
    // --- INITIALIZE FREETYPE ---
    esp_err_t ft_ok = initialize_freetype();
    if (ft_ok != ESP_OK) {
        FreeOSLogE(TAG, "Failed to initialize FreeType, deleting task.");
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
        FreeOSLogE(TAG, "Failed to allocate display buffer in PSRAM. Deleting task.");
        vTaskDelete(NULL); // Delete this task
        return;
    }
    FreeOSLogI(TAG, "Framebuffer allocated.");
    
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
    FreeOSLogI(TAG, "Drawing initial UI...");
    
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
     FreeOSLogE(TAG, "Failed to draw initial bitmap! Error: %s", esp_err_to_name(err));
     }
     FreeOSLogI(TAG, "Initial UI drawn. GRAPHICS TASK READY FOR COMMANDS.");*/
    
    
    
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
        if (xQueueReceive(g_graphics_queue, &cmd, QUEUE_MAX_DELAY) == pdTRUE) {
            
            //FreeOSLogI(TAG, "Graphics task received command: %d", cmd.cmd);
            
            switch (cmd.cmd) {
                    
                    // --- Case 1: Draw a Rectangle ---
                case CMD_DRAW_RECT:
                    FreeOSLogI(TAG, "Drawing rect to PSRAM");
                    drawRectangleCFramebuffer(&fb, cmd.x, cmd.y, cmd.w, cmd.h, cmd.color, true);
                    break;
                    
                case CMD_DRAW_FRAMEBUFFER: {
                    // Unpack the struct and call the reusable blitter!
                    // 'framebuffer' is your main OS screen buffer
                    drawFramebuffer(&fb, cmd.pixelBuffer, cmd.x, cmd.y);
                    break;
                }
                    
                    // --- Case 2: Draw Text ---
                case CMD_DRAW_TEXT:
                    FreeOSLogI(TAG, "Drawing text to PSRAM");
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
                    FreeOSLogI(TAG, "Drawing Text Box: %s", cmd.text);
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
                    FreeOSLogI(TAG, "calling function save_cursor_background");
                    save_cursor_background(&fb, cmd.x, cmd.y);
                    
                    // 2. Draw the cursor immediately (first blink ON)
                    drawRectangleCFramebuffer(&fb, cmd.x, cmd.y, CURSOR_W, CURSOR_H, white, true);
                    
                    // 3. Send the small update area to show the cursor
                    GraphicsCommand cmd_update = {.cmd = CMD_UPDATE_AREA, .x = cmd.x, .y = cmd.y, .w = CURSOR_W, .h = CURSOR_H};
                    //QueueSend(g_graphics_queue, &cmd_update, 0);
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
                    //QueueSend(g_graphics_queue, &cmd_draw, 0);
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
                    QueueSend(g_graphics_queue, &cmd_redraw_viewport, 0);
                    
                    FreeOSLogI(TAG, "Scroll updated to: %d/%d", g_scroll_offset_y, max_scroll);
                    break;
                }
                    // Inside graphics_task switch (cmd.cmd):
                case CMD_LOAD_PAGE_1:{ // Triggered by CMD_SCROLL_CONTENT
                    FreeOSLogI(TAG, "Redrawing entire view due to scroll event.");
                    
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

                        //FreeOSLogI(TAG, "SIMD (Fixed Pt) Time:  %lld us", (t4 - t3));
                        //FreeOSLogI(TAG, "Speedup Factor:        %.2fx", (float)(t2 - t1) / (float)(t4 - t3));
                    
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
                            drawImageTextureWithAlpha(
                                &fb,
                                tex,
                                cmd.x, cmd.y,
                                cmd.w, cmd.h, cmd.alpha
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
                        FreeOSLogI("GFX", "Received Pixel Buffer Command");
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
                        FreeOSLogI(TAG, "Animation background saved.");
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


