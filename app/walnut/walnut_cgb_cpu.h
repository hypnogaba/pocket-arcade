#ifndef WALNUT_GB_CPU_H
#define WALNUT_GB_CPU_H
#ifndef WALNUT_GB_HEADER_ONLY
static inline void __gb_step_cpu(struct gb_s *gb)
{
uint16_t oppair;
uint8_t opcode;
uint_fast16_t inst_cycles;
static const uint8_t op_cycles[0x100] =
{
4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,
4,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,
8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,
8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
8,12,12,16,12,16, 8,16, 8,16,12, 8,12,24, 8,16,
8,12,12, 0,12,16, 8,16, 8,16,12, 0,12, 0, 8,16,
12,12,8, 0, 0,16, 8,16,16, 4,16, 0, 0, 0, 8,16,
12,12,8, 4, 0,16, 8,16,12, 8,16, 4, 0, 0, 8,16
};
static const uint_fast16_t TAC_CYCLES[4] = {1024, 16, 64, 256};
if(gb->gb_halt && gb->hram_io[IO_IE] && (gb->hram_io[IO_IF] & gb->hram_io[IO_IE]) == 0)
{
gb->gb_halt = false;
gb->cpu_reg.pc.reg--;
}
while(gb->gb_halt || (gb->gb_ime &&
gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & ANY_INTR))
{
gb->gb_halt = false;
if(!gb->gb_ime)
break;
gb->gb_ime = false;
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & VBLANK_INTR)
{
gb->cpu_reg.pc.reg = VBLANK_INTR_ADDR;
gb->hram_io[IO_IF] ^= VBLANK_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & LCDC_INTR)
{
gb->cpu_reg.pc.reg = LCDC_INTR_ADDR;
gb->hram_io[IO_IF] ^= LCDC_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & TIMER_INTR)
{
gb->cpu_reg.pc.reg = TIMER_INTR_ADDR;
gb->hram_io[IO_IF] ^= TIMER_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & SERIAL_INTR)
{
gb->cpu_reg.pc.reg = SERIAL_INTR_ADDR;
gb->hram_io[IO_IF] ^= SERIAL_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & CONTROL_INTR)
{
gb->cpu_reg.pc.reg = CONTROL_INTR_ADDR;
gb->hram_io[IO_IF] ^= CONTROL_INTR;
}
break;
}
oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
opcode = (uint8_t)oppair;
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
gb->prefetch_invalid=false;
#endif
inst_cycles = op_cycles[opcode];
switch(opcode)
{
case 0x00:
opcode = (uint8_t)(oppair >> 8);
break;
case 0x01:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
gb->cpu_reg.bc.bytes.c = (uint8_t)(oppair >> 8);
oppair = __gb_read16(gb, gb->cpu_reg.pc.reg + 1);
gb->cpu_reg.bc.bytes.b = (uint8_t)(oppair);
gb->cpu_reg.pc.reg += 2;
opcode = (uint8_t)(oppair >> 8);
#else
gb->cpu_reg.bc.bytes.c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.pc.reg++);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
break;
case 0x02:
__gb_write(gb, gb->cpu_reg.bc.reg, gb->cpu_reg.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x03:
gb->cpu_reg.bc.reg++;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x04:
WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.b);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x05:
WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.b);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x06:
gb->cpu_reg.bc.bytes.b = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x07:
gb->cpu_reg.a = (gb->cpu_reg.a << 1) | (gb->cpu_reg.a >> 7);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a & 0x01);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x08:
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
uint8_t l = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
uint8_t h = oppair;
uint16_t temp = WALNUT_GB_U8_TO_U16(h, l);
__gb_write16(gb,temp,gb->cpu_reg.sp.reg);
opcode = oppair >> 8;
#else
uint8_t l = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t temp = WALNUT_GB_U8_TO_U16(h, l);
__gb_write(gb, temp++, gb->cpu_reg.sp.bytes.p);
__gb_write(gb, temp, gb->cpu_reg.sp.bytes.s);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
break;
}
case 0x09:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.bc.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.bc.reg) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x0A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.bc.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x0B:
gb->cpu_reg.bc.reg--;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x0C:
WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x0D:
WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x0E:
gb->cpu_reg.bc.bytes.c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x0F:
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 0x01;
gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (gb->cpu_reg.a << 7);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x10:
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode & gb->cgb.doubleSpeedPrep)
{
gb->cgb.doubleSpeedPrep = 0;
gb->cgb.doubleSpeed ^= 1;
}
#endif
opcode = (uint8_t)(oppair >> 8);
break;
case 0x11:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
gb->cpu_reg.de.bytes.e = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.de.bytes.d = oppair;
opcode = oppair >> 8;
#else
gb->cpu_reg.de.bytes.e = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.pc.reg++);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
break;
case 0x12:
__gb_write(gb, gb->cpu_reg.de.reg, gb->cpu_reg.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x13:
gb->cpu_reg.de.reg++;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x14:
WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.d);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x15:
WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.d);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x16:
gb->cpu_reg.de.bytes.d = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x17:
{
uint8_t temp = gb->cpu_reg.a;
gb->cpu_reg.a = (gb->cpu_reg.a << 1) | gb->cpu_reg.f.f_bits.c;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = (temp >> 7) & 0x01;
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x18:
{
int8_t temp = (int8_t) (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.pc.reg += temp;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0x19:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.de.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.de.reg) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x1A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.de.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x1B:
gb->cpu_reg.de.reg--;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x1C:
WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.e);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x1D:
WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.e);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x1E:
gb->cpu_reg.de.bytes.e =  (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x1F:
{
uint8_t temp = gb->cpu_reg.a;
gb->cpu_reg.a = gb->cpu_reg.a >> 1 | (gb->cpu_reg.f.f_bits.c << 7);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = temp & 0x1;
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x20:
if(!gb->cpu_reg.f.f_bits.z)
{
int8_t temp = (int8_t) (oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x21:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
gb->cpu_reg.hl.bytes.l =(uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.hl.bytes.h = oppair;
opcode = oppair >> 8;
#else
gb->cpu_reg.hl.bytes.l =(uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.pc.reg++);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
break;
case 0x22:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
gb->cpu_reg.hl.reg++;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x23:
gb->cpu_reg.hl.reg++;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x24:
WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.h);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x25:
WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.h);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x26:
gb->cpu_reg.hl.bytes.h =(uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x27:
{
int16_t a = gb->cpu_reg.a;
if(gb->cpu_reg.f.f_bits.n)
{
if(gb->cpu_reg.f.f_bits.h)
a = (a - 0x06) & 0xFF;
if(gb->cpu_reg.f.f_bits.c)
a -= 0x60;
}
else
{
if(gb->cpu_reg.f.f_bits.h || (a & 0x0F) > 9)
a += 0x06;
if(gb->cpu_reg.f.f_bits.c || a > 0x9F)
a += 0x60;
}
if((a & 0x100) == 0x100)
gb->cpu_reg.f.f_bits.c = 1;
gb->cpu_reg.a = (uint8_t)a;
gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0);
gb->cpu_reg.f.f_bits.h = 0;
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x28:
if(gb->cpu_reg.f.f_bits.z)
{
int8_t temp = (int8_t) (oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x29:
{
gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.hl.reg & 0x8000) > 0;
gb->cpu_reg.hl.reg <<= 1;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = (gb->cpu_reg.hl.reg & 0x1000) > 0;
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x2A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg++);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x2B:
gb->cpu_reg.hl.reg--;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x2C:
WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.l);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x2D:
WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.l);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x2E:
gb->cpu_reg.hl.bytes.l = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x2F:
gb->cpu_reg.a = ~gb->cpu_reg.a;
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h = 1;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x30:
if(!gb->cpu_reg.f.f_bits.c)
{
int8_t temp = (int8_t) (oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x31:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
gb->cpu_reg.sp.bytes.p = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg ++;
oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.sp.bytes.s = oppair;
opcode = oppair >> 8;
#else
gb->cpu_reg.sp.bytes.p = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.sp.bytes.s = __gb_read(gb, gb->cpu_reg.pc.reg++);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
break;
case 0x32:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
gb->cpu_reg.hl.reg--;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x33:
gb->cpu_reg.sp.reg++;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x34:
{
uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
WGB_INSTR_INC_R8(temp);
__gb_write(gb, gb->cpu_reg.hl.reg, temp);
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x35:
{
uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
WGB_INSTR_DEC_R8(temp);
__gb_write(gb, gb->cpu_reg.hl.reg, temp);
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x36:
__gb_write(gb, gb->cpu_reg.hl.reg,(uint8_t)(oppair >> 8));
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x37:
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = 0;
gb->cpu_reg.f.f_bits.c = 1;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x38:
if(gb->cpu_reg.f.f_bits.c)
{
int8_t temp = (int8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x39:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.sp.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
((gb->cpu_reg.hl.reg & 0xFFF) + (gb->cpu_reg.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = temp & 0x10000 ? 1 : 0;
gb->cpu_reg.hl.reg = (uint16_t)temp;
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0x3A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg--);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x3B:
gb->cpu_reg.sp.reg--;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x3C:
WGB_INSTR_INC_R8(gb->cpu_reg.a);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x3D:
WGB_INSTR_DEC_R8(gb->cpu_reg.a);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x3E:
gb->cpu_reg.a = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0x3F:
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = 0;
gb->cpu_reg.f.f_bits.c = ~gb->cpu_reg.f.f_bits.c;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x40:
opcode = (uint8_t)(oppair >> 8);
break;
case 0x41:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.bc.bytes.c;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x42:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.d;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x43:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.e;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x44:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.h;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x45:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.l;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x46:
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.hl.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x47:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.a;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x48:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.bc.bytes.b;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x49:
opcode = (uint8_t)(oppair >> 8);
break;
case 0x4A:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.d;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x4B:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.e;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x4C:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.h;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x4D:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.l;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x4E:
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.hl.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x4F:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.a;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x50:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.b;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x51:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.c;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x52:
opcode = (uint8_t)(oppair >> 8);
break;
case 0x53:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.de.bytes.e;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x54:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.h;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x55:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.l;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x56:
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.hl.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x57:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.a;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x58:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.b;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x59:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.c;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x5A:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.de.bytes.d;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x5B:
opcode = (uint8_t)(oppair >> 8);
break;
case 0x5C:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.h;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x5D:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.l;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x5E:
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.hl.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x5F:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.a;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x60:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.b;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x61:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.c;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x62:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.d;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x63:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.e;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x64:
opcode = (uint8_t)(oppair >> 8);
break;
case 0x65:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.hl.bytes.l;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x66:
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.hl.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x67:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.a;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x68:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.b;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x69:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.c;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x6A:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.d;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x6B:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.e;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x6C:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.hl.bytes.h;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x6D:
opcode = (uint8_t)(oppair >> 8);
break;
case 0x6E:
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.hl.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x6F:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.a;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x70:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.b);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x71:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.c);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x72:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.d);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x73:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.e);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x74:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.h);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x75:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.l);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x76:
{
int_fast16_t halt_cycles = INT_FAST16_MAX;
gb->gb_halt = true;
if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
{
int serial_cycles = SERIAL_CYCLES -
gb->counter.serial_count;
if(serial_cycles < halt_cycles)
halt_cycles = serial_cycles;
}
if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
{
int tac_cycles = TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK] -
gb->counter.tima_count;
if(tac_cycles < halt_cycles)
halt_cycles = tac_cycles;
}
if((gb->hram_io[IO_LCDC] & LCDC_ENABLE))
{
int lcd_cycles;
if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_HBLANK)
{
lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN)
{
lcd_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW)
{
lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else
{
lcd_cycles = LCD_LINE_CYCLES - gb->counter.lcd_count;
}
if(lcd_cycles < halt_cycles)
halt_cycles = lcd_cycles;
}
if(halt_cycles <= 0)
halt_cycles = 4;
inst_cycles = (uint_fast16_t)halt_cycles;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
}
case 0x77:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0x78:
gb->cpu_reg.a = gb->cpu_reg.bc.bytes.b;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x79:
gb->cpu_reg.a = gb->cpu_reg.bc.bytes.c;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x7A:
gb->cpu_reg.a = gb->cpu_reg.de.bytes.d;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x7B:
gb->cpu_reg.a = gb->cpu_reg.de.bytes.e;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x7C:
gb->cpu_reg.a = gb->cpu_reg.hl.bytes.h;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x7D:
gb->cpu_reg.a = gb->cpu_reg.hl.bytes.l;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x7E:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x7F:
opcode = (uint8_t)(oppair >> 8);
break;
case 0x80:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x81:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x82:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x83:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x84:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x85:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x86:
WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x87:
WGB_INSTR_ADC_R8(gb->cpu_reg.a, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x88:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x89:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x8A:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x8B:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x8C:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x8D:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x8E:
WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x8F:
WGB_INSTR_ADC_R8(gb->cpu_reg.a, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x90:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x91:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x92:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x93:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x94:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x95:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x96:
WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x97:
gb->cpu_reg.a = 0;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = 1;
gb->cpu_reg.f.f_bits.n = 1;
opcode = (uint8_t)(oppair >> 8);
break;
case 0x98:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x99:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x9A:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x9B:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x9C:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x9D:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x9E:
WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0x9F:
gb->cpu_reg.a = gb->cpu_reg.f.f_bits.c ? 0xFF : 0x00;
gb->cpu_reg.f.f_bits.z = !gb->cpu_reg.f.f_bits.c;
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h = gb->cpu_reg.f.f_bits.c;
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA0:
WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.b);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA1:
WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA2:
WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.d);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA3:
WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.e);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA4:
WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.h);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA5:
WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.l);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA6:
WGB_INSTR_AND_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA7:
WGB_INSTR_AND_R8(gb->cpu_reg.a);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA8:
WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.b);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xA9:
WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xAA:
WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.d);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xAB:
WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.e);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xAC:
WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.h);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xAD:
WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.l);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xAE:
WGB_INSTR_XOR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
opcode = (uint8_t)(oppair >> 8);
break;
case 0xAF:
WGB_INSTR_XOR_R8(gb->cpu_reg.a);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB0:
WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.b);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB1:
WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB2:
WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.d);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB3:
WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.e);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB4:
WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.h);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB5:
WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.l);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB6:
WGB_INSTR_OR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB7:
WGB_INSTR_OR_R8(gb->cpu_reg.a);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB8:
WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.b);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xB9:
WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.c);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xBA:
WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.d);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xBB:
WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.e);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xBC:
WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.h);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xBD:
WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.l);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xBE:
WGB_INSTR_CP_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
opcode = (uint8_t)(oppair >> 8);
break;
case 0xBF:
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = 1;
gb->cpu_reg.f.f_bits.n = 1;
opcode = (uint8_t)(oppair >> 8);
break;
case 0xC0:
if(!gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED3
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xC1:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED3
gb->cpu_reg.bc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
opcode = (uint8_t)(oppair >> 8);
break;
case 0xC2:
if(!gb->cpu_reg.f.f_bits.z)
{
uint8_t c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xC3:
{
uint8_t c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
}
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xC4:
if(!gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED3
uint8_t c, p;
c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.sp.reg -= 2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#else
uint8_t c, p;
c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xC5:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED3
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.bc.reg);
#else
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.bc.bytes.b);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.bc.bytes.c);
#endif
opcode = (uint8_t)(oppair >> 8);
break;
case 0xC6:
{
uint8_t val = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
WGB_INSTR_ADC_R8(val, 0);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xC7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0000;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xC8:
if(gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED2
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
}
else
opcode=(uint8_t)(oppair >> 8);
break;
case 0xC9:
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED2
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xCA:
if(gb->cpu_reg.f.f_bits.z)
{
uint8_t c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xCB:
inst_cycles = __gb_execute_cb(gb);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xCC:
if(gb->cpu_reg.f.f_bits.z)
{
uint8_t c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xCD:
{
uint8_t c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
}
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xCE:
{
uint8_t val = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
WGB_INSTR_ADC_R8(val, gb->cpu_reg.f.f_bits.c);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xCF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0008;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xD0:
if (!gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xD1:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.de.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
opcode = (uint8_t)(oppair >> 8);
break;
case 0xD2:
if(!gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xD4:
if(!gb->cpu_reg.f.f_bits.c)
{
uint8_t c =(uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
#else
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
#endif
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xD5:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.de.reg);
#else
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.de.bytes.d);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.de.bytes.e);
#endif
opcode = (uint8_t)(oppair >> 8);
break;
case 0xD6:
{
uint8_t val =(uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint16_t temp = gb->cpu_reg.a - val;
gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h =
(gb->cpu_reg.a ^ val ^ temp) & 0x10 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;
gb->cpu_reg.a = (temp & 0xFF);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xD7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0010;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xD8:
if(gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
}
else
opcode = (uint8_t)(oppair >> 8);
break;
case 0xD9:
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
gb->gb_ime = true;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
}
break;
case 0xDA:
if(gb->cpu_reg.f.f_bits.c)
{
uint8_t c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xDC:
if(gb->cpu_reg.f.f_bits.c)
{
uint8_t c = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
#else
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
#endif
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xDE:
{
uint8_t val = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
WGB_INSTR_SBC_R8(val, gb->cpu_reg.f.f_bits.c);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xDF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0018;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xE0:
__gb_write(gb, 0xFF00 | (uint8_t)(oppair >> 8),
gb->cpu_reg.a);
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xE1:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED_DISABLED
gb->cpu_reg.hl.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
opcode = (uint8_t)(oppair >> 8);
break;
case 0xE2:
__gb_write(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES_DISABLED
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0xE5:
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.hl.reg);
#else
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.hl.bytes.h);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.hl.bytes.l);
#endif
opcode = (uint8_t)(oppair >> 8);
break;
case 0xE6:
{
uint8_t temp = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
WGB_INSTR_AND_R8(temp);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xE7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0020;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xE8:
{
int8_t offset = (int8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF);
gb->cpu_reg.sp.reg += offset;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xE9:
gb->cpu_reg.pc.reg = gb->cpu_reg.hl.reg;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xEA:
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
uint8_t l = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
uint8_t h = oppair;
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
__gb_write(gb, addr, gb->cpu_reg.a);
opcode = oppair >> 8;
#else
uint8_t l = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
__gb_write(gb, addr, gb->cpu_reg.a);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
break;
}
case 0xEE:
WGB_INSTR_XOR_R8((uint8_t)(oppair >> 8));
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xEF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0028;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xF0:
gb->cpu_reg.a =
__gb_read(gb, 0xFF00 | (uint8_t)(oppair >> 8));
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xF1:
{
uint8_t temp_8 = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.f.f_bits.z = (temp_8 >> 7) & 1;
gb->cpu_reg.f.f_bits.n = (temp_8 >> 6) & 1;
gb->cpu_reg.f.f_bits.h = (temp_8 >> 5) & 1;
gb->cpu_reg.f.f_bits.c = (temp_8 >> 4) & 1;
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.sp.reg++);
opcode = (uint8_t)(oppair >> 8);
break;
}
case 0xF2:
gb->cpu_reg.a = __gb_read(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#else
opcode = (uint8_t)(oppair >> 8);
#endif
break;
case 0xF3:
gb->gb_ime = false;
opcode = (uint8_t)(oppair >> 8);
break;
case 0xF5:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.a);
__gb_write(gb, --gb->cpu_reg.sp.reg,
gb->cpu_reg.f.f_bits.z << 7 | gb->cpu_reg.f.f_bits.n << 6 |
gb->cpu_reg.f.f_bits.h << 5 | gb->cpu_reg.f.f_bits.c << 4);
opcode = (uint8_t)(oppair >> 8);
break;
case 0xF6:
WGB_INSTR_OR_R8((uint8_t)(oppair >> 8));
gb->cpu_reg.pc.reg++;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xF7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0030;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
case 0xF8:
{
int8_t offset = (int8_t) (oppair >> 8);
gb->cpu_reg.pc.reg++;
gb->cpu_reg.hl.reg = gb->cpu_reg.sp.reg + offset;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF) ? 1 : 0;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xF9:
gb->cpu_reg.sp.reg = gb->cpu_reg.hl.reg;
opcode = (uint8_t)(oppair >> 8);
break;
case 0xFA:
{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
uint8_t l = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
oppair = __gb_read16(gb, gb->cpu_reg.pc.reg++);
uint8_t h = oppair;
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
gb->cpu_reg.a = __gb_read(gb, addr);
opcode = oppair >> 8;
#else
uint8_t l = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
gb->cpu_reg.a = __gb_read(gb, addr);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
#endif
break;
}
case 0xFB:
gb->gb_ime = true;
opcode = (uint8_t)(oppair >> 8);
break;
case 0xFE:
{
uint8_t val = (uint8_t)(oppair >> 8);
gb->cpu_reg.pc.reg++;
WGB_INSTR_CP_R8(val);
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
case 0xFF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0038;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
default:
(gb->gb_error)(gb, GB_INVALID_OPCODE, gb->cpu_reg.pc.reg - 1);
WGB_UNREACHABLE();
return;
}
do
{
gb->counter.div_count += inst_cycles;
while(gb->counter.div_count >= DIV_CYCLES)
{
gb->hram_io[IO_DIV]++;
gb->counter.div_count -= DIV_CYCLES;
}
if(gb->mbc == 3 && (gb->rtc_real.reg.high & 0x40) == 0)
{
gb->counter.rtc_count += inst_cycles;
while(WGB_UNLIKELY(gb->counter.rtc_count >= RTC_CYCLES))
{
gb->counter.rtc_count -= RTC_CYCLES;
if(WGB_UNLIKELY(gb->rtc_real.reg.sec == 63))
{
gb->rtc_real.reg.sec = 0;
continue;
}
if(++gb->rtc_real.reg.sec != 60)
continue;
gb->rtc_real.reg.sec = 0;
if(gb->rtc_real.reg.min == 63)
{
gb->rtc_real.reg.min = 0;
continue;
}
if(++gb->rtc_real.reg.min != 60)
continue;
gb->rtc_real.reg.min = 0;
if(gb->rtc_real.reg.hour == 31)
{
gb->rtc_real.reg.hour = 0;
continue;
}
if(++gb->rtc_real.reg.hour != 24)
continue;
gb->rtc_real.reg.hour = 0;
if(++gb->rtc_real.reg.yday != 0)
continue;
if(gb->rtc_real.reg.high & 1)
gb->rtc_real.reg.high |= 0x80;
gb->rtc_real.reg.high ^= 1;
}
}
if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
{
unsigned int serial_cycles = SERIAL_CYCLES_1KB;
if(gb->counter.serial_count == 0 &&
gb->gb_serial_tx != NULL)
(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);
#if WALNUT_FULL_GBC_SUPPORT
if(gb->hram_io[IO_SC] & 0x3)
serial_cycles = SERIAL_CYCLES_32KB;
#endif
gb->counter.serial_count += inst_cycles;
if(gb->counter.serial_count >= serial_cycles)
{
uint8_t rx;
if(gb->gb_serial_rx != NULL &&
(gb->gb_serial_rx(gb, &rx) ==
GB_SERIAL_RX_SUCCESS))
{
gb->hram_io[IO_SB] = rx;
gb->hram_io[IO_SC] &= 0x01;
gb->hram_io[IO_IF] |= SERIAL_INTR;
}
else if(gb->hram_io[IO_SC] & SERIAL_SC_CLOCK_SRC)
{
gb->hram_io[IO_SB] = 0xFF;
gb->hram_io[IO_SC] &= 0x01;
gb->hram_io[IO_IF] |= SERIAL_INTR;
}
else
{
}
gb->counter.serial_count = 0;
}
}
if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
{
gb->counter.tima_count += inst_cycles;
while(gb->counter.tima_count >=
TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK])
{
gb->counter.tima_count -=
TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK];
if(++gb->hram_io[IO_TIMA] == 0)
{
gb->hram_io[IO_IF] |= TIMER_INTR;
gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
}
}
}
if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
{
gb->counter.lcd_off_count += inst_cycles;
if(gb->counter.lcd_off_count >= LCD_FRAME_CYCLES)
{
gb->counter.lcd_off_count -= LCD_FRAME_CYCLES;
gb->gb_frame = true;
}
continue;
}
#if WALNUT_FULL_GBC_SUPPORT
if (inst_cycles > 1) {
gb->counter.lcd_count += (inst_cycles >> gb->cgb.doubleSpeed);
} else {
#endif
gb->counter.lcd_count += inst_cycles;
#if WALNUT_FULL_GBC_SUPPORT
}
#endif
if(gb->counter.lcd_count >= LCD_LINE_CYCLES)
{
gb->counter.lcd_count -= LCD_LINE_CYCLES;
gb->hram_io[IO_LY] = gb->hram_io[IO_LY] + 1;
if (gb->hram_io[IO_LY] == LCD_VERT_LINES)
gb->hram_io[IO_LY] = 0;
if(gb->hram_io[IO_LY] == gb->hram_io[IO_LYC])
{
gb->hram_io[IO_STAT] |= STAT_LYC_COINC;
if(gb->hram_io[IO_STAT] & STAT_LYC_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
}
else
gb->hram_io[IO_STAT] &= 0xFB;
if(gb->hram_io[IO_LY] == LCD_HEIGHT)
{
gb->hram_io[IO_STAT] =
(gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_VBLANK;
gb->gb_frame = true;
gb->hram_io[IO_IF] |= VBLANK_INTR;
gb->lcd_blank = false;
if(gb->hram_io[IO_STAT] & STAT_MODE_1_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
#if ENABLE_LCD
if(gb->direct.frame_skip)
{
gb->display.frame_skip_count =
!gb->display.frame_skip_count;
}
if(gb->direct.interlace &&
(!gb->direct.frame_skip ||
gb->display.frame_skip_count))
{
gb->display.interlace_count =
!gb->display.interlace_count;
}
#endif
if(gb->gb_halt && !gb->hram_io[IO_IE])
break;
}
else if(gb->hram_io[IO_LY] < LCD_HEIGHT)
{
if(gb->hram_io[IO_LY] == 0)
{
gb->display.WY = gb->hram_io[IO_WY];
gb->display.window_clear = 0;
}
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_OAM_SCAN;
gb->counter.lcd_count = 0;
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode && !gb->cgb.dmaActive && gb->cgb.dmaMode)
{
#if WALNUT_GB_32BIT_DMA
for (uint8_t i = 0; i < 0x10; i += 4)
{
uint32_t val = __gb_read32(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
__gb_write32(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
}
#elif WALNUT_GB_16BIT_DMA
for (uint8_t i = 0; i < 0x10; i += 2)
{
uint16_t val = __gb_read16(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
__gb_write16(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
}
#else
for (uint8_t i = 0; i < 0x10; i++)
{
__gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i,
__gb_read(gb, (gb->cgb.dmaSource & 0xFFF0) + i));
}
#endif
gb->cgb.dmaSource += 0x10;
gb->cgb.dmaDest += 0x10;
if(!(--gb->cgb.dmaSize)) {gb->cgb.dmaActive = 1;
}
#if WALNUT_GB_SAFE_DUALFETCH_DMA
gb->prefetch_invalid=true;
#endif
}
#endif
if(gb->hram_io[IO_STAT] & STAT_MODE_2_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
inst_cycles = LCD_MODE2_OAM_SCAN_DURATION;
}
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW &&
gb->counter.lcd_count >= LCD_MODE3_LCD_DRAW_END)
{
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_HBLANK;
if(gb->hram_io[IO_STAT] & STAT_MODE_0_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
if (gb->counter.lcd_count < LCD_MODE0_HBLANK_MAX_DRUATION)
inst_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN &&
gb->counter.lcd_count >= LCD_MODE2_OAM_SCAN_END)
{
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_LCD_DRAW;
#if ENABLE_LCD
if(!gb->lcd_blank)
__gb_draw_line(gb);
#endif
if (gb->counter.lcd_count < LCD_MODE3_LCD_DRAW_MIN_DURATION)
inst_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
}
} while(gb->gb_halt && (gb->hram_io[IO_IF] & gb->hram_io[IO_IE]) == 0 && !gb->gb_frame);
if(gb->gb_halt && gb->hram_io[IO_IE] && (gb->hram_io[IO_IF] & gb->hram_io[IO_IE]) == 0)
return;
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
if (gb->prefetch_invalid)
{
gb->prefetch_invalid=false;
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
}
#endif
while(gb->gb_halt || (gb->gb_ime &&
gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & ANY_INTR))
{
gb->gb_halt = false;
if(!gb->gb_ime)
break;
gb->gb_ime = false;
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & VBLANK_INTR)
{
gb->cpu_reg.pc.reg = VBLANK_INTR_ADDR;
gb->hram_io[IO_IF] ^= VBLANK_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & LCDC_INTR)
{
gb->cpu_reg.pc.reg = LCDC_INTR_ADDR;
gb->hram_io[IO_IF] ^= LCDC_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & TIMER_INTR)
{
gb->cpu_reg.pc.reg = TIMER_INTR_ADDR;
gb->hram_io[IO_IF] ^= TIMER_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & SERIAL_INTR)
{
gb->cpu_reg.pc.reg = SERIAL_INTR_ADDR;
gb->hram_io[IO_IF] ^= SERIAL_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & CONTROL_INTR)
{
gb->cpu_reg.pc.reg = CONTROL_INTR_ADDR;
gb->hram_io[IO_IF] ^= CONTROL_INTR;
}
opcode = __gb_read(gb, gb->cpu_reg.pc.reg);
break;
}
gb->cpu_reg.pc.reg++;
inst_cycles = op_cycles[opcode];
switch(opcode)
{
case 0x00:
break;
case 0x01:
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.bc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.pc.reg++);
#endif
break;
case 0x02:
__gb_write(gb, gb->cpu_reg.bc.reg, gb->cpu_reg.a);
break;
case 0x03:
gb->cpu_reg.bc.reg++;
break;
case 0x04:
WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0x05:
WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0x06:
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x07:
gb->cpu_reg.a = (gb->cpu_reg.a << 1) | (gb->cpu_reg.a >> 7);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a & 0x01);
break;
case 0x08:
{
#if WALNUT_GB_16_BIT_OPS
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
__gb_write16(gb, addr, gb->cpu_reg.sp.reg);
#else
uint8_t l = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
__gb_write(gb, addr++, gb->cpu_reg.sp.bytes.p);
__gb_write(gb, addr, gb->cpu_reg.sp.bytes.s);
#endif
break;
}
case 0x09:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.bc.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.bc.reg) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
break;
}
case 0x0A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.bc.reg);
break;
case 0x0B:
gb->cpu_reg.bc.reg--;
break;
case 0x0C:
WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0x0D:
WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0x0E:
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x0F:
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 0x01;
gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (gb->cpu_reg.a << 7);
break;
case 0x10:
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode & gb->cgb.doubleSpeedPrep)
{
gb->cgb.doubleSpeedPrep = 0;
gb->cgb.doubleSpeed ^= 1;
}
#endif
break;
case 0x11:
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.de.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.pc.reg++);
#endif
break;
case 0x12:
__gb_write(gb, gb->cpu_reg.de.reg, gb->cpu_reg.a);
break;
case 0x13:
gb->cpu_reg.de.reg++;
break;
case 0x14:
WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.d);
break;
case 0x15:
WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.d);
break;
case 0x16:
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x17:
{
uint8_t temp = gb->cpu_reg.a;
gb->cpu_reg.a = (gb->cpu_reg.a << 1) | gb->cpu_reg.f.f_bits.c;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = (temp >> 7) & 0x01;
break;
}
case 0x18:
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
break;
}
case 0x19:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.de.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.de.reg) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
break;
}
case 0x1A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.de.reg);
break;
case 0x1B:
gb->cpu_reg.de.reg--;
break;
case 0x1C:
WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.e);
break;
case 0x1D:
WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.e);
break;
case 0x1E:
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x1F:
{
uint8_t temp = gb->cpu_reg.a;
gb->cpu_reg.a = gb->cpu_reg.a >> 1 | (gb->cpu_reg.f.f_bits.c << 7);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = temp & 0x1;
break;
}
case 0x20:
if(!gb->cpu_reg.f.f_bits.z)
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
break;
case 0x21:
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.hl.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.pc.reg++);
#endif
break;
case 0x22:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
gb->cpu_reg.hl.reg++;
break;
case 0x23:
gb->cpu_reg.hl.reg++;
break;
case 0x24:
WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0x25:
WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0x26:
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x27:
{
int16_t a = gb->cpu_reg.a;
if(gb->cpu_reg.f.f_bits.n)
{
if(gb->cpu_reg.f.f_bits.h)
a = (a - 0x06) & 0xFF;
if(gb->cpu_reg.f.f_bits.c)
a -= 0x60;
}
else
{
if(gb->cpu_reg.f.f_bits.h || (a & 0x0F) > 9)
a += 0x06;
if(gb->cpu_reg.f.f_bits.c || a > 0x9F)
a += 0x60;
}
if((a & 0x100) == 0x100)
gb->cpu_reg.f.f_bits.c = 1;
gb->cpu_reg.a = (uint8_t)a;
gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0);
gb->cpu_reg.f.f_bits.h = 0;
break;
}
case 0x28:
if(gb->cpu_reg.f.f_bits.z)
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
break;
case 0x29:
{
gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.hl.reg & 0x8000) > 0;
gb->cpu_reg.hl.reg <<= 1;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = (gb->cpu_reg.hl.reg & 0x1000) > 0;
break;
}
case 0x2A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg++);
break;
case 0x2B:
gb->cpu_reg.hl.reg--;
break;
case 0x2C:
WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0x2D:
WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0x2E:
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x2F:
gb->cpu_reg.a = ~gb->cpu_reg.a;
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h = 1;
break;
case 0x30:
if(!gb->cpu_reg.f.f_bits.c)
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
break;
case 0x31:
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.sp.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
gb->cpu_reg.sp.bytes.p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.sp.bytes.s = __gb_read(gb, gb->cpu_reg.pc.reg++);
#endif
break;
case 0x32:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
gb->cpu_reg.hl.reg--;
break;
case 0x33:
gb->cpu_reg.sp.reg++;
break;
case 0x34:
{
uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
WGB_INSTR_INC_R8(temp);
__gb_write(gb, gb->cpu_reg.hl.reg, temp);
break;
}
case 0x35:
{
uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
WGB_INSTR_DEC_R8(temp);
__gb_write(gb, gb->cpu_reg.hl.reg, temp);
break;
}
case 0x36:
__gb_write(gb, gb->cpu_reg.hl.reg, __gb_read(gb, gb->cpu_reg.pc.reg++));
break;
case 0x37:
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = 0;
gb->cpu_reg.f.f_bits.c = 1;
break;
case 0x38:
if(gb->cpu_reg.f.f_bits.c)
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
break;
case 0x39:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.sp.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
((gb->cpu_reg.hl.reg & 0xFFF) + (gb->cpu_reg.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = temp & 0x10000 ? 1 : 0;
gb->cpu_reg.hl.reg = (uint16_t)temp;
break;
}
case 0x3A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg--);
break;
case 0x3B:
gb->cpu_reg.sp.reg--;
break;
case 0x3C:
WGB_INSTR_INC_R8(gb->cpu_reg.a);
break;
case 0x3D:
WGB_INSTR_DEC_R8(gb->cpu_reg.a);
break;
case 0x3E:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x3F:
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = 0;
gb->cpu_reg.f.f_bits.c = ~gb->cpu_reg.f.f_bits.c;
break;
case 0x40:
break;
case 0x41:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.bc.bytes.c;
break;
case 0x42:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.d;
break;
case 0x43:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.e;
break;
case 0x44:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.h;
break;
case 0x45:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.l;
break;
case 0x46:
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x47:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.a;
break;
case 0x48:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.bc.bytes.b;
break;
case 0x49:
break;
case 0x4A:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.d;
break;
case 0x4B:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.e;
break;
case 0x4C:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.h;
break;
case 0x4D:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.l;
break;
case 0x4E:
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x4F:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.a;
break;
case 0x50:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.b;
break;
case 0x51:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.c;
break;
case 0x52:
break;
case 0x53:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.de.bytes.e;
break;
case 0x54:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.h;
break;
case 0x55:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.l;
break;
case 0x56:
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x57:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.a;
break;
case 0x58:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.b;
break;
case 0x59:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.c;
break;
case 0x5A:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.de.bytes.d;
break;
case 0x5B:
break;
case 0x5C:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.h;
break;
case 0x5D:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.l;
break;
case 0x5E:
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x5F:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.a;
break;
case 0x60:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.b;
break;
case 0x61:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.c;
break;
case 0x62:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.d;
break;
case 0x63:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.e;
break;
case 0x64:
break;
case 0x65:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.hl.bytes.l;
break;
case 0x66:
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x67:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.a;
break;
case 0x68:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.b;
break;
case 0x69:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.c;
break;
case 0x6A:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.d;
break;
case 0x6B:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.e;
break;
case 0x6C:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.hl.bytes.h;
break;
case 0x6D:
break;
case 0x6E:
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x6F:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.a;
break;
case 0x70:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.b);
break;
case 0x71:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.c);
break;
case 0x72:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.d);
break;
case 0x73:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.e);
break;
case 0x74:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.h);
break;
case 0x75:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.l);
break;
case 0x76:
{
int_fast16_t halt_cycles = INT_FAST16_MAX;
gb->gb_halt = true;
if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
{
int serial_cycles = SERIAL_CYCLES -
gb->counter.serial_count;
if(serial_cycles < halt_cycles)
halt_cycles = serial_cycles;
}
if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
{
int tac_cycles = TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK] -
gb->counter.tima_count;
if(tac_cycles < halt_cycles)
halt_cycles = tac_cycles;
}
if((gb->hram_io[IO_LCDC] & LCDC_ENABLE))
{
int lcd_cycles;
if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_HBLANK)
{
lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN)
{
lcd_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW)
{
lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else
{
lcd_cycles = LCD_LINE_CYCLES - gb->counter.lcd_count;
}
if(lcd_cycles < halt_cycles)
halt_cycles = lcd_cycles;
}
if(halt_cycles <= 0)
halt_cycles = 4;
inst_cycles = (uint_fast16_t)halt_cycles;
break;
}
case 0x77:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
break;
case 0x78:
gb->cpu_reg.a = gb->cpu_reg.bc.bytes.b;
break;
case 0x79:
gb->cpu_reg.a = gb->cpu_reg.bc.bytes.c;
break;
case 0x7A:
gb->cpu_reg.a = gb->cpu_reg.de.bytes.d;
break;
case 0x7B:
gb->cpu_reg.a = gb->cpu_reg.de.bytes.e;
break;
case 0x7C:
gb->cpu_reg.a = gb->cpu_reg.hl.bytes.h;
break;
case 0x7D:
gb->cpu_reg.a = gb->cpu_reg.hl.bytes.l;
break;
case 0x7E:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x7F:
break;
case 0x80:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, 0);
break;
case 0x81:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, 0);
break;
case 0x82:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, 0);
break;
case 0x83:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, 0);
break;
case 0x84:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, 0);
break;
case 0x85:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, 0);
break;
case 0x86:
WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
break;
case 0x87:
WGB_INSTR_ADC_R8(gb->cpu_reg.a, 0);
break;
case 0x88:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
break;
case 0x89:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
break;
case 0x8A:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
break;
case 0x8B:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
break;
case 0x8C:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
break;
case 0x8D:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
break;
case 0x8E:
WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
break;
case 0x8F:
WGB_INSTR_ADC_R8(gb->cpu_reg.a, gb->cpu_reg.f.f_bits.c);
break;
case 0x90:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, 0);
break;
case 0x91:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, 0);
break;
case 0x92:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, 0);
break;
case 0x93:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, 0);
break;
case 0x94:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, 0);
break;
case 0x95:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, 0);
break;
case 0x96:
WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
break;
case 0x97:
gb->cpu_reg.a = 0;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = 1;
gb->cpu_reg.f.f_bits.n = 1;
break;
case 0x98:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
break;
case 0x99:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
break;
case 0x9A:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
break;
case 0x9B:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
break;
case 0x9C:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
break;
case 0x9D:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
break;
case 0x9E:
WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
break;
case 0x9F:
gb->cpu_reg.a = gb->cpu_reg.f.f_bits.c ? 0xFF : 0x00;
gb->cpu_reg.f.f_bits.z = !gb->cpu_reg.f.f_bits.c;
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h = gb->cpu_reg.f.f_bits.c;
break;
case 0xA0:
WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0xA1:
WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0xA2:
WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.d);
break;
case 0xA3:
WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.e);
break;
case 0xA4:
WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0xA5:
WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0xA6:
WGB_INSTR_AND_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
break;
case 0xA7:
WGB_INSTR_AND_R8(gb->cpu_reg.a);
break;
case 0xA8:
WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0xA9:
WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0xAA:
WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.d);
break;
case 0xAB:
WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.e);
break;
case 0xAC:
WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0xAD:
WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0xAE:
WGB_INSTR_XOR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
break;
case 0xAF:
WGB_INSTR_XOR_R8(gb->cpu_reg.a);
break;
case 0xB0:
WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0xB1:
WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0xB2:
WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.d);
break;
case 0xB3:
WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.e);
break;
case 0xB4:
WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0xB5:
WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0xB6:
WGB_INSTR_OR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
break;
case 0xB7:
WGB_INSTR_OR_R8(gb->cpu_reg.a);
break;
case 0xB8:
WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0xB9:
WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0xBA:
WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.d);
break;
case 0xBB:
WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.e);
break;
case 0xBC:
WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0xBD:
WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0xBE:
WGB_INSTR_CP_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
break;
case 0xBF:
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = 1;
gb->cpu_reg.f.f_bits.n = 1;
break;
case 0xC0:
if(!gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
break;
case 0xC1:
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.bc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
break;
case 0xC2:
if(!gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xC3:
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
{
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
}
#endif
break;
case 0xC4:
if(!gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
gb->cpu_reg.sp.reg -= 2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg = addr;
#else
uint8_t c, p;
c = __gb_read(gb, gb->cpu_reg.pc.reg++);
p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xC5:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.bc.bytes.b);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.bc.bytes.c);
break;
case 0xC6:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_ADC_R8(val, 0);
break;
}
case 0xC7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0000;
break;
case 0xC8:
if(gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
break;
case 0xC9:
{
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
break;
}
case 0xCA:
if(gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xCB:
inst_cycles = __gb_execute_cb(gb);
break;
case 0xCC:
if(gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_OPS
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg = addr;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xCD:
#if WALNUT_GB_16_BIT_OPS
{
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg = addr;
}
#else
{
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
}
#endif
break;
case 0xCE:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_ADC_R8(val, gb->cpu_reg.f.f_bits.c);
break;
}
case 0xCF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0008;
break;
case 0xD0:
if (!gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
break;
case 0xD1:
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.de.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
break;
case 0xD2:
if(!gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xD4:
if(!gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_OPS
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg = addr;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xD5:
#if WALNUT_GB_16_BIT_OPS
gb->cpu_reg.sp.reg-=2;
__gb_write16(gb,gb->cpu_reg.sp.reg,gb->cpu_reg.de.reg);
#else
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.de.bytes.d);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.de.bytes.e);
#endif
break;
case 0xD6:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t temp = gb->cpu_reg.a - val;
gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h =
(gb->cpu_reg.a ^ val ^ temp) & 0x10 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;
gb->cpu_reg.a = (temp & 0xFF);
break;
}
case 0xD7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0010;
break;
case 0xD8:
if(gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
break;
case 0xD9:
{
#if WALNUT_GB_16_BIT
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
gb->gb_ime = true;
}
break;
case 0xDA:
if(gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xDC:
if(gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT
uint16_t target = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
gb->cpu_reg.sp.reg -= 2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg)
gb->cpu_reg.pc.reg = target;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xDE:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_SBC_R8(val, gb->cpu_reg.f.f_bits.c);
break;
}
case 0xDF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0018;
break;
case 0xE0:
__gb_write(gb, 0xFF00 | __gb_read(gb, gb->cpu_reg.pc.reg++),
gb->cpu_reg.a);
break;
case 0xE1:
#if WALNUT_GB_16_BIT
gb->cpu_reg.hl.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
break;
case 0xE2:
__gb_write(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a);
break;
case 0xE5:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.hl.bytes.h);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.hl.bytes.l);
break;
case 0xE6:
{
uint8_t temp = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_AND_R8(temp);
break;
}
case 0xE7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0020;
break;
case 0xE8:
{
int8_t offset = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF);
gb->cpu_reg.sp.reg += offset;
break;
}
case 0xE9:
gb->cpu_reg.pc.reg = gb->cpu_reg.hl.reg;
break;
case 0xEA:
{
#if WALNUT_GB_16_BIT
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t l = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
__gb_write(gb, addr, gb->cpu_reg.a);
break;
}
case 0xEE:
WGB_INSTR_XOR_R8(__gb_read(gb, gb->cpu_reg.pc.reg++));
break;
case 0xEF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0028;
break;
case 0xF0:
gb->cpu_reg.a =
__gb_read(gb, 0xFF00 | __gb_read(gb, gb->cpu_reg.pc.reg++));
break;
case 0xF1:
{
uint8_t temp_8 = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.f.f_bits.z = (temp_8 >> 7) & 1;
gb->cpu_reg.f.f_bits.n = (temp_8 >> 6) & 1;
gb->cpu_reg.f.f_bits.h = (temp_8 >> 5) & 1;
gb->cpu_reg.f.f_bits.c = (temp_8 >> 4) & 1;
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.sp.reg++);
break;
}
case 0xF2:
gb->cpu_reg.a = __gb_read(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c);
break;
case 0xF3:
gb->gb_ime = false;
break;
case 0xF5:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.a);
__gb_write(gb, --gb->cpu_reg.sp.reg,
gb->cpu_reg.f.f_bits.z << 7 | gb->cpu_reg.f.f_bits.n << 6 |
gb->cpu_reg.f.f_bits.h << 5 | gb->cpu_reg.f.f_bits.c << 4);
break;
case 0xF6:
WGB_INSTR_OR_R8(__gb_read(gb, gb->cpu_reg.pc.reg++));
break;
case 0xF7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0030;
break;
case 0xF8:
{
int8_t offset = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.hl.reg = gb->cpu_reg.sp.reg + offset;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF) ? 1 : 0;
break;
}
case 0xF9:
gb->cpu_reg.sp.reg = gb->cpu_reg.hl.reg;
break;
case 0xFA:
{
#if WALNUT_GB_16_BIT
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t l = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
gb->cpu_reg.a = __gb_read(gb, addr);
break;
}
case 0xFB:
gb->gb_ime = true;
break;
case 0xFE:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_CP_R8(val);
break;
}
case 0xFF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0038;
break;
default:
(gb->gb_error)(gb, GB_INVALID_OPCODE, gb->cpu_reg.pc.reg - 1);
WGB_UNREACHABLE();
}
do
{
gb->counter.div_count += inst_cycles;
while(gb->counter.div_count >= DIV_CYCLES)
{
gb->hram_io[IO_DIV]++;
gb->counter.div_count -= DIV_CYCLES;
}
if(gb->mbc == 3 && (gb->rtc_real.reg.high & 0x40) == 0)
{
gb->counter.rtc_count += inst_cycles;
while(WGB_UNLIKELY(gb->counter.rtc_count >= RTC_CYCLES))
{
gb->counter.rtc_count -= RTC_CYCLES;
if(WGB_UNLIKELY(gb->rtc_real.reg.sec == 63))
{
gb->rtc_real.reg.sec = 0;
continue;
}
if(++gb->rtc_real.reg.sec != 60)
continue;
gb->rtc_real.reg.sec = 0;
if(gb->rtc_real.reg.min == 63)
{
gb->rtc_real.reg.min = 0;
continue;
}
if(++gb->rtc_real.reg.min != 60)
continue;
gb->rtc_real.reg.min = 0;
if(gb->rtc_real.reg.hour == 31)
{
gb->rtc_real.reg.hour = 0;
continue;
}
if(++gb->rtc_real.reg.hour != 24)
continue;
gb->rtc_real.reg.hour = 0;
if(++gb->rtc_real.reg.yday != 0)
continue;
if(gb->rtc_real.reg.high & 1)
gb->rtc_real.reg.high |= 0x80;
gb->rtc_real.reg.high ^= 1;
}
}
if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
{
unsigned int serial_cycles = SERIAL_CYCLES_1KB;
if(gb->counter.serial_count == 0 &&
gb->gb_serial_tx != NULL)
(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);
#if WALNUT_FULL_GBC_SUPPORT
if(gb->hram_io[IO_SC] & 0x3)
serial_cycles = SERIAL_CYCLES_32KB;
#endif
gb->counter.serial_count += inst_cycles;
if(gb->counter.serial_count >= serial_cycles)
{
uint8_t rx;
if(gb->gb_serial_rx != NULL &&
(gb->gb_serial_rx(gb, &rx) ==
GB_SERIAL_RX_SUCCESS))
{
gb->hram_io[IO_SB] = rx;
gb->hram_io[IO_SC] &= 0x01;
gb->hram_io[IO_IF] |= SERIAL_INTR;
}
else if(gb->hram_io[IO_SC] & SERIAL_SC_CLOCK_SRC)
{
gb->hram_io[IO_SB] = 0xFF;
gb->hram_io[IO_SC] &= 0x01;
gb->hram_io[IO_IF] |= SERIAL_INTR;
}
else
{
}
gb->counter.serial_count = 0;
}
}
if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
{
gb->counter.tima_count += inst_cycles;
while(gb->counter.tima_count >=
TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK])
{
gb->counter.tima_count -=
TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK];
if(++gb->hram_io[IO_TIMA] == 0)
{
gb->hram_io[IO_IF] |= TIMER_INTR;
gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
}
}
}
if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
{
gb->counter.lcd_off_count += inst_cycles;
if(gb->counter.lcd_off_count >= LCD_FRAME_CYCLES)
{
gb->counter.lcd_off_count -= LCD_FRAME_CYCLES;
gb->gb_frame = true;
}
continue;
}
#if WALNUT_FULL_GBC_SUPPORT
if (inst_cycles > 1) {
gb->counter.lcd_count += (inst_cycles >> gb->cgb.doubleSpeed);
} else {
#endif
gb->counter.lcd_count += inst_cycles;
#if WALNUT_FULL_GBC_SUPPORT
}
#endif
if(gb->counter.lcd_count >= LCD_LINE_CYCLES)
{
gb->counter.lcd_count -= LCD_LINE_CYCLES;
gb->hram_io[IO_LY] = gb->hram_io[IO_LY] + 1;
if (gb->hram_io[IO_LY] == LCD_VERT_LINES)
gb->hram_io[IO_LY] = 0;
if(gb->hram_io[IO_LY] == gb->hram_io[IO_LYC])
{
gb->hram_io[IO_STAT] |= STAT_LYC_COINC;
if(gb->hram_io[IO_STAT] & STAT_LYC_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
}
else
gb->hram_io[IO_STAT] &= 0xFB;
if(gb->hram_io[IO_LY] == LCD_HEIGHT)
{
gb->hram_io[IO_STAT] =
(gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_VBLANK;
gb->gb_frame = true;
gb->hram_io[IO_IF] |= VBLANK_INTR;
gb->lcd_blank = false;
if(gb->hram_io[IO_STAT] & STAT_MODE_1_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
#if ENABLE_LCD
if(gb->direct.frame_skip)
{
gb->display.frame_skip_count =
!gb->display.frame_skip_count;
}
if(gb->direct.interlace &&
(!gb->direct.frame_skip ||
gb->display.frame_skip_count))
{
gb->display.interlace_count =
!gb->display.interlace_count;
}
#endif
if(gb->gb_halt && !gb->hram_io[IO_IE])
break;
}
else if(gb->hram_io[IO_LY] < LCD_HEIGHT)
{
if(gb->hram_io[IO_LY] == 0)
{
gb->display.WY = gb->hram_io[IO_WY];
gb->display.window_clear = 0;
}
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_OAM_SCAN;
gb->counter.lcd_count = 0;
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode && !gb->cgb.dmaActive && gb->cgb.dmaMode)
{
#if WALNUT_GB_32BIT_DMA
for (uint8_t i = 0; i < 0x10; i += 4)
{
uint32_t val = __gb_read32(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
__gb_write32(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
}
#elif WALNUT_GB_16BIT_DMA
for (uint8_t i = 0; i < 0x10; i += 2)
{
uint16_t val = __gb_read16(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
__gb_write16(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
}
#else
for (uint8_t i = 0; i < 0x10; i++)
{
__gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i,
__gb_read(gb, (gb->cgb.dmaSource & 0xFFF0) + i));
}
#endif
gb->cgb.dmaSource += 0x10;
gb->cgb.dmaDest += 0x10;
if(!(--gb->cgb.dmaSize)) {gb->cgb.dmaActive = 1;
}
}
#endif
if(gb->hram_io[IO_STAT] & STAT_MODE_2_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
inst_cycles = LCD_MODE2_OAM_SCAN_DURATION;
}
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW &&
gb->counter.lcd_count >= LCD_MODE3_LCD_DRAW_END)
{
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_HBLANK;
if(gb->hram_io[IO_STAT] & STAT_MODE_0_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
if (gb->counter.lcd_count < LCD_MODE0_HBLANK_MAX_DRUATION)
inst_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN &&
gb->counter.lcd_count >= LCD_MODE2_OAM_SCAN_END)
{
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_LCD_DRAW;
#if ENABLE_LCD
if(!gb->lcd_blank)
__gb_draw_line(gb);
#endif
if (gb->counter.lcd_count < LCD_MODE3_LCD_DRAW_MIN_DURATION)
inst_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
}
} while(gb->gb_halt && (gb->hram_io[IO_IF] & gb->hram_io[IO_IE]) == 0 && !gb->gb_frame);
}
void gb_run_frame(struct gb_s *gb)
{
gb->gb_frame = false;
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
gb->prefetch_invalid=false;
#endif
while(!gb->gb_frame)
{
__gb_step_cpu_x(gb);
}
}
void gb_run_frame_dualfetch(struct gb_s *gb)
{
gb->gb_frame = false;
#if (WALNUT_GB_SAFE_DUALFETCH_DMA || WALNUT_GB_SAFE_DUALFETCH_MBC)
gb->prefetch_invalid=false;
#endif
while(!gb->gb_frame)
{
__gb_step_cpu(gb);
}
}
int gb_get_save_size_s(struct gb_s *gb, size_t *ram_size)
{
const uint_fast16_t ram_size_location = 0x0149;
const uint_fast32_t ram_sizes[] =
{
0x00, 0x800, 0x2000, 0x8000, 0x20000, 0x10000
};
uint8_t ram_size_code = gb->gb_rom_read(gb, ram_size_location);
if(gb->mbc == 2)
{
*ram_size = 0x200;
return 0;
}
if(ram_size_code >= WALNUT_GB_ARRAYSIZE(ram_sizes))
return -1;
*ram_size = ram_sizes[ram_size_code];
return 0;
}
WGB_DEPRECATED("Does not return error code. Use gb_get_save_size_s instead.")
uint_fast32_t gb_get_save_size(struct gb_s *gb)
{
const uint_fast16_t ram_size_location = 0x0149;
const uint_fast32_t ram_sizes[] =
{
0x00, 0x800, 0x2000, 0x8000, 0x20000, 0x10000
};
uint8_t ram_size_code = gb->gb_rom_read(gb, ram_size_location);
if(gb->mbc == 2)
return 0x200;
if(ram_size_code >= WALNUT_GB_ARRAYSIZE(ram_sizes))
return 0;
return ram_sizes[ram_size_code];
}
void gb_init_serial(struct gb_s *gb,
void (*gb_serial_tx)(struct gb_s*, const uint8_t),
enum gb_serial_rx_ret_e (*gb_serial_rx)(struct gb_s*,
uint8_t*))
{
gb->gb_serial_tx = gb_serial_tx;
gb->gb_serial_rx = gb_serial_rx;
}
uint8_t gb_colour_hash(struct gb_s *gb)
{
#define ROM_TITLE_START_ADDR	0x0134
#define ROM_TITLE_END_ADDR	0x0143
uint8_t x = 0;
uint16_t i;
for(i = ROM_TITLE_START_ADDR; i <= ROM_TITLE_END_ADDR; i++)
x += gb->gb_rom_read(gb, i);
return x;
}
void gb_reset(struct gb_s *gb)
{
gb->gb_halt = false;
gb->gb_ime = true;
gb->selected_rom_bank = 1;
gb->cart_ram_bank = 0;
gb->enable_cart_ram = 0;
gb->cart_mode_select = 0;
if(gb->gb_bootrom_read == NULL)
{
uint8_t hdr_chk;
hdr_chk = gb->gb_rom_read(gb, ROM_HEADER_CHECKSUM_LOC) != 0;
gb->cpu_reg.a = 0x01;
gb->cpu_reg.f.f_bits.z = 1;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = hdr_chk;
gb->cpu_reg.f.f_bits.c = hdr_chk;
gb->cpu_reg.bc.reg = 0x0013;
gb->cpu_reg.de.reg = 0x00D8;
gb->cpu_reg.hl.reg = 0x014D;
gb->cpu_reg.sp.reg = 0xFFFE;
gb->cpu_reg.pc.reg = 0x0100;
gb->hram_io[IO_DIV ] = 0xAB;
gb->hram_io[IO_LCDC] = 0x91;
gb->hram_io[IO_STAT] = 0x85;
gb->hram_io[IO_BOOT] = 0x01;
__gb_write(gb, 0xFF26, 0xF1);
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode)
{
gb->cpu_reg.a = 0x11;
gb->cpu_reg.f.f_bits.z = 1;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = hdr_chk;
gb->cpu_reg.f.f_bits.c = hdr_chk;
gb->cpu_reg.bc.reg = 0x0000;
gb->cpu_reg.de.reg = 0x0008;
gb->cpu_reg.hl.reg = 0x007C;
gb->hram_io[IO_DIV] = 0xFF;
}
#endif
memset(gb->vram, 0x00, VRAM_SIZE);
}
else
{
gb->cpu_reg.pc.reg = 0x0000;
gb->hram_io[IO_DIV ] = 0x00;
gb->hram_io[IO_LCDC] = 0x00;
gb->hram_io[IO_STAT] = 0x84;
gb->hram_io[IO_BOOT] = 0x00;
}
gb->counter.lcd_count = 0;
gb->counter.div_count = 0;
gb->counter.tima_count = 0;
gb->counter.serial_count = 0;
gb->counter.rtc_count = 0;
gb->counter.lcd_off_count = 0;
gb->direct.joypad = 0xFF;
gb->hram_io[IO_JOYP] = 0xCF;
gb->hram_io[IO_SB  ] = 0x00;
gb->hram_io[IO_SC  ] = 0x7E;
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode) gb->hram_io[IO_SC] = 0x7F;
#endif
gb->hram_io[IO_TIMA] = 0x00;
gb->hram_io[IO_TMA ] = 0x00;
gb->hram_io[IO_TAC ] = 0xF8;
gb->hram_io[IO_IF  ] = 0xE1;
gb->hram_io[IO_SCY ] = 0x00;
gb->hram_io[IO_SCX ] = 0x00;
gb->hram_io[IO_LY  ] = 0x00;
gb->hram_io[IO_LYC ] = 0x00;
__gb_write(gb, 0xFF47, 0xFC);
__gb_write(gb, 0xFF48, 0xFF);
__gb_write(gb, 0xFF49, 0xFF);
gb->hram_io[IO_WY] = 0x00;
gb->hram_io[IO_WX] = 0x00;
gb->hram_io[IO_IE] = 0x00;
gb->hram_io[IO_IF] = 0xE1;
#if WALNUT_FULL_GBC_SUPPORT
gb->cgb.doubleSpeed = 0;
gb->cgb.doubleSpeedPrep = 0;
gb->cgb.wramBank = 1;
gb->cgb.wramBankOffset = WRAM_0_ADDR;
gb->cgb.vramBank = 0;
gb->cgb.vramBankOffset = VRAM_ADDR;
for (int i = 0; i < 0x20; i++)
{
gb->cgb.OAMPalette[(i << 1)] = gb->cgb.BGPalette[(i << 1)] = 0x7F;
gb->cgb.OAMPalette[(i << 1) + 1] = gb->cgb.BGPalette[(i << 1) + 1] = 0xFF;
}
gb->cgb.OAMPaletteID = 0;
gb->cgb.BGPaletteID = 0;
gb->cgb.OAMPaletteInc = 0;
gb->cgb.BGPaletteInc = 0;
gb->cgb.dmaActive = 1;
gb->cgb.dmaMode = 0;
gb->cgb.dmaSize = 0;
gb->cgb.dmaSource = 0;
gb->cgb.dmaDest = 0;
#endif
}
enum gb_init_error_e gb_init(struct gb_s *gb,
uint8_t (*gb_rom_read)(struct gb_s*, const uint_fast32_t),
uint16_t (*gb_rom_read16)(struct gb_s*, const uint_fast32_t),
uint32_t (*gb_rom_read32)(struct gb_s*, const uint_fast32_t),
uint8_t (*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t),
void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t, const uint8_t),
void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t),
void *priv)
{
#if WALNUT_FULL_GBC_SUPPORT
const uint16_t cgb_flag = 0x0143;
#endif
const uint16_t mbc_location = 0x0147;
const uint16_t bank_count_location = 0x0148;
const uint16_t ram_size_location = 0x0149;
const int8_t cart_mbc[] =
{
0, 1, 1, 1, -1, 2, 2, -1, 0, 0, -1, 0, 0, 0, -1, 3,
3, 3, 3, 3, -1, -1, -1, -1, -1, 5, 5, 5, 5, 5, 5, -1
};
const uint8_t cart_ram[] =
{
0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0,
1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0
};
const uint16_t num_rom_banks_mask[] =
{
2, 4, 8, 16, 32, 64, 128, 256, 512
};
const uint8_t num_ram_banks[] = { 0, 1, 1, 4, 16, 8 };
gb->gb_rom_read = gb_rom_read;
gb->gb_rom_read_16bit = gb_rom_read16;
gb->gb_rom_read_32bit = gb_rom_read32;
gb->gb_cart_ram_read = gb_cart_ram_read;
gb->gb_cart_ram_write = gb_cart_ram_write;
gb->gb_error = gb_error;
gb->direct.priv = priv;
gb->gb_serial_tx = NULL;
gb->gb_serial_rx = NULL;
gb->gb_bootrom_read = NULL;
{
uint8_t x = 0;
uint16_t i;
for(i = 0x0134; i <= 0x014C; i++)
x = x - gb->gb_rom_read(gb, i) - 1;
if(x != gb->gb_rom_read(gb, ROM_HEADER_CHECKSUM_LOC))
return GB_INIT_INVALID_CHECKSUM;
}
{
#if WALNUT_FULL_GBC_SUPPORT
gb->cgb.cgbMode = (gb_rom_read(gb, cgb_flag) & 0x80) >> 7;
#endif
const uint8_t mbc_value = gb->gb_rom_read(gb, mbc_location);
if(mbc_value > sizeof(cart_mbc) - 1 ||
(gb->mbc = cart_mbc[mbc_value]) == -1)
return GB_INIT_CARTRIDGE_UNSUPPORTED;
}
gb->num_rom_banks_mask = num_rom_banks_mask[gb_rom_read(gb, bank_count_location)] - 1;
gb->cart_ram = cart_ram[gb_rom_read(gb, mbc_location)];
gb->num_ram_banks = num_ram_banks[gb_rom_read(gb, ram_size_location)];
if(gb->cart_ram == 0 || gb->num_ram_banks == 0)
{
gb->cart_ram = 0;
gb->num_ram_banks = 0;
}
if(gb->mbc == 3)
gb->cart_is_mbc3O = gb->num_rom_banks_mask > 128 || gb->num_ram_banks > 4;
gb->lcd_blank = false;
gb->display.lcd_draw_line = NULL;
gb_reset(gb);
return GB_INIT_NO_ERROR;
}
const char* gb_get_rom_name(struct gb_s* gb, char *title_str)
{
uint_fast16_t title_loc = 0x134;
const uint_fast16_t title_end = 0x143;
const char* title_start = title_str;
for(; title_loc <= title_end; title_loc++)
{
const char title_char = gb->gb_rom_read(gb, title_loc);
if(title_char >= ' ' && title_char <= '_')
{
*title_str = title_char;
title_str++;
}
else
break;
}
*title_str = '\0';
return title_start;
}
#if ENABLE_LCD
void gb_init_lcd(struct gb_s *gb,
void (*lcd_draw_line)(struct gb_s *gb,
const uint8_t *pixels,
const uint_fast8_t line))
{
gb->display.lcd_draw_line = lcd_draw_line;
gb->direct.interlace = false;
gb->display.interlace_count = false;
gb->direct.frame_skip = false;
gb->display.frame_skip_count = false;
gb->display.window_clear = 0;
gb->display.WY = 0;
return;
}
#endif
void gb_set_bootrom(struct gb_s *gb,
uint8_t (*gb_bootrom_read)(struct gb_s*, const uint_fast16_t))
{
gb->gb_bootrom_read = gb_bootrom_read;
}
WGB_DEPRECATED("RTC is now ticked internally; this function has no effect")
void gb_tick_rtc(struct gb_s *gb)
{
(void) gb;
return;
}
void gb_set_rtc(struct gb_s *gb, const struct tm * const time)
{
gb->rtc_real.bytes[0] = time->tm_sec;
gb->rtc_real.bytes[1] = time->tm_min;
gb->rtc_real.bytes[2] = time->tm_hour;
gb->rtc_real.bytes[3] = time->tm_yday & 0xFF;
gb->rtc_real.bytes[4] = time->tm_yday >> 8;
}
#endif
enum gb_init_error_e gb_init(struct gb_s* gb,
uint8_t(*gb_rom_read)(struct gb_s*, const uint_fast32_t),
uint16_t(*gb_rom_read16)(struct gb_s*, const uint_fast32_t),
uint32_t(*gb_rom_read32)(struct gb_s*, const uint_fast32_t),
uint8_t(*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t),
void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t, const uint8_t),
void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t),
void* priv);
void gb_run_frame(struct gb_s *gb);
static inline void __gb_step_cpu(struct gb_s *gb);
void gb_reset(struct gb_s *gb);
#if ENABLE_LCD
void gb_init_lcd(struct gb_s *gb,
void (*lcd_draw_line)(struct gb_s *gb,
const uint8_t *pixels,
const uint_fast8_t line));
#endif
void gb_init_serial(struct gb_s *gb,
void (*gb_serial_tx)(struct gb_s*, const uint8_t),
enum gb_serial_rx_ret_e (*gb_serial_rx)(struct gb_s*,
uint8_t*));
int gb_get_save_size_s(struct gb_s *gb, size_t *ram_size);
uint_fast32_t gb_get_save_size(struct gb_s *gb);
uint8_t gb_colour_hash(struct gb_s *gb);
const char* gb_get_rom_name(struct gb_s* gb, char *title_str);
void gb_tick_rtc(struct gb_s *gb);
void gb_set_rtc(struct gb_s *gb, const struct tm * const time);
void gb_set_bootrom(struct gb_s *gb,
uint8_t (*gb_bootrom_read)(struct gb_s*, const uint_fast16_t));
void __gb_step_cpu_x(struct gb_s *gb)
{
uint8_t opcode;
uint_fast16_t inst_cycles;
static const uint8_t op_cycles[0x100] =
{
4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,
4,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,
8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,
8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
8,12,12,16,12,16, 8,16, 8,16,12, 8,12,24, 8,16,
8,12,12, 0,12,16, 8,16, 8,16,12, 0,12, 0, 8,16,
12,12,8, 0, 0,16, 8,16,16, 4,16, 0, 0, 0, 8,16,
12,12,8, 4, 0,16, 8,16,12, 8,16, 4, 0, 0, 8,16
};
static const uint_fast16_t TAC_CYCLES[4] = {1024, 16, 64, 256};
while(gb->gb_halt || (gb->gb_ime &&
gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & ANY_INTR))
{
gb->gb_halt = false;
if(!gb->gb_ime)
break;
gb->gb_ime = false;
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & VBLANK_INTR)
{
gb->cpu_reg.pc.reg = VBLANK_INTR_ADDR;
gb->hram_io[IO_IF] ^= VBLANK_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & LCDC_INTR)
{
gb->cpu_reg.pc.reg = LCDC_INTR_ADDR;
gb->hram_io[IO_IF] ^= LCDC_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & TIMER_INTR)
{
gb->cpu_reg.pc.reg = TIMER_INTR_ADDR;
gb->hram_io[IO_IF] ^= TIMER_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & SERIAL_INTR)
{
gb->cpu_reg.pc.reg = SERIAL_INTR_ADDR;
gb->hram_io[IO_IF] ^= SERIAL_INTR;
}
else if(gb->hram_io[IO_IF] & gb->hram_io[IO_IE] & CONTROL_INTR)
{
gb->cpu_reg.pc.reg = CONTROL_INTR_ADDR;
gb->hram_io[IO_IF] ^= CONTROL_INTR;
}
break;
}
opcode = __gb_read(gb, gb->cpu_reg.pc.reg++);
inst_cycles = op_cycles[opcode];
switch(opcode)
{
case 0x00:
break;
case 0x01:
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.bc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.pc.reg++);
#endif
break;
case 0x02:
__gb_write(gb, gb->cpu_reg.bc.reg, gb->cpu_reg.a);
break;
case 0x03:
gb->cpu_reg.bc.reg++;
break;
case 0x04:
WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0x05:
WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0x06:
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x07:
gb->cpu_reg.a = (gb->cpu_reg.a << 1) | (gb->cpu_reg.a >> 7);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a & 0x01);
break;
case 0x08:
{
#if WALNUT_GB_16_BIT_DISABLED
uint16_t temp = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t l = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t temp = WALNUT_GB_U8_TO_U16(h, l);
#endif
__gb_write(gb, temp++, gb->cpu_reg.sp.bytes.p);
__gb_write(gb, temp, gb->cpu_reg.sp.bytes.s);
break;
}
case 0x09:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.bc.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.bc.reg) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
break;
}
case 0x0A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.bc.reg);
break;
case 0x0B:
gb->cpu_reg.bc.reg--;
break;
case 0x0C:
WGB_INSTR_INC_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0x0D:
WGB_INSTR_DEC_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0x0E:
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x0F:
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 0x01;
gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (gb->cpu_reg.a << 7);
break;
case 0x10:
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode & gb->cgb.doubleSpeedPrep)
{
gb->cgb.doubleSpeedPrep = 0;
gb->cgb.doubleSpeed ^= 1;
}
#endif
break;
case 0x11:
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.de.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.pc.reg++);
#endif
break;
case 0x12:
__gb_write(gb, gb->cpu_reg.de.reg, gb->cpu_reg.a);
break;
case 0x13:
gb->cpu_reg.de.reg++;
break;
case 0x14:
WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.d);
break;
case 0x15:
WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.d);
break;
case 0x16:
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x17:
{
uint8_t temp = gb->cpu_reg.a;
gb->cpu_reg.a = (gb->cpu_reg.a << 1) | gb->cpu_reg.f.f_bits.c;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = (temp >> 7) & 0x01;
break;
}
case 0x18:
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
break;
}
case 0x19:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.de.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
(temp ^ gb->cpu_reg.hl.reg ^ gb->cpu_reg.de.reg) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
gb->cpu_reg.hl.reg = (temp & 0x0000FFFF);
break;
}
case 0x1A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.de.reg);
break;
case 0x1B:
gb->cpu_reg.de.reg--;
break;
case 0x1C:
WGB_INSTR_INC_R8(gb->cpu_reg.de.bytes.e);
break;
case 0x1D:
WGB_INSTR_DEC_R8(gb->cpu_reg.de.bytes.e);
break;
case 0x1E:
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x1F:
{
uint8_t temp = gb->cpu_reg.a;
gb->cpu_reg.a = gb->cpu_reg.a >> 1 | (gb->cpu_reg.f.f_bits.c << 7);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.c = temp & 0x1;
break;
}
case 0x20:
if(!gb->cpu_reg.f.f_bits.z)
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
break;
case 0x21:
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.hl.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.pc.reg++);
#endif
break;
case 0x22:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
gb->cpu_reg.hl.reg++;
break;
case 0x23:
gb->cpu_reg.hl.reg++;
break;
case 0x24:
WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0x25:
WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0x26:
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x27:
{
int16_t a = gb->cpu_reg.a;
if(gb->cpu_reg.f.f_bits.n)
{
if(gb->cpu_reg.f.f_bits.h)
a = (a - 0x06) & 0xFF;
if(gb->cpu_reg.f.f_bits.c)
a -= 0x60;
}
else
{
if(gb->cpu_reg.f.f_bits.h || (a & 0x0F) > 9)
a += 0x06;
if(gb->cpu_reg.f.f_bits.c || a > 0x9F)
a += 0x60;
}
if((a & 0x100) == 0x100)
gb->cpu_reg.f.f_bits.c = 1;
gb->cpu_reg.a = (uint8_t)a;
gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0);
gb->cpu_reg.f.f_bits.h = 0;
break;
}
case 0x28:
if(gb->cpu_reg.f.f_bits.z)
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
break;
case 0x29:
{
gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.hl.reg & 0x8000) > 0;
gb->cpu_reg.hl.reg <<= 1;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = (gb->cpu_reg.hl.reg & 0x1000) > 0;
break;
}
case 0x2A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg++);
break;
case 0x2B:
gb->cpu_reg.hl.reg--;
break;
case 0x2C:
WGB_INSTR_INC_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0x2D:
WGB_INSTR_DEC_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0x2E:
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x2F:
gb->cpu_reg.a = ~gb->cpu_reg.a;
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h = 1;
break;
case 0x30:
if(!gb->cpu_reg.f.f_bits.c)
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
break;
case 0x31:
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.sp.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
gb->cpu_reg.sp.bytes.p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.sp.bytes.s = __gb_read(gb, gb->cpu_reg.pc.reg++);
#endif
break;
case 0x32:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
gb->cpu_reg.hl.reg--;
break;
case 0x33:
gb->cpu_reg.sp.reg++;
break;
case 0x34:
{
uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
WGB_INSTR_INC_R8(temp);
__gb_write(gb, gb->cpu_reg.hl.reg, temp);
break;
}
case 0x35:
{
uint8_t temp = __gb_read(gb, gb->cpu_reg.hl.reg);
WGB_INSTR_DEC_R8(temp);
__gb_write(gb, gb->cpu_reg.hl.reg, temp);
break;
}
case 0x36:
__gb_write(gb, gb->cpu_reg.hl.reg, __gb_read(gb, gb->cpu_reg.pc.reg++));
break;
case 0x37:
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = 0;
gb->cpu_reg.f.f_bits.c = 1;
break;
case 0x38:
if(gb->cpu_reg.f.f_bits.c)
{
int8_t temp = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.reg += temp;
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg++;
break;
case 0x39:
{
uint_fast32_t temp = gb->cpu_reg.hl.reg + gb->cpu_reg.sp.reg;
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h =
((gb->cpu_reg.hl.reg & 0xFFF) + (gb->cpu_reg.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = temp & 0x10000 ? 1 : 0;
gb->cpu_reg.hl.reg = (uint16_t)temp;
break;
}
case 0x3A:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg--);
break;
case 0x3B:
gb->cpu_reg.sp.reg--;
break;
case 0x3C:
WGB_INSTR_INC_R8(gb->cpu_reg.a);
break;
case 0x3D:
WGB_INSTR_DEC_R8(gb->cpu_reg.a);
break;
case 0x3E:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.pc.reg++);
break;
case 0x3F:
gb->cpu_reg.f.f_bits.n = 0;
gb->cpu_reg.f.f_bits.h = 0;
gb->cpu_reg.f.f_bits.c = ~gb->cpu_reg.f.f_bits.c;
break;
case 0x40:
break;
case 0x41:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.bc.bytes.c;
break;
case 0x42:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.d;
break;
case 0x43:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.e;
break;
case 0x44:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.h;
break;
case 0x45:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.l;
break;
case 0x46:
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x47:
gb->cpu_reg.bc.bytes.b = gb->cpu_reg.a;
break;
case 0x48:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.bc.bytes.b;
break;
case 0x49:
break;
case 0x4A:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.d;
break;
case 0x4B:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.e;
break;
case 0x4C:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.h;
break;
case 0x4D:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.l;
break;
case 0x4E:
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x4F:
gb->cpu_reg.bc.bytes.c = gb->cpu_reg.a;
break;
case 0x50:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.b;
break;
case 0x51:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.c;
break;
case 0x52:
break;
case 0x53:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.de.bytes.e;
break;
case 0x54:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.h;
break;
case 0x55:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.l;
break;
case 0x56:
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x57:
gb->cpu_reg.de.bytes.d = gb->cpu_reg.a;
break;
case 0x58:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.b;
break;
case 0x59:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.c;
break;
case 0x5A:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.de.bytes.d;
break;
case 0x5B:
break;
case 0x5C:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.h;
break;
case 0x5D:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.l;
break;
case 0x5E:
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x5F:
gb->cpu_reg.de.bytes.e = gb->cpu_reg.a;
break;
case 0x60:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.b;
break;
case 0x61:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.c;
break;
case 0x62:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.d;
break;
case 0x63:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.e;
break;
case 0x64:
break;
case 0x65:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.hl.bytes.l;
break;
case 0x66:
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x67:
gb->cpu_reg.hl.bytes.h = gb->cpu_reg.a;
break;
case 0x68:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.b;
break;
case 0x69:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.c;
break;
case 0x6A:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.d;
break;
case 0x6B:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.e;
break;
case 0x6C:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.hl.bytes.h;
break;
case 0x6D:
break;
case 0x6E:
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x6F:
gb->cpu_reg.hl.bytes.l = gb->cpu_reg.a;
break;
case 0x70:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.b);
break;
case 0x71:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.c);
break;
case 0x72:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.d);
break;
case 0x73:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.e);
break;
case 0x74:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.h);
break;
case 0x75:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.l);
break;
case 0x76:
{
int_fast16_t halt_cycles = INT_FAST16_MAX;
gb->gb_halt = true;
if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
{
int serial_cycles = SERIAL_CYCLES -
gb->counter.serial_count;
if(serial_cycles < halt_cycles)
halt_cycles = serial_cycles;
}
if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
{
int tac_cycles = TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK] -
gb->counter.tima_count;
if(tac_cycles < halt_cycles)
halt_cycles = tac_cycles;
}
if((gb->hram_io[IO_LCDC] & LCDC_ENABLE))
{
int lcd_cycles;
if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_HBLANK)
{
lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN)
{
lcd_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW)
{
lcd_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else
{
lcd_cycles = LCD_LINE_CYCLES - gb->counter.lcd_count;
}
if(lcd_cycles < halt_cycles)
halt_cycles = lcd_cycles;
}
if(halt_cycles <= 0)
halt_cycles = 4;
inst_cycles = (uint_fast16_t)halt_cycles;
break;
}
case 0x77:
__gb_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a);
break;
case 0x78:
gb->cpu_reg.a = gb->cpu_reg.bc.bytes.b;
break;
case 0x79:
gb->cpu_reg.a = gb->cpu_reg.bc.bytes.c;
break;
case 0x7A:
gb->cpu_reg.a = gb->cpu_reg.de.bytes.d;
break;
case 0x7B:
gb->cpu_reg.a = gb->cpu_reg.de.bytes.e;
break;
case 0x7C:
gb->cpu_reg.a = gb->cpu_reg.hl.bytes.h;
break;
case 0x7D:
gb->cpu_reg.a = gb->cpu_reg.hl.bytes.l;
break;
case 0x7E:
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.hl.reg);
break;
case 0x7F:
break;
case 0x80:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, 0);
break;
case 0x81:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, 0);
break;
case 0x82:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, 0);
break;
case 0x83:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, 0);
break;
case 0x84:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, 0);
break;
case 0x85:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, 0);
break;
case 0x86:
WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
break;
case 0x87:
WGB_INSTR_ADC_R8(gb->cpu_reg.a, 0);
break;
case 0x88:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
break;
case 0x89:
WGB_INSTR_ADC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
break;
case 0x8A:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
break;
case 0x8B:
WGB_INSTR_ADC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
break;
case 0x8C:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
break;
case 0x8D:
WGB_INSTR_ADC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
break;
case 0x8E:
WGB_INSTR_ADC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
break;
case 0x8F:
WGB_INSTR_ADC_R8(gb->cpu_reg.a, gb->cpu_reg.f.f_bits.c);
break;
case 0x90:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, 0);
break;
case 0x91:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, 0);
break;
case 0x92:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, 0);
break;
case 0x93:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, 0);
break;
case 0x94:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, 0);
break;
case 0x95:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, 0);
break;
case 0x96:
WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), 0);
break;
case 0x97:
gb->cpu_reg.a = 0;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = 1;
gb->cpu_reg.f.f_bits.n = 1;
break;
case 0x98:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c);
break;
case 0x99:
WGB_INSTR_SBC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c);
break;
case 0x9A:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c);
break;
case 0x9B:
WGB_INSTR_SBC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c);
break;
case 0x9C:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c);
break;
case 0x9D:
WGB_INSTR_SBC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c);
break;
case 0x9E:
WGB_INSTR_SBC_R8(__gb_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c);
break;
case 0x9F:
gb->cpu_reg.a = gb->cpu_reg.f.f_bits.c ? 0xFF : 0x00;
gb->cpu_reg.f.f_bits.z = !gb->cpu_reg.f.f_bits.c;
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h = gb->cpu_reg.f.f_bits.c;
break;
case 0xA0:
WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0xA1:
WGB_INSTR_AND_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0xA2:
WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.d);
break;
case 0xA3:
WGB_INSTR_AND_R8(gb->cpu_reg.de.bytes.e);
break;
case 0xA4:
WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0xA5:
WGB_INSTR_AND_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0xA6:
WGB_INSTR_AND_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
break;
case 0xA7:
WGB_INSTR_AND_R8(gb->cpu_reg.a);
break;
case 0xA8:
WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0xA9:
WGB_INSTR_XOR_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0xAA:
WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.d);
break;
case 0xAB:
WGB_INSTR_XOR_R8(gb->cpu_reg.de.bytes.e);
break;
case 0xAC:
WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0xAD:
WGB_INSTR_XOR_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0xAE:
WGB_INSTR_XOR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
break;
case 0xAF:
WGB_INSTR_XOR_R8(gb->cpu_reg.a);
break;
case 0xB0:
WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0xB1:
WGB_INSTR_OR_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0xB2:
WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.d);
break;
case 0xB3:
WGB_INSTR_OR_R8(gb->cpu_reg.de.bytes.e);
break;
case 0xB4:
WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0xB5:
WGB_INSTR_OR_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0xB6:
WGB_INSTR_OR_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
break;
case 0xB7:
WGB_INSTR_OR_R8(gb->cpu_reg.a);
break;
case 0xB8:
WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.b);
break;
case 0xB9:
WGB_INSTR_CP_R8(gb->cpu_reg.bc.bytes.c);
break;
case 0xBA:
WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.d);
break;
case 0xBB:
WGB_INSTR_CP_R8(gb->cpu_reg.de.bytes.e);
break;
case 0xBC:
WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.h);
break;
case 0xBD:
WGB_INSTR_CP_R8(gb->cpu_reg.hl.bytes.l);
break;
case 0xBE:
WGB_INSTR_CP_R8(__gb_read(gb, gb->cpu_reg.hl.reg));
break;
case 0xBF:
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.z = 1;
gb->cpu_reg.f.f_bits.n = 1;
break;
case 0xC0:
if(!gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
break;
case 0xC1:
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.bc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.bc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.bc.bytes.b = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
break;
case 0xC2:
if(!gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xC3:
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
#else
{
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
}
#endif
break;
case 0xC4:
if(!gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_DISABLED
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = addr;
#else
uint8_t c, p;
c = __gb_read(gb, gb->cpu_reg.pc.reg++);
p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xC5:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.bc.bytes.b);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.bc.bytes.c);
break;
case 0xC6:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_ADC_R8(val, 0);
break;
}
case 0xC7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0000;
break;
case 0xC8:
if(gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
break;
case 0xC9:
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
break;
}
case 0xCA:
if(gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xCB:
inst_cycles = __gb_execute_cb(gb);
break;
case 0xCC:
if(gb->cpu_reg.f.f_bits.z)
{
#if WALNUT_GB_16_BIT_DISABLED
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
__gb_write(gb, --gb->cpu_reg.sp.reg, (gb->cpu_reg.pc.reg >> 8) & 0xFF);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg & 0xFF);
gb->cpu_reg.pc.reg = addr;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xCD:
#if WALNUT_GB_16_BIT_DISABLED
{
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
__gb_write(gb, --gb->cpu_reg.sp.reg, (gb->cpu_reg.pc.reg >> 8) & 0xFF);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg & 0xFF);
gb->cpu_reg.pc.reg = addr;
}
#else
{
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
}
#endif
break;
case 0xCE:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_ADC_R8(val, gb->cpu_reg.f.f_bits.c);
break;
}
case 0xCF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0008;
break;
case 0xD0:
if (!gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
break;
case 0xD1:
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.de.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.de.bytes.e = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.de.bytes.d = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
break;
case 0xD2:
if(!gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xD4:
if(!gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_DISABLED
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
__gb_write(gb, --gb->cpu_reg.sp.reg, (addr >> 8) & 0xFF);
__gb_write(gb, --gb->cpu_reg.sp.reg, addr & 0xFF);
gb->cpu_reg.pc.reg = addr;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xD5:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.de.bytes.d);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.de.bytes.e);
break;
case 0xD6:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t temp = gb->cpu_reg.a - val;
gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);
gb->cpu_reg.f.f_bits.n = 1;
gb->cpu_reg.f.f_bits.h =
(gb->cpu_reg.a ^ val ^ temp) & 0x10 ? 1 : 0;
gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;
gb->cpu_reg.a = (temp & 0xFF);
break;
}
case 0xD7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0010;
break;
case 0xD8:
if(gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
inst_cycles += 12;
}
break;
case 0xD9:
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.pc.bytes.c = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.pc.bytes.p = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
gb->gb_ime = true;
}
break;
case 0xDA:
if(gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.pc.reg = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 4;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xDC:
if(gb->cpu_reg.f.f_bits.c)
{
#if WALNUT_GB_16_BIT_DISABLED
uint16_t target = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
gb->cpu_reg.sp.reg -= 2;
__gb_write16(gb, gb->cpu_reg.sp.reg, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg = target;
#else
uint8_t c = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t p = __gb_read(gb, gb->cpu_reg.pc.reg++);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.bytes.c = c;
gb->cpu_reg.pc.bytes.p = p;
#endif
inst_cycles += 12;
}
else
gb->cpu_reg.pc.reg += 2;
break;
case 0xDE:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_SBC_R8(val, gb->cpu_reg.f.f_bits.c);
break;
}
case 0xDF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0018;
break;
case 0xE0:
__gb_write(gb, 0xFF00 | __gb_read(gb, gb->cpu_reg.pc.reg++),
gb->cpu_reg.a);
break;
case 0xE1:
#if WALNUT_GB_16_BIT_DISABLED
gb->cpu_reg.hl.reg = __gb_read16(gb, gb->cpu_reg.sp.reg);
gb->cpu_reg.sp.reg += 2;
#else
gb->cpu_reg.hl.bytes.l = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.hl.bytes.h = __gb_read(gb, gb->cpu_reg.sp.reg++);
#endif
break;
case 0xE2:
__gb_write(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a);
break;
case 0xE5:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.hl.bytes.h);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.hl.bytes.l);
break;
case 0xE6:
{
uint8_t temp = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_AND_R8(temp);
break;
}
case 0xE7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0020;
break;
case 0xE8:
{
int8_t offset = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF);
gb->cpu_reg.sp.reg += offset;
break;
}
case 0xE9:
gb->cpu_reg.pc.reg = gb->cpu_reg.hl.reg;
break;
case 0xEA:
{
#if WALNUT_GB_16_BIT_DISABLED
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t l = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
__gb_write(gb, addr, gb->cpu_reg.a);
break;
}
case 0xEE:
WGB_INSTR_XOR_R8(__gb_read(gb, gb->cpu_reg.pc.reg++));
break;
case 0xEF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0028;
break;
case 0xF0:
gb->cpu_reg.a =
__gb_read(gb, 0xFF00 | __gb_read(gb, gb->cpu_reg.pc.reg++));
break;
case 0xF1:
{
uint8_t temp_8 = __gb_read(gb, gb->cpu_reg.sp.reg++);
gb->cpu_reg.f.f_bits.z = (temp_8 >> 7) & 1;
gb->cpu_reg.f.f_bits.n = (temp_8 >> 6) & 1;
gb->cpu_reg.f.f_bits.h = (temp_8 >> 5) & 1;
gb->cpu_reg.f.f_bits.c = (temp_8 >> 4) & 1;
gb->cpu_reg.a = __gb_read(gb, gb->cpu_reg.sp.reg++);
break;
}
case 0xF2:
gb->cpu_reg.a = __gb_read(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c);
break;
case 0xF3:
gb->gb_ime = false;
break;
case 0xF5:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.a);
__gb_write(gb, --gb->cpu_reg.sp.reg,
gb->cpu_reg.f.f_bits.z << 7 | gb->cpu_reg.f.f_bits.n << 6 |
gb->cpu_reg.f.f_bits.h << 5 | gb->cpu_reg.f.f_bits.c << 4);
break;
case 0xF6:
WGB_INSTR_OR_R8(__gb_read(gb, gb->cpu_reg.pc.reg++));
break;
case 0xF7:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0030;
break;
case 0xF8:
{
int8_t offset = (int8_t) __gb_read(gb, gb->cpu_reg.pc.reg++);
gb->cpu_reg.hl.reg = gb->cpu_reg.sp.reg + offset;
gb->cpu_reg.f.reg = 0;
gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF) ? 1 : 0;
break;
}
case 0xF9:
gb->cpu_reg.sp.reg = gb->cpu_reg.hl.reg;
break;
case 0xFA:
{
#if WALNUT_GB_16_BIT_DISABLED
uint16_t addr = __gb_read16(gb, gb->cpu_reg.pc.reg);
gb->cpu_reg.pc.reg += 2;
#else
uint8_t l = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint8_t h = __gb_read(gb, gb->cpu_reg.pc.reg++);
uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
gb->cpu_reg.a = __gb_read(gb, addr);
break;
}
case 0xFB:
gb->gb_ime = true;
break;
case 0xFE:
{
uint8_t val = __gb_read(gb, gb->cpu_reg.pc.reg++);
WGB_INSTR_CP_R8(val);
break;
}
case 0xFF:
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
__gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
gb->cpu_reg.pc.reg = 0x0038;
break;
default:
(gb->gb_error)(gb, GB_INVALID_OPCODE, gb->cpu_reg.pc.reg - 1);
WGB_UNREACHABLE();
}
do
{
gb->counter.div_count += inst_cycles;
while(gb->counter.div_count >= DIV_CYCLES)
{
gb->hram_io[IO_DIV]++;
gb->counter.div_count -= DIV_CYCLES;
}
if(gb->mbc == 3 && (gb->rtc_real.reg.high & 0x40) == 0)
{
gb->counter.rtc_count += inst_cycles;
while(WGB_UNLIKELY(gb->counter.rtc_count >= RTC_CYCLES))
{
gb->counter.rtc_count -= RTC_CYCLES;
if(WGB_UNLIKELY(gb->rtc_real.reg.sec == 63))
{
gb->rtc_real.reg.sec = 0;
continue;
}
if(++gb->rtc_real.reg.sec != 60)
continue;
gb->rtc_real.reg.sec = 0;
if(gb->rtc_real.reg.min == 63)
{
gb->rtc_real.reg.min = 0;
continue;
}
if(++gb->rtc_real.reg.min != 60)
continue;
gb->rtc_real.reg.min = 0;
if(gb->rtc_real.reg.hour == 31)
{
gb->rtc_real.reg.hour = 0;
continue;
}
if(++gb->rtc_real.reg.hour != 24)
continue;
gb->rtc_real.reg.hour = 0;
if(++gb->rtc_real.reg.yday != 0)
continue;
if(gb->rtc_real.reg.high & 1)
gb->rtc_real.reg.high |= 0x80;
gb->rtc_real.reg.high ^= 1;
}
}
if(gb->hram_io[IO_SC] & SERIAL_SC_TX_START)
{
unsigned int serial_cycles = SERIAL_CYCLES_1KB;
if(gb->counter.serial_count == 0 &&
gb->gb_serial_tx != NULL)
(gb->gb_serial_tx)(gb, gb->hram_io[IO_SB]);
#if WALNUT_FULL_GBC_SUPPORT
if(gb->hram_io[IO_SC] & 0x3)
serial_cycles = SERIAL_CYCLES_32KB;
#endif
gb->counter.serial_count += inst_cycles;
if(gb->counter.serial_count >= serial_cycles)
{
uint8_t rx;
if(gb->gb_serial_rx != NULL &&
(gb->gb_serial_rx(gb, &rx) ==
GB_SERIAL_RX_SUCCESS))
{
gb->hram_io[IO_SB] = rx;
gb->hram_io[IO_SC] &= 0x01;
gb->hram_io[IO_IF] |= SERIAL_INTR;
}
else if(gb->hram_io[IO_SC] & SERIAL_SC_CLOCK_SRC)
{
gb->hram_io[IO_SB] = 0xFF;
gb->hram_io[IO_SC] &= 0x01;
gb->hram_io[IO_IF] |= SERIAL_INTR;
}
else
{
}
gb->counter.serial_count = 0;
}
}
if(gb->hram_io[IO_TAC] & IO_TAC_ENABLE_MASK)
{
gb->counter.tima_count += inst_cycles;
while(gb->counter.tima_count >=
TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK])
{
gb->counter.tima_count -=
TAC_CYCLES[gb->hram_io[IO_TAC] & IO_TAC_RATE_MASK];
if(++gb->hram_io[IO_TIMA] == 0)
{
gb->hram_io[IO_IF] |= TIMER_INTR;
gb->hram_io[IO_TIMA] = gb->hram_io[IO_TMA];
}
}
}
if(!(gb->hram_io[IO_LCDC] & LCDC_ENABLE))
{
gb->counter.lcd_off_count += inst_cycles;
if(gb->counter.lcd_off_count >= LCD_FRAME_CYCLES)
{
gb->counter.lcd_off_count -= LCD_FRAME_CYCLES;
gb->gb_frame = true;
}
continue;
}
#if WALNUT_FULL_GBC_SUPPORT
if (inst_cycles > 1) {
gb->counter.lcd_count += (inst_cycles >> gb->cgb.doubleSpeed);
} else {
#endif
gb->counter.lcd_count += inst_cycles;
#if WALNUT_FULL_GBC_SUPPORT
}
#endif
if(gb->counter.lcd_count >= LCD_LINE_CYCLES)
{
gb->counter.lcd_count -= LCD_LINE_CYCLES;
gb->hram_io[IO_LY] = gb->hram_io[IO_LY] + 1;
if (gb->hram_io[IO_LY] == LCD_VERT_LINES)
gb->hram_io[IO_LY] = 0;
if(gb->hram_io[IO_LY] == gb->hram_io[IO_LYC])
{
gb->hram_io[IO_STAT] |= STAT_LYC_COINC;
if(gb->hram_io[IO_STAT] & STAT_LYC_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
}
else
gb->hram_io[IO_STAT] &= 0xFB;
if(gb->hram_io[IO_LY] == LCD_HEIGHT)
{
gb->hram_io[IO_STAT] =
(gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_VBLANK;
gb->gb_frame = true;
gb->hram_io[IO_IF] |= VBLANK_INTR;
gb->lcd_blank = false;
if(gb->hram_io[IO_STAT] & STAT_MODE_1_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
#if ENABLE_LCD
if(gb->direct.frame_skip)
{
gb->display.frame_skip_count =
!gb->display.frame_skip_count;
}
if(gb->direct.interlace &&
(!gb->direct.frame_skip ||
gb->display.frame_skip_count))
{
gb->display.interlace_count =
!gb->display.interlace_count;
}
#endif
if(gb->gb_halt && !gb->hram_io[IO_IE])
break;
}
else if(gb->hram_io[IO_LY] < LCD_HEIGHT)
{
if(gb->hram_io[IO_LY] == 0)
{
gb->display.WY = gb->hram_io[IO_WY];
gb->display.window_clear = 0;
}
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_OAM_SCAN;
gb->counter.lcd_count = 0;
#if WALNUT_FULL_GBC_SUPPORT
if(gb->cgb.cgbMode && !gb->cgb.dmaActive && gb->cgb.dmaMode)
{
#if WALNUT_GB_32BIT_DMA
for (uint8_t i = 0; i < 0x10; i += 4)
{
uint32_t val = __gb_read32(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
__gb_write32(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
}
#elif WALNUT_GB_16BIT_DMA
for (uint8_t i = 0; i < 0x10; i += 2)
{
uint16_t val = __gb_read16(gb, (gb->cgb.dmaSource & 0xFFF0) + i);
__gb_write16(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i, val);
}
#else
for (uint8_t i = 0; i < 0x10; i++)
{
__gb_write(gb, ((gb->cgb.dmaDest & 0x1FF0) | 0x8000) + i,
__gb_read(gb, (gb->cgb.dmaSource & 0xFFF0) + i));
}
#endif
gb->cgb.dmaSource += 0x10;
gb->cgb.dmaDest += 0x10;
if(!(--gb->cgb.dmaSize)) {gb->cgb.dmaActive = 1;
}
}
#endif
if(gb->hram_io[IO_STAT] & STAT_MODE_2_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
inst_cycles = LCD_MODE2_OAM_SCAN_DURATION;
}
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_LCD_DRAW &&
gb->counter.lcd_count >= LCD_MODE3_LCD_DRAW_END)
{
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_HBLANK;
if(gb->hram_io[IO_STAT] & STAT_MODE_0_INTR)
gb->hram_io[IO_IF] |= LCDC_INTR;
if (gb->counter.lcd_count < LCD_MODE0_HBLANK_MAX_DRUATION)
inst_cycles = LCD_MODE0_HBLANK_MAX_DRUATION - gb->counter.lcd_count;
}
else if((gb->hram_io[IO_STAT] & STAT_MODE) == IO_STAT_MODE_OAM_SCAN &&
gb->counter.lcd_count >= LCD_MODE2_OAM_SCAN_END)
{
gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | IO_STAT_MODE_LCD_DRAW;
#if ENABLE_LCD
if(!gb->lcd_blank)
__gb_draw_line(gb);
#endif
if (gb->counter.lcd_count < LCD_MODE3_LCD_DRAW_MIN_DURATION)
inst_cycles = LCD_MODE3_LCD_DRAW_MIN_DURATION - gb->counter.lcd_count;
}
} while(gb->gb_halt && (gb->hram_io[IO_IF] & gb->hram_io[IO_IE]) == 0);
}
#undef WALNUT_GB_CPUFLAG_MASK_CARRY
#undef WALNUT_GB_CPUFLAG_MASK_HALFC
#undef WALNUT_GB_CPUFLAG_MASK_ARITH
#undef WALNUT_GB_CPUFLAG_MASK_ZERO
#undef WALNUT_GB_CPUFLAG_BIT_CARRY
#undef WALNUT_GB_CPUFLAG_BIT_HALFC
#undef WALNUT_GB_CPUFLAG_BIT_ARITH
#undef WALNUT_GB_CPUFLAG_BIT_ZERO
#undef WGB_SET_CARRY
#undef WGB_SET_HALFC
#undef WGB_SET_ARITH
#undef WGB_SET_ZERO
#undef WGB_GET_CARRY
#undef WGB_GET_HALFC
#undef WGB_GET_ARITH
#undef WGB_GET_ZERO
#endif
