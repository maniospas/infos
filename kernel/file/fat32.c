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
// Helpers (no libc)
// -----------------------------------------------------------------------------
static inline char to_lower(char c) { return (c >= 'A' && c <= 'Z') ? (c + 32) : c; }

static int str_length_bounded(const char *s, int maxn) {
    int n = 0; while (n < maxn && s[n]) n++; return n;
}
static int str_length(const char *s) { return str_length_bounded(s, 32767); }

static void str_copy(char *dst, const char *src, int maxlen) {
    int i = 0; while (src[i] && i < maxlen - 1) { dst[i] = src[i]; i++; } dst[i] = 0;
}
static void str_append_char(char *dst, int *len, char c, int maxlen) {
    if (*len < maxlen - 1) { dst[*len] = c; (*len)++; dst[*len] = 0; }
}
static int str_starts_with(const char *s, const char *prefix) {
    int i = 0; while (prefix[i]) { if (s[i] != prefix[i]) return 0; i++; } return 1;
}

// Case-insensitive string compare (ASCII only)
static int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2)
            return (unsigned char)c1 - (unsigned char)c2;
        s1++; s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

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

    fb_write(win, "\n");
    while (cluster != 0) {
        uint32_t lba = cluster_to_lba(cluster);
        for (int s = 0; s < bpb.SecPerClus; s++) {
            ata_read_sector(lba + s, sector_buf);
            struct FAT32_DirEntry *e = (struct FAT32_DirEntry *)sector_buf;

            char lfn_buf[256]; lfn_buf[0] = 0;

            for (int i = 0; i < eps; i++) {
                if (e[i].Name[0] == 0x00) { fb_write(win, "\n"); return; }
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

                fb_write(win, "  ");
                if (e[i].Attr & 0x10) fb_write_ansi(win, "\033[33m");
                fb_write(win, disp);
                if (e[i].Attr & 0x10) fb_write_ansi(win, "\033[0m");
                else {
                    fb_write(win, " ");
                    fb_write_ansi(win, "\033[36m");
                    fb_write_dec(win, e[i].FileSize);
                    fb_write_ansi(win, " bytes\033[0m");
                }
                fb_put_char(win, '\n');

                lfn_buf[0] = 0; // reset for next
            }
        }
        cluster = get_next_cluster(cluster);
    }
    fb_write(win, "\n");
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
                        fb_write(win, "\n\n");
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
// cat with LFN support
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// cat with LFN support + highlighting for extensionless files
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// cat with LFN support + highlighting for extensionless files (no stdlib)
// -----------------------------------------------------------------------------

// --- Minimal helper functions ---
static int str_len(const char *s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static const char* str_find_last_dot(const char *s) {
    const char *last = 0;
    while (s && *s) {
        if (*s == '.') last = s;
        s++;
    }
    return last;
}

static int str_casecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}

static int str_ncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i] || !b[i])
            return a[i] - b[i];
    }
    return 0;
}

static int is_alnum(char c) {
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9'));
}

// -----------------------------------------------------------------------------
void fat32_cat(Window* win, const char *filename) {
    uint32_t cluster = current_dir_cluster;
    uint8_t sector_buf[512];
    int eps = bpb.BytsPerSec / sizeof(struct FAT32_DirEntry);

    // Detect if the file has an extension
    const char *dot = str_find_last_dot(filename);
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
                    fb_write(win, "\n\n");
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

                if (!str_casecmp(cand, filename) && !(e[i].Attr & 0x10)) {
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
                        fb_write(win, "\n  ");
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
                                        fb_write(win, "\n  ");
                                    else
                                        fb_put_char(win, '\n');
                                }
                                continue;
                            }

                            // Green for keywords and braces
                            if (!str_ncmp(&ptr[i], "let", 3) && !is_alnum(ptr[i+3])) {
                                fb_write_ansi(win, "\x1b[32mlet\x1b[0m");
                                i += 3;
                                continue;
                            }
                            if (!str_ncmp(&ptr[i], "loop", 4) && !is_alnum(ptr[i+4])) {
                                fb_write_ansi(win, "\x1b[32mloop\x1b[0m");
                                i += 4;
                                continue;
                            }
                            if (!str_ncmp(&ptr[i], "if", 2) && !is_alnum(ptr[i+2])) {
                                fb_write_ansi(win, "\x1b[32mloop\x1b[0m");
                                i += 2;
                                continue;
                            }
                            if (!str_ncmp(&ptr[i], "return", 6) && !is_alnum(ptr[i+6])) {
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

                    fb_write(win, "\n\n");
                    return;
                }

                lfn_buf[0] = 0;
            }
        }
        cluster = get_next_cluster(cluster);
    }

    fb_write_ansi(win, "  \x1b[32mERROR\x1b[0m File not found: ");
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
    uint64_t used_bytes  = (total_bytes > free_bytes) ? total_bytes - free_bytes : 0;

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
