/* Copyright 2021 - 2025 R. Thomas
 * Copyright 2021 - 2025 Quarkslab
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
#include <LIEF/third-party/expected.hpp>
#include <cstdint>

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
  require_extended_version
  /*
   * When adding a new error, do not forget
   * to update the Python bindings as well (pyErr.cpp) and Rust bindings:
   * lief/src/error.rs
   */
};

const char* to_string(lief_errors err);

/// Create an standard error code from lief_errors
inline tl::unexpected<lief_errors> make_error_code(lief_errors e) {
  return tl::make_unexpected(e);
}


namespace LIEF {
/// Wrapper that contains an Object (``T``) or an error
///
/// The tl/expected implementation exposes the method ``value()`` to access the underlying object (if no error)
///
/// Typical usage is:
///
/// \code{.cpp}
/// result<int> intval = my_function();
/// if (intval) {
///  int val = intval.value();
/// } else { // There is an error
///  std::cout << get_error(intval).message() << "\n";
/// }
/// \endcode
///
/// See https://tl.tartanllama.xyz/en/latest/api/expected.html for more details
template<typename T>
 class [[maybe_unused]]result : public tl::expected<T, lief_errors> {
  public:
  using tl::expected<T, lief_errors>::expected;
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
/// \code{.cpp}
/// ok_error_t process() {
///   if (fail) {
///     return make_error_code(...);
///   }
///   return ok();
/// }
/// \endcode
class [[maybe_unused]] ok_error_t : public result<ok_t> {
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





#endif
