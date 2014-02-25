/*
** String scanning.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_STRSCAN_H
#define _LJ_STRSCAN_H

#include "lj_obj.h"

/* Options for accepted/returned formats. */
#define STRSCAN_OPT_TOINT	0x01  /* Convert to int32_t, if possible. */
#define STRSCAN_OPT_TONUM	0x02  /* Always convert to double. */
#define STRSCAN_OPT_IMAG	0x04
#define STRSCAN_OPT_LL		0x08
#define STRSCAN_OPT_C		0x10

/* Returned format. */
typedef enum {
  STRSCAN_ERROR,
  STRSCAN_NUM, STRSCAN_IMAG,
  STRSCAN_INT, STRSCAN_U32, STRSCAN_I64, STRSCAN_U64,
} StrScanFmt;

LJ_FUNC StrScanFmt lj_strscan_scan(const uint8_t *p, TValue *o, uint32_t opt);
LJ_FUNC int LJ_FASTCALL lj_strscan_num(GCstr *str, TValue *o);
#if LJ_DUALNUM
LJ_FUNC int LJ_FASTCALL lj_strscan_number(GCstr *str, TValue *o);
#else
#define lj_strscan_number(s, o)		lj_strscan_num((s), (o))
#endif

/* Check for number or convert string to number/int in-place (!). */
static LJ_AINLINE int lj_strscan_numberobj(TValue *o)
{
  return tvisnumber(o) || (tvisstr(o) && lj_strscan_number(strV(o), o));
}

#endif
