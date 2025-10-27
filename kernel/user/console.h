#pragma once
#include <stddef.h>
#include "../screen/screen.h"
#include "../user/application.h"

void console_execute(Application *win);
void console_prompt(Window *win);
int console_readline(Window *win, char *buffer, size_t size);
