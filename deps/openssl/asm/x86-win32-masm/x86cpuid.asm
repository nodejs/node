TITLE	x86cpuid.asm
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
	mov	edi,-1
	jb	$L003nocacheinfo
	mov	eax,4
	mov	ecx,0
	cpuid
	mov	edi,eax
	shr	edi,14
	and	edi,4095
$L003nocacheinfo:
	mov	eax,1
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
	cmp	edi,0
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
	bt	ecx,27
	jnc	$L005clear_avx
	xor	ecx,ecx
DB	15,1,208
	and	eax,6
	cmp	eax,6
	je	$L006done
	cmp	eax,2
	je	$L005clear_avx
$L007clear_xmm:
	and	ebp,4261412861
	and	esi,4278190079
$L005clear_avx:
	and	ebp,4026525695
$L006done:
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
	jnc	$L008notsc
	rdtsc
$L008notsc:
	ret
_OPENSSL_rdtsc ENDP
ALIGN	16
_OPENSSL_instrument_halt	PROC PUBLIC
$L_OPENSSL_instrument_halt_begin::
	lea	ecx,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [ecx],4
	jnc	$L009nohalt
DD	2421723150
	and	eax,3
	jnz	$L009nohalt
	pushfd
	pop	eax
	bt	eax,9
	jnc	$L009nohalt
	rdtsc
	push	edx
	push	eax
	hlt
	rdtsc
	sub	eax,DWORD PTR [esp]
	sbb	edx,DWORD PTR 4[esp]
	add	esp,8
	ret
$L009nohalt:
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
	jnc	$L010nospin
	mov	eax,DWORD PTR 4[esp]
	mov	ecx,DWORD PTR 8[esp]
DD	2430111262
	xor	eax,eax
	mov	edx,DWORD PTR [ecx]
	jmp	$L011spin
ALIGN	16
$L011spin:
	inc	eax
	cmp	edx,DWORD PTR [ecx]
	je	$L011spin
DD	529567888
	ret
$L010nospin:
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
	jnc	$L012no_x87
DD	4007259865,4007259865,4007259865,4007259865,2430851995
$L012no_x87:
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
$L013spin:
	lea	ebx,DWORD PTR [ecx*1+eax]
	nop
DD	447811568
	jne	$L013spin
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
	jae	$L014lot
	cmp	ecx,0
	je	$L015ret
$L016little:
	mov	BYTE PTR [edx],al
	sub	ecx,1
	lea	edx,DWORD PTR 1[edx]
	jnz	$L016little
$L015ret:
	ret
ALIGN	16
$L014lot:
	test	edx,3
	jz	$L017aligned
	mov	BYTE PTR [edx],al
	lea	ecx,DWORD PTR [ecx-1]
	lea	edx,DWORD PTR 1[edx]
	jmp	$L014lot
$L017aligned:
	mov	DWORD PTR [edx],eax
	lea	ecx,DWORD PTR [ecx-4]
	test	ecx,-4
	lea	edx,DWORD PTR 4[edx]
	jnz	$L017aligned
	cmp	ecx,0
	jne	$L016little
	ret
_OPENSSL_cleanse ENDP
ALIGN	16
_OPENSSL_ia32_rdrand	PROC PUBLIC
$L_OPENSSL_ia32_rdrand_begin::
	mov	ecx,8
$L018loop:
DB	15,199,240
	jc	$L019break
	loop	$L018loop
$L019break:
	cmp	eax,0
	cmove	eax,ecx
	ret
_OPENSSL_ia32_rdrand ENDP
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:QWORD
.bss	ENDS
.CRT$XCU	SEGMENT DWORD PUBLIC 'DATA'
EXTERN	_OPENSSL_cpuid_setup:NEAR
DD	_OPENSSL_cpuid_setup
.CRT$XCU	ENDS
END
