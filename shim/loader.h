#pragma once
#include <stdint.h>

/* Boot the SWEETXNU payload found in the ABL ramdisk; returns only on error. */
const char *loader_boot(uintptr_t dtb);

/*
 * 1 if [base, base + size) is DDR per the live FDT's /memory and overlaps no fixed
 * /reserved-memory region; prints the DDR ranges and every CLASH on the way.
 */
int loader_ram_ok(uintptr_t dtb, uint64_t base, uint64_t size);
