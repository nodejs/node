#if defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
.text
.align 0x2000
.global __node_text_start
.hidden __node_text_start
__node_text_start:
