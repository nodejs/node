#ifndef TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
#define TEST_EMBEDDING_EMBEDTEST_NODE_API_H_

#define NAPI_EXPERIMENTAL
#include <node_embedding_api.h>

#ifdef __cplusplus

#include <functional>
#include <string>
#include <vector>

extern "C" inline void NAPI_CDECL GetArgsVector(void* data,
                                                int32_t argc,
                                                const char* argv[]) {
  static_cast<std::vector<std::string>*>(data)->assign(argv, argv + argc);
}

extern "C" inline node_embedding_exit_code NAPI_CDECL
HandleTestError(void* handler_data,
                const char* messages[],
                size_t messages_size,
                node_embedding_exit_code exit_code) {
  auto exe_name = static_cast<const char*>(handler_data);
  if (exit_code != 0) {
    for (size_t i = 0; i < messages_size; ++i)
      fprintf(stderr, "%s: %s\n", exe_name, messages[i]);
    exit(static_cast<int32_t>(exit_code));
  } else {
    for (size_t i = 0; i < messages_size; ++i) printf("%s\n", messages[i]);
  }
  return node_embedding_exit_code_ok;
}

#endif

extern const char* main_script;

napi_status AddUtf8String(std::string& str, napi_env env, napi_value value);

void GetAndThrowLastErrorMessage(napi_env env);

void ThrowLastErrorMessage(napi_env env, const char* message);

std::string FormatString(const char* format, ...);

inline node_embedding_exit_code RunMain(
    int32_t argc,
    char* argv[],
    const std::function<node_embedding_exit_code(
        node_embedding_platform_config)>& configurePlatform,
    const std::function<node_embedding_exit_code(
        node_embedding_platform, node_embedding_runtime_config)>&
        configureRuntime,
    const std::function<void(node_embedding_runtime, napi_env)>& runNodeApi) {
  return node_embedding_run_main(
      argc,
      argv,
      configurePlatform ?
        [](void* cb_data, node_embedding_platform_config platform_config) {
          auto configurePlatform = static_cast<
              std::function<node_embedding_exit_code(node_embedding_platform_config)>*>(
              cb_data);
          return (*configurePlatform)(platform_config);
        } : nullptr,
      const_cast<
          std::function<node_embedding_exit_code(node_embedding_platform_config)>*>(
          &configurePlatform),
      configureRuntime ?
        [](void* cb_data,
           node_embedding_platform platform,
           node_embedding_runtime_config runtime_config) {
          auto configureRuntime =
              static_cast<std::function<node_embedding_exit_code(
                  node_embedding_platform, node_embedding_runtime_config)>*>(cb_data);
          return (*configureRuntime)(platform, runtime_config);
        } : nullptr,
      const_cast<std::function<node_embedding_exit_code(
          node_embedding_platform, node_embedding_runtime_config)>*>(
          &configureRuntime),
      runNodeApi ?
        [](void* cb_data, node_embedding_runtime runtime, napi_env env) {
          auto runNodeApi =
              static_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
                  cb_data);
          (*runNodeApi)(runtime, env);
        } : nullptr,
      const_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
          &runNodeApi));
}

inline node_embedding_exit_code RunRuntime(
    node_embedding_platform platform,
    const std::function<node_embedding_exit_code(
        node_embedding_platform, node_embedding_runtime_config)>&
        configureRuntime,
    const std::function<void(node_embedding_runtime, napi_env)>& runNodeApi) {
  return node_embedding_run_runtime(
      platform,
      configureRuntime ?
        [](void* cb_data,
           node_embedding_platform platform,
           node_embedding_runtime_config runtime_config) {
          auto configureRuntime =
              static_cast<std::function<node_embedding_exit_code(
                  node_embedding_platform, node_embedding_runtime_config)>*>(cb_data);
          return (*configureRuntime)(platform, runtime_config);
        } : nullptr,
      const_cast<std::function<node_embedding_exit_code(
          node_embedding_platform, node_embedding_runtime_config)>*>(
          &configureRuntime),
      runNodeApi ?
        [](void* cb_data, node_embedding_runtime runtime, napi_env env) {
          auto runNodeApi =
              static_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
                  cb_data);
          (*runNodeApi)(runtime, env);
        } : nullptr,
      const_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
          &runNodeApi));
}

