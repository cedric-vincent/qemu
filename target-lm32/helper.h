#include "exec/def-helper.h"

#ifdef CONFIG_TCG_PLUGIN
DEF_HELPER_FLAGS_4(tcg_plugin_pre_tb, 0, void, i64, i64, i64, i64)
#endif

DEF_HELPER_2(raise_exception, void, env, i32)
DEF_HELPER_1(hlt, void, env)
DEF_HELPER_2(wcsr_im, void, env, i32)
DEF_HELPER_2(wcsr_ip, void, env, i32)
DEF_HELPER_2(wcsr_jtx, void, env, i32)
DEF_HELPER_2(wcsr_jrx, void, env, i32)
DEF_HELPER_1(rcsr_im, i32, env)
DEF_HELPER_1(rcsr_ip, i32, env)
DEF_HELPER_1(rcsr_jtx, i32, env)
DEF_HELPER_1(rcsr_jrx, i32, env)

#include "exec/def-helper.h"
