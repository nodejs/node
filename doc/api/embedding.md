# C++ embedder API

<!--introduced_in=v12.19.0-->

Node.js provides a number of C++ APIs that can be used to execute JavaScript
in a Node.js environment from other C++ software.

The documentation for these APIs can be found in [src/node.h][] in the Node.js
source tree. In addition to the APIs exposed by Node.js, some required concepts
are provided by the V8 embedder API.

Because using Node.js as an embedded library is different from writing code
that is executed by Node.js, breaking changes do not follow typical Node.js
[deprecation policy][] and may occur on each semver-major release without prior
warning.

## Example embedding application

The following sections will provide an overview over how to use these APIs
to create an application from scratch that will perform the equivalent of
`node -e <code>`, i.e. that will take a piece of JavaScript and run it in
a Node.js-specific environment.

The full code can be found [in the Node.js source tree][embedtest.cc].

### Setting up a per-process state

Node.js requires some per-process state management in order to run:

- Arguments parsing for Node.js [CLI options][],
- V8 per-process requirements, such as a `v8::Platform` instance.

The following example shows how these can be set up. Some class names are from
the `node` and `v8` C++ namespaces, respectively.

```cpp
int main(int argc, char** argv) {
  argv = uv_setup_args(argc, argv);
  std::vector<std::string> args(argv, argv + argc);
  // Parse Node.js CLI options, and print any errors that have occurred while
  // trying to parse them.
  std::unique_ptr<node::InitializationResult> result =
      node::InitializeOncePerProcess(args, {
        node::ProcessInitializationFlags::kNoInitializeV8,
        node::ProcessInitializationFlags::kNoInitializeNodeV8Platform
      });

  for (const std::string& error : result->errors())
    fprintf(stderr, "%s: %s\n", args[0].c_str(), error.c_str());
  if (result->early_return() != 0) {
    return result->exit_code();
  }

  // Create a v8::Platform instance. `MultiIsolatePlatform::Create()` is a way
  // to create a v8::Platform instance that Node.js can use when creating
  // Worker threads. When no `MultiIsolatePlatform` instance is present,
  // Worker threads are disabled.
  std::unique_ptr<MultiIsolatePlatform> platform =
      MultiIsolatePlatform::Create(4);
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  // See below for the contents of this function.
  int ret = RunNodeInstance(
      platform.get(), result->args(), result->exec_args());

  V8::Dispose();
  V8::DisposePlatform();

  node::TearDownOncePerProcess();
  return ret;
}
```

### Setting up a per-instance state

<!-- YAML
changes:
  - version: v15.0.0
    pr-url: https://github.com/nodejs/node/pull/35597
    description:
      The `CommonEnvironmentSetup` and `SpinEventLoop` utilities were added.
-->

Node.js has a concept of a “Node.js instance”, that is commonly being referred
to as `node::Environment`. Each `node::Environment` is associated with:

- Exactly one `v8::Isolate`, i.e. one JS Engine instance,
- Exactly one `uv_loop_t`, i.e. one event loop,
- A number of `v8::Context`s, but exactly one main `v8::Context`, and
- One `node::IsolateData` instance that contains information that could be
  shared by multiple `node::Environment`s. The embedder should make sure
  that `node::IsolateData` is shared only among `node::Environment`s that
  use the same `v8::Isolate`, Node.js does not perform this check.

In order to set up a `v8::Isolate`, an `v8::ArrayBuffer::Allocator` needs
to be provided. One possible choice is the default Node.js allocator, which
can be created through `node::ArrayBufferAllocator::Create()`. Using the Node.js
allocator allows minor performance optimizations when addons use the Node.js
C++ `Buffer` API, and is required in order to track `ArrayBuffer` memory in
[`process.memoryUsage()`][].

Additionally, each `v8::Isolate` that is used for a Node.js instance needs to
be registered and unregistered with the `MultiIsolatePlatform` instance, if one
is being used, in order for the platform to know which event loop to use
for tasks scheduled by the `v8::Isolate`.

The `node::NewIsolate()` helper function creates a `v8::Isolate`,
sets it up with some Node.js-specific hooks (e.g. the Node.js error handler),
and registers it with the platform automatically.

```cpp
int RunNodeInstance(MultiIsolatePlatform* platform,
                    const std::vector<std::string>& args,
                    const std::vector<std::string>& exec_args) {
  int exit_code = 0;

  // Setup up a libuv event loop, v8::Isolate, and Node.js Environment.
  std::vector<std::string> errors;
  std::unique_ptr<CommonEnvironmentSetup> setup =
      CommonEnvironmentSetup::Create(platform, &errors, args, exec_args);
  if (!setup) {
    for (const std::string& err : errors)
      fprintf(stderr, "%s: %s\n", args[0].c_str(), err.c_str());
    return 1;
  }

  Isolate* isolate = setup->isolate();
  Environment* env = setup->env();

  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    // The v8::Context needs to be entered when node::CreateEnvironment() and
    // node::LoadEnvironment() are being called.
    Context::Scope context_scope(setup->context());

    // Set up the Node.js instance for execution, and run code inside of it.
    // There is also a variant that takes a callback and provides it with
    // the `require` and `process` objects, so that it can manually compile
    // and run scripts as needed.
    // The `require` function inside this script does *not* access the file
    // system, and can only load built-in Node.js modules.
    // `module.createRequire()` is being used to create one that is able to
    // load files from the disk, and uses the standard CommonJS file loader
    // instead of the internal-only `require` function.
    MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
        env,
        "const publicRequire ="
        "  require('node:module').createRequire(process.cwd() + '/');"
        "globalThis.require = publicRequire;"
        "require('node:vm').runInThisContext(process.argv[1]);");

    if (loadenv_ret.IsEmpty())  // There has been a JS exception.
      return 1;

    exit_code = node::SpinEventLoop(env).FromMaybe(1);

    // node::Stop() can be used to explicitly stop the event loop and keep
    // further JavaScript from running. It can be called from any thread,
    // and will act like worker.terminate() if called from another thread.
    node::Stop(env);
  }

  return exit_code;
}
```

# C embedder API

<!--introduced_in=REPLACEME-->

While Node.js provides an extensive C++ embedding API that can be used from C++
applications, the C-based API is useful when Node.js is embedded as a shared
libnode library into C++ or non-C++ applications.

The C embedding API is defined in [src/node_embedding_api.h][] in the Node.js
source tree.

## API design overview

One of the goals for the C based embedder API is to be ABI stable. It means that
applications must be able to use newer libnode versions without recompilation.
The following design principles are targeting to achieve that goal.

- Follow the best practices for the [node-api][] design and build on top of
  the [node-api][].
- Use the [Builder pattern][] as the way to configure the global platform and
  the instance environments. It enables us incrementally add new flags,
  settings, callbacks, and behavior without changing the existing
  functions.
- Use the API version as a way to add new or change existing behavior.
- Make the common scenarios simple and the complex scenarios possible. In some
  cases we may provide some "shortcut" APIs that combine calls to multiple other
  APIs to simplify some common scenarios.

The C embedder API has the four major API function groups:

- **Global platform APIs.** These are the global settings and initializations
  that are done once per process. They include parsing CLI arguments, setting
  the V8 platform, V8 thread pool, and initializing V8.
- **Runtime instance APIs.** This is the main Node.js working environment that
  combines V8 `Isolate`, `Context`, and a UV loop. It is used run the Node.js
  JavaScript code and modules. Each process we may have one or more runtime
  environments. Its behavior is based on the global platform API.
- **Event loop APIs.** The event loop is one of the key concepts of Node.js. It
  handles IO callbacks, timer jobs, and Promise continuations. These APIs are
  related to a specific Node.js runtime instance and control handling of the
  event loop tasks. The event loop tasks can be executed in the chosen thread.
  The API controls how many tasks executed at one: all, one-by-one, or until a
  predicate becomes false. We can also choose if the even loop must block the
  current thread while waiting for a new task to arrive.
- **JavaScript/Native interop APIs.** They rely on the existing [node-api][]
  set of functions. The embedding APIs provide access to functions that
  retrieve or create `napi_env` instances related to a runtime instance.

## API reference

The C embedder API is split up by the four major groups described above.

### Global platform APIs

#### Data types

##### `node_embedding_platform`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

This is an opaque pointer that represents a Node.js platform instance.
Node.js allows only a single platform instance per process.

##### `node_embedding_exit_code`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The exit code returned from the C Node.js embedding APIs.

```c
typedef enum {
  node_embedding_exit_code_ok = 0,
  node_embedding_exit_code_generic_user_error = 1,
  node_embedding_exit_code_internal_js_parse_error = 3,
  node_embedding_exit_code_internal_js_evaluation_failure = 4,
  node_embedding_exit_code_v8_fatal_error = 5,
  node_embedding_exit_code_invalid_fatal_exception_monkey_patching = 6,
  node_embedding_exit_code_exception_in_fatal_exception_handler = 7,
  node_embedding_exit_code_invalid_command_line_argument = 9,
  node_embedding_exit_code_bootstrap_failure = 10,
  node_embedding_exit_code_invalid_command_line_argument2 = 12,
  node_embedding_exit_code_unsettled_top_level_await = 13,
  node_embedding_exit_code_startup_snapshot_failure = 14,
  node_embedding_exit_code_abort = 134,
} node_embedding_exit_code;
```

These values match to the C++ `node::ExitCode` enum that are used as Node.js
process exit codes.

- `node_embedding_exit_code_ok` - No issues.
- `node_embedding_exit_code_generic_user_error` - It was originally intended for
  uncaught JS exceptions from the user land but we actually use this for all
  kinds of generic errors.
- `node_embedding_exit_code_internal_js_parse_error` - It is unused because we
  pre-compile all builtins during snapshot building, when we exit with 1 if
  there's any error.
- `node_embedding_exit_code_internal_js_evaluation_failure` - It is actually
  unused. We exit with 1 in this case.
- `node_embedding_exit_code_v8_fatal_error` - It is actually unused. We exit
  with 133 (128+`SIGTRAP`) or 134 (128+`SIGABRT`) in this case.
- `node_embedding_exit_code_invalid_fatal_exception_monkey_patching`
- `node_embedding_exit_code_exception_in_fatal_exception_handler`
- `node_embedding_exit_code_invalid_command_line_argument`
- `node_embedding_exit_code_bootstrap_failure`
- `node_embedding_exit_code_invalid_command_line_argument2` - This was intended
  for invalid inspector arguments but is actually now just a duplicate of
  `node_embedding_exit_code_invalid_command_line_argument`.
- `node_embedding_exit_code_unsettled_top_level_await` -
- `node_embedding_exit_code_startup_snapshot_failure` -
- `node_embedding_exit_code_abort` - If the process exits from unhandled signals
  e.g. `SIGABRT`, `SIGTRAP`, typically the exit codes are 128 + signal number.
  We also exit with certain error codes directly for legacy reasons. Here we
  define those that are used to normalize the exit code on Windows.

##### `node_embedding_platform_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Flags are used to initialize a Node.js platform instance.

```c
typedef enum {
  node_embedding_platform_no_flags = 0,
  node_embedding_platform_enable_stdio_inheritance = 1 << 0,
  node_embedding_platform_disable_node_options_env = 1 << 1,
  node_embedding_platform_disable_cli_options = 1 << 2,
  node_embedding_platform_no_icu = 1 << 3,
  node_embedding_platform_no_stdio_initialization = 1 << 4,
  node_embedding_platform_no_default_signal_handling = 1 << 5,
  node_embedding_platform_no_init_openssl = 1 << 8,
  node_embedding_platform_no_parse_global_debug_variables = 1 << 9,
  node_embedding_platform_no_adjust_resource_limits = 1 << 10,
  node_embedding_platform_no_use_large_pages = 1 << 11,
  node_embedding_platform_no_print_help_or_version_output = 1 << 12,
  node_embedding_platform_generate_predictable_snapshot = 1 << 14,
} node_embedding_platform_flags;
```

These flags match to the C++ `node::ProcessInitializationFlags` and control the
Node.js platform initialization.

- `node_embedding_platform_no_flags` - The default flags.
- `node_embedding_platform_enable_stdio_inheritance` - Enable `stdio`
  inheritance, which is disabled by default. This flag is also implied by the
  `node_embedding_platform_no_stdio_initialization`.
- `node_embedding_platform_disable_node_options_env` - Disable reading the
  `NODE_OPTIONS` environment variable.
- `node_embedding_platform_disable_cli_options` - Do not parse CLI options.
- `node_embedding_platform_no_icu` - Do not initialize ICU.
- `node_embedding_platform_no_stdio_initialization` - Do not modify `stdio` file
  descriptor or TTY state.
- `node_embedding_platform_no_default_signal_handling` - Do not register
  Node.js-specific signal handlers and reset other signal handlers to
  default state.
- `node_embedding_platform_no_init_openssl` - Do not initialize OpenSSL config.
- `node_embedding_platform_no_parse_global_debug_variables` - Do not initialize
  Node.js debugging based on environment variables.
- `node_embedding_platform_no_adjust_resource_limits` - Do not adjust OS
  resource limits for this process.
- `node_embedding_platform_no_use_large_pages` - Do not map code segments into
  large pages for this process.
- `node_embedding_platform_no_print_help_or_version_output` - Skip printing
  output for `--help`, `--version`, `--v8-options`.
- `node_embedding_platform_generate_predictable_snapshot` - Initialize the
  process for predictable snapshot generation.

#### Callback types

##### `node_embedding_error_handler`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef node_embedding_exit_code(NAPI_CDECL* node_embedding_error_handler)(
    void* handler_data,
    const char* messages[],
    size_t messages_size,
    node_embedding_exit_code exit_code);
```

Function pointer type for user-provided native function that handles the list
of error messages and the exit code.

The callback parameters:

- `[in] handler_data`: The user data associated with this callback.
- `[in] messages`: Pointer to an array of zero terminating strings.
- `[in] messages_size`: Size of the `messages` string array.
- `[in] exit_code`: The suggested process exit code in case of error. If the
  `exit_code` is zero, then the callback is used to output non-error messages.

##### `node_embedding_get_args_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(NAPI_CDECL* node_embedding_get_args_callback)(void* cb_data,
                                                           int32_t argc,
                                                           const char* argv[]);
```

Function pointer type for user-provided native function that receives list of
CLI arguments from the `node_embedding_platform`.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] argc`: Number of items in the `argv` array.
- `[in] argv`: CLI arguments as an array of zero terminating strings.

#### Functions

##### `node_embedding_run_nodejs_main`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Runs Node.js main function as if it is invoked from Node.js CLI without any
embedder customizations.

```c
int32_t NAPI_CDECL
node_embedding_run_nodejs_main(int32_t argc,
                               char* argv[]);
```

- `[in] argc`: Number of items in the `argv` array.
- `[in] argv`: CLI arguments as an array of zero terminating strings.

Returns 0 if there were no issues.

##### `node_embedding_on_error`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets global custom error handler for the Node.js embedded code.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_on_error(node_embedding_error_handler error_handler,
                        void* error_handler_data);
```

- `[in] error_handler`: The error handler callback.
- `[in] error_handler_data`: Optional. The error handler data that will be
  passed to the `error_handler` callback. It can be removed after the
  `node_embedding_delete_platform()` call.

Returns `node_embedding_exit_code_ok` if there were no issues.

It is recommended to call this function before the creation of the
`node_embedding_platform` instance to handle all error messages the same way.

This function assigns a custom platform error handler. It replaces the default
error handler that outputs error messages to the `stderr` and exits the current
process with the `exit_code` when it is not zero.

The zero `exit_code` indicates reported warnings or text messages. For example,
it can be Node.js help text returned in response to the `--help` CLI argument.

##### `node_embedding_create_platform`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Creates new Node.js platform instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_create_platform(int32_t api_version,
                               node_embedding_platform* result);
```

- `[in] api_version`: The version of the C embedder API.
- `[out] result`: New Node.js platform instance.

Returns `node_embedding_exit_code_ok` if there were no issues.

It is a simple object allocation. It does not do any initialization or any
other complex work that may fail. It only checks the argument.

Node.js allows only a single platform instance per process.

##### `node_embedding_delete_platform`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Deletes Node.js platform instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_delete_platform(node_embedding_platform platform);
```

- `[in] platform`: The Node.js platform instance to delete.

Returns `node_embedding_exit_code_ok` if there were no issues.

If the platform was initialized before the deletion, then the method
uninitializes the platform before deletion.

##### `node_embedding_platform_is_initialized`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Checks if the Node.js platform instance is initialized.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_platform_is_initialized(node_embedding_platform platform,
                                       bool* result);
```

- `[in] platform`: The Node.js platform instance to check.
- `[out] result`: `true` if the platform is already initialized.

Returns `node_embedding_exit_code_ok` if there were no issues.

The platform instance settings can be changed until the platform is initialized.
After the `node_embedding_platform_initialize` function call any attempt to
change platform instance settings will fail.

##### `node_embedding_platform_set_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the Node.js platform instance flags.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_platform_set_flags(node_embedding_platform platform,
                                  node_embedding_platform_flags flags);
```

- `[in] platform`: The Node.js platform instance to configure.
- `[in] flags`: The platform flags that control the platform behavior.

Returns `node_embedding_exit_code_ok` if there were no issues.

##### `node_embedding_platform_set_args`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the CLI args for the Node.js platform instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_platform_set_args(node_embedding_platform platform,
                                 int32_t argc,
                                 char* argv[]);
```

- `[in] platform`: The Node.js platform instance to configure.
- `[in] argc`: Number of items in the `argv` array.
- `[in] argv`: CLI arguments as an array of zero terminating strings.

Returns `node_embedding_exit_code_ok` if there were no issues.

##### `node_embedding_platform_initialize`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Initializes the Node.js platform instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_platform_initialize(node_platform platform,
                                   bool* early_return);
```

- `[in] platform`: The Node.js platform instance to initialize.
- `[out] early_return`: Optional. `true` if the initialization result requires
  early return either because of an error or the Node.js completed the work.
  For example, it had printed Node.js version or the help text.

Returns `node_embedding_exit_code_ok` if there were no issues.

The Node.js platform instance initialization parses CLI args, initializes
Node.js internals and the V8 runtime. If the initial work such as printing the
Node.js version number is completed, then the `early_return` is set to `true`.

After the initialization is completed the Node.js platform settings cannot be
changed anymore. The parsed arguments can be accessed by calling the
`node_embedding_platform_get_args` and `node_embedding_platform_get_exec_args`
functions.

##### `node_embedding_platform_get_args`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Gets the parsed list of non-Node.js arguments.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_platform_get_parsed_args(
    node_embedding_platform platform,
    node_embedding_get_args_callback get_args_cb,
    void* get_args_cb_data,
    node_embedding_get_args_callback get_exec_args_cb,
    void* get_exec_args_cb_data);
```

