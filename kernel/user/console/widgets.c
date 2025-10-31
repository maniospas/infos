#include "../console.h"

void poweroff(Window *win) {
    fb_write_ansi(win, "\x1b[32mShutting down...\x1b[0m\n");
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    for (;;) __asm__("hlt");
}

extern uint32_t margin;
extern uint32_t focus_id;

unsigned char get_char(Window* win) {
    if(win!=apps[focus_id].window) 
        return '\0';
    while(1) {
        unsigned char key = keyboard_read();
        if (!key) { __asm__("hlt"); continue; }
        else return key;
    }
    return 0;
}

void lose_focus(Window* win) {
    if(win==apps[focus_id].window) 
        focus_id = 0;
}

void console_prompt(Window* win) {
    fb_write_ansi(win, "\x1b[33m");
    fb_write(win, fat32_get_current_path());
    if(focus_id) {
        fb_write(win, " [app");
        fb_write_dec(win, focus_id);
        fb_write(win, "] ");
    }
    else
        fb_write(win, ": ");
    fb_write_ansi(win, "\x1b[0m");
}
void widget_run(Application* app, uint32_t appid) {
    if (!app->window || !app->window->width || !app->window->height)
        return;
    if(appid && focus_id!=appid) 
        app->window->bg_color = app->window->DEFAULT_BG = 0X3F3F3F;
    else
        app->window->bg_color = app->window->DEFAULT_BG = 0X1F1F1F;
    fb_clear(app->window);
    fb_set_scale(app->window, 1, 1);
    if(appid && focus_id!=appid) {
        uint32_t saved_fg = app->window->DEFAULT_FG;
        app->window->DEFAULT_FG = 0xAAAAAA;
        fb_window_border(app->window, "", 0x444444, appid);
        app->window->DEFAULT_FG = saved_fg;
    }
    else 
        fb_window_border(app->window, "", 0x000000, appid);
    fb_set_scale(app->window, 20, 16);
    console_execute(app);
    //fb_put_char(app->window, '\n');
    lose_focus(app->window); // terminate our app by always losing focus anyway
}


void widget_terminate(Application* app, uint32_t appid) {
    if(!app->window || !app->window->width || !app->window->height)
        return;
    uint32_t prev = app->window->bg_color;
    app->window->bg_color = 0;
    app->window->y -= 60;
    app->window->height += 100;
    app->window->width += 40;
    fb_clear(app->window);
    app->window->y += 60;
    app->window->height -= 100;
    app->window->width -= 40;
    app->window->bg_color = prev;
}
