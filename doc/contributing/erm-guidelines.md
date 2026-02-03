# Explicit Resource Management (`using`) Guidelines

[Explicit Resource Management](https://github.com/tc39/proposal-explicit-resource-management)
is a capability that was introduced to the JavaScript language in 2025. It provides a way
of marking objects as disposable resources such that the JavaScript engine will automatically
invoke disposal methods when the object is no longer in scope. For example:

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

This document outlines some specific guidelines for using explicit resource
management in the Node.js project -- specifically, guidelines around how to
make objects disposable and how to introduce the new capabilities into existing
APIs.

The caveat to this guidance is that explicit resource management is a brand new
language feature, and there is not an existing body of experience to draw from
when writing these guidelines. The points outlined here are based on the
current understanding of how the mechanism works and how it is expected to
be used. As such, these guidelines may change over time as more experience
is gained with explicit resource management in Node.js and the ecosystem.
It is always a good idea to check the latest version of this document, and
more importantly, to suggest changes to it based on evolving understanding,
needs, and experience.

## Some background

Objects can be made disposable by implementing either, or both, the
`Symbol.dispose` and `Symbol.asyncDispose` methods:

```js
class MySyncResource {
  [Symbol.dispose]() {
    // Synchronous disposal logic
  }
}

class MyAsyncDisposableResource {
  async [Symbol.asyncDispose]() {
    // Asynchronous disposal logic
  }
}
```

An object that implements `Symbol.dispose` can be used with the `using`
statement, which will automatically call the `Symbol.dispose` method when the
object goes out of scope. If an object implements `Symbol.asyncDispose`, it can
be used with the `await using` statement in an asynchronous context. It is
worth noting here that `await using` means the disposal is asynchronous,
not the initialization.

```mjs
{
  using resource = new MyResource();
  await using asyncResource = new MyResource();
}
```

Importantly, it is necessary to understand that the design of `using` makes it
possible for user code to call the `Symbol.dispose` or `Symbol.asyncDispose`
methods directly, outside of the `using` or `await using` statements. These
can also be called multiple times and by any code that is holding a reference
to the object. That is to say, explicit resource management does not imply
ownership of the object. It is not a form of RAII (Resource Acquisition Is
Initialization) as seen in some other languages and there is no notion of
exclusive ownership of the object. A disposable object can become disposed
at any time.

The `Symbol.dispose` and `Symbol.asyncDispose` methods are called in both
successful and exceptional exits from the scopes in which the `using` keyword
is used. This means that if an exception is thrown within the scope, the
disposal methods will still be called (similar to how `finally { }` blocks work).
However, when the disposal methods are called they are not aware of the context.
These methods will not receive any information about any exception that may have
been thrown. This means that it is often safest to assume that the disposal
methods will be called in a context where the object may not be in a valid
state or that an exception may be pending.

## Guidelines for disposable objects

So with this in mind, it is necessary to outline some guidelines for disposers:

1. Disposers should be idempotent. Multiple calls to the disposal methods
   should not cause any issues or have any additional side effects.
2. Disposers should assume that they are being called in an exception context.
   Always assume there is likely a pending exception and that if the object
   has not been explicitly closed when the disposal method is called, the
   object should be disposed as if an exception had occurred. For instance,
   if the object API exposes both a `close()` method and an `abort()` method,
   the disposal method should call `abort()` if the object is not already
   closed. If there is no difference in disposing in success or exception
   contexts, then separate disposal methods are unnecessary.
3. It is recommended to avoid throwing errors within disposers.
   If a disposer throws an exception while there is another pending
   exception, then both exceptions will be wrapped in a `SuppressedError`
   that masks both. This makes it difficult to understand the context
   in which the exceptions were thrown.
4. Disposable objects should expose named disposal methods in addition
   to the `Symbol.dispose` and `Symbol.asyncDispose` methods. This allows
   user code to explicitly dispose of the object without using the `using`
   or `await using` statements. For example, a disposable object might
   expose a `close()` method that can be called to dispose of the object.
   The `Symbol.dispose` and `Symbol.asyncDispose` methods should then invoke
   these named disposal methods in an idempotent manner.
5. Because it is safest to assume that the disposal method will be called
   in an exception context, it is generally recommended to prefer use of
   `Symbol.dispose` over `Symbol.asyncDispose` when possible. Asynchronous
   disposal can lead to delaying the handling of exceptions and can make it
   difficult to reason about the state of the object while the disposal is
   in progress. Disposal in an exception context is preferably synchronous
   and immediate. That said, for some types of objects async disposal is not
   avoidable.
6. Asynchronous disposers, by definition, are able to yield to other tasks
   while waiting for their disposal task(s) to complete. This means that, as a
   minimum, a `Symbol.asyncDispose` method must be an `async` function, and
   must `await` at least one asynchronous disposal task. If either of these
   criteria is not met, then the disposer is actually a synchronous disposer in
   disguise, and will block the execution thread until it returns; such a
   disposer should instead be defined using `Symbol.dispose`.
7. Because the disposal process is strictly ordered, there is an intrinsic
   expectation that all tasks performed by a single disposer are fully complete
   at the point that the disposer returns. This means, for example, that
   "callback-style" APIs must not be invoked within a disposer, unless they are
   promisified and awaited. Any Promise created within a disposer must be
   awaited, to ensure its resolution prior to the disposer returning.
8. Avoid, as much as possible, using both `Symbol.dispose` and `Symbol.asyncDispose`
   in the same object. This can make it difficult to reason about which method
   will be called in a given context and could lead to unexpected behavior or
   subtle bugs. This is not a firm rule, however; there may be specific cases
   where it makes sense to define both, such as where a resource already exposes
   both synchronous and asynchronous methods for closing down the resource.

### Example disposable objects

A disposable object can be quite simple:

```js
class MyResource {
  #disposed = false;
  dispose() {
    if (this.#disposed) return;
    this.#disposed = true;
    console.log('Resource disposed');
  }

  [Symbol.dispose]() {
    this.dispose();
  }
}

{ using myDisposable = new MyResource(); }
```

Or even fully anonymous objects:

```js
function getDisposable() {
  let disposed = false;
  return {
    dispose() {
      if (disposed) return;
      disposed = true;
      console.log('Resource disposed');
    },
    [Symbol.dispose]() {
      this.dispose();
    },
  };
}

{ using myDisposable = getDisposable(); }
```

Some disposable objects, however, may need to differentiate between disposal
in a success context and disposal in an exception context as in the following
example:

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
  // Explicitly close the resource if no error was thrown to
  // avoid the resource being aborted when the `Symbol.dispose`
  // method is called.
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
  // be a SuppressedError if an error was thrown during disposal and
  // there was a pending exception already.
  if (error instanceof SuppressedError) {
    console.error('An error occurred during disposal masking pending error:',
                  error.error, error.suppressed);
  } else {
    console.error('An error occurred:', error);
  }
}
```

### Symbol.dispose and Symbol.asyncDispose return values

The `Symbol.dispose` method should return `undefined` and the
`Symbol.asyncDispose` method should return a `Promise` that resolves to
`undefined`.

<!-- eslint-skip -->

```js
[Symbol.dispose]() {
  return void this.dispose();
  // or
  this.dispose();
  // or
  return;
  // or
  // no return
}

