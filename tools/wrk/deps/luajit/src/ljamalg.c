/*
** LuaJIT core and libraries amalgamation.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

/*
+--------------------------------------------------------------------------+
| WARNING: Compiling the amalgamation needs a lot of virtual memory        |
| (around 200 MB with GCC 4.x)! If you don't have enough physical memory   |
| your machine will start swapping to disk and the compile will not finish |
| within a reasonable amount of time.                                      |
| So either compile on a bigger machine or use the non-amalgamated build.  |
+--------------------------------------------------------------------------+
*/

#define ljamalg_c
#define LUA_CORE

/* To get the mremap prototype. Must be defined before any system includes. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#ifndef WINVER
#define WINVER 0x0500
#endif

#include "lua.h"
#include "lauxlib.h"

#include "lj_gc.c"
#include "lj_err.c"
#include "lj_char.c"
#include "lj_bc.c"
#include "lj_obj.c"
#include "lj_str.c"
#include "lj_tab.c"
#include "lj_func.c"
#include "lj_udata.c"
#include "lj_meta.c"
#include "lj_debug.c"
#include "lj_state.c"
#include "lj_dispatch.c"
#include "lj_vmevent.c"
#include "lj_vmmath.c"
#include "lj_strscan.c"
#include "lj_api.c"
#include "lj_lex.c"
#include "lj_parse.c"
#include "lj_bcread.c"
#include "lj_bcwrite.c"
#include "lj_load.c"
#include "lj_ctype.c"
#include "lj_cdata.c"
#include "lj_cconv.c"
#include "lj_ccall.c"
#include "lj_ccallback.c"
#include "lj_carith.c"
#include "lj_clib.c"
#include "lj_cparse.c"
#include "lj_lib.c"
#include "lj_ir.c"
#include "lj_opt_mem.c"
#include "lj_opt_fold.c"
#include "lj_opt_narrow.c"
#include "lj_opt_dce.c"
#include "lj_opt_loop.c"
#include "lj_opt_split.c"
#include "lj_opt_sink.c"
#include "lj_mcode.c"
#include "lj_snap.c"
#include "lj_record.c"
#include "lj_crecord.c"
#include "lj_ffrecord.c"
#include "lj_asm.c"
#include "lj_trace.c"
#include "lj_gdbjit.c"
#include "lj_alloc.c"

#include "lib_aux.c"
#include "lib_base.c"
#include "lib_math.c"
#include "lib_string.c"
#include "lib_table.c"
#include "lib_io.c"
#include "lib_os.c"
#include "lib_package.c"
#include "lib_debug.c"
#include "lib_bit.c"
#include "lib_jit.c"
#include "lib_ffi.c"
#include "lib_init.c"

