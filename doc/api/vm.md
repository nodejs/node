# VM (Executing JavaScript)

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

<!--name=vm-->

The `vm` module provides APIs for compiling and running code within V8 Virtual
Machine contexts.

JavaScript code can be compiled and run immediately or
compiled, saved, and run later.

A common use case is to run the code in a sandboxed environment.
The sandboxed code uses a different V8 Context, meaning that
it has a different global object than the rest of the code.

One can provide the context by ["contextifying"][contextified] a sandbox
object. The sandboxed code treats any property on the sandbox like a
global variable. Any changes on global variables caused by the sandboxed
code are reflected in the sandbox object.

```js
const vm = require('vm');

const x = 1;

const sandbox = { x: 2 };
vm.createContext(sandbox); // Contextify the sandbox.

const code = 'x += 40; var y = 17;';
// x and y are global variables in the sandboxed environment.
// Initially, x has the value 2 because that is the value of sandbox.x.
vm.runInContext(code, sandbox);

console.log(sandbox.x); // 42
console.log(sandbox.y); // 17

console.log(x); // 1; y is not defined.
```

*Note*: The vm module is not a security mechanism.
**Do not use it to run untrusted code**.

## Class: vm.Module
<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

*This feature is only available with the `--experimental-vm-modules` command
flag enabled.*

The `vm.Module` class provides a low-level interface for using ECMAScript
modules in VM contexts. It is the counterpart of the `vm.Script` class that
closely mirrors [Source Text Module Record][]s as defined in the ECMAScript
specification.

Unlike `vm.Script` however, every `vm.Module` object is bound to a context from
its creation. Operations on `vm.Module` objects are intrinsically asynchronous,
in contrast with the synchronous nature of `vm.Script` objects. With the help
of async functions, however, manipulating `vm.Module` objects is fairly
straightforward.

Using a `vm.Module` object requires four distinct steps: creation/parsing,
linking, instantiation, and evaluation. These four steps are illustrated in the
following example.

*Note*: This implementation lies at a lower level than the [ECMAScript Module
loader][]. There is also currently no way to interact with the Loader, though
support is planned.

```js
const vm = require('vm');

const contextifiedSandbox = vm.createContext({ secret: 42 });

(async () => {
  // Step 1
  //
  // Create a Module by constructing a new `vm.Module` object. This parses the
  // provided source text, throwing a `SyntaxError` if anything goes wrong. By
  // default, a Module is created in the top context. But here, we specify
  // `contextifiedSandbox` as the context this Module belongs to.
  //
  // Here, we attempt to obtain the default export from the module "foo", and
  // put it into local binding "secret".

  const bar = new vm.Module(`
    import s from 'foo';
    s;
  `, { context: contextifiedSandbox });


  // Step 2
  //
  // "Link" the imported dependencies of this Module to it.
  //
  // The provided linking callback (the "linker") accepts two arguments: the
  // parent module (`bar` in this case) and the string that is the specifier of
  // the imported module. The callback is expected to return a Module that
  // corresponds to the provided specifier, with certain requirements documented
  // in `module.link()`.
  //
  // If linking has not started for the returned Module, the same linker
  // callback will be called on the returned Module.
  //
  // Even top-level Modules without dependencies must be explicitly linked. The
  // callback provided would never be called, however.
  //
  // The link() method returns a Promise that will be resolved when all the
  // Promises returned by the linker resolve.
  //
  // Note: This is a contrived example in that the linker function creates a new
  // "foo" module every time it is called. In a full-fledged module system, a
  // cache would probably be used to avoid duplicated modules.

  async function linker(referencingModule, specifier) {
    if (specifier === 'foo') {
      return new vm.Module(`
        // The "secret" variable refers to the global variable we added to
        // "contextifiedSandbox" when creating the context.
        export default secret;
      `, { context: referencingModule.context });

      // Using `contextifiedSandbox` instead of `referencingModule.context`
      // here would work as well.
    }
    throw new Error(`Unable to resolve dependency: ${specifier}`);
  }
  await bar.link(linker);


  // Step 3
  //
  // Instantiate the top-level Module.
  //
  // Only the top-level Module needs to be explicitly instantiated; its
  // dependencies will be recursively instantiated by instantiate().

  bar.instantiate();


  // Step 4
  //
  // Evaluate the Module. The evaluate() method returns a Promise with a single
  // property "result" that contains the result of the very last statement
  // executed in the Module. In the case of `bar`, it is `s;`, which refers to
  // the default export of the `foo` module, the `secret` we set in the
  // beginning to 42.

  const { result } = await bar.evaluate();

  console.log(result);
  // Prints 42.
})();
```

