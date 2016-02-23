/*
 * Time operation macros based on sys/time.h
 * Copyright 2013 Balint Reczey <balint@balintreczey.hu>
 *
 * This file is part of libfaketime.
 *
 * libfaketime is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v2 as published by the Free
 * Software Foundation.
 *
 * libfaketime is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License v2 along
 * with libfaketime; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TIME_OPS_H
#define TIME_OPS_H
#include <time.h>

#define SEC_TO_uSEC 1000000
#define SEC_TO_nSEC 1000000000

/* Convenience macros for operations on timevals.
   NOTE: `timercmp' does not work for >= or <=.  */
#define timerisset2(tvp, prefix) ((tvp)->tv_sec || (tvp)->tv_##prefix##sec)
#define timerclear2(tvp, prefix) ((tvp)->tv_sec = (tvp)->tv_##prefix##sec = 0)
#define timercmp2(a, b, CMP, prefix)                                \
  (((a)->tv_sec == (b)->tv_sec) ?                                   \
   ((a)->tv_##prefix##sec CMP (b)->tv_##prefix##sec) :              \
   ((a)->tv_sec CMP (b)->tv_sec))
#define timeradd2(a, b, result, prefix)                             \
  do                                                                \
  {                                                                 \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;                   \
    (result)->tv_##prefix##sec = (a)->tv_##prefix##sec +            \
      (b)->tv_##prefix##sec;                                        \
    if ((result)->tv_##prefix##sec >= SEC_TO_##prefix##SEC)         \
      {                                                             \
        ++(result)->tv_sec;                                         \
        (result)->tv_##prefix##sec -= SEC_TO_##prefix##SEC;         \
      }                                                             \
  } while (0)
#define timersub2(a, b, result, prefix)                             \
  do                                                                \
  {                                                                 \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                   \
    (result)->tv_##prefix##sec = (a)->tv_##prefix##sec -            \
      (b)->tv_##prefix##sec;                                        \
    if ((result)->tv_##prefix##sec < 0)                             \
    {                                                               \
      --(result)->tv_sec;                                           \
      (result)->tv_##prefix##sec += SEC_TO_##prefix##SEC;           \
    }                                                               \
  } while (0)
#define timermul2(tvp, c, result, prefix)                           \
  do                                                                \
  {                                                                 \
    long long tmp_time;                                             \
    tmp_time = (c) * ((tvp)->tv_sec * SEC_TO_##prefix##SEC +        \
               (tvp)->tv_##prefix##sec);                            \
    (result)->tv_##prefix##sec = tmp_time % SEC_TO_##prefix##SEC;   \
    (result)->tv_sec = (tmp_time - (result)->tv_##prefix##sec) /    \
      SEC_TO_##prefix##SEC;                                         \
    if ((result)->tv_##prefix##sec < 0)                             \
    {                                                               \
      (result)->tv_##prefix##sec +=  SEC_TO_##prefix##SEC;          \
      (result)->tv_sec -= 1;                                        \
    }                                                               \
  } while (0)

/* ops for microsecs */
#ifndef timerisset
#define timerisset(tvp) timerisset2(tvp,u)
#endif
#ifndef timerclear
#define timerclear(tvp) timerclear2(tvp, u)
#endif
#ifndef timercmp
#define timercmp(a, b, CMP) timercmp2(a, b, CMP, u)
#endif
#ifndef timeradd
#define timeradd(a, b, result) timeradd2(a, b, result, u)
#endif
#ifndef timersub
#define timersub(a, b, result) timersub2(a, b, result, u)
#endif
#ifndef timersub
#define timermul(a, c, result) timermul2(a, c, result, u)
#endif

/* ops for nanosecs */
#define timespecisset(tvp) timerisset2(tvp,n)
#define timespecclear(tvp) timerclear2(tvp, n)
#define timespeccmp(a, b, CMP) timercmp2(a, b, CMP, n)
#define timespecadd(a, b, result) timeradd2(a, b, result, n)
#define timespecsub(a, b, result) timersub2(a, b, result, n)
#define timespecmul(a, c, result) timermul2(a, c, result, n)

#endif
