// smp.h
#pragma once
#include <stdint.h>

void smp_init(void);
void ap_main(void);
uint32_t get_cpu_id(void);
