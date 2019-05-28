// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_UTILS_H_
#define V8_TORQUE_UTILS_H_

#include <ostream>
#include <streambuf>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/base/functional.h"
#include "src/base/optional.h"
#include "src/torque/contextual.h"
#include "src/torque/source-positions.h"

namespace v8 {
namespace internal {
namespace torque {

std::string StringLiteralUnquote(const std::string& s);
std::string StringLiteralQuote(const std::string& s);

// Decodes "file://" URIs into file paths which can then be used
// with the standard stream API.
V8_EXPORT_PRIVATE base::Optional<std::string> FileUriDecode(
    const std::string& s);

class LintErrorStatus : public ContextualClass<LintErrorStatus> {
 public:
  LintErrorStatus() : has_lint_errors_(false) {}

  static bool HasLintErrors() { return Get().has_lint_errors_; }
  static void SetLintError() { Get().has_lint_errors_ = true; }

 private:
  bool has_lint_errors_;
};

void LintError(const std::string& error);

// Prints a LintError with the format "{type} '{name}' doesn't follow
// '{convention}' naming convention".
void NamingConventionError(const std::string& type, const std::string& name,
                           const std::string& convention);

bool IsLowerCamelCase(const std::string& s);
bool IsUpperCamelCase(const std::string& s);
bool IsSnakeCase(const std::string& s);
bool IsValidNamespaceConstName(const std::string& s);
bool IsValidTypeName(const std::string& s);

struct TorqueError : public std::exception {
  explicit TorqueError(const std::string& message) : message(message) {}

  std::string message;
  base::Optional<SourcePosition> position;
};

[[noreturn]] void ThrowTorqueError(const std::string& error,
                                   bool include_position);
template <class... Args>
[[noreturn]] void ReportError(Args&&... args) {
  std::stringstream s;
  USE((s << std::forward<Args>(args))...);
  ThrowTorqueError(s.str(), true);
}
template <class... Args>
[[noreturn]] void ReportErrorWithoutPosition(Args&&... args) {
  std::stringstream s;
  USE((s << std::forward<Args>(args))...);
  ThrowTorqueError(s.str(), false);
}

std::string CapifyStringWithUnderscores(const std::string& camellified_string);
std::string CamelifyString(const std::string& underscore_string);
std::string DashifyString(const std::string& underscore_string);

void ReplaceFileContentsIfDifferent(const std::string& file_path,
                                    const std::string& contents);

std::string CurrentPositionAsString();

template <class T>
class Deduplicator {
 public:
  const T* Add(T x) { return &*(storage_.insert(std::move(x)).first); }

 private:
  std::unordered_set<T, base::hash<T>> storage_;
};

template <class C, class T>
void PrintCommaSeparatedList(std::ostream& os, const T& list, C transform) {
  bool first = true;
  for (auto& e : list) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << transform(e);
  }
}

template <class T,
          typename std::enable_if<
              std::is_pointer<typename T::value_type>::value, int>::type = 0>
void PrintCommaSeparatedList(std::ostream& os, const T& list) {
  bool first = true;
  for (auto& e : list) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << *e;
  }
}

template <class T,
          typename std::enable_if<
              !std::is_pointer<typename T::value_type>::value, int>::type = 0>
void PrintCommaSeparatedList(std::ostream& os, const T& list) {
  bool first = true;
  for (auto& e : list) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << e;
  }
}

struct BottomOffset {
  size_t offset;
  BottomOffset& operator++() {
    ++offset;
    return *this;
  }
  BottomOffset operator+(size_t x) const { return BottomOffset{offset + x}; }
  BottomOffset operator-(size_t x) const {
    DCHECK_LE(x, offset);
    return BottomOffset{offset - x};
  }
  bool operator<(const BottomOffset& other) const {
    return offset < other.offset;
  }
  bool operator<=(const BottomOffset& other) const {
    return offset <= other.offset;
  }
  bool operator==(const BottomOffset& other) const {
    return offset == other.offset;
  }
  bool operator!=(const BottomOffset& other) const {
    return offset != other.offset;
  }
};

inline std::ostream& operator<<(std::ostream& out, BottomOffset from_bottom) {
  return out << "BottomOffset{" << from_bottom.offset << "}";
}

// An iterator-style range of stack slots.
class StackRange {
 public:
  StackRange(BottomOffset begin, BottomOffset end) : begin_(begin), end_(end) {
    DCHECK_LE(begin_, end_);
  }

  bool operator==(const StackRange& other) const {
    return begin_ == other.begin_ && end_ == other.end_;
  }

  void Extend(StackRange adjacent) {
    DCHECK_EQ(end_, adjacent.begin_);
    end_ = adjacent.end_;
  }

  size_t Size() const { return end_.offset - begin_.offset; }
  BottomOffset begin() const { return begin_; }
  BottomOffset end() const { return end_; }

 private:
  BottomOffset begin_;
  BottomOffset end_;
};

inline std::ostream& operator<<(std::ostream& out, StackRange range) {
  return out << "StackRange{" << range.begin() << ", " << range.end() << "}";
}

template <class T>
class Stack {
 public:
  using value_type = T;
  Stack() = default;
  Stack(std::initializer_list<T> initializer)
      : Stack(std::vector<T>(initializer)) {}
  explicit Stack(std::vector<T> v) : elements_(std::move(v)) {}
  size_t Size() const { return elements_.size(); }
  const T& Peek(BottomOffset from_bottom) const {
    return elements_.at(from_bottom.offset);
  }
  void Poke(BottomOffset from_bottom, T x) {
    elements_.at(from_bottom.offset) = std::move(x);
  }
  void Push(T x) { elements_.push_back(std::move(x)); }
  StackRange TopRange(size_t slot_count) const {
    DCHECK_GE(Size(), slot_count);
    return StackRange{AboveTop() - slot_count, AboveTop()};
  }
  StackRange PushMany(const std::vector<T>& v) {
    for (const T& x : v) {
      Push(x);
    }
    return TopRange(v.size());
  }
  const T& Top() const { return Peek(AboveTop() - 1); }
  T Pop() {
    T result = std::move(elements_.back());
    elements_.pop_back();
    return result;
  }
  std::vector<T> PopMany(size_t count) {
    DCHECK_GE(elements_.size(), count);
    std::vector<T> result;
    result.reserve(count);
    for (auto it = elements_.end() - count; it != elements_.end(); ++it) {
      result.push_back(std::move(*it));
    }
    elements_.resize(elements_.size() - count);
    return result;
  }
  // The invalid offset above the top element. This is useful for StackRange.
  BottomOffset AboveTop() const { return BottomOffset{Size()}; }
  // Delete the slots in {range}, moving higher slots to fill the gap.
  void DeleteRange(StackRange range) {
    DCHECK_LE(range.end(), AboveTop());
    if (range.Size() == 0) return;
    for (BottomOffset i = range.end(); i < AboveTop(); ++i) {
      elements_[i.offset - range.Size()] = std::move(elements_[i.offset]);
    }
    elements_.resize(elements_.size() - range.Size());
  }

  bool operator==(const Stack& other) const {
    return elements_ == other.elements_;
  }
  bool operator!=(const Stack& other) const {
    return elements_ != other.elements_;
  }

  T* begin() { return elements_.data(); }
  T* end() { return begin() + elements_.size(); }
  const T* begin() const { return elements_.data(); }
  const T* end() const { return begin() + elements_.size(); }

 private:
  std::vector<T> elements_;
};

template <class T>
T* CheckNotNull(T* x) {
  CHECK_NOT_NULL(x);
  return x;
}

template <class T>
inline std::ostream& operator<<(std::ostream& os, Stack<T>& t) {
  os << "Stack{";
  PrintCommaSeparatedList(os, t);
  os << "}";
  return os;
}
class ToString {
 public:
  template <class T>
  ToString& operator<<(T&& x) {
    s_ << std::forward<T>(x);
    return *this;
  }
  operator std::string() { return s_.str(); }

 private:
  std::stringstream s_;
};

static const char* const kBaseNamespaceName = "base";
static const char* const kTestNamespaceName = "test";

// Erase elements of a container that has a constant-time erase function, like
// std::set or std::list. Calling this on std::vector would have quadratic
// complexity.
template <class Container, class F>
void EraseIf(Container* container, F f) {
  for (auto it = container->begin(); it != container->end();) {
    if (f(*it)) {
      it = container->erase(it);
    } else {
      ++it;
    }
  }
}

class NullStreambuf : public std::streambuf {
 public:
  virtual int overflow(int c) {
    setp(buffer_, buffer_ + sizeof(buffer_));
    return (c == traits_type::eof()) ? '\0' : c;
  }

 private:
  char buffer_[64];
};

class NullOStream : public std::ostream {
 public:
  NullOStream() : std::ostream(&buffer_) {}

 private:
  NullStreambuf buffer_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_UTILS_H_
