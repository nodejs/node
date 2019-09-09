# VM (Executing JavaScript)

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

<!--name=vm-->

The `vm` module provides APIs for compiling and running code within V8 Virtual
Machine contexts. **The `vm` module is not a security mechanism. Do
not use it to run untrusted code**. The term "sandbox" is used throughout these
docs simply to refer to a separate context, and does not confer any security
guarantees.

JavaScript code can be compiled and run immediately or
compiled, saved, and run later.

A common use case is to run the code in a sandboxed environment.
The sandboxed code uses a different V8 Context, meaning that
it has a different global object than the rest of the code.

One can provide the context by ["contextifying"][contextified] a sandbox
object. The sandboxed code treats any property in the sandbox like a
global variable. Any changes to global variables caused by the sandboxed
code are reflected in the sandbox object.

```js
const vm = require('vm');

const x = 1;

const sandbox = { x: 2 };
vm.createContext(sandbox); // Contextify the sandbox.

const code = 'x += 40; var y = 17;';
// `x` and `y` are global variables in the sandboxed environment.
// Initially, x has the value 2 because that is the value of sandbox.x.
vm.runInContext(code, sandbox);

console.log(sandbox.x); // 42
console.log(sandbox.y); // 17

console.log(x); // 1; y is not defined.
```

## Class: vm.Script
<!-- YAML
added: v0.3.1
-->

Instances of the `vm.Script` class contain precompiled scripts that can be
executed in specific sandboxes (or "contexts").

### Constructor: new vm.Script(code\[, options\])
<!-- YAML
added: v0.3.1
changes:
  - version: v5.7.0
    pr-url: https://github.com/nodejs/node/pull/4777
    description: The `cachedData` and `produceCachedData` options are
                 supported now.
  - version: v10.6.0
    pr-url: https://github.com/nodejs/node/pull/20300
    description: The `produceCachedData` is deprecated in favour of
                 `script.createCachedData()`
-->

* `code` {string} The JavaScript code to compile.
* `options` {Object|string}
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script. **Default:** `'evalmachine.<anonymous>'`.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `cachedData` {Buffer|TypedArray|DataView} Provides an optional `Buffer` or
    `TypedArray`, or `DataView` with V8's code cache data for the supplied
     source. When supplied, the `cachedDataRejected` value will be set to
     either `true` or `false` depending on acceptance of the data by V8.
  * `produceCachedData` {boolean} When `true` and no `cachedData` is present, V8
    will attempt to produce code cache data for `code`. Upon success, a
    `Buffer` with V8's code cache data will be produced and stored in the
    `cachedData` property of the returned `vm.Script` instance.
    The `cachedDataProduced` value will be set to either `true` or `false`
    depending on whether code cache data is produced successfully.
    This option is **deprecated** in favor of `script.createCachedData()`.
    **Default:** `false`.
  * `importModuleDynamically` {Function} Called during evaluation of this module
    when `import()` is called. If this option is not specified, calls to
    `import()` will reject with [`ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING`][].
    This option is part of the experimental API for the `--experimental-modules`
    flag, and should not be considered stable.
    * `specifier` {string} specifier passed to `import()`
    * `module` {vm.Module}
    * Returns: {Module Namespace Object|vm.Module} Returning a `vm.Module` is
      recommended in order to take advantage of error tracking, and to avoid
      issues with namespaces that contain `then` function exports.

If `options` is a string, then it specifies the filename.

Creating a new `vm.Script` object compiles `code` but does not run it. The
compiled `vm.Script` can be run later multiple times. The `code` is not bound to
any global object; rather, it is bound before each run, just for that run.

### script.createCachedData()
<!-- YAML
added: v10.6.0
-->

* Returns: {Buffer}

Creates a code cache that can be used with the Script constructor's
`cachedData` option. Returns a Buffer. This method may be called at any
time and any number of times.

```js
const script = new vm.Script(`
function add(a, b) {
  return a + b;
}

const x = add(1, 2);
`);

const cacheWithoutX = script.createCachedData();

script.runInThisContext();

const cacheWithX = script.createCachedData();
```

