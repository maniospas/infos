#pragma once
#include <stdint.h>
#include <stddef.h>

void fb_init(void* mb_info_addr);
void fb_clear(void);
void fb_write(const char *s);
void fb_write_ansi(const char *s);
void fb_put_char(char c);
void fb_removechar();
void fb_set_scale(uint32_t nom, uint32_t denom);
void fb_write_dec(uint64_t num);
void fb_write_hex(uint64_t num);
extern uint32_t margin;