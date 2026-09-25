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

#ifndef ABSL_STRINGS_INTERNAL_GENERIC_PRINTER_INTERNAL_H_
#define ABSL_STRINGS_INTERNAL_GENERIC_PRINTER_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/config.h"
#include "absl/log/internal/container.h"
#include "absl/meta/internal/requires.h"
#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

// NOTE: we do not want to expand the dependencies of this library. All
// compatibility detection must be done in a generic way, without having to
// include the headers of other libraries.

// Internal implementation details: see generic_printer.h.
namespace absl {
ABSL_NAMESPACE_BEGIN
namespace internal_generic_printer {

template <typename T>
std::ostream& GenericPrintImpl(std::ostream& os, const T& v);

// Scope blocker: we must always use ADL in our predicates below.
struct Anonymous;
std::ostream& operator<<(const Anonymous&, const Anonymous&) = delete;

// Logging policy for LogContainer. Our SFINAE overload will not fail
// if the contained type cannot be printed, so make sure to circle back to
// GenericPrinter.
struct ContainerLogPolicy {
  void LogOpening(std::ostream& os) const { os << "["; }
  void LogClosing(std::ostream& os) const { os << "]"; }
  void LogFirstSeparator(std::ostream& os) const { os << ""; }
  void LogSeparator(std::ostream& os) const { os << ", "; }
  int64_t MaxElements() const { return 15; }
  template <typename T>
  void Log(std::ostream& os, const T& element) const {
    internal_generic_printer::GenericPrintImpl(os, element);
  }
  void LogEllipsis(std::ostream& os) const { os << "..."; }
};

// Out-of-line helper for PrintAsStringWithEscaping.
std::ostream& PrintEscapedString(std::ostream& os, absl::string_view v);

// Trait to recognize a string, possibly with a custom allocator.
template <class T>
inline constexpr bool is_any_string = false;
template <class A>
inline constexpr bool
    is_any_string<std::basic_string<char, std::char_traits<char>, A>> = true;

// Trait to recognize a supported pointer. Below are documented reasons why
// raw pointers and std::shared_ptr are not currently supported.
// See also http://b/239459272#comment9 and the discussion in the comment
// threads of cl/485200996.
//
// The tl;dr:
// 1. Pointers that logically represent an object (or nullptr) are safe to print
//    out.
// 2. Pointers that may represent some other concept (delineating memory bounds,
//    overridden managed-memory deleters to mimic RAII, ...) may be unsafe to
//    print out.
//
// raw pointers:
//  - A pointer one-past the last element of an array
//  - Non-null but non-dereferenceable: https://gcc.godbolt.org/z/sqsqGvKbP
//
// `std::shared_ptr`:
//  - "Aliasing" / similar to raw pointers: https://gcc.godbolt.org/z/YbWPzvhae
template <class T, class = void>
inline constexpr bool is_supported_ptr = false;
// `std::unique_ptr` has the same theoretical risks as raw pointers, but those
// risks are far less likely (typically requiring a custom deleter), and the
// benefits of supporting unique_ptr outweigh the costs.
//  - Note: `std::unique_ptr<T[]>` cannot safely be and is not dereferenced.
template <class A, class... Deleter>
inline constexpr bool is_supported_ptr<std::unique_ptr<A, Deleter...>> = true;
// `ArenaSafeUniquePtr` is at least as safe as `std::unique_ptr`.
template <class T>
inline constexpr bool is_supported_ptr<
    T,
    // Check for `ArenaSafeUniquePtr` without having to include its header here.
    // This does match any type named `ArenaSafeUniquePtr`, regardless of the
    // namespace it is defined in, but it's pretty plausible that any such type
    // would be a fork.
    decltype(T().~ArenaSafeUniquePtr())> = true;

// Specialization for floats: print floating point types using their
// max_digits10 precision. This ensures each distinct underlying values
// can be represented uniquely, even though it's not (strictly speaking)
// the most precise representation.
std::ostream& PrintPreciseFP(std::ostream& os, float v);
std::ostream& PrintPreciseFP(std::ostream& os, double v);
std::ostream& PrintPreciseFP(std::ostream& os, long double v);

std::ostream& PrintChar(std::ostream& os, char c);
std::ostream& PrintChar(std::ostream& os, signed char c);
std::ostream& PrintChar(std::ostream& os, unsigned char c);

std::ostream& PrintByte(std::ostream& os, std::byte b);

template <class... Ts>
std::ostream& PrintTuple(std::ostream& os, const std::tuple<Ts...>& tuple) {
  absl::string_view sep = "";
  const auto print_one = [&](const auto& v) {
    os << sep;
    (GenericPrintImpl)(os, v);
    sep = ", ";
  };
  os << "<";
  std::apply([&](const auto&... v) { (print_one(v), ...); }, tuple);
  os << ">";
  return os;
}

template <typename T, typename U>
std::ostream& PrintPair(std::ostream& os, const std::pair<T, U>& p) {
  os << "<";
  (GenericPrintImpl)(os, p.first);
  os << ", ";
  (GenericPrintImpl)(os, p.second);
  os << ">";
  return os;
}

template <typename T>
std::ostream& PrintOptionalLike(std::ostream& os, const T& v) {
  if (v.has_value()) {
    os << "<";
    (GenericPrintImpl)(os, *v);
    os << ">";
  } else {
    (GenericPrintImpl)(os, std::nullopt);
  }
  return os;
}

template <typename... Ts>
std::ostream& PrintVariant(std::ostream& os, const std::variant<Ts...>& v) {
  os << "(";
  os << "'(index = " << v.index() << ")' ";

  // NOTE(derekbailey): This may throw a std::bad_variant_access if the variant
  // is "valueless", which only occurs if exceptions are thrown. This is
  // non-relevant when not using exceptions, but it is worth mentioning if that
  // invariant is ever changed.
  std::visit([&](const auto& arg) { (GenericPrintImpl)(os, arg); }, v);
  os << ")";
  return os;
}

template <typename StatusOrLike>
std::ostream& PrintStatusOrLike(std::ostream& os, const StatusOrLike& v) {
  os << "<";
  if (v.ok()) {
    os << "OK: ";
    (GenericPrintImpl)(os, *v);
  } else {
    (GenericPrintImpl)(os, v.status());
  }
  os << ">";
  return os;
}

template <typename SmartPointer>
std::ostream& PrintSmartPointerContents(std::ostream& os,
                                        const SmartPointer& v) {
  os << "<";
  if (v == nullptr) {
    (GenericPrintImpl)(os, nullptr);
  } else {
    // Cast to void* so that every type (e.g. `char*`) is printed as an address.
    os << absl::implicit_cast<const void*>(v.get()) << " pointing to ";

    if constexpr (meta_internal::Requires<SmartPointer>(
                      [](auto&& p) -> decltype(p[0]) {})) {
      // e.g. std::unique_ptr<int[]>, which only has operator[]
      os << "an array";
    } else if constexpr (std::is_object_v<
                             typename SmartPointer::element_type>) {
      (GenericPrintImpl)(os, *v);
    } else {
      // e.g. std::unique_ptr<void, MyCustomDeleter>
      os << "a non-object type";
    }
  }
  os << ">";
  return os;
}

template <typename T>
std::ostream& GenericPrintImpl(std::ostream& os, const T& v) {
  if constexpr (is_any_string<T> || std::is_same_v<T, absl::string_view>) {
    // Specialization for strings: prints with plausible quoting and escaping.
    return PrintEscapedString(os, v);
  } else if constexpr (absl::HasAbslStringify<T>::value) {
    // If someone has specified `AbslStringify`, we should prefer that.
    return os << absl::StrCat(v);
  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w)
                               -> decltype((
                                   PrintTuple)(std::declval<std::ostream&>(),
                                               w)) {})) {
    // For tuples, use `< elem0, ..., elemN >`.
    return (PrintTuple)(os, v);

  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w)
                               -> decltype((
                                   PrintPair)(std::declval<std::ostream&>(),
                                              w)) {})) {
    // For pairs, use `< first, second >`.
    return (PrintPair)(os, v);

  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w)
                               -> decltype((
                                   PrintVariant)(std::declval<std::ostream&>(),
                                                 w)) {})) {
    // For std::variant, use `std::visit(v)`
    return (PrintVariant)(os, v);
  } else if constexpr (is_supported_ptr<T>) {
    return (PrintSmartPointerContents)(os, v);
  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w) -> decltype(w.ok(), w.status(), *w) {
                           })) {
    return (PrintStatusOrLike)(os, v);
  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w) -> decltype(w.has_value(), *w) {})) {
    return (PrintOptionalLike)(os, v);
  } else if constexpr (std::is_same_v<T, std::nullopt_t>) {
    // Specialization for nullopt.
    return os << "nullopt";

  } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
    // Specialization for nullptr.
    return os << "nullptr";

  } else if constexpr (std::is_floating_point_v<T>) {
    // For floating point print with enough precision for a roundtrip.
    return PrintPreciseFP(os, v);

  } else if constexpr (std::is_same_v<T, char> ||
                       std::is_same_v<T, signed char> ||
                       std::is_same_v<T, unsigned char>) {
    // Chars are printed as the char (if a printable char) and the integral
    // representation in hex and decimal.
    return PrintChar(os, v);

  } else if constexpr (std::is_same_v<T, std::byte>) {
    return PrintByte(os, v);

  } else if constexpr (std::is_same_v<T, bool> ||
                       std::is_same_v<T,
                                      typename std::vector<bool>::reference> ||
                       std::is_same_v<
                           T, typename std::vector<bool>::const_reference>) {
    return os << (v ? "true" : "false");

  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w)
                               -> decltype(ProtobufInternalGetEnumDescriptor(
                                   w)) {})) {
    os << static_cast<std::underlying_type_t<T>>(v);
    if (auto* desc =
            ProtobufInternalGetEnumDescriptor(T{})->FindValueByNumber(v)) {
      os << "(" << desc->name() << ")";
    }
    return os;
  } else if constexpr (!std::is_enum_v<T> &&
                       meta_internal::Requires<const T>(
                           [&](auto&& w) -> decltype(absl::StrCat(w)) {})) {
    return os << absl::StrCat(v);

  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w)
                               -> decltype(std::declval<std::ostream&>()
                                           << log_internal::LogContainer(w)) {
                           })) {
    // For containers, use `[ elem0, ..., elemN ]` with a max of 15.
    return os << log_internal::LogContainer(v, ContainerLogPolicy());

  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w)
                               -> decltype(std::declval<std::ostream&>() << w) {
                           })) {
    // Streaming
    return os << v;

  } else if constexpr (meta_internal::Requires<const T>(
                           [&](auto&& w)
                               -> decltype(std::declval<std::ostream&>()
                                           << w.DebugString()) {})) {
    // DebugString
    return os << v.DebugString();

  } else if constexpr (std::is_enum_v<T>) {
    // In case the underlying type is some kind of char, we have to recurse.
    return GenericPrintImpl(os, static_cast<std::underlying_type_t<T>>(v));

  } else {
    // Default: we don't have anything to stream the value.
    return os << "[unprintable value of size " << sizeof(T) << " @" << &v
              << "]";
  }
}

// GenericPrinter always has a valid operator<<. It defers to the disjuction
// above.
template <typename T>
class GenericPrinter {
 public:
  explicit GenericPrinter(const T& value) : value_(value) {}

 private:
  friend std::ostream& operator<<(std::ostream& os, const GenericPrinter& gp) {
    return internal_generic_printer::GenericPrintImpl(os, gp.value_);
  }
  const T& value_;
};

struct GenericPrintStreamAdapter {
  template <class StreamT>
  struct Impl {
    // Stream operator: this object will adapt to the underlying stream type,
    // but only if the Impl is an rvalue. For example, this works:
    //   std::cout << GenericPrint() << foo;
    // but not:
    //   auto adapter = (std::cout << GenericPrint());
    //   adapter << foo;
    template <typename T>
    Impl&& operator<<(const T& value) && {
      os << internal_generic_printer::GenericPrinter<T>(value);
      return std::move(*this);
    }

    // Inhibit using a stack variable for the adapter:
    template <typename T>
    Impl& operator<<(const T& value) & = delete;

    // Detects a Flush() method, for LogMessage compatibility.
    template <typename T>
    class HasFlushMethod {
     private:
      template <typename C>
      static std::true_type Test(decltype(&C::Flush));
      template <typename C>
      static std::false_type Test(...);

