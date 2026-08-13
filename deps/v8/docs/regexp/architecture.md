# RegExp Engine (Irregexp) Architecture

This document describes the architecture of V8's regular expression engine, **Irregexp**.

## Overview

Irregexp is a highly optimized regular expression engine. It does not use a simple backtracking NFA/DFA interpreter by default (though an experimental non-backtracking engine exists). Instead, it compiles regular expressions either directly into **native machine code** or into a specialized **bytecode** that is executed by a dedicated interpreter.

## The Compilation Pipeline

Irregexp has its own complete mini-compiler pipeline:

1.  **Parsing**: The `Parser` (`src/regexp/regexp-parser.h`) parses the regular expression pattern string into a specialized Abstract Syntax Tree (AST) defined in `src/regexp/regexp-ast.h`. Note that AST classes are not prefixed with `RegExp` (e.g., `Tree`, `Disjunction`).
2.  **Node Graph Generation**: The AST is converted into a graph of `Node` objects (`src/regexp/regexp-nodes.h`). This graph represents the state machine for matching. Similar to AST classes, Node classes are not prefixed with `RegExp` (e.g., `Node`, `TextNode`).
3.  **Optimization**: The engine performs analysis on the graph, such as computing `eats_at_least` (minimum characters needed to match) and setting up Boyer-Moore lookahead tables for fast scanning.
4.  **Code Generation**: The `Compiler` (`src/regexp/regexp-compiler.h`) traverses the graph and generates code using `RegExpMacroAssembler`.
    *   **Native Target**: Generates raw machine code for the target architecture.
    *   **Bytecode Target**: Generates Irregexp bytecode.

## Execution Modes

Irregexp can execute in two main modes, defined by `regexp::CompilationTarget`:

*   **`kNative`**: The engine generates machine code. This provides the fastest execution but consumes more memory for the generated code.
*   **`kBytecode`**: The engine generates bytecode. This is interpreted by `IrregexpInterpreter` (`src/regexp/regexp-interpreter.h`). This saves memory and is useful on platforms where JIT compilation is not allowed or for less frequently used regexps.

## Key Optimizations

*   **Boyer-Moore Lookahead**: For non-anchored regular expressions, Irregexp uses a Boyer-Moore-like skip table to quickly skip characters in the subject string that cannot possibly match the start of the pattern.
*   **Quick Check**: A mechanism that loads multiple characters at once and uses a mask-and-compare strategy to quickly reject non-matches.
*   **Traces**: During code generation, the compiler uses `Trace` objects to track the current state (character position, deferred actions, backtracking stack) to generate efficient code paths and avoid redundant work.

## File Structure
*   `src/regexp/`: Core RegExp implementation.
*   `src/regexp/regexp-parser.h` / `.cc`: Parser.
*   `src/regexp/regexp-ast.h`: AST definitions.
*   `src/regexp/regexp-compiler.h` / `.cc`: Compiler.
*   `src/regexp/regexp-interpreter.h` / `.cc`: Bytecode interpreter.
*   `src/regexp/regexp-nodes.h`: Graph node definitions.
