#include "string.h"

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || !s1[i] || !s2[i])
            return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
    return 0;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; i++)
        p[i] = (unsigned char)c;
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || n == 0)
        return dest;

    if (d < s) {
        // Copy forward
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        // Copy backward to handle overlap
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dest;
}


size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}

// static int str_len(const char *s) {
//     int n = 0;
//     while (s && s[n]) n++;
//     return n;
// }

const char* strfindlastdot(const char *s) {
    const char *last = 0;
    while (s && *s) {
        if (*s == '.') last = s;
        s++;
    }
    return last;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}


// static int str_ncmp(const char *a, const char *b, int n) {
//     for (int i = 0; i < n; i++) {
//         if (a[i] != b[i] || !a[i] || !b[i])
//             return a[i] - b[i];
//     }
//     return 0;
// }

int is_alnum(char c) {
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9'));
}


char to_lower(char c) { return (c >= 'A' && c <= 'Z') ? (c + 32) : c; }
int str_length_bounded(const char *s, int maxn) {
    int n = 0; while (n < maxn && s[n]) n++; return n;
}
int str_length(const char *s) { return str_length_bounded(s, 32767); }
void str_copy(char *dst, const char *src, int maxlen) {
    int i = 0; while (src[i] && i < maxlen - 1) { dst[i] = src[i]; i++; } dst[i] = 0;
}
void str_append_char(char *dst, int *len, char c, int maxlen) {
    if (*len < maxlen - 1) { dst[*len] = c; (*len)++; dst[*len] = 0; }
}
int str_starts_with(const char *s, const char *prefix) {
    int i = 0; while (prefix[i]) { if (s[i] != prefix[i]) return 0; i++; } return 1;
}