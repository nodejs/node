---
title: 'Torque Tutorial'
description: 'This document is intended as an introduction to writing Torque builtins, and is targeted towards V8 developers.'
---

This document is intended as an introduction to writing Torque builtins, and is targeted towards V8 developers. Torque replaces CodeStubAssembler as the recommended way to implement new builtins. See [CodeStubAssembler Tutorial](../codegen/csa-tutorial.md) for the CSA version of this guide.

## Writing a Torque builtin

In this section, we will write a simple CSA builtin that takes a single argument, and returns whether it represents the number `42`. The builtin is exposed to JS by installing it on the `Math` object (because we can).

This example demonstrates:

- Creating a Torque builtin with JavaScript linkage, which can be called like a JS function.
- Using Torque to implement simple logic: type distinction, Smi and heap-number handling, conditionals.
- Installation of the CSA builtin on the `Math` object.

In case you’d like to follow along locally, the following code is based off revision [589af9f2](https://chromium.googlesource.com/v8/v8/+/589af9f257166f66774b4fb3008cd09f192c2614).

## Defining `MathIs42`

Torque code is located in `src/builtins/*.tq` files, roughly organized by topic. Since we will be writing a `Math` builtin, we’ll put our definition into `src/builtins/math.tq`.

```torque
namespace math {
  javascript builtin MathIs42(
      js-implicit context: NativeContext, receiver: JSAny)(x: JSAny): Boolean {
    // At this point, x can be basically anything - a Smi, a HeapNumber,
    // undefined, or any other arbitrary JS object. ToNumber_Inline is defined
    // in CodeStubAssembler. It inlines a fast-path (if the argument is a number
    // already) and calls the ToNumber builtin otherwise.
    const number: Number = ToNumber_Inline(x);
    // A typeswitch allows us to switch on the dynamic type of a value. The type
    // system knows that a Number can only be a Smi or a HeapNumber, so this
    // switch is exhaustive.
    typeswitch (number) {
      case (smi: Smi): {
        // The result of smi == 42 is not a Javascript boolean, so we use a
        // conditional to create a Javascript boolean value.
        return smi == 42 ? True : False;
      }
      case (heapNumber: HeapNumber): {
        return Convert<float64>(heapNumber) == 42 ? True : False;
      }
    }
  }
}
```

We put the definition in the Torque namespace `math`, which already contains other math-related builtins.

## Attaching `Math.is42`

Builtin objects such as `Math` are set up mostly in `src/init/bootstrapper.cc` (with some setup occurring in `.js` files). Attaching our new builtin is simple:

```cpp
// Existing code to set up Math, included here for clarity.
Handle<JSObject> math = factory->NewJSObject(cons, AllocationType::kOld);
JSObject::AddProperty(global, name, math, DONT_ENUM);
// […snip…]
SimpleInstallFunction(isolate_, math, "is42", Builtins::kMathIs42, 1, true);
```

Now that `is42` is attached, it can be called from JS:

```bash
$ out/x64.optdebug/d8
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

Builtins can also be created with stub linkage (instead of JS linkage as we used above in `MathIs42`). Such builtins can be useful to extract commonly-used code into a separate code object that can be used by multiple callers, while the code is only produced once. Let’s extract the code that handles heap numbers into a separate builtin called `HeapNumberIs42`, and call it from `MathIs42`.

The definition is also straightforward. The only difference to our builtin with Javascript linkage is that we omit the keyword `javascript` and there is no receiver argument.

```torque
namespace math {
  builtin HeapNumberIs42(implicit context: Context)(heapNumber: HeapNumber):
      Boolean {
    return Convert<float64>(heapNumber) == 42 ? True : False;
  }

  javascript builtin MathIs42(js-implicit context: NativeContext, receiver: JSAny)(
      x: JSAny): Boolean {
    const number: Number = ToNumber_Inline(x);
    typeswitch (number) {
      case (smi: Smi): {
        return smi == 42 ? True : False;
      }
      case (heapNumber: HeapNumber): {
        // Instead of handling heap numbers inline, we now call our new builtin.
        return HeapNumberIs42(heapNumber);
      }
    }
  }
}
```

Why should you care about builtins at all? Why not leave the code inline (or extracted into macros for better readability)?

An important reason is code space: builtins are generated at compile-time and included in the V8 snapshot or embedded into the binary. Extracting large chunks of commonly used code to separate builtins can quickly lead to space savings in the 10s to 100s of KBs.

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
