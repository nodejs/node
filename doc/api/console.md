# Console

> Stability: 2 - Stable

The `console` module provides a simple debugging console that is similar to the
JavaScript console mechanism provided by web browsers.

The module exports two specific components:

* A `Console` class with methods such as `console.log()`, `console.error()` and
  `console.warn()` that can be used to write to any Node.js stream.
* A global `console` instance configured to write to `stdout` and `stderr`.
  Because this object is global, it can be used without calling
  `require('console')`.

Example using the global `console`:

```js
console.log('hello world');
  // Prints: hello world, to stdout
console.log('hello %s', 'world');
  // Prints: hello world, to stdout
console.error(new Error('Whoops, something bad happened'));
  // Prints: [Error: Whoops, something bad happened], to stderr

const name = 'Will Robinson';
console.warn(`Danger ${name}! Danger!`);
  // Prints: Danger Will Robinson! Danger!, to stderr
```

Example using the `Console` class:

```js
const out = getStreamSomehow();
const err = getStreamSomehow();
const myConsole = new console.Console(out, err);

myConsole.log('hello world');
  // Prints: hello world, to out
myConsole.log('hello %s', 'world');
  // Prints: hello world, to out
myConsole.error(new Error('Whoops, something bad happened'));
  // Prints: [Error: Whoops, something bad happened], to err

const name = 'Will Robinson';
myConsole.warn(`Danger ${name}! Danger!`);
  // Prints: Danger Will Robinson! Danger!, to err
```

While the API for the `Console` class is designed fundamentally around the
browser `console` object, the `Console` in Node.js is *not* intended to
duplicate the browser's functionality exactly.

## Asynchronous vs Synchronous Consoles

The console functions are usually asynchronous unless the destination is a file.
Disks are fast and operating systems normally employ write-back caching;
it should be a very rare occurrence indeed that a write blocks, but it
is possible.

Additionally, console functions are blocking when outputting to TTYs
(terminals) on OS X as a workaround for the OS's very small, 1kb buffer size.
This is to prevent interleaving between `stdout` and `stderr`.

## Class: Console

<!--type=class-->

The `Console` class can be used to create a simple logger with configurable
output streams and can be accessed using either `require('console').Console`
or `console.Console`:

```js
const Console = require('console').Console;
const Console = console.Console;
```

### new Console(stdout[, stderr])

Creates a new `Console` by passing one or two writable stream instances.
`stdout` is a writable stream to print log or info output. `stderr`
is used for warning or error output. If `stderr` isn't passed, warning and error
output will be sent to `stdout`.

```js
const output = fs.createWriteStream('./stdout.log');
const errorOutput = fs.createWriteStream('./stderr.log');
// custom simple logger
const logger = new Console(output, errorOutput);
// use it like console
var count = 5;
logger.log('count: %d', count);
// in stdout.log: count 5
```

The global `console` is a special `Console` whose output is sent to
[`process.stdout`][] and [`process.stderr`][]. It is equivalent to calling:

```js
new Console(process.stdout, process.stderr);
```

### console.assert(value[, message][, ...])
<!-- YAML
added: v0.1.101
-->

A simple assertion test that verifies whether `value` is truthy. If it is not,
an `AssertionError` is thrown. If provided, the error `message` is formatted
using [`util.format()`][] and used as the error message.

```js
console.assert(true, 'does nothing');
  // OK
console.assert(false, 'Whoops %s', 'didn\'t work');
  // AssertionError: Whoops didn't work
```

*Note: the `console.assert()` method is implemented differently in Node.js
than the `console.assert()` method [available in browsers][web-api-assert].*

Specifically, in browsers, calling `console.assert()` with a falsy
assertion will cause the `message` to be printed to the console without
interrupting execution of subsequent code. In Node.js, however, a falsy
assertion will cause an `AssertionError` to be thrown.

Functionality approximating that implemented by browsers can be implemented
by extending Node.js' `console` and overriding the `console.assert()` method.

In the following example, a simple module is created that extends and overrides
the default behavior of `console` in Node.js.

```js
'use strict';

// Creates a simple extension of console with a
// new impl for assert without monkey-patching.
const myConsole = Object.setPrototypeOf({
  assert(assertion, message, ...args) {
    try {
      console.assert(assertion, message, ...args);
    } catch (err) {
      console.error(err.stack);
    }
  }
}, console);

module.exports = myConsole;
```

This can then be used as a direct replacement for the built in console:

```js
const console = require('./myConsole');
console.assert(false, 'this message will print, but no error thrown');
console.log('this will also print');
```

### console.dir(obj[, options])
<!-- YAML
added: v0.1.101
-->

Uses [`util.inspect()`][] on `obj` and prints the resulting string to `stdout`.
This function bypasses any custom `inspect()` function defined on `obj`. An
optional `options` object may be passed to alter certain aspects of the
formatted string:

- `showHidden` - if `true` then the object's non-enumerable and symbol
properties will be shown too. Defaults to `false`.

- `depth` - tells [`util.inspect()`][] how many times to recurse while
formatting the object. This is useful for inspecting large complicated objects.
Defaults to `2`. To make it recurse indefinitely, pass `null`.

- `colors` - if `true`, then the output will be styled with ANSI color codes.
Defaults to `false`. Colors are customizable; see
[customizing `util.inspect()` colors][].

### console.error([data][, ...])
<!-- YAML
added: v0.1.100
-->

Prints to `stderr` with newline. Multiple arguments can be passed, with the
first used as the primary message and all additional used as substitution
values similar to `printf(3)` (the arguments are all passed to
[`util.format()`][]).

```js
const code = 5;
console.error('error #%d', code);
  // Prints: error #5, to stderr
console.error('error', code);
  // Prints: error 5, to stderr
```

If formatting elements (e.g. `%d`) are not found in the first string then
[`util.inspect()`][] is called on each argument and the resulting string
values are concatenated. See [`util.format()`][] for more information.

### console.info([data][, ...])
<!-- YAML
added: v0.1.100
-->

The `console.info()` function is an alias for [`console.log()`][].

### console.log([data][, ...])
<!-- YAML
added: v0.1.100
-->

Prints to `stdout` with newline. Multiple arguments can be passed, with the
first used as the primary message and all additional used as substitution
values similar to `printf(3)` (the arguments are all passed to
[`util.format()`][]).

```js
var count = 5;
console.log('count: %d', count);
  // Prints: count: 5, to stdout
console.log('count:', count);
  // Prints: count: 5, to stdout
```

If formatting elements (e.g. `%d`) are not found in the first string then
[`util.inspect()`][] is called on each argument and the resulting string
values are concatenated. See [`util.format()`][] for more information.

### console.time(label)
<!-- YAML
added: v0.1.104
-->

Starts a timer that can be used to compute the duration of an operation. Timers
are identified by a unique `label`. Use the same `label` when you call
[`console.timeEnd()`][] to stop the timer and output the elapsed time in
milliseconds to `stdout`. Timer durations are accurate to the sub-millisecond.

### console.timeEnd(label)
<!-- YAML
added: v0.1.104
-->

Stops a timer that was previously started by calling [`console.time()`][] and
prints the result to `stdout`:

```js
console.time('100-elements');
for (var i = 0; i < 100; i++) {
  ;
}
console.timeEnd('100-elements');
// prints 100-elements: 225.438ms
```

*Note: As of Node.js v6.0.0, `console.timeEnd()` deletes the timer to avoid
leaking it. On older versions, the timer persisted. This allowed
`console.timeEnd()` to be called multiple times for the same label. This
functionality was unintended and is no longer supported.*

### console.trace(message[, ...])
<!-- YAML
added: v0.1.104
-->

Prints to `stderr` the string `'Trace :'`, followed by the [`util.format()`][]
formatted message and stack trace to the current position in the code.

```js
console.trace('Show me');
  // Prints: (stack trace will vary based on where trace is called)
  //  Trace: Show me
  //    at repl:2:9
  //    at REPLServer.defaultEval (repl.js:248:27)
  //    at bound (domain.js:287:14)
  //    at REPLServer.runBound [as eval] (domain.js:300:12)
  //    at REPLServer.<anonymous> (repl.js:412:12)
  //    at emitOne (events.js:82:20)
  //    at REPLServer.emit (events.js:169:7)
  //    at REPLServer.Interface._onLine (readline.js:210:10)
  //    at REPLServer.Interface._line (readline.js:549:8)
  //    at REPLServer.Interface._ttyWrite (readline.js:826:14)
```

### console.warn([data][, ...])
<!-- YAML
added: v0.1.100
-->

The `console.warn()` function is an alias for [`console.error()`][].

[`console.error()`]: #console_console_error_data
[`console.log()`]: #console_console_log_data
[`console.time()`]: #console_console_time_label
[`console.timeEnd()`]: #console_console_timeend_label
[`process.stderr`]: process.html#process_process_stderr
[`process.stdout`]: process.html#process_process_stdout
[`util.format()`]: util.html#util_util_format_format
[`util.inspect()`]: util.html#util_util_inspect_object_options
[customizing `util.inspect()` colors]: util.html#util_customizing_util_inspect_colors
[web-api-assert]: https://developer.mozilla.org/en-US/docs/Web/API/console/assert
