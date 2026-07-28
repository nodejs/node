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

/***********************************************************************/
/*MODULE_NAME: dla.h                                                   */
/*                                                                     */
/* This file holds C language macros for 'Double Length Floating Point */
/* Arithmetic'. The macros are based on the paper:                     */
/* T.J.Dekker, "A floating-point Technique for extending the           */
/* Available Precision", Number. Math. 18, 224-242 (1971).              */
/* A Double-Length number is defined by a pair (r,s), of IEEE double    */
/* precision floating point numbers that satisfy,                      */
/*                                                                     */
/*              abs(s) <= abs(r+s)*2**(-53)/(1+2**(-53)).              */
/*                                                                     */
/* The computer arithmetic assumed is IEEE double precision in         */
/* round to nearest mode. All variables in the macros must be of type  */
/* IEEE double.                                                        */
/***********************************************************************/

/* CN = 1+2**27 = '41a0000002000000' IEEE double format.  Use it to split a
   double for better accuracy.  */
#define  CN   134217729.0
