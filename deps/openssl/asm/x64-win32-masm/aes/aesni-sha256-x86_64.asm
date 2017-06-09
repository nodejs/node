OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR
PUBLIC	aesni_cbc_sha256_enc

ALIGN	16
aesni_cbc_sha256_enc	PROC PUBLIC
	lea	r11,QWORD PTR[OPENSSL_ia32cap_P]
	mov	eax,1
	cmp	rcx,0
	je	$L$probe
	mov	eax,DWORD PTR[r11]
	mov	r10,QWORD PTR[4+r11]
	bt	r10,61
	jc	aesni_cbc_sha256_enc_shaext
	mov	r11,r10
	shr	r11,32

	test	r10d,2048
	jnz	aesni_cbc_sha256_enc_xop
	and	r11d,296
	cmp	r11d,296
	je	aesni_cbc_sha256_enc_avx2
	and	r10d,268435456
	jnz	aesni_cbc_sha256_enc_avx
	ud2
	xor	eax,eax
	cmp	rcx,0
	je	$L$probe
	ud2
$L$probe::
	DB	0F3h,0C3h		;repret
aesni_cbc_sha256_enc	ENDP

ALIGN	64

K256::
	DD	0428a2f98h,071374491h,0b5c0fbcfh,0e9b5dba5h
	DD	0428a2f98h,071374491h,0b5c0fbcfh,0e9b5dba5h
	DD	03956c25bh,059f111f1h,0923f82a4h,0ab1c5ed5h
	DD	03956c25bh,059f111f1h,0923f82a4h,0ab1c5ed5h
	DD	0d807aa98h,012835b01h,0243185beh,0550c7dc3h
	DD	0d807aa98h,012835b01h,0243185beh,0550c7dc3h
	DD	072be5d74h,080deb1feh,09bdc06a7h,0c19bf174h
	DD	072be5d74h,080deb1feh,09bdc06a7h,0c19bf174h
	DD	0e49b69c1h,0efbe4786h,00fc19dc6h,0240ca1cch
	DD	0e49b69c1h,0efbe4786h,00fc19dc6h,0240ca1cch
	DD	02de92c6fh,04a7484aah,05cb0a9dch,076f988dah
	DD	02de92c6fh,04a7484aah,05cb0a9dch,076f988dah
	DD	0983e5152h,0a831c66dh,0b00327c8h,0bf597fc7h
	DD	0983e5152h,0a831c66dh,0b00327c8h,0bf597fc7h
	DD	0c6e00bf3h,0d5a79147h,006ca6351h,014292967h
	DD	0c6e00bf3h,0d5a79147h,006ca6351h,014292967h
	DD	027b70a85h,02e1b2138h,04d2c6dfch,053380d13h
	DD	027b70a85h,02e1b2138h,04d2c6dfch,053380d13h
	DD	0650a7354h,0766a0abbh,081c2c92eh,092722c85h
	DD	0650a7354h,0766a0abbh,081c2c92eh,092722c85h
	DD	0a2bfe8a1h,0a81a664bh,0c24b8b70h,0c76c51a3h
	DD	0a2bfe8a1h,0a81a664bh,0c24b8b70h,0c76c51a3h
	DD	0d192e819h,0d6990624h,0f40e3585h,0106aa070h
	DD	0d192e819h,0d6990624h,0f40e3585h,0106aa070h
	DD	019a4c116h,01e376c08h,02748774ch,034b0bcb5h
	DD	019a4c116h,01e376c08h,02748774ch,034b0bcb5h
	DD	0391c0cb3h,04ed8aa4ah,05b9cca4fh,0682e6ff3h
	DD	0391c0cb3h,04ed8aa4ah,05b9cca4fh,0682e6ff3h
	DD	0748f82eeh,078a5636fh,084c87814h,08cc70208h
	DD	0748f82eeh,078a5636fh,084c87814h,08cc70208h
	DD	090befffah,0a4506cebh,0bef9a3f7h,0c67178f2h
	DD	090befffah,0a4506cebh,0bef9a3f7h,0c67178f2h

	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
	DD	0,0,0,0,0,0,0,0,-1,-1,-1,-1
	DD	0,0,0,0,0,0,0,0
DB	65,69,83,78,73,45,67,66,67,43,83,72,65,50,53,54
DB	32,115,116,105,116,99,104,32,102,111,114,32,120,56,54,95
DB	54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98
DB	121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108
DB	46,111,114,103,62,0
ALIGN	64

ALIGN	64
aesni_cbc_sha256_enc_xop	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_cbc_sha256_enc_xop::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


$L$xop_shortcut::
	mov	r10,QWORD PTR[56+rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	r11,rsp
	sub	rsp,288
	and	rsp,-64

	shl	rdx,6
	sub	rsi,rdi
	sub	r10,rdi
	add	rdx,rdi


	mov	QWORD PTR[((64+8))+rsp],rsi
	mov	QWORD PTR[((64+16))+rsp],rdx

	mov	QWORD PTR[((64+32))+rsp],r8
	mov	QWORD PTR[((64+40))+rsp],r9
	mov	QWORD PTR[((64+48))+rsp],r10
	mov	QWORD PTR[((64+56))+rsp],r11
	movaps	XMMWORD PTR[128+rsp],xmm6
	movaps	XMMWORD PTR[144+rsp],xmm7
	movaps	XMMWORD PTR[160+rsp],xmm8
	movaps	XMMWORD PTR[176+rsp],xmm9
	movaps	XMMWORD PTR[192+rsp],xmm10
	movaps	XMMWORD PTR[208+rsp],xmm11
	movaps	XMMWORD PTR[224+rsp],xmm12
	movaps	XMMWORD PTR[240+rsp],xmm13
	movaps	XMMWORD PTR[256+rsp],xmm14
	movaps	XMMWORD PTR[272+rsp],xmm15
$L$prologue_xop::
	vzeroall

	mov	r12,rdi
	lea	rdi,QWORD PTR[128+rcx]
	lea	r13,QWORD PTR[((K256+544))]
	mov	r14d,DWORD PTR[((240-128))+rdi]
	mov	r15,r9
	mov	rsi,r10
	vmovdqu	xmm8,XMMWORD PTR[r8]
	sub	r14,9

	mov	eax,DWORD PTR[r15]
	mov	ebx,DWORD PTR[4+r15]
	mov	ecx,DWORD PTR[8+r15]
	mov	edx,DWORD PTR[12+r15]
	mov	r8d,DWORD PTR[16+r15]
	mov	r9d,DWORD PTR[20+r15]
	mov	r10d,DWORD PTR[24+r15]
	mov	r11d,DWORD PTR[28+r15]

	vmovdqa	xmm14,XMMWORD PTR[r14*8+r13]
	vmovdqa	xmm13,XMMWORD PTR[16+r14*8+r13]
	vmovdqa	xmm12,XMMWORD PTR[32+r14*8+r13]
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	jmp	$L$loop_xop
ALIGN	16
$L$loop_xop::
	vmovdqa	xmm7,XMMWORD PTR[((K256+512))]
	vmovdqu	xmm0,XMMWORD PTR[r12*1+rsi]
	vmovdqu	xmm1,XMMWORD PTR[16+r12*1+rsi]
	vmovdqu	xmm2,XMMWORD PTR[32+r12*1+rsi]
	vmovdqu	xmm3,XMMWORD PTR[48+r12*1+rsi]
	vpshufb	xmm0,xmm0,xmm7
	lea	rbp,QWORD PTR[K256]
	vpshufb	xmm1,xmm1,xmm7
	vpshufb	xmm2,xmm2,xmm7
	vpaddd	xmm4,xmm0,XMMWORD PTR[rbp]
	vpshufb	xmm3,xmm3,xmm7
	vpaddd	xmm5,xmm1,XMMWORD PTR[32+rbp]
	vpaddd	xmm6,xmm2,XMMWORD PTR[64+rbp]
	vpaddd	xmm7,xmm3,XMMWORD PTR[96+rbp]
	vmovdqa	XMMWORD PTR[rsp],xmm4
	mov	r14d,eax
	vmovdqa	XMMWORD PTR[16+rsp],xmm5
	mov	esi,ebx
	vmovdqa	XMMWORD PTR[32+rsp],xmm6
	xor	esi,ecx
	vmovdqa	XMMWORD PTR[48+rsp],xmm7
	mov	r13d,r8d
	jmp	$L$xop_00_47

ALIGN	16
$L$xop_00_47::
	sub	rbp,-16*2*4
	vmovdqu	xmm9,XMMWORD PTR[r12]
	mov	QWORD PTR[((64+0))+rsp],r12
	vpalignr	xmm4,xmm1,xmm0,4
	ror	r13d,14
	mov	eax,r14d
	vpalignr	xmm7,xmm3,xmm2,4
	mov	r12d,r9d
	xor	r13d,r8d
DB	143,232,120,194,236,14
	ror	r14d,9
	xor	r12d,r10d
	vpsrld	xmm4,xmm4,3
	ror	r13d,5
	xor	r14d,eax
	vpaddd	xmm0,xmm0,xmm7
	and	r12d,r8d
	vpxor	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((16-128))+rdi]
	xor	r13d,r8d
	add	r11d,DWORD PTR[rsp]
	mov	r15d,eax
DB	143,232,120,194,245,11
	ror	r14d,11
	xor	r12d,r10d
	vpxor	xmm4,xmm4,xmm5
	xor	r15d,ebx
	ror	r13d,6
	add	r11d,r12d
	and	esi,r15d
DB	143,232,120,194,251,13
	xor	r14d,eax
	add	r11d,r13d
	vpxor	xmm4,xmm4,xmm6
	xor	esi,ebx
	add	edx,r11d
	vpsrld	xmm6,xmm3,10
	ror	r14d,2
	add	r11d,esi
	vpaddd	xmm0,xmm0,xmm4
	mov	r13d,edx
	add	r14d,r11d
DB	143,232,120,194,239,2
	ror	r13d,14
	mov	r11d,r14d
	vpxor	xmm7,xmm7,xmm6
	mov	r12d,r8d
	xor	r13d,edx
	ror	r14d,9
	xor	r12d,r9d
	vpxor	xmm7,xmm7,xmm5
	ror	r13d,5
	xor	r14d,r11d
	and	r12d,edx
	vpxor	xmm9,xmm9,xmm8
	xor	r13d,edx
	vpsrldq	xmm7,xmm7,8
	add	r10d,DWORD PTR[4+rsp]
	mov	esi,r11d
	ror	r14d,11
	xor	r12d,r9d
	vpaddd	xmm0,xmm0,xmm7
	xor	esi,eax
	ror	r13d,6
	add	r10d,r12d
	and	r15d,esi
DB	143,232,120,194,248,13
	xor	r14d,r11d
	add	r10d,r13d
	vpsrld	xmm6,xmm0,10
	xor	r15d,eax
	add	ecx,r10d
DB	143,232,120,194,239,2
	ror	r14d,2
	add	r10d,r15d
	vpxor	xmm7,xmm7,xmm6
	mov	r13d,ecx
	add	r14d,r10d
	ror	r13d,14
	mov	r10d,r14d
	vpxor	xmm7,xmm7,xmm5
	mov	r12d,edx
	xor	r13d,ecx
	ror	r14d,9
	xor	r12d,r8d
	vpslldq	xmm7,xmm7,8
	ror	r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((32-128))+rdi]
	xor	r13d,ecx
	vpaddd	xmm0,xmm0,xmm7
	add	r9d,DWORD PTR[8+rsp]
	mov	r15d,r10d
	ror	r14d,11
	xor	r12d,r8d
	vpaddd	xmm6,xmm0,XMMWORD PTR[rbp]
	xor	r15d,r11d
	ror	r13d,6
	add	r9d,r12d
	and	esi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	xor	esi,r11d
	add	ebx,r9d
	ror	r14d,2
	add	r9d,esi
	mov	r13d,ebx
	add	r14d,r9d
	ror	r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	xor	r13d,ebx
	ror	r14d,9
	xor	r12d,edx
	ror	r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((48-128))+rdi]
	xor	r13d,ebx
	add	r8d,DWORD PTR[12+rsp]
	mov	esi,r9d
	ror	r14d,11
	xor	r12d,edx
	xor	esi,r10d
	ror	r13d,6
	add	r8d,r12d
	and	r15d,esi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	add	eax,r8d
	ror	r14d,2
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	vmovdqa	XMMWORD PTR[rsp],xmm6
	vpalignr	xmm4,xmm2,xmm1,4
	ror	r13d,14
	mov	r8d,r14d
	vpalignr	xmm7,xmm0,xmm3,4
	mov	r12d,ebx
	xor	r13d,eax
DB	143,232,120,194,236,14
	ror	r14d,9
	xor	r12d,ecx
	vpsrld	xmm4,xmm4,3
	ror	r13d,5
	xor	r14d,r8d
	vpaddd	xmm1,xmm1,xmm7
	and	r12d,eax
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((64-128))+rdi]
	xor	r13d,eax
	add	edx,DWORD PTR[16+rsp]
	mov	r15d,r8d
DB	143,232,120,194,245,11
	ror	r14d,11
	xor	r12d,ecx
	vpxor	xmm4,xmm4,xmm5
	xor	r15d,r9d
	ror	r13d,6
	add	edx,r12d
	and	esi,r15d
DB	143,232,120,194,248,13
	xor	r14d,r8d
	add	edx,r13d
	vpxor	xmm4,xmm4,xmm6
	xor	esi,r9d
	add	r11d,edx
	vpsrld	xmm6,xmm0,10
	ror	r14d,2
	add	edx,esi
	vpaddd	xmm1,xmm1,xmm4
	mov	r13d,r11d
	add	r14d,edx
DB	143,232,120,194,239,2
	ror	r13d,14
	mov	edx,r14d
	vpxor	xmm7,xmm7,xmm6
	mov	r12d,eax
	xor	r13d,r11d
	ror	r14d,9
	xor	r12d,ebx
	vpxor	xmm7,xmm7,xmm5
	ror	r13d,5
	xor	r14d,edx
	and	r12d,r11d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((80-128))+rdi]
	xor	r13d,r11d
	vpsrldq	xmm7,xmm7,8
	add	ecx,DWORD PTR[20+rsp]
	mov	esi,edx
	ror	r14d,11
	xor	r12d,ebx
	vpaddd	xmm1,xmm1,xmm7
	xor	esi,r8d
	ror	r13d,6
	add	ecx,r12d
	and	r15d,esi
DB	143,232,120,194,249,13
	xor	r14d,edx
	add	ecx,r13d
	vpsrld	xmm6,xmm1,10
	xor	r15d,r8d
	add	r10d,ecx
DB	143,232,120,194,239,2
	ror	r14d,2
	add	ecx,r15d
	vpxor	xmm7,xmm7,xmm6
	mov	r13d,r10d
	add	r14d,ecx
	ror	r13d,14
	mov	ecx,r14d
	vpxor	xmm7,xmm7,xmm5
	mov	r12d,r11d
	xor	r13d,r10d
	ror	r14d,9
	xor	r12d,eax
	vpslldq	xmm7,xmm7,8
	ror	r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((96-128))+rdi]
	xor	r13d,r10d
	vpaddd	xmm1,xmm1,xmm7
	add	ebx,DWORD PTR[24+rsp]
	mov	r15d,ecx
	ror	r14d,11
	xor	r12d,eax
	vpaddd	xmm6,xmm1,XMMWORD PTR[32+rbp]
	xor	r15d,edx
	ror	r13d,6
	add	ebx,r12d
	and	esi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	xor	esi,edx
	add	r9d,ebx
	ror	r14d,2
	add	ebx,esi
	mov	r13d,r9d
	add	r14d,ebx
	ror	r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	xor	r13d,r9d
	ror	r14d,9
	xor	r12d,r11d
	ror	r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((112-128))+rdi]
	xor	r13d,r9d
	add	eax,DWORD PTR[28+rsp]
	mov	esi,ebx
	ror	r14d,11
	xor	r12d,r11d
	xor	esi,ecx
	ror	r13d,6
	add	eax,r12d
	and	r15d,esi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	add	r8d,eax
	ror	r14d,2
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	vmovdqa	XMMWORD PTR[16+rsp],xmm6
	vpalignr	xmm4,xmm3,xmm2,4
	ror	r13d,14
	mov	eax,r14d
	vpalignr	xmm7,xmm1,xmm0,4
	mov	r12d,r9d
	xor	r13d,r8d
DB	143,232,120,194,236,14
	ror	r14d,9
	xor	r12d,r10d
	vpsrld	xmm4,xmm4,3
	ror	r13d,5
	xor	r14d,eax
	vpaddd	xmm2,xmm2,xmm7
	and	r12d,r8d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((128-128))+rdi]
	xor	r13d,r8d
	add	r11d,DWORD PTR[32+rsp]
	mov	r15d,eax
DB	143,232,120,194,245,11
	ror	r14d,11
	xor	r12d,r10d
	vpxor	xmm4,xmm4,xmm5
	xor	r15d,ebx
	ror	r13d,6
	add	r11d,r12d
	and	esi,r15d
DB	143,232,120,194,249,13
	xor	r14d,eax
	add	r11d,r13d
	vpxor	xmm4,xmm4,xmm6
	xor	esi,ebx
	add	edx,r11d
	vpsrld	xmm6,xmm1,10
	ror	r14d,2
	add	r11d,esi
	vpaddd	xmm2,xmm2,xmm4
	mov	r13d,edx
	add	r14d,r11d
DB	143,232,120,194,239,2
	ror	r13d,14
	mov	r11d,r14d
	vpxor	xmm7,xmm7,xmm6
	mov	r12d,r8d
	xor	r13d,edx
	ror	r14d,9
	xor	r12d,r9d
	vpxor	xmm7,xmm7,xmm5
	ror	r13d,5
	xor	r14d,r11d
	and	r12d,edx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((144-128))+rdi]
	xor	r13d,edx
	vpsrldq	xmm7,xmm7,8
	add	r10d,DWORD PTR[36+rsp]
	mov	esi,r11d
	ror	r14d,11
	xor	r12d,r9d
	vpaddd	xmm2,xmm2,xmm7
	xor	esi,eax
	ror	r13d,6
	add	r10d,r12d
	and	r15d,esi
DB	143,232,120,194,250,13
	xor	r14d,r11d
	add	r10d,r13d
	vpsrld	xmm6,xmm2,10
	xor	r15d,eax
	add	ecx,r10d
DB	143,232,120,194,239,2
	ror	r14d,2
	add	r10d,r15d
	vpxor	xmm7,xmm7,xmm6
	mov	r13d,ecx
	add	r14d,r10d
	ror	r13d,14
	mov	r10d,r14d
	vpxor	xmm7,xmm7,xmm5
	mov	r12d,edx
	xor	r13d,ecx
	ror	r14d,9
	xor	r12d,r8d
	vpslldq	xmm7,xmm7,8
	ror	r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((160-128))+rdi]
	xor	r13d,ecx
	vpaddd	xmm2,xmm2,xmm7
	add	r9d,DWORD PTR[40+rsp]
	mov	r15d,r10d
	ror	r14d,11
	xor	r12d,r8d
	vpaddd	xmm6,xmm2,XMMWORD PTR[64+rbp]
	xor	r15d,r11d
	ror	r13d,6
	add	r9d,r12d
	and	esi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	xor	esi,r11d
	add	ebx,r9d
	ror	r14d,2
	add	r9d,esi
	mov	r13d,ebx
	add	r14d,r9d
	ror	r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	xor	r13d,ebx
	ror	r14d,9
	xor	r12d,edx
	ror	r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((176-128))+rdi]
	xor	r13d,ebx
	add	r8d,DWORD PTR[44+rsp]
	mov	esi,r9d
	ror	r14d,11
	xor	r12d,edx
	xor	esi,r10d
	ror	r13d,6
	add	r8d,r12d
	and	r15d,esi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	add	eax,r8d
	ror	r14d,2
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	vmovdqa	XMMWORD PTR[32+rsp],xmm6
	vpalignr	xmm4,xmm0,xmm3,4
	ror	r13d,14
	mov	r8d,r14d
	vpalignr	xmm7,xmm2,xmm1,4
	mov	r12d,ebx
	xor	r13d,eax
