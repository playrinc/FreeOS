//
//  CPUGraphics.c
//  
//
//  Created by Chris Galzerano on 2/15/25.
//

#include "CPUGraphics.h"
//#include <png.h>
#include <math.h>

void clearFramebuffer(Framebuffer *framebuffer, ColorRGBA clearColor) {
    switch (framebuffer->colorMode) {
        case COLOR_MODE_TWO:
        {
            uint8_t clearBit = (clearColor.r == framebuffer->colors[1].r &&
                                clearColor.g == framebuffer->colors[1].g &&
                                clearColor.b == framebuffer->colors[1].b) ? 0xFF : 0x00;
            memset(framebuffer->pixelData, clearBit, framebuffer->displayWidth * framebuffer->displayHeight / 8);
            break;
        }
            
        case COLOR_MODE_SIXTEEN:
        {
            uint8_t clearNibble = (clearColor.r >> 4) & 0xF;
            uint8_t clearByte = (clearNibble << 4) | clearNibble;
            memset(framebuffer->pixelData, clearByte, framebuffer->displayWidth * framebuffer->displayHeight / 2);
            break;
        }
            
        case COLOR_MODE_256:
        {
            uint8_t clearIndex = (clearColor.r >> 6) | ((clearColor.g >> 6) << 2) | ((clearColor.b >> 6) << 4);
            memset(framebuffer->pixelData, clearIndex, framebuffer->displayWidth * framebuffer->displayHeight);
            break;
        }
            
        case COLOR_MODE_RGBA:
        {
            ColorRGBA *pixelPtr = (ColorRGBA*)framebuffer->pixelData;
            for (int i = 0; i < framebuffer->displayWidth * framebuffer->displayHeight; i++) {
                pixelPtr[i] = clearColor;
            }
            break;
        }

        // --- ADD THIS NEW CASE ---
        case COLOR_MODE_BGR888:
        {
            uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData;
            int num_pixels = framebuffer->displayWidth * framebuffer->displayHeight;
            
            // Pre-calculate the 3-byte color
            uint8_t b = clearColor.b;
            uint8_t g = clearColor.g;
            uint8_t r = clearColor.r;

            // Loop and set all pixels to the 3-byte color
            for (int i = 0; i < num_pixels; i++) {
                *pixelPtr++ = b; // Blue
                *pixelPtr++ = g; // Green
                *pixelPtr++ = r; // Red
            }
            break;
        }
        // --- END OF NEW CASE ---
            
        default:
            // Optional: Log an error for unknown mode
            break;
    }
}

void drawFramebuffer(Framebuffer* destFb, Framebuffer* srcFb, int destX, int destY) {
    if (!destFb || !srcFb || !destFb->pixelData || !srcFb->pixelData) return;

    for (int y = 0; y < srcFb->displayHeight; y++) {
        int drawY = destY + y;
        if (drawY < 0 || drawY >= destFb->displayHeight) continue; // Y Clip

        for (int x = 0; x < srcFb->displayWidth; x++) {
            int drawX = destX + x;
            if (drawX < 0 || drawX >= destFb->displayWidth) continue; // X Clip

            uint8_t* srcPixel = (uint8_t*)srcFb->pixelData + (y * srcFb->displayWidth + x) * 3;
            uint8_t* destPixel = (uint8_t*)destFb->pixelData + (drawY * destFb->displayWidth + drawX) * 3;

            // Treat pure black (0,0,0) as transparent background
            if (srcPixel[0] != 0 || srcPixel[1] != 0 || srcPixel[2] != 0) {
                destPixel[0] = srcPixel[0]; // R
                destPixel[1] = srcPixel[1]; // G
                destPixel[2] = srcPixel[2]; // B
            }
        }
    }
}

void blendPixel(ColorRGBA *dest, ColorRGBA src) {
    float alpha = src.a / 255.0f;
    dest->r = (uint8_t)(alpha * src.r + (1 - alpha) * dest->r);
    dest->g = (uint8_t)(alpha * src.g + (1 - alpha) * dest->g);
    dest->b = (uint8_t)(alpha * src.b + (1 - alpha) * dest->b);
}

void blendPixelWithAlpha(ColorRGBA *dest, ColorRGBA src, float alphaFactor) {
    float alpha = (src.a * alphaFactor) / 255.0f;
    dest->r = (uint8_t)(alpha * src.r + (1 - alpha) * dest->r);
    dest->g = (uint8_t)(alpha * src.g + (1 - alpha) * dest->g);
    dest->b = (uint8_t)(alpha * src.b + (1 - alpha) * dest->b);
}

#include <math.h>
#include <string.h>

// --- YOUR PROVIDED FUNCTIONS ---

void TransformPoint(float *x, float *y, const Matrix3x3 *transform) {
    float newX = *x * transform->m[0][0] + *y * transform->m[0][1] + transform->m[0][2];
    float newY = *x * transform->m[1][0] + *y * transform->m[1][1] + transform->m[1][2];
    *x = newX;
    *y = newY;
}

Matrix3x3 IdentityMatrix(void) {
    return (Matrix3x3) {{{1, 0, 0},
                         {0, 1, 0},
                         {0, 0, 1}}};
}

Matrix3x3 TranslationMatrix(float tx, float ty) {
    Matrix3x3 mat = IdentityMatrix();
    mat.m[0][2] = tx;
    mat.m[1][2] = ty;
    return mat;
}

Matrix3x3 RotationMatrix(float angle) {
    Matrix3x3 mat = IdentityMatrix();
    float cosA = cosf(angle);
    float sinA = sinf(angle);
    mat.m[0][0] = cosA;
    mat.m[0][1] = -sinA;
    mat.m[1][0] = sinA;
    mat.m[1][1] = cosA;
    return mat;
}

Matrix3x3 ScalingMatrix(float sx, float sy) {
    Matrix3x3 mat = IdentityMatrix();
    mat.m[0][0] = sx;
    mat.m[1][1] = sy;
    return mat;
}

Matrix3x3 MultiplyMatrix3x3(const Matrix3x3 *a, const Matrix3x3 *b) {
    Matrix3x3 result = {{{0}}};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                result.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
    return result;
}

void RotationYMatrix(float angle, float *cosAngle, float *sinAngle) {
    *cosAngle = cosf(angle);
    *sinAngle = sinf(angle);
}

// --- NEW REQUIRED FUNCTIONS FOR INVERSE MAPPING ---

float Matrix3x3Determinant(const Matrix3x3 *m) {
    // Standard 3x3 Determinant formula
    return m->m[0][0] * (m->m[1][1] * m->m[2][2] - m->m[1][2] * m->m[2][1]) -
           m->m[0][1] * (m->m[1][0] * m->m[2][2] - m->m[1][2] * m->m[2][0]) +
           m->m[0][2] * (m->m[1][0] * m->m[2][1] - m->m[1][1] * m->m[2][0]);
}

bool Matrix3x3Inverse(const Matrix3x3 *in, Matrix3x3 *out) {
    float det = Matrix3x3Determinant(in);
    if (fabs(det) < 0.0001f) return false; // Singular matrix (scale=0?), cannot invert

    float invDet = 1.0f / det;

    // Calculate Matrix of Minors / Adjugate
    out->m[0][0] = (in->m[1][1] * in->m[2][2] - in->m[1][2] * in->m[2][1]) * invDet;
    out->m[0][1] = (in->m[0][2] * in->m[2][1] - in->m[0][1] * in->m[2][2]) * invDet;
    out->m[0][2] = (in->m[0][1] * in->m[1][2] - in->m[0][2] * in->m[1][1]) * invDet;

    out->m[1][0] = (in->m[1][2] * in->m[2][0] - in->m[1][0] * in->m[2][2]) * invDet;
    out->m[1][1] = (in->m[0][0] * in->m[2][2] - in->m[0][2] * in->m[2][0]) * invDet;
    out->m[1][2] = (in->m[0][2] * in->m[1][0] - in->m[0][0] * in->m[1][2]) * invDet;

    out->m[2][0] = (in->m[1][0] * in->m[2][1] - in->m[1][1] * in->m[2][0]) * invDet;
    out->m[2][1] = (in->m[0][1] * in->m[2][0] - in->m[0][0] * in->m[2][1]) * invDet;
    out->m[2][2] = (in->m[0][0] * in->m[1][1] - in->m[0][1] * in->m[1][0]) * invDet;

    return true;
}

/*void exportFramebufferToPNG(Framebuffer *framebuffer, const char *filePath) {
    FILE *fp = fopen(filePath, "wb");
    if (!fp) {
        fprintf(stderr, "Could not open file %s for writing\n", filePath);
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        fprintf(stderr, "Failed to create png write struct\n");
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        fprintf(stderr, "Failed to create png info struct\n");
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        fprintf(stderr, "Error during png creation\n");
        return;
    }

    png_init_io(png, fp);

    png_set_IHDR(
        png,
        info,
        framebuffer->displayWidth,
        framebuffer->displayHeight,
        8,  // Bit depth
        framebuffer->colorMode == COLOR_MODE_RGBA ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    png_bytep row_pointers[framebuffer->displayHeight];

    // Process different color modes
    switch (framebuffer->colorMode) {
        case COLOR_MODE_TWO:
            for (int y = 0; y < framebuffer->displayHeight; y++) {
                row_pointers[y] = malloc(framebuffer->displayWidth * 3); // RGB format for each pixel
                uint8_t *pixelRow = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth / 8);
                for (int x = 0; x < framebuffer->displayWidth; x++) {
                    int byteIndex = x / 8;
                    int bitIndex = 7 - (x % 8);
                    int bitValue = (pixelRow[byteIndex] >> bitIndex) & 1;
                    ColorRGBA color = framebuffer->colors[bitValue];
                    row_pointers[y][x * 3 + 0] = color.r;
                    row_pointers[y][x * 3 + 1] = color.g;
                    row_pointers[y][x * 3 + 2] = color.b;
                }
            }
            break;

        case COLOR_MODE_SIXTEEN:
            for (int y = 0; y < framebuffer->displayHeight; y++) {
                row_pointers[y] = malloc(framebuffer->displayWidth * 3);
                uint8_t *pixelRow = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth / 2);
                for (int x = 0; x < framebuffer->displayWidth; x++) {
                    int byteIndex = x / 2;
                    int nibbleShift = (x % 2) ? 0 : 4;
                    int colorIndex = (pixelRow[byteIndex] >> nibbleShift) & 0xF;
                    // Map colorIndex to RGB using a predefined 16-color palette
                    ColorRGBA color = {colorIndex * 17, colorIndex * 17, colorIndex * 17};
                    row_pointers[y][x * 3 + 0] = color.r;
                    row_pointers[y][x * 3 + 1] = color.g;
                    row_pointers[y][x * 3 + 2] = color.b;
                }
            }
            break;

        case COLOR_MODE_256:
            for (int y = 0; y < framebuffer->displayHeight; y++) {
                row_pointers[y] = malloc(framebuffer->displayWidth * 3);
                uint8_t *pixelRow = ((uint8_t *)framebuffer->pixelData + y * framebuffer->displayWidth);
                for (int x = 0; x < framebuffer->displayWidth; x++) {
                    uint8_t colorIndex = pixelRow[x];
                    // Map colorIndex to RGB using a predefined 256-color palette
                    ColorRGBA color = {colorIndex, colorIndex, colorIndex}; // Simplified mapping
                    row_pointers[y][x * 3 + 0] = color.r;
                    row_pointers[y][x * 3 + 1] = color.g;
                    row_pointers[y][x * 3 + 2] = color.b;
                }
            }
            break;

        case COLOR_MODE_RGBA:
        default:
            for (int y = 0; y < framebuffer->displayHeight; y++) {
                row_pointers[y] = ((uint8_t *)framebuffer->pixelData + y * framebuffer->displayWidth * 4);
            }
            break;
    }

    png_set_rows(png, info, row_pointers);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

    for (int y = 0; y < framebuffer->displayHeight; y++) {
        if (framebuffer->colorMode != COLOR_MODE_RGBA)
            free(row_pointers[y]);
    }

    png_destroy_write_struct(&png, &info);
    fclose(fp);
}*/

/*void drawRectangleCFramebuffer(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, bool fill) {
    for (int i = 0; i < height; i++) {
        int Y = y + i;
        if (Y >= 0 && Y < framebuffer->displayHeight) {
            for (int j = 0; j < width; j++) {
                int X = x + j;
                if (X >= 0 && X < framebuffer->displayWidth) {
                    switch (framebuffer->colorMode) {
                        case COLOR_MODE_TWO:
                        {
                            uint8_t *pixelByte = ((uint8_t *)framebuffer->pixelData) + (Y * framebuffer->displayWidth + X) / 8;
                            int bitIndex = 7 - ((Y * framebuffer->displayWidth + X) % 8);
                            if (color.r == framebuffer->colors[1].r &&
                                color.g == framebuffer->colors[1].g &&
                                color.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex);
                            } else {
                                *pixelByte &= ~(1 << bitIndex);
                            }
                            break;
                        }
                        case COLOR_MODE_SIXTEEN:
                        {
                            uint8_t *pixelByte = ((uint8_t *)framebuffer->pixelData) + (Y * framebuffer->displayWidth + X) / 2;
                            int nibble = ((Y * framebuffer->displayWidth + X) % 2) ? 0 : 4;
                            *pixelByte = (*pixelByte & (~(0xF << nibble))) | ((color.r >> 4) << nibble);
                            break;
                        }
                        case COLOR_MODE_256:
                        {
                            uint8_t *pixelByte = ((uint8_t *)framebuffer->pixelData) + Y * framebuffer->displayWidth + X;
                            *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                            break;
                        }
                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[Y * framebuffer->displayWidth + X];
                            blendPixel(fbColor, color);
                            break;
                        }
                    }
                }
            }
        }
    }
}*/


void drawRectangleCFramebuffer(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, bool fill) {
    // Loop over every pixel in the rectangle's bounds
    for (int i = 0; i < height; i++) {
        int Y = y + i; // Current pixel Y
        
        // Skip this row if it's outside the framebuffer
        if (Y >= 0 && Y < framebuffer->displayHeight) {
            
            for (int j = 0; j < width; j++) {
                int X = x + j; // Current pixel X
                
                // Draw pixel if it's within the framebuffer
                if (X >= 0 && X < framebuffer->displayWidth) {
                    
                    switch (framebuffer->colorMode) {
                        case COLOR_MODE_TWO:
                        {
                            uint8_t *pixelByte = ((uint8_t *)framebuffer->pixelData) + (Y * framebuffer->displayWidth + X) / 8;
                            int bitIndex = 7 - ((Y * framebuffer->displayWidth + X) % 8);
                            if (color.r == framebuffer->colors[1].r &&
                                color.g == framebuffer->colors[1].g &&
                                color.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex);
                            } else {
                                *pixelByte &= ~(1 << bitIndex);
                            }
                            break;
                        }
                            
                        case COLOR_MODE_SIXTEEN:
                        {
                            uint8_t *pixelByte = ((uint8_t *)framebuffer->pixelData) + (Y * framebuffer->displayWidth + X) / 2;
                            int nibble = ((Y * framebuffer->displayWidth + X) % 2) ? 0 : 4;
                            *pixelByte = (*pixelByte & (~(0xF << nibble))) | ((color.r >> 4) << nibble);
                            break;
                        }
                            
                        case COLOR_MODE_256:
                        {
                            uint8_t *pixelByte = ((uint8_t *)framebuffer->pixelData) + Y * framebuffer->displayWidth + X;
                            *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                            break;
                        }

                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[Y * framebuffer->displayWidth + X];
                            blendPixel(fbColor, color); // Assumes you have blendPixel
                            break;
                        }

                        // --- NEW OPTIMIZED BGR888 MODE ---
                        case COLOR_MODE_BGR888:
                        {
                            uint32_t src_alpha = color.a;

                            if (src_alpha == 255) {
                                // Fast path: Fully opaque, just overwrite
                                uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (Y * framebuffer->displayWidth + X) * 3;
                                *(pixelPtr + 0) = color.b; // Blue
                                *(pixelPtr + 1) = color.g; // Green
                                *(pixelPtr + 2) = color.r; // Red
                            }
                            else if (src_alpha > 0) {
                                // Blend path: Semi-transparent
                                
                                // 1. Get pointer to the destination pixel (3 bytes)
                                uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (Y * framebuffer->displayWidth + X) * 3;

                                // 2. Read the destination color
                                uint8_t dest_b = *(pixelPtr + 0);
                                uint8_t dest_g = *(pixelPtr + 1);
                                uint8_t dest_r = *(pixelPtr + 2);

                                // 3. Calculate "one minus alpha"
                                uint32_t one_minus_alpha = 256 - src_alpha;

                                // 4. Perform integer blend for each channel
                                uint8_t final_r = (uint8_t)((color.r * src_alpha + dest_r * one_minus_alpha) >> 8);
                                uint8_t final_g = (uint8_t)((color.g * src_alpha + dest_g * one_minus_alpha) >> 8);
                                uint8_t final_b = (uint8_t)((color.b * src_alpha + dest_b * one_minus_alpha) >> 8);

                                // 5. Write the final color back
                                *(pixelPtr + 0) = final_b;
                                *(pixelPtr + 1) = final_g;
                                *(pixelPtr + 2) = final_r;
                            }
                            // if src_alpha == 0, do nothing
                            break;
                        }
                    } // end switch
                } // end if X
            } // end for j
        } // end if Y
    } // end for i
}

// (Located in your CPUGraphics implementation file)

static inline void write_pixel_rgb888_alpha(Framebuffer *framebuffer, int x, int y, ColorRGBA src) {
    
    uint32_t final_alpha = src.a;
    
    if (final_alpha == 255) {
        uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) * 3;
        // FAST PATH: Write BGR (which corrects our RGB input to the BGR display)
        *(pixelPtr + 0) = src.b; // B -> R position
        *(pixelPtr + 1) = src.g; // G -> G position
        *(pixelPtr + 2) = src.r; // R -> B position
    }
    else if (final_alpha > 0) {
            // Blend path: Semi-transparent
            uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) * 3;
            
            // --- THE FIX IS HERE: Read the BGR bytes into correctly labeled RGB variables ---
            
            // Read BGR bytes directly from the buffer
            uint8_t B_byte = *(pixelPtr + 0);
            uint8_t G_byte = *(pixelPtr + 1);
            uint8_t R_byte = *(pixelPtr + 2);

            // Label them correctly for the blending math:
            uint8_t dest_r = R_byte; // Destination Red channel data
            uint8_t dest_g = G_byte; // Destination Green channel data
            uint8_t dest_b = B_byte; // Destination Blue channel data
            
            // --- END FIX ---
            
            uint32_t one_minus_alpha = 256 - final_alpha;

            // Perform integer blend for each channel (R source with R dest, etc.)
            uint8_t final_r = (uint8_t)((src.r * final_alpha + dest_r * one_minus_alpha) >> 8);
            uint8_t final_g = (uint8_t)((src.g * final_alpha + dest_g * one_minus_alpha) >> 8);
            uint8_t final_b = (uint8_t)((src.b * final_alpha + dest_b * one_minus_alpha) >> 8);

            // Write BGR (The correct order for the display to interpret: B, G, R)
            *(pixelPtr + 0) = final_b; // New Blue goes to the R position (0)
            *(pixelPtr + 1) = final_g; // New Green goes to the G position (1)
            *(pixelPtr + 2) = final_r; // New Red goes to the B position (2)
        }
}

void drawRectangleWithTransform(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, bool fill, const Matrix3x3 *transform) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Calculate transformed position for the current pixel
            float transformedX = (float)(x + j);
            float transformedY = (float)(y + i);
            TransformPoint(&transformedX, &transformedY, transform);
            int pixelX = (int)transformedX;
            int pixelY = (int)transformedY;
            
            // Check bounds and apply blending if within framebuffer
            if (pixelX >= 0 && pixelX < framebuffer->displayWidth && pixelY >= 0 && pixelY < framebuffer->displayHeight) {
                switch (framebuffer->colorMode) {
                    case COLOR_MODE_TWO:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (pixelY * framebuffer->displayWidth + pixelX) / 8;
                        int bitIndex = 7 - ((pixelY * framebuffer->displayWidth + pixelX) % 8);
                        uint8_t bitMask = 1 << bitIndex;
                        if (color.r == framebuffer->colors[1].r &&
                            color.g == framebuffer->colors[1].g &&
                            color.b == framebuffer->colors[1].b) {
                                *pixelByte |= bitMask; // Set bit for color 1
                        } else {
                            *pixelByte &= ~bitMask; // Clear bit for color 0
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (pixelY * framebuffer->displayWidth + pixelX) / 2;
                        int nibbleShift = ((pixelY * framebuffer->displayWidth + pixelX) % 2) ? 0 : 4;
                        uint8_t colorIndex = (color.r >> 4) & 0xF; // Simplified mapping of color to 16-color index
                        *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + pixelY * framebuffer->displayWidth + pixelX;
                        // Simplified, assuming grayscale mapping; exact palette mapping needed
                        *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                        break;
                    }
                    case COLOR_MODE_BGR888: // Our 18-bit / RGB888 mode
                    {
                        // Use the existing ColorRGBA (which contains the required alpha)
                        write_pixel_rgb888_alpha(framebuffer, pixelX, pixelY, color);
                        break;
                    }
                    case COLOR_MODE_RGBA:
                    default:
                    {
                        ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[pixelY * framebuffer->displayWidth + pixelX];
                        blendPixel(fbColor, color);
                        break;
                    }
                }
            }
        }
    }
}

// Assuming write_pixel_rgb888_alpha is available and handles the R/B swap.

void drawRoundedRectangle_AntiAliasing(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, int radius, bool fill) {
    drawRoundedRectangle_AntiAliasingExtended(framebuffer, x, y, width, height, 0, 0, width, height, color, radius, fill);
}

void drawRoundedRectangle_AntiAliasingExtended(Framebuffer *framebuffer, int x, int y, int width, int height, int clipX, int clipY, int clipW, int clipH, ColorRGBA color, int radius, bool fill) {
    
    // Debug Log
    if (fill) { // Only log the main body, not borders
        //printf("DRAW RECT: X=%d Y=%d W=%d H=%d | ClipY=%d\n", x, y, width, height, clipY);
    }
    
    // Ensure radius does not exceed half of the smallest dimension
    int maxRadius = (width < height ? width : height) / 2;
    radius = (radius > maxRadius) ? maxRadius : radius;
    
    // Use radius + 1 for the AA curve calculation
    int adjustedRadius = radius + 1;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            
            int posX = x + j;
            int posY = y + i;
            
            // Assume the pixel is fully opaque by default
            float alphaFactor = 1.0f;
            bool is_in_corner_quadrant = false;

            // --- 1. Identify if the pixel is in a corner region ---
            if (i < adjustedRadius || i >= height - adjustedRadius) {
                if (j < adjustedRadius || j >= width - adjustedRadius) {
                    is_in_corner_quadrant = true;
                }
            }
            
            // --- 2. Calculate Alpha Factor based on Geometry ---
            if (is_in_corner_quadrant) {
                
                // Determine the center point (cx, cy) of the relevant curve
                float cx = (j < adjustedRadius) ? adjustedRadius : (float)(width - adjustedRadius);
                float cy = (i < adjustedRadius) ? adjustedRadius : (float)(height - adjustedRadius);

                // Calculate distance from the curve center to the pixel center
                float dx = j - cx + 0.5f;
                float dy = i - cy + 0.5f;

                float distance = sqrtf(dx * dx + dy * dy);
                
                if (distance > adjustedRadius) {
                    alphaFactor = 0.0f; // Outside the entire shape
                } else {
                    // Calculate blend: 1.0 at the center, 0.0 at adjustedRadius distance
                    alphaFactor = fminf(1.0f, adjustedRadius - distance);
                }
            }
            // ELSE: If not in a corner quadrant, alphaFactor remains 1.0f (fully opaque, covers straight edges)
            
            // --- 3. Final Draw Condition ---
            
            // We only proceed if the pixel has opacity AND (we are filling OR we are drawing a border)
            if (alphaFactor > 0.0f) {
                // If fill is true, we draw the whole shape (opaque center + blend).
                // If fill is false, we want to draw the blend area (alphaFactor < 1.0)
                // AND the straight edges (alphaFactor == 1.0).
                if (fill || alphaFactor < 1.0f || (alphaFactor == 1.0f && !fill)) {
                    
                    if (posX >= 0 && posX < framebuffer->displayWidth && posY >= 0 && posY < framebuffer->displayHeight) {
                        
                        // Combine the input color's alpha with the calculated geometry factor
                        uint32_t final_alpha = (uint32_t)(color.a * alphaFactor);
                        ColorRGBA srcColor = color;
                        srcColor.a = (uint8_t)final_alpha;
                        
                        if (framebuffer->colorMode == COLOR_MODE_BGR888) {
                            write_pixel_rgb888_alpha(framebuffer, posX, posY, srcColor);
                        } else {
                            // Fallback to RGBA blending for other modes
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[posY * framebuffer->displayWidth + posX];
                            blendPixel(fbColor, srcColor);
                        }
                    }
                }
            }
        }
    }
}

