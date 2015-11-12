# ARM debugging with the simulator

The simulator and debugger can be very helpful when working with v8 code generation.

  * It is convenient as it allows you to test code generation without access to actual hardware.
  * No cross or native compilation is needed.
  * The simulator fully supports the debugging of generated code.

Please note that this simulator is designed for v8 purposes. Only the features used by v8 are implemented, and you might encounter unimplemented features or instructions. In this case, feel free to implement them and submit the code!


## Details on the ARM Debugger

Compile the ARM simulator shell with:
```
make arm.debug
```
on an x86 host using your regular compiler.

### Starting the Debugger
There are different ways of starting the debugger:

```
$ out/arm.debug/d8 --stop_sim_at <n>
```
The simulator will start the debugger after executing n instructions.

```
$ out/arm.debug/d8 --stop_at <function name>
```

The simulator will stop at the given JavaScript function.

Also you can directly generate 'stop' instructions in the ARM code. Stops are generated with

```
Assembler::stop(const char* msg, Condition cond, int32_t code)
```

When the Simulator hits a stop, it will print msg and start the debugger.


### Debugging commands.

**Usual commands:**

Enter `help` in the debugger prompt to get details on available commands. These include usual gdb-like commands, such as stepi, cont, disasm, etc. If the Simulator is run under gdb, the “gdb” debugger command will give control to gdb. You can then use cont from gdb to go back to the debugger.


**Debugger specific commands:**

Here's a list of the ARM debugger specific commands, along with examples.
The JavaScript file “func.js” used below contains:

```
function test() {
  print(“In function test.”);
}
test();
```

  * **printobject** `<`register`>` (alias po),  will describe an object held in a register.

```
$ out/arm.debug/d8 func.js --stop_at test

Simulator hit stop-at 
  0xb544d6a8  e92d4902       stmdb sp!, {r1, r8, fp, lr} 
sim> print r0 
r0: 0xb547ec15 -1253577707 
sim> printobject r0 
r0: 
0xb547ec15: [Function] 
 - map = 0x0xb540ff01 
 - initial_map = 
 - shared_info = 0xb547eb2d <SharedFunctionInfo> 
   - name = #test 
 - context = 0xb60083f1 <FixedArray[52]> 
 - code = 0xb544d681 <Code> 
   #arguments: 0xb545a15d <Proxy> (callback) 
   #length: 0xb545a14d <Proxy> (callback) 
   #name: 0xb545a155 <Proxy> (callback) 
   #prototype: 0xb545a145 <Proxy> (callback) 
   #caller: 0xb545a165 <Proxy> (callback)
```

  * **break** `<`address`>`,  will insert a breakpoint at the specified address.

  * **del**, will delete the current breakpoint.

You can have only one such breakpoint. This is useful if you want to insert a breakpoint at runtime.
```
$ out/arm.debug/d8 func.js --stop_at test

Simulator hit stop-at 
  0xb53a1ee8  e92d4902       stmdb sp!, {r1, r8, fp, lr} 
sim> disasm 5 
  0xb53a1ee8  e92d4902       stmdb sp!, {r1, r8, fp, lr} 
  0xb53a1eec  e28db008       add fp, sp, #8 
  0xb53a1ef0  e59a200c       ldr r2, [r10, #+12] 
  0xb53a1ef4  e28fe004       add lr, pc, #4 
  0xb53a1ef8  e15d0002       cmp sp, r2 
sim> break 0xb53a1ef8 
sim> cont 
  0xb53a1ef8  e15d0002       cmp sp, r2 
sim> disasm 5 
  0xb53a1ef8  e15d0002       cmp sp, r2 
  0xb53a1efc  359ff034       ldrcc pc, [pc, #+52] 
  0xb53a1f00  e5980017       ldr r0, [r8, #+23] 
  0xb53a1f04  e59f1030       ldr r1, [pc, #+48] 
  0xb53a1f08  e52d0004       str r0, [sp, #-4]! 
sim> break 0xb53a1f08 
setting breakpoint failed 
sim> del 
sim> break 0xb53a1f08 
sim> cont 
  0xb53a1f08  e52d0004       str r0, [sp, #-4]! 
sim> del 
sim> cont 
In function test.
```

  * Generated `stop` instuctions, will work as breakpoints with a few additional features.

The first argument is a help message, the second is the condition, and the third is the stop code. If a code is specified, and is less than 256, the stop is said to be “watched”, and can be disabled/enabled; a counter also keeps track of how many times the Simulator hits this code.

If we are working on this v8 C++ code, which is reached when running our JavaScript file.

```
__ stop("My stop.", al, 123); 
__ mov(r0, r0); 
__ mov(r0, r0); 
__ mov(r0, r0);
__ mov(r0, r0);
__ mov(r0, r0); 
__ stop("My second stop.", al, 0x1); 
__ mov(r1, r1); 
__ mov(r1, r1); 
__ mov(r1, r1); 
__ mov(r1, r1);
__ mov(r1, r1); 
```

Here's a sample debugging session:

We hit the first stop.

```
Simulator hit My stop. 
  0xb53559e8  e1a00000       mov r0, r0
```

We can see the following stop using disasm. The address of the message string is inlined in the code after the svc stop instruction.

```
sim> disasm 
  0xb53559e8  e1a00000       mov r0, r0 
  0xb53559ec  e1a00000       mov r0, r0 
  0xb53559f0  e1a00000       mov r0, r0 
  0xb53559f4  e1a00000       mov r0, r0 
  0xb53559f8  e1a00000       mov r0, r0 
  0xb53559fc  ef800001       stop 1 - 0x1 
  0xb5355a00  08338a97       stop message: My second stop 
  0xb5355a04  e1a00000       mov r1, r1 
  0xb5355a08  e1a00000       mov r1, r1 
  0xb5355a0c  e1a00000       mov r1, r1
```

Information can be printed for all (watched) stops which were hit at least once.

```
sim> stop info all 
Stop information: 
stop 123 - 0x7b: 	Enabled, 	counter = 1, 	My stop. 
sim> cont 
Simulator hit My second stop 
  0xb5355a04  e1a00000       mov r1, r1 
sim> stop info all 
Stop information: 
stop 1 - 0x1: 	Enabled, 	counter = 1, 	My second stop 
stop 123 - 0x7b: 	Enabled, 	counter = 1, 	My stop.
```

Stops can be disabled or enabled. (Only available for watched stops.)

```
sim> stop disable 1 
sim> cont 
Simulator hit My stop. 
  0xb5356808  e1a00000       mov r0, r0 
sim> cont 
Simulator hit My stop. 
  0xb5356c28  e1a00000       mov r0, r0 
sim> stop info all 
Stop information: 
stop 1 - 0x1: 	Disabled, 	counter = 2, 	My second stop 
stop 123 - 0x7b: 	Enabled, 	counter = 3, 	My stop. 
sim> stop enable 1 
sim> cont 
Simulator hit My second stop 
  0xb5356c44  e1a00000       mov r1, r1 
sim> stop disable all 
sim> con
In function test.
```