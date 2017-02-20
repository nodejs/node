;/* inffas32.asm is a hand tuned assembler version of inffast.c -- fast decoding
; *
; * inffas32.asm is derivated from inffas86.c, with translation of assembly code
; *
; * Copyright (C) 1995-2003 Mark Adler
; * For conditions of distribution and use, see copyright notice in zlib.h
; *
; * Copyright (C) 2003 Chris Anderson <christop@charm.net>
; * Please use the copyright conditions above.
; *
; * Mar-13-2003 -- Most of this is derived from inffast.S which is derived from
; * the gcc -S output of zlib-1.2.0/inffast.c.  Zlib-1.2.0 is in beta release at
; * the moment.  I have successfully compiled and tested this code with gcc2.96,
; * gcc3.2, icc5.0, msvc6.0.  It is very close to the speed of inffast.S
; * compiled with gcc -DNO_MMX, but inffast.S is still faster on the P3 with MMX
; * enabled.  I will attempt to merge the MMX code into this version.  Newer
; * versions of this and inffast.S can be found at
; * http://www.eetbeetee.com/zlib/ and http://www.charm.net/~christop/zlib/
; *
; * 2005 : modification by Gilles Vollant
; */
; For Visual C++ 4.x and higher and ML 6.x and higher
;   ml.exe is in directory \MASM611C of Win95 DDK
;   ml.exe is also distributed in http://www.masm32.com/masmdl.htm
;    and in VC++2003 toolkit at http://msdn.microsoft.com/visualc/vctoolkit2003/
;
;
;   compile with command line option
;   ml  /coff /Zi /c /Flinffas32.lst inffas32.asm

;   if you define NO_GZIP (see inflate.h), compile with
;   ml  /coff /Zi /c /Flinffas32.lst /DNO_GUNZIP inffas32.asm


; zlib122sup is 0 fort zlib 1.2.2.1 and lower
; zlib122sup is 8 fort zlib 1.2.2.2 and more (with addition of dmax and head
;        in inflate_state in inflate.h)
zlib1222sup      equ    8


IFDEF GUNZIP
  INFLATE_MODE_TYPE    equ 11
  INFLATE_MODE_BAD     equ 26
ELSE
  IFNDEF NO_GUNZIP
    INFLATE_MODE_TYPE    equ 11
    INFLATE_MODE_BAD     equ 26
  ELSE
    INFLATE_MODE_TYPE    equ 3
    INFLATE_MODE_BAD     equ 17
  ENDIF
ENDIF


; 75 "inffast.S"
;FILE "inffast.S"

;;;GLOBAL _inflate_fast

;;;SECTION .text



	.586p
	.mmx

	name	inflate_fast_x86
	.MODEL	FLAT

_DATA			segment
inflate_fast_use_mmx:
	dd	1


_TEXT			segment



ALIGN 4
	db	'Fast decoding Code from Chris Anderson'
	db	0

ALIGN 4
invalid_literal_length_code_msg:
	db	'invalid literal/length code'
	db	0

ALIGN 4
invalid_distance_code_msg:
	db	'invalid distance code'
	db	0

ALIGN 4
invalid_distance_too_far_msg:
	db	'invalid distance too far back'
	db	0


ALIGN 4
inflate_fast_mask:
dd	0
dd	1
dd	3
dd	7
dd	15
dd	31
dd	63
dd	127
dd	255
dd	511
dd	1023
dd	2047
dd	4095
dd	8191
dd	16383
dd	32767
dd	65535
dd	131071
dd	262143
dd	524287
dd	1048575
dd	2097151
dd	4194303
dd	8388607
dd	16777215
dd	33554431
dd	67108863
dd	134217727
dd	268435455
dd	536870911
dd	1073741823
dd	2147483647
dd	4294967295


mode_state	 equ	0	;/* state->mode	*/
wsize_state	 equ	(32+zlib1222sup)	;/* state->wsize */
write_state	 equ	(36+4+zlib1222sup)	;/* state->write */
window_state	 equ	(40+4+zlib1222sup)	;/* state->window */
hold_state	 equ	(44+4+zlib1222sup)	;/* state->hold	*/
bits_state	 equ	(48+4+zlib1222sup)	;/* state->bits	*/
lencode_state	 equ	(64+4+zlib1222sup)	;/* state->lencode */
distcode_state	 equ	(68+4+zlib1222sup)	;/* state->distcode */
lenbits_state	 equ	(72+4+zlib1222sup)	;/* state->lenbits */
distbits_state	 equ	(76+4+zlib1222sup)	;/* state->distbits */


;;SECTION .text
; 205 "inffast.S"
;GLOBAL	inflate_fast_use_mmx

;SECTION .data


; GLOBAL inflate_fast_use_mmx:object
;.size inflate_fast_use_mmx, 4
; 226 "inffast.S"
;SECTION .text

ALIGN 4
_inflate_fast proc near
.FPO (16, 4, 0, 0, 1, 0)
	push  edi
	push  esi
	push  ebp
	push  ebx
	pushfd
	sub  esp,64
	cld




	mov  esi, [esp+88]
	mov  edi, [esi+28]







	mov  edx, [esi+4]
	mov  eax, [esi+0]

	add  edx,eax
	sub  edx,11

	mov  [esp+44],eax
	mov  [esp+20],edx

	mov  ebp, [esp+92]
	mov  ecx, [esi+16]
	mov  ebx, [esi+12]

	sub  ebp,ecx
	neg  ebp
	add  ebp,ebx

	sub  ecx,257
	add  ecx,ebx

	mov  [esp+60],ebx
	mov  [esp+40],ebp
	mov  [esp+16],ecx
; 285 "inffast.S"
	mov  eax, [edi+lencode_state]
	mov  ecx, [edi+distcode_state]

	mov  [esp+8],eax
	mov  [esp+12],ecx

	mov  eax,1
	mov  ecx, [edi+lenbits_state]
	shl  eax,cl
	dec  eax
	mov  [esp+0],eax

	mov  eax,1
	mov  ecx, [edi+distbits_state]
	shl  eax,cl
	dec  eax
	mov  [esp+4],eax

	mov  eax, [edi+wsize_state]
	mov  ecx, [edi+write_state]
	mov  edx, [edi+window_state]

	mov  [esp+52],eax
	mov  [esp+48],ecx
	mov  [esp+56],edx

	mov  ebp, [edi+hold_state]
	mov  ebx, [edi+bits_state]
; 321 "inffast.S"
	mov  esi, [esp+44]
	mov  ecx, [esp+20]
	cmp  ecx,esi
	ja   L_align_long

	add  ecx,11
	sub  ecx,esi
	mov  eax,12
	sub  eax,ecx
	lea  edi, [esp+28]
	rep movsb
	mov  ecx,eax
	xor  eax,eax
	rep stosb
	lea  esi, [esp+28]
	mov  [esp+20],esi
	jmp  L_is_aligned


L_align_long:
	test  esi,3
	jz   L_is_aligned
	xor  eax,eax
	mov  al, [esi]
	inc  esi
	mov  ecx,ebx
	add  ebx,8
	shl  eax,cl
	or  ebp,eax
	jmp L_align_long

L_is_aligned:
	mov  edi, [esp+60]
; 366 "inffast.S"
L_check_mmx:
	cmp  dword ptr [inflate_fast_use_mmx],2
	je   L_init_mmx
	ja   L_do_loop

	push  eax
	push  ebx
	push  ecx
	push  edx
	pushfd
	mov  eax, [esp]
	xor  dword ptr [esp],0200000h




	popfd
	pushfd
	pop  edx
	xor  edx,eax
	jz   L_dont_use_mmx
	xor  eax,eax
	cpuid
	cmp  ebx,0756e6547h
	jne  L_dont_use_mmx
	cmp  ecx,06c65746eh
	jne  L_dont_use_mmx
	cmp  edx,049656e69h
	jne  L_dont_use_mmx
	mov  eax,1
	cpuid
	shr  eax,8
	and  eax,15
	cmp  eax,6
	jne  L_dont_use_mmx
	test  edx,0800000h
	jnz  L_use_mmx
	jmp  L_dont_use_mmx
L_use_mmx:
	mov  dword ptr [inflate_fast_use_mmx],2
	jmp  L_check_mmx_pop
L_dont_use_mmx:
	mov  dword ptr [inflate_fast_use_mmx],3
L_check_mmx_pop:
	pop  edx
	pop  ecx
	pop  ebx
	pop  eax
	jmp  L_check_mmx
; 426 "inffast.S"
ALIGN 4
L_do_loop:
; 437 "inffast.S"
	cmp  bl,15
	ja   L_get_length_code

	xor  eax,eax
	lodsw
	mov  cl,bl
	add  bl,16
	shl  eax,cl
	or  ebp,eax

L_get_length_code:
	mov  edx, [esp+0]
	mov  ecx, [esp+8]
	and  edx,ebp
	mov  eax, [ecx+edx*4]

L_dolen:






	mov  cl,ah
	sub  bl,ah
	shr  ebp,cl






	test  al,al
	jnz   L_test_for_length_base

	shr  eax,16
	stosb

L_while_test:


	cmp  [esp+16],edi
	jbe  L_break_loop

	cmp  [esp+20],esi
	ja   L_do_loop
	jmp  L_break_loop

L_test_for_length_base:
; 502 "inffast.S"
	mov  edx,eax
	shr  edx,16
	mov  cl,al

	test  al,16
	jz   L_test_for_second_level_length
	and  cl,15
	jz   L_save_len
	cmp  bl,cl
	jae  L_add_bits_to_len

	mov  ch,cl
	xor  eax,eax
	lodsw
	mov  cl,bl
	add  bl,16
	shl  eax,cl
	or  ebp,eax
	mov  cl,ch

L_add_bits_to_len:
	mov  eax,1
	shl  eax,cl
	dec  eax
	sub  bl,cl
	and  eax,ebp
	shr  ebp,cl
	add  edx,eax

L_save_len:
	mov  [esp+24],edx


L_decode_distance:
; 549 "inffast.S"
	cmp  bl,15
	ja   L_get_distance_code

	xor  eax,eax
	lodsw
	mov  cl,bl
	add  bl,16
	shl  eax,cl
	or  ebp,eax

L_get_distance_code:
	mov  edx, [esp+4]
	mov  ecx, [esp+12]
	and  edx,ebp
	mov  eax, [ecx+edx*4]


L_dodist:
	mov  edx,eax
	shr  edx,16
	mov  cl,ah
	sub  bl,ah
	shr  ebp,cl
; 584 "inffast.S"
	mov  cl,al

	test  al,16
	jz  L_test_for_second_level_dist
	and  cl,15
	jz  L_check_dist_one
	cmp  bl,cl
	jae  L_add_bits_to_dist

	mov  ch,cl
	xor  eax,eax
	lodsw
	mov  cl,bl
	add  bl,16
	shl  eax,cl
	or  ebp,eax
	mov  cl,ch

L_add_bits_to_dist:
	mov  eax,1
	shl  eax,cl
	dec  eax
	sub  bl,cl
	and  eax,ebp
	shr  ebp,cl
	add  edx,eax
	jmp  L_check_window

L_check_window:
; 625 "inffast.S"
	mov  [esp+44],esi
	mov  eax,edi
	sub  eax, [esp+40]

	cmp  eax,edx
	jb   L_clip_window

	mov  ecx, [esp+24]
	mov  esi,edi
	sub  esi,edx

	sub  ecx,3
	mov  al, [esi]
	mov  [edi],al
	mov  al, [esi+1]
	mov  dl, [esi+2]
	add  esi,3
	mov  [edi+1],al
	mov  [edi+2],dl
	add  edi,3
	rep movsb

	mov  esi, [esp+44]
	jmp  L_while_test

ALIGN 4
L_check_dist_one:
	cmp  edx,1
	jne  L_check_window
	cmp  [esp+40],edi
	je  L_check_window

	dec  edi
	mov  ecx, [esp+24]
	mov  al, [edi]
	sub  ecx,3

	mov  [edi+1],al
	mov  [edi+2],al
	mov  [edi+3],al
	add  edi,4
	rep stosb

	jmp  L_while_test

