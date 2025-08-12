%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
;extern	_OPENSSL_ia32cap_P
align	16
__mul_1x1_mmx:
	sub	esp,36
	mov	ecx,eax
	lea	edx,[eax*1+eax]
	and	ecx,1073741823
	lea	ebp,[edx*1+edx]
	mov	DWORD [esp],0
	and	edx,2147483647
	movd	mm2,eax
	movd	mm3,ebx
	mov	DWORD [4+esp],ecx
	xor	ecx,edx
	pxor	mm5,mm5
	pxor	mm4,mm4
	mov	DWORD [8+esp],edx
	xor	edx,ebp
	mov	DWORD [12+esp],ecx
	pcmpgtd	mm5,mm2
	paddd	mm2,mm2
	xor	ecx,edx
	mov	DWORD [16+esp],ebp
	xor	ebp,edx
	pand	mm5,mm3
	pcmpgtd	mm4,mm2
	mov	DWORD [20+esp],ecx
	xor	ebp,ecx
	psllq	mm5,31
	pand	mm4,mm3
	mov	DWORD [24+esp],edx
	mov	esi,7
	mov	DWORD [28+esp],ebp
	mov	ebp,esi
	and	esi,ebx
	shr	ebx,3
	mov	edi,ebp
	psllq	mm4,30
	and	edi,ebx
	shr	ebx,3
	movd	mm0,DWORD [esi*4+esp]
	mov	esi,ebp
	and	esi,ebx
	shr	ebx,3
	movd	mm2,DWORD [edi*4+esp]
	mov	edi,ebp
	psllq	mm2,3
	and	edi,ebx
	shr	ebx,3
	pxor	mm0,mm2
	movd	mm1,DWORD [esi*4+esp]
	mov	esi,ebp
	psllq	mm1,6
	and	esi,ebx
	shr	ebx,3
	pxor	mm0,mm1
	movd	mm2,DWORD [edi*4+esp]
	mov	edi,ebp
	psllq	mm2,9
	and	edi,ebx
	shr	ebx,3
	pxor	mm0,mm2
	movd	mm1,DWORD [esi*4+esp]
	mov	esi,ebp
	psllq	mm1,12
	and	esi,ebx
	shr	ebx,3
	pxor	mm0,mm1
	movd	mm2,DWORD [edi*4+esp]
	mov	edi,ebp
	psllq	mm2,15
	and	edi,ebx
	shr	ebx,3
	pxor	mm0,mm2
	movd	mm1,DWORD [esi*4+esp]
	mov	esi,ebp
	psllq	mm1,18
	and	esi,ebx
	shr	ebx,3
	pxor	mm0,mm1
	movd	mm2,DWORD [edi*4+esp]
	mov	edi,ebp
	psllq	mm2,21
	and	edi,ebx
	shr	ebx,3
	pxor	mm0,mm2
	movd	mm1,DWORD [esi*4+esp]
	mov	esi,ebp
	psllq	mm1,24
	and	esi,ebx
	shr	ebx,3
	pxor	mm0,mm1
	movd	mm2,DWORD [edi*4+esp]
	pxor	mm0,mm4
	psllq	mm2,27
	pxor	mm0,mm2
	movd	mm1,DWORD [esi*4+esp]
	pxor	mm0,mm5
	psllq	mm1,30
	add	esp,36
	pxor	mm0,mm1
	ret
align	16
__mul_1x1_ialu:
	sub	esp,36
	mov	ecx,eax
	lea	edx,[eax*1+eax]
	lea	ebp,[eax*4]
	and	ecx,1073741823
	lea	edi,[eax*1+eax]
	sar	eax,31
	mov	DWORD [esp],0
	and	edx,2147483647
	mov	DWORD [4+esp],ecx
	xor	ecx,edx
	mov	DWORD [8+esp],edx
	xor	edx,ebp
	mov	DWORD [12+esp],ecx
	xor	ecx,edx
	mov	DWORD [16+esp],ebp
	xor	ebp,edx
	mov	DWORD [20+esp],ecx
	xor	ebp,ecx
	sar	edi,31
	and	eax,ebx
	mov	DWORD [24+esp],edx
	and	edi,ebx
	mov	DWORD [28+esp],ebp
	mov	edx,eax
	shl	eax,31
	mov	ecx,edi
	shr	edx,1
	mov	esi,7
	shl	edi,30
	and	esi,ebx
	shr	ecx,2
	xor	eax,edi
	shr	ebx,3
	mov	edi,7
	and	edi,ebx
	shr	ebx,3
	xor	edx,ecx
	xor	eax,DWORD [esi*4+esp]
	mov	esi,7
	and	esi,ebx
	shr	ebx,3
	mov	ebp,DWORD [edi*4+esp]
	mov	edi,7
	mov	ecx,ebp
	shl	ebp,3
	and	edi,ebx
	shr	ecx,29
	xor	eax,ebp
	shr	ebx,3
	xor	edx,ecx
	mov	ecx,DWORD [esi*4+esp]
	mov	esi,7
	mov	ebp,ecx
	shl	ecx,6
	and	esi,ebx
	shr	ebp,26
	xor	eax,ecx
	shr	ebx,3
	xor	edx,ebp
	mov	ebp,DWORD [edi*4+esp]
	mov	edi,7
	mov	ecx,ebp
	shl	ebp,9
	and	edi,ebx
	shr	ecx,23
	xor	eax,ebp
	shr	ebx,3
	xor	edx,ecx
	mov	ecx,DWORD [esi*4+esp]
	mov	esi,7
	mov	ebp,ecx
	shl	ecx,12
	and	esi,ebx
	shr	ebp,20
	xor	eax,ecx
	shr	ebx,3
	xor	edx,ebp
	mov	ebp,DWORD [edi*4+esp]
	mov	edi,7
	mov	ecx,ebp
	shl	ebp,15
	and	edi,ebx
	shr	ecx,17
	xor	eax,ebp
	shr	ebx,3
	xor	edx,ecx
	mov	ecx,DWORD [esi*4+esp]
	mov	esi,7
	mov	ebp,ecx
	shl	ecx,18
	and	esi,ebx
	shr	ebp,14
	xor	eax,ecx
	shr	ebx,3
	xor	edx,ebp
	mov	ebp,DWORD [edi*4+esp]
	mov	edi,7
	mov	ecx,ebp
	shl	ebp,21
	and	edi,ebx
	shr	ecx,11
	xor	eax,ebp
	shr	ebx,3
	xor	edx,ecx
	mov	ecx,DWORD [esi*4+esp]
	mov	esi,7
	mov	ebp,ecx
	shl	ecx,24
	and	esi,ebx
	shr	ebp,8
	xor	eax,ecx
	shr	ebx,3
	xor	edx,ebp
	mov	ebp,DWORD [edi*4+esp]
	mov	ecx,ebp
	shl	ebp,27
	mov	edi,DWORD [esi*4+esp]
	shr	ecx,5
	mov	esi,edi
	xor	eax,ebp
	shl	edi,30
	xor	edx,ecx
	shr	esi,2
	xor	eax,edi
	xor	edx,esi
	add	esp,36
	ret
global	_bn_GF2m_mul_2x2
align	16
_bn_GF2m_mul_2x2:
L$_bn_GF2m_mul_2x2_begin:
	lea	edx,[_OPENSSL_ia32cap_P]
	mov	eax,DWORD [edx]
	mov	edx,DWORD [4+edx]
	test	eax,8388608
	jz	NEAR L$000ialu
	test	eax,16777216
	jz	NEAR L$001mmx
	test	edx,2
	jz	NEAR L$001mmx
	movups	xmm0,[8+esp]
	shufps	xmm0,xmm0,177
db	102,15,58,68,192,1
	mov	eax,DWORD [4+esp]
	movups	[eax],xmm0
	ret
align	16
L$001mmx:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,DWORD [24+esp]
	mov	ebx,DWORD [32+esp]
	call	__mul_1x1_mmx
	movq	mm7,mm0
	mov	eax,DWORD [28+esp]
	mov	ebx,DWORD [36+esp]
	call	__mul_1x1_mmx
	movq	mm6,mm0
	mov	eax,DWORD [24+esp]
	mov	ebx,DWORD [32+esp]
	xor	eax,DWORD [28+esp]
	xor	ebx,DWORD [36+esp]
	call	__mul_1x1_mmx
	pxor	mm0,mm7
	mov	eax,DWORD [20+esp]
	pxor	mm0,mm6
	movq	mm2,mm0
	psllq	mm0,32
	pop	edi
	psrlq	mm2,32
	pop	esi
	pxor	mm0,mm6
	pop	ebx
	pxor	mm2,mm7
	movq	[eax],mm0
	pop	ebp
	movq	[8+eax],mm2
	emms
	ret
align	16
L$000ialu:
	push	ebp
	push	ebx
	push	esi
	push	edi
	sub	esp,20
	mov	eax,DWORD [44+esp]
	mov	ebx,DWORD [52+esp]
	call	__mul_1x1_ialu
	mov	DWORD [8+esp],eax
	mov	DWORD [12+esp],edx
	mov	eax,DWORD [48+esp]
	mov	ebx,DWORD [56+esp]
	call	__mul_1x1_ialu
	mov	DWORD [esp],eax
	mov	DWORD [4+esp],edx
	mov	eax,DWORD [44+esp]
	mov	ebx,DWORD [52+esp]
	xor	eax,DWORD [48+esp]
	xor	ebx,DWORD [56+esp]
	call	__mul_1x1_ialu
	mov	ebp,DWORD [40+esp]
	mov	ebx,DWORD [esp]
	mov	ecx,DWORD [4+esp]
	mov	edi,DWORD [8+esp]
	mov	esi,DWORD [12+esp]
	xor	eax,edx
	xor	edx,ecx
	xor	eax,ebx
	mov	DWORD [ebp],ebx
	xor	edx,edi
	mov	DWORD [12+ebp],esi
	xor	eax,esi
	add	esp,20
	xor	edx,esi
	pop	edi
	xor	eax,edx
	pop	esi
	mov	DWORD [8+ebp],edx
	pop	ebx
	mov	DWORD [4+ebp],eax
	pop	ebp
	ret
db	71,70,40,50,94,109,41,32,77,117,108,116,105,112,108,105
db	99,97,116,105,111,110,32,102,111,114,32,120,56,54,44,32
db	67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97
db	112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103
db	62,0
segment	.bss
common	_OPENSSL_ia32cap_P 40
