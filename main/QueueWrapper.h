//
//  QueueWrapper.h
//  
//
//  Created by Chris Galzerano on 2/7/26.
//

#ifndef QUEUE_WRAPPER_H
#define QUEUE_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Universal Infinite Delay Constant
#define QUEUE_MAX_DELAY 0xFFFFFFFF

#ifdef ESP_PLATFORM
    #include "freertos/FreeRTOS.h"
    #include "freertos/queue.h"
    // On ESP, it's just a raw handle
    typedef QueueHandle_t MyQueueHandle_t;
#else
    // On Linux, it's a pointer to our wrapper struct
    typedef struct LinuxQueueWrapper* MyQueueHandle_t;
#endif

// Create
MyQueueHandle_t QueueCreate(size_t queueLength, size_t itemSize);

// Send (Matches QueueSend signature)
bool QueueSend(MyQueueHandle_t queue, const void *item, uint32_t timeout_ms);

// Receive (Matches xQueueReceive signature)
bool QueueReceive(MyQueueHandle_t queue, void *buffer, uint32_t timeout_ms);

// Destroy (Cleans up memory)
void QueueDestroy(MyQueueHandle_t queue);
/**
 * Send to queue from an ISR (Interrupt Service Routine).
 * @param queue  The queue handle
 * @param item   Pointer to the item to send
 * @param woke   (Optional) Output bool, set to true if a task was woken. Can be NULL.
 * @return       true if sent, false if full.
 */
bool QueueSendFromISR(MyQueueHandle_t queue, const void *item, int *woke);

#endif // QUEUE_WRAPPER_H
