# V8 Sandbox Architecture

This document describes the architecture of the V8 Sandbox, a low-overhead, in-process sandbox designed to protect the rest of the process from typical V8 vulnerabilities.

## Overview

The V8 Sandbox restricts the code executed by V8 to a subset of the process's virtual address space (typically 1TB), called **the sandbox**. All untrusted V8 objects live inside this sandbox. If an attacker exploits a vulnerability to gain arbitrary read/write primitives, they are ideally confined to this sandbox and cannot corrupt memory outside of it.

The sandbox works purely in software by converting raw pointers into either:
1.  **Offsets** from the base of the sandbox.
2.  **Indices** into out-of-sandbox pointer tables (similar to file descriptors in Unix).

## Attacker Model

The sandbox assumes that an attacker can:
*   Arbitrarily and concurrently modify any memory **inside** the sandbox address space.
*   Read memory outside of the sandbox (e.g., via side channels).

The goal is to prevent the attacker from corrupting memory **outside** the sandbox. Any such corruption is considered a sandbox violation.

## Core Mechanisms: Sandboxed Pointers

To maintain isolation, V8 replaces raw pointers with specialized pointer types when referencing objects across the sandbox boundary or even within it for specific security properties.

### 1. Pointers pointing into the Sandbox

*   **Compressed Pointer**: Stored as a 32-bit offset from the start of the 4GB heap area inside the sandbox. Always points into the sandbox.
*   **Sandboxed Pointer**: Used for referencing larger areas like ArrayBuffer backing stores. It is stored as a 40-bit offset from the base of the sandbox, shifted to the left to guarantee it stays within the sandbox size when decoded.

### 2. Pointers pointing outside the Sandbox

To securely reference objects outside the sandbox, V8 uses indirection via pointer tables located outside the sandbox. An attacker can only modify the index (which is inside the sandbox), but the table itself is protected.

*   **External Pointer**: References non-V8 ("external") objects. It is an index into the **ExternalPointerTable (EPT)**. The EPT performs type checks on access to prevent type confusion attacks.
*   **CppHeap Pointer**: References objects on the `CppHeap`. It is an index into the **CppHeapPointerTable**. It uses a different type tagging scheme supporting type hierarchies.
*   **Trusted Pointer**: References a **TrustedObject** (see below) located in trusted space. It is an index into the **TrustedPointerTable (TPT)**.
*   **Code Pointer**: A specialized trusted pointer that always points to a `Code` object, using the **CodePointerTable (CPT)**. This also directly provides the entry point for execution.

### 3. Pointers strictly outside the Sandbox

*   **Protected Pointer**: Used only between `TrustedObjects` in trusted space. They are implemented as compressed pointers but relative to the trusted space base. Neither the pointer nor the object can be modified by an attacker.

## External Pointer Table

The `ExternalPointerTable` (EPT) is a fundamental component of the V8 Sandbox designed to provide memory-safe access to objects located outside the V8 heap (external pointers) from within the sandbox.

### Key Security Mechanisms

1.  **Indirection via Handles**:
    *   Objects inside the sandbox do not store raw pointers to external objects. Instead, they store an `ExternalPointerHandle`, which is an index into the EPT.
    *   This prevents attackers from modifying a pointer in the heap to point to an arbitrary memory location. They can only point to entries already present in the table.

2.  **Type Tagging**:
    *   Each entry in the table consists of a pointer-sized word that includes the raw pointer, a marking bit, and a **type tag** (`ExternalPointerTag`).
    *   The tag is ORed into the unused upper bits of the pointer when it is stored.
    *   When the pointer is loaded, it must be untagged using the expected tag. If the specified tag does not match the actual tag of the entry, the resulting pointer will have invalid top bits and will cause a crash upon dereference. This prevents type-confusion attacks.

3.  **Temporal Memory Safety (Garbage Collection)**:
    *   The table is garbage-collected to ensure that entries either contain valid pointers to live objects or are invalidated.
    *   It uses a marking bit for tracking live entries. The GC sweeps the table, building a freelist from dead entries.

4.  **Managed Resources**:
    *   For external resources whose lifetime is tied to objects inside the V8 heap, the `ManagedResource` class provides a way to explicitly zap (invalidate) the table entry when the external resource is destroyed. This prevents dangling pointers and use-after-free vulnerabilities.

### Caveats
*   **LSan Mode**: When Leak Sanitizer (LSan) is active, the table uses "fat" entries (16 bytes) to store the raw pointer for LSan to see. **This mode is not secure** as it could allow bypassing type checks if an attacker can make a handle point to the raw pointer part of the entry.

## Memory Spaces

*   **The Sandbox**: The large virtual address space reservation containing the standard V8 heap and all untrusted objects.
*   **Trusted Space**: A separate heap space located **outside** the sandbox. It contains sensitive objects like bytecode containers and JIT metadata (`TrustedObject`). Attackers cannot directly write to this space.

## File Structure
*   `src/sandbox/`: Implementation of the sandbox.
*   `src/sandbox/sandbox.h`: Main sandbox class and configuration.
*   `src/sandbox/external-pointer-table.h`: External pointer table implementation.
*   `src/sandbox/code-pointer-table.h`: Code pointer table implementation.
*   `src/sandbox/trusted-pointer-table.h`: Trusted pointer table implementation.
*   `src/sandbox/cppheap-pointer-table.h`: CppHeap pointer table implementation.
