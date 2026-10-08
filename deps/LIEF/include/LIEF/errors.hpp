/* Copyright 2021 - 2026 R. Thomas
 * Copyright 2021 - 2026 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_ERROR_H
#define LIEF_ERROR_H
#include <cstdint>
#include <string>
#include <LIEF/compiler_attributes.hpp>
#include <LIEF/third-party/expected.hpp>

/// LIEF error codes definition
enum class lief_errors : uint32_t {
  read_error = 1,
  not_found,
  not_implemented,
  not_supported,

  corrupted,
  conversion_error,

  read_out_of_bound,
  asn1_bad_tag,
  file_error,

  file_format_error,
  parsing_error,
  build_error,

  data_too_large,
  require_extended_version,
  inconsistent,
  runtime_error,
  /*
   * When adding a new error, do not forget
   * to update the Python bindings as well (pyErr.cpp) and Rust bindings:
   * lief/src/error.rs
   */
};

const char* to_string(lief_errors err);

/// Create a standard error code from lief_errors
inline tl::unexpected<lief_errors> make_error_code(lief_errors e) {
  return tl::make_unexpected(e);
}


namespace LIEF {
/// Wrapper that contains an Object (``T``) or an error
///
/// The tl/expected implementation exposes the method ``value()`` to access the
/// underlying object (if no error)
///
/// Typical usage is:
///
/// @code{.cpp}
/// result<int> intval = my_function();
/// if (intval) {
///  int val = intval.value();
/// } else { // There is an error
///  std::cout << get_error(intval).message() << "\n";
/// }
/// @endcode
///
/// See https://tl.tartanllama.xyz/en/latest/api/expected.html for more details
template<typename T>
class LIEF_MAYBE_UNUSED result : public tl::expected<T, lief_errors> {
  public:
  using ExpectedType = T;
  using tl::expected<T, lief_errors>::expected;
  result(tl::expected<T, lief_errors> e) :
    tl::expected<T, lief_errors>::expected(std::move(e)) {}
};

/// Get the error code associated with the result
template<class T>
lief_errors get_error(result<T>& err) {
  return err.error();
}

/// Return the lief_errors when the provided ``result<T>`` is an error
template<class T>
lief_errors as_lief_err(result<T>& err) {
  return err.error();
}

/// Opaque structure used by ok_error_t
struct ok_t {};

/// Return success for function with return type ok_error_t.
inline ok_t ok() {
  return ok_t{};
}

/// Opaque structure that is used by LIEF to avoid
/// writing ``result<void> f(...)``. Instead, it makes the output
/// explicit such as:
///
/// @code{.cpp}
/// ok_error_t process() {
///   if (fail) {
///     return make_error_code(...);
///   }
///   return ok();
/// }
/// @endcode
class LIEF_MAYBE_UNUSED ok_error_t : public result<ok_t> {
  public:
  using result<ok_t>::result;
};

inline bool is_ok(const ok_error_t& val) {
  return val.has_value();
}

inline bool is_err(const ok_error_t& val) {
  return !is_ok(val);
}

}

extern template class tl::expected<LIEF::ok_t, lief_errors>;
extern template class LIEF::result<LIEF::ok_t>;
extern template class tl::expected<uint64_t, lief_errors>;
extern template class LIEF::result<uint64_t>;
extern template class tl::expected<uint32_t, lief_errors>;
extern template class LIEF::result<uint32_t>;
extern template class tl::expected<std::string, lief_errors>;
extern template class LIEF::result<std::string>;

#endif
