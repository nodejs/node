/*
 * IBM Accurate Mathematical Library
 * Copyright (C) 2001-2022 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */
/************************************************************************/
/*  MODULE_NAME: branred.h                                              */
/*                                                                      */
/*                                                                      */
/* 	common data and variables definition for BIG or LITTLE ENDIAN   */
/************************************************************************/

#ifndef BRANRED_H
#define BRANRED_H

#include "dla.h"

#ifdef BIG_ENDI
static const mynumber

/**/           t576 = {{0x63f00000, 0x00000000}}, /* 2 ^ 576  */
/**/          tm600 = {{0x1a700000, 0x00000000}}, /* 2 ^- 600 */
/**/           tm24 = {{0x3e700000, 0x00000000}}, /* 2 ^- 24  */
/**/            big = {{0x43380000, 0x00000000}}, /*  6755399441055744      */
/**/           big1 = {{0x43580000, 0x00000000}}, /* 27021597764222976      */
/**/            hp0 = {{0x3FF921FB, 0x54442D18}} ,/* 1.5707963267948966     */
/**/            hp1 = {{0x3C91A626, 0x33145C07}} ,/* 6.123233995736766e-17  */
/**/            mp1 = {{0x3FF921FB, 0x58000000}}, /* 1.5707963407039642     */
/**/            mp2 = {{0xBE4DDE97, 0x40000000}}; /*-1.3909067675399456e-08 */

#else
#ifdef LITTLE_ENDI
static const mynumber

/**/           t576 = {{0x00000000, 0x63f00000}},  /* 2 ^ 576  */
/**/          tm600 = {{0x00000000, 0x1a700000}},  /* 2 ^- 600 */
/**/           tm24 = {{0x00000000, 0x3e700000}},  /* 2 ^- 24  */
/**/            big = {{0x00000000, 0x43380000}},  /*  6755399441055744      */
/**/           big1 = {{0x00000000, 0x43580000}},  /* 27021597764222976      */
/**/            hp0 = {{0x54442D18, 0x3FF921FB}},  /* 1.5707963267948966     */
/**/            hp1 = {{0x33145C07, 0x3C91A626}},  /* 6.123233995736766e-17  */
/**/            mp1 = {{0x58000000, 0x3FF921FB}},  /* 1.5707963407039642     */
/**/            mp2 = {{0x40000000, 0xBE4DDE97}};  /*-1.3909067675399456e-08 */

#endif
#endif

static const double toverp[75] = { /*  2/ PI base 24*/
  10680707.0,  7228996.0,  1387004.0,  2578385.0, 16069853.0,
  12639074.0,  9804092.0,  4427841.0, 16666979.0, 11263675.0,
  12935607.0,  2387514.0,  4345298.0, 14681673.0,  3074569.0,
  13734428.0, 16653803.0,  1880361.0, 10960616.0,  8533493.0,
   3062596.0,  8710556.0,  7349940.0,  6258241.0,  3772886.0,
   3769171.0,  3798172.0,  8675211.0, 12450088.0,  3874808.0,
   9961438.0,   366607.0, 15675153.0,  9132554.0,  7151469.0,
   3571407.0,  2607881.0, 12013382.0,  4155038.0,  6285869.0,
   7677882.0, 13102053.0, 15825725.0,   473591.0,  9065106.0,
  15363067.0,  6271263.0,  9264392.0,  5636912.0,  4652155.0,
   7056368.0, 13614112.0, 10155062.0,  1944035.0,  9527646.0,
  15080200.0,  6658437.0,  6231200.0,  6832269.0, 16767104.0,
   5075751.0,  3212806.0,  1398474.0,  7579849.0,  6349435.0,
  12618859.0,  4703257.0, 12806093.0, 14477321.0,  2786137.0,
  12875403.0,  9837734.0, 14528324.0, 13719321.0,   343717.0 };

static const double split =  CN;	/* 2^27 + 1 */

#endif
