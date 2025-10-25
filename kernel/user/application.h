#pragma once
#include <stdint.h>
#include "../screen/screen.h"
#include "../memory/dynamic.h"

/*
TERMINATION PROTOCOL
- The OS calls save on applications.
  This signals that it cannot guarantee to them that they will keep running 
  (because the user wants to close them).
  These can return non-zero to REQUEST that the OS does not terminate them.
  But the OS is free to ignore this request and do whatever. It may ask the
  user to ignore for confirmation.
- The OS calls terminate on applications to let them know
  that they will never run again. They NEED to release
  any and all resources they consume (NOT their data).
*/

typedef struct Application {
    void (*run)(struct Application*, int appid);  // function pointer that receives itself
    int (*save)(struct Application*, int appid);  // a signal that we need to save data
    void (*terminate)(struct Application*, int appid);
    void* data;                        // arbitrary data (4KB buffer)
    uint32_t data_size;                // size of data buffer
    Window* window;                    // pointer to associated window
    char** vars;
    size_t MAX_VARS;
} Application;

void app_init(Application* app, void (*func)(Application*, int appid), Window* win, size_t data_size);
void app_run(Application* app, int appid);
extern Application *apps;
extern uint32_t MAX_APPLICATIONS;
extern uint32_t text_size;
