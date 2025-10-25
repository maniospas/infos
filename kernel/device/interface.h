#pragma once
#include <stdint.h>
#include <stddef.h>

#define DEVICE_DISK 1

typedef struct Disk {
    const long device_type;
    const char *name;
    const char *error;
    char *internal_buffer; // only used for internal operations
    uint64_t internal_buffer_size; // only used for internal operations or diagnostics

    uint32_t root; // file handle corresponding to root
    const uint32_t (*open_file)(const char*);
    const uint32_t (*open_dir)(const char*);
    const
} Disk;

void* devices;