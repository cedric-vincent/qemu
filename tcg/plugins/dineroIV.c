/*
 * TCG plugin for QEMU: wrapper for Dinero IV (a cache simulator)
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

#define D4ADDR uint64_t
#include "d4-7/d4.h"
#include "d4-7/cmdd4.h"
#include "d4-7/cmdargs.h"

static FILE *output;

static d4cache *instr_cache, *data_cache;

static void after_exec_opc(uint64_t info_, uint64_t address, uint64_t value, uint64_t pc)
{
    TPIHelperInfo info = *(TPIHelperInfo *)&info_;
    d4memref memref;

    switch (info.type) {
    case 'i':
        address = pc;
        value = 0;

	memref.address    = pc;
	memref.accesstype = D4XINSTRN;
	memref.size       = (unsigned short) info.size;
        d4ref(instr_cache, memref);
        break;

    case 'r':
	memref.address    = address;
	memref.accesstype = D4XREAD;
	memref.size       = (unsigned short) info.size;
        d4ref(data_cache, memref);
        break;

    case 'w':
	memref.address    = address;
	memref.accesstype = D4XWRITE;
	memref.size       = (unsigned short) info.size;
        d4ref(data_cache, memref);
        break;
    }

#if 0
    fprintf(output, "%c 0x%016" PRIx64 " 0x%08" PRIx32 " (0x%016" PRIx64 ") CPU #%" PRIu32 " 0x%016" PRIx64 "\n",
            info.type, address, info.size, value, info.cpu_index, pc);
#endif
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

extern void dostats (void);
extern void doargs (int, char **);

static void cpus_stopped(const TCGPluginInterface *tpi)
{
    d4memref memref;
    FILE *saved_stdout = stdout;

    /* Flush the data cache.  */
    memref.accesstype = D4XCOPYB;
    memref.address = 0;
    memref.size = 0;
    d4ref(data_cache, memref);

    stdout = output;
    dostats();
    stdout = saved_stdout;

    fprintf(output, "---Execution complete.\n");
}

void tpi_init(TCGPluginInterface *tpi)
{
    int i, argc;
    char **argv;
    char *cmdline;

    TPI_INIT_VERSION(*tpi);
    output = tpi->output;

    tpi->after_gen_opc = after_gen_opc;
    tpi->decode_instr  = decode_instr;
    tpi->cpus_stopped  = cpus_stopped;

#if !defined(TARGET_SH4) && !defined(TARGET_ARM)
    fprintf(output, "# WARNING: instruction cache simulation NYI");
#endif

    cmdline = getenv("DINEROIV_CMDLINE");
    if (cmdline == NULL) {
        cmdline = g_strdup("-l1-isize 16k -l1-dsize 8192 -l1-ibsize 32 -l1-dbsize 16");
        fprintf(output, "# WARNING: using default Dineroiv command-line: %s\n", cmdline);
        fprintf(output, "# INFO: use the DINEROIV_CMDLINE environment variable to specify a command-line\n");
    }

    /* Create a valid argv[] for Dineroiv.  */
    argv = g_malloc0(2 * sizeof(char *));
    argv[0] = "tcg-plugin-dineroIV";
    argv[1] = cmdline;
    argc = 2;

    for (i = 0; cmdline[i] != '\0'; i++) {
        if (cmdline[i] == ' ') {
            cmdline[i] = '\0';
            argv = g_realloc(argv, (argc + 1) * sizeof(char *));
            argv[argc++] = cmdline + i + 1;
        }
    }

    doargs(argc, argv);
    verify_options();
    initialize_caches(&instr_cache, &data_cache);

    if (data_cache == NULL)
        data_cache = instr_cache;

    fprintf(output, "---Dinero IV cache simulator, version %s\n", D4VERSION);
    fprintf(output, "---Written by Jan Edler and Mark D. Hill\n");
    fprintf(output, "---Copyright (C) 1997 NEC Research Institute, Inc. and Mark D. Hill.\n");
    fprintf(output, "---All rights reserved.\n");
    fprintf(output, "---Copyright (C) 1985, 1989 Mark D. Hill.  All rights reserved.\n");
    fprintf(output, "---See -copyright option for details\n");
}
