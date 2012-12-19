OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'


ALIGN	16
MULADD_128x512	PROC PRIVATE
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	mov	QWORD PTR[rcx],r8
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	r8,rdx
	mov	rbp,QWORD PTR[8+rdi]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	mov	QWORD PTR[8+rcx],r9
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	r9,rdx
	DB	0F3h,0C3h		;repret
MULADD_128x512	ENDP

ALIGN	16
mont_reduce	PROC PRIVATE
	lea	rdi,QWORD PTR[192+rsp]
	mov	rsi,QWORD PTR[32+rsp]
	add	rsi,576
	lea	rcx,QWORD PTR[520+rsp]

	mov	rbp,QWORD PTR[96+rcx]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	mov	r8,QWORD PTR[rcx]
	add	r8,rax
	adc	rdx,0
	mov	QWORD PTR[rdi],r8
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	mov	r9,QWORD PTR[8+rcx]
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	mov	r10,QWORD PTR[16+rcx]
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	mov	r11,QWORD PTR[24+rcx]
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	mov	r12,QWORD PTR[32+rcx]
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	mov	r13,QWORD PTR[40+rcx]
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	mov	r14,QWORD PTR[48+rcx]
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	mov	r15,QWORD PTR[56+rcx]
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	r8,rdx
	mov	rbp,QWORD PTR[104+rcx]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	mov	QWORD PTR[8+rdi],r9
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	r9,rdx
	mov	rbp,QWORD PTR[112+rcx]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	mov	QWORD PTR[16+rdi],r10
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	r10,rdx
	mov	rbp,QWORD PTR[120+rcx]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	mov	QWORD PTR[24+rdi],r11
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	r11,rdx
	xor	rax,rax

	add	r8,QWORD PTR[64+rcx]
	adc	r9,QWORD PTR[72+rcx]
	adc	r10,QWORD PTR[80+rcx]
	adc	r11,QWORD PTR[88+rcx]
	adc	rax,0




	mov	QWORD PTR[64+rdi],r8
	mov	QWORD PTR[72+rdi],r9
	mov	rbp,r10
	mov	QWORD PTR[88+rdi],r11

	mov	QWORD PTR[384+rsp],rax

	mov	r8,QWORD PTR[rdi]
	mov	r9,QWORD PTR[8+rdi]
	mov	r10,QWORD PTR[16+rdi]
	mov	r11,QWORD PTR[24+rdi]








	add	rdi,8*10

	add	rsi,64
	lea	rcx,QWORD PTR[296+rsp]

	call	MULADD_128x512


	mov	rax,QWORD PTR[384+rsp]


	add	r8,QWORD PTR[((-16))+rdi]
	adc	r9,QWORD PTR[((-8))+rdi]
	mov	QWORD PTR[64+rcx],r8
	mov	QWORD PTR[72+rcx],r9

	adc	rax,rax
	mov	QWORD PTR[384+rsp],rax

	lea	rdi,QWORD PTR[192+rsp]
	add	rsi,64





	mov	r8,QWORD PTR[rsi]
	mov	rbx,QWORD PTR[8+rsi]

	mov	rax,QWORD PTR[rcx]
	mul	r8
	mov	rbp,rax
	mov	r9,rdx

	mov	rax,QWORD PTR[8+rcx]
	mul	r8
	add	r9,rax

	mov	rax,QWORD PTR[rcx]
	mul	rbx
	add	r9,rax

	mov	QWORD PTR[8+rdi],r9


	sub	rsi,192

	mov	r8,QWORD PTR[rcx]
	mov	r9,QWORD PTR[8+rcx]

	call	MULADD_128x512





	mov	rax,QWORD PTR[rsi]
	mov	rbx,QWORD PTR[8+rsi]
	mov	rdi,QWORD PTR[16+rsi]
	mov	rdx,QWORD PTR[24+rsi]


	mov	rbp,QWORD PTR[384+rsp]

	add	r8,QWORD PTR[64+rcx]
	adc	r9,QWORD PTR[72+rcx]


	adc	rbp,rbp



	shl	rbp,3
	mov	rcx,QWORD PTR[32+rsp]
	add	rbp,rcx


	xor	rsi,rsi

	add	r10,QWORD PTR[rbp]
	adc	r11,QWORD PTR[64+rbp]
	adc	r12,QWORD PTR[128+rbp]
	adc	r13,QWORD PTR[192+rbp]
	adc	r14,QWORD PTR[256+rbp]
	adc	r15,QWORD PTR[320+rbp]
	adc	r8,QWORD PTR[384+rbp]
	adc	r9,QWORD PTR[448+rbp]



	sbb	rsi,0


	and	rax,rsi
	and	rbx,rsi
	and	rdi,rsi
	and	rdx,rsi

	mov	rbp,1
	sub	r10,rax
	sbb	r11,rbx
	sbb	r12,rdi
	sbb	r13,rdx




	sbb	rbp,0



	add	rcx,512
	mov	rax,QWORD PTR[32+rcx]
	mov	rbx,QWORD PTR[40+rcx]
	mov	rdi,QWORD PTR[48+rcx]
	mov	rdx,QWORD PTR[56+rcx]



	and	rax,rsi
	and	rbx,rsi
	and	rdi,rsi
	and	rdx,rsi



	sub	rbp,1

	sbb	r14,rax
	sbb	r15,rbx
	sbb	r8,rdi
	sbb	r9,rdx



	mov	rsi,QWORD PTR[144+rsp]
	mov	QWORD PTR[rsi],r10
	mov	QWORD PTR[8+rsi],r11
	mov	QWORD PTR[16+rsi],r12
	mov	QWORD PTR[24+rsi],r13
	mov	QWORD PTR[32+rsi],r14
	mov	QWORD PTR[40+rsi],r15
	mov	QWORD PTR[48+rsi],r8
	mov	QWORD PTR[56+rsi],r9

	DB	0F3h,0C3h		;repret
mont_reduce	ENDP

ALIGN	16
mont_mul_a3b	PROC PRIVATE




	mov	rbp,QWORD PTR[rdi]

	mov	rax,r10
	mul	rbp
	mov	QWORD PTR[520+rsp],rax
	mov	r10,rdx
	mov	rax,r11
	mul	rbp
	add	r10,rax
	adc	rdx,0
	mov	r11,rdx
	mov	rax,r12
	mul	rbp
	add	r11,rax
	adc	rdx,0
	mov	r12,rdx
	mov	rax,r13
	mul	rbp
	add	r12,rax
	adc	rdx,0
	mov	r13,rdx
	mov	rax,r14
	mul	rbp
	add	r13,rax
	adc	rdx,0
	mov	r14,rdx
	mov	rax,r15
	mul	rbp
	add	r14,rax
	adc	rdx,0
	mov	r15,rdx
	mov	rax,r8
	mul	rbp
	add	r15,rax
	adc	rdx,0
	mov	r8,rdx
	mov	rax,r9
	mul	rbp
	add	r8,rax
	adc	rdx,0
	mov	r9,rdx
	mov	rbp,QWORD PTR[8+rdi]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	mov	QWORD PTR[528+rsp],r10
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	r10,rdx
	mov	rbp,QWORD PTR[16+rdi]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	mov	QWORD PTR[536+rsp],r11
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	r11,rdx
	mov	rbp,QWORD PTR[24+rdi]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	mov	QWORD PTR[544+rsp],r12
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	r12,rdx
	mov	rbp,QWORD PTR[32+rdi]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	mov	QWORD PTR[552+rsp],r13
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	r13,rdx
	mov	rbp,QWORD PTR[40+rdi]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	mov	QWORD PTR[560+rsp],r14
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	r14,rdx
	mov	rbp,QWORD PTR[48+rdi]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	mov	QWORD PTR[568+rsp],r15
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	add	r8,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	r15,rdx
	mov	rbp,QWORD PTR[56+rdi]
	mov	rax,QWORD PTR[rsi]
	mul	rbp
	add	r8,rax
	adc	rdx,0
	mov	QWORD PTR[576+rsp],r8
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rsi]
	mul	rbp
	add	r9,rax
	adc	rdx,0
	add	r9,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[16+rsi]
	mul	rbp
	add	r10,rax
	adc	rdx,0
	add	r10,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[24+rsi]
	mul	rbp
	add	r11,rax
	adc	rdx,0
	add	r11,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[32+rsi]
	mul	rbp
	add	r12,rax
	adc	rdx,0
	add	r12,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[40+rsi]
	mul	rbp
	add	r13,rax
	adc	rdx,0
	add	r13,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[48+rsi]
	mul	rbp
	add	r14,rax
	adc	rdx,0
	add	r14,rbx
	adc	rdx,0
	mov	rbx,rdx

	mov	rax,QWORD PTR[56+rsi]
	mul	rbp
	add	r15,rax
	adc	rdx,0
	add	r15,rbx
	adc	rdx,0
	mov	r8,rdx
	mov	QWORD PTR[584+rsp],r9
	mov	QWORD PTR[592+rsp],r10
	mov	QWORD PTR[600+rsp],r11
	mov	QWORD PTR[608+rsp],r12
	mov	QWORD PTR[616+rsp],r13
	mov	QWORD PTR[624+rsp],r14
	mov	QWORD PTR[632+rsp],r15
	mov	QWORD PTR[640+rsp],r8





	jmp	mont_reduce


