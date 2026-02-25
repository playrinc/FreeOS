//
//  FilesApp.h
//  
//
//  Created by Chris Galzerano on 2/17/26.
//

#ifndef FilesApp_h
#define FilesApp_h

#include <stdio.h>
#include <main.h>

// SystemApps.h

// --- Files App Globals ---
//extern CCView* mainWindowView;
extern CCView* uiFilesHeader;
extern CCView* uiFilesContextMenu; // The popup menu
extern CCView* uiFilesProperties;
extern char currentPath[512];
extern CCArray* currentFileList;
extern int filesPageIndex;

// --- Sort State ---
typedef enum {
    SORT_MODE_NAME,
    SORT_MODE_DATE,
    SORT_MODE_SIZE
} FileSortMode;
extern FileSortMode fileSortMode;
extern bool fileSortAscending;

extern char clipboardPath[512];
extern bool isCutOperation;

// Files Constants
#define FILES_PER_PAGE 5

// Tags
#define TAG_FILES_ITEM_BASE 4000
#define TAG_FILES_PREV      4100
#define TAG_FILES_NEXT      4101
#define TAG_FILES_UP_DIR    4102

#define TAG_NEW_FOLDER_CREATE 1052
#define TAG_NEW_FOLDER_CANCEL 1053

#define TAG_FILES_APP_WINDOW 9999

// --- Context Menu Tags ---
#define TAG_CTX_OPEN        4200
#define TAG_CTX_COPY        4201
#define TAG_CTX_NEW         4202 // Requested: New
#define TAG_CTX_PROPS       4203 // Requested: Properties
#define TAG_CTX_SEND        4204 // Requested: Send
#define TAG_CTX_COMPRESS    4205 // Requested: Compress
#define TAG_CTX_DECOMPRESS  4206 // Requested: Decompress
#define TAG_CTX_CANCEL      4299
#define TAG_CTX_PASTE       1050
#define TAG_CTX_PROPS_CLOSE 1051

// --- Touch Logic ---
#define LONG_PRESS_MS 600

void format_file_size(long bytes, char* buffer, size_t bufSize);
int file_compare(const void* a, const void* b);
void sort_file_list();
void files_open_file(const char* name);
void scan_current_directory();
void show_context_menu(const char* filename);
void setup_files_ui(void);
void freeFilesView(void);
void handle_files_touch(int x, int y, int touchState);
void show_properties_view(const char* filename);
void show_new_folder_dialog(void);
void show_directory_context_menu(void);

#endif /* FilesApp_h */
