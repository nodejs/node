OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'
ALIGN	16

PUBLIC	md5_block_asm_data_order

md5_block_asm_data_order	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_md5_block_asm_data_order::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	push	rbp
	push	rbx
	push	r12
	push	r14
	push	r15
$L$prologue::




	mov	rbp,rdi
	shl	rdx,6
	lea	rdi,QWORD PTR[rdx*1+rsi]
	mov	eax,DWORD PTR[((0*4))+rbp]
	mov	ebx,DWORD PTR[((1*4))+rbp]
	mov	ecx,DWORD PTR[((2*4))+rbp]
	mov	edx,DWORD PTR[((3*4))+rbp]







	cmp	rsi,rdi
	je	$L$end



$L$loop::
	mov	r8d,eax
	mov	r9d,ebx
	mov	r14d,ecx
	mov	r15d,edx
	mov	r10d,DWORD PTR[((0*4))+rsi]
	mov	r11d,edx
	xor	r11d,ecx
	lea	eax,DWORD PTR[0d76aa478h+r10*1+rax]
	and	r11d,ebx
	xor	r11d,edx
	mov	r10d,DWORD PTR[((1*4))+rsi]
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,DWORD PTR[0e8c7b756h+r10*1+rdx]
	and	r11d,eax
	xor	r11d,ecx
	mov	r10d,DWORD PTR[((2*4))+rsi]
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,DWORD PTR[0242070dbh+r10*1+rcx]
	and	r11d,edx
	xor	r11d,ebx
	mov	r10d,DWORD PTR[((3*4))+rsi]
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,DWORD PTR[0c1bdceeeh+r10*1+rbx]
	and	r11d,ecx
	xor	r11d,eax
	mov	r10d,DWORD PTR[((4*4))+rsi]
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,DWORD PTR[0f57c0fafh+r10*1+rax]
	and	r11d,ebx
	xor	r11d,edx
	mov	r10d,DWORD PTR[((5*4))+rsi]
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,DWORD PTR[04787c62ah+r10*1+rdx]
	and	r11d,eax
	xor	r11d,ecx
	mov	r10d,DWORD PTR[((6*4))+rsi]
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,DWORD PTR[0a8304613h+r10*1+rcx]
	and	r11d,edx
	xor	r11d,ebx
	mov	r10d,DWORD PTR[((7*4))+rsi]
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,DWORD PTR[0fd469501h+r10*1+rbx]
	and	r11d,ecx
	xor	r11d,eax
	mov	r10d,DWORD PTR[((8*4))+rsi]
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,DWORD PTR[0698098d8h+r10*1+rax]
	and	r11d,ebx
	xor	r11d,edx
	mov	r10d,DWORD PTR[((9*4))+rsi]
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,DWORD PTR[08b44f7afh+r10*1+rdx]
	and	r11d,eax
	xor	r11d,ecx
	mov	r10d,DWORD PTR[((10*4))+rsi]
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,DWORD PTR[0ffff5bb1h+r10*1+rcx]
	and	r11d,edx
	xor	r11d,ebx
	mov	r10d,DWORD PTR[((11*4))+rsi]
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,DWORD PTR[0895cd7beh+r10*1+rbx]
	and	r11d,ecx
	xor	r11d,eax
	mov	r10d,DWORD PTR[((12*4))+rsi]
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	xor	r11d,ecx
	lea	eax,DWORD PTR[06b901122h+r10*1+rax]
	and	r11d,ebx
	xor	r11d,edx
	mov	r10d,DWORD PTR[((13*4))+rsi]
	add	eax,r11d
	rol	eax,7
	mov	r11d,ecx
	add	eax,ebx
	xor	r11d,ebx
	lea	edx,DWORD PTR[0fd987193h+r10*1+rdx]
	and	r11d,eax
	xor	r11d,ecx
	mov	r10d,DWORD PTR[((14*4))+rsi]
	add	edx,r11d
	rol	edx,12
	mov	r11d,ebx
	add	edx,eax
	xor	r11d,eax
	lea	ecx,DWORD PTR[0a679438eh+r10*1+rcx]
	and	r11d,edx
	xor	r11d,ebx
	mov	r10d,DWORD PTR[((15*4))+rsi]
	add	ecx,r11d
	rol	ecx,17
	mov	r11d,eax
	add	ecx,edx
	xor	r11d,edx
	lea	ebx,DWORD PTR[049b40821h+r10*1+rbx]
	and	r11d,ecx
	xor	r11d,eax
	mov	r10d,DWORD PTR[((0*4))+rsi]
	add	ebx,r11d
	rol	ebx,22
	mov	r11d,edx
	add	ebx,ecx
	mov	r10d,DWORD PTR[((1*4))+rsi]
	mov	r11d,edx
	mov	r12d,edx
	not	r11d
	lea	eax,DWORD PTR[0f61e2562h+r10*1+rax]
	and	r12d,ebx
	and	r11d,ecx
	mov	r10d,DWORD PTR[((6*4))+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	lea	edx,DWORD PTR[0c040b340h+r10*1+rdx]
	and	r12d,eax
	and	r11d,ebx
	mov	r10d,DWORD PTR[((11*4))+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	lea	ecx,DWORD PTR[0265e5a51h+r10*1+rcx]
	and	r12d,edx
	and	r11d,eax
	mov	r10d,DWORD PTR[((0*4))+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	lea	ebx,DWORD PTR[0e9b6c7aah+r10*1+rbx]
	and	r12d,ecx
	and	r11d,edx
	mov	r10d,DWORD PTR[((5*4))+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	lea	eax,DWORD PTR[0d62f105dh+r10*1+rax]
	and	r12d,ebx
	and	r11d,ecx
	mov	r10d,DWORD PTR[((10*4))+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	lea	edx,DWORD PTR[02441453h+r10*1+rdx]
	and	r12d,eax
	and	r11d,ebx
	mov	r10d,DWORD PTR[((15*4))+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	lea	ecx,DWORD PTR[0d8a1e681h+r10*1+rcx]
	and	r12d,edx
	and	r11d,eax
	mov	r10d,DWORD PTR[((4*4))+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	lea	ebx,DWORD PTR[0e7d3fbc8h+r10*1+rbx]
	and	r12d,ecx
	and	r11d,edx
	mov	r10d,DWORD PTR[((9*4))+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	lea	eax,DWORD PTR[021e1cde6h+r10*1+rax]
	and	r12d,ebx
	and	r11d,ecx
	mov	r10d,DWORD PTR[((14*4))+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	lea	edx,DWORD PTR[0c33707d6h+r10*1+rdx]
	and	r12d,eax
	and	r11d,ebx
	mov	r10d,DWORD PTR[((3*4))+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	lea	ecx,DWORD PTR[0f4d50d87h+r10*1+rcx]
	and	r12d,edx
	and	r11d,eax
	mov	r10d,DWORD PTR[((8*4))+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	lea	ebx,DWORD PTR[0455a14edh+r10*1+rbx]
	and	r12d,ecx
	and	r11d,edx
	mov	r10d,DWORD PTR[((13*4))+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	not	r11d
	lea	eax,DWORD PTR[0a9e3e905h+r10*1+rax]
	and	r12d,ebx
	and	r11d,ecx
	mov	r10d,DWORD PTR[((2*4))+rsi]
	or	r12d,r11d
	mov	r11d,ecx
	add	eax,r12d
	mov	r12d,ecx
	rol	eax,5
	add	eax,ebx
	not	r11d
	lea	edx,DWORD PTR[0fcefa3f8h+r10*1+rdx]
	and	r12d,eax
	and	r11d,ebx
	mov	r10d,DWORD PTR[((7*4))+rsi]
	or	r12d,r11d
	mov	r11d,ebx
	add	edx,r12d
	mov	r12d,ebx
	rol	edx,9
	add	edx,eax
	not	r11d
	lea	ecx,DWORD PTR[0676f02d9h+r10*1+rcx]
	and	r12d,edx
	and	r11d,eax
	mov	r10d,DWORD PTR[((12*4))+rsi]
	or	r12d,r11d
	mov	r11d,eax
	add	ecx,r12d
	mov	r12d,eax
	rol	ecx,14
	add	ecx,edx
	not	r11d
	lea	ebx,DWORD PTR[08d2a4c8ah+r10*1+rbx]
	and	r12d,ecx
	and	r11d,edx
	mov	r10d,DWORD PTR[((0*4))+rsi]
	or	r12d,r11d
	mov	r11d,edx
	add	ebx,r12d
	mov	r12d,edx
	rol	ebx,20
	add	ebx,ecx
	mov	r10d,DWORD PTR[((5*4))+rsi]
	mov	r11d,ecx
	lea	eax,DWORD PTR[0fffa3942h+r10*1+rax]
	mov	r10d,DWORD PTR[((8*4))+rsi]
	xor	r11d,edx
	xor	r11d,ebx
	add	eax,r11d
	rol	eax,4
	mov	r11d,ebx
	add	eax,ebx
	lea	edx,DWORD PTR[08771f681h+r10*1+rdx]
	mov	r10d,DWORD PTR[((11*4))+rsi]
	xor	r11d,ecx
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,DWORD PTR[06d9d6122h+r10*1+rcx]
	mov	r10d,DWORD PTR[((14*4))+rsi]
	xor	r11d,ebx
	xor	r11d,edx
	add	ecx,r11d
	rol	ecx,16
	mov	r11d,edx
	add	ecx,edx
	lea	ebx,DWORD PTR[0fde5380ch+r10*1+rbx]
	mov	r10d,DWORD PTR[((1*4))+rsi]
	xor	r11d,eax
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,DWORD PTR[0a4beea44h+r10*1+rax]
	mov	r10d,DWORD PTR[((4*4))+rsi]
	xor	r11d,edx
	xor	r11d,ebx
	add	eax,r11d
	rol	eax,4
	mov	r11d,ebx
	add	eax,ebx
	lea	edx,DWORD PTR[04bdecfa9h+r10*1+rdx]
	mov	r10d,DWORD PTR[((7*4))+rsi]
	xor	r11d,ecx
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,DWORD PTR[0f6bb4b60h+r10*1+rcx]
	mov	r10d,DWORD PTR[((10*4))+rsi]
	xor	r11d,ebx
	xor	r11d,edx
	add	ecx,r11d
	rol	ecx,16
	mov	r11d,edx
	add	ecx,edx
	lea	ebx,DWORD PTR[0bebfbc70h+r10*1+rbx]
	mov	r10d,DWORD PTR[((13*4))+rsi]
	xor	r11d,eax
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,DWORD PTR[0289b7ec6h+r10*1+rax]
	mov	r10d,DWORD PTR[((0*4))+rsi]
	xor	r11d,edx
	xor	r11d,ebx
	add	eax,r11d
	rol	eax,4
	mov	r11d,ebx
	add	eax,ebx
	lea	edx,DWORD PTR[0eaa127fah+r10*1+rdx]
	mov	r10d,DWORD PTR[((3*4))+rsi]
	xor	r11d,ecx
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,DWORD PTR[0d4ef3085h+r10*1+rcx]
	mov	r10d,DWORD PTR[((6*4))+rsi]
	xor	r11d,ebx
	xor	r11d,edx
	add	ecx,r11d
	rol	ecx,16
	mov	r11d,edx
	add	ecx,edx
	lea	ebx,DWORD PTR[04881d05h+r10*1+rbx]
	mov	r10d,DWORD PTR[((9*4))+rsi]
	xor	r11d,eax
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	lea	eax,DWORD PTR[0d9d4d039h+r10*1+rax]
	mov	r10d,DWORD PTR[((12*4))+rsi]
	xor	r11d,edx
	xor	r11d,ebx
	add	eax,r11d
	rol	eax,4
	mov	r11d,ebx
	add	eax,ebx
	lea	edx,DWORD PTR[0e6db99e5h+r10*1+rdx]
	mov	r10d,DWORD PTR[((15*4))+rsi]
	xor	r11d,ecx
	xor	r11d,eax
	add	edx,r11d
	rol	edx,11
	mov	r11d,eax
	add	edx,eax
	lea	ecx,DWORD PTR[01fa27cf8h+r10*1+rcx]
	mov	r10d,DWORD PTR[((2*4))+rsi]
	xor	r11d,ebx
	xor	r11d,edx
	add	ecx,r11d
	rol	ecx,16
	mov	r11d,edx
	add	ecx,edx
	lea	ebx,DWORD PTR[0c4ac5665h+r10*1+rbx]
	mov	r10d,DWORD PTR[((0*4))+rsi]
	xor	r11d,eax
	xor	r11d,ecx
	add	ebx,r11d
	rol	ebx,23
	mov	r11d,ecx
	add	ebx,ecx
	mov	r10d,DWORD PTR[((0*4))+rsi]
	mov	r11d,0ffffffffh
	xor	r11d,edx
	lea	eax,DWORD PTR[0f4292244h+r10*1+rax]
	or	r11d,ebx
	xor	r11d,ecx
	add	eax,r11d
	mov	r10d,DWORD PTR[((7*4))+rsi]
	mov	r11d,0ffffffffh
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,DWORD PTR[0432aff97h+r10*1+rdx]
	or	r11d,eax
	xor	r11d,ebx
	add	edx,r11d
	mov	r10d,DWORD PTR[((14*4))+rsi]
	mov	r11d,0ffffffffh
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,DWORD PTR[0ab9423a7h+r10*1+rcx]
	or	r11d,edx
	xor	r11d,eax
	add	ecx,r11d
	mov	r10d,DWORD PTR[((5*4))+rsi]
	mov	r11d,0ffffffffh
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,DWORD PTR[0fc93a039h+r10*1+rbx]
	or	r11d,ecx
	xor	r11d,edx
	add	ebx,r11d
	mov	r10d,DWORD PTR[((12*4))+rsi]
	mov	r11d,0ffffffffh
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,DWORD PTR[0655b59c3h+r10*1+rax]
	or	r11d,ebx
	xor	r11d,ecx
	add	eax,r11d
	mov	r10d,DWORD PTR[((3*4))+rsi]
	mov	r11d,0ffffffffh
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,DWORD PTR[08f0ccc92h+r10*1+rdx]
	or	r11d,eax
	xor	r11d,ebx
	add	edx,r11d
	mov	r10d,DWORD PTR[((10*4))+rsi]
	mov	r11d,0ffffffffh
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,DWORD PTR[0ffeff47dh+r10*1+rcx]
	or	r11d,edx
	xor	r11d,eax
	add	ecx,r11d
	mov	r10d,DWORD PTR[((1*4))+rsi]
	mov	r11d,0ffffffffh
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,DWORD PTR[085845dd1h+r10*1+rbx]
	or	r11d,ecx
	xor	r11d,edx
	add	ebx,r11d
	mov	r10d,DWORD PTR[((8*4))+rsi]
	mov	r11d,0ffffffffh
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,DWORD PTR[06fa87e4fh+r10*1+rax]
	or	r11d,ebx
	xor	r11d,ecx
	add	eax,r11d
	mov	r10d,DWORD PTR[((15*4))+rsi]
	mov	r11d,0ffffffffh
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,DWORD PTR[0fe2ce6e0h+r10*1+rdx]
	or	r11d,eax
	xor	r11d,ebx
	add	edx,r11d
	mov	r10d,DWORD PTR[((6*4))+rsi]
	mov	r11d,0ffffffffh
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,DWORD PTR[0a3014314h+r10*1+rcx]
	or	r11d,edx
	xor	r11d,eax
	add	ecx,r11d
	mov	r10d,DWORD PTR[((13*4))+rsi]
	mov	r11d,0ffffffffh
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,DWORD PTR[04e0811a1h+r10*1+rbx]
	or	r11d,ecx
	xor	r11d,edx
	add	ebx,r11d
	mov	r10d,DWORD PTR[((4*4))+rsi]
	mov	r11d,0ffffffffh
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx
	lea	eax,DWORD PTR[0f7537e82h+r10*1+rax]
	or	r11d,ebx
	xor	r11d,ecx
	add	eax,r11d
	mov	r10d,DWORD PTR[((11*4))+rsi]
	mov	r11d,0ffffffffh
	rol	eax,6
	xor	r11d,ecx
	add	eax,ebx
	lea	edx,DWORD PTR[0bd3af235h+r10*1+rdx]
	or	r11d,eax
	xor	r11d,ebx
	add	edx,r11d
	mov	r10d,DWORD PTR[((2*4))+rsi]
	mov	r11d,0ffffffffh
	rol	edx,10
	xor	r11d,ebx
	add	edx,eax
	lea	ecx,DWORD PTR[02ad7d2bbh+r10*1+rcx]
	or	r11d,edx
	xor	r11d,eax
	add	ecx,r11d
	mov	r10d,DWORD PTR[((9*4))+rsi]
	mov	r11d,0ffffffffh
	rol	ecx,15
	xor	r11d,eax
	add	ecx,edx
	lea	ebx,DWORD PTR[0eb86d391h+r10*1+rbx]
	or	r11d,ecx
	xor	r11d,edx
	add	ebx,r11d
	mov	r10d,DWORD PTR[((0*4))+rsi]
	mov	r11d,0ffffffffh
	rol	ebx,21
	xor	r11d,edx
	add	ebx,ecx

	add	eax,r8d
	add	ebx,r9d
	add	ecx,r14d
	add	edx,r15d


	add	rsi,64
	cmp	rsi,rdi
	jb	$L$loop



$L$end::
	mov	DWORD PTR[((0*4))+rbp],eax
	mov	DWORD PTR[((1*4))+rbp],ebx
	mov	DWORD PTR[((2*4))+rbp],ecx
	mov	DWORD PTR[((3*4))+rbp],edx

	mov	r15,QWORD PTR[rsp]
	mov	r14,QWORD PTR[8+rsp]
	mov	r12,QWORD PTR[16+rsp]
	mov	rbx,QWORD PTR[24+rsp]
	mov	rbp,QWORD PTR[32+rsp]
	add	rsp,40
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_md5_block_asm_data_order::
md5_block_asm_data_order	ENDP
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

	lea	r10,QWORD PTR[$L$prologue]
	cmp	rbx,r10
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$epilogue]
	cmp	rbx,r10
	jae	$L$in_prologue

	lea	rax,QWORD PTR[40+rax]

	mov	rbp,QWORD PTR[((-8))+rax]
	mov	rbx,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	r14,QWORD PTR[((-32))+rax]
	mov	r15,QWORD PTR[((-40))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
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
se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_md5_block_asm_data_order
	DD	imagerel $L$SEH_end_md5_block_asm_data_order
	DD	imagerel $L$SEH_info_md5_block_asm_data_order

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_md5_block_asm_data_order::
DB	9,0,0,0
	DD	imagerel se_handler

.xdata	ENDS
END
