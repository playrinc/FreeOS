//
//  PaintApp.c
//
//
//  Created by Chris Galzerano on 2/22/26.
//


#include "main.h"
#include "PaintApp.h"
#include "ColorWheel.h"


// --- APP STATE & MEMORY ---
CCView* colorPickerContainer = NULL;

Framebuffer* canvasFramebuffer = NULL;
CCFramebufferView* canvasView = NULL;

bool isColorPickerVisible = true; // We start with it open for testing

CCView* shapesMenu = NULL;
CCView* fileMenu = NULL;

static bool isShapesMenuOpen = false;
static bool isFileMenuOpen = false;
static bool isColorPickerOpen = false;

// --- TOUCH STATE TRACKING ---
static bool is_pressing = false;
static bool long_press_fired = false;
static uint64_t touchStartTime = 0;
static int touchStartX = 0;
static int touchStartY = 0;

static int activeTool = TAG_PAINT_PEN;
static ColorRGBA activeColor = {0.0, 0.0, 0.0, 1.0}; // Default to black brush
static int lastDragX = -1;
static int lastDragY = -1;

static int dirtyMinX = 9999, dirtyMinY = 9999, dirtyMaxX = -1, dirtyMaxY = -1;
static uint64_t lastCanvasUpdateTime = 0;
static uint64_t lastPointProcessTime = 0;
static int currentBrushThickness = 5;



// Helper: Safely draw a single pixel to the PSRAM buffer
static void draw_pixel_to_canvas(int x, int y, ColorRGBA color) {
    if (!canvasFramebuffer || !canvasFramebuffer->pixelData) return;
    
    // Safety clip so we don't crash if dragging outside the canvas
    if (x < 0 || x >= canvasFramebuffer->displayWidth || y < 0 || y >= canvasFramebuffer->displayHeight) return;
    
    int idx = (y * canvasFramebuffer->displayWidth + x) * 3;
    uint8_t* pixels = (uint8_t*)canvasFramebuffer->pixelData;
    
    pixels[idx]   = (uint8_t)(color.r * 255.0f); // BGR/RGB depending on your engine
    pixels[idx+1] = (uint8_t)(color.g * 255.0f);
    pixels[idx+2] = (uint8_t)(color.b * 255.0f);
}

