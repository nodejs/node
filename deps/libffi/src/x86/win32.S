/* -----------------------------------------------------------------------
   win32.S - Copyright (c) 1996, 1998, 2001, 2002, 2009  Red Hat, Inc.
	     Copyright (c) 2001  John Beniton
	     Copyright (c) 2002  Ranjit Mathew
	     Copyright (c) 2009  Daniel Witte
			
 
   X86 Foreign Function Interface
 
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
   -----------------------------------------------------------------------
   */
 
#define LIBFFI_ASM
#include <fficonfig.h>
#include <ffi.h>

#ifdef _MSC_VER

.386
.MODEL FLAT, C

EXTRN ffi_closure_SYSV_inner:NEAR

_TEXT SEGMENT

ffi_call_win32 PROC NEAR,
    ffi_prep_args : NEAR PTR DWORD,
    ecif          : NEAR PTR DWORD,
    cif_abi       : DWORD,
    cif_bytes     : DWORD,
    cif_flags     : DWORD,
    rvalue        : NEAR PTR DWORD,
    fn            : NEAR PTR DWORD

        ;; Make room for all of the new args.
        mov  ecx, cif_bytes
        sub  esp, ecx

        mov  eax, esp

        ;; Place all of the ffi_prep_args in position
        push ecif
        push eax
        call ffi_prep_args

        ;; Return stack to previous state and call the function
        add  esp, 8

	;; Handle thiscall and fastcall
	cmp cif_abi, 3 ;; FFI_THISCALL
	jz do_thiscall
	cmp cif_abi, 4 ;; FFI_FASTCALL
	jnz do_stdcall
	mov ecx, DWORD PTR [esp]
	mov edx, DWORD PTR [esp+4]
	add esp, 8
	jmp do_stdcall
do_thiscall:
	mov ecx, DWORD PTR [esp]
	add esp, 4
do_stdcall:
        call fn

        ;; cdecl:   we restore esp in the epilogue, so there's no need to
        ;;          remove the space we pushed for the args.
        ;; stdcall: the callee has already cleaned the stack.

        ;; Load ecx with the return type code
        mov  ecx, cif_flags

        ;; If the return value pointer is NULL, assume no return value.
        cmp  rvalue, 0
        jne  ca_jumptable

        ;; Even if there is no space for the return value, we are
        ;; obliged to handle floating-point values.
        cmp  ecx, FFI_TYPE_FLOAT
        jne  ca_epilogue
        fstp st(0)

        jmp  ca_epilogue

ca_jumptable:
        jmp  [ca_jumpdata + 4 * ecx]
ca_jumpdata:
        ;; Do not insert anything here between label and jump table.
        dd offset ca_epilogue       ;; FFI_TYPE_VOID
        dd offset ca_retint         ;; FFI_TYPE_INT
        dd offset ca_retfloat       ;; FFI_TYPE_FLOAT
        dd offset ca_retdouble      ;; FFI_TYPE_DOUBLE
        dd offset ca_retlongdouble  ;; FFI_TYPE_LONGDOUBLE
        dd offset ca_retuint8       ;; FFI_TYPE_UINT8
        dd offset ca_retsint8       ;; FFI_TYPE_SINT8
        dd offset ca_retuint16      ;; FFI_TYPE_UINT16
        dd offset ca_retsint16      ;; FFI_TYPE_SINT16
        dd offset ca_retint         ;; FFI_TYPE_UINT32
        dd offset ca_retint         ;; FFI_TYPE_SINT32
        dd offset ca_retint64       ;; FFI_TYPE_UINT64
        dd offset ca_retint64       ;; FFI_TYPE_SINT64
        dd offset ca_epilogue       ;; FFI_TYPE_STRUCT
        dd offset ca_retint         ;; FFI_TYPE_POINTER
        dd offset ca_retstruct1b    ;; FFI_TYPE_SMALL_STRUCT_1B
        dd offset ca_retstruct2b    ;; FFI_TYPE_SMALL_STRUCT_2B
        dd offset ca_retint         ;; FFI_TYPE_SMALL_STRUCT_4B
        dd offset ca_epilogue       ;; FFI_TYPE_MS_STRUCT

        /* Sign/zero extend as appropriate.  */
ca_retuint8:
        movzx eax, al
        jmp   ca_retint

ca_retsint8:
        movsx eax, al
        jmp   ca_retint

ca_retuint16:
        movzx eax, ax
        jmp   ca_retint

ca_retsint16:
        movsx eax, ax
        jmp   ca_retint

ca_retint:
        ;; Load %ecx with the pointer to storage for the return value
        mov   ecx, rvalue
        mov   [ecx + 0], eax
        jmp   ca_epilogue

ca_retint64:
        ;; Load %ecx with the pointer to storage for the return value
        mov   ecx, rvalue
        mov   [ecx + 0], eax
        mov   [ecx + 4], edx
        jmp   ca_epilogue

ca_retfloat:
        ;; Load %ecx with the pointer to storage for the return value
        mov   ecx, rvalue
        fstp  DWORD PTR [ecx]
        jmp   ca_epilogue

ca_retdouble:
        ;; Load %ecx with the pointer to storage for the return value
        mov   ecx, rvalue
        fstp  QWORD PTR [ecx]
        jmp   ca_epilogue

ca_retlongdouble:
        ;; Load %ecx with the pointer to storage for the return value
        mov   ecx, rvalue
        fstp  TBYTE PTR [ecx]
        jmp   ca_epilogue

ca_retstruct1b:
        ;; Load %ecx with the pointer to storage for the return value
        mov   ecx, rvalue
        mov   [ecx + 0], al
        jmp   ca_epilogue

ca_retstruct2b:
        ;; Load %ecx with the pointer to storage for the return value
        mov   ecx, rvalue
        mov   [ecx + 0], ax
        jmp   ca_epilogue

ca_epilogue:
        ;; Epilogue code is autogenerated.
        ret
ffi_call_win32 ENDP

ffi_closure_THISCALL PROC NEAR FORCEFRAME
	sub	esp, 40
	lea	edx, [ebp -24]
	mov	[ebp - 12], edx	/* resp */
	lea	edx, [ebp + 12]  /* account for stub return address on stack */
	jmp	stub
ffi_closure_THISCALL ENDP

ffi_closure_SYSV PROC NEAR FORCEFRAME
    ;; the ffi_closure ctx is passed in eax by the trampoline.

        sub  esp, 40
        lea  edx, [ebp - 24]
        mov  [ebp - 12], edx         ;; resp
        lea  edx, [ebp + 8]
stub::
        mov  [esp + 8], edx          ;; args
        lea  edx, [ebp - 12]
        mov  [esp + 4], edx          ;; &resp
        mov  [esp], eax              ;; closure
        call ffi_closure_SYSV_inner
        mov  ecx, [ebp - 12]

cs_jumptable:
        jmp  [cs_jumpdata + 4 * eax]
cs_jumpdata:
        ;; Do not insert anything here between the label and jump table.
        dd offset cs_epilogue       ;; FFI_TYPE_VOID
        dd offset cs_retint         ;; FFI_TYPE_INT
        dd offset cs_retfloat       ;; FFI_TYPE_FLOAT
        dd offset cs_retdouble      ;; FFI_TYPE_DOUBLE
        dd offset cs_retlongdouble  ;; FFI_TYPE_LONGDOUBLE
        dd offset cs_retuint8       ;; FFI_TYPE_UINT8
        dd offset cs_retsint8       ;; FFI_TYPE_SINT8
        dd offset cs_retuint16      ;; FFI_TYPE_UINT16
        dd offset cs_retsint16      ;; FFI_TYPE_SINT16
        dd offset cs_retint         ;; FFI_TYPE_UINT32
        dd offset cs_retint         ;; FFI_TYPE_SINT32
        dd offset cs_retint64       ;; FFI_TYPE_UINT64
        dd offset cs_retint64       ;; FFI_TYPE_SINT64
        dd offset cs_retstruct      ;; FFI_TYPE_STRUCT
        dd offset cs_retint         ;; FFI_TYPE_POINTER
        dd offset cs_retsint8       ;; FFI_TYPE_SMALL_STRUCT_1B
        dd offset cs_retsint16      ;; FFI_TYPE_SMALL_STRUCT_2B
        dd offset cs_retint         ;; FFI_TYPE_SMALL_STRUCT_4B
        dd offset cs_retmsstruct    ;; FFI_TYPE_MS_STRUCT

cs_retuint8:
        movzx eax, BYTE PTR [ecx]
        jmp   cs_epilogue

cs_retsint8:
        movsx eax, BYTE PTR [ecx]
        jmp   cs_epilogue

cs_retuint16:
        movzx eax, WORD PTR [ecx]
        jmp   cs_epilogue

cs_retsint16:
        movsx eax, WORD PTR [ecx]
        jmp   cs_epilogue

cs_retint:
        mov   eax, [ecx]
        jmp   cs_epilogue

cs_retint64:
        mov   eax, [ecx + 0]
        mov   edx, [ecx + 4]
        jmp   cs_epilogue

cs_retfloat:
        fld   DWORD PTR [ecx]
        jmp   cs_epilogue

cs_retdouble:
        fld   QWORD PTR [ecx]
        jmp   cs_epilogue

cs_retlongdouble:
        fld   TBYTE PTR [ecx]
        jmp   cs_epilogue

cs_retstruct:
        ;; Caller expects us to pop struct return value pointer hidden arg.
        ;; Epilogue code is autogenerated.
        ret	4

cs_retmsstruct:
        ;; Caller expects us to return a pointer to the real return value.
        mov   eax, ecx
        ;; Caller doesn't expects us to pop struct return value pointer hidden arg.
        jmp   cs_epilogue

cs_epilogue:
        ;; Epilogue code is autogenerated.
        ret
ffi_closure_SYSV ENDP

#if !FFI_NO_RAW_API

#define RAW_CLOSURE_CIF_OFFSET ((FFI_TRAMPOLINE_SIZE + 3) AND NOT 3)
#define RAW_CLOSURE_FUN_OFFSET (RAW_CLOSURE_CIF_OFFSET + 4)
#define RAW_CLOSURE_USER_DATA_OFFSET (RAW_CLOSURE_FUN_OFFSET + 4)
#define CIF_FLAGS_OFFSET 20

ffi_closure_raw_THISCALL PROC NEAR USES esi FORCEFRAME
	sub esp, 36
	mov  esi, [eax + RAW_CLOSURE_CIF_OFFSET]        ;; closure->cif
	mov  edx, [eax + RAW_CLOSURE_USER_DATA_OFFSET]  ;; closure->user_data
	mov [esp + 12], edx
	lea edx, [ebp + 12]
	jmp stubraw
ffi_closure_raw_THISCALL ENDP

ffi_closure_raw_SYSV PROC NEAR USES esi FORCEFRAME
    ;; the ffi_closure ctx is passed in eax by the trampoline.

        sub  esp, 40
        mov  esi, [eax + RAW_CLOSURE_CIF_OFFSET]        ;; closure->cif
        mov  edx, [eax + RAW_CLOSURE_USER_DATA_OFFSET]  ;; closure->user_data
        mov  [esp + 12], edx                            ;; user_data
        lea  edx, [ebp + 8]
