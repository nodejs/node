.text
.align 0x2000
.global __node_text_start
.hidden __node_text_start
#if defined(__ELF__)
.section .note.GNU-stack,"",@progbits
#endif
__node_text_start:
