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
_sha512_block_data_order	PROC PUBLIC
$L_sha512_block_data_order_begin::
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
	lea	ebp,DWORD PTR ($L001K512-$L000pic_point)[ebp]
	sub	esp,16
	and	esp,-64
	shl	eax,7
	add	eax,edi
	mov	DWORD PTR [esp],esi
	mov	DWORD PTR 4[esp],edi
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
ALIGN	16
$L002loop_x86:
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
	mov	eax,DWORD PTR 64[edi]
	mov	ebx,DWORD PTR 68[edi]
	mov	ecx,DWORD PTR 72[edi]
	mov	edx,DWORD PTR 76[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 80[edi]
	mov	ebx,DWORD PTR 84[edi]
	mov	ecx,DWORD PTR 88[edi]
	mov	edx,DWORD PTR 92[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 96[edi]
	mov	ebx,DWORD PTR 100[edi]
	mov	ecx,DWORD PTR 104[edi]
	mov	edx,DWORD PTR 108[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 112[edi]
	mov	ebx,DWORD PTR 116[edi]
	mov	ecx,DWORD PTR 120[edi]
	mov	edx,DWORD PTR 124[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	add	edi,128
	sub	esp,72
	mov	DWORD PTR 204[esp],edi
	lea	edi,DWORD PTR 8[esp]
	mov	ecx,16
DD	2784229001
ALIGN	16
$L00300_15_x86:
	mov	ecx,DWORD PTR 40[esp]
	mov	edx,DWORD PTR 44[esp]
	mov	esi,ecx
	shr	ecx,9
	mov	edi,edx
	shr	edx,9
	mov	ebx,ecx
	shl	esi,14
	mov	eax,edx
	shl	edi,14
	xor	ebx,esi
	shr	ecx,5
	xor	eax,edi
	shr	edx,5
	xor	eax,ecx
	shl	esi,4
	xor	ebx,edx
	shl	edi,4
	xor	ebx,esi
	shr	ecx,4
	xor	eax,edi
	shr	edx,4
	xor	eax,ecx
	shl	esi,5
	xor	ebx,edx
	shl	edi,5
	xor	eax,esi
	xor	ebx,edi
	mov	ecx,DWORD PTR 48[esp]
	mov	edx,DWORD PTR 52[esp]
	mov	esi,DWORD PTR 56[esp]
	mov	edi,DWORD PTR 60[esp]
	add	eax,DWORD PTR 64[esp]
	adc	ebx,DWORD PTR 68[esp]
	xor	ecx,esi
	xor	edx,edi
	and	ecx,DWORD PTR 40[esp]
	and	edx,DWORD PTR 44[esp]
	add	eax,DWORD PTR 192[esp]
	adc	ebx,DWORD PTR 196[esp]
	xor	ecx,esi
	xor	edx,edi
	mov	esi,DWORD PTR [ebp]
	mov	edi,DWORD PTR 4[ebp]
	add	eax,ecx
	adc	ebx,edx
	mov	ecx,DWORD PTR 32[esp]
	mov	edx,DWORD PTR 36[esp]
	add	eax,esi
	adc	ebx,edi
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	add	eax,ecx
	adc	ebx,edx
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	DWORD PTR 32[esp],eax
	mov	DWORD PTR 36[esp],ebx
	mov	esi,ecx
	shr	ecx,2
	mov	edi,edx
	shr	edx,2
	mov	ebx,ecx
	shl	esi,4
	mov	eax,edx
	shl	edi,4
	xor	ebx,esi
	shr	ecx,5
	xor	eax,edi
	shr	edx,5
	xor	ebx,ecx
	shl	esi,21
	xor	eax,edx
	shl	edi,21
	xor	eax,esi
	shr	ecx,21
	xor	ebx,edi
	shr	edx,21
	xor	eax,ecx
	shl	esi,5
	xor	ebx,edx
	shl	edi,5
	xor	eax,esi
	xor	ebx,edi
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 16[esp]
	mov	edi,DWORD PTR 20[esp]
	add	eax,DWORD PTR [esp]
	adc	ebx,DWORD PTR 4[esp]
	or	ecx,esi
	or	edx,edi
	and	ecx,DWORD PTR 24[esp]
	and	edx,DWORD PTR 28[esp]
	and	esi,DWORD PTR 8[esp]
	and	edi,DWORD PTR 12[esp]
	or	ecx,esi
	or	edx,edi
	add	eax,ecx
	adc	ebx,edx
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	dl,BYTE PTR [ebp]
	sub	esp,8
	lea	ebp,DWORD PTR 8[ebp]
	cmp	dl,148
	jne	$L00300_15_x86
ALIGN	16
$L00416_79_x86:
	mov	ecx,DWORD PTR 312[esp]
	mov	edx,DWORD PTR 316[esp]
	mov	esi,ecx
	shr	ecx,1
	mov	edi,edx
	shr	edx,1
	mov	eax,ecx
	shl	esi,24
	mov	ebx,edx
	shl	edi,24
	xor	ebx,esi
	shr	ecx,6
	xor	eax,edi
	shr	edx,6
	xor	eax,ecx
	shl	esi,7
	xor	ebx,edx
	shl	edi,1
	xor	ebx,esi
	shr	ecx,1
	xor	eax,edi
	shr	edx,1
	xor	eax,ecx
	shl	edi,6
	xor	ebx,edx
	xor	eax,edi
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	ecx,DWORD PTR 208[esp]
	mov	edx,DWORD PTR 212[esp]
	mov	esi,ecx
	shr	ecx,6
	mov	edi,edx
	shr	edx,6
	mov	eax,ecx
	shl	esi,3
	mov	ebx,edx
	shl	edi,3
	xor	eax,esi
	shr	ecx,13
	xor	ebx,edi
	shr	edx,13
	xor	eax,ecx
	shl	esi,10
	xor	ebx,edx
	shl	edi,10
	xor	ebx,esi
	shr	ecx,10
	xor	eax,edi
	shr	edx,10
	xor	ebx,ecx
	shl	edi,13
	xor	eax,edx
	xor	eax,edi
	mov	ecx,DWORD PTR 320[esp]
	mov	edx,DWORD PTR 324[esp]
	add	eax,DWORD PTR [esp]
	adc	ebx,DWORD PTR 4[esp]
	mov	esi,DWORD PTR 248[esp]
	mov	edi,DWORD PTR 252[esp]
	add	eax,ecx
	adc	ebx,edx
	add	eax,esi
	adc	ebx,edi
	mov	DWORD PTR 192[esp],eax
	mov	DWORD PTR 196[esp],ebx
	mov	ecx,DWORD PTR 40[esp]
	mov	edx,DWORD PTR 44[esp]
	mov	esi,ecx
	shr	ecx,9
	mov	edi,edx
	shr	edx,9
	mov	ebx,ecx
	shl	esi,14
	mov	eax,edx
	shl	edi,14
	xor	ebx,esi
	shr	ecx,5
	xor	eax,edi
	shr	edx,5
	xor	eax,ecx
	shl	esi,4
	xor	ebx,edx
	shl	edi,4
	xor	ebx,esi
	shr	ecx,4
	xor	eax,edi
	shr	edx,4
	xor	eax,ecx
	shl	esi,5
	xor	ebx,edx
	shl	edi,5
	xor	eax,esi
	xor	ebx,edi
	mov	ecx,DWORD PTR 48[esp]
	mov	edx,DWORD PTR 52[esp]
	mov	esi,DWORD PTR 56[esp]
	mov	edi,DWORD PTR 60[esp]
	add	eax,DWORD PTR 64[esp]
	adc	ebx,DWORD PTR 68[esp]
	xor	ecx,esi
	xor	edx,edi
	and	ecx,DWORD PTR 40[esp]
	and	edx,DWORD PTR 44[esp]
	add	eax,DWORD PTR 192[esp]
	adc	ebx,DWORD PTR 196[esp]
	xor	ecx,esi
	xor	edx,edi
	mov	esi,DWORD PTR [ebp]
	mov	edi,DWORD PTR 4[ebp]
	add	eax,ecx
	adc	ebx,edx
	mov	ecx,DWORD PTR 32[esp]
	mov	edx,DWORD PTR 36[esp]
	add	eax,esi
	adc	ebx,edi
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	add	eax,ecx
	adc	ebx,edx
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	DWORD PTR 32[esp],eax
	mov	DWORD PTR 36[esp],ebx
	mov	esi,ecx
	shr	ecx,2
	mov	edi,edx
	shr	edx,2
	mov	ebx,ecx
	shl	esi,4
	mov	eax,edx
	shl	edi,4
	xor	ebx,esi
	shr	ecx,5
	xor	eax,edi
	shr	edx,5
	xor	ebx,ecx
	shl	esi,21
	xor	eax,edx
	shl	edi,21
	xor	eax,esi
	shr	ecx,21
	xor	ebx,edi
	shr	edx,21
	xor	eax,ecx
	shl	esi,5
	xor	ebx,edx
	shl	edi,5
	xor	eax,esi
	xor	ebx,edi
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 16[esp]
	mov	edi,DWORD PTR 20[esp]
	add	eax,DWORD PTR [esp]
	adc	ebx,DWORD PTR 4[esp]
	or	ecx,esi
	or	edx,edi
	and	ecx,DWORD PTR 24[esp]
	and	edx,DWORD PTR 28[esp]
	and	esi,DWORD PTR 8[esp]
	and	edi,DWORD PTR 12[esp]
	or	ecx,esi
	or	edx,edi
	add	eax,ecx
	adc	ebx,edx
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	dl,BYTE PTR [ebp]
	sub	esp,8
	lea	ebp,DWORD PTR 8[ebp]
	cmp	dl,23
	jne	$L00416_79_x86
	mov	esi,DWORD PTR 840[esp]
	mov	edi,DWORD PTR 844[esp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	add	eax,DWORD PTR 8[esp]
	adc	ebx,DWORD PTR 12[esp]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	add	ecx,DWORD PTR 16[esp]
	adc	edx,DWORD PTR 20[esp]
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	mov	eax,DWORD PTR 16[esi]
	mov	ebx,DWORD PTR 20[esi]
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 28[esi]
	add	eax,DWORD PTR 24[esp]
	adc	ebx,DWORD PTR 28[esp]
	mov	DWORD PTR 16[esi],eax
	mov	DWORD PTR 20[esi],ebx
	add	ecx,DWORD PTR 32[esp]
	adc	edx,DWORD PTR 36[esp]
	mov	DWORD PTR 24[esi],ecx
	mov	DWORD PTR 28[esi],edx
	mov	eax,DWORD PTR 32[esi]
	mov	ebx,DWORD PTR 36[esi]
	mov	ecx,DWORD PTR 40[esi]
	mov	edx,DWORD PTR 44[esi]
	add	eax,DWORD PTR 40[esp]
	adc	ebx,DWORD PTR 44[esp]
	mov	DWORD PTR 32[esi],eax
	mov	DWORD PTR 36[esi],ebx
	add	ecx,DWORD PTR 48[esp]
	adc	edx,DWORD PTR 52[esp]
	mov	DWORD PTR 40[esi],ecx
	mov	DWORD PTR 44[esi],edx
	mov	eax,DWORD PTR 48[esi]
	mov	ebx,DWORD PTR 52[esi]
	mov	ecx,DWORD PTR 56[esi]
	mov	edx,DWORD PTR 60[esi]
	add	eax,DWORD PTR 56[esp]
	adc	ebx,DWORD PTR 60[esp]
	mov	DWORD PTR 48[esi],eax
	mov	DWORD PTR 52[esi],ebx
	add	ecx,DWORD PTR 64[esp]
	adc	edx,DWORD PTR 68[esp]
	mov	DWORD PTR 56[esi],ecx
	mov	DWORD PTR 60[esi],edx
	add	esp,840
	sub	ebp,640
	cmp	edi,DWORD PTR 8[esp]
	jb	$L002loop_x86
	mov	esp,DWORD PTR 12[esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	64
$L001K512:
DD	3609767458,1116352408
DD	602891725,1899447441
DD	3964484399,3049323471
DD	2173295548,3921009573
DD	4081628472,961987163
DD	3053834265,1508970993
DD	2937671579,2453635748
DD	3664609560,2870763221
DD	2734883394,3624381080
DD	1164996542,310598401
DD	1323610764,607225278
DD	3590304994,1426881987
DD	4068182383,1925078388
DD	991336113,2162078206
DD	633803317,2614888103
DD	3479774868,3248222580
DD	2666613458,3835390401
DD	944711139,4022224774
DD	2341262773,264347078
DD	2007800933,604807628
DD	1495990901,770255983
DD	1856431235,1249150122
DD	3175218132,1555081692
DD	2198950837,1996064986
DD	3999719339,2554220882
DD	766784016,2821834349
DD	2566594879,2952996808
DD	3203337956,3210313671
DD	1034457026,3336571891
DD	2466948901,3584528711
DD	3758326383,113926993
DD	168717936,338241895
DD	1188179964,666307205
DD	1546045734,773529912
DD	1522805485,1294757372
DD	2643833823,1396182291
DD	2343527390,1695183700
DD	1014477480,1986661051
DD	1206759142,2177026350
DD	344077627,2456956037
DD	1290863460,2730485921
DD	3158454273,2820302411
DD	3505952657,3259730800
DD	106217008,3345764771
DD	3606008344,3516065817
DD	1432725776,3600352804
DD	1467031594,4094571909
DD	851169720,275423344
DD	3100823752,430227734
DD	1363258195,506948616
DD	3750685593,659060556
DD	3785050280,883997877
DD	3318307427,958139571
DD	3812723403,1322822218
DD	2003034995,1537002063
DD	3602036899,1747873779
DD	1575990012,1955562222
DD	1125592928,2024104815
DD	2716904306,2227730452
DD	442776044,2361852424
DD	593698344,2428436474
DD	3733110249,2756734187
DD	2999351573,3204031479
DD	3815920427,3329325298
DD	3928383900,3391569614
DD	566280711,3515267271
DD	3454069534,3940187606
DD	4000239992,4118630271
DD	1914138554,116418474
DD	2731055270,174292421
DD	3203993006,289380356
DD	320620315,460393269
DD	587496836,685471733
DD	1086792851,852142971
DD	365543100,1017036298
DD	2618297676,1126000580
DD	3409855158,1288033470
DD	4234509866,1501505948
DD	987167468,1607167915
DD	1246189591,1816402316
_sha512_block_data_order ENDP
DB	83,72,65,53,49,50,32,98,108,111,99,107,32,116,114,97
DB	110,115,102,111,114,109,32,102,111,114,32,120,56,54,44,32
DB	67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97
DB	112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103
DB	62,0
.text$	ENDS
END
