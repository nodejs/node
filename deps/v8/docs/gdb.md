---
title: 'Debugging builtins with GDB'
description: 'As of V8 v6.9, it’s possible to create breakpoints in GDB to debug CSA / ASM / Torque builtins.'
---
As of V8 v6.9, it’s possible to create breakpoints in GDB (and possibly other debuggers) to debug CSA / ASM / Torque builtins.

```
(gdb) tb i::Isolate::Init
Temporary breakpoint 1 at 0x7ffff706742b: i::Isolate::Init. (2 locations)
(gdb) r
Thread 1 "d8" hit Temporary breakpoint 1, 0x00007ffff7c55bc0 in Isolate::Init
(gdb) br Builtins_RegExpPrototypeExec
Breakpoint 2 at 0x7ffff7ac8784
(gdb) c
Thread 1 "d8" hit Breakpoint 2, 0x00007ffff7ac8784 in Builtins_RegExpPrototypeExec ()
```

Note that it works well to use a temporary breakpoint (shortcut `tb` in GDB) instead of a regular breakpoint (`br`) for this, since you only need it at process start.

Builtins are also visible in stack traces:

```
(gdb) bt
#0  0x00007ffff7ac8784 in Builtins_RegExpPrototypeExec ()
#1  0x00007ffff78f5066 in Builtins_ArgumentsAdaptorTrampoline ()
#2  0x000039751d2825b1 in ?? ()
#3  0x000037ef23a0fa59 in ?? ()
#4  0x0000000000000000 in ?? ()
```

Caveats:

- Only works with embedded builtins.
- Breakpoints can only be set at the start of the builtin.
- The initial breakpoint in `Isolate::Init` is needed prior to setting the builtin breakpoint, since GDB modifies the binary and we verify a hash of the builtins section in the binary at startup. Otherwise, V8 complains about a hash mismatch:

    ```
    # Fatal error in ../../src/isolate.cc, line 117
    # Check failed: d.Hash() == d.CreateHash() (11095509419988753467 vs. 3539781814546519144).
    ```
