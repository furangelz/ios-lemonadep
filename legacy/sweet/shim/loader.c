/*
 * Milestone 4: act as iBoot for xnu.
 *
 * mkpayload.py already resolved the kernel's chained fixups and built the
 * ADT, so this only has to: find the payload in the ABL ramdisk, copy the
 * segments to phys_base + (vmaddr - virt_base), place the ADT and
 * boot_args behind the kernel, and jump to the entry point with the MMU
 * and caches off, x0 = boot_args (physical).
 */
#include <stdint.h>
#include "console.h"
#include "fdt.h"
#include "loader.h"

#define BOOT_LINE_LENGTH	608
#define PAGE_16K		0x4000UL

struct payload_seg {
	uint64_t vmaddr, vmsize, file_off, file_size;
};

struct payload {
	char magic[8];
	uint32_t version, nsegs;
	uint64_t virt_base, entry_va, adt_off, adt_size, kernel_end;
	uint64_t phys_base, mem_size;
	char cmdline[BOOT_LINE_LENGTH];
	struct payload_seg segs[];
};

/* pexpert/pexpert/arm64/boot.h, Revision/Version 2 */
struct boot_video {
	uint64_t v_baseAddr, v_display, v_rowBytes, v_width, v_height, v_depth;
};

struct boot_args {
	uint16_t Revision, Version;
	uint64_t virtBase, physBase, memSize, topOfKernelData;
	struct boot_video Video;
	uint32_t machineType;
	uint64_t deviceTreeP;
	uint32_t deviceTreeLength;
	char CommandLine[BOOT_LINE_LENGTH];
	uint64_t bootFlags, memSizeActual;
};

_Static_assert(__builtin_offsetof(struct payload, segs) == 0x2a8, "payload header layout");
_Static_assert(__builtin_offsetof(struct boot_args, Video) == 40, "boot_args layout");
_Static_assert(__builtin_offsetof(struct boot_args, CommandLine) == 108, "boot_args layout");
_Static_assert(sizeof(struct boot_args) == 736, "boot_args layout");

/* MMU off: everything is Device memory, so copy in aligned 8-byte words. */
static void copy64(uintptr_t dst, uintptr_t src, uint64_t n)
{
	volatile uint64_t *d = (volatile uint64_t *)dst;
	volatile const uint64_t *s = (volatile const uint64_t *)src;
	for (uint64_t i = 0; i < (n + 7) / 8; i++)
		d[i] = s[i];
}

static void zero64(uintptr_t dst, uint64_t n)
{
	volatile uint64_t *d = (volatile uint64_t *)dst;
	for (uint64_t i = 0; i < n / 8; i++)
		d[i] = 0;
}

static int magic_eq(const char *m, const char *want)
{
	for (int i = 0; i < 8; i++)
		if (m[i] != want[i])
			return 0;
	return 1;
}

static int magic_ok(const char *m)
{
	return magic_eq(m, "SWEETXNU");
}

/* Byte loads: the trailer can be at any alignment. */
static uint64_t rd_u64(uintptr_t a)
{
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--)
		v = v << 8 | *(volatile uint8_t *)(a + i);
	return v;
}

/* mkpayload.py's trailer, the last 32 bytes of the payload. */
struct trailer {
	uintptr_t start;		/* payload start (may be misaligned) */
	uint64_t len;			/* payload size, trailer included */
	uint64_t ram_base, ram_size;	/* all RAM the kernel touches: dram + /pram */
};

/*
 * Boot header v3 (lemonadep): ABL loads the vendor_boot ramdisk at
 * initrd-start and ours right behind it; with v2 (sweet) ours is the whole
 * initrd. Either way the payload ends in a "SWEETEND" trailer, searched
 * backwards in case ABL padded the end.
 */
static int find_payload(uintptr_t rd_start, uintptr_t rd_end, struct trailer *tr)
{
	for (uintptr_t t = rd_end - 16; t >= rd_start + 16 && t + 0x10000 >= rd_end; t--) {
		if (!magic_eq((const char *)t, "SWEETEND"))
			continue;
		tr->len = rd_u64(t + 8);
		if (tr->len < 32 || tr->len > t + 16 - rd_start)
			return 0;
		tr->start = t + 16 - tr->len;
		tr->ram_base = rd_u64(t - 16);
		tr->ram_size = rd_u64(t - 8);
		return magic_ok((const char *)tr->start);
	}
	return 0;
}

/*
 * Shift a misaligned payload down to 8-byte alignment (over the tail of the
 * vendor ramdisk), so everything after this can use aligned word copies.
 * Forward order is safe: word i is written after words i and i+1 were read.
 */
static uintptr_t realign(uintptr_t src, uint64_t len)
{
	uintptr_t dst = src & ~7UL;
	unsigned int sh = (src & 7) * 8;
	volatile uint64_t *w = (volatile uint64_t *)dst;
	uint64_t cur = w[0];

	if (!sh)
		return src;
	for (uint64_t i = 0; i < (len + 7) / 8; i++) {
		uint64_t next = w[i + 1];
		w[i] = cur >> sh | next << (64 - sh);
		cur = next;
	}
	return dst;
}

typedef void (*xnu_entry_t)(uint64_t boot_args_pa);

/* A payload is present but unsafe to boot: keep the reason on screen. */
static void __attribute__((noreturn)) refuse(const char *why)
{
	con_str("LOADER", why);
	con_str("HALT", "hold Power+VolDown to restart");
	for (;;)
		__asm__ volatile("wfe");
}

#define MAX_DDR		32

struct ram_check {
	uint64_t lo, hi;	/* RAM the kernel touches [lo, hi) */
	int n_ddr, clashes;
	uint64_t ddr[MAX_DDR][2];	/* /memory ranges [base, end) */
};

static void ram_range(void *ctx, int kind, uint64_t base, uint64_t size)
{
	struct ram_check *rc = ctx;

	if (kind == FDT_RANGE_MEMORY) {
		con_range("DDR", base, size);
		if (rc->n_ddr < MAX_DDR) {
			rc->ddr[rc->n_ddr][0] = base;
			rc->ddr[rc->n_ddr][1] = base + size;
			rc->n_ddr++;
		}
	} else if (base < rc->hi && rc->lo < base + size) {
		con_range("CLASH", base, size);
		rc->clashes++;
	}
}

/* ABL may list DDR as several adjacent ranges: is [lo, hi) inside their union? */
static int ram_in_ddr(const struct ram_check *rc)
{
	uint64_t at = rc->lo;

	for (int progress = 1; progress && at < rc->hi;) {
		progress = 0;
		for (int i = 0; i < rc->n_ddr; i++)
			if (rc->ddr[i][0] <= at && at < rc->ddr[i][1]) {
				at = rc->ddr[i][1];
				progress = 1;
			}
	}
	return at >= rc->hi;
}

int loader_ram_ok(uintptr_t dtb, uint64_t base, uint64_t size)
{
	struct ram_check rc;	/* no initializer: zeroing ddr[] could become a memset call */
	rc.lo = base;
	rc.hi = base + size;
	rc.n_ddr = rc.clashes = 0;
	return fdt_ranges(dtb, ram_range, &rc) && ram_in_ddr(&rc) && !rc.clashes;
}

const char *loader_boot(uintptr_t dtb)
{
	uint64_t rd_start, rd_end;

	if (!fdt_chosen_u64(dtb, "linux,initrd-start", &rd_start) ||
	    !fdt_chosen_u64(dtb, "linux,initrd-end", &rd_end))
		return "no initrd in /chosen";
	con_hex2("INITRD", "lo=", rd_start, "hi=", rd_end);

	struct trailer tr;
	if (!find_payload(rd_start, rd_end, &tr))
		return "no SWEETXNU payload";
	if (tr.start != rd_start)
		con_hex2("PAYLOAD", "at=", tr.start, "len=", tr.len);
	uintptr_t start = realign(tr.start, tr.len);
	const struct payload *p = (const struct payload *)start;
	if (p->version != 1 || (p->adt_off & 7))
		return "bad payload header";

	uint64_t pa_of_va_base = p->phys_base - p->virt_base;	/* pa = va + this */
	con_range("PHYS", p->phys_base, p->mem_size);
	con_range("RAM", tr.ram_base, tr.ram_size);
	con_hex("VIRT", p->virt_base);
	con_hex("ENTRY", p->entry_va);
	if (p->phys_base < tr.ram_base || p->phys_base + p->mem_size > tr.ram_base + tr.ram_size)
		return "bad payload trailer";

	/* The payload must not overlap where the kernel goes. */
	struct ram_check rc;	/* no initializer: zeroing ddr[] could become a memset call */
	rc.lo = tr.ram_base;
	rc.hi = tr.ram_base + tr.ram_size;
	rc.n_ddr = rc.clashes = 0;
	if (rd_end > rc.lo && rd_start < rc.hi)
		return "ramdisk overlaps kernel memory";

	/* ABL's live FDT: the kernel's RAM must be DDR and free of fixed carve-outs. */
	if (!fdt_ranges(dtb, ram_range, &rc))
		refuse("FDT unreadable, no RAM check");
	if (!ram_in_ddr(&rc))
		refuse("kernel RAM is not inside /memory");
	if (rc.clashes)
		refuse("kernel RAM overlaps reserved-memory");

	for (uint32_t i = 0; i < p->nsegs; i++) {
		const struct payload_seg *s = &p->segs[i];
		uintptr_t dst = s->vmaddr + pa_of_va_base;
		copy64(dst, start + s->file_off, s->file_size);
		if (s->vmsize > s->file_size)
			zero64(dst + ((s->file_size + 7) & ~7UL), s->vmsize - ((s->file_size + 7) & ~7UL));
	}
	con_hex2("SEGS", "n=", p->nsegs, "end=", p->kernel_end + pa_of_va_base);

	/* ADT, then boot_args, behind the kernel; everything below topOfKernelData is ours. */
	uintptr_t adt_pa = p->kernel_end + pa_of_va_base;
	copy64(adt_pa, start + p->adt_off, p->adt_size);
	uintptr_t ba_pa = (adt_pa + p->adt_size + PAGE_16K - 1) & ~(PAGE_16K - 1);
	uintptr_t top = ba_pa + PAGE_16K;

	zero64(ba_pa, PAGE_16K);
	volatile struct boot_args *ba = (volatile struct boot_args *)ba_pa;
	ba->Revision = 2;
	ba->Version = 2;
	ba->virtBase = p->virt_base;
	ba->physBase = p->phys_base;
	ba->memSize = p->mem_size;
	ba->topOfKernelData = top;
	ba->Video.v_baseAddr = FB_BASE;
	ba->Video.v_display = 1;
	ba->Video.v_rowBytes = FB_STRIDE;
	ba->Video.v_width = FB_W;
	ba->Video.v_height = FB_H;
	ba->Video.v_depth = 32;
	ba->deviceTreeP = adt_pa - pa_of_va_base;	/* xnu reads it with the MMU on: a VA */
	ba->deviceTreeLength = (uint32_t)p->adt_size;
	for (int i = 0; i < BOOT_LINE_LENGTH; i++)
		ba->CommandLine[i] = p->cmdline[i];
	ba->memSizeActual = p->mem_size;
	con_hex2("BOOTARG", "pa=", ba_pa, "top=", top);

	uintptr_t entry_pa = p->entry_va + pa_of_va_base;
	con_hex("JUMP", entry_pa);
	con_str("XNU", "jumping (fbcon clears screen)");

	/* Everything was written with the MMU and caches off; drop any stale I-cache lines too. */
	__asm__ volatile("dsb sy; ic iallu; dsb sy; isb" ::: "memory");
	((xnu_entry_t)entry_pa)(ba_pa);
	return "xnu returned";
}
