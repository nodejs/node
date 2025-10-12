# Adding V8 Fast API callbacks

Node.js uses [V8](https://v8.dev/) as its JavaScript engine. Embedding
functions implemented in C++ incurs a high overhead, so V8 provides an API to
implement native C++ functions which may be invoked directly from JIT-ed code.

Early iterations of the Fast API imposed significant constraints on these
functions, such as not allowing re-entry into JavaScript execution and not
throwing errors directly from fast calls. As of V8 12.6, these constraints no
longer exist; however, a function whose execution cost is far higher than its
calling cost is unlikely to benefit from having a "fast" variant, so some
judgement is required when considering whether or not to add a Fast API
callback.

## Basics

A Fast API callback must correspond to a conventional ("slow") implementation
of the same callback. Compare the two conventions:

```cpp
// Conventional ("slow") implementation
void IsEven(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args[0]->IsInt32()) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "argument must be an integer");
  }

  int32_t n = args[0]->Int32Value(env->context()).FromJust();
  bool result = n % 2 == 0;
  args.GetReturnValue().Set(result);
}

// Fast implementation
bool FastIsEven(v8::Local<v8::Value> receiver,
                const int32_t n) {
  return n % 2 == 0;
}
static v8::CFunction fast_is_even(v8::CFunction::Make(FastIsEven));
```

The main differences between the two call conventions are:

* A conventional call passes its arguments as `v8::Value` objects, via a
  `v8::FunctionCallbackInfo` object. A Fast API call passes its arguments
  directly to the C++ function, as native C++ types where possible.
* A conventional call passes its return value via a `v8::ReturnValue` object,
  accessible via the `v8::FunctionCallbackInfo` object. A Fast API call returns
  its value directly from the C++ function, as a native C++ type.
* A conventional call can pass any number of arguments of any type, which must
  be validated within the implementation. A Fast API callback will only ever be
  called in compliance with its function signature, so the `FastIsEven` example
  above will only ever be called with a single argument of type `int32_t`. Any
  calls from JavaScript whose arguments do not correspond to a fast callback
  signature will be directed to the slow path by V8, even if the function is
  optimized.
* The fast callback cannot be bound directly. It must first be used to build a
  `v8::CFunction` handle, which is passed alongside the conventional callback
  when binding the function.

## Argument and return types

The following are valid argument types in Fast API callback signatures:

* `bool`
* `int32_t`
* `uint32_t`
* `int64_t`
* `uint64_t`
* `float`
* `double`
* `v8::Local<v8::Value>` (analogous to `any`)
* `v8::FastOneByteString&` (analogous to `string`, but _only_ allows sequential
  one-byte strings, which is often not useful)

<!--
Deliberately omitted:
* `void *` (external object pointers)
* `v8::Local<v8::Object>` (this is actually treated the same as
                           v8::Local<v8::Value> by the API - in other words, V8
                           will pass _any_ JS value in an "object" handle,
                           whether it's an object or not, which is effectively
                           an unsafe cast and can lead to unexpected errors)
-->

The list of valid return types is similar:

* `void`
* `bool`
* `int32_t`
* `uint32_t`
* `int64_t`
* `uint64_t`
* `float`
* `double`

<!-- * `void *` -->

### Prepending a `receiver` argument

V8 will always pass the "receiver" (the `this` value of the JavaScript function
call) in the first argument position. The arguments to the JavaScript function
call are then passed from the second position onwards.

```cpp
// Let's say that this function was bound as a method on some object,
// such that it would be called in JavaScript as `object.hasProperty(foo)`.
bool FastHasProperty(v8::Local<v8::Value> receiver,
                     v8::Local<v8::Value> property,
                     v8::FastApiCallbackOptions& options) {
  v8::Isolate* isolate = options.isolate;

  if (!receiver->IsObject()) {
    // invalid `this` value; throw some kind of error here
  }

  bool result;
  if (!receiver.As<v8::Object>()->Has(isolate->GetCurrentContext(),
                                      property).To(&result)) {
    // error pending in V8, value is ignored
    return false;
  }

  return result;
}
```

Even if your function binding does not need access to the receiver, you must
still prepend it to your function arguments.

```cpp
bool FastIsObject(v8::Local<v8::Value> receiver, // unused
                  v8::Local<v8::Value> value) {
  return value->IsObject();
}
```

### Appending an `options` argument (optional)

Fast callbacks may add an optional final function argument of type
`v8::FastApiCallbackOptions&`. This is required if the callback interacts with
the isolate in any way: see
[Stack-allocated objects and garbage collection](#stack-allocated-objects-and-garbage-collection)
and [Handling errors](#handling-errors).

```cpp
void FastThrowExample(v8::Local<v8::Value> receiver,
                      const int32_t n,
                      v8::FastApiCallbackOptions& options) {
  if (IsEvilNumber(n)) {
    v8::HandleScope handle_scope(options.isolate);
    THROW_ERR_INVALID_ARG_VALUE(options.isolate, "Begone, foul spirit!");
  }
}
```

## Registering a Fast API callback

Compare registering a conventional API binding:

```cpp
void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  SetMethodNoSideEffect(context, target, "isEven", IsEven);
}
```

with registering an API binding with a fast callback:

```cpp
void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  SetFastMethodNoSideEffect(context,
                            target,
                            "isEven",
                            SlowIsEven,
                            &fast_is_even);
}
```

The Fast API equivalents of the method binding functions take an additional
parameter, which specifies the fast callback(s).

In the majority of cases, there will only be a single fast callback, and the
additional parameter should be a pointer to the `v8::CFunction` object
constructed by the call to `CFunction::Make`.

In rare cases, there may be more than one fast callback, _eg._ if the function
accepts optional arguments. In this case, the additional parameter should be a
reference to an array of `v8::CFunction` objects, which is used to initialize a
`v8::MemorySpan<v8::CFunction>`:

```cpp
int32_t FastFuncWithoutArg(v8::Local<v8::Value> receiver) {
  return -1;
}
int32_t FastFuncWithArg(v8::Local<v8::Value> receiver,
                        const v8::FastOneByteString& s) {
  return s.length;
}
static CFunction fast_func_callbacks[] = {CFunction::Make(FastFuncWithoutArg),
                                          CFunction::Make(FastFuncWithArg)};

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  SetFastMethodNoSideEffect(context,
                            target,
                            "func",
                            SlowFunc,
                            fast_func_callbacks);
}
```

In addition, all method bindings should be registered with the external
reference registry. This is done by passing both the conventional callback
pointer and the `v8::CFunction` handle to `registry->Register`.

```cpp
void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(SlowIsEven);
  registry->Register(fast_is_even);
}
```

Omitting this step can lead to fatal exceptions if the callback ends up in a
snapshot (either the built-in snapshot, or a user-land one). Refer to the
[binding functions documentation](../../src/README.md#registering-binding-functions-used-in-bootstrap)
for more information.

## Type checking

A callback argument that is a "primitive" C++ type (for example, `int32_t`)
does not require type checks, as V8 will only ever invoke the fast callback if
the argument in the JavaScript function call matches the corresponding argument
type in the fast callback signature.

Non-primitive arguments (such as TypedArrays) are passed to Fast API callbacks
as `v8::Local<v8::Value>`. However, registering a fast callback with this
argument type signals to the V8 engine that it can invoke the fast callback
with _any value_ as that argument.

If using arguments of type `v8::Local<v8::Value>`, then it is the
implementation's responsibility to ensure that the arguments are validated
before casting or otherwise consuming them. This can either take place within
the C++ callbacks themselves, or within a JavaScript wrapper function that
performs any necessary validation before calling the bound function.

## Stack-allocated objects and garbage collection

The Fast API now allows access to the isolate, and allows allocation of
`v8::Local` handles on the stack.

A fast callback intending to make use of this functionality should accept a
final argument of type `v8::FastApiCallbackOptions&`. V8 will pass the isolate
pointer in `options.isolate`.

If a fast callback creates any `v8::Local` handles within the fast callback,
then it must first initialize a new `v8::HandleScope` to ensure that the
handles are correctly scoped and garbage-collected.

```cpp
bool FastIsIterable(v8::Local<v8::Value> receiver,
                    v8::Local<v8::Value> argument,
                    v8::FastApiCallbackOptions& options) {
  if (!argument->IsObject()) {
    return false;
  }

  // In order to create any Local handles, we first need a HandleScope
  v8::HandleScope HandleScope(options.isolate);

  v8::Local<v8::Object> object = argument.As<v8::Object>();
  v8::Local<v8::Value> value;
  if (!object->Get(options.isolate->GetCurrentContext(),
                   v8::Symbol::GetIterator(options.isolate)).ToLocal(&value)) {
    return false;
  }
  return value->IsFunction();
}
```

The same applies if the fast callback calls other functions which themselves
create `v8::Local` handles, unless those functions create their own
`v8::HandleScope`. In general, if the fast callback interacts with
`v8::Local` handles within the body of the callback, it likely needs a handle
scope.

## Debug tracking of Fast API callbacks

In order to allow the test suite to track when a function call uses the Fast
API path, add the `TRACK_V8_FAST_API_CALL` macro to your fast callback.

```cpp
bool FastIsEven(v8::Local<v8::Value> receiver,
                const int32_t n) {
  TRACK_V8_FAST_API_CALL("util.isEven");
  return n % 2 == 0;
}
```

The tracking key must be unique, and should be of the form:

`<namespace> "." <function> [ "." <subpath> ]`

The above example assumes that the fast callback is bound to the `isEven`
method of the `util` module binding. To track specific subpaths within the
callback, use a key with a subpath specifier, like `"util.isEven.error"`.

These tracking events can be observed in debug mode, and are used to test that
the fast path is being correctly invoked. See
[Testing Fast API callbacks](#testing-fast-api-callbacks) for details.

## Handling errors

It is now possible to throw errors from within fast API calls.

Any fast callback that might potentially need to throw an error back to the
JavaScript environment should accept a final `options` argument of type
`v8::FastApiCallbackOptions&`. V8 will pass the isolate pointer in
`options.isolate`.

The callback should then throw a JavaScript error in the standard fashion. It
also needs to return a dummy value, to satisfy the function signature.

As above, initializing a `v8::HandleScope` is mandatory before any operations
which create local handles.

```cpp
static double FastDivide(v8::Local<v8::Value> receiver,
                         const int32_t a,
                         const int32_t b,
                         v8::FastApiCallbackOptions& options) {
  if (b == 0) {
    TRACK_V8_FAST_API_CALL("math.divide.error");
    v8::HandleScope handle_scope(options.isolate);
    THROW_ERR_INVALID_ARG_VALUE(options.isolate,
                                "cannot divide by zero");
    return 0; // dummy value, ignored by V8
  }

  TRACK_V8_FAST_API_CALL("math.divide.ok");
  return a / b;
}
```

## Testing Fast API callbacks

To force V8 to use a Fast API path in testing, use V8 natives to force
optimization of the JavaScript function that calls the fast target. If
importing the binding directly, you will need to wrap the call within a
JavaScript function first.

```js
// Flags: --allow-natives-syntax --expose-internals --no-warnings

const common = require('../common');
const assert = require('node:assert');

const { internalBinding } = require('internal/test/binding');
const { isEven } = internalBinding('...');

function testFastAPICall() {
  assert.strictEqual(isEven(0), true);
}

// The first V8 directive prepares the wrapper function for optimization.
eval('%PrepareFunctionForOptimization(testFastAPICall)');
// This call will use the slow path.
testFastAPICall();

// The second V8 directive will trigger optimization.
eval('%OptimizeFunctionOnNextCall(testFastAPICall)');
// This call will use the fast path.
testFastAPICall();
```

In debug builds, it is possible to observe
[`TRACK_V8_FAST_API_CALL`](#debug-tracking-of-fast-api-callbacks) events using
the`getV8FastApiCallCount` function, to verify that the fast path is being
correctly invoked. All fast callbacks should be tested in this way.

```js
function testFastAPICalls() {
  assert.strictEqual(isEven(1), false);
  assert.strictEqual(isEven(2), true);
}

eval('%PrepareFunctionForOptimization(testFastAPICalls)');
testFastAPICalls();
eval('%OptimizeFunctionOnNextCall(testFastAPICalls)');
testFastAPICalls();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('util.isEven'), 2);
}
```

## Example

A typical function that communicates between JavaScript and C++ is as follows.

* On the JavaScript side:

  ```js
  const { divide } = internalBinding('custom_namespace');
  ```

* On the C++ side:

  ```cpp
  #include "node_debug.h"
  #include "v8-fast-api-calls.h"

  namespace node {
  namespace custom_namespace {

  using v8::FastApiCallbackOptions;
  using v8::FunctionCallbackInfo;
  using v8::HandleScope;
  using v8::Int32;
  using v8::Number;
  using v8::Value;

  static void SlowDivide(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    if (!args[0]->IsInt32() || !args[1]->IsInt32()) {
      return THROW_ERR_INVALID_ARG_TYPE(env, "operands must be integers");
    }
    auto a = args[0].As<Int32>();
    auto b = args[1].As<Int32>();

    if (b->Value() == 0) {
      return THROW_ERR_INVALID_ARG_VALUE(env, "cannot divide by zero");
    }

    double result = a->Value() / b->Value();
    args.GetReturnValue().Set(Number::New(env->isolate(), result));
  }

  static double FastDivide(v8::Local<v8::Value> receiver,
                           const int32_t a,
                           const int32_t b,
                           FastApiCallbackOptions& options) {
    if (b == 0) {
      TRACK_V8_FAST_API_CALL("custom_namespace.divide.error");
      HandleScope handle_scope(options.isolate);
      THROW_ERR_INVALID_ARG_VALUE(options.isolate, "cannot divide by zero");
      return 0;
    }

    TRACK_V8_FAST_API_CALL("custom_namespace.divide.ok");
    return a / b;
  }

  static CFunction fast_divide(CFunction::Make(FastDivide));

  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context,
                         void* priv) {
    SetFastMethodNoSideEffect(context,
                              target,
                              "divide",
                              SlowDivide,
                              &fast_divide);
  }

  void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
    registry->Register(SlowDivide);
    registry->Register(fast_divide);
  }

  } // namespace custom_namespace
  } // namespace node

  NODE_BINDING_CONTEXT_AWARE_INTERNAL(custom_namespace,
                                      node::custom_namespace::Initialize);
  NODE_BINDING_EXTERNAL_REFERENCE(
                        custom_namespace,
                        node::custom_namespace::RegisterExternalReferences);
  ```

* In the unit tests:

  Since the Fast API callback uses `TRACK_V8_FAST_API_CALL`, we can ensure that
  the fast paths are taken and test them by writing tests that force
  V8 optimizations and check the counters.

In the unit tests, since the fast API function uses `TRACK_V8_FAST_API_CALL`,
we can ensure that the fast paths are taken and test them by writing tests that
force V8 optimizations and check the counters.

```js
// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';
const common = require('../common');

const { internalBinding } = require('internal/test/binding');
// We could also require a function that uses the internal binding internally.
const { divide } = internalBinding('custom_namespace');

// The function that will be optimized. It has to be a function written in
// JavaScript. Since `divide` comes from the C++ side, we need to wrap it.
function testFastPath(a, b) {
  return divide(a, b);
}

eval('%PrepareFunctionForOptimization(testFastPath)');
// This call will let V8 know about the argument types that the function expects.
assert.strictEqual(testFastPath(6, 3), 2);

eval('%OptimizeFunctionOnNextCall(testFastPath)');
assert.strictEqual(testFastPath(8, 2), 4);
assert.throws(() => testFastPath(1, 0), {
  code: 'ERR_INVALID_STATE',
});

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('custom_namespace.divide.ok'), 1);
  assert.strictEqual(getV8FastApiCallCount('custom_namespace.divide.error'), 1);
}
```