- `[in] platform`: The Node.js platform instance.
- `[in] get_args_cb`: The callback to receive non-Node.js arguments.
- `[in] get_args_cb_data`: Optional. The callback data that will be passed to
  the `get_args_cb` callback. It can be deleted right after the function call.
- `[in] get_exec_args_cb`: The callback to receive Node.js arguments.
- `[in] get_exec_args_cb_data`: Optional. The callback data that will be passed
  to the `get_exec_args_cb` callback. It can be deleted right after the function
  call.

Returns `node_embedding_exit_code_ok` if there were no issues.

### Runtime instance APIs

#### Data types

##### `node_embedding_runtime`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

This is an opaque pointer that represents a Node.js runtime instance.
It wraps up the C++ `node::Environment`.
There can be one or more runtime instances in the process.

##### `node_embedding_runtime_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Flags are used to initialize a Node.js runtime instance.

```c
typedef enum {
  node_embedding_runtime_no_flags = 0,
  node_embedding_runtime_default_flags = 1 << 0,
  node_embedding_runtime_owns_process_state = 1 << 1,
  node_embedding_runtime_owns_inspector = 1 << 2,
  node_embedding_runtime_no_register_esm_loader = 1 << 3,
  node_embedding_runtime_track_unmanaged_fds = 1 << 4,
  node_embedding_runtime_hide_console_windows = 1 << 5,
  node_embedding_runtime_no_native_addons = 1 << 6,
  node_embedding_runtime_no_global_search_paths = 1 << 7,
  node_embedding_runtime_no_browser_globals = 1 << 8,
  node_embedding_runtime_no_create_inspector = 1 << 9,
  node_embedding_runtime_no_start_debug_signal_handler = 1 << 10,
  node_embedding_runtime_no_wait_for_inspector_frontend = 1 << 11
} node_embedding_runtime_flags;
```

These flags match to the C++ `node::EnvironmentFlags` and control the
Node.js runtime initialization.

- `node_embedding_runtime_no_flags` - No flags set.
- `node_embedding_runtime_default_flags` - Use the default behavior for
  Node.js instances.
- `node_embedding_runtime_owns_process_state` - Controls whether this runtime
  is allowed to affect per-process state (e.g. cwd, process title, uid, etc.).
  This is set when using `node_embedding_runtime_default_flags`.
- `node_embedding_runtime_owns_inspector` - Set if this runtime instance is
  associated with the global inspector handling code (i.e. listening
  on `SIGUSR1`).
  This is set when using `node_embedding_runtime_default_flags`.
- `node_embedding_runtime_no_register_esm_loader` - Set if Node.js should not
  run its own esm loader. This is needed by some embedders, because it's
  possible for the Node.js esm loader to conflict with another one in an
  embedder environment, e.g. Blink's in Chromium.
- `node_embedding_runtime_track_unmanaged_fds` - Set this flag to make Node.js
  track "raw" file descriptors, i.e. managed by `fs.open()` and `fs.close()`,
  and close them during `node_embedding_delete_runtime`.
- `node_embedding_runtime_hide_console_windows` - Set this flag to force hiding
  console windows when spawning child processes. This is usually used when
  embedding Node.js in GUI programs on Windows.
- `node_embedding_runtime_no_native_addons` - Set this flag to disable loading
  native addons via `process.dlopen`. This runtime flag is especially important
  for worker threads so that a worker thread can't load a native addon even if
  `execArgv` is overwritten and `--no-addons` is not specified but was specified
  for this runtime instance.
- `node_embedding_runtime_no_global_search_paths` - Set this flag to disable
  searching modules from global paths like `$HOME/.node_modules` and
  `$NODE_PATH`. This is used by standalone apps that do not expect to have their
  behaviors changed because of globally installed modules.
- `node_embedding_runtime_no_browser_globals` - Do not export browser globals
  like setTimeout, console, etc.
- `node_embedding_runtime_no_create_inspector` - Controls whether or not the
  runtime should call `V8Inspector::create()`. This control is needed by
  embedders who may not want to initialize the V8 inspector in situations where
  one has already been created, e.g. Blink's in Chromium.
- `node_embedding_runtime_no_start_debug_signal_handler` - Controls whether or
  not the `InspectorAgent` for this runtime should call
  `StartDebugSignalHandler`. This control is needed by embedders who may not
  want to allow other processes to start the V8 inspector.
- `node_embedding_runtime_no_wait_for_inspector_frontend` - Controls whether the
  `InspectorAgent` created for this runtime waits for Inspector frontend events
  during the runtime creation. It's used to call `node::Stop(env)` on a Worker
  thread that is waiting for the events.

##### `node_embedding_snapshot_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Flags are used to create a Node.js runtime JavaScript snapshot.

```c
typedef enum {
  node_embedding_snapshot_no_flags = 0,
  node_embedding_snapshot_no_code_cache = 1 << 0,
} node_embedding_snapshot_flags;
```

These flags match to the C++ `node::SnapshotFlags` and control the
Node.js runtime snapshot creation.

- `node_embedding_snapshot_no_flags` - No flags set.
- `node_embedding_snapshot_no_code_cache` - Whether code cache should be
  generated as part of the snapshot. Code cache reduces the time spent on
  compiling functions included in the snapshot at the expense of a bigger
  snapshot size and potentially breaking portability of the snapshot.

#### Callback types

##### `node_embedding_runtime_preload_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(NAPI_CDECL* node_embedding_runtime_preload_callback)(
    void* cb_data,
    napi_env env,
    napi_value process,
    napi_value require);
```

Function pointer type for user-provided native function that is called when the
runtime initially loads the JavaScript code.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] env`: Node-API environmentStart of the blob memory span.
- `[in] process`: The Node.js `process` object.
- `[in] require`: The internal `require` function.

##### `node_embedding_store_blob_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(NAPI_CDECL* node_embedding_store_blob_callback)(
    void* cb_data,
    const uint8_t* blob,
    size_t size);
```

Function pointer type for user-provided native function that is called when the
runtime needs to store the snapshot blob.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] blob`: Start of the blob memory span.
- `[in] size`: Size of the blob memory span.

##### `node_embedding_initialize_module_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef napi_value(NAPI_CDECL* node_embedding_initialize_module_callback)(
    void* cb_data,
    napi_env env,
    const char* module_name,
    napi_value exports);
```

Function pointer type for initializing linked native module that can be defined
in the embedder executable.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] env`: Node API environment.
- `[in] module_name`: Name of the module.
- `[in] exports`: The `exports` module object.

All module exports must be added as properties to the `exports` object.
As an alternative a new object or another value can be created in the callback
and returned.

#### Functions

##### `node_embedding_create_runtime`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Creates new Node.js runtime instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_create_runtime(node_embedding_platform platform,
                              node_embedding_runtime* result);
```

- `[in] platform`: Optional. An initialized Node.js platform instance.
- `[out] result`: Upon return has a new Node.js runtime instance.

Returns `node_embedding_exit_code_ok` if there were no issues.

Creates new Node.js runtime instance based on the provided platform instance.

It is a simple object allocation. It does not do any initialization or any
other complex work that may fail besides checking the arguments.

If the platform instance is `NULL` then a default platform instance will be
created internally when the `node_embedding_platform_initialize` is called.
Since there can be only one platform instance per process, only one runtime
instance can be created this way per process.

If it is planned to create more than one runtime instance or a non-default
platform configuration is required, then it is recommended to create the
Node.js platform instance explicitly.

##### `node_embedding_delete_runtime`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Deletes Node.js runtime instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_delete_runtime(node_embedding_runtime runtime);
```

- `[in] runtime`: The Node.js runtime instance to delete.

Returns `node_embedding_exit_code_ok` if there were no issues.

If the runtime was initialized, then the method un-initializes the runtime
before the deletion.

As a part of the un-initialization it can store created snapshot blob if the
`node_embedding_runtime_on_create_snapshot` set the callback to save the
snapshot blob.

##### `node_embedding_runtime_is_initialized`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Checks if the Node.js runtime instance is initialized.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_is_initialized(node_embedding_runtime runtime,
                                      bool* result);
```

- `[in] runtime`: The Node.js runtime instance to check.
- `[out] result`: `true` if the runtime is already initialized.

Returns `node_embedding_exit_code_ok` if there were no issues.

The runtime settings can be changed until the runtime is initialized.
After the `node_embedding_runtime_initialize` function is called any attempt
to change runtime settings will fail.

##### `node_embedding_runtime_set_flags`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the Node.js runtime instance flags.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_set_flags(node_embedding_runtime runtime,
                                 node_embedding_runtime_flags flags);
```

- `[in] runtime`: The Node.js runtime instance to configure.
- `[in] flags`: The runtime flags that control the runtime behavior.

Returns `node_embedding_exit_code_ok` if there were no issues.

##### `node_embedding_runtime_set_args`

Sets the non-Node.js arguments for the Node.js runtime instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_set_args(node_embedding_runtime runtime,
                                int32_t argc,
                                const char* argv[],
                                int32_t exec_argc,
                                const char* exec_argv[]);
```

- `[in] runtime`: The Node.js runtime instance to configure.
- `[in] argc`: Number of items in the `argv` array.
- `[in] argv`: non-Node.js arguments as an array of zero terminating strings.
- `[in] exec_argc`: Number of items in the `exec_argv` array.
- `[in] exec_argv`: Node.js arguments as an array of zero terminating strings.

Returns `node_embedding_exit_code_ok` if there were no issues.

##### `node_embedding_runtime_on_preload`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets a preload callback to call before Node.js runtime instance is loaded.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_on_preload(
    node_embedding_runtime runtime,
    node_embedding_runtime_preload_callback preload_cb,
    void* preload_cb_data);
```

- `[in] runtime`: The Node.js runtime instance.
- `[in] preload_cb`: The preload callback to be called before Node.js runtime
  instance is loaded.
- `[in] preload_cb_data`: Optional. The preload callback data that will be
  passed to the `preload_cb` callback. It can be removed after the
  `node_embedding_delete_runtime` call.

Returns `node_embedding_exit_code_ok` if there were no issues.

##### `node_embedding_runtime_add_module`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Adds a linked module for the Node.js runtime instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_add_module(
    node_embedding_runtime runtime,
    const char* module_name,
    node_embedding_initialize_module_callback init_module_cb,
    void* init_module_cb_data,
    int32_t module_node_api_version);
```

- `[in] runtime`: The Node.js runtime instance.
- `[in] module_name`: The name of the module.
- `[in] init_module_cb`: Module initialization callback. It is called for the
  main and worker threads. The caller must take care about the thread safety.
- `[in] init_module_cb_data`: The user data for the init_module_cb.
- `[in] module_node_api_version`: The Node API version used by the module.

Returns `node_embedding_exit_code_ok` if there were no issues.

The registered module can be accessed in JavaScript as
`process._linkedBinding(module_name)` in the main JS and in the related
worker threads.

##### `node_embedding_runtime_on_create_snapshot`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets a callback to store created snapshot when Node.js runtime instance
finished execution.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_on_create_snapshot(
    node_embedding_runtime runtime,
    node_embedding_store_blob_callback store_blob_cb,
    void* store_blob_cb_data,
    node_embedding_snapshot_flags snapshot_flags);
```

- `[in] runtime`: The Node.js runtime instance.
- `[in] store_blob_cb`: The store blob callback to be called before Node.js
  runtime instance is deleted.
- `[in] store_blob_cb_data`: Optional. The store blob callback data that will be
  passed to the `store_blob_cb` callback. It can be removed after the
  `node_embedding_delete_runtime` call.
- `[in] snapshot_flags`: The flags controlling the snapshot creation.

Returns `node_embedding_exit_code_ok` if there were no issues.

##### `node_embedding_runtime_initialize_from_script`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Initializes the Node.js runtime instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_initialize_from_script(node_embedding_runtime runtime,
                                              const char* main_script);
```

- `[in] runtime`: The Node.js runtime instance to initialize.
- `[in] main_script`: The main script to run.

Returns `node_embedding_exit_code_ok` if there were no issues.

The Node.js runtime initialization creates new Node.js environment associated
with a V8 `Isolate` and V8 `Context`.

After the initialization is completed the Node.js runtime settings cannot be
changed anymore.

##### `node_embedding_runtime_initialize_from_snapshot`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Initializes the Node.js runtime instance.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_initialize_from_snapshot(node_embedding_runtime runtime,
                                                const uint8_t* snapshot,
                                                size_t size);
```

- `[in] runtime`: The Node.js runtime instance to initialize.
- `[in] snapshot`: Start of the snapshot memory span.
- `[in] size`: Size of the snapshot memory span.

Returns `node_embedding_exit_code_ok` if there were no issues.

The Node.js runtime initialization creates new Node.js environment associated
with a V8 `Isolate` and V8 `Context`.

After the initialization is completed the Node.js runtime settings cannot be
changed anymore.

### Event loop APIs

#### Data types

##### `node_embedding_event_loop_run_mode`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The event loop run mode.

```c
typedef enum {
  node_embedding_event_loop_run_once = 1,
  node_embedding_event_loop_run_nowait = 2,
} node_embedding_event_loop_run_mode;
```

These values match to UV library `uv_run_mode` enum and control the event loop
beahvior.

- `node_embedding_event_loop_run_once` - Run the event loop once and wait if
  there are no items. It matches the `UV_RUN_ONCE` behavior.
- `node_embedding_event_loop_run_nowait` - Run the event loop once and do not
  wait if there are no items. It matches the `UV_RUN_NOWAIT` behavior.

##### `node_embedding_promise_state`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The state of the completed `Promise`.

```c
typedef enum {
  node_embedding_promise_state_pending = 0,
  node_embedding_promise_state_fulfilled = 1,
  node_embedding_promise_state_rejected = 2,
} node_embedding_promise_state;
```

These values match to `v8::Promise::PromiseState` enum and indicate the internal
state of a `Promise` object.

- `node_embedding_promise_state_pending` - The Promise is still awaiting to
  be completed.
- `node_embedding_promise_state_fulfilled` - The Promise was successfully
  fulfilled.
- `node_embedding_promise_state_rejected` - The Promise was rejected due an
  error.

#### Callback types

##### `node_embedding_event_loop_predicate`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef bool(NAPI_CDECL* node_embedding_event_loop_predicate)(
    void* predicate_data,
    bool has_work);
