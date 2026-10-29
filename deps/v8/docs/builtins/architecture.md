# V8 Builtins Architecture

Builtins are the pre-compiled functions provided by V8 that implement JavaScript's standard library (e.g., `Array.prototype.map`, `Object.create`) and internal runtime helpers.

## Flavors of Builtins

V8 implements builtins in several different ways, balancing performance, ease of writing, and platform independence.

### 1. Torque Builtins

Torque is V8's custom, strongly-typed domain-specific language. It is the modern way to write builtins. See [Torque Architecture](../torque/architecture.md) for details.
*   **Files**: `.tq` files in `src/builtins/`.
*   **How it works**: The Torque compiler translates `.tq` files into C++ CodeStubAssembler (CSA) code (compiled by TurboFan) or Turboshaft reducers (compiled by Turboshaft). V8 is currently transitioning from CSA to Turboshaft for builtins. Torque also generates C++ headers that are used by both C++ and CSA code.
*   **Benefits**: Strong typing, automatic generation of call descriptors, and safety checks.

### 2. CodeStubAssembler (CSA) Builtins

CSA is a low-level C++ API that allows writing code that directly generates TurboFan graphs. See [CodeStubAssembler](../codegen/code-stub-assembler.md) for details.
*   **Files**: `builtins-*-gen.cc` and `.h` files in `src/builtins/`.
*   **How it works**: CSA provides a programmatic way to emit TurboFan nodes (e.g., `Parameter`, `Load`, `Branch`, `CallRuntime`). It bypasses the JavaScript parser and AST, and generates machine code directly via TurboFan's backend.
*   **Benefits**: Highly efficient, platform-independent (TurboFan handles the machine code generation), and has access to low-level V8 internals.

### 3. C++ Builtins (Runtime)

Standard C++ functions that can be called from JavaScript or other builtins.
*   **Files**: `builtins-*.cc` files in `src/builtins/`.
*   **How it works**: These are normal C++ functions registered with V8. They are called via a special runtime call mechanism.
*   **Benefits**: Easiest to write, full access to C++ library and V8 complex internals.
*   **Drawbacks**: Slower than Torque or CSA because of the boundary crossing between JS and C++.

### 4. Architecture-Specific Assembler Builtins

Low-level builtins written using V8's `MacroAssembler` in C++ for specific architectures.
*   **Files**: `src/builtins/[arch]/builtins-[arch].cc`.
*   **How it works**: Developers write C++ code that uses V8's `MacroAssembler` to emit raw machine instructions.
*   **Benefits**: Ultimate performance, used for low-level entry points, trampolines, and operations where every instruction counts.
*   **Constraint**: Architecture-specific builtins must be used for any operations that need to manipulate the stack (e.g., pushing arguments, setting up the interpreter stack frame) because TurboFan code assumes a fixed stack layout.
*   **Drawbacks**: Must be written separately for every supported architecture (x64, arm, arm64, etc.).

## How Builtins are Loaded

Builtins are compiled during V8's build process (or at startup if snapshots are not used) and are stored in the **Snapshot**. This ensures that V8 starts up with all standard library functions ready to execute without needing to parse or compile them.

They are shared across all Isolates and contexts in a single process.

## Embedding and Copying

To reduce startup time and memory sharing across Isolates, V8 **embeds** builtins into the binary.

*   **Embedded Blob**: Builtins are compiled and written into a dedicated section of the binary (the embedded blob).
*   **Sharing**: This allows multiple Isolates in the same process to share the same executable code for builtins, saving memory.

### Copying Out (Remapping)

Under certain conditions, V8 may **copy** or **remap** the embedded builtins into the `CodeRange` of a specific Isolate:

*   **Short Builtin Calls**: On some architectures (like x64 and arm64), function calls have a limited relative range. If generated code in the `CodeRange` needs to call a builtin, and the embedded blob is too far away in memory, V8 cannot use efficient PC-relative calls.
*   **The Solution**: If `v8_flags.short_builtin_calls` is enabled (often depending on available physical memory), V8 will copy or remap the embedded builtins into the `CodeRange` so they are close to the generated code.
*   **Remapping vs Copying**:
    *   If the OS supports it (e.g., via page remapping), V8 will **remap** the pages to avoid making them private dirty memory, keeping them shared.
    *   If not supported, V8 falls back to **copying** them via `memcpy`, which increases private memory usage.

---

## See Also
-   `docs/torque/architecture.md`: Deep dive into Torque.
-   `src/builtins/`: Source directory for builtins.
