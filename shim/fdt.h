#pragma once
#include <stdint.h>

/* Read a u32/u64 (cell-size agnostic) property of /chosen. Returns 0 if absent. */
int fdt_chosen_u64(uintptr_t fdt, const char *prop, uint64_t *out);

/*
 * Call fn for every <base, size> in "reg" of the root's memory nodes and of
 * /reserved-memory's children. Assumes #address-cells = #size-cells = 2 (the
 * Qualcomm root and reserved-memory). Returns 0 if the FDT is unreadable.
 */
#define FDT_RANGE_MEMORY	0
#define FDT_RANGE_RESERVED	1
typedef void (*fdt_range_fn)(void *ctx, int kind, uint64_t base, uint64_t size);
int fdt_ranges(uintptr_t fdt, fdt_range_fn fn, void *ctx);
