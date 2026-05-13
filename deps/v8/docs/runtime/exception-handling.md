# Exception Handling in V8

Exception handling in V8 is implemented using side-tables rather than explicit bytecodes for entering and leaving `try` blocks. This ensures zero overhead for the "happy path" (when no exception is thrown).

## The Handler Table

The core data structure for exception handling is the `HandlerTable` (defined in `src/codegen/handler-table.h`). It maps code regions or specific instructions to their corresponding exception handlers.

There are two flavors of handler tables, depending on the execution tier:

### 1. Range-Based Handler Table (Unoptimized Code)

Used by **Ignition** (interpreter) and **Sparkplug** (baseline compiler). It is stored as a `TrustedByteArray` attached to the `BytecodeArray`.

Since unoptimized code is relatively linear and compact, it is efficient to use ranges to describe `try` blocks.

**Layout:**
An array of 4-element entries:
`[ range-start , range-end , handler-offset , handler-data ]`

*   **range-start**: The bytecode offset where the `try` block begins.
*   **range-end**: The bytecode offset where the `try` block ends.
*   **handler-offset**: The bytecode offset of the `catch` or `finally` block. It also encodes prediction information (whether it's likely to catch the exception) and a flag indicating if the handler was used.
*   **handler-data**: The index of the context register.

**Zero-Overhead Try Blocks:**
When generating bytecode for a `try-catch` statement, the `BytecodeGenerator` does not emit any bytecodes to "enter" the try block. Instead, it calls `MarkTryBegin` and `MarkTryEnd` on the `BytecodeArrayBuilder`, which records the offsets in the `HandlerTableBuilder`. The execution proceeds linearly.

### 2. Return-Address-Based Handler Table (Optimized Code)

Used by **Maglev** and **TurboFan**. It is stored directly in the instruction stream of the generated code.

Optimized code can be heavily reordered and scheduled, making ranges impractical. Instead, V8 tracks specific instructions that can throw (primarily calls) and maps their return addresses to handlers.

**Layout:**
An array of 2-element entries:
`[ return-address-offset , handler-offset ]`

*   **return-address-offset**: The offset from the start of the code where a call returns. If the call throws, the return address is used to look up the handler.
*   **handler-offset**: The offset in the generated code where the handler starts.

## Exception Flow across Tiers

### Ignition
1.  **Try Entry**: No operation.
2.  **Throw**: The `Throw` or `ReThrow` bytecode is executed, or a C++ runtime function throws an exception.
3.  **Lookup**: The runtime looks up the current bytecode offset in the `BytecodeArray`'s `HandlerTable` using `LookupHandlerIndexForRange`.
4.  **Unwinding**: If found, the interpreter restores the context (saved in a register at `try` entry) and jumps to the handler offset. If not found, it unwinds the stack frame and continues searching in the caller.

### Sparkplug
Sparkplug shares the `BytecodeArray` with Ignition and therefore reuses the same range-based `HandlerTable`. When an exception occurs in Sparkplug code, it can often be handled directly in Sparkplug or it might deoptimize to Ignition to handle it.

### Maglev & TurboFan
1.  **Try Entry**: No operation.
2.  **Throw**: An instruction (like a call) throws an exception.
3.  **Lookup**: The runtime uses the return address of the throwing call to look up the handler in the code object's return-address-based `HandlerTable` using `LookupReturn`.
4.  **Unwinding**: The runtime unwinds the stack to the handler and resumes execution there.

## Lazy Deoptimization on Throw

In optimized code (Maglev and TurboFan), V8 sometimes uses a mechanism called **Lazy Deopt on Throw** for certain calls.

Instead of generating complex exception handling code (catch blocks) in the optimized code itself, V8 can mark a call descriptor with `LazyDeoptOnThrow::kYes`.

**How it works:**
1.  **Sentinel Handler**: In the return-address-based handler table, instead of mapping to a real handler offset, the entry for that call maps to a special sentinel value (`kLazyDeoptOnThrowSentinel` which is -1).
2.  **Exception Trigger**: If the called function throws an exception, the runtime exception search looks up the handler.
3.  **Deopt Trigger**: When it finds the sentinel value, it knows that it should not branch to a catch block in the optimized code. Instead, it triggers a **Lazy Deoptimization** of the frame.
4.  **Interpreter Handles It**: The frame is translated back to an Ignition interpreter frame, and the exception is handled by Ignition's standard range-based handler table lookup.

This simplifies optimized code generation for operations where exceptions are rare or where generating optimized catch blocks would be too costly.

## Catch Prediction
V8 uses "Catch Prediction" to guess whether an exception will be caught locally. This is used by the debugger to pause on uncaught exceptions.
*   `CAUGHT`: The exception is caught by a local `catch` block.
*   `UNCAUGHT`: The exception will (likely) rethrow the exception to outside the code boundary.
*   `PROMISE`: The exception will be caught and cause a promise rejection.
*   `ASYNC_AWAIT`: The exception will be caught and cause a promise rejection in the desugaring of an async function.
*   `UNCAUGHT_ASYNC_AWAIT`: The exception will be caught and cause a promise rejection in the desugaring of an async REPL script.

> [!NOTE]
> We cannot perform exception prediction on optimized code directly. Instead, V8 uses `FrameSummary` to find the corresponding code offset in unoptimized code to perform prediction there.

---

## See Also
-   [Stack Walking](stack-walking.md)
-   [Ignition](../interpreter/interpreter-ignition.md)
-   [Function Architecture](function-architecture.md)
