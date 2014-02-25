/*
** VM event handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_VMEVENT_H
#define _LJ_VMEVENT_H

#include "lj_obj.h"

/* Registry key for VM event handler table. */
#define LJ_VMEVENTS_REGKEY	"_VMEVENTS"
#define LJ_VMEVENTS_HSIZE	4

#define VMEVENT_MASK(ev)	((uint8_t)1 << ((int)(ev) & 7))
#define VMEVENT_HASH(ev)	((int)(ev) & ~7)
#define VMEVENT_HASHIDX(h)	((int)(h) << 3)
#define VMEVENT_NOCACHE		255

#define VMEVENT_DEF(name, hash) \
  LJ_VMEVENT_##name##_, \
  LJ_VMEVENT_##name = ((LJ_VMEVENT_##name##_) & 7)|((hash) << 3)

/* VM event IDs. */
typedef enum {
  VMEVENT_DEF(BC,	0x00003883),
  VMEVENT_DEF(TRACE,	0xb2d91467),
  VMEVENT_DEF(RECORD,	0x9284bf4f),
  VMEVENT_DEF(TEXIT,	0xb29df2b0),
  LJ_VMEVENT__MAX
} VMEvent;

#ifdef LUAJIT_DISABLE_VMEVENT
#define lj_vmevent_send(L, ev, args)		UNUSED(L)
#define lj_vmevent_send_(L, ev, args, post)	UNUSED(L)
#else
#define lj_vmevent_send(L, ev, args) \
  if (G(L)->vmevmask & VMEVENT_MASK(LJ_VMEVENT_##ev)) { \
    ptrdiff_t argbase = lj_vmevent_prepare(L, LJ_VMEVENT_##ev); \
    if (argbase) { \
      args \
      lj_vmevent_call(L, argbase); \
    } \
  }
#define lj_vmevent_send_(L, ev, args, post) \
  if (G(L)->vmevmask & VMEVENT_MASK(LJ_VMEVENT_##ev)) { \
    ptrdiff_t argbase = lj_vmevent_prepare(L, LJ_VMEVENT_##ev); \
    if (argbase) { \
      args \
      lj_vmevent_call(L, argbase); \
      post \
    } \
  }

LJ_FUNC ptrdiff_t lj_vmevent_prepare(lua_State *L, VMEvent ev);
LJ_FUNC void lj_vmevent_call(lua_State *L, ptrdiff_t argbase);
#endif

#endif
