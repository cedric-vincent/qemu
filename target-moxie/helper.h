#include "exec/def-helper.h"

#ifdef CONFIG_TCG_PLUGIN
DEF_HELPER_FLAGS_4(tcg_plugin_pre_tb, 0, void, i64, i64, i64, i64)
#endif

DEF_HELPER_2(raise_exception, void, env, int)
DEF_HELPER_1(debug, void, env)

DEF_HELPER_FLAGS_3(div, TCG_CALL_NO_WG, i32, env, i32, i32)
DEF_HELPER_FLAGS_3(udiv, TCG_CALL_NO_WG, i32, env, i32, i32)

#include "exec/def-helper.h"
