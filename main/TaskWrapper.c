//
//  TaskWrapper.c
//  
//
//  Created by Chris Galzerano on 2/7/26.
//

#define _GNU_SOURCE // For core pinning
#include "TaskWrapper.h"
#include <stdio.h>
#include <stdlib.h>

// ==========================================
// ESP32 IMPLEMENTATION (Native vTaskDelete)
// ==========================================
#ifdef ESP_PLATFORM

// Internal wrapper to handle argument passing
typedef struct {
    TaskFunction_t userFunc;
    void *userArgs;
} EspArgs;

static void esp_task_entry(void *arg) {
    EspArgs *p = (EspArgs *)arg;
    p->userFunc(p->userArgs);
    free(p);
    vTaskDelete(NULL); // Suicide if function returns
}

TaskContext_t TaskStart(const char *name, uint32_t stackSize, int priority, TaskFunction_t taskFunc, void *args, int coreID) {
    EspArgs *p = malloc(sizeof(EspArgs));
    p->userFunc = taskFunc;
    p->userArgs = args;

    TaskHandle_t handle;
    BaseType_t res;

    if (coreID >= 0) {
        res = xTaskCreatePinnedToCore(esp_task_entry, name, stackSize, p, priority, &handle, coreID);
    } else {
        res = xTaskCreate(esp_task_entry, name, stackSize, p, priority, &handle);
    }

    if (res != pdPASS) {
        free(p);
        return NULL;
    }
    return handle;
}

void TaskDelete(TaskContext_t task) {
    if (task) {
        vTaskDelete(task); // NATIVE ESP32 DELETE
    }
}

bool TaskShouldExit(void) {
    // On ESP32, vTaskDelete kills the task instantly.
    // The code never reaches this check if it's dead.
    return false;
}

void TaskDelay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// ==========================================
// LINUX IMPLEMENTATION (Flag + Join)
// ==========================================
#else

#include <unistd.h>
#include <sched.h>
#include <time.h>

struct LinuxTaskState {
    pthread_t thread;
    bool stopFlag;
    TaskFunction_t userFunc;
    void *userArgs;
};

// Thread-Local Storage to track current task context
static __thread struct LinuxTaskState* g_current_ctx = NULL;

static void *linux_thread_entry(void *arg) {
    struct LinuxTaskState *ctx = (struct LinuxTaskState *)arg;
    g_current_ctx = ctx; // Save context to TLS
    
    ctx->userFunc(ctx->userArgs); // Run user code
    
    return NULL;
}

TaskContext_t TaskStart(const char *name, uint32_t stackSize, int priority, TaskFunction_t taskFunc, void *args, int coreID) {
    struct LinuxTaskState *ctx = malloc(sizeof(struct LinuxTaskState));
    if (!ctx) return NULL;

    ctx->stopFlag = false;
    ctx->userFunc = taskFunc;
    ctx->userArgs = args;

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    // 1. Scale Stack
    size_t safeStack = (stackSize < 16384) ? 16384 : stackSize;
    pthread_attr_setstacksize(&attr, safeStack);

    // 2. Core Pinning
    if (coreID >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreID, &cpuset);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
    }

    if (pthread_create(&ctx->thread, &attr, linux_thread_entry, ctx) != 0) {
        free(ctx);
        pthread_attr_destroy(&attr);
        return NULL;
    }
    
    pthread_attr_destroy(&attr);
    return ctx;
}

void TaskDelete(TaskContext_t task) {
    if (!task) return;

    // 1. Signal Stop
    task->stopFlag = true;

    // 2. Wait for graceful exit
    pthread_join(task->thread, NULL);

    // 3. Free Memory
    free(task);
}

bool TaskShouldExit(void) {
    if (g_current_ctx) {
        return g_current_ctx->stopFlag;
    }
    return false;
}

void TaskDelay(uint32_t ms) {
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
}

#endif
