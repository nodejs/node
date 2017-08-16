TITLE	aes-586.asm
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
__x86_AES_encrypt_compact	PROC PRIVATE
	mov	DWORD PTR 20[esp],edi
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	esi,DWORD PTR 240[edi]
	lea	esi,DWORD PTR [esi*1+esi-2]
	lea	esi,DWORD PTR [esi*8+edi]
	mov	DWORD PTR 24[esp],esi
	mov	edi,DWORD PTR [ebp-128]
	mov	esi,DWORD PTR [ebp-96]
	mov	edi,DWORD PTR [ebp-64]
	mov	esi,DWORD PTR [ebp-32]
	mov	edi,DWORD PTR [ebp]
	mov	esi,DWORD PTR 32[ebp]
	mov	edi,DWORD PTR 64[ebp]
	mov	esi,DWORD PTR 96[ebp]
ALIGN	16
$L000loop:
	mov	esi,eax
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,bh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,ecx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,edx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 4[esp],esi
	mov	esi,ebx
	and	esi,255
	shr	ebx,16
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,ch
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,edx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,eax
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 8[esp],esi
	mov	esi,ecx
	and	esi,255
	shr	ecx,24
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,dh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,eax
	shr	edi,16
	and	edx,255
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	movzx	edi,bh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	and	edx,255
	movzx	edx,BYTE PTR [edx*1+ebp-128]
	movzx	eax,ah
	movzx	eax,BYTE PTR [eax*1+ebp-128]
	shl	eax,8
	xor	edx,eax
	mov	eax,DWORD PTR 4[esp]
	and	ebx,255
	movzx	ebx,BYTE PTR [ebx*1+ebp-128]
	shl	ebx,16
	xor	edx,ebx
	mov	ebx,DWORD PTR 8[esp]
	movzx	ecx,BYTE PTR [ecx*1+ebp-128]
	shl	ecx,24
	xor	edx,ecx
	mov	ecx,esi
	mov	ebp,2155905152
	and	ebp,ecx
	lea	edi,DWORD PTR [ecx*1+ecx]
	mov	esi,ebp
	shr	ebp,7
	and	edi,4278124286
	sub	esi,ebp
	mov	ebp,ecx
	and	esi,454761243
	ror	ebp,16
	xor	esi,edi
	mov	edi,ecx
	xor	ecx,esi
	ror	edi,24
	xor	esi,ebp
	rol	ecx,24
	xor	esi,edi
	mov	ebp,2155905152
	xor	ecx,esi
	and	ebp,edx
	lea	edi,DWORD PTR [edx*1+edx]
	mov	esi,ebp
	shr	ebp,7
	and	edi,4278124286
	sub	esi,ebp
	mov	ebp,edx
	and	esi,454761243
	ror	ebp,16
	xor	esi,edi
	mov	edi,edx
	xor	edx,esi
	ror	edi,24
	xor	esi,ebp
	rol	edx,24
	xor	esi,edi
	mov	ebp,2155905152
	xor	edx,esi
	and	ebp,eax
	lea	edi,DWORD PTR [eax*1+eax]
	mov	esi,ebp
	shr	ebp,7
	and	edi,4278124286
	sub	esi,ebp
	mov	ebp,eax
	and	esi,454761243
	ror	ebp,16
	xor	esi,edi
	mov	edi,eax
	xor	eax,esi
	ror	edi,24
	xor	esi,ebp
	rol	eax,24
	xor	esi,edi
	mov	ebp,2155905152
	xor	eax,esi
	and	ebp,ebx
	lea	edi,DWORD PTR [ebx*1+ebx]
	mov	esi,ebp
	shr	ebp,7
	and	edi,4278124286
	sub	esi,ebp
	mov	ebp,ebx
	and	esi,454761243
	ror	ebp,16
	xor	esi,edi
	mov	edi,ebx
	xor	ebx,esi
	ror	edi,24
	xor	esi,ebp
	rol	ebx,24
	xor	esi,edi
	xor	ebx,esi
	mov	edi,DWORD PTR 20[esp]
	mov	ebp,DWORD PTR 28[esp]
	add	edi,16
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	cmp	edi,DWORD PTR 24[esp]
	mov	DWORD PTR 20[esp],edi
	jb	$L000loop
	mov	esi,eax
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,bh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,ecx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,edx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 4[esp],esi
	mov	esi,ebx
	and	esi,255
	shr	ebx,16
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,ch
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,edx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,eax
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 8[esp],esi
	mov	esi,ecx
	and	esi,255
	shr	ecx,24
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,dh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,eax
	shr	edi,16
	and	edx,255
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	movzx	edi,bh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	edi,DWORD PTR 20[esp]
	and	edx,255
	movzx	edx,BYTE PTR [edx*1+ebp-128]
	movzx	eax,ah
	movzx	eax,BYTE PTR [eax*1+ebp-128]
	shl	eax,8
	xor	edx,eax
	mov	eax,DWORD PTR 4[esp]
	and	ebx,255
	movzx	ebx,BYTE PTR [ebx*1+ebp-128]
	shl	ebx,16
	xor	edx,ebx
	mov	ebx,DWORD PTR 8[esp]
	movzx	ecx,BYTE PTR [ecx*1+ebp-128]
	shl	ecx,24
	xor	edx,ecx
	mov	ecx,esi
	xor	eax,DWORD PTR 16[edi]
	xor	ebx,DWORD PTR 20[edi]
	xor	ecx,DWORD PTR 24[edi]
	xor	edx,DWORD PTR 28[edi]
	ret
__x86_AES_encrypt_compact ENDP
ALIGN	16
__sse_AES_encrypt_compact	PROC PRIVATE
	pxor	mm0,QWORD PTR [edi]
	pxor	mm4,QWORD PTR 8[edi]
	mov	esi,DWORD PTR 240[edi]
	lea	esi,DWORD PTR [esi*1+esi-2]
	lea	esi,DWORD PTR [esi*8+edi]
	mov	DWORD PTR 24[esp],esi
	mov	eax,454761243
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],eax
	mov	eax,DWORD PTR [ebp-128]
	mov	ebx,DWORD PTR [ebp-96]
	mov	ecx,DWORD PTR [ebp-64]
	mov	edx,DWORD PTR [ebp-32]
	mov	eax,DWORD PTR [ebp]
	mov	ebx,DWORD PTR 32[ebp]
	mov	ecx,DWORD PTR 64[ebp]
	mov	edx,DWORD PTR 96[ebp]
ALIGN	16
$L001loop:
	pshufw	mm1,mm0,8
	pshufw	mm5,mm4,13
	movd	eax,mm1
	movd	ebx,mm5
	mov	DWORD PTR 20[esp],edi
	movzx	esi,al
	movzx	edx,ah
	pshufw	mm2,mm0,13
	movzx	ecx,BYTE PTR [esi*1+ebp-128]
	movzx	edi,bl
	movzx	edx,BYTE PTR [edx*1+ebp-128]
	shr	eax,16
	shl	edx,8
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bh
	shl	esi,16
	pshufw	mm6,mm4,8
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,ah
	shl	esi,24
	shr	ebx,16
	or	edx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bh
	shl	esi,8
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,al
	shl	esi,24
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bl
	movd	eax,mm2
	movd	mm0,ecx
	movzx	ecx,BYTE PTR [edi*1+ebp-128]
	movzx	edi,ah
	shl	ecx,16
	movd	ebx,mm6
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bh
	shl	esi,24
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bl
	shl	esi,8
	shr	ebx,16
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,al
	shr	eax,16
	movd	mm1,ecx
	movzx	ecx,BYTE PTR [edi*1+ebp-128]
	movzx	edi,ah
	shl	ecx,16
	and	eax,255
	or	ecx,esi
	punpckldq	mm0,mm1
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bh
	shl	esi,24
	and	ebx,255
	movzx	eax,BYTE PTR [eax*1+ebp-128]
	or	ecx,esi
	shl	eax,16
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	or	edx,eax
	shl	esi,8
	movzx	ebx,BYTE PTR [ebx*1+ebp-128]
	or	ecx,esi
	or	edx,ebx
	mov	edi,DWORD PTR 20[esp]
	movd	mm4,ecx
	movd	mm5,edx
	punpckldq	mm4,mm5
	add	edi,16
	cmp	edi,DWORD PTR 24[esp]
	ja	$L002out
	movq	mm2,QWORD PTR 8[esp]
	pxor	mm3,mm3
	pxor	mm7,mm7
	movq	mm1,mm0
	movq	mm5,mm4
	pcmpgtb	mm3,mm0
	pcmpgtb	mm7,mm4
	pand	mm3,mm2
	pand	mm7,mm2
	pshufw	mm2,mm0,177
	pshufw	mm6,mm4,177
	paddb	mm0,mm0
	paddb	mm4,mm4
	pxor	mm0,mm3
	pxor	mm4,mm7
	pshufw	mm3,mm2,177
	pshufw	mm7,mm6,177
	pxor	mm1,mm0
	pxor	mm5,mm4
	pxor	mm0,mm2
	pxor	mm4,mm6
	movq	mm2,mm3
	movq	mm6,mm7
	pslld	mm3,8
	pslld	mm7,8
	psrld	mm2,24
	psrld	mm6,24
	pxor	mm0,mm3
	pxor	mm4,mm7
	pxor	mm0,mm2
	pxor	mm4,mm6
	movq	mm3,mm1
	movq	mm7,mm5
	movq	mm2,QWORD PTR [edi]
	movq	mm6,QWORD PTR 8[edi]
	psrld	mm1,8
	psrld	mm5,8
	mov	eax,DWORD PTR [ebp-128]
	pslld	mm3,24
	pslld	mm7,24
	mov	ebx,DWORD PTR [ebp-64]
	pxor	mm0,mm1
	pxor	mm4,mm5
	mov	ecx,DWORD PTR [ebp]
	pxor	mm0,mm3
	pxor	mm4,mm7
	mov	edx,DWORD PTR 64[ebp]
	pxor	mm0,mm2
	pxor	mm4,mm6
	jmp	$L001loop
ALIGN	16
$L002out:
	pxor	mm0,QWORD PTR [edi]
	pxor	mm4,QWORD PTR 8[edi]
	ret
__sse_AES_encrypt_compact ENDP
ALIGN	16
__x86_AES_encrypt	PROC PRIVATE
	mov	DWORD PTR 20[esp],edi
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	esi,DWORD PTR 240[edi]
	lea	esi,DWORD PTR [esi*1+esi-2]
	lea	esi,DWORD PTR [esi*8+edi]
	mov	DWORD PTR 24[esp],esi
ALIGN	16
$L003loop:
	mov	esi,eax
	and	esi,255
	mov	esi,DWORD PTR [esi*8+ebp]
	movzx	edi,bh
	xor	esi,DWORD PTR 3[edi*8+ebp]
	mov	edi,ecx
	shr	edi,16
	and	edi,255
	xor	esi,DWORD PTR 2[edi*8+ebp]
	mov	edi,edx
	shr	edi,24
	xor	esi,DWORD PTR 1[edi*8+ebp]
	mov	DWORD PTR 4[esp],esi
	mov	esi,ebx
	and	esi,255
	shr	ebx,16
	mov	esi,DWORD PTR [esi*8+ebp]
	movzx	edi,ch
	xor	esi,DWORD PTR 3[edi*8+ebp]
	mov	edi,edx
	shr	edi,16
	and	edi,255
	xor	esi,DWORD PTR 2[edi*8+ebp]
	mov	edi,eax
	shr	edi,24
	xor	esi,DWORD PTR 1[edi*8+ebp]
	mov	DWORD PTR 8[esp],esi
	mov	esi,ecx
	and	esi,255
	shr	ecx,24
	mov	esi,DWORD PTR [esi*8+ebp]
	movzx	edi,dh
	xor	esi,DWORD PTR 3[edi*8+ebp]
	mov	edi,eax
	shr	edi,16
	and	edx,255
	and	edi,255
	xor	esi,DWORD PTR 2[edi*8+ebp]
	movzx	edi,bh
	xor	esi,DWORD PTR 1[edi*8+ebp]
	mov	edi,DWORD PTR 20[esp]
	mov	edx,DWORD PTR [edx*8+ebp]
	movzx	eax,ah
	xor	edx,DWORD PTR 3[eax*8+ebp]
	mov	eax,DWORD PTR 4[esp]
	and	ebx,255
	xor	edx,DWORD PTR 2[ebx*8+ebp]
	mov	ebx,DWORD PTR 8[esp]
	xor	edx,DWORD PTR 1[ecx*8+ebp]
	mov	ecx,esi
	add	edi,16
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	cmp	edi,DWORD PTR 24[esp]
	mov	DWORD PTR 20[esp],edi
	jb	$L003loop
	mov	esi,eax
	and	esi,255
	mov	esi,DWORD PTR 2[esi*8+ebp]
	and	esi,255
	movzx	edi,bh
	mov	edi,DWORD PTR [edi*8+ebp]
	and	edi,65280
	xor	esi,edi
	mov	edi,ecx
	shr	edi,16
	and	edi,255
	mov	edi,DWORD PTR [edi*8+ebp]
	and	edi,16711680
	xor	esi,edi
	mov	edi,edx
	shr	edi,24
	mov	edi,DWORD PTR 2[edi*8+ebp]
	and	edi,4278190080
	xor	esi,edi
	mov	DWORD PTR 4[esp],esi
	mov	esi,ebx
	and	esi,255
	shr	ebx,16
	mov	esi,DWORD PTR 2[esi*8+ebp]
	and	esi,255
	movzx	edi,ch
	mov	edi,DWORD PTR [edi*8+ebp]
	and	edi,65280
	xor	esi,edi
	mov	edi,edx
	shr	edi,16
	and	edi,255
	mov	edi,DWORD PTR [edi*8+ebp]
	and	edi,16711680
	xor	esi,edi
	mov	edi,eax
	shr	edi,24
	mov	edi,DWORD PTR 2[edi*8+ebp]
	and	edi,4278190080
	xor	esi,edi
	mov	DWORD PTR 8[esp],esi
	mov	esi,ecx
	and	esi,255
	shr	ecx,24
	mov	esi,DWORD PTR 2[esi*8+ebp]
	and	esi,255
	movzx	edi,dh
	mov	edi,DWORD PTR [edi*8+ebp]
	and	edi,65280
	xor	esi,edi
	mov	edi,eax
	shr	edi,16
	and	edx,255
	and	edi,255
	mov	edi,DWORD PTR [edi*8+ebp]
	and	edi,16711680
	xor	esi,edi
	movzx	edi,bh
	mov	edi,DWORD PTR 2[edi*8+ebp]
	and	edi,4278190080
	xor	esi,edi
	mov	edi,DWORD PTR 20[esp]
	and	edx,255
	mov	edx,DWORD PTR 2[edx*8+ebp]
	and	edx,255
	movzx	eax,ah
	mov	eax,DWORD PTR [eax*8+ebp]
	and	eax,65280
	xor	edx,eax
	mov	eax,DWORD PTR 4[esp]
	and	ebx,255
	mov	ebx,DWORD PTR [ebx*8+ebp]
	and	ebx,16711680
	xor	edx,ebx
	mov	ebx,DWORD PTR 8[esp]
	mov	ecx,DWORD PTR 2[ecx*8+ebp]
	and	ecx,4278190080
	xor	edx,ecx
	mov	ecx,esi
	add	edi,16
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	ret
ALIGN	64
$LAES_Te::
DD	2774754246,2774754246
DD	2222750968,2222750968
DD	2574743534,2574743534
DD	2373680118,2373680118
DD	234025727,234025727
DD	3177933782,3177933782
DD	2976870366,2976870366
DD	1422247313,1422247313
DD	1345335392,1345335392
DD	50397442,50397442
DD	2842126286,2842126286
DD	2099981142,2099981142
DD	436141799,436141799
DD	1658312629,1658312629
DD	3870010189,3870010189
DD	2591454956,2591454956
DD	1170918031,1170918031
DD	2642575903,2642575903
DD	1086966153,1086966153
DD	2273148410,2273148410
DD	368769775,368769775
DD	3948501426,3948501426
DD	3376891790,3376891790
DD	200339707,200339707
DD	3970805057,3970805057
DD	1742001331,1742001331
DD	4255294047,4255294047
DD	3937382213,3937382213
DD	3214711843,3214711843
DD	4154762323,4154762323
DD	2524082916,2524082916
DD	1539358875,1539358875
DD	3266819957,3266819957
DD	486407649,486407649
DD	2928907069,2928907069
DD	1780885068,1780885068
DD	1513502316,1513502316
DD	1094664062,1094664062
DD	49805301,49805301
DD	1338821763,1338821763
DD	1546925160,1546925160
DD	4104496465,4104496465
DD	887481809,887481809
DD	150073849,150073849
DD	2473685474,2473685474
DD	1943591083,1943591083
DD	1395732834,1395732834
DD	1058346282,1058346282
DD	201589768,201589768
DD	1388824469,1388824469
DD	1696801606,1696801606
DD	1589887901,1589887901
DD	672667696,672667696
DD	2711000631,2711000631
DD	251987210,251987210
DD	3046808111,3046808111
DD	151455502,151455502
DD	907153956,907153956
DD	2608889883,2608889883
DD	1038279391,1038279391
DD	652995533,652995533
DD	1764173646,1764173646
DD	3451040383,3451040383
DD	2675275242,2675275242
DD	453576978,453576978
DD	2659418909,2659418909
DD	1949051992,1949051992
DD	773462580,773462580
DD	756751158,756751158
DD	2993581788,2993581788
DD	3998898868,3998898868
DD	4221608027,4221608027
DD	4132590244,4132590244
DD	1295727478,1295727478
DD	1641469623,1641469623
DD	3467883389,3467883389
DD	2066295122,2066295122
DD	1055122397,1055122397
DD	1898917726,1898917726
DD	2542044179,2542044179
DD	4115878822,4115878822
DD	1758581177,1758581177
DD	0,0
DD	753790401,753790401
DD	1612718144,1612718144
DD	536673507,536673507
DD	3367088505,3367088505
DD	3982187446,3982187446
DD	3194645204,3194645204
DD	1187761037,1187761037
DD	3653156455,3653156455
DD	1262041458,1262041458
DD	3729410708,3729410708
DD	3561770136,3561770136
DD	3898103984,3898103984
DD	1255133061,1255133061
DD	1808847035,1808847035
DD	720367557,720367557
DD	3853167183,3853167183
DD	385612781,385612781
DD	3309519750,3309519750
DD	3612167578,3612167578
DD	1429418854,1429418854
DD	2491778321,2491778321
DD	3477423498,3477423498
DD	284817897,284817897
DD	100794884,100794884
DD	2172616702,2172616702
DD	4031795360,4031795360
DD	1144798328,1144798328
DD	3131023141,3131023141
DD	3819481163,3819481163
DD	4082192802,4082192802
DD	4272137053,4272137053
DD	3225436288,3225436288
DD	2324664069,2324664069
DD	2912064063,2912064063
DD	3164445985,3164445985
DD	1211644016,1211644016
DD	83228145,83228145
DD	3753688163,3753688163
DD	3249976951,3249976951
DD	1977277103,1977277103
DD	1663115586,1663115586
DD	806359072,806359072
DD	452984805,452984805
DD	250868733,250868733
DD	1842533055,1842533055
DD	1288555905,1288555905
DD	336333848,336333848
DD	890442534,890442534
DD	804056259,804056259
DD	3781124030,3781124030
DD	2727843637,2727843637
DD	3427026056,3427026056
DD	957814574,957814574
DD	1472513171,1472513171
DD	4071073621,4071073621
DD	2189328124,2189328124
DD	1195195770,1195195770
DD	2892260552,2892260552
DD	3881655738,3881655738
DD	723065138,723065138
DD	2507371494,2507371494
DD	2690670784,2690670784
DD	2558624025,2558624025
DD	3511635870,3511635870
DD	2145180835,2145180835
DD	1713513028,1713513028
DD	2116692564,2116692564
DD	2878378043,2878378043
DD	2206763019,2206763019
DD	3393603212,3393603212
DD	703524551,703524551
DD	3552098411,3552098411
DD	1007948840,1007948840
DD	2044649127,2044649127
DD	3797835452,3797835452
DD	487262998,487262998
DD	1994120109,1994120109
DD	1004593371,1004593371
DD	1446130276,1446130276
DD	1312438900,1312438900
DD	503974420,503974420
DD	3679013266,3679013266
DD	168166924,168166924
DD	1814307912,1814307912
DD	3831258296,3831258296
DD	1573044895,1573044895
DD	1859376061,1859376061
DD	4021070915,4021070915
DD	2791465668,2791465668
DD	2828112185,2828112185
DD	2761266481,2761266481
DD	937747667,937747667
DD	2339994098,2339994098
DD	854058965,854058965
DD	1137232011,1137232011
DD	1496790894,1496790894
DD	3077402074,3077402074
DD	2358086913,2358086913
DD	1691735473,1691735473
DD	3528347292,3528347292
DD	3769215305,3769215305
DD	3027004632,3027004632
DD	4199962284,4199962284
DD	133494003,133494003
DD	636152527,636152527
DD	2942657994,2942657994
DD	2390391540,2390391540
DD	3920539207,3920539207
DD	403179536,403179536
DD	3585784431,3585784431
DD	2289596656,2289596656
DD	1864705354,1864705354
DD	1915629148,1915629148
DD	605822008,605822008
DD	4054230615,4054230615
DD	3350508659,3350508659
DD	1371981463,1371981463
DD	602466507,602466507
DD	2094914977,2094914977
DD	2624877800,2624877800
DD	555687742,555687742
DD	3712699286,3712699286
DD	3703422305,3703422305
DD	2257292045,2257292045
DD	2240449039,2240449039
DD	2423288032,2423288032
DD	1111375484,1111375484
DD	3300242801,3300242801
DD	2858837708,2858837708
DD	3628615824,3628615824
DD	84083462,84083462
DD	32962295,32962295
DD	302911004,302911004
DD	2741068226,2741068226
DD	1597322602,1597322602
DD	4183250862,4183250862
DD	3501832553,3501832553
DD	2441512471,2441512471
DD	1489093017,1489093017
DD	656219450,656219450
DD	3114180135,3114180135
DD	954327513,954327513
DD	335083755,335083755
DD	3013122091,3013122091
DD	856756514,856756514
DD	3144247762,3144247762
DD	1893325225,1893325225
DD	2307821063,2307821063
DD	2811532339,2811532339
DD	3063651117,3063651117
DD	572399164,572399164
DD	2458355477,2458355477
DD	552200649,552200649
DD	1238290055,1238290055
DD	4283782570,4283782570
DD	2015897680,2015897680
DD	2061492133,2061492133
DD	2408352771,2408352771
DD	4171342169,4171342169
DD	2156497161,2156497161
DD	386731290,386731290
DD	3669999461,3669999461
DD	837215959,837215959
DD	3326231172,3326231172
DD	3093850320,3093850320
DD	3275833730,3275833730
DD	2962856233,2962856233
DD	1999449434,1999449434
DD	286199582,286199582
DD	3417354363,3417354363
DD	4233385128,4233385128
DD	3602627437,3602627437
DD	974525996,974525996
DB	99,124,119,123,242,107,111,197
DB	48,1,103,43,254,215,171,118
DB	202,130,201,125,250,89,71,240
DB	173,212,162,175,156,164,114,192
DB	183,253,147,38,54,63,247,204
DB	52,165,229,241,113,216,49,21
DB	4,199,35,195,24,150,5,154
DB	7,18,128,226,235,39,178,117
DB	9,131,44,26,27,110,90,160
DB	82,59,214,179,41,227,47,132
DB	83,209,0,237,32,252,177,91
DB	106,203,190,57,74,76,88,207
DB	208,239,170,251,67,77,51,133
DB	69,249,2,127,80,60,159,168
DB	81,163,64,143,146,157,56,245
DB	188,182,218,33,16,255,243,210
DB	205,12,19,236,95,151,68,23
DB	196,167,126,61,100,93,25,115
DB	96,129,79,220,34,42,144,136
DB	70,238,184,20,222,94,11,219
DB	224,50,58,10,73,6,36,92
DB	194,211,172,98,145,149,228,121
DB	231,200,55,109,141,213,78,169
DB	108,86,244,234,101,122,174,8
DB	186,120,37,46,28,166,180,198
DB	232,221,116,31,75,189,139,138
DB	112,62,181,102,72,3,246,14
DB	97,53,87,185,134,193,29,158
DB	225,248,152,17,105,217,142,148
DB	155,30,135,233,206,85,40,223
DB	140,161,137,13,191,230,66,104
DB	65,153,45,15,176,84,187,22
DB	99,124,119,123,242,107,111,197
DB	48,1,103,43,254,215,171,118
DB	202,130,201,125,250,89,71,240
DB	173,212,162,175,156,164,114,192
DB	183,253,147,38,54,63,247,204
DB	52,165,229,241,113,216,49,21
DB	4,199,35,195,24,150,5,154
DB	7,18,128,226,235,39,178,117
DB	9,131,44,26,27,110,90,160
DB	82,59,214,179,41,227,47,132
DB	83,209,0,237,32,252,177,91
DB	106,203,190,57,74,76,88,207
DB	208,239,170,251,67,77,51,133
DB	69,249,2,127,80,60,159,168
DB	81,163,64,143,146,157,56,245
DB	188,182,218,33,16,255,243,210
DB	205,12,19,236,95,151,68,23
DB	196,167,126,61,100,93,25,115
DB	96,129,79,220,34,42,144,136
DB	70,238,184,20,222,94,11,219
DB	224,50,58,10,73,6,36,92
DB	194,211,172,98,145,149,228,121
DB	231,200,55,109,141,213,78,169
DB	108,86,244,234,101,122,174,8
DB	186,120,37,46,28,166,180,198
DB	232,221,116,31,75,189,139,138
DB	112,62,181,102,72,3,246,14
DB	97,53,87,185,134,193,29,158
DB	225,248,152,17,105,217,142,148
DB	155,30,135,233,206,85,40,223
DB	140,161,137,13,191,230,66,104
DB	65,153,45,15,176,84,187,22
DB	99,124,119,123,242,107,111,197
DB	48,1,103,43,254,215,171,118
DB	202,130,201,125,250,89,71,240
DB	173,212,162,175,156,164,114,192
DB	183,253,147,38,54,63,247,204
DB	52,165,229,241,113,216,49,21
DB	4,199,35,195,24,150,5,154
DB	7,18,128,226,235,39,178,117
DB	9,131,44,26,27,110,90,160
DB	82,59,214,179,41,227,47,132
DB	83,209,0,237,32,252,177,91
DB	106,203,190,57,74,76,88,207
DB	208,239,170,251,67,77,51,133
DB	69,249,2,127,80,60,159,168
DB	81,163,64,143,146,157,56,245
DB	188,182,218,33,16,255,243,210
DB	205,12,19,236,95,151,68,23
DB	196,167,126,61,100,93,25,115
DB	96,129,79,220,34,42,144,136
DB	70,238,184,20,222,94,11,219
DB	224,50,58,10,73,6,36,92
DB	194,211,172,98,145,149,228,121
DB	231,200,55,109,141,213,78,169
DB	108,86,244,234,101,122,174,8
DB	186,120,37,46,28,166,180,198
DB	232,221,116,31,75,189,139,138
DB	112,62,181,102,72,3,246,14
DB	97,53,87,185,134,193,29,158
DB	225,248,152,17,105,217,142,148
DB	155,30,135,233,206,85,40,223
DB	140,161,137,13,191,230,66,104
DB	65,153,45,15,176,84,187,22
DB	99,124,119,123,242,107,111,197
DB	48,1,103,43,254,215,171,118
DB	202,130,201,125,250,89,71,240
DB	173,212,162,175,156,164,114,192
DB	183,253,147,38,54,63,247,204
DB	52,165,229,241,113,216,49,21
DB	4,199,35,195,24,150,5,154
DB	7,18,128,226,235,39,178,117
DB	9,131,44,26,27,110,90,160
DB	82,59,214,179,41,227,47,132
DB	83,209,0,237,32,252,177,91
DB	106,203,190,57,74,76,88,207
DB	208,239,170,251,67,77,51,133
DB	69,249,2,127,80,60,159,168
DB	81,163,64,143,146,157,56,245
DB	188,182,218,33,16,255,243,210
DB	205,12,19,236,95,151,68,23
DB	196,167,126,61,100,93,25,115
DB	96,129,79,220,34,42,144,136
DB	70,238,184,20,222,94,11,219
DB	224,50,58,10,73,6,36,92
DB	194,211,172,98,145,149,228,121
DB	231,200,55,109,141,213,78,169
DB	108,86,244,234,101,122,174,8
DB	186,120,37,46,28,166,180,198
DB	232,221,116,31,75,189,139,138
DB	112,62,181,102,72,3,246,14
DB	97,53,87,185,134,193,29,158
DB	225,248,152,17,105,217,142,148
DB	155,30,135,233,206,85,40,223
DB	140,161,137,13,191,230,66,104
DB	65,153,45,15,176,84,187,22
DD	1,2,4,8
DD	16,32,64,128
DD	27,54,0,0
DD	0,0,0,0
__x86_AES_encrypt ENDP
ALIGN	16
_AES_encrypt	PROC PUBLIC
$L_AES_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	eax,esp
	sub	esp,36
	and	esp,-64
	lea	ebx,DWORD PTR [edi-127]
	sub	ebx,esp
	neg	ebx
	and	ebx,960
	sub	esp,ebx
	add	esp,4
	mov	DWORD PTR 28[esp],eax
	call	$L004pic_point
$L004pic_point:
	pop	ebp
	lea	eax,DWORD PTR _OPENSSL_ia32cap_P
	lea	ebp,DWORD PTR ($LAES_Te-$L004pic_point)[ebp]
	lea	ebx,DWORD PTR 764[esp]
	sub	ebx,ebp
	and	ebx,768
	lea	ebp,DWORD PTR 2176[ebx*1+ebp]
	bt	DWORD PTR [eax],25
	jnc	$L005x86
	movq	mm0,QWORD PTR [esi]
	movq	mm4,QWORD PTR 8[esi]
	call	__sse_AES_encrypt_compact
	mov	esp,DWORD PTR 28[esp]
	mov	esi,DWORD PTR 24[esp]
	movq	QWORD PTR [esi],mm0
	movq	QWORD PTR 8[esi],mm4
	emms
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	16
$L005x86:
	mov	DWORD PTR 24[esp],ebp
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	call	__x86_AES_encrypt_compact
	mov	esp,DWORD PTR 28[esp]
	mov	esi,DWORD PTR 24[esp]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_AES_encrypt ENDP
ALIGN	16
__x86_AES_decrypt_compact	PROC PRIVATE
	mov	DWORD PTR 20[esp],edi
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	esi,DWORD PTR 240[edi]
	lea	esi,DWORD PTR [esi*1+esi-2]
	lea	esi,DWORD PTR [esi*8+edi]
	mov	DWORD PTR 24[esp],esi
	mov	edi,DWORD PTR [ebp-128]
	mov	esi,DWORD PTR [ebp-96]
	mov	edi,DWORD PTR [ebp-64]
	mov	esi,DWORD PTR [ebp-32]
	mov	edi,DWORD PTR [ebp]
	mov	esi,DWORD PTR 32[ebp]
	mov	edi,DWORD PTR 64[ebp]
	mov	esi,DWORD PTR 96[ebp]
ALIGN	16
$L006loop:
	mov	esi,eax
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,dh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,ecx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,ebx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 4[esp],esi
	mov	esi,ebx
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,ah
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,edx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,ecx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 8[esp],esi
	mov	esi,ecx
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,bh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,eax
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,edx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	and	edx,255
	movzx	edx,BYTE PTR [edx*1+ebp-128]
	movzx	ecx,ch
	movzx	ecx,BYTE PTR [ecx*1+ebp-128]
	shl	ecx,8
	xor	edx,ecx
	mov	ecx,esi
	shr	ebx,16
	and	ebx,255
	movzx	ebx,BYTE PTR [ebx*1+ebp-128]
	shl	ebx,16
	xor	edx,ebx
	shr	eax,24
	movzx	eax,BYTE PTR [eax*1+ebp-128]
	shl	eax,24
	xor	edx,eax
	mov	edi,2155905152
	and	edi,ecx
	mov	esi,edi
	shr	edi,7
	lea	eax,DWORD PTR [ecx*1+ecx]
	sub	esi,edi
	and	eax,4278124286
	and	esi,454761243
	xor	eax,esi
	mov	edi,2155905152
	and	edi,eax
	mov	esi,edi
	shr	edi,7
	lea	ebx,DWORD PTR [eax*1+eax]
	sub	esi,edi
	and	ebx,4278124286
	and	esi,454761243
	xor	eax,ecx
	xor	ebx,esi
	mov	edi,2155905152
	and	edi,ebx
	mov	esi,edi
	shr	edi,7
	lea	ebp,DWORD PTR [ebx*1+ebx]
	sub	esi,edi
	and	ebp,4278124286
	and	esi,454761243
	xor	ebx,ecx
	rol	ecx,8
	xor	ebp,esi
	xor	ecx,eax
	xor	eax,ebp
	xor	ecx,ebx
	xor	ebx,ebp
	rol	eax,24
	xor	ecx,ebp
	rol	ebx,16
	xor	ecx,eax
	rol	ebp,8
	xor	ecx,ebx
	mov	eax,DWORD PTR 4[esp]
	xor	ecx,ebp
	mov	DWORD PTR 12[esp],ecx
	mov	edi,2155905152
	and	edi,edx
	mov	esi,edi
	shr	edi,7
	lea	ebx,DWORD PTR [edx*1+edx]
	sub	esi,edi
	and	ebx,4278124286
	and	esi,454761243
	xor	ebx,esi
	mov	edi,2155905152
	and	edi,ebx
	mov	esi,edi
	shr	edi,7
	lea	ecx,DWORD PTR [ebx*1+ebx]
	sub	esi,edi
	and	ecx,4278124286
	and	esi,454761243
	xor	ebx,edx
	xor	ecx,esi
	mov	edi,2155905152
	and	edi,ecx
	mov	esi,edi
	shr	edi,7
	lea	ebp,DWORD PTR [ecx*1+ecx]
	sub	esi,edi
	and	ebp,4278124286
	and	esi,454761243
	xor	ecx,edx
	rol	edx,8
	xor	ebp,esi
	xor	edx,ebx
	xor	ebx,ebp
	xor	edx,ecx
	xor	ecx,ebp
	rol	ebx,24
	xor	edx,ebp
	rol	ecx,16
	xor	edx,ebx
	rol	ebp,8
	xor	edx,ecx
	mov	ebx,DWORD PTR 8[esp]
	xor	edx,ebp
	mov	DWORD PTR 16[esp],edx
	mov	edi,2155905152
	and	edi,eax
	mov	esi,edi
	shr	edi,7
	lea	ecx,DWORD PTR [eax*1+eax]
	sub	esi,edi
	and	ecx,4278124286
	and	esi,454761243
	xor	ecx,esi
	mov	edi,2155905152
	and	edi,ecx
	mov	esi,edi
	shr	edi,7
	lea	edx,DWORD PTR [ecx*1+ecx]
	sub	esi,edi
	and	edx,4278124286
	and	esi,454761243
	xor	ecx,eax
	xor	edx,esi
	mov	edi,2155905152
	and	edi,edx
	mov	esi,edi
	shr	edi,7
	lea	ebp,DWORD PTR [edx*1+edx]
	sub	esi,edi
	and	ebp,4278124286
	and	esi,454761243
	xor	edx,eax
	rol	eax,8
	xor	ebp,esi
	xor	eax,ecx
	xor	ecx,ebp
	xor	eax,edx
	xor	edx,ebp
	rol	ecx,24
	xor	eax,ebp
	rol	edx,16
	xor	eax,ecx
	rol	ebp,8
	xor	eax,edx
	xor	eax,ebp
	mov	edi,2155905152
	and	edi,ebx
	mov	esi,edi
	shr	edi,7
	lea	ecx,DWORD PTR [ebx*1+ebx]
	sub	esi,edi
	and	ecx,4278124286
	and	esi,454761243
	xor	ecx,esi
	mov	edi,2155905152
	and	edi,ecx
	mov	esi,edi
	shr	edi,7
	lea	edx,DWORD PTR [ecx*1+ecx]
	sub	esi,edi
	and	edx,4278124286
	and	esi,454761243
	xor	ecx,ebx
	xor	edx,esi
	mov	edi,2155905152
	and	edi,edx
	mov	esi,edi
	shr	edi,7
	lea	ebp,DWORD PTR [edx*1+edx]
	sub	esi,edi
	and	ebp,4278124286
	and	esi,454761243
	xor	edx,ebx
	rol	ebx,8
	xor	ebp,esi
	xor	ebx,ecx
	xor	ecx,ebp
	xor	ebx,edx
	xor	edx,ebp
	rol	ecx,24
	xor	ebx,ebp
	rol	edx,16
	xor	ebx,ecx
	rol	ebp,8
	xor	ebx,edx
	mov	ecx,DWORD PTR 12[esp]
	xor	ebx,ebp
	mov	edx,DWORD PTR 16[esp]
	mov	edi,DWORD PTR 20[esp]
	mov	ebp,DWORD PTR 28[esp]
	add	edi,16
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	cmp	edi,DWORD PTR 24[esp]
	mov	DWORD PTR 20[esp],edi
	jb	$L006loop
	mov	esi,eax
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,dh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,ecx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,ebx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 4[esp],esi
	mov	esi,ebx
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,ah
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,edx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,ecx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 8[esp],esi
	mov	esi,ecx
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp-128]
	movzx	edi,bh
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,8
	xor	esi,edi
	mov	edi,eax
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,16
	xor	esi,edi
	mov	edi,edx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp-128]
	shl	edi,24
	xor	esi,edi
	mov	edi,DWORD PTR 20[esp]
	and	edx,255
	movzx	edx,BYTE PTR [edx*1+ebp-128]
	movzx	ecx,ch
	movzx	ecx,BYTE PTR [ecx*1+ebp-128]
	shl	ecx,8
	xor	edx,ecx
	mov	ecx,esi
	shr	ebx,16
	and	ebx,255
	movzx	ebx,BYTE PTR [ebx*1+ebp-128]
	shl	ebx,16
	xor	edx,ebx
	mov	ebx,DWORD PTR 8[esp]
	shr	eax,24
	movzx	eax,BYTE PTR [eax*1+ebp-128]
	shl	eax,24
	xor	edx,eax
	mov	eax,DWORD PTR 4[esp]
	xor	eax,DWORD PTR 16[edi]
	xor	ebx,DWORD PTR 20[edi]
	xor	ecx,DWORD PTR 24[edi]
	xor	edx,DWORD PTR 28[edi]
	ret
__x86_AES_decrypt_compact ENDP
ALIGN	16
__sse_AES_decrypt_compact	PROC PRIVATE
	pxor	mm0,QWORD PTR [edi]
	pxor	mm4,QWORD PTR 8[edi]
	mov	esi,DWORD PTR 240[edi]
	lea	esi,DWORD PTR [esi*1+esi-2]
	lea	esi,DWORD PTR [esi*8+edi]
	mov	DWORD PTR 24[esp],esi
	mov	eax,454761243
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],eax
	mov	eax,DWORD PTR [ebp-128]
	mov	ebx,DWORD PTR [ebp-96]
	mov	ecx,DWORD PTR [ebp-64]
	mov	edx,DWORD PTR [ebp-32]
	mov	eax,DWORD PTR [ebp]
	mov	ebx,DWORD PTR 32[ebp]
	mov	ecx,DWORD PTR 64[ebp]
	mov	edx,DWORD PTR 96[ebp]
ALIGN	16
$L007loop:
	pshufw	mm1,mm0,12
	pshufw	mm5,mm4,9
	movd	eax,mm1
	movd	ebx,mm5
	mov	DWORD PTR 20[esp],edi
	movzx	esi,al
	movzx	edx,ah
	pshufw	mm2,mm0,6
	movzx	ecx,BYTE PTR [esi*1+ebp-128]
	movzx	edi,bl
	movzx	edx,BYTE PTR [edx*1+ebp-128]
	shr	eax,16
	shl	edx,8
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bh
	shl	esi,16
	pshufw	mm6,mm4,3
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,ah
	shl	esi,24
	shr	ebx,16
	or	edx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bh
	shl	esi,24
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,al
	shl	esi,8
	movd	eax,mm2
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bl
	shl	esi,16
	movd	ebx,mm6
	movd	mm0,ecx
	movzx	ecx,BYTE PTR [edi*1+ebp-128]
	movzx	edi,al
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bl
	or	edx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,ah
	shl	esi,16
	shr	eax,16
	or	edx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bh
	shr	ebx,16
	shl	esi,8
	movd	mm1,edx
	movzx	edx,BYTE PTR [edi*1+ebp-128]
	movzx	edi,bh
	shl	edx,24
	and	ebx,255
	or	edx,esi
	punpckldq	mm0,mm1
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	movzx	edi,al
	shl	esi,8
	movzx	eax,ah
	movzx	ebx,BYTE PTR [ebx*1+ebp-128]
	or	ecx,esi
	movzx	esi,BYTE PTR [edi*1+ebp-128]
	or	edx,ebx
	shl	esi,16
	movzx	eax,BYTE PTR [eax*1+ebp-128]
	or	edx,esi
	shl	eax,24
	or	ecx,eax
	mov	edi,DWORD PTR 20[esp]
	movd	mm4,edx
	movd	mm5,ecx
	punpckldq	mm4,mm5
	add	edi,16
	cmp	edi,DWORD PTR 24[esp]
	ja	$L008out
	movq	mm3,mm0
	movq	mm7,mm4
	pshufw	mm2,mm0,228
	pshufw	mm6,mm4,228
	movq	mm1,mm0
	movq	mm5,mm4
	pshufw	mm0,mm0,177
	pshufw	mm4,mm4,177
	pslld	mm2,8
	pslld	mm6,8
	psrld	mm3,8
	psrld	mm7,8
	pxor	mm0,mm2
	pxor	mm4,mm6
	pxor	mm0,mm3
	pxor	mm4,mm7
	pslld	mm2,16
	pslld	mm6,16
	psrld	mm3,16
	psrld	mm7,16
	pxor	mm0,mm2
	pxor	mm4,mm6
	pxor	mm0,mm3
	pxor	mm4,mm7
	movq	mm3,QWORD PTR 8[esp]
	pxor	mm2,mm2
	pxor	mm6,mm6
	pcmpgtb	mm2,mm1
	pcmpgtb	mm6,mm5
	pand	mm2,mm3
	pand	mm6,mm3
	paddb	mm1,mm1
	paddb	mm5,mm5
	pxor	mm1,mm2
	pxor	mm5,mm6
	movq	mm3,mm1
	movq	mm7,mm5
	movq	mm2,mm1
	movq	mm6,mm5
	pxor	mm0,mm1
	pxor	mm4,mm5
	pslld	mm3,24
	pslld	mm7,24
	psrld	mm2,8
	psrld	mm6,8
	pxor	mm0,mm3
	pxor	mm4,mm7
	pxor	mm0,mm2
	pxor	mm4,mm6
	movq	mm2,QWORD PTR 8[esp]
	pxor	mm3,mm3
	pxor	mm7,mm7
	pcmpgtb	mm3,mm1
	pcmpgtb	mm7,mm5
	pand	mm3,mm2
	pand	mm7,mm2
	paddb	mm1,mm1
	paddb	mm5,mm5
	pxor	mm1,mm3
	pxor	mm5,mm7
	pshufw	mm3,mm1,177
	pshufw	mm7,mm5,177
	pxor	mm0,mm1
	pxor	mm4,mm5
	pxor	mm0,mm3
	pxor	mm4,mm7
	pxor	mm3,mm3
	pxor	mm7,mm7
	pcmpgtb	mm3,mm1
	pcmpgtb	mm7,mm5
	pand	mm3,mm2
	pand	mm7,mm2
	paddb	mm1,mm1
	paddb	mm5,mm5
	pxor	mm1,mm3
	pxor	mm5,mm7
	pxor	mm0,mm1
	pxor	mm4,mm5
	movq	mm3,mm1
	movq	mm7,mm5
	pshufw	mm2,mm1,177
	pshufw	mm6,mm5,177
	pxor	mm0,mm2
	pxor	mm4,mm6
	pslld	mm1,8
	pslld	mm5,8
	psrld	mm3,8
	psrld	mm7,8
	movq	mm2,QWORD PTR [edi]
	movq	mm6,QWORD PTR 8[edi]
	pxor	mm0,mm1
	pxor	mm4,mm5
	pxor	mm0,mm3
	pxor	mm4,mm7
	mov	eax,DWORD PTR [ebp-128]
	pslld	mm1,16
	pslld	mm5,16
	mov	ebx,DWORD PTR [ebp-64]
	psrld	mm3,16
	psrld	mm7,16
	mov	ecx,DWORD PTR [ebp]
	pxor	mm0,mm1
	pxor	mm4,mm5
	mov	edx,DWORD PTR 64[ebp]
	pxor	mm0,mm3
	pxor	mm4,mm7
	pxor	mm0,mm2
	pxor	mm4,mm6
	jmp	$L007loop
ALIGN	16
$L008out:
	pxor	mm0,QWORD PTR [edi]
	pxor	mm4,QWORD PTR 8[edi]
	ret
__sse_AES_decrypt_compact ENDP
ALIGN	16
__x86_AES_decrypt	PROC PRIVATE
	mov	DWORD PTR 20[esp],edi
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	esi,DWORD PTR 240[edi]
	lea	esi,DWORD PTR [esi*1+esi-2]
	lea	esi,DWORD PTR [esi*8+edi]
	mov	DWORD PTR 24[esp],esi
ALIGN	16
$L009loop:
	mov	esi,eax
	and	esi,255
	mov	esi,DWORD PTR [esi*8+ebp]
	movzx	edi,dh
	xor	esi,DWORD PTR 3[edi*8+ebp]
	mov	edi,ecx
	shr	edi,16
	and	edi,255
	xor	esi,DWORD PTR 2[edi*8+ebp]
	mov	edi,ebx
	shr	edi,24
	xor	esi,DWORD PTR 1[edi*8+ebp]
	mov	DWORD PTR 4[esp],esi
	mov	esi,ebx
	and	esi,255
	mov	esi,DWORD PTR [esi*8+ebp]
	movzx	edi,ah
	xor	esi,DWORD PTR 3[edi*8+ebp]
	mov	edi,edx
	shr	edi,16
	and	edi,255
	xor	esi,DWORD PTR 2[edi*8+ebp]
	mov	edi,ecx
	shr	edi,24
	xor	esi,DWORD PTR 1[edi*8+ebp]
	mov	DWORD PTR 8[esp],esi
	mov	esi,ecx
	and	esi,255
	mov	esi,DWORD PTR [esi*8+ebp]
	movzx	edi,bh
	xor	esi,DWORD PTR 3[edi*8+ebp]
	mov	edi,eax
	shr	edi,16
	and	edi,255
	xor	esi,DWORD PTR 2[edi*8+ebp]
	mov	edi,edx
	shr	edi,24
	xor	esi,DWORD PTR 1[edi*8+ebp]
	mov	edi,DWORD PTR 20[esp]
	and	edx,255
	mov	edx,DWORD PTR [edx*8+ebp]
	movzx	ecx,ch
	xor	edx,DWORD PTR 3[ecx*8+ebp]
	mov	ecx,esi
	shr	ebx,16
	and	ebx,255
	xor	edx,DWORD PTR 2[ebx*8+ebp]
	mov	ebx,DWORD PTR 8[esp]
	shr	eax,24
	xor	edx,DWORD PTR 1[eax*8+ebp]
	mov	eax,DWORD PTR 4[esp]
	add	edi,16
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	cmp	edi,DWORD PTR 24[esp]
	mov	DWORD PTR 20[esp],edi
	jb	$L009loop
	lea	ebp,DWORD PTR 2176[ebp]
	mov	edi,DWORD PTR [ebp-128]
	mov	esi,DWORD PTR [ebp-96]
	mov	edi,DWORD PTR [ebp-64]
	mov	esi,DWORD PTR [ebp-32]
	mov	edi,DWORD PTR [ebp]
	mov	esi,DWORD PTR 32[ebp]
	mov	edi,DWORD PTR 64[ebp]
	mov	esi,DWORD PTR 96[ebp]
	lea	ebp,DWORD PTR [ebp-128]
	mov	esi,eax
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp]
	movzx	edi,dh
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,8
	xor	esi,edi
	mov	edi,ecx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,16
	xor	esi,edi
	mov	edi,ebx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 4[esp],esi
	mov	esi,ebx
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp]
	movzx	edi,ah
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,8
	xor	esi,edi
	mov	edi,edx
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,16
	xor	esi,edi
	mov	edi,ecx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,24
	xor	esi,edi
	mov	DWORD PTR 8[esp],esi
	mov	esi,ecx
	and	esi,255
	movzx	esi,BYTE PTR [esi*1+ebp]
	movzx	edi,bh
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,8
	xor	esi,edi
	mov	edi,eax
	shr	edi,16
	and	edi,255
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,16
	xor	esi,edi
	mov	edi,edx
	shr	edi,24
	movzx	edi,BYTE PTR [edi*1+ebp]
	shl	edi,24
	xor	esi,edi
	mov	edi,DWORD PTR 20[esp]
	and	edx,255
	movzx	edx,BYTE PTR [edx*1+ebp]
	movzx	ecx,ch
	movzx	ecx,BYTE PTR [ecx*1+ebp]
	shl	ecx,8
	xor	edx,ecx
	mov	ecx,esi
	shr	ebx,16
	and	ebx,255
	movzx	ebx,BYTE PTR [ebx*1+ebp]
	shl	ebx,16
	xor	edx,ebx
	mov	ebx,DWORD PTR 8[esp]
	shr	eax,24
	movzx	eax,BYTE PTR [eax*1+ebp]
	shl	eax,24
	xor	edx,eax
	mov	eax,DWORD PTR 4[esp]
	lea	ebp,DWORD PTR [ebp-2048]
	add	edi,16
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	ret
ALIGN	64
$LAES_Td::
DD	1353184337,1353184337
DD	1399144830,1399144830
DD	3282310938,3282310938
DD	2522752826,2522752826
DD	3412831035,3412831035
DD	4047871263,4047871263
DD	2874735276,2874735276
DD	2466505547,2466505547
DD	1442459680,1442459680
DD	4134368941,4134368941
DD	2440481928,2440481928
DD	625738485,625738485
DD	4242007375,4242007375
DD	3620416197,3620416197
DD	2151953702,2151953702
DD	2409849525,2409849525
DD	1230680542,1230680542
DD	1729870373,1729870373
DD	2551114309,2551114309
DD	3787521629,3787521629
DD	41234371,41234371
DD	317738113,317738113
DD	2744600205,2744600205
DD	3338261355,3338261355
DD	3881799427,3881799427
DD	2510066197,2510066197
DD	3950669247,3950669247
DD	3663286933,3663286933
DD	763608788,763608788
DD	3542185048,3542185048
DD	694804553,694804553
DD	1154009486,1154009486
DD	1787413109,1787413109
DD	2021232372,2021232372
DD	1799248025,1799248025
DD	3715217703,3715217703
DD	3058688446,3058688446
DD	397248752,397248752
DD	1722556617,1722556617
DD	3023752829,3023752829
DD	407560035,407560035
DD	2184256229,2184256229
DD	1613975959,1613975959
DD	1165972322,1165972322
DD	3765920945,3765920945
DD	2226023355,2226023355
DD	480281086,480281086
DD	2485848313,2485848313
DD	1483229296,1483229296
DD	436028815,436028815
DD	2272059028,2272059028
DD	3086515026,3086515026
DD	601060267,601060267
DD	3791801202,3791801202
DD	1468997603,1468997603
DD	715871590,715871590
DD	120122290,120122290
DD	63092015,63092015
DD	2591802758,2591802758
DD	2768779219,2768779219
DD	4068943920,4068943920
DD	2997206819,2997206819
DD	3127509762,3127509762
DD	1552029421,1552029421
DD	723308426,723308426
DD	2461301159,2461301159
DD	4042393587,4042393587
DD	2715969870,2715969870
DD	3455375973,3455375973
DD	3586000134,3586000134
DD	526529745,526529745
DD	2331944644,2331944644
DD	2639474228,2639474228
DD	2689987490,2689987490
DD	853641733,853641733
DD	1978398372,1978398372
DD	971801355,971801355
DD	2867814464,2867814464
DD	111112542,111112542
DD	1360031421,1360031421
DD	4186579262,4186579262
DD	1023860118,1023860118
DD	2919579357,2919579357
DD	1186850381,1186850381
DD	3045938321,3045938321
DD	90031217,90031217
DD	1876166148,1876166148
DD	4279586912,4279586912
DD	620468249,620468249
DD	2548678102,2548678102
DD	3426959497,3426959497
DD	2006899047,2006899047
DD	3175278768,3175278768
DD	2290845959,2290845959
DD	945494503,945494503
DD	3689859193,3689859193
DD	1191869601,1191869601
DD	3910091388,3910091388
DD	3374220536,3374220536
DD	0,0
DD	2206629897,2206629897
DD	1223502642,1223502642
DD	2893025566,2893025566
DD	1316117100,1316117100
DD	4227796733,4227796733
DD	1446544655,1446544655
DD	517320253,517320253
DD	658058550,658058550
DD	1691946762,1691946762
DD	564550760,564550760
DD	3511966619,3511966619
DD	976107044,976107044
DD	2976320012,2976320012
DD	266819475,266819475
DD	3533106868,3533106868
DD	2660342555,2660342555
DD	1338359936,1338359936
DD	2720062561,2720062561
DD	1766553434,1766553434
DD	370807324,370807324
DD	179999714,179999714
DD	3844776128,3844776128
DD	1138762300,1138762300
DD	488053522,488053522
DD	185403662,185403662
DD	2915535858,2915535858
DD	3114841645,3114841645
DD	3366526484,3366526484
DD	2233069911,2233069911
DD	1275557295,1275557295
DD	3151862254,3151862254
DD	4250959779,4250959779
DD	2670068215,2670068215
DD	3170202204,3170202204
DD	3309004356,3309004356
DD	880737115,880737115
DD	1982415755,1982415755
DD	3703972811,3703972811
DD	1761406390,1761406390
DD	1676797112,1676797112
DD	3403428311,3403428311
DD	277177154,277177154
DD	1076008723,1076008723
DD	538035844,538035844
DD	2099530373,2099530373
DD	4164795346,4164795346
DD	288553390,288553390
DD	1839278535,1839278535
DD	1261411869,1261411869
DD	4080055004,4080055004
DD	3964831245,3964831245
DD	3504587127,3504587127
DD	1813426987,1813426987
DD	2579067049,2579067049
DD	4199060497,4199060497
DD	577038663,577038663
DD	3297574056,3297574056
DD	440397984,440397984
DD	3626794326,3626794326
DD	4019204898,4019204898
DD	3343796615,3343796615
DD	3251714265,3251714265
DD	4272081548,4272081548
DD	906744984,906744984
DD	3481400742,3481400742
DD	685669029,685669029
DD	646887386,646887386
DD	2764025151,2764025151
DD	3835509292,3835509292
DD	227702864,227702864
DD	2613862250,2613862250
DD	1648787028,1648787028
DD	3256061430,3256061430
DD	3904428176,3904428176
DD	1593260334,1593260334
DD	4121936770,4121936770
DD	3196083615,3196083615
DD	2090061929,2090061929
DD	2838353263,2838353263
DD	3004310991,3004310991
DD	999926984,999926984
DD	2809993232,2809993232
DD	1852021992,1852021992
DD	2075868123,2075868123
DD	158869197,158869197
DD	4095236462,4095236462
DD	28809964,28809964
DD	2828685187,2828685187
DD	1701746150,1701746150
DD	2129067946,2129067946
DD	147831841,147831841
DD	3873969647,3873969647
DD	3650873274,3650873274
DD	3459673930,3459673930
DD	3557400554,3557400554
DD	3598495785,3598495785
DD	2947720241,2947720241
DD	824393514,824393514
DD	815048134,815048134
DD	3227951669,3227951669
DD	935087732,935087732
DD	2798289660,2798289660
DD	2966458592,2966458592
DD	366520115,366520115
DD	1251476721,1251476721
DD	4158319681,4158319681
DD	240176511,240176511
DD	804688151,804688151
DD	2379631990,2379631990
DD	1303441219,1303441219
DD	1414376140,1414376140
DD	3741619940,3741619940
DD	3820343710,3820343710
DD	461924940,461924940
DD	3089050817,3089050817
DD	2136040774,2136040774
DD	82468509,82468509
DD	1563790337,1563790337
DD	1937016826,1937016826
DD	776014843,776014843
DD	1511876531,1511876531
DD	1389550482,1389550482
DD	861278441,861278441
DD	323475053,323475053
DD	2355222426,2355222426
DD	2047648055,2047648055
DD	2383738969,2383738969
DD	2302415851,2302415851
DD	3995576782,3995576782
DD	902390199,902390199
DD	3991215329,3991215329
DD	1018251130,1018251130
DD	1507840668,1507840668
DD	1064563285,1064563285
DD	2043548696,2043548696
DD	3208103795,3208103795
DD	3939366739,3939366739
DD	1537932639,1537932639
DD	342834655,342834655
DD	2262516856,2262516856
DD	2180231114,2180231114
DD	1053059257,1053059257
DD	741614648,741614648
DD	1598071746,1598071746
DD	1925389590,1925389590
DD	203809468,203809468
DD	2336832552,2336832552
DD	1100287487,1100287487
DD	1895934009,1895934009
DD	3736275976,3736275976
DD	2632234200,2632234200
DD	2428589668,2428589668
DD	1636092795,1636092795
DD	1890988757,1890988757
DD	1952214088,1952214088
DD	1113045200,1113045200
DB	82,9,106,213,48,54,165,56
DB	191,64,163,158,129,243,215,251
DB	124,227,57,130,155,47,255,135
DB	52,142,67,68,196,222,233,203
DB	84,123,148,50,166,194,35,61
DB	238,76,149,11,66,250,195,78
DB	8,46,161,102,40,217,36,178
DB	118,91,162,73,109,139,209,37
DB	114,248,246,100,134,104,152,22
DB	212,164,92,204,93,101,182,146
DB	108,112,72,80,253,237,185,218
DB	94,21,70,87,167,141,157,132
DB	144,216,171,0,140,188,211,10
DB	247,228,88,5,184,179,69,6
DB	208,44,30,143,202,63,15,2
DB	193,175,189,3,1,19,138,107
DB	58,145,17,65,79,103,220,234
DB	151,242,207,206,240,180,230,115
DB	150,172,116,34,231,173,53,133
DB	226,249,55,232,28,117,223,110
DB	71,241,26,113,29,41,197,137
DB	111,183,98,14,170,24,190,27
DB	252,86,62,75,198,210,121,32
DB	154,219,192,254,120,205,90,244
DB	31,221,168,51,136,7,199,49
DB	177,18,16,89,39,128,236,95
DB	96,81,127,169,25,181,74,13
DB	45,229,122,159,147,201,156,239
DB	160,224,59,77,174,42,245,176
DB	200,235,187,60,131,83,153,97
DB	23,43,4,126,186,119,214,38
DB	225,105,20,99,85,33,12,125
DB	82,9,106,213,48,54,165,56
DB	191,64,163,158,129,243,215,251
DB	124,227,57,130,155,47,255,135
DB	52,142,67,68,196,222,233,203
DB	84,123,148,50,166,194,35,61
DB	238,76,149,11,66,250,195,78
DB	8,46,161,102,40,217,36,178
DB	118,91,162,73,109,139,209,37
DB	114,248,246,100,134,104,152,22
DB	212,164,92,204,93,101,182,146
DB	108,112,72,80,253,237,185,218
DB	94,21,70,87,167,141,157,132
DB	144,216,171,0,140,188,211,10
DB	247,228,88,5,184,179,69,6
DB	208,44,30,143,202,63,15,2
DB	193,175,189,3,1,19,138,107
DB	58,145,17,65,79,103,220,234
DB	151,242,207,206,240,180,230,115
DB	150,172,116,34,231,173,53,133
DB	226,249,55,232,28,117,223,110
DB	71,241,26,113,29,41,197,137
DB	111,183,98,14,170,24,190,27
DB	252,86,62,75,198,210,121,32
DB	154,219,192,254,120,205,90,244
DB	31,221,168,51,136,7,199,49
DB	177,18,16,89,39,128,236,95
DB	96,81,127,169,25,181,74,13
DB	45,229,122,159,147,201,156,239
DB	160,224,59,77,174,42,245,176
DB	200,235,187,60,131,83,153,97
DB	23,43,4,126,186,119,214,38
DB	225,105,20,99,85,33,12,125
DB	82,9,106,213,48,54,165,56
DB	191,64,163,158,129,243,215,251
DB	124,227,57,130,155,47,255,135
DB	52,142,67,68,196,222,233,203
DB	84,123,148,50,166,194,35,61
DB	238,76,149,11,66,250,195,78
DB	8,46,161,102,40,217,36,178
DB	118,91,162,73,109,139,209,37
DB	114,248,246,100,134,104,152,22
DB	212,164,92,204,93,101,182,146
DB	108,112,72,80,253,237,185,218
DB	94,21,70,87,167,141,157,132
DB	144,216,171,0,140,188,211,10
DB	247,228,88,5,184,179,69,6
DB	208,44,30,143,202,63,15,2
DB	193,175,189,3,1,19,138,107
DB	58,145,17,65,79,103,220,234
DB	151,242,207,206,240,180,230,115
DB	150,172,116,34,231,173,53,133
DB	226,249,55,232,28,117,223,110
DB	71,241,26,113,29,41,197,137
DB	111,183,98,14,170,24,190,27
DB	252,86,62,75,198,210,121,32
DB	154,219,192,254,120,205,90,244
DB	31,221,168,51,136,7,199,49
DB	177,18,16,89,39,128,236,95
DB	96,81,127,169,25,181,74,13
DB	45,229,122,159,147,201,156,239
DB	160,224,59,77,174,42,245,176
DB	200,235,187,60,131,83,153,97
DB	23,43,4,126,186,119,214,38
DB	225,105,20,99,85,33,12,125
DB	82,9,106,213,48,54,165,56
DB	191,64,163,158,129,243,215,251
DB	124,227,57,130,155,47,255,135
DB	52,142,67,68,196,222,233,203
DB	84,123,148,50,166,194,35,61
DB	238,76,149,11,66,250,195,78
DB	8,46,161,102,40,217,36,178
DB	118,91,162,73,109,139,209,37
DB	114,248,246,100,134,104,152,22
DB	212,164,92,204,93,101,182,146
DB	108,112,72,80,253,237,185,218
DB	94,21,70,87,167,141,157,132
DB	144,216,171,0,140,188,211,10
DB	247,228,88,5,184,179,69,6
DB	208,44,30,143,202,63,15,2
DB	193,175,189,3,1,19,138,107
DB	58,145,17,65,79,103,220,234
DB	151,242,207,206,240,180,230,115
DB	150,172,116,34,231,173,53,133
DB	226,249,55,232,28,117,223,110
DB	71,241,26,113,29,41,197,137
DB	111,183,98,14,170,24,190,27
DB	252,86,62,75,198,210,121,32
DB	154,219,192,254,120,205,90,244
DB	31,221,168,51,136,7,199,49
DB	177,18,16,89,39,128,236,95
DB	96,81,127,169,25,181,74,13
DB	45,229,122,159,147,201,156,239
DB	160,224,59,77,174,42,245,176
DB	200,235,187,60,131,83,153,97
DB	23,43,4,126,186,119,214,38
DB	225,105,20,99,85,33,12,125
__x86_AES_decrypt ENDP
ALIGN	16
_AES_decrypt	PROC PUBLIC
$L_AES_decrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	eax,esp
	sub	esp,36
	and	esp,-64
	lea	ebx,DWORD PTR [edi-127]
	sub	ebx,esp
	neg	ebx
	and	ebx,960
	sub	esp,ebx
	add	esp,4
	mov	DWORD PTR 28[esp],eax
	call	$L010pic_point
$L010pic_point:
	pop	ebp
	lea	eax,DWORD PTR _OPENSSL_ia32cap_P
	lea	ebp,DWORD PTR ($LAES_Td-$L010pic_point)[ebp]
	lea	ebx,DWORD PTR 764[esp]
	sub	ebx,ebp
	and	ebx,768
	lea	ebp,DWORD PTR 2176[ebx*1+ebp]
	bt	DWORD PTR [eax],25
	jnc	$L011x86
	movq	mm0,QWORD PTR [esi]
	movq	mm4,QWORD PTR 8[esi]
	call	__sse_AES_decrypt_compact
	mov	esp,DWORD PTR 28[esp]
	mov	esi,DWORD PTR 24[esp]
	movq	QWORD PTR [esi],mm0
	movq	QWORD PTR 8[esi],mm4
	emms
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	16
$L011x86:
	mov	DWORD PTR 24[esp],ebp
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	call	__x86_AES_decrypt_compact
	mov	esp,DWORD PTR 28[esp]
	mov	esi,DWORD PTR 24[esp]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_AES_decrypt ENDP
ALIGN	16
_AES_cbc_encrypt	PROC PUBLIC
$L_AES_cbc_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ecx,DWORD PTR 28[esp]
	cmp	ecx,0
	je	$L012drop_out
	call	$L013pic_point
$L013pic_point:
	pop	ebp
	lea	eax,DWORD PTR _OPENSSL_ia32cap_P
	cmp	DWORD PTR 40[esp],0
	lea	ebp,DWORD PTR ($LAES_Te-$L013pic_point)[ebp]
	jne	$L014picked_te
	lea	ebp,DWORD PTR ($LAES_Td-$LAES_Te)[ebp]
$L014picked_te:
	pushfd
	cld
	cmp	ecx,512
	jb	$L015slow_way
	test	ecx,15
	jnz	$L015slow_way
	bt	DWORD PTR [eax],28
	jc	$L015slow_way
	lea	esi,DWORD PTR [esp-324]
	and	esi,-64
	mov	eax,ebp
	lea	ebx,DWORD PTR 2304[ebp]
	mov	edx,esi
	and	eax,4095
	and	ebx,4095
	and	edx,4095
	cmp	edx,ebx
	jb	$L016tbl_break_out
	sub	edx,ebx
	sub	esi,edx
	jmp	$L017tbl_ok
ALIGN	4
$L016tbl_break_out:
	sub	edx,eax
	and	edx,4095
	add	edx,384
	sub	esi,edx
ALIGN	4
$L017tbl_ok:
	lea	edx,DWORD PTR 24[esp]
	xchg	esp,esi
	add	esp,4
	mov	DWORD PTR 24[esp],ebp
	mov	DWORD PTR 28[esp],esi
	mov	eax,DWORD PTR [edx]
	mov	ebx,DWORD PTR 4[edx]
	mov	edi,DWORD PTR 12[edx]
	mov	esi,DWORD PTR 16[edx]
	mov	edx,DWORD PTR 20[edx]
	mov	DWORD PTR 32[esp],eax
	mov	DWORD PTR 36[esp],ebx
	mov	DWORD PTR 40[esp],ecx
	mov	DWORD PTR 44[esp],edi
	mov	DWORD PTR 48[esp],esi
	mov	DWORD PTR 316[esp],0
	mov	ebx,edi
	mov	ecx,61
	sub	ebx,ebp
	mov	esi,edi
	and	ebx,4095
	lea	edi,DWORD PTR 76[esp]
	cmp	ebx,2304
	jb	$L018do_copy
	cmp	ebx,3852
	jb	$L019skip_copy
ALIGN	4
$L018do_copy:
	mov	DWORD PTR 44[esp],edi
DD	2784229001
$L019skip_copy:
	mov	edi,16
ALIGN	4
$L020prefetch_tbl:
	mov	eax,DWORD PTR [ebp]
	mov	ebx,DWORD PTR 32[ebp]
	mov	ecx,DWORD PTR 64[ebp]
	mov	esi,DWORD PTR 96[ebp]
	lea	ebp,DWORD PTR 128[ebp]
	sub	edi,1
	jnz	$L020prefetch_tbl
	sub	ebp,2048
	mov	esi,DWORD PTR 32[esp]
	mov	edi,DWORD PTR 48[esp]
	cmp	edx,0
	je	$L021fast_decrypt
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
ALIGN	16
$L022fast_enc_loop:
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	xor	eax,DWORD PTR [esi]
	xor	ebx,DWORD PTR 4[esi]
	xor	ecx,DWORD PTR 8[esi]
	xor	edx,DWORD PTR 12[esi]
	mov	edi,DWORD PTR 44[esp]
	call	__x86_AES_encrypt
	mov	esi,DWORD PTR 32[esp]
	mov	edi,DWORD PTR 36[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	lea	esi,DWORD PTR 16[esi]
	mov	ecx,DWORD PTR 40[esp]
	mov	DWORD PTR 32[esp],esi
	lea	edx,DWORD PTR 16[edi]
	mov	DWORD PTR 36[esp],edx
	sub	ecx,16
	mov	DWORD PTR 40[esp],ecx
	jnz	$L022fast_enc_loop
	mov	esi,DWORD PTR 48[esp]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	cmp	DWORD PTR 316[esp],0
	mov	edi,DWORD PTR 44[esp]
	je	$L023skip_ezero
	mov	ecx,60
	xor	eax,eax
ALIGN	4
DD	2884892297
$L023skip_ezero:
	mov	esp,DWORD PTR 28[esp]
	popfd
$L012drop_out:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
ALIGN	16
$L021fast_decrypt:
	cmp	esi,DWORD PTR 36[esp]
	je	$L024fast_dec_in_place
	mov	DWORD PTR 52[esp],edi
ALIGN	4
ALIGN	16
$L025fast_dec_loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	edi,DWORD PTR 44[esp]
	call	__x86_AES_decrypt
	mov	edi,DWORD PTR 52[esp]
	mov	esi,DWORD PTR 40[esp]
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	edi,DWORD PTR 36[esp]
	mov	esi,DWORD PTR 32[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	ecx,DWORD PTR 40[esp]
	mov	DWORD PTR 52[esp],esi
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 32[esp],esi
	lea	edi,DWORD PTR 16[edi]
	mov	DWORD PTR 36[esp],edi
	sub	ecx,16
	mov	DWORD PTR 40[esp],ecx
	jnz	$L025fast_dec_loop
	mov	edi,DWORD PTR 52[esp]
	mov	esi,DWORD PTR 48[esp]
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	jmp	$L026fast_dec_out
ALIGN	16
$L024fast_dec_in_place:
$L027fast_dec_in_place_loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	lea	edi,DWORD PTR 60[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	edi,DWORD PTR 44[esp]
	call	__x86_AES_decrypt
	mov	edi,DWORD PTR 48[esp]
	mov	esi,DWORD PTR 36[esp]
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 36[esp],esi
	lea	esi,DWORD PTR 60[esp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	esi,DWORD PTR 32[esp]
	mov	ecx,DWORD PTR 40[esp]
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 32[esp],esi
	sub	ecx,16
	mov	DWORD PTR 40[esp],ecx
	jnz	$L027fast_dec_in_place_loop
ALIGN	4
$L026fast_dec_out:
	cmp	DWORD PTR 316[esp],0
	mov	edi,DWORD PTR 44[esp]
	je	$L028skip_dzero
	mov	ecx,60
	xor	eax,eax
ALIGN	4
DD	2884892297
$L028skip_dzero:
	mov	esp,DWORD PTR 28[esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
ALIGN	16
$L015slow_way:
	mov	eax,DWORD PTR [eax]
	mov	edi,DWORD PTR 36[esp]
	lea	esi,DWORD PTR [esp-80]
	and	esi,-64
	lea	ebx,DWORD PTR [edi-143]
	sub	ebx,esi
	neg	ebx
	and	ebx,960
	sub	esi,ebx
	lea	ebx,DWORD PTR 768[esi]
	sub	ebx,ebp
	and	ebx,768
	lea	ebp,DWORD PTR 2176[ebx*1+ebp]
	lea	edx,DWORD PTR 24[esp]
	xchg	esp,esi
	add	esp,4
	mov	DWORD PTR 24[esp],ebp
	mov	DWORD PTR 28[esp],esi
	mov	DWORD PTR 52[esp],eax
	mov	eax,DWORD PTR [edx]
	mov	ebx,DWORD PTR 4[edx]
	mov	esi,DWORD PTR 16[edx]
	mov	edx,DWORD PTR 20[edx]
	mov	DWORD PTR 32[esp],eax
	mov	DWORD PTR 36[esp],ebx
	mov	DWORD PTR 40[esp],ecx
	mov	DWORD PTR 44[esp],edi
	mov	DWORD PTR 48[esp],esi
	mov	edi,esi
	mov	esi,eax
	cmp	edx,0
	je	$L029slow_decrypt
	cmp	ecx,16
	mov	edx,ebx
	jb	$L030slow_enc_tail
	bt	DWORD PTR 52[esp],25
	jnc	$L031slow_enc_x86
	movq	mm0,QWORD PTR [edi]
	movq	mm4,QWORD PTR 8[edi]
ALIGN	16
$L032slow_enc_loop_sse:
	pxor	mm0,QWORD PTR [esi]
	pxor	mm4,QWORD PTR 8[esi]
	mov	edi,DWORD PTR 44[esp]
	call	__sse_AES_encrypt_compact
	mov	esi,DWORD PTR 32[esp]
	mov	edi,DWORD PTR 36[esp]
	mov	ecx,DWORD PTR 40[esp]
	movq	QWORD PTR [edi],mm0
	movq	QWORD PTR 8[edi],mm4
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 32[esp],esi
	lea	edx,DWORD PTR 16[edi]
	mov	DWORD PTR 36[esp],edx
	sub	ecx,16
	cmp	ecx,16
	mov	DWORD PTR 40[esp],ecx
	jae	$L032slow_enc_loop_sse
	test	ecx,15
	jnz	$L030slow_enc_tail
	mov	esi,DWORD PTR 48[esp]
	movq	QWORD PTR [esi],mm0
	movq	QWORD PTR 8[esi],mm4
	emms
	mov	esp,DWORD PTR 28[esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
ALIGN	16
$L031slow_enc_x86:
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
ALIGN	4
$L033slow_enc_loop_x86:
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	xor	eax,DWORD PTR [esi]
	xor	ebx,DWORD PTR 4[esi]
	xor	ecx,DWORD PTR 8[esi]
	xor	edx,DWORD PTR 12[esi]
	mov	edi,DWORD PTR 44[esp]
	call	__x86_AES_encrypt_compact
	mov	esi,DWORD PTR 32[esp]
	mov	edi,DWORD PTR 36[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	ecx,DWORD PTR 40[esp]
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 32[esp],esi
	lea	edx,DWORD PTR 16[edi]
	mov	DWORD PTR 36[esp],edx
	sub	ecx,16
	cmp	ecx,16
	mov	DWORD PTR 40[esp],ecx
	jae	$L033slow_enc_loop_x86
	test	ecx,15
	jnz	$L030slow_enc_tail
	mov	esi,DWORD PTR 48[esp]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	mov	esp,DWORD PTR 28[esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
ALIGN	16
$L030slow_enc_tail:
	emms
	mov	edi,edx
	mov	ebx,16
	sub	ebx,ecx
	cmp	edi,esi
	je	$L034enc_in_place
ALIGN	4
DD	2767451785
	jmp	$L035enc_skip_in_place
$L034enc_in_place:
	lea	edi,DWORD PTR [ecx*1+edi]
$L035enc_skip_in_place:
	mov	ecx,ebx
	xor	eax,eax
ALIGN	4
DD	2868115081
	mov	edi,DWORD PTR 48[esp]
	mov	esi,edx
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
	mov	DWORD PTR 40[esp],16
	jmp	$L033slow_enc_loop_x86
ALIGN	16
$L029slow_decrypt:
	bt	DWORD PTR 52[esp],25
	jnc	$L036slow_dec_loop_x86
ALIGN	4
$L037slow_dec_loop_sse:
	movq	mm0,QWORD PTR [esi]
	movq	mm4,QWORD PTR 8[esi]
	mov	edi,DWORD PTR 44[esp]
	call	__sse_AES_decrypt_compact
	mov	esi,DWORD PTR 32[esp]
	lea	eax,DWORD PTR 60[esp]
	mov	ebx,DWORD PTR 36[esp]
	mov	ecx,DWORD PTR 40[esp]
	mov	edi,DWORD PTR 48[esp]
	movq	mm1,QWORD PTR [esi]
	movq	mm5,QWORD PTR 8[esi]
	pxor	mm0,QWORD PTR [edi]
	pxor	mm4,QWORD PTR 8[edi]
	movq	QWORD PTR [edi],mm1
	movq	QWORD PTR 8[edi],mm5
	sub	ecx,16
	jc	$L038slow_dec_partial_sse
	movq	QWORD PTR [ebx],mm0
	movq	QWORD PTR 8[ebx],mm4
	lea	ebx,DWORD PTR 16[ebx]
	mov	DWORD PTR 36[esp],ebx
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 32[esp],esi
	mov	DWORD PTR 40[esp],ecx
	jnz	$L037slow_dec_loop_sse
	emms
	mov	esp,DWORD PTR 28[esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
ALIGN	16
$L038slow_dec_partial_sse:
	movq	QWORD PTR [eax],mm0
	movq	QWORD PTR 8[eax],mm4
	emms
	add	ecx,16
	mov	edi,ebx
	mov	esi,eax
ALIGN	4
DD	2767451785
	mov	esp,DWORD PTR 28[esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
ALIGN	16
$L036slow_dec_loop_x86:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	lea	edi,DWORD PTR 60[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	edi,DWORD PTR 44[esp]
	call	__x86_AES_decrypt_compact
	mov	edi,DWORD PTR 48[esp]
	mov	esi,DWORD PTR 40[esp]
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	sub	esi,16
	jc	$L039slow_dec_partial_x86
	mov	DWORD PTR 40[esp],esi
	mov	esi,DWORD PTR 36[esp]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 36[esp],esi
	lea	esi,DWORD PTR 60[esp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	esi,DWORD PTR 32[esp]
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 32[esp],esi
	jnz	$L036slow_dec_loop_x86
	mov	esp,DWORD PTR 28[esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
ALIGN	16
$L039slow_dec_partial_x86:
	lea	esi,DWORD PTR 60[esp]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	mov	esi,DWORD PTR 32[esp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	ecx,DWORD PTR 40[esp]
	mov	edi,DWORD PTR 36[esp]
	lea	esi,DWORD PTR 60[esp]
ALIGN	4
DD	2767451785
	mov	esp,DWORD PTR 28[esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_AES_cbc_encrypt ENDP
ALIGN	16
__x86_AES_set_encrypt_key	PROC PRIVATE
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 24[esp]
	mov	edi,DWORD PTR 32[esp]
	test	esi,-1
	jz	$L040badpointer
	test	edi,-1
	jz	$L040badpointer
	call	$L041pic_point
$L041pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($LAES_Te-$L041pic_point)[ebp]
	lea	ebp,DWORD PTR 2176[ebp]
	mov	eax,DWORD PTR [ebp-128]
	mov	ebx,DWORD PTR [ebp-96]
	mov	ecx,DWORD PTR [ebp-64]
	mov	edx,DWORD PTR [ebp-32]
	mov	eax,DWORD PTR [ebp]
	mov	ebx,DWORD PTR 32[ebp]
	mov	ecx,DWORD PTR 64[ebp]
	mov	edx,DWORD PTR 96[ebp]
	mov	ecx,DWORD PTR 28[esp]
	cmp	ecx,128
	je	$L04210rounds
	cmp	ecx,192
	je	$L04312rounds
	cmp	ecx,256
	je	$L04414rounds
	mov	eax,-2
	jmp	$L045exit
$L04210rounds:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	xor	ecx,ecx
	jmp	$L04610shortcut
ALIGN	4
$L04710loop:
	mov	eax,DWORD PTR [edi]
	mov	edx,DWORD PTR 12[edi]
$L04610shortcut:
	movzx	esi,dl
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	movzx	esi,dh
	shl	ebx,24
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	shr	edx,16
	movzx	esi,dl
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	movzx	esi,dh
	shl	ebx,8
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	shl	ebx,16
	xor	eax,ebx
	xor	eax,DWORD PTR 896[ecx*4+ebp]
	mov	DWORD PTR 16[edi],eax
	xor	eax,DWORD PTR 4[edi]
	mov	DWORD PTR 20[edi],eax
	xor	eax,DWORD PTR 8[edi]
	mov	DWORD PTR 24[edi],eax
	xor	eax,DWORD PTR 12[edi]
	mov	DWORD PTR 28[edi],eax
	inc	ecx
	add	edi,16
	cmp	ecx,10
	jl	$L04710loop
	mov	DWORD PTR 80[edi],10
	xor	eax,eax
	jmp	$L045exit
$L04312rounds:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	ecx,DWORD PTR 16[esi]
	mov	edx,DWORD PTR 20[esi]
	mov	DWORD PTR 16[edi],ecx
	mov	DWORD PTR 20[edi],edx
	xor	ecx,ecx
	jmp	$L04812shortcut
ALIGN	4
$L04912loop:
	mov	eax,DWORD PTR [edi]
	mov	edx,DWORD PTR 20[edi]
$L04812shortcut:
	movzx	esi,dl
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	movzx	esi,dh
	shl	ebx,24
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	shr	edx,16
	movzx	esi,dl
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	movzx	esi,dh
	shl	ebx,8
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	shl	ebx,16
	xor	eax,ebx
	xor	eax,DWORD PTR 896[ecx*4+ebp]
	mov	DWORD PTR 24[edi],eax
	xor	eax,DWORD PTR 4[edi]
	mov	DWORD PTR 28[edi],eax
	xor	eax,DWORD PTR 8[edi]
	mov	DWORD PTR 32[edi],eax
	xor	eax,DWORD PTR 12[edi]
	mov	DWORD PTR 36[edi],eax
	cmp	ecx,7
	je	$L05012break
	inc	ecx
	xor	eax,DWORD PTR 16[edi]
	mov	DWORD PTR 40[edi],eax
	xor	eax,DWORD PTR 20[edi]
	mov	DWORD PTR 44[edi],eax
	add	edi,24
	jmp	$L04912loop
$L05012break:
	mov	DWORD PTR 72[edi],12
	xor	eax,eax
	jmp	$L045exit
$L04414rounds:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	eax,DWORD PTR 16[esi]
	mov	ebx,DWORD PTR 20[esi]
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 28[esi]
	mov	DWORD PTR 16[edi],eax
	mov	DWORD PTR 20[edi],ebx
	mov	DWORD PTR 24[edi],ecx
	mov	DWORD PTR 28[edi],edx
	xor	ecx,ecx
	jmp	$L05114shortcut
ALIGN	4
$L05214loop:
	mov	edx,DWORD PTR 28[edi]
$L05114shortcut:
	mov	eax,DWORD PTR [edi]
	movzx	esi,dl
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	movzx	esi,dh
	shl	ebx,24
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	shr	edx,16
	movzx	esi,dl
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	movzx	esi,dh
	shl	ebx,8
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	shl	ebx,16
	xor	eax,ebx
	xor	eax,DWORD PTR 896[ecx*4+ebp]
	mov	DWORD PTR 32[edi],eax
	xor	eax,DWORD PTR 4[edi]
	mov	DWORD PTR 36[edi],eax
	xor	eax,DWORD PTR 8[edi]
	mov	DWORD PTR 40[edi],eax
	xor	eax,DWORD PTR 12[edi]
	mov	DWORD PTR 44[edi],eax
	cmp	ecx,6
	je	$L05314break
	inc	ecx
	mov	edx,eax
	mov	eax,DWORD PTR 16[edi]
	movzx	esi,dl
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	movzx	esi,dh
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	shr	edx,16
	shl	ebx,8
	movzx	esi,dl
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	movzx	esi,dh
	shl	ebx,16
	xor	eax,ebx
	movzx	ebx,BYTE PTR [esi*1+ebp-128]
	shl	ebx,24
	xor	eax,ebx
	mov	DWORD PTR 48[edi],eax
	xor	eax,DWORD PTR 20[edi]
	mov	DWORD PTR 52[edi],eax
	xor	eax,DWORD PTR 24[edi]
	mov	DWORD PTR 56[edi],eax
	xor	eax,DWORD PTR 28[edi]
	mov	DWORD PTR 60[edi],eax
	add	edi,32
	jmp	$L05214loop
$L05314break:
	mov	DWORD PTR 48[edi],14
	xor	eax,eax
	jmp	$L045exit
$L040badpointer:
	mov	eax,-1
$L045exit:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
__x86_AES_set_encrypt_key ENDP
ALIGN	16
_private_AES_set_encrypt_key	PROC PUBLIC
$L_private_AES_set_encrypt_key_begin::
	call	__x86_AES_set_encrypt_key
	ret
_private_AES_set_encrypt_key ENDP
ALIGN	16
_private_AES_set_decrypt_key	PROC PUBLIC
$L_private_AES_set_decrypt_key_begin::
	call	__x86_AES_set_encrypt_key
	cmp	eax,0
	je	$L054proceed
	ret
$L054proceed:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 28[esp]
	mov	ecx,DWORD PTR 240[esi]
	lea	ecx,DWORD PTR [ecx*4]
	lea	edi,DWORD PTR [ecx*4+esi]
ALIGN	4
$L055invert:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR [edi]
	mov	edx,DWORD PTR 4[edi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR [esi],ecx
	mov	DWORD PTR 4[esi],edx
	mov	eax,DWORD PTR 8[esi]
	mov	ebx,DWORD PTR 12[esi]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	mov	DWORD PTR 8[edi],eax
	mov	DWORD PTR 12[edi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	add	esi,16
	sub	edi,16
	cmp	esi,edi
	jne	$L055invert
	mov	edi,DWORD PTR 28[esp]
	mov	esi,DWORD PTR 240[edi]
	lea	esi,DWORD PTR [esi*1+esi-2]
	lea	esi,DWORD PTR [esi*8+edi]
	mov	DWORD PTR 28[esp],esi
	mov	eax,DWORD PTR 16[edi]
ALIGN	4
$L056permute:
	add	edi,16
	mov	ebp,2155905152
	and	ebp,eax
	lea	ebx,DWORD PTR [eax*1+eax]
	mov	esi,ebp
	shr	ebp,7
	sub	esi,ebp
	and	ebx,4278124286
	and	esi,454761243
	xor	ebx,esi
	mov	ebp,2155905152
	and	ebp,ebx
	lea	ecx,DWORD PTR [ebx*1+ebx]
	mov	esi,ebp
	shr	ebp,7
	sub	esi,ebp
	and	ecx,4278124286
	and	esi,454761243
	xor	ebx,eax
	xor	ecx,esi
	mov	ebp,2155905152
	and	ebp,ecx
	lea	edx,DWORD PTR [ecx*1+ecx]
	mov	esi,ebp
	shr	ebp,7
	xor	ecx,eax
	sub	esi,ebp
	and	edx,4278124286
	and	esi,454761243
	rol	eax,8
	xor	edx,esi
	mov	ebp,DWORD PTR 4[edi]
	xor	eax,ebx
	xor	ebx,edx
	xor	eax,ecx
	rol	ebx,24
	xor	ecx,edx
	xor	eax,edx
	rol	ecx,16
	xor	eax,ebx
	rol	edx,8
	xor	eax,ecx
	mov	ebx,ebp
	xor	eax,edx
	mov	DWORD PTR [edi],eax
	mov	ebp,2155905152
	and	ebp,ebx
	lea	ecx,DWORD PTR [ebx*1+ebx]
	mov	esi,ebp
	shr	ebp,7
	sub	esi,ebp
	and	ecx,4278124286
	and	esi,454761243
	xor	ecx,esi
	mov	ebp,2155905152
	and	ebp,ecx
	lea	edx,DWORD PTR [ecx*1+ecx]
	mov	esi,ebp
	shr	ebp,7
	sub	esi,ebp
	and	edx,4278124286
	and	esi,454761243
	xor	ecx,ebx
	xor	edx,esi
	mov	ebp,2155905152
	and	ebp,edx
	lea	eax,DWORD PTR [edx*1+edx]
	mov	esi,ebp
	shr	ebp,7
	xor	edx,ebx
	sub	esi,ebp
	and	eax,4278124286
	and	esi,454761243
	rol	ebx,8
	xor	eax,esi
	mov	ebp,DWORD PTR 8[edi]
	xor	ebx,ecx
	xor	ecx,eax
	xor	ebx,edx
	rol	ecx,24
	xor	edx,eax
	xor	ebx,eax
	rol	edx,16
	xor	ebx,ecx
	rol	eax,8
	xor	ebx,edx
	mov	ecx,ebp
	xor	ebx,eax
	mov	DWORD PTR 4[edi],ebx
	mov	ebp,2155905152
	and	ebp,ecx
	lea	edx,DWORD PTR [ecx*1+ecx]
	mov	esi,ebp
	shr	ebp,7
	sub	esi,ebp
	and	edx,4278124286
	and	esi,454761243
	xor	edx,esi
	mov	ebp,2155905152
	and	ebp,edx
	lea	eax,DWORD PTR [edx*1+edx]
	mov	esi,ebp
	shr	ebp,7
	sub	esi,ebp
	and	eax,4278124286
	and	esi,454761243
	xor	edx,ecx
	xor	eax,esi
	mov	ebp,2155905152
	and	ebp,eax
	lea	ebx,DWORD PTR [eax*1+eax]
	mov	esi,ebp
	shr	ebp,7
	xor	eax,ecx
	sub	esi,ebp
	and	ebx,4278124286
	and	esi,454761243
	rol	ecx,8
	xor	ebx,esi
	mov	ebp,DWORD PTR 12[edi]
	xor	ecx,edx
	xor	edx,ebx
	xor	ecx,eax
	rol	edx,24
	xor	eax,ebx
	xor	ecx,ebx
	rol	eax,16
	xor	ecx,edx
	rol	ebx,8
	xor	ecx,eax
	mov	edx,ebp
	xor	ecx,ebx
	mov	DWORD PTR 8[edi],ecx
	mov	ebp,2155905152
	and	ebp,edx
	lea	eax,DWORD PTR [edx*1+edx]
	mov	esi,ebp
	shr	ebp,7
	sub	esi,ebp
	and	eax,4278124286
	and	esi,454761243
	xor	eax,esi
	mov	ebp,2155905152
	and	ebp,eax
	lea	ebx,DWORD PTR [eax*1+eax]
	mov	esi,ebp
	shr	ebp,7
	sub	esi,ebp
	and	ebx,4278124286
	and	esi,454761243
	xor	eax,edx
	xor	ebx,esi
	mov	ebp,2155905152
	and	ebp,ebx
	lea	ecx,DWORD PTR [ebx*1+ebx]
	mov	esi,ebp
	shr	ebp,7
	xor	ebx,edx
	sub	esi,ebp
	and	ecx,4278124286
	and	esi,454761243
	rol	edx,8
	xor	ecx,esi
	mov	ebp,DWORD PTR 16[edi]
	xor	edx,eax
	xor	eax,ecx
	xor	edx,ebx
	rol	eax,24
	xor	ebx,ecx
	xor	edx,ecx
	rol	ebx,16
	xor	edx,eax
	rol	ecx,8
	xor	edx,ebx
	mov	eax,ebp
	xor	edx,ecx
	mov	DWORD PTR 12[edi],edx
	cmp	edi,DWORD PTR 28[esp]
	jb	$L056permute
	xor	eax,eax
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_private_AES_set_decrypt_key ENDP
DB	65,69,83,32,102,111,114,32,120,56,54,44,32,67,82,89
DB	80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114
DB	111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD:4
.bss	ENDS
END
