---
title: 'Arm debugging with the simulator'
description: 'The Arm simulator and debugger can be very helpful when working with V8 code generation.'
---
The simulator and debugger can be very helpful when working with V8 code generation.

- It is convenient as it allows you to test code generation without access to actual hardware.
- No [cross](/docs/cross-compile-arm) or native compilation is needed.
- The simulator fully supports the debugging of generated code.

Please note that this simulator is designed for V8 purposes. Only the features used by V8 are implemented, and you might encounter unimplemented features or instructions. In this case, feel free to implement them and submit the code!

- [Compiling](#compiling)
- [Starting the debugger](#start_debug)
- [Debugging commands](#debug_commands)
    - [printobject](#po)
    - [trace](#trace)
    - [break](#break)
- [Extra breakpoint features](#extra)
    - [32-bit: `stop()`](#arm32_stop)
    - [64-bit: `Debug()`](#arm64_debug)

## Compiling for Arm using the simulator { #compiling }

By default on an x86 host, compiling for Arm with [gm](/docs/build-gn#gm) will give you a simulator build:

```bash
gm arm64.debug # For a 64-bit build or...
gm arm.debug   # ... for a 32-bit build.
```

You may also build the `optdebug` configuration as the `debug` may be a little slow, especially if you want to run the V8 test suite.

## Starting the debugger { #start_debug }

You can start the debugger immediately from the command line after `n` instructions:

```bash
out/arm64.debug/d8 --stop_sim_at <n> # Or out/arm.debug/d8 for a 32-bit build.
```

Alternatively, you can generate a breakpoint instruction in the generated code:

Natively, breakpoint instructions cause the program to halt with a `SIGTRAP` signal, allowing you to debug the issue with gdb. However, if running with a simulator, a breakpoint instruction in generated code will instead drop you into the simulator debugger.

You can generate a breakpoint in multiple ways by using `DebugBreak()` from [Torque](/docs/torque-builtins), from the [CodeStubAssembler](/docs/csa-builtins), as a node in a [TurboFan](/docs/turbofan) pass, or directly using an assembler.

Here we focus on debugging low-level native code, so let's look at the assembler method:

```cpp
TurboAssembler::DebugBreak();
```

Let's say we have a jitted function called `add` compiled with [TurboFan](/docs/turbofan) and we'd like to break at the start. Given a `test.js` example:

{ #test.js }

```js
// Our optimized function.
function add(a, b) {
  return a + b;
}

// Typical cheat code enabled by --allow-natives-syntax.
%PrepareFunctionForOptimization(add);

// Give the optimizing compiler type feedback so it'll speculate `a` and `b` are
// numbers.
add(1, 3);

// And force it to optimize.
%OptimizeFunctionOnNextCall(add);
add(5, 7);
```

To do it, we can hook into TurboFan's [code generator](https://crsrc.org/c/v8/src/compiler/backend/code-generator.cc?q=CodeGenerator::AssembleCode) and access the assembler to insert our breakpoint:

```cpp
void CodeGenerator::AssembleCode() {
  // ...

  // Check if we're optimizing, then look-up the name of the current function and
  // insert a breakpoint.
  if (info->IsOptimizing()) {
    AllowHandleDereference allow_handle_dereference;
    if (info->shared_info()->PassesFilter("add")) {
      tasm()->DebugBreak();
    }
  }

  // ...
}
```

And let's run it:

```simulator
$ d8 \
    # Enable '%' cheat code JS functions.
    --allow-natives-syntax \
    # Disassemble our function.
    --print-opt-code --print-opt-code-filter="add" --code-comments \
    # Disable spectre mitigations for readability.
    --no-untrusted-code-mitigations \
    test.js
--- Raw source ---
(a, b) {
  return a + b;
}


--- Optimized code ---
optimization_id = 0
source_position = 12
kind = OPTIMIZED_FUNCTION
name = add
stack_slots = 6
compiler = turbofan
address = 0x7f0900082ba1

Instructions (size = 504)
0x7f0900082be0     0  d45bd600       constant pool begin (num_const = 6)
0x7f0900082be4     4  00000000       constant
0x7f0900082be8     8  00000001       constant
0x7f0900082bec     c  75626544       constant
0x7f0900082bf0    10  65724267       constant
0x7f0900082bf4    14  00006b61       constant
0x7f0900082bf8    18  d45bd7e0       constant
                  -- Prologue: check code start register --
0x7f0900082bfc    1c  10ffff30       adr x16, #-0x1c (addr 0x7f0900082be0)
0x7f0900082c00    20  eb02021f       cmp x16, x2
0x7f0900082c04    24  54000080       b.eq #+0x10 (addr 0x7f0900082c14)
                  Abort message:
                  Wrong value in code start register passed
0x7f0900082c08    28  d2800d01       movz x1, #0x68
                  -- Inlined Trampoline to Abort --
0x7f0900082c0c    2c  58000d70       ldr x16, pc+428 (addr 0x00007f0900082db8)    ;; off heap target
0x7f0900082c10    30  d63f0200       blr x16
                  -- Prologue: check for deoptimization --
                  [ DecompressTaggedPointer
0x7f0900082c14    34  b85d0050       ldur w16, [x2, #-48]
0x7f0900082c18    38  8b100350       add x16, x26, x16
                  ]
0x7f0900082c1c    3c  b8407210       ldur w16, [x16, #7]
0x7f0900082c20    40  36000070       tbz w16, #0, #+0xc (addr 0x7f0900082c2c)
                  -- Inlined Trampoline to CompileLazyDeoptimizedCode --
0x7f0900082c24    44  58000c31       ldr x17, pc+388 (addr 0x00007f0900082da8)    ;; off heap target
0x7f0900082c28    48  d61f0220       br x17
                  -- B0 start (construct frame) --
(...)

--- End code ---
# Debugger hit 0: DebugBreak
0x00007f0900082bfc 10ffff30            adr x16, #-0x1c (addr 0x7f0900082be0)
sim>
```

We can see we've stopped at the start of the optimized function and the simulator gave us a prompt!

Note this is just an example and V8 changes quickly so the details may vary. But you should be able to do this anywhere where an assembler is available.

## Debugging commands { #debug_commands }

### Common commands

Enter `help` in the debugger prompt to get details on available commands. These include usual gdb-like commands, such as `stepi`, `cont`, `disasm`, etc. If the Simulator is run under gdb, the `gdb` debugger command will give control to gdb. You can then use `cont` from gdb to go back to the debugger.

### Architecture specific commands

Each target architecture implements its own simulator and debugger, so the experience and details will vary.

- [printobject](#po)
- [trace](#trace)
- [break](#break)

#### `printobject $register` (alias `po`) { #po }

Describe a JS object held in a register.

For example, let's say this time we're running [our example](#test.js) on a 32-bit Arm simulator build. We can examine incoming arguments passed in registers:

```simulator
$ ./out/arm.debug/d8 --allow-natives-syntax test.js
Simulator hit stop, breaking at the next instruction:
  0x26842e24  e24fc00c       sub ip, pc, #12
sim> print r1
r1: 0x4b60ffb1 1264648113
# The current function object is passed with r1.
sim> printobject r1
r1:
0x4b60ffb1: [Function] in OldSpace
 - map: 0x485801f9 <Map(HOLEY_ELEMENTS)> [FastProperties]
 - prototype: 0x4b6010f1 <JSFunction (sfi = 0x42404e99)>
 - elements: 0x5b700661 <FixedArray[0]> [HOLEY_ELEMENTS]
 - function prototype:
 - initial_map:
 - shared_info: 0x4b60fe9d <SharedFunctionInfo add>
 - name: 0x5b701c5d <String[#3]: add>
 - formal_parameter_count: 2
 - kind: NormalFunction
 - context: 0x4b600c65 <NativeContext[261]>
 - code: 0x26842de1 <Code OPTIMIZED_FUNCTION>
 - source code: (a, b) {
  return a + b;
}
(...)

# Now print the current JS context passed in r7.
sim> printobject r7
r7:
0x449c0c65: [NativeContext] in OldSpace
 - map: 0x561000b9 <Map>
 - length: 261
 - scope_info: 0x34081341 <ScopeInfo SCRIPT_SCOPE [5]>
 - previous: 0
 - native_context: 0x449c0c65 <NativeContext[261]>
           0: 0x34081341 <ScopeInfo SCRIPT_SCOPE [5]>
           1: 0
           2: 0x449cdaf5 <JSObject>
           3: 0x58480c25 <JSGlobal Object>
           4: 0x58485499 <Other heap object (EMBEDDER_DATA_ARRAY_TYPE)>
           5: 0x561018a1 <Map(HOLEY_ELEMENTS)>
           6: 0x3408027d <undefined>
           7: 0x449c75c1 <JSFunction ArrayBuffer (sfi = 0x4be8ade1)>
           8: 0x561010f9 <Map(HOLEY_ELEMENTS)>
           9: 0x449c967d <JSFunction arrayBufferConstructor_DoNotInitialize (sfi = 0x4be8c3ed)>
          10: 0x449c8dbd <JSFunction Array (sfi = 0x4be8be59)>
(...)
```

#### `trace` (alias `t`) { #trace }

Enable or disable tracing executed instructions.

When enabled, the simulator will print disassembled instructions as it is executing them. If you're running a 64-bit Arm build, the simulator is also able to trace changes to register values.

You may also enable this from the command-line with the `--trace-sim` flag to enable tracing from the start.

With the same [example](#test.js):

```simulator
$ out/arm64.debug/d8 --allow-natives-syntax \
    # --debug-sim is required on 64-bit Arm to enable disassembly
    # when tracing.
    --debug-sim test.js
# Debugger hit 0: DebugBreak
0x00007f1e00082bfc  10ffff30            adr x16, #-0x1c (addr 0x7f1e00082be0)
sim> trace
0x00007f1e00082bfc  10ffff30            adr x16, #-0x1c (addr 0x7f1e00082be0)
Enabling disassembly, registers and memory write tracing

# Break on the return address stored in the lr register.
sim> break lr
Set a breakpoint at 0x7f1f880abd28
0x00007f1e00082bfc  10ffff30            adr x16, #-0x1c (addr 0x7f1e00082be0)

# Continuing will trace the function's execution until we return, allowing
# us to make sense of what is happening.
sim> continue
#    x0: 0x00007f1e00082ba1
#    x1: 0x00007f1e08250125
#    x2: 0x00007f1e00082be0
(...)

# We first load the 'a' and 'b' arguments from the stack and check if they
# are tagged numbers. This is indicated by the least significant bit being 0.
0x00007f1e00082c90  f9401fe2            ldr x2, [sp, #56]
#    x2: 0x000000000000000a <- 0x00007f1f821f0278
0x00007f1e00082c94  7200005f            tst w2, #0x1
# NZCV: N:0 Z:1 C:0 V:0
0x00007f1e00082c98  54000ac1            b.ne #+0x158 (addr 0x7f1e00082df0)
0x00007f1e00082c9c  f9401be3            ldr x3, [sp, #48]
#    x3: 0x000000000000000e <- 0x00007f1f821f0270
0x00007f1e00082ca0  7200007f            tst w3, #0x1
# NZCV: N:0 Z:1 C:0 V:0
0x00007f1e00082ca4  54000a81            b.ne #+0x150 (addr 0x7f1e00082df4)

# Then we untag and add 'a' and 'b' together.
0x00007f1e00082ca8  13017c44            asr w4, w2, #1
#    x4: 0x0000000000000005
0x00007f1e00082cac  2b830484            adds w4, w4, w3, asr #1
# NZCV: N:0 Z:0 C:0 V:0
#    x4: 0x000000000000000c
# That's 5 + 7 == 12, all good!

# Then we check for overflows and tag the result again.
0x00007f1e00082cb0  54000a46            b.vs #+0x148 (addr 0x7f1e00082df8)
0x00007f1e00082cb4  2b040082            adds w2, w4, w4
# NZCV: N:0 Z:0 C:0 V:0
#    x2: 0x0000000000000018
0x00007f1e00082cb8  54000466            b.vs #+0x8c (addr 0x7f1e00082d44)


# And finally we place the result in x0.
0x00007f1e00082cbc  aa0203e0            mov x0, x2
#    x0: 0x0000000000000018
(...)

0x00007f1e00082cec  d65f03c0            ret
Hit and disabled a breakpoint at 0x7f1f880abd28.
0x00007f1f880abd28  f85e83b4            ldur x20, [fp, #-24]
sim>
```

#### `break $address` { #break }

Inserts a breakpoint at the specified address.

Note that on 32-bit Arm, you can have only one breakpoint and you'll need to disable write protection on code pages to insert it. The 64-bit Arm simulator does not have such restrictions.

With our [example](#test.js) again:

```simulator
$ out/arm.debug/d8 --allow-natives-syntax \
    # This is useful to know which address to break to.
    --print-opt-code --print-opt-code-filter="add" \
    test.js
(...)

Simulator hit stop, breaking at the next instruction:
  0x488c2e20  e24fc00c       sub ip, pc, #12

# Break on a known interesting address, where we start
# loading 'a' and 'b'.
sim> break 0x488c2e9c
sim> continue
  0x488c2e9c  e59b200c       ldr r2, [fp, #+12]

# We can look-ahead with 'disasm'.
sim> disasm 10
  0x488c2e9c  e59b200c       ldr r2, [fp, #+12]
  0x488c2ea0  e3120001       tst r2, #1
  0x488c2ea4  1a000037       bne +228 -> 0x488c2f88
  0x488c2ea8  e59b3008       ldr r3, [fp, #+8]
  0x488c2eac  e3130001       tst r3, #1
  0x488c2eb0  1a000037       bne +228 -> 0x488c2f94
  0x488c2eb4  e1a040c2       mov r4, r2, asr #1
  0x488c2eb8  e09440c3       adds r4, r4, r3, asr #1
  0x488c2ebc  6a000037       bvs +228 -> 0x488c2fa0
  0x488c2ec0  e0942004       adds r2, r4, r4

# And try and break on the result of the first `adds` instructions.
sim> break 0x488c2ebc
setting breakpoint failed

# Ah, we need to delete the breakpoint first.
sim> del
sim> break 0x488c2ebc
sim> cont
  0x488c2ebc  6a000037       bvs +228 -> 0x488c2fa0

sim> print r4
r4: 0x0000000c 12
# That's 5 + 7 == 12, all good!
```

### Generated breakpoint instructions with a few additional features { #extra }

Instead of `TurboAssembler::DebugBreak()`, you may use a lower-level instruction which has the same effect except with additional features.

- [32-bit: `stop()`](#arm32_stop)
- [64-bit: `Debug()`](#arm64_debug)

#### `stop()` (32-bit Arm) { #arm32_stop }

```cpp
Assembler::stop(Condition cond = al, int32_t code = kDefaultStopCode);
```

The first argument is the condition and the second is the stop code. If a code is specified, and is less than 256, the stop is said to be “watched”, and can be disabled/enabled; a counter also keeps track of how many times the Simulator hits this code.

Imagine we are working on this V8 C++ code:

```cpp
__ stop(al, 123);
__ mov(r0, r0);
__ mov(r0, r0);
__ mov(r0, r0);
__ mov(r0, r0);
__ mov(r0, r0);
__ stop(al, 0x1);
__ mov(r1, r1);
__ mov(r1, r1);
__ mov(r1, r1);
__ mov(r1, r1);
__ mov(r1, r1);
```

Here's a sample debugging session:

We hit the first stop.

```simulator
Simulator hit stop 123, breaking at the next instruction:
  0xb53559e8  e1a00000       mov r0, r0
```

We can see the following stop using `disasm`.

```simulator
sim> disasm
  0xb53559e8  e1a00000       mov r0, r0
  0xb53559ec  e1a00000       mov r0, r0
  0xb53559f0  e1a00000       mov r0, r0
  0xb53559f4  e1a00000       mov r0, r0
  0xb53559f8  e1a00000       mov r0, r0
  0xb53559fc  ef800001       stop 1 - 0x1
  0xb5355a00  e1a00000       mov r1, r1
  0xb5355a04  e1a00000       mov r1, r1
  0xb5355a08  e1a00000       mov r1, r1
```

Information can be printed for all (watched) stops which were hit at least once.

```simulator
sim> stop info all
Stop information:
stop 123 - 0x7b:      Enabled,      counter = 1
sim> cont
Simulator hit stop 1, breaking at the next instruction:
  0xb5355a04  e1a00000       mov r1, r1
sim> stop info all
Stop information:
stop 1 - 0x1:         Enabled,      counter = 1
stop 123 - 0x7b:      Enabled,      counter = 1
```

Stops can be disabled or enabled. (Only available for watched stops.)

```simulator
sim> stop disable 1
sim> cont
Simulator hit stop 123, breaking at the next instruction:
  0xb5356808  e1a00000       mov r0, r0
sim> cont
Simulator hit stop 123, breaking at the next instruction:
  0xb5356c28  e1a00000       mov r0, r0
sim> stop info all
Stop information:
stop 1 - 0x1:         Disabled,     counter = 2
stop 123 - 0x7b:      Enabled,      counter = 3
sim> stop enable 1
sim> cont
Simulator hit stop 1, breaking at the next instruction:
  0xb5356c44  e1a00000       mov r1, r1
sim> stop disable all
sim> con
```

#### `Debug()` (64-bit Arm) { #arm64_debug }

```cpp
MacroAssembler::Debug(const char* message, uint32_t code, Instr params = BREAK);
```

This instruction is a breakpoint by default, but is also able to enable and disable tracing as if you had done it with the [`trace`](#trace) command in the debugger. You can also give it a message and a code as an identifier.

Imagine we are working on this V8 C++ code, taken from the native builtin that prepares the frame to call a JS function.

```cpp
int64_t bad_frame_pointer = -1L;  // Bad frame pointer, should fail if it is used.
__ Mov(x13, bad_frame_pointer);
__ Mov(x12, StackFrame::TypeToMarker(type));
__ Mov(x11, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                      masm->isolate()));
__ Ldr(x10, MemOperand(x11));

__ Push(x13, x12, xzr, x10);
```

It might be useful to insert a breakpoint with `DebugBreak()` so we can examine the current state when we run this. But we can go further and trace this code if we use `Debug()` instead:

```cpp
// Start tracing and log disassembly and register values.
__ Debug("start tracing", 42, TRACE_ENABLE | LOG_ALL);

int64_t bad_frame_pointer = -1L;  // Bad frame pointer, should fail if it is used.
__ Mov(x13, bad_frame_pointer);
__ Mov(x12, StackFrame::TypeToMarker(type));
__ Mov(x11, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                      masm->isolate()));
__ Ldr(x10, MemOperand(x11));

__ Push(x13, x12, xzr, x10);

// Stop tracing.
__ Debug("stop tracing", 42, TRACE_DISABLE);
```

It allows us to trace register values for __just__ the snippet of code we're working on:

```simulator
$ d8 --allow-natives-syntax --debug-sim test.js
# NZCV: N:0 Z:0 C:0 V:0
# FPCR: AHP:0 DN:0 FZ:0 RMode:0b00 (Round to Nearest)
#    x0: 0x00007fbf00000000
#    x1: 0x00007fbf0804030d
#    x2: 0x00007fbf082500e1
(...)

0x00007fc039d31cb0  9280000d            movn x13, #0x0
#   x13: 0xffffffffffffffff
0x00007fc039d31cb4  d280004c            movz x12, #0x2
#   x12: 0x0000000000000002
0x00007fc039d31cb8  d2864110            movz x16, #0x3208
#   ip0: 0x0000000000003208
0x00007fc039d31cbc  8b10034b            add x11, x26, x16
#   x11: 0x00007fbf00003208
0x00007fc039d31cc0  f940016a            ldr x10, [x11]
#   x10: 0x0000000000000000 <- 0x00007fbf00003208
0x00007fc039d31cc4  a9be7fea            stp x10, xzr, [sp, #-32]!
#    sp: 0x00007fc033e81340
#   x10: 0x0000000000000000 -> 0x00007fc033e81340
#   xzr: 0x0000000000000000 -> 0x00007fc033e81348
0x00007fc039d31cc8  a90137ec            stp x12, x13, [sp, #16]
#   x12: 0x0000000000000002 -> 0x00007fc033e81350
#   x13: 0xffffffffffffffff -> 0x00007fc033e81358
0x00007fc039d31ccc  910063fd            add fp, sp, #0x18 (24)
#    fp: 0x00007fc033e81358
0x00007fc039d31cd0  d45bd600            hlt #0xdeb0
```
