#ifndef SRC_NODE_API_EMBEDDING_H_
#define SRC_NODE_API_EMBEDDING_H_

#include "node_api.h"

EXTERN_C_START

typedef struct node_api_env_options__* node_api_env_options;

typedef enum {
  node_api_platform_no_flags = 0,
  // Enable stdio inheritance, which is disabled by default.
  // This flag is also implied by node_api_platform_no_stdio_initialization.
  node_api_platform_enable_stdio_inheritance = 1 << 0,
  // Disable reading the NODE_OPTIONS environment variable.
  node_api_platform_disable_node_options_env = 1 << 1,
  // Do not parse CLI options.
  node_api_platform_disable_cli_options = 1 << 2,
  // Do not initialize ICU.
  node_api_platform_no_icu = 1 << 3,
  // Do not modify stdio file descriptor or TTY state.
  node_api_platform_no_stdio_initialization = 1 << 4,
  // Do not register Node.js-specific signal handlers
  // and reset other signal handlers to default state.
  node_api_platform_no_default_signal_handling = 1 << 5,
  // Do not initialize OpenSSL config.
  node_api_platform_no_init_openssl = 1 << 8,
  // Do not initialize Node.js debugging based on environment variables.
  node_api_platform_no_parse_global_debug_variables = 1 << 9,
  // Do not adjust OS resource limits for this process.
  node_api_platform_no_adjust_resource_limits = 1 << 10,
  // Do not map code segments into large pages for this process.
  node_api_platform_no_use_large_pages = 1 << 11,
  // Skip printing output for --help, --version, --v8-options.
  node_api_platform_no_print_help_or_version_output = 1 << 12,
  // Initialize the process for predictable snapshot generation.
  node_api_platform_generate_predictable_snapshot = 1 << 14,
} node_api_platform_flags;

typedef enum : uint64_t {
  node_api_env_no_flags = 0,
  // Use the default behaviour for Node.js instances.
  node_api_env_default_flags = 1 << 0,
  // Controls whether this Environment is allowed to affect per-process state
  // (e.g. cwd, process title, uid, etc.).
  // This is set when using node_api_env_default_flags.
  node_api_env_owns_process_state = 1 << 1,
  // Set if this Environment instance is associated with the global inspector
  // handling code (i.e. listening on SIGUSR1).
  // This is set when using node_api_env_default_flags.
  node_api_env_owns_inspector = 1 << 2,
  // Set if Node.js should not run its own esm loader. This is needed by some
  // embedders, because it's possible for the Node.js esm loader to conflict
  // with another one in an embedder environment, e.g. Blink's in Chromium.
  node_api_env_no_register_esm_loader = 1 << 3,
  // Set this flag to make Node.js track "raw" file descriptors, i.e. managed
  // by fs.open() and fs.close(), and close them during node_api_delete_env().
  node_api_env_track_unmanaged_fds = 1 << 4,
  // Set this flag to force hiding console windows when spawning child
  // processes. This is usually used when embedding Node.js in GUI programs on
  // Windows.
  node_api_env_hide_console_windows = 1 << 5,
  // Set this flag to disable loading native addons via `process.dlopen`.
  // This environment flag is especially important for worker threads
  // so that a worker thread can't load a native addon even if `execArgv`
  // is overwritten and `--no-addons` is not specified but was specified
  // for this Environment instance.
  node_api_env_no_native_addons = 1 << 6,
  // Set this flag to disable searching modules from global paths like
  // $HOME/.node_modules and $NODE_PATH. This is used by standalone apps that
  // do not expect to have their behaviors changed because of globally
  // installed modules.
  node_api_env_no_global_search_paths = 1 << 7,
  // Do not export browser globals like setTimeout, console, etc.
  node_api_env_no_browser_globals = 1 << 8,
  // Controls whether or not the Environment should call V8Inspector::create().
  // This control is needed by embedders who may not want to initialize the V8
  // inspector in situations where one has already been created,
  // e.g. Blink's in Chromium.
  node_api_env_no_create_inspector = 1 << 9,
  // Controls whether or not the InspectorAgent for this Environment should
  // call StartDebugSignalHandler. This control is needed by embedders who may
  // not want to allow other processes to start the V8 inspector.
  node_api_env_no_start_debug_signal_handler = 1 << 10,
  // Controls whether the InspectorAgent created for this Environment waits for
  // Inspector frontend events during the Environment creation. It's used to
  // call node::Stop(env) on a Worker thread that is waiting for the events.
  node_api_env_no_wait_for_inspector_frontend = 1 << 11
} node_api_env_flags;

typedef enum {
  node_api_snapshot_no_flags = 0,
  // Whether code cache should be generated as part of the snapshot.
  // Code cache reduces the time spent on compiling functions included
  // in the snapshot at the expense of a bigger snapshot size and
  // potentially breaking portability of the snapshot.
  node_api_snapshot_no_code_cache = 1 << 0,
} node_api_snapshot_flags;

