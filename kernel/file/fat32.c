#include "fat32.h"
#include "disk.h"
#include "../string.h"

#define BUFFER_SIZE   4096
#define MAX_PATH_LEN  256

static uint8_t file_buffer[BUFFER_SIZE];
static char current_dir_path[MAX_PATH_LEN] = "/home";
const char *fat32_get_current_path(void) { return current_dir_path; }

// --- FAT globals ---
static struct FAT32_BPB bpb;
static uint32_t fat_begin_lba;
static uint32_t first_data_sector;
static uint32_t cluster_size;
static uint32_t current_dir_cluster = 0;

// -----------------------------------------------------------------------------
// On-disk structures for LFN
// -----------------------------------------------------------------------------
struct FAT32_LFN_Entry {
    uint8_t  Ord;
    uint16_t Name1[5];
    uint8_t  Attr;
    uint8_t  Type;
    uint8_t  Checksum;
    uint16_t Name2[6];
    uint16_t Zero;
    uint16_t Name3[2];
} __attribute__((packed));

// -----------------------------------------------------------------------------
// Disk/FAT helpers
// -----------------------------------------------------------------------------
static inline uint32_t cluster_to_lba(uint32_t cluster) {
    return first_data_sector + ((cluster - 2) * bpb.SecPerClus);
}

static inline int32_t get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_begin_lba + (fat_offset / bpb.BytsPerSec);
    uint32_t ent_offset = fat_offset % bpb.BytsPerSec;
    uint8_t sector_buf[512];
    ata_read_sector(fat_sector, sector_buf);
    uint32_t *entry = (uint32_t *)(sector_buf + ent_offset);
    uint32_t next = *entry & 0x0FFFFFFF;
    return (next >= 0x0FFFFFF8) ? 0 : next;
}

// Build a short 8.3 lowercase name into out[13]
static void make_short_name_lower(const uint8_t Name[11], char out[13]) {
    char base[9], ext[4];
    int i, p = 0;
    for (i = 0; i < 8; i++) base[i] = Name[i];
    base[8] = 0;
    for (i = 7; i >= 0 && base[i] == ' '; i--) base[i] = 0;
    for (i = 0; i < 3; i++) ext[i] = Name[8+i];
    ext[3] = 0;
    for (i = 2; i >= 0 && ext[i] == ' '; i--) ext[i] = 0;
    out[0] = 0;
    for (i = 0; base[i]; i++) str_append_char(out, &p, to_lower(base[i]), 13);
    if (ext[0]) {
        str_append_char(out, &p, '.', 13);
        for (i = 0; ext[i]; i++) str_append_char(out, &p, to_lower(ext[i]), 13);
    }
    out[p] = 0;
}

// Append up to 13 chars from an LFN entry (UTF-16 -> ASCII) to the **front** of buffer
static void lfn_prepend_piece(char *lfn_buf, const struct FAT32_LFN_Entry *lfn, int maxlen) {
    char temp[14]; int pos = 0;
    int j;
    for (j = 0; j < 5; j++)   { uint16_t c = lfn->Name1[j]; if (!c || c == 0xFFFF) break; temp[pos++] = (char)c; }
    for (j = 0; j < 6; j++)   { uint16_t c = lfn->Name2[j]; if (!c || c == 0xFFFF) break; temp[pos++] = (char)c; }
    for (j = 0; j < 2; j++)   { uint16_t c = lfn->Name3[j]; if (!c || c == 0xFFFF) break; temp[pos++] = (char)c; }
    temp[pos] = 0;

    // prepend temp to lfn_buf
    char merged[256];
    int i = 0, k = 0;
    while (temp[i] && i < maxlen - 1) { merged[i] = temp[i]; i++; }
    while (lfn_buf[k] && i < maxlen - 1) { merged[i++] = lfn_buf[k++]; }
    merged[i] = 0;
    str_copy(lfn_buf, merged, maxlen);
}

// Choose display/match name (LFN if present, else short)
static const char* choose_name(const char *lfn, const char *shortnm) {
    return (lfn && lfn[0]) ? lfn : shortnm;
}

// Compare ASCII case-insensitive (use your provided strcmp/strcasecmp if available)
int strcasecmp(const char *s1, const char *s2);

// -----------------------------------------------------------------------------
// MBR partition search (unchanged)
// -----------------------------------------------------------------------------
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
    if (ata_read_sector(0, mbr) != 0) return 0;
    struct PartitionEntry *entries = (struct PartitionEntry *)(mbr + 446);
    for (int i = 0; i < 4; i++)
        if (entries[i].partition_type == 0x0B || entries[i].partition_type == 0x0C)
            return entries[i].start_lba;
    return 0;
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------
int fat32_init(uint32_t partition_lba_start) {
    uint8_t sector[512];
    if (ata_read_sector(partition_lba_start, sector) != 0) return -1;

    memcpy(&bpb, sector, sizeof(struct FAT32_BPB));
    if (bpb.BytsPerSec != 512 || bpb.SecPerClus == 0 || bpb.NumFATs == 0) return -1;

    fat_begin_lba      = partition_lba_start + bpb.RsvdSecCnt;
    first_data_sector  = partition_lba_start + bpb.RsvdSecCnt + (bpb.NumFATs * bpb.FATSz32);
    cluster_size       = bpb.SecPerClus * bpb.BytsPerSec;
    current_dir_cluster = bpb.RootClus;
    str_copy(current_dir_path, "/home", MAX_PATH_LEN);
    return 0;
}

// -----------------------------------------------------------------------------
// ls with LFN support
// -----------------------------------------------------------------------------
void fat32_ls(Window* win, uint32_t dir_cluster) {
    uint32_t cluster = dir_cluster;
    uint8_t sector_buf[512];
    int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);
    while (cluster != 0) {
        uint32_t lba = cluster_to_lba(cluster);
        for (int s = 0; s < bpb.SecPerClus; s++) {
            ata_read_sector(lba + s, sector_buf);
            struct FAT32_DirEntry *e = (struct FAT32_DirEntry *)sector_buf;
            char lfn_buf[256]; lfn_buf[0] = 0;
            for (int i = 0; i < eps; i++) {
                if (e[i].Name[0] == 0x00) { return; }
                if (e[i].Name[0] == 0xE5) { lfn_buf[0] = 0; continue; }
                if (e[i].Attr == 0x0F) {
                    // LFN entry piece
                    const struct FAT32_LFN_Entry *lfn = (const struct FAT32_LFN_Entry *)&e[i];
                    lfn_prepend_piece(lfn_buf, lfn, sizeof(lfn_buf));
                    continue;
                }
                // Normal entry
                char shortnm[13]; make_short_name_lower(e[i].Name, shortnm);
                const char *disp = choose_name(lfn_buf, shortnm);
                if (e[i].Attr & 0x10) fb_write_ansi(win, "\033[33m");
                fb_write(win, disp);
                if (e[i].Attr & 0x10) fb_write_ansi(win, "\033[0m");
                else {
                    fb_write(win, " ");
                    fb_write_ansi(win, "\033[36m");
                    fb_write_dec(win, e[i].FileSize);
                    fb_write_ansi(win, " bytes\033[0m");
                }
                fb_write(win, "\n");
                lfn_buf[0] = 0; // reset for next
            }
        }
        cluster = get_next_cluster(cluster);
    }
}

// -----------------------------------------------------------------------------
// cd with LFN support (+ /home handling and clean "..")
// -----------------------------------------------------------------------------
static void reset_to_home(void) { str_copy(current_dir_path, "/home", MAX_PATH_LEN); current_dir_cluster = bpb.RootClus; }

