# Executing JavaScript

    Stability: 2 - Stable

<!--name=vm-->

You can access this module with:

```js
const vm = require('vm');
```

JavaScript code can be compiled and run immediately or compiled, saved, and run
later.

## Class: Script
<!-- YAML
added: v0.3.1
-->

A class for holding precompiled scripts, and running them in specific sandboxes.

### new vm.Script(code, options)
<!-- YAML
added: v0.3.1
-->

Creating a new `Script` compiles `code` but does not run it. Instead, the
created `vm.Script` object represents this compiled code. This script can be run
later many times using methods below. The returned script is not bound to any
global object. It is bound before each run, just for that run.

The options when creating a script are:

- `filename`: allows you to control the filename that shows up in any stack
  traces produced from this script.
- `lineOffset`: allows you to add an offset to the line number that is
  displayed in stack traces
- `columnOffset`: allows you to add an offset to the column number that is
  displayed in stack traces
- `displayErrors`: whether or not to print any errors to stderr, with the
  line of code that caused them highlighted, before throwing an exception.
  Applies only to syntax errors compiling the code; errors while running the
  code are controlled by the options to the script's methods.
- `timeout`: a number of milliseconds to execute `code` before terminating
  execution. If execution is terminated, an [`Error`][] will be thrown.

### script.runInContext(contextifiedSandbox[, options])
<!-- YAML
added: v0.3.1
-->

Similar to [`vm.runInContext()`][] but a method of a precompiled `Script`
object. `script.runInContext()` runs `script`'s compiled code in
`contextifiedSandbox` and returns the result. Running code does not have access
to local scope.

`script.runInContext()` takes the same options as
[`script.runInThisContext()`][].

Example: compile code that increments a global variable and sets one, then
execute the code multiple times. These globals are contained in the sandbox.

```js
const util = require('util');
const vm = require('vm');

var sandbox = {
  animal: 'cat',
  count: 2
};

var context = new vm.createContext(sandbox);
var script = new vm.Script('count += 1; name = "kitty"');

for (var i = 0; i < 10; ++i) {
  script.runInContext(context);
}

console.log(util.inspect(sandbox));

// { animal: 'cat', count: 12, name: 'kitty' }
```

Note that running untrusted code is a tricky business requiring great care.
`script.runInContext()` is quite useful, but safely running untrusted code
requires a separate process.

### script.runInNewContext([sandbox][, options])
<!-- YAML
added: v0.3.1
-->

Similar to [`vm.runInNewContext()`][] but a method of a precompiled `Script`
object. `script.runInNewContext()` contextifies `sandbox` if passed or creates a
new contextified sandbox if it's omitted, and then runs `script`'s compiled code
with the sandbox as the global object and returns the result. Running code does
not have access to local scope.

`script.runInNewContext()` takes the same options as
[`script.runInThisContext()`][].

Example: compile code that sets a global variable, then execute the code
multiple times in different contexts. These globals are set on and contained in
the sandboxes.

```js
const util = require('util');
const vm = require('vm');

const sandboxes = [{}, {}, {}];

const script = new vm.Script('globalVar = "set"');

sandboxes.forEach((sandbox) => {
  script.runInNewContext(sandbox);
});

console.log(util.inspect(sandboxes));

// [{ globalVar: 'set' }, { globalVar: 'set' }, { globalVar: 'set' }]
```

Note that running untrusted code is a tricky business requiring great care.
`script.runInNewContext()` is quite useful, but safely running untrusted code
requires a separate process.

### script.runInThisContext([options])
<!-- YAML
added: v0.3.1
-->

Similar to [`vm.runInThisContext()`]() but a method of a precompiled `Script`
object. `script.runInThisContext()` runs `script`'s compiled code and returns
the result. Running code does not have access to local scope, but does have
access to the current `global` object.

Example of using `script.runInThisContext()` to compile code once and run it
multiple times:

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

The options for running a script are:

- `filename`: allows you to control the filename that shows up in any stack
  traces produced.
- `lineOffset`: allows you to add an offset to the line number that is
  displayed in stack traces
- `columnOffset`: allows you to add an offset to the column number that is
  displayed in stack traces
- `displayErrors`: whether or not to print any errors to stderr, with the
  line of code that caused them highlighted, before throwing an exception.
  Applies only to runtime errors executing the code; it is impossible to create
  a `Script` instance with syntax errors, as the constructor will throw.
- `timeout`: a number of milliseconds to execute the script before terminating
  execution. If execution is terminated, an [`Error`][] will be thrown.

## vm.createContext([sandbox])
<!-- YAML
added: v0.3.1
-->

If given a `sandbox` object, will "contextify" that sandbox so that it can be
used in calls to [`vm.runInContext()`][] or [`script.runInContext()`][]. Inside
scripts run as such, `sandbox` will be the global object, retaining all its
existing properties but also having the built-in objects and functions any
standard [global object][] has. Outside of scripts run by the vm module,
`sandbox` will be unchanged.

If not given a sandbox object, returns a new, empty contextified sandbox object
you can use.

This function is useful for creating a sandbox that can be used to run multiple
scripts, e.g. if you were emulating a web browser it could be used to create a
single sandbox representing a window's global object, then run all `<script>`
tags together inside that sandbox.

## vm.isContext(sandbox)
<!-- YAML
added: v0.11.7
-->

Returns whether or not a sandbox object has been contextified by calling
[`vm.createContext()`][] on it.

## vm.runInContext(code, contextifiedSandbox[, options])

`vm.runInContext()` compiles `code`, then runs it in `contextifiedSandbox` and
returns the result. Running code does not have access to local scope. The
`contextifiedSandbox` object must have been previously contextified via
[`vm.createContext()`][]; it will be used as the global object for `code`.

`vm.runInContext()` takes the same options as [`vm.runInThisContext()`][].

Example: compile and execute different scripts in a single existing context.

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

Note that running untrusted code is a tricky business requiring great care.
`vm.runInContext()` is quite useful, but safely running untrusted code requires
a separate process.

## vm.runInDebugContext(code)
<!-- YAML
added: v0.11.14
-->

`vm.runInDebugContext()` compiles and executes `code` inside the V8 debug
context. The primary use case is to get access to the V8 debug object:

```js
const vm = require('vm');
const Debug = vm.runInDebugContext('Debug');
console.log(Debug.findScript(process.emit).name);  // 'events.js'
console.log(Debug.findScript(process.exit).name);  // 'internal/process.js'
```

Note that the debug context and object are intrinsically tied to V8's debugger
implementation and may change (or even get removed) without prior warning.

The debug object can also be exposed with the `--expose_debug_as=` switch.

## vm.runInNewContext(code[, sandbox][, options])
<!-- YAML
added: v0.3.1
-->

`vm.runInNewContext()` compiles `code`, contextifies `sandbox` if passed or
creates a new contextified sandbox if it's omitted, and then runs the code with
the sandbox as the global object and returns the result.

`vm.runInNewContext()` takes the same options as [`vm.runInThisContext()`][].

Example: compile and execute code that increments a global variable and sets a
new one. These globals are contained in the sandbox.

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

Note that running untrusted code is a tricky business requiring great care.
`vm.runInNewContext()` is quite useful, but safely running untrusted code requires
a separate process.

## vm.runInThisContext(code[, options])
<!-- YAML
added: v0.3.1
-->

`vm.runInThisContext()` compiles `code`, runs it and returns the result. Running
code does not have access to local scope, but does have access to the current
`global` object.

Example of using `vm.runInThisContext()` and [`eval()`][] to run the same code:

```js
const vm = require('vm');
var localVar = 'initial value';

const vmResult = vm.runInThisContext('localVar = "vm";');
console.log('vmResult: ', vmResult);
console.log('localVar: ', localVar);

const evalResult = eval('localVar = "eval";');
console.log('evalResult: ', evalResult);
console.log('localVar: ', localVar);

// vmResult: 'vm', localVar: 'initial value'
// evalResult: 'eval', localVar: 'eval'
```

`vm.runInThisContext()` does not have access to the local scope, so `localVar`
is unchanged. [`eval()`][] does have access to the local scope, so `localVar` is
changed.

In this way `vm.runInThisContext()` is much like an [indirect `eval()` call][],
e.g. `(0,eval)('code')`. However, it also has the following additional options:

- `filename`: allows you to control the filename that shows up in any stack
  traces produced.
- `lineOffset`: allows you to add an offset to the line number that is
  displayed in stack traces
- `columnOffset`: allows you to add an offset to the column number that is
  displayed in stack traces
- `displayErrors`: whether or not to print any errors to stderr, with the
  line of code that caused them highlighted, before throwing an exception.
  Will capture both syntax errors from compiling `code` and runtime errors
  thrown by executing the compiled code. Defaults to `true`.
- `timeout`: a number of milliseconds to execute `code` before terminating
  execution. If execution is terminated, an [`Error`][] will be thrown.

## Example: Run a Server within a VM

The context of `.runInThisContext()` refers to the V8 context. The code passed
to this VM context will have it's own isolated scope. To run a simple web server
using the `http` module, for instance, the code passed to the context must either
call `require('http')` on its own, or have a reference to the `http` module passed
to it. For instance:

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

_Note: `require()` in the above case shares the state with context it is passed
from. This might introduce risks when unknown code is executed, e.g. altering
objects from the calling thread's context in unwanted ways. It is advisable to
run `vm` code in a separate process._

[indirect `eval()` call]: https://es5.github.io/#x10.4.2
[global object]: https://es5.github.io/#x15.1
[`Error`]: errors.html#errors_class_error
[`script.runInContext()`]: #vm_script_runincontext_contextifiedsandbox_options
[`script.runInThisContext()`]: #vm_script_runinthiscontext_options
[`vm.createContext()`]: #vm_vm_createcontext_sandbox
[`vm.runInContext()`]: #vm_vm_runincontext_code_contextifiedsandbox_options
[`vm.runInNewContext()`]: #vm_vm_runinnewcontext_code_sandbox_options
[`vm.runInThisContext()`]: #vm_vm_runinthiscontext_code_options
[`eval()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval
