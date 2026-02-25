//
//  ColorWheel.h
//  
//
//  Created by Chris Galzerano on 2/22/26.
//

#ifndef COLOR_WHEEL_H
#define COLOR_WHEEL_H

#include "ObjectiveCC.h"
#include "CPUGraphics.h"

#define TAG_CCCOLOR_PICKER_THUMB 8889

// The buffer we will actually give to the CCFramebufferView
extern Framebuffer* g_displayColorWheelFb;

// Setup and Update
void initColorWheelIfNeeded(void);
void updateColorWheelBrightness(float brightness);
CCView* colorPickerView(int x, int y);

// Touch Math
CCColor* getColorFromWheelTouch(int localX, int localY, int width, int height);

#endif
