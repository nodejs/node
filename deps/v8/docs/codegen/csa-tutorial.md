---
title: 'CodeStubAssembler Tutorial'
description: 'This document is intended as an introduction to writing CodeStubAssembler builtins, and is targeted towards V8 developers.'
---

This document is intended as an introduction to writing CodeStubAssembler builtins, and is targeted towards V8 developers.

:::note
**Note:** [Torque](/docs/torque) replaces CodeStubAssembler as the recommended way to implement new builtins. See [Torque builtins](/docs/torque-builtins) for the Torque version of this guide.
:::

## Builtins

In V8, builtins can be seen as chunks of code that are executable by the VM at runtime. A common use case is to implement the functions of builtin objects (such as RegExp or Promise), but builtins can also be used to provide other internal functionality (e.g. as part of the IC system).

V8’s builtins can be implemented using a number of different methods (each with different trade-offs):

- **Platform-dependent assembly language**: can be highly efficient, but need manual ports to all platforms and are difficult to maintain.
- **C++**: very similar in style to runtime functions and have access to V8’s powerful runtime functionality, but usually not suited to performance-sensitive areas.
- **JavaScript**: concise and readable code, access to fast intrinsics, but frequent usage of slow runtime calls, subject to unpredictable performance through type pollution, and subtle issues around JS semantics. *(Mostly deprecated now for new builtins, replaced by Torque).*
- **CodeStubAssembler**: provides efficient low-level functionality that is very close to assembly language while remaining platform-independent and preserving readability.
- **Torque**: The recommended high-level language that generates CSA code.

The remaining document focuses on CodeStubAssembler and gives a brief tutorial for developing a simple CodeStubAssembler (CSA) builtin exposed to JavaScript.

## CodeStubAssembler

V8’s CodeStubAssembler is a custom, platform-agnostic assembler that provides low-level primitives as a thin abstraction over assembly, but also offers an extensive library of higher-level functionality.

```cpp
// Low-level:
// Loads the pointer-sized data at addr into value.
TNode<IntPtrT> addr = /* ... */;
TNode<Object> value = Load<Object>(addr);

// And high-level:
// Performs the JS operation ToString(object).
// ToString semantics are specified at https://tc39.es/ecma262/#sec-tostring.
TNode<Context> context = /* ... */;
TNode<Object> object = /* ... */;
TNode<String> string = ToString(context, object);
```

CSA builtins run through part of the TurboFan compilation pipeline (including block scheduling and register allocation, but notably not through optimization passes) which then emits the final executable code.

## Writing a CodeStubAssembler builtin

In this section, we will write a simple CSA builtin that takes a single argument, and returns whether it represents the number `42`. The builtin is exposed to JS by installing it on the `Math` object (because we can).

This example demonstrates:

- Creating a CSA builtin with JavaScript linkage, which can be called like a JS function.
- Using CSA to implement simple logic: Smi and heap-number handling, conditionals, and calls to TFS builtins.
- Using CSA Variables (`TVARIABLE`).
- Installation of the CSA builtin on the `Math` object.

## Declaring `MathIs42`

Builtins are declared in the `BUILTIN_LIST_BASE` macro in `src/builtins/builtins-definitions.h`. To create a new CSA builtin with JS linkage and one parameter named `X`:

```cpp
#define BUILTIN_LIST_BASE(CPP, API, TFJ, TFC, TFS, TFH, ASM, DBG)              \
  // […snip…]
  TFJ(MathIs42, 1, kX)                                                         \
  // […snip…]
```

Note that `BUILTIN_LIST_BASE` takes several different macros that denote different builtin kinds (see inline documentation for more details). CSA builtins specifically are split into:

- **TFJ**: JavaScript linkage.
- **TFS**: Stub linkage.
- **TFC**: Stub linkage builtin requiring a custom interface descriptor (e.g. if arguments are untagged or need to be passed in specific registers).
- **TFH**: Specialized stub linkage builtin used for IC handlers.

## Defining `MathIs42`

Builtin definitions are located in `src/builtins/builtins-*-gen.cc` files, roughly organized by topic. Since we will be writing a `Math` builtin, we’ll put our definition into `src/builtins/builtins-math-gen.cc`.

