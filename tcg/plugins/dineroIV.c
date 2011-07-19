/*
 * TCG plugin for QEMU: provide instruction memory references for
 *                      Dinero IV (a cache simulator)
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

static void tb_helper_func(TCGPluginInterface *tpi, uint64_t address,
                           TPIHelperInfo info, uint64_t data1, uint64_t data2)
{
    if (!info.size)
        return;

    /* Compute the size of an instruction; for variable-length
     * instruction (VLI) set CPU, it's the mean instruction size for
     * this basic block.  */
    uint32_t instruction_size = info.size / info.icount;
    uint32_t current_size = 0;
    do {
        /* Note: chunk_size != instruction_size only for the last
         * "instruction" of a VLI set CPU.  */
        uint32_t chunk_size = MIN(instruction_size,
                                  info.size - current_size);

        fprintf(tpi->output,
                "%c 0x%016" PRIx64 " 0x%08" PRIx32 " CPU #%" PRIu32 "\n",
                'i', address, chunk_size, info.cpu_index);

        current_size += chunk_size;
        address += chunk_size;
    } while (current_size < info.size);
}

void tpi_init(TCGPluginInterface *tpi)
{
    TPI_INIT_VERSION_GENERIC(*tpi);
    tpi->tb_helper_func = tb_helper_func;
}