// Optimized Anti-Aliased Rounded Rectangle
// Speedup: Removes sqrtf() and logic checks for 90% of the pixels.
void drawRoundedRectangle_AntiAliasingOptimized(Framebuffer *fb, int x, int y, int width, int height,
                                                int clipX, int clipY, int clipW, int clipH,
                                                ColorRGBA color, int radius, bool fill) {

    // 1. Safety Clamps
    int maxRadius = (width < height ? width : height) / 2;
    if (radius > maxRadius) radius = maxRadius;
    int adjustedRadius = radius + 1; // Used for soft edge calculation

    // Pre-calculate squared radius for fast rejection
    // (Optional, but sqrt is usually fast enough for small corners now)
    
    // ---------------------------------------------------------
    // PHASE 1: DRAW SOLID INTERIORS (No Math Required)
    // ---------------------------------------------------------
    if (fill) {
        // A. Draw the "Center Body" (Full width, excluding top/bottom corners)
        // From (y + radius) to (height - radius)
        int bodyY = y + radius;
        int bodyH = height - (2 * radius);
        if (bodyH > 0) {
            // Draw a solid rectangle (Use your fast fill rect function here or a simple loop)
            // For this example, I'll allow a simple loop to demonstrate
             for (int i = 0; i < bodyH; i++) {
                int lineY = bodyY + i;
                if (lineY < clipY || lineY >= clipY + clipH) continue;
                
                // Get pointer to start of line
                int idx = (lineY * fb->displayWidth + x) * 3;
                uint8_t *p = (uint8_t *)fb->pixelData + idx;
                
                for (int j = 0; j < width; j++) {
                    int screenX = x + j;
                    if (screenX >= clipX && screenX < clipX + clipW) {
                        // Fast Write (Assume Opaque for center)
                        if (color.a == 255) {
                            p[0] = color.b; p[1] = color.g; p[2] = color.r;
                        } else {
                            fast_blend_pixel(fb, screenX, lineY, color.r, color.g, color.b, color.a);
                        }
                    }
                    p += 3;
                }
            }
        }

        // B. Draw Top and Bottom Center Strips (Between corners)
        // Top Strip: x+radius to width-radius, height=radius
        int stripW = width - (2 * radius);
        if (stripW > 0) {
            // Helper to draw horizontal line
            for (int r = 0; r < radius; r++) {
                int topY = y + r;
                int botY = y + height - 1 - r;
                
                int startX = x + radius;

                for (int k = 0; k < stripW; k++) {
                    int screenX = startX + k;
                    if (screenX < clipX || screenX >= clipX + clipW) continue;

                    // Draw Top Pixel
                    if (topY >= clipY && topY < clipY + clipH) {
                        fast_blend_pixel(fb, screenX, topY, color.r, color.g, color.b, color.a);
                    }
                    // Draw Bottom Pixel
                    if (botY >= clipY && botY < clipY + clipH) {
                        fast_blend_pixel(fb, screenX, botY, color.r, color.g, color.b, color.a);
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------
    // PHASE 2: DRAW CORNERS (The Math Part)
    // ---------------------------------------------------------
    // We only loop 0 to Radius. We calculate ONE corner and plot FOUR.
    
    // Center of the top-left curve
    float cx = (float)radius;
    float cy = (float)radius;

    for (int i = 0; i < radius; i++) {
        for (int j = 0; j < radius; j++) {
            
            // Calculate distance from center of circle
            // Note: We add 0.5 to center the pixel sample
            float dx = (float)j - cx + 0.5f;
            float dy = (float)i - cy + 0.5f;
            float dist = sqrtf(dx*dx + dy*dy);

            float alphaFactor = 0.0f;

            if (dist > (float)adjustedRadius) {
                alphaFactor = 0.0f; // Outside
            } else {
                // Anti-Aliasing math
                alphaFactor = fminf(1.0f, (float)adjustedRadius - dist);
            }

            if (alphaFactor <= 0.0f) continue;
            
            // If we are NOT filling, and this pixel is fully solid internal, skip it
            if (!fill && alphaFactor >= 1.0f) continue;

            // Compute Final Color for this pixel
            uint8_t finalA = (uint8_t)(color.a * alphaFactor);
            if (finalA == 0) continue;

            // --- PLOT 4 MIRRORED PIXELS ---
            
            // 1. Top-Left: (x + j, y + i)
            int px = x + j;
            int py = y + i;
            if (px >= clipX && px < clipX + clipW && py >= clipY && py < clipY + clipH)
                fast_blend_pixel(fb, px, py, color.r, color.g, color.b, finalA);

            // 2. Top-Right: (x + width - 1 - j, y + i)
            px = x + width - 1 - j;
            // py same
            if (px >= clipX && px < clipX + clipW && py >= clipY && py < clipY + clipH)
                fast_blend_pixel(fb, px, py, color.r, color.g, color.b, finalA);

            // 3. Bottom-Left: (x + j, y + height - 1 - i)
            px = x + j;
            py = y + height - 1 - i;
            if (px >= clipX && px < clipX + clipW && py >= clipY && py < clipY + clipH)
                fast_blend_pixel(fb, px, py, color.r, color.g, color.b, finalA);

            // 4. Bottom-Right: (x + width - 1 - j, y + height - 1 - i)
            px = x + width - 1 - j;
            // py same
            if (px >= clipX && px < clipX + clipW && py >= clipY && py < clipY + clipH)
                fast_blend_pixel(fb, px, py, color.r, color.g, color.b, finalA);
        }
    }
}
                                       
void drawRoundedRectangle(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, int radius, bool fill) {
    // Ensure radius does not exceed half of the smallest dimension
    int maxRadius = (width < height ? width : height) / 2;
    radius = (radius > maxRadius) ? maxRadius : radius; // Adjust radius if needed
    
    // Iterate over each pixel within the bounds of the rectangle
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int posX = x + j;
            int posY = y + i;

            bool withinEllipse = false;
            
            // Calculate if the pixel is within the rounded corner range
            if (j < radius && i < radius) {
                // Top-left corner
                withinEllipse = (j - radius) * (j - radius) + (i - radius) * (i - radius) <= radius * radius;
            } else if (j >= width - radius && i < radius) {
                // Top-right corner
                withinEllipse = (j - (width - radius)) * (j - (width - radius)) + (i - radius) * (i - radius) <= radius * radius;
            } else if (j < radius && i >= height - radius) {
                // Bottom-left corner
                withinEllipse = (j - radius) * (j - radius) + (i - (height - radius)) * (i - (height - radius)) <= radius * radius;
            } else if (j >= width - radius && i >= height - radius) {
                // Bottom-right corner
                withinEllipse = (j - (width - radius)) * (j - (width - radius)) + (i - (height - radius)) * (i - (height - radius)) <= radius * radius;
            }
            
            if (fill || withinEllipse || (i >= radius && i < height - radius) || (j >= radius && j < width - radius)) {
                if (posX >= 0 && posX < framebuffer->displayWidth && posY >= 0 && posY < framebuffer->displayHeight) {
                    switch (framebuffer->colorMode) {
                        case COLOR_MODE_TWO:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (posY * framebuffer->displayWidth + posX) / 8;
                            int bitIndex = 7 - ((posY * framebuffer->displayWidth + posX) % 8);
                            if (color.r == framebuffer->colors[1].r && color.g == framebuffer->colors[1].g && color.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex); // Set bit
                            } else {
                                *pixelByte &= ~(1 << bitIndex); // Clear bit
                            }
                            break;
                        }
                        case COLOR_MODE_SIXTEEN:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (posY * framebuffer->displayWidth + posX) / 2;
                            int nibble = ((posY * framebuffer->displayWidth + posX) % 2) ? 0 : 4;
                            uint8_t colorIndex = (color.r >> 4) & 0xF; // Simplified mapping of color to 16-color index
                            *pixelByte = (*pixelByte & (~(0xF << nibble))) | (colorIndex << nibble);
                            break;
                        }
                        case COLOR_MODE_256:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + posY * framebuffer->displayWidth + posX;
                            // Simplified, assuming grayscale mapping; exact palette mapping needed
                            *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                            break;
                        }
                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[posY * framebuffer->displayWidth + posX];
                            blendPixel(fbColor, color);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void drawRoundedRectangleWithTransform(Framebuffer *framebuffer, int x, int y, int width, int height, ColorRGBA color, int radius, bool fill, const Matrix3x3 *transform) {
    // Ensure radius does not exceed half of the smallest dimension
    int maxRadius = (width < height ? width : height) / 2;
    if (radius > maxRadius) radius = maxRadius;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Calculate transformed position for the current pixel
            float transformedX = (float)(x + j);
            float transformedY = (float)(y + i);
            TransformPoint(&transformedX, &transformedY, transform);

            int pixelX = (int)transformedX;
            int pixelY = (int)transformedY;

            // Calculate distance from nearest corner for rounded edges
            int distLeftTop = (j - radius) * (j - radius) + (i - radius) * (i - radius);
            int distRightTop = (j - (width - radius)) * (j - (width - radius)) + (i - radius) * (i - radius);
            int distLeftBottom = (j - radius) * (j - radius) + (i - (height - radius)) * (i - (height - radius));
            int distRightBottom = (j - (width - radius)) * (j - (width - radius)) + (i - (height - radius)) * (i - (height - radius));

            if (fill || (
                (i < radius && j < radius && distLeftTop >= radius * radius) ||
                (i < radius && j >= width - radius && distRightTop >= radius * radius) ||
                (i >= height - radius && j < radius && distLeftBottom >= radius * radius) ||
                (i >= height - radius && j >= width - radius && distRightBottom >= radius * radius) ||
                (i >= radius && i < height - radius) ||
                (j >= radius && j < width - radius)
            )) {
                if (pixelX >= 0 && pixelX < framebuffer->displayWidth && pixelY >= 0 && pixelY < framebuffer->displayHeight) {
                    switch (framebuffer->colorMode) {
                        case COLOR_MODE_TWO:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (pixelY * framebuffer->displayWidth + pixelX) / 8;
                            int bitIndex = 7 - ((pixelY * framebuffer->displayWidth + pixelX) % 8);
                            if (color.r == framebuffer->colors[1].r &&
                                color.g == framebuffer->colors[1].g &&
                                color.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex); // Set bit
                            } else {
                                *pixelByte &= ~(1 << bitIndex); // Clear bit
                            }
                            break;
                        }
                        case COLOR_MODE_SIXTEEN:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (pixelY * framebuffer->displayWidth + pixelX) / 2;
                            int nibbleShift = ((pixelY * framebuffer->displayWidth + pixelX) % 2) ? 0 : 4;
                            uint8_t colorIndex = (color.r >> 4) & 0xF; // Simplified color mapping
                            *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                            break;
                        }
                        case COLOR_MODE_256:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + pixelY * framebuffer->displayWidth + pixelX;
                            // Simplified grayscale; assumes direct grayscale mapping
                            *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                            break;
                        }
                        case COLOR_MODE_BGR888: // Our 18-bit / RGB888 mode
                        {
                            write_pixel_rgb888_alpha(framebuffer, pixelX, pixelY, color);
                            break;
                        }
                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[pixelY * framebuffer->displayWidth + pixelX];
                            blendPixel(fbColor, color);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void drawPolygon_C(Framebuffer *framebuffer, int *xPoints, int *yPoints, int numPoints, ColorRGBA color, bool fill) {
    // Draw the outline using Bresenham's line algorithm
    for (int i = 0; i < numPoints; i++) {
        int x0 = xPoints[i];
        int y0 = yPoints[i];
        int x1 = xPoints[(i + 1) % numPoints];
        int y1 = yPoints[(i + 1) % numPoints];

        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        while (true) {
            if (x0 >= 0 && x0 < framebuffer->displayWidth && y0 >= 0 && y0 < framebuffer->displayHeight) {
                switch (framebuffer->colorMode) {
                    case COLOR_MODE_TWO:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (y0 * framebuffer->displayWidth + x0) / 8;
                        int bitIndex = 7 - ((y0 * framebuffer->displayWidth + x0) % 8);
                        if (color.r == framebuffer->colors[1].r && color.g == framebuffer->colors[1].g && color.b == framebuffer->colors[1].b) {
                            *pixelByte |= (1 << bitIndex); // Set bit
                        } else {
                            *pixelByte &= ~(1 << bitIndex); // Clear bit
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (y0 * framebuffer->displayWidth + x0) / 2;
                        int nibbleShift = ((y0 * framebuffer->displayWidth + x0) % 2) ? 0 : 4;
                        uint8_t colorIndex = (color.r >> 4) & 0xF;
                        *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + y0 * framebuffer->displayWidth + x0;
                        *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                        break;
                    }
                    case COLOR_MODE_RGBA:
                    default:
                    {
                        ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[y0 * framebuffer->displayWidth + x0];
                        blendPixel(fbColor, color);
                        break;
                    }
                }
            }

            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    if (fill) {
        // Find ymin and ymax for scanlines
        int ymin = framebuffer->displayHeight, ymax = 0;
        for (int i = 0; i < numPoints; i++) {
            if (yPoints[i] < ymin) ymin = yPoints[i];
            if (yPoints[i] > ymax) ymax = yPoints[i];
        }

        // Perform scanline filling
        for (int y = ymin; y <= ymax; y++) {
            int intersections[10];
            int numIntersections = 0;

            for (int i = 0; i < numPoints; i++) {
                int x0 = xPoints[i];
                int y0 = yPoints[i];
                int x1 = xPoints[(i + 1) % numPoints];
                int y1 = yPoints[(i + 1) % numPoints];

                if ((y0 < y && y1 >= y) || (y1 < y && y0 >= y)) {
                    int intersectionX = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                    intersections[numIntersections++] = intersectionX;
                }
            }

            // Sort intersections
            for (int i = 0; i < numIntersections - 1; i++) {
                for (int j = i + 1; j < numIntersections; j++) {
                    if (intersections[i] > intersections[j]) {
                        int tmp = intersections[i];
                        intersections[i] = intersections[j];
                        intersections[j] = tmp;
                    }
                }
            }

            // Fill between pairs of intersections
            for (int i = 0; i < numIntersections; i += 2) {
                int startX = intersections[i];
                int endX = intersections[i + 1];

                for (int x = startX; x < endX; x++) {
                    if (x >= 0 && x < framebuffer->displayWidth && y >= 0 && y < framebuffer->displayHeight) {
                        switch (framebuffer->colorMode) {
                            case COLOR_MODE_TWO:
                            {
                                uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) / 8;
                                int bitIndex = 7 - ((y * framebuffer->displayWidth + x) % 8);
                                if (color.r == framebuffer->colors[1].r && color.g == framebuffer->colors[1].g && color.b == framebuffer->colors[1].b) {
                                    *pixelByte |= (1 << bitIndex);
                                } else {
                                    *pixelByte &= ~(1 << bitIndex);
                                }
                                break;
                            }
                            case COLOR_MODE_SIXTEEN:
                            {
                                uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) / 2;
                                int nibbleShift = ((y * framebuffer->displayWidth + x) % 2) ? 0 : 4;
                                uint8_t colorIndex = (color.r >> 4) & 0xF;
                                *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                                break;
                            }
                            case COLOR_MODE_256:
                            {
                                uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + y * framebuffer->displayWidth + x;
                                *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                                break;
                            }
                            case COLOR_MODE_RGBA:
                            default:
                            {
                                ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[y * framebuffer->displayWidth + x];
                                blendPixel(fbColor, color);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

void drawPolygon_C_AntiAliasing(Framebuffer *framebuffer, int *xPoints, int *yPoints, int numPoints, ColorRGBA color, bool fill, bool smooth) {
    // Function to blend color with an additional alpha factor
        
    for (int i = 0; i < numPoints; i++) {
       int x0 = xPoints[i];
       int y0 = yPoints[i];
       int x1 = xPoints[(i + 1) % numPoints];
       int y1 = yPoints[(i + 1) % numPoints];

       if (!smooth) {
           // Regular Bresenham's without changes
           int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
           int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
           int err = dx + dy, e2;

           while (true) {
               if (x0 >= 0 && x0 < framebuffer->displayWidth && y0 >= 0 && y0 < framebuffer->displayHeight) {
                   ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[y0 * framebuffer->displayWidth + x0];
                   blendPixel(fbColor, color);
               }
               if (x0 == x1 && y0 == y1) break;
               e2 = 2 * err;
               if (e2 >= dy) { err += dy; x0 += sx; }
               if (e2 <= dx) { err += dx; y0 += sy; }
           }
       } else {
           // Ensure we draw from base x0,y0 to destination consistently
           float dx = (float)(x1 - x0);
           float dy = (float)(y1 - y0);

           if (fabs(dx) > fabs(dy)) {
               // Graduated along x-axis - Less steep
               if (x0 > x1) { int tmp = x0; x0 = x1; x1 = tmp; tmp = y0; y0 = y1; y1 = tmp; }
               float gradient = dy / dx;
               float y = y0 + gradient;

               for (int x = x0 + 1; x < x1; x++) {
                   if (x >= 0 && x < framebuffer->displayWidth) {
                       int ybase = (int)y;
                       float fractional = y - ybase;
                       if (ybase >= 0 && ybase < framebuffer->displayHeight) {
                           ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[ybase * framebuffer->displayWidth + x];
                           blendPixelWithAlpha(fbColor, color, 1.0f - fractional);
                       }
                       if (ybase + 1 >= 0 && ybase + 1 < framebuffer->displayHeight) {
                           ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[(ybase + 1) * framebuffer->displayWidth + x];
                           blendPixelWithAlpha(fbColor, color, fractional);
                       }
                       y += gradient;
                   }
               }
           } else {
               // Graduated along y-axis - Steep lines
               if (y0 > y1) { int tmp = x0; x0 = x1; x1 = tmp; tmp = y0; y0 = y1; y1 = tmp; }
               float gradient = dx / dy;
               float x = x0 + gradient;

               for (int y = y0 + 1; y < y1; y++) {
                   if (y >= 0 && y < framebuffer->displayHeight) {
                       int xbase = (int)x;
                       float fractional = x - xbase;
                       if (xbase >= 0 && xbase < framebuffer->displayWidth) {
                           ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[y * framebuffer->displayWidth + xbase];
                           blendPixelWithAlpha(fbColor, color, 1.0f - fractional);
                       }
                       if (xbase + 1 >= 0 && xbase + 1 < framebuffer->displayWidth) {
                           ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[y * framebuffer->displayWidth + xbase + 1];
                           blendPixelWithAlpha(fbColor, color, fractional);
                       }
                       x += gradient;
                   }
               }
           }
       }
   }

    if (fill) {
        // Find ymin and ymax for scanlines
        int ymin = framebuffer->displayHeight, ymax = 0;
        for (int i = 0; i < numPoints; i++) {
            if (yPoints[i] < ymin) ymin = yPoints[i];
            if (yPoints[i] > ymax) ymax = yPoints[i];
        }

        // Perform scanline filling
        for (int y = ymin; y <= ymax; y++) {
            int intersections[10];
            int numIntersections = 0;

            for (int i = 0; i < numPoints; i++) {
                int x0 = xPoints[i];
                int y0 = yPoints[i];
                int x1 = xPoints[(i + 1) % numPoints];
                int y1 = yPoints[(i + 1) % numPoints];

                if ((y0 < y && y1 >= y) || (y1 < y && y0 >= y)) {
                    int intersectionX = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                    intersections[numIntersections++] = intersectionX;
                }
            }

            // Sort intersections
            for (int i = 0; i < numIntersections - 1; i++) {
                for (int j = i + 1; j < numIntersections; j++) {
                    if (intersections[i] > intersections[j]) {
                        int tmp = intersections[i];
                        intersections[i] = intersections[j];
                        intersections[j] = tmp;
                    }
                }
            }

            // Fill between pairs of intersections
            for (int i = 0; i < numIntersections; i += 2) {
                int startX = intersections[i];
                int endX = intersections[i + 1];

                for (int x = startX; x < endX; x++) {
                    if (x >= 0 && x < framebuffer->displayWidth && y >= 0 && y < framebuffer->displayHeight) {
                        switch (framebuffer->colorMode) {
                            case COLOR_MODE_TWO:
                            {
                                uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) / 8;
                                int bitIndex = 7 - ((y * framebuffer->displayWidth + x) % 8);
                                if (color.r == framebuffer->colors[1].r && color.g == framebuffer->colors[1].g && color.b == framebuffer->colors[1].b) {
                                    *pixelByte |= (1 << bitIndex);
                                } else {
                                    *pixelByte &= ~(1 << bitIndex);
                                }
                                break;
                            }
                            case COLOR_MODE_SIXTEEN:
                            {
                                uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) / 2;
                                int nibbleShift = ((y * framebuffer->displayWidth + x) % 2) ? 0 : 4;
                                uint8_t colorIndex = (color.r >> 4) & 0xF;
                                *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                                break;
                            }
                            case COLOR_MODE_256:
                            {
                                uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + y * framebuffer->displayWidth + x;
                                *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                                break;
                            }
                            case COLOR_MODE_RGBA:
                            default:
                            {
                                ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[y * framebuffer->displayWidth + x];
                                blendPixel(fbColor, color);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}



void drawPolygonWithTransform(Framebuffer *framebuffer, Vector3 *vertices, int numVertices, ColorRGBA color, bool fill, const Matrix3x3 *transform) {

    // Allocate transformed points
    int *xPoints = (int *)malloc(numVertices * sizeof(int));
    int *yPoints = (int *)malloc(numVertices * sizeof(int));

    // Apply transformation to vertices
    for (int i = 0; i < numVertices; i++) {
        float x = vertices[i].x;
        float y = vertices[i].y;
        TransformPoint(&x, &y, transform);
        xPoints[i] = (int)x;
        yPoints[i] = (int)y;
    }

    // Draw the outline using Bresenham's line algorithm
    for (int i = 0; i < numVertices; i++) {
        int x0 = xPoints[i];
        int y0 = yPoints[i];
        int x1 = xPoints[(i + 1) % numVertices];
        int y1 = yPoints[(i + 1) % numVertices];

        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        while (true) {
            if (x0 >= 0 && x0 < framebuffer->displayWidth && y0 >= 0 && y0 < framebuffer->displayHeight) {
                ColorRGBA *fbColor = &framebuffer->pixelData[y0 * framebuffer->displayWidth + x0];
                blendPixel(fbColor, color);
            }

            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    if (fill) {
        int ymin = framebuffer->displayHeight, ymax = 0;
        for (int i = 0; i < numVertices; i++) {
            if (yPoints[i] < ymin) ymin = yPoints[i];
            if (yPoints[i] > ymax) ymax = yPoints[i];
        }

        // Dynamic array to store intersections, using twice the number of vertices
        int *intersections = (int *)malloc(numVertices * 2 * sizeof(int));

        for (int y = ymin; y <= ymax; y++) {
            int numIntersections = 0;

            for (int i = 0; i < numVertices; i++) {
                int x0 = xPoints[i];
                int y0 = yPoints[i];
                int x1 = xPoints[(i + 1) % numVertices];
                int y1 = yPoints[(i + 1) % numVertices];

                if ((y0 < y && y1 >= y) || (y1 < y && y0 >= y)) {
                    int intersectionX = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                    intersections[numIntersections++] = intersectionX;
                }
            }

            // Sort intersections to find valid fill regions
            for (int i = 0; i < numIntersections - 1; i++) {
                for (int j = i + 1; j < numIntersections; j++) {
                    if (intersections[i] > intersections[j]) {
                        int tmp = intersections[i];
                        intersections[i] = intersections[j];
                        intersections[j] = tmp;
                    }
                }
            }

            for (int i = 0; i < numIntersections; i += 2) {
                int startX = intersections[i];
                int endX = intersections[i + 1];

                for (int x = startX; x < endX; x++) {
                    if (x >= 0 && x < framebuffer->displayWidth && y >= 0 && y < framebuffer->displayHeight) {
                        ColorRGBA *fbColor = &framebuffer->pixelData[y * framebuffer->displayWidth + x];
                        blendPixel(fbColor, color);
                    }
                }
            }
        }

        free(intersections);
    }

    // Free temporary arrays
    free(xPoints);
    free(yPoints);
}



void drawPolygonRotate3DY(Framebuffer *framebuffer, Vector3 *vertices, int numVertices, ColorRGBA color, bool fill, float angle) {
    float cosAngle, sinAngle;
    RotationYMatrix(angle, &cosAngle, &sinAngle);

    // Project 3D rotation onto 2D plane
    int *xPoints = (int *)malloc(numVertices * sizeof(int));
    int *yPoints = (int *)malloc(numVertices * sizeof(int));

    // Apply 3D rotation in the Y-axis to each vertex
    for (int i = 0; i < numVertices; i++) {
        float transformedX = vertices[i].x * cosAngle + vertices[i].z * sinAngle;
        float transformedY = vertices[i].y;
        float depth = vertices[i].z * cosAngle - vertices[i].x * sinAngle;

        // Perspective projection into 2D
        float perspective = 1 / (1 + depth * 0.001f);  // Adjust scaling for perception depth
        xPoints[i] = (int)(transformedX * perspective + framebuffer->displayWidth / 2);
        yPoints[i] = (int)(transformedY * perspective + framebuffer->displayHeight / 2);
    }

    // Draw the outline using Bresenham's line algorithm
    for (int i = 0; i < numVertices; i++) {
        int x0 = xPoints[i];
        int y0 = yPoints[i];
        int x1 = xPoints[(i + 1) % numVertices];
        int y1 = yPoints[(i + 1) % numVertices];

        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        while (true) {
            if (x0 >= 0 && x0 < framebuffer->displayWidth && y0 >= 0 && y0 < framebuffer->displayHeight) {
                ColorRGBA *fbColor = &framebuffer->pixelData[y0 * framebuffer->displayWidth + x0];
                blendPixel(fbColor, color);
            }

            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    if (fill) {
        int ymin = framebuffer->displayHeight, ymax = 0;
        for (int i = 0; i < numVertices; i++) {
            if (yPoints[i] < ymin) ymin = yPoints[i];
            if (yPoints[i] > ymax) ymax = yPoints[i];
        }

        int *intersections = (int *)malloc(numVertices * 2 * sizeof(int));

        for (int y = ymin; y <= ymax; y++) {
            int numIntersections = 0;

            for (int i = 0; i < numVertices; i++) {
                int x0 = xPoints[i];
                int y0 = yPoints[i];
                int x1 = xPoints[(i + 1) % numVertices];
                int y1 = yPoints[(i + 1) % numVertices];

                if ((y0 < y && y1 >= y) || (y1 < y && y0 >= y)) {
                    int intersectionX = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                    intersections[numIntersections++] = intersectionX;
                }
            }

            for (int i = 0; i < numIntersections - 1; i++) {
                for (int j = i + 1; j < numIntersections; j++) {
                    if (intersections[i] > intersections[j]) {
                        int tmp = intersections[i];
                        intersections[i] = intersections[j];
                        intersections[j] = tmp;
                    }
                }
            }

            for (int i = 0; i < numIntersections; i += 2) {
                int startX = intersections[i];
                int endX = intersections[i + 1];

                for (int x = startX; x < endX; x++) {
                    if (x >= 0 && x < framebuffer->displayWidth && y >= 0 && y < framebuffer->displayHeight) {
                        ColorRGBA *fbColor = &framebuffer->pixelData[y * framebuffer->displayWidth + x];
                        blendPixel(fbColor, color);
                    }
                }
            }
        }

        free(intersections);
    }

    free(xPoints);
    free(yPoints);
}

void fillRectangleWithGradient(Framebuffer *framebuffer, int x, int y, int width, int height, const Gradient *gradient, const Matrix3x3 *transform) {
    fillRectangleWithGradientExtended(framebuffer, x, y, width, height, 0, 0, width, height, gradient, transform);
}

// Assuming write_pixel_rgb888_alpha helper (or its logic) is available.
void fillRectangleWithGradientExtended(Framebuffer *framebuffer, int x, int y, int width, int height, int clipX, int clipY, int clipW, int clipH, const Gradient *gradient, const Matrix3x3 *transform) {

    // Calculate gradient direction vectors from the angle
    float dirX = cosf(gradient->angle);
    float dirY = sinf(gradient->angle);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Find the position along the gradient line for each pixel
            float factor = ((float)j * dirX + (float)i * dirY) / (float)width;
            ColorRGBA startColor, endColor;
            float startPos = 0.0f, endPos = 1.0f;
            
            // Clamp factor to 0.0-1.0 range
            if (factor < 0.0f) factor = 0.0f;
            if (factor > 1.0f) factor = 1.0f;

            // Determine the correct color stops to interpolate between
            for (int k = 0; k < gradient->numStops - 1; k++) {
                if (factor >= gradient->stops[k].position && factor <= gradient->stops[k + 1].position) {
                    startColor = gradient->stops[k].color;
                    endColor = gradient->stops[k + 1].color;
                    startPos = gradient->stops[k].position;
                    endPos = gradient->stops[k + 1].position;
                    break;
                }
            }

            float localFactor = (factor - startPos) / (endPos - startPos);
            
            // Interpolate R, G, B, and Alpha
            ColorRGBA interpColor;
            interpColor.r = (uint8_t)((1 - localFactor) * startColor.r + localFactor * endColor.r);
            interpColor.g = (uint8_t)((1 - localFactor) * startColor.g + localFactor * endColor.g);
            interpColor.b = (uint8_t)((1 - localFactor) * startColor.b + localFactor * endColor.b);
            interpColor.a = (uint8_t)((1 - localFactor) * startColor.a + localFactor * endColor.a); // Interpolate alpha too

            float pixelX = (float)(x + j);
            float pixelY = (float)(y + i);
            
            if (pixelX < clipX || pixelX >= clipX + clipW ||
                pixelY < clipY || pixelY >= clipY + clipH) {
                            continue;
                        }

            if (transform) {
                TransformPoint(&pixelX, &pixelY, transform);
            }

            int indexX = (int)pixelX;
            int indexY = (int)pixelY;

            // Check bounds and apply blending
            if (indexX >= 0 && indexX < framebuffer->displayWidth && indexY >= 0 && indexY < framebuffer->displayHeight) {
                
                switch (framebuffer->colorMode) {
                    case COLOR_MODE_TWO:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (indexY * framebuffer->displayWidth + indexX) / 8;
                        int bitIndex = 7 - ((indexY * framebuffer->displayWidth + indexX) % 8);
                        if (interpColor.r == framebuffer->colors[1].r && interpColor.g == framebuffer->colors[1].g && interpColor.b == framebuffer->colors[1].b) {
                            *pixelByte |= (1 << bitIndex); // Set bit for color 1
                        } else {
                            *pixelByte &= ~(1 << bitIndex); // Clear bit for color 0
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (indexY * framebuffer->displayWidth + indexX) / 2;
                        int nibbleShift = ((indexY * framebuffer->displayWidth + indexX) % 2) ? 0 : 4;
                        uint8_t colorIndex = (interpColor.r >> 4) & 0xF; // Simplified mapping
                        *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + indexY * framebuffer->displayWidth + indexX;
                        *pixelByte = (interpColor.r >> 6) | ((interpColor.g >> 6) << 2) | ((interpColor.b >> 6) << 4);
                        break;
                    }
                    
                    // --- NEW 18-BIT COLOR MODE (RGB888) ---
                    case COLOR_MODE_BGR888:
                    {
                        // Use the optimized integer blending function
                        write_pixel_rgb888_alpha(framebuffer, indexX, indexY, interpColor);
                        break;
                    }
                    
                    case COLOR_MODE_RGBA:
                    default:
                    {
                        ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[indexY * framebuffer->displayWidth + indexX];
                        blendPixel(fbColor, interpColor);
                        break;
                    }
                }
            }
        }
    }
}

// Helper for optimized blending (Integer version of your blendPixel)
static inline void fast_blend_pixel(Framebuffer *fb, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (a == 0) return;
    int index = (y * fb->displayWidth + x) * 3;
    uint8_t *p = (uint8_t *)fb->pixelData + index;

    if (a == 255) {
        p[0] = b; p[1] = g; p[2] = r; // Write BGR directly
    } else {
        // Integer blend: (src * a + dest * (255-a)) / 255
        // We use bitwise shift >> 8 for speed (approx / 256)
        uint16_t inv_a = 255 - a;
        p[0] = (b * a + p[0] * inv_a) >> 8;
        p[1] = (g * a + p[1] * inv_a) >> 8;
        p[2] = (r * a + p[2] * inv_a) >> 8;
    }
}

// --- 3. THE OPTIMIZED "SIMD" FUNCTION ---
// Uses Fixed-Point Math (Q16.16) to replace Floats.
// This triggers the ESP32-S3's efficient integer DSP pipeline.
void fillRectangleWithGradientSIMD(Framebuffer *framebuffer, int x, int y, int width, int height, const Gradient *gradient) {
    
    // 1. Pre-calculate Steps in Fixed Point (Q16.16)
    // 65536 = 1.0f
    int32_t angle_cos = (int32_t)(cosf(gradient->angle) * 65536.0f);
    int32_t angle_sin = (int32_t)(sinf(gradient->angle) * 65536.0f);
    
    // step = (change in factor per pixel)
    // If width is 0, avoid division by zero
    int32_t stepX = (width > 0) ? (angle_cos / width) : 0;
    int32_t stepY = (width > 0) ? (angle_sin / width) : 0;

    // 2. Pre-calculate Raw Color Diffs (Simple Integer Math)
    // Range: -255 to +255
    ColorRGBA c1 = gradient->stops[0].color;
    ColorRGBA c2 = gradient->stops[gradient->numStops - 1].color;

    int32_t dR = c2.r - c1.r;
    int32_t dG = c2.g - c1.g;
    int32_t dB = c2.b - c1.b;
    int32_t dA = c2.a - c1.a;

    for (int i = 0; i < height; i++) {
        
        // Calculate starting factor for this row
        int32_t rowFactor = (i * stepY);

        // Pointer setup
        int bufferIdx = ((y + i) * framebuffer->displayWidth + x) * 3;
        uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + bufferIdx;

        for (int j = 0; j < width; j++) {
            
            // Limit factor to 0..65536 (0.0 to 1.0)
            // We use a temporary variable so we don't clamp the actual accumulator
            int32_t drawFactor = rowFactor;
            if (drawFactor < 0) drawFactor = 0;
            if (drawFactor > 65536) drawFactor = 65536;

            // --- FIXED COLOR MATH ---
            // Formula: Start + (Difference * Factor) / 65536
            // Since Factor is already scaled by 65536, ">> 16" does the division.
            // Max Value: 255 * 65536 = 16,711,680 (Fits easily in int32)
            
            uint8_t r = c1.r + ((dR * drawFactor) >> 16);
            uint8_t g = c1.g + ((dG * drawFactor) >> 16);
            uint8_t b = c1.b + ((dB * drawFactor) >> 16);
            uint8_t a = c1.a + ((dA * drawFactor) >> 16);

            // Write Pixel (Fast Path BGR888)
            if (a == 255) {
                pixelPtr[0] = b;
                pixelPtr[1] = g;
                pixelPtr[2] = r;
            } else if (a > 0) {
                // Alpha Blend
                uint16_t inv_a = 255 - a;
                pixelPtr[0] = (b * a + pixelPtr[0] * inv_a) >> 8;
                pixelPtr[1] = (g * a + pixelPtr[1] * inv_a) >> 8;
                pixelPtr[2] = (r * a + pixelPtr[2] * inv_a) >> 8;
            }

            // Next Pixel
            pixelPtr += 3;
            rowFactor += stepX;
        }
    }
}

void fillRectangleWithGradientOptimized(Framebuffer *framebuffer, int x, int y, int width, int height,
                               int clipX, int clipY, int clipW, int clipH,
                               const Gradient *gradient) {

    // 1. Calculate Intersection of Draw Rect and Clip Rect
    int startX = (x > clipX) ? x : clipX;
    int startY = (y > clipY) ? y : clipY;
    int endX   = ((x + width) < (clipX + clipW)) ? (x + width) : (clipX + clipW);
    int endY   = ((y + height) < (clipY + clipH)) ? (y + height) : (clipY + clipH);

    // If the intersection is invalid (rect is fully outside clip), return immediately
    if (endX <= startX || endY <= startY) return;

    // 2. Setup Gradient Constants (Fixed Point Q16.16)
    // 65536 = 1.0f
    int32_t angle_cos = (int32_t)(cosf(gradient->angle) * 65536.0f);
    int32_t angle_sin = (int32_t)(sinf(gradient->angle) * 65536.0f);
    
    // Steps: How much the gradient factor changes per 1 pixel
    // Avoid division by zero
    int32_t stepX = (width > 0) ? (angle_cos / width) : 0;
    int32_t stepY = (width > 0) ? (angle_sin / width) : 0;

    // 3. Pre-calculate Color Differences (Range -255 to +255)
    ColorRGBA c1 = gradient->stops[0].color;
    ColorRGBA c2 = gradient->stops[gradient->numStops - 1].color;

    int32_t dR = c2.r - c1.r;
    int32_t dG = c2.g - c1.g;
    int32_t dB = c2.b - c1.b;
    int32_t dA = c2.a - c1.a;

    // 4. Adjust Gradient Start Offsets
    // Since we might be starting at pixel 10 (startX) instead of 0 (x),
    // we need to "fast forward" the gradient math to the correct starting color.
    int offsetX = startX - x;
    int offsetY = startY - y;
    
    // Calculate the factor for the top-left corner of the *visible* area
    int32_t initialRowFactor = (offsetY * stepY) + (offsetX * stepX);

    // 5. RENDER LOOP (Only loops visible pixels)
    for (int i = startY; i < endY; i++) {
        
        // Start factor for this specific row
        int32_t currentFactor = initialRowFactor;

        // Calculate memory pointer for the start of this clipped row
        int bufferIdx = (i * framebuffer->displayWidth + startX) * 3;
        uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + bufferIdx;

        for (int j = startX; j < endX; j++) {
            
            // Clamp factor to valid range [0..65536]
            int32_t drawFactor = currentFactor;
            if (drawFactor < 0) drawFactor = 0;
            if (drawFactor > 65536) drawFactor = 65536;

            // Compute Color (Fixed Point Math)
            // Result = Start + ((Diff * Factor) >> 16)
            uint8_t r = c1.r + ((dR * drawFactor) >> 16);
            uint8_t g = c1.g + ((dG * drawFactor) >> 16);
            uint8_t b = c1.b + ((dB * drawFactor) >> 16);
            uint8_t a = c1.a + ((dA * drawFactor) >> 16);

            // Write Pixel to Memory
            if (a == 255) {
                // Fast Path: Opaque
                pixelPtr[0] = b;
                pixelPtr[1] = g;
                pixelPtr[2] = r;
            } else if (a > 0) {
                // Blend Path: Transparent
                uint16_t inv_a = 255 - a;
                pixelPtr[0] = (b * a + pixelPtr[0] * inv_a) >> 8;
                pixelPtr[1] = (g * a + pixelPtr[1] * inv_a) >> 8;
                pixelPtr[2] = (r * a + pixelPtr[2] * inv_a) >> 8;
            }

            // Advance Pointers
            pixelPtr += 3;       // Move right one pixel in memory
            currentFactor += stepX; // Move gradient factor forward
        }
        
        // Advance the row factor for the next line (Vertical Step)
        // Note: We only add stepY because offsetX is constant for the start of every row
        initialRowFactor += stepY;
    }
}

// Requires your Matrix3x3Inverse and Matrix3x3Determinant functions to be defined above

void fillRectangleWithGradientOptimizedWithTransform(Framebuffer *framebuffer, int x, int y, int width, int height,
                                          int clipX, int clipY, int clipW, int clipH,
                                          const Gradient *gradient, const Matrix3x3 *transform) {

    // 1. Setup Gradient Constants (Fixed Point Q16.16)
    int32_t angle_cos = (int32_t)(cosf(gradient->angle) * 65536.0f);
    int32_t angle_sin = (int32_t)(sinf(gradient->angle) * 65536.0f);

    ColorRGBA c1 = gradient->stops[0].color;
    ColorRGBA c2 = gradient->stops[gradient->numStops - 1].color;

    int32_t dR = c2.r - c1.r;
    int32_t dG = c2.g - c1.g;
    int32_t dB = c2.b - c1.b;
    int32_t dA = c2.a - c1.a;

    // Gradient length squared (for normalizing factor)
    // We assume the gradient spans the whole width of the generic rect
    // Factor calculation: (localX * cos + localY * sin) / width
    
    // 2. Handle Transform & Bounding Box
    // We need to find where the rectangle ends up on screen to know which pixels to loop
    float corners[4][2] = {
        {(float)x,         (float)y},          // Top-Left
        {(float)(x+width), (float)y},          // Top-Right
        {(float)(x+width), (float)(y+height)}, // Bottom-Right
        {(float)x,         (float)(y+height)}  // Bottom-Left
    };

    float minSx = 100000.0f, minSy = 100000.0f;
    float maxSx = -100000.0f, maxSy = -100000.0f;

    // Transform all 4 corners to find screen bounds
    for(int i=0; i<4; i++) {
        if (transform) {
            TransformPoint(&corners[i][0], &corners[i][1], transform);
        }
        if (corners[i][0] < minSx) minSx = corners[i][0];
        if (corners[i][0] > maxSx) maxSx = corners[i][0];
        if (corners[i][1] < minSy) minSy = corners[i][1];
        if (corners[i][1] > maxSy) maxSy = corners[i][1];
    }

    // 3. Clip the Bounding Box to the Clip Rect
    int startX = (int)minSx;
    int startY = (int)minSy;
    int endX   = (int)maxSx + 1; // +1 to ensure coverage
    int endY   = (int)maxSy + 1;

    if (startX < clipX) startX = clipX;
    if (startY < clipY) startY = clipY;
    if (endX > clipX + clipW) endX = clipX + clipW;
    if (endY > clipY + clipH) endY = clipY + clipH;

    if (endX <= startX || endY <= startY) return; // Nothing to draw

    // 4. Calculate Inverse Matrix steps
    // Instead of multiplying a matrix for every pixel (slow), we calculate
    // how much 'u' and 'v' (source coords) change for 1 pixel of movement on screen.
    float dU_dx = 1.0f, dV_dx = 0.0f; // Default (No transform)
    float dU_dy = 0.0f, dV_dy = 1.0f;
    float startU = (float)startX - x; // Default offset
    float startV = (float)startY - y;

    if (transform) {
        Matrix3x3 inv;
        if (!Matrix3x3Inverse(transform, &inv)) return; // Singular matrix

        // Calculate how source coords change when screen X increases by 1
        dU_dx = inv.m[0][0];
        dV_dx = inv.m[1][0];

        // Calculate how source coords change when screen Y increases by 1
        dU_dy = inv.m[0][1];
        dV_dy = inv.m[1][1];

        // Calculate source coord for the very first pixel (startX, startY)
        // source = Inv * screen
        startU = startX * inv.m[0][0] + startY * inv.m[0][1] + inv.m[0][2];
        startV = startX * inv.m[1][0] + startY * inv.m[1][1] + inv.m[1][2];
        
        // Adjust for the rectangle's local origin
        startU -= x;
        startV -= y;
    }

    // Convert Steps to Fixed Point (Q16.16) for the loop
    int32_t fp_dU_dx = (int32_t)(dU_dx * 65536.0f);
    int32_t fp_dV_dx = (int32_t)(dV_dx * 65536.0f);
    int32_t fp_dU_dy = (int32_t)(dU_dy * 65536.0f);
    int32_t fp_dV_dy = (int32_t)(dV_dy * 65536.0f);
    
    int32_t fp_U_row = (int32_t)(startU * 65536.0f);
    int32_t fp_V_row = (int32_t)(startV * 65536.0f);
    
    // Width inverse for gradient calculation (avoid division in loop)
    // We want: factor = (u*cos + v*sin) / width
    // So: factor = (u*cos + v*sin) * (1/width)
    // We pre-scale 1/width by a bit less to keep things in range if needed,
    // but here we just rely on standard fixed point.
    // Optimization: Pre-calculate the "Factor Step" per screen pixel
    // dFactor/dx = (dU/dx * cos + dV/dx * sin) / width
    
    int32_t grad_step_X = 0, grad_step_Y = 0, grad_start = 0;
    
    if (width > 0) {
        // How much the gradient factor changes per X screen pixel
        // We use float intermediate to prevent overflow before converting back to int
        float invW = 1.0f / width;
        grad_step_X = (int32_t)((dU_dx * angle_cos + dV_dx * angle_sin) * invW);
        grad_step_Y = (int32_t)((dU_dy * angle_cos + dV_dy * angle_sin) * invW);
        
        // Starting gradient factor
        float u0 = startU;
        float v0 = startV;
        // Factor = (u*cos + v*sin) / width. Scale by 65536 for Fixed Point.
        grad_start = (int32_t)(((u0 * cosf(gradient->angle) + v0 * sinf(gradient->angle)) * invW) * 65536.0f);
    }


    // 5. RENDER LOOP
    for (int i = startY; i < endY; i++) {
        
        int32_t fp_U = fp_U_row;
        int32_t fp_V = fp_V_row;
        int32_t currentFactor = grad_start;

        int bufferIdx = (i * framebuffer->displayWidth + startX) * 3;
        uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + bufferIdx;

        for (int j = startX; j < endX; j++) {
            
            // 6. Check if this screen pixel maps to a valid spot in the source image
            // fp_U and fp_V are in Q16.16, so 1.0 = 65536
            // We need 0 <= u < width and 0 <= v < height
            
            int32_t u_int = fp_U >> 16;
            int32_t v_int = fp_V >> 16;

            if (u_int >= 0 && u_int < width && v_int >= 0 && v_int < height) {
                
                // VALID PIXEL - Calculate Color
                int32_t drawFactor = currentFactor;
                if (drawFactor < 0) drawFactor = 0;
                if (drawFactor > 65536) drawFactor = 65536;

                // Color Math
                uint8_t r = c1.r + ((dR * drawFactor) >> 16);
                uint8_t g = c1.g + ((dG * drawFactor) >> 16);
                uint8_t b = c1.b + ((dB * drawFactor) >> 16);
                uint8_t a = c1.a + ((dA * drawFactor) >> 16);

                // Write (Fast Path)
                if (a == 255) {
                    pixelPtr[0] = b; pixelPtr[1] = g; pixelPtr[2] = r;
                } else if (a > 0) {
                    uint16_t inv_a = 255 - a;
                    pixelPtr[0] = (b * a + pixelPtr[0] * inv_a) >> 8;
                    pixelPtr[1] = (g * a + pixelPtr[1] * inv_a) >> 8;
                    pixelPtr[2] = (r * a + pixelPtr[2] * inv_a) >> 8;
                }
            }
            
            // Advance One Pixel Right
            pixelPtr += 3;
            fp_U += fp_dU_dx;
            fp_V += fp_dV_dx;
            currentFactor += grad_step_X;
        }
        
        // Advance One Pixel Down
        fp_U_row += fp_dU_dy;
        fp_V_row += fp_dV_dy;
        grad_start += grad_step_Y;
    }
}

void fillPolygonWithGradient(Framebuffer *framebuffer, Vector3 *vertices, int numVertices, const Gradient *gradient, const Matrix3x3 *transform, bool antiAlias) {
    
    int *xPoints = (int *)malloc(numVertices * sizeof(int));
    int *yPoints = (int *)malloc(numVertices * sizeof(int));

    // Apply transformation to each vertex and convert to integer screen coordinates
    for (int i = 0; i < numVertices; i++) {
        float x = vertices[i].x;
        float y = vertices[i].y;
        if (transform) {
            TransformPoint(&x, &y, transform);
        }
        xPoints[i] = (int)roundf(x);
        yPoints[i] = (int)roundf(y);
    }

    // Calculate gradient direction vectors
    float dirX = cosf(gradient->angle);
    float dirY = sinf(gradient->angle);

    // --- NEW/CORRECTED BOUNDING BOX CALCULATION ---
    int xmin = framebuffer->displayWidth, xmax = 0;
    int ymin = framebuffer->displayHeight, ymax = 0;
    for (int i = 0; i < numVertices; i++) {
        xmin = (xPoints[i] < xmin) ? xPoints[i] : xmin;
        xmax = (xPoints[i] > xmax) ? xPoints[i] : xmax;
        ymin = (yPoints[i] < ymin) ? yPoints[i] : ymin;
        ymax = (yPoints[i] > ymax) ? yPoints[i] : ymax;
    }
    
    // Calculate the dimensions of the polygon's bounding box
    float gradient_width = (float)(xmax - xmin);
    float gradient_height = (float)(ymax - ymin);
    
    // Calculate the total length (L) the gradient must span, projected onto the axis
    float total_projected_length = fabsf(dirX * gradient_width) + fabsf(dirY * gradient_height);
    if (total_projected_length < 0.001f) total_projected_length = 1.0f; // Prevent division by zero

    // --- End NEW/CORRECTED BOUNDING BOX CALCULATION ---

    // Dynamic array to store intersections
    int *intersections = (int *)malloc(numVertices * 2 * sizeof(int));

    for (int y = ymin; y <= ymax; y++) {
        int numIntersections = 0;
        
        // --- Scanline Edge Intersection Calculation ---
        for (int i = 0; i < numVertices; i++) {
            int x0 = xPoints[i];
            int y0 = yPoints[i];
            int x1 = xPoints[(i + 1) % numVertices];
            int y1 = yPoints[(i + 1) % numVertices];

            // Check if the edge crosses the current scanline 'y'
            if ((y0 < y && y1 >= y) || (y1 < y && y0 >= y)) {
                float dy = (float)(y1 - y0);
                float dx = (float)(x1 - x0);
                float slope = dx / dy;
                float diff = (float)(y - y0);
                float x_intersect = x0 + diff * slope;
                intersections[numIntersections++] = (int)x_intersect;
            }
        }

        // --- Sort intersections for filling ---
        // (Simple bubble sort used for simplicity, insertion sort or qsort is faster)
        for (int i = 0; i < numIntersections - 1; i++) {
            for (int j = i + 1; j < numIntersections; j++) {
                if (intersections[i] > intersections[j]) {
                    int tmp = intersections[i];
                    intersections[i] = intersections[j];
                    intersections[j] = tmp;
                }
            }
        }

        // Calculate factors and fill between pairs of intersections
                for (int i = 0; i < numIntersections; i += 2) {
                    int startX = intersections[i];
                    int endX = intersections[i + 1];

                    for (int x = startX; x < endX; x++) {
                        
                        // --- CORRECTED FACTOR CALCULATION ---
                        
                        // 1. Calculate the distance from the polygon's top-left (xmin, ymin)
                        float projected_distance = dirX * (x - xmin) + dirY * (y - ymin);
                        
                        // 2. Normalize by the calculated total projected length (L)
                        float factor = projected_distance / total_projected_length;
                        
                        // --- END CORRECTED FACTOR CALCULATION ---

                        if (factor < 0) factor = 0;
                        if (factor > 1) factor = 1;

                        // Determine correct color stops to interpolate between
                        ColorRGBA startColor, endColor;
                        float startPos = 0.0f, endPos = 1.0f;
                        // ... (Color stop logic remains the same) ...
                        
                        for (int k = 0; k < gradient->numStops - 1; k++) {
                            if (factor >= gradient->stops[k].position && factor <= gradient->stops[k + 1].position) {
                                startColor = gradient->stops[k].color;
                                endColor = gradient->stops[k + 1].color;
                                startPos = gradient->stops[k].position;
                                endPos = gradient->stops[k + 1].position;
                                break;
                            }
                        }

                        float localFactor = (factor - startPos) / (endPos - startPos);

                        ColorRGBA interpColor = {
                            .r = (uint8_t)((1 - localFactor) * startColor.r + localFactor * endColor.r),
                            .g = (uint8_t)((1 - localFactor) * startColor.g + localFactor * endColor.g),
                            .b = (uint8_t)((1 - localFactor) * startColor.b + localFactor * endColor.b),
                            .a = (uint8_t)((1 - localFactor) * startColor.a + localFactor * endColor.a)
                        };

                        // Check framebuffer bounds and apply according to color mode
                        if (x >= 0 && x < framebuffer->displayWidth && y >= 0 && y < framebuffer->displayHeight) {
                            switch (framebuffer->colorMode) {
                                // ... (existing cases) ...
                                case COLOR_MODE_BGR888:
                                {
                                    write_pixel_rgb888_alpha(framebuffer, x, y, interpColor);
                                    break;
                                }
                                case COLOR_MODE_RGBA:
                                default:
                                {
                                    ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[y * framebuffer->displayWidth + x];
                                    blendPixelWithAlpha(fbColor, interpColor, 1.0f);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            free(intersections);
            free(xPoints);
            free(yPoints);
}


/*void fillRoundedRectangleWithGradient(Framebuffer *framebuffer, int x, int y, int width, int height, const Gradient *gradient, int radius, const Matrix3x3 *transform, bool antiAlias) {
    // Ensure radius does not exceed half of the smallest dimension
    int maxRadius = (width < height ? width : height) / 2;
    radius = (radius > maxRadius) ? maxRadius : radius; // Adjust radius if needed

    // Calculate gradient direction vectors from the angle
    float dirX = cosf(gradient->angle);
    float dirY = sinf(gradient->angle);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            bool withinEllipse = false;
            float alphaFactor = 1.0f; // Full opacity by default

            // Calculate distance from the nearest corner for rounded edges
            if (j < radius && i < radius) {
                // Top-left corner
                float dx = radius - j - (antiAlias ? 0.5f : 0.0f);
                float dy = radius - i - (antiAlias ? 0.5f : 0.0f);
                float distance = sqrtf(dx * dx + dy * dy);
                if (distance <= radius) {
                    withinEllipse = true;
                    alphaFactor = antiAlias ? fminf(1.0f, radius - distance) : 1.0f;
                }
            }
            else if (j >= width - radius && i < radius) {
                // Top-right corner
                float dx = j - (width - radius) + (antiAlias ? 0.5f : 0.0f);
                float dy = radius - i - (antiAlias ? 0.5f : 0.0f);
                float distance = sqrtf(dx * dx + dy * dy);
                if (distance <= radius) {
                    withinEllipse = true;
                    alphaFactor = antiAlias ? fminf(1.0f, radius - distance) : 1.0f;
                }
            }
            else if (j < radius && i >= height - radius) {
                // Bottom-left corner
                float dx = radius - j - (antiAlias ? 0.5f : 0.0f);
                float dy = i - (height - radius) + (antiAlias ? 0.5f : 0.0f);
                float distance = sqrtf(dx * dx + dy * dy);
                if (distance <= radius) {
                    withinEllipse = true;
                    alphaFactor = antiAlias ? fminf(1.0f, radius - distance) : 1.0f;
                }
            }
            else if (j >= width - radius && i >= height - radius) {
                // Bottom-right corner
                float dx = j - (width - radius) + (antiAlias ? 0.5f : 0.0f);
                float dy = i - (height - radius) + (antiAlias ? 0.5f : 0.0f);
                float distance = sqrtf(dx * dx + dy * dy);
                if (distance <= radius) {
                    withinEllipse = true;
                    alphaFactor = antiAlias ? fminf(1.0f, radius - distance) : 1.0f;
                }
            }

            if (withinEllipse || (i >= radius && i < height - radius) || (j >= radius && j < width - radius)) {
                // Find the position along the gradient line for each pixel
                float factor = ((float)j * dirX + (float)i * dirY) / (float)width;
                ColorRGBA startColor, endColor;
                float startPos = 0.0f, endPos = 1.0f;

                // Determine correct color stops to interpolate between
                for (int k = 0; k < gradient->numStops - 1; k++) {
                    if (factor >= gradient->stops[k].position && factor <= gradient->stops[k + 1].position) {
                        startColor = gradient->stops[k].color;
                        endColor = gradient->stops[k + 1].color;
                        startPos = gradient->stops[k].position;
                        endPos = gradient->stops[k + 1].position;
                        break;
                    }
                }

                float localFactor = (factor - startPos) / (endPos - startPos);
                ColorRGBA interpColor = {
                    .r = (uint8_t)((1 - localFactor) * startColor.r + localFactor * endColor.r),
                    .g = (uint8_t)((1 - localFactor) * startColor.g + localFactor * endColor.g),
                    .b = (uint8_t)((1 - localFactor) * startColor.b + localFactor * endColor.b),
                    .a = (uint8_t)((1 - localFactor) * startColor.a + localFactor * endColor.a) * alphaFactor
                };

                float pixelX = (float)(x + j);
                float pixelY = (float)(y + i);

                if (transform) {
                    TransformPoint(&pixelX, &pixelY, transform);
                }

                int indexX = (int)pixelX;
                int indexY = (int)pixelY;

                if (indexX >= 0 && indexX < framebuffer->displayWidth && indexY >= 0 && indexY < framebuffer->displayHeight) {
                    switch (framebuffer->colorMode) {
                        case COLOR_MODE_TWO:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (indexY * framebuffer->displayWidth + indexX) / 8;
                            int bitIndex = 7 - ((indexY * framebuffer->displayWidth + indexX) % 8);
                            if (interpColor.r == framebuffer->colors[1].r && interpColor.g == framebuffer->colors[1].g && interpColor.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex); // Set bit for color 1
                            } else {
                                *pixelByte &= ~(1 << bitIndex); // Clear bit for color 0
                            }
                            break;
                        }
                        case COLOR_MODE_SIXTEEN:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (indexY * framebuffer->displayWidth + indexX) / 2;
                            int nibbleShift = ((indexY * framebuffer->displayWidth + indexX) % 2) ? 0 : 4;
                            uint8_t colorIndex = (interpColor.r >> 4) & 0xF; // Simplified mapping
                            *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                            break;
                        }
                        case COLOR_MODE_256:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + indexY * framebuffer->displayWidth + indexX;
                            *pixelByte = (interpColor.r >> 6) | ((interpColor.g >> 6) << 2) | ((interpColor.b >> 6) << 4);
                            break;
                        }
                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[indexY * framebuffer->displayWidth + indexX];
                            blendPixelWithAlpha(fbColor, interpColor, antiAlias ? alphaFactor : 1.0f);
                            break;
                        }
                    }
                }
            }
        }
    }
}*/


// Assuming write_pixel_rgb888_alpha helper (or its logic) is available.

/*void fillRoundedRectangleWithGradient(Framebuffer *framebuffer, int x, int y, int width, int height, const Gradient *gradient, int radius, const Matrix3x3 *transform, bool antiAlias) {
    // Ensure radius does not exceed half of the smallest dimension
    int maxRadius = (width < height ? width : height) / 2;
    radius = (radius > maxRadius) ? maxRadius : radius; // Adjust radius if needed

    // Calculate gradient direction vectors from the angle
    float dirX = cosf(gradient->angle);
    float dirY = sinf(gradient->angle);
    
    // Determine the Anti-Alias offset (0.5f if AA is on, 0.0f if off)
    const float aa_offset = antiAlias ? 0.5f : 0.0f;
    const int effectiveRadius = radius; // Base radius for non-AA checks

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            
            bool withinEllipse = false;
            float alphaFactor = 1.0f; // Calculated factor from 0.0 to 1.0 (geometry only)

            // --- Corner Geometry Calculation (Simplified) ---

            // Identify if the pixel is in a corner region (using effectiveRadius)
            bool is_in_corner_quadrant = (i < effectiveRadius || i >= height - effectiveRadius) &&
                                         (j < effectiveRadius || j >= width - effectiveRadius);

            if (is_in_corner_quadrant) {
                
                float cx = (j < effectiveRadius) ? effectiveRadius : (float)(width - effectiveRadius);
                float cy = (i < effectiveRadius) ? effectiveRadius : (float)(height - effectiveRadius);

                // Calculate distance from the curve center to the pixel center
                float dx = j - cx + aa_offset;
                float dy = i - cy + aa_offset;
                float distance = sqrtf(dx * dx + dy * dy);
                
                if (distance <= effectiveRadius) {
                    withinEllipse = true;
                    // Apply blending only if antiAlias is true
                    if (antiAlias) {
                        alphaFactor = fminf(1.0f, effectiveRadius - distance);
                    }
                } else {
                    alphaFactor = 0.0f; // Outside the entire shape
                }
            }
            
            // --- Center Area Check ---
            if (alphaFactor > 0.0f && (withinEllipse || (i >= effectiveRadius && i < height - effectiveRadius) || (j >= effectiveRadius && j < width - effectiveRadius))) {

                // --- 1. Gradient Color Interpolation ---
                
                // Use relative position to interpolate color based on gradient stops
                float factor = ((float)j * dirX + (float)i * dirY) / (float)width;
                ColorRGBA startColor, endColor;
                float startPos = 0.0f, endPos = 1.0f;
                
                // Clamp factor to 0.0-1.0 range
                if (factor < 0.0f) factor = 0.0f;
                if (factor > 1.0f) factor = 1.0f;

                // Determine correct color stops
                for (int k = 0; k < gradient->numStops - 1; k++) {
                    if (factor >= gradient->stops[k].position && factor <= gradient->stops[k + 1].position) {
                        startColor = gradient->stops[k].color;
                        endColor = gradient->stops[k + 1].color;
                        startPos = gradient->stops[k].position;
                        endPos = gradient->stops[k + 1].position;
                        break;
                    }
                }
                
                // --- In fillRoundedRectangleWithGradient ---
                // ... (just before the switch statement) ...

                                float localFactor = (factor - startPos) / (endPos - startPos);
                                ColorRGBA interpColor; // Declare uninitialized for simplicity

                                // 1. Interpolate R, G, B, and the starting gradient Alpha
                                interpColor.r = (uint8_t)((1 - localFactor) * startColor.r + localFactor * endColor.r);
                                interpColor.g = (uint8_t)((1 - localFactor) * startColor.g + localFactor * endColor.g);
                                interpColor.b = (uint8_t)((1 - localFactor) * startColor.b + localFactor * endColor.b);
                                
                                // Calculate the interpolated gradient alpha value
                                float interp_alpha_float = (1 - localFactor) * (float)startColor.a + localFactor * (float)endColor.a;

                                // 2. Apply the geometric AA factor to the interpolated alpha (FIX FOR LINE 1654)
                                interpColor.a = (uint8_t)(interp_alpha_float * alphaFactor);
                                
                                // Note: The previous buggy line (interpColor.a = (uint8_t)(((float)interpColor.a / 255.0f) * alphaFactor * 255.0f);)
                                // was trying to do this step by relying on the uninitialized value.


                // --- 2. Coordinate Transformation ---
                float pixelX = (float)(x + j);
                float pixelY = (float)(y + i);

                if (transform) {
                    TransformPoint(&pixelX, &pixelY, transform);
                }

                int indexX = (int)pixelX;
                int indexY = (int)pixelY;

                // --- 3. Final Framebuffer Write ---
                if (indexX >= 0 && indexX < framebuffer->displayWidth && indexY >= 0 && indexY < framebuffer->displayHeight) {
                    
                    switch (framebuffer->colorMode) {
                        // ... (existing cases: COLOR_MODE_TWO, COLOR_MODE_SIXTEEN, COLOR_MODE_256, COLOR_MODE_RGBA) ...

                        // --- NEW 18-BIT COLOR MODE (RGB888) ---
                        case COLOR_MODE_BGR888:
                        {
                            // Use the optimized integer blending function
                            write_pixel_rgb888_alpha(framebuffer, indexX, indexY, interpColor);
                            break;
                        }
                        
                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[indexY * framebuffer->displayWidth + indexX];
                            // Note: interpColor.a now holds the FINAL combined alpha value.
                            blendPixel(fbColor, interpColor);
                            break;
                        }
                    }
                }
            }
        }
    }
}*/

// NOTE: This requires the 'write_pixel_rgb888_alpha' helper function to be defined above it.

void fillRoundedRectangleWithGradient(Framebuffer *framebuffer, int x, int y, int width, int height, const Gradient *gradient, int radius, const Matrix3x3 *transform, bool antiAlias) {
    fillRoundedRectangleWithGradientExtended(framebuffer, x, y, width, height, 0, 0, width, height, gradient, radius, transform, antiAlias);
}

void fillRoundedRectangleWithGradientExtended(Framebuffer *framebuffer, int x, int y, int width, int height, int clipX, int clipY, int clipW, int clipH, const Gradient *gradient, int radius, const Matrix3x3 *transform, bool antiAlias) {
    
    // 1. Geometry Setup: Clamp radius to half the smallest dimension
    int maxRadius = (width < height ? width : height) / 2;
    radius = (radius > maxRadius) ? maxRadius : radius;
    
    // Pre-calculate constants for AA and Gradient math
    const float aa_offset = antiAlias ? 0.5f : 0.0f;
    const int effectiveRadius = radius;

    // Pre-calculate Gradient Geometry
    float dirX = 0, dirY = 0;
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float maxDist = 0;

    if (gradient->type == GRADIENT_TYPE_LINEAR) {
        dirX = cosf(gradient->angle);
        dirY = sinf(gradient->angle);
    }
    else if (gradient->type == GRADIENT_TYPE_RADIAL) {
        // Max distance is center to corner
        maxDist = sqrtf(centerX * centerX + centerY * centerY);
    }
    else if (gradient->type == GRADIENT_TYPE_BOX) {
        // OLD BROKEN CODE:
        // maxDist = (float)radius;

        // NEW FIXED CODE:
        // If radius is 0, we default to a standard "blur" size (e.g., 10px)
        // or simply clamp to 1.0f to prevent division by zero.
        // Ideally, this value should come from 'shadowRadius', but we don't have that param here.
        // So we clamp it.
        maxDist = (radius > 0) ? (float)radius : 1.0f;
    }

    int start_j = (clipX > x) ? (clipX - x) : 0;
        int start_i = (clipY > y) ? (clipY - y) : 0;
        
        // End loop: Min(width, clipEnd - shapeStart)
        int end_j = ((clipX + clipW) < (x + width)) ? ((clipX + clipW) - x) : width;
        int end_i = ((clipY + clipH) < (y + height)) ? ((clipY + clipH) - y) : height;

        // 2. Loop only the valid area
        for (int i = start_i; i < end_i; i++) {
            for (int j = start_j; j < end_j; j++) {
            
            // --- 2. Geometric Alpha Calculation (Shape Clipping) ---
            float alphaFactor = 1.0f; // Default to opaque
            
            // Check if we are in a corner region
            bool is_in_corner_quadrant = (i < effectiveRadius || i >= height - effectiveRadius) &&
                                         (j < effectiveRadius || j >= width - effectiveRadius);

            if (is_in_corner_quadrant) {
                float cx = (j < effectiveRadius) ? effectiveRadius : (float)(width - effectiveRadius);
                float cy = (i < effectiveRadius) ? effectiveRadius : (float)(height - effectiveRadius);

                float dx = j - cx + aa_offset;
                float dy = i - cy + aa_offset;
                float distance = sqrtf(dx * dx + dy * dy);
                
                if (distance > effectiveRadius) {
                    alphaFactor = 0.0f; // Pixel is outside the rounded shape
                } else if (antiAlias) {
                    // Smooth edge blend
                    alphaFactor = fminf(1.0f, effectiveRadius - distance);
                }
            }
            
            // Only perform expensive gradient math if the pixel is visible
            if (alphaFactor > 0.0f) {

                // --- 3. Gradient Factor Calculation ---
                float factor = 0.0f;

                switch (gradient->type) {
                    case GRADIENT_TYPE_LINEAR:
                        // Project position onto angle vector
                        factor = ((float)j * dirX + (float)i * dirY) / (float)width;
                        break;

                    case GRADIENT_TYPE_RADIAL:
                    {
                        // Distance from center / Max Distance
                        float dx = (float)j - centerX;
                        float dy = (float)i - centerY;
                        float dist = sqrtf(dx * dx + dy * dy);
                        factor = dist / maxDist;
                        break;
                    }

                    case GRADIENT_TYPE_BOX:
                    {
                        // Signed Distance Field for Box Gradient
                        float dx = fabsf((float)j - centerX);
                        float dy = fabsf((float)i - centerY);

                        // Subtract the straight parts (inner box dimensions)
                        float inner_w = centerX - radius;
                        float inner_h = centerY - radius;

                        float d_ax = (dx > inner_w) ? (dx - inner_w) : 0.0f;
                        float d_ay = (dy > inner_h) ? (dy - inner_h) : 0.0f;

                        // Calculate distance from inner box edge
                        float dist_from_inner = sqrtf(d_ax * d_ax + d_ay * d_ay);

                        // Normalize: 0.0 at inner rect edge, 1.0 at outer border
                        factor = dist_from_inner / maxDist;
                        break;
                    }
                }

                // Clamp factor to valid range [0.0, 1.0]
                if (factor < 0.0f) factor = 0.0f;
                if (factor > 1.0f) factor = 1.0f;

                // --- 4. Color Interpolation ---
                ColorRGBA startColor = gradient->stops[0].color;
                ColorRGBA endColor = gradient->stops[gradient->numStops - 1].color;
                float startPos = 0.0f;
                float endPos = 1.0f;

                // Find the two stops surrounding the current factor
                for (int k = 0; k < gradient->numStops - 1; k++) {
                    if (factor >= gradient->stops[k].position && factor <= gradient->stops[k + 1].position) {
                        startColor = gradient->stops[k].color;
                        endColor = gradient->stops[k + 1].color;
                        startPos = gradient->stops[k].position;
                        endPos = gradient->stops[k + 1].position;
                        break;
                    }
                }
                
                float localFactor = (factor - startPos) / (endPos - startPos);
                
                ColorRGBA interpColor;
                // Interpolate RGB channels
                interpColor.r = (uint8_t)((1 - localFactor) * startColor.r + localFactor * endColor.r);
                interpColor.g = (uint8_t)((1 - localFactor) * startColor.g + localFactor * endColor.g);
                interpColor.b = (uint8_t)((1 - localFactor) * startColor.b + localFactor * endColor.b);
                
                // Calculate Gradient Alpha first (to avoid uninitialized read warning)
                float gradientAlpha = (1 - localFactor) * (float)startColor.a + localFactor * (float)endColor.a;
                
                // Combine Gradient Alpha with Geometric AA Alpha
                interpColor.a = (uint8_t)(gradientAlpha * alphaFactor);


                // --- 5. Coordinate Transformation ---
                float pixelX = (float)(x + j);
                float pixelY = (float)(y + i);

                if (transform) {
                    TransformPoint(&pixelX, &pixelY, transform);
                }

                int indexX = (int)pixelX;
                int indexY = (int)pixelY;
                
                if (indexX < clipX || indexX >= clipX + clipW ||
                                indexY < clipY || indexY >= clipY + clipH) {
                                continue;
                            }

                // --- 6. Final Draw to Framebuffer ---
                if (indexX >= 0 && indexX < framebuffer->displayWidth && indexY >= 0 && indexY < framebuffer->displayHeight) {
                    
                    switch (framebuffer->colorMode) {
                        // --- NEW 18-BIT COLOR MODE (RGB888) ---
                        case COLOR_MODE_BGR888:
                        {
                            write_pixel_rgb888_alpha(framebuffer, indexX, indexY, interpColor);
                            break;
                        }
                        
                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[indexY * framebuffer->displayWidth + indexX];
                            blendPixel(fbColor, interpColor);
                            break;
                        }
                        // (Low color modes TWO/SIXTEEN/256 omitted for brevity, add if needed)
                    }
                }
            }
        }
    }
}

void renderText(Framebuffer *framebuffer, FT_Face face, const char *text, int startX, int startY, ColorRGBA textColor, int fontSize, const Gradient *gradient) {
    FT_Set_Pixel_Sizes(face, 0, fontSize);
    int x = startX;
    int y = startY;

    for (const char *p = text; *p; p++) {
        if (FT_Load_Char(face, *p, FT_LOAD_RENDER)) {
            fprintf(stderr, "Failed to load character '%c'\n", *p);
            continue;
        }

        FT_Bitmap *bitmap = &face->glyph->bitmap;
        if (bitmap->width > 0 && bitmap->rows > 0) {
            int x0 = x + face->glyph->bitmap_left;
            int y0 = y - face->glyph->bitmap_top;

            drawGlyph(framebuffer, bitmap, x0, y0, textColor);
        } else {
            fprintf(stderr, "Empty bitmap for character '%c'\n", *p);
        }

        x += (face->glyph->advance.x >> 6);
    }
}

// Note: Assumes FT_Face face is available and drawGlyph() is defined.

// Note: This is the complete function implementation for your CPUGraphics.c file.

// Helper to calculate total text height before drawing
// Helper to calculate total text height before drawing
// UPDATED: Now matches the 'p--' backtracking logic of the renderer exactly.
int measureTextHeight(FT_Face face, const char *text, int clipWidth, int fontSize, const TextFormat *format) {
    
    FT_Set_Pixel_Sizes(face, 0, fontSize);
    
    int line_height = (face->size->metrics.height >> 6) + format->lineSpacing;
    if (line_height <= 0) line_height = fontSize + 4;

    // Start with 0 height. If there is text, we add at least one line_height.
    if (text == NULL || text[0] == '\0') return 0;

    int current_height = line_height;
    
    const char *line_start = text;
    const char *p_word_start = text;
    int current_line_width = 0;
    int current_word_width = 0;

    for (const char *p = text; *p != '\0'; p++) {
        
        // 1. Measure character
        if (FT_Load_Char(face, *p, FT_LOAD_NO_BITMAP)) continue;
        int char_w = (face->glyph->advance.x >> 6) + format->glyphSpacing;
        
        current_word_width += char_w;

        // 2. Check Triggers
        bool overflow = (current_line_width + current_word_width > clipWidth);
        bool is_newline = (*p == '\n');

        if (is_newline || overflow || *(p+1) == '\0') {
            
            bool perform_wrap = false;

            if (is_newline) {
                perform_wrap = true;
            }
            else if (overflow) {
                if (format->wrapMode == TEXT_WRAP_MODE_TRUNCATE) {
                     // Truncate doesn't add height, it stops.
                     // But for measurement, we assume it takes up this line.
                     break;
                }
                else {
                    // WHOLE WORD WRAP LOGIC
                    if (current_line_width == 0) {
                        perform_wrap = true; // Huge word, force wrap
                    } else {
                        // BACKTRACK LOGIC (Must match renderer)
                        p = p_word_start - 1;
                        perform_wrap = true;
                    }
                }
            }

            if (perform_wrap || is_newline) {
                // Add height for the new line
                current_height += line_height;
                
                // Reset State
                if (overflow && format->wrapMode == TEXT_WRAP_MODE_WHOLE_WORD && current_line_width > 0) {
                    line_start = p_word_start;
                    current_line_width = 0;
                    current_word_width = 0;
                } else {
                    line_start = p + 1;
                    p_word_start = p + 1;
                    current_line_width = 0;
                    current_word_width = 0;
                }
            }
            continue;
        }

        if (*p == ' ') {
            current_line_width += current_word_width;
            current_word_width = 0;
            p_word_start = p + 1;
        }
    }
    
    return current_height;
}

void renderTextBox(Framebuffer *framebuffer, FT_Face face, const char *text, int x, int y, int clipWidth, int clipHeight, ColorRGBA textColor, int fontSize, const TextFormat *format) {
    // Call the extended version with default scroll/clip parameters
    renderTextBoxExtended(
        framebuffer,
        face,
        text,
        x, y,
        clipWidth, clipHeight,
        x, y, clipWidth, clipHeight, // Hard Clip == Text Box itself
        0,                           // Scroll Y = 0
        textColor,
        fontSize,
        format
    );
}

/*void renderTextBoxExtended(Framebuffer *framebuffer, FT_Face face, const char *text, int x, int y, int clipWidth, int clipHeight, int hardClipX, int hardClipY, int hardClipW, int hardClipH, int scrollY, ColorRGBA textColor, int fontSize, const TextFormat *format) {
    
    printf("renderTextBox");
    FT_Set_Pixel_Sizes(face, 0, fontSize);
    
    // 1. Calculate Line Metrics
    int line_height = (face->size->metrics.height >> 6) + format->lineSpacing;
    if (line_height <= 0) { line_height = fontSize + 4; }

    // 2. Calculate Ascender (Distance from Baseline to Top)
    // This is needed to convert "Top-Left" coordinates to "Baseline" coordinates.
    int ascender = face->size->metrics.ascender >> 6;
    if (ascender <= 0) ascender = fontSize; // Fallback estimate if metrics missing

    // 3. Vertical Alignment Logic
    // 'startY' will represent the TOP-LEFT Y coordinate where the text block begins.
    int startY = y;

    if (format->valignment != TEXT_VALIGN_TOP) {
        int totalTextHeight = measureTextHeight(face, text, clipWidth, fontSize, format);
        
        // Clamp height to clip area to prevent alignment weirdness if text is huge
        if (totalTextHeight > clipHeight) totalTextHeight = clipHeight;

        if (format->valignment == TEXT_VALIGN_CENTER) {
            // (Container - Content) / 2
            startY += (clipHeight - totalTextHeight) / 2;
        }
        else if (format->valignment == TEXT_VALIGN_BOTTOM) {
            // Container - Content
            startY += (clipHeight - totalTextHeight);
        }
    }
    
    // Safety: Don't start above the clip box
    if (startY < y) startY = y;

    // 4. Set Drawing Cursor
    int cursorX = x;
    
    // CRITICAL FIX: Add 'ascender' to convert Top-Left Y to Baseline Y
    int cursorY = startY + ascender - scrollY;

    
    // 4. Define Scissor Rect (Hard Clip)
    int scissorTop = hardClipY;
    int scissorBottom = hardClipY + hardClipH;
    int scissorLeft = hardClipX;
    int scissorRight = hardClipX + hardClipW;
    
    
    // ... (The rest of variables: line_start, p_word_start...)
    const char *line_start = text;
    const char *p_word_start = text;
    int current_line_width = 0;
    int current_word_width = 0;

    // Outer loop iterates through characters
    for (const char *p = text; *p != '\0'; p++) {
        
        // --- 1. Load Metrics for Current Character ---
        // FT_Load_NO_BITMAP is used here to get width without rendering
        if (FT_Load_Char(face, *p, FT_LOAD_NO_BITMAP)) {
            continue;
        }
        
        // Calculate the total width this character will advance the cursor
        int char_advance_total = (face->glyph->advance.x >> 6) + format->glyphSpacing;
        current_word_width += char_advance_total;


        // --- 2. Check for Line Break Triggers ---
        
        // A segment is drawn if:
        //   a) Explicit Newline ('\n')
        //   b) End of String ('\0')
        //   c) The next word won't fit (Word Wrap)
        //   d) The current line is too wide (Truncation)
        
        // The condition for a forced break: the next position will exceed the clipWidth.
        bool force_break = (current_line_width + current_word_width > clipWidth);
        
        if (*p == '\n' || *(p + 1) == '\0' || force_break) {
            
            // --- A. Identify the Segment to Draw ---
            // Inside the renderTextBox function body:

            const char *draw_segment_start = NULL; // Initialized to NULL
            int draw_segment_length = 0;             // Initialized to 0

            if (*p == '\n' || *(p + 1) == '\0') {
                // EXPLICIT BREAK: Draw everything up to here.
                draw_segment_start = line_start;
                draw_segment_length = (*p == '\n') ? (p - line_start) : (p - line_start + 1);

            } else if (force_break) {
                // FORCED WRAP: Handle based on wrap mode
                
                if (format->wrapMode == TEXT_WRAP_MODE_TRUNCATE) {
                    // TRUNCATE: Draw the previous segment and stop the entire function.
                    draw_segment_start = line_start;
                    draw_segment_length = p - line_start;
                    p += draw_segment_length; // Force 'p' past the end to trigger break
                    break;
                }
                
                // WORD_BREAK: We draw the segment *before* the word that didn't fit.
                draw_segment_start = line_start;
                draw_segment_length = p_word_start - line_start;

                // If the word is longer than the clip width, we must draw char-by-char up to the boundary.
                if (draw_segment_length == 0) {
                    // This means the first word itself is wider than clipWidth. Truncate.
                    draw_segment_length = p - line_start;
                    p += draw_segment_length;
                    break;
                }

                // Now that the segment is defined, we must backtrack the main loop pointer
                // so it can restart drawing the word that didn't fit on the new line.
                p = p_word_start;
                p--; // 🚨 CRITICAL FIX: Decrement pointer to counteract the 'for' loop's automatic p++
            }


            // --- B. Measurement (Pass 1: Final Line Width for Alignment) ---
            int final_segment_width = 0;
            for (int k = 0; k < draw_segment_length; k++) {
                FT_Load_Char(face, draw_segment_start[k], FT_LOAD_NO_BITMAP);
                final_segment_width += (face->glyph->advance.x >> 6) + format->glyphSpacing;
            }
            
            // --- C. Alignment Offset Calculation ---
            int offset_x = 0;
            if (format->alignment == TEXT_ALIGN_CENTER) {
                offset_x = (clipWidth - final_segment_width) / 2;
            } else if (format->alignment == TEXT_ALIGN_RIGHT) {
                offset_x = clipWidth - final_segment_width;
            }
            
            // --- D. Drawing Loop (Pass 2: Draw Glyphs with Offset) ---
            cursorX = x + offset_x; // Set drawing X to aligned position
            
            bool isLineVisible = (cursorY >= scissorTop - line_height && (cursorY - fontSize) <= scissorBottom);

            if (isLineVisible) {
                for (int k = 0; k < draw_segment_length; k++) {
                    if (FT_Load_Char(face, draw_segment_start[k], FT_LOAD_RENDER)) { continue; }
                    
                    int drawX = cursorX + face->glyph->bitmap_left;
                    int drawY = cursorY - face->glyph->bitmap_top;

                    // STRICT SCISSOR CHECK: Only draw if glyph is inside the Hard Clip area
                    if (drawX >= scissorLeft && drawX < scissorRight &&
                        drawY >= scissorTop && drawY < scissorBottom)
                    {
                        drawGlyph(framebuffer, &face->glyph->bitmap, drawX, drawY, textColor);
                    }
                    
                    cursorX += (face->glyph->advance.x >> 6) + format->glyphSpacing;
                }
            }
            
            for (int k = 0; k < draw_segment_length; k++) {
                // Re-load with render flag to generate the actual bitmap
                if (FT_Load_Char(face, draw_segment_start[k], FT_LOAD_RENDER)) { continue; }
                
                // Draw only if within the vertical clipping area
                if (cursorY < y + clipHeight && cursorY >= y) {
                    drawGlyph(framebuffer,
                              &face->glyph->bitmap,
                              cursorX + face->glyph->bitmap_left,
                              cursorY - face->glyph->bitmap_top,
                              textColor);
                }
                
                // Advance cursor: Base advance + manual spacing
                cursorX += (face->glyph->advance.x >> 6) + format->glyphSpacing;
            }
            
            // --- E. Advance to the Next Line (Cleanup) ---
            cursorX = x;
            cursorY += line_height;
            line_start = p + 1; // Start of the next line (or word)
            p_word_start = p + 1;
            current_line_width = 0;
            current_word_width = 0;

            if (cursorY > scissorBottom + line_height) break;
            
            // If we backtracked (CMD_WRAP_WHOLE_WORD), we continue the outer loop
            // to process the character the pointer was set back to.
            if (force_break) {
                continue;
            }
        }


        // --- 3. Update Word and Line Trackers (If no break occurred) ---
        if (*p == ' ') {
            current_line_width += current_word_width;
            current_word_width = 0;
            p_word_start = p + 1;
        }
        
        // This is a normal advance, just tracking the width.
        // It's the same logic as the measurement in the forced_break block.
        // current_line_width is updated during the space check,
        // current_word_width is updated by the char_advance_total.
    }
}*/

void renderTextBoxExtended(Framebuffer *framebuffer, FT_Face face, const char *text, int x, int y, int clipWidth, int clipHeight, int hardClipX, int hardClipY, int hardClipW, int hardClipH, int scrollY, ColorRGBA textColor, int fontSize, const TextFormat *format) {
    
    FT_Set_Pixel_Sizes(face, 0, fontSize);
    
    int line_height = (face->size->metrics.height >> 6) + format->lineSpacing;
    if (line_height <= 0) { line_height = fontSize + 4; }
    
    int ascender = face->size->metrics.ascender >> 6;
    if (ascender <= 0) ascender = fontSize;

    // 3. Vertical Alignment Logic
    // 'startY' will represent the TOP-LEFT Y coordinate where the text block begins.
    int startY = y;

    if (format->valignment != TEXT_VALIGN_TOP) {
        int totalTextHeight = measureTextHeight(face, text, clipWidth, fontSize, format);
        
        // Clamp height to clip area to prevent alignment weirdness if text is huge
        if (totalTextHeight > clipHeight) totalTextHeight = clipHeight;

        if (format->valignment == TEXT_VALIGN_CENTER) {
            // (Container - Content) / 2
            startY += (clipHeight - totalTextHeight) / 2;
        }
        else if (format->valignment == TEXT_VALIGN_BOTTOM) {
            // Container - Content
            startY += (clipHeight - totalTextHeight);
        }
    }
    
    // Safety: Don't start above the clip box
    if (startY < y) startY = y;

    // 4. Set Drawing Cursor
    int cursorX = x;
    
    // CRITICAL FIX: Add 'ascender' to convert Top-Left Y to Baseline Y
    int cursorY = startY + ascender - scrollY;
    
    // Scissor Rect (Hard Clip)
    int scissorTop = hardClipY;
    int scissorBottom = hardClipY + hardClipH;
    int scissorLeft = hardClipX;
    int scissorRight = hardClipX + hardClipW;

    // State Tracking
    const char *line_start = text;
    const char *p_word_start = text; // Start of the current word being measured
    
    int current_line_width = 0; // Width of "committed" words on this line
    int current_word_width = 0; // Width of the current word being built
    
    bool is_newline = false;

    for (const char *p = text; *p != '\0'; p++) {
        
        is_newline = (*p == '\n');

        // 1. Measure Character
        if (FT_Load_Char(face, *p, FT_LOAD_NO_BITMAP)) continue;
        int char_w = (face->glyph->advance.x >> 6) + format->glyphSpacing;
        
        // 2. Update current word width
        current_word_width += char_w;

        // 3. Check for Overflow
        // Calculate total width if we were to print this word right now
        int potential_total_width = current_line_width + current_word_width;
        bool overflow = (potential_total_width > clipWidth);

        // 4. Trigger Draw if: Newline, Overflow, or End of String
        if (is_newline || overflow || *(p+1) == '\0') {
            
            const char* draw_end = p; // Default: draw up to current char
            bool perform_wrap = false;

            if (is_newline) {
                // Draw up to newline (exclusive)
                draw_end = p;
                perform_wrap = true;
            }
            else if (overflow) {
                // WRAPPING LOGIC
                if (format->wrapMode == TEXT_WRAP_MODE_TRUNCATE) {
                    draw_end = p; // Draw what fits
                    // Skip forward until next space or newline to avoid drawing the rest of cut word
                    while (*p != ' ' && *p != '\n' && *p != '\0') p++;
                    p--; // Back up one so the loop increment works
                    perform_wrap = true;
                }
                else {
                    // WHOLE WORD WRAP
                    // If this is the very first word and it doesn't fit, we MUST truncate/split it
                    if (current_line_width == 0) {
                        draw_end = p; // Draw what fits of the huge word
                        perform_wrap = true;
                    } else {
                        // Normal Case: Push the WHOLE current word to the next line.
                        // Draw from line_start up to p_word_start (exclusive)
                        draw_end = p_word_start;
                        
                        // 🚨 BACKTRACK 🚨
                        // Reset 'p' to start of word - 1.
                        // The loop will do p++, landing us at the start of the word on the new line.
                        p = p_word_start - 1;
                        perform_wrap = true;
                    }
                }
            }
            else if (*(p+1) == '\0') {
                // End of string: Draw everything including current char
                draw_end = p + 1;
            }

            // --- DRAWING PHASE ---
            int draw_len = draw_end - line_start;
            
            if (draw_len > 0) {
                // Optimization: Is line visible?
                bool isLineVisible = (cursorY >= scissorTop - line_height && (cursorY - fontSize) <= scissorBottom);

                if (isLineVisible) {
                    // Calculate Alignment Offset
                    int final_width = 0;
                    for (int k=0; k<draw_len; k++) {
                        if(FT_Load_Char(face, line_start[k], FT_LOAD_NO_BITMAP)) continue;
                        final_width += (face->glyph->advance.x >> 6) + format->glyphSpacing;
                    }
                    
                    int align_x = x;
                    if (format->alignment == TEXT_ALIGN_CENTER) align_x += (clipWidth - final_width) / 2;
                    else if (format->alignment == TEXT_ALIGN_RIGHT) align_x += (clipWidth - final_width);
                    
                    // Render Glyphs
                    int tempX = align_x;
                    for (int k=0; k<draw_len; k++) {
                         if (!FT_Load_Char(face, line_start[k], FT_LOAD_RENDER)) {
                            int drawX = tempX + face->glyph->bitmap_left;
                            int drawY = cursorY - face->glyph->bitmap_top;
                            
                            // Hard Clip Check
                            if (drawX >= scissorLeft && drawX < scissorRight &&
                                drawY >= scissorTop && drawY < scissorBottom) {
                                drawGlyph(framebuffer, &face->glyph->bitmap, drawX, drawY, textColor);
                            }
                         }
                         tempX += (face->glyph->advance.x >> 6) + format->glyphSpacing;
                    }
                }
            }

            // --- RESET PHASE ---
            if (perform_wrap || is_newline) {
                cursorX = x;
                cursorY += line_height;
                
                // If we backtracked, the next line starts at the word start.
                // If it was a newline, it starts after the newline.
                if (overflow && format->wrapMode == TEXT_WRAP_MODE_WHOLE_WORD && current_line_width > 0) {
                    line_start = p_word_start;
                    // We are starting a new line with the current word, so width is 0
                    // (The loop will re-measure this word on the next pass)
                    current_line_width = 0;
                    current_word_width = 0;
                } else {
                    // Normal Newline
                    line_start = p + 1;
                    p_word_start = p + 1;
                    current_line_width = 0;
                    current_word_width = 0;
                }

                if (cursorY > scissorBottom + line_height) break; // Stop rendering if off bottom
            }
            
            continue; // Loop again
        }

        // 5. Handle Space (Commit word)
        if (*p == ' ') {
            current_line_width += current_word_width;
            current_word_width = 0;
            p_word_start = p + 1; // Next word starts after this space
        }
    }
}


int measureTextHeightCached(FTC_Manager manager,
                            FTC_ImageCache image_cache,
                            FTC_CMapCache cmap_cache,
                            FTC_FaceID face_id,
                            const char *text,
                            int clipWidth,
                            int fontSize,
                            const TextFormat *format)
{
    if (text == NULL) return 0; // <--- ADD THIS LINE
    
    // 1. Setup Scaler
    FTC_ScalerRec scaler;
    scaler.face_id = face_id;
    scaler.width = 0;
    scaler.height = fontSize;
    scaler.pixel = 1;
    scaler.x_res = 0;
    scaler.y_res = 0;

    // 2. Get Metrics
    FT_Size size;
    if (FTC_Manager_LookupSize(manager, &scaler, &size) != 0) return fontSize;
    
    int line_height = (size->metrics.height >> 6) + format->lineSpacing;
    if (line_height <= 0) line_height = fontSize + 4;

    // 3. Simulation Loop
    int total_lines = 1;
    int current_line_width = 0;
    int current_word_width = 0;
    
    const char *p_word_start = text;
    bool is_newline = false;

    for (const char *p = text; *p != '\0'; p++) {
        is_newline = (*p == '\n');

        // Measure Char (Use Cache!)
        FT_UInt glyph_index = FTC_CMapCache_Lookup(cmap_cache, face_id, -1, *p);
        FT_Glyph glyph;
        if (FTC_ImageCache_LookupScaler(image_cache, &scaler, FT_LOAD_DEFAULT, glyph_index, &glyph, NULL) != 0) continue;
        
        int char_w = (glyph->advance.x >> 16) + format->glyphSpacing;
        current_word_width += char_w;

        // Check Overflow
        if (current_line_width + current_word_width > clipWidth || is_newline) {
            total_lines++;
            
            if (is_newline) {
                current_line_width = 0;
                current_word_width = 0;
            }
            else {
                // Wrap Logic
                if (format->wrapMode == TEXT_WRAP_MODE_WHOLE_WORD && current_line_width > 0) {
                    // Word wraps to next line
                    current_line_width = current_word_width; // New line starts with this word
                    current_word_width = 0;
                } else {
                    // Char wrap / Truncate
                    current_line_width = 0;
                    current_word_width = 0;
                }
            }
        }
        else if (*p == ' ') {
            current_line_width += current_word_width;
            current_word_width = 0;
        }
    }

    // Return exact pixel height
    return total_lines * line_height;
}

// Helper to draw the FT_Bitmap (Assuming you have this or similar)
// void drawGlyph(Framebuffer *fb, FT_Bitmap *bitmap, int x, int y, ColorRGBA color);

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
                                 const TextFormat *format)
{
    // 1. Setup Scaler (Replaces FT_Set_Pixel_Sizes)
    FTC_ScalerRec scaler;
    scaler.face_id = face_id;
    scaler.width = 0;
    scaler.height = fontSize;
    scaler.pixel = 1;
    scaler.x_res = 0;
    scaler.y_res = 0;

    // 2. Get Metrics (We need to lookup the face briefly to get global metrics)
    // ... inside renderTextBoxExtendedCached ...

    // 2. Get Metrics
    FT_Face face;
    FT_Size size; // <--- The Cache gives us this specific Size Object

    // Look up the specific size object for this font size
    if (FTC_Manager_LookupSize(manager, &scaler, &size) != 0) return;
    
    // Look up the face (just to keep the face alive/pinned if needed)
    if (FTC_Manager_LookupFace(manager, face_id, &face) != 0) return;

    // --- FIX: Use 'size->metrics', NOT 'face->size->metrics' ---
    
    // Calculate Line Height
    int line_height = (size->metrics.height >> 6) + format->lineSpacing;
    if (line_height <= 0) { line_height = fontSize + 4; }
    
    // Calculate Ascender (Distance from Baseline to Top)
    int ascender = size->metrics.ascender >> 6;
    if (ascender <= 0) ascender = fontSize;

    // ... continue with vertical alignment logic ...

    // 3. Vertical Alignment Logic
    int startY = y;

    if (format->valignment != TEXT_VALIGN_TOP) {
        
        // FIX: Calculate the Real Height
        int totalTextHeight = measureTextHeightCached(
            manager, image_cache, cmap_cache, face_id,
            text, clipWidth, fontSize, format
        );
        
        // Clamp height (optional, prevents negative math if text > box)
        if (totalTextHeight > clipHeight) totalTextHeight = clipHeight;

        if (format->valignment == TEXT_VALIGN_CENTER) {
            // (Container - Content) / 2
            // Now this math works because totalTextHeight is real (e.g., 24px)
            // Example: (100 - 24) / 2 = 38. Top starts at 38. Correct.
            startY += (clipHeight - totalTextHeight) / 2;
        }
        else if (format->valignment == TEXT_VALIGN_BOTTOM) {
            startY += (clipHeight - totalTextHeight);
        }
    }

    if (startY < y) startY = y;

    // 4. Set Drawing Cursor
    int cursorX = x;
    int cursorY = startY + ascender - scrollY;
    
    // Scissor Rect
    int scissorTop = hardClipY;
    int scissorBottom = hardClipY + hardClipH;
    int scissorLeft = hardClipX;
    int scissorRight = hardClipX + hardClipW;

    // State Tracking
    const char *line_start = text;
    const char *p_word_start = text;
    
    int current_line_width = 0;
    int current_word_width = 0;
    
    bool is_newline = false;

    // --- MAIN PARSING LOOP ---
    for (const char *p = text; *p != '\0'; p++) {
        
        is_newline = (*p == '\n');

        // 1. Measure Character using CACHE
        FT_UInt glyph_index = FTC_CMapCache_Lookup(cmap_cache, face_id, -1, *p);
        FT_Glyph glyph;
        
        // Lookup the glyph to get metrics (this is fast if cached)
        if (FTC_ImageCache_LookupScaler(image_cache, &scaler, FT_LOAD_RENDER, glyph_index, &glyph, NULL) != 0) continue;
        
        // Advance is stored in 16.16 format in the glyph object
        int char_w = (glyph->advance.x >> 16) + format->glyphSpacing;
        
        // 2. Update current word width
        current_word_width += char_w;

        // 3. Check for Overflow
        int potential_total_width = current_line_width + current_word_width;
        bool overflow = (potential_total_width > clipWidth);

        // 4. Trigger Draw
        if (is_newline || overflow || *(p+1) == '\0') {
            
            const char* draw_end = p;
            bool perform_wrap = false;

            if (is_newline) {
                draw_end = p;
                perform_wrap = true;
            }
            else if (overflow) {
                if (format->wrapMode == TEXT_WRAP_MODE_TRUNCATE) {
                    draw_end = p;
                    while (*p != ' ' && *p != '\n' && *p != '\0') p++;
                    p--;
                    perform_wrap = true;
                }
                else {
                    // Whole Word Wrap
                    if (current_line_width == 0) {
                        draw_end = p;
                        perform_wrap = true;
                    } else {
                        draw_end = p_word_start;
                        p = p_word_start - 1;
                        perform_wrap = true;
                    }
                }
            }
            else if (*(p+1) == '\0') {
                draw_end = p + 1;
            }

            // --- DRAWING PHASE (CACHED) ---
            int draw_len = draw_end - line_start;
            
            if (draw_len > 0) {
                bool isLineVisible = (cursorY >= scissorTop - line_height && (cursorY - fontSize) <= scissorBottom);

                if (isLineVisible) {
                    // Measure Final Width for Alignment
                    int final_width = 0;
                    for (int k=0; k<draw_len; k++) {
                        FT_UInt g_idx = FTC_CMapCache_Lookup(cmap_cache, face_id, -1, line_start[k]);
                        FT_Glyph g;
                        if(FTC_ImageCache_LookupScaler(image_cache, &scaler, FT_LOAD_RENDER, g_idx, &g, NULL) == 0) {
                             final_width += (g->advance.x >> 16) + format->glyphSpacing;
                        }
                    }
                    
                    int align_x = x;
                    if (format->alignment == TEXT_ALIGN_CENTER) align_x += (clipWidth - final_width) / 2;
                    else if (format->alignment == TEXT_ALIGN_RIGHT) align_x += (clipWidth - final_width);
                    
                    // Render Glyphs
                    int tempX = align_x;
                    for (int k=0; k<draw_len; k++) {
                        FT_UInt g_idx = FTC_CMapCache_Lookup(cmap_cache, face_id, -1, line_start[k]);
                        FT_Glyph g;
                        
                        if (FTC_ImageCache_LookupScaler(image_cache, &scaler, FT_LOAD_RENDER, g_idx, &g, NULL) == 0) {
                            
                            // Cast to Bitmap Glyph to get drawing coordinates
                            FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)g;
                            
                            int drawX = tempX + bitmap_glyph->left;
                            int drawY = cursorY - bitmap_glyph->top;
                            
                            // Hard Clip Check
                            if (drawX >= scissorLeft && drawX < scissorRight &&
                                drawY >= scissorTop && drawY < scissorBottom) {
                                
                                drawGlyph(framebuffer, &bitmap_glyph->bitmap, drawX, drawY, textColor);
                            }
                            
                            tempX += (g->advance.x >> 16) + format->glyphSpacing;
                        }
                    }
                }
            }

            // --- RESET PHASE ---
            if (perform_wrap || is_newline) {
                cursorX = x;
                cursorY += line_height;
                
                if (overflow && format->wrapMode == TEXT_WRAP_MODE_WHOLE_WORD && current_line_width > 0) {
                    line_start = p_word_start;
                    current_line_width = 0;
                    current_word_width = 0;
                } else {
                    line_start = p + 1;
                    p_word_start = p + 1;
                    current_line_width = 0;
                    current_word_width = 0;
                }

                if (cursorY > scissorBottom + line_height) break;
            }
            
            continue;
        }

        // 5. Handle Space
        if (*p == ' ') {
            current_line_width += current_word_width;
            current_word_width = 0;
            p_word_start = p + 1;
        }
    }
}

// Note: This function assumes the TextFormat struct is defined and passed in.

void renderTextBoxScroll(Framebuffer *framebuffer, FT_Face face, const char *text, int x, int y, int clipWidth, int clipHeight, int scrollY, ColorRGBA textColor, int fontSize, const TextFormat *format) {
    
    // 1. Setup Metrics (Identical to old function)
    FT_Set_Pixel_Sizes(face, 0, fontSize);
    
    int line_height = (face->size->metrics.height >> 6) + format->lineSpacing;
    if (line_height <= 0) { line_height = fontSize + 4; }

    int ascender = face->size->metrics.ascender >> 6;
    if (ascender <= 0) ascender = fontSize; // Baseline adjustment
    
    
    // 2. MEASUREMENT PASS (Calculates total height for alignment)
    int totalTextHeight = measureTextHeight(face, text, clipWidth, fontSize, format);

    
    // 3. CALCULATE ALIGNMENT OFFSET (Vertical Centering/Bottom)
    int offset_y = 0;
    
    // Only apply alignment offset if text is smaller than viewport
    if (totalTextHeight < clipHeight) {
        if (format->valignment == TEXT_VALIGN_CENTER) {
            // Center Alignment: (Container - Content) / 2
            offset_y = (clipHeight - totalTextHeight) / 2;
        } else if (format->valignment == TEXT_VALIGN_BOTTOM) {
            // Bottom Alignment: Container - Content
            offset_y = (clipHeight - totalTextHeight);
        }
    }
    
    // 4. APPLY FINAL BASELINE & SCROLL OFFSET
    
    // a) Calculate the Top-Left corner of the aligned text block:
    int topY = y + offset_y;
    
    // b) Apply the SCROLL offset (Moves content up when scrolling down)
    topY -= scrollY;
    
    // c) Convert to Baseline Y (The actual starting point for the drawing loop)
    int cursorY = topY + ascender;
    
    // Safety check: Don't start drawing lines above the top clip edge (y)
    if (cursorY < y + ascender) cursorY = y + ascender;


    // --- 5. CORE DRAWING LOOP START ---
    // The rest of the function body (the 'for' loops) should be identical
    // to your existing renderTextBox function, using the new cursorY.
    
    int cursorX = x;
    const char *line_start = text;
    const char *p_word_start = text;
    
    int current_line_width = 0;
    int current_word_width = 0;

    // Outer loop iterates through characters
    for (const char *p = text; *p != '\0'; p++) {
        
        // --- 1. Load Metrics for Current Character ---
        if (FT_Load_Char(face, *p, FT_LOAD_NO_BITMAP)) { continue; }
        
        int char_advance_total = (face->glyph->advance.x >> 6) + format->glyphSpacing;
        current_word_width += char_advance_total;

        // --- 2. Check for Line Break Triggers ---
        bool force_break = (current_line_width + current_word_width > clipWidth);
        
        if (*p == '\n' || *(p + 1) == '\0' || force_break) {
            
            // --- A. Identify the Segment to Draw ---
            // --- Fix: Initialize the variables to a safe state ---
            const char *draw_segment_start = NULL; // Initialized to NULL
            int draw_segment_length = 0;             // Initialized to 0
            
            if (*p == '\n' || *(p + 1) == '\0') {
                draw_segment_start = line_start;
                draw_segment_length = (*p == '\n') ? (p - line_start) : (p - line_start + 1);
            } else if (force_break) {
                
                if (format->wrapMode == TEXT_WRAP_MODE_TRUNCATE) {
                    draw_segment_length = p - line_start;
                    p += draw_segment_length;
                    break;
                }
                
                // WORD_BREAK mode: Draw the previous segment and backtrack
                draw_segment_start = line_start;
                draw_segment_length = p_word_start - line_start;

                if (draw_segment_length == 0) {
                    // This means the first word itself is wider than clipWidth. Truncate and stop.
                    draw_segment_length = p - line_start;
                    p += draw_segment_length;
                    break;
                }

                p = p_word_start;
                p--;
            }


            // --- B. Measurement (Pass 1: Final Line Width for Alignment) ---
            int final_segment_width = 0;
            for (int k = 0; k < draw_segment_length; k++) {
                FT_Load_Char(face, draw_segment_start[k], FT_LOAD_NO_BITMAP);
                final_segment_width += (face->glyph->advance.x >> 6) + format->glyphSpacing;
            }
            
            // --- C. Alignment Offset Calculation ---
            int offset_x = 0;
            if (format->alignment == TEXT_ALIGN_CENTER) {
                offset_x = (clipWidth - final_segment_width) / 2;
            } else if (format->alignment == TEXT_ALIGN_RIGHT) {
                offset_x = clipWidth - final_segment_width;
            }
            
            // --- D. Drawing Loop (Pass 2: Draw Glyphs with Offset and CLIPPING) ---
            cursorX = x + offset_x;
            
            for (int k = 0; k < draw_segment_length; k++) {
                if (FT_Load_Char(face, draw_segment_start[k], FT_LOAD_RENDER)) { continue; }
                
                // --- GLYPH CLIPPING CHECK ---
                // We only draw if the glyph is entirely or partially within the clip window
                // Vertical clip check:
                if (cursorY < y + clipHeight && cursorY + line_height > y) {
                    
                    int drawX = cursorX + face->glyph->bitmap_left;
                    int drawY = cursorY - face->glyph->bitmap_top;

                    // Horizontal clip check: Ensure glyph doesn't start or end outside the horizontal viewport
                    if (drawX + face->glyph->bitmap.width > x && drawX < x + clipWidth) {

                        // Call drawGlyph (which handles framebuffer boundary checks)
                        // Note: The drawGlyph function itself must now perform the clipX/Y/W/H check.
                        // For this high-level call, we trust drawGlyph to clip correctly.
                        drawGlyph(framebuffer,
                                  &face->glyph->bitmap,
                                  drawX,
                                  drawY,
                                  textColor);
                    }
                }
                
                cursorX += (face->glyph->advance.x >> 6) + format->glyphSpacing;
            }
            
            // --- E. Advance to the Next Line (Cleanup) ---
            cursorX = x;
            cursorY += line_height;
            line_start = p + 1;
            p_word_start = p + 1;
            current_line_width = 0;
            current_word_width = 0;

            if (cursorY > y + clipHeight) break;
            
            if (force_break) { continue; }
        }


        // --- 3. Update Word and Line Trackers (If no break occurred) ---
        if (*p == ' ') {
            current_line_width += current_word_width;
            current_word_width = 0;
            p_word_start = p + 1;
        }
    }
}

/*void renderTextBoxScroll(Framebuffer *framebuffer, FT_Face face, const char *text, int x, int y, int clipWidth, int clipHeight, int hardClipX, int hardClipY, int hardClipW, int hardClipH, int scrollY, ColorRGBA textColor, int fontSize, const TextFormat *format) {
    
    FT_Set_Pixel_Sizes(face, 0, fontSize);
    
    // 1. Calculate Line Height & Ascender
    int line_height = (face->size->metrics.height >> 6) + format->lineSpacing;
    if (line_height <= 0) line_height = fontSize + 4;
    
    int ascender = face->size->metrics.ascender >> 6;
    if (ascender <= 0) ascender = fontSize;

    // 2. Calculate Start Position (Baseline) adjusted by ScrollY
    int cursorX = x;
    int cursorY = (y + ascender) - scrollY;
    
    // 3. Define Hard Clip Boundaries (The Scissor Rect)
    int scissorTop = hardClipY;
    int scissorBottom = hardClipY + hardClipH;
    int scissorLeft = hardClipX;
    int scissorRight = hardClipX + hardClipW;

    // Pointers for wrapping logic
    const char *line_start = text;
    const char *p_word_start = text;
    int current_line_width = 0;
    int current_word_width = 0;

    for (const char *p = text; *p != '\0'; p++) {
        
        // --- Measure ---
        if (FT_Load_Char(face, *p, FT_LOAD_NO_BITMAP)) continue;
        int char_advance = (face->glyph->advance.x >> 6) + format->glyphSpacing;
        current_word_width += char_advance;

        // --- Check Line Break ---
        bool force_break = (current_line_width + current_word_width > clipWidth);
        
        if (*p == '\n' || *(p + 1) == '\0' || force_break) {
            
            const char *draw_start;
            int draw_len;

            // Determine segment (Explicit vs Wrap)
            if (*p == '\n' || *(p + 1) == '\0') {
                draw_start = line_start;
                draw_len = (*p == '\n') ? (p - line_start) : (p - line_start + 1);
            } else {
                if (format->wrapMode == TEXT_WRAP_MODE_WHOLE_WORD) {
                     draw_start = line_start;
                     draw_len = p_word_start - line_start;
                     if (draw_len == 0) { draw_len = p - line_start; p += draw_len; break; }
                     p = p_word_start; p--;
                } else {
                     draw_start = line_start;
                     draw_len = p - line_start;
                     p--;
                }
            }

            // --- DRAWING LOGIC ---
            
            // Optimization: Is this line vertically visible?
            // Check if the glyph's vertical span overlaps the scissor rect
            bool isLineVisible = (cursorY >= scissorTop - line_height && (cursorY - fontSize) <= scissorBottom);

            if (isLineVisible) {
                
                // Alignment Calculation
                int final_width = 0;
                for (int k=0; k<draw_len; k++) {
                    FT_Load_Char(face, draw_start[k], FT_LOAD_NO_BITMAP);
                    final_width += (face->glyph->advance.x >> 6) + format->glyphSpacing;
                }
                
                int align_x = x;
                if (format->alignment == TEXT_ALIGN_CENTER) align_x += (clipWidth - final_width) / 2;
                else if (format->alignment == TEXT_ALIGN_RIGHT) align_x += (clipWidth - final_width);

                int tempX = align_x;

                // Draw Glyphs
                for (int k = 0; k < draw_len; k++) {
                    if (!FT_Load_Char(face, draw_start[k], FT_LOAD_RENDER)) {
                        
                        int drawX = tempX + face->glyph->bitmap_left;
                        int drawY = cursorY - face->glyph->bitmap_top;

                        // Strict Clipping Check: Is the glyph inside the viewport?
                        if (drawX >= scissorLeft && drawX < scissorRight &&
                            drawY >= scissorTop && drawY < scissorBottom)
                        {
                            drawGlyph(framebuffer, &face->glyph->bitmap, drawX, drawY, textColor);
                        }
                    }
                    tempX += (face->glyph->advance.x >> 6) + format->glyphSpacing;
                }
            }

            // Advance Line
            cursorX = x;
            cursorY += line_height;
            line_start = p + 1;
            p_word_start = p + 1;
            current_line_width = 0;
            current_word_width = 0;

            // Optimization: Stop if we are way past the bottom
            if (cursorY > scissorBottom + line_height) break;
            
            if (force_break) continue;
        }
        
        if (*p == ' ') {
            current_line_width += current_word_width;
            current_word_width = 0;
            p_word_start = p + 1;
        }
    }
}*/

void drawGlyph(Framebuffer *framebuffer, FT_Bitmap *bitmap, int x, int y, ColorRGBA textColor) {
    // printf("\ndrawGlyph %d %d\n", bitmap->rows, bitmap->width); // Preserving your debug print
    
    for (int i = 0; i < bitmap->rows; i++) {
        for (int j = 0; j < bitmap->width; j++) {
            int framebufferX = x + j;
            int framebufferY = y + i;

            if (framebufferX >= 0 && framebufferX < framebuffer->displayWidth && framebufferY >= 0 && framebufferY < framebuffer->displayHeight) {
                
                // Get the glyph's 8-bit alpha value (0-255)
                uint8_t glyph_alpha = bitmap->buffer[i * bitmap->pitch + j];
                
                // Combine with the main text color's alpha
                uint32_t final_alpha = (uint32_t)(glyph_alpha * textColor.a) / 255;

                // Don't draw if it's fully transparent
                if (final_alpha == 0) {
                    continue;
                }

                switch (framebuffer->colorMode) {
                    case COLOR_MODE_TWO:
                    {
                        // Thresholding for 1-bit display. Only draw if > 50% opaque.
                        if (final_alpha > 128) {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 8;
                            int bitIndex = 7 - ((framebufferY * framebuffer->displayWidth + framebufferX) % 8);
                            if (textColor.r == framebuffer->colors[1].r && textColor.g == framebuffer->colors[1].g && textColor.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex); // Set bit for color 1
                            } else {
                                *pixelByte &= ~(1 << bitIndex); // Clear bit for color 0
                            }
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        // Thresholding for 4-bit display.
                        if (final_alpha > 128) {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 2;
                            int nibbleShift = ((framebufferY * framebuffer->displayWidth + framebufferX) % 2) ? 0 : 4;
                            uint8_t colorIndex = (textColor.r >> 4) & 0xF; // Simplified mapping
                            *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        }
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        // Thresholding for 8-bit display.
                        if (final_alpha > 128) {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + framebufferY * framebuffer->displayWidth + framebufferX;
                            *pixelByte = (textColor.r >> 6) | ((textColor.g >> 6) << 2) | ((textColor.b >> 6) << 4);
                        }
                        break;
                    }
                    case COLOR_MODE_RGBA:
                    default:
                    {
                        // Full alpha blending
                        ColorRGBA *pixel = &((ColorRGBA*)framebuffer->pixelData)[framebufferY * framebuffer->displayWidth + framebufferX];
                        ColorRGBA srcColor = { textColor.r, textColor.g, textColor.b, (uint8_t)final_alpha };
                        blendPixel(pixel, srcColor); // Assumes blendPixel exists and is correct
                        break;
                    }
                    
                    // --- OUR NEW/FIXED MODE ---
                    case COLOR_MODE_BGR888: // We know this is RGB888
                    {
                        if (final_alpha == 255) {
                            // Fast path: Fully opaque
                            uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) * 3;
                            *(pixelPtr + 0) = textColor.r; // Red
                            *(pixelPtr + 1) = textColor.g; // Green
                            *(pixelPtr + 2) = textColor.b; // Blue
                        }
                        else if (final_alpha > 0) {
                            // Blend path: Semi-transparent
                            uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) * 3;
                            uint8_t dest_r = *(pixelPtr + 0);
                            uint8_t dest_g = *(pixelPtr + 1);
                            uint8_t dest_b = *(pixelPtr + 2);
                            
                            uint32_t one_minus_alpha = 256 - final_alpha;

                            uint8_t final_r = (uint8_t)((textColor.r * final_alpha + dest_r * one_minus_alpha) >> 8);
                            uint8_t final_g = (uint8_t)((textColor.g * final_alpha + dest_g * one_minus_alpha) >> 8);
                            uint8_t final_b = (uint8_t)((textColor.b * final_alpha + dest_b * one_minus_alpha) >> 8);

                            *(pixelPtr + 0) = final_r; // Red
                            *(pixelPtr + 1) = final_g; // Green
                            *(pixelPtr + 2) = final_b; // Blue
                        }
                        // if final_alpha == 0, we do nothing (already handled by outer check)
                        break;
                    }
                }
            }
        }
    }
}


void drawTransformedGlyph(Framebuffer *framebuffer, FT_Bitmap *bitmap, float x, float y, ColorRGBA textColor, const Matrix3x3 *transform) {

    for (int i = 0; i < bitmap->rows; i++) {
        for (int j = 0; j < bitmap->width; j++) {
            
            float transformedX = x + j;
            float transformedY = y + i;
            TransformPoint(&transformedX, &transformedY, transform);

            int framebufferX = (int)transformedX;
            int framebufferY = (int)transformedY;

            if (framebufferX >= 0 && framebufferX < framebuffer->displayWidth && framebufferY >= 0 && framebufferY < framebuffer->displayHeight) {

                uint8_t glyph_alpha = bitmap->buffer[i * bitmap->pitch + j];
                uint32_t final_alpha = (uint32_t)(glyph_alpha * textColor.a) / 255;
                
                if (final_alpha == 0) {
                    continue;
                }

                // --- Insert the full switch statement ---
                switch (framebuffer->colorMode) {
                    case COLOR_MODE_TWO:
                    {
                        if (final_alpha > 128) {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 8;
                            int bitIndex = 7 - ((framebufferY * framebuffer->displayWidth + framebufferX) % 8);
                            if (textColor.r == framebuffer->colors[1].r && textColor.g == framebuffer->colors[1].g && textColor.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex);
                            } else {
                                *pixelByte &= ~(1 << bitIndex);
                            }
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        if (final_alpha > 128) {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 2;
                            int nibbleShift = ((framebufferY * framebuffer->displayWidth + framebufferX) % 2) ? 0 : 4;
                            uint8_t colorIndex = (textColor.r >> 4) & 0xF;
                            *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        }
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        if (final_alpha > 128) {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + framebufferY * framebuffer->displayWidth + framebufferX;
                            *pixelByte = (textColor.r >> 6) | ((textColor.g >> 6) << 2) | ((textColor.b >> 6) << 4);
                        }
                        break;
                    }
                    case COLOR_MODE_RGBA:
                    default:
                    {
                        ColorRGBA *pixel = &((ColorRGBA*)framebuffer->pixelData)[framebufferY * framebuffer->displayWidth + framebufferX];
                        ColorRGBA srcColor = { textColor.r, textColor.g, textColor.b, (uint8_t)final_alpha };
                        blendPixel(pixel, srcColor); // Assumes blendPixel exists and is correct
                        break;
                    }
                    case COLOR_MODE_BGR888: // We know this is RGB888
                    {
                        if (final_alpha == 255) {
                            uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) * 3;
                            *(pixelPtr + 0) = textColor.r; // Red
                            *(pixelPtr + 1) = textColor.g; // Green
                            *(pixelPtr + 2) = textColor.b; // Blue
                        }
                        else if (final_alpha > 0) {
                            uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) * 3;
                            uint8_t dest_r = *(pixelPtr + 0);
                            uint8_t dest_g = *(pixelPtr + 1);
                            uint8_t dest_b = *(pixelPtr + 2);
                            
                            uint32_t one_minus_alpha = 256 - final_alpha;

                            uint8_t final_r = (uint8_t)((textColor.r * final_alpha + dest_r * one_minus_alpha) >> 8);
                            uint8_t final_g = (uint8_t)((textColor.g * final_alpha + dest_g * one_minus_alpha) >> 8);
                            uint8_t final_b = (uint8_t)((textColor.b * final_alpha + dest_b * one_minus_alpha) >> 8);

                            *(pixelPtr + 0) = final_r; // Red
                            *(pixelPtr + 1) = final_g; // Green
                            *(pixelPtr + 2) = final_b; // Blue
                        }
                        break;
                    }
                }
            }
        }
    }
}

// Render text with transformation and optional gradient, using alpha blending
void renderTextWithTransform(Framebuffer *framebuffer, FT_Face face, const char *text, int startX, int startY, ColorRGBA textColor, int fontSize, const Matrix3x3 *transform, const Gradient *gradient) {

    FT_Set_Pixel_Sizes(face, 0, fontSize);
    float x = (float)startX;
    float y = (float)startY;

    for (const char *p = text; *p; p++) {
        if (FT_Load_Char(face, *p, FT_LOAD_RENDER)) {
            continue; // Skip characters that fail to load
        }

        FT_Bitmap *bitmap = &face->glyph->bitmap;
        float x0 = x + face->glyph->bitmap_left;
        float y0 = y - face->glyph->bitmap_top;

        // Apply gradient coloring if present
        if (gradient) {
            for (int i = 0; i < bitmap->rows; i++) {
                for (int j = 0; j < bitmap->width; j++) {
                    float factor = (float)j / bitmap->width;
                    ColorRGBA startColor, endColor;
                    float startPos = 0.0f, endPos = 1.0f;

                    for (int k = 0; k < gradient->numStops - 1; k++) {
                        if (factor >= gradient->stops[k].position && factor <= gradient->stops[k + 1].position) {
                            startColor = gradient->stops[k].color;
                            endColor = gradient->stops[k + 1].color;
                            startPos = gradient->stops[k].position;
                            endPos = gradient->stops[k + 1].position;
                            break;
                        }
                    }

                    float localFactor = (factor - startPos) / (endPos - startPos);
                    ColorRGBA interpColor = {
                        .r = (uint8_t)((1 - localFactor) * startColor.r + localFactor * endColor.r),
                        .g = (uint8_t)((1 - localFactor) * startColor.g + localFactor * endColor.g),
                        .b = (uint8_t)((1 - localFactor) * startColor.b + localFactor * endColor.b),
                        .a = textColor.a  // Use the textColor's alpha
                    };

                    drawTransformedGlyph(framebuffer, bitmap, x0, y0, interpColor, transform);
                }
            }
        } else {
            drawTransformedGlyph(framebuffer, bitmap, x0, y0, textColor, transform);
        }

        // Advance cursor position with glyph advance
        float advX = (float)(face->glyph->advance.x >> 6);
        float advY = 0.0f;  // Typically, advance in the Y-direction for horizontal text is zero
        TransformPoint(&advX, &advY, transform);
        x += advX;
        y += advY;
    }
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "CPUGraphics.h"
#include "esp_heap_caps.h" // For memory allocation

// Helper function to blend/write a pixel in the BGR888/RGB888 framebuffer
// Note: This is an unrolled version of the logic from drawRectangleCFramebuffer
static inline void write_pixel_rgb888(Framebuffer *framebuffer, int x, int y, ColorRGBA src) {
    
    // Calculate the final alpha value (0-255)
    uint32_t final_alpha = src.a;
    
    if (final_alpha == 255) {
        // Fast path: Fully opaque, just overwrite
        uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) * 3;
        *(pixelPtr + 0) = src.r; // R
        *(pixelPtr + 1) = src.g; // G
        *(pixelPtr + 2) = src.b; // B
    }
    else if (final_alpha > 0) {
        // Blend path: Semi-transparent (using integer math)
        uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) * 3;
        
        // Read destination color (R, G, B)
        uint8_t dest_r = *(pixelPtr + 0);
        uint8_t dest_g = *(pixelPtr + 1);
        uint8_t dest_b = *(pixelPtr + 2);
        
        uint32_t one_minus_alpha = 256 - final_alpha;

        // Perform integer blend for each channel
        uint8_t final_r = (uint8_t)((src.r * final_alpha + dest_r * one_minus_alpha) >> 8);
        uint8_t final_g = (uint8_t)((src.g * final_alpha + dest_g * one_minus_alpha) >> 8);
        uint8_t final_b = (uint8_t)((src.b * final_alpha + dest_b * one_minus_alpha) >> 8);

        // Write the final color back (R, G, B order)
        *(pixelPtr + 0) = final_r;
        *(pixelPtr + 1) = final_g;
        *(pixelPtr + 2) = final_b;
    }
}


ImageTexture* resizeImageBilinear(const ImageTexture* input, int newWidth, int newHeight) {
    // Allocate the ImageTexture struct in internal RAM
    ImageTexture* resized = (ImageTexture*)malloc(sizeof(ImageTexture));
    if (!resized) return NULL;
    
    resized->width = newWidth;
    resized->height = newHeight;
    
    // Allocate the pixel data in PSRAM (critical for large textures)
    size_t new_size = newWidth * newHeight * sizeof(ColorRGBA);
    resized->data = (ColorRGBA*)heap_caps_malloc(new_size, MALLOC_CAP_SPIRAM);
    
    if (!resized->data) {
        free(resized);
        return NULL;
    }

    float x_ratio = (float)input->width / newWidth;
    float y_ratio = (float)input->height / newHeight;

    for (int i = 0; i < newHeight; ++i) {
        for (int j = 0; j < newWidth; ++j) {
            float px = j * x_ratio;
            float py = i * y_ratio;
            int x = (int)px;
            int y = (int)py;

            float x_diff = px - x;
            float y_diff = py - y;

            // Ensure we don't read past the edge of the input texture
            int x1 = (x + 1 < input->width) ? x + 1 : x;
            int y1 = (y + 1 < input->height) ? y + 1 : y;

            const ColorRGBA *c00 = &input->data[y * input->width + x];
            const ColorRGBA *c10 = &input->data[y * input->width + x1];
            const ColorRGBA *c01 = &input->data[y1 * input->width + x];
            const ColorRGBA *c11 = &input->data[y1 * input->width + x1];

            // Perform Bilinear interpolation for R, G, B, A components
            // R = c00*w0 + c10*w1 + c01*w2 + c11*w3
            
            float w0 = (1.0f - x_diff) * (1.0f - y_diff);
            float w1 = x_diff * (1.0f - y_diff);
            float w2 = y_diff * (1.0f - x_diff);
            float w3 = x_diff * y_diff;
            
            ColorRGBA result;

            result.r = (uint8_t)(c00->r * w0 + c10->r * w1 + c01->r * w2 + c11->r * w3);
            result.g = (uint8_t)(c00->g * w0 + c10->g * w1 + c01->g * w2 + c11->g * w3);
            result.b = (uint8_t)(c00->b * w0 + c10->b * w1 + c01->b * w2 + c11->b * w3);
            result.a = (uint8_t)(c00->a * w0 + c10->a * w1 + c01->a * w2 + c11->a * w3);

            resized->data[i * newWidth + j] = result;
        }
    }

    return resized;
}


void drawImageTexture(Framebuffer *framebuffer, const ImageTexture *texture, int x, int y, int width, int height) {
    // Calculate scaling factors for texture mapping
    float scaleX = (float)texture->width / width;
    float scaleY = (float)texture->height / height;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Calculate the corresponding position in the texture
            int texX = (int)(j * scaleX);
            int texY = (int)(i * scaleY);

            int framebufferX = x + j;
            int framebufferY = y + i;

            if (framebufferX >= 0 && framebufferX < framebuffer->displayWidth && framebufferY >= 0 && framebufferY < framebuffer->displayHeight) {
                // Get the color from the texture
                ColorRGBA textureColor = texture->data[texY * texture->width + texX];

                switch (framebuffer->colorMode) {
                    // (Retained original cases for completeness)
                    case COLOR_MODE_TWO:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 8;
                        int bitIndex = 7 - ((framebufferY * framebuffer->displayWidth + framebufferX) % 8);
                        if (textureColor.r == framebuffer->colors[1].r && textureColor.g == framebuffer->colors[1].g && textureColor.b == framebuffer->colors[1].b) {
                            *pixelByte |= (1 << bitIndex);
                        } else {
                            *pixelByte &= ~(1 << bitIndex);
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 2;
                        int nibbleShift = ((framebufferY * framebuffer->displayWidth + framebufferX) % 2) ? 0 : 4;
                        uint8_t colorIndex = (textureColor.r >> 4) & 0xF;
                        *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + framebufferY * framebuffer->displayWidth + framebufferX;
                        *pixelByte = (textureColor.r >> 6) | ((textureColor.g >> 6) << 2) | ((textureColor.b >> 6) << 4);
                        break;
                    }
                    case COLOR_MODE_RGBA:
                    default:
                    {
                        ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[framebufferY * framebuffer->displayWidth + framebufferX];
                        blendPixel(fbColor, textureColor);
                        break;
                    }
                    
                    // --- NEW/FIXED RGB888 CASE ---
                    case COLOR_MODE_BGR888:
                    {
                        // Use the optimized inline writer function
                        write_pixel_rgb888(framebuffer, framebufferX, framebufferY, textureColor);
                        break;
                    }
                }
            }
        }
    }
}

void drawImageTextureWithAlpha(Framebuffer *framebuffer, const ImageTexture *texture, int x, int y, int width, int height, float alpha) {
    // Safety clamp to ensure alpha stays between 0.0 and 1.0
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    // Calculate scaling factors for texture mapping
    float scaleX = (float)texture->width / width;
    float scaleY = (float)texture->height / height;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Calculate the corresponding position in the texture
            int texX = (int)(j * scaleX);
            int texY = (int)(i * scaleY);

            int framebufferX = x + j;
            int framebufferY = y + i;

            if (framebufferX >= 0 && framebufferX < framebuffer->displayWidth && framebufferY >= 0 && framebufferY < framebuffer->displayHeight) {
                
                // 1. Get the original color from the texture
                ColorRGBA textureColor = texture->data[texY * texture->width + texX];

                // 2. Pre-multiply the pixel's intrinsic alpha by the global alpha modifier
                textureColor.a = (uint8_t)(textureColor.a * alpha);

                // 3. OPTIMIZATION: If the pixel is now fully transparent, skip the math entirely!
                if (textureColor.a == 0) {
                    continue;
                }

                switch (framebuffer->colorMode) {
                    case COLOR_MODE_TWO:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 8;
                        int bitIndex = 7 - ((framebufferY * framebuffer->displayWidth + framebufferX) % 8);
                        // Thresholding for 1-bit monochrome (alpha > 128 considered solid enough to draw if color matches)
                        if (textureColor.a > 128 && textureColor.r == framebuffer->colors[1].r && textureColor.g == framebuffer->colors[1].g && textureColor.b == framebuffer->colors[1].b) {
                            *pixelByte |= (1 << bitIndex);
                        } else {
                            *pixelByte &= ~(1 << bitIndex);
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        // 16-color mode generally doesn't support alpha blending, so we write if visible
                        if (textureColor.a > 128) {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 2;
                            int nibbleShift = ((framebufferY * framebuffer->displayWidth + framebufferX) % 2) ? 0 : 4;
                            uint8_t colorIndex = (textureColor.r >> 4) & 0xF;
                            *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        }
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        if (textureColor.a > 128) {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + framebufferY * framebuffer->displayWidth + framebufferX;
                            *pixelByte = (textureColor.r >> 6) | ((textureColor.g >> 6) << 2) | ((textureColor.b >> 6) << 4);
                        }
                        break;
                    }
                    case COLOR_MODE_RGBA:
                    {
                        ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[framebufferY * framebuffer->displayWidth + framebufferX];
                        // Since we already pre-multiplied textureColor.a, normal blendPixel handles it perfectly
                        blendPixel(fbColor, textureColor);
                        break;
                    }
                    case COLOR_MODE_BGR888:
                    {
                        // Your integer-math writer instantly processes the pre-multiplied alpha
                        write_pixel_rgb888(framebuffer, framebufferX, framebufferY, textureColor);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
}

/*void drawImageTextureWithAlpha(Framebuffer *fb, const ImageTexture *texture,
                               int x, int y, int width, int height, float alpha) {
    drawImageTextureWithAlphaClip(fb, texture, x, y, width, height, alpha, 0, 0, fb->displayWidth, fb->displayHeight);
}*/

void drawImageTextureWithAlphaClip(Framebuffer *fb, const ImageTexture *texture,
                               int x, int y, int width, int height, float alpha,
                               int clipX, int clipY, int clipW, int clipH) {

    // 1. CLIP RECTANGLE INTERSECTION (Remove bounds checks from loop)
    int startX = (x > clipX) ? x : clipX;
    int startY = (y > clipY) ? y : clipY;
    int endX   = ((x + width) < (clipX + clipW)) ? (x + width) : (clipX + clipW);
    int endY   = ((y + height) < (clipY + clipH)) ? (y + height) : (clipY + clipH);

    if (endX <= startX || endY <= startY) return;

    // 2. CONVERT FLOAT ALPHA TO FAST INTEGER MULTIPLIER (0 to 256)
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    uint32_t global_alpha_fp = (uint32_t)(alpha * 256.0f);
    if (global_alpha_fp > 256) global_alpha_fp = 256;

    // 3. SETUP FIXED POINT SCALING (Q16.16)
    int32_t fp_scaleX = (width > 0) ? ((texture->width << 16) / width) : 0;
    int32_t fp_scaleY = (height > 0) ? ((texture->height << 16) / height) : 0;

    int offsetX = startX - x;
    int offsetY = startY - y;

    int32_t startU = offsetX * fp_scaleX;
    int32_t startV = offsetY * fp_scaleY;

    // We only support BGR888 in the fast path for simplicity here.
    if (fb->colorMode != COLOR_MODE_BGR888) return;

    for (int i = startY; i < endY; i++) {
        
        int texY = startV >> 16;
        if (texY >= texture->height) texY = texture->height - 1;

        const ColorRGBA *texRowPtr = &texture->data[texY * texture->width];
        int32_t currentU = startU;

        int fbIdx = (i * fb->displayWidth + startX) * 3;
        uint8_t *fbPtr = (uint8_t *)fb->pixelData + fbIdx;

        for (int j = startX; j < endX; j++) {
            
            int texX = currentU >> 16;
            if (texX >= texture->width) texX = texture->width - 1;

            ColorRGBA val = texRowPtr[texX];

            // FAST GLOBAL ALPHA PRE-MULTIPLY (Integer math only!)
            uint8_t final_a = (val.a * global_alpha_fp) >> 8;

            // ONLY draw if the pixel has visible opacity
            if (final_a > 0) {
                if (final_a == 255) {
                    fbPtr[0] = val.b;
                    fbPtr[1] = val.g;
                    fbPtr[2] = val.r;
                } else {
                    // Alpha Blend
                    uint16_t inv_a = 256 - final_a;
                    fbPtr[0] = (val.b * final_a + fbPtr[0] * inv_a) >> 8;
                    fbPtr[1] = (val.g * final_a + fbPtr[1] * inv_a) >> 8;
                    fbPtr[2] = (val.r * final_a + fbPtr[2] * inv_a) >> 8;
                }
            }

            // INCREMENT (Must happen outside the 'if' block so we don't desync pointers)
            fbPtr += 3;
            currentU += fp_scaleX;
        }

        startV += fp_scaleY;
    }
}

void drawImageTextureOptimizedExtended(Framebuffer *fb, const ImageTexture *texture,
                               int x, int y, int width, int height,
                               int clipX, int clipY, int clipW, int clipH) {

    // 1. CLIP RECTANGLE INTERSECTION (Remove bounds checks from loop)
    int startX = (x > clipX) ? x : clipX;
    int startY = (y > clipY) ? y : clipY;
    int endX   = ((x + width) < (clipX + clipW)) ? (x + width) : (clipX + clipW);
    int endY   = ((y + height) < (clipY + clipH)) ? (y + height) : (clipY + clipH);

    if (endX <= startX || endY <= startY) return;

    // 2. SETUP FIXED POINT SCALING (Q16.16)
    // 65536 = 1.0.  We use this to avoid float math.
    // Scale = (TextureWidth / DrawWidth) * 65536
    int32_t fp_scaleX = (width > 0) ? ((texture->width << 16) / width) : 0;
    int32_t fp_scaleY = (height > 0) ? ((texture->height << 16) / height) : 0;

    // 3. CALCULATE INITIAL OFFSETS
    // Because we might start drawing at x=10 (due to clipping) instead of x=0,
    // we need to fast-forward the texture coordinate 'u' to match startX.
    int offsetX = startX - x;
    int offsetY = startY - y;

    int32_t startU = offsetX * fp_scaleX;
    int32_t startV = offsetY * fp_scaleY; // Vertical accumulator

    // 4. PRE-CALCULATE POINTERS
    // We only support BGR888 in the fast path for simplicity here.
    // (You can wrap this whole function in a switch(mode) if you want others)
    if (fb->colorMode != COLOR_MODE_BGR888) return; // Fallback or exit

    for (int i = startY; i < endY; i++) {
        
        // Calculate the Texture Y coordinate for this row
        int texY = startV >> 16;
        if (texY >= texture->height) texY = texture->height - 1; // Safety clamp

        // Get the pointer to the start of this ROW in the texture
        // This avoids doing (y * width) multiplication inside the inner loop
        const ColorRGBA *texRowPtr = &texture->data[texY * texture->width];

        // Reset Horizontal accumulator for the new row
        int32_t currentU = startU;

        // Get pointer to the start of this ROW in the framebuffer
        int fbIdx = (i * fb->displayWidth + startX) * 3;
        uint8_t *fbPtr = (uint8_t *)fb->pixelData + fbIdx;

        for (int j = startX; j < endX; j++) {
            
            // Get Texture X coordinate
            int texX = currentU >> 16;
            // (Optional: bitwise mask if texture is power-of-2 width, faster than check)
            if (texX >= texture->width) texX = texture->width - 1;

            // READ COLOR
            ColorRGBA val = texRowPtr[texX];

            // WRITE COLOR (Fast Path BGR888)
            if (val.a == 255) {
                fbPtr[0] = val.b;
                fbPtr[1] = val.g;
                fbPtr[2] = val.r;
            } else if (val.a > 0) {
                // Alpha Blend
                uint16_t inv_a = 255 - val.a;
                fbPtr[0] = (val.b * val.a + fbPtr[0] * inv_a) >> 8;
                fbPtr[1] = (val.g * val.a + fbPtr[1] * inv_a) >> 8;
                fbPtr[2] = (val.r * val.a + fbPtr[2] * inv_a) >> 8;
            }

            // INCREMENT
            fbPtr += 3;           // Move FB pointer right 1 pixel
            currentU += fp_scaleX; // Move Texture coordinate right 1 step
        }

        // Move Texture coordinate down 1 step
        startV += fp_scaleY;
    }
}

// Requires your Matrix3x3Inverse, Matrix3x3Determinant, and TransformPoint functions

void drawImageTextureOptimizedExtendedTransformed(Framebuffer *fb, const ImageTexture *texture,
                                 int x, int y, int width, int height,
                                 int clipX, int clipY, int clipW, int clipH,
                                 const Matrix3x3 *transform) {

    // 1. CALCULATE BOUNDING BOX
    // We need to figure out where the rotated image lands on the screen
    // so we only loop over those specific pixels.
    
    // The 4 corners of the destination rectangle
    float corners[4][2] = {
        {(float)x,         (float)y},          // Top-Left
        {(float)(x+width), (float)y},          // Top-Right
        {(float)(x+width), (float)(y+height)}, // Bottom-Right
        {(float)x,         (float)(y+height)}  // Bottom-Left
    };

    float minSx = 100000.0f, minSy = 100000.0f;
    float maxSx = -100000.0f, maxSy = -100000.0f;

    // Transform corners to screen space to find the bounding box
    for(int i=0; i<4; i++) {
        if (transform) {
            TransformPoint(&corners[i][0], &corners[i][1], transform);
        }
        if (corners[i][0] < minSx) minSx = corners[i][0];
        if (corners[i][0] > maxSx) maxSx = corners[i][0];
        if (corners[i][1] < minSy) minSy = corners[i][1];
        if (corners[i][1] > maxSy) maxSy = corners[i][1];
    }

    // 2. CLIP THE BOUNDING BOX
    int startX = (int)minSx;
    int startY = (int)minSy;
    int endX   = (int)maxSx + 1;
    int endY   = (int)maxSy + 1;

    if (startX < clipX) startX = clipX;
    if (startY < clipY) startY = clipY;
    if (endX > clipX + clipW) endX = clipX + clipW;
    if (endY > clipY + clipH) endY = clipY + clipH;

    if (endX <= startX || endY <= startY) return; // Fully clipped out

    // 3. CALCULATE INVERSE MATRIX (Screen -> Local Space)
    // We need to know: For a given pixel on screen (sx, sy), what is the local (u,v)?
    Matrix3x3 inv;
    if (transform) {
        if (!Matrix3x3Inverse(transform, &inv)) return; // Singular matrix
    } else {
        inv = IdentityMatrix(); // Should likely use the optimized function instead if no transform
    }

    // 4. SETUP INVERSE MAPPING STEPS
    // Calculate how much U (local x) and V (local y) change when we move 1 pixel on screen.
    // This allows us to use fast addition in the loop instead of matrix multiplication.
    
    float dU_dx = inv.m[0][0]; // Change in local X per screen X
    float dV_dx = inv.m[1][0]; // Change in local Y per screen X
    float dU_dy = inv.m[0][1]; // Change in local X per screen Y
    float dV_dy = inv.m[1][1]; // Change in local Y per screen Y

    // Calculate source coords for the very first pixel (startX, startY)
    float startU_float = startX * inv.m[0][0] + startY * inv.m[0][1] + inv.m[0][2];
    float startV_float = startX * inv.m[1][0] + startY * inv.m[1][1] + inv.m[1][2];

    // Adjust for the local rectangle offset (since texture sampling starts at 0,0)
    // The "local space" coordinates we get back are relative to (x,y)
    startU_float -= x;
    startV_float -= y;

    // 5. CONVERT TO FIXED POINT (Q16.16)
    int32_t fp_dU_dx = (int32_t)(dU_dx * 65536.0f);
    int32_t fp_dV_dx = (int32_t)(dV_dx * 65536.0f);
    int32_t fp_dU_dy = (int32_t)(dU_dy * 65536.0f);
    int32_t fp_dV_dy = (int32_t)(dV_dy * 65536.0f);

    int32_t fp_U_row = (int32_t)(startU_float * 65536.0f);
    int32_t fp_V_row = (int32_t)(startV_float * 65536.0f);

    // Pre-calculate scaling ratios (Texture Size / Rect Size)
    // We need to map the "local coordinate" (0..width) to "texture coordinate" (0..texWidth)
    // We use float here because this is calculated once per frame, not per pixel.
    float ratioX = (float)texture->width / width;
    float ratioY = (float)texture->height / height;
    
    // Convert ratios to Fixed Point for the final lookup step
    int32_t fp_ratioX = (int32_t)(ratioX * 65536.0f);
    int32_t fp_ratioY = (int32_t)(ratioY * 65536.0f);

    // 6. RENDER LOOP
    for (int i = startY; i < endY; i++) {
        
        int32_t fp_U = fp_U_row;
        int32_t fp_V = fp_V_row;
        
        int fbIdx = (i * fb->displayWidth + startX) * 3;
        uint8_t *fbPtr = (uint8_t *)fb->pixelData + fbIdx;

        for (int j = startX; j < endX; j++) {
            
            // Convert Local Coords (u,v) to Texture Coords (texX, texY)
            // Math: (fp_U * fp_ratioX) >> 16
            // We use 64-bit int intermediate cast to prevent overflow during multiply
            int32_t texX = ((int64_t)fp_U * fp_ratioX) >> 32; // result is integer index
            int32_t texY = ((int64_t)fp_V * fp_ratioY) >> 32;

            // Check if this pixel is actually inside the texture
            if (texX >= 0 && texX < texture->width && texY >= 0 && texY < texture->height) {
                
                // Get Color
                ColorRGBA val = texture->data[texY * texture->width + texX];

                // Write Pixel (Fast Path BGR888)
                if (val.a == 255) {
                    fbPtr[0] = val.b; fbPtr[1] = val.g; fbPtr[2] = val.r;
                } else if (val.a > 0) {
                    uint16_t inv_a = 255 - val.a;
                    fbPtr[0] = (val.b * val.a + fbPtr[0] * inv_a) >> 8;
                    fbPtr[1] = (val.g * val.a + fbPtr[1] * inv_a) >> 8;
                    fbPtr[2] = (val.r * val.a + fbPtr[2] * inv_a) >> 8;
                }
            }

            // Advance
            fbPtr += 3;
            fp_U += fp_dU_dx;
            fp_V += fp_dV_dx;
        }

        // Advance Row
        fp_U_row += fp_dU_dy;
        fp_V_row += fp_dV_dy;
    }
}


/*void drawImageTextureWithTransform(Framebuffer *framebuffer, const ImageTexture *texture, int x, int y, int width, int height, const Matrix3x3 *transform) {
    // Calculate scaling factors for texture mapping
    float scaleX = (float)texture->width / width;
    float scaleY = (float)texture->height / height;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Calculate the corresponding position in the texture
            int texX = (int)(j * scaleX);
            int texY = (int)(i * scaleY);

            // Calculate the transformed position on the framebuffer
            float framebufferX = (float)(x + j);
            float framebufferY = (float)(y + i);
            TransformPoint(&framebufferX, &framebufferY, transform);

            int indexX = (int)framebufferX;
            int indexY = (int)framebufferY;

            if (indexX >= 0 && indexX < framebuffer->displayWidth && indexY >= 0 && indexY < framebuffer->displayHeight) {
                // Get the color from the texture
                ColorRGBA textureColor = texture->data[texY * texture->width + texX];

                switch (framebuffer->colorMode) {
                    // (Retained original cases for completeness)
                    case COLOR_MODE_TWO:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (indexY * framebuffer->displayWidth + indexX) / 8;
                        int bitIndex = 7 - ((indexY * framebuffer->displayWidth + indexX) % 8);
                        if (textureColor.r == framebuffer->colors[1].r && textureColor.g == framebuffer->colors[1].g && textureColor.b == framebuffer->colors[1].b) {
                            *pixelByte |= (1 << bitIndex);
                        } else {
                            *pixelByte &= ~(1 << bitIndex);
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (indexY * framebuffer->displayWidth + indexX) / 2;
                        int nibbleShift = ((indexY * framebuffer->displayWidth + indexX) % 2) ? 0 : 4;
                        uint8_t colorIndex = (textureColor.r >> 4) & 0xF;
                        *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + indexY * framebuffer->displayWidth + indexX;
                        *pixelByte = (textureColor.r >> 6) | ((textureColor.g >> 6) << 2) | ((textureColor.b >> 6) << 4);
                        break;
                    }
                    case COLOR_MODE_RGBA:
                    default:
                    {
                        ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[indexY * framebuffer->displayWidth + indexX];
                        blendPixel(fbColor, textureColor);
                        break;
                    }

                    // --- NEW/FIXED RGB888 CASE ---
                    case COLOR_MODE_BGR888:
                    {
                        write_pixel_rgb888(framebuffer, indexX, indexY, textureColor);
                        break;
                    }
                }
            }
        }
    }
}*/

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void drawImageTextureWithTransform(Framebuffer *framebuffer, const ImageTexture *texture, int x, int y, int width, int height, const Matrix3x3 *transform) {
    // 1. Calculate Inverse Transform (Screen Space -> Local Space)
    Matrix3x3 inverseTransform;
    if (!Matrix3x3Inverse(transform, &inverseTransform)) {
        return; // Cannot invert (e.g., scale is 0), abort drawing
    }

    // 2. Find Screen Bounding Box
    // We project the 4 corners of the command's rectangle onto the screen
    // to find out which pixels we actually need to loop over.
    // The "Local Space" for the command is 0..width and 0..height.
    
    // Define the 4 corners in Local Space (Relative to the transform origin)
    typedef struct { float x, y; } Vec2;
    Vec2 corners[4] = {
        {0, 0},
        {(float)width, 0},
        {(float)width, (float)height},
        {0, (float)height}
    };

    float minX = 100000, maxX = -100000;
    float minY = 100000, maxY = -100000;

    for (int i = 0; i < 4; i++) {
        // Apply Forward Transform (Local -> Screen)
        float tx = corners[i].x;
        float ty = corners[i].y;
        TransformPoint(&tx, &ty, transform);
        
        if (tx < minX) minX = tx;
        if (tx > maxX) maxX = tx;
        if (ty < minY) minY = ty;
        if (ty > maxY) maxY = ty;
    }

    // 3. Clip Bounding Box to Display Limits
    int startX = MAX(0, (int)floorf(minX));
    int endX   = MIN(framebuffer->displayWidth, (int)ceilf(maxX));
    int startY = MAX(0, (int)floorf(minY));
    int endY   = MIN(framebuffer->displayHeight, (int)ceilf(maxY));

    // 4. Iterate over the Bounding Box (Inverse Mapping)
    // For every pixel on the screen, check if it belongs to our texture.
    for (int screenY = startY; screenY < endY; screenY++) {
        for (int screenX = startX; screenX < endX; screenX++) {
            
            // A. Map Screen Coordinate -> Local Coordinate
            float u = (float)screenX;
            float v = (float)screenY;
            TransformPoint(&u, &v, &inverseTransform);

            // B. Handle Scaling (Command Size vs Texture Size)
            // The command might say "Draw at 100x100", but the PNG is 50x50.
            // We must map the 0..100 range to 0..50.
            // Formula: TextureCoord = LocalCoord * (TextureSize / CommandSize)
            float texU = u * ((float)texture->width / (float)width);
            float texV = v * ((float)texture->height / (float)height);

            // C. Nearest Neighbor Rounding
            int texX = (int)roundf(texU);
            int texY = (int)roundf(texV);

            // D. Check Bounds (Is this pixel actually inside the texture?)
            if (texX >= 0 && texX < texture->width && texY >= 0 && texY < texture->height) {
                
                // Sample Color
                ColorRGBA textureColor = texture->data[texY * texture->width + texX];

                // Transparency Check
                if (textureColor.a == 0) continue;

                // Draw Pixel (Using your existing mode logic)
                switch (framebuffer->colorMode) {
                     case COLOR_MODE_BGR888:
                        write_pixel_rgb888(framebuffer, screenX, screenY, textureColor);
                        break;
                    
                    case COLOR_MODE_RGBA:
                    default:
                        // Assuming you have a helper for this pointer math
                        ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[screenY * framebuffer->displayWidth + screenX];
                        blendPixel(fbColor, textureColor);
                        break;
                    
                    // Add other cases (16-bit, etc) if needed
                }
            }
        }
    }
}

void drawImageTextureRotate3DY(Framebuffer *framebuffer, const ImageTexture *texture, int x, int y, int width, int height, float angle) {
    // Calculate scaling factors for the texture mapping
    float scaleX = (float)texture->width / width;
    float scaleY = (float)texture->height / height;

    // Calculate rotation components
    float cosAngle = cosf(angle);
    float sinAngle = sinf(angle);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Calculate the texture coordinates
            int texX = (int)(j * scaleX);
            int texY = (int)(i * scaleY);

            // Calculate the 3D rotation and projection onto the 2D plane
            float transformedX = (float)(texX - texture->width / 2) * cosAngle;
            float rotatedZ = (float)(texX - texture->width / 2) * sinAngle;
            float perspective = 1.0f / (1.0f + rotatedZ * 0.001f);  // Adjust perspective scaling factor
            float projectedX = transformedX * perspective + framebuffer->displayWidth / 2;
            float projectedY = (float)(texY - texture->height / 2) * perspective + framebuffer->displayHeight / 2;

            // Use the projected coordinates for plotting
            int framebufferX = (int)projectedX;
            int framebufferY = (int)projectedY;

            if (framebufferX >= 0 && framebufferX < framebuffer->displayWidth &&
                framebufferY >= 0 && framebufferY < framebuffer->displayHeight) {
                // Fetch the texture color
                ColorRGBA textureColor = texture->data[texY * texture->width + texX];

                switch (framebuffer->colorMode) {
                    case COLOR_MODE_TWO:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 8;
                        int bitIndex = 7 - ((framebufferY * framebuffer->displayWidth + framebufferX) % 8);

                        if (textureColor.r == framebuffer->colors[1].r && textureColor.g == framebuffer->colors[1].g && textureColor.b == framebuffer->colors[1].b) {
                            *pixelByte |= (1 << bitIndex); // Set bit for color 1
                        } else {
                            *pixelByte &= ~(1 << bitIndex); // Clear bit for color 0
                        }
                        break;
                    }
                    case COLOR_MODE_SIXTEEN:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (framebufferY * framebuffer->displayWidth + framebufferX) / 2;
                        int nibbleShift = ((framebufferY * framebuffer->displayWidth + framebufferX) % 2) ? 0 : 4;
                        uint8_t colorIndex = (textureColor.r >> 4) & 0xF; // Simplified mapping
                        *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                        break;
                    }
                    case COLOR_MODE_256:
                    {
                        uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + framebufferY * framebuffer->displayWidth + framebufferX;
                        *pixelByte = (textureColor.r >> 6) | ((textureColor.g >> 6) << 2) | ((textureColor.b >> 6) << 4);
                        break;
                    }
                    case COLOR_MODE_RGBA:
                    default:
                    {
                        ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[framebufferY * framebuffer->displayWidth + framebufferX];
                        blendPixel(fbColor, textureColor);
                        break;
                    }
                }
            }
        }
    }
}



// NOTE: This function assumes the write_pixel_rgb888_alpha helper (or its logic)
// is available and correctly handles the R/B swap for the BGR888 framebuffer.

static inline void write_pixel_rgb888_alpha_inline(Framebuffer *framebuffer, int x, int y, ColorRGBA src) {
    
    uint32_t final_alpha = src.a;
    
    if (final_alpha == 255) {
        // Fast path: Opaque (R/B Swapped on write)
        uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) * 3;
        *(pixelPtr + 0) = src.b; // B -> R position
        *(pixelPtr + 1) = src.g; // G -> G position
        *(pixelPtr + 2) = src.r; // R -> B position
    }
    else if (final_alpha > 0) {
        // Blend path (R/B Swapped on read and write)
        uint8_t *pixelPtr = (uint8_t *)framebuffer->pixelData + (y * framebuffer->displayWidth + x) * 3;
        
        // Read destination color (Correctly labeled R, G, B)
        uint8_t dest_b = *(pixelPtr + 0);
        uint8_t dest_g = *(pixelPtr + 1);
        uint8_t dest_r = *(pixelPtr + 2);
        
        uint32_t one_minus_alpha = 256 - final_alpha;

        // Perform integer blend for each channel
        uint8_t final_r = (uint8_t)((src.r * final_alpha + dest_r * one_minus_alpha) >> 8);
        uint8_t final_g = (uint8_t)((src.g * final_alpha + dest_g * one_minus_alpha) >> 8);
        uint8_t final_b = (uint8_t)((src.b * final_alpha + dest_b * one_minus_alpha) >> 8);

        // Write BGR (The correct order for the display to interpret)
        *(pixelPtr + 0) = final_b;
        *(pixelPtr + 1) = final_g;
        *(pixelPtr + 2) = final_r;
    }
}


void drawLineWithThickness(Framebuffer *framebuffer, int x0, int y0, int x1, int y1, ColorRGBA color, int thickness) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        // Draw square brush at each point along the line for thickness
        for (int i = -thickness / 2; i <= thickness / 2; i++) {
            for (int j = -thickness / 2; j <= thickness / 2; j++) {
                int px = x0 + i;
                int py = y0 + j;
                
                // --- Bounds Check ---
                if (px >= 0 && px < framebuffer->displayWidth && py >= 0 && py < framebuffer->displayHeight) {
                    
                    switch (framebuffer->colorMode) {
                        case COLOR_MODE_TWO:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (py * framebuffer->displayWidth + px) / 8;
                            int bitIndex = 7 - ((py * framebuffer->displayWidth + px) % 8);

                            if (color.r == framebuffer->colors[1].r && color.g == framebuffer->colors[1].g && color.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex); // Set bit for color 1
                            } else {
                                *pixelByte &= ~(1 << bitIndex); // Clear bit for color 0
                            }
                            break;
                        }
                        case COLOR_MODE_SIXTEEN:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (py * framebuffer->displayWidth + px) / 2;
                            int nibbleShift = ((py * framebuffer->displayWidth + px) % 2) ? 0 : 4;
                            uint8_t colorIndex = (color.r >> 4) & 0xF; // Simplified mapping
                            *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                            break;
                        }
                        case COLOR_MODE_256:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + py * framebuffer->displayWidth + px;
                            *pixelByte = (color.r >> 6) | ((color.g >> 6) << 2) | ((color.b >> 6) << 4);
                            break;
                        }
                        
                        // --- NEW 18-BIT COLOR MODE (RGB888) ---
                        case COLOR_MODE_BGR888:
                        {
                            // Use the inline logic for optimized integer blending
                            write_pixel_rgb888_alpha_inline(framebuffer, px, py, color);
                            break;
                        }
                        
                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[py * framebuffer->displayWidth + px];
                            blendPixel(fbColor, color);
                            break;
                        }
                    }
                }
            }
        }

        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}



void drawOval(Framebuffer *framebuffer, int startX, int startY, int endX, int endY, const ColorRGBA fillColor, const Gradient *gradient, const Matrix3x3 *transform, bool antiAlias) {
    // Dimensions of the oval
    int width = endX - startX;
    int height = endY - startY;

    // Center coordinates and radii
    float centerX = (startX + endX) / 2.0f;
    float centerY = (startY + endY) / 2.0f;
    float radX = width / 2.0f;
    float radY = height / 2.0f;

    // Determine gradient direction
    float dirX = gradient ? cosf(gradient->angle) : 0.0f;
    float dirY = gradient ? sinf(gradient->angle) : 0.0f;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            float dx = (j - radX) / radX;
            float dy = (i - radY) / radY;

            float distanceSquared = dx * dx + dy * dy;

            if (distanceSquared <= 1.0f) {
                ColorRGBA interpColor = fillColor; // Initialize with solid color

                if (gradient) {
                    // Calculate non-filled factor
                    float factor = ((j - startX) * dirX + (i - startY) * dirY) / width;
                    factor = fminf(fmaxf(factor, 0.0f), 1.0f);

                    ColorRGBA startColor, endColor;
                    float startPos = 0.0f, endPos = 1.0f;

                    for (int k = 0; k < gradient->numStops - 1; k++) {
                        if (factor >= gradient->stops[k].position && factor <= gradient->stops[k + 1].position) {
                            startColor = gradient->stops[k].color;
                            endColor = gradient->stops[k + 1].color;
                            startPos = gradient->stops[k].position;
                            endPos = gradient->stops[k + 1].position;
                            break;
                        }
                    }

                    float localFactor = (factor - startPos) / (endPos - startPos);
                    interpColor.r = (uint8_t)((1 - localFactor) * startColor.r + localFactor * endColor.r);
                    interpColor.g = (uint8_t)((1 - localFactor) * startColor.g + localFactor * endColor.g);
                    interpColor.b = (uint8_t)((1 - localFactor) * startColor.b + localFactor * endColor.b);
                    interpColor.a = (uint8_t)((1 - localFactor) * startColor.a + localFactor * endColor.a);
                }

                // Apply antialiasing adjustment if enabled
                float alphaCoverage = antiAlias ? fminf(1.0f, sqrtf(1.0f - distanceSquared)) : 1.0f;
                interpColor.a *= alphaCoverage;

                float pixelX = startX + j;
                float pixelY = startY + i;

                if (transform) {
                    TransformPoint(&pixelX, &pixelY, transform);
                }

                int indexX = (int)pixelX;
                int indexY = (int)pixelY;

                if (indexX >= 0 && indexX < framebuffer->displayWidth && indexY >= 0 && indexY < framebuffer->displayHeight) {
                    switch (framebuffer->colorMode) {
                        case COLOR_MODE_TWO:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (indexY * framebuffer->displayWidth + indexX) / 8;
                            int bitIndex = 7 - ((indexY * framebuffer->displayWidth + indexX) % 8);
                            if (interpColor.r == framebuffer->colors[1].r && interpColor.g == framebuffer->colors[1].g && interpColor.b == framebuffer->colors[1].b) {
                                *pixelByte |= (1 << bitIndex); // Set bit for color 1
                            } else {
                                *pixelByte &= ~(1 << bitIndex); // Clear bit for color 0
                            }
                            break;
                        }
                        case COLOR_MODE_SIXTEEN:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + (indexY * framebuffer->displayWidth + indexX) / 2;
                            int nibbleShift = ((indexY * framebuffer->displayWidth + indexX) % 2) ? 0 : 4;
                            uint8_t colorIndex = (interpColor.r >> 4) & 0xF; // Simplified mapping
                            *pixelByte = (*pixelByte & (~(0xF << nibbleShift))) | (colorIndex << nibbleShift);
                            break;
                        }
                        case COLOR_MODE_256:
                        {
                            uint8_t *pixelByte = (uint8_t *)framebuffer->pixelData + indexY * framebuffer->displayWidth + indexX;
                            *pixelByte = (interpColor.r >> 6) | ((interpColor.g >> 6) << 2) | ((interpColor.b >> 6) << 4);
                            break;
                        }
                        case COLOR_MODE_RGBA:
                        default:
                        {
                            ColorRGBA *fbColor = &((ColorRGBA*)framebuffer->pixelData)[indexY * framebuffer->displayWidth + indexX];
                            blendPixelWithAlpha(fbColor, interpColor, alphaCoverage);
                            break;
                        }
                    }
                }
            }
        }
    }
}

// --- 1. Basic Line (Bresenham) ---
void drawLine(Framebuffer* fb, int x0, int y0, int x1, int y1, ColorRGBA color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        if (x0 >= 0 && x0 < fb->displayWidth && y0 >= 0 && y0 < fb->displayHeight) {
            if (fb->colorMode == COLOR_MODE_BGR888) {
                write_pixel_rgb888(fb, x0, y0, color);
            } else {
                ColorRGBA *pixel = &((ColorRGBA*)fb->pixelData)[y0 * fb->displayWidth + x0];
                *pixel = color;
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// --- 2. Fast Circle Fill (Scanline) ---
void drawCircleFilled(Framebuffer* fb, int cx, int cy, int radius, ColorRGBA color) {
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    while (y >= x) {
        // Draw horizontal spans (much faster than setting individual pixels)
        drawLine(fb, cx - x, cy - y, cx + x, cy - y, color);
        drawLine(fb, cx - x, cy + y, cx + x, cy + y, color);
        drawLine(fb, cx - y, cy - x, cx + y, cy - x, color);
        drawLine(fb, cx - y, cy + x, cx + y, cy + x, color);

        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

// --- 3. Fast Triangle Fill (Scanline) ---
void drawTriangleFilled(Framebuffer* fb, int x0, int y0, int x1, int y1, int x2, int y2, ColorRGBA color) {
    // Sort vertices by Y
    if (y0 > y1) { int t=y0; y0=y1; y1=t; t=x0; x0=x1; x1=t; }
    if (y0 > y2) { int t=y0; y0=y2; y2=t; t=x0; x0=x2; x2=t; }
    if (y1 > y2) { int t=y1; y1=y2; y2=t; t=x1; x1=x2; x2=t; }

    int total_height = y2 - y0;
    if (total_height == 0) return;

    for (int i = 0; i < total_height; i++) {
        int y = y0 + i;
        if (y < 0 || y >= fb->displayHeight) continue;

        bool second_half = i > (y1 - y0) || (y1 == y0);
        int segment_height = second_half ? (y2 - y1) : (y1 - y0);
        
        float alpha = (float)i / total_height;
        float beta  = (float)(i - (second_half ? (y1 - y0) : 0)) / segment_height;
        
        int A = x0 + (x2 - x0) * alpha;
        int B = second_half ? (x1 + (x2 - x1) * beta) : (x0 + (x1 - x0) * beta);

        if (A > B) { int t = A; A = B; B = t; }

        for (int x = A; x <= B; x++) {
             if (x >= 0 && x < fb->displayWidth) {
                 if (fb->colorMode == COLOR_MODE_BGR888) {
                     write_pixel_rgb888(fb, x, y, color);
                 } else {
                     ColorRGBA *p = &((ColorRGBA*)fb->pixelData)[y * fb->displayWidth + x];
                     *p = color;
                 }
             }
        }
    }
}

// --- 4. The Main Hand Renderer ---
void drawRoundedHand(Framebuffer* fb, int x0, int y0, int x1, int y1, int thickness, ColorRGBA color) {
    int radius = thickness / 2;
    if (radius < 1) radius = 1;

    // 1. Draw Caps
    drawCircleFilled(fb, x0, y0, radius, color);
    drawCircleFilled(fb, x1, y1, radius, color);

    // 2. Calculate Body Rect
    float dx = (float)(x1 - x0);
    float dy = (float)(y1 - y0);
    float len = sqrtf(dx*dx + dy*dy);
    if (len == 0) return;

    float scale = (float)radius / len;
    float nx = -dy * scale;
    float ny = dx * scale;

    // 3. Draw Body (2 Triangles)
    drawTriangleFilled(fb, x0 - nx, y0 - ny, x0 + nx, y0 + ny, x1 + nx, y1 + ny, color);
    drawTriangleFilled(fb, x0 - nx, y0 - ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, color);
}

// --- HELPER: Darken a single pixel by ~50% (Fast bitshifts) ---
static inline void darkenPixelFast(Framebuffer* fb, int x, int y) {
    if (x < 0 || x >= fb->displayWidth || y < 0 || y >= fb->displayHeight) return;

    if (fb->colorMode == COLOR_MODE_BGR888) {
        // Pointer math to find the 3 bytes for this pixel
        uint8_t* p = (uint8_t*)fb->pixelData + (y * fb->displayWidth + x) * 3;
        // Shift right by 1 divides by 2 (50% brightness)
        p[0] = p[0] >> 1; // Blue
        p[1] = p[1] >> 1; // Green
        p[2] = p[2] >> 1; // Red
    }
    else if (fb->colorMode == COLOR_MODE_RGBA) {
        ColorRGBA *p = &((ColorRGBA*)fb->pixelData)[y * fb->displayWidth + x];
        p->r = p->r >> 1;
        p->g = p->g >> 1;
        p->b = p->b >> 1;
        // Keep existing alpha
    }
    // Add other color modes if necessary (e.g. RGB565)
}

// --- MAIN FUNCTION: Draw Day/Night Terminator ---
// time01: 0.0 to 1.0 represents 00:00 to 23:59 UTC.
// seasonStrength: approx -1.0 (Dec Solstice) to 1.0 (June Solstice). 0.0 is Equinox.
void drawDayNightOverlay(Framebuffer* fb, int xOffset, int yOffset, int w, int h, float time01, float seasonStrength) {
    
    int centerX = w / 2;
    // How much the curve bends depending on season. Max bend is 1/4 screen width.
    float maxBend = w / 4.0f;

    // Iterate over every horizontal line (scanline) of the map area
    for (int y = 0; y < h; y++) {
        
        // 1. Calculate Normalized Latitude for this row
        // Map Y 0..h to Latitude -PI/2..+PI/2 (South Pole to North Pole)
        // Using cosine gives a nicer looking curve projection for typical world maps
        float normalizedLat = ((float)y / (float)h) * M_PI; // 0 to PI
        float latEffect = cosf(normalizedLat); // 1.0 (South) to -1.0 (North)

        // 2. Calculate Sunrise/Sunset X coordinates for this latitude
        
        // Base day width at equator (half screen)
        float halfDayWidth = w / 4.0f;
        // Adjust width based on season and latitude (Polar nights/days)
        // In summer (season > 0), northern hemisphere (latEffect < 0) gets wider days.
        float adjustedHalfWidth = halfDayWidth - (latEffect * seasonStrength * maxBend);

        // Center of the "Day" patch moves based on time
        float timeCenterX = time01 * w;

        // Calculate boundaries, handling modulo wrapping for world map
        int sunriseX = (int)(timeCenterX - adjustedHalfWidth + w) % w;
        int sunsetX = (int)(timeCenterX + adjustedHalfWidth + w) % w;

        // 3. Fill the "Night" pixels for this row
        for (int x = 0; x < w; x++) {
            bool isNight = false;
            
            if (sunriseX < sunsetX) {
                // Standard day: [ dark | sunrise | DAY | sunset | dark ]
                if (x < sunriseX || x >= sunsetX) isNight = true;
            } else {
                // Wrap-around day: [ DAY | sunset | dark | sunrise | DAY ]
                if (x >= sunsetX && x < sunriseX) isNight = true;
            }

            if (isNight) {
                // Apply 50% darkness to the pixel relative to the map offset
                darkenPixelFast(fb, xOffset + x, yOffset + y);
            }
        }
    }
}


// Copy pixel data FROM the Framebuffer TO the Backup Buffer
void anim_save_background(Framebuffer *fb, uint8_t* backupBuffer, int x, int y, int w, int h) {
    if (!fb || !fb->pixelData || !backupBuffer) return;

    // We assume 3 bytes per pixel (RGB888 / BGR888)
    const int BPP = 3;
    
    // Pointers to the current row
    uint8_t* fb_row_ptr;
    uint8_t* backup_ptr = backupBuffer;

    for (int i = 0; i < h; i++) {
        // Calculate the starting address of this specific row in the main framebuffer
        // Formula: ( (CurrentY * ScreenWidth) + StartX ) * BytesPerPixel
        int fb_offset = ((y + i) * fb->displayWidth + x) * BPP;
        fb_row_ptr = (uint8_t*)fb->pixelData + fb_offset;

        // Copy one full row of pixels
        memcpy(backup_ptr, fb_row_ptr, w * BPP);

        // Advance the backup pointer to the next row in the linear backup buffer
        backup_ptr += (w * BPP);
    }
}

// Copy pixel data FROM the Backup Buffer BACK TO the Framebuffer
void anim_restore_background(Framebuffer *fb, uint8_t* backupBuffer, int x, int y, int w, int h) {
    if (!fb || !fb->pixelData || !backupBuffer) return;

    const int BPP = 3;
    
    uint8_t* fb_row_ptr;
    uint8_t* backup_ptr = backupBuffer;

    for (int i = 0; i < h; i++) {
        // Calculate the starting address of this specific row in the main framebuffer
        int fb_offset = ((y + i) * fb->displayWidth + x) * BPP;
        fb_row_ptr = (uint8_t*)fb->pixelData + fb_offset;

        // Copy one full row FROM backup TO framebuffer
        memcpy(fb_row_ptr, backup_ptr, w * BPP);

        // Advance the backup pointer
        backup_ptr += (w * BPP);
    }
}