mont_mul_a3b	ENDP

ALIGN	16
sqr_reduce	PROC PRIVATE
	mov	rcx,QWORD PTR[16+rsp]



	mov	rbx,r10

	mov	rax,r11
	mul	rbx
	mov	QWORD PTR[528+rsp],rax
	mov	r10,rdx
	mov	rax,r12
	mul	rbx
	add	r10,rax
	adc	rdx,0
	mov	r11,rdx
	mov	rax,r13
	mul	rbx
	add	r11,rax
	adc	rdx,0
	mov	r12,rdx
	mov	rax,r14
	mul	rbx
	add	r12,rax
	adc	rdx,0
	mov	r13,rdx
	mov	rax,r15
	mul	rbx
	add	r13,rax
	adc	rdx,0
	mov	r14,rdx
	mov	rax,r8
	mul	rbx
	add	r14,rax
	adc	rdx,0
	mov	r15,rdx
	mov	rax,r9
	mul	rbx
	add	r15,rax
	adc	rdx,0
	mov	rsi,rdx

	mov	QWORD PTR[536+rsp],r10





	mov	rbx,QWORD PTR[8+rcx]

	mov	rax,QWORD PTR[16+rcx]
	mul	rbx
	add	r11,rax
	adc	rdx,0
	mov	QWORD PTR[544+rsp],r11

	mov	r10,rdx
	mov	rax,QWORD PTR[24+rcx]
	mul	rbx
	add	r12,rax
	adc	rdx,0
	add	r12,r10
	adc	rdx,0
	mov	QWORD PTR[552+rsp],r12

	mov	r10,rdx
	mov	rax,QWORD PTR[32+rcx]
	mul	rbx
	add	r13,rax
	adc	rdx,0
	add	r13,r10
	adc	rdx,0

	mov	r10,rdx
	mov	rax,QWORD PTR[40+rcx]
	mul	rbx
	add	r14,rax
	adc	rdx,0
	add	r14,r10
	adc	rdx,0

	mov	r10,rdx
	mov	rax,r8
	mul	rbx
	add	r15,rax
	adc	rdx,0
	add	r15,r10
	adc	rdx,0

	mov	r10,rdx
	mov	rax,r9
	mul	rbx
	add	rsi,rax
	adc	rdx,0
	add	rsi,r10
	adc	rdx,0

	mov	r11,rdx




	mov	rbx,QWORD PTR[16+rcx]

	mov	rax,QWORD PTR[24+rcx]
	mul	rbx
	add	r13,rax
	adc	rdx,0
	mov	QWORD PTR[560+rsp],r13

	mov	r10,rdx
	mov	rax,QWORD PTR[32+rcx]
	mul	rbx
	add	r14,rax
	adc	rdx,0
	add	r14,r10
	adc	rdx,0
	mov	QWORD PTR[568+rsp],r14

	mov	r10,rdx
	mov	rax,QWORD PTR[40+rcx]
	mul	rbx
	add	r15,rax
	adc	rdx,0
	add	r15,r10
	adc	rdx,0

	mov	r10,rdx
	mov	rax,r8
	mul	rbx
	add	rsi,rax
	adc	rdx,0
	add	rsi,r10
	adc	rdx,0

	mov	r10,rdx
	mov	rax,r9
	mul	rbx
	add	r11,rax
	adc	rdx,0
	add	r11,r10
	adc	rdx,0

	mov	r12,rdx





	mov	rbx,QWORD PTR[24+rcx]

	mov	rax,QWORD PTR[32+rcx]
	mul	rbx
	add	r15,rax
	adc	rdx,0
	mov	QWORD PTR[576+rsp],r15

	mov	r10,rdx
	mov	rax,QWORD PTR[40+rcx]
	mul	rbx
	add	rsi,rax
	adc	rdx,0
	add	rsi,r10
	adc	rdx,0
	mov	QWORD PTR[584+rsp],rsi

	mov	r10,rdx
	mov	rax,r8
	mul	rbx
	add	r11,rax
	adc	rdx,0
	add	r11,r10
	adc	rdx,0

	mov	r10,rdx
	mov	rax,r9
	mul	rbx
	add	r12,rax
	adc	rdx,0
	add	r12,r10
	adc	rdx,0

	mov	r15,rdx




	mov	rbx,QWORD PTR[32+rcx]

	mov	rax,QWORD PTR[40+rcx]
	mul	rbx
	add	r11,rax
	adc	rdx,0
	mov	QWORD PTR[592+rsp],r11

	mov	r10,rdx
	mov	rax,r8
	mul	rbx
	add	r12,rax
	adc	rdx,0
	add	r12,r10
	adc	rdx,0
	mov	QWORD PTR[600+rsp],r12

	mov	r10,rdx
	mov	rax,r9
	mul	rbx
	add	r15,rax
	adc	rdx,0
	add	r15,r10
	adc	rdx,0

	mov	r11,rdx




	mov	rbx,QWORD PTR[40+rcx]

	mov	rax,r8
	mul	rbx
	add	r15,rax
	adc	rdx,0
	mov	QWORD PTR[608+rsp],r15

	mov	r10,rdx
	mov	rax,r9
	mul	rbx
	add	r11,rax
	adc	rdx,0
	add	r11,r10
	adc	rdx,0
	mov	QWORD PTR[616+rsp],r11

	mov	r12,rdx




	mov	rbx,r8

	mov	rax,r9
	mul	rbx
	add	r12,rax
	adc	rdx,0
	mov	QWORD PTR[624+rsp],r12

	mov	QWORD PTR[632+rsp],rdx


	mov	r10,QWORD PTR[528+rsp]
	mov	r11,QWORD PTR[536+rsp]
	mov	r12,QWORD PTR[544+rsp]
	mov	r13,QWORD PTR[552+rsp]
	mov	r14,QWORD PTR[560+rsp]
	mov	r15,QWORD PTR[568+rsp]

	mov	rax,QWORD PTR[24+rcx]
	mul	rax
	mov	rdi,rax
	mov	r8,rdx

	add	r10,r10
	adc	r11,r11
	adc	r12,r12
	adc	r13,r13
	adc	r14,r14
	adc	r15,r15
	adc	r8,0

	mov	rax,QWORD PTR[rcx]
	mul	rax
	mov	QWORD PTR[520+rsp],rax
	mov	rbx,rdx

	mov	rax,QWORD PTR[8+rcx]
	mul	rax

	add	r10,rbx
	adc	r11,rax
	adc	rdx,0

	mov	rbx,rdx
	mov	QWORD PTR[528+rsp],r10
	mov	QWORD PTR[536+rsp],r11

	mov	rax,QWORD PTR[16+rcx]
	mul	rax

	add	r12,rbx
	adc	r13,rax
	adc	rdx,0

	mov	rbx,rdx

	mov	QWORD PTR[544+rsp],r12
	mov	QWORD PTR[552+rsp],r13

	xor	rbp,rbp
	add	r14,rbx
	adc	r15,rdi
	adc	rbp,0

	mov	QWORD PTR[560+rsp],r14
	mov	QWORD PTR[568+rsp],r15




	mov	r10,QWORD PTR[576+rsp]
	mov	r11,QWORD PTR[584+rsp]
	mov	r12,QWORD PTR[592+rsp]
	mov	r13,QWORD PTR[600+rsp]
	mov	r14,QWORD PTR[608+rsp]
	mov	r15,QWORD PTR[616+rsp]
	mov	rdi,QWORD PTR[624+rsp]
	mov	rsi,QWORD PTR[632+rsp]

	mov	rax,r9
	mul	rax
	mov	r9,rax
	mov	rbx,rdx

	add	r10,r10
	adc	r11,r11
	adc	r12,r12
	adc	r13,r13
	adc	r14,r14
	adc	r15,r15
	adc	rdi,rdi
	adc	rsi,rsi
	adc	rbx,0

	add	r10,rbp

	mov	rax,QWORD PTR[32+rcx]
	mul	rax

	add	r10,r8
	adc	r11,rax
	adc	rdx,0

	mov	rbp,rdx

	mov	QWORD PTR[576+rsp],r10
	mov	QWORD PTR[584+rsp],r11

	mov	rax,QWORD PTR[40+rcx]
	mul	rax

	add	r12,rbp
	adc	r13,rax
	adc	rdx,0

	mov	rbp,rdx

	mov	QWORD PTR[592+rsp],r12
	mov	QWORD PTR[600+rsp],r13

	mov	rax,QWORD PTR[48+rcx]
	mul	rax

	add	r14,rbp
	adc	r15,rax
	adc	rdx,0

	mov	QWORD PTR[608+rsp],r14
	mov	QWORD PTR[616+rsp],r15

	add	rdi,rdx
	adc	rsi,r9
	adc	rbx,0

	mov	QWORD PTR[624+rsp],rdi
	mov	QWORD PTR[632+rsp],rsi
	mov	QWORD PTR[640+rsp],rbx

	jmp	mont_reduce