typedef void(NAPI_CDECL* node_api_error_message_handler)(void* handler_data,
                                                         const char* messages[],
                                                         size_t size);

typedef void(NAPI_CDECL* node_api_get_args_callback)(void* cb_data,
                                                     int32_t argc,
                                                     const char* size[]);

typedef void(NAPI_CDECL* node_api_store_blob_callback)(void* cb_data,
                                                       const uint8_t* blob,
                                                       size_t size);

typedef void(NAPI_CDECL* node_api_preload_callback)(napi_env env,
                                                    napi_value process,
                                                    napi_value require,
                                                    void* cb_data);

typedef bool(NAPI_CDECL* node_api_run_predicate)(void* predicate_data);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_initialize_platform(int32_t argc,
                             char* argv[],
                             node_api_platform_flags flags,
                             node_api_error_message_handler error_handler,
                             void* error_handler_data,
                             bool* early_return,
                             int32_t* exit_code);

NAPI_EXTERN napi_status NAPI_CDECL node_api_dispose_platform();

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_env_options(node_api_env_options* result);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_get_args(node_api_env_options options,
                              node_api_get_args_callback get_args_cb,
                              void* cb_data);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_get_exec_args(node_api_env_options options,
                                   node_api_get_args_callback get_args_cb,
                                   void* cb_data);

NAPI_EXTERN napi_status NAPI_CDECL node_api_env_options_set_flags(
    node_api_env_options options, node_api_env_flags flags);

NAPI_EXTERN napi_status NAPI_CDECL node_api_env_options_set_args(
    node_api_env_options options, size_t argc, const char* argv[]);

NAPI_EXTERN napi_status NAPI_CDECL node_api_env_options_set_exec_args(
    node_api_env_options options, size_t argc, const char* argv[]);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_set_preload_callback(node_api_env_options options,
                                          node_api_preload_callback preload_cb,
                                          void* cb_data);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_use_snapshot(node_api_env_options options,
                                  const char* snapshot_data,
                                  size_t snapshot_size);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_create_snapshot(node_api_env_options options,
                                     node_api_store_blob_callback store_blob_cb,
                                     void* cb_data,
                                     node_api_snapshot_flags snapshot_flags);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_env(node_api_env_options options,
                    node_api_error_message_handler error_handler,
                    void* error_handler_data,
                    const char* main_script,
                    int32_t api_version,
                    napi_env* result);

NAPI_EXTERN napi_status NAPI_CDECL node_api_delete_env(napi_env env,
                                                       int* exit_code);

NAPI_EXTERN napi_status NAPI_CDECL node_api_open_env_scope(napi_env env);

NAPI_EXTERN napi_status NAPI_CDECL node_api_close_env_scope(napi_env env);

NAPI_EXTERN napi_status NAPI_CDECL node_api_run_env(napi_env env);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_run_env_while(napi_env env,
                       node_api_run_predicate predicate,
                       void* predicate_data,
                       bool* has_more_work);

NAPI_EXTERN napi_status NAPI_CDECL node_api_await_promise(napi_env env,
                                                          napi_value promise,
                                                          napi_value* result,
                                                          bool* has_more_work);

EXTERN_C_END

#ifdef __cplusplus

inline node_api_platform_flags operator|(node_api_platform_flags lhs,
                                         node_api_platform_flags rhs) {
  return static_cast<node_api_platform_flags>(static_cast<int32_t>(lhs) |
                                              static_cast<int32_t>(rhs));
}

inline node_api_env_flags operator|(node_api_env_flags lhs,
                                    node_api_env_flags rhs) {
  return static_cast<node_api_env_flags>(static_cast<uint64_t>(lhs) |
                                         static_cast<uint64_t>(rhs));
}

inline node_api_snapshot_flags operator|(node_api_snapshot_flags lhs,
                                         node_api_snapshot_flags rhs) {
  return static_cast<node_api_snapshot_flags>(static_cast<int32_t>(lhs) |
                                              static_cast<int32_t>(rhs));
}

#endif

#endif  // SRC_NODE_API_EMBEDDING_H_

// TODO: (vmoroz) Remove the main_script parameter.
// TODO: (vmoroz) Add startup callback with process and require parameters.
// TODO: (vmoroz) Add ABI-safe way to access internal module functionality.
// TODO: (vmoroz) Allow setting the global inspector for a specific environment.
// TODO: (vmoroz) Start workers from C++.
// TODO: (vmoroz) Worker to inherit parent inspector.
// TODO: (vmoroz) Cancel pending tasks on delete env.
// TODO: (vmoroz) Can we init plat again if it retuns early?
// TODO: (vmoroz) Add simpler threading model - without open/close scope.
// TODO: (vmoroz) Simplify API use for simple default cases.
// TODO: (vmoroz) Add a way to add embedded modules.
// TODO: (vmoroz) Check how to pass the V8 thread pool size.
// TODO: (vmoroz) Provide better error handling for args.
