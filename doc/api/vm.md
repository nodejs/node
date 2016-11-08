# Executing JavaScript

> Stability: 2 - Stable

<!--name=vm-->

The `vm` module provides APIs for compiling and running code within V8 Virtual
Machine contexts. It can be accessed using:

```js
const vm = require('vm');
```

JavaScript code can be compiled and run immediately or compiled, saved, and run
later.

## Class: vm.Script
<!-- YAML
added: v0.3.1
-->

Instances of the `vm.Script` class contain precompiled scripts that can be
executed in specific sandboxes (or "contexts").

### new vm.Script(code, options)
<!-- YAML
added: v0.3.1
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
compiled `vm.Script` can be run later multiple times. It is important to note
that the `code` is not bound to any global object; rather, it is bound before
each run, just for that run.

### script.runInContext(contextifiedSandbox[, options])
<!-- YAML
added: v0.3.1
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

const context = new vm.createContext(sandbox);
for (var i = 0; i < 10; ++i) {
  script.runInContext(context);
}

console.log(util.inspect(sandbox));

// { animal: 'cat', count: 12, name: 'kitty' }
```

### script.runInNewContext([sandbox][, options])
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

for (var i = 0; i < 1000; ++i) {
  script.runInThisContext();
}

console.log(globalVar);

// 1000
```

## vm.createContext([sandbox])
<!-- YAML
added: v0.3.1
-->

* `sandbox` {Object}

If given a `sandbox` object, the `vm.createContext()` method will [prepare
that sandbox][contextified] so that it can be used in calls to
[`vm.runInContext()`][] or [`script.runInContext()`][]. Inside such scripts,
the `sandbox` object will be the global object, retaining all of its existing
properties but also having the built-in objects and functions any standard
[global object][] has. Outside of scripts run by the vm module, `sandbox` will
remain unchanged.

If `sandbox` is omitted (or passed explicitly as `undefined`), a new, empty
[contextified][] sandbox object will be returned.

The `vm.createContext()` method is primarily useful for creating a single
sandbox that can be used to run multiple scripts. For instance, if emulating a
web browser, the method can be used to create a single sandbox representing a
window's global object, then run all `<script>` tags together within the context
of that sandbox.

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

for (var i = 0; i < 10; ++i) {
  vm.runInContext('globalVar *= 2;', sandbox);
}
console.log(util.inspect(sandbox));

// { globalVar: 1024 }
```

## vm.runInDebugContext(code)
<!-- YAML
added: v0.11.14
-->

* `code` {string} The JavaScript code to compile and run.

The `vm.runInDebugContext()` method compiles and executes `code` inside the V8
debug context. The primary use case is to gain access to the V8 `Debug` object:

```js
const vm = require('vm');
const Debug = vm.runInDebugContext('Debug');
console.log(Debug.findScript(process.emit).name);  // 'events.js'
console.log(Debug.findScript(process.exit).name);  // 'internal/process.js'
```

*Note*: The debug context and object are intrinsically tied to V8's debugger
implementation and may change (or even be removed) without prior warning.

The `Debug` object can also be made available using the V8-specific
`--expose_debug_as=` [command line option][].

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

```js
const vm = require('vm');
var localVar = 'initial value';

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

let code =
`(function(require) {

   const http = require('http');

   http.createServer( (request, response) => {
     response.writeHead(200, {'Content-Type': 'text/plain'});
     response.end('Hello World\\n');
   }).listen(8124);

   console.log('Server running at http://127.0.0.1:8124/');
 })`;

 vm.runInThisContext(code)(require);
 ```

*Note*: The `require()` in the above case shares the state with context it is
passed from. This may introduce risks when untrusted code is executed, e.g.
altering objects from the calling thread's context in unwanted ways.

## What does it mean to "contextify" an object?

All JavaScript executed within Node.js runs within the scope of a "context".
According to the [V8 Embedder's Guide][]:

> In V8, a context is an execution environment that allows separate, unrelated,
> JavaScript applications to run in a single instance of V8. You must explicitly
> specify the context in which you want any JavaScript code to be run.

When the method `vm.createContext()` is called, the `sandbox` object that is
passed in (or a newly created object if `sandbox` is `undefined`) is associated
internally with a new instance of a V8 Context. This V8 Context provides the
`code` run using the `vm` modules methods with an isolated global environment
within which it can operate. The process of creating the V8 Context and
associating it with the `sandbox` object is what this document refers to as
"contextifying" the `sandbox`.

[indirect `eval()` call]: https://es5.github.io/#x10.4.2
[global object]: https://es5.github.io/#x15.1
[`Error`]: errors.html#errors_class_error
[`script.runInContext()`]: #vm_script_runincontext_contextifiedsandbox_options
[`script.runInThisContext()`]: #vm_script_runinthiscontext_options
[`vm.createContext()`]: #vm_vm_createcontext_sandbox
[`vm.runInContext()`]: #vm_vm_runincontext_code_contextifiedsandbox_options
[`vm.runInThisContext()`]: #vm_vm_runinthiscontext_code_options
[`eval()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval
[V8 Embedder's Guide]: https://developers.google.com/v8/embed#contexts
[contextified]: #vm_what_does_it_mean_to_contextify_an_object
[command line option]: cli.html
