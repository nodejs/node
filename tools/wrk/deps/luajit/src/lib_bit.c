/*
** Bit manipulation library.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lib_bit_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_lib.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_bit

LJLIB_ASM(bit_tobit)		LJLIB_REC(bit_unary IR_TOBIT)
{
  lj_lib_checknumber(L, 1);
  return FFH_RETRY;
}
LJLIB_ASM_(bit_bnot)		LJLIB_REC(bit_unary IR_BNOT)
LJLIB_ASM_(bit_bswap)		LJLIB_REC(bit_unary IR_BSWAP)

LJLIB_ASM(bit_lshift)		LJLIB_REC(bit_shift IR_BSHL)
{
  lj_lib_checknumber(L, 1);
  lj_lib_checkbit(L, 2);
  return FFH_RETRY;
}
LJLIB_ASM_(bit_rshift)		LJLIB_REC(bit_shift IR_BSHR)
LJLIB_ASM_(bit_arshift)		LJLIB_REC(bit_shift IR_BSAR)
LJLIB_ASM_(bit_rol)		LJLIB_REC(bit_shift IR_BROL)
LJLIB_ASM_(bit_ror)		LJLIB_REC(bit_shift IR_BROR)

LJLIB_ASM(bit_band)		LJLIB_REC(bit_nary IR_BAND)
{
  int i = 0;
  do { lj_lib_checknumber(L, ++i); } while (L->base+i < L->top);
  return FFH_RETRY;
}
LJLIB_ASM_(bit_bor)		LJLIB_REC(bit_nary IR_BOR)
LJLIB_ASM_(bit_bxor)		LJLIB_REC(bit_nary IR_BXOR)

/* ------------------------------------------------------------------------ */

LJLIB_CF(bit_tohex)
{
  uint32_t b = (uint32_t)lj_lib_checkbit(L, 1);
  int32_t i, n = L->base+1 >= L->top ? 8 : lj_lib_checkbit(L, 2);
  const char *hexdigits = "0123456789abcdef";
  char buf[8];
  if (n < 0) { n = -n; hexdigits = "0123456789ABCDEF"; }
  if (n > 8) n = 8;
  for (i = n; --i >= 0; ) { buf[i] = hexdigits[b & 15]; b >>= 4; }
  lua_pushlstring(L, buf, (size_t)n);
  return 1;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_bit(lua_State *L)
{
  LJ_LIB_REG(L, LUA_BITLIBNAME, bit);
  return 1;
}

