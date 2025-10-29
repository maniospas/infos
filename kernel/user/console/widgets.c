#include "../console.h"

void poweroff(Window *win) {
    fb_write_ansi(win, "\x1b[32mShutting down...\x1b[0m\n");
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    for (;;) __asm__("hlt");
}

extern uint32_t margin;
extern uint32_t focus_id;

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

    unsigned char key = 0;
    if (appid && focus_id == appid) {
        for(;;) {
            key = keyboard_read();
            if (!key) { 
                __asm__("hlt"); 
                continue; 
            }
            else
                break;
        }
        if (key == 0x01) { // ESC key
            focus_id = 0;
        }
    }


    if(appid && focus_id!=appid) 
        app->window->bg_color = app->window->DEFAULT_BG = 0X3F3F3F;
    else
        app->window->bg_color = app->window->DEFAULT_BG = 0X1F1F1F;
        
    fb_clear(app->window);
    fb_set_scale(app->window, 2, 1);
    if(appid && focus_id!=appid) {
        uint32_t saved_fg = app->window->DEFAULT_FG;
        app->window->DEFAULT_FG = 0xAAAAAA;
        fb_window_border(app->window, " ", 0x444444, appid);
        app->window->DEFAULT_FG = saved_fg;
    }
    else 
        fb_window_border(app->window, "Esc: lose focus", 0x000000, appid);
    fb_set_scale(app->window, 3, 2);
    console_execute(app);
    fb_put_char(app->window, '\n');

    if(app->window->scroll_limit) {
        if (key == KEY_ARROW_UP) {
            app->window->scroll_limit -= app->window->height; // scroll up
            if (app->window->scroll_limit < 1)
                app->window->scroll_limit = 1;
        } 
        else if (key == KEY_ARROW_DOWN ){
            if(app->window->accumulated_scroll_limit)
                app->window->scroll_limit += app->window->height; // scroll down
        }
    }

    // scrolling
    long scroll_pos  = 0;
    long scroll_size = 500;  // % thumb size
    if (app->window->scroll_limit) {
        long scroll_limit = app->window->scroll_limit+app->window->height;
        long scrolled     = app->window->accumulated_scroll_limit;
        if (scrolled < 0) scrolled = 0;
        if (scrolled > scroll_limit) scrolled = scroll_limit;
        scroll_pos = 10000-(scrolled * 10000) / scroll_limit;
        if (scroll_pos < 0) scroll_pos = 0;
        if (scroll_pos > 10000) scroll_pos = 10000;
        if (scroll_size < 500)   scroll_size = 500;   // min 5%
        if (scroll_size > 10000) scroll_size = 10000; // max 100%
    }
    if (app->window->accumulated_scroll_limit)
        fb_scrollbar(app->window, scroll_pos, scroll_size);
    app->window->accumulated_scroll_limit = 0;
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
