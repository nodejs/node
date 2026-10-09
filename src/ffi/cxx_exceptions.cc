#include "ffi/cxx_exceptions.h"

#include <exception>

namespace node::cxx_exceptions {

struct CxxException final : std::exception {
  CxxExceptionInfo info;
  CxxException(const CxxExceptionInfo& info) : info(info) {}
  const char* what() const noexcept override { return info.message.c_str(); }
};

void CxxExceptionThrow(const CxxExceptionInfo& exception) {
  throw CxxException(exception);
}

std::optional<CxxExceptionInfo> CxxExceptionCatch(void (*fn)(void*),
                                                  void* arg) noexcept {
  try {
    fn(arg);
    return std::nullopt;
  } catch (const CxxException& e) {
    return e.info;
  } catch (const std::exception& e) {
    return CxxExceptionInfo{.message = e.what()};
  } catch (...) {
    return CxxExceptionInfo{.message = "Unknown exception"};
  }
}

}  // namespace node::cxx_exceptions
