#pragma once
#include <stdint.h>

#if defined(TARGET_LEMONADEP)
#define FB_BASE		0xe4d00000UL	/* OnePlus 9 Pro cont_splash_region (lemonade_common.dtsi) */
#define FB_W		1080
#define FB_H		2412
#define FB_STRIDE	(FB_W * 4)	/* a8r8g8b8, 4320 bytes: ABL scans out the FHD+ mode, not 1440x3216 */
#else
#define FB_BASE		0x9c000000UL	/* reserved-memory cont_splash_region */
#define FB_W		1080
#define FB_H		2400
#define FB_STRIDE	(FB_W * 4)	/* a8r8g8b8, matches mainline simplefb */
#endif

#define RED	0xffff0000U
#define BLUE	0xff0000a0U
#define WHITE	0xffffffffU
#define YELLOW	0xffffff00U
#define GREEN	0xff00ff00U

extern volatile uint32_t *con_fb;

void con_clear(uint32_t bg);
void con_skip(void);
void con_str(const char *label, const char *s);
void con_hex(const char *label, uint64_t v);
/* label, then two 64-bit values (e.g. base and size) */
void con_range(const char *label, uint64_t a, uint64_t b);
/* label, then "a=xxxxxxxx b=xxxxxxxx" style pairs of 32-bit values */
void con_hex2(const char *label, const char *a, uint32_t va, const char *b, uint32_t vb);
