//
//  MusicApp.c
//
//
//  Created by Chris Galzerano on 2/26/26.
//

#include "main.h"
#include "MusicApp.h"

/*
 // --- Music Player Globals ---
 extern CCLabel* uiMusicTitleLbl;
 extern CCLabel* uiMusicArtistLbl;
 extern CCView* uiMusicProgressFill;
 extern CCLabel* uiMusicPlayBtnLbl;
 
 // Tag Constants
 #define TAG_MUSIC_PREV 20
 #define TAG_MUSIC_PLAY 21
 #define TAG_MUSIC_NEXT 22
 #define TAG_MUSIC_PROGRESS_BAR 23
 
 // Layout Constants
 #define SCREEN_W 320
 #define MAIN_PADDING 20
 #define ART_SIZE 280
 
 // --- Music Player Globals ---
 CCLabel* uiMusicTitleLbl = NULL;
 CCLabel* uiMusicArtistLbl = NULL;
 CCView* uiMusicProgressFill = NULL;
 CCLabel* uiMusicPlayBtnLbl = NULL;
 
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
 */

// ======================================================================
// 1. UNIVERSAL CONFIG I/O & MP3 PARSERS
// ======================================================================

#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>

// ======================================================================
// MUSIC APP GLOBALS & TAGS
// ======================================================================

// --- UI TAGS (For Touch Routing) ---
// Navigation & Core UI
#define TAG_MUSIC_NAV_LIBRARY     1001
#define TAG_MUSIC_NAV_PLAYLISTS   1002
#define TAG_MUSIC_NAV_ARTISTS     1003 // Replaced Settings

// Player Controls
#define TAG_MUSIC_TOGGLE_LOOP     2001
#define TAG_MUSIC_TOGGLE_SHUFFLE  2002
#define TAG_MUSIC_PLAY            2003
#define TAG_MUSIC_PREV            2004
#define TAG_MUSIC_NEXT            2005
#define TAG_MUSIC_PROGRESS_BAR    2006

// Library & Lists
#define TAG_MUSIC_SEARCH_BAR      500
#define TAG_MUSIC_SCAN_BTN        501
#define TAG_MUSIC_LIB_ROW_START   10000 // e.g., 10005 = Track Index 5
#define TAG_MUSIC_LIB_CHECK_START 20000 // e.g., 20005 = Checkbox Index 5

// Context Menu (Long Press)
#define TAG_MUSIC_CTX_BG          30000
#define TAG_MUSIC_CTX_TRACK_PLAY  30001
#define TAG_MUSIC_CTX_TRACK_ADD   30002
#define TAG_MUSIC_CTX_TRACK_PROP  30003
#define TAG_MUSIC_CTX_TRACK_DEL   30004
#define TAG_MUSIC_CTX_PL_OPEN     30101
#define TAG_MUSIC_CTX_PL_COPY     30102
#define TAG_MUSIC_CTX_PL_DEL      30103

#define TAG_MUSIC_ARTIST_ROW_START  40000 // Tapping an artist name
#define TAG_MUSIC_ALBUM_ROW_START   50000 // Tapping an album name
#define TAG_MUSIC_ARTIST_BACK       60000 // "< Artists" back button
#define TAG_MUSIC_ARTIST_TAB_SONGS  60001 // Sub-tab
#define TAG_MUSIC_ARTIST_TAB_ALBUMS 60002 // Sub-tab




// --- CONTEXT MENU TYPES ---
typedef enum {
    MusicContextTypeTrack = 0,
    MusicContextTypePlaylist = 1
} MusicContextType;


// --- GLOBAL POINTERS & STATE ---

// 1. Master View Containers
CCView* uiMusicTopBar = NULL;
CCView* uiMusicContentView = NULL;
CCView* currentMusicSubView = NULL; // Tracks active tab for safe memory deletion

// 2. Player Tab Elements
CCImageView* uiMusicLoopIcon = NULL;
CCImageView* uiMusicShuffleIcon = NULL;
CCLabel* uiMusicTitleLbl = NULL;
CCLabel* uiMusicArtistLbl = NULL;
CCLabel* uiMusicPlayBtnLbl = NULL;
CCView* uiMusicProgressFill = NULL;
bool isMusicLooping = false;
bool isMusicShuffling = false;

// 3. Library & Data State
CCArray* globalMusicLibrary = NULL; // Holds CCDictionaries of scanned MP3 metadata
bool isMusicLibrarySelectionMode = false; // Toggles checkboxes when building playlists

// 4. Artists Tab State
CCString* currentSelectedArtist = NULL; // NULL means show master artist list
bool isArtistDetailShowingAlbums = false; // Toggles between "Songs" and "Albums" sub-tabs

// 5. Context Menu State
CCView* uiMusicContextMenu = NULL;

// ======================================================================
// TOUCH STATE TRACKERS
// ======================================================================
static bool is_pressing = false;
static bool long_press_fired = false;
static uint64_t touchStartTime = 0;
static int touchStartX = 0;
static int touchStartY = 0;

// --- PAGINATION GLOBALS ---
int musicRowsPerPage = 6; // Configurable variable for different screens

// --- NEW TAGS ---
#define TAG_MUSIC_PAGE_PREV 70001
#define TAG_MUSIC_PAGE_NEXT 70002

#define TAG_MUSIC_MENU_BTN 70003
CCView* uiMusicMenuOverlay = NULL; // Tracks the top-left menu popup

// --- NEW TAGS ---
#define TAG_MUSIC_PL_ADD_ITEM  70012
#define TAG_MUSIC_PL_ADD_DONE  70013
#define TAG_MUSIC_PL_BACK      70014

// --- TAB STATE ---
typedef enum {
    MusicTabLibrary = 0,
    MusicTabPlaylists,
    MusicTabArtists
} MusicTab;

MusicTab currentMusicTab = MusicTabLibrary;
int currentDataPage = 0;   // Replaces currentLibraryPage
int totalDataPages = 1;

// --- PERSISTENT SHELL VIEWS ---
CCView* uiMusicHeaderView = NULL;
CCView* uiMusicFooterView = NULL;
CCView* uiMusicPaginationContainer = NULL; // The invisible wrapper for the buttons

// --- NEW TAGS ---
#define TAG_MUSIC_PL_CREATE 70004

// --- PLAYLISTS GLOBALS ---
CCArray* globalPlaylists = NULL;
CCView* uiNewPlaylistDialog = NULL;
CCLabel* uiNewPlaylistInput = NULL;
CCString* currentSelectedPlaylist = NULL; // For when we build the detail view later

// --- NEW TAGS ---
#define TAG_MUSIC_PL_CREATE_DIALOG_CANCEL  70010
#define TAG_MUSIC_PL_CREATE_DIALOG_CONFIRM 70011
#define TAG_MUSIC_PLAYLIST_ROW_START       80000 // Maps to playlist array index

void ensure_parent_directories(const char* filepath) {
    char tmp[256];
    strncpy(tmp, filepath, sizeof(tmp));
    char* lastSlash = strrchr(tmp, '/');
    if (lastSlash) {
        *lastSlash = '\0';
        mkdir(tmp, 0777);
    }
}

// ======================================================================
// 1. UNIVERSAL CONFIG I/O & MP3 PARSERS (CORRECTED API)
// ======================================================================

void saveConfigFile(CCString* relativePath, CCDictionary* config) {
    if (!relativePath || !config) return;
    
    // 1. Wrap the Dictionary in a CCJSONObject
    CCJSONObject* jsonObj = jsonObjectWithObject(config);
    if (!jsonObj) return;
    
    // 2. Generate the JSON String (Using your Compressed style to save SD card space)
    generateJsonStringFromObject(jsonObj, CCJSONWriteStyleCompressed); // Assuming this enum is available
    
    // 3. Extract the generated string
    CCString* jsonString = jsonObj->jsonString;
    
    if (!jsonString || !cStringOfString(jsonString)) {
        // Handle cleanup if needed
        return;
    }
    
    CCString* sdPath = stringWithFormat("/sdcard/freeos/%s", cStringOfString(relativePath));
    CCString* flashPath = stringWithFormat("/spiflash/freeos/%s", cStringOfString(relativePath));
    
    ensure_parent_directories(cStringOfString(sdPath));
    FILE* file = fopen(cStringOfString(sdPath), "w");
    
    if (file) {
        fwrite(cStringOfString(jsonString), 1, strlen(cStringOfString(jsonString)), file);
        fclose(file);
    } else {
        ensure_parent_directories(cStringOfString(flashPath));
        file = fopen(cStringOfString(flashPath), "w");
        if (file) {
            fwrite(cStringOfString(jsonString), 1, strlen(cStringOfString(jsonString)), file);
            fclose(file);
        }
    }
    
    // NOTE: Depending on your memory model, you may need to free 'jsonObj' and paths here.
}

CCDictionary* loadConfigFile(CCString* relativePath) {
    if (!relativePath || !cStringOfString(relativePath)) return NULL;
    
    CCString* flashPath = stringWithFormat("/spiflash/freeos/%s", cStringOfString(relativePath));
    CCString* sdPath = stringWithFormat("/sdcard/freeos/%s", cStringOfString(relativePath));
    
    // Try Flash first
    CCString* jsonRawText = stringWithContentsOfFile(flashPath);
    if (!jsonRawText) {
        // Fallback to SD card
        jsonRawText = stringWithContentsOfFile(sdPath);
    }
    
    if (!jsonRawText) return NULL; // No file found
    
    // 1. Wrap the raw text in your CCJSONObject
    CCJSONObject* jsonObj = jsonObjectWithJSONString(jsonRawText);
    if (!jsonObj) return NULL;
    
    // 2. Parse the JSON string into ObjectiveCC Dictionaries/Arrays
    generateObjectFromJsonString(jsonObj);
    
    // 3. Extract the parsed dictionary
    CCDictionary* parsedConfig = (CCDictionary*)jsonObj->jsonObject;
    
    // NOTE: Free 'jsonObj' and 'jsonRawText' wrapper here if your API requires manual cleanup.
    
    return parsedConfig;
}

// --- HELPER FUNCTION ---
CCDictionary* get_current_playlist_dict(void) {
    if (!globalPlaylists || !currentSelectedPlaylist) return NULL;
    for(int i = 0; i < arrayCount(globalPlaylists); i++) {
        CCDictionary* pl = arrayObjectAtIndex(globalPlaylists, i);
        CCString* name = dictionaryObjectForKey(pl, ccs("Name"));
        if (name && stringEqualsString(name, currentSelectedPlaylist)) return pl;
    }
    return NULL;
}

int decode_id3_size(unsigned char* size_bytes) {
    return (size_bytes[0] << 21) | (size_bytes[1] << 14) | (size_bytes[2] << 7) | size_bytes[3];
}

// 1. Math for the master header and ID3v2.4 frames
int decode_syncsafe_size(unsigned char* bytes) {
    return (bytes[0] << 21) | (bytes[1] << 14) | (bytes[2] << 7) | bytes[3];
}

// 2. Math for ID3v2.3 frames
int decode_standard_size(unsigned char* bytes) {
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

CCDictionary* music_parse_mp3_metadata(const char* filepath) {
    CCDictionary* trackData = dictionary();
    
    // Set default fallbacks
    dictionarySetObjectForKey(trackData, ccs(filepath), ccs("Path"));
    const char* filename = strrchr(filepath, '/');
    filename = (filename) ? filename + 1 : filepath;
    dictionarySetObjectForKey(trackData, ccs(filename), ccs("Title"));
    dictionarySetObjectForKey(trackData, ccs("Unknown Artist"), ccs("Artist"));
    dictionarySetObjectForKey(trackData, ccs("Unknown Album"), ccs("Album"));
    dictionarySetObjectForKey(trackData, ccs("0:00"), ccs("Duration"));
    
    FILE* file = fopen(filepath, "rb");
    if (!file) return trackData;
    
    // Basic CBR duration estimate (minimp3 will fix this for VBR later)
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    int durationSeconds = (fileSize * 8) / 128000;
    char durationStr[16];
    snprintf(durationStr, sizeof(durationStr), "%d:%02d", durationSeconds / 60, durationSeconds % 60);
    dictionarySetObjectForKey(trackData, ccs(durationStr), ccs("Duration"));
    
    unsigned char header[10];
    if (fread(header, 1, 10, file) == 10) {
        // Confirm it's an ID3 tag
        if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
            int version = header[3]; // Typically 3 (v2.3) or 4 (v2.4)
            int tagSize = decode_syncsafe_size(&header[6]);
            int bytesRead = 0;
            
            // Limit scanning to first 4KB to save RAM
            int maxRead = (tagSize < 4096) ? tagSize : 4096;
            
            while (bytesRead < maxRead) {
                unsigned char frameHeader[10];
                if (fread(frameHeader, 1, 10, file) != 10) break;
                bytesRead += 10;
                
                if (frameHeader[0] == 0) break; // Reached padding, we're done
                
                // APPLY THE VERSION MATH FIX
                int frameSize = 0;
                if (version >= 4) {
                    frameSize = decode_syncsafe_size(&frameHeader[4]);
                } else {
                    frameSize = decode_standard_size(&frameHeader[4]);
                }
                
                if (frameSize <= 0 || frameSize > 5000000) break; // Sanity check
                
                // Read up to 256 bytes of the frame body
                char frameData[257] = {0};
                int readSize = (frameSize > 256) ? 256 : frameSize;
                
                if (fread(frameData, 1, readSize, file) == readSize) {
                    // Fast-forward the file pointer if the frame was larger than our buffer (e.g. cover art)
                    if (frameSize > 256) fseek(file, frameSize - 256, SEEK_CUR);
                    
                    char parsedText[256] = {0};
                    int encoding = frameData[0];
                    int outIdx = 0;
                    
                    // Text starting index: Skip encoding byte
                    int startIdx = 1;
                    
                    // Skip UTF-16 Byte Order Marks (BOM)
                    if (encoding == 1 || encoding == 2) {
                        if (readSize >= 3 && ((frameData[1] == (char)0xFF && frameData[2] == (char)0xFE) ||
                                              (frameData[1] == (char)0xFE && frameData[2] == (char)0xFF))) {
                            startIdx = 3;
                        }
                    }
                    
                    // Extract printable characters, skipping nulls and hidden control characters
                    for (int k = startIdx; k < readSize && outIdx < 255; k++) {
                        if ((unsigned char)frameData[k] >= 32) { // Only standard/extended printable characters
                            parsedText[outIdx++] = frameData[k];
                        }
                    }
                    
                    if (outIdx > 0) {
                        if (strncmp((char*)frameHeader, "TIT2", 4) == 0) {
                            dictionarySetObjectForKey(trackData, ccs(parsedText), ccs("Title"));
                        } else if (strncmp((char*)frameHeader, "TPE1", 4) == 0) {
                            dictionarySetObjectForKey(trackData, ccs(parsedText), ccs("Artist"));
                        } else if (strncmp((char*)frameHeader, "TALB", 4) == 0) {
                            dictionarySetObjectForKey(trackData, ccs(parsedText), ccs("Album"));
                        }
                    }
                } else {
                    break;
                }
                bytesRead += frameSize;
            }
        }
    }
    fclose(file);
    return trackData;
}

void music_scan_directory(const char* dirPath) {
    DIR* dir = opendir(dirPath);
    if (!dir) {
        printf("MUSIC SCAN: Could not open directory -> %s\n", dirPath);
        return;
    }
    
    printf("MUSIC SCAN: Reading directory -> %s\n", dirPath);
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden files, current dir, and parent dir
        if (entry->d_name[0] == '.') continue;
        
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);
        
        struct stat path_stat;
        stat(fullPath, &path_stat);
        
        if (S_ISDIR(path_stat.st_mode)) {
            // It's a directory: Recurse!
            music_scan_directory(fullPath);
        } else {
            // It's a file: Check for .mp3 extension
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && (strcmp(ext, ".mp3") == 0 || strcmp(ext, ".MP3") == 0)) {
                printf("MUSIC SCAN: Found MP3 -> %s\n", entry->d_name);
                
                // Parse the metadata into a CCDictionary
                CCDictionary* trackMetadata = music_parse_mp3_metadata(fullPath);
                
                // Add to the global array
                arrayAddObject(globalMusicLibrary, trackMetadata);
            }
        }
    }
    closedir(dir);
}

void music_trigger_full_scan(void) {
    if (globalMusicLibrary) {
        freeCCArray(globalMusicLibrary);
    }
    globalMusicLibrary = array();
    
    printf("=== STARTING FULL MP3 SCAN ===\n");
    
    // NOTE: Verify these are your exact FreeOS mount points!
    music_scan_directory("/sdcard");
    music_scan_directory("/spiflash");
    // music_scan_directory("/internal"); // Uncomment if your flash uses this name
    
    printf("=== SCAN COMPLETE: Found %d tracks ===\n", arrayCount(globalMusicLibrary));
    
    // Wrap and Save
    CCDictionary* libraryWrapper = dictionary();
    dictionarySetObjectForKey(libraryWrapper, globalMusicLibrary, ccs("tracks"));
    saveConfigFile(ccs("music/musicLibrary.json"), libraryWrapper);
}

// ======================================================================
// 2. RELATIONAL DATA EXTRACTORS
// ======================================================================

CCArray* music_get_unique_artists(void) {
    CCArray* uniqueArtists = array();
    if (!globalMusicLibrary) return uniqueArtists;
    
    int trackCount = arrayCount(globalMusicLibrary);
    for (int i = 0; i < trackCount; i++) {
        CCDictionary* track = arrayObjectAtIndex(globalMusicLibrary, i);
        CCString* artistName = dictionaryObjectForKey(track, ccs("Artist"));
        if (!artistName) artistName = ccs("Unknown Artist");
        
        bool found = false;
        for (int j = 0; j < arrayCount(uniqueArtists); j++) {
            CCString* existing = arrayObjectAtIndex(uniqueArtists, j);
            if (stringEqualsString(existing, artistName)) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            // FIX: Pass a COPY of the string into the temporary array
            arrayAddObject(uniqueArtists, copyCCString(artistName));
        }
    }
    return uniqueArtists;
}

CCArray* music_get_albums_for_artist(CCString* targetArtist) {
    CCArray* uniqueAlbums = array();
    if (!globalMusicLibrary || !targetArtist) return uniqueAlbums;
    
    int trackCount = arrayCount(globalMusicLibrary);
    for (int i = 0; i < trackCount; i++) {
        CCDictionary* track = arrayObjectAtIndex(globalMusicLibrary, i);
        CCString* artistName = dictionaryObjectForKey(track, ccs("Artist"));
        if (!artistName) artistName = ccs("Unknown Artist");
        
        if (stringEqualsString(artistName, targetArtist)) {
            CCString* albumName = dictionaryObjectForKey(track, ccs("Album"));
            if (!albumName) albumName = ccs("Unknown Album");
            
            bool found = false;
            for (int j = 0; j < arrayCount(uniqueAlbums); j++) {
                CCString* existing = arrayObjectAtIndex(uniqueAlbums, j);
                if (stringEqualsString(existing, albumName)) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                // FIX: Pass a COPY of the string into the temporary array
                arrayAddObject(uniqueAlbums, copyCCString(albumName));
            }
        }
    }
    return uniqueAlbums;
}

// ======================================================================
// 3. MUSIC APP UI BUILDERS
// ======================================================================

void update_music_pagination_ui(void) {
    // 1. THE CONTAINER FIX: Safely wipe the old buttons without any array loops!
    if (uiMusicPaginationContainer) {
        viewRemoveFromSuperview(uiMusicPaginationContainer);
        freeViewHierarchy(uiMusicPaginationContainer);
        uiMusicPaginationContainer = NULL;
    }
    
    // Create a fresh container that fills the footer
    uiMusicPaginationContainer = viewWithFrame(ccRect(0, 0, SCREEN_W, 60));
    uiMusicPaginationContainer->backgroundColor = color(0, 0, 0, 0.0); // Transparent
    
    int btnWidth = 80;
    int currentY = 10; // Margin from top of footer
    
    // ==========================================
    // PREV BUTTON
    // ==========================================
    if (currentDataPage > 0) {
        CCView* prevBtn = viewWithFrame(ccRect(10, currentY, btnWidth, 40));
        prevBtn->backgroundColor = color(0.2, 0.2, 0.25, 1.0);
        layerSetCornerRadius(prevBtn->layer, 8.0);
        prevBtn->tag = TAG_MUSIC_PAGE_PREV;
        
        CCLabel* prevLbl = labelWithFrame(ccRect(0, 0, btnWidth, 40));
        prevLbl->text = ccs("< Back");
        prevLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
        prevLbl->textAlignment = CCTextAlignmentCenter;
        prevLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        prevLbl->ignoreTouch = true;
        viewAddSubview(prevBtn, prevLbl);
        viewAddSubview(uiMusicPaginationContainer, prevBtn); // Add to container!
    }
    
    // ==========================================
    // PAGE INDICATOR
    // ==========================================
    // FIX: Use snprintf + ccs to guarantee a fresh, isolated heap allocation!
    char pageStr[32];
    snprintf(pageStr, sizeof(pageStr), "Page %d of %d", currentDataPage + 1, totalDataPages);
    
    CCLabel* pageIndicator = labelWithFrame(ccRect(btnWidth + 10, currentY, SCREEN_W - (btnWidth * 2) - 20, 40));
    pageIndicator->text = ccs(pageStr);
    pageIndicator->textColor = color(0.6, 0.6, 0.7, 1.0);
    pageIndicator->textAlignment = CCTextAlignmentCenter;
    pageIndicator->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    pageIndicator->ignoreTouch = true;
    viewAddSubview(uiMusicPaginationContainer, pageIndicator); // Add to container!
    
    // ==========================================
    // NEXT BUTTON
    // ==========================================
    if (currentDataPage < totalDataPages - 1) {
        CCView* nextBtn = viewWithFrame(ccRect(SCREEN_W - btnWidth - 10, currentY, btnWidth, 40));
        nextBtn->backgroundColor = color(0.2, 0.2, 0.25, 1.0);
        layerSetCornerRadius(nextBtn->layer, 8.0);
        nextBtn->tag = TAG_MUSIC_PAGE_NEXT;
        
        CCLabel* nextLbl = labelWithFrame(ccRect(0, 0, btnWidth, 40));
        nextLbl->text = ccs("Next >");
        nextLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
        nextLbl->textAlignment = CCTextAlignmentCenter;
        nextLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        nextLbl->ignoreTouch = true;
        viewAddSubview(nextBtn, nextLbl);
        viewAddSubview(uiMusicPaginationContainer, nextBtn); // Add to container!
    }
    
    // Finally, attach the fully assembled container to the persistent footer
    viewAddSubview(uiMusicFooterView, uiMusicPaginationContainer);
}

void setup_music_app(void) {
    currentView = CurrentViewMusic;
    if (mainWindowView) freeViewHierarchy(mainWindowView);
    mainWindowView = viewWithFrame(ccRect(0, 0, SCREEN_W, 480));
    mainWindowView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
    
    // ==========================================
    // 1. TOP NAV BAR
    // ==========================================
    uiMusicTopBar = viewWithFrame(ccRect(0, 25, SCREEN_W, 40));
    uiMusicTopBar->backgroundColor = color(0.12, 0.12, 0.16, 1.0);
    
    CCView* navBorder = viewWithFrame(ccRect(0, 39, SCREEN_W, 1));
    navBorder->backgroundColor = color(0.3, 0.3, 0.35, 1.0);
    viewAddSubview(uiMusicTopBar, navBorder);
    
    int navBtnW = SCREEN_W / 3;
    
    // Library Tab
    CCView* libBtn = viewWithFrame(ccRect(0, 0, navBtnW, 40));
    libBtn->tag = TAG_MUSIC_NAV_LIBRARY;
    CCLabel* libLbl = labelWithFrame(ccRect(0, 0, navBtnW, 40));
    libLbl->text = ccs("Library");
    libLbl->textColor = color(0.8, 0.8, 0.9, 1.0);
    libLbl->fontSize = 14;
    libLbl->textAlignment = CCTextAlignmentCenter;
    libLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    libLbl->ignoreTouch = true;
    viewAddSubview(libBtn, libLbl);
    viewAddSubview(uiMusicTopBar, libBtn);
    
    // Playlists Tab
    CCView* plBtn = viewWithFrame(ccRect(navBtnW, 0, navBtnW, 40));
    plBtn->tag = TAG_MUSIC_NAV_PLAYLISTS;
    CCLabel* plLbl = labelWithFrame(ccRect(0, 0, navBtnW, 40));
    plLbl->text = ccs("Playlists");
    plLbl->textColor = color(0.8, 0.8, 0.9, 1.0);
    plLbl->fontSize = 14;
    plLbl->textAlignment = CCTextAlignmentCenter;
    plLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    plLbl->ignoreTouch = true;
    viewAddSubview(plBtn, plLbl);
    viewAddSubview(uiMusicTopBar, plBtn);
    
    // Artists Tab
    CCView* artBtn = viewWithFrame(ccRect(navBtnW * 2, 0, navBtnW, 40));
    artBtn->tag = TAG_MUSIC_NAV_ARTISTS;
    CCLabel* artLbl = labelWithFrame(ccRect(0, 0, navBtnW, 40));
    artLbl->text = ccs("Artists");
    artLbl->textColor = color(0.8, 0.8, 0.9, 1.0);
    artLbl->fontSize = 14;
    artLbl->textAlignment = CCTextAlignmentCenter;
    artLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    artLbl->ignoreTouch = true;
    viewAddSubview(artBtn, artLbl);
    viewAddSubview(uiMusicTopBar, artBtn);
    
    viewAddSubview(mainWindowView, uiMusicTopBar);
    
    // ==========================================
    // 2. PERSISTENT HEADER (Menu & Search)
    // ==========================================
    uiMusicHeaderView = viewWithFrame(ccRect(0, afb(uiMusicTopBar), SCREEN_W, 46));
    
    CCImageView* menuIcon = imageViewWithFrame(ccRect(10, 5, 36, 36));
    menuIcon->image = imageWithFile(ccs("/spiflash/menu.png"));
    menuIcon->ignoreTouch = true;
    menuIcon->alpha = 0.7;
    viewAddSubview(uiMusicHeaderView, menuIcon);
    
    CCView* menuBtnHitbox = viewWithFrame(ccRect(5, 0, 46, 46));
    menuBtnHitbox->tag = TAG_MUSIC_MENU_BTN;
    menuBtnHitbox->backgroundColor = color(0, 0, 0, 0.0);
    viewAddSubview(uiMusicHeaderView, menuBtnHitbox);
    
    CCView* searchBarView = viewWithFrame(ccRect(56, 5, SCREEN_W - 66, 36));
    searchBarView->backgroundColor = color(0.15, 0.15, 0.2, 1.0);
    layerSetCornerRadius(searchBarView->layer, 18.0);
    searchBarView->tag = TAG_MUSIC_SEARCH_BAR;
    
    CCLabel* searchPlaceholder = labelWithFrame(ccRect(15, 0, 200, 36));
    searchPlaceholder->text = ccs("Search...");
    searchPlaceholder->textColor = color(0.5, 0.5, 0.6, 1.0);
    searchPlaceholder->fontSize = 14;
    searchPlaceholder->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    searchPlaceholder->ignoreTouch = true;
    viewAddSubview(searchBarView, searchPlaceholder);
    viewAddSubview(uiMusicHeaderView, searchBarView);
    
    viewAddSubview(mainWindowView, uiMusicHeaderView);
    
    // ==========================================
    // 3. PERSISTENT FOOTER (Pagination)
    // ==========================================
    int footerHeight = 60;
    uiMusicFooterView = viewWithFrame(ccRect(0, 480 - footerHeight, SCREEN_W, footerHeight));
    uiMusicFooterView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
    viewAddSubview(mainWindowView, uiMusicFooterView);
    // (We will populate this dynamically using the update_music_pagination_ui function below)
    
    // ==========================================
    // 4. DYNAMIC CONTENT CANVAS
    // ==========================================
    int contentY = afb(uiMusicHeaderView);
    int contentHeight = (480 - footerHeight) - contentY;
    
    uiMusicContentView = viewWithFrame(ccRect(0, contentY, SCREEN_W, contentHeight));
    uiMusicContentView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
    viewAddSubview(mainWindowView, uiMusicContentView);
    
    // Initial Load
    currentMusicTab = MusicTabLibrary;
    currentDataPage = 0;
    build_music_library_ui();
}

void show_music_context_menu(MusicContextType type, int index, int touchX, int touchY) {
    if (uiMusicContextMenu) {
        viewRemoveFromSuperview(uiMusicContextMenu);
        freeViewHierarchy(uiMusicContextMenu);
        uiMusicContextMenu = NULL;
    }
    
    uiMusicContextMenu = viewWithFrame(ccRect(0, 0, SCREEN_W, 480));
    uiMusicContextMenu->backgroundColor = color(0, 0, 0, 0.0);
    uiMusicContextMenu->tag = TAG_MUSIC_CTX_BG;
    
    int menuW = 180;
    int btnH = 44;
    int itemCount = (type == MusicContextTypeTrack) ? 4 : 3;
    int menuH = itemCount * btnH;
    
    if (touchX + menuW > SCREEN_W - 10) touchX = SCREEN_W - menuW - 10;
    if (touchY + menuH > 480 - 10) touchY = 480 - menuH - 10;
    
    CCView* menuBox = viewWithFrame(ccRect(touchX, touchY, menuW, menuH));
    menuBox->backgroundColor = color(0.15, 0.15, 0.2, 0.95);
    layerSetCornerRadius(menuBox->layer, 12.0);
    menuBox->layer->borderWidth = 1.0;
    menuBox->layer->borderColor = color(0.3, 0.3, 0.4, 1.0);
    menuBox->layer->shadowOpacity = 0.5;
    menuBox->layer->shadowRadius = 10;
    menuBox->layer->shadowOffset = ccPoint(0, 5);
    
    CCArray* buttonLabels = array();
    
    if (type == MusicContextTypeTrack) {
        arrayAddObject(buttonLabels, ccs("Play"));
        arrayAddObject(buttonLabels, ccs("Add to Playlist"));
        arrayAddObject(buttonLabels, ccs("Properties"));
        arrayAddObject(buttonLabels, ccs("Remove from Library"));
    } else {
        arrayAddObject(buttonLabels, ccs("Open Playlist"));
        arrayAddObject(buttonLabels, ccs("Copy Playlist"));
        arrayAddObject(buttonLabels, ccs("Delete Playlist"));
    }
    
    for (int i = 0; i < itemCount; i++) {
        CCView* btn = viewWithFrame(ccRect(0, i * btnH, menuW, btnH));
        btn->tag = (type == MusicContextTypeTrack ? TAG_MUSIC_CTX_TRACK_PLAY : TAG_MUSIC_CTX_PL_OPEN) + i + (index * 100000);
        
        CCString* lblStr = arrayObjectAtIndex(buttonLabels, i);
        CCLabel* lbl = labelWithFrame(ccRect(15, 0, menuW - 30, btnH));
        lbl->text = lblStr;
        lbl->textColor = color(0.9, 0.9, 0.95, 1.0);
        lbl->fontSize = 14;
        lbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        lbl->ignoreTouch = true;
        
        if (i < itemCount - 1) {
            CCView* line = viewWithFrame(ccRect(10, btnH - 1, menuW - 20, 1));
            line->backgroundColor = color(0.3, 0.3, 0.4, 0.5);
            viewAddSubview(btn, line);
        }
        
        viewAddSubview(btn, lbl);
        viewAddSubview(menuBox, btn);
    }
    
    viewAddSubview(uiMusicContextMenu, menuBox);
    viewAddSubview(mainWindowView, uiMusicContextMenu);
    freeCCArray(buttonLabels);
}

