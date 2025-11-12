/*
 * IBM Accurate Mathematical Library
 * written by International Business Machines Corp.
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
 * You should have received a copy of the GNU  Lesser General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */
/****************************************************************************/
/*                                                                          */
/* MODULE_NAME:usncs.c                                                      */
/*                                                                          */
/* FUNCTIONS: usin                                                          */
/*            ucos                                                          */
/* FILES NEEDED: dla.h endian.h mpa.h mydefs.h  usncs.h                     */
/*		 branred.c sincos.tbl					    */
/*                                                                          */
/* An ultimate sin and cos routine. Given an IEEE double machine number x   */
/* it computes sin(x) or cos(x) with ~0.55 ULP.				    */
/* Assumption: Machine arithmetic operations are performed in               */
/* round to nearest mode of IEEE 754 standard.                              */
/*                                                                          */
/****************************************************************************/


#include <errno.h>
#include <float.h>
#include "endian.h"
#include "mydefs.h"
#include "usncs.h"
#include <math.h>

#define attribute_hidden
#if !defined(__always_inline)
#define __always_inline
#endif

/* Helper macros to compute sin of the input values.  */
#define POLYNOMIAL2(xx) ((((s5 * (xx) + s4) * (xx) + s3) * (xx) + s2) * (xx))

#define POLYNOMIAL(xx) (POLYNOMIAL2 (xx) + s1)

/* The computed polynomial is a variation of the Taylor series expansion for
   sin(x):

   x - x^3/3! + x^5/5! - x^7/7! + x^9/9! - dx*x^2/2 + dx

   The constants s1, s2, s3, etc. are pre-computed values of 1/3!, 1/5! and so
   on.  The result is returned to LHS.  */
#define TAYLOR_SIN(xx, x, dx) \
({									      \
  double t = ((POLYNOMIAL (xx)  * (x) - 0.5 * (dx))  * (xx) + (dx));	      \
  double res = (x) + t;							      \
  res;									      \
})

#define SINCOS_TABLE_LOOKUP(u, sn, ssn, cs, ccs) \
({									      \
  int4 k = u.i[LOW_HALF] << 2;						      \
  sn = __sincostab.x[k];						      \
  ssn = __sincostab.x[k + 1];						      \
  cs = __sincostab.x[k + 2];						      \
  ccs = __sincostab.x[k + 3];						      \
})

#ifndef SECTION
# define SECTION
#endif

extern const union
{
  int4 i[880];
  double x[440];
} __sincostab attribute_hidden;

static const double
  sn3 = -1.66666666666664880952546298448555E-01,
  sn5 = 8.33333214285722277379541354343671E-03,
  cs2 = 4.99999999999999999999950396842453E-01,
  cs4 = -4.16666666666664434524222570944589E-02,
  cs6 = 1.38888874007937613028114285595617E-03;

int __branred (double x, double *a, double *aa);

/* Given a number partitioned into X and DX, this function computes the cosine
   of the number by combining the sin and cos of X (as computed by a variation
   of the Taylor series) with the values looked up from the sin/cos table to
   get the result.  */
static __always_inline double
do_cos (double x, double dx)
{
  mynumber u;

  if (x < 0)
    dx = -dx;

  u.x = big + fabs (x);
  x = fabs (x) - (u.x - big) + dx;

  double xx, s, sn, ssn, c, cs, ccs, cor;
  xx = x * x;
  s = x + x * xx * (sn3 + xx * sn5);
  c = xx * (cs2 + xx * (cs4 + xx * cs6));
  SINCOS_TABLE_LOOKUP (u, sn, ssn, cs, ccs);
  cor = (ccs - s * ssn - cs * c) - sn * s;
  return cs + cor;
}

/* Given a number partitioned into X and DX, this function computes the sine of
   the number by combining the sin and cos of X (as computed by a variation of
   the Taylor series) with the values looked up from the sin/cos table to get
   the result.  */
static __always_inline double
do_sin (double x, double dx)
{
  double xold = x;
  /* Max ULP is 0.501 if |x| < 0.126, otherwise ULP is 0.518.  */
  if (fabs (x) < 0.126)
    return TAYLOR_SIN (x * x, x, dx);

  mynumber u;

  if (x <= 0)
    dx = -dx;
  u.x = big + fabs (x);
  x = fabs (x) - (u.x - big);

  double xx, s, sn, ssn, c, cs, ccs, cor;
  xx = x * x;
  s = x + (dx + x * xx * (sn3 + xx * sn5));
  c = x * dx + xx * (cs2 + xx * (cs4 + xx * cs6));
  SINCOS_TABLE_LOOKUP (u, sn, ssn, cs, ccs);
  cor = (ssn + s * ccs - sn * c) + cs * s;
  return copysign (sn + cor, xold);
}

/* Reduce range of x to within PI/2 with abs (x) < 105414350.  The high part
   is written to *a, the low part to *da.  Range reduction is accurate to 136
   bits so that when x is large and *a very close to zero, all 53 bits of *a
   are correct.  */
static __always_inline int4
reduce_sincos (double x, double *a, double *da)
{
  mynumber v;

  double t = (x * hpinv + toint);
  double xn = t - toint;
  v.x = t;
  double y = (x - xn * mp1) - xn * mp2;
  int4 n = v.i[LOW_HALF] & 3;

  double b, db, t1, t2;
  t1 = xn * pp3;
  t2 = y - t1;
  db = (y - t2) - t1;

  t1 = xn * pp4;
  b = t2 - t1;
  db += (t2 - b) - t1;

  *a = b;
  *da = db;
  return n;
}

/* Compute sin or cos (A + DA) for the given quadrant N.  */
static __always_inline double
do_sincos (double a, double da, int4 n)
{
  double retval;

  if (n & 1)
    /* Max ULP is 0.513.  */
    retval = do_cos (a, da);
  else
    /* Max ULP is 0.501 if xx < 0.01588, otherwise ULP is 0.518.  */
    retval = do_sin (a, da);

  return (n & 2) ? -retval : retval;
}


/*******************************************************************/
/* An ultimate sin routine. Given an IEEE double machine number x  */
/* it computes the rounded value of sin(x).			   */
/*******************************************************************/
#ifndef IN_SINCOS
double
SECTION
glibc_sin (double x)
{
  double t, a, da;
  mynumber u;
  int4 k, m, n;
  double retval = 0;

  u.x = x;
  m = u.i[HIGH_HALF];
  k = 0x7fffffff & m;		/* no sign           */
  if (k < 0x3e500000)		/* if x->0 =>sin(x)=x */
    {
      retval = x;
    }
/*--------------------------- 2^-26<|x|< 0.855469---------------------- */
  else if (k < 0x3feb6000)
    {
      /* Max ULP is 0.548.  */
      retval = do_sin (x, 0);
    }				/*   else  if (k < 0x3feb6000)    */

/*----------------------- 0.855469  <|x|<2.426265  ----------------------*/
  else if (k < 0x400368fd)
    {
      t = hp0 - fabs (x);
      /* Max ULP is 0.51.  */
      retval = copysign (do_cos (t, hp1), x);
    }				/*   else  if (k < 0x400368fd)    */

/*-------------------------- 2.426265<|x|< 105414350 ----------------------*/
  else if (k < 0x419921FB)
    {
      n = reduce_sincos (x, &a, &da);
      retval = do_sincos (a, da, n);
    }				/*   else  if (k <  0x419921FB )    */

/* --------------------105414350 <|x| <2^1024------------------------------*/
  else if (k < 0x7ff00000)
    {
      n = __branred (x, &a, &da);
      retval = do_sincos (a, da, n);
    }
/*--------------------- |x| > 2^1024 ----------------------------------*/
  else
    {
      retval = x / x;
    }

  return retval;
}


/*******************************************************************/
/* An ultimate cos routine. Given an IEEE double machine number x  */
/* it computes the rounded value of cos(x).			   */
/*******************************************************************/

double
SECTION
glibc_cos (double x)
{
  double y, a, da;
  mynumber u;
  int4 k, m, n;

  double retval = 0;

  u.x = x;
  m = u.i[HIGH_HALF];
  k = 0x7fffffff & m;

  /* |x|<2^-27 => cos(x)=1 */
  if (k < 0x3e400000)
    retval = 1.0;

  else if (k < 0x3feb6000)
    {				/* 2^-27 < |x| < 0.855469 */
      /* Max ULP is 0.51.  */
      retval = do_cos (x, 0);
    }				/*   else  if (k < 0x3feb6000)    */

  else if (k < 0x400368fd)
    { /* 0.855469  <|x|<2.426265  */ ;
      y = hp0 - fabs (x);
      a = y + hp1;
      da = (y - a) + hp1;
      /* Max ULP is 0.501 if xx < 0.01588 or 0.518 otherwise.
	 Range reduction uses 106 bits here which is sufficient.  */
      retval = do_sin (a, da);
    }				/*   else  if (k < 0x400368fd)    */

  else if (k < 0x419921FB)
    {				/* 2.426265<|x|< 105414350 */
      n = reduce_sincos (x, &a, &da);
      retval = do_sincos (a, da, n + 1);
    }				/*   else  if (k <  0x419921FB )    */

  /* 105414350 <|x| <2^1024 */
  else if (k < 0x7ff00000)
    {
      n = __branred (x, &a, &da);
      retval = do_sincos (a, da, n + 1);
    }

  else
    {
      retval = x / x;		/* |x| > 2^1024 */
    }

  return retval;
}

#endif
