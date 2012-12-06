# Global Objects

<!-- type=misc -->

These objects are available in all modules. Some of these objects aren't
actually in the global scope but in the module scope - this will be noted.

## global

<!-- type=global -->

* {Object} The global namespace object.

In browsers, the top-level scope is the global scope. That means that in
browsers if you're in the global scope `var something` will define a global
variable. In Node this is different. The top-level scope is not the global
scope; `var something` inside a Node module will be local to that module.

## process

<!-- type=global -->

* {Object}

The process object. See the [process object][] section.

## console

<!-- type=global -->

* {Object}

Used to print to stdout and stderr. See the [stdio][] section.

## Class: Buffer

<!-- type=global -->

* {Function}

Used to handle binary data. See the [buffer section][]

## require()

<!-- type=var -->

* {Function}

To require modules. See the [Modules][] section.  `require` isn't actually a
global but rather local to each module.

### require.resolve()

Use the internal `require()` machinery to look up the location of a module,
but rather than loading the module, just return the resolved filename.

### require.cache

* {Object}

Modules are cached in this object when they are required. By deleting a key
value from this object, the next `require` will reload the module.

### require.extensions

* {Array}

Instruct `require` on how to handle certain file extensions.

Process files with the extension `.sjs` as `.js`:

    require.extensions['.sjs'] = require.extensions['.js'];

## __filename

<!-- type=var -->

* {String}

The filename of the code being executed.  This is the resolved absolute path
of this code file.  For a main program this is not necessarily the same
filename used in the command line.  The value inside a module is the path
to that module file.

Example: running `node example.js` from `/Users/mjr`

    console.log(__filename);
    // /Users/mjr/example.js

`__filename` isn't actually a global but rather local to each module.

## __dirname

<!-- type=var -->

* {String}

The name of the directory that the currently executing script resides in.

Example: running `node example.js` from `/Users/mjr`

    console.log(__dirname);
    // /Users/mjr

`__dirname` isn't actually a global but rather local to each module.


## module

<!-- type=var -->

* {Object}

A reference to the current module. In particular
`module.exports` is the same as the `exports` object.
`module` isn't actually a global but rather local to each module.

See the [module system documentation][] for more information.

## exports

<!-- type=var -->

An object which is shared between all instances of the current module and
made accessible through `require()`.
`exports` is the same as the `module.exports` object.
`exports` isn't actually a global but rather local to each module.

See the [module system documentation][] for more information.

See the [module section][] for more information.

## setTimeout(cb, ms)

Run callback `cb` after *at least* `ms` milliseconds. The actual delay depends
on external factors like OS timer granularity and system load.

The timeout must be in the range of 1-2,147,483,647 inclusive. If the value is
outside that range, it's changed to 1 millisecond. Broadly speaking, a timer
cannot span more than 24.8 days.

Returns an opaque value that represents the timer.

## clearTimeout(t)

Stop a timer that was previously created with `setTimeout()`. The callback will
not execute.

## setInterval(cb, ms)

Run callback `cb` repeatedly every `ms` milliseconds. Note that the actual
interval may vary, depending on external factors like OS timer granularity and
system load. It's never less than `ms` but it may be longer.

The interval must be in the range of 1-2,147,483,647 inclusive. If the value is
outside that range, it's changed to 1 millisecond. Broadly speaking, a timer
cannot span more than 24.8 days.

Returns an opaque value that represents the timer.

## clearInterval(t)

Stop a timer that was previously created with `setInterval()`. The callback
will not execute.

<!--type=global-->

The timer functions are global variables. See the [timers][] section.

[buffer section]: buffer.html
[module section]: modules.html
[module system documentation]: modules.html
[Modules]: modules.html#modules_modules
[process object]: process.html#process_process
[stdio]: stdio.html
[timers]: timers.html
