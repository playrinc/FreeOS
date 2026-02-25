//
//  ClockApp.h
//  
//
//  Created by Chris Galzerano on 2/19/26.
//

#ifndef ClockApp_h
#define ClockApp_h

#include <stdio.h>
#include <main.h>

// --- CLOCK APP GLOBALS ---
typedef enum {
    TAB_WORLD_CLOCK = 0,
    TAB_CLOCK_FACE,
    TAB_ALARM,
    TAB_TIMER
} ClockTab;

extern ClockTab currentClockTab;
extern bool clock_app_running;
extern TaskHandle_t clock_anim_task_handle;

// UI Pointers
extern CCView* clockMainContainer;
extern CCView* tabContentView;
extern CCView* tabBar;

// Layout Constants
#define CLOCK_CENTER_X 160
#define CLOCK_CENTER_Y 210
#define CLOCK_RADIUS   100

// --- WORLD CLOCK DATA ---
typedef struct {
    char* city;
    int offset;
} TimeZone;

extern TimeZone zones[]; // Declaration of array
extern int zonesCount;

// --- WORLD CLOCK PAGINATION ---
extern int worldClockPage;
extern const int CLOCK_ITEMS_PER_PAGE;
#define TAG_CLOCK_PREV 701
#define TAG_CLOCK_NEXT 702

// --- WORLD CLOCK GLOBALS ---
extern CCView* mapContainerView;
extern CCImageView* worldMapImageView;
extern CCView* dayNightOverlayView;

// Constants for map size/position
#define MAP_X 0
#define MAP_Y 20
#define MAP_W 320
#define MAP_H 120

// --- ALARM DATA ---
extern int alarmHour;
extern int alarmMinute;
extern bool alarmEnabled;

// --- TIMER DATA ---
extern int timerSecondsTotal;
extern int timerSecondsLeft;
extern bool timerRunning;
extern uint64_t lastTimerTick;

// Tags for touch handling
#define TAG_ALARM_H_UP   501
#define TAG_ALARM_H_DOWN 502
#define TAG_ALARM_M_UP   503
#define TAG_ALARM_M_DOWN 504
#define TAG_ALARM_TOGGLE 505

#define TAG_TIMER_START 601
#define TAG_TIMER_RESET 602
#define TAG_TIMER_ADD_1MIN 603

CCPoint* getHandTip(int cx, int cy, float radius, float angleRad);
void clock_animation_task(void *pvParam);
void formatTimeStr(char* buf, int h, int m, bool seconds);
void setup_clock_app(void);
void draw_analog_face_ui(void);
void draw_world_clock_ui(void);
void draw_alarm_ui(void);
void makeBtn(int idx, char* txt, int tag);
void draw_timer_ui(void);
void handle_clock_touch(int x, int y, int type);
void switch_clock_tab(ClockTab tab);

#endif /* ClockApp_h */
