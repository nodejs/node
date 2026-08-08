#ifndef SRC_NODE_CONCEPTS_H_
#define SRC_NODE_CONCEPTS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <concepts>
#include <limits>
#include <type_traits>

namespace node {

// A numeric type recognized by std::numeric_limits.
template <typename T>
concept NumericValue = std::numeric_limits<T>::is_specialized;

// A numeric type or an enum (which has an underlying numeric type).
template <typename T>
concept NumericOrEnum = NumericValue<T> || std::is_enum_v<T>;

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CONCEPTS_H_
