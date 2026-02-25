//
//  PaintApp.h
//  
//
//  Created by Chris Galzerano on 2/22/26.
//

#ifndef PAINT_APP_H
#define PAINT_APP_H

#include "ObjectiveCC.h"
#include "CPUGraphics.h"

// --- MAIN TOOLBAR TAGS ---
#define TAG_PAINT_PEN           1001
#define TAG_PAINT_SHAPES        1002
#define TAG_PAINT_BUCKET        1003
#define TAG_PAINT_TEXT          1004
#define TAG_PAINT_COLOR         1005
#define TAG_PAINT_PROPERTIES    1006
#define TAG_PAINT_FILEMENU      1007

// --- SHAPES MENU TAGS ---
#define TAG_SHAPE_LINE          2001
#define TAG_SHAPE_RECT          2002
#define TAG_SHAPE_CIRCLE        2003
#define TAG_SHAPE_POLYGON       2004

// --- FILE MENU TAGS ---
#define TAG_FILE_NEW            3001
#define TAG_FILE_OPEN           3002
#define TAG_FILE_SAVE           3003
#define TAG_FILE_SAVEAS         3004
#define TAG_FILE_COPY           3005
#define TAG_FILE_PASTE          3006

#define TAG_CANVAS              7000

void setup_paint_ui(void);
void handle_paint_touch(int x, int y, int type);


#endif
