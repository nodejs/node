.text	

.globl	rsaz_avx512ifma_eligible
.type	rsaz_avx512ifma_eligible,@function
rsaz_avx512ifma_eligible:
	xorl	%eax,%eax
	.byte	0xf3,0xc3
.size	rsaz_avx512ifma_eligible, .-rsaz_avx512ifma_eligible

.globl	RSAZ_amm52x20_x1_256
.globl	RSAZ_amm52x20_x2_256
.globl	extract_multiplier_2x20_win5
.type	RSAZ_amm52x20_x1_256,@function
RSAZ_amm52x20_x1_256:
RSAZ_amm52x20_x2_256:
extract_multiplier_2x20_win5:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3
.size	RSAZ_amm52x20_x1_256, .-RSAZ_amm52x20_x1_256
	.section ".note.gnu.property", "a"
	.p2align 2
	.long 1f - 0f
	.long 4f - 1f
	.long 5
0:
	# "GNU" encoded with .byte, since .asciz isn't supported
	# on Solaris.
	.byte 0x47
	.byte 0x4e
	.byte 0x55
	.byte 0
1:
	.p2align 2
	.long 0xc0000002
	.long 3f - 2f
2:
	.long 3
3:
	.p2align 2
4:
