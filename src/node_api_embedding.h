#ifndef SRC_NODE_API_EMBEDDING_H_
#define SRC_NODE_API_EMBEDDING_H_

#include "node_api.h"

EXTERN_C_START

typedef struct node_api_env_options__* node_api_env_options;

typedef enum {
  node_api_platform_no_flags = 0,
  // Enable reading the NODE_OPTIONS environment variable.
  node_api_platform_enable_env_var = 1 << 0,
  // Initialize ICU.
  node_api_platform_init_icu = 1 << 1,
  // Initialize OpenSSL config.
  node_api_platform_init_openssl = 1 << 2,
  // Initialize Node.js debugging based on environment variables.
  node_api_platform_parse_global_debug_vars = 1 << 3,
  // Adjust OS resource limits for this process.
  node_api_platform_adjust_resource_limits = 1 << 4,
  // Do not map code segments into large pages for this process.
  node_api_platform_no_large_pages = 1 << 5,
  // Allow printing output for --help, --version, --v8-options.
  node_api_platform_print_help_or_version = 1 << 6,
  // Initialize the process for predictable snapshot generation.
  node_api_platform_generate_predictable_snapshot = 1 << 7,
} node_api_platform_flags;

typedef enum {
  node_api_snapshot_no_flags = 0,
  // Whether code cache should be generated as part of the snapshot.
  // Code cache reduces the time spent on compiling functions included
  // in the snapshot at the expense of a bigger snapshot size and
  // potentially breaking portability of the snapshot.
  node_api_snapshot_no_code_cache = 1 << 0,
} node_api_snapshot_flags;

typedef void(NAPI_CDECL* node_api_get_strings_callback)(
    void* data, size_t str_count, const char* str_array[]);

typedef void(NAPI_CDECL* node_api_get_string_callback)(void* cb_data,
                                                       const char* str,
                                                       size_t size);

typedef bool(NAPI_CDECL* node_api_run_predicate)(void* predicate_data);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_init_once_per_process(int32_t argc,
                               char* argv[],
                               node_api_platform_flags flags,
                               node_api_get_strings_callback get_errors_cb,
                               void* errors_data,
                               bool* early_return,
                               int32_t* exit_code);

NAPI_EXTERN napi_status NAPI_CDECL node_api_uninit_once_per_process();

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_env_options(node_api_env_options* result);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_get_args(node_api_env_options options,
                              node_api_get_strings_callback get_strings_cb,
                              void* strings_data);

NAPI_EXTERN napi_status NAPI_CDECL node_api_env_options_set_args(
    node_api_env_options options, size_t argc, const char* argv[]);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_get_exec_args(node_api_env_options options,
                                   node_api_get_strings_callback get_strings_cb,
                                   void* strings_data);

NAPI_EXTERN napi_status NAPI_CDECL node_api_env_options_set_exec_args(
    node_api_env_options options, size_t argc, const char* argv[]);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_set_snapshot(node_api_env_options options,
                                  const char* snapshot_data,
                                  size_t snapshot_size);

NAPI_EXTERN napi_status NAPI_CDECL
node_api_env_options_create_snapshot(node_api_env_options options,
                                     node_api_get_string_callback get_blob_cb,
                                     void* blob_cb_data,
                                     node_api_snapshot_flags snapshot_flags);

// TODO(vmoroz): Remove the main_script parameter.
// TODO(vmoroz): Add ABI-safe way to access internal module functionality.
// TODO(vmoroz): Pass EnvironmentFlags
// TODO(vmoroz): Allow setting the global inspector for a specific environment.
NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_env(node_api_env_options options,
                    node_api_get_strings_callback get_errors_cb,
                    void* errors_data,
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
                                                          napi_value* result);

EXTERN_C_END

#endif  // SRC_NODE_API_EMBEDDING_H_
