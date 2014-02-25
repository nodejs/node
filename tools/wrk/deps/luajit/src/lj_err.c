/*
** Error handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_err_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_str.h"
#include "lj_func.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_ff.h"
#include "lj_trace.h"
#include "lj_vm.h"

/*
** LuaJIT can either use internal or external frame unwinding:
**
** - Internal frame unwinding (INT) is free-standing and doesn't require
**   any OS or library support.
**
** - External frame unwinding (EXT) uses the system-provided unwind handler.
**
** Pros and Cons:
**
** - EXT requires unwind tables for *all* functions on the C stack between
**   the pcall/catch and the error/throw. This is the default on x64,
**   but needs to be manually enabled on x86/PPC for non-C++ code.
**
** - INT is faster when actually throwing errors (but this happens rarely).
**   Setting up error handlers is zero-cost in any case.
**
** - EXT provides full interoperability with C++ exceptions. You can throw
**   Lua errors or C++ exceptions through a mix of Lua frames and C++ frames.
**   C++ destructors are called as needed. C++ exceptions caught by pcall
**   are converted to the string "C++ exception". Lua errors can be caught
**   with catch (...) in C++.
**
** - INT has only limited support for automatically catching C++ exceptions
**   on POSIX systems using DWARF2 stack unwinding. Other systems may use
**   the wrapper function feature. Lua errors thrown through C++ frames
**   cannot be caught by C++ code and C++ destructors are not run.
**
** EXT is the default on x64 systems, INT is the default on all other systems.
**
** EXT can be manually enabled on POSIX systems using GCC and DWARF2 stack
** unwinding with -DLUAJIT_UNWIND_EXTERNAL. *All* C code must be compiled
** with -funwind-tables (or -fexceptions). This includes LuaJIT itself (set
** TARGET_CFLAGS), all of your C/Lua binding code, all loadable C modules
** and all C libraries that have callbacks which may be used to call back
** into Lua. C++ code must *not* be compiled with -fno-exceptions.
**
** EXT cannot be enabled on WIN32 since system exceptions use code-driven SEH.
** EXT is mandatory on WIN64 since the calling convention has an abundance
** of callee-saved registers (rbx, rbp, rsi, rdi, r12-r15, xmm6-xmm15).
** EXT is mandatory on POSIX/x64 since the interpreter doesn't save r12/r13.
*/

#if defined(__GNUC__) && (LJ_TARGET_X64 || defined(LUAJIT_UNWIND_EXTERNAL))
#define LJ_UNWIND_EXT	1
#elif LJ_TARGET_X64 && LJ_TARGET_WINDOWS
#define LJ_UNWIND_EXT	1
#endif

/* -- Error messages ------------------------------------------------------ */

/* Error message strings. */
LJ_DATADEF const char *lj_err_allmsg =
#define ERRDEF(name, msg)	msg "\0"
#include "lj_errmsg.h"
;

/* -- Internal frame unwinding -------------------------------------------- */

/* Unwind Lua stack and move error message to new top. */
LJ_NOINLINE static void unwindstack(lua_State *L, TValue *top)
{
  lj_func_closeuv(L, top);
  if (top < L->top-1) {
    copyTV(L, top, L->top-1);
    L->top = top+1;
  }
  lj_state_relimitstack(L);
}

/* Unwind until stop frame. Optionally cleanup frames. */
static void *err_unwind(lua_State *L, void *stopcf, int errcode)
{
  TValue *frame = L->base-1;
  void *cf = L->cframe;
  while (cf) {
    int32_t nres = cframe_nres(cframe_raw(cf));
    if (nres < 0) {  /* C frame without Lua frame? */
      TValue *top = restorestack(L, -nres);
      if (frame < top) {  /* Frame reached? */
	if (errcode) {
	  L->cframe = cframe_prev(cf);
	  L->base = frame+1;
	  unwindstack(L, top);
	}
	return cf;
      }
    }
    if (frame <= tvref(L->stack))
      break;
    switch (frame_typep(frame)) {
    case FRAME_LUA:  /* Lua frame. */
    case FRAME_LUAP:
      frame = frame_prevl(frame);
      break;
    case FRAME_C:  /* C frame. */
#if LJ_HASFFI
    unwind_c:
#endif
#if LJ_UNWIND_EXT
      if (errcode) {
	L->cframe = cframe_prev(cf);
	L->base = frame_prevd(frame) + 1;
	unwindstack(L, frame);
      } else if (cf != stopcf) {
	cf = cframe_prev(cf);
	frame = frame_prevd(frame);
	break;
      }
      return NULL;  /* Continue unwinding. */
#else
      UNUSED(stopcf);
      cf = cframe_prev(cf);
      frame = frame_prevd(frame);
      break;
#endif
    case FRAME_CP:  /* Protected C frame. */
      if (cframe_canyield(cf)) {  /* Resume? */
	if (errcode) {
	  hook_leave(G(L));  /* Assumes nobody uses coroutines inside hooks. */
	  L->cframe = NULL;
	  L->status = (uint8_t)errcode;
	}
	return cf;
      }
      if (errcode) {
	L->cframe = cframe_prev(cf);
	L->base = frame_prevd(frame) + 1;
	unwindstack(L, frame);
      }
      return cf;
    case FRAME_CONT:  /* Continuation frame. */
#if LJ_HASFFI
      if ((frame-1)->u32.lo == LJ_CONT_FFI_CALLBACK)
	goto unwind_c;
#endif
    case FRAME_VARG:  /* Vararg frame. */
      frame = frame_prevd(frame);
      break;
    case FRAME_PCALL:  /* FF pcall() frame. */
    case FRAME_PCALLH:  /* FF pcall() frame inside hook. */
      if (errcode) {
	if (errcode == LUA_YIELD) {
	  frame = frame_prevd(frame);
	  break;
	}
	if (frame_typep(frame) == FRAME_PCALL)
	  hook_leave(G(L));
	L->cframe = cf;
	L->base = frame_prevd(frame) + 1;
	unwindstack(L, L->base);
      }
      return (void *)((intptr_t)cf | CFRAME_UNWIND_FF);
    }
  }
  /* No C frame. */
  if (errcode) {
    L->cframe = NULL;
    L->base = tvref(L->stack)+1;
    unwindstack(L, L->base);
    if (G(L)->panic)
      G(L)->panic(L);
    exit(EXIT_FAILURE);
  }
  return L;  /* Anything non-NULL will do. */
}

/* -- External frame unwinding -------------------------------------------- */

#if defined(__GNUC__) && !LJ_NO_UNWIND && !LJ_TARGET_WINDOWS

/*
** We have to use our own definitions instead of the mandatory (!) unwind.h,
** since various OS, distros and compilers mess up the header installation.
*/

typedef struct _Unwind_Exception
{
  uint64_t exclass;
  void (*excleanup)(int, struct _Unwind_Exception);
  uintptr_t p1, p2;
} __attribute__((__aligned__)) _Unwind_Exception;

typedef struct _Unwind_Context _Unwind_Context;

#define _URC_OK			0
#define _URC_FATAL_PHASE1_ERROR	3
#define _URC_HANDLER_FOUND	6
#define _URC_INSTALL_CONTEXT	7
#define _URC_CONTINUE_UNWIND	8
#define _URC_FAILURE		9

#if !LJ_TARGET_ARM

extern uintptr_t _Unwind_GetCFA(_Unwind_Context *);
extern void _Unwind_SetGR(_Unwind_Context *, int, uintptr_t);
extern void _Unwind_SetIP(_Unwind_Context *, uintptr_t);
extern void _Unwind_DeleteException(_Unwind_Exception *);
extern int _Unwind_RaiseException(_Unwind_Exception *);

#define _UA_SEARCH_PHASE	1
#define _UA_CLEANUP_PHASE	2
#define _UA_HANDLER_FRAME	4
#define _UA_FORCE_UNWIND	8

#define LJ_UEXCLASS		0x4c55414a49543200ULL	/* LUAJIT2\0 */
#define LJ_UEXCLASS_MAKE(c)	(LJ_UEXCLASS | (uint64_t)(c))
#define LJ_UEXCLASS_CHECK(cl)	(((cl) ^ LJ_UEXCLASS) <= 0xff)
#define LJ_UEXCLASS_ERRCODE(cl)	((int)((cl) & 0xff))

/* DWARF2 personality handler referenced from interpreter .eh_frame. */
LJ_FUNCA int lj_err_unwind_dwarf(int version, int actions,
  uint64_t uexclass, _Unwind_Exception *uex, _Unwind_Context *ctx)
{
  void *cf;
  lua_State *L;
  if (version != 1)
    return _URC_FATAL_PHASE1_ERROR;
  UNUSED(uexclass);
  cf = (void *)_Unwind_GetCFA(ctx);
  L = cframe_L(cf);
  if ((actions & _UA_SEARCH_PHASE)) {
#if LJ_UNWIND_EXT
    if (err_unwind(L, cf, 0) == NULL)
      return _URC_CONTINUE_UNWIND;
#endif
    if (!LJ_UEXCLASS_CHECK(uexclass)) {
      setstrV(L, L->top++, lj_err_str(L, LJ_ERR_ERRCPP));
    }
    return _URC_HANDLER_FOUND;
  }
  if ((actions & _UA_CLEANUP_PHASE)) {
    int errcode;
    if (LJ_UEXCLASS_CHECK(uexclass)) {
      errcode = LJ_UEXCLASS_ERRCODE(uexclass);
    } else {
      if ((actions & _UA_HANDLER_FRAME))
	_Unwind_DeleteException(uex);
      errcode = LUA_ERRRUN;
    }
#if LJ_UNWIND_EXT
    cf = err_unwind(L, cf, errcode);
    if ((actions & _UA_FORCE_UNWIND)) {
      return _URC_CONTINUE_UNWIND;
    } else if (cf) {
      _Unwind_SetGR(ctx, LJ_TARGET_EHRETREG, errcode);
      _Unwind_SetIP(ctx, (uintptr_t)(cframe_unwind_ff(cf) ?
				     lj_vm_unwind_ff_eh :
				     lj_vm_unwind_c_eh));
      return _URC_INSTALL_CONTEXT;
    }
#if LJ_TARGET_X86ORX64
    else if ((actions & _UA_HANDLER_FRAME)) {
      /* Workaround for ancient libgcc bug. Still present in RHEL 5.5. :-/
      ** Real fix: http://gcc.gnu.org/viewcvs/trunk/gcc/unwind-dw2.c?r1=121165&r2=124837&pathrev=153877&diff_format=h
      */
      _Unwind_SetGR(ctx, LJ_TARGET_EHRETREG, errcode);
      _Unwind_SetIP(ctx, (uintptr_t)lj_vm_unwind_rethrow);
      return _URC_INSTALL_CONTEXT;
    }
#endif
#else
    /* This is not the proper way to escape from the unwinder. We get away with
    ** it on non-x64 because the interpreter restores all callee-saved regs.
    */
    lj_err_throw(L, errcode);
#endif
  }
  return _URC_CONTINUE_UNWIND;
}

#if LJ_UNWIND_EXT
#if LJ_TARGET_OSX || defined(__OpenBSD__)
/* Sorry, no thread safety for OSX. Complain to Apple, not me. */
static _Unwind_Exception static_uex;
#else
static __thread _Unwind_Exception static_uex;
#endif