DB	143,232,120,194,236,14
	ror	r14d,9
	xor	r12d,ecx
	vpsrld	xmm4,xmm4,3
	ror	r13d,5
	xor	r14d,r8d
	vpaddd	xmm3,xmm3,xmm7
	and	r12d,eax
	vpand	xmm8,xmm11,xmm12
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((192-128))+rdi]
	xor	r13d,eax
	add	edx,DWORD PTR[48+rsp]
	mov	r15d,r8d
DB	143,232,120,194,245,11
	ror	r14d,11
	xor	r12d,ecx
	vpxor	xmm4,xmm4,xmm5
	xor	r15d,r9d
	ror	r13d,6
	add	edx,r12d
	and	esi,r15d
DB	143,232,120,194,250,13
	xor	r14d,r8d
	add	edx,r13d
	vpxor	xmm4,xmm4,xmm6
	xor	esi,r9d
	add	r11d,edx
	vpsrld	xmm6,xmm2,10
	ror	r14d,2
	add	edx,esi
	vpaddd	xmm3,xmm3,xmm4
	mov	r13d,r11d
	add	r14d,edx
DB	143,232,120,194,239,2
	ror	r13d,14
	mov	edx,r14d
	vpxor	xmm7,xmm7,xmm6
	mov	r12d,eax
	xor	r13d,r11d
	ror	r14d,9
	xor	r12d,ebx
	vpxor	xmm7,xmm7,xmm5
	ror	r13d,5
	xor	r14d,edx
	and	r12d,r11d
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((208-128))+rdi]
	xor	r13d,r11d
	vpsrldq	xmm7,xmm7,8
	add	ecx,DWORD PTR[52+rsp]
	mov	esi,edx
	ror	r14d,11
	xor	r12d,ebx
	vpaddd	xmm3,xmm3,xmm7
	xor	esi,r8d
	ror	r13d,6
	add	ecx,r12d
	and	r15d,esi
DB	143,232,120,194,251,13
	xor	r14d,edx
	add	ecx,r13d
	vpsrld	xmm6,xmm3,10
	xor	r15d,r8d
	add	r10d,ecx
DB	143,232,120,194,239,2
	ror	r14d,2
	add	ecx,r15d
	vpxor	xmm7,xmm7,xmm6
	mov	r13d,r10d
	add	r14d,ecx
	ror	r13d,14
	mov	ecx,r14d
	vpxor	xmm7,xmm7,xmm5
	mov	r12d,r11d
	xor	r13d,r10d
	ror	r14d,9
	xor	r12d,eax
	vpslldq	xmm7,xmm7,8
	ror	r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	vpand	xmm11,xmm11,xmm13
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((224-128))+rdi]
	xor	r13d,r10d
	vpaddd	xmm3,xmm3,xmm7
	add	ebx,DWORD PTR[56+rsp]
	mov	r15d,ecx
	ror	r14d,11
	xor	r12d,eax
	vpaddd	xmm6,xmm3,XMMWORD PTR[96+rbp]
	xor	r15d,edx
	ror	r13d,6
	add	ebx,r12d
	and	esi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	xor	esi,edx
	add	r9d,ebx
	ror	r14d,2
	add	ebx,esi
	mov	r13d,r9d
	add	r14d,ebx
	ror	r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	xor	r13d,r9d
	ror	r14d,9
	xor	r12d,r11d
	ror	r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	vpor	xmm8,xmm8,xmm11
	vaesenclast	xmm11,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	xor	r13d,r9d
	add	eax,DWORD PTR[60+rsp]
	mov	esi,ebx
	ror	r14d,11
	xor	r12d,r11d
	xor	esi,ecx
	ror	r13d,6
	add	eax,r12d
	and	r15d,esi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	add	r8d,eax
	ror	r14d,2
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	vmovdqa	XMMWORD PTR[48+rsp],xmm6
	mov	r12,QWORD PTR[((64+0))+rsp]
	vpand	xmm11,xmm11,xmm14
	mov	r15,QWORD PTR[((64+8))+rsp]
	vpor	xmm8,xmm8,xmm11
	vmovdqu	XMMWORD PTR[r12*1+r15],xmm8
	lea	r12,QWORD PTR[16+r12]
	cmp	BYTE PTR[131+rbp],0
	jne	$L$xop_00_47
	vmovdqu	xmm9,XMMWORD PTR[r12]
	mov	QWORD PTR[((64+0))+rsp],r12
	ror	r13d,14
	mov	eax,r14d
	mov	r12d,r9d
	xor	r13d,r8d
	ror	r14d,9
	xor	r12d,r10d
	ror	r13d,5
	xor	r14d,eax
	and	r12d,r8d
	vpxor	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((16-128))+rdi]
	xor	r13d,r8d
	add	r11d,DWORD PTR[rsp]
	mov	r15d,eax
	ror	r14d,11
	xor	r12d,r10d
	xor	r15d,ebx
	ror	r13d,6
	add	r11d,r12d
	and	esi,r15d
	xor	r14d,eax
	add	r11d,r13d
	xor	esi,ebx
	add	edx,r11d
	ror	r14d,2
	add	r11d,esi
	mov	r13d,edx
	add	r14d,r11d
	ror	r13d,14
	mov	r11d,r14d
	mov	r12d,r8d
	xor	r13d,edx
	ror	r14d,9
	xor	r12d,r9d
	ror	r13d,5
	xor	r14d,r11d
	and	r12d,edx
	vpxor	xmm9,xmm9,xmm8
	xor	r13d,edx
	add	r10d,DWORD PTR[4+rsp]
	mov	esi,r11d
	ror	r14d,11
	xor	r12d,r9d
	xor	esi,eax
	ror	r13d,6
	add	r10d,r12d
	and	r15d,esi
	xor	r14d,r11d
	add	r10d,r13d
	xor	r15d,eax
	add	ecx,r10d
	ror	r14d,2
	add	r10d,r15d
	mov	r13d,ecx
	add	r14d,r10d
	ror	r13d,14
	mov	r10d,r14d
	mov	r12d,edx
	xor	r13d,ecx
	ror	r14d,9
	xor	r12d,r8d
	ror	r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((32-128))+rdi]
	xor	r13d,ecx
	add	r9d,DWORD PTR[8+rsp]
	mov	r15d,r10d
	ror	r14d,11
	xor	r12d,r8d
	xor	r15d,r11d
	ror	r13d,6
	add	r9d,r12d
	and	esi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	xor	esi,r11d
	add	ebx,r9d
	ror	r14d,2
	add	r9d,esi
	mov	r13d,ebx
	add	r14d,r9d
	ror	r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	xor	r13d,ebx
	ror	r14d,9
	xor	r12d,edx
	ror	r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((48-128))+rdi]
	xor	r13d,ebx
	add	r8d,DWORD PTR[12+rsp]
	mov	esi,r9d
	ror	r14d,11
	xor	r12d,edx
	xor	esi,r10d
	ror	r13d,6
	add	r8d,r12d
	and	r15d,esi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	add	eax,r8d
	ror	r14d,2
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	ror	r13d,14
	mov	r8d,r14d
	mov	r12d,ebx
	xor	r13d,eax
	ror	r14d,9
	xor	r12d,ecx
	ror	r13d,5
	xor	r14d,r8d
	and	r12d,eax
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((64-128))+rdi]
	xor	r13d,eax
	add	edx,DWORD PTR[16+rsp]
	mov	r15d,r8d
	ror	r14d,11
	xor	r12d,ecx
	xor	r15d,r9d
	ror	r13d,6
	add	edx,r12d
	and	esi,r15d
	xor	r14d,r8d
	add	edx,r13d
	xor	esi,r9d
	add	r11d,edx
	ror	r14d,2
	add	edx,esi
	mov	r13d,r11d
	add	r14d,edx
	ror	r13d,14
	mov	edx,r14d
	mov	r12d,eax
	xor	r13d,r11d
	ror	r14d,9
	xor	r12d,ebx
	ror	r13d,5
	xor	r14d,edx
	and	r12d,r11d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((80-128))+rdi]
	xor	r13d,r11d
	add	ecx,DWORD PTR[20+rsp]
	mov	esi,edx
	ror	r14d,11
	xor	r12d,ebx
	xor	esi,r8d
	ror	r13d,6
	add	ecx,r12d
	and	r15d,esi
	xor	r14d,edx
	add	ecx,r13d
	xor	r15d,r8d
	add	r10d,ecx
	ror	r14d,2
	add	ecx,r15d
	mov	r13d,r10d
	add	r14d,ecx
	ror	r13d,14
	mov	ecx,r14d
	mov	r12d,r11d
	xor	r13d,r10d
	ror	r14d,9
	xor	r12d,eax
	ror	r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((96-128))+rdi]
	xor	r13d,r10d
	add	ebx,DWORD PTR[24+rsp]
	mov	r15d,ecx
	ror	r14d,11
	xor	r12d,eax
	xor	r15d,edx
	ror	r13d,6
	add	ebx,r12d
	and	esi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	xor	esi,edx
	add	r9d,ebx
	ror	r14d,2
	add	ebx,esi
	mov	r13d,r9d
	add	r14d,ebx
	ror	r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	xor	r13d,r9d
	ror	r14d,9
	xor	r12d,r11d
	ror	r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((112-128))+rdi]
	xor	r13d,r9d
	add	eax,DWORD PTR[28+rsp]
	mov	esi,ebx
	ror	r14d,11
	xor	r12d,r11d
	xor	esi,ecx
	ror	r13d,6
	add	eax,r12d
	and	r15d,esi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	add	r8d,eax
	ror	r14d,2
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	ror	r13d,14
	mov	eax,r14d
	mov	r12d,r9d
	xor	r13d,r8d
	ror	r14d,9
	xor	r12d,r10d
	ror	r13d,5
	xor	r14d,eax
	and	r12d,r8d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((128-128))+rdi]
	xor	r13d,r8d
	add	r11d,DWORD PTR[32+rsp]
	mov	r15d,eax
	ror	r14d,11
	xor	r12d,r10d
	xor	r15d,ebx
	ror	r13d,6
	add	r11d,r12d
	and	esi,r15d
	xor	r14d,eax
	add	r11d,r13d
	xor	esi,ebx
	add	edx,r11d
	ror	r14d,2
	add	r11d,esi
	mov	r13d,edx
	add	r14d,r11d
	ror	r13d,14
	mov	r11d,r14d
	mov	r12d,r8d
	xor	r13d,edx
	ror	r14d,9
	xor	r12d,r9d
	ror	r13d,5
	xor	r14d,r11d
	and	r12d,edx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((144-128))+rdi]
	xor	r13d,edx
	add	r10d,DWORD PTR[36+rsp]
	mov	esi,r11d
	ror	r14d,11
	xor	r12d,r9d
	xor	esi,eax
	ror	r13d,6
	add	r10d,r12d
	and	r15d,esi
	xor	r14d,r11d
	add	r10d,r13d
	xor	r15d,eax
	add	ecx,r10d
	ror	r14d,2
	add	r10d,r15d
	mov	r13d,ecx
	add	r14d,r10d
	ror	r13d,14
	mov	r10d,r14d
	mov	r12d,edx
	xor	r13d,ecx
	ror	r14d,9
	xor	r12d,r8d
	ror	r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((160-128))+rdi]
	xor	r13d,ecx
	add	r9d,DWORD PTR[40+rsp]
	mov	r15d,r10d
	ror	r14d,11
	xor	r12d,r8d
	xor	r15d,r11d
	ror	r13d,6
	add	r9d,r12d
	and	esi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	xor	esi,r11d
	add	ebx,r9d
	ror	r14d,2
	add	r9d,esi
	mov	r13d,ebx
	add	r14d,r9d
	ror	r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	xor	r13d,ebx
	ror	r14d,9
	xor	r12d,edx
	ror	r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((176-128))+rdi]
	xor	r13d,ebx
	add	r8d,DWORD PTR[44+rsp]
	mov	esi,r9d
	ror	r14d,11
	xor	r12d,edx
	xor	esi,r10d
	ror	r13d,6
	add	r8d,r12d
	and	r15d,esi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	add	eax,r8d
	ror	r14d,2
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	ror	r13d,14
	mov	r8d,r14d
	mov	r12d,ebx
	xor	r13d,eax
	ror	r14d,9
	xor	r12d,ecx
	ror	r13d,5
	xor	r14d,r8d
	and	r12d,eax
	vpand	xmm8,xmm11,xmm12
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((192-128))+rdi]
	xor	r13d,eax
	add	edx,DWORD PTR[48+rsp]
	mov	r15d,r8d
	ror	r14d,11
	xor	r12d,ecx
	xor	r15d,r9d
	ror	r13d,6
	add	edx,r12d
	and	esi,r15d
	xor	r14d,r8d
	add	edx,r13d
	xor	esi,r9d
	add	r11d,edx
	ror	r14d,2
	add	edx,esi
	mov	r13d,r11d
	add	r14d,edx
	ror	r13d,14
	mov	edx,r14d
	mov	r12d,eax
	xor	r13d,r11d
	ror	r14d,9
	xor	r12d,ebx
	ror	r13d,5
	xor	r14d,edx
	and	r12d,r11d
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((208-128))+rdi]
	xor	r13d,r11d
	add	ecx,DWORD PTR[52+rsp]
	mov	esi,edx
	ror	r14d,11
	xor	r12d,ebx
	xor	esi,r8d
	ror	r13d,6
	add	ecx,r12d
	and	r15d,esi
	xor	r14d,edx
	add	ecx,r13d
	xor	r15d,r8d
	add	r10d,ecx
	ror	r14d,2
	add	ecx,r15d
	mov	r13d,r10d
	add	r14d,ecx
	ror	r13d,14
	mov	ecx,r14d
	mov	r12d,r11d
	xor	r13d,r10d
	ror	r14d,9
	xor	r12d,eax
	ror	r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	vpand	xmm11,xmm11,xmm13
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((224-128))+rdi]
	xor	r13d,r10d
	add	ebx,DWORD PTR[56+rsp]
	mov	r15d,ecx
	ror	r14d,11
	xor	r12d,eax
	xor	r15d,edx
	ror	r13d,6
	add	ebx,r12d
	and	esi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	xor	esi,edx
	add	r9d,ebx
	ror	r14d,2
	add	ebx,esi
	mov	r13d,r9d
	add	r14d,ebx
	ror	r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	xor	r13d,r9d
	ror	r14d,9
	xor	r12d,r11d
	ror	r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	vpor	xmm8,xmm8,xmm11
	vaesenclast	xmm11,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	xor	r13d,r9d
	add	eax,DWORD PTR[60+rsp]
	mov	esi,ebx
	ror	r14d,11
	xor	r12d,r11d
	xor	esi,ecx
	ror	r13d,6
	add	eax,r12d
	and	r15d,esi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	add	r8d,eax
	ror	r14d,2
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	mov	r12,QWORD PTR[((64+0))+rsp]
	mov	r13,QWORD PTR[((64+8))+rsp]
	mov	r15,QWORD PTR[((64+40))+rsp]
	mov	rsi,QWORD PTR[((64+48))+rsp]

	vpand	xmm11,xmm11,xmm14
	mov	eax,r14d
	vpor	xmm8,xmm8,xmm11
	vmovdqu	XMMWORD PTR[r13*1+r12],xmm8
	lea	r12,QWORD PTR[16+r12]

	add	eax,DWORD PTR[r15]
	add	ebx,DWORD PTR[4+r15]
	add	ecx,DWORD PTR[8+r15]
	add	edx,DWORD PTR[12+r15]
	add	r8d,DWORD PTR[16+r15]
	add	r9d,DWORD PTR[20+r15]
	add	r10d,DWORD PTR[24+r15]
	add	r11d,DWORD PTR[28+r15]

	cmp	r12,QWORD PTR[((64+16))+rsp]

	mov	DWORD PTR[r15],eax
	mov	DWORD PTR[4+r15],ebx
	mov	DWORD PTR[8+r15],ecx
	mov	DWORD PTR[12+r15],edx
	mov	DWORD PTR[16+r15],r8d
	mov	DWORD PTR[20+r15],r9d
	mov	DWORD PTR[24+r15],r10d
	mov	DWORD PTR[28+r15],r11d

	jb	$L$loop_xop

	mov	r8,QWORD PTR[((64+32))+rsp]
	mov	rsi,QWORD PTR[((64+56))+rsp]
	vmovdqu	XMMWORD PTR[r8],xmm8
	vzeroall
	movaps	xmm6,XMMWORD PTR[128+rsp]
	movaps	xmm7,XMMWORD PTR[144+rsp]
	movaps	xmm8,XMMWORD PTR[160+rsp]
	movaps	xmm9,XMMWORD PTR[176+rsp]
	movaps	xmm10,XMMWORD PTR[192+rsp]
	movaps	xmm11,XMMWORD PTR[208+rsp]
	movaps	xmm12,XMMWORD PTR[224+rsp]
	movaps	xmm13,XMMWORD PTR[240+rsp]
	movaps	xmm14,XMMWORD PTR[256+rsp]
	movaps	xmm15,XMMWORD PTR[272+rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$epilogue_xop::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_cbc_sha256_enc_xop::
aesni_cbc_sha256_enc_xop	ENDP

ALIGN	64
aesni_cbc_sha256_enc_avx	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_cbc_sha256_enc_avx::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


$L$avx_shortcut::
	mov	r10,QWORD PTR[56+rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	r11,rsp
	sub	rsp,288
	and	rsp,-64

	shl	rdx,6
	sub	rsi,rdi
	sub	r10,rdi
	add	rdx,rdi


	mov	QWORD PTR[((64+8))+rsp],rsi
	mov	QWORD PTR[((64+16))+rsp],rdx

	mov	QWORD PTR[((64+32))+rsp],r8
	mov	QWORD PTR[((64+40))+rsp],r9
	mov	QWORD PTR[((64+48))+rsp],r10
	mov	QWORD PTR[((64+56))+rsp],r11
	movaps	XMMWORD PTR[128+rsp],xmm6
	movaps	XMMWORD PTR[144+rsp],xmm7
	movaps	XMMWORD PTR[160+rsp],xmm8
	movaps	XMMWORD PTR[176+rsp],xmm9
	movaps	XMMWORD PTR[192+rsp],xmm10
	movaps	XMMWORD PTR[208+rsp],xmm11
	movaps	XMMWORD PTR[224+rsp],xmm12
	movaps	XMMWORD PTR[240+rsp],xmm13
	movaps	XMMWORD PTR[256+rsp],xmm14
	movaps	XMMWORD PTR[272+rsp],xmm15
$L$prologue_avx::
	vzeroall

	mov	r12,rdi
	lea	rdi,QWORD PTR[128+rcx]
	lea	r13,QWORD PTR[((K256+544))]
	mov	r14d,DWORD PTR[((240-128))+rdi]
	mov	r15,r9
	mov	rsi,r10
	vmovdqu	xmm8,XMMWORD PTR[r8]
	sub	r14,9

	mov	eax,DWORD PTR[r15]
	mov	ebx,DWORD PTR[4+r15]
	mov	ecx,DWORD PTR[8+r15]
	mov	edx,DWORD PTR[12+r15]
	mov	r8d,DWORD PTR[16+r15]
	mov	r9d,DWORD PTR[20+r15]
	mov	r10d,DWORD PTR[24+r15]
	mov	r11d,DWORD PTR[28+r15]

	vmovdqa	xmm14,XMMWORD PTR[r14*8+r13]
	vmovdqa	xmm13,XMMWORD PTR[16+r14*8+r13]
	vmovdqa	xmm12,XMMWORD PTR[32+r14*8+r13]
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	jmp	$L$loop_avx
ALIGN	16
$L$loop_avx::
	vmovdqa	xmm7,XMMWORD PTR[((K256+512))]
	vmovdqu	xmm0,XMMWORD PTR[r12*1+rsi]
	vmovdqu	xmm1,XMMWORD PTR[16+r12*1+rsi]
	vmovdqu	xmm2,XMMWORD PTR[32+r12*1+rsi]
	vmovdqu	xmm3,XMMWORD PTR[48+r12*1+rsi]
	vpshufb	xmm0,xmm0,xmm7
	lea	rbp,QWORD PTR[K256]
	vpshufb	xmm1,xmm1,xmm7
	vpshufb	xmm2,xmm2,xmm7
	vpaddd	xmm4,xmm0,XMMWORD PTR[rbp]
	vpshufb	xmm3,xmm3,xmm7
	vpaddd	xmm5,xmm1,XMMWORD PTR[32+rbp]
	vpaddd	xmm6,xmm2,XMMWORD PTR[64+rbp]
	vpaddd	xmm7,xmm3,XMMWORD PTR[96+rbp]
	vmovdqa	XMMWORD PTR[rsp],xmm4
	mov	r14d,eax
	vmovdqa	XMMWORD PTR[16+rsp],xmm5
	mov	esi,ebx
	vmovdqa	XMMWORD PTR[32+rsp],xmm6
	xor	esi,ecx
	vmovdqa	XMMWORD PTR[48+rsp],xmm7
	mov	r13d,r8d
	jmp	$L$avx_00_47

ALIGN	16
$L$avx_00_47::
	sub	rbp,-16*2*4
	vmovdqu	xmm9,XMMWORD PTR[r12]
	mov	QWORD PTR[((64+0))+rsp],r12
	vpalignr	xmm4,xmm1,xmm0,4
	shrd	r13d,r13d,14
	mov	eax,r14d
	mov	r12d,r9d
	vpalignr	xmm7,xmm3,xmm2,4
	xor	r13d,r8d
	shrd	r14d,r14d,9
	xor	r12d,r10d
	vpsrld	xmm6,xmm4,7
	shrd	r13d,r13d,5
	xor	r14d,eax
	and	r12d,r8d
	vpaddd	xmm0,xmm0,xmm7
	vpxor	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((16-128))+rdi]
	xor	r13d,r8d
	add	r11d,DWORD PTR[rsp]
	mov	r15d,eax
	vpsrld	xmm7,xmm4,3
	shrd	r14d,r14d,11
	xor	r12d,r10d
	xor	r15d,ebx
	vpslld	xmm5,xmm4,14
	shrd	r13d,r13d,6
	add	r11d,r12d
	and	esi,r15d
	vpxor	xmm4,xmm7,xmm6
	xor	r14d,eax
	add	r11d,r13d
	xor	esi,ebx
	vpshufd	xmm7,xmm3,250
	add	edx,r11d
	shrd	r14d,r14d,2
	add	r11d,esi
	vpsrld	xmm6,xmm6,11
	mov	r13d,edx
	add	r14d,r11d
	shrd	r13d,r13d,14
	vpxor	xmm4,xmm4,xmm5
	mov	r11d,r14d
	mov	r12d,r8d
	xor	r13d,edx
	vpslld	xmm5,xmm5,11
	shrd	r14d,r14d,9
	xor	r12d,r9d
	shrd	r13d,r13d,5
	vpxor	xmm4,xmm4,xmm6
	xor	r14d,r11d
	and	r12d,edx
	vpxor	xmm9,xmm9,xmm8
	xor	r13d,edx
	vpsrld	xmm6,xmm7,10
	add	r10d,DWORD PTR[4+rsp]
	mov	esi,r11d
	shrd	r14d,r14d,11
	vpxor	xmm4,xmm4,xmm5
	xor	r12d,r9d
	xor	esi,eax
	shrd	r13d,r13d,6
	vpsrlq	xmm7,xmm7,17
	add	r10d,r12d
	and	r15d,esi
	xor	r14d,r11d
	vpaddd	xmm0,xmm0,xmm4
	add	r10d,r13d
	xor	r15d,eax
	add	ecx,r10d
	vpxor	xmm6,xmm6,xmm7
	shrd	r14d,r14d,2
	add	r10d,r15d
	mov	r13d,ecx
	vpsrlq	xmm7,xmm7,2
	add	r14d,r10d
	shrd	r13d,r13d,14
	mov	r10d,r14d
	vpxor	xmm6,xmm6,xmm7
	mov	r12d,edx
	xor	r13d,ecx
	shrd	r14d,r14d,9
	vpshufd	xmm6,xmm6,132
	xor	r12d,r8d
	shrd	r13d,r13d,5
	xor	r14d,r10d
	vpsrldq	xmm6,xmm6,8
	and	r12d,ecx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((32-128))+rdi]
	xor	r13d,ecx
	add	r9d,DWORD PTR[8+rsp]
	vpaddd	xmm0,xmm0,xmm6
	mov	r15d,r10d
	shrd	r14d,r14d,11
	xor	r12d,r8d
	vpshufd	xmm7,xmm0,80
	xor	r15d,r11d
	shrd	r13d,r13d,6
	add	r9d,r12d
	vpsrld	xmm6,xmm7,10
	and	esi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	vpsrlq	xmm7,xmm7,17
	xor	esi,r11d
	add	ebx,r9d
	shrd	r14d,r14d,2
	vpxor	xmm6,xmm6,xmm7
	add	r9d,esi
	mov	r13d,ebx
	add	r14d,r9d
	vpsrlq	xmm7,xmm7,2
	shrd	r13d,r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	vpxor	xmm6,xmm6,xmm7
	xor	r13d,ebx
	shrd	r14d,r14d,9
	xor	r12d,edx
	vpshufd	xmm6,xmm6,232
	shrd	r13d,r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	vpslldq	xmm6,xmm6,8
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((48-128))+rdi]
	xor	r13d,ebx
	add	r8d,DWORD PTR[12+rsp]
	mov	esi,r9d
	vpaddd	xmm0,xmm0,xmm6
	shrd	r14d,r14d,11
	xor	r12d,edx
	xor	esi,r10d
	vpaddd	xmm6,xmm0,XMMWORD PTR[rbp]
	shrd	r13d,r13d,6
	add	r8d,r12d
	and	r15d,esi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	add	eax,r8d
	shrd	r14d,r14d,2
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	vmovdqa	XMMWORD PTR[rsp],xmm6
	vpalignr	xmm4,xmm2,xmm1,4
	shrd	r13d,r13d,14
	mov	r8d,r14d
	mov	r12d,ebx
	vpalignr	xmm7,xmm0,xmm3,4
	xor	r13d,eax
	shrd	r14d,r14d,9
	xor	r12d,ecx
	vpsrld	xmm6,xmm4,7
	shrd	r13d,r13d,5
	xor	r14d,r8d
	and	r12d,eax
	vpaddd	xmm1,xmm1,xmm7
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((64-128))+rdi]
	xor	r13d,eax
	add	edx,DWORD PTR[16+rsp]
	mov	r15d,r8d
	vpsrld	xmm7,xmm4,3
	shrd	r14d,r14d,11
	xor	r12d,ecx
	xor	r15d,r9d
	vpslld	xmm5,xmm4,14
	shrd	r13d,r13d,6
	add	edx,r12d
	and	esi,r15d
	vpxor	xmm4,xmm7,xmm6
	xor	r14d,r8d
	add	edx,r13d
	xor	esi,r9d
	vpshufd	xmm7,xmm0,250
	add	r11d,edx
	shrd	r14d,r14d,2
	add	edx,esi
	vpsrld	xmm6,xmm6,11
	mov	r13d,r11d
	add	r14d,edx
	shrd	r13d,r13d,14
	vpxor	xmm4,xmm4,xmm5
	mov	edx,r14d
	mov	r12d,eax
	xor	r13d,r11d
	vpslld	xmm5,xmm5,11
	shrd	r14d,r14d,9
	xor	r12d,ebx
	shrd	r13d,r13d,5
	vpxor	xmm4,xmm4,xmm6
	xor	r14d,edx
	and	r12d,r11d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((80-128))+rdi]
	xor	r13d,r11d
	vpsrld	xmm6,xmm7,10
	add	ecx,DWORD PTR[20+rsp]
	mov	esi,edx
	shrd	r14d,r14d,11
	vpxor	xmm4,xmm4,xmm5
	xor	r12d,ebx
	xor	esi,r8d
	shrd	r13d,r13d,6
	vpsrlq	xmm7,xmm7,17
	add	ecx,r12d
	and	r15d,esi
	xor	r14d,edx
	vpaddd	xmm1,xmm1,xmm4
	add	ecx,r13d
	xor	r15d,r8d
	add	r10d,ecx
	vpxor	xmm6,xmm6,xmm7
	shrd	r14d,r14d,2
	add	ecx,r15d
	mov	r13d,r10d
	vpsrlq	xmm7,xmm7,2
	add	r14d,ecx
	shrd	r13d,r13d,14
	mov	ecx,r14d
	vpxor	xmm6,xmm6,xmm7
	mov	r12d,r11d
	xor	r13d,r10d
	shrd	r14d,r14d,9
	vpshufd	xmm6,xmm6,132
	xor	r12d,eax
	shrd	r13d,r13d,5
	xor	r14d,ecx
	vpsrldq	xmm6,xmm6,8
	and	r12d,r10d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((96-128))+rdi]
	xor	r13d,r10d
	add	ebx,DWORD PTR[24+rsp]
	vpaddd	xmm1,xmm1,xmm6
	mov	r15d,ecx
	shrd	r14d,r14d,11
	xor	r12d,eax
	vpshufd	xmm7,xmm1,80
	xor	r15d,edx
	shrd	r13d,r13d,6
	add	ebx,r12d
	vpsrld	xmm6,xmm7,10
	and	esi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	vpsrlq	xmm7,xmm7,17
	xor	esi,edx
	add	r9d,ebx
	shrd	r14d,r14d,2
	vpxor	xmm6,xmm6,xmm7
	add	ebx,esi
	mov	r13d,r9d
	add	r14d,ebx
	vpsrlq	xmm7,xmm7,2
	shrd	r13d,r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	vpxor	xmm6,xmm6,xmm7
	xor	r13d,r9d
	shrd	r14d,r14d,9
	xor	r12d,r11d
	vpshufd	xmm6,xmm6,232
	shrd	r13d,r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	vpslldq	xmm6,xmm6,8
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((112-128))+rdi]
	xor	r13d,r9d
	add	eax,DWORD PTR[28+rsp]
	mov	esi,ebx
	vpaddd	xmm1,xmm1,xmm6
	shrd	r14d,r14d,11
	xor	r12d,r11d
	xor	esi,ecx
	vpaddd	xmm6,xmm1,XMMWORD PTR[32+rbp]
	shrd	r13d,r13d,6
	add	eax,r12d
	and	r15d,esi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	add	r8d,eax
	shrd	r14d,r14d,2
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	vmovdqa	XMMWORD PTR[16+rsp],xmm6
	vpalignr	xmm4,xmm3,xmm2,4
	shrd	r13d,r13d,14
	mov	eax,r14d
	mov	r12d,r9d
	vpalignr	xmm7,xmm1,xmm0,4
	xor	r13d,r8d
	shrd	r14d,r14d,9
	xor	r12d,r10d
	vpsrld	xmm6,xmm4,7
	shrd	r13d,r13d,5
	xor	r14d,eax
	and	r12d,r8d
	vpaddd	xmm2,xmm2,xmm7
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((128-128))+rdi]
	xor	r13d,r8d
	add	r11d,DWORD PTR[32+rsp]
	mov	r15d,eax
	vpsrld	xmm7,xmm4,3
	shrd	r14d,r14d,11
	xor	r12d,r10d
	xor	r15d,ebx
	vpslld	xmm5,xmm4,14
	shrd	r13d,r13d,6
	add	r11d,r12d
	and	esi,r15d
	vpxor	xmm4,xmm7,xmm6
	xor	r14d,eax
	add	r11d,r13d
	xor	esi,ebx
	vpshufd	xmm7,xmm1,250
	add	edx,r11d
	shrd	r14d,r14d,2
	add	r11d,esi
	vpsrld	xmm6,xmm6,11
	mov	r13d,edx
	add	r14d,r11d
	shrd	r13d,r13d,14
	vpxor	xmm4,xmm4,xmm5
	mov	r11d,r14d
	mov	r12d,r8d
	xor	r13d,edx
	vpslld	xmm5,xmm5,11
	shrd	r14d,r14d,9
	xor	r12d,r9d
	shrd	r13d,r13d,5
	vpxor	xmm4,xmm4,xmm6
	xor	r14d,r11d
	and	r12d,edx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((144-128))+rdi]
	xor	r13d,edx
	vpsrld	xmm6,xmm7,10
	add	r10d,DWORD PTR[36+rsp]
	mov	esi,r11d
	shrd	r14d,r14d,11
	vpxor	xmm4,xmm4,xmm5
	xor	r12d,r9d
	xor	esi,eax
	shrd	r13d,r13d,6
	vpsrlq	xmm7,xmm7,17
	add	r10d,r12d
	and	r15d,esi
	xor	r14d,r11d
	vpaddd	xmm2,xmm2,xmm4
	add	r10d,r13d
	xor	r15d,eax
	add	ecx,r10d
	vpxor	xmm6,xmm6,xmm7
	shrd	r14d,r14d,2
	add	r10d,r15d
	mov	r13d,ecx
	vpsrlq	xmm7,xmm7,2
	add	r14d,r10d
	shrd	r13d,r13d,14
	mov	r10d,r14d
	vpxor	xmm6,xmm6,xmm7
	mov	r12d,edx
	xor	r13d,ecx
	shrd	r14d,r14d,9
	vpshufd	xmm6,xmm6,132
	xor	r12d,r8d
	shrd	r13d,r13d,5
	xor	r14d,r10d
	vpsrldq	xmm6,xmm6,8
	and	r12d,ecx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((160-128))+rdi]
	xor	r13d,ecx
	add	r9d,DWORD PTR[40+rsp]
	vpaddd	xmm2,xmm2,xmm6
	mov	r15d,r10d
	shrd	r14d,r14d,11
	xor	r12d,r8d
	vpshufd	xmm7,xmm2,80
	xor	r15d,r11d
	shrd	r13d,r13d,6
	add	r9d,r12d
	vpsrld	xmm6,xmm7,10
	and	esi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	vpsrlq	xmm7,xmm7,17
	xor	esi,r11d
	add	ebx,r9d
	shrd	r14d,r14d,2
	vpxor	xmm6,xmm6,xmm7
	add	r9d,esi
	mov	r13d,ebx
	add	r14d,r9d
	vpsrlq	xmm7,xmm7,2
	shrd	r13d,r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	vpxor	xmm6,xmm6,xmm7
	xor	r13d,ebx
	shrd	r14d,r14d,9
	xor	r12d,edx
	vpshufd	xmm6,xmm6,232
	shrd	r13d,r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	vpslldq	xmm6,xmm6,8
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((176-128))+rdi]
	xor	r13d,ebx
	add	r8d,DWORD PTR[44+rsp]
	mov	esi,r9d
	vpaddd	xmm2,xmm2,xmm6
	shrd	r14d,r14d,11
	xor	r12d,edx
	xor	esi,r10d
	vpaddd	xmm6,xmm2,XMMWORD PTR[64+rbp]
	shrd	r13d,r13d,6
	add	r8d,r12d
	and	r15d,esi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	add	eax,r8d
	shrd	r14d,r14d,2
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	vmovdqa	XMMWORD PTR[32+rsp],xmm6
	vpalignr	xmm4,xmm0,xmm3,4
	shrd	r13d,r13d,14
	mov	r8d,r14d
	mov	r12d,ebx
	vpalignr	xmm7,xmm2,xmm1,4
	xor	r13d,eax
	shrd	r14d,r14d,9
	xor	r12d,ecx
	vpsrld	xmm6,xmm4,7
	shrd	r13d,r13d,5
	xor	r14d,r8d
	and	r12d,eax
	vpaddd	xmm3,xmm3,xmm7
	vpand	xmm8,xmm11,xmm12
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((192-128))+rdi]
	xor	r13d,eax
	add	edx,DWORD PTR[48+rsp]
	mov	r15d,r8d
	vpsrld	xmm7,xmm4,3
	shrd	r14d,r14d,11
	xor	r12d,ecx
	xor	r15d,r9d
	vpslld	xmm5,xmm4,14
	shrd	r13d,r13d,6
	add	edx,r12d
	and	esi,r15d
	vpxor	xmm4,xmm7,xmm6
	xor	r14d,r8d
	add	edx,r13d
	xor	esi,r9d
	vpshufd	xmm7,xmm2,250
	add	r11d,edx
	shrd	r14d,r14d,2
	add	edx,esi
	vpsrld	xmm6,xmm6,11
	mov	r13d,r11d
	add	r14d,edx
	shrd	r13d,r13d,14
	vpxor	xmm4,xmm4,xmm5
	mov	edx,r14d
	mov	r12d,eax
	xor	r13d,r11d
	vpslld	xmm5,xmm5,11
	shrd	r14d,r14d,9
	xor	r12d,ebx
	shrd	r13d,r13d,5
	vpxor	xmm4,xmm4,xmm6
	xor	r14d,edx
	and	r12d,r11d
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((208-128))+rdi]
	xor	r13d,r11d
	vpsrld	xmm6,xmm7,10
	add	ecx,DWORD PTR[52+rsp]
	mov	esi,edx
	shrd	r14d,r14d,11
	vpxor	xmm4,xmm4,xmm5
	xor	r12d,ebx
	xor	esi,r8d
	shrd	r13d,r13d,6
	vpsrlq	xmm7,xmm7,17
	add	ecx,r12d
	and	r15d,esi
	xor	r14d,edx
	vpaddd	xmm3,xmm3,xmm4
	add	ecx,r13d
	xor	r15d,r8d
	add	r10d,ecx
	vpxor	xmm6,xmm6,xmm7
	shrd	r14d,r14d,2
	add	ecx,r15d
	mov	r13d,r10d
	vpsrlq	xmm7,xmm7,2
	add	r14d,ecx
	shrd	r13d,r13d,14
	mov	ecx,r14d
	vpxor	xmm6,xmm6,xmm7
	mov	r12d,r11d
	xor	r13d,r10d
	shrd	r14d,r14d,9
	vpshufd	xmm6,xmm6,132
	xor	r12d,eax
	shrd	r13d,r13d,5
	xor	r14d,ecx
	vpsrldq	xmm6,xmm6,8
	and	r12d,r10d
	vpand	xmm11,xmm11,xmm13
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((224-128))+rdi]
	xor	r13d,r10d
	add	ebx,DWORD PTR[56+rsp]
	vpaddd	xmm3,xmm3,xmm6
	mov	r15d,ecx
	shrd	r14d,r14d,11
	xor	r12d,eax
	vpshufd	xmm7,xmm3,80
	xor	r15d,edx
	shrd	r13d,r13d,6
	add	ebx,r12d
	vpsrld	xmm6,xmm7,10
	and	esi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	vpsrlq	xmm7,xmm7,17
	xor	esi,edx
	add	r9d,ebx
	shrd	r14d,r14d,2
	vpxor	xmm6,xmm6,xmm7
	add	ebx,esi
	mov	r13d,r9d
	add	r14d,ebx
	vpsrlq	xmm7,xmm7,2
	shrd	r13d,r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	vpxor	xmm6,xmm6,xmm7
	xor	r13d,r9d
	shrd	r14d,r14d,9
	xor	r12d,r11d
	vpshufd	xmm6,xmm6,232
	shrd	r13d,r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	vpslldq	xmm6,xmm6,8
	vpor	xmm8,xmm8,xmm11
	vaesenclast	xmm11,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	xor	r13d,r9d
	add	eax,DWORD PTR[60+rsp]
	mov	esi,ebx
	vpaddd	xmm3,xmm3,xmm6
	shrd	r14d,r14d,11
	xor	r12d,r11d
	xor	esi,ecx
	vpaddd	xmm6,xmm3,XMMWORD PTR[96+rbp]
	shrd	r13d,r13d,6
	add	eax,r12d
	and	r15d,esi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	add	r8d,eax
	shrd	r14d,r14d,2
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	vmovdqa	XMMWORD PTR[48+rsp],xmm6
	mov	r12,QWORD PTR[((64+0))+rsp]
	vpand	xmm11,xmm11,xmm14
	mov	r15,QWORD PTR[((64+8))+rsp]
	vpor	xmm8,xmm8,xmm11
	vmovdqu	XMMWORD PTR[r12*1+r15],xmm8
	lea	r12,QWORD PTR[16+r12]
	cmp	BYTE PTR[131+rbp],0
	jne	$L$avx_00_47
	vmovdqu	xmm9,XMMWORD PTR[r12]
	mov	QWORD PTR[((64+0))+rsp],r12
	shrd	r13d,r13d,14
	mov	eax,r14d
	mov	r12d,r9d
	xor	r13d,r8d
	shrd	r14d,r14d,9
	xor	r12d,r10d
	shrd	r13d,r13d,5
	xor	r14d,eax
	and	r12d,r8d
	vpxor	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((16-128))+rdi]
	xor	r13d,r8d
	add	r11d,DWORD PTR[rsp]
	mov	r15d,eax
	shrd	r14d,r14d,11
	xor	r12d,r10d
	xor	r15d,ebx
	shrd	r13d,r13d,6
	add	r11d,r12d
	and	esi,r15d
	xor	r14d,eax
	add	r11d,r13d
	xor	esi,ebx
	add	edx,r11d
	shrd	r14d,r14d,2
	add	r11d,esi
	mov	r13d,edx
	add	r14d,r11d
	shrd	r13d,r13d,14
	mov	r11d,r14d
	mov	r12d,r8d
	xor	r13d,edx
	shrd	r14d,r14d,9
	xor	r12d,r9d
	shrd	r13d,r13d,5
	xor	r14d,r11d
	and	r12d,edx
	vpxor	xmm9,xmm9,xmm8
	xor	r13d,edx
	add	r10d,DWORD PTR[4+rsp]
	mov	esi,r11d
	shrd	r14d,r14d,11
	xor	r12d,r9d
	xor	esi,eax
	shrd	r13d,r13d,6
	add	r10d,r12d
	and	r15d,esi
	xor	r14d,r11d
	add	r10d,r13d
	xor	r15d,eax
	add	ecx,r10d
	shrd	r14d,r14d,2
	add	r10d,r15d
	mov	r13d,ecx
	add	r14d,r10d
	shrd	r13d,r13d,14
	mov	r10d,r14d
	mov	r12d,edx
	xor	r13d,ecx
	shrd	r14d,r14d,9
	xor	r12d,r8d
	shrd	r13d,r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((32-128))+rdi]
	xor	r13d,ecx
	add	r9d,DWORD PTR[8+rsp]
	mov	r15d,r10d
	shrd	r14d,r14d,11
	xor	r12d,r8d
	xor	r15d,r11d
	shrd	r13d,r13d,6
	add	r9d,r12d
	and	esi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	xor	esi,r11d
	add	ebx,r9d
	shrd	r14d,r14d,2
	add	r9d,esi
	mov	r13d,ebx
	add	r14d,r9d
	shrd	r13d,r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	xor	r13d,ebx
	shrd	r14d,r14d,9
	xor	r12d,edx
	shrd	r13d,r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((48-128))+rdi]
	xor	r13d,ebx
	add	r8d,DWORD PTR[12+rsp]
	mov	esi,r9d
	shrd	r14d,r14d,11
	xor	r12d,edx
	xor	esi,r10d
	shrd	r13d,r13d,6
	add	r8d,r12d
	and	r15d,esi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	add	eax,r8d
	shrd	r14d,r14d,2
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	shrd	r13d,r13d,14
	mov	r8d,r14d
	mov	r12d,ebx
	xor	r13d,eax
	shrd	r14d,r14d,9
	xor	r12d,ecx
	shrd	r13d,r13d,5
	xor	r14d,r8d
	and	r12d,eax
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((64-128))+rdi]
	xor	r13d,eax
	add	edx,DWORD PTR[16+rsp]
	mov	r15d,r8d
	shrd	r14d,r14d,11
	xor	r12d,ecx
	xor	r15d,r9d
	shrd	r13d,r13d,6
	add	edx,r12d
	and	esi,r15d
	xor	r14d,r8d
	add	edx,r13d
	xor	esi,r9d
	add	r11d,edx
	shrd	r14d,r14d,2
	add	edx,esi
	mov	r13d,r11d
	add	r14d,edx
	shrd	r13d,r13d,14
	mov	edx,r14d
	mov	r12d,eax
	xor	r13d,r11d
	shrd	r14d,r14d,9
	xor	r12d,ebx
	shrd	r13d,r13d,5
	xor	r14d,edx
	and	r12d,r11d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((80-128))+rdi]
	xor	r13d,r11d
	add	ecx,DWORD PTR[20+rsp]
	mov	esi,edx
	shrd	r14d,r14d,11
	xor	r12d,ebx
	xor	esi,r8d
	shrd	r13d,r13d,6
	add	ecx,r12d
	and	r15d,esi
	xor	r14d,edx
	add	ecx,r13d
	xor	r15d,r8d
	add	r10d,ecx
	shrd	r14d,r14d,2
	add	ecx,r15d
	mov	r13d,r10d
	add	r14d,ecx
	shrd	r13d,r13d,14
	mov	ecx,r14d
	mov	r12d,r11d
	xor	r13d,r10d
	shrd	r14d,r14d,9
	xor	r12d,eax
	shrd	r13d,r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((96-128))+rdi]
	xor	r13d,r10d
	add	ebx,DWORD PTR[24+rsp]
	mov	r15d,ecx
	shrd	r14d,r14d,11
	xor	r12d,eax
	xor	r15d,edx
	shrd	r13d,r13d,6
	add	ebx,r12d
	and	esi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	xor	esi,edx
	add	r9d,ebx
	shrd	r14d,r14d,2
	add	ebx,esi
	mov	r13d,r9d
	add	r14d,ebx
	shrd	r13d,r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	xor	r13d,r9d
	shrd	r14d,r14d,9
	xor	r12d,r11d
	shrd	r13d,r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((112-128))+rdi]
	xor	r13d,r9d
	add	eax,DWORD PTR[28+rsp]
	mov	esi,ebx
	shrd	r14d,r14d,11
	xor	r12d,r11d
	xor	esi,ecx
	shrd	r13d,r13d,6
	add	eax,r12d
	and	r15d,esi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	add	r8d,eax
	shrd	r14d,r14d,2
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	shrd	r13d,r13d,14
	mov	eax,r14d
	mov	r12d,r9d
	xor	r13d,r8d
	shrd	r14d,r14d,9
	xor	r12d,r10d
	shrd	r13d,r13d,5
	xor	r14d,eax
	and	r12d,r8d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((128-128))+rdi]
	xor	r13d,r8d
	add	r11d,DWORD PTR[32+rsp]
	mov	r15d,eax
	shrd	r14d,r14d,11
	xor	r12d,r10d
	xor	r15d,ebx
	shrd	r13d,r13d,6
	add	r11d,r12d
	and	esi,r15d
	xor	r14d,eax
	add	r11d,r13d
	xor	esi,ebx
	add	edx,r11d
	shrd	r14d,r14d,2
	add	r11d,esi
	mov	r13d,edx
	add	r14d,r11d
	shrd	r13d,r13d,14
	mov	r11d,r14d
	mov	r12d,r8d
	xor	r13d,edx
	shrd	r14d,r14d,9
	xor	r12d,r9d
	shrd	r13d,r13d,5
	xor	r14d,r11d
	and	r12d,edx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((144-128))+rdi]
	xor	r13d,edx
	add	r10d,DWORD PTR[36+rsp]
	mov	esi,r11d
	shrd	r14d,r14d,11
	xor	r12d,r9d
	xor	esi,eax
	shrd	r13d,r13d,6
	add	r10d,r12d
	and	r15d,esi
	xor	r14d,r11d
	add	r10d,r13d
	xor	r15d,eax
	add	ecx,r10d
	shrd	r14d,r14d,2
	add	r10d,r15d
	mov	r13d,ecx
	add	r14d,r10d
	shrd	r13d,r13d,14
	mov	r10d,r14d
	mov	r12d,edx
	xor	r13d,ecx
	shrd	r14d,r14d,9
	xor	r12d,r8d
	shrd	r13d,r13d,5
	xor	r14d,r10d
	and	r12d,ecx
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((160-128))+rdi]
	xor	r13d,ecx
	add	r9d,DWORD PTR[40+rsp]
	mov	r15d,r10d
	shrd	r14d,r14d,11
	xor	r12d,r8d
	xor	r15d,r11d
	shrd	r13d,r13d,6
	add	r9d,r12d
	and	esi,r15d
	xor	r14d,r10d
	add	r9d,r13d
	xor	esi,r11d
	add	ebx,r9d
	shrd	r14d,r14d,2
	add	r9d,esi
	mov	r13d,ebx
	add	r14d,r9d
	shrd	r13d,r13d,14
	mov	r9d,r14d
	mov	r12d,ecx
	xor	r13d,ebx
	shrd	r14d,r14d,9
	xor	r12d,edx
	shrd	r13d,r13d,5
	xor	r14d,r9d
	and	r12d,ebx
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((176-128))+rdi]
	xor	r13d,ebx
	add	r8d,DWORD PTR[44+rsp]
	mov	esi,r9d
	shrd	r14d,r14d,11
	xor	r12d,edx
	xor	esi,r10d
	shrd	r13d,r13d,6
	add	r8d,r12d
	and	r15d,esi
	xor	r14d,r9d
	add	r8d,r13d
	xor	r15d,r10d
	add	eax,r8d
	shrd	r14d,r14d,2
	add	r8d,r15d
	mov	r13d,eax
	add	r14d,r8d
	shrd	r13d,r13d,14
	mov	r8d,r14d
	mov	r12d,ebx
	xor	r13d,eax
	shrd	r14d,r14d,9
	xor	r12d,ecx
	shrd	r13d,r13d,5
	xor	r14d,r8d
	and	r12d,eax
	vpand	xmm8,xmm11,xmm12
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((192-128))+rdi]
	xor	r13d,eax
	add	edx,DWORD PTR[48+rsp]
	mov	r15d,r8d
	shrd	r14d,r14d,11
	xor	r12d,ecx
	xor	r15d,r9d
	shrd	r13d,r13d,6
	add	edx,r12d
	and	esi,r15d
	xor	r14d,r8d
	add	edx,r13d
	xor	esi,r9d
	add	r11d,edx
	shrd	r14d,r14d,2
	add	edx,esi
	mov	r13d,r11d
	add	r14d,edx
	shrd	r13d,r13d,14
	mov	edx,r14d
	mov	r12d,eax
	xor	r13d,r11d
	shrd	r14d,r14d,9
	xor	r12d,ebx
	shrd	r13d,r13d,5
	xor	r14d,edx
	and	r12d,r11d
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((208-128))+rdi]
	xor	r13d,r11d
	add	ecx,DWORD PTR[52+rsp]
	mov	esi,edx
	shrd	r14d,r14d,11
	xor	r12d,ebx
	xor	esi,r8d
	shrd	r13d,r13d,6
	add	ecx,r12d
	and	r15d,esi
	xor	r14d,edx
	add	ecx,r13d
	xor	r15d,r8d
	add	r10d,ecx
	shrd	r14d,r14d,2
	add	ecx,r15d
	mov	r13d,r10d
	add	r14d,ecx
	shrd	r13d,r13d,14
	mov	ecx,r14d
	mov	r12d,r11d
	xor	r13d,r10d
	shrd	r14d,r14d,9
	xor	r12d,eax
	shrd	r13d,r13d,5
	xor	r14d,ecx
	and	r12d,r10d
	vpand	xmm11,xmm11,xmm13
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((224-128))+rdi]
	xor	r13d,r10d
	add	ebx,DWORD PTR[56+rsp]
	mov	r15d,ecx
	shrd	r14d,r14d,11
	xor	r12d,eax
	xor	r15d,edx
	shrd	r13d,r13d,6
	add	ebx,r12d
	and	esi,r15d
	xor	r14d,ecx
	add	ebx,r13d
	xor	esi,edx
	add	r9d,ebx
	shrd	r14d,r14d,2
	add	ebx,esi
	mov	r13d,r9d
	add	r14d,ebx
	shrd	r13d,r13d,14
	mov	ebx,r14d
	mov	r12d,r10d
	xor	r13d,r9d
	shrd	r14d,r14d,9
	xor	r12d,r11d
	shrd	r13d,r13d,5
	xor	r14d,ebx
	and	r12d,r9d
	vpor	xmm8,xmm8,xmm11
	vaesenclast	xmm11,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	xor	r13d,r9d
	add	eax,DWORD PTR[60+rsp]
	mov	esi,ebx
	shrd	r14d,r14d,11
	xor	r12d,r11d
	xor	esi,ecx
	shrd	r13d,r13d,6
	add	eax,r12d
	and	r15d,esi
	xor	r14d,ebx
	add	eax,r13d
	xor	r15d,ecx
	add	r8d,eax
	shrd	r14d,r14d,2
	add	eax,r15d
	mov	r13d,r8d
	add	r14d,eax
	mov	r12,QWORD PTR[((64+0))+rsp]
	mov	r13,QWORD PTR[((64+8))+rsp]
	mov	r15,QWORD PTR[((64+40))+rsp]
	mov	rsi,QWORD PTR[((64+48))+rsp]

	vpand	xmm11,xmm11,xmm14
	mov	eax,r14d
	vpor	xmm8,xmm8,xmm11
	vmovdqu	XMMWORD PTR[r13*1+r12],xmm8
	lea	r12,QWORD PTR[16+r12]

	add	eax,DWORD PTR[r15]
	add	ebx,DWORD PTR[4+r15]
	add	ecx,DWORD PTR[8+r15]
	add	edx,DWORD PTR[12+r15]
	add	r8d,DWORD PTR[16+r15]
	add	r9d,DWORD PTR[20+r15]
	add	r10d,DWORD PTR[24+r15]
	add	r11d,DWORD PTR[28+r15]

	cmp	r12,QWORD PTR[((64+16))+rsp]

	mov	DWORD PTR[r15],eax
	mov	DWORD PTR[4+r15],ebx
	mov	DWORD PTR[8+r15],ecx
	mov	DWORD PTR[12+r15],edx
	mov	DWORD PTR[16+r15],r8d
	mov	DWORD PTR[20+r15],r9d
	mov	DWORD PTR[24+r15],r10d
	mov	DWORD PTR[28+r15],r11d
	jb	$L$loop_avx

	mov	r8,QWORD PTR[((64+32))+rsp]
	mov	rsi,QWORD PTR[((64+56))+rsp]
	vmovdqu	XMMWORD PTR[r8],xmm8
	vzeroall
	movaps	xmm6,XMMWORD PTR[128+rsp]
	movaps	xmm7,XMMWORD PTR[144+rsp]
	movaps	xmm8,XMMWORD PTR[160+rsp]
	movaps	xmm9,XMMWORD PTR[176+rsp]
	movaps	xmm10,XMMWORD PTR[192+rsp]
	movaps	xmm11,XMMWORD PTR[208+rsp]
	movaps	xmm12,XMMWORD PTR[224+rsp]
	movaps	xmm13,XMMWORD PTR[240+rsp]
	movaps	xmm14,XMMWORD PTR[256+rsp]
	movaps	xmm15,XMMWORD PTR[272+rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$epilogue_avx::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_cbc_sha256_enc_avx::
aesni_cbc_sha256_enc_avx	ENDP

ALIGN	64
aesni_cbc_sha256_enc_avx2	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_cbc_sha256_enc_avx2::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


$L$avx2_shortcut::
	mov	r10,QWORD PTR[56+rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	r11,rsp
	sub	rsp,736
	and	rsp,-256*4
	add	rsp,448

	shl	rdx,6
	sub	rsi,rdi
	sub	r10,rdi
	add	rdx,rdi



	mov	QWORD PTR[((64+16))+rsp],rdx

	mov	QWORD PTR[((64+32))+rsp],r8
	mov	QWORD PTR[((64+40))+rsp],r9
	mov	QWORD PTR[((64+48))+rsp],r10
	mov	QWORD PTR[((64+56))+rsp],r11
	movaps	XMMWORD PTR[128+rsp],xmm6
	movaps	XMMWORD PTR[144+rsp],xmm7
	movaps	XMMWORD PTR[160+rsp],xmm8
	movaps	XMMWORD PTR[176+rsp],xmm9
	movaps	XMMWORD PTR[192+rsp],xmm10
	movaps	XMMWORD PTR[208+rsp],xmm11
	movaps	XMMWORD PTR[224+rsp],xmm12
	movaps	XMMWORD PTR[240+rsp],xmm13
	movaps	XMMWORD PTR[256+rsp],xmm14
	movaps	XMMWORD PTR[272+rsp],xmm15
$L$prologue_avx2::
	vzeroall

	mov	r13,rdi
	vpinsrq	xmm15,xmm15,rsi,1
	lea	rdi,QWORD PTR[128+rcx]
	lea	r12,QWORD PTR[((K256+544))]
	mov	r14d,DWORD PTR[((240-128))+rdi]
	mov	r15,r9
	mov	rsi,r10
	vmovdqu	xmm8,XMMWORD PTR[r8]
	lea	r14,QWORD PTR[((-9))+r14]

	vmovdqa	xmm14,XMMWORD PTR[r14*8+r12]
	vmovdqa	xmm13,XMMWORD PTR[16+r14*8+r12]
	vmovdqa	xmm12,XMMWORD PTR[32+r14*8+r12]

	sub	r13,-16*4
	mov	eax,DWORD PTR[r15]
	lea	r12,QWORD PTR[r13*1+rsi]
	mov	ebx,DWORD PTR[4+r15]
	cmp	r13,rdx
	mov	ecx,DWORD PTR[8+r15]
	cmove	r12,rsp
	mov	edx,DWORD PTR[12+r15]
	mov	r8d,DWORD PTR[16+r15]
	mov	r9d,DWORD PTR[20+r15]
	mov	r10d,DWORD PTR[24+r15]
	mov	r11d,DWORD PTR[28+r15]
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	jmp	$L$oop_avx2
ALIGN	16
$L$oop_avx2::
	vmovdqa	ymm7,YMMWORD PTR[((K256+512))]
	vmovdqu	xmm0,XMMWORD PTR[((-64+0))+r13*1+rsi]
	vmovdqu	xmm1,XMMWORD PTR[((-64+16))+r13*1+rsi]
	vmovdqu	xmm2,XMMWORD PTR[((-64+32))+r13*1+rsi]
	vmovdqu	xmm3,XMMWORD PTR[((-64+48))+r13*1+rsi]

	vinserti128	ymm0,ymm0,XMMWORD PTR[r12],1
	vinserti128	ymm1,ymm1,XMMWORD PTR[16+r12],1
	vpshufb	ymm0,ymm0,ymm7
	vinserti128	ymm2,ymm2,XMMWORD PTR[32+r12],1
	vpshufb	ymm1,ymm1,ymm7
	vinserti128	ymm3,ymm3,XMMWORD PTR[48+r12],1

	lea	rbp,QWORD PTR[K256]
	vpshufb	ymm2,ymm2,ymm7
	lea	r13,QWORD PTR[((-64))+r13]
	vpaddd	ymm4,ymm0,YMMWORD PTR[rbp]
	vpshufb	ymm3,ymm3,ymm7
	vpaddd	ymm5,ymm1,YMMWORD PTR[32+rbp]
	vpaddd	ymm6,ymm2,YMMWORD PTR[64+rbp]
	vpaddd	ymm7,ymm3,YMMWORD PTR[96+rbp]
	vmovdqa	YMMWORD PTR[rsp],ymm4
	xor	r14d,r14d
	vmovdqa	YMMWORD PTR[32+rsp],ymm5
	lea	rsp,QWORD PTR[((-64))+rsp]
	mov	esi,ebx
	vmovdqa	YMMWORD PTR[rsp],ymm6
	xor	esi,ecx
	vmovdqa	YMMWORD PTR[32+rsp],ymm7
	mov	r12d,r9d
	sub	rbp,-16*2*4
	jmp	$L$avx2_00_47

ALIGN	16
$L$avx2_00_47::
	vmovdqu	xmm9,XMMWORD PTR[r13]
	vpinsrq	xmm15,xmm15,r13,0
	lea	rsp,QWORD PTR[((-64))+rsp]
	vpalignr	ymm4,ymm1,ymm0,4
	add	r11d,DWORD PTR[((0+128))+rsp]
	and	r12d,r8d
	rorx	r13d,r8d,25
	vpalignr	ymm7,ymm3,ymm2,4
	rorx	r15d,r8d,11
	lea	eax,DWORD PTR[r14*1+rax]
	lea	r11d,DWORD PTR[r12*1+r11]
	vpsrld	ymm6,ymm4,7
	andn	r12d,r8d,r10d
	xor	r13d,r15d
	rorx	r14d,r8d,6
	vpaddd	ymm0,ymm0,ymm7
	lea	r11d,DWORD PTR[r12*1+r11]
	xor	r13d,r14d
	mov	r15d,eax
	vpsrld	ymm7,ymm4,3
	rorx	r12d,eax,22
	lea	r11d,DWORD PTR[r13*1+r11]
	xor	r15d,ebx
	vpslld	ymm5,ymm4,14
	rorx	r14d,eax,13
	rorx	r13d,eax,2
	lea	edx,DWORD PTR[r11*1+rdx]
	vpxor	ymm4,ymm7,ymm6
	and	esi,r15d
	vpxor	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((16-128))+rdi]
	xor	r14d,r12d
	xor	esi,ebx
	vpshufd	ymm7,ymm3,250
	xor	r14d,r13d
	lea	r11d,DWORD PTR[rsi*1+r11]
	mov	r12d,r8d
	vpsrld	ymm6,ymm6,11
	add	r10d,DWORD PTR[((4+128))+rsp]
	and	r12d,edx
	rorx	r13d,edx,25
	vpxor	ymm4,ymm4,ymm5
	rorx	esi,edx,11
	lea	r11d,DWORD PTR[r14*1+r11]
	lea	r10d,DWORD PTR[r12*1+r10]
	vpslld	ymm5,ymm5,11
	andn	r12d,edx,r9d
	xor	r13d,esi
	rorx	r14d,edx,6
	vpxor	ymm4,ymm4,ymm6
	lea	r10d,DWORD PTR[r12*1+r10]
	xor	r13d,r14d
	mov	esi,r11d
	vpsrld	ymm6,ymm7,10
	rorx	r12d,r11d,22
	lea	r10d,DWORD PTR[r13*1+r10]
	xor	esi,eax
	vpxor	ymm4,ymm4,ymm5
	rorx	r14d,r11d,13
	rorx	r13d,r11d,2
	lea	ecx,DWORD PTR[r10*1+rcx]
	vpsrlq	ymm7,ymm7,17
	and	r15d,esi
	vpxor	xmm9,xmm9,xmm8
	xor	r14d,r12d
	xor	r15d,eax
	vpaddd	ymm0,ymm0,ymm4
	xor	r14d,r13d
	lea	r10d,DWORD PTR[r15*1+r10]
	mov	r12d,edx
	vpxor	ymm6,ymm6,ymm7
	add	r9d,DWORD PTR[((8+128))+rsp]
	and	r12d,ecx
	rorx	r13d,ecx,25
	vpsrlq	ymm7,ymm7,2
	rorx	r15d,ecx,11
	lea	r10d,DWORD PTR[r14*1+r10]
	lea	r9d,DWORD PTR[r12*1+r9]
	vpxor	ymm6,ymm6,ymm7
	andn	r12d,ecx,r8d
	xor	r13d,r15d
	rorx	r14d,ecx,6
	vpshufd	ymm6,ymm6,132
	lea	r9d,DWORD PTR[r12*1+r9]
	xor	r13d,r14d
	mov	r15d,r10d
	vpsrldq	ymm6,ymm6,8
	rorx	r12d,r10d,22
	lea	r9d,DWORD PTR[r13*1+r9]
	xor	r15d,r11d
	vpaddd	ymm0,ymm0,ymm6
	rorx	r14d,r10d,13
	rorx	r13d,r10d,2
	lea	ebx,DWORD PTR[r9*1+rbx]
	vpshufd	ymm7,ymm0,80
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((32-128))+rdi]
	xor	r14d,r12d
	xor	esi,r11d
	vpsrld	ymm6,ymm7,10
	xor	r14d,r13d
	lea	r9d,DWORD PTR[rsi*1+r9]
	mov	r12d,ecx
	vpsrlq	ymm7,ymm7,17
	add	r8d,DWORD PTR[((12+128))+rsp]
	and	r12d,ebx
	rorx	r13d,ebx,25
	vpxor	ymm6,ymm6,ymm7
	rorx	esi,ebx,11
	lea	r9d,DWORD PTR[r14*1+r9]
	lea	r8d,DWORD PTR[r12*1+r8]
	vpsrlq	ymm7,ymm7,2
	andn	r12d,ebx,edx
	xor	r13d,esi
	rorx	r14d,ebx,6
	vpxor	ymm6,ymm6,ymm7
	lea	r8d,DWORD PTR[r12*1+r8]
	xor	r13d,r14d
	mov	esi,r9d
	vpshufd	ymm6,ymm6,232
	rorx	r12d,r9d,22
	lea	r8d,DWORD PTR[r13*1+r8]
	xor	esi,r10d
	vpslldq	ymm6,ymm6,8
	rorx	r14d,r9d,13
	rorx	r13d,r9d,2
	lea	eax,DWORD PTR[r8*1+rax]
	vpaddd	ymm0,ymm0,ymm6
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((48-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r10d
	vpaddd	ymm6,ymm0,YMMWORD PTR[rbp]
	xor	r14d,r13d
	lea	r8d,DWORD PTR[r15*1+r8]
	mov	r12d,ebx
	vmovdqa	YMMWORD PTR[rsp],ymm6
	vpalignr	ymm4,ymm2,ymm1,4
	add	edx,DWORD PTR[((32+128))+rsp]
	and	r12d,eax
	rorx	r13d,eax,25
	vpalignr	ymm7,ymm0,ymm3,4
	rorx	r15d,eax,11
	lea	r8d,DWORD PTR[r14*1+r8]
	lea	edx,DWORD PTR[r12*1+rdx]
	vpsrld	ymm6,ymm4,7
	andn	r12d,eax,ecx
	xor	r13d,r15d
	rorx	r14d,eax,6
	vpaddd	ymm1,ymm1,ymm7
	lea	edx,DWORD PTR[r12*1+rdx]
	xor	r13d,r14d
	mov	r15d,r8d
	vpsrld	ymm7,ymm4,3
	rorx	r12d,r8d,22
	lea	edx,DWORD PTR[r13*1+rdx]
	xor	r15d,r9d
	vpslld	ymm5,ymm4,14
	rorx	r14d,r8d,13
	rorx	r13d,r8d,2
	lea	r11d,DWORD PTR[rdx*1+r11]
	vpxor	ymm4,ymm7,ymm6
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((64-128))+rdi]
	xor	r14d,r12d
	xor	esi,r9d
	vpshufd	ymm7,ymm0,250
	xor	r14d,r13d
	lea	edx,DWORD PTR[rsi*1+rdx]
	mov	r12d,eax
	vpsrld	ymm6,ymm6,11
	add	ecx,DWORD PTR[((36+128))+rsp]
	and	r12d,r11d
	rorx	r13d,r11d,25
	vpxor	ymm4,ymm4,ymm5
	rorx	esi,r11d,11
	lea	edx,DWORD PTR[r14*1+rdx]
	lea	ecx,DWORD PTR[r12*1+rcx]
	vpslld	ymm5,ymm5,11
	andn	r12d,r11d,ebx
	xor	r13d,esi
	rorx	r14d,r11d,6
	vpxor	ymm4,ymm4,ymm6
	lea	ecx,DWORD PTR[r12*1+rcx]
	xor	r13d,r14d
	mov	esi,edx
	vpsrld	ymm6,ymm7,10
	rorx	r12d,edx,22
	lea	ecx,DWORD PTR[r13*1+rcx]
	xor	esi,r8d
	vpxor	ymm4,ymm4,ymm5
	rorx	r14d,edx,13
	rorx	r13d,edx,2
	lea	r10d,DWORD PTR[rcx*1+r10]
	vpsrlq	ymm7,ymm7,17
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((80-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r8d
	vpaddd	ymm1,ymm1,ymm4
	xor	r14d,r13d
	lea	ecx,DWORD PTR[r15*1+rcx]
	mov	r12d,r11d
	vpxor	ymm6,ymm6,ymm7
	add	ebx,DWORD PTR[((40+128))+rsp]
	and	r12d,r10d
	rorx	r13d,r10d,25
	vpsrlq	ymm7,ymm7,2
	rorx	r15d,r10d,11
	lea	ecx,DWORD PTR[r14*1+rcx]
	lea	ebx,DWORD PTR[r12*1+rbx]
	vpxor	ymm6,ymm6,ymm7
	andn	r12d,r10d,eax
	xor	r13d,r15d
	rorx	r14d,r10d,6
	vpshufd	ymm6,ymm6,132
	lea	ebx,DWORD PTR[r12*1+rbx]
	xor	r13d,r14d
	mov	r15d,ecx
	vpsrldq	ymm6,ymm6,8
	rorx	r12d,ecx,22
	lea	ebx,DWORD PTR[r13*1+rbx]
	xor	r15d,edx
	vpaddd	ymm1,ymm1,ymm6
	rorx	r14d,ecx,13
	rorx	r13d,ecx,2
	lea	r9d,DWORD PTR[rbx*1+r9]
	vpshufd	ymm7,ymm1,80
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((96-128))+rdi]
	xor	r14d,r12d
	xor	esi,edx
	vpsrld	ymm6,ymm7,10
	xor	r14d,r13d
	lea	ebx,DWORD PTR[rsi*1+rbx]
	mov	r12d,r10d
	vpsrlq	ymm7,ymm7,17
	add	eax,DWORD PTR[((44+128))+rsp]
	and	r12d,r9d
	rorx	r13d,r9d,25
	vpxor	ymm6,ymm6,ymm7
	rorx	esi,r9d,11
	lea	ebx,DWORD PTR[r14*1+rbx]
	lea	eax,DWORD PTR[r12*1+rax]
	vpsrlq	ymm7,ymm7,2
	andn	r12d,r9d,r11d
	xor	r13d,esi
	rorx	r14d,r9d,6
	vpxor	ymm6,ymm6,ymm7
	lea	eax,DWORD PTR[r12*1+rax]
	xor	r13d,r14d
	mov	esi,ebx
	vpshufd	ymm6,ymm6,232
	rorx	r12d,ebx,22
	lea	eax,DWORD PTR[r13*1+rax]
	xor	esi,ecx
	vpslldq	ymm6,ymm6,8
	rorx	r14d,ebx,13
	rorx	r13d,ebx,2
	lea	r8d,DWORD PTR[rax*1+r8]
	vpaddd	ymm1,ymm1,ymm6
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((112-128))+rdi]
	xor	r14d,r12d
	xor	r15d,ecx
	vpaddd	ymm6,ymm1,YMMWORD PTR[32+rbp]
	xor	r14d,r13d
	lea	eax,DWORD PTR[r15*1+rax]
	mov	r12d,r9d
	vmovdqa	YMMWORD PTR[32+rsp],ymm6
	lea	rsp,QWORD PTR[((-64))+rsp]
	vpalignr	ymm4,ymm3,ymm2,4
	add	r11d,DWORD PTR[((0+128))+rsp]
	and	r12d,r8d
	rorx	r13d,r8d,25
	vpalignr	ymm7,ymm1,ymm0,4
	rorx	r15d,r8d,11
	lea	eax,DWORD PTR[r14*1+rax]
	lea	r11d,DWORD PTR[r12*1+r11]
	vpsrld	ymm6,ymm4,7
	andn	r12d,r8d,r10d
	xor	r13d,r15d
	rorx	r14d,r8d,6
	vpaddd	ymm2,ymm2,ymm7
	lea	r11d,DWORD PTR[r12*1+r11]
	xor	r13d,r14d
	mov	r15d,eax
	vpsrld	ymm7,ymm4,3
	rorx	r12d,eax,22
	lea	r11d,DWORD PTR[r13*1+r11]
	xor	r15d,ebx
	vpslld	ymm5,ymm4,14
	rorx	r14d,eax,13
	rorx	r13d,eax,2
	lea	edx,DWORD PTR[r11*1+rdx]
	vpxor	ymm4,ymm7,ymm6
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((128-128))+rdi]
	xor	r14d,r12d
	xor	esi,ebx
	vpshufd	ymm7,ymm1,250
	xor	r14d,r13d
	lea	r11d,DWORD PTR[rsi*1+r11]
	mov	r12d,r8d
	vpsrld	ymm6,ymm6,11
	add	r10d,DWORD PTR[((4+128))+rsp]
	and	r12d,edx
	rorx	r13d,edx,25
	vpxor	ymm4,ymm4,ymm5
	rorx	esi,edx,11
	lea	r11d,DWORD PTR[r14*1+r11]
	lea	r10d,DWORD PTR[r12*1+r10]
	vpslld	ymm5,ymm5,11
	andn	r12d,edx,r9d
	xor	r13d,esi
	rorx	r14d,edx,6
	vpxor	ymm4,ymm4,ymm6
	lea	r10d,DWORD PTR[r12*1+r10]
	xor	r13d,r14d
	mov	esi,r11d
	vpsrld	ymm6,ymm7,10
	rorx	r12d,r11d,22
	lea	r10d,DWORD PTR[r13*1+r10]
	xor	esi,eax
	vpxor	ymm4,ymm4,ymm5
	rorx	r14d,r11d,13
	rorx	r13d,r11d,2
	lea	ecx,DWORD PTR[r10*1+rcx]
	vpsrlq	ymm7,ymm7,17
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((144-128))+rdi]
	xor	r14d,r12d
	xor	r15d,eax
	vpaddd	ymm2,ymm2,ymm4
	xor	r14d,r13d
	lea	r10d,DWORD PTR[r15*1+r10]
	mov	r12d,edx
	vpxor	ymm6,ymm6,ymm7
	add	r9d,DWORD PTR[((8+128))+rsp]
	and	r12d,ecx
	rorx	r13d,ecx,25
	vpsrlq	ymm7,ymm7,2
	rorx	r15d,ecx,11
	lea	r10d,DWORD PTR[r14*1+r10]
	lea	r9d,DWORD PTR[r12*1+r9]
	vpxor	ymm6,ymm6,ymm7
	andn	r12d,ecx,r8d
	xor	r13d,r15d
	rorx	r14d,ecx,6
	vpshufd	ymm6,ymm6,132
	lea	r9d,DWORD PTR[r12*1+r9]
	xor	r13d,r14d
	mov	r15d,r10d
	vpsrldq	ymm6,ymm6,8
	rorx	r12d,r10d,22
	lea	r9d,DWORD PTR[r13*1+r9]
	xor	r15d,r11d
	vpaddd	ymm2,ymm2,ymm6
	rorx	r14d,r10d,13
	rorx	r13d,r10d,2
	lea	ebx,DWORD PTR[r9*1+rbx]
	vpshufd	ymm7,ymm2,80
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((160-128))+rdi]
	xor	r14d,r12d
	xor	esi,r11d
	vpsrld	ymm6,ymm7,10
	xor	r14d,r13d
	lea	r9d,DWORD PTR[rsi*1+r9]
	mov	r12d,ecx
	vpsrlq	ymm7,ymm7,17
	add	r8d,DWORD PTR[((12+128))+rsp]
	and	r12d,ebx
	rorx	r13d,ebx,25
	vpxor	ymm6,ymm6,ymm7
	rorx	esi,ebx,11
	lea	r9d,DWORD PTR[r14*1+r9]
	lea	r8d,DWORD PTR[r12*1+r8]
	vpsrlq	ymm7,ymm7,2
	andn	r12d,ebx,edx
	xor	r13d,esi
	rorx	r14d,ebx,6
	vpxor	ymm6,ymm6,ymm7
	lea	r8d,DWORD PTR[r12*1+r8]
	xor	r13d,r14d
	mov	esi,r9d
	vpshufd	ymm6,ymm6,232
	rorx	r12d,r9d,22
	lea	r8d,DWORD PTR[r13*1+r8]
	xor	esi,r10d
	vpslldq	ymm6,ymm6,8
	rorx	r14d,r9d,13
	rorx	r13d,r9d,2
	lea	eax,DWORD PTR[r8*1+rax]
	vpaddd	ymm2,ymm2,ymm6
	and	r15d,esi
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((176-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r10d
	vpaddd	ymm6,ymm2,YMMWORD PTR[64+rbp]
	xor	r14d,r13d
	lea	r8d,DWORD PTR[r15*1+r8]
	mov	r12d,ebx
	vmovdqa	YMMWORD PTR[rsp],ymm6
	vpalignr	ymm4,ymm0,ymm3,4
	add	edx,DWORD PTR[((32+128))+rsp]
	and	r12d,eax
	rorx	r13d,eax,25
	vpalignr	ymm7,ymm2,ymm1,4
	rorx	r15d,eax,11
	lea	r8d,DWORD PTR[r14*1+r8]
	lea	edx,DWORD PTR[r12*1+rdx]
	vpsrld	ymm6,ymm4,7
	andn	r12d,eax,ecx
	xor	r13d,r15d
	rorx	r14d,eax,6
	vpaddd	ymm3,ymm3,ymm7
	lea	edx,DWORD PTR[r12*1+rdx]
	xor	r13d,r14d
	mov	r15d,r8d
	vpsrld	ymm7,ymm4,3
	rorx	r12d,r8d,22
	lea	edx,DWORD PTR[r13*1+rdx]
	xor	r15d,r9d
	vpslld	ymm5,ymm4,14
	rorx	r14d,r8d,13
	rorx	r13d,r8d,2
	lea	r11d,DWORD PTR[rdx*1+r11]
	vpxor	ymm4,ymm7,ymm6
	and	esi,r15d
	vpand	xmm8,xmm11,xmm12
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((192-128))+rdi]
	xor	r14d,r12d
	xor	esi,r9d
	vpshufd	ymm7,ymm2,250
	xor	r14d,r13d
	lea	edx,DWORD PTR[rsi*1+rdx]
	mov	r12d,eax
	vpsrld	ymm6,ymm6,11
	add	ecx,DWORD PTR[((36+128))+rsp]
	and	r12d,r11d
	rorx	r13d,r11d,25
	vpxor	ymm4,ymm4,ymm5
	rorx	esi,r11d,11
	lea	edx,DWORD PTR[r14*1+rdx]
	lea	ecx,DWORD PTR[r12*1+rcx]
	vpslld	ymm5,ymm5,11
	andn	r12d,r11d,ebx
	xor	r13d,esi
	rorx	r14d,r11d,6
	vpxor	ymm4,ymm4,ymm6
	lea	ecx,DWORD PTR[r12*1+rcx]
	xor	r13d,r14d
	mov	esi,edx
	vpsrld	ymm6,ymm7,10
	rorx	r12d,edx,22
	lea	ecx,DWORD PTR[r13*1+rcx]
	xor	esi,r8d
	vpxor	ymm4,ymm4,ymm5
	rorx	r14d,edx,13
	rorx	r13d,edx,2
	lea	r10d,DWORD PTR[rcx*1+r10]
	vpsrlq	ymm7,ymm7,17
	and	r15d,esi
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((208-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r8d
	vpaddd	ymm3,ymm3,ymm4
	xor	r14d,r13d
	lea	ecx,DWORD PTR[r15*1+rcx]
	mov	r12d,r11d
	vpxor	ymm6,ymm6,ymm7
	add	ebx,DWORD PTR[((40+128))+rsp]
	and	r12d,r10d
	rorx	r13d,r10d,25
	vpsrlq	ymm7,ymm7,2
	rorx	r15d,r10d,11
	lea	ecx,DWORD PTR[r14*1+rcx]
	lea	ebx,DWORD PTR[r12*1+rbx]
	vpxor	ymm6,ymm6,ymm7
	andn	r12d,r10d,eax
	xor	r13d,r15d
	rorx	r14d,r10d,6
	vpshufd	ymm6,ymm6,132
	lea	ebx,DWORD PTR[r12*1+rbx]
	xor	r13d,r14d
	mov	r15d,ecx
	vpsrldq	ymm6,ymm6,8
	rorx	r12d,ecx,22
	lea	ebx,DWORD PTR[r13*1+rbx]
	xor	r15d,edx
	vpaddd	ymm3,ymm3,ymm6
	rorx	r14d,ecx,13
	rorx	r13d,ecx,2
	lea	r9d,DWORD PTR[rbx*1+r9]
	vpshufd	ymm7,ymm3,80
	and	esi,r15d
	vpand	xmm11,xmm11,xmm13
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((224-128))+rdi]
	xor	r14d,r12d
	xor	esi,edx
	vpsrld	ymm6,ymm7,10
	xor	r14d,r13d
	lea	ebx,DWORD PTR[rsi*1+rbx]
	mov	r12d,r10d
	vpsrlq	ymm7,ymm7,17
	add	eax,DWORD PTR[((44+128))+rsp]
	and	r12d,r9d
	rorx	r13d,r9d,25
	vpxor	ymm6,ymm6,ymm7
	rorx	esi,r9d,11
	lea	ebx,DWORD PTR[r14*1+rbx]
	lea	eax,DWORD PTR[r12*1+rax]
	vpsrlq	ymm7,ymm7,2
	andn	r12d,r9d,r11d
	xor	r13d,esi
	rorx	r14d,r9d,6
	vpxor	ymm6,ymm6,ymm7
	lea	eax,DWORD PTR[r12*1+rax]
	xor	r13d,r14d
	mov	esi,ebx
	vpshufd	ymm6,ymm6,232
	rorx	r12d,ebx,22
	lea	eax,DWORD PTR[r13*1+rax]
	xor	esi,ecx
	vpslldq	ymm6,ymm6,8
	rorx	r14d,ebx,13
	rorx	r13d,ebx,2
	lea	r8d,DWORD PTR[rax*1+r8]
	vpaddd	ymm3,ymm3,ymm6
	and	r15d,esi
	vpor	xmm8,xmm8,xmm11
	vaesenclast	xmm11,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	xor	r14d,r12d
	xor	r15d,ecx
	vpaddd	ymm6,ymm3,YMMWORD PTR[96+rbp]
	xor	r14d,r13d
	lea	eax,DWORD PTR[r15*1+rax]
	mov	r12d,r9d
	vmovdqa	YMMWORD PTR[32+rsp],ymm6
	vmovq	r13,xmm15
	vpextrq	r15,xmm15,1
	vpand	xmm11,xmm11,xmm14
	vpor	xmm8,xmm8,xmm11
	vmovdqu	XMMWORD PTR[r13*1+r15],xmm8
	lea	r13,QWORD PTR[16+r13]
	lea	rbp,QWORD PTR[128+rbp]
	cmp	BYTE PTR[3+rbp],0
	jne	$L$avx2_00_47
	vmovdqu	xmm9,XMMWORD PTR[r13]
	vpinsrq	xmm15,xmm15,r13,0
	add	r11d,DWORD PTR[((0+64))+rsp]
	and	r12d,r8d
	rorx	r13d,r8d,25
	rorx	r15d,r8d,11
	lea	eax,DWORD PTR[r14*1+rax]
	lea	r11d,DWORD PTR[r12*1+r11]
	andn	r12d,r8d,r10d
	xor	r13d,r15d
	rorx	r14d,r8d,6
	lea	r11d,DWORD PTR[r12*1+r11]
	xor	r13d,r14d
	mov	r15d,eax
	rorx	r12d,eax,22
	lea	r11d,DWORD PTR[r13*1+r11]
	xor	r15d,ebx
	rorx	r14d,eax,13
	rorx	r13d,eax,2
	lea	edx,DWORD PTR[r11*1+rdx]
	and	esi,r15d
	vpxor	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((16-128))+rdi]
	xor	r14d,r12d
	xor	esi,ebx
	xor	r14d,r13d
	lea	r11d,DWORD PTR[rsi*1+r11]
	mov	r12d,r8d
	add	r10d,DWORD PTR[((4+64))+rsp]
	and	r12d,edx
	rorx	r13d,edx,25
	rorx	esi,edx,11
	lea	r11d,DWORD PTR[r14*1+r11]
	lea	r10d,DWORD PTR[r12*1+r10]
	andn	r12d,edx,r9d
	xor	r13d,esi
	rorx	r14d,edx,6
	lea	r10d,DWORD PTR[r12*1+r10]
	xor	r13d,r14d
	mov	esi,r11d
	rorx	r12d,r11d,22
	lea	r10d,DWORD PTR[r13*1+r10]
	xor	esi,eax
	rorx	r14d,r11d,13
	rorx	r13d,r11d,2
	lea	ecx,DWORD PTR[r10*1+rcx]
	and	r15d,esi
	vpxor	xmm9,xmm9,xmm8
	xor	r14d,r12d
	xor	r15d,eax
	xor	r14d,r13d
	lea	r10d,DWORD PTR[r15*1+r10]
	mov	r12d,edx
	add	r9d,DWORD PTR[((8+64))+rsp]
	and	r12d,ecx
	rorx	r13d,ecx,25
	rorx	r15d,ecx,11
	lea	r10d,DWORD PTR[r14*1+r10]
	lea	r9d,DWORD PTR[r12*1+r9]
	andn	r12d,ecx,r8d
	xor	r13d,r15d
	rorx	r14d,ecx,6
	lea	r9d,DWORD PTR[r12*1+r9]
	xor	r13d,r14d
	mov	r15d,r10d
	rorx	r12d,r10d,22
	lea	r9d,DWORD PTR[r13*1+r9]
	xor	r15d,r11d
	rorx	r14d,r10d,13
	rorx	r13d,r10d,2
	lea	ebx,DWORD PTR[r9*1+rbx]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((32-128))+rdi]
	xor	r14d,r12d
	xor	esi,r11d
	xor	r14d,r13d
	lea	r9d,DWORD PTR[rsi*1+r9]
	mov	r12d,ecx
	add	r8d,DWORD PTR[((12+64))+rsp]
	and	r12d,ebx
	rorx	r13d,ebx,25
	rorx	esi,ebx,11
	lea	r9d,DWORD PTR[r14*1+r9]
	lea	r8d,DWORD PTR[r12*1+r8]
	andn	r12d,ebx,edx
	xor	r13d,esi
	rorx	r14d,ebx,6
	lea	r8d,DWORD PTR[r12*1+r8]
	xor	r13d,r14d
	mov	esi,r9d
	rorx	r12d,r9d,22
	lea	r8d,DWORD PTR[r13*1+r8]
	xor	esi,r10d
	rorx	r14d,r9d,13
	rorx	r13d,r9d,2
	lea	eax,DWORD PTR[r8*1+rax]
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((48-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r10d
	xor	r14d,r13d
	lea	r8d,DWORD PTR[r15*1+r8]
	mov	r12d,ebx
	add	edx,DWORD PTR[((32+64))+rsp]
	and	r12d,eax
	rorx	r13d,eax,25
	rorx	r15d,eax,11
	lea	r8d,DWORD PTR[r14*1+r8]
	lea	edx,DWORD PTR[r12*1+rdx]
	andn	r12d,eax,ecx
	xor	r13d,r15d
	rorx	r14d,eax,6
	lea	edx,DWORD PTR[r12*1+rdx]
	xor	r13d,r14d
	mov	r15d,r8d
	rorx	r12d,r8d,22
	lea	edx,DWORD PTR[r13*1+rdx]
	xor	r15d,r9d
	rorx	r14d,r8d,13
	rorx	r13d,r8d,2
	lea	r11d,DWORD PTR[rdx*1+r11]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((64-128))+rdi]
	xor	r14d,r12d
	xor	esi,r9d
	xor	r14d,r13d
	lea	edx,DWORD PTR[rsi*1+rdx]
	mov	r12d,eax
	add	ecx,DWORD PTR[((36+64))+rsp]
	and	r12d,r11d
	rorx	r13d,r11d,25
	rorx	esi,r11d,11
	lea	edx,DWORD PTR[r14*1+rdx]
	lea	ecx,DWORD PTR[r12*1+rcx]
	andn	r12d,r11d,ebx
	xor	r13d,esi
	rorx	r14d,r11d,6
	lea	ecx,DWORD PTR[r12*1+rcx]
	xor	r13d,r14d
	mov	esi,edx
	rorx	r12d,edx,22
	lea	ecx,DWORD PTR[r13*1+rcx]
	xor	esi,r8d
	rorx	r14d,edx,13
	rorx	r13d,edx,2
	lea	r10d,DWORD PTR[rcx*1+r10]
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((80-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r8d
	xor	r14d,r13d
	lea	ecx,DWORD PTR[r15*1+rcx]
	mov	r12d,r11d
	add	ebx,DWORD PTR[((40+64))+rsp]
	and	r12d,r10d
	rorx	r13d,r10d,25
	rorx	r15d,r10d,11
	lea	ecx,DWORD PTR[r14*1+rcx]
	lea	ebx,DWORD PTR[r12*1+rbx]
	andn	r12d,r10d,eax
	xor	r13d,r15d
	rorx	r14d,r10d,6
	lea	ebx,DWORD PTR[r12*1+rbx]
	xor	r13d,r14d
	mov	r15d,ecx
	rorx	r12d,ecx,22
	lea	ebx,DWORD PTR[r13*1+rbx]
	xor	r15d,edx
	rorx	r14d,ecx,13
	rorx	r13d,ecx,2
	lea	r9d,DWORD PTR[rbx*1+r9]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((96-128))+rdi]
	xor	r14d,r12d
	xor	esi,edx
	xor	r14d,r13d
	lea	ebx,DWORD PTR[rsi*1+rbx]
	mov	r12d,r10d
	add	eax,DWORD PTR[((44+64))+rsp]
	and	r12d,r9d
	rorx	r13d,r9d,25
	rorx	esi,r9d,11
	lea	ebx,DWORD PTR[r14*1+rbx]
	lea	eax,DWORD PTR[r12*1+rax]
	andn	r12d,r9d,r11d
	xor	r13d,esi
	rorx	r14d,r9d,6
	lea	eax,DWORD PTR[r12*1+rax]
	xor	r13d,r14d
	mov	esi,ebx
	rorx	r12d,ebx,22
	lea	eax,DWORD PTR[r13*1+rax]
	xor	esi,ecx
	rorx	r14d,ebx,13
	rorx	r13d,ebx,2
	lea	r8d,DWORD PTR[rax*1+r8]
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((112-128))+rdi]
	xor	r14d,r12d
	xor	r15d,ecx
	xor	r14d,r13d
	lea	eax,DWORD PTR[r15*1+rax]
	mov	r12d,r9d
	add	r11d,DWORD PTR[rsp]
	and	r12d,r8d
	rorx	r13d,r8d,25
	rorx	r15d,r8d,11
	lea	eax,DWORD PTR[r14*1+rax]
	lea	r11d,DWORD PTR[r12*1+r11]
	andn	r12d,r8d,r10d
	xor	r13d,r15d
	rorx	r14d,r8d,6
	lea	r11d,DWORD PTR[r12*1+r11]
	xor	r13d,r14d
	mov	r15d,eax
	rorx	r12d,eax,22
	lea	r11d,DWORD PTR[r13*1+r11]
	xor	r15d,ebx
	rorx	r14d,eax,13
	rorx	r13d,eax,2
	lea	edx,DWORD PTR[r11*1+rdx]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((128-128))+rdi]
	xor	r14d,r12d
	xor	esi,ebx
	xor	r14d,r13d
	lea	r11d,DWORD PTR[rsi*1+r11]
	mov	r12d,r8d
	add	r10d,DWORD PTR[4+rsp]
	and	r12d,edx
	rorx	r13d,edx,25
	rorx	esi,edx,11
	lea	r11d,DWORD PTR[r14*1+r11]
	lea	r10d,DWORD PTR[r12*1+r10]
	andn	r12d,edx,r9d
	xor	r13d,esi
	rorx	r14d,edx,6
	lea	r10d,DWORD PTR[r12*1+r10]
	xor	r13d,r14d
	mov	esi,r11d
	rorx	r12d,r11d,22
	lea	r10d,DWORD PTR[r13*1+r10]
	xor	esi,eax
	rorx	r14d,r11d,13
	rorx	r13d,r11d,2
	lea	ecx,DWORD PTR[r10*1+rcx]
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((144-128))+rdi]
	xor	r14d,r12d
	xor	r15d,eax
	xor	r14d,r13d
	lea	r10d,DWORD PTR[r15*1+r10]
	mov	r12d,edx
	add	r9d,DWORD PTR[8+rsp]
	and	r12d,ecx
	rorx	r13d,ecx,25
	rorx	r15d,ecx,11
	lea	r10d,DWORD PTR[r14*1+r10]
	lea	r9d,DWORD PTR[r12*1+r9]
	andn	r12d,ecx,r8d
	xor	r13d,r15d
	rorx	r14d,ecx,6
	lea	r9d,DWORD PTR[r12*1+r9]
	xor	r13d,r14d
	mov	r15d,r10d
	rorx	r12d,r10d,22
	lea	r9d,DWORD PTR[r13*1+r9]
	xor	r15d,r11d
	rorx	r14d,r10d,13
	rorx	r13d,r10d,2
	lea	ebx,DWORD PTR[r9*1+rbx]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((160-128))+rdi]
	xor	r14d,r12d
	xor	esi,r11d
	xor	r14d,r13d
	lea	r9d,DWORD PTR[rsi*1+r9]
	mov	r12d,ecx
	add	r8d,DWORD PTR[12+rsp]
	and	r12d,ebx
	rorx	r13d,ebx,25
	rorx	esi,ebx,11
	lea	r9d,DWORD PTR[r14*1+r9]
	lea	r8d,DWORD PTR[r12*1+r8]
	andn	r12d,ebx,edx
	xor	r13d,esi
	rorx	r14d,ebx,6
	lea	r8d,DWORD PTR[r12*1+r8]
	xor	r13d,r14d
	mov	esi,r9d
	rorx	r12d,r9d,22
	lea	r8d,DWORD PTR[r13*1+r8]
	xor	esi,r10d
	rorx	r14d,r9d,13
	rorx	r13d,r9d,2
	lea	eax,DWORD PTR[r8*1+rax]
	and	r15d,esi
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((176-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r10d
	xor	r14d,r13d
	lea	r8d,DWORD PTR[r15*1+r8]
	mov	r12d,ebx
	add	edx,DWORD PTR[32+rsp]
	and	r12d,eax
	rorx	r13d,eax,25
	rorx	r15d,eax,11
	lea	r8d,DWORD PTR[r14*1+r8]
	lea	edx,DWORD PTR[r12*1+rdx]
	andn	r12d,eax,ecx
	xor	r13d,r15d
	rorx	r14d,eax,6
	lea	edx,DWORD PTR[r12*1+rdx]
	xor	r13d,r14d
	mov	r15d,r8d
	rorx	r12d,r8d,22
	lea	edx,DWORD PTR[r13*1+rdx]
	xor	r15d,r9d
	rorx	r14d,r8d,13
	rorx	r13d,r8d,2
	lea	r11d,DWORD PTR[rdx*1+r11]
	and	esi,r15d
	vpand	xmm8,xmm11,xmm12
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((192-128))+rdi]
	xor	r14d,r12d
	xor	esi,r9d
	xor	r14d,r13d
	lea	edx,DWORD PTR[rsi*1+rdx]
	mov	r12d,eax
	add	ecx,DWORD PTR[36+rsp]
	and	r12d,r11d
	rorx	r13d,r11d,25
	rorx	esi,r11d,11
	lea	edx,DWORD PTR[r14*1+rdx]
	lea	ecx,DWORD PTR[r12*1+rcx]
	andn	r12d,r11d,ebx
	xor	r13d,esi
	rorx	r14d,r11d,6
	lea	ecx,DWORD PTR[r12*1+rcx]
	xor	r13d,r14d
	mov	esi,edx
	rorx	r12d,edx,22
	lea	ecx,DWORD PTR[r13*1+rcx]
	xor	esi,r8d
	rorx	r14d,edx,13
	rorx	r13d,edx,2
	lea	r10d,DWORD PTR[rcx*1+r10]
	and	r15d,esi
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((208-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r8d
	xor	r14d,r13d
	lea	ecx,DWORD PTR[r15*1+rcx]
	mov	r12d,r11d
	add	ebx,DWORD PTR[40+rsp]
	and	r12d,r10d
	rorx	r13d,r10d,25
	rorx	r15d,r10d,11
	lea	ecx,DWORD PTR[r14*1+rcx]
	lea	ebx,DWORD PTR[r12*1+rbx]
	andn	r12d,r10d,eax
	xor	r13d,r15d
	rorx	r14d,r10d,6
	lea	ebx,DWORD PTR[r12*1+rbx]
	xor	r13d,r14d
	mov	r15d,ecx
	rorx	r12d,ecx,22
	lea	ebx,DWORD PTR[r13*1+rbx]
	xor	r15d,edx
	rorx	r14d,ecx,13
	rorx	r13d,ecx,2
	lea	r9d,DWORD PTR[rbx*1+r9]
	and	esi,r15d
	vpand	xmm11,xmm11,xmm13
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((224-128))+rdi]
	xor	r14d,r12d
	xor	esi,edx
	xor	r14d,r13d
	lea	ebx,DWORD PTR[rsi*1+rbx]
	mov	r12d,r10d
	add	eax,DWORD PTR[44+rsp]
	and	r12d,r9d
	rorx	r13d,r9d,25
	rorx	esi,r9d,11
	lea	ebx,DWORD PTR[r14*1+rbx]
	lea	eax,DWORD PTR[r12*1+rax]
	andn	r12d,r9d,r11d
	xor	r13d,esi
	rorx	r14d,r9d,6
	lea	eax,DWORD PTR[r12*1+rax]
	xor	r13d,r14d
	mov	esi,ebx
	rorx	r12d,ebx,22
	lea	eax,DWORD PTR[r13*1+rax]
	xor	esi,ecx
	rorx	r14d,ebx,13
	rorx	r13d,ebx,2
	lea	r8d,DWORD PTR[rax*1+r8]
	and	r15d,esi
	vpor	xmm8,xmm8,xmm11
	vaesenclast	xmm11,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	xor	r14d,r12d
	xor	r15d,ecx
	xor	r14d,r13d
	lea	eax,DWORD PTR[r15*1+rax]
	mov	r12d,r9d
	vpextrq	r12,xmm15,1
	vmovq	r13,xmm15
	mov	r15,QWORD PTR[552+rsp]
	add	eax,r14d
	lea	rbp,QWORD PTR[448+rsp]

	vpand	xmm11,xmm11,xmm14
	vpor	xmm8,xmm8,xmm11
	vmovdqu	XMMWORD PTR[r13*1+r12],xmm8
	lea	r13,QWORD PTR[16+r13]

	add	eax,DWORD PTR[r15]
	add	ebx,DWORD PTR[4+r15]
	add	ecx,DWORD PTR[8+r15]
	add	edx,DWORD PTR[12+r15]
	add	r8d,DWORD PTR[16+r15]
	add	r9d,DWORD PTR[20+r15]
	add	r10d,DWORD PTR[24+r15]
	add	r11d,DWORD PTR[28+r15]

	mov	DWORD PTR[r15],eax
	mov	DWORD PTR[4+r15],ebx
	mov	DWORD PTR[8+r15],ecx
	mov	DWORD PTR[12+r15],edx
	mov	DWORD PTR[16+r15],r8d
	mov	DWORD PTR[20+r15],r9d
	mov	DWORD PTR[24+r15],r10d
	mov	DWORD PTR[28+r15],r11d

	cmp	r13,QWORD PTR[80+rbp]
	je	$L$done_avx2

	xor	r14d,r14d
	mov	esi,ebx
	mov	r12d,r9d
	xor	esi,ecx
	jmp	$L$ower_avx2
ALIGN	16
$L$ower_avx2::
	vmovdqu	xmm9,XMMWORD PTR[r13]
	vpinsrq	xmm15,xmm15,r13,0
	add	r11d,DWORD PTR[((0+16))+rbp]
	and	r12d,r8d
	rorx	r13d,r8d,25
	rorx	r15d,r8d,11
	lea	eax,DWORD PTR[r14*1+rax]
	lea	r11d,DWORD PTR[r12*1+r11]
	andn	r12d,r8d,r10d
	xor	r13d,r15d
	rorx	r14d,r8d,6
	lea	r11d,DWORD PTR[r12*1+r11]
	xor	r13d,r14d
	mov	r15d,eax
	rorx	r12d,eax,22
	lea	r11d,DWORD PTR[r13*1+r11]
	xor	r15d,ebx
	rorx	r14d,eax,13
	rorx	r13d,eax,2
	lea	edx,DWORD PTR[r11*1+rdx]
	and	esi,r15d
	vpxor	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((16-128))+rdi]
	xor	r14d,r12d
	xor	esi,ebx
	xor	r14d,r13d
	lea	r11d,DWORD PTR[rsi*1+r11]
	mov	r12d,r8d
	add	r10d,DWORD PTR[((4+16))+rbp]
	and	r12d,edx
	rorx	r13d,edx,25
	rorx	esi,edx,11
	lea	r11d,DWORD PTR[r14*1+r11]
	lea	r10d,DWORD PTR[r12*1+r10]
	andn	r12d,edx,r9d
	xor	r13d,esi
	rorx	r14d,edx,6
	lea	r10d,DWORD PTR[r12*1+r10]
	xor	r13d,r14d
	mov	esi,r11d
	rorx	r12d,r11d,22
	lea	r10d,DWORD PTR[r13*1+r10]
	xor	esi,eax
	rorx	r14d,r11d,13
	rorx	r13d,r11d,2
	lea	ecx,DWORD PTR[r10*1+rcx]
	and	r15d,esi
	vpxor	xmm9,xmm9,xmm8
	xor	r14d,r12d
	xor	r15d,eax
	xor	r14d,r13d
	lea	r10d,DWORD PTR[r15*1+r10]
	mov	r12d,edx
	add	r9d,DWORD PTR[((8+16))+rbp]
	and	r12d,ecx
	rorx	r13d,ecx,25
	rorx	r15d,ecx,11
	lea	r10d,DWORD PTR[r14*1+r10]
	lea	r9d,DWORD PTR[r12*1+r9]
	andn	r12d,ecx,r8d
	xor	r13d,r15d
	rorx	r14d,ecx,6
	lea	r9d,DWORD PTR[r12*1+r9]
	xor	r13d,r14d
	mov	r15d,r10d
	rorx	r12d,r10d,22
	lea	r9d,DWORD PTR[r13*1+r9]
	xor	r15d,r11d
	rorx	r14d,r10d,13
	rorx	r13d,r10d,2
	lea	ebx,DWORD PTR[r9*1+rbx]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((32-128))+rdi]
	xor	r14d,r12d
	xor	esi,r11d
	xor	r14d,r13d
	lea	r9d,DWORD PTR[rsi*1+r9]
	mov	r12d,ecx
	add	r8d,DWORD PTR[((12+16))+rbp]
	and	r12d,ebx
	rorx	r13d,ebx,25
	rorx	esi,ebx,11
	lea	r9d,DWORD PTR[r14*1+r9]
	lea	r8d,DWORD PTR[r12*1+r8]
	andn	r12d,ebx,edx
	xor	r13d,esi
	rorx	r14d,ebx,6
	lea	r8d,DWORD PTR[r12*1+r8]
	xor	r13d,r14d
	mov	esi,r9d
	rorx	r12d,r9d,22
	lea	r8d,DWORD PTR[r13*1+r8]
	xor	esi,r10d
	rorx	r14d,r9d,13
	rorx	r13d,r9d,2
	lea	eax,DWORD PTR[r8*1+rax]
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((48-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r10d
	xor	r14d,r13d
	lea	r8d,DWORD PTR[r15*1+r8]
	mov	r12d,ebx
	add	edx,DWORD PTR[((32+16))+rbp]
	and	r12d,eax
	rorx	r13d,eax,25
	rorx	r15d,eax,11
	lea	r8d,DWORD PTR[r14*1+r8]
	lea	edx,DWORD PTR[r12*1+rdx]
	andn	r12d,eax,ecx
	xor	r13d,r15d
	rorx	r14d,eax,6
	lea	edx,DWORD PTR[r12*1+rdx]
	xor	r13d,r14d
	mov	r15d,r8d
	rorx	r12d,r8d,22
	lea	edx,DWORD PTR[r13*1+rdx]
	xor	r15d,r9d
	rorx	r14d,r8d,13
	rorx	r13d,r8d,2
	lea	r11d,DWORD PTR[rdx*1+r11]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((64-128))+rdi]
	xor	r14d,r12d
	xor	esi,r9d
	xor	r14d,r13d
	lea	edx,DWORD PTR[rsi*1+rdx]
	mov	r12d,eax
	add	ecx,DWORD PTR[((36+16))+rbp]
	and	r12d,r11d
	rorx	r13d,r11d,25
	rorx	esi,r11d,11
	lea	edx,DWORD PTR[r14*1+rdx]
	lea	ecx,DWORD PTR[r12*1+rcx]
	andn	r12d,r11d,ebx
	xor	r13d,esi
	rorx	r14d,r11d,6
	lea	ecx,DWORD PTR[r12*1+rcx]
	xor	r13d,r14d
	mov	esi,edx
	rorx	r12d,edx,22
	lea	ecx,DWORD PTR[r13*1+rcx]
	xor	esi,r8d
	rorx	r14d,edx,13
	rorx	r13d,edx,2
	lea	r10d,DWORD PTR[rcx*1+r10]
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((80-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r8d
	xor	r14d,r13d
	lea	ecx,DWORD PTR[r15*1+rcx]
	mov	r12d,r11d
	add	ebx,DWORD PTR[((40+16))+rbp]
	and	r12d,r10d
	rorx	r13d,r10d,25
	rorx	r15d,r10d,11
	lea	ecx,DWORD PTR[r14*1+rcx]
	lea	ebx,DWORD PTR[r12*1+rbx]
	andn	r12d,r10d,eax
	xor	r13d,r15d
	rorx	r14d,r10d,6
	lea	ebx,DWORD PTR[r12*1+rbx]
	xor	r13d,r14d
	mov	r15d,ecx
	rorx	r12d,ecx,22
	lea	ebx,DWORD PTR[r13*1+rbx]
	xor	r15d,edx
	rorx	r14d,ecx,13
	rorx	r13d,ecx,2
	lea	r9d,DWORD PTR[rbx*1+r9]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((96-128))+rdi]
	xor	r14d,r12d
	xor	esi,edx
	xor	r14d,r13d
	lea	ebx,DWORD PTR[rsi*1+rbx]
	mov	r12d,r10d
	add	eax,DWORD PTR[((44+16))+rbp]
	and	r12d,r9d
	rorx	r13d,r9d,25
	rorx	esi,r9d,11
	lea	ebx,DWORD PTR[r14*1+rbx]
	lea	eax,DWORD PTR[r12*1+rax]
	andn	r12d,r9d,r11d
	xor	r13d,esi
	rorx	r14d,r9d,6
	lea	eax,DWORD PTR[r12*1+rax]
	xor	r13d,r14d
	mov	esi,ebx
	rorx	r12d,ebx,22
	lea	eax,DWORD PTR[r13*1+rax]
	xor	esi,ecx
	rorx	r14d,ebx,13
	rorx	r13d,ebx,2
	lea	r8d,DWORD PTR[rax*1+r8]
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((112-128))+rdi]
	xor	r14d,r12d
	xor	r15d,ecx
	xor	r14d,r13d
	lea	eax,DWORD PTR[r15*1+rax]
	mov	r12d,r9d
	lea	rbp,QWORD PTR[((-64))+rbp]
	add	r11d,DWORD PTR[((0+16))+rbp]
	and	r12d,r8d
	rorx	r13d,r8d,25
	rorx	r15d,r8d,11
	lea	eax,DWORD PTR[r14*1+rax]
	lea	r11d,DWORD PTR[r12*1+r11]
	andn	r12d,r8d,r10d
	xor	r13d,r15d
	rorx	r14d,r8d,6
	lea	r11d,DWORD PTR[r12*1+r11]
	xor	r13d,r14d
	mov	r15d,eax
	rorx	r12d,eax,22
	lea	r11d,DWORD PTR[r13*1+r11]
	xor	r15d,ebx
	rorx	r14d,eax,13
	rorx	r13d,eax,2
	lea	edx,DWORD PTR[r11*1+rdx]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((128-128))+rdi]
	xor	r14d,r12d
	xor	esi,ebx
	xor	r14d,r13d
	lea	r11d,DWORD PTR[rsi*1+r11]
	mov	r12d,r8d
	add	r10d,DWORD PTR[((4+16))+rbp]
	and	r12d,edx
	rorx	r13d,edx,25
	rorx	esi,edx,11
	lea	r11d,DWORD PTR[r14*1+r11]
	lea	r10d,DWORD PTR[r12*1+r10]
	andn	r12d,edx,r9d
	xor	r13d,esi
	rorx	r14d,edx,6
	lea	r10d,DWORD PTR[r12*1+r10]
	xor	r13d,r14d
	mov	esi,r11d
	rorx	r12d,r11d,22
	lea	r10d,DWORD PTR[r13*1+r10]
	xor	esi,eax
	rorx	r14d,r11d,13
	rorx	r13d,r11d,2
	lea	ecx,DWORD PTR[r10*1+rcx]
	and	r15d,esi
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((144-128))+rdi]
	xor	r14d,r12d
	xor	r15d,eax
	xor	r14d,r13d
	lea	r10d,DWORD PTR[r15*1+r10]
	mov	r12d,edx
	add	r9d,DWORD PTR[((8+16))+rbp]
	and	r12d,ecx
	rorx	r13d,ecx,25
	rorx	r15d,ecx,11
	lea	r10d,DWORD PTR[r14*1+r10]
	lea	r9d,DWORD PTR[r12*1+r9]
	andn	r12d,ecx,r8d
	xor	r13d,r15d
	rorx	r14d,ecx,6
	lea	r9d,DWORD PTR[r12*1+r9]
	xor	r13d,r14d
	mov	r15d,r10d
	rorx	r12d,r10d,22
	lea	r9d,DWORD PTR[r13*1+r9]
	xor	r15d,r11d
	rorx	r14d,r10d,13
	rorx	r13d,r10d,2
	lea	ebx,DWORD PTR[r9*1+rbx]
	and	esi,r15d
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((160-128))+rdi]
	xor	r14d,r12d
	xor	esi,r11d
	xor	r14d,r13d
	lea	r9d,DWORD PTR[rsi*1+r9]
	mov	r12d,ecx
	add	r8d,DWORD PTR[((12+16))+rbp]
	and	r12d,ebx
	rorx	r13d,ebx,25
	rorx	esi,ebx,11
	lea	r9d,DWORD PTR[r14*1+r9]
	lea	r8d,DWORD PTR[r12*1+r8]
	andn	r12d,ebx,edx
	xor	r13d,esi
	rorx	r14d,ebx,6
	lea	r8d,DWORD PTR[r12*1+r8]
	xor	r13d,r14d
	mov	esi,r9d
	rorx	r12d,r9d,22
	lea	r8d,DWORD PTR[r13*1+r8]
	xor	esi,r10d
	rorx	r14d,r9d,13
	rorx	r13d,r9d,2
	lea	eax,DWORD PTR[r8*1+rax]
	and	r15d,esi
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((176-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r10d
	xor	r14d,r13d
	lea	r8d,DWORD PTR[r15*1+r8]
	mov	r12d,ebx
	add	edx,DWORD PTR[((32+16))+rbp]
	and	r12d,eax
	rorx	r13d,eax,25
	rorx	r15d,eax,11
	lea	r8d,DWORD PTR[r14*1+r8]
	lea	edx,DWORD PTR[r12*1+rdx]
	andn	r12d,eax,ecx
	xor	r13d,r15d
	rorx	r14d,eax,6
	lea	edx,DWORD PTR[r12*1+rdx]
	xor	r13d,r14d
	mov	r15d,r8d
	rorx	r12d,r8d,22
	lea	edx,DWORD PTR[r13*1+rdx]
	xor	r15d,r9d
	rorx	r14d,r8d,13
	rorx	r13d,r8d,2
	lea	r11d,DWORD PTR[rdx*1+r11]
	and	esi,r15d
	vpand	xmm8,xmm11,xmm12
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((192-128))+rdi]
	xor	r14d,r12d
	xor	esi,r9d
	xor	r14d,r13d
	lea	edx,DWORD PTR[rsi*1+rdx]
	mov	r12d,eax
	add	ecx,DWORD PTR[((36+16))+rbp]
	and	r12d,r11d
	rorx	r13d,r11d,25
	rorx	esi,r11d,11
	lea	edx,DWORD PTR[r14*1+rdx]
	lea	ecx,DWORD PTR[r12*1+rcx]
	andn	r12d,r11d,ebx
	xor	r13d,esi
	rorx	r14d,r11d,6
	lea	ecx,DWORD PTR[r12*1+rcx]
	xor	r13d,r14d
	mov	esi,edx
	rorx	r12d,edx,22
	lea	ecx,DWORD PTR[r13*1+rcx]
	xor	esi,r8d
	rorx	r14d,edx,13
	rorx	r13d,edx,2
	lea	r10d,DWORD PTR[rcx*1+r10]
	and	r15d,esi
	vaesenclast	xmm11,xmm9,xmm10
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((208-128))+rdi]
	xor	r14d,r12d
	xor	r15d,r8d
	xor	r14d,r13d
	lea	ecx,DWORD PTR[r15*1+rcx]
	mov	r12d,r11d
	add	ebx,DWORD PTR[((40+16))+rbp]
	and	r12d,r10d
	rorx	r13d,r10d,25
	rorx	r15d,r10d,11
	lea	ecx,DWORD PTR[r14*1+rcx]
	lea	ebx,DWORD PTR[r12*1+rbx]
	andn	r12d,r10d,eax
	xor	r13d,r15d
	rorx	r14d,r10d,6
	lea	ebx,DWORD PTR[r12*1+rbx]
	xor	r13d,r14d
	mov	r15d,ecx
	rorx	r12d,ecx,22
	lea	ebx,DWORD PTR[r13*1+rbx]
	xor	r15d,edx
	rorx	r14d,ecx,13
	rorx	r13d,ecx,2
	lea	r9d,DWORD PTR[rbx*1+r9]
	and	esi,r15d
	vpand	xmm11,xmm11,xmm13
	vaesenc	xmm9,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((224-128))+rdi]
	xor	r14d,r12d
	xor	esi,edx
	xor	r14d,r13d
	lea	ebx,DWORD PTR[rsi*1+rbx]
	mov	r12d,r10d
	add	eax,DWORD PTR[((44+16))+rbp]
	and	r12d,r9d
	rorx	r13d,r9d,25
	rorx	esi,r9d,11
	lea	ebx,DWORD PTR[r14*1+rbx]
	lea	eax,DWORD PTR[r12*1+rax]
	andn	r12d,r9d,r11d
	xor	r13d,esi
	rorx	r14d,r9d,6
	lea	eax,DWORD PTR[r12*1+rax]
	xor	r13d,r14d
	mov	esi,ebx
	rorx	r12d,ebx,22
	lea	eax,DWORD PTR[r13*1+rax]
	xor	esi,ecx
	rorx	r14d,ebx,13
	rorx	r13d,ebx,2
	lea	r8d,DWORD PTR[rax*1+r8]
	and	r15d,esi
	vpor	xmm8,xmm8,xmm11
	vaesenclast	xmm11,xmm9,xmm10
	vmovdqu	xmm10,XMMWORD PTR[((0-128))+rdi]
	xor	r14d,r12d
	xor	r15d,ecx
	xor	r14d,r13d
	lea	eax,DWORD PTR[r15*1+rax]
	mov	r12d,r9d
	vmovq	r13,xmm15
	vpextrq	r15,xmm15,1
	vpand	xmm11,xmm11,xmm14
	vpor	xmm8,xmm8,xmm11
	lea	rbp,QWORD PTR[((-64))+rbp]
	vmovdqu	XMMWORD PTR[r13*1+r15],xmm8
	lea	r13,QWORD PTR[16+r13]
	cmp	rbp,rsp
	jae	$L$ower_avx2

	mov	r15,QWORD PTR[552+rsp]
	lea	r13,QWORD PTR[64+r13]
	mov	rsi,QWORD PTR[560+rsp]
	add	eax,r14d
	lea	rsp,QWORD PTR[448+rsp]

	add	eax,DWORD PTR[r15]
	add	ebx,DWORD PTR[4+r15]
	add	ecx,DWORD PTR[8+r15]
	add	edx,DWORD PTR[12+r15]
	add	r8d,DWORD PTR[16+r15]
	add	r9d,DWORD PTR[20+r15]
	add	r10d,DWORD PTR[24+r15]
	lea	r12,QWORD PTR[r13*1+rsi]
	add	r11d,DWORD PTR[28+r15]

	cmp	r13,QWORD PTR[((64+16))+rsp]

	mov	DWORD PTR[r15],eax
	cmove	r12,rsp
	mov	DWORD PTR[4+r15],ebx
	mov	DWORD PTR[8+r15],ecx
	mov	DWORD PTR[12+r15],edx
	mov	DWORD PTR[16+r15],r8d
	mov	DWORD PTR[20+r15],r9d
	mov	DWORD PTR[24+r15],r10d
	mov	DWORD PTR[28+r15],r11d

	jbe	$L$oop_avx2
	lea	rbp,QWORD PTR[rsp]

$L$done_avx2::
	lea	rsp,QWORD PTR[rbp]
	mov	r8,QWORD PTR[((64+32))+rsp]
	mov	rsi,QWORD PTR[((64+56))+rsp]
	vmovdqu	XMMWORD PTR[r8],xmm8
	vzeroall
	movaps	xmm6,XMMWORD PTR[128+rsp]
	movaps	xmm7,XMMWORD PTR[144+rsp]
	movaps	xmm8,XMMWORD PTR[160+rsp]
	movaps	xmm9,XMMWORD PTR[176+rsp]
	movaps	xmm10,XMMWORD PTR[192+rsp]
	movaps	xmm11,XMMWORD PTR[208+rsp]
	movaps	xmm12,XMMWORD PTR[224+rsp]
	movaps	xmm13,XMMWORD PTR[240+rsp]
	movaps	xmm14,XMMWORD PTR[256+rsp]
	movaps	xmm15,XMMWORD PTR[272+rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$epilogue_avx2::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_cbc_sha256_enc_avx2::
aesni_cbc_sha256_enc_avx2	ENDP

ALIGN	32
aesni_cbc_sha256_enc_shaext	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_cbc_sha256_enc_shaext::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	mov	r10,QWORD PTR[56+rsp]
	lea	rsp,QWORD PTR[((-168))+rsp]
	movaps	XMMWORD PTR[(-8-160)+rax],xmm6
	movaps	XMMWORD PTR[(-8-144)+rax],xmm7
	movaps	XMMWORD PTR[(-8-128)+rax],xmm8
	movaps	XMMWORD PTR[(-8-112)+rax],xmm9
	movaps	XMMWORD PTR[(-8-96)+rax],xmm10
	movaps	XMMWORD PTR[(-8-80)+rax],xmm11
	movaps	XMMWORD PTR[(-8-64)+rax],xmm12
	movaps	XMMWORD PTR[(-8-48)+rax],xmm13
	movaps	XMMWORD PTR[(-8-32)+rax],xmm14
	movaps	XMMWORD PTR[(-8-16)+rax],xmm15
$L$prologue_shaext::
	lea	rax,QWORD PTR[((K256+128))]
	movdqu	xmm1,XMMWORD PTR[r9]
	movdqu	xmm2,XMMWORD PTR[16+r9]
	movdqa	xmm3,XMMWORD PTR[((512-128))+rax]

	mov	r11d,DWORD PTR[240+rcx]
	sub	rsi,rdi
	movups	xmm15,XMMWORD PTR[rcx]
	movups	xmm4,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[112+rcx]

	pshufd	xmm0,xmm1,01bh
	pshufd	xmm1,xmm1,1h
	pshufd	xmm2,xmm2,01bh
	movdqa	xmm7,xmm3
DB	102,15,58,15,202,8
	punpcklqdq	xmm2,xmm0

	jmp	$L$oop_shaext

ALIGN	16
$L$oop_shaext::
	movdqu	xmm10,XMMWORD PTR[r10]
	movdqu	xmm11,XMMWORD PTR[16+r10]
	movdqu	xmm12,XMMWORD PTR[32+r10]
DB	102,68,15,56,0,211
	movdqu	xmm13,XMMWORD PTR[48+r10]

	movdqa	xmm0,XMMWORD PTR[((0-128))+rax]
	paddd	xmm0,xmm10
DB	102,68,15,56,0,219
	movdqa	xmm9,xmm2
	movdqa	xmm8,xmm1
	movups	xmm14,XMMWORD PTR[rdi]
	xorps	xmm14,xmm15
	xorps	xmm6,xmm14
	movups	xmm5,XMMWORD PTR[((-80))+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movups	xmm4,XMMWORD PTR[((-64))+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((32-128))+rax]
	paddd	xmm0,xmm11
DB	102,68,15,56,0,227
	lea	r10,QWORD PTR[64+r10]
	movups	xmm5,XMMWORD PTR[((-48))+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movups	xmm4,XMMWORD PTR[((-32))+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((64-128))+rax]
	paddd	xmm0,xmm12
DB	102,68,15,56,0,235
DB	69,15,56,204,211
	movups	xmm5,XMMWORD PTR[((-16))+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm13
DB	102,65,15,58,15,220,4
	paddd	xmm10,xmm3
	movups	xmm4,XMMWORD PTR[rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((96-128))+rax]
	paddd	xmm0,xmm13
DB	69,15,56,205,213
DB	69,15,56,204,220
	movups	xmm5,XMMWORD PTR[16+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movups	xmm4,XMMWORD PTR[32+rcx]
	aesenc	xmm6,xmm5
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,221,4
	paddd	xmm11,xmm3
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((128-128))+rax]
	paddd	xmm0,xmm10
DB	69,15,56,205,218
DB	69,15,56,204,229
	movups	xmm5,XMMWORD PTR[48+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
	paddd	xmm12,xmm3
	cmp	r11d,11
	jb	$L$aesenclast1
	movups	xmm4,XMMWORD PTR[64+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[80+rcx]
	aesenc	xmm6,xmm4
	je	$L$aesenclast1
	movups	xmm4,XMMWORD PTR[96+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[112+rcx]
	aesenc	xmm6,xmm4
$L$aesenclast1::
	aesenclast	xmm6,xmm5
	movups	xmm4,XMMWORD PTR[((16-112))+rcx]
	nop
DB	15,56,203,202
	movups	xmm14,XMMWORD PTR[16+rdi]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[rdi*1+rsi],xmm6
	xorps	xmm6,xmm14
	movups	xmm5,XMMWORD PTR[((-80))+rcx]
	aesenc	xmm6,xmm4
	movdqa	xmm0,XMMWORD PTR[((160-128))+rax]
	paddd	xmm0,xmm11
DB	69,15,56,205,227
DB	69,15,56,204,234
	movups	xmm4,XMMWORD PTR[((-64))+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm12
DB	102,65,15,58,15,219,4
	paddd	xmm13,xmm3
	movups	xmm5,XMMWORD PTR[((-48))+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((192-128))+rax]
	paddd	xmm0,xmm12
DB	69,15,56,205,236
DB	69,15,56,204,211
	movups	xmm4,XMMWORD PTR[((-32))+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm13
DB	102,65,15,58,15,220,4
	paddd	xmm10,xmm3
	movups	xmm5,XMMWORD PTR[((-16))+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((224-128))+rax]
	paddd	xmm0,xmm13
DB	69,15,56,205,213
DB	69,15,56,204,220
	movups	xmm4,XMMWORD PTR[rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,221,4
	paddd	xmm11,xmm3
	movups	xmm5,XMMWORD PTR[16+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((256-128))+rax]
	paddd	xmm0,xmm10
DB	69,15,56,205,218
DB	69,15,56,204,229
	movups	xmm4,XMMWORD PTR[32+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
	paddd	xmm12,xmm3
	movups	xmm5,XMMWORD PTR[48+rcx]
	aesenc	xmm6,xmm4
	cmp	r11d,11
	jb	$L$aesenclast2
	movups	xmm4,XMMWORD PTR[64+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[80+rcx]
	aesenc	xmm6,xmm4
	je	$L$aesenclast2
	movups	xmm4,XMMWORD PTR[96+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[112+rcx]
	aesenc	xmm6,xmm4
$L$aesenclast2::
	aesenclast	xmm6,xmm5
	movups	xmm4,XMMWORD PTR[((16-112))+rcx]
	nop
DB	15,56,203,202
	movups	xmm14,XMMWORD PTR[32+rdi]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[16+rdi*1+rsi],xmm6
	xorps	xmm6,xmm14
	movups	xmm5,XMMWORD PTR[((-80))+rcx]
	aesenc	xmm6,xmm4
	movdqa	xmm0,XMMWORD PTR[((288-128))+rax]
	paddd	xmm0,xmm11
DB	69,15,56,205,227
DB	69,15,56,204,234
	movups	xmm4,XMMWORD PTR[((-64))+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm12
DB	102,65,15,58,15,219,4
	paddd	xmm13,xmm3
	movups	xmm5,XMMWORD PTR[((-48))+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((320-128))+rax]
	paddd	xmm0,xmm12
DB	69,15,56,205,236
DB	69,15,56,204,211
	movups	xmm4,XMMWORD PTR[((-32))+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm13
DB	102,65,15,58,15,220,4
	paddd	xmm10,xmm3
	movups	xmm5,XMMWORD PTR[((-16))+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((352-128))+rax]
	paddd	xmm0,xmm13
DB	69,15,56,205,213
DB	69,15,56,204,220
	movups	xmm4,XMMWORD PTR[rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,221,4
	paddd	xmm11,xmm3
	movups	xmm5,XMMWORD PTR[16+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((384-128))+rax]
	paddd	xmm0,xmm10
DB	69,15,56,205,218
DB	69,15,56,204,229
	movups	xmm4,XMMWORD PTR[32+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
	paddd	xmm12,xmm3
	movups	xmm5,XMMWORD PTR[48+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,202
	movdqa	xmm0,XMMWORD PTR[((416-128))+rax]
	paddd	xmm0,xmm11
DB	69,15,56,205,227
DB	69,15,56,204,234
	cmp	r11d,11
	jb	$L$aesenclast3
	movups	xmm4,XMMWORD PTR[64+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[80+rcx]
	aesenc	xmm6,xmm4
	je	$L$aesenclast3
	movups	xmm4,XMMWORD PTR[96+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[112+rcx]
	aesenc	xmm6,xmm4
$L$aesenclast3::
	aesenclast	xmm6,xmm5
	movups	xmm4,XMMWORD PTR[((16-112))+rcx]
	nop
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movdqa	xmm3,xmm12
DB	102,65,15,58,15,219,4
	paddd	xmm13,xmm3
	movups	xmm14,XMMWORD PTR[48+rdi]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[32+rdi*1+rsi],xmm6
	xorps	xmm6,xmm14
	movups	xmm5,XMMWORD PTR[((-80))+rcx]
	aesenc	xmm6,xmm4
	movups	xmm4,XMMWORD PTR[((-64))+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((448-128))+rax]
	paddd	xmm0,xmm12
DB	69,15,56,205,236
	movdqa	xmm3,xmm7
	movups	xmm5,XMMWORD PTR[((-48))+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movups	xmm4,XMMWORD PTR[((-32))+rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,202

	movdqa	xmm0,XMMWORD PTR[((480-128))+rax]
	paddd	xmm0,xmm13
	movups	xmm5,XMMWORD PTR[((-16))+rcx]
	aesenc	xmm6,xmm4
	movups	xmm4,XMMWORD PTR[rcx]
	aesenc	xmm6,xmm5
DB	15,56,203,209
	pshufd	xmm0,xmm0,00eh
	movups	xmm5,XMMWORD PTR[16+rcx]
	aesenc	xmm6,xmm4
DB	15,56,203,202

	movups	xmm4,XMMWORD PTR[32+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[48+rcx]
	aesenc	xmm6,xmm4
	cmp	r11d,11
	jb	$L$aesenclast4
	movups	xmm4,XMMWORD PTR[64+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[80+rcx]
	aesenc	xmm6,xmm4
	je	$L$aesenclast4
	movups	xmm4,XMMWORD PTR[96+rcx]
	aesenc	xmm6,xmm5
	movups	xmm5,XMMWORD PTR[112+rcx]
	aesenc	xmm6,xmm4
$L$aesenclast4::
	aesenclast	xmm6,xmm5
	movups	xmm4,XMMWORD PTR[((16-112))+rcx]
	nop

	paddd	xmm2,xmm9
	paddd	xmm1,xmm8

	dec	rdx
	movups	XMMWORD PTR[48+rdi*1+rsi],xmm6
	lea	rdi,QWORD PTR[64+rdi]
	jnz	$L$oop_shaext

	pshufd	xmm2,xmm2,1h
	pshufd	xmm3,xmm1,01bh
	pshufd	xmm1,xmm1,1h
	punpckhqdq	xmm1,xmm2
DB	102,15,58,15,211,8

	movups	XMMWORD PTR[r8],xmm6
	movdqu	XMMWORD PTR[r9],xmm1
	movdqu	XMMWORD PTR[16+r9],xmm2
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	movaps	xmm11,XMMWORD PTR[80+rsp]
	movaps	xmm12,XMMWORD PTR[96+rsp]
	movaps	xmm13,XMMWORD PTR[112+rsp]
	movaps	xmm14,XMMWORD PTR[128+rsp]
	movaps	xmm15,XMMWORD PTR[144+rsp]
	lea	rsp,QWORD PTR[((8+160))+rsp]
$L$epilogue_shaext::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_cbc_sha256_enc_shaext::
aesni_cbc_sha256_enc_shaext	ENDP
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	mov	rsi,QWORD PTR[8+r9]
	mov	r11,QWORD PTR[56+r9]

	mov	r10d,DWORD PTR[r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$in_prologue
	lea	r10,QWORD PTR[aesni_cbc_sha256_enc_shaext]
	cmp	rbx,r10
	jb	$L$not_in_shaext

	lea	rsi,QWORD PTR[rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch
	lea	rax,QWORD PTR[168+rax]
	jmp	$L$in_prologue
$L$not_in_shaext::
	lea	r10,QWORD PTR[$L$avx2_shortcut]
	cmp	rbx,r10
	jb	$L$not_in_avx2

	and	rax,-256*4
	add	rax,448
$L$not_in_avx2::
	mov	rsi,rax
	mov	rax,QWORD PTR[((64+56))+rax]
	lea	rax,QWORD PTR[48+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[232+r8],r14
	mov	QWORD PTR[240+r8],r15

	lea	rsi,QWORD PTR[((64+64))+rsi]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

$L$in_prologue::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	mov	rdi,QWORD PTR[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0a548f3fch

	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD PTR[8+rsi]
	mov	r8,QWORD PTR[rsi]
	mov	r9,QWORD PTR[16+rsi]
	mov	r10,QWORD PTR[40+rsi]
	lea	r11,QWORD PTR[56+rsi]
	lea	r12,QWORD PTR[24+rsi]
	mov	QWORD PTR[32+rsp],r10
	mov	QWORD PTR[40+rsp],r11
	mov	QWORD PTR[48+rsp],r12
	mov	QWORD PTR[56+rsp],rcx
	call	QWORD PTR[__imp_RtlVirtualUnwind]

	mov	eax,1
	add	rsp,64
	popfq
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	pop	rdi
	pop	rsi
	DB	0F3h,0C3h		;repret
se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
	DD	imagerel $L$SEH_begin_aesni_cbc_sha256_enc_xop
	DD	imagerel $L$SEH_end_aesni_cbc_sha256_enc_xop
	DD	imagerel $L$SEH_info_aesni_cbc_sha256_enc_xop

	DD	imagerel $L$SEH_begin_aesni_cbc_sha256_enc_avx
	DD	imagerel $L$SEH_end_aesni_cbc_sha256_enc_avx
	DD	imagerel $L$SEH_info_aesni_cbc_sha256_enc_avx
	DD	imagerel $L$SEH_begin_aesni_cbc_sha256_enc_avx2
	DD	imagerel $L$SEH_end_aesni_cbc_sha256_enc_avx2
	DD	imagerel $L$SEH_info_aesni_cbc_sha256_enc_avx2
	DD	imagerel $L$SEH_begin_aesni_cbc_sha256_enc_shaext
	DD	imagerel $L$SEH_end_aesni_cbc_sha256_enc_shaext
	DD	imagerel $L$SEH_info_aesni_cbc_sha256_enc_shaext
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_aesni_cbc_sha256_enc_xop::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$prologue_xop,imagerel $L$epilogue_xop

$L$SEH_info_aesni_cbc_sha256_enc_avx::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$prologue_avx,imagerel $L$epilogue_avx
$L$SEH_info_aesni_cbc_sha256_enc_avx2::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$prologue_avx2,imagerel $L$epilogue_avx2
$L$SEH_info_aesni_cbc_sha256_enc_shaext::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$prologue_shaext,imagerel $L$epilogue_shaext

.xdata	ENDS
END
