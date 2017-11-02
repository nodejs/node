OPTION	DOTNAME
EXTERN	OPENSSL_cpuid_setup:NEAR

.CRT$XCU	SEGMENT READONLY ALIGN(8)
		DQ	OPENSSL_cpuid_setup


.CRT$XCU	ENDS
_DATA	SEGMENT
COMM	OPENSSL_ia32cap_P:DWORD:4

_DATA	ENDS
.text$	SEGMENT ALIGN(256) 'CODE'

PUBLIC	OPENSSL_atomic_add

ALIGN	16
OPENSSL_atomic_add	PROC PUBLIC
	mov	eax,DWORD PTR[rcx]
$L$spin::	lea	r8,QWORD PTR[rax*1+rdx]
DB	0f0h
	cmpxchg	DWORD PTR[rcx],r8d
	jne	$L$spin
	mov	eax,r8d
DB	048h,098h
	DB	0F3h,0C3h		;repret
OPENSSL_atomic_add	ENDP

PUBLIC	OPENSSL_rdtsc

ALIGN	16
OPENSSL_rdtsc	PROC PUBLIC
	rdtsc
	shl	rdx,32
	or	rax,rdx
	DB	0F3h,0C3h		;repret
OPENSSL_rdtsc	ENDP

PUBLIC	OPENSSL_ia32_cpuid

ALIGN	16
OPENSSL_ia32_cpuid	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_OPENSSL_ia32_cpuid::
	mov	rdi,rcx


	mov	r8,rbx

	xor	eax,eax
	mov	DWORD PTR[8+rdi],eax
	cpuid
	mov	r11d,eax

	xor	eax,eax
	cmp	ebx,0756e6547h
	setne	al
	mov	r9d,eax
	cmp	edx,049656e69h
	setne	al
	or	r9d,eax
	cmp	ecx,06c65746eh
	setne	al
	or	r9d,eax
	jz	$L$intel

	cmp	ebx,068747541h
	setne	al
	mov	r10d,eax
	cmp	edx,069746E65h
	setne	al
	or	r10d,eax
	cmp	ecx,0444D4163h
	setne	al
	or	r10d,eax
	jnz	$L$intel


	mov	eax,080000000h
	cpuid
	cmp	eax,080000001h
	jb	$L$intel
	mov	r10d,eax
	mov	eax,080000001h
	cpuid
	or	r9d,ecx
	and	r9d,000000801h

	cmp	r10d,080000008h
	jb	$L$intel

	mov	eax,080000008h
	cpuid
	movzx	r10,cl
	inc	r10

	mov	eax,1
	cpuid
	bt	edx,28
	jnc	$L$generic
	shr	ebx,16
	cmp	bl,r10b
	ja	$L$generic
	and	edx,0efffffffh
	jmp	$L$generic

$L$intel::
	cmp	r11d,4
	mov	r10d,-1
	jb	$L$nocacheinfo

	mov	eax,4
	mov	ecx,0
	cpuid
	mov	r10d,eax
	shr	r10d,14
	and	r10d,0fffh

$L$nocacheinfo::
	mov	eax,1
	cpuid
	and	edx,0bfefffffh
	cmp	r9d,0
	jne	$L$notintel
	or	edx,040000000h
	and	ah,15
	cmp	ah,15
	jne	$L$notP4
	or	edx,000100000h
$L$notP4::
	cmp	ah,6
	jne	$L$notintel
	and	eax,00fff0ff0h
	cmp	eax,000050670h
	je	$L$knights
	cmp	eax,000080650h
	jne	$L$notintel
$L$knights::
	and	ecx,0fbffffffh

$L$notintel::
	bt	edx,28
	jnc	$L$generic
	and	edx,0efffffffh
	cmp	r10d,0
	je	$L$generic

	or	edx,010000000h
	shr	ebx,16
	cmp	bl,1
	ja	$L$generic
	and	edx,0efffffffh
$L$generic::
	and	r9d,000000800h
	and	ecx,0fffff7ffh
	or	r9d,ecx

	mov	r10d,edx

	cmp	r11d,7
	jb	$L$no_extended_info
	mov	eax,7
	xor	ecx,ecx
	cpuid
	bt	r9d,26
	jc	$L$notknights
	and	ebx,0fff7ffffh
$L$notknights::
	mov	DWORD PTR[8+rdi],ebx
$L$no_extended_info::

	bt	r9d,27
	jnc	$L$clear_avx
	xor	ecx,ecx
DB	00fh,001h,0d0h
	and	eax,6
	cmp	eax,6
	je	$L$done
$L$clear_avx::
	mov	eax,0efffe7ffh
	and	r9d,eax
	and	DWORD PTR[8+rdi],0ffffffdfh
$L$done::
	shl	r9,32
	mov	eax,r10d
	mov	rbx,r8
	or	rax,r9
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_OPENSSL_ia32_cpuid::
OPENSSL_ia32_cpuid	ENDP

PUBLIC	OPENSSL_cleanse

ALIGN	16
OPENSSL_cleanse	PROC PUBLIC
	xor	rax,rax
	cmp	rdx,15
	jae	$L$ot
	cmp	rdx,0
	je	$L$ret
$L$ittle::
	mov	BYTE PTR[rcx],al
	sub	rdx,1
	lea	rcx,QWORD PTR[1+rcx]
	jnz	$L$ittle
$L$ret::
	DB	0F3h,0C3h		;repret
ALIGN	16
$L$ot::
	test	rcx,7
	jz	$L$aligned
	mov	BYTE PTR[rcx],al
	lea	rdx,QWORD PTR[((-1))+rdx]
	lea	rcx,QWORD PTR[1+rcx]
	jmp	$L$ot
$L$aligned::
	mov	QWORD PTR[rcx],rax
	lea	rdx,QWORD PTR[((-8))+rdx]
	test	rdx,-8
	lea	rcx,QWORD PTR[8+rcx]
	jnz	$L$aligned
	cmp	rdx,0
	jne	$L$ittle
	DB	0F3h,0C3h		;repret
OPENSSL_cleanse	ENDP
PUBLIC	OPENSSL_wipe_cpu

ALIGN	16
OPENSSL_wipe_cpu	PROC PUBLIC
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	xor	rcx,rcx
	xor	rdx,rdx
	xor	r8,r8
	xor	r9,r9
	xor	r10,r10
	xor	r11,r11
	lea	rax,QWORD PTR[8+rsp]
	DB	0F3h,0C3h		;repret
OPENSSL_wipe_cpu	ENDP
PUBLIC	OPENSSL_ia32_rdrand

ALIGN	16
OPENSSL_ia32_rdrand	PROC PUBLIC
	mov	ecx,8
$L$oop_rdrand::
DB	72,15,199,240
	jc	$L$break_rdrand
	loop	$L$oop_rdrand
$L$break_rdrand::
	cmp	rax,0
	cmove	rax,rcx
	DB	0F3h,0C3h		;repret
OPENSSL_ia32_rdrand	ENDP

PUBLIC	OPENSSL_ia32_rdseed

ALIGN	16
OPENSSL_ia32_rdseed	PROC PUBLIC
	mov	ecx,8
$L$oop_rdseed::
DB	72,15,199,248
	jc	$L$break_rdseed
	loop	$L$oop_rdseed
$L$break_rdseed::
	cmp	rax,0
	cmove	rax,rcx
	DB	0F3h,0C3h		;repret
OPENSSL_ia32_rdseed	ENDP

.text$	ENDS
END
