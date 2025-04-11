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

extern "C" inline node_embedding_status NAPI_CDECL
HandleTestError(void* handler_data,
                const char* messages[],
                size_t messages_size,
                node_embedding_status status) {
  const char* exe_name = static_cast<const char*>(handler_data);
  if (status != node_embedding_status_ok) {
    for (size_t i = 0; i < messages_size; ++i) {
      fprintf(stderr, "%s: %s\n", exe_name, messages[i]);
    }
    int32_t exit_code = ((status & node_embedding_status_error_exit_code) != 0)
                            ? status & ~node_embedding_status_error_exit_code
                            : 1;
    exit(exit_code);
  } else {
    for (size_t i = 0; i < messages_size; ++i) {
      printf("%s\n", messages[i]);
    }
  }
  return node_embedding_status_ok;
}

#endif

extern const char* main_script;

napi_status AddUtf8String(std::string& str, napi_env env, napi_value value);

void GetAndThrowLastErrorMessage(napi_env env);

void ThrowLastErrorMessage(napi_env env, const char* message);

std::string FormatString(const char* format, ...);

template <typename TLambda, typename TFunctor>
struct Adapter {
  static_assert(sizeof(TLambda) == -1, "Unsupported signature");
};

template <typename TLambda, typename TResult, typename... TArgs>
struct Adapter<TLambda, TResult(void*, TArgs...)> {
  static TResult Invoke(void* data, TArgs... args) {
    return reinterpret_cast<TLambda*>(data)->operator()(args...);
  }
};

template <typename TFunctor, typename TLambda>
inline TFunctor AsFunctorRef(TLambda&& lambda) {
  using TLambdaType = std::remove_reference_t<TLambda>;
  using TAdapter =
      Adapter<TLambdaType,
              std::remove_pointer_t<
                  decltype(std::remove_reference_t<TFunctor>::invoke)>>;
  return TFunctor{static_cast<void*>(&lambda), &TAdapter::Invoke};
}

template <typename TFunctor, typename TLambda>
inline TFunctor AsFunctor(TLambda&& lambda) {
  using TLambdaType = std::remove_reference_t<TLambda>;
  using TAdapter =
      Adapter<TLambdaType,
              std::remove_pointer_t<
                  decltype(std::remove_reference_t<TFunctor>::invoke)>>;
  return TFunctor{
      static_cast<void*>(new TLambdaType(std::forward<TLambdaType>(lambda))),
      &TAdapter::Invoke,
      [](void* data) { delete static_cast<TLambdaType*>(data); }};
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

#define CHECK_STATUS(expr)                                                     \
  do {                                                                         \
    node_embedding_status status_ = (expr);                                    \
    if (status_ != node_embedding_status_ok) {                                 \
      return status_;                                                          \
    }                                                                          \
  } while (0)

#define CHECK_STATUS_OR_EXIT(expr)                                             \
  do {                                                                         \
    node_embedding_status status_ = (expr);                                    \
    if (status_ != node_embedding_status_ok) {                                 \
      int32_t exit_code =                                                      \
          ((status_ & node_embedding_status_error_exit_code) != 0)             \
              ? status_ & ~node_embedding_status_error_exit_code               \
              : 1;                                                             \
      exit(exit_code);                                                         \
    }                                                                          \
  } while (0)

#define ASSERT_OR_EXIT(expr)                                                   \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#endif  // TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
