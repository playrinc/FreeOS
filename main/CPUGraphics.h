//
//  CPUGraphics.h
//  
//
//  Created by Chris Galzerano on 2/15/25.
//

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_dsp.h"

// Define structures

// Represents a color with RGBA components
typedef struct {
    uint8_t r, g, b, a;
} ColorRGBA;

typedef struct {
    uint8_t r, g, b;
} Color;

typedef enum {
    TEXT_ALIGN_LEFT = 0,    // Default (starts at X)
    TEXT_ALIGN_CENTER,      // Centers text within clipWidth
    TEXT_ALIGN_RIGHT        // Aligns text to the right edge of clipWidth
} TextAlign;

// --- Correct Text Wrap Mode Enum (CPUGraphics.h) ---
typedef enum {
    TEXT_WRAP_MODE_TRUNCATE = 0, // Draw char-by-char, truncate if necessary
    TEXT_WRAP_MODE_WHOLE_WORD    // Move whole word to next line if it doesn't fit
} TextWrapMode;

// --- New Text Formatting Structure ---
// --- Correct Text Formatting Structure (CPUGraphics.h) ---
// --- New Vertical Alignment Enum ---
typedef enum {
    TEXT_VALIGN_TOP = 0,
    TEXT_VALIGN_CENTER,     // We will make this the default logic
    TEXT_VALIGN_BOTTOM
} TextVerticalAlign;

// --- Updated TextFormat Struct ---
typedef struct {
    TextAlign alignment;      // Horizontal
    TextVerticalAlign valignment; // NEW: Vertical
    TextWrapMode wrapMode;
    int lineSpacing;
    int glyphSpacing;
} TextFormat;

// Represents a 2D framebuffer
/*typedef struct {
    int displayWidth;
    int displayHeight;
    ColorRGBA *pixelData;
} Framebuffer;*/

typedef struct {
    int displayWidth;
    int displayHeight;
    void *pixelData;  // Generic pointer for different framebuffer modes
    int colorMode;    // Determines the color mode of the framebuffer
    ColorRGBA colors[2]; // Used for two-color mode
} Framebuffer;

typedef enum {
    COLOR_MODE_TWO = 0,
    COLOR_MODE_SIXTEEN,
    COLOR_MODE_256,
    COLOR_MODE_RGBA,
    COLOR_MODE_BGR888
} ColorMode;

// Represents a 3D vector or a point
typedef struct {
    float x, y, z;
} Vector3;

// Represents a 3x3 transformation matrix
typedef struct {
    float m[3][3];
} Matrix3x3;

// Represents a color stop for gradients
typedef struct {
    ColorRGBA color;
    float position; // Position along gradient (0.0 to 1.0)
} ColorStop;

typedef enum {
    GRADIENT_TYPE_LINEAR = 0,
    GRADIENT_TYPE_RADIAL,
    GRADIENT_TYPE_BOX
} GradientType;

// Represents a gradient
typedef struct {
    ColorStop *stops;
    int numStops;
    float angle;
    GradientType type;
} Gradient;

// Represents an image with RGBA data
typedef struct {
    int width;
    int height;
    ColorRGBA *data; // Pointer to pixel data
} ImageTexture;

// Function prototypes

// Basic framebuffer operations
void clearFramebuffer(Framebuffer *framebuffer, ColorRGBA clearColor);
void exportFramebufferToPNG(Framebuffer *framebuffer, const char *filePath);
void drawFramebuffer(Framebuffer* destFb, Framebuffer* srcFb, int destX, int destY);

// Rectangle drawing functions
void drawRectangleCFramebuffer(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, bool fill);
void drawRectangleWithTransform(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, bool fill, const Matrix3x3 *transform);
void drawRectangleRotate3DY(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, float angle);

// Rounded rectangle drawing functions
void drawRoundedRectangle(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, int radius, bool fill);
void drawRoundedRectangle_AntiAliasing(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, int radius, bool fill);
void drawRoundedRectangle_AntiAliasingExtended(Framebuffer *framebuffer, int x, int y, int width, int height, int clipX, int clipY, int clipW, int clipH, ColorRGBA color, int radius, bool fill);
void drawRoundedRectangleWithTransform(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, int radius, bool fill, const Matrix3x3 *transform);
void drawRoundedRectangleRotate3DY(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, int radius, bool fill, float angle);

