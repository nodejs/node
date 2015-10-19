# REPL

    Stability: 2 - Stable

A Read-Eval-Print-Loop (REPL) is available both as a standalone program and
easily includable in other programs. The REPL provides a way to interactively
run JavaScript and see the results.  It can be used for debugging, testing, or
just trying things out.

By executing `node` without any arguments from the command-line you will be
dropped into the REPL. It has simplistic emacs line-editing.

    mjr:~$ node
    Type '.help' for options.
    > a = [ 1, 2, 3];
    [ 1, 2, 3 ]
    > a.forEach(function (v) {
    ...   console.log(v);
    ...   });
    1
    2
    3

For advanced line-editors, start Node.js with the environmental variable
`NODE_NO_READLINE=1`. This will start the main and debugger REPL in canonical
terminal settings which will allow you to use with `rlwrap`.

For example, you could add this to your bashrc file:

    alias node="env NODE_NO_READLINE=1 rlwrap node"

## Persistent History

By default, the REPL will persist history between `node` REPL sessions by saving
to a `.node_repl_history` file in the user's home directory. This can be
disabled by setting the environment variable `NODE_REPL_HISTORY=""`.

### NODE_REPL_HISTORY_FILE

    Stability: 0 - Deprecated: Use `NODE_REPL_HISTORY` instead.

Previously in Node.js/io.js v2.x, REPL history was controlled by using a
`NODE_REPL_HISTORY_FILE` environment variable, and the history was saved in JSON
format. This variable has now been deprecated, and your REPL history will
automatically be converted to using plain text. The new file will be saved to
either your home directory, or a directory defined by the `NODE_REPL_HISTORY`
variable, as documented below.

## Environment Variable Options

The built-in repl (invoked by running `node` or `node -i`) may be controlled
via the following environment variables:

 - `NODE_REPL_HISTORY` - When a valid path is given, persistent REPL history
   will be saved to the specified file rather than `.node_repl_history` in the
   user's home directory. Setting this value to `""` will disable persistent
   REPL history.
 - `NODE_REPL_HISTORY_SIZE` - defaults to `1000`. Controls how many lines of
   history will be persisted if history is available. Must be a positive number.
 - `NODE_REPL_MODE` - may be any of `sloppy`, `strict`, or `magic`. Defaults
   to `magic`, which will automatically run "strict mode only" statements in
   strict mode.

## repl.start(options)

Returns and starts a `REPLServer` instance, that inherits from
[Readline Interface][]. Accepts an "options" Object that takes
the following values:

 - `prompt` - the prompt and `stream` for all I/O. Defaults to `> `.

 - `input` - the readable stream to listen to. Defaults to `process.stdin`.

 - `output` - the writable stream to write readline data to. Defaults to
   `process.stdout`.

 - `terminal` - pass `true` if the `stream` should be treated like a TTY, and
   have ANSI/VT100 escape codes written to it. Defaults to checking `isTTY`
   on the `output` stream upon instantiation.

 - `eval` - function that will be used to eval each given line. Defaults to
   an async wrapper for `eval()`. See below for an example of a custom `eval`.

 - `useColors` - a boolean which specifies whether or not the `writer` function
   should output colors. If a different `writer` function is set then this does
   nothing. Defaults to the repl's `terminal` value.

 - `useGlobal` - if set to `true`, then the repl will use the `global` object,
   instead of running scripts in a separate context. Defaults to `false`.

 - `ignoreUndefined` - if set to `true`, then the repl will not output the
   return value of command if it's `undefined`. Defaults to `false`.

 - `writer` - the function to invoke for each command that gets evaluated which
   returns the formatting (including coloring) to display. Defaults to
   `util.inspect`.

 - `replMode` - controls whether the repl runs all commands in strict mode,
   default mode, or a hybrid mode ("magic" mode.) Acceptable values are:
  * `repl.REPL_MODE_SLOPPY` - run commands in sloppy mode.
  * `repl.REPL_MODE_STRICT` - run commands in strict mode. This is equivalent to
  prefacing every repl statement with `'use strict'`.
  * `repl.REPL_MODE_MAGIC` - attempt to run commands in default mode. If they
  fail to parse, re-try in strict mode.

You can use your own `eval` function if it has following signature:

    function eval(cmd, context, filename, callback) {
      callback(null, result);
    }

On tab completion - `eval` will be called with `.scope` as an input string. It
is expected to return an array of scope names to be used for the auto-completion.

Multiple REPLs may be started against the same running instance of Node.js.  Each
will share the same global object but will have unique I/O.

