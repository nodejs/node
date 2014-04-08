OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'

















ALIGN	16
_vpaes_encrypt_core	PROC PRIVATE
	mov	r9,rdx
	mov	r11,16
	mov	eax,DWORD PTR[240+rdx]
	movdqa	xmm1,xmm9
	movdqa	xmm2,XMMWORD PTR[$L$k_ipt]
	pandn	xmm1,xmm0
	movdqu	xmm5,XMMWORD PTR[r9]
	psrld	xmm1,4
	pand	xmm0,xmm9
DB	102,15,56,0,208
	movdqa	xmm0,XMMWORD PTR[(($L$k_ipt+16))]
DB	102,15,56,0,193
	pxor	xmm2,xmm5
	pxor	xmm0,xmm2
	add	r9,16
	lea	r10,QWORD PTR[$L$k_mc_backward]
	jmp	$L$enc_entry

ALIGN	16
$L$enc_loop::

	movdqa	xmm4,xmm13
DB	102,15,56,0,226
	pxor	xmm4,xmm5
	movdqa	xmm0,xmm12
DB	102,15,56,0,195
	pxor	xmm0,xmm4
	movdqa	xmm5,xmm15
DB	102,15,56,0,234
	movdqa	xmm1,XMMWORD PTR[((-64))+r10*1+r11]
	movdqa	xmm2,xmm14
DB	102,15,56,0,211
	pxor	xmm2,xmm5
	movdqa	xmm4,XMMWORD PTR[r10*1+r11]
	movdqa	xmm3,xmm0
DB	102,15,56,0,193
	add	r9,16
	pxor	xmm0,xmm2
DB	102,15,56,0,220
	add	r11,16
	pxor	xmm3,xmm0
DB	102,15,56,0,193
	and	r11,030h
	pxor	xmm0,xmm3
	sub	rax,1

$L$enc_entry::

	movdqa	xmm1,xmm9
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm9
	movdqa	xmm5,xmm11
DB	102,15,56,0,232
	pxor	xmm0,xmm1
	movdqa	xmm3,xmm10
DB	102,15,56,0,217
	pxor	xmm3,xmm5
	movdqa	xmm4,xmm10
DB	102,15,56,0,224
	pxor	xmm4,xmm5
	movdqa	xmm2,xmm10
DB	102,15,56,0,211
	pxor	xmm2,xmm0
	movdqa	xmm3,xmm10
	movdqu	xmm5,XMMWORD PTR[r9]
DB	102,15,56,0,220
	pxor	xmm3,xmm1
	jnz	$L$enc_loop


	movdqa	xmm4,XMMWORD PTR[((-96))+r10]
	movdqa	xmm0,XMMWORD PTR[((-80))+r10]
DB	102,15,56,0,226
	pxor	xmm4,xmm5
DB	102,15,56,0,195
	movdqa	xmm1,XMMWORD PTR[64+r10*1+r11]
	pxor	xmm0,xmm4
DB	102,15,56,0,193
	DB	0F3h,0C3h		;repret
_vpaes_encrypt_core	ENDP







ALIGN	16
_vpaes_decrypt_core	PROC PRIVATE
	mov	r9,rdx
	mov	eax,DWORD PTR[240+rdx]
	movdqa	xmm1,xmm9
	movdqa	xmm2,XMMWORD PTR[$L$k_dipt]
	pandn	xmm1,xmm0
	mov	r11,rax
	psrld	xmm1,4
	movdqu	xmm5,XMMWORD PTR[r9]
	shl	r11,4
	pand	xmm0,xmm9
DB	102,15,56,0,208
	movdqa	xmm0,XMMWORD PTR[(($L$k_dipt+16))]
	xor	r11,030h
	lea	r10,QWORD PTR[$L$k_dsbd]
DB	102,15,56,0,193
	and	r11,030h
	pxor	xmm2,xmm5
	movdqa	xmm5,XMMWORD PTR[(($L$k_mc_forward+48))]
	pxor	xmm0,xmm2
	add	r9,16
	add	r11,r10
	jmp	$L$dec_entry

ALIGN	16
$L$dec_loop::



	movdqa	xmm4,XMMWORD PTR[((-32))+r10]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR[((-16))+r10]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
	add	r9,16

DB	102,15,56,0,197
	movdqa	xmm4,XMMWORD PTR[r10]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR[16+r10]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
	sub	rax,1

DB	102,15,56,0,197
	movdqa	xmm4,XMMWORD PTR[32+r10]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR[48+r10]
DB	102,15,56,0,195
	pxor	xmm0,xmm4

DB	102,15,56,0,197
	movdqa	xmm4,XMMWORD PTR[64+r10]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR[80+r10]
DB	102,15,56,0,195
	pxor	xmm0,xmm4

DB	102,15,58,15,237,12

$L$dec_entry::

	movdqa	xmm1,xmm9
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm9
	movdqa	xmm2,xmm11
DB	102,15,56,0,208
	pxor	xmm0,xmm1
	movdqa	xmm3,xmm10
DB	102,15,56,0,217
	pxor	xmm3,xmm2
	movdqa	xmm4,xmm10
DB	102,15,56,0,224
	pxor	xmm4,xmm2
	movdqa	xmm2,xmm10
DB	102,15,56,0,211
	pxor	xmm2,xmm0
	movdqa	xmm3,xmm10
DB	102,15,56,0,220
	pxor	xmm3,xmm1
	movdqu	xmm0,XMMWORD PTR[r9]
	jnz	$L$dec_loop


	movdqa	xmm4,XMMWORD PTR[96+r10]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR[112+r10]
	movdqa	xmm2,XMMWORD PTR[((-352))+r11]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
DB	102,15,56,0,194
	DB	0F3h,0C3h		;repret
_vpaes_decrypt_core	ENDP







ALIGN	16
_vpaes_schedule_core	PROC PRIVATE





	call	_vpaes_preheat

	movdqa	xmm8,XMMWORD PTR[$L$k_rcon]
	movdqu	xmm0,XMMWORD PTR[rdi]


	movdqa	xmm3,xmm0
	lea	r11,QWORD PTR[$L$k_ipt]
	call	_vpaes_schedule_transform
	movdqa	xmm7,xmm0

	lea	r10,QWORD PTR[$L$k_sr]
	test	rcx,rcx
	jnz	$L$schedule_am_decrypting


	movdqu	XMMWORD PTR[rdx],xmm0
	jmp	$L$schedule_go

$L$schedule_am_decrypting::

	movdqa	xmm1,XMMWORD PTR[r10*1+r8]
DB	102,15,56,0,217
	movdqu	XMMWORD PTR[rdx],xmm3
	xor	r8,030h

$L$schedule_go::
	cmp	esi,192
	ja	$L$schedule_256
	je	$L$schedule_192










$L$schedule_128::
	mov	esi,10

$L$oop_schedule_128::
	call	_vpaes_schedule_round
	dec	rsi
	jz	$L$schedule_mangle_last
	call	_vpaes_schedule_mangle

	jmp	$L$oop_schedule_128
















ALIGN	16
$L$schedule_192::
	movdqu	xmm0,XMMWORD PTR[8+rdi]
	call	_vpaes_schedule_transform

	movdqa	xmm6,xmm0
	pxor	xmm4,xmm4
	movhlps	xmm6,xmm4
	mov	esi,4

$L$oop_schedule_192::
	call	_vpaes_schedule_round
DB	102,15,58,15,198,8
	call	_vpaes_schedule_mangle

	call	_vpaes_schedule_192_smear
	call	_vpaes_schedule_mangle

	call	_vpaes_schedule_round
	dec	rsi
	jz	$L$schedule_mangle_last
	call	_vpaes_schedule_mangle

	call	_vpaes_schedule_192_smear
	jmp	$L$oop_schedule_192











ALIGN	16
$L$schedule_256::
	movdqu	xmm0,XMMWORD PTR[16+rdi]
	call	_vpaes_schedule_transform

	mov	esi,7

