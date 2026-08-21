# Assembler and MacroAssembler Architecture

This document explains V8's low-level code generation abstractions: the `Assembler` and `MacroAssembler`. These are used by all compilers (Sparkplug, Maglev, TurboFan) and hand-written builtins to emit machine code.

## The Layered Approach

V8 uses a multi-layered approach to code generation to balance performance, platform independence, and ease of use.

### 1. `AssemblerBase` and `Assembler`

At the lowest level is the `Assembler` (defined per architecture, e.g., `src/codegen/x64/assembler-x64.h`).

*   **`AssemblerBase`**: Provides common functionality like managing the instruction buffer, relocation information (reloc info), and code comments.
*   **`Assembler`**: Implements the actual instruction encoding for a specific architecture. For example, on x64, calling `movq(rax, rbx)` directly emits the corresponding bytes into the buffer.
*   **Role**: It is a 1:1 mapping to machine instructions. It does not perform any abstraction or simplification.

### 2. `MacroAssembler`

The `MacroAssembler` (also defined per architecture, e.g., `src/codegen/x64/macro-assembler-x64.h`) inherits from `Assembler` (via `MacroAssemblerBase` and often via a shared intermediate class like `SharedMacroAssembler` for related architectures).

*   **`MacroAssemblerBase`**: Contains platform-independent macro-assembler functionality (e.g., loading constants, root-relative addressing).
*   **Role**: It provides higher-level "macro" instructions and common idioms that are composed of multiple raw instructions.
*   **Abstractions**:
    *   **Runtime Calls**: `CallRuntime(Runtime::k...)` handles the complexity of setting up arguments and calling a C++ runtime function.
    *   **Frame Setup**: Methods to push/pop standard frame headers.
    *   **Pseudo-instructions**: Instructions that might not exist directly on the hardware but are common (e.g., `Push` on architectures that don't have a dedicated push instruction).

## How They Are Used

### In Compilers
Compilers like Maglev use the `MacroAssembler` in their `GenerateCode` methods.

```cpp
void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  __ addl(left, right); // Raw assembler call via MacroAssembler
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this); // MacroAssembler helper
}
```
*(Note: `MaglevAssembler` inherits from `MacroAssembler` and adds Maglev-specific helpers).*

### In Builtins
Hand-written assembler builtins are written directly using the `MacroAssembler`.

## The Instruction Buffer and Relocation

The `Assembler` writes bytes into a growable buffer. Because code often contains references to objects or labels that are not yet fixed in memory, V8 uses **Relocation Information** (`RelocInfo`).

*   When the assembler emits a reference to a HeapObject or another code chunk, it records the position and the type of reference.
*   When the code is finalized and moved to its permanent location in the heap, V8 "relocates" these references, patching the code with the actual memory addresses.

---

## See Also
-   `src/codegen/assembler.h`: Base class definitions.
-   `src/codegen/x64/assembler-x64.h`: x64 specific assembler.
-   `src/codegen/x64/macro-assembler-x64.h`: x64 specific macro-assembler.
