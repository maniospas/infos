#include "fat32.h"
#include "disk.h"
#include "../screen/screen.h"
#include "../string.h"

static struct FAT32_BPB bpb;
static uint32_t first_data_sector;
static uint32_t fat_begin_lba;
static uint32_t cluster_size;

// Static read buffer (4 KB max)
#define BUFFER_SIZE 4096
static uint8_t file_buffer[BUFFER_SIZE];

int fat32_init() {
    if (ata_read_sector(0, &bpb) != 0)
        return -1;

    fat_begin_lba = bpb.RsvdSecCnt + bpb.HiddSec;
    first_data_sector = bpb.RsvdSecCnt + (bpb.NumFATs * bpb.FATSz32);
    cluster_size = bpb.SecPerClus * bpb.BytsPerSec;
    return 0;
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return first_data_sector + ((cluster - 2) * bpb.SecPerClus);
}

void fat32_ls_root() {
    struct FAT32_DirEntry entries[16];
    uint32_t lba = cluster_to_lba(bpb.RootClus);

    for (int s = 0; s < bpb.SecPerClus; s++) {
        ata_read_sector(lba + s, entries);
        for (int i = 0; i < 16; i++) {
            if (entries[i].Name[0] == 0x00)
                return;
            if (entries[i].Name[0] == 0xE5 || entries[i].Attr == 0x0F)
                continue;

            char name[12];
            memcpy(name, entries[i].Name, 11);
            name[11] = 0;

            for (int j = 10; j >= 0; j--)
                if (name[j] == ' ')
                    name[j] = 0;

            fb_write(name);
            fb_put_char('\n');
        }
    }
}

void fat32_cat(const char* filename) {
    struct FAT32_DirEntry entries[16];
    uint32_t lba = cluster_to_lba(bpb.RootClus);

    for (int s = 0; s < bpb.SecPerClus; s++) {
        ata_read_sector(lba + s, entries);
        for (int i = 0; i < 16; i++) {
            if (entries[i].Name[0] == 0x00)
                return;
            if (entries[i].Attr == 0x0F)
                continue;

            char name[12];
            memcpy(name, entries[i].Name, 11);
            name[11] = 0;

            for (int j = 10; j >= 0; j--)
                if (name[j] == ' ')
                    name[j] = 0;

            if (strncmp(name, filename, 11) == 0) {
                uint32_t cluster = (entries[i].FstClusHI << 16) | entries[i].FstClusLO;
                uint32_t size = entries[i].FileSize;
                uint32_t bytes_read = 0;

                while (bytes_read < size && bytes_read < BUFFER_SIZE) {
                    uint32_t sec_lba = cluster_to_lba(cluster);
                    for (int s2 = 0; s2 < bpb.SecPerClus; s2++) {
                        ata_read_sector(sec_lba + s2, file_buffer + bytes_read);
                        bytes_read += bpb.BytsPerSec;
                        if (bytes_read >= size || bytes_read >= BUFFER_SIZE)
                            break;
                    }
                    break; // no FAT traversal yet
                }

                for (uint32_t i2 = 0; i2 < bytes_read && i2 < BUFFER_SIZE; i2++)
                    fb_put_char(file_buffer[i2]);
                return;
            }
        }
    }
}
