#pragma once
#include <stdint.h>

int file_read(const char* path, void* buffer, uint32_t size);
void file_list();
