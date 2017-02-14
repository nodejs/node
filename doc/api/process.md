# Process

<!-- type=global -->

The `process` object is a `global` that provides information about, and control
over, the current Node.js process. As a global, it is always available to
Node.js applications without using `require()`.

## Process Events

The `process` object is an instance of [`EventEmitter`][].

### Event: 'beforeExit'
<!-- YAML
added: v0.11.12
-->

The `'beforeExit'` event is emitted when Node.js empties its event loop and has
no additional work to schedule. Normally, the Node.js process will exit when
there is no work scheduled, but a listener registered on the `'beforeExit'`
event can make asynchronous calls, and thereby cause the Node.js process to
continue.

The listener callback function is invoked with the value of
[`process.exitCode`][] passed as the only argument.

The `'beforeExit'` event is *not* emitted for conditions causing explicit
termination, such as calling [`process.exit()`][] or uncaught exceptions.

The `'beforeExit'` should *not* be used as an alternative to the `'exit'` event
unless the intention is to schedule additional work.

### Event: 'disconnect'
<!-- YAML
added: v0.7.7
-->

If the Node.js process is spawned with an IPC channel (see the [Child Process][]
and [Cluster][] documentation), the `'disconnect'` event will be emitted when
the IPC channel is closed.

### Event: 'exit'
<!-- YAML
added: v0.1.7
-->

The `'exit'` event is emitted when the Node.js process is about to exit as a
result of either:

* The `process.exit()` method being called explicitly;
* The Node.js event loop no longer having any additional work to perform.

There is no way to prevent the exiting of the event loop at this point, and once
all `'exit'` listeners have finished running the Node.js process will terminate.

The listener callback function is invoked with the exit code specified either
by the [`process.exitCode`][] property, or the `exitCode` argument passed to the
[`process.exit()`] method, as the only argument.

For example:

```js
process.on('exit', (code) => {
  console.log(`About to exit with code: ${code}`);
});
```

Listener functions **must** only perform **synchronous** operations. The Node.js
process will exit immediately after calling the `'exit'` event listeners
causing any additional work still queued in the event loop to be abandoned.
In the following example, for instance, the timeout will never occur:

```js
process.on('exit', (code) => {
  setTimeout(() => {
    console.log('This will not run');
  }, 0);
});
```

### Event: 'message'
<!-- YAML
added: v0.5.10
-->

If the Node.js process is spawned with an IPC channel (see the [Child Process][]
and [Cluster][] documentation), the `'message'` event is emitted whenever a
message sent by a parent process using [`childprocess.send()`][] is received by
the child process.

The listener callback is invoked with the following arguments:
* `message` {Object} a parsed JSON object or primitive value
* `sendHandle` {Handle object} a [`net.Socket`][] or [`net.Server`][] object, or
  undefined.


### Event: 'rejectionHandled'
<!-- YAML
added: v1.4.1
-->

The `'rejectionHandled'` event is emitted whenever a `Promise` has been rejected
and an error handler was attached to it (using [`promise.catch()`][], for
example) later than one turn of the Node.js event loop.

The listener callback is invoked with a reference to the rejected `Promise` as
the only argument.

The `Promise` object would have previously been emitted in an
`'unhandledRejection'` event, but during the course of processing gained a
rejection handler.

There is no notion of a top level for a `Promise` chain at which rejections can
always be handled. Being inherently asynchronous in nature, a `Promise`
rejection can be handled at a future point in time — possibly much later than
the event loop turn it takes for the `'unhandledRejection'` event to be emitted.

Another way of stating this is that, unlike in synchronous code where there is
an ever-growing list of unhandled exceptions, with Promises there can be a
growing-and-shrinking list of unhandled rejections.

In synchronous code, the `'uncaughtException'` event is emitted when the list of
unhandled exceptions grows.

In asynchronous code, the `'unhandledRejection'` event is emitted when the list
of unhandled rejections grows, and the `'rejectionHandled'` event is emitted
when the list of unhandled rejections shrinks.

For example:

```js
const unhandledRejections = new Map();
process.on('unhandledRejection', (reason, p) => {
  unhandledRejections.set(p, reason);
});
process.on('rejectionHandled', (p) => {
  unhandledRejections.delete(p);
});
```

In this example, the `unhandledRejections` `Map` will grow and shrink over time,
reflecting rejections that start unhandled and then become handled. It is
possible to record such errors in an error log, either periodically (which is
likely best for long-running application) or upon process exit (which is likely
most convenient for scripts).

### Event: 'uncaughtException'
<!-- YAML
added: v0.1.18
-->

The `'uncaughtException'` event is emitted when an uncaught JavaScript
exception bubbles all the way back to the event loop. By default, Node.js
handles such exceptions by printing the stack trace to `stderr` and exiting.
Adding a handler for the `'uncaughtException'` event overrides this default
behavior.

The listener function is called with the `Error` object passed as the only
argument.

For example:

```js
process.on('uncaughtException', (err) => {
  fs.writeSync(1, `Caught exception: ${err}`);
});

setTimeout(() => {
  console.log('This will still run.');
}, 500);

// Intentionally cause an exception, but don't catch it.
nonexistentFunc();
console.log('This will not run.');
```

#### Warning: Using `'uncaughtException'` correctly

Note that `'uncaughtException'` is a crude mechanism for exception handling
intended to be used only as a last resort. The event *should not* be used as
an equivalent to `On Error Resume Next`. Unhandled exceptions inherently mean
that an application is in an undefined state. Attempting to resume application
code without properly recovering from the exception can cause additional
unforeseen and unpredictable issues.

Exceptions thrown from within the event handler will not be caught. Instead the
process will exit with a non-zero exit code and the stack trace will be printed.
This is to avoid infinite recursion.

Attempting to resume normally after an uncaught exception can be similar to
pulling out of the power cord when upgrading a computer -- nine out of ten
times nothing happens - but the 10th time, the system becomes corrupted.

The correct use of `'uncaughtException'` is to perform synchronous cleanup
of allocated resources (e.g. file descriptors, handles, etc) before shutting
down the process. **It is not safe to resume normal operation after
`'uncaughtException'`.**

To restart a crashed application in a more reliable way, whether `uncaughtException`
is emitted or not, an external monitor should be employed in a separate process
to detect application failures and recover or restart as needed.

### Event: 'unhandledRejection'
<!-- YAML
added: v1.4.1
-->

The `'unhandledRejection`' event is emitted whenever a `Promise` is rejected and
no error handler is attached to the promise within a turn of the event loop.
When programming with Promises, exceptions are encapsulated as "rejected
promises". Rejections can be caught and handled using [`promise.catch()`][] and
are propagated through a `Promise` chain. The `'unhandledRejection'` event is
useful for detecting and keeping track of promises that were rejected whose
rejections have not yet been handled.

The listener function is called with the following arguments:

* `reason` {Error|any} The object with which the promise was rejected
  (typically an [`Error`][] object).
* `p` the `Promise` that was rejected.

For example:

```js
process.on('unhandledRejection', (reason, p) => {
  console.log('Unhandled Rejection at: Promise', p, 'reason:', reason);
  // application specific logging, throwing an error, or other logic here
});

somePromise.then((res) => {
  return reportToUser(JSON.pasre(res)); // note the typo (`pasre`)
}); // no `.catch` or `.then`
```

The following will also trigger the `'unhandledRejection'` event to be
emitted:

```js
function SomeResource() {
  // Initially set the loaded status to a rejected promise
  this.loaded = Promise.reject(new Error('Resource not yet loaded!'));
}

var resource = new SomeResource();
// no .catch or .then on resource.loaded for at least a turn
```

In this example case, it is possible to track the rejection as a developer error
as would typically be the case for other `'unhandledRejection'` events. To
address such failures, a non-operational
[`.catch(() => { })`][`promise.catch()`] handler may be attached to
`resource.loaded`, which would prevent the `'unhandledRejection'` event from
being emitted. Alternatively, the [`'rejectionHandled'`][] event may be used.

### Event: 'warning'
<!-- YAML
added: v6.0.0
-->

The `'warning'` event is emitted whenever Node.js emits a process warning.

A process warning is similar to an error in that it describes exceptional
conditions that are being brought to the user's attention. However, warnings
are not part of the normal Node.js and JavaScript error handling flow.
Node.js can emit warnings whenever it detects bad coding practices that could
lead to sub-optimal application performance, bugs or security vulnerabilities.

The listener function is called with a single `warning` argument whose value is
an `Error` object. There are three key properties that describe the warning:

* `name` {String} The name of the warning (currently `Warning` by default).
* `message` {String} A system-provided description of the warning.
* `stack` {String} A stack trace to the location in the code where the warning
  was issued.

```js
process.on('warning', (warning) => {
  console.warn(warning.name);    // Print the warning name
  console.warn(warning.message); // Print the warning message
  console.warn(warning.stack);   // Print the stack trace
});
```

By default, Node.js will print process warnings to `stderr`. The `--no-warnings`
command-line option can be used to suppress the default console output but the
`'warning'` event will still be emitted by the `process` object.

The following example illustrates the warning that is printed to `stderr` when
too many listeners have been added to an event

```txt
$ node
> event.defaultMaxListeners = 1;
> process.on('foo', () => {});
> process.on('foo', () => {});
> (node:38638) Warning: Possible EventEmitter memory leak detected. 2 foo
... listeners added. Use emitter.setMaxListeners() to increase limit
```

In contrast, the following example turns off the default warning output and
adds a custom handler to the `'warning'` event:

```txt
$ node --no-warnings
> var p = process.on('warning', (warning) => console.warn('Do not do that!'));
> event.defaultMaxListeners = 1;
> process.on('foo', () => {});
> process.on('foo', () => {});
> Do not do that!
```

The `--trace-warnings` command-line option can be used to have the default
console output for warnings include the full stack trace of the warning.

Launching Node.js using the `--throw-deprecation` command line flag will
cause custom deprecation warnings to be thrown as exceptions.

Using the `--trace-deprecation` command line flag will cause the custom
deprecation to be printed to `stderr` along with the stack trace.

Using the `--no-deprecation` command line flag will suppress all reporting
of the custom deprecation.

The `*-deprecation` command line flags only affect warnings that use the name
`DeprecationWarning`.

#### Emitting custom warnings

See the [`process.emitWarning()`][process_emit_warning] method for issuing
custom or application-specific warnings.

### Signal Events

<!--type=event-->
<!--name=SIGINT, SIGHUP, etc.-->

Signal events will be emitted when the Node.js process receives a signal. Please
refer to signal(7) for a listing of standard POSIX signal names such as
`SIGINT`, `SIGHUP`, etc.

The name of each event will be the uppercase common name for the signal (e.g.
`'SIGINT'` for `SIGINT` signals).

For example:

```js
// Begin reading from stdin so the process does not exit.
process.stdin.resume();

process.on('SIGINT', () => {
  console.log('Received SIGINT.  Press Control-D to exit.');
});
```

*Note*: An easy way to send the `SIGINT` signal is with `<Ctrl>-C` in most
terminal programs.

It is important to take note of the following:

* `SIGUSR1` is reserved by Node.js to start the debugger.  It's possible to
  install a listener but doing so will _not_ stop the debugger from starting.
* `SIGTERM` and `SIGINT` have default handlers on non-Windows platforms that
  resets the terminal mode before exiting with code `128 + signal number`. If
  one of these signals has a listener installed, its default behavior will be
  removed (Node.js will no longer exit).
* `SIGPIPE` is ignored by default. It can have a listener installed.
* `SIGHUP` is generated on Windows when the console window is closed, and on
  other platforms under various similar conditions, see signal(7). It can have a
  listener installed, however Node.js will be unconditionally terminated by
  Windows about 10 seconds later. On non-Windows platforms, the default
  behavior of `SIGHUP` is to terminate Node.js, but once a listener has been
  installed its default behavior will be removed.
* `SIGTERM` is not supported on Windows, it can be listened on.
* `SIGINT` from the terminal is supported on all platforms, and can usually be
  generated with `CTRL+C` (though this may be configurable). It is not generated
  when terminal raw mode is enabled.
* `SIGBREAK` is delivered on Windows when `<Ctrl>+<Break>` is pressed, on
  non-Windows platforms it can be listened on, but there is no way to send or
  generate it.
* `SIGWINCH` is delivered when the console has been resized. On Windows, this
  will only happen on write to the console when the cursor is being moved, or
  when a readable tty is used in raw mode.
* `SIGKILL` cannot have a listener installed, it will unconditionally terminate
  Node.js on all platforms.
* `SIGSTOP` cannot have a listener installed.
* `SIGBUS`, `SIGFPE`, `SIGSEGV` and `SIGILL`, when not raised artificially
   using kill(2), inherently leave the process in a state from which it is not
   safe to attempt to call JS listeners. Doing so might lead to the process
   hanging in an endless loop, since listeners attached using `process.on()` are
   called asynchronously and therefore unable to correct the underlying problem.

*Note*: Windows does not support sending signals, but Node.js offers some
emulation with [`process.kill()`][], and [`ChildProcess.kill()`][]. Sending
signal `0` can be used to test for the existence of a process. Sending `SIGINT`,
`SIGTERM`, and `SIGKILL` cause the unconditional termination of the target
process.

## process.abort()
<!-- YAML
added: v0.7.0
-->

The `process.abort()` method causes the Node.js process to exit immediately and
generate a core file.

## process.arch
<!-- YAML
added: v0.5.0
-->

* {String}

The `process.arch` property returns a String identifying the processor
architecture that the Node.js process is currently running on. For instance
`'arm'`, `'ia32'`, or `'x64'`.

```js
console.log(`This processor architecture is ${process.arch}`);
```

## process.argv
<!-- YAML
added: v0.1.27
-->

* {Array}

The `process.argv` property returns an array containing the command line
arguments passed when the Node.js process was launched. The first element will
be [`process.execPath`]. See `process.argv0` if access to the original value of
`argv[0]` is needed.  The second element will be the path to the JavaScript
file being executed. The remaining elements will be any additional command line
arguments.

For example, assuming the following script for `process-args.js`:

```js
// print process.argv
process.argv.forEach((val, index) => {
  console.log(`${index}: ${val}`);
});
```

Launching the Node.js process as:

```console
$ node process-2.js one two=three four
```

Would generate the output:

```text
0: /usr/local/bin/node
1: /Users/mjr/work/node/process-2.js
2: one
3: two=three
4: four
```

## process.argv0
<!-- YAML
added: 6.4.0
-->

* {String}

The `process.argv0` property stores a read-only copy of the original value of
`argv[0]` passed when Node.js starts.

```console
$ bash -c 'exec -a customArgv0 ./node'
> process.argv[0]
'/Volumes/code/external/node/out/Release/node'
> process.argv0
'customArgv0'
```

## process.channel
<!-- YAML
added: v7.1.0
-->

If the Node.js process was spawned with an IPC channel (see the
[Child Process][] documentation), the `process.channel`
property is a reference to the IPC channel. If no IPC channel exists, this
property is `undefined`.

## process.chdir(directory)
<!-- YAML
added: v0.1.17
-->

* `directory` {String}

The `process.chdir()` method changes the current working directory of the
Node.js process or throws an exception if doing so fails (for instance, if
the specified `directory` does not exist).

```js
console.log(`Starting directory: ${process.cwd()}`);
try {
  process.chdir('/tmp');
  console.log(`New directory: ${process.cwd()}`);
}
catch (err) {
  console.log(`chdir: ${err}`);
}
```

## process.config
<!-- YAML
added: v0.7.7
-->

* {Object}

The `process.config` property returns an Object containing the JavaScript
representation of the configure options used to compile the current Node.js
executable. This is the same as the `config.gypi` file that was produced when
running the `./configure` script.

An example of the possible output looks like:

```js
{
  target_defaults:
   { cflags: [],
     default_configuration: 'Release',
     defines: [],
     include_dirs: [],
     libraries: [] },
  variables:
   {
     host_arch: 'x64',
     node_install_npm: 'true',
     node_prefix: '',
     node_shared_cares: 'false',
     node_shared_http_parser: 'false',
     node_shared_libuv: 'false',
     node_shared_zlib: 'false',
     node_use_dtrace: 'false',
     node_use_openssl: 'true',
     node_shared_openssl: 'false',
     strict_aliasing: 'true',
     target_arch: 'x64',
     v8_use_snapshot: 'true'
   }
}
```

*Note*: The `process.config` property is **not** read-only and there are
existing modules in the ecosystem that are known to extend, modify, or entirely
replace the value of `process.config`.

## process.connected
<!-- YAML
added: v0.7.2
-->

* {Boolean}

If the Node.js process is spawned with an IPC channel (see the [Child Process][]
and [Cluster][] documentation), the `process.connected` property will return
`true` so long as the IPC channel is connected and will return `false` after
`process.disconnect()` is called.

Once `process.connected` is `false`, it is no longer possible to send messages
over the IPC channel using `process.send()`.

## process.cpuUsage([previousValue])
<!-- YAML
added: v6.1.0
-->

* `previousValue` {Object} A previous return value from calling
  `process.cpuUsage()`
* Returns: {Object}
    * `user` {Integer}
    * `system` {Integer}

The `process.cpuUsage()` method returns the user and system CPU time usage of
the current process, in an object with properties `user` and `system`, whose
values are microsecond values (millionth of a second). These values measure time
spent in user and system code respectively, and may end up being greater than
actual elapsed time if multiple CPU cores are performing work for this process.

The result of a previous call to `process.cpuUsage()` can be passed as the
argument to the function, to get a diff reading.

```js
const startUsage = process.cpuUsage();
// { user: 38579, system: 6986 }

// spin the CPU for 500 milliseconds
const now = Date.now();
while (Date.now() - now < 500);

console.log(process.cpuUsage(startUsage));
// { user: 514883, system: 11226 }
```

## process.cwd()
<!-- YAML
added: v0.1.8
-->

* Returns: {String}

The `process.cwd()` method returns the current working directory of the Node.js
process.

```js
console.log(`Current directory: ${process.cwd()}`);
```

## process.disconnect()
<!-- YAML
added: v0.7.2
-->

