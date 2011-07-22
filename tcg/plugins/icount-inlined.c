/*
 * TCG plugin for QEMU: count the number of executed instructions per
 *                      CPU.  This plugin is *not* thread-safe!
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

#include "tcg-plugin.h"
#include "tcg-op.h"

static TCGArg **icount_total_args;
static uint64_t *icount_total;

static void cpus_stopped(TCGPluginInterface *tpi)
{
    unsigned int i;
    for (i = 0; i < tpi->nb_cpus; i++) {
        fprintf(tpi->output,
                "Number of executed instructions on CPU #%d = %" PRIu64 "\n",
                i, icount_total[i]);
    }
}

/* This function generates code which is *not* thread-safe!  */
static void before_icg(TCGPluginInterface *tpi, CPUState *env, TranslationBlock *tb)
{
    TCGv_ptr icount_ptr;
    TCGv_i64 icount_tmp;
    TCGv_i32 tb_icount32;
    TCGv_i64 tb_icount64;

    /* icount_ptr = &icount */
    icount_ptr = tcg_const_ptr((tcg_target_long)&icount_total[env->cpu_index]);

    /* icount_tmp = *icount_ptr */
    icount_tmp = tcg_temp_new_i64();
    tcg_gen_ld_i64(icount_tmp, icount_ptr, 0);

    /* icount_args = &tb_icount32 */
    /* tb_icount32 = fixup(tb->icount) */
    icount_total_args[env->cpu_index] = gen_opparam_ptr + 1;
    tb_icount32 = tcg_const_i32(0);

    /* tb_icount64 = (int64_t)tb_icount32 */
    tb_icount64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(tb_icount64, tb_icount32);

    /* icount_tmp += tb_icount64 */
    tcg_gen_add_i64(icount_tmp, icount_tmp, tb_icount64);

    /* *icount_ptr = icount_tmp */
    tcg_gen_st_i64(icount_tmp, icount_ptr, 0);

    tcg_temp_free_i64(tb_icount64);
    tcg_temp_free_i32(tb_icount32);
    tcg_temp_free_i64(icount_tmp);
    tcg_temp_free_ptr(icount_ptr);
}

static void after_icg(TCGPluginInterface *tpi, CPUState *env, TranslationBlock *tb)
{
    /* Patch parameter value.  */
    *icount_total_args[env->cpu_index] = tb->icount;
}

void tpi_init(TCGPluginInterface *tpi)
{
    TPI_INIT_VERSION(*tpi);

    tpi->cpus_stopped = cpus_stopped;
    tpi->before_icg   = before_icg;
    tpi->after_icg    = after_icg;

    icount_total = qemu_mallocz(tpi->nb_cpus * sizeof(uint64_t));
    icount_total_args = qemu_mallocz(tpi->nb_cpus * sizeof(TCGArg *));
}
