# Command Line Options

<!--type=misc-->

Node.js comes with a variety of CLI options. These options expose built-in
debugging, multiple ways to execute scripts, and other helpful runtime options.

To view this documentation as a manual page in your terminal, run `man node`.


## Synopsis

`node [options] [v8 options] [script.js | -e "script"] [--] [arguments]`

`node debug [script.js | -e "script" | <host>:<port>] …`

`node --v8-options`

Execute without arguments to start the [REPL][].

_For more info about `node debug`, please see the [debugger][] documentation._


## Options

### `-v`, `--version`
<!-- YAML
added: v0.1.3
-->

Print node's version.


### `-h`, `--help`
<!-- YAML
added: v0.1.3
-->

Print node command line options.
The output of this option is less detailed than this document.


### `-e`, `--eval "script"`
<!-- YAML
added: v0.5.2
changes:
  - version: v5.11.0
    pr-url: https://github.com/nodejs/node/pull/5348
    description: Built-in libraries are now available as predefined variables.
-->

Evaluate the following argument as JavaScript. The modules which are
predefined in the REPL can also be used in `script`.


### `-p`, `--print "script"`
<!-- YAML
added: v0.6.4
changes:
  - version: v5.11.0
    pr-url: https://github.com/nodejs/node/pull/5348
    description: Built-in libraries are now available as predefined variables.
-->

Identical to `-e` but prints the result.


### `-c`, `--check`
<!-- YAML
added:
  - v5.0.0
  - v4.2.0
-->

Syntax check the script without executing.


### `-i`, `--interactive`
<!-- YAML
added: v0.7.7
-->

Opens the REPL even if stdin does not appear to be a terminal.


### `-r`, `--require module`
<!-- YAML
added: v1.6.0
-->

Preload the specified module at startup.

Follows `require()`'s module resolution
rules. `module` may be either a path to a file, or a node module name.


### `--inspect[=host:port]`
<!-- YAML
added: v6.3.0
-->

Activate inspector on host:port. Default is 127.0.0.1:9229.

V8 inspector integration allows tools such as Chrome DevTools and IDEs to debug
and profile Node.js instances. The tools attach to Node.js instances via a
tcp port and communicate using the [Chrome Debugging Protocol][].


### `--inspect-brk[=host:port]`
<!-- YAML
added: v7.6.0
-->

Activate inspector on host:port and break at start of user script.


### `--no-deprecation`
<!-- YAML
added: v0.8.0
-->

Silence deprecation warnings.


### `--trace-deprecation`
<!-- YAML
added: v0.8.0
-->

Print stack traces for deprecations.


### `--throw-deprecation`
<!-- YAML
added: v0.11.14
-->

Throw errors for deprecations.

### `--no-warnings`
<!-- YAML
added: v6.0.0
-->

Silence all process warnings (including deprecations).

### `--trace-warnings`
<!-- YAML
added: v6.0.0
-->

Print stack traces for process warnings (including deprecations).

### `--redirect-warnings=file`
<!-- YAML
added: REPLACEME
-->

Write process warnings to the given file instead of printing to stderr. The
file will be created if it does not exist, and will be appended to if it does.
If an error occurs while attempting to write the warning to the file, the
warning will be written to stderr instead.

### `--trace-sync-io`
<!-- YAML
added: v2.1.0
-->

Prints a stack trace whenever synchronous I/O is detected after the first turn
of the event loop.

### `--trace-events-enabled`
<!-- YAML
added: v7.7.0
-->

Enables the collection of trace event tracing information.

### `--trace-event-categories`
<!-- YAML
added: v7.7.0
-->

A comma separated list of categories that should be traced when trace event
tracing is enabled using `--trace-events-enabled`.

### `--zero-fill-buffers`
<!-- YAML
added: v6.0.0
-->

Automatically zero-fills all newly allocated [Buffer][] and [SlowBuffer][]
instances.


