# Prerequisites

  * V8 3.0.9 or newer
  * GDB 7.0 or newer
  * Linux OS
  * CPU with Intel-compatible architecture (ia32 or x64)

# Introduction

GDB JIT interface integration allows V8 to provide GDB with the symbol and debugging information for a native code emitted in runtime.

When GDB JIT interface is disabled a typical backtrace in GDB will contain frames marked with ??. This frames correspond to dynamically generated code:

```
#8  0x08281674 in v8::internal::Runtime_SetProperty (args=...) at src/runtime.cc:3758
#9  0xf5cae28e in ?? ()
#10 0xf5cc3a0a in ?? ()
#11 0xf5cc38f4 in ?? ()
#12 0xf5cbef19 in ?? ()
#13 0xf5cb09a2 in ?? ()
#14 0x0809e0a5 in v8::internal::Invoke (construct=false, func=..., receiver=..., argc=0, args=0x0, 
    has_pending_exception=0xffffd46f) at src/execution.cc:97
```

However enabling GDB JIT integration allows GDB to produce more informative stack trace:

```
#6  0x082857fc in v8::internal::Runtime_SetProperty (args=...) at src/runtime.cc:3758
#7  0xf5cae28e in ?? ()
#8  0xf5cc3a0a in loop () at test.js:6
#9  0xf5cc38f4 in test.js () at test.js:13
#10 0xf5cbef19 in ?? ()
#11 0xf5cb09a2 in ?? ()
#12 0x0809e1f9 in v8::internal::Invoke (construct=false, func=..., receiver=..., argc=0, args=0x0, 
    has_pending_exception=0xffffd44f) at src/execution.cc:97
```

Frames still unknown to GDB correspond to native code without source information. See [GDBJITInterface#KnownLimitations](GDBJITInterface#KnownLimitations.md) for more details.

GDB JIT interface is specified in the GDB documentation: http://sourceware.org/gdb/current/onlinedocs/gdb/JIT-Interface.html

# Enabling GDB JIT integration

GDBJIT currently is by default excluded from the compilation and disabled in runtime. To enable it:

  1. Build V8 library with `ENABLE_GDB_JIT_INTERFACE` defined. If you are using scons to build V8 run it with `gdbjit=on`.
  1. Pass `--gdbjit` flag when starting V8.

To check that you have enabled GDB JIT integration correctly try setting breakpoint on `__jit_debug_register_code`. This function will be invoked to notify GDB about new code objects.

# Known Limitations

  * GDB side of JIT Interface currently (as of GDB 7.2) does not handle registration of code objects very effectively. Each next registration takes more time: with 500 registered objects each next registration takes more than 50ms, with 1000 registered code objects - more than 300 ms. This problem was reported to GDB developers (http://sourceware.org/ml/gdb/2011-01/msg00002.html) but currently there is no solution available. To reduce pressure on GDB current implementation of GDB JIT integration operates in two modes: _default_ and _full_ (enabled by `--gdbjit-full` flag). In _default_ mode V8 notifies GDB only about code objects that have source information attached (this usually includes all user scripts). In _full_ - about all generated code objects (stubs, ICs, trampolines).

  * On x64 GDB is unable to properly unwind stack without `.eh_frame` section (Issue 1053 (on Google Code))

  * GDB is not notified about code deserialized from the snapshot (Issue 1054 (on Google Code))

  * Only Linux OS on Intel-compatible CPUs is supported. For different OSes either a different ELF-header should be generated or a completely different object format should be used.

  * Enabling GDB JIT interface disables compacting GC. This is done to reduce pressure on GDB as unregistering and registering each moved code object will incur considerable overhead.

  * GDB JIT integration provides only _approximate_ source information. It does not provide any information about local variables, function's arguments, stack layout etc. It does not enable stepping through JavaScript code or setting breakpoint on the given line. However one can set a breakpoint on a function by it's name.