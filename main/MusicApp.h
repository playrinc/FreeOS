//
//  MusicApp.h
//  
//
//  Created by Chris Galzerano on 2/26/26.
//

#ifndef MusicApp_h
#define MusicApp_h

#include <stdio.h>

void update_music_progress(float percentage);

void build_music_library_ui(void);
void setup_music_app(void);

void handle_music_touch(int x, int y, int touchState);

#endif /* MusicApp_h */
