/*
** VM event handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include <stdio.h>

#define lj_vmevent_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_state.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_vmevent.h"

ptrdiff_t lj_vmevent_prepare(lua_State *L, VMEvent ev)
{
  global_State *g = G(L);
  GCstr *s = lj_str_newlit(L, LJ_VMEVENTS_REGKEY);
  cTValue *tv = lj_tab_getstr(tabV(registry(L)), s);
  if (tvistab(tv)) {
    int hash = VMEVENT_HASH(ev);
    tv = lj_tab_getint(tabV(tv), hash);
    if (tv && tvisfunc(tv)) {
      lj_state_checkstack(L, LUA_MINSTACK);
      setfuncV(L, L->top++, funcV(tv));
      return savestack(L, L->top);
    }
  }
  g->vmevmask &= ~VMEVENT_MASK(ev);  /* No handler: cache this fact. */
  return 0;
}

void lj_vmevent_call(lua_State *L, ptrdiff_t argbase)
{
  global_State *g = G(L);
  uint8_t oldmask = g->vmevmask;
  uint8_t oldh = hook_save(g);
  int status;
  g->vmevmask = 0;  /* Disable all events. */
  hook_vmevent(g);
  status = lj_vm_pcall(L, restorestack(L, argbase), 0+1, 0);
  if (LJ_UNLIKELY(status)) {
    /* Really shouldn't use stderr here, but where else to complain? */
    L->top--;
    fputs("VM handler failed: ", stderr);
    fputs(tvisstr(L->top) ? strVdata(L->top) : "?", stderr);
    fputc('\n', stderr);
  }
  hook_restore(g, oldh);
  if (g->vmevmask != VMEVENT_NOCACHE)
    g->vmevmask = oldmask;  /* Restore event mask, but not if not modified. */
}

