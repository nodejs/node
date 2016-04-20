# Command Line Options

<!--type=misc-->

Node.js comes with a wide variety of CLI options. These options expose built-in
debugging, multiple ways to execute scripts, and other helpful runtime options.

To view this documentation as a manual page in your terminal, run `man node`.


## Synopsis

`node [options] [v8 options] [script.js | -e "script"] [arguments]`

`node debug [script.js | -e "script" | <host>:<port>] …`

`node --v8-options`

Execute without arguments to start the [REPL][].

_For more info about `node debug`, please see the [debugger][] documentation._


## Options

### `-v`, `--version`

Print node's version.


### `-h`, `--help`

Print node command line options.
The output of this option is less detailed than this document.


### `-e`, `--eval "script"`

Evaluate the following argument as JavaScript. The modules which are
predefined in the REPL can also be used in `script`.


### `-p`, `--print "script"`

Identical to `-e` but prints the result.


### `-c`, `--check`

Syntax check the script without executing.


### `-i`, `--interactive`

Opens the REPL even if stdin does not appear to be a terminal.


### `-r`, `--require module`

Preload the specified module at startup.

Follows `require()`'s module resolution
rules. `module` may be either a path to a file, or a node module name.


### `--no-deprecation`

Silence deprecation warnings.


### `--trace-deprecation`

Print stack traces for deprecations.


### `--throw-deprecation`

Throw errors for deprecations.


### `--trace-sync-io`

Prints a stack trace whenever synchronous I/O is detected after the first turn
of the event loop.


### `--zero-fill-buffers`

Automatically zero-fills all newly allocated [Buffer][] and [SlowBuffer][]
instances.


### `--track-heap-objects`

Track heap object allocations for heap snapshots.

### `--zero-fill-buffers`

Automatically zero-fills all newly allocated Buffer and SlowBuffer instances.

### `--prof-process`

Process v8 profiler output generated using the v8 option `--prof`.


### `--v8-options`

Print v8 command line options.


### `--tls-cipher-list=list`

Specify an alternative default TLS cipher list. (Requires Node.js to be built
with crypto support. (Default))


### `--enable-fips`

Enable FIPS-compliant crypto at startup. (Requires Node.js to be built with
`./configure --openssl-fips`)


### `--force-fips`

Force FIPS-compliant crypto on startup. (Cannot be disabled from script code.)
(Same requirements as `--enable-fips`)


### `--icu-data-dir=file`

Specify ICU data load path. (overrides `NODE_ICU_DATA`)


## Environment Variables

### `NODE_DEBUG=module[,…]`

`','`-separated list of core modules that should print debug information.


### `NODE_PATH=path[:…]`

`':'`-separated list of directories prefixed to the module search path.

_Note: on Windows, this is a `';'`-separated list instead._


### `NODE_DISABLE_COLORS=1`

When set to `1` colors will not be used in the REPL.


### `NODE_ICU_DATA=file`

Data path for ICU (Intl object) data. Will extend linked-in data when compiled
with small-icu support.


### `NODE_REPL_HISTORY=file`

Path to the file used to store the persistent REPL history. The default path is
`~/.node_repl_history`, which is overridden by this variable. Setting the value
to an empty string (`""` or `" "`) disables persistent REPL history.


[Buffer]: buffer.html#buffer_buffer
[debugger]: debugger.html
[REPL]: repl.html
[SlowBuffer]: buffer.html#buffer_class_slowbuffer
