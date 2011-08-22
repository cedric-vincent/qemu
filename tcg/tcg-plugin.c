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

#include <stdbool.h> /* bool, true, false, */
#include <assert.h>  /* assert(3), */
#include <dlfcn.h>   /* dlopen(3), dlsym(3), */
#include <unistd.h>  /* access(2), STDERR_FILENO, getpid(2), */
#include <fcntl.h>   /* open(2), */
#include <stdlib.h>  /* getenv(3), */
#include <string.h>  /* strlen(3), */
#include <stdio.h>   /* *printf(3), memset(3), */
#include <pthread.h> /* pthread_*, */

#include "tcg-op.h"

/* Define helper.  */
#include "tcg-plugin-helper.h"
#define GEN_HELPER 1
#include "tcg-plugin-helper.h"

#include "tcg-plugin.h"
#include "exec-all.h"   /* CPUState, TranslationBlock */
#include "sysemu.h"     /* max_cpus */

/* Interface for the TCG plugin.  */
static TCGPluginInterface tpi;

/* Load the dynamic shared object "name" and call its function
 * "tpi_init()" to initialize itself.  Then, some sanity checks are
 * performed to ensure the dynamic shared object is compatible with
 * this instance of QEMU (guest CPU, emulation mode, ...).  */
void tcg_plugin_load(const char *name)
{
#if !defined(CONFIG_SOFTMMU)
    unsigned int max_cpus = 1;
#endif
    tpi_init_t tpi_init;
    char *path = NULL;
    bool done = false;
    void *handle;

    /* Check if "name" refers to an installed plugin (short form).  */
    if (name[0] != '.' && name[0] != '/') {
        const char *format = CONFIG_QEMU_LIBEXECDIR "/" TARGET_ARCH
            "/" EMULATION_MODE "/tcg-plugin-%s.so";
        size_t size = strlen(format) + strlen(name);
        int status;

        path = qemu_mallocz(size);
        snprintf(path, size, format, name);

        status = access(path, F_OK);
        if (status) {
            qemu_free(path);
            path = NULL;
        }
    }

    /*
     * Load the dynamic shared object and retreive its symbol
     * "tpi_init".
     */

    handle = dlopen(path ?: name, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "plugin: error: %s\n", dlerror());
        goto error;
    }

    tpi_init = dlsym(handle, "tpi_init");
    if (!tpi_init) {
        fprintf(stderr, "plugin: error: %s\n", dlerror());
        goto error;
    }

    /*
     * Fill the interface with information that may be useful to the
     * plugin initialization.
     */

    TPI_INIT_VERSION(tpi);

    tpi.nb_cpus = max_cpus;

    tpi.output = NULL;
    if (getenv("TPI_OUTPUT")) {
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s.%d", getenv("TPI_OUTPUT"), getpid());
        tpi.output = fopen(path, "w");
        if (!tpi.output) {
            perror("plugin: warning: can't open TPI_OUTPUT.$PID (fall back to stderr)");
        }
        /* This is a compromise between buffered output and truncated
         * output when exiting through _exit(2) in user-mode.  */
        setlinebuf(tpi.output);
    }
    if (!tpi.output)
        tpi.output = stderr;

    tpi.low_pc = 0;
    tpi.high_pc = UINT64_MAX;

    if (getenv("TPI_SYMBOL_PC")) {
#if 0
        struct syminfo *syminfo =
            reverse_lookup_symbol(getenv("TPI_SYMBOL_PC"));
        if (!syminfo)  {
            fprintf(tpi.output,
                    "plugin: warning: symbol '%s' not found\n",
                    getenv("TPI_SYMBOL_PC"));
        }
        tpi.low_pc  = syminfo.disas_symtab.elfXX.st_value;
        tpi.high_pc = tpi.low_pc + syminfo.disas_symtab.elfXX.st_size;
#else
        fprintf(tpi.output,
                "plugin: warning: TPI_SYMBOL_PC parameter not supported yet\n");
#endif
    }

    if (getenv("TPI_LOW_PC")) {
        tpi.low_pc = (uint64_t) strtoull(getenv("TPI_LOW_PC"), NULL, 0);
        if (!tpi.low_pc) {
            fprintf(tpi.output,
                    "plugin: warning: can't parse TPI_LOW_PC (fall back to 0)\n");
        }
    }

    if (getenv("TPI_HIGH_PC")) {
        tpi.high_pc = (uint64_t) strtoull(getenv("TPI_HIGH_PC"), NULL, 0);
        if (!tpi.high_pc) {
            fprintf(tpi.output,
                    "plugin: warning: can't parse TPI_HIGH_PC (fall back to UINT64_MAX)\n");
            tpi.high_pc = UINT64_MAX;
        }
    }

    /*
     * Tell the plugin to initialize itself.
     */

    tpi_init(&tpi);

    /*
     * Perform some sanity checks to ensure this TCG plugin is
     * compatible with this instance of QEMU (guest CPU, emulation
     * mode, ...)
     */

    if (!tpi.version) {
        fprintf(stderr, "plugin: error: initialization has failed\n");
        goto error;
    }

    if (tpi.version != TPI_VERSION) {
        fprintf(stderr, "plugin: error: incompatible plugin interface (%d != %d)\n",
                tpi.version, TPI_VERSION);
        goto error;
    }

    if (tpi.sizeof_CPUState != 0
        && tpi.sizeof_CPUState != sizeof(CPUState)) {
        fprintf(stderr, "plugin: error: incompatible CPUState size "
                "(%zu != %zu)\n", tpi.sizeof_CPUState, sizeof(CPUState));
        goto error;
    }

    if (tpi.sizeof_TranslationBlock != 0
        && tpi.sizeof_TranslationBlock != sizeof(TranslationBlock)) {
        fprintf(stderr, "plugin: error: incompatible TranslationBlock size "
                "(%zu != %zu)\n", tpi.sizeof_TranslationBlock,
                sizeof(TranslationBlock));
        goto error;
    }

    if (strcmp(tpi.guest, TARGET_ARCH) != 0
        && strcmp(tpi.guest, "any") != 0) {
        fprintf(stderr, "plugin: warning: incompatible guest CPU "
                "(%s != %s)\n", tpi.guest, TARGET_ARCH);
    }

    if (strcmp(tpi.mode, EMULATION_MODE) != 0
        && strcmp(tpi.mode, "any") != 0) {
        fprintf(stderr, "plugin: warning: incompatible emulation mode "
                "(%s != %s)\n", tpi.mode, EMULATION_MODE);
    }

    tpi.is_generic = strcmp(tpi.guest, "any") == 0 && strcmp(tpi.mode, "any") == 0;

    if (getenv("TPI_VERBOSE")) {
        tpi.verbose = true;
        fprintf(tpi.output, "plugin: info: version = %d\n", tpi.version);
        fprintf(tpi.output, "plugin: info: guest = %s\n", tpi.guest);
        fprintf(tpi.output, "plugin: info: mode = %s\n", tpi.mode);
        fprintf(tpi.output, "plugin: info: sizeof(CPUState) = %zu\n", tpi.sizeof_CPUState);
        fprintf(tpi.output, "plugin: info: sizeof(TranslationBlock) = %zu\n", tpi.sizeof_TranslationBlock);
        fprintf(tpi.output, "plugin: info: output fd = %d\n", fileno(tpi.output));
        fprintf(tpi.output, "plugin: info: low pc = 0x%016" PRIx64 "\n", tpi.low_pc);
        fprintf(tpi.output, "plugin: info: high pc = 0x%016" PRIx64 "\n", tpi.high_pc);
        fprintf(tpi.output, "plugin: info: cpus_stopped callback = %p\n", tpi.cpus_stopped);
        fprintf(tpi.output, "plugin: info: before_icg callback = %p\n", tpi.before_icg);
        fprintf(tpi.output, "plugin: info: after_icg callback = %p\n", tpi.after_icg);
        fprintf(tpi.output, "plugin: info: tb_helper_code callback = %p\n", tpi.tb_helper_code);
        fprintf(tpi.output, "plugin: info: tb_helper_data callback = %p\n", tpi.tb_helper_data);
        fprintf(tpi.output, "plugin: info: is%s generic\n", tpi.is_generic ? "" : " not");
    }

    /* Register helper.  */
#define GEN_HELPER 2
#include "tcg-plugin-helper.h"

    done = true;

error:
    if (path)
        qemu_free(path);

    if (!done)
        memset(&tpi, 0, sizeof(tpi));

    return;
}

static TCGArg *tb_info;
static TCGArg *tb_data1;
static TCGArg *tb_data2;

/* Generate TCG opcodes to call helper_tcg_plugin_tb*().  */
static void gen_tb_helper(CPUState *env, TranslationBlock *tb)
{
    TCGv_i64 address = tcg_const_i64((uint64_t)tb->pc);

    /* Patched in tcg_plugin_after_icg().  */
    tb_info = gen_opparam_ptr + 1;
    TCGv_i64 info = tcg_const_i64(0);

    TCGv_i64 data1;
    TCGv_i64 data2;
    if (tpi.tb_helper_data) {
        /* Patched in tcg_plugin_after_icg().  */
        tb_data1 = gen_opparam_ptr + 1;
        data1 = tcg_const_i64(0);

        /* Patched in tcg_plugin_after_icg().  */
        tb_data2 = gen_opparam_ptr + 1;
        data2 = tcg_const_i64(0);

        gen_helper_tcg_plugin_tb2(address, info, data1, data2);
    }
    else {
        gen_helper_tcg_plugin_tb(address, info);
    }

    tcg_temp_free_i64(address);
    tcg_temp_free_i64(info);

    if (tpi.tb_helper_data) {
        tcg_temp_free_i64(data1);
        tcg_temp_free_i64(data2);
    }
}

