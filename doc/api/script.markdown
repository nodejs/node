## Script

`Script` class compiles and runs JavaScript code. You can access this class with:

    var Script = process.binding('evals').Script;

New JavaScript code can be compiled and run immediately or compiled, saved, and run later.


### Script.runInThisContext(code, [filename])

Similar to `process.compile`.  `Script.runInThisContext` compiles `code` as if it were loaded from `filename`,
runs it and returns the result. Running code does not have access to local scope. `filename` is optional.

Example of using `Script.runInThisContext` and `eval` to run the same code:

    var localVar = 123,
        usingscript, evaled,
        Script = process.binding('evals').Script;

    usingscript = Script.runInThisContext('localVar = 1;',
      'myfile.js');
    console.log('localVar: ' + localVar + ', usingscript: ' +
      usingscript);
    evaled = eval('localVar = 1;');
    console.log('localVar: ' + localVar + ', evaled: ' +
      evaled);

    // localVar: 123, usingscript: 1
    // localVar: 1, evaled: 1

`Script.runInThisContext` does not have access to the local scope, so `localVar` is unchanged.
`eval` does have access to the local scope, so `localVar` is changed.

In case of syntax error in `code`, `Script.runInThisContext` emits the syntax error to stderr
and throws.an exception.


### Script.runInNewContext(code, [sandbox], [filename])

`Script.runInNewContext` compiles `code` to run in `sandbox` as if it were loaded from `filename`,
then runs it and returns the result. Running code does not have access to local scope and
the object `sandbox` will be used as the global object for `code`.
`sandbox` and `filename` are optional.

Example: compile and execute code that increments a global variable and sets a new one.
These globals are contained in the sandbox.

    var util = require('util'),
        Script = process.binding('evals').Script,
        sandbox = {
          animal: 'cat',
          count: 2
        };

    Script.runInNewContext(
      'count += 1; name = "kitty"', sandbox, 'myfile.js');
    console.log(util.inspect(sandbox));

    // { animal: 'cat', count: 3, name: 'kitty' }

Note that running untrusted code is a tricky business requiring great care.  To prevent accidental
global variable leakage, `Script.runInNewContext` is quite useful, but safely running untrusted code
requires a separate process.

In case of syntax error in `code`, `Script.runInThisContext` emits the syntax error to stderr
and throws an exception.


### new Script(code, [filename])

`new Script` compiles `code` as if it were loaded from `filename`,
but does not run it. Instead, it returns a `Script` object representing this compiled code.
This script can be run later many times using methods below.
The returned script is not bound to any global object.
It is bound before each run, just for that run. `filename` is optional.

In case of syntax error in `code`, `new Script` emits the syntax error to stderr
and throws an exception.


### script.runInThisContext()

Similar to `Script.runInThisContext` (note capital 'S'), but now being a method of a precompiled Script object.
`script.runInThisContext` runs the code of `script` and returns the result.
Running code does not have access to local scope, but does have access to the `global` object
(v8: in actual context).

Example of using `script.runInThisContext` to compile code once and run it multiple times:

    var Script = process.binding('evals').Script,
        scriptObj, i;
    
    globalVar = 0;

    scriptObj = new Script('globalVar += 1', 'myfile.js');

    for (i = 0; i < 1000 ; i += 1) {
      scriptObj.runInThisContext();
    }

    console.log(globalVar);

    // 1000


### script.runInNewContext([sandbox])

Similar to `Script.runInNewContext` (note capital 'S'), but now being a method of a precompiled Script object.
`script.runInNewContext` runs the code of `script` with `sandbox` as the global object and returns the result.
Running code does not have access to local scope. `sandbox` is optional.

Example: compile code that increments a global variable and sets one, then execute this code multiple times.
These globals are contained in the sandbox.

    var util = require('util'),
        Script = process.binding('evals').Script,
        scriptObj, i,
        sandbox = {
          animal: 'cat',
          count: 2
        };

    scriptObj = new Script(
        'count += 1; name = "kitty"', 'myfile.js');

    for (i = 0; i < 10 ; i += 1) {
      scriptObj.runInNewContext(sandbox);
    }

    console.log(util.inspect(sandbox));

    // { animal: 'cat', count: 12, name: 'kitty' }

Note that running untrusted code is a tricky business requiring great care.  To prevent accidental
global variable leakage, `script.runInNewContext` is quite useful, but safely running untrusted code
requires a separate process.
