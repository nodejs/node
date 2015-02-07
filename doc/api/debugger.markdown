# Debugger

    Stability: 3 - Stable

<!-- type=misc -->

V8 comes with an extensive debugger which is accessible out-of-process via a
simple [TCP protocol](http://code.google.com/p/v8/wiki/DebuggerProtocol).
io.js has a built-in client for this debugger. To use this, start io.js with the
`debug` argument; a prompt will appear:

    % iojs debug myscript.js
    < debugger listening on port 5858
    connecting... ok
    break in /home/indutny/Code/git/indutny/myscript.js:1
      1 x = 5;
      2 setTimeout(function () {
      3   debugger;
    debug>

io.js's debugger client doesn't support the full range of commands, but
simple step and inspection is possible. By putting the statement `debugger;`
into the source code of your script, you will enable a breakpoint.

For example, suppose `myscript.js` looked like this:

    // myscript.js
    x = 5;
    setTimeout(function () {
      debugger;
      console.log("world");
    }, 1000);
    console.log("hello");

Then once the debugger is run, it will break on line 4.

    % iojs debug myscript.js
    < debugger listening on port 5858
    connecting... ok
    break in /home/indutny/Code/git/indutny/myscript.js:1
      1 x = 5;
      2 setTimeout(function () {
      3   debugger;
    debug> cont
    < hello
    break in /home/indutny/Code/git/indutny/myscript.js:3
      1 x = 5;
      2 setTimeout(function () {
      3   debugger;
      4   console.log("world");
      5 }, 1000);
    debug> next
    break in /home/indutny/Code/git/indutny/myscript.js:4
      2 setTimeout(function () {
      3   debugger;
      4   console.log("world");
      5 }, 1000);
      6 console.log("hello");
    debug> repl
    Press Ctrl + C to leave debug repl
    > x
    5
    > 2+2
    4
    debug> next
    < world
    break in /home/indutny/Code/git/indutny/myscript.js:5
      3   debugger;
      4   console.log("world");
      5 }, 1000);
      6 console.log("hello");
      7
    debug> quit
    %


The `repl` command allows you to evaluate code remotely. The `next` command
steps over to the next line. There are a few other commands available and more
to come. Type `help` to see others.

## Watchers

You can watch expression and variable values while debugging your code.
On every breakpoint each expression from the watchers list will be evaluated
in the current context and displayed just before the breakpoint's source code
listing.

To start watching an expression, type `watch("my_expression")`. `watchers`
prints the active watchers. To remove a watcher, type
`unwatch("my_expression")`.

## Commands reference

### Stepping

* `cont`, `c` - Continue execution
* `next`, `n` - Step next
* `step`, `s` - Step in
* `out`, `o` - Step out
* `pause` - Pause running code (like pause button in Developer Tools)

### Breakpoints

* `setBreakpoint()`, `sb()` - Set breakpoint on current line
* `setBreakpoint(line)`, `sb(line)` - Set breakpoint on specific line
* `setBreakpoint('fn()')`, `sb(...)` - Set breakpoint on a first statement in
functions body
* `setBreakpoint('script.js', 1)`, `sb(...)` - Set breakpoint on first line of
script.js
* `clearBreakpoint('script.js', 1)`, `cb(...)` - Clear breakpoint in script.js
on line 1

It is also possible to set a breakpoint in a file (module) that
isn't loaded yet:

    % ./iojs debug test/fixtures/break-in-module/main.js
    < debugger listening on port 5858
    connecting to port 5858... ok
    break in test/fixtures/break-in-module/main.js:1
      1 var mod = require('./mod.js');
      2 mod.hello();
      3 mod.hello();
    debug> setBreakpoint('mod.js', 23)
    Warning: script 'mod.js' was not loaded yet.
      1 var mod = require('./mod.js');
      2 mod.hello();
      3 mod.hello();
    debug> c
    break in test/fixtures/break-in-module/mod.js:23
     21
     22 exports.hello = function() {
     23   return 'hello from module';
     24 };
     25
    debug>

### Info

* `backtrace`, `bt` - Print backtrace of current execution frame
* `list(5)` - List scripts source code with 5 line context (5 lines before and
after)
* `watch(expr)` - Add expression to watch list
* `unwatch(expr)` - Remove expression from watch list
* `watchers` - List all watchers and their values (automatically listed on each
breakpoint)
* `repl` - Open debugger's repl for evaluation in debugging script's context

### Execution control

* `run` - Run script (automatically runs on debugger's start)
* `restart` - Restart script
* `kill` - Kill script

### Various

* `scripts` - List all loaded scripts
* `version` - Display v8's version

## Advanced Usage

The V8 debugger can be enabled and accessed either by starting io.js with
the `--debug` command-line flag or by signaling an existing io.js process
with `SIGUSR1`.

Once a process has been set in debug mode with this it can be connected to
with the io.js debugger. Either connect to the `pid` or the URI to the debugger.
The syntax is:

* `iojs debug -p <pid>` - Connects to the process via the `pid`
* `iojs debug <URI>` - Connects to the process via the URI such as localhost:5858
