
_TEXT SEGMENT


ALIGN 16
PUBLIC bn_mul_add_words
bn_mul_add_words PROC
  push rbp
  push rdi
  push rsi
  push rbx
  test r8d, r8d
  jle label0
  xor ebx, ebx
  test r8d, -4
  mov r10, rdx
  je label1
  lea rbp, qword ptr [rcx + 8]
  lea rdi, qword ptr [rcx + 16]
  lea rsi, qword ptr [rcx + 24]
  sub rbp, rdx
  sub rdi, rdx
  sub rsi, rdx
label2:
  sub r8d, 4
  mov rax, r9

  mul qword ptr [r10]
  add rbx, rax
  adc rdx, 0

  mov rax, r9

  add qword ptr [rcx], rbx
  adc rdx, 0

  add rcx, 32
  mov r11, rdx

  mul qword ptr [r10 + 8]
  add r11, rax
  adc rdx, 0

  mov rax, r9

  add qword ptr [r10 + rbp], r11
  adc rdx, 0

  mov rbx, rdx

  mul qword ptr [r10 + 16]
  add rbx, rax
  adc rdx, 0

  mov rax, r9

  add qword ptr [r10 + rdi], rbx
  adc rdx, 0

  mov r11, rdx

  mul qword ptr [r10 + 24]
  add r11, rax
  adc rdx, 0
  add qword ptr [r10 + rsi], r11
  adc rdx, 0

  add r10, 32
  test r8d, -4
  mov rbx, rdx
  jne label2
  test r8d, r8d
  mov r11, rdx
  je label3
label1:
  mov rax, r9

  mul qword ptr [r10]
  add rbx, rax
  adc rdx, 0
  add qword ptr [rcx], rbx
  adc rdx, 0

  sub r8d, 1
  mov r11, rdx
  je label3
  mov rax, r9

  mul qword ptr [r10 + 8]
  add r11, rax
  adc rdx, 0
  add qword ptr [rcx + 8], r11
  adc rdx, 0

  cmp r8d, 1
  mov rbx, rdx
  je label4
  mov rax, r9

  mul qword ptr [r10 + 16]
  add rbx, rax
  adc rdx, 0
  add qword ptr [rcx + 16], rbx
  adc rdx, 0

  mov r11, rdx
label3:
  mov rax, r11
  pop rbx
  pop rsi
  pop rdi
  pop rbp
  ret
label0:
  xor r11d, r11d
  mov rax, r11
  pop rbx
  pop rsi
  pop rdi
  pop rbp
  ret
label4:
  mov r11, rdx
  jmp label3
bn_mul_add_words ENDP


ALIGN 16
PUBLIC bn_mul_words
bn_mul_words PROC
  push rsi
  push rbx
  test r8d, r8d
  mov r10, rdx
  jle label5
  xor ebx, ebx
  test r8d, -4
  je label6
label7:
  sub r8d, 4
  mov rax, r9

  mul qword ptr [r10]
  add rbx, rax
  adc rdx, 0

  mov rax, r9
  mov qword ptr [rcx], rbx
  mov r11, rdx

  mul qword ptr [r10 + 8]
  add r11, rax
  adc rdx, 0

  mov rax, r9
  mov qword ptr [rcx + 8], r11
  mov rbx, rdx

  mul qword ptr [r10 + 16]
  add rbx, rax
  adc rdx, 0

  mov rax, r9
  mov qword ptr [rcx + 16], rbx
  mov r11, rdx

  mul qword ptr [r10 + 24]

  add r10, 32

  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 24], r11
  add rcx, 32
  test r8d, -4
  mov rbx, rdx
  jne label7
  test r8d, r8d
  mov r11, rdx
  jne label6
label8:
  mov rax, r11
  pop rbx
  pop rsi
  ret
label6:
  mov rax, r9

  mul qword ptr [r10]
  add rbx, rax
  adc rdx, 0

  sub r8d, 1
  mov qword ptr [rcx], rbx
  mov r11, rdx
  je label8
  mov rax, r9

  mul qword ptr [r10 + 8]
  add r11, rax
  adc rdx, 0

  cmp r8d, 1
  mov rbx, rdx
  mov qword ptr [rcx + 8], r11
  je label9
  mov rax, r9

  mul qword ptr [r10 + 16]
  add rbx, rax
  adc rdx, 0

  mov r11, rdx
  mov qword ptr [rcx + 16], rbx
  mov rax, r11
  pop rbx
  pop rsi
  ret
label9:
  mov r11, rdx
  jmp label8
label5:
  xor r11d, r11d
  mov rax, r11
  pop rbx
  pop rsi
  ret
