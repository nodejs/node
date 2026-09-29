# Lazy Parsing and the Preparser

This document explains V8's strategy for fast JavaScript parsing, focusing on lazy parsing and the preparser, as described in the "Blazingly fast parsing" series on v8.dev.

## The Problem: Too Much Code

Web pages often ship much more JavaScript than is needed for initial startup. Eagerly parsing and compiling all this code has significant costs:
*   **CPU Time**: Wasted on compiling code that might never run.
*   **Memory**: Code objects take up heap space.
*   **Disk Cache**: Compiled code is cached, taking up disk space.

To mitigate this, V8 uses **Lazy Parsing**.

## The Preparser

When V8 encounters a function during top-level parsing, it often decides to "pre-parse" it instead of fully parsing and compiling it.

*   **Bare Minimum**: The preparser does the bare minimum needed to skip over the function.
*   **Syntax Validation**: It verifies that the function is syntactically valid.
*   **No AST**: It does *not* build an Abstract Syntax Tree (AST).
*   **Scope Info**: It produces the necessary scope and variable information needed for the outer function to be compiled correctly.

When the function is actually called, it is fully parsed and compiled on-demand.

## The Variable Allocation Challenge

The main complexity in pre-parsing is **variable allocation** for closures.

If an inner function references a variable declared in an outer function, that variable cannot be allocated on the stack (since the outer function's frame will disappear). It must be allocated in a heap-based **Context**.

To know whether to allocate a variable on the stack or in a context, the preparser must track variable declarations and references across scopes, even though it is skipping the function body.

### Shadowing and Ambiguity
JavaScript syntax can be ambiguous (e.g., arrow functions vs. destructuring). The preparser must correctly resolve whether a reference actually targets an outer variable or a shadowed inner one.

## Sharing Code: `ParserBase` and CRTP

To avoid code duplication and divergence between the full parser and the preparser, V8 uses the **Curiously Recurring Template Pattern (CRTP)**.

Both `Parser` and `PreParser` inherit from a shared `ParserBase`:

```cpp
template <typename Impl>
class ParserBase { ... };

class Parser : public ParserBase<Parser> { ... };
class PreParser : public ParserBase<PreParser> { ... };
```

This allows maximum code sharing for the complex logic of variable tracking and spec-compliance checks while allowing specific overrides for AST building vs. skipping.

## Avoiding Non-Linear Parse Costs

In early versions of V8, when a lazy function was finally called and needed to be compiled, V8 had to re-preparse its inner functions to figure out scope resolution again. In deeply nested code (common with module bundlers), this led to non-linear parse times.

To solve this, V8 produces serialized log data during pre-parsing, known as **PreparseData** (handled by `PreparseDataBuilder`). When the outer function is finally compiled, it reads this metadata to know where variables live, without needing to re-preparse the inner functions.

## Eager Parsing Heuristics

To avoid pre-parsing functions that are likely to be executed immediately, V8 uses heuristics to detect them and eagerly parses and compiles them.

### PIFEs: Possibly-Invoked Function Expressions
*   **Parentheses**: An open parenthesis before a function expression (e.g., `(function(){...})`) is a strong hint that it will be called immediately.
*   **Prefix operators**: The specific prefix operator `!` before a function (e.g., `!function(){...}()`), common in minifiers like UglifyJS, also triggers eager compilation.

### Compile Hints and Magic Comments
V8 also supports "magic comments" to guide eager vs. lazy compilation (when enabled by flags):
*   **`//# allFunctionsCalledOnLoad`**: Hints that all functions in the script should be eagerly compiled.
*   **`//# functionsCalledOnLoad=<data>`**: Contains VLQ Base64 encoded data specifying the positions of functions that should be eagerly compiled.

---

## See Also
-   `src/parsing/preparser.h`: Implementation of the preparser.
-   `src/parsing/parser-base.h`: The shared base class using CRTP.
-   [Blazingly fast parsing, part 2: lazy parsing](https://v8.dev/blog/preparser) on v8.dev.