/* Hook called once all CPUs are stopped/paused.  */
void tcg_plugin_cpus_stopped(void)
{
    if (tpi.cpus_stopped) {
        tpi.cpus_stopped(&tpi);
    }
}

/* Wrapper to ensure only non-generic plugins can access non-generic data.  */
#define TPI_CALLBACK_NOT_GENERIC(callback, ...)         \
    do {                                                \
        if (!tpi.is_generic) {                          \
            tpi.env = env;                              \
            tpi.tb = tb;                                \
        }                                               \
        tpi.callback(&tpi, ##__VA_ARGS__);              \
        tpi.env = NULL;                                 \
        tpi.tb = NULL;                                  \
    } while (0);

/* Hook called before the Intermediate Code Generation (ICG).  */
void tcg_plugin_before_icg(CPUState *env, TranslationBlock *tb)
{
    if (tb->pc < tpi.low_pc || tb->pc >= tpi.high_pc) {
        return;
    }

    if (tpi.before_icg) {
        TPI_CALLBACK_NOT_GENERIC(before_icg);
    }

    if (tpi.tb_helper_code) {
        gen_tb_helper(env, tb);
    }
}

/* Hook called after the Intermediate Code Generation (ICG).  */
void tcg_plugin_after_icg(CPUState *env, TranslationBlock *tb)
{
    if (tb->pc < tpi.low_pc || tb->pc >= tpi.high_pc) {
        return;
    }

    if (tpi.after_icg) {
        TPI_CALLBACK_NOT_GENERIC(after_icg);
    }

    if (tpi.tb_helper_code) {
        /* Patch helper_tcg_plugin_tb*() parameters.  */

        ((TPIHelperInfo *)tb_info)->cpu_index = env->cpu_index;
        ((TPIHelperInfo *)tb_info)->size = tb->size;
#if TCG_TARGET_REG_BITS == 64
        ((TPIHelperInfo *)tb_info)->icount = tb->icount;
#else
        /* i64 variables use 2 arguments on 32-bit host.  */
        *(tb_info + 2) = tb->icount;
#endif

        if (tpi.tb_helper_data) {
            /* FIXME: qemu-sh4 crashes when built on Ubuntu 10.04.1
             * x86_64 (not on Slackware64-13.37) if some bits of data2
             * aren't initialized, either here or in the plugin.  This
             * is really unexpected since data2 should be read by the
             * plugin only!
             */

            uint64_t data1 = 0;
            uint64_t data2 = 0;

            TPI_CALLBACK_NOT_GENERIC(tb_helper_data, *(TPIHelperInfo *)tb_info, tb->pc, &data1, &data2);

#if TCG_TARGET_REG_BITS == 64
            *(uint64_t *)tb_data1 = data1;
            *(uint64_t *)tb_data2 = data2;
#else
            /* i64 variables use 2 arguments on 32-bit host.  */
            *tb_data1 = data1 & 0xFFFFFFFF;
            *(tb_data1 + 2) = data1 >> 32;

            *tb_data2 = data2 & 0xFFFFFFFF;
            *(tb_data2 + 2) = data2 >> 32;
#endif
        }
    }
}

/* Real TCG helper used to call tb_helper_code() in a thread-safe
 * way.  */
static inline void call_tb_helper_code(uint64_t address, uint64_t info,
                                       uint64_t data1, uint64_t data2)
{
    /* Ensure resources only used by tb_helper_code are protected from
       concurrent access.  */
    static pthread_mutex_t tb_helper_mutex = PTHREAD_MUTEX_INITIALIZER;

    int error = pthread_mutex_lock(&tb_helper_mutex);
    if (error) {
        fprintf(stderr, "plugin: call_tb_helper() failed to pthread_mutex_lock(): %s\n",
                strerror(error));
        goto end;
    }

    tpi.tb_helper_code(&tpi, *(TPIHelperInfo *)&info, address, data1, data2);

end:
    pthread_mutex_unlock(&tb_helper_mutex);
}

void helper_tcg_plugin_tb(uint64_t address, uint64_t info)
{
    call_tb_helper_code(address, info, 0, 0);
}

void helper_tcg_plugin_tb2(uint64_t address, uint64_t info,
                           uint64_t data1, uint64_t data2)
{
    call_tb_helper_code(address, info, data1, data2);
}
