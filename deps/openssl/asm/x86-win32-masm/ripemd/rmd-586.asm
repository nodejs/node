TITLE	../openssl/crypto/ripemd/asm/rmd-586.asm
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
_ripemd160_block_asm_data_order	PROC PUBLIC
$L_ripemd160_block_asm_data_order_begin::
	mov	edx,DWORD PTR 4[esp]
	mov	eax,DWORD PTR 8[esp]
	push	esi
	mov	ecx,DWORD PTR [edx]
	push	edi
	mov	esi,DWORD PTR 4[edx]
	push	ebp
	mov	edi,DWORD PTR 8[edx]
	push	ebx
	sub	esp,108
$L000start:
	;

	mov	ebx,DWORD PTR [eax]
	mov	ebp,DWORD PTR 4[eax]
	mov	DWORD PTR [esp],ebx
	mov	DWORD PTR 4[esp],ebp
	mov	ebx,DWORD PTR 8[eax]
	mov	ebp,DWORD PTR 12[eax]
	mov	DWORD PTR 8[esp],ebx
	mov	DWORD PTR 12[esp],ebp
	mov	ebx,DWORD PTR 16[eax]
	mov	ebp,DWORD PTR 20[eax]
	mov	DWORD PTR 16[esp],ebx
	mov	DWORD PTR 20[esp],ebp
	mov	ebx,DWORD PTR 24[eax]
	mov	ebp,DWORD PTR 28[eax]
	mov	DWORD PTR 24[esp],ebx
	mov	DWORD PTR 28[esp],ebp
	mov	ebx,DWORD PTR 32[eax]
	mov	ebp,DWORD PTR 36[eax]
	mov	DWORD PTR 32[esp],ebx
	mov	DWORD PTR 36[esp],ebp
	mov	ebx,DWORD PTR 40[eax]
	mov	ebp,DWORD PTR 44[eax]
	mov	DWORD PTR 40[esp],ebx
	mov	DWORD PTR 44[esp],ebp
	mov	ebx,DWORD PTR 48[eax]
	mov	ebp,DWORD PTR 52[eax]
	mov	DWORD PTR 48[esp],ebx
	mov	DWORD PTR 52[esp],ebp
	mov	ebx,DWORD PTR 56[eax]
	mov	ebp,DWORD PTR 60[eax]
	mov	DWORD PTR 56[esp],ebx
	mov	DWORD PTR 60[esp],ebp
	mov	eax,edi
	mov	ebx,DWORD PTR 12[edx]
	mov	ebp,DWORD PTR 16[edx]
	; 0
	xor	eax,ebx
	mov	edx,DWORD PTR [esp]
	xor	eax,esi
	add	ecx,edx
	rol	edi,10
	add	ecx,eax
	mov	eax,esi
	rol	ecx,11
	add	ecx,ebp
	; 1
	xor	eax,edi
	mov	edx,DWORD PTR 4[esp]
	xor	eax,ecx
	add	ebp,eax
	mov	eax,ecx
	rol	esi,10
	add	ebp,edx
	xor	eax,esi
	rol	ebp,14
	add	ebp,ebx
	; 2
	mov	edx,DWORD PTR 8[esp]
	xor	eax,ebp
	add	ebx,edx
	rol	ecx,10
	add	ebx,eax
	mov	eax,ebp
	rol	ebx,15
	add	ebx,edi
	; 3
	xor	eax,ecx
	mov	edx,DWORD PTR 12[esp]
	xor	eax,ebx
	add	edi,eax
	mov	eax,ebx
	rol	ebp,10
	add	edi,edx
	xor	eax,ebp
	rol	edi,12
	add	edi,esi
	; 4
	mov	edx,DWORD PTR 16[esp]
	xor	eax,edi
	add	esi,edx
	rol	ebx,10
	add	esi,eax
	mov	eax,edi
	rol	esi,5
	add	esi,ecx
	; 5
	xor	eax,ebx
	mov	edx,DWORD PTR 20[esp]
	xor	eax,esi
	add	ecx,eax
	mov	eax,esi
	rol	edi,10
	add	ecx,edx
	xor	eax,edi
	rol	ecx,8
	add	ecx,ebp
	; 6
	mov	edx,DWORD PTR 24[esp]
	xor	eax,ecx
	add	ebp,edx
	rol	esi,10
	add	ebp,eax
	mov	eax,ecx
	rol	ebp,7
	add	ebp,ebx
	; 7
	xor	eax,esi
	mov	edx,DWORD PTR 28[esp]
	xor	eax,ebp
	add	ebx,eax
	mov	eax,ebp
	rol	ecx,10
	add	ebx,edx
	xor	eax,ecx
	rol	ebx,9
	add	ebx,edi
	; 8
	mov	edx,DWORD PTR 32[esp]
	xor	eax,ebx
	add	edi,edx
	rol	ebp,10
	add	edi,eax
	mov	eax,ebx
	rol	edi,11
	add	edi,esi
	; 9
	xor	eax,ebp
	mov	edx,DWORD PTR 36[esp]
	xor	eax,edi
	add	esi,eax
	mov	eax,edi
	rol	ebx,10
	add	esi,edx
	xor	eax,ebx
	rol	esi,13
	add	esi,ecx
	; 10
	mov	edx,DWORD PTR 40[esp]
	xor	eax,esi
	add	ecx,edx
	rol	edi,10
	add	ecx,eax
	mov	eax,esi
	rol	ecx,14
	add	ecx,ebp
	; 11
	xor	eax,edi
	mov	edx,DWORD PTR 44[esp]
	xor	eax,ecx
	add	ebp,eax
	mov	eax,ecx
	rol	esi,10
	add	ebp,edx
	xor	eax,esi
	rol	ebp,15
	add	ebp,ebx
	; 12
	mov	edx,DWORD PTR 48[esp]
	xor	eax,ebp
	add	ebx,edx
	rol	ecx,10
	add	ebx,eax
	mov	eax,ebp
	rol	ebx,6
	add	ebx,edi
	; 13
	xor	eax,ecx
	mov	edx,DWORD PTR 52[esp]
	xor	eax,ebx
	add	edi,eax
	mov	eax,ebx
	rol	ebp,10
	add	edi,edx
	xor	eax,ebp
	rol	edi,7
	add	edi,esi
	; 14
	mov	edx,DWORD PTR 56[esp]
	xor	eax,edi
	add	esi,edx
	rol	ebx,10
	add	esi,eax
	mov	eax,edi
	rol	esi,9
	add	esi,ecx
	; 15
	xor	eax,ebx
	mov	edx,DWORD PTR 60[esp]
	xor	eax,esi
	add	ecx,eax
	mov	eax,-1
	rol	edi,10
	add	ecx,edx
	mov	edx,DWORD PTR 28[esp]
	rol	ecx,8
	add	ecx,ebp
	; 16
	add	ebp,edx
	mov	edx,esi
	sub	eax,ecx
	and	edx,ecx
	and	eax,edi
	or	edx,eax
	mov	eax,DWORD PTR 16[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 1518500249[edx*1+ebp]
	mov	edx,-1
	rol	ebp,7
	add	ebp,ebx
	; 17
	add	ebx,eax
	mov	eax,ecx
	sub	edx,ebp
	and	eax,ebp
	and	edx,esi
	or	eax,edx
	mov	edx,DWORD PTR 52[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 1518500249[eax*1+ebx]
	mov	eax,-1
	rol	ebx,6
	add	ebx,edi
	; 18
	add	edi,edx
	mov	edx,ebp
	sub	eax,ebx
	and	edx,ebx
	and	eax,ecx
	or	edx,eax
	mov	eax,DWORD PTR 4[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 1518500249[edx*1+edi]
	mov	edx,-1
	rol	edi,8
	add	edi,esi
	; 19
	add	esi,eax
	mov	eax,ebx
	sub	edx,edi
	and	eax,edi
	and	edx,ebp
	or	eax,edx
	mov	edx,DWORD PTR 40[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 1518500249[eax*1+esi]
	mov	eax,-1
	rol	esi,13
	add	esi,ecx
	; 20
	add	ecx,edx
	mov	edx,edi
	sub	eax,esi
	and	edx,esi
	and	eax,ebx
	or	edx,eax
	mov	eax,DWORD PTR 24[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 1518500249[edx*1+ecx]
	mov	edx,-1
	rol	ecx,11
	add	ecx,ebp
	; 21
	add	ebp,eax
	mov	eax,esi
	sub	edx,ecx
	and	eax,ecx
	and	edx,edi
	or	eax,edx
	mov	edx,DWORD PTR 60[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 1518500249[eax*1+ebp]
	mov	eax,-1
	rol	ebp,9
	add	ebp,ebx
	; 22
	add	ebx,edx
	mov	edx,ecx
	sub	eax,ebp
	and	edx,ebp
	and	eax,esi
	or	edx,eax
	mov	eax,DWORD PTR 12[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 1518500249[edx*1+ebx]
	mov	edx,-1
	rol	ebx,7
	add	ebx,edi
	; 23
	add	edi,eax
	mov	eax,ebp
	sub	edx,ebx
	and	eax,ebx
	and	edx,ecx
	or	eax,edx
	mov	edx,DWORD PTR 48[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 1518500249[eax*1+edi]
	mov	eax,-1
	rol	edi,15
	add	edi,esi
	; 24
	add	esi,edx
	mov	edx,ebx
	sub	eax,edi
	and	edx,edi
	and	eax,ebp
	or	edx,eax
	mov	eax,DWORD PTR [esp]
	rol	ebx,10
	lea	esi,DWORD PTR 1518500249[edx*1+esi]
	mov	edx,-1
	rol	esi,7
	add	esi,ecx
	; 25
	add	ecx,eax
	mov	eax,edi
	sub	edx,esi
	and	eax,esi
	and	edx,ebx
	or	eax,edx
	mov	edx,DWORD PTR 36[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 1518500249[eax*1+ecx]
	mov	eax,-1
	rol	ecx,12
	add	ecx,ebp
	; 26
	add	ebp,edx
	mov	edx,esi
	sub	eax,ecx
	and	edx,ecx
	and	eax,edi
	or	edx,eax
	mov	eax,DWORD PTR 20[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 1518500249[edx*1+ebp]
	mov	edx,-1
	rol	ebp,15
	add	ebp,ebx
	; 27
	add	ebx,eax
	mov	eax,ecx
	sub	edx,ebp
	and	eax,ebp
	and	edx,esi
	or	eax,edx
	mov	edx,DWORD PTR 8[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 1518500249[eax*1+ebx]
	mov	eax,-1
	rol	ebx,9
	add	ebx,edi
	; 28
	add	edi,edx
	mov	edx,ebp
	sub	eax,ebx
	and	edx,ebx
	and	eax,ecx
	or	edx,eax
	mov	eax,DWORD PTR 56[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 1518500249[edx*1+edi]
	mov	edx,-1
	rol	edi,11
	add	edi,esi
	; 29
	add	esi,eax
	mov	eax,ebx
	sub	edx,edi
	and	eax,edi
	and	edx,ebp
	or	eax,edx
	mov	edx,DWORD PTR 44[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 1518500249[eax*1+esi]
	mov	eax,-1
	rol	esi,7
	add	esi,ecx
	; 30
	add	ecx,edx
	mov	edx,edi
	sub	eax,esi
	and	edx,esi
	and	eax,ebx
	or	edx,eax
	mov	eax,DWORD PTR 32[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 1518500249[edx*1+ecx]
	mov	edx,-1
	rol	ecx,13
	add	ecx,ebp
	; 31
	add	ebp,eax
	mov	eax,esi
	sub	edx,ecx
	and	eax,ecx
	and	edx,edi
	or	eax,edx
	mov	edx,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1518500249[eax*1+ebp]
	sub	edx,ecx
	rol	ebp,12
	add	ebp,ebx
	; 32
	mov	eax,DWORD PTR 12[esp]
	or	edx,ebp
	add	ebx,eax
	xor	edx,esi
	mov	eax,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1859775393[edx*1+ebx]
	sub	eax,ebp
	rol	ebx,11
	add	ebx,edi
	; 33
	mov	edx,DWORD PTR 40[esp]
	or	eax,ebx
	add	edi,edx
	xor	eax,ecx
	mov	edx,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1859775393[eax*1+edi]
	sub	edx,ebx
	rol	edi,13
	add	edi,esi
	; 34
	mov	eax,DWORD PTR 56[esp]
	or	edx,edi
	add	esi,eax
	xor	edx,ebp
	mov	eax,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1859775393[edx*1+esi]
	sub	eax,edi
	rol	esi,6
	add	esi,ecx
	; 35
	mov	edx,DWORD PTR 16[esp]
	or	eax,esi
	add	ecx,edx
	xor	eax,ebx
	mov	edx,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1859775393[eax*1+ecx]
	sub	edx,esi
	rol	ecx,7
	add	ecx,ebp
	; 36
	mov	eax,DWORD PTR 36[esp]
	or	edx,ecx
	add	ebp,eax
	xor	edx,edi
	mov	eax,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1859775393[edx*1+ebp]
	sub	eax,ecx
	rol	ebp,14
	add	ebp,ebx
	; 37
	mov	edx,DWORD PTR 60[esp]
	or	eax,ebp
	add	ebx,edx
	xor	eax,esi
	mov	edx,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1859775393[eax*1+ebx]
	sub	edx,ebp
	rol	ebx,9
	add	ebx,edi
	; 38
	mov	eax,DWORD PTR 32[esp]
	or	edx,ebx
	add	edi,eax
	xor	edx,ecx
	mov	eax,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1859775393[edx*1+edi]
	sub	eax,ebx
	rol	edi,13
	add	edi,esi
	; 39
	mov	edx,DWORD PTR 4[esp]
	or	eax,edi
	add	esi,edx
	xor	eax,ebp
	mov	edx,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1859775393[eax*1+esi]
	sub	edx,edi
	rol	esi,15
	add	esi,ecx
	; 40
	mov	eax,DWORD PTR 8[esp]
	or	edx,esi
	add	ecx,eax
	xor	edx,ebx
	mov	eax,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1859775393[edx*1+ecx]
	sub	eax,esi
	rol	ecx,14
	add	ecx,ebp
	; 41
	mov	edx,DWORD PTR 28[esp]
	or	eax,ecx
	add	ebp,edx
	xor	eax,edi
	mov	edx,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1859775393[eax*1+ebp]
	sub	edx,ecx
	rol	ebp,8
	add	ebp,ebx
	; 42
	mov	eax,DWORD PTR [esp]
	or	edx,ebp
	add	ebx,eax
	xor	edx,esi
	mov	eax,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1859775393[edx*1+ebx]
	sub	eax,ebp
	rol	ebx,13
	add	ebx,edi
	; 43
	mov	edx,DWORD PTR 24[esp]
	or	eax,ebx
	add	edi,edx
	xor	eax,ecx
	mov	edx,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1859775393[eax*1+edi]
	sub	edx,ebx
	rol	edi,6
	add	edi,esi
	; 44
	mov	eax,DWORD PTR 52[esp]
	or	edx,edi
	add	esi,eax
	xor	edx,ebp
	mov	eax,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1859775393[edx*1+esi]
	sub	eax,edi
	rol	esi,5
	add	esi,ecx
	; 45
	mov	edx,DWORD PTR 44[esp]
	or	eax,esi
	add	ecx,edx
	xor	eax,ebx
	mov	edx,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1859775393[eax*1+ecx]
	sub	edx,esi
	rol	ecx,12
	add	ecx,ebp
	; 46
	mov	eax,DWORD PTR 20[esp]
	or	edx,ecx
	add	ebp,eax
	xor	edx,edi
	mov	eax,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1859775393[edx*1+ebp]
	sub	eax,ecx
	rol	ebp,7
	add	ebp,ebx
	; 47
	mov	edx,DWORD PTR 48[esp]
	or	eax,ebp
	add	ebx,edx
	xor	eax,esi
	mov	edx,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1859775393[eax*1+ebx]
	mov	eax,ecx
	rol	ebx,5
	add	ebx,edi
	; 48
	sub	edx,ecx
	and	eax,ebx
	and	edx,ebp
	or	edx,eax
	mov	eax,DWORD PTR 4[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 2400959708[edx*1+edi]
	mov	edx,-1
	add	edi,eax
	mov	eax,ebp
	rol	edi,11
	add	edi,esi
	; 49
	sub	edx,ebp
	and	eax,edi
	and	edx,ebx
	or	edx,eax
	mov	eax,DWORD PTR 36[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 2400959708[edx*1+esi]
	mov	edx,-1
	add	esi,eax
	mov	eax,ebx
	rol	esi,12
	add	esi,ecx
	; 50
	sub	edx,ebx
	and	eax,esi
	and	edx,edi
	or	edx,eax
	mov	eax,DWORD PTR 44[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 2400959708[edx*1+ecx]
	mov	edx,-1
	add	ecx,eax
	mov	eax,edi
	rol	ecx,14
	add	ecx,ebp
	; 51
	sub	edx,edi
	and	eax,ecx
	and	edx,esi
	or	edx,eax
	mov	eax,DWORD PTR 40[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 2400959708[edx*1+ebp]
	mov	edx,-1
	add	ebp,eax
	mov	eax,esi
	rol	ebp,15
	add	ebp,ebx
	; 52
	sub	edx,esi
	and	eax,ebp
	and	edx,ecx
	or	edx,eax
	mov	eax,DWORD PTR [esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 2400959708[edx*1+ebx]
	mov	edx,-1
	add	ebx,eax
	mov	eax,ecx
	rol	ebx,14
	add	ebx,edi
	; 53
	sub	edx,ecx
	and	eax,ebx
	and	edx,ebp
	or	edx,eax
	mov	eax,DWORD PTR 32[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 2400959708[edx*1+edi]
	mov	edx,-1
	add	edi,eax
	mov	eax,ebp
	rol	edi,15
	add	edi,esi
	; 54
	sub	edx,ebp
	and	eax,edi
	and	edx,ebx
	or	edx,eax
	mov	eax,DWORD PTR 48[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 2400959708[edx*1+esi]
	mov	edx,-1
	add	esi,eax
	mov	eax,ebx
	rol	esi,9
	add	esi,ecx
	; 55
	sub	edx,ebx
	and	eax,esi
	and	edx,edi
	or	edx,eax
	mov	eax,DWORD PTR 16[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 2400959708[edx*1+ecx]
	mov	edx,-1
	add	ecx,eax
	mov	eax,edi
	rol	ecx,8
	add	ecx,ebp
	; 56
	sub	edx,edi
	and	eax,ecx
	and	edx,esi
	or	edx,eax
	mov	eax,DWORD PTR 52[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 2400959708[edx*1+ebp]
	mov	edx,-1
	add	ebp,eax
	mov	eax,esi
	rol	ebp,9
	add	ebp,ebx
	; 57
	sub	edx,esi
	and	eax,ebp
	and	edx,ecx
	or	edx,eax
	mov	eax,DWORD PTR 12[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 2400959708[edx*1+ebx]
	mov	edx,-1
	add	ebx,eax
	mov	eax,ecx
	rol	ebx,14
	add	ebx,edi
	; 58
	sub	edx,ecx
	and	eax,ebx
	and	edx,ebp
	or	edx,eax
	mov	eax,DWORD PTR 28[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 2400959708[edx*1+edi]
	mov	edx,-1
	add	edi,eax
	mov	eax,ebp
	rol	edi,5
	add	edi,esi
	; 59
	sub	edx,ebp
	and	eax,edi
	and	edx,ebx
	or	edx,eax
	mov	eax,DWORD PTR 60[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 2400959708[edx*1+esi]
	mov	edx,-1
	add	esi,eax
	mov	eax,ebx
	rol	esi,6
	add	esi,ecx
	; 60
	sub	edx,ebx
	and	eax,esi
	and	edx,edi
	or	edx,eax
	mov	eax,DWORD PTR 56[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 2400959708[edx*1+ecx]
	mov	edx,-1
	add	ecx,eax
	mov	eax,edi
	rol	ecx,8
	add	ecx,ebp
	; 61
	sub	edx,edi
	and	eax,ecx
	and	edx,esi
	or	edx,eax
	mov	eax,DWORD PTR 20[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 2400959708[edx*1+ebp]
	mov	edx,-1
	add	ebp,eax
	mov	eax,esi
	rol	ebp,6
	add	ebp,ebx
	; 62
	sub	edx,esi
	and	eax,ebp
	and	edx,ecx
	or	edx,eax
	mov	eax,DWORD PTR 24[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 2400959708[edx*1+ebx]
	mov	edx,-1
	add	ebx,eax
	mov	eax,ecx
	rol	ebx,5
	add	ebx,edi
	; 63
	sub	edx,ecx
	and	eax,ebx
	and	edx,ebp
	or	edx,eax
	mov	eax,DWORD PTR 8[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 2400959708[edx*1+edi]
	mov	edx,-1
	add	edi,eax
	sub	edx,ebp
	rol	edi,12
	add	edi,esi
	; 64
	mov	eax,DWORD PTR 16[esp]
	or	edx,ebx
	add	esi,eax
	xor	edx,edi
	mov	eax,-1
	rol	ebx,10
	lea	esi,DWORD PTR 2840853838[edx*1+esi]
	sub	eax,ebx
	rol	esi,9
	add	esi,ecx
	; 65
	mov	edx,DWORD PTR [esp]
	or	eax,edi
	add	ecx,edx
	xor	eax,esi
	mov	edx,-1
	rol	edi,10
	lea	ecx,DWORD PTR 2840853838[eax*1+ecx]
	sub	edx,edi
	rol	ecx,15
	add	ecx,ebp
	; 66
	mov	eax,DWORD PTR 20[esp]
	or	edx,esi
	add	ebp,eax
	xor	edx,ecx
	mov	eax,-1
	rol	esi,10
	lea	ebp,DWORD PTR 2840853838[edx*1+ebp]
	sub	eax,esi
	rol	ebp,5
	add	ebp,ebx
	; 67
	mov	edx,DWORD PTR 36[esp]
	or	eax,ecx
	add	ebx,edx
	xor	eax,ebp
	mov	edx,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 2840853838[eax*1+ebx]
	sub	edx,ecx
	rol	ebx,11
	add	ebx,edi
	; 68
	mov	eax,DWORD PTR 28[esp]
	or	edx,ebp
	add	edi,eax
	xor	edx,ebx
	mov	eax,-1
	rol	ebp,10
	lea	edi,DWORD PTR 2840853838[edx*1+edi]
	sub	eax,ebp
	rol	edi,6
	add	edi,esi
	; 69
	mov	edx,DWORD PTR 48[esp]
	or	eax,ebx
	add	esi,edx
	xor	eax,edi
	mov	edx,-1
	rol	ebx,10
	lea	esi,DWORD PTR 2840853838[eax*1+esi]
	sub	edx,ebx
	rol	esi,8
	add	esi,ecx
	; 70
	mov	eax,DWORD PTR 8[esp]
	or	edx,edi
	add	ecx,eax
	xor	edx,esi
	mov	eax,-1
	rol	edi,10
	lea	ecx,DWORD PTR 2840853838[edx*1+ecx]
	sub	eax,edi
	rol	ecx,13
	add	ecx,ebp
	; 71
	mov	edx,DWORD PTR 40[esp]
	or	eax,esi
	add	ebp,edx
	xor	eax,ecx
	mov	edx,-1
	rol	esi,10
	lea	ebp,DWORD PTR 2840853838[eax*1+ebp]
	sub	edx,esi
	rol	ebp,12
	add	ebp,ebx
	; 72
	mov	eax,DWORD PTR 56[esp]
	or	edx,ecx
	add	ebx,eax
	xor	edx,ebp
	mov	eax,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 2840853838[edx*1+ebx]
	sub	eax,ecx
	rol	ebx,5
	add	ebx,edi
	; 73
	mov	edx,DWORD PTR 4[esp]
	or	eax,ebp
	add	edi,edx
	xor	eax,ebx
	mov	edx,-1
	rol	ebp,10
	lea	edi,DWORD PTR 2840853838[eax*1+edi]
	sub	edx,ebp
	rol	edi,12
	add	edi,esi
	; 74
	mov	eax,DWORD PTR 12[esp]
	or	edx,ebx
	add	esi,eax
	xor	edx,edi
	mov	eax,-1
	rol	ebx,10
	lea	esi,DWORD PTR 2840853838[edx*1+esi]
	sub	eax,ebx
	rol	esi,13
	add	esi,ecx
	; 75
	mov	edx,DWORD PTR 32[esp]
	or	eax,edi
	add	ecx,edx
	xor	eax,esi
	mov	edx,-1
	rol	edi,10
	lea	ecx,DWORD PTR 2840853838[eax*1+ecx]
	sub	edx,edi
	rol	ecx,14
	add	ecx,ebp
	; 76
	mov	eax,DWORD PTR 44[esp]
	or	edx,esi
	add	ebp,eax
	xor	edx,ecx
	mov	eax,-1
	rol	esi,10
	lea	ebp,DWORD PTR 2840853838[edx*1+ebp]
	sub	eax,esi
	rol	ebp,11
	add	ebp,ebx
	; 77
	mov	edx,DWORD PTR 24[esp]
	or	eax,ecx
	add	ebx,edx
	xor	eax,ebp
	mov	edx,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 2840853838[eax*1+ebx]
	sub	edx,ecx
	rol	ebx,8
	add	ebx,edi
	; 78
	mov	eax,DWORD PTR 60[esp]
	or	edx,ebp
	add	edi,eax
	xor	edx,ebx
	mov	eax,-1
	rol	ebp,10
	lea	edi,DWORD PTR 2840853838[edx*1+edi]
	sub	eax,ebp
	rol	edi,5
	add	edi,esi
	; 79
	mov	edx,DWORD PTR 52[esp]
	or	eax,ebx
	add	esi,edx
	xor	eax,edi
	mov	edx,DWORD PTR 128[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 2840853838[eax*1+esi]
	mov	DWORD PTR 64[esp],ecx
	rol	esi,6
	add	esi,ecx
	mov	ecx,DWORD PTR [edx]
	mov	DWORD PTR 68[esp],esi
	mov	DWORD PTR 72[esp],edi
	mov	esi,DWORD PTR 4[edx]
	mov	DWORD PTR 76[esp],ebx
	mov	edi,DWORD PTR 8[edx]
	mov	DWORD PTR 80[esp],ebp
	mov	ebx,DWORD PTR 12[edx]
	mov	ebp,DWORD PTR 16[edx]
	; 80
	mov	edx,-1
	sub	edx,ebx
	mov	eax,DWORD PTR 20[esp]
	or	edx,edi
	add	ecx,eax
	xor	edx,esi
	mov	eax,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1352829926[edx*1+ecx]
	sub	eax,edi
	rol	ecx,8
	add	ecx,ebp
	; 81
	mov	edx,DWORD PTR 56[esp]
	or	eax,esi
	add	ebp,edx
	xor	eax,ecx
	mov	edx,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1352829926[eax*1+ebp]
	sub	edx,esi
	rol	ebp,9
	add	ebp,ebx
	; 82
	mov	eax,DWORD PTR 28[esp]
	or	edx,ecx
	add	ebx,eax
	xor	edx,ebp
	mov	eax,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1352829926[edx*1+ebx]
	sub	eax,ecx
	rol	ebx,9
	add	ebx,edi
	; 83
	mov	edx,DWORD PTR [esp]
	or	eax,ebp
	add	edi,edx
	xor	eax,ebx
	mov	edx,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1352829926[eax*1+edi]
	sub	edx,ebp
	rol	edi,11
	add	edi,esi
	; 84
	mov	eax,DWORD PTR 36[esp]
	or	edx,ebx
	add	esi,eax
	xor	edx,edi
	mov	eax,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1352829926[edx*1+esi]
	sub	eax,ebx
	rol	esi,13
	add	esi,ecx
	; 85
	mov	edx,DWORD PTR 8[esp]
	or	eax,edi
	add	ecx,edx
	xor	eax,esi
	mov	edx,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1352829926[eax*1+ecx]
	sub	edx,edi
	rol	ecx,15
	add	ecx,ebp
	; 86
	mov	eax,DWORD PTR 44[esp]
	or	edx,esi
	add	ebp,eax
	xor	edx,ecx
	mov	eax,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1352829926[edx*1+ebp]
	sub	eax,esi
	rol	ebp,15
	add	ebp,ebx
	; 87
	mov	edx,DWORD PTR 16[esp]
	or	eax,ecx
	add	ebx,edx
	xor	eax,ebp
	mov	edx,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1352829926[eax*1+ebx]
	sub	edx,ecx
	rol	ebx,5
	add	ebx,edi
	; 88
	mov	eax,DWORD PTR 52[esp]
	or	edx,ebp
	add	edi,eax
	xor	edx,ebx
	mov	eax,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1352829926[edx*1+edi]
	sub	eax,ebp
	rol	edi,7
	add	edi,esi
	; 89
	mov	edx,DWORD PTR 24[esp]
	or	eax,ebx
	add	esi,edx
	xor	eax,edi
	mov	edx,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1352829926[eax*1+esi]
	sub	edx,ebx
	rol	esi,7
	add	esi,ecx
	; 90
	mov	eax,DWORD PTR 60[esp]
	or	edx,edi
	add	ecx,eax
	xor	edx,esi
	mov	eax,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1352829926[edx*1+ecx]
	sub	eax,edi
	rol	ecx,8
	add	ecx,ebp
	; 91
	mov	edx,DWORD PTR 32[esp]
	or	eax,esi
	add	ebp,edx
	xor	eax,ecx
	mov	edx,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1352829926[eax*1+ebp]
	sub	edx,esi
	rol	ebp,11
	add	ebp,ebx
	; 92
	mov	eax,DWORD PTR 4[esp]
	or	edx,ecx
	add	ebx,eax
	xor	edx,ebp
	mov	eax,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1352829926[edx*1+ebx]
	sub	eax,ecx
	rol	ebx,14
	add	ebx,edi
	; 93
	mov	edx,DWORD PTR 40[esp]
	or	eax,ebp
	add	edi,edx
	xor	eax,ebx
	mov	edx,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1352829926[eax*1+edi]
	sub	edx,ebp
	rol	edi,14
	add	edi,esi
	; 94
	mov	eax,DWORD PTR 12[esp]
	or	edx,ebx
	add	esi,eax
	xor	edx,edi
	mov	eax,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1352829926[edx*1+esi]
	sub	eax,ebx
	rol	esi,12
	add	esi,ecx
	; 95
	mov	edx,DWORD PTR 48[esp]
	or	eax,edi
	add	ecx,edx
	xor	eax,esi
	mov	edx,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1352829926[eax*1+ecx]
	mov	eax,edi
	rol	ecx,6
	add	ecx,ebp
	; 96
	sub	edx,edi
	and	eax,ecx
	and	edx,esi
	or	edx,eax
	mov	eax,DWORD PTR 24[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 1548603684[edx*1+ebp]
	mov	edx,-1
	add	ebp,eax
	mov	eax,esi
	rol	ebp,9
	add	ebp,ebx
	; 97
	sub	edx,esi
	and	eax,ebp
	and	edx,ecx
	or	edx,eax
	mov	eax,DWORD PTR 44[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 1548603684[edx*1+ebx]
	mov	edx,-1
	add	ebx,eax
	mov	eax,ecx
	rol	ebx,13
	add	ebx,edi
	; 98
	sub	edx,ecx
	and	eax,ebx
	and	edx,ebp
	or	edx,eax
	mov	eax,DWORD PTR 12[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 1548603684[edx*1+edi]
	mov	edx,-1
	add	edi,eax
	mov	eax,ebp
	rol	edi,15
	add	edi,esi
	; 99
	sub	edx,ebp
	and	eax,edi
	and	edx,ebx
	or	edx,eax
	mov	eax,DWORD PTR 28[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 1548603684[edx*1+esi]
	mov	edx,-1
	add	esi,eax
	mov	eax,ebx
	rol	esi,7
	add	esi,ecx
	; 100
	sub	edx,ebx
	and	eax,esi
	and	edx,edi
	or	edx,eax
	mov	eax,DWORD PTR [esp]
	rol	edi,10
	lea	ecx,DWORD PTR 1548603684[edx*1+ecx]
	mov	edx,-1
	add	ecx,eax
	mov	eax,edi
	rol	ecx,12
	add	ecx,ebp
	; 101
	sub	edx,edi
	and	eax,ecx
	and	edx,esi
	or	edx,eax
	mov	eax,DWORD PTR 52[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 1548603684[edx*1+ebp]
	mov	edx,-1
	add	ebp,eax
	mov	eax,esi
	rol	ebp,8
	add	ebp,ebx
	; 102
	sub	edx,esi
	and	eax,ebp
	and	edx,ecx
	or	edx,eax
	mov	eax,DWORD PTR 20[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 1548603684[edx*1+ebx]
	mov	edx,-1
	add	ebx,eax
	mov	eax,ecx
	rol	ebx,9
	add	ebx,edi
	; 103
	sub	edx,ecx
	and	eax,ebx
	and	edx,ebp
	or	edx,eax
	mov	eax,DWORD PTR 40[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 1548603684[edx*1+edi]
	mov	edx,-1
	add	edi,eax
	mov	eax,ebp
	rol	edi,11
	add	edi,esi
	; 104
	sub	edx,ebp
	and	eax,edi
	and	edx,ebx
	or	edx,eax
	mov	eax,DWORD PTR 56[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 1548603684[edx*1+esi]
	mov	edx,-1
	add	esi,eax
	mov	eax,ebx
	rol	esi,7
	add	esi,ecx
	; 105
	sub	edx,ebx
	and	eax,esi
	and	edx,edi
	or	edx,eax
	mov	eax,DWORD PTR 60[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 1548603684[edx*1+ecx]
	mov	edx,-1
	add	ecx,eax
	mov	eax,edi
	rol	ecx,7
	add	ecx,ebp
	; 106
	sub	edx,edi
	and	eax,ecx
	and	edx,esi
	or	edx,eax
	mov	eax,DWORD PTR 32[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 1548603684[edx*1+ebp]
	mov	edx,-1
	add	ebp,eax
	mov	eax,esi
	rol	ebp,12
	add	ebp,ebx
	; 107
	sub	edx,esi
	and	eax,ebp
	and	edx,ecx
	or	edx,eax
	mov	eax,DWORD PTR 48[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 1548603684[edx*1+ebx]
	mov	edx,-1
	add	ebx,eax
	mov	eax,ecx
	rol	ebx,7
	add	ebx,edi
	; 108
	sub	edx,ecx
	and	eax,ebx
	and	edx,ebp
	or	edx,eax
	mov	eax,DWORD PTR 16[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 1548603684[edx*1+edi]
	mov	edx,-1
	add	edi,eax
	mov	eax,ebp
	rol	edi,6
	add	edi,esi
	; 109
	sub	edx,ebp
	and	eax,edi
	and	edx,ebx
	or	edx,eax
	mov	eax,DWORD PTR 36[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 1548603684[edx*1+esi]
	mov	edx,-1
	add	esi,eax
	mov	eax,ebx
	rol	esi,15
	add	esi,ecx
	; 110
	sub	edx,ebx
	and	eax,esi
	and	edx,edi
	or	edx,eax
	mov	eax,DWORD PTR 4[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 1548603684[edx*1+ecx]
	mov	edx,-1
	add	ecx,eax
	mov	eax,edi
	rol	ecx,13
	add	ecx,ebp
	; 111
	sub	edx,edi
	and	eax,ecx
	and	edx,esi
	or	edx,eax
	mov	eax,DWORD PTR 8[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 1548603684[edx*1+ebp]
	mov	edx,-1
	add	ebp,eax
	sub	edx,ecx
	rol	ebp,11
	add	ebp,ebx
	; 112
	mov	eax,DWORD PTR 60[esp]
	or	edx,ebp
	add	ebx,eax
	xor	edx,esi
	mov	eax,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1836072691[edx*1+ebx]
	sub	eax,ebp
	rol	ebx,9
	add	ebx,edi
	; 113
	mov	edx,DWORD PTR 20[esp]
	or	eax,ebx
	add	edi,edx
	xor	eax,ecx
	mov	edx,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1836072691[eax*1+edi]
	sub	edx,ebx
	rol	edi,7
	add	edi,esi
	; 114
	mov	eax,DWORD PTR 4[esp]
	or	edx,edi
	add	esi,eax
	xor	edx,ebp
	mov	eax,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1836072691[edx*1+esi]
	sub	eax,edi
	rol	esi,15
	add	esi,ecx
	; 115
	mov	edx,DWORD PTR 12[esp]
	or	eax,esi
	add	ecx,edx
	xor	eax,ebx
	mov	edx,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1836072691[eax*1+ecx]
	sub	edx,esi
	rol	ecx,11
	add	ecx,ebp
	; 116
	mov	eax,DWORD PTR 28[esp]
	or	edx,ecx
	add	ebp,eax
	xor	edx,edi
	mov	eax,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1836072691[edx*1+ebp]
	sub	eax,ecx
	rol	ebp,8
	add	ebp,ebx
	; 117
	mov	edx,DWORD PTR 56[esp]
	or	eax,ebp
	add	ebx,edx
	xor	eax,esi
	mov	edx,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1836072691[eax*1+ebx]
	sub	edx,ebp
	rol	ebx,6
	add	ebx,edi
	; 118
	mov	eax,DWORD PTR 24[esp]
	or	edx,ebx
	add	edi,eax
	xor	edx,ecx
	mov	eax,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1836072691[edx*1+edi]
	sub	eax,ebx
	rol	edi,6
	add	edi,esi
	; 119
	mov	edx,DWORD PTR 36[esp]
	or	eax,edi
	add	esi,edx
	xor	eax,ebp
	mov	edx,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1836072691[eax*1+esi]
	sub	edx,edi
	rol	esi,14
	add	esi,ecx
	; 120
	mov	eax,DWORD PTR 44[esp]
	or	edx,esi
	add	ecx,eax
	xor	edx,ebx
	mov	eax,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1836072691[edx*1+ecx]
	sub	eax,esi
	rol	ecx,12
	add	ecx,ebp
	; 121
	mov	edx,DWORD PTR 32[esp]
	or	eax,ecx
	add	ebp,edx
	xor	eax,edi
	mov	edx,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1836072691[eax*1+ebp]
	sub	edx,ecx
	rol	ebp,13
	add	ebp,ebx
	; 122
	mov	eax,DWORD PTR 48[esp]
	or	edx,ebp
	add	ebx,eax
	xor	edx,esi
	mov	eax,-1
	rol	ecx,10
	lea	ebx,DWORD PTR 1836072691[edx*1+ebx]
	sub	eax,ebp
	rol	ebx,5
	add	ebx,edi
	; 123
	mov	edx,DWORD PTR 8[esp]
	or	eax,ebx
	add	edi,edx
	xor	eax,ecx
	mov	edx,-1
	rol	ebp,10
	lea	edi,DWORD PTR 1836072691[eax*1+edi]
	sub	edx,ebx
	rol	edi,14
	add	edi,esi
	; 124
	mov	eax,DWORD PTR 40[esp]
	or	edx,edi
	add	esi,eax
	xor	edx,ebp
	mov	eax,-1
	rol	ebx,10
	lea	esi,DWORD PTR 1836072691[edx*1+esi]
	sub	eax,edi
	rol	esi,13
	add	esi,ecx
	; 125
	mov	edx,DWORD PTR [esp]
	or	eax,esi
	add	ecx,edx
	xor	eax,ebx
	mov	edx,-1
	rol	edi,10
	lea	ecx,DWORD PTR 1836072691[eax*1+ecx]
	sub	edx,esi
	rol	ecx,13
	add	ecx,ebp
	; 126
	mov	eax,DWORD PTR 16[esp]
	or	edx,ecx
	add	ebp,eax
	xor	edx,edi
	mov	eax,-1
	rol	esi,10
	lea	ebp,DWORD PTR 1836072691[edx*1+ebp]
	sub	eax,ecx
	rol	ebp,7
	add	ebp,ebx
	; 127
	mov	edx,DWORD PTR 52[esp]
	or	eax,ebp
	add	ebx,edx
	xor	eax,esi
	mov	edx,DWORD PTR 32[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 1836072691[eax*1+ebx]
	mov	eax,-1
	rol	ebx,5
	add	ebx,edi
	; 128
	add	edi,edx
	mov	edx,ebp
	sub	eax,ebx
	and	edx,ebx
	and	eax,ecx
	or	edx,eax
	mov	eax,DWORD PTR 24[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 2053994217[edx*1+edi]
	mov	edx,-1
	rol	edi,15
	add	edi,esi
	; 129
	add	esi,eax
	mov	eax,ebx
	sub	edx,edi
	and	eax,edi
	and	edx,ebp
	or	eax,edx
	mov	edx,DWORD PTR 16[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 2053994217[eax*1+esi]
	mov	eax,-1
	rol	esi,5
	add	esi,ecx
	; 130
	add	ecx,edx
	mov	edx,edi
	sub	eax,esi
	and	edx,esi
	and	eax,ebx
	or	edx,eax
	mov	eax,DWORD PTR 4[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 2053994217[edx*1+ecx]
	mov	edx,-1
	rol	ecx,8
	add	ecx,ebp
	; 131
	add	ebp,eax
	mov	eax,esi
	sub	edx,ecx
	and	eax,ecx
	and	edx,edi
	or	eax,edx
	mov	edx,DWORD PTR 12[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 2053994217[eax*1+ebp]
	mov	eax,-1
	rol	ebp,11
	add	ebp,ebx
	; 132
	add	ebx,edx
	mov	edx,ecx
	sub	eax,ebp
	and	edx,ebp
	and	eax,esi
	or	edx,eax
	mov	eax,DWORD PTR 44[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 2053994217[edx*1+ebx]
	mov	edx,-1
	rol	ebx,14
	add	ebx,edi
	; 133
	add	edi,eax
	mov	eax,ebp
	sub	edx,ebx
	and	eax,ebx
	and	edx,ecx
	or	eax,edx
	mov	edx,DWORD PTR 60[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 2053994217[eax*1+edi]
	mov	eax,-1
	rol	edi,14
	add	edi,esi
	; 134
	add	esi,edx
	mov	edx,ebx
	sub	eax,edi
	and	edx,edi
	and	eax,ebp
	or	edx,eax
	mov	eax,DWORD PTR [esp]
	rol	ebx,10
	lea	esi,DWORD PTR 2053994217[edx*1+esi]
	mov	edx,-1
	rol	esi,6
	add	esi,ecx
	; 135
	add	ecx,eax
	mov	eax,edi
	sub	edx,esi
	and	eax,esi
	and	edx,ebx
	or	eax,edx
	mov	edx,DWORD PTR 20[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 2053994217[eax*1+ecx]
	mov	eax,-1
	rol	ecx,14
	add	ecx,ebp
	; 136
	add	ebp,edx
	mov	edx,esi
	sub	eax,ecx
	and	edx,ecx
	and	eax,edi
	or	edx,eax
	mov	eax,DWORD PTR 48[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 2053994217[edx*1+ebp]
	mov	edx,-1
	rol	ebp,6
	add	ebp,ebx
	; 137
	add	ebx,eax
	mov	eax,ecx
	sub	edx,ebp
	and	eax,ebp
	and	edx,esi
	or	eax,edx
	mov	edx,DWORD PTR 8[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 2053994217[eax*1+ebx]
	mov	eax,-1
	rol	ebx,9
	add	ebx,edi
	; 138
	add	edi,edx
	mov	edx,ebp
	sub	eax,ebx
	and	edx,ebx
	and	eax,ecx
	or	edx,eax
	mov	eax,DWORD PTR 52[esp]
	rol	ebp,10
	lea	edi,DWORD PTR 2053994217[edx*1+edi]
	mov	edx,-1
	rol	edi,12
	add	edi,esi
	; 139
	add	esi,eax
	mov	eax,ebx
	sub	edx,edi
	and	eax,edi
	and	edx,ebp
	or	eax,edx
	mov	edx,DWORD PTR 36[esp]
	rol	ebx,10
	lea	esi,DWORD PTR 2053994217[eax*1+esi]
	mov	eax,-1
	rol	esi,9
	add	esi,ecx
	; 140
	add	ecx,edx
	mov	edx,edi
	sub	eax,esi
	and	edx,esi
	and	eax,ebx
	or	edx,eax
	mov	eax,DWORD PTR 28[esp]
	rol	edi,10
	lea	ecx,DWORD PTR 2053994217[edx*1+ecx]
	mov	edx,-1
	rol	ecx,12
	add	ecx,ebp
	; 141
	add	ebp,eax
	mov	eax,esi
	sub	edx,ecx
	and	eax,ecx
	and	edx,edi
	or	eax,edx
	mov	edx,DWORD PTR 40[esp]
	rol	esi,10
	lea	ebp,DWORD PTR 2053994217[eax*1+ebp]
	mov	eax,-1
	rol	ebp,5
	add	ebp,ebx
	; 142
	add	ebx,edx
	mov	edx,ecx
	sub	eax,ebp
	and	edx,ebp
	and	eax,esi
	or	edx,eax
	mov	eax,DWORD PTR 56[esp]
	rol	ecx,10
	lea	ebx,DWORD PTR 2053994217[edx*1+ebx]
	mov	edx,-1
	rol	ebx,15
	add	ebx,edi
	; 143
	add	edi,eax
	mov	eax,ebp
	sub	edx,ebx
	and	eax,ebx
	and	edx,ecx
	or	edx,eax
	mov	eax,ebx
	rol	ebp,10
	lea	edi,DWORD PTR 2053994217[edx*1+edi]
	xor	eax,ebp
	rol	edi,8
	add	edi,esi
	; 144
	mov	edx,DWORD PTR 48[esp]
	xor	eax,edi
	add	esi,edx
	rol	ebx,10
	add	esi,eax
	mov	eax,edi
	rol	esi,8
	add	esi,ecx
	; 145
	xor	eax,ebx
	mov	edx,DWORD PTR 60[esp]
	xor	eax,esi
	add	ecx,eax
	mov	eax,esi
	rol	edi,10
	add	ecx,edx
	xor	eax,edi
	rol	ecx,5
	add	ecx,ebp
	; 146
	mov	edx,DWORD PTR 40[esp]
	xor	eax,ecx
	add	ebp,edx
	rol	esi,10
	add	ebp,eax
	mov	eax,ecx
	rol	ebp,12
	add	ebp,ebx
	; 147
	xor	eax,esi
	mov	edx,DWORD PTR 16[esp]
	xor	eax,ebp
	add	ebx,eax
	mov	eax,ebp
	rol	ecx,10
	add	ebx,edx
	xor	eax,ecx
	rol	ebx,9
	add	ebx,edi
	; 148
	mov	edx,DWORD PTR 4[esp]
	xor	eax,ebx
	add	edi,edx
	rol	ebp,10
	add	edi,eax
	mov	eax,ebx
	rol	edi,12
	add	edi,esi
	; 149
	xor	eax,ebp
	mov	edx,DWORD PTR 20[esp]
	xor	eax,edi
	add	esi,eax
	mov	eax,edi
	rol	ebx,10
	add	esi,edx
	xor	eax,ebx
	rol	esi,5
	add	esi,ecx
	; 150
	mov	edx,DWORD PTR 32[esp]
	xor	eax,esi
	add	ecx,edx
	rol	edi,10
	add	ecx,eax
	mov	eax,esi
	rol	ecx,14
	add	ecx,ebp
	; 151
	xor	eax,edi
	mov	edx,DWORD PTR 28[esp]
	xor	eax,ecx
	add	ebp,eax
	mov	eax,ecx
	rol	esi,10
	add	ebp,edx
	xor	eax,esi
	rol	ebp,6
	add	ebp,ebx
	; 152
	mov	edx,DWORD PTR 24[esp]
	xor	eax,ebp
	add	ebx,edx
	rol	ecx,10
	add	ebx,eax
	mov	eax,ebp
	rol	ebx,8
	add	ebx,edi
	; 153
	xor	eax,ecx
	mov	edx,DWORD PTR 8[esp]
	xor	eax,ebx
	add	edi,eax
	mov	eax,ebx
	rol	ebp,10
	add	edi,edx
	xor	eax,ebp
	rol	edi,13
	add	edi,esi
	; 154
	mov	edx,DWORD PTR 52[esp]
	xor	eax,edi
	add	esi,edx
	rol	ebx,10
	add	esi,eax
	mov	eax,edi
	rol	esi,6
	add	esi,ecx
	; 155
	xor	eax,ebx
	mov	edx,DWORD PTR 56[esp]
	xor	eax,esi
	add	ecx,eax
	mov	eax,esi
	rol	edi,10
	add	ecx,edx
	xor	eax,edi
	rol	ecx,5
	add	ecx,ebp
	; 156
	mov	edx,DWORD PTR [esp]
	xor	eax,ecx
	add	ebp,edx
	rol	esi,10
	add	ebp,eax
	mov	eax,ecx
	rol	ebp,15
	add	ebp,ebx
	; 157
	xor	eax,esi
	mov	edx,DWORD PTR 12[esp]
	xor	eax,ebp
	add	ebx,eax
	mov	eax,ebp
	rol	ecx,10
	add	ebx,edx
	xor	eax,ecx
	rol	ebx,13
	add	ebx,edi
	; 158
	mov	edx,DWORD PTR 36[esp]
	xor	eax,ebx
	add	edi,edx
	rol	ebp,10
	add	edi,eax
	mov	eax,ebx
	rol	edi,11
	add	edi,esi
	; 159
	xor	eax,ebp
	mov	edx,DWORD PTR 44[esp]
	xor	eax,edi
	add	esi,eax
	rol	ebx,10
	add	esi,edx
	mov	edx,DWORD PTR 128[esp]
	rol	esi,11
	add	esi,ecx
	mov	eax,DWORD PTR 4[edx]
	add	ebx,eax
	mov	eax,DWORD PTR 72[esp]
	add	ebx,eax
	mov	eax,DWORD PTR 8[edx]
	add	ebp,eax
	mov	eax,DWORD PTR 76[esp]
	add	ebp,eax
	mov	eax,DWORD PTR 12[edx]
	add	ecx,eax
	mov	eax,DWORD PTR 80[esp]
	add	ecx,eax
	mov	eax,DWORD PTR 16[edx]
	add	esi,eax
	mov	eax,DWORD PTR 64[esp]
	add	esi,eax
	mov	eax,DWORD PTR [edx]
	add	edi,eax
	mov	eax,DWORD PTR 68[esp]
	add	edi,eax
	mov	eax,DWORD PTR 136[esp]
	mov	DWORD PTR [edx],ebx
	mov	DWORD PTR 4[edx],ebp
	mov	DWORD PTR 8[edx],ecx
	sub	eax,1
	mov	DWORD PTR 12[edx],esi
	mov	DWORD PTR 16[edx],edi
	jle	$L001get_out
	mov	DWORD PTR 136[esp],eax
	mov	edi,ecx
	mov	eax,DWORD PTR 132[esp]
	mov	ecx,ebx
	add	eax,64
	mov	esi,ebp
	mov	DWORD PTR 132[esp],eax
	jmp	$L000start
$L001get_out:
	add	esp,108
	pop	ebx
	pop	ebp
	pop	edi
	pop	esi
	ret
_ripemd160_block_asm_data_order ENDP
.text$	ENDS
END
