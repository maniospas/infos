#include "application.h"
#include "memory/dynamic.h"

void app_init(Application* app, void (*func)(Application*), Window* win, size_t data_size) {
    app->run = func;
    app->data_size = data_size;
    app->data = malloc(app->data_size);
    app->window = win;
}

void app_run(Application* app) {
    if (app->run)
        app->run(app);
}