void fat32_cd(Window *win, const char *path) {
    char segment[64]; int seg_i = 0;

    // If empty, "/" alone, or explicit "/home" or "/home/..." => treat as root (/home)
    if (!path || !path[0] || (path[0] == '/' && !path[1]) || !strcmp(path, "/home") || str_starts_with(path, "/home/")) {
        reset_to_home();
        // If it's "/home/..." continue consuming the pieces after "/home/"
        if (path && str_starts_with(path, "/home/")) path += 6; else return;
    }

    // Absolute non-home path: start from root anyway
    if (path[0] == '/') { reset_to_home(); path++; }

    for (int i = 0;; i++) {
        char c = path[i];
        if (c == '/' || c == '\0') {
            segment[seg_i] = 0;
            if (seg_i > 0) {
                if (!strcmp(segment, ".")) {
                    // nothing
                } else if (!strcmp(segment, "..")) {
                    // back one level; never go above /home and no trailing slash
                    if (current_dir_cluster != bpb.RootClus) {
                        // cluster: follow ".." entry
                        uint8_t sb[512]; int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);
                        uint32_t temp = current_dir_cluster; int found = 0;
                        while (temp && !found) {
                            uint32_t lba = cluster_to_lba(temp);
                            for (int s = 0; s < bpb.SecPerClus && !found; s++) {
                                ata_read_sector(lba + s, sb);
                                struct FAT32_DirEntry *de = (struct FAT32_DirEntry *)sb;
                                for (int k = 0; k < eps; k++) {
                                    if (de[k].Name[0] == 0x00) break;
                                    if (de[k].Attr == 0x0F) continue;
                                    if (de[k].Name[0] == '.' && de[k].Name[1] == '.') {
                                        current_dir_cluster = (de[k].FstClusHI << 16) | de[k].FstClusLO;
                                        found = 1; break;
                                    }
                                }
                            }
                            if (!found) temp = get_next_cluster(temp);
                        }
                        // Trim current_dir_path back one component
                        int len = str_length(current_dir_path);
                        if (len > 1 && current_dir_path[len - 1] == '/') len--;
                        while (len > 1 && current_dir_path[len - 1] != '/') len--;
                        if (len > 1 && current_dir_path[len - 1] == '/') len--;
                        if (len <= 5) reset_to_home();
                        else current_dir_path[len] = 0; // ensures no trailing '/'
                    }
                } else {
                    // descend into a subdir by matching LFN or 8.3
                    uint8_t sb[512]; int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);
                    uint32_t found_cluster = 0;
                    uint32_t scan = current_dir_cluster;
                    while (scan && !found_cluster) {
                        uint32_t lba = cluster_to_lba(scan);
                        for (int s = 0; s < bpb.SecPerClus && !found_cluster; s++) {
                            ata_read_sector(lba + s, sb);
                            struct FAT32_DirEntry *e = (struct FAT32_DirEntry *)sb;

                            char lfn_buf[256]; lfn_buf[0] = 0;
                            for (int k = 0; k < eps; k++) {
                                if (e[k].Name[0] == 0x00) break;
                                if (e[k].Name[0] == 0xE5) { lfn_buf[0] = 0; continue; }

                                if (e[k].Attr == 0x0F) {
                                    const struct FAT32_LFN_Entry *lfn = (const struct FAT32_LFN_Entry *)&e[k];
                                    lfn_prepend_piece(lfn_buf, lfn, sizeof(lfn_buf));
                                    continue;
                                }

                                // Regular entry
                                if (!(e[k].Attr & 0x10)) { lfn_buf[0] = 0; continue; } // only dirs

                                char shortnm[13]; make_short_name_lower(e[k].Name, shortnm);
                                const char *cand = choose_name(lfn_buf, shortnm);

                                // Lowercase compare (segment assumed ASCII)
                                int match = !strcasecmp(cand, segment);
                                if (match) {
                                    found_cluster = (e[k].FstClusHI << 16) | e[k].FstClusLO;
                                    break;
                                }
                                lfn_buf[0] = 0;
                            }
                        }
                        if (!found_cluster) scan = get_next_cluster(scan);
                    }

                    if (!found_cluster) {
                        fb_write_ansi(win, "\n\033[31m  ERROR\033[0m No such directory: ");
                        fb_write(win, segment);
                        fb_write(win, "\n");
                        return;
                    }

                    // Update cluster and path safely
                    current_dir_cluster = found_cluster;
                    int plen = str_length(current_dir_path);
                    if (plen < MAX_PATH_LEN - 1 && current_dir_path[plen - 1] != '/')
                        current_dir_path[plen++] = '/';
                    for (int p = 0; segment[p] && plen < MAX_PATH_LEN - 1; p++)
                        current_dir_path[plen++] = segment[p];
                    current_dir_path[plen] = 0;
                }
                seg_i = 0;
            }
            if (c == '\0') break;
        } else if (seg_i < (int)sizeof(segment) - 1) {
            segment[seg_i++] = c;
        }
    }
}

