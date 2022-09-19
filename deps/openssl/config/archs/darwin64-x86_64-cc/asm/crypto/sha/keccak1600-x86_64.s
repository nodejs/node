.text	


.p2align	5
__KeccakF1600:

	movq	60(%rdi),%rax
	movq	68(%rdi),%rbx
	movq	76(%rdi),%rcx
	movq	84(%rdi),%rdx
	movq	92(%rdi),%rbp
	jmp	L$oop

.p2align	5
L$oop:
	movq	-100(%rdi),%r8
	movq	-52(%rdi),%r9
	movq	-4(%rdi),%r10
	movq	44(%rdi),%r11

	xorq	-84(%rdi),%rcx
	xorq	-76(%rdi),%rdx
	xorq	%r8,%rax
	xorq	-92(%rdi),%rbx
	xorq	-44(%rdi),%rcx
	xorq	-60(%rdi),%rax
	movq	%rbp,%r12
	xorq	-68(%rdi),%rbp

	xorq	%r10,%rcx
	xorq	-20(%rdi),%rax
	xorq	-36(%rdi),%rdx
	xorq	%r9,%rbx
	xorq	-28(%rdi),%rbp

	xorq	36(%rdi),%rcx
	xorq	20(%rdi),%rax
	xorq	4(%rdi),%rdx
	xorq	-12(%rdi),%rbx
	xorq	12(%rdi),%rbp

	movq	%rcx,%r13
	rolq	$1,%rcx
	xorq	%rax,%rcx
	xorq	%r11,%rdx

	rolq	$1,%rax
	xorq	%rdx,%rax
	xorq	28(%rdi),%rbx

	rolq	$1,%rdx
	xorq	%rbx,%rdx
	xorq	52(%rdi),%rbp

	rolq	$1,%rbx
	xorq	%rbp,%rbx

	rolq	$1,%rbp
	xorq	%r13,%rbp
	xorq	%rcx,%r9
	xorq	%rdx,%r10
	rolq	$44,%r9
	xorq	%rbp,%r11
	xorq	%rax,%r12
	rolq	$43,%r10
	xorq	%rbx,%r8
	movq	%r9,%r13
	rolq	$21,%r11
	orq	%r10,%r9
	xorq	%r8,%r9
	rolq	$14,%r12

	xorq	(%r15),%r9
	leaq	8(%r15),%r15

	movq	%r12,%r14
	andq	%r11,%r12
	movq	%r9,-100(%rsi)
	xorq	%r10,%r12
	notq	%r10
	movq	%r12,-84(%rsi)

	orq	%r11,%r10
	movq	76(%rdi),%r12
	xorq	%r13,%r10
	movq	%r10,-92(%rsi)

	andq	%r8,%r13
	movq	-28(%rdi),%r9
	xorq	%r14,%r13
	movq	-20(%rdi),%r10
	movq	%r13,-68(%rsi)

	orq	%r8,%r14
	movq	-76(%rdi),%r8
	xorq	%r11,%r14
	movq	28(%rdi),%r11
	movq	%r14,-76(%rsi)


	xorq	%rbp,%r8
	xorq	%rdx,%r12
	rolq	$28,%r8
	xorq	%rcx,%r11
	xorq	%rax,%r9
	rolq	$61,%r12
	rolq	$45,%r11
	xorq	%rbx,%r10
	rolq	$20,%r9
	movq	%r8,%r13
	orq	%r12,%r8
	rolq	$3,%r10

	xorq	%r11,%r8
	movq	%r8,-36(%rsi)

	movq	%r9,%r14
	andq	%r13,%r9
	movq	-92(%rdi),%r8
	xorq	%r12,%r9
	notq	%r12
	movq	%r9,-28(%rsi)

	orq	%r11,%r12
	movq	-44(%rdi),%r9
	xorq	%r10,%r12
	movq	%r12,-44(%rsi)

	andq	%r10,%r11
	movq	60(%rdi),%r12
	xorq	%r14,%r11
	movq	%r11,-52(%rsi)

	orq	%r10,%r14
	movq	4(%rdi),%r10
	xorq	%r13,%r14
	movq	52(%rdi),%r11
	movq	%r14,-60(%rsi)


	xorq	%rbp,%r10
	xorq	%rax,%r11
	rolq	$25,%r10
	xorq	%rdx,%r9
	rolq	$8,%r11
	xorq	%rbx,%r12
	rolq	$6,%r9
	xorq	%rcx,%r8
	rolq	$18,%r12
	movq	%r10,%r13
	andq	%r11,%r10
	rolq	$1,%r8

	notq	%r11
	xorq	%r9,%r10
	movq	%r10,-12(%rsi)

	movq	%r12,%r14
	andq	%r11,%r12
	movq	-12(%rdi),%r10
	xorq	%r13,%r12
	movq	%r12,-4(%rsi)

	orq	%r9,%r13
	movq	84(%rdi),%r12
	xorq	%r8,%r13
	movq	%r13,-20(%rsi)

	andq	%r8,%r9
	xorq	%r14,%r9
	movq	%r9,12(%rsi)

	orq	%r8,%r14
	movq	-60(%rdi),%r9
	xorq	%r11,%r14
	movq	36(%rdi),%r11
	movq	%r14,4(%rsi)


	movq	-68(%rdi),%r8

	xorq	%rcx,%r10
	xorq	%rdx,%r11
	rolq	$10,%r10
	xorq	%rbx,%r9
	rolq	$15,%r11
	xorq	%rbp,%r12
	rolq	$36,%r9
	xorq	%rax,%r8
	rolq	$56,%r12
	movq	%r10,%r13
	orq	%r11,%r10
	rolq	$27,%r8

	notq	%r11
	xorq	%r9,%r10
	movq	%r10,28(%rsi)

	movq	%r12,%r14
	orq	%r11,%r12
	xorq	%r13,%r12
	movq	%r12,36(%rsi)

	andq	%r9,%r13
	xorq	%r8,%r13
	movq	%r13,20(%rsi)

	orq	%r8,%r9
	xorq	%r14,%r9
	movq	%r9,52(%rsi)

	andq	%r14,%r8
	xorq	%r11,%r8
	movq	%r8,44(%rsi)


	xorq	-84(%rdi),%rdx
	xorq	-36(%rdi),%rbp
	rolq	$62,%rdx
	xorq	68(%rdi),%rcx
	rolq	$55,%rbp
	xorq	12(%rdi),%rax
	rolq	$2,%rcx
	xorq	20(%rdi),%rbx
	xchgq	%rsi,%rdi
	rolq	$39,%rax
	rolq	$41,%rbx
	movq	%rdx,%r13
	andq	%rbp,%rdx
	notq	%rbp
	xorq	%rcx,%rdx
	movq	%rdx,92(%rdi)

	movq	%rax,%r14
	andq	%rbp,%rax
	xorq	%r13,%rax
	movq	%rax,60(%rdi)

	orq	%rcx,%r13
	xorq	%rbx,%r13
	movq	%r13,84(%rdi)

	andq	%rbx,%rcx
	xorq	%r14,%rcx
	movq	%rcx,76(%rdi)

	orq	%r14,%rbx
	xorq	%rbp,%rbx
	movq	%rbx,68(%rdi)

	movq	%rdx,%rbp
	movq	%r13,%rdx

	testq	$255,%r15
	jnz	L$oop

	leaq	-192(%r15),%r15
	.byte	0xf3,0xc3




.p2align	5
KeccakF1600:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	leaq	100(%rdi),%rdi
	subq	$200,%rsp


	notq	-92(%rdi)
	notq	-84(%rdi)
	notq	-36(%rdi)
	notq	-4(%rdi)
	notq	36(%rdi)
	notq	60(%rdi)

	leaq	iotas(%rip),%r15
	leaq	100(%rsp),%rsi

	call	__KeccakF1600

	notq	-92(%rdi)
	notq	-84(%rdi)
	notq	-36(%rdi)
	notq	-4(%rdi)
	notq	36(%rdi)
	notq	60(%rdi)
	leaq	-100(%rdi),%rdi

	addq	$200,%rsp


	popq	%r15

	popq	%r14

	popq	%r13

	popq	%r12

	popq	%rbp

	popq	%rbx

	.byte	0xf3,0xc3


.globl	_SHA3_absorb

.p2align	5
_SHA3_absorb:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	leaq	100(%rdi),%rdi
	subq	$232,%rsp


	movq	%rsi,%r9
	leaq	100(%rsp),%rsi

	notq	-92(%rdi)
	notq	-84(%rdi)
	notq	-36(%rdi)
	notq	-4(%rdi)
	notq	36(%rdi)
	notq	60(%rdi)
	leaq	iotas(%rip),%r15

	movq	%rcx,216-100(%rsi)

L$oop_absorb:
	cmpq	%rcx,%rdx
	jc	L$done_absorb

	shrq	$3,%rcx
	leaq	-100(%rdi),%r8

L$block_absorb:
	movq	(%r9),%rax
	leaq	8(%r9),%r9
	xorq	(%r8),%rax
	leaq	8(%r8),%r8
	subq	$8,%rdx
	movq	%rax,-8(%r8)
	subq	$1,%rcx
	jnz	L$block_absorb

	movq	%r9,200-100(%rsi)
	movq	%rdx,208-100(%rsi)
	call	__KeccakF1600
	movq	200-100(%rsi),%r9
	movq	208-100(%rsi),%rdx
	movq	216-100(%rsi),%rcx
	jmp	L$oop_absorb

.p2align	5
L$done_absorb:
	movq	%rdx,%rax

	notq	-92(%rdi)
	notq	-84(%rdi)
	notq	-36(%rdi)
	notq	-4(%rdi)
	notq	36(%rdi)
	notq	60(%rdi)

	addq	$232,%rsp


	popq	%r15

	popq	%r14

	popq	%r13

	popq	%r12

	popq	%rbp

	popq	%rbx

	.byte	0xf3,0xc3


.globl	_SHA3_squeeze

.p2align	5
_SHA3_squeeze:

	pushq	%r12

	pushq	%r13

	pushq	%r14


	shrq	$3,%rcx
	movq	%rdi,%r8
	movq	%rsi,%r12
	movq	%rdx,%r13
	movq	%rcx,%r14
	jmp	L$oop_squeeze

.p2align	5
L$oop_squeeze:
	cmpq	$8,%r13
	jb	L$tail_squeeze

	movq	(%r8),%rax
	leaq	8(%r8),%r8
	movq	%rax,(%r12)
	leaq	8(%r12),%r12
	subq	$8,%r13
	jz	L$done_squeeze

	subq	$1,%rcx
	jnz	L$oop_squeeze

	call	KeccakF1600
	movq	%rdi,%r8
	movq	%r14,%rcx
	jmp	L$oop_squeeze

L$tail_squeeze:
	movq	%r8,%rsi
	movq	%r12,%rdi
	movq	%r13,%rcx
.byte	0xf3,0xa4

L$done_squeeze:
	popq	%r14

	popq	%r13

	popq	%r12

	.byte	0xf3,0xc3


.p2align	8
.quad	0,0,0,0,0,0,0,0

iotas:
.quad	0x0000000000000001
.quad	0x0000000000008082
.quad	0x800000000000808a
.quad	0x8000000080008000
.quad	0x000000000000808b
.quad	0x0000000080000001
.quad	0x8000000080008081
.quad	0x8000000000008009
.quad	0x000000000000008a
.quad	0x0000000000000088
.quad	0x0000000080008009
.quad	0x000000008000000a
.quad	0x000000008000808b
.quad	0x800000000000008b
.quad	0x8000000000008089
.quad	0x8000000000008003
.quad	0x8000000000008002
.quad	0x8000000000000080
.quad	0x000000000000800a
.quad	0x800000008000000a
.quad	0x8000000080008081
.quad	0x8000000000008080
.quad	0x0000000080000001
.quad	0x8000000080008008

.byte	75,101,99,99,97,107,45,49,54,48,48,32,97,98,115,111,114,98,32,97,110,100,32,115,113,117,101,101,122,101,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
