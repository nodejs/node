# Global Objects

<!-- type=misc -->

These objects are available in all modules. Some of these objects aren't
actually in the global scope but in the module scope - this will be noted.

The objects listed here are specific to Node.js. There are a number of
[built-in objects][] that are part of the JavaScript language itself, which are
also globally accessible.

## Class: Buffer
<!-- YAML
added: v0.1.103
-->

<!-- type=global -->

* {Function}

Used to handle binary data. See the [buffer section][].

## \_\_dirname
<!-- YAML
added: v0.1.27
-->

<!-- type=var -->

* {string}

The directory name of the current module. This the same as the
[`path.dirname()`][] of the [`__filename`][].

`__dirname` is not actually a global but rather local to each module.

Example: running `node example.js` from `/Users/mjr`

```js
console.log(__dirname);
// Prints: /Users/mjr
console.log(path.dirname(__filename));
// Prints: /Users/mjr
```

## \_\_filename
<!-- YAML
added: v0.0.1
-->

<!-- type=var -->

* {string}

The file name of the current module. This is the resolved absolute path of the
current module file.

For a main program this is not necessarily the same as the file name used in the
command line.

See [`__dirname`][] for the directory name of the current module.

`__filename` is not actually a global but rather local to each module.

Examples:

Running `node example.js` from `/Users/mjr`

```js
console.log(__filename);
// Prints: /Users/mjr/example.js
console.log(__dirname);
// Prints: /Users/mjr
```

Given two modules: `a` and `b`, where `b` is a dependency of
`a` and there is a directory structure of:

* `/Users/mjr/app/a.js`
* `/Users/mjr/app/node_modules/b/b.js`

References to `__filename` within `b.js` will return
`/Users/mjr/app/node_modules/b/b.js` while references to `__filename` within
`a.js` will return `/Users/mjr/app/a.js`.

## clearImmediate(immediateObject)
<!-- YAML
added: v0.9.1
-->

<!--type=global-->

[`clearImmediate`] is described in the [timers][] section.

## clearInterval(intervalObject)
<!-- YAML
added: v0.0.1
-->

<!--type=global-->

[`clearInterval`] is described in the [timers][] section.

## clearTimeout(timeoutObject)
<!-- YAML
added: v0.0.1
-->

<!--type=global-->

[`clearTimeout`] is described in the [timers][] section.

## console
<!-- YAML
added: v0.1.100
-->

<!-- type=global -->

* {Object}

Used to print to stdout and stderr. See the [`console`][] section.

## exports
<!-- YAML
added: v0.1.12
-->

<!-- type=var -->

A reference to the `module.exports` that is shorter to type.
See [module system documentation][] for details on when to use `exports` and
when to use `module.exports`.

`exports` is not actually a global but rather local to each module.

See the [module system documentation][] for more information.

## global
<!-- YAML
added: v0.1.27
-->

<!-- type=global -->

* {Object} The global namespace object.

In browsers, the top-level scope is the global scope. That means that in
browsers if you're in the global scope `var something` will define a global
variable. In Node.js this is different. The top-level scope is not the global
scope; `var something` inside an Node.js module will be local to that module.

## module
<!-- YAML
added: v0.1.16
-->

<!-- type=var -->

* {Object}

A reference to the current module. In particular
`module.exports` is used for defining what a module exports and makes
available through `require()`.

`module` is not actually a global but rather local to each module.

See the [module system documentation][] for more information.

## process
<!-- YAML
added: v0.1.7
-->

<!-- type=global -->

* {Object}

The process object. See the [`process` object][] section.

## require()
<!-- YAML
added: v0.1.13
-->

<!-- type=var -->

* {Function}

To require modules. See the [Modules][] section.  `require` is not actually a
global but rather local to each module.

### require.cache
<!-- YAML
added: v0.3.0
-->

* {Object}

Modules are cached in this object when they are required. By deleting a key
value from this object, the next `require` will reload the module. Note that
this does not apply to [native addons][], for which reloading will result in an
Error.

### require.extensions
<!-- YAML
added: v0.3.0
deprecated: v0.10.6
-->

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

Since the module system is locked, this feature will probably never go
away.  However, it may have subtle bugs and complexities that are best
left untouched.

Note that the number of file system operations that the module system
has to perform in order to resolve a `require(...)` statement to a
filename scales linearly with the number of registered extensions.

In other words, adding extensions slows down the module loader and
should be discouraged.

### require.resolve()
<!-- YAML
added: v0.3.0
-->

Use the internal `require()` machinery to look up the location of a module,
but rather than loading the module, just return the resolved filename.

## setImmediate(callback[, ...args])
<!-- YAML
added: v0.9.1
-->

<!-- type=global -->

[`setImmediate`] is described in the [timers][] section.

## setInterval(callback, delay[, ...args])
<!-- YAML
added: v0.0.1
-->

<!-- type=global -->

[`setInterval`] is described in the [timers][] section.

## setTimeout(callback, delay[, ...args])
<!-- YAML
added: v0.0.1
-->

<!-- type=global -->

[`setTimeout`] is described in the [timers][] section.

[`__dirname`]: #globals_dirname
[`__filename`]: #globals_filename
[`console`]: console.html
[`path.dirname()`]: path.html#path_path_dirname_path
[`process` object]: process.html#process_process
[buffer section]: buffer.html
[module system documentation]: modules.html
[Modules]: modules.html#modules_modules
[native addons]: addons.html
[timers]: timers.html
[`clearImmediate`]: timers.html#timers_clearimmediate_immediate
[`clearInterval`]: timers.html#timers_clearinterval_timeout
[`clearTimeout`]: timers.html#timers_cleartimeout_timeout
[`setImmediate`]: timers.html#timers_setimmediate_callback_args
[`setInterval`]: timers.html#timers_setinterval_callback_delay_args
[`setTimeout`]: timers.html#timers_settimeout_callback_delay_args
[built-in objects]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects
