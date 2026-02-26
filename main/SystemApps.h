//
//  SystemApps.h
//  
//
//  Created by Chris Galzerano on 2/9/26.
//

#ifndef SystemApps_h
#define SystemApps_h

#include <stdio.h>
#include <main.h>



// --- Settings Globals ---
extern int settingsPageIndex;
extern const int SETTINGS_PER_PAGE;
extern CCView* uiSettingsContainer;
extern CCView* buttonsContainer;

// Tags to identify buttons
#define TAG_SETTINGS_PREV 2001
#define TAG_SETTINGS_NEXT 2002
#define TAG_SETTINGS_ROW_BASE 3000

// --- Calculator State ---
extern CCString* calcDisplayStr;
extern double calcStoredValue;
extern char calcCurrentOp;
extern bool calcIsNewEntry;

// Tag constants
#define TAG_BTN_CLEAR 10
#define TAG_BTN_EQUALS 11
#define TAG_BTN_ADD 12
#define TAG_BTN_SUB 13
#define TAG_BTN_MUL 14
#define TAG_BTN_DIV 15

#define SCREEN_W 320

void update_calculator_label(void);
extern CCLabel* uiCalcLabel;

// --- Gallery Globals ---
extern CCArray* galleryImagePaths;
extern int galleryCurrentPage;
extern int gallerySelectedIdx;
extern float galleryZoomScale;

// Layout Constants
#define ITEMS_PER_PAGE 12
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
#define TAG_GAL_PHOTO_BASE 1000

// --- WiFi Layout Globals ---
extern CCView* uiWifiContainer;
extern CCView* uiWifiListContainer;
extern CCView* uiWifiToggleBtn;
extern bool isWifiEnabled;

// Tag Constants
#define TAG_WIFI_TOGGLE      4000
#define TAG_WIFI_NET_BASE    4100
#define TAG_WIFI_BACK        4001

// Layout Constants
#define WIFI_ROW_HEIGHT      50
#define WIFI_HEADER_HEIGHT   140

// --- WiFi Data Structures ---
#define MAX_WIFI_RESULTS 20

typedef struct {
    char ssid[33];
    int rssi;
    int channel;
    int auth_mode;
} WifiNetwork;

// Global access to scan results
extern WifiNetwork g_wifi_scan_results[MAX_WIFI_RESULTS];
extern int g_wifi_scan_count;
extern bool wifi_initialized;

// --- Pagination Globals ---
extern int g_wifi_page_index;
#define WIFI_ITEMS_PER_PAGE  5

// New Tags for Buttons
#define TAG_WIFI_BTN_PREV    4002
#define TAG_WIFI_BTN_NEXT    4003

void setup_wifi_ui(void);
void init_wifi_stack_once(void);
void trigger_wifi_scan(void);



//void setup_files_ui(void);
void update_settings_list(void);
void setup_settings_ui(void);
void handle_settings_touch(int x, int y, int touchState);
void handle_calculator_input(int tag);
void setup_calculator_ui(void);
void update_calculator_label(void);
void init_gallery_data();
void handle_gallery_touch(int tag);
void layout_grid_mode(void);
void layout_detail_mode(void);
void setup_gallery_ui(void);
void setup_text_ui(void);
CCView* create_wifi_row(const char* ssid, int rssi, int index, int yPos);
void refresh_wifi_list_ui(void);
void setup_wifi_ui(void);
void openHomeMenuItem(int tag);
void handle_wifi_touch(int x, int y);

#endif /* SystemApps_h */
