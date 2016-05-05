# process

<!-- type=global -->

The `process` object is a global object and can be accessed from anywhere.
It is an instance of [`EventEmitter`][].

## Event: 'beforeExit'
<!-- YAML
added: v0.11.12
-->

This event is emitted when Node.js empties its event loop and has nothing else
to schedule. Normally, Node.js exits when there is no work scheduled, but a
listener for `'beforeExit'` can make asynchronous calls, and cause Node.js to
continue.

`'beforeExit'` is not emitted for conditions causing explicit termination, such
as [`process.exit()`][] or uncaught exceptions, and should not be used as an
alternative to the `'exit'` event unless the intention is to schedule more work.

## Event: 'exit'
<!-- YAML
added: v0.1.7
-->

Emitted when the process is about to exit. There is no way to prevent the
exiting of the event loop at this point, and once all `'exit'` listeners have
finished running the process will exit. Therefore you **must** only perform
**synchronous** operations in this handler. This is a good hook to perform
checks on the module's state (like for unit tests). The callback takes one
argument, the code the process is exiting with.

This event is only emitted when Node.js exits explicitly by process.exit() or
implicitly by the event loop draining.

Example of listening for `'exit'`:

```js
process.on('exit', (code) => {
  // do *NOT* do this
  setTimeout(() => {
    console.log('This will not run');
  }, 0);
  console.log('About to exit with code:', code);
});
```

## Event: 'message'
<!-- YAML
added: v0.5.10
-->

* `message` {Object} a parsed JSON object or primitive value
* `sendHandle` {Handle object} a [`net.Socket`][] or [`net.Server`][] object, or
  undefined.

Messages sent by [`ChildProcess.send()`][] are obtained using the `'message'`
event on the child's process object.

## Event: 'rejectionHandled'
<!-- YAML
added: v1.4.1
-->

Emitted whenever a Promise was rejected and an error handler was attached to it
(for example with `.catch()`) later than after an event loop turn. This event
is emitted with the following arguments:

 - `p` the promise that was previously emitted in an `'unhandledRejection'`
 event, but which has now gained a rejection handler.

There is no notion of a top level for a promise chain at which rejections can
always be handled. Being inherently asynchronous in nature, a promise rejection
can be handled at a future point in time — possibly much later than the
event loop turn it takes for the `'unhandledRejection'` event to be emitted.

Another way of stating this is that, unlike in synchronous code where there is
an ever-growing list of unhandled exceptions, with promises there is a
growing-and-shrinking list of unhandled rejections. In synchronous code, the
`'uncaughtException'` event tells you when the list of unhandled exceptions
grows. And in asynchronous code, the `'unhandledRejection'` event tells you
when the list of unhandled rejections grows, while the `'rejectionHandled'`
event tells you when the list of unhandled rejections shrinks.

For example using the rejection detection hooks in order to keep a map of all
the rejected promise reasons at a given time:

```js
const unhandledRejections = new Map();
process.on('unhandledRejection', (reason, p) => {
  unhandledRejections.set(p, reason);
});
process.on('rejectionHandled', (p) => {
  unhandledRejections.delete(p);
});
```

This map will grow and shrink over time, reflecting rejections that start
unhandled and then become handled. You could record the errors in some error
log, either periodically (probably best for long-running programs, allowing
you to clear the map, which in the case of a very buggy program could grow
indefinitely) or upon process exit (more convenient for scripts).

## Event: 'uncaughtException'
<!-- YAML
added: v0.1.18
-->

The `'uncaughtException'` event is emitted when an exception bubbles all the
way back to the event loop. By default, Node.js handles such exceptions by
printing the stack trace to stderr and exiting. Adding a handler for the
`'uncaughtException'` event overrides this default behavior.

For example:

```js
process.on('uncaughtException', (err) => {
  console.log(`Caught exception: ${err}`);
});

setTimeout(() => {
  console.log('This will still run.');
}, 500);

// Intentionally cause an exception, but don't catch it.
nonexistentFunc();
console.log('This will not run.');
```

### Warning: Using `'uncaughtException'` correctly

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
down the process. It is not safe to resume normal operation after
`'uncaughtException'`.

## Event: 'unhandledRejection'
<!-- YAML
added: v1.4.1
-->

Emitted whenever a `Promise` is rejected and no error handler is attached to
the promise within a turn of the event loop. When programming with promises
exceptions are encapsulated as rejected promises. Such promises can be caught
and handled using [`promise.catch(...)`][] and rejections are propagated through
a promise chain. This event is useful for detecting and keeping track of
promises that were rejected whose rejections were not handled yet. This event
is emitted with the following arguments:

 - `reason` the object with which the promise was rejected (usually an
   [`Error`][] instance).
 - `p` the promise that was rejected.

Here is an example that logs every unhandled rejection to the console

```js
process.on('unhandledRejection', (reason, p) => {
    console.log("Unhandled Rejection at: Promise ", p, " reason: ", reason);
    // application specific logging, throwing an error, or other logic here
});
```

For example, here is a rejection that will trigger the `'unhandledRejection'`
event:

```js
somePromise.then((res) => {
  return reportToUser(JSON.pasre(res)); // note the typo (`pasre`)
}); // no `.catch` or `.then`
```

Here is an example of a coding pattern that will also trigger
`'unhandledRejection'`:

```js
function SomeResource() {
  // Initially set the loaded status to a rejected promise
  this.loaded = Promise.reject(new Error('Resource not yet loaded!'));
}

var resource = new SomeResource();
// no .catch or .then on resource.loaded for at least a turn
```

In cases like this, you may not want to track the rejection as a developer
error like you would for other `'unhandledRejection'` events. To address
this, you can either attach a dummy `.catch(() => { })` handler to
`resource.loaded`, preventing the `'unhandledRejection'` event from being
emitted, or you can use the [`'rejectionHandled'`][] event.

## Exit Codes

Node.js will normally exit with a `0` status code when no more async
operations are pending.  The following status codes are used in other
cases:

* `1` **Uncaught Fatal Exception** - There was an uncaught exception,
  and it was not handled by a domain or an `'uncaughtException'` event
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
  can happen, for example, if a `process.on('uncaughtException')` or
  `domain.on('error')` handler throws an error.
* `8` - Unused.  In previous versions of Node.js, exit code 8 sometimes
  indicated an uncaught exception.
* `9` - **Invalid Argument** - Either an unknown option was specified,
  or an option requiring a value was provided without a value.
* `10` **Internal JavaScript Run-Time Failure** - The JavaScript
  source code internal in Node.js's bootstrapping process threw an error
  when the bootstrapping function was called.  This is extremely rare,
  and generally can only happen during development of Node.js itself.
* `12` **Invalid Debug Argument** - The `--debug` and/or `--debug-brk`
  options were set, but an invalid port number was chosen.
* `>128` **Signal Exits** - If Node.js receives a fatal signal such as
  `SIGKILL` or `SIGHUP`, then its exit code will be `128` plus the
  value of the signal code.  This is a standard Unix practice, since
  exit codes are defined to be 7-bit integers, and signal exits set
  the high-order bit, and then contain the value of the signal code.

## Signal Events

<!--type=event-->
<!--name=SIGINT, SIGHUP, etc.-->

Emitted when the processes receives a signal. See sigaction(2) for a list of
standard POSIX signal names such as `SIGINT`, `SIGHUP`, etc.

Example of listening for `SIGINT`:

```js
// Start reading from stdin so we don't exit.
process.stdin.resume();

process.on('SIGINT', () => {
  console.log('Got SIGINT.  Press Control-D to exit.');
});
```

An easy way to send the `SIGINT` signal is with `Control-C` in most terminal
programs.

Note:

- `SIGUSR1` is reserved by Node.js to start the debugger.  It's possible to
  install a listener but that won't stop the debugger from starting.
- `SIGTERM` and `SIGINT` have default handlers on non-Windows platforms that
  resets the terminal mode before exiting with code `128 + signal number`. If
  one of these signals has a listener installed, its default behavior will be
  removed (Node.js will no longer exit).
- `SIGPIPE` is ignored by default. It can have a listener installed.
- `SIGHUP` is generated on Windows when the console window is closed, and on other
  platforms under various similar conditions, see signal(7). It can have a
  listener installed, however Node.js will be unconditionally terminated by
  Windows about 10 seconds later. On non-Windows platforms, the default
  behavior of `SIGHUP` is to terminate Node.js, but once a listener has been
  installed its default behavior will be removed.
- `SIGTERM` is not supported on Windows, it can be listened on.
- `SIGINT` from the terminal is supported on all platforms, and can usually be
  generated with `CTRL+C` (though this may be configurable). It is not generated
  when terminal raw mode is enabled.
- `SIGBREAK` is delivered on Windows when `CTRL+BREAK` is pressed, on
  non-Windows
  platforms it can be listened on, but there is no way to send or generate it.
- `SIGWINCH` is delivered when the console has been resized. On Windows, this
  will only happen on write to the console when the cursor is being moved, or
  when a readable tty is used in raw mode.
- `SIGKILL` cannot have a listener installed, it will unconditionally terminate
  Node.js on all platforms.
- `SIGSTOP` cannot have a listener installed.

Note that Windows does not support sending Signals, but Node.js offers some
emulation with `process.kill()`, and `child_process.kill()`. Sending signal `0`
can be used to test for the existence of a process. Sending `SIGINT`,
`SIGTERM`, and `SIGKILL` cause the unconditional termination of the target
process.

## process.abort()
<!-- YAML
added: v0.7.0
-->

This causes Node.js to emit an abort. This will cause Node.js to exit and
generate a core file.

## process.arch
<!-- YAML
added: v0.5.0
-->

What processor architecture you're running on: `'arm'`, `'ia32'`, or `'x64'`.

```js
console.log('This processor architecture is ' + process.arch);
```

## process.argv
<!-- YAML
added: v0.1.27
-->

An array containing the command line arguments.  The first element will be
'node', the second element will be the name of the JavaScript file.  The
next elements will be any additional command line arguments.

```js
// print process.argv
process.argv.forEach((val, index, array) => {
  console.log(`${index}: ${val}`);
});
```

This will generate:

```
$ node process-2.js one two=three four
0: node
1: /Users/mjr/work/node/process-2.js
2: one
3: two=three
4: four
```

## process.chdir(directory)
<!-- YAML
added: v0.1.17
-->

Changes the current working directory of the process or throws an exception if that fails.

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

An Object containing the JavaScript representation of the configure options
that were used to compile the current Node.js executable. This is the same as
the `config.gypi` file that was produced when running the `./configure` script.

An example of the possible output looks like:

```
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

*Note: the `process.config` property is **not** read-only and there are existing
modules in the ecosystem that are known to extend, modify, or entirely replace
the value of `process.config`.*

## process.connected
<!-- YAML
added: v0.7.2
-->

* {Boolean} Set to false after `process.disconnect()` is called

If `process.connected` is false, it is no longer possible to send messages.

## process.cwd()
<!-- YAML
added: v0.1.8
-->

Returns the current working directory of the process.

```js
console.log(`Current directory: ${process.cwd()}`);
```

## process.disconnect()
<!-- YAML
added: v0.7.2
-->

Close the IPC channel to the parent process, allowing this child to exit
gracefully once there are no other connections keeping it alive.

Identical to the parent process's [`ChildProcess.disconnect()`][].

If Node.js was not spawned with an IPC channel, `process.disconnect()` will be
undefined.

## process.env
<!-- YAML
added: v0.1.27
-->

An object containing the user environment. See environ(7).

An example of this object looks like:

```js
{ TERM: 'xterm-256color',
  SHELL: '/usr/local/bin/bash',
  USER: 'maciej',
  PATH: '~/.bin/:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin',
  PWD: '/Users/maciej',
  EDITOR: 'vim',
  SHLVL: '1',
  HOME: '/Users/maciej',
  LOGNAME: 'maciej',
  _: '/usr/local/bin/node' }
```

You can write to this object, but changes won't be reflected outside of your
process. That means that the following won't work:

```
$ node -e 'process.env.foo = "bar"' && echo $foo
```

But this will:

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

## process.execArgv
<!-- YAML
added: v0.7.7
-->

This is the set of Node.js-specific command line options from the
executable that started the process.  These options do not show up in
`process.argv`, and do not include the Node.js executable, the name of
the script, or any options following the script name. These options
are useful in order to spawn child processes with the same execution
environment as the parent.

Example:

```
$ node --harmony script.js --version
```

results in process.execArgv:

```js
['--harmony']
```

and process.argv:

```js
['/usr/local/bin/node', 'script.js', '--version']
```

## process.execPath
<!-- YAML
added: v0.1.100
-->

This is the absolute pathname of the executable that started the process.

Example:

```
/usr/local/bin/node
```


## process.exit([code])
<!-- YAML
added: v0.1.13
-->

Ends the process with the specified `code`.  If omitted, exit uses the
'success' code `0`.

To exit with a 'failure' code:

```js
process.exit(1);
```

The shell that executed Node.js should see the exit code as 1.


## process.exitCode
<!-- YAML
added: v0.11.8
-->

A number which will be the process exit code, when the process either
exits gracefully, or is exited via [`process.exit()`][] without specifying
a code.

Specifying a code to `process.exit(code)` will override any previous
setting of `process.exitCode`.


## process.getegid()
<!-- YAML
added: v2.0.0
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the effective group identity of the process. (See getegid(2).)
This is the numerical group id, not the group name.

```js
if (process.getegid) {
  console.log(`Current gid: ${process.getegid()}`);
}
```


## process.geteuid()
<!-- YAML
added: v2.0.0
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the effective user identity of the process. (See geteuid(2).)
This is the numerical userid, not the username.

```js
if (process.geteuid) {
  console.log(`Current uid: ${process.geteuid()}`);
}
```

## process.getgid()
<!-- YAML
added: v0.1.31
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the group identity of the process. (See getgid(2).)
This is the numerical group id, not the group name.

```js
if (process.getgid) {
  console.log(`Current gid: ${process.getgid()}`);
}
```

## process.getgroups()
<!-- YAML
added: v0.9.4
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Returns an array with the supplementary group IDs. POSIX leaves it unspecified
if the effective group ID is included but Node.js ensures it always is.

## process.getuid()
<!-- YAML
added: v0.1.28
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the user identity of the process. (See getuid(2).)
This is the numerical userid, not the username.

```js
if (process.getuid) {
  console.log(`Current uid: ${process.getuid()}`);
}
```

## process.hrtime()
<!-- YAML
added: v0.7.6
-->

Returns the current high-resolution real time in a `[seconds, nanoseconds]`
tuple Array. It is relative to an arbitrary time in the past. It is not
related to the time of day and therefore not subject to clock drift. The
primary use is for measuring performance between intervals.

You may pass in the result of a previous call to `process.hrtime()` to get
a diff reading, useful for benchmarks and measuring intervals:

```js
var time = process.hrtime();
// [ 1800216, 25 ]

setTimeout(() => {
  var diff = process.hrtime(time);
  // [ 1, 552 ]

  console.log('benchmark took %d nanoseconds', diff[0] * 1e9 + diff[1]);
  // benchmark took 1000000527 nanoseconds
}, 1000);
```


## process.initgroups(user, extra_group)
<!-- YAML
added: v0.9.4
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Reads /etc/group and initializes the group access list, using all groups of
which the user is a member. This is a privileged operation, meaning you need
to be root or have the `CAP_SETGID` capability.

`user` is a user name or user ID. `extra_group` is a group name or group ID.

Some care needs to be taken when dropping privileges. Example:

```js
console.log(process.getgroups());         // [ 0 ]
process.initgroups('bnoordhuis', 1000);   // switch user
console.log(process.getgroups());         // [ 27, 30, 46, 1000, 0 ]
process.setgid(1000);                     // drop root gid
console.log(process.getgroups());         // [ 27, 30, 46, 1000 ]
```

## process.kill(pid[, signal])
<!-- YAML
added: v0.0.6
-->

Send a signal to a process. `pid` is the process id and `signal` is the
string describing the signal to send.  Signal names are strings like
`SIGINT` or `SIGHUP`.  If omitted, the signal will be `SIGTERM`.
See [Signal Events][] and kill(2) for more information.

Will throw an error if target does not exist, and as a special case, a signal
of `0` can be used to test for the existence of a process. Windows platforms
will throw an error if the `pid` is used to kill a process group.

Note that even though the name of this function is `process.kill`, it is really
just a signal sender, like the `kill` system call.  The signal sent may do
something other than kill the target process.

Example of sending a signal to yourself:

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

Note: When SIGUSR1 is received by Node.js it starts the debugger, see
[Signal Events][].

## process.mainModule
<!-- YAML
added: v0.1.17
-->

Alternate way to retrieve [`require.main`][]. The difference is that if the main
module changes at runtime, `require.main` might still refer to the original main
module in modules that were required before the change occurred. Generally it's
safe to assume that the two refer to the same module.

As with `require.main`, it will be `undefined` if there was no entry script.

## process.memoryUsage()
<!-- YAML
added: v0.1.16
-->

Returns an object describing the memory usage of the Node.js process
measured in bytes.

```js
const util = require('util');

console.log(util.inspect(process.memoryUsage()));
```

This will generate:

```js
{ rss: 4935680,
  heapTotal: 1826816,
  heapUsed: 650472 }
```

`heapTotal` and `heapUsed` refer to V8's memory usage.


## process.nextTick(callback[, arg][, ...])
<!-- YAML
added: v0.1.26
-->

* `callback` {Function}

Once the current event loop turn runs to completion, call the callback
function.

This is *not* a simple alias to [`setTimeout(fn, 0)`][], it's much more
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

This is important in developing APIs where you want to give the user the
chance to assign event handlers after an object has been constructed,
but before any I/O has occurred.

```js
function MyThing(options) {
  this.setupOptions(options);

  process.nextTick(() => {
    this.startDoingStuff();
  });
}
```

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

This API is hazardous.  If you do this:

```js
maybeSync(true, () => {
  foo();
});
bar();
```

then it's not clear whether `foo()` or `bar()` will be called first.

This approach is much better:

```js
function definitelyAsync(arg, cb) {
  if (arg) {
    process.nextTick(cb);
    return;
  }

  fs.stat('file', cb);
}
```

Note: the nextTick queue is completely drained on each pass of the
event loop **before** additional I/O is processed.  As a result,
recursively setting nextTick callbacks will block any I/O from
happening, just like a `while(true);` loop.

## process.pid
<!-- YAML
added: v0.1.15
-->

The PID of the process.

```js
console.log(`This process is pid ${process.pid}`);
```

## process.platform
<!-- YAML
added: v0.1.16
-->

What platform you're running on:
`'darwin'`, `'freebsd'`, `'linux'`, `'sunos'` or `'win32'`

```js
console.log(`This platform is ${process.platform}`);
```

## process.release
<!-- YAML
added: v3.0.0
-->

An Object containing metadata related to the current release, including URLs
for the source tarball and headers-only tarball.

`process.release` contains the following properties:

* `name`: a string with a value that will always be `'node'` for Node.js. For
  legacy io.js releases, this will be `'io.js'`.
* `sourceUrl`: a complete URL pointing to a _.tar.gz_ file containing the
  source of the current release.
* `headersUrl`: a complete URL pointing to a _.tar.gz_ file containing only
  the header files for the current release. This file is significantly smaller
  than the full source file and can be used for compiling add-ons against
  Node.js.
* `libUrl`: a complete URL pointing to an _node.lib_ file matching the
  architecture and version of the current release. This file is used for
  compiling add-ons against Node.js. _This property is only present on Windows
  builds of Node.js and will be missing on all other platforms._

e.g.

```js
{ name: 'node',
  sourceUrl: 'https://nodejs.org/download/release/v4.0.0/node-v4.0.0.tar.gz',
  headersUrl: 'https://nodejs.org/download/release/v4.0.0/node-v4.0.0-headers.tar.gz',
  libUrl: 'https://nodejs.org/download/release/v4.0.0/win-x64/node.lib' }
