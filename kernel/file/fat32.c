#include "fat32.h"
#include "disk.h"
#include "../screen/screen.h"
#include "../string.h"

#define BUFFER_SIZE 4096
static uint8_t file_buffer[BUFFER_SIZE];

static struct FAT32_BPB bpb;
static uint32_t fat_begin_lba;
static uint32_t first_data_sector;
static uint32_t cluster_size;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

#include <stdint.h>

#include "disk.h"
#include "../screen/screen.h"
#include <stdint.h>

struct __attribute__((packed)) PartitionEntry {
    uint8_t  boot_indicator;
    uint8_t  start_head;
    uint8_t  start_sector;
    uint8_t  start_cylinder;
    uint8_t  partition_type;
    uint8_t  end_head;
    uint8_t  end_sector;
    uint8_t  end_cylinder;
    uint32_t start_lba;
    uint32_t total_sectors;
};

uint32_t find_fat32_partition() {
    uint8_t mbr[512];
    if (ata_read_sector(0, mbr) != 0) 
        return 0;

    struct PartitionEntry *entries = (struct PartitionEntry *)(mbr + 446);

    for (int i = 0; i < 4; i++) 
        if (entries[i].partition_type == 0x0B || entries[i].partition_type == 0x0C) 
            return entries[i].start_lba;
    return 0;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;

        // Convert to uppercase if lowercase
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;

        if (c1 != c2)
            return (unsigned char)c1 - (unsigned char)c2;

        s1++;
        s2++;
    }

    return (unsigned char)*s1 - (unsigned char)*s2;
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return first_data_sector + ((cluster - 2) * bpb.SecPerClus);
}

static uint32_t get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_begin_lba + (fat_offset / bpb.BytsPerSec);
    uint32_t ent_offset = fat_offset % bpb.BytsPerSec;

    uint8_t sector_buf[512];
    ata_read_sector(fat_sector, sector_buf);

    uint32_t *entry = (uint32_t *)(sector_buf + ent_offset);
    uint32_t next = *entry & 0x0FFFFFFF;

    if (next >= 0x0FFFFFF8)
        return 0; // end of chain
    return next;
}

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------

int fat32_init(uint32_t partition_lba_start) {
    if (ata_read_sector(partition_lba_start, &bpb) != 0)
        return -1;

    fat_begin_lba = partition_lba_start + bpb.RsvdSecCnt;
    first_data_sector = partition_lba_start + bpb.RsvdSecCnt + (bpb.NumFATs * bpb.FATSz32);
    cluster_size = bpb.SecPerClus * bpb.BytsPerSec;

    fb_write("FAT32 initialized.\n");
    return 0;
}

// -----------------------------------------------------------------------------
// Directory listing
// -----------------------------------------------------------------------------

void fat32_ls_root() {
    uint32_t root_cluster = bpb.RootClus;
    uint32_t lba = cluster_to_lba(root_cluster);

    uint8_t sector_buf[512];
    int entries_per_sector = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);

    while (root_cluster != 0) {
        for (int s = 0; s < bpb.SecPerClus; s++) {
            ata_read_sector(lba + s, sector_buf);
            struct FAT32_DirEntry *entries = (struct FAT32_DirEntry *)sector_buf;

            for (int i = 0; i < entries_per_sector; i++) {
                struct FAT32_DirEntry *e = &entries[i];
                if (e->Name[0] == 0x00)
                    return;
                if (e->Name[0] == 0xE5 || e->Attr == 0x0F)
                    continue;

                char name[12];
                memcpy(name, e->Name, 11);
                name[11] = 0;
                for (int j = 10; j >= 0; j--)
                    if (name[j] == ' ')
                        name[j] = 0;

                fb_write(name);
                fb_put_char('\n');
            }
        }
        root_cluster = get_next_cluster(root_cluster);
        lba = cluster_to_lba(root_cluster);
    }
}

// -----------------------------------------------------------------------------
// File reading
// -----------------------------------------------------------------------------

void fat32_cat(const char *filename) {
    uint32_t root_cluster = bpb.RootClus;
    uint8_t sector_buf[512];
    int entries_per_sector = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);

    while (root_cluster != 0) {
        uint32_t lba = cluster_to_lba(root_cluster);
        for (int s = 0; s < bpb.SecPerClus; s++) {
            ata_read_sector(lba + s, sector_buf);
            struct FAT32_DirEntry *entries = (struct FAT32_DirEntry *)sector_buf;

            for (int i = 0; i < entries_per_sector; i++) {
                struct FAT32_DirEntry *e = &entries[i];
                if (e->Name[0] == 0x00)
                    return;
                if (e->Attr == 0x0F)
                    continue;

                char name[12];
                memcpy(name, e->Name, 11);
                name[11] = 0;
                for (int j = 10; j >= 0; j--)
                    if (name[j] == ' ')
                        name[j] = 0;

                if (strcasecmp(name, filename) == 0) {
                    uint32_t cluster = (e->FstClusHI << 16) | e->FstClusLO;
                    uint32_t size = e->FileSize;
                    uint32_t bytes_read = 0;

                    while (cluster != 0 && bytes_read < size && bytes_read < BUFFER_SIZE) {
                        uint32_t sec_lba = cluster_to_lba(cluster);
                        for (int s2 = 0; s2 < bpb.SecPerClus; s2++) {
                            ata_read_sector(sec_lba + s2, file_buffer + bytes_read);
                            bytes_read += bpb.BytsPerSec;
                            if (bytes_read >= size || bytes_read >= BUFFER_SIZE)
                                break;
                        }
                        cluster = get_next_cluster(cluster);
                    }

                    for (uint32_t i2 = 0; i2 < bytes_read && i2 < size; i2++)
                        fb_put_char(file_buffer[i2]);
                    fb_put_char('\n');
                    return;
                }
            }
        }
        root_cluster = get_next_cluster(root_cluster);
    }

    fb_write("File not found.\n");
}