/* Raise DWARF2 exception. */
static void err_raise_ext(int errcode)
{
  static_uex.exclass = LJ_UEXCLASS_MAKE(errcode);
  static_uex.excleanup = NULL;
  _Unwind_RaiseException(&static_uex);
}
#endif

#else

extern void _Unwind_DeleteException(void *);
extern int __gnu_unwind_frame (void *, _Unwind_Context *);
extern int _Unwind_VRS_Set(_Unwind_Context *, int, uint32_t, int, void *);
extern int _Unwind_VRS_Get(_Unwind_Context *, int, uint32_t, int, void *);

static inline uint32_t _Unwind_GetGR(_Unwind_Context *ctx, int r)
{
  uint32_t v;
  _Unwind_VRS_Get(ctx, 0, r, 0, &v);
  return v;
}

static inline void _Unwind_SetGR(_Unwind_Context *ctx, int r, uint32_t v)
{
  _Unwind_VRS_Set(ctx, 0, r, 0, &v);
}

#define _US_VIRTUAL_UNWIND_FRAME	0
#define _US_UNWIND_FRAME_STARTING	1
#define _US_ACTION_MASK			3
#define _US_FORCE_UNWIND		8

/* ARM unwinder personality handler referenced from interpreter .ARM.extab. */
LJ_FUNCA int lj_err_unwind_arm(int state, void *ucb, _Unwind_Context *ctx)
{
  void *cf = (void *)_Unwind_GetGR(ctx, 13);
  lua_State *L = cframe_L(cf);
  if ((state & _US_ACTION_MASK) == _US_VIRTUAL_UNWIND_FRAME) {
    setstrV(L, L->top++, lj_err_str(L, LJ_ERR_ERRCPP));
    return _URC_HANDLER_FOUND;
  }
  if ((state&(_US_ACTION_MASK|_US_FORCE_UNWIND)) == _US_UNWIND_FRAME_STARTING) {
    _Unwind_DeleteException(ucb);
    _Unwind_SetGR(ctx, 15, (uint32_t)(void *)lj_err_throw);
    _Unwind_SetGR(ctx, 0, (uint32_t)L);
    _Unwind_SetGR(ctx, 1, (uint32_t)LUA_ERRRUN);
    return _URC_INSTALL_CONTEXT;
  }
  if (__gnu_unwind_frame(ucb, ctx) != _URC_OK)
    return _URC_FAILURE;
  return _URC_CONTINUE_UNWIND;
}

#endif

#elif LJ_TARGET_X64 && LJ_TARGET_WINDOWS

/*
** Someone in Redmond owes me several days of my life. A lot of this is
** undocumented or just plain wrong on MSDN. Some of it can be gathered
** from 3rd party docs or must be found by trial-and-error. They really
** don't want you to write your own language-specific exception handler
** or to interact gracefully with MSVC. :-(
**
** Apparently MSVC doesn't call C++ destructors for foreign exceptions
** unless you compile your C++ code with /EHa. Unfortunately this means
** catch (...) also catches things like access violations. The use of
** _set_se_translator doesn't really help, because it requires /EHa, too.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Taken from: http://www.nynaeve.net/?p=99 */
typedef struct UndocumentedDispatcherContext {
  ULONG64 ControlPc;
  ULONG64 ImageBase;
  PRUNTIME_FUNCTION FunctionEntry;
  ULONG64 EstablisherFrame;
  ULONG64 TargetIp;
  PCONTEXT ContextRecord;
  PEXCEPTION_ROUTINE LanguageHandler;
  PVOID HandlerData;
  PUNWIND_HISTORY_TABLE HistoryTable;
  ULONG ScopeIndex;
  ULONG Fill0;
} UndocumentedDispatcherContext;

/* Another wild guess. */
extern void __DestructExceptionObject(EXCEPTION_RECORD *rec, int nothrow);

#ifdef MINGW_SDK_INIT
/* Workaround for broken MinGW64 declaration. */
VOID RtlUnwindEx_FIXED(PVOID,PVOID,PVOID,PVOID,PVOID,PVOID) asm("RtlUnwindEx");
#define RtlUnwindEx RtlUnwindEx_FIXED
#endif

#define LJ_MSVC_EXCODE		((DWORD)0xe06d7363)
#define LJ_GCC_EXCODE		((DWORD)0x20474343)

#define LJ_EXCODE		((DWORD)0xe24c4a00)
#define LJ_EXCODE_MAKE(c)	(LJ_EXCODE | (DWORD)(c))
#define LJ_EXCODE_CHECK(cl)	(((cl) ^ LJ_EXCODE) <= 0xff)
#define LJ_EXCODE_ERRCODE(cl)	((int)((cl) & 0xff))

/* Win64 exception handler for interpreter frame. */
LJ_FUNCA EXCEPTION_DISPOSITION lj_err_unwind_win64(EXCEPTION_RECORD *rec,
  void *cf, CONTEXT *ctx, UndocumentedDispatcherContext *dispatch)
{
  lua_State *L = cframe_L(cf);
  int errcode = LJ_EXCODE_CHECK(rec->ExceptionCode) ?
		LJ_EXCODE_ERRCODE(rec->ExceptionCode) : LUA_ERRRUN;
  if ((rec->ExceptionFlags & 6)) {  /* EH_UNWINDING|EH_EXIT_UNWIND */
    /* Unwind internal frames. */
    err_unwind(L, cf, errcode);
  } else {
    void *cf2 = err_unwind(L, cf, 0);
    if (cf2) {  /* We catch it, so start unwinding the upper frames. */
      if (rec->ExceptionCode == LJ_MSVC_EXCODE ||
	  rec->ExceptionCode == LJ_GCC_EXCODE) {
	__DestructExceptionObject(rec, 1);
	setstrV(L, L->top++, lj_err_str(L, LJ_ERR_ERRCPP));
      } else if (!LJ_EXCODE_CHECK(rec->ExceptionCode)) {
	/* Don't catch access violations etc. */
	return ExceptionContinueSearch;
      }
      /* Unwind the stack and call all handlers for all lower C frames
      ** (including ourselves) again with EH_UNWINDING set. Then set
      ** rsp = cf, rax = errcode and jump to the specified target.
      */
      RtlUnwindEx(cf, (void *)((cframe_unwind_ff(cf2) && errcode != LUA_YIELD) ?
			       lj_vm_unwind_ff_eh :
			       lj_vm_unwind_c_eh),
		  rec, (void *)(uintptr_t)errcode, ctx, dispatch->HistoryTable);
      /* RtlUnwindEx should never return. */
    }
  }
  return ExceptionContinueSearch;
}

/* Raise Windows exception. */
static void err_raise_ext(int errcode)
{
  RaiseException(LJ_EXCODE_MAKE(errcode), 1 /* EH_NONCONTINUABLE */, 0, NULL);
}

#endif

/* -- Error handling ------------------------------------------------------ */

/* Throw error. Find catch frame, unwind stack and continue. */
LJ_NOINLINE void LJ_FASTCALL lj_err_throw(lua_State *L, int errcode)
{
  global_State *g = G(L);
  lj_trace_abort(g);
  setgcrefnull(g->jit_L);
  L->status = 0;
#if LJ_UNWIND_EXT
  err_raise_ext(errcode);
  /*
  ** A return from this function signals a corrupt C stack that cannot be
  ** unwound. We have no choice but to call the panic function and exit.
  **
  ** Usually this is caused by a C function without unwind information.
  ** This should never happen on x64, but may happen if you've manually
  ** enabled LUAJIT_UNWIND_EXTERNAL and forgot to recompile *every*
  ** non-C++ file with -funwind-tables.
  */
  if (G(L)->panic)
    G(L)->panic(L);
#else
  {
    void *cf = err_unwind(L, NULL, errcode);
    if (cframe_unwind_ff(cf))
      lj_vm_unwind_ff(cframe_raw(cf));
    else
      lj_vm_unwind_c(cframe_raw(cf), errcode);
  }
#endif
  exit(EXIT_FAILURE);
}

