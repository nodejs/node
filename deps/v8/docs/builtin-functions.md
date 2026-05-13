---
title: 'Built-in functions'
description: 'This document explains what “built-ins” are in V8.'
---
Built-in functions in V8 come in different flavors w.r.t. implementation, depending on their functionality, performance requirements, and sometimes plain historical development.

Some are implemented in JavaScript directly, and are compiled into executable code at runtime just like any user JavaScript. Some of them resort to so-called _runtime functions_ for part of their functionality. Runtime functions are written in C++ and called from JavaScript through a `%`-prefix. Usually, these runtime functions are limited to V8 internal JavaScript code. For debugging purposes, they can also be called from normal JavaScript code, if V8 is run with the flag `--allow-natives-syntax`. Some runtime functions are directly embedded by the compiler into generated code. For a list, see `src/runtime/runtime.h`.

Other functions are implemented as _built-ins_, which themselves can be implemented in a number of different ways. Some are implemented directly in platform-dependent assembly. Some are implemented in _CodeStubAssembler_, a platform-independent abstraction. Yet others are directly implemented in C++. Built-ins are sometimes also used to implement pieces of glue code, not necessarily entire functions. For a list, see `src/builtins/builtins.h`.
