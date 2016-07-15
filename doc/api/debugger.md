# Debugger

> Stability: 2 - Stable

<!-- type=misc -->

Node.js includes a full-featured out-of-process debugging utility accessible
via a simple [TCP-based protocol][] and built-in debugging client. To use it,
start Node.js with the `debug` argument followed by the path to the script to
debug; a prompt will be displayed indicating successful launch of the debugger:

```txt
$ node debug myscript.js
< debugger listening on port 5858
connecting... ok
break in /home/indutny/Code/git/indutny/myscript.js:1
  1 x = 5;
  2 setTimeout(() => {
  3   debugger;
debug>
```

Node.js's debugger client is not a full-featured debugger, but simple step and
inspection are possible.

Inserting the statement `debugger;` into the source code of a script will
enable a breakpoint at that position in the code:

```js
// myscript.js
x = 5;
setTimeout(() => {
  debugger;
  console.log('world');
}, 1000);
console.log('hello');
```

Once the debugger is run, a breakpoint will occur at line 4:

```txt
$ node debug myscript.js
< debugger listening on port 5858
connecting... ok
break in /home/indutny/Code/git/indutny/myscript.js:1
  1 x = 5;
  2 setTimeout(() => {
  3   debugger;
debug> cont
< hello
break in /home/indutny/Code/git/indutny/myscript.js:3
  1 x = 5;
  2 setTimeout(() => {
  3   debugger;
  4   console.log('world');
  5 }, 1000);
debug> next
break in /home/indutny/Code/git/indutny/myscript.js:4
  2 setTimeout(() => {
  3   debugger;
  4   console.log('world');
  5 }, 1000);
  6 console.log('hello');
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
  4   console.log('world');
  5 }, 1000);
  6 console.log('hello');
  7
debug> quit
```

The `repl` command allows code to be evaluated remotely. The `next` command
steps to the next line. Type `help` to see what other commands are available.

Pressing `enter` without typing a command will repeat the previous debugger
command.

## Watchers

It is possible to watch expression and variable values while debugging. On
every breakpoint, each expression from the watchers list will be evaluated
in the current context and displayed immediately before the breakpoint's
source code listing.

To begin watching an expression, type `watch('my_expression')`. The command
`watchers` will print the active watchers. To remove a watcher, type
`unwatch('my_expression')`.

## Command reference

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

```txt
$ ./node debug test/fixtures/break-in-module/main.js
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
 22 exports.hello = () => {
 23   return 'hello from module';
 24 };
 25
debug>
```

### Information

* `backtrace`, `bt` - Print backtrace of current execution frame
* `list(5)` - List scripts source code with 5 line context (5 lines before and
after)
* `watch(expr)` - Add expression to watch list
* `unwatch(expr)` - Remove expression from watch list
* `watchers` - List all watchers and their values (automatically listed on each
breakpoint)
* `repl` - Open debugger's repl for evaluation in debugging script's context
* `exec expr` - Execute an expression in debugging script's context

### Execution control

* `run` - Run script (automatically runs on debugger's start)
* `restart` - Restart script
* `kill` - Kill script

### Various

* `scripts` - List all loaded scripts
* `version` - Display V8's version

## Advanced Usage

An alternative way of enabling and accessing the debugger is to start
Node.js with the `--debug` command-line flag or by signaling an existing
Node.js process with `SIGUSR1`.

Once a process has been set in debug mode this way, it can be inspected
using the Node.js debugger by either connecting to the `pid` of the running
process or via URI reference to the listening debugger:

* `node debug -p <pid>` - Connects to the process via the `pid`
* `node debug <URI>` - Connects to the process via the URI such as
localhost:5858

## V8 Inspector Integration for Node.js

__NOTE: This is an experimental feature.__

V8 Inspector integration allows attaching Chrome DevTools to Node.js
instances for debugging and profiling.

V8 Inspector can be enabled by passing the `--inspect` flag when starting a
Node.js application. It is also possible to supply a custom port with that flag,
e.g. `--inspect=9222` will accept DevTools connections on port 9222.

To break on the first line of the application code, provide the `--debug-brk`
flag in addition to `--inspect`.

[TCP-based protocol]: https://github.com/v8/v8/wiki/Debugging-Protocol