sqr_reduce	ENDP
PUBLIC	mod_exp_512

mod_exp_512	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_mod_exp_512::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbp
	push	rbx
	push	r12
	push	r13
	push	r14
	push	r15


	mov	r8,rsp
	sub	rsp,2688
	and	rsp,-64


	mov	QWORD PTR[rsp],r8
	mov	QWORD PTR[8+rsp],rdi
	mov	QWORD PTR[16+rsp],rsi
	mov	QWORD PTR[24+rsp],rcx
$L$body::



	pxor	xmm4,xmm4
	movdqu	xmm0,XMMWORD PTR[rsi]
	movdqu	xmm1,XMMWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[32+rsi]
	movdqu	xmm3,XMMWORD PTR[48+rsi]
	movdqa	XMMWORD PTR[512+rsp],xmm4
	movdqa	XMMWORD PTR[528+rsp],xmm4
	movdqa	XMMWORD PTR[608+rsp],xmm4
	movdqa	XMMWORD PTR[624+rsp],xmm4
	movdqa	XMMWORD PTR[544+rsp],xmm0
	movdqa	XMMWORD PTR[560+rsp],xmm1
	movdqa	XMMWORD PTR[576+rsp],xmm2
	movdqa	XMMWORD PTR[592+rsp],xmm3


	movdqu	xmm0,XMMWORD PTR[rdx]
	movdqu	xmm1,XMMWORD PTR[16+rdx]
	movdqu	xmm2,XMMWORD PTR[32+rdx]
	movdqu	xmm3,XMMWORD PTR[48+rdx]

	lea	rbx,QWORD PTR[384+rsp]
	mov	QWORD PTR[136+rsp],rbx
	call	mont_reduce


	lea	rcx,QWORD PTR[448+rsp]
	xor	rax,rax
	mov	QWORD PTR[rcx],rax
	mov	QWORD PTR[8+rcx],rax
	mov	QWORD PTR[24+rcx],rax
	mov	QWORD PTR[32+rcx],rax
	mov	QWORD PTR[40+rcx],rax
	mov	QWORD PTR[48+rcx],rax
	mov	QWORD PTR[56+rcx],rax
	mov	QWORD PTR[128+rsp],rax
	mov	QWORD PTR[16+rcx],1

	lea	rbp,QWORD PTR[640+rsp]
	mov	rsi,rcx
	mov	rdi,rbp
	mov	rax,8
