---
name: header-cycle-breaker
description: Guide for breaking cyclic dependencies in V8 headers and resolving cascade IWYU failures.
---

# Header Cycle Breaker Skill

This skill outlines the systematic approach to breaking cyclic dependencies in
V8 headers, specifically focusing on moving heavy inline includes to more
localized `.cc` files or non-inline headers, and resolving the resulting cascade
of "undefined inline function" errors (IWYU - Include What You Use style).

## When to Use

Use this skill when:

- You are tasked with breaking a header cycle in V8 (e.g., core-to-wasm cycle).
- You need to remove an `-inl.h` include from another `-inl.h` or `.h` file.
- You encounter a flood of `[-Werror,-Wundefined-inline]` errors after modifying
  a heavy header.

## Workflow

### Phase 1: Identification and Cycle Breaking

1. **Identify the cycle**: Use tools or subagents to map the include graph and
   find the cyclic path.
2. **Plan the break**: Identify a point where an `-inl.h` can be replaced with
   its non-inline counterpart (`.h`) or where an include can be moved entirely
   to implementation files.
3. **Apply the break**: Modify the header file to break the cycle.

### Phase 2: Resolving the Cascade (IWYU)

Breaking a heavy include chain will cause many implementation files (`.cc`) and
even generated files to fail because they were relying on transitive includes
for inline definitions.

1. **Do not panic**: Expect tens or hundreds of compilation errors initially.
2. **Aggressive Delegation**: Spawn subagents for independent file failures.
   Prompt them to identify missing headers for specific undefined symbols.
3. **IWYU Application**: For each failing file:
   - Identify which inline method is undefined.
   - Find which `-inl.h` file defines it.
   - Include that `-inl.h` file directly in the failing `.cc` file, preferably
     within appropriate guards (e.g., `#if V8_ENABLE_WEBASSEMBLY`).
4. **Repeat until clean**: Build, fix, and build again. This is an iterative
   process.

### Phase 3: Special Case - Generated Files

If a generated file (e.g., `objects-printer.cc`) fails with undefined inline
methods:

1. **Trace to generator**: Realize that editing the generated file is temporary.
   Find the source generator (e.g., in `src/torque/`).
2. **Modify the generator**: Update the generator code to emit the necessary
   `#include` statements in the files it generates.
3. **Rebuild the generator**: Recompile the tool (e.g., Torque) and let the
   build system regenerate the files.

## Best Practices

- **Keep files isolated**: Do not revert the cycle-breaking change when you see
  cascade errors. Push through by fixing the files.
- **Use guards**: Always wrap Wasm-specific includes in
  `#if V8_ENABLE_WEBASSEMBLY`.
- **Check trait caching**: Be aware of C++ trait caching issues if you encounter
  weird subtyping errors during header reorganization.
