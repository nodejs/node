#ifndef SRC_NODE_CONCEPTS_H_
#define SRC_NODE_CONCEPTS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <v8.h>
#include <concepts>
#include <limits>
#include <type_traits>

namespace node {

// A numeric type recognized by std::numeric_limits.
// The is_array guard prevents a hard error from instantiating
// numeric_limits<T[N]>, whose member functions would return array types.
template <typename T>
concept NumericValue = !
std::is_array_v<T>&& std::numeric_limits<T>::is_specialized;

// A numeric type or an enum (which has an underlying numeric type).
template <typename T>
concept NumericOrEnum = NumericValue<T> || std::is_enum_v<T>;

// A type that has a valid std::char_traits specialization, as required by
// std::basic_string and std::basic_string_view.
template <typename T>
concept StandardCharType =
    std::is_same_v<T, char> || std::is_same_v<T, wchar_t> ||
    std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> ||
    std::is_same_v<T, char32_t>;

// Test whether some value can be called with ().
template <typename T>
concept IsCallable = std::is_function<T>::value || requires { &T::operator(); };

// Types that can reside on V8's managed heap (v8::Value, v8::Object, etc.).
// Used to select the MaybeStackBuffer specialization that holds handles in a
// v8::LocalVector instead of malloc'd memory.
template <typename T>
concept V8Type = std::is_base_of_v<v8::Data, T>;

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CONCEPTS_H_
