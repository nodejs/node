#if defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
// Add .note.gnu.property note for x86_64 to enable Intel CET
// Based on: https://sourceware.org/annobin/annobin.html/Test-cf-protection.html
// Refs: https://github.com/nodejs/node/issues/59084
#if defined(__x86_64__) || defined(_M_X64)
.section	.note.gnu.property,"a"
.align 8
.long	 1f - 0f
.long	 4f - 1f
.long	 5
0:
.string	 "GNU"
1:
.align 8
.long	 0xc0000002
.long	 3f - 2f
2:
.long	 0x3
3:
.align 8
4:
#endif
.text
.align 0x2000
.global __node_text_start
.hidden __node_text_start
__node_text_start:
