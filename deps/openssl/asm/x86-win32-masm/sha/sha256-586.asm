TITLE	sha512-586.asm
IF @Version LT 800
ECHO MASM version 8.00 or later is strongly recommended.
ENDIF
.686
.MODEL	FLAT
OPTION	DOTNAME
IF @Version LT 800
.text$	SEGMENT PAGE 'CODE'
ELSE
.text$	SEGMENT ALIGN(64) 'CODE'
ENDIF
ALIGN	16
_sha256_block_data_order	PROC PUBLIC
$L_sha256_block_data_order_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	ebx,esp
	call	$L000pic_point
$L000pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($L001K256-$L000pic_point)[ebp]
	sub	esp,16
	and	esp,-64
	shl	eax,6
	add	eax,edi
	mov	DWORD PTR [esp],esi
	mov	DWORD PTR 4[esp],edi
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
ALIGN	16
$L002loop:
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 16[edi]
	mov	ebx,DWORD PTR 20[edi]
	mov	ecx,DWORD PTR 24[edi]
	mov	edx,DWORD PTR 28[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 32[edi]
	mov	ebx,DWORD PTR 36[edi]
	mov	ecx,DWORD PTR 40[edi]
	mov	edx,DWORD PTR 44[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 48[edi]
	mov	ebx,DWORD PTR 52[edi]
	mov	ecx,DWORD PTR 56[edi]
	mov	edx,DWORD PTR 60[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	add	edi,64
	sub	esp,32
	mov	DWORD PTR 100[esp],edi
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edi,DWORD PTR 12[esi]
	mov	DWORD PTR 4[esp],ebx
	mov	DWORD PTR 8[esp],ecx
	mov	DWORD PTR 12[esp],edi
	mov	edx,DWORD PTR 16[esi]
	mov	ebx,DWORD PTR 20[esi]
	mov	ecx,DWORD PTR 24[esi]
	mov	edi,DWORD PTR 28[esi]
	mov	DWORD PTR 20[esp],ebx
	mov	DWORD PTR 24[esp],ecx
	mov	DWORD PTR 28[esp],edi
ALIGN	16
$L00300_15:
	mov	ebx,DWORD PTR 92[esp]
	mov	ecx,edx
	ror	ecx,14
	mov	esi,DWORD PTR 20[esp]
	xor	ecx,edx
	ror	ecx,5
	xor	ecx,edx
	ror	ecx,6
	mov	edi,DWORD PTR 24[esp]
	add	ebx,ecx
	xor	esi,edi
	mov	DWORD PTR 16[esp],edx
	mov	ecx,eax
	and	esi,edx
	mov	edx,DWORD PTR 12[esp]
	xor	esi,edi
	mov	edi,eax
	add	ebx,esi
	ror	ecx,9
	add	ebx,DWORD PTR 28[esp]
	xor	ecx,eax
	ror	ecx,11
	mov	esi,DWORD PTR 4[esp]
	xor	ecx,eax
	ror	ecx,2
	add	edx,ebx
	mov	edi,DWORD PTR 8[esp]
	add	ebx,ecx
	mov	DWORD PTR [esp],eax
	mov	ecx,eax
	sub	esp,4
	or	eax,esi
	and	ecx,esi
	and	eax,edi
	mov	esi,DWORD PTR [ebp]
	or	eax,ecx
	add	ebp,4
	add	eax,ebx
	add	edx,esi
	add	eax,esi
	cmp	esi,3248222580
	jne	$L00300_15
	mov	ebx,DWORD PTR 152[esp]
ALIGN	16
$L00416_63:
	mov	esi,ebx
	mov	ecx,DWORD PTR 100[esp]
	ror	esi,11
	mov	edi,ecx
	xor	esi,ebx
	ror	esi,7
	shr	ebx,3
	ror	edi,2
	xor	ebx,esi
	xor	edi,ecx
	ror	edi,17
	shr	ecx,10
	add	ebx,DWORD PTR 156[esp]
	xor	edi,ecx
	add	ebx,DWORD PTR 120[esp]
	mov	ecx,edx
	add	ebx,edi
	ror	ecx,14
	mov	esi,DWORD PTR 20[esp]
	xor	ecx,edx
	ror	ecx,5
	mov	DWORD PTR 92[esp],ebx
	xor	ecx,edx
	ror	ecx,6
	mov	edi,DWORD PTR 24[esp]
	add	ebx,ecx
	xor	esi,edi
	mov	DWORD PTR 16[esp],edx
	mov	ecx,eax
	and	esi,edx
	mov	edx,DWORD PTR 12[esp]
	xor	esi,edi
	mov	edi,eax
	add	ebx,esi
	ror	ecx,9
	add	ebx,DWORD PTR 28[esp]
	xor	ecx,eax
	ror	ecx,11
	mov	esi,DWORD PTR 4[esp]
	xor	ecx,eax
	ror	ecx,2
	add	edx,ebx
	mov	edi,DWORD PTR 8[esp]
	add	ebx,ecx
	mov	DWORD PTR [esp],eax
	mov	ecx,eax
	sub	esp,4
	or	eax,esi
	and	ecx,esi
	and	eax,edi
	mov	esi,DWORD PTR [ebp]
	or	eax,ecx
	add	ebp,4
	add	eax,ebx
	mov	ebx,DWORD PTR 152[esp]
	add	edx,esi
	add	eax,esi
	cmp	esi,3329325298
	jne	$L00416_63
	mov	esi,DWORD PTR 352[esp]
	mov	ebx,DWORD PTR 4[esp]
	mov	ecx,DWORD PTR 8[esp]
	mov	edi,DWORD PTR 12[esp]
	add	eax,DWORD PTR [esi]
	add	ebx,DWORD PTR 4[esi]
	add	ecx,DWORD PTR 8[esi]
	add	edi,DWORD PTR 12[esi]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edi
	mov	eax,DWORD PTR 20[esp]
	mov	ebx,DWORD PTR 24[esp]
	mov	ecx,DWORD PTR 28[esp]
	mov	edi,DWORD PTR 356[esp]
	add	edx,DWORD PTR 16[esi]
	add	eax,DWORD PTR 20[esi]
	add	ebx,DWORD PTR 24[esi]
	add	ecx,DWORD PTR 28[esi]
	mov	DWORD PTR 16[esi],edx
	mov	DWORD PTR 20[esi],eax
	mov	DWORD PTR 24[esi],ebx
	mov	DWORD PTR 28[esi],ecx
	add	esp,352
	sub	ebp,256
	cmp	edi,DWORD PTR 8[esp]
	jb	$L002loop
	mov	esp,DWORD PTR 12[esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	64
$L001K256:
DD	1116352408,1899447441,3049323471,3921009573
DD	961987163,1508970993,2453635748,2870763221
DD	3624381080,310598401,607225278,1426881987
DD	1925078388,2162078206,2614888103,3248222580
DD	3835390401,4022224774,264347078,604807628
DD	770255983,1249150122,1555081692,1996064986
DD	2554220882,2821834349,2952996808,3210313671
DD	3336571891,3584528711,113926993,338241895
DD	666307205,773529912,1294757372,1396182291
DD	1695183700,1986661051,2177026350,2456956037
DD	2730485921,2820302411,3259730800,3345764771
DD	3516065817,3600352804,4094571909,275423344
DD	430227734,506948616,659060556,883997877
DD	958139571,1322822218,1537002063,1747873779
DD	1955562222,2024104815,2227730452,2361852424
DD	2428436474,2756734187,3204031479,3329325298
_sha256_block_data_order ENDP
DB	83,72,65,50,53,54,32,98,108,111,99,107,32,116,114,97
DB	110,115,102,111,114,109,32,102,111,114,32,120,56,54,44,32
DB	67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97
DB	112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103
DB	62,0
.text$	ENDS
END
