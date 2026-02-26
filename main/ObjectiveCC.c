//
//  ObjectiveCC.c
//  Objective CC
//
//  Created by Chris Galzerano on 1/22/25.
//

#include "ObjectiveCC.h"

//C Standard Library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>
#include <setjmp.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <libgen.h>
#include <regex.h>
#include <pthread.h>
#include <sys/termios.h>
#include <fcntl.h>
#include <sys/select.h>
#include <dirent.h>

#include <cJSON.h>

//Graphics Library
#include "CPUGraphics.h"

#include "esp_heap_caps.h"

CCRange* ccRange(UInteger location, UInteger length) {
    CCRange* range = (CCRange*)cc_safe_alloc(1, sizeof(CCRange));
    range->type = CCType_Range;
    if (!range) return NULL;
    range->loc = location;
    range->len = length;
    return range;
}

CCPoint* ccPoint(float x, float y) {
    CCPoint* point = (CCPoint*)cc_safe_alloc(1, sizeof(CCPoint));
    point->type = CCType_Point;
    if (!point) return NULL;
    point->x = x;
    point->y = y;
    return point;
}

CCSize* ccSize(float width, float height) {
    CCSize* size = (CCSize*)cc_safe_alloc(1, sizeof(CCSize));
    size->type = CCType_Size;
    if (!size) return NULL;
    size->width = width;
    size->height = height;
    return size;
}

CCRect* ccRect(float x, float y, float width, float height) {
    CCRect* rect = (CCRect*)cc_safe_alloc(1, sizeof(CCRect));
    rect->type = CCType_Rect;
    if (!rect) return NULL;
    rect->origin = ccPoint(x, y);
    rect->size = ccSize(width, height);
    return rect;
}

bool rectContainsPoint(CCRect* rect, CCPoint* point) {
    if ((point->x >= rect->origin->x) &&
        (point->y >= rect->origin->y) &&
        (point->x <= rect->origin->x+rect->size->width) &&
        (point->y <= rect->origin->y+rect->size->height)) {
        return true;
    }
    return false;
}

bool rectContainsRect(CCRect* rect, CCRect* rect1) {
    //Is rect1 x origin >= rect x origin AND
    //Is rect1 x origin + rect1 width <= rect x origin + rect width AND
    //Is rect1 y origin >= rect y origin AND
    //Is rect1 y origin + rect1 width <= rect y origin + rect height
    
    if ((rect1->origin->x >= rect->origin->x) &&
        (rect1->origin->x + rect1->size->width <= rect->origin->x + rect->size->width) &&
        (rect1->origin->y >= rect->origin->y) &&
        (rect1->origin->y + rect1->size->height <= rect->origin->y + rect->size->height)) {
        return true;
    }
    return false;
}

//Threading Functions
CCThread* ccThread(void) {
    CCThread* newThread = (CCThread*)cc_safe_alloc(1, sizeof(CCThread));
    if (!newThread) return NULL;
    newThread->type = CCType_Thread;
    newThread->threadType = CCThreadBackground;
    return newThread;
}

CCThread* ccThreadWithType(CCThreadType threadType) {
    CCThread* newThread = (CCThread*)cc_safe_alloc(1, sizeof(CCThread));
    if (!newThread) return NULL;
    newThread->type = CCType_Thread;
    newThread->threadType = threadType;
    return newThread;
}

CCThread* ccThreadWithParameters(CCThreadType threadType, CCThreadFunction function, CCThreadCompletionCallback callback, void* functionArg, void* callbackArg) {
    CCThread* newThread = (CCThread*)cc_safe_alloc(1, sizeof(CCThread));
    if (!newThread) return NULL;
    newThread->type = CCType_Thread;
    newThread->threadType = threadType;
    newThread->function = function;
    newThread->callback = callback;
    newThread->callbackArg = callbackArg;
    newThread->functionArg = functionArg;
    return newThread;
}

void* threadWrapper(void* arg) {
    CCThread* task = (CCThread*)arg;
    task->function(task->functionArg);
    if (task->callback) {
        task->callback(task->callbackArg);
    }
    free(task);
    return NULL;
}

void threadExecuteWithCallback(CCThreadFunction task, void* taskArg, CCThreadType mode, CCThreadCompletionCallback callback, void* callbackArg) {
    if (mode == CCThreadBackground) {
        pthread_t thread;
        CCThread* t = (CCThread*)cc_safe_alloc(1, sizeof(CCThread));
        t->function = task;
        t->functionArg = taskArg;
        t->callback = callback;
        t->callbackArg = callbackArg;
        
        int result = pthread_create(&thread, NULL, threadWrapper, t);
        if (result != 0) {
            fprintf(stderr, "Error creating background thread\n");
            free(t);
        } else {
            pthread_detach(thread);
        }
    } else if (mode == CCThreadMain) {
        task(taskArg);
        if (callback) {
            callback(callbackArg);
        }
    }
}

void threadExecute(CCThread* thread) {
    threadExecuteWithCallback(thread->function, thread->functionArg, thread->threadType, thread->callback, thread->callbackArg);
}


//Serial Port Functions
/*CCArray* serialPortList(void) {
    CCArray *result = array();
        const char *port_prefixes[] = {
            "/dev/ttyUSB",  // USB-serial devices on Linux
            "/dev/ttyACM",  // ACM serial devices on Linux
            "/dev/ttyS",    // Standard serial ports on Linux
            "/dev/cu.usb",  // USB-serial devices on macOS
            "/dev/tty.usb"  // USB-serial devices on macOS
        };

        const int prefix_count = sizeof(port_prefixes) / sizeof(port_prefixes[0]);
        DIR *dev_dir = opendir("/dev");
        if (dev_dir == NULL) {
            perror("opendir");
            return NULL;
        }

        struct dirent *entry;
        while ((entry = readdir(dev_dir)) != NULL) {
            for (int i = 0; i < prefix_count; ++i) {
                if (strncmp(entry->d_name, port_prefixes[i] + strlen("/dev/"), strlen(port_prefixes[i]) - strlen("/dev/")) == 0) {
                    char full_path[256];
                    snprintf(full_path, sizeof(full_path), "/dev/%s", entry->d_name);
                    CCString *portString = stringWithCString(full_path);
                    arrayAddObject(result, portString);
                }
            }
        }
        closedir(dev_dir);
        return result;
}*/

int open_serial_port(const char *device, speed_t baudrate) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    cfsetispeed(&options, baudrate);
    cfsetospeed(&options, baudrate);
    options.c_cflag = (options.c_cflag & ~CSIZE) | CS8;
    options.c_iflag &= ~IGNBRK;
    options.c_lflag = 0;
    options.c_oflag = 0;
    options.c_cc[VMIN]  = 1;
    options.c_cc[VTIME] = 1;
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~(PARENB | PARODD);
    options.c_cflag &= ~CSTOPB;
    //options.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    return fd;
}

// Function to monitor the serial port and invoke the callback when data is available
void listen_serial_port(bool *listening, int fd, CCSerialPortCallback callback) {
    char buffer[256];
    fd_set readfds;
    
    if (listening == false) {
        printf("serial port remove listener");
    }

    while (*listening) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        int max_fd = fd + 1;

        // Wait for data on the serial port
        int result = select(max_fd, &readfds, NULL, NULL, NULL);
        if (result > 0 && FD_ISSET(fd, &readfds)) {
            ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                callback(buffer); // Call the callback function with the data
            } else if (n < 0) {
                perror("read");
                break;
            }
        } else if (result < 0) {
            perror("select");
            break;
        }
    }
}

void close_serial_port(int fd) {
    close(fd);
}

void write_to_serial_port(int fd, const char *message) {
    size_t len = strlen(message);
    ssize_t n = write(fd, message, len);
    if (n != len) {
        perror("write");
    }
}

char* read_from_serial_port(int fd) {
    char buffer[256];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        char *result = strdup(buffer);
        return result;
    } else if (n < 0) {
        perror("read");
    }
    return NULL;
}

CCSerialPort* serialPort(void) {
    CCSerialPort* newSerialPort = (CCSerialPort*)cc_safe_alloc(1, sizeof(CCSerialPort));
    if (!newSerialPort) return NULL;
    newSerialPort->type = CCType_SerialPort;
    return newSerialPort;
}

CCSerialPort* serialPortWithName(CCString* name) {
    CCSerialPort* newSerialPort = (CCSerialPort*)cc_safe_alloc(1, sizeof(CCSerialPort));
    if (!newSerialPort) return NULL;
    newSerialPort->type = CCType_SerialPort;
    newSerialPort->name = name;
    return newSerialPort;
}

int serialPortOpen(CCSerialPort* serialPort) {
    int fd = open_serial_port(cStringOfString(serialPort->name), serialPort->baudRate);
    serialPort->fd = fd;
    return fd;
}

void serialPortClose(CCSerialPort* serialPort) {
    serialPortRemoveListener(serialPort);
    close_serial_port(serialPort->fd);
}

void serialPortAddListener(CCSerialPort* serialPort) {
    listen_serial_port(&serialPort->listening, serialPort->fd, serialPort->callback);
}

void serialPortRemoveListener(CCSerialPort* serialPort) {
    *(&serialPort->listening) = false;
}

void serialPortSendData(CCSerialPort* serialPort, CCString* data) {
    write_to_serial_port(serialPort->fd, cStringOfString(data));
}


//Graphics Functions
//Should redraw all graphics of program?
static bool shouldUpdate = true;

// Create a new CCColor
CCColor* color(Float r, Float g, Float b, Float a) {
    CCColor* newColor = (CCColor*)cc_safe_alloc(1, sizeof(CCColor));
    newColor->type = CCType_Color;
    if (!newColor) return NULL;
    newColor->r = r;
    newColor->g = g;
    newColor->b = b;
    newColor->a = a;
    return newColor;
}

// Helper to convert CCColor (0.0-1.0) to ColorRGBA (0-255)
// --- Helper: Convert High-Level CCColor (float) to Low-Level ColorRGBA (byte) ---
ColorRGBA convert_cc_color(CCColor* cc) {
    ColorRGBA c;
    if (cc) {
        // Clamp and convert 0.0-1.0 float to 0-255 byte
        c.r = (uint8_t)(cc->r * 255.0f);
        c.g = (uint8_t)(cc->g * 255.0f);
        c.b = (uint8_t)(cc->b * 255.0f);
        c.a = (uint8_t)(cc->a * 255.0f);
    } else {
        // Default to transparent if null
        c.r = 0; c.g = 0; c.b = 0; c.a = 0;
    }
    return c;
}

CCColor* convert_colorrgba_to_cccolor(ColorRGBA c) {
    CCColor* cc = (CCColor*)cc_safe_alloc(1, sizeof(CCColor));
    cc->type = CCType_Color;
    if (!cc) return NULL;
    
    // Convert 0-255 byte back to 0.0-1.0 float
    cc->r = (Float)(c.r / 255.0f);
    cc->g = (Float)(c.g / 255.0f);
    cc->b = (Float)(c.b / 255.0f);
    cc->a = (Float)(c.a / 255.0f);
    
    cc->type = CCType_Color;
    
    return cc;
}

// Constructor for high-level CCGradient
CCGradient* gradientWithColors(CCArray* colors, CCArray* locations, Float angle) {
    CCGradient* newGrad = (CCGradient*)cc_safe_alloc(1, sizeof(CCGradient));
    newGrad->type = CCType_Gradient; // You might need to add this to your CCType enum
    newGrad->colors = colors;
    newGrad->locations = locations;
    newGrad->angle = angle;
    newGrad->gradType = GRADIENT_TYPE_LINEAR;
    return newGrad;
}



// Helper to allocate a low-level Gradient from a high-level CCGradient
// NOTE: Returns a pointer that must be freed by the graphics task!
Gradient* create_low_level_gradient(CCGradient* ccGrad) {
    if (!ccGrad || !ccGrad->colors || !ccGrad->locations) return NULL;
    
    int count = ccGrad->colors->count;
    
    Gradient* lowLevelGrad = (Gradient*)cc_safe_alloc(1, sizeof(Gradient));
    lowLevelGrad->stops = (ColorStop*)cc_safe_alloc(1, sizeof(ColorStop) * count);
    lowLevelGrad->numStops = count;
    lowLevelGrad->angle = ccGrad->angle;
    
    // --- FIX: Copy the type ---
    lowLevelGrad->type = ccGrad->gradType;
    // --------------------------
    
    // Convert data
    for (int i = 0; i < count; i++) {
        CCColor* ccCol = (CCColor*)arrayObjectAtIndex(ccGrad->colors, i);
        CCNumber* ccLoc = (CCNumber*)arrayObjectAtIndex(ccGrad->locations, i);
        
        lowLevelGrad->stops[i].color = convert_cc_color(ccCol);
        lowLevelGrad->stops[i].position = (float)ccLoc->doubleValue;
    }
    
    return lowLevelGrad;
}

// Create a new CCLayer with default properties
CCLayer* layer(void) {
    CCLayer* newLayer = (CCLayer*)cc_safe_alloc(1, sizeof(CCLayer));
    newLayer->type = CCType_Layer;
    if (!newLayer) return NULL;
    
    newLayer->frame = ccRect(0, 0, 0, 0);
    newLayer->backgroundColor = color(1, 1, 1, 0);
    newLayer->gradient = NULL;
    newLayer->cornerRadius = 0.0;
    newLayer->borderColor = color(0, 0, 0, 1); // Black border
    newLayer->borderWidth = 0.0;
    newLayer->shadowOffset = ccPoint(0, 0);
    newLayer->shadowRadius = 0.0;
    newLayer->shadowOpacity = 0.0;
    newLayer->shadowColor = color(0, 0, 0, 0);  // Transparent shadow
    newLayer->superlayer = NULL;
    newLayer->sublayers = array();  // Empty initially

    return newLayer;
}

// Create a new CCLayer with a specified frame
CCLayer* layerWithFrame(CCRect* frame) {
    CCLayer* newLayer = layer();
    if (!newLayer) return NULL;

    newLayer->frame = frame;

    return newLayer;
}

// Create a new CCView with default properties
CCView* view(void) {
    CCView* newView = (CCView*)cc_safe_alloc(1, sizeof(CCView));
    newView->type = CCType_View;
    if (!newView) return NULL;

    newView->frame = ccRect(0, 0, 0, 0);
    newView->backgroundColor = color(1, 1, 1, 0);
    newView->layer = layer();  // Use existing layer() function to initialize the layer
    if (!newView->layer) {
        free(newView);
        return NULL;
    }
    newView->shapeLayer = NULL;

    newView->superview = NULL;
    newView->subviews = array();  // Empty initially
    newView->gestureRecognizers = array();
    
    return newView;
}

// Create a new CCView with a specified frame
CCView* viewWithFrame(CCRect* frame) {
    CCView* newView = (CCView*)cc_safe_alloc(1, sizeof(CCView));
    newView->type = CCType_View;
    if (!newView) return NULL;

    newView->frame = frame;
    newView->backgroundColor = color(1, 1, 1, 0);
    //newView->layer = (CCLayer*)cc_safe_alloc(1, sizeof(CCLayer));  // Initialize the layer with the specified frame
    newView->layer = layerWithFrame(frame);
    newView->frame = frame;
    if (!newView->layer) {
        free(newView);
        return NULL;
    }
    newView->shapeLayer = NULL;
    
    newView->superview = NULL;
    newView->subviews = array();  // Empty initially
    newView->gestureRecognizers = array();
    
    return newView;
}

/*void viewAddSubview(void* view, void* subview) {
    CCView* view1 = view;
    arrayAddObject(view1->subviews, subview);
    shouldUpdate = true;
}*/

void viewRemoveFromSuperview(void* child) {
    if (!child) return;

    // 1. Unwrap the CHILD to find the underlying CCView struct
    CCType childType = *((CCType*)child);
    CCView* childBase = NULL;

    if (childType == CCType_View) {
        childBase = (CCView*)child;
    }
    else if (childType == CCType_ImageView) {
        childBase = ((CCImageView*)child)->view;
    }
    else if (childType == CCType_Label) {
        childBase = ((CCLabel*)child)->view;
    }
    else if (childType == CCType_ScrollView) {
        childBase = ((CCScrollView*)child)->view;
    }

    // 2. Remove if linked
    if (childBase && childBase->superview) {
        // FIX: Cast struct CCView* to CCView* to satisfy the compiler
        CCView* parent = (CCView*)childBase->superview;

        // Remove the Wrapper Object (the original 'child' void*) from parent's list
        if (parent->subviews) {
            arrayRemoveObject(parent->subviews, child);
        }

        // Clear the link
        childBase->superview = NULL;
    }
}

void viewAddSubview(void* parent, void* child) {
    if (!parent || !child) return;

    // 1. Unwrap the PARENT (The container)
    // We assume the parent passed in is usually a CCView (like mainWindowView or container),
    // but safety checking the type is good practice.
    CCView* parentView = (CCView*)parent;

    // 2. Unwrap the CHILD (The item being added)
    // We need to find the actual CCView struct inside the wrapper to set its 'superview'.
    CCType childType = *((CCType*)child);
    CCView* childBase = NULL;

    if (childType == CCType_View) {
        childBase = (CCView*)child;
    }
    else if (childType == CCType_ImageView) {
        childBase = ((CCImageView*)child)->view;
    }
    else if (childType == CCType_Label) {
        childBase = ((CCLabel*)child)->view;
    }
    else if (childType == CCType_ScrollView) {
        childBase = ((CCScrollView*)child)->view;
    }
    else if (childType == CCType_FramebufferView) {
        childBase = ((CCFramebufferView*)child)->view;
    }

    // 3. Perform the Linking
    if (parentView && childBase) {
        // A. Add to Parent's list (Visual hierarchy)
        arrayAddObject(parentView->subviews, child);
        
        // B. Link Child to Parent (Logic hierarchy) -> THIS WAS MISSING
        childBase->superview = (struct CCView*)parentView;
    }
    
    shouldUpdate = true;
}

//If a CCLabel exists or a CCImageView exists, it is not a class
//that inherits or is subclassed from CCView, it is its own struct
//that contains a CCView member.
//
//A CCImageView would also be its own struct that has a CCView member
//
//If a CCView is the root concept of the view heiarchy, when adding
//subviews that are a more complex type than CCView, such as CCLabel
//or CCImageView structs, the subviews member of CCView is a CCArray
//The CCArray allows to hold any type of struct, but there is no way
//to know what type of struct it is...
//
//Now, each view has a type parameter that can be checked when
//iterating through the array

void viewSetBackgroundColor(void* view, CCColor* color) {
    CCView* view1 = view;
    view1->layer->backgroundColor = color;
    shouldUpdate = true;
}

void layerSetCornerRadius(CCLayer* layer, Float cornerRadius) {
    layer->cornerRadius = cornerRadius;
    shouldUpdate = true;
}

void viewSetFrame(void* view, CCRect* frame) {
    CCView* view1 = view;
    view1->frame = frame;
    view1->layer->frame = frame;
    shouldUpdate = true;
}

void viewAddGestureRecognizer(void* view, CCGestureRecognizer* gestureRecognizer) {
    CCView* view1 = view;
    if (view1->type == CCType_Label) {
        view1 = ((CCLabel*)view)->view;
    }
    else if (view1->type == CCType_ImageView) {
        view1 = ((CCImageView*)view)->view;
    }
    gestureRecognizer->view = view1;
    arrayAddObject(view1->gestureRecognizers, gestureRecognizer);
}

// Clean up function for CCView
void freeCCView(CCView* view) {
    if (view) {
        if (view->layer) free(view->layer);
        //if (view->subviews) freeCCArray(view->subviews);
        free(view);
    }
}

void freeCCGradient(CCGradient* gradient) {
    // 1. Safety check
    if (gradient == NULL) {
        return;
    }

    // 2. Free the Colors Array
    // This assumes freeCCArray handles freeing the array struct
    // AND the objects inside it (CCColor objects).
    if (gradient->colors != NULL) {
        freeCCArray(gradient->colors);
        gradient->colors = NULL; // Safety: prevent dangling pointer
    }

    // 3. Free the Locations Array
    // This assumes freeCCArray handles freeing the CCNumber objects inside.
    if (gradient->locations != NULL) {
        freeCCArray(gradient->locations);
        gradient->locations = NULL;
    }

    // 4. Free the Gradient Struct itself
    free(gradient);
}

void freeViewHierarchy(CCView* view) {
    if (!view) return;

    // 1. SAFETY: Detach from Parent (Critical for Sub-tree deletion)
    // If you free a child but the parent is still alive, the parent
    // holds a dangling pointer and will crash if it tries to draw.
    // (Assuming you have a function to remove it from the parent's array)
    // if (view->superview) viewRemoveSubview(view->superview, view);

    // 2. Recursively free subviews
    if (view->subviews) {
        for (int i = 0; i < view->subviews->count; i++) {
            void* childObj = arrayObjectAtIndex(view->subviews, i);
            CCType type = *((CCType*)childObj);

            if (type == CCType_View) {
                freeViewHierarchy((CCView*)childObj);
            }
            else if (type == CCType_Label) {
                CCLabel* lbl = (CCLabel*)childObj;
                freeViewHierarchy(lbl->view); // Free inner view
                if (lbl->text) freeCCString(lbl->text);
                if (lbl->textColor) free(lbl->textColor); // <-- LEAK FIX
                free(lbl);
            }
            else if (type == CCType_ImageView) {
                CCImageView* img = (CCImageView*)childObj;
                freeViewHierarchy(img->view);
                // Only free the image if you own it (not cached)
                // if (img->image) freeCCImage(img->image);
                free(img);
            }
            else if (type == CCType_ScrollView) {
                CCScrollView* sv = (CCScrollView*)childObj;
                freeViewHierarchy(sv->view);
                free(sv);
            }
            // Add Button/Switch cases here if you wrap them differently!
        }
        freeCCArray(view->subviews);
    }

    // 3. Free Layer & its properties
    if (view->layer) {
        if (view->layer->gradient) freeCCGradient(view->layer->gradient);
        
        // --- LEAK FIXES ---
        if (view->layer->borderColor) free(view->layer->borderColor);
        if (view->layer->shadowColor) free(view->layer->shadowColor);
        // ------------------
        
        free(view->layer);
    }
    
    // 4. Free View Properties
    if (view->frame) freeCCRect(view->frame);
    
    // --- LEAK FIX ---
    //if (view->backgroundColor) free(view->backgroundColor);
    // ----------------

    // 5. Finally, free the struct
    free(view);
}

CCGestureRecognizer* gestureRecognizerWithType(CCGestureType gestureType, CCGestureAction action) {
    CCGestureRecognizer* newGestureRecognizer = (CCGestureRecognizer*)cc_safe_alloc(1, sizeof(CCGestureRecognizer));
    newGestureRecognizer->type = CCType_GestureRecognizer;
    if (!newGestureRecognizer) return NULL;
    
    newGestureRecognizer->gestureType = gestureType;
    newGestureRecognizer->action = action;
    
    return newGestureRecognizer;
}

// Clean up function for CCColor
void freeCCColor(CCColor* color) {
    free(color);
}

// Clean up function for CCColor
void freeCCRect(CCRect* rect) {
    free(rect);
}

// Create a new CCFont
CCFont* font(CCString* filePath, Float renderingSize) {
    CCFont* newFont = (CCFont*)cc_safe_alloc(1, sizeof(CCFont));
    newFont->type = CCType_Font;
    if (!newFont) return NULL;
    newFont->filePath = filePath;
    newFont->renderingSize = renderingSize;
    //int loadedFont = LoadFont(filePath->string, (int)renderingSize);
    //if (loadedFont != -1) {
    //    newFont->isLoaded = true;
    //}
    return newFont;
}

void labelSetDefault(CCLabel* newLabel) {
    newLabel->textColor = color(0, 0, 0, 1.0);
    newLabel->text = string();
    newLabel->textAlignment = CCTextAlignmentLeft;
    newLabel->textVerticalAlignment = CCTextVerticalAlignmentCenter;
    newLabel->lineBreakMode = CCLineBreakWordWrapping;
    newLabel->fontSize = 12.0f;
    
    //newLabel->font = defaultFont;
}

CCLabel* label(void) {
    CCLabel* newLabel = (CCLabel*)cc_safe_alloc(1, sizeof(CCLabel));
    newLabel->type = CCType_Label;
    if (!newLabel) return NULL;

    newLabel->view = view();
    if (!newLabel->view) {
        free(newLabel);
        return NULL;
    }
    
    labelSetDefault(newLabel);
    
    return newLabel;
}

// Create a new CCView with a specified frame
CCLabel* labelWithFrame(CCRect* frame) {
    CCLabel* newLabel = (CCLabel*)cc_safe_alloc(1, sizeof(CCLabel));
    newLabel->type = CCType_Label;
    if (!newLabel) return NULL;

    newLabel->view = viewWithFrame(frame);
    if (!newLabel->view) {
        free(newLabel);
        return NULL;
    }
    
    labelSetDefault(newLabel);

    return newLabel;
}

void labelSetText(CCLabel* label, CCString* text) {
    label->text = text;
    shouldUpdate = true;
}

CCFramebufferView* framebufferView(void) {
    CCFramebufferView* newFbView = (CCFramebufferView*)cc_safe_alloc(1, sizeof(CCFramebufferView));
    if (!newFbView) return NULL;

    newFbView->type = CCType_FramebufferView; // Make sure this is in your CCType enum!

    newFbView->view = view();
    if (!newFbView->view) {
        free(newFbView);
        return NULL;
    }
    
    newFbView->framebuffer = NULL;
    
    return newFbView;
}

CCFramebufferView* framebufferViewWithFrame(CCRect* frame) {
    CCFramebufferView* newFbView = (CCFramebufferView*)cc_safe_alloc(1, sizeof(CCFramebufferView));
    if (!newFbView) return NULL;

    newFbView->type = CCType_FramebufferView;

    newFbView->view = viewWithFrame(frame);
    if (!newFbView->view) {
        free(newFbView);
        return NULL;
    }
    
    newFbView->framebuffer = NULL;

    return newFbView;
}

void framebufferViewSetFramebuffer(CCFramebufferView* fbView, Framebuffer* fb) {
    if (!fbView) return;
    fbView->framebuffer = fb;
    shouldUpdate = true;
}

CCPointPath* pointPath(void) {
    CCPointPath* newPointPath = (CCPointPath*)cc_safe_alloc(1, sizeof(CCPointPath));
    newPointPath->type = CCType_PointPath;
    if (!newPointPath) return NULL;
    newPointPath->points = array();
    if (!newPointPath->points) {
        free(newPointPath);
        return NULL;
    }
    return newPointPath;
}

CCPointPath* pointPathWithPoints(CCArray* points) {
    CCPointPath* newPointPath = (CCPointPath*)cc_safe_alloc(1, sizeof(CCPointPath));
    newPointPath->type = CCType_PointPath;
    newPointPath->points = points;
    if (!newPointPath) return NULL;
    return newPointPath;
}

void pointPathAddPoint(CCPointPath* pointPath, CCPoint* point) {
    if (pointPath && pointPath->points) {
        arrayAddObject(pointPath->points, point);
    }
}

CCShapeLayer* shapeLayer(void) {
    CCShapeLayer* newShapeLayer = (CCShapeLayer*)cc_safe_alloc(1, sizeof(CCShapeLayer));
    newShapeLayer->type = CCType_ShapeLayer;
    newShapeLayer->shapeType = CCShapeTypePolygon;
    if (!newShapeLayer) return NULL;

    newShapeLayer->pointPath = pointPath();
    if (!newShapeLayer->pointPath) {
        free(newShapeLayer);
        return NULL;
    }
    
    return newShapeLayer;
}

CCShapeLayer* shapeLayerWithPointPath(CCPointPath* pointPath) {
    CCShapeLayer* newShapeLayer = (CCShapeLayer*)cc_safe_alloc(1, sizeof(CCShapeLayer));
    newShapeLayer->type = CCType_ShapeLayer;
    newShapeLayer->shapeType = CCShapeTypePolygon;
    newShapeLayer->gradient = NULL;
    if (!newShapeLayer) return NULL;

    newShapeLayer->pointPath = pointPath;
    
    return newShapeLayer;
}

// Clean up function for CCFont
void freeCCFont(CCFont* font) {
    free(font);
}

// Clean up function for CCLabel
void freeCCLabel(CCLabel* label) {
    if (label) {
        if (label->view) freeCCView(label->view);
        free(label);
    }
}

CCImage* image(void) {
    CCImage* newImage = (CCImage*)cc_safe_alloc(1, sizeof(CCImage));
    newImage->type = CCType_Image;
    if (!newImage) return NULL;
    newImage->size = ccSize(0, 0);
    return newImage;
}

CCImage* imageWithData(unsigned char* imageData, Integer width, Integer height) {
    CCImage* newImage = (CCImage*)cc_safe_alloc(1, sizeof(CCImage));
    newImage->type = CCType_Image;
    if (!newImage) return NULL;
    newImage->imageData = imageData;
    newImage->size = ccSize(width, height);
    return newImage;
}

CCImage* imageWithFile(CCString* filePath) {
    CCImage* newImage = (CCImage*)cc_safe_alloc(1, sizeof(CCImage));
    if (!newImage) return NULL;

    newImage->type = CCType_Image;
    
    // 1. Store the path. We will use this later in the graphics loop.
    // We assume filePath is a valid CCString created via ccs() or similar.
    newImage->filePath = filePath;
    
    // 2. Initialize Data to NULL (Lazy Loading)
    newImage->imageData = NULL;
    
    // 3. Initialize Size to 0,0
    // Since we haven't opened the file, we don't know the size yet.
    // This is fine because CCImageView uses the VIEW'S frame to size the image.
    newImage->size = ccSize(0, 0);
    
    newImage->texture = 0; // Unused in this software renderer implementation

    return newImage;
}

CCImageView* imageView(void) {
    CCImageView* newImageView = (CCImageView*)cc_safe_alloc(1, sizeof(CCImageView));
    newImageView->type = CCType_ImageView;
    if (!newImageView) return NULL;
    
    newImageView->alpha = 1.0;
    newImageView->ignoreTouch = false;

    newImageView->view = view();
    if (!newImageView->view) {
        free(newImageView);
        return NULL;
    }
    
    return newImageView;
}

CCImageView* imageViewWithFrame(CCRect* frame) {
    CCImageView* newImageView = (CCImageView*)cc_safe_alloc(1, sizeof(CCImageView));
    newImageView->type = CCType_ImageView;
    if (!newImageView) return NULL;
    
    newImageView->alpha = 1.0;
    newImageView->ignoreTouch = false;

    newImageView->view = viewWithFrame(frame);
    if (!newImageView->view) {
        free(newImageView);
        return NULL;
    }

    return newImageView;
}

void imageViewSetImage(CCImageView* imageView, CCImage* image) {
    imageView->image = image;
    shouldUpdate = true;
}

CCScrollView* scrollViewWithFrame(CCRect* frame) {
    CCScrollView* sv = (CCScrollView*)cc_safe_alloc(1, sizeof(CCScrollView));
    if (!sv) return NULL;
    sv->type = CCType_ScrollView;

    // 1. Create Viewport (The fixed window)
    sv->view = viewWithFrame(frame);
    sv->view->layer->masksToBounds = true; // CRITICAL: Clip children
    sv->view->backgroundColor = color(0,0,0,0); // Default transparent

    // 2. Create Content View (The moving sheet)
    // Starts at 0,0 with same size as viewport (will grow later)
    sv->contentView = viewWithFrame(ccRect(0, 0, frame->size->width, frame->size->height));
    sv->contentView->backgroundColor = color(0,0,0,0);
    
    // Add content to viewport
    viewAddSubview(sv->view, sv->contentView);

    // 3. Init properties
    sv->contentSize = ccSize(frame->size->width, frame->size->height);
    sv->contentOffset = ccPoint(0, 0);

    return sv;
}

void scrollViewSetContentSize(CCScrollView* sv, CCSize* size) {
    if (!sv) return;
    // Update struct
    sv->contentSize->width = size->width;
    sv->contentSize->height = size->height;
    
    // Update actual view frame
    sv->contentView->frame->size->width = size->width;
    sv->contentView->frame->size->height = size->height;
    
    printf("scrollViewSetContentSize");
    printf("sv->contentView->layer %d\n", (sv->contentView->layer == NULL)? 0 : 1);
    printf("sv->contentView->layer->frame %d\n", (sv->contentView->layer->frame == NULL)? 0 : 1);
    
    
    // Also update layer frame (needed for borders/bg)
    sv->contentView->layer->frame->size->width = size->width;
    sv->contentView->layer->frame->size->height = size->height;
}

void scrollViewSetContentOffset(CCScrollView* sv, CCPoint* offset) {
    if (!sv) return;

    float contentH = sv->contentSize->height;
    float viewportH = sv->view->frame->size->height;
    
    // Calculate Max Offset
    float maxOffsetY = contentH - viewportH;
    if (maxOffsetY < 0) maxOffsetY = 0; // Content is smaller than view

    // --- DEBUG LOG ---
    //printf("DEBUG SCROLL: Request=%.2f, ContentH=%.2f, ViewH=%.2f, MaxOffset=%.2f\n", offset->y, contentH, viewportH, maxOffsetY);
    // -----------------
    // Uses the ESP log system (safer)
    FreeOSLogI("", "Scroll: Req=%.2f, Content=%.2f, View=%.2f", offset->y, contentH, viewportH);

    // Clamp
    if (offset->y < 0) offset->y = 0;
    if (offset->y > maxOffsetY) offset->y = maxOffsetY;

    // Update State
    sv->contentOffset->y = offset->y;
    sv->contentOffset->x = offset->x;

    // Move View
    sv->contentView->frame->origin->y = -(offset->y);
    sv->contentView->layer->frame->origin->y = -(offset->y);
}


CCTextView* textViewWithFrame(CCRect* frame) {
    CCTextView* tv = (CCTextView*)cc_safe_alloc(1, sizeof(CCTextView));
    if (!tv) return NULL;
    tv->type = CCType_TextView;

    // 1. Create the Scroll Container
    tv->scrollView = scrollViewWithFrame(frame);
    
    // 2. Create the Label
    // Width matches the frame, Height will be dynamic
    tv->label = labelWithFrame(ccRect(0, 0, frame->size->width, frame->size->height));
    tv->label->lineBreakMode = CCLineBreakWordWrapping;
    tv->label->textAlignment = CCTextAlignmentLeft;
    
    // Disable the border on the label itself (so you don't see a black box)
    if (tv->label->view->layer) tv->label->view->layer->borderWidth = 0;
    
    // 3. Add Label to Content
    // ------------------------------------------------------------
    // CRITICAL FIX: Add 'tv->label', NOT 'tv->label->view'
    // This ensures the renderer sees it as CCType_Label (Text),
    // not CCType_View (Rectangle).
    // ------------------------------------------------------------
    viewAddSubview(tv->scrollView->contentView, tv->label);

    return tv;
}

/*void textViewSetText(CCTextView* tv, CCString* text, FT_Face face) {
    if (!tv || !text) return;

    // 1. Set the text
    labelSetText(tv->label, text);

    // 2. Calculate the required height
    // We use your existing helper (make sure it's exposed in header)
    // If you don't have the helper exposed, you can estimate: lines * fontSize
    
    // Setup a temporary format for measurement
    TextFormat fmt = {
        .alignment = TEXT_ALIGN_LEFT,
        .wrapMode = TEXT_WRAP_MODE_WHOLE_WORD,
        .lineSpacing = (int)tv->label->lineSpacing,
        .glyphSpacing = (int)tv->label->glyphSpacing
    };
    
    int calculatedHeight = measureTextHeight(
        face,
        text->string,
        (int)tv->label->view->frame->size->width,
        (int)tv->label->fontSize,
        &fmt
    );
    
    // Add padding
    calculatedHeight += 20;

    // 2. Get the Current Viewport Width/Height
        // We need to trust the width set in textViewWithFrame
        float viewportWidth = tv->scrollView->view->frame->size->width;
        
        // 3. Update the Label's Frame
        tv->label->view->frame->size->height = (float)calculatedHeight;
        tv->label->view->layer->frame->size->height = (float)calculatedHeight;

        // 4. CRITICAL: Update the ScrollView Content Size
        // This is what enables the scrolling math (MaxOffset = 365 - 300 = 65)
        CCSize* newContentSize = ccSize(viewportWidth, (float)calculatedHeight);
        
        scrollViewSetContentSize(tv->scrollView, newContentSize);
    
    printf("calculated height of scroll view: %d", calculatedHeight);
    
    // Clean up temp size struct if ccSize mallocs
    free(newContentSize);
}*/

void textViewSetText(CCTextView* tv, CCString* text) {
    if (!tv || !text) return;
    
    
    
    // Safety Check: Ensure the cache properties are set!
    if (!tv->ftManager || !tv->ftImageCache || !tv->ftCMapCache) {
        printf("Error: TextView cache properties are NULL. Assign them before setting text.\n");
        return;
    }

    // 1. Set the text content
    labelSetText(tv->label, text);

    // 2. Identify the Font ID (Path)
    const char* fontPath = cStringOfString(tv->label->fontName);
    //if (!fontPath) fontPath = "spiffs/fonts/Monitorica-Bd.ttf";
    if (!fontPath) {
        printf("\ntextViewSetText !fontPath\n");
    }
    
    printf("\nfontPath %s\n", fontPath);

    // 3. Setup Format
    TextFormat fmt = {
        .alignment = TEXT_ALIGN_LEFT,
        .wrapMode = TEXT_WRAP_MODE_WHOLE_WORD,
        .lineSpacing = (int)tv->label->lineSpacing,
        .glyphSpacing = (int)tv->label->glyphSpacing
    };
    
    // 4. Calculate Height using the STRUCT's properties
    int calculatedHeight = measureTextHeightCached(
        tv->ftManager,       // <--- Use the struct property
        tv->ftImageCache,    // <--- Use the struct property
        tv->ftCMapCache,     // <--- Use the struct property
        (FTC_FaceID)fontPath,
        text->string,
        (int)tv->label->view->frame->size->width,
        (int)tv->label->fontSize,
        &fmt
    );
    
    calculatedHeight += 20; // Padding

    // 5. Update UI
    float viewportWidth = tv->scrollView->view->frame->size->width;
    
    printf("\ncalculatedHeight %d\n", calculatedHeight);
    
    printf("\tv->label->view->layer %d\n", (tv->label->view->layer == NULL)? 0 : 1);
    printf("\tv->label->view->layer->frame %d\n", (tv->label->view->layer->frame == NULL)? 0 : 1);
    
    //    v->label->view->layer 1
    //    v->label->view->layer->frame 0
        
    tv->label->view->frame->size->height = (float)calculatedHeight;
    tv->label->view->layer->frame->size->height = (float)calculatedHeight;

    CCSize* newContentSize = ccSize(viewportWidth, (float)calculatedHeight);
    
    printf("tv->scrollView %d %f %f\n", (tv->label->view->layer == NULL)? 0 : 1, newContentSize->width, newContentSize->height);
    
    scrollViewSetContentSize(tv->scrollView, newContentSize);
    
    free(newContentSize);
}


CCGraphicsContext* graphicsContextCreate(Float width, Float height) {
    CCGraphicsContext* newGraphicsContext = (CCGraphicsContext*)cc_safe_alloc(1, sizeof(CCGraphicsContext));
    newGraphicsContext->type = CCType_GraphicsContext;
    if (!newGraphicsContext) return NULL;

    newGraphicsContext->size = ccSize(width, height);
    
    //GLuint fbo, texture;
    //createFBO(&fbo, &texture, (int)width, (int)height);
    //newGraphicsContext->fbo = fbo;
    //newGraphicsContext->texture = texture;
    
    return newGraphicsContext;
}

#define EPSILON 1e-6

CCTransform* transform(void) {
    CCTransform* newTransform = (CCTransform*)cc_safe_alloc(1, sizeof(CCTransform));
    if (!newTransform) return NULL;
    newTransform->type = CCType_Transform;
    return newTransform;
}

CCTransform* transformWithMatrix(float* matrix) {
    CCTransform* newTransform = transform();
    //newTransform->matrix = matrix;
    return newTransform;
}

CCTransform* transformRotate(Float rotationAngle) {
    CCTransform* newTransform = transform();
    newTransform->rotationAngle = rotationAngle;
    //newTransform->matrix = CCTransformRotateMatrix(rotationAngle);
    return newTransform;;
}

CCTransform* transformTranslation(Float x, Float y) {
    CCTransform* newTransform = transform();
    newTransform->translation = ccPoint(x, y);
    //newTransform->matrix = CCTransformTranslateMatrix(x, y);
    return newTransform;;
}

CCTransform* transformScale(Float x, Float y) {
    CCTransform* newTransform = transform();
    newTransform->scale = ccSize(x, y);
    //newTransform->matrix = CCTransformScaleMatrix(x, y);
    return newTransform;;
}

CCTransform* transformConcat(CCTransform* transform1, CCTransform* transform2) {
    CCTransform *newTransform = transform();
    //newTransform->matrix = CCTransformConcatMatrix(transform1->matrix, transform2->matrix);
    return newTransform;
}

bool transformEqualsTransform(CCTransform* transform1, CCTransform* transform2) {
    //return transformEqualsTransformMatrix(transform1->matrix, transform2->matrix);
    return false;
}

CCTransform3D* transform3D(void) {
    CCTransform3D* newTransform = (CCTransform3D*)cc_safe_alloc(1, sizeof(CCTransform3D));
    newTransform->type = CCType_Transform3D;
    if (!newTransform) return NULL;
    //newTransform->matrix = CCTransform3DIdentity();
    return newTransform;
}

CCTransform3D* transform3DWithMatrix(CCTransform3DMatrix matrix){
    CCTransform3D* newTransform = (CCTransform3D*)cc_safe_alloc(1, sizeof(CCTransform3D));
    newTransform->type = CCType_Transform3D;
    if (!newTransform) return NULL;
    //newTransform->matrix = matrix;
    return newTransform;
}

float afb(CCView* view) {
    return view->frame->origin->y+view->frame->size->height;
}

float afr(CCView* view) {
    return view->frame->origin->x+view->frame->size->width;
}

static Float windowWidth = 1280;
static Float windowHeight = 800;

static int drawLayerCount = 0;

void drawLayer(void* view) {
    
    printf("draw layer: %d %s", drawLayerCount, cStringOfString(stringForType(((CCView*)view)->type)));
    drawLayerCount++;
    
    CCView* viewObject = (CCView*)view;
    CCLayer* layer = NULL;
    
    bool isImage = false;
    bool isText = false;
    
    CCImage* image = NULL;
    
    if (viewObject->type == CCType_View) {
        layer = viewObject->layer;
    }
    else if (viewObject->type == CCType_Label) {
        layer = ((CCLabel*)viewObject)->view->layer;
        isText = true;
    }
    else if (viewObject->type == CCType_ImageView) {
        CCImageView* imageViewObject = ((CCImageView*)viewObject);
        layer = imageViewObject->view->layer;
        isImage = true;
        image = imageViewObject->image;
    }
    
    if (!layer) {
        printf("no layer error");
        return;
    }
    
    printf("\ndraw layer of rect %f %f %f %f and color %f %f %f %f\n", layer->frame->origin->x, layer->frame->origin->y, layer->frame->size->width, layer->frame->size->height, layer->backgroundColor->r, layer->backgroundColor->g, layer->backgroundColor->b, layer->backgroundColor->a);
    
    float cornerRadius = layer->cornerRadius; // Assume this field is added to the layer struct
    
    if (cornerRadius == 0) {
        
    }
    else {
        
    }
    
    //Draw sublayers of layer
    for (int i = 0; i < layer->sublayers->count; i++) {
        CCLayer* layer1 = arrayObjectAtIndex(layer->sublayers, i);
        drawLayer(layer1);
    }
    
}

void drawView(void* view) {
    //Draw this view's layer. Then draw all subviews of this view
    
    printf("\ndraw drawView: %d %s\n", drawLayerCount, cStringOfString(stringForType(((CCView*)view)->type)));
    drawLayer(view);
    
    CCView* viewObject = NULL;
    if (((CCView*)view)->type == CCType_View) {
        viewObject = view;
    }
    else if (((CCView*)view)->type == CCType_Label) {
        viewObject = ((CCLabel*)view)->view;
    }
    else if (((CCView*)view)->type == CCType_ImageView) {
        viewObject = ((CCImageView*)view)->view;
    }
    
    if (!viewObject) {
        printf("no view object error");
        return;
    }
    
    for (int i = 0; i < viewObject->subviews->count; i++) {
        CCView* view1 = arrayObjectAtIndex(viewObject->subviews, i);
        drawView(view1);
    }
    
}

static bool isMousePressed = false;
static CCView* mainWindowView;
static CCPoint* mousePosition;

void handleGestureOnView(CCView* view1) {
    for (int ii = 0; ii < view1->gestureRecognizers->count; ii++) {
        printf("\ngesture recognizer found\n");
        CCGestureRecognizer* gestureRecognizer = arrayObjectAtIndex(view1->gestureRecognizers, ii);
        if (rectContainsPoint(view1->frame, mousePosition)) {
            CCGestureType gestureTypeForMouseEvent = (isMousePressed) ? CCGestureDown : CCGestureUp;
            printf("\nrect contains point\n");
            gestureRecognizer->gestureType = gestureTypeForMouseEvent;
            if (gestureRecognizer->gestureType == gestureTypeForMouseEvent) {
                gestureRecognizer->action(gestureRecognizer);
                printf("\ngesture recognizer action\n");
            }
        }
    }
}

void handleClickOnView(CCView* view) {
    printf("\nhandle click on view\n");
    
    CCView* view1 = view;
    if (view1->type == CCType_Label) {
        view1 = ((CCLabel*)view)->view;
    }
    else if (view1->type == CCType_ImageView) {
        view1 = ((CCImageView*)view)->view;
    }
    
    handleGestureOnView(view1);
    for (int i = 0; i < view1->subviews->count; i++) {
        CCView* view2 = arrayObjectAtIndex(view1->subviews, i);
        CCView* view3 = view2;
        if (view2->type == CCType_Label) {
            view3 = ((CCLabel*)view2)->view;
        }
        else if (view2->type == CCType_ImageView) {
            view3 = ((CCImageView*)view2)->view;
        }
        handleGestureOnView(view3);
        handleClickOnView(view2);
    }
}

void handleClickOnWindowViews(void) {
    handleClickOnView(mainWindowView);
}

static CCView* topBarView;

static bool isRed = false;

void topBarClickAction(void* gestureRecognizer) {
    printf("\ntop click action\n");
    if (!isRed) {
        isRed = true;
        viewSetBackgroundColor(topBarView, color(1, 0.2, 0.2, 1.0));
    }
    else {
        isRed = false;
        viewSetBackgroundColor(topBarView, color(0.2, 1.0, 1.0, 1.0));
    }
    shouldUpdate = true;
}

static CCImageView* imageView1;

void imageViewClickAction(void* gestureRecognizer) {
    printf("imageViewClickAction");
    imageView1->view->transform = transformRotate(M_PI_4);
    shouldUpdate = true;
}

static CCString* programLocaleIdentifier = NULL;
static CCString* programTimeZoneIdentifier = NULL;

static CCLocale* programLocale = NULL;
static CCTimeZone* programTimeZone = NULL;

void loadMainProgramViews(void) {
    
    
    mainWindowView = viewWithFrame(ccRect(0, 0, windowWidth, windowHeight));
    mainWindowView->backgroundColor = color(1, 1, 1, 1);
    
    topBarView = viewWithFrame(ccRect(0, 0, windowWidth-100, 120));
    viewSetBackgroundColor(topBarView, color(0, 0, 0, 0.1));
    layerSetCornerRadius(topBarView->layer, 30.0f);
    viewAddSubview(mainWindowView, topBarView);
    
    CCGestureRecognizer* topBarClick = gestureRecognizerWithType(CCGestureUp, topBarClickAction);
    viewAddGestureRecognizer(topBarView, topBarClick);
    
    CCView* bottomBarView = viewWithFrame(ccRect(0, windowHeight-120, windowWidth-200, 120));
    viewSetBackgroundColor(bottomBarView, color(0, 1, 0, 1.0));
    layerSetCornerRadius(bottomBarView->layer, 30.0f);
    viewAddSubview(mainWindowView, bottomBarView);
    
    imageView1 = imageViewWithFrame(ccRect(500, 200, 100, 100));
    CCImage* imageObject = imageWithFile(ccs("testImage.png"));
    //CCImage* imageObject = imageWithFile(resourceFilePath(ccs("testImage.png")));
    
    //size_t dataSize;
    //unsigned char* data = rgbaToPngData(imageObject->imageData, imageObject->size->width, imageObject->size->height, &dataSize);
    //CCData* data1 = dataWithBytes(data, dataSize);
    //dataWriteToFile(data1, ccs("/Users/chrisgalzerano/Desktop/testImage767.png"));
    
    imageViewSetImage(imageView1, imageObject);
    viewAddSubview(mainWindowView, imageView1);
    
    CCGestureRecognizer* imageViewClick = gestureRecognizerWithType(CCGestureUp, imageViewClickAction);
    viewAddGestureRecognizer(imageView1, imageViewClick);
    
    printf("\nmainWindowView subviews: %d\n", mainWindowView->subviews->count);
    
}

void printCurrentWorkingDirectory() {
    char cwd[PATH_MAX];  // Define a buffer to hold the current working directory path
    // Attempt to get the current working directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current Working Directory: %s\n", cwd);
    } else {
        perror("getcwd() error");  // Print an error message if getcwd fails
    }
}

