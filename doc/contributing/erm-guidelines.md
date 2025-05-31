# Explicit Resource Management (ERM) Guidelines

Explicit Resource Management is a capability that was introduced to the JavaScript
langauge in 2025. It provides a way of marking objects as disposable resources such
that the JavaScript engine will automatically invoke disposal methods when the
object is no longer in scope. For example:

```js
class MyResource {
  dispose() {
    console.log('Resource disposed');
  }

  [Symbol.dispose]() {
    this.dispose();
  }
}

{
  using resource = new MyResource();
  // When this block exits, the `Symbol.dispose` method will be called
  // automatically by the JavaScript engine.
}
```

This document outlines some specific guidelines for using ERM in the Node.js
project -- specifically, guidelines around how to make objects disposable and
how to introduce ERM capabilities into existing APIs.

## Some background

Objects can be made disposable by implementing either, or both, the
`Symbol.dispose` and `Symbol.asyncDispose` methods:

```js
class MyResource {
  [Symbol.dispose]() {
    // Synchronous disposal logic
  }

  async [Symbol.asyncDispose]() {
    // Asynchronous disposal logic
  }
}
```

An object that implements `Symbol.dispose` can be used with the `using`
statement, which will automatically call the `Symbol.dispose` method when the
object goes out of scope. If an object implements `Symbol.asyncDispose`, it can
be used with the `await using` statement in an asynchronous context.

```mjs
{
  using resource = new MyResource();
  await using asyncResource = new MyResource();
}
```

Importantly, it is necessary to understand that the design of ERM makes it
possible for user code to call the `Symbol.dispose` or `Symbol.asyncDispose`
methods directly, outside of the `using` or `await using` statements. These
can also be called multiple times and by any code that is holding a reference
to the object. That is to say, ERM does not imply ownership of the object. It
is not a form of RAII (Resource Acquisition Is Initialization) as seen in some
other languages and there is no notion of exclusive ownership of the object.
A disposable object can become disposed at any time.

The `Symbol.dispose` and `Symbol.asyncDispose` methods are called in both
successful and exceptional exits from the scopes in which the using keyword
is used. This means that if an exception is thrown within the scope, the
disposal methods will still be called. However, when the disposal methods are
called they are not aware of the context in which they were called. These
methods will not receive any information about the exception that was thrown
or whether an exception was thrown at all. This means that it is safest to
assume that the disposal methods will be called in a context where the object
may not be in a valid state or that an exception may be pending.

## Guidelines for Disposable Objects

So with this is mind, it is necessary to outline some guidelines for disposers:

1. Disposers should be idempotent. Multiple calls to the disposal methods
   should not cause any issues or have any additional side effects.
2. Disposers should assume that it is being called in an exception context.
   Always assume there is likely a pending exception and that if the object
   has not been explicitly closed when the disposal method is called, that
   the object should be disposed as if an exception had occurred. For instance,
   if the object API exposes both a `close()` method and an `abort()` method,
   the disposal method should call `abort()` if the object is not already
   closed. If there is no difference in disposing in success or exception
   contexts, then separate disposal methods are unnecessary.
3. Disposers may throw their own exceptions but this is not recommended.
   If a disposer throws an exception while there is another pending
   exception, then both exceptions will be wrapped in a `SupressedError`
   that masks both. This makes it difficult to understand the context
   in which the exceptions were thrown. It is, however, not possible to
   completely prevent exceptions from being thrown in the disposal methods
   so this guideline is more of a recommendation than a hard rule.
4. Disposable objects should expose explicit disposal methods in addition
   to the `Symbol.dispose` and `Symbol.asyncDispose` methods. This allows
   user code to explicitly dispose of the object without using the `using`
   or `await using` statements. For example, a disposable object might
   expose a `close()` method that can be called to dispose of the object.
   The `Symbol.dispose` and `Symbol.asyncDispose` methods should delegate to
   these explicit disposal methods.
5. Because it is safest to assume that the disposal method will be called
   in an exception context, it is strongly recommended to just generally avoid
   use of `Symbol.asyncDispose` as much as possible. Asynchronous disposal can
   lead delaying the handling of exceptions and can make it difficult to
   reason about the state of the object while the disposal is in progress and
   is often an anti-pattern. Disposal in an exception context should always
   be synchronous and immediate.

### Example Disposable Object

```js
class MyDisposableResource {
  constructor() {
    this.closed = false;
  }

  doSomething() {
    if (maybeShouldThrow()) {
      throw new Error('Something went wrong');
    }
  }

  close() {
    // Gracefully close the resource.
    if (this.closed) return;
    this.closed = true;
    console.log('Resource closed');
  }

  abort(maybeError) {
    // Abort the resource, optionally with an exception. Calling this
    // method multiple times should not cause any issues or additional
    // side effects.
    if (this.closed) return;
    this.closed = true;
    if (maybeError) {
      console.error('Resource aborted due to error:', maybeError);
    } else {
      console.log('Resource aborted');
    }
  }

  [Symbol.dispose]() {
    // Note that when this is called, we cannot pass any pending
    // exceptions to the abort method because we do not know if
    // there is a pending exception or not.
    this.abort();
  }
}
```

Then in use:

```js
{
  using resource = new MyDisposableResource();
  // Do something with the resource that might throw an error
  resource.doSomething();
  resource.close();
}
```

Here, if an error is thrown in the `doSomething()` method, the `Symbol.dispose`
method will still be called when the block exits, ensuring that the resource is
disposed of properly using the `abort()` method. If no error is thrown, the
`close()` method is called explicitly to gracefully close the resource. When the
block exits, the `Symbol.dispose` method is still called but it will be a non-op
since the resource has already been closed.

To deal with errors that may occur during disposal, it is necessary to wrap
the disposal block in a try-catch:

```js
try {
  using resource = new MyDisposableResource();
  // Do something with the resource that might throw an error
  resource.doSomething();
  resource.close();
} catch (error) {
  // Error might be the actual error thrown in the block, or might
  // be a SupressedError if an error was thrown during disposal and
  // there was a pending exception already.
  if (error instanceof SuppressedError) {
    console.error('An error occurred during disposal masking pending error:',
                  error.error, error.suppressed);
  } else {
    console.error('An error occurred:', error);
  }
}
```

## Guidelines for Introducing ERM into Existing APIs

Introducing ERM capabilities into existing APIs can be tricky.

The best way to understand the issues is to look at a real world example. PR
[58516](https://github.com/nodejs/node/pull/58516) is a good case. This PR
sought to introduce ERM capabilities into the `fs.mkdtemp` API such that a
temporary directory could be created and automatically disposed of when the
scope in which it was created exited. However, the existing implementation of
the `fs.mkdtemp` API returns a string value that cannot be made disposable.
There are also sync, callback, and promise-based variations of the existing
API that further complicate the situation.

In the initial proposal, the `fs.mkdtemp` API was changed to return an object
that implements the `Symbol.dispose` method but only if a specific option is
provided. This would mean that the return value of the API would become
polymorphic, returning different types based on how it was called. This adds
a lot of complexity to the API and makes it difficult to reason about the
return value. It also makes it difficult to programmatically detect whether
the version of the API being used supports ERM capabilities or not. That is,
`fs.mkdtemp('...', { dispoable: true })` would act differently in older versions
of Node.js than in newer versions with no way to detect this at runtime other
than to inspect the return value.

Some APIs that already return object that can be made disposable do not have
this kind of issue. For example, the `setTimeout` API in Node.js returns an
object that implements the `Symbol.dispose` method. This change was made without
much fanfare because the return value of the API was already an object.

So, some APIs can be made disposable easily without any issues while others
require more thought and consideration. The following guidelines can help
when introducing ERM capabilities into existing APIs:

1. Avoid polymorphic return values: If an API already returns a value that
   can be made disposable and it makes sense to make it disposable, do so. Do
   not, however, make the return value polymorphic determined by an option
   passed into the API.
2. Introduce new API variants that are ERM capable: If an existing API
   cannot be made disposable without changing the return type or making it
   polymorphic, consider introducing a new API variant that is ERM capable.
   For example, `fs.mkdtempDisposable` could be introduced to return a
   disposable object while the existing `fs.mkdtemp` API continues to return
   a string. Yes, it means more APIs to maintain but it avoids the complexity
   and confusion of polymorphic return values.
3. If adding a new API variant is not ideal, remember that changing the
   return type of an existing API is a breaking change.

## Guidelines for using disposable objects

Because disposable objects can be disposed of at any time, it is important
to be careful when using them. Here are some guidelines for using disposable:

1. Never use `using` or `await using` with disposable objects that you
   do not own. For instance, the following code is problematic if you
   are not the owner of `someObject`:

```js
function foo(someObject) {
  using resource = someObject;
}
```

The reason this is problematic is that the `using` statement will
call the `Symbol.dispose` method on `someObject` when the block exits,
but you do not control the lifecycle of `someObject`. If `someObject`
is disposed of, it may lead to unexpected behavior in the rest of the
code that called the `foo` function.

2. Always explicitly dispose of objects in successful code paths, including
   early returns. For example:

```js
function foo() {
  using res = new MyDisposableResource();
  if (someCondition) {
    // Early return, ensure the resource is disposed of
    res.close();
    return;
  }
  // do other stuff
  res.close();
}
```

3. Remember that disposers are invoked in a stack, in the reverse order
   in which there were created. For example,

```js
class MyDisposable {
  constructor(name) {
    this.name = name;
  }
  [Symbol.dispose]() {
    console.log(`Disposing ${this.name}`);
  }
}

{
  using a = new MyDisposable('A');
  using b = new MyDisposable('B');
  using c = new MyDisposable('C');
  // When this block exits, the disposal methods will be called in the
  // reverse order: C, B, A.
}
```

Because of this, it is important to consider the possible relationships
between disposable objects. For example, if one disposable object holds a
reference to another disposable object the cleanup order may be important.
If disposers are properly idempotent, however, this should not cause any
issue, but it still requires careful consideration.
