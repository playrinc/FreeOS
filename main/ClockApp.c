//
//  ClockApp.c
//  
//
//  Created by Chris Galzerano on 2/19/26.
//

#include "ClockApp.h"

// --- CLOCK APP GLOBALS ---
ClockTab currentClockTab = TAB_CLOCK_FACE;
bool clock_app_running = false;
TaskHandle_t clock_anim_task_handle = NULL;

CCView* clockMainContainer = NULL;
CCView* tabContentView = NULL;
CCView* tabBar = NULL;

// --- WORLD CLOCK DATA ---
TimeZone zones[] = {
    {"New York", -5}, {"London", 0}, {"Paris", 1},
    {"Tokyo", 9}, {"Sydney", 11}, {"Los Angeles", -8},
    {"Dubai", 4}, {"Hong Kong", 8}
};
int zonesCount = 8;

// --- WORLD CLOCK PAGINATION ---
int worldClockPage = 0;
const int CLOCK_ITEMS_PER_PAGE = 3;

// --- WORLD CLOCK GLOBALS ---
CCView* mapContainerView = NULL;
CCImageView* worldMapImageView = NULL;
CCView* dayNightOverlayView = NULL;

// --- ALARM DATA ---
int alarmHour = 7;
int alarmMinute = 30;
bool alarmEnabled = false;

// --- TIMER DATA ---
int timerSecondsTotal = 0;
int timerSecondsLeft = 0;
bool timerRunning = false;
uint64_t lastTimerTick = 0;

// --- Clock Touch State Globals ---
static uint64_t clockTouchStartTime = 0;
static int clockTouchStartX = 0;
static int clockTouchStartY = 0;
static bool clock_is_pressing = false;
static bool clock_long_press_fired = false;

CCPoint* getHandTip(int cx, int cy, float radius, float angleRad) {
    // -PI/2 rotates the "0" angle from 3 o'clock to 12 o'clock
    float adj = angleRad - M_PI_2;
    return ccPoint(cx + cosf(adj)*radius, cy + sinf(adj)*radius);
}



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
    QueueSend(g_graphics_queue, &cmd_save, QUEUE_MAX_DELAY);
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
                    // QueueSend(ui_event_queue, &UPDATE_MSG, 0);
                }
                
                if (timerSecondsLeft == 0) {
                    timerRunning = false;
                    // TODO: Play Sound!
                    FreeOSLogI(TAG, "TIMER FINISHED!");
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
            QueueSend(g_graphics_queue, &cmd_restore, 0);

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
            QueueSend(g_graphics_queue, &cmd_h, 0);

            // Minute Hand (Long, Medium)
            CCPoint* minTip = getHandTip(CLOCK_CENTER_X, CLOCK_CENTER_Y, 80, minAng);
            GraphicsCommand cmd_m = {
                .cmd = CMD_DRAW_ROUNDED_HAND,
                .x = CLOCK_CENTER_X, .y = CLOCK_CENTER_Y,
                .w = (int)minTip->x, .h = (int)minTip->y,
                .radius = 4, // 4px Thick
                .color = {255, 255, 255, 255} // White
            };
            QueueSend(g_graphics_queue, &cmd_m, 0);

            // Second Hand (Long, Thin, Red)
            CCPoint* secTip = getHandTip(CLOCK_CENTER_X, CLOCK_CENTER_Y, 90, secAng);
            GraphicsCommand cmd_s = {
                .cmd = CMD_DRAW_ROUNDED_HAND,
                .x = CLOCK_CENTER_X, .y = CLOCK_CENTER_Y,
                .w = (int)secTip->x, .h = (int)secTip->y,
                .radius = 2, // 2px Thick
                .color = {0, 0, 255, 255} // Red (BGR format: Blue is 0, Red is 255)
            };
            QueueSend(g_graphics_queue, &cmd_s, 0);

            // Center Dot (Cover the joints)
            // (Optional: send a CMD_DRAW_CIRCLE_FILLED here if you have one exposed)

            // E. Flush Screen
            GraphicsCommand cmd_flush = {
                .cmd = CMD_UPDATE_AREA,
                .x = dirty_x, .y = dirty_y,
                .w = dirty_w, .h = dirty_h
            };
            QueueSend(g_graphics_queue, &cmd_flush, 0);
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
    FreeOSLogI(TAG, "Starting Clock App");
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
    QueueSend(g_graphics_queue, &cmd_overlay, 0);
    
    // Force a specific area update for the map just in case
    GraphicsCommand cmd_flush = {
        .cmd = CMD_UPDATE_AREA,
        .x = MAP_X, .y = MAP_Y, .w = MAP_W, .h = MAP_H
    };
    QueueSend(g_graphics_queue, &cmd_flush, 0);
    
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



void draw_alarm_ui(void) {
    
    CCLabel* lblTime = labelWithFrame(ccRect(0, 80, 320, 80));
    lblTime->text = ccs("test");
    lblTime->fontSize = 60;
    lblTime->textAlignment = CCTextAlignmentCenter;
    lblTime->textColor = alarmEnabled ? color(1,1,1,1) : color(0.5,0.5,0.5,1);
    viewAddSubview(tabContentView, lblTime);
    
    // 1. Alarm Time Display (Big Center Text)
    char buf[16];
    formatTimeStr(buf, alarmHour, alarmMinute, false);
    

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
    viewAddSubview(tabContentView, btnToggle);
}



void draw_timer_ui(void) {
    FreeOSLogI("draw_timer_ui", "Starting draw_timer_ui App");
    
    // 1. Countdown Display
    int m = timerSecondsLeft / 60;
    int s = timerSecondsLeft % 60;
    char buf[16];
    sprintf(buf, "%02d:%02d", m, s);
    
    FreeOSLogI("draw_timer_ui", "1Starting draw_timer_ui App");

    CCLabel* lblTimer = labelWithFrame(ccRect(0, 80, 320, 80));
    lblTimer->text = ccs(buf);
    lblTimer->fontSize = 24;
    lblTimer->textAlignment = CCTextAlignmentCenter;
    lblTimer->textColor = timerRunning ? color(1, 0.8, 0, 1) : color(1,1,1,1);
    viewAddSubview(tabContentView, lblTimer);
    
    FreeOSLogI("draw_timer_ui", "2Starting draw_timer_ui App");

    // 2. Add Time Button (+1 Min)
    CCView* btnAdd = viewWithFrame(ccRect(110, 180, 100, 40));
    btnAdd->backgroundColor = color(0.2, 0.2, 0.2, 1);
    btnAdd->tag = TAG_TIMER_ADD_1MIN;
    CCLabel* lAdd = labelWithFrame(ccRect(0,0,100,40));
    lAdd->text = ccs("+1 Min"); lAdd->textAlignment=CCTextAlignmentCenter; lAdd->textVerticalAlignment=CCTextVerticalAlignmentCenter;
    lAdd->textColor = color(1,1,1,1);
    viewAddSubview(btnAdd, lAdd);
    viewAddSubview(tabContentView, btnAdd);
    
    FreeOSLogI("draw_timer_ui", "3Starting draw_timer_ui App");

    // 3. Start/Stop Button
    CCView* btnStart = viewWithFrame(ccRect(40, 260, 100, 60));
    btnStart->backgroundColor = timerRunning ? color(0.6, 0.2, 0, 1) : color(0, 0.6, 0, 1);
    btnStart->tag = TAG_TIMER_START;
    CCLabel* lStart = labelWithFrame(ccRect(0,0,100,60));
    lStart->text = ccs(timerRunning ? "Stop" : "Start"); lStart->textAlignment=CCTextAlignmentCenter; lStart->textVerticalAlignment=CCTextVerticalAlignmentCenter;
    lStart->textColor = color(1,1,1,1);
    viewAddSubview(btnStart, lStart);
    viewAddSubview(tabContentView, btnStart);
    
    FreeOSLogI("draw_timer_ui", "4Starting draw_timer_ui App");

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
    
    // ==========================================
    // FINGER IS TOUCHING THE SCREEN (DOWN / HELD)
    // ==========================================
    if (type == 1) {
        if (!clock_is_pressing) {
            // First frame of touch down
            clock_is_pressing = true;
            clock_long_press_fired = false;
            clockTouchStartTime = esp_timer_get_time() / 1000;
            clockTouchStartX = x;
            clockTouchStartY = y;
        } else {
            // Finger is held down or dragging
            if (abs(x - clockTouchStartX) > 15 || abs(y - clockTouchStartY) > 15) {
                clockTouchStartTime = 0; // Dragged too far, cancel the tap
            }
            
            // --- REAL-TIME LONG PRESS CHECK ---
            if (clockTouchStartTime != 0 && !clock_long_press_fired) {
                uint64_t now = esp_timer_get_time() / 1000;
                
                if ((now - clockTouchStartTime) >= 1000) { // 1 SECOND LONG PRESS
                    clock_long_press_fired = true;
                    
                    // FUTURE HOOK: Add Long Press actions here!
                    // e.g., Deleting an alarm, resetting a specific world clock, etc.
                    FreeOSLogI("ClockApp", "Long press detected at %d, %d", clockTouchStartX, clockTouchStartY);
                }
            }
        }
        return;
    }

    // ==========================================
    // FINGER LIFTED (RELEASED) - SHORT TAP
    // ==========================================
    if (type == 0) {
        if (!clock_is_pressing) return; // Ignore if we are already released
        clock_is_pressing = false;      // Reset state for next tap
        
        // If they scrolled or the long press already fired, DO NOTHING on release!
        if (clockTouchStartTime == 0 || clock_long_press_fired) {
            clock_long_press_fired = false;
            return;
        }

        // --- If we reach here, it was a valid SHORT TAP! ---
        
        // CRITICAL: Use the saved X and Y from when the finger was actually down!
        int tapX = clockTouchStartX;
        int tapY = clockTouchStartY;

        // 1. Tab Bar Logic
        if (tapY > 420) {
            int tabWidth = 320 / 4;
            int index = tapX / tabWidth;
            if (index >= 0 && index <= 3 && index != currentClockTab) {
                printf("switch_clock_tab %d\n", index);
                switch_clock_tab((ClockTab)index);
                update_full_ui(); // Push changes to screen
            }
            return;
        }

        // 2. Tab Specific Logic
        // Helper to find clicked view by tag
        CCView* clicked = find_subview_at_point(tabContentView, tapX, tapY);
                if (!clicked) return;

                // --- THE FIX: Prevent Labels from Stealing Touches ---
                // If we tapped the text inside the button, 'clicked' is the Label (tag 0).
                // Grab the tag from the Label's parent (the Button) instead!
        // --- THE FIX: Prevent Labels from Stealing Touches ---
                int tag = clicked->tag;
                
                if (tag == 0 && clicked->superview) {
                    // Cast the raw superview pointer to your known CCView typedef
                    // to bypass the compiler's "undefined struct" quirk!
                    CCView* parentView = (CCView*)clicked->superview;
                    tag = parentView->tag;
                }

                if (currentClockTab == TAB_ALARM) {
                    bool ui_changed = false; // Track if we actually hit a valid button
                    
                    if (tag == TAG_ALARM_H_UP) { alarmHour = (alarmHour + 1) % 24; ui_changed = true; }
                    else if (tag == TAG_ALARM_H_DOWN) { alarmHour = (alarmHour - 1 + 24) % 24; ui_changed = true; }
                    else if (tag == TAG_ALARM_M_UP) { alarmMinute = (alarmMinute + 5) % 60; ui_changed = true; }
                    else if (tag == TAG_ALARM_M_DOWN) { alarmMinute = (alarmMinute - 5 + 60) % 60; ui_changed = true; }
                    else if (tag == TAG_ALARM_TOGGLE) { alarmEnabled = !alarmEnabled; ui_changed = true; }
                    
                    // --- THE FIX: Only rebuild if a valid button was pressed ---
                    if (ui_changed) {
                        switch_clock_tab(TAB_ALARM);
                        update_full_ui(); // Push to screen!
                    }
                }
                else if (currentClockTab == TAB_TIMER) {
                    if (tag == TAG_TIMER_ADD_1MIN) {
                        timerSecondsLeft += 60;
                        switch_clock_tab(TAB_TIMER);
                        update_full_ui();
                    }
                    else if (tag == TAG_TIMER_RESET) {
                        timerRunning = false;
                        timerSecondsLeft = 0;
                        switch_clock_tab(TAB_TIMER);
                        update_full_ui();
                    }
                    else if (tag == TAG_TIMER_START) {
                        timerRunning = !timerRunning;
                        if (timerRunning) lastTimerTick = esp_timer_get_time() / 1000;
                        switch_clock_tab(TAB_TIMER);
                        update_full_ui();
                    }
                }
                else if (currentClockTab == TAB_WORLD_CLOCK) {
                    if (tag == TAG_CLOCK_PREV) {
                        if (worldClockPage > 0) {
                            worldClockPage--;
                            switch_clock_tab(TAB_WORLD_CLOCK);
                            update_full_ui();
                        }
                    }
                    else if (tag == TAG_CLOCK_NEXT) {
                        if ((worldClockPage + 1) * CLOCK_ITEMS_PER_PAGE < zonesCount) {
                            worldClockPage++;
                            switch_clock_tab(TAB_WORLD_CLOCK);
                            update_full_ui();
                        }
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
    else if (tab == TAB_CLOCK_FACE) {
        draw_analog_face_ui();
        if (clock_anim_task_handle) vTaskDelete(clock_anim_task_handle);
        xTaskCreatePinnedToCore(clock_animation_task, "clock_anim", 4096, NULL, 5, &clock_anim_task_handle, 1);
    }
    else if (tab == TAB_ALARM) draw_alarm_ui();
    else if (tab == TAB_TIMER) draw_timer_ui();
    
    // Trigger update
    update_full_ui();
}