$L$oop_schedule_256::
	call	_vpaes_schedule_mangle

	movdqa	xmm6,xmm0


	call	_vpaes_schedule_round
	dec	rsi
	jz	$L$schedule_mangle_last
	call	_vpaes_schedule_mangle



	pshufd	xmm0,xmm0,0FFh
	movdqa	xmm5,xmm7
	movdqa	xmm7,xmm6
	call	_vpaes_schedule_low_round
	movdqa	xmm7,xmm5

	jmp	$L$oop_schedule_256












ALIGN	16
$L$schedule_mangle_last::

	lea	r11,QWORD PTR[$L$k_deskew]
	test	rcx,rcx
	jnz	$L$schedule_mangle_last_dec


	movdqa	xmm1,XMMWORD PTR[r10*1+r8]
DB	102,15,56,0,193
	lea	r11,QWORD PTR[$L$k_opt]
	add	rdx,32

$L$schedule_mangle_last_dec::
	add	rdx,-16
	pxor	xmm0,XMMWORD PTR[$L$k_s63]
	call	_vpaes_schedule_transform

	movdqu	XMMWORD PTR[rdx],xmm0


	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
	DB	0F3h,0C3h		;repret
_vpaes_schedule_core	ENDP
















ALIGN	16
_vpaes_schedule_192_smear	PROC PRIVATE
	pshufd	xmm0,xmm6,080h
	pxor	xmm6,xmm0
	pshufd	xmm0,xmm7,0FEh
	pxor	xmm6,xmm0
	movdqa	xmm0,xmm6
	pxor	xmm1,xmm1
	movhlps	xmm6,xmm1
	DB	0F3h,0C3h		;repret
_vpaes_schedule_192_smear	ENDP




















ALIGN	16
_vpaes_schedule_round	PROC PRIVATE

	pxor	xmm1,xmm1
DB	102,65,15,58,15,200,15
DB	102,69,15,58,15,192,15
	pxor	xmm7,xmm1


	pshufd	xmm0,xmm0,0FFh
DB	102,15,58,15,192,1




_vpaes_schedule_low_round::

	movdqa	xmm1,xmm7
	pslldq	xmm7,4
	pxor	xmm7,xmm1
	movdqa	xmm1,xmm7
	pslldq	xmm7,8
	pxor	xmm7,xmm1
	pxor	xmm7,XMMWORD PTR[$L$k_s63]


	movdqa	xmm1,xmm9
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm9
	movdqa	xmm2,xmm11
DB	102,15,56,0,208
	pxor	xmm0,xmm1
	movdqa	xmm3,xmm10
DB	102,15,56,0,217
	pxor	xmm3,xmm2
	movdqa	xmm4,xmm10
DB	102,15,56,0,224
	pxor	xmm4,xmm2
	movdqa	xmm2,xmm10
DB	102,15,56,0,211
	pxor	xmm2,xmm0
	movdqa	xmm3,xmm10
DB	102,15,56,0,220
	pxor	xmm3,xmm1
	movdqa	xmm4,xmm13
DB	102,15,56,0,226
	movdqa	xmm0,xmm12
DB	102,15,56,0,195
	pxor	xmm0,xmm4


	pxor	xmm0,xmm7
	movdqa	xmm7,xmm0
	DB	0F3h,0C3h		;repret
_vpaes_schedule_round	ENDP











ALIGN	16
_vpaes_schedule_transform	PROC PRIVATE
	movdqa	xmm1,xmm9
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm9
	movdqa	xmm2,XMMWORD PTR[r11]
DB	102,15,56,0,208
	movdqa	xmm0,XMMWORD PTR[16+r11]
DB	102,15,56,0,193
	pxor	xmm0,xmm2
	DB	0F3h,0C3h		;repret
_vpaes_schedule_transform	ENDP

























ALIGN	16
_vpaes_schedule_mangle	PROC PRIVATE
	movdqa	xmm4,xmm0
	movdqa	xmm5,XMMWORD PTR[$L$k_mc_forward]
	test	rcx,rcx
	jnz	$L$schedule_mangle_dec


	add	rdx,16
	pxor	xmm4,XMMWORD PTR[$L$k_s63]
DB	102,15,56,0,229
	movdqa	xmm3,xmm4
DB	102,15,56,0,229
	pxor	xmm3,xmm4