// Helper: Bresenham's Line Algorithm to connect fast touch drags
static void draw_line_to_canvas(int x0, int y0, int x1, int y1, ColorRGBA color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    
    while (1) {
        draw_pixel_to_canvas(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Helper: Stamps a solid circle of pixels to create a thick, rounded brush tip
static void draw_brush_stamp(int cx, int cy, int radius, ColorRGBA color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            // Check if the pixel is inside the circle's radius
            if (x * x + y * y <= radius * radius) {
                draw_pixel_to_canvas(cx + x, cy + y, color);
            }
        }
    }
}

// Helper: Bresenham's line updated to stamp the thick brush!
static void draw_thick_line_to_canvas(int x0, int y0, int x1, int y1, int thickness, ColorRGBA color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    
    int radius = thickness / 2;

    while (1) {
        draw_brush_stamp(x0, y0, radius, color); // Stamp the thick circle!
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void push_partial_canvas_update(int localX, int localY, int w, int h) {
    if (!canvasFramebuffer || !fb.pixelData) return;

    // 1. Get the global screen coordinates of the canvas
    CCPoint absOrigin = getAbsoluteOrigin(canvasView->view);
    int globalX = absOrigin.x + localX;
    int globalY = absOrigin.y + localY;

    // 2. Blit ONLY the dirty rectangle from the Canvas to the Main OS Framebuffer
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int srcIdx = ((localY + y) * canvasFramebuffer->displayWidth + (localX + x)) * 3;
            int destIdx = ((globalY + y) * fb.displayWidth + (globalX + x)) * 3;
            
            uint8_t* srcPixels = (uint8_t*)canvasFramebuffer->pixelData;
            uint8_t* destPixels = (uint8_t*)fb.pixelData;
            
            destPixels[destIdx]   = srcPixels[srcIdx];
            destPixels[destIdx+1] = srcPixels[srcIdx+1];
            destPixels[destIdx+2] = srcPixels[srcIdx+2];
        }
    }

    // 3. Package the command
    GraphicsCommand cmd;
    cmd.x = globalX;
    cmd.y = globalY;
    cmd.w = w;
    cmd.h = h;
    
    // --- START PROFILING TIMER ---
    uint64_t start_time = esp_timer_get_time();
    
    // Fire the SPI update!
    updateArea1(fb, cmd);
    
    // --- END PROFILING TIMER ---
    uint64_t end_time = esp_timer_get_time();
    
    // Calculate the duration
    uint64_t duration_us = end_time - start_time;
    uint64_t duration_ns = duration_us * 1000; // Convert microseconds to nanoseconds
    
    // Log the exact time and the size of the box we just sent
    FreeOSLogI("PaintApp", "updateArea pushed [%dx%d] box in %llu us (%llu ns)", w, h, duration_us, duration_ns);
}

// --- REUSABLE ICON BUTTON BUILDER ---
static CCView* make_icon_button(int x, int y, int tag, CCString* img) {
    // 1. Use CCRect* pointer
    CCRect* btnRect = ccRect(x, y, 40, 40);
    CCView* btn = viewWithFrame(btnRect);
    btn->backgroundColor = color(0.25, 0.25, 0.25, 1.0);
    btn->tag = tag;
    
    // 2. Use CCRect* pointer for the image view frame
    CCRect* imgRect = ccRect(5, 5, 30, 30);
    CCImageView* icon = imageViewWithFrame(imgRect);
    icon->ignoreTouch = true;
    imageViewSetImage(icon, imageWithFile(img));
    
    // 3. Add the wrapper directly!
    viewAddSubview(btn, icon);
    
    return btn;
}

// --- FILE MENU BUILDER (160px Wide with Text) ---
static CCView* create_file_menu(int x, int y) {
    CCRect* menuRect = ccRect(x, y, 160, 250);
    CCView* menu = viewWithFrame(menuRect);
    menu->backgroundColor = color(0.2, 0.2, 0.2, 1.0);
    
    const char* labels[] = {"New", "Open", "Save", "Save As", "Copy", "Paste"};
    int tags[] = {TAG_FILE_NEW, TAG_FILE_OPEN, TAG_FILE_SAVE, TAG_FILE_SAVEAS, TAG_FILE_COPY, TAG_FILE_PASTE};
    
    for (int i = 0; i < 6; i++) {
        CCRect* itemRect = ccRect(5, 5 + (i * 40), 150, 35);
        CCView* itemBtn = viewWithFrame(itemRect);
        itemBtn->backgroundColor = color(0.3, 0.3, 0.3, 1.0);
        itemBtn->tag = tags[i];
        
        CCRect* lblRect = ccRect(10, 8, 130, 20);
        CCLabel* lbl = labelWithFrame(lblRect);
        lbl->fontSize = 18.0;
        lbl->textColor = color(1,1,1,1);
        lbl->ignoreTouch = true;
        labelSetText(lbl, ccs(labels[i]));
        
        // Add the wrapper directly!
        viewAddSubview(itemBtn, lbl);
        viewAddSubview(menu, itemBtn);
    }
    
    return menu;
}

// --- SHAPES MENU BUILDER (Vertical Icons) ---
static CCView* create_shapes_menu(int x, int y) {
    CCRect* menuRect = ccRect(x, y, 50, 186);
    CCView* menu = viewWithFrame(menuRect);
    menu->backgroundColor = color(0.2, 0.2, 0.2, 1.0);
    
    // Using leftArrow20 for now until you add the specific shape glyphs
    viewAddSubview(menu, make_icon_button(5, 5 + (44 * 0), TAG_SHAPE_LINE, ccs("/spiflash/leftArrow20.png")));
    viewAddSubview(menu, make_icon_button(5, 5 + (44 * 1), TAG_SHAPE_RECT, ccs("/spiflash/leftArrow20.png")));
    viewAddSubview(menu, make_icon_button(5, 5 + (44 * 2), TAG_SHAPE_CIRCLE, ccs("/spiflash/leftArrow20.png")));
    viewAddSubview(menu, make_icon_button(5, 5 + (44 * 3), TAG_SHAPE_POLYGON, ccs("/spiflash/leftArrow20.png")));
    
    return menu;
}

void setup_paint_ui(void) {
    FreeOSLogI("PaintApp", "Starting Paint App Initialization");
    
    // 1. Container & Canvas Setup
    CCRect* screenRect = ccRect(0, 0, 320, 480);
    mainWindowView = viewWithFrame(screenRect);
    mainWindowView->backgroundColor = color(0.1, 0.1, 0.1, 1.0);
    
    if (!canvasFramebuffer) {
        canvasFramebuffer = (Framebuffer*)cc_safe_alloc(1, sizeof(Framebuffer));
        canvasFramebuffer->displayWidth = 320;
        canvasFramebuffer->displayHeight = 430;
        canvasFramebuffer->colorMode = COLOR_MODE_BGR888;
        canvasFramebuffer->pixelData = heap_caps_malloc(320 * 430 * 3, MALLOC_CAP_SPIRAM);
        memset(canvasFramebuffer->pixelData, 255, 320 * 430 * 3); // Fill White
    }
    
    CCRect* canvasRect = ccRect(0, 50, 320, 430);
    canvasView = framebufferViewWithFrame(canvasRect);
    canvasView->tag = TAG_CANVAS;
    framebufferViewSetFramebuffer(canvasView, canvasFramebuffer);
    
    // Add the Framebuffer wrapper directly!
    viewAddSubview(mainWindowView, canvasView);
    
    // 2. Toolbar Setup
    CCRect* toolbarRect = ccRect(0, 23, 320, 50);
    CCView* toolbar = viewWithFrame(toolbarRect);
    toolbar->backgroundColor = color(0.15, 0.15, 0.15, 1.0);
    viewAddSubview(mainWindowView, toolbar);
    
    // 3. Add the 7 Main Toolbar Buttons
    int startX = 5;
    int spacing = 44;
    
    // Note: Y is passed as 5 to keep them centered vertically in the 50px toolbar
    viewAddSubview(toolbar, make_icon_button(startX + (spacing * 0), 5, TAG_PAINT_PEN, ccs("/spiflash/brush.png")));
    viewAddSubview(toolbar, make_icon_button(startX + (spacing * 1), 5, TAG_PAINT_SHAPES, ccs("/spiflash/leftArrow20.png"))); // Shape Menu Trigger
    viewAddSubview(toolbar, make_icon_button(startX + (spacing * 2), 5, TAG_PAINT_BUCKET, ccs("/spiflash/leftArrow20.png")));
    viewAddSubview(toolbar, make_icon_button(startX + (spacing * 3), 5, TAG_PAINT_TEXT, ccs("/spiflash/leftArrow20.png")));
    viewAddSubview(toolbar, make_icon_button(startX + (spacing * 4), 5, TAG_PAINT_COLOR, ccs("/spiflash/colorIcon.png")));
    viewAddSubview(toolbar, make_icon_button(startX + (spacing * 5), 5, TAG_PAINT_PROPERTIES, ccs("/spiflash/leftArrow20.png")));
    viewAddSubview(toolbar, make_icon_button(startX + (spacing * 6), 5, TAG_PAINT_FILEMENU, ccs("/spiflash/leftArrow20.png"))); // File Menu Trigger
    
    // 4. Pre-build the Dropdown Menus
    // Positioned directly under their respective toolbar buttons!
    shapesMenu = create_shapes_menu(startX + (spacing * 1), afb(toolbar));
    fileMenu = create_file_menu(320 - 165, afb(toolbar));
    
    // 6. Add the Color Picker View!
    // (We put it at X:40, Y:100 so it floats nicely over the canvas)
    //colorPickerContainer = colorPickerView(40, 100);
    //viewAddSubview(mainWindowView, colorPickerContainer);
    
    
    
}

void handle_paint_touch(int x, int y, int touchState) {
    
    // ==========================================
    // 1. FINGER DOWN OR DRAGGING
    // ==========================================
    if (touchState == 1) {
        if (!is_pressing) {
            // --- FIRST FRAME OF TOUCH ---
            is_pressing = true;
            long_press_fired = false;
            touchStartTime = esp_timer_get_time() / 1000;
            touchStartX = x;
            touchStartY = y;
            
            // Set up our initial drag coordinates by converting the global touch
            // into the Canvas's local coordinate system!
            CCPoint localPt = viewConvertPoint(canvasView->view, x, y);
            lastDragX = localPt.x;
            lastDragY = localPt.y;
            
            // Dismiss menus if clicking outside...
            CCView* downHit = find_subview_at_point_recursive(mainWindowView, x, y);
            if (isShapesMenuOpen && (!downHit || (downHit->tag < TAG_SHAPE_LINE || downHit->tag > TAG_SHAPE_POLYGON))) {
                viewRemoveFromSuperview(shapesMenu);
                isShapesMenuOpen = false;
                update_full_ui();
            }
            if (isFileMenuOpen && (!downHit || (downHit->tag < TAG_FILE_NEW || downHit->tag > TAG_FILE_PASTE))) {
                viewRemoveFromSuperview(fileMenu);
                isFileMenuOpen = false;
                update_full_ui();
            }
        } else {
                    // --- FINGER IS HELD DOWN OR DRAGGING ---
                    if (abs(x - touchStartX) > 15 || abs(y - touchStartY) > 15) {
                        touchStartTime = 0;
                    }
                    
                    CCPoint localPt = viewConvertPoint(canvasView->view, x, y);
                    int canvasX = localPt.x;
                    int canvasY = localPt.y;

                    if (activeTool == TAG_PAINT_PEN && canvasY >= 0 && canvasY < canvasFramebuffer->displayHeight) {
                        if (lastDragX != -1 && lastDragY != -1) {
                            
                            // 1. INSTANT PSRAM UPDATE
                            // This takes ~0.001ms. The CPU catches every tiny curve of your signature!
                            draw_thick_line_to_canvas(lastDragX, lastDragY, canvasX, canvasY, currentBrushThickness, activeColor);
                            
                            // 2. EXPAND THE DIRTY ACCUMULATOR
                            // Keep stretching the box to fit the new lines
                            int currentMinX = (lastDragX < canvasX) ? lastDragX : canvasX;
                            int currentMaxX = (lastDragX > canvasX) ? lastDragX : canvasX;
                            int currentMinY = (lastDragY < canvasY) ? lastDragY : canvasY;
                            int currentMaxY = (lastDragY > canvasY) ? lastDragY : canvasY;
                            
                            if (currentMinX < dirtyMinX) dirtyMinX = currentMinX;
                            if (currentMaxX > dirtyMaxX) dirtyMaxX = currentMaxX;
                            if (currentMinY < dirtyMinY) dirtyMinY = currentMinY;
                            if (currentMaxY > dirtyMaxY) dirtyMaxY = currentMaxY;
                            
                            // 3. THROTTLE THE SPI UPDATE (The 6.7ms Bottleneck)
                            uint64_t now = esp_timer_get_time() / 1000; // Get current milliseconds
                            
                            // Only push to the screen every 30 milliseconds (~33 FPS)
                            if (now - lastCanvasUpdateTime > 30) {
                                
                                // Add brush padding to the accumulated box
                                int pad = (currentBrushThickness / 2) + 2;
                                dirtyMinX = (dirtyMinX > pad) ? dirtyMinX - pad : 0;
                                dirtyMinY = (dirtyMinY > pad) ? dirtyMinY - pad : 0;
                                dirtyMaxX = (dirtyMaxX < canvasFramebuffer->displayWidth - pad - 1) ? dirtyMaxX + pad : canvasFramebuffer->displayWidth - 1;
                                dirtyMaxY = (dirtyMaxY < canvasFramebuffer->displayHeight - pad - 1) ? dirtyMaxY + pad : canvasFramebuffer->displayHeight - 1;
                                
                                int rectW = dirtyMaxX - dirtyMinX + 1;
                                int rectH = dirtyMaxY - dirtyMinY + 1;
                                
                                // Push the combined movements over SPI!
                                push_partial_canvas_update(dirtyMinX, dirtyMinY, rectW, rectH);
                                
                                // Reset the accumulator box and timer
                                dirtyMinX = 9999; dirtyMinY = 9999; dirtyMaxX = -1; dirtyMaxY = -1;
                                lastCanvasUpdateTime = now;
                            }
                        }
                    }
                    
                    // Save the coordinate for the next frame
                    lastDragX = canvasX;
                    lastDragY = canvasY;
                }
        return;
    }
    
    // ==========================================
    // 2. FINGER LIFTED (RELEASED)
    // ==========================================
    if (touchState == 0) {
            if (!is_pressing) return;
            is_pressing = false;
            
            // FORCE FLUSH ANY REMAINING DRAWING!
            /*if (dirtyMaxX != -1) {
                int rectW = dirtyMaxX - dirtyMinX + 1;
                int rectH = dirtyMaxY - dirtyMinY + 1;
                push_partial_canvas_update(dirtyMinX, dirtyMinY, rectW, rectH);
                dirtyMinX = 9999; dirtyMinY = 9999; dirtyMaxX = -1; dirtyMaxY = -1;
            }*/
        
        // Flush any remaining accumulated drawing to the screen!
                if (dirtyMaxX != -1) {
                    int pad = (currentBrushThickness / 2) + 2;
                    dirtyMinX = (dirtyMinX > pad) ? dirtyMinX - pad : 0;
                    dirtyMinY = (dirtyMinY > pad) ? dirtyMinY - pad : 0;
                    dirtyMaxX = (dirtyMaxX < canvasFramebuffer->displayWidth - pad - 1) ? dirtyMaxX + pad : canvasFramebuffer->displayWidth - 1;
                    dirtyMaxY = (dirtyMaxY < canvasFramebuffer->displayHeight - pad - 1) ? dirtyMaxY + pad : canvasFramebuffer->displayHeight - 1;
                    
                    int rectW = dirtyMaxX - dirtyMinX + 1;
                    int rectH = dirtyMaxY - dirtyMinY + 1;
                    
                    push_partial_canvas_update(dirtyMinX, dirtyMinY, rectW, rectH);
                    dirtyMinX = 9999; dirtyMinY = 9999; dirtyMaxX = -1; dirtyMaxY = -1;
                }
            
            lastDragX = -1;
            lastDragY = -1;
        
        if (touchStartTime == 0 || long_press_fired) {
            long_press_fired = false;
            return;
        }
        
        // --- DYNAMIC HIT TESTING! ---
        CCView* tappedView = find_subview_at_point_recursive(mainWindowView, touchStartX, touchStartY);
        
        if (!tappedView) {
            FreeOSLogI("PaintApp", "Tapped the blank background.");
            return;
        }
        
        FreeOSLogI("PaintApp", "Tapped the blank background %d", tappedView->tag);
        
        // Just route the action based on the View's Tag!
        switch (tappedView->tag) {
                
            case TAG_PAINT_PEN:
            case TAG_PAINT_BUCKET:
            case TAG_PAINT_TEXT:
                activeTool = tappedView->tag; // Switch current tool!
                FreeOSLogI("PaintApp", "Active tool changed to: %d", activeTool);
                break;
                
                // --- TOOLBAR TOGGLES ---
            case TAG_PAINT_SHAPES:
                isShapesMenuOpen = !isShapesMenuOpen;
                if (isShapesMenuOpen) viewAddSubview(mainWindowView, shapesMenu);
                else viewRemoveFromSuperview(shapesMenu);
                update_full_ui();
                break;
                
            case TAG_PAINT_FILEMENU:
                isFileMenuOpen = !isFileMenuOpen;
                if (isFileMenuOpen) viewAddSubview(mainWindowView, fileMenu);
                else viewRemoveFromSuperview(fileMenu);
                update_full_ui();
                break;
                
            case TAG_PAINT_COLOR:
                isColorPickerOpen = !isColorPickerOpen;
                if (isColorPickerOpen) {
                    colorPickerContainer = colorPickerView(40, 100);
                    viewAddSubview(mainWindowView, colorPickerContainer);
                }
                else viewRemoveFromSuperview(colorPickerContainer);
                update_full_ui();
                break;
                
            case TAG_FILE_NEW:
                FreeOSLogI("PaintApp", "New File Selected!");
                viewRemoveFromSuperview(fileMenu);
                isFileMenuOpen = false;
                update_full_ui();
                break;
                
            case TAG_FILE_OPEN:
            case TAG_FILE_SAVE:
                FreeOSLogI("PaintApp", "File Menu Action %d triggered", tappedView->tag);
                viewRemoveFromSuperview(fileMenu);
                isFileMenuOpen = false;
                update_full_ui();
                break;
                
            case TAG_SHAPE_LINE:
            case TAG_SHAPE_RECT:
            case TAG_SHAPE_CIRCLE:
            case TAG_SHAPE_POLYGON:
                activeTool = tappedView->tag;
                FreeOSLogI("PaintApp", "Active tool changed to Shape: %d", activeTool);
                viewRemoveFromSuperview(shapesMenu);
                isShapesMenuOpen = false;
                update_full_ui();
                break;
                
                
                
                // --- COLOR PICKER INTERACTION ---
            case TAG_CANVAS:
                // Using your new viewConvertPoint for the color wheel math!
                if (isColorPickerOpen) {
                    CCPoint localTap = viewConvertPoint(tappedView, touchStartX, touchStartY);
                    CCColor* picked = getColorFromWheelTouch(localTap.x, localTap.y, tappedView->frame->size->width, tappedView->frame->size->height);
                    FreeOSLogI("PaintApp", "Picked Color: R:%f G:%f B:%f", picked->r, picked->g, picked->b);
                }
                break;
        }
    }
}
