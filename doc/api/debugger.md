# Debugger

<!--introduced_in=v0.9.12-->

> Stability: 2 - Stable

<!-- type=misc -->

Node.js includes a command-line debugging utility. The Node.js debugger client
is not a full-featured debugger, but simple stepping and inspection are
possible.

To use it, start Node.js with the `inspect` argument followed by the path to the
script to debug.

```console
$ node inspect myscript.js
< Debugger listening on ws://127.0.0.1:9229/621111f9-ffcb-4e82-b718-48a145fa5db8
< For help, see: https://nodejs.org/en/docs/inspector
<
connecting to 127.0.0.1:9229 ... ok
< Debugger attached.
<
 ok
Break on start in myscript.js:2
  1 // myscript.js
> 2 global.x = 5;
  3 setTimeout(() => {
  4   debugger;
debug>
```

The debugger automatically breaks on the first executable line. To instead
run until the first breakpoint (specified by a [`debugger`][] statement), set
the `NODE_INSPECT_RESUME_ON_START` environment variable to `1`.

```console
$ cat myscript.js
// myscript.js
global.x = 5;
setTimeout(() => {
  debugger;
  console.log('world');
}, 1000);
console.log('hello');
$ NODE_INSPECT_RESUME_ON_START=1 node inspect myscript.js
< Debugger listening on ws://127.0.0.1:9229/f1ed133e-7876-495b-83ae-c32c6fc319c2
< For help, see: https://nodejs.org/en/docs/inspector
<
connecting to 127.0.0.1:9229 ... ok
< Debugger attached.
<
< hello
<
break in myscript.js:4
  2 global.x = 5;
  3 setTimeout(() => {
> 4   debugger;
  5   console.log('world');
  6 }, 1000);
debug> next
break in myscript.js:5
  3 setTimeout(() => {
  4   debugger;
> 5   console.log('world');
  6 }, 1000);
  7 console.log('hello');
debug> repl
Press Ctrl+C to leave debug repl
> x
5
> 2 + 2
4
debug> next
< world
<
break in myscript.js:6
  4   debugger;
  5   console.log('world');
> 6 }, 1000);
  7 console.log('hello');
  8
debug> .exit
$
```

The `repl` command allows code to be evaluated remotely. The `next` command
steps to the next line. Type `help` to see what other commands are available.

Pressing `enter` without typing a command will repeat the previous debugger
command.

## Probe mode

<!-- YAML
added:
  - REPLACEME
-->

> Stability: 1 - Experimental

`node inspect` supports a non-interactive probe mode for inspecting runtime values
in an application via the flag `--probe`. Probe mode launches the application,
sets one or more source breakpoints, evaluates one expression whenever a
matching breakpoint is hit, and prints one final report when the session ends
(either on normal completion or timeout). This allows developers to perform
printf-style debugging without having to modify the application code and
clean up afterwards, and it supports structured output for tool use.

```console
$ node inspect [--json] [--preview] [--timeout=<ms>] [--port=<port>] \
    --probe app.js:10 --expr 'x' \
    [--probe app.js:20 --expr 'y' ...] \
    [--] [<node-option> ...] <script.js> [args...]
```

* `--probe <file>:<line>[:<col>]`: Source location to probe. Line and column number
  are 1-based.
* `--timeout=<ms>`: A global wall-clock deadline for the entire probe session.
  The default is `30000`. This can be used to probe a long-running application
  that can be terminated externally.
* `--json`: If used, prints a structured JSON report instead of the default text report.
* `--preview`: If used, non-primitive values will include CDP property previews for
  object-like JSON probe values.
* `--port=<port>`: Selects the local inspector port used for the `--inspect-brk`
  launch path. Probe mode defaults to `0`, which requests a random port.
* `--` is optional unless the child needs its own Node.js flags.

Additional rules about the `--probe` and `--expr` arguments:

* `--probe <file>:<line>[:<col>]` and `--expr <expr>` are strict pairs. Each
  `--probe` must be followed immediately by exactly one `--expr`.
* `--timeout`, `--json`, `--preview`, and `--port` are global probe options
  for the whole probe session. They may appear before or between probe pairs,
  but not between a `--probe` and its matching `--expr`.

If a single probe needs to evaluate more than one value,
evaluate a structured value in `--expr`, for example `--expr "{ foo, bar }"`
or `--expr "[foo, bar]"`, and use `--preview` to include property previews for
any object-like values in the output.

Probe mode only prints the final probe report to stdout, and otherwise silences
stdout/stderr from the child process. If the child exits with an error after the
probe session starts, the final report records a terminal `error` event with the
exit code and captured child stderr. Invalid arguments and fatal launch or
connect failures may still print diagnostics to stderr without a final probe
result.

Consider this script:

```js
// cli.js
let maxRSS = 0;
for (let i = 0; i < 2; i++) {
  const { rss } = process.memoryUsage();
  maxRSS = Math.max(maxRSS, rss);
}
```

If `--json` is not used, the output is printed in a human-readable text format:

```console
$ node inspect --probe cli.js:5 --expr 'rss' cli.js
Hit 1 at cli.js:5
  rss = 54935552
Hit 2 at cli.js:5
  rss = 55083008
Completed
```

Primitive results are printed directly, while objects and arrays use Chrome
DevTools Protocol preview data when available. Other non-primitive values
fall back to the Chrome DevTools Protocol `description` string.
Expression failures are recorded as `[error] ...` lines and do not fail
the overall session. If richer text formatting is needed, wrap the expression
in `JSON.stringify(...)` or `util.inspect(...)`.

When `--json` is used, the output shape looks like this:

```console
$ node inspect --json --probe cli.js:5 --expr 'rss' cli.js
{"v":1,"probes":[{"expr":"rss","target":["cli.js",5]}],"results":[{"probe":0,"event":"hit","hit":1,"result":{"type":"number","value":55443456,"description":"55443456"}},{"probe":0,"event":"hit","hit":2,"result":{"type":"number","value":55574528,"description":"55574528"}},{"event":"completed"}]}
```

```json
{
  "v": 1, // Probe JSON schema version.
  "probes": [
    {
      "expr": "rss", // The expression paired with --probe.
      "target": ["cli.js", 5] // [file, line] or [file, line, col].
    }
  ],
  "results": [
    {
      "probe": 0, // Index into probes[].
      "event": "hit", // Hit events are recorded in observation order.
      "hit": 1, // 1-based hit count for this probe.
      "result": {
        "type": "number",
        "value": 55443456,
        "description": "55443456"
      }
      // If the expression throws, "error" is present instead of "result".
    },
    {
      "probe": 0,
      "event": "hit",
      "hit": 2,
      "result": {
        "type": "number",
        "value": 55574528,
        "description": "55574528"
      }
    },
    {
      "event": "completed"
      // The final entry is always a terminal event, for example:
      // 1. { "event": "completed" }
      // 2. { "event": "miss", "pending": [0, 1] }
      // 3. {
      //      "event": "timeout",
      //      "pending": [0],
      //      "error": {
      //       "code": "probe_timeout",
      //       "message": "Timed out after 30000ms waiting for probes: app.js:10"
      //      }
      //    }
      // 4. {
      //      "event": "error",
      //      "pending": [0],
      //      "error": {
      //       "code": "probe_target_exit",
      //       "exitCode": 1,
      //       "stderr": "[Error: boom]",
      //       "message": "Target exited with code 1 before probes: app.js:10"
      //      }
      //    }
    }
  ]
}
```

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
< Debugger listening on ws://127.0.0.1:9229/48a5b28a-550c-471b-b5e1-d13dd7165df9
< For help, see: https://nodejs.org/en/docs/inspector
<
connecting to 127.0.0.1:9229 ... ok
< Debugger attached.
<
Break on start in main.js:1
> 1 const mod = require('./mod.js');
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
<
connecting to 127.0.0.1:9229 ... ok
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
* `unwatch(index)`: Remove expression at specific index from watch list
* `watchers`: List all watchers and their values (automatically listed on each
  breakpoint)
* `repl`: Open debugger's repl for evaluation in debugging script's context
* `exec expr`, `p expr`: Execute an expression in debugging script's context and
  print its value
* `profile`: Start CPU profiling session
* `profileEnd`: Stop current CPU profiling session
* `profiles`: List all completed CPU profiling sessions
* `profiles[n].save(filepath = 'node.cpuprofile')`: Save CPU profiling session
  to disk as JSON
* `takeHeapSnapshot(filepath = 'node.heapsnapshot')`: Take a heap snapshot
  and save to disk as JSON

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

Using the `--inspect` flag will execute the code immediately before debugger is connected.
This means that the code will start running before you can start debugging, which might
not be ideal if you want to debug from the very beginning.

In such cases, you have two alternatives:

1. `--inspect-wait` flag: This flag will wait for debugger to be attached before executing the code.
   This allows you to start debugging right from the beginning of the execution.
2. `--inspect-brk` flag: Unlike `--inspect`, this flag will break on the first line of the code
   as soon as debugger is attached. This is useful when you want to debug the code step by step
   from the very beginning, without any code execution prior to debugging.

So, when deciding between `--inspect`, `--inspect-wait`, and `--inspect-brk`, consider whether you want
the code to start executing immediately, wait for debugger to be attached before execution,
or break on the first line for step-by-step debugging.

```console
$ node --inspect index.js
Debugger listening on ws://127.0.0.1:9229/dc9010dd-f8b8-4ac5-a510-c1a114ec7d29
For help, see: https://nodejs.org/en/docs/inspector
```

(In the example above, the UUID dc9010dd-f8b8-4ac5-a510-c1a114ec7d29
at the end of the URL is generated on the fly, it varies in different
debugging sessions.)

[Chrome DevTools Protocol]: https://chromedevtools.github.io/devtools-protocol/
[`debugger`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/debugger