If the Node.js process is spawned with an IPC channel (see the [Child Process][]
and [Cluster][] documentation), the `process.disconnect()` method will close the
IPC channel to the parent process, allowing the child process to exit gracefully
once there are no other connections keeping it alive.

The effect of calling `process.disconnect()` is that same as calling the parent
process's [`ChildProcess.disconnect()`][].

If the Node.js process was not spawned with an IPC channel,
`process.disconnect()` will be `undefined`.

## process.env
<!-- YAML
added: v0.1.27
-->

* {Object}

The `process.env` property returns an object containing the user environment.
See environ(7).

An example of this object looks like:

```js
{
  TERM: 'xterm-256color',
  SHELL: '/usr/local/bin/bash',
  USER: 'maciej',
  PATH: '~/.bin/:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin',
  PWD: '/Users/maciej',
  EDITOR: 'vim',
  SHLVL: '1',
  HOME: '/Users/maciej',
  LOGNAME: 'maciej',
  _: '/usr/local/bin/node'
}
```

It is possible to modify this object, but such modifications will not be
reflected outside the Node.js process. In other words, the following example
would not work:

```console
$ node -e 'process.env.foo = "bar"' && echo $foo
```

While the following will:

```js
process.env.foo = 'bar';
console.log(process.env.foo);
```

Assigning a property on `process.env` will implicitly convert the value
to a string.

Example:

```js
process.env.test = null;
console.log(process.env.test);
// => 'null'
process.env.test = undefined;
console.log(process.env.test);
// => 'undefined'
```

Use `delete` to delete a property from `process.env`.

Example:

```js
process.env.TEST = 1;
delete process.env.TEST;
console.log(process.env.TEST);
// => undefined
```

On Windows operating systems, environment variables are case-insensitive.

Example:

```js
process.env.TEST = 1;
console.log(process.env.test);
// => 1
```

## process.emitWarning(warning[, type[, code]][, ctor])
<!-- YAML
added: v6.0.0
-->

* `warning` {String | Error} The warning to emit.
* `type` {String} When `warning` is a String, `type` is the name to use
  for the *type* of warning being emitted. Default: `Warning`.
* `code` {String} A unique identifier for the warning instance being emitted.
* `ctor` {Function} When `warning` is a String, `ctor` is an optional
  function used to limit the generated stack trace. Default
  `process.emitWarning`

The `process.emitWarning()` method can be used to emit custom or application
specific process warnings. These can be listened for by adding a handler to the
[`process.on('warning')`][process_warning] event.

```js
// Emit a warning using a string...
process.emitWarning('Something happened!');
// Emits: (node: 56338) Warning: Something happened!
```

```js
// Emit a warning using a string and a type...
process.emitWarning('Something Happened!', 'CustomWarning');
// Emits: (node:56338) CustomWarning: Something Happened!
```

```js
process.emitWarning('Something happened!', 'CustomWarning', 'WARN001');
// Emits: (node:56338) CustomWarning [WARN001]: Something Happened!
```

In each of the previous examples, an `Error` object is generated internally by
`process.emitWarning()` and passed through to the
[`process.on('warning')`][process_warning] event.

```js
process.on('warning', (warning) => {
  console.warn(warning.name);
  console.warn(warning.message);
  console.warn(warning.code);
  console.warn(warning.stack);
});
```

If `warning` is passed as an `Error` object, it will be passed through to the
`process.on('warning')` event handler unmodified (and the optional `type`,
`code` and `ctor` arguments will be ignored):

```js
// Emit a warning using an Error object...
const myWarning = new Error('Warning! Something happened!');
// Use the Error name property to specify the type name
myWarning.name = 'CustomWarning';
myWarning.code = 'WARN001';

process.emitWarning(myWarning);
// Emits: (node:56338) CustomWarning [WARN001]: Warning! Something Happened!
```

A `TypeError` is thrown if `warning` is anything other than a string or `Error`
object.

Note that while process warnings use `Error` objects, the process warning
mechanism is **not** a replacement for normal error handling mechanisms.

The following additional handling is implemented if the warning `type` is
`DeprecationWarning`:

* If the `--throw-deprecation` command-line flag is used, the deprecation
  warning is thrown as an exception rather than being emitted as an event.
* If the `--no-deprecation` command-line flag is used, the deprecation
  warning is suppressed.
* If the `--trace-deprecation` command-line flag is used, the deprecation
  warning is printed to `stderr` along with the full stack trace.

### Avoiding duplicate warnings

As a best practice, warnings should be emitted only once per process. To do
so, it is recommended to place the `emitWarning()` behind a simple boolean
flag as illustrated in the example below:

```js
function emitMyWarning() {
  if (!emitMyWarning.warned) {
    emitMyWarning.warned = true;
    process.emitWarning('Only warn once!');
  }
}
emitMyWarning();
// Emits: (node: 56339) Warning: Only warn once!
emitMyWarning();
// Emits nothing
```

## process.execArgv
<!-- YAML
added: v0.7.7
-->

* {Object}

The `process.execArgv` property returns the set of Node.js-specific command-line
options passed when the Node.js process was launched. These options do not
appear in the array returned by the [`process.argv`][] property, and do not
include the Node.js executable, the name of the script, or any options following
the script name. These options are useful in order to spawn child processes with
the same execution environment as the parent.

For example:

```console
$ node --harmony script.js --version
```

Results in `process.execArgv`:

```js
['--harmony']
```

And `process.argv`:

```js
['/usr/local/bin/node', 'script.js', '--version']
```

## process.execPath
<!-- YAML
added: v0.1.100
-->

* {String}

The `process.execPath` property returns the absolute pathname of the executable
that started the Node.js process.

For example:

```js
'/usr/local/bin/node'
```


## process.exit([code])
<!-- YAML
added: v0.1.13
-->

* `code` {Integer} The exit code. Defaults to `0`.

The `process.exit()` method instructs Node.js to terminate the process as
quickly as possible with the specified exit `code`. If the `code` is omitted,
exit uses either the 'success' code `0` or the value of `process.exitCode` if
specified.

To exit with a 'failure' code:

```js
process.exit(1);
```