Here is an example that starts a REPL on stdin, a Unix socket, and a TCP socket:

    var net = require('net'),
        repl = require('repl'),
        connections = 0;

    repl.start({
      prompt: 'Node.js via stdin> ',
      input: process.stdin,
      output: process.stdout
    });

    net.createServer(function (socket) {
      connections += 1;
      repl.start({
        prompt: 'Node.js via Unix socket> ',
        input: socket,
        output: socket
      }).on('exit', function() {
        socket.end();
      })
    }).listen('/tmp/node-repl-sock');

    net.createServer(function (socket) {
      connections += 1;
      repl.start({
        prompt: 'Node.js via TCP socket> ',
        input: socket,
        output: socket
      }).on('exit', function() {
        socket.end();
      });
    }).listen(5001);

Running this program from the command line will start a REPL on stdin.  Other
REPL clients may connect through the Unix socket or TCP socket. `telnet` is useful
for connecting to TCP sockets, and `socat` can be used to connect to both Unix and
TCP sockets.

By starting a REPL from a Unix socket-based server instead of stdin, you can
connect to a long-running Node.js process without restarting it.

For an example of running a "full-featured" (`terminal`) REPL over
a `net.Server` and `net.Socket` instance, see: https://gist.github.com/2209310

For an example of running a REPL instance over `curl(1)`,
see: https://gist.github.com/2053342

### Event: 'exit'

`function () {}`

Emitted when the user exits the REPL in any of the defined ways. Namely, typing
`.exit` at the repl, pressing Ctrl+C twice to signal SIGINT, or pressing Ctrl+D
to signal "end" on the `input` stream.

Example of listening for `exit`:

    r.on('exit', function () {
      console.log('Got "exit" event from repl!');
      process.exit();
    });


### Event: 'reset'

`function (context) {}`

Emitted when the REPL's context is reset. This happens when you type `.clear`.
If you start the repl with `{ useGlobal: true }` then this event will never
be emitted.

Example of listening for `reset`:

    // Extend the initial repl context.
    var r = repl.start({ options ... });
    someExtension.extend(r.context);

    // When a new context is created extend it as well.
    r.on('reset', function (context) {
      console.log('repl has a new context');
      someExtension.extend(context);
    });


## REPL Features

<!-- type=misc -->

Inside the REPL, Control+D will exit.  Multi-line expressions can be input.
Tab completion is supported for both global and local variables.

Core modules will be loaded on-demand into the environment. For example,
accessing `fs` will `require()` the `fs` module as `global.fs`.

The special variable `_` (underscore) contains the result of the last expression.

    > [ 'a', 'b', 'c' ]
    [ 'a', 'b', 'c' ]
    > _.length
    3
    > _ += 1
    4

The REPL provides access to any variables in the global scope. You can expose
a variable to the REPL explicitly by assigning it to the `context` object
associated with each `REPLServer`.  For example:

    // repl_test.js
    var repl = require('repl'),
        msg = 'message';

    repl.start('> ').context.m = msg;

Things in the `context` object appear as local within the REPL:

    mjr:~$ node repl_test.js
    > m
    'message'

There are a few special REPL commands:

  - `.break` - While inputting a multi-line expression, sometimes you get lost
    or just don't care about completing it. `.break` will start over.
  - `.clear` - Resets the `context` object to an empty object and clears any
    multi-line expression.
  - `.exit` - Close the I/O stream, which will cause the REPL to exit.
  - `.help` - Show this list of special commands.
  - `.save` - Save the current REPL session to a file
    >.save ./file/to/save.js
  - `.load` - Load a file into the current REPL session.
    >.load ./file/to/load.js

The following key combinations in the REPL have these special effects:

  - `<ctrl>C` - Similar to the `.break` keyword.  Terminates the current
    command.  Press twice on a blank line to forcibly exit.
  - `<ctrl>D` - Similar to the `.exit` keyword.
  - `<tab>` - Show both global and local(scope) variables


### Customizing Object displays in the REPL

The REPL module internally uses
[util.inspect()][], when printing values. However, `util.inspect` delegates the
 call to the object's `inspect()` function, if it has one. You can read more
 about this delegation [here][].

For example, if you have defined an `inspect()` function on an object, like this:

    > var obj = { foo: 'this will not show up in the inspect() output' };
    undefined
    > obj.inspect = function() {
    ...   return { bar: 'baz' };
    ... };
    [Function]

and try to print `obj` in REPL, it will invoke the custom `inspect()` function:

    > obj
    { bar: 'baz' }

[Readline Interface]: readline.html#readline_class_interface
[util.inspect()]: util.html#util_util_inspect_object_options
[here]: util.html#util_custom_inspect_function_on_objects