```

In custom builds from non-release versions of the source tree, only the
`name` property may be present. The additional properties should not be
relied upon to exist.

## process.send(message[, sendHandle][, callback])
<!-- YAML
added: v0.5.9
-->

* `message` {Object}
* `sendHandle` {Handle object}
* `callback` {Function}
* Return: {Boolean}

When Node.js is spawned with an IPC channel attached, it can send messages to its
parent process using `process.send()`. Each will be received as a
[`'message'`][] event on the parent's `ChildProcess` object.

*Note: this function uses [`JSON.stringify()`][] internally to serialize the `message`.*

If Node.js was not spawned with an IPC channel, `process.send()` will be undefined.

## process.setegid(id)
<!-- YAML
added: v2.0.0
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the effective group identity of the process. (See setegid(2).)
This accepts either a numerical ID or a groupname string. If a groupname
is specified, this method blocks while resolving it to a numerical ID.

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

## process.seteuid(id)
<!-- YAML
added: v2.0.0
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the effective user identity of the process. (See seteuid(2).)
This accepts either a numerical ID or a username string.  If a username
is specified, this method blocks while resolving it to a numerical ID.

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

## process.setgid(id)
<!-- YAML
added: v0.1.31
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the group identity of the process. (See setgid(2).)  This accepts either
a numerical ID or a groupname string. If a groupname is specified, this method
blocks while resolving it to a numerical ID.

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

## process.setgroups(groups)
<!-- YAML
added: v0.9.4
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the supplementary group IDs. This is a privileged operation, meaning you
need to be root or have the `CAP_SETGID` capability.

The list can contain group IDs, group names or both.

## process.setuid(id)
<!-- YAML
added: v0.1.28
-->

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the user identity of the process. (See setuid(2).)  This accepts either
a numerical ID or a username string.  If a username is specified, this method
blocks while resolving it to a numerical ID.

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

## process.stderr

A writable stream to stderr (on fd `2`).

`process.stderr` and `process.stdout` are unlike other streams in Node.js in
that they cannot be closed (`end()` will throw), they never emit the `finish`
event and that writes can block when output is redirected to a file (although
disks are fast and operating systems normally employ write-back caching so it
should be a very rare occurrence indeed.)

Additionally, `process.stderr` and `process.stdout` are blocking when outputting
to TTYs (terminals) on OS X as a workaround for the OS's very small, 1kb
buffer size. This is to prevent interleaving between `stdout` and `stderr`.

## process.stdin

A `Readable Stream` for stdin (on fd `0`).

Example of opening standard input and listening for both events:

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

As a Stream, `process.stdin` can also be used in "old" mode that is compatible
with scripts written for node.js prior to v0.10.
For more information see [Stream compatibility][].

In "old" Streams mode the stdin stream is paused by default, so one
must call `process.stdin.resume()` to read from it. Note also that calling
`process.stdin.resume()` itself would switch stream to "old" mode.

If you are starting a new project you should prefer a more recent "new" Streams
mode over "old" one.

## process.stdout

A `Writable Stream` to `stdout` (on fd `1`).

For example, a `console.log` equivalent could look like this:

```js
console.log = (msg) => {
  process.stdout.write(`${msg}\n`);
};
```

`process.stderr` and `process.stdout` are unlike other streams in Node.js in
that they cannot be closed (`end()` will throw), they never emit the `'finish'`
event and that writes can block when output is redirected to a file (although
disks are fast and operating systems normally employ write-back caching so it
should be a very rare occurrence indeed.)

Additionally, `process.stderr` and `process.stdout` are blocking when outputting
to TTYs (terminals) on OS X as a workaround for the OS's very small, 1kb
buffer size. This is to prevent interleaving between `stdout` and `stderr`.

To check if Node.js is being run in a TTY context, read the `isTTY` property
on `process.stderr`, `process.stdout`, or `process.stdin`:

```
$ node -p "Boolean(process.stdin.isTTY)"
true
$ echo "foo" | node -p "Boolean(process.stdin.isTTY)"
false

$ node -p "Boolean(process.stdout.isTTY)"
true
$ node -p "Boolean(process.stdout.isTTY)" | cat
false
```

See [the tty docs][] for more information.

## process.title
<!-- YAML
added: v0.1.104
-->

Getter/setter to set what is displayed in `ps`.

When used as a setter, the maximum length is platform-specific and probably
short.

On Linux and OS X, it's limited to the size of the binary name plus the
length of the command line arguments because it overwrites the argv memory.

v0.8 allowed for longer process title strings by also overwriting the environ
memory but that was potentially insecure/confusing in some (rather obscure)
cases.

## process.umask([mask])
<!-- YAML
added: v0.1.19
-->

Sets or reads the process's file mode creation mask. Child processes inherit
the mask from the parent process. Returns the old mask if `mask` argument is
given, otherwise returns the current mask.

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

Number of seconds Node.js has been running.

## process.version
<!-- YAML
added: v0.1.3
-->

A compiled-in property that exposes `NODE_VERSION`.

```js
console.log(`Version: ${process.version}`);
```

## process.versions
<!-- YAML
added: v0.2.0
-->

A property exposing version strings of Node.js and its dependencies.

```js
console.log(process.versions);
```

Will print something like:

```js
{ http_parser: '2.3.0',
  node: '1.1.1',
  v8: '4.1.0.14',
  uv: '1.3.0',
  zlib: '1.2.8',
  ares: '1.10.0-DEV',
  modules: '43',
  icu: '55.1',
  openssl: '1.0.1k' }
```

[`'message'`]: child_process.html#child_process_event_message
[`ChildProcess.disconnect()`]: child_process.html#child_process_child_disconnect
[`ChildProcess.send()`]: child_process.html#child_process_child_send_message_sendhandle_callback
[`Error`]: errors.html#errors_class_error
[`EventEmitter`]: events.html#events_class_events_eventemitter
[`net.Server`]: net.html#net_class_net_server
[`net.Socket`]: net.html#net_class_net_socket
[`process.exit()`]: #process_process_exit_code
[`promise.catch(...)`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/catch
[`'rejectionHandled'`]: #process_event_rejectionhandled
[`require.main`]: modules.html#modules_accessing_the_main_module
[`setTimeout(fn, 0)`]: timers.html#timers_settimeout_callback_delay_arg
[Signal Events]: #process_signal_events
[Stream compatibility]: stream.html#stream_compatibility_with_older_node_js_versions
[the tty docs]: tty.html#tty_tty
[`JSON.stringify()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify
