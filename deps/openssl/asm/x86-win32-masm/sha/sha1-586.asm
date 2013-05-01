TITLE	sha1-586.asm
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
_sha1_block_data_order	PROC PUBLIC
$L_sha1_block_data_order_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,DWORD PTR 20[esp]
	mov	esi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	sub	esp,76
	shl	eax,6
	add	eax,esi
	mov	DWORD PTR 104[esp],eax
	mov	edi,DWORD PTR 16[ebp]
	jmp	$L000loop
ALIGN	16
$L000loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	DWORD PTR 8[esp],ecx
	mov	DWORD PTR 12[esp],edx
	mov	eax,DWORD PTR 16[esi]
	mov	ebx,DWORD PTR 20[esi]
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 28[esi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR 16[esp],eax
	mov	DWORD PTR 20[esp],ebx
	mov	DWORD PTR 24[esp],ecx
	mov	DWORD PTR 28[esp],edx
	mov	eax,DWORD PTR 32[esi]
	mov	ebx,DWORD PTR 36[esi]
	mov	ecx,DWORD PTR 40[esi]
	mov	edx,DWORD PTR 44[esi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR 32[esp],eax
	mov	DWORD PTR 36[esp],ebx
	mov	DWORD PTR 40[esp],ecx
	mov	DWORD PTR 44[esp],edx
	mov	eax,DWORD PTR 48[esi]
	mov	ebx,DWORD PTR 52[esi]
	mov	ecx,DWORD PTR 56[esi]
	mov	edx,DWORD PTR 60[esi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR 48[esp],eax
	mov	DWORD PTR 52[esp],ebx
	mov	DWORD PTR 56[esp],ecx
	mov	DWORD PTR 60[esp],edx
	mov	DWORD PTR 100[esp],esi
	mov	eax,DWORD PTR [ebp]
	mov	ebx,DWORD PTR 4[ebp]
	mov	ecx,DWORD PTR 8[ebp]
	mov	edx,DWORD PTR 12[ebp]
	; 00_15 0
	mov	esi,ecx
	mov	ebp,eax
	rol	ebp,5
	xor	esi,edx
	add	ebp,edi
	mov	edi,DWORD PTR [esp]
	and	esi,ebx
	ror	ebx,2
	xor	esi,edx
	lea	ebp,DWORD PTR 1518500249[edi*1+ebp]
	add	ebp,esi
	; 00_15 1
	mov	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	xor	edi,ecx
	add	ebp,edx
	mov	edx,DWORD PTR 4[esp]
	and	edi,eax
	ror	eax,2
	xor	edi,ecx
	lea	ebp,DWORD PTR 1518500249[edx*1+ebp]
	add	ebp,edi
	; 00_15 2
	mov	edx,eax
	mov	edi,ebp
	rol	ebp,5
	xor	edx,ebx
	add	ebp,ecx
	mov	ecx,DWORD PTR 8[esp]
	and	edx,esi
	ror	esi,2
	xor	edx,ebx
	lea	ebp,DWORD PTR 1518500249[ecx*1+ebp]
	add	ebp,edx
	; 00_15 3
	mov	ecx,esi
	mov	edx,ebp
	rol	ebp,5
	xor	ecx,eax
	add	ebp,ebx
	mov	ebx,DWORD PTR 12[esp]
	and	ecx,edi
	ror	edi,2
	xor	ecx,eax
	lea	ebp,DWORD PTR 1518500249[ebx*1+ebp]
	add	ebp,ecx
	; 00_15 4
	mov	ebx,edi
	mov	ecx,ebp
	rol	ebp,5
	xor	ebx,esi
	add	ebp,eax
	mov	eax,DWORD PTR 16[esp]
	and	ebx,edx
	ror	edx,2
	xor	ebx,esi
	lea	ebp,DWORD PTR 1518500249[eax*1+ebp]
	add	ebp,ebx
	; 00_15 5
	mov	eax,edx
	mov	ebx,ebp
	rol	ebp,5
	xor	eax,edi
	add	ebp,esi
	mov	esi,DWORD PTR 20[esp]
	and	eax,ecx
	ror	ecx,2
	xor	eax,edi
	lea	ebp,DWORD PTR 1518500249[esi*1+ebp]
	add	ebp,eax
	; 00_15 6
	mov	esi,ecx
	mov	eax,ebp
	rol	ebp,5
	xor	esi,edx
	add	ebp,edi
	mov	edi,DWORD PTR 24[esp]
	and	esi,ebx
	ror	ebx,2
	xor	esi,edx
	lea	ebp,DWORD PTR 1518500249[edi*1+ebp]
	add	ebp,esi
	; 00_15 7
	mov	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	xor	edi,ecx
	add	ebp,edx
	mov	edx,DWORD PTR 28[esp]
	and	edi,eax
	ror	eax,2
	xor	edi,ecx
	lea	ebp,DWORD PTR 1518500249[edx*1+ebp]
	add	ebp,edi
	; 00_15 8
	mov	edx,eax
	mov	edi,ebp
	rol	ebp,5
	xor	edx,ebx
	add	ebp,ecx
	mov	ecx,DWORD PTR 32[esp]
	and	edx,esi
	ror	esi,2
	xor	edx,ebx
	lea	ebp,DWORD PTR 1518500249[ecx*1+ebp]
	add	ebp,edx
	; 00_15 9
	mov	ecx,esi
	mov	edx,ebp
	rol	ebp,5
	xor	ecx,eax
	add	ebp,ebx
	mov	ebx,DWORD PTR 36[esp]
	and	ecx,edi
	ror	edi,2
	xor	ecx,eax
	lea	ebp,DWORD PTR 1518500249[ebx*1+ebp]
	add	ebp,ecx
	; 00_15 10
	mov	ebx,edi
	mov	ecx,ebp
	rol	ebp,5
	xor	ebx,esi
	add	ebp,eax
	mov	eax,DWORD PTR 40[esp]
	and	ebx,edx
	ror	edx,2
	xor	ebx,esi
	lea	ebp,DWORD PTR 1518500249[eax*1+ebp]
	add	ebp,ebx
	; 00_15 11
	mov	eax,edx
	mov	ebx,ebp
	rol	ebp,5
	xor	eax,edi
	add	ebp,esi
	mov	esi,DWORD PTR 44[esp]
	and	eax,ecx
	ror	ecx,2
	xor	eax,edi
	lea	ebp,DWORD PTR 1518500249[esi*1+ebp]
	add	ebp,eax
	; 00_15 12
	mov	esi,ecx
	mov	eax,ebp
	rol	ebp,5
	xor	esi,edx
	add	ebp,edi
	mov	edi,DWORD PTR 48[esp]
	and	esi,ebx
	ror	ebx,2
	xor	esi,edx
	lea	ebp,DWORD PTR 1518500249[edi*1+ebp]
	add	ebp,esi
	; 00_15 13
	mov	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	xor	edi,ecx
	add	ebp,edx
	mov	edx,DWORD PTR 52[esp]
	and	edi,eax
	ror	eax,2
	xor	edi,ecx
	lea	ebp,DWORD PTR 1518500249[edx*1+ebp]
	add	ebp,edi
	; 00_15 14
	mov	edx,eax
	mov	edi,ebp
	rol	ebp,5
	xor	edx,ebx
	add	ebp,ecx
	mov	ecx,DWORD PTR 56[esp]
	and	edx,esi
	ror	esi,2
	xor	edx,ebx
	lea	ebp,DWORD PTR 1518500249[ecx*1+ebp]
	add	ebp,edx
	; 00_15 15
	mov	ecx,esi
	mov	edx,ebp
	rol	ebp,5
	xor	ecx,eax
	add	ebp,ebx
	mov	ebx,DWORD PTR 60[esp]
	and	ecx,edi
	ror	edi,2
	xor	ecx,eax
	lea	ebp,DWORD PTR 1518500249[ebx*1+ebp]
	mov	ebx,DWORD PTR [esp]
	add	ecx,ebp
	; 16_19 16
	mov	ebp,edi
	xor	ebx,DWORD PTR 8[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 32[esp]
	and	ebp,edx
	xor	ebx,DWORD PTR 52[esp]
	rol	ebx,1
	xor	ebp,esi
	add	eax,ebp
	mov	ebp,ecx
	ror	edx,2
	mov	DWORD PTR [esp],ebx
	rol	ebp,5
	lea	ebx,DWORD PTR 1518500249[eax*1+ebx]
	mov	eax,DWORD PTR 4[esp]
	add	ebx,ebp
	; 16_19 17
	mov	ebp,edx
	xor	eax,DWORD PTR 12[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 36[esp]
	and	ebp,ecx
	xor	eax,DWORD PTR 56[esp]
	rol	eax,1
	xor	ebp,edi
	add	esi,ebp
	mov	ebp,ebx
	ror	ecx,2
	mov	DWORD PTR 4[esp],eax
	rol	ebp,5
	lea	eax,DWORD PTR 1518500249[esi*1+eax]
	mov	esi,DWORD PTR 8[esp]
	add	eax,ebp
	; 16_19 18
	mov	ebp,ecx
	xor	esi,DWORD PTR 16[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 40[esp]
	and	ebp,ebx
	xor	esi,DWORD PTR 60[esp]
	rol	esi,1
	xor	ebp,edx
	add	edi,ebp
	mov	ebp,eax
	ror	ebx,2
	mov	DWORD PTR 8[esp],esi
	rol	ebp,5
	lea	esi,DWORD PTR 1518500249[edi*1+esi]
	mov	edi,DWORD PTR 12[esp]
	add	esi,ebp
	; 16_19 19
	mov	ebp,ebx
	xor	edi,DWORD PTR 20[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 44[esp]
	and	ebp,eax
	xor	edi,DWORD PTR [esp]
	rol	edi,1
	xor	ebp,ecx
	add	edx,ebp
	mov	ebp,esi
	ror	eax,2
	mov	DWORD PTR 12[esp],edi
	rol	ebp,5
	lea	edi,DWORD PTR 1518500249[edx*1+edi]
	mov	edx,DWORD PTR 16[esp]
	add	edi,ebp
	; 20_39 20
	mov	ebp,esi
	xor	edx,DWORD PTR 24[esp]
	xor	ebp,eax
	xor	edx,DWORD PTR 48[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 4[esp]
	rol	edx,1
	add	ecx,ebp
	ror	esi,2
	mov	ebp,edi
	rol	ebp,5
	mov	DWORD PTR 16[esp],edx
	lea	edx,DWORD PTR 1859775393[ecx*1+edx]
	mov	ecx,DWORD PTR 20[esp]
	add	edx,ebp
	; 20_39 21
	mov	ebp,edi
	xor	ecx,DWORD PTR 28[esp]
	xor	ebp,esi
	xor	ecx,DWORD PTR 52[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 8[esp]
	rol	ecx,1
	add	ebx,ebp
	ror	edi,2
	mov	ebp,edx
	rol	ebp,5
	mov	DWORD PTR 20[esp],ecx
	lea	ecx,DWORD PTR 1859775393[ebx*1+ecx]
	mov	ebx,DWORD PTR 24[esp]
	add	ecx,ebp
	; 20_39 22
	mov	ebp,edx
	xor	ebx,DWORD PTR 32[esp]
	xor	ebp,edi
	xor	ebx,DWORD PTR 56[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 12[esp]
	rol	ebx,1
	add	eax,ebp
	ror	edx,2
	mov	ebp,ecx
	rol	ebp,5
	mov	DWORD PTR 24[esp],ebx
	lea	ebx,DWORD PTR 1859775393[eax*1+ebx]
	mov	eax,DWORD PTR 28[esp]
	add	ebx,ebp
	; 20_39 23
	mov	ebp,ecx
	xor	eax,DWORD PTR 36[esp]
	xor	ebp,edx
	xor	eax,DWORD PTR 60[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 16[esp]
	rol	eax,1
	add	esi,ebp
	ror	ecx,2
	mov	ebp,ebx
	rol	ebp,5
	mov	DWORD PTR 28[esp],eax
	lea	eax,DWORD PTR 1859775393[esi*1+eax]
	mov	esi,DWORD PTR 32[esp]
	add	eax,ebp
	; 20_39 24
	mov	ebp,ebx
	xor	esi,DWORD PTR 40[esp]
	xor	ebp,ecx
	xor	esi,DWORD PTR [esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 20[esp]
	rol	esi,1
	add	edi,ebp
	ror	ebx,2
	mov	ebp,eax
	rol	ebp,5
	mov	DWORD PTR 32[esp],esi
	lea	esi,DWORD PTR 1859775393[edi*1+esi]
	mov	edi,DWORD PTR 36[esp]
	add	esi,ebp
	; 20_39 25
	mov	ebp,eax
	xor	edi,DWORD PTR 44[esp]
	xor	ebp,ebx
	xor	edi,DWORD PTR 4[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 24[esp]
	rol	edi,1
	add	edx,ebp
	ror	eax,2
	mov	ebp,esi
	rol	ebp,5
	mov	DWORD PTR 36[esp],edi
	lea	edi,DWORD PTR 1859775393[edx*1+edi]
	mov	edx,DWORD PTR 40[esp]
	add	edi,ebp
	; 20_39 26
	mov	ebp,esi
	xor	edx,DWORD PTR 48[esp]
	xor	ebp,eax
	xor	edx,DWORD PTR 8[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 28[esp]
	rol	edx,1
	add	ecx,ebp
	ror	esi,2
	mov	ebp,edi
	rol	ebp,5
	mov	DWORD PTR 40[esp],edx
	lea	edx,DWORD PTR 1859775393[ecx*1+edx]
	mov	ecx,DWORD PTR 44[esp]
	add	edx,ebp
	; 20_39 27
	mov	ebp,edi
	xor	ecx,DWORD PTR 52[esp]
	xor	ebp,esi
	xor	ecx,DWORD PTR 12[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 32[esp]
	rol	ecx,1
	add	ebx,ebp
	ror	edi,2
	mov	ebp,edx
	rol	ebp,5
	mov	DWORD PTR 44[esp],ecx
	lea	ecx,DWORD PTR 1859775393[ebx*1+ecx]
	mov	ebx,DWORD PTR 48[esp]
	add	ecx,ebp
	; 20_39 28
	mov	ebp,edx
	xor	ebx,DWORD PTR 56[esp]
	xor	ebp,edi
	xor	ebx,DWORD PTR 16[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 36[esp]
	rol	ebx,1
	add	eax,ebp
	ror	edx,2
	mov	ebp,ecx
	rol	ebp,5
	mov	DWORD PTR 48[esp],ebx
	lea	ebx,DWORD PTR 1859775393[eax*1+ebx]
	mov	eax,DWORD PTR 52[esp]
	add	ebx,ebp
	; 20_39 29
	mov	ebp,ecx
	xor	eax,DWORD PTR 60[esp]
	xor	ebp,edx
	xor	eax,DWORD PTR 20[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 40[esp]
	rol	eax,1
	add	esi,ebp
	ror	ecx,2
	mov	ebp,ebx
	rol	ebp,5
	mov	DWORD PTR 52[esp],eax
	lea	eax,DWORD PTR 1859775393[esi*1+eax]
	mov	esi,DWORD PTR 56[esp]
	add	eax,ebp
	; 20_39 30
	mov	ebp,ebx
	xor	esi,DWORD PTR [esp]
	xor	ebp,ecx
	xor	esi,DWORD PTR 24[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 44[esp]
	rol	esi,1
	add	edi,ebp
	ror	ebx,2
	mov	ebp,eax
	rol	ebp,5
	mov	DWORD PTR 56[esp],esi
	lea	esi,DWORD PTR 1859775393[edi*1+esi]
	mov	edi,DWORD PTR 60[esp]
	add	esi,ebp
	; 20_39 31
	mov	ebp,eax
	xor	edi,DWORD PTR 4[esp]
	xor	ebp,ebx
	xor	edi,DWORD PTR 28[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 48[esp]
	rol	edi,1
	add	edx,ebp
	ror	eax,2
	mov	ebp,esi
	rol	ebp,5
	mov	DWORD PTR 60[esp],edi
	lea	edi,DWORD PTR 1859775393[edx*1+edi]
	mov	edx,DWORD PTR [esp]
	add	edi,ebp
	; 20_39 32
	mov	ebp,esi
	xor	edx,DWORD PTR 8[esp]
	xor	ebp,eax
	xor	edx,DWORD PTR 32[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 52[esp]
	rol	edx,1
	add	ecx,ebp
	ror	esi,2
	mov	ebp,edi
	rol	ebp,5
	mov	DWORD PTR [esp],edx
	lea	edx,DWORD PTR 1859775393[ecx*1+edx]
	mov	ecx,DWORD PTR 4[esp]
	add	edx,ebp
	; 20_39 33
	mov	ebp,edi
	xor	ecx,DWORD PTR 12[esp]
	xor	ebp,esi
	xor	ecx,DWORD PTR 36[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 56[esp]
	rol	ecx,1
	add	ebx,ebp
	ror	edi,2
	mov	ebp,edx
	rol	ebp,5
	mov	DWORD PTR 4[esp],ecx
	lea	ecx,DWORD PTR 1859775393[ebx*1+ecx]
	mov	ebx,DWORD PTR 8[esp]
	add	ecx,ebp
	; 20_39 34
	mov	ebp,edx
	xor	ebx,DWORD PTR 16[esp]
	xor	ebp,edi
	xor	ebx,DWORD PTR 40[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 60[esp]
	rol	ebx,1
	add	eax,ebp
	ror	edx,2
	mov	ebp,ecx
	rol	ebp,5
	mov	DWORD PTR 8[esp],ebx
	lea	ebx,DWORD PTR 1859775393[eax*1+ebx]
	mov	eax,DWORD PTR 12[esp]
	add	ebx,ebp
	; 20_39 35
	mov	ebp,ecx
	xor	eax,DWORD PTR 20[esp]
	xor	ebp,edx
	xor	eax,DWORD PTR 44[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR [esp]
	rol	eax,1
	add	esi,ebp
	ror	ecx,2
	mov	ebp,ebx
	rol	ebp,5
	mov	DWORD PTR 12[esp],eax
	lea	eax,DWORD PTR 1859775393[esi*1+eax]
	mov	esi,DWORD PTR 16[esp]
	add	eax,ebp
	; 20_39 36
	mov	ebp,ebx
	xor	esi,DWORD PTR 24[esp]
	xor	ebp,ecx
	xor	esi,DWORD PTR 48[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 4[esp]
	rol	esi,1
	add	edi,ebp
	ror	ebx,2
	mov	ebp,eax
	rol	ebp,5
	mov	DWORD PTR 16[esp],esi
	lea	esi,DWORD PTR 1859775393[edi*1+esi]
	mov	edi,DWORD PTR 20[esp]
	add	esi,ebp
	; 20_39 37
	mov	ebp,eax
	xor	edi,DWORD PTR 28[esp]
	xor	ebp,ebx
	xor	edi,DWORD PTR 52[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 8[esp]
	rol	edi,1
	add	edx,ebp
	ror	eax,2
	mov	ebp,esi
	rol	ebp,5
	mov	DWORD PTR 20[esp],edi
	lea	edi,DWORD PTR 1859775393[edx*1+edi]
	mov	edx,DWORD PTR 24[esp]
	add	edi,ebp
	; 20_39 38
	mov	ebp,esi
	xor	edx,DWORD PTR 32[esp]
	xor	ebp,eax
	xor	edx,DWORD PTR 56[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 12[esp]
	rol	edx,1
	add	ecx,ebp
	ror	esi,2
	mov	ebp,edi
	rol	ebp,5
	mov	DWORD PTR 24[esp],edx
	lea	edx,DWORD PTR 1859775393[ecx*1+edx]
	mov	ecx,DWORD PTR 28[esp]
	add	edx,ebp
	; 20_39 39
	mov	ebp,edi
	xor	ecx,DWORD PTR 36[esp]
	xor	ebp,esi
	xor	ecx,DWORD PTR 60[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 16[esp]
	rol	ecx,1
	add	ebx,ebp
	ror	edi,2
	mov	ebp,edx
	rol	ebp,5
	mov	DWORD PTR 28[esp],ecx
	lea	ecx,DWORD PTR 1859775393[ebx*1+ecx]
	mov	ebx,DWORD PTR 32[esp]
	add	ecx,ebp
	; 40_59 40
	mov	ebp,edi
	xor	ebx,DWORD PTR 40[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR [esp]
	and	ebp,edx
	xor	ebx,DWORD PTR 20[esp]
	rol	ebx,1
	add	ebp,eax
	ror	edx,2
	mov	eax,ecx
	rol	eax,5
	mov	DWORD PTR 32[esp],ebx
	lea	ebx,DWORD PTR 2400959708[ebp*1+ebx]
	mov	ebp,edi
	add	ebx,eax
	and	ebp,esi
	mov	eax,DWORD PTR 36[esp]
	add	ebx,ebp
	; 40_59 41
	mov	ebp,edx
	xor	eax,DWORD PTR 44[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 4[esp]
	and	ebp,ecx
	xor	eax,DWORD PTR 24[esp]
	rol	eax,1
	add	ebp,esi
	ror	ecx,2
	mov	esi,ebx
	rol	esi,5
	mov	DWORD PTR 36[esp],eax
	lea	eax,DWORD PTR 2400959708[ebp*1+eax]
	mov	ebp,edx
	add	eax,esi
	and	ebp,edi
	mov	esi,DWORD PTR 40[esp]
	add	eax,ebp
	; 40_59 42
	mov	ebp,ecx
	xor	esi,DWORD PTR 48[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 8[esp]
	and	ebp,ebx
	xor	esi,DWORD PTR 28[esp]
	rol	esi,1
	add	ebp,edi
	ror	ebx,2
	mov	edi,eax
	rol	edi,5
	mov	DWORD PTR 40[esp],esi
	lea	esi,DWORD PTR 2400959708[ebp*1+esi]
	mov	ebp,ecx
	add	esi,edi
	and	ebp,edx
	mov	edi,DWORD PTR 44[esp]
	add	esi,ebp
	; 40_59 43
	mov	ebp,ebx
	xor	edi,DWORD PTR 52[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 12[esp]
	and	ebp,eax
	xor	edi,DWORD PTR 32[esp]
	rol	edi,1
	add	ebp,edx
	ror	eax,2
	mov	edx,esi
	rol	edx,5
	mov	DWORD PTR 44[esp],edi
	lea	edi,DWORD PTR 2400959708[ebp*1+edi]
	mov	ebp,ebx
	add	edi,edx
	and	ebp,ecx
	mov	edx,DWORD PTR 48[esp]
	add	edi,ebp
	; 40_59 44
	mov	ebp,eax
	xor	edx,DWORD PTR 56[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 16[esp]
	and	ebp,esi
	xor	edx,DWORD PTR 36[esp]
	rol	edx,1
	add	ebp,ecx
	ror	esi,2
	mov	ecx,edi
	rol	ecx,5
	mov	DWORD PTR 48[esp],edx
	lea	edx,DWORD PTR 2400959708[ebp*1+edx]
	mov	ebp,eax
	add	edx,ecx
	and	ebp,ebx
	mov	ecx,DWORD PTR 52[esp]
	add	edx,ebp
	; 40_59 45
	mov	ebp,esi
	xor	ecx,DWORD PTR 60[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 20[esp]
	and	ebp,edi
	xor	ecx,DWORD PTR 40[esp]
	rol	ecx,1
	add	ebp,ebx
	ror	edi,2
	mov	ebx,edx
	rol	ebx,5
	mov	DWORD PTR 52[esp],ecx
	lea	ecx,DWORD PTR 2400959708[ebp*1+ecx]
	mov	ebp,esi
	add	ecx,ebx
	and	ebp,eax
	mov	ebx,DWORD PTR 56[esp]
	add	ecx,ebp
	; 40_59 46
	mov	ebp,edi
	xor	ebx,DWORD PTR [esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 24[esp]
	and	ebp,edx
	xor	ebx,DWORD PTR 44[esp]
	rol	ebx,1
	add	ebp,eax
	ror	edx,2
	mov	eax,ecx
	rol	eax,5
	mov	DWORD PTR 56[esp],ebx
	lea	ebx,DWORD PTR 2400959708[ebp*1+ebx]
	mov	ebp,edi
	add	ebx,eax
	and	ebp,esi
	mov	eax,DWORD PTR 60[esp]
	add	ebx,ebp
	; 40_59 47
	mov	ebp,edx
	xor	eax,DWORD PTR 4[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 28[esp]
	and	ebp,ecx
	xor	eax,DWORD PTR 48[esp]
	rol	eax,1
	add	ebp,esi
	ror	ecx,2
	mov	esi,ebx
	rol	esi,5
	mov	DWORD PTR 60[esp],eax
	lea	eax,DWORD PTR 2400959708[ebp*1+eax]
	mov	ebp,edx
	add	eax,esi
	and	ebp,edi
	mov	esi,DWORD PTR [esp]
	add	eax,ebp
	; 40_59 48
	mov	ebp,ecx
	xor	esi,DWORD PTR 8[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 32[esp]
	and	ebp,ebx
	xor	esi,DWORD PTR 52[esp]
	rol	esi,1
	add	ebp,edi
	ror	ebx,2
	mov	edi,eax
	rol	edi,5
	mov	DWORD PTR [esp],esi
	lea	esi,DWORD PTR 2400959708[ebp*1+esi]
	mov	ebp,ecx
	add	esi,edi
	and	ebp,edx
	mov	edi,DWORD PTR 4[esp]
	add	esi,ebp
	; 40_59 49
	mov	ebp,ebx
	xor	edi,DWORD PTR 12[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 36[esp]
	and	ebp,eax
	xor	edi,DWORD PTR 56[esp]
	rol	edi,1
	add	ebp,edx
	ror	eax,2
	mov	edx,esi
	rol	edx,5
	mov	DWORD PTR 4[esp],edi
	lea	edi,DWORD PTR 2400959708[ebp*1+edi]
	mov	ebp,ebx
	add	edi,edx
	and	ebp,ecx
	mov	edx,DWORD PTR 8[esp]
	add	edi,ebp
	; 40_59 50
	mov	ebp,eax
	xor	edx,DWORD PTR 16[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 40[esp]
	and	ebp,esi
	xor	edx,DWORD PTR 60[esp]
	rol	edx,1
	add	ebp,ecx
	ror	esi,2
	mov	ecx,edi
	rol	ecx,5
	mov	DWORD PTR 8[esp],edx
	lea	edx,DWORD PTR 2400959708[ebp*1+edx]
	mov	ebp,eax
	add	edx,ecx
	and	ebp,ebx
	mov	ecx,DWORD PTR 12[esp]
	add	edx,ebp
	; 40_59 51
	mov	ebp,esi
	xor	ecx,DWORD PTR 20[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 44[esp]
	and	ebp,edi
	xor	ecx,DWORD PTR [esp]
	rol	ecx,1
	add	ebp,ebx
	ror	edi,2
	mov	ebx,edx
	rol	ebx,5
	mov	DWORD PTR 12[esp],ecx
	lea	ecx,DWORD PTR 2400959708[ebp*1+ecx]
	mov	ebp,esi
	add	ecx,ebx
	and	ebp,eax
	mov	ebx,DWORD PTR 16[esp]
	add	ecx,ebp
	; 40_59 52
	mov	ebp,edi
	xor	ebx,DWORD PTR 24[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 48[esp]
	and	ebp,edx
	xor	ebx,DWORD PTR 4[esp]
	rol	ebx,1
	add	ebp,eax
	ror	edx,2
	mov	eax,ecx
	rol	eax,5
	mov	DWORD PTR 16[esp],ebx
	lea	ebx,DWORD PTR 2400959708[ebp*1+ebx]
	mov	ebp,edi
	add	ebx,eax
	and	ebp,esi
	mov	eax,DWORD PTR 20[esp]
	add	ebx,ebp
	; 40_59 53
	mov	ebp,edx
	xor	eax,DWORD PTR 28[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 52[esp]
	and	ebp,ecx
	xor	eax,DWORD PTR 8[esp]
	rol	eax,1
	add	ebp,esi
	ror	ecx,2
	mov	esi,ebx
	rol	esi,5
	mov	DWORD PTR 20[esp],eax
	lea	eax,DWORD PTR 2400959708[ebp*1+eax]
	mov	ebp,edx
	add	eax,esi
	and	ebp,edi
	mov	esi,DWORD PTR 24[esp]
	add	eax,ebp
	; 40_59 54
	mov	ebp,ecx
	xor	esi,DWORD PTR 32[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 56[esp]
	and	ebp,ebx
	xor	esi,DWORD PTR 12[esp]
	rol	esi,1
	add	ebp,edi
	ror	ebx,2
	mov	edi,eax
	rol	edi,5
	mov	DWORD PTR 24[esp],esi
	lea	esi,DWORD PTR 2400959708[ebp*1+esi]
	mov	ebp,ecx
	add	esi,edi
	and	ebp,edx
	mov	edi,DWORD PTR 28[esp]
	add	esi,ebp
	; 40_59 55
	mov	ebp,ebx
	xor	edi,DWORD PTR 36[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 60[esp]
	and	ebp,eax
	xor	edi,DWORD PTR 16[esp]
	rol	edi,1
	add	ebp,edx
	ror	eax,2
	mov	edx,esi
	rol	edx,5
	mov	DWORD PTR 28[esp],edi
	lea	edi,DWORD PTR 2400959708[ebp*1+edi]
	mov	ebp,ebx
	add	edi,edx
	and	ebp,ecx
	mov	edx,DWORD PTR 32[esp]
	add	edi,ebp
	; 40_59 56
	mov	ebp,eax
	xor	edx,DWORD PTR 40[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR [esp]
	and	ebp,esi
	xor	edx,DWORD PTR 20[esp]
	rol	edx,1
	add	ebp,ecx
	ror	esi,2
	mov	ecx,edi
	rol	ecx,5
	mov	DWORD PTR 32[esp],edx
	lea	edx,DWORD PTR 2400959708[ebp*1+edx]
	mov	ebp,eax
	add	edx,ecx
	and	ebp,ebx
	mov	ecx,DWORD PTR 36[esp]
	add	edx,ebp
	; 40_59 57
	mov	ebp,esi
	xor	ecx,DWORD PTR 44[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 4[esp]
	and	ebp,edi
	xor	ecx,DWORD PTR 24[esp]
	rol	ecx,1
	add	ebp,ebx
	ror	edi,2
	mov	ebx,edx
	rol	ebx,5
	mov	DWORD PTR 36[esp],ecx
	lea	ecx,DWORD PTR 2400959708[ebp*1+ecx]
	mov	ebp,esi
	add	ecx,ebx
	and	ebp,eax
	mov	ebx,DWORD PTR 40[esp]
	add	ecx,ebp
	; 40_59 58
	mov	ebp,edi
	xor	ebx,DWORD PTR 48[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 8[esp]
	and	ebp,edx
	xor	ebx,DWORD PTR 28[esp]
	rol	ebx,1
	add	ebp,eax
	ror	edx,2
	mov	eax,ecx
	rol	eax,5
	mov	DWORD PTR 40[esp],ebx
	lea	ebx,DWORD PTR 2400959708[ebp*1+ebx]
	mov	ebp,edi
	add	ebx,eax
	and	ebp,esi
	mov	eax,DWORD PTR 44[esp]
	add	ebx,ebp
	; 40_59 59
	mov	ebp,edx
	xor	eax,DWORD PTR 52[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 12[esp]
	and	ebp,ecx
	xor	eax,DWORD PTR 32[esp]
	rol	eax,1
	add	ebp,esi
	ror	ecx,2
	mov	esi,ebx
	rol	esi,5
	mov	DWORD PTR 44[esp],eax
	lea	eax,DWORD PTR 2400959708[ebp*1+eax]
	mov	ebp,edx
	add	eax,esi
	and	ebp,edi
	mov	esi,DWORD PTR 48[esp]
	add	eax,ebp
	; 20_39 60
	mov	ebp,ebx
	xor	esi,DWORD PTR 56[esp]
	xor	ebp,ecx
	xor	esi,DWORD PTR 16[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 36[esp]
	rol	esi,1
	add	edi,ebp
	ror	ebx,2
	mov	ebp,eax
	rol	ebp,5
	mov	DWORD PTR 48[esp],esi
	lea	esi,DWORD PTR 3395469782[edi*1+esi]
	mov	edi,DWORD PTR 52[esp]
	add	esi,ebp
	; 20_39 61
	mov	ebp,eax
	xor	edi,DWORD PTR 60[esp]
	xor	ebp,ebx
	xor	edi,DWORD PTR 20[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 40[esp]
	rol	edi,1
	add	edx,ebp
	ror	eax,2
	mov	ebp,esi
	rol	ebp,5
	mov	DWORD PTR 52[esp],edi
	lea	edi,DWORD PTR 3395469782[edx*1+edi]
	mov	edx,DWORD PTR 56[esp]
	add	edi,ebp
	; 20_39 62
	mov	ebp,esi
	xor	edx,DWORD PTR [esp]
	xor	ebp,eax
	xor	edx,DWORD PTR 24[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 44[esp]
	rol	edx,1
	add	ecx,ebp
	ror	esi,2
	mov	ebp,edi
	rol	ebp,5
	mov	DWORD PTR 56[esp],edx
	lea	edx,DWORD PTR 3395469782[ecx*1+edx]
	mov	ecx,DWORD PTR 60[esp]
	add	edx,ebp
	; 20_39 63
	mov	ebp,edi
	xor	ecx,DWORD PTR 4[esp]
	xor	ebp,esi
	xor	ecx,DWORD PTR 28[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 48[esp]
	rol	ecx,1
	add	ebx,ebp
	ror	edi,2
	mov	ebp,edx
	rol	ebp,5
	mov	DWORD PTR 60[esp],ecx
	lea	ecx,DWORD PTR 3395469782[ebx*1+ecx]
	mov	ebx,DWORD PTR [esp]
	add	ecx,ebp
	; 20_39 64
	mov	ebp,edx
	xor	ebx,DWORD PTR 8[esp]
	xor	ebp,edi
	xor	ebx,DWORD PTR 32[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 52[esp]
	rol	ebx,1
	add	eax,ebp
	ror	edx,2
	mov	ebp,ecx
	rol	ebp,5
	mov	DWORD PTR [esp],ebx
	lea	ebx,DWORD PTR 3395469782[eax*1+ebx]
	mov	eax,DWORD PTR 4[esp]
	add	ebx,ebp
	; 20_39 65
	mov	ebp,ecx
	xor	eax,DWORD PTR 12[esp]
	xor	ebp,edx
	xor	eax,DWORD PTR 36[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 56[esp]
	rol	eax,1
	add	esi,ebp
	ror	ecx,2
	mov	ebp,ebx
	rol	ebp,5
	mov	DWORD PTR 4[esp],eax
	lea	eax,DWORD PTR 3395469782[esi*1+eax]
	mov	esi,DWORD PTR 8[esp]
	add	eax,ebp
	; 20_39 66
	mov	ebp,ebx
	xor	esi,DWORD PTR 16[esp]
	xor	ebp,ecx
	xor	esi,DWORD PTR 40[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 60[esp]
	rol	esi,1
	add	edi,ebp
	ror	ebx,2
	mov	ebp,eax
	rol	ebp,5
	mov	DWORD PTR 8[esp],esi
	lea	esi,DWORD PTR 3395469782[edi*1+esi]
	mov	edi,DWORD PTR 12[esp]
	add	esi,ebp
	; 20_39 67
	mov	ebp,eax
	xor	edi,DWORD PTR 20[esp]
	xor	ebp,ebx
	xor	edi,DWORD PTR 44[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR [esp]
	rol	edi,1
	add	edx,ebp
	ror	eax,2
	mov	ebp,esi
	rol	ebp,5
	mov	DWORD PTR 12[esp],edi
	lea	edi,DWORD PTR 3395469782[edx*1+edi]
	mov	edx,DWORD PTR 16[esp]
	add	edi,ebp
	; 20_39 68
	mov	ebp,esi
	xor	edx,DWORD PTR 24[esp]
	xor	ebp,eax
	xor	edx,DWORD PTR 48[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 4[esp]
	rol	edx,1
	add	ecx,ebp
	ror	esi,2
	mov	ebp,edi
	rol	ebp,5
	mov	DWORD PTR 16[esp],edx
	lea	edx,DWORD PTR 3395469782[ecx*1+edx]
	mov	ecx,DWORD PTR 20[esp]
	add	edx,ebp
	; 20_39 69
	mov	ebp,edi
	xor	ecx,DWORD PTR 28[esp]
	xor	ebp,esi
	xor	ecx,DWORD PTR 52[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 8[esp]
	rol	ecx,1
	add	ebx,ebp
	ror	edi,2
	mov	ebp,edx
	rol	ebp,5
	mov	DWORD PTR 20[esp],ecx
	lea	ecx,DWORD PTR 3395469782[ebx*1+ecx]
	mov	ebx,DWORD PTR 24[esp]
	add	ecx,ebp
	; 20_39 70
	mov	ebp,edx
	xor	ebx,DWORD PTR 32[esp]
	xor	ebp,edi
	xor	ebx,DWORD PTR 56[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 12[esp]
	rol	ebx,1
	add	eax,ebp
	ror	edx,2
	mov	ebp,ecx
	rol	ebp,5
	mov	DWORD PTR 24[esp],ebx
	lea	ebx,DWORD PTR 3395469782[eax*1+ebx]
	mov	eax,DWORD PTR 28[esp]
	add	ebx,ebp
	; 20_39 71
	mov	ebp,ecx
	xor	eax,DWORD PTR 36[esp]
	xor	ebp,edx
	xor	eax,DWORD PTR 60[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 16[esp]
	rol	eax,1
	add	esi,ebp
	ror	ecx,2
	mov	ebp,ebx
	rol	ebp,5
	mov	DWORD PTR 28[esp],eax
	lea	eax,DWORD PTR 3395469782[esi*1+eax]
	mov	esi,DWORD PTR 32[esp]
	add	eax,ebp
	; 20_39 72
	mov	ebp,ebx
	xor	esi,DWORD PTR 40[esp]
	xor	ebp,ecx
	xor	esi,DWORD PTR [esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 20[esp]
	rol	esi,1
	add	edi,ebp
	ror	ebx,2
	mov	ebp,eax
	rol	ebp,5
	mov	DWORD PTR 32[esp],esi
	lea	esi,DWORD PTR 3395469782[edi*1+esi]
	mov	edi,DWORD PTR 36[esp]
	add	esi,ebp
	; 20_39 73
	mov	ebp,eax
	xor	edi,DWORD PTR 44[esp]
	xor	ebp,ebx
	xor	edi,DWORD PTR 4[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 24[esp]
	rol	edi,1
	add	edx,ebp
	ror	eax,2
	mov	ebp,esi
	rol	ebp,5
	mov	DWORD PTR 36[esp],edi
	lea	edi,DWORD PTR 3395469782[edx*1+edi]
	mov	edx,DWORD PTR 40[esp]
	add	edi,ebp
	; 20_39 74
	mov	ebp,esi
	xor	edx,DWORD PTR 48[esp]
	xor	ebp,eax
	xor	edx,DWORD PTR 8[esp]
	xor	ebp,ebx
	xor	edx,DWORD PTR 28[esp]
	rol	edx,1
	add	ecx,ebp
	ror	esi,2
	mov	ebp,edi
	rol	ebp,5
	mov	DWORD PTR 40[esp],edx
	lea	edx,DWORD PTR 3395469782[ecx*1+edx]
	mov	ecx,DWORD PTR 44[esp]
	add	edx,ebp
	; 20_39 75
	mov	ebp,edi
	xor	ecx,DWORD PTR 52[esp]
	xor	ebp,esi
	xor	ecx,DWORD PTR 12[esp]
	xor	ebp,eax
	xor	ecx,DWORD PTR 32[esp]
	rol	ecx,1
	add	ebx,ebp
	ror	edi,2
	mov	ebp,edx
	rol	ebp,5
	mov	DWORD PTR 44[esp],ecx
	lea	ecx,DWORD PTR 3395469782[ebx*1+ecx]
	mov	ebx,DWORD PTR 48[esp]
	add	ecx,ebp
	; 20_39 76
	mov	ebp,edx
	xor	ebx,DWORD PTR 56[esp]
	xor	ebp,edi
	xor	ebx,DWORD PTR 16[esp]
	xor	ebp,esi
	xor	ebx,DWORD PTR 36[esp]
	rol	ebx,1
	add	eax,ebp
	ror	edx,2
	mov	ebp,ecx
	rol	ebp,5
	mov	DWORD PTR 48[esp],ebx
	lea	ebx,DWORD PTR 3395469782[eax*1+ebx]
	mov	eax,DWORD PTR 52[esp]
	add	ebx,ebp
	; 20_39 77
	mov	ebp,ecx
	xor	eax,DWORD PTR 60[esp]
	xor	ebp,edx
	xor	eax,DWORD PTR 20[esp]
	xor	ebp,edi
	xor	eax,DWORD PTR 40[esp]
	rol	eax,1
	add	esi,ebp
	ror	ecx,2
	mov	ebp,ebx
	rol	ebp,5
	lea	eax,DWORD PTR 3395469782[esi*1+eax]
	mov	esi,DWORD PTR 56[esp]
	add	eax,ebp
	; 20_39 78
	mov	ebp,ebx
	xor	esi,DWORD PTR [esp]
	xor	ebp,ecx
	xor	esi,DWORD PTR 24[esp]
	xor	ebp,edx
	xor	esi,DWORD PTR 44[esp]
	rol	esi,1
	add	edi,ebp
	ror	ebx,2
	mov	ebp,eax
	rol	ebp,5
	lea	esi,DWORD PTR 3395469782[edi*1+esi]
	mov	edi,DWORD PTR 60[esp]
	add	esi,ebp
	; 20_39 79
	mov	ebp,eax
	xor	edi,DWORD PTR 4[esp]
	xor	ebp,ebx
	xor	edi,DWORD PTR 28[esp]
	xor	ebp,ecx
	xor	edi,DWORD PTR 48[esp]
	rol	edi,1
	add	edx,ebp
	ror	eax,2
	mov	ebp,esi
	rol	ebp,5
	lea	edi,DWORD PTR 3395469782[edx*1+edi]
	add	edi,ebp
	mov	ebp,DWORD PTR 96[esp]
	mov	edx,DWORD PTR 100[esp]
	add	edi,DWORD PTR [ebp]
	add	esi,DWORD PTR 4[ebp]
	add	eax,DWORD PTR 8[ebp]
	add	ebx,DWORD PTR 12[ebp]
	add	ecx,DWORD PTR 16[ebp]
	mov	DWORD PTR [ebp],edi
	add	edx,64
	mov	DWORD PTR 4[ebp],esi
	cmp	edx,DWORD PTR 104[esp]
	mov	DWORD PTR 8[ebp],eax
	mov	edi,ecx
	mov	DWORD PTR 12[ebp],ebx
	mov	esi,edx
	mov	DWORD PTR 16[ebp],ecx
	jb	$L000loop
	add	esp,76
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_sha1_block_data_order ENDP
DB	83,72,65,49,32,98,108,111,99,107,32,116,114,97,110,115
DB	102,111,114,109,32,102,111,114,32,120,56,54,44,32,67,82
DB	89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112
DB	114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.text$	ENDS
END