async [Symbol.asyncDispose]() {
  await this.dispose();
  // or
  return;
  // or
  // no return
}
```

### Debuggability of disposer methods

To improve debugging experience, The `Symbol.dispose` and `Symbol.asyncDispose`
functions should not be direct aliases of named disposer functions. They should
instead defer to the named disposer. This ensures the stack traces can be more
informative about whether a disposer was called via `using` or was called
directly.

For example:

<!-- eslint-skip -->

```js
// Do something like this:
function dispose() { ... }
return {
  dispose,
  [Symbol.dispose]() { this.dispose(); }
};

// Rather than this:
function dispose() { ... }
return {
  dispose,
  [Symbol.dispose]: dispose
};
```

### A Note on documenting disposable objects

When documenting disposable objects, it is important to clearly indicate that
the object is disposable and how it should be disposed of. This includes
documenting the `Symbol.dispose` and `Symbol.asyncDispose` methods, as well as
any named disposal methods that the object exposes.

If the disposable object is anonymous (that is, it is a regular JavaScript
object that implements the `Symbol.dispose` method), it is still important to
document that it is disposable and how it should be disposed of.

Within the documentation, it is possible to document anonymous objects as if
they were classes, using the `Class: ` prefix and otherwise presenting the
object as if you were documenting a regular JavaScript class, even if it is
never actually instantiated as a class. Examples of this pattern can be seen,
for instance, in the documentation of the [Web Crypto API](../api/webcrypto.md).

So, for example, if you have an API that returns an anonymous disposable object,
you might document it like:

```markdown
### Class: `MyDisposableObject`

#### `myDisposableObject.dispose()`

...

#### `myDisposableObject[Symbol.dispose]()`

...

### `foo.getMyDisposableObject()`

* Returns: {MyDisposableObject}
```

## Guidelines for introducing explicit resource management into existing APIs

Introducing the ability to use `using` into existing APIs can be tricky.

The best way to understand the issues is to look at a real world example. PR
[58516](https://github.com/nodejs/node/pull/58516) is a good case. This PR
sought to introduce `Symbol.dispose` and `Symbol.asyncDispose` capabilities
into the `fs.mkdtemp` API such that a temporary directory could be created and
be automatically disposed of when the scope in which it was created exited.
However, the existing implementation of the `fs.mkdtemp` API returns a string
value that cannot be made disposable. There are also sync, callback, and
promise-based variations of the existing API that further complicate the
situation.

In the initial proposal, the `fs.mkdtemp` API was changed to return an object
that implements the `Symbol.dispose` method but only if a specific option is
provided. This would mean that the return value of the API would become
polymorphic, returning different types based on how it was called. This adds
a lot of complexity to the API and makes it difficult to reason about the
return value. It also makes it difficult to programmatically detect whether
the version of the API being used supports `using` or not.
`fs.mkdtemp('...', { disposable: true })` would act differently in older versions
of Node.js than in newer versions with no way to detect this at runtime other
than to inspect the return value.

Some APIs that already return objects that can be made disposable do not have
this kind of issue. For example, the `setImmediate()` API in Node.js returns an
object that implements the `Symbol.dispose` method. This change was made without
much fanfare because the return value of the API was already an object.

So, some APIs can be made disposable easily without any issues while others
require more thought and consideration. The following guidelines can help
when introducing these capabilities into existing APIs:

1. Avoid polymorphic return values: If an API already returns a value that
   can be made disposable, and it makes sense to make it disposable, do so. Do
   not, however, make the return value polymorphic determined by an option
   passed into the API.
2. Introduce new API variants that are `using` capable: If an existing API
   cannot be made disposable without changing the return type or making it
   polymorphic, consider introducing a new API variant. For example,
   `fs.mkdtempDisposable` could be introduced to return a disposable object
   while the existing `fs.mkdtemp` API continues to return a string. Yes, it
   means more APIs to maintain but it avoids the complexity and confusion of
   polymorphic return values. If adding a new API variant is not ideal, remember
   that changing the return type of an existing API is quite likely a breaking
   change.
3. When an existing API signature does not lend itself easily to supporting making
   the return value disposable and a new API needs to be introduced, it is worth
   considering whether the existing API should be deprecated in favor of the new.
   Deprecation is never a decision to be taken lightly, however, as it can have major
   ecosystem impact.

## Guidelines for using disposable objects

Because disposable objects can be disposed of at any time, it is important
to be careful when using them. Here are some guidelines for using disposables:

1. Never use `using` or `await using` with disposable objects that you
   do not own. For instance, the following code is problematic if you
   are not the owner of `someObject`:

```js
function foo(someObject) {
  using resource = someObject;
}
```

The reason this is problematic is that the `using` statement will
unconditionally call the `Symbol.dispose` method on `someObject` when the block
exits, but you do not control the lifecycle of `someObject`. If `someObject`
is disposed of, it may lead to unexpected behavior in the rest of the
code that called the `foo` function.

2. When there is a clear difference between disposing of an object in a success
   context vs. an exception context, always explicitly dispose of objects the
   successful code paths, including early returns. For example:

```js
class MyDisposableResource {
  close() {
    console.log('Resource closed');
  }

  abort() {
    console.log('Resource aborted');
  }

  [Symbol.dispose]() {
    // Assume the error case here...
    this.abort();
  }
}

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

This is because of the fact that, when the disposer is called, it has no way
of knowing if there is a pending exception or not and it is generally safest
to assume that it is being called in an exceptional state.

Many types of disposable objects make no differentiation between success and
exception cases, in which case relying entirely on `using` is just fine (and
preferred). The disposable returned by `setImmediate()` is a good example here.
All that does is call `clearImmediate()` and it does not matter if the block
errored or not.

3. Remember that disposers are invoked in a stack, in the reverse order
   in which they were created. For example,

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
