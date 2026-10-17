#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>
#include <optional>
#include <string>

// Using forward declarations to avoid including V8 headers
// and inlining V8 code by accident.
namespace v8 {
template <typename T>
class Global;
class Value;
}  // namespace v8

namespace node::cxx_exceptions {

// `js_exception` is set when the original error is a JS exception that should
// be rethrown as-is instead of being reported only through `message`.
struct CxxExceptionInfo {
  std::string message;
  std::shared_ptr<v8::Global<v8::Value>> js_exception = {};
};

// Throws `exception` as a C++ exception; must be caught via CxxExceptionCatch()
// or catch { ... }.
void CxxExceptionThrow(const CxxExceptionInfo& exception);
// Runs fn(arg), catching any C++ exception thrown through it (including ones
// thrown by CxxExceptionThrow() further down the call stack).
std::optional<CxxExceptionInfo> CxxExceptionCatch(void (*fn)(void*),
                                                  void* arg) noexcept;
template <typename T>
std::optional<CxxExceptionInfo> CxxExceptionCatch(T fn) noexcept {
  return CxxExceptionCatch([](void* ptr) { (*static_cast<T*>(ptr))(); },
                           static_cast<void*>(&fn));
}

}  // namespace node::cxx_exceptions

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