The shell that executed Node.js should see the exit code as `1`.

It is important to note that calling `process.exit()` will force the process to
exit as quickly as possible *even if there are still asynchronous operations
pending* that have not yet completed fully, *including* I/O operations to
`process.stdout` and `process.stderr`.

In most situations, it is not actually necessary to call `process.exit()`
explicitly. The Node.js process will exit on its own *if there is no additional
work pending* in the event loop. The `process.exitCode` property can be set to
tell the process which exit code to use when the process exits gracefully.

For instance, the following example illustrates a *misuse* of the
`process.exit()` method that could lead to data printed to stdout being
truncated and lost:

```js
// This is an example of what *not* to do:
if (someConditionNotMet()) {
  printUsageToStdout();
  process.exit(1);
}
```

The reason this is problematic is because writes to `process.stdout` in Node.js
are sometimes *non-blocking* and may occur over multiple ticks of the Node.js
event loop. Calling `process.exit()`, however, forces the process to exit
*before* those additional writes to `stdout` can be performed.

Rather than calling `process.exit()` directly, the code *should* set the
`process.exitCode` and allow the process to exit naturally by avoiding
scheduling any additional work for the event loop:

```js
// How to properly set the exit code while letting
// the process exit gracefully.
if (someConditionNotMet()) {
  printUsageToStdout();
  process.exitCode = 1;
}
```

If it is necessary to terminate the Node.js process due to an error condition,
throwing an *uncaught* error and allowing the process to terminate accordingly
is safer than calling `process.exit()`.

## process.exitCode
<!-- YAML
added: v0.11.8
-->

* {Integer}

A number which will be the process exit code, when the process either
exits gracefully, or is exited via [`process.exit()`][] without specifying
a code.

Specifying a code to [`process.exit(code)`][`process.exit()`] will override any
previous setting of `process.exitCode`.


## process.getegid()
<!-- YAML
added: v2.0.0
-->

The `process.getegid()` method returns the numerical effective group identity
of the Node.js process. (See getegid(2).)

```js
if (process.getegid) {
  console.log(`Current gid: ${process.getegid()}`);
}
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows
or Android)

## process.geteuid()
<!-- YAML
added: v2.0.0
-->

* Returns: {Object}

The `process.geteuid()` method returns the numerical effective user identity of
the process. (See geteuid(2).)

```js
if (process.geteuid) {
  console.log(`Current uid: ${process.geteuid()}`);
}
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)

## process.getgid()
<!-- YAML
added: v0.1.31
-->

* Returns: {Object}

The `process.getgid()` method returns the numerical group identity of the
process. (See getgid(2).)

```js
if (process.getgid) {
  console.log(`Current gid: ${process.getgid()}`);
}
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)


## process.getgroups()
<!-- YAML
added: v0.9.4
-->

* Returns: {Array}

The `process.getgroups()` method returns an array with the supplementary group
IDs. POSIX leaves it unspecified if the effective group ID is included but
Node.js ensures it always is.

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)

## process.getuid()
<!-- YAML
added: v0.1.28
-->

* Returns: {Integer}

The `process.getuid()` method returns the numeric user identity of the process.
(See getuid(2).)

```js
if (process.getuid) {
  console.log(`Current uid: ${process.getuid()}`);
}
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)

## process.hrtime([time])
<!-- YAML
added: v0.7.6
-->

* `time` {Array} The result of a previous call to `process.hrtime()`
* Returns: {Array}

The `process.hrtime()` method returns the current high-resolution real time
in a `[seconds, nanoseconds]` tuple Array, where `nanoseconds` is the
remaining part of the real time that can't be represented in second precision.

`time` is an optional parameter that must be the result of a previous
`process.hrtime()` call to diff with the current time. If the parameter
passed in is not a tuple Array, a `TypeError` will be thrown. Passing in a
user-defined array instead of the result of a previous call to
`process.hrtime()` will lead to undefined behavior.

These times are relative to an arbitrary time in the
past, and not related to the time of day and therefore not subject to clock
drift. The primary use is for measuring performance between intervals:

```js
const NS_PER_SEC = 1e9;
var time = process.hrtime();
// [ 1800216, 25 ]

setTimeout(() => {
  var diff = process.hrtime(time);
  // [ 1, 552 ]

  console.log(`Benchmark took ${diff[0] * NS_PER_SEC + diff[1]} nanoseconds`);
  // benchmark took 1000000552 nanoseconds
}, 1000);
```


## process.initgroups(user, extra_group)
<!-- YAML
added: v0.9.4
-->

* `user` {String|number} The user name or numeric identifier.
* `extra_group` {String|number} A group name or numeric identifier.

The `process.initgroups()` method reads the `/etc/group` file and initializes
the group access list, using all groups of which the user is a member. This is
a privileged operation that requires that the Node.js process either have `root`
access or the `CAP_SETGID` capability.

Note that care must be taken when dropping privileges. Example:

```js
console.log(process.getgroups());         // [ 0 ]
process.initgroups('bnoordhuis', 1000);   // switch user
console.log(process.getgroups());         // [ 27, 30, 46, 1000, 0 ]
process.setgid(1000);                     // drop root gid
console.log(process.getgroups());         // [ 27, 30, 46, 1000 ]
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)

## process.kill(pid[, signal])
<!-- YAML
added: v0.0.6
-->

* `pid` {number} A process ID
* `signal` {String|number} The signal to send, either as a string or number.
  Defaults to `'SIGTERM'`.

The `process.kill()` method sends the `signal` to the process identified by
`pid`.

Signal names are strings such as `'SIGINT'` or `'SIGHUP'`. See [Signal Events][]
and kill(2) for more information.

This method will throw an error if the target `pid` does not exist. As a special
case, a signal of `0` can be used to test for the existence of a process.
Windows platforms will throw an error if the `pid` is used to kill a process
group.

*Note*:Even though the name of this function is `process.kill()`, it is really
just a signal sender, like the `kill` system call.  The signal sent may do
something other than kill the target process.

For example:

```js
process.on('SIGHUP', () => {
  console.log('Got SIGHUP signal.');
});

setTimeout(() => {
  console.log('Exiting.');
  process.exit(0);
}, 100);

