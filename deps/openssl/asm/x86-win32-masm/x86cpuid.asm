TITLE	x86cpuid.asm
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
_OPENSSL_ia32_cpuid	PROC PUBLIC
$L_OPENSSL_ia32_cpuid_begin::
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
	mov	esi,DWORD PTR 20[esp]
	mov	DWORD PTR 8[esi],eax
	bt	ecx,21
	jnc	$L000nocpuid
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
	jz	$L001intel
	cmp	ebx,1752462657
	setne	al
	mov	esi,eax
	cmp	edx,1769238117
	setne	al
	or	esi,eax
	cmp	ecx,1145913699
	setne	al
	or	esi,eax
	jnz	$L001intel
	mov	eax,2147483648
	cpuid
	cmp	eax,2147483649
	jb	$L001intel
	mov	esi,eax
	mov	eax,2147483649
	cpuid
	or	ebp,ecx
	and	ebp,2049
	cmp	esi,2147483656
	jb	$L001intel
	mov	eax,2147483656
	cpuid
	movzx	esi,cl
	inc	esi
	mov	eax,1
	xor	ecx,ecx
	cpuid
	bt	edx,28
	jnc	$L002generic
	shr	ebx,16
	and	ebx,255
	cmp	ebx,esi
	ja	$L002generic
	and	edx,4026531839
	jmp	$L002generic
$L001intel:
	cmp	edi,4
	mov	esi,-1
	jb	$L003nocacheinfo
	mov	eax,4
	mov	ecx,0
	cpuid
	mov	esi,eax
	shr	esi,14
	and	esi,4095
$L003nocacheinfo:
	mov	eax,1
	xor	ecx,ecx
	cpuid
	and	edx,3220176895
	cmp	ebp,0
	jne	$L004notintel
	or	edx,1073741824
	and	ah,15
	cmp	ah,15
	jne	$L004notintel
	or	edx,1048576
$L004notintel:
	bt	edx,28
	jnc	$L002generic
	and	edx,4026531839
	cmp	esi,0
	je	$L002generic
	or	edx,268435456
	shr	ebx,16
	cmp	bl,1
	ja	$L002generic
	and	edx,4026531839
$L002generic:
	and	ebp,2048
	and	ecx,4294965247
	mov	esi,edx
	or	ebp,ecx
	cmp	edi,7
	mov	edi,DWORD PTR 20[esp]
	jb	$L005no_extended_info
	mov	eax,7
	xor	ecx,ecx
	cpuid
	mov	DWORD PTR 8[edi],ebx
$L005no_extended_info:
	bt	ebp,27
	jnc	$L006clear_avx
	xor	ecx,ecx
DB	15,1,208
	and	eax,6
	cmp	eax,6
	je	$L007done
	cmp	eax,2
	je	$L006clear_avx
$L008clear_xmm:
	and	ebp,4261412861
	and	esi,4278190079
$L006clear_avx:
	and	ebp,4026525695
	and	DWORD PTR 8[edi],4294967263
$L007done:
	mov	eax,esi
	mov	edx,ebp
$L000nocpuid:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_OPENSSL_ia32_cpuid ENDP
;EXTERN	_OPENSSL_ia32cap_P:NEAR
ALIGN	16
_OPENSSL_rdtsc	PROC PUBLIC
$L_OPENSSL_rdtsc_begin::
	xor	eax,eax
	xor	edx,edx
	lea	ecx,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [ecx],4
	jnc	$L009notsc
	rdtsc
$L009notsc:
	ret
_OPENSSL_rdtsc ENDP
ALIGN	16
_OPENSSL_instrument_halt	PROC PUBLIC
$L_OPENSSL_instrument_halt_begin::
	lea	ecx,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [ecx],4
	jnc	$L010nohalt
DD	2421723150
	and	eax,3
	jnz	$L010nohalt
	pushfd
	pop	eax
	bt	eax,9
	jnc	$L010nohalt
	rdtsc
	push	edx
	push	eax
	hlt
	rdtsc
	sub	eax,DWORD PTR [esp]
	sbb	edx,DWORD PTR 4[esp]
	add	esp,8
	ret
$L010nohalt:
	xor	eax,eax
	xor	edx,edx
	ret
_OPENSSL_instrument_halt ENDP
ALIGN	16
_OPENSSL_far_spin	PROC PUBLIC
$L_OPENSSL_far_spin_begin::
	pushfd
	pop	eax
	bt	eax,9
	jnc	$L011nospin
	mov	eax,DWORD PTR 4[esp]
	mov	ecx,DWORD PTR 8[esp]
DD	2430111262
	xor	eax,eax
	mov	edx,DWORD PTR [ecx]
	jmp	$L012spin
ALIGN	16
$L012spin:
	inc	eax
	cmp	edx,DWORD PTR [ecx]
	je	$L012spin
DD	529567888
	ret
$L011nospin:
	xor	eax,eax
	xor	edx,edx
	ret
_OPENSSL_far_spin ENDP
ALIGN	16
_OPENSSL_wipe_cpu	PROC PUBLIC
$L_OPENSSL_wipe_cpu_begin::
	xor	eax,eax
	xor	edx,edx
	lea	ecx,DWORD PTR _OPENSSL_ia32cap_P
	mov	ecx,DWORD PTR [ecx]
	bt	DWORD PTR [ecx],1
	jnc	$L013no_x87
	and	ecx,83886080
	cmp	ecx,83886080
	jne	$L014no_sse2
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
$L014no_sse2:
DD	4007259865,4007259865,4007259865,4007259865
DD	2430851995
$L013no_x87:
	lea	eax,DWORD PTR 4[esp]
	ret
_OPENSSL_wipe_cpu ENDP
ALIGN	16
_OPENSSL_atomic_add	PROC PUBLIC
$L_OPENSSL_atomic_add_begin::
	mov	edx,DWORD PTR 4[esp]
	mov	ecx,DWORD PTR 8[esp]
	push	ebx
	nop
	mov	eax,DWORD PTR [edx]
$L015spin:
	lea	ebx,DWORD PTR [ecx*1+eax]
	nop
DD	447811568
	jne	$L015spin
	mov	eax,ebx
	pop	ebx
	ret
_OPENSSL_atomic_add ENDP
ALIGN	16
_OPENSSL_indirect_call	PROC PUBLIC
$L_OPENSSL_indirect_call_begin::
	push	ebp
	mov	ebp,esp
	sub	esp,28
	mov	ecx,DWORD PTR 12[ebp]
	mov	DWORD PTR [esp],ecx
	mov	edx,DWORD PTR 16[ebp]
	mov	DWORD PTR 4[esp],edx
	mov	eax,DWORD PTR 20[ebp]
	mov	DWORD PTR 8[esp],eax
	mov	eax,DWORD PTR 24[ebp]
	mov	DWORD PTR 12[esp],eax
	mov	eax,DWORD PTR 28[ebp]
	mov	DWORD PTR 16[esp],eax
	mov	eax,DWORD PTR 32[ebp]
	mov	DWORD PTR 20[esp],eax
	mov	eax,DWORD PTR 36[ebp]
	mov	DWORD PTR 24[esp],eax
	call	DWORD PTR 8[ebp]
	mov	esp,ebp
	pop	ebp
	ret
_OPENSSL_indirect_call ENDP
ALIGN	16
_OPENSSL_cleanse	PROC PUBLIC
$L_OPENSSL_cleanse_begin::
	mov	edx,DWORD PTR 4[esp]
	mov	ecx,DWORD PTR 8[esp]
	xor	eax,eax
	cmp	ecx,7
	jae	$L016lot
	cmp	ecx,0
	je	$L017ret
$L018little:
	mov	BYTE PTR [edx],al
	sub	ecx,1
	lea	edx,DWORD PTR 1[edx]
	jnz	$L018little
$L017ret:
	ret
ALIGN	16
$L016lot:
	test	edx,3
	jz	$L019aligned
	mov	BYTE PTR [edx],al
	lea	ecx,DWORD PTR [ecx-1]
	lea	edx,DWORD PTR 1[edx]
	jmp	$L016lot
$L019aligned:
	mov	DWORD PTR [edx],eax
	lea	ecx,DWORD PTR [ecx-4]
	test	ecx,-4
	lea	edx,DWORD PTR 4[edx]
	jnz	$L019aligned
	cmp	ecx,0
	jne	$L018little
	ret
_OPENSSL_cleanse ENDP
ALIGN	16
_OPENSSL_ia32_rdrand	PROC PUBLIC
$L_OPENSSL_ia32_rdrand_begin::
	mov	ecx,8
$L020loop:
DB	15,199,240
	jc	$L021break
	loop	$L020loop
$L021break:
	cmp	eax,0
	cmove	eax,ecx
	ret
_OPENSSL_ia32_rdrand ENDP
ALIGN	16
_OPENSSL_ia32_rdseed	PROC PUBLIC
$L_OPENSSL_ia32_rdseed_begin::
	mov	ecx,8
$L022loop:
DB	15,199,248
	jc	$L023break
	loop	$L022loop
$L023break:
	cmp	eax,0
	cmove	eax,ecx
	ret
_OPENSSL_ia32_rdseed ENDP
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD:4
.bss	ENDS
.CRT$XCU	SEGMENT DWORD PUBLIC 'DATA'
EXTERN	_OPENSSL_cpuid_setup:NEAR
DD	_OPENSSL_cpuid_setup
.CRT$XCU	ENDS
END
