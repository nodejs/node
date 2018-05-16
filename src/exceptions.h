#ifndef SRC_EXCEPTIONS_H_
#define SRC_EXCEPTIONS_H_

#include "core.h"
#include "v8.h"

namespace node {

NODE_EXTERN v8::Local<v8::Value> ErrnoException(v8::Isolate* isolate,
                                                int errorno,
                                                const char* syscall = nullptr,
                                                const char* message = nullptr,
                                                const char* path = nullptr);
NODE_EXTERN v8::Local<v8::Value> UVException(v8::Isolate* isolate,
                                             int errorno,
                                             const char* syscall = nullptr,
                                             const char* message = nullptr,
                                             const char* path = nullptr);
NODE_EXTERN v8::Local<v8::Value> UVException(v8::Isolate* isolate,
                                             int errorno,
                                             const char* syscall,
                                             const char* message,
                                             const char* path,
                                             const char* dest);

NODE_DEPRECATED(
    "Use ErrnoException(isolate, ...)",
    inline v8::Local<v8::Value> ErrnoException(
        int errorno,
        const char* syscall = nullptr,
        const char* message = nullptr,
        const char* path = nullptr) {
  return ErrnoException(v8::Isolate::GetCurrent(),
                        errorno,
                        syscall,
                        message,
                        path);
})

inline v8::Local<v8::Value> UVException(int errorno,
                                        const char* syscall = nullptr,
                                        const char* message = nullptr,
                                        const char* path = nullptr) {
  return UVException(v8::Isolate::GetCurrent(),
                     errorno,
                     syscall,
                     message,
                     path);
}

#ifdef _WIN32
NODE_EXTERN v8::Local<v8::Value> WinapiErrnoException(
    v8::Isolate* isolate,
    int errorno,
    const char *syscall = nullptr,
    const char *msg = "",
    const char *path = nullptr);

NODE_DEPRECATED(
    "Use WinapiErrnoException(isolate, ...)",
    inline v8::Local<v8::Value> WinapiErrnoException(
        int errorno,
        const char *syscall = nullptr,
        const char *msg = "",
        const char *path = nullptr) {
  return WinapiErrnoException(v8::Isolate::GetCurrent(),
                              errorno,
                              syscall,
                              msg,
                              path);
})
#endif

}  // namespace node

#endif  // SRC_EXCEPTIONS_H_
