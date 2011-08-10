/*
 * TCG plugin for QEMU: for each executed block, print its address and
 *                      the name of the function if available.
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
#include "disas.h"

static void tb_helper_func(TCGPluginInterface *tpi, uint64_t address,
                           TPIHelperInfo info, uint64_t data1, uint64_t data2)
{
    const char *symbol = (const char *)(uintptr_t)data1;

    if (!info.icount)
        return;

    fprintf(tpi->output, "CPU #%" PRIu32 " - 0x%016" PRIx64 " [%" PRIu32 "]: %" PRIu32 " instruction(s) in '%s'\n",
            info.cpu_index, address, info.size, info.icount, symbol[0] != '\0' ? symbol : "unknown");
}

static void tb_helper_data(TCGPluginInterface *tpi, CPUState *env,
                           TranslationBlock *tb, uint64_t *data1,
                           uint64_t *data2)
{
    const char *symbol = lookup_symbol(tb->pc);
    *data1 = (uintptr_t)symbol;
}

void tpi_init(TCGPluginInterface *tpi)
{
    TPI_INIT_VERSION(*tpi);
    tpi->tb_helper_func = tb_helper_func;
    tpi->tb_helper_data = tb_helper_data;
}
