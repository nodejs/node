/* -----------------------------------------------------------------------
   ia64_flags.h - Copyright (c) 2000 Hewlett Packard Company
   
   IA64/unix Foreign Function Interface 

   Original author: Hans Boehm, HP Labs

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

/* "Type" codes used between assembly and C.  When used as a part of
   a cfi->flags value, the low byte will be these extra type codes,
   and bits 8-31 will be the actual size of the type.  */

/* Small structures containing N words in integer registers.  */
#define FFI_IA64_TYPE_SMALL_STRUCT	(FFI_TYPE_LAST + 1)

/* Homogeneous Floating Point Aggregates (HFAs) which are returned
   in FP registers.  */
#define FFI_IA64_TYPE_HFA_FLOAT		(FFI_TYPE_LAST + 2)
#define FFI_IA64_TYPE_HFA_DOUBLE	(FFI_TYPE_LAST + 3)
#define FFI_IA64_TYPE_HFA_LDOUBLE	(FFI_TYPE_LAST + 4)

/* Tripwire: the .Lst_table / .Lld_table return-value jump tables in unix.S place
   the FFI_IA64_TYPE_* pseudo-types (which are FFI_TYPE_LAST-relative) immediately
   after the generic FFI_TYPE_* codes.  Adding a new generic type bumps
   FFI_TYPE_LAST, shifts those codes, and desyncs the tables -- silently
   misdispatching small-struct/HFA returns.  When this fires: add a matching slot
   for the new type to both tables in unix.S, then bump FFI_IA64_TYPE_LAST.  */
#define FFI_IA64_TYPE_LAST FFI_TYPE_VECTOR
#if FFI_TYPE_LAST != FFI_IA64_TYPE_LAST
# error "new FFI_TYPE_* added: sync the unix.S jump tables and bump FFI_IA64_TYPE_LAST"
#endif
