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

/******************************************************************/
/*                                                                */
/* MODULE_NAME:mydefs.h                                           */
/*                                                                */
/* common data and definition                                     */
/******************************************************************/

#ifndef MY_H
#define MY_H

typedef int int4;
typedef union { int4 i[2]; double x; double d; } mynumber;

#define max(x, y)  (((y) > (x)) ? (y) : (x))
#define min(x, y)  (((y) < (x)) ? (y) : (x))
#endif
