#pragma once
#include <stdint.h>

// Reads a 512-byte sector from the disk at given LBA into buffer.
// Returns 0 on success, -1 on failure.
int ata_read_sector(uint32_t lba, void *buffer);
