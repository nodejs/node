# Debugger

<!--introduced_in=v0.9.12-->

> Stability: 2 - Stable

<!-- type=misc -->

Node.js includes a command-line debugging utility. The Node.js debugger client
is not a full-featured debugger, but simple stepping and inspection are
possible.

The debugger supports two modes of operation: [interactive mode][] and [non-interactive probe mode][].

## Interactive mode

```console
$ node inspect [--port=<port>] [<node-option> ...] [<script> [<script-args>] | <host>:<port> | -p <pid>]
```

To use it, start Node.js with the `inspect` argument followed by the path to the
script to debug.

```console
$ node inspect myscript.js
< Debugger listening on ws://127.0.0.1:9229/621111f9-ffcb-4e82-b718-48a145fa5db8
< For help, see: https://nodejs.org/learn/getting-started/debugging
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
< For help, see: https://nodejs.org/learn/getting-started/debugging
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

### Watchers

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

#### Breakpoints

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
< For help, see: https://nodejs.org/learn/getting-started/debugging
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
< For help, see: https://nodejs.org/learn/getting-started/debugging
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

#### Information

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

#### Execution control

* `run`: Run script (automatically runs on debugger's start)
* `restart`: Restart script
* `kill`: Kill script

#### Various

* `scripts`: List all loaded scripts
* `version`: Display V8's version

## Probe mode

<!-- YAML
added:
  - v26.1.0
  - v24.16.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/64328
    description: Add per-probe `--cond <expr>` option to only record a hit when the
        condition is truthy at the probe location.
  - version: v26.4.0
    pr-url: https://github.com/nodejs/node/pull/63704
    description: Add per-probe `--max-hit <n>` option to limit evaluated hits and finish
        with a `completed` terminal event as soon as any probe reaches its limit.
  - version: v26.3.0
    pr-url: https://github.com/nodejs/node/pull/63437
    description: Add `probe_failure` terminal `error` event for inspector-side mid-session
        failures, and `error.details` for additional context on per-hit and terminal errors.
  - version: v26.2.0
    pr-url: https://github.com/nodejs/node/pull/63286
    description: JSON report schema bumped to v2. Probe `target` is now
        `{ suffix, line, column? }` instead of an array. Each "hit" event carries a
        `location` field reporting the actual evaluation location.  When the probe does not
        specify a column, it binds to the first executable column instead of defaulting to 1.
-->

> Stability: 1 - Experimental

`node inspect` supports a non-interactive probe mode for inspecting runtime values
in an application via the flag `--probe`.

Currently, probe mode only supports launching a new process from the entry point
script specified on the command line.

The probe mode sets one or more source breakpoints, evaluates specified
expressions whenever the execution reaches a breakpoint, and prints one
final report of all the evaluated expressions when the session ends
(either on normal completion, error, or timeout). This allows developers to perform
printf-style debugging without having to modify the application code and
clean up afterwards. It also supports structured JSON output for tool use.

```console
$ node inspect --probe <file>:<line>[:<col>] --expr <expr> [--cond <expr>] [--max-hit <n>]
              [--probe <file>:<line>[:<col>] --expr <expr> [--cond <expr>] [--max-hit <n>] ...]
              [--json] [--preview] [--timeout=<ms>] [--port=<port>]
              [--] [<node-option> ...] <script> [<script-args> ...]
```

* `--probe <file>:<line>[:<col>]`: Source location of the probe. When execution
  reaches the location, the provided expressions are evaluated and printed in
  the output. `<file>` matches the URL suffix of the script to probe.
  `<line>` and `<col>` numbers are 1-based. When `<col>` is omitted, the probe
  binds to the first executable column on the line.
* `--expr <expr>`: JavaScript expression to evaluate whenever execution reaches
  the location specified by the preceding `--probe`.
  Must immediately follow the `--probe` it belongs to.
* `--cond <expr>`: An optional condition for the probe location. The probe only
  records a hit when `<expr>` is truthy at the location. A condition that throws
  is treated as false.
* `--max-hit <n>`: An optional per-probe limit on the number of times the probe
  can be hit. When not specified, there's no hit limit. When any probe reaches
  its hit limit, the probing process will detach and report the results. The process
  being probed will continue to run. If any other probe is never reached by the time
  the session ends, it will be reported as a missed probe.
* `--timeout=<ms>`: A global wall-clock deadline for the entire probe session.
  The default is `30000`. This can be used to probe a long-running application
  that can be terminated externally.
* `--json`: If used, prints a structured JSON report instead of the default text report.
* `--preview`: If used, non-primitive values will include CDP property previews for
  object-like JSON probe values.
* `--port=<port>`: Selects the local inspector port where the probing session
  will listen. Defaults to `0`, which requests a random port.
* `--` is optional unless the child needs its own Node.js flags.

Additional rules about the composition of the options:

* `--probe <file>:<line>[:<col>]` and `--expr <expr>` are strict pairs. Each
  `--probe` must be followed immediately by exactly one `--expr`.
* `--cond <expr>` and `--max-hit <n>` are optional modifiers written _after_ the
  `--probe`/`--expr` pair they apply to, each at most once per pair. They may not
  appear before the first `--probe` or between a `--probe` and its matching
  `--expr`.
* `--max-hit` scopes to the `--probe`/`--expr` pair it follows, so pairs
  sharing a location may set different limits. `--cond` scopes to the whole
  location, probes sharing a location must share one condition (or none).
* `--timeout`, `--json`, `--preview`, and `--port` are global probe options
  for the whole probe session. They may appear before or between probe pairs,
  but not between a `--probe` and its matching `--expr`.
* If additional Node.js execution arguments need to be passed to the child
  script, `--` must be used to separate the probe options from the Node.js
  options for the child script.

Example:

```console
$ node inspect --probe app.js:10 --expr "user"
               --probe src/utils.js:5:15 --expr "config.options"
               --json --preview -- --no-warnings app.js --arg-for-app=foo
```

### Probe output format

When the probe session ends, the probing process prints a final report of all the probe hits and results.

Consider this script:

```js
// cli.js
let maxRSS = 0;
for (let i = 0; i < 2; i++) {
  const { rss } = process.memoryUsage();
  maxRSS = Math.max(maxRSS, rss);
}
```

Without `--json`, by default the output is printed in a human-readable text format:

```console
$ node inspect --probe cli.js:5 --expr 'rss' cli.js
Hit 1 at file:///path/to/cli.js:5:3
  rss = 54935552
Hit 2 at file:///path/to/cli.js:5:3
  rss = 55083008
Completed
```

The original `<file>:<line>[:<col>]` passed to `--probe` may be resolved to a different
location to ensure it's pausable, or it can match multiple loaded scripts, so the actual
evaluation location helps disambiguate the results.

Primitive results are printed directly, while objects and arrays use Chrome
DevTools Protocol preview data when available. Other non-primitive values
fall back to the Chrome DevTools Protocol `description` string.
Expression failures are recorded as `[error] ...` lines and do not fail
the overall session. If richer text formatting is needed, wrap the expression
in `JSON.stringify(...)` or `util.inspect(...)`.

When `--json` is used, the output shape looks like this:

```console
$ node inspect --json --probe cli.js:5 --expr 'rss' cli.js
{"v":2,"probes":[{"expr":"rss","target":{"suffix":"cli.js","line":5}}],"results":[{"probe":0,"event":"hit","hit":1,"location":{"url":"file:///path/to/cli.js","line":5,"column":3},"result":{"type":"number","value":55443456,"description":"55443456"}},{"probe":0,"event":"hit","hit":2,"location":{"url":"file:///path/to/cli.js","line":5,"column":3},"result":{"type":"number","value":55574528,"description":"55574528"}},{"event":"completed"}]}
```

```json
{
  "v": 2, // Probe JSON schema version.
  "probes": [
    {
      "expr": "rss", // The expression paired with --probe.
      "target": {
        // The user's probe specification. `suffix` is the raw <file> passed
        // to --probe and is matched as a path-separator-anchored suffix
        // against every loaded script's URL. `column` is present only if the
        // user supplied `:col`. The actual evaluation location may differ
        // from the target and will be reported in each hit's `location` field.
        "suffix": "cli.js",
        "line": 5
      }
      // `condition` is present only when the probe was given a --cond expression.
      // `maxHit` is present only when the probe was given a --max-hit limit.
    }
  ],
  "results": [
    {
      "probe": 0, // Index into probes[].
      "event": "hit", // Hit events are recorded in observation order.
      "hit": 1, // 1-based hit count for this probe.
      "location": {
        // The actual location where the execution is paused to evaluate
        // the expression of the probe. This may differ from the probe's
        // target due to pausability adjustments or multiple matches.
        "url": "file:///path/to/cli.js",
        "line": 5,
        "column": 3
      },
      "result": {
        "type": "number",
        "value": 55443456,
        "description": "55443456"
      }
      // If the probe expression throws, fails, or never completes, the entry
      // carries an `error` field instead of `result` with the shape
      // `{ message: string, details?: object }`. The `message` and `details`
      // content is informational only and may change between releases.
    },
    {
      "probe": 0,
      "event": "hit",
      "hit": 2,
      "location": { "url": "file:///path/to/cli.js", "line": 5, "column": 3 },
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
      //       "stderr": "Error: boom",
      //       "message": "Target exited with code 1 before probes: app.js:10"
      //      }
      //    }
      // 5. {
      //      "event": "error",
      //      "pending": [1],
      //      "error": {
      //       "code": "probe_failure",
      //       "probe": 0,
      //       "stderr": "...",
      //       "message": "Target process exited during probe evaluation before probes: app.js:12. If the failure repeats, review the probe expression.",
      //       "details": { "lastCdpMethod": "Debugger.evaluateOnCallFrame" }
      //      }
      //    }
    }
  ]
}
```

### Output and exit codes

Probe mode only prints the final probe report to stdout, and otherwise silences
stdout/stderr from the child process. When the probing session ends,
the probing process typically exits with code `0` and prints a final report to
stdout. If the child process exits with a non-zero code before the probe
session ends, or the probe session cannot complete for another reason, the
final report records a terminal `error` event.

When `error.code` is `'probe_failure'` or `'probe_timeout'`, the probing process
exits with a non-zero code, indicating recorded hits may be incomplete.
In this case, `error.message` will contain recovery hints, and `error.probe`,
when present, is an index into the report's `probes` array that identifies
the possible culprit probe on a best-effort basis to help guide debugging.

Invalid arguments and fatal launch or connect failures may cause the
probing process to exit with a non-zero code and print an error message
to stderr without a final probe report.

### Probing multiple expressions at the same execution point

When multiple `--probe`/`--expr` pairs share the same `--probe`, the
expressions will be evaluated on the same pause in the order they appear
on the command line.

For each location, there can only be at most one `--cond` (or none).
Multiple `--probe`/`--expr` pairs with conflicting conditions
at the same location will be rejected at launch time.

```js
// app.js
const x = { x: 42 };       // line 2
const y = { y: 35 };       // line 3
const z = { ...x, ...y };  // line 4
```

```console
$ node inspect --probe app.js:4 --expr 'x' --probe app.js:4 --expr 'y' -- app.js
```

Prints

```text
Hit 1 at file:///path/to/app.js:4:1
  x = {x: 42}
Hit 1 at file:///path/to/app.js:4:1
  y = {y: 35}
Completed
```

```console
$ node inspect --probe app.js:4 --expr 'x' --probe app.js:4 --expr 'y' --json --preview -- app.js
```

Prints

```json
{"v":2,"probes":[{"expr":"x","target":{"suffix":"app.js","line":4}},{"expr":"y","target":{"suffix":"app.js","line":4}}],"results":[{"probe":0,"event":"hit","hit":1,"location":{"url":"file:///path/to/app.js","line":4,"column":1},"result":{"type":"object","description":"Object","preview":{"type":"object","description":"Object","overflow":false,"properties":[{"name":"x","type":"number","value":"42"}]}}},{"probe":1,"event":"hit","hit":1,"location":{"url":"file:///path/to/app.js","line":4,"column":1},"result":{"type":"object","description":"Object","preview":{"type":"object","description":"Object","overflow":false,"properties":[{"name":"y","type":"number","value":"35"}]}}},{"event":"completed"}]}
```

### Selecting the probe location

The expressions are evaluated in the lexical scope of the probe location when
execution reaches it. Avoid probing a variable declared by `let` or `const` at its
declaration site, as this leads to a `ReferenceError` caused by
accessing the variable in its temporal dead zone (TDZ).

```js
// app.js
const x = 42;        // line 2
console.log(x);      // line 3
```

```console
$ node inspect --probe app.js:1 --expr 'x' app.js
Hit 1 at file:///path/to/app.js:1:1
  [error] x = ReferenceError: Cannot access 'x' from debugger
  ...
Completed
```

Instead, probe at a location where the variable is already initialized:

```console
$ node inspect --probe app.js:3 --expr 'x' app.js
Hit 1 at file:///path/to/app.js:3:1
  x = 42
Completed
```

The `<file>` argument is matched as a path suffix of every loaded
script URL, anchored on a path separator. Passing only a basename
matches every loaded script with that basename, similar to how native
debuggers typically match breakpoints, while passing a partial path
narrows the match. Given:

```text
project/
  - src/utils.js
  - lib/utils.js
```

`--probe utils.js:10` binds to _both_ files and produces one hit per match.
Each hit carries its own `location` field identifying where the expression
was actually executed, so consumers can attribute the result to one of the
two files accurately. To disambiguate at bind time, specify a fuller path
that only matches the intended file:

```console
$ node inspect --probe src/utils.js:10 --expr 'x' main.js   # matches only src/utils.js
```

### Probe examples

#### Probing a variable conditionally

```js
// app.js
let total = 0;
for (let i = 0; i < 10; i++) {
  total += i;  // line 4
}
```

```console
$ out/Release/node inspect --probe app.js:4 --expr 'total' \
                           --cond 'i % 3 === 0' app.js
```

```text
Hit 1 at file:///path/to/app.js:3:3
  total = 0
Hit 2 at file:///path/to/app.js:3:3
  total = 3
Hit 3 at file:///path/to/app.js:3:3
  total = 15
Hit 4 at file:///path/to/app.js:3:3
  total = 36
Completed
```

<!-- TODO(joyeecheung): add more examples for different options -->

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
For help, see: https://nodejs.org/learn/getting-started/debugging
```

(In the example above, the UUID dc9010dd-f8b8-4ac5-a510-c1a114ec7d29
at the end of the URL is generated on the fly, it varies in different
debugging sessions.)

[Chrome DevTools Protocol]: https://chromedevtools.github.io/devtools-protocol/
[`debugger`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/debugger
[interactive mode]: #interactive-mode
[non-interactive probe mode]: #probe-mode