bn_mul_words ENDP


ALIGN 16
PUBLIC bn_sqr_words
bn_sqr_words PROC
  test r8d, r8d
  mov r9, rdx
  jle label10
  test r8d, -4
  je label11
label12:
  sub r8d, 4
  mov rax, qword ptr [r9]

  mul rax

  mov qword ptr [rcx + 8], rdx
  mov qword ptr [rcx], rax
  mov rax, qword ptr [r9 + 8]

  mul rax

  mov qword ptr [rcx + 24], rdx
  mov qword ptr [rcx + 16], rax
  mov rax, qword ptr [r9 + 16]

  mul rax

  mov qword ptr [rcx + 40], rdx
  mov qword ptr [rcx + 32], rax
  mov rax, qword ptr [r9 + 24]
  add r9, 32

  mul rax

  mov qword ptr [rcx + 48], rax
  mov qword ptr [rcx + 56], rdx
  add rcx, 64
  test r8d, -4
  jne label12
  test r8d, r8d
  je label10
label11:
  mov rax, qword ptr [r9]

  mul rax

  sub r8d, 1
  mov qword ptr [rcx], rax
  mov qword ptr [rcx + 8], rdx
  je label10
  mov rax, qword ptr [r9 + 8]

  mul rax

  cmp r8d, 1
  mov qword ptr [rcx + 16], rax
  mov qword ptr [rcx + 24], rdx
  je label10
  mov rax, qword ptr [r9 + 16]

  mul rax

  mov qword ptr [rcx + 32], rax
  mov qword ptr [rcx + 40], rdx
label10:
  ret 0
bn_sqr_words ENDP


ALIGN 16
PUBLIC bn_div_words
bn_div_words PROC
  mov rax, rdx
  mov rdx, rcx

  div r8

  ret
bn_div_words ENDP


ALIGN 16
PUBLIC bn_add_words
bn_add_words PROC
  test r9d, r9d
  mov r10, rcx
  jle label13
  mov ecx, r9d

  sub r11, r11
label14:
  mov rax, qword ptr [rdx + r11 * 8]
  adc rax, qword ptr [r8 + r11 * 8]
  mov qword ptr [r10 + r11 * 8], rax
  lea r11, qword ptr [r11 + 1]
  loop label14
  sbb rax, rax

  and eax, 1
  ret
label13:
  xor eax, eax
  ret
bn_add_words ENDP


ALIGN 16
PUBLIC bn_sub_words
bn_sub_words PROC
  test r9d, r9d
  mov r10, rcx
  jle label15
  mov ecx, r9d

  sub r11, r11
label16:
  mov rax, qword ptr [rdx + r11 * 8]
  sbb rax, qword ptr [r8 + r11 * 8]
  mov qword ptr [r10 + r11 * 8], rax
  lea r11, qword ptr [r11 + 1]
  loop label16
  sbb rax, rax

  and eax, 1
  ret
label15:
  xor eax, eax
  ret
bn_sub_words ENDP


ALIGN 16
PUBLIC bn_mul_comba8
bn_mul_comba8 PROC
  push rsi
  push rbx
  xor r10d, r10d
  mov rax, qword ptr [rdx]
  mov r9, rdx
  mov rsi, r10
  mov r11, r10

  mul qword ptr [r8]

  mov rbx, r10

  add rsi, rax
  adc rdx, 0

  mov qword ptr [rcx], rsi
  mov rax, qword ptr [r9]

  add r11, rdx
  adc rbx, 0
  mul qword ptr [r8 + 8]

  mov rsi, r11
  mov r11, r10

  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8]
  add rsi, rax
  adc rdx, 0

  mov qword ptr [rcx + 8], rsi
  mov rsi, r10
  mov rax, qword ptr [r9 + 16]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 8]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 16]
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 16], rbx
  mov rbx, r10
  mov rax, qword ptr [r9]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 24]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 16]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 16]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 8]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8]
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 24], r11
  mov rax, qword ptr [r9 + 32]
  mov r11, r10

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 8]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 16]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 16]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 24]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 32]
  add rsi, rax
  adc rdx, 0

  mov qword ptr [rcx + 32], rsi
  mov rsi, r10
  mov rax, qword ptr [r9]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 40]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 32]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 16]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 24]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 16]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 32]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 8]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 40]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8]
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 40], rbx
  mov rax, qword ptr [r9 + 48]
  mov rbx, r10

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 40]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 8]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 32]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 16]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 24]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 16]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 32]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 40]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 48]
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 48], r11
  mov r11, r10
  mov rax, qword ptr [r9]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 56]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 48]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 16]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 40]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 32]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 32]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 24]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 40]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 16]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 48]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 8]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 56]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8]
  add rsi, rax
  adc rdx, 0

  mov qword ptr [rcx + 56], rsi
  mov rax, qword ptr [r9 + 56]
  mov rsi, r10

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 8]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 48]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 16]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 40]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 24]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 32]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 32]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 40]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 16]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 48]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 56]
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 64], rbx
  mov rbx, r10
  mov rax, qword ptr [r9 + 16]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 56]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 48]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 32]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 40]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 40]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 32]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 48]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 24]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 56]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 16]
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 72], r11
  mov rax, qword ptr [r9 + 56]
  mov r11, r10

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 24]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 48]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 32]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 40]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 40]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 32]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 48]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 56]
  add rsi, rax
  adc rdx, 0

  mov qword ptr [rcx + 80], rsi
  mov rsi, r10
  mov rax, qword ptr [r9 + 32]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 56]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 40]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 48]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 48]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 40]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 56]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 32]
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 88], rbx
  mov rbx, r10
  mov rax, qword ptr [r9 + 56]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 40]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 48]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 48]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 40]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 56]
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 96], r11
  mov rax, qword ptr [r9 + 48]
  mov r11, r10

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 56]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 56]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 48]
  add rsi, rax
  adc rdx, 0
  add rbx, rdx
  adc r11, 0

  mov qword ptr [rcx + 104], rsi
  mov rax, qword ptr [r9 + 56]

  mul qword ptr [r8 + 56]

  mov r8, rbx

  add r8, rax
  adc rdx, 0
  add r11, rdx
  adc r10, 0

  mov qword ptr [rcx + 112], r8
  mov qword ptr [rcx + 120], r11
  pop rbx
  pop rsi
  ret
bn_mul_comba8 ENDP


ALIGN 16
PUBLIC bn_mul_comba4
bn_mul_comba4 PROC
  push rsi
  push rbx
  xor r10d, r10d
  mov rax, qword ptr [rdx]
  mov r9, rdx
  mov rbx, r10
  mov r11, r10

  mul qword ptr [r8]

  mov rsi, r10

  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx], rbx
  mov rbx, r10
  mov rax, qword ptr [r9]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 8]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8]
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 8], r11
  mov r11, r10
  mov rax, qword ptr [r9 + 16]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 8]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 16]
  add rsi, rax
  adc rdx, 0

  mov qword ptr [rcx + 16], rsi
  mov rsi, r10
  mov rax, qword ptr [r9]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 24]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 16]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 16]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 8]
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8]
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 24], rbx
  mov rax, qword ptr [r9 + 24]
  mov rbx, r10

  add r11, rdx
  adc rsi, 0
  mul qword ptr [r8 + 8]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 16]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 16]
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 8]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 24]
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 32], r11
  mov r11, r10
  mov rax, qword ptr [r9 + 16]

  add rsi, rdx
  adc rbx, 0
  mul qword ptr [r8 + 24]
  add rsi, rax
  adc rdx, 0

  mov rax, qword ptr [r9 + 24]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 16]
  add rsi, rax
  adc rdx, 0
  add rbx, rdx
  adc r11, 0

  mov qword ptr [rcx + 40], rsi
  mov rax, qword ptr [r9 + 24]

  mul qword ptr [r8 + 24]

  mov r8, rbx

  add r8, rax
  adc rdx, 0
  add r11, rdx
  adc r10, 0

  mov qword ptr [rcx + 48], r8
  mov qword ptr [rcx + 56], r11
  pop rbx
  pop rsi
  ret
bn_mul_comba4 ENDP


ALIGN 16
PUBLIC bn_sqr_comba8
bn_sqr_comba8 PROC
  push rsi
  push rbx
  xor r9d, r9d
  mov rax, qword ptr [rdx]
  mov r8, rdx
  mov rbx, r9
  mov r10, r9

  mul rax

  mov r11, r9
  mov rsi, r9

  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx], rbx
  mov rax, qword ptr [r8 + 8]

  add r10, rdx
  adc r11, 0
  mul qword ptr [r8]

  mov rbx, r10

  add rdx, rdx
  adc rsi, 0
  add rax, rax
  adc rdx, 0

  mov r10, rsi

  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 8], rbx
  mov rbx, r9
  mov rax, qword ptr [r8 + 8]

  add r11, rdx
  adc r10, 0
  mul rax
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 16]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 16], r11
  mov r11, r9
  mov rax, qword ptr [r8 + 24]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 16]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 8]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov qword ptr [rcx + 24], r10
  mov r10, r9
  mov rax, qword ptr [r8 + 16]

  add rbx, rdx
  adc r11, 0
  mul rax
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 24]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8 + 8]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 32]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 32], rbx
  mov rbx, r9
  mov rax, qword ptr [r8 + 40]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 32]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8 + 8]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 24]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8 + 16]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 40], r11
  mov rax, qword ptr [r8 + 24]
  mov r11, r9

  add r10, rdx
  adc rbx, 0
  mul rax
  add r10, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 32]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 16]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 40]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 8]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 48]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov qword ptr [rcx + 48], r10
  mov r10, r9
  mov rax, qword ptr [r8 + 56]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 48]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8 + 8]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 40]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8 + 16]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 32]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8 + 24]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 56], rbx
  mov rbx, r9
  mov rax, qword ptr [r8 + 32]

  add r11, rdx
  adc r10, 0
  mul rax
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 40]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8 + 24]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 48]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8 + 16]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 56]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8 + 8]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 64], r11
  mov rax, qword ptr [r8 + 56]
  mov r11, r9

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8 + 16]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 48]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 24]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 40]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 32]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov qword ptr [rcx + 72], r10
  mov r10, r9
  mov rax, qword ptr [r8 + 40]

  add rbx, rdx
  adc r11, 0
  mul rax
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 48]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8 + 32]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 56]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8 + 24]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 80], rbx
  mov rbx, r9
  mov rax, qword ptr [r8 + 56]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8 + 32]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 48]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8 + 40]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 88], r11
  mov r11, r9
  mov rax, qword ptr [r8 + 48]

  add r10, rdx
  adc rbx, 0
  mul rax
  add r10, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 56]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 40]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov qword ptr [rcx + 96], r10
  mov r10, r9
  mov rax, qword ptr [r8 + 56]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 48]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0
  add r11, rdx
  adc r10, 0

  mov qword ptr [rcx + 104], rbx
  mov rax, qword ptr [r8 + 56]
  mov r8, r11

  mul rax
  add r8, rax
  adc rdx, 0
  add r10, rdx
  adc r9, 0

  mov qword ptr [rcx + 112], r8
  mov qword ptr [rcx + 120], r10
  pop rbx
  pop rsi
  ret
bn_sqr_comba8 ENDP


ALIGN 16
PUBLIC bn_sqr_comba4
bn_sqr_comba4 PROC
  push rbx
  xor r9d, r9d
  mov rax, qword ptr [rdx]
  mov r8, rdx
  mov r11, r9
  mov r10, r9

  mul rax

  mov rbx, r9

  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx], r11
  mov r11, r9
  mov rax, qword ptr [r8 + 8]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0

  mov qword ptr [rcx + 8], r10
  mov r10, r9
  mov rax, qword ptr [r8 + 8]

  add rbx, rdx
  adc r11, 0
  mul rax
  add rbx, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 16]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0

  mov qword ptr [rcx + 16], rbx
  mov rbx, r9
  mov rax, qword ptr [r8 + 24]

  add r11, rdx
  adc r10, 0
  mul qword ptr [r8]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 16]

  add r10, rdx
  adc rbx, 0
  mul qword ptr [r8 + 8]
  add rdx, rdx
  adc rbx, 0
  add rax, rax
  adc rdx, 0
  add r11, rax
  adc rdx, 0

  mov qword ptr [rcx + 24], r11
  mov r11, r9
  mov rax, qword ptr [r8 + 16]

  add r10, rdx
  adc rbx, 0
  mul rax
  add r10, rax
  adc rdx, 0

  mov rax, qword ptr [r8 + 24]

  add rbx, rdx
  adc r11, 0
  mul qword ptr [r8 + 8]
  add rdx, rdx
  adc r11, 0
  add rax, rax
  adc rdx, 0
  add r10, rax
  adc rdx, 0
  add rbx, rdx
  adc r11, 0

  mov qword ptr [rcx + 32], r10
  mov r10, r9
  mov rax, qword ptr [r8 + 24]

  mul qword ptr [r8 + 16]
  add rdx, rdx
  adc r10, 0
  add rax, rax
  adc rdx, 0
  add rbx, rax
  adc rdx, 0
  add r11, rdx
  adc r10, 0

  mov qword ptr [rcx + 40], rbx
  mov rax, qword ptr [r8 + 24]
  mov r8, r11

  mul rax
  add r8, rax
  adc rdx, 0
  add r10, rdx
  adc r9, 0

  mov qword ptr [rcx + 48], r8
  mov qword ptr [rcx + 56], r10
  pop rbx
  ret
bn_sqr_comba4 ENDP


_TEXT ENDS

END
