/* -----------------------------------------------------------------------
   arcompact.S - Copyright (c) 2013 Synposys, Inc. (www.synopsys.com)
   
   ARCompact Foreign Function Interface 

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL RENESAS TECHNOLOGY BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

#define LIBFFI_ASM
#include <fficonfig.h>
#include <ffi.h>
#ifdef HAVE_MACHINE_ASM_H
#include <machine/asm.h>
#else
#define CNAME(x) x
#define ENTRY(x) .globl CNAME(x)` .type CNAME(x),%function` CNAME(x):
#endif

.text

        /* R0:   ffi_prep_args */
        /* R1:   &ecif */
        /* R2:   cif->bytes */
        /* R3:   fig->flags */
        /* R4:   ecif.rvalue */
        /* R5:   fn */
ENTRY(ffi_call_ARCompact)
        /* Save registers.  */
        st.a       fp, [sp, -4]        /* fp + 20, fp */
        push_s     blink               /* fp + 16, blink */
        st.a       r4, [sp, -4]        /* fp + 12, ecif.rvalue */
        push_s     r3                  /* fp +  8, fig->flags */
        st.a       r5, [sp, -4]        /* fp +  4, fn */
        push_s     r2                  /* fp +  0, cif->bytes */
        mov        fp, sp

        /* Make room for all of the new args.  */
        sub        sp, sp, r2

        /* Place all of the ffi_prep_args in position.  */
        /* ffi_prep_args(char *stack, extended_cif *ecif) */
        /* R1 already set.  */

        /* And call.  */
        jl_s.d     [r0]
        mov_s      r0, sp

        ld.ab      r12, [fp, 4]        /* cif->bytes */
        ld.ab      r11, [fp, 4]        /* fn */

        /* Move first 8 parameters in registers...  */
        ld_s       r0, [sp]
        ld_s       r1, [sp, 4]
        ld_s       r2, [sp, 8]
        ld_s       r3, [sp, 12]
        ld         r4, [sp, 16]
        ld         r5, [sp, 20]
        ld         r6, [sp, 24]
        ld         r7, [sp, 28]

        /* ...and adjust the stack.  */
        min        r12, r12, 32

        /* Call the function.  */
        jl.d       [r11]
        add        sp, sp, r12 

        mov        sp, fp        
        pop_s      r3        /* fig->flags, return type */
        pop_s      r2        /* ecif.rvalue, pointer for return value */

        /* If the return value pointer is NULL, assume no return value.  */
        breq.d     r2, 0, epilogue
        pop_s      blink

        /* Return INT.  */
        brne       r3, FFI_TYPE_INT, return_double
        b.d        epilogue
        st_s       r0, [r2]        

return_double:
        brne       r3, FFI_TYPE_DOUBLE, epilogue
        st_s       r0, [r2]        
        st_s       r1, [r2,4]

epilogue:
        j_s.d      [blink]
        ld.ab      fp, [sp, 4]

ENTRY(ffi_closure_ARCompact)
        st.a       r0, [sp, -32]
        st_s       r1, [sp, 4]
        st_s       r2, [sp, 8]
        st_s       r3, [sp, 12]
        st         r4, [sp, 16]
        st         r5, [sp, 20]
        st         r6, [sp, 24]
        st         r7, [sp, 28]

        /* pointer to arguments */
        mov_s      r2, sp

        /* return value goes here */
        sub        sp, sp, 8
        mov_s      r1, sp

        push_s     blink
        
        bl.d       ffi_closure_inner_ARCompact
        mov_s      r0, r8                /* codeloc, set by trampoline */

        pop_s      blink

        /* set return value to r1:r0 */
        pop_s      r0
        pop_s      r1
        j_s.d      [blink]
        add_s      sp, sp, 32
