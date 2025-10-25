#pragma once
#include <stddef.h>
#include "screen/screen.h"

void console_execute(Window *win, const char *cmd, char** vars, size_t MAX_VARS);
void console_prompt(Window *win);
void console_readline(Window *win, char *buffer, size_t size);