DB	102,15,56,0,229
	pxor	xmm3,xmm4

	jmp	$L$schedule_mangle_both
ALIGN	16
$L$schedule_mangle_dec::

	lea	r11,QWORD PTR[$L$k_dksd]
	movdqa	xmm1,xmm9
	pandn	xmm1,xmm4
	psrld	xmm1,4
	pand	xmm4,xmm9

	movdqa	xmm2,XMMWORD PTR[r11]
DB	102,15,56,0,212
	movdqa	xmm3,XMMWORD PTR[16+r11]
DB	102,15,56,0,217
	pxor	xmm3,xmm2
DB	102,15,56,0,221

	movdqa	xmm2,XMMWORD PTR[32+r11]
DB	102,15,56,0,212
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD PTR[48+r11]
DB	102,15,56,0,217
	pxor	xmm3,xmm2
DB	102,15,56,0,221

	movdqa	xmm2,XMMWORD PTR[64+r11]
DB	102,15,56,0,212
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD PTR[80+r11]
DB	102,15,56,0,217
	pxor	xmm3,xmm2
DB	102,15,56,0,221

	movdqa	xmm2,XMMWORD PTR[96+r11]
DB	102,15,56,0,212
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD PTR[112+r11]
DB	102,15,56,0,217
	pxor	xmm3,xmm2

	add	rdx,-16

$L$schedule_mangle_both::
	movdqa	xmm1,XMMWORD PTR[r10*1+r8]
DB	102,15,56,0,217
	add	r8,-16
	and	r8,030h
	movdqu	XMMWORD PTR[rdx],xmm3
	DB	0F3h,0C3h		;repret
_vpaes_schedule_mangle	ENDP




PUBLIC	vpaes_set_encrypt_key