/* Return string object for error message. */
LJ_NOINLINE GCstr *lj_err_str(lua_State *L, ErrMsg em)
{
  return lj_str_newz(L, err2msg(em));
}

/* Out-of-memory error. */
LJ_NOINLINE void lj_err_mem(lua_State *L)
{
  if (L->status == LUA_ERRERR+1)  /* Don't touch the stack during lua_open. */
    lj_vm_unwind_c(L->cframe, LUA_ERRMEM);
  setstrV(L, L->top++, lj_err_str(L, LJ_ERR_ERRMEM));
  lj_err_throw(L, LUA_ERRMEM);
}

/* Find error function for runtime errors. Requires an extra stack traversal. */
static ptrdiff_t finderrfunc(lua_State *L)
{
  cTValue *frame = L->base-1, *bot = tvref(L->stack);
  void *cf = L->cframe;
  while (frame > bot) {
    lua_assert(cf != NULL);
    while (cframe_nres(cframe_raw(cf)) < 0) {  /* cframe without frame? */
      if (frame >= restorestack(L, -cframe_nres(cf)))
	break;
      if (cframe_errfunc(cf) >= 0)  /* Error handler not inherited (-1)? */
	return cframe_errfunc(cf);
      cf = cframe_prev(cf);  /* Else unwind cframe and continue searching. */
      if (cf == NULL)
	return 0;
    }
    switch (frame_typep(frame)) {
    case FRAME_LUA:
    case FRAME_LUAP:
      frame = frame_prevl(frame);
      break;
    case FRAME_C:
      cf = cframe_prev(cf);
      /* fallthrough */
    case FRAME_CONT:
#if LJ_HASFFI
      if ((frame-1)->u32.lo == LJ_CONT_FFI_CALLBACK)
	cf = cframe_prev(cf);
#endif
    case FRAME_VARG:
      frame = frame_prevd(frame);
      break;
    case FRAME_CP:
      if (cframe_canyield(cf)) return 0;
      if (cframe_errfunc(cf) >= 0)
	return cframe_errfunc(cf);
      frame = frame_prevd(frame);
      break;
    case FRAME_PCALL:
    case FRAME_PCALLH:
      if (frame_ftsz(frame) >= (ptrdiff_t)(2*sizeof(TValue)))  /* xpcall? */
	return savestack(L, frame-1);  /* Point to xpcall's errorfunc. */
      return 0;
    default:
      lua_assert(0);
      return 0;
    }
  }
  return 0;
}

/* Runtime error. */
LJ_NOINLINE void lj_err_run(lua_State *L)
{
  ptrdiff_t ef = finderrfunc(L);
  if (ef) {
    TValue *errfunc = restorestack(L, ef);
    TValue *top = L->top;
    lj_trace_abort(G(L));
    if (!tvisfunc(errfunc) || L->status == LUA_ERRERR) {
      setstrV(L, top-1, lj_err_str(L, LJ_ERR_ERRERR));
      lj_err_throw(L, LUA_ERRERR);
    }
    L->status = LUA_ERRERR;
    copyTV(L, top, top-1);
    copyTV(L, top-1, errfunc);
    L->top = top+1;
    lj_vm_call(L, top, 1+1);  /* Stack: |errfunc|msg| -> |msg| */
  }
  lj_err_throw(L, LUA_ERRRUN);
}

/* Formatted runtime error message. */
LJ_NORET LJ_NOINLINE static void err_msgv(lua_State *L, ErrMsg em, ...)
{
  const char *msg;
  va_list argp;
  va_start(argp, em);
  if (curr_funcisL(L)) L->top = curr_topL(L);
  msg = lj_str_pushvf(L, err2msg(em), argp);
  va_end(argp);
  lj_debug_addloc(L, msg, L->base-1, NULL);
  lj_err_run(L);
}

