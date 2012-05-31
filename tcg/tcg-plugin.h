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

#ifndef TCG_TARGET_REG_BITS
#include "tcg.h"
#endif

#if TARGET_LONG_BITS == 32
#define MAKE_TCGV MAKE_TCGV_I32
#else
#define MAKE_TCGV MAKE_TCGV_I64
#endif

/***********************************************************************
 * Hooks inserted into QEMU here and there.
 */

#ifdef CONFIG_TCG_PLUGIN
    bool tcg_plugin_enabled(void);
    void tcg_plugin_load(const char *name);
    void tcg_plugin_cpus_stopped(void);
    void tcg_plugin_register_info(uint64_t pc, CPUState *env, TranslationBlock *tb);
    void tcg_plugin_before_gen_tb(CPUState *env, TranslationBlock *tb);
    void tcg_plugin_after_gen_tb(CPUState *env, TranslationBlock *tb);
    void tcg_plugin_after_gen_opc(TCGOpcode opname, uint16_t *opcode, TCGArg *opargs, uint8_t nb_args);
    const char *tcg_plugin_get_filename(void);
#else
#   define tcg_plugin_enabled() false
#   define tcg_plugin_load(dso)
#   define tcg_plugin_cpus_stopped()
#   define tcg_plugin_register_info(pc, env, tb)
#   define tcg_plugin_before_gen_tb(env, tb)
#   define tcg_plugin_after_gen_tb(env, tb)
#   define tcg_plugin_after_gen_opc(opname, tcg_opcode, tcg_opargs_, nb_args)
#   define tcg_plugin_get_filename() "<unknown>"
#endif /* !CONFIG_TCG_PLUGIN */

/***********************************************************************
 * TCG plugin interface.
 */

/* This structure shall be 64 bits, see call_tb_helper_code() for
 * details.  */
typedef struct
{
    uint16_t cpu_index;
    uint16_t size;
    union {
        char type;
        uint32_t icount;
    };
} __attribute__((__packed__, __may_alias__)) TPIHelperInfo;

#define TPI_MAX_OP_ARGS 6
typedef struct
{
    TCGOpcode name;
    uint64_t pc;
    uint8_t nb_args;

    uint16_t *opcode;
    TCGArg *opargs;

    /* Should be used by the plugin only.  */
    void *data;
} TPIOpCode;

struct TCGPluginInterface;
typedef struct TCGPluginInterface TCGPluginInterface;

typedef void (* tpi_cpus_stopped_t)(const TCGPluginInterface *tpi);

typedef void (* tpi_before_gen_tb_t)(const TCGPluginInterface *tpi);
typedef void (* tpi_after_gen_tb_t)(const TCGPluginInterface *tpi);

typedef void (* tpi_after_gen_opc_t)(const TCGPluginInterface *tpi, const TPIOpCode *opcode);

typedef void (* tpi_pre_tb_helper_code_t)(const TCGPluginInterface *tpi,
                                          TPIHelperInfo info, uint64_t address,
                                          uint64_t data1, uint64_t data2);

typedef void (* tpi_pre_tb_helper_data_t)(const TCGPluginInterface *tpi,
                                          TPIHelperInfo info, uint64_t address,
                                          uint64_t *data1, uint64_t *data2);

#define TPI_VERSION 3
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
    tpi_before_gen_tb_t before_gen_tb;
    tpi_after_gen_tb_t  after_gen_tb;
    tpi_pre_tb_helper_code_t pre_tb_helper_code;
    tpi_pre_tb_helper_data_t pre_tb_helper_data;
    tpi_after_gen_opc_t after_gen_opc;
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