char* getResourceFilePath(const char* pathToAppend) {
    char cwd[PATH_MAX];  // Buffer to store the current working directory

    // Attempt to get the current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return NULL;
    }

    // Calculate the total length needed for the resulting path
    size_t totalLength = strlen(cwd) + strlen(pathToAppend) + 2; // Extra space for '/' and '\0'

    // Allocate memory for the full path
    char* fullPath = (char*)cc_safe_alloc(1, totalLength);
    if (fullPath == NULL) {
        perror("cc_safe_alloc(1, ) error");
        return NULL;
    }

    // Construct the full path
    snprintf(fullPath, totalLength, "%s/%s", cwd, pathToAppend);

    return fullPath;
}

CCString* resourceFilePath(CCString* filePath) {
    return ccs(getResourceFilePath(cStringOfString(filePath)));
}

static CCShapeLayer* shapeLayerTest;

int mainProgram(void) {
    programLocaleIdentifier = ccs("en_US");
    programTimeZoneIdentifier = ccs("America\/New_York");
    
    CCArray* points = arrayWithObjects(ccPoint(100, 100), ccPoint(100, 200), ccPoint(200, 200), ccPoint(200, 30), NULL);
    
    /*CCArray *points = array();
    arrayAddObject(points, ccPoint(100, 100));
    arrayAddObject(points, ccPoint(100, 200));
    arrayAddObject(points, ccPoint(200, 100));
    arrayAddObject(points, ccPoint(200, 30));*/
    
    CCString* testPath = ccs("/Users/chrisgalzerano/Desktop");
    CCArray* components = stringComponentsSeparatedByString(testPath, ccs("/"));
    printf("\ncomponents %d\n", components->count);
    for (int i = 0; i < components->count; i++) {
        printf("\ncomponent %d %s\n", i, cStringOfString(arrayObjectAtIndex(components, i)));
    }
    
    printf("main program points %d", points->count);
    shapeLayerTest = shapeLayerWithPointPath(pointPathWithPoints(points));
    shapeLayerTest->fillColor = color(0, 1.0, 0, 1.0);
    
    printCurrentWorkingDirectory();

    // Dirty state to track if the graphics need to be re-rendered
    bool isDirty = true;
    
    loadMainProgramViews();
    
    //CCLocale* locale = localeWithIdentifier(programLocaleIdentifier);
    //CCTimeZone* timeZone = timeZoneWithName(programTimeZoneIdentifier);
    
    const char* fontPath = getResourceFilePath("proximanovaRegular.ttf");
    int fontSize = 48;
    //LoadFont(fontPath, fontSize);
    
    //defaultFont = font(ccs(fontPath), 48);

    CCView* viewTest = viewWithFrame(ccRect(100, 100, 200, 120));
    viewSetBackgroundColor(viewTest, color(0, 1, 0, 1));
    CCLayer* layer = viewTest->layer;
    
    return 0;
}


//String Functions
// Function to create a CCString from a C string
CCString* string(void) {
    CCString* newString = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    newString->type = CCType_String;
    if (!newString) {
        return NULL;
    }
    newString->length = 0;
    newString->string = "";

    return newString;
}

CCString* ccs(const char * string) {
    if (string == NULL) {
        return NULL;
    }

    CCString* newString = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    newString->type = CCType_String;
    if (!newString) {
        return NULL;
    }

    newString->length = strlen(string);
    newString->string = (char*)cc_safe_alloc(1, newString->length + 1); // +1 for null terminator

    if (newString->string) {
        strcpy(newString->string, string);
    } else {
        free(newString);
        return NULL;
    }

    return newString;
}

// Function to create a CCString from a C string (identical to ccs)
CCString* stringWithCString(const char * string) {
    return ccs(string);
}

// Function to create a CCString with formatted content
CCString* stringWithFormat(const char* format, ...) {
    if (format == NULL) {
        return NULL;
    }

    va_list args;
    va_start(args, format);

    int size = vsnprintf(NULL, 0, format, args) + 1; // Determine the size needed
    va_end(args);
    
    va_start(args, format);
    char* buffer = (char*)cc_safe_alloc(1, size);

    if (!buffer) {
        va_end(args);
        return NULL;
    }

    vsnprintf(buffer, size, format, args);
    va_end(args);

    CCString* formattedString = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    formattedString->type = CCType_String;
    if (!formattedString) {
        free(buffer);
        return NULL;
    }

    formattedString->length = size - 1; // Exclude null terminator
    formattedString->string = buffer;

    return formattedString;
}

// Function to create a substring based on a specified range
CCString* substringWithRange(CCString* string, CCRange range) {
    if (string == NULL || string->string == NULL || range.loc + range.len > string->length) {
        return NULL;
    }

    CCString* substring = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    substring->type = CCType_String;
    if (!substring) {
        return NULL;
    }

    substring->length = range.len;
    substring->string = (char*)cc_safe_alloc(1, substring->length + 1);

    if (substring->string) {
        strncpy(substring->string, string->string + range.loc, range.len);
        substring->string[range.len] = '\0';
    } else {
        free(substring);
        return NULL;
    }

    return substring;
}

// Function to create a substring from a specific index to the end of the string
CCString* substringFromIndex(CCString* string, int index) {
    if (string == NULL || string->string == NULL || index < 0 || index > string->length) {
        return NULL;
    }

    CCString* substring = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    substring->type = CCType_String;
    if (!substring) {
        return NULL;
    }

    substring->length = string->length - index;
    substring->string = (char*)cc_safe_alloc(1, substring->length + 1);

    if (substring->string) {
        strcpy(substring->string, string->string + index);
    } else {
        free(substring);
        return NULL;
    }

    return substring;
}

// Function to create a substring from the beginning of the string up to a specific index
CCString* substringToIndex(CCString* string, int index) {
    if (string == NULL || string->string == NULL || index < 0 || index > string->length) {
        return NULL;
    }

    CCString* substring = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    substring->type = CCType_String;
    if (!substring) {
        return NULL;
    }

    substring->length = index;
    substring->string = (char*)cc_safe_alloc(1, substring->length + 1);

    if (substring->string) {
        strncpy(substring->string, string->string, index);
        substring->string[index] = '\0';
    } else {
        free(substring);
        return NULL;
    }

    return substring;
}

// Function to append one CCString to another and return a new CCString
CCString* stringByAppendingString(CCString* string, CCString* string1) {
    if (!string || !string->string) return string1;
    if (!string1 || !string1->string) return string;

    // Allocate space for the new concatenated string
    int newLength = string->length + string1->length;
    char* newStr = (char*)cc_safe_alloc(1, newLength + 1); // +1 for null terminator
    if (!newStr) return NULL;

    // Copy both strings into the new allocated space
    strcpy(newStr, string->string);
    strcat(newStr, string1->string);

    // Create new CCString
    CCString* newString = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    newString->type = CCType_String;
    if (!newString) {
        free(newStr);
        return NULL;
    }

    newString->length = newLength;
    newString->string = newStr;

    return newString;
}

// Function to append formatted content to a CCString and return a new CCString
CCString* stringByAppendingFormat(CCString* string, const char* format, ...) {
    if (!string || !string->string || !format) return NULL;

    va_list args;
    va_start(args, format);

    // Calculate length needed for formatted string
    int formatLength = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (formatLength < 0) return NULL;

    va_start(args, format);
    char* formatStr = (char*)cc_safe_alloc(1, formatLength + 1);
    if (!formatStr) {
        va_end(args);
        return NULL;
    }

    vsnprintf(formatStr, formatLength + 1, format, args);
    va_end(args);

    // Create the concatenated string
    int newLength = string->length + formatLength;
    char* newStr = (char*)cc_safe_alloc(1, newLength + 1);
    if (!newStr) {
        free(formatStr);
        return NULL;
    }

    strcpy(newStr, string->string);
    strcat(newStr, formatStr);

    free(formatStr);

    // Create new CCString
    CCString* newString = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    newString->type = CCType_String;
    if (!newString) {
        free(newStr);
        return NULL;
    }

    newString->length = newLength;
    newString->string = newStr;

    return newString;
}

CCArray* stringPathComponents(CCString* string) {
    return stringComponentsSeparatedByString(string, ccs("/"));
}

CCString* stringLastPathComponent(CCString* string) {
    CCArray* array = stringComponentsSeparatedByString(string, ccs("/"));
    if (array->count > 0) {
        return arrayObjectAtIndex(array, array->count-1);
    }
    return NULL;
}

CCString* stringFileExtension(CCString* string) {
    CCString* lastPathComponent = stringLastPathComponent(string);
    CCArray* components = stringComponentsSeparatedByString(lastPathComponent, ccs("."));
    if (components->count > 1) {
        return arrayObjectAtIndex(components, 1);
    }
    else {
        return ccs("");
    }
}

bool stringContainsString(CCString* string, CCString* containsString) {
    if (string == NULL || string->string == NULL || containsString == NULL || containsString->string == NULL) {
        return false;
    }

    // Use strstr to check if containsString->string is found within string->string
    return strstr(string->string, containsString->string) != NULL;
}

// Function to check if one CCString is equal to another CCString
bool stringEqualsString(CCString* string, CCString* equalsString) {
    if (string == NULL || string->string == NULL || equalsString == NULL || equalsString->string == NULL) {
        return false;
    }

    // Use strcmp to check for string equality
    return strcmp(string->string, equalsString->string) == 0;
}

CCString* stringLowercase(CCString* string) {
    if (string == NULL || string->string == NULL) {
        return NULL;
    }
    
    CCString* lowerString = ccs(string->string);
    for (UInteger i = 0; i < lowerString->length; i++) {
        lowerString->string[i] = tolower(lowerString->string[i]);
    }
    return lowerString;
}

// Function to convert the entire CCString to uppercase
CCString* stringUppercase(CCString* string) {
    if (string == NULL || string->string == NULL) {
        return NULL;
    }
    
    CCString* upperString = ccs(string->string);
    for (UInteger i = 0; i < upperString->length; i++) {
        upperString->string[i] = toupper(upperString->string[i]);
    }
    return upperString;
}

// Function to capitalize the first letter of each word in a CCString
CCString* stringCapitalized(CCString* string) {
    if (string == NULL || string->string == NULL) {
        return NULL;
    }
    
    CCString* capitalizedString = ccs(string->string);
    bool newWord = true;

    for (UInteger i = 0; i < capitalizedString->length; i++) {
        if (isspace(capitalizedString->string[i])) {
            newWord = true;
        } else if (newWord) {
            capitalizedString->string[i] = toupper(capitalizedString->string[i]);
            newWord = false;
        } else {
            capitalizedString->string[i] = tolower(capitalizedString->string[i]);
        }
    }
    return capitalizedString;
}

// Function to append one CCString to another, modifying the first CCString
void appendString(CCString* string, CCString* stringToAppend) {
    if (!string || !string->string || !stringToAppend || !stringToAppend->string) return;

    int newLength = string->length + stringToAppend->length;
    char* newStr = (char*)cc_safe_realloc(string->string, newLength + 1); // +1 for null terminator
    if (!newStr) return;

    strcat(newStr, stringToAppend->string);

    string->string = newStr;
    string->length = newLength;
}

// Function to append formatted content, modifying the original CCString
void appendFormat(CCString* string, const char* format, ...) {
    if (!string || !string->string || !format) return;

    va_list args;
    va_start(args, format);

    // Calculate length needed for formatted string
    int formatLength = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (formatLength < 0) return;

    va_start(args, format);
    char* formatStr = (char*)cc_safe_alloc(1, formatLength + 1);
    if (!formatStr) {
        va_end(args);
        return;
    }

    vsnprintf(formatStr, formatLength + 1, format, args);
    va_end(args);

    int newLength = string->length + formatLength;
    char* newStr = (char*)cc_safe_realloc(string->string, newLength + 1);
    if (!newStr) {
        free(formatStr);
        return;
    }

    strcat(newStr, formatStr);

    free(formatStr);

    string->string = newStr;
    string->length = newLength;
}

CCString* replaceOccurencesOfStringWithString(CCString* string, CCString* target, CCString* replacement) {
    if (string == NULL || target == NULL || replacement == NULL ||
        string->string == NULL || target->string == NULL || replacement->string == NULL) {
        return NULL;
    }
    // Calculate occurrences of target in string.
    int occurrences = 0;
    const char* tempStr = string->string;
    while ((tempStr = strstr(tempStr, target->string)) != NULL) {
        occurrences++;
        tempStr += target->length;
    }

    // Calculate new length after replacement.
    int newLength = string->length + occurrences * (replacement->length - target->length);
    char* buffer = (char*)cc_safe_alloc(1, newLength + 1); // +1 for null terminator

    if (!buffer) {
        return NULL;
    }
    
    // Create the new string with replacements.
    char* srcPtr = string->string;
    char* dstPtr = buffer;
    const char* matchPtr;

    while ((matchPtr = strstr(srcPtr, target->string)) != NULL) {
        int prefixLength = matchPtr - srcPtr;
        strncpy(dstPtr, srcPtr, prefixLength);
        dstPtr += prefixLength;
        strncpy(dstPtr, replacement->string, replacement->length);
        dstPtr += replacement->length;
        srcPtr = matchPtr + target->length;
    }

    // Copy remaining part of the original string.
    strcpy(dstPtr, srcPtr);

    CCString* resultString = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    resultString->type = CCType_Array;
    if (!resultString) {
        free(buffer);
        return NULL;
    }
    
    resultString->length = newLength;
    resultString->string = buffer;

    return resultString;
}

// Function to split a string into components using a separator string
CCArray* stringComponentsSeparatedByString(CCString* string, CCString* separator) {
    if (string == NULL || separator == NULL || string->string == NULL || separator->string == NULL) {
        return NULL;
    }

    int occurrences = 0;
    const char* tempStr = string->string;
    while ((tempStr = strstr(tempStr, separator->string)) != NULL) {
        occurrences++;
        tempStr += separator->length;
    }

    CCArray* components = (CCArray*)cc_safe_alloc(1, sizeof(CCArray));
    components->type = CCType_Array;
    if (!components) return NULL;

    components->count = occurrences + 1;
    components->array = cc_safe_alloc(1, (occurrences + 1) * sizeof(void*));

    if (!components->array) {
        free(components);
        return NULL;
    }

    const char* start = string->string;
    const char* end = NULL;
    int index = 0;

    while ((end = strstr(start, separator->string)) != NULL) {
        int partLength = end - start;
        CCString* part = (CCString*)cc_safe_alloc(1, sizeof(CCString));
        part->type = CCType_String;
        if (!part) {
            free(components->array);
            free(components);
            return NULL;
        }

        part->length = partLength;
        part->string = (char*)cc_safe_alloc(1, partLength + 1);
        strncpy(part->string, start, partLength);
        part->string[partLength] = '\0';
        
        components->array[index++] = part;
        start = end + separator->length;
    }

    CCString* part = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    part->type = CCType_String;
    if (!part) {
        free(components->array);
        free(components);
        return NULL;
    }

    int partLength = string->length - (start - string->string);
    part->length = partLength;
    part->string = (char*)cc_safe_alloc(1, partLength + 1);
    strcpy(part->string, start);

    components->array[index] = part;
    
    //This might need mixed into the function above but is
    //technically fine like this for now or a while
    //bug where this array has first item blank as one extra object
    if (((CCString*)arrayObjectAtIndex(components, 0))->length == 0) {
        arrayDeleteObjectAtIndex(components, 0);
    }

    return components;
}

// Function to combine an array of strings into a single string with a combiner
CCString* stringsCombinedWithString(CCArray* strings, CCString* combiner) {
    if (strings == NULL || combiner == NULL || strings->count == 0) {
        return NULL;
    }

    int newLength = 0;

    // Calculate combined string length including combiners
    for (int i = 0; i < strings->count; ++i) {
        CCString* stringPart = (CCString*)strings->array[i];
        newLength += stringPart->length;
    }
    newLength += (strings->count - 1) * combiner->length;

    // Allocate memory for combined string
    char* combinedString = (char*)cc_safe_alloc(1, newLength + 1); // +1 for null terminator
    if (!combinedString) {
        return NULL;
    }

    // Build the combined string
    char* current = combinedString;
    for (int i = 0; i < strings->count; ++i) {
        CCString* stringPart = (CCString*)strings->array[i];
        strcpy(current, stringPart->string);
        current += stringPart->length;
        if (i < strings->count - 1) {
            strcpy(current, combiner->string);
            current += combiner->length;
        }
    }
    *current = '\0'; // Null-terminate

    CCString* result = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    result->type = CCType_String;
    if (!result) {
        free(combinedString);
        return NULL;
    }

    result->length = newLength;
    result->string = combinedString;

    return result;
}

CCData* stringDataWithEncoding(CCString* string, StringEncoding encoding) {
    if (!string || !string->string || encoding != UTF8StringEncoding) {
        return NULL; // Handle invalid input or unsupported encoding
    }

    CCData* data = (CCData*)cc_safe_alloc(1, sizeof(CCData));
    data->type = CCType_Data;
    if (!data) {
        return NULL;
    }

    data->length = string->length;
    data->bytes = cc_safe_alloc(1, data->length);
    if (!data->bytes) {
        free(data);
        return NULL;
    }

    memcpy(data->bytes, string->string, data->length);
    return data;
}