// -----------------------------------------------------------------------------
void fat32_cat(Window* win, const char *filename) {
    uint32_t cluster = current_dir_cluster;
    uint8_t sector_buf[512];
    int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);

    // Detect if the file has an extension
    const char *dot = strfindlastdot(filename);
    int has_extension = (dot && *(dot + 1) != 0);

    while (cluster != 0) {
        uint32_t lba = cluster_to_lba(cluster);
        for (int s = 0; s < bpb.SecPerClus; s++) {
            ata_read_sector(lba + s, sector_buf);
            struct FAT32_DirEntry *e = (struct FAT32_DirEntry *)sector_buf;

            char lfn_buf[256]; lfn_buf[0] = 0;
            for (int i = 0; i < eps; i++) {
                if (e[i].Name[0] == 0x00) {
                    fb_write_ansi(win, "\n\033[31m  ERROR\033[0m No such file: ");
                    fb_write(win, filename);
                    fb_write(win, "\n");
                    return;
                }
                if (e[i].Name[0] == 0xE5) { lfn_buf[0] = 0; continue; }
                if (e[i].Attr == 0x0F) {
                    const struct FAT32_LFN_Entry *lfn = (const struct FAT32_LFN_Entry *)&e[i];
                    lfn_prepend_piece(lfn_buf, lfn, sizeof(lfn_buf));
                    continue;
                }
                // Regular
                char shortnm[13]; make_short_name_lower(e[i].Name, shortnm);
                const char *cand = choose_name(lfn_buf, shortnm);
                if (!strcasecmp(cand, filename) && !(e[i].Attr & 0x10)) {
                    uint32_t fclus = (e[i].FstClusHI << 16) | e[i].FstClusLO;
                    uint32_t size  = e[i].FileSize;
                    uint32_t bytes_read = 0;
                    while (fclus && bytes_read < size && bytes_read < BUFFER_SIZE) {
                        uint32_t sec_lba = cluster_to_lba(fclus);
                        for (int s2 = 0; s2 < bpb.SecPerClus; s2++) {
                            ata_read_sector(sec_lba + s2, file_buffer + bytes_read);
                            bytes_read += bpb.BytsPerSec;
                            if (bytes_read >= size || bytes_read >= BUFFER_SIZE) break;
                        }
                        fclus = get_next_cluster(fclus);
                    }

                    // Display with or without highlighting
                    if (!has_extension) {
                        fb_write(win, "\n");
                        char *ptr = (char *)file_buffer;
                        uint32_t i = 0;
                        while (i < size && i < bytes_read) {
                            char c = ptr[i];

                            // Gray comments (#)
                            if (c == '#') {
                                fb_write_ansi(win, "\x1b[90m");
                                while (i < size && ptr[i] != '\n') {
                                    fb_put_char(win, ptr[i++]);
                                }
                                fb_write_ansi(win, "\x1b[0m");
                                if (i < size) {
                                    char c = ptr[i++];
                                    if(c=='\n')
                                        fb_write(win, "\n");
                                    else
                                        fb_put_char(win, '\n');
                                }
                                continue;
                            }

                            // Green for keywords and braces
                            if (!strncmp(&ptr[i], "let", 3) && !is_alnum(ptr[i+3])) {
                                fb_write_ansi(win, "\x1b[32mlet\x1b[0m");
                                i += 3;
                                continue;
                            }
                            if (!strncmp(&ptr[i], "loop", 4) && !is_alnum(ptr[i+4])) {
                                fb_write_ansi(win, "\x1b[32mloop\x1b[0m");
                                i += 4;
                                continue;
                            }
                            if (!strncmp(&ptr[i], "if", 2) && !is_alnum(ptr[i+2])) {
                                fb_write_ansi(win, "\x1b[32mloop\x1b[0m");
                                i += 2;
                                continue;
                            }
                            if (!strncmp(&ptr[i], "return", 6) && !is_alnum(ptr[i+6])) {
                                fb_write_ansi(win, "\x1b[32mreturn\x1b[0m");
                                i += 6;
                                continue;
                            }
                            if (c == '{' || c == '}') {
                                fb_write_ansi(win, "\x1b[32m");
                                fb_put_char(win, c);
                                fb_write_ansi(win, "\x1b[0m");
                                i++;
                                continue;
                            }

                            // Yellow for parentheses
                            if (c == '(' || c == ')' || c==':') {
                                fb_write_ansi(win, "\x1b[33m");
                                fb_put_char(win, c);
                                fb_write_ansi(win, "\x1b[0m");
                                i++;
                                continue;
                            }
                            if (c == '\n') {
                                fb_write(win, "\n  ");
                                i++;
                                continue;
                            }

                            // Default
                            fb_put_char(win, c);
                            i++;
                        }
                        fb_write(win, "\n");
                    } else {
                        // Normal display for files with extensions
                        for (uint32_t k = 0; k < bytes_read && k < size; k++)
                            fb_put_char(win, file_buffer[k]);
                    }
                    fb_write(win, "\n");
                    return;
                }

                lfn_buf[0] = 0;
            }
        }
        cluster = get_next_cluster(cluster);
    }
    fb_write_ansi(win, "\x1b[32mERROR\x1b[0m File not found: ");
    fb_write(win, filename);
    fb_write(win, "\n");
}

