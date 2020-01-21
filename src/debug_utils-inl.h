#ifndef SRC_DEBUG_UTILS_INL_H_
#define SRC_DEBUG_UTILS_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "debug_utils.h"

#include <type_traits>

namespace node {

struct ToStringHelper {
  template <typename T>
  static std::string Convert(
      const T& value,
      std::string(T::* to_string)() const = &T::ToString) {
    return (value.*to_string)();
  }
  template <typename T,
            typename test_for_number = typename std::
                enable_if<std::is_arithmetic<T>::value, bool>::type,
            typename dummy = bool>
  static std::string Convert(const T& value) { return std::to_string(value); }
  static std::string Convert(const char* value) { return value; }
  static std::string Convert(const std::string& value) { return value; }
  static std::string Convert(bool value) { return value ? "true" : "false"; }
};

template <typename T>
std::string ToString(const T& value) {
  return ToStringHelper::Convert(value);
}

inline std::string SPrintFImpl(const char* format) {
  return format;
}

template <typename Arg, typename... Args>
std::string COLD_NOINLINE SPrintFImpl(  // NOLINT(runtime/string)
    const char* format, Arg&& arg, Args&&... args) {
  const char* p = strchr(format, '%');
  if (p == nullptr) return format;
  std::string ret(format, p);
  // Ignore long / size_t modifiers
  while (strchr("lz", *++p) != nullptr) {}
  switch (*p) {
    case '%': {
      return ret + '%' + SPrintFImpl(p + 1,
                                     std::forward<Arg>(arg),
                                     std::forward<Args>(args)...);
    }
    default: {
      return ret + '%' + SPrintFImpl(p,
                                     std::forward<Arg>(arg),
                                     std::forward<Args>(args)...);
    }
    case 'd':
    case 'i':
    case 'u':
    case 's': ret += ToString(arg); break;
    case 'p': {
      CHECK(std::is_pointer<typename std::remove_reference<Arg>::type>::value);
      char out[20];
      int n = snprintf(out,
                       sizeof(out),
                       "%p",
                       *reinterpret_cast<const void* const*>(&arg));
      CHECK_GE(n, 0);
      ret += out;
      break;
    }
  }
  return ret + SPrintFImpl(p + 1, std::forward<Args>(args)...);
}

template <typename... Args>
std::string COLD_NOINLINE SPrintF(  // NOLINT(runtime/string)
    const char* format, Args&&... args) {
  return SPrintFImpl(format, std::forward<Args>(args)...);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_DEBUG_UTILS_INL_H_
