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

The process object. See the [process object](process.html#process) section.

## console

<!-- type=global -->

* {Object}

Used to print to stdout and stderr. See the [stdio](stdio.html) section.

## Buffer

<!-- type=global -->

* {Object}

Used to handle binary data. See the [buffer section](buffer.html).

## require()

<!-- type=var -->

* {Function}

To require modules. See the [Modules](modules.html#modules) section.
`require` isn't actually a global but rather local to each module.


### require.resolve()

Use the internal `require()` machinery to look up the location of a module,
but rather than loading the module, just return the resolved filename.

### require.cache

* {Object}

Modules are cached in this object when they are required. By deleting a key
value from this object, the next `require` will reload the module.

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
`module.exports` is the same as the `exports` object. See `src/node.js`
for more information.
`module` isn't actually a global but rather local to each module.


## exports

<!-- type=var -->

An object which is shared between all instances of the current module and
made accessible through `require()`.
`exports` is the same as the `module.exports` object. See `src/node.js`
for more information.
`exports` isn't actually a global but rather local to each module.

See the [module system documentation](modules.html) for more
information.

See the [module section](modules.html) for more information.

## setTimeout(cb, ms)
## clearTimeout(t)
## setInterval(cb, ms)
## clearInterval(t)

<!--type=global-->

The timer functions are global variables. See the [timers](timers.html) section.
