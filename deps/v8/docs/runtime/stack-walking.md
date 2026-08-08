# Stack Walking in V8

Stack walking is the process of traversing the call stack to inspect active frames. V8 uses stack walking for several critical features:
*   **Exception Handling**: Finding the appropriate catch handler when an exception is thrown.
*   **Stack Traces**: Generating the string representation of the stack for `Error.stack`.
*   **Garbage Collection**: Identifying root pointers held in stack slots.
*   **Profiling**: The CPU profiler samples the stack to determine where time is spent.

## Frame Types

V8 has a rich set of frame types, as execution can transition between interpreted JavaScript, optimized JavaScript, WebAssembly, and C++ host code.

Key frame types (defined in `src/execution/frames.h`):
*   **`ENTRY`**: The entry point from C++ into JavaScript.
*   **`EXIT`**: The exit point from JavaScript back into C++ (e.g., calling a C++ runtime function).
*   **`INTERPRETED`**: A frame for code running in the Ignition interpreter.
*   **`BASELINE`**: A frame for code compiled by Sparkplug.
*   **`MAGLEV`**: A frame for code compiled by Maglev.
*   **`TURBOFAN`**: A frame for code compiled by TurboFan.
*   **`BUILTIN`**: A frame for a builtin function (e.g., written in Torque or CSA).
*   **`WASM`**: Frames for WebAssembly execution.

## The Stack Frame Iterator

The primary mechanism for stack walking in C++ is the `StackFrameIterator` class (and its variants like `DebuggableStackFrameIterator`).

```cpp
for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
  StackFrame* frame = it.frame();
  // Inspect frame...
}
```

### How it Works:
1.  **Starting Point**: The iterator typically starts from the current top of the stack (retrieved from the isolate's saved state or current CPU registers).
2.  **Frame Identification**: V8 identifies the type of frame by looking at the **Frame Marker** (a special value stored in the frame) or by looking up the PC (Program Counter) in the instruction stream of active code objects.
3.  **Navigation**: The iterator uses the saved Frame Pointer (FP) to find the caller's frame. Standard V8 frames maintain a consistent link chain of FPs.

## Standard Frame Layout

Most V8 frames follow a standard layout (though optimized frames may vary to save space):

```text
                      +---------------------------+
                      |  Caller's Frame Pointer   |  <- Higher addresses
                      +---------------------------+
                      |  Caller's Return Address  |
                      +---------------------------+
                      |  Context / Marker         |
                      +---------------------------+
                      |  Function / Code Object   |  <- Current FP points here
                      +---------------------------+
                      |  ... Locals / Stack ...   |  <- SP points to the top
                      +---------------------------+
```

## GC Stack Scanning

During Garbage Collection, V8 must scan the stack to find all root pointers to heap objects. This process relies on the stack walking infrastructure.

### Precise vs. Conservative Scanning
V8 primarily uses **Precise Stack Scanning**. It knows exactly which slots on the stack contain tagged pointers and which contain raw data (like integers or floats).
*   **Tagged Pointers**: V8 uses pointer tagging (typically the lowest bit being 1 for HeapObjects and 0 for Smis). The GC uses this tag to identify pointers that need to be traced and updated if the object moves.
*   **Safepoint Tables**: For optimized code (Maglev and TurboFan), V8 generates **Safepoint Tables**. At specific instruction offsets (safepoints, usually calls), these tables describe exactly which stack slots and registers contain tagged pointers. The GC uses the return address to look up the safepoint table entry.
*   **Unoptimized Code**: For Ignition frames, the GC knows that the slots correspond to bytecode registers, which are all treated as tagged values containing either Smis or HeapObjects.

### Handling Arguments
Arguments to functions can be passed on the stack or in registers, depending on the calling convention and execution tier.
*   **JS Linkage**: For standard JavaScript function calls, arguments are pushed onto the stack by the caller.
*   **Accessing Arguments**: `CommonFrameWithJSLinkage` provides methods like `GetParameter(index)` to access arguments. The location is calculated relative to the frame pointer.
*   **GC Scanning of Arguments**: The GC needs to scan arguments passed on the stack. To do this precisely, V8 frames track the actual number of arguments passed (e.g., via `GetActualArgumentCount()`). This ensures that even with variadic arguments or parameter mismatch, the GC scans exactly the valid range of arguments on the stack.

## Safe Stack Walking

Stack walking must be done safely, especially when handling signals for the CPU profiler or during GC when the stack might be in an inconsistent state.

*   **Profiler Safety**: The CPU profiler interrupts execution at arbitrary points. The stack may be in an inconsistent state (e.g., in the middle of pushing a frame). V8 uses a specialized, safe stack walker (`SafeStackFrameIterator`) that performs sanity checks on pointers before dereferencing them to avoid crashes.

---

## See Also
-   [Exception Handling](exception-handling.md)
-   [Deoptimization](deoptimization.md)
-   [Ignition](../interpreter/interpreter-ignition.md)