void drawRoundedRectangle_AntiAliasingOptimized(Framebuffer *fb, int x, int y, int width, int height,
                                                int clipX, int clipY, int clipW, int clipH,
                                                ColorRGBA color, int radius, bool fill);

// Polygon drawing functions
void drawPolygon_C(Framebuffer *framebuffer, int *xPoints, int *yPoints, int numPoints, ColorRGBA color, bool fill);
void drawPolygon_C_AntiAliasing(Framebuffer *framebuffer, int *xPoints, int *yPoints, int numPoints, ColorRGBA color, bool fill, bool smooth);
void drawPolygonWithTransform(Framebuffer *framebuffer, Vector3 *vertices, int numVertices, ColorRGBA color, bool fill, const Matrix3x3 *transform);
void drawPolygonRotate3DY(Framebuffer *framebuffer, Vector3 *vertices, int numVertices, ColorRGBA color, bool fill, float angle);

// Gradient filling functions
void fillRectangleWithGradient(Framebuffer *framebuffer, int x, int y, int width, int height, const Gradient *gradient, const Matrix3x3 *transform);
void fillRectangleWithGradientExtended(Framebuffer *framebuffer, int x, int y, int width, int height, int clipX, int clipY, int clipW, int clipH, const Gradient *gradient, const Matrix3x3 *transform);
void fillPolygonWithGradient(Framebuffer *framebuffer, Vector3 *vertices, int numVertices, const Gradient *gradient, const Matrix3x3 *transform, bool antiAlias);
void fillRoundedRectangleWithGradient(Framebuffer *framebuffer, int x, int y, int width, int height, const Gradient *gradient, int radius, const Matrix3x3 *transform, bool antiAlias);
void fillRoundedRectangleWithGradientExtended(Framebuffer *framebuffer, int x, int y, int width, int height, int clipX, int clipY, int clipW, int clipH, const Gradient *gradient, int radius, const Matrix3x3 *transform, bool antiAlias);