```cpp
// TF_BUILTIN is a convenience macro that creates a new subclass of the given
// assembler behind the scenes.
TF_BUILTIN(MathIs42, MathBuiltinsAssembler) {
  // Load the current function context (an implicit argument for every stub)
  // and the X argument. Note that we can refer to parameters by the names
  // defined in the builtin declaration.
  TNode<Context> const context = Parameter<Context>(Descriptor::kContext);
  TNode<Object> const x = Parameter<Object>(Descriptor::kX);

  // At this point, x can be basically anything - a Smi, a HeapNumber,
  // undefined, or any other arbitrary JS object. Let’s call the ToNumber
  // builtin to convert x to a number we can use.
  // CallBuiltin can be used to conveniently call any CSA builtin.
  TNode<Number> const number = CAST(CallBuiltin(Builtins::kToNumber, context, x));

  // Create a CSA variable to store the resulting value.
  TVARIABLE(Object, var_result);

  // We need to define a couple of labels which will be used as jump targets.
  Label if_issmi(this), if_isheapnumber(this), out(this);

  // ToNumber always returns a number. We need to distinguish between Smis
  // and heap numbers - here, we check whether number is a Smi and conditionally
  // jump to the corresponding labels.
  Branch(TaggedIsSmi(number), &if_issmi, &if_isheapnumber);

  // Binding a label begins generating code for it.
  BIND(&if_issmi);
  {
    // SelectBooleanConstant returns the JS true/false values depending on
    // whether the passed condition is true/false. The result is bound to our
    // var_result variable, and we then unconditionally jump to the out label.
    TNode<Smi> smi_number = CAST(number);
    var_result.Bind(SelectBooleanConstant(SmiEqual(smi_number, SmiConstant(42))));
    Goto(&out);
  }

  BIND(&if_isheapnumber);
  {
    // ToNumber can only return either a Smi or a heap number. Just to make sure
    // we add an assertion here that verifies number is actually a heap number.
    CSA_ASSERT(this, IsHeapNumber(number));
    
    // Heap numbers wrap a floating point value. We need to explicitly extract
    // this value, perform a floating point comparison, and again bind
    // var_result based on the outcome.
    TNode<HeapNumber> hn_number = CAST(number);
    TNode<Float64T> const value = LoadHeapNumberValue(hn_number);
    TNode<BoolT> const is_42 = Float64Equal(value, Float64Constant(42));
    var_result.Bind(SelectBooleanConstant(is_42));
    Goto(&out);
  }

  BIND(&out);
  {
    TNode<Object> const result = var_result.value();
    CSA_ASSERT(this, IsBoolean(result));
    Return(result);
  }
}
```

## Attaching `Math.Is42`

Builtin objects such as `Math` are set up mostly in `src/init/bootstrapper.cc`. Attaching our new builtin is simple:

```cpp
// Existing code to set up Math, included here for clarity.
Handle<JSObject> math = factory->NewJSObject(cons, TENURED);
JSObject::AddProperty(global, name, math, DONT_ENUM);
// […snip…]
SimpleInstallFunction(math, "is42", Builtins::kMathIs42, 1, true);
```

Now that `Is42` is attached, it can be called from JS:

```bash
$ out/debug/d8
d8> Math.is42(42);
true
d8> Math.is42('42.0');
true
d8> Math.is42(true);
false
d8> Math.is42({ valueOf: () => 42 });
true
```

## Defining and calling a builtin with stub linkage

CSA builtins can also be created with stub linkage (instead of JS linkage as we used above in `MathIs42`). Such builtins can be useful to extract commonly-used code into a separate code object that can be used by multiple callers, while the code is only produced once. Let’s extract the code that handles heap numbers into a separate builtin called `MathIsHeapNumber42`, and call it from `MathIs42`.

Defining and using TFS stubs is easy; declaration are again placed in `src/builtins/builtins-definitions.h`:

```cpp
#define BUILTIN_LIST_BASE(CPP, API, TFJ, TFC, TFS, TFH, ASM, DBG)              \
  // […snip…]
  TFS(MathIsHeapNumber42, kX)                                                  \
  TFJ(MathIs42, 1, kX)                                                         \
  // […snip…]
```

Note that currently, order within `BUILTIN_LIST_BASE` does matter. Since `MathIs42` calls `MathIsHeapNumber42`, the former needs to be listed after the latter.

The definition is also straightforward. In `src/builtins/builtins-math-gen.cc`:

```cpp
// Defining a TFS builtin works exactly the same way as TFJ builtins.
TF_BUILTIN(MathIsHeapNumber42, MathBuiltinsAssembler) {
  TNode<Object> const x = Parameter<Object>(Descriptor::kX);
  CSA_ASSERT(this, IsHeapNumber(x));
  TNode<HeapNumber> hn_x = CAST(x);
  TNode<Float64T> const value = LoadHeapNumberValue(hn_x);
  TNode<BoolT> const is_42 = Float64Equal(value, Float64Constant(42));
  Return(SelectBooleanConstant(is_42));
}
```

Finally, let’s call our new builtin from `MathIs42`:

```cpp
TF_BUILTIN(MathIs42, MathBuiltinsAssembler) {
  // […snip…]
  BIND(&if_isheapnumber);
  {
    // Instead of handling heap numbers inline, we now call into our new TFS stub.
    var_result.Bind(CallBuiltin(Builtins::kMathIsHeapNumber42, context, number));
    Goto(&out);
  }
  // […snip…]
}
```

Why should you care about TFS builtins at all? Why not leave the code inline (or extracted into a helper method for better readability)?

An important reason is code space: builtins are generated at compile-time and included in the V8 snapshot, thus unconditionally taking up (significant) space in every created isolate. Extracting large chunks of commonly used code to TFS builtins can quickly lead to space savings in the 10s to 100s of KBs.

## Testing stub-linkage builtins

Even though our new builtin uses a non-standard (at least non-C++) calling convention, it’s possible to write test cases for it. The following code can be added to `test/cctest/compiler/test-run-stubs.cc` to test the builtin on all platforms:

```cpp
TEST(MathIsHeapNumber42) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Heap* heap = isolate->heap();
  Zone* zone = scope.main_zone();

  StubTester tester(isolate, zone, Builtins::kMathIs42);
  Handle<Object> result1 = tester.Call(Handle<Smi>(Smi::FromInt(0), isolate));
  CHECK(result1->BooleanValue());
}
```
