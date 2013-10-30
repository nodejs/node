# process

<!-- type=global -->

The `process` object is a global object and can be accessed from anywhere.
It is an instance of [EventEmitter][].

## Exit Codes

Node will normally exit with a `0` status code when no more async
operations are pending.  The following status codes are used in other
cases:

* `1` **Uncaught Fatal Exception** - There was an uncaught exception,
  and it was not handled by a domain or an `uncaughtException` event
  handler.
* `2` - Unused (reserved by Bash for builtin misuse)
* `3` **Internal JavaScript Parse Error** - The JavaScript source code
  internal in Node's bootstrapping process caused a parse error.  This
  is extremely rare, and generally can only happen during development
  of Node itself.
* `4` **Internal JavaScript Evaluation Failure** - The JavaScript
  source code internal in Node's bootstrapping process failed to
  return a function value when evaluated.  This is extremely rare, and
  generally can only happen during development of Node itself.
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
* `8` - Unused.  In previous versions of Node, exit code 8 sometimes
  indicated an uncaught exception.
* `9` - **Invalid Argument** - Either an unknown option was specified,
  or an option requiring a value was provided without a value.
* `10` **Internal JavaScript Run-Time Failure** - The JavaScript
  source code internal in Node's bootstrapping process threw an error
  when the bootstrapping function was called.  This is extremely rare,
  and generally can only happen during development of Node itself.
* `12` **Invalid Debug Argument** - The `--debug` and/or `--debug-brk`
  options were set, but an invalid port number was chosen.
* `>128` **Signal Exits** - If Node receives a fatal signal such as
  `SIGKILL` or `SIGHUP`, then its exit code will be `128` plus the
  value of the signal code.  This is a standard Unix practice, since
  exit codes are defined to be 7-bit integers, and signal exits set
  the high-order bit, and then contain the value of the signal code.

## Event: 'exit'

Emitted when the process is about to exit.  This is a good hook to perform
constant time checks of the module's state (like for unit tests).  The main
event loop will no longer be run after the 'exit' callback finishes, so
timers may not be scheduled.

Example of listening for `exit`:

    process.on('exit', function() {
      setTimeout(function() {
        console.log('This will not run');
      }, 0);
      console.log('About to exit.');
    });

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

Don't use it, use [domains](domain.html) instead. If you do use it, restart
your application after every unhandled exception!

Do *not* use it as the node.js equivalent of `On Error Resume Next`. An
unhandled exception means your application - and by extension node.js itself -
is in an undefined state. Blindly resuming means *anything* could happen.

Think of resuming as pulling the power cord when you are upgrading your system.
Nine out of ten times nothing happens - but the 10th time, your system is bust.

You have been warned.

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

Note: SIGUSR1 is reserved by node.js to kickstart the debugger.  It's possible
to install a listener but that won't stop the debugger from starting.

## process.stdout

A `Writable Stream` to `stdout`.

Example: the definition of `console.log`

    console.log = function(d) {
      process.stdout.write(d + '\n');
    };

`process.stderr` and `process.stdout` are unlike other streams in Node in
that writes to them are usually blocking.  They are blocking in the case
that they refer to regular files or TTY file descriptors. In the case they
refer to pipes, they are non-blocking like other streams.

To check if Node is being run in a TTY context, read the `isTTY` property
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

A writable stream to stderr.

`process.stderr` and `process.stdout` are unlike other streams in Node in
that writes to them are usually blocking.  They are blocking in the case
that they refer to regular files or TTY file descriptors. In the case they
refer to pipes, they are non-blocking like other streams.


## process.stdin

A `Readable Stream` for stdin. The stdin stream is paused by default, so one
must call `process.stdin.resume()` to read from it.

Example of opening standard input and listening for both events:

    process.stdin.resume();
    process.stdin.setEncoding('utf8');

    process.stdin.on('data', function(chunk) {
      process.stdout.write('data: ' + chunk);
    });

    process.stdin.on('end', function() {
      process.stdout.write('end');
    });


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

This is the set of node-specific command line options from the
executable that started the process.  These options do not show up in
`process.argv`, and do not include the node executable, the name of
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

This causes node to emit an abort. This will cause node to exit and
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


## process.exit([code])

Ends the process with the specified `code`.  If omitted, exit uses the
'success' code `0`.

To exit with a 'failure' code:

    process.exit(1);

The shell that executed node should see the exit code as 1.


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


## process.getuid()

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Gets the user identity of the process. (See getuid(2).)
This is the numerical userid, not the username.

    if (process.getuid) {
      console.log('Current uid: ' + process.getuid());
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


## process.getgroups()

Note: this function is only available on POSIX platforms (i.e. not Windows,
Android)

Returns an array with the supplementary group IDs. POSIX leaves it unspecified
if the effective group ID is included but node.js ensures it always is.


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

A property exposing version strings of node and its dependencies.

    console.log(process.versions);

Will print something like:

    { http_parser: '1.0',
      node: '0.10.4',
      v8: '3.14.5.8',
      ares: '1.9.0-DEV',
      uv: '0.10.3',
      zlib: '1.2.3',
      modules: '11',
      openssl: '1.0.1e' }

## process.config

An Object containing the JavaScript representation of the configure options
that were used to compile the current node executable. This is the same as
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
         node_shared_v8: 'false',
         node_shared_zlib: 'false',
         node_use_dtrace: 'false',
         node_use_openssl: 'true',
         node_shared_openssl: 'false',
         strict_aliasing: 'true',
         target_arch: 'x64',
         v8_use_snapshot: 'true' } }

## process.kill(pid, [signal])

Send a signal to a process. `pid` is the process id and `signal` is the
string describing the signal to send.  Signal names are strings like
'SIGINT' or 'SIGHUP'.  If omitted, the signal will be 'SIGTERM'.
See kill(2) for more information.

Note that just because the name of this function is `process.kill`, it is
really just a signal sender, like the `kill` system call.  The signal sent
may do something other than kill the target process.

Example of sending a signal to yourself:

    process.on('SIGHUP', function() {
      console.log('Got SIGHUP signal.');
    });

    setTimeout(function() {
      console.log('Exiting.');
      process.exit(0);
    }, 100);

    process.kill(process.pid, 'SIGHUP');

Note: SIGUSR1 is reserved by node.js.  It can be used to kickstart the
debugger.

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

Returns an object describing the memory usage of the Node process
measured in bytes.

    var util = require('util');

    console.log(util.inspect(process.memoryUsage()));

This will generate:

    { rss: 4935680,
      heapTotal: 1826816,
      heapUsed: 650472 }

`heapTotal` and `heapUsed` refer to V8's memory usage.


## process.nextTick(callback)

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

    var oldmask, newmask = 0644;

    oldmask = process.umask(newmask);
    console.log('Changed umask from: ' + oldmask.toString(8) +
                ' to ' + newmask.toString(8));


## process.uptime()

Number of seconds Node has been running.


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


## Async Listeners

<!-- type=misc -->

    Stability: 1 - Experimental

The `AsyncListener` API is the JavaScript interface for the `AsyncWrap`
class which allows developers to be notified about key events in the
lifetime of an asynchronous event. Node performs a lot of asynchronous
events internally, and significant use of this API will have a **dramatic
performance impact** on your application.


## process.createAsyncListener(asyncListener[, callbacksObj[, storageValue]])

* `asyncListener` {Function} callback fired when an asynchronous event is
instantiated.
* `callbacksObj` {Object} optional callbacks that will fire at specific
times in the lifetime of the asynchronous event.
* `storageValue` {Value} a value that will be passed as the first argument
when the `asyncListener` callback is run, and to all subsequent callback.

Returns a constructed `AsyncListener` object.

To begin capturing asynchronous events pass the object to
[`process.addAsyncListener()`][]. The same `AsyncListener` instance can
only be added once to the active queue, and subsequent attempts to add the
instance will be ignored.

To stop capturing pass the object to [`process.removeAsyncListener()`][].
This does _not_ mean the `AsyncListener` previously added will stop
triggering callbacks. Once attached to an asynchronous event it will
persist with the lifetime of the asynchronous call stack.

Explanation of function parameters:

`asyncListener(storageValue)`: A `Function` called when an asynchronous
event is instantiated. If a `Value` is returned then it will be attached
to the event and overwrite any value that had been passed to
`process.createAsyncListener()`'s `storageValue` argument. If an initial
`storageValue` was passed when created, then `asyncListener()` will
receive that as a function argument.

`callbacksObj`: An `Object` which may contain three optional fields:

* `before(context, storageValue)`: A `Function` that is called immediately
before the asynchronous callback is about to run. It will be passed both
the `context` (i.e. `this`) of the calling function and the `storageValue`
either returned from `asyncListener` or passed during construction (if
either occurred).

* `after(context, storageValue)`: A `Function` called immediately after
the asynchronous event's callback has run. Note this will not be called
if the callback throws and the error is not handled.

* `error(storageValue, error)`: A `Function` called if the event's
callback threw. If `error` returns `true` then Node will assume the error
has been properly handled and resume execution normally. When multiple
`error()` callbacks have been registered, only **one** of those callbacks
needs to return `true` for `AsyncListener` to accept that the error has
been handled.

`storageValue`: A `Value` (i.e. anything) that will be, by default,
attached to all new event instances. This will be overwritten if a `Value`
is returned by `asyncListener()`.

Here is an example of overwriting the `storageValue`:

    process.createAsyncListener(function listener(value) {
      // value === true
      return false;
    }, {
      before: function before(context, value) {
        // value === false
      }
    }, true);

**Note:** The [EventEmitter][], while used to emit status of an asynchronous
event, is not itself asynchronous. So `asyncListener()` will not fire when
an event is added, and `before`/`after` will not fire when emitted
callbacks are called.


## process.addAsyncListener(asyncListener[, callbacksObj[, storageValue]])
## process.addAsyncListener(asyncListener)

Returns a constructed `AsyncListener` object and immediately adds it to
the listening queue to begin capturing asynchronous events.

Function parameters can either be the same as
[`process.createAsyncListener()`][], or a constructed `AsyncListener`
object.

Example usage for capturing errors:

    var cntr = 0;
    var key = process.addAsyncListener(function() {
      return { uid: cntr++ };
    }, {
      before: function onBefore(context, storage) {
        // Need to remove the listener while logging or will end up
        // with an infinite call loop.
        process.removeAsyncListener(key);
        console.log('uid: %s is about to run', storage.uid);
        process.addAsyncListener(key);
      },
      after: function onAfter(context, storage) {
        process.removeAsyncListener(key);
        console.log('uid: %s is about to run', storage.uid);
        process.addAsyncListener(key);
      },
      error: function onError(storage, err) {
        // Handle known errors
        if (err.message === 'really, it\'s ok') {
          process.removeAsyncListener(key);
          console.log('handled error just threw:');
          console.log(err.stack);
          process.addAsyncListener(key);
          return true;
        }
      }
    });

    process.nextTick(function() {
      throw new Error('really, it\'s ok');
    });

    // Output:
    // uid: 0 is about to run
    // handled error just threw:
    // Error: really, it's ok
    //     at /tmp/test2.js:27:9
    //     at process._tickCallback (node.js:583:11)
    //     at Function.Module.runMain (module.js:492:11)
    //     at startup (node.js:123:16)
    //     at node.js:1012:3

## process.removeAsyncListener(asyncListener)

Removes the `AsyncListener` from the listening queue.

Removing the `AsyncListener` from the queue does _not_ mean asynchronous
events called during its execution scope will stop firing callbacks. Once
attached to an event it will persist for the entire asynchronous call
stack. For example:

    var key = process.createAsyncListener(function asyncListener() {
      // To log we must stop listening or we'll enter infinite recursion.
      process.removeAsyncListener(key);
      console.log('You summoned me?');
      process.addAsyncListener(key);
    });

    // We want to begin capturing async events some time in the future.
    setTimeout(function() {
      process.addAsyncListener(key);

      // Perform a few additional async events.
      setTimeout(function() {
        setImmediate(function() {
          process.nextTick(function() { });
        });
      });

      // Removing the listener doesn't mean to stop capturing events that
      // have already been added.
      process.removeAsyncListener(key);
    }, 100);

    // Output:
    // You summoned me?
    // You summoned me?
    // You summoned me?
    // You summoned me?

The fact that we logged 4 asynchronous events is an implementation detail
of Node's [Timers][].

To stop capturing from a specific asynchronous event stack
`process.removeAsyncListener()` must be called from within the call
stack itself. For example:

    var key = process.createAsyncListener(function asyncListener() {
      // To log we must stop listening or we'll enter infinite recursion.
      process.removeAsyncListener(key);
      console.log('You summoned me?');
      process.addAsyncListener(key);
    });

    // We want to begin capturing async events some time in the future.
    setTimeout(function() {
      process.addAsyncListener(key);

      // Perform a few additional async events.
      setImmediate(function() {
        // Stop capturing from this call stack.
        process.removeAsyncListener(key);

        process.nextTick(function() { });
      });
    }, 100);

    // Output:
    // You summoned me?

The user must be explicit and always pass the `AsyncListener` they wish
to remove. It is not possible to simply remove all listeners at once.


[EventEmitter]: events.html#events_class_events_eventemitter
[Timers]: timers.html
[`process.createAsyncListener()`]: #process_process_createasynclistener_asynclistener_callbacksobj_storagevalue
[`process.addAsyncListener()`]: #process_process_addasynclistener_asynclistener
[`process.removeAsyncListener()`]: #process_process_removeasynclistener_asynclistener
