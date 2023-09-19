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
global	_OPENSSL_wipe_cpu
align	16
_OPENSSL_wipe_cpu:
L$_OPENSSL_wipe_cpu_begin:
	xor	eax,eax
	xor	edx,edx
	lea	ecx,[_OPENSSL_ia32cap_P]
	mov	ecx,DWORD [ecx]
	bt	DWORD [ecx],1
	jnc	NEAR L$013no_x87
	and	ecx,83886080
	cmp	ecx,83886080
	jne	NEAR L$014no_sse2
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
L$014no_sse2:
dd	4007259865,4007259865,4007259865,4007259865,2430851995
L$013no_x87:
	lea	eax,[4+esp]
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
L$015spin:
	lea	ebx,[ecx*1+eax]
	nop
dd	447811568
	jne	NEAR L$015spin
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
	jae	NEAR L$016lot
	cmp	ecx,0
	je	NEAR L$017ret
L$018little:
	mov	BYTE [edx],al
	sub	ecx,1
	lea	edx,[1+edx]
	jnz	NEAR L$018little
L$017ret:
	ret
align	16
L$016lot:
	test	edx,3
	jz	NEAR L$019aligned
	mov	BYTE [edx],al
	lea	ecx,[ecx-1]
	lea	edx,[1+edx]
	jmp	NEAR L$016lot
L$019aligned:
	mov	DWORD [edx],eax
	lea	ecx,[ecx-4]
	test	ecx,-4
	lea	edx,[4+edx]
	jnz	NEAR L$019aligned
	cmp	ecx,0
	jne	NEAR L$018little
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
	je	NEAR L$020no_data
L$021loop:
	mov	dl,BYTE [esi]
	lea	esi,[1+esi]
	xor	dl,BYTE [edi]
	lea	edi,[1+edi]
	or	al,dl
	dec	ecx
	jnz	NEAR L$021loop
	neg	eax
	shr	eax,31
L$020no_data:
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
	jnc	NEAR L$022nogo
	bt	DWORD [edx],19
	jnc	NEAR L$022nogo
	mov	edi,DWORD [20+esp]
	mov	ecx,DWORD [24+esp]
	rdtsc
	mov	esi,eax
	mov	ebx,0
	clflush	[edi]
db	240
	add	DWORD [edi],ebx
	jmp	NEAR L$023loop
align	16
L$023loop:
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
	jnz	NEAR L$023loop
	mov	eax,DWORD [24+esp]
L$022nogo:
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
	jnc	NEAR L$024nogo
	bt	DWORD [edx],19
	jnc	NEAR L$024nogo
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
	jmp	NEAR L$025loop2
align	16
L$025loop2:
	clflush	[edi]
db	240
	add	DWORD [edi],eax
	sub	ebp,1
	jz	NEAR L$026done2
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
	jnz	NEAR L$025loop2
L$026done2:
	mov	eax,DWORD [24+esp]
	sub	eax,ecx
L$024nogo:
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
	je	NEAR L$027done
	mov	ecx,8
L$028loop:
db	15,199,242
	jc	NEAR L$029break
	loop	L$028loop
	jmp	NEAR L$027done
align	16
L$029break:
	cmp	ebx,4
	jb	NEAR L$030tail
	mov	DWORD [edi],edx
	lea	edi,[4+edi]
	add	eax,4
	sub	ebx,4
	jz	NEAR L$027done
	mov	ecx,8
	jmp	NEAR L$028loop
align	16
L$030tail:
	mov	BYTE [edi],dl
	lea	edi,[1+edi]
	inc	eax
	shr	edx,8
	dec	ebx
	jnz	NEAR L$030tail
L$027done:
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
	je	NEAR L$031done
	mov	ecx,8
L$032loop:
db	15,199,250
	jc	NEAR L$033break
	loop	L$032loop
	jmp	NEAR L$031done
align	16
L$033break:
	cmp	ebx,4
	jb	NEAR L$034tail
	mov	DWORD [edi],edx
	lea	edi,[4+edi]
	add	eax,4
	sub	ebx,4
	jz	NEAR L$031done
	mov	ecx,8
	jmp	NEAR L$032loop
align	16
L$034tail:
	mov	BYTE [edi],dl
	lea	edi,[1+edi]
	inc	eax
	shr	edx,8
	dec	ebx
	jnz	NEAR L$034tail
L$031done:
	xor	edx,edx
	pop	ebx
	pop	edi
	ret
segment	.bss
common	_OPENSSL_ia32cap_P 16
segment	.CRT$XCU data align=4
extern	_OPENSSL_cpuid_setup
dd	_OPENSSL_cpuid_setup
