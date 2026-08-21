---
title: 'WebAssembly - adding a new opcode'
description: 'This tutorial explains how to implement a new WebAssembly instruction in V8.'
---
[WebAssembly](https://webassembly.org/) (Wasm) is a binary instruction format for a stack-based virtual machine. This tutorial walks the reader through implementing a new WebAssembly instruction in V8.

WebAssembly is implemented in V8 in three parts:

- the interpreter
- the baseline compiler (Liftoff)
- the optimizing compiler (TurboFan)

The rest of this document focuses on the TurboFan pipeline, walking through how to add a new Wasm instruction and implement it in TurboFan.

At a high level, Wasm instructions are compiled into a TurboFan graph, and we rely on the TurboFan pipeline to compile the graph into (ultimately) machine code. For more on TurboFan, check out the [V8 docs](/docs/turbofan).

## Opcodes/Instructions

Let’s define a new instruction that adds `1` to an [`int32`](https://webassembly.github.io/spec/core/syntax/types.html#syntax-valtype) (on the top of the stack).

:::note
**Note:** A list of instructions supported by all Wasm implementations can be found in the [spec](https://webassembly.github.io/spec/core/appendix/index-instructions.html).
:::

All Wasm instructions are defined in [`src/wasm/wasm-opcodes.h`](https://cs.chromium.org/chromium/src/v8/src/wasm/wasm-opcodes.h). The instructions are grouped roughly by what they do, e.g. control, memory, SIMD, atomic, etc.

Let’s add our new instruction, `I32Add1`, to the `FOREACH_SIMPLE_OPCODE` section:

```diff
diff --git a/src/wasm/wasm-opcodes.h b/src/wasm/wasm-opcodes.h
index 6970c667e7..867cbf451a 100644
--- a/src/wasm/wasm-opcodes.h
+++ b/src/wasm/wasm-opcodes.h
@@ -96,6 +96,7 @@ bool IsJSCompatibleSignature(const FunctionSig* sig, bool hasBigIntFeature);

 // Expressions with signatures.
 #define FOREACH_SIMPLE_OPCODE(V)  \
+  V(I32Add1, 0xee, i_i)           \
   V(I32Eqz, 0x45, i_i)            \
   V(I32Eq, 0x46, i_ii)            \
   V(I32Ne, 0x47, i_ii)            \
```

WebAssembly is a binary format, so `0xee` specifies the encoding of this instruction. In this tutorial we chose `0xee` as it is currently unused.

:::note
**Note:** Actually adding an instruction to the spec involves work beyond what is described here.
:::

We can run a simple unit test for opcodes with:

```
$ tools/dev/gm.py x64.debug unittests/WasmOpcodesTest*
...
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from WasmOpcodesTest
[ RUN      ] WasmOpcodesTest.EveryOpcodeHasAName
../../test/unittests/wasm/wasm-opcodes-unittest.cc:27: Failure
Value of: false
  Actual: false
Expected: true
WasmOpcodes::OpcodeName(kExprI32Add1) == "unknown"; plazz halp in src/wasm/wasm-opcodes.cc
[  FAILED  ] WasmOpcodesTest.EveryOpcodeHasAName
```

This error indicates that we don’t have a name for our new instruction. Adding a name for the new opcode can be done in [`src/wasm/wasm-opcodes.cc`](https://cs.chromium.org/chromium/src/v8/src/wasm/wasm-opcodes.cc):

```diff
diff --git a/src/wasm/wasm-opcodes.cc b/src/wasm/wasm-opcodes.cc
index 5ed664441d..2d4e9554fe 100644
--- a/src/wasm/wasm-opcodes.cc
+++ b/src/wasm/wasm-opcodes.cc
@@ -75,6 +75,7 @@ const char* WasmOpcodes::OpcodeName(WasmOpcode opcode) {
     // clang-format off

     // Standard opcodes
+    CASE_I32_OP(Add1, "add1")
     CASE_INT_OP(Eqz, "eqz")
     CASE_ALL_OP(Eq, "eq")
     CASE_I64x2_OP(Eq, "eq")
```

By adding our new instruction in `FOREACH_SIMPLE_OPCODE`, we are skipping a [fair amount of work](https://cs.chromium.org/chromium/src/v8/src/wasm/function-body-decoder-impl.h?l=1751-1756&rcl=686b68edf9f42c201c2b25bca9f4bef72ff41c0b) that is done in `src/wasm/function-body-decoder-impl.h`, which decodes Wasm opcodes and calls into the TurboFan graph generator. Thus, depending on what your opcode does, you might have more work to do. We skip this in the interest of brevity.

## Writing a test for the new opcode { #test }

Wasm tests can be found in [`test/cctest/wasm/`](https://cs.chromium.org/chromium/src/v8/test/cctest/wasm/). Let’s take a look at [`test/cctest/wasm/test-run-wasm.cc`](https://cs.chromium.org/chromium/src/v8/test/cctest/wasm/test-run-wasm.cc), where many “simple” opcodes are tested.

There are many examples in this file that we can follow. The general setup is:

- create a `WasmRunner`
- set up globals to hold result (optional)
- set up locals as parameters to instruction (optional)
- build the wasm module
- run it and compare with an expected output

Here’s a simple test for our new opcode:

```diff
diff --git a/test/cctest/wasm/test-run-wasm.cc b/test/cctest/wasm/test-run-wasm.cc
index 26df61ceb8..b1ee6edd71 100644
--- a/test/cctest/wasm/test-run-wasm.cc
+++ b/test/cctest/wasm/test-run-wasm.cc
@@ -28,6 +28,15 @@ namespace test_run_wasm {
 #define RET(x) x, kExprReturn
 #define RET_I8(x) WASM_I32V_2(x), kExprReturn

+#define WASM_I32_ADD1(x) x, kExprI32Add1
+
+WASM_EXEC_TEST(Int32Add1) {
+  WasmRunner<int32_t> r(execution_tier);
+  // 10 + 1
+  BUILD(r, WASM_I32_ADD1(WASM_I32V_1(10)));
+  CHECK_EQ(11, r.Call());
+}
+
 WASM_EXEC_TEST(Int32Const) {
   WasmRunner<int32_t> r(execution_tier);
   const int32_t kExpectedValue = 0x11223344;
```

Run the test:

```
$ tools/dev/gm.py x64.debug 'cctest/test-run-wasm-simd/RunWasmTurbofan_I32Add1'
...
=== cctest/test-run-wasm/RunWasmTurbofan_Int32Add1 ===
#
# Fatal error in ../../src/compiler/wasm-compiler.cc, line 988
# Unsupported opcode 0xee:i32.add1
```

:::note
**Tip:** Finding the test name can be tricky, since the test definition is behind a macro. Use [Code Search](https://cs.chromium.org/) to click around to discover the macro definitions.
:::

This error indicates that the compiler does not know of our new instruction. That will change in the next section.

## Compiling Wasm into TurboFan

In the introduction, we mentioned that Wasm instructions are compiled into a TurboFan graph. `wasm-compiler.cc` is where this happens. Let’s take a look at an example opcode, [`I32Eqz`](https://cs.chromium.org/chromium/src/v8/src/compiler/wasm-compiler.cc?l=716&rcl=686b68edf9f42c201c2b25bca9f4bef72ff41c0b):

```cpp
  switch (opcode) {
    case wasm::kExprI32Eqz:
      op = m->Word32Equal();
      return graph()->NewNode(op, input, mcgraph()->Int32Constant(0));
```

This switches on the Wasm opcode `wasm::kExprI32Eqz`, and builds a TurboFan graph consisting of the operation `Word32Equal` with the inputs `input`, which is the argument to the Wasm instruction, and a constant `0`.

The `Word32Equal` operator is provided by the underlying V8 abstract machine, which is architecture-independent. Later in the pipeline, this abstract machine operator will be translated into architecture-dependent assembly.

For our new opcode, `I32Add1`, we need a graph that adds a constant 1 to the input, so we can reuse an existing machine operator, `Int32Add`, passing it the input, and a constant 1:

```diff
diff --git a/src/compiler/wasm-compiler.cc b/src/compiler/wasm-compiler.cc
index f666bbb7c1..399293c03b 100644
--- a/src/compiler/wasm-compiler.cc
+++ b/src/compiler/wasm-compiler.cc
@@ -713,6 +713,8 @@ Node* WasmGraphBuilder::Unop(wasm::WasmOpcode opcode, Node* input,
   const Operator* op;
   MachineOperatorBuilder* m = mcgraph()->machine();
   switch (opcode) {
+    case wasm::kExprI32Add1:
+      return graph()->NewNode(m->Int32Add(), input, mcgraph()->Int32Constant(1));
     case wasm::kExprI32Eqz:
       op = m->Word32Equal();
       return graph()->NewNode(op, input, mcgraph()->Int32Constant(0));
```

This is enough to get the test passing. However, not all instructions have an existing TurboFan machine operator. In that case we have to add this new operator to the machine. Let’s try that.

## TurboFan machine operators

We want to add the knowledge of `Int32Add1` to the TurboFan machine. So let’s pretend that it exists and use it first:

```diff
diff --git a/src/compiler/wasm-compiler.cc b/src/compiler/wasm-compiler.cc
index f666bbb7c1..1d93601584 100644
--- a/src/compiler/wasm-compiler.cc
+++ b/src/compiler/wasm-compiler.cc
@@ -713,6 +713,8 @@ Node* WasmGraphBuilder::Unop(wasm::WasmOpcode opcode, Node* input,
   const Operator* op;
   MachineOperatorBuilder* m = mcgraph()->machine();
   switch (opcode) {
+    case wasm::kExprI32Add1:
+      return graph()->NewNode(m->Int32Add1(), input);
     case wasm::kExprI32Eqz:
       op = m->Word32Equal();
       return graph()->NewNode(op, input, mcgraph()->Int32Constant(0));
```

Trying to run the same test leads to a compilation failure that hints at where to make changes:

```
../../src/compiler/wasm-compiler.cc:717:34: error: no member named 'Int32Add1' in 'v8::internal::compiler::MachineOperatorBuilder'; did you mean 'Int32Add'?
      return graph()->NewNode(m->Int32Add1(), input);
                                 ^~~~~~~~~
                                 Int32Add
```

There are a couple of places that needs to be modified to add an operator:

1. [`src/compiler/machine-operator.cc`](https://cs.chromium.org/chromium/src/v8/src/compiler/machine-operator.cc)
1. header [`src/compiler/machine-operator.h`](https://cs.chromium.org/chromium/src/v8/src/compiler/machine-operator.h)
1. list of opcodes that the machine understands [`src/compiler/opcodes.h`](https://cs.chromium.org/chromium/src/v8/src/compiler/opcodes.h)
1. verifier [`src/compiler/verifier.cc`](https://cs.chromium.org/chromium/src/v8/src/compiler/verifier.cc)

```diff
diff --git a/src/compiler/machine-operator.cc b/src/compiler/machine-operator.cc
index 16e838c2aa..fdd6d951f0 100644
--- a/src/compiler/machine-operator.cc
+++ b/src/compiler/machine-operator.cc
@@ -136,6 +136,7 @@ MachineType AtomicOpType(Operator const* op) {
 #define MACHINE_PURE_OP_LIST(V)                                               \
   PURE_BINARY_OP_LIST_32(V)                                                   \
   PURE_BINARY_OP_LIST_64(V)                                                   \
+  V(Int32Add1, Operator::kNoProperties, 1, 0, 1)                              \
   V(Word32Clz, Operator::kNoProperties, 1, 0, 1)                              \
   V(Word64Clz, Operator::kNoProperties, 1, 0, 1)                              \
   V(Word32ReverseBytes, Operator::kNoProperties, 1, 0, 1)                     \
```

```diff
diff --git a/src/compiler/machine-operator.h b/src/compiler/machine-operator.h
index a2b9fce0ee..f95e75a445 100644
--- a/src/compiler/machine-operator.h
+++ b/src/compiler/machine-operator.h
@@ -265,6 +265,8 @@ class V8_EXPORT_PRIVATE MachineOperatorBuilder final
   const Operator* Word32PairShr();
   const Operator* Word32PairSar();

+  const Operator* Int32Add1();
+
   const Operator* Int32Add();
   const Operator* Int32AddWithOverflow();
   const Operator* Int32Sub();
```

```diff
diff --git a/src/compiler/opcodes.h b/src/compiler/opcodes.h
index ce24a0bd3f..2c8c5ebaca 100644
--- a/src/compiler/opcodes.h
+++ b/src/compiler/opcodes.h
@@ -506,6 +506,7 @@
   V(Float64LessThanOrEqual)

 #define MACHINE_UNOP_32_LIST(V) \
+  V(Int32Add1)                  \
   V(Word32Clz)                  \
   V(Word32Ctz)                  \
   V(Int32AbsWithOverflow)       \
```

```diff
diff --git a/src/compiler/verifier.cc b/src/compiler/verifier.cc
index 461aef0023..95251934ce 100644
--- a/src/compiler/verifier.cc
+++ b/src/compiler/verifier.cc
@@ -1861,6 +1861,7 @@ void Verifier::Visitor::Check(Node* node, const AllNodes& all) {
     case IrOpcode::kSignExtendWord16ToInt64:
     case IrOpcode::kSignExtendWord32ToInt64:
     case IrOpcode::kStaticAssert:
+    case IrOpcode::kInt32Add1:

 #define SIMD_MACHINE_OP_CASE(Name) case IrOpcode::k##Name:
       MACHINE_SIMD_OP_LIST(SIMD_MACHINE_OP_CASE)
```

Running the test again now gives us a different failure:

```
=== cctest/test-run-wasm/RunWasmTurbofan_Int32Add1 ===
#
# Fatal error in ../../src/compiler/backend/instruction-selector.cc, line 2072
# Unexpected operator #289:Int32Add1 @ node #7
```

## Instruction selection

So far we have been working at the TurboFan level, dealing with (a sea of) nodes in the TurboFan graph. However, at the assembly level, we have instructions and operands. Instruction selection is the process of translating this graph to instructions and operands.

The last test error indicated that we need something in [`src/compiler/backend/instruction-selector.cc`](https://cs.chromium.org/chromium/src/v8/src/compiler/backend/instruction-selector.cc).  This is a big file with a giant switch statement over all the machine opcodes.  It calls into architecture specific instruction selection, using the visitor pattern to emit instructions for each type of node.

Since we added a new TurboFan machine opcode, we need to add it here as well:

```diff
diff --git a/src/compiler/backend/instruction-selector.cc b/src/compiler/backend/instruction-selector.cc
index 3152b2d41e..7375085649 100644
--- a/src/compiler/backend/instruction-selector.cc
+++ b/src/compiler/backend/instruction-selector.cc
@@ -2067,6 +2067,8 @@ void InstructionSelector::VisitNode(Node* node) {
       return MarkAsWord32(node), VisitS1x16AnyTrue(node);
     case IrOpcode::kS1x16AllTrue:
       return MarkAsWord32(node), VisitS1x16AllTrue(node);
+    case IrOpcode::kInt32Add1:
+      return MarkAsWord32(node), VisitInt32Add1(node);
     default:
       FATAL("Unexpected operator #%d:%s @ node #%d", node->opcode(),
             node->op()->mnemonic(), node->id());
```

Instruction selection is architecture dependent, so we have to add it to the architecture specific instruction selector files too. For this codelab we only focus on the x64 architecture, so [`src/compiler/backend/x64/instruction-selector-x64.cc`](https://cs.chromium.org/chromium/src/v8/src/compiler/backend/x64/instruction-selector-x64.cc)
needs to be modified:

```diff
diff --git a/src/compiler/backend/x64/instruction-selector-x64.cc b/src/compiler/backend/x64/instruction-selector-x64.cc
index 2324e119a6..4b55671243 100644
--- a/src/compiler/backend/x64/instruction-selector-x64.cc
+++ b/src/compiler/backend/x64/instruction-selector-x64.cc
@@ -841,6 +841,11 @@ void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
   Emit(kX64Bswap32, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)));
 }

+void InstructionSelector::VisitInt32Add1(Node* node) {
+  X64OperandGenerator g(this);
+  Emit(kX64Int32Add1, g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)));
+}
+
```

And we also need to add this new x64-specific opcode, `kX64Int32Add1` to [`src/compiler/backend/x64/instruction-codes-x64.h`](https://cs.chromium.org/chromium/src/v8/src/compiler/backend/x64/instruction-codes-x64.h):

```diff
diff --git a/src/compiler/backend/x64/instruction-codes-x64.h b/src/compiler/backend/x64/instruction-codes-x64.h
index 9b8be0e0b5..7f5faeb87b 100644
--- a/src/compiler/backend/x64/instruction-codes-x64.h
+++ b/src/compiler/backend/x64/instruction-codes-x64.h
@@ -12,6 +12,7 @@ namespace compiler {
 // X64-specific opcodes that specify which assembly sequence to emit.
 // Most opcodes specify a single instruction.
 #define TARGET_ARCH_OPCODE_LIST(V)        \
+  V(X64Int32Add1)                         \
   V(X64Add)                               \
   V(X64Add32)                             \
   V(X64And)                               \
```

## Instruction scheduling and code generation

Running our test, we see new compilation errors:

```
../../src/compiler/backend/x64/instruction-scheduler-x64.cc:15:11: error: enumeration value 'kX64Int32Add1' not handled in switch [-Werror,-Wswitch]
  switch (instr->arch_opcode()) {
          ^
1 error generated.
...
../../src/compiler/backend/x64/code-generator-x64.cc:733:11: error: enumeration value 'kX64Int32Add1' not handled in switch [-Werror,-Wswitch]
  switch (arch_opcode) {
          ^
1 error generated.
```

[Instruction scheduling](https://en.wikipedia.org/wiki/Instruction_scheduling) takes care of dependencies that instructions may have to allow for more optimization (e.g. instruction reordering). Our new opcode has no data dependency, so we can add it simply to: [`src/compiler/backend/x64/instruction-scheduler-x64.cc`](https://cs.chromium.org/chromium/src/v8/src/compiler/backend/x64/instruction-scheduler-x64.cc):

```diff
diff --git a/src/compiler/backend/x64/instruction-scheduler-x64.cc b/src/compiler/backend/x64/instruction-scheduler-x64.cc
index 79eda7e78d..3667a84577 100644
--- a/src/compiler/backend/x64/instruction-scheduler-x64.cc
+++ b/src/compiler/backend/x64/instruction-scheduler-x64.cc
@@ -13,6 +13,7 @@ bool InstructionScheduler::SchedulerSupported() { return true; }
 int InstructionScheduler::GetTargetInstructionFlags(
     const Instruction* instr) const {
   switch (instr->arch_opcode()) {
+    case kX64Int32Add1:
     case kX64Add:
     case kX64Add32:
     case kX64And:
```

Code generation is where we translate our architecture specific opcodes into assembly. Let’s add a clause to [`src/compiler/backend/x64/code-generator-x64.cc`](https://cs.chromium.org/chromium/src/v8/src/compiler/backend/x64/code-generator-x64.cc):

```diff
diff --git a/src/compiler/backend/x64/code-generator-x64.cc b/src/compiler/backend/x64/code-generator-x64.cc
index 61c3a45a16..9c37ed7464 100644
--- a/src/compiler/backend/x64/code-generator-x64.cc
+++ b/src/compiler/backend/x64/code-generator-x64.cc
@@ -731,6 +731,9 @@ CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
   InstructionCode opcode = instr->opcode();
   ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
   switch (arch_opcode) {
+    case kX64Int32Add1: {
+      break;
+    }
     case kArchCallCodeObject: {
       if (HasImmediateInput(instr, 0)) {
         Handle<Code> code = i.InputCode(0);
```

For now we leave our code generation empty, and we can run the test to make sure everything compiles:

```
=== cctest/test-run-wasm/RunWasmTurbofan_Int32Add1 ===
#
# Fatal error in ../../test/cctest/wasm/test-run-wasm.cc, line 37
# Check failed: 11 == r.Call() (11 vs. 10).
```

This failure is expected, since our new instruction is not implemented yet — it is essentially a no-op, so our actual value was unchanged (`10`).

To implement our opcode, we can use the `add` assembly instruction:

```diff
diff --git a/src/compiler/backend/x64/code-generator-x64.cc b/src/compiler/backend/x64/code-generator-x64.cc
index 6c828d6bc4..260c8619f2 100644
--- a/src/compiler/backend/x64/code-generator-x64.cc
+++ b/src/compiler/backend/x64/code-generator-x64.cc
@@ -744,6 +744,11 @@ CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
   InstructionCode opcode = instr->opcode();
   ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
   switch (arch_opcode) {
+    case kX64Int32Add1: {
+      DCHECK_EQ(i.OutputRegister(), i.InputRegister(0));
+      __ addl(i.InputRegister(0), Immediate(1));
+      break;
+    }
     case kArchCallCodeObject: {
       if (HasImmediateInput(instr, 0)) {
         Handle<Code> code = i.InputCode(0);
```

And this makes the test pass:

Luckily for us `addl` is already implemented. If our new opcode required writing a new assembly instruction implementation, we would add it to [`src/compiler/backend/x64/assembler-x64.cc`](https://cs.chromium.org/chromium/src/v8/src/codegen/x64/assembler-x64.cc), where the assembly instruction is encoded into bytes and emitted.

:::note
**Tip:** To inspect the generated code, we can pass `--print-code` to `cctest`.
:::

## Other architectures

In this codelab we only implemented this new instruction for x64. The steps required for other architectures are similar: add TurboFan machine operators, use the platform-dependent files for instruction selection, scheduling, code generation, assembler.

Tip: if we compile what we have done so far on another target, e.g. arm64, we are likely to get errors in linking. To resolve those errors, add `UNIMPLEMENTED()` stubs.