void build_music_library_ui(void) {
    if (currentMusicSubView) {
        viewRemoveFromSuperview(currentMusicSubView);
        freeViewHierarchy(currentMusicSubView);
        currentMusicSubView = NULL;
    }
    
    int trackCount = arrayCount(globalMusicLibrary);
    totalDataPages = (trackCount > 0) ? ((trackCount + musicRowsPerPage - 1) / musicRowsPerPage) : 1;
    
    if (currentDataPage >= totalDataPages) currentDataPage = totalDataPages - 1;
    if (currentDataPage < 0) currentDataPage = 0;
    
    int startIndex = currentDataPage * musicRowsPerPage;
    int endIndex = startIndex + musicRowsPerPage;
    if (endIndex > trackCount) endIndex = trackCount;
    
    // Canvas size is now just the exact height of the rows!
    int rowHeight = 44;
    int totalCanvasHeight = 25 + (musicRowsPerPage * rowHeight);
    
    currentMusicSubView = viewWithFrame(ccRect(0, 0, SCREEN_W, totalCanvasHeight));
    currentMusicSubView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
    
    int currentY = 0;
    
    // --- SELECTION MODE "DONE" BUTTON ---
    CCDictionary* activePl = NULL;
    CCArray* plTracks = NULL;
    
    if (isMusicLibrarySelectionMode) {
        activePl = get_current_playlist_dict();
        if (activePl) plTracks = dictionaryObjectForKey(activePl, ccs("Tracks"));
        
        CCView* doneHeader = viewWithFrame(ccRect(0, currentY, SCREEN_W, 44));
        doneHeader->backgroundColor = color(0.2, 0.6, 1.0, 1.0);
        doneHeader->tag = TAG_MUSIC_PL_ADD_DONE;
        
        CCLabel* doneLbl = labelWithFrame(ccRect(0, 0, SCREEN_W, 44));
        doneLbl->text = copyCCString(ccs("Done Adding Tracks"));
        doneLbl->textColor = color(1, 1, 1, 1);
        doneLbl->fontSize = 16;
        doneLbl->textAlignment = CCTextAlignmentCenter;
        doneLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        doneLbl->ignoreTouch = true;
        viewAddSubview(doneHeader, doneLbl);
        viewAddSubview(currentMusicSubView, doneHeader);
        currentY += 44;
    }
    
    // ==========================================
    // 2. COLUMN HEADERS
    // ==========================================
    int textStartX = isMusicLibrarySelectionMode ? 45 : 15;
    int durationWidth = 45;
    int availableTextWidth = SCREEN_W - textStartX - durationWidth - 10;
    int titleWidth = (int)(availableTextWidth * 0.55);
    int artistWidth = (int)(availableTextWidth * 0.45);
    int artistStartX = textStartX + titleWidth;
    int durationStartX = SCREEN_W - durationWidth - 10;
    
    CCView* headerRow = viewWithFrame(ccRect(0, currentY, SCREEN_W, 20));
    
    CCLabel* headTitle = labelWithFrame(ccRect(textStartX, 0, titleWidth, 20));
    headTitle->text = ccs("TITLE");
    headTitle->textColor = color(0.4, 0.4, 0.5, 1.0);
    headTitle->fontSize = 10;
    headTitle->ignoreTouch = true;
    viewAddSubview(headerRow, headTitle);
    
    CCLabel* headArtist = labelWithFrame(ccRect(artistStartX, 0, artistWidth, 20));
    headArtist->text = ccs("ARTIST");
    headArtist->textColor = color(0.4, 0.4, 0.5, 1.0);
    headArtist->fontSize = 10;
    headArtist->ignoreTouch = true;
    viewAddSubview(headerRow, headArtist);
    
    CCLabel* headTime = labelWithFrame(ccRect(durationStartX, 0, durationWidth, 20));
    headTime->text = ccs("TIME");
    headTime->textColor = color(0.4, 0.4, 0.5, 1.0);
    headTime->fontSize = 10;
    headTime->textAlignment = CCTextAlignmentRight;
    headTime->ignoreTouch = true;
    viewAddSubview(headerRow, headTime);
    
    viewAddSubview(currentMusicSubView, headerRow);
    currentY += 20 + 5;
    
    // ==========================================
    // 3. PAGINATED TABLE ROWS
    // ==========================================
    for (int i = startIndex; i < endIndex; i++) {
        CCDictionary* track = arrayObjectAtIndex(globalMusicLibrary, i);
        
        CCString* title = dictionaryObjectForKey(track, ccs("Title"));
        CCString* artist = dictionaryObjectForKey(track, ccs("Artist"));
        CCString* duration = dictionaryObjectForKey(track, ccs("Duration"));
        
        // FIX: Copy the strings from the dictionary so the view hierarchy
        // can safely free them later without corrupting the master array!
        CCString* safeTitle = title ? copyCCString(title) : ccs("Unknown");
        CCString* safeArtist = artist ? copyCCString(artist) : ccs("Unknown Artist");
        CCString* safeDuration = duration ? copyCCString(duration) : ccs("-:--");
        
        CCView* rowView = viewWithFrame(ccRect(0, currentY, SCREEN_W, rowHeight));
        rowView->tag = TAG_MUSIC_LIB_ROW_START + i;
        
        if (i % 2 == 0) rowView->backgroundColor = color(0.1, 0.1, 0.15, 1.0);
        else rowView->backgroundColor = color(0.08, 0.08, 0.12, 0.0);
        
        if (isMusicLibrarySelectionMode) {
            int checkSize = 20;
            CCView* checkbox = viewWithFrame(ccRect(15, (rowHeight - checkSize) / 2, checkSize, checkSize));
            
            // Check if this track's Path is already in the playlist!
            bool isChecked = false;
            CCString* trackPath = dictionaryObjectForKey(track, ccs("Path"));
            if (plTracks && trackPath) {
                for (int t = 0; t < arrayCount(plTracks); t++) {
                    CCString* savedPath = arrayObjectAtIndex(plTracks, t);
                    if (stringEqualsString(savedPath, trackPath)) {
                        isChecked = true; break;
                    }
                }
            }
            
            checkbox->backgroundColor = isChecked ? color(0.2, 0.6, 1.0, 1.0) : color(0.08, 0.08, 0.12, 1.0);
            checkbox->layer->borderWidth = 1.5;
            checkbox->layer->borderColor = color(0.3, 0.3, 0.4, 1.0);
            layerSetCornerRadius(checkbox->layer, checkSize / 2.0);
            checkbox->tag = TAG_MUSIC_LIB_CHECK_START + i;
            viewAddSubview(rowView, checkbox);
        }
        
        CCLabel* titleLbl = labelWithFrame(ccRect(textStartX, 0, titleWidth - 10, rowHeight));
        titleLbl->text = safeTitle; // Assign the safe copy
        titleLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
        titleLbl->fontSize = 14;
        titleLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        titleLbl->ignoreTouch = true;
        viewAddSubview(rowView, titleLbl);
        
        CCLabel* artistLbl = labelWithFrame(ccRect(artistStartX, 0, artistWidth - 5, rowHeight));
        artistLbl->text = safeArtist; // Assign the safe copy
        artistLbl->textColor = color(0.6, 0.6, 0.7, 1.0);
        artistLbl->fontSize = 12;
        artistLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        artistLbl->ignoreTouch = true;
        viewAddSubview(rowView, artistLbl);
        
        CCLabel* durLbl = labelWithFrame(ccRect(durationStartX, 0, durationWidth, rowHeight));
        durLbl->text = safeDuration; // Assign the safe copy
        durLbl->textColor = color(0.5, 0.5, 0.6, 1.0);
        durLbl->fontSize = 12;
        durLbl->textAlignment = CCTextAlignmentRight;
        durLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        durLbl->ignoreTouch = true;
        viewAddSubview(rowView, durLbl);
        
        viewAddSubview(currentMusicSubView, rowView);
        currentY += rowHeight;
    }
    
    currentY += 10;
    
    viewAddSubview(uiMusicContentView, currentMusicSubView);
    
    // Tell the footer to refresh its buttons!
    update_music_pagination_ui();
}

void show_music_menu_overlay(void) {
    if (uiMusicMenuOverlay) {
        viewRemoveFromSuperview(uiMusicMenuOverlay);
        freeViewHierarchy(uiMusicMenuOverlay);
        uiMusicMenuOverlay = NULL;
    }
    
    uiMusicMenuOverlay = viewWithFrame(ccRect(0, 0, SCREEN_W, 480));
    uiMusicMenuOverlay->backgroundColor = color(0, 0, 0, 0.0);
    uiMusicMenuOverlay->tag = TAG_MUSIC_CTX_BG;
    
    int menuW = 210;
    int btnH = 44;
    int menuX = 10;
    int menuY = afb(uiMusicTopBar) + 5; // Perfectly nested under the top bar!
    
    CCView* menuBox = viewWithFrame(ccRect(menuX, menuY, menuW, btnH));
    menuBox->backgroundColor = color(0.15, 0.15, 0.2, 0.95);
    layerSetCornerRadius(menuBox->layer, 8.0);
    menuBox->layer->borderWidth = 1.0;
    menuBox->layer->borderColor = color(0.3, 0.3, 0.4, 1.0);
    menuBox->ignoreTouch = true;
    viewAddSubview(uiMusicMenuOverlay, menuBox);
    
    // Contextual Logic based on current Tab!
    CCView* actionBtn = viewWithFrame(ccRect(menuX, menuY, menuW, btnH));
    actionBtn->backgroundColor = color(0, 0, 0, 0.0);
    
    CCLabel* actionLbl = labelWithFrame(ccRect(15, 0, menuW - 30, btnH));
    actionLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
    actionLbl->fontSize = 14;
    actionLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    actionLbl->ignoreTouch = true;
    
    // Inside show_music_menu_overlay()...
    if (currentMusicTab == MusicTabPlaylists) {
        if (currentSelectedPlaylist == NULL) {
            actionLbl->text = ccs("Create a New Playlist");
            actionBtn->tag = TAG_MUSIC_PL_CREATE;
        } else {
            // We are inside a Playlist Detail View!
            actionLbl->text = ccs("Add Item to Playlist");
            actionBtn->tag = TAG_MUSIC_PL_ADD_ITEM;
        }
    } else {
        actionLbl->text = ccs("Scan System for MP3s");
        actionBtn->tag = TAG_MUSIC_SCAN_BTN;
    }
    
    viewAddSubview(actionBtn, actionLbl);
    viewAddSubview(uiMusicMenuOverlay, actionBtn);
    viewAddSubview(mainWindowView, uiMusicMenuOverlay);
}

