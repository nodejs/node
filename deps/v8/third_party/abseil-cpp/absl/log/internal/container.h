// Copyright 2025 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// The typical use looks like this:
//
//   LOG(INFO) << LogContainer(container);
//
// By default, LogContainer() uses the LogShortUpTo100 policy: comma-space
// separation, no newlines, and with limit of 100 items.
//
// Policies can be specified:
//
//   LOG(INFO) << LogContainer(container, LogMultiline());
//
// The above example will print the container using newlines between elements,
// enclosed in [] braces.
//
// See below for further details on policies.

#ifndef ABSL_LOG_INTERNAL_CONTAINER_H_
#define ABSL_LOG_INTERNAL_CONTAINER_H_

#include <cstdint>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

#include "absl/base/config.h"
#include "absl/meta/internal/requires.h"
#include "absl/strings/str_cat.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace log_internal {

// Several policy classes below determine how LogRangeToStream will
// format a range of items.  A Policy class should have these methods:
//
// Called to print an individual container element.
//   void Log(ostream &out, const ElementT &element) const;
//
// Called before printing the set of elements:
//   void LogOpening(ostream &out) const;
//
// Called after printing the set of elements:
//   void LogClosing(ostream &out) const;
//
// Called before printing the first element:
//   void LogFirstSeparator(ostream &out) const;
//
// Called before printing the remaining elements:
//   void LogSeparator(ostream &out) const;
//
// Returns the maximum number of elements to print:
//   int64 MaxElements() const;
//
// Called to print an indication that MaximumElements() was reached:
//   void LogEllipsis(ostream &out) const;

namespace internal {

struct LogBase {
  template <typename ElementT>
  void Log(std::ostream &out, const ElementT &element) const {  // NOLINT
    // Fallback to `AbslStringify` if the type does not have `operator<<`.
    if constexpr (meta_internal::Requires<std::ostream, ElementT>(
                      [](auto&& x, auto&& y) -> decltype(x << y) {})) {
      out << element;
    } else {
      out << absl::StrCat(element);
    }
  }
  void LogEllipsis(std::ostream &out) const {  // NOLINT
    out << "...";
  }
};

struct LogShortBase : public LogBase {
  void LogOpening(std::ostream &out) const { out << "["; }        // NOLINT
  void LogClosing(std::ostream &out) const { out << "]"; }        // NOLINT
  void LogFirstSeparator(std::ostream &out) const { out << ""; }  // NOLINT
  void LogSeparator(std::ostream &out) const { out << ", "; }     // NOLINT
};

struct LogMultilineBase : public LogBase {
  void LogOpening(std::ostream &out) const { out << "["; }          // NOLINT
  void LogClosing(std::ostream &out) const { out << "\n]"; }        // NOLINT
  void LogFirstSeparator(std::ostream &out) const { out << "\n"; }  // NOLINT
  void LogSeparator(std::ostream &out) const { out << "\n"; }       // NOLINT
};

struct LogLegacyBase : public LogBase {
  void LogOpening(std::ostream &out) const { out << ""; }         // NOLINT
  void LogClosing(std::ostream &out) const { out << ""; }         // NOLINT
  void LogFirstSeparator(std::ostream &out) const { out << ""; }  // NOLINT
  void LogSeparator(std::ostream &out) const { out << " "; }      // NOLINT
};

}  // namespace internal

// LogShort uses [] braces and separates items with comma-spaces.  For
// example "[1, 2, 3]".
struct LogShort : public internal::LogShortBase {
  int64_t MaxElements() const { return (std::numeric_limits<int64_t>::max)(); }
};

// LogShortUpToN(max_elements) formats the same as LogShort but prints no more
// than the max_elements elements.
class LogShortUpToN : public internal::LogShortBase {
 public:
  explicit LogShortUpToN(int64_t max_elements) : max_elements_(max_elements) {}
  int64_t MaxElements() const { return max_elements_; }

 private:
  int64_t max_elements_;
};

// LogShortUpTo100 formats the same as LogShort but prints no more
// than 100 elements.
struct LogShortUpTo100 : public LogShortUpToN {
  LogShortUpTo100() : LogShortUpToN(100) {}
};

// LogMultiline uses [] braces and separates items with
// newlines.  For example "[
// 1
// 2
// 3
// ]".
struct LogMultiline : public internal::LogMultilineBase {
  int64_t MaxElements() const { return (std::numeric_limits<int64_t>::max)(); }
};

// LogMultilineUpToN(max_elements) formats the same as LogMultiline but
// prints no more than max_elements elements.
class LogMultilineUpToN : public internal::LogMultilineBase {
 public:
  explicit LogMultilineUpToN(int64_t max_elements)
      : max_elements_(max_elements) {}
  int64_t MaxElements() const { return max_elements_; }

 private:
  int64_t max_elements_;
};

// LogMultilineUpTo100 formats the same as LogMultiline but
// prints no more than 100 elements.
struct LogMultilineUpTo100 : public LogMultilineUpToN {
  LogMultilineUpTo100() : LogMultilineUpToN(100) {}
};

// The legacy behavior of LogSequence() does not use braces and
// separates items with spaces.  For example "1 2 3".
struct LogLegacyUpTo100 : public internal::LogLegacyBase {
  int64_t MaxElements() const { return 100; }
};
struct LogLegacy : public internal::LogLegacyBase {
  int64_t MaxElements() const { return (std::numeric_limits<int64_t>::max)(); }
};

// The default policy for new code.
typedef LogShortUpTo100 LogDefault;

// LogRangeToStream should be used to define operator<< for
// STL and STL-like containers.  For example, see stl_logging.h.
template <typename IteratorT, typename PolicyT>
inline void LogRangeToStream(std::ostream &out,  // NOLINT
                             IteratorT begin, IteratorT end,
                             const PolicyT &policy) {
  policy.LogOpening(out);
  for (int64_t i = 0; begin != end && i < policy.MaxElements(); ++i, ++begin) {
    if (i == 0) {
      policy.LogFirstSeparator(out);
    } else {
      policy.LogSeparator(out);
    }
    policy.Log(out, *begin);
  }
  if (begin != end) {
    policy.LogSeparator(out);
    policy.LogEllipsis(out);
  }
  policy.LogClosing(out);
}

namespace detail {

// RangeLogger is a helper class for LogRange and LogContainer; do not use it
// directly.  This object captures iterators into the argument of the LogRange
// and LogContainer functions, so its lifetime should be confined to a single
// logging statement.  Objects of this type should not be assigned to local
// variables.
template <typename IteratorT, typename PolicyT>
class RangeLogger {
 public:
  RangeLogger(const IteratorT &begin, const IteratorT &end,
              const PolicyT &policy)
      : begin_(begin), end_(end), policy_(policy) {}

  friend std::ostream &operator<<(std::ostream &out, const RangeLogger &range) {
    LogRangeToStream<IteratorT, PolicyT>(out, range.begin_, range.end_,
                                         range.policy_);
    return out;
  }

  // operator<< above is generally recommended. However, some situations may
  // require a string, so a convenience str() method is provided as well.
  std::string str() const {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

 private:
  IteratorT begin_;
  IteratorT end_;
  PolicyT policy_;
};

template <typename E>
class EnumLogger {
 public:
  explicit EnumLogger(E e) : e_(e) {}

  friend std::ostream &operator<<(std::ostream &out, const EnumLogger &v) {
    using I = typename std::underlying_type<E>::type;
    return out << static_cast<I>(v.e_);
  }

 private:
  E e_;
};

}  // namespace detail

// Log a range using "policy".  For example:
//
//   LOG(INFO) << LogRange(start_pos, end_pos, LogMultiline());
//
// The above example will print the range using newlines between
// elements, enclosed in [] braces.
template <typename IteratorT, typename PolicyT>
detail::RangeLogger<IteratorT, PolicyT> LogRange(const IteratorT &begin,
                                                 const IteratorT &end,
                                                 const PolicyT &policy) {
  return detail::RangeLogger<IteratorT, PolicyT>(begin, end, policy);
}

// Log a range.  For example:
//
//   LOG(INFO) << LogRange(start_pos, end_pos);
//
// By default, Range() uses the LogShortUpTo100 policy: comma-space
// separation, no newlines, and with limit of 100 items.
template <typename IteratorT>
detail::RangeLogger<IteratorT, LogDefault> LogRange(const IteratorT &begin,
                                                    const IteratorT &end) {
  return LogRange(begin, end, LogDefault());
}

// Log a container using "policy".  For example:
//
//   LOG(INFO) << LogContainer(container, LogMultiline());
//
// The above example will print the container using newlines between
// elements, enclosed in [] braces.
template <typename ContainerT, typename PolicyT>
auto LogContainer(const ContainerT& container, const PolicyT& policy)
    -> decltype(LogRange(container.begin(), container.end(), policy)) {
  return LogRange(container.begin(), container.end(), policy);
}

// Log a container.  For example:
//
//   LOG(INFO) << LogContainer(container);
//
// By default, Container() uses the LogShortUpTo100 policy: comma-space
// separation, no newlines, and with limit of 100 items.
template <typename ContainerT>
auto LogContainer(const ContainerT& container)
    -> decltype(LogContainer(container, LogDefault())) {
  return LogContainer(container, LogDefault());
}

// Log a (possibly scoped) enum.  For example:
//
//   enum class Color { kRed, kGreen, kBlue };
//   LOG(INFO) << LogEnum(kRed);
template <typename E>
detail::EnumLogger<E> LogEnum(E e) {
  static_assert(std::is_enum<E>::value, "must be an enum");
  return detail::EnumLogger<E>(e);
}

}  // namespace log_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_LOG_INTERNAL_CONTAINER_H_
