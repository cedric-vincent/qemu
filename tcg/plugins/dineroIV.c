/*
 * TCG plugin for QEMU: provide memory references for Dinero IV (a
 *                      cache simulator)
 *
 * Copyright (C) 2011 STMicroelectronics
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "tcg-op.h"
#include "def-helper.h"
#include "tcg-plugin.h"

static FILE *output;

static void after_exec_opc(uint64_t info_, uint64_t address, uint64_t value, uint64_t pc)
{
    TPIHelperInfo info = *(TPIHelperInfo *)&info_;

    if (info.type == 'i') {
        address = pc;
        value = 0;
    }

    fprintf(output, "%c 0x%016" PRIx64 " 0x%08" PRIx32 " (0x%016" PRIx64 ") CPU #%" PRIu32 " 0x%016" PRIx64 "\n",
            info.type, address, info.size, value, info.cpu_index, pc);
}

static void gen_helper(const TCGPluginInterface *tpi, TCGArg *opargs, uint64_t pc, TPIHelperInfo info);

static void after_gen_opc(const TCGPluginInterface *tpi, const TPIOpCode *tpi_opcode)
{
    TPIHelperInfo info;

#define MEMACCESS(type_, size_) do {                            \
        info.type = type_;                                      \
        info.size = size_;                                      \
        info.cpu_index = 0; /* tpi_opcode->cpu_index NYI */     \
    } while (0);

    switch (tpi_opcode->name) {
    case INDEX_op_qemu_ld8s:
    case INDEX_op_qemu_ld8u:
        MEMACCESS('r', 1);
        break;

    case INDEX_op_qemu_ld16s:
    case INDEX_op_qemu_ld16u:
        MEMACCESS('r', 2);
        break;

    case INDEX_op_qemu_ld32:
#if TCG_TARGET_REG_BITS == 64
    case INDEX_op_qemu_ld32s:
    case INDEX_op_qemu_ld32u:
#endif
        MEMACCESS('r', 4);
        break;

    case INDEX_op_qemu_ld64:
        MEMACCESS('r', 8);
        break;

    case INDEX_op_qemu_st8:
        MEMACCESS('w', 1);
        break;

    case INDEX_op_qemu_st16:
        MEMACCESS('w', 2);
        break;

    case INDEX_op_qemu_st32:
        MEMACCESS('w', 4);
        break;

    case INDEX_op_qemu_st64:
        MEMACCESS('w', 8);
        break;

    default:
        return;
    }

    gen_helper(tpi, tpi_opcode->opargs, tpi_opcode->pc, info);
}

static void gen_helper(const TCGPluginInterface *tpi, TCGArg *opargs, uint64_t pc, TPIHelperInfo info)
{
    int sizemask = 0;
    TCGArg args[4];

    TCGv_i64 tcgv_info = tcg_const_i64(*(uint64_t *)&info);
    TCGv_i64 tcgv_pc   = tcg_const_i64(pc);

    args[0] = GET_TCGV_I64(tcgv_info);
    args[1] = opargs ? opargs[1] : 0;
    args[2] = opargs ? opargs[0] : 0;
    args[3] = GET_TCGV_I64(tcgv_pc);

    dh_sizemask(void, 0);
    dh_sizemask(i64, 1);
    dh_sizemask(i64, 2);
    dh_sizemask(i64, 3);
    dh_sizemask(i64, 4);

    tcg_gen_helperN(after_exec_opc, TCG_CALL_CONST, sizemask, TCG_CALL_DUMMY_ARG, 4, args);

    tcg_temp_free_i64(tcgv_pc);
    tcg_temp_free_i64(tcgv_info);
}

static void decode_instr(const TCGPluginInterface *tpi, uint64_t pc)
{
    TPIHelperInfo info;

#if defined(TARGET_SH4)
    MEMACCESS('i', 2);
#elif defined(TARGET_ARM)
    MEMACCESS('i', ARM_TBFLAG_THUMB(tpi->tb->flags) ? 2 : 4);
#else
    MEMACCESS('i', 0);
#endif

    gen_helper(tpi, NULL, pc, info);
}

void tpi_init(TCGPluginInterface *tpi)
{
    TPI_INIT_VERSION(*tpi);
    tpi->after_gen_opc = after_gen_opc;
    tpi->decode_instr  = decode_instr;
    output = tpi->output;

#if !defined(TARGET_SH4) && !defined(TARGET_ARM)
    fprintf(output, "# WARNING: instruction cache simulation NYI");
#endif
}
