# process

<!-- type=global -->

The `process` object is a global object and can be accessed from anywhere.
It is an instance of [EventEmitter][].

## Exit Codes

Node.js will normally exit with a `0` status code when no more async
operations are pending.  The following status codes are used in other
cases:

* `1` **Uncaught Fatal Exception** - There was an uncaught exception,
  and it was not handled by a domain or an `uncaughtException` event
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

## Event: 'exit'

Emitted when the process is about to exit. There is no way to prevent the
exiting of the event loop at this point, and once all `exit` listeners have
finished running the process will exit. Therefore you **must** only perform
**synchronous** operations in this handler. This is a good hook to perform
checks on the module's state (like for unit tests). The callback takes one
argument, the code the process is exiting with.

This event is only emitted when node exits explicitly by process.exit() or
implicitly by the event loop draining.

Example of listening for `exit`:

    process.on('exit', function(code) {
      // do *NOT* do this
      setTimeout(function() {
        console.log('This will not run');
      }, 0);
      console.log('About to exit with code:', code);
    });


## Event: 'message'

* `message` {Object} a parsed JSON object or primitive value
* `sendHandle` {Handle object} a [net.Socket][] or [net.Server][] object, or
  undefined.

Messages sent by [ChildProcess.send()][] are obtained using the `'message'`
event on the child's process object.


## Event: 'beforeExit'

This event is emitted when Node.js empties its event loop and has nothing else to
schedule. Normally, Node.js exits when there is no work scheduled, but a listener
for 'beforeExit' can make asynchronous calls, and cause Node.js to continue.

'beforeExit' is not emitted for conditions causing explicit termination, such as
`process.exit()` or uncaught exceptions, and should not be used as an
alternative to the 'exit' event unless the intention is to schedule more work.


## Event: 'uncaughtException'

Emitted when an exception bubbles all the way back to the event loop. If a
listener is added for this exception, the default action (which is to print
a stack trace and exit) will not occur.

Example of listening for `uncaughtException`:

    process.on('uncaughtException', function(err) {
      console.log('Caught exception: ' + err);
    });

    setTimeout(function() {
      console.log('This will still run.');
    }, 500);

    // Intentionally cause an exception, but don't catch it.
    nonexistentFunc();
    console.log('This will not run.');

Note that `uncaughtException` is a very crude mechanism for exception
handling.

Do *not* use it as the Node.js equivalent of `On Error Resume Next`. An
unhandled exception means your application - and by extension Node.js itself -
is in an undefined state. Blindly resuming means *anything* could happen.

Think of resuming as pulling the power cord when you are upgrading your system.
Nine out of ten times nothing happens - but the 10th time, your system is bust.

`uncaughtException` should be used to perform synchronous cleanup before
shutting down the process. It is not safe to resume normal operation after
`uncaughtException`. If you do use it, restart your application after every
unhandled exception!

You have been warned.

## Event: 'unhandledRejection'

Emitted whenever a `Promise` is rejected and no error handler is attached to
the promise within a turn of the event loop. When programming with promises
exceptions are encapsulated as rejected promises. Such promises can be caught
and handled using `promise.catch(...)` and rejections are propagated through
a promise chain. This event is useful for detecting and keeping track of
promises that were rejected whose rejections were not handled yet. This event
is emitted with the following arguments:

 - `reason` the object with which the promise was rejected (usually an `Error`
instance).
 - `p` the promise that was rejected.

Here is an example that logs every unhandled rejection to the console

    process.on('unhandledRejection', function(reason, p) {
        console.log("Unhandled Rejection at: Promise ", p, " reason: ", reason);
        // application specific logging, throwing an error, or other logic here
    });

For example, here is a rejection that will trigger the `'unhandledRejection'`
event:

    somePromise.then(function(res) {
      return reportToUser(JSON.pasre(res)); // note the typo
    }); // no `.catch` or `.then`

## Event: 'rejectionHandled'

Emitted whenever a Promise was rejected and an error handler was attached to it
(for example with `.catch()`) later than after an event loop turn. This event
is emitted with the following arguments:

 - `p` the promise that was previously emitted in an 'unhandledRejection'
 event, but which has now gained a rejection handler.

There is no notion of a top level for a promise chain at which rejections can
always be handled. Being inherently asynchronous in nature, a promise rejection
can be be handled at a future point in time — possibly much later than the
event loop turn it takes for the 'unhandledRejection' event to be emitted.

Another way of stating this is that, unlike in synchronous code where there is
an ever-growing list of unhandled exceptions, with promises there is a
growing-and-shrinking list of unhandled rejections. In synchronous code, the
'uncaughtException' event tells you when the list of unhandled exceptions
grows. And in asynchronous code, the 'unhandledRejection' event tells you
when the list of unhandled rejections grows, while the 'rejectionHandled'
event tells you when the list of unhandled rejections shrinks.

For example using the rejection detection hooks in order to keep a list of all
the rejected promises at a given time:

    var unhandledRejections = [];
    process.on('unhandledRejection', function(reason, p) {
        unhandledRejections.push(p);
    });
    process.on('rejectionHandled', function(p) {
        var index = unhandledRejections.indexOf(p);
        unhandledRejections.splice(index, 1);
    });

## Signal Events

<!--type=event-->
<!--name=SIGINT, SIGHUP, etc.-->

Emitted when the processes receives a signal. See sigaction(2) for a list of
standard POSIX signal names such as SIGINT, SIGHUP, etc.

Example of listening for `SIGINT`:

    // Start reading from stdin so we don't exit.
    process.stdin.resume();

    process.on('SIGINT', function() {
      console.log('Got SIGINT.  Press Control-D to exit.');
    });

An easy way to send the `SIGINT` signal is with `Control-C` in most terminal
programs.

Note:

- `SIGUSR1` is reserved by Node.js to start the debugger.  It's possible to
  install a listener but that won't stop the debugger from starting.
- `SIGTERM` and `SIGINT` have default handlers on non-Windows platforms that resets
  the terminal mode before exiting with code `128 + signal number`. If one of
  these signals has a listener installed, its default behavior will be removed
  (Node.js will no longer exit).
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
- `SIGBREAK` is delivered on Windows when `CTRL+BREAK` is pressed, on non-Windows
  platforms it can be listened on, but there is no way to send or generate it.
- `SIGWINCH` is delivered when the console has been resized. On Windows, this will
  only happen on write to the console when the cursor is being moved, or when a
  readable tty is used in raw mode.
- `SIGKILL` cannot have a listener installed, it will unconditionally terminate
  Node.js on all platforms.
- `SIGSTOP` cannot have a listener installed.

Note that Windows does not support sending Signals, but Node.js offers some
emulation with `process.kill()`, and `child_process.kill()`. Sending signal `0`
can be used to test for the existence of a process. Sending `SIGINT`,
`SIGTERM`, and `SIGKILL` cause the unconditional termination of the target
process.

## process.stdout

A `Writable Stream` to `stdout` (on fd `1`).

For example, a `console.log` equivalent could look like this:

    console.log = function(msg) {
      process.stdout.write(msg + '\n');
    };

`process.stderr` and `process.stdout` are unlike other streams in Node.js in
that they cannot be closed (`end()` will throw), they never emit the `finish`
event and that writes are always blocking.

To check if Node.js is being run in a TTY context, read the `isTTY` property
on `process.stderr`, `process.stdout`, or `process.stdin`:

    $ node -p "Boolean(process.stdin.isTTY)"
    true
    $ echo "foo" | node -p "Boolean(process.stdin.isTTY)"
    false

    $ node -p "Boolean(process.stdout.isTTY)"
    true
    $ node -p "Boolean(process.stdout.isTTY)" | cat
    false

See [the tty docs](tty.html#tty_tty) for more information.

## process.stderr

A writable stream to stderr (on fd `2`).

`process.stderr` and `process.stdout` are unlike other streams in Node.js in
that they cannot be closed (`end()` will throw), they never emit the `finish`
event and that writes are usually blocking.

- They are blocking in the case that they refer to regular files or TTY file
  descriptors.
- In the case they refer to pipes:
  - They are blocking in Linux/Unix.
  - They are non-blocking like other streams in Windows.


## process.stdin

A `Readable Stream` for stdin (on fd `0`).

Example of opening standard input and listening for both events:

    process.stdin.setEncoding('utf8');

    process.stdin.on('readable', function() {
      var chunk = process.stdin.read();
      if (chunk !== null) {
        process.stdout.write('data: ' + chunk);
      }
    });

    process.stdin.on('end', function() {
      process.stdout.write('end');
    });

As a Stream, `process.stdin` can also be used in "old" mode that is compatible
with scripts written for node.js prior to v0.10.
For more information see
[Stream compatibility](stream.html#stream_compatibility_with_older_node_js_versions).

In "old" Streams mode the stdin stream is paused by default, so one
must call `process.stdin.resume()` to read from it. Note also that calling
`process.stdin.resume()` itself would switch stream to "old" mode.

If you are starting a new project you should prefer a more recent "new" Streams
mode over "old" one.

## process.argv

An array containing the command line arguments.  The first element will be
'node', the second element will be the name of the JavaScript file.  The
next elements will be any additional command line arguments.

    // print process.argv
    process.argv.forEach(function(val, index, array) {
      console.log(index + ': ' + val);
    });

This will generate:

    $ node process-2.js one two=three four
    0: node
    1: /Users/mjr/work/node/process-2.js
    2: one
    3: two=three
    4: four


## process.execPath

This is the absolute pathname of the executable that started the process.

Example:

    /usr/local/bin/node


## process.execArgv

This is the set of Node.js-specific command line options from the
executable that started the process.  These options do not show up in
`process.argv`, and do not include the Node.js executable, the name of
the script, or any options following the script name. These options
are useful in order to spawn child processes with the same execution
environment as the parent.

Example:

    $ node --harmony script.js --version

results in process.execArgv:

    ['--harmony']

and process.argv:

    ['/usr/local/bin/node', 'script.js', '--version']


## process.abort()

This causes Node.js to emit an abort. This will cause Node.js to exit and
generate a core file.

## process.chdir(directory)

Changes the current working directory of the process or throws an exception if that fails.

    console.log('Starting directory: ' + process.cwd());
    try {
      process.chdir('/tmp');
      console.log('New directory: ' + process.cwd());
    }
    catch (err) {
      console.log('chdir: ' + err);
    }



## process.cwd()

Returns the current working directory of the process.

    console.log('Current directory: ' + process.cwd());


## process.env

An object containing the user environment. See environ(7).

An example of this object looks like:

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

You can write to this object, but changes won't be reflected outside of your
process. That means that the following won't work:

    $ node -e 'process.env.foo = "bar"' && echo $foo

But this will:

    process.env.foo = 'bar';
    console.log(process.env.foo);


## process.exit([code])

Ends the process with the specified `code`.  If omitted, exit uses the
'success' code `0`.

To exit with a 'failure' code:

    process.exit(1);

The shell that executed Node.js should see the exit code as 1.


## process.exitCode

A number which will be the process exit code, when the process either
exits gracefully, or is exited via `process.exit()` without specifying
a code.

Specifying a code to `process.exit(code)` will override any previous
setting of `process.exitCode`.


## process.getgid()

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the group identity of the process. (See getgid(2).)
This is the numerical group id, not the group name.

    if (process.getgid) {
      console.log('Current gid: ' + process.getgid());
    }


## process.getegid()

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the effective group identity of the process. (See getegid(2).)
This is the numerical group id, not the group name.

    if (process.getegid) {
      console.log('Current gid: ' + process.getegid());
    }


## process.setgid(id)

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the group identity of the process. (See setgid(2).)  This accepts either
a numerical ID or a groupname string. If a groupname is specified, this method
blocks while resolving it to a numerical ID.

    if (process.getgid && process.setgid) {
      console.log('Current gid: ' + process.getgid());
      try {
        process.setgid(501);
        console.log('New gid: ' + process.getgid());
      }
      catch (err) {
        console.log('Failed to set gid: ' + err);
      }
    }


## process.setegid(id)

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the effective group identity of the process. (See setegid(2).)
This accepts either a numerical ID or a groupname string. If a groupname
is specified, this method blocks while resolving it to a numerical ID.

    if (process.getegid && process.setegid) {
      console.log('Current gid: ' + process.getegid());
      try {
        process.setegid(501);
        console.log('New gid: ' + process.getegid());
      }
      catch (err) {
        console.log('Failed to set gid: ' + err);
      }
    }


## process.getuid()

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the user identity of the process. (See getuid(2).)
This is the numerical userid, not the username.

    if (process.getuid) {
      console.log('Current uid: ' + process.getuid());
    }


## process.geteuid()

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the effective user identity of the process. (See geteuid(2).)
This is the numerical userid, not the username.

    if (process.geteuid) {
      console.log('Current uid: ' + process.geteuid());
    }


## process.setuid(id)

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the user identity of the process. (See setuid(2).)  This accepts either
a numerical ID or a username string.  If a username is specified, this method
blocks while resolving it to a numerical ID.

    if (process.getuid && process.setuid) {
      console.log('Current uid: ' + process.getuid());
      try {
        process.setuid(501);
        console.log('New uid: ' + process.getuid());
      }
      catch (err) {
        console.log('Failed to set uid: ' + err);
      }
    }


## process.seteuid(id)

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the effective user identity of the process. (See seteuid(2).)
This accepts either a numerical ID or a username string.  If a username
is specified, this method blocks while resolving it to a numerical ID.

    if (process.geteuid && process.seteuid) {
      console.log('Current uid: ' + process.geteuid());
      try {
        process.seteuid(501);
        console.log('New uid: ' + process.geteuid());
      }
      catch (err) {
        console.log('Failed to set uid: ' + err);
      }
    }


## process.getgroups()

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Returns an array with the supplementary group IDs. POSIX leaves it unspecified
if the effective group ID is included but Node.js ensures it always is.


## process.setgroups(groups)

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Sets the supplementary group IDs. This is a privileged operation, meaning you
need to be root or have the CAP_SETGID capability.

The list can contain group IDs, group names or both.


## process.initgroups(user, extra_group)

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Reads /etc/group and initializes the group access list, using all groups of
which the user is a member. This is a privileged operation, meaning you need
to be root or have the CAP_SETGID capability.

`user` is a user name or user ID. `extra_group` is a group name or group ID.

Some care needs to be taken when dropping privileges. Example:

    console.log(process.getgroups());         // [ 0 ]
    process.initgroups('bnoordhuis', 1000);   // switch user
    console.log(process.getgroups());         // [ 27, 30, 46, 1000, 0 ]
    process.setgid(1000);                     // drop root gid
    console.log(process.getgroups());         // [ 27, 30, 46, 1000 ]


## process.version

A compiled-in property that exposes `NODE_VERSION`.

    console.log('Version: ' + process.version);

## process.versions

A property exposing version strings of Node.js and its dependencies.

    console.log(process.versions);

Will print something like:

    { http_parser: '2.3.0',
      node: '1.1.1',
      v8: '4.1.0.14',
      uv: '1.3.0',
      zlib: '1.2.8',
      ares: '1.10.0-DEV',
      modules: '43',
      icu: '55.1',
      openssl: '1.0.1k' }

## process.config

An Object containing the JavaScript representation of the configure options
that were used to compile the current Node.js executable. This is the same as
the "config.gypi" file that was produced when running the `./configure` script.

An example of the possible output looks like:

    { target_defaults:
       { cflags: [],
         default_configuration: 'Release',
         defines: [],
         include_dirs: [],
         libraries: [] },
      variables:
       { host_arch: 'x64',
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
         v8_use_snapshot: 'true' } }

## process.release

An Object containing metadata related to the current release, including URLs
for the source tarball and headers-only tarball.

`process.release` contains the following properties:

* `name`: a string with a value that will always be `"node"` for Node.js. For
  legacy io.js releases, this will be `"io.js"`.
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

    { name: 'node',
      sourceUrl: 'https://nodejs.org/download/release/v4.0.0/node-v4.0.0.tar.gz',
      headersUrl: 'https://nodejs.org/download/release/v4.0.0/node-v4.0.0-headers.tar.gz',
      libUrl: 'https://nodejs.org/download/release/v4.0.0/win-x64/node.lib' }

In custom builds from non-release versions of the source tree, only the
`name` property may be present. The additional properties should not be
relied upon to exist.

## process.kill(pid[, signal])

Send a signal to a process. `pid` is the process id and `signal` is the
string describing the signal to send.  Signal names are strings like
'SIGINT' or 'SIGHUP'.  If omitted, the signal will be 'SIGTERM'.
See [Signal Events](#process_signal_events) and kill(2) for more information.

Will throw an error if target does not exist, and as a special case, a signal
of `0` can be used to test for the existence of a process.

Note that even though the name of this function is `process.kill`, it is really
just a signal sender, like the `kill` system call.  The signal sent may do
something other than kill the target process.

Example of sending a signal to yourself:

    process.on('SIGHUP', function() {
      console.log('Got SIGHUP signal.');
    });

    setTimeout(function() {
      console.log('Exiting.');
      process.exit(0);
    }, 100);

    process.kill(process.pid, 'SIGHUP');

Note: When SIGUSR1 is received by Node.js it starts the debugger, see
[Signal Events](#process_signal_events).

## process.pid

The PID of the process.

    console.log('This process is pid ' + process.pid);


## process.title

Getter/setter to set what is displayed in 'ps'.

When used as a setter, the maximum length is platform-specific and probably
short.

On Linux and OS X, it's limited to the size of the binary name plus the
length of the command line arguments because it overwrites the argv memory.

v0.8 allowed for longer process title strings by also overwriting the environ
memory but that was potentially insecure/confusing in some (rather obscure)
cases.


## process.arch

What processor architecture you're running on: `'arm'`, `'ia32'`, or `'x64'`.

    console.log('This processor architecture is ' + process.arch);


## process.platform

What platform you're running on:
`'darwin'`, `'freebsd'`, `'linux'`, `'sunos'` or `'win32'`

    console.log('This platform is ' + process.platform);


## process.memoryUsage()

Returns an object describing the memory usage of the Node.js process
measured in bytes.

    var util = require('util');

    console.log(util.inspect(process.memoryUsage()));

This will generate:

    { rss: 4935680,
      heapTotal: 1826816,
      heapUsed: 650472 }

`heapTotal` and `heapUsed` refer to V8's memory usage.


## process.nextTick(callback[, arg][, ...])

* `callback` {Function}

Once the current event loop turn runs to completion, call the callback
function.

This is *not* a simple alias to `setTimeout(fn, 0)`, it's much more
efficient.  It runs before any additional I/O events (including
timers) fire in subsequent ticks of the event loop.

    console.log('start');
    process.nextTick(function() {
      console.log('nextTick callback');
    });
    console.log('scheduled');
    // Output:
    // start
    // scheduled
    // nextTick callback

This is important in developing APIs where you want to give the user the
chance to assign event handlers after an object has been constructed,
but before any I/O has occurred.

    function MyThing(options) {
      this.setupOptions(options);

      process.nextTick(function() {
        this.startDoingStuff();
      }.bind(this));
    }

    var thing = new MyThing();
    thing.getReadyForStuff();

    // thing.startDoingStuff() gets called now, not before.

It is very important for APIs to be either 100% synchronous or 100%
asynchronous.  Consider this example:

    // WARNING!  DO NOT USE!  BAD UNSAFE HAZARD!
    function maybeSync(arg, cb) {
      if (arg) {
        cb();
        return;
      }

      fs.stat('file', cb);
    }

This API is hazardous.  If you do this:

    maybeSync(true, function() {
      foo();
    });
    bar();

then it's not clear whether `foo()` or `bar()` will be called first.

This approach is much better:

    function definitelyAsync(arg, cb) {
      if (arg) {
        process.nextTick(cb);
        return;
      }

      fs.stat('file', cb);
    }

Note: the nextTick queue is completely drained on each pass of the
event loop **before** additional I/O is processed.  As a result,
recursively setting nextTick callbacks will block any I/O from
happening, just like a `while(true);` loop.

## process.umask([mask])

Sets or reads the process's file mode creation mask. Child processes inherit
the mask from the parent process. Returns the old mask if `mask` argument is
given, otherwise returns the current mask.

    var oldmask, newmask = 0022;

    oldmask = process.umask(newmask);
    console.log('Changed umask from: ' + oldmask.toString(8) +
                ' to ' + newmask.toString(8));


## process.uptime()

Number of seconds Node.js has been running.


## process.hrtime()

Returns the current high-resolution real time in a `[seconds, nanoseconds]`
tuple Array. It is relative to an arbitrary time in the past. It is not
related to the time of day and therefore not subject to clock drift. The
primary use is for measuring performance between intervals.

You may pass in the result of a previous call to `process.hrtime()` to get
a diff reading, useful for benchmarks and measuring intervals:

    var time = process.hrtime();
    // [ 1800216, 25 ]

    setTimeout(function() {
      var diff = process.hrtime(time);
      // [ 1, 552 ]

      console.log('benchmark took %d nanoseconds', diff[0] * 1e9 + diff[1]);
      // benchmark took 1000000527 nanoseconds
    }, 1000);


## process.send(message[, sendHandle][, callback])

* `message` {Object}
* `sendHandle` {Handle object}

When Node.js is spawned with an IPC channel attached, it can send messages to its
parent process using `process.send()`. Each will be received as a
['message'](child_process.html#child_process_event_message)
event on the parent's `ChildProcess` object.

If Node.js was not spawned with an IPC channel, `process.send()` will be undefined.


## process.disconnect()

Close the IPC channel to the parent process, allowing this child to exit
gracefully once there are no other connections keeping it alive.

Identical to the parent process's
[ChildProcess.disconnect()](child_process.html#child_process_child_disconnect).

If Node.js was not spawned with an IPC channel, `process.disconnect()` will be
undefined.


### process.connected

* {Boolean} Set to false after `process.disconnect()` is called

If `process.connected` is false, it is no longer possible to send messages.


## process.mainModule

Alternate way to retrieve
[`require.main`](modules.html#modules_accessing_the_main_module).
The difference is that if the main module changes at runtime, `require.main`
might still refer to the original main module in modules that were required
before the change occurred. Generally it's safe to assume that the two refer
to the same module.

As with `require.main`, it will be `undefined` if there was no entry script.

[ChildProcess.send()]: child_process.html#child_process_child_send_message_sendhandle_callback
[EventEmitter]: events.html#events_class_events_eventemitter
[net.Server]: net.html#net_class_net_server
[net.Socket]: net.html#net_class_net_socket
