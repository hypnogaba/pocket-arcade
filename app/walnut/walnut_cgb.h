/*
Walnut-CGB additions
Copyright (c) 2025-2026 Mr. Paul (https://github.com/Mr-PauI)

Licensed under the MIT License.

--------------------------------------------------------------------------

Peanut-GB
Copyright (c) 2018-2023 Mahyar Koshkouei

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Please note that at least two parts of source code within this project were
taken from the SameBoy project at https://github.com/LIJI32/SameBoy/
which, at the time of this writing, is released under the MIT License.
Occurrences of this code are marked as being taken from SameBoy with a comment.

SameBoy
Copyright (c) 2015-2019 Lior Halphon


--- walnut/license.txt ---
MIT License

Copyright (c) 2025 Mr-PauI

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Comments and indentation removed by Pocket Arcade (tools/strip_c.py)
to fit the App Store's size limit; the original is in vendor/.
*/
#ifndef WALNUT_GB_H
#define WALNUT_GB_H
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#define WALNUT_GB_SAFE_DUALFETCH_OPCODES 0
#define WALNUT_GB_SAFE_DUALFETCH_DMA  0
#define WALNUT_GB_SAFE_DUALFETCH_MBC  0
#define WALNUT_GB_16_BIT_OPS_DUALFETCH 0
#define WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED2 0
#define WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED 0
#define WALNUT_GB_16_BIT_OPS 0
#define WALNUT_GB_16BIT_DMA 0
#define WALNUT_GB_32BIT_DMA 1
#define WALNUT_GB_16BIT_ALIGNED 1
#define WALNUT_GB_32BIT_ALIGNED 1
#define WALNUT_GB_RGB565_BIGENDIAN 0
uint8_t __gb_read(struct gb_s *gb, uint16_t addr);
void __gb_write(struct gb_s *gb, uint_fast16_t addr, uint8_t val);
void __gb_write16(struct gb_s *gb, uint_fast16_t addr, uint16_t val);
void __gb_write32(struct gb_s *gb, uint16_t addr, uint32_t val);
void __gb_step_cpu_x(struct gb_s *gb);
#if WALNUT_GB_32BIT_DMA && WALNUT_GB_16BIT_DMA
#error "Only one DMA mode can be enabled at a time!"
#endif
#define WALNUT_GB
#define WALNUT_GB_IS_LITTLE_ENDIAN 1
#if !defined(WALNUT_GB_IS_LITTLE_ENDIAN)
# if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#   define WALNUT_GB_IS_LITTLE_ENDIAN 0
#  else
#   define WALNUT_GB_IS_LITTLE_ENDIAN 1
#  endif
# elif defined(_WIN32)
#  define WALNUT_GB_IS_LITTLE_ENDIAN 1
# elif !defined(WALNUT_GB_IS_LITTLE_ENDIAN)
#  error "Could not detect target platform endian. Please define WALNUT_GB_IS_LITTLE_ENDIAN"
# endif
#endif
#if WALNUT_GB_IS_LITTLE_ENDIAN == 0
# error "Walnut-GB only supports little endian targets"
#endif
#ifndef ENABLE_SOUND
# define ENABLE_SOUND 1
#endif
#ifndef ENABLE_LCD
# define ENABLE_LCD 1
#endif
#ifndef WALNUT_GB_12_COLOUR
# define WALNUT_GB_12_COLOUR 1
#endif
#ifndef WALNUT_GB_HIGH_LCD_ACCURACY
# define WALNUT_GB_HIGH_LCD_ACCURACY 1
#endif
#ifndef WALNUT_GB_USE_INTRINSICS
# define WALNUT_GB_USE_INTRINSICS 1
#endif
#ifndef WALNUT_FULL_GBC_SUPPORT
# define WALNUT_FULL_GBC_SUPPORT 1
#endif
#if (WALNUT_GB_12_COLOUR != WALNUT_FULL_GBC_SUPPORT)
#error "WALNUT_GB_12_COLOUR and WALNUT_FULL_GBC_SUPPORT must both be enabled or both be disabled"
#endif
#define VBLANK_INTR	0x01
#define LCDC_INTR	0x02
#define TIMER_INTR	0x04
#define SERIAL_INTR	0x08
#define CONTROL_INTR	0x10
#define ANY_INTR	0x1F
#if WALNUT_FULL_GBC_SUPPORT
#define WRAM_SIZE	0x8000
#define VRAM_SIZE	0x4000
#else
#define WRAM_SIZE	0x2000
#define VRAM_SIZE	0x2000
#endif
#define HRAM_IO_SIZE	0x0100
#define OAM_SIZE	0x00A0
#define ROM_0_ADDR      0x0000
#define ROM_N_ADDR      0x4000
#define VRAM_ADDR       0x8000
#define CART_RAM_ADDR   0xA000
#define WRAM_0_ADDR     0xC000
#define WRAM_1_ADDR     0xD000
#define ECHO_ADDR       0xE000
#define OAM_ADDR        0xFE00
#define UNUSED_ADDR     0xFEA0
#define IO_ADDR         0xFF00
#define HRAM_ADDR       0xFF80
#define INTR_EN_ADDR    0xFFFF
#define ROM_BANK_SIZE   0x4000
#define WRAM_BANK_SIZE  0x1000
#define CRAM_BANK_SIZE  0x2000
#define VRAM_BANK_SIZE  0x2000
#define DIV_CYCLES          256
#define SERIAL_CYCLES       4096
#define SERIAL_CYCLES_1KB   (SERIAL_CYCLES/1ul)
#define SERIAL_CYCLES_2KB   (SERIAL_CYCLES/2ul)
#define SERIAL_CYCLES_32KB  (SERIAL_CYCLES/32ul)
#define SERIAL_CYCLES_64KB  (SERIAL_CYCLES/64ul)
#define DMG_CLOCK_FREQ      4194304.0
#define SCREEN_REFRESH_CYCLES 70224.0
#define VERTICAL_SYNC       (DMG_CLOCK_FREQ/SCREEN_REFRESH_CYCLES)
#define RTC_CYCLES	((uint_fast32_t)DMG_CLOCK_FREQ)
#define SERIAL_SC_TX_START  0x80
#define SERIAL_SC_CLOCK_SRC 0x01
#define STAT_LYC_INTR       0x40
#define STAT_MODE_2_INTR    0x20
#define STAT_MODE_1_INTR    0x10
#define STAT_MODE_0_INTR    0x08
#define STAT_LYC_COINC      0x04
#define STAT_MODE           0x03
#define STAT_USER_BITS      0xF8
#define LCDC_ENABLE         0x80
#define LCDC_WINDOW_MAP     0x40
#define LCDC_WINDOW_ENABLE  0x20
#define LCDC_TILE_SELECT    0x10
#define LCDC_BG_MAP         0x08
#define LCDC_OBJ_SIZE       0x04
#define LCDC_OBJ_ENABLE     0x02
#define LCDC_BG_ENABLE      0x01
#define LCD_VERT_LINES      154
#define LCD_WIDTH           160
#define LCD_HEIGHT          144
#define LCD_LINE_CYCLES     456
#define LCD_MODE0_HBLANK_MAX_DRUATION	204
#define LCD_MODE0_HBLANK_MIN_DRUATION	87
#define LCD_MODE2_OAM_SCAN_DURATION	80
#define LCD_MODE3_LCD_DRAW_MIN_DURATION	172
#define LCD_MODE3_LCD_DRAW_MAX_DURATION	289
#define LCD_MODE1_VBLANK_DURATION	(LCD_LINE_CYCLES * (LCD_VERT_LINES - LCD_HEIGHT))
#define LCD_FRAME_CYCLES		(LCD_LINE_CYCLES * LCD_VERT_LINES)
#define LCD_MODE_2_CYCLES   LCD_MODE0_HBLANK_MAX_DRUATION
#define LCD_MODE_3_CYCLES   (LCD_MODE_2_CYCLES + LCD_MODE2_OAM_SCAN_DURATION)
#define LCD_MODE_0_CYCLES   (LCD_MODE_3_CYCLES + LCD_MODE3_LCD_DRAW_MIN_DURATION)
#define LCD_MODE2_OAM_SCAN_START	0
#define LCD_MODE2_OAM_SCAN_END		(LCD_MODE2_OAM_SCAN_DURATION)
#define LCD_MODE3_LCD_DRAW_END		(LCD_MODE2_OAM_SCAN_END + LCD_MODE3_LCD_DRAW_MIN_DURATION)
#define LCD_MODE0_HBLANK_END		(LCD_MODE3_LCD_DRAW_END + LCD_MODE0_HBLANK_MAX_DRUATION)
#if LCD_MODE0_HBLANK_END != LCD_LINE_CYCLES
#error "LCD length not equal"
#endif
#define VRAM_TILES_1        (0x8000 - VRAM_ADDR)
#define VRAM_TILES_2        (0x8800 - VRAM_ADDR)
#define VRAM_BMAP_1         (0x9800 - VRAM_ADDR)
#define VRAM_BMAP_2         (0x9C00 - VRAM_ADDR)
#define VRAM_TILES_3        (0x8000 - VRAM_ADDR + VRAM_BANK_SIZE)
#define VRAM_TILES_4        (0x8800 - VRAM_ADDR + VRAM_BANK_SIZE)
#define VBLANK_INTR_ADDR    0x0040
#define LCDC_INTR_ADDR      0x0048
#define TIMER_INTR_ADDR     0x0050
#define SERIAL_INTR_ADDR    0x0058
#define CONTROL_INTR_ADDR   0x0060
#define NUM_SPRITES         0x28
#define MAX_SPRITES_LINE    0x0A
#define OBJ_PRIORITY        0x80
#define OBJ_FLIP_Y          0x40
#define OBJ_FLIP_X          0x20
#define OBJ_PALETTE         0x10
#if WALNUT_FULL_GBC_SUPPORT
#define OBJ_BANK            0x08
#define OBJ_CGB_PALETTE     0x07
#endif
#define JOYPAD_A            0x01
#define JOYPAD_B            0x02
#define JOYPAD_SELECT       0x04
#define JOYPAD_START        0x08
#define JOYPAD_RIGHT        0x10
#define JOYPAD_LEFT         0x20
#define JOYPAD_UP           0x40
#define JOYPAD_DOWN         0x80
#define ROM_HEADER_CHECKSUM_LOC	0x014D
#ifndef MIN
# define MIN(a, b)          ((a) < (b) ? (a) : (b))
#endif
#define WALNUT_GB_ARRAYSIZE(array)    (sizeof(array)/sizeof(array[0]))
#if (defined(__GNUC__) && __GNUC__ >= 6) || (defined(__clang__) && __clang_major__ >= 4)
# define WGB_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
# define WGB_DEPRECATED(msg)
#endif
#if !defined(__has_builtin)
# define __has_builtin(x) 0
#endif
#if !defined(WGB_UNREACHABLE)
# if __has_builtin(__builtin_unreachable)
#  define WGB_UNREACHABLE() __builtin_unreachable()
# elif defined(_MSC_VER) && _MSC_VER >= 1200
#
#  define WGB_UNREACHABLE() __assume(0)
# else
#  define WGB_UNREACHABLE() abort()
# endif
#endif
#if !defined(WGB_UNLIKELY)
# if __has_builtin(__builtin_expect)
#  define WGB_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
# else
#  define WGB_UNLIKELY(expr) (expr)
# endif
#endif
#if !defined(WGB_LIKELY)
# if __has_builtin(__builtin_expect)
#  define WGB_LIKELY(expr) __builtin_expect(!!(expr), 1)
# else
#  define WGB_LIKELY(expr) (expr)
# endif
#endif
#if WALNUT_GB_USE_INTRINSICS
# if defined(_MSC_VER) && 0   && \
(defined(_M_IX86_FP) || defined(_M_AMD64) || defined(_M_X64))
#  include <intrin.h>
#  define WGB_INTRIN_SBC(x,y,cin,res) _subborrow_u8(cin,x,y,&res)
#  define WGB_INTRIN_ADC(x,y,cin,res) _addcarry_u8(cin,x,y,&res)
# endif
# if __has_builtin(__builtin_sub_overflow)
#  define WGB_INTRIN_SBC(x,y,cin,res) __builtin_sub_overflow(x,y+cin,&res)
#  define WGB_INTRIN_ADC(x,y,cin,res) __builtin_add_overflow(x,y+cin,&res)
# endif
#endif
#if defined(WGB_INTRIN_SBC)
# define WGB_INSTR_SBC_R8(r,cin)						\
{									\
uint8_t temp;							\
gb->cpu_reg.f.f_bits.c = WGB_INTRIN_SBC(gb->cpu_reg.a,r,cin,temp);\
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0;\
gb->cpu_reg.f.f_bits.n = 1;					\
gb->cpu_reg.f.f_bits.z = (temp == 0x00);			\
gb->cpu_reg.a = temp;						\
}
# define WGB_INSTR_CP_R8(r)							\
{									\
uint8_t temp;							\
gb->cpu_reg.f.f_bits.c = WGB_INTRIN_SBC(gb->cpu_reg.a,r,0,temp);\
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0;\
gb->cpu_reg.f.f_bits.n = 1;					\
gb->cpu_reg.f.f_bits.z = (temp == 0x00);			\
}
#else
# define WGB_INSTR_SBC_R8(r,cin)						\
{									\
uint16_t temp = gb->cpu_reg.a - (r + cin);			\
gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0; \
gb->cpu_reg.f.f_bits.n = 1;					\
gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
gb->cpu_reg.a = (temp & 0xFF);					\
}
# define WGB_INSTR_CP_R8(r)							\
{									\
uint16_t temp = gb->cpu_reg.a - r;				\
gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0; \
gb->cpu_reg.f.f_bits.n = 1;					\
gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
}
#endif
#if defined(WGB_INTRIN_ADC)
# define WGB_INSTR_ADC_R8(r,cin)						\
{									\
uint8_t temp;							\
gb->cpu_reg.f.f_bits.c = WGB_INTRIN_ADC(gb->cpu_reg.a,r,cin,temp);\
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0; \
gb->cpu_reg.f.f_bits.n = 0;					\
gb->cpu_reg.f.f_bits.z = (temp == 0x00);			\
gb->cpu_reg.a = temp;						\
}
#else
# define WGB_INSTR_ADC_R8(r,cin)						\
{									\
uint16_t temp = gb->cpu_reg.a + r + cin;			\
gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0; \
gb->cpu_reg.f.f_bits.n = 0;					\
gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
gb->cpu_reg.a = (temp & 0xFF);					\
}
#endif
#define WGB_INSTR_INC_R8(r)							\
r++;									\
gb->cpu_reg.f.f_bits.h = ((r & 0x0F) == 0x00);				\
gb->cpu_reg.f.f_bits.n = 0;						\
gb->cpu_reg.f.f_bits.z = (r == 0x00)
#define WGB_INSTR_DEC_R8(r)							\
r--;									\
gb->cpu_reg.f.f_bits.h = ((r & 0x0F) == 0x0F);				\
gb->cpu_reg.f.f_bits.n = 1;						\
gb->cpu_reg.f.f_bits.z = (r == 0x00)
#define WGB_INSTR_XOR_R8(r)							\
gb->cpu_reg.a ^= r;							\
gb->cpu_reg.f.reg = 0;							\
gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00)
#define WGB_INSTR_OR_R8(r)							\
gb->cpu_reg.a |= r;							\
gb->cpu_reg.f.reg = 0;							\
gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00)
#define WGB_INSTR_AND_R8(r)							\
gb->cpu_reg.a &= r;							\
gb->cpu_reg.f.reg = 0;							\
gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00);			\
gb->cpu_reg.f.f_bits.h = 1
#if WALNUT_GB_IS_LITTLE_ENDIAN
# define WALNUT_GB_GET_LSB16(x) (x & 0xFF)
# define WALNUT_GB_GET_MSB16(x) (x >> 8)
# define WALNUT_GB_GET_MSN16(x) (x >> 12)
# define WALNUT_GB_U8_TO_U16(h,l) ((l) | ((h) << 8))
#else
# define WALNUT_GB_GET_LSB16(x) (x >> 8)
# define WALNUT_GB_GET_MSB16(x) (x & 0xFF)
# define WALNUT_GB_GET_MSN16(x) ((x & 0xF0) >> 4)
# define WALNUT_GB_U8_TO_U16(h,l) ((h) | ((l) << 8))
#endif
static inline uint16_t bgr555_to_rgb565BE_accurate(uint16_t c)
{
uint16_t r = c & 0x001F;
uint16_t g = (c >> 5) & 0x001F;
uint16_t b = (c >> 10) & 0x001F;
g = (g << 1) | (g >> 4);
uint16_t tempRGB565 = (r << 11) | (g << 5) | b;
return (tempRGB565 << 8) | (tempRGB565 >> 8);
}
static inline uint16_t bgr555_to_rgb565_accurate(uint16_t c)
{
uint16_t r = c & 0x001F;
uint16_t g = (c >> 5) & 0x001F;
uint16_t b = (c >> 10) & 0x001F;
g = (g << 1) | (g >> 4);
return (r << 11) | (g << 5) | b;
}
static inline uint16_t bgr555_to_rgb565_fast(uint16_t c)
{
return ((c & 0x7C00) >> 10) | ((c & 0x03E0) << 1) | ((c & 0x001F) << 11);
}
struct cpu_registers_s
{
#if WALNUT_GB_IS_LITTLE_ENDIAN
# define WALNUT_GB_LE_REG(x,y) x,y
#else
# define WALNUT_GB_LE_REG(x,y) y,x
#endif
union {
struct {
uint8_t  : 4;
uint8_t c: 1;
uint8_t h: 1;
uint8_t n: 1;
uint8_t z: 1;
} f_bits;
uint8_t reg;
} f;
uint8_t a;
union
{
struct
{
uint8_t WALNUT_GB_LE_REG(c,b);
} bytes;
uint16_t reg;
} bc;
union
{
struct
{
uint8_t WALNUT_GB_LE_REG(e,d);
} bytes;
uint16_t reg;
} de;
union
{
struct
{
uint8_t WALNUT_GB_LE_REG(l,h);
} bytes;
uint16_t reg;
} hl;
union
{
struct
{
uint8_t WALNUT_GB_LE_REG(p, s);
} bytes;
uint16_t reg;
} sp;
union
{
struct
{
uint8_t WALNUT_GB_LE_REG(c, p);
} bytes;
uint16_t reg;
} pc;
#undef WALNUT_GB_LE_REG
};
struct count_s
{
uint_fast16_t lcd_count;
uint_fast16_t div_count;
uint_fast16_t tima_count;
uint_fast16_t serial_count;
uint_fast32_t rtc_count;
uint_fast32_t lcd_off_count;
};
#if ENABLE_LCD
#define LCD_COLOUR	0x03
# if WALNUT_GB_12_COLOUR
#define LCD_PALETTE_OBJ	0x10
#define LCD_PALETTE_BG	0x20
#define LCD_PALETTE_ALL 0x30
# endif
#endif
enum gb_error_e
{
GB_UNKNOWN_ERROR = 0,
GB_INVALID_OPCODE = 1,
GB_INVALID_READ = 2,
GB_INVALID_WRITE = 3,
GB_HALT_FOREVER WGB_DEPRECATED("Error no longer issued by Walnut-GB") = 4,
GB_INVALID_MAX = 5
};
enum gb_init_error_e
{
GB_INIT_NO_ERROR = 0,
GB_INIT_CARTRIDGE_UNSUPPORTED,
GB_INIT_INVALID_CHECKSUM,
GB_INIT_INVALID_MAX
};
enum gb_serial_rx_ret_e
{
GB_SERIAL_RX_SUCCESS = 0,
GB_SERIAL_RX_NO_CONNECTION = 1
};
union cart_rtc
{
struct
{
uint8_t sec;
uint8_t min;
uint8_t hour;
uint8_t yday;
uint8_t high;
} reg;
uint8_t bytes[5];
};
struct gb_s
{
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
bool prefetch_invalid;
#endif
uint8_t (*gb_rom_read)(struct gb_s*, const uint_fast32_t addr);
uint16_t (*gb_rom_read_16bit)(struct gb_s*, const uint_fast32_t addr);
uint32_t (*gb_rom_read_32bit)(struct gb_s*, const uint_fast32_t addr);
uint8_t (*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t addr);
void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t addr,
const uint8_t val);
void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t addr);
void (*gb_serial_tx)(struct gb_s*, const uint8_t tx);
enum gb_serial_rx_ret_e (*gb_serial_rx)(struct gb_s*, uint8_t* rx);
uint8_t (*gb_bootrom_read)(struct gb_s*, const uint_fast16_t addr);
struct
{
bool gb_halt	: 1;
bool gb_ime	: 1;
bool gb_frame	: 1;
bool lcd_blank	: 1;
bool cart_is_mbc3O : 1;
};
int8_t mbc;
uint8_t cart_ram;
uint16_t num_rom_banks_mask;
uint8_t num_ram_banks;
uint16_t selected_rom_bank;
uint8_t cart_ram_bank;
uint8_t enable_cart_ram;
uint8_t cart_mode_select;
union cart_rtc rtc_latched, rtc_real;
struct cpu_registers_s cpu_reg;
struct count_s counter;
uint8_t wram[WRAM_SIZE];
uint8_t vram[VRAM_SIZE];
uint8_t oam[OAM_SIZE];
uint8_t hram_io[HRAM_IO_SIZE];
struct
{
void (*lcd_draw_line)(struct gb_s *gb,
const uint8_t *pixels,
const uint_fast8_t line);
uint8_t bg_palette[4];
uint8_t sp_palette[8];
uint8_t window_clear;
uint8_t WY;
bool frame_skip_count : 1;
bool interlace_count : 1;
} display;
#if WALNUT_FULL_GBC_SUPPORT
struct {
uint8_t cgbMode;
uint8_t doubleSpeed;
uint8_t doubleSpeedPrep;
uint8_t wramBank;
uint16_t wramBankOffset;
uint8_t vramBank;
uint16_t vramBankOffset;
uint16_t fixPalette[0x40];
uint8_t OAMPalette[0x40];
uint8_t BGPalette[0x40];
uint8_t OAMPaletteID;
uint8_t BGPaletteID;
uint8_t OAMPaletteInc;
uint8_t BGPaletteInc;
uint8_t dmaActive;
uint8_t dmaMode;
uint8_t dmaSize;
uint16_t dmaSource;
uint16_t dmaDest;
} cgb;
#endif
struct
{
bool interlace : 1;
bool frame_skip : 1;
union
{
struct
{
bool a		: 1;
bool b		: 1;
bool select	: 1;
bool start	: 1;
bool right	: 1;
bool left	: 1;
bool up		: 1;
bool down	: 1;
} joypad_bits;
uint8_t joypad;
};
void *priv;
} direct;
};
#ifndef WALNUT_GB_HEADER_ONLY
#define IO_JOYP	0x00
#define IO_SB	0x01
#define IO_SC	0x02
#define IO_DIV	0x04
#define IO_TIMA	0x05
#define IO_TMA	0x06
#define IO_TAC	0x07
#define IO_IF	0x0F
#define IO_LCDC	0x40
#define IO_STAT	0x41
#define IO_SCY	0x42
#define IO_SCX	0x43
#define IO_LY	0x44
#define IO_LYC	0x45
#define	IO_DMA	0x46
#define	IO_BGP	0x47
#define	IO_OBP0	0x48
#define IO_OBP1	0x49
#define IO_WY	0x4A
#define IO_WX	0x4B
#define IO_BOOT	0x50
#define IO_IE	0xFF
#define IO_TAC_RATE_MASK	0x3
#define IO_TAC_ENABLE_MASK	0x4
#define IO_STAT_MODE_HBLANK		0
#define IO_STAT_MODE_VBLANK		1
#define IO_STAT_MODE_OAM_SCAN		2
#define IO_STAT_MODE_LCD_DRAW		3
#define IO_STAT_MODE_VBLANK_OR_TRANSFER_MASK 0x1
#if WALNUT_GB_16BIT_ALIGNED
uint16_t __gb_read16(struct gb_s *gb, uint16_t addr)
{
switch (WALNUT_GB_GET_MSN16(addr))
{
case 0x0:
if (gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
return (uint16_t)gb->gb_bootrom_read(gb, addr)
| ((uint16_t)gb->gb_bootrom_read(gb, addr + 1) << 8);
#if WALNUT_FULL_GBC_SUPPORT
else if (gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 && addr >= 0x0200 && addr < 0x0900)
return (uint16_t)gb->gb_bootrom_read(gb, addr)
| ((uint16_t)gb->gb_bootrom_read(gb, addr + 1) << 8);
#endif
case 0x1:
case 0x2:
case 0x3:
return gb->gb_rom_read_16bit(gb, addr);
case 0x4:
case 0x5:
case 0x6:
case 0x7:
{
uint32_t bank_offset = (gb->selected_rom_bank - 1) * ROM_BANK_SIZE;
return gb->gb_rom_read_16bit(gb, addr + bank_offset);
}
case 0x8:
case 0x9:
{
uint8_t *vram_base =
#if WALNUT_FULL_GBC_SUPPORT
&gb->vram[addr - gb->cgb.vramBankOffset];
#else
&gb->vram[addr - VRAM_ADDR];
#endif
if (addr + 1 < 0xA000)
{
if (((uintptr_t)vram_base & 1) == 0)
return *(uint16_t *)vram_base;
else
return (uint16_t)vram_base[0] | ((uint16_t)vram_base[1] << 8);
}
return vram_base[0];
}
case 0xA:
case 0xB:
if (gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
return gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08]
| ((uint16_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 1] << 8);
else if (gb->cart_ram && gb->enable_cart_ram)
{
uint16_t offset;
if (gb->mbc == 2)
offset = addr & 0x1FF;
else if ((gb->cart_mode_select || gb->mbc != 1) && gb->cart_ram_bank < gb->num_ram_banks)
offset = addr - CART_RAM_ADDR + (gb->cart_ram_bank * CRAM_BANK_SIZE);
else
offset = addr - CART_RAM_ADDR;
return gb->gb_cart_ram_read(gb, offset)
| ((uint16_t)gb->gb_cart_ram_read(gb, offset + 1) << 8);
}
return 0xFFFF;
case 0xC:
case 0xD:
{
uint8_t *wram_ptr =
#if WALNUT_FULL_GBC_SUPPORT
(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
? &gb->wram[addr - gb->cgb.wramBankOffset]
: &gb->wram[addr - WRAM_0_ADDR];
#else
&gb->wram[addr - WRAM_0_ADDR];
#endif
if (((uintptr_t)wram_ptr & 1) == 0)
return *(uint16_t *)wram_ptr;
else
return (uint16_t)wram_ptr[0] | ((uint16_t)wram_ptr[1] << 8);
}
case 0xE:
{
uint8_t *echo_ptr = &gb->wram[addr - ECHO_ADDR];
if (((uintptr_t)echo_ptr & 1) == 0)
return *(uint16_t *)echo_ptr;
else
return (uint16_t)echo_ptr[0] | ((uint16_t)echo_ptr[1] << 8);
}
case 0xF:
if (addr < 0xFEA0)
{
uint8_t *oam_ptr = &gb->oam[addr - OAM_ADDR];
if (((uintptr_t)oam_ptr & 1) == 0)
return *(uint16_t *)oam_ptr;
else
return (uint16_t)oam_ptr[0] | ((uint16_t)oam_ptr[1] << 8);
}
else if (addr >= IO_ADDR)
{
#if ENABLE_SOUND
if (addr >= 0xFF10 && addr <= 0xFF3F)
{
uint8_t lo = audio_read(addr);
uint8_t hi = audio_read(addr + 1);
return lo | ((uint16_t)hi << 8);
}
#endif
return (uint16_t)gb->hram_io[addr - IO_ADDR]
| ((uint16_t)gb->hram_io[addr + 1 - IO_ADDR] << 8);
}
else
return 0xFFFF;
}
(gb->gb_error)(gb, GB_INVALID_READ, addr);
WGB_UNREACHABLE();
}
#else
uint16_t __gb_read16(struct gb_s *gb, uint16_t addr)
{
switch(WALNUT_GB_GET_MSN16(addr))
{
case 0x0:
if(gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
return (uint16_t)gb->gb_bootrom_read(gb, addr)
| ((uint16_t)gb->gb_bootrom_read(gb, addr + 1) << 8);
#if WALNUT_FULL_GBC_SUPPORT
else if(gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 && addr >= 0x0200 && addr < 0x0900)
return (uint16_t)gb->gb_bootrom_read(gb, addr)
| ((uint16_t)gb->gb_bootrom_read(gb, addr + 1) << 8);
#endif
case 0x1:
case 0x2:
case 0x3:
return gb->gb_rom_read_16bit(gb,addr);
case 0x4:
case 0x5:
case 0x6:
case 0x7:
{
uint32_t bank_offset = (gb->selected_rom_bank - 1) * ROM_BANK_SIZE;
return gb->gb_rom_read_16bit(gb,addr + bank_offset);
}
case 0x8:
case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
if (addr + 1 < 0xA000)
return *(uint16_t *)&gb->vram[addr - gb->cgb.vramBankOffset];
else
return gb->vram[addr - gb->cgb.vramBankOffset];
#else
if (addr + 1 < 0xA000)
return *(uint16_t *)&gb->vram[addr - VRAM_ADDR];
else
return gb->vram[addr - VRAM_ADDR];
#endif
case 0xA:
case 0xB:
if (gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
{
return gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08]
| ((uint16_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 1] << 8);
}
else if (gb->cart_ram && gb->enable_cart_ram)
{
uint16_t offset;
if (gb->mbc == 2)
{
addr &= 0x1FF;
offset = addr;
}
else if ((gb->cart_mode_select || gb->mbc != 1) && gb->cart_ram_bank < gb->num_ram_banks)
{
offset = addr - CART_RAM_ADDR + (gb->cart_ram_bank * CRAM_BANK_SIZE);
}
else
{
offset = addr - CART_RAM_ADDR;
}
return gb->gb_cart_ram_read(gb, offset) | ((uint16_t)gb->gb_cart_ram_read(gb, offset + 1) << 8);
}
return 0xFFFF;
case 0xC:
case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
return *(uint16_t *)&gb->wram[addr - gb->cgb.wramBankOffset];
#endif
return *(uint16_t *)&gb->wram[addr - WRAM_0_ADDR];
case 0xE:
return *(uint16_t *)&gb->wram[addr - ECHO_ADDR];
case 0xF:
if (addr < 0xFEA0)
return *(uint16_t *)&gb->oam[addr - OAM_ADDR];
else if (addr >= IO_ADDR)
{
#if ENABLE_SOUND
if (addr >= 0xFF10 && addr <= 0xFF3F)
{
uint8_t lo = audio_read(addr);
uint8_t hi = audio_read(addr + 1);
return lo | ((uint16_t)hi << 8);
}
#endif
#if WALNUT_FULL_GBC_SUPPORT
switch (addr & 0xFF)
{
case 0x4D:
return gb->cgb.doubleSpeed | ((uint16_t)gb->cgb.doubleSpeedPrep << 8);
case 0x4F:
return gb->cgb.vramBank | 0xFE00;
case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
{
uint8_t lo = gb->hram_io[addr - IO_ADDR];
uint8_t hi = gb->hram_io[addr + 1 - IO_ADDR];
return lo | ((uint16_t)hi << 8);
}
case 0x56:
return gb->hram_io[0x56] | ((uint16_t)gb->hram_io[0x57] << 8);
case 0x68: case 0x69: case 0x6A: case 0x6B:
{
uint8_t lo = gb->hram_io[addr - IO_ADDR];
uint8_t hi = gb->hram_io[addr + 1 - IO_ADDR];
return lo | ((uint16_t)hi << 8);
}
case 0x70:
return gb->cgb.wramBank | 0x00;
default:
return *(uint16_t *)&gb->hram_io[addr - IO_ADDR];
}
#else
return *(uint16_t *)&gb->hram_io[addr - IO_ADDR];
#endif
}
else
{
return 0xFFFF;
}
}
(gb->gb_error)(gb, GB_INVALID_READ, addr);
WGB_UNREACHABLE();
}
#endif
#if WALNUT_GB_32BIT_ALIGNED
uint32_t __gb_read32(struct gb_s *gb, uint16_t addr)
{
switch (WALNUT_GB_GET_MSN16(addr))
{
case 0x0:
if (gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
return (uint32_t)gb->gb_bootrom_read(gb, addr)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 1) << 8)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 2) << 16)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 3) << 24);
#if WALNUT_FULL_GBC_SUPPORT
else if (gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
addr >= 0x0200 && addr < 0x0900)
return (uint32_t)gb->gb_bootrom_read(gb, addr)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 1) << 8)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 2) << 16)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 3) << 24);
#endif
case 0x1:
case 0x2:
case 0x3:
return gb->gb_rom_read_32bit(gb, addr);
case 0x4:
case 0x5:
case 0x6:
case 0x7:
{
uint32_t bank_offset = (gb->selected_rom_bank - 1) * ROM_BANK_SIZE;
return gb->gb_rom_read_32bit(gb, addr + bank_offset);
}
case 0x8:
case 0x9:
{
#if WALNUT_FULL_GBC_SUPPORT
uint8_t *p = &gb->vram[addr - gb->cgb.vramBankOffset];
#else
uint8_t *p = &gb->vram[addr - VRAM_ADDR];
#endif
if (addr + 3 < 0xA000 && (((uintptr_t)p & 3) == 0))
return *(uint32_t *)p;
return (uint32_t)p[0]
| ((uint32_t)p[1] << 8)
| ((uint32_t)p[2] << 16)
| ((uint32_t)p[3] << 24);
}
case 0xA:
case 0xB:
if (gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
{
const uint8_t *p =
&gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08];
return (uint32_t)p[0]
| ((uint32_t)p[1] << 8)
| ((uint32_t)p[2] << 16)
| ((uint32_t)p[3] << 24);
}
else if (gb->cart_ram && gb->enable_cart_ram)
{
uint16_t offset;
if (gb->mbc == 2)
{
addr &= 0x1FF;
offset = addr;
}
else if ((gb->cart_mode_select || gb->mbc != 1) &&
gb->cart_ram_bank < gb->num_ram_banks)
{
offset = addr - CART_RAM_ADDR +
(gb->cart_ram_bank * CRAM_BANK_SIZE);
}
else
{
offset = addr - CART_RAM_ADDR;
}
return (uint32_t)gb->gb_cart_ram_read(gb, offset)
| ((uint32_t)gb->gb_cart_ram_read(gb, offset + 1) << 8)
| ((uint32_t)gb->gb_cart_ram_read(gb, offset + 2) << 16)
| ((uint32_t)gb->gb_cart_ram_read(gb, offset + 3) << 24);
}
return 0xFFFFFFFF;
case 0xC:
case 0xD:
{
#if WALNUT_FULL_GBC_SUPPORT
if (gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
{
uint8_t *p = &gb->wram[addr - gb->cgb.wramBankOffset];
if (((uintptr_t)p & 3) == 0)
return *(uint32_t *)p;
return (uint32_t)p[0]
| ((uint32_t)p[1] << 8)
| ((uint32_t)p[2] << 16)
| ((uint32_t)p[3] << 24);
}
#endif
uint8_t *p = &gb->wram[addr - WRAM_0_ADDR];
if (((uintptr_t)p & 3) == 0)
return *(uint32_t *)p;
return (uint32_t)p[0]
| ((uint32_t)p[1] << 8)
| ((uint32_t)p[2] << 16)
| ((uint32_t)p[3] << 24);
}
case 0xE:
{
uint8_t *p = &gb->wram[addr - ECHO_ADDR];
if (((uintptr_t)p & 3) == 0)
return *(uint32_t *)p;
return (uint32_t)p[0]
| ((uint32_t)p[1] << 8)
| ((uint32_t)p[2] << 16)
| ((uint32_t)p[3] << 24);
}
case 0xF:
if (addr < 0xFEA0)
{
uint8_t *p = &gb->oam[addr - OAM_ADDR];
if (((uintptr_t)p & 3) == 0)
return *(uint32_t *)p;
return (uint32_t)p[0]
| ((uint32_t)p[1] << 8)
| ((uint32_t)p[2] << 16)
| ((uint32_t)p[3] << 24);
}
else if (addr >= IO_ADDR)
{
#if ENABLE_SOUND
if (addr >= 0xFF10 && addr <= 0xFF3F)
{
uint32_t v = 0;
for (int i = 0; i < 4; ++i)
v |= ((uint32_t)audio_read(addr + i)) << (8 * i);
return v;
}
#endif
uint8_t *p = &gb->hram_io[addr - IO_ADDR];
if (((uintptr_t)p & 3) == 0)
return *(uint32_t *)p;
return (uint32_t)p[0]
| ((uint32_t)p[1] << 8)
| ((uint32_t)p[2] << 16)
| ((uint32_t)p[3] << 24);
}
return 0xFFFFFFFF;
}
(gb->gb_error)(gb, GB_INVALID_READ, addr);
WGB_UNREACHABLE();
}
#else
uint32_t __gb_read32(struct gb_s *gb, uint16_t addr)
{
switch (WALNUT_GB_GET_MSN16(addr))
{
case 0x0:
if (gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
return (uint32_t)gb->gb_bootrom_read(gb, addr)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 1) << 8)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 2) << 16)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 3) << 24);
#if WALNUT_FULL_GBC_SUPPORT
else if (gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 &&
addr >= 0x0200 && addr < 0x0900)
return (uint32_t)gb->gb_bootrom_read(gb, addr)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 1) << 8)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 2) << 16)
| ((uint32_t)gb->gb_bootrom_read(gb, addr + 3) << 24);
#endif
case 0x1:
case 0x2:
case 0x3:
return gb->gb_rom_read_32bit(gb,addr);
case 0x4:
case 0x5:
case 0x6:
case 0x7:
{
uint32_t bank_offset = (gb->selected_rom_bank - 1) * ROM_BANK_SIZE;
return gb->gb_rom_read_32bit(gb,addr + bank_offset);
}
case 0x8:
case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
if (addr + 3 < 0xA000)
return *(uint32_t *)&gb->vram[addr - gb->cgb.vramBankOffset];
else
return *(uint16_t *)&gb->vram[addr - gb->cgb.vramBankOffset];
#else
if (addr + 3 < 0xA000)
return *(uint32_t *)&gb->vram[addr - VRAM_ADDR];
else
return *(uint16_t *)&gb->vram[addr - VRAM_ADDR];
#endif
case 0xA:
case 0xB:
if (gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
{
return (uint32_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08]
| ((uint32_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 1] << 8)
| ((uint32_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 2] << 16)
| ((uint32_t)gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08 + 3] << 24);
}
else if (gb->cart_ram && gb->enable_cart_ram)
{
uint16_t offset;
if (gb->mbc == 2)
{
addr &= 0x1FF;
offset = addr;
}
else if ((gb->cart_mode_select || gb->mbc != 1) &&
gb->cart_ram_bank < gb->num_ram_banks)
{
offset = addr - CART_RAM_ADDR +
(gb->cart_ram_bank * CRAM_BANK_SIZE);
}
else
{
offset = addr - CART_RAM_ADDR;
}
return (uint32_t)gb->gb_cart_ram_read(gb, offset)
| ((uint32_t)gb->gb_cart_ram_read(gb, offset + 1) << 8)
| ((uint32_t)gb->gb_cart_ram_read(gb, offset + 2) << 16)
| ((uint32_t)gb->gb_cart_ram_read(gb, offset + 3) << 24);
}
return 0xFFFFFFFF;
case 0xC:
case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
if (gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
return *(uint32_t *)&gb->wram[addr - gb->cgb.wramBankOffset];
#endif
return *(uint32_t *)&gb->wram[addr - WRAM_0_ADDR];
case 0xE:
return *(uint32_t *)&gb->wram[addr - ECHO_ADDR];
case 0xF:
if (addr < 0xFEA0)
return *(uint32_t *)&gb->oam[addr - OAM_ADDR];
else if (addr >= IO_ADDR)
{
#if ENABLE_SOUND
if (addr >= 0xFF10 && addr <= 0xFF3F)
{
uint32_t v = 0;
for (int i = 0; i < 4; ++i)
v |= ((uint32_t)audio_read(addr + i)) << (8 * i);
return v;
}
#endif
#if WALNUT_FULL_GBC_SUPPORT
switch (addr & 0xFF)
{
case 0x4D:
return gb->cgb.doubleSpeed |
((uint32_t)gb->cgb.doubleSpeedPrep << 8);
case 0x4F:
return gb->cgb.vramBank | 0xFE00;
case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
case 0x56: case 0x68: case 0x69: case 0x6A: case 0x6B:
case 0x70:
return *(uint32_t *)&gb->hram_io[addr - IO_ADDR];
default:
return *(uint32_t *)&gb->hram_io[addr - IO_ADDR];
}
#else
return *(uint32_t *)&gb->hram_io[addr - IO_ADDR];
#endif
}
return 0xFFFFFFFF;
}
(gb->gb_error)(gb, GB_INVALID_READ, addr);
WGB_UNREACHABLE();
}
#endif
uint8_t __gb_read(struct gb_s *gb, uint16_t addr)
{
switch(WALNUT_GB_GET_MSN16(addr))
{
case 0x0:
if(gb->hram_io[IO_BOOT] == 0 && addr < 0x0100)
{
return gb->gb_bootrom_read(gb, addr);
}
#if WALNUT_FULL_GBC_SUPPORT
else if (gb->cgb.cgbMode && gb->hram_io[IO_BOOT] == 0 && addr < 0x0900 && addr >= 0x0200)
{
return gb->gb_bootrom_read(gb, addr);
}
#endif
case 0x1:
case 0x2:
case 0x3:
return gb->gb_rom_read(gb, addr);
case 0x4:
case 0x5:
case 0x6:
case 0x7:
if(gb->mbc == 1 && gb->cart_mode_select)
return gb->gb_rom_read(gb,
addr + ((gb->selected_rom_bank & 0x1F) - 1) * ROM_BANK_SIZE);
else
return gb->gb_rom_read(gb, addr + (gb->selected_rom_bank - 1) * ROM_BANK_SIZE);
case 0x8:
case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
return gb->vram[addr - gb->cgb.vramBankOffset];
#else
return gb->vram[addr - VRAM_ADDR];
#endif
case 0xA:
case 0xB:
if(gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
{
return gb->rtc_latched.bytes[gb->cart_ram_bank - 0x08];
}
else if(gb->cart_ram && gb->enable_cart_ram)
{
if(gb->mbc == 2)
{
addr &= 0x1FF;
return gb->gb_cart_ram_read(gb, addr);
}
else if((gb->cart_mode_select || gb->mbc != 1) &&
gb->cart_ram_bank < gb->num_ram_banks)
{
return gb->gb_cart_ram_read(gb, addr - CART_RAM_ADDR +
(gb->cart_ram_bank * CRAM_BANK_SIZE));
}
else
return gb->gb_cart_ram_read(gb, addr - CART_RAM_ADDR);
}
return 0xFF;
case 0xC:
case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode && addr >= WRAM_1_ADDR)
return gb->wram[addr - gb->cgb.wramBankOffset];
#endif
return gb->wram[addr - WRAM_0_ADDR];
case 0xE:
return gb->wram[addr - ECHO_ADDR];
case 0xF:
if(addr < OAM_ADDR)
#if WALNUT_FULL_GBC_SUPPORT
return gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset];
#else
return gb->wram[addr - ECHO_ADDR];
#endif
if(addr < UNUSED_ADDR)
return gb->oam[addr - OAM_ADDR];
if(addr < IO_ADDR)
return 0xFF;
if((addr >= 0xFF10) && (addr <= 0xFF3F))
{
#if ENABLE_SOUND
return audio_read(addr);
#else
static const uint8_t ortab[] = {
0x80, 0x3f, 0x00, 0xff, 0xbf,
0xff, 0x3f, 0x00, 0xff, 0xbf,
0x7f, 0xff, 0x9f, 0xff, 0xbf,
0xff, 0xff, 0x00, 0x00, 0xbf,
0x00, 0x00, 0x70,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
return gb->hram_io[addr - IO_ADDR] | ortab[addr - IO_ADDR];
#endif
}
#if WALNUT_FULL_GBC_SUPPORT
switch (addr & 0xFF)
{
case 0x4D:
return (gb->cgb.doubleSpeed << 7) + gb->cgb.doubleSpeedPrep;
case 0x4F:
return gb->cgb.vramBank | 0xFE;
case 0x51:
return (gb->cgb.dmaSource >> 8);
case 0x52:
return (gb->cgb.dmaSource & 0xF0);
case 0x53:
return (gb->cgb.dmaDest >> 8);
case 0x54:
return (gb->cgb.dmaDest & 0xF0);
case 0x55:
return (gb->cgb.dmaActive << 7) | (gb->cgb.dmaSize - 1);
case 0x56:
return gb->hram_io[0x56];
case 0x68:
return (gb->cgb.BGPaletteID & 0x3F) + (gb->cgb.BGPaletteInc << 7);
case 0x69:
return gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3F)];
case 0x6A:
return (gb->cgb.OAMPaletteID & 0x3F) + (gb->cgb.OAMPaletteInc << 7);
case 0x6B:
return gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3F)];
case 0x70:
return gb->cgb.wramBank;
default:
#endif
if(addr >= IO_ADDR)
return gb->hram_io[addr - IO_ADDR];
#if WALNUT_FULL_GBC_SUPPORT
}
#endif
}
(gb->gb_error)(gb, GB_INVALID_READ, addr);
WGB_UNREACHABLE();
}
void __gb_write(struct gb_s *gb, uint_fast16_t addr, uint8_t val)
{
switch(WALNUT_GB_GET_MSN16(addr))
{
case 0x0:
case 0x1:
if (gb->mbc > 0 && gb->mbc != 2)
{
if (gb->cart_ram)
gb->enable_cart_ram = ((val & 0x0F) == 0x0A);
return;
}
case 0x2:
if (gb->mbc == 5)
{
gb->selected_rom_bank =
(gb->selected_rom_bank & 0x100) | val;
gb->selected_rom_bank =
gb->selected_rom_bank & gb->num_rom_banks_mask;
#if WALNUT_GB_SAFE_DUALFETCH_MBC
gb->prefetch_invalid=true;
#endif
return;
}
case 0x3:
if(gb->mbc == 1)
{
gb->selected_rom_bank = (val & 0x1F) | (gb->selected_rom_bank & 0x60);
if((gb->selected_rom_bank & 0x1F) == 0x00)
gb->selected_rom_bank++;
#if WALNUT_GB_SAFE_DUALFETCH_MBC
gb->prefetch_invalid=true;
#endif
}
else if(gb->mbc == 2)
{
if(addr & 0x100)
{
gb->selected_rom_bank = val & 0x0F;
if(!gb->selected_rom_bank)
gb->selected_rom_bank++;
}
else
{
gb->enable_cart_ram = ((val & 0x0F) == 0x0A);
return;
}
}
else if(gb->mbc == 3)
{
gb->selected_rom_bank = val;
if(!gb->cart_is_mbc3O)
gb->selected_rom_bank = val & 0x7F;
if(!gb->selected_rom_bank)
gb->selected_rom_bank++;
}
else if(gb->mbc == 5)
gb->selected_rom_bank = (val & 0x01) << 8 | (gb->selected_rom_bank & 0xFF);
gb->selected_rom_bank = gb->selected_rom_bank & gb->num_rom_banks_mask;
#if WALNUT_GB_SAFE_DUALFETCH_MBC
gb->prefetch_invalid=true;
#endif
return;
case 0x4:
case 0x5:
if(gb->mbc == 1)
{
gb->cart_ram_bank = (val & 3);
gb->selected_rom_bank = ((val & 3) << 5) | (gb->selected_rom_bank & 0x1F);
gb->selected_rom_bank = gb->selected_rom_bank & gb->num_rom_banks_mask;
}
else if(gb->mbc == 3)
{
gb->cart_ram_bank = val;
if(!gb->cart_is_mbc3O && gb->cart_ram_bank < 0x8)
gb->cart_ram_bank &= 0x3;
}
else if(gb->mbc == 5)
gb->cart_ram_bank = (val & 0x0F);
#if WALNUT_GB_SAFE_DUALFETCH_MBC
gb->prefetch_invalid=true;
#endif
return;
case 0x6:
case 0x7:
val &= 1;
if(gb->mbc == 3 && val && gb->cart_mode_select == 0)
memcpy(&gb->rtc_latched.bytes, &gb->rtc_real.bytes, sizeof(gb->rtc_latched.bytes));
gb->cart_mode_select = val;
#if WALNUT_GB_SAFE_DUALFETCH_MBC
gb->prefetch_invalid=true;
#endif
return;
case 0x8:
case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
gb->vram[addr - gb->cgb.vramBankOffset] = val;
#else
gb->vram[addr - VRAM_ADDR] = val;
#endif
return;
case 0xA:
case 0xB:
if(gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
{
const uint8_t rtc_reg_mask[5] = {
0x3F, 0x3F, 0x1F, 0xFF, 0xC1
};
uint8_t reg = gb->cart_ram_bank - 0x08;
gb->rtc_real.bytes[reg] = val & rtc_reg_mask[reg];
}
else if(gb->cart_ram && gb->enable_cart_ram)
{
if(gb->mbc == 2)
{
addr &= 0x1FF;
val &= 0x0F;
val |= 0xF0;
gb->gb_cart_ram_write(gb, addr, val);
}
else if(((gb->mbc == 1 && gb->cart_mode_select) || gb->mbc != 1) &&
gb->cart_ram_bank < gb->num_ram_banks)
{
gb->gb_cart_ram_write(gb,
addr - CART_RAM_ADDR + (gb->cart_ram_bank * CRAM_BANK_SIZE), val);
}
else if(gb->num_ram_banks)
gb->gb_cart_ram_write(gb, addr - CART_RAM_ADDR, val);
}
return;
case 0xC:
gb->wram[addr - WRAM_0_ADDR] = val;
return;
case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
gb->wram[addr - gb->cgb.wramBankOffset] = val;
#else
gb->wram[addr - WRAM_1_ADDR + WRAM_BANK_SIZE] = val;
#endif
return;
case 0xE:
gb->wram[addr - ECHO_ADDR] = val;
return;
case 0xF:
if(addr < OAM_ADDR)
{
#if WALNUT_FULL_GBC_SUPPORT
gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset] = val;
#else
gb->wram[addr - ECHO_ADDR] = val;
#endif
return;
}
if(addr < UNUSED_ADDR)
{
gb->oam[addr - OAM_ADDR] = val;
return;
}
if(addr < IO_ADDR)
return;
if(HRAM_ADDR <= addr && addr < INTR_EN_ADDR)
{
gb->hram_io[addr - IO_ADDR] = val;
return;
}
if((addr >= 0xFF10) && (addr <= 0xFF3F))
{
#if ENABLE_SOUND
audio_write(addr, val);
#else
gb->hram_io[addr - IO_ADDR] = val;
#endif
return;
}
switch(WALNUT_GB_GET_LSB16(addr))
{
case 0x00:
gb->hram_io[IO_JOYP] = val;
if((gb->hram_io[IO_JOYP] & 0x10) == 0)
gb->hram_io[IO_JOYP] |= (gb->direct.joypad >> 4);
else
gb->hram_io[IO_JOYP] |= (gb->direct.joypad & 0x0F);
return;
case 0x01:
gb->hram_io[IO_SB] = val;
return;
case 0x02:
gb->hram_io[IO_SC] = val;
return;
case 0x04:
gb->hram_io[IO_DIV] = 0x00;
return;
case 0x05:
gb->hram_io[IO_TIMA] = val;
return;
case 0x06:
gb->hram_io[IO_TMA] = val;
return;
case 0x07:
gb->hram_io[IO_TAC] = val;
return;
case 0x0F:
gb->hram_io[IO_IF] = (val | 0xE0);
return;
case 0x40:
{
uint8_t lcd_enabled;
lcd_enabled = (gb->hram_io[IO_LCDC] & LCDC_ENABLE);
gb->hram_io[IO_LCDC] = val;
if (!lcd_enabled && (val & LCDC_ENABLE))
{
gb->lcd_blank = true;
}
else if (lcd_enabled && !(val & LCDC_ENABLE))
{
gb->hram_io[IO_STAT] =
(gb->hram_io[IO_STAT] & ~STAT_MODE) |
IO_STAT_MODE_HBLANK;
gb->hram_io[IO_LY] = 0;
gb->counter.lcd_off_count += gb->counter.lcd_count;
gb->counter.lcd_count = 0;
}
return;
}
case 0x41:
gb->hram_io[IO_STAT] = (val & STAT_USER_BITS) | (gb->hram_io[IO_STAT] & STAT_MODE) | 0x80;
return;
case 0x42:
gb->hram_io[IO_SCY] = val;
return;
case 0x43:
gb->hram_io[IO_SCX] = val;
return;
case 0x45:
gb->hram_io[IO_LYC] = val;
return;
case 0x46:
{
uint16_t dma_addr;
uint16_t i;
#if WALNUT_FULL_GBC_SUPPORT
dma_addr = (uint_fast16_t)(val % 0xF1) << 8;
gb->hram_io[IO_DMA] = (val % 0xF1);
#else
dma_addr = (uint_fast16_t)val << 8;
gb->hram_io[IO_DMA] = val;
#endif
#if WALNUT_GB_32BIT_DMA
#if WALNUT_GB_32BIT_ALIGNED
uint8_t *oam = gb->oam;
if (((uintptr_t)oam & 3))
for (i = 0; i < OAM_SIZE; i += 4)
{
uint32_t v = __gb_read32(gb, dma_addr + i);
oam[i+0] = v;
oam[i+1] = v >> 8;
oam[i+2] = v >> 16;
oam[i+3] = v >> 24;
}
else
for (i = 0; i < OAM_SIZE; i += 4)
{
uint32_t v = __gb_read32(gb, dma_addr + i);
*(uint32_t *)(oam + i) = v;
}
#else
for (i = 0; i < OAM_SIZE; i += 4)
{
uint32_t v = __gb_read32(gb, dma_addr + i);
*((uint32_t *)(gb->oam + i)) = v;
}
#endif
#elif WALNUT_GB_16BIT_DMA
for (i = 0; i < OAM_SIZE; i += 2)
{
uint16_t v = __gb_read16(gb, dma_addr + i);
gb->oam[i]   = v & 0xFF;
gb->oam[i+1] = v >> 8;
}
#else
for (i = 0; i < OAM_SIZE; i++)
gb->oam[i] = __gb_read(gb, dma_addr + i);
#endif
#if WALNUT_GB_SAFE_DUALFETCH_DMA
gb->prefetch_invalid=true;
#endif
return;
}
case 0x47:
gb->hram_io[IO_BGP] = val;
gb->display.bg_palette[0] = (gb->hram_io[IO_BGP] & 0x03);
gb->display.bg_palette[1] = (gb->hram_io[IO_BGP] >> 2) & 0x03;
gb->display.bg_palette[2] = (gb->hram_io[IO_BGP] >> 4) & 0x03;
gb->display.bg_palette[3] = (gb->hram_io[IO_BGP] >> 6) & 0x03;
return;
case 0x48:
gb->hram_io[IO_OBP0] = val;
gb->display.sp_palette[0] = (gb->hram_io[IO_OBP0] & 0x03);
gb->display.sp_palette[1] = (gb->hram_io[IO_OBP0] >> 2) & 0x03;
gb->display.sp_palette[2] = (gb->hram_io[IO_OBP0] >> 4) & 0x03;
gb->display.sp_palette[3] = (gb->hram_io[IO_OBP0] >> 6) & 0x03;
return;
case 0x49:
gb->hram_io[IO_OBP1] = val;
gb->display.sp_palette[4] = (gb->hram_io[IO_OBP1] & 0x03);
gb->display.sp_palette[5] = (gb->hram_io[IO_OBP1] >> 2) & 0x03;
gb->display.sp_palette[6] = (gb->hram_io[IO_OBP1] >> 4) & 0x03;
gb->display.sp_palette[7] = (gb->hram_io[IO_OBP1] >> 6) & 0x03;
return;
case 0x4A:
gb->hram_io[IO_WY] = val;
return;
case 0x4B:
gb->hram_io[IO_WX] = val;
return;
#if WALNUT_FULL_GBC_SUPPORT
case 0x4D:
gb->cgb.doubleSpeedPrep = val & 1;
return;
case 0x4F:
gb->cgb.vramBank = val & 0x01;
if(gb->cgb.cgbMode) gb->cgb.vramBankOffset = VRAM_ADDR - (gb->cgb.vramBank << 13);
return;
#endif
case 0x50:
gb->hram_io[IO_BOOT] = 0x01;
return;
#if WALNUT_FULL_GBC_SUPPORT
case 0x51:
gb->cgb.dmaSource = (gb->cgb.dmaSource & 0xFF) + (val << 8);
return;
case 0x52:
gb->cgb.dmaSource = (gb->cgb.dmaSource & 0xFF00) + val;
return;
case 0x53:
gb->cgb.dmaDest = (gb->cgb.dmaDest & 0xFF) + (val << 8);
return;
case 0x54:
gb->cgb.dmaDest = (gb->cgb.dmaDest & 0xFF00) + val;
return;
case 0x55:
gb->cgb.dmaSize = (val & 0x7F) + 1;
gb->cgb.dmaMode = val >> 7;
if(gb->cgb.dmaActive)
{
#if	WALNUT_GB_32BIT_DMA
if (gb->cgb.cgbMode && !gb->cgb.dmaMode)
{
uint16_t src = gb->cgb.dmaSource & 0xFFF0;
uint16_t dst = (gb->cgb.dmaDest & 0x1FF0) | 0x8000;
uint16_t count = gb->cgb.dmaSize << 4;
for (uint16_t i = 0; i < count; i += 4)
{
uint32_t val32 = __gb_read32(gb, src + i);
__gb_write32(gb, dst + i, val32);
}
gb->cgb.dmaSource += count;
gb->cgb.dmaDest += count;
gb->cgb.dmaSize = 0;
#if WALNUT_GB_SAFE_DUALFETCH_DMA
gb->prefetch_invalid=true;
#endif
}
#elif WALNUT_GB_16BIT_DMA
if (gb->cgb.cgbMode && !gb->cgb.dmaMode)
{
uint16_t src = gb->cgb.dmaSource & 0xFFF0;
uint16_t dst = (gb->cgb.dmaDest & 0x1FF0) | 0x8000;
uint16_t count = gb->cgb.dmaSize << 4;
for (uint16_t i = 0; i < count; i += 2)
{
uint16_t val16 = __gb_read16(gb, src + i);
__gb_write16(gb, dst + i, val16);
}
gb->cgb.dmaSource += count;
gb->cgb.dmaDest += count;
gb->cgb.dmaSize = 0;
#if WALNUT_GB_SAFE_DUALFETCH_DMA
gb->prefetch_invalid=true;
#endif
}
#else
if(gb->cgb.cgbMode && (!gb->cgb.dmaMode))
{
for (int i = 0; i < (gb->cgb.dmaSize << 4); i++)
{
__gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, __gb_read(gb, (gb->cgb.dmaSource & 0xFFF0) + i));
}
gb->cgb.dmaSource += (gb->cgb.dmaSize << 4);
gb->cgb.dmaDest += (gb->cgb.dmaSize << 4);
gb->cgb.dmaSize = 0;
#if WALNUT_GB_SAFE_DUALFETCH_DMA
gb->prefetch_invalid=true;
#endif
}
#endif
}
gb->cgb.dmaActive = gb->cgb.dmaMode ^ 1;
return;
case 0x56:
gb->hram_io[0x56] = val;
return;
case 0x68:
gb->cgb.BGPaletteID = val & 0x3F;
gb->cgb.BGPaletteInc = val >> 7;
return;
case 0x69:
gb->cgb.BGPalette[gb->cgb.BGPaletteID & 0x3F] = val;
#if WALNUT_GB_RGB565_BIGENDIAN
gb->cgb.fixPalette[(gb->cgb.BGPaletteID & 0x3E) >> 1] = bgr555_to_rgb565BE_accurate((gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3E) + 1] << 8) | (gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3E)]));
#else
gb->cgb.fixPalette[(gb->cgb.BGPaletteID & 0x3E) >> 1] = bgr555_to_rgb565_accurate((gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3E) + 1] << 8) | (gb->cgb.BGPalette[(gb->cgb.BGPaletteID & 0x3E)]));
#endif
if(gb->cgb.BGPaletteInc) {
gb->cgb.BGPaletteID++;
gb->cgb.BGPaletteID = (gb->cgb.BGPaletteID) & 0x3F;
}
return;
case 0x6A:
gb->cgb.OAMPaletteID = val & 0x3F;
gb->cgb.OAMPaletteInc = val >> 7;
return;
case 0x6B:
gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3F)] = val;
#if WALNUT_GB_RGB565_BIGENDIAN
gb->cgb.fixPalette[0x20 + ((gb->cgb.OAMPaletteID & 0x3E) >> 1)] = bgr555_to_rgb565BE_accurate((gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3E) + 1] << 8) + (gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3E)]));
#else
gb->cgb.fixPalette[0x20 + ((gb->cgb.OAMPaletteID & 0x3E) >> 1)] = bgr555_to_rgb565_accurate((gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3E) + 1] << 8) + (gb->cgb.OAMPalette[(gb->cgb.OAMPaletteID & 0x3E)]));
#endif
if(gb->cgb.OAMPaletteInc) {
gb->cgb.OAMPaletteID++;
gb->cgb.OAMPaletteID = (gb->cgb.OAMPaletteID) & 0x3F;
}
return;
case 0x70:
gb->cgb.wramBank = val;
gb->cgb.wramBankOffset = WRAM_1_ADDR - (1 << 12);
if(gb->cgb.cgbMode && (gb->cgb.wramBank & 7) > 0) gb->cgb.wramBankOffset = WRAM_1_ADDR - ((gb->cgb.wramBank & 7) << 12);
return;
#endif
case 0xFF:
gb->hram_io[IO_IE] = val;
return;
}
}
return;
}
#if WALNUT_GB_32BIT_ALIGNED
void __gb_write32(struct gb_s *gb, uint16_t addr, uint32_t val) {
uint8_t *dst = NULL;
switch (WALNUT_GB_GET_MSN16(addr)) {
case 0x8:
case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
dst = &gb->vram[addr - gb->cgb.vramBankOffset];
#else
dst = &gb->vram[addr - VRAM_ADDR];
#endif
break;
case 0xC:
case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
dst = &gb->wram[addr - gb->cgb.wramBankOffset];
#else
dst = &gb->wram[addr - WRAM_1_ADDR + WRAM_BANK_SIZE];
#endif
break;
case 0xE:
dst = &gb->wram[addr - ECHO_ADDR];
break;
case 0xF:
if (addr < OAM_ADDR) {
#if WALNUT_FULL_GBC_SUPPORT
dst = &gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset];
#else
dst = &gb->wram[addr - ECHO_ADDR];
#endif
} else if (addr < UNUSED_ADDR) {
dst = &gb->oam[addr - OAM_ADDR];
}
break;
default:
break;
}
if (dst) {
if (((uintptr_t)dst & 3) == 0) {
*(uint32_t *)dst = val;
} else {
dst[0] = (uint8_t)(val);
dst[1] = (uint8_t)(val >> 8);
dst[2] = (uint8_t)(val >> 16);
dst[3] = (uint8_t)(val >> 24);
}
return;
}
__gb_write(gb, addr + 0, (uint8_t)(val));
__gb_write(gb, addr + 1, (uint8_t)(val >> 8));
__gb_write(gb, addr + 2, (uint8_t)(val >> 16));
__gb_write(gb, addr + 3, (uint8_t)(val >> 24));
}
#else
void __gb_write32(struct gb_s *gb, uint16_t addr, uint32_t val) {
switch (WALNUT_GB_GET_MSN16(addr)) {
case 0x8:
case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
*(uint32_t*)&gb->vram[addr - gb->cgb.vramBankOffset] = val;
#else
*(uint32_t*)&gb->vram[addr - VRAM_ADDR] = val;
#endif
return;
case 0xC:
case 0xD:
#if WALNUT_FULL_GBC_SUPPORT
*(uint32_t*)&gb->wram[addr - gb->cgb.wramBankOffset] = val;
#else
*(uint32_t*)&gb->wram[addr - WRAM_1_ADDR + WRAM_BANK_SIZE] = val;
#endif
return;
case 0xE:
*(uint32_t*)&gb->wram[addr - ECHO_ADDR] = val;
return;
case 0xF:
if(addr < OAM_ADDR) {
#if WALNUT_FULL_GBC_SUPPORT
*(uint32_t*)&gb->wram[(addr - 0x2000) - gb->cgb.wramBankOffset] = val;
#else
*(uint32_t*)&gb->wram[addr - ECHO_ADDR] = val;
#endif
return;
}
if(addr < UNUSED_ADDR) {
*(uint32_t*)&gb->oam[addr - OAM_ADDR] = val;
return;
}
break;
default:
break;
}
__gb_write(gb, addr, val & 0xFF);
__gb_write(gb, addr + 1, (val >> 8) & 0xFF);
__gb_write(gb, addr + 2, (val >> 16) & 0xFF);
__gb_write(gb, addr + 3, (val >> 24) & 0xFF);
}
#endif
void __gb_write16(struct gb_s *gb, uint_fast16_t addr, uint16_t val)
{
switch(WALNUT_GB_GET_MSN16(addr))
{
case 0x8:
case 0x9:
#if WALNUT_FULL_GBC_SUPPORT
*((uint16_t *)(gb->vram + (addr - gb->cgb.vramBankOffset))) = val;
#else
*((uint16_t *)(gb->vram + (addr - VRAM_ADDR))) = val;
#endif
return;
case 0xA:
case 0xB:
if(gb->mbc == 3 && gb->cart_ram_bank >= 0x08)
{
__gb_write(gb, addr, val & 0xFF);
__gb_write(gb, addr + 1, val >> 8);
}
else if(gb->cart_ram && gb->enable_cart_ram)
{
gb->gb_cart_ram_write(gb, addr - CART_RAM_ADDR + (gb->cart_ram_bank * CRAM_BANK_SIZE), val & 0xFF);
gb->gb_cart_ram_write(gb, addr - CART_RAM_ADDR + 1 + (gb->cart_ram_bank * CRAM_BANK_SIZE), val >> 8);
}
return;
case 0xC:
#if WALNUT_FULL_GBC_SUPPORT
case 0xD:
*((uint16_t *)(gb->wram + (addr - gb->cgb.wramBankOffset))) = val;
#else
case 0xD:
*((uint16_t *)(gb->wram + (addr - WRAM_1_ADDR + WRAM_BANK_SIZE))) = val;
#endif
return;
case 0xE:
*((uint16_t *)(gb->wram + (addr - ECHO_ADDR))) = val;
return;
case 0xF:
if(addr < OAM_ADDR)
{
#if WALNUT_FULL_GBC_SUPPORT
*((uint16_t *)(gb->wram + (addr - 0x2000 - gb->cgb.wramBankOffset))) = val;
#else
*((uint16_t *)(gb->wram + (addr - ECHO_ADDR))) = val;
#endif
}
else if(addr < UNUSED_ADDR || (addr >= HRAM_ADDR && addr < INTR_EN_ADDR))
{
__gb_write(gb, addr, val & 0xFF);
__gb_write(gb, addr + 1, val >> 8);
}
else
{
__gb_write(gb, addr, val & 0xFF);
__gb_write(gb, addr + 1, val >> 8);
}
return;
default:
__gb_write(gb, addr, val & 0xFF);
__gb_write(gb, addr + 1, val >> 8);
return;
}
}
uint8_t __gb_execute_cb(struct gb_s *gb)
{
uint8_t inst_cycles;
uint8_t cbop = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t r = (cbop & 0x7);
uint8_t b = (cbop >> 3) & 0x7;
uint8_t d = (cbop >> 3) & 0x1;
uint8_t val;
uint8_t writeback = 1;
inst_cycles = 8;
switch(cbop & 0xC7)
{
case 0x06:
case 0x86:
case 0xC6:
inst_cycles += 8;
break;
case 0x46:
inst_cycles += 4;
break;
}
switch(r)
{
case 0:
val = gb->cpu_reg.bc.bytes.b;
break;
case 1:
val = gb->cpu_reg.bc.bytes.c;
break;
case 2:
val = gb->cpu_reg.de.bytes.d;
break;
case 3:
val = gb->cpu_reg.de.bytes.e;
break;
case 4:
val = gb->cpu_reg.hl.bytes.h;
break;
case 5:
val = gb->cpu_reg.hl.bytes.l;
break;
case 6:
val = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
default:
val = gb->cpu_reg.a;
break;
}
switch(cbop >> 6)
{
case 0x0:
cbop = (cbop >> 4) & 0x3;
switch(cbop)
{
case 0x0:
case 0x1:
if(d)
{
uint8_t temp = val;
val = (val >> 1);
val |= cbop ? (gb->cpu_reg.f.f_bits.c << 7) : (temp << 7);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = (val == 0x00);
gb->cpu_reg.f.f_bits.c = (temp & 0x01);
}
else
{
uint8_t temp = val;
val = (val << 1);
val |= cbop ? gb->cpu_reg.f.f_bits.c : (temp >> 7);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = (val == 0x00);
gb->cpu_reg.f.f_bits.c = (temp >> 7);
}
break;
case 0x2:
if(d)
{
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = val & 0x01;
val = (val >> 1) | (val & 0x80);
gb->cpu_reg.f.f_bits.z = (val == 0x00);
}
else
{
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = (val >> 7);
val = val << 1;
gb->cpu_reg.f.f_bits.z = (val == 0x00);
}
break;
case 0x3:
if(d)
{
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = val & 0x01;
val = val >> 1;
gb->cpu_reg.f.f_bits.z = (val == 0x00);
}
else
{
uint8_t temp = (val >> 4) & 0x0F;
temp |= (val << 4) & 0xF0;
val = temp;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = (val == 0x00);
}
break;
}
break;
case 0x1:
gb->cpu_reg.f.f_bits.z = !((val >> b) & 0x1);
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = 1;
writeback = 0;
break;
case 0x2:
val &= (0xFE << b) | (0xFF >> (8 - b));
break;
case 0x3:
val |= (0x1 << b);
break;
}
if(writeback)
{
switch(r)
{
case 0:
gb->cpu_reg.bc.bytes.b = val;
break;
case 1:
gb->cpu_reg.bc.bytes.c = val;
break;
case 2:
gb->cpu_reg.de.bytes.d = val;
break;
case 3:
gb->cpu_reg.de.bytes.e = val;
break;
case 4:
gb->cpu_reg.hl.bytes.h = val;
break;
case 5:
gb->cpu_reg.hl.bytes.l = val;
break;
case 6:
__gb_write(gb, gb->cpu_reg.hl.reg, val);
break;
case 7:
gb->cpu_reg.a = val;
break;
}
}
return inst_cycles;
}
#if ENABLE_LCD
struct sprite_data {
uint8_t sprite_number;
uint8_t x;
};
#if WALNUT_GB_HIGH_LCD_ACCURACY
static int compare_sprites(const struct sprite_data *const sd1, const struct sprite_data *const sd2)
{
int x_res;
x_res = (int)sd1->x - (int)sd2->x;
if(x_res != 0)
return x_res;
return (int)sd1->sprite_number - (int)sd2->sprite_number;
}
#endif
void __gb_draw_line(struct gb_s *gb)
{
uint8_t pixels[160] = {0};
const uint8_t hram_io_ly = gb->hram_io[IO_LY];
#if WALNUT_FULL_GBC_SUPPORT
const uint8_t cgbMode = gb->cgb.cgbMode;
#endif
if(gb->display.lcd_draw_line == NULL)
return;
if(gb->direct.frame_skip && !gb->display.frame_skip_count)
return;
#if WALNUT_FULL_GBC_SUPPORT
uint8_t pixelsPrio[160] = {0};
#endif
if(gb->direct.interlace)
{
if((!gb->display.interlace_count
&& (hram_io_ly & 1) == 0)
|| (gb->display.interlace_count
&& (hram_io_ly & 1) == 1))
{
if(gb->hram_io[IO_LCDC] & LCDC_WINDOW_ENABLE
&& hram_io_ly >= gb->display.WY
&& gb->hram_io[IO_WX] <= 166)
gb->display.window_clear++;
return;
}
}
#if WALNUT_FULL_GBC_SUPPORT
if(cgbMode || gb->hram_io[IO_LCDC] & LCDC_BG_ENABLE)
#else
if(gb->hram_io[IO_LCDC] & LCDC_BG_ENABLE)
#endif
{
uint8_t bg_y, disp_x, bg_x, idx, py, px, t1, t2;
uint16_t bg_map, tile;
bg_y = hram_io_ly + gb->hram_io[IO_SCY];
bg_map =
((gb->hram_io[IO_LCDC] & LCDC_BG_MAP) ?
VRAM_BMAP_2 : VRAM_BMAP_1)
+ (bg_y >> 3) * 0x20;
disp_x = LCD_WIDTH - 1;
bg_x = disp_x + gb->hram_io[IO_SCX];
idx = gb->vram[bg_map + (bg_x >> 3)];
#if WALNUT_FULL_GBC_SUPPORT
uint8_t idxAtt = gb->vram[bg_map + (bg_x >> 3) + 0x2000];
#endif
py = (bg_y & 0x07);
px = 7 - (bg_x & 0x07);
if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT)
tile = VRAM_TILES_1 + idx * 0x10;
else
tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
#if WALNUT_FULL_GBC_SUPPORT
if(cgbMode)
{
if(idxAtt & 0x08) tile += 0x2000;
if(idxAtt & 0x40) tile += 2 * (7 - py);
}
if(!(idxAtt & 0x40))
{
tile += 2 * py;
}
if(cgbMode && (idxAtt & 0x20))
{
t1 = gb->vram[tile] << px;
t2 = gb->vram[tile + 1] << px;
}
else
{
t1 = gb->vram[tile] >> px;
t2 = gb->vram[tile + 1] >> px;
}
#else
tile += 2 * py;
t1 = gb->vram[tile] >> px;
t2 = gb->vram[tile + 1] >> px;
#endif
for(; disp_x != 0xFF; disp_x--)
{
uint8_t c;
if(px == 8)
{
px = 0;
bg_x = disp_x + gb->hram_io[IO_SCX];
idx = gb->vram[bg_map + (bg_x >> 3)];
#if WALNUT_FULL_GBC_SUPPORT
idxAtt = gb->vram[bg_map + (bg_x >> 3) + 0x2000];
#endif
if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT)
tile = VRAM_TILES_1 + idx * 0x10;
else
tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
#if WALNUT_FULL_GBC_SUPPORT
if(cgbMode)
{
if(idxAtt & 0x08) tile += 0x2000;
if(idxAtt & 0x40) tile += 2 * (7 - py);
}
if(!(idxAtt & 0x40))
{
tile += 2 * py;
}
#else
tile += 2 * py;
#endif
t1 = gb->vram[tile];
t2 = gb->vram[tile + 1];
}
#if WALNUT_FULL_GBC_SUPPORT
if(cgbMode && (idxAtt & 0x20))
{
c = (((t1 & 0x80) >> 1) | (t2 & 0x80)) >> 6;
pixels[disp_x] = ((idxAtt & 0x07) << 2) + c;
pixelsPrio[disp_x] = (idxAtt >> 7);
t1 = t1 << 1;
t2 = t2 << 1;
}
else
{
c = (t1 & 0x1) | ((t2 & 0x1) << 1);
if(cgbMode)
{
pixels[disp_x] = ((idxAtt & 0x07) << 2) + c;
pixelsPrio[disp_x] = (idxAtt >> 7);
}
else
{
pixels[disp_x] = gb->display.bg_palette[c];
#if WALNUT_GB_12_COLOUR
pixels[disp_x] |= LCD_PALETTE_BG;
#endif
}
t1 = t1 >> 1;
t2 = t2 >> 1;
}
#else
c = (t1 & 0x1) | ((t2 & 0x1) << 1);
pixels[disp_x] = gb->display.bg_palette[c];
#if WALNUT_GB_12_COLOUR
pixels[disp_x] |= LCD_PALETTE_BG;
#endif
t1 = t1 >> 1;
t2 = t2 >> 1;
#endif
px++;
}
}
if(gb->hram_io[IO_LCDC] & LCDC_WINDOW_ENABLE
&& hram_io_ly >= gb->display.WY
&& gb->hram_io[IO_WX] <= 166)
{
uint16_t win_line, tile;
uint8_t disp_x, win_x, py, px, idx, t1, t2, end;
win_line = (gb->hram_io[IO_LCDC] & LCDC_WINDOW_MAP) ?
VRAM_BMAP_2 : VRAM_BMAP_1;
win_line += (gb->display.window_clear >> 3) * 0x20;
disp_x = LCD_WIDTH - 1;
win_x = disp_x - gb->hram_io[IO_WX] + 7;
py = gb->display.window_clear & 0x07;
px = 7 - (win_x & 0x07);
idx = gb->vram[win_line + (win_x >> 3)];
#if WALNUT_FULL_GBC_SUPPORT
uint8_t idxAtt = gb->vram[win_line + (win_x >> 3) + 0x2000];
#endif
if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT)
tile = VRAM_TILES_1 + idx * 0x10;
else
tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
#if WALNUT_FULL_GBC_SUPPORT
if(cgbMode)
{
if(idxAtt & 0x08) tile += 0x2000;
if(idxAtt & 0x40) tile += 2 * (7 - py);
}
if(!(idxAtt & 0x40))
{
tile += 2 * py;
}
if(cgbMode && (idxAtt & 0x20))
{
t1 = gb->vram[tile] << px;
t2 = gb->vram[tile + 1] << px;
}
else
{
t1 = gb->vram[tile] >> px;
t2 = gb->vram[tile + 1] >> px;
}
#else
tile += 2 * py;
t1 = gb->vram[tile] >> px;
t2 = gb->vram[tile + 1] >> px;
#endif
end = (gb->hram_io[IO_WX] < 7 ? 0 : gb->hram_io[IO_WX] - 7) - 1;
for(; disp_x != end; disp_x--)
{
uint8_t c;
if(px == 8)
{
px = 0;
win_x = disp_x - gb->hram_io[IO_WX] + 7;
idx = gb->vram[win_line + (win_x >> 3)];
#if WALNUT_FULL_GBC_SUPPORT
idxAtt = gb->vram[win_line + (win_x >> 3) + 0x2000];
#endif
if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT)
tile = VRAM_TILES_1 + idx * 0x10;
else
tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
#if WALNUT_FULL_GBC_SUPPORT
if(cgbMode)
{
if(idxAtt & 0x08) tile += 0x2000;
if(idxAtt & 0x40) tile += 2 * (7 - py);
}
if(!(idxAtt & 0x40))
{
tile += 2 * py;
}
#else
tile += 2 * py;
#endif
t1 = gb->vram[tile];
t2 = gb->vram[tile + 1];
}
#if WALNUT_FULL_GBC_SUPPORT
if(idxAtt & 0x20)
{
c = (((t1 & 0x80) >> 1) | (t2 & 0x80)) >> 6;
pixels[disp_x] = ((idxAtt & 0x07) << 2) + c;
pixelsPrio[disp_x] = (idxAtt >> 7);
t1 = t1 << 1;
t2 = t2 << 1;
}
else
{
c = (t1 & 0x1) | ((t2 & 0x1) << 1);
if(cgbMode)
{
pixels[disp_x] = ((idxAtt & 0x07) << 2) + c;
pixelsPrio[disp_x] = (idxAtt >> 7);
}
else
{
pixels[disp_x] = gb->display.bg_palette[c];
#if WALNUT_GB_12_COLOUR
pixels[disp_x] |= LCD_PALETTE_BG;
#endif
}
t1 = t1 >> 1;
t2 = t2 >> 1;
}
#else
c = (t1 & 0x1) | ((t2 & 0x1) << 1);
pixels[disp_x] = gb->display.bg_palette[c];
#if WALNUT_GB_12_COLOUR
pixels[disp_x] |= LCD_PALETTE_BG;
#endif
t1 = t1 >> 1;
t2 = t2 >> 1;
#endif
px++;
}
gb->display.window_clear++;
}
if(gb->hram_io[IO_LCDC] & LCDC_OBJ_ENABLE)
{
uint8_t sprite_number;
#if WALNUT_GB_HIGH_LCD_ACCURACY
uint8_t number_of_sprites = 0;
struct sprite_data sprites_to_render[MAX_SPRITES_LINE + 1];
for(sprite_number = 0;
sprite_number < NUM_SPRITES;
sprite_number++)
{
uint8_t OY = gb->oam[4 * sprite_number + 0];
uint8_t OX = gb->oam[4 * sprite_number + 1];
if(hram_io_ly +
(gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 0 : 8) >= OY
|| hram_io_ly + 16 < OY)
continue;
#if WALNUT_FULL_GBC_SUPPORT
if (!cgbMode)
{
#endif
struct sprite_data current;
current.sprite_number = sprite_number;
current.x = OX;
uint8_t place;
for (place = number_of_sprites; place != 0; place--)
{
if(compare_sprites(&sprites_to_render[place - 1], &current) < 0)
break;
}
if(place >= MAX_SPRITES_LINE)
continue;
for (uint8_t i = number_of_sprites; i > place; --i) {
sprites_to_render[i] = sprites_to_render[i - 1];
}
if(number_of_sprites < MAX_SPRITES_LINE)
number_of_sprites++;
sprites_to_render[place] = current;
#if WALNUT_FULL_GBC_SUPPORT
}
else
{
if (number_of_sprites >= MAX_SPRITES_LINE) continue;
sprites_to_render[number_of_sprites].sprite_number = sprite_number;
sprites_to_render[number_of_sprites].x = OX;
number_of_sprites++;
}
#endif
}
#endif
#if WALNUT_GB_HIGH_LCD_ACCURACY
for(sprite_number = number_of_sprites - 1;
sprite_number != 0xFF;
sprite_number--)
{
uint8_t s = sprites_to_render[sprite_number].sprite_number;
#else
for (sprite_number = NUM_SPRITES - 1;
sprite_number != 0xFF;
sprite_number--)
{
uint8_t s = sprite_number;
#endif
uint8_t py, t1, t2, dir, start, end, shift, disp_x;
uint8_t OY = gb->oam[4 * s + 0];
uint8_t OX = gb->oam[4 * s + 1];
uint8_t OT = gb->oam[4 * s + 2]
& (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 0xFE : 0xFF);
uint8_t OF = gb->oam[4 * s + 3];
#if !WALNUT_GB_HIGH_LCD_ACCURACY
if(hram_io_ly +
(gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 0 : 8) >= OY ||
hram_io_ly + 16 < OY)
continue;
#endif
if(OX == 0 || OX >= 168)
continue;
py = hram_io_ly - OY + 16;
if(OF & OBJ_FLIP_Y)
py = (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 15 : 7) - py;
#if WALNUT_FULL_GBC_SUPPORT
if(cgbMode)
{
t1 = gb->vram[((OF & OBJ_BANK) << 10) + VRAM_TILES_1 + OT * 0x10 + 2 * py];
t2 = gb->vram[((OF & OBJ_BANK) << 10) + VRAM_TILES_1 + OT * 0x10 + 2 * py + 1];
}
else
#endif
{
t1 = gb->vram[VRAM_TILES_1 + OT * 0x10 + 2 * py];
t2 = gb->vram[VRAM_TILES_1 + OT * 0x10 + 2 * py + 1];
}
if(OF & OBJ_FLIP_X)
{
dir = 1;
start = (OX < 8 ? 0 : OX - 8);
end = MIN(OX, LCD_WIDTH);
shift = 8 - OX + start;
}
else
{
dir = (uint8_t)-1;
start = MIN(OX, LCD_WIDTH) - 1;
end = (OX < 8 ? 0 : OX - 8) - 1;
shift = OX - (start + 1);
}
t1 >>= shift;
t2 >>= shift;
for(disp_x = start; disp_x != end; disp_x += dir)
{
uint8_t c = (t1 & 0x1) | ((t2 & 0x1) << 1);
#if WALNUT_FULL_GBC_SUPPORT
if(cgbMode)
{
uint8_t isBackgroundDisabled = c && !(gb->hram_io[IO_LCDC] & LCDC_BG_ENABLE);
uint8_t isPixelPriorityNonConflicting = c &&
!(pixelsPrio[disp_x] && (pixels[disp_x] & 0x3)) &&
!((OF & OBJ_PRIORITY) && (pixels[disp_x] & 0x3));
if(isBackgroundDisabled || isPixelPriorityNonConflicting)
{
pixels[disp_x] = ((OF & OBJ_CGB_PALETTE) << 2) + c + 0x20;
}
}
else
#endif
if(c && !(OF & OBJ_PRIORITY && !((pixels[disp_x] & 0x3) == gb->display.bg_palette[0])))
{
pixels[disp_x] = (OF & OBJ_PALETTE)
? gb->display.sp_palette[c + 4]
: gb->display.sp_palette[c];
#if WALNUT_GB_12_COLOUR
pixels[disp_x] |= (OF & OBJ_PALETTE);
#endif
#if WALNUT_FULL_GBC_SUPPORT
pixels[disp_x] &= ~LCD_PALETTE_BG;
#endif
}
t1 = t1 >> 1;
t2 = t2 >> 1;
}
}
}
gb->display.lcd_draw_line(gb, pixels, gb->hram_io[IO_LY]);
}
#endif
#endif
#endif
