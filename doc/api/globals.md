# Global Objects

<!--introduced_in=v0.10.0-->
<!-- type=misc -->

These objects are available in all modules. The following variables may appear
to be global but are not. They exist only in the scope of modules, see the
[module system documentation][]:

- [`__dirname`][]
- [`__filename`][]
- [`exports`][]
- [`module`][]
- [`require()`][]

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

This variable may appear to be global but is not. See [`__dirname`].

## \_\_filename

This variable may appear to be global but is not. See [`__filename`].

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

This variable may appear to be global but is not. See [`exports`].

## global
<!-- YAML
added: v0.1.27
-->

<!-- type=global -->

* {Object} The global namespace object.

In browsers, the top-level scope is the global scope. This means that
within the browser `var something` will define a new global variable. In
Node.js this is different. The top-level scope is not the global scope;
`var something` inside a Node.js module will be local to that module.

## module

This variable may appear to be global but is not. See [`module`].

## process
<!-- YAML
added: v0.1.7
-->

<!-- type=global -->

* {Object}

The process object. See the [`process` object][] section.

## require()

This variable may appear to be global but is not. See [`require()`].

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

## URL
<!-- YAML
added: REPLACEME
-->

<!-- type=global -->

The WHATWG `URL` class. See the [`URL`][] section.

## URLSearchParams
<!-- YAML
added: REPLACEME
-->

<!-- type=global -->

The WHATWG `URLSearchParams` class. See the [`URLSearchParams`][] section.

[`__dirname`]: modules.html#modules_dirname
[`__filename`]: modules.html#modules_filename
[`clearImmediate`]: timers.html#timers_clearimmediate_immediate
[`clearInterval`]: timers.html#timers_clearinterval_timeout
[`clearTimeout`]: timers.html#timers_cleartimeout_timeout
[`console`]: console.html
[`exports`]: modules.html#modules_exports
[`module`]: modules.html#modules_module
[`process` object]: process.html#process_process
[`require()`]: modules.html#modules_require
[`setImmediate`]: timers.html#timers_setimmediate_callback_args
[`setInterval`]: timers.html#timers_setinterval_callback_delay_args
[`setTimeout`]: timers.html#timers_settimeout_callback_delay_args
[`URL`]: url.html#url_class_url
[`URLSearchParams`]: url.html#url_class_urlsearchparams
[buffer section]: buffer.html
[built-in objects]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects
[module system documentation]: modules.html
[timers]: timers.html
