# process

<!-- type=global -->

The `process` object is a global object and can be accessed from anywhere.
It is an instance of `EventEmitter`.


## Event: 'exit'

Emitted when the process is about to exit.  This is a good hook to perform
constant time checks of the module's state (like for unit tests).  The main
event loop will no longer be run after the 'exit' callback finishes, so
timers may not be scheduled.

Example of listening for `exit`:

    process.on('exit', function () {
      process.nextTick(function () {
       console.log('This will not run');
      });
      console.log('About to exit.');
    });

## Event: 'uncaughtException'

Emitted when an exception bubbles all the way back to the event loop. If a
listener is added for this exception, the default action (which is to print
a stack trace and exit) will not occur.

Example of listening for `uncaughtException`:

    process.on('uncaughtException', function (err) {
      console.log('Caught exception: ' + err);
    });

    setTimeout(function () {
      console.log('This will still run.');
    }, 500);

    // Intentionally cause an exception, but don't catch it.
    nonexistentFunc();
    console.log('This will not run.');

Note that `uncaughtException` is a very crude mechanism for exception
handling.  Using try / catch in your program will give you more control over
your program's flow.  Especially for server programs that are designed to
stay running forever, `uncaughtException` can be a useful safety mechanism.


## Signal Events

<!--type=event-->
<!--name=SIGINT, SIGUSR1, etc.-->

Emitted when the processes receives a signal. See sigaction(2) for a list of
standard POSIX signal names such as SIGINT, SIGUSR1, etc.

Example of listening for `SIGINT`:

    // Start reading from stdin so we don't exit.
    process.stdin.resume();

    process.on('SIGINT', function () {
      console.log('Got SIGINT.  Press Control-D to exit.');
    });

An easy way to send the `SIGINT` signal is with `Control-C` in most terminal
programs.


## process.stdout

A `Writable Stream` to `stdout`.

Example: the definition of `console.log`

    console.log = function (d) {
      process.stdout.write(d + '\n');
    };

`process.stderr` and `process.stdout` are unlike other streams in Node in
that writes to them are usually blocking.  They are blocking in the case
that they refer to regular files or TTY file descriptors. In the case they
refer to pipes, they are non-blocking like other streams.


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

    process.stdin.on('data', function (chunk) {
      process.stdout.write('data: ' + chunk);
    });

    process.stdin.on('end', function () {
      process.stdout.write('end');
    });


## process.argv

An array containing the command line arguments.  The first element will be
'node', the second element will be the name of the JavaScript file.  The
next elements will be any additional command line arguments.

    // print process.argv
    process.argv.forEach(function (val, index, array) {
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


## process.getgid()

Gets the group identity of the process. (See getgid(2).)
This is the numerical group id, not the group name.

    console.log('Current gid: ' + process.getgid());


## process.setgid(id)

Sets the group identity of the process. (See setgid(2).)  This accepts either
a numerical ID or a groupname string. If a groupname is specified, this method
blocks while resolving it to a numerical ID.

    console.log('Current gid: ' + process.getgid());
    try {
      process.setgid(501);
      console.log('New gid: ' + process.getgid());
    }
    catch (err) {
      console.log('Failed to set gid: ' + err);
    }


## process.getuid()

Gets the user identity of the process. (See getuid(2).)
This is the numerical userid, not the username.

    console.log('Current uid: ' + process.getuid());


## process.setuid(id)

Sets the user identity of the process. (See setuid(2).)  This accepts either
a numerical ID or a username string.  If a username is specified, this method
blocks while resolving it to a numerical ID.

    console.log('Current uid: ' + process.getuid());
    try {
      process.setuid(501);
      console.log('New uid: ' + process.getuid());
    }
    catch (err) {
      console.log('Failed to set uid: ' + err);
    }


## process.version

A compiled-in property that exposes `NODE_VERSION`.

    console.log('Version: ' + process.version);

## process.versions

A property exposing version strings of node and its dependencies.

    console.log(process.versions);

Will output:

    { node: '0.4.12',
      v8: '3.1.8.26',
      ares: '1.7.4',
      ev: '4.4',
      openssl: '1.0.0e-fips' }


## process.installPrefix

A compiled-in property that exposes `NODE_PREFIX`.

    console.log('Prefix: ' + process.installPrefix);


## process.kill(pid, [signal])

Send a signal to a process. `pid` is the process id and `signal` is the
string describing the signal to send.  Signal names are strings like
'SIGINT' or 'SIGUSR1'.  If omitted, the signal will be 'SIGTERM'.
See kill(2) for more information.

Note that just because the name of this function is `process.kill`, it is
really just a signal sender, like the `kill` system call.  The signal sent
may do something other than kill the target process.

Example of sending a signal to yourself:

    process.on('SIGHUP', function () {
      console.log('Got SIGHUP signal.');
    });

    setTimeout(function () {
      console.log('Exiting.');
      process.exit(0);
    }, 100);

    process.kill(process.pid, 'SIGHUP');


## process.pid

The PID of the process.

    console.log('This process is pid ' + process.pid);

## process.title

Getter/setter to set what is displayed in 'ps'.


## process.arch

What processor architecture you're running on: `'arm'`, `'ia32'`, or `'x64'`.

    console.log('This processor architecture is ' + process.arch);


## process.platform

What platform you're running on. `'linux2'`, `'darwin'`, etc.

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

On the next loop around the event loop call this callback.
This is *not* a simple alias to `setTimeout(fn, 0)`, it's much more
efficient.

    process.nextTick(function () {
      console.log('nextTick callback');
    });


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
