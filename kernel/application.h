#pragma once

#include "screen/screen.h"
#include <stdint.h>

typedef struct Application {
    void (*run)(struct Application*);  // function pointer that receives itself
    void* data;                        // arbitrary data (4KB buffer)
    uint32_t data_size;                // size of data buffer
    Window* window;                    // pointer to associated window
} Application;

void app_init(Application* app, void (*func)(Application*), Window* win, size_t data_size);
void app_run(Application* app);
void widget_run(Application* app);