stubraw::
        mov  [esp + 8], edx                             ;; raw_args
        lea  edx, [ebp - 24]
        mov  [esp + 4], edx                             ;; &res
        mov  [esp], esi                                 ;; cif
        call DWORD PTR [eax + RAW_CLOSURE_FUN_OFFSET]   ;; closure->fun
        mov  eax, [esi + CIF_FLAGS_OFFSET]              ;; cif->flags
        lea  ecx, [ebp - 24]

cr_jumptable:
        jmp  [cr_jumpdata + 4 * eax]
cr_jumpdata:
        ;; Do not insert anything here between the label and jump table.
        dd offset cr_epilogue       ;; FFI_TYPE_VOID
        dd offset cr_retint         ;; FFI_TYPE_INT
        dd offset cr_retfloat       ;; FFI_TYPE_FLOAT
        dd offset cr_retdouble      ;; FFI_TYPE_DOUBLE
        dd offset cr_retlongdouble  ;; FFI_TYPE_LONGDOUBLE
        dd offset cr_retuint8       ;; FFI_TYPE_UINT8
        dd offset cr_retsint8       ;; FFI_TYPE_SINT8
        dd offset cr_retuint16      ;; FFI_TYPE_UINT16
        dd offset cr_retsint16      ;; FFI_TYPE_SINT16
        dd offset cr_retint         ;; FFI_TYPE_UINT32
        dd offset cr_retint         ;; FFI_TYPE_SINT32
        dd offset cr_retint64       ;; FFI_TYPE_UINT64
        dd offset cr_retint64       ;; FFI_TYPE_SINT64
        dd offset cr_epilogue       ;; FFI_TYPE_STRUCT
        dd offset cr_retint         ;; FFI_TYPE_POINTER
        dd offset cr_retsint8       ;; FFI_TYPE_SMALL_STRUCT_1B
        dd offset cr_retsint16      ;; FFI_TYPE_SMALL_STRUCT_2B
        dd offset cr_retint         ;; FFI_TYPE_SMALL_STRUCT_4B
        dd offset cr_epilogue       ;; FFI_TYPE_MS_STRUCT

cr_retuint8:
        movzx eax, BYTE PTR [ecx]
        jmp   cr_epilogue

cr_retsint8:
        movsx eax, BYTE PTR [ecx]
        jmp   cr_epilogue

cr_retuint16:
        movzx eax, WORD PTR [ecx]
        jmp   cr_epilogue

cr_retsint16:
        movsx eax, WORD PTR [ecx]
        jmp   cr_epilogue

cr_retint:
        mov   eax, [ecx]
        jmp   cr_epilogue

cr_retint64:
        mov   eax, [ecx + 0]
        mov   edx, [ecx + 4]
        jmp   cr_epilogue

cr_retfloat:
        fld   DWORD PTR [ecx]
        jmp   cr_epilogue

cr_retdouble:
        fld   QWORD PTR [ecx]
        jmp   cr_epilogue

cr_retlongdouble:
        fld   TBYTE PTR [ecx]
        jmp   cr_epilogue

cr_epilogue:
        ;; Epilogue code is autogenerated.
        ret
ffi_closure_raw_SYSV ENDP

#endif /* !FFI_NO_RAW_API */

ffi_closure_STDCALL PROC NEAR FORCEFRAME
    ;; the ffi_closure ctx is passed in eax by the trampoline.

        sub  esp, 40
        lea  edx, [ebp - 24]
        mov  [ebp - 12], edx         ;; resp
        lea  edx, [ebp + 12]         ;; account for stub return address on stack
        mov  [esp + 8], edx          ;; args
        lea  edx, [ebp - 12]
        mov  [esp + 4], edx          ;; &resp
        mov  [esp], eax              ;; closure
        call ffi_closure_SYSV_inner
        mov  ecx, [ebp - 12]

cd_jumptable:
        jmp  [cd_jumpdata + 4 * eax]
cd_jumpdata:
        ;; Do not insert anything here between the label and jump table.
        dd offset cd_epilogue       ;; FFI_TYPE_VOID
        dd offset cd_retint         ;; FFI_TYPE_INT
        dd offset cd_retfloat       ;; FFI_TYPE_FLOAT
        dd offset cd_retdouble      ;; FFI_TYPE_DOUBLE
        dd offset cd_retlongdouble  ;; FFI_TYPE_LONGDOUBLE
        dd offset cd_retuint8       ;; FFI_TYPE_UINT8
        dd offset cd_retsint8       ;; FFI_TYPE_SINT8
        dd offset cd_retuint16      ;; FFI_TYPE_UINT16
        dd offset cd_retsint16      ;; FFI_TYPE_SINT16
        dd offset cd_retint         ;; FFI_TYPE_UINT32
        dd offset cd_retint         ;; FFI_TYPE_SINT32
        dd offset cd_retint64       ;; FFI_TYPE_UINT64
        dd offset cd_retint64       ;; FFI_TYPE_SINT64
        dd offset cd_epilogue       ;; FFI_TYPE_STRUCT
        dd offset cd_retint         ;; FFI_TYPE_POINTER
        dd offset cd_retsint8       ;; FFI_TYPE_SMALL_STRUCT_1B
        dd offset cd_retsint16      ;; FFI_TYPE_SMALL_STRUCT_2B
        dd offset cd_retint         ;; FFI_TYPE_SMALL_STRUCT_4B

cd_retuint8:
        movzx eax, BYTE PTR [ecx]
        jmp   cd_epilogue

cd_retsint8:
        movsx eax, BYTE PTR [ecx]
        jmp   cd_epilogue

cd_retuint16:
        movzx eax, WORD PTR [ecx]
        jmp   cd_epilogue

cd_retsint16:
        movsx eax, WORD PTR [ecx]
        jmp   cd_epilogue

cd_retint:
        mov   eax, [ecx]
        jmp   cd_epilogue

cd_retint64:
        mov   eax, [ecx + 0]
        mov   edx, [ecx + 4]
        jmp   cd_epilogue

cd_retfloat:
        fld   DWORD PTR [ecx]
        jmp   cd_epilogue

cd_retdouble:
        fld   QWORD PTR [ecx]
        jmp   cd_epilogue

cd_retlongdouble:
        fld   TBYTE PTR [ecx]
        jmp   cd_epilogue

cd_epilogue:
        ;; Epilogue code is autogenerated.
        ret
ffi_closure_STDCALL ENDP

_TEXT ENDS
END

#else

	.text
 
        # This assumes we are using gas.
        .balign 16
	.globl	_ffi_call_win32
#ifndef __OS2__
	.def	_ffi_call_win32;	.scl	2;	.type	32;	.endef
#endif
_ffi_call_win32:
.LFB1:
        pushl %ebp
.LCFI0:
        movl  %esp,%ebp
.LCFI1:
        # Make room for all of the new args.
        movl  20(%ebp),%ecx                                                     
        subl  %ecx,%esp
 
        movl  %esp,%eax
 
        # Place all of the ffi_prep_args in position
        pushl 12(%ebp)
        pushl %eax
        call  *8(%ebp)
 
        # Return stack to previous state and call the function
        addl  $8,%esp

	# Handle fastcall and thiscall
	cmpl $3, 16(%ebp)  # FFI_THISCALL
	jz .do_thiscall
	cmpl $4, 16(%ebp) # FFI_FASTCALL
	jnz .do_fncall
	movl (%esp), %ecx
	movl 4(%esp), %edx
	addl $8, %esp
	jmp .do_fncall
.do_thiscall:
	movl (%esp), %ecx
	addl $4, %esp

.do_fncall:
	 
        # FIXME: Align the stack to a 128-bit boundary to avoid
        # potential performance hits.

        call  *32(%ebp)
 
        # stdcall functions pop arguments off the stack themselves

        # Load %ecx with the return type code
        movl  24(%ebp),%ecx
 
        # If the return value pointer is NULL, assume no return value.
        cmpl  $0,28(%ebp)
        jne   0f
 
        # Even if there is no space for the return value, we are
        # obliged to handle floating-point values.
        cmpl  $FFI_TYPE_FLOAT,%ecx
        jne   .Lnoretval
        fstp  %st(0)
 
        jmp   .Lepilogue

0:
	call	1f
	# Do not insert anything here between the call and the jump table.
.Lstore_table:
	.long	.Lnoretval		/* FFI_TYPE_VOID */
	.long	.Lretint		/* FFI_TYPE_INT */
	.long	.Lretfloat		/* FFI_TYPE_FLOAT */
	.long	.Lretdouble		/* FFI_TYPE_DOUBLE */
	.long	.Lretlongdouble		/* FFI_TYPE_LONGDOUBLE */
	.long	.Lretuint8		/* FFI_TYPE_UINT8 */
	.long	.Lretsint8		/* FFI_TYPE_SINT8 */
	.long	.Lretuint16		/* FFI_TYPE_UINT16 */
	.long	.Lretsint16		/* FFI_TYPE_SINT16 */
	.long	.Lretint		/* FFI_TYPE_UINT32 */
	.long	.Lretint		/* FFI_TYPE_SINT32 */
	.long	.Lretint64		/* FFI_TYPE_UINT64 */
	.long	.Lretint64		/* FFI_TYPE_SINT64 */
	.long	.Lretstruct		/* FFI_TYPE_STRUCT */
	.long	.Lretint		/* FFI_TYPE_POINTER */
	.long	.Lretstruct1b		/* FFI_TYPE_SMALL_STRUCT_1B */
	.long	.Lretstruct2b		/* FFI_TYPE_SMALL_STRUCT_2B */
	.long	.Lretstruct4b		/* FFI_TYPE_SMALL_STRUCT_4B */
	.long	.Lretstruct		/* FFI_TYPE_MS_STRUCT */
1:
	add	%ecx, %ecx
	add	%ecx, %ecx
	add	(%esp),%ecx
	add	$4, %esp
	jmp	*(%ecx)

	/* Sign/zero extend as appropriate.  */
.Lretsint8:
	movsbl	%al, %eax
	jmp	.Lretint

.Lretsint16:
	movswl	%ax, %eax
	jmp	.Lretint

.Lretuint8:
	movzbl	%al, %eax
	jmp	.Lretint

.Lretuint16:
	movzwl	%ax, %eax
	jmp	.Lretint

.Lretint:
        # Load %ecx with the pointer to storage for the return value
        movl  28(%ebp),%ecx
        movl  %eax,0(%ecx)
        jmp   .Lepilogue
 
.Lretfloat:
         # Load %ecx with the pointer to storage for the return value
        movl  28(%ebp),%ecx
        fstps (%ecx)
        jmp   .Lepilogue
 
.Lretdouble:
        # Load %ecx with the pointer to storage for the return value
        movl  28(%ebp),%ecx
        fstpl (%ecx)
        jmp   .Lepilogue
 
.Lretlongdouble:
        # Load %ecx with the pointer to storage for the return value
        movl  28(%ebp),%ecx
        fstpt (%ecx)
        jmp   .Lepilogue
 
.Lretint64:
        # Load %ecx with the pointer to storage for the return value
        movl  28(%ebp),%ecx
        movl  %eax,0(%ecx)
        movl  %edx,4(%ecx)
	jmp   .Lepilogue

.Lretstruct1b:
        # Load %ecx with the pointer to storage for the return value
        movl  28(%ebp),%ecx
        movb  %al,0(%ecx)
        jmp   .Lepilogue
 
.Lretstruct2b:
        # Load %ecx with the pointer to storage for the return value
        movl  28(%ebp),%ecx
        movw  %ax,0(%ecx)
        jmp   .Lepilogue

.Lretstruct4b:
        # Load %ecx with the pointer to storage for the return value
        movl  28(%ebp),%ecx
        movl  %eax,0(%ecx)
        jmp   .Lepilogue

.Lretstruct:
        # Nothing to do!
 
.Lnoretval:
.Lepilogue:
        movl %ebp,%esp
        popl %ebp
        ret
.ffi_call_win32_end:
        .balign 16
	.globl	_ffi_closure_THISCALL
#ifndef __OS2__
	.def	_ffi_closure_THISCALL;	.scl	2;	.type	32;	.endef
#endif
_ffi_closure_THISCALL:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	leal	-24(%ebp), %edx
	movl	%edx, -12(%ebp)	/* resp */
	leal	12(%ebp), %edx  /* account for stub return address on stack */
	jmp	.stub
.LFE1:

        # This assumes we are using gas.
        .balign 16
	.globl	_ffi_closure_SYSV
#ifndef __OS2__
	.def	_ffi_closure_SYSV;	.scl	2;	.type	32;	.endef
#endif
_ffi_closure_SYSV:
.LFB3:
	pushl	%ebp
.LCFI4:
	movl	%esp, %ebp
.LCFI5:
	subl	$40, %esp
	leal	-24(%ebp), %edx
	movl	%edx, -12(%ebp)	/* resp */
	leal	8(%ebp), %edx
.stub:
	movl	%edx, 4(%esp)	/* args = __builtin_dwarf_cfa () */
	leal	-12(%ebp), %edx
	movl	%edx, (%esp)	/* &resp */
	call	_ffi_closure_SYSV_inner
	movl	-12(%ebp), %ecx

0:
	call	1f
	# Do not insert anything here between the call and the jump table.
.Lcls_store_table:
	.long	.Lcls_noretval		/* FFI_TYPE_VOID */
	.long	.Lcls_retint		/* FFI_TYPE_INT */
	.long	.Lcls_retfloat		/* FFI_TYPE_FLOAT */
	.long	.Lcls_retdouble		/* FFI_TYPE_DOUBLE */
	.long	.Lcls_retldouble	/* FFI_TYPE_LONGDOUBLE */
	.long	.Lcls_retuint8		/* FFI_TYPE_UINT8 */
	.long	.Lcls_retsint8		/* FFI_TYPE_SINT8 */
	.long	.Lcls_retuint16		/* FFI_TYPE_UINT16 */
	.long	.Lcls_retsint16		/* FFI_TYPE_SINT16 */
	.long	.Lcls_retint		/* FFI_TYPE_UINT32 */
	.long	.Lcls_retint		/* FFI_TYPE_SINT32 */
	.long	.Lcls_retllong		/* FFI_TYPE_UINT64 */
	.long	.Lcls_retllong		/* FFI_TYPE_SINT64 */
	.long	.Lcls_retstruct		/* FFI_TYPE_STRUCT */
	.long	.Lcls_retint		/* FFI_TYPE_POINTER */
	.long	.Lcls_retstruct1	/* FFI_TYPE_SMALL_STRUCT_1B */
	.long	.Lcls_retstruct2	/* FFI_TYPE_SMALL_STRUCT_2B */
	.long	.Lcls_retstruct4	/* FFI_TYPE_SMALL_STRUCT_4B */
	.long	.Lcls_retmsstruct	/* FFI_TYPE_MS_STRUCT */

1:
	add	%eax, %eax
	add	%eax, %eax
	add	(%esp),%eax
	add	$4, %esp
	jmp	*(%eax)

	/* Sign/zero extend as appropriate.  */
.Lcls_retsint8:
	movsbl	(%ecx), %eax
	jmp	.Lcls_epilogue

.Lcls_retsint16:
	movswl	(%ecx), %eax
	jmp	.Lcls_epilogue

.Lcls_retuint8:
	movzbl	(%ecx), %eax
	jmp	.Lcls_epilogue

.Lcls_retuint16:
	movzwl	(%ecx), %eax
	jmp	.Lcls_epilogue

.Lcls_retint:
	movl	(%ecx), %eax
	jmp	.Lcls_epilogue

.Lcls_retfloat:
	flds	(%ecx)
	jmp	.Lcls_epilogue

.Lcls_retdouble:
	fldl	(%ecx)
	jmp	.Lcls_epilogue

.Lcls_retldouble:
	fldt	(%ecx)
	jmp	.Lcls_epilogue

.Lcls_retllong:
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	jmp	.Lcls_epilogue

.Lcls_retstruct1:
	movsbl	(%ecx), %eax
	jmp	.Lcls_epilogue

.Lcls_retstruct2:
	movswl	(%ecx), %eax
	jmp	.Lcls_epilogue

.Lcls_retstruct4:
	movl	(%ecx), %eax
	jmp	.Lcls_epilogue

.Lcls_retstruct:
        # Caller expects us to pop struct return value pointer hidden arg.
	movl	%ebp, %esp
	popl	%ebp
	ret	$0x4

.Lcls_retmsstruct:
	# Caller expects us to return a pointer to the real return value.
	mov	%ecx, %eax
	# Caller doesn't expects us to pop struct return value pointer hidden arg.
	jmp	.Lcls_epilogue

.Lcls_noretval:
.Lcls_epilogue:
	movl	%ebp, %esp
	popl	%ebp
	ret
.ffi_closure_SYSV_end:
.LFE3:

#if !FFI_NO_RAW_API

#define RAW_CLOSURE_CIF_OFFSET ((FFI_TRAMPOLINE_SIZE + 3) & ~3)
#define RAW_CLOSURE_FUN_OFFSET (RAW_CLOSURE_CIF_OFFSET + 4)
#define RAW_CLOSURE_USER_DATA_OFFSET (RAW_CLOSURE_FUN_OFFSET + 4)
#define CIF_FLAGS_OFFSET 20
        .balign 16
	.globl	_ffi_closure_raw_THISCALL
#ifndef __OS2__
	.def	_ffi_closure_raw_THISCALL;	.scl	2;	.type	32;	.endef
#endif
_ffi_closure_raw_THISCALL:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	subl	$36, %esp
	movl	RAW_CLOSURE_CIF_OFFSET(%eax), %esi	 /* closure->cif */
	movl	RAW_CLOSURE_USER_DATA_OFFSET(%eax), %edx /* closure->user_data */
	movl	%edx, 12(%esp)	/* user_data */
	leal	12(%ebp), %edx	/* __builtin_dwarf_cfa () */
	jmp	.stubraw
        # This assumes we are using gas.
        .balign 16
	.globl	_ffi_closure_raw_SYSV
#ifndef __OS2__
	.def	_ffi_closure_raw_SYSV;	.scl	2;	.type	32;	.endef
#endif
_ffi_closure_raw_SYSV:
.LFB4:
	pushl	%ebp
.LCFI6:
	movl	%esp, %ebp
.LCFI7:
	pushl	%esi
.LCFI8:
	subl	$36, %esp
	movl	RAW_CLOSURE_CIF_OFFSET(%eax), %esi	 /* closure->cif */
	movl	RAW_CLOSURE_USER_DATA_OFFSET(%eax), %edx /* closure->user_data */
	movl	%edx, 12(%esp)	/* user_data */
	leal	8(%ebp), %edx	/* __builtin_dwarf_cfa () */
.stubraw:
	movl	%edx, 8(%esp)	/* raw_args */
	leal	-24(%ebp), %edx
	movl	%edx, 4(%esp)	/* &res */
	movl	%esi, (%esp)	/* cif */
	call	*RAW_CLOSURE_FUN_OFFSET(%eax)		 /* closure->fun */
	movl	CIF_FLAGS_OFFSET(%esi), %eax		 /* rtype */
0:
	call	1f
	# Do not insert anything here between the call and the jump table.
.Lrcls_store_table:
	.long	.Lrcls_noretval		/* FFI_TYPE_VOID */
	.long	.Lrcls_retint		/* FFI_TYPE_INT */
	.long	.Lrcls_retfloat		/* FFI_TYPE_FLOAT */
	.long	.Lrcls_retdouble	/* FFI_TYPE_DOUBLE */
	.long	.Lrcls_retldouble	/* FFI_TYPE_LONGDOUBLE */
	.long	.Lrcls_retuint8		/* FFI_TYPE_UINT8 */
	.long	.Lrcls_retsint8		/* FFI_TYPE_SINT8 */
	.long	.Lrcls_retuint16	/* FFI_TYPE_UINT16 */
	.long	.Lrcls_retsint16	/* FFI_TYPE_SINT16 */
	.long	.Lrcls_retint		/* FFI_TYPE_UINT32 */
	.long	.Lrcls_retint		/* FFI_TYPE_SINT32 */
	.long	.Lrcls_retllong		/* FFI_TYPE_UINT64 */
	.long	.Lrcls_retllong		/* FFI_TYPE_SINT64 */
	.long	.Lrcls_retstruct	/* FFI_TYPE_STRUCT */
	.long	.Lrcls_retint		/* FFI_TYPE_POINTER */
	.long	.Lrcls_retstruct1	/* FFI_TYPE_SMALL_STRUCT_1B */
	.long	.Lrcls_retstruct2	/* FFI_TYPE_SMALL_STRUCT_2B */
	.long	.Lrcls_retstruct4	/* FFI_TYPE_SMALL_STRUCT_4B */
	.long	.Lrcls_retstruct	/* FFI_TYPE_MS_STRUCT */
1:
	add	%eax, %eax
	add	%eax, %eax
	add	(%esp),%eax
	add	$4, %esp
	jmp	*(%eax)

	/* Sign/zero extend as appropriate.  */
.Lrcls_retsint8:
	movsbl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue

.Lrcls_retsint16:
	movswl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue

.Lrcls_retuint8:
	movzbl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue

.Lrcls_retuint16:
	movzwl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue

.Lrcls_retint:
	movl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue

.Lrcls_retfloat:
	flds	-24(%ebp)
	jmp	.Lrcls_epilogue

.Lrcls_retdouble:
	fldl	-24(%ebp)
	jmp	.Lrcls_epilogue

.Lrcls_retldouble:
	fldt	-24(%ebp)
	jmp	.Lrcls_epilogue

.Lrcls_retllong:
	movl	-24(%ebp), %eax
	movl	-20(%ebp), %edx
	jmp	.Lrcls_epilogue

.Lrcls_retstruct1:
	movsbl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue

.Lrcls_retstruct2:
	movswl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue

.Lrcls_retstruct4:
	movl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue

.Lrcls_retstruct:
	# Nothing to do!

.Lrcls_noretval:
.Lrcls_epilogue:
	addl	$36, %esp
	popl	%esi
	popl	%ebp
	ret
.ffi_closure_raw_SYSV_end:
.LFE4:

#endif /* !FFI_NO_RAW_API */

        # This assumes we are using gas.
	.balign	16
	.globl	_ffi_closure_STDCALL
#ifndef __OS2__
	.def	_ffi_closure_STDCALL;	.scl	2;	.type	32;	.endef
#endif
_ffi_closure_STDCALL:
.LFB5:
	pushl	%ebp
.LCFI9:
	movl	%esp, %ebp
.LCFI10:
	subl	$40, %esp
	leal	-24(%ebp), %edx
	movl	%edx, -12(%ebp)	/* resp */
	leal	12(%ebp), %edx  /* account for stub return address on stack */
	movl	%edx, 4(%esp)	/* args */
	leal	-12(%ebp), %edx
	movl	%edx, (%esp)	/* &resp */
	call	_ffi_closure_SYSV_inner
	movl	-12(%ebp), %ecx
0:
	call	1f
	# Do not insert anything here between the call and the jump table.
.Lscls_store_table:
	.long	.Lscls_noretval		/* FFI_TYPE_VOID */
	.long	.Lscls_retint		/* FFI_TYPE_INT */
	.long	.Lscls_retfloat		/* FFI_TYPE_FLOAT */
	.long	.Lscls_retdouble	/* FFI_TYPE_DOUBLE */
	.long	.Lscls_retldouble	/* FFI_TYPE_LONGDOUBLE */
	.long	.Lscls_retuint8		/* FFI_TYPE_UINT8 */
	.long	.Lscls_retsint8		/* FFI_TYPE_SINT8 */
	.long	.Lscls_retuint16	/* FFI_TYPE_UINT16 */
	.long	.Lscls_retsint16	/* FFI_TYPE_SINT16 */
	.long	.Lscls_retint		/* FFI_TYPE_UINT32 */
	.long	.Lscls_retint		/* FFI_TYPE_SINT32 */
	.long	.Lscls_retllong		/* FFI_TYPE_UINT64 */
	.long	.Lscls_retllong		/* FFI_TYPE_SINT64 */
	.long	.Lscls_retstruct	/* FFI_TYPE_STRUCT */
	.long	.Lscls_retint		/* FFI_TYPE_POINTER */
	.long	.Lscls_retstruct1	/* FFI_TYPE_SMALL_STRUCT_1B */
	.long	.Lscls_retstruct2	/* FFI_TYPE_SMALL_STRUCT_2B */
	.long	.Lscls_retstruct4	/* FFI_TYPE_SMALL_STRUCT_4B */
1:
	add	%eax, %eax
	add	%eax, %eax
	add	(%esp),%eax
	add	$4, %esp
	jmp	*(%eax)

	/* Sign/zero extend as appropriate.  */
.Lscls_retsint8:
	movsbl	(%ecx), %eax
	jmp	.Lscls_epilogue

.Lscls_retsint16:
	movswl	(%ecx), %eax
	jmp	.Lscls_epilogue

.Lscls_retuint8:
	movzbl	(%ecx), %eax
	jmp	.Lscls_epilogue

.Lscls_retuint16:
	movzwl	(%ecx), %eax
	jmp	.Lscls_epilogue

.Lscls_retint:
	movl	(%ecx), %eax
	jmp	.Lscls_epilogue

.Lscls_retfloat:
	flds	(%ecx)
	jmp	.Lscls_epilogue

.Lscls_retdouble:
	fldl	(%ecx)
	jmp	.Lscls_epilogue

.Lscls_retldouble:
	fldt	(%ecx)
	jmp	.Lscls_epilogue

.Lscls_retllong:
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	jmp	.Lscls_epilogue

.Lscls_retstruct1:
	movsbl	(%ecx), %eax
	jmp	.Lscls_epilogue

.Lscls_retstruct2:
	movswl	(%ecx), %eax
	jmp	.Lscls_epilogue

.Lscls_retstruct4:
	movl	(%ecx), %eax
	jmp	.Lscls_epilogue

.Lscls_retstruct:
	# Nothing to do!

.Lscls_noretval:
.Lscls_epilogue:
	movl	%ebp, %esp
	popl	%ebp
	ret
.ffi_closure_STDCALL_end:
.LFE5:

#ifndef __OS2__
	.section	.eh_frame,"w"
#endif
.Lframe1:
.LSCIE1:
	.long	.LECIE1-.LASCIE1  /* Length of Common Information Entry */
.LASCIE1:
	.long	0x0	/* CIE Identifier Tag */
	.byte	0x1	/* CIE Version */
#ifdef __PIC__
	.ascii "zR\0"	/* CIE Augmentation */
#else
	.ascii "\0"	/* CIE Augmentation */
#endif
	.byte	0x1	/* .uleb128 0x1; CIE Code Alignment Factor */
	.byte	0x7c	/* .sleb128 -4; CIE Data Alignment Factor */
	.byte	0x8	/* CIE RA Column */
#ifdef __PIC__
	.byte	0x1	/* .uleb128 0x1; Augmentation size */
	.byte	0x1b	/* FDE Encoding (pcrel sdata4) */
#endif
	.byte	0xc	/* DW_CFA_def_cfa CFA = r4 + 4 = 4(%esp) */
	.byte	0x4	/* .uleb128 0x4 */
	.byte	0x4	/* .uleb128 0x4 */
	.byte	0x88	/* DW_CFA_offset, column 0x8 %eip at CFA + 1 * -4 */
	.byte	0x1	/* .uleb128 0x1 */
	.align 4
.LECIE1:

.LSFDE1:
	.long	.LEFDE1-.LASFDE1	/* FDE Length */
.LASFDE1:
	.long	.LASFDE1-.Lframe1	/* FDE CIE offset */
#if defined __PIC__ && defined HAVE_AS_X86_PCREL
	.long	.LFB1-.	/* FDE initial location */
#else
	.long	.LFB1
#endif
	.long	.LFE1-.LFB1	/* FDE address range */
#ifdef __PIC__
	.byte	0x0	/* .uleb128 0x0; Augmentation size */
#endif
	/* DW_CFA_xxx CFI instructions go here.  */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI0-.LFB1
	.byte	0xe	/* DW_CFA_def_cfa_offset CFA = r4 + 8 = 8(%esp) */
	.byte	0x8	/* .uleb128 0x8 */
	.byte	0x85	/* DW_CFA_offset, column 0x5 %ebp at CFA + 2 * -4 */
	.byte	0x2	/* .uleb128 0x2 */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI1-.LCFI0
	.byte	0xd	/* DW_CFA_def_cfa_register CFA = r5 = %ebp */
	.byte	0x5	/* .uleb128 0x5 */

	/* End of DW_CFA_xxx CFI instructions.  */
	.align 4
.LEFDE1:


.LSFDE3:
	.long	.LEFDE3-.LASFDE3	/* FDE Length */
.LASFDE3:
	.long	.LASFDE3-.Lframe1	/* FDE CIE offset */
#if defined __PIC__ && defined HAVE_AS_X86_PCREL
	.long	.LFB3-.	/* FDE initial location */
#else
	.long	.LFB3
#endif
	.long	.LFE3-.LFB3	/* FDE address range */
#ifdef __PIC__
	.byte	0x0	/* .uleb128 0x0; Augmentation size */
#endif
	/* DW_CFA_xxx CFI instructions go here.  */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI4-.LFB3
	.byte	0xe	/* DW_CFA_def_cfa_offset CFA = r4 + 8 = 8(%esp) */
	.byte	0x8	/* .uleb128 0x8 */
	.byte	0x85	/* DW_CFA_offset, column 0x5 %ebp at CFA + 2 * -4 */
	.byte	0x2	/* .uleb128 0x2 */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI5-.LCFI4
	.byte	0xd	/* DW_CFA_def_cfa_register CFA = r5 = %ebp */
	.byte	0x5	/* .uleb128 0x5 */

	/* End of DW_CFA_xxx CFI instructions.  */
	.align 4
.LEFDE3:

#if !FFI_NO_RAW_API

.LSFDE4:
	.long	.LEFDE4-.LASFDE4	/* FDE Length */
.LASFDE4:
	.long	.LASFDE4-.Lframe1	/* FDE CIE offset */
#if defined __PIC__ && defined HAVE_AS_X86_PCREL
	.long	.LFB4-.	/* FDE initial location */
#else
	.long	.LFB4
#endif
	.long	.LFE4-.LFB4	/* FDE address range */
#ifdef __PIC__
	.byte	0x0	/* .uleb128 0x0; Augmentation size */
#endif
	/* DW_CFA_xxx CFI instructions go here.  */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI6-.LFB4
	.byte	0xe	/* DW_CFA_def_cfa_offset CFA = r4 + 8 = 8(%esp) */
	.byte	0x8	/* .uleb128 0x8 */
	.byte	0x85	/* DW_CFA_offset, column 0x5 %ebp at CFA + 2 * -4 */
	.byte	0x2	/* .uleb128 0x2 */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI7-.LCFI6
	.byte	0xd	/* DW_CFA_def_cfa_register CFA = r5 = %ebp */
	.byte	0x5	/* .uleb128 0x5 */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI8-.LCFI7
	.byte	0x86	/* DW_CFA_offset, column 0x6 %esi at CFA + 3 * -4 */
	.byte	0x3	/* .uleb128 0x3 */

	/* End of DW_CFA_xxx CFI instructions.  */
	.align 4
.LEFDE4:

#endif /* !FFI_NO_RAW_API */

.LSFDE5:
	.long	.LEFDE5-.LASFDE5	/* FDE Length */
.LASFDE5:
	.long	.LASFDE5-.Lframe1	/* FDE CIE offset */
#if defined __PIC__ && defined HAVE_AS_X86_PCREL
	.long	.LFB5-.	/* FDE initial location */
#else
	.long	.LFB5
#endif
	.long	.LFE5-.LFB5	/* FDE address range */
#ifdef __PIC__
	.byte	0x0	/* .uleb128 0x0; Augmentation size */
#endif
	/* DW_CFA_xxx CFI instructions go here.  */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI9-.LFB5
	.byte	0xe	/* DW_CFA_def_cfa_offset CFA = r4 + 8 = 8(%esp) */
	.byte	0x8	/* .uleb128 0x8 */
	.byte	0x85	/* DW_CFA_offset, column 0x5 %ebp at CFA + 2 * -4 */
	.byte	0x2	/* .uleb128 0x2 */

	.byte	0x4	/* DW_CFA_advance_loc4 */
	.long	.LCFI10-.LCFI9
	.byte	0xd	/* DW_CFA_def_cfa_register CFA = r5 = %ebp */
	.byte	0x5	/* .uleb128 0x5 */

	/* End of DW_CFA_xxx CFI instructions.  */
	.align 4
.LEFDE5:

#endif /* !_MSC_VER */