void build_music_artists_ui(void) {
    if (currentMusicSubView) {
        viewRemoveFromSuperview(currentMusicSubView);
        freeViewHierarchy(currentMusicSubView);
        currentMusicSubView = NULL;
    }
    
    int currentY = 0;
    int rowHeight = 44;
    int itemCount = 0;
    CCArray* displayData = NULL;
    
    // ==========================================
    // STATE A: MASTER ARTIST LIST
    // ==========================================
    if (currentSelectedArtist == NULL) {
        displayData = music_get_unique_artists();
        itemCount = arrayCount(displayData);
        
        int totalCanvasHeight = (itemCount * rowHeight);
        if (totalCanvasHeight < 440) totalCanvasHeight = 440;
        
        currentMusicSubView = viewWithFrame(ccRect(0, 0, SCREEN_W, totalCanvasHeight));
        currentMusicSubView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
        
        for (int i = 0; i < itemCount; i++) {
            CCString* artistName = arrayObjectAtIndex(displayData, i);
            
            CCView* rowView = viewWithFrame(ccRect(0, currentY, SCREEN_W, rowHeight));
            rowView->tag = TAG_MUSIC_ARTIST_ROW_START + i;
            
            if (i % 2 == 0) rowView->backgroundColor = color(0.1, 0.1, 0.15, 1.0);
            else rowView->backgroundColor = color(0.08, 0.08, 0.12, 0.0);
            
            CCLabel* nameLbl = labelWithFrame(ccRect(15, 0, SCREEN_W - 50, rowHeight));
            // FIX: Pass a distinct copy to the UI!
            nameLbl->text = copyCCString(artistName);
            nameLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
            nameLbl->fontSize = 16;
            nameLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
            nameLbl->ignoreTouch = true;
            viewAddSubview(rowView, nameLbl);
            
            CCLabel* chevronLbl = labelWithFrame(ccRect(SCREEN_W - 30, 0, 20, rowHeight));
            chevronLbl->text = ccs(">");
            chevronLbl->textColor = color(0.4, 0.4, 0.5, 1.0);
            chevronLbl->fontSize = 16;
            chevronLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
            chevronLbl->ignoreTouch = true;
            viewAddSubview(rowView, chevronLbl);
            
            viewAddSubview(currentMusicSubView, rowView);
            currentY += rowHeight;
        }
        
        // This is now 100% safe to call, because the UI labels own their own copies!
        freeCCArray(displayData);
    }
    // ==========================================
    // STATE B: ARTIST DETAIL VIEW (Songs & Albums)
    // ==========================================
    else {
        if (isArtistDetailShowingAlbums) {
            displayData = music_get_albums_for_artist(currentSelectedArtist);
            itemCount = arrayCount(displayData);
        } else {
            itemCount = 0;
            for (int i = 0; i < arrayCount(globalMusicLibrary); i++) {
                CCDictionary* track = arrayObjectAtIndex(globalMusicLibrary, i);
                CCString* trackArtist = dictionaryObjectForKey(track, ccs("Artist"));
                if (trackArtist && stringEqualsString(trackArtist, currentSelectedArtist)) {
                    itemCount++;
                }
            }
        }
        
        int headerHeight = 44 + 40;
        int totalCanvasHeight = headerHeight + (itemCount * rowHeight);
        if (totalCanvasHeight < 440) totalCanvasHeight = 440;
        
        currentMusicSubView = viewWithFrame(ccRect(0, 0, SCREEN_W, totalCanvasHeight));
        currentMusicSubView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
        
        CCView* backHeader = viewWithFrame(ccRect(0, currentY, SCREEN_W, 44));
        backHeader->backgroundColor = color(0.12, 0.12, 0.16, 1.0);
        
        CCView* backBtn = viewWithFrame(ccRect(5, 5, 100, 34));
        backBtn->tag = TAG_MUSIC_ARTIST_BACK;
        CCLabel* backLbl = labelWithFrame(ccRect(10, 0, 90, 34));
        backLbl->text = ccs("< Artists");
        backLbl->textColor = color(0.2, 0.6, 1.0, 1.0);
        backLbl->fontSize = 14;
        backLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        backLbl->ignoreTouch = true;
        viewAddSubview(backBtn, backLbl);
        viewAddSubview(backHeader, backBtn);
        
        CCLabel* titleLbl = labelWithFrame(ccRect(80, 0, SCREEN_W - 160, 44));
        // FIX: NEVER assign a global directly to a label, or freeViewHierarchy will destroy the global!
        titleLbl->text = copyCCString(currentSelectedArtist);
        titleLbl->textColor = color(1, 1, 1, 1);
        titleLbl->fontSize = 16;
        titleLbl->textAlignment = CCTextAlignmentCenter;
        titleLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        titleLbl->ignoreTouch = true;
        viewAddSubview(backHeader, titleLbl);
        
        viewAddSubview(currentMusicSubView, backHeader);
        currentY += 44;
        
        CCView* subTabBar = viewWithFrame(ccRect(0, currentY, SCREEN_W, 40));
        subTabBar->backgroundColor = color(0.1, 0.1, 0.14, 1.0);
        
        int tabW = SCREEN_W / 2;
        
        CCView* songsTab = viewWithFrame(ccRect(0, 0, tabW, 40));
        songsTab->tag = TAG_MUSIC_ARTIST_TAB_SONGS;
        CCLabel* songsLbl = labelWithFrame(ccRect(0, 0, tabW, 40));
        songsLbl->text = ccs("Songs");
        songsLbl->textColor = isArtistDetailShowingAlbums ? color(0.5, 0.5, 0.6, 1.0) : color(1, 1, 1, 1);
        songsLbl->fontSize = 14;
        songsLbl->textAlignment = CCTextAlignmentCenter;
        songsLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        songsLbl->ignoreTouch = true;
        viewAddSubview(songsTab, songsLbl);
        
        if (!isArtistDetailShowingAlbums) {
            CCView* highlight = viewWithFrame(ccRect(0, 38, tabW, 2));
            highlight->backgroundColor = color(0.2, 0.6, 1.0, 1.0);
            viewAddSubview(songsTab, highlight);
        }
        viewAddSubview(subTabBar, songsTab);
        
        CCView* albumsTab = viewWithFrame(ccRect(tabW, 0, tabW, 40));
        albumsTab->tag = TAG_MUSIC_ARTIST_TAB_ALBUMS;
        CCLabel* albumsLbl = labelWithFrame(ccRect(0, 0, tabW, 40));
        albumsLbl->text = ccs("Albums");
        albumsLbl->textColor = isArtistDetailShowingAlbums ? color(1, 1, 1, 1) : color(0.5, 0.5, 0.6, 1.0);
        albumsLbl->fontSize = 14;
        albumsLbl->textAlignment = CCTextAlignmentCenter;
        albumsLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        albumsLbl->ignoreTouch = true;
        viewAddSubview(albumsTab, albumsLbl);
        
        if (isArtistDetailShowingAlbums) {
            CCView* highlight = viewWithFrame(ccRect(0, 38, tabW, 2));
            highlight->backgroundColor = color(0.2, 0.6, 1.0, 1.0);
            viewAddSubview(albumsTab, highlight);
        }
        viewAddSubview(subTabBar, albumsTab);
        
        viewAddSubview(currentMusicSubView, subTabBar);
        currentY += 40;
        
        if (isArtistDetailShowingAlbums) {
            // Draw Albums List
            for (int i = 0; i < itemCount; i++) {
                CCString* albumName = arrayObjectAtIndex(displayData, i);
                CCView* rowView = viewWithFrame(ccRect(0, currentY, SCREEN_W, rowHeight));
                rowView->tag = TAG_MUSIC_ALBUM_ROW_START + i;
                if (i % 2 == 0) rowView->backgroundColor = color(0.1, 0.1, 0.15, 1.0);
                
                CCLabel* nameLbl = labelWithFrame(ccRect(15, 0, SCREEN_W - 30, rowHeight));
                // FIX: Copy string
                nameLbl->text = copyCCString(albumName);
                nameLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
                nameLbl->fontSize = 14;
                nameLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
                nameLbl->ignoreTouch = true;
                viewAddSubview(rowView, nameLbl);
                viewAddSubview(currentMusicSubView, rowView);
                currentY += rowHeight;
            }
            freeCCArray(displayData);
        } else {
            // Draw Songs List
            for (int i = 0; i < arrayCount(globalMusicLibrary); i++) {
                CCDictionary* track = arrayObjectAtIndex(globalMusicLibrary, i);
                CCString* trackArtist = dictionaryObjectForKey(track, ccs("Artist"));
                
                if (trackArtist && stringEqualsString(trackArtist, currentSelectedArtist)) {
                    CCString* trackTitle = dictionaryObjectForKey(track, ccs("Title"));
                    CCString* trackDuration = dictionaryObjectForKey(track, ccs("Duration"));
                    
                    // FIX: Isolate strings
                    CCString* safeTitle = trackTitle ? copyCCString(trackTitle) : ccs("Unknown Track");
                    CCString* safeDuration = trackDuration ? copyCCString(trackDuration) : ccs("-:--");
                    
                    CCView* rowView = viewWithFrame(ccRect(0, currentY, SCREEN_W, rowHeight));
                    rowView->tag = TAG_MUSIC_LIB_ROW_START + i;
                    if (i % 2 == 0) rowView->backgroundColor = color(0.1, 0.1, 0.15, 1.0);
                    
                    CCLabel* titleLbl = labelWithFrame(ccRect(15, 0, SCREEN_W - 80, rowHeight));
                    titleLbl->text = safeTitle;
                    titleLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
                    titleLbl->fontSize = 14;
                    titleLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
                    titleLbl->ignoreTouch = true;
                    viewAddSubview(rowView, titleLbl);
                    
                    CCLabel* durLbl = labelWithFrame(ccRect(SCREEN_W - 65, 0, 50, rowHeight));
                    durLbl->text = safeDuration;
                    durLbl->textColor = color(0.5, 0.5, 0.6, 1.0);
                    durLbl->fontSize = 12;
                    durLbl->textAlignment = CCTextAlignmentRight;
                    durLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
                    durLbl->ignoreTouch = true;
                    viewAddSubview(rowView, durLbl);
                    
                    viewAddSubview(currentMusicSubView, rowView);
                    currentY += rowHeight;
                }
            }
        }
    }
    viewAddSubview(uiMusicContentView, currentMusicSubView);
}

