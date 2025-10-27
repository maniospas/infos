#pragma once
#include <stdint.h>
#include "../screen/screen.h"

struct FAT32_BPB {
    uint8_t  jmpBoot[3];
    uint8_t  OEMName[8];
    uint16_t BytsPerSec;
    uint8_t  SecPerClus;
    uint16_t RsvdSecCnt;
    uint8_t  NumFATs;
    uint16_t RootEntCnt;
    uint16_t TotSec16;
    uint8_t  Media;
    uint16_t FATSz16;
    uint16_t SecPerTrk;
    uint16_t NumHeads;
    uint32_t HiddSec;
    uint32_t TotSec32;
    uint32_t FATSz32;
    uint16_t ExtFlags;
    uint16_t FSVer;
    uint32_t RootClus;
    uint16_t FSInfo;
    uint16_t BkBootSec;
    uint8_t  Reserved[12];
    uint8_t  DrvNum;
    uint8_t  Reserved1;
    uint8_t  BootSig;
    uint32_t VolID;
    uint8_t  VolLab[11];
    uint8_t  FilSysType[8];
} __attribute__((packed));

struct FAT32_DirEntry {
    uint8_t  Name[11];
    uint8_t  Attr;
    uint8_t  NTRes;
    uint8_t  CrtTimeTenth;
    uint16_t CrtTime;
    uint16_t CrtDate;
    uint16_t LstAccDate;
    uint16_t FstClusHI;
    uint16_t WrtTime;
    uint16_t WrtDate;
    uint16_t FstClusLO;
    uint32_t FileSize;
} __attribute__((packed));

struct FAT32_Usage {
    uint32_t used_mb;
    uint32_t total_mb;
};

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

int fat32_init(uint32_t partition_lba_start);
uint32_t find_fat32_partition();
void fat32_ls(Window* win, uint32_t dir_cluster);
void fat32_cd(Window* win, const char *path);
void fat32_cat(Window* win, const char *filename);
struct FAT32_Usage fat32_get_usage(void);

// Accessors
uint32_t fat32_get_current_dir(void);
void fat32_set_current_dir(uint32_t cluster);
const char *fat32_get_current_path(void);
int fat32_read(char*buf,size_t bufsize,size_t start_page,const char*path);