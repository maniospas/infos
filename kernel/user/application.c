#include "application.h"

void app_init(Application* app, void (*func)(Application*, int appid), Window* win, size_t data_size) {
    app->run = func;
    app->data_size = data_size;
    app->data = malloc(app->data_size);
    app->window = win;
    app->terminate = NULL;
    app->save = NULL;
}

void app_run(Application* app, int appid) {
    if (app->run)
        app->run(app, appid);
}