loop_0::
	mov	rbx,QWORD PTR[rcx]
	mov	WORD PTR[rdi],bx
	shr	rbx,16
	mov	WORD PTR[64+rdi],bx
	shr	rbx,16
	mov	WORD PTR[128+rdi],bx
	shr	rbx,16
	mov	WORD PTR[192+rdi],bx
	lea	rcx,QWORD PTR[8+rcx]
	lea	rdi,QWORD PTR[256+rdi]
	dec	rax
	jnz	loop_0
	mov	rax,31
	mov	QWORD PTR[32+rsp],rax
	mov	QWORD PTR[40+rsp],rbp

	mov	QWORD PTR[136+rsp],rsi
	mov	r10,QWORD PTR[rsi]
	mov	r11,QWORD PTR[8+rsi]
	mov	r12,QWORD PTR[16+rsi]
	mov	r13,QWORD PTR[24+rsi]
	mov	r14,QWORD PTR[32+rsi]
	mov	r15,QWORD PTR[40+rsi]
	mov	r8,QWORD PTR[48+rsi]
	mov	r9,QWORD PTR[56+rsi]
init_loop::
	lea	rdi,QWORD PTR[384+rsp]
	call	mont_mul_a3b
	lea	rsi,QWORD PTR[448+rsp]
	mov	rbp,QWORD PTR[40+rsp]
	add	rbp,2
	mov	QWORD PTR[40+rsp],rbp
	mov	rcx,rsi
	mov	rax,8
loop_1::
	mov	rbx,QWORD PTR[rcx]
	mov	WORD PTR[rbp],bx
	shr	rbx,16
	mov	WORD PTR[64+rbp],bx
	shr	rbx,16
	mov	WORD PTR[128+rbp],bx
	shr	rbx,16
	mov	WORD PTR[192+rbp],bx
	lea	rcx,QWORD PTR[8+rcx]
	lea	rbp,QWORD PTR[256+rbp]
	dec	rax
	jnz	loop_1
	mov	rax,QWORD PTR[32+rsp]
	sub	rax,1
	mov	QWORD PTR[32+rsp],rax
	jne	init_loop



	movdqa	XMMWORD PTR[64+rsp],xmm0
	movdqa	XMMWORD PTR[80+rsp],xmm1
	movdqa	XMMWORD PTR[96+rsp],xmm2
	movdqa	XMMWORD PTR[112+rsp],xmm3





	mov	eax,DWORD PTR[126+rsp]
	mov	rdx,rax
	shr	rax,11
	and	edx,007FFh
	mov	DWORD PTR[126+rsp],edx
	lea	rsi,QWORD PTR[640+rax*2+rsp]
	mov	rdx,QWORD PTR[8+rsp]
	mov	rbp,4
loop_2::
	movzx	rbx,WORD PTR[192+rsi]
	movzx	rax,WORD PTR[448+rsi]
	shl	rbx,16
	shl	rax,16
	mov	bx,WORD PTR[128+rsi]
	mov	ax,WORD PTR[384+rsi]
	shl	rbx,16
	shl	rax,16
	mov	bx,WORD PTR[64+rsi]
	mov	ax,WORD PTR[320+rsi]
	shl	rbx,16
	shl	rax,16
	mov	bx,WORD PTR[rsi]
	mov	ax,WORD PTR[256+rsi]
	mov	QWORD PTR[rdx],rbx
	mov	QWORD PTR[8+rdx],rax
	lea	rsi,QWORD PTR[512+rsi]
	lea	rdx,QWORD PTR[16+rdx]
	sub	rbp,1
	jnz	loop_2
	mov	QWORD PTR[48+rsp],505

	mov	rcx,QWORD PTR[8+rsp]
	mov	QWORD PTR[136+rsp],rcx
	mov	r10,QWORD PTR[rcx]
	mov	r11,QWORD PTR[8+rcx]
	mov	r12,QWORD PTR[16+rcx]
	mov	r13,QWORD PTR[24+rcx]
	mov	r14,QWORD PTR[32+rcx]
	mov	r15,QWORD PTR[40+rcx]
	mov	r8,QWORD PTR[48+rcx]
	mov	r9,QWORD PTR[56+rcx]
	jmp	sqr_2

