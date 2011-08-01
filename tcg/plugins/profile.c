/*
 * TCG plugin for QEMU: count the number of executed instructions per
 *                      function.
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
#include <pthread.h>

#include "tcg-plugin.h"
#include "disas.h"

#include <glib.h>

static GHashTable *hash;

typedef struct {
    uint64_t size;
    uint64_t icount;
} HashEntry;

static void tb_helper_func(TCGPluginInterface *tpi, uint64_t address,
                           TPIHelperInfo info, uint64_t data1, uint64_t data2)
{
    HashEntry *hash_entry = (HashEntry *)(uintptr_t)data1;
    hash_entry->size   += info.size;
    hash_entry->icount += info.icount;
}

static void tb_helper_data(TCGPluginInterface *tpi, CPUState *env,
                           TranslationBlock *tb, uint64_t *data1,
                           uint64_t *data2)
{
    const char *symbol = lookup_symbol(tb->pc);
    HashEntry *hash_entry;

    if (symbol[0] == '\0')
        symbol = "/unknown";

    hash_entry = g_hash_table_lookup(hash, symbol);
    if (!hash_entry) {
        hash_entry = g_new0(HashEntry, 1);
        g_hash_table_insert(hash, g_strdup(symbol), hash_entry);
    }

    *data1 = (uintptr_t)hash_entry;
}

/**********************************************************************
 * Pretty printing.
 */

static size_t symbol_length   = 0;
static size_t nb_bytes_length = 0;
static size_t nb_instr_length = 0;
static char format[1024] = "%-30s: %10" PRIu64 " %10" PRIu64 "\n";

static unsigned int ilog10(uint64_t value)
{
    unsigned int result = 0;
    do { result++; } while(value /= 10);
    return result;
}

static void compute_entry_length(gchar *symbol, HashEntry *hash_entry)
{
    symbol_length   = MAX(symbol_length, strlen(symbol));
    nb_bytes_length = MAX(nb_bytes_length, ilog10(hash_entry->size));
    nb_instr_length = MAX(nb_instr_length, ilog10(hash_entry->icount));
}

static void print_entry(gchar *symbol, HashEntry *hash_entry, FILE *output)
{
    fprintf(output, format, symbol, hash_entry->size, hash_entry->icount);
}

static void cpus_stopped(TCGPluginInterface *tpi)
{
    size_t line_length = 0;

    symbol_length   = strlen("SYMBOL");
    nb_bytes_length = strlen("#BYTES");
    nb_instr_length = strlen("#INSTR");
    g_hash_table_foreach(hash, (GHFunc)compute_entry_length, NULL);

    snprintf(format, sizeof(format), "%%-%zus | %%%zus | %%%zus\n",
             symbol_length, nb_bytes_length, nb_instr_length);
    fprintf(tpi->output, format, "SYMBOL", "#BYTES", "#INSTR");

    line_length = symbol_length + 3 + nb_bytes_length + 3 + nb_instr_length;
    do { fprintf(tpi->output, "-"); } while (line_length--);
    fprintf(tpi->output, "\n");

    snprintf(format, sizeof(format), "%%-%zus | %%%zu" PRIu64 " | %%%zu" PRIu64 "\n",
             symbol_length, nb_bytes_length, nb_instr_length);

    g_hash_table_foreach(hash, (GHFunc)print_entry, tpi->output);
}

/**********************************************************************/

void tpi_init(TCGPluginInterface *tpi)
{
    TPI_INIT_VERSION_GENERIC(*tpi);

    tpi->tb_helper_func = tb_helper_func;
    tpi->tb_helper_data = tb_helper_data;
    tpi->cpus_stopped = cpus_stopped;

    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}