### script.runInContext(contextifiedSandbox\[, options\])
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
  * `displayErrors` {boolean} When `true`, if an [`Error`][] occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace. **Default:** `true`.
  * `timeout` {integer} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown. This value must be a strictly positive integer.
  * `breakOnSigint` {boolean} If `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the
    event that have been attached via `process.on('SIGINT')` will be disabled
    during script execution, but will continue to work after that. If execution
    is terminated, an [`Error`][] will be thrown. **Default:** `false`.
* Returns: {any} the result of the very last statement executed in the script.

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

Using the `timeout` or `breakOnSigint` options will result in new event loops
and corresponding threads being started, which have a non-zero performance
overhead.

### script.runInNewContext(\[sandbox\[, options\]\])
<!-- YAML
added: v0.3.1
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19016
    description: The `contextCodeGeneration` option is supported now.
  - version: v6.3.0
    pr-url: https://github.com/nodejs/node/pull/6635
    description: The `breakOnSigint` option is supported now.
-->

* `sandbox` {Object} An object that will be [contextified][]. If `undefined`, a
  new object will be created.
* `options` {Object}
  * `displayErrors` {boolean} When `true`, if an [`Error`][] occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace. **Default:** `true`.
  * `timeout` {integer} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown. This value must be a strictly positive integer.
  * `breakOnSigint` {boolean} If `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the
    event that have been attached via `process.on('SIGINT')` will be disabled
    during script execution, but will continue to work after that. If execution
    is terminated, an [`Error`][] will be thrown. **Default:** `false`.
  * `contextName` {string} Human-readable name of the newly created context.
    **Default:** `'VM Context i'`, where `i` is an ascending numerical index of
    the created context.
  * `contextOrigin` {string} [Origin][origin] corresponding to the newly
    created context for display purposes. The origin should be formatted like a
    URL, but with only the scheme, host, and port (if necessary), like the
    value of the [`url.origin`][] property of a [`URL`][] object. Most notably,
    this string should omit the trailing slash, as that denotes a path.
    **Default:** `''`.
  * `contextCodeGeneration` {Object}
    * `strings` {boolean} If set to false any calls to `eval` or function
      constructors (`Function`, `GeneratorFunction`, etc) will throw an
      `EvalError`. **Default:** `true`.
    * `wasm` {boolean} If set to false any attempt to compile a WebAssembly
      module will throw a `WebAssembly.CompileError`. **Default:** `true`.
* Returns: {any} the result of the very last statement executed in the script.

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

### script.runInThisContext(\[options\])
<!-- YAML
added: v0.3.1
changes:
  - version: v6.3.0
    pr-url: https://github.com/nodejs/node/pull/6635
    description: The `breakOnSigint` option is supported now.
-->

* `options` {Object}
  * `displayErrors` {boolean} When `true`, if an [`Error`][] occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace. **Default:** `true`.
  * `timeout` {integer} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown. This value must be a strictly positive integer.
  * `breakOnSigint` {boolean} If `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the
    event that have been attached via `process.on('SIGINT')` will be disabled
    during script execution, but will continue to work after that. If execution
    is terminated, an [`Error`][] will be thrown. **Default:** `false`.
* Returns: {any} the result of the very last statement executed in the script.

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

## Class: vm.Module
<!-- YAML
added: v13.0.0
-->

> Stability: 1 - Experimental

*This feature is only available with the `--experimental-vm-modules` command
flag enabled.*

The `vm.Module` class provides a low-level interface for using
ECMAScript modules in VM contexts. It is the counterpart of the `vm.Script`
class that closely mirrors [Module Record][]s as defined in the ECMAScript
specification.

Unlike `vm.Script` however, every `vm.Module` object is bound to a context from
its creation. Operations on `vm.Module` objects are intrinsically asynchronous,
in contrast with the synchronous nature of `vm.Script` objects. With the help
of async functions, however, manipulating `vm.Module` objects is fairly
straightforward.

Using a `vm.Module` object requires three distinct steps: creation/parsing,
linking, and evaluation. These three steps are illustrated in the following
example.

This implementation lies at a lower level than the [ECMAScript Module
loader][]. There is also currently no way to interact with the Loader, though
support is planned.

```js
const vm = require('vm');

const contextifiedSandbox = vm.createContext({ secret: 42 });

(async () => {
  // Step 1
  //
  // Create a Module by constructing a new `vm.SourceTextModule` object. This
  // parses the provided source text, throwing a `SyntaxError` if anything goes
  // wrong. By default, a Module is created in the top context. But here, we
  // specify `contextifiedSandbox` as the context this Module belongs to.
  //
  // Here, we attempt to obtain the default export from the module "foo", and
  // put it into local binding "secret".

  const bar = new vm.SourceTextModule(`
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

  async function linker(specifier, referencingModule) {
    if (specifier === 'foo') {
      return new vm.SourceTextModule(`
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

### module.dependencySpecifiers

* {string[]}

The specifiers of all dependencies of this module. The returned array is frozen
to disallow any changes to it.

Corresponds to the `[[RequestedModules]]` field of [Cyclic Module Record][]s in
the ECMAScript specification.

### module.error

* {any}

If the `module.status` is `'errored'`, this property contains the exception
thrown by the module during evaluation. If the status is anything else,
accessing this property will result in a thrown exception.

The value `undefined` cannot be used for cases where there is not a thrown
exception due to possible ambiguity with `throw undefined;`.

Corresponds to the `[[EvaluationError]]` field of [Cyclic Module Record][]s
in the ECMAScript specification.

### module.evaluate(\[options\])

* `options` {Object}
  * `timeout` {integer} Specifies the number of milliseconds to evaluate
    before terminating execution. If execution is interrupted, an [`Error`][]
    will be thrown. This value must be a strictly positive integer.
  * `breakOnSigint` {boolean} If `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the event that have
    been attached via `process.on('SIGINT')` will be disabled during script
    execution, but will continue to work after that. If execution is
    interrupted, an [`Error`][] will be thrown. **Default:** `false`.
* Returns: {Promise}

Evaluate the module.

This must be called after the module has been linked; otherwise it will
throw an error. It could be called also when the module has already been
evaluated, in which case it will do one of the following two things:

* return `undefined` if the initial evaluation ended in success (`module.status`
  is `'evaluated'`)
* rethrow the same exception the initial evaluation threw if the initial
  evaluation ended in an error (`module.status` is `'errored'`)

This method cannot be called while the module is being evaluated
(`module.status` is `'evaluating'`) to prevent infinite recursion.

Corresponds to the [Evaluate() concrete method][] field of [Cyclic Module
Record][]s in the ECMAScript specification.

### module.link(linker)

* `linker` {Function}
  * `specifier` {string} The specifier of the requested module:
    <!-- eslint-skip -->
    ```js
    import foo from 'foo';
    //              ^^^^^ the module specifier
    ```

  * `referencingModule` {vm.Module} The `Module` object `link()` is called on.
  * Returns: {vm.Module|Promise}
* Returns: {Promise}

Link module dependencies. This method must be called before evaluation, and
can only be called once per module.

The function is expected to return a `Module` object or a `Promise` that
eventually resolves to a `Module` object. The returned `Module` must satisfy the
following two invariants:

* It must belong to the same context as the parent `Module`.
* Its `status` must not be `'errored'`.

If the returned `Module`'s `status` is `'unlinked'`, this method will be
recursively called on the returned `Module` with the same provided `linker`
function.

`link()` returns a `Promise` that will either get resolved when all linking
instances resolve to a valid `Module`, or rejected if the linker function either
throws an exception or returns an invalid `Module`.

The linker function roughly corresponds to the implementation-defined
[HostResolveImportedModule][] abstract operation in the ECMAScript
specification, with a few key differences:

* The linker function is allowed to be asynchronous while
  [HostResolveImportedModule][] is synchronous.

The actual [HostResolveImportedModule][] implementation used during module
linking is one that returns the modules linked during linking. Since at
that point all modules would have been fully linked already, the
[HostResolveImportedModule][] implementation is fully synchronous per
specification.

Corresponds to the [Link() concrete method][] field of [Cyclic Module
Record][]s in the ECMAScript specification.

### module.namespace

* {Object}

The namespace object of the module. This is only available after linking
(`module.link()`) has completed.

Corresponds to the [GetModuleNamespace][] abstract operation in the ECMAScript
specification.

### module.status

* {string}

The current status of the module. Will be one of:

* `'unlinked'`: `module.link()` has not yet been called.

* `'linking'`: `module.link()` has been called, but not all Promises returned
  by the linker function have been resolved yet.

* `'linked'`: The module has been linked successfully, and all of its
  dependencies are linked, but `module.evaluate()` has not yet been called.

* `'evaluating'`: The module is being evaluated through a `module.evaluate()` on
  itself or a parent module.

* `'evaluated'`: The module has been successfully evaluated.

* `'errored'`: The module has been evaluated, but an exception was thrown.

Other than `'errored'`, this status string corresponds to the specification's
[Cyclic Module Record][]'s `[[Status]]` field. `'errored'` corresponds to
`'evaluated'` in the specification, but with `[[EvaluationError]]` set to a
value that is not `undefined`.

### module.identifier

* {string}

The identifier of the current module, as set in the constructor.

## Class: vm.SourceTextModule
<!-- YAML
added: v9.6.0
-->

> Stability: 1 - Experimental

*This feature is only available with the `--experimental-vm-modules` command
flag enabled.*

* Extends: {vm.Module}

The `vm.SourceTextModule` class provides the [Source Text Module Record][] as
defined in the ECMAScript specification.

### Constructor: new vm.SourceTextModule(code\[, options\])

* `code` {string} JavaScript Module code to parse
* `options`
  * `identifier` {string} String used in stack traces.
    **Default:** `'vm:module(i)'` where `i` is a context-specific ascending
    index.
  * `context` {Object} The [contextified][] object as returned by the
    `vm.createContext()` method, to compile and evaluate this `Module` in.
  * `lineOffset` {integer} Specifies the line number offset that is displayed
    in stack traces produced by this `Module`. **Default:** `0`.
  * `columnOffset` {integer} Specifies the column number offset that is
    displayed in stack traces produced by this `Module`. **Default:** `0`.
  * `initializeImportMeta` {Function} Called during evaluation of this `Module`
    to initialize the `import.meta`.
    * `meta` {import.meta}
    * `module` {vm.SourceTextModule}
  * `importModuleDynamically` {Function} Called during evaluation of this module
    when `import()` is called. If this option is not specified, calls to
    `import()` will reject with [`ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING`][].
    * `specifier` {string} specifier passed to `import()`
    * `module` {vm.Module}
    * Returns: {Module Namespace Object|vm.Module} Returning a `vm.Module` is
      recommended in order to take advantage of error tracking, and to avoid
      issues with namespaces that contain `then` function exports.

Creates a new `SourceTextModule` instance.

Properties assigned to the `import.meta` object that are objects may
allow the module to access information outside the specified `context`. Use
`vm.runInContext()` to create objects in a specific context.

```js
const vm = require('vm');

const contextifiedSandbox = vm.createContext({ secret: 42 });

(async () => {
  const module = new vm.SourceTextModule(
    'Object.getPrototypeOf(import.meta.prop).secret = secret;',
    {
      initializeImportMeta(meta) {
        // Note: this object is created in the top context. As such,
        // Object.getPrototypeOf(import.meta.prop) points to the
        // Object.prototype in the top context rather than that in
        // the sandbox.
        meta.prop = {};
      }
    });
  // Since module has no dependencies, the linker function will never be called.
  await module.link(() => {});
  await module.evaluate();

  // Now, Object.prototype.secret will be equal to 42.
  //
  // To fix this problem, replace
  //     meta.prop = {};
  // above with
  //     meta.prop = vm.runInContext('{}', contextifiedSandbox);
})();
```

## Class: vm.SyntheticModule
<!-- YAML
added: v13.0.0
-->

> Stability: 1 - Experimental

*This feature is only available with the `--experimental-vm-modules` command
flag enabled.*

* Extends: {vm.Module}

The `vm.SyntheticModule` class provides the [Synthetic Module Record][] as
defined in the WebIDL specification. The purpose of synthetic modules is to
provide a generic interface for exposing non-JavaScript sources to ECMAScript
module graphs.

```js
const vm = require('vm');

const source = '{ "a": 1 }';
const module = new vm.SyntheticModule(['default'], function() {
  const obj = JSON.parse(source);
  this.setExport('default', obj);
});

// Use `module` in linking...
```

### Constructor: new vm.SyntheticModule(exportNames, evaluateCallback\[, options\])
<!-- YAML
added: v13.0.0
-->

* `exportNames` {string[]} Array of names that will be exported from the module.
* `evaluateCallback` {Function} Called when the module is evaluated.
* `options`
  * `identifier` {string} String used in stack traces.
   **Default:** `'vm:module(i)'` where `i` is a context-specific ascending
    index.
  * `context` {Object} The [contextified][] object as returned by the
    `vm.createContext()` method, to compile and evaluate this `Module` in.

Creates a new `SyntheticModule` instance.

Objects assigned to the exports of this instance may allow importers of
the module to access information outside the specified `context`. Use
`vm.runInContext()` to create objects in a specific context.

### syntheticModule.setExport(name, value)
<!-- YAML
added: v13.0.0
-->

* `name` {string} Name of the export to set.
* `value` {any} The value to set the export to.

This method is used after the module is linked to set the values of exports. If
it is called before the module is linked, an [`ERR_VM_MODULE_STATUS`][] error
will be thrown.

```js
const vm = require('vm');

(async () => {
  const m = new vm.SyntheticModule(['x'], () => {
    m.setExport('x', 1);
  });

  await m.link(() => {});
  await m.evaluate();

  assert.strictEqual(m.namespace.x, 1);
})();
```

## vm.compileFunction(code\[, params\[, options\]\])
<!-- YAML
added: v10.10.0
-->

* `code` {string} The body of the function to compile.
* `params` {string[]} An array of strings containing all parameters for the
  function.
* `options` {Object}
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script. **Default:** `''`.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `cachedData` {Buffer|TypedArray|DataView} Provides an optional `Buffer` or
    `TypedArray`, or `DataView` with V8's code cache data for the supplied
     source.
  * `produceCachedData` {boolean} Specifies whether to produce new cache data.
    **Default:** `false`.
  * `parsingContext` {Object} The [contextified][] sandbox in which the said
    function should be compiled in.
  * `contextExtensions` {Object[]} An array containing a collection of context
    extensions (objects wrapping the current scope) to be applied while
    compiling. **Default:** `[]`.
* Returns: {Function}

Compiles the given code into the provided context/sandbox (if no context is
supplied, the current context is used), and returns it wrapped inside a
function with the given `params`.

## vm.createContext(\[sandbox\[, options\]\])
<!-- YAML
added: v0.3.1
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19398
    description: The `sandbox` object can no longer be a function.
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19016
    description: The `codeGeneration` option is supported now.
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
  * `codeGeneration` {Object}
    * `strings` {boolean} If set to false any calls to `eval` or function
      constructors (`Function`, `GeneratorFunction`, etc) will throw an
      `EvalError`. **Default:** `true`.
    * `wasm` {boolean} If set to false any attempt to compile a WebAssembly
      module will throw a `WebAssembly.CompileError`. **Default:** `true`.
* Returns: {Object} contextified sandbox.

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
* Returns: {boolean}

Returns `true` if the given `sandbox` object has been [contextified][] using
[`vm.createContext()`][].

## vm.runInContext(code, contextifiedSandbox\[, options\])
<!-- YAML
added: v0.3.1
changes:
  - version: v6.3.0
    pr-url: https://github.com/nodejs/node/pull/6635
    description: The `breakOnSigint` option is supported now.
-->

* `code` {string} The JavaScript code to compile and run.
* `contextifiedSandbox` {Object} The [contextified][] object that will be used
  as the `global` when the `code` is compiled and run.
* `options` {Object|string}
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script. **Default:** `'evalmachine.<anonymous>'`.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace. **Default:** `true`.
  * `timeout` {integer} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown. This value must be a strictly positive integer.
  * `breakOnSigint` {boolean} If `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the
    event that have been attached via `process.on('SIGINT')` will be disabled
    during script execution, but will continue to work after that. If execution
    is terminated, an [`Error`][] will be thrown. **Default:** `false`.
  * `cachedData` {Buffer|TypedArray|DataView} Provides an optional `Buffer` or
    `TypedArray`, or `DataView` with V8's code cache data for the supplied
     source. When supplied, the `cachedDataRejected` value will be set to
     either `true` or `false` depending on acceptance of the data by V8.
  * `produceCachedData` {boolean} When `true` and no `cachedData` is present, V8
    will attempt to produce code cache data for `code`. Upon success, a
    `Buffer` with V8's code cache data will be produced and stored in the
    `cachedData` property of the returned `vm.Script` instance.
    The `cachedDataProduced` value will be set to either `true` or `false`
    depending on whether code cache data is produced successfully.
    This option is **deprecated** in favor of `script.createCachedData()`.
    **Default:** `false`.
  * `importModuleDynamically` {Function} Called during evaluation of this module
    when `import()` is called. If this option is not specified, calls to
    `import()` will reject with [`ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING`][].
    This option is part of the experimental API for the `--experimental-modules`
    flag, and should not be considered stable.
    * `specifier` {string} specifier passed to `import()`
    * `module` {vm.Module}
    * Returns: {Module Namespace Object|vm.Module} Returning a `vm.Module` is
      recommended in order to take advantage of error tracking, and to avoid
      issues with namespaces that contain `then` function exports.
* Returns: {any} the result of the very last statement executed in the script.

The `vm.runInContext()` method compiles `code`, runs it within the context of
the `contextifiedSandbox`, then returns the result. Running code does not have
access to the local scope. The `contextifiedSandbox` object *must* have been
previously [contextified][] using the [`vm.createContext()`][] method.

If `options` is a string, then it specifies the filename.

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

## vm.runInNewContext(code\[, sandbox\[, options\]\])
<!-- YAML
added: v0.3.1
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19016
    description: The `contextCodeGeneration` option is supported now.
  - version: v6.3.0
    pr-url: https://github.com/nodejs/node/pull/6635
    description: The `breakOnSigint` option is supported now.
-->

* `code` {string} The JavaScript code to compile and run.
* `sandbox` {Object} An object that will be [contextified][]. If `undefined`, a
  new object will be created.
* `options` {Object|string}
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script. **Default:** `'evalmachine.<anonymous>'`.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace. **Default:** `true`.
  * `timeout` {integer} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown. This value must be a strictly positive integer.
  * `breakOnSigint` {boolean} If `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the
    event that have been attached via `process.on('SIGINT')` will be disabled
    during script execution, but will continue to work after that. If execution
    is terminated, an [`Error`][] will be thrown. **Default:** `false`.
  * `contextName` {string} Human-readable name of the newly created context.
    **Default:** `'VM Context i'`, where `i` is an ascending numerical index of
    the created context.
  * `contextOrigin` {string} [Origin][origin] corresponding to the newly
    created context for display purposes. The origin should be formatted like a
    URL, but with only the scheme, host, and port (if necessary), like the
    value of the [`url.origin`][] property of a [`URL`][] object. Most notably,
    this string should omit the trailing slash, as that denotes a path.
    **Default:** `''`.
  * `contextCodeGeneration` {Object}
    * `strings` {boolean} If set to false any calls to `eval` or function
      constructors (`Function`, `GeneratorFunction`, etc) will throw an
      `EvalError`. **Default:** `true`.
    * `wasm` {boolean} If set to false any attempt to compile a WebAssembly
      module will throw a `WebAssembly.CompileError`. **Default:** `true`.
  * `cachedData` {Buffer|TypedArray|DataView} Provides an optional `Buffer` or
    `TypedArray`, or `DataView` with V8's code cache data for the supplied
     source. When supplied, the `cachedDataRejected` value will be set to
     either `true` or `false` depending on acceptance of the data by V8.
  * `produceCachedData` {boolean} When `true` and no `cachedData` is present, V8
    will attempt to produce code cache data for `code`. Upon success, a
    `Buffer` with V8's code cache data will be produced and stored in the
    `cachedData` property of the returned `vm.Script` instance.
    The `cachedDataProduced` value will be set to either `true` or `false`
    depending on whether code cache data is produced successfully.
    This option is **deprecated** in favor of `script.createCachedData()`.
    **Default:** `false`.
  * `importModuleDynamically` {Function} Called during evaluation of this module
    when `import()` is called. If this option is not specified, calls to
    `import()` will reject with [`ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING`][].
    This option is part of the experimental API for the `--experimental-modules`
    flag, and should not be considered stable.
    * `specifier` {string} specifier passed to `import()`
    * `module` {vm.Module}
    * Returns: {Module Namespace Object|vm.Module} Returning a `vm.Module` is
      recommended in order to take advantage of error tracking, and to avoid
      issues with namespaces that contain `then` function exports.
* Returns: {any} the result of the very last statement executed in the script.

The `vm.runInNewContext()` first contextifies the given `sandbox` object (or
creates a new `sandbox` if passed as `undefined`), compiles the `code`, runs it
within the context of the created context, then returns the result. Running code
does not have access to the local scope.

If `options` is a string, then it specifies the filename.

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

## vm.runInThisContext(code\[, options\])
<!-- YAML
added: v0.3.1
changes:
  - version: v6.3.0
    pr-url: https://github.com/nodejs/node/pull/6635
    description: The `breakOnSigint` option is supported now.
-->

* `code` {string} The JavaScript code to compile and run.
* `options` {Object|string}
  * `filename` {string} Specifies the filename used in stack traces produced
    by this script. **Default:** `'evalmachine.<anonymous>'`.
  * `lineOffset` {number} Specifies the line number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `columnOffset` {number} Specifies the column number offset that is displayed
    in stack traces produced by this script. **Default:** `0`.
  * `displayErrors` {boolean} When `true`, if an [`Error`][] occurs
    while compiling the `code`, the line of code causing the error is attached
    to the stack trace. **Default:** `true`.
  * `timeout` {integer} Specifies the number of milliseconds to execute `code`
    before terminating execution. If execution is terminated, an [`Error`][]
    will be thrown. This value must be a strictly positive integer.
  * `breakOnSigint` {boolean} If `true`, the execution will be terminated when
    `SIGINT` (Ctrl+C) is received. Existing handlers for the
    event that have been attached via `process.on('SIGINT')` will be disabled
    during script execution, but will continue to work after that. If execution
    is terminated, an [`Error`][] will be thrown. **Default:** `false`.
  * `cachedData` {Buffer|TypedArray|DataView} Provides an optional `Buffer` or
    `TypedArray`, or `DataView` with V8's code cache data for the supplied
     source. When supplied, the `cachedDataRejected` value will be set to
     either `true` or `false` depending on acceptance of the data by V8.
  * `produceCachedData` {boolean} When `true` and no `cachedData` is present, V8
    will attempt to produce code cache data for `code`. Upon success, a
    `Buffer` with V8's code cache data will be produced and stored in the
    `cachedData` property of the returned `vm.Script` instance.
    The `cachedDataProduced` value will be set to either `true` or `false`
    depending on whether code cache data is produced successfully.
    This option is **deprecated** in favor of `script.createCachedData()`.
    **Default:** `false`.
  * `importModuleDynamically` {Function} Called during evaluation of this module
    when `import()` is called. If this option is not specified, calls to
    `import()` will reject with [`ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING`][].
    This option is part of the experimental API for the `--experimental-modules`
    flag, and should not be considered stable.
    * `specifier` {string} specifier passed to `import()`
    * `module` {vm.Module}
    * Returns: {Module Namespace Object|vm.Module} Returning a `vm.Module` is
      recommended in order to take advantage of error tracking, and to avoid
      issues with namespaces that contain `then` function exports.
* Returns: {any} the result of the very last statement executed in the script.

`vm.runInThisContext()` compiles `code`, runs it within the context of the
current `global` and returns the result. Running code does not have access to
local scope, but does have access to the current `global` object.

If `options` is a string, then it specifies the filename.

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

When using either [`script.runInThisContext()`][] or
[`vm.runInThisContext()`][], the code is executed within the current V8 global
context. The code passed to this VM context will have its own isolated scope.

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

The `require()` in the above case shares the state with the context it is
passed from. This may introduce risks when untrusted code is executed, e.g.
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

## Timeout limitations when using process.nextTick(), Promises, and queueMicrotask()

Because of the internal mechanics of how the `process.nextTick()` queue and
the microtask queue that underlies Promises are implemented within V8 and
Node.js, it is possible for code running within a context to "escape" the
`timeout` set using `vm.runInContext()`, `vm.runInNewContext()`, and
`vm.runInThisContext()`.

For example, the following code executed by `vm.runInNewContext()` with a
timeout of 5 milliseconds schedules an infinite loop to run after a promise
resolves. The scheduled loop is never interrupted by the timeout:

```js
const vm = require('vm');

function loop() {
  while (1) console.log(Date.now());
}

vm.runInNewContext(
  'Promise.resolve().then(loop);',
  { loop, console },
  { timeout: 5 }
);
```

This issue also occurs when the `loop()` call is scheduled using
the `process.nextTick()` and `queueMicrotask()` functions.

This issue occurs because all contexts share the same microtask and nextTick
queues.

[`ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING`]: errors.html#ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING
[`ERR_VM_MODULE_STATUS`]: errors.html#ERR_VM_MODULE_STATUS
[`Error`]: errors.html#errors_class_error
[`URL`]: url.html#url_class_url
[`eval()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval
[`script.runInContext()`]: #vm_script_runincontext_contextifiedsandbox_options
[`script.runInThisContext()`]: #vm_script_runinthiscontext_options
[`url.origin`]: url.html#url_url_origin
[`vm.createContext()`]: #vm_vm_createcontext_sandbox_options
[`vm.runInContext()`]: #vm_vm_runincontext_code_contextifiedsandbox_options
[`vm.runInThisContext()`]: #vm_vm_runinthiscontext_code_options
[Cyclic Module Record]: https://tc39.es/ecma262/#sec-cyclic-module-records
[ECMAScript Module Loader]: esm.html#esm_ecmascript_modules
[Evaluate() concrete method]: https://tc39.es/ecma262/#sec-moduleevaluation
[GetModuleNamespace]: https://tc39.es/ecma262/#sec-getmodulenamespace
[HostResolveImportedModule]: https://tc39.es/ecma262/#sec-hostresolveimportedmodule
[Link() concrete method]: https://tc39.es/ecma262/#sec-moduledeclarationlinking
[Module Record]: https://www.ecma-international.org/ecma-262/#sec-abstract-module-records
[Source Text Module Record]: https://tc39.es/ecma262/#sec-source-text-module-records
[Synthetic Module Record]: https://heycam.github.io/webidl/#synthetic-module-records
[V8 Embedder's Guide]: https://v8.dev/docs/embed#contexts
[contextified]: #vm_what_does_it_mean_to_contextify_an_object
[global object]: https://es5.github.io/#x15.1
[indirect `eval()` call]: https://es5.github.io/#x10.4.2
[origin]: https://developer.mozilla.org/en-US/docs/Glossary/Origin
