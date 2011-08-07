/*
 * context.h
 *
 * Description:
 * POSIX thread macros related to thread cancellation.
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999,2005 Pthreads-win32 contributors
 * 
 *      Contact Email: rpj@callisto.canberra.edu.au
 * 
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
 * 
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 * 
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 * 
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef PTW32_CONTEXT_H
#define PTW32_CONTEXT_H

#undef PTW32_PROGCTR

#if defined(_M_IX86) || (defined(_X86_) && !defined(__amd64__))
#define PTW32_PROGCTR(Context)  ((Context).Eip)
#endif

#if defined (_M_IA64) || defined(_IA64)
#define PTW32_PROGCTR(Context)  ((Context).StIIP)
#endif

#if defined(_MIPS_) || defined(MIPS)
#define PTW32_PROGCTR(Context)  ((Context).Fir)
#endif

#if defined(_ALPHA_)
#define PTW32_PROGCTR(Context)  ((Context).Fir)
#endif

#if defined(_PPC_)
#define PTW32_PROGCTR(Context)  ((Context).Iar)
#endif

#if defined(_AMD64_) || defined(__amd64__)
#define PTW32_PROGCTR(Context)  ((Context).Rip)
#endif

#if defined(_ARM_) || defined(ARM)
#define PTW32_PROGCTR(Context)  ((Context).Pc)
#endif

#if !defined(PTW32_PROGCTR)
#error Module contains CPU-specific code; modify and recompile.
#endif

#endif