void build_music_playlists_ui(void) {
    if (!globalPlaylists) {
        CCDictionary* savedPls = loadConfigFile(ccs("music/playlists.json"));
        if (savedPls) {
            CCArray* loadedPls = dictionaryObjectForKey(savedPls, ccs("playlists"));
            if (loadedPls) globalPlaylists = loadedPls;
        }
        if (!globalPlaylists) globalPlaylists = array();
    }
    
    if (currentMusicSubView) {
        viewRemoveFromSuperview(currentMusicSubView);
        freeViewHierarchy(currentMusicSubView);
        currentMusicSubView = NULL;
    }
    
    int rowHeight = 44;
    int currentY = 0;
    
    // ==========================================
    // STATE A: MASTER PLAYLIST LIST
    // ==========================================
    if (currentSelectedPlaylist == NULL) {
        int plCount = arrayCount(globalPlaylists);
        totalDataPages = (plCount > 0) ? ((plCount + musicRowsPerPage - 1) / musicRowsPerPage) : 1;
        if (currentDataPage >= totalDataPages) currentDataPage = totalDataPages - 1;
        if (currentDataPage < 0) currentDataPage = 0;
        
        int startIndex = currentDataPage * musicRowsPerPage;
        int endIndex = startIndex + musicRowsPerPage;
        if (endIndex > plCount) endIndex = plCount;
        
        int totalCanvasHeight = 25 + (musicRowsPerPage * rowHeight);
        currentMusicSubView = viewWithFrame(ccRect(0, 0, SCREEN_W, totalCanvasHeight));
        currentMusicSubView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
        
        // --- COLUMN HEADERS ---
        CCView* headerRow = viewWithFrame(ccRect(0, currentY, SCREEN_W, 20));
        CCLabel* headTitle = labelWithFrame(ccRect(15, 0, 200, 20));
        headTitle->text = copyCCString(ccs("PLAYLIST NAME"));
        headTitle->textColor = color(0.4, 0.4, 0.5, 1.0);
        headTitle->fontSize = 10;
        headTitle->ignoreTouch = true;
        viewAddSubview(headerRow, headTitle);
        viewAddSubview(currentMusicSubView, headerRow);
        currentY += 25;
        
        // --- PLAYLIST ROWS ---
        for (int i = startIndex; i < endIndex; i++) {
            CCDictionary* playlist = arrayObjectAtIndex(globalPlaylists, i);
            CCString* plName = dictionaryObjectForKey(playlist, ccs("Name"));
            CCArray* plTracks = dictionaryObjectForKey(playlist, ccs("Tracks"));
            
            int trackCount = plTracks ? arrayCount(plTracks) : 0;
            char countStr[16];
            snprintf(countStr, sizeof(countStr), "%d Tracks", trackCount);
            
            CCView* rowView = viewWithFrame(ccRect(0, currentY, SCREEN_W, rowHeight));
            rowView->tag = TAG_MUSIC_PLAYLIST_ROW_START + i;
            if (i % 2 == 0) rowView->backgroundColor = color(0.1, 0.1, 0.15, 1.0);
            
            CCLabel* titleLbl = labelWithFrame(ccRect(15, 0, SCREEN_W - 100, rowHeight));
            titleLbl->text = plName ? copyCCString(plName) : copyCCString(ccs("Unknown"));
            titleLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
            titleLbl->fontSize = 14;
            titleLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
            titleLbl->ignoreTouch = true;
            viewAddSubview(rowView, titleLbl);
            
            CCLabel* countLbl = labelWithFrame(ccRect(SCREEN_W - 85, 0, 70, rowHeight));
            countLbl->text = copyCCString(ccs(countStr));
            countLbl->textColor = color(0.5, 0.5, 0.6, 1.0);
            countLbl->fontSize = 12;
            countLbl->textAlignment = CCTextAlignmentRight;
            countLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
            countLbl->ignoreTouch = true;
            viewAddSubview(rowView, countLbl);
            
            viewAddSubview(currentMusicSubView, rowView);
            currentY += rowHeight;
        }
    }
    // ==========================================
    // STATE B: PLAYLIST DETAIL VIEW
    // ==========================================
    else {
        CCDictionary* activePl = get_current_playlist_dict();
        CCArray* plTracks = activePl ? dictionaryObjectForKey(activePl, ccs("Tracks")) : NULL;
        int trackCount = plTracks ? arrayCount(plTracks) : 0;
        
        totalDataPages = (trackCount > 0) ? ((trackCount + musicRowsPerPage - 1) / musicRowsPerPage) : 1;
        if (currentDataPage >= totalDataPages) currentDataPage = totalDataPages - 1;
        if (currentDataPage < 0) currentDataPage = 0;
        
        int startIndex = currentDataPage * musicRowsPerPage;
        int endIndex = startIndex + musicRowsPerPage;
        if (endIndex > trackCount) endIndex = trackCount;
        
        int headerHeight = 44 + 5; // Back button row
        int totalCanvasHeight = headerHeight + (musicRowsPerPage * rowHeight);
        currentMusicSubView = viewWithFrame(ccRect(0, 0, SCREEN_W, totalCanvasHeight));
        currentMusicSubView->backgroundColor = color(0.08, 0.08, 0.12, 1.0);
        
        // --- BACK BUTTON ROW ---
        CCView* backHeader = viewWithFrame(ccRect(0, currentY, SCREEN_W, 44));
        backHeader->backgroundColor = color(0.12, 0.12, 0.16, 1.0);
        
        CCView* backBtn = viewWithFrame(ccRect(5, 5, 100, 34));
        backBtn->tag = TAG_MUSIC_PL_BACK;
        CCLabel* backLbl = labelWithFrame(ccRect(10, 0, 90, 34));
        backLbl->text = copyCCString(ccs("< Playlists"));
        backLbl->textColor = color(0.2, 0.6, 1.0, 1.0);
        backLbl->fontSize = 14;
        backLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        backLbl->ignoreTouch = true;
        viewAddSubview(backBtn, backLbl);
        viewAddSubview(backHeader, backBtn);
        
        CCLabel* titleLbl = labelWithFrame(ccRect(80, 0, SCREEN_W - 160, 44));
        titleLbl->text = copyCCString(currentSelectedPlaylist);
        titleLbl->textColor = color(1, 1, 1, 1);
        titleLbl->fontSize = 16;
        titleLbl->textAlignment = CCTextAlignmentCenter;
        titleLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
        titleLbl->ignoreTouch = true;
        viewAddSubview(backHeader, titleLbl);
        viewAddSubview(currentMusicSubView, backHeader);
        currentY += 44 + 5;
        
        // --- TRACK ROWS ---
        for (int i = startIndex; i < endIndex; i++) {
            CCString* savedPath = arrayObjectAtIndex(plTracks, i);
            
            // Cross-reference the master library to get Title/Artist
            CCString* titleStr = ccs("Unknown");
            CCString* durStr = ccs("-:--");
            
            for (int j = 0; j < arrayCount(globalMusicLibrary); j++) {
                CCDictionary* libTrack = arrayObjectAtIndex(globalMusicLibrary, j);
                CCString* libPath = dictionaryObjectForKey(libTrack, ccs("Path"));
                if (libPath && stringEqualsString(libPath, savedPath)) {
                    CCString* t = dictionaryObjectForKey(libTrack, ccs("Title"));
                    CCString* d = dictionaryObjectForKey(libTrack, ccs("Duration"));
                    if (t) titleStr = t;
                    if (d) durStr = d;
                    break;
                }
            }
            
            CCView* rowView = viewWithFrame(ccRect(0, currentY, SCREEN_W, rowHeight));
            // We tag it with the playlist track index! (Requires a new start tag later for playback)
            rowView->tag = TAG_MUSIC_LIB_ROW_START + i;
            if (i % 2 == 0) rowView->backgroundColor = color(0.1, 0.1, 0.15, 1.0);
            
            CCLabel* titleLbl = labelWithFrame(ccRect(15, 0, SCREEN_W - 80, rowHeight));
            titleLbl->text = copyCCString(titleStr);
            titleLbl->textColor = color(0.9, 0.9, 0.95, 1.0);
            titleLbl->fontSize = 14;
            titleLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
            titleLbl->ignoreTouch = true;
            viewAddSubview(rowView, titleLbl);
            
            CCLabel* durLbl = labelWithFrame(ccRect(SCREEN_W - 65, 0, 50, rowHeight));
            durLbl->text = copyCCString(durStr);
            durLbl->textColor = color(0.5, 0.5, 0.6, 1.0);
            durLbl->fontSize = 12;
            durLbl->textAlignment = CCTextAlignmentRight;
            durLbl->textVerticalAlignment = CCTextVerticalAlignmentCenter;
            durLbl->ignoreTouch = true;
            viewAddSubview(rowView, durLbl);
            
            viewAddSubview(currentMusicSubView, rowView);
            currentY += rowHeight;
        }
    }
    
    viewAddSubview(uiMusicContentView, currentMusicSubView);
    update_music_pagination_ui();
}