inline node_embedding_exit_code CreateRuntime(
    node_embedding_platform platform,
    const std::function<node_embedding_exit_code(
        node_embedding_platform, node_embedding_runtime_config)>&
        configureRuntime,
    node_embedding_runtime* runtime) {
  return node_embedding_create_runtime(
      platform,
      configureRuntime ?
        [](void* cb_data,
           node_embedding_platform platform,
           node_embedding_runtime_config runtime_config) {
          auto configureRuntime =
              static_cast<std::function<node_embedding_exit_code(
                  node_embedding_platform, node_embedding_runtime_config)>*>(cb_data);
          return (*configureRuntime)(platform, runtime_config);
        } : nullptr,
      const_cast<std::function<node_embedding_exit_code(
          node_embedding_platform, node_embedding_runtime_config)>*>(
          &configureRuntime),
      runtime);
}

inline node_embedding_exit_code RunNodeApi(
    node_embedding_runtime runtime,
    const std::function<void(node_embedding_runtime, napi_env)>& func) {
  return node_embedding_run_node_api(
      runtime,
      [](void* cb_data, node_embedding_runtime runtime, napi_env env) {
        auto func =
            static_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
                cb_data);
        (*func)(runtime, env);
      },
      const_cast<std::function<void(node_embedding_runtime, napi_env)>*>(
          &func));
}

//
// Error handling macros copied from test/js_native_api/common.h
//

// Empty value so that macros here are able to return NULL or void
#define NODE_API_RETVAL_NOTHING  // Intentionally blank #define

#define NODE_API_FAIL_BASE(ret_val, ...)                                       \
  do {                                                                         \
    ThrowLastErrorMessage(env, FormatString(__VA_ARGS__).c_str());             \
    return ret_val;                                                            \
  } while (0)

// Returns NULL on failed assertion.
// This is meant to be used inside napi_callback methods.
#define NODE_API_FAIL(...) NODE_API_FAIL_BASE(NULL, __VA_ARGS__)

// Returns empty on failed assertion.
// This is meant to be used inside functions with void return type.
#define NODE_API_FAIL_RETURN_VOID(...)                                         \
  NODE_API_FAIL_BASE(NODE_API_RETVAL_NOTHING, __VA_ARGS__)

#define NODE_API_ASSERT_BASE(expr, ret_val)                                    \
  do {                                                                         \
    if (!(expr)) {                                                             \
      napi_throw_error(env, NULL, "Failed: (" #expr ")");                      \
      return ret_val;                                                          \
    }                                                                          \
  } while (0)

// Returns NULL on failed assertion.
// This is meant to be used inside napi_callback methods.
#define NODE_API_ASSERT(expr) NODE_API_ASSERT_BASE(expr, NULL)

// Returns empty on failed assertion.
// This is meant to be used inside functions with void return type.
#define NODE_API_ASSERT_RETURN_VOID(expr)                                      \
  NODE_API_ASSERT_BASE(expr, NODE_API_RETVAL_NOTHING)

#define NODE_API_CALL_BASE(expr, ret_val)                                      \
  do {                                                                         \
    if ((expr) != napi_ok) {                                                   \
      GetAndThrowLastErrorMessage(env);                                        \
      return ret_val;                                                          \
    }                                                                          \
  } while (0)

// Returns NULL if the_call doesn't return napi_ok.
#define NODE_API_CALL(expr) NODE_API_CALL_BASE(expr, NULL)

// Returns empty if the_call doesn't return napi_ok.
#define NODE_API_CALL_RETURN_VOID(expr)                                        \
  NODE_API_CALL_BASE(expr, NODE_API_RETVAL_NOTHING)

#define CHECK_EXIT_CODE(expr)                                                  \
  do {                                                                         \
    node_embedding_exit_code exit_code = (expr);                               \
    if (exit_code != node_embedding_exit_code_ok) {                            \
      return exit_code;                                                        \
    }                                                                          \
  } while (0)

#define CHECK_EXIT_CODE_RETURN_VOID(expr)                                      \
  do {                                                                         \
    node_embedding_exit_code exit_code_ = (expr);                              \
    if (exit_code_ != node_embedding_exit_code_ok) {                           \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      exit(exit_code_);                                                        \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_TRUE(expr)                                                       \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      return node_embedding_exit_code_generic_user_error;                      \
    }                                                                          \
  } while (0)

#endif  // TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