ALIGN 4
L_test_for_second_level_length:




	test  al,64
	jnz   L_test_for_end_of_block

	mov  eax,1
	shl  eax,cl
	dec  eax
	and  eax,ebp
	add  eax,edx
	mov  edx, [esp+8]
	mov  eax, [edx+eax*4]
	jmp  L_dolen

ALIGN 4
L_test_for_second_level_dist:




	test  al,64
	jnz   L_invalid_distance_code

	mov  eax,1
	shl  eax,cl
	dec  eax
	and  eax,ebp
	add  eax,edx
	mov  edx, [esp+12]
	mov  eax, [edx+eax*4]
	jmp  L_dodist

ALIGN 4
L_clip_window:
; 721 "inffast.S"
	mov  ecx,eax
	mov  eax, [esp+52]
	neg  ecx
	mov  esi, [esp+56]

	cmp  eax,edx
	jb   L_invalid_distance_too_far

	add  ecx,edx
	cmp  dword ptr [esp+48],0
	jne  L_wrap_around_window

	sub  eax,ecx
	add  esi,eax
; 749 "inffast.S"
	mov  eax, [esp+24]
	cmp  eax,ecx
	jbe  L_do_copy1

	sub  eax,ecx
	rep movsb
	mov  esi,edi
	sub  esi,edx
	jmp  L_do_copy1

	cmp  eax,ecx
	jbe  L_do_copy1

	sub  eax,ecx
	rep movsb
	mov  esi,edi
	sub  esi,edx
	jmp  L_do_copy1

L_wrap_around_window:
; 793 "inffast.S"
	mov  eax, [esp+48]
	cmp  ecx,eax
	jbe  L_contiguous_in_window

	add  esi, [esp+52]
	add  esi,eax
	sub  esi,ecx
	sub  ecx,eax


	mov  eax, [esp+24]
	cmp  eax,ecx
	jbe  L_do_copy1

	sub  eax,ecx
	rep movsb
	mov  esi, [esp+56]
	mov  ecx, [esp+48]
	cmp  eax,ecx
	jbe  L_do_copy1

	sub  eax,ecx
	rep movsb
	mov  esi,edi
	sub  esi,edx
	jmp  L_do_copy1

L_contiguous_in_window:
; 836 "inffast.S"
	add  esi,eax
	sub  esi,ecx


	mov  eax, [esp+24]
	cmp  eax,ecx
	jbe  L_do_copy1

	sub  eax,ecx
	rep movsb
	mov  esi,edi
	sub  esi,edx

L_do_copy1:
; 862 "inffast.S"
	mov  ecx,eax
	rep movsb

	mov  esi, [esp+44]
	jmp  L_while_test
; 878 "inffast.S"
ALIGN 4
L_init_mmx:
	emms





	movd mm0,ebp
	mov  ebp,ebx
; 896 "inffast.S"
	movd mm4,dword ptr [esp+0]
	movq mm3,mm4
	movd mm5,dword ptr [esp+4]
	movq mm2,mm5
	pxor mm1,mm1
	mov  ebx, [esp+8]
	jmp  L_do_loop_mmx

ALIGN 4
L_do_loop_mmx:
	psrlq mm0,mm1

	cmp  ebp,32
	ja  L_get_length_code_mmx

	movd mm6,ebp
	movd mm7,dword ptr [esi]
	add  esi,4
	psllq mm7,mm6
	add  ebp,32
	por mm0,mm7

L_get_length_code_mmx:
	pand mm4,mm0
	movd eax,mm4
	movq mm4,mm3
	mov  eax, [ebx+eax*4]

L_dolen_mmx:
	movzx  ecx,ah
	movd mm1,ecx
	sub  ebp,ecx

	test  al,al
	jnz L_test_for_length_base_mmx

	shr  eax,16
	stosb

L_while_test_mmx:


	cmp  [esp+16],edi
	jbe L_break_loop

	cmp  [esp+20],esi
	ja L_do_loop_mmx
	jmp L_break_loop

L_test_for_length_base_mmx:

	mov  edx,eax
	shr  edx,16

	test  al,16
	jz  L_test_for_second_level_length_mmx
	and  eax,15
	jz L_decode_distance_mmx

	psrlq mm0,mm1
	movd mm1,eax
	movd ecx,mm0
	sub  ebp,eax
	and  ecx, [inflate_fast_mask+eax*4]
	add  edx,ecx

