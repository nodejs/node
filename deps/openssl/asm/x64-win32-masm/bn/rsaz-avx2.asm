OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

PUBLIC	rsaz_1024_sqr_avx2

ALIGN	64
rsaz_1024_sqr_avx2	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_rsaz_1024_sqr_avx2::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]


	lea	rax,QWORD PTR[rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	vzeroupper
	lea	rsp,QWORD PTR[((-168))+rsp]
	vmovaps	XMMWORD PTR[(-216)+rax],xmm6
	vmovaps	XMMWORD PTR[(-200)+rax],xmm7
	vmovaps	XMMWORD PTR[(-184)+rax],xmm8
	vmovaps	XMMWORD PTR[(-168)+rax],xmm9
	vmovaps	XMMWORD PTR[(-152)+rax],xmm10
	vmovaps	XMMWORD PTR[(-136)+rax],xmm11
	vmovaps	XMMWORD PTR[(-120)+rax],xmm12
	vmovaps	XMMWORD PTR[(-104)+rax],xmm13
	vmovaps	XMMWORD PTR[(-88)+rax],xmm14
	vmovaps	XMMWORD PTR[(-72)+rax],xmm15
$L$sqr_1024_body::
	mov	rbp,rax
	mov	r13,rdx
	sub	rsp,832
	mov	r15,r13
	sub	rdi,-128
	sub	rsi,-128
	sub	r13,-128

	and	r15,4095
	add	r15,32*10
	shr	r15,12
	vpxor	ymm9,ymm9,ymm9
	jz	$L$sqr_1024_no_n_copy





	sub	rsp,32*10
	vmovdqu	ymm0,YMMWORD PTR[((0-128))+r13]
	and	rsp,-2048
	vmovdqu	ymm1,YMMWORD PTR[((32-128))+r13]
	vmovdqu	ymm2,YMMWORD PTR[((64-128))+r13]
	vmovdqu	ymm3,YMMWORD PTR[((96-128))+r13]
	vmovdqu	ymm4,YMMWORD PTR[((128-128))+r13]
	vmovdqu	ymm5,YMMWORD PTR[((160-128))+r13]
	vmovdqu	ymm6,YMMWORD PTR[((192-128))+r13]
	vmovdqu	ymm7,YMMWORD PTR[((224-128))+r13]
	vmovdqu	ymm8,YMMWORD PTR[((256-128))+r13]
	lea	r13,QWORD PTR[((832+128))+rsp]
	vmovdqu	YMMWORD PTR[(0-128)+r13],ymm0
	vmovdqu	YMMWORD PTR[(32-128)+r13],ymm1
	vmovdqu	YMMWORD PTR[(64-128)+r13],ymm2
	vmovdqu	YMMWORD PTR[(96-128)+r13],ymm3
	vmovdqu	YMMWORD PTR[(128-128)+r13],ymm4
	vmovdqu	YMMWORD PTR[(160-128)+r13],ymm5
	vmovdqu	YMMWORD PTR[(192-128)+r13],ymm6
	vmovdqu	YMMWORD PTR[(224-128)+r13],ymm7
	vmovdqu	YMMWORD PTR[(256-128)+r13],ymm8
	vmovdqu	YMMWORD PTR[(288-128)+r13],ymm9

$L$sqr_1024_no_n_copy::
	and	rsp,-1024

	vmovdqu	ymm1,YMMWORD PTR[((32-128))+rsi]
	vmovdqu	ymm2,YMMWORD PTR[((64-128))+rsi]
	vmovdqu	ymm3,YMMWORD PTR[((96-128))+rsi]
	vmovdqu	ymm4,YMMWORD PTR[((128-128))+rsi]
	vmovdqu	ymm5,YMMWORD PTR[((160-128))+rsi]
	vmovdqu	ymm6,YMMWORD PTR[((192-128))+rsi]
	vmovdqu	ymm7,YMMWORD PTR[((224-128))+rsi]
	vmovdqu	ymm8,YMMWORD PTR[((256-128))+rsi]

	lea	rbx,QWORD PTR[192+rsp]
	vmovdqu	ymm15,YMMWORD PTR[$L$and_mask]
	jmp	$L$OOP_GRANDE_SQR_1024

ALIGN	32
$L$OOP_GRANDE_SQR_1024::
	lea	r9,QWORD PTR[((576+128))+rsp]
	lea	r12,QWORD PTR[448+rsp]




	vpaddq	ymm1,ymm1,ymm1
	vpbroadcastq	ymm10,QWORD PTR[((0-128))+rsi]
	vpaddq	ymm2,ymm2,ymm2
	vmovdqa	YMMWORD PTR[(0-128)+r9],ymm1
	vpaddq	ymm3,ymm3,ymm3
	vmovdqa	YMMWORD PTR[(32-128)+r9],ymm2
	vpaddq	ymm4,ymm4,ymm4
	vmovdqa	YMMWORD PTR[(64-128)+r9],ymm3
	vpaddq	ymm5,ymm5,ymm5
	vmovdqa	YMMWORD PTR[(96-128)+r9],ymm4
	vpaddq	ymm6,ymm6,ymm6
	vmovdqa	YMMWORD PTR[(128-128)+r9],ymm5
	vpaddq	ymm7,ymm7,ymm7
	vmovdqa	YMMWORD PTR[(160-128)+r9],ymm6
	vpaddq	ymm8,ymm8,ymm8
	vmovdqa	YMMWORD PTR[(192-128)+r9],ymm7
	vpxor	ymm9,ymm9,ymm9
	vmovdqa	YMMWORD PTR[(224-128)+r9],ymm8

	vpmuludq	ymm0,ymm10,YMMWORD PTR[((0-128))+rsi]
	vpbroadcastq	ymm11,QWORD PTR[((32-128))+rsi]
	vmovdqu	YMMWORD PTR[(288-192)+rbx],ymm9
	vpmuludq	ymm1,ymm1,ymm10
	vmovdqu	YMMWORD PTR[(320-448)+r12],ymm9
	vpmuludq	ymm2,ymm2,ymm10
	vmovdqu	YMMWORD PTR[(352-448)+r12],ymm9
	vpmuludq	ymm3,ymm3,ymm10
	vmovdqu	YMMWORD PTR[(384-448)+r12],ymm9
	vpmuludq	ymm4,ymm4,ymm10
	vmovdqu	YMMWORD PTR[(416-448)+r12],ymm9
	vpmuludq	ymm5,ymm5,ymm10
	vmovdqu	YMMWORD PTR[(448-448)+r12],ymm9
	vpmuludq	ymm6,ymm6,ymm10
	vmovdqu	YMMWORD PTR[(480-448)+r12],ymm9
	vpmuludq	ymm7,ymm7,ymm10
	vmovdqu	YMMWORD PTR[(512-448)+r12],ymm9
	vpmuludq	ymm8,ymm8,ymm10
	vpbroadcastq	ymm10,QWORD PTR[((64-128))+rsi]
	vmovdqu	YMMWORD PTR[(544-448)+r12],ymm9

	mov	r15,rsi
	mov	r14d,4
	jmp	$L$sqr_entry_1024
ALIGN	32
$L$OOP_SQR_1024::
	vpbroadcastq	ymm11,QWORD PTR[((32-128))+r15]
	vpmuludq	ymm0,ymm10,YMMWORD PTR[((0-128))+rsi]
	vpaddq	ymm0,ymm0,YMMWORD PTR[((0-192))+rbx]
	vpmuludq	ymm1,ymm10,YMMWORD PTR[((0-128))+r9]
	vpaddq	ymm1,ymm1,YMMWORD PTR[((32-192))+rbx]
	vpmuludq	ymm2,ymm10,YMMWORD PTR[((32-128))+r9]
	vpaddq	ymm2,ymm2,YMMWORD PTR[((64-192))+rbx]
	vpmuludq	ymm3,ymm10,YMMWORD PTR[((64-128))+r9]
	vpaddq	ymm3,ymm3,YMMWORD PTR[((96-192))+rbx]
	vpmuludq	ymm4,ymm10,YMMWORD PTR[((96-128))+r9]
	vpaddq	ymm4,ymm4,YMMWORD PTR[((128-192))+rbx]
	vpmuludq	ymm5,ymm10,YMMWORD PTR[((128-128))+r9]
	vpaddq	ymm5,ymm5,YMMWORD PTR[((160-192))+rbx]
	vpmuludq	ymm6,ymm10,YMMWORD PTR[((160-128))+r9]
	vpaddq	ymm6,ymm6,YMMWORD PTR[((192-192))+rbx]
	vpmuludq	ymm7,ymm10,YMMWORD PTR[((192-128))+r9]
	vpaddq	ymm7,ymm7,YMMWORD PTR[((224-192))+rbx]
	vpmuludq	ymm8,ymm10,YMMWORD PTR[((224-128))+r9]
	vpbroadcastq	ymm10,QWORD PTR[((64-128))+r15]
	vpaddq	ymm8,ymm8,YMMWORD PTR[((256-192))+rbx]
$L$sqr_entry_1024::
	vmovdqu	YMMWORD PTR[(0-192)+rbx],ymm0
	vmovdqu	YMMWORD PTR[(32-192)+rbx],ymm1

	vpmuludq	ymm12,ymm11,YMMWORD PTR[((32-128))+rsi]
	vpaddq	ymm2,ymm2,ymm12
	vpmuludq	ymm14,ymm11,YMMWORD PTR[((32-128))+r9]
	vpaddq	ymm3,ymm3,ymm14
	vpmuludq	ymm13,ymm11,YMMWORD PTR[((64-128))+r9]
	vpaddq	ymm4,ymm4,ymm13
	vpmuludq	ymm12,ymm11,YMMWORD PTR[((96-128))+r9]
	vpaddq	ymm5,ymm5,ymm12
	vpmuludq	ymm14,ymm11,YMMWORD PTR[((128-128))+r9]
	vpaddq	ymm6,ymm6,ymm14
	vpmuludq	ymm13,ymm11,YMMWORD PTR[((160-128))+r9]
	vpaddq	ymm7,ymm7,ymm13
	vpmuludq	ymm12,ymm11,YMMWORD PTR[((192-128))+r9]
	vpaddq	ymm8,ymm8,ymm12
	vpmuludq	ymm0,ymm11,YMMWORD PTR[((224-128))+r9]
	vpbroadcastq	ymm11,QWORD PTR[((96-128))+r15]
	vpaddq	ymm0,ymm0,YMMWORD PTR[((288-192))+rbx]

	vmovdqu	YMMWORD PTR[(64-192)+rbx],ymm2
	vmovdqu	YMMWORD PTR[(96-192)+rbx],ymm3

	vpmuludq	ymm13,ymm10,YMMWORD PTR[((64-128))+rsi]
	vpaddq	ymm4,ymm4,ymm13
	vpmuludq	ymm12,ymm10,YMMWORD PTR[((64-128))+r9]
	vpaddq	ymm5,ymm5,ymm12
	vpmuludq	ymm14,ymm10,YMMWORD PTR[((96-128))+r9]
	vpaddq	ymm6,ymm6,ymm14
	vpmuludq	ymm13,ymm10,YMMWORD PTR[((128-128))+r9]
	vpaddq	ymm7,ymm7,ymm13
	vpmuludq	ymm12,ymm10,YMMWORD PTR[((160-128))+r9]
	vpaddq	ymm8,ymm8,ymm12
	vpmuludq	ymm14,ymm10,YMMWORD PTR[((192-128))+r9]
	vpaddq	ymm0,ymm0,ymm14
	vpmuludq	ymm1,ymm10,YMMWORD PTR[((224-128))+r9]
	vpbroadcastq	ymm10,QWORD PTR[((128-128))+r15]
	vpaddq	ymm1,ymm1,YMMWORD PTR[((320-448))+r12]

	vmovdqu	YMMWORD PTR[(128-192)+rbx],ymm4
	vmovdqu	YMMWORD PTR[(160-192)+rbx],ymm5

	vpmuludq	ymm12,ymm11,YMMWORD PTR[((96-128))+rsi]
	vpaddq	ymm6,ymm6,ymm12
	vpmuludq	ymm14,ymm11,YMMWORD PTR[((96-128))+r9]
	vpaddq	ymm7,ymm7,ymm14
	vpmuludq	ymm13,ymm11,YMMWORD PTR[((128-128))+r9]
	vpaddq	ymm8,ymm8,ymm13
	vpmuludq	ymm12,ymm11,YMMWORD PTR[((160-128))+r9]
	vpaddq	ymm0,ymm0,ymm12
	vpmuludq	ymm14,ymm11,YMMWORD PTR[((192-128))+r9]
	vpaddq	ymm1,ymm1,ymm14
	vpmuludq	ymm2,ymm11,YMMWORD PTR[((224-128))+r9]
	vpbroadcastq	ymm11,QWORD PTR[((160-128))+r15]
	vpaddq	ymm2,ymm2,YMMWORD PTR[((352-448))+r12]

	vmovdqu	YMMWORD PTR[(192-192)+rbx],ymm6
	vmovdqu	YMMWORD PTR[(224-192)+rbx],ymm7

	vpmuludq	ymm12,ymm10,YMMWORD PTR[((128-128))+rsi]
	vpaddq	ymm8,ymm8,ymm12
	vpmuludq	ymm14,ymm10,YMMWORD PTR[((128-128))+r9]
	vpaddq	ymm0,ymm0,ymm14
	vpmuludq	ymm13,ymm10,YMMWORD PTR[((160-128))+r9]
	vpaddq	ymm1,ymm1,ymm13
	vpmuludq	ymm12,ymm10,YMMWORD PTR[((192-128))+r9]
	vpaddq	ymm2,ymm2,ymm12
	vpmuludq	ymm3,ymm10,YMMWORD PTR[((224-128))+r9]
	vpbroadcastq	ymm10,QWORD PTR[((192-128))+r15]
	vpaddq	ymm3,ymm3,YMMWORD PTR[((384-448))+r12]

	vmovdqu	YMMWORD PTR[(256-192)+rbx],ymm8
	vmovdqu	YMMWORD PTR[(288-192)+rbx],ymm0
	lea	rbx,QWORD PTR[8+rbx]

	vpmuludq	ymm13,ymm11,YMMWORD PTR[((160-128))+rsi]
	vpaddq	ymm1,ymm1,ymm13
	vpmuludq	ymm12,ymm11,YMMWORD PTR[((160-128))+r9]
	vpaddq	ymm2,ymm2,ymm12
	vpmuludq	ymm14,ymm11,YMMWORD PTR[((192-128))+r9]
	vpaddq	ymm3,ymm3,ymm14
	vpmuludq	ymm4,ymm11,YMMWORD PTR[((224-128))+r9]
	vpbroadcastq	ymm11,QWORD PTR[((224-128))+r15]
	vpaddq	ymm4,ymm4,YMMWORD PTR[((416-448))+r12]

	vmovdqu	YMMWORD PTR[(320-448)+r12],ymm1
	vmovdqu	YMMWORD PTR[(352-448)+r12],ymm2

	vpmuludq	ymm12,ymm10,YMMWORD PTR[((192-128))+rsi]
	vpaddq	ymm3,ymm3,ymm12
	vpmuludq	ymm14,ymm10,YMMWORD PTR[((192-128))+r9]
	vpbroadcastq	ymm0,QWORD PTR[((256-128))+r15]
	vpaddq	ymm4,ymm4,ymm14
	vpmuludq	ymm5,ymm10,YMMWORD PTR[((224-128))+r9]
	vpbroadcastq	ymm10,QWORD PTR[((0+8-128))+r15]
	vpaddq	ymm5,ymm5,YMMWORD PTR[((448-448))+r12]

	vmovdqu	YMMWORD PTR[(384-448)+r12],ymm3
	vmovdqu	YMMWORD PTR[(416-448)+r12],ymm4
	lea	r15,QWORD PTR[8+r15]

	vpmuludq	ymm12,ymm11,YMMWORD PTR[((224-128))+rsi]
	vpaddq	ymm5,ymm5,ymm12
	vpmuludq	ymm6,ymm11,YMMWORD PTR[((224-128))+r9]
	vpaddq	ymm6,ymm6,YMMWORD PTR[((480-448))+r12]

	vpmuludq	ymm7,ymm0,YMMWORD PTR[((256-128))+rsi]
	vmovdqu	YMMWORD PTR[(448-448)+r12],ymm5
	vpaddq	ymm7,ymm7,YMMWORD PTR[((512-448))+r12]
	vmovdqu	YMMWORD PTR[(480-448)+r12],ymm6
	vmovdqu	YMMWORD PTR[(512-448)+r12],ymm7
	lea	r12,QWORD PTR[8+r12]

	dec	r14d
	jnz	$L$OOP_SQR_1024

	vmovdqu	ymm8,YMMWORD PTR[256+rsp]
	vmovdqu	ymm1,YMMWORD PTR[288+rsp]
	vmovdqu	ymm2,YMMWORD PTR[320+rsp]
	lea	rbx,QWORD PTR[192+rsp]

	vpsrlq	ymm14,ymm8,29
	vpand	ymm8,ymm8,ymm15
	vpsrlq	ymm11,ymm1,29
	vpand	ymm1,ymm1,ymm15

	vpermq	ymm14,ymm14,093h
	vpxor	ymm9,ymm9,ymm9
	vpermq	ymm11,ymm11,093h

	vpblendd	ymm10,ymm14,ymm9,3
	vpblendd	ymm14,ymm11,ymm14,3
	vpaddq	ymm8,ymm8,ymm10
	vpblendd	ymm11,ymm9,ymm11,3
	vpaddq	ymm1,ymm1,ymm14
	vpaddq	ymm2,ymm2,ymm11
	vmovdqu	YMMWORD PTR[(288-192)+rbx],ymm1
	vmovdqu	YMMWORD PTR[(320-192)+rbx],ymm2

	mov	rax,QWORD PTR[rsp]
	mov	r10,QWORD PTR[8+rsp]
	mov	r11,QWORD PTR[16+rsp]
	mov	r12,QWORD PTR[24+rsp]
	vmovdqu	ymm1,YMMWORD PTR[32+rsp]
	vmovdqu	ymm2,YMMWORD PTR[((64-192))+rbx]
	vmovdqu	ymm3,YMMWORD PTR[((96-192))+rbx]
	vmovdqu	ymm4,YMMWORD PTR[((128-192))+rbx]
	vmovdqu	ymm5,YMMWORD PTR[((160-192))+rbx]
	vmovdqu	ymm6,YMMWORD PTR[((192-192))+rbx]
	vmovdqu	ymm7,YMMWORD PTR[((224-192))+rbx]

	mov	r9,rax
	imul	eax,ecx
	and	eax,01fffffffh
	vmovd	xmm12,eax

	mov	rdx,rax
	imul	rax,QWORD PTR[((-128))+r13]
	vpbroadcastq	ymm12,xmm12
	add	r9,rax
	mov	rax,rdx
	imul	rax,QWORD PTR[((8-128))+r13]
	shr	r9,29
	add	r10,rax
	mov	rax,rdx
	imul	rax,QWORD PTR[((16-128))+r13]
	add	r10,r9
	add	r11,rax
	imul	rdx,QWORD PTR[((24-128))+r13]
	add	r12,rdx

	mov	rax,r10
	imul	eax,ecx
	and	eax,01fffffffh

	mov	r14d,9
	jmp	$L$OOP_REDUCE_1024

ALIGN	32
$L$OOP_REDUCE_1024::
	vmovd	xmm13,eax
	vpbroadcastq	ymm13,xmm13

	vpmuludq	ymm10,ymm12,YMMWORD PTR[((32-128))+r13]
	mov	rdx,rax
	imul	rax,QWORD PTR[((-128))+r13]
	vpaddq	ymm1,ymm1,ymm10
	add	r10,rax
	vpmuludq	ymm14,ymm12,YMMWORD PTR[((64-128))+r13]
	mov	rax,rdx
	imul	rax,QWORD PTR[((8-128))+r13]
	vpaddq	ymm2,ymm2,ymm14
	vpmuludq	ymm11,ymm12,YMMWORD PTR[((96-128))+r13]
DB	067h
	add	r11,rax
DB	067h
	mov	rax,rdx
	imul	rax,QWORD PTR[((16-128))+r13]
	shr	r10,29
	vpaddq	ymm3,ymm3,ymm11
	vpmuludq	ymm10,ymm12,YMMWORD PTR[((128-128))+r13]
	add	r12,rax
	add	r11,r10
	vpaddq	ymm4,ymm4,ymm10
	vpmuludq	ymm14,ymm12,YMMWORD PTR[((160-128))+r13]
	mov	rax,r11
	imul	eax,ecx
	vpaddq	ymm5,ymm5,ymm14
	vpmuludq	ymm11,ymm12,YMMWORD PTR[((192-128))+r13]
	and	eax,01fffffffh
	vpaddq	ymm6,ymm6,ymm11
	vpmuludq	ymm10,ymm12,YMMWORD PTR[((224-128))+r13]
	vpaddq	ymm7,ymm7,ymm10
	vpmuludq	ymm14,ymm12,YMMWORD PTR[((256-128))+r13]
	vmovd	xmm12,eax

	vpaddq	ymm8,ymm8,ymm14

	vpbroadcastq	ymm12,xmm12

	vpmuludq	ymm11,ymm13,YMMWORD PTR[((32-8-128))+r13]
	vmovdqu	ymm14,YMMWORD PTR[((96-8-128))+r13]
	mov	rdx,rax
	imul	rax,QWORD PTR[((-128))+r13]
	vpaddq	ymm1,ymm1,ymm11
	vpmuludq	ymm10,ymm13,YMMWORD PTR[((64-8-128))+r13]
	vmovdqu	ymm11,YMMWORD PTR[((128-8-128))+r13]
	add	r11,rax
	mov	rax,rdx
	imul	rax,QWORD PTR[((8-128))+r13]
	vpaddq	ymm2,ymm2,ymm10
	add	rax,r12
	shr	r11,29
	vpmuludq	ymm14,ymm14,ymm13
	vmovdqu	ymm10,YMMWORD PTR[((160-8-128))+r13]
	add	rax,r11
	vpaddq	ymm3,ymm3,ymm14
	vpmuludq	ymm11,ymm11,ymm13
	vmovdqu	ymm14,YMMWORD PTR[((192-8-128))+r13]
DB	067h
	mov	r12,rax
	imul	eax,ecx
	vpaddq	ymm4,ymm4,ymm11
	vpmuludq	ymm10,ymm10,ymm13
DB	0c4h,041h,07eh,06fh,09dh,058h,000h,000h,000h
	and	eax,01fffffffh
	vpaddq	ymm5,ymm5,ymm10
	vpmuludq	ymm14,ymm14,ymm13
	vmovdqu	ymm10,YMMWORD PTR[((256-8-128))+r13]
	vpaddq	ymm6,ymm6,ymm14
	vpmuludq	ymm11,ymm11,ymm13
	vmovdqu	ymm9,YMMWORD PTR[((288-8-128))+r13]
	vmovd	xmm0,eax
	imul	rax,QWORD PTR[((-128))+r13]
	vpaddq	ymm7,ymm7,ymm11
	vpmuludq	ymm10,ymm10,ymm13
	vmovdqu	ymm14,YMMWORD PTR[((32-16-128))+r13]
	vpbroadcastq	ymm0,xmm0
	vpaddq	ymm8,ymm8,ymm10
	vpmuludq	ymm9,ymm9,ymm13
	vmovdqu	ymm11,YMMWORD PTR[((64-16-128))+r13]
	add	r12,rax

	vmovdqu	ymm13,YMMWORD PTR[((32-24-128))+r13]
	vpmuludq	ymm14,ymm14,ymm12
	vmovdqu	ymm10,YMMWORD PTR[((96-16-128))+r13]
	vpaddq	ymm1,ymm1,ymm14
	vpmuludq	ymm13,ymm13,ymm0
	vpmuludq	ymm11,ymm11,ymm12
DB	0c4h,041h,07eh,06fh,0b5h,0f0h,0ffh,0ffh,0ffh
	vpaddq	ymm13,ymm13,ymm1
	vpaddq	ymm2,ymm2,ymm11
	vpmuludq	ymm10,ymm10,ymm12
	vmovdqu	ymm11,YMMWORD PTR[((160-16-128))+r13]
DB	067h
	vmovq	rax,xmm13
	vmovdqu	YMMWORD PTR[rsp],ymm13
	vpaddq	ymm3,ymm3,ymm10
	vpmuludq	ymm14,ymm14,ymm12
	vmovdqu	ymm10,YMMWORD PTR[((192-16-128))+r13]
	vpaddq	ymm4,ymm4,ymm14
	vpmuludq	ymm11,ymm11,ymm12
	vmovdqu	ymm14,YMMWORD PTR[((224-16-128))+r13]
	vpaddq	ymm5,ymm5,ymm11
	vpmuludq	ymm10,ymm10,ymm12
	vmovdqu	ymm11,YMMWORD PTR[((256-16-128))+r13]
	vpaddq	ymm6,ymm6,ymm10
	vpmuludq	ymm14,ymm14,ymm12
	shr	r12,29
	vmovdqu	ymm10,YMMWORD PTR[((288-16-128))+r13]
	add	rax,r12
	vpaddq	ymm7,ymm7,ymm14
	vpmuludq	ymm11,ymm11,ymm12

	mov	r9,rax
	imul	eax,ecx
	vpaddq	ymm8,ymm8,ymm11
	vpmuludq	ymm10,ymm10,ymm12
	and	eax,01fffffffh
	vmovd	xmm12,eax
	vmovdqu	ymm11,YMMWORD PTR[((96-24-128))+r13]
DB	067h
	vpaddq	ymm9,ymm9,ymm10
	vpbroadcastq	ymm12,xmm12

	vpmuludq	ymm14,ymm0,YMMWORD PTR[((64-24-128))+r13]
	vmovdqu	ymm10,YMMWORD PTR[((128-24-128))+r13]
	mov	rdx,rax
	imul	rax,QWORD PTR[((-128))+r13]
	mov	r10,QWORD PTR[8+rsp]
	vpaddq	ymm1,ymm2,ymm14
	vpmuludq	ymm11,ymm11,ymm0
	vmovdqu	ymm14,YMMWORD PTR[((160-24-128))+r13]
	add	r9,rax
	mov	rax,rdx
	imul	rax,QWORD PTR[((8-128))+r13]
DB	067h
	shr	r9,29
	mov	r11,QWORD PTR[16+rsp]
	vpaddq	ymm2,ymm3,ymm11
	vpmuludq	ymm10,ymm10,ymm0
	vmovdqu	ymm11,YMMWORD PTR[((192-24-128))+r13]
	add	r10,rax
	mov	rax,rdx
	imul	rax,QWORD PTR[((16-128))+r13]
	vpaddq	ymm3,ymm4,ymm10
	vpmuludq	ymm14,ymm14,ymm0
	vmovdqu	ymm10,YMMWORD PTR[((224-24-128))+r13]
	imul	rdx,QWORD PTR[((24-128))+r13]
	add	r11,rax
	lea	rax,QWORD PTR[r10*1+r9]
	vpaddq	ymm4,ymm5,ymm14
	vpmuludq	ymm11,ymm11,ymm0
	vmovdqu	ymm14,YMMWORD PTR[((256-24-128))+r13]
	mov	r10,rax
	imul	eax,ecx
	vpmuludq	ymm10,ymm10,ymm0
	vpaddq	ymm5,ymm6,ymm11
	vmovdqu	ymm11,YMMWORD PTR[((288-24-128))+r13]
	and	eax,01fffffffh
	vpaddq	ymm6,ymm7,ymm10
	vpmuludq	ymm14,ymm14,ymm0
	add	rdx,QWORD PTR[24+rsp]
	vpaddq	ymm7,ymm8,ymm14
	vpmuludq	ymm11,ymm11,ymm0
	vpaddq	ymm8,ymm9,ymm11
	vmovq	xmm9,r12
	mov	r12,rdx

	dec	r14d
	jnz	$L$OOP_REDUCE_1024
	lea	r12,QWORD PTR[448+rsp]
	vpaddq	ymm0,ymm13,ymm9
	vpxor	ymm9,ymm9,ymm9

	vpaddq	ymm0,ymm0,YMMWORD PTR[((288-192))+rbx]
	vpaddq	ymm1,ymm1,YMMWORD PTR[((320-448))+r12]
	vpaddq	ymm2,ymm2,YMMWORD PTR[((352-448))+r12]
	vpaddq	ymm3,ymm3,YMMWORD PTR[((384-448))+r12]
	vpaddq	ymm4,ymm4,YMMWORD PTR[((416-448))+r12]
	vpaddq	ymm5,ymm5,YMMWORD PTR[((448-448))+r12]
	vpaddq	ymm6,ymm6,YMMWORD PTR[((480-448))+r12]
	vpaddq	ymm7,ymm7,YMMWORD PTR[((512-448))+r12]
	vpaddq	ymm8,ymm8,YMMWORD PTR[((544-448))+r12]

	vpsrlq	ymm14,ymm0,29
	vpand	ymm0,ymm0,ymm15
	vpsrlq	ymm11,ymm1,29
	vpand	ymm1,ymm1,ymm15
	vpsrlq	ymm12,ymm2,29
	vpermq	ymm14,ymm14,093h
	vpand	ymm2,ymm2,ymm15
	vpsrlq	ymm13,ymm3,29
	vpermq	ymm11,ymm11,093h
	vpand	ymm3,ymm3,ymm15
	vpermq	ymm12,ymm12,093h

	vpblendd	ymm10,ymm14,ymm9,3
	vpermq	ymm13,ymm13,093h
	vpblendd	ymm14,ymm11,ymm14,3
	vpaddq	ymm0,ymm0,ymm10
	vpblendd	ymm11,ymm12,ymm11,3
	vpaddq	ymm1,ymm1,ymm14
	vpblendd	ymm12,ymm13,ymm12,3
	vpaddq	ymm2,ymm2,ymm11
	vpblendd	ymm13,ymm9,ymm13,3
	vpaddq	ymm3,ymm3,ymm12
	vpaddq	ymm4,ymm4,ymm13

	vpsrlq	ymm14,ymm0,29
	vpand	ymm0,ymm0,ymm15
	vpsrlq	ymm11,ymm1,29
	vpand	ymm1,ymm1,ymm15
	vpsrlq	ymm12,ymm2,29
	vpermq	ymm14,ymm14,093h
	vpand	ymm2,ymm2,ymm15
	vpsrlq	ymm13,ymm3,29
	vpermq	ymm11,ymm11,093h
	vpand	ymm3,ymm3,ymm15
	vpermq	ymm12,ymm12,093h

	vpblendd	ymm10,ymm14,ymm9,3
	vpermq	ymm13,ymm13,093h
	vpblendd	ymm14,ymm11,ymm14,3
	vpaddq	ymm0,ymm0,ymm10
	vpblendd	ymm11,ymm12,ymm11,3
	vpaddq	ymm1,ymm1,ymm14
	vmovdqu	YMMWORD PTR[(0-128)+rdi],ymm0
	vpblendd	ymm12,ymm13,ymm12,3
	vpaddq	ymm2,ymm2,ymm11
	vmovdqu	YMMWORD PTR[(32-128)+rdi],ymm1
	vpblendd	ymm13,ymm9,ymm13,3
	vpaddq	ymm3,ymm3,ymm12
	vmovdqu	YMMWORD PTR[(64-128)+rdi],ymm2
	vpaddq	ymm4,ymm4,ymm13
	vmovdqu	YMMWORD PTR[(96-128)+rdi],ymm3
	vpsrlq	ymm14,ymm4,29
	vpand	ymm4,ymm4,ymm15
	vpsrlq	ymm11,ymm5,29
	vpand	ymm5,ymm5,ymm15
	vpsrlq	ymm12,ymm6,29
	vpermq	ymm14,ymm14,093h
	vpand	ymm6,ymm6,ymm15
	vpsrlq	ymm13,ymm7,29
	vpermq	ymm11,ymm11,093h
	vpand	ymm7,ymm7,ymm15
	vpsrlq	ymm0,ymm8,29
	vpermq	ymm12,ymm12,093h
	vpand	ymm8,ymm8,ymm15
	vpermq	ymm13,ymm13,093h

	vpblendd	ymm10,ymm14,ymm9,3
	vpermq	ymm0,ymm0,093h
	vpblendd	ymm14,ymm11,ymm14,3
	vpaddq	ymm4,ymm4,ymm10
	vpblendd	ymm11,ymm12,ymm11,3
	vpaddq	ymm5,ymm5,ymm14
	vpblendd	ymm12,ymm13,ymm12,3
	vpaddq	ymm6,ymm6,ymm11
	vpblendd	ymm13,ymm0,ymm13,3
	vpaddq	ymm7,ymm7,ymm12
	vpaddq	ymm8,ymm8,ymm13

	vpsrlq	ymm14,ymm4,29
	vpand	ymm4,ymm4,ymm15
	vpsrlq	ymm11,ymm5,29
	vpand	ymm5,ymm5,ymm15
	vpsrlq	ymm12,ymm6,29
	vpermq	ymm14,ymm14,093h
	vpand	ymm6,ymm6,ymm15
	vpsrlq	ymm13,ymm7,29
	vpermq	ymm11,ymm11,093h
	vpand	ymm7,ymm7,ymm15
	vpsrlq	ymm0,ymm8,29
	vpermq	ymm12,ymm12,093h
	vpand	ymm8,ymm8,ymm15
	vpermq	ymm13,ymm13,093h

	vpblendd	ymm10,ymm14,ymm9,3
	vpermq	ymm0,ymm0,093h
	vpblendd	ymm14,ymm11,ymm14,3
	vpaddq	ymm4,ymm4,ymm10
	vpblendd	ymm11,ymm12,ymm11,3
	vpaddq	ymm5,ymm5,ymm14
	vmovdqu	YMMWORD PTR[(128-128)+rdi],ymm4
	vpblendd	ymm12,ymm13,ymm12,3
	vpaddq	ymm6,ymm6,ymm11
	vmovdqu	YMMWORD PTR[(160-128)+rdi],ymm5
	vpblendd	ymm13,ymm0,ymm13,3
	vpaddq	ymm7,ymm7,ymm12
	vmovdqu	YMMWORD PTR[(192-128)+rdi],ymm6
	vpaddq	ymm8,ymm8,ymm13
	vmovdqu	YMMWORD PTR[(224-128)+rdi],ymm7
	vmovdqu	YMMWORD PTR[(256-128)+rdi],ymm8

	mov	rsi,rdi
	dec	r8d
	jne	$L$OOP_GRANDE_SQR_1024

	vzeroall
	mov	rax,rbp
	movaps	xmm6,XMMWORD PTR[((-216))+rax]
	movaps	xmm7,XMMWORD PTR[((-200))+rax]
	movaps	xmm8,XMMWORD PTR[((-184))+rax]
	movaps	xmm9,XMMWORD PTR[((-168))+rax]
	movaps	xmm10,XMMWORD PTR[((-152))+rax]
	movaps	xmm11,XMMWORD PTR[((-136))+rax]
	movaps	xmm12,XMMWORD PTR[((-120))+rax]
	movaps	xmm13,XMMWORD PTR[((-104))+rax]
	movaps	xmm14,XMMWORD PTR[((-88))+rax]
	movaps	xmm15,XMMWORD PTR[((-72))+rax]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$sqr_1024_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_rsaz_1024_sqr_avx2::
rsaz_1024_sqr_avx2	ENDP
PUBLIC	rsaz_1024_mul_avx2

ALIGN	64
rsaz_1024_mul_avx2	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_rsaz_1024_mul_avx2::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]


	lea	rax,QWORD PTR[rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	vzeroupper
	lea	rsp,QWORD PTR[((-168))+rsp]
	vmovaps	XMMWORD PTR[(-216)+rax],xmm6
	vmovaps	XMMWORD PTR[(-200)+rax],xmm7
	vmovaps	XMMWORD PTR[(-184)+rax],xmm8
	vmovaps	XMMWORD PTR[(-168)+rax],xmm9
	vmovaps	XMMWORD PTR[(-152)+rax],xmm10
	vmovaps	XMMWORD PTR[(-136)+rax],xmm11
	vmovaps	XMMWORD PTR[(-120)+rax],xmm12
	vmovaps	XMMWORD PTR[(-104)+rax],xmm13
	vmovaps	XMMWORD PTR[(-88)+rax],xmm14
	vmovaps	XMMWORD PTR[(-72)+rax],xmm15
$L$mul_1024_body::
	mov	rbp,rax
	vzeroall
	mov	r13,rdx
	sub	rsp,64






DB	067h,067h
	mov	r15,rsi
	and	r15,4095
	add	r15,32*10
	shr	r15,12
	mov	r15,rsi
	cmovnz	rsi,r13
	cmovnz	r13,r15

	mov	r15,rcx
	sub	rsi,-128
	sub	rcx,-128
	sub	rdi,-128

	and	r15,4095
	add	r15,32*10
DB	067h,067h
	shr	r15,12
	jz	$L$mul_1024_no_n_copy





	sub	rsp,32*10
	vmovdqu	ymm0,YMMWORD PTR[((0-128))+rcx]
	and	rsp,-512
	vmovdqu	ymm1,YMMWORD PTR[((32-128))+rcx]
	vmovdqu	ymm2,YMMWORD PTR[((64-128))+rcx]
	vmovdqu	ymm3,YMMWORD PTR[((96-128))+rcx]
	vmovdqu	ymm4,YMMWORD PTR[((128-128))+rcx]
	vmovdqu	ymm5,YMMWORD PTR[((160-128))+rcx]
	vmovdqu	ymm6,YMMWORD PTR[((192-128))+rcx]
	vmovdqu	ymm7,YMMWORD PTR[((224-128))+rcx]
	vmovdqu	ymm8,YMMWORD PTR[((256-128))+rcx]
	lea	rcx,QWORD PTR[((64+128))+rsp]
	vmovdqu	YMMWORD PTR[(0-128)+rcx],ymm0
	vpxor	ymm0,ymm0,ymm0
	vmovdqu	YMMWORD PTR[(32-128)+rcx],ymm1
	vpxor	ymm1,ymm1,ymm1
	vmovdqu	YMMWORD PTR[(64-128)+rcx],ymm2
	vpxor	ymm2,ymm2,ymm2
	vmovdqu	YMMWORD PTR[(96-128)+rcx],ymm3
	vpxor	ymm3,ymm3,ymm3
	vmovdqu	YMMWORD PTR[(128-128)+rcx],ymm4
	vpxor	ymm4,ymm4,ymm4
	vmovdqu	YMMWORD PTR[(160-128)+rcx],ymm5
	vpxor	ymm5,ymm5,ymm5
	vmovdqu	YMMWORD PTR[(192-128)+rcx],ymm6
	vpxor	ymm6,ymm6,ymm6
	vmovdqu	YMMWORD PTR[(224-128)+rcx],ymm7
	vpxor	ymm7,ymm7,ymm7
	vmovdqu	YMMWORD PTR[(256-128)+rcx],ymm8
	vmovdqa	ymm8,ymm0
	vmovdqu	YMMWORD PTR[(288-128)+rcx],ymm9
$L$mul_1024_no_n_copy::
	and	rsp,-64

	mov	rbx,QWORD PTR[r13]
	vpbroadcastq	ymm10,QWORD PTR[r13]
	vmovdqu	YMMWORD PTR[rsp],ymm0
	xor	r9,r9
DB	067h
	xor	r10,r10
	xor	r11,r11
	xor	r12,r12

	vmovdqu	ymm15,YMMWORD PTR[$L$and_mask]
	mov	r14d,9
	vmovdqu	YMMWORD PTR[(288-128)+rdi],ymm9
	jmp	$L$oop_mul_1024

ALIGN	32
$L$oop_mul_1024::
	vpsrlq	ymm9,ymm3,29
	mov	rax,rbx
	imul	rax,QWORD PTR[((-128))+rsi]
	add	rax,r9
	mov	r10,rbx
	imul	r10,QWORD PTR[((8-128))+rsi]
	add	r10,QWORD PTR[8+rsp]

	mov	r9,rax
	imul	eax,r8d
	and	eax,01fffffffh

	mov	r11,rbx
	imul	r11,QWORD PTR[((16-128))+rsi]
	add	r11,QWORD PTR[16+rsp]

	mov	r12,rbx
	imul	r12,QWORD PTR[((24-128))+rsi]
	add	r12,QWORD PTR[24+rsp]
	vpmuludq	ymm0,ymm10,YMMWORD PTR[((32-128))+rsi]
	vmovd	xmm11,eax
	vpaddq	ymm1,ymm1,ymm0
	vpmuludq	ymm12,ymm10,YMMWORD PTR[((64-128))+rsi]
	vpbroadcastq	ymm11,xmm11
	vpaddq	ymm2,ymm2,ymm12
	vpmuludq	ymm13,ymm10,YMMWORD PTR[((96-128))+rsi]
	vpand	ymm3,ymm3,ymm15
	vpaddq	ymm3,ymm3,ymm13
	vpmuludq	ymm0,ymm10,YMMWORD PTR[((128-128))+rsi]
	vpaddq	ymm4,ymm4,ymm0
	vpmuludq	ymm12,ymm10,YMMWORD PTR[((160-128))+rsi]
	vpaddq	ymm5,ymm5,ymm12
	vpmuludq	ymm13,ymm10,YMMWORD PTR[((192-128))+rsi]
	vpaddq	ymm6,ymm6,ymm13
	vpmuludq	ymm0,ymm10,YMMWORD PTR[((224-128))+rsi]
	vpermq	ymm9,ymm9,093h
	vpaddq	ymm7,ymm7,ymm0
	vpmuludq	ymm12,ymm10,YMMWORD PTR[((256-128))+rsi]
	vpbroadcastq	ymm10,QWORD PTR[8+r13]
	vpaddq	ymm8,ymm8,ymm12

	mov	rdx,rax
	imul	rax,QWORD PTR[((-128))+rcx]
	add	r9,rax
	mov	rax,rdx
	imul	rax,QWORD PTR[((8-128))+rcx]
	add	r10,rax
	mov	rax,rdx
	imul	rax,QWORD PTR[((16-128))+rcx]
	add	r11,rax
	shr	r9,29
	imul	rdx,QWORD PTR[((24-128))+rcx]
	add	r12,rdx
	add	r10,r9

	vpmuludq	ymm13,ymm11,YMMWORD PTR[((32-128))+rcx]
	vmovq	rbx,xmm10
	vpaddq	ymm1,ymm1,ymm13
	vpmuludq	ymm0,ymm11,YMMWORD PTR[((64-128))+rcx]
	vpaddq	ymm2,ymm2,ymm0
	vpmuludq	ymm12,ymm11,YMMWORD PTR[((96-128))+rcx]
	vpaddq	ymm3,ymm3,ymm12
	vpmuludq	ymm13,ymm11,YMMWORD PTR[((128-128))+rcx]
	vpaddq	ymm4,ymm4,ymm13
	vpmuludq	ymm0,ymm11,YMMWORD PTR[((160-128))+rcx]
	vpaddq	ymm5,ymm5,ymm0
	vpmuludq	ymm12,ymm11,YMMWORD PTR[((192-128))+rcx]
	vpaddq	ymm6,ymm6,ymm12
	vpmuludq	ymm13,ymm11,YMMWORD PTR[((224-128))+rcx]
	vpblendd	ymm12,ymm9,ymm14,3
	vpaddq	ymm7,ymm7,ymm13
	vpmuludq	ymm0,ymm11,YMMWORD PTR[((256-128))+rcx]
	vpaddq	ymm3,ymm3,ymm12
	vpaddq	ymm8,ymm8,ymm0

	mov	rax,rbx
	imul	rax,QWORD PTR[((-128))+rsi]
	add	r10,rax
	vmovdqu	ymm12,YMMWORD PTR[((-8+32-128))+rsi]
	mov	rax,rbx
	imul	rax,QWORD PTR[((8-128))+rsi]
	add	r11,rax
	vmovdqu	ymm13,YMMWORD PTR[((-8+64-128))+rsi]

	mov	rax,r10
	vpblendd	ymm9,ymm9,ymm14,0fch
	imul	eax,r8d
	vpaddq	ymm4,ymm4,ymm9
	and	eax,01fffffffh

	imul	rbx,QWORD PTR[((16-128))+rsi]
	add	r12,rbx
	vpmuludq	ymm12,ymm12,ymm10
	vmovd	xmm11,eax
	vmovdqu	ymm0,YMMWORD PTR[((-8+96-128))+rsi]
	vpaddq	ymm1,ymm1,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vpbroadcastq	ymm11,xmm11
	vmovdqu	ymm12,YMMWORD PTR[((-8+128-128))+rsi]
	vpaddq	ymm2,ymm2,ymm13
	vpmuludq	ymm0,ymm0,ymm10
	vmovdqu	ymm13,YMMWORD PTR[((-8+160-128))+rsi]
	vpaddq	ymm3,ymm3,ymm0
	vpmuludq	ymm12,ymm12,ymm10
	vmovdqu	ymm0,YMMWORD PTR[((-8+192-128))+rsi]
	vpaddq	ymm4,ymm4,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vmovdqu	ymm12,YMMWORD PTR[((-8+224-128))+rsi]
	vpaddq	ymm5,ymm5,ymm13
	vpmuludq	ymm0,ymm0,ymm10
	vmovdqu	ymm13,YMMWORD PTR[((-8+256-128))+rsi]
	vpaddq	ymm6,ymm6,ymm0
	vpmuludq	ymm12,ymm12,ymm10
	vmovdqu	ymm9,YMMWORD PTR[((-8+288-128))+rsi]
	vpaddq	ymm7,ymm7,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vpaddq	ymm8,ymm8,ymm13
	vpmuludq	ymm9,ymm9,ymm10
	vpbroadcastq	ymm10,QWORD PTR[16+r13]

	mov	rdx,rax
	imul	rax,QWORD PTR[((-128))+rcx]
	add	r10,rax
	vmovdqu	ymm0,YMMWORD PTR[((-8+32-128))+rcx]
	mov	rax,rdx
	imul	rax,QWORD PTR[((8-128))+rcx]
	add	r11,rax
	vmovdqu	ymm12,YMMWORD PTR[((-8+64-128))+rcx]
	shr	r10,29
	imul	rdx,QWORD PTR[((16-128))+rcx]
	add	r12,rdx
	add	r11,r10

	vpmuludq	ymm0,ymm0,ymm11
	vmovq	rbx,xmm10
	vmovdqu	ymm13,YMMWORD PTR[((-8+96-128))+rcx]
	vpaddq	ymm1,ymm1,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	vmovdqu	ymm0,YMMWORD PTR[((-8+128-128))+rcx]
	vpaddq	ymm2,ymm2,ymm12
	vpmuludq	ymm13,ymm13,ymm11
	vmovdqu	ymm12,YMMWORD PTR[((-8+160-128))+rcx]
	vpaddq	ymm3,ymm3,ymm13
	vpmuludq	ymm0,ymm0,ymm11
	vmovdqu	ymm13,YMMWORD PTR[((-8+192-128))+rcx]
	vpaddq	ymm4,ymm4,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	vmovdqu	ymm0,YMMWORD PTR[((-8+224-128))+rcx]
	vpaddq	ymm5,ymm5,ymm12
	vpmuludq	ymm13,ymm13,ymm11
	vmovdqu	ymm12,YMMWORD PTR[((-8+256-128))+rcx]
	vpaddq	ymm6,ymm6,ymm13
	vpmuludq	ymm0,ymm0,ymm11
	vmovdqu	ymm13,YMMWORD PTR[((-8+288-128))+rcx]
	vpaddq	ymm7,ymm7,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	vpaddq	ymm8,ymm8,ymm12
	vpmuludq	ymm13,ymm13,ymm11
	vpaddq	ymm9,ymm9,ymm13

	vmovdqu	ymm0,YMMWORD PTR[((-16+32-128))+rsi]
	mov	rax,rbx
	imul	rax,QWORD PTR[((-128))+rsi]
	add	rax,r11

	vmovdqu	ymm12,YMMWORD PTR[((-16+64-128))+rsi]
	mov	r11,rax
	imul	eax,r8d
	and	eax,01fffffffh

	imul	rbx,QWORD PTR[((8-128))+rsi]
	add	r12,rbx
	vpmuludq	ymm0,ymm0,ymm10
	vmovd	xmm11,eax
	vmovdqu	ymm13,YMMWORD PTR[((-16+96-128))+rsi]
	vpaddq	ymm1,ymm1,ymm0
	vpmuludq	ymm12,ymm12,ymm10
	vpbroadcastq	ymm11,xmm11
	vmovdqu	ymm0,YMMWORD PTR[((-16+128-128))+rsi]
	vpaddq	ymm2,ymm2,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vmovdqu	ymm12,YMMWORD PTR[((-16+160-128))+rsi]
	vpaddq	ymm3,ymm3,ymm13
	vpmuludq	ymm0,ymm0,ymm10
	vmovdqu	ymm13,YMMWORD PTR[((-16+192-128))+rsi]
	vpaddq	ymm4,ymm4,ymm0
	vpmuludq	ymm12,ymm12,ymm10
	vmovdqu	ymm0,YMMWORD PTR[((-16+224-128))+rsi]
	vpaddq	ymm5,ymm5,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vmovdqu	ymm12,YMMWORD PTR[((-16+256-128))+rsi]
	vpaddq	ymm6,ymm6,ymm13
	vpmuludq	ymm0,ymm0,ymm10
	vmovdqu	ymm13,YMMWORD PTR[((-16+288-128))+rsi]
	vpaddq	ymm7,ymm7,ymm0
	vpmuludq	ymm12,ymm12,ymm10
	vpaddq	ymm8,ymm8,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vpbroadcastq	ymm10,QWORD PTR[24+r13]
	vpaddq	ymm9,ymm9,ymm13

	vmovdqu	ymm0,YMMWORD PTR[((-16+32-128))+rcx]
	mov	rdx,rax
	imul	rax,QWORD PTR[((-128))+rcx]
	add	r11,rax
	vmovdqu	ymm12,YMMWORD PTR[((-16+64-128))+rcx]
	imul	rdx,QWORD PTR[((8-128))+rcx]
	add	r12,rdx
	shr	r11,29

	vpmuludq	ymm0,ymm0,ymm11
	vmovq	rbx,xmm10
	vmovdqu	ymm13,YMMWORD PTR[((-16+96-128))+rcx]
	vpaddq	ymm1,ymm1,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	vmovdqu	ymm0,YMMWORD PTR[((-16+128-128))+rcx]
	vpaddq	ymm2,ymm2,ymm12
	vpmuludq	ymm13,ymm13,ymm11
	vmovdqu	ymm12,YMMWORD PTR[((-16+160-128))+rcx]
	vpaddq	ymm3,ymm3,ymm13
	vpmuludq	ymm0,ymm0,ymm11
	vmovdqu	ymm13,YMMWORD PTR[((-16+192-128))+rcx]
	vpaddq	ymm4,ymm4,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	vmovdqu	ymm0,YMMWORD PTR[((-16+224-128))+rcx]
	vpaddq	ymm5,ymm5,ymm12
	vpmuludq	ymm13,ymm13,ymm11
	vmovdqu	ymm12,YMMWORD PTR[((-16+256-128))+rcx]
	vpaddq	ymm6,ymm6,ymm13
	vpmuludq	ymm0,ymm0,ymm11
	vmovdqu	ymm13,YMMWORD PTR[((-16+288-128))+rcx]
	vpaddq	ymm7,ymm7,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	vmovdqu	ymm0,YMMWORD PTR[((-24+32-128))+rsi]
	vpaddq	ymm8,ymm8,ymm12
	vpmuludq	ymm13,ymm13,ymm11
	vmovdqu	ymm12,YMMWORD PTR[((-24+64-128))+rsi]
	vpaddq	ymm9,ymm9,ymm13

	add	r12,r11
	imul	rbx,QWORD PTR[((-128))+rsi]
	add	r12,rbx

	mov	rax,r12
	imul	eax,r8d
	and	eax,01fffffffh

	vpmuludq	ymm0,ymm0,ymm10
	vmovd	xmm11,eax
	vmovdqu	ymm13,YMMWORD PTR[((-24+96-128))+rsi]
	vpaddq	ymm1,ymm1,ymm0
	vpmuludq	ymm12,ymm12,ymm10
	vpbroadcastq	ymm11,xmm11
	vmovdqu	ymm0,YMMWORD PTR[((-24+128-128))+rsi]
	vpaddq	ymm2,ymm2,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vmovdqu	ymm12,YMMWORD PTR[((-24+160-128))+rsi]
	vpaddq	ymm3,ymm3,ymm13
	vpmuludq	ymm0,ymm0,ymm10
	vmovdqu	ymm13,YMMWORD PTR[((-24+192-128))+rsi]
	vpaddq	ymm4,ymm4,ymm0
	vpmuludq	ymm12,ymm12,ymm10
	vmovdqu	ymm0,YMMWORD PTR[((-24+224-128))+rsi]
	vpaddq	ymm5,ymm5,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vmovdqu	ymm12,YMMWORD PTR[((-24+256-128))+rsi]
	vpaddq	ymm6,ymm6,ymm13
	vpmuludq	ymm0,ymm0,ymm10
	vmovdqu	ymm13,YMMWORD PTR[((-24+288-128))+rsi]
	vpaddq	ymm7,ymm7,ymm0
	vpmuludq	ymm12,ymm12,ymm10
	vpaddq	ymm8,ymm8,ymm12
	vpmuludq	ymm13,ymm13,ymm10
	vpbroadcastq	ymm10,QWORD PTR[32+r13]
	vpaddq	ymm9,ymm9,ymm13
	add	r13,32

	vmovdqu	ymm0,YMMWORD PTR[((-24+32-128))+rcx]
	imul	rax,QWORD PTR[((-128))+rcx]
	add	r12,rax
	shr	r12,29

	vmovdqu	ymm12,YMMWORD PTR[((-24+64-128))+rcx]
	vpmuludq	ymm0,ymm0,ymm11
	vmovq	rbx,xmm10
	vmovdqu	ymm13,YMMWORD PTR[((-24+96-128))+rcx]
	vpaddq	ymm0,ymm1,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	vmovdqu	YMMWORD PTR[rsp],ymm0
	vpaddq	ymm1,ymm2,ymm12
	vmovdqu	ymm0,YMMWORD PTR[((-24+128-128))+rcx]
	vpmuludq	ymm13,ymm13,ymm11
	vmovdqu	ymm12,YMMWORD PTR[((-24+160-128))+rcx]
	vpaddq	ymm2,ymm3,ymm13
	vpmuludq	ymm0,ymm0,ymm11
	vmovdqu	ymm13,YMMWORD PTR[((-24+192-128))+rcx]
	vpaddq	ymm3,ymm4,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	vmovdqu	ymm0,YMMWORD PTR[((-24+224-128))+rcx]
	vpaddq	ymm4,ymm5,ymm12
	vpmuludq	ymm13,ymm13,ymm11
	vmovdqu	ymm12,YMMWORD PTR[((-24+256-128))+rcx]
	vpaddq	ymm5,ymm6,ymm13
	vpmuludq	ymm0,ymm0,ymm11
	vmovdqu	ymm13,YMMWORD PTR[((-24+288-128))+rcx]
	mov	r9,r12
	vpaddq	ymm6,ymm7,ymm0
	vpmuludq	ymm12,ymm12,ymm11
	add	r9,QWORD PTR[rsp]
	vpaddq	ymm7,ymm8,ymm12
	vpmuludq	ymm13,ymm13,ymm11
	vmovq	xmm12,r12
	vpaddq	ymm8,ymm9,ymm13

	dec	r14d
	jnz	$L$oop_mul_1024
	vpaddq	ymm0,ymm12,YMMWORD PTR[rsp]

	vpsrlq	ymm12,ymm0,29
	vpand	ymm0,ymm0,ymm15
	vpsrlq	ymm13,ymm1,29
	vpand	ymm1,ymm1,ymm15
	vpsrlq	ymm10,ymm2,29
	vpermq	ymm12,ymm12,093h
	vpand	ymm2,ymm2,ymm15
	vpsrlq	ymm11,ymm3,29
	vpermq	ymm13,ymm13,093h
	vpand	ymm3,ymm3,ymm15

	vpblendd	ymm9,ymm12,ymm14,3
	vpermq	ymm10,ymm10,093h
	vpblendd	ymm12,ymm13,ymm12,3
	vpermq	ymm11,ymm11,093h
	vpaddq	ymm0,ymm0,ymm9
	vpblendd	ymm13,ymm10,ymm13,3
	vpaddq	ymm1,ymm1,ymm12
	vpblendd	ymm10,ymm11,ymm10,3
	vpaddq	ymm2,ymm2,ymm13
	vpblendd	ymm11,ymm14,ymm11,3
	vpaddq	ymm3,ymm3,ymm10
	vpaddq	ymm4,ymm4,ymm11

	vpsrlq	ymm12,ymm0,29
	vpand	ymm0,ymm0,ymm15
	vpsrlq	ymm13,ymm1,29
	vpand	ymm1,ymm1,ymm15
	vpsrlq	ymm10,ymm2,29
	vpermq	ymm12,ymm12,093h
	vpand	ymm2,ymm2,ymm15
	vpsrlq	ymm11,ymm3,29
	vpermq	ymm13,ymm13,093h
	vpand	ymm3,ymm3,ymm15
	vpermq	ymm10,ymm10,093h

	vpblendd	ymm9,ymm12,ymm14,3
	vpermq	ymm11,ymm11,093h
	vpblendd	ymm12,ymm13,ymm12,3
	vpaddq	ymm0,ymm0,ymm9
	vpblendd	ymm13,ymm10,ymm13,3
	vpaddq	ymm1,ymm1,ymm12
	vpblendd	ymm10,ymm11,ymm10,3
	vpaddq	ymm2,ymm2,ymm13
	vpblendd	ymm11,ymm14,ymm11,3
	vpaddq	ymm3,ymm3,ymm10
	vpaddq	ymm4,ymm4,ymm11

	vmovdqu	YMMWORD PTR[(0-128)+rdi],ymm0
	vmovdqu	YMMWORD PTR[(32-128)+rdi],ymm1
	vmovdqu	YMMWORD PTR[(64-128)+rdi],ymm2
	vmovdqu	YMMWORD PTR[(96-128)+rdi],ymm3
	vpsrlq	ymm12,ymm4,29
	vpand	ymm4,ymm4,ymm15
	vpsrlq	ymm13,ymm5,29
	vpand	ymm5,ymm5,ymm15
	vpsrlq	ymm10,ymm6,29
	vpermq	ymm12,ymm12,093h
	vpand	ymm6,ymm6,ymm15
	vpsrlq	ymm11,ymm7,29
	vpermq	ymm13,ymm13,093h
	vpand	ymm7,ymm7,ymm15
	vpsrlq	ymm0,ymm8,29
	vpermq	ymm10,ymm10,093h
	vpand	ymm8,ymm8,ymm15
	vpermq	ymm11,ymm11,093h

	vpblendd	ymm9,ymm12,ymm14,3
	vpermq	ymm0,ymm0,093h
	vpblendd	ymm12,ymm13,ymm12,3
	vpaddq	ymm4,ymm4,ymm9
	vpblendd	ymm13,ymm10,ymm13,3
	vpaddq	ymm5,ymm5,ymm12
	vpblendd	ymm10,ymm11,ymm10,3
	vpaddq	ymm6,ymm6,ymm13
	vpblendd	ymm11,ymm0,ymm11,3
	vpaddq	ymm7,ymm7,ymm10
	vpaddq	ymm8,ymm8,ymm11

	vpsrlq	ymm12,ymm4,29
	vpand	ymm4,ymm4,ymm15
	vpsrlq	ymm13,ymm5,29
	vpand	ymm5,ymm5,ymm15
	vpsrlq	ymm10,ymm6,29
	vpermq	ymm12,ymm12,093h
	vpand	ymm6,ymm6,ymm15
	vpsrlq	ymm11,ymm7,29
	vpermq	ymm13,ymm13,093h
	vpand	ymm7,ymm7,ymm15
	vpsrlq	ymm0,ymm8,29
	vpermq	ymm10,ymm10,093h
	vpand	ymm8,ymm8,ymm15
	vpermq	ymm11,ymm11,093h

	vpblendd	ymm9,ymm12,ymm14,3
	vpermq	ymm0,ymm0,093h
	vpblendd	ymm12,ymm13,ymm12,3
	vpaddq	ymm4,ymm4,ymm9
	vpblendd	ymm13,ymm10,ymm13,3
	vpaddq	ymm5,ymm5,ymm12
	vpblendd	ymm10,ymm11,ymm10,3
	vpaddq	ymm6,ymm6,ymm13
	vpblendd	ymm11,ymm0,ymm11,3
	vpaddq	ymm7,ymm7,ymm10
	vpaddq	ymm8,ymm8,ymm11

	vmovdqu	YMMWORD PTR[(128-128)+rdi],ymm4
	vmovdqu	YMMWORD PTR[(160-128)+rdi],ymm5
	vmovdqu	YMMWORD PTR[(192-128)+rdi],ymm6
	vmovdqu	YMMWORD PTR[(224-128)+rdi],ymm7
	vmovdqu	YMMWORD PTR[(256-128)+rdi],ymm8
	vzeroupper

	mov	rax,rbp
	movaps	xmm6,XMMWORD PTR[((-216))+rax]
	movaps	xmm7,XMMWORD PTR[((-200))+rax]
	movaps	xmm8,XMMWORD PTR[((-184))+rax]
	movaps	xmm9,XMMWORD PTR[((-168))+rax]
	movaps	xmm10,XMMWORD PTR[((-152))+rax]
	movaps	xmm11,XMMWORD PTR[((-136))+rax]
	movaps	xmm12,XMMWORD PTR[((-120))+rax]
	movaps	xmm13,XMMWORD PTR[((-104))+rax]
	movaps	xmm14,XMMWORD PTR[((-88))+rax]
	movaps	xmm15,XMMWORD PTR[((-72))+rax]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$mul_1024_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_rsaz_1024_mul_avx2::
rsaz_1024_mul_avx2	ENDP
PUBLIC	rsaz_1024_red2norm_avx2

ALIGN	32
rsaz_1024_red2norm_avx2	PROC PUBLIC
	sub	rdx,-128
	xor	rax,rax
	mov	r8,QWORD PTR[((-128))+rdx]
	mov	r9,QWORD PTR[((-120))+rdx]
	mov	r10,QWORD PTR[((-112))+rdx]
	shl	r8,0
	shl	r9,29
	mov	r11,r10
	shl	r10,58
	shr	r11,6
	add	rax,r8
	add	rax,r9
	add	rax,r10
	adc	r11,0
	mov	QWORD PTR[rcx],rax
	mov	rax,r11
	mov	r8,QWORD PTR[((-104))+rdx]
	mov	r9,QWORD PTR[((-96))+rdx]
	shl	r8,23
	mov	r10,r9
	shl	r9,52
	shr	r10,12
	add	rax,r8
	add	rax,r9
	adc	r10,0
	mov	QWORD PTR[8+rcx],rax
	mov	rax,r10
	mov	r11,QWORD PTR[((-88))+rdx]
	mov	r8,QWORD PTR[((-80))+rdx]
	shl	r11,17
	mov	r9,r8
	shl	r8,46
	shr	r9,18
	add	rax,r11
	add	rax,r8
	adc	r9,0
	mov	QWORD PTR[16+rcx],rax
	mov	rax,r9
	mov	r10,QWORD PTR[((-72))+rdx]
	mov	r11,QWORD PTR[((-64))+rdx]
	shl	r10,11
	mov	r8,r11
	shl	r11,40
	shr	r8,24
	add	rax,r10
	add	rax,r11
	adc	r8,0
	mov	QWORD PTR[24+rcx],rax
	mov	rax,r8
	mov	r9,QWORD PTR[((-56))+rdx]
	mov	r10,QWORD PTR[((-48))+rdx]
	mov	r11,QWORD PTR[((-40))+rdx]
	shl	r9,5
	shl	r10,34
	mov	r8,r11
	shl	r11,63
	shr	r8,1
	add	rax,r9
	add	rax,r10
	add	rax,r11
	adc	r8,0
	mov	QWORD PTR[32+rcx],rax
	mov	rax,r8
	mov	r9,QWORD PTR[((-32))+rdx]
	mov	r10,QWORD PTR[((-24))+rdx]
	shl	r9,28
	mov	r11,r10
	shl	r10,57
	shr	r11,7
	add	rax,r9
	add	rax,r10
	adc	r11,0
	mov	QWORD PTR[40+rcx],rax
	mov	rax,r11
	mov	r8,QWORD PTR[((-16))+rdx]
	mov	r9,QWORD PTR[((-8))+rdx]
	shl	r8,22
	mov	r10,r9
	shl	r9,51
	shr	r10,13
	add	rax,r8
	add	rax,r9
	adc	r10,0
	mov	QWORD PTR[48+rcx],rax
	mov	rax,r10
	mov	r11,QWORD PTR[rdx]
	mov	r8,QWORD PTR[8+rdx]
	shl	r11,16
	mov	r9,r8
	shl	r8,45
	shr	r9,19
	add	rax,r11
	add	rax,r8
	adc	r9,0
	mov	QWORD PTR[56+rcx],rax
	mov	rax,r9
	mov	r10,QWORD PTR[16+rdx]
	mov	r11,QWORD PTR[24+rdx]
	shl	r10,10
	mov	r8,r11
	shl	r11,39
	shr	r8,25
	add	rax,r10
	add	rax,r11
	adc	r8,0
	mov	QWORD PTR[64+rcx],rax
	mov	rax,r8
	mov	r9,QWORD PTR[32+rdx]
	mov	r10,QWORD PTR[40+rdx]
	mov	r11,QWORD PTR[48+rdx]
	shl	r9,4
	shl	r10,33
	mov	r8,r11
	shl	r11,62
	shr	r8,2
	add	rax,r9
	add	rax,r10
	add	rax,r11
	adc	r8,0
	mov	QWORD PTR[72+rcx],rax
	mov	rax,r8
	mov	r9,QWORD PTR[56+rdx]
	mov	r10,QWORD PTR[64+rdx]
	shl	r9,27
	mov	r11,r10
	shl	r10,56
	shr	r11,8
	add	rax,r9
	add	rax,r10
	adc	r11,0
	mov	QWORD PTR[80+rcx],rax
	mov	rax,r11
	mov	r8,QWORD PTR[72+rdx]
	mov	r9,QWORD PTR[80+rdx]
	shl	r8,21
	mov	r10,r9
	shl	r9,50
	shr	r10,14
	add	rax,r8
	add	rax,r9
	adc	r10,0
	mov	QWORD PTR[88+rcx],rax
	mov	rax,r10
	mov	r11,QWORD PTR[88+rdx]
	mov	r8,QWORD PTR[96+rdx]
	shl	r11,15
	mov	r9,r8
	shl	r8,44
	shr	r9,20
	add	rax,r11
	add	rax,r8
	adc	r9,0
	mov	QWORD PTR[96+rcx],rax
	mov	rax,r9
	mov	r10,QWORD PTR[104+rdx]
	mov	r11,QWORD PTR[112+rdx]
	shl	r10,9
	mov	r8,r11
	shl	r11,38
	shr	r8,26
	add	rax,r10
	add	rax,r11
	adc	r8,0
	mov	QWORD PTR[104+rcx],rax
	mov	rax,r8
	mov	r9,QWORD PTR[120+rdx]
	mov	r10,QWORD PTR[128+rdx]
	mov	r11,QWORD PTR[136+rdx]
	shl	r9,3
	shl	r10,32
	mov	r8,r11
	shl	r11,61
	shr	r8,3
	add	rax,r9
	add	rax,r10
	add	rax,r11
	adc	r8,0
	mov	QWORD PTR[112+rcx],rax
	mov	rax,r8
	mov	r9,QWORD PTR[144+rdx]
	mov	r10,QWORD PTR[152+rdx]
	shl	r9,26
	mov	r11,r10
	shl	r10,55
	shr	r11,9
	add	rax,r9
	add	rax,r10
	adc	r11,0
	mov	QWORD PTR[120+rcx],rax
	mov	rax,r11
	DB	0F3h,0C3h		;repret
rsaz_1024_red2norm_avx2	ENDP

PUBLIC	rsaz_1024_norm2red_avx2

ALIGN	32
rsaz_1024_norm2red_avx2	PROC PUBLIC
	sub	rcx,-128
	mov	r8,QWORD PTR[rdx]
	mov	eax,01fffffffh
	mov	r9,QWORD PTR[8+rdx]
	mov	r11,r8
	shr	r11,0
	and	r11,rax
	mov	QWORD PTR[((-128))+rcx],r11
	mov	r10,r8
	shr	r10,29
	and	r10,rax
	mov	QWORD PTR[((-120))+rcx],r10
	shrd	r8,r9,58
	and	r8,rax
	mov	QWORD PTR[((-112))+rcx],r8
	mov	r10,QWORD PTR[16+rdx]
	mov	r8,r9
	shr	r8,23
	and	r8,rax
	mov	QWORD PTR[((-104))+rcx],r8
	shrd	r9,r10,52
	and	r9,rax
	mov	QWORD PTR[((-96))+rcx],r9
	mov	r11,QWORD PTR[24+rdx]
	mov	r9,r10
	shr	r9,17
	and	r9,rax
	mov	QWORD PTR[((-88))+rcx],r9
	shrd	r10,r11,46
	and	r10,rax
	mov	QWORD PTR[((-80))+rcx],r10
	mov	r8,QWORD PTR[32+rdx]
	mov	r10,r11
	shr	r10,11
	and	r10,rax
	mov	QWORD PTR[((-72))+rcx],r10
	shrd	r11,r8,40
	and	r11,rax
	mov	QWORD PTR[((-64))+rcx],r11
	mov	r9,QWORD PTR[40+rdx]
	mov	r11,r8
	shr	r11,5
	and	r11,rax
	mov	QWORD PTR[((-56))+rcx],r11
	mov	r10,r8
	shr	r10,34
	and	r10,rax
	mov	QWORD PTR[((-48))+rcx],r10
	shrd	r8,r9,63
	and	r8,rax
	mov	QWORD PTR[((-40))+rcx],r8
	mov	r10,QWORD PTR[48+rdx]
	mov	r8,r9
	shr	r8,28
	and	r8,rax
	mov	QWORD PTR[((-32))+rcx],r8
	shrd	r9,r10,57
	and	r9,rax
	mov	QWORD PTR[((-24))+rcx],r9
	mov	r11,QWORD PTR[56+rdx]
	mov	r9,r10
	shr	r9,22
	and	r9,rax
	mov	QWORD PTR[((-16))+rcx],r9
	shrd	r10,r11,51
	and	r10,rax
	mov	QWORD PTR[((-8))+rcx],r10
	mov	r8,QWORD PTR[64+rdx]
	mov	r10,r11
	shr	r10,16
	and	r10,rax
	mov	QWORD PTR[rcx],r10
	shrd	r11,r8,45
	and	r11,rax
	mov	QWORD PTR[8+rcx],r11
	mov	r9,QWORD PTR[72+rdx]
	mov	r11,r8
	shr	r11,10
	and	r11,rax
	mov	QWORD PTR[16+rcx],r11
	shrd	r8,r9,39
	and	r8,rax
	mov	QWORD PTR[24+rcx],r8
	mov	r10,QWORD PTR[80+rdx]
	mov	r8,r9
	shr	r8,4
	and	r8,rax
	mov	QWORD PTR[32+rcx],r8
	mov	r11,r9
	shr	r11,33
	and	r11,rax
	mov	QWORD PTR[40+rcx],r11
	shrd	r9,r10,62
	and	r9,rax
	mov	QWORD PTR[48+rcx],r9
	mov	r11,QWORD PTR[88+rdx]
	mov	r9,r10
	shr	r9,27
	and	r9,rax
	mov	QWORD PTR[56+rcx],r9
	shrd	r10,r11,56
	and	r10,rax
	mov	QWORD PTR[64+rcx],r10
	mov	r8,QWORD PTR[96+rdx]
	mov	r10,r11
	shr	r10,21
	and	r10,rax
	mov	QWORD PTR[72+rcx],r10
	shrd	r11,r8,50
	and	r11,rax
	mov	QWORD PTR[80+rcx],r11
	mov	r9,QWORD PTR[104+rdx]
	mov	r11,r8
	shr	r11,15
	and	r11,rax
	mov	QWORD PTR[88+rcx],r11
	shrd	r8,r9,44
	and	r8,rax
	mov	QWORD PTR[96+rcx],r8
	mov	r10,QWORD PTR[112+rdx]
	mov	r8,r9
	shr	r8,9
	and	r8,rax
	mov	QWORD PTR[104+rcx],r8
	shrd	r9,r10,38
	and	r9,rax
	mov	QWORD PTR[112+rcx],r9
	mov	r11,QWORD PTR[120+rdx]
	mov	r9,r10
	shr	r9,3
	and	r9,rax
	mov	QWORD PTR[120+rcx],r9
	mov	r8,r10
	shr	r8,32
	and	r8,rax
	mov	QWORD PTR[128+rcx],r8
	shrd	r10,r11,61
	and	r10,rax
	mov	QWORD PTR[136+rcx],r10
	xor	r8,r8
	mov	r10,r11
	shr	r10,26
	and	r10,rax
	mov	QWORD PTR[144+rcx],r10
	shrd	r11,r8,55
	and	r11,rax
	mov	QWORD PTR[152+rcx],r11
	mov	QWORD PTR[160+rcx],r8
	mov	QWORD PTR[168+rcx],r8
	mov	QWORD PTR[176+rcx],r8
	mov	QWORD PTR[184+rcx],r8
	DB	0F3h,0C3h		;repret
rsaz_1024_norm2red_avx2	ENDP
PUBLIC	rsaz_1024_scatter5_avx2

ALIGN	32
rsaz_1024_scatter5_avx2	PROC PUBLIC
	vzeroupper
	vmovdqu	ymm5,YMMWORD PTR[$L$scatter_permd]
	shl	r8d,4
	lea	rcx,QWORD PTR[r8*1+rcx]
	mov	eax,9
	jmp	$L$oop_scatter_1024

ALIGN	32
$L$oop_scatter_1024::
	vmovdqu	ymm0,YMMWORD PTR[rdx]
	lea	rdx,QWORD PTR[32+rdx]
	vpermd	ymm0,ymm5,ymm0
	vmovdqu	XMMWORD PTR[rcx],xmm0
	lea	rcx,QWORD PTR[512+rcx]
	dec	eax
	jnz	$L$oop_scatter_1024

	vzeroupper
	DB	0F3h,0C3h		;repret
rsaz_1024_scatter5_avx2	ENDP

PUBLIC	rsaz_1024_gather5_avx2

ALIGN	32
rsaz_1024_gather5_avx2	PROC PUBLIC
	vzeroupper
	mov	r11,rsp
	lea	rax,QWORD PTR[((-136))+rsp]
$L$SEH_begin_rsaz_1024_gather5::

DB	048h,08dh,060h,0e0h
DB	0c5h,0f8h,029h,070h,0e0h
DB	0c5h,0f8h,029h,078h,0f0h
DB	0c5h,078h,029h,040h,000h
DB	0c5h,078h,029h,048h,010h
DB	0c5h,078h,029h,050h,020h
DB	0c5h,078h,029h,058h,030h
DB	0c5h,078h,029h,060h,040h
DB	0c5h,078h,029h,068h,050h
DB	0c5h,078h,029h,070h,060h
DB	0c5h,078h,029h,078h,070h
	lea	rsp,QWORD PTR[((-256))+rsp]
	and	rsp,-32
	lea	r10,QWORD PTR[$L$inc]
	lea	rax,QWORD PTR[((-128))+rsp]

	vmovd	xmm4,r8d
	vmovdqa	ymm0,YMMWORD PTR[r10]
	vmovdqa	ymm1,YMMWORD PTR[32+r10]
	vmovdqa	ymm5,YMMWORD PTR[64+r10]
	vpbroadcastd	ymm4,xmm4

	vpaddd	ymm2,ymm0,ymm5
	vpcmpeqd	ymm0,ymm0,ymm4
	vpaddd	ymm3,ymm1,ymm5
	vpcmpeqd	ymm1,ymm1,ymm4
	vmovdqa	YMMWORD PTR[(0+128)+rax],ymm0
	vpaddd	ymm0,ymm2,ymm5
	vpcmpeqd	ymm2,ymm2,ymm4
	vmovdqa	YMMWORD PTR[(32+128)+rax],ymm1
	vpaddd	ymm1,ymm3,ymm5
	vpcmpeqd	ymm3,ymm3,ymm4
	vmovdqa	YMMWORD PTR[(64+128)+rax],ymm2
	vpaddd	ymm2,ymm0,ymm5
	vpcmpeqd	ymm0,ymm0,ymm4
	vmovdqa	YMMWORD PTR[(96+128)+rax],ymm3
	vpaddd	ymm3,ymm1,ymm5
	vpcmpeqd	ymm1,ymm1,ymm4
	vmovdqa	YMMWORD PTR[(128+128)+rax],ymm0
	vpaddd	ymm8,ymm2,ymm5
	vpcmpeqd	ymm2,ymm2,ymm4
	vmovdqa	YMMWORD PTR[(160+128)+rax],ymm1
	vpaddd	ymm9,ymm3,ymm5
	vpcmpeqd	ymm3,ymm3,ymm4
	vmovdqa	YMMWORD PTR[(192+128)+rax],ymm2
	vpaddd	ymm10,ymm8,ymm5
	vpcmpeqd	ymm8,ymm8,ymm4
	vmovdqa	YMMWORD PTR[(224+128)+rax],ymm3
	vpaddd	ymm11,ymm9,ymm5
	vpcmpeqd	ymm9,ymm9,ymm4
	vpaddd	ymm12,ymm10,ymm5
	vpcmpeqd	ymm10,ymm10,ymm4
	vpaddd	ymm13,ymm11,ymm5
	vpcmpeqd	ymm11,ymm11,ymm4
	vpaddd	ymm14,ymm12,ymm5
	vpcmpeqd	ymm12,ymm12,ymm4
	vpaddd	ymm15,ymm13,ymm5
	vpcmpeqd	ymm13,ymm13,ymm4
	vpcmpeqd	ymm14,ymm14,ymm4
	vpcmpeqd	ymm15,ymm15,ymm4

	vmovdqa	ymm7,YMMWORD PTR[((-32))+r10]
	lea	rdx,QWORD PTR[128+rdx]
	mov	r8d,9

$L$oop_gather_1024::
	vmovdqa	ymm0,YMMWORD PTR[((0-128))+rdx]
	vmovdqa	ymm1,YMMWORD PTR[((32-128))+rdx]
	vmovdqa	ymm2,YMMWORD PTR[((64-128))+rdx]
	vmovdqa	ymm3,YMMWORD PTR[((96-128))+rdx]
	vpand	ymm0,ymm0,YMMWORD PTR[((0+128))+rax]
	vpand	ymm1,ymm1,YMMWORD PTR[((32+128))+rax]
	vpand	ymm2,ymm2,YMMWORD PTR[((64+128))+rax]
	vpor	ymm4,ymm1,ymm0
	vpand	ymm3,ymm3,YMMWORD PTR[((96+128))+rax]
	vmovdqa	ymm0,YMMWORD PTR[((128-128))+rdx]
	vmovdqa	ymm1,YMMWORD PTR[((160-128))+rdx]
	vpor	ymm5,ymm3,ymm2
	vmovdqa	ymm2,YMMWORD PTR[((192-128))+rdx]
	vmovdqa	ymm3,YMMWORD PTR[((224-128))+rdx]
	vpand	ymm0,ymm0,YMMWORD PTR[((128+128))+rax]
	vpand	ymm1,ymm1,YMMWORD PTR[((160+128))+rax]
	vpand	ymm2,ymm2,YMMWORD PTR[((192+128))+rax]
	vpor	ymm4,ymm4,ymm0
	vpand	ymm3,ymm3,YMMWORD PTR[((224+128))+rax]
	vpand	ymm0,ymm8,YMMWORD PTR[((256-128))+rdx]
	vpor	ymm5,ymm5,ymm1
	vpand	ymm1,ymm9,YMMWORD PTR[((288-128))+rdx]
	vpor	ymm4,ymm4,ymm2
	vpand	ymm2,ymm10,YMMWORD PTR[((320-128))+rdx]
	vpor	ymm5,ymm5,ymm3
	vpand	ymm3,ymm11,YMMWORD PTR[((352-128))+rdx]
	vpor	ymm4,ymm4,ymm0
	vpand	ymm0,ymm12,YMMWORD PTR[((384-128))+rdx]
	vpor	ymm5,ymm5,ymm1
	vpand	ymm1,ymm13,YMMWORD PTR[((416-128))+rdx]
	vpor	ymm4,ymm4,ymm2
	vpand	ymm2,ymm14,YMMWORD PTR[((448-128))+rdx]
	vpor	ymm5,ymm5,ymm3
	vpand	ymm3,ymm15,YMMWORD PTR[((480-128))+rdx]
	lea	rdx,QWORD PTR[512+rdx]
	vpor	ymm4,ymm4,ymm0
	vpor	ymm5,ymm5,ymm1
	vpor	ymm4,ymm4,ymm2
	vpor	ymm5,ymm5,ymm3

	vpor	ymm4,ymm4,ymm5
	vextracti128	xmm5,ymm4,1
	vpor	xmm5,xmm5,xmm4
	vpermd	ymm5,ymm7,ymm5
	vmovdqu	YMMWORD PTR[rcx],ymm5
	lea	rcx,QWORD PTR[32+rcx]
	dec	r8d
	jnz	$L$oop_gather_1024

	vpxor	ymm0,ymm0,ymm0
	vmovdqu	YMMWORD PTR[rcx],ymm0
	vzeroupper
	movaps	xmm6,XMMWORD PTR[((-168))+r11]
	movaps	xmm7,XMMWORD PTR[((-152))+r11]
	movaps	xmm8,XMMWORD PTR[((-136))+r11]
	movaps	xmm9,XMMWORD PTR[((-120))+r11]
	movaps	xmm10,XMMWORD PTR[((-104))+r11]
	movaps	xmm11,XMMWORD PTR[((-88))+r11]
	movaps	xmm12,XMMWORD PTR[((-72))+r11]
	movaps	xmm13,XMMWORD PTR[((-56))+r11]
	movaps	xmm14,XMMWORD PTR[((-40))+r11]
	movaps	xmm15,XMMWORD PTR[((-24))+r11]
$L$SEH_end_rsaz_1024_gather5::
	lea	rsp,QWORD PTR[r11]
	DB	0F3h,0C3h		;repret
rsaz_1024_gather5_avx2	ENDP
EXTERN	OPENSSL_ia32cap_P:NEAR
PUBLIC	rsaz_avx2_eligible

ALIGN	32
rsaz_avx2_eligible	PROC PUBLIC
	mov	eax,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	mov	ecx,524544
	mov	edx,0
	and	ecx,eax
	cmp	ecx,524544
	cmove	eax,edx
	and	eax,32
	shr	eax,5
	DB	0F3h,0C3h		;repret
rsaz_avx2_eligible	ENDP

ALIGN	64
$L$and_mask::
	DQ	01fffffffh,01fffffffh,01fffffffh,01fffffffh
$L$scatter_permd::
	DD	0,2,4,6,7,7,7,7
$L$gather_permd::
	DD	0,7,1,7,2,7,3,7
$L$inc::
	DD	0,0,0,0,1,1,1,1
	DD	2,2,2,2,3,3,3,3
	DD	4,4,4,4,4,4,4,4
ALIGN	64
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
rsaz_se_handler	PROC PRIVATE
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
	jb	$L$common_seh_tail

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	mov	rax,QWORD PTR[160+r8]

	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	mov	QWORD PTR[240+r8],r15
	mov	QWORD PTR[232+r8],r14
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[144+r8],rbx

	lea	rsi,QWORD PTR[((-216))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

$L$common_seh_tail::
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
rsaz_se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_rsaz_1024_sqr_avx2
	DD	imagerel $L$SEH_end_rsaz_1024_sqr_avx2
	DD	imagerel $L$SEH_info_rsaz_1024_sqr_avx2

	DD	imagerel $L$SEH_begin_rsaz_1024_mul_avx2
	DD	imagerel $L$SEH_end_rsaz_1024_mul_avx2
	DD	imagerel $L$SEH_info_rsaz_1024_mul_avx2

	DD	imagerel $L$SEH_begin_rsaz_1024_gather5
	DD	imagerel $L$SEH_end_rsaz_1024_gather5
	DD	imagerel $L$SEH_info_rsaz_1024_gather5
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_rsaz_1024_sqr_avx2::
DB	9,0,0,0
	DD	imagerel rsaz_se_handler
	DD	imagerel $L$sqr_1024_body,imagerel $L$sqr_1024_epilogue
$L$SEH_info_rsaz_1024_mul_avx2::
DB	9,0,0,0
	DD	imagerel rsaz_se_handler
	DD	imagerel $L$mul_1024_body,imagerel $L$mul_1024_epilogue
$L$SEH_info_rsaz_1024_gather5::
DB	001h,036h,017h,00bh
DB	036h,0f8h,009h,000h
DB	031h,0e8h,008h,000h
DB	02ch,0d8h,007h,000h
DB	027h,0c8h,006h,000h
DB	022h,0b8h,005h,000h
DB	01dh,0a8h,004h,000h
DB	018h,098h,003h,000h
DB	013h,088h,002h,000h
DB	00eh,078h,001h,000h
DB	009h,068h,000h,000h
DB	004h,001h,015h,000h
DB	000h,0b3h,000h,000h

.xdata	ENDS
END