/* Non-vararg variant for better calling conventions. */
LJ_NOINLINE void lj_err_msg(lua_State *L, ErrMsg em)
{
  err_msgv(L, em);
}

/* Lexer error. */
LJ_NOINLINE void lj_err_lex(lua_State *L, GCstr *src, const char *tok,
			    BCLine line, ErrMsg em, va_list argp)
{
  char buff[LUA_IDSIZE];
  const char *msg;
  lj_debug_shortname(buff, src);
  msg = lj_str_pushvf(L, err2msg(em), argp);
  msg = lj_str_pushf(L, "%s:%d: %s", buff, line, msg);
  if (tok)
    lj_str_pushf(L, err2msg(LJ_ERR_XNEAR), msg, tok);
  lj_err_throw(L, LUA_ERRSYNTAX);
}

/* Typecheck error for operands. */
LJ_NOINLINE void lj_err_optype(lua_State *L, cTValue *o, ErrMsg opm)
{
  const char *tname = lj_typename(o);
  const char *opname = err2msg(opm);
  if (curr_funcisL(L)) {
    GCproto *pt = curr_proto(L);
    const BCIns *pc = cframe_Lpc(L) - 1;
    const char *oname = NULL;
    const char *kind = lj_debug_slotname(pt, pc, (BCReg)(o-L->base), &oname);
    if (kind)
      err_msgv(L, LJ_ERR_BADOPRT, opname, kind, oname, tname);
  }
  err_msgv(L, LJ_ERR_BADOPRV, opname, tname);
}

/* Typecheck error for ordered comparisons. */
LJ_NOINLINE void lj_err_comp(lua_State *L, cTValue *o1, cTValue *o2)
{
  const char *t1 = lj_typename(o1);
  const char *t2 = lj_typename(o2);
  err_msgv(L, t1 == t2 ? LJ_ERR_BADCMPV : LJ_ERR_BADCMPT, t1, t2);
  /* This assumes the two "boolean" entries are commoned by the C compiler. */
}

/* Typecheck error for __call. */
LJ_NOINLINE void lj_err_optype_call(lua_State *L, TValue *o)
{
  /* Gross hack if lua_[p]call or pcall/xpcall fail for a non-callable object:
  ** L->base still points to the caller. So add a dummy frame with L instead
  ** of a function. See lua_getstack().
  */
  const BCIns *pc = cframe_Lpc(L);
  if (((ptrdiff_t)pc & FRAME_TYPE) != FRAME_LUA) {
    const char *tname = lj_typename(o);
    setframe_pc(o, pc);
    setframe_gc(o, obj2gco(L));
    L->top = L->base = o+1;
    err_msgv(L, LJ_ERR_BADCALL, tname);
  }
  lj_err_optype(L, o, LJ_ERR_OPCALL);
}

/* Error in context of caller. */
LJ_NOINLINE void lj_err_callermsg(lua_State *L, const char *msg)
{
  TValue *frame = L->base-1;
  TValue *pframe = NULL;
  if (frame_islua(frame)) {
    pframe = frame_prevl(frame);
  } else if (frame_iscont(frame)) {
#if LJ_HASFFI
    if ((frame-1)->u32.lo == LJ_CONT_FFI_CALLBACK) {
      pframe = frame;
      frame = NULL;
    } else
#endif
    {
      pframe = frame_prevd(frame);
#if LJ_HASFFI
      /* Remove frame for FFI metamethods. */
      if (frame_func(frame)->c.ffid >= FF_ffi_meta___index &&
	  frame_func(frame)->c.ffid <= FF_ffi_meta___tostring) {
	L->base = pframe+1;
	L->top = frame;
	setcframe_pc(cframe_raw(L->cframe), frame_contpc(frame));
      }
#endif
    }
  }
  lj_debug_addloc(L, msg, pframe, frame);
  lj_err_run(L);
}

/* Formatted error in context of caller. */
LJ_NOINLINE void lj_err_callerv(lua_State *L, ErrMsg em, ...)
{
  const char *msg;
  va_list argp;
  va_start(argp, em);
  msg = lj_str_pushvf(L, err2msg(em), argp);
  va_end(argp);
  lj_err_callermsg(L, msg);
}

/* Error in context of caller. */
LJ_NOINLINE void lj_err_caller(lua_State *L, ErrMsg em)
{
  lj_err_callermsg(L, err2msg(em));
}

/* Argument error message. */
LJ_NORET LJ_NOINLINE static void err_argmsg(lua_State *L, int narg,
					    const char *msg)
{
  const char *fname = "?";
  const char *ftype = lj_debug_funcname(L, L->base - 1, &fname);
  if (narg < 0 && narg > LUA_REGISTRYINDEX)
    narg = (int)(L->top - L->base) + narg + 1;
  if (ftype && ftype[3] == 'h' && --narg == 0)  /* Check for "method". */
    msg = lj_str_pushf(L, err2msg(LJ_ERR_BADSELF), fname, msg);
  else
    msg = lj_str_pushf(L, err2msg(LJ_ERR_BADARG), narg, fname, msg);
  lj_err_callermsg(L, msg);
}

/* Formatted argument error. */
LJ_NOINLINE void lj_err_argv(lua_State *L, int narg, ErrMsg em, ...)
{
  const char *msg;
  va_list argp;
  va_start(argp, em);
  msg = lj_str_pushvf(L, err2msg(em), argp);
  va_end(argp);
  err_argmsg(L, narg, msg);
}

/* Argument error. */
LJ_NOINLINE void lj_err_arg(lua_State *L, int narg, ErrMsg em)
{
  err_argmsg(L, narg, err2msg(em));
}

/* Typecheck error for arguments. */
LJ_NOINLINE void lj_err_argtype(lua_State *L, int narg, const char *xname)
{
  TValue *o = narg < 0 ? L->top + narg : L->base + narg-1;
  const char *tname = o < L->top ? lj_typename(o) : lj_obj_typename[0];
  const char *msg = lj_str_pushf(L, err2msg(LJ_ERR_BADTYPE), xname, tname);
  err_argmsg(L, narg, msg);
}

/* Typecheck error for arguments. */
LJ_NOINLINE void lj_err_argt(lua_State *L, int narg, int tt)
{
  lj_err_argtype(L, narg, lj_obj_typename[tt+1]);
}

/* -- Public error handling API ------------------------------------------- */

LUA_API lua_CFunction lua_atpanic(lua_State *L, lua_CFunction panicf)
{
  lua_CFunction old = G(L)->panic;
  G(L)->panic = panicf;
  return old;
}

/* Forwarders for the public API (C calling convention and no LJ_NORET). */
LUA_API int lua_error(lua_State *L)
{
  lj_err_run(L);
  return 0;  /* unreachable */
}

LUALIB_API int luaL_argerror(lua_State *L, int narg, const char *msg)
{
  err_argmsg(L, narg, msg);
  return 0;  /* unreachable */
}

LUALIB_API int luaL_typerror(lua_State *L, int narg, const char *xname)
{
  lj_err_argtype(L, narg, xname);
  return 0;  /* unreachable */
}

LUALIB_API void luaL_where(lua_State *L, int level)
{
  int size;
  cTValue *frame = lj_debug_frame(L, level, &size);
  lj_debug_addloc(L, "", frame, size ? frame+size : NULL);
}

LUALIB_API int luaL_error(lua_State *L, const char *fmt, ...)
{
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = lj_str_pushvf(L, fmt, argp);
  va_end(argp);
  lj_err_callermsg(L, msg);
  return 0;  /* unreachable */
}

