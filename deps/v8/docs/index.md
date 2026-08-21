---
title: 'Documentation'
description: 'Documentation for the V8 project.'
---
V8 is Google’s open source high-performance JavaScript and WebAssembly engine, written in C++. It is used in Chrome and in Node.js, among others.

This documentation is aimed at C++ developers who want to use V8 in their applications, as well as anyone interested in V8’s design and performance. This document introduces you to V8, while the remaining documentation shows you how to use V8 in your code and describes some of its design details, as well as providing a set of JavaScript benchmarks for measuring V8’s performance.

## About V8

V8 implements <a href="https://tc39.es/ecma262/">ECMAScript</a> and <a href="https://webassembly.github.io/spec/core/">WebAssembly</a>, and runs on Windows, macOS, and Linux systems that use x64, IA-32, or ARM processors. Additional systems (IBM i, AIX) and processors (MIPS, ppcle64, s390x) are externally maintained, see [ports](/docs/ports). V8 can be embedded into any C++ application.

V8 compiles and executes JavaScript source code, handles memory allocation for objects, and garbage collects objects it no longer needs. V8’s stop-the-world, generational, accurate garbage collector is one of the keys to V8’s performance.

JavaScript is commonly used for client-side scripting in a browser, being used to manipulate Document Object Model (DOM) objects for example. The DOM is not, however, typically provided by the JavaScript engine but instead by a browser. The same is true of V8 — Google Chrome provides the DOM. V8 does however provide all the data types, operators, objects and functions specified in the ECMA standard.

V8 enables any C++ application to expose its own objects and functions to JavaScript code. It’s up to you to decide on the objects and functions you would like to expose to JavaScript.

## Documentation overview

- [Building V8 from source](/docs/build)
    - [Checking out the V8 source code](/docs/source-code)
    - [Building with GN](/docs/build-gn)
    - [Cross-compiling and debugging for ARM/Android](/docs/cross-compile-arm)
    - [Cross-compiling for iOS](/docs/cross-compile-ios)
    - [GUI and IDE setup](/docs/ide-setup)
    - [Compiling on Arm64](/docs/compile-arm64)
- [Contributing](/docs/contribute)
    - [Respectful code](/docs/respectful-code)
    - [V8’s public API and its stability](/docs/api)
    - [Becoming a V8 committer](/docs/become-committer)
    - [Committer’s responsibility](/docs/committer-responsibility)
    - [Blink web tests (a.k.a. layout tests)](/docs/blink-layout-tests)
    - [Evaluating code coverage](/docs/evaluate-code-coverage)
    - [Release process](/docs/release-process)
    - [Design review guidelines](/docs/design-review-guidelines)
    - [Implementing and shipping JavaScript/WebAssembly language features](/docs/feature-launch-process)
    - [Checklist for staging and shipping of WebAssembly features](/docs/wasm-shipping-checklist)
    - [Flake bisect](/docs/flake-bisect)
    - [Handling of ports](/docs/ports)
    - [Official support](/docs/official-support)
    - [Merging & patching](/docs/merge-patch)
    - [Node.js integration build](/docs/node-integration)
    - [Reporting security bugs](https://chromium.googlesource.com/v8/v8/+/HEAD/SECURITY.md)
    - [Running benchmarks locally](/docs/benchmarks)
    - [Testing](/docs/test)
    - [Triaging issues](/docs/triage-issues)
- Debugging
    - [Arm debugging with the simulator](/docs/debug-arm)
    - [Cross-compiling and debugging for ARM/Android](/docs/cross-compile-arm)
    - [Debugging builtins with GDB](/docs/gdb)
    - [Debugging over the V8 Inspector Protocol](/docs/inspector)
    - [GDB JIT Compilation Interface integration](/docs/gdb-jit)
    - [Investigating memory leaks](/docs/memory-leaks)
    - [Stack trace API](/docs/stack-trace-api)
    - [Using D8](/docs/d8)
    - [V8 Tools](https://v8.dev/tools)
- Embedding V8
    - [Guide to embedding V8](/docs/embed)
    - [Version numbers](/docs/version-numbers)
    - [Built-in functions](/docs/builtin-functions)
    - [i18n support](/docs/i18n)
    - [Untrusted code mitigations](/docs/untrusted-code-mitigations)
- Under the hood
    - [Ignition](/docs/ignition)
    - [TurboFan](/docs/turbofan)
    - [Torque user manual](/docs/torque)
    - [Writing Torque built-ins](/docs/torque-builtins)
    - [Writing CSA built-ins](/docs/csa-builtins)
    - [Adding a new WebAssembly opcode](/docs/webassembly-opcode)
    - [Maps, aka "Hidden Classes"](/docs/hidden-classes)
    - [Slack Tracking - what is it?](/blog/slack-tracking)
    - [WebAssembly compilation pipeline](/docs/wasm-compilation-pipeline)
- Writing optimizable JavaScript
    - [Using V8’s sample-based profiler](/docs/profile)
    - [Profiling Chromium with V8](/docs/profile-chromium)
    - [Using Linux `perf` with V8](/docs/linux-perf)
    - [Tracing V8](/docs/trace)
    - [Using Runtime Call Stats](/docs/rcs)
