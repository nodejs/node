//
// Description: C-based API for embedding Node.js.
//
// !!! WARNING !!! WARNING !!! WARNING !!!
// This is a new API and is subject to change.
// While it is C-based, it is not ABI safe yet.
// Consider all functions and data structures as experimental.
// !!! WARNING !!! WARNING !!! WARNING !!!
//
// This file contains the C-based API for embedding Node.js in a host
// application. The API is designed to be used by applications that want to
// embed Node.js as a shared library (.so or .dll) and can interop with
// C-based API.
//

#ifndef SRC_NODE_EMBEDDING_API_H_
#define SRC_NODE_EMBEDDING_API_H_

#include "node_api.h"

#define NODE_EMBEDDING_VERSION 1

EXTERN_C_START

//==============================================================================
// Data types
//==============================================================================

typedef struct node_embedding_platform__* node_embedding_platform;
typedef struct node_embedding_runtime__* node_embedding_runtime;

typedef enum {
  node_embedding_exit_code_ok = 0,
  // 1 was intended for uncaught JS exceptions from the user land but we
  // actually use this for all kinds of generic errors.
  node_embedding_exit_code_generic_user_error = 1,
  // 2 is unused
  // 3 is actually unused because we pre-compile all builtins during
  // snapshot building, when we exit with 1 if there's any error.
  node_embedding_exit_code_internal_js_parse_error = 3,
  // 4 is actually unused. We exit with 1 in this case.
  node_embedding_exit_code_internal_js_evaluation_failure = 4,
  // 5 is actually unused. We exit with 133 (128+SIGTRAP) or 134
  // (128+SIGABRT) in this case.
  node_embedding_exit_code_v8_fatal_error = 5,
  node_embedding_exit_code_invalid_fatal_exception_monkey_patching = 6,
  node_embedding_exit_code_exception_in_fatal_exception_handler = 7,
  // 8 is unused
  node_embedding_exit_code_invalid_command_line_argument = 9,
  node_embedding_exit_code_bootstrap_failure = 10,
  // 11 is unused
  // This was intended for invalid inspector arguments but is actually now
  // just a duplicate of node_embedding_exit_code_invalid_command_line_argument
  node_embedding_exit_code_invalid_command_line_argument2 = 12,
  node_embedding_exit_code_unsettled_top_level_await = 13,
  node_embedding_exit_code_startup_snapshot_failure = 14,
  // If the process exits from unhandled signals e.g. SIGABRT, SIGTRAP,
  // typically the exit codes are 128 + signal number. We also exit with
  // certain error codes directly for legacy reasons. Here we define those
  // that are used to normalize the exit code on Windows.
  node_embedding_exit_code_abort = 134,
} node_embedding_exit_code;

typedef enum {
  node_embedding_platform_no_flags = 0,
  // Enable stdio inheritance, which is disabled by default.
  // This flag is also implied by
  // node_embedding_platform_no_stdio_initialization.
  node_embedding_platform_enable_stdio_inheritance = 1 << 0,
  // Disable reading the NODE_OPTIONS environment variable.
  node_embedding_platform_disable_node_options_env = 1 << 1,
  // Do not parse CLI options.
  node_embedding_platform_disable_cli_options = 1 << 2,
  // Do not initialize ICU.
  node_embedding_platform_no_icu = 1 << 3,
  // Do not modify stdio file descriptor or TTY state.
  node_embedding_platform_no_stdio_initialization = 1 << 4,
  // Do not register Node.js-specific signal handlers
  // and reset other signal handlers to default state.
  node_embedding_platform_no_default_signal_handling = 1 << 5,
  // Do not initialize OpenSSL config.
  node_embedding_platform_no_init_openssl = 1 << 8,
  // Do not initialize Node.js debugging based on environment variables.
  node_embedding_platform_no_parse_global_debug_variables = 1 << 9,
  // Do not adjust OS resource limits for this process.
  node_embedding_platform_no_adjust_resource_limits = 1 << 10,
  // Do not map code segments into large pages for this process.
  node_embedding_platform_no_use_large_pages = 1 << 11,
  // Skip printing output for --help, --version, --v8-options.
  node_embedding_platform_no_print_help_or_version_output = 1 << 12,
  // Initialize the process for predictable snapshot generation.
  node_embedding_platform_generate_predictable_snapshot = 1 << 14,
} node_embedding_platform_flags;

typedef enum {
  node_embedding_runtime_no_flags = 0,
  // Use the default behavior for Node.js instances.
  node_embedding_runtime_default_flags = 1 << 0,
  // Controls whether this Environment is allowed to affect per-process state
  // (e.g. cwd, process title, uid, etc.).
  // This is set when using node_embedding_runtime_default_flags.
  node_embedding_runtime_owns_process_state = 1 << 1,
  // Set if this Environment instance is associated with the global inspector
  // handling code (i.e. listening on SIGUSR1).
  // This is set when using node_embedding_runtime_default_flags.
  node_embedding_runtime_owns_inspector = 1 << 2,
  // Set if Node.js should not run its own esm loader. This is needed by some
  // embedders, because it's possible for the Node.js esm loader to conflict
  // with another one in an embedder environment, e.g. Blink's in Chromium.
  node_embedding_runtime_no_register_esm_loader = 1 << 3,
  // Set this flag to make Node.js track "raw" file descriptors, i.e. managed
  // by fs.open() and fs.close(), and close them during
  // node_embedding_delete_runtime().
  node_embedding_runtime_track_unmanaged_fds = 1 << 4,
  // Set this flag to force hiding console windows when spawning child
  // processes. This is usually used when embedding Node.js in GUI programs on
  // Windows.
  node_embedding_runtime_hide_console_windows = 1 << 5,
  // Set this flag to disable loading native addons via `process.dlopen`.
  // This environment flag is especially important for worker threads
  // so that a worker thread can't load a native addon even if `execArgv`
  // is overwritten and `--no-addons` is not specified but was specified
  // for this Environment instance.
  node_embedding_runtime_no_native_addons = 1 << 6,
  // Set this flag to disable searching modules from global paths like
  // $HOME/.node_modules and $NODE_PATH. This is used by standalone apps that
  // do not expect to have their behaviors changed because of globally
  // installed modules.
  node_embedding_runtime_no_global_search_paths = 1 << 7,
  // Do not export browser globals like setTimeout, console, etc.
  node_embedding_runtime_no_browser_globals = 1 << 8,
  // Controls whether or not the Environment should call V8Inspector::create().
  // This control is needed by embedders who may not want to initialize the V8
  // inspector in situations where one has already been created,
  // e.g. Blink's in Chromium.
  node_embedding_runtime_no_create_inspector = 1 << 9,
  // Controls whether or not the InspectorAgent for this Environment should
  // call StartDebugSignalHandler. This control is needed by embedders who may
  // not want to allow other processes to start the V8 inspector.
  node_embedding_runtime_no_start_debug_signal_handler = 1 << 10,
  // Controls whether the InspectorAgent created for this Environment waits for
  // Inspector frontend events during the Environment creation. It's used to
  // call node::Stop(env) on a Worker thread that is waiting for the events.
  node_embedding_runtime_no_wait_for_inspector_frontend = 1 << 11
} node_embedding_runtime_flags;

typedef enum {
  // Run the event loop until it is completed.
  // It matches the UV_RUN_DEFAULT behavior.
  node_embedding_event_loop_run_default = 0,
  // Run the event loop once and wait if there are no items.
  // It matches the UV_RUN_ONCE behavior.
  node_embedding_event_loop_run_once = 1,
  // Run the event loop once and do not wait if there are no items.
  // It matches the UV_RUN_NOWAIT behavior.
  node_embedding_event_loop_run_nowait = 2,
} node_embedding_event_loop_run_mode;

//==============================================================================
// Callbacks
//==============================================================================

typedef node_embedding_exit_code(NAPI_CDECL* node_embedding_error_handler)(
    void* handler_data,
    const char* messages[],
    size_t messages_size,
    node_embedding_exit_code exit_code);

typedef void(NAPI_CDECL* node_embedding_get_args_callback)(void* cb_data,
                                                           int32_t argc,
                                                           const char* argv[]);

typedef void(NAPI_CDECL* node_embedding_preload_callback)(
    node_embedding_runtime runtime,
    void* cb_data,
    napi_env env,
    napi_value process,
    napi_value require);

typedef napi_value(NAPI_CDECL* node_embedding_initialize_module_callback)(
    node_embedding_runtime runtime,
    void* cb_data,
    napi_env env,
    const char* module_name,
    napi_value exports);

typedef void(NAPI_CDECL* node_embedding_event_loop_handler)(
    node_embedding_runtime runtime, void* handler_data);

typedef void(NAPI_CDECL* node_embedding_node_api_callback)(
    node_embedding_runtime runtime, void* cb_data, napi_env env);

//==============================================================================
// Functions
//==============================================================================

//------------------------------------------------------------------------------
// Node.js main function.
//------------------------------------------------------------------------------

// Runs Node.js main function as if it is invoked from Node.js CLI without any
// embedder customizations.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_run_nodejs_main(int32_t argc, char* argv[]);

//------------------------------------------------------------------------------
// Error handling functions.
//------------------------------------------------------------------------------

// Sets the global error handing for the Node.js embedding API.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL node_embedding_on_error(
    node_embedding_error_handler error_handler, void* error_handler_data);

//------------------------------------------------------------------------------
// Node.js global platform functions.
//------------------------------------------------------------------------------

// Creates a new Node.js platform instance.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL node_embedding_create_platform(
    int32_t api_version, node_embedding_platform* result);

// Deletes the Node.js platform instance.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_delete_platform(node_embedding_platform platform);

// Checks if the Node.js platform is initialized.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_platform_is_initialized(node_embedding_platform platform,
                                       bool* result);

// Sets the flags for the Node.js platform initialization.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_platform_set_flags(node_embedding_platform platform,
                                  node_embedding_platform_flags flags);

// Sets the CLI arguments for the Node.js platform initialization.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_platform_set_args(node_embedding_platform platform,
                                 int32_t argc,
                                 char* argv[]);

// Initializes the Node.js platform.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_platform_initialize(node_embedding_platform platform,
                                   bool* early_return);

// Gets the parsed list of non-Node.js and Node.js arguments.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_platform_get_parsed_args(
    node_embedding_platform platform,
    node_embedding_get_args_callback get_args_cb,
    void* get_args_cb_data,
    node_embedding_get_args_callback get_exec_args_cb,
    void* get_exec_args_cb_data);

//------------------------------------------------------------------------------
// Node.js runtime functions.
//------------------------------------------------------------------------------

// Creates a new Node.js runtime instance.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL node_embedding_create_runtime(
    node_embedding_platform platform, node_embedding_runtime* result);

// Deletes the Node.js runtime instance.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_delete_runtime(node_embedding_runtime runtime);

// Checks if the Node.js runtime is initialized.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_is_initialized(node_embedding_runtime runtime,
                                      bool* result);

// Sets the flags for the Node.js runtime initialization.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_set_flags(node_embedding_runtime runtime,
                                 node_embedding_runtime_flags flags);

// Sets the non-Node.js and Node.js CLI arguments for the Node.js runtime
// initialization.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_set_args(node_embedding_runtime runtime,
                                int32_t argc,
                                const char* argv[],
                                int32_t exec_argc,
                                const char* exec_argv[]);

// Sets the preload callback for the Node.js runtime initialization.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_on_preload(node_embedding_runtime runtime,
                                  node_embedding_preload_callback preload_cb,
                                  void* preload_cb_data);

// Adds a new module to the Node.js runtime.
// It is accessed as process._linkedBinding(module_name) in the main JS and in
// the related worker threads.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_add_module(
    node_embedding_runtime runtime,
    const char* module_name,
    node_embedding_initialize_module_callback init_module_cb,
    void* init_module_cb_data,
    int32_t module_node_api_version);

// Initializes the Node.js runtime.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_initialize(node_embedding_runtime runtime,
                                  const char* main_script);

//------------------------------------------------------------------------------
// Node.js runtime functions for the event loop.
//------------------------------------------------------------------------------

// Initializes the runtime to call the provided handler when the runtime event
// loop has some work to do. It starts an observer thread that is stopped by the
// `node_embedding_runtime_complete_event_loop` function call. This function
// helps to integrate the Node.js runtime event loop with the host UI loop.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_on_event_loop_run_request(
    node_embedding_runtime runtime,
    node_embedding_event_loop_handler event_loop_handler,
    void* event_loop_handler_data);

// Runs the Node.js runtime event loop.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_run_event_loop(
    node_embedding_runtime runtime,
    node_embedding_event_loop_run_mode run_mode,
    bool* has_more_work);

// Runs the Node.js runtime event loop in node_embedding_event_loop_run_default
// mode and finishes it with emitting the beforeExit and exit process events.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_complete_event_loop(node_embedding_runtime runtime);

//------------------------------------------------------------------------------
// Node.js runtime functions for the Node-API interop.
//------------------------------------------------------------------------------

// Sets the Node-API version for the Node.js runtime initialization.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_set_node_api_version(node_embedding_runtime runtime,
                                            int32_t node_api_version);

// Invokes Node-API code.
NAPI_EXTERN node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_invoke_node_api(
    node_embedding_runtime runtime,
    node_embedding_node_api_callback node_api_cb,
    void* node_api_cb_data);

EXTERN_C_END

#ifdef __cplusplus

//------------------------------------------------------------------------------
// Convenience union operator for the Node.js flags.
//------------------------------------------------------------------------------

inline constexpr node_embedding_platform_flags operator|(
    node_embedding_platform_flags lhs, node_embedding_platform_flags rhs) {
  return static_cast<node_embedding_platform_flags>(static_cast<int32_t>(lhs) |
                                                    static_cast<int32_t>(rhs));
}

inline constexpr node_embedding_runtime_flags operator|(
    node_embedding_runtime_flags lhs, node_embedding_runtime_flags rhs) {
  return static_cast<node_embedding_runtime_flags>(static_cast<int32_t>(lhs) |
                                                   static_cast<int32_t>(rhs));
}

#endif

#endif  // SRC_NODE_EMBEDDING_API_H_

// TODO(vmoroz): Allow running Node.js uv_loop from UI loop. Follow the Electron
//               implementation. - Complete implementation for non-Windows.
// TODO(vmoroz): Can we use some kind of waiter concept instead of the
//               observer thread?
// TODO(vmoroz): Add startup callback with process and require parameters.
// TODO(vmoroz): Generate the main script based on the runtime settings.
// TODO(vmoroz): Set the global Inspector for he main runtime.
// TODO(vmoroz): Start workers from C++.
// TODO(vmoroz): Worker to inherit parent Inspector.
// TODO(vmoroz): Cancel pending tasks on runtime deletion.
// TODO(vmoroz): Can we initialize platform again if it returns early?
// TODO(vmoroz): Simplify API use for simple default cases.
// TODO(vmoroz): Test passing the V8 thread pool size.
// TODO(vmoroz): Make the args story simpler or clear named.
// TODO(vmoroz): Single runtime by default vs multiple runtimes on demand.
// TODO(vmoroz): Add a way to terminate the runtime.
// TODO(vmoroz): Allow to provide custom thread pool from the app.
// TODO(vmoroz): Consider adding a v-table for the API functions to simplify
//               binding with other languages.
// TODO(vmoroz): libuv setup for the platform.
// TODO(vmoroz): Augment the node_embedding_run_nodejs_main with callbacks to
//               setup platform, setup runtime, and to run runtime.
// TODO(vmoroz): We must not exit the process on node::Environment errors.
// TODO(vmoroz): Be explicit about the recoverable errors.
