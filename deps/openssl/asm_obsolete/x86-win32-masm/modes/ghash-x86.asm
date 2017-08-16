TITLE	ghash-x86.asm
IF @Version LT 800
ECHO MASM version 8.00 or later is strongly recommended.
ENDIF
.686
.XMM
IF @Version LT 800
XMMWORD STRUCT 16
DQ	2 dup (?)
XMMWORD	ENDS
ENDIF

.MODEL	FLAT
OPTION	DOTNAME
IF @Version LT 800
.text$	SEGMENT PAGE 'CODE'
ELSE
.text$	SEGMENT ALIGN(64) 'CODE'
ENDIF
ALIGN	16
_gcm_gmult_4bit_x86	PROC PUBLIC
$L_gcm_gmult_4bit_x86_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	sub	esp,84
	mov	edi,DWORD PTR 104[esp]
	mov	esi,DWORD PTR 108[esp]
	mov	ebp,DWORD PTR [edi]
	mov	edx,DWORD PTR 4[edi]
	mov	ecx,DWORD PTR 8[edi]
	mov	ebx,DWORD PTR 12[edi]
	mov	DWORD PTR 16[esp],0
	mov	DWORD PTR 20[esp],471859200
	mov	DWORD PTR 24[esp],943718400
	mov	DWORD PTR 28[esp],610271232
	mov	DWORD PTR 32[esp],1887436800
	mov	DWORD PTR 36[esp],1822425088
	mov	DWORD PTR 40[esp],1220542464
	mov	DWORD PTR 44[esp],1423966208
	mov	DWORD PTR 48[esp],3774873600
	mov	DWORD PTR 52[esp],4246732800
	mov	DWORD PTR 56[esp],3644850176
	mov	DWORD PTR 60[esp],3311403008
	mov	DWORD PTR 64[esp],2441084928
	mov	DWORD PTR 68[esp],2376073216
	mov	DWORD PTR 72[esp],2847932416
	mov	DWORD PTR 76[esp],3051356160
	mov	DWORD PTR [esp],ebp
	mov	DWORD PTR 4[esp],edx
	mov	DWORD PTR 8[esp],ecx
	mov	DWORD PTR 12[esp],ebx
	shr	ebx,20
	and	ebx,240
	mov	ebp,DWORD PTR 4[ebx*1+esi]
	mov	edx,DWORD PTR [ebx*1+esi]
	mov	ecx,DWORD PTR 12[ebx*1+esi]
	mov	ebx,DWORD PTR 8[ebx*1+esi]
	xor	eax,eax
	mov	edi,15
	jmp	$L000x86_loop
ALIGN	16
$L000x86_loop:
	mov	al,bl
	shrd	ebx,ecx,4
	and	al,15
	shrd	ecx,edx,4
	shrd	edx,ebp,4
	shr	ebp,4
	xor	ebp,DWORD PTR 16[eax*4+esp]
	mov	al,BYTE PTR [edi*1+esp]
	and	al,240
	xor	ebx,DWORD PTR 8[eax*1+esi]
	xor	ecx,DWORD PTR 12[eax*1+esi]
	xor	edx,DWORD PTR [eax*1+esi]
	xor	ebp,DWORD PTR 4[eax*1+esi]
	dec	edi
	js	$L001x86_break
	mov	al,bl
	shrd	ebx,ecx,4
	and	al,15
	shrd	ecx,edx,4
	shrd	edx,ebp,4
	shr	ebp,4
	xor	ebp,DWORD PTR 16[eax*4+esp]
	mov	al,BYTE PTR [edi*1+esp]
	shl	al,4
	xor	ebx,DWORD PTR 8[eax*1+esi]
	xor	ecx,DWORD PTR 12[eax*1+esi]
	xor	edx,DWORD PTR [eax*1+esi]
	xor	ebp,DWORD PTR 4[eax*1+esi]
	jmp	$L000x86_loop
ALIGN	16
$L001x86_break:
	bswap	ebx
	bswap	ecx
	bswap	edx
	bswap	ebp
	mov	edi,DWORD PTR 104[esp]
	mov	DWORD PTR 12[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 4[edi],edx
	mov	DWORD PTR [edi],ebp
	add	esp,84
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_gcm_gmult_4bit_x86 ENDP
ALIGN	16
_gcm_ghash_4bit_x86	PROC PUBLIC
$L_gcm_ghash_4bit_x86_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	sub	esp,84
	mov	ebx,DWORD PTR 104[esp]
	mov	esi,DWORD PTR 108[esp]
	mov	edi,DWORD PTR 112[esp]
	mov	ecx,DWORD PTR 116[esp]
	add	ecx,edi
	mov	DWORD PTR 116[esp],ecx
	mov	ebp,DWORD PTR [ebx]
	mov	edx,DWORD PTR 4[ebx]
	mov	ecx,DWORD PTR 8[ebx]
	mov	ebx,DWORD PTR 12[ebx]
	mov	DWORD PTR 16[esp],0
	mov	DWORD PTR 20[esp],471859200
	mov	DWORD PTR 24[esp],943718400
	mov	DWORD PTR 28[esp],610271232
	mov	DWORD PTR 32[esp],1887436800
	mov	DWORD PTR 36[esp],1822425088
	mov	DWORD PTR 40[esp],1220542464
	mov	DWORD PTR 44[esp],1423966208
	mov	DWORD PTR 48[esp],3774873600
	mov	DWORD PTR 52[esp],4246732800
	mov	DWORD PTR 56[esp],3644850176
	mov	DWORD PTR 60[esp],3311403008
	mov	DWORD PTR 64[esp],2441084928
	mov	DWORD PTR 68[esp],2376073216
	mov	DWORD PTR 72[esp],2847932416
	mov	DWORD PTR 76[esp],3051356160
ALIGN	16
$L002x86_outer_loop:
	xor	ebx,DWORD PTR 12[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 4[edi]
	xor	ebp,DWORD PTR [edi]
	mov	DWORD PTR 12[esp],ebx
	mov	DWORD PTR 8[esp],ecx
	mov	DWORD PTR 4[esp],edx
	mov	DWORD PTR [esp],ebp
	shr	ebx,20
	and	ebx,240
	mov	ebp,DWORD PTR 4[ebx*1+esi]
	mov	edx,DWORD PTR [ebx*1+esi]
	mov	ecx,DWORD PTR 12[ebx*1+esi]
	mov	ebx,DWORD PTR 8[ebx*1+esi]
	xor	eax,eax
	mov	edi,15
	jmp	$L003x86_loop
ALIGN	16
$L003x86_loop:
	mov	al,bl
	shrd	ebx,ecx,4
	and	al,15
	shrd	ecx,edx,4
	shrd	edx,ebp,4
	shr	ebp,4
	xor	ebp,DWORD PTR 16[eax*4+esp]
	mov	al,BYTE PTR [edi*1+esp]
	and	al,240
	xor	ebx,DWORD PTR 8[eax*1+esi]
	xor	ecx,DWORD PTR 12[eax*1+esi]
	xor	edx,DWORD PTR [eax*1+esi]
	xor	ebp,DWORD PTR 4[eax*1+esi]
	dec	edi
	js	$L004x86_break
	mov	al,bl
	shrd	ebx,ecx,4
	and	al,15
	shrd	ecx,edx,4
	shrd	edx,ebp,4
	shr	ebp,4
	xor	ebp,DWORD PTR 16[eax*4+esp]
	mov	al,BYTE PTR [edi*1+esp]
	shl	al,4
	xor	ebx,DWORD PTR 8[eax*1+esi]
	xor	ecx,DWORD PTR 12[eax*1+esi]
	xor	edx,DWORD PTR [eax*1+esi]
	xor	ebp,DWORD PTR 4[eax*1+esi]
	jmp	$L003x86_loop
ALIGN	16
$L004x86_break:
	bswap	ebx
	bswap	ecx
	bswap	edx
	bswap	ebp
	mov	edi,DWORD PTR 112[esp]
	lea	edi,DWORD PTR 16[edi]
	cmp	edi,DWORD PTR 116[esp]
	mov	DWORD PTR 112[esp],edi
	jb	$L002x86_outer_loop
	mov	edi,DWORD PTR 104[esp]
	mov	DWORD PTR 12[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 4[edi],edx
	mov	DWORD PTR [edi],ebp
	add	esp,84
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_gcm_ghash_4bit_x86 ENDP
ALIGN	16
_gcm_gmult_4bit_mmx	PROC PUBLIC
$L_gcm_gmult_4bit_mmx_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD PTR 20[esp]
	mov	esi,DWORD PTR 24[esp]
	call	$L005pic_point
$L005pic_point:
	pop	eax
	lea	eax,DWORD PTR ($Lrem_4bit-$L005pic_point)[eax]
	movzx	ebx,BYTE PTR 15[edi]
	xor	ecx,ecx
	mov	edx,ebx
	mov	cl,dl
	mov	ebp,14
	shl	cl,4
	and	edx,240
	movq	mm0,QWORD PTR 8[ecx*1+esi]
	movq	mm1,QWORD PTR [ecx*1+esi]
	movd	ebx,mm0
	jmp	$L006mmx_loop
ALIGN	16
$L006mmx_loop:
	psrlq	mm0,4
	and	ebx,15
	movq	mm2,mm1
	psrlq	mm1,4
	pxor	mm0,QWORD PTR 8[edx*1+esi]
	mov	cl,BYTE PTR [ebp*1+edi]
	psllq	mm2,60
	pxor	mm1,QWORD PTR [ebx*8+eax]
	dec	ebp
	movd	ebx,mm0
	pxor	mm1,QWORD PTR [edx*1+esi]
	mov	edx,ecx
	pxor	mm0,mm2
	js	$L007mmx_break
	shl	cl,4
	and	ebx,15
	psrlq	mm0,4
	and	edx,240
	movq	mm2,mm1
	psrlq	mm1,4
	pxor	mm0,QWORD PTR 8[ecx*1+esi]
	psllq	mm2,60
	pxor	mm1,QWORD PTR [ebx*8+eax]
	movd	ebx,mm0
	pxor	mm1,QWORD PTR [ecx*1+esi]
	pxor	mm0,mm2
	jmp	$L006mmx_loop
ALIGN	16
$L007mmx_break:
	shl	cl,4
	and	ebx,15
	psrlq	mm0,4
	and	edx,240
	movq	mm2,mm1
	psrlq	mm1,4
	pxor	mm0,QWORD PTR 8[ecx*1+esi]
	psllq	mm2,60
	pxor	mm1,QWORD PTR [ebx*8+eax]
	movd	ebx,mm0
	pxor	mm1,QWORD PTR [ecx*1+esi]
	pxor	mm0,mm2
	psrlq	mm0,4
	and	ebx,15
	movq	mm2,mm1
	psrlq	mm1,4
	pxor	mm0,QWORD PTR 8[edx*1+esi]
	psllq	mm2,60
	pxor	mm1,QWORD PTR [ebx*8+eax]
	movd	ebx,mm0
	pxor	mm1,QWORD PTR [edx*1+esi]
	pxor	mm0,mm2
	psrlq	mm0,32
	movd	edx,mm1
	psrlq	mm1,32
	movd	ecx,mm0
	movd	ebp,mm1
	bswap	ebx
	bswap	edx
	bswap	ecx
	bswap	ebp
	emms
	mov	DWORD PTR 12[edi],ebx
	mov	DWORD PTR 4[edi],edx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR [edi],ebp
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_gcm_gmult_4bit_mmx ENDP
ALIGN	16
_gcm_ghash_4bit_mmx	PROC PUBLIC
$L_gcm_ghash_4bit_mmx_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,DWORD PTR 20[esp]
	mov	ebx,DWORD PTR 24[esp]
	mov	ecx,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 32[esp]
	mov	ebp,esp
	call	$L008pic_point
$L008pic_point:
	pop	esi
	lea	esi,DWORD PTR ($Lrem_8bit-$L008pic_point)[esi]
	sub	esp,544
	and	esp,-64
	sub	esp,16
	add	edx,ecx
	mov	DWORD PTR 544[esp],eax
	mov	DWORD PTR 552[esp],edx
	mov	DWORD PTR 556[esp],ebp
	add	ebx,128
	lea	edi,DWORD PTR 144[esp]
	lea	ebp,DWORD PTR 400[esp]
	mov	edx,DWORD PTR [ebx-120]
	movq	mm0,QWORD PTR [ebx-120]
	movq	mm3,QWORD PTR [ebx-128]
	shl	edx,4
	mov	BYTE PTR [esp],dl
	mov	edx,DWORD PTR [ebx-104]
	movq	mm2,QWORD PTR [ebx-104]
	movq	mm5,QWORD PTR [ebx-112]
	movq	QWORD PTR [edi-128],mm0
	psrlq	mm0,4
	movq	QWORD PTR [edi],mm3
	movq	mm7,mm3
	psrlq	mm3,4
	shl	edx,4
	mov	BYTE PTR 1[esp],dl
	mov	edx,DWORD PTR [ebx-88]
	movq	mm1,QWORD PTR [ebx-88]
	psllq	mm7,60
	movq	mm4,QWORD PTR [ebx-96]
	por	mm0,mm7
	movq	QWORD PTR [edi-120],mm2
	psrlq	mm2,4
	movq	QWORD PTR 8[edi],mm5
	movq	mm6,mm5
	movq	QWORD PTR [ebp-128],mm0
	psrlq	mm5,4
	movq	QWORD PTR [ebp],mm3
	shl	edx,4
	mov	BYTE PTR 2[esp],dl
	mov	edx,DWORD PTR [ebx-72]
	movq	mm0,QWORD PTR [ebx-72]
	psllq	mm6,60
	movq	mm3,QWORD PTR [ebx-80]
	por	mm2,mm6
	movq	QWORD PTR [edi-112],mm1
	psrlq	mm1,4
	movq	QWORD PTR 16[edi],mm4
	movq	mm7,mm4
	movq	QWORD PTR [ebp-120],mm2
	psrlq	mm4,4
	movq	QWORD PTR 8[ebp],mm5
	shl	edx,4
	mov	BYTE PTR 3[esp],dl
	mov	edx,DWORD PTR [ebx-56]
	movq	mm2,QWORD PTR [ebx-56]
	psllq	mm7,60
	movq	mm5,QWORD PTR [ebx-64]
	por	mm1,mm7
	movq	QWORD PTR [edi-104],mm0
	psrlq	mm0,4
	movq	QWORD PTR 24[edi],mm3
	movq	mm6,mm3
	movq	QWORD PTR [ebp-112],mm1
	psrlq	mm3,4
	movq	QWORD PTR 16[ebp],mm4
	shl	edx,4
	mov	BYTE PTR 4[esp],dl
	mov	edx,DWORD PTR [ebx-40]
	movq	mm1,QWORD PTR [ebx-40]
	psllq	mm6,60
	movq	mm4,QWORD PTR [ebx-48]
	por	mm0,mm6
	movq	QWORD PTR [edi-96],mm2
	psrlq	mm2,4
	movq	QWORD PTR 32[edi],mm5
	movq	mm7,mm5
	movq	QWORD PTR [ebp-104],mm0
	psrlq	mm5,4
	movq	QWORD PTR 24[ebp],mm3
	shl	edx,4
	mov	BYTE PTR 5[esp],dl
	mov	edx,DWORD PTR [ebx-24]
	movq	mm0,QWORD PTR [ebx-24]
	psllq	mm7,60
	movq	mm3,QWORD PTR [ebx-32]
	por	mm2,mm7
	movq	QWORD PTR [edi-88],mm1
	psrlq	mm1,4
	movq	QWORD PTR 40[edi],mm4
	movq	mm6,mm4
	movq	QWORD PTR [ebp-96],mm2
	psrlq	mm4,4
	movq	QWORD PTR 32[ebp],mm5
	shl	edx,4
	mov	BYTE PTR 6[esp],dl
	mov	edx,DWORD PTR [ebx-8]
	movq	mm2,QWORD PTR [ebx-8]
	psllq	mm6,60
	movq	mm5,QWORD PTR [ebx-16]
	por	mm1,mm6
	movq	QWORD PTR [edi-80],mm0
	psrlq	mm0,4
	movq	QWORD PTR 48[edi],mm3
	movq	mm7,mm3
	movq	QWORD PTR [ebp-88],mm1
	psrlq	mm3,4
	movq	QWORD PTR 40[ebp],mm4
	shl	edx,4
	mov	BYTE PTR 7[esp],dl
	mov	edx,DWORD PTR 8[ebx]
	movq	mm1,QWORD PTR 8[ebx]
	psllq	mm7,60
	movq	mm4,QWORD PTR [ebx]
	por	mm0,mm7
	movq	QWORD PTR [edi-72],mm2
	psrlq	mm2,4
	movq	QWORD PTR 56[edi],mm5
	movq	mm6,mm5
	movq	QWORD PTR [ebp-80],mm0
	psrlq	mm5,4
	movq	QWORD PTR 48[ebp],mm3
	shl	edx,4
	mov	BYTE PTR 8[esp],dl
	mov	edx,DWORD PTR 24[ebx]
	movq	mm0,QWORD PTR 24[ebx]
	psllq	mm6,60
	movq	mm3,QWORD PTR 16[ebx]
	por	mm2,mm6
	movq	QWORD PTR [edi-64],mm1
	psrlq	mm1,4
	movq	QWORD PTR 64[edi],mm4
	movq	mm7,mm4
	movq	QWORD PTR [ebp-72],mm2
	psrlq	mm4,4
	movq	QWORD PTR 56[ebp],mm5
	shl	edx,4
	mov	BYTE PTR 9[esp],dl
	mov	edx,DWORD PTR 40[ebx]
	movq	mm2,QWORD PTR 40[ebx]
	psllq	mm7,60
	movq	mm5,QWORD PTR 32[ebx]
	por	mm1,mm7
	movq	QWORD PTR [edi-56],mm0
	psrlq	mm0,4
	movq	QWORD PTR 72[edi],mm3
	movq	mm6,mm3
	movq	QWORD PTR [ebp-64],mm1
	psrlq	mm3,4
	movq	QWORD PTR 64[ebp],mm4
	shl	edx,4
	mov	BYTE PTR 10[esp],dl
	mov	edx,DWORD PTR 56[ebx]
	movq	mm1,QWORD PTR 56[ebx]
	psllq	mm6,60
	movq	mm4,QWORD PTR 48[ebx]
	por	mm0,mm6
	movq	QWORD PTR [edi-48],mm2
	psrlq	mm2,4
	movq	QWORD PTR 80[edi],mm5
	movq	mm7,mm5
	movq	QWORD PTR [ebp-56],mm0
	psrlq	mm5,4
	movq	QWORD PTR 72[ebp],mm3
	shl	edx,4
	mov	BYTE PTR 11[esp],dl
	mov	edx,DWORD PTR 72[ebx]
	movq	mm0,QWORD PTR 72[ebx]
	psllq	mm7,60
	movq	mm3,QWORD PTR 64[ebx]
	por	mm2,mm7
	movq	QWORD PTR [edi-40],mm1
	psrlq	mm1,4
	movq	QWORD PTR 88[edi],mm4
	movq	mm6,mm4
	movq	QWORD PTR [ebp-48],mm2
	psrlq	mm4,4
	movq	QWORD PTR 80[ebp],mm5
	shl	edx,4
	mov	BYTE PTR 12[esp],dl
	mov	edx,DWORD PTR 88[ebx]
	movq	mm2,QWORD PTR 88[ebx]
	psllq	mm6,60
	movq	mm5,QWORD PTR 80[ebx]
	por	mm1,mm6
	movq	QWORD PTR [edi-32],mm0
	psrlq	mm0,4
	movq	QWORD PTR 96[edi],mm3
	movq	mm7,mm3
	movq	QWORD PTR [ebp-40],mm1
	psrlq	mm3,4
	movq	QWORD PTR 88[ebp],mm4
	shl	edx,4
	mov	BYTE PTR 13[esp],dl
	mov	edx,DWORD PTR 104[ebx]
	movq	mm1,QWORD PTR 104[ebx]
	psllq	mm7,60
	movq	mm4,QWORD PTR 96[ebx]
	por	mm0,mm7
	movq	QWORD PTR [edi-24],mm2
	psrlq	mm2,4
	movq	QWORD PTR 104[edi],mm5
	movq	mm6,mm5
	movq	QWORD PTR [ebp-32],mm0
	psrlq	mm5,4
	movq	QWORD PTR 96[ebp],mm3
	shl	edx,4
	mov	BYTE PTR 14[esp],dl
	mov	edx,DWORD PTR 120[ebx]
	movq	mm0,QWORD PTR 120[ebx]
	psllq	mm6,60
	movq	mm3,QWORD PTR 112[ebx]
	por	mm2,mm6
	movq	QWORD PTR [edi-16],mm1
	psrlq	mm1,4
	movq	QWORD PTR 112[edi],mm4
	movq	mm7,mm4
	movq	QWORD PTR [ebp-24],mm2
	psrlq	mm4,4
	movq	QWORD PTR 104[ebp],mm5
	shl	edx,4
	mov	BYTE PTR 15[esp],dl
	psllq	mm7,60
	por	mm1,mm7
	movq	QWORD PTR [edi-8],mm0
	psrlq	mm0,4
	movq	QWORD PTR 120[edi],mm3
	movq	mm6,mm3
	movq	QWORD PTR [ebp-16],mm1
	psrlq	mm3,4
	movq	QWORD PTR 112[ebp],mm4
	psllq	mm6,60
	por	mm0,mm6
	movq	QWORD PTR [ebp-8],mm0
	movq	QWORD PTR 120[ebp],mm3
	movq	mm6,QWORD PTR [eax]
	mov	ebx,DWORD PTR 8[eax]
	mov	edx,DWORD PTR 12[eax]
ALIGN	16
$L009outer:
	xor	edx,DWORD PTR 12[ecx]
	xor	ebx,DWORD PTR 8[ecx]
	pxor	mm6,QWORD PTR [ecx]
	lea	ecx,DWORD PTR 16[ecx]
	mov	DWORD PTR 536[esp],ebx
	movq	QWORD PTR 528[esp],mm6
	mov	DWORD PTR 548[esp],ecx
	xor	eax,eax
	rol	edx,8
	mov	al,dl
	mov	ebp,eax
	and	al,15
	shr	ebp,4
	pxor	mm0,mm0
	rol	edx,8
	pxor	mm1,mm1
	pxor	mm2,mm2
	movq	mm7,QWORD PTR 16[eax*8+esp]
	movq	mm6,QWORD PTR 144[eax*8+esp]
	mov	al,dl
	movd	ebx,mm7
	psrlq	mm7,8
	movq	mm3,mm6
	mov	edi,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[ebp*8+esp]
	and	al,15
	psllq	mm3,56
	shr	edi,4
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[ebp*8+esp]
	xor	bl,BYTE PTR [ebp*1+esp]
	mov	al,dl
	movd	ecx,mm7
	movzx	ebx,bl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	ebp,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[edi*8+esp]
	and	al,15
	psllq	mm3,56
	shr	ebp,4
	pinsrw	mm2,WORD PTR [ebx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[edi*8+esp]
	xor	cl,BYTE PTR [edi*1+esp]
	mov	al,dl
	mov	edx,DWORD PTR 536[esp]
	movd	ebx,mm7
	movzx	ecx,cl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	edi,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[ebp*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm2
	shr	edi,4
	pinsrw	mm1,WORD PTR [ecx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[ebp*8+esp]
	xor	bl,BYTE PTR [ebp*1+esp]
	mov	al,dl
	movd	ecx,mm7
	movzx	ebx,bl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	ebp,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[edi*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm1
	shr	ebp,4
	pinsrw	mm0,WORD PTR [ebx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[edi*8+esp]
	xor	cl,BYTE PTR [edi*1+esp]
	mov	al,dl
	movd	ebx,mm7
	movzx	ecx,cl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	edi,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[ebp*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm0
	shr	edi,4
	pinsrw	mm2,WORD PTR [ecx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[ebp*8+esp]
	xor	bl,BYTE PTR [ebp*1+esp]
	mov	al,dl
	movd	ecx,mm7
	movzx	ebx,bl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	ebp,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[edi*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm2
	shr	ebp,4
	pinsrw	mm1,WORD PTR [ebx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[edi*8+esp]
	xor	cl,BYTE PTR [edi*1+esp]
	mov	al,dl
	mov	edx,DWORD PTR 532[esp]
	movd	ebx,mm7
	movzx	ecx,cl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	edi,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[ebp*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm1
	shr	edi,4
	pinsrw	mm0,WORD PTR [ecx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[ebp*8+esp]
	xor	bl,BYTE PTR [ebp*1+esp]
	mov	al,dl
	movd	ecx,mm7
	movzx	ebx,bl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	ebp,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[edi*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm0
	shr	ebp,4
	pinsrw	mm2,WORD PTR [ebx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[edi*8+esp]
	xor	cl,BYTE PTR [edi*1+esp]
	mov	al,dl
	movd	ebx,mm7
	movzx	ecx,cl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	edi,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[ebp*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm2
	shr	edi,4
	pinsrw	mm1,WORD PTR [ecx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[ebp*8+esp]
	xor	bl,BYTE PTR [ebp*1+esp]
	mov	al,dl
	movd	ecx,mm7
	movzx	ebx,bl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	ebp,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[edi*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm1
	shr	ebp,4
	pinsrw	mm0,WORD PTR [ebx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[edi*8+esp]
	xor	cl,BYTE PTR [edi*1+esp]
	mov	al,dl
	mov	edx,DWORD PTR 528[esp]
	movd	ebx,mm7
	movzx	ecx,cl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	edi,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[ebp*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm0
	shr	edi,4
	pinsrw	mm2,WORD PTR [ecx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[ebp*8+esp]
	xor	bl,BYTE PTR [ebp*1+esp]
	mov	al,dl
	movd	ecx,mm7
	movzx	ebx,bl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	ebp,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[edi*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm2
	shr	ebp,4
	pinsrw	mm1,WORD PTR [ebx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[edi*8+esp]
	xor	cl,BYTE PTR [edi*1+esp]
	mov	al,dl
	movd	ebx,mm7
	movzx	ecx,cl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	edi,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[ebp*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm1
	shr	edi,4
	pinsrw	mm0,WORD PTR [ecx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[ebp*8+esp]
	xor	bl,BYTE PTR [ebp*1+esp]
	mov	al,dl
	movd	ecx,mm7
	movzx	ebx,bl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	ebp,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[edi*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm0
	shr	ebp,4
	pinsrw	mm2,WORD PTR [ebx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	rol	edx,8
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[edi*8+esp]
	xor	cl,BYTE PTR [edi*1+esp]
	mov	al,dl
	mov	edx,DWORD PTR 524[esp]
	movd	ebx,mm7
	movzx	ecx,cl
	psrlq	mm7,8
	movq	mm3,mm6
	mov	edi,eax
	psrlq	mm6,8
	pxor	mm7,QWORD PTR 272[ebp*8+esp]
	and	al,15
	psllq	mm3,56
	pxor	mm6,mm2
	shr	edi,4
	pinsrw	mm1,WORD PTR [ecx*2+esi],2
	pxor	mm7,QWORD PTR 16[eax*8+esp]
	pxor	mm6,QWORD PTR 144[eax*8+esp]
	xor	bl,BYTE PTR [ebp*1+esp]
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 400[ebp*8+esp]
	movzx	ebx,bl
	pxor	mm2,mm2
	psllq	mm1,4
	movd	ecx,mm7
	psrlq	mm7,4
	movq	mm3,mm6
	psrlq	mm6,4
	shl	ecx,4
	pxor	mm7,QWORD PTR 16[edi*8+esp]
	psllq	mm3,60
	movzx	ecx,cl
	pxor	mm7,mm3
	pxor	mm6,QWORD PTR 144[edi*8+esp]
	pinsrw	mm0,WORD PTR [ebx*2+esi],2
	pxor	mm6,mm1
	movd	edx,mm7
	pinsrw	mm2,WORD PTR [ecx*2+esi],3
	psllq	mm0,12
	pxor	mm6,mm0
	psrlq	mm7,32
	pxor	mm6,mm2
	mov	ecx,DWORD PTR 548[esp]
	movd	ebx,mm7
	movq	mm3,mm6
	psllw	mm6,8
	psrlw	mm3,8
	por	mm6,mm3
	bswap	edx
	pshufw	mm6,mm6,27
	bswap	ebx
	cmp	ecx,DWORD PTR 552[esp]
	jne	$L009outer
	mov	eax,DWORD PTR 544[esp]
	mov	DWORD PTR 12[eax],edx
	mov	DWORD PTR 8[eax],ebx
	movq	QWORD PTR [eax],mm6
	mov	esp,DWORD PTR 556[esp]
	emms
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_gcm_ghash_4bit_mmx ENDP
ALIGN	16
_gcm_init_clmul	PROC PUBLIC
$L_gcm_init_clmul_begin::
	mov	edx,DWORD PTR 4[esp]
	mov	eax,DWORD PTR 8[esp]
	call	$L010pic
$L010pic:
	pop	ecx
	lea	ecx,DWORD PTR ($Lbswap-$L010pic)[ecx]
	movdqu	xmm2,XMMWORD PTR [eax]
	pshufd	xmm2,xmm2,78
	pshufd	xmm4,xmm2,255
	movdqa	xmm3,xmm2
	psllq	xmm2,1
	pxor	xmm5,xmm5
	psrlq	xmm3,63
	pcmpgtd	xmm5,xmm4
	pslldq	xmm3,8
	por	xmm2,xmm3
	pand	xmm5,XMMWORD PTR 16[ecx]
	pxor	xmm2,xmm5
	movdqa	xmm0,xmm2
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pshufd	xmm4,xmm2,78
	pxor	xmm3,xmm0
	pxor	xmm4,xmm2
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,220,0
	xorps	xmm3,xmm0
	xorps	xmm3,xmm1
	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3
	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
	pshufd	xmm3,xmm2,78
	pshufd	xmm4,xmm0,78
	pxor	xmm3,xmm2
	movdqu	XMMWORD PTR [edx],xmm2
	pxor	xmm4,xmm0
	movdqu	XMMWORD PTR 16[edx],xmm0
DB	102,15,58,15,227,8
	movdqu	XMMWORD PTR 32[edx],xmm4
	ret
_gcm_init_clmul ENDP
ALIGN	16
_gcm_gmult_clmul	PROC PUBLIC
$L_gcm_gmult_clmul_begin::
	mov	eax,DWORD PTR 4[esp]
	mov	edx,DWORD PTR 8[esp]
	call	$L011pic
$L011pic:
	pop	ecx
	lea	ecx,DWORD PTR ($Lbswap-$L011pic)[ecx]
	movdqu	xmm0,XMMWORD PTR [eax]
	movdqa	xmm5,XMMWORD PTR [ecx]
	movups	xmm2,XMMWORD PTR [edx]
DB	102,15,56,0,197
	movups	xmm4,XMMWORD PTR 32[edx]
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pxor	xmm3,xmm0
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,220,0
	xorps	xmm3,xmm0
	xorps	xmm3,xmm1
	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3
	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
DB	102,15,56,0,197
	movdqu	XMMWORD PTR [eax],xmm0
	ret
_gcm_gmult_clmul ENDP
ALIGN	16
_gcm_ghash_clmul	PROC PUBLIC
$L_gcm_ghash_clmul_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,DWORD PTR 20[esp]
	mov	edx,DWORD PTR 24[esp]
	mov	esi,DWORD PTR 28[esp]
	mov	ebx,DWORD PTR 32[esp]
	call	$L012pic
$L012pic:
	pop	ecx
	lea	ecx,DWORD PTR ($Lbswap-$L012pic)[ecx]
	movdqu	xmm0,XMMWORD PTR [eax]
	movdqa	xmm5,XMMWORD PTR [ecx]
	movdqu	xmm2,XMMWORD PTR [edx]
DB	102,15,56,0,197
	sub	ebx,16
	jz	$L013odd_tail
	movdqu	xmm3,XMMWORD PTR [esi]
	movdqu	xmm6,XMMWORD PTR 16[esi]
DB	102,15,56,0,221
DB	102,15,56,0,245
	movdqu	xmm5,XMMWORD PTR 32[edx]
	pxor	xmm0,xmm3
	pshufd	xmm3,xmm6,78
	movdqa	xmm7,xmm6
	pxor	xmm3,xmm6
	lea	esi,DWORD PTR 32[esi]
DB	102,15,58,68,242,0
DB	102,15,58,68,250,17
DB	102,15,58,68,221,0
	movups	xmm2,XMMWORD PTR 16[edx]
	nop
	sub	ebx,32
	jbe	$L014even_tail
	jmp	$L015mod_loop
ALIGN	32
$L015mod_loop:
	pshufd	xmm4,xmm0,78
	movdqa	xmm1,xmm0
	pxor	xmm4,xmm0
	nop
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,229,16
	movups	xmm2,XMMWORD PTR [edx]
	xorps	xmm0,xmm6
	movdqa	xmm5,XMMWORD PTR [ecx]
	xorps	xmm1,xmm7
	movdqu	xmm7,XMMWORD PTR [esi]
	pxor	xmm3,xmm0
	movdqu	xmm6,XMMWORD PTR 16[esi]
	pxor	xmm3,xmm1
DB	102,15,56,0,253
	pxor	xmm4,xmm3
	movdqa	xmm3,xmm4
	psrldq	xmm4,8
	pslldq	xmm3,8
	pxor	xmm1,xmm4
	pxor	xmm0,xmm3
DB	102,15,56,0,245
	pxor	xmm1,xmm7
	movdqa	xmm7,xmm6
	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
DB	102,15,58,68,242,0
	movups	xmm5,XMMWORD PTR 32[edx]
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3
	pshufd	xmm3,xmm7,78
	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm3,xmm7
	pxor	xmm1,xmm4
DB	102,15,58,68,250,17
	movups	xmm2,XMMWORD PTR 16[edx]
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
DB	102,15,58,68,221,0
	lea	esi,DWORD PTR 32[esi]
	sub	ebx,32
	ja	$L015mod_loop
$L014even_tail:
	pshufd	xmm4,xmm0,78
	movdqa	xmm1,xmm0
	pxor	xmm4,xmm0
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,229,16
	movdqa	xmm5,XMMWORD PTR [ecx]
	xorps	xmm0,xmm6
	xorps	xmm1,xmm7
	pxor	xmm3,xmm0
	pxor	xmm3,xmm1
	pxor	xmm4,xmm3
	movdqa	xmm3,xmm4
	psrldq	xmm4,8
	pslldq	xmm3,8
	pxor	xmm1,xmm4
	pxor	xmm0,xmm3
	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3
	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
	test	ebx,ebx
	jnz	$L016done
	movups	xmm2,XMMWORD PTR [edx]
$L013odd_tail:
	movdqu	xmm3,XMMWORD PTR [esi]
DB	102,15,56,0,221
	pxor	xmm0,xmm3
	movdqa	xmm1,xmm0
	pshufd	xmm3,xmm0,78
	pshufd	xmm4,xmm2,78
	pxor	xmm3,xmm0
	pxor	xmm4,xmm2
DB	102,15,58,68,194,0
DB	102,15,58,68,202,17
DB	102,15,58,68,220,0
	xorps	xmm3,xmm0
	xorps	xmm3,xmm1
	movdqa	xmm4,xmm3
	psrldq	xmm3,8
	pslldq	xmm4,8
	pxor	xmm1,xmm3
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm0
	movdqa	xmm3,xmm0
	psllq	xmm0,5
	pxor	xmm3,xmm0
	psllq	xmm0,1
	pxor	xmm0,xmm3
	psllq	xmm0,57
	movdqa	xmm3,xmm0
	pslldq	xmm0,8
	psrldq	xmm3,8
	pxor	xmm0,xmm4
	pxor	xmm1,xmm3
	movdqa	xmm4,xmm0
	psrlq	xmm0,1
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	psrlq	xmm0,5
	pxor	xmm0,xmm4
	psrlq	xmm0,1
	pxor	xmm0,xmm1
$L016done:
DB	102,15,56,0,197
	movdqu	XMMWORD PTR [eax],xmm0
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_gcm_ghash_clmul ENDP
ALIGN	64
$Lbswap::
DB	15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
DB	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,194
ALIGN	64
$Lrem_8bit::
DW	0,450,900,582,1800,1738,1164,1358
DW	3600,4050,3476,3158,2328,2266,2716,2910
DW	7200,7650,8100,7782,6952,6890,6316,6510
DW	4656,5106,4532,4214,5432,5370,5820,6014
DW	14400,14722,15300,14854,16200,16010,15564,15630
DW	13904,14226,13780,13334,12632,12442,13020,13086
DW	9312,9634,10212,9766,9064,8874,8428,8494
DW	10864,11186,10740,10294,11640,11450,12028,12094
DW	28800,28994,29444,29382,30600,30282,29708,30158
DW	32400,32594,32020,31958,31128,30810,31260,31710
DW	27808,28002,28452,28390,27560,27242,26668,27118
DW	25264,25458,24884,24822,26040,25722,26172,26622
DW	18624,18690,19268,19078,20424,19978,19532,19854
DW	18128,18194,17748,17558,16856,16410,16988,17310
DW	21728,21794,22372,22182,21480,21034,20588,20910
DW	23280,23346,22900,22710,24056,23610,24188,24510
DW	57600,57538,57988,58182,58888,59338,58764,58446
DW	61200,61138,60564,60758,59416,59866,60316,59998
DW	64800,64738,65188,65382,64040,64490,63916,63598
DW	62256,62194,61620,61814,62520,62970,63420,63102
DW	55616,55426,56004,56070,56904,57226,56780,56334
DW	55120,54930,54484,54550,53336,53658,54236,53790
DW	50528,50338,50916,50982,49768,50090,49644,49198
DW	52080,51890,51444,51510,52344,52666,53244,52798
DW	37248,36930,37380,37830,38536,38730,38156,38094
DW	40848,40530,39956,40406,39064,39258,39708,39646
DW	36256,35938,36388,36838,35496,35690,35116,35054
DW	33712,33394,32820,33270,33976,34170,34620,34558
DW	43456,43010,43588,43910,44744,44810,44364,44174
DW	42960,42514,42068,42390,41176,41242,41820,41630
DW	46560,46114,46692,47014,45800,45866,45420,45230
DW	48112,47666,47220,47542,48376,48442,49020,48830
ALIGN	64
$Lrem_4bit::
DD	0,0,0,471859200
DD	0,943718400,0,610271232
DD	0,1887436800,0,1822425088
DD	0,1220542464,0,1423966208
DD	0,3774873600,0,4246732800
DD	0,3644850176,0,3311403008
DD	0,2441084928,0,2376073216
DD	0,2847932416,0,3051356160
DB	71,72,65,83,72,32,102,111,114,32,120,56,54,44,32,67
DB	82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112
DB	112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62
DB	0
.text$	ENDS
END