     public:
      static constexpr bool value = decltype(Test<T>(nullptr))::value;
    };

    // LogMessage compatibility requires a Flush() method.
    void Flush() {
      if constexpr (HasFlushMethod<StreamT>::value) {
        os.Flush();
      }
    }

    StreamT& os;
  };

  // If Impl is evaluated on the RHS of an 'operator&&', and 'lhs && Impl.os'
  // implicitly converts to void, then it's fine for Impl to do so, too. This
  // will create precisely as many objects as 'lhs && Impl.os', so we should
  // both observe any side effects, and avoid observing multiple side
  // effects. (See absl::log_internal::Voidify for an example of why this might
  // be useful.)
  template <typename LHS, typename RHS>
  friend auto operator&&(LHS&& lhs, Impl<RHS>&& rhs)
      -> decltype(lhs && rhs.os) {
    return lhs && rhs.os;
  }

  template <class StreamT>
  friend Impl<StreamT> operator<<(StreamT& os, GenericPrintStreamAdapter&&) {
    return Impl<StreamT>{os};
  }
};

struct GenericPrintAdapterFactory {
  internal_generic_printer::GenericPrintStreamAdapter operator()() const {
    return internal_generic_printer::GenericPrintStreamAdapter{};
  }
  template <typename T>
  internal_generic_printer::GenericPrinter<T> operator()(const T& value) const {
    return internal_generic_printer::GenericPrinter<T>{value};
  }
};

}  // namespace internal_generic_printer
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STRINGS_INTERNAL_GENERIC_PRINTER_INTERNAL_H_
