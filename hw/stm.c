/*
 * QEMU SYSTEM TRACE MODULE EMULATION
 *
 * Copyright (c) 2011 ST-Microelectronics
 *
 * $Author: Marc Titinger <marc.titinger@st.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/*
 * simply add those lines to the board model init routine:
 *
  #define STM_STIH415_BASE	0xFD4D0000
     sysbus_create_varargs("stm_trace",STM_STIH415_BASE, NULL);
 *
 */

#include "sysbus.h"
#include "trace.h"

/* u8500 STM spec. */
#define STM_STIH415_CR_OFF	0x000
#define STM_STIH415_MMC_OFF	0x008
#define STM_STIH415_TER_OFF	0x010
#define STM_STIH415_ID0_OFF	0xFC0
#define STM_STIH415_ID1_OFF	0xFC8
#define STM_STIH415_ID2_OFF	0xFD0

#define STM_CR	0
#define STM_MMC	1
#define STM_TER	2
#define STM_ID0	3
#define STM_ID1	4
#define STM_ID2	5

#define STM_STIH415_LAST	6

/*FIXME: hardcoded for now*/
#define STM_REGS	STM_STIH415_LAST


typedef struct STMState
{
  SysBusDevice busdev;
  uint32_t reg_val[STM_REGS]; /*values*/
  uint32_t reg_off[STM_REGS]; /*offsets*/
  uint32_t reg_wmsk[STM_REGS]; /*write mask*/
 } STMState;


static void
stm_STIH415_reset (DeviceState * d)
{
  STMState *s = container_of (d, STMState, busdev.qdev);

  /* reset values */
  s->reg_val[STM_CR]  = 0x000;
  s->reg_val[STM_MMC] = 0x020;
  s->reg_val[STM_TER] = 0x23D;
  s->reg_val[STM_ID0]  = 0x0ED;
  s->reg_val[STM_ID1]  = 0x038;
  s->reg_val[STM_ID2]  = 0x00D;

  /* offsets for this IP */
  s->reg_off[STM_CR]  = STM_STIH415_CR_OFF;
  s->reg_off[STM_MMC] = STM_STIH415_MMC_OFF;
  s->reg_off[STM_TER] = STM_STIH415_TER_OFF;
  s->reg_off[STM_ID0] = STM_STIH415_ID0_OFF;
  s->reg_off[STM_ID1] = STM_STIH415_ID1_OFF;
  s->reg_off[STM_ID2] = STM_STIH415_ID2_OFF;

  /* write masks for this IP*/
  s->reg_wmsk[STM_CR]  = 0x1FF;//fixme
  s->reg_wmsk[STM_MMC] = 0x03F;//fixme
  s->reg_wmsk[STM_TER] = 0x2FF;//fixme
  s->reg_wmsk[STM_ID0] = 0;
  s->reg_wmsk[STM_ID1] = 0;
  s->reg_wmsk[STM_ID2] = 0;
}

static uint32_t
stm_mem_r_REG32 (void *opaque, target_phys_addr_t addr)
{
  int i;
  STMState *s = opaque;

  for (i = 0; i < STM_REGS; i++)
  {  if (s->reg_off[i] == addr) {
	  //printf("r32 on REG @%x = 0x%08x\n", addr, val);
	 return s->reg_val[i];
  }
  }
  return 0xdeadbeef;
}

/*todo: implement reading from the channel ranges*/
static uint32_t
stm_mem_read_nil (void *opaque, target_phys_addr_t addr)
{
  return 0xdeadbeef;
}

static void
stm_mem_w_REG32 (void *opaque, target_phys_addr_t addr, uint32_t val)
{
  STMState *s = opaque;
  int i, found = 0;

  //printf("w32 on REG @%x = 0x%08x\n", addr, val);

  for (i = 0; i < STM_REGS; i++)
	  if (s->reg_off[i] == addr) {
	  found = 1;
	  break;
  	  }

  if (found) {
	  s->reg_val[i] &= ~s->reg_wmsk[i];
	  s->reg_val[i] |= (s->reg_wmsk[i] & val) ;
  }
}

/*todo: implement writing to MASTER0 */
static void
stm_mem_w_MAO_D32 (void *opaque, target_phys_addr_t addr, uint32_t val)
{
	printf("D32 on MA0 @%x = 0x%08x\n", addr, val);
}

/*todo: implement writing to MASTER1 */
static void
stm_mem_w_MA1_D32 (void *opaque, target_phys_addr_t addr, uint32_t val)
{
	printf("D32 on MA1 @%x = 0x%08x\n", addr, val);
}

/*todo: implement writing to MASTER0 */
static void
stm_mem_w_MAO_D16 (void *opaque, target_phys_addr_t addr, uint32_t val)
{
	printf("D16 on MA0 @%x = 0x%08x\n", addr, val);
}

/*todo: implement writing to MASTER1 */
static void
stm_mem_w_MA1_D16 (void *opaque, target_phys_addr_t addr, uint32_t val)
{
	printf("D16 on MA1 @%x = 0x%08x\n", addr, val);
}

/*todo: implement writing to MASTER0 */
static void
stm_mem_w_MAO_D8 (void *opaque, target_phys_addr_t addr, uint32_t val)
{
	printf("D8 on MA0 @%x = 0x%08x\n", addr, val);
}

/*todo: implement writing to MASTER1 */
static void
stm_mem_w_MA1_D8 (void *opaque, target_phys_addr_t addr, uint32_t val)
{
	printf("D8 on MA1 @%x = 0x%08x\n", addr, val);
}

/* register MASTER0 R/W handlers
 **/
static CPUReadMemoryFunc *const stm_mem_r_MA0[3] = {
		stm_mem_read_nil,  stm_mem_read_nil,  stm_mem_read_nil,
};
static CPUWriteMemoryFunc *const stm_mem_w_MA0[3] = {
		stm_mem_w_MAO_D8,  stm_mem_w_MAO_D16,  stm_mem_w_MAO_D32,
};

/* register MASTER1 R/W handlers
 **/
static CPUWriteMemoryFunc *const stm_mem_w_MA1[3] = {
		stm_mem_w_MA1_D8,  stm_mem_w_MA1_D16,  stm_mem_w_MA1_D32,
};

/* register range R/W handlers
 **/
static CPUReadMemoryFunc *const stm_mem_r_REG[3] = {
	stm_mem_r_REG32,  stm_mem_r_REG32,  stm_mem_r_REG32,
};

static CPUWriteMemoryFunc *const stm_mem_w_REG[3] = {
	stm_mem_w_REG32,  stm_mem_w_REG32,  stm_mem_w_REG32,
};


static const VMStateDescription vmstate_stm_trace = {
  .name = "stm_trace",
  .version_id = 1,
  .minimum_version_id = 1,
  .minimum_version_id_old = 1,
  .fields = (VMStateField[]){
		VMSTATE_UINT32_ARRAY (reg_val, STMState, STM_REGS),
		VMSTATE_UINT32_ARRAY (reg_off, STMState, STM_REGS),
		VMSTATE_UINT32_ARRAY (reg_wmsk, STMState, STM_REGS),
		VMSTATE_END_OF_LIST ()}
};

static int
stm_trace_STIH415_init (SysBusDevice * dev)
{
  int io_ma0, io_ma1, io_reg;
  STMState *s = FROM_SYSBUS (STMState, dev);

  io_ma0 = cpu_register_io_memory (stm_mem_r_MA0, stm_mem_w_MA0, s,
			       DEVICE_NATIVE_ENDIAN);

  io_ma1 = cpu_register_io_memory (stm_mem_r_MA0, stm_mem_w_MA1, s,
			       DEVICE_NATIVE_ENDIAN);

  io_reg = cpu_register_io_memory (stm_mem_r_REG, stm_mem_w_REG, s,
			       DEVICE_NATIVE_ENDIAN);

  /* todo get base reg from realview.c
   */
  sysbus_init_mmio (dev, 4096, io_ma0);
  sysbus_init_mmio (dev, 4096, io_ma1);
  sysbus_init_mmio (dev, 4096, io_reg);

  sysbus_mmio_map (dev, 0, 0xFD4D0000); /*write_master0*/
  sysbus_mmio_map (dev, 1, 0xFD4D1000); /*write_master1*/
  sysbus_mmio_map (dev, 2, 0xFD4DF000);	/*write_regs*/

  return 0;
}

static SysBusDeviceInfo stm_trace_info = {
  .init = stm_trace_STIH415_init,
  .qdev.name = "stm_trace",
  .qdev.size = sizeof (STMState),
  .qdev.vmsd = &vmstate_stm_trace,
  .qdev.reset = stm_STIH415_reset,
  .qdev.props = (Property[]){
			     {.name = NULL}
			     }
};

static void
stm_trace_register_devices (void)
{
  sysbus_register_withprop (&stm_trace_info);
}

device_init (stm_trace_register_devices)
