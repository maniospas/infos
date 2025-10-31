#pragma once
#include <stddef.h>
#include "../screen/screen.h"
#include "../user/application.h"
#include "../string.h"
#include "../file/fat32.h"
#include "../keyboard/keyboard.h"
#include "../memory/memory.h"
#include "../io.h"

int console_execute(Application *win);
int console_command(Application *app);
void console_prompt(Window *win);
int console_readline(Window *win, char *buffer, size_t size);
void poweroff(Window *win);
uint64_t hash_str(const char* s);
int find_var(const char* name);
int find_or_insert_var(const char* name);
void widget_run(Application* app, uint32_t appid);
void widget_terminate(Application* app, uint32_t appid);
void fb_image_from_file(Window *win, int file_id, size_t target_width, size_t target_height) ;
unsigned char get_char(Window* win);
void lose_focus(Window* win);

#define MAX_HASH_VARS 1024

#define CONSOLE_EXECUTE_OK 0
#define CONSOLE_EXECUTE_OOM 1
#define CONSOLE_EXECUTE_RUNTIME_ERROR 2

typedef struct {
    char* name;
    char* value;
} VarEntry;
extern VarEntry var_table[MAX_HASH_VARS];
extern size_t var_count;