main_loop_a3b::
	call	sqr_reduce
	call	sqr_reduce
	call	sqr_reduce
sqr_2::
	call	sqr_reduce
	call	sqr_reduce



	mov	rcx,QWORD PTR[48+rsp]
	mov	rax,rcx
	shr	rax,4
	mov	edx,DWORD PTR[64+rax*2+rsp]
	and	rcx,15
	shr	rdx,cl
	and	rdx,01Fh

	lea	rsi,QWORD PTR[640+rdx*2+rsp]
	lea	rdx,QWORD PTR[448+rsp]
	mov	rdi,rdx
	mov	rbp,4
loop_3::
	movzx	rbx,WORD PTR[192+rsi]
	movzx	rax,WORD PTR[448+rsi]
	shl	rbx,16
	shl	rax,16
	mov	bx,WORD PTR[128+rsi]
	mov	ax,WORD PTR[384+rsi]
	shl	rbx,16
	shl	rax,16
	mov	bx,WORD PTR[64+rsi]
	mov	ax,WORD PTR[320+rsi]
	shl	rbx,16
	shl	rax,16
	mov	bx,WORD PTR[rsi]
	mov	ax,WORD PTR[256+rsi]
	mov	QWORD PTR[rdx],rbx
	mov	QWORD PTR[8+rdx],rax
	lea	rsi,QWORD PTR[512+rsi]
	lea	rdx,QWORD PTR[16+rdx]
	sub	rbp,1
	jnz	loop_3
	mov	rsi,QWORD PTR[8+rsp]
	call	mont_mul_a3b



	mov	rcx,QWORD PTR[48+rsp]
	sub	rcx,5
	mov	QWORD PTR[48+rsp],rcx
	jge	main_loop_a3b



