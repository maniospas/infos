#pragma once
#include <stddef.h>
#include <stdint.h>

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
size_t strlen(const char *s);

const char *strfindlastdot(const char *s);
int strcasecmp(const char *a, const char *b);
int is_alnum(char c);
char to_lower(char c);

int  str_length_bounded(const char *s, int maxn);
int  str_length(const char *s);
void str_copy(char *dst, const char *src, int maxlen);
void str_append_char(char *dst, int *len, char c, int maxlen);
int  str_starts_with(const char *s, const char *prefix);