// -----------------------------------------------------------------------------
// Usage
// -----------------------------------------------------------------------------
struct FAT32_Usage fat32_get_usage(void) {
    uint8_t sector[512];
    uint32_t free_clusters = 0;
    for (uint32_t s = 0; s < bpb.FATSz32; s++) {
        if (ata_read_sector(fat_begin_lba + s, sector) != 0) break;
        uint32_t *entry = (uint32_t *)sector;
        for (uint32_t i = 0; i < bpb.BytsPerSec / 4; i++)
            if ((*entry++ & 0x0FFFFFFF) == 0) free_clusters++;
    }
    uint32_t data_sectors   = bpb.TotSec32 - (bpb.RsvdSecCnt + (bpb.NumFATs * bpb.FATSz32));
    uint32_t total_clusters = bpb.SecPerClus ? data_sectors / bpb.SecPerClus : 0;
    uint32_t cluster_bytes  = bpb.SecPerClus * bpb.BytsPerSec;
    uint64_t total_bytes = (uint64_t)total_clusters * cluster_bytes;
    uint64_t free_bytes  = (uint64_t)free_clusters * cluster_bytes;
    uint64_t used_bytes  = (total_bytes >= free_bytes) ? total_bytes - free_bytes : 0;
    struct FAT32_Usage u;
    u.total_mb = (uint32_t)(total_bytes / (1024ULL * 1024ULL));
    u.used_mb  = (uint32_t)(used_bytes  / (1024ULL * 1024ULL));
    return u;
}

// -----------------------------------------------------------------------------
// Accessors
// -----------------------------------------------------------------------------
uint32_t fat32_get_current_dir(void) { return current_dir_cluster; }
void     fat32_set_current_dir(uint32_t cluster) { current_dir_cluster = cluster; }

size_t fat32_read(char* buf, size_t bufsize, size_t start_page, const char* path) {
    uint32_t cluster = bpb.RootClus;
    char segment[64];
    int seg_i = 0;
    int reading_file = 0;

    if (!path || !path[0]) 
        return 0;

    // Handle special base paths
    if (!strcmp(path, "/home") || !strcmp(path, "/"))
        cluster = bpb.RootClus;
    else if (str_starts_with(path, "/home/"))
        path += 6;
    else if (path[0] == '/')
        path++;

    // ---- Parse path segments ----
    for (int i = 0;; i++) {
        char c = path[i];
        if (c == '/' || c == '\0') {
            segment[seg_i] = 0;
            if (seg_i > 0) {
                // Handle '.' (current dir) and '..' (parent dir)
                if (!strcmp(segment, ".")) {
                    seg_i = 0;
                    if (c == '\0') break;
                    continue;
                } else if (!strcmp(segment, "..")) {
                    // TODO: handle parent dir tracking if implemented
                    seg_i = 0;
                    if (c == '\0') break;
                    continue;
                }

                uint8_t sb[512];
                int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);
                uint32_t found_cluster = 0;
                uint32_t scan = cluster;

                while (scan && !found_cluster) {
                    uint32_t lba = cluster_to_lba(scan);
                    for (int s = 0; s < bpb.SecPerClus && !found_cluster; s++) {
                        ata_read_sector(lba + s, sb);
                        struct FAT32_DirEntry* e = (struct FAT32_DirEntry*)sb;
                        char lfn_buf[256]; lfn_buf[0] = 0;

                        for (int k = 0; k < eps; k++) {
                            if (e[k].Name[0] == 0x00) break;
                            if (e[k].Name[0] == 0xE5) { lfn_buf[0] = 0; continue; }
                            if (e[k].Attr == 0x0F) {
                                const struct FAT32_LFN_Entry* lfn = (const struct FAT32_LFN_Entry*)&e[k];
                                lfn_prepend_piece(lfn_buf, lfn, sizeof(lfn_buf));
                                continue;
                            }

                            char shortnm[13]; make_short_name_lower(e[k].Name, shortnm);
                            const char* cand = choose_name(lfn_buf, shortnm);

                            if (!strcasecmp(cand, segment)) {
                                found_cluster = (e[k].FstClusHI << 16) | e[k].FstClusLO;
                                reading_file = !(e[k].Attr & 0x10);
                                break;
                            }
                            lfn_buf[0] = 0;
                        }
                    }
                    if (!found_cluster) scan = get_next_cluster(scan);
                }

                if (!found_cluster) return 0; // path not found
                cluster = found_cluster;
            }
            seg_i = 0;
            if (c == '\0') break;
        } else if (seg_i < 63) {
            segment[seg_i++] = c;
        }
    }

    // ---- Read directory or file ----
    uint8_t sector_buf[512];
    int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);
    size_t pos = 0, page = 0;

    if (!reading_file) {
        while (cluster != 0) {
            uint32_t lba = cluster_to_lba(cluster);
            for (int s = 0; s < bpb.SecPerClus; s++) {
                if (page++ < start_page) continue;
                ata_read_sector(lba + s, sector_buf);
                struct FAT32_DirEntry* e = (struct FAT32_DirEntry*)sector_buf;
                char lfn_buf[256]; lfn_buf[0] = 0;

                for (int i = 0; i < eps; i++) {
                    if (e[i].Name[0] == 0x00) { buf[pos] = 0; return pos; }
                    if (e[i].Name[0] == 0xE5) { lfn_buf[0] = 0; continue; }
                    if (e[i].Attr == 0x0F) {
                        const struct FAT32_LFN_Entry* lfn = (const struct FAT32_LFN_Entry*)&e[i];
                        lfn_prepend_piece(lfn_buf, lfn, sizeof(lfn_buf));
                        continue;
                    }

                    char shortnm[13]; make_short_name_lower(e[i].Name, shortnm);
                    const char* name = choose_name(lfn_buf, shortnm);

                    for (int j = 0; name[j]; j++) {
                        if (pos >= bufsize - 2) return 0; // too long
                        buf[pos++] = name[j];
                    }
                    buf[pos++] = '\n';
                    if (pos >= bufsize - 1) return 0;
                    lfn_buf[0] = 0;
                }
            }
            cluster = get_next_cluster(cluster);
        }
        buf[pos] = 0;
        return pos;
    } 
    else {
        uint32_t fclus = cluster;
        uint32_t bytes_read = 0;
        uint32_t file_size = 0;

        // find the size of this file
        uint8_t sb[512];
        uint32_t scan = current_dir_cluster;
        while (scan && !file_size) {
            uint32_t lba = cluster_to_lba(scan);
            for (int s = 0; s < bpb.SecPerClus && !file_size; s++) {
                ata_read_sector(lba + s, sb);
                struct FAT32_DirEntry *e = (struct FAT32_DirEntry *)sb;
                int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);
                for (int k = 0; k < eps; k++) {
                    if (e[k].Name[0] == 0x00) break;
                    if (e[k].Attr == 0x0F) continue;
                    uint32_t clus = (e[k].FstClusHI << 16) | e[k].FstClusLO;
                    if (clus == cluster) {
                        file_size = e[k].FileSize;
                        break;
                    }
                }
            }
            scan = get_next_cluster(scan);
        }

        // ---- pre-check buffer size ----
        // ensure we can hold the *entire* file (including null terminator)
        if (file_size + 1 > bufsize)
            return 0;  // not enough space; prevent partial read

        // ---- read entire file ----
        while (fclus && bytes_read < file_size) {
            uint32_t sec_lba = cluster_to_lba(fclus);
            for (int s = 0; s < bpb.SecPerClus && bytes_read < file_size; s++) {
                ata_read_sector(sec_lba + s, (uint8_t*)buf + bytes_read);
                bytes_read += 512;
                if (bytes_read >= file_size)
                    break;
            }
            fclus = get_next_cluster(fclus);
        }

        if (bytes_read < file_size)
            return 0; // file chain ended prematurely — incomplete read

        buf[file_size] = 0;
        return file_size;
    }

}

