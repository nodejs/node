TITLE	x86cpuid.asm
IF @Version LT 800
ECHO MASM version 8.00 or later is strongly recommended.
ENDIF
.586
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
	bt	ecx,21
	jnc	$L000done
	xor	eax,eax
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
	cmp	eax,2147483656
	jb	$L001intel
	mov	eax,2147483656
	cpuid
	movzx	esi,cl
	inc	esi
	mov	eax,1
	cpuid
	bt	edx,28
	jnc	$L000done
	shr	ebx,16
	and	ebx,255
	cmp	ebx,esi
	ja	$L000done
	and	edx,4026531839
	jmp	$L000done
$L001intel:
	cmp	edi,4
	mov	edi,-1
	jb	$L002nocacheinfo
	mov	eax,4
	mov	ecx,0
	cpuid
	mov	edi,eax
	shr	edi,14
	and	edi,4095
$L002nocacheinfo:
	mov	eax,1
	cpuid
	cmp	ebp,0
	jne	$L003notP4
	and	ah,15
	cmp	ah,15
	jne	$L003notP4
	or	edx,1048576
$L003notP4:
	bt	edx,28
	jnc	$L000done
	and	edx,4026531839
	cmp	edi,0
	je	$L000done
	or	edx,268435456
	shr	ebx,16
	cmp	bl,1
	ja	$L000done
	and	edx,4026531839
$L000done:
	mov	eax,edx
	mov	edx,ecx
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
	jnc	$L004notsc
	rdtsc
$L004notsc:
	ret
_OPENSSL_rdtsc ENDP
ALIGN	16
_OPENSSL_instrument_halt	PROC PUBLIC
$L_OPENSSL_instrument_halt_begin::
	lea	ecx,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [ecx],4
	jnc	$L005nohalt
DD	2421723150
	and	eax,3
	jnz	$L005nohalt
	pushfd
	pop	eax
	bt	eax,9
	jnc	$L005nohalt
	rdtsc
	push	edx
	push	eax
	hlt
	rdtsc
	sub	eax,DWORD PTR [esp]
	sbb	edx,DWORD PTR 4[esp]
	add	esp,8
	ret
$L005nohalt:
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
	jnc	$L006nospin
	mov	eax,DWORD PTR 4[esp]
	mov	ecx,DWORD PTR 8[esp]
DD	2430111262
	xor	eax,eax
	mov	edx,DWORD PTR [ecx]
	jmp	$L007spin
ALIGN	16
$L007spin:
	inc	eax
	cmp	edx,DWORD PTR [ecx]
	je	$L007spin
DD	529567888
	ret
$L006nospin:
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
	jnc	$L008no_x87
DD	4007259865,4007259865,4007259865,4007259865,2430851995
$L008no_x87:
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
$L009spin:
	lea	ebx,DWORD PTR [ecx*1+eax]
	nop
DD	447811568
	jne	$L009spin
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
	jae	$L010lot
	cmp	ecx,0
	je	$L011ret
$L012little:
	mov	BYTE PTR [edx],al
	sub	ecx,1
	lea	edx,DWORD PTR 1[edx]
	jnz	$L012little
$L011ret:
	ret
ALIGN	16
$L010lot:
	test	edx,3
	jz	$L013aligned
	mov	BYTE PTR [edx],al
	lea	ecx,DWORD PTR [ecx-1]
	lea	edx,DWORD PTR 1[edx]
	jmp	$L010lot
$L013aligned:
	mov	DWORD PTR [edx],eax
	lea	ecx,DWORD PTR [ecx-4]
	test	ecx,-4
	lea	edx,DWORD PTR 4[edx]
	jnz	$L013aligned
	cmp	ecx,0
	jne	$L012little
	ret
_OPENSSL_cleanse ENDP
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD
.bss	ENDS
.CRT$XCU	SEGMENT DWORD PUBLIC 'DATA'
EXTERN	_OPENSSL_cpuid_setup:NEAR
DD	_OPENSSL_cpuid_setup
.CRT$XCU	ENDS
END
