//
//  SystemApps.c
//  
//
//  Created by Chris Galzerano on 2/9/26.
//

#include "SystemApps.h"

#include "FilesApp.h"
#include "ClockApp.h"
#include "PaintApp.h"

// --- Settings Globals ---
// Removed 'static', added definitions
int settingsPageIndex = 0;
const int SETTINGS_PER_PAGE = 6;
CCView* uiSettingsContainer = NULL;
CCView* buttonsContainer = NULL;

// --- Calculator State ---
CCString* calcDisplayStr = NULL;
double calcStoredValue = 0.0;
char calcCurrentOp = 0;
bool calcIsNewEntry = true;
CCLabel* uiCalcLabel = NULL;

// --- Music Player Globals ---
CCLabel* uiMusicTitleLbl = NULL;
CCLabel* uiMusicArtistLbl = NULL;
CCView* uiMusicProgressFill = NULL;
CCLabel* uiMusicPlayBtnLbl = NULL;

// --- Gallery Globals ---
CCArray* galleryImagePaths = NULL;
int galleryCurrentPage = 0;
int gallerySelectedIdx = -1;
float galleryZoomScale = 1.0f;

// --- WiFi Layout Globals ---
CCView* uiWifiContainer = NULL;
CCView* uiWifiListContainer = NULL;
CCView* uiWifiToggleBtn = NULL;
bool isWifiEnabled = true;

// --- WiFi Data Definitions ---
WifiNetwork g_wifi_scan_results[MAX_WIFI_RESULTS]; // Actual memory allocation
int g_wifi_scan_count = 0;
bool wifi_initialized = false;
int g_wifi_page_index = 0;


/*
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
*/



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






void openHomeMenuItem(int tag) {
    if (tag == 0) {
        printf("Opening Files App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        setup_files_ui();
        update_full_ui();
        
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
    else if (tag == 2) {
        printf("Opening Text App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        setup_text_ui();
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
        QueueSend(g_graphics_queue, &cmd_clear, 0);

        // 3. Start the Video Task
        // Stack size 8192 (8KB) is recommended for JPEG decoding
        xTaskCreatePinnedToCore(video_player_task, "video_task", 16384, NULL, 5, NULL, 1);*/
        
        printf("Opening Paint App...\n");
        openedApp = true;
        
        // A. Save the current Home Menu view
        if (mainWindowView != NULL) {
            push_view(mainWindowView);
        }
        
        currentView = CurrentViewPaint;
        
        
        //if (mainWindowView) freeViewHierarchy(mainWindowView);
        //mainWindowView = viewWithFrame(ccRect(0, 0, 320, 480));
        //mainWindowView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
        
        //setup_video_preview_ui();
        
        //start_video_player(&fb);
        
        //load_video_poster();
        
        //setup_video_preview_ui1();
        
        setup_paint_ui();
        
        update_full_ui();
        
        //xTaskCreatePinnedToCore(next_frame_task, "anim_task", 16384, (void*)videoView, 5, NULL, 1);
        
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
