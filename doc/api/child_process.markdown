# Child Process

    Stability: 3 - Stable

Node provides a tri-directional `popen(3)` facility through the
`child_process` module.

It is possible to stream data through a child's `stdin`, `stdout`, and
`stderr` in a fully non-blocking way.

To create a child process use `require('child_process').spawn()` or
`require('child_process').fork()`.  The semantics of each are slightly
different, and explained below.

## Class: ChildProcess

`ChildProcess` is an `EventEmitter`.

Child processes always have three streams associated with them. `child.stdin`,
`child.stdout`, and `child.stderr`.  These may be shared with the stdio
streams of the parent process, or they may be separate stream objects
which can be piped to and from.

The ChildProcess class is not intended to be used directly.  Use the
`spawn()` or `fork()` methods to create a Child Process instance.

### Event:  'exit'

* `code` {Number} the exit code, if it exited normally.
* `signal` {String} the signal passed to kill the child process, if it
  was killed by the parent.

This event is emitted after the child process ends. If the process terminated
normally, `code` is the final exit code of the process, otherwise `null`. If
the process terminated due to receipt of a signal, `signal` is the string name
of the signal, otherwise `null`.

See `waitpid(2)`.

### child.stdin

* {Stream object}

A `Writable Stream` that represents the child process's `stdin`.
Closing this stream via `end()` often causes the child process to terminate.

If the child stdio streams are shared with the parent, then this will
not be set.

### child.stdout

* {Stream object}

A `Readable Stream` that represents the child process's `stdout`.

If the child stdio streams are shared with the parent, then this will
not be set.

### child.stderr

* {Stream object}

A `Readable Stream` that represents the child process's `stderr`.

If the child stdio streams are shared with the parent, then this will
not be set.

### child.pid

* {Integer}

The PID of the child process.

Example:

    var spawn = require('child_process').spawn,
        grep  = spawn('grep', ['ssh']);

    console.log('Spawned child pid: ' + grep.pid);
    grep.stdin.end();

### child.kill([signal])

* `signal` {String}

Send a signal to the child process. If no argument is given, the process will
be sent `'SIGTERM'`. See `signal(7)` for a list of available signals.

    var spawn = require('child_process').spawn,
        grep  = spawn('grep', ['ssh']);

    grep.on('exit', function (code, signal) {
      console.log('child process terminated due to receipt of signal '+signal);
    });

    // send SIGHUP to process
    grep.kill('SIGHUP');

Note that while the function is called `kill`, the signal delivered to the child
process may not actually kill it.  `kill` really just sends a signal to a process.

See `kill(2)`

### child.send(message, [sendHandle])

* `message` {Object}
* `sendHandle` {Handle object}

Send a message (and, optionally, a handle object) to a child process.

See `child_process.fork()` for details.

## child_process.spawn(command, [args], [options])

* `command` {String} The command to run
* `args` {Array} List of string arguments
* `options` {Object}
  * `cwd` {String} Current working directory of the child process
  * `customFds` {Array} **Deprecated** File descriptors for the child to use
    for stdio.  (See below)
  * `env` {Object} Environment key-value pairs
  * `setsid` {Boolean}
* return: {ChildProcess object}

Launches a new process with the given `command`, with  command line arguments in `args`.
If omitted, `args` defaults to an empty Array.

The third argument is used to specify additional options, which defaults to:

    { cwd: undefined,
      env: process.env
    }

`cwd` allows you to specify the working directory from which the process is spawned.
Use `env` to specify environment variables that will be visible to the new process.

Example of running `ls -lh /usr`, capturing `stdout`, `stderr`, and the exit code:

    var util  = require('util'),
        spawn = require('child_process').spawn,
        ls    = spawn('ls', ['-lh', '/usr']);

    ls.stdout.on('data', function (data) {
      console.log('stdout: ' + data);
    });

    ls.stderr.on('data', function (data) {
      console.log('stderr: ' + data);
    });

    ls.on('exit', function (code) {
      console.log('child process exited with code ' + code);
    });


Example: A very elaborate way to run 'ps ax | grep ssh'

    var util  = require('util'),
        spawn = require('child_process').spawn,
        ps    = spawn('ps', ['ax']),
        grep  = spawn('grep', ['ssh']);

    ps.stdout.on('data', function (data) {
      grep.stdin.write(data);
    });

    ps.stderr.on('data', function (data) {
      console.log('ps stderr: ' + data);
    });

    ps.on('exit', function (code) {
      if (code !== 0) {
        console.log('ps process exited with code ' + code);
      }
      grep.stdin.end();
    });

    grep.stdout.on('data', function (data) {
      console.log(data);
    });

    grep.stderr.on('data', function (data) {
      console.log('grep stderr: ' + data);
    });

    grep.on('exit', function (code) {
      if (code !== 0) {
        console.log('grep process exited with code ' + code);
      }
    });


Example of checking for failed exec:

    var spawn = require('child_process').spawn,
        child = spawn('bad_command');

    child.stderr.setEncoding('utf8');
    child.stderr.on('data', function (data) {
      if (/^execvp\(\)/.test(data)) {
        console.log('Failed to start child process.');
      }
    });

Note that if spawn receives an empty options object, it will result in
spawning the process with an empty environment rather than using
`process.env`. This due to backwards compatibility issues with a deprecated
API.

There is a deprecated option called `customFds` which allows one to specify
specific file descriptors for the stdio of the child process. This API was
not portable to all platforms and therefore removed.
With `customFds` it was possible to hook up the new process' `[stdin, stdout,
stderr]` to existing streams; `-1` meant that a new stream should be created.
Use at your own risk.

There are several internal options. In particular `stdinStream`,
`stdoutStream`, `stderrStream`. They are for INTERNAL USE ONLY. As with all
undocumented APIs in Node, they should not be used.

See also: `child_process.exec()` and `child_process.fork()`

## child_process.exec(command, [options], callback)

* `command` {String} The command to run, with space-separated arguments
* `options` {Object}
  * `cwd` {String} Current working directory of the child process
  * `customFds` {Array} **Deprecated** File descriptors for the child to use
    for stdio.  (See below)
  * `env` {Object} Environment key-value pairs
  * `setsid` {Boolean}
  * `encoding` {String} (Default: 'utf8')
  * `timeout` {Number} (Default: 0)
  * `maxBuffer` {Number} (Default: 200*1024)
  * `killSignal` {String} (Default: 'SIGTERM')
* `callback` {Function} called with the output when process terminates
  * `code` {Integer} Exit code
  * `stdout` {Buffer}
  * `stderr` {Buffer}
* Return: ChildProcess object

Runs a command in a shell and buffers the output.

    var util = require('util'),
        exec = require('child_process').exec,
        child;

    child = exec('cat *.js bad_file | wc -l',
      function (error, stdout, stderr) {
        console.log('stdout: ' + stdout);
        console.log('stderr: ' + stderr);
        if (error !== null) {
          console.log('exec error: ' + error);
        }
    });

The callback gets the arguments `(error, stdout, stderr)`. On success, `error`
will be `null`.  On error, `error` will be an instance of `Error` and `err.code`
will be the exit code of the child process, and `err.signal` will be set to the
signal that terminated the process.

There is a second optional argument to specify several options. The
default options are

    { encoding: 'utf8',
      timeout: 0,
      maxBuffer: 200*1024,
      killSignal: 'SIGTERM',
      cwd: null,
      env: null }

If `timeout` is greater than 0, then it will kill the child process
if it runs longer than `timeout` milliseconds. The child process is killed with
`killSignal` (default: `'SIGTERM'`). `maxBuffer` specifies the largest
amount of data allowed on stdout or stderr - if this value is exceeded then
the child process is killed.


## child_process.execFile(file, args, options, callback)

* `file` {String} The filename of the program to run
* `args` {Array} List of string arguments
* `options` {Object}
  * `cwd` {String} Current working directory of the child process
  * `customFds` {Array} **Deprecated** File descriptors for the child to use
    for stdio.  (See below)
  * `env` {Object} Environment key-value pairs
  * `setsid` {Boolean}
  * `encoding` {String} (Default: 'utf8')
  * `timeout` {Number} (Default: 0)
  * `maxBuffer` {Number} (Default: 200*1024)
  * `killSignal` {String} (Default: 'SIGTERM')
* `callback` {Function} called with the output when process terminates
  * `code` {Integer} Exit code
  * `stdout` {Buffer}
  * `stderr` {Buffer}
* Return: ChildProcess object

This is similar to `child_process.exec()` except it does not execute a
subshell but rather the specified file directly. This makes it slightly
leaner than `child_process.exec`. It has the same options.


## child_process.fork(modulePath, [args], [options])

* `modulePath` {String} The module to run in the child
* `args` {Array} List of string arguments
* `options` {Object}
  * `cwd` {String} Current working directory of the child process
  * `customFds` {Array} **Deprecated** File descriptors for the child to use
    for stdio.  (See below)
  * `env` {Object} Environment key-value pairs
  * `setsid` {Boolean}
  * `encoding` {String} (Default: 'utf8')
  * `timeout` {Number} (Default: 0)
* `callback` {Function} called with the output when process terminates
  * `code` {Integer} Exit code
  * `stdout` {Buffer}
  * `stderr` {Buffer}
* Return: ChildProcess object

This is a special case of the `spawn()` functionality for spawning Node
processes. In addition to having all the methods in a normal ChildProcess
instance, the returned object has a communication channel built-in. The
channel is written to with `child.send(message, [sendHandle])` and messages
are received by a `'message'` event on the child.

For example:

    var cp = require('child_process');

    var n = cp.fork(__dirname + '/sub.js');

    n.on('message', function(m) {
      console.log('PARENT got message:', m);
    });

    n.send({ hello: 'world' });

And then the child script, `'sub.js'` might look like this:

    process.on('message', function(m) {
      console.log('CHILD got message:', m);
    });

    process.send({ foo: 'bar' });

In the child the `process` object will have a `send()` method, and `process`
will emit objects each time it receives a message on its channel.

By default the spawned Node process will have the stdin, stdout, stderr
associated with the parent's.

These child Nodes are still whole new instances of V8. Assume at least 30ms
startup and 10mb memory for each new Node. That is, you cannot create many
thousands of them.

The `sendHandle` option to `child.send()` is for sending a handle object to
another process. Child will receive the handle as as second argument to the
`message` event. Here is an example of sending a handle:

    var server = require('net').createServer();
    var child = require('child_process').fork(__dirname + '/child.js');
    // Open up the server object and send the handle.
    server.listen(1337, function() {
      child.send({ server: true }, server._handle);
    });

Here is an example of receiving the server handle and sharing it between
processes:

    process.on('message', function(m, serverHandle) {
      if (serverHandle) {
        var server = require('net').createServer();
        server.listen(serverHandle);
      }
    });



