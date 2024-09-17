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
  return exit_code;
}

#endif

extern const char* main_script;

napi_status AddUtf8String(std::string& str, napi_env env, napi_value value);

void GetAndThrowLastErrorMessage(napi_env env);

inline node_embedding_exit_code InvokeNodeApi(
    node_embedding_runtime runtime, const std::function<void(napi_env)>& func) {
  return node_embedding_runtime_invoke_node_api(
      runtime,
      [](node_embedding_runtime runtime, void* cb_data, napi_env env) {
        auto func = static_cast<std::function<void(napi_env)>*>(cb_data);
        (*func)(env);
      },
      const_cast<std::function<void(napi_env)>*>(&func));
}

#define NODE_API_CALL(expr)                                                    \
  do {                                                                         \
    if ((expr) != napi_ok) {                                                   \
      GetAndThrowLastErrorMessage(env);                                        \
      exit_code = 1;                                                           \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK(expr)                                                            \
  do {                                                                         \
    if ((expr) != node_embedding_exit_code_ok) {                               \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define CHECK_RETURN_VOID(expr)                                                \
  do {                                                                         \
    if ((expr) != node_embedding_exit_code_ok) {                               \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      exit_code = 1;                                                           \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_TRUE(expr)                                                       \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define CHECK_TRUE_RETURN_VOID(expr)                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      exit_code = 1;                                                           \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define FAIL(...)                                                              \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    return 1;                                                                  \
  } while (0)

#define FAIL_RETURN_VOID(...)                                                  \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit_code = 1;                                                             \
    return;                                                                    \
  } while (0)

#define CHECK_EXIT_CODE(code)                                                  \
  do {                                                                         \
    int exit_code = (code);                                                    \
    if (exit_code != 0) {                                                      \
      return exit_code;                                                        \
    }                                                                          \
  } while (0)

#endif  // TEST_EMBEDDING_EMBEDTEST_NODE_API_H_

// TODO(vmoroz): Enable the test_main_modules_node_api test.
// TODO(vmoroz): Test failure in Preload callback.
// TODO(vmoroz): Test failure in linked modules.
// TODO(vmoroz): Add a test that handles JS errors.
// TODO(vmoroz): Make sure that delete call matches the create call.