end_main_loop_a3b::


	mov	rdx,QWORD PTR[8+rsp]
	pxor	xmm4,xmm4
	movdqu	xmm0,XMMWORD PTR[rdx]
	movdqu	xmm1,XMMWORD PTR[16+rdx]
	movdqu	xmm2,XMMWORD PTR[32+rdx]
	movdqu	xmm3,XMMWORD PTR[48+rdx]
	movdqa	XMMWORD PTR[576+rsp],xmm4
	movdqa	XMMWORD PTR[592+rsp],xmm4
	movdqa	XMMWORD PTR[608+rsp],xmm4
	movdqa	XMMWORD PTR[624+rsp],xmm4
	movdqa	XMMWORD PTR[512+rsp],xmm0
	movdqa	XMMWORD PTR[528+rsp],xmm1
	movdqa	XMMWORD PTR[544+rsp],xmm2
	movdqa	XMMWORD PTR[560+rsp],xmm3
	call	mont_reduce



	mov	rax,QWORD PTR[8+rsp]
	mov	r8,QWORD PTR[rax]
	mov	r9,QWORD PTR[8+rax]
	mov	r10,QWORD PTR[16+rax]
	mov	r11,QWORD PTR[24+rax]
	mov	r12,QWORD PTR[32+rax]
	mov	r13,QWORD PTR[40+rax]
	mov	r14,QWORD PTR[48+rax]
	mov	r15,QWORD PTR[56+rax]


	mov	rbx,QWORD PTR[24+rsp]
	add	rbx,512

	sub	r8,QWORD PTR[rbx]
	sbb	r9,QWORD PTR[8+rbx]
	sbb	r10,QWORD PTR[16+rbx]
	sbb	r11,QWORD PTR[24+rbx]
	sbb	r12,QWORD PTR[32+rbx]
	sbb	r13,QWORD PTR[40+rbx]
	sbb	r14,QWORD PTR[48+rbx]
	sbb	r15,QWORD PTR[56+rbx]


	mov	rsi,QWORD PTR[rax]
	mov	rdi,QWORD PTR[8+rax]
	mov	rcx,QWORD PTR[16+rax]
	mov	rdx,QWORD PTR[24+rax]
	cmovnc	rsi,r8
	cmovnc	rdi,r9
	cmovnc	rcx,r10
	cmovnc	rdx,r11
	mov	QWORD PTR[rax],rsi
	mov	QWORD PTR[8+rax],rdi
	mov	QWORD PTR[16+rax],rcx
	mov	QWORD PTR[24+rax],rdx

	mov	rsi,QWORD PTR[32+rax]
	mov	rdi,QWORD PTR[40+rax]
	mov	rcx,QWORD PTR[48+rax]
	mov	rdx,QWORD PTR[56+rax]
	cmovnc	rsi,r12
	cmovnc	rdi,r13
	cmovnc	rcx,r14
	cmovnc	rdx,r15
	mov	QWORD PTR[32+rax],rsi
	mov	QWORD PTR[40+rax],rdi
	mov	QWORD PTR[48+rax],rcx
	mov	QWORD PTR[56+rax],rdx

	mov	rsi,QWORD PTR[rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbx,QWORD PTR[32+rsi]
	mov	rbp,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_mod_exp_512::
mod_exp_512	ENDP
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
mod_exp_512_se_handler	PROC PRIVATE
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

	lea	r10,QWORD PTR[$L$body]
	cmp	rbx,r10
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$epilogue]
	cmp	rbx,r10
	jae	$L$in_prologue

	mov	rax,QWORD PTR[rax]

	mov	rbx,QWORD PTR[32+rax]
	mov	rbp,QWORD PTR[40+rax]
	mov	r12,QWORD PTR[24+rax]
	mov	r13,QWORD PTR[16+rax]
	mov	r14,QWORD PTR[8+rax]
	mov	r15,QWORD PTR[rax]
	lea	rax,QWORD PTR[48+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[232+r8],r14
	mov	QWORD PTR[240+r8],r15

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
mod_exp_512_se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_mod_exp_512
	DD	imagerel $L$SEH_end_mod_exp_512
	DD	imagerel $L$SEH_info_mod_exp_512

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_mod_exp_512::
DB	9,0,0,0
	DD	imagerel mod_exp_512_se_handler

.xdata	ENDS
END
