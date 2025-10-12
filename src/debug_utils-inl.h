#ifndef SRC_DEBUG_UTILS_INL_H_
#define SRC_DEBUG_UTILS_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "debug_utils.h"
#include "env.h"
#include "util-inl.h"

#include <type_traits>

namespace node {

template <typename T>
concept StringViewConvertible = requires(T a) {
                                  {
                                    a.ToStringView()
                                    } -> std::convertible_to<std::string_view>;
                                };
template <typename T>
concept StringConvertible = requires(T a) {
                              {
                                a.ToString()
                                } -> std::convertible_to<std::string>;
                            };

struct ToStringHelper {
  template <typename T>
    requires(StringConvertible<T>) && (!StringViewConvertible<T>)
  static std::string Convert(const T& value) {
    return value.ToString();
  }
  template <typename T>
    requires StringViewConvertible<T>
  static std::string_view Convert(const T& value) {
    return value.ToStringView();
  }

  template <typename T,
            typename test_for_number = typename std::
                enable_if<std::is_arithmetic<T>::value, bool>::type,
            typename dummy = bool>
  static std::string Convert(const T& value) { return std::to_string(value); }
  static std::string_view Convert(const char* value) {
    return value != nullptr ? value : "(null)";
  }
  static std::string Convert(const std::string& value) { return value; }
  static std::string_view Convert(std::string_view value) { return value; }
  static std::string Convert(bool value) { return value ? "true" : "false"; }
  template <unsigned BASE_BITS,
            typename T,
            typename = std::enable_if_t<std::is_integral_v<T>>>
  static std::string BaseConvert(const T& value) {
    auto v = static_cast<uint64_t>(value);
    char ret[3 * sizeof(T)];
    char* ptr = ret + 3 * sizeof(T) - 1;
    *ptr = '\0';
    const char* digits = "0123456789abcdef";
    do {
      unsigned digit = v & ((1 << BASE_BITS) - 1);
      *--ptr =
          (BASE_BITS < 4 ? static_cast<char>('0' + digit) : digits[digit]);
    } while ((v >>= BASE_BITS) != 0);
    return ptr;
  }
  template <unsigned BASE_BITS,
            typename T,
            typename = std::enable_if_t<!std::is_integral_v<T>>>
  static auto BaseConvert(T&& value) {
    return Convert(std::forward<T>(value));
  }
};

template <typename T>
auto ToStringOrStringView(const T& value) {
  return ToStringHelper::Convert(value);
}

template <typename T>
std::string ToString(const T& value) {
  return std::string(ToStringOrStringView(value));
}

template <unsigned BASE_BITS, typename T>
auto ToBaseString(const T& value) {
  return ToStringHelper::BaseConvert<BASE_BITS>(value);
}

inline std::string SPrintFImpl(std::string_view format) {
  auto offset = format.find('%');
  if (offset == std::string_view::npos) return std::string(format);
  CHECK_LT(offset + 1, format.size());
  CHECK_EQ(format[offset + 1],
           '%');  // Only '%%' allowed when there are no arguments.

  return std::string(format.substr(0, offset + 1)) +
         SPrintFImpl(format.substr(offset + 2));
}

template <typename Arg, typename... Args>
std::string COLD_NOINLINE SPrintFImpl(  // NOLINT(runtime/string)
    std::string_view format,
    Arg&& arg,
    Args&&... args) {
  auto offset = format.find('%');
  CHECK_NE(offset, std::string_view::npos);  // If you hit this, you passed in
                                             // too many arguments.
  std::string ret(format.substr(0, offset));
  // Ignore long / size_t modifiers
  while (++offset < format.size() &&
         (format[offset] == 'l' || format[offset] == 'z')) {
  }
  switch (offset == format.size() ? '\0' : format[offset]) {
    case '%': {
      return ret + '%' +
             SPrintFImpl(format.substr(offset + 1),
                         std::forward<Arg>(arg),
                         std::forward<Args>(args)...);
    }
    default: {
      return ret + '%' +
             SPrintFImpl(format.substr(offset),
                         std::forward<Arg>(arg),
                         std::forward<Args>(args)...);
    }
    case 'd':
    case 'i':
    case 'u':
    case 's':
      ret += ToStringOrStringView(arg);
      break;
    case 'o':
      ret += ToBaseString<3>(arg);
      break;
    case 'x':
      ret += ToBaseString<4>(arg);
      break;
    case 'X':
      ret += node::ToUpper(ToBaseString<4>(arg));
      break;
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
  return ret +
         SPrintFImpl(format.substr(offset + 1), std::forward<Args>(args)...);
}

template <typename... Args>
std::string COLD_NOINLINE SPrintF(  // NOLINT(runtime/string)
    std::string_view format,
    Args&&... args) {
  return SPrintFImpl(format, std::forward<Args>(args)...);
}

template <typename... Args>
void COLD_NOINLINE FPrintF(FILE* file,
                           std::string_view format,
                           Args&&... args) {
  FWrite(file, SPrintF(format, std::forward<Args>(args)...));
}

template <typename... Args>
inline void FORCE_INLINE Debug(EnabledDebugList* list,
                               DebugCategory cat,
                               const char* format,
                               Args&&... args) {
  if (!list->enabled(cat)) [[unlikely]]
    return;
  FPrintF(stderr, format, std::forward<Args>(args)...);
}

inline void FORCE_INLINE Debug(EnabledDebugList* list,
                               DebugCategory cat,
                               const char* message) {
  if (!list->enabled(cat)) [[unlikely]]
    return;
  FPrintF(stderr, "%s", message);
}

template <typename... Args>
inline void FORCE_INLINE
Debug(Environment* env, DebugCategory cat, const char* format, Args&&... args) {
  Debug(env->enabled_debug_list(), cat, format, std::forward<Args>(args)...);
}

inline void FORCE_INLINE Debug(Environment* env,
                               DebugCategory cat,
                               const char* message) {
  Debug(env->enabled_debug_list(), cat, message);
}

template <typename... Args>
inline void Debug(Environment* env,
                  DebugCategory cat,
                  const std::string& format,
                  Args&&... args) {
  Debug(env->enabled_debug_list(),
        cat,
        format.c_str(),
        std::forward<Args>(args)...);
}

// Used internally by the 'real' Debug(AsyncWrap*, ...) functions below, so that
// the FORCE_INLINE flag on them doesn't apply to the contents of this function
// as well.
// We apply COLD_NOINLINE to tell the compiler that it's not worth optimizing
// this function for speed and it should rather focus on keeping it out of
// hot code paths. In particular, we want to keep the string concatenating code
// out of the function containing the original `Debug()` call.
template <typename... Args>
void COLD_NOINLINE UnconditionalAsyncWrapDebug(AsyncWrap* async_wrap,
                                               const char* format,
                                               Args&&... args) {
  Debug(async_wrap->env(),
        static_cast<DebugCategory>(async_wrap->provider_type()),
        async_wrap->diagnostic_name() + " " + format + "\n",
        std::forward<Args>(args)...);
}

template <typename... Args>
inline void FORCE_INLINE Debug(AsyncWrap* async_wrap,
                               const char* format,
                               Args&&... args) {
  DCHECK_NOT_NULL(async_wrap);
  if (auto cat = static_cast<DebugCategory>(async_wrap->provider_type());
      !async_wrap->env()->enabled_debug_list()->enabled(cat)) [[unlikely]] {
    return;
  }
  UnconditionalAsyncWrapDebug(async_wrap, format, std::forward<Args>(args)...);
}

template <typename... Args>
inline void FORCE_INLINE Debug(AsyncWrap* async_wrap,
                               const std::string& format,
                               Args&&... args) {
  Debug(async_wrap, format.c_str(), std::forward<Args>(args)...);
}

namespace per_process {

template <typename... Args>
inline void FORCE_INLINE Debug(DebugCategory cat,
                               const char* format,
                               Args&&... args) {
  Debug(&enabled_debug_list, cat, format, std::forward<Args>(args)...);
}

inline void FORCE_INLINE Debug(DebugCategory cat, const char* message) {
  Debug(&enabled_debug_list, cat, message);
}

}  // namespace per_process
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_DEBUG_UTILS_INL_H_