```

Function pointer type for user-provided predicate function that is checked
before each task execution in the Node.js runtime event loop.

The callback parameters:

- `[in] predicate_data`: The user data associated with this predicate callback.
- `[in] has_work`: `true` if the event loop has work to do.

Returns `true` if the runtime loop must continue to run.

#### Functions

##### `node_embedding_runtime_run_event_loop`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Runs Node.js runtime instance event loop.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_run_event_loop(node_embedding_runtime runtime);
```

- `[in] runtime`: The Node.js runtime instance.

Returns `node_embedding_exit_code_ok` if there were no issues.

The function exits when there are no more tasks to process in the loop.

##### `node_embedding_runtime_run_event_loop_while`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Runs Node.js runtime instance event loop while there tasks to process and
the provided predicate returns true.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_run_event_loop_while(
    node_embedding_runtime runtime,
    node_embedding_runtime_event_loop_predicate predicate,
    void* predicate_data,
    node_embedding_event_loop_run_mode run_mode,
    bool* has_more_work);
```

- `[in] runtime`: The Node.js runtime instance.
- `[in] predicate`: The predicate to check before each runtime event loop
  task invocation.
- `[in] predicate_data`: Optional. The predicate data that will be
  passed to the `predicate` callback. It can be removed right after this
  function call.
- `[in] run_mode`: Specifies behavior in case if the event loop does not have
  items to process. The `node_embedding_event_loop_run_once` will block the
  current thread and the `node_embedding_event_loop_run_nowait` will not wait
  and exit.
- `[out] has_more_work`: `true` if the runtime event loop has more tasks after
  returning from the function.

Returns `node_embedding_exit_code_ok` if there were no issues.

##### `node_embedding_runtime_await_promise`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Runs Node.js runtime instance event loop until the provided promise is completed
with a success of a failure. It blocks the thread if there are to tasks in the
loop and the promise is not completed.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_await_promise(node_embedding_runtime runtime,
                                     napi_value promise,
                                     node_embedding_promise_state* state,
                                     napi_value* result,
                                     bool* has_more_work);
```

- `[in] runtime`: The Node.js runtime instance.
- `[in] promise`: The promise to complete.
- `[out] state`: The state of the `promise` upon the return from the function.
- `[out] result`: Result of the `promise` completion. It is either fulfilled or
  rejected value depending on `state`.
- `[out] has_more_work`: `true` if the runtime event loop has more tasks after
  returning from the function.

Returns `node_embedding_exit_code_ok` if there were no issues.

### JavaScript/Native interop APIs

#### Callback types

##### `node_embedding_node_api_callback`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

```c
typedef void(NAPI_CDECL* node_embedding_node_api_callback)(
    void* cb_data,
    napi_env env);
```

Function pointer type for callback that invokes Node-API code.

The callback parameters:

- `[in] cb_data`: The user data associated with this callback.
- `[in] env`: Node-API environment.

#### Functions

##### `node_embedding_runtime_set_node_api_version`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Sets the Node-API version for the Node.js runtime instance.

```c
node_embedding_exit_code NAPI_CDECL
node_runtime_set_node_api_version(node_embedding_runtime runtime,
                                  int32_t node_api_version);
```

- `[in] runtime`: The Node.js runtime instance.
- `[in] node_api_version`: The version of the Node-API.

Returns `node_embedding_exit_code_ok` if there were no issues.

By default it is using the Node-API version 8.

##### `node_embedding_runtime_invoke_node_api`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Invokes a callback that runs Node-API code.

```c
node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_invoke_node_api(
    node_embedding_runtime runtime,
    node_embedding_node_api_callback node_api_cb,
    void* node_api_cb_data);
```

- `[in] runtime`: The Node.js embedding_runtime instance.
- `[in] node_api_cb`: The callback that executes Node-API code.
- `[in] node_api_cb_data`: The data associated with the callback.

Returns `node_embedding_exit_code_ok` if there were no issues.

The function invokes the callback that runs Node-API code in the V8 Isolate
scope, V8 Handle scope, and V8 Context scope. Then, it triggers the uncaught
exception handler if there were any unhandled errors.

## Examples

The examples listed here are part of the Node.js
[embedding unit tests][test_embedding].

```c
  // TODO: add example here.
```

[Builder pattern]: https://en.wikipedia.org/wiki/Builder_pattern
[CLI options]: cli.md
[`process.memoryUsage()`]: process.md#processmemoryusage
[deprecation policy]: deprecations.md
[embedtest.cc]: https://github.com/nodejs/node/blob/HEAD/test/embedding/embedtest.cc
[test_embedding]: https://github.com/nodejs/node/blob/HEAD/test/embedding
[node-api]: n-api.md
[src/node.h]: https://github.com/nodejs/node/blob/HEAD/src/node.h
