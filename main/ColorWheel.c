//
//  ColorWheel.c
//  
//
//  Created by Chris Galzerano on 2/22/26.
//

#include "ColorWheel.h"
#include <math.h>

// The untouchable 100% brightness original in PSRAM
static Framebuffer* g_masterColorWheelFb = NULL;

// The mutated version sent to the UI
Framebuffer* g_displayColorWheelFb = NULL;


CCView* colorPickerView(int x, int y) {
    // 1. Main Container Background
    CCRect* containerRect = ccRect(x, y, 240, 320);
    CCView* container = viewWithFrame(containerRect);
    container->backgroundColor = color(0.15, 0.15, 0.15, 1.0); // Dark Gray
    
    // 2. Ensure the master and display buffers are built in PSRAM
    initColorWheelIfNeeded();

    // 3. Create our generic Framebuffer View and give it the Display buffer
    CCRect* wheelRect = ccRect(20, 20, 200, 200);
    CCFramebufferView* wheelFbView = framebufferViewWithFrame(wheelRect);
    framebufferViewSetFramebuffer(wheelFbView, g_displayColorWheelFb);
    
    viewAddSubview(container, wheelFbView);

    // 4. The Brightness Slider Track
    int trackX = 20;
    int trackY = 250;
    int trackW = 200;
    int trackH = 4;
    
    CCRect* trackRect = ccRect(trackX, trackY, trackW, trackH);
    CCView* sliderTrack = viewWithFrame(trackRect);
    sliderTrack->backgroundColor = color(0.5, 0.5, 0.5, 1.0);
    viewAddSubview(container, sliderTrack);

    // 5. The 10 Tick Marks (Spaced evenly across the track)
    int tickSpacing = trackW / 9; // 9 gaps for 10 ticks
    for (int i = 0; i < 10; i++) {
        int tickX = trackX + (i * tickSpacing);
        // Draw tick slightly taller than the track
        CCRect* tickRect = ccRect(tickX - 1, trackY - 4, 2, 12);
        CCView* tick = viewWithFrame(tickRect);
        tick->backgroundColor = color(0.7, 0.7, 0.7, 1.0);
        viewAddSubview(container, tick);
    }

    // 6. The Draggable Slider Thumb
    // Default position at far right (1.0 brightness)
    CCRect* thumbRect = ccRect(trackX + trackW - 10, trackY - 10, 20, 24);
    CCView* sliderThumb = viewWithFrame(thumbRect);
    sliderThumb->backgroundColor = color(1.0, 1.0, 1.0, 1.0);
    sliderThumb->tag = TAG_CCCOLOR_PICKER_THUMB;
    viewAddSubview(container, sliderThumb);

    return container;
}

// Fast C function to convert HSV to RGB
static void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b) {
    if (s == 0.0f) { *r = *g = *b = v; return; }
    int i = (int)(h * 6.0f);
    float f = (h * 6.0f) - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    i = i % 6;
    switch (i) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        case 5: *r = v; *g = p; *b = q; break;
    }
}

// Internal helper to build the initial circle
static Framebuffer* createColorWheelFramebuffer(int width, int height) {
    Framebuffer* fb = (Framebuffer*)cc_safe_alloc(1, sizeof(Framebuffer));
    fb->displayWidth = width;
    fb->displayHeight = height;
    fb->colorMode = COLOR_MODE_BGR888;
    fb->pixelData = heap_caps_malloc(width * height * 3, MALLOC_CAP_SPIRAM);
    
    uint8_t* pixels = (uint8_t*)fb->pixelData;
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float maxRadius = (cx < cy) ? cx : cy;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float distance = sqrt(dx*dx + dy*dy);
            
            int idx = (y * width + x) * 3;
            
            if (distance <= maxRadius) {
                float theta = atan2(dy, dx);
                float hue = (theta + M_PI) / (2.0f * M_PI);
                float saturation = distance / maxRadius;
                
                float r, g, b;
                hsv_to_rgb(hue, saturation, 1.0f, &r, &g, &b);
                
                pixels[idx]   = (uint8_t)(r * 255.0f); // R
                pixels[idx+1] = (uint8_t)(g * 255.0f); // G
                pixels[idx+2] = (uint8_t)(b * 255.0f); // B
            } else {
                // Transparent corners
                pixels[idx] = 0; pixels[idx+1] = 0; pixels[idx+2] = 0;
            }
        }
    }
    return fb;
}

void initColorWheelIfNeeded(void) {
    if (!g_masterColorWheelFb) {
        // 1. Create the pristine master copy
        g_masterColorWheelFb = createColorWheelFramebuffer(200, 200);
        
        // 2. Allocate the display copy
        g_displayColorWheelFb = (Framebuffer*)cc_safe_alloc(1, sizeof(Framebuffer));
        g_displayColorWheelFb->displayWidth = 200;
        g_displayColorWheelFb->displayHeight = 200;
        g_displayColorWheelFb->colorMode = COLOR_MODE_BGR888;
        g_displayColorWheelFb->pixelData = heap_caps_malloc(200 * 200 * 3, MALLOC_CAP_SPIRAM);
        
        // 3. Populate the display copy at 100% brightness
        updateColorWheelBrightness(1.0f);
    }
}

void updateColorWheelBrightness(float brightness) {
    if (!g_masterColorWheelFb || !g_displayColorWheelFb) return;
    
    uint8_t* src = (uint8_t*)g_masterColorWheelFb->pixelData;
    uint8_t* dst = (uint8_t*)g_displayColorWheelFb->pixelData;
    
    int totalBytes = g_masterColorWheelFb->displayWidth * g_masterColorWheelFb->displayHeight * 3;
    
    // Fast 1D array loop for instant dimming
    for (int i = 0; i < totalBytes; i++) {
        dst[i] = (uint8_t)(src[i] * brightness);
    }
}

CCColor* getColorFromWheelTouch(int localX, int localY, int width, int height) {
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float maxRadius = (cx < cy) ? cx : cy;
    
    float dx = localX - cx;
    float dy = localY - cy;
    float distance = sqrt(dx*dx + dy*dy);
    
    // Check if they tapped inside the circle
    if (distance <= maxRadius) {
        float theta = atan2(dy, dx);
        float hue = (theta + M_PI) / (2.0f * M_PI);
        float saturation = distance / maxRadius;
        
        float r, g, b;
        hsv_to_rgb(hue, saturation, 1.0f, &r, &g, &b);
        return color(r, g, b, 1.0f);
    }
    
    return color(0, 0, 0, 0); // Transparent if they missed
}
