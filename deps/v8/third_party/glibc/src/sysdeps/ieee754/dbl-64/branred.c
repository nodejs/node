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
/*******************************************************************/
/*                                                                 */
/* MODULE_NAME: branred.c                                          */
/*                                                                 */
/* FUNCTIONS:   branred                                            */
/*                                                                 */
/* FILES NEEDED: branred.h mydefs.h endian.h mpa.h                 */
/*               mha.c                                             */
/*                                                                 */
/* Routine  branred() performs range  reduction of a double number */
/* x into Double length number  a+aa,such that                     */
/* x=n*pi/2+(a+aa), abs(a+aa)<pi/4, n=0,+-1,+-2,....               */
/* Routine returns the integer (n mod 4) of the above description  */
/* of x.                                                           */
/*******************************************************************/

#include "endian.h"
#include "mydefs.h"
#include "branred.h"

#include <math.h>

#ifndef SECTION
# define SECTION
#endif


/*******************************************************************/
/* Routine  branred() performs range  reduction of a double number */
/* x into Double length number a+aa,such that                      */
/* x=n*pi/2+(a+aa), abs(a+aa)<pi/4, n=0,+-1,+-2,....               */
/* Routine return integer (n mod 4)                                */
/*******************************************************************/
int
SECTION
__branred(double x, double *a, double *aa)
{
  int i,k;
  mynumber  u,gor;
  double r[6],s,t,sum,b,bb,sum1,sum2,b1,bb1,b2,bb2,x1,x2,t1,t2;

  x*=tm600.x;
  t=x*split;   /* split x to two numbers */
  x1=t-(t-x);
  x2=x-x1;
  sum=0;
  u.x = x1;
  k = (u.i[HIGH_HALF]>>20)&2047;
  k = (k-450)/24;
  if (k<0)
    k=0;
  gor.x = t576.x;
  gor.i[HIGH_HALF] -= ((k*24)<<20);
  for (i=0;i<6;i++)
    { r[i] = x1*toverp[k+i]*gor.x; gor.x *= tm24.x; }
  for (i=0;i<3;i++) {
    s=(r[i]+big.x)-big.x;
    sum+=s;
    r[i]-=s;
  }
  t=0;
  for (i=0;i<6;i++)
    t+=r[5-i];
  bb=(((((r[0]-t)+r[1])+r[2])+r[3])+r[4])+r[5];
  s=(t+big.x)-big.x;
  sum+=s;
  t-=s;
  b=t+bb;
  bb=(t-b)+bb;
  s=(sum+big1.x)-big1.x;
  sum-=s;
  b1=b;
  bb1=bb;
  sum1=sum;
  sum=0;

  u.x = x2;
  k = (u.i[HIGH_HALF]>>20)&2047;
  k = (k-450)/24;
  if (k<0)
    k=0;
  gor.x = t576.x;
  gor.i[HIGH_HALF] -= ((k*24)<<20);
  for (i=0;i<6;i++)
    { r[i] = x2*toverp[k+i]*gor.x; gor.x *= tm24.x; }
  for (i=0;i<3;i++) {
    s=(r[i]+big.x)-big.x;
    sum+=s;
    r[i]-=s;
  }
  t=0;
  for (i=0;i<6;i++)
    t+=r[5-i];
  bb=(((((r[0]-t)+r[1])+r[2])+r[3])+r[4])+r[5];
  s=(t+big.x)-big.x;
 sum+=s;
 t-=s;
 b=t+bb;
 bb=(t-b)+bb;
 s=(sum+big1.x)-big1.x;
 sum-=s;

 b2=b;
 bb2=bb;
 sum2=sum;

 sum=sum1+sum2;
 b=b1+b2;
 bb = (fabs(b1)>fabs(b2))? (b1-b)+b2 : (b2-b)+b1;
 if (b > 0.5)
   {b-=1.0; sum+=1.0;}
 else if (b < -0.5)
   {b+=1.0; sum-=1.0;}
 s=b+(bb+bb1+bb2);
 t=((b-s)+bb)+(bb1+bb2);
 b=s*split;
 t1=b-(b-s);
 t2=s-t1;
 b=s*hp0.x;
 bb=(((t1*mp1.x-b)+t1*mp2.x)+t2*mp1.x)+(t2*mp2.x+s*hp1.x+t*hp0.x);
 s=b+bb;
 t=(b-s)+bb;
 *a=s;
 *aa=t;
 return ((int) sum)&3; /* return quater of unit circle */
}
