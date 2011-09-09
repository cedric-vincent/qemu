/*
 * QEMU TCG plugin support.
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

#ifndef TCG_PLUGIN_H
#define TCG_PLUGIN_H

#include "qemu-common.h"
#include "exec-all.h"

/***********************************************************************
 * Hooks inserted into QEMU here and there.
 */

#ifdef CONFIG_TCG_PLUGIN
    bool tcg_plugin_enabled(void);
    void tcg_plugin_load(const char *dso);
    void tcg_plugin_cpus_stopped(void);
    void tcg_plugin_before_icg(CPUState *env, TranslationBlock *tb);
    void tcg_plugin_after_icg(CPUState *env, TranslationBlock *tb);
#else
#   define tcg_plugin_enabled() false
#   define tcg_plugin_load(dso)
#   define tcg_plugin_cpus_stopped()
#   define tcg_plugin_before_icg(env, tb)
#   define tcg_plugin_after_icg(env, tb)
#endif /* !CONFIG_INSTRUMENTATION */

/***********************************************************************
 * TCG plugin interface.
 */

/* This structure shall be 64 bits, see call_tb_helper_code() for
 * details.  */
typedef struct
{
    uint16_t cpu_index;
    uint16_t size;
    uint32_t icount;
} __attribute__((__packed__)) TPIHelperInfo;

struct TCGPluginInterface;
typedef struct TCGPluginInterface TCGPluginInterface;

typedef void (* tpi_cpus_stopped_t)(const TCGPluginInterface *tpi);

typedef void (* tpi_before_icg_t)(const TCGPluginInterface *tpi);

typedef void (* tpi_after_icg_t)(const TCGPluginInterface *tpi);

typedef void (* tpi_tb_helper_code_t)(const TCGPluginInterface *tpi,
                                      TPIHelperInfo info, uint64_t address,
                                      uint64_t data1, uint64_t data2);

typedef void (* tpi_tb_helper_data_t)(const TCGPluginInterface *tpi,
                                      TPIHelperInfo info, uint64_t address,
                                      uint64_t *data1, uint64_t *data2);

#define TPI_VERSION 2
struct TCGPluginInterface
{
    /* Compatibility information.  */
    int version;
    const char *guest;
    const char *mode;
    size_t sizeof_CPUState;
    size_t sizeof_TranslationBlock;

    /* Common parameters.  */
    int nb_cpus;
    FILE *output;
    uint64_t low_pc;
    uint64_t high_pc;
    bool verbose;

    /* Parameters for non-generic plugins.  */
    bool is_generic;
    const CPUState *env;
    const TranslationBlock *tb;

    /* Plugin's callbacks.  */
    tpi_cpus_stopped_t cpus_stopped;
    tpi_before_icg_t   before_icg;
    tpi_after_icg_t    after_icg;
    tpi_tb_helper_code_t tb_helper_code;
    tpi_tb_helper_data_t tb_helper_data;
};

#define TPI_INIT_VERSION(tpi) do {                                     \
        (tpi).version = TPI_VERSION;                                   \
        (tpi).guest   = TARGET_ARCH;                                   \
        (tpi).mode    = EMULATION_MODE;                                \
        (tpi).sizeof_CPUState = sizeof(CPUState);                      \
        (tpi).sizeof_TranslationBlock = sizeof(TranslationBlock);      \
    } while (0);

#define TPI_INIT_VERSION_GENERIC(tpi) do {                             \
        (tpi).version = TPI_VERSION;                                   \
        (tpi).guest   = "any";                                         \
        (tpi).mode    = "any";                                         \
        (tpi).sizeof_CPUState = 0;                                     \
        (tpi).sizeof_TranslationBlock = 0;                             \
    } while (0);

typedef void (* tpi_init_t)(TCGPluginInterface *tpi);
void tpi_init(TCGPluginInterface *tpi);

#endif /* TCG_PLUGIN_H */
