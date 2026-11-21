// Formatting library for C++ - dynamic argument lists
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_ARGS_H_
#define FMT_ARGS_H_

#ifndef FMT_MODULE
#  include <functional>  // std::reference_wrapper
#  include <memory>      // std::unique_ptr
#  include <vector>
#endif

#include "format.h"  // std_string_view

FMT_BEGIN_NAMESPACE
namespace detail {

template <typename T> struct is_reference_wrapper : std::false_type {};
template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T> auto unwrap(const T& v) -> const T& { return v; }
template <typename T>
auto unwrap(const std::reference_wrapper<T>& v) -> const T& {
  return static_cast<const T&>(v);
}

// node is defined outside dynamic_arg_list to workaround a C2504 bug in MSVC
// 2022 (v17.10.0).
//
// Workaround for clang's -Wweak-vtables. Unlike for regular classes, for
// templates it doesn't complain about inability to deduce single translation
// unit for placing vtable. So node is made a fake template.
template <typename = void> struct node {
  virtual ~node() = default;
  std::unique_ptr<node<>> next;
};

class dynamic_arg_list {
  template <typename T> struct typed_node : node<> {
    T value;

    template <typename Arg>
    FMT_CONSTEXPR typed_node(const Arg& arg) : value(arg) {}

    template <typename Char>
    FMT_CONSTEXPR typed_node(const basic_string_view<Char>& arg)
        : value(arg.data(), arg.size()) {}
  };

  std::unique_ptr<node<>> head_;

 public:
  template <typename T, typename Arg> auto push(const Arg& arg) -> const T& {
    auto new_node = std::unique_ptr<typed_node<T>>(new typed_node<T>(arg));
    auto& value = new_node->value;
    new_node->next = std::move(head_);
    head_ = std::move(new_node);
    return value;
  }
};
}  // namespace detail

/**
 * A dynamic list of formatting arguments with storage.
 *
 * It can be implicitly converted into `fmt::basic_format_args` for passing
 * into type-erased formatting functions such as `fmt::vformat`.
 */
FMT_EXPORT template <typename Context> class dynamic_format_arg_store {
 private:
  using char_type = typename Context::char_type;

  template <typename T> struct need_copy {
    static constexpr detail::type mapped_type =
        detail::mapped_type_constant<T, char_type>::value;

    enum {
      value = !(detail::is_reference_wrapper<T>::value ||
                std::is_same<T, basic_string_view<char_type>>::value ||
                std::is_same<T, detail::std_string_view<char_type>>::value ||
                (mapped_type != detail::type::cstring_type &&
                 mapped_type != detail::type::string_type &&
                 mapped_type != detail::type::custom_type))
    };
  };

  template <typename T>
  using stored_t = conditional_t<
      std::is_convertible<T, std::basic_string<char_type>>::value &&
          !detail::is_reference_wrapper<T>::value,
      std::basic_string<char_type>, T>;

  // Storage of basic_format_arg must be contiguous.
  std::vector<basic_format_arg<Context>> data_;
  std::vector<detail::named_arg_info<char_type>> named_info_;

  // Storage of arguments not fitting into basic_format_arg must grow
  // without relocation because items in data_ refer to it.
  detail::dynamic_arg_list dynamic_args_;

  friend class basic_format_args<Context>;

  auto data() const -> const basic_format_arg<Context>* {
    return named_info_.empty() ? data_.data() : data_.data() + 1;
  }

  template <typename T> void emplace_arg(const T& arg) {
    data_.emplace_back(arg);
  }

  template <typename T>
  void emplace_arg(const detail::named_arg<char_type, T>& arg) {
    if (named_info_.empty())
      data_.insert(data_.begin(), basic_format_arg<Context>(nullptr, 0));
    data_.emplace_back(detail::unwrap(arg.value));
    auto pop_one = [](std::vector<basic_format_arg<Context>>* data) {
      data->pop_back();
    };
    std::unique_ptr<std::vector<basic_format_arg<Context>>, decltype(pop_one)>
        guard{&data_, pop_one};
    named_info_.push_back({arg.name, static_cast<int>(data_.size() - 2u)});
    data_[0] = {named_info_.data(), named_info_.size()};
    guard.release();
  }

 public:
  constexpr dynamic_format_arg_store() = default;

  operator basic_format_args<Context>() const {
    return basic_format_args<Context>(data(), static_cast<int>(data_.size()),
                                      !named_info_.empty());
  }

  /**
   * Adds an argument into the dynamic store for later passing to a formatting
   * function.
   *
   * Note that custom types and string types (but not string views) are copied
   * into the store dynamically allocating memory if necessary.
   *
   * **Example**:
   *
   *     fmt::dynamic_format_arg_store<fmt::format_context> store;
   *     store.push_back(42);
   *     store.push_back("abc");
   *     store.push_back(1.5f);
   *     std::string result = fmt::vformat("{} and {} and {}", store);
   */
  template <typename T> void push_back(const T& arg) {
    if (detail::const_check(need_copy<T>::value))
      emplace_arg(dynamic_args_.push<stored_t<T>>(arg));
    else
      emplace_arg(detail::unwrap(arg));
  }

  /**
   * Adds a reference to the argument into the dynamic store for later passing
   * to a formatting function.
   *
   * **Example**:
   *
   *     fmt::dynamic_format_arg_store<fmt::format_context> store;
   *     char band[] = "Rolling Stones";
   *     store.push_back(std::cref(band));
   *     band[9] = 'c'; // Changing str affects the output.
   *     std::string result = fmt::vformat("{}", store);
   *     // result == "Rolling Scones"
   */
  template <typename T> void push_back(std::reference_wrapper<T> arg) {
    static_assert(
        need_copy<T>::value,
        "objects of built-in types and string views are always copied");
    emplace_arg(arg.get());
  }

  /**
   * Adds named argument into the dynamic store for later passing to a
   * formatting function. `std::reference_wrapper` is supported to avoid
   * copying of the argument. The name is always copied into the store.
   */
  template <typename T>
  void push_back(const detail::named_arg<char_type, T>& arg) {
    const char_type* arg_name =
        dynamic_args_.push<std::basic_string<char_type>>(arg.name).c_str();
    if (detail::const_check(need_copy<T>::value)) {
      emplace_arg(
          fmt::arg(arg_name, dynamic_args_.push<stored_t<T>>(arg.value)));
    } else {
      emplace_arg(fmt::arg(arg_name, arg.value));
    }
  }

  /// Erase all elements from the store.
  void clear() {
    data_.clear();
    named_info_.clear();
    dynamic_args_ = {};
  }

  /// Reserves space to store at least `new_cap` arguments including
  /// `new_cap_named` named arguments.
  void reserve(size_t new_cap, size_t new_cap_named) {
    FMT_ASSERT(new_cap >= new_cap_named,
               "set of arguments includes set of named arguments");
    data_.reserve(new_cap);
    named_info_.reserve(new_cap_named);
  }

  /// Returns the number of elements in the store.
  auto size() const noexcept -> size_t { return data_.size(); }
};

FMT_END_NAMESPACE

#endif  // FMT_ARGS_H_
