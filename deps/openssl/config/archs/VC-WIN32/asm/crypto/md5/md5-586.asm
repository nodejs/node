%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
global	_ossl_md5_block_asm_data_order
align	16
_ossl_md5_block_asm_data_order:
L$_ossl_md5_block_asm_data_order_begin:
	push	esi
	push	edi
	mov	edi,DWORD [12+esp]
	mov	esi,DWORD [16+esp]
	mov	ecx,DWORD [20+esp]
	push	ebp
	shl	ecx,6
	push	ebx
	add	ecx,esi
	sub	ecx,64
	mov	eax,DWORD [edi]
	push	ecx
	mov	ebx,DWORD [4+edi]
	mov	ecx,DWORD [8+edi]
	mov	edx,DWORD [12+edi]
L$000start:
	; 
	; R0 section
	mov	edi,ecx
	mov	ebp,DWORD [esi]
	; R0 0
	xor	edi,edx
	and	edi,ebx
	lea	eax,[3614090360+ebp*1+eax]
	xor	edi,edx
	mov	ebp,DWORD [4+esi]
	add	eax,edi
	rol	eax,7
	mov	edi,ebx
	add	eax,ebx
	; R0 1
	xor	edi,ecx
	and	edi,eax
	lea	edx,[3905402710+ebp*1+edx]
	xor	edi,ecx
	mov	ebp,DWORD [8+esi]
	add	edx,edi
	rol	edx,12
	mov	edi,eax
	add	edx,eax
	; R0 2
	xor	edi,ebx
	and	edi,edx
	lea	ecx,[606105819+ebp*1+ecx]
	xor	edi,ebx
	mov	ebp,DWORD [12+esi]
	add	ecx,edi
	rol	ecx,17
	mov	edi,edx
	add	ecx,edx
	; R0 3
	xor	edi,eax
	and	edi,ecx
	lea	ebx,[3250441966+ebp*1+ebx]
	xor	edi,eax
	mov	ebp,DWORD [16+esi]
	add	ebx,edi
	rol	ebx,22
	mov	edi,ecx
	add	ebx,ecx
	; R0 4
	xor	edi,edx
	and	edi,ebx
	lea	eax,[4118548399+ebp*1+eax]
	xor	edi,edx
	mov	ebp,DWORD [20+esi]
	add	eax,edi
	rol	eax,7
	mov	edi,ebx
	add	eax,ebx
	; R0 5
	xor	edi,ecx
	and	edi,eax
	lea	edx,[1200080426+ebp*1+edx]
	xor	edi,ecx
	mov	ebp,DWORD [24+esi]
	add	edx,edi
	rol	edx,12
	mov	edi,eax
	add	edx,eax
	; R0 6
	xor	edi,ebx
	and	edi,edx
	lea	ecx,[2821735955+ebp*1+ecx]
	xor	edi,ebx
	mov	ebp,DWORD [28+esi]
	add	ecx,edi
	rol	ecx,17
	mov	edi,edx
	add	ecx,edx
	; R0 7
	xor	edi,eax
	and	edi,ecx
	lea	ebx,[4249261313+ebp*1+ebx]
	xor	edi,eax
	mov	ebp,DWORD [32+esi]
	add	ebx,edi
	rol	ebx,22
	mov	edi,ecx
	add	ebx,ecx
	; R0 8
	xor	edi,edx
	and	edi,ebx
	lea	eax,[1770035416+ebp*1+eax]
	xor	edi,edx
	mov	ebp,DWORD [36+esi]
	add	eax,edi
	rol	eax,7
	mov	edi,ebx
	add	eax,ebx
	; R0 9
	xor	edi,ecx
	and	edi,eax
	lea	edx,[2336552879+ebp*1+edx]
	xor	edi,ecx
	mov	ebp,DWORD [40+esi]
	add	edx,edi
	rol	edx,12
	mov	edi,eax
	add	edx,eax
	; R0 10
	xor	edi,ebx
	and	edi,edx
	lea	ecx,[4294925233+ebp*1+ecx]
	xor	edi,ebx
	mov	ebp,DWORD [44+esi]
	add	ecx,edi
	rol	ecx,17
	mov	edi,edx
	add	ecx,edx
	; R0 11
	xor	edi,eax
	and	edi,ecx
	lea	ebx,[2304563134+ebp*1+ebx]
	xor	edi,eax
	mov	ebp,DWORD [48+esi]
	add	ebx,edi
	rol	ebx,22
	mov	edi,ecx
	add	ebx,ecx
	; R0 12
	xor	edi,edx
	and	edi,ebx
	lea	eax,[1804603682+ebp*1+eax]
	xor	edi,edx
	mov	ebp,DWORD [52+esi]
	add	eax,edi
	rol	eax,7
	mov	edi,ebx
	add	eax,ebx
	; R0 13
	xor	edi,ecx
	and	edi,eax
	lea	edx,[4254626195+ebp*1+edx]
	xor	edi,ecx
	mov	ebp,DWORD [56+esi]
	add	edx,edi
	rol	edx,12
	mov	edi,eax
	add	edx,eax
	; R0 14
	xor	edi,ebx
	and	edi,edx
	lea	ecx,[2792965006+ebp*1+ecx]
	xor	edi,ebx
	mov	ebp,DWORD [60+esi]
	add	ecx,edi
	rol	ecx,17
	mov	edi,edx
	add	ecx,edx
	; R0 15
	xor	edi,eax
	and	edi,ecx
	lea	ebx,[1236535329+ebp*1+ebx]
	xor	edi,eax
	mov	ebp,DWORD [4+esi]
	add	ebx,edi
	rol	ebx,22
	mov	edi,ecx
	add	ebx,ecx
	; 
	; R1 section
	; R1 16
	xor	edi,ebx
	and	edi,edx
	lea	eax,[4129170786+ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD [24+esi]
	add	eax,edi
	mov	edi,ebx
	rol	eax,5
	add	eax,ebx
	; R1 17
	xor	edi,eax
	and	edi,ecx
	lea	edx,[3225465664+ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD [44+esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,9
	add	edx,eax
	; R1 18
	xor	edi,edx
	and	edi,ebx
	lea	ecx,[643717713+ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD [esi]
	add	ecx,edi
	mov	edi,edx
	rol	ecx,14
	add	ecx,edx
	; R1 19
	xor	edi,ecx
	and	edi,eax
	lea	ebx,[3921069994+ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD [20+esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,20
	add	ebx,ecx
	; R1 20
	xor	edi,ebx
	and	edi,edx
	lea	eax,[3593408605+ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD [40+esi]
	add	eax,edi
	mov	edi,ebx
	rol	eax,5
	add	eax,ebx
	; R1 21
	xor	edi,eax
	and	edi,ecx
	lea	edx,[38016083+ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD [60+esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,9
	add	edx,eax
	; R1 22
	xor	edi,edx
	and	edi,ebx
	lea	ecx,[3634488961+ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD [16+esi]
	add	ecx,edi
	mov	edi,edx
	rol	ecx,14
	add	ecx,edx
	; R1 23
	xor	edi,ecx
	and	edi,eax
	lea	ebx,[3889429448+ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD [36+esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,20
	add	ebx,ecx
	; R1 24
	xor	edi,ebx
	and	edi,edx
	lea	eax,[568446438+ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD [56+esi]
	add	eax,edi
	mov	edi,ebx
	rol	eax,5
	add	eax,ebx
	; R1 25
	xor	edi,eax
	and	edi,ecx
	lea	edx,[3275163606+ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD [12+esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,9
	add	edx,eax
	; R1 26
	xor	edi,edx
	and	edi,ebx
	lea	ecx,[4107603335+ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD [32+esi]
	add	ecx,edi
	mov	edi,edx
	rol	ecx,14
	add	ecx,edx
	; R1 27
	xor	edi,ecx
	and	edi,eax
	lea	ebx,[1163531501+ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD [52+esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,20
	add	ebx,ecx
	; R1 28
	xor	edi,ebx
	and	edi,edx
	lea	eax,[2850285829+ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD [8+esi]
	add	eax,edi
	mov	edi,ebx
	rol	eax,5
	add	eax,ebx
	; R1 29
	xor	edi,eax
	and	edi,ecx
	lea	edx,[4243563512+ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD [28+esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,9
	add	edx,eax
	; R1 30
	xor	edi,edx
	and	edi,ebx
	lea	ecx,[1735328473+ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD [48+esi]
	add	ecx,edi
	mov	edi,edx
	rol	ecx,14
	add	ecx,edx
	; R1 31
	xor	edi,ecx
	and	edi,eax
	lea	ebx,[2368359562+ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD [20+esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,20
	add	ebx,ecx
	; 
	; R2 section
	; R2 32
	xor	edi,edx
	xor	edi,ebx
	lea	eax,[4294588738+ebp*1+eax]
	add	eax,edi
	mov	ebp,DWORD [32+esi]
	rol	eax,4
	mov	edi,ebx
	; R2 33
	add	eax,ebx
	xor	edi,ecx
	lea	edx,[2272392833+ebp*1+edx]
	xor	edi,eax
	mov	ebp,DWORD [44+esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,11
	add	edx,eax
	; R2 34
	xor	edi,ebx
	xor	edi,edx
	lea	ecx,[1839030562+ebp*1+ecx]
	add	ecx,edi
	mov	ebp,DWORD [56+esi]
	rol	ecx,16
	mov	edi,edx
	; R2 35
	add	ecx,edx
	xor	edi,eax
	lea	ebx,[4259657740+ebp*1+ebx]
	xor	edi,ecx
	mov	ebp,DWORD [4+esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,23
	add	ebx,ecx
	; R2 36
	xor	edi,edx
	xor	edi,ebx
	lea	eax,[2763975236+ebp*1+eax]
	add	eax,edi
	mov	ebp,DWORD [16+esi]
	rol	eax,4
	mov	edi,ebx
	; R2 37
	add	eax,ebx
	xor	edi,ecx
	lea	edx,[1272893353+ebp*1+edx]
	xor	edi,eax
	mov	ebp,DWORD [28+esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,11
	add	edx,eax
	; R2 38
	xor	edi,ebx
	xor	edi,edx
	lea	ecx,[4139469664+ebp*1+ecx]
	add	ecx,edi
	mov	ebp,DWORD [40+esi]
	rol	ecx,16
	mov	edi,edx
	; R2 39
	add	ecx,edx
	xor	edi,eax
	lea	ebx,[3200236656+ebp*1+ebx]
	xor	edi,ecx
	mov	ebp,DWORD [52+esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,23
	add	ebx,ecx
	; R2 40
	xor	edi,edx
	xor	edi,ebx
	lea	eax,[681279174+ebp*1+eax]
	add	eax,edi
	mov	ebp,DWORD [esi]
	rol	eax,4
	mov	edi,ebx
	; R2 41
	add	eax,ebx
	xor	edi,ecx
	lea	edx,[3936430074+ebp*1+edx]
	xor	edi,eax
	mov	ebp,DWORD [12+esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,11
	add	edx,eax
	; R2 42
	xor	edi,ebx
	xor	edi,edx
	lea	ecx,[3572445317+ebp*1+ecx]
	add	ecx,edi
	mov	ebp,DWORD [24+esi]
	rol	ecx,16
	mov	edi,edx
	; R2 43
	add	ecx,edx
	xor	edi,eax
	lea	ebx,[76029189+ebp*1+ebx]
	xor	edi,ecx
	mov	ebp,DWORD [36+esi]
	add	ebx,edi
	mov	edi,ecx
	rol	ebx,23
	add	ebx,ecx
	; R2 44
	xor	edi,edx
	xor	edi,ebx
	lea	eax,[3654602809+ebp*1+eax]
	add	eax,edi
	mov	ebp,DWORD [48+esi]
	rol	eax,4
	mov	edi,ebx
	; R2 45
	add	eax,ebx
	xor	edi,ecx
	lea	edx,[3873151461+ebp*1+edx]
	xor	edi,eax
	mov	ebp,DWORD [60+esi]
	add	edx,edi
	mov	edi,eax
	rol	edx,11
	add	edx,eax
	; R2 46
	xor	edi,ebx
	xor	edi,edx
	lea	ecx,[530742520+ebp*1+ecx]
	add	ecx,edi
	mov	ebp,DWORD [8+esi]
	rol	ecx,16
	mov	edi,edx
	; R2 47
	add	ecx,edx
	xor	edi,eax
	lea	ebx,[3299628645+ebp*1+ebx]
	xor	edi,ecx
	mov	ebp,DWORD [esi]
	add	ebx,edi
	mov	edi,-1
	rol	ebx,23
	add	ebx,ecx
	; 
	; R3 section
	; R3 48
	xor	edi,edx
	or	edi,ebx
	lea	eax,[4096336452+ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD [28+esi]
	add	eax,edi
	mov	edi,-1
	rol	eax,6
	xor	edi,ecx
	add	eax,ebx
	; R3 49
	or	edi,eax
	lea	edx,[1126891415+ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD [56+esi]
	add	edx,edi
	mov	edi,-1
	rol	edx,10
	xor	edi,ebx
	add	edx,eax
	; R3 50
	or	edi,edx
	lea	ecx,[2878612391+ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD [20+esi]
	add	ecx,edi
	mov	edi,-1
	rol	ecx,15
	xor	edi,eax
	add	ecx,edx
	; R3 51
	or	edi,ecx
	lea	ebx,[4237533241+ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD [48+esi]
	add	ebx,edi
	mov	edi,-1
	rol	ebx,21
	xor	edi,edx
	add	ebx,ecx
	; R3 52
	or	edi,ebx
	lea	eax,[1700485571+ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD [12+esi]
	add	eax,edi
	mov	edi,-1
	rol	eax,6
	xor	edi,ecx
	add	eax,ebx
	; R3 53
	or	edi,eax
	lea	edx,[2399980690+ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD [40+esi]
	add	edx,edi
	mov	edi,-1
	rol	edx,10
	xor	edi,ebx
	add	edx,eax
	; R3 54
	or	edi,edx
	lea	ecx,[4293915773+ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD [4+esi]
	add	ecx,edi
	mov	edi,-1
	rol	ecx,15
	xor	edi,eax
	add	ecx,edx
	; R3 55
	or	edi,ecx
	lea	ebx,[2240044497+ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD [32+esi]
	add	ebx,edi
	mov	edi,-1
	rol	ebx,21
	xor	edi,edx
	add	ebx,ecx
	; R3 56
	or	edi,ebx
	lea	eax,[1873313359+ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD [60+esi]
	add	eax,edi
	mov	edi,-1
	rol	eax,6
	xor	edi,ecx
	add	eax,ebx
	; R3 57
	or	edi,eax
	lea	edx,[4264355552+ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD [24+esi]
	add	edx,edi
	mov	edi,-1
	rol	edx,10
	xor	edi,ebx
	add	edx,eax
	; R3 58
	or	edi,edx
	lea	ecx,[2734768916+ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD [52+esi]
	add	ecx,edi
	mov	edi,-1
	rol	ecx,15
	xor	edi,eax
	add	ecx,edx
	; R3 59
	or	edi,ecx
	lea	ebx,[1309151649+ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD [16+esi]
	add	ebx,edi
	mov	edi,-1
	rol	ebx,21
	xor	edi,edx
	add	ebx,ecx
	; R3 60
	or	edi,ebx
	lea	eax,[4149444226+ebp*1+eax]
	xor	edi,ecx
	mov	ebp,DWORD [44+esi]
	add	eax,edi
	mov	edi,-1
	rol	eax,6
	xor	edi,ecx
	add	eax,ebx
	; R3 61
	or	edi,eax
	lea	edx,[3174756917+ebp*1+edx]
	xor	edi,ebx
	mov	ebp,DWORD [8+esi]
	add	edx,edi
	mov	edi,-1
	rol	edx,10
	xor	edi,ebx
	add	edx,eax
	; R3 62
	or	edi,edx
	lea	ecx,[718787259+ebp*1+ecx]
	xor	edi,eax
	mov	ebp,DWORD [36+esi]
	add	ecx,edi
	mov	edi,-1
	rol	ecx,15
	xor	edi,eax
	add	ecx,edx
	; R3 63
	or	edi,ecx
	lea	ebx,[3951481745+ebp*1+ebx]
	xor	edi,edx
	mov	ebp,DWORD [24+esp]
	add	ebx,edi
	add	esi,64
	rol	ebx,21
	mov	edi,DWORD [ebp]
	add	ebx,ecx
	add	eax,edi
	mov	edi,DWORD [4+ebp]
	add	ebx,edi
	mov	edi,DWORD [8+ebp]
	add	ecx,edi
	mov	edi,DWORD [12+ebp]
	add	edx,edi
	mov	DWORD [ebp],eax
	mov	DWORD [4+ebp],ebx
	mov	edi,DWORD [esp]
	mov	DWORD [8+ebp],ecx
	mov	DWORD [12+ebp],edx
	cmp	edi,esi
	jae	NEAR L$000start
	pop	eax
	pop	ebx
	pop	ebp
	pop	edi
	pop	esi
	ret
