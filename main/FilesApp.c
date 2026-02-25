//
//  FilesApp.c
//
//
//  Created by Chris Galzerano on 2/17/26.
//

#include "FilesApp.h"

//
//  SystemApps.c
//  Files App - Table View Implementation
//

#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include "esp_vfs_fat.h"

// --- Globals ---
FileSortMode fileSortMode = SORT_MODE_NAME;
bool fileSortAscending = true;
CCView* uiFilesHeader = NULL;

// Re-declaring globals from previous context
char currentPath[512]= "/";
CCArray* currentFileList= NULL;
int filesPageIndex = 0;
//CCView* mainWindowView= NULL;
CCView* uiFilesContextMenu= NULL;

char clipboardPath[512] = {0};
bool isCutOperation = false;
CCView* uiFilesProperties = NULL;

CCView* uiNewFolderDialog = NULL;
CCLabel* uiNewFolderInput = NULL;
CCArray* uiDynamicPageViews = NULL;

// --- Constants ---
#define COL_NAME_WIDTH 170
#define COL_DATE_WIDTH 90
#define COL_SIZE_WIDTH 60
#define ROW_HEIGHT 40

// --- Helpers ---

// Long Press State
static uint64_t touchStartTime = 0;
static int touchStartX = 0;
static int touchStartY = 0;
static char selectedFilename[128] = {0};
static bool is_pressing = false;
static bool long_press_fired = false;

// --- Helper: Detect Archives ---
bool is_archive_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    if (strcasecmp(ext, ".zip") == 0 ||
        strcasecmp(ext, ".tar") == 0 ||
        strcasecmp(ext, ".gz") == 0 ||
        strcasecmp(ext, ".tar.gz") == 0) {
        return true;
    }
    return false;
}


static bool is_root_mode() {
    return (strcmp(currentPath, "/") == 0);
}

void format_file_size(long bytes, char* buffer, size_t bufSize) {
    if (bytes < 1024) {
        snprintf(buffer, bufSize, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, bufSize, "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(buffer, bufSize, "%.1f MB", bytes / (1024.0 * 1024.0));
    }
}

int compare_file_items(CCDictionary* dictA, CCDictionary* dictB) {
    CCString* keyName = ccs("name");
    CCString* keyDate = ccs("time_t");
    CCString* keySize = ccs("size_bytes");
    
    CCString* nameA = (CCString*)dictionaryObjectForKey(dictA, keyName);
    CCString* nameB = (CCString*)dictionaryObjectForKey(dictB, keyName);
    CCNumber* sizeA = (CCNumber*)dictionaryObjectForKey(dictA, keySize);
    CCNumber* sizeB = (CCNumber*)dictionaryObjectForKey(dictB, keySize);
    CCNumber* timeA = (CCNumber*)dictionaryObjectForKey(dictA, keyDate);
    CCNumber* timeB = (CCNumber*)dictionaryObjectForKey(dictB, keyDate);
    
    freeCCString(keyName);
    freeCCString(keyDate);
    freeCCString(keySize);
    
    int result = 0;
    
    switch (fileSortMode) {
        case SORT_MODE_NAME:
            // FIXED: Using cStringOfString
            result = strcmp(cStringOfString(nameA), cStringOfString(nameB));
            break;
        case SORT_MODE_SIZE: {
            double valA = numberDoubleValue(sizeA);
            double valB = numberDoubleValue(sizeB);
            if (valA > valB) result = 1;
            else if (valA < valB) result = -1;
            break;
        }
        case SORT_MODE_DATE: {
            double valA = numberDoubleValue(timeA);
            double valB = numberDoubleValue(timeB);
            if (valA > valB) result = 1;
            else if (valA < valB) result = -1;
            break;
        }
    }
    
    return fileSortAscending ? result : -result;
}

void sort_file_list() {
    if (!currentFileList || currentFileList->count < 2) return;
    
    // Bubble Sort for safe array access
    for (int i = 0; i < currentFileList->count - 1; i++) {
        for (int j = 0; j < currentFileList->count - i - 1; j++) {
            CCDictionary* a = (CCDictionary*)arrayObjectAtIndex(currentFileList, j);
            CCDictionary* b = (CCDictionary*)arrayObjectAtIndex(currentFileList, j + 1);
            
            if (compare_file_items(a, b) > 0) {
                arrayDeleteObjectAtIndex(currentFileList, j);
                arrayInsertObjectAtIndex(currentFileList, a, j + 1);
            }
        }
    }
}

// --- Directory Scanning ---

void scan_current_directory() {
    if (currentFileList) {
        currentFileList = NULL; // Assuming close_current_app already handles the freeing
    }
    
    currentFileList = array();
    
    if (is_root_mode()) {
        CCDictionary* d1 = dictionary();
        // BOTH Values and Keys must be copied to the heap so freeElement() doesn't crash on ROM literals
        dictionarySetObjectForKey(d1, copyCCString(ccs("Internal Storage")), copyCCString(ccs("name")));
        dictionarySetObjectForKey(d1, copyCCString(ccs("-")), copyCCString(ccs("date")));
        dictionarySetObjectForKey(d1, copyCCString(ccs("-")), copyCCString(ccs("size")));
        dictionarySetObjectForKey(d1, numberWithInt(1), copyCCString(ccs("is_dir")));
        dictionarySetObjectForKey(d1, numberWithInt(0), copyCCString(ccs("size_bytes")));
        dictionarySetObjectForKey(d1, numberWithInt(0), copyCCString(ccs("time_t")));
        arrayAddObject(currentFileList, d1);
        
        CCDictionary* d2 = dictionary();
        dictionarySetObjectForKey(d2, copyCCString(ccs("SD Card")), copyCCString(ccs("name")));
        dictionarySetObjectForKey(d2, copyCCString(ccs("-")), copyCCString(ccs("date")));
        dictionarySetObjectForKey(d2, copyCCString(ccs("-")), copyCCString(ccs("size")));
        dictionarySetObjectForKey(d2, numberWithInt(1), copyCCString(ccs("is_dir")));
        dictionarySetObjectForKey(d2, numberWithInt(0), copyCCString(ccs("size_bytes")));
        dictionarySetObjectForKey(d2, numberWithInt(0), copyCCString(ccs("time_t")));
        arrayAddObject(currentFileList, d2);
        return;
    }
    
    DIR *d = opendir(currentPath);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] == '.') continue;
            
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, dir->d_name);
            
            struct stat st;
            stat(fullPath, &st);
            
            CCDictionary* item = dictionary();
            
            // Heap-allocate name and key
            dictionarySetObjectForKey(item, copyCCString(ccs(dir->d_name)), copyCCString(ccs("name")));
            
            bool isDir = S_ISDIR(st.st_mode);
            // CCNumber is already dynamically allocated by numberWithInt, but the key needs copying
            dictionarySetObjectForKey(item, numberWithInt(isDir ? 1 : 0), copyCCString(ccs("is_dir")));
            
            char sizeBuf[32];
            if (isDir) strcpy(sizeBuf, "--");
            else format_file_size(st.st_size, sizeBuf, 32);
            
            // Heap-allocate size and key
            dictionarySetObjectForKey(item, copyCCString(ccs(sizeBuf)), copyCCString(ccs("size")));
            dictionarySetObjectForKey(item, numberWithDouble((double)st.st_size), copyCCString(ccs("size_bytes")));
            
            struct tm *tm_info = localtime(&st.st_mtime);
            char dateBuf[32];
            strftime(dateBuf, 32, "%m/%d/%y", tm_info);
            
            // Heap-allocate date and key
            dictionarySetObjectForKey(item, copyCCString(ccs(dateBuf)), copyCCString(ccs("date")));
            dictionarySetObjectForKey(item, numberWithDouble((double)st.st_mtime), copyCCString(ccs("time_t")));
            
            arrayAddObject(currentFileList, item);
        }
        closedir(d);
        sort_file_list();
    }
}


// --- Context Menu Logic ---

void show_context_menu(const char* filename) {
    if (uiFilesContextMenu) {
        uiFilesContextMenu = NULL;
    }
    
    uiFilesContextMenu = viewWithFrame(ccRect(40, 40, 240, 400));
    uiFilesContextMenu->backgroundColor = color(0.14, 0.14, 0.14, 1.0);
    
    CCLabel* title = labelWithFrame(ccRect(10, 10, 220, 20));
    title->text = copyCCString(ccs(filename));
    title->fontSize = 16;
    title->textColor = color(1.0, 1.0, 0.0, 1.0);
    viewAddSubview(uiFilesContextMenu, title);
    
    // Build options dynamically
    const char* options[10];
    int tags[10];
    int count = 0;
    
    options[count] = "Open";       tags[count++] = TAG_CTX_OPEN;
    options[count] = "Copy";       tags[count++] = TAG_CTX_COPY;
    
    // Only show Paste if something is on the clipboard
    if (strlen(clipboardPath) > 0) {
        options[count] = "Paste";  tags[count++] = TAG_CTX_PASTE;
    }
    
    options[count] = "Properties"; tags[count++] = TAG_CTX_PROPS;
    options[count] = "New Folder"; tags[count++] = TAG_CTX_NEW;
    
    // Check if it's an archive
    if (is_archive_file(filename)) {
        options[count] = "Decompress"; tags[count++] = TAG_CTX_DECOMPRESS;
    } else {
        options[count] = "Compress";   tags[count++] = TAG_CTX_COMPRESS;
    }
    
    options[count] = "Cancel";     tags[count++] = TAG_CTX_CANCEL;
    
    // Render the dynamic list
    int y = 40;
    for (int i = 0; i < count; i++) {
        CCView* btn = viewWithFrame(ccRect(10, y, 220, 35));
        btn->backgroundColor = color(0.24, 0.24, 0.24, 1.0);
        btn->tag = tags[i];
        
        CCLabel* l = labelWithFrame(ccRect(10, 8, 200, 20));
        l->text = copyCCString(ccs(options[i]));
        l->fontSize = 16;
        l->textColor = color(1.0, 1.0, 1.0, 1.0);
        viewAddSubview(btn, l);
        
        viewAddSubview(uiFilesContextMenu, btn);
        y += 40;
    }
    
    // Adjust container height to fit the dynamic number of buttons
    uiFilesContextMenu->frame->size->height = y + 10;
    
    viewAddSubview(mainWindowView, uiFilesContextMenu);
}

void show_directory_context_menu(void) {
    if (uiFilesContextMenu) uiFilesContextMenu = NULL;
    
    uiFilesContextMenu = viewWithFrame(ccRect(40, 100, 240, 200));
    uiFilesContextMenu->backgroundColor = color(0.14, 0.14, 0.14, 1.0);
    
    CCLabel* title = labelWithFrame(ccRect(10, 10, 220, 20));
    title->text = copyCCString(ccs("Current Folder"));
    title->fontSize = 16;
    title->textColor = color(1.0, 1.0, 0.0, 1.0);
    viewAddSubview(uiFilesContextMenu, title);
    
    const char* options[5];
    int tags[5];
    int count = 0;
    
    // Only show paste if clipboard has data
    if (strlen(clipboardPath) > 0) {
        options[count] = "Paste";  tags[count++] = TAG_CTX_PASTE;
    }
    options[count] = "New Folder"; tags[count++] = TAG_CTX_NEW;
    options[count] = "Properties"; tags[count++] = TAG_CTX_PROPS;
    options[count] = "Cancel";     tags[count++] = TAG_CTX_CANCEL;
    
    int y = 40;
    for (int i = 0; i < count; i++) {
        CCView* btn = viewWithFrame(ccRect(10, y, 220, 35));
        btn->backgroundColor = color(0.24, 0.24, 0.24, 1.0);
        btn->tag = tags[i];
        
        CCLabel* l = labelWithFrame(ccRect(10, 8, 200, 20));
        l->text = copyCCString(ccs(options[i]));
        l->fontSize = 16;
        l->textColor = color(1.0, 1.0, 1.0, 1.0);
        viewAddSubview(btn, l);
        
        viewAddSubview(uiFilesContextMenu, btn);
        y += 40;
    }
    
    uiFilesContextMenu->frame->size->height = y + 10;
    viewAddSubview(mainWindowView, uiFilesContextMenu);
}

void show_new_folder_dialog(void) {
    if (uiNewFolderDialog) uiNewFolderDialog = NULL;
    
    // Placed near the top (y=30) so the keyboard has room below it
    uiNewFolderDialog = viewWithFrame(ccRect(20, 30, 280, 130));
    uiNewFolderDialog->backgroundColor = color(0.18, 0.18, 0.18, 1.0); // Dark Gray
    
    // Title
    CCLabel* title = labelWithFrame(ccRect(10, 10, 260, 20));
    title->text = copyCCString(ccs("Create New Folder"));
    title->fontSize = 16;
    title->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(uiNewFolderDialog, title);
    
    // Pseudo-Text Field (The Target Label)
    uiNewFolderInput = labelWithFrame(ccRect(10, 40, 260, 30));
    uiNewFolderInput->text = copyCCString(ccs("New_Folder")); // Default placeholder
    uiNewFolderInput->fontSize = 16;
    uiNewFolderInput->textColor = color(0.0, 0.0, 0.0, 1.0); // Black text so it's readable
    uiNewFolderInput->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    
    // Give it a white background so it visually looks like an input box
    uiNewFolderInput->view->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(uiNewFolderDialog, uiNewFolderInput);
    
    // Cancel Button
    CCView* btnCancel = viewWithFrame(ccRect(10, 85, 120, 35));
    btnCancel->backgroundColor = color(0.4, 0.4, 0.4, 1.0); // Gray
    btnCancel->tag = TAG_NEW_FOLDER_CANCEL;
    
    CCLabel* lblCancel = labelWithFrame(ccRect(0, 8, 120, 20));
    lblCancel->text = copyCCString(ccs("Cancel"));
    lblCancel->fontSize = 16;
    lblCancel->textAlignment = CCTextAlignmentCenter;
    lblCancel->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(btnCancel, lblCancel);
    viewAddSubview(uiNewFolderDialog, btnCancel);
    
    // Create Button
    CCView* btnCreate = viewWithFrame(ccRect(150, 85, 120, 35));
    btnCreate->backgroundColor = color(0.0, 0.47, 1.0, 1.0); // Blue
    btnCreate->tag = TAG_NEW_FOLDER_CREATE;
    
    CCLabel* lblCreate = labelWithFrame(ccRect(0, 8, 120, 20));
    lblCreate->text = copyCCString(ccs("Create"));
    lblCreate->fontSize = 16;
    lblCreate->textAlignment = CCTextAlignmentCenter;
    lblCreate->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(btnCreate, lblCreate);
    viewAddSubview(uiNewFolderDialog, btnCreate);
    
    viewAddSubview(mainWindowView, uiNewFolderDialog);
    
    // Summon the OS Keyboard and point it at our Label!
    setup_keyboard_ui(uiNewFolderInput);
}

void drawFileTableView(void) {
    // --- 1. TARGETED CLEANUP ---
    if (uiDynamicPageViews) {
        for (int i = 0; i < uiDynamicPageViews->count; i++) {
            CCView* oldView = (CCView*)arrayObjectAtIndex(uiDynamicPageViews, i);
            
            // Unlink from the main window so the graphics engine stops rendering it
            viewRemoveFromSuperview(oldView);
            
            // CRITICAL: You must still free the memory, otherwise the detached views leak!
            freeViewHierarchy(oldView);
        }
        freeElement(uiDynamicPageViews); // Delete the tracking array itself
        uiDynamicPageViews = NULL;
    }
    
    // --- 2. BUILD NEW PAGE ---
    uiDynamicPageViews = array(); // Create a fresh array for the new page
    
    int headerY = 30;
#define ROW_HEIGHT 40
    int yPos = headerY + 28;
    int count = currentFileList ? currentFileList->count : 0;
    int startIdx = filesPageIndex * 5;
    int endIdx = startIdx + 5;
    if (endIdx > count) endIdx = count;
    
    // Draw Rows
    for (int i = startIdx; i < endIdx; i++) {
        CCDictionary* item = (CCDictionary*)arrayObjectAtIndex(currentFileList, i);
        
        CCString* keyName = ccs("name");
        CCString* keyDate = ccs("date");
        CCString* keySize = ccs("size");
        CCString* keyIsDir = ccs("is_dir");
        
        CCString* nameStr = (CCString*)dictionaryObjectForKey(item, keyName);
        CCString* dateStr = (CCString*)dictionaryObjectForKey(item, keyDate);
        CCString* sizeStr = (CCString*)dictionaryObjectForKey(item, keySize);
        CCNumber* isDirNum = (CCNumber*)dictionaryObjectForKey(item, keyIsDir);
        
        freeCCString(keyName);
        freeCCString(keyDate);
        freeCCString(keySize);
        freeCCString(keyIsDir);
        
        bool isDir = (numberIntValue(isDirNum) > 0);
        
        CCView* row = viewWithFrame(ccRect(0, yPos, 320, ROW_HEIGHT));
        row->backgroundColor = (i % 2 == 0) ? color(0.12, 0.12, 0.12, 1.0) : color(0.14, 0.14, 0.14, 1.0);
        row->tag = TAG_FILES_ITEM_BASE + i;
        
        CCView* icon = viewWithFrame(ccRect(5, 5, 30, 30));
        icon->backgroundColor = isDir ? color(1.0, 0.8, 0.2, 1.0) : color(0.4, 0.4, 1.0, 1.0);
        viewAddSubview(row, icon);
        
        CCLabel* lName = labelWithFrame(ccRect(45, 10, 170 - 40, 20));
        lName->text = copyCCString(nameStr); // Deep Copy!
        lName->fontSize = 16;
        lName->textColor = color(1.0, 1.0, 1.0, 1.0);
        viewAddSubview(row, lName);
        
        CCLabel* lDate = labelWithFrame(ccRect(170 + 10, 12, 90, 20));
        lDate->text = copyCCString(dateStr); // Deep Copy!
        lDate->fontSize = 12;
        lDate->textColor = color(0.7, 0.7, 0.7, 1.0);
        viewAddSubview(row, lDate);
        
        CCLabel* lSize = labelWithFrame(ccRect(170 + 90 + 10, 12, 60, 20));
        lSize->text = copyCCString(sizeStr); // Deep Copy!
        lSize->fontSize = 12;
        lSize->textColor = color(0.59, 0.59, 0.59, 1.0);
        viewAddSubview(row, lSize);
        
        // Add to Screen AND Tracking Array
        viewAddSubview(mainWindowView, row);
        arrayAddObject(uiDynamicPageViews, row);
        
        yPos += (ROW_HEIGHT + 2);
    }
    
    // Draw Pagination Controls
    int maxPages = (count + 5 - 1) / 5;
    if (maxPages > 1) {
        if (filesPageIndex > 0) {
            CCView* btnPrev = viewWithFrame(ccRect(10, 430, 80, 40));
            btnPrev->backgroundColor = color(0.0, 0.47, 1.0, 1.0);
            btnPrev->tag = TAG_FILES_PREV;
            
            CCLabel* l = labelWithFrame(ccRect(30, 10, 20, 20));
            l->text = copyCCString(ccs("<"));
            l->fontSize = 20;
            l->textColor = color(1.0, 1.0, 1.0, 1.0);
            viewAddSubview(btnPrev, l);
            
            viewAddSubview(mainWindowView, btnPrev);
            arrayAddObject(uiDynamicPageViews, btnPrev); // TRACK IT
        }
        
        // --- THE FIX: Wrap the label in a true CCView container ---
                CCView* pageContainer = viewWithFrame(ccRect(110, 440, 100, 20));
                pageContainer->backgroundColor = color(0.0, 0.0, 0.0, 0.0); // Transparent
                pageContainer->tag = 0; // No tag needed

                char pageStr[16];
                snprintf(pageStr, 16, "%d / %d", filesPageIndex + 1, maxPages);
                
                // Coordinates are now relative to the new pageContainer (0,0)
                CCLabel* lblPage = labelWithFrame(ccRect(0, 0, 100, 20));
                lblPage->text = copyCCString(ccs(pageStr));
                lblPage->textColor = color(1.0, 1.0, 1.0, 1.0);
                lblPage->textAlignment = CCTextAlignmentCenter; // Optional: keeps it looking nice
                
                viewAddSubview(pageContainer, lblPage); // Add label to container
                
                // Add the container to the screen AND the tracking array
                viewAddSubview(mainWindowView, pageContainer);
                arrayAddObject(uiDynamicPageViews, pageContainer);
        
        if (filesPageIndex < maxPages - 1) {
            CCView* btnNext = viewWithFrame(ccRect(230, 430, 80, 40));
            btnNext->backgroundColor = color(0.0, 0.47, 1.0, 1.0);
            btnNext->tag = TAG_FILES_NEXT;
            
            CCLabel* l = labelWithFrame(ccRect(30, 10, 20, 20));
            l->text = copyCCString(ccs(">"));
            l->fontSize = 20;
            l->textColor = color(1.0, 1.0, 1.0, 1.0);
            viewAddSubview(btnNext, l);
            
            viewAddSubview(mainWindowView, btnNext);
            arrayAddObject(uiDynamicPageViews, btnNext); // TRACK IT
        }
    }
}

// --- UI Construction ---

void setup_files_ui(void) {
    FreeOSLogI("FilesApp", "Starting setup_files_ui App");
    
    currentView = CurrentViewFiles;
    
    uiFilesContextMenu = NULL;
    uiFilesProperties = NULL;
    uiNewFolderDialog = NULL;
    uiNewFolderInput = NULL;
    
    if (uiDynamicPageViews) {
        freeElement(uiDynamicPageViews); // Just free the array container
        uiDynamicPageViews = NULL;       // Reset so drawFileTableView starts fresh
    }
    
    // --- 3. THE FIX: ONLY FREE IF IT IS A FILES APP WINDOW ---
        // If the window has our tag, it's safe to destroy (e.g., navigating folders).
        // If it does NOT have our tag, it's the Home Screen on the stack! Leave it alone!
        if (mainWindowView != NULL && mainWindowView->tag == TAG_FILES_APP_WINDOW) {
            freeViewHierarchy(mainWindowView);
            mainWindowView = NULL;
        }
        
        if (!currentFileList) {
            scan_current_directory();
        }
        
        // --- 4. BUILD THE NEW WINDOW & TAG IT ---
        // Only build a new base window if we don't already have one
        if (mainWindowView == NULL || mainWindowView->tag != TAG_FILES_APP_WINDOW) {
            mainWindowView = viewWithFrame(ccRect(0, 0, 320, 480));
            mainWindowView->backgroundColor = color(0.08, 0.08, 0.08, 1.0);
            mainWindowView->tag = TAG_FILES_APP_WINDOW; // Stamp it so we know it's ours!
        }
    
    CCLabel* lblPath = labelWithFrame(ccRect(10, 5, 300, 20));
    lblPath->text = copyCCString(ccs(is_root_mode() ? "My Devices" : currentPath));
    lblPath->fontSize = 14;
    lblPath->textColor = color(0.78, 0.78, 0.78, 1.0);
    viewAddSubview(mainWindowView, lblPath); // NO CAST
    
    int headerY = 30;
    uiFilesHeader = viewWithFrame(ccRect(0, headerY, 320, 25));
    uiFilesHeader->backgroundColor = color(0.24, 0.24, 0.24, 1.0);
    
#define COL_NAME_WIDTH 170
#define COL_DATE_WIDTH 90
#define COL_SIZE_WIDTH 60
    
    CCLabel* hName = labelWithFrame(ccRect(10, 5, COL_NAME_WIDTH, 15));
    hName->text = copyCCString(ccs("Name"));
    hName->fontSize = 12;
    hName->textColor = (fileSortMode == SORT_MODE_NAME) ? color(1.0, 1.0, 0.0, 1.0) : color(0.78, 0.78, 0.78, 1.0);
    viewAddSubview(uiFilesHeader, hName); // NO CAST
    
    CCLabel* hDate = labelWithFrame(ccRect(COL_NAME_WIDTH + 10, 5, COL_DATE_WIDTH, 15));
    hDate->text = copyCCString(ccs("Date"));
    hDate->fontSize = 12;
    hDate->textColor = (fileSortMode == SORT_MODE_DATE) ? color(1.0, 1.0, 0.0, 1.0) : color(0.78, 0.78, 0.78, 1.0);
    viewAddSubview(uiFilesHeader, hDate); // NO CAST
    
    CCLabel* hSize = labelWithFrame(ccRect(COL_NAME_WIDTH + COL_DATE_WIDTH + 10, 5, COL_SIZE_WIDTH, 15));
    hSize->text = copyCCString(ccs("Size"));
    hSize->fontSize = 12;
    hSize->textColor = (fileSortMode == SORT_MODE_SIZE) ? color(1.0, 1.0, 0.0, 1.0) : color(0.78, 0.78, 0.78, 1.0);
    viewAddSubview(uiFilesHeader, hSize); // NO CAST
    
    viewAddSubview(mainWindowView, uiFilesHeader);
    
    drawFileTableView();
    
    if (!is_root_mode()) {
        CCView* btnUp = viewWithFrame(ccRect(260, 0, 60, 30));
        btnUp->backgroundColor = color(0.31, 0.31, 0.31, 1.0);
        btnUp->tag = TAG_FILES_UP_DIR;
        
        CCLabel* l = labelWithFrame(ccRect(20, 5, 20, 20));
        l->text = copyCCString(ccs("UP"));
        l->fontSize = 14;
        l->textColor = color(1.0, 1.0, 1.0, 1.0);
        
        viewAddSubview(btnUp, l); // NO CAST
        viewAddSubview(mainWindowView, btnUp);
    }
    
    if (uiFilesContextMenu) {
        viewAddSubview(mainWindowView, uiFilesContextMenu);
    }
    if (uiFilesProperties) {
        viewAddSubview(mainWindowView, uiFilesProperties);
    }
}
// --- Navigation & Touch Handling ---

void files_open_file(const char* name) {
    if (!name) return; // Safety check
    
    if (is_root_mode()) {
        if (strcmp(name, "Internal Storage") == 0) strcpy(currentPath, "/spiflash");
        else if (strcmp(name, "SD Card") == 0) strcpy(currentPath, "/sdcard");
        else return;
        
        filesPageIndex = 0;
        scan_current_directory();
        setup_files_ui();
        update_full_ui(); // Force refresh to show new directory
        return;
    }
    
    char fullPath[512];
    snprintf(fullPath, 512, "%s/%s", currentPath, name);
    
    struct stat path_stat;
    if (stat(fullPath, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        strcpy(currentPath, fullPath);
        filesPageIndex = 0;
        scan_current_directory();
        setup_files_ui();
        update_full_ui(); // Force refresh to show new directory
    } else {
        FreeOSLogI("FilesApp", "Opening file: %s", fullPath);
        // Put file opening logic here (e.g., text editor/image viewer)
    }
}

// --- Properties Viewer ---

void show_properties_view(const char* filename) {
    if (uiFilesProperties) uiFilesProperties = NULL;
    
    uiFilesProperties = viewWithFrame(ccRect(20, 60, 280, 360));
    uiFilesProperties->backgroundColor = color(0.18, 0.18, 0.18, 1.0); // Slightly lighter than context menu
    
    // Header
    CCLabel* title = labelWithFrame(ccRect(10, 10, 260, 20));
    title->text = copyCCString(ccs("Properties"));
    title->fontSize = 18;
    title->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(uiFilesProperties, title);
    
    char infoBuffer[512] = {0};
    
    // Drive Logic vs File Logic
    if (is_root_mode() && (strcmp(filename, "Internal Storage") == 0 || strcmp(filename, "SD Card") == 0)) {
        const char* drivePath = (strcmp(filename, "Internal Storage") == 0) ? "/spiflash" : "/sdcard";
        uint64_t total_bytes = 0, free_bytes = 0;
        
        esp_vfs_fat_info(drivePath, &total_bytes, &free_bytes);
        
        char totalStr[32], freeStr[32];
        format_file_size(total_bytes, totalStr, 32);
        format_file_size(free_bytes, freeStr, 32);
        
        snprintf(infoBuffer, sizeof(infoBuffer),
                 "Name: %s\nType: Drive\nPath: %s\n\nTotal Space: %s\nFree Space: %s",
                 filename, drivePath, totalStr, freeStr);
    }
    else {
        // Standard File/Folder Logic
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", strcmp(currentPath, "/") == 0 ? "" : currentPath, filename);
        
        struct stat st;
        if (stat(fullPath, &st) == 0) {
            char sizeStr[32];
            format_file_size(st.st_size, sizeStr, 32);
            
            struct tm *tm_mod = localtime(&st.st_mtime);
            char modBuf[64];
            strftime(modBuf, sizeof(modBuf), "%b %d, %Y %H:%M", tm_mod);
            
            const char* typeStr = S_ISDIR(st.st_mode) ? "Folder" : (is_archive_file(filename) ? "Archive" : "File");
            
            snprintf(infoBuffer, sizeof(infoBuffer),
                     "Name: %s\nType: %s\nSize: %s\n\nModified:\n%s",
                     filename, typeStr, S_ISDIR(st.st_mode) ? "--" : sizeStr, modBuf);
        } else {
            strcpy(infoBuffer, "Error: Could not read file stat.");
        }
    }
    
    // Body Text
    CCLabel* body = labelWithFrame(ccRect(15, 50, 250, 240));
    body->text = copyCCString(ccs(infoBuffer));
    body->fontSize = 14;
    body->textColor = color(0.8, 0.8, 0.8, 1.0);
    body->textVerticalAlignment = CCTextVerticalAlignmentTop;
    viewAddSubview(uiFilesProperties, body);
    
    // Close Button
    CCView* btnClose = viewWithFrame(ccRect(90, 310, 100, 35));
    btnClose->backgroundColor = color(0.0, 0.47, 1.0, 1.0); // Blue
    btnClose->tag = TAG_CTX_PROPS_CLOSE;
    
    CCLabel* lblClose = labelWithFrame(ccRect(0, 8, 100, 20));
    lblClose->text = copyCCString(ccs("Close"));
    lblClose->fontSize = 16;
    lblClose->textAlignment = CCTextAlignmentCenter;
    lblClose->textColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(btnClose, lblClose);
    
    viewAddSubview(uiFilesProperties, btnClose);
    viewAddSubview(mainWindowView, uiFilesProperties);
}

void paste_clipboard_file(void) {
    if (strlen(clipboardPath) == 0) return;
    if (is_root_mode()) return;
    
    // 1. Extract the original filename
    const char* original_filename = strrchr(clipboardPath, '/');
    if (!original_filename) original_filename = clipboardPath;
    else original_filename++; // Skip the '/'
    
    // 2. Separate basename and extension
    char basename[128] = {0};
    char ext[32] = {0};
    const char* dot = strrchr(original_filename, '.');
    
    if (dot && dot != original_filename) {
        strncpy(basename, original_filename, dot - original_filename);
        strcpy(ext, dot); // Includes the '.'
    } else {
        strcpy(basename, original_filename); // No extension
    }
    
    // 3. Find an available filename
    char destPath[512];
    char newFilename[128];
    strcpy(newFilename, original_filename);
    snprintf(destPath, sizeof(destPath), "%s/%s", currentPath, newFilename);
    
    struct stat st;
    int copyNum = 0;
    
    // Loop until we find a filename that DOES NOT exist
    while (stat(destPath, &st) == 0) {
        if (copyNum == 0) {
            snprintf(newFilename, sizeof(newFilename), "%s copy%s", basename, ext);
        } else {
            snprintf(newFilename, sizeof(newFilename), "%s copy %d%s", basename, copyNum, ext);
        }
        snprintf(destPath, sizeof(destPath), "%s/%s", currentPath, newFilename);
        copyNum++;
    }
    
    // 4. Perform the byte-by-byte copy
    FILE* src = fopen(clipboardPath, "rb");
    if (!src) return;
    
    FILE* dst = fopen(destPath, "wb");
    if (!dst) { fclose(src); return; }
    
    size_t bufferSize = 4096;
    char* buffer = malloc(bufferSize);
    if (buffer) {
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, bufferSize, src)) > 0) {
            fwrite(buffer, 1, bytesRead, dst);
        }
        free(buffer);
    }
    
    fclose(src);
    fclose(dst);
}


void handle_files_touch(int x, int y, int touchState) {
    
    // ==========================================
    // FINGER IS TOUCHING THE SCREEN (DOWN / HELD)
    // ==========================================
    if (touchState == 1) {
        if (!is_pressing) {
            // First frame of touch down
            is_pressing = true;
            long_press_fired = false; // Reset the flag for this new tap
            touchStartTime = esp_timer_get_time() / 1000;
            touchStartX = x;
            touchStartY = y;
        } else {
            // Finger is held down or dragging
            if (abs(x - touchStartX) > 15 || abs(y - touchStartY) > 15) {
                touchStartTime = 0; // Dragged too far, cancel the tap
            }
            
            // --- REAL-TIME LONG PRESS CHECK ---
            if (touchStartTime != 0 && !long_press_fired && !uiFilesContextMenu && !uiNewFolderDialog && !uiFilesProperties) {
                uint64_t now = esp_timer_get_time() / 1000;
                
                if ((now - touchStartTime) >= 1000) { // EXACTLY 1 SECOND
                    long_press_fired = true;
                    
                    CCView* target = find_subview_at_point(mainWindowView, touchStartX, touchStartY);
                    
                    // 1. Did they long-press a file/folder row?
                    if (target && target->tag >= TAG_FILES_ITEM_BASE) {
                        int idx = target->tag - TAG_FILES_ITEM_BASE;
                        CCDictionary* item = (CCDictionary*)arrayObjectAtIndex(currentFileList, idx);
                        CCString* name = (CCString*)dictionaryObjectForKeyFreeKey(item, ccs("name"));
                        
                        if (name && name->string) {
                            strcpy(selectedFilename, name->string);
                            
                            // --- CORRECT ORDER ---
                            //setup_files_ui();                  // 1. Wipe and rebuild base UI
                            show_context_menu(name->string);   // 2. Spawn menu ON TOP safely
                            update_full_ui();                  // 3. Draw
                        }
                    }
                    // 2. Did they long-press the background/empty space?
                    else if (!is_root_mode()) {
                        selectedFilename[0] = '\0';
                        
                        // --- CORRECT ORDER ---
                        //setup_files_ui();                  // 1. Wipe and rebuild base UI
                        show_directory_context_menu();     // 2. Spawn directory menu ON TOP safely
                        update_full_ui();                  // 3. Draw
                    }
                }
            }
        }
        
        return;
    }
    
    // ==========================================
    // FINGER LIFTED (RELEASED)
    // ==========================================
    if (touchState == 0) {
        if (!is_pressing) return; // Ignore if we are already released
        is_pressing = false;      // Reset state for next tap
        
        // If they scrolled, OR if the 2-second long press already fired, DO NOTHING on release!
        if (touchStartTime == 0 || long_press_fired) {
            long_press_fired = false; // Clean up
            return;
        }
        
        // --- If we reach here, it was a valid SHORT TAP! ---
        
        int tapX = touchStartX;
        int tapY = touchStartY;
        
        // 0. Handle Properties Window Taps (Highest Z-Index)
        if (uiFilesProperties) {
            CCView* target = find_subview_at_point(uiFilesProperties, tapX, tapY);
            if (target && target->tag == TAG_CTX_PROPS_CLOSE) {
                // --- YOUR ARCHITECTURAL FIX ---
                // 1. Detach from the main window so it stops rendering
                viewRemoveFromSuperview(uiFilesProperties);
                
                // 2. Safely destroy the isolated menu
                freeViewHierarchy(uiFilesProperties);
                uiFilesProperties = NULL;
                
                // 3. Just push the graphics update to erase it from the screen!
                update_full_ui();
            }
            return; // Eat the touch so it doesn't click items behind the window
        }
        
        // 1. Handle Context Menu Taps
        if (uiFilesContextMenu) {
            CCView* target = find_subview_at_point(uiFilesContextMenu, tapX, tapY);
            if (target) {
                int tag = target->tag;
                
                viewRemoveFromSuperview(uiFilesContextMenu);
                freeViewHierarchy(uiFilesContextMenu);
                uiFilesContextMenu = NULL;
                
                if (tag == TAG_CTX_CANCEL) {
                    
                }
                else if (tag == TAG_CTX_OPEN) {
                    files_open_file(selectedFilename);
                }
                else if (tag == TAG_CTX_COPY) {
                    // Save to clipboard
                    snprintf(clipboardPath, sizeof(clipboardPath), "%s/%s", strcmp(currentPath, "/") == 0 ? "" : currentPath, selectedFilename);
                    FreeOSLogI("FilesApp", "Copied to clipboard: %s", clipboardPath);
                }
                else if (tag == TAG_CTX_PASTE) {
                    paste_clipboard_file();
                    scan_current_directory();
                    //setup_files_ui(); // Rebuild one more time to show the pasted file
                }
                else if (tag == TAG_CTX_PROPS) {
                    // 2. Spawn Properties window on top
                    if (strlen(selectedFilename) > 0) {
                        show_properties_view(selectedFilename);
                    } else {
                        char* folderName = strrchr(currentPath, '/');
                        if (folderName && strlen(folderName) > 1) show_properties_view(folderName + 1);
                        else show_properties_view("SD Card");
                    }
                }
                else if (tag == TAG_CTX_COMPRESS) {
                    FreeOSLogI("FilesApp", "Compressing %s...", selectedFilename);
                    uiFilesContextMenu = NULL;
                }
                else if (tag == TAG_CTX_DECOMPRESS) {
                    FreeOSLogI("FilesApp", "Decompressing archive %s...", selectedFilename);
                    uiFilesContextMenu = NULL;
                }
                else if (tag == TAG_CTX_NEW) {
                    uiFilesContextMenu = NULL;
                    show_new_folder_dialog();
                }
                
                update_full_ui();
            }
            return; // Eat the touch
        }
        
        // 0. Handle New Folder Dialog Taps (Highest Z-Index)
        if (uiNewFolderDialog) {
            CCView* target = find_subview_at_point(uiNewFolderDialog, tapX, tapY);
            if (target) {
                if (target->tag == TAG_NEW_FOLDER_CANCEL) {
                    uiNewFolderDialog = NULL;
                    uiNewFolderInput = NULL;
                    setup_files_ui(); // Rebuild UI (which should clear keyboard)
                    update_full_ui();
                }
                else if (target->tag == TAG_NEW_FOLDER_CREATE) {
                    if (uiNewFolderInput && uiNewFolderInput->text && uiNewFolderInput->text->string) {
                        if (!is_root_mode()) {
                            char newDirPath[512];
                            snprintf(newDirPath, sizeof(newDirPath), "%s/%s", currentPath, uiNewFolderInput->text->string);
                            
                            FreeOSLogI("FilesApp", "Creating folder: %s", newDirPath);
                            
                            // 0777 grants read/write/execute permissions
                            if (mkdir(newDirPath, 0777) == 0) {
                                FreeOSLogI("FilesApp", "Folder created successfully.");
                            } else {
                                FreeOSLogE("FilesApp", "Failed to create folder.");
                            }
                        } else {
                            FreeOSLogE("FilesApp", "Cannot create folder in virtual root.");
                        }
                    }
                    
                    uiNewFolderDialog = NULL;
                    uiNewFolderInput = NULL;
                    filesPageIndex = 0; // Reset to page 1 to see the new folder
                    scan_current_directory(); // Rescan drive
                    setup_files_ui(); // Rebuild UI
                    update_full_ui();
                }
            }
            return; // Eat the touch so we don't click anything else
        }
        
        // 2. Handle Header Sort Taps
        if (tapY > 30 && tapY < 55) {
            if (tapX < 170) {
                if (fileSortMode == SORT_MODE_NAME) fileSortAscending = !fileSortAscending;
                else { fileSortMode = SORT_MODE_NAME; fileSortAscending = true; }
            } else if (tapX < 260) {
                if (fileSortMode == SORT_MODE_DATE) fileSortAscending = !fileSortAscending;
                else { fileSortMode = SORT_MODE_DATE; fileSortAscending = false; }
            } else {
                if (fileSortMode == SORT_MODE_SIZE) fileSortAscending = !fileSortAscending;
                else { fileSortMode = SORT_MODE_SIZE; fileSortAscending = false; }
            }
            sort_file_list();
            setup_files_ui();
            update_full_ui();
            return;
        }
        
        // 3. Find what row was tapped
        CCView* target = find_subview_at_point(mainWindowView, tapX, tapY);
        if (!target) return;
        
        // Handle Back/Up Navigation
        if (target->tag == TAG_FILES_UP_DIR) {
            if (strcmp(currentPath, "/fat") == 0 || strcmp(currentPath, "/sdcard") == 0) {
                strcpy(currentPath, "/");
            } else {
                char* lastSlash = strrchr(currentPath, '/');
                if (lastSlash && lastSlash != currentPath) *lastSlash = '\0';
            }
            filesPageIndex = 0;
            
            // 1. WIPE THE UI FIRST so the graphics engine stops trying to read the old text
            if (mainWindowView) {
                freeViewHierarchy(mainWindowView);
                mainWindowView = NULL;
            }
            
            // 2. FREE OLD LIST & SCAN NEW DIRECTORY safely
            if (currentFileList) {
                freeElement(currentFileList);
                currentFileList = NULL;
            }
            scan_current_directory();
            
            // 3. REBUILD THE CLEAN UI
            setup_files_ui();
            update_full_ui();
            return;
        }
        
        // Handle Pagination
        if (target->tag == TAG_FILES_PREV) {
            if (filesPageIndex > 0) {
                filesPageIndex--;
                FreeOSLogI("FilesApp", "filesPageIndex - %d", filesPageIndex);
                
                // Targeted redraw instead of full UI rebuild!
                drawFileTableView();
                update_full_ui();
            }
            return;
        }
        if (target->tag == TAG_FILES_NEXT) {
            filesPageIndex++;
            FreeOSLogI("FilesApp", "filesPageIndex + %d", filesPageIndex);
            
            // Targeted redraw instead of full UI rebuild!
            drawFileTableView();
            update_full_ui();
            return;
        }
        
        // 4. Handle File/Folder Row Taps (Short Tap Only)
        if (target->tag >= TAG_FILES_ITEM_BASE) {
            int idx = target->tag - TAG_FILES_ITEM_BASE;
            CCDictionary* item = (CCDictionary*)arrayObjectAtIndex(currentFileList, idx);
            CCString* name = (CCString*)dictionaryObjectForKeyFreeKey(item, ccs("name"));
            
            if (!name || !name->string) return;
            
            files_open_file(name->string);
        }
    }
}

void freeFilesView(void) {
    if (currentFileList != NULL) {
        freeElement(currentFileList);
        currentFileList = NULL;  // <--- CRITICAL FIX
        filesPageIndex = 0;      // Reset pagination for the next time it opens
    }
}
