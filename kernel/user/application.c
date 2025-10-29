#include "application.h"

void app_init(Application* app, void (*func)(Application*, uint32_t appid), Window* win) {
    app->run = func;
    app->input = malloc(APPLICATION_MESSAGE_SIZE);
    app->output = malloc(APPLICATION_MESSAGE_SIZE);
    app->data = malloc(APPLICATION_MESSAGE_SIZE);
    app->window = win;
    app->terminate = NULL;
    app->save = NULL;
    app->output_state = 0;
    app->input[0] = '\0';
    app->output[0] = '\0';
}

void app_run(Application* app, uint32_t appid) {
    if (app->run)
        app->run(app, appid);
}

uint32_t MAX_APPLICATIONS = 0;
Application *apps = NULL;