### Constructor: new vm.Module(code[, options])

* `code` {string} JavaScript Module code to parse
* `options`
  * `url` {string} URL used in module resolution and stack traces. **Default**:
    `'vm:module(i)'` where `i` is a context-specific ascending index.
  * `context` {Object} The [contextified][] object as returned by the
    `vm.createContext()` method, to compile and evaluate this Module in.
  * `lineOffset` {integer} Specifies the line number offset that is displayed
    in stack traces produced by this Module.
  * `columnOffset` {integer} Spcifies the column number offset that is displayed
    in stack traces produced by this Module.

Creates a new ES `Module` object.

### module.dependencySpecifiers

* {string[]}

The specifiers of all dependencies of this module. The returned array is frozen
to disallow any changes to it.

Corresponds to the [[RequestedModules]] field of [Source Text Module Record][]s
in the ECMAScript specification.

### module.error

* {any}

If the `module.status` is `'errored'`, this property contains the exception thrown
by the module during evaluation. If the status is anything else, accessing this
property will result in a thrown exception.

*Note*: `undefined` cannot be used for cases where there is not a thrown
exception due to possible ambiguity with `throw undefined;`.

Corresponds to the [[EvaluationError]] field of [Source Text Module Record][]s
in the ECMAScript specification.

### module.linkingStatus

* {string}

The current linking status of `module`. It will be one of the following values:

- `'unlinked'`: `module.link()` has not yet been called.
- `'linking'`: `module.link()` has been called, but not all Promises returned by
  the linker function have been resolved yet.
- `'linked'`: `module.link()` has been called, and all its dependencies have
  been successfully linked.
- `'errored'`: `module.link()` has been called, but at least one of its
  dependencies failed to link, either because the callback returned a Promise
  that is rejected, or because the Module the callback returned is invalid.

### module.namespace

* {Object}

The namespace object of the module. This is only available after instantiation
(`module.instantiate()`) has completed.

Corresponds to the [GetModuleNamespace][] abstract operation in the ECMAScript
specification.

### module.status

* {string}

The current status of the module. Will be one of:

- `'uninstantiated'`: The module is not instantiated. It may because of any of
  the following reasons:

  - The module was just created.
  - `module.instantiate()` has been called on this module, but it failed for
    some reason.

  This status does not convey any information regarding if `module.link()` has
  been called. See `module.linkingStatus` for that.

- `'instantiating'`: The module is currently being instantiated through a
  `module.instantiate()` call on itself or a parent module.

- `'instantiated'`: The module has been instantiated successfully, but
  `module.evaluate()` has not yet been called.

- `'evaluating'`: The module is being evaluated through a `module.evaluate()` on
  itself or a parent module.

- `'evaluated'`: The module has been successfully evaluated.

- `'errored'`: The module has been evaluated, but an exception was thrown.

Other than `'errored'`, this status string corresponds to the specification's
[Source Text Module Record][]'s [[Status]] field. `'errored'` corresponds to
`'evaluated'` in the specification, but with [[EvaluationError]] set to a value
that is not `undefined`.

### module.url

* {string}

The URL of the current module, as set in the constructor.

### module.evaluate([options])

* `options` {Object}
  * `timeout` {number} Specifies the number of milliseconds to evaluate
    before terminating execution. If execution is interrupted, an [`Error`][]
    will be thrown.
  * `breakOnSigint` {boolean} If `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the event that have
    been attached via `process.on("SIGINT")` will be disabled during script
    execution, but will continue to work after that. If execution is
    interrupted, an [`Error`][] will be thrown.
* Returns: {Promise}

Evaluate the module.

This must be called after the module has been instantiated; otherwise it will
throw an error. It could be called also when the module has already been
evaluated, in which case it will do one of the following two things:

- return `undefined` if the initial evaluation ended in success (`module.status`
  is `'evaluated'`)
- rethrow the same exception the initial evaluation threw if the initial
  evaluation ended in an error (`module.status` is `'errored'`)

This method cannot be called while the module is being evaluated
(`module.status` is `'evaluating'`) to prevent infinite recursion.

Corresponds to the [Evaluate() concrete method][] field of [Source Text Module
Record][]s in the ECMAScript specification.

### module.instantiate()

Instantiate the module. This must be called after linking has completed
(`linkingStatus` is `'linked'`); otherwise it will throw an error. It may also
throw an exception if one of the dependencies does not provide an export the
parent module requires.

However, if this function succeeded, further calls to this function after the
initial instantiation will be no-ops, to be consistent with the ECMAScript
specification.

Unlike other methods operating on `Module`, this function completes
synchronously and returns nothing.

Corresponds to the [Instantiate() concrete method][] field of [Source Text
Module Record][]s in the ECMAScript specification.

### module.link(linker)

* `linker` {Function}
* Returns: {Promise}

Link module dependencies. This method must be called before instantiation, and
can only be called once per module.

Two parameters will be passed to the `linker` function:

- `referencingModule` The `Module` object `link()` is called on.
- `specifier` The specifier of the requested module:

  <!-- eslint-skip -->
  ```js
  import foo from 'foo';
  //              ^^^^^ the module specifier
  ```

The function is expected to return a `Module` object or a `Promise` that
eventually resolves to a `Module` object. The returned `Module` must satisfy the
following two invariants:

- It must belong to the same context as the parent `Module`.
- Its `linkingStatus` must not be `'errored'`.

If the returned `Module`'s `linkingStatus` is `'unlinked'`, this method will be
recursively called on the returned `Module` with the same provided `linker`
function.

`link()` returns a `Promise` that will either get resolved when all linking
instances resolve to a valid `Module`, or rejected if the linker function either
throws an exception or returns an invalid `Module`.

The linker function roughly corresponds to the implementation-defined
[HostResolveImportedModule][] abstract operation in the ECMAScript
specification, with a few key differences:

- The linker function is allowed to be asynchronous while
  [HostResolveImportedModule][] is synchronous.
- The linker function is executed during linking, a Node.js-specific stage
  before instantiation, while [HostResolveImportedModule][] is called during
  instantiation.

The actual [HostResolveImportedModule][] implementation used during module
instantiation is one that returns the modules linked during linking. Since at
that point all modules would have been fully linked already, the
[HostResolveImportedModule][] implementation is fully synchronous per
specification.

## Class: vm.Script
<!-- YAML
added: v0.3.1
-->

Instances of the `vm.Script` class contain precompiled scripts that can be
executed in specific sandboxes (or "contexts").

### new vm.Script(code, options)
<!-- YAML
added: v0.3.1
changes:
  - version: v5.7.0
    pr-url: https://github.com/nodejs/node/pull/4777
    description: The `cachedData` and `produceCachedData` options are
                 supported now.
-->

* `code` {string} The JavaScript code to compile.
* `options`
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] error occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace.
  * `timeout` {number} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown.
  * `cachedData` {Buffer} Provides an optional `Buffer` with V8's code cache
    data for the supplied source. When supplied, the `cachedDataRejected` value
    will be set to either `true` or `false` depending on acceptance of the data
    by V8.
  * `produceCachedData` {boolean} When `true` and no `cachedData` is present, V8
    will attempt to produce code cache data for `code`. Upon success, a
    `Buffer` with V8's code cache data will be produced and stored in the
    `cachedData` property of the returned `vm.Script` instance.
    The `cachedDataProduced` value will be set to either `true` or `false`
    depending on whether code cache data is produced successfully.

Creating a new `vm.Script` object compiles `code` but does not run it. The
compiled `vm.Script` can be run later multiple times. The `code` is not bound to
any global object; rather, it is bound before each run, just for that run.

### script.runInContext(contextifiedSandbox[, options])
<!-- YAML
added: v0.3.1
changes:
  - version: v6.3.0
    pr-url: https://github.com/nodejs/node/pull/6635
    description: The `breakOnSigint` option is supported now.
-->

* `contextifiedSandbox` {Object} A [contextified][] object as returned by the
  `vm.createContext()` method.
* `options` {Object}
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] error occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace.
  * `timeout` {number} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown.
  * `breakOnSigint`: if `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the
    event that have been attached via `process.on("SIGINT")` will be disabled
    during script execution, but will continue to work after that.
    If execution is terminated, an [`Error`][] will be thrown.


Runs the compiled code contained by the `vm.Script` object within the given
`contextifiedSandbox` and returns the result. Running code does not have access
to local scope.

The following example compiles code that increments a global variable, sets
the value of another global variable, then execute the code multiple times.
The globals are contained in the `sandbox` object.

```js
const util = require('util');
const vm = require('vm');

const sandbox = {
  animal: 'cat',
  count: 2
};

const script = new vm.Script('count += 1; name = "kitty";');

const context = vm.createContext(sandbox);
for (let i = 0; i < 10; ++i) {
  script.runInContext(context);
}

console.log(util.inspect(sandbox));

// { animal: 'cat', count: 12, name: 'kitty' }
```

*Note*: Using the `timeout` or `breakOnSigint` options will result in new
event loops and corresponding threads being started, which have a non-zero
performance overhead.

### script.runInNewContext([sandbox[, options]])
<!-- YAML
added: v0.3.1
-->

* `sandbox` {Object} An object that will be [contextified][]. If `undefined`, a
  new object will be created.
* `options` {Object}
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] error occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace.
  * `timeout` {number} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown.
  * `contextName` {string} Human-readable name of the newly created context.
    **Default:** `'VM Context i'`, where `i` is an ascending numerical index of
    the created context.
  * `contextOrigin` {string} [Origin][origin] corresponding to the newly
    created context for display purposes. The origin should be formatted like a
    URL, but with only the scheme, host, and port (if necessary), like the
    value of the [`url.origin`][] property of a [`URL`][] object. Most notably,
    this string should omit the trailing slash, as that denotes a path.
    **Default:** `''`.

First contextifies the given `sandbox`, runs the compiled code contained by
the `vm.Script` object within the created sandbox, and returns the result.
Running code does not have access to local scope.

The following example compiles code that sets a global variable, then executes
the code multiple times in different contexts. The globals are set on and
contained within each individual `sandbox`.

```js
const util = require('util');
const vm = require('vm');

const script = new vm.Script('globalVar = "set"');

const sandboxes = [{}, {}, {}];
sandboxes.forEach((sandbox) => {
  script.runInNewContext(sandbox);
});

console.log(util.inspect(sandboxes));

// [{ globalVar: 'set' }, { globalVar: 'set' }, { globalVar: 'set' }]
```

### script.runInThisContext([options])
<!-- YAML
added: v0.3.1
-->

* `options` {Object}
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] error occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace.
  * `timeout` {number} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown.

Runs the compiled code contained by the `vm.Script` within the context of the
current `global` object. Running code does not have access to local scope, but
*does* have access to the current `global` object.

The following example compiles code that increments a `global` variable then
executes that code multiple times:

```js
const vm = require('vm');

global.globalVar = 0;

const script = new vm.Script('globalVar += 1', { filename: 'myfile.vm' });

for (let i = 0; i < 1000; ++i) {
  script.runInThisContext();
}

console.log(globalVar);

// 1000
```

## vm.createContext([sandbox[, options]])
<!-- YAML
added: v0.3.1
-->

* `sandbox` {Object}
* `options` {Object}
  * `name` {string} Human-readable name of the newly created context.
    **Default:** `'VM Context i'`, where `i` is an ascending numerical index of
    the created context.
  * `origin` {string} [Origin][origin] corresponding to the newly created
    context for display purposes. The origin should be formatted like a URL,
    but with only the scheme, host, and port (if necessary), like the value of
    the [`url.origin`][] property of a [`URL`][] object. Most notably, this
    string should omit the trailing slash, as that denotes a path.
    **Default:** `''`.

If given a `sandbox` object, the `vm.createContext()` method will [prepare
that sandbox][contextified] so that it can be used in calls to
[`vm.runInContext()`][] or [`script.runInContext()`][]. Inside such scripts,
the `sandbox` object will be the global object, retaining all of its existing
properties but also having the built-in objects and functions any standard
[global object][] has. Outside of scripts run by the vm module, global variables
will remain unchanged.

```js
const util = require('util');
const vm = require('vm');

global.globalVar = 3;

const sandbox = { globalVar: 1 };
vm.createContext(sandbox);

vm.runInContext('globalVar *= 2;', sandbox);

console.log(util.inspect(sandbox)); // { globalVar: 2 }

console.log(util.inspect(globalVar)); // 3
```

If `sandbox` is omitted (or passed explicitly as `undefined`), a new, empty
[contextified][] sandbox object will be returned.

The `vm.createContext()` method is primarily useful for creating a single
sandbox that can be used to run multiple scripts. For instance, if emulating a
web browser, the method can be used to create a single sandbox representing a
window's global object, then run all `<script>` tags together within the context
of that sandbox.

The provided `name` and `origin` of the context are made visible through the
Inspector API.

## vm.isContext(sandbox)
<!-- YAML
added: v0.11.7
-->

* `sandbox` {Object}

Returns `true` if the given `sandbox` object has been [contextified][] using
[`vm.createContext()`][].

## vm.runInContext(code, contextifiedSandbox[, options])

* `code` {string} The JavaScript code to compile and run.
* `contextifiedSandbox` {Object} The [contextified][] object that will be used
  as the `global` when the `code` is compiled and run.
* `options`
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] error occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace.
  * `timeout` {number} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown.

The `vm.runInContext()` method compiles `code`, runs it within the context of
the `contextifiedSandbox`, then returns the result. Running code does not have
access to the local scope. The `contextifiedSandbox` object *must* have been
previously [contextified][] using the [`vm.createContext()`][] method.

The following example compiles and executes different scripts using a single
[contextified][] object:

```js
const util = require('util');
const vm = require('vm');

const sandbox = { globalVar: 1 };
vm.createContext(sandbox);

for (let i = 0; i < 10; ++i) {
  vm.runInContext('globalVar *= 2;', sandbox);
}
console.log(util.inspect(sandbox));

// { globalVar: 1024 }
```

## vm.runInNewContext(code[, sandbox][, options])
<!-- YAML
added: v0.3.1
-->

* `code` {string} The JavaScript code to compile and run.
* `sandbox` {Object} An object that will be [contextified][]. If `undefined`, a
  new object will be created.
* `options`
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] error occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace.
  * `timeout` {number} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown.
  * `contextName` {string} Human-readable name of the newly created context.
    **Default:** `'VM Context i'`, where `i` is an ascending numerical index of
    the created context.
  * `contextOrigin` {string} [Origin][origin] corresponding to the newly
    created context for display purposes. The origin should be formatted like a
    URL, but with only the scheme, host, and port (if necessary), like the
    value of the [`url.origin`][] property of a [`URL`][] object. Most notably,
    this string should omit the trailing slash, as that denotes a path.
    **Default:** `''`.

The `vm.runInNewContext()` first contextifies the given `sandbox` object (or
creates a new `sandbox` if passed as `undefined`), compiles the `code`, runs it
within the context of the created context, then returns the result. Running code
does not have access to the local scope.

The following example compiles and executes code that increments a global
variable and sets a new one. These globals are contained in the `sandbox`.

```js
const util = require('util');
const vm = require('vm');

const sandbox = {
  animal: 'cat',
  count: 2
};

vm.runInNewContext('count += 1; name = "kitty"', sandbox);
console.log(util.inspect(sandbox));

// { animal: 'cat', count: 3, name: 'kitty' }
```

## vm.runInThisContext(code[, options])
<!-- YAML
added: v0.3.1
-->

* `code` {string} The JavaScript code to compile and run.
* `options`
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] error occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace.
  * `timeout` {number} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown.

`vm.runInThisContext()` compiles `code`, runs it within the context of the
current `global` and returns the result. Running code does not have access to
local scope, but does have access to the current `global` object.

The following example illustrates using both `vm.runInThisContext()` and
the JavaScript [`eval()`][] function to run the same code:

<!-- eslint-disable prefer-const -->
```js
const vm = require('vm');
let localVar = 'initial value';

const vmResult = vm.runInThisContext('localVar = "vm";');
console.log('vmResult:', vmResult);
console.log('localVar:', localVar);

const evalResult = eval('localVar = "eval";');
console.log('evalResult:', evalResult);
console.log('localVar:', localVar);

// vmResult: 'vm', localVar: 'initial value'
// evalResult: 'eval', localVar: 'eval'
```

Because `vm.runInThisContext()` does not have access to the local scope,
`localVar` is unchanged. In contrast, [`eval()`][] *does* have access to the
local scope, so the value `localVar` is changed. In this way
`vm.runInThisContext()` is much like an [indirect `eval()` call][], e.g.
`(0,eval)('code')`.

## Example: Running an HTTP Server within a VM

When using either [`script.runInThisContext()`][] or [`vm.runInThisContext()`][], the
code is executed within the current V8 global context. The code passed
to this VM context will have its own isolated scope.

In order to run a simple web server using the `http` module the code passed to
the context must either call `require('http')` on its own, or have a reference
to the `http` module passed to it. For instance:

```js
'use strict';
const vm = require('vm');

const code = `
((require) => {
  const http = require('http');

  http.createServer((request, response) => {
    response.writeHead(200, { 'Content-Type': 'text/plain' });
    response.end('Hello World\\n');
  }).listen(8124);

  console.log('Server running at http://127.0.0.1:8124/');
})`;

vm.runInThisContext(code)(require);
 ```

*Note*: The `require()` in the above case shares the state with the context it
is passed from. This may introduce risks when untrusted code is executed, e.g.
altering objects in the context in unwanted ways.

## What does it mean to "contextify" an object?

All JavaScript executed within Node.js runs within the scope of a "context".
According to the [V8 Embedder's Guide][]:

> In V8, a context is an execution environment that allows separate, unrelated,
> JavaScript applications to run in a single instance of V8. You must explicitly
> specify the context in which you want any JavaScript code to be run.

When the method `vm.createContext()` is called, the `sandbox` object that is
passed in (or a newly created object if `sandbox` is `undefined`) is associated
internally with a new instance of a V8 Context. This V8 Context provides the
`code` run using the `vm` module's methods with an isolated global environment
within which it can operate. The process of creating the V8 Context and
associating it with the `sandbox` object is what this document refers to as
"contextifying" the `sandbox`.

[`Error`]: errors.html#errors_class_error
[`URL`]: url.html#url_class_url
[`eval()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval
[`script.runInContext()`]: #vm_script_runincontext_contextifiedsandbox_options
[`script.runInThisContext()`]: #vm_script_runinthiscontext_options
[`url.origin`]: https://nodejs.org/api/url.html#url_url_origin
[`vm.createContext()`]: #vm_vm_createcontext_sandbox_options
[`vm.runInContext()`]: #vm_vm_runincontext_code_contextifiedsandbox_options
[`vm.runInThisContext()`]: #vm_vm_runinthiscontext_code_options
[GetModuleNamespace]: https://tc39.github.io/ecma262/#sec-getmodulenamespace
[ECMAScript Module Loader]: esm.html#esm_ecmascript_modules
[Evaluate() concrete method]: https://tc39.github.io/ecma262/#sec-moduleevaluation
[HostResolveImportedModule]: https://tc39.github.io/ecma262/#sec-hostresolveimportedmodule
[Instantiate() concrete method]: https://tc39.github.io/ecma262/#sec-moduledeclarationinstantiation
[V8 Embedder's Guide]: https://github.com/v8/v8/wiki/Embedder's%20Guide#contexts
[contextified]: #vm_what_does_it_mean_to_contextify_an_object
[global object]: https://es5.github.io/#x15.1
[indirect `eval()` call]: https://es5.github.io/#x10.4.2
[origin]: https://developer.mozilla.org/en-US/docs/Glossary/Origin
[Source Text Module Record]: https://tc39.github.io/ecma262/#sec-source-text-module-records
