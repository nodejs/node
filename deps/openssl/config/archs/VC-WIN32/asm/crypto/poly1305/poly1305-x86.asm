%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
;extern	_OPENSSL_ia32cap_P
align	64
global	_poly1305_init
align	16
_poly1305_init:
L$_poly1305_init_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	ebp,DWORD [28+esp]
	xor	eax,eax
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],eax
	mov	DWORD [8+edi],eax
	mov	DWORD [12+edi],eax
	mov	DWORD [16+edi],eax
	mov	DWORD [20+edi],eax
	cmp	esi,0
	je	NEAR L$000nokey
	call	L$001pic_point
L$001pic_point:
	pop	ebx
	lea	eax,[(_poly1305_blocks-L$001pic_point)+ebx]
	lea	edx,[(_poly1305_emit-L$001pic_point)+ebx]
	lea	edi,[_OPENSSL_ia32cap_P]
	mov	ecx,DWORD [edi]
	and	ecx,83886080
	cmp	ecx,83886080
	jne	NEAR L$002no_sse2
	lea	eax,[(__poly1305_blocks_sse2-L$001pic_point)+ebx]
	lea	edx,[(__poly1305_emit_sse2-L$001pic_point)+ebx]
	mov	ecx,DWORD [8+edi]
	test	ecx,32
	jz	NEAR L$002no_sse2
	lea	eax,[(__poly1305_blocks_avx2-L$001pic_point)+ebx]
L$002no_sse2:
	mov	edi,DWORD [20+esp]
	mov	DWORD [ebp],eax
	mov	DWORD [4+ebp],edx
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	mov	edx,DWORD [12+esi]
	and	eax,268435455
	and	ebx,268435452
	and	ecx,268435452
	and	edx,268435452
	mov	DWORD [24+edi],eax
	mov	DWORD [28+edi],ebx
	mov	DWORD [32+edi],ecx
	mov	DWORD [36+edi],edx
	mov	eax,1
L$000nokey:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_poly1305_blocks
align	16
_poly1305_blocks:
L$_poly1305_blocks_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	ecx,DWORD [28+esp]
L$enter_blocks:
	and	ecx,-15
	jz	NEAR L$003nodata
	sub	esp,64
	mov	eax,DWORD [24+edi]
	mov	ebx,DWORD [28+edi]
	lea	ebp,[ecx*1+esi]
	mov	ecx,DWORD [32+edi]
	mov	edx,DWORD [36+edi]
	mov	DWORD [92+esp],ebp
	mov	ebp,esi
	mov	DWORD [36+esp],eax
	mov	eax,ebx
	shr	eax,2
	mov	DWORD [40+esp],ebx
	add	eax,ebx
	mov	ebx,ecx
	shr	ebx,2
	mov	DWORD [44+esp],ecx
	add	ebx,ecx
	mov	ecx,edx
	shr	ecx,2
	mov	DWORD [48+esp],edx
	add	ecx,edx
	mov	DWORD [52+esp],eax
	mov	DWORD [56+esp],ebx
	mov	DWORD [60+esp],ecx
	mov	eax,DWORD [edi]
	mov	ebx,DWORD [4+edi]
	mov	ecx,DWORD [8+edi]
	mov	esi,DWORD [12+edi]
	mov	edi,DWORD [16+edi]
	jmp	NEAR L$004loop
align	32
L$004loop:
	add	eax,DWORD [ebp]
	adc	ebx,DWORD [4+ebp]
	adc	ecx,DWORD [8+ebp]
	adc	esi,DWORD [12+ebp]
	lea	ebp,[16+ebp]
	adc	edi,DWORD [96+esp]
	mov	DWORD [esp],eax
	mov	DWORD [12+esp],esi
	mul	DWORD [36+esp]
	mov	DWORD [16+esp],edi
	mov	edi,eax
	mov	eax,ebx
	mov	esi,edx
	mul	DWORD [60+esp]
	add	edi,eax
	mov	eax,ecx
	adc	esi,edx
	mul	DWORD [56+esp]
	add	edi,eax
	mov	eax,DWORD [12+esp]
	adc	esi,edx
	mul	DWORD [52+esp]
	add	edi,eax
	mov	eax,DWORD [esp]
	adc	esi,edx
	mul	DWORD [40+esp]
	mov	DWORD [20+esp],edi
	xor	edi,edi
	add	esi,eax
	mov	eax,ebx
	adc	edi,edx
	mul	DWORD [36+esp]
	add	esi,eax
	mov	eax,ecx
	adc	edi,edx
	mul	DWORD [60+esp]
	add	esi,eax
	mov	eax,DWORD [12+esp]
	adc	edi,edx
	mul	DWORD [56+esp]
	add	esi,eax
	mov	eax,DWORD [16+esp]
	adc	edi,edx
	imul	eax,DWORD [52+esp]
	add	esi,eax
	mov	eax,DWORD [esp]
	adc	edi,0
	mul	DWORD [44+esp]
	mov	DWORD [24+esp],esi
	xor	esi,esi
	add	edi,eax
	mov	eax,ebx
	adc	esi,edx
	mul	DWORD [40+esp]
	add	edi,eax
	mov	eax,ecx
	adc	esi,edx
	mul	DWORD [36+esp]
	add	edi,eax
	mov	eax,DWORD [12+esp]
	adc	esi,edx
	mul	DWORD [60+esp]
	add	edi,eax
	mov	eax,DWORD [16+esp]
	adc	esi,edx
	imul	eax,DWORD [56+esp]
	add	edi,eax
	mov	eax,DWORD [esp]
	adc	esi,0
	mul	DWORD [48+esp]
	mov	DWORD [28+esp],edi
	xor	edi,edi
	add	esi,eax
	mov	eax,ebx
	adc	edi,edx
	mul	DWORD [44+esp]
	add	esi,eax
	mov	eax,ecx
	adc	edi,edx
	mul	DWORD [40+esp]
	add	esi,eax
	mov	eax,DWORD [12+esp]
	adc	edi,edx
	mul	DWORD [36+esp]
	add	esi,eax
	mov	ecx,DWORD [16+esp]
	adc	edi,edx
	mov	edx,ecx
	imul	ecx,DWORD [60+esp]
	add	esi,ecx
	mov	eax,DWORD [20+esp]
	adc	edi,0
	imul	edx,DWORD [36+esp]
	add	edx,edi
	mov	ebx,DWORD [24+esp]
	mov	ecx,DWORD [28+esp]
	mov	edi,edx
	shr	edx,2
	and	edi,3
	lea	edx,[edx*4+edx]
	add	eax,edx
	adc	ebx,0
	adc	ecx,0
	adc	esi,0
	adc	edi,0
	cmp	ebp,DWORD [92+esp]
	jne	NEAR L$004loop
	mov	edx,DWORD [84+esp]
	add	esp,64
	mov	DWORD [edx],eax
	mov	DWORD [4+edx],ebx
	mov	DWORD [8+edx],ecx
	mov	DWORD [12+edx],esi
	mov	DWORD [16+edx],edi
L$003nodata:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_poly1305_emit
align	16
_poly1305_emit:
L$_poly1305_emit_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,DWORD [20+esp]
L$enter_emit:
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [ebp]
	mov	ebx,DWORD [4+ebp]
	mov	ecx,DWORD [8+ebp]
	mov	edx,DWORD [12+ebp]
	mov	esi,DWORD [16+ebp]
	add	eax,5
	adc	ebx,0
	adc	ecx,0
	adc	edx,0
	adc	esi,0
	shr	esi,2
	neg	esi
	and	eax,esi
	and	ebx,esi
	and	ecx,esi
	and	edx,esi
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	not	esi
	mov	eax,DWORD [ebp]
	mov	ebx,DWORD [4+ebp]
	mov	ecx,DWORD [8+ebp]
	mov	edx,DWORD [12+ebp]
	mov	ebp,DWORD [28+esp]
	and	eax,esi
	and	ebx,esi
	and	ecx,esi
	and	edx,esi
	or	eax,DWORD [edi]
	or	ebx,DWORD [4+edi]
	or	ecx,DWORD [8+edi]
	or	edx,DWORD [12+edi]
	add	eax,DWORD [ebp]
	adc	ebx,DWORD [4+ebp]
	adc	ecx,DWORD [8+ebp]
	adc	edx,DWORD [12+ebp]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	32
align	16
__poly1305_init_sse2:
	movdqu	xmm4,[24+edi]
	lea	edi,[48+edi]
	mov	ebp,esp
	sub	esp,224
	and	esp,-16
	movq	xmm7,[64+ebx]
	movdqa	xmm0,xmm4
	movdqa	xmm1,xmm4
	movdqa	xmm2,xmm4
	pand	xmm0,xmm7
	psrlq	xmm1,26
	psrldq	xmm2,6
	pand	xmm1,xmm7
	movdqa	xmm3,xmm2
	psrlq	xmm2,4
	psrlq	xmm3,30
	pand	xmm2,xmm7
	pand	xmm3,xmm7
	psrldq	xmm4,13
	lea	edx,[144+esp]
	mov	ecx,2
L$005square:
	movdqa	[esp],xmm0
	movdqa	[16+esp],xmm1
	movdqa	[32+esp],xmm2
	movdqa	[48+esp],xmm3
	movdqa	[64+esp],xmm4
	movdqa	xmm6,xmm1
	movdqa	xmm5,xmm2
	pslld	xmm6,2
	pslld	xmm5,2
	paddd	xmm6,xmm1
	paddd	xmm5,xmm2
	movdqa	[80+esp],xmm6
	movdqa	[96+esp],xmm5
	movdqa	xmm6,xmm3
	movdqa	xmm5,xmm4
	pslld	xmm6,2
	pslld	xmm5,2
	paddd	xmm6,xmm3
	paddd	xmm5,xmm4
	movdqa	[112+esp],xmm6
	movdqa	[128+esp],xmm5
	pshufd	xmm6,xmm0,68
	movdqa	xmm5,xmm1
	pshufd	xmm1,xmm1,68
	pshufd	xmm2,xmm2,68
	pshufd	xmm3,xmm3,68
	pshufd	xmm4,xmm4,68
	movdqa	[edx],xmm6
	movdqa	[16+edx],xmm1
	movdqa	[32+edx],xmm2
	movdqa	[48+edx],xmm3
	movdqa	[64+edx],xmm4
	pmuludq	xmm4,xmm0
	pmuludq	xmm3,xmm0
	pmuludq	xmm2,xmm0
	pmuludq	xmm1,xmm0
	pmuludq	xmm0,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[48+edx]
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[32+edx]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+edx]
	paddq	xmm3,xmm6
	movdqa	xmm6,[80+esp]
	pmuludq	xmm5,[edx]
	paddq	xmm2,xmm7
	pmuludq	xmm6,[64+edx]
	movdqa	xmm7,[32+esp]
	paddq	xmm1,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[32+edx]
	paddq	xmm0,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[16+edx]
	paddq	xmm4,xmm7
	movdqa	xmm7,[96+esp]
	pmuludq	xmm6,[edx]
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[64+edx]
	paddq	xmm2,xmm6
	pmuludq	xmm5,[48+edx]
	movdqa	xmm6,[48+esp]
	paddq	xmm1,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[16+edx]
	paddq	xmm0,xmm5
	movdqa	xmm5,[112+esp]
	pmuludq	xmm7,[edx]
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[64+edx]
	paddq	xmm3,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[48+edx]
	paddq	xmm2,xmm5
	pmuludq	xmm7,[32+edx]
	movdqa	xmm5,[64+esp]
	paddq	xmm1,xmm6
	movdqa	xmm6,[128+esp]
	pmuludq	xmm5,[edx]
	paddq	xmm0,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[64+edx]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+edx]
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[32+edx]
	paddq	xmm0,xmm7
	pmuludq	xmm6,[48+edx]
	movdqa	xmm7,[64+ebx]
	paddq	xmm1,xmm5
	paddq	xmm2,xmm6
	movdqa	xmm5,xmm3
	pand	xmm3,xmm7
	psrlq	xmm5,26
	paddq	xmm5,xmm4
	movdqa	xmm6,xmm0
	pand	xmm0,xmm7
	psrlq	xmm6,26
	movdqa	xmm4,xmm5
	paddq	xmm6,xmm1
	psrlq	xmm5,26
	pand	xmm4,xmm7
	movdqa	xmm1,xmm6
	psrlq	xmm6,26
	paddd	xmm0,xmm5
	psllq	xmm5,2
	paddq	xmm6,xmm2
	paddq	xmm5,xmm0
	pand	xmm1,xmm7
	movdqa	xmm2,xmm6
	psrlq	xmm6,26
	pand	xmm2,xmm7
	paddd	xmm6,xmm3
	movdqa	xmm0,xmm5
	psrlq	xmm5,26
	movdqa	xmm3,xmm6
	psrlq	xmm6,26
	pand	xmm0,xmm7
	paddd	xmm1,xmm5
	pand	xmm3,xmm7
	paddd	xmm4,xmm6
	dec	ecx
	jz	NEAR L$006square_break
	punpcklqdq	xmm0,[esp]
	punpcklqdq	xmm1,[16+esp]
	punpcklqdq	xmm2,[32+esp]
	punpcklqdq	xmm3,[48+esp]
	punpcklqdq	xmm4,[64+esp]
	jmp	NEAR L$005square
L$006square_break:
	psllq	xmm0,32
	psllq	xmm1,32
	psllq	xmm2,32
	psllq	xmm3,32
	psllq	xmm4,32
	por	xmm0,[esp]
	por	xmm1,[16+esp]
	por	xmm2,[32+esp]
	por	xmm3,[48+esp]
	por	xmm4,[64+esp]
	pshufd	xmm0,xmm0,141
	pshufd	xmm1,xmm1,141
	pshufd	xmm2,xmm2,141
	pshufd	xmm3,xmm3,141
	pshufd	xmm4,xmm4,141
	movdqu	[edi],xmm0
	movdqu	[16+edi],xmm1
	movdqu	[32+edi],xmm2
	movdqu	[48+edi],xmm3
	movdqu	[64+edi],xmm4
	movdqa	xmm6,xmm1
	movdqa	xmm5,xmm2
	pslld	xmm6,2
	pslld	xmm5,2
	paddd	xmm6,xmm1
	paddd	xmm5,xmm2
	movdqu	[80+edi],xmm6
	movdqu	[96+edi],xmm5
	movdqa	xmm6,xmm3
	movdqa	xmm5,xmm4
	pslld	xmm6,2
	pslld	xmm5,2
	paddd	xmm6,xmm3
	paddd	xmm5,xmm4
	movdqu	[112+edi],xmm6
	movdqu	[128+edi],xmm5
	mov	esp,ebp
	lea	edi,[edi-48]
	ret
align	32
align	16
__poly1305_blocks_sse2:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	ecx,DWORD [28+esp]
	mov	eax,DWORD [20+edi]
	and	ecx,-16
	jz	NEAR L$007nodata
	cmp	ecx,64
	jae	NEAR L$008enter_sse2
	test	eax,eax
	jz	NEAR L$enter_blocks
align	16
L$008enter_sse2:
	call	L$009pic_point
L$009pic_point:
	pop	ebx
	lea	ebx,[(L$const_sse2-L$009pic_point)+ebx]
	test	eax,eax
	jnz	NEAR L$010base2_26
	call	__poly1305_init_sse2
	mov	eax,DWORD [edi]
	mov	ecx,DWORD [3+edi]
	mov	edx,DWORD [6+edi]
	mov	esi,DWORD [9+edi]
	mov	ebp,DWORD [13+edi]
	mov	DWORD [20+edi],1
	shr	ecx,2
	and	eax,67108863
	shr	edx,4
	and	ecx,67108863
	shr	esi,6
	and	edx,67108863
	movd	xmm0,eax
	movd	xmm1,ecx
	movd	xmm2,edx
	movd	xmm3,esi
	movd	xmm4,ebp
	mov	esi,DWORD [24+esp]
	mov	ecx,DWORD [28+esp]
	jmp	NEAR L$011base2_32
align	16
L$010base2_26:
	movd	xmm0,DWORD [edi]
	movd	xmm1,DWORD [4+edi]
	movd	xmm2,DWORD [8+edi]
	movd	xmm3,DWORD [12+edi]
	movd	xmm4,DWORD [16+edi]
	movdqa	xmm7,[64+ebx]
L$011base2_32:
	mov	eax,DWORD [32+esp]
	mov	ebp,esp
	sub	esp,528
	and	esp,-16
	lea	edi,[48+edi]
	shl	eax,24
	test	ecx,31
	jz	NEAR L$012even
	movdqu	xmm6,[esi]
	lea	esi,[16+esi]
	movdqa	xmm5,xmm6
	pand	xmm6,xmm7
	paddd	xmm0,xmm6
	movdqa	xmm6,xmm5
	psrlq	xmm5,26
	psrldq	xmm6,6
	pand	xmm5,xmm7
	paddd	xmm1,xmm5
	movdqa	xmm5,xmm6
	psrlq	xmm6,4
	pand	xmm6,xmm7
	paddd	xmm2,xmm6
	movdqa	xmm6,xmm5
	psrlq	xmm5,30
	pand	xmm5,xmm7
	psrldq	xmm6,7
	paddd	xmm3,xmm5
	movd	xmm5,eax
	paddd	xmm4,xmm6
	movd	xmm6,DWORD [12+edi]
	paddd	xmm4,xmm5
	movdqa	[esp],xmm0
	movdqa	[16+esp],xmm1
	movdqa	[32+esp],xmm2
	movdqa	[48+esp],xmm3
	movdqa	[64+esp],xmm4
	pmuludq	xmm0,xmm6
	pmuludq	xmm1,xmm6
	pmuludq	xmm2,xmm6
	movd	xmm5,DWORD [28+edi]
	pmuludq	xmm3,xmm6
	pmuludq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[48+esp]
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[32+esp]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+esp]
	paddq	xmm3,xmm6
	movd	xmm6,DWORD [92+edi]
	pmuludq	xmm5,[esp]
	paddq	xmm2,xmm7
	pmuludq	xmm6,[64+esp]
	movd	xmm7,DWORD [44+edi]
	paddq	xmm1,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[32+esp]
	paddq	xmm0,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[16+esp]
	paddq	xmm4,xmm7
	movd	xmm7,DWORD [108+edi]
	pmuludq	xmm6,[esp]
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[64+esp]
	paddq	xmm2,xmm6
	pmuludq	xmm5,[48+esp]
	movd	xmm6,DWORD [60+edi]
	paddq	xmm1,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[16+esp]
	paddq	xmm0,xmm5
	movd	xmm5,DWORD [124+edi]
	pmuludq	xmm7,[esp]
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[64+esp]
	paddq	xmm3,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[48+esp]
	paddq	xmm2,xmm5
	pmuludq	xmm7,[32+esp]
	movd	xmm5,DWORD [76+edi]
	paddq	xmm1,xmm6
	movd	xmm6,DWORD [140+edi]
	pmuludq	xmm5,[esp]
	paddq	xmm0,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[64+esp]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+esp]
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[32+esp]
	paddq	xmm0,xmm7
	pmuludq	xmm6,[48+esp]
	movdqa	xmm7,[64+ebx]
	paddq	xmm1,xmm5
	paddq	xmm2,xmm6
	movdqa	xmm5,xmm3
	pand	xmm3,xmm7
	psrlq	xmm5,26
	paddq	xmm5,xmm4
	movdqa	xmm6,xmm0
	pand	xmm0,xmm7
	psrlq	xmm6,26
	movdqa	xmm4,xmm5
	paddq	xmm6,xmm1
	psrlq	xmm5,26
	pand	xmm4,xmm7
	movdqa	xmm1,xmm6
	psrlq	xmm6,26
	paddd	xmm0,xmm5
	psllq	xmm5,2
	paddq	xmm6,xmm2
	paddq	xmm5,xmm0
	pand	xmm1,xmm7
	movdqa	xmm2,xmm6
	psrlq	xmm6,26
	pand	xmm2,xmm7
	paddd	xmm6,xmm3
	movdqa	xmm0,xmm5
	psrlq	xmm5,26
	movdqa	xmm3,xmm6
	psrlq	xmm6,26
	pand	xmm0,xmm7
	paddd	xmm1,xmm5
	pand	xmm3,xmm7
	paddd	xmm4,xmm6
	sub	ecx,16
	jz	NEAR L$013done
L$012even:
	lea	edx,[384+esp]
	lea	eax,[esi-32]
	sub	ecx,64
	movdqu	xmm5,[edi]
	pshufd	xmm6,xmm5,68
	cmovb	esi,eax
	pshufd	xmm5,xmm5,238
	movdqa	[edx],xmm6
	lea	eax,[160+esp]
	movdqu	xmm6,[16+edi]
	movdqa	[edx-144],xmm5
	pshufd	xmm5,xmm6,68
	pshufd	xmm6,xmm6,238
	movdqa	[16+edx],xmm5
	movdqu	xmm5,[32+edi]
	movdqa	[edx-128],xmm6
	pshufd	xmm6,xmm5,68
	pshufd	xmm5,xmm5,238
	movdqa	[32+edx],xmm6
	movdqu	xmm6,[48+edi]
	movdqa	[edx-112],xmm5
	pshufd	xmm5,xmm6,68
	pshufd	xmm6,xmm6,238
	movdqa	[48+edx],xmm5
	movdqu	xmm5,[64+edi]
	movdqa	[edx-96],xmm6
	pshufd	xmm6,xmm5,68
	pshufd	xmm5,xmm5,238
	movdqa	[64+edx],xmm6
	movdqu	xmm6,[80+edi]
	movdqa	[edx-80],xmm5
	pshufd	xmm5,xmm6,68
	pshufd	xmm6,xmm6,238
	movdqa	[80+edx],xmm5
	movdqu	xmm5,[96+edi]
	movdqa	[edx-64],xmm6
	pshufd	xmm6,xmm5,68
	pshufd	xmm5,xmm5,238
	movdqa	[96+edx],xmm6
	movdqu	xmm6,[112+edi]
	movdqa	[edx-48],xmm5
	pshufd	xmm5,xmm6,68
	pshufd	xmm6,xmm6,238
	movdqa	[112+edx],xmm5
	movdqu	xmm5,[128+edi]
	movdqa	[edx-32],xmm6
	pshufd	xmm6,xmm5,68
	pshufd	xmm5,xmm5,238
	movdqa	[128+edx],xmm6
	movdqa	[edx-16],xmm5
	movdqu	xmm5,[32+esi]
	movdqu	xmm6,[48+esi]
	lea	esi,[32+esi]
	movdqa	[112+esp],xmm2
	movdqa	[128+esp],xmm3
	movdqa	[144+esp],xmm4
	movdqa	xmm2,xmm5
	movdqa	xmm3,xmm6
	psrldq	xmm2,6
	psrldq	xmm3,6
	movdqa	xmm4,xmm5
	punpcklqdq	xmm2,xmm3
	punpckhqdq	xmm4,xmm6
	punpcklqdq	xmm5,xmm6
	movdqa	xmm3,xmm2
	psrlq	xmm2,4
	psrlq	xmm3,30
	movdqa	xmm6,xmm5
	psrlq	xmm4,40
	psrlq	xmm6,26
	pand	xmm5,xmm7
	pand	xmm6,xmm7
	pand	xmm2,xmm7
	pand	xmm3,xmm7
	por	xmm4,[ebx]
	movdqa	[80+esp],xmm0
	movdqa	[96+esp],xmm1
	jbe	NEAR L$014skip_loop
	jmp	NEAR L$015loop
align	32
L$015loop:
	movdqa	xmm7,[edx-144]
	movdqa	[16+eax],xmm6
	movdqa	[32+eax],xmm2
	movdqa	[48+eax],xmm3
	movdqa	[64+eax],xmm4
	movdqa	xmm1,xmm5
	pmuludq	xmm5,xmm7
	movdqa	xmm0,xmm6
	pmuludq	xmm6,xmm7
	pmuludq	xmm2,xmm7
	pmuludq	xmm3,xmm7
	pmuludq	xmm4,xmm7
	pmuludq	xmm0,[edx-16]
	movdqa	xmm7,xmm1
	pmuludq	xmm1,[edx-128]
	paddq	xmm0,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[edx-112]
	paddq	xmm1,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[edx-96]
	paddq	xmm2,xmm7
	movdqa	xmm7,[16+eax]
	pmuludq	xmm6,[edx-80]
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[edx-128]
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[edx-112]
	paddq	xmm2,xmm7
	movdqa	xmm7,[32+eax]
	pmuludq	xmm6,[edx-96]
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[edx-32]
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[edx-16]
	paddq	xmm0,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[edx-128]
	paddq	xmm1,xmm5
	movdqa	xmm5,[48+eax]
	pmuludq	xmm7,[edx-112]
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[edx-48]
	paddq	xmm4,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[edx-32]
	paddq	xmm0,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[edx-16]
	paddq	xmm1,xmm6
	movdqa	xmm6,[64+eax]
	pmuludq	xmm5,[edx-128]
	paddq	xmm2,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[edx-16]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[edx-64]
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[edx-48]
	paddq	xmm0,xmm7
	movdqa	xmm7,[64+ebx]
	pmuludq	xmm6,[edx-32]
	paddq	xmm1,xmm5
	paddq	xmm2,xmm6
	movdqu	xmm5,[esi-32]
	movdqu	xmm6,[esi-16]
	lea	esi,[32+esi]
	movdqa	[32+esp],xmm2
	movdqa	[48+esp],xmm3
	movdqa	[64+esp],xmm4
	movdqa	xmm2,xmm5
	movdqa	xmm3,xmm6
	psrldq	xmm2,6
	psrldq	xmm3,6
	movdqa	xmm4,xmm5
	punpcklqdq	xmm2,xmm3
	punpckhqdq	xmm4,xmm6
	punpcklqdq	xmm5,xmm6
	movdqa	xmm3,xmm2
	psrlq	xmm2,4
	psrlq	xmm3,30
	movdqa	xmm6,xmm5
	psrlq	xmm4,40
	psrlq	xmm6,26
	pand	xmm5,xmm7
	pand	xmm6,xmm7
	pand	xmm2,xmm7
	pand	xmm3,xmm7
	por	xmm4,[ebx]
	lea	eax,[esi-32]
	sub	ecx,64
	paddd	xmm5,[80+esp]
	paddd	xmm6,[96+esp]
	paddd	xmm2,[112+esp]
	paddd	xmm3,[128+esp]
	paddd	xmm4,[144+esp]
	cmovb	esi,eax
	lea	eax,[160+esp]
	movdqa	xmm7,[edx]
	movdqa	[16+esp],xmm1
	movdqa	[16+eax],xmm6
	movdqa	[32+eax],xmm2
	movdqa	[48+eax],xmm3
	movdqa	[64+eax],xmm4
	movdqa	xmm1,xmm5
	pmuludq	xmm5,xmm7
	paddq	xmm5,xmm0
	movdqa	xmm0,xmm6
	pmuludq	xmm6,xmm7
	pmuludq	xmm2,xmm7
	pmuludq	xmm3,xmm7
	pmuludq	xmm4,xmm7
	paddq	xmm6,[16+esp]
	paddq	xmm2,[32+esp]
	paddq	xmm3,[48+esp]
	paddq	xmm4,[64+esp]
	pmuludq	xmm0,[128+edx]
	movdqa	xmm7,xmm1
	pmuludq	xmm1,[16+edx]
	paddq	xmm0,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[32+edx]
	paddq	xmm1,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[48+edx]
	paddq	xmm2,xmm7
	movdqa	xmm7,[16+eax]
	pmuludq	xmm6,[64+edx]
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+edx]
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[32+edx]
	paddq	xmm2,xmm7
	movdqa	xmm7,[32+eax]
	pmuludq	xmm6,[48+edx]
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[112+edx]
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[128+edx]
	paddq	xmm0,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[16+edx]
	paddq	xmm1,xmm5
	movdqa	xmm5,[48+eax]
	pmuludq	xmm7,[32+edx]
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[96+edx]
	paddq	xmm4,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[112+edx]
	paddq	xmm0,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[128+edx]
	paddq	xmm1,xmm6
	movdqa	xmm6,[64+eax]
	pmuludq	xmm5,[16+edx]
	paddq	xmm2,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[128+edx]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[80+edx]
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[96+edx]
	paddq	xmm0,xmm7
	movdqa	xmm7,[64+ebx]
	pmuludq	xmm6,[112+edx]
	paddq	xmm1,xmm5
	paddq	xmm2,xmm6
	movdqa	xmm5,xmm3
	pand	xmm3,xmm7
	psrlq	xmm5,26
	paddq	xmm5,xmm4
	movdqa	xmm6,xmm0
	pand	xmm0,xmm7
	psrlq	xmm6,26
	movdqa	xmm4,xmm5
	paddq	xmm6,xmm1
	psrlq	xmm5,26
	pand	xmm4,xmm7
	movdqa	xmm1,xmm6
	psrlq	xmm6,26
	paddd	xmm0,xmm5
	psllq	xmm5,2
	paddq	xmm6,xmm2
	paddq	xmm5,xmm0
	pand	xmm1,xmm7
	movdqa	xmm2,xmm6
	psrlq	xmm6,26
	pand	xmm2,xmm7
	paddd	xmm6,xmm3
	movdqa	xmm0,xmm5
	psrlq	xmm5,26
	movdqa	xmm3,xmm6
	psrlq	xmm6,26
	pand	xmm0,xmm7
	paddd	xmm1,xmm5
	pand	xmm3,xmm7
	paddd	xmm4,xmm6
	movdqu	xmm5,[32+esi]
	movdqu	xmm6,[48+esi]
	lea	esi,[32+esi]
	movdqa	[112+esp],xmm2
	movdqa	[128+esp],xmm3
	movdqa	[144+esp],xmm4
	movdqa	xmm2,xmm5
	movdqa	xmm3,xmm6
	psrldq	xmm2,6
	psrldq	xmm3,6
	movdqa	xmm4,xmm5
	punpcklqdq	xmm2,xmm3
	punpckhqdq	xmm4,xmm6
	punpcklqdq	xmm5,xmm6
	movdqa	xmm3,xmm2
	psrlq	xmm2,4
	psrlq	xmm3,30
	movdqa	xmm6,xmm5
	psrlq	xmm4,40
	psrlq	xmm6,26
	pand	xmm5,xmm7
	pand	xmm6,xmm7
	pand	xmm2,xmm7
	pand	xmm3,xmm7
	por	xmm4,[ebx]
	movdqa	[80+esp],xmm0
	movdqa	[96+esp],xmm1
	ja	NEAR L$015loop
L$014skip_loop:
	pshufd	xmm7,[edx-144],16
	add	ecx,32
	jnz	NEAR L$016long_tail
	paddd	xmm5,xmm0
	paddd	xmm6,xmm1
	paddd	xmm2,[112+esp]
	paddd	xmm3,[128+esp]
	paddd	xmm4,[144+esp]
L$016long_tail:
	movdqa	[eax],xmm5
	movdqa	[16+eax],xmm6
	movdqa	[32+eax],xmm2
	movdqa	[48+eax],xmm3
	movdqa	[64+eax],xmm4
	pmuludq	xmm5,xmm7
	pmuludq	xmm6,xmm7
	pmuludq	xmm2,xmm7
	movdqa	xmm0,xmm5
	pshufd	xmm5,[edx-128],16
	pmuludq	xmm3,xmm7
	movdqa	xmm1,xmm6
	pmuludq	xmm4,xmm7
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[48+eax]
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[32+eax]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+eax]
	paddq	xmm3,xmm6
	pshufd	xmm6,[edx-64],16
	pmuludq	xmm5,[eax]
	paddq	xmm2,xmm7
	pmuludq	xmm6,[64+eax]
	pshufd	xmm7,[edx-112],16
	paddq	xmm1,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[32+eax]
	paddq	xmm0,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[16+eax]
	paddq	xmm4,xmm7
	pshufd	xmm7,[edx-48],16
	pmuludq	xmm6,[eax]
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[64+eax]
	paddq	xmm2,xmm6
	pmuludq	xmm5,[48+eax]
	pshufd	xmm6,[edx-96],16
	paddq	xmm1,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[16+eax]
	paddq	xmm0,xmm5
	pshufd	xmm5,[edx-32],16
	pmuludq	xmm7,[eax]
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[64+eax]
	paddq	xmm3,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[48+eax]
	paddq	xmm2,xmm5
	pmuludq	xmm7,[32+eax]
	pshufd	xmm5,[edx-80],16
	paddq	xmm1,xmm6
	pshufd	xmm6,[edx-16],16
	pmuludq	xmm5,[eax]
	paddq	xmm0,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[64+eax]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+eax]
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[32+eax]
	paddq	xmm0,xmm7
	pmuludq	xmm6,[48+eax]
	movdqa	xmm7,[64+ebx]
	paddq	xmm1,xmm5
	paddq	xmm2,xmm6
	jz	NEAR L$017short_tail
	movdqu	xmm5,[esi-32]
	movdqu	xmm6,[esi-16]
	lea	esi,[32+esi]
	movdqa	[32+esp],xmm2
	movdqa	[48+esp],xmm3
	movdqa	[64+esp],xmm4
	movdqa	xmm2,xmm5
	movdqa	xmm3,xmm6
	psrldq	xmm2,6
	psrldq	xmm3,6
	movdqa	xmm4,xmm5
	punpcklqdq	xmm2,xmm3
	punpckhqdq	xmm4,xmm6
	punpcklqdq	xmm5,xmm6
	movdqa	xmm3,xmm2
	psrlq	xmm2,4
	psrlq	xmm3,30
	movdqa	xmm6,xmm5
	psrlq	xmm4,40
	psrlq	xmm6,26
	pand	xmm5,xmm7
	pand	xmm6,xmm7
	pand	xmm2,xmm7
	pand	xmm3,xmm7
	por	xmm4,[ebx]
	pshufd	xmm7,[edx],16
	paddd	xmm5,[80+esp]
	paddd	xmm6,[96+esp]
	paddd	xmm2,[112+esp]
	paddd	xmm3,[128+esp]
	paddd	xmm4,[144+esp]
	movdqa	[esp],xmm5
	pmuludq	xmm5,xmm7
	movdqa	[16+esp],xmm6
	pmuludq	xmm6,xmm7
	paddq	xmm0,xmm5
	movdqa	xmm5,xmm2
	pmuludq	xmm2,xmm7
	paddq	xmm1,xmm6
	movdqa	xmm6,xmm3
	pmuludq	xmm3,xmm7
	paddq	xmm2,[32+esp]
	movdqa	[32+esp],xmm5
	pshufd	xmm5,[16+edx],16
	paddq	xmm3,[48+esp]
	movdqa	[48+esp],xmm6
	movdqa	xmm6,xmm4
	pmuludq	xmm4,xmm7
	paddq	xmm4,[64+esp]
	movdqa	[64+esp],xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[48+esp]
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[32+esp]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+esp]
	paddq	xmm3,xmm6
	pshufd	xmm6,[80+edx],16
	pmuludq	xmm5,[esp]
	paddq	xmm2,xmm7
	pmuludq	xmm6,[64+esp]
	pshufd	xmm7,[32+edx],16
	paddq	xmm1,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[32+esp]
	paddq	xmm0,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[16+esp]
	paddq	xmm4,xmm7
	pshufd	xmm7,[96+edx],16
	pmuludq	xmm6,[esp]
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[64+esp]
	paddq	xmm2,xmm6
	pmuludq	xmm5,[48+esp]
	pshufd	xmm6,[48+edx],16
	paddq	xmm1,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[16+esp]
	paddq	xmm0,xmm5
	pshufd	xmm5,[112+edx],16
	pmuludq	xmm7,[esp]
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[64+esp]
	paddq	xmm3,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[48+esp]
	paddq	xmm2,xmm5
	pmuludq	xmm7,[32+esp]
	pshufd	xmm5,[64+edx],16
	paddq	xmm1,xmm6
	pshufd	xmm6,[128+edx],16
	pmuludq	xmm5,[esp]
	paddq	xmm0,xmm7
	movdqa	xmm7,xmm6
	pmuludq	xmm6,[64+esp]
	paddq	xmm4,xmm5
	movdqa	xmm5,xmm7
	pmuludq	xmm7,[16+esp]
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm5
	pmuludq	xmm5,[32+esp]
	paddq	xmm0,xmm7
	pmuludq	xmm6,[48+esp]
	movdqa	xmm7,[64+ebx]
	paddq	xmm1,xmm5
	paddq	xmm2,xmm6
L$017short_tail:
	pshufd	xmm6,xmm4,78
	pshufd	xmm5,xmm3,78
	paddq	xmm4,xmm6
	paddq	xmm3,xmm5
	pshufd	xmm6,xmm0,78
	pshufd	xmm5,xmm1,78
	paddq	xmm0,xmm6
	paddq	xmm1,xmm5
	pshufd	xmm6,xmm2,78
	movdqa	xmm5,xmm3
	pand	xmm3,xmm7
	psrlq	xmm5,26
	paddq	xmm2,xmm6
	paddq	xmm5,xmm4
	movdqa	xmm6,xmm0
	pand	xmm0,xmm7
	psrlq	xmm6,26
	movdqa	xmm4,xmm5
	paddq	xmm6,xmm1
	psrlq	xmm5,26
	pand	xmm4,xmm7
	movdqa	xmm1,xmm6
	psrlq	xmm6,26
	paddd	xmm0,xmm5
	psllq	xmm5,2
	paddq	xmm6,xmm2
	paddq	xmm5,xmm0
	pand	xmm1,xmm7
	movdqa	xmm2,xmm6
	psrlq	xmm6,26
	pand	xmm2,xmm7
	paddd	xmm6,xmm3
	movdqa	xmm0,xmm5
	psrlq	xmm5,26
	movdqa	xmm3,xmm6
	psrlq	xmm6,26
	pand	xmm0,xmm7
	paddd	xmm1,xmm5
	pand	xmm3,xmm7
	paddd	xmm4,xmm6
L$013done:
	movd	DWORD [edi-48],xmm0
	movd	DWORD [edi-44],xmm1
	movd	DWORD [edi-40],xmm2
	movd	DWORD [edi-36],xmm3
	movd	DWORD [edi-32],xmm4
	mov	esp,ebp
L$007nodata:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	32
align	16
__poly1305_emit_sse2:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,DWORD [20+esp]
	cmp	DWORD [20+ebp],0
	je	NEAR L$enter_emit
	mov	eax,DWORD [ebp]
	mov	edi,DWORD [4+ebp]
	mov	ecx,DWORD [8+ebp]
	mov	edx,DWORD [12+ebp]
	mov	esi,DWORD [16+ebp]
	mov	ebx,edi
	shl	edi,26
	shr	ebx,6
	add	eax,edi
	mov	edi,ecx
	adc	ebx,0
	shl	edi,20
	shr	ecx,12
	add	ebx,edi
	mov	edi,edx
	adc	ecx,0
	shl	edi,14
	shr	edx,18
	add	ecx,edi
	mov	edi,esi
	adc	edx,0
	shl	edi,8
	shr	esi,24
	add	edx,edi
	adc	esi,0
	mov	edi,esi
	and	esi,3
	shr	edi,2
	lea	ebp,[edi*4+edi]
	mov	edi,DWORD [24+esp]
	add	eax,ebp
	mov	ebp,DWORD [28+esp]
	adc	ebx,0
	adc	ecx,0
	adc	edx,0
	adc	esi,0
	movd	xmm0,eax
	add	eax,5
	movd	xmm1,ebx
	adc	ebx,0
	movd	xmm2,ecx
	adc	ecx,0
	movd	xmm3,edx
	adc	edx,0
	adc	esi,0
	shr	esi,2
	neg	esi
	and	eax,esi
	and	ebx,esi
	and	ecx,esi
	and	edx,esi
	mov	DWORD [edi],eax
	movd	eax,xmm0
	mov	DWORD [4+edi],ebx
	movd	ebx,xmm1
	mov	DWORD [8+edi],ecx
	movd	ecx,xmm2
	mov	DWORD [12+edi],edx
	movd	edx,xmm3
	not	esi
	and	eax,esi
	and	ebx,esi
	or	eax,DWORD [edi]
	and	ecx,esi
	or	ebx,DWORD [4+edi]
	and	edx,esi
	or	ecx,DWORD [8+edi]
	or	edx,DWORD [12+edi]
	add	eax,DWORD [ebp]
	adc	ebx,DWORD [4+ebp]
	mov	DWORD [edi],eax
	adc	ecx,DWORD [8+ebp]
	mov	DWORD [4+edi],ebx
	adc	edx,DWORD [12+ebp]
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	32
align	16
__poly1305_init_avx2:
	vmovdqu	xmm4,[24+edi]
	lea	edi,[48+edi]
	mov	ebp,esp
	sub	esp,224
	and	esp,-16
	vmovdqa	xmm7,[64+ebx]
	vpand	xmm0,xmm4,xmm7
	vpsrlq	xmm1,xmm4,26
	vpsrldq	xmm3,xmm4,6
	vpand	xmm1,xmm1,xmm7
	vpsrlq	xmm2,xmm3,4
	vpsrlq	xmm3,xmm3,30
	vpand	xmm2,xmm2,xmm7
	vpand	xmm3,xmm3,xmm7
	vpsrldq	xmm4,xmm4,13
	lea	edx,[144+esp]
	mov	ecx,2
L$018square:
	vmovdqa	[esp],xmm0
	vmovdqa	[16+esp],xmm1
	vmovdqa	[32+esp],xmm2
	vmovdqa	[48+esp],xmm3
	vmovdqa	[64+esp],xmm4
	vpslld	xmm6,xmm1,2
	vpslld	xmm5,xmm2,2
	vpaddd	xmm6,xmm6,xmm1
	vpaddd	xmm5,xmm5,xmm2
	vmovdqa	[80+esp],xmm6
	vmovdqa	[96+esp],xmm5
	vpslld	xmm6,xmm3,2
	vpslld	xmm5,xmm4,2
	vpaddd	xmm6,xmm6,xmm3
	vpaddd	xmm5,xmm5,xmm4
	vmovdqa	[112+esp],xmm6
	vmovdqa	[128+esp],xmm5
	vpshufd	xmm5,xmm0,68
	vmovdqa	xmm6,xmm1
	vpshufd	xmm1,xmm1,68
	vpshufd	xmm2,xmm2,68
	vpshufd	xmm3,xmm3,68
	vpshufd	xmm4,xmm4,68
	vmovdqa	[edx],xmm5
	vmovdqa	[16+edx],xmm1
	vmovdqa	[32+edx],xmm2
	vmovdqa	[48+edx],xmm3
	vmovdqa	[64+edx],xmm4
	vpmuludq	xmm4,xmm4,xmm0
	vpmuludq	xmm3,xmm3,xmm0
	vpmuludq	xmm2,xmm2,xmm0
	vpmuludq	xmm1,xmm1,xmm0
	vpmuludq	xmm0,xmm5,xmm0
	vpmuludq	xmm5,xmm6,[48+edx]
	vpaddq	xmm4,xmm4,xmm5
	vpmuludq	xmm7,xmm6,[32+edx]
	vpaddq	xmm3,xmm3,xmm7
	vpmuludq	xmm5,xmm6,[16+edx]
	vpaddq	xmm2,xmm2,xmm5
	vmovdqa	xmm7,[80+esp]
	vpmuludq	xmm6,xmm6,[edx]
	vpaddq	xmm1,xmm1,xmm6
	vmovdqa	xmm5,[32+esp]
	vpmuludq	xmm7,xmm7,[64+edx]
	vpaddq	xmm0,xmm0,xmm7
	vpmuludq	xmm6,xmm5,[32+edx]
	vpaddq	xmm4,xmm4,xmm6
	vpmuludq	xmm7,xmm5,[16+edx]
	vpaddq	xmm3,xmm3,xmm7
	vmovdqa	xmm6,[96+esp]
	vpmuludq	xmm5,xmm5,[edx]
	vpaddq	xmm2,xmm2,xmm5
	vpmuludq	xmm7,xmm6,[64+edx]
	vpaddq	xmm1,xmm1,xmm7
	vmovdqa	xmm5,[48+esp]
	vpmuludq	xmm6,xmm6,[48+edx]
	vpaddq	xmm0,xmm0,xmm6
	vpmuludq	xmm7,xmm5,[16+edx]
	vpaddq	xmm4,xmm4,xmm7
	vmovdqa	xmm6,[112+esp]
	vpmuludq	xmm5,xmm5,[edx]
	vpaddq	xmm3,xmm3,xmm5
	vpmuludq	xmm7,xmm6,[64+edx]
	vpaddq	xmm2,xmm2,xmm7
	vpmuludq	xmm5,xmm6,[48+edx]
	vpaddq	xmm1,xmm1,xmm5
	vmovdqa	xmm7,[64+esp]
	vpmuludq	xmm6,xmm6,[32+edx]
	vpaddq	xmm0,xmm0,xmm6
	vmovdqa	xmm5,[128+esp]
	vpmuludq	xmm7,xmm7,[edx]
	vpaddq	xmm4,xmm4,xmm7
	vpmuludq	xmm6,xmm5,[64+edx]
	vpaddq	xmm3,xmm3,xmm6
	vpmuludq	xmm7,xmm5,[16+edx]
	vpaddq	xmm0,xmm0,xmm7
	vpmuludq	xmm6,xmm5,[32+edx]
	vpaddq	xmm1,xmm1,xmm6
	vmovdqa	xmm7,[64+ebx]
	vpmuludq	xmm5,xmm5,[48+edx]
	vpaddq	xmm2,xmm2,xmm5
	vpsrlq	xmm5,xmm3,26
	vpand	xmm3,xmm3,xmm7
	vpsrlq	xmm6,xmm0,26
	vpand	xmm0,xmm0,xmm7
	vpaddq	xmm4,xmm4,xmm5
	vpaddq	xmm1,xmm1,xmm6
	vpsrlq	xmm5,xmm4,26
	vpand	xmm4,xmm4,xmm7
	vpsrlq	xmm6,xmm1,26
	vpand	xmm1,xmm1,xmm7
	vpaddq	xmm2,xmm2,xmm6
	vpaddd	xmm0,xmm0,xmm5
	vpsllq	xmm5,xmm5,2
	vpsrlq	xmm6,xmm2,26
	vpand	xmm2,xmm2,xmm7
	vpaddd	xmm0,xmm0,xmm5
	vpaddd	xmm3,xmm3,xmm6
	vpsrlq	xmm6,xmm3,26
	vpsrlq	xmm5,xmm0,26
	vpand	xmm0,xmm0,xmm7
	vpand	xmm3,xmm3,xmm7
	vpaddd	xmm1,xmm1,xmm5
	vpaddd	xmm4,xmm4,xmm6
	dec	ecx
	jz	NEAR L$019square_break
	vpunpcklqdq	xmm0,xmm0,[esp]
	vpunpcklqdq	xmm1,xmm1,[16+esp]
	vpunpcklqdq	xmm2,xmm2,[32+esp]
	vpunpcklqdq	xmm3,xmm3,[48+esp]
	vpunpcklqdq	xmm4,xmm4,[64+esp]
	jmp	NEAR L$018square
L$019square_break:
	vpsllq	xmm0,xmm0,32
	vpsllq	xmm1,xmm1,32
	vpsllq	xmm2,xmm2,32
	vpsllq	xmm3,xmm3,32
	vpsllq	xmm4,xmm4,32
	vpor	xmm0,xmm0,[esp]
	vpor	xmm1,xmm1,[16+esp]
	vpor	xmm2,xmm2,[32+esp]
	vpor	xmm3,xmm3,[48+esp]
	vpor	xmm4,xmm4,[64+esp]
	vpshufd	xmm0,xmm0,141
	vpshufd	xmm1,xmm1,141
	vpshufd	xmm2,xmm2,141
	vpshufd	xmm3,xmm3,141
	vpshufd	xmm4,xmm4,141
	vmovdqu	[edi],xmm0
	vmovdqu	[16+edi],xmm1
	vmovdqu	[32+edi],xmm2
	vmovdqu	[48+edi],xmm3
	vmovdqu	[64+edi],xmm4
	vpslld	xmm6,xmm1,2
	vpslld	xmm5,xmm2,2
	vpaddd	xmm6,xmm6,xmm1
	vpaddd	xmm5,xmm5,xmm2
	vmovdqu	[80+edi],xmm6
	vmovdqu	[96+edi],xmm5
	vpslld	xmm6,xmm3,2
	vpslld	xmm5,xmm4,2
	vpaddd	xmm6,xmm6,xmm3
	vpaddd	xmm5,xmm5,xmm4
	vmovdqu	[112+edi],xmm6
	vmovdqu	[128+edi],xmm5
	mov	esp,ebp
	lea	edi,[edi-48]
	ret
align	32
align	16
__poly1305_blocks_avx2:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	ecx,DWORD [28+esp]
	mov	eax,DWORD [20+edi]
	and	ecx,-16
	jz	NEAR L$020nodata
	cmp	ecx,64
	jae	NEAR L$021enter_avx2
	test	eax,eax
	jz	NEAR L$enter_blocks
L$021enter_avx2:
	vzeroupper
	call	L$022pic_point
L$022pic_point:
	pop	ebx
	lea	ebx,[(L$const_sse2-L$022pic_point)+ebx]
	test	eax,eax
	jnz	NEAR L$023base2_26
	call	__poly1305_init_avx2
	mov	eax,DWORD [edi]
	mov	ecx,DWORD [3+edi]
	mov	edx,DWORD [6+edi]
	mov	esi,DWORD [9+edi]
	mov	ebp,DWORD [13+edi]
	shr	ecx,2
	and	eax,67108863
	shr	edx,4
	and	ecx,67108863
	shr	esi,6
	and	edx,67108863
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ecx
	mov	DWORD [8+edi],edx
	mov	DWORD [12+edi],esi
	mov	DWORD [16+edi],ebp
	mov	DWORD [20+edi],1
	mov	esi,DWORD [24+esp]
	mov	ecx,DWORD [28+esp]
L$023base2_26:
	mov	eax,DWORD [32+esp]
	mov	ebp,esp
	sub	esp,448
	and	esp,-512
	vmovdqu	xmm0,[48+edi]
	lea	edx,[288+esp]
	vmovdqu	xmm1,[64+edi]
	vmovdqu	xmm2,[80+edi]
	vmovdqu	xmm3,[96+edi]
	vmovdqu	xmm4,[112+edi]
	lea	edi,[48+edi]
	vpermq	ymm0,ymm0,64
	vpermq	ymm1,ymm1,64
	vpermq	ymm2,ymm2,64
	vpermq	ymm3,ymm3,64
	vpermq	ymm4,ymm4,64
	vpshufd	ymm0,ymm0,200
	vpshufd	ymm1,ymm1,200
	vpshufd	ymm2,ymm2,200
	vpshufd	ymm3,ymm3,200
	vpshufd	ymm4,ymm4,200
	vmovdqa	[edx-128],ymm0
	vmovdqu	xmm0,[80+edi]
	vmovdqa	[edx-96],ymm1
	vmovdqu	xmm1,[96+edi]
	vmovdqa	[edx-64],ymm2
	vmovdqu	xmm2,[112+edi]
	vmovdqa	[edx-32],ymm3
	vmovdqu	xmm3,[128+edi]
	vmovdqa	[edx],ymm4
	vpermq	ymm0,ymm0,64
	vpermq	ymm1,ymm1,64
	vpermq	ymm2,ymm2,64
	vpermq	ymm3,ymm3,64
	vpshufd	ymm0,ymm0,200
	vpshufd	ymm1,ymm1,200
	vpshufd	ymm2,ymm2,200
	vpshufd	ymm3,ymm3,200
	vmovdqa	[32+edx],ymm0
	vmovd	xmm0,DWORD [edi-48]
	vmovdqa	[64+edx],ymm1
	vmovd	xmm1,DWORD [edi-44]
	vmovdqa	[96+edx],ymm2
	vmovd	xmm2,DWORD [edi-40]
	vmovdqa	[128+edx],ymm3
	vmovd	xmm3,DWORD [edi-36]
	vmovd	xmm4,DWORD [edi-32]
	vmovdqa	ymm7,[64+ebx]
	neg	eax
	test	ecx,63
	jz	NEAR L$024even
	mov	edx,ecx
	and	ecx,-64
	and	edx,63
	vmovdqu	xmm5,[esi]
	cmp	edx,32
	jb	NEAR L$025one
	vmovdqu	xmm6,[16+esi]
	je	NEAR L$026two
	vinserti128	ymm5,ymm5,[32+esi],1
	lea	esi,[48+esi]
	lea	ebx,[8+ebx]
	lea	edx,[296+esp]
	jmp	NEAR L$027tail
L$026two:
	lea	esi,[32+esi]
	lea	ebx,[16+ebx]
	lea	edx,[304+esp]
	jmp	NEAR L$027tail
L$025one:
	lea	esi,[16+esi]
	vpxor	ymm6,ymm6,ymm6
	lea	ebx,[32+eax*8+ebx]
	lea	edx,[312+esp]
	jmp	NEAR L$027tail
align	32
L$024even:
	vmovdqu	xmm5,[esi]
	vmovdqu	xmm6,[16+esi]
	vinserti128	ymm5,ymm5,[32+esi],1
	vinserti128	ymm6,ymm6,[48+esi],1
	lea	esi,[64+esi]
	sub	ecx,64
	jz	NEAR L$027tail
L$028loop:
	vmovdqa	[64+esp],ymm2
	vpsrldq	ymm2,ymm5,6
	vmovdqa	[esp],ymm0
	vpsrldq	ymm0,ymm6,6
	vmovdqa	[32+esp],ymm1
	vpunpckhqdq	ymm1,ymm5,ymm6
	vpunpcklqdq	ymm5,ymm5,ymm6
	vpunpcklqdq	ymm2,ymm2,ymm0
	vpsrlq	ymm0,ymm2,30
	vpsrlq	ymm2,ymm2,4
	vpsrlq	ymm6,ymm5,26
	vpsrlq	ymm1,ymm1,40
	vpand	ymm2,ymm2,ymm7
	vpand	ymm5,ymm5,ymm7
	vpand	ymm6,ymm6,ymm7
	vpand	ymm0,ymm0,ymm7
	vpor	ymm1,ymm1,[ebx]
	vpaddq	ymm2,ymm2,[64+esp]
	vpaddq	ymm5,ymm5,[esp]
	vpaddq	ymm6,ymm6,[32+esp]
	vpaddq	ymm0,ymm0,ymm3
	vpaddq	ymm1,ymm1,ymm4
	vpmuludq	ymm3,ymm2,[edx-96]
	vmovdqa	[32+esp],ymm6
	vpmuludq	ymm4,ymm2,[edx-64]
	vmovdqa	[96+esp],ymm0
	vpmuludq	ymm0,ymm2,[96+edx]
	vmovdqa	[128+esp],ymm1
	vpmuludq	ymm1,ymm2,[128+edx]
	vpmuludq	ymm2,ymm2,[edx-128]
	vpmuludq	ymm7,ymm5,[edx-32]
	vpaddq	ymm3,ymm3,ymm7
	vpmuludq	ymm6,ymm5,[edx]
	vpaddq	ymm4,ymm4,ymm6
	vpmuludq	ymm7,ymm5,[edx-128]
	vpaddq	ymm0,ymm0,ymm7
	vmovdqa	ymm7,[32+esp]
	vpmuludq	ymm6,ymm5,[edx-96]
	vpaddq	ymm1,ymm1,ymm6
	vpmuludq	ymm5,ymm5,[edx-64]
	vpaddq	ymm2,ymm2,ymm5
	vpmuludq	ymm6,ymm7,[edx-64]
	vpaddq	ymm3,ymm3,ymm6
	vpmuludq	ymm5,ymm7,[edx-32]
	vpaddq	ymm4,ymm4,ymm5
	vpmuludq	ymm6,ymm7,[128+edx]
	vpaddq	ymm0,ymm0,ymm6
	vmovdqa	ymm6,[96+esp]
	vpmuludq	ymm5,ymm7,[edx-128]
	vpaddq	ymm1,ymm1,ymm5
	vpmuludq	ymm7,ymm7,[edx-96]
	vpaddq	ymm2,ymm2,ymm7
	vpmuludq	ymm5,ymm6,[edx-128]
	vpaddq	ymm3,ymm3,ymm5
	vpmuludq	ymm7,ymm6,[edx-96]
	vpaddq	ymm4,ymm4,ymm7
	vpmuludq	ymm5,ymm6,[64+edx]
	vpaddq	ymm0,ymm0,ymm5
	vmovdqa	ymm5,[128+esp]
	vpmuludq	ymm7,ymm6,[96+edx]
	vpaddq	ymm1,ymm1,ymm7
	vpmuludq	ymm6,ymm6,[128+edx]
	vpaddq	ymm2,ymm2,ymm6
	vpmuludq	ymm7,ymm5,[128+edx]
	vpaddq	ymm3,ymm3,ymm7
	vpmuludq	ymm6,ymm5,[32+edx]
	vpaddq	ymm0,ymm0,ymm6
	vpmuludq	ymm7,ymm5,[edx-128]
	vpaddq	ymm4,ymm4,ymm7
	vmovdqa	ymm7,[64+ebx]
	vpmuludq	ymm6,ymm5,[64+edx]
	vpaddq	ymm1,ymm1,ymm6
	vpmuludq	ymm5,ymm5,[96+edx]
	vpaddq	ymm2,ymm2,ymm5
	vpsrlq	ymm5,ymm3,26
	vpand	ymm3,ymm3,ymm7
	vpsrlq	ymm6,ymm0,26
	vpand	ymm0,ymm0,ymm7
	vpaddq	ymm4,ymm4,ymm5
	vpaddq	ymm1,ymm1,ymm6
	vpsrlq	ymm5,ymm4,26
	vpand	ymm4,ymm4,ymm7
	vpsrlq	ymm6,ymm1,26
	vpand	ymm1,ymm1,ymm7
	vpaddq	ymm2,ymm2,ymm6
	vpaddq	ymm0,ymm0,ymm5
	vpsllq	ymm5,ymm5,2
	vpsrlq	ymm6,ymm2,26
	vpand	ymm2,ymm2,ymm7
	vpaddq	ymm0,ymm0,ymm5
	vpaddq	ymm3,ymm3,ymm6
	vpsrlq	ymm6,ymm3,26
	vpsrlq	ymm5,ymm0,26
	vpand	ymm0,ymm0,ymm7
	vpand	ymm3,ymm3,ymm7
	vpaddq	ymm1,ymm1,ymm5
	vpaddq	ymm4,ymm4,ymm6
	vmovdqu	xmm5,[esi]
	vmovdqu	xmm6,[16+esi]
	vinserti128	ymm5,ymm5,[32+esi],1
	vinserti128	ymm6,ymm6,[48+esi],1
	lea	esi,[64+esi]
	sub	ecx,64
	jnz	NEAR L$028loop
L$027tail:
	vmovdqa	[64+esp],ymm2
	vpsrldq	ymm2,ymm5,6
	vmovdqa	[esp],ymm0
	vpsrldq	ymm0,ymm6,6
	vmovdqa	[32+esp],ymm1
	vpunpckhqdq	ymm1,ymm5,ymm6
	vpunpcklqdq	ymm5,ymm5,ymm6
	vpunpcklqdq	ymm2,ymm2,ymm0
	vpsrlq	ymm0,ymm2,30
	vpsrlq	ymm2,ymm2,4
	vpsrlq	ymm6,ymm5,26
	vpsrlq	ymm1,ymm1,40
	vpand	ymm2,ymm2,ymm7
	vpand	ymm5,ymm5,ymm7
	vpand	ymm6,ymm6,ymm7
	vpand	ymm0,ymm0,ymm7
	vpor	ymm1,ymm1,[ebx]
	and	ebx,-64
	vpaddq	ymm2,ymm2,[64+esp]
	vpaddq	ymm5,ymm5,[esp]
	vpaddq	ymm6,ymm6,[32+esp]
	vpaddq	ymm0,ymm0,ymm3
	vpaddq	ymm1,ymm1,ymm4
	vpmuludq	ymm3,ymm2,[edx-92]
	vmovdqa	[32+esp],ymm6
	vpmuludq	ymm4,ymm2,[edx-60]
	vmovdqa	[96+esp],ymm0
	vpmuludq	ymm0,ymm2,[100+edx]
	vmovdqa	[128+esp],ymm1
	vpmuludq	ymm1,ymm2,[132+edx]
	vpmuludq	ymm2,ymm2,[edx-124]
	vpmuludq	ymm7,ymm5,[edx-28]
	vpaddq	ymm3,ymm3,ymm7
	vpmuludq	ymm6,ymm5,[4+edx]
	vpaddq	ymm4,ymm4,ymm6
	vpmuludq	ymm7,ymm5,[edx-124]
	vpaddq	ymm0,ymm0,ymm7
	vmovdqa	ymm7,[32+esp]
	vpmuludq	ymm6,ymm5,[edx-92]
	vpaddq	ymm1,ymm1,ymm6
	vpmuludq	ymm5,ymm5,[edx-60]
	vpaddq	ymm2,ymm2,ymm5
	vpmuludq	ymm6,ymm7,[edx-60]
	vpaddq	ymm3,ymm3,ymm6
	vpmuludq	ymm5,ymm7,[edx-28]
	vpaddq	ymm4,ymm4,ymm5
	vpmuludq	ymm6,ymm7,[132+edx]
	vpaddq	ymm0,ymm0,ymm6
	vmovdqa	ymm6,[96+esp]
	vpmuludq	ymm5,ymm7,[edx-124]
	vpaddq	ymm1,ymm1,ymm5
	vpmuludq	ymm7,ymm7,[edx-92]
	vpaddq	ymm2,ymm2,ymm7
	vpmuludq	ymm5,ymm6,[edx-124]
	vpaddq	ymm3,ymm3,ymm5
	vpmuludq	ymm7,ymm6,[edx-92]
	vpaddq	ymm4,ymm4,ymm7
	vpmuludq	ymm5,ymm6,[68+edx]
	vpaddq	ymm0,ymm0,ymm5
	vmovdqa	ymm5,[128+esp]
	vpmuludq	ymm7,ymm6,[100+edx]
	vpaddq	ymm1,ymm1,ymm7
	vpmuludq	ymm6,ymm6,[132+edx]
	vpaddq	ymm2,ymm2,ymm6
	vpmuludq	ymm7,ymm5,[132+edx]
	vpaddq	ymm3,ymm3,ymm7
	vpmuludq	ymm6,ymm5,[36+edx]
	vpaddq	ymm0,ymm0,ymm6
	vpmuludq	ymm7,ymm5,[edx-124]
	vpaddq	ymm4,ymm4,ymm7
	vmovdqa	ymm7,[64+ebx]
	vpmuludq	ymm6,ymm5,[68+edx]
	vpaddq	ymm1,ymm1,ymm6
	vpmuludq	ymm5,ymm5,[100+edx]
	vpaddq	ymm2,ymm2,ymm5
	vpsrldq	ymm5,ymm4,8
	vpsrldq	ymm6,ymm3,8
	vpaddq	ymm4,ymm4,ymm5
	vpsrldq	ymm5,ymm0,8
	vpaddq	ymm3,ymm3,ymm6
	vpsrldq	ymm6,ymm1,8
	vpaddq	ymm0,ymm0,ymm5
	vpsrldq	ymm5,ymm2,8
	vpaddq	ymm1,ymm1,ymm6
	vpermq	ymm6,ymm4,2
	vpaddq	ymm2,ymm2,ymm5
	vpermq	ymm5,ymm3,2
	vpaddq	ymm4,ymm4,ymm6
	vpermq	ymm6,ymm0,2
	vpaddq	ymm3,ymm3,ymm5
	vpermq	ymm5,ymm1,2
	vpaddq	ymm0,ymm0,ymm6
	vpermq	ymm6,ymm2,2
	vpaddq	ymm1,ymm1,ymm5
	vpaddq	ymm2,ymm2,ymm6
	vpsrlq	ymm5,ymm3,26
	vpand	ymm3,ymm3,ymm7
	vpsrlq	ymm6,ymm0,26
	vpand	ymm0,ymm0,ymm7
	vpaddq	ymm4,ymm4,ymm5
	vpaddq	ymm1,ymm1,ymm6
	vpsrlq	ymm5,ymm4,26
	vpand	ymm4,ymm4,ymm7
	vpsrlq	ymm6,ymm1,26
	vpand	ymm1,ymm1,ymm7
	vpaddq	ymm2,ymm2,ymm6
	vpaddq	ymm0,ymm0,ymm5
	vpsllq	ymm5,ymm5,2
	vpsrlq	ymm6,ymm2,26
	vpand	ymm2,ymm2,ymm7
	vpaddq	ymm0,ymm0,ymm5
	vpaddq	ymm3,ymm3,ymm6
	vpsrlq	ymm6,ymm3,26
	vpsrlq	ymm5,ymm0,26
	vpand	ymm0,ymm0,ymm7
	vpand	ymm3,ymm3,ymm7
	vpaddq	ymm1,ymm1,ymm5
	vpaddq	ymm4,ymm4,ymm6
	cmp	ecx,0
	je	NEAR L$029done
	vpshufd	xmm0,xmm0,252
	lea	edx,[288+esp]
	vpshufd	xmm1,xmm1,252
	vpshufd	xmm2,xmm2,252
	vpshufd	xmm3,xmm3,252
	vpshufd	xmm4,xmm4,252
	jmp	NEAR L$024even
align	16
L$029done:
	vmovd	DWORD [edi-48],xmm0
	vmovd	DWORD [edi-44],xmm1
	vmovd	DWORD [edi-40],xmm2
	vmovd	DWORD [edi-36],xmm3
	vmovd	DWORD [edi-32],xmm4
	vzeroupper
	mov	esp,ebp
L$020nodata:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	64
L$const_sse2:
dd	16777216,0,16777216,0,16777216,0,16777216,0
dd	0,0,0,0,0,0,0,0
dd	67108863,0,67108863,0,67108863,0,67108863,0
dd	268435455,268435452,268435452,268435452
db	80,111,108,121,49,51,48,53,32,102,111,114,32,120,56,54
db	44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32
db	60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111
db	114,103,62,0
align	4
segment	.bss
common	_OPENSSL_ia32cap_P 40
