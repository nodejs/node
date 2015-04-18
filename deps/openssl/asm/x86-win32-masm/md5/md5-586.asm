TITLE	../openssl/crypto/md5/asm/md5-586.asm
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
_md5_block_asm_data_order	PROC PUBLIC
$L_md5_block_asm_data_order_begin::
	push	esi
	push	edi
	mov	edi,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 16[esp]
	mov	ecx,DWORD PTR 20[esp]
	push	ebp
	shl	ecx,6
	push	ebx
	add	ecx,esi
	sub	ecx,64
	mov	eax,DWORD PTR [edi]
	push	ecx
	mov	ebx,DWORD PTR 4[edi]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
$L000start:
	;

	; R0 section
	mov	edi,ecx
	mov	ebp,DWORD PTR [esi]
	; R0 0
	xor	edi,edx
	and	edi,ebx
	lea	eax,DWORD PTR 3614090360[ebp*1+eax]
	xor	edi,edx
	add	eax,edi
	mov	edi,ebx
	rol	eax,7
	mov	ebp,DWORD PTR 4[esi]
	add	eax,ebx
	; R0 1
	xor	edi,ecx
	and	edi,eax
	lea	edx,DWORD PTR 3905402710[ebp*1+edx]
	xor	edi,ecx
	add	edx,edi
	mov	edi,eax
	rol	edx,12
	mov	ebp,DWORD PTR 8[esi]
	add	edx,eax
	; R0 2
	xor	edi,ebx
	and	edi,edx
	lea	ecx,DWORD PTR 606105819[ebp*1+ecx]
	xor	edi,ebx
	add	ecx,edi
	mov	edi,edx
	rol	ecx,17
	mov	ebp,DWORD PTR 12[esi]
	add	ecx,edx
	; R0 3
	xor	edi,eax
	and	edi,ecx
	lea	ebx,DWORD PTR 3250441966[ebp*1+ebx]
	xor	edi,eax
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,22
	mov	ebp,DWORD PTR 16[esi]
	add	ebx,ecx
	; R0 4
	xor	edi,edx
	and	edi,ebx
	lea	eax,DWORD PTR 4118548399[ebp*1+eax]
	xor	edi,edx
	add	eax,edi
	mov	edi,ebx
	rol	eax,7
	mov	ebp,DWORD PTR 20[esi]
	add	eax,ebx
	; R0 5
	xor	edi,ecx
	and	edi,eax
	lea	edx,DWORD PTR 1200080426[ebp*1+edx]
	xor	edi,ecx
	add	edx,edi
	mov	edi,eax
	rol	edx,12
	mov	ebp,DWORD PTR 24[esi]
	add	edx,eax
	; R0 6
	xor	edi,ebx
	and	edi,edx
	lea	ecx,DWORD PTR 2821735955[ebp*1+ecx]
	xor	edi,ebx
	add	ecx,edi
	mov	edi,edx
	rol	ecx,17
	mov	ebp,DWORD PTR 28[esi]
	add	ecx,edx
	; R0 7
	xor	edi,eax
	and	edi,ecx
	lea	ebx,DWORD PTR 4249261313[ebp*1+ebx]
	xor	edi,eax
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,22
	mov	ebp,DWORD PTR 32[esi]
	add	ebx,ecx
	; R0 8
	xor	edi,edx
	and	edi,ebx
	lea	eax,DWORD PTR 1770035416[ebp*1+eax]
	xor	edi,edx
	add	eax,edi
	mov	edi,ebx
	rol	eax,7
	mov	ebp,DWORD PTR 36[esi]
	add	eax,ebx
	; R0 9
	xor	edi,ecx
	and	edi,eax
	lea	edx,DWORD PTR 2336552879[ebp*1+edx]
	xor	edi,ecx
	add	edx,edi
	mov	edi,eax
	rol	edx,12
	mov	ebp,DWORD PTR 40[esi]
	add	edx,eax
	; R0 10
	xor	edi,ebx
	and	edi,edx
	lea	ecx,DWORD PTR 4294925233[ebp*1+ecx]
	xor	edi,ebx
	add	ecx,edi
	mov	edi,edx
	rol	ecx,17
	mov	ebp,DWORD PTR 44[esi]
	add	ecx,edx
	; R0 11
	xor	edi,eax
	and	edi,ecx
	lea	ebx,DWORD PTR 2304563134[ebp*1+ebx]
	xor	edi,eax
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,22
	mov	ebp,DWORD PTR 48[esi]
	add	ebx,ecx
	; R0 12
	xor	edi,edx
	and	edi,ebx
	lea	eax,DWORD PTR 1804603682[ebp*1+eax]
	xor	edi,edx
	add	eax,edi
	mov	edi,ebx
	rol	eax,7
	mov	ebp,DWORD PTR 52[esi]
	add	eax,ebx
	; R0 13
	xor	edi,ecx
	and	edi,eax
	lea	edx,DWORD PTR 4254626195[ebp*1+edx]
	xor	edi,ecx
	add	edx,edi
	mov	edi,eax
	rol	edx,12
	mov	ebp,DWORD PTR 56[esi]
	add	edx,eax
	; R0 14
	xor	edi,ebx
	and	edi,edx
	lea	ecx,DWORD PTR 2792965006[ebp*1+ecx]
	xor	edi,ebx
	add	ecx,edi
	mov	edi,edx
	rol	ecx,17
	mov	ebp,DWORD PTR 60[esi]
	add	ecx,edx
	; R0 15
	xor	edi,eax
	and	edi,ecx
	lea	ebx,DWORD PTR 1236535329[ebp*1+ebx]
	xor	edi,eax
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,22
	mov	ebp,DWORD PTR 4[esi]
	add	ebx,ecx
	;

	; R1 section
	; R1 16
	lea	eax,DWORD PTR 4129170786[ebp*1+eax]
	xor	edi,ebx
	and	edi,edx
	mov	ebp,DWORD PTR 24[esi]
	xor	edi,ecx
	add	eax,edi
	mov	edi,ebx
	rol	eax,5
	add	eax,ebx
	; R1 17
	lea	edx,DWORD PTR 3225465664[ebp*1+edx]
	xor	edi,eax
	and	edi,ecx
	mov	ebp,DWORD PTR 44[esi]
	xor	edi,ebx
	add	edx,edi
	mov	edi,eax
	rol	edx,9
	add	edx,eax
	; R1 18
	lea	ecx,DWORD PTR 643717713[ebp*1+ecx]
	xor	edi,edx
	and	edi,ebx
	mov	ebp,DWORD PTR [esi]
	xor	edi,eax
	add	ecx,edi
	mov	edi,edx
	rol	ecx,14
	add	ecx,edx
	; R1 19
	lea	ebx,DWORD PTR 3921069994[ebp*1+ebx]
	xor	edi,ecx
	and	edi,eax
	mov	ebp,DWORD PTR 20[esi]
	xor	edi,edx
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,20
	add	ebx,ecx
	; R1 20
	lea	eax,DWORD PTR 3593408605[ebp*1+eax]
	xor	edi,ebx
	and	edi,edx
	mov	ebp,DWORD PTR 40[esi]
	xor	edi,ecx
	add	eax,edi
	mov	edi,ebx
	rol	eax,5
	add	eax,ebx
	; R1 21
	lea	edx,DWORD PTR 38016083[ebp*1+edx]
	xor	edi,eax
	and	edi,ecx
	mov	ebp,DWORD PTR 60[esi]
	xor	edi,ebx
	add	edx,edi
	mov	edi,eax
	rol	edx,9
	add	edx,eax
	; R1 22
	lea	ecx,DWORD PTR 3634488961[ebp*1+ecx]
	xor	edi,edx
	and	edi,ebx
	mov	ebp,DWORD PTR 16[esi]
	xor	edi,eax
	add	ecx,edi
	mov	edi,edx
	rol	ecx,14
	add	ecx,edx
	; R1 23
	lea	ebx,DWORD PTR 3889429448[ebp*1+ebx]
	xor	edi,ecx
	and	edi,eax
	mov	ebp,DWORD PTR 36[esi]
	xor	edi,edx
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,20
	add	ebx,ecx
	; R1 24
	lea	eax,DWORD PTR 568446438[ebp*1+eax]
	xor	edi,ebx
	and	edi,edx
	mov	ebp,DWORD PTR 56[esi]
	xor	edi,ecx
	add	eax,edi
	mov	edi,ebx
	rol	eax,5
	add	eax,ebx
	; R1 25
	lea	edx,DWORD PTR 3275163606[ebp*1+edx]
	xor	edi,eax
	and	edi,ecx
	mov	ebp,DWORD PTR 12[esi]
	xor	edi,ebx
	add	edx,edi
	mov	edi,eax
	rol	edx,9
	add	edx,eax
	; R1 26
	lea	ecx,DWORD PTR 4107603335[ebp*1+ecx]
	xor	edi,edx
	and	edi,ebx
	mov	ebp,DWORD PTR 32[esi]
	xor	edi,eax
	add	ecx,edi
	mov	edi,edx
	rol	ecx,14
	add	ecx,edx
	; R1 27
	lea	ebx,DWORD PTR 1163531501[ebp*1+ebx]
	xor	edi,ecx
	and	edi,eax
	mov	ebp,DWORD PTR 52[esi]
	xor	edi,edx
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,20
	add	ebx,ecx
	; R1 28
	lea	eax,DWORD PTR 2850285829[ebp*1+eax]
	xor	edi,ebx
	and	edi,edx
	mov	ebp,DWORD PTR 8[esi]
	xor	edi,ecx
	add	eax,edi
	mov	edi,ebx
	rol	eax,5
	add	eax,ebx
	; R1 29
	lea	edx,DWORD PTR 4243563512[ebp*1+edx]
	xor	edi,eax
	and	edi,ecx
	mov	ebp,DWORD PTR 28[esi]
	xor	edi,ebx
	add	edx,edi
	mov	edi,eax
	rol	edx,9
	add	edx,eax
	; R1 30
	lea	ecx,DWORD PTR 1735328473[ebp*1+ecx]
	xor	edi,edx
	and	edi,ebx
	mov	ebp,DWORD PTR 48[esi]
	xor	edi,eax
	add	ecx,edi
	mov	edi,edx
	rol	ecx,14
	add	ecx,edx
	; R1 31
	lea	ebx,DWORD PTR 2368359562[ebp*1+ebx]
	xor	edi,ecx
	and	edi,eax
	mov	ebp,DWORD PTR 20[esi]
	xor	edi,edx
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,20
	add	ebx,ecx
	;

	; R2 section
	; R2 32
	xor	edi,edx
	xor	edi,ebx
	lea	eax,DWORD PTR 4294588738[ebp*1+eax]
	add	eax,edi
	rol	eax,4
	mov	ebp,DWORD PTR 32[esi]
	mov	edi,ebx
	; R2 33
	lea	edx,DWORD PTR 2272392833[ebp*1+edx]
	add	eax,ebx
	xor	edi,ecx
	xor	edi,eax
	mov	ebp,DWORD PTR 44[esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,11
	add	edx,eax
	; R2 34
	xor	edi,ebx
	xor	edi,edx
	lea	ecx,DWORD PTR 1839030562[ebp*1+ecx]
	add	ecx,edi
	rol	ecx,16
	mov	ebp,DWORD PTR 56[esi]
	mov	edi,edx
	; R2 35
	lea	ebx,DWORD PTR 4259657740[ebp*1+ebx]
	add	ecx,edx
	xor	edi,eax
	xor	edi,ecx
	mov	ebp,DWORD PTR 4[esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,23
	add	ebx,ecx
	; R2 36
	xor	edi,edx
	xor	edi,ebx
	lea	eax,DWORD PTR 2763975236[ebp*1+eax]
	add	eax,edi
	rol	eax,4
	mov	ebp,DWORD PTR 16[esi]
	mov	edi,ebx
	; R2 37
	lea	edx,DWORD PTR 1272893353[ebp*1+edx]
	add	eax,ebx
	xor	edi,ecx
	xor	edi,eax
	mov	ebp,DWORD PTR 28[esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,11
	add	edx,eax
	; R2 38
	xor	edi,ebx
	xor	edi,edx
	lea	ecx,DWORD PTR 4139469664[ebp*1+ecx]
	add	ecx,edi
	rol	ecx,16
	mov	ebp,DWORD PTR 40[esi]
	mov	edi,edx
	; R2 39
	lea	ebx,DWORD PTR 3200236656[ebp*1+ebx]
	add	ecx,edx
	xor	edi,eax
	xor	edi,ecx
	mov	ebp,DWORD PTR 52[esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,23
	add	ebx,ecx
	; R2 40
	xor	edi,edx
	xor	edi,ebx
	lea	eax,DWORD PTR 681279174[ebp*1+eax]
	add	eax,edi
	rol	eax,4
	mov	ebp,DWORD PTR [esi]
	mov	edi,ebx
	; R2 41
	lea	edx,DWORD PTR 3936430074[ebp*1+edx]
	add	eax,ebx
	xor	edi,ecx
	xor	edi,eax
	mov	ebp,DWORD PTR 12[esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,11
	add	edx,eax
	; R2 42
	xor	edi,ebx
	xor	edi,edx
	lea	ecx,DWORD PTR 3572445317[ebp*1+ecx]
	add	ecx,edi
	rol	ecx,16
	mov	ebp,DWORD PTR 24[esi]
	mov	edi,edx
	; R2 43
	lea	ebx,DWORD PTR 76029189[ebp*1+ebx]
	add	ecx,edx
	xor	edi,eax
	xor	edi,ecx
	mov	ebp,DWORD PTR 36[esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,23
	add	ebx,ecx
	; R2 44
	xor	edi,edx
	xor	edi,ebx
	lea	eax,DWORD PTR 3654602809[ebp*1+eax]
	add	eax,edi
	rol	eax,4
	mov	ebp,DWORD PTR 48[esi]
	mov	edi,ebx
	; R2 45
	lea	edx,DWORD PTR 3873151461[ebp*1+edx]
	add	eax,ebx
	xor	edi,ecx
	xor	edi,eax
	mov	ebp,DWORD PTR 60[esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,11
	add	edx,eax
	; R2 46
	xor	edi,ebx
	xor	edi,edx
	lea	ecx,DWORD PTR 530742520[ebp*1+ecx]
	add	ecx,edi
	rol	ecx,16
	mov	ebp,DWORD PTR 8[esi]
	mov	edi,edx
	; R2 47
	lea	ebx,DWORD PTR 3299628645[ebp*1+ebx]
	add	ecx,edx
	xor	edi,eax
	xor	edi,ecx
	mov	ebp,DWORD PTR [esi]
	add	ebx,edi
	mov	edi,-1
	rol	ebx,23
	add	ebx,ecx
	;

	; R3 section
	; R3 48
	xor	edi,edx
	or	edi,ebx
	lea	eax,DWORD PTR 4096336452[ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD PTR 28[esi]
	add	eax,edi
	mov	edi,-1
	rol	eax,6
	xor	edi,ecx
	add	eax,ebx
	; R3 49
	or	edi,eax
	lea	edx,DWORD PTR 1126891415[ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD PTR 56[esi]
	add	edx,edi
	mov	edi,-1
	rol	edx,10
	xor	edi,ebx
	add	edx,eax
	; R3 50
	or	edi,edx
	lea	ecx,DWORD PTR 2878612391[ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD PTR 20[esi]
	add	ecx,edi
	mov	edi,-1
	rol	ecx,15
	xor	edi,eax
	add	ecx,edx
	; R3 51
	or	edi,ecx
	lea	ebx,DWORD PTR 4237533241[ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD PTR 48[esi]
	add	ebx,edi
	mov	edi,-1
	rol	ebx,21
	xor	edi,edx
	add	ebx,ecx
	; R3 52
	or	edi,ebx
	lea	eax,DWORD PTR 1700485571[ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD PTR 12[esi]
	add	eax,edi
	mov	edi,-1
	rol	eax,6
	xor	edi,ecx
	add	eax,ebx
	; R3 53
	or	edi,eax
	lea	edx,DWORD PTR 2399980690[ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD PTR 40[esi]
	add	edx,edi
	mov	edi,-1
	rol	edx,10
	xor	edi,ebx
	add	edx,eax
	; R3 54
	or	edi,edx
	lea	ecx,DWORD PTR 4293915773[ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD PTR 4[esi]
	add	ecx,edi
	mov	edi,-1
	rol	ecx,15
	xor	edi,eax
	add	ecx,edx
	; R3 55
	or	edi,ecx
	lea	ebx,DWORD PTR 2240044497[ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD PTR 32[esi]
	add	ebx,edi
	mov	edi,-1
	rol	ebx,21
	xor	edi,edx
	add	ebx,ecx
	; R3 56
	or	edi,ebx
	lea	eax,DWORD PTR 1873313359[ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD PTR 60[esi]
	add	eax,edi
	mov	edi,-1
	rol	eax,6
	xor	edi,ecx
	add	eax,ebx
	; R3 57
	or	edi,eax
	lea	edx,DWORD PTR 4264355552[ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD PTR 24[esi]
	add	edx,edi
	mov	edi,-1
	rol	edx,10
	xor	edi,ebx
	add	edx,eax
	; R3 58
	or	edi,edx
	lea	ecx,DWORD PTR 2734768916[ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD PTR 52[esi]
	add	ecx,edi
	mov	edi,-1
	rol	ecx,15
	xor	edi,eax
	add	ecx,edx
	; R3 59
	or	edi,ecx
	lea	ebx,DWORD PTR 1309151649[ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD PTR 16[esi]
	add	ebx,edi
	mov	edi,-1
	rol	ebx,21
	xor	edi,edx
	add	ebx,ecx
	; R3 60
	or	edi,ebx
	lea	eax,DWORD PTR 4149444226[ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD PTR 44[esi]
	add	eax,edi
	mov	edi,-1
	rol	eax,6
	xor	edi,ecx
	add	eax,ebx
	; R3 61
	or	edi,eax
	lea	edx,DWORD PTR 3174756917[ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD PTR 8[esi]
	add	edx,edi
	mov	edi,-1
	rol	edx,10
	xor	edi,ebx
	add	edx,eax
	; R3 62
	or	edi,edx
	lea	ecx,DWORD PTR 718787259[ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD PTR 36[esi]
	add	ecx,edi
	mov	edi,-1
	rol	ecx,15
	xor	edi,eax
	add	ecx,edx
	; R3 63
	or	edi,ecx
	lea	ebx,DWORD PTR 3951481745[ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD PTR 24[esp]
	add	ebx,edi
	add	esi,64
	rol	ebx,21
	mov	edi,DWORD PTR [ebp]
	add	ebx,ecx
	add	eax,edi
	mov	edi,DWORD PTR 4[ebp]
	add	ebx,edi
	mov	edi,DWORD PTR 8[ebp]
	add	ecx,edi
	mov	edi,DWORD PTR 12[ebp]
	add	edx,edi
	mov	DWORD PTR [ebp],eax
	mov	DWORD PTR 4[ebp],ebx
	mov	edi,DWORD PTR [esp]
	mov	DWORD PTR 8[ebp],ecx
	mov	DWORD PTR 12[ebp],edx
	cmp	edi,esi
	jae	$L000start
	pop	eax
	pop	ebx
	pop	ebp
	pop	edi
	pop	esi
	ret
_md5_block_asm_data_order ENDP
.text$	ENDS
END