process.kill(process.pid, 'SIGHUP');
```

*Note*: When `SIGUSR1` is received by a Node.js process, Node.js will start the
debugger, see [Signal Events][].

## process.mainModule
<!-- YAML
added: v0.1.17
-->

The `process.mainModule` property provides an alternative way of retrieving
[`require.main`][]. The difference is that if the main module changes at
runtime, [`require.main`][] may still refer to the original main module in
modules that were required before the change occurred. Generally, it's
safe to assume that the two refer to the same module.

As with [`require.main`][], `process.mainModule` will be `undefined` if there
is no entry script.

## process.memoryUsage()
<!-- YAML
added: v0.1.16
-->

* Returns: {Object}
    * `rss` {Integer}
    * `heapTotal` {Integer}
    * `heapUsed` {Integer}
    * `external` {Integer}

The `process.memoryUsage()` method returns an object describing the memory usage
of the Node.js process measured in bytes.

For example, the code:

```js
console.log(process.memoryUsage());
```

Will generate:

```js
{
  rss: 4935680,
  heapTotal: 1826816,
  heapUsed: 650472,
  external: 49879
}
```

`heapTotal` and `heapUsed` refer to V8's memory usage.
`external` refers to the memory usage of C++ objects bound to JavaScript
objects managed by V8.

## process.nextTick(callback[, ...args])
<!-- YAML
added: v0.1.26
-->

* `callback` {Function}
* `...args` {any} Additional arguments to pass when invoking the `callback`

The `process.nextTick()` method adds the `callback` to the "next tick queue".
Once the current turn of the event loop turn runs to completion, all callbacks
currently in the next tick queue will be called.

This is *not* a simple alias to [`setTimeout(fn, 0)`][]. It is much more
efficient.  It runs before any additional I/O events (including
timers) fire in subsequent ticks of the event loop.

```js
console.log('start');
process.nextTick(() => {
  console.log('nextTick callback');
});
console.log('scheduled');
// Output:
// start
// scheduled
// nextTick callback
```

This is important when developing APIs in order to give users the opportunity
to assign event handlers *after* an object has been constructed but before any
I/O has occurred:

```js
function MyThing(options) {
  this.setupOptions(options);

  process.nextTick(() => {
    this.startDoingStuff();
  });
}

var thing = new MyThing();
thing.getReadyForStuff();

// thing.startDoingStuff() gets called now, not before.
```

It is very important for APIs to be either 100% synchronous or 100%
asynchronous.  Consider this example:

```js
// WARNING!  DO NOT USE!  BAD UNSAFE HAZARD!
function maybeSync(arg, cb) {
  if (arg) {
    cb();
    return;
  }

  fs.stat('file', cb);
}
```

This API is hazardous because in the following case:

```js
maybeSync(true, () => {
  foo();
});
bar();
```

It is not clear whether `foo()` or `bar()` will be called first.

The following approach is much better:

```js
function definitelyAsync(arg, cb) {
  if (arg) {
    process.nextTick(cb);
    return;
  }

  fs.stat('file', cb);
}
```

*Note*: the next tick queue is completely drained on each pass of the
event loop **before** additional I/O is processed.  As a result,
recursively setting nextTick callbacks will block any I/O from
happening, just like a `while(true);` loop.

## process.pid
<!-- YAML
added: v0.1.15
-->

* {Integer}

The `process.pid` property returns the PID of the process.

```js
console.log(`This process is pid ${process.pid}`);
```

## process.platform
<!-- YAML
added: v0.1.16
-->

* {String}

The `process.platform` property returns a string identifying the operating
system platform on which the Node.js process is running. For instance
`'darwin'`, `'freebsd'`, `'linux'`, `'sunos'` or `'win32'`

```js
console.log(`This platform is ${process.platform}`);
```

## process.release
<!-- YAML
added: v3.0.0
-->

The `process.release` property returns an Object containing metadata related to
the current release, including URLs for the source tarball and headers-only
tarball.

`process.release` contains the following properties:

* `name` {String} A value that will always be `'node'` for Node.js. For
  legacy io.js releases, this will be `'io.js'`.
* `sourceUrl` {String} an absolute URL pointing to a _`.tar.gz`_ file containing
  the source code of the current release.
* `headersUrl`{String} an absolute URL pointing to a _`.tar.gz`_ file containing
  only the source header files for the current release. This file is
  significantly smaller than the full source file and can be used for compiling
  Node.js native add-ons.
* `libUrl` {String} an absolute URL pointing to a _`node.lib`_ file matching the
  architecture and version of the current release. This file is used for
  compiling Node.js native add-ons. _This property is only present on Windows
  builds of Node.js and will be missing on all other platforms._
* `lts` {String} a string label identifying the [LTS][] label for this release.
  If the Node.js release is not an LTS release, this will be `undefined`.

For example:

```js
{
  name: 'node',
  lts: 'Argon',
  sourceUrl: 'https://nodejs.org/download/release/v4.4.5/node-v4.4.5.tar.gz',
  headersUrl: 'https://nodejs.org/download/release/v4.4.5/node-v4.4.5-headers.tar.gz',
  libUrl: 'https://nodejs.org/download/release/v4.4.5/win-x64/node.lib'
}
```

In custom builds from non-release versions of the source tree, only the
`name` property may be present. The additional properties should not be
relied upon to exist.

## process.send(message[, sendHandle[, options]][, callback])
<!-- YAML
added: v0.5.9
-->

* `message` {Object}
* `sendHandle` {Handle object}
* `options` {Object}
* `callback` {Function}
* Returns: {Boolean}

If Node.js is spawned with an IPC channel, the `process.send()` method can be
used to send messages to the parent process. Messages will be received as a
[`'message'`][] event on the parent's [`ChildProcess`][] object.

If Node.js was not spawned with an IPC channel, `process.send()` will be
`undefined`.

*Note*: This function uses [`JSON.stringify()`][] internally to serialize the
`message`.*

## process.setegid(id)
<!-- YAML
added: v2.0.0
-->

* `id` {String|number} A group name or ID

The `process.setegid()` method sets the effective group identity of the process.
(See setegid(2).) The `id` can be passed as either a numeric ID or a group
name string. If a group name is specified, this method blocks while resolving
the associated a numeric ID.

```js
if (process.getegid && process.setegid) {
  console.log(`Current gid: ${process.getegid()}`);
  try {
    process.setegid(501);
    console.log(`New gid: ${process.getegid()}`);
  }
  catch (err) {
    console.log(`Failed to set gid: ${err}`);
  }
}
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)