L_decode_distance_mmx:
	psrlq mm0,mm1

	cmp  ebp,32
	ja L_get_dist_code_mmx

	movd mm6,ebp
	movd mm7,dword ptr [esi]
	add  esi,4
	psllq mm7,mm6
	add  ebp,32
	por mm0,mm7

L_get_dist_code_mmx:
	mov  ebx, [esp+12]
	pand mm5,mm0
	movd eax,mm5
	movq mm5,mm2
	mov  eax, [ebx+eax*4]

L_dodist_mmx:

	movzx  ecx,ah
	mov  ebx,eax
	shr  ebx,16
	sub  ebp,ecx
	movd mm1,ecx

	test  al,16
	jz L_test_for_second_level_dist_mmx
	and  eax,15
	jz L_check_dist_one_mmx

L_add_bits_to_dist_mmx:
	psrlq mm0,mm1
	movd mm1,eax
	movd ecx,mm0
	sub  ebp,eax
	and  ecx, [inflate_fast_mask+eax*4]
	add  ebx,ecx

L_check_window_mmx:
	mov  [esp+44],esi
	mov  eax,edi
	sub  eax, [esp+40]

	cmp  eax,ebx
	jb L_clip_window_mmx

	mov  ecx,edx
	mov  esi,edi
	sub  esi,ebx

	sub  ecx,3
	mov  al, [esi]
	mov  [edi],al
	mov  al, [esi+1]
	mov  dl, [esi+2]
	add  esi,3
	mov  [edi+1],al
	mov  [edi+2],dl
	add  edi,3
	rep movsb

	mov  esi, [esp+44]
	mov  ebx, [esp+8]
	jmp  L_while_test_mmx

ALIGN 4
L_check_dist_one_mmx:
	cmp  ebx,1
	jne  L_check_window_mmx
	cmp  [esp+40],edi
	je   L_check_window_mmx

	dec  edi
	mov  ecx,edx
	mov  al, [edi]
	sub  ecx,3

	mov  [edi+1],al
	mov  [edi+2],al
	mov  [edi+3],al
	add  edi,4
	rep stosb

	mov  ebx, [esp+8]
	jmp  L_while_test_mmx

ALIGN 4
L_test_for_second_level_length_mmx:
	test  al,64
	jnz L_test_for_end_of_block

	and  eax,15
	psrlq mm0,mm1
	movd ecx,mm0
	and  ecx, [inflate_fast_mask+eax*4]
	add  ecx,edx
	mov  eax, [ebx+ecx*4]
	jmp L_dolen_mmx

ALIGN 4
L_test_for_second_level_dist_mmx:
	test  al,64
	jnz L_invalid_distance_code

	and  eax,15
	psrlq mm0,mm1
	movd ecx,mm0
	and  ecx, [inflate_fast_mask+eax*4]
	mov  eax, [esp+12]
	add  ecx,ebx
	mov  eax, [eax+ecx*4]
	jmp  L_dodist_mmx

ALIGN 4
L_clip_window_mmx:

	mov  ecx,eax
	mov  eax, [esp+52]
	neg  ecx
	mov  esi, [esp+56]

	cmp  eax,ebx
	jb  L_invalid_distance_too_far

	add  ecx,ebx
	cmp  dword ptr [esp+48],0
	jne  L_wrap_around_window_mmx

	sub  eax,ecx
	add  esi,eax

	cmp  edx,ecx
	jbe  L_do_copy1_mmx

	sub  edx,ecx
	rep movsb
	mov  esi,edi
	sub  esi,ebx
	jmp  L_do_copy1_mmx

	cmp  edx,ecx
	jbe  L_do_copy1_mmx

	sub  edx,ecx
	rep movsb
	mov  esi,edi
	sub  esi,ebx
	jmp  L_do_copy1_mmx

L_wrap_around_window_mmx:

	mov  eax, [esp+48]
	cmp  ecx,eax
	jbe  L_contiguous_in_window_mmx

	add  esi, [esp+52]
	add  esi,eax
	sub  esi,ecx
	sub  ecx,eax


	cmp  edx,ecx
	jbe  L_do_copy1_mmx

	sub  edx,ecx
	rep movsb
	mov  esi, [esp+56]
	mov  ecx, [esp+48]
	cmp  edx,ecx
	jbe  L_do_copy1_mmx

	sub  edx,ecx
	rep movsb
	mov  esi,edi
	sub  esi,ebx
	jmp  L_do_copy1_mmx

L_contiguous_in_window_mmx:

	add  esi,eax
	sub  esi,ecx


	cmp  edx,ecx
	jbe  L_do_copy1_mmx

	sub  edx,ecx
	rep movsb
	mov  esi,edi
	sub  esi,ebx

L_do_copy1_mmx:


	mov  ecx,edx
	rep movsb

	mov  esi, [esp+44]
	mov  ebx, [esp+8]
	jmp  L_while_test_mmx
; 1174 "inffast.S"
L_invalid_distance_code:





	mov  ecx, invalid_distance_code_msg
	mov  edx,INFLATE_MODE_BAD
	jmp  L_update_stream_state

L_test_for_end_of_block:





	test  al,32
	jz  L_invalid_literal_length_code

	mov  ecx,0
	mov  edx,INFLATE_MODE_TYPE
	jmp  L_update_stream_state

L_invalid_literal_length_code:





	mov  ecx, invalid_literal_length_code_msg
	mov  edx,INFLATE_MODE_BAD
	jmp  L_update_stream_state

L_invalid_distance_too_far:



	mov  esi, [esp+44]
	mov  ecx, invalid_distance_too_far_msg
	mov  edx,INFLATE_MODE_BAD
	jmp  L_update_stream_state

L_update_stream_state:

	mov  eax, [esp+88]
	test  ecx,ecx
	jz  L_skip_msg
	mov  [eax+24],ecx
L_skip_msg:
	mov  eax, [eax+28]
	mov  [eax+mode_state],edx
	jmp  L_break_loop

ALIGN 4
L_break_loop:
; 1243 "inffast.S"
	cmp  dword ptr [inflate_fast_use_mmx],2
	jne  L_update_next_in



	mov  ebx,ebp

L_update_next_in:
; 1266 "inffast.S"
	mov  eax, [esp+88]
	mov  ecx,ebx
	mov  edx, [eax+28]
	shr  ecx,3
	sub  esi,ecx
	shl  ecx,3
	sub  ebx,ecx
	mov  [eax+12],edi
	mov  [edx+bits_state],ebx
	mov  ecx,ebx

	lea  ebx, [esp+28]
	cmp  [esp+20],ebx
	jne  L_buf_not_used

	sub  esi,ebx
	mov  ebx, [eax+0]
	mov  [esp+20],ebx
	add  esi,ebx
	mov  ebx, [eax+4]
	sub  ebx,11
	add  [esp+20],ebx

L_buf_not_used:
	mov  [eax+0],esi

	mov  ebx,1
	shl  ebx,cl
	dec  ebx





	cmp  dword ptr [inflate_fast_use_mmx],2
	jne  L_update_hold



	psrlq mm0,mm1
	movd ebp,mm0

	emms

L_update_hold:



	and  ebp,ebx
	mov  [edx+hold_state],ebp




	mov  ebx, [esp+20]
	cmp  ebx,esi
	jbe  L_last_is_smaller

	sub  ebx,esi
	add  ebx,11
	mov  [eax+4],ebx
	jmp  L_fixup_out
L_last_is_smaller:
	sub  esi,ebx
	neg  esi
	add  esi,11
	mov  [eax+4],esi




L_fixup_out:

	mov  ebx, [esp+16]
	cmp  ebx,edi
	jbe  L_end_is_smaller

	sub  ebx,edi
	add  ebx,257
	mov  [eax+16],ebx
	jmp  L_done
L_end_is_smaller:
	sub  edi,ebx
	neg  edi
	add  edi,257
	mov  [eax+16],edi





L_done:
	add  esp,64
	popfd
	pop  ebx
	pop  ebp
	pop  esi
	pop  edi
	ret
_inflate_fast endp

_TEXT	ends
end
