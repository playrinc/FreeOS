//
//  TaskWrapper.h
//  
//
//  Created by Chris Galzerano on 2/7/26.
//

#ifndef TASK_WRAPPER_H
#define TASK_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef ESP_PLATFORM
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    typedef TaskHandle_t TaskContext_t;
#else
    #include <pthread.h>
    // Linux: A pointer to our control struct
    typedef struct LinuxTaskState* TaskContext_t;
#endif

// Function Pointer Type
typedef void (*TaskFunction_t)(void *);

/**
 * Start a Task
 * @param name       Debug name
 * @param stackSize  Stack size in bytes (Auto-scales on Linux)
 * @param priority   FreeRTOS priority (Ignored on Linux)
 * @param taskFunc   The function to run
 * @param args       User arguments
 * @param coreID     Core to pin to (-1 for no pinning)
 */
TaskContext_t TaskStart(const char *name,
                        uint32_t stackSize,
                        int priority,
                        TaskFunction_t taskFunc,
                        void *args,
                        int coreID);

/**
 * Stop/Delete a Task
 * @param task       The handle returned by TaskStart
 * * On ESP32: Calls vTaskDelete(task).
 * WARNING: Immediate termination (unsafe for files/mutexes).
 * * On Linux: Sets a flag, waits for the thread to exit, then cleans memory.
 * Requires the task loop to check TaskShouldExit().
 */
void TaskDelete(TaskContext_t task);

/**
 * Check if we should exit (For Linux compatibility)
 * On ESP32 this always returns false (since vTaskDelete kills you instantly).
 */
bool TaskShouldExit(void);

/**
 * Sleep (ms)
 */
void TaskDelay(uint32_t ms);

#endif