// -----------------------------------------------------------------------------
// Retrieve file size by path (returns 0 if not found or it's a directory)
// -----------------------------------------------------------------------------
size_t fat32_get_file_size(const char *path) {
    if (!path || !path[0])
        return 0;

    uint32_t cluster = bpb.RootClus;
    char segment[64];
    int seg_i = 0;
    int is_file = 0;
    uint32_t file_size = 0;

    // Normalize base path
    if (!strcmp(path, "/home") || !strcmp(path, "/"))
        cluster = bpb.RootClus;
    else if (str_starts_with(path, "/home/"))
        path += 6;
    else if (path[0] == '/')
        path++;

    // ---- Walk the path ----
    for (int i = 0;; i++) {
        char c = path[i];
        if (c == '/' || c == '\0') {
            segment[seg_i] = 0;
            if (seg_i > 0) {
                uint8_t sb[512];
                int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);
                uint32_t found_cluster = 0;
                uint32_t scan = cluster;

                while (scan && !found_cluster) {
                    uint32_t lba = cluster_to_lba(scan);
                    for (int s = 0; s < bpb.SecPerClus && !found_cluster; s++) {
                        ata_read_sector(lba + s, sb);
                        struct FAT32_DirEntry *e = (struct FAT32_DirEntry *)sb;
                        char lfn_buf[256]; lfn_buf[0] = 0;

                        for (int k = 0; k < eps; k++) {
                            if (e[k].Name[0] == 0x00) break;
                            if (e[k].Name[0] == 0xE5) { lfn_buf[0] = 0; continue; }
                            if (e[k].Attr == 0x0F) {
                                const struct FAT32_LFN_Entry *lfn = (const struct FAT32_LFN_Entry *)&e[k];
                                lfn_prepend_piece(lfn_buf, lfn, sizeof(lfn_buf));
                                continue;
                            }

                            char shortnm[13]; make_short_name_lower(e[k].Name, shortnm);
                            const char *cand = choose_name(lfn_buf, shortnm);

                            if (!strcasecmp(cand, segment)) {
                                found_cluster = (e[k].FstClusHI << 16) | e[k].FstClusLO;
                                is_file = !(e[k].Attr & 0x10);
                                if (is_file)
                                    file_size = e[k].FileSize;
                                break;
                            }
                            lfn_buf[0] = 0;
                        }
                    }
                    if (!found_cluster)
                        scan = get_next_cluster(scan);
                }

                if (!found_cluster)
                    return 0; // Not found

                cluster = found_cluster;
            }

            seg_i = 0;
            if (c == '\0') break;
        } else if (seg_i < 63) {
            segment[seg_i++] = c;
        }
    }

    return is_file ? file_size : 0;
}

static int fat32_alloc_handle(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!fat32_open_files[i].used) {
            fat32_open_files[i].used = 1;
            return i;
        }
    }
    return -1;
}

void fat32_close_file(int handle) {
    if (handle < 0 || handle >= MAX_OPEN_FILES) return;
    fat32_open_files[handle].used = 0;
}
int fat32_open_file(const char *path) {
    if (!path || !path[0]) return -1;

    uint32_t cluster = bpb.RootClus;
    char segment[256];
    int seg_i = 0;
    int is_file = 0;
    uint32_t file_size = 0;

    // Optional prefix stripping
    if (str_starts_with(path, "/home/")) path += 6;
    else if (path[0] == '/') path++;

    // Walk the path components
    for (int i = 0;; i++) {
        char c = path[i];
        if (c == '/' || c == '\0') {
            segment[seg_i] = 0;
            if (seg_i > 0) {
                uint32_t found_cluster = 0;
                int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);

                uint32_t dir_cluster = cluster;
                while (dir_cluster && !found_cluster) {
                    uint8_t sb[512];
                    uint32_t lba = cluster_to_lba(dir_cluster);

                    for (int s = 0; s < bpb.SecPerClus && !found_cluster; s++) {
                        ata_read_sector(lba + s, sb);
                        struct FAT32_DirEntry *e = (struct FAT32_DirEntry *)sb;
                        char lfn_buf[256]; 
                        lfn_buf[0] = 0;

                        for (int k = 0; k < eps; k++) {
                            if (e[k].Name[0] == 0x00) break;      // end of directory
                            if (e[k].Name[0] == 0xE5) {           // deleted entry
                                lfn_buf[0] = 0;
                                continue;
                            }
                            if (e[k].Attr == 0x0F) {
                                // Long file name entry
                                const struct FAT32_LFN_Entry *lfn = (const struct FAT32_LFN_Entry *)&e[k];
                                lfn_prepend_piece(lfn_buf, lfn, sizeof(lfn_buf));
                                continue;
                            }

                            // Standard 8.3 entry
                            char shortnm[13];
                            make_short_name_lower(e[k].Name, shortnm);
                            const char *cand = choose_name(lfn_buf, shortnm);

                            if (!strcasecmp(cand, segment)) {
                                found_cluster = ((uint32_t)e[k].FstClusHI << 16) | e[k].FstClusLO;
                                is_file = !(e[k].Attr & 0x10);
                                if (is_file)
                                    file_size = e[k].FileSize;
                                break;
                            }

                            lfn_buf[0] = 0; // reset after a valid entry
                        }
                    }

                    if (!found_cluster)
                        dir_cluster = get_next_cluster(dir_cluster);
                }

                if (!found_cluster)
                    return -1; // Not found
                cluster = found_cluster;
            }

            seg_i = 0;
            if (c == '\0') break;
        } else if (seg_i < (int)(sizeof(segment) - 1)) {
            segment[seg_i++] = c;
        }
    }

    if (!is_file) return -1;

    int handle = fat32_alloc_handle();
    if (handle < 0)
        return -2; // no available handles

    FAT32_FileHandle *f = &fat32_open_files[handle];
    memset(f, 0, sizeof(*f));
    f->used = 1;
    f->start_cluster = cluster;
    f->current_cluster = cluster;
    f->cluster_index = 0;
    f->file_size = file_size;
    f->cache_valid = 0;
    f->cached_cluster = 0;
    f->bytes_read = 0;

    return handle;
}

// --- Cluster-level read helper (safe and simple) ---
static void ata_read_cluster(uint32_t cluster, uint8_t *dst) {
    uint32_t lba = cluster_to_lba(cluster);
    for (int s = 0; s < bpb.SecPerClus; s++, dst += bpb.BytsPerSec)
        ata_read_sector(lba + s, dst);
}

// --- Correct + fast replacement for fat32_read_chunk() ---
size_t fat32_read_chunk(int handle, void *buf, size_t size, size_t position) {
    if (handle < 0 || handle >= MAX_OPEN_FILES || !buf || size == 0)
        return 0;

    FAT32_FileHandle *f = &fat32_open_files[handle];
    if (!f->used || f->file_size == 0 || position >= f->file_size)
        return 0;

    const size_t bytes_per_cluster = bpb.BytsPerSec * bpb.SecPerClus;
    size_t bytes_read_total = 0;
    uint8_t *out = (uint8_t *)buf;
    size_t pos = position;

    // Determine which cluster this position starts in
    uint32_t cluster_index = pos / bytes_per_cluster;
    size_t cluster_offset = pos % bytes_per_cluster;

    // Find the correct cluster — try to reuse last one
    uint32_t cluster = f->cached_cluster;
    uint32_t current_idx = f->cluster_index;

    if (!f->cache_valid || cluster == 0 || cluster_index < current_idx) {
        // Start from the beginning of the chain
        cluster = f->start_cluster;
        current_idx = 0;
    }

    // Advance to desired cluster
    while (current_idx < cluster_index && cluster) {
        cluster = get_next_cluster(cluster);
        current_idx++;
    }

    if (!cluster)
        return 0;

    // Read until requested size or EOF
    while (bytes_read_total < size && pos < f->file_size && cluster) {
        // Load current cluster if not cached
        if (!f->cache_valid || f->cached_cluster != cluster) {
            ata_read_cluster(cluster, f->cache_buf);
            f->cached_cluster = cluster;
            f->cache_valid = 1;
            f->cluster_index = current_idx;
        }

        // Compute available bytes from this cluster
        size_t available = bytes_per_cluster - cluster_offset;
        size_t remaining = size - bytes_read_total;
        if (available > remaining)
            available = remaining;
        size_t file_remaining = f->file_size - pos;
        if (available > file_remaining)
            available = file_remaining;

        // Copy from cache to user buffer
        memcpy(out + bytes_read_total, f->cache_buf + cluster_offset, available);
        bytes_read_total += available;
        pos += available;

        // Move to next cluster
        cluster_offset = 0;
        cluster = get_next_cluster(cluster);
        current_idx++;
    }

    f->bytes_read = pos;
    return bytes_read_total;
}