void show_new_playlist_dialog(void) {
    if (uiNewPlaylistDialog) {
        viewRemoveFromSuperview(uiNewPlaylistDialog);
        freeViewHierarchy(uiNewPlaylistDialog);
        uiNewPlaylistDialog = NULL;
    }
    
    // Placed near the top so the OS keyboard has room below it
    uiNewPlaylistDialog = viewWithFrame(ccRect(20, 30, 280, 130));
    uiNewPlaylistDialog->backgroundColor = color(0.18, 0.18, 0.18, 1.0);
    layerSetCornerRadius(uiNewPlaylistDialog->layer, 8.0);
    
    // Title
    CCLabel* title = labelWithFrame(ccRect(10, 10, 260, 20));
    title->text = copyCCString(ccs("Create New Playlist"));
    title->fontSize = 16;
    title->textColor = color(1.0, 1.0, 1.0, 1.0);
    title->ignoreTouch = true;
    viewAddSubview(uiNewPlaylistDialog, title);
    
    // Pseudo-Text Field (The Target Label for the OS Keyboard)
    uiNewPlaylistInput = labelWithFrame(ccRect(10, 40, 260, 30));
    uiNewPlaylistInput->text = copyCCString(ccs("New Playlist"));
    uiNewPlaylistInput->fontSize = 16;
    uiNewPlaylistInput->textColor = color(0.0, 0.0, 0.0, 1.0);
    uiNewPlaylistInput->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    uiNewPlaylistInput->ignoreTouch = true;
    
    // Give it a white background so it visually looks like an input box
    uiNewPlaylistInput->view->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    viewAddSubview(uiNewPlaylistDialog, uiNewPlaylistInput);
    
    // Cancel Button
    CCView* btnCancel = viewWithFrame(ccRect(10, 85, 120, 35));
    btnCancel->backgroundColor = color(0.4, 0.4, 0.4, 1.0);
    btnCancel->tag = TAG_MUSIC_PL_CREATE_DIALOG_CANCEL;
    layerSetCornerRadius(btnCancel->layer, 4.0);
    
    CCLabel* lblCancel = labelWithFrame(ccRect(0, 0, 120, 35));
    lblCancel->text = copyCCString(ccs("Cancel"));
    lblCancel->fontSize = 16;
    lblCancel->textAlignment = CCTextAlignmentCenter;
    lblCancel->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    lblCancel->textColor = color(1.0, 1.0, 1.0, 1.0);
    lblCancel->ignoreTouch = true;
    viewAddSubview(btnCancel, lblCancel);
    viewAddSubview(uiNewPlaylistDialog, btnCancel);
    
    // Create Button
    CCView* btnCreate = viewWithFrame(ccRect(150, 85, 120, 35));
    btnCreate->backgroundColor = color(0.0, 0.47, 1.0, 1.0); // iOS Blue
    btnCreate->tag = TAG_MUSIC_PL_CREATE_DIALOG_CONFIRM;
    layerSetCornerRadius(btnCreate->layer, 4.0);
    
    CCLabel* lblCreate = labelWithFrame(ccRect(0, 0, 120, 35));
    lblCreate->text = copyCCString(ccs("Create"));
    lblCreate->fontSize = 16;
    lblCreate->textAlignment = CCTextAlignmentCenter;
    lblCreate->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    lblCreate->textColor = color(1.0, 1.0, 1.0, 1.0);
    lblCreate->ignoreTouch = true;
    viewAddSubview(btnCreate, lblCreate);
    viewAddSubview(uiNewPlaylistDialog, btnCreate);
    
    viewAddSubview(mainWindowView, uiNewPlaylistDialog);
    
    // Summon the OS Keyboard and point it at our Label
    setup_keyboard_ui(uiNewPlaylistInput); // Uncomment to link to your exact keyboard function
}

void handle_music_touch(int x, int y, int touchState) {
    
    // ==========================================
    // FINGER IS TOUCHING THE SCREEN (DOWN / HELD)
    // ==========================================
    if (touchState == 1) {
        FreeOSLogI("handle_music_touch", "Starting setup_files_ui App");
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
            if (touchStartTime != 0 && !long_press_fired && !uiMusicContextMenu) {
                uint64_t now = esp_timer_get_time() / 1000;
                
                if ((now - touchStartTime) >= 1000) { // EXACTLY 1 SECOND
                    long_press_fired = true;
                    
                    CCView* target = find_subview_at_point_recursive(mainWindowView, touchStartX, touchStartY);
                    
                    // 1. Did they long-press a track row in the library?
                    if (target && target->tag >= TAG_MUSIC_LIB_ROW_START && target->tag < TAG_MUSIC_LIB_CHECK_START) {
                        int trackIndex = target->tag - TAG_MUSIC_LIB_ROW_START;
                        show_music_context_menu(MusicContextTypeTrack, trackIndex, touchStartX, touchStartY);
                        update_full_ui();
                    }
                    // TODO: Add playlist row long press detection here
                }
            }
        }
    }
    
    // ==========================================
    // FINGER LIFTED (RELEASED)
    // ==========================================
    if (touchState == 0) {
        if (!is_pressing) return; // Ignore if we are already released
        is_pressing = false;      // Reset state for next tap
        
        // If they scrolled, OR if the 1-second long press already fired, DO NOTHING on release!
        if (touchStartTime == 0 || long_press_fired) {
            long_press_fired = false; // Clean up
            return;
        }
        
        // --- If we reach here, it was a valid SHORT TAP! ---
        int tapX = touchStartX;
        int tapY = touchStartY;
        
        // 0. Handle Context Menu Taps (Highest Z-Index)
        if (uiMusicContextMenu) {
            CCView* target = find_subview_at_point_recursive(uiMusicContextMenu, tapX, tapY);
            
            if (target && target->tag >= TAG_MUSIC_CTX_TRACK_PLAY && target->tag <= TAG_MUSIC_CTX_TRACK_DEL) {
                int baseTag = target->tag % 100000;
                int trackIndex = target->tag / 100000;
                CCDictionary* trackData = arrayObjectAtIndex(globalMusicLibrary, trackIndex);
                
                if (baseTag == TAG_MUSIC_CTX_TRACK_PLAY) {
                    CCString* path = dictionaryObjectForKey(trackData, ccs("Path"));
                    // trigger_i2s_playback(path);
                }
            }
            
            // Detach and free the context menu regardless of where they tapped
            viewRemoveFromSuperview(uiMusicContextMenu);
            freeViewHierarchy(uiMusicContextMenu);
            uiMusicContextMenu = NULL;
            update_full_ui();
            return; // Block underneath UI from receiving this tap
        }
        
        // 0.5 Handle Top-Left Menu Overlay Taps
        if (uiMusicMenuOverlay) {
            CCView* target = find_subview_at_point(uiMusicMenuOverlay, tapX, tapY);
            
            if (target) {
                if (target->tag == TAG_MUSIC_SCAN_BTN) {
                    // Destroy the menu first
                    viewRemoveFromSuperview(uiMusicMenuOverlay);
                    freeViewHierarchy(uiMusicMenuOverlay);
                    uiMusicMenuOverlay = NULL;
                    
                    // Run the heavy SD card scan
                    music_trigger_full_scan();
                    
                    // Rebuild the library UI to show new tracks
                    build_music_library_ui();
                    update_full_ui();
                    return;
                }
                else if (target->tag == TAG_MUSIC_PL_CREATE) {
                    // Destroy the menu overlay first
                    viewRemoveFromSuperview(uiMusicMenuOverlay);
                    freeViewHierarchy(uiMusicMenuOverlay);
                    uiMusicMenuOverlay = NULL;
                    
                    // Trigger the dialog!
                    show_new_playlist_dialog();
                    update_full_ui();
                    return;
                }
                else if (target->tag == TAG_MUSIC_PL_ADD_ITEM) {
                    viewRemoveFromSuperview(uiMusicMenuOverlay);
                    freeViewHierarchy(uiMusicMenuOverlay);
                    uiMusicMenuOverlay = NULL;
                    
                    isMusicLibrarySelectionMode = true;
                    currentDataPage = 0;
                    build_music_library_ui(); // Switch Canvas to Library Mode!
                    update_full_ui();
                    return;
                }
            }
            
            // If they clicked the background (dismiss), just destroy the menu
            viewRemoveFromSuperview(uiMusicMenuOverlay);
            freeViewHierarchy(uiMusicMenuOverlay);
            uiMusicMenuOverlay = NULL;
            update_full_ui();
            return; // Block underneath UI from receiving this tap
        }
        
        // --- 0.7 Handle New Playlist Dialog Taps ---
        if (uiNewPlaylistDialog) {
            CCView* target = find_subview_at_point(uiNewPlaylistDialog, tapX, tapY);
            
            if (target) {
                if (target->tag == TAG_MUSIC_PL_CREATE_DIALOG_CANCEL) {
                    // Close dialog and keyboard
                    hide_keyboard_ui();
                    viewRemoveFromSuperview(uiNewPlaylistDialog);
                    freeViewHierarchy(uiNewPlaylistDialog);
                    uiNewPlaylistDialog = NULL;
                     // Add your keyboard dismiss function here
                    update_full_ui();
                    return;
                }
                else if (target->tag == TAG_MUSIC_PL_CREATE_DIALOG_CONFIRM) {
                    // 1. Grab the text from the keyboard label
                    hide_keyboard_ui();
                    CCString* finalName = uiNewPlaylistInput->text;
                    
                    // 2. Create the new Playlist Dictionary
                    CCDictionary* newPlaylist = dictionary();
                    dictionarySetObjectForKey(newPlaylist, copyCCString(finalName), ccs("Name"));
                    dictionarySetObjectForKey(newPlaylist, array(), ccs("Tracks")); // Empty tracks array
                    
                    // 3. Add to globals and save to SD card
                    if (!globalPlaylists) globalPlaylists = array();
                    arrayAddObject(globalPlaylists, newPlaylist);
                    
                    CCDictionary* wrapper = dictionary();
                    dictionarySetObjectForKey(wrapper, globalPlaylists, ccs("playlists"));
                    saveConfigFile(ccs("music/playlists.json"), wrapper);
                    
                    // 4. Cleanup UI
                    viewRemoveFromSuperview(uiNewPlaylistDialog);
                    freeViewHierarchy(uiNewPlaylistDialog);
                    uiNewPlaylistDialog = NULL;
                    // teardown_keyboard_ui();
                    
                    
                    // 5. Rebuild the list to show the new playlist
                    build_music_playlists_ui();
                    update_full_ui();
                    return;
                }
            }
            return; // Block underneath UI from receiving taps while dialog is open
        }
        
        // 1. Standard Short Tap Routing
        CCView* tappedView = find_subview_at_point_recursive(mainWindowView, tapX, tapY);
        FreeOSLogI("handle_music_touch", "%d %d %d %d", (tappedView == NULL)? 1 : 0, tappedView->tag, tapX, tapY);
        if (!tappedView) return;
        
        switch (tappedView->tag) {
            case TAG_MUSIC_NAV_LIBRARY:
                currentMusicTab = MusicTabLibrary;
                currentDataPage = 0; // Reset pagination!
                isMusicLibrarySelectionMode = false;
                if (currentSelectedArtist) {
                    freeCCString(currentSelectedArtist);
                    currentSelectedArtist = NULL;
                }
                build_music_library_ui();
                update_full_ui();
                break;
                
            case TAG_MUSIC_SCAN_BTN:
            {
                // 1. Change the button text so the user knows it's working
                CCView* btn = find_subview_at_point_recursive(mainWindowView, tapX, tapY);
                if (btn && btn->subviews->count > 0) {
                    CCLabel* lbl = (CCLabel*)arrayObjectAtIndex(btn->subviews, 0);
                    lbl->text = ccs("Scanning drives...");
                    update_full_ui(); // Force the screen to draw the new text
                }
                
                // 2. Run the heavy SD card scan
                music_trigger_full_scan();
                
                // 3. Rebuild the list with the new songs
                build_music_library_ui();
                update_full_ui();
            }
                break;
                
            case TAG_MUSIC_NAV_PLAYLISTS:
                currentMusicTab = MusicTabPlaylists;
                currentDataPage = 0;
                build_music_playlists_ui(); // Route to the new builder!
                update_full_ui();
                break;
                
            case TAG_MUSIC_PL_CREATE:
                // Triggered from the "Create a New Playlist" overlay menu
                FreeOSLogI("TAG_MUSIC_PL_CREATE", "TAG_MUSIC_PL_CREATE");
                
                // Destroy the menu overlay first
                if (uiMusicMenuOverlay) {
                    viewRemoveFromSuperview(uiMusicMenuOverlay);
                    freeViewHierarchy(uiMusicMenuOverlay);
                    uiMusicMenuOverlay = NULL;
                }
                
                show_new_playlist_dialog();
                update_full_ui();
                break;
                
            case TAG_MUSIC_NAV_ARTISTS:
                currentMusicTab = MusicTabArtists;
                currentDataPage = 0;
                if (currentSelectedArtist) {
                    freeCCString(currentSelectedArtist);
                    currentSelectedArtist = NULL;
                }
                build_music_artists_ui();
                update_full_ui();
                break;
                
            case TAG_MUSIC_ARTIST_BACK:
                if (currentSelectedArtist) {
                    freeCCString(currentSelectedArtist);
                    currentSelectedArtist = NULL;
                }
                build_music_artists_ui();
                update_full_ui();
                break;
                
            case TAG_MUSIC_ARTIST_TAB_SONGS:
                isArtistDetailShowingAlbums = false;
                build_music_artists_ui();
                update_full_ui();
                break;
                
            case TAG_MUSIC_ARTIST_TAB_ALBUMS:
                isArtistDetailShowingAlbums = true;
                build_music_artists_ui();
                update_full_ui();
                break;
                
            case TAG_MUSIC_TOGGLE_LOOP:
                isMusicLooping = !isMusicLooping;
                uiMusicLoopIcon->alpha = isMusicLooping ? 1.0f : 0.3f;
                // updateDisplayArea(uiMusicLoopIcon->frame);
                break;
                
            case TAG_MUSIC_TOGGLE_SHUFFLE:
                isMusicShuffling = !isMusicShuffling;
                uiMusicShuffleIcon->alpha = isMusicShuffling ? 1.0f : 0.3f;
                // updateDisplayArea(uiMusicShuffleIcon->frame);
                break;
                
            case TAG_MUSIC_MENU_BTN:
                show_music_menu_overlay();
                update_full_ui();
                break;
                
            case TAG_MUSIC_PAGE_PREV:
                if (currentDataPage > 0) {
                    currentDataPage--;
                    if (currentMusicTab == MusicTabLibrary) build_music_library_ui();
                    else if (currentMusicTab == MusicTabArtists) build_music_artists_ui();
                    // else if (currentMusicTab == MusicTabPlaylists) build_music_playlists_ui();
                    update_full_ui();
                }
                break;
                
            case TAG_MUSIC_PAGE_NEXT:
                if (currentDataPage < totalDataPages - 1) {
                    currentDataPage++;
                    if (currentMusicTab == MusicTabLibrary) build_music_library_ui();
                    else if (currentMusicTab == MusicTabArtists) build_music_artists_ui();
                    // else if (currentMusicTab == MusicTabPlaylists) build_music_playlists_ui();
                    update_full_ui();
                }
                break;
                
                // 2. In the standard short tap switch (tappedView->tag):
            case TAG_MUSIC_PLAYLIST_ROW_START ... (TAG_MUSIC_PLAYLIST_ROW_START + 9999):
            {
                int plIndex = tappedView->tag - TAG_MUSIC_PLAYLIST_ROW_START;
                CCDictionary* pl = arrayObjectAtIndex(globalPlaylists, plIndex);
                CCString* plName = dictionaryObjectForKey(pl, ccs("Name"));
                
                if (currentSelectedPlaylist) freeCCString(currentSelectedPlaylist);
                currentSelectedPlaylist = copyCCString(plName);
                
                currentDataPage = 0;
                build_music_playlists_ui();
                update_full_ui();
            }
                break;
                
            case TAG_MUSIC_PL_BACK:
            case TAG_MUSIC_PL_ADD_DONE:
                // This button brings us back from Selection Mode OR Detail View to the previous list
                isMusicLibrarySelectionMode = false;
                if (tappedView->tag == TAG_MUSIC_PL_BACK) {
                    if (currentSelectedPlaylist) freeCCString(currentSelectedPlaylist);
                    currentSelectedPlaylist = NULL;
                }
                currentDataPage = 0;
                build_music_playlists_ui(); // Always returns to Playlists
                update_full_ui();
                break;
                
                
                // 3. Modifying the Checkbox Tap Logic (Dynamic Saving)
            case TAG_MUSIC_LIB_CHECK_START ... (TAG_MUSIC_LIB_CHECK_START + 9999):
            {
                int trackIndex = tappedView->tag - TAG_MUSIC_LIB_CHECK_START;
                CCDictionary* trackData = arrayObjectAtIndex(globalMusicLibrary, trackIndex);
                CCString* trackPath = dictionaryObjectForKey(trackData, ccs("Path"));
                
                CCDictionary* activePl = get_current_playlist_dict();
                if (activePl && trackPath) {
                    CCArray* tracks = dictionaryObjectForKey(activePl, ccs("Tracks"));
                    if (!tracks) {
                        tracks = array();
                        dictionarySetObjectForKey(activePl, tracks, ccs("Tracks"));
                    }
                    
                    // Add or Remove
                    bool found = false;
                    for (int i = 0; i < arrayCount(tracks); i++) {
                        CCString* existing = arrayObjectAtIndex(tracks, i);
                        if (stringEqualsString(existing, trackPath)) {
                            arrayRemoveObject(tracks, existing); // Untick
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) arrayAddObject(tracks, copyCCString(trackPath)); // Tick
                    
                    // Save instantly to JSON!
                    CCDictionary* wrapper = dictionary();
                    dictionarySetObjectForKey(wrapper, globalPlaylists, ccs("playlists"));
                    saveConfigFile(ccs("music/playlists.json"), wrapper);
                    
                    // Visually toggle without full redraw
                    tappedView->backgroundColor = found ? color(0.08, 0.08, 0.12, 1.0) : color(0.2, 0.6, 1.0, 1.0);
                    // updateDisplayArea(tappedView->frame);
                }
            }
                break;
                
        }
        
        // --- Dynamic List Interactions ---
        if (tappedView->tag >= TAG_MUSIC_LIB_ROW_START && tappedView->tag < TAG_MUSIC_CTX_BG) {
            
            // 1. Determine which track was tapped, regardless of whether they hit the row or the tiny checkbox
            int trackIndex = -1;
            if (tappedView->tag >= TAG_MUSIC_LIB_CHECK_START) {
                trackIndex = tappedView->tag - TAG_MUSIC_LIB_CHECK_START;
            } else {
                trackIndex = tappedView->tag - TAG_MUSIC_LIB_ROW_START;
            }
            
            // 2. SELECTION MODE ROUTING (Add/Remove from Playlist)
            if (isMusicLibrarySelectionMode) {
                CCDictionary* trackData = arrayObjectAtIndex(globalMusicLibrary, trackIndex);
                CCString* trackPath = dictionaryObjectForKey(trackData, ccs("Path"));
                
                CCDictionary* activePl = get_current_playlist_dict();
                
                if (activePl && trackPath) {
                    CCArray* tracks = dictionaryObjectForKey(activePl, ccs("Tracks"));
                    if (!tracks) {
                        tracks = array();
                        dictionarySetObjectForKey(activePl, tracks, ccs("Tracks"));
                    }
                    
                    // Toggle Logic: Add or Remove
                    bool found = false;
                    for (int i = 0; i < arrayCount(tracks); i++) {
                        CCString* existing = arrayObjectAtIndex(tracks, i);
                        if (stringEqualsString(existing, trackPath)) {
                            // It's already in the playlist -> Remove it
                            arrayRemoveObject(tracks, existing);
                            found = true;
                            break;
                        }
                    }
                    
                    // Not in the playlist -> Add it
                    if (!found) {
                        arrayAddObject(tracks, copyCCString(trackPath));
                    }
                    
                    // Save instantly to JSON so state is permanently locked
                    CCDictionary* wrapper = dictionary();
                    dictionarySetObjectForKey(wrapper, globalPlaylists, ccs("playlists"));
                    saveConfigFile(ccs("music/playlists.json"), wrapper);
                    
                    // Force a UI rebuild so the checkbox visually updates
                    // (This is much safer than hunting for the checkbox pointer!)
                    build_music_library_ui();
                    update_full_ui();
                }
                return;
            }
            
            // 3. NORMAL MODE ROUTING (Play the Song)
            else {
                if (tappedView->tag >= TAG_MUSIC_LIB_ROW_START && tappedView->tag < TAG_MUSIC_LIB_CHECK_START) {
                    CCDictionary* trackData = arrayObjectAtIndex(globalMusicLibrary, trackIndex);
                    CCString* path = dictionaryObjectForKey(trackData, ccs("Path"));
                    
                    // trigger_i2s_playback(path);
                    printf("PLAYING TRACK: %s\n", cStringOfString(path));
                }
            }
        }
        else if (tappedView->tag >= TAG_MUSIC_ARTIST_ROW_START && tappedView->tag < TAG_MUSIC_ALBUM_ROW_START) {
            int artistIndex = tappedView->tag - TAG_MUSIC_ARTIST_ROW_START;
            CCArray* uniqueArtists = music_get_unique_artists();
            CCString* tappedArtist = arrayObjectAtIndex(uniqueArtists, artistIndex);
            
            currentSelectedArtist = copyCCString(tappedArtist);
            freeCCArray(uniqueArtists);
            
            build_music_artists_ui();
            update_full_ui();
        }
        else if (tappedView->tag >= TAG_MUSIC_LIB_CHECK_START && tappedView->tag < TAG_MUSIC_CTX_BG) {
            if (tappedView->backgroundColor->r < 0.5) {
                tappedView->backgroundColor = color(0.2, 0.6, 1.0, 1.0); // Active
            } else {
                tappedView->backgroundColor = color(0.08, 0.08, 0.12, 1.0); // Inactive
            }
            // updateDisplayArea(tappedView->frame);
        }
    }
}
