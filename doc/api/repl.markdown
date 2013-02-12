# REPL

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

For advanced line-editors, start node with the environmental variable
`NODE_NO_READLINE=1`. This will start the main and debugger REPL in canonical
terminal settings which will allow you to use with `rlwrap`.

For example, you could add this to your bashrc file:

    alias node="env NODE_NO_READLINE=1 rlwrap node"


## repl.start(options)

Returns and starts a `REPLServer` instance. Accepts an "options" Object that
takes the following values:

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

You can use your own `eval` function if it has following signature:

    function eval(cmd, context, filename, callback) {
      callback(null, result);
    }

Multiple REPLs may be started against the same running instance of node.  Each
will share the same global object but will have unique I/O.

Here is an example that starts a REPL on stdin, a Unix socket, and a TCP socket:

    var net = require("net"),
        repl = require("repl");

    connections = 0;

    repl.start({
      prompt: "node via stdin> ",
      input: process.stdin,
      output: process.stdout
    });

    net.createServer(function (socket) {
      connections += 1;
      repl.start({
        prompt: "node via Unix socket> ",
        input: socket,
        output: socket
      }).on('exit', function() {
        socket.end();
      })
    }).listen("/tmp/node-repl-sock");

    net.createServer(function (socket) {
      connections += 1;
      repl.start({
        prompt: "node via TCP socket> ",
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
connect to a long-running node process without restarting it.

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


## REPL Features

<!-- type=misc -->

Inside the REPL, Control+D will exit.  Multi-line expressions can be input.
Tab completion is supported for both global and local variables.

The special variable `_` (underscore) contains the result of the last expression.

    > [ "a", "b", "c" ]
    [ 'a', 'b', 'c' ]
    > _.length
    3
    > _ += 1
    4

The REPL provides access to any variables in the global scope. You can expose
a variable to the REPL explicitly by assigning it to the `context` object
associated with each `REPLServer`.  For example:

    // repl_test.js
    var repl = require("repl"),
        msg = "message";

    repl.start("> ").context.m = msg;

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

