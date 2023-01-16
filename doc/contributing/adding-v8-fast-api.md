# Adding V8 Fast API

Node.js uses [V8](https://v8.dev/) as its JavaScript engine.
Embedder functions implemented in C++ incur a high overhead, so V8
provides an API to implement fast-path C functions which may be invoked directly
from JITted code. These functions also come with additional constraints,
for example, they may not trigger garbage collection.

## Limitations

* Fast API functions may not trigger garbage collection. This means by proxy
  that JavaScript execution and heap allocation are also forbidden, including
  `v8::Array::Get()` or `v8::Number::New()`.
* Throwing errors is not available on fast API, but can be done
  through the fallback to slow API.
* Not all parameter and return types are supported in fast API calls.
  For a full list, please look into
  [`v8-fast-api-calls.h`](../../deps/v8/include/v8-fast-api-calls.h) file.

## Requirements

* Each unique fast API function signature should be defined inside the
  [`external references`](../../src/node_external_reference.h) file.
* To test fast APIs, make sure to run the tests in a loop with a decent
  iterations count to trigger V8 for optimization and to prefer the fast API
  over slow one.
* The fast callback must be idempotent up to the point where error and fallback
  conditions are checked, because otherwise executing the slow callback might
  produce visible side effects twice.

## Fallback to slow path

Fast API supports fallback to slow path in case logically it is wise to do so,
for example when providing a detailed error, or need to trigger JavaScript.
Fallback mechanism can be enabled and changed from the C++ implementation of
the fast API function declaration.

Passing a `true` value to `fallback` option will force V8 to run the slow path
with the same arguments.

In V8, the options fallback is defined as `FastApiCallbackOptions` inside
[`v8-fast-api-calls.h`](../../deps/v8/include/v8-fast-api-calls.h).

* C++ land

  Example of a conditional fast path on C++

  ```cpp
  // Anywhere in the execution flow, you can set fallback and stop the execution.
  static double divide(const int32_t a,
                       const int32_t b,
                       v8::FastApiCallbackOptions& options) {
    if (b == 0) {
      options.fallback = true;
      return 0;
    } else {
      return a / b;
    }
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
  #include "v8-fast-api-calls.h"

  namespace node {
  namespace custom_namespace {

  static void divide(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    CHECK_GE(args.Length(), 2);
    CHECK(args[0]->IsInt32());
    CHECK(args[1]->IsInt32());
    auto a = args[0].As<v8::Int32>();
    auto b = args[1].As<v8::Int32>();

    if (b->Value() == 0) {
      return node::THROW_ERR_INVALID_STATE(env, "Error");
    }

    double result = a->Value() / b->Value();
    args.GetReturnValue().Set(result);
  }

  static double FastDivide(const int32_t a,
                           const int32_t b,
                           v8::FastApiCallbackOptions& options) {
    if (b == 0) {
      options.fallback = true;
      return 0;
    } else {
      return a / b;
    }
  }

  CFunction fast_divide_(CFunction::Make(FastDivide));

  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context,
                         void* priv) {
    SetFastMethod(context, target, "divide", Divide, &fast_divide_);
  }

  void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
    registry->Register(Divide);
    registry->Register(FastDivide);
    registry->Register(fast_divide_.GetTypeInfo());
  }

  } // namespace custom_namespace
  } // namespace node

  NODE_BINDING_CONTEXT_AWARE_INTERNAL(custom_namespace,
                                      node::custom_namespace::Initialize);
  NODE_BINDING_EXTERNAL_REFERENCE(
                        custom_namespace,
                        node::custom_namespace::RegisterExternalReferences);
  ```

* Update external references ([`node_external_reference.h`](../../src/node_external_reference.h))

  Since our implementation used
  `double(const int32_t a, const int32_t b, v8::FastApiCallbackOptions& options)`
  signature, we need to add it to external references and in
  `ALLOWED_EXTERNAL_REFERENCE_TYPES`.

  Example declaration:

  ```cpp
  using CFunctionCallbackReturningDouble = double (*)(const int32_t a,
                                                      const int32_t b,
                                                      v8::FastApiCallbackOptions& options);
  ```
