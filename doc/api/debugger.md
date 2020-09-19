# Debugger

<!--introduced_in=v0.9.12-->

> Stability: 2 - Stable

<!-- type=misc -->

Node.js includes an out-of-process debugging utility accessible via a
[V8 Inspector][] and built-in debugging client. To use it, start Node.js
with the `inspect` argument followed by the path to the script to debug; a
prompt will be displayed indicating successful launch of the debugger:

```console
$ node inspect myscript.js
< Debugger listening on ws://127.0.0.1:9229/80e7a814-7cd3-49fb-921a-2e02228cd5ba
< For help, see: https://nodejs.org/en/docs/inspector
< Debugger attached.
Break on start in myscript.js:1
> 1 (function (exports, require, module, __filename, __dirname) { global.x = 5;
  2 setTimeout(() => {
  3   console.log('world');
debug>
```

The Node.js debugger client is not a full-featured debugger, but simple step and
inspection are possible.

Inserting the statement `debugger;` into the source code of a script will
enable a breakpoint at that position in the code:

<!-- eslint-disable no-debugger -->
```js
// myscript.js
global.x = 5;
setTimeout(() => {
  debugger;
  console.log('world');
}, 1000);
console.log('hello');
```

Once the debugger is run, a breakpoint will occur at line 3:

```console
$ node inspect myscript.js
< Debugger listening on ws://127.0.0.1:9229/80e7a814-7cd3-49fb-921a-2e02228cd5ba
< For help, see: https://nodejs.org/en/docs/inspector
< Debugger attached.
Break on start in myscript.js:1
> 1 (function (exports, require, module, __filename, __dirname) { global.x = 5;
  2 setTimeout(() => {
  3   debugger;
debug> cont
< hello
break in myscript.js:3
  1 (function (exports, require, module, __filename, __dirname) { global.x = 5;
  2 setTimeout(() => {
> 3   debugger;
  4   console.log('world');
  5 }, 1000);
debug> next
break in myscript.js:4
  2 setTimeout(() => {
  3   debugger;
> 4   console.log('world');
  5 }, 1000);
  6 console.log('hello');
debug> repl
Press Ctrl+C to leave debug repl
> x
5
> 2 + 2
4
debug> next
< world
break in myscript.js:5
  3   debugger;
  4   console.log('world');
> 5 }, 1000);
  6 console.log('hello');
  7
debug> .exit
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

* `cont`, `c`: Continue execution
* `next`, `n`: Step next
* `step`, `s`: Step in
* `out`, `o`: Step out
* `pause`: Pause running code (like pause button in Developer Tools)

### Breakpoints

* `setBreakpoint()`, `sb()`: Set breakpoint on current line
* `setBreakpoint(line)`, `sb(line)`: Set breakpoint on specific line
* `setBreakpoint('fn()')`, `sb(...)`: Set breakpoint on a first statement in
  function's body
* `setBreakpoint('script.js', 1)`, `sb(...)`: Set breakpoint on first line of
  `script.js`
* `setBreakpoint('script.js', 1, 'num < 4')`, `sb(...)`: Set conditional
  breakpoint on first line of `script.js` that only breaks when `num < 4`
  evaluates to `true`
* `clearBreakpoint('script.js', 1)`, `cb(...)`: Clear breakpoint in `script.js`
  on line 1

It is also possible to set a breakpoint in a file (module) that
is not loaded yet:

```console
$ node inspect main.js
< Debugger listening on ws://127.0.0.1:9229/4e3db158-9791-4274-8909-914f7facf3bd
< For help, see: https://nodejs.org/en/docs/inspector
< Debugger attached.
Break on start in main.js:1
> 1 (function (exports, require, module, __filename, __dirname) { const mod = require('./mod.js');
  2 mod.hello();
  3 mod.hello();
debug> setBreakpoint('mod.js', 22)
Warning: script 'mod.js' was not loaded yet.
debug> c
break in mod.js:22
 20 // USE OR OTHER DEALINGS IN THE SOFTWARE.
 21
>22 exports.hello = function() {
 23   return 'hello from module';
 24 };
debug>
```

It is also possible to set a conditional breakpoint that only breaks when a
given expression evaluates to `true`:

```console
$ node inspect main.js
< Debugger listening on ws://127.0.0.1:9229/ce24daa8-3816-44d4-b8ab-8273c8a66d35
< For help, see: https://nodejs.org/en/docs/inspector
< Debugger attached.
Break on start in main.js:7
  5 }
  6
> 7 addOne(10);
  8 addOne(-1);
  9
debug> setBreakpoint('main.js', 4, 'num < 0')
  1 'use strict';
  2
  3 function addOne(num) {
> 4   return num + 1;
  5 }
  6
  7 addOne(10);
  8 addOne(-1);
  9
debug> cont
break in main.js:4
  2
  3 function addOne(num) {
> 4   return num + 1;
  5 }
  6
debug> exec('num')
-1
debug>
```

### Information

* `backtrace`, `bt`: Print backtrace of current execution frame
* `list(5)`: List scripts source code with 5 line context (5 lines before and
  after)
* `watch(expr)`: Add expression to watch list
* `unwatch(expr)`: Remove expression from watch list
* `watchers`: List all watchers and their values (automatically listed on each
  breakpoint)
* `repl`: Open debugger's repl for evaluation in debugging script's context
* `exec expr`: Execute an expression in debugging script's context

### Execution control

* `run`: Run script (automatically runs on debugger's start)
* `restart`: Restart script
* `kill`: Kill script

### Various

* `scripts`: List all loaded scripts
* `version`: Display V8's version

## Advanced usage

### V8 inspector integration for Node.js

V8 Inspector integration allows attaching Chrome DevTools to Node.js
instances for debugging and profiling. It uses the
[Chrome DevTools Protocol][].

V8 Inspector can be enabled by passing the `--inspect` flag when starting a
Node.js application. It is also possible to supply a custom port with that flag,
e.g. `--inspect=9222` will accept DevTools connections on port 9222.

To break on the first line of the application code, pass the `--inspect-brk`
flag instead of `--inspect`.

```console
$ node --inspect index.js
Debugger listening on ws://127.0.0.1:9229/dc9010dd-f8b8-4ac5-a510-c1a114ec7d29
For help, see: https://nodejs.org/en/docs/inspector
```

(In the example above, the UUID dc9010dd-f8b8-4ac5-a510-c1a114ec7d29
at the end of the URL is generated on the fly, it varies in different
debugging sessions.)

If the Chrome browser is older than 66.0.3345.0,
use `inspector.html` instead of `js_app.html` in the above URL.

Chrome DevTools doesn't support debugging [worker threads][] yet.
[ndb][] can be used to debug them.

[Chrome DevTools Protocol]: https://chromedevtools.github.io/devtools-protocol/
[V8 Inspector]: #debugger_v8_inspector_integration_for_node_js
[ndb]: https://github.com/GoogleChromeLabs/ndb/
[worker threads]: worker_threads.md
