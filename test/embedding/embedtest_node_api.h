#ifndef TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
#define TEST_EMBEDDING_EMBEDTEST_NODE_API_H_

#define NAPI_EXPERIMENTAL
#include <node_api_embedding.h>

#ifdef __cplusplus

#include <string>
#include <vector>

extern "C" inline void NAPI_CDECL GetMessageVector(void* data,
                                                   const char* messages[],
                                                   size_t size) {
  static_cast<std::vector<std::string>*>(data)->assign(messages,
                                                       messages + size);
}

extern "C" inline void NAPI_CDECL GetArgsVector(void* data,
                                                int32_t argc,
                                                const char* argv[]) {
  static_cast<std::vector<std::string>*>(data)->assign(argv, argv + argc);
}

#endif

#define CHECK(expr)                                                            \
  do {                                                                         \
    if ((expr) != napi_ok) {                                                   \
      fprintf(stderr, "Failed: %s\n", #expr);                                  \
      fprintf(stderr, "File: %s\n", __FILE__);                                 \
      fprintf(stderr, "Line: %d\n", __LINE__);                                 \
      return 1;                                                                \
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

#define FAIL(...)                                                              \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    return 1;                                                                  \
  } while (0)

#define CHECK_EXIT_CODE(code)                                                  \
  do {                                                                         \
    int exit_code = (code);                                                    \
    if (exit_code != 0) {                                                      \
      return exit_code;                                                        \
    }                                                                          \
  } while (0)

#endif  // TEST_EMBEDDING_EMBEDTEST_NODE_API_H_
