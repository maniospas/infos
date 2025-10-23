#pragma once
#include <stddef.h>

void console_execute(const char *cmd);
void console_prompt(void);
void console_readline(char *buffer, size_t size);
