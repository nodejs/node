/*
** Bytecode dump definitions.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_BCDUMP_H
#define _LJ_BCDUMP_H

#include "lj_obj.h"
#include "lj_lex.h"

/* -- Bytecode dump format ------------------------------------------------ */

/*
** dump   = header proto+ 0U
** header = ESC 'L' 'J' versionB flagsU [namelenU nameB*]
** proto  = lengthU pdata
** pdata  = phead bcinsW* uvdataH* kgc* knum* [debugB*]
** phead  = flagsB numparamsB framesizeB numuvB numkgcU numknU numbcU
**          [debuglenU [firstlineU numlineU]]
** kgc    = kgctypeU { ktab | (loU hiU) | (rloU rhiU iloU ihiU) | strB* }
** knum   = intU0 | (loU1 hiU)
** ktab   = narrayU nhashU karray* khash*
** karray = ktabk
** khash  = ktabk ktabk
** ktabk  = ktabtypeU { intU | (loU hiU) | strB* }
**
** B = 8 bit, H = 16 bit, W = 32 bit, U = ULEB128 of W, U0/U1 = ULEB128 of W+1
*/

/* Bytecode dump header. */
#define BCDUMP_HEAD1		0x1b
#define BCDUMP_HEAD2		0x4c
#define BCDUMP_HEAD3		0x4a

/* If you perform *any* kind of private modifications to the bytecode itself
** or to the dump format, you *must* set BCDUMP_VERSION to 0x80 or higher.
*/
#define BCDUMP_VERSION		1

/* Compatibility flags. */
#define BCDUMP_F_BE		0x01
#define BCDUMP_F_STRIP		0x02
#define BCDUMP_F_FFI		0x04

#define BCDUMP_F_KNOWN		(BCDUMP_F_FFI*2-1)

/* Type codes for the GC constants of a prototype. Plus length for strings. */
enum {
  BCDUMP_KGC_CHILD, BCDUMP_KGC_TAB, BCDUMP_KGC_I64, BCDUMP_KGC_U64,
  BCDUMP_KGC_COMPLEX, BCDUMP_KGC_STR
};

/* Type codes for the keys/values of a constant table. */
enum {
  BCDUMP_KTAB_NIL, BCDUMP_KTAB_FALSE, BCDUMP_KTAB_TRUE,
  BCDUMP_KTAB_INT, BCDUMP_KTAB_NUM, BCDUMP_KTAB_STR
};

/* -- Bytecode reader/writer ---------------------------------------------- */

LJ_FUNC int lj_bcwrite(lua_State *L, GCproto *pt, lua_Writer writer,
		       void *data, int strip);
LJ_FUNC GCproto *lj_bcread(LexState *ls);

#endif
