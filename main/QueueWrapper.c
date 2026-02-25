//
//  QueueWrapper.c
//  
//
//  Created by Chris Galzerano on 2/7/26.
//

#include "QueueWrapper.h"
#include <stdlib.h>
#include <stdio.h>

// =======================
// ESP32 IMPLEMENTATION
// =======================
#ifdef ESP_PLATFORM

MyQueueHandle_t QueueCreate(size_t queueLength, size_t itemSize) {
    return xQueueCreate(queueLength, itemSize);
}

bool QueueSend(MyQueueHandle_t queue, const void *item, uint32_t timeout_ms) {
    TickType_t ticks = (timeout_ms == QUEUE_MAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend(queue, item, ticks) == pdTRUE;
}

bool QueueReceive(MyQueueHandle_t queue, void *buffer, uint32_t timeout_ms) {
    TickType_t ticks = (timeout_ms == QUEUE_MAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(queue, buffer, ticks) == pdTRUE;
}

void QueueDestroy(MyQueueHandle_t queue) {
    vQueueDelete(queue);
}

// =======================
// ESP32 IMPLEMENTATION
// =======================

bool QueueSendFromISR(MyQueueHandle_t queue, const void *item, int *woke) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // 1. Call native ISR function
    BaseType_t result = xQueueSendFromISR(queue, item, &xHigherPriorityTaskWoken);
    
    // 2. Return the wake status if user asked for it
    if (woke) {
        *woke = (xHigherPriorityTaskWoken == pdTRUE);
    }
    
    // 3. Optional: If you want the wrapper to auto-yield, uncomment this:
    // if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();

    return (result == pdTRUE);
}

// =======================
// LINUX IMPLEMENTATION
// =======================
#else

#include <mqueue.h>
#include <time.h>
#include <string.h>
#include <fcntl.h> // For O_CREAT
#include <errno.h>

// The Wrapper Struct
struct LinuxQueueWrapper {
    mqd_t mq;
    size_t itemSize;
};

MyQueueHandle_t QueueCreate(size_t queueLength, size_t itemSize) {
    struct LinuxQueueWrapper *ctx = malloc(sizeof(struct LinuxQueueWrapper));
    if (!ctx) return NULL;
    
    ctx->itemSize = itemSize;

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = queueLength;
    attr.mq_msgsize = itemSize;
    attr.mq_curmsgs = 0;

    char name[64];
    snprintf(name, sizeof(name), "/q_%ld", random());

    ctx->mq = mq_open(name, O_CREAT | O_RDWR, 0644, &attr);
    
    // Unlink immediately so it disappears from the OS when we close it
    mq_unlink(name);

    if (ctx->mq == (mqd_t)-1) {
        free(ctx);
        return NULL;
    }
    return (MyQueueHandle_t)ctx;
}

// Helper for Timeouts
static void get_timeout(struct timespec *ts, uint32_t ms) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (ms % 1000) * 1000000;
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
}

bool QueueSend(MyQueueHandle_t queue, const void *item, uint32_t timeout_ms) {
    if (!queue) return false;
    struct LinuxQueueWrapper *ctx = (struct LinuxQueueWrapper *)queue;

    if (timeout_ms == QUEUE_MAX_DELAY) {
        return mq_send(ctx->mq, (const char *)item, ctx->itemSize, 0) == 0;
    } else {
        struct timespec ts;
        get_timeout(&ts, timeout_ms);
        return mq_timedsend(ctx->mq, (const char *)item, ctx->itemSize, 0, &ts) == 0;
    }
}

bool QueueReceive(MyQueueHandle_t queue, void *buffer, uint32_t timeout_ms) {
    if (!queue) return false;
    struct LinuxQueueWrapper *ctx = (struct LinuxQueueWrapper *)queue;

    ssize_t res;
    if (timeout_ms == QUEUE_MAX_DELAY) {
        res = mq_receive(ctx->mq, (char *)buffer, ctx->itemSize, NULL);
    } else {
        struct timespec ts;
        get_timeout(&ts, timeout_ms);
        res = mq_timedreceive(ctx->mq, (char *)buffer, ctx->itemSize, NULL, &ts);
    }
    return res >= 0;
}

void QueueDestroy(MyQueueHandle_t queue) {
    if (!queue) return;
    struct LinuxQueueWrapper *ctx = (struct LinuxQueueWrapper *)queue;
    
    // 1. Close the OS handle
    mq_close(ctx->mq);
    
    // 2. Free the wrapper memory (prevent leak)
    free(ctx);
}

bool QueueSendFromISR(MyQueueHandle_t queue, const void *item, int *woke) {
    // Linux doesn't have real ISRs in this context.
    // We treat it as a non-blocking send (timeout = 0).
    
    if (woke) {
        *woke = 0; // Linux threads handle scheduling automatically
    }
    
    // Reuse our existing Send function with 0 timeout
    return QueueSend(queue, item, 0);
}

#endif
