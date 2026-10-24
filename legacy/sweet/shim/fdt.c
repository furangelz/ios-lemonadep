/*
 * Just enough flattened-device-tree parsing to find what ABL put in
 * /chosen (initrd location) and the RAM layout. Big-endian, 4-byte aligned
 * tokens.
 */
#include "fdt.h"

#define FDT_MAGIC	0xd00dfeed
#define FDT_BEGIN_NODE	1
#define FDT_END_NODE	2
#define FDT_PROP	3
#define FDT_NOP		4
#define FDT_END		9

static uint32_t be32(uintptr_t a)
{
	return __builtin_bswap32(*(volatile uint32_t *)a);
}

static int streq(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *a == *b;
}

static uintptr_t align4(uintptr_t a)
{
	return (a + 3) & ~3UL;
}

static uintptr_t skip_str(uintptr_t p)
{
	/* byte loads are fine on Device memory as long as they are aligned to 1 */
	while (*(volatile char *)p)
		p++;
	return p + 1;
}

int fdt_chosen_u64(uintptr_t fdt, const char *prop, uint64_t *out)
{
	if (!fdt || be32(fdt) != FDT_MAGIC)
		return 0;
	uintptr_t p = fdt + be32(fdt + 8);		/* off_dt_struct */
	uintptr_t strings = fdt + be32(fdt + 12);	/* off_dt_strings */
	int depth = 0, in_chosen = 0;

	for (;;) {
		uint32_t tok = be32(p);
		p += 4;
		switch (tok) {
		case FDT_BEGIN_NODE: {
			const char *name = (const char *)p;
			depth++;
			in_chosen = depth == 2 && streq(name, "chosen");
			p = align4(skip_str(p));
			break;
		}
		case FDT_END_NODE:
			if (in_chosen)
				return 0;
			depth--;
			break;
		case FDT_PROP: {
			uint32_t len = be32(p), nameoff = be32(p + 4);
			uintptr_t val = p + 8;
			if (in_chosen && streq((const char *)(strings + nameoff), prop)) {
				if (len == 4)
					*out = be32(val);
				else if (len == 8)
					*out = (uint64_t)be32(val) << 32 | be32(val + 4);
				else
					return 0;
				return 1;
			}
			p = align4(val + len);
			break;
		}
		case FDT_NOP:
			break;
		default:	/* FDT_END or garbage */
			return 0;
		}
	}
}

static int is_memory_node(const char *n)
{
	static const char m[] = "memory";
	for (int i = 0; i < 6; i++)
		if (n[i] != m[i])
			return 0;
	return n[6] == 0 || n[6] == '@';
}

int fdt_ranges(uintptr_t fdt, fdt_range_fn fn, void *ctx)
{
	if (!fdt || be32(fdt) != FDT_MAGIC)
		return 0;
	uintptr_t p = fdt + be32(fdt + 8);
	uintptr_t strings = fdt + be32(fdt + 12);
	int depth = 0, in_mem = 0, in_resv = 0;

	for (;;) {
		uint32_t tok = be32(p);
		p += 4;
		switch (tok) {
		case FDT_BEGIN_NODE: {
			const char *name = (const char *)p;
			depth++;
			if (depth == 2) {
				in_mem = is_memory_node(name);
				in_resv = streq(name, "reserved-memory");
			}
			p = align4(skip_str(p));
			break;
		}
		case FDT_END_NODE:
			if (depth == 2)
				in_mem = in_resv = 0;
			depth--;
			break;
		case FDT_PROP: {
			uint32_t len = be32(p), nameoff = be32(p + 4);
			uintptr_t val = p + 8;
			int kind = in_mem && depth == 2 ? FDT_RANGE_MEMORY :
				   in_resv && depth == 3 ? FDT_RANGE_RESERVED : -1;
			if (kind >= 0 && streq((const char *)(strings + nameoff), "reg"))
				for (uint32_t o = 0; o + 16 <= len; o += 16)
					fn(ctx, kind,
					   (uint64_t)be32(val + o) << 32 | be32(val + o + 4),
					   (uint64_t)be32(val + o + 8) << 32 | be32(val + o + 12));
			p = align4(val + len);
			break;
		}
		case FDT_NOP:
			break;
		case FDT_END:
			return 1;
		default:
			return 0;
		}
	}
}
