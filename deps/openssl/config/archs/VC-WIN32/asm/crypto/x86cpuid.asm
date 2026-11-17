%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
global	_OPENSSL_ia32_cpuid
align	16
_OPENSSL_ia32_cpuid:
L$_OPENSSL_ia32_cpuid_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	xor	edx,edx
	pushfd
	pop	eax
	mov	ecx,eax
	xor	eax,2097152
	push	eax
	popfd
	pushfd
	pop	eax
	xor	ecx,eax
	xor	eax,eax
	mov	esi,DWORD [20+esp]
	mov	DWORD [8+esi],eax
	bt	ecx,21
	jnc	NEAR L$000nocpuid
	cpuid
	mov	edi,eax
	xor	eax,eax
	cmp	ebx,1970169159
	setne	al
	mov	ebp,eax
	cmp	edx,1231384169
	setne	al
	or	ebp,eax
	cmp	ecx,1818588270
	setne	al
	or	ebp,eax
	jz	NEAR L$001intel
	cmp	ebx,1752462657
	setne	al
	mov	esi,eax
	cmp	edx,1769238117
	setne	al
	or	esi,eax
	cmp	ecx,1145913699
	setne	al
	or	esi,eax
	jnz	NEAR L$001intel
	mov	eax,2147483648
	cpuid
	cmp	eax,2147483649
	jb	NEAR L$001intel
	mov	esi,eax
	mov	eax,2147483649
	cpuid
	or	ebp,ecx
	and	ebp,2049
	cmp	esi,2147483656
	jb	NEAR L$001intel
	mov	eax,2147483656
	cpuid
	movzx	esi,cl
	inc	esi
	mov	eax,1
	xor	ecx,ecx
	cpuid
	bt	edx,28
	jnc	NEAR L$002generic
	shr	ebx,16
	and	ebx,255
	cmp	ebx,esi
	ja	NEAR L$002generic
	and	edx,4026531839
	jmp	NEAR L$002generic
L$001intel:
	cmp	edi,4
	mov	esi,-1
	jb	NEAR L$003nocacheinfo
	mov	eax,4
	mov	ecx,0
	cpuid
	mov	esi,eax
	shr	esi,14
	and	esi,4095
L$003nocacheinfo:
	mov	eax,1
	xor	ecx,ecx
	cpuid
	and	edx,3220176895
	cmp	ebp,0
	jne	NEAR L$004notintel
	or	edx,1073741824
	and	ah,15
	cmp	ah,15
	jne	NEAR L$004notintel
	or	edx,1048576
L$004notintel:
	bt	edx,28
	jnc	NEAR L$002generic
	and	edx,4026531839
	cmp	esi,0
	je	NEAR L$002generic
	or	edx,268435456
	shr	ebx,16
	cmp	bl,1
	ja	NEAR L$002generic
	and	edx,4026531839
L$002generic:
	and	ebp,2048
	and	ecx,4294965247
	mov	esi,edx
	or	ebp,ecx
	cmp	edi,7
	mov	edi,DWORD [20+esp]
	jb	NEAR L$005no_extended_info
	mov	eax,7
	xor	ecx,ecx
	cpuid
	mov	DWORD [8+edi],ebx
	mov	DWORD [12+edi],ecx
	mov	DWORD [16+edi],edx
	cmp	eax,1
	jb	NEAR L$005no_extended_info
	mov	eax,7
	mov	ecx,1
	cpuid
	mov	DWORD [20+edi],eax
	mov	DWORD [24+edi],edx
	mov	DWORD [28+edi],ebx
	mov	DWORD [32+edi],ecx
	and	edx,524288
	cmp	edx,0
	je	NEAR L$005no_extended_info
	mov	eax,36
	mov	ecx,0
	cpuid
	mov	DWORD [36+edi],ebx
L$005no_extended_info:
	bt	ebp,27
	jnc	NEAR L$006clear_avx
	xor	ecx,ecx
db	15,1,208
	and	eax,6
	cmp	eax,6
	je	NEAR L$007done
	cmp	eax,2
	je	NEAR L$006clear_avx
L$008clear_xmm:
	and	ebp,4261412861
	and	esi,4278190079
L$006clear_avx:
	and	ebp,4026525695
	and	DWORD [20+edi],4286578687
	and	DWORD [8+edi],4294967263
L$007done:
	mov	eax,esi
	mov	edx,ebp
L$000nocpuid:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
;extern	_OPENSSL_ia32cap_P
global	_OPENSSL_rdtsc
align	16
_OPENSSL_rdtsc:
L$_OPENSSL_rdtsc_begin:
	xor	eax,eax
	xor	edx,edx
	lea	ecx,[_OPENSSL_ia32cap_P]
	bt	DWORD [ecx],4
	jnc	NEAR L$009notsc
	rdtsc
L$009notsc:
	ret
global	_OPENSSL_instrument_halt
align	16
_OPENSSL_instrument_halt:
L$_OPENSSL_instrument_halt_begin:
	lea	ecx,[_OPENSSL_ia32cap_P]
	bt	DWORD [ecx],4
	jnc	NEAR L$010nohalt
dd	2421723150
	and	eax,3
	jnz	NEAR L$010nohalt
	pushfd
	pop	eax
	bt	eax,9
	jnc	NEAR L$010nohalt
	rdtsc
	push	edx
	push	eax
	hlt
	rdtsc
	sub	eax,DWORD [esp]
	sbb	edx,DWORD [4+esp]
	add	esp,8
	ret
L$010nohalt:
	xor	eax,eax
	xor	edx,edx
	ret
global	_OPENSSL_far_spin
align	16
_OPENSSL_far_spin:
L$_OPENSSL_far_spin_begin:
	pushfd
	pop	eax
	bt	eax,9
	jnc	NEAR L$011nospin
	mov	eax,DWORD [4+esp]
	mov	ecx,DWORD [8+esp]
dd	2430111262
	xor	eax,eax
	mov	edx,DWORD [ecx]
	jmp	NEAR L$012spin
align	16
L$012spin:
	inc	eax
	cmp	edx,DWORD [ecx]
	je	NEAR L$012spin
dd	529567888
	ret
L$011nospin:
	xor	eax,eax
	xor	edx,edx
	ret
global	_OPENSSL_atomic_add
align	16
_OPENSSL_atomic_add:
L$_OPENSSL_atomic_add_begin:
	mov	edx,DWORD [4+esp]
	mov	ecx,DWORD [8+esp]
	push	ebx
	nop
	mov	eax,DWORD [edx]
L$013spin:
	lea	ebx,[ecx*1+eax]
	nop
dd	447811568
	jne	NEAR L$013spin
	mov	eax,ebx
	pop	ebx
	ret
global	_OPENSSL_cleanse
align	16
_OPENSSL_cleanse:
L$_OPENSSL_cleanse_begin:
	mov	edx,DWORD [4+esp]
	mov	ecx,DWORD [8+esp]
	xor	eax,eax
	cmp	ecx,7
	jae	NEAR L$014lot
	cmp	ecx,0
	je	NEAR L$015ret
L$016little:
	mov	BYTE [edx],al
	sub	ecx,1
	lea	edx,[1+edx]
	jnz	NEAR L$016little
L$015ret:
	ret
align	16
L$014lot:
	test	edx,3
	jz	NEAR L$017aligned
	mov	BYTE [edx],al
	lea	ecx,[ecx-1]
	lea	edx,[1+edx]
	jmp	NEAR L$014lot
L$017aligned:
	mov	DWORD [edx],eax
	lea	ecx,[ecx-4]
	test	ecx,-4
	lea	edx,[4+edx]
	jnz	NEAR L$017aligned
	cmp	ecx,0
	jne	NEAR L$016little
	ret
global	_CRYPTO_memcmp
align	16
_CRYPTO_memcmp:
L$_CRYPTO_memcmp_begin:
	push	esi
	push	edi
	mov	esi,DWORD [12+esp]
	mov	edi,DWORD [16+esp]
	mov	ecx,DWORD [20+esp]
	xor	eax,eax
	xor	edx,edx
	cmp	ecx,0
	je	NEAR L$018no_data
L$019loop:
	mov	dl,BYTE [esi]
	lea	esi,[1+esi]
	xor	dl,BYTE [edi]
	lea	edi,[1+edi]
	or	al,dl
	dec	ecx
	jnz	NEAR L$019loop
	neg	eax
	shr	eax,31
L$018no_data:
	pop	edi
	pop	esi
	ret
global	_OPENSSL_instrument_bus
align	16
_OPENSSL_instrument_bus:
L$_OPENSSL_instrument_bus_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,0
	lea	edx,[_OPENSSL_ia32cap_P]
	bt	DWORD [edx],4
	jnc	NEAR L$020nogo
	bt	DWORD [edx],19
	jnc	NEAR L$020nogo
	mov	edi,DWORD [20+esp]
	mov	ecx,DWORD [24+esp]
	rdtsc
	mov	esi,eax
	mov	ebx,0
	clflush	[edi]
db	240
	add	DWORD [edi],ebx
	jmp	NEAR L$021loop
align	16
L$021loop:
	rdtsc
	mov	edx,eax
	sub	eax,esi
	mov	esi,edx
	mov	ebx,eax
	clflush	[edi]
db	240
	add	DWORD [edi],eax
	lea	edi,[4+edi]
	sub	ecx,1
	jnz	NEAR L$021loop
	mov	eax,DWORD [24+esp]
L$020nogo:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_OPENSSL_instrument_bus2
align	16
_OPENSSL_instrument_bus2:
L$_OPENSSL_instrument_bus2_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,0
	lea	edx,[_OPENSSL_ia32cap_P]
	bt	DWORD [edx],4
	jnc	NEAR L$022nogo
	bt	DWORD [edx],19
	jnc	NEAR L$022nogo
	mov	edi,DWORD [20+esp]
	mov	ecx,DWORD [24+esp]
	mov	ebp,DWORD [28+esp]
	rdtsc
	mov	esi,eax
	mov	ebx,0
	clflush	[edi]
db	240
	add	DWORD [edi],ebx
	rdtsc
	mov	edx,eax
	sub	eax,esi
	mov	esi,edx
	mov	ebx,eax
	jmp	NEAR L$023loop2
align	16
L$023loop2:
	clflush	[edi]
db	240
	add	DWORD [edi],eax
	sub	ebp,1
	jz	NEAR L$024done2
	rdtsc
	mov	edx,eax
	sub	eax,esi
	mov	esi,edx
	cmp	eax,ebx
	mov	ebx,eax
	mov	edx,0
	setne	dl
	sub	ecx,edx
	lea	edi,[edx*4+edi]
	jnz	NEAR L$023loop2
L$024done2:
	mov	eax,DWORD [24+esp]
	sub	eax,ecx
L$022nogo:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_OPENSSL_ia32_rdrand_bytes
align	16
_OPENSSL_ia32_rdrand_bytes:
L$_OPENSSL_ia32_rdrand_bytes_begin:
	push	edi
	push	ebx
	xor	eax,eax
	mov	edi,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	cmp	ebx,0
	je	NEAR L$025done
	mov	ecx,8
L$026loop:
db	15,199,242
	jc	NEAR L$027break
	loop	L$026loop
	jmp	NEAR L$025done
align	16
L$027break:
	cmp	ebx,4
	jb	NEAR L$028tail
	mov	DWORD [edi],edx
	lea	edi,[4+edi]
	add	eax,4
	sub	ebx,4
	jz	NEAR L$025done
	mov	ecx,8
	jmp	NEAR L$026loop
align	16
L$028tail:
	mov	BYTE [edi],dl
	lea	edi,[1+edi]
	inc	eax
	shr	edx,8
	dec	ebx
	jnz	NEAR L$028tail
L$025done:
	xor	edx,edx
	pop	ebx
	pop	edi
	ret
global	_OPENSSL_ia32_rdseed_bytes
align	16
_OPENSSL_ia32_rdseed_bytes:
L$_OPENSSL_ia32_rdseed_bytes_begin:
	push	edi
	push	ebx
	xor	eax,eax
	mov	edi,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	cmp	ebx,0
	je	NEAR L$029done
	mov	ecx,8
L$030loop:
db	15,199,250
	jc	NEAR L$031break
	loop	L$030loop
	jmp	NEAR L$029done
align	16
L$031break:
	cmp	ebx,4
	jb	NEAR L$032tail
	mov	DWORD [edi],edx
	lea	edi,[4+edi]
	add	eax,4
	sub	ebx,4
	jz	NEAR L$029done
	mov	ecx,8
	jmp	NEAR L$030loop
align	16
L$032tail:
	mov	BYTE [edi],dl
	lea	edi,[1+edi]
	inc	eax
	shr	edx,8
	dec	ebx
	jnz	NEAR L$032tail
L$029done:
	xor	edx,edx
	pop	ebx
	pop	edi
	ret
segment	.bss
common	_OPENSSL_ia32cap_P 40
segment	.CRT$XCU data align=4
extern	_OPENSSL_cpuid_setup
dd	_OPENSSL_cpuid_setup