static inline void fast_blend_pixel(Framebuffer *fb, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void fillRectangleWithGradientOptimized(Framebuffer *framebuffer, int x, int y, int width, int height,
                               int clipX, int clipY, int clipW, int clipH,
                                        const Gradient *gradient);
void fillRectangleWithGradientOptimizedWithTransform(Framebuffer *framebuffer, int x, int y, int width, int height,
                                          int clipX, int clipY, int clipW, int clipH,
                                                     const Gradient *gradient, const Matrix3x3 *transform);


// Transformation utility functions
Matrix3x3 IdentityMatrix(void);
Matrix3x3 TranslationMatrix(float tx, float ty);
Matrix3x3 RotationMatrix(float angle);
void RotationYMatrix(float angle, float *cosAngle, float *sinAngle);
Matrix3x3 ScalingMatrix(float sx, float sy);
Matrix3x3 MultiplyMatrix3x3(const Matrix3x3 *a, const Matrix3x3 *b);
void TransformPoint(float *x, float *y, const Matrix3x3 *mat);
Vector3 RotateY(const Vector3 *point, const Matrix3x3 *rotMatrix);

// Text rendering functions using FreeType
void renderText(Framebuffer *framebuffer, FT_Face face, const char *text, int startX, int startY, ColorRGBA textColor, int fontSize, const Gradient *gradient);
void renderTextBox(Framebuffer *framebuffer, FT_Face face, const char *text, int x, int y, int clipWidth, int clipHeight, ColorRGBA textColor, int fontSize, const TextFormat *format);
void renderTextBoxExtended(Framebuffer *framebuffer, FT_Face face, const char *text, int x, int y, int clipWidth, int clipHeight, int hardClipX, int hardClipY, int hardClipW, int hardClipH, int scrollY, ColorRGBA textColor, int fontSize, const TextFormat *format);
void renderTextBoxExtendedCached(Framebuffer *framebuffer,
                                 FTC_Manager manager,
                                 FTC_ImageCache image_cache,
                                 FTC_CMapCache cmap_cache,
                                 FTC_FaceID face_id, // Usually (FTC_FaceID)"/spiflash/font.ttf"
                                 const char *text,
                                 int x, int y,
                                 int clipWidth, int clipHeight,
                                 int hardClipX, int hardClipY, int hardClipW, int hardClipH,
                                 int scrollY,
                                 ColorRGBA textColor,
                                 int fontSize,
                                 const TextFormat *format);
void renderTextBoxScroll(Framebuffer *framebuffer, FT_Face face, const char *text, int x, int y, int clipWidth, int clipHeight, int scrollY, ColorRGBA textColor, int fontSize, const TextFormat *format);
void renderTextWithTransform(Framebuffer *framebuffer, FT_Face face, const char *text, int startX, int startY, ColorRGBA textColor, int fontSize, const Matrix3x3 *transform, const Gradient *gradient);
int measureTextHeight(FT_Face face, const char *text, int clipWidth, int fontSize, const TextFormat *format);
int measureTextHeightCached(FTC_Manager manager,
                            FTC_ImageCache image_cache,
                            FTC_CMapCache cmap_cache,
                            FTC_FaceID face_id,
                            const char *text,
                            int clipWidth,
                            int fontSize,
                            const TextFormat *format);
// Glyph drawing functions
void drawGlyph(Framebuffer *framebuffer, FT_Bitmap *bitmap, int x, int y, ColorRGBA textColor);

// Image texture rendering functions
void drawImageTexture(Framebuffer *framebuffer, const ImageTexture *texture, int x, int y, int width, int height);
void drawImageTextureWithAlpha(Framebuffer *fb, const ImageTexture *texture,
                               int x, int y, int width, int height, float alpha);
void drawImageTextureWithAlphaClip(Framebuffer *fb, const ImageTexture *texture,
                               int x, int y, int width, int height, float alpha,
                               int clipX, int clipY, int clipW, int clipH);
void drawImageTextureWithTransform(Framebuffer *framebuffer, const ImageTexture *texture, int x, int y, int width, int height, const Matrix3x3 *transform);
void drawImageTextureRotate3DY(Framebuffer *framebuffer, const ImageTexture *texture, int x, int y, int width, int height, float angle);
ImageTexture* resizeImageBilinear(const ImageTexture* input, int newWidth, int newHeight);

void drawImageTextureOptimizedExtended(Framebuffer *fb, const ImageTexture *texture,
                               int x, int y, int width, int height,
                                       int clipX, int clipY, int clipW, int clipH);
void drawImageTextureOptimizedExtendedTransformed(Framebuffer *fb, const ImageTexture *texture,
                                 int x, int y, int width, int height,
                                 int clipX, int clipY, int clipW, int clipH,
                                                  const Matrix3x3 *transform);


void drawLineWithThickness(Framebuffer *framebuffer, int x0, int y0, int x1, int y1, ColorRGBA color, int thickness);

void drawLine(Framebuffer* fb, int x0, int y0, int x1, int y1, ColorRGBA color);
void drawCircleFilled(Framebuffer* fb, int cx, int cy, int radius, ColorRGBA color);
void drawTriangleFilled(Framebuffer* fb, int x0, int y0, int x1, int y1, int x2, int y2, ColorRGBA color);
void drawRoundedHand(Framebuffer* fb, int x0, int y0, int x1, int y1, int thickness, ColorRGBA color);

static inline void darkenPixelFast(Framebuffer* fb, int x, int y);
void drawDayNightOverlay(Framebuffer* fb, int xOffset, int yOffset, int w, int h, float time01, float seasonStrength);

// Animation Helpers
void anim_save_background(Framebuffer *fb, uint8_t* backupBuffer, int x, int y, int w, int h);
void anim_restore_background(Framebuffer *fb, uint8_t* backupBuffer, int x, int y, int w, int h);

#endif // GRAPHICS_H