### `--preserve-symlinks`
<!-- YAML
added: v6.3.0
-->

Instructs the module loader to preserve symbolic links when resolving and
caching modules.

By default, when Node.js loads a module from a path that is symbolically linked
to a different on-disk location, Node.js will dereference the link and use the
actual on-disk "real path" of the module as both an identifier and as a root
path to locate other dependency modules. In most cases, this default behavior
is acceptable. However, when using symbolically linked peer dependencies, as
illustrated in the example below, the default behavior causes an exception to
be thrown if `moduleA` attempts to require `moduleB` as a peer dependency:

```text
{appDir}
 ├── app
 │   ├── index.js
 │   └── node_modules
 │       ├── moduleA -> {appDir}/moduleA
 │       └── moduleB
 │           ├── index.js
 │           └── package.json
 └── moduleA
     ├── index.js
     └── package.json
```

The `--preserve-symlinks` command line flag instructs Node.js to use the
symlink path for modules as opposed to the real path, allowing symbolically
linked peer dependencies to be found.

Note, however, that using `--preserve-symlinks` can have other side effects.
Specifically, symbolically linked *native* modules can fail to load if those
are linked from more than one location in the dependency tree (Node.js would
see those as two separate modules and would attempt to load the module multiple
times, causing an exception to be thrown).

### `--track-heap-objects`
<!-- YAML
added: v2.4.0
-->

Track heap object allocations for heap snapshots.


### `--prof-process`
<!-- YAML
added: v5.2.0
-->

Process v8 profiler output generated using the v8 option `--prof`.


### `--v8-options`
<!-- YAML
added: v0.1.3
-->

Print v8 command line options.

Note: v8 options allow words to be separated by both dashes (`-`) or underscores
(`_`).

For example, `--stack-trace-limit` is equivalent to `--stack_trace_limit`.

### `--tls-cipher-list=list`
<!-- YAML
added: v4.0.0
-->

Specify an alternative default TLS cipher list. (Requires Node.js to be built
with crypto support. (Default))


### `--enable-fips`
<!-- YAML
added: v6.0.0
-->

Enable FIPS-compliant crypto at startup. (Requires Node.js to be built with
`./configure --openssl-fips`)


### `--force-fips`
<!-- YAML
added: v6.0.0
-->

Force FIPS-compliant crypto on startup. (Cannot be disabled from script code.)
(Same requirements as `--enable-fips`)


### `--openssl-config=file`
<!-- YAML
added: v6.9.0
-->

Load an OpenSSL configuration file on startup. Among other uses, this can be
used to enable FIPS-compliant crypto if Node.js is built with
`./configure --openssl-fips`.

### `--use-openssl-ca`, `--use-bundled-ca`
<!-- YAML
added: v7.5.0
-->

Use OpenSSL's default CA store or use bundled Mozilla CA store as supplied by
current NodeJS version. The default store is selectable at build-time.

Using OpenSSL store allows for external modifications of the store. For most
Linux and BSD distributions, this store is maintained by the distribution
maintainers and system administrators. OpenSSL CA store location is dependent on
configuration of the OpenSSL library but this can be altered at runtime using
environmental variables.

The bundled CA store, as supplied by NodeJS, is a snapshot of Mozilla CA store
that is fixed at release time. It is identical on all supported platforms.

See `SSL_CERT_DIR` and `SSL_CERT_FILE`.

### `--icu-data-dir=file`
<!-- YAML
added: v0.11.15
-->

Specify ICU data load path. (overrides `NODE_ICU_DATA`)

### `--`
<!-- YAML
added: v7.5.0
-->

Indicate the end of node options. Pass the rest of the arguments to the script.
If no script filename or eval/print script is supplied prior to this, then
the next argument will be used as a script filename.

## Environment Variables

### `NODE_DEBUG=module[,…]`
<!-- YAML
added: v0.1.32
-->

`','`-separated list of core modules that should print debug information.


### `NODE_PATH=path[:…]`
<!-- YAML
added: v0.1.32
-->