## process.seteuid(id)
<!-- YAML
added: v2.0.0
-->

* `id` {String|number} A user name or ID

The `process.seteuid()` method sets the effective user identity of the process.
(See seteuid(2).) The `id` can be passed as either a numeric ID or a username
string.  If a username is specified, the method blocks while resolving the
associated numeric ID.

```js
if (process.geteuid && process.seteuid) {
  console.log(`Current uid: ${process.geteuid()}`);
  try {
    process.seteuid(501);
    console.log(`New uid: ${process.geteuid()}`);
  }
  catch (err) {
    console.log(`Failed to set uid: ${err}`);
  }
}
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)

## process.setgid(id)
<!-- YAML
added: v0.1.31
-->

* `id` {String|number} The group name or ID

The `process.setgid()` method sets the group identity of the process. (See
setgid(2).)  The `id` can be passed as either a numeric ID or a group name
string. If a group name is specified, this method blocks while resolving the
associated numeric ID.

```js
if (process.getgid && process.setgid) {
  console.log(`Current gid: ${process.getgid()}`);
  try {
    process.setgid(501);
    console.log(`New gid: ${process.getgid()}`);
  }
  catch (err) {
    console.log(`Failed to set gid: ${err}`);
  }
}
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)

## process.setgroups(groups)
<!-- YAML
added: v0.9.4
-->

* `groups` {Array}

The `process.setgroups()` method sets the supplementary group IDs for the
Node.js process. This is a privileged operation that requires the Node.js process
to have `root` or the `CAP_SETGID` capability.

The `groups` array can contain numeric group IDs, group names or both.

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)

## process.setuid(id)
<!-- YAML
added: v0.1.28
-->

The `process.setuid(id)` method sets the user identity of the process. (See
setuid(2).)  The `id` can be passed as either a numeric ID or a username string.
If a username is specified, the method blocks while resolving the associated
numeric ID.

```js
if (process.getuid && process.setuid) {
  console.log(`Current uid: ${process.getuid()}`);
  try {
    process.setuid(501);
    console.log(`New uid: ${process.getuid()}`);
  }
  catch (err) {
    console.log(`Failed to set uid: ${err}`);
  }
}
```

*Note*: This function is only available on POSIX platforms (i.e. not Windows or
Android)


## process.stderr

* {Stream}

The `process.stderr` property returns a [Writable][] stream equivalent to or
associated with `stderr` (fd `2`).

Note: `process.stderr` and `process.stdout` differ from other Node.js streams
in several ways:
1. They cannot be closed ([`end()`][] will throw).
2. They never emit the [`'finish'`][] event.
3. Writes _can_ block when output is redirected to a file.
  - Note that disks are fast and operating systems normally employ write-back
    caching so this is very uncommon.
4. Writes on UNIX **will** block by default if output is going to a TTY
   (a terminal).
5. Windows functionality differs. Writes block except when output is going to a
   TTY.

To check if Node.js is being run in a TTY context, read the `isTTY` property
on `process.stderr`, `process.stdout`, or `process.stdin`:

## process.stdin

* {Stream}

The `process.stdin` property returns a [Readable][] stream equivalent to or
associated with `stdin` (fd `0`).

For example:

```js
process.stdin.setEncoding('utf8');

process.stdin.on('readable', () => {
  var chunk = process.stdin.read();
  if (chunk !== null) {
    process.stdout.write(`data: ${chunk}`);
  }
});

process.stdin.on('end', () => {
  process.stdout.write('end');
});
```

As a [Readable][] stream, `process.stdin` can also be used in "old" mode that
is compatible with scripts written for Node.js prior to v0.10.
For more information see [Stream compatibility][].

*Note*: In "old" streams mode the `stdin` stream is paused by default, so one
must call `process.stdin.resume()` to read from it. Note also that calling
`process.stdin.resume()` itself would switch stream to "old" mode.

## process.stdout

* {Stream}

The `process.stdout` property returns a [Writable][] stream equivalent to or
associated with `stdout` (fd `1`).

For example:

```js
console.log = (msg) => {
  process.stdout.write(`${msg}\n`);
};
```

Note: `process.stderr` and `process.stdout` differ from other Node.js streams
in several ways:
1. They cannot be closed ([`end()`][] will throw).
2. They never emit the [`'finish'`][] event.
3. Writes _can_ block when output is redirected to a file.
  - Note that disks are fast and operating systems normally employ write-back
    caching so this is very uncommon.
4. Writes on UNIX **will** block by default if output is going to a TTY
   (a terminal).
5. Windows functionality differs. Writes block except when output is going to a
   TTY.

To check if Node.js is being run in a TTY context, read the `isTTY` property
on `process.stderr`, `process.stdout`, or `process.stdin`:

### TTY Terminals and `process.stdout`

The `process.stderr` and `process.stdout` streams are blocking when outputting
to TTYs (terminals) on OS X as a workaround for the operating system's small,
1kb buffer size. This is to prevent interleaving between `stdout` and `stderr`.

To check if Node.js is being run in a [TTY][] context, check the `isTTY`
property on `process.stderr`, `process.stdout`, or `process.stdin`.

For instance:
```console
$ node -p "Boolean(process.stdin.isTTY)"
true
$ echo "foo" | node -p "Boolean(process.stdin.isTTY)"
false

$ node -p "Boolean(process.stdout.isTTY)"
true
$ node -p "Boolean(process.stdout.isTTY)" | cat
false
```

See the [TTY][] documentation for more information.

## process.title
<!-- YAML
added: v0.1.104
-->

* {String}

The `process.title` property returns the current process title (i.e. returns
the current value of `ps`). Assigning a new value to `process.title` modifies
the current value of `ps`.

*Note*: When a new value is assigned, different platforms will impose different
maximum length restrictions on the title. Usually such restrictions are quite
limited. For instance, on Linux and OS X, `process.title` is limited to the size
of the binary name plus the length of the command line arguments because setting
the `process.title` overwrites the `argv` memory of the process. Node.js v0.8
allowed for longer process title strings by also overwriting the `environ`
memory but that was potentially insecure and confusing in some (rather obscure)
cases.

## process.umask([mask])
<!-- YAML
added: v0.1.19
-->

* `mask` {number}

The `process.umask()` method sets or returns the Node.js process's file mode
creation mask. Child processes inherit the mask from the parent process. The old
mask is return if the `mask` argument is given, otherwise returns the current
mask.

```js
const newmask = 0o022;
const oldmask = process.umask(newmask);
console.log(
  `Changed umask from ${oldmask.toString(8)} to ${newmask.toString(8)}`
);
```


## process.uptime()
<!-- YAML
added: v0.5.0
-->

* Returns: {Number}

The `process.uptime()` method returns the number of seconds the current Node.js
process has been running.

## process.version
<!-- YAML
added: v0.1.3
-->

* {String}

The `process.version` property returns the Node.js version string.

```js
console.log(`Version: ${process.version}`);
```

## process.versions
<!-- YAML
added: v0.2.0
-->

* {Object}

The `process.versions` property returns an object listing the version strings of
Node.js and its dependencies. `process.versions.modules` indicates the current
ABI version, which is increased whenever a C++ API changes. Node.js will refuse
to load modules that were compiled against a different module ABI version.

```js
console.log(process.versions);
```

Will generate output similar to:

```js
{
  http_parser: '2.3.0',
  node: '1.1.1',
  v8: '4.1.0.14',
  uv: '1.3.0',
  zlib: '1.2.8',
  ares: '1.10.0-DEV',
  modules: '43',
  icu: '55.1',
  openssl: '1.0.1k',
  unicode: '8.0',
  cldr: '29.0',
  tz: '2016b' }
```

## Exit Codes

Node.js will normally exit with a `0` status code when no more async
operations are pending.  The following status codes are used in other
cases:

* `1` **Uncaught Fatal Exception** - There was an uncaught exception,
  and it was not handled by a domain or an [`'uncaughtException'`][] event
  handler.
* `2` - Unused (reserved by Bash for builtin misuse)
* `3` **Internal JavaScript Parse Error** - The JavaScript source code
  internal in Node.js's bootstrapping process caused a parse error.  This
  is extremely rare, and generally can only happen during development
  of Node.js itself.
* `4` **Internal JavaScript Evaluation Failure** - The JavaScript
  source code internal in Node.js's bootstrapping process failed to
  return a function value when evaluated.  This is extremely rare, and
  generally can only happen during development of Node.js itself.
* `5` **Fatal Error** - There was a fatal unrecoverable error in V8.
  Typically a message will be printed to stderr with the prefix `FATAL
  ERROR`.
* `6` **Non-function Internal Exception Handler** - There was an
  uncaught exception, but the internal fatal exception handler
  function was somehow set to a non-function, and could not be called.
* `7` **Internal Exception Handler Run-Time Failure** - There was an
  uncaught exception, and the internal fatal exception handler
  function itself threw an error while attempting to handle it.  This
  can happen, for example, if a [`'uncaughtException'`][] or
  `domain.on('error')` handler throws an error.
* `8` - Unused.  In previous versions of Node.js, exit code 8 sometimes
  indicated an uncaught exception.
* `9` - **Invalid Argument** - Either an unknown option was specified,
  or an option requiring a value was provided without a value.
* `10` **Internal JavaScript Run-Time Failure** - The JavaScript
  source code internal in Node.js's bootstrapping process threw an error
  when the bootstrapping function was called.  This is extremely rare,
  and generally can only happen during development of Node.js itself.
* `12` **Invalid Debug Argument** - The `--debug`, `--inspect` and/or
  `--debug-brk` options were set, but the port number chosen was invalid
  or unavailable.
* `>128` **Signal Exits** - If Node.js receives a fatal signal such as
  `SIGKILL` or `SIGHUP`, then its exit code will be `128` plus the
  value of the signal code.  This is a standard Unix practice, since
  exit codes are defined to be 7-bit integers, and signal exits set
  the high-order bit, and then contain the value of the signal code.


[`'finish'`]: stream.html#stream_event_finish
[`'message'`]: child_process.html#child_process_event_message
[`'rejectionHandled'`]: #process_event_rejectionhandled
[`'uncaughtException'`]: #process_event_uncaughtexception
[`ChildProcess.disconnect()`]: child_process.html#child_process_child_disconnect
[`ChildProcess.kill()`]: child_process.html#child_process_child_kill_signal
[`ChildProcess.send()`]: child_process.html#child_process_child_send_message_sendhandle_options_callback
[`ChildProcess`]: child_process.html#child_process_class_childprocess
[`end()`]: stream.html#stream_writable_end_chunk_encoding_callback
[`Error`]: errors.html#errors_class_error
[`EventEmitter`]: events.html#events_class_eventemitter
[`JSON.stringify()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify
[`net.Server`]: net.html#net_class_net_server
[`net.Socket`]: net.html#net_class_net_socket
[`process.argv`]: #process_process_argv
[`process.exit()`]: #process_process_exit_code
[`process.kill()`]: #process_process_kill_pid_signal
[`process.execPath`]: #process_process_execpath
[`promise.catch()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/catch
[`require.main`]: modules.html#modules_accessing_the_main_module
[`setTimeout(fn, 0)`]: timers.html#timers_settimeout_callback_delay_args
[process_emit_warning]: #process_process_emitwarning_warning_name_ctor
[process_warning]: #process_event_warning
[Signal Events]: #process_signal_events
[Stream compatibility]: stream.html#stream_compatibility_with_older_node_js_versions
[TTY]: tty.html#tty_tty
[Writable]: stream.html
[Readable]: stream.html
[Child Process]: child_process.html
[Cluster]: cluster.html
[`process.exitCode`]: #process_process_exitcode
[LTS]: https://github.com/nodejs/LTS/
