/*
** C declaration parser.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_CPARSE_H
#define _LJ_CPARSE_H

#include "lj_obj.h"
#include "lj_ctype.h"

#if LJ_HASFFI

/* C parser limits. */
#define CPARSE_MAX_BUF		32768	/* Max. token buffer size. */
#define CPARSE_MAX_DECLSTACK	100	/* Max. declaration stack depth. */
#define CPARSE_MAX_DECLDEPTH	20	/* Max. recursive declaration depth. */
#define CPARSE_MAX_PACKSTACK	7	/* Max. pack pragma stack depth. */

/* Flags for C parser mode. */
#define CPARSE_MODE_MULTI	1	/* Process multiple declarations. */
#define CPARSE_MODE_ABSTRACT	2	/* Accept abstract declarators. */
#define CPARSE_MODE_DIRECT	4	/* Accept direct declarators. */
#define CPARSE_MODE_FIELD	8	/* Accept field width in bits, too. */
#define CPARSE_MODE_NOIMPLICIT	16	/* Reject implicit declarations. */
#define CPARSE_MODE_SKIP	32	/* Skip definitions, ignore errors. */

typedef int CPChar;	/* C parser character. Unsigned ext. from char. */
typedef int CPToken;	/* C parser token. */

/* C parser internal value representation. */
typedef struct CPValue {
  union {
    int32_t i32;	/* Value for CTID_INT32. */
    uint32_t u32;	/* Value for CTID_UINT32. */
  };
  CTypeID id;		/* C Type ID of the value. */
} CPValue;

/* C parser state. */
typedef struct CPState {
  CPChar c;		/* Current character. */
  CPToken tok;		/* Current token. */
  CPValue val;		/* Token value. */
  GCstr *str;		/* Interned string of identifier/keyword. */
  CType *ct;		/* C type table entry. */
  const char *p;	/* Current position in input buffer. */
  SBuf sb;		/* String buffer for tokens. */
  lua_State *L;		/* Lua state. */
  CTState *cts;		/* C type state. */
  TValue *param;	/* C type parameters. */
  const char *srcname;	/* Current source name. */
  BCLine linenumber;	/* Input line counter. */
  int depth;		/* Recursive declaration depth. */
  uint32_t tmask;	/* Type mask for next identifier. */
  uint32_t mode;	/* C parser mode. */
  uint8_t packstack[CPARSE_MAX_PACKSTACK];  /* Stack for pack pragmas. */
  uint8_t curpack;	/* Current position in pack pragma stack. */
} CPState;

LJ_FUNC int lj_cparse(CPState *cp);

#endif

#endif