`':'`-separated list of directories prefixed to the module search path.

_Note: on Windows, this is a `';'`-separated list instead._


### `NODE_DISABLE_COLORS=1`
<!-- YAML
added: v0.3.0
-->

When set to `1` colors will not be used in the REPL.


### `NODE_ICU_DATA=file`
<!-- YAML
added: v0.11.15
-->

Data path for ICU (Intl object) data. Will extend linked-in data when compiled
with small-icu support.

### `NODE_NO_WARNINGS=1`
<!-- YAML
added: v7.5.0
-->

When set to `1`, process warnings are silenced.

### `NODE_PRESERVE_SYMLINKS=1`
<!-- YAML
added: v7.1.0
-->

When set to `1`, instructs the module loader to preserve symbolic links when
resolving and caching modules.

### `NODE_REPL_HISTORY=file`
<!-- YAML
added: v3.0.0
-->

Path to the file used to store the persistent REPL history. The default path is
`~/.node_repl_history`, which is overridden by this variable. Setting the value
to an empty string (`""` or `" "`) disables persistent REPL history.


### `NODE_TTY_UNSAFE_ASYNC=1`
<!-- YAML
added: v6.4.0
-->

When set to `1`, writes to `stdout` and `stderr` will be non-blocking and
asynchronous when outputting to a TTY on platforms which support async stdio.
Setting this will void any guarantee that stdio will not be interleaved or
dropped at program exit. **Use of this mode is not recommended.**

### `NODE_EXTRA_CA_CERTS=file`
<!-- YAML
added: v7.3.0
-->

When set, the well known "root" CAs (like VeriSign) will be extended with the
extra certificates in `file`. The file should consist of one or more trusted
certificates in PEM format. A message will be emitted (once) with
[`process.emitWarning()`][emit_warning] if the file is missing or
malformed, but any errors are otherwise ignored.

Note that neither the well known nor extra certificates are used when the `ca`
options property is explicitly specified for a TLS or HTTPS client or server.

### `OPENSSL_CONF=file`
<!-- YAML
added: v7.7.0
-->

Load an OpenSSL configuration file on startup. Among other uses, this can be
used to enable FIPS-compliant crypto if Node.js is built with `./configure
\-\-openssl\-fips`.

If the [`--openssl-config`][] command line option is used, the environment
variable is ignored.

### `SSL_CERT_DIR=dir`
<!-- YAML
added: v7.7.0
-->

If `--use-openssl-ca` is enabled, this overrides and sets OpenSSL's directory
containing trusted certificates.

Note: Be aware that unless the child environment is explicitly set, this
evironment variable will be inherited by any child processes, and if they use
OpenSSL, it may cause them to trust the same CAs as node.

### `SSL_CERT_FILE=file`
<!-- YAML
added: v7.7.0
-->

If `--use-openssl-ca` is enabled, this overrides and sets OpenSSL's file
containing trusted certificates.

Note: Be aware that unless the child environment is explicitly set, this
evironment variable will be inherited by any child processes, and if they use
OpenSSL, it may cause them to trust the same CAs as node.

### `NODE_REDIRECT_WARNINGS=file`
<!-- YAML
added: REPLACEME
-->

When set, process warnings will be emitted to the given file instead of
printing to stderr. The file will be created if it does not exist, and will be
appended to if it does. If an error occurs while attempting to write the
warning to the file, the warning will be written to stderr instead. This is
equivalent to using the `--redirect-warnings=file` command-line flag.

[emit_warning]: process.html#process_process_emitwarning_warning_name_ctor
[Buffer]: buffer.html#buffer_buffer
[Chrome Debugging Protocol]: https://chromedevtools.github.io/debugger-protocol-viewer
[debugger]: debugger.html
[REPL]: repl.html
[SlowBuffer]: buffer.html#buffer_class_slowbuffer
[`--openssl-config`]: #cli_openssl_config_file