CCString* stringFromDataWithEncoding(CCData* data, StringEncoding encoding) {
    if (!data || !data->bytes || encoding != UTF8StringEncoding) {
        return NULL; // Handle invalid input or unsupported encoding
    }

    CCString* string = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    string->type = CCType_String;
    if (!string) {
        return NULL;
    }

    string->length = (int)data->length;
    string->string = (char*)cc_safe_alloc(1, data->length + 1);
    if (!string->string) {
        free(string);
        return NULL;
    }

    memcpy(string->string, data->bytes, data->length);
    string->string[data->length] = '\0'; // Null-terminate the string
    return string;
}

CCString* stringWithContentsOfFile(CCString* filePath) {
    CCData* fileData = dataWithContentsOfFile(filePath);
    CCString* fileString = stringFromDataWithEncoding(fileData, UTF8StringEncoding);
    return fileString;
}

// Function to get the C-style string from a CCString
const char* cStringOfString(CCString* string) {
    if (!string) {
        return NULL;  // Handle the case where the input is NULL
    }
    return string->string;  // Return the internal C-style string
}

// Function to convert a CCString to an int
int stringIntValue(CCString* string) {
    if (!string || !string->string) {
        return 0;  // Default return value
    }
    return (int)strtol(string->string, NULL, 10);  // Convert to long and cast to int
}

// Function to convert a CCString to a float
float stringFloatValue(CCString* string) {
    if (!string || !string->string) {
        return 0.0f;  // Default return value
    }
    return strtof(string->string, NULL);  // Direct conversion to float
}

// Function to convert a CCString to a long
long stringLongValue(CCString* string) {
    if (!string || !string->string) {
        return 0L;  // Default return value
    }
    return strtol(string->string, NULL, 10);  // Use base 10
}

// Function to convert a CCString to a double
double stringDoubleValue(CCString* string) {
    if (!string || !string->string) {
        return 0.0;  // Default return value
    }
    return strtod(string->string, NULL);  // Direct conversion to double
}

// Function to copy a CCString
// Function to copy a CCString
CCString* copyCCString(const CCString* original) {
    if (!original) return NULL;  // Handle NULL input for safety

    // Allocate memory for the new CCString
    CCString* copy = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    if (!copy) return NULL;  // Check allocation

    // Copy the type and length
    copy->type = original->type;
    copy->length = original->length;

    // We MUST allocate heap memory even for empty strings to prevent tlsf_free ROM crashes!
    if (original->string) {
        // Allocate memory for the string copy
        copy->string = (char*)cc_safe_alloc(1, original->length + 1);  // +1 for null terminator
        if (!copy->string) {
            free(copy);  // Free the CCString structure if string allocation fails
            return NULL;
        }
        
        // Copy the string contents (Safe for both populated and empty strings)
        strncpy(copy->string, original->string, original->length);
        copy->string[original->length] = '\0';  // Ensure null termination
    } else {
        copy->string = NULL; // Safe to pass NULL to free() later
    }

    return copy;
}

void freeCCString(CCString* ccstring) {
    if (ccstring) {
        // Only free if `string` is dynamically allocated
        // This assumes that only non-empty strings were dynamically allocated
        if (ccstring->string && ccstring->string[0] != '\0') {
            free(ccstring->string);
        }
        free(ccstring);
    }
}


//Number Functions
// Function to create a CCNumber from an int
CCNumber* numberWithInt(int number) {
    CCNumber* newNumber = (CCNumber*)cc_safe_alloc(1, sizeof(CCNumber));
    newNumber->type = CCType_Number;
    if (!newNumber) {
        return NULL;
    }
    newNumber->doubleValue = (double)number;
    return newNumber;
}

// Function to create a CCNumber from a float
CCNumber* numberWithFloat(float number) {
    CCNumber* newNumber = (CCNumber*)cc_safe_alloc(1, sizeof(CCNumber));
    newNumber->type = CCType_Number;
    if (!newNumber) {
        return NULL;
    }
    newNumber->doubleValue = (double)number;
    return newNumber;
}

// Function to create a CCNumber from a long
CCNumber* numberWithLong(long number) {
    CCNumber* newNumber = (CCNumber*)cc_safe_alloc(1, sizeof(CCNumber));
    newNumber->type = CCType_Number;
    if (!newNumber) {
        return NULL;
    }
    newNumber->doubleValue = (double)number;
    return newNumber;
}

// Function to create a CCNumber from a double
CCNumber* numberWithDouble(double number) {
    CCNumber* newNumber = (CCNumber*)cc_safe_alloc(1, sizeof(CCNumber));
    newNumber->type = CCType_Number;
    if (!newNumber) {
        return NULL;
    }
    newNumber->doubleValue = number;
    return newNumber;
}

// Function to get the int value from a CCNumber
int numberIntValue(CCNumber* number) {
    if (!number) return 0;
    return (int)number->doubleValue;
}

// Function to get the float value from a CCNumber
float numberFloatValue(CCNumber* number) {
    if (!number) return 0.0f;
    return (float)number->doubleValue;
}

// Function to get the long value from a CCNumber
long numberLongValue(CCNumber* number) {
    if (!number) return 0L;
    return (long)number->doubleValue;
}

// Function to get the double value from a CCNumber
double numberDoubleValue(CCNumber* number) {
    if (!number) return 0.0;
    return number->doubleValue;
}

CCNumber* copyCCNumber(const CCNumber* original) {
    if (!original) return NULL;

    CCNumber* copy = (CCNumber*)cc_safe_alloc(1, sizeof(CCNumber));
    if (!copy) return NULL;

    copy->type = original->type;
    copy->doubleValue = original->doubleValue;

    return copy;
}

// Free function for CCNumber
void freeCCNumber(CCNumber* number) {
    if (number) {
        free(number);
    }
}

//Array Functions
CCArray* array(void) {
    // OLD BROKEN CODE:
    // CCArray* arr = (CCArray*)cc_safe_alloc(1, sizeof(CCArray));
    // arr->type = CCType_Array;
    // return arr;
    
    // NEW FIXED CODE:
    CCArray* arr = (CCArray*)cc_safe_alloc(1, sizeof(CCArray)); // Zero out memory!
    if (!arr) return NULL;
    
    arr->type = CCType_Array;
    arr->count = 0;      // Explicitly start at 0
    arr->array = NULL;    // Explicitly start with no buffer
    
    return arr;
}

// Function to create a new CCArray with the same contents as the given array
CCArray* arrayWithArray(CCArray* sourceArray) {
    if (!sourceArray || sourceArray->count < 0) {
        return NULL;
    }
    
    CCArray* newArray = (CCArray*)cc_safe_alloc(1, sizeof(CCArray));
    newArray->type = CCType_Array;
    if (!newArray) {
        return NULL;
    }

    newArray->count = sourceArray->count;

    if (newArray->count > 0) {
        newArray->array = (void**)cc_safe_alloc(1, newArray->count * sizeof(void*));
        if (!newArray->array) {
            free(newArray);
            return NULL;
        }

        // Copy elements from source array to new array
        for (int i = 0; i < newArray->count; i++) {
            newArray->array[i] = sourceArray->array[i];
        }
    } else {
        newArray->array = NULL;
    }

    return newArray;
}

// Function to create a CCArray with a list of objects terminated by NULL
CCArray* arrayWithObjects(void* firstObject, ...) {
    va_list args;
    
    // Count the number of objects
    int count = 0;
    void* currentObject = firstObject;

    va_start(args, firstObject);
    while (currentObject != NULL) {
        count++;
        currentObject = va_arg(args, void*);
    }
    va_end(args);

    // Allocate memory for CCArray
    CCArray* newArray = (CCArray*)cc_safe_alloc(1, sizeof(CCArray));
    if (!newArray) {
        return NULL;
    }

    newArray->type = CCType_Array;
    newArray->count = count;
    newArray->array = (void**)cc_safe_alloc(1, count * sizeof(void*));
    if (!newArray->array) {
        free(newArray);
        return NULL;
    }

    // Populate newArray
    va_start(args, firstObject);
    currentObject = firstObject;  // Reset to the firstObject
    for (int i = 0; i < count; i++) {
        newArray->array[i] = currentObject;
        currentObject = va_arg(args, void*);
    }
    va_end(args);

    return newArray;
}

int arrayIndexOfObject(CCArray* ccArray, void* object) {
    if (!ccArray || !object || ccArray->count == 0) return -1;

    for (int i = 0; i < ccArray->count; i++) {
        // Compare the memory addresses
        if (ccArray->array[i] == object) {
            return i;
        }
    }
    
    return -1; // Not found
}

void arrayRemoveObject(CCArray* ccArray, void* object) {
    if (!ccArray || !object || ccArray->count == 0) return;

    int index = arrayIndexOfObject(ccArray, object);
    
    if (index != -1) {
        // Calculate how many items are AFTER the one we are removing
        int numToMove = ccArray->count - 1 - index;
        
        // Shift memory down to fill the gap
        if (numToMove > 0) {
            memmove(
                &ccArray->array[index],      // Destination (current slot)
                &ccArray->array[index + 1],  // Source (next slot)
                numToMove * sizeof(void*)
            );
        }
        
        // Nullify the last slot (good hygiene, though not strictly required since we decrement count)
        ccArray->array[ccArray->count - 1] = NULL;
        
        // Decrease count
        ccArray->count--;

        // OPTIONAL: Shrink memory (Realloc down)
        // Since your AddObject reallocs every time, you might want to match that pattern,
        // but it is usually safer/faster to just leave the capacity as is.
        // If you want to be strict:
        // ccArray->array = cc_safe_realloc(ccArray->array, ccArray->count * sizeof(void*));
    }
}


// Function to add an object to the end of the array
void arrayAddObject(CCArray* array, void* object) {
    if (!array) return;

    // Allocate a new larger array to hold the existing objects plus the new one
    void** newArray = cc_safe_realloc(array->array, (array->count + 1) * sizeof(void*));
    if (!newArray) return; // Handle allocation failure

    // Assign the new array back and add the new object
    array->array = newArray;
    array->array[array->count] = object;
    array->count++;
}

// Function to add all objects from another array to the end of the array
void arrayAddObjectsFromArray(CCArray* array, CCArray* array2) {
    if (!array || !array2) return;

    // Allocate a new larger array to hold all objects
    void** newArray = cc_safe_realloc(array->array, (array->count + array2->count) * sizeof(void*));
    if (!newArray) return;

    // Copy objects from array2 to the end of the new array
    for (int i = 0; i < array2->count; i++) {
        newArray[array->count + i] = array2->array[i];
    }

    array->array = newArray;
    array->count += array2->count;
}

// Function to insert an object at a specific index in the array
void arrayInsertObjectAtIndex(CCArray* array, void* object, UInteger index) {
    if (!array || index > array->count) return;

    // Allocate a new larger array to insert the new object
    void** newArray = cc_safe_realloc(array->array, (array->count + 1) * sizeof(void*));
    if (!newArray) return;

    // Shift elements to make space for the new object
    for (UInteger i = array->count; i > index; i--) {
        newArray[i] = newArray[i - 1];
    }

    // Insert new object
    newArray[index] = object;

    array->array = newArray;
    array->count++;
}

// Function to get the object at a specific index
void* arrayObjectAtIndex(CCArray* array, UInteger index) {
    if (!array || index >= array->count) return NULL;
    return array->array[index];
}

// Function to delete the object at a specific index
void arrayDeleteObjectAtIndex(CCArray* array, UInteger index) {
    if (!array || index >= array->count) return;

    // Shift elements to fill the gap of removed object
    for (UInteger i = index; i < array->count - 1; i++) {
        array->array[i] = array->array[i + 1];
    }

    // Reallocate a smaller array
    void** newArray = cc_safe_realloc(array->array, (array->count - 1) * sizeof(void*));
    if (newArray || array->count == 1) { // Allow shrinkage but handle the case where count becomes 0
        array->array = newArray;
    }
    array->count--;
}

// Function to get the count of objects in the array
UInteger arrayCount(CCArray* array) {
    if (!array) return 0;
    return array->count;
}

CCArray* copyCCArray(const CCArray* original) {
    if (!original) return NULL;

    CCArray* copy = (CCArray*)cc_safe_alloc(1, sizeof(CCArray));
    if (!copy) return NULL;

    copy->type = original->type;
    copy->count = original->count;

    copy->array = (void**)cc_safe_alloc(1, copy->count * sizeof(void*));
    if (!copy->array) {
        free(copy);
        return NULL;
    }

    for (int i = 0; i < original->count; i++) {
        copy->array[i] = copyElement(original->array[i]);
        if (!copy->array[i]) {
            // Handle error: free allocated resources
            for (int j = 0; j < i; j++) {
                freeElement(copy->array[j]);
            }
            free(copy->array);
            free(copy);
            return NULL;
        }
    }
    return copy;
}



void freeCCArray(CCArray* array) {
    if (!array) return;

    // Free each element in the array
    if (array->array) {
        for (int i = 0; i < array->count; i++) {
            if (array->array[i]) {
                freeElement(array->array[i]);
            }
        }
        free(array->array);
    }

    // Free the CCArray struct itself
    free(array);
}

void* copyElement(void* element) {
    if (!element) return NULL;

    CCType type = *(CCType*)element;

    switch (type) {
        case CCType_String:
            return copyCCString((CCString*)element);
        case CCType_Number:
            return copyCCNumber((CCNumber*)element);
        case CCType_Data:
            return copyCCData((CCData*)element);
        case CCType_Date:
            return copyCCDate((CCDate*)element);
        case CCType_SortDescriptor:
            return copyCCSortDescriptor((CCSortDescriptor*)element);
        case CCType_Array:
            return copyCCArray((CCArray*)element);
        case CCType_Dictionary:
            return copyCCDictionary((CCDictionary*)element);
        default:
            return NULL;
    }
}

void freeElement(void* element) {
    if (!element) return;

    CCType type = *(CCType*)element;
    switch (type) {
        case CCType_String:
            freeCCString((CCString*)element);
            break;
        case CCType_Number:
            freeCCNumber((CCNumber*)element);
            break;
        case CCType_Data:
            freeCCData((CCData*)element);
            break;
        case CCType_Date:
            freeCCDate((CCDate*)element);
            break;
        case CCType_SortDescriptor:
            freeCCSortDescriptor((CCSortDescriptor*)element);
            break;
        case CCType_Array:
            freeCCArray((CCArray*)element);
            break;
        case CCType_Dictionary:
            freeCCDictionary((CCDictionary*)element);
            break;
        default:
            break;
    }
}

//Dictionary Functions
// Function to create and return an empty CCDictionary
CCDictionary* dictionary(void) {
    CCDictionary* newDict = (CCDictionary*)cc_safe_alloc(1, sizeof(CCDictionary));
    newDict->type = CCType_Dictionary;
    if (!newDict) {
        return NULL;
    }
    newDict->count = 0;
    newDict->items = NULL;
    return newDict;
}

// Function to create a new CCDictionary with the same contents as the given dictionary
CCDictionary* dictionaryWithDictionary(CCDictionary* dictionary) {
    if (!dictionary) {
        return NULL;
    }
    
    CCDictionary* newDict = (CCDictionary*)cc_safe_alloc(1, sizeof(CCDictionary));
    newDict->type = CCType_Dictionary;
    if (!newDict) {
        return NULL;
    }
    
    newDict->count = dictionary->count;
    newDict->items = (CCKeyValuePair*)cc_safe_alloc(1, newDict->count * sizeof(CCKeyValuePair));
    if (!newDict->items) {
        free(newDict);
        return NULL;
    }
    
    for (Integer i = 0; i < newDict->count; i++) {
        newDict->items[i].key = dictionary->items[i].key;
        newDict->items[i].value = dictionary->items[i].value;
    }
    
    return newDict;
}

// Function to create a CCDictionary from a list of key-value pairs
CCDictionary* dictionaryWithKeysAndObjects(CCString* key, ...) {
    va_list args;
    va_start(args, key);
    
    CCDictionary* newDict = dictionary();
    if (!newDict) {
        va_end(args);
        return NULL;
    }

    CCString* currentKey = key;
    while (currentKey != NULL) {
        void* value = va_arg(args, void*);

        // Add the key-value pair to the dictionary
        dictionarySetObjectForKey(newDict, value, currentKey);

        // Move to the next key
        currentKey = va_arg(args, CCString*);
    }
    va_end(args);

    return newDict;
}

// Function to set an object for a specific key in the dictionary
void dictionarySetObjectForKey(CCDictionary* dictionary, void* object, CCString* key) {
    if (!dictionary || !key || !key->string) return;

    // Check if the key already exists and update the value
    for (Integer i = 0; i < dictionary->count; i++) {
        if (strcmp(dictionary->items[i].key->string, key->string) == 0) {
            dictionary->items[i].value = object;
            return;
        }
    }

    // If key does not exist, add new key-value pair
    CCKeyValuePair* newItems = (CCKeyValuePair*)cc_safe_realloc(dictionary->items, (dictionary->count + 1) * sizeof(CCKeyValuePair));
    if (!newItems) return;
    dictionary->items = newItems;

    dictionary->items[dictionary->count].key = key;
    dictionary->items[dictionary->count].value = object;
    dictionary->count++;
}

// Function to get the object for a specific key in the dictionary
void* dictionaryObjectForKey(CCDictionary* dictionary, CCString* key) {
    if (!dictionary || !key) return NULL;

    for (Integer i = 0; i < dictionary->count; i++) {
        if (strcmp(dictionary->items[i].key->string, key->string) == 0) {
            return dictionary->items[i].value;
        }
    }
    return NULL; // Return NULL if key is not found
}

// Function to get the object for a specific key, then free the key used for lookup
void* dictionaryObjectForKeyFreeKey(CCDictionary* dictionary, CCString* key) {
    // If key is NULL, there's nothing to find and nothing to free
    if (!key) return NULL;

    void* result = NULL;

    // Only search if the dictionary is valid
    if (dictionary) {
        for (Integer i = 0; i < dictionary->count; i++) {
            // Safety check: ensure the dictionary item's key and string exist
            if (dictionary->items[i].key && dictionary->items[i].key->string) {
                if (strcmp(dictionary->items[i].key->string, key->string) == 0) {
                    result = dictionary->items[i].value;
                    break; // Found it, stop searching
                }
            }
        }
    }

    // Safely free the temporary lookup key before we exit the function
    freeCCString(key);

    return result;
}

// Function to get all keys in the dictionary
CCArray* dictionaryAllKeys(CCDictionary* dictionary) {
    if (!dictionary) return NULL;
    CCArray* keysArray = (CCArray*)cc_safe_alloc(1, sizeof(CCArray));
    keysArray->type = CCType_Array;
    
    if (!keysArray) return NULL;

    keysArray->count = dictionary->count;
    keysArray->array = (void**)cc_safe_alloc(1, dictionary->count * sizeof(void*));
    if (!keysArray->array) {
        free(keysArray);
        return NULL;
    }

    for (Integer i = 0; i < dictionary->count; i++) {
        keysArray->array[i] = dictionary->items[i].key;
    }

    return keysArray;
}

// Function to get all objects in the dictionary
CCArray* dictionaryAllObjects(CCDictionary* dictionary) {
    if (!dictionary) return NULL;
    CCArray* objectsArray = (CCArray*)cc_safe_alloc(1, sizeof(CCArray));
    objectsArray->type = CCType_Array;

    if (!objectsArray) return NULL;

    objectsArray->count = dictionary->count;
    objectsArray->array = (void**)cc_safe_alloc(1, dictionary->count * sizeof(void*));
    if (!objectsArray->array) {
        free(objectsArray);
        return NULL;
    }

    for (Integer i = 0; i < dictionary->count; i++) {
        objectsArray->array[i] = dictionary->items[i].value;
    }

    return objectsArray;
}

CCDictionary* copyCCDictionary(const CCDictionary* original) {
    if (!original) return NULL;

    CCDictionary* copy = (CCDictionary*)cc_safe_alloc(1, sizeof(CCDictionary));
    if (!copy) return NULL;

    copy->type = original->type;
    copy->count = original->count;

    copy->items = (CCKeyValuePair*)cc_safe_alloc(1, copy->count * sizeof(CCKeyValuePair));
    if (!copy->items) {
        free(copy);
        return NULL;
    }

    for (int i = 0; i < original->count; i++) {
        copy->items[i].key = copyCCString(original->items[i].key);
        copy->items[i].value = copyElement(original->items[i].value);
        
        if (!copy->items[i].key || !copy->items[i].value) {
            // Handle error: free allocated resources
            for (int j = 0; j < i; j++) {
                freeCCString(copy->items[j].key);
                freeElement(copy->items[j].value);
            }
            free(copy->items);
            free(copy);
            return NULL;
        }
    }
    return copy;
}

void freeCCDictionary(CCDictionary* dict) {
    if (!dict) return;

    // Free each key-value pair
    if (dict->items) {
        for (int i = 0; i < dict->count; i++) {
            if (dict->items[i].key) {
                freeCCString(dict->items[i].key);
            }
            if (dict->items[i].value) {
                freeElement(dict->items[i].value);
            }
        }
        free(dict->items);
    }

    // Free the CCDictionary struct itself
    free(dict);
}

//Sort Descriptor Functions
CCSortDescriptor* sortDescriptor(void) {
    CCSortDescriptor* newSortDescriptor = (CCSortDescriptor*)cc_safe_alloc(1, sizeof(CCSortDescriptor));
    if (!newSortDescriptor) return NULL;
    newSortDescriptor->type = CCType_SortDescriptor;
    return newSortDescriptor;
}

CCSortDescriptor* sortDescriptorWithKey(CCString* key, bool ascending) {
    CCSortDescriptor* newSortDescriptor = (CCSortDescriptor*)cc_safe_alloc(1, sizeof(CCSortDescriptor));
    if (!newSortDescriptor) return NULL;
    newSortDescriptor->type = CCType_SortDescriptor;
    newSortDescriptor->key = key;
    newSortDescriptor->ascending = ascending;
    return newSortDescriptor;
}

// Quick Sort function for CCArray
void quickSort(CCArray* array, int low, int high, CCSortDescriptor* sortDescriptor) {
    if (low < high) {
        // Partition the array
        int pi = partition(array, low, high, sortDescriptor);

        // Recursively sort the two halves
        quickSort(array, low, pi - 1, sortDescriptor);
        quickSort(array, pi + 1, high, sortDescriptor);
    }
}

int partition(CCArray* array, int low, int high, CCSortDescriptor* sortDescriptor) {
    void* pivot = array->array[high];
    CCDictionary* pivotDict = (CCDictionary*) pivot;
    void* pivotValue = dictionaryObjectForKey(pivotDict, sortDescriptor->key);

    int i = low - 1;
    for (int j = low; j < high; j++) {
        CCDictionary* currentDict = (CCDictionary*) array->array[j];
        void* currentValue = dictionaryObjectForKey(currentDict, sortDescriptor->key);

        if (compareDictionaryValues(currentValue, pivotValue, sortDescriptor->ascending) < 0) {
            i++;
            swap(array->array, i, j);
        }
    }
    swap(array->array, i + 1, high);
    return i + 1;
}

int compareDictionaryValues(void* value1, void* value2, bool ascending) {
    // Compare as strings for demonstration purposes
    CCType type = ((CCNull*)value1)->type;
    if (type == CCType_String) {
        const char* str1 = cStringOfString(value1);
        const char* str2 = cStringOfString(value2);
        
        int cmpResult = strcmp(str1, str2);
        return ascending ? cmpResult : -cmpResult;
    }
    else if (type == CCType_Number) {
        double num1 = numberDoubleValue(value1);
        double num2 = numberDoubleValue(value2);
        
        return compareNumberValues(num1, num2, ascending);
    }
    else if (type == CCType_Date) {
        double num1 = ((CCDate*)value1)->timeValue;
        double num2 = ((CCDate*)value2)->timeValue;
        
        return compareNumberValues(num1, num2, ascending);
    }
    else if (type == CCType_Data) {
        double num1 = (double)(((CCData*)value1)->length);
        double num2 = (double)(((CCData*)value2)->length);
        
        return compareNumberValues(num1, num2, ascending);
    }
    return 0;
}

int compareNumberValues(double num1, double num2, bool ascending) {
    int comparisonResult;
    if (num1 < num2) {
        comparisonResult = -1;
    } else if (num1 > num2) {
        comparisonResult = 1;
    } else {
        comparisonResult = 0;
    }
    return ascending ? comparisonResult : -comparisonResult;
}

// Utility function to swap two elements in an array
void swap(void** array, int i, int j) {
    void* temp = array[i];
    array[i] = array[j];
    array[j] = temp;
}

CCArray* sortedArrayUsingSortDescriptor(CCArray* array, CCSortDescriptor *sortDescriptor) {
    if (!array || !sortDescriptor || array->count <= 1) return array;

    // Create a copy of the array for the sorted result
    CCArray* sortedArray = arrayWithArray(array);

    // Perform Quick Sort
    quickSort(sortedArray, 0, sortedArray->count - 1, sortDescriptor);

    return sortedArray;
}

CCSortDescriptor* copyCCSortDescriptor(const CCSortDescriptor* original) {
    if (!original) return NULL;  // Handle NULL input safely

    // Allocate memory for the new CCSortDescriptor
    CCSortDescriptor* copy = (CCSortDescriptor*)cc_safe_alloc(1, sizeof(CCSortDescriptor));
    if (!copy) return NULL;  // Check allocation success

    // Copy simple fields
    copy->type = original->type;
    copy->ascending = original->ascending;

    // Deep copy the key (assuming CCString has a proper copy mechanism)
    copy->key = copyCCString(original->key);
    if (!copy->key) {
        free(copy);  // Clean up if copying key fails
        return NULL;
    }

    return copy;
}

void freeCCSortDescriptor(CCSortDescriptor* descriptor) {
    if (!descriptor) return;  // Handle null input safely

    // Free the key if it exists
    if (descriptor->key) {
        freeCCString(descriptor->key);
    }

    // Free the descriptor struct itself
    free(descriptor);
}

//Regular Expression Functions
CCRegularExpression* regularExpression(void) {
    CCRegularExpression* newRegularExpression = (CCRegularExpression*)cc_safe_alloc(1, sizeof(CCRegularExpression));
    if (!newRegularExpression) return NULL;
    newRegularExpression->type = CCType_RegularExpression;
    return newRegularExpression;
}

CCRegularExpression* regularExpressionWithPattern(CCString* pattern, CCRegularExpressionOptions options) {
    CCRegularExpression* newRegularExpression = (CCRegularExpression*)cc_safe_alloc(1, sizeof(CCRegularExpression));
    if (!newRegularExpression) return NULL;
    newRegularExpression->type = CCType_RegularExpression;
    newRegularExpression->pattern = pattern;
    newRegularExpression->options = options;
    return newRegularExpression;
}

int convertOptionsToPOSIXFlags(CCRegularExpressionOptions options) {
    int flags = REG_EXTENDED; // Use extended regex syntax

    if (options & CCRegularExpressionCaseInsensitive) {
        flags |= REG_ICASE;
    }
    // Other options like multiline matching may need additional handling outside of POSIX

    return flags;
}

CCArray* matchesInString(CCRegularExpression* regex, CCString* string, CCRange* range) {
    regex_t preg;
    regmatch_t pmatch[1];
    int posixFlags = convertOptionsToPOSIXFlags(regex->options);
    const char* pattern = cStringOfString(regex->pattern);
    const char* targetString = cStringOfString(string) + range->loc;

    if (regcomp(&preg, pattern, posixFlags) != 0) {
        perror("Regex compilation failed");
        return NULL;
    }

    CCArray* resultsArray = array(); // Initializes a CCArray
    unsigned long start = range->loc;
    long long matchLength = 0;

    while (regexec(&preg, targetString, 1, pmatch, 0) == 0) {
        matchLength = pmatch[0].rm_eo - pmatch[0].rm_so;
        CCRange* matchRange = ccRange(start + pmatch[0].rm_so, matchLength);
        arrayAddObject(resultsArray, matchRange);

        // Move past this match for the next search iteration
        targetString += pmatch[0].rm_eo;
        start += pmatch[0].rm_eo;
    }

    regfree(&preg);
    return resultsArray;
}

//Data Functions
// Function to create and return an empty CCData
CCData* data(void) {
    CCData* newData = (CCData*)cc_safe_alloc(1, sizeof(CCData));
    newData->type = CCType_Data;
    if (!newData) {
        return NULL;
    }
    newData->bytes = NULL;
    newData->length = 0;
    return newData;
}

// Function to create a new CCData by copying an existing CCData
CCData* dataWithData(CCData* data) {
    if (!data) {
        return NULL;
    }

    CCData* newData = (CCData*)cc_safe_alloc(1, sizeof(CCData));
    newData->type = CCType_Data;
    if (!newData) {
        return NULL;
    }

    newData->length = data->length;
    if (newData->length > 0) {
        newData->bytes = cc_safe_alloc(1, newData->length);
        if (!newData->bytes) {
            free(newData);
            return NULL;
        }
        memcpy(newData->bytes, data->bytes, newData->length);
    } else {
        newData->bytes = NULL;
    }

    return newData;
}

// Function to create a CCData with a given bytes buffer
CCData* dataWithBytes(void* bytes, UInteger length) {
    if (!bytes || length == 0) {
        return data();  // Create an empty data object
    }

    CCData* newData = (CCData*)cc_safe_alloc(1, sizeof(CCData));
    newData->type = CCType_Data;
    if (!newData) {
        return NULL;
    }

    newData->length = length;
    newData->bytes = cc_safe_alloc(1, length);
    if (!newData->bytes) {
        free(newData);
        return NULL;
    }
    memcpy(newData->bytes, bytes, length);

    return newData;
}

// Function to check if two CCData objects are equal
bool dataIsEqualToData(CCData* data, CCData* data1) {
    if (!data || !data1 || data->length != data1->length) {
        return false;
    }
    
    if (data->length == 0) { // Both are empty and equal
        return true;
    }

    return memcmp(data->bytes, data1->bytes, data->length) == 0;
}

// Function to read the contents of a file into a CCData structure
CCData* dataWithContentsOfFile(CCString* filePath) {
    if (!filePath || !filePath->string) {
        return NULL; // Handle invalid input
    }

    FILE *file = fopen(filePath->string, "rb");
    if (!file) {
        perror(cStringOfString(stringWithFormat("Failed to open file: %s", cStringOfString(filePath))));
        return NULL;
    }

    // Determine the size of the file
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);

    if (fileSize < 0) {
        fclose(file);
        return NULL;
    }

    // Allocate memory for CCData
    CCData* data = (CCData*)cc_safe_alloc(1, sizeof(CCData));
    data->type = CCType_Data;
    if (!data) {
        fclose(file);
        return NULL;
    }

    data->bytes = cc_safe_alloc(1, fileSize);
    if (!data->bytes) {
        free(data);
        fclose(file);
        return NULL;
    }

    // Read file contents into the buffer
    if (fread(data->bytes, 1, fileSize, file) != (size_t)fileSize) {
        free(data->bytes);
        free(data);
        fclose(file);
        return NULL;
    }

    data->length = (UInteger)fileSize;

    fclose(file);
    return data;
}

int writeDataToFile(const char* path, void* data, size_t dataSize) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        perror("Failed to open file");
        return -1;
    }

    size_t written = fwrite(data, 1, dataSize, file);
    if (written != dataSize) {
        perror("Failed to write entire data");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

bool dataWriteToFile(CCData* data, CCString* path) {
    int write = writeDataToFile(cStringOfString(path), data->bytes, data->length);
    return (write == -1) ? false : true;
}

CCData* copyCCData(const CCData* original) {
    if (!original) return NULL;

    CCData* copy = (CCData*)cc_safe_alloc(1, sizeof(CCData));
    if (!copy) return NULL;

    copy->type = original->type;
    copy->length = original->length;

    if (original->bytes && original->length > 0) {
        copy->bytes = cc_safe_alloc(1, original->length);
        if (!copy->bytes) {
            free(copy);  // Free the struct if byte allocation fails
            return NULL;
        }
        // Copy the byte data
        memcpy(copy->bytes, original->bytes, original->length);
    } else {
        copy->bytes = NULL;
    }

    return copy;
}

// Free function for CCData
void freeCCData(CCData* data) {
    if (data) {
        free(data->bytes);
        free(data);
    }
}


//Null Functions
CCNull* null() {
    CCNull* newNull = (CCNull*)cc_safe_alloc(1, sizeof(CCNull));
    newNull->type = CCType_Null;
    if (!newNull) return NULL;
    return newNull;
}

void* cc_safe_realloc(void* ptr, size_t new_size) {
    // 1. Attempt to realloc while keeping SPIRAM preference
    void* new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    // 2. Fallback: If SPIRAM fails (or system doesn't support caps_realloc well),
    // try standard realloc as a last resort.
    if (!new_ptr && new_size > 0) {
        new_ptr = realloc(ptr, new_size);
    }
    
    return new_ptr;
}

// Add this helper at the top of ObjectiveCC.c
void* cc_safe_alloc(size_t count, size_t size) {
    // Force allocation in SPIRAM (PSRAM)
    // MALLOC_CAP_SPIRAM: Use external RAM
    // MALLOC_CAP_8BIT: Byte-addressable (required for structs)
    void* ptr = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    // Fallback: If SPIRAM fails, try Internal (though unlikely with 8MB)
    if (!ptr) {
        ptr = calloc(count, size);
    }
    return ptr;
}

// NOW: Find-and-Replace in ObjectiveCC.c:
// Replace 'calloc(1, sizeof(CCView))' with 'cc_safe_alloc(1, sizeof(CCView))'
// Replace 'calloc(1, sizeof(CCLabel))' with 'cc_safe_alloc(1, sizeof(CCLabel))'
// Replace 'cc_safe_alloc(1, size)' for strings with 'heap_caps_cc_safe_alloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)'

//JSON Functions
CCJSONObject* jsonObject(void) {
    CCJSONObject* newJsonObject = (CCJSONObject*)cc_safe_alloc(1, sizeof(CCJSONObject));
    if (!newJsonObject) return NULL;
    newJsonObject->type = CCType_JSONObject;
    return newJsonObject;
}

CCJSONObject* jsonObjectWithJSONString(CCString* string) {
    CCJSONObject* newJsonObject = (CCJSONObject*)cc_safe_alloc(1, sizeof(CCJSONObject));
    if (!newJsonObject) return NULL;
    newJsonObject->type = CCType_JSONObject;
    newJsonObject->jsonString = string;
    return newJsonObject;
}

CCJSONObject* jsonObjectWithObject(void* object) {
    CCJSONObject* newJsonObject = (CCJSONObject*)cc_safe_alloc(1, sizeof(CCJSONObject));
    if (!newJsonObject) return NULL;
    newJsonObject->type = CCType_JSONObject;
    newJsonObject->jsonObject = object;
    return newJsonObject;
}

CCString* stringForJsonObject(cJSON* jsonElement) {
    return stringWithCString(jsonElement->valuestring);
}

CCNumber* numberForJsonObject(cJSON* jsonElement) {
    return numberWithDouble(jsonElement->valuedouble);
}

CCDictionary* dictionaryForJsonObject(void* jsonObject1) {
    cJSON* jsonObject = jsonObject1;
    CCDictionary* newDictionary = dictionary();
    cJSON *current_element = NULL;
    cJSON_ArrayForEach(current_element, jsonObject) {
        if (cJSON_IsString(current_element)) {
            dictionarySetObjectForKey(newDictionary, stringForJsonObject(current_element), stringWithCString(current_element->string));
        }
        else if (cJSON_IsNumber(current_element)) {
            dictionarySetObjectForKey(newDictionary, numberForJsonObject(current_element), stringWithCString(current_element->string));
        }
        else if (cJSON_IsObject(current_element)) {
            dictionarySetObjectForKey(newDictionary, dictionaryForJsonObject(current_element), stringWithCString(current_element->string));
        }
        else if (cJSON_IsArray(current_element)) {
            dictionarySetObjectForKey(newDictionary, arrayForJsonObject(current_element), stringWithCString(current_element->string));
        }
        else if (cJSON_IsNull(current_element)) {
            dictionarySetObjectForKey(newDictionary, null(), stringWithCString(current_element->string));
        }
    }
    return newDictionary;
}

CCArray* arrayForJsonObject(void* jsonObject1) {
    cJSON* jsonObject = jsonObject1;
    CCArray* newArray = array();
    cJSON *current_element = NULL;
    cJSON_ArrayForEach(current_element, jsonObject) {
        if (cJSON_IsString(current_element)) {
            arrayAddObject(newArray, stringForJsonObject(current_element));
        }
        else if (cJSON_IsNumber(current_element)) {
            arrayAddObject(newArray, numberForJsonObject(current_element));
        }
        else if (cJSON_IsObject(current_element)) {
            arrayAddObject(newArray, dictionaryForJsonObject(current_element));
        }
        else if (cJSON_IsArray(current_element)) {
            arrayAddObject(newArray, arrayForJsonObject(current_element));
        }
        else if (cJSON_IsNull(current_element)) {
            arrayAddObject(newArray, null());
        }
    }
    return newArray;
}


void generateJsonStringFromObject(CCJSONObject* object, CCJSONWriteStyle writeStyle) {
    CCType type = ((CCNull*)object->jsonObject)->type;
    cJSON* jsonObject = NULL;
    if (type == CCType_Array) {
        jsonObject = cJsonArrayForArray(object->jsonObject);
    }
    else if (type == CCType_Dictionary) {
        jsonObject = cJsonDictionaryForDictionary(object->jsonObject);
    }
    if (jsonObject != NULL) {
        if (writeStyle == CCJSONWriteStyleReadable) {
            char *prettyPrintedJson = cJSON_Print(jsonObject);
            object->jsonString = stringWithCString(prettyPrintedJson);
        }
        else if (writeStyle == CCJSONWriteStyleCompressed) {
            char *compressedJson = cJSON_PrintUnformatted(jsonObject);
            object->jsonString = stringWithCString(compressedJson);
        }
    }
}

void* cJsonArrayForArray(CCArray* array) {
    cJSON *children = cJSON_CreateArray();
    for (int i = 0; i < array->count; i++) {
        void* arrayObject = arrayObjectAtIndex(array, i);
        CCType type = ((CCNull*)arrayObject)->type;
        if (type == CCType_String) {
            CCString* string = (CCString*)arrayObject;
            cJSON_AddItemToArray(children, cJSON_CreateString(cStringOfString(string)));
        }
        else if (type == CCType_Number) {
            CCNumber* number = (CCNumber*)arrayObject;
            cJSON_AddItemToArray(children, cJSON_CreateNumber(numberDoubleValue(number)));
        }
        else if (type == CCType_Null) {
            cJSON_AddItemToArray(children, cJSON_CreateNull());
        }
        else if (type == CCType_Array) {
            CCArray* array1 = (CCArray*)arrayObject;
            cJSON_AddItemToArray(children, cJsonArrayForArray(array1));
        }
        else if (type == CCType_Dictionary) {
            CCDictionary* dictionary = (CCDictionary*)arrayObject;
            cJSON_AddItemToArray(children, cJsonDictionaryForDictionary(dictionary));
        }
    }
    return children;
}

void* cJsonDictionaryForDictionary(CCDictionary* dictionary) {
    cJSON* object = cJSON_CreateObject();
    CCArray* allKeys = dictionaryAllKeys(dictionary);
    for (int i = 0; i < allKeys->count; i++) {
        CCString* key = arrayObjectAtIndex(allKeys, i);
        void* dictObject = dictionaryObjectForKey(dictionary, key);
        CCType dictObjectType = ((CCNull*)dictObject)->type;
        if (dictObjectType == CCType_String) {
            cJSON_AddStringToObject(object, cStringOfString(key), cStringOfString(dictObject));
        }
        else if (dictObjectType == CCType_Number) {
            cJSON_AddNumberToObject(object, cStringOfString(key), numberDoubleValue(dictObject));
        }
        else if (dictObjectType == CCType_Null) {
            cJSON_AddNullToObject(object, cStringOfString(key));
        }
        else if (dictObjectType == CCType_Array) {
            cJSON_AddItemToObject(object, cStringOfString(key), cJsonArrayForArray(dictObject));
        }
        else if (dictObjectType == CCType_Dictionary) {
            cJSON_AddItemToObject(object, cStringOfString(key), cJsonDictionaryForDictionary(dictObject));
        }
    }
    return object;
}

void generateObjectFromJsonString(CCJSONObject* object) {
    CCString* string = object->jsonString;
    cJSON *root = cJSON_Parse(cStringOfString(string));
    if (root == NULL) {
        fprintf(stderr, "Error parsing JSON data\n");
        return;
    }
    if (cJSON_IsObject(root)) {
        object->jsonObject = dictionaryForJsonObject(root);
    }
    else if (cJSON_IsArray(root)) {
        object->jsonObject = arrayForJsonObject(root);
    }
}

//Time Functions

double getCurrentTimeInSecondsSinceEpoch(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Convert seconds and microseconds into a total number of seconds
    // with millisecond precision as a decimal part
    double secondsSinceEpoch = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;

    return secondsSinceEpoch;
}

void getCurrentDateTime(int *year, int *month, int *day, int *hour, int *minute, int *second, int *millisecond) {
    // Step 1: Get the current time in seconds since the Unix epoch
    time_t current_time = time(NULL);

    // Step 2: Convert to local time structure
    struct tm *local_time = localtime(&current_time);

    // Assign date and time components to respective variables
    *year = local_time->tm_year + 1900; // tm_year is years since 1900
    *month = local_time->tm_mon + 1;    // tm_mon is months since January (0-11)
    *day = local_time->tm_mday;
    *hour = local_time->tm_hour;
    *minute = local_time->tm_min;
    *second = local_time->tm_sec;

    // Step 3: Get current time with milliseconds using gettimeofday
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *millisecond = tv.tv_usec / 1000; // Convert microseconds to milliseconds
}

//Date Functions
CCDate* date(void) {
    CCDate* newDate = (CCDate*)cc_safe_alloc(1, sizeof(CCDate));
    if (!newDate) return NULL;
    newDate->type = CCType_Date;
    
    newDate->timeValue = getCurrentTimeInSecondsSinceEpoch();
    
    return newDate;
}

CCDate* dateWithTimeInterval(double timeInterval) {
    CCDate* newDate = (CCDate*)cc_safe_alloc(1, sizeof(CCDate));
    if (!newDate) return NULL;
    newDate->type = CCType_Date;
    
    newDate->timeValue = timeInterval;
    
    return newDate;
}

void dateAddTimeInterval(CCDate* date, double timeInterval) {
    date->timeValue += timeInterval;
}

bool dateEarlierThanDate(CCDate* date, CCDate* date1) {
    return (date->timeValue < date1->timeValue);
}

bool dateLaterThanDate(CCDate* date, CCDate* date1) {
    return (date->timeValue > date1->timeValue);
}

bool dateEqualToDate(CCDate* date, CCDate* date1) {
    return (date->timeValue == date1->timeValue);
}

// Function to copy a CCDate
CCDate* copyCCDate(const CCDate* original) {
    if (!original) return NULL;

    CCDate* copy = (CCDate*)cc_safe_alloc(1, sizeof(CCDate));
    if (!copy) return NULL;

    copy->type = original->type;
    copy->timeValue = original->timeValue;

    return copy;
}

void freeCCDate(CCDate* date) {
    if (date) {
        free(date);
    }
}

CCDateFormatter* dateFormatter(void) {
    // 1. Use calloc instead of malloc.
    // This ensures all pointers (like dateFormat) start as NULL
    // and all integers/enums start as 0.
    CCDateFormatter* newDateFormatter = (CCDateFormatter*)cc_safe_alloc(1, sizeof(CCDateFormatter));
    
    if (!newDateFormatter) return NULL;
    
    newDateFormatter->type = CCType_DateFormatter;
    
    // 2. Initialize required children
    newDateFormatter->calendar = calendar();
    newDateFormatter->locale = locale();
    
    // Explicitly unnecessary if using calloc, but good for documentation:
    newDateFormatter->dateFormat = NULL;
    newDateFormatter->timeZone = NULL;

    return newDateFormatter;
}

// Helper to convert struct tm to time_t treating the tm as UTC
// This is standard in BSD/GNU but might need manual implementation on some embedded systems.
// If your environment has timegm(), use that. Otherwise, this logic works.
time_t _timegm_custom(struct tm *tm) {
    time_t ret;
    char *tz;

    // Save current TZ
    tz = getenv("TZ");
    // Set TZ to UTC
    setenv("TZ", "", 1);
    tzset();
    
    // Convert
    ret = mktime(tm);
    
    // Restore TZ
    if (tz) setenv("TZ", tz, 1);
    else unsetenv("TZ");
    tzset();
    
    return ret;
}

// ------------------------------------------------------------
// Date Formatter Functions
// ------------------------------------------------------------

// Internal helper: Map Date Style to Format String
const char* _getFormatStringForDateStyle(CCDateFormatterStyle style) {
    switch (style) {
        case CCDateFormatterStyleShort:
            return "%m/%d/%y";      // e.g. 12/31/25
        case CCDateFormatterStyleMedium:
            return "%b %d, %Y";     // e.g. Dec 31, 2025
        case CCDateFormatterStyleLong:
            return "%B %d, %Y";     // e.g. December 31, 2025
        default:
            return "";              // No date component
    }
}

// Internal helper: Map Time Style to Format String
const char* _getFormatStringForTimeStyle(CCDateFormatterStyle style) {
    switch (style) {
        case CCDateFormatterStyleShort:
            return "%H:%M";         // e.g. 14:30
        case CCDateFormatterStyleMedium:
            return "%H:%M:%S";      // e.g. 14:30:59
        case CCDateFormatterStyleLong:
            return "%H:%M:%S %Z";   // e.g. 14:30:59 EST
        default:
            return "";              // No time component
    }
}

CCDate* dateFromString(CCDateFormatter* dateFormatter, CCString* string) {
    if (!dateFormatter || !string) return NULL;
    
    CCDate* newDate = (CCDate*)cc_safe_alloc(1, sizeof(CCDate));
    if (!newDate) return NULL;
    newDate->type = CCType_Date;
    
    // 1. Determine Format String
    char formatBuffer[64];
    const char* finalFormatStr;
    
    if (dateFormatter->dateFormat != NULL) {
        finalFormatStr = cStringOfString(dateFormatter->dateFormat);
    } else {
        // Generate from styles
        const char* dStr = _getFormatStringForDateStyle(dateFormatter->dateStyle);
        const char* tStr = _getFormatStringForTimeStyle(dateFormatter->timeStyle);
        
        if (strlen(dStr) > 0 && strlen(tStr) > 0) {
            snprintf(formatBuffer, sizeof(formatBuffer), "%s %s", dStr, tStr);
        } else {
            snprintf(formatBuffer, sizeof(formatBuffer), "%s%s", dStr, tStr);
        }
        finalFormatStr = formatBuffer;
    }

    // 2. Parse using strptime
    struct tm tm_struct;
    memset(&tm_struct, 0, sizeof(struct tm));
    const char* inputStr = cStringOfString(string);
    
    if (strptime(inputStr, finalFormatStr, &tm_struct) == NULL) {
        free(newDate);
        return NULL;
    }
    
    // 3. Convert to time_t (UTC)
    time_t rawTime = _timegm_custom(&tm_struct);
    
    // 4. Remove TimeZone Offset
    if (dateFormatter->timeZone) {
        rawTime -= (long)dateFormatter->timeZone->secondsFromGmt;
    }
    
    newDate->timeValue = (double)rawTime;
    return newDate;
}

CCString* stringFromDate(CCDateFormatter* dateFormatter, CCDate* date) {
    if (!dateFormatter || !date) return NULL;
    
    // 1. Determine Format String
    // If the user manually set 'dateFormat', use it.
    // Otherwise, generate it from the DateStyle and TimeStyle.
    char formatBuffer[64];
    const char* finalFormatStr;
    
    if (dateFormatter->dateFormat != NULL) {
        finalFormatStr = cStringOfString(dateFormatter->dateFormat);
    } else {
        // Generate format from Styles
        const char* dStr = _getFormatStringForDateStyle(dateFormatter->dateStyle);
        const char* tStr = _getFormatStringForTimeStyle(dateFormatter->timeStyle);
        
        // Combine them.
        // Logic: If we have both, put a space in between.
        if (strlen(dStr) > 0 && strlen(tStr) > 0) {
            snprintf(formatBuffer, sizeof(formatBuffer), "%s %s", dStr, tStr);
        } else {
            snprintf(formatBuffer, sizeof(formatBuffer), "%s%s", dStr, tStr);
        }
        finalFormatStr = formatBuffer;
    }

    // 2. Get UTC Timestamp
    time_t rawTime = (time_t)date->timeValue;
    
    // 3. Apply TimeZone Offset
    if (dateFormatter->timeZone) {
        rawTime += (long)dateFormatter->timeZone->secondsFromGmt;
    }
    
    // 4. Format
    struct tm *timeInfo = gmtime(&rawTime);
    char outputBuffer[128];
    
    if (strftime(outputBuffer, sizeof(outputBuffer), finalFormatStr, timeInfo) == 0) {
        return NULL;
    }
    
    return stringWithCString(outputBuffer);
}

// ------------------------------------------------------------
// Calendar Functions (Completed)
// ------------------------------------------------------------

CCCalendar* calendar(void) {
    CCCalendar* newCalendar = (CCCalendar*)cc_safe_alloc(1, sizeof(CCCalendar));
    if (!newCalendar) return NULL;
    newCalendar->type = CCType_Calendar;
    newCalendar->identifier = CCCalendarIdentifierGregorian; // Default
    newCalendar->locale = locale();
    newCalendar->timeZone = timeZone();
    return newCalendar;
}

CCCalendar* calendarWithIdentifier(CCCalendarIdentifier identifier) {
    CCCalendar* newCalendar = (CCCalendar*)cc_safe_alloc(1, sizeof(CCCalendar));
    if (!newCalendar) return NULL;
    newCalendar->type = CCType_Calendar;
    newCalendar->identifier = identifier;
    newCalendar->locale = locale();
    newCalendar->timeZone = timeZone();
    return newCalendar;
}

// ------------------------------------------------------------
// Component Logic (Completed)
// ------------------------------------------------------------

CCDateComponents* componentsFromDate(CCDate* date) {
    if (!date) return NULL;

    CCDateComponents* comps = (CCDateComponents*)cc_safe_alloc(1, sizeof(CCDateComponents));
    if (!comps) return NULL;
    comps->type = CCType_DateComponents;
    
    // Default dependencies
    comps->calendar = calendar();
    comps->timeZone = timeZone();
    comps->date = date; // Keep reference if needed, or copy

    // 1. Apply Timezone Offset
    time_t rawTime = (time_t)date->timeValue;
    if (comps->timeZone) {
        rawTime += (long)comps->timeZone->secondsFromGmt;
    }

    // 2. Break down into components
    struct tm *tm_val = gmtime(&rawTime);

    // 3. Map struct tm to CCDateComponents
    comps->year = tm_val->tm_year + 1900;
    comps->month = tm_val->tm_mon + 1; // tm_mon is 0-11
    comps->day = tm_val->tm_mday;
    comps->hour = tm_val->tm_hour;
    comps->minute = tm_val->tm_min;
    comps->second = tm_val->tm_sec;
    comps->weekday = tm_val->tm_wday + 1; // Typically 1-7 in high level frameworks
    comps->yearForWeekOfYear = tm_val->tm_year + 1900; // Simplified
    
    // Calculated fields not in struct tm directly
    comps->era = 1; // Simplified Gregorian AD
    comps->quarter = (comps->month - 1) / 3 + 1;
    
    return comps;
}

CCDate* dateFromComponents(CCDateComponents* components) {
    if (!components) return NULL;
    
    CCDate* newDate = (CCDate*)cc_safe_alloc(1, sizeof(CCDate));
    if (!newDate) return NULL;
    newDate->type = CCType_Date;
    
    // 1. Map components to struct tm
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(struct tm));
    
    tm_val.tm_year = components->year - 1900;
    tm_val.tm_mon = components->month - 1; // 0-11
    tm_val.tm_mday = components->day;
    tm_val.tm_hour = components->hour;
    tm_val.tm_min = components->minute;
    tm_val.tm_sec = components->second;
    tm_val.tm_isdst = -1; // Let system determine DST if possible, or 0
    
    // 2. Convert to timestamp (Treating as UTC first)
    time_t rawTime = _timegm_custom(&tm_val);
    
    // 3. Remove TimeZone Offset to get back to absolute UTC
    if (components->timeZone) {
        rawTime -= (long)components->timeZone->secondsFromGmt;
    }
    
    newDate->timeValue = (double)rawTime;
    
    return newDate;
}

Integer dateComponentValueForDate(CCDate* date, CCCalendarUnit calendarUnit) {
    return 0;
}

//Date Components Functions
void dateComponentsSetValueForComponent(CCDateComponents* dateComponents, Integer value, CCCalendarUnit component) {
    
}

Integer dateComponentsValueForComponent(CCDateComponents* dateComponents, CCCalendarUnit component) {
    return 0;
}

//Time Zone Functions
CCTimeZone* timeZone(void) {
    if (programTimeZone != NULL) {
        return programTimeZone;
    }
    programTimeZone = timeZoneWithName(programTimeZoneIdentifier);
    return programTimeZone;
}

CCTimeZone* timeZoneWithName(CCString* name) {
    CCTimeZone* newTimeZone = (CCTimeZone*)cc_safe_alloc(1, sizeof(CCTimeZone));
    if (!newTimeZone) return NULL;
    newTimeZone->type = CCType_TimeZone;
    
    newTimeZone->name = ccs("America / New York");
    newTimeZone->secondsFromGmt = -18000;
    newTimeZone->abbreviation = ccs("GMT");
    
    
    /*CCString* timeZoneFilePath = ccs("/spiflash/timeZoneData.json");
    CCJSONObject* timeZoneFile = jsonObjectWithJSONString(stringWithContentsOfFile(timeZoneFilePath));
    generateObjectFromJsonString(timeZoneFile);
    CCArray* timeZoneItems = (CCArray*)timeZoneFile->jsonObject;
    for (int i = 0; i < timeZoneItems->count; i++) {
        CCDictionary* timeZoneItem = arrayObjectAtIndex(timeZoneItems, i);
        CCString *timeZoneName = dictionaryObjectForKey(timeZoneItem, ccs("name"));
        if (stringEqualsString(name, timeZoneName)) {
            CCString* dictDescription = objectDescription(timeZoneItem);
            printf("\n\Time Zone Item Description\n\n%s\n\n", cStringOfString(dictDescription));
            newTimeZone->name = dictionaryObjectForKey(timeZoneItem, ccs("name"));
            newTimeZone->secondsFromGmt = numberDoubleValue(dictionaryObjectForKey(timeZoneItem, ccs("secondsFromGmt")));
            newTimeZone->abbreviation = dictionaryObjectForKey(timeZoneItem, ccs("abbreviation"));
        }
    }*/
    
    return newTimeZone;
}

CCArray* timeZoneNames(void) {
    CCString* timeZoneNamesPath = ccs("/spiflash/timeZoneNames.txt");
    CCString* timeZoneStrings = stringWithContentsOfFile(timeZoneNamesPath);
    CCArray* timeZoneNames = stringComponentsSeparatedByString(timeZoneStrings, ccs(","));
    return timeZoneNames;
}

CCTimeZone* systemTimeZone(void) {
    //Use C function to get the current time zone
    //return a CCTimeZone with the member values set
    //to whatever data the C functions return
    return NULL;
}

//Locale Functions
CCLocale* locale(void) {
    if (programLocale != NULL) {
        return programLocale;
    }
    programLocale = localeWithIdentifier(programLocaleIdentifier);
    return programLocale;
}

CCLocale* localeWithIdentifier(CCString* identifier) {
    CCLocale* newLocale = (CCLocale*)cc_safe_alloc(1, sizeof(CCLocale));
    if (!newLocale) return NULL;
    newLocale->type = CCType_Locale;
    
    //CCString* localeFilePath = resourceFilePath(ccs("localeData.json"));
    CCString* localeFilePath = ccs("/spiflash/localeData.json");
    printf("\nlocaleFilePath %s\n", cStringOfString(localeFilePath));
    CCJSONObject* localeFile = jsonObjectWithJSONString(stringWithContentsOfFile(localeFilePath));
    generateObjectFromJsonString(localeFile);
    
    CCArray* localeItems = (CCArray*)localeFile->jsonObject;
    printf("\nlocaleItems: %d\n", localeItems->count);
    for (int i = 0; i < localeItems->count; i++) {
        CCDictionary* localeItem = arrayObjectAtIndex(localeItems, i);
        CCString *localeIdentifier = dictionaryObjectForKey(localeItem, ccs("localeIdentifier"));
        if (stringEqualsString(identifier, localeIdentifier)) {
            CCString* dictDescription = objectDescription(localeItem);
            printf("\n\nLocale Item Description\n\n%s\n\n", cStringOfString(dictDescription));
        }
        newLocale->alternateQuotationBeginDelimiter = dictionaryObjectForKey(localeItem, ccs("alternateQuotationBeginDelimiter"));
        newLocale->alternateQuotationEndDelimiter = dictionaryObjectForKey(localeItem, ccs("alternateQuotationEndDelimiter"));
        newLocale->countryCode = dictionaryObjectForKey(localeItem, ccs("countryCode"));
        newLocale->quotationBeginDelimiter = dictionaryObjectForKey(localeItem, ccs("quotationBeginDelimiter"));
        newLocale->quotationEndDelimiter = dictionaryObjectForKey(localeItem, ccs("quotationEndDelimiter"));
        newLocale->languageCode = dictionaryObjectForKey(localeItem, ccs("languageCode"));
        newLocale->groupingSeparator = dictionaryObjectForKey(localeItem, ccs("groupingSeparator"));
        newLocale->currencySymbol = dictionaryObjectForKey(localeItem, ccs("currencySymbol"));
        newLocale->localeIdentifier = dictionaryObjectForKey(localeItem, ccs("localeIdentifier"));
        newLocale->collatorIdentifier = dictionaryObjectForKey(localeItem, ccs("collatorIdentifier"));
        newLocale->collationIdentifier = dictionaryObjectForKey(localeItem, ccs("collationIdentifier"));
        newLocale->decimalSeparator = dictionaryObjectForKey(localeItem, ccs("decimalSeparator"));
        newLocale->calendarIdentifier = dictionaryObjectForKey(localeItem, ccs("calendarIdentifier"));
        newLocale->currencyCode = dictionaryObjectForKey(localeItem, ccs("currencyCode"));
        
    }
    
    return newLocale;
}

CCArray* localeIdentifiers(void) {
    CCString* localeIdentifiersPath = ccs("/spiflash/localeIdentifiers.txt");
    CCString* localeIdentifiersStrings = stringWithContentsOfFile(localeIdentifiersPath);
    CCArray* localeIdentifiers = stringComponentsSeparatedByString(localeIdentifiersStrings, ccs(","));
    return localeIdentifiers;
}

//Log Functions
void ccLog(const char* format, ...) {
    if (format == NULL) {
        return;
    }

    va_list args;
    va_start(args, format);

    int size = vsnprintf(NULL, 0, format, args) + 1; // Determine the size needed
    va_end(args);
    
    va_start(args, format);
    char* buffer = (char*)cc_safe_alloc(1, size);

    if (!buffer) {
        va_end(args);
        return;
    }

    vsnprintf(buffer, size, format, args);
    va_end(args);

    CCString* formattedString = (CCString*)cc_safe_alloc(1, sizeof(CCString));
    formattedString->type = CCType_String;
    if (!formattedString) {
        free(buffer);
        return;
    }

    formattedString->length = size - 1; // Exclude null terminator
    formattedString->string = buffer;

    ccLogString(formattedString);
    freeCCString(formattedString);
}

void ccLogString(CCString* string) {
    
}

CCString* stringForType(CCType type) {
    CCString* typeString = NULL;
    if (type == CCType_Range) typeString = ccs("Range");
    else if (type == CCType_Point) typeString = ccs("Point");
    else if (type == CCType_Size) typeString = ccs("Size");
    else if (type == CCType_Rect) typeString = ccs("Rect");
    else if (type == CCType_StringEncoding) typeString = ccs("StringEncoding");
    else if (type == CCType_String) typeString = ccs("String");
    else if (type == CCType_Number) typeString = ccs("Number");
    else if (type == CCType_Date) typeString = ccs("Date");
    else if (type == CCType_DateFormatter) typeString = ccs("DateFormatter");
    else if (type == CCType_Calendar) typeString = ccs("Calendar");
    else if (type == CCType_DateComponents) typeString = ccs("DateComponents");
    else if (type == CCType_TimeZone) typeString = ccs("TimeZone");
    else if (type == CCType_Locale) typeString = ccs("Locale");
    else if (type == CCType_Data) typeString = ccs("Data");
    else if (type == CCType_Archiver) typeString = ccs("Archiver");
    else if (type == CCType_Array) typeString = ccs("Array");
    else if (type == CCType_KeyValuePair) typeString = ccs("KeyValuePair");
    else if (type == CCType_Dictionary) typeString = ccs("Dictionary");
    else if (type == CCType_SortDescriptor) typeString = ccs("SortDescriptor");
    else if (type == CCType_RegularExpression) typeString = ccs("RegularExpression");
    else if (type == CCType_JSONObject) typeString = ccs("JSONObject");
    else if (type == CCType_Null) typeString = ccs("Null");
    else if (type == CCType_Thread) typeString = ccs("Thread");
    else if (type == CCType_URLResponse) typeString = ccs("URLResponse");
    else if (type == CCType_URLRequest) typeString = ccs("URLRequest");
    else if (type == CCType_SerialPort) typeString = ccs("SerialPort");
    else if (type == CCType_Color) typeString = ccs("Color");
    else if (type == CCType_Layer) typeString = ccs("Layer");
    else if (type == CCType_View) typeString = ccs("View");
    else if (type == CCType_Font) typeString = ccs("Font");
    else if (type == CCType_Label) typeString = ccs("Label");
    else if (type == CCType_PointPath) typeString = ccs("PointPath");
    else if (type == CCType_ShapeLayer) typeString = ccs("ShapeLayer");
    else if (type == CCType_Image) typeString = ccs("Image");
    else if (type == CCType_ImageView) typeString = ccs("ImageView");
    else if (type == CCType_GraphicsContext) typeString = ccs("GraphicsContext");
    else if (type == CCType_Transform) typeString = ccs("Transform");
    else if (type == CCType_Transform3D) typeString = ccs("Transform3D");
    else if (type == CCType_GestureRecognizer) typeString = ccs("GestureRecognizer");
    else typeString = ccs("Unknown Type");
    return typeString;
}

CCString* objectDescription(void* object) {
    CCString* descriptionString = string();
    CCType type = ((CCNull*)object)->type;
    if (type == CCType_String) {
        CCString* string = ((CCString*)object);
        descriptionString = string;
    }
    else if (type == CCType_Array) {
        CCArray* array = ((CCArray*)object);
        CCString* arrayString = string();
        for (int i = 0; i < array->count; i++) {
            void* arrayObject = arrayObjectAtIndex(array, i);
            CCString* arrayObjectDescription = objectDescription(arrayObject);
            arrayString = stringByAppendingFormat(arrayString, "\n%s,\n", cStringOfString(arrayObjectDescription));
        }
        descriptionString = arrayString;
    }
    else if (type == CCType_Dictionary) {
        CCDictionary* dictionary = ((CCDictionary*)object);
        CCString* dictionaryString = string();
        CCArray* dictionaryKeys = dictionaryAllKeys(dictionary);
        for (int i = 0; i < dictionaryKeys->count; i++) {
            CCString* key = arrayObjectAtIndex(dictionaryKeys, i);
            void* dictionaryObject = dictionaryObjectForKey(dictionary, key);
            CCString* dictionaryObjectDescription = objectDescription(dictionaryObject);
            dictionaryString = stringByAppendingFormat(dictionaryString, "\n%s: %s,\n", cStringOfString(key), cStringOfString(dictionaryObjectDescription));
        }
        descriptionString = dictionaryString;
    }
    else if (type == CCType_Number) {
        CCNumber* number = ((CCNumber*)object);
        CCString* numberString = stringWithFormat("%f", number->doubleValue);
        descriptionString = numberString;
    }
    else if (type == CCType_Null) {
        CCString* nullString = ccs("CCNull");
        descriptionString = nullString;
    }
    else if (type == CCType_Rect) {
        CCRect* rect = ((CCRect*)object);
        CCString* rectString = stringWithFormat("X: %f, y: %f, width: %f, height: %f", rect->origin->x, rect->origin->y, rect->size->width, rect->size->height);
        descriptionString = rectString;
    }
    else if (type == CCType_Size) {
        CCSize* size = ((CCSize*)object);
        CCString* sizeString = stringWithFormat("width: %f, height: %f", size->width, size->height);
        descriptionString = sizeString;
    }
    else if (type == CCType_Point) {
        CCPoint* point = ((CCPoint*)object);
        CCString* pointString = stringWithFormat("X: %f, y: %f", point->x, point->y);
        descriptionString = pointString;
    }
    else if (type == CCType_Data) {
        CCData* data = ((CCData*)object);
        CCString* dataString = stringWithFormat("Data: %ld Bytes", data->length);
        descriptionString = dataString;
    }
    else if (type == CCType_Color) {
        CCColor* color = ((CCColor*)object);
        CCString* colorString = stringWithFormat("R: %f, G: %f, B: %f, A: %f", color->r, color->g, color->b, color->a);
        descriptionString = colorString;
    }
    else if (type == CCType_Date) {
        // 2. Setup Date Formatter
        CCDateFormatter* fmt = dateFormatter();
        fmt->dateStyle = CCDateFormatterStyleShort; // e.g., 12/31/25
        fmt->timeStyle = CCDateFormatterStyleShort; // e.g., 14:30
        CCDate* date = ((CCDate*)object);
        CCString* dateString = stringFromDate(fmt, date);
        descriptionString = dateString;
    }
    return descriptionString;
}
