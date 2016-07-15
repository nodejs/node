# Global Objects

<!-- type=misc -->

These objects are available in all modules. Some of these objects aren't
actually in the global scope but in the module scope - this will be noted.

The objects listed here are specific to Node.js. There are a number of
[built-in objects][] that are part of the JavaScript language itself, which are
also globally accessible.

## Class: Buffer

<!-- type=global -->

* {Function}

Used to handle binary data. See the [buffer section][].

## \_\_dirname

<!-- type=var -->

* {String}

The name of the directory that the currently executing script resides in.

Example: running `node example.js` from `/Users/mjr`

```js
console.log(__dirname);
// /Users/mjr
```

`__dirname` isn't actually a global but rather local to each module.

For instance, given two modules: `a` and `b`, where `b` is a dependency of
`a` and there is a directory structure of:

* `/Users/mjr/app/a.js`
* `/Users/mjr/app/node_modules/b/b.js`

References to `__dirname` within `b.js` will return
`/Users/mjr/app/node_modules/b` while references to `__dirname` within `a.js`
will return `/Users/mjr/app`.

## \_\_filename

<!-- type=var -->

* {String}

The filename of the code being executed.  This is the resolved absolute path
of this code file.  For a main program this is not necessarily the same
filename used in the command line.  The value inside a module is the path
to that module file.

Example: running `node example.js` from `/Users/mjr`

```js
console.log(__filename);
// /Users/mjr/example.js
```

`__filename` isn't actually a global but rather local to each module.

## clearImmediate(immediateObject)

<!--type=global-->

[`clearImmediate`] is described in the [timers][] section.

## clearInterval(intervalObject)

<!--type=global-->

[`clearInterval`] is described in the [timers][] section.

## clearTimeout(timeoutObject)

<!--type=global-->

[`clearTimeout`] is described in the [timers][] section.

## console

<!-- type=global -->

* {Object}

Used to print to stdout and stderr. See the [`console`][] section.

## exports

<!-- type=var -->

A reference to the `module.exports` that is shorter to type.
See [module system documentation][] for details on when to use `exports` and
when to use `module.exports`.

`exports` isn't actually a global but rather local to each module.

See the [module system documentation][] for more information.

## global

<!-- type=global -->

* {Object} The global namespace object.

In browsers, the top-level scope is the global scope. That means that in
browsers if you're in the global scope `var something` will define a global
variable. In Node.js this is different. The top-level scope is not the global
scope; `var something` inside an Node.js module will be local to that module.

## module

<!-- type=var -->

* {Object}

A reference to the current module. In particular
`module.exports` is used for defining what a module exports and makes
available through `require()`.

`module` isn't actually a global but rather local to each module.

See the [module system documentation][] for more information.

## process

<!-- type=global -->

* {Object}

The process object. See the [`process` object][] section.

## require()

<!-- type=var -->

* {Function}

To require modules. See the [Modules][] section.  `require` isn't actually a
global but rather local to each module.

### require.cache

* {Object}

Modules are cached in this object when they are required. By deleting a key
value from this object, the next `require` will reload the module. Note that
this does not apply to [native addons][], for which reloading will result in an
Error.

### require.extensions

> Stability: 0 - Deprecated

* {Object}

Instruct `require` on how to handle certain file extensions.

Process files with the extension `.sjs` as `.js`:

```js
require.extensions['.sjs'] = require.extensions['.js'];
```

**Deprecated**  In the past, this list has been used to load
non-JavaScript modules into Node.js by compiling them on-demand.
However, in practice, there are much better ways to do this, such as
loading modules via some other Node.js program, or compiling them to
JavaScript ahead of time.

Since the Module system is locked, this feature will probably never go
away.  However, it may have subtle bugs and complexities that are best
left untouched.

### require.resolve()

Use the internal `require()` machinery to look up the location of a module,
but rather than loading the module, just return the resolved filename.

## setImmediate(callback[, arg][, ...])

<!-- type=global -->

[`setImmediate`] is described in the [timers][] section.

## setInterval(callback, delay[, arg][, ...])

<!-- type=global -->

[`setInterval`] is described in the [timers][] section.

## setTimeout(callback, delay[, arg][, ...])

<!-- type=global -->

[`setTimeout`] is described in the [timers][] section.

[`console`]: console.html
[`process` object]: process.html#process_process
[buffer section]: buffer.html
[module system documentation]: modules.html
[Modules]: modules.html#modules_modules
[native addons]: addons.html
[timers]: timers.html
[`clearImmediate`]: timers.html#timers_clearimmediate_immediate
[`clearInterval`]: timers.html#timers_clearinterval_timeout
[`clearTimeout`]: timers.html#timers_cleartimeout_timeout
[`setImmediate`]: timers.html#timers_setimmediate_callback_arg
[`setInterval`]: timers.html#timers_setinterval_callback_delay_arg
[`setTimeout`]: timers.html#timers_settimeout_callback_delay_arg
[built-in objects]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects
