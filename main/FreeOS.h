//
//  FreeOS.h
//  
//
//  Created by Chris Galzerano on 2/7/26.
//

#ifndef FreeOS_h
#define FreeOS_h

#include <stdio.h>
#include <main.h>

#include <LogWrapper.h>
#include <QueueWrapper.h>


static bool cpuGraphics = true;

static const char *TAG = "scan";

#define MAX_VIEW_STACK 5

// --- Global Animation State ---
// 200x200 pixels * 3 bytes = 120 KB (Must use PSRAM)
#define ANIM_W 200
#define ANIM_H 200
#define ANIM_X 60  // Center X (160) - Half Width (100)
#define ANIM_Y 140 // Center Y (240) - Half Height (100)

#define MAX_INTERACT_W 80
#define MAX_INTERACT_H 100
#define BUFFER_SIZE (MAX_INTERACT_W * MAX_INTERACT_H * 3)

#define CURSOR_W 2   // Width of the cursor (e.g., 10 pixels wide)
#define CURSOR_H 18   // Height of the cursor (matching 24pt font)
#define CURSOR_BPP 3  // Bytes per pixel (RGB888)

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

// Parameters for the image animation
#define IMG_ANIM_X 130  // 160 - 50 (Centered)
#define IMG_ANIM_Y 210  // 240 - 50 (Centered)
#define IMG_ANIM_W 60
#define IMG_ANIM_H 60

// Define the commands
typedef enum {
    CMD_DRAW_RECT,       // NEW: Draw a solid rectangle
    CMD_DRAW_TEXT,       // Draws text
    CMD_DRAW_TEXT_BOX,
    CMD_DRAW_TEXT_BOX_CACHED,
    CMD_DRAW_FRAMEBUFFER,
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
    float alpha;
} GraphicsCommand;

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

typedef struct {
    CCView* container;
    CCImageView* icon;
    CCLabel* label;
} CCIconView;

// Keyboard Modes
typedef enum {
    KB_MODE_ABC_LOWER,
    KB_MODE_ABC_UPPER,
    KB_MODE_NUMBERS,
    KB_MODE_SYMBOLS
} KeyboardMode;

typedef enum {
    IMAGE_TYPE_UNKNOWN = 0,
    IMAGE_TYPE_PNG,
    IMAGE_TYPE_JPEG
} ImageFileType;


extern FT_Library ft_library;
extern FT_Face    ft_face;
extern uint8_t* font_buffer;
extern Framebuffer fb;
extern MyQueueHandle_t g_graphics_queue;

extern bool setupui;
extern bool touchEnabled;

// --- Global Root View ---
extern CCView* mainWindowView;
extern CCScrollView* g_active_scrollview;
extern int g_touch_last_y;
extern int lastOffY;
extern CCTextView* myDemoTextView;

// --- Global Scroll State ---
extern int g_text_scroll_y;
extern int g_text_total_height;
extern int g_text_view_h;
extern const char* g_long_text_ptr;
extern bool notFirstTouch;

// --- FreeType Cache Globals ---
extern FTC_Manager     g_ftc_manager;
extern FTC_ImageCache  g_ftc_image_cache;
extern FTC_CMapCache   g_ftc_cmap_cache;

// --- View Stack ---
// Note: Removed 'static' so all files share the same stack
extern CCView* viewStack[MAX_VIEW_STACK];
extern int viewStackPointer;

extern CurrentView currentView;

// --- App/System State ---
extern bool openedApp;
extern CCArray* files;
extern CCArray* settings;

// --- Keyboard Globals ---
extern CCView* uiKeyboardView;
extern CCLabel* uiTargetLabel;
extern CCString* uiInputBuffer;

// --- Animation & Backup Buffers ---
extern uint8_t* g_anim_backup_buffer;
extern uint8_t* g_ui_backup_buffer_A;
extern uint8_t* g_ui_backup_buffer_B;

extern CCArray* g_grid_items_registry;
extern CCView* g_last_touched_icon;

// --- Global Cursor State ---
extern uint8_t* g_cursor_backup_buffer;
// Note: Removed 'static' so these can be accessed across files
extern bool addedCursor;
extern int g_cursor_x;
extern int g_cursor_y;
extern bool g_cursor_visible;
extern TaskHandle_t g_cursor_blink_handle;

// --- Scrolling UI State ---
extern int g_scroll_offset_y;
extern int g_scroll_total_height;
extern int g_scroll_viewport_h;
extern int g_drag_start_y;
extern float g_drag_velocity;

// --- Keyboard Internal State ---
extern bool hasDrawnKeyboard;
extern int keyboardCursorPosition;

// --- UI Highlighting ---
extern CCView* g_pressed_icon_view;
extern ColorRGBA g_color_highlight;
extern ColorRGBA g_color_transparent;

extern KeyboardMode kbCurrentMode;

// --- Task Handles ---
extern TaskHandle_t g_triangle_task_handle;
extern TaskHandle_t g_image_task_handle;

// --- Image Decoders ---
extern const char *TAG_PNG;
extern FILE *png_file_handle;
extern uint8_t *png_load_buffer;
extern int png_load_width;
extern int png_load_height;
extern const char *TAG_JPG;

/*void hideTriangleAnimation(void);
void triangle_animation_task(void *pvParameter);
void testGraphics(void);
void showTriangleAnimation(void);
void showRotatingImageAnimation(void);
void hideRotatingImageAnimation(void);
void rotating_image_task(void *pvParameter);
void drawViewHierarchy(void* object, int parentX, int parentY, CCRect currentClip, bool notHierarchy);
CCView* find_subview_at_point(CCView* container, int globalX, int globalY);*/

void list_directory_contents(const char *mount_point);
void graphics_task(void *arg);
const char* letterForCoordinate(void);
FT_Error face_requester(FTC_FaceID face_id, FT_Library library, FT_Pointer req_data, FT_Face* aface);
void init_freetype_cache_system(void);
void push_view(CCView* currentView);
CCView* pop_view();
void teardown_keyboard_data(void);
void close_current_app(void);
CCArray* get_directory_files_as_array(const char *mount_point);
void init_ui_buffers();
CCPoint getAbsoluteOrigin(CCView* view);
CCRect getAbsoluteVisibleRect(CCView* view);
CCPoint viewConvertPoint(CCView* targetView, int globalX, int globalY);
void update_label_safe(CCLabel* label);
void update_view_area_via_parent(CCView* view);
void update_view_only(CCView* view);
void update_full_ui(void);
void update_full_ui1(void);
void updateArea1(Framebuffer fb, GraphicsCommand cmd);
void drawShapeLayer(CCShapeLayer* shapeLayer, int absX, int absY);
CCRect intersectRects(CCRect r1, CCRect r2);
void drawViewHierarchy(void* object, int parentX, int parentY, CCRect currentClip, bool notHierarchy);
CCIconView* create_icon_view(CCRect* frame, const char* imgPath, const char* title);
CCArray* create_grid_data_source(void);
void drawHomeMenu(void);
void drawSampleViews(void);
void setup_ui_demo(void);
esp_err_t setup_cursor_buffers();
void save_cursor_background(Framebuffer *fb, int x, int y);
void restore_cursor_background(Framebuffer *fb);
Vector3* create_star_vertices(float centerX, float centerY, float outerRadius, float innerRadius, int *numVertices);
int randNumberTo(int max);
const char* letterForCoordinate(void);
CCScrollView* findParentScrollView(CCView* view);
CCView* find_grid_item_at(int x, int y);
CCString* formatFileSize(int bytes);
CCView* create_key_btn(const char* text, int x, int y, int w, int h, int tag);
void layout_keyboard_keys(void);
void setup_keyboard_ui(CCLabel* targetLabel);
void hide_keyboard_ui(void);
CCView* find_key_at_point(int x, int y);
void handle_keyboard_touch(int x, int y);
void debug_print_view_hierarchy(CCView* view, int depth);
void hideTriangleAnimation(void);
void showRotatingImageAnimation(void);
void hideRotatingImageAnimation(void);
void rotating_image_task(void *pvParameter);
void showTriangleAnimation(void);
CCView* find_subview_at_point(CCView* container, int globalX, int globalY);
CCView* find_subview_at_point_recursive(CCView* container, int globalX, int globalY);
void print_heap_info();
long get_file_size(const char *filename);
void load_and_execute_program(const char* file_path);
void list_directory_contents(const char *mount_point);
void testCPUGraphicsBenchmark(void);
void testCPUGraphics(void);
static void png_read_data(png_structp png_ptr, png_bytep out_data, png_size_t length);
static void png_warning_handler(png_structp png_ptr, png_const_charp warning_message);
static void png_error_handler(png_structp png_ptr, png_const_charp error_message);
ImageFileType get_image_type_from_path(const char* path);
ImageTexture* load_image_from_file(const char* imgPath);
ImageTexture* load_jpeg_from_file(const char* imgPath);
void updateArea(Framebuffer fb, GraphicsCommand cmd);
void draw_current_view(Framebuffer *fb, FT_Face ft_face);
Vector3* create_gear_vertices(float centerX, float centerY, float outerRadius, float innerRadius, int numTeeth, int *outNumVertices);
Vector3* create_spiked_center_gear_vertices(float centerX, float centerY, float outerRadius, float innerRadius, float holeOuterRadius, float holeInnerRadius, int numTeeth, int *outNumVertices);
void draw_complex_gear_gradient_example(Framebuffer *fb);
void draw_star(Framebuffer *fb);
void setup_scroll_text_demo();
void init_anim_buffer();
void triangle_animation_task(void *pvParameter);

#include "SystemApps.h"

#endif /* FreeOS_h */
