/*
** FFI C library loader.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"
#include "lj_str.h"
#include "lj_udata.h"
#include "lj_ctype.h"
#include "lj_cconv.h"
#include "lj_cdata.h"
#include "lj_clib.h"

/* -- OS-specific functions ----------------------------------------------- */

#if LJ_TARGET_DLOPEN

#include <dlfcn.h>
#include <stdio.h>

#if defined(RTLD_DEFAULT)
#define CLIB_DEFHANDLE	RTLD_DEFAULT
#elif LJ_TARGET_OSX || LJ_TARGET_BSD
#define CLIB_DEFHANDLE	((void *)(intptr_t)-2)
#else
#define CLIB_DEFHANDLE	NULL
#endif

LJ_NORET LJ_NOINLINE static void clib_error_(lua_State *L)
{
  lj_err_callermsg(L, dlerror());
}

#define clib_error(L, fmt, name)	clib_error_(L)

#if defined(__CYGWIN__)
#define CLIB_SOPREFIX	"cyg"
#else
#define CLIB_SOPREFIX	"lib"
#endif

#if LJ_TARGET_OSX
#define CLIB_SOEXT	"%s.dylib"
#elif defined(__CYGWIN__)
#define CLIB_SOEXT	"%s.dll"
#else
#define CLIB_SOEXT	"%s.so"
#endif

static const char *clib_extname(lua_State *L, const char *name)
{
  if (!strchr(name, '/')
#ifdef __CYGWIN__
      && !strchr(name, '\\')
#endif
     ) {
    if (!strchr(name, '.')) {
      name = lj_str_pushf(L, CLIB_SOEXT, name);
      L->top--;
#ifdef __CYGWIN__
    } else {
      return name;
#endif
    }
    if (!(name[0] == CLIB_SOPREFIX[0] && name[1] == CLIB_SOPREFIX[1] &&
	  name[2] == CLIB_SOPREFIX[2])) {
      name = lj_str_pushf(L, CLIB_SOPREFIX "%s", name);
      L->top--;
    }
  }
  return name;
}

/* Check for a recognized ld script line. */
static const char *clib_check_lds(lua_State *L, const char *buf)
{
  char *p, *e;
  if ((!strncmp(buf, "GROUP", 5) || !strncmp(buf, "INPUT", 5)) &&
      (p = strchr(buf, '('))) {
    while (*++p == ' ') ;
    for (e = p; *e && *e != ' ' && *e != ')'; e++) ;
    return strdata(lj_str_new(L, p, e-p));
  }
  return NULL;
}

/* Quick and dirty solution to resolve shared library name from ld script. */
static const char *clib_resolve_lds(lua_State *L, const char *name)
{
  FILE *fp = fopen(name, "r");
  const char *p = NULL;
  if (fp) {
    char buf[256];
    if (fgets(buf, sizeof(buf), fp)) {
      if (!strncmp(buf, "/* GNU ld script", 16)) {  /* ld script magic? */
	while (fgets(buf, sizeof(buf), fp)) {  /* Check all lines. */
	  p = clib_check_lds(L, buf);
	  if (p) break;
	}
      } else {  /* Otherwise check only the first line. */
	p = clib_check_lds(L, buf);
      }
    }
    fclose(fp);
  }
  return p;
}

static void *clib_loadlib(lua_State *L, const char *name, int global)
{
  void *h = dlopen(clib_extname(L, name),
		   RTLD_LAZY | (global?RTLD_GLOBAL:RTLD_LOCAL));
  if (!h) {
    const char *e, *err = dlerror();
    if (*err == '/' && (e = strchr(err, ':')) &&
	(name = clib_resolve_lds(L, strdata(lj_str_new(L, err, e-err))))) {
      h = dlopen(name, RTLD_LAZY | (global?RTLD_GLOBAL:RTLD_LOCAL));
      if (h) return h;
      err = dlerror();
    }
    lj_err_callermsg(L, err);
  }
  return h;
}

static void clib_unloadlib(CLibrary *cl)
{
  if (cl->handle && cl->handle != CLIB_DEFHANDLE)
    dlclose(cl->handle);
}

static void *clib_getsym(CLibrary *cl, const char *name)
{
  void *p = dlsym(cl->handle, name);
  return p;
}

#elif LJ_TARGET_WINDOWS

#define WIN32_LEAN_AND_MEAN
#ifndef WINVER
#define WINVER 0x0500
#endif
#include <windows.h>

#ifndef GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS	4
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT	2
BOOL WINAPI GetModuleHandleExA(DWORD, LPCSTR, HMODULE*);
#endif

#define CLIB_DEFHANDLE	((void *)-1)

/* Default libraries. */
enum {
  CLIB_HANDLE_EXE,
  CLIB_HANDLE_DLL,
  CLIB_HANDLE_CRT,
  CLIB_HANDLE_KERNEL32,
  CLIB_HANDLE_USER32,
  CLIB_HANDLE_GDI32,
  CLIB_HANDLE_MAX
};

static void *clib_def_handle[CLIB_HANDLE_MAX];

LJ_NORET LJ_NOINLINE static void clib_error(lua_State *L, const char *fmt,
					    const char *name)
{
  DWORD err = GetLastError();
  char buf[128];
  if (!FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_SYSTEM,
		      NULL, err, 0, buf, sizeof(buf), NULL))
    buf[0] = '\0';
  lj_err_callermsg(L, lj_str_pushf(L, fmt, name, buf));
}

static int clib_needext(const char *s)
{
  while (*s) {
    if (*s == '/' || *s == '\\' || *s == '.') return 0;
    s++;
  }
  return 1;
}

static const char *clib_extname(lua_State *L, const char *name)
{
  if (clib_needext(name)) {
    name = lj_str_pushf(L, "%s.dll", name);
    L->top--;
  }
  return name;
}

static void *clib_loadlib(lua_State *L, const char *name, int global)
{
  DWORD oldwerr = GetLastError();
  void *h = (void *)LoadLibraryA(clib_extname(L, name));
  if (!h) clib_error(L, "cannot load module " LUA_QS ": %s", name);
  SetLastError(oldwerr);
  UNUSED(global);
  return h;
}

static void clib_unloadlib(CLibrary *cl)
{
  if (cl->handle == CLIB_DEFHANDLE) {
    MSize i;
    for (i = CLIB_HANDLE_KERNEL32; i < CLIB_HANDLE_MAX; i++) {
      void *h = clib_def_handle[i];
      if (h) {
	clib_def_handle[i] = NULL;
	FreeLibrary((HINSTANCE)h);
      }
    }
  } else if (!cl->handle) {
    FreeLibrary((HINSTANCE)cl->handle);
  }
}

static void *clib_getsym(CLibrary *cl, const char *name)
{
  void *p = NULL;
  if (cl->handle == CLIB_DEFHANDLE) {  /* Search default libraries. */
    MSize i;
    for (i = 0; i < CLIB_HANDLE_MAX; i++) {
      HINSTANCE h = (HINSTANCE)clib_def_handle[i];
      if (!(void *)h) {  /* Resolve default library handles (once). */
	switch (i) {
	case CLIB_HANDLE_EXE: GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL, &h); break;
	case CLIB_HANDLE_DLL:
	  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			     (const char *)clib_def_handle, &h);
	  break;
	case CLIB_HANDLE_CRT:
	  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			     (const char *)&_fmode, &h);
	  break;
	case CLIB_HANDLE_KERNEL32: h = LoadLibraryA("kernel32.dll"); break;
	case CLIB_HANDLE_USER32: h = LoadLibraryA("user32.dll"); break;
	case CLIB_HANDLE_GDI32: h = LoadLibraryA("gdi32.dll"); break;
	}
	if (!h) continue;
	clib_def_handle[i] = (void *)h;
      }
      p = (void *)GetProcAddress(h, name);
      if (p) break;
    }
  } else {
    p = (void *)GetProcAddress((HINSTANCE)cl->handle, name);
  }
  return p;
}

#else

#define CLIB_DEFHANDLE	NULL

LJ_NORET LJ_NOINLINE static void clib_error(lua_State *L, const char *fmt,
					    const char *name)
{
  lj_err_callermsg(L, lj_str_pushf(L, fmt, name, "no support for this OS"));
}

static void *clib_loadlib(lua_State *L, const char *name, int global)
{
  lj_err_callermsg(L, "no support for loading dynamic libraries for this OS");
  UNUSED(name); UNUSED(global);
  return NULL;
}

static void clib_unloadlib(CLibrary *cl)
{
  UNUSED(cl);
}

static void *clib_getsym(CLibrary *cl, const char *name)
{
  UNUSED(cl); UNUSED(name);
  return NULL;
}

#endif

/* -- C library indexing -------------------------------------------------- */

#if LJ_TARGET_X86 && LJ_ABI_WIN
/* Compute argument size for fastcall/stdcall functions. */
static CTSize clib_func_argsize(CTState *cts, CType *ct)
{
  CTSize n = 0;
  while (ct->sib) {
    CType *d;
    ct = ctype_get(cts, ct->sib);
    if (ctype_isfield(ct->info)) {
      d = ctype_rawchild(cts, ct);
      n += ((d->size + 3) & ~3);
    }
  }
  return n;
}
#endif

/* Get redirected or mangled external symbol. */
static const char *clib_extsym(CTState *cts, CType *ct, GCstr *name)
{
  if (ct->sib) {
    CType *ctf = ctype_get(cts, ct->sib);
    if (ctype_isxattrib(ctf->info, CTA_REDIR))
      return strdata(gco2str(gcref(ctf->name)));
  }
  return strdata(name);
}

/* Index a C library by name. */
TValue *lj_clib_index(lua_State *L, CLibrary *cl, GCstr *name)
{
  TValue *tv = lj_tab_setstr(L, cl->cache, name);
  if (LJ_UNLIKELY(tvisnil(tv))) {
    CTState *cts = ctype_cts(L);
    CType *ct;
    CTypeID id = lj_ctype_getname(cts, &ct, name, CLNS_INDEX);
    if (!id)
      lj_err_callerv(L, LJ_ERR_FFI_NODECL, strdata(name));
    if (ctype_isconstval(ct->info)) {
      CType *ctt = ctype_child(cts, ct);
      lua_assert(ctype_isinteger(ctt->info) && ctt->size <= 4);
      if ((ctt->info & CTF_UNSIGNED) && (int32_t)ct->size < 0)
	setnumV(tv, (lua_Number)(uint32_t)ct->size);
      else
	setintV(tv, (int32_t)ct->size);
    } else {
      const char *sym = clib_extsym(cts, ct, name);
#if LJ_TARGET_WINDOWS
      DWORD oldwerr = GetLastError();
#endif
      void *p = clib_getsym(cl, sym);
      GCcdata *cd;
      lua_assert(ctype_isfunc(ct->info) || ctype_isextern(ct->info));
#if LJ_TARGET_X86 && LJ_ABI_WIN
      /* Retry with decorated name for fastcall/stdcall functions. */
      if (!p && ctype_isfunc(ct->info)) {
	CTInfo cconv = ctype_cconv(ct->info);
	if (cconv == CTCC_FASTCALL || cconv == CTCC_STDCALL) {
	  CTSize sz = clib_func_argsize(cts, ct);
	  const char *symd = lj_str_pushf(L,
			       cconv == CTCC_FASTCALL ? "@%s@%d" : "_%s@%d",
			       sym, sz);
	  L->top--;
	  p = clib_getsym(cl, symd);
	}
      }
#endif
      if (!p)
	clib_error(L, "cannot resolve symbol " LUA_QS ": %s", sym);
#if LJ_TARGET_WINDOWS
      SetLastError(oldwerr);
#endif
      cd = lj_cdata_new(cts, id, CTSIZE_PTR);
      *(void **)cdataptr(cd) = p;
      setcdataV(L, tv, cd);
    }
  }
  return tv;
}

/* -- C library management ------------------------------------------------ */

/* Create a new CLibrary object and push it on the stack. */
static CLibrary *clib_new(lua_State *L, GCtab *mt)
{
  GCtab *t = lj_tab_new(L, 0, 0);
  GCudata *ud = lj_udata_new(L, sizeof(CLibrary), t);
  CLibrary *cl = (CLibrary *)uddata(ud);
  cl->cache = t;
  ud->udtype = UDTYPE_FFI_CLIB;
  /* NOBARRIER: The GCudata is new (marked white). */
  setgcref(ud->metatable, obj2gco(mt));
  setudataV(L, L->top++, ud);
  return cl;
}

/* Load a C library. */
void lj_clib_load(lua_State *L, GCtab *mt, GCstr *name, int global)
{
  void *handle = clib_loadlib(L, strdata(name), global);
  CLibrary *cl = clib_new(L, mt);
  cl->handle = handle;
}

/* Unload a C library. */
void lj_clib_unload(CLibrary *cl)
{
  clib_unloadlib(cl);
  cl->handle = NULL;
}

/* Create the default C library object. */
void lj_clib_default(lua_State *L, GCtab *mt)
{
  CLibrary *cl = clib_new(L, mt);
  cl->handle = CLIB_DEFHANDLE;
}

#endif
