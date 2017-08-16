; inffasx64.asm is a hand tuned assembler version of inffast.c - fast decoding
; version for AMD64 on Windows using Microsoft C compiler
;
; inffasx64.asm is automatically convert from AMD64 portion of inffas86.c
; inffasx64.asm is called by inffas8664.c, which contain more info.


; to compile this file, I use option
;   ml64.exe /Flinffasx64 /c /Zi inffasx64.asm
;   with Microsoft Macro Assembler (x64) for AMD64
;

; This file compile with Microsoft Macro Assembler (x64) for AMD64
;
;   ml64.exe is given with Visual Studio 2005/2008/2010 and Windows WDK
;
;   (you can get Windows WDK with ml64 for AMD64 from
;      http://www.microsoft.com/whdc/Devtools/wdk/default.mspx for low price)
;


.code
inffas8664fnc PROC

; see http://weblogs.asp.net/oldnewthing/archive/2004/01/14/58579.aspx and
; http://msdn.microsoft.com/library/en-us/kmarch/hh/kmarch/64bitAMD_8e951dd2-ee77-4728-8702-55ce4b5dd24a.xml.asp
;
; All registers must be preserved across the call, except for
;   rax, rcx, rdx, r8, r-9, r10, and r11, which are scratch.


	mov [rsp-8],rsi
	mov [rsp-16],rdi
	mov [rsp-24],r12
	mov [rsp-32],r13
	mov [rsp-40],r14
	mov [rsp-48],r15
	mov [rsp-56],rbx

	mov rax,rcx

	mov	[rax+8], rbp       ; /* save regs rbp and rsp */
	mov	[rax], rsp

	mov	rsp, rax          ; /* make rsp point to &ar */

	mov	rsi, [rsp+16]      ; /* rsi  = in */
	mov	rdi, [rsp+32]      ; /* rdi  = out */
	mov	r9, [rsp+24]       ; /* r9   = last */
	mov	r10, [rsp+48]      ; /* r10  = end */
	mov	rbp, [rsp+64]      ; /* rbp  = lcode */
	mov	r11, [rsp+72]      ; /* r11  = dcode */
	mov	rdx, [rsp+80]      ; /* rdx  = hold */
	mov	ebx, [rsp+88]      ; /* ebx  = bits */
	mov	r12d, [rsp+100]    ; /* r12d = lmask */
	mov	r13d, [rsp+104]    ; /* r13d = dmask */
                                          ; /* r14d = len */
                                          ; /* r15d = dist */


	cld
	cmp	r10, rdi
	je	L_one_time           ; /* if only one decode left */
	cmp	r9, rsi

    jne L_do_loop


L_one_time:
	mov	r8, r12           ; /* r8 = lmask */
	cmp	bl, 32
	ja	L_get_length_code_one_time

	lodsd                         ; /* eax = *(uint *)in++ */
	mov	cl, bl            ; /* cl = bits, needs it for shifting */
	add	bl, 32             ; /* bits += 32 */
	shl	rax, cl
	or	rdx, rax          ; /* hold |= *((uint *)in)++ << bits */
	jmp	L_get_length_code_one_time

ALIGN 4
L_while_test:
	cmp	r10, rdi
	jbe	L_break_loop
	cmp	r9, rsi
	jbe	L_break_loop

L_do_loop:
	mov	r8, r12           ; /* r8 = lmask */
	cmp	bl, 32
	ja	L_get_length_code    ; /* if (32 < bits) */

	lodsd                         ; /* eax = *(uint *)in++ */
	mov	cl, bl            ; /* cl = bits, needs it for shifting */
	add	bl, 32             ; /* bits += 32 */
	shl	rax, cl
	or	rdx, rax          ; /* hold |= *((uint *)in)++ << bits */

L_get_length_code:
	and	r8, rdx            ; /* r8 &= hold */
	mov	eax, [rbp+r8*4]  ; /* eax = lcode[hold & lmask] */

	mov	cl, ah            ; /* cl = this.bits */
	sub	bl, ah            ; /* bits -= this.bits */
	shr	rdx, cl           ; /* hold >>= this.bits */

	test	al, al
	jnz	L_test_for_length_base ; /* if (op != 0) 45.7% */

	mov	r8, r12            ; /* r8 = lmask */
	shr	eax, 16            ; /* output this.val char */
	stosb

L_get_length_code_one_time:
	and	r8, rdx            ; /* r8 &= hold */
	mov	eax, [rbp+r8*4] ; /* eax = lcode[hold & lmask] */

L_dolen:
	mov	cl, ah            ; /* cl = this.bits */
	sub	bl, ah            ; /* bits -= this.bits */
	shr	rdx, cl           ; /* hold >>= this.bits */

	test	al, al
	jnz	L_test_for_length_base ; /* if (op != 0) 45.7% */

	shr	eax, 16            ; /* output this.val char */
	stosb
	jmp	L_while_test

ALIGN 4
L_test_for_length_base:
	mov	r14d, eax         ; /* len = this */
	shr	r14d, 16           ; /* len = this.val */
	mov	cl, al

	test	al, 16
	jz	L_test_for_second_level_length ; /* if ((op & 16) == 0) 8% */
	and	cl, 15             ; /* op &= 15 */
	jz	L_decode_distance    ; /* if (!op) */

L_add_bits_to_len:
	sub	bl, cl
	xor	eax, eax
	inc	eax
	shl	eax, cl
	dec	eax
	and	eax, edx          ; /* eax &= hold */
	shr	rdx, cl
	add	r14d, eax         ; /* len += hold & mask[op] */

L_decode_distance:
	mov	r8, r13           ; /* r8 = dmask */
	cmp	bl, 32
	ja	L_get_distance_code  ; /* if (32 < bits) */

	lodsd                         ; /* eax = *(uint *)in++ */
	mov	cl, bl            ; /* cl = bits, needs it for shifting */
	add	bl, 32             ; /* bits += 32 */
	shl	rax, cl
	or	rdx, rax          ; /* hold |= *((uint *)in)++ << bits */

L_get_distance_code:
	and	r8, rdx           ; /* r8 &= hold */
	mov	eax, [r11+r8*4] ; /* eax = dcode[hold & dmask] */

L_dodist:
	mov	r15d, eax         ; /* dist = this */
	shr	r15d, 16           ; /* dist = this.val */
	mov	cl, ah
	sub	bl, ah            ; /* bits -= this.bits */
	shr	rdx, cl           ; /* hold >>= this.bits */
	mov	cl, al            ; /* cl = this.op */

	test	al, 16             ; /* if ((op & 16) == 0) */
	jz	L_test_for_second_level_dist
	and	cl, 15             ; /* op &= 15 */
	jz	L_check_dist_one

L_add_bits_to_dist:
	sub	bl, cl
	xor	eax, eax
	inc	eax
	shl	eax, cl
	dec	eax                 ; /* (1 << op) - 1 */
	and	eax, edx          ; /* eax &= hold */
	shr	rdx, cl
	add	r15d, eax         ; /* dist += hold & ((1 << op) - 1) */

L_check_window:
	mov	r8, rsi           ; /* save in so from can use it's reg */
	mov	rax, rdi
	sub	rax, [rsp+40]      ; /* nbytes = out - beg */

	cmp	eax, r15d
	jb	L_clip_window        ; /* if (dist > nbytes) 4.2% */

	mov	ecx, r14d         ; /* ecx = len */
	mov	rsi, rdi
	sub	rsi, r15          ; /* from = out - dist */

	sar	ecx, 1
	jnc	L_copy_two           ; /* if len % 2 == 0 */

	rep     movsw
	mov	al, [rsi]
	mov	[rdi], al
	inc	rdi

	mov	rsi, r8           ; /* move in back to %rsi, toss from */
	jmp	L_while_test

L_copy_two:
	rep     movsw
	mov	rsi, r8           ; /* move in back to %rsi, toss from */
	jmp	L_while_test

ALIGN 4
L_check_dist_one:
	cmp	r15d, 1            ; /* if dist 1, is a memset */
	jne	L_check_window
	cmp	[rsp+40], rdi      ; /* if out == beg, outside window */
	je	L_check_window

	mov	ecx, r14d         ; /* ecx = len */
	mov	al, [rdi-1]
	mov	ah, al

	sar	ecx, 1
	jnc	L_set_two
	mov	[rdi], al
	inc	rdi

L_set_two:
	rep     stosw
	jmp	L_while_test

ALIGN 4
L_test_for_second_level_length:
	test	al, 64
	jnz	L_test_for_end_of_block ; /* if ((op & 64) != 0) */

	xor	eax, eax
	inc	eax
	shl	eax, cl
	dec	eax
	and	eax, edx         ; /* eax &= hold */
	add	eax, r14d        ; /* eax += len */
	mov	eax, [rbp+rax*4] ; /* eax = lcode[val+(hold&mask[op])]*/
	jmp	L_dolen

ALIGN 4
L_test_for_second_level_dist:
	test	al, 64
	jnz	L_invalid_distance_code ; /* if ((op & 64) != 0) */

	xor	eax, eax
	inc	eax
	shl	eax, cl
	dec	eax
	and	eax, edx         ; /* eax &= hold */
	add	eax, r15d        ; /* eax += dist */
	mov	eax, [r11+rax*4] ; /* eax = dcode[val+(hold&mask[op])]*/
	jmp	L_dodist

ALIGN 4
L_clip_window:
	mov	ecx, eax         ; /* ecx = nbytes */
	mov	eax, [rsp+92]     ; /* eax = wsize, prepare for dist cmp */
	neg	ecx                ; /* nbytes = -nbytes */

	cmp	eax, r15d
	jb	L_invalid_distance_too_far ; /* if (dist > wsize) */

	add	ecx, r15d         ; /* nbytes = dist - nbytes */
	cmp	dword ptr [rsp+96], 0
	jne	L_wrap_around_window ; /* if (write != 0) */

	mov	rsi, [rsp+56]     ; /* from  = window */
	sub	eax, ecx         ; /* eax  -= nbytes */
	add	rsi, rax         ; /* from += wsize - nbytes */

	mov	eax, r14d        ; /* eax = len */
	cmp	r14d, ecx
	jbe	L_do_copy           ; /* if (nbytes >= len) */

	sub	eax, ecx         ; /* eax -= nbytes */
	rep     movsb
	mov	rsi, rdi
	sub	rsi, r15         ; /* from = &out[ -dist ] */
	jmp	L_do_copy

ALIGN 4
L_wrap_around_window:
	mov	eax, [rsp+96]     ; /* eax = write */
	cmp	ecx, eax
	jbe	L_contiguous_in_window ; /* if (write >= nbytes) */

	mov	esi, [rsp+92]     ; /* from  = wsize */
	add	rsi, [rsp+56]     ; /* from += window */
	add	rsi, rax         ; /* from += write */
	sub	rsi, rcx         ; /* from -= nbytes */
	sub	ecx, eax         ; /* nbytes -= write */

	mov	eax, r14d        ; /* eax = len */
	cmp	eax, ecx
	jbe	L_do_copy           ; /* if (nbytes >= len) */

	sub	eax, ecx         ; /* len -= nbytes */
	rep     movsb
	mov	rsi, [rsp+56]     ; /* from = window */
	mov	ecx, [rsp+96]     ; /* nbytes = write */
	cmp	eax, ecx
	jbe	L_do_copy           ; /* if (nbytes >= len) */

	sub	eax, ecx         ; /* len -= nbytes */
	rep     movsb
	mov	rsi, rdi
	sub	rsi, r15         ; /* from = out - dist */
	jmp	L_do_copy

ALIGN 4
L_contiguous_in_window:
	mov	rsi, [rsp+56]     ; /* rsi = window */
	add	rsi, rax
	sub	rsi, rcx         ; /* from += write - nbytes */

	mov	eax, r14d        ; /* eax = len */
	cmp	eax, ecx
	jbe	L_do_copy           ; /* if (nbytes >= len) */

	sub	eax, ecx         ; /* len -= nbytes */
	rep     movsb
	mov	rsi, rdi
	sub	rsi, r15         ; /* from = out - dist */
	jmp	L_do_copy           ; /* if (nbytes >= len) */

ALIGN 4
L_do_copy:
	mov	ecx, eax         ; /* ecx = len */
	rep     movsb

	mov	rsi, r8          ; /* move in back to %esi, toss from */
	jmp	L_while_test

L_test_for_end_of_block:
	test	al, 32
	jz	L_invalid_literal_length_code
	mov	dword ptr [rsp+116], 1
	jmp	L_break_loop_with_status

L_invalid_literal_length_code:
	mov	dword ptr [rsp+116], 2
	jmp	L_break_loop_with_status

L_invalid_distance_code:
	mov	dword ptr [rsp+116], 3
	jmp	L_break_loop_with_status

L_invalid_distance_too_far:
	mov	dword ptr [rsp+116], 4
	jmp	L_break_loop_with_status

L_break_loop:
	mov	dword ptr [rsp+116], 0

L_break_loop_with_status:
; /* put in, out, bits, and hold back into ar and pop esp */
	mov	[rsp+16], rsi     ; /* in */
	mov	[rsp+32], rdi     ; /* out */
	mov	[rsp+88], ebx     ; /* bits */
	mov	[rsp+80], rdx     ; /* hold */

	mov	rax, [rsp]       ; /* restore rbp and rsp */
	mov	rbp, [rsp+8]
	mov	rsp, rax



	mov rsi,[rsp-8]
	mov rdi,[rsp-16]
	mov r12,[rsp-24]
	mov r13,[rsp-32]
	mov r14,[rsp-40]
	mov r15,[rsp-48]
	mov rbx,[rsp-56]

    ret 0
;          :
;          : "m" (ar)
;          : "memory", "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi",
;            "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
;    );

inffas8664fnc 	ENDP
;_TEXT	ENDS
END
