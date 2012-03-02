# Executing JavaScript

    Stability: 3 - Stable

<!--name=vm-->

You can access this module with:

    var vm = require('vm');

JavaScript code can be compiled and run immediately or compiled, saved, and run later.


## vm.runInThisContext(code, [filename])

`vm.runInThisContext()` compiles `code`, runs it and returns the result. Running
code does not have access to local scope. `filename` is optional, it's used only
in stack traces.

Example of using `vm.runInThisContext` and `eval` to run the same code:

    var localVar = 123,
        usingscript, evaled,
        vm = require('vm');

    usingscript = vm.runInThisContext('localVar = 1;',
      'myfile.vm');
    console.log('localVar: ' + localVar + ', usingscript: ' +
      usingscript);
    evaled = eval('localVar = 1;');
    console.log('localVar: ' + localVar + ', evaled: ' +
      evaled);

    // localVar: 123, usingscript: 1
    // localVar: 1, evaled: 1

`vm.runInThisContext` does not have access to the local scope, so `localVar` is unchanged.
`eval` does have access to the local scope, so `localVar` is changed.

In case of syntax error in `code`, `vm.runInThisContext` emits the syntax error to stderr
and throws an exception.


## vm.runInNewContext(code, [sandbox], [filename])

`vm.runInNewContext` compiles `code`, then runs it in `sandbox` and returns the
result. Running code does not have access to local scope. The object `sandbox`
will be used as the global object for `code`.
`sandbox` and `filename` are optional, `filename` is only used in stack traces.

Example: compile and execute code that increments a global variable and sets a new one.
These globals are contained in the sandbox.

    var util = require('util'),
        vm = require('vm'),
        sandbox = {
          animal: 'cat',
          count: 2
        };

    vm.runInNewContext('count += 1; name = "kitty"', sandbox, 'myfile.vm');
    console.log(util.inspect(sandbox));

    // { animal: 'cat', count: 3, name: 'kitty' }

Note that running untrusted code is a tricky business requiring great care.  To prevent accidental
global variable leakage, `vm.runInNewContext` is quite useful, but safely running untrusted code
requires a separate process.

In case of syntax error in `code`, `vm.runInNewContext` emits the syntax error to stderr
and throws an exception.

## vm.runInContext(code, context, [filename])

`vm.runInContext` compiles `code`, then runs it in `context` and returns the
result. A (V8) context comprises a global object, together with a set of
built-in objects and functions. Running code does not have access to local scope
and the global object held within `context` will be used as the global object
for `code`.
`filename` is optional, it's used only in stack traces.

Example: compile and execute code in a existing context.

    var util = require('util'),
        vm = require('vm'),
        initSandbox = {
          animal: 'cat',
          count: 2
        },
        context = vm.createContext(initSandbox);

    vm.runInContext('count += 1; name = "CATT"', context, 'myfile.vm');
    console.log(util.inspect(context));

    // { animal: 'cat', count: 3, name: 'CATT' }

Note that `createContext` will perform a shallow clone of the supplied sandbox object in order to
initialise the global object of the freshly constructed context.

Note that running untrusted code is a tricky business requiring great care.  To prevent accidental
global variable leakage, `vm.runInContext` is quite useful, but safely running untrusted code
requires a separate process.

In case of syntax error in `code`, `vm.runInContext` emits the syntax error to stderr
and throws an exception.

## vm.createContext([initSandbox])

`vm.createContext` creates a new context which is suitable for use as the 2nd argument of a subsequent
call to `vm.runInContext`. A (V8) context comprises a global object together with a set of
build-in objects and functions. The optional argument `initSandbox` will be shallow-copied
to seed the initial contents of the global object used by the context.

## vm.createScript(code, [filename])

`createScript` compiles `code` but does not run it. Instead, it returns a
`vm.Script` object representing this compiled code. This script can be run
later many times using methods below. The returned script is not bound to any
global object. It is bound before each run, just for that run. `filename` is
optional, it's only used in stack traces.

In case of syntax error in `code`, `createScript` prints the syntax error to stderr
and throws an exception.


## Class: Script

A class for running scripts.  Returned by vm.createScript.

### script.runInThisContext()

Similar to `vm.runInThisContext` but a method of a precompiled `Script` object.
`script.runInThisContext` runs the code of `script` and returns the result.
Running code does not have access to local scope, but does have access to the `global` object
(v8: in actual context).

Example of using `script.runInThisContext` to compile code once and run it multiple times:

    var vm = require('vm');

    globalVar = 0;

    var script = vm.createScript('globalVar += 1', 'myfile.vm');

    for (var i = 0; i < 1000 ; i += 1) {
      script.runInThisContext();
    }

    console.log(globalVar);

    // 1000


### script.runInNewContext([sandbox])

Similar to `vm.runInNewContext` a method of a precompiled `Script` object.
`script.runInNewContext` runs the code of `script` with `sandbox` as the global object and returns the result.
Running code does not have access to local scope. `sandbox` is optional.

Example: compile code that increments a global variable and sets one, then execute this code multiple times.
These globals are contained in the sandbox.

    var util = require('util'),
        vm = require('vm'),
        sandbox = {
          animal: 'cat',
          count: 2
        };

    var script = vm.createScript('count += 1; name = "kitty"', 'myfile.vm');

    for (var i = 0; i < 10 ; i += 1) {
      script.runInNewContext(sandbox);
    }

    console.log(util.inspect(sandbox));

    // { animal: 'cat', count: 12, name: 'kitty' }

Note that running untrusted code is a tricky business requiring great care.  To prevent accidental
global variable leakage, `script.runInNewContext` is quite useful, but safely running untrusted code
requires a separate process.
