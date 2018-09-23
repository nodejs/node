OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	asm_AES_encrypt:NEAR
EXTERN	asm_AES_decrypt:NEAR


ALIGN	64
_bsaes_encrypt8	PROC PRIVATE
	lea	r11,QWORD PTR[$L$BS0]

	movdqa	xmm8,XMMWORD PTR[rax]
	lea	rax,QWORD PTR[16+rax]
	movdqa	xmm7,XMMWORD PTR[80+r11]
	pxor	xmm15,xmm8
	pxor	xmm0,xmm8
	pxor	xmm1,xmm8
	pxor	xmm2,xmm8
DB	102,68,15,56,0,255
DB	102,15,56,0,199
	pxor	xmm3,xmm8
	pxor	xmm4,xmm8
DB	102,15,56,0,207
DB	102,15,56,0,215
	pxor	xmm5,xmm8
	pxor	xmm6,xmm8
DB	102,15,56,0,223
DB	102,15,56,0,231
DB	102,15,56,0,239
DB	102,15,56,0,247
_bsaes_encrypt8_bitslice::
	movdqa	xmm7,XMMWORD PTR[r11]
	movdqa	xmm8,XMMWORD PTR[16+r11]
	movdqa	xmm9,xmm5
	psrlq	xmm5,1
	movdqa	xmm10,xmm3
	psrlq	xmm3,1
	pxor	xmm5,xmm6
	pxor	xmm3,xmm4
	pand	xmm5,xmm7
	pand	xmm3,xmm7
	pxor	xmm6,xmm5
	psllq	xmm5,1
	pxor	xmm4,xmm3
	psllq	xmm3,1
	pxor	xmm5,xmm9
	pxor	xmm3,xmm10
	movdqa	xmm9,xmm1
	psrlq	xmm1,1
	movdqa	xmm10,xmm15
	psrlq	xmm15,1
	pxor	xmm1,xmm2
	pxor	xmm15,xmm0
	pand	xmm1,xmm7
	pand	xmm15,xmm7
	pxor	xmm2,xmm1
	psllq	xmm1,1
	pxor	xmm0,xmm15
	psllq	xmm15,1
	pxor	xmm1,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm7,XMMWORD PTR[32+r11]
	movdqa	xmm9,xmm4
	psrlq	xmm4,2
	movdqa	xmm10,xmm3
	psrlq	xmm3,2
	pxor	xmm4,xmm6
	pxor	xmm3,xmm5
	pand	xmm4,xmm8
	pand	xmm3,xmm8
	pxor	xmm6,xmm4
	psllq	xmm4,2
	pxor	xmm5,xmm3
	psllq	xmm3,2
	pxor	xmm4,xmm9
	pxor	xmm3,xmm10
	movdqa	xmm9,xmm0
	psrlq	xmm0,2
	movdqa	xmm10,xmm15
	psrlq	xmm15,2
	pxor	xmm0,xmm2
	pxor	xmm15,xmm1
	pand	xmm0,xmm8
	pand	xmm15,xmm8
	pxor	xmm2,xmm0
	psllq	xmm0,2
	pxor	xmm1,xmm15
	psllq	xmm15,2
	pxor	xmm0,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm9,xmm2
	psrlq	xmm2,4
	movdqa	xmm10,xmm1
	psrlq	xmm1,4
	pxor	xmm2,xmm6
	pxor	xmm1,xmm5
	pand	xmm2,xmm7
	pand	xmm1,xmm7
	pxor	xmm6,xmm2
	psllq	xmm2,4
	pxor	xmm5,xmm1
	psllq	xmm1,4
	pxor	xmm2,xmm9
	pxor	xmm1,xmm10
	movdqa	xmm9,xmm0
	psrlq	xmm0,4
	movdqa	xmm10,xmm15
	psrlq	xmm15,4
	pxor	xmm0,xmm4
	pxor	xmm15,xmm3
	pand	xmm0,xmm7
	pand	xmm15,xmm7
	pxor	xmm4,xmm0
	psllq	xmm0,4
	pxor	xmm3,xmm15
	psllq	xmm15,4
	pxor	xmm0,xmm9
	pxor	xmm15,xmm10
	dec	r10d
	jmp	$L$enc_sbox
ALIGN	16
$L$enc_loop::
	pxor	xmm15,XMMWORD PTR[rax]
	pxor	xmm0,XMMWORD PTR[16+rax]
	pxor	xmm1,XMMWORD PTR[32+rax]
	pxor	xmm2,XMMWORD PTR[48+rax]
DB	102,68,15,56,0,255
DB	102,15,56,0,199
	pxor	xmm3,XMMWORD PTR[64+rax]
	pxor	xmm4,XMMWORD PTR[80+rax]
DB	102,15,56,0,207
DB	102,15,56,0,215
	pxor	xmm5,XMMWORD PTR[96+rax]
	pxor	xmm6,XMMWORD PTR[112+rax]
DB	102,15,56,0,223
DB	102,15,56,0,231
DB	102,15,56,0,239
DB	102,15,56,0,247
	lea	rax,QWORD PTR[128+rax]
$L$enc_sbox::
	pxor	xmm4,xmm5
	pxor	xmm1,xmm0
	pxor	xmm2,xmm15
	pxor	xmm5,xmm1
	pxor	xmm4,xmm15

	pxor	xmm5,xmm2
	pxor	xmm2,xmm6
	pxor	xmm6,xmm4
	pxor	xmm2,xmm3
	pxor	xmm3,xmm4
	pxor	xmm2,xmm0

	pxor	xmm1,xmm6
	pxor	xmm0,xmm4
	movdqa	xmm10,xmm6
	movdqa	xmm9,xmm0
	movdqa	xmm8,xmm4
	movdqa	xmm12,xmm1
	movdqa	xmm11,xmm5

	pxor	xmm10,xmm3
	pxor	xmm9,xmm1
	pxor	xmm8,xmm2
	movdqa	xmm13,xmm10
	pxor	xmm12,xmm3
	movdqa	xmm7,xmm9
	pxor	xmm11,xmm15
	movdqa	xmm14,xmm10

	por	xmm9,xmm8
	por	xmm10,xmm11
	pxor	xmm14,xmm7
	pand	xmm13,xmm11
	pxor	xmm11,xmm8
	pand	xmm7,xmm8
	pand	xmm14,xmm11
	movdqa	xmm11,xmm2
	pxor	xmm11,xmm15
	pand	xmm12,xmm11
	pxor	xmm10,xmm12
	pxor	xmm9,xmm12
	movdqa	xmm12,xmm6
	movdqa	xmm11,xmm4
	pxor	xmm12,xmm0
	pxor	xmm11,xmm5
	movdqa	xmm8,xmm12
	pand	xmm12,xmm11
	por	xmm8,xmm11
	pxor	xmm7,xmm12
	pxor	xmm10,xmm14
	pxor	xmm9,xmm13
	pxor	xmm8,xmm14
	movdqa	xmm11,xmm1
	pxor	xmm7,xmm13
	movdqa	xmm12,xmm3
	pxor	xmm8,xmm13
	movdqa	xmm13,xmm0
	pand	xmm11,xmm2
	movdqa	xmm14,xmm6
	pand	xmm12,xmm15
	pand	xmm13,xmm4
	por	xmm14,xmm5
	pxor	xmm10,xmm11
	pxor	xmm9,xmm12
	pxor	xmm8,xmm13
	pxor	xmm7,xmm14





	movdqa	xmm11,xmm10
	pand	xmm10,xmm8
	pxor	xmm11,xmm9

	movdqa	xmm13,xmm7
	movdqa	xmm14,xmm11
	pxor	xmm13,xmm10
	pand	xmm14,xmm13

	movdqa	xmm12,xmm8
	pxor	xmm14,xmm9
	pxor	xmm12,xmm7

	pxor	xmm10,xmm9

	pand	xmm12,xmm10

	movdqa	xmm9,xmm13
	pxor	xmm12,xmm7

	pxor	xmm9,xmm12
	pxor	xmm8,xmm12

	pand	xmm9,xmm7

	pxor	xmm13,xmm9
	pxor	xmm8,xmm9

	pand	xmm13,xmm14

	pxor	xmm13,xmm11
	movdqa	xmm11,xmm5
	movdqa	xmm7,xmm4
	movdqa	xmm9,xmm14
	pxor	xmm9,xmm13
	pand	xmm9,xmm5
	pxor	xmm5,xmm4
	pand	xmm4,xmm14
	pand	xmm5,xmm13
	pxor	xmm5,xmm4
	pxor	xmm4,xmm9
	pxor	xmm11,xmm15
	pxor	xmm7,xmm2
	pxor	xmm14,xmm12
	pxor	xmm13,xmm8
	movdqa	xmm10,xmm14
	movdqa	xmm9,xmm12
	pxor	xmm10,xmm13
	pxor	xmm9,xmm8
	pand	xmm10,xmm11
	pand	xmm9,xmm15
	pxor	xmm11,xmm7
	pxor	xmm15,xmm2
	pand	xmm7,xmm14
	pand	xmm2,xmm12
	pand	xmm11,xmm13
	pand	xmm15,xmm8
	pxor	xmm7,xmm11
	pxor	xmm15,xmm2
	pxor	xmm11,xmm10
	pxor	xmm2,xmm9
	pxor	xmm5,xmm11
	pxor	xmm15,xmm11
	pxor	xmm4,xmm7
	pxor	xmm2,xmm7

	movdqa	xmm11,xmm6
	movdqa	xmm7,xmm0
	pxor	xmm11,xmm3
	pxor	xmm7,xmm1
	movdqa	xmm10,xmm14
	movdqa	xmm9,xmm12
	pxor	xmm10,xmm13
	pxor	xmm9,xmm8
	pand	xmm10,xmm11
	pand	xmm9,xmm3
	pxor	xmm11,xmm7
	pxor	xmm3,xmm1
	pand	xmm7,xmm14
	pand	xmm1,xmm12
	pand	xmm11,xmm13
	pand	xmm3,xmm8
	pxor	xmm7,xmm11
	pxor	xmm3,xmm1
	pxor	xmm11,xmm10
	pxor	xmm1,xmm9
	pxor	xmm14,xmm12
	pxor	xmm13,xmm8
	movdqa	xmm10,xmm14
	pxor	xmm10,xmm13
	pand	xmm10,xmm6
	pxor	xmm6,xmm0
	pand	xmm0,xmm14
	pand	xmm6,xmm13
	pxor	xmm6,xmm0
	pxor	xmm0,xmm10
	pxor	xmm6,xmm11
	pxor	xmm3,xmm11
	pxor	xmm0,xmm7
	pxor	xmm1,xmm7
	pxor	xmm6,xmm15
	pxor	xmm0,xmm5
	pxor	xmm3,xmm6
	pxor	xmm5,xmm15
	pxor	xmm15,xmm0

	pxor	xmm0,xmm4
	pxor	xmm4,xmm1
	pxor	xmm1,xmm2
	pxor	xmm2,xmm4
	pxor	xmm3,xmm4

	pxor	xmm5,xmm2
	dec	r10d
	jl	$L$enc_done
	pshufd	xmm7,xmm15,093h
	pshufd	xmm8,xmm0,093h
	pxor	xmm15,xmm7
	pshufd	xmm9,xmm3,093h
	pxor	xmm0,xmm8
	pshufd	xmm10,xmm5,093h
	pxor	xmm3,xmm9
	pshufd	xmm11,xmm2,093h
	pxor	xmm5,xmm10
	pshufd	xmm12,xmm6,093h
	pxor	xmm2,xmm11
	pshufd	xmm13,xmm1,093h
	pxor	xmm6,xmm12
	pshufd	xmm14,xmm4,093h
	pxor	xmm1,xmm13
	pxor	xmm4,xmm14

	pxor	xmm8,xmm15
	pxor	xmm7,xmm4
	pxor	xmm8,xmm4
	pshufd	xmm15,xmm15,04Eh
	pxor	xmm9,xmm0
	pshufd	xmm0,xmm0,04Eh
	pxor	xmm12,xmm2
	pxor	xmm15,xmm7
	pxor	xmm13,xmm6
	pxor	xmm0,xmm8
	pxor	xmm11,xmm5
	pshufd	xmm7,xmm2,04Eh
	pxor	xmm14,xmm1
	pshufd	xmm8,xmm6,04Eh
	pxor	xmm10,xmm3
	pshufd	xmm2,xmm5,04Eh
	pxor	xmm10,xmm4
	pshufd	xmm6,xmm4,04Eh
	pxor	xmm11,xmm4
	pshufd	xmm5,xmm1,04Eh
	pxor	xmm7,xmm11
	pshufd	xmm1,xmm3,04Eh
	pxor	xmm8,xmm12
	pxor	xmm2,xmm10
	pxor	xmm6,xmm14
	pxor	xmm5,xmm13
	movdqa	xmm3,xmm7
	pxor	xmm1,xmm9
	movdqa	xmm4,xmm8
	movdqa	xmm7,XMMWORD PTR[48+r11]
	jnz	$L$enc_loop
	movdqa	xmm7,XMMWORD PTR[64+r11]
	jmp	$L$enc_loop
ALIGN	16
$L$enc_done::
	movdqa	xmm7,XMMWORD PTR[r11]
	movdqa	xmm8,XMMWORD PTR[16+r11]
	movdqa	xmm9,xmm1
	psrlq	xmm1,1
	movdqa	xmm10,xmm2
	psrlq	xmm2,1
	pxor	xmm1,xmm4
	pxor	xmm2,xmm6
	pand	xmm1,xmm7
	pand	xmm2,xmm7
	pxor	xmm4,xmm1
	psllq	xmm1,1
	pxor	xmm6,xmm2
	psllq	xmm2,1
	pxor	xmm1,xmm9
	pxor	xmm2,xmm10
	movdqa	xmm9,xmm3
	psrlq	xmm3,1
	movdqa	xmm10,xmm15
	psrlq	xmm15,1
	pxor	xmm3,xmm5
	pxor	xmm15,xmm0
	pand	xmm3,xmm7
	pand	xmm15,xmm7
	pxor	xmm5,xmm3
	psllq	xmm3,1
	pxor	xmm0,xmm15
	psllq	xmm15,1
	pxor	xmm3,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm7,XMMWORD PTR[32+r11]
	movdqa	xmm9,xmm6
	psrlq	xmm6,2
	movdqa	xmm10,xmm2
	psrlq	xmm2,2
	pxor	xmm6,xmm4
	pxor	xmm2,xmm1
	pand	xmm6,xmm8
	pand	xmm2,xmm8
	pxor	xmm4,xmm6
	psllq	xmm6,2
	pxor	xmm1,xmm2
	psllq	xmm2,2
	pxor	xmm6,xmm9
	pxor	xmm2,xmm10
	movdqa	xmm9,xmm0
	psrlq	xmm0,2
	movdqa	xmm10,xmm15
	psrlq	xmm15,2
	pxor	xmm0,xmm5
	pxor	xmm15,xmm3
	pand	xmm0,xmm8
	pand	xmm15,xmm8
	pxor	xmm5,xmm0
	psllq	xmm0,2
	pxor	xmm3,xmm15
	psllq	xmm15,2
	pxor	xmm0,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm9,xmm5
	psrlq	xmm5,4
	movdqa	xmm10,xmm3
	psrlq	xmm3,4
	pxor	xmm5,xmm4
	pxor	xmm3,xmm1
	pand	xmm5,xmm7
	pand	xmm3,xmm7
	pxor	xmm4,xmm5
	psllq	xmm5,4
	pxor	xmm1,xmm3
	psllq	xmm3,4
	pxor	xmm5,xmm9
	pxor	xmm3,xmm10
	movdqa	xmm9,xmm0
	psrlq	xmm0,4
	movdqa	xmm10,xmm15
	psrlq	xmm15,4
	pxor	xmm0,xmm6
	pxor	xmm15,xmm2
	pand	xmm0,xmm7
	pand	xmm15,xmm7
	pxor	xmm6,xmm0
	psllq	xmm0,4
	pxor	xmm2,xmm15
	psllq	xmm15,4
	pxor	xmm0,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm7,XMMWORD PTR[rax]
	pxor	xmm3,xmm7
	pxor	xmm5,xmm7
	pxor	xmm2,xmm7
	pxor	xmm6,xmm7
	pxor	xmm1,xmm7
	pxor	xmm4,xmm7
	pxor	xmm15,xmm7
	pxor	xmm0,xmm7
	DB	0F3h,0C3h		;repret
_bsaes_encrypt8	ENDP


ALIGN	64
_bsaes_decrypt8	PROC PRIVATE
	lea	r11,QWORD PTR[$L$BS0]

	movdqa	xmm8,XMMWORD PTR[rax]
	lea	rax,QWORD PTR[16+rax]
	movdqa	xmm7,XMMWORD PTR[((-48))+r11]
	pxor	xmm15,xmm8
	pxor	xmm0,xmm8
	pxor	xmm1,xmm8
	pxor	xmm2,xmm8
DB	102,68,15,56,0,255
DB	102,15,56,0,199
	pxor	xmm3,xmm8
	pxor	xmm4,xmm8
DB	102,15,56,0,207
DB	102,15,56,0,215
	pxor	xmm5,xmm8
	pxor	xmm6,xmm8
DB	102,15,56,0,223
DB	102,15,56,0,231
DB	102,15,56,0,239
DB	102,15,56,0,247
	movdqa	xmm7,XMMWORD PTR[r11]
	movdqa	xmm8,XMMWORD PTR[16+r11]
	movdqa	xmm9,xmm5
	psrlq	xmm5,1
	movdqa	xmm10,xmm3
	psrlq	xmm3,1
	pxor	xmm5,xmm6
	pxor	xmm3,xmm4
	pand	xmm5,xmm7
	pand	xmm3,xmm7
	pxor	xmm6,xmm5
	psllq	xmm5,1
	pxor	xmm4,xmm3
	psllq	xmm3,1
	pxor	xmm5,xmm9
	pxor	xmm3,xmm10
	movdqa	xmm9,xmm1
	psrlq	xmm1,1
	movdqa	xmm10,xmm15
	psrlq	xmm15,1
	pxor	xmm1,xmm2
	pxor	xmm15,xmm0
	pand	xmm1,xmm7
	pand	xmm15,xmm7
	pxor	xmm2,xmm1
	psllq	xmm1,1
	pxor	xmm0,xmm15
	psllq	xmm15,1
	pxor	xmm1,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm7,XMMWORD PTR[32+r11]
	movdqa	xmm9,xmm4
	psrlq	xmm4,2
	movdqa	xmm10,xmm3
	psrlq	xmm3,2
	pxor	xmm4,xmm6
	pxor	xmm3,xmm5
	pand	xmm4,xmm8
	pand	xmm3,xmm8
	pxor	xmm6,xmm4
	psllq	xmm4,2
	pxor	xmm5,xmm3
	psllq	xmm3,2
	pxor	xmm4,xmm9
	pxor	xmm3,xmm10
	movdqa	xmm9,xmm0
	psrlq	xmm0,2
	movdqa	xmm10,xmm15
	psrlq	xmm15,2
	pxor	xmm0,xmm2
	pxor	xmm15,xmm1
	pand	xmm0,xmm8
	pand	xmm15,xmm8
	pxor	xmm2,xmm0
	psllq	xmm0,2
	pxor	xmm1,xmm15
	psllq	xmm15,2
	pxor	xmm0,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm9,xmm2
	psrlq	xmm2,4
	movdqa	xmm10,xmm1
	psrlq	xmm1,4
	pxor	xmm2,xmm6
	pxor	xmm1,xmm5
	pand	xmm2,xmm7
	pand	xmm1,xmm7
	pxor	xmm6,xmm2
	psllq	xmm2,4
	pxor	xmm5,xmm1
	psllq	xmm1,4
	pxor	xmm2,xmm9
	pxor	xmm1,xmm10
	movdqa	xmm9,xmm0
	psrlq	xmm0,4
	movdqa	xmm10,xmm15
	psrlq	xmm15,4
	pxor	xmm0,xmm4
	pxor	xmm15,xmm3
	pand	xmm0,xmm7
	pand	xmm15,xmm7
	pxor	xmm4,xmm0
	psllq	xmm0,4
	pxor	xmm3,xmm15
	psllq	xmm15,4
	pxor	xmm0,xmm9
	pxor	xmm15,xmm10
	dec	r10d
	jmp	$L$dec_sbox
ALIGN	16
$L$dec_loop::
	pxor	xmm15,XMMWORD PTR[rax]
	pxor	xmm0,XMMWORD PTR[16+rax]
	pxor	xmm1,XMMWORD PTR[32+rax]
	pxor	xmm2,XMMWORD PTR[48+rax]
DB	102,68,15,56,0,255
DB	102,15,56,0,199
	pxor	xmm3,XMMWORD PTR[64+rax]
	pxor	xmm4,XMMWORD PTR[80+rax]
DB	102,15,56,0,207
DB	102,15,56,0,215
	pxor	xmm5,XMMWORD PTR[96+rax]
	pxor	xmm6,XMMWORD PTR[112+rax]
DB	102,15,56,0,223
DB	102,15,56,0,231
DB	102,15,56,0,239
DB	102,15,56,0,247
	lea	rax,QWORD PTR[128+rax]
$L$dec_sbox::
	pxor	xmm2,xmm3

	pxor	xmm3,xmm6
	pxor	xmm1,xmm6
	pxor	xmm5,xmm3
	pxor	xmm6,xmm5
	pxor	xmm0,xmm6

	pxor	xmm15,xmm0
	pxor	xmm1,xmm4
	pxor	xmm2,xmm15
	pxor	xmm4,xmm15
	pxor	xmm0,xmm2
	movdqa	xmm10,xmm2
	movdqa	xmm9,xmm6
	movdqa	xmm8,xmm0
	movdqa	xmm12,xmm3
	movdqa	xmm11,xmm4

	pxor	xmm10,xmm15
	pxor	xmm9,xmm3
	pxor	xmm8,xmm5
	movdqa	xmm13,xmm10
	pxor	xmm12,xmm15
	movdqa	xmm7,xmm9
	pxor	xmm11,xmm1
	movdqa	xmm14,xmm10

	por	xmm9,xmm8
	por	xmm10,xmm11
	pxor	xmm14,xmm7
	pand	xmm13,xmm11
	pxor	xmm11,xmm8
	pand	xmm7,xmm8
	pand	xmm14,xmm11
	movdqa	xmm11,xmm5
	pxor	xmm11,xmm1
	pand	xmm12,xmm11
	pxor	xmm10,xmm12
	pxor	xmm9,xmm12
	movdqa	xmm12,xmm2
	movdqa	xmm11,xmm0
	pxor	xmm12,xmm6
	pxor	xmm11,xmm4
	movdqa	xmm8,xmm12
	pand	xmm12,xmm11
	por	xmm8,xmm11
	pxor	xmm7,xmm12
	pxor	xmm10,xmm14
	pxor	xmm9,xmm13
	pxor	xmm8,xmm14
	movdqa	xmm11,xmm3
	pxor	xmm7,xmm13
	movdqa	xmm12,xmm15
	pxor	xmm8,xmm13
	movdqa	xmm13,xmm6
	pand	xmm11,xmm5
	movdqa	xmm14,xmm2
	pand	xmm12,xmm1
	pand	xmm13,xmm0
	por	xmm14,xmm4
	pxor	xmm10,xmm11
	pxor	xmm9,xmm12
	pxor	xmm8,xmm13
	pxor	xmm7,xmm14





	movdqa	xmm11,xmm10
	pand	xmm10,xmm8
	pxor	xmm11,xmm9

	movdqa	xmm13,xmm7
	movdqa	xmm14,xmm11
	pxor	xmm13,xmm10
	pand	xmm14,xmm13

	movdqa	xmm12,xmm8
	pxor	xmm14,xmm9
	pxor	xmm12,xmm7

	pxor	xmm10,xmm9

	pand	xmm12,xmm10

	movdqa	xmm9,xmm13
	pxor	xmm12,xmm7

	pxor	xmm9,xmm12
	pxor	xmm8,xmm12

	pand	xmm9,xmm7

	pxor	xmm13,xmm9
	pxor	xmm8,xmm9

	pand	xmm13,xmm14

	pxor	xmm13,xmm11
	movdqa	xmm11,xmm4
	movdqa	xmm7,xmm0
	movdqa	xmm9,xmm14
	pxor	xmm9,xmm13
	pand	xmm9,xmm4
	pxor	xmm4,xmm0
	pand	xmm0,xmm14
	pand	xmm4,xmm13
	pxor	xmm4,xmm0
	pxor	xmm0,xmm9
	pxor	xmm11,xmm1
	pxor	xmm7,xmm5
	pxor	xmm14,xmm12
	pxor	xmm13,xmm8
	movdqa	xmm10,xmm14
	movdqa	xmm9,xmm12
	pxor	xmm10,xmm13
	pxor	xmm9,xmm8
	pand	xmm10,xmm11
	pand	xmm9,xmm1
	pxor	xmm11,xmm7
	pxor	xmm1,xmm5
	pand	xmm7,xmm14
	pand	xmm5,xmm12
	pand	xmm11,xmm13
	pand	xmm1,xmm8
	pxor	xmm7,xmm11
	pxor	xmm1,xmm5
	pxor	xmm11,xmm10
	pxor	xmm5,xmm9
	pxor	xmm4,xmm11
	pxor	xmm1,xmm11
	pxor	xmm0,xmm7
	pxor	xmm5,xmm7

	movdqa	xmm11,xmm2
	movdqa	xmm7,xmm6
	pxor	xmm11,xmm15
	pxor	xmm7,xmm3
	movdqa	xmm10,xmm14
	movdqa	xmm9,xmm12
	pxor	xmm10,xmm13
	pxor	xmm9,xmm8
	pand	xmm10,xmm11
	pand	xmm9,xmm15
	pxor	xmm11,xmm7
	pxor	xmm15,xmm3
	pand	xmm7,xmm14
	pand	xmm3,xmm12
	pand	xmm11,xmm13
	pand	xmm15,xmm8
	pxor	xmm7,xmm11
	pxor	xmm15,xmm3
	pxor	xmm11,xmm10
	pxor	xmm3,xmm9
	pxor	xmm14,xmm12
	pxor	xmm13,xmm8
	movdqa	xmm10,xmm14
	pxor	xmm10,xmm13
	pand	xmm10,xmm2
	pxor	xmm2,xmm6
	pand	xmm6,xmm14
	pand	xmm2,xmm13
	pxor	xmm2,xmm6
	pxor	xmm6,xmm10
	pxor	xmm2,xmm11
	pxor	xmm15,xmm11
	pxor	xmm6,xmm7
	pxor	xmm3,xmm7
	pxor	xmm0,xmm6
	pxor	xmm5,xmm4

	pxor	xmm3,xmm0
	pxor	xmm1,xmm6
	pxor	xmm4,xmm6
	pxor	xmm3,xmm1
	pxor	xmm6,xmm15
	pxor	xmm3,xmm4
	pxor	xmm2,xmm5
	pxor	xmm5,xmm0
	pxor	xmm2,xmm3

	pxor	xmm3,xmm15
	pxor	xmm6,xmm2
	dec	r10d
	jl	$L$dec_done

	pshufd	xmm7,xmm15,04Eh
	pshufd	xmm13,xmm2,04Eh
	pxor	xmm7,xmm15
	pshufd	xmm14,xmm4,04Eh
	pxor	xmm13,xmm2
	pshufd	xmm8,xmm0,04Eh
	pxor	xmm14,xmm4
	pshufd	xmm9,xmm5,04Eh
	pxor	xmm8,xmm0
	pshufd	xmm10,xmm3,04Eh
	pxor	xmm9,xmm5
	pxor	xmm15,xmm13
	pxor	xmm0,xmm13
	pshufd	xmm11,xmm1,04Eh
	pxor	xmm10,xmm3
	pxor	xmm5,xmm7
	pxor	xmm3,xmm8
	pshufd	xmm12,xmm6,04Eh
	pxor	xmm11,xmm1
	pxor	xmm0,xmm14
	pxor	xmm1,xmm9
	pxor	xmm12,xmm6

	pxor	xmm5,xmm14
	pxor	xmm3,xmm13
	pxor	xmm1,xmm13
	pxor	xmm6,xmm10
	pxor	xmm2,xmm11
	pxor	xmm1,xmm14
	pxor	xmm6,xmm14
	pxor	xmm4,xmm12
	pshufd	xmm7,xmm15,093h
	pshufd	xmm8,xmm0,093h
	pxor	xmm15,xmm7
	pshufd	xmm9,xmm5,093h
	pxor	xmm0,xmm8
	pshufd	xmm10,xmm3,093h
	pxor	xmm5,xmm9
	pshufd	xmm11,xmm1,093h
	pxor	xmm3,xmm10
	pshufd	xmm12,xmm6,093h
	pxor	xmm1,xmm11
	pshufd	xmm13,xmm2,093h
	pxor	xmm6,xmm12
	pshufd	xmm14,xmm4,093h
	pxor	xmm2,xmm13
	pxor	xmm4,xmm14

	pxor	xmm8,xmm15
	pxor	xmm7,xmm4
	pxor	xmm8,xmm4
	pshufd	xmm15,xmm15,04Eh
	pxor	xmm9,xmm0
	pshufd	xmm0,xmm0,04Eh
	pxor	xmm12,xmm1
	pxor	xmm15,xmm7
	pxor	xmm13,xmm6
	pxor	xmm0,xmm8
	pxor	xmm11,xmm3
	pshufd	xmm7,xmm1,04Eh
	pxor	xmm14,xmm2
	pshufd	xmm8,xmm6,04Eh
	pxor	xmm10,xmm5
	pshufd	xmm1,xmm3,04Eh
	pxor	xmm10,xmm4
	pshufd	xmm6,xmm4,04Eh
	pxor	xmm11,xmm4
	pshufd	xmm3,xmm2,04Eh
	pxor	xmm7,xmm11
	pshufd	xmm2,xmm5,04Eh
	pxor	xmm8,xmm12
	pxor	xmm10,xmm1
	pxor	xmm6,xmm14
	pxor	xmm13,xmm3
	movdqa	xmm3,xmm7
	pxor	xmm2,xmm9
	movdqa	xmm5,xmm13
	movdqa	xmm4,xmm8
	movdqa	xmm1,xmm2
	movdqa	xmm2,xmm10
	movdqa	xmm7,XMMWORD PTR[((-16))+r11]
	jnz	$L$dec_loop
	movdqa	xmm7,XMMWORD PTR[((-32))+r11]
	jmp	$L$dec_loop
ALIGN	16
$L$dec_done::
	movdqa	xmm7,XMMWORD PTR[r11]
	movdqa	xmm8,XMMWORD PTR[16+r11]
	movdqa	xmm9,xmm2
	psrlq	xmm2,1
	movdqa	xmm10,xmm1
	psrlq	xmm1,1
	pxor	xmm2,xmm4
	pxor	xmm1,xmm6
	pand	xmm2,xmm7
	pand	xmm1,xmm7
	pxor	xmm4,xmm2
	psllq	xmm2,1
	pxor	xmm6,xmm1
	psllq	xmm1,1
	pxor	xmm2,xmm9
	pxor	xmm1,xmm10
	movdqa	xmm9,xmm5
	psrlq	xmm5,1
	movdqa	xmm10,xmm15
	psrlq	xmm15,1
	pxor	xmm5,xmm3
	pxor	xmm15,xmm0
	pand	xmm5,xmm7
	pand	xmm15,xmm7
	pxor	xmm3,xmm5
	psllq	xmm5,1
	pxor	xmm0,xmm15
	psllq	xmm15,1
	pxor	xmm5,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm7,XMMWORD PTR[32+r11]
	movdqa	xmm9,xmm6
	psrlq	xmm6,2
	movdqa	xmm10,xmm1
	psrlq	xmm1,2
	pxor	xmm6,xmm4
	pxor	xmm1,xmm2
	pand	xmm6,xmm8
	pand	xmm1,xmm8
	pxor	xmm4,xmm6
	psllq	xmm6,2
	pxor	xmm2,xmm1
	psllq	xmm1,2
	pxor	xmm6,xmm9
	pxor	xmm1,xmm10
	movdqa	xmm9,xmm0
	psrlq	xmm0,2
	movdqa	xmm10,xmm15
	psrlq	xmm15,2
	pxor	xmm0,xmm3
	pxor	xmm15,xmm5
	pand	xmm0,xmm8
	pand	xmm15,xmm8
	pxor	xmm3,xmm0
	psllq	xmm0,2
	pxor	xmm5,xmm15
	psllq	xmm15,2
	pxor	xmm0,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm9,xmm3
	psrlq	xmm3,4
	movdqa	xmm10,xmm5
	psrlq	xmm5,4
	pxor	xmm3,xmm4
	pxor	xmm5,xmm2
	pand	xmm3,xmm7
	pand	xmm5,xmm7
	pxor	xmm4,xmm3
	psllq	xmm3,4
	pxor	xmm2,xmm5
	psllq	xmm5,4
	pxor	xmm3,xmm9
	pxor	xmm5,xmm10
	movdqa	xmm9,xmm0
	psrlq	xmm0,4
	movdqa	xmm10,xmm15
	psrlq	xmm15,4
	pxor	xmm0,xmm6
	pxor	xmm15,xmm1
	pand	xmm0,xmm7
	pand	xmm15,xmm7
	pxor	xmm6,xmm0
	psllq	xmm0,4
	pxor	xmm1,xmm15
	psllq	xmm15,4
	pxor	xmm0,xmm9
	pxor	xmm15,xmm10
	movdqa	xmm7,XMMWORD PTR[rax]
	pxor	xmm5,xmm7
	pxor	xmm3,xmm7
	pxor	xmm1,xmm7
	pxor	xmm6,xmm7
	pxor	xmm2,xmm7
	pxor	xmm4,xmm7
	pxor	xmm15,xmm7
	pxor	xmm0,xmm7
	DB	0F3h,0C3h		;repret
_bsaes_decrypt8	ENDP

ALIGN	16
_bsaes_key_convert	PROC PRIVATE
	lea	r11,QWORD PTR[$L$masks]
	movdqu	xmm7,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	movdqa	xmm0,XMMWORD PTR[r11]
	movdqa	xmm1,XMMWORD PTR[16+r11]
	movdqa	xmm2,XMMWORD PTR[32+r11]
	movdqa	xmm3,XMMWORD PTR[48+r11]
	movdqa	xmm4,XMMWORD PTR[64+r11]
	pcmpeqd	xmm5,xmm5

	movdqu	xmm6,XMMWORD PTR[rcx]
	movdqa	XMMWORD PTR[rax],xmm7
	lea	rax,QWORD PTR[16+rax]
	dec	r10d
	jmp	$L$key_loop
ALIGN	16
$L$key_loop::
DB	102,15,56,0,244

	movdqa	xmm8,xmm0
	movdqa	xmm9,xmm1

	pand	xmm8,xmm6
	pand	xmm9,xmm6
	movdqa	xmm10,xmm2
	pcmpeqb	xmm8,xmm0
	psllq	xmm0,4
	movdqa	xmm11,xmm3
	pcmpeqb	xmm9,xmm1
	psllq	xmm1,4

	pand	xmm10,xmm6
	pand	xmm11,xmm6
	movdqa	xmm12,xmm0
	pcmpeqb	xmm10,xmm2
	psllq	xmm2,4
	movdqa	xmm13,xmm1
	pcmpeqb	xmm11,xmm3
	psllq	xmm3,4

	movdqa	xmm14,xmm2
	movdqa	xmm15,xmm3
	pxor	xmm8,xmm5
	pxor	xmm9,xmm5

	pand	xmm12,xmm6
	pand	xmm13,xmm6
	movdqa	XMMWORD PTR[rax],xmm8
	pcmpeqb	xmm12,xmm0
	psrlq	xmm0,4
	movdqa	XMMWORD PTR[16+rax],xmm9
	pcmpeqb	xmm13,xmm1
	psrlq	xmm1,4
	lea	rcx,QWORD PTR[16+rcx]

	pand	xmm14,xmm6
	pand	xmm15,xmm6
	movdqa	XMMWORD PTR[32+rax],xmm10
	pcmpeqb	xmm14,xmm2
	psrlq	xmm2,4
	movdqa	XMMWORD PTR[48+rax],xmm11
	pcmpeqb	xmm15,xmm3
	psrlq	xmm3,4
	movdqu	xmm6,XMMWORD PTR[rcx]

	pxor	xmm13,xmm5
	pxor	xmm14,xmm5
	movdqa	XMMWORD PTR[64+rax],xmm12
	movdqa	XMMWORD PTR[80+rax],xmm13
	movdqa	XMMWORD PTR[96+rax],xmm14
	movdqa	XMMWORD PTR[112+rax],xmm15
	lea	rax,QWORD PTR[128+rax]
	dec	r10d
	jnz	$L$key_loop

	movdqa	xmm7,XMMWORD PTR[80+r11]

	DB	0F3h,0C3h		;repret
_bsaes_key_convert	ENDP
EXTERN	asm_AES_cbc_encrypt:NEAR
PUBLIC	bsaes_cbc_encrypt

ALIGN	16
bsaes_cbc_encrypt	PROC PUBLIC
	mov	r11d,DWORD PTR[48+rsp]
	cmp	r11d,0
	jne	asm_AES_cbc_encrypt
	cmp	r8,128
	jb	asm_AES_cbc_encrypt

	mov	rax,rsp
$L$cbc_dec_prologue::
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-72))+rsp]
	mov	r10,QWORD PTR[160+rsp]
	lea	rsp,QWORD PTR[((-160))+rsp]
	movaps	XMMWORD PTR[64+rsp],xmm6
	movaps	XMMWORD PTR[80+rsp],xmm7
	movaps	XMMWORD PTR[96+rsp],xmm8
	movaps	XMMWORD PTR[112+rsp],xmm9
	movaps	XMMWORD PTR[128+rsp],xmm10
	movaps	XMMWORD PTR[144+rsp],xmm11
	movaps	XMMWORD PTR[160+rsp],xmm12
	movaps	XMMWORD PTR[176+rsp],xmm13
	movaps	XMMWORD PTR[192+rsp],xmm14
	movaps	XMMWORD PTR[208+rsp],xmm15
$L$cbc_dec_body::
	mov	rbp,rsp
	mov	eax,DWORD PTR[240+r9]
	mov	r12,rcx
	mov	r13,rdx
	mov	r14,r8
	mov	r15,r9
	mov	rbx,r10
	shr	r14,4

	mov	edx,eax
	shl	rax,7
	sub	rax,96
	sub	rsp,rax

	mov	rax,rsp
	mov	rcx,r15
	mov	r10d,edx
	call	_bsaes_key_convert
	pxor	xmm7,XMMWORD PTR[rsp]
	movdqa	XMMWORD PTR[rax],xmm6
	movdqa	XMMWORD PTR[rsp],xmm7

	movdqu	xmm14,XMMWORD PTR[rbx]
	sub	r14,8
$L$cbc_dec_loop::
	movdqu	xmm15,XMMWORD PTR[r12]
	movdqu	xmm0,XMMWORD PTR[16+r12]
	movdqu	xmm1,XMMWORD PTR[32+r12]
	movdqu	xmm2,XMMWORD PTR[48+r12]
	movdqu	xmm3,XMMWORD PTR[64+r12]
	movdqu	xmm4,XMMWORD PTR[80+r12]
	mov	rax,rsp
	movdqu	xmm5,XMMWORD PTR[96+r12]
	mov	r10d,edx
	movdqu	xmm6,XMMWORD PTR[112+r12]
	movdqa	XMMWORD PTR[32+rbp],xmm14

	call	_bsaes_decrypt8

	pxor	xmm15,XMMWORD PTR[32+rbp]
	movdqu	xmm7,XMMWORD PTR[r12]
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm0,xmm7
	movdqu	xmm9,XMMWORD PTR[32+r12]
	pxor	xmm5,xmm8
	movdqu	xmm10,XMMWORD PTR[48+r12]
	pxor	xmm3,xmm9
	movdqu	xmm11,XMMWORD PTR[64+r12]
	pxor	xmm1,xmm10
	movdqu	xmm12,XMMWORD PTR[80+r12]
	pxor	xmm6,xmm11
	movdqu	xmm13,XMMWORD PTR[96+r12]
	pxor	xmm2,xmm12
	movdqu	xmm14,XMMWORD PTR[112+r12]
	pxor	xmm4,xmm13
	movdqu	XMMWORD PTR[r13],xmm15
	lea	r12,QWORD PTR[128+r12]
	movdqu	XMMWORD PTR[16+r13],xmm0
	movdqu	XMMWORD PTR[32+r13],xmm5
	movdqu	XMMWORD PTR[48+r13],xmm3
	movdqu	XMMWORD PTR[64+r13],xmm1
	movdqu	XMMWORD PTR[80+r13],xmm6
	movdqu	XMMWORD PTR[96+r13],xmm2
	movdqu	XMMWORD PTR[112+r13],xmm4
	lea	r13,QWORD PTR[128+r13]
	sub	r14,8
	jnc	$L$cbc_dec_loop

	add	r14,8
	jz	$L$cbc_dec_done

	movdqu	xmm15,XMMWORD PTR[r12]
	mov	rax,rsp
	mov	r10d,edx
	cmp	r14,2
	jb	$L$cbc_dec_one
	movdqu	xmm0,XMMWORD PTR[16+r12]
	je	$L$cbc_dec_two
	movdqu	xmm1,XMMWORD PTR[32+r12]
	cmp	r14,4
	jb	$L$cbc_dec_three
	movdqu	xmm2,XMMWORD PTR[48+r12]
	je	$L$cbc_dec_four
	movdqu	xmm3,XMMWORD PTR[64+r12]
	cmp	r14,6
	jb	$L$cbc_dec_five
	movdqu	xmm4,XMMWORD PTR[80+r12]
	je	$L$cbc_dec_six
	movdqu	xmm5,XMMWORD PTR[96+r12]
	movdqa	XMMWORD PTR[32+rbp],xmm14
	call	_bsaes_decrypt8
	pxor	xmm15,XMMWORD PTR[32+rbp]
	movdqu	xmm7,XMMWORD PTR[r12]
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm0,xmm7
	movdqu	xmm9,XMMWORD PTR[32+r12]
	pxor	xmm5,xmm8
	movdqu	xmm10,XMMWORD PTR[48+r12]
	pxor	xmm3,xmm9
	movdqu	xmm11,XMMWORD PTR[64+r12]
	pxor	xmm1,xmm10
	movdqu	xmm12,XMMWORD PTR[80+r12]
	pxor	xmm6,xmm11
	movdqu	xmm14,XMMWORD PTR[96+r12]
	pxor	xmm2,xmm12
	movdqu	XMMWORD PTR[r13],xmm15
	movdqu	XMMWORD PTR[16+r13],xmm0
	movdqu	XMMWORD PTR[32+r13],xmm5
	movdqu	XMMWORD PTR[48+r13],xmm3
	movdqu	XMMWORD PTR[64+r13],xmm1
	movdqu	XMMWORD PTR[80+r13],xmm6
	movdqu	XMMWORD PTR[96+r13],xmm2
	jmp	$L$cbc_dec_done
ALIGN	16
$L$cbc_dec_six::
	movdqa	XMMWORD PTR[32+rbp],xmm14
	call	_bsaes_decrypt8
	pxor	xmm15,XMMWORD PTR[32+rbp]
	movdqu	xmm7,XMMWORD PTR[r12]
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm0,xmm7
	movdqu	xmm9,XMMWORD PTR[32+r12]
	pxor	xmm5,xmm8
	movdqu	xmm10,XMMWORD PTR[48+r12]
	pxor	xmm3,xmm9
	movdqu	xmm11,XMMWORD PTR[64+r12]
	pxor	xmm1,xmm10
	movdqu	xmm14,XMMWORD PTR[80+r12]
	pxor	xmm6,xmm11
	movdqu	XMMWORD PTR[r13],xmm15
	movdqu	XMMWORD PTR[16+r13],xmm0
	movdqu	XMMWORD PTR[32+r13],xmm5
	movdqu	XMMWORD PTR[48+r13],xmm3
	movdqu	XMMWORD PTR[64+r13],xmm1
	movdqu	XMMWORD PTR[80+r13],xmm6
	jmp	$L$cbc_dec_done
ALIGN	16
$L$cbc_dec_five::
	movdqa	XMMWORD PTR[32+rbp],xmm14
	call	_bsaes_decrypt8
	pxor	xmm15,XMMWORD PTR[32+rbp]
	movdqu	xmm7,XMMWORD PTR[r12]
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm0,xmm7
	movdqu	xmm9,XMMWORD PTR[32+r12]
	pxor	xmm5,xmm8
	movdqu	xmm10,XMMWORD PTR[48+r12]
	pxor	xmm3,xmm9
	movdqu	xmm14,XMMWORD PTR[64+r12]
	pxor	xmm1,xmm10
	movdqu	XMMWORD PTR[r13],xmm15
	movdqu	XMMWORD PTR[16+r13],xmm0
	movdqu	XMMWORD PTR[32+r13],xmm5
	movdqu	XMMWORD PTR[48+r13],xmm3
	movdqu	XMMWORD PTR[64+r13],xmm1
	jmp	$L$cbc_dec_done
ALIGN	16
$L$cbc_dec_four::
	movdqa	XMMWORD PTR[32+rbp],xmm14
	call	_bsaes_decrypt8
	pxor	xmm15,XMMWORD PTR[32+rbp]
	movdqu	xmm7,XMMWORD PTR[r12]
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm0,xmm7
	movdqu	xmm9,XMMWORD PTR[32+r12]
	pxor	xmm5,xmm8
	movdqu	xmm14,XMMWORD PTR[48+r12]
	pxor	xmm3,xmm9
	movdqu	XMMWORD PTR[r13],xmm15
	movdqu	XMMWORD PTR[16+r13],xmm0
	movdqu	XMMWORD PTR[32+r13],xmm5
	movdqu	XMMWORD PTR[48+r13],xmm3
	jmp	$L$cbc_dec_done
ALIGN	16
$L$cbc_dec_three::
	movdqa	XMMWORD PTR[32+rbp],xmm14
	call	_bsaes_decrypt8
	pxor	xmm15,XMMWORD PTR[32+rbp]
	movdqu	xmm7,XMMWORD PTR[r12]
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm0,xmm7
	movdqu	xmm14,XMMWORD PTR[32+r12]
	pxor	xmm5,xmm8
	movdqu	XMMWORD PTR[r13],xmm15
	movdqu	XMMWORD PTR[16+r13],xmm0
	movdqu	XMMWORD PTR[32+r13],xmm5
	jmp	$L$cbc_dec_done
ALIGN	16
$L$cbc_dec_two::
	movdqa	XMMWORD PTR[32+rbp],xmm14
	call	_bsaes_decrypt8
	pxor	xmm15,XMMWORD PTR[32+rbp]
	movdqu	xmm7,XMMWORD PTR[r12]
	movdqu	xmm14,XMMWORD PTR[16+r12]
	pxor	xmm0,xmm7
	movdqu	XMMWORD PTR[r13],xmm15
	movdqu	XMMWORD PTR[16+r13],xmm0
	jmp	$L$cbc_dec_done
ALIGN	16
$L$cbc_dec_one::
	lea	rcx,QWORD PTR[r12]
	lea	rdx,QWORD PTR[32+rbp]
	lea	r8,QWORD PTR[r15]
	call	asm_AES_decrypt
	pxor	xmm14,XMMWORD PTR[32+rbp]
	movdqu	XMMWORD PTR[r13],xmm14
	movdqa	xmm14,xmm15

$L$cbc_dec_done::
	movdqu	XMMWORD PTR[rbx],xmm14
	lea	rax,QWORD PTR[rsp]
	pxor	xmm0,xmm0
$L$cbc_dec_bzero::
	movdqa	XMMWORD PTR[rax],xmm0
	movdqa	XMMWORD PTR[16+rax],xmm0
	lea	rax,QWORD PTR[32+rax]
	cmp	rbp,rax
	ja	$L$cbc_dec_bzero

	lea	rsp,QWORD PTR[rbp]
	movaps	xmm6,XMMWORD PTR[64+rbp]
	movaps	xmm7,XMMWORD PTR[80+rbp]
	movaps	xmm8,XMMWORD PTR[96+rbp]
	movaps	xmm9,XMMWORD PTR[112+rbp]
	movaps	xmm10,XMMWORD PTR[128+rbp]
	movaps	xmm11,XMMWORD PTR[144+rbp]
	movaps	xmm12,XMMWORD PTR[160+rbp]
	movaps	xmm13,XMMWORD PTR[176+rbp]
	movaps	xmm14,XMMWORD PTR[192+rbp]
	movaps	xmm15,XMMWORD PTR[208+rbp]
	lea	rsp,QWORD PTR[160+rbp]
	mov	r15,QWORD PTR[72+rsp]
	mov	r14,QWORD PTR[80+rsp]
	mov	r13,QWORD PTR[88+rsp]
	mov	r12,QWORD PTR[96+rsp]
	mov	rbx,QWORD PTR[104+rsp]
	mov	rax,QWORD PTR[112+rsp]
	lea	rsp,QWORD PTR[120+rsp]
	mov	rbp,rax
$L$cbc_dec_epilogue::
	DB	0F3h,0C3h		;repret
bsaes_cbc_encrypt	ENDP

PUBLIC	bsaes_ctr32_encrypt_blocks

ALIGN	16
bsaes_ctr32_encrypt_blocks	PROC PUBLIC
	mov	rax,rsp
$L$ctr_enc_prologue::
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-72))+rsp]
	mov	r10,QWORD PTR[160+rsp]
	lea	rsp,QWORD PTR[((-160))+rsp]
	movaps	XMMWORD PTR[64+rsp],xmm6
	movaps	XMMWORD PTR[80+rsp],xmm7
	movaps	XMMWORD PTR[96+rsp],xmm8
	movaps	XMMWORD PTR[112+rsp],xmm9
	movaps	XMMWORD PTR[128+rsp],xmm10
	movaps	XMMWORD PTR[144+rsp],xmm11
	movaps	XMMWORD PTR[160+rsp],xmm12
	movaps	XMMWORD PTR[176+rsp],xmm13
	movaps	XMMWORD PTR[192+rsp],xmm14
	movaps	XMMWORD PTR[208+rsp],xmm15
$L$ctr_enc_body::
	mov	rbp,rsp
	movdqu	xmm0,XMMWORD PTR[r10]
	mov	eax,DWORD PTR[240+r9]
	mov	r12,rcx
	mov	r13,rdx
	mov	r14,r8
	mov	r15,r9
	movdqa	XMMWORD PTR[32+rbp],xmm0
	cmp	r8,8
	jb	$L$ctr_enc_short

	mov	ebx,eax
	shl	rax,7
	sub	rax,96
	sub	rsp,rax

	mov	rax,rsp
	mov	rcx,r15
	mov	r10d,ebx
	call	_bsaes_key_convert
	pxor	xmm7,xmm6
	movdqa	XMMWORD PTR[rax],xmm7

	movdqa	xmm8,XMMWORD PTR[rsp]
	lea	r11,QWORD PTR[$L$ADD1]
	movdqa	xmm15,XMMWORD PTR[32+rbp]
	movdqa	xmm7,XMMWORD PTR[((-32))+r11]
DB	102,68,15,56,0,199
DB	102,68,15,56,0,255
	movdqa	XMMWORD PTR[rsp],xmm8
	jmp	$L$ctr_enc_loop
ALIGN	16
$L$ctr_enc_loop::
	movdqa	XMMWORD PTR[32+rbp],xmm15
	movdqa	xmm0,xmm15
	movdqa	xmm1,xmm15
	paddd	xmm0,XMMWORD PTR[r11]
	movdqa	xmm2,xmm15
	paddd	xmm1,XMMWORD PTR[16+r11]
	movdqa	xmm3,xmm15
	paddd	xmm2,XMMWORD PTR[32+r11]
	movdqa	xmm4,xmm15
	paddd	xmm3,XMMWORD PTR[48+r11]
	movdqa	xmm5,xmm15
	paddd	xmm4,XMMWORD PTR[64+r11]
	movdqa	xmm6,xmm15
	paddd	xmm5,XMMWORD PTR[80+r11]
	paddd	xmm6,XMMWORD PTR[96+r11]



	movdqa	xmm8,XMMWORD PTR[rsp]
	lea	rax,QWORD PTR[16+rsp]
	movdqa	xmm7,XMMWORD PTR[((-16))+r11]
	pxor	xmm15,xmm8
	pxor	xmm0,xmm8
	pxor	xmm1,xmm8
	pxor	xmm2,xmm8
DB	102,68,15,56,0,255
DB	102,15,56,0,199
	pxor	xmm3,xmm8
	pxor	xmm4,xmm8
DB	102,15,56,0,207
DB	102,15,56,0,215
	pxor	xmm5,xmm8
	pxor	xmm6,xmm8
DB	102,15,56,0,223
DB	102,15,56,0,231
DB	102,15,56,0,239
DB	102,15,56,0,247
	lea	r11,QWORD PTR[$L$BS0]
	mov	r10d,ebx

	call	_bsaes_encrypt8_bitslice

	sub	r14,8
	jc	$L$ctr_enc_loop_done

	movdqu	xmm7,XMMWORD PTR[r12]
	movdqu	xmm8,XMMWORD PTR[16+r12]
	movdqu	xmm9,XMMWORD PTR[32+r12]
	movdqu	xmm10,XMMWORD PTR[48+r12]
	movdqu	xmm11,XMMWORD PTR[64+r12]
	movdqu	xmm12,XMMWORD PTR[80+r12]
	movdqu	xmm13,XMMWORD PTR[96+r12]
	movdqu	xmm14,XMMWORD PTR[112+r12]
	lea	r12,QWORD PTR[128+r12]
	pxor	xmm7,xmm15
	movdqa	xmm15,XMMWORD PTR[32+rbp]
	pxor	xmm0,xmm8
	movdqu	XMMWORD PTR[r13],xmm7
	pxor	xmm3,xmm9
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm5,xmm10
	movdqu	XMMWORD PTR[32+r13],xmm3
	pxor	xmm2,xmm11
	movdqu	XMMWORD PTR[48+r13],xmm5
	pxor	xmm6,xmm12
	movdqu	XMMWORD PTR[64+r13],xmm2
	pxor	xmm1,xmm13
	movdqu	XMMWORD PTR[80+r13],xmm6
	pxor	xmm4,xmm14
	movdqu	XMMWORD PTR[96+r13],xmm1
	lea	r11,QWORD PTR[$L$ADD1]
	movdqu	XMMWORD PTR[112+r13],xmm4
	lea	r13,QWORD PTR[128+r13]
	paddd	xmm15,XMMWORD PTR[112+r11]
	jnz	$L$ctr_enc_loop

	jmp	$L$ctr_enc_done
ALIGN	16
$L$ctr_enc_loop_done::
	add	r14,8
	movdqu	xmm7,XMMWORD PTR[r12]
	pxor	xmm15,xmm7
	movdqu	XMMWORD PTR[r13],xmm15
	cmp	r14,2
	jb	$L$ctr_enc_done
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm0,xmm8
	movdqu	XMMWORD PTR[16+r13],xmm0
	je	$L$ctr_enc_done
	movdqu	xmm9,XMMWORD PTR[32+r12]
	pxor	xmm3,xmm9
	movdqu	XMMWORD PTR[32+r13],xmm3
	cmp	r14,4
	jb	$L$ctr_enc_done
	movdqu	xmm10,XMMWORD PTR[48+r12]
	pxor	xmm5,xmm10
	movdqu	XMMWORD PTR[48+r13],xmm5
	je	$L$ctr_enc_done
	movdqu	xmm11,XMMWORD PTR[64+r12]
	pxor	xmm2,xmm11
	movdqu	XMMWORD PTR[64+r13],xmm2
	cmp	r14,6
	jb	$L$ctr_enc_done
	movdqu	xmm12,XMMWORD PTR[80+r12]
	pxor	xmm6,xmm12
	movdqu	XMMWORD PTR[80+r13],xmm6
	je	$L$ctr_enc_done
	movdqu	xmm13,XMMWORD PTR[96+r12]
	pxor	xmm1,xmm13
	movdqu	XMMWORD PTR[96+r13],xmm1
	jmp	$L$ctr_enc_done

ALIGN	16
$L$ctr_enc_short::
	lea	rcx,QWORD PTR[32+rbp]
	lea	rdx,QWORD PTR[48+rbp]
	lea	r8,QWORD PTR[r15]
	call	asm_AES_encrypt
	movdqu	xmm0,XMMWORD PTR[r12]
	lea	r12,QWORD PTR[16+r12]
	mov	eax,DWORD PTR[44+rbp]
	bswap	eax
	pxor	xmm0,XMMWORD PTR[48+rbp]
	inc	eax
	movdqu	XMMWORD PTR[r13],xmm0
	bswap	eax
	lea	r13,QWORD PTR[16+r13]
	mov	DWORD PTR[44+rsp],eax
	dec	r14
	jnz	$L$ctr_enc_short

$L$ctr_enc_done::
	lea	rax,QWORD PTR[rsp]
	pxor	xmm0,xmm0
$L$ctr_enc_bzero::
	movdqa	XMMWORD PTR[rax],xmm0
	movdqa	XMMWORD PTR[16+rax],xmm0
	lea	rax,QWORD PTR[32+rax]
	cmp	rbp,rax
	ja	$L$ctr_enc_bzero

	lea	rsp,QWORD PTR[rbp]
	movaps	xmm6,XMMWORD PTR[64+rbp]
	movaps	xmm7,XMMWORD PTR[80+rbp]
	movaps	xmm8,XMMWORD PTR[96+rbp]
	movaps	xmm9,XMMWORD PTR[112+rbp]
	movaps	xmm10,XMMWORD PTR[128+rbp]
	movaps	xmm11,XMMWORD PTR[144+rbp]
	movaps	xmm12,XMMWORD PTR[160+rbp]
	movaps	xmm13,XMMWORD PTR[176+rbp]
	movaps	xmm14,XMMWORD PTR[192+rbp]
	movaps	xmm15,XMMWORD PTR[208+rbp]
	lea	rsp,QWORD PTR[160+rbp]
	mov	r15,QWORD PTR[72+rsp]
	mov	r14,QWORD PTR[80+rsp]
	mov	r13,QWORD PTR[88+rsp]
	mov	r12,QWORD PTR[96+rsp]
	mov	rbx,QWORD PTR[104+rsp]
	mov	rax,QWORD PTR[112+rsp]
	lea	rsp,QWORD PTR[120+rsp]
	mov	rbp,rax
$L$ctr_enc_epilogue::
	DB	0F3h,0C3h		;repret
bsaes_ctr32_encrypt_blocks	ENDP
PUBLIC	bsaes_xts_encrypt

ALIGN	16
bsaes_xts_encrypt	PROC PUBLIC
	mov	rax,rsp
$L$xts_enc_prologue::
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-72))+rsp]
	mov	r10,QWORD PTR[160+rsp]
	mov	r11,QWORD PTR[168+rsp]
	lea	rsp,QWORD PTR[((-160))+rsp]
	movaps	XMMWORD PTR[64+rsp],xmm6
	movaps	XMMWORD PTR[80+rsp],xmm7
	movaps	XMMWORD PTR[96+rsp],xmm8
	movaps	XMMWORD PTR[112+rsp],xmm9
	movaps	XMMWORD PTR[128+rsp],xmm10
	movaps	XMMWORD PTR[144+rsp],xmm11
	movaps	XMMWORD PTR[160+rsp],xmm12
	movaps	XMMWORD PTR[176+rsp],xmm13
	movaps	XMMWORD PTR[192+rsp],xmm14
	movaps	XMMWORD PTR[208+rsp],xmm15
$L$xts_enc_body::
	mov	rbp,rsp
	mov	r12,rcx
	mov	r13,rdx
	mov	r14,r8
	mov	r15,r9

	lea	rcx,QWORD PTR[r11]
	lea	rdx,QWORD PTR[32+rbp]
	lea	r8,QWORD PTR[r10]
	call	asm_AES_encrypt

	mov	eax,DWORD PTR[240+r15]
	mov	rbx,r14

	mov	edx,eax
	shl	rax,7
	sub	rax,96
	sub	rsp,rax

	mov	rax,rsp
	mov	rcx,r15
	mov	r10d,edx
	call	_bsaes_key_convert
	pxor	xmm7,xmm6
	movdqa	XMMWORD PTR[rax],xmm7

	and	r14,-16
	sub	rsp,080h
	movdqa	xmm6,XMMWORD PTR[32+rbp]

	pxor	xmm14,xmm14
	movdqa	xmm12,XMMWORD PTR[$L$xts_magic]
	pcmpgtd	xmm14,xmm6

	sub	r14,080h
	jc	$L$xts_enc_short
	jmp	$L$xts_enc_loop

ALIGN	16
$L$xts_enc_loop::
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm15,xmm6
	movdqa	XMMWORD PTR[rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm0,xmm6
	movdqa	XMMWORD PTR[16+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm7,XMMWORD PTR[r12]
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm1,xmm6
	movdqa	XMMWORD PTR[32+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm15,xmm7
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm2,xmm6
	movdqa	XMMWORD PTR[48+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm9,XMMWORD PTR[32+r12]
	pxor	xmm0,xmm8
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm3,xmm6
	movdqa	XMMWORD PTR[64+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm10,XMMWORD PTR[48+r12]
	pxor	xmm1,xmm9
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm4,xmm6
	movdqa	XMMWORD PTR[80+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm11,XMMWORD PTR[64+r12]
	pxor	xmm2,xmm10
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm5,xmm6
	movdqa	XMMWORD PTR[96+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm12,XMMWORD PTR[80+r12]
	pxor	xmm3,xmm11
	movdqu	xmm13,XMMWORD PTR[96+r12]
	pxor	xmm4,xmm12
	movdqu	xmm14,XMMWORD PTR[112+r12]
	lea	r12,QWORD PTR[128+r12]
	movdqa	XMMWORD PTR[112+rsp],xmm6
	pxor	xmm5,xmm13
	lea	rax,QWORD PTR[128+rsp]
	pxor	xmm6,xmm14
	mov	r10d,edx

	call	_bsaes_encrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm3,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm5,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm3
	pxor	xmm2,XMMWORD PTR[64+rsp]
	movdqu	XMMWORD PTR[48+r13],xmm5
	pxor	xmm6,XMMWORD PTR[80+rsp]
	movdqu	XMMWORD PTR[64+r13],xmm2
	pxor	xmm1,XMMWORD PTR[96+rsp]
	movdqu	XMMWORD PTR[80+r13],xmm6
	pxor	xmm4,XMMWORD PTR[112+rsp]
	movdqu	XMMWORD PTR[96+r13],xmm1
	movdqu	XMMWORD PTR[112+r13],xmm4
	lea	r13,QWORD PTR[128+r13]

	movdqa	xmm6,XMMWORD PTR[112+rsp]
	pxor	xmm14,xmm14
	movdqa	xmm12,XMMWORD PTR[$L$xts_magic]
	pcmpgtd	xmm14,xmm6
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13

	sub	r14,080h
	jnc	$L$xts_enc_loop

$L$xts_enc_short::
	add	r14,080h
	jz	$L$xts_enc_done
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm15,xmm6
	movdqa	XMMWORD PTR[rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm0,xmm6
	movdqa	XMMWORD PTR[16+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm7,XMMWORD PTR[r12]
	cmp	r14,16
	je	$L$xts_enc_1
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm1,xmm6
	movdqa	XMMWORD PTR[32+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm8,XMMWORD PTR[16+r12]
	cmp	r14,32
	je	$L$xts_enc_2
	pxor	xmm15,xmm7
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm2,xmm6
	movdqa	XMMWORD PTR[48+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm9,XMMWORD PTR[32+r12]
	cmp	r14,48
	je	$L$xts_enc_3
	pxor	xmm0,xmm8
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm3,xmm6
	movdqa	XMMWORD PTR[64+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm10,XMMWORD PTR[48+r12]
	cmp	r14,64
	je	$L$xts_enc_4
	pxor	xmm1,xmm9
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm4,xmm6
	movdqa	XMMWORD PTR[80+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm11,XMMWORD PTR[64+r12]
	cmp	r14,80
	je	$L$xts_enc_5
	pxor	xmm2,xmm10
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm5,xmm6
	movdqa	XMMWORD PTR[96+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm12,XMMWORD PTR[80+r12]
	cmp	r14,96
	je	$L$xts_enc_6
	pxor	xmm3,xmm11
	movdqu	xmm13,XMMWORD PTR[96+r12]
	pxor	xmm4,xmm12
	movdqa	XMMWORD PTR[112+rsp],xmm6
	lea	r12,QWORD PTR[112+r12]
	pxor	xmm5,xmm13
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_encrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm3,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm5,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm3
	pxor	xmm2,XMMWORD PTR[64+rsp]
	movdqu	XMMWORD PTR[48+r13],xmm5
	pxor	xmm6,XMMWORD PTR[80+rsp]
	movdqu	XMMWORD PTR[64+r13],xmm2
	pxor	xmm1,XMMWORD PTR[96+rsp]
	movdqu	XMMWORD PTR[80+r13],xmm6
	movdqu	XMMWORD PTR[96+r13],xmm1
	lea	r13,QWORD PTR[112+r13]

	movdqa	xmm6,XMMWORD PTR[112+rsp]
	jmp	$L$xts_enc_done
ALIGN	16
$L$xts_enc_6::
	pxor	xmm3,xmm11
	lea	r12,QWORD PTR[96+r12]
	pxor	xmm4,xmm12
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_encrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm3,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm5,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm3
	pxor	xmm2,XMMWORD PTR[64+rsp]
	movdqu	XMMWORD PTR[48+r13],xmm5
	pxor	xmm6,XMMWORD PTR[80+rsp]
	movdqu	XMMWORD PTR[64+r13],xmm2
	movdqu	XMMWORD PTR[80+r13],xmm6
	lea	r13,QWORD PTR[96+r13]

	movdqa	xmm6,XMMWORD PTR[96+rsp]
	jmp	$L$xts_enc_done
ALIGN	16
$L$xts_enc_5::
	pxor	xmm2,xmm10
	lea	r12,QWORD PTR[80+r12]
	pxor	xmm3,xmm11
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_encrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm3,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm5,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm3
	pxor	xmm2,XMMWORD PTR[64+rsp]
	movdqu	XMMWORD PTR[48+r13],xmm5
	movdqu	XMMWORD PTR[64+r13],xmm2
	lea	r13,QWORD PTR[80+r13]

	movdqa	xmm6,XMMWORD PTR[80+rsp]
	jmp	$L$xts_enc_done
ALIGN	16
$L$xts_enc_4::
	pxor	xmm1,xmm9
	lea	r12,QWORD PTR[64+r12]
	pxor	xmm2,xmm10
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_encrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm3,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm5,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm3
	movdqu	XMMWORD PTR[48+r13],xmm5
	lea	r13,QWORD PTR[64+r13]

	movdqa	xmm6,XMMWORD PTR[64+rsp]
	jmp	$L$xts_enc_done
ALIGN	16
$L$xts_enc_3::
	pxor	xmm0,xmm8
	lea	r12,QWORD PTR[48+r12]
	pxor	xmm1,xmm9
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_encrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm3,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	movdqu	XMMWORD PTR[32+r13],xmm3
	lea	r13,QWORD PTR[48+r13]

	movdqa	xmm6,XMMWORD PTR[48+rsp]
	jmp	$L$xts_enc_done
ALIGN	16
$L$xts_enc_2::
	pxor	xmm15,xmm7
	lea	r12,QWORD PTR[32+r12]
	pxor	xmm0,xmm8
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_encrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	movdqu	XMMWORD PTR[16+r13],xmm0
	lea	r13,QWORD PTR[32+r13]

	movdqa	xmm6,XMMWORD PTR[32+rsp]
	jmp	$L$xts_enc_done
ALIGN	16
$L$xts_enc_1::
	pxor	xmm7,xmm15
	lea	r12,QWORD PTR[16+r12]
	movdqa	XMMWORD PTR[32+rbp],xmm7
	lea	rcx,QWORD PTR[32+rbp]
	lea	rdx,QWORD PTR[32+rbp]
	lea	r8,QWORD PTR[r15]
	call	asm_AES_encrypt
	pxor	xmm15,XMMWORD PTR[32+rbp]





	movdqu	XMMWORD PTR[r13],xmm15
	lea	r13,QWORD PTR[16+r13]

	movdqa	xmm6,XMMWORD PTR[16+rsp]

$L$xts_enc_done::
	and	ebx,15
	jz	$L$xts_enc_ret
	mov	rdx,r13

$L$xts_enc_steal::
	movzx	eax,BYTE PTR[r12]
	movzx	ecx,BYTE PTR[((-16))+rdx]
	lea	r12,QWORD PTR[1+r12]
	mov	BYTE PTR[((-16))+rdx],al
	mov	BYTE PTR[rdx],cl
	lea	rdx,QWORD PTR[1+rdx]
	sub	ebx,1
	jnz	$L$xts_enc_steal

	movdqu	xmm15,XMMWORD PTR[((-16))+r13]
	lea	rcx,QWORD PTR[32+rbp]
	pxor	xmm15,xmm6
	lea	rdx,QWORD PTR[32+rbp]
	movdqa	XMMWORD PTR[32+rbp],xmm15
	lea	r8,QWORD PTR[r15]
	call	asm_AES_encrypt
	pxor	xmm6,XMMWORD PTR[32+rbp]
	movdqu	XMMWORD PTR[(-16)+r13],xmm6

$L$xts_enc_ret::
	lea	rax,QWORD PTR[rsp]
	pxor	xmm0,xmm0
$L$xts_enc_bzero::
	movdqa	XMMWORD PTR[rax],xmm0
	movdqa	XMMWORD PTR[16+rax],xmm0
	lea	rax,QWORD PTR[32+rax]
	cmp	rbp,rax
	ja	$L$xts_enc_bzero

	lea	rsp,QWORD PTR[rbp]
	movaps	xmm6,XMMWORD PTR[64+rbp]
	movaps	xmm7,XMMWORD PTR[80+rbp]
	movaps	xmm8,XMMWORD PTR[96+rbp]
	movaps	xmm9,XMMWORD PTR[112+rbp]
	movaps	xmm10,XMMWORD PTR[128+rbp]
	movaps	xmm11,XMMWORD PTR[144+rbp]
	movaps	xmm12,XMMWORD PTR[160+rbp]
	movaps	xmm13,XMMWORD PTR[176+rbp]
	movaps	xmm14,XMMWORD PTR[192+rbp]
	movaps	xmm15,XMMWORD PTR[208+rbp]
	lea	rsp,QWORD PTR[160+rbp]
	mov	r15,QWORD PTR[72+rsp]
	mov	r14,QWORD PTR[80+rsp]
	mov	r13,QWORD PTR[88+rsp]
	mov	r12,QWORD PTR[96+rsp]
	mov	rbx,QWORD PTR[104+rsp]
	mov	rax,QWORD PTR[112+rsp]
	lea	rsp,QWORD PTR[120+rsp]
	mov	rbp,rax
$L$xts_enc_epilogue::
	DB	0F3h,0C3h		;repret
bsaes_xts_encrypt	ENDP

PUBLIC	bsaes_xts_decrypt

ALIGN	16
bsaes_xts_decrypt	PROC PUBLIC
	mov	rax,rsp
$L$xts_dec_prologue::
	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-72))+rsp]
	mov	r10,QWORD PTR[160+rsp]
	mov	r11,QWORD PTR[168+rsp]
	lea	rsp,QWORD PTR[((-160))+rsp]
	movaps	XMMWORD PTR[64+rsp],xmm6
	movaps	XMMWORD PTR[80+rsp],xmm7
	movaps	XMMWORD PTR[96+rsp],xmm8
	movaps	XMMWORD PTR[112+rsp],xmm9
	movaps	XMMWORD PTR[128+rsp],xmm10
	movaps	XMMWORD PTR[144+rsp],xmm11
	movaps	XMMWORD PTR[160+rsp],xmm12
	movaps	XMMWORD PTR[176+rsp],xmm13
	movaps	XMMWORD PTR[192+rsp],xmm14
	movaps	XMMWORD PTR[208+rsp],xmm15
$L$xts_dec_body::
	mov	rbp,rsp
	mov	r12,rcx
	mov	r13,rdx
	mov	r14,r8
	mov	r15,r9

	lea	rcx,QWORD PTR[r11]
	lea	rdx,QWORD PTR[32+rbp]
	lea	r8,QWORD PTR[r10]
	call	asm_AES_encrypt

	mov	eax,DWORD PTR[240+r15]
	mov	rbx,r14

	mov	edx,eax
	shl	rax,7
	sub	rax,96
	sub	rsp,rax

	mov	rax,rsp
	mov	rcx,r15
	mov	r10d,edx
	call	_bsaes_key_convert
	pxor	xmm7,XMMWORD PTR[rsp]
	movdqa	XMMWORD PTR[rax],xmm6
	movdqa	XMMWORD PTR[rsp],xmm7

	xor	eax,eax
	and	r14,-16
	test	ebx,15
	setnz	al
	shl	rax,4
	sub	r14,rax

	sub	rsp,080h
	movdqa	xmm6,XMMWORD PTR[32+rbp]

	pxor	xmm14,xmm14
	movdqa	xmm12,XMMWORD PTR[$L$xts_magic]
	pcmpgtd	xmm14,xmm6

	sub	r14,080h
	jc	$L$xts_dec_short
	jmp	$L$xts_dec_loop

ALIGN	16
$L$xts_dec_loop::
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm15,xmm6
	movdqa	XMMWORD PTR[rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm0,xmm6
	movdqa	XMMWORD PTR[16+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm7,XMMWORD PTR[r12]
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm1,xmm6
	movdqa	XMMWORD PTR[32+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm8,XMMWORD PTR[16+r12]
	pxor	xmm15,xmm7
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm2,xmm6
	movdqa	XMMWORD PTR[48+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm9,XMMWORD PTR[32+r12]
	pxor	xmm0,xmm8
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm3,xmm6
	movdqa	XMMWORD PTR[64+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm10,XMMWORD PTR[48+r12]
	pxor	xmm1,xmm9
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm4,xmm6
	movdqa	XMMWORD PTR[80+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm11,XMMWORD PTR[64+r12]
	pxor	xmm2,xmm10
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm5,xmm6
	movdqa	XMMWORD PTR[96+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm12,XMMWORD PTR[80+r12]
	pxor	xmm3,xmm11
	movdqu	xmm13,XMMWORD PTR[96+r12]
	pxor	xmm4,xmm12
	movdqu	xmm14,XMMWORD PTR[112+r12]
	lea	r12,QWORD PTR[128+r12]
	movdqa	XMMWORD PTR[112+rsp],xmm6
	pxor	xmm5,xmm13
	lea	rax,QWORD PTR[128+rsp]
	pxor	xmm6,xmm14
	mov	r10d,edx

	call	_bsaes_decrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm5,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm3,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm5
	pxor	xmm1,XMMWORD PTR[64+rsp]
	movdqu	XMMWORD PTR[48+r13],xmm3
	pxor	xmm6,XMMWORD PTR[80+rsp]
	movdqu	XMMWORD PTR[64+r13],xmm1
	pxor	xmm2,XMMWORD PTR[96+rsp]
	movdqu	XMMWORD PTR[80+r13],xmm6
	pxor	xmm4,XMMWORD PTR[112+rsp]
	movdqu	XMMWORD PTR[96+r13],xmm2
	movdqu	XMMWORD PTR[112+r13],xmm4
	lea	r13,QWORD PTR[128+r13]

	movdqa	xmm6,XMMWORD PTR[112+rsp]
	pxor	xmm14,xmm14
	movdqa	xmm12,XMMWORD PTR[$L$xts_magic]
	pcmpgtd	xmm14,xmm6
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13

	sub	r14,080h
	jnc	$L$xts_dec_loop

$L$xts_dec_short::
	add	r14,080h
	jz	$L$xts_dec_done
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm15,xmm6
	movdqa	XMMWORD PTR[rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm0,xmm6
	movdqa	XMMWORD PTR[16+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm7,XMMWORD PTR[r12]
	cmp	r14,16
	je	$L$xts_dec_1
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm1,xmm6
	movdqa	XMMWORD PTR[32+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm8,XMMWORD PTR[16+r12]
	cmp	r14,32
	je	$L$xts_dec_2
	pxor	xmm15,xmm7
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm2,xmm6
	movdqa	XMMWORD PTR[48+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm9,XMMWORD PTR[32+r12]
	cmp	r14,48
	je	$L$xts_dec_3
	pxor	xmm0,xmm8
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm3,xmm6
	movdqa	XMMWORD PTR[64+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm10,XMMWORD PTR[48+r12]
	cmp	r14,64
	je	$L$xts_dec_4
	pxor	xmm1,xmm9
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm4,xmm6
	movdqa	XMMWORD PTR[80+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm11,XMMWORD PTR[64+r12]
	cmp	r14,80
	je	$L$xts_dec_5
	pxor	xmm2,xmm10
	pshufd	xmm13,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm5,xmm6
	movdqa	XMMWORD PTR[96+rsp],xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	pcmpgtd	xmm14,xmm6
	pxor	xmm6,xmm13
	movdqu	xmm12,XMMWORD PTR[80+r12]
	cmp	r14,96
	je	$L$xts_dec_6
	pxor	xmm3,xmm11
	movdqu	xmm13,XMMWORD PTR[96+r12]
	pxor	xmm4,xmm12
	movdqa	XMMWORD PTR[112+rsp],xmm6
	lea	r12,QWORD PTR[112+r12]
	pxor	xmm5,xmm13
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_decrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm5,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm3,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm5
	pxor	xmm1,XMMWORD PTR[64+rsp]
	movdqu	XMMWORD PTR[48+r13],xmm3
	pxor	xmm6,XMMWORD PTR[80+rsp]
	movdqu	XMMWORD PTR[64+r13],xmm1
	pxor	xmm2,XMMWORD PTR[96+rsp]
	movdqu	XMMWORD PTR[80+r13],xmm6
	movdqu	XMMWORD PTR[96+r13],xmm2
	lea	r13,QWORD PTR[112+r13]

	movdqa	xmm6,XMMWORD PTR[112+rsp]
	jmp	$L$xts_dec_done
ALIGN	16
$L$xts_dec_6::
	pxor	xmm3,xmm11
	lea	r12,QWORD PTR[96+r12]
	pxor	xmm4,xmm12
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_decrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm5,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm3,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm5
	pxor	xmm1,XMMWORD PTR[64+rsp]
	movdqu	XMMWORD PTR[48+r13],xmm3
	pxor	xmm6,XMMWORD PTR[80+rsp]
	movdqu	XMMWORD PTR[64+r13],xmm1
	movdqu	XMMWORD PTR[80+r13],xmm6
	lea	r13,QWORD PTR[96+r13]

	movdqa	xmm6,XMMWORD PTR[96+rsp]
	jmp	$L$xts_dec_done
ALIGN	16
$L$xts_dec_5::
	pxor	xmm2,xmm10
	lea	r12,QWORD PTR[80+r12]
	pxor	xmm3,xmm11
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_decrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm5,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm3,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm5
	pxor	xmm1,XMMWORD PTR[64+rsp]
	movdqu	XMMWORD PTR[48+r13],xmm3
	movdqu	XMMWORD PTR[64+r13],xmm1
	lea	r13,QWORD PTR[80+r13]

	movdqa	xmm6,XMMWORD PTR[80+rsp]
	jmp	$L$xts_dec_done
ALIGN	16
$L$xts_dec_4::
	pxor	xmm1,xmm9
	lea	r12,QWORD PTR[64+r12]
	pxor	xmm2,xmm10
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_decrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm5,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	pxor	xmm3,XMMWORD PTR[48+rsp]
	movdqu	XMMWORD PTR[32+r13],xmm5
	movdqu	XMMWORD PTR[48+r13],xmm3
	lea	r13,QWORD PTR[64+r13]

	movdqa	xmm6,XMMWORD PTR[64+rsp]
	jmp	$L$xts_dec_done
ALIGN	16
$L$xts_dec_3::
	pxor	xmm0,xmm8
	lea	r12,QWORD PTR[48+r12]
	pxor	xmm1,xmm9
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_decrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	pxor	xmm5,XMMWORD PTR[32+rsp]
	movdqu	XMMWORD PTR[16+r13],xmm0
	movdqu	XMMWORD PTR[32+r13],xmm5
	lea	r13,QWORD PTR[48+r13]

	movdqa	xmm6,XMMWORD PTR[48+rsp]
	jmp	$L$xts_dec_done
ALIGN	16
$L$xts_dec_2::
	pxor	xmm15,xmm7
	lea	r12,QWORD PTR[32+r12]
	pxor	xmm0,xmm8
	lea	rax,QWORD PTR[128+rsp]
	mov	r10d,edx

	call	_bsaes_decrypt8

	pxor	xmm15,XMMWORD PTR[rsp]
	pxor	xmm0,XMMWORD PTR[16+rsp]
	movdqu	XMMWORD PTR[r13],xmm15
	movdqu	XMMWORD PTR[16+r13],xmm0
	lea	r13,QWORD PTR[32+r13]

	movdqa	xmm6,XMMWORD PTR[32+rsp]
	jmp	$L$xts_dec_done
ALIGN	16
$L$xts_dec_1::
	pxor	xmm7,xmm15
	lea	r12,QWORD PTR[16+r12]
	movdqa	XMMWORD PTR[32+rbp],xmm7
	lea	rcx,QWORD PTR[32+rbp]
	lea	rdx,QWORD PTR[32+rbp]
	lea	r8,QWORD PTR[r15]
	call	asm_AES_decrypt
	pxor	xmm15,XMMWORD PTR[32+rbp]





	movdqu	XMMWORD PTR[r13],xmm15
	lea	r13,QWORD PTR[16+r13]

	movdqa	xmm6,XMMWORD PTR[16+rsp]

$L$xts_dec_done::
	and	ebx,15
	jz	$L$xts_dec_ret

	pxor	xmm14,xmm14
	movdqa	xmm12,XMMWORD PTR[$L$xts_magic]
	pcmpgtd	xmm14,xmm6
	pshufd	xmm13,xmm14,013h
	movdqa	xmm5,xmm6
	paddq	xmm6,xmm6
	pand	xmm13,xmm12
	movdqu	xmm15,XMMWORD PTR[r12]
	pxor	xmm6,xmm13

	lea	rcx,QWORD PTR[32+rbp]
	pxor	xmm15,xmm6
	lea	rdx,QWORD PTR[32+rbp]
	movdqa	XMMWORD PTR[32+rbp],xmm15
	lea	r8,QWORD PTR[r15]
	call	asm_AES_decrypt
	pxor	xmm6,XMMWORD PTR[32+rbp]
	mov	rdx,r13
	movdqu	XMMWORD PTR[r13],xmm6

$L$xts_dec_steal::
	movzx	eax,BYTE PTR[16+r12]
	movzx	ecx,BYTE PTR[rdx]
	lea	r12,QWORD PTR[1+r12]
	mov	BYTE PTR[rdx],al
	mov	BYTE PTR[16+rdx],cl
	lea	rdx,QWORD PTR[1+rdx]
	sub	ebx,1
	jnz	$L$xts_dec_steal

	movdqu	xmm15,XMMWORD PTR[r13]
	lea	rcx,QWORD PTR[32+rbp]
	pxor	xmm15,xmm5
	lea	rdx,QWORD PTR[32+rbp]
	movdqa	XMMWORD PTR[32+rbp],xmm15
	lea	r8,QWORD PTR[r15]
	call	asm_AES_decrypt
	pxor	xmm5,XMMWORD PTR[32+rbp]
	movdqu	XMMWORD PTR[r13],xmm5

$L$xts_dec_ret::
	lea	rax,QWORD PTR[rsp]
	pxor	xmm0,xmm0
$L$xts_dec_bzero::
	movdqa	XMMWORD PTR[rax],xmm0
	movdqa	XMMWORD PTR[16+rax],xmm0
	lea	rax,QWORD PTR[32+rax]
	cmp	rbp,rax
	ja	$L$xts_dec_bzero

	lea	rsp,QWORD PTR[rbp]
	movaps	xmm6,XMMWORD PTR[64+rbp]
	movaps	xmm7,XMMWORD PTR[80+rbp]
	movaps	xmm8,XMMWORD PTR[96+rbp]
	movaps	xmm9,XMMWORD PTR[112+rbp]
	movaps	xmm10,XMMWORD PTR[128+rbp]
	movaps	xmm11,XMMWORD PTR[144+rbp]
	movaps	xmm12,XMMWORD PTR[160+rbp]
	movaps	xmm13,XMMWORD PTR[176+rbp]
	movaps	xmm14,XMMWORD PTR[192+rbp]
	movaps	xmm15,XMMWORD PTR[208+rbp]
	lea	rsp,QWORD PTR[160+rbp]
	mov	r15,QWORD PTR[72+rsp]
	mov	r14,QWORD PTR[80+rsp]
	mov	r13,QWORD PTR[88+rsp]
	mov	r12,QWORD PTR[96+rsp]
	mov	rbx,QWORD PTR[104+rsp]
	mov	rax,QWORD PTR[112+rsp]
	lea	rsp,QWORD PTR[120+rsp]
	mov	rbp,rax
$L$xts_dec_epilogue::
	DB	0F3h,0C3h		;repret
bsaes_xts_decrypt	ENDP

ALIGN	64
_bsaes_const::
$L$M0ISR::
	DQ	00a0e0206070b0f03h,00004080c0d010509h
$L$ISRM0::
	DQ	001040b0e0205080fh,00306090c00070a0dh
$L$ISR::
	DQ	00504070602010003h,00f0e0d0c080b0a09h
$L$BS0::
	DQ	05555555555555555h,05555555555555555h
$L$BS1::
	DQ	03333333333333333h,03333333333333333h
$L$BS2::
	DQ	00f0f0f0f0f0f0f0fh,00f0f0f0f0f0f0f0fh
$L$SR::
	DQ	00504070600030201h,00f0e0d0c0a09080bh
$L$SRM0::
	DQ	00304090e00050a0fh,001060b0c0207080dh
$L$M0SR::
	DQ	00a0e02060f03070bh,00004080c05090d01h
$L$SWPUP::
	DQ	00706050403020100h,00c0d0e0f0b0a0908h
$L$SWPUPM0SR::
	DQ	00a0d02060c03070bh,00004080f05090e01h
$L$ADD1::
	DQ	00000000000000000h,00000000100000000h
$L$ADD2::
	DQ	00000000000000000h,00000000200000000h
$L$ADD3::
	DQ	00000000000000000h,00000000300000000h
$L$ADD4::
	DQ	00000000000000000h,00000000400000000h
$L$ADD5::
	DQ	00000000000000000h,00000000500000000h
$L$ADD6::
	DQ	00000000000000000h,00000000600000000h
$L$ADD7::
	DQ	00000000000000000h,00000000700000000h
$L$ADD8::
	DQ	00000000000000000h,00000000800000000h
$L$xts_magic::
	DD	087h,0,1,0
$L$masks::
	DQ	00101010101010101h,00101010101010101h
	DQ	00202020202020202h,00202020202020202h
	DQ	00404040404040404h,00404040404040404h
	DQ	00808080808080808h,00808080808080808h
$L$M0::
	DQ	002060a0e03070b0fh,00004080c0105090dh
$L$63::
	DQ	06363636363636363h,06363636363636363h
DB	66,105,116,45,115,108,105,99,101,100,32,65,69,83,32,102
DB	111,114,32,120,56,54,95,54,52,47,83,83,83,69,51,44
DB	32,69,109,105,108,105,97,32,75,195,164,115,112,101,114,44
DB	32,80,101,116,101,114,32,83,99,104,119,97,98,101,44,32
DB	65,110,100,121,32,80,111,108,121,97,107,111,118,0
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

	mov	rax,QWORD PTR[160+r8]

	lea	rsi,QWORD PTR[64+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch
	lea	rax,QWORD PTR[160+rax]

	mov	rbp,QWORD PTR[112+rax]
	mov	rbx,QWORD PTR[104+rax]
	mov	r12,QWORD PTR[96+rax]
	mov	r13,QWORD PTR[88+rax]
	mov	r14,QWORD PTR[80+rax]
	mov	r15,QWORD PTR[72+rax]
	lea	rax,QWORD PTR[120+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[232+r8],r14
	mov	QWORD PTR[240+r8],r15

$L$in_prologue::
	mov	QWORD PTR[152+r8],rax

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
	DD	imagerel $L$cbc_dec_prologue
	DD	imagerel $L$cbc_dec_epilogue
	DD	imagerel $L$cbc_dec_info

	DD	imagerel $L$ctr_enc_prologue
	DD	imagerel $L$ctr_enc_epilogue
	DD	imagerel $L$ctr_enc_info

	DD	imagerel $L$xts_enc_prologue
	DD	imagerel $L$xts_enc_epilogue
	DD	imagerel $L$xts_enc_info

	DD	imagerel $L$xts_dec_prologue
	DD	imagerel $L$xts_dec_epilogue
	DD	imagerel $L$xts_dec_info

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$cbc_dec_info::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$cbc_dec_body,imagerel $L$cbc_dec_epilogue
$L$ctr_enc_info::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$ctr_enc_body,imagerel $L$ctr_enc_epilogue
$L$xts_enc_info::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$xts_enc_body,imagerel $L$xts_enc_epilogue
$L$xts_dec_info::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$xts_dec_body,imagerel $L$xts_dec_epilogue

.xdata	ENDS
END