ALIGN	16
vpaes_set_encrypt_key	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_set_encrypt_key::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	lea	rsp,QWORD PTR[((-184))+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm6
	movaps	XMMWORD PTR[32+rsp],xmm7
	movaps	XMMWORD PTR[48+rsp],xmm8
	movaps	XMMWORD PTR[64+rsp],xmm9
	movaps	XMMWORD PTR[80+rsp],xmm10
	movaps	XMMWORD PTR[96+rsp],xmm11
	movaps	XMMWORD PTR[112+rsp],xmm12
	movaps	XMMWORD PTR[128+rsp],xmm13
	movaps	XMMWORD PTR[144+rsp],xmm14
	movaps	XMMWORD PTR[160+rsp],xmm15
$L$enc_key_body::
	mov	eax,esi
	shr	eax,5
	add	eax,5
	mov	DWORD PTR[240+rdx],eax

	mov	ecx,0
	mov	r8d,030h
	call	_vpaes_schedule_core
	movaps	xmm6,XMMWORD PTR[16+rsp]
	movaps	xmm7,XMMWORD PTR[32+rsp]
	movaps	xmm8,XMMWORD PTR[48+rsp]
	movaps	xmm9,XMMWORD PTR[64+rsp]
	movaps	xmm10,XMMWORD PTR[80+rsp]
	movaps	xmm11,XMMWORD PTR[96+rsp]
	movaps	xmm12,XMMWORD PTR[112+rsp]
	movaps	xmm13,XMMWORD PTR[128+rsp]
	movaps	xmm14,XMMWORD PTR[144+rsp]
	movaps	xmm15,XMMWORD PTR[160+rsp]
	lea	rsp,QWORD PTR[184+rsp]
$L$enc_key_epilogue::
	xor	eax,eax
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_vpaes_set_encrypt_key::
vpaes_set_encrypt_key	ENDP

PUBLIC	vpaes_set_decrypt_key

ALIGN	16
vpaes_set_decrypt_key	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_set_decrypt_key::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	lea	rsp,QWORD PTR[((-184))+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm6
	movaps	XMMWORD PTR[32+rsp],xmm7
	movaps	XMMWORD PTR[48+rsp],xmm8
	movaps	XMMWORD PTR[64+rsp],xmm9
	movaps	XMMWORD PTR[80+rsp],xmm10
	movaps	XMMWORD PTR[96+rsp],xmm11
	movaps	XMMWORD PTR[112+rsp],xmm12
	movaps	XMMWORD PTR[128+rsp],xmm13
	movaps	XMMWORD PTR[144+rsp],xmm14
	movaps	XMMWORD PTR[160+rsp],xmm15
$L$dec_key_body::
	mov	eax,esi
	shr	eax,5
	add	eax,5
	mov	DWORD PTR[240+rdx],eax
	shl	eax,4
	lea	rdx,QWORD PTR[16+rax*1+rdx]

	mov	ecx,1
	mov	r8d,esi
	shr	r8d,1
	and	r8d,32
	xor	r8d,32
	call	_vpaes_schedule_core
	movaps	xmm6,XMMWORD PTR[16+rsp]
	movaps	xmm7,XMMWORD PTR[32+rsp]
	movaps	xmm8,XMMWORD PTR[48+rsp]
	movaps	xmm9,XMMWORD PTR[64+rsp]
	movaps	xmm10,XMMWORD PTR[80+rsp]
	movaps	xmm11,XMMWORD PTR[96+rsp]
	movaps	xmm12,XMMWORD PTR[112+rsp]
	movaps	xmm13,XMMWORD PTR[128+rsp]
	movaps	xmm14,XMMWORD PTR[144+rsp]
	movaps	xmm15,XMMWORD PTR[160+rsp]
	lea	rsp,QWORD PTR[184+rsp]
$L$dec_key_epilogue::
	xor	eax,eax
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_vpaes_set_decrypt_key::
vpaes_set_decrypt_key	ENDP

PUBLIC	vpaes_encrypt

ALIGN	16
vpaes_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	lea	rsp,QWORD PTR[((-184))+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm6
	movaps	XMMWORD PTR[32+rsp],xmm7
	movaps	XMMWORD PTR[48+rsp],xmm8
	movaps	XMMWORD PTR[64+rsp],xmm9
	movaps	XMMWORD PTR[80+rsp],xmm10
	movaps	XMMWORD PTR[96+rsp],xmm11
	movaps	XMMWORD PTR[112+rsp],xmm12
	movaps	XMMWORD PTR[128+rsp],xmm13
	movaps	XMMWORD PTR[144+rsp],xmm14
	movaps	XMMWORD PTR[160+rsp],xmm15
$L$enc_body::
	movdqu	xmm0,XMMWORD PTR[rdi]
	call	_vpaes_preheat
	call	_vpaes_encrypt_core
	movdqu	XMMWORD PTR[rsi],xmm0
	movaps	xmm6,XMMWORD PTR[16+rsp]
	movaps	xmm7,XMMWORD PTR[32+rsp]
	movaps	xmm8,XMMWORD PTR[48+rsp]
	movaps	xmm9,XMMWORD PTR[64+rsp]
	movaps	xmm10,XMMWORD PTR[80+rsp]
	movaps	xmm11,XMMWORD PTR[96+rsp]
	movaps	xmm12,XMMWORD PTR[112+rsp]
	movaps	xmm13,XMMWORD PTR[128+rsp]
	movaps	xmm14,XMMWORD PTR[144+rsp]
	movaps	xmm15,XMMWORD PTR[160+rsp]
	lea	rsp,QWORD PTR[184+rsp]
$L$enc_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_vpaes_encrypt::
vpaes_encrypt	ENDP

PUBLIC	vpaes_decrypt

ALIGN	16
vpaes_decrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_decrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	lea	rsp,QWORD PTR[((-184))+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm6
	movaps	XMMWORD PTR[32+rsp],xmm7
	movaps	XMMWORD PTR[48+rsp],xmm8
	movaps	XMMWORD PTR[64+rsp],xmm9
	movaps	XMMWORD PTR[80+rsp],xmm10
	movaps	XMMWORD PTR[96+rsp],xmm11
	movaps	XMMWORD PTR[112+rsp],xmm12
	movaps	XMMWORD PTR[128+rsp],xmm13
	movaps	XMMWORD PTR[144+rsp],xmm14
	movaps	XMMWORD PTR[160+rsp],xmm15
$L$dec_body::
	movdqu	xmm0,XMMWORD PTR[rdi]
	call	_vpaes_preheat
	call	_vpaes_decrypt_core
	movdqu	XMMWORD PTR[rsi],xmm0
	movaps	xmm6,XMMWORD PTR[16+rsp]
	movaps	xmm7,XMMWORD PTR[32+rsp]
	movaps	xmm8,XMMWORD PTR[48+rsp]
	movaps	xmm9,XMMWORD PTR[64+rsp]
	movaps	xmm10,XMMWORD PTR[80+rsp]
	movaps	xmm11,XMMWORD PTR[96+rsp]
	movaps	xmm12,XMMWORD PTR[112+rsp]
	movaps	xmm13,XMMWORD PTR[128+rsp]
	movaps	xmm14,XMMWORD PTR[144+rsp]
	movaps	xmm15,XMMWORD PTR[160+rsp]
	lea	rsp,QWORD PTR[184+rsp]
$L$dec_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_vpaes_decrypt::
vpaes_decrypt	ENDP
PUBLIC	vpaes_cbc_encrypt

ALIGN	16
vpaes_cbc_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_vpaes_cbc_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	xchg	rdx,rcx
	sub	rcx,16
	jc	$L$cbc_abort
	lea	rsp,QWORD PTR[((-184))+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm6
	movaps	XMMWORD PTR[32+rsp],xmm7
	movaps	XMMWORD PTR[48+rsp],xmm8
	movaps	XMMWORD PTR[64+rsp],xmm9
	movaps	XMMWORD PTR[80+rsp],xmm10
	movaps	XMMWORD PTR[96+rsp],xmm11
	movaps	XMMWORD PTR[112+rsp],xmm12
	movaps	XMMWORD PTR[128+rsp],xmm13
	movaps	XMMWORD PTR[144+rsp],xmm14
	movaps	XMMWORD PTR[160+rsp],xmm15
$L$cbc_body::
	movdqu	xmm6,XMMWORD PTR[r8]
	sub	rsi,rdi
	call	_vpaes_preheat
	cmp	r9d,0
	je	$L$cbc_dec_loop
	jmp	$L$cbc_enc_loop
ALIGN	16
$L$cbc_enc_loop::
	movdqu	xmm0,XMMWORD PTR[rdi]
	pxor	xmm0,xmm6
	call	_vpaes_encrypt_core
	movdqa	xmm6,xmm0
	movdqu	XMMWORD PTR[rdi*1+rsi],xmm0
	lea	rdi,QWORD PTR[16+rdi]
	sub	rcx,16
	jnc	$L$cbc_enc_loop
	jmp	$L$cbc_done
ALIGN	16
$L$cbc_dec_loop::
	movdqu	xmm0,XMMWORD PTR[rdi]
	movdqa	xmm7,xmm0
	call	_vpaes_decrypt_core
	pxor	xmm0,xmm6
	movdqa	xmm6,xmm7
	movdqu	XMMWORD PTR[rdi*1+rsi],xmm0
	lea	rdi,QWORD PTR[16+rdi]
	sub	rcx,16
	jnc	$L$cbc_dec_loop
$L$cbc_done::
	movdqu	XMMWORD PTR[r8],xmm6
	movaps	xmm6,XMMWORD PTR[16+rsp]
	movaps	xmm7,XMMWORD PTR[32+rsp]
	movaps	xmm8,XMMWORD PTR[48+rsp]
	movaps	xmm9,XMMWORD PTR[64+rsp]
	movaps	xmm10,XMMWORD PTR[80+rsp]
	movaps	xmm11,XMMWORD PTR[96+rsp]
	movaps	xmm12,XMMWORD PTR[112+rsp]
	movaps	xmm13,XMMWORD PTR[128+rsp]
	movaps	xmm14,XMMWORD PTR[144+rsp]
	movaps	xmm15,XMMWORD PTR[160+rsp]
	lea	rsp,QWORD PTR[184+rsp]
$L$cbc_epilogue::
$L$cbc_abort::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_vpaes_cbc_encrypt::
vpaes_cbc_encrypt	ENDP







ALIGN	16
_vpaes_preheat	PROC PRIVATE
	lea	r10,QWORD PTR[$L$k_s0F]
	movdqa	xmm10,XMMWORD PTR[((-32))+r10]
	movdqa	xmm11,XMMWORD PTR[((-16))+r10]
	movdqa	xmm9,XMMWORD PTR[r10]
	movdqa	xmm13,XMMWORD PTR[48+r10]
	movdqa	xmm12,XMMWORD PTR[64+r10]
	movdqa	xmm15,XMMWORD PTR[80+r10]
	movdqa	xmm14,XMMWORD PTR[96+r10]
	DB	0F3h,0C3h		;repret
_vpaes_preheat	ENDP






ALIGN	64
_vpaes_consts::
$L$k_inv::
	DQ	00E05060F0D080180h,0040703090A0B0C02h
	DQ	001040A060F0B0780h,0030D0E0C02050809h

$L$k_s0F::
	DQ	00F0F0F0F0F0F0F0Fh,00F0F0F0F0F0F0F0Fh

$L$k_ipt::
	DQ	0C2B2E8985A2A7000h,0CABAE09052227808h
	DQ	04C01307D317C4D00h,0CD80B1FCB0FDCC81h

$L$k_sb1::
	DQ	0B19BE18FCB503E00h,0A5DF7A6E142AF544h
	DQ	03618D415FAE22300h,03BF7CCC10D2ED9EFh
$L$k_sb2::
	DQ	0E27A93C60B712400h,05EB7E955BC982FCDh
	DQ	069EB88400AE12900h,0C2A163C8AB82234Ah
$L$k_sbo::
	DQ	0D0D26D176FBDC700h,015AABF7AC502A878h
	DQ	0CFE474A55FBB6A00h,08E1E90D1412B35FAh

$L$k_mc_forward::
	DQ	00407060500030201h,00C0F0E0D080B0A09h
	DQ	0080B0A0904070605h,0000302010C0F0E0Dh
	DQ	00C0F0E0D080B0A09h,00407060500030201h
	DQ	0000302010C0F0E0Dh,0080B0A0904070605h

$L$k_mc_backward::
	DQ	00605040702010003h,00E0D0C0F0A09080Bh
	DQ	0020100030E0D0C0Fh,00A09080B06050407h
	DQ	00E0D0C0F0A09080Bh,00605040702010003h
	DQ	00A09080B06050407h,0020100030E0D0C0Fh

$L$k_sr::
	DQ	00706050403020100h,00F0E0D0C0B0A0908h
	DQ	0030E09040F0A0500h,00B06010C07020D08h
	DQ	00F060D040B020900h,0070E050C030A0108h
	DQ	00B0E0104070A0D00h,00306090C0F020508h

$L$k_rcon::
	DQ	01F8391B9AF9DEEB6h,0702A98084D7C7D81h

$L$k_s63::
	DQ	05B5B5B5B5B5B5B5Bh,05B5B5B5B5B5B5B5Bh

$L$k_opt::
	DQ	0FF9F4929D6B66000h,0F7974121DEBE6808h
	DQ	001EDBD5150BCEC00h,0E10D5DB1B05C0CE0h

$L$k_deskew::
	DQ	007E4A34047A4E300h,01DFEB95A5DBEF91Ah
	DQ	05F36B5DC83EA6900h,02841C2ABF49D1E77h





$L$k_dksd::
	DQ	0FEB91A5DA3E44700h,00740E3A45A1DBEF9h
	DQ	041C277F4B5368300h,05FDC69EAAB289D1Eh
$L$k_dksb::
	DQ	09A4FCA1F8550D500h,003D653861CC94C99h
	DQ	0115BEDA7B6FC4A00h,0D993256F7E3482C8h
$L$k_dkse::
	DQ	0D5031CCA1FC9D600h,053859A4C994F5086h
	DQ	0A23196054FDC7BE8h,0CD5EF96A20B31487h
$L$k_dks9::
	DQ	0B6116FC87ED9A700h,04AED933482255BFCh
	DQ	04576516227143300h,08BB89FACE9DAFDCEh





$L$k_dipt::
	DQ	00F505B040B545F00h,0154A411E114E451Ah
	DQ	086E383E660056500h,012771772F491F194h

$L$k_dsb9::
	DQ	0851C03539A86D600h,0CAD51F504F994CC9h
	DQ	0C03B1789ECD74900h,0725E2C9EB2FBA565h
$L$k_dsbd::
	DQ	07D57CCDFE6B1A200h,0F56E9B13882A4439h
	DQ	03CE2FAF724C6CB00h,02931180D15DEEFD3h
$L$k_dsbb::
	DQ	0D022649296B44200h,0602646F6B0F2D404h
	DQ	0C19498A6CD596700h,0F3FF0C3E3255AA6Bh
$L$k_dsbe::
	DQ	046F2929626D4D000h,02242600464B4F6B0h
	DQ	00C55A6CDFFAAC100h,09467F36B98593E32h
$L$k_dsbo::
	DQ	01387EA537EF94000h,0C7AA6DB9D4943E2Dh
	DQ	012D7560F93441D00h,0CA4B8159D8C58E9Ch
DB	86,101,99,116,111,114,32,80,101,114,109,117,116,97,116,105
DB	111,110,32,65,69,83,32,102,111,114,32,120,56,54,95,54
DB	52,47,83,83,83,69,51,44,32,77,105,107,101,32,72,97
DB	109,98,117,114,103,32,40,83,116,97,110,102,111,114,100,32
DB	85,110,105,118,101,114,115,105,116,121,41,0
ALIGN	64

EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	mov	rsi,QWORD PTR[8+r9]
	mov	r11,QWORD PTR[56+r9]

	mov	r10d,DWORD PTR[r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$in_prologue

	lea	rsi,QWORD PTR[16+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

	lea	rax,QWORD PTR[184+rax]

$L$in_prologue::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	mov	rdi,QWORD PTR[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0a548f3fch


	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD PTR[8+rsi]
	mov	r8,QWORD PTR[rsi]
	mov	r9,QWORD PTR[16+rsi]
	mov	r10,QWORD PTR[40+rsi]
	lea	r11,QWORD PTR[56+rsi]
	lea	r12,QWORD PTR[24+rsi]
	mov	QWORD PTR[32+rsp],r10
	mov	QWORD PTR[40+rsp],r11
	mov	QWORD PTR[48+rsp],r12
	mov	QWORD PTR[56+rsp],rcx
	call	QWORD PTR[__imp_RtlVirtualUnwind]

	mov	eax,1
	add	rsp,64
	popfq
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	pop	rdi
	pop	rsi
	DB	0F3h,0C3h		;repret
se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_vpaes_set_encrypt_key
	DD	imagerel $L$SEH_end_vpaes_set_encrypt_key
	DD	imagerel $L$SEH_info_vpaes_set_encrypt_key

	DD	imagerel $L$SEH_begin_vpaes_set_decrypt_key
	DD	imagerel $L$SEH_end_vpaes_set_decrypt_key
	DD	imagerel $L$SEH_info_vpaes_set_decrypt_key

	DD	imagerel $L$SEH_begin_vpaes_encrypt
	DD	imagerel $L$SEH_end_vpaes_encrypt
	DD	imagerel $L$SEH_info_vpaes_encrypt

	DD	imagerel $L$SEH_begin_vpaes_decrypt
	DD	imagerel $L$SEH_end_vpaes_decrypt
	DD	imagerel $L$SEH_info_vpaes_decrypt

	DD	imagerel $L$SEH_begin_vpaes_cbc_encrypt
	DD	imagerel $L$SEH_end_vpaes_cbc_encrypt
	DD	imagerel $L$SEH_info_vpaes_cbc_encrypt

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_vpaes_set_encrypt_key::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$enc_key_body,imagerel $L$enc_key_epilogue

$L$SEH_info_vpaes_set_decrypt_key::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$dec_key_body,imagerel $L$dec_key_epilogue

$L$SEH_info_vpaes_encrypt::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$enc_body,imagerel $L$enc_epilogue

$L$SEH_info_vpaes_decrypt::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$dec_body,imagerel $L$dec_epilogue

$L$SEH_info_vpaes_cbc_encrypt::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$cbc_body,imagerel $L$cbc_epilogue


.xdata	ENDS
END
