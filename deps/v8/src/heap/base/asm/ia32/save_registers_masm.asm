;; Copyright 2020 the V8 project authors. All rights reserved.
;; Use of this source code is governed by a BSD-style license that can be
;; found in the LICENSE file.

;; MASM syntax
;; https://docs.microsoft.com/en-us/cpp/assembler/masm/microsoft-macro-assembler-reference?view=vs-2019

.model flat, C

public SaveCalleeSavedRegisters

.code
    ;; Save all callee-saved registers in the specified buffer.
    ;; extern "C" void SaveCalleeSavedRegisters(intptr_t* buffer);
    ;;
    ;; The following assumes cdecl calling convention.
    ;; Source: https://docs.microsoft.com/en-us/cpp/cpp/cdecl?view=vs-2019

SaveCalleeSavedRegisters:
    ;; 8: [ intptr_t* buffer ]
    ;; 4: [ ret              ]
    ;; 0: [ saved %ebp       ]
    ;; %ebp is callee-saved. Maintain proper frame pointer for debugging.
    push ebp
    mov ebp, esp
    ;; Load the buffer's address in %ecx.
    mov ecx, [ebp + 8]
    ;; Save the callee-saved registers.
    mov [ecx], ebx
    mov [ecx + 4], esi
    mov [ecx + 8], edi
    ;; Restore %ebp as it was used as frame pointer and return.
    pop ebp
    ret

end
