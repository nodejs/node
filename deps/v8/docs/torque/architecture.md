# Torque Architecture

This document describes Torque, V8's domain-specific language for implementing built-in functions.

## Overview

Torque is a language developed specifically for V8 to make it easier and safer to write built-in functions (like `Array.prototype.map`, `Promise.prototype.then`, etc.).

Before Torque, builtins were written in:
1.  **C++**: Slow for execution because of call overhead from JS.
2.  **Platform-specific Assembly**: Fast, but hard to maintain and error-prone.
3.  **CodeStubAssembler (CSA)**: A C++ API to generate machine code. Safer than raw assembly, but still verbose and hard to read.

Torque provides a higher-level, strongly-typed syntax that compiles down to CSA or the newer Turboshaft Assembler (TSA).

## Torque Compilation Pipeline

The Torque compiler is a standalone executable (built from `src/torque/`) that runs during the V8 build process. The translation from `.tq` to C++ involves several stages:

### 1. Parsing
*   Source files are read and parsed by the `TorqueParser` (`src/torque/torque-parser.cc`).
*   Torque uses an **Earley parser** implementation to handle its grammar.
*   This stage produces an Abstract Syntax Tree (AST).

### 2. Declaration and Type Analysis
*   The compiler performs multiple passes over the AST to resolve types and declarations.
*   **Predeclaration**: Handles type declarations first to allow mutual recursion.
*   **Declaration**: Processes macros, builtins, and constants.
*   **Type Finalization**: `TypeOracle` manages and finalizes types, including resolving class fields and calculating layout offsets.

### 3. Intermediate Representation (IR) Generation
*   The `ImplementationVisitor` (`src/torque/implementation-visitor.cc`) translates the AST into a **Control Flow Graph (CFG)**.
*   The CFG consists of basic blocks containing low-level Torque instructions (e.g., `Branch`, `Goto`, `CallBuiltin`).

### 4. Code Generation
Torque supports multiple backends, with some differences in how they process the IR:
*   **CSA Backend**: `CSAGenerator` iterates over the generated CFG instructions and translates them into C++ code that calls V8's `CodeStubAssembler` (CSA) API. This produces `*-tq-csa.cc` and `*-tq-csa.h` files.
*   **CC Backend**: `CCGenerator` also iterates over the CFG to generate standard C++ code for class definitions, debug readers, and verifiers, producing `*-tq.inc`, `*-tq-inl.inc`, and `*-tq.cc` files.
*   **TSA Backend**: Unlike the others, the current experimental TSA backend (`TSAGenerator`) visits the AST directly (using `AstVisitor`) to generate code using the **Turboshaft Assembler** API, bypassing the CFG generation in `ImplementationVisitor`.

The generated C++ files are placed in the build output directory (e.g., `out/x64.debug/gen/torque-generated/`) and are compiled into the V8 binary.

## Key Concepts

### Types
Torque has a strong type system that understands V8's object model:
*   **Tagged Types**: Represent pointers to GC-managed objects (e.g., `Object`, `Smi`, `JSArray`).
*   **Untagged Types**: Represent raw machine types (e.g., `intptr`, `int32`, `bool`).
*   **Structs**: Can be defined to group related data.

### Macros and Builtins
*   **Macros**: Inlined functions in Torque. They are expanded by the Torque compiler.
*   **Builtins**: Functions that can be called from JavaScript or other builtins. They have a specific calling convention.

### Labels and Control Flow
Torque does not have traditional exceptions. Instead, it uses **Labels** for non-local control flow. Macros can take labels as arguments and "emit" them to transfer control to a handler in the caller. This maps efficiently to machine code jumps.

## Example

Here is a conceptual example of a Torque macro:

```torque
// Torque definition
macro IsZero(x: int32): bool {
  if (x == 0) {
    return true;
  } else {
    return false;
  }
}
```

The CSA backend might generate C++ code similar to this:

```cpp
// Generated CSA C++ code
TNode<BoolT> IsZero(CodeStubAssembler* ca_, TNode<Int32T> x) {
  compiler::CodeAssemblerLabel label_true(ca_);
  compiler::CodeAssemblerLabel label_false(ca_);
  
  ca_->Branch(ca_->Word32Equal(x, ca_->Int32Constant(0)), &label_true, &label_false);
  
  ca_->Bind(&label_true);
  return ca_->TrueConstant();
  
  ca_->Bind(&label_false);
  return ca_->FalseConstant();
}
```

## Transition to Turboshaft (TSA)

With the introduction of the Turboshaft compiler, V8 is transitioning Torque to generate **Turboshaft Assembler (TSA)** code instead of CSA.
*   **Current Status**: The TSA backend is experimental and gated by the `V8_ENABLE_EXPERIMENTAL_TQ_TO_TSA` macro.
*   **Implementation**: Unlike the CSA backend which operates on a CFG, the current TSA implementation (`src/torque/tsa-generator.cc`) acts as an `AstVisitor` and generates code directly from the AST.

---

## See Also
-   [Torque User Manual](user-manual.md)
-   `src/torque/`: Torque compiler source code.
-   `src/builtins/`: Torque builtin definitions.
-   [Turboshaft](../compiler/turboshaft/compiler-turboshaft.md)
