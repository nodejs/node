# Global Objects

<!-- type=misc -->

Node.js provides a number of global and module scope variables that are
generally accessible to all modules.

## Global-scope Variables

### `Buffer`

<!-- type=global -->

* {Function}

The `Buffer` class is used to handle binary data. See the
[`Buffer` documentation][] for information.

### `clearInterval(t)`

<!--type=global-->

Stops a timer that was previously created with [`setInterval()`][]. The timers
callback function will not be called. See the [Timers documentation][] for
information.

### `clearTimeout(t)`

<!-- type=global -->

Stops a timer that was previously created with [`setTimeout()`][]. The timers
callback function will not be called. See the [Timers documentation][] for
information.

### `console`

<!-- type=global -->

* {Object}

A `Console` object used to print to log information to stdout and
stderr. See the [`Console` documentation][] for information.

### `global`

<!-- type=global -->

* {Object} The global namespace object.

Unlike browsers, which use a single top-level global scope, Node.js modules
have a global and module scope.

Any JavaScript that is run from a `*.js` file is run within a module scope.
This means that when a variable is defined using, for instance,
`var something = 1`, the variable is defined only within the scope of that one
`*.js` file.

On the other hand, when running JavaScript directly from the Node.js command
line REPL, variables are defined in the global scope.

The `global` object provides access to all available globally defined variables.

For example, given the following JavaScript file `myScript.js`:

    // myScript.js
    const something = 1;
    console.log(something);
    console.log(global.something);
    
When the `myScript.js` file is run directly from the command line using
`node myScript.js`, the output is:

    $ node myScript.js
    1
    undefined
    
However, when the same code is run from the Node.js REPL, the output changes:

    $ node
    > const something = 1;
    undefined
    > console.log(something);
    1
    undefined
    > console.log(global.something);
    1
    undefined

### `process`

<!-- type=global -->

* {Object}

References the `Process` object representing the current running Node.js
process. See the [`Process` documentation][] for information.

### `setImmediate(callback)`

the `setImmediate()` method schedules a `callback` function to be run on the
immediate next tick of the Node.js event loop. See the [Timers documentation][]
for more information.

### `setInterval(callback, ms)`

The `setInterval()` method creates a Timer that will call the `callback`
function repeatedly every `ms` milliseconds. Note that the actual
interval may vary, depending on external factors such as operating system
timer granularity and system load. The `callback` will never be called in less
than `ms` milliseconds but it may be longer.

The interval must be in the range of 1-2,147,483,647 (inclusive). If the value
is outside of that range, the value is changed to 1 millisecond. Broadly
speaking, a timer cannot span more than 24.8 days.

The method returns an object representing the timer than can be passed later
to the `clearInterval()` method to halt further calling of the `callback`
function.

See the [Timers documentation][] for more information.

### `setTimeout(callback, ms)`

The `setTimeout()` method creates a Timer that will call the `callback`
function after *at least* `ms` milliseconds. The actual delay depends
on external factors such as the operating system timer granularity and system
load.

The timeout must be in the range of 1-2,147,483,647 (inclusive). If the value is
outside that range, the value is changed to 1 millisecond. Broadly speaking, a
timer cannot span more than 24.8 days.

The method returns an object representing the timer than can be passed later
to the `clearTimeout()` method to halt further calling of the `callback`
function.

See the [Timers documentation][] for more information.

## Module-scope Variables

Module-scoped variables are accessible to all modules but are not true
globals.

### `__dirname`

<!-- type=var -->

* {String}

The name of the directory that the currently executing script resides in.

Example: running `node example.js` from `/Users/mjr`

    console.log(__dirname);
      // Prints: /Users/mjr

### `__filename`

<!-- type=var -->

* {String}

The filename of the code being executed.  This is the resolved absolute path
of this code file.  For a main program this is not necessarily the same
filename used in the command line.  The value inside a module is the path
to that module file.

Example: running `node example.js` from `/Users/mjr`

    console.log(__filename);
      // Prints: /Users/mjr/example.js

### `exports`

<!-- type=var -->

A reference to the `module.exports`. See the [Module system documentation][]
for details on when to use `exports` and when to use `module.exports`.

### `module`

<!-- type=var -->

* {Object}

A reference to the current module. In particular, `module.exports` is used to
define what a module exports and makes available through `require()`.

See the [Module system documentation][] for more information.

## Global and Module Scope

Certain variables are defined at *both* the global and module scopes. The
specific operation of these can vary depending on the current scope in
which JavaScript code is executing.

### `require()`

<!-- type=var -->

* {Function}

The `require()` method is the primary mechanism for importing the functionality
of another module into the current module. See the
[Module system documentation][] for more information.

### `require.cache`

* {Object}

Modules are cached in this object when they are required. By deleting a key
value from this object, the next `require()` will reload the module.

### `require.extensions`

    Stability: 0 - Deprecated

* {Object}

Used to instruct `require` on how to handle certain file extensions.

**Deprecated**: Historically, this list has been used to load
non-JavaScript modules into Node.js by compiling them on-demand.
In practice, however, there are much better ways to do this, such as
loading modules via some other Node.js program, or compiling them to
JavaScript ahead of time.

Because the Module system is Node.js is [Locked][], it is unlikely that this
feature will be removed entirely. However, the mechanism is considered to be
unsupported and should not be used.

Note that files with the extension `*.sjs` are processed as `*.js` files.

    require.extensions['.sjs'] = require.extensions['.js'];

### `require.resolve()`

The `require.resolve()` method is part of the internal machinery that allows
the `require()` method to resolve the location of a module in the file
system. Rather than loading the module, however, the `require.resolve()`
method simply returns the resolved filename.


[`Console` documentation]: console.html
[`Process` documentation]: process.html#process_process
[`setInterval()`]: #globals_setinterval_callback_ms
[`setTimeout()`]: #globals_settimeout_callback_ms
[`Buffer` documentation]: buffer.html
[Module system documentation]: modules.html
[Timers documentation]: timers.html
[Locked]: documentation.html#documentation_stability_index
