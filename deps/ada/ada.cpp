/* auto-generated on 2023-02-26 15:07:41 -0500. Do not edit! */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=ada.cpp
/* begin file src/ada.cpp */
#include "ada.h"
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=checkers.cpp
/* begin file src/checkers.cpp */
#include <algorithm>

namespace ada::checkers {

  ada_really_inline ada_constexpr bool is_ipv4(std::string_view view) noexcept {
    size_t last_dot = view.rfind('.');
    if(last_dot == view.size() - 1) {
      view.remove_suffix(1);
      last_dot = view.rfind('.');
    }
    std::string_view number = (last_dot == std::string_view::npos) ? view : view.substr(last_dot+1);
    if(number.empty()) { return false; }
    /** Optimization opportunity: we have basically identified the last number of the
        ipv4 if we return true here. We might as well parse it and have at least one
        number parsed when we get to parse_ipv4. */
    if(std::all_of(number.begin(), number.end(), ada::checkers::is_digit)) { return true; }
    return (checkers::has_hex_prefix(number) && std::all_of(number.begin()+2, number.end(), ada::unicode::is_lowercase_hex));
  }


  // for use with path_signature, we include all characters that need percent encoding.
  static constexpr uint8_t path_signature_table[256] = {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  static_assert(path_signature_table[uint8_t('?')] == 1);
  static_assert(path_signature_table[uint8_t('`')] == 1);
  static_assert(path_signature_table[uint8_t('{')] == 1);
  static_assert(path_signature_table[uint8_t('}')] == 1);
  //
  static_assert(path_signature_table[uint8_t(' ')] == 1);
  static_assert(path_signature_table[uint8_t('?')] == 1);
  static_assert(path_signature_table[uint8_t('"')] == 1);
  static_assert(path_signature_table[uint8_t('#')] == 1);
  static_assert(path_signature_table[uint8_t('<')] == 1);
  static_assert(path_signature_table[uint8_t('>')] == 1);
  //
  static_assert(path_signature_table[0] == 1);
  static_assert(path_signature_table[31] == 1);
  static_assert(path_signature_table[127] == 1);
  static_assert(path_signature_table[128] == 1);
  static_assert(path_signature_table[255] == 1);

  ada_really_inline constexpr uint8_t path_signature(std::string_view input) noexcept {
    // The path percent-encode set is the query percent-encode set and U+003F (?), U+0060 (`), U+007B ({), and U+007D (}).
    // The query percent-encode set is the C0 control percent-encode set and U+0020 SPACE, U+0022 ("), U+0023 (#), U+003C (<), and U+003E (>).
    // The C0 control percent-encode set are the C0 controls and all code points greater than U+007E (~).
    size_t i = 0;
    uint8_t accumulator{};
    for (; i + 7 < input.size(); i += 8) {
      accumulator |= uint8_t(path_signature_table[uint8_t(input[i])] |
                    path_signature_table[uint8_t(input[i + 1])] |
                    path_signature_table[uint8_t(input[i + 2])] |
                    path_signature_table[uint8_t(input[i + 3])] |
                    path_signature_table[uint8_t(input[i + 4])] |
                    path_signature_table[uint8_t(input[i + 5])] |
                    path_signature_table[uint8_t(input[i + 6])] |
                    path_signature_table[uint8_t(input[i + 7])]);
    }
    for (; i < input.size(); i++) {
      accumulator |= uint8_t(path_signature_table[uint8_t(input[i])]);
    }
    return accumulator;
  }


  ada_really_inline constexpr bool verify_dns_length(std::string_view input) noexcept {
    if(input.back() == '.') {
      if(input.size() > 254) return false;
    } else if (input.size() > 253) return false;

    size_t start = 0;
    while (start < input.size()) {
      auto dot_location = input.find('.', start);
      // If not found, it's likely the end of the domain
      if(dot_location == std::string_view::npos) dot_location = input.size();

      auto label_size = dot_location - start;
      if (label_size > 63 || label_size == 0) return false;

      start = dot_location + 1;
    }

    return true;
  }
} // namespace ada::checkers
/* end file src/checkers.cpp */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=unicode.cpp
/* begin file src/unicode.cpp */

#include <algorithm>
#if ADA_HAS_ICU
// We are good.
#else

#if defined(_WIN32) && ADA_WINDOWS_TO_ASCII_FALLBACK

#ifndef __wtypes_h__
#include <wtypes.h>
#endif // __wtypes_h__

#ifndef __WINDEF_
#include <windef.h>
#endif // __WINDEF_

#include <winnls.h>
#endif //defined(_WIN32) && ADA_WINDOWS_TO_ASCII_FALLBACK

#endif // ADA_HAS_ICU

namespace ada::unicode {

  constexpr bool to_lower_ascii(char * input, size_t length) noexcept {
    auto broadcast = [](uint8_t v) -> uint64_t { return 0x101010101010101 * v; };
    uint64_t broadcast_80 = broadcast(0x80);
    uint64_t broadcast_Ap = broadcast(128 - 'A');
    uint64_t broadcast_Zp = broadcast(128 - 'Z');
    uint64_t non_ascii = 0;
    size_t i = 0;

    for (; i + 7 < length; i += 8) {
      uint64_t word{};
      memcpy(&word, input + i, sizeof(word));
      non_ascii |= (word & broadcast_80);
      word ^= (((word+broadcast_Ap)^(word+broadcast_Zp))&broadcast_80)>>2;
      memcpy(input + i, &word, sizeof(word));
    }
    if (i < length) {
      uint64_t word{};
      memcpy(&word, input + i, length - i);
      non_ascii |= (word & broadcast_80);
      word ^= (((word+broadcast_Ap)^(word+broadcast_Zp))&broadcast_80)>>2;
      memcpy(input + i, &word, length - i);
    }
    return non_ascii == 0;
  }

  ada_really_inline constexpr bool has_tabs_or_newline(std::string_view user_input) noexcept {
    auto has_zero_byte = [](uint64_t v) {
      return ((v - 0x0101010101010101) & ~(v)&0x8080808080808080);
    };
    auto broadcast = [](uint8_t v) -> uint64_t { return 0x101010101010101 * v; };
    size_t i = 0;
    uint64_t mask1 = broadcast('\r');
    uint64_t mask2 = broadcast('\n');
    uint64_t mask3 = broadcast('\t');
    uint64_t running{0};
    for (; i + 7 < user_input.size(); i += 8) {
      uint64_t word{};
      memcpy(&word, user_input.data() + i, sizeof(word));
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor3 = word ^ mask3;
      running |= has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor3);
    }
    if (i < user_input.size()) {
      uint64_t word{};
      memcpy(&word, user_input.data() + i, user_input.size() - i);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor3 = word ^ mask3;
      running |= has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor3);
    }
    return running;
  }

  // A forbidden host code point is U+0000 NULL, U+0009 TAB, U+000A LF, U+000D CR, U+0020 SPACE, U+0023 (#),
  // U+002F (/), U+003A (:), U+003C (<), U+003E (>), U+003F (?), U+0040 (@), U+005B ([), U+005C (\), U+005D (]),
  // U+005E (^), or U+007C (|).
  constexpr static bool is_forbidden_host_code_point_table[] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    static_assert(sizeof(is_forbidden_host_code_point_table) == 256);

  ada_really_inline constexpr bool is_forbidden_host_code_point(const char c) noexcept {
    return is_forbidden_host_code_point_table[uint8_t(c)];
  }

  static_assert(unicode::is_forbidden_host_code_point('\0'));
  static_assert(unicode::is_forbidden_host_code_point('\t'));
  static_assert(unicode::is_forbidden_host_code_point('\n'));
  static_assert(unicode::is_forbidden_host_code_point('\r'));
  static_assert(unicode::is_forbidden_host_code_point(' '));
  static_assert(unicode::is_forbidden_host_code_point('#'));
  static_assert(unicode::is_forbidden_host_code_point('/'));
  static_assert(unicode::is_forbidden_host_code_point(':'));
  static_assert(unicode::is_forbidden_host_code_point('?'));
  static_assert(unicode::is_forbidden_host_code_point('@'));
  static_assert(unicode::is_forbidden_host_code_point('['));
  static_assert(unicode::is_forbidden_host_code_point('?'));
  static_assert(unicode::is_forbidden_host_code_point('<'));
  static_assert(unicode::is_forbidden_host_code_point('>'));
  static_assert(unicode::is_forbidden_host_code_point('\\'));
  static_assert(unicode::is_forbidden_host_code_point(']'));
  static_assert(unicode::is_forbidden_host_code_point('^'));
  static_assert(unicode::is_forbidden_host_code_point('|'));

constexpr static uint8_t is_forbidden_domain_code_point_table[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    static_assert(sizeof(is_forbidden_domain_code_point_table) == 256);

  ada_really_inline constexpr bool is_forbidden_domain_code_point(const char c) noexcept {
    return is_forbidden_domain_code_point_table[uint8_t(c)];
  }

  ada_really_inline constexpr bool contains_forbidden_domain_code_point(char * input, size_t length) noexcept {
    size_t i = 0;
    uint8_t accumulator{};
    for(; i + 4 <= length; i+=4) {
      accumulator |= is_forbidden_domain_code_point_table[uint8_t(input[i])];
      accumulator |= is_forbidden_domain_code_point_table[uint8_t(input[i+1])];
      accumulator |= is_forbidden_domain_code_point_table[uint8_t(input[i+2])];
      accumulator |= is_forbidden_domain_code_point_table[uint8_t(input[i+3])];
    }
    for(; i < length; i++) {
      accumulator |= is_forbidden_domain_code_point_table[uint8_t(input[i])];
    }
    return accumulator;
  }

  static_assert(unicode::is_forbidden_domain_code_point('%'));
  static_assert(unicode::is_forbidden_domain_code_point('\x7f'));
  static_assert(unicode::is_forbidden_domain_code_point('\0'));
  static_assert(unicode::is_forbidden_domain_code_point('\t'));
  static_assert(unicode::is_forbidden_domain_code_point('\n'));
  static_assert(unicode::is_forbidden_domain_code_point('\r'));
  static_assert(unicode::is_forbidden_domain_code_point(' '));
  static_assert(unicode::is_forbidden_domain_code_point('#'));
  static_assert(unicode::is_forbidden_domain_code_point('/'));
  static_assert(unicode::is_forbidden_domain_code_point(':'));
  static_assert(unicode::is_forbidden_domain_code_point('?'));
  static_assert(unicode::is_forbidden_domain_code_point('@'));
  static_assert(unicode::is_forbidden_domain_code_point('['));
  static_assert(unicode::is_forbidden_domain_code_point('?'));
  static_assert(unicode::is_forbidden_domain_code_point('<'));
  static_assert(unicode::is_forbidden_domain_code_point('>'));
  static_assert(unicode::is_forbidden_domain_code_point('\\'));
  static_assert(unicode::is_forbidden_domain_code_point(']'));
  static_assert(unicode::is_forbidden_domain_code_point('^'));
  static_assert(unicode::is_forbidden_domain_code_point('|'));

  constexpr static bool is_alnum_plus_table[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  static_assert(sizeof(is_alnum_plus_table) == 256);

  ada_really_inline constexpr bool is_alnum_plus(const char c) noexcept {
    return is_alnum_plus_table[uint8_t(c)];
    // A table is almost surely much faster than the
    // following under most compilers: return
    // return (std::isalnum(c) || c == '+' || c == '-' || c == '.');
  }
  static_assert(unicode::is_alnum_plus('+'));
  static_assert(unicode::is_alnum_plus('-'));
  static_assert(unicode::is_alnum_plus('.'));
  static_assert(unicode::is_alnum_plus('0'));
  static_assert(unicode::is_alnum_plus('1'));
  static_assert(unicode::is_alnum_plus('a'));
  static_assert(unicode::is_alnum_plus('b'));

  ada_really_inline constexpr bool is_ascii_hex_digit(const char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c<= 'f');
  }

  ada_really_inline constexpr bool is_c0_control_or_space(const char c) noexcept {
    return (unsigned char) c <= ' ';
  }

  ada_really_inline constexpr bool is_ascii_tab_or_newline(const char c) noexcept {
    return c == '\t' || c == '\n' || c == '\r';
  }

  constexpr std::string_view table_is_double_dot_path_segment[] = {"..", "%2e.", ".%2e", "%2e%2e"};

  ada_really_inline ada_constexpr bool is_double_dot_path_segment(std::string_view input) noexcept {
    // This will catch most cases:
    // The length must be 2,4 or 6.
    // We divide by two and require
    // that the result be between 1 and 3 inclusively.
    uint64_t half_length = uint64_t(input.size())/2;
    if(half_length - 1 > 2) { return false; }
    // We have a string of length 2, 4 or 6.
    // We now check the first character:
    if((input[0] != '.') && (input[0] != '%')) { return false; }
     // We are unlikely the get beyond this point.
    int hash_value = (input.size() + (unsigned)(input[0])) & 3;
    const std::string_view target = table_is_double_dot_path_segment[hash_value];
    if(target.size() != input.size()) { return false; }
    // We almost never get here.
    // Optimizing the rest is relatively unimportant.
    auto prefix_equal_unsafe = [](std::string_view a, std::string_view b) {
      uint16_t A, B;
      memcpy(&A,a.data(), sizeof(A));
      memcpy(&B,b.data(), sizeof(B));
      return A == B;
    };
    if(!prefix_equal_unsafe(input,target)) { return false; }
    for(size_t i = 2; i < input.size(); i++) {
      char c = input[i];
      if((uint8_t((c|0x20) - 0x61) <= 25 ? (c|0x20) : c) != target[i]) { return false; }
    }
    return true;
    // The above code might be a bit better than the code below. Compilers
    // are not stupid and may use the fact that these strings have length 2,4 and 6
    // and other tricks.
    //return input == ".." ||
    //  input == ".%2e" || input == ".%2E" ||
    //  input == "%2e." || input == "%2E." ||
    //  input == "%2e%2e" || input == "%2E%2E" || input == "%2E%2e" || input == "%2e%2E";
  }

  ada_really_inline constexpr bool is_single_dot_path_segment(std::string_view input) noexcept {
    return input == "." || input == "%2e" || input == "%2E";
  }

  ada_really_inline constexpr bool is_lowercase_hex(const char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c<= 'f');
  }

  unsigned constexpr convert_hex_to_binary(const char c) noexcept {
    // this code can be optimized.
    if (c <= '9') { return c - '0'; }
    char del = c >= 'a' ? 'a' : 'A';
    return 10 + (c - del);
  }

  std::string percent_decode(const std::string_view input, size_t first_percent) {
    // next line is for safety only, we expect users to avoid calling percent_decode
    // when first_percent is outside the range.
    if(first_percent == std::string_view::npos) { return std::string(input); }
    std::string dest(input.substr(0, first_percent));
    dest.reserve(input.length());
    const char* pointer = input.data() + first_percent;
    const char* end = input.data() + input.size();
    // Optimization opportunity: if the following code gets
    // called often, it can be optimized quite a bit.
    while (pointer < end) {
      const char ch = pointer[0];
      size_t remaining = end - pointer - 1;
      if (ch != '%' || remaining < 2 ||
          (//ch == '%' && // It is unnecessary to check that ch == '%'.
           (!is_ascii_hex_digit(pointer[1]) ||
            !is_ascii_hex_digit(pointer[2])))) {
        dest += ch;
        pointer++;
        continue;
      } else {
        unsigned a = convert_hex_to_binary(pointer[1]);
        unsigned b = convert_hex_to_binary(pointer[2]);
        char c = static_cast<char>(a * 16 + b);
        dest += c;
        pointer += 3;
      }
    }
    return dest;
  }

  std::string percent_encode(const std::string_view input, const uint8_t character_set[]) {
    auto pointer = std::find_if(input.begin(), input.end(), [character_set](const char c) {
      return character_sets::bit_at(character_set, c);
    });
    // Optimization: Don't iterate if percent encode is not required
    if (pointer == input.end()) { return std::string(input); }

    std::string result(input.substr(0,std::distance(input.begin(), pointer)));
    result.reserve(input.length()); // in the worst case, percent encoding might produce 3 characters.

    for (;pointer != input.end(); pointer++) {
      if (character_sets::bit_at(character_set, *pointer)) {
        result.append(character_sets::hex + uint8_t(*pointer) * 4, 3);
      } else {
        result += *pointer;
      }
    }

    return result;
  }


  bool percent_encode(const std::string_view input, const uint8_t character_set[], std::string &out) {
    auto pointer = std::find_if(input.begin(), input.end(), [character_set](const char c) {
      return character_sets::bit_at(character_set, c);
    });
    // Optimization: Don't iterate if percent encode is not required
    if (pointer == input.end()) { return false; }
    out.clear();
    out.append(input.data(), std::distance(input.begin(), pointer));

    for (;pointer != input.end(); pointer++) {
      if (character_sets::bit_at(character_set, *pointer)) {
        out.append(character_sets::hex + uint8_t(*pointer) * 4, 3);
      } else {
        out += *pointer;
      }
    }
    return true;
  }

  bool to_ascii(std::optional<std::string>& out, const std::string_view plain, const bool be_strict, size_t first_percent) {
    std::string percent_decoded_buffer;
    std::string_view input = plain;
    if(first_percent != std::string_view::npos) {
      percent_decoded_buffer = unicode::percent_decode(plain, first_percent);
      input = percent_decoded_buffer;
    }
#if ADA_HAS_ICU
    out = std::string(255, 0);

    UErrorCode status = U_ZERO_ERROR;
    uint32_t options = UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_NONTRANSITIONAL_TO_ASCII;

    if (be_strict) {
      options |= UIDNA_USE_STD3_RULES;
    }

    UIDNA* uidna = uidna_openUTS46(options, &status);
    if (U_FAILURE(status)) {
      return false;
    }

    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    // RFC 1035 section 2.3.4.
    // The domain  name must be at most 255 octets.
    // It cannot contain a label longer than 63 octets.
    // Thus we should never need more than 255 octets, if we
    // do the domain name is in error.
    int32_t length = uidna_nameToASCII_UTF8(uidna,
                                         input.data(),
                                         int32_t(input.length()),
                                         out.value().data(), 255,
                                         &info,
                                         &status);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      out.value().resize(length);
      // When be_strict is true, this should not be allowed!
      length = uidna_nameToASCII_UTF8(uidna,
                                     input.data(),
                                     int32_t(input.length()),
                                     out.value().data(), length,
                                     &info,
                                     &status);
    }

    // A label contains hyphen-minus ('-') in the third and fourth positions.
    info.errors &= ~UIDNA_ERROR_HYPHEN_3_4;
    // A label starts with a hyphen-minus ('-').
    info.errors &= ~UIDNA_ERROR_LEADING_HYPHEN;
    // A label ends with a hyphen-minus ('-').
    info.errors &= ~UIDNA_ERROR_TRAILING_HYPHEN;

    if (!be_strict) { // This seems to violate RFC 1035 section 2.3.4.
      // A non-final domain name label (or the whole domain name) is empty.
      info.errors &= ~UIDNA_ERROR_EMPTY_LABEL;
      // A domain name label is longer than 63 bytes.
      info.errors &= ~UIDNA_ERROR_LABEL_TOO_LONG;
      // A domain name is longer than 255 bytes in its storage form.
      info.errors &= ~UIDNA_ERROR_DOMAIN_NAME_TOO_LONG;
    }

    uidna_close(uidna);

    if (U_FAILURE(status) || info.errors != 0 || length == 0) {
      out = std::nullopt;
      return false;
    }
    out.value().resize(length); // we possibly want to call :shrink_to_fit otherwise we use 255 bytes.
    out.value().shrink_to_fit();
#elif defined(_WIN32) && ADA_WINDOWS_TO_ASCII_FALLBACK
    (void)be_strict; // unused.
    // Fallback on the system if ICU is not available.
    // Windows function assumes UTF-16.
    std::unique_ptr<char16_t[]> buffer(new char16_t[input.size()]);
    auto convert = [](const char* buf, size_t len, char16_t* utf16_output) {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buf);
      size_t pos = 0;
      char16_t* start{utf16_output};
      while (pos < len) {
        // try to convert the next block of 16 ASCII bytes
        if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that they are ascii
          uint64_t v1;
          ::memcpy(&v1, data + pos, sizeof(uint64_t));
          uint64_t v2;
          ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
          uint64_t v{v1 | v2};
          if ((v & 0x8080808080808080) == 0) {
            size_t final_pos = pos + 16;
            while(pos < final_pos) {
              *utf16_output++ = char16_t(buf[pos]);
              pos++;
            }
            continue;
          }
        }
        uint8_t leading_byte = data[pos]; // leading byte
        if (leading_byte < 0b10000000) {
          // converting one ASCII byte !!!
          *utf16_output++ = char16_t(leading_byte);
          pos++;
        } else if ((leading_byte & 0b11100000) == 0b11000000) {
          // We have a two-byte UTF-8, it should become
          // a single UTF-16 word.
          if(pos + 1 >= len) { return 0; } // minimal bound checking
          if ((data[pos + 1] & 0b11000000) != 0b10000000) { return 0; }
          // range check
          uint32_t code_point = (leading_byte & 0b00011111) << 6 | (data[pos + 1] & 0b00111111);
          if (code_point < 0x80 || 0x7ff < code_point) { return 0; }
          *utf16_output++ = char16_t(code_point);
          pos += 2;
        } else if ((leading_byte & 0b11110000) == 0b11100000) {
          // We have a three-byte UTF-8, it should become
          // a single UTF-16 word.
          if(pos + 2 >= len) { return 0; } // minimal bound checking

          if ((data[pos + 1] & 0b11000000) != 0b10000000) { return 0; }
          if ((data[pos + 2] & 0b11000000) != 0b10000000) { return 0; }
          // range check
          uint32_t code_point = (leading_byte & 0b00001111) << 12 |
                      (data[pos + 1] & 0b00111111) << 6 |
                      (data[pos + 2] & 0b00111111);
          if (code_point < 0x800 || 0xffff < code_point ||
              (0xd7ff < code_point && code_point < 0xe000)) {
            return 0;
          }
          *utf16_output++ = char16_t(code_point);
          pos += 3;
        } else if ((leading_byte & 0b11111000) == 0b11110000) { // 0b11110000
          // we have a 4-byte UTF-8 word.
          if(pos + 3 >= len) { return 0; } // minimal bound checking
          if ((data[pos + 1] & 0b11000000) != 0b10000000) { return 0; }
          if ((data[pos + 2] & 0b11000000) != 0b10000000) { return 0; }
          if ((data[pos + 3] & 0b11000000) != 0b10000000) { return 0; }

          // range check
          uint32_t code_point =
              (leading_byte & 0b00000111) << 18 | (data[pos + 1] & 0b00111111) << 12 |
              (data[pos + 2] & 0b00111111) << 6 | (data[pos + 3] & 0b00111111);
          if (code_point <= 0xffff || 0x10ffff < code_point) { return 0; }
          code_point -= 0x10000;
          uint16_t high_surrogate = uint16_t(0xD800 + (code_point >> 10));
          uint16_t low_surrogate = uint16_t(0xDC00 + (code_point & 0x3FF));
          *utf16_output++ = char16_t(high_surrogate);
          *utf16_output++ = char16_t(low_surrogate);
          pos += 4;
        } else {
          return 0;
        }
      }
      return int(utf16_output - start);
    };
    size_t codepoints = convert(input.data(), input.size(), buffer.get());
    if(codepoints == 0) {
          out = std::nullopt;
          return false;
    }
    int required_buffer_size = IdnToAscii(IDN_ALLOW_UNASSIGNED, (LPCWSTR)buffer.get(), codepoints, NULL, 0);

    if(required_buffer_size == 0) {
      out = std::nullopt;
      return false;
    }

    out = std::string(required_buffer_size, 0);
    std::unique_ptr<char16_t[]> ascii_buffer(new char16_t[required_buffer_size]);

    required_buffer_size = IdnToAscii(IDN_ALLOW_UNASSIGNED, (LPCWSTR)buffer.get(), codepoints, (LPWSTR)ascii_buffer.get(), required_buffer_size);
    if(required_buffer_size == 0) {
      out = std::nullopt;
      return false;
    }
    // This will not validate the punycode, so let us work it in reverse.
    int test_reverse = IdnToUnicode(IDN_ALLOW_UNASSIGNED, (LPCWSTR)ascii_buffer.get(), required_buffer_size, NULL, 0);
    if(test_reverse == 0) {
      out = std::nullopt;
      return false;
    }
    out = std::string(required_buffer_size, 0);
    for(size_t i = 0; i < required_buffer_size; i++) { (*out)[i] = char(ascii_buffer.get()[i]); }
#else
    (void)be_strict; // unused.
    out = input; // We cannot do much more for now.
#endif
    return true;
  }

} // namespace ada::unicode
/* end file src/unicode.cpp */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=serializers.cpp
/* begin file src/serializers.cpp */

#include <array>
#include <string>

namespace ada::serializers {

  void find_longest_sequence_of_ipv6_pieces(const std::array<uint16_t, 8>& address, size_t& compress, size_t& compress_length) noexcept {
    for (size_t i = 0; i < 8; i++) {
      if (address[i] == 0) {
        size_t next = i + 1;
        while (next != 8 && address[next] == 0) ++next;
        const size_t count = next - i;
        if (compress_length < count) {
          compress_length = count;
          compress = i;
          if (next == 8) break;
          i = next;
        }
      }
    }
  }

  std::string ipv6(const std::array<uint16_t, 8>& address) noexcept {
    size_t compress_length = 0; // The length of a long sequence of zeros.
    size_t compress = 0; // The start of a long sequence of zeros.
    find_longest_sequence_of_ipv6_pieces(address, compress, compress_length);

    if (compress_length <= 1) {
      // Optimization opportunity: Find a faster way then snprintf for imploding and return here.
      compress = compress_length = 8;
    }

    std::string output(4 * 8 + 7 + 2, '\0');
    size_t piece_index = 0;
    char *point = output.data();
    char *point_end = output.data() + output.size();
    *point++ = '[';
    while (true) {
      if (piece_index == compress) {
        *point++ = ':';
        // If we skip a value initially, we need to write '::', otherwise
        // a single ':' will do since it follows a previous ':'.
        if(piece_index == 0) { *point++ = ':'; }
        piece_index += compress_length;
        if(piece_index == 8) { break; }
      }
      point = std::to_chars(point, point_end, address[piece_index], 16).ptr;
      piece_index++;
      if(piece_index == 8) { break; }
      *point++ = ':';
    }
    *point++ = ']';
    output.resize(point - output.data());
    return output;
  }

  std::string ipv4(const uint64_t address) noexcept {
    std::string output(15, '\0');
    char *point = output.data();
    char *point_end = output.data() + output.size();
    point = std::to_chars(point, point_end, uint8_t(address >> 24)).ptr;
    for (int i = 2; i >= 0; i--) {
     *point++ = '.';
     point = std::to_chars(point, point_end, uint8_t(address >> (i * 8))).ptr;
    }
    output.resize(point - output.data());
    return output;
  }

} // namespace ada::serializers
/* end file src/serializers.cpp */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=implementation.cpp
/* begin file src/implementation.cpp */
#include <string_view>


namespace ada {

  ada_warn_unused tl::expected<ada::url,ada::errors> parse(std::string_view input,
                            const ada::url* base_url,
                            ada::encoding_type encoding) {
    if(encoding != encoding_type::UTF8) {
      // @todo Add support for non UTF8 input
    }
    ada::url u = ada::parser::parse_url(input, base_url, encoding);
    if(!u.is_valid) { return tl::unexpected(errors::generic_error); }
    return u;
  }

  std::string href_from_file(std::string_view input) {
    // This is going to be much faster than constructing a URL.
    std::string tmp_buffer;
    std::string_view internal_input;
    if(unicode::has_tabs_or_newline(input)) {
      tmp_buffer = input;
      helpers::remove_ascii_tab_or_newline(tmp_buffer);
      internal_input = tmp_buffer;
    } else {
      internal_input = input;
    }
    std::string path;
    if(internal_input.empty()) {
      path = "/";
    } else if((internal_input[0] == '/') ||(internal_input[0] == '\\')){
      helpers::parse_prepared_path(internal_input.substr(1), ada::scheme::type::FILE, path);
    } else {
      helpers::parse_prepared_path(internal_input, ada::scheme::type::FILE, path);
    }
    return "file://" + path;
  }

  ada_warn_unused std::string to_string(ada::encoding_type type) {
    switch(type) {
    case ada::encoding_type::UTF8 : return "UTF-8";
    case ada::encoding_type::UTF_16LE : return "UTF-16LE";
    case ada::encoding_type::UTF_16BE : return "UTF-16BE";
    default: unreachable();
    }
  }

} // namespace ada
/* end file src/implementation.cpp */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=helpers.cpp
/* begin file src/helpers.cpp */

#include <algorithm>
#include <charconv>
#include <cstring>
#include <sstream>

namespace ada::helpers {

  template <typename out_iter>
  void encode_json(std::string_view view, out_iter out) {
    // trivial implementation. could be faster.
    const char * hexvalues = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    for(uint8_t c : view) {
      if(c == '\\') {
        *out++ = '\\'; *out++ = '\\';
      } else if(c == '"') {
        *out++ = '\\'; *out++ = '"';
      } else if(c <= 0x1f) {
        *out++ = '\\'; *out++= 'u'; *out++= '0'; *out++= '0';
        *out++ = hexvalues[2*c];
        *out++ = hexvalues[2*c+1];
      } else {
        *out++ = c;
      }
    }
  }

  ada_unused std::string get_state(ada::state s) {
    switch (s) {
      case ada::state::AUTHORITY: return "Authority";
      case ada::state::SCHEME_START: return "Scheme Start";
      case ada::state::SCHEME: return "Scheme";
      case ada::state::HOST: return "Host";
      case ada::state::NO_SCHEME: return "No Scheme";
      case ada::state::FRAGMENT: return "Fragment";
      case ada::state::RELATIVE_SCHEME: return "Relative Scheme";
      case ada::state::RELATIVE_SLASH: return "Relative Slash";
      case ada::state::FILE: return "File";
      case ada::state::FILE_HOST: return "File Host";
      case ada::state::FILE_SLASH: return "File Slash";
      case ada::state::PATH_OR_AUTHORITY: return "Path or Authority";
      case ada::state::SPECIAL_AUTHORITY_IGNORE_SLASHES: return "Special Authority Ignore Slashes";
      case ada::state::SPECIAL_AUTHORITY_SLASHES: return "Special Authority Slashes";
      case ada::state::SPECIAL_RELATIVE_OR_AUTHORITY: return "Special Relative or Authority";
      case ada::state::QUERY: return "Query";
      case ada::state::PATH: return "Path";
      case ada::state::PATH_START: return "Path Start";
      case ada::state::OPAQUE_PATH: return "Opaque Path";
      case ada::state::PORT: return "Port";
      default: return "unknown state";
    }
  }

  ada_really_inline std::optional<std::string_view> prune_fragment(std::string_view& input) noexcept {
    // compiles down to 20--30 instructions including a class to memchr (C function).
    // this function should be quite fast.
    size_t location_of_first = input.find('#');
    if(location_of_first == std::string_view::npos) { return std::nullopt; }
    std::string_view fragment = input;
    fragment.remove_prefix(location_of_first+1);
    input.remove_suffix(input.size() - location_of_first);
    return fragment;
  }

  ada_really_inline void shorten_path(std::string& path, ada::scheme::type type) noexcept {
    size_t first_delimiter = path.find_first_of('/', 1);

    // Let path be url’s path.
    // If url’s scheme is "file", path’s size is 1, and path[0] is a normalized Windows drive letter, then return.
    if (type == ada::scheme::type::FILE && first_delimiter == std::string_view::npos) {
      if (checkers::is_normalized_windows_drive_letter(std::string_view(path.data() + 1, first_delimiter - 1))) {
        return;
      }
    }

    // Remove path’s last item, if any.
    if (!path.empty()) {
      path.erase(path.rfind('/'));
    }
  }

  ada_really_inline void remove_ascii_tab_or_newline(std::string& input) noexcept {
    // if this ever becomes a performance issue, we could use an approach similar to has_tabs_or_newline
    input.erase(std::remove_if(input.begin(), input.end(), [](char c) {
      return ada::unicode::is_ascii_tab_or_newline(c);
    }), input.end());
  }

  ada_really_inline std::string_view substring(std::string_view input, size_t pos) noexcept {
    ada_log("substring(", input, " [", input.size() ,"bytes],", pos, ")");
    return pos > input.size() ? std::string_view() : input.substr(pos);
  }

  // Reverse the byte order.
  ada_really_inline uint64_t swap_bytes(uint64_t val) noexcept {
    // performance: this often compiles to a single instruction (e.g., bswap)
    return ((((val) & 0xff00000000000000ull) >> 56) |
          (((val) & 0x00ff000000000000ull) >> 40) |
          (((val) & 0x0000ff0000000000ull) >> 24) |
          (((val) & 0x000000ff00000000ull) >> 8 ) |
          (((val) & 0x00000000ff000000ull) << 8 ) |
          (((val) & 0x0000000000ff0000ull) << 24) |
          (((val) & 0x000000000000ff00ull) << 40) |
          (((val) & 0x00000000000000ffull) << 56));
  }

  ada_really_inline uint64_t swap_bytes_if_big_endian(uint64_t val) noexcept {
    // performance: under little-endian systems (most systems), this function
    // is free (just returns the input).
#if ADA_IS_BIG_ENDIAN
    return swap_bytes(val);
#else
    return val; // unchanged (trivial)
#endif
  }

  // starting at index location, this finds the next location of a character
  // :, /, \\, ? or [. If none is found, view.size() is returned.
  // For use within get_host_delimiter_location.
  ada_really_inline size_t find_next_host_delimiter_special(std::string_view view, size_t location) noexcept {
    // performance: if you plan to call find_next_host_delimiter more than once,
    // you *really* want find_next_host_delimiter to be inlined, because
    // otherwise, the constants may get reloaded each time (bad).
    auto has_zero_byte = [](uint64_t v) {
      return ((v - 0x0101010101010101) & ~(v)&0x8080808080808080);
    };
    auto index_of_first_set_byte = [](uint64_t v) {
      return ((((v - 1) & 0x101010101010101) * 0x101010101010101) >> 56) - 1;
    };
    auto broadcast = [](uint8_t v) -> uint64_t { return 0x101010101010101 * v; };
    size_t i = location;
    uint64_t mask1 = broadcast(':');
    uint64_t mask2 = broadcast('/');
    uint64_t mask3 = broadcast('\\');
    uint64_t mask4 = broadcast('?');
    uint64_t mask5 = broadcast('[');
    // This loop will get autovectorized under many optimizing compilers,
    // so you get actually SIMD!
    for (; i + 7 < view.size(); i += 8) {
      uint64_t word{};
      // performance: the next memcpy translates into a single CPU instruction.
      memcpy(&word, view.data() + i, sizeof(word));
      // performance: on little-endian systems (most systems), this next line is free.
      word = swap_bytes_if_big_endian(word);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor3 = word ^ mask3;
      uint64_t xor4 = word ^ mask4;
      uint64_t xor5 = word ^ mask5;
      uint64_t is_match = has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor3) | has_zero_byte(xor4) | has_zero_byte(xor5);
      if(is_match) {
        return i + index_of_first_set_byte(is_match);
      }
    }
    if (i < view.size()) {
      uint64_t word{};
      // performance: the next memcpy translates into a function call, but
      // that is difficult to avoid. Might be a bit expensive.
      memcpy(&word, view.data() + i, view.size() - i);
      word = swap_bytes_if_big_endian(word);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor3 = word ^ mask3;
      uint64_t xor4 = word ^ mask4;
      uint64_t xor5 = word ^ mask5;
      uint64_t is_match = has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor3) | has_zero_byte(xor4) | has_zero_byte(xor5);
      if(is_match) {
        return i + index_of_first_set_byte(is_match);
      }
    }
    return view.size();
  }

  // starting at index location, this finds the next location of a character
  // :, /, ? or [. If none is found, view.size() is returned.
  // For use within get_host_delimiter_location.
  ada_really_inline size_t find_next_host_delimiter(std::string_view view, size_t location) noexcept {
    // performance: if you plan to call find_next_host_delimiter more than once,
    // you *really* want find_next_host_delimiter to be inlined, because
    // otherwise, the constants may get reloaded each time (bad).
    auto has_zero_byte = [](uint64_t v) {
      return ((v - 0x0101010101010101) & ~(v)&0x8080808080808080);
    };
    auto index_of_first_set_byte = [](uint64_t v) {
      return ((((v - 1) & 0x101010101010101) * 0x101010101010101) >> 56) - 1;
    };
    auto broadcast = [](uint8_t v) -> uint64_t { return 0x101010101010101 * v; };
    size_t i = location;
    uint64_t mask1 = broadcast(':');
    uint64_t mask2 = broadcast('/');
    uint64_t mask4 = broadcast('?');
    uint64_t mask5 = broadcast('[');
    // This loop will get autovectorized under many optimizing compilers,
    // so you get actually SIMD!
    for (; i + 7 < view.size(); i += 8) {
      uint64_t word{};
      // performance: the next memcpy translates into a single CPU instruction.
      memcpy(&word, view.data() + i, sizeof(word));
      // performance: on little-endian systems (most systems), this next line is free.
      word = swap_bytes_if_big_endian(word);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor4 = word ^ mask4;
      uint64_t xor5 = word ^ mask5;
      uint64_t is_match = has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor4) | has_zero_byte(xor5);
      if(is_match) {
        return i + index_of_first_set_byte(is_match);
      }
    }
    if (i < view.size()) {
      uint64_t word{};
      // performance: the next memcpy translates into a function call, but
      // that is difficult to avoid. Might be a bit expensive.
      memcpy(&word, view.data() + i, view.size() - i);
      // performance: on little-endian systems (most systems), this next line is free.
      word = swap_bytes_if_big_endian(word);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor4 = word ^ mask4;
      uint64_t xor5 = word ^ mask5;
      uint64_t is_match = has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor4) | has_zero_byte(xor5);
      if(is_match) {
        return i + index_of_first_set_byte(is_match);
      }
    }
    return view.size();
  }

  ada_really_inline std::pair<size_t,bool> get_host_delimiter_location(const bool is_special, std::string_view& view) noexcept {
    /**
     * The spec at https://url.spec.whatwg.org/#hostname-state expects us to compute
     * a variable called insideBrackets but this variable is only used once, to check
     * whether a ':' character was found outside brackets.
     * Exact text:
     * "Otherwise, if c is U+003A (:) and insideBrackets is false, then:".
     * It is conceptually simpler and arguably more efficient to just return a Boolean
     * indicating whether ':' was found outside brackets.
     */
    const size_t view_size = view.size();
    size_t location = 0;
    bool found_colon = false;
    /**
     * Performance analysis:
     *
     * We are basically seeking the end of the hostname which can be indicated
     * by the end of the view, or by one of the characters ':', '/', '?', '\\' (where '\\' is only
     * applicable for special URLs). However, these must appear outside a bracket range. E.g.,
     * if you have [something?]fd: then the '?' does not count.
     *
     * So we can skip ahead to the next delimiter, as long as we include '[' in the set of delimiters,
     * and that we handle it first.
     *
     * So the trick is to have a fast function that locates the next delimiter. Unless we find '[',
     * then it only needs to be called once! Ideally, such a function would be provided by the C++
     * standard library, but it seems that find_first_of is not very fast, so we are forced to roll
     * our own.
     *
     * We do not break into two loops for speed, but for clarity.
     */
    if(is_special) {
      // We move to the next delimiter.
      location = find_next_host_delimiter_special(view, location);
      // Unless we find '[' then we are going only going to have to call
      // find_next_host_delimiter_special once.
      for (;location < view_size; location = find_next_host_delimiter_special(view, location)) {
        if (view[location] == '[') {
          location = view.find(']', location);
          if (location == std::string_view::npos) {
            // performance: view.find might get translated to a memchr, which
            // has no notion of std::string_view::npos, so the code does not
            // reflect the assembly.
            location = view_size;
            break;
          }
        } else {
          found_colon = view[location] == ':';
          break;
        }
      }
    } else {
      // We move to the next delimiter.
      location = find_next_host_delimiter(view, location);
      // Unless we find '[' then we are going only going to have to call
      // find_next_host_delimiter_special once.
      for (;location < view_size; location = find_next_host_delimiter(view, location)) {
        if (view[location] == '[') {
          location = view.find(']', location);
          if (location == std::string_view::npos) {
            // performance: view.find might get translated to a memchr, which
            // has no notion of std::string_view::npos, so the code does not
            // reflect the assembly.
            location = view_size;
            break;
          }
        } else {
          found_colon = view[location] == ':';
          break;
        }
      }
    }
    // performance: remove_suffix may translate into a single instruction.
    view.remove_suffix(view_size - location);
    return {location, found_colon};
  }

  ada_really_inline void trim_c0_whitespace(std::string_view& input) noexcept {
    while(!input.empty() && ada::unicode::is_c0_control_or_space(input.front())) { input.remove_prefix(1); }
    while(!input.empty() && ada::unicode::is_c0_control_or_space(input.back())) { input.remove_suffix(1); }
  }


  ada_really_inline bool parse_prepared_path(std::string_view input, ada::scheme::type type, std::string& path) {
    ada_log("parse_path ", input);
    uint8_t accumulator = checkers::path_signature(input);
    // Let us first detect a trivial case.
    // If it is special, we check that we have no dot, no %,  no \ and no
    // character needing percent encoding. Otherwise, we check that we have no %,
    // no dot, and no character needing percent encoding.
    bool special = type != ada::scheme::NOT_SPECIAL;
    bool trivial_path =
        (special ? (accumulator == 0) : ((accumulator & 0b11111101) == 0)) &&
        (type != ada::scheme::type::FILE);
    if (trivial_path) {
      ada_log("parse_path trivial");
      path += '/';
      path += input;
      return true;
    }
    // We are going to need to look a bit at the path, but let us see if we can
    // ignore percent encoding *and* backslashes *and* percent characters.
    // Except for the trivial case, this is likely to capture 99% of paths out
    // there.
    bool fast_path = (special && (accumulator & 0b11111011) == 0) &&
                    (type != ada::scheme::type::FILE);
    if (fast_path) {
      ada_log("parse_path fast");
      // Here we don't need to worry about \ or percent encoding.
      // We also do not have a file protocol. We might have dots, however,
      // but dots must as appear as '.', and they cannot be encoded because
      // the symbol '%' is not present.
      size_t previous_location = 0; // We start at 0.
      do {
        size_t new_location = input.find('/', previous_location);
        //std::string_view path_view = input;
        // We process the last segment separately:
        if (new_location == std::string_view::npos) {
          std::string_view path_view = input.substr(previous_location);
          if (path_view == "..") { // The path ends with ..
            // e.g., if you receive ".." with an empty path, you go to "/".
            if(path.empty()) { path = '/'; return true; }
            // Fast case where we have nothing to do:
            if(path.back() == '/') { return true; }
            // If you have the path "/joe/myfriend",
            // then you delete 'myfriend'.
            path.resize(path.rfind('/') + 1);
            return true;
          }
          path += '/';
          if (path_view != ".") {
            path.append(path_view);
          }
          return true;
        } else {
          // This is a non-final segment.
          std::string_view path_view = input.substr(previous_location, new_location - previous_location);
          previous_location = new_location + 1;
          if (path_view == "..") {
            if(!path.empty()) { path.erase(path.rfind('/')); }
          } else if (path_view != ".") {
            path += '/';
            path.append(path_view);
          }
        }
      } while (true);
    } else {
      ada_log("parse_path slow");
      // we have reached the general case
      bool needs_percent_encoding = (accumulator & 1);
      std::string path_buffer_tmp;
      do {
        size_t location = (special && (accumulator & 2))
                              ? input.find_first_of("/\\")
                              : input.find('/');
        std::string_view path_view = input;
        if (location != std::string_view::npos) {
          path_view.remove_suffix(path_view.size() - location);
          input.remove_prefix(location + 1);
        }
        // path_buffer is either path_view or it might point at a percent encoded temporary file.
        std::string_view path_buffer =
         (needs_percent_encoding
           && ada::unicode::percent_encode(path_view, character_sets::PATH_PERCENT_ENCODE, path_buffer_tmp)) ?
          path_buffer_tmp :
          path_view;
        if (unicode::is_double_dot_path_segment(path_buffer)) {
          helpers::shorten_path(path, type);
          if (location == std::string_view::npos) {
            path += '/';
          }
        } else if (unicode::is_single_dot_path_segment(path_buffer) &&
                  (location == std::string_view::npos)) {
          path += '/';
        }
        // Otherwise, if path_buffer is not a single-dot path segment, then:
        else if (!unicode::is_single_dot_path_segment(path_buffer)) {
          // If url’s scheme is "file", url’s path is empty, and path_buffer is a
          // Windows drive letter, then replace the second code point in
          // path_buffer with U+003A (:).
          if (type == ada::scheme::type::FILE && path.empty() &&
              checkers::is_windows_drive_letter(path_buffer)) {
            path += '/';
            path += path_buffer[0];
            path += ':';
            path_buffer.remove_prefix(2);
            path.append(path_buffer);
          } else {
            // Append path_buffer to url’s path.
            path += '/';
            path.append(path_buffer);
          }
        }
        if (location == std::string_view::npos) {
          return true;
        }
      } while (true);
    }
  }

  ada_really_inline void strip_trailing_spaces_from_opaque_path(ada::url& url) noexcept {
    if (!url.has_opaque_path) return;
    if (url.fragment.has_value()) return;
    if (url.query.has_value()) return;
    while (!url.path.empty() && url.path.back() == ' ') { url.path.resize(url.path.size()-1); }
  }

  ada_really_inline size_t find_authority_delimiter_special(std::string_view view) noexcept {
    auto has_zero_byte = [](uint64_t v) {
      return ((v - 0x0101010101010101) & ~(v)&0x8080808080808080);
    };
    auto index_of_first_set_byte = [](uint64_t v) {
      return ((((v - 1) & 0x101010101010101) * 0x101010101010101) >> 56) - 1;
    };
    auto broadcast = [](uint8_t v) -> uint64_t { return 0x101010101010101 * v; };
    size_t i = 0;
    uint64_t mask1 = broadcast('@');
    uint64_t mask2 = broadcast('/');
    uint64_t mask3 = broadcast('?');
    uint64_t mask4 = broadcast('\\');

    for (; i + 7 < view.size(); i += 8) {
      uint64_t word{};
      memcpy(&word, view.data() + i, sizeof(word));
      word = swap_bytes_if_big_endian(word);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor3 = word ^ mask3;
      uint64_t xor4 = word ^ mask4;
      uint64_t is_match = has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor3) | has_zero_byte(xor4);
      if (is_match) {
        return i + index_of_first_set_byte(is_match);
      }
    }

    if (i < view.size()) {
      uint64_t word{};
      memcpy(&word, view.data() + i, view.size() - i);
      word = swap_bytes_if_big_endian(word);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor3 = word ^ mask3;
      uint64_t xor4 = word ^ mask4;
      uint64_t is_match = has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor3) | has_zero_byte(xor4);
      if (is_match) {
        return i + index_of_first_set_byte(is_match);
      }
    }

    return view.size();
  }

  ada_really_inline size_t find_authority_delimiter(std::string_view view) noexcept {
    auto has_zero_byte = [](uint64_t v) {
      return ((v - 0x0101010101010101) & ~(v)&0x8080808080808080);
    };
    auto index_of_first_set_byte = [](uint64_t v) {
      return ((((v - 1) & 0x101010101010101) * 0x101010101010101) >> 56) - 1;
    };
    auto broadcast = [](uint8_t v) -> uint64_t { return 0x101010101010101 * v; };
    size_t i = 0;
    uint64_t mask1 = broadcast('@');
    uint64_t mask2 = broadcast('/');
    uint64_t mask3 = broadcast('?');

    for (; i + 7 < view.size(); i += 8) {
      uint64_t word{};
      memcpy(&word, view.data() + i, sizeof(word));
      word = swap_bytes_if_big_endian(word);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor3 = word ^ mask3;
      uint64_t is_match = has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor3);
      if (is_match) {
        return i + index_of_first_set_byte(is_match);
      }
    }

    if (i < view.size()) {
      uint64_t word{};
      memcpy(&word, view.data() + i, view.size() - i);
      word = swap_bytes_if_big_endian(word);
      uint64_t xor1 = word ^ mask1;
      uint64_t xor2 = word ^ mask2;
      uint64_t xor3 = word ^ mask3;
      uint64_t is_match = has_zero_byte(xor1) | has_zero_byte(xor2) | has_zero_byte(xor3);
      if (is_match) {
        return i + index_of_first_set_byte(is_match);
      }
    }

    return view.size();
  }
} // namespace ada::helpers

namespace ada {
  ada_warn_unused std::string to_string(ada::state state) {
    return ada::helpers::get_state(state);
  }
}
/* end file src/helpers.cpp */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=url.cpp
/* begin file src/url.cpp */

#include <numeric>
#include <algorithm>
#include <string>

namespace ada {
  ada_really_inline bool url::parse_path(std::string_view input) {
    ada_log("parse_path ", input);
    std::string tmp_buffer;
    std::string_view internal_input;
    if(unicode::has_tabs_or_newline(input)) {
      tmp_buffer = input;
      // Optimization opportunity: Instead of copying and then pruning, we could just directly
      // build the string from user_input.
      helpers::remove_ascii_tab_or_newline(tmp_buffer);
      internal_input = tmp_buffer;
    } else {
      internal_input = input;
    }

    // If url is special, then:
    if (is_special()) {
      if(internal_input.empty()) {
        path = "/";
      } else if((internal_input[0] == '/') ||(internal_input[0] == '\\')){
        return helpers::parse_prepared_path(internal_input.substr(1), get_scheme_type(), path);
      } else {
        return helpers::parse_prepared_path(internal_input, get_scheme_type(), path);
      }
    } else if (!internal_input.empty()) {
      if(internal_input[0] == '/') {
        return helpers::parse_prepared_path(internal_input.substr(1), get_scheme_type(), path);
      } else {
        return helpers::parse_prepared_path(internal_input, get_scheme_type(), path);
      }
    } else {
      if(!host.has_value()) {
        path = "/";
      }
    }
    return true;
  }

  bool url::parse_opaque_host(std::string_view input) {
    ada_log("parse_opaque_host ", input, "[", input.size(), " bytes]");
    if (std::any_of(input.begin(), input.end(), ada::unicode::is_forbidden_host_code_point)) {
      return is_valid = false;
    }

    // Return the result of running UTF-8 percent-encode on input using the C0 control percent-encode set.
    host = ada::unicode::percent_encode(input, ada::character_sets::C0_CONTROL_PERCENT_ENCODE);
    return true;
  }

  bool url::parse_ipv4(std::string_view input) {
    ada_log("parse_ipv4 ", input, "[", input.size(), " bytes]");
    if(input.back()=='.') {
      input.remove_suffix(1);
    }
    size_t digit_count{0};
    int pure_decimal_count = 0; // entries that are decimal
    std::string_view original_input = input; // we might use this if pure_decimal_count == 4.
    uint64_t ipv4{0};
    // we could unroll for better performance?
    for(;(digit_count < 4) && !(input.empty()); digit_count++) {
      uint32_t segment_result{}; // If any number exceeds 32 bits, we have an error.
      bool is_hex = checkers::has_hex_prefix(input);
      if(is_hex && ((input.length() == 2)|| ((input.length() > 2) && (input[2]=='.')))) {
        // special case
        segment_result = 0;
        input.remove_prefix(2);
      } else {
        std::from_chars_result r;
        if(is_hex) {
          r = std::from_chars(input.data() + 2, input.data() + input.size(), segment_result, 16);
        } else if ((input.length() >= 2) && input[0] == '0' && checkers::is_digit(input[1])) {
          r = std::from_chars(input.data() + 1, input.data() + input.size(), segment_result, 8);
        } else {
          pure_decimal_count++;
          r = std::from_chars(input.data(), input.data() + input.size(), segment_result, 10);
        }
        if (r.ec != std::errc()) { return is_valid = false; }
        input.remove_prefix(r.ptr-input.data());
      }
      if(input.empty()) {
        // We have the last value.
        // At this stage, ipv4 contains digit_count*8 bits.
        // So we have 32-digit_count*8 bits left.
        if(segment_result > (uint64_t(1)<<(32-digit_count*8))) { return is_valid = false; }
        ipv4 <<=(32-digit_count*8);
        ipv4 |= segment_result;
        goto final;
      } else {
        // There is more, so that the value must no be larger than 255
        // and we must have a '.'.
        if ((segment_result>255) || (input[0]!='.')) { return is_valid = false; }
        ipv4 <<=8;
        ipv4 |= segment_result;
        input.remove_prefix(1); // remove '.'
      }
    }
    if((digit_count != 4) || (!input.empty())) {return is_valid = false; }
    final:
    // We could also check r.ptr to see where the parsing ended.
    if(pure_decimal_count == 4) {
      host = original_input; // The original input was already all decimal and we validated it.
    } else {
      host = ada::serializers::ipv4(ipv4); // We have to reserialize the address.
    }
    return true;
  }

  bool url::parse_ipv6(std::string_view input) {
    ada_log("parse_ipv6 ", input, "[", input.size(), " bytes]");

    if(input.empty()) { return is_valid = false; }
    // Let address be a new IPv6 address whose IPv6 pieces are all 0.
    std::array<uint16_t, 8> address{};

    // Let pieceIndex be 0.
    int piece_index = 0;

    // Let compress be null.
    std::optional<int> compress{};

    // Let pointer be a pointer for input.
    std::string_view::iterator pointer = input.begin();

    // If c is U+003A (:), then:
    if (input[0] == ':') {
      // If remaining does not start with U+003A (:), validation error, return failure.
      if(input.size() == 1 || input[1] != ':') {
        ada_log("parse_ipv6 starts with : but the rest does not start with :");
        return is_valid = false;
      }

      // Increase pointer by 2.
      pointer += 2;

      // Increase pieceIndex by 1 and then set compress to pieceIndex.
      compress = ++piece_index;
    }

    // While c is not the EOF code point:
    while (pointer != input.end()) {
      // If pieceIndex is 8, validation error, return failure.
      if (piece_index == 8) {
        ada_log("parse_ipv6 piece_index == 8");
        return is_valid = false;
      }

      // If c is U+003A (:), then:
      if (*pointer == ':') {
        // If compress is non-null, validation error, return failure.
        if (compress.has_value()) {
          ada_log("parse_ipv6 compress is non-null");
          return is_valid = false;
        }

        // Increase pointer and pieceIndex by 1, set compress to pieceIndex, and then continue.
        pointer++;
        compress = ++piece_index;
        continue;
      }

      // Let value and length be 0.
      uint16_t value = 0, length = 0;

      // While length is less than 4 and c is an ASCII hex digit,
      // set value to value × 0x10 + c interpreted as hexadecimal number, and increase pointer and length by 1.
      while (length < 4 && pointer != input.end() && unicode::is_ascii_hex_digit(*pointer)) {
        // https://stackoverflow.com/questions/39060852/why-does-the-addition-of-two-shorts-return-an-int
        value = uint16_t(value * 0x10 + unicode::convert_hex_to_binary(*pointer));
        pointer++;
        length++;
      }

      // If c is U+002E (.), then:
      if (pointer != input.end() && *pointer == '.') {
        // If length is 0, validation error, return failure.
        if (length == 0) {
          ada_log("parse_ipv6 length is 0");
          return is_valid = false;
        }

        // Decrease pointer by length.
        pointer -= length;

        // If pieceIndex is greater than 6, validation error, return failure.
        if (piece_index > 6) {
          ada_log("parse_ipv6 piece_index > 6");
          return is_valid = false;
        }

        // Let numbersSeen be 0.
        int numbers_seen = 0;

        // While c is not the EOF code point:
        while (pointer != input.end()) {
          // Let ipv4Piece be null.
          std::optional<uint16_t> ipv4_piece{};

          // If numbersSeen is greater than 0, then:
          if (numbers_seen > 0) {
            // If c is a U+002E (.) and numbersSeen is less than 4, then increase pointer by 1.
            if (*pointer == '.' && numbers_seen < 4) {
              pointer++;
            }
            // Otherwise, validation error, return failure.
            else {
              ada_log("parse_ipv6 Otherwise, validation error, return failure");
              return is_valid = false;
            }
          }

          // If c is not an ASCII digit, validation error, return failure.
          if (pointer == input.end() || !checkers::is_digit(*pointer)) {
            ada_log("parse_ipv6 If c is not an ASCII digit, validation error, return failure");
            return is_valid = false;
          }

          // While c is an ASCII digit:
          while (pointer != input.end() && checkers::is_digit(*pointer)) {
            // Let number be c interpreted as decimal number.
            int number = *pointer - '0';

            // If ipv4Piece is null, then set ipv4Piece to number.
            if (!ipv4_piece.has_value()) {
              ipv4_piece = number;
            }
            // Otherwise, if ipv4Piece is 0, validation error, return failure.
            else if (ipv4_piece == 0) {
              ada_log("parse_ipv6 if ipv4Piece is 0, validation error");
              return is_valid = false;
            }
            // Otherwise, set ipv4Piece to ipv4Piece × 10 + number.
            else {
              ipv4_piece = *ipv4_piece * 10 + number;
            }

            // If ipv4Piece is greater than 255, validation error, return failure.
            if (ipv4_piece > 255) {
              ada_log("parse_ipv6 ipv4_piece > 255");
              return is_valid = false;
            }

            // Increase pointer by 1.
            pointer++;
          }

          // Set address[pieceIndex] to address[pieceIndex] × 0x100 + ipv4Piece.
          // https://stackoverflow.com/questions/39060852/why-does-the-addition-of-two-shorts-return-an-int
          address[piece_index] = uint16_t(address[piece_index] * 0x100 + *ipv4_piece);

          // Increase numbersSeen by 1.
          numbers_seen++;

          // If numbersSeen is 2 or 4, then increase pieceIndex by 1.
          if (numbers_seen == 2 || numbers_seen == 4) {
            piece_index++;
          }
        }

        // If numbersSeen is not 4, validation error, return failure.
        if (numbers_seen != 4) {
          return is_valid = false;
        }

        // Break.
        break;
      }
      // Otherwise, if c is U+003A (:):
      else if ((pointer != input.end()) && (*pointer == ':')) {
        // Increase pointer by 1.
        pointer++;

        // If c is the EOF code point, validation error, return failure.
        if (pointer == input.end()) {
          ada_log("parse_ipv6 If c is the EOF code point, validation error, return failure");
          return is_valid = false;
        }
      }
      // Otherwise, if c is not the EOF code point, validation error, return failure.
      else if (pointer != input.end()) {
        ada_log("parse_ipv6 Otherwise, if c is not the EOF code point, validation error, return failure");
        return is_valid = false;
      }

      // Set address[pieceIndex] to value.
      address[piece_index] = value;

      // Increase pieceIndex by 1.
      piece_index++;
    }

    // If compress is non-null, then:
    if (compress.has_value()) {
      // Let swaps be pieceIndex − compress.
      int swaps = piece_index - *compress;

      // Set pieceIndex to 7.
      piece_index = 7;

      // While pieceIndex is not 0 and swaps is greater than 0,
      // swap address[pieceIndex] with address[compress + swaps − 1], and then decrease both pieceIndex and swaps by 1.
      while (piece_index != 0 && swaps > 0) {
        std::swap(address[piece_index], address[*compress + swaps - 1]);
        piece_index--;
        swaps--;
      }
    }
    // Otherwise, if compress is null and pieceIndex is not 8, validation error, return failure.
    else if (piece_index != 8) {
      ada_log("parse_ipv6 if compress is null and pieceIndex is not 8, validation error, return failure");
      return is_valid = false;
    }
    host = ada::serializers::ipv6(address);
    ada_log("parse_ipv6 ", *host);
    return true;
  }

  ada_really_inline bool url::parse_host(std::string_view input) {
    ada_log("parse_host ", input, "[", input.size(), " bytes]");
    if(input.empty()) { return is_valid = false; } // technically unnecessary.
    // If input starts with U+005B ([), then:
    if (input[0] == '[') {
      // If input does not end with U+005D (]), validation error, return failure.
      if (input.back() != ']') {
        return is_valid = false;
      }
      ada_log("parse_host ipv6");

      // Return the result of IPv6 parsing input with its leading U+005B ([) and trailing U+005D (]) removed.
      input.remove_prefix(1);
      input.remove_suffix(1);
      return parse_ipv6(input);
    }

    // If isNotSpecial is true, then return the result of opaque-host parsing input.
    if (!is_special()) {
      return parse_opaque_host(input);
    }
    // Let domain be the result of running UTF-8 decode without BOM on the percent-decoding of input.
    // Let asciiDomain be the result of running domain to ASCII with domain and false.
    // The most common case is an ASCII input, in which case we do not need to call the expensive 'to_ascii'
    // if a few conditions are met: no '%' and no 'xn-' subsequence.
    std::string buffer = std::string(input);
    // This next function checks that the result is ascii, but we are going to
    // to check anyhow with is_forbidden.
    // bool is_ascii =
    unicode::to_lower_ascii(buffer.data(), buffer.size());
    bool is_forbidden = unicode::contains_forbidden_domain_code_point(buffer.data(), buffer.size());
    if (is_forbidden == 0 && buffer.find("xn-") == std::string_view::npos) {
      // fast path
      host = std::move(buffer);
      if (checkers::is_ipv4(host.value())) {
        ada_log("parse_host fast path ipv4");
        return parse_ipv4(host.value());
      }
      ada_log("parse_host fast path ", *host);
      return true;
    }
    ada_log("parse_host calling to_ascii");
    is_valid = ada::unicode::to_ascii(host, input, false,  input.find('%'));
    if (!is_valid) {
      ada_log("parse_host to_ascii returns false");
      return is_valid = false;
    }

    if(std::any_of(host.value().begin(), host.value().end(), ada::unicode::is_forbidden_domain_code_point)) {
      host = std::nullopt;
      return is_valid = false;
    }

    // If asciiDomain ends in a number, then return the result of IPv4 parsing asciiDomain.
    if(checkers::is_ipv4(host.value())) {
      ada_log("parse_host got ipv4", *host);
      return parse_ipv4(host.value());
    }

    return true;
  }

  template <bool has_state_override>
  ada_really_inline bool url::parse_scheme(const std::string_view input) {
    auto parsed_type = ada::scheme::get_scheme_type(input);
    bool is_input_special = (parsed_type != ada::scheme::NOT_SPECIAL);
    /**
     * In the common case, we will immediately recognize a special scheme (e.g., http, https),
     * in which case, we can go really fast.
     **/
    if(is_input_special) { // fast path!!!
      if (has_state_override) {
        // If url’s scheme is not a special scheme and buffer is a special scheme, then return.
        if (is_special() != is_input_special) {
          return true;
        }

        // If url includes credentials or has a non-null port, and buffer is "file", then return.
        if ((includes_credentials() || port.has_value()) && parsed_type == ada::scheme::type::FILE) {
          return true;
        }

        // If url’s scheme is "file" and its host is an empty host, then return.
        // An empty host is the empty string.
        if (get_scheme_type() == ada::scheme::type::FILE && host.has_value() && host.value().empty()) {
          return true;
        }
      }

      type = parsed_type;

      if (has_state_override) {
        // This is uncommon.
        uint16_t urls_scheme_port = get_special_port();

        if (urls_scheme_port) {
          // If url’s port is url’s scheme’s default port, then set url’s port to null.
          if (port.has_value() && *port == urls_scheme_port) {
            port = std::nullopt;
          }
        }
      }
    } else { // slow path
      std::string _buffer = std::string(input);
      // Next function is only valid if the input is ASCII and returns false
      // otherwise, but it seems that we always have ascii content so we do not need
      // to check the return value.
      //bool is_ascii =
      unicode::to_lower_ascii(_buffer.data(), _buffer.size());

      if (has_state_override) {
        // If url’s scheme is a special scheme and buffer is not a special scheme, then return.
        // If url’s scheme is not a special scheme and buffer is a special scheme, then return.
        if (is_special() != ada::scheme::is_special(_buffer)) {
          return true;
        }

        // If url includes credentials or has a non-null port, and buffer is "file", then return.
        if ((includes_credentials() || port.has_value()) && _buffer == "file") {
          return true;
        }

        // If url’s scheme is "file" and its host is an empty host, then return.
        // An empty host is the empty string.
        if (get_scheme_type() == ada::scheme::type::FILE && host.has_value() && host.value().empty()) {
          return true;
        }
      }

      set_scheme(std::move(_buffer));

      if (has_state_override) {
        // This is uncommon.
        uint16_t urls_scheme_port = get_special_port();

        if (urls_scheme_port) {
          // If url’s port is url’s scheme’s default port, then set url’s port to null.
          if (port.has_value() && *port == urls_scheme_port) {
            port = std::nullopt;
          }
        }
      }
    }

    return true;
  }

  std::string url::to_string() const {
    if (!is_valid) {
      return "null";
    }
    std::string answer;
    auto back = std::back_insert_iterator(answer);
    answer.append("{\n");
    answer.append("\t\"scheme\":\"");
    helpers::encode_json(get_scheme(), back);
    answer.append("\",\n");
    if(includes_credentials()) {
      answer.append("\t\"username\":\"");
      helpers::encode_json(username, back);
      answer.append("\",\n");
      answer.append("\t\"password\":\"");
      helpers::encode_json(password, back);
      answer.append("\",\n");
    }
    if(host.has_value()) {
      answer.append("\t\"host\":\"");
      helpers::encode_json(host.value(), back);
      answer.append("\",\n");
    }
    if(port.has_value()) {
      answer.append("\t\"port\":\"");
      answer.append(std::to_string(port.value()));
      answer.append("\",\n");
    }
    answer.append("\t\"path\":\"");
    helpers::encode_json(path, back);
    answer.append("\",\n");
    answer.append("\t\"opaque path\":");
    answer.append((has_opaque_path ? "true" : "false"));
    if(query.has_value()) {
      answer.append(",\n");
      answer.append("\t\"query\":\"");
      helpers::encode_json(query.value(), back);
      answer.append("\"");
    }
    if(fragment.has_value()) {
      answer.append(",\n");
      answer.append("\t\"fragment\":\"");
      helpers::encode_json(fragment.value(), back);
      answer.append("\"");
    }
    answer.append("\n}");
    return answer;
  }

  [[nodiscard]] bool url::has_valid_domain() const noexcept {
    if(!host.has_value()) { return false; }
    return checkers::verify_dns_length(host.value());
  }
} // namespace ada
/* end file src/url.cpp */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=url-getters.cpp
/* begin file src/url-getters.cpp */
/**
 * @file url-getters.cpp
 * Includes all the getters of `ada::url`
 */

#include <algorithm>
#include <string>

namespace ada {

  [[nodiscard]] std::string url::get_href() const noexcept {
    std::string output = get_protocol();
    size_t url_delimiter_count = std::count(path.begin(), path.end(), '/');

    if (host.has_value()) {
      output += "//";
      if (includes_credentials()) {
        output += get_username();
        if (!get_password().empty()) {
          output += ":" + get_password();
        }
        output += "@";
      }

      output += get_host();
    } else if (!has_opaque_path && url_delimiter_count > 1 && path.length() >= 2 && path[0] == '/' && path[1] == '/') {
      // If url’s host is null, url does not have an opaque path, url’s path’s size is greater than 1,
      // and url’s path[0] is the empty string, then append U+002F (/) followed by U+002E (.) to output.
      output += "/.";
    }

    output += get_pathname() 
           // If query is non-null, then set this’s query object’s list to the result of parsing query.
           + (query.has_value() ? "?" + query.value() : "")
           // If  url’s fragment is non-null, then append U+0023 (#), followed by url’s fragment, to output.
           + (fragment.has_value() ? "#" + fragment.value() : "");
    return output;
  }

  [[nodiscard]] std::string url::get_origin() const noexcept {
    if (is_special()) {
      // Return a new opaque origin.
      if (get_scheme_type() == scheme::FILE) { return "null"; }

      return get_protocol() + "//" + get_host();
    }

    if (get_scheme() == "blob") {
      if (path.length() > 0) {
        url path_result = ada::parser::parse_url(get_pathname());
        if (path_result.is_valid) {
          if (path_result.is_special()) {
            return path_result.get_protocol() + "//" + path_result.get_host();
          }
        }
      }
    }

    // Return a new opaque origin.
    return "null";
  }

  [[nodiscard]] std::string url::get_protocol() const noexcept {
    return std::string(get_scheme()) + ":";
  }

  [[nodiscard]] std::string url::get_host() const noexcept {
    // If url’s host is null, then return the empty string.
    // If url’s port is null, return url’s host, serialized.
    // Return url’s host, serialized, followed by U+003A (:) and url’s port, serialized.
    if (!host.has_value()) { return ""; }
    return host.value() + (port.has_value() ? ":" + get_port() : "");
  }

  [[nodiscard]] std::string url::get_hostname() const noexcept {
    return host.value_or("");
  }

  [[nodiscard]] std::string url::get_pathname() const noexcept {
    return path;
  }

  [[nodiscard]] std::string url::get_search() const noexcept {
    // If this’s URL’s query is either null or the empty string, then return the empty string.
    // Return U+003F (?), followed by this’s URL’s query.
    return (!query.has_value() || (query.value().empty())) ? "" : "?" + query.value();
  }

  [[nodiscard]] std::string url::get_username() const noexcept {
    return username;
  }

  [[nodiscard]] std::string url::get_password() const noexcept {
    return password;
  }

  [[nodiscard]] std::string url::get_port() const noexcept {
    return port.has_value() ? std::to_string(port.value()) : "";
  }

  [[nodiscard]] std::string url::get_hash() const noexcept {
    // If this’s URL’s fragment is either null or the empty string, then return the empty string.
    // Return U+0023 (#), followed by this’s URL’s fragment.
    return (!fragment.has_value() || (fragment.value().empty())) ? "" : "#" + fragment.value();
  }

} // namespace ada
/* end file src/url-getters.cpp */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=url-setters.cpp
/* begin file src/url-setters.cpp */
/**
 * @file url-setters.cpp
 * Includes all the setters of `ada::url`
 */

#include <optional>
#include <string>

namespace ada {

  bool url::set_username(const std::string_view input) {
    if (cannot_have_credentials_or_port()) { return false; }
    username = ada::unicode::percent_encode(input, character_sets::USERINFO_PERCENT_ENCODE);
    return true;
  }

  bool url::set_password(const std::string_view input) {
    if (cannot_have_credentials_or_port()) { return false; }
    password = ada::unicode::percent_encode(input, character_sets::USERINFO_PERCENT_ENCODE);
    return true;
  }

  bool url::set_port(const std::string_view input) {
    if (cannot_have_credentials_or_port()) { return false; }
    std::string trimmed(input);
    helpers::remove_ascii_tab_or_newline(trimmed);
    if (trimmed.empty()) { port = std::nullopt; return true; }
    // Input should not start with control characters.
    if (ada::unicode::is_c0_control_or_space(trimmed.front())) { return false; }
    // Input should contain at least one ascii digit.
    if (input.find_first_of("0123456789") == std::string_view::npos) { return false; }

    // Revert changes if parse_port fails.
    std::optional<uint16_t> previous_port = port;
    parse_port(trimmed);
    if (is_valid) { return true; }
    port = previous_port;
    is_valid = true;
    return false;
  }

  void url::set_hash(const std::string_view input) {
    if (input.empty()) {
      fragment = std::nullopt;
      helpers::strip_trailing_spaces_from_opaque_path(*this);
      return;
    }

    std::string new_value;
    new_value = input[0] == '#' ? input.substr(1) : input;
    helpers::remove_ascii_tab_or_newline(new_value);
    fragment = unicode::percent_encode(new_value, ada::character_sets::FRAGMENT_PERCENT_ENCODE);
    return;
  }

  void url::set_search(const std::string_view input) {
    if (input.empty()) {
      query = std::nullopt;
      helpers::strip_trailing_spaces_from_opaque_path(*this);
      return;
    }

    std::string new_value;
    new_value = input[0] == '?' ? input.substr(1) : input;
    helpers::remove_ascii_tab_or_newline(new_value);

    auto query_percent_encode_set = is_special() ?
      ada::character_sets::SPECIAL_QUERY_PERCENT_ENCODE :
      ada::character_sets::QUERY_PERCENT_ENCODE;

    query = ada::unicode::percent_encode(std::string_view(new_value), query_percent_encode_set);
  }

  bool url::set_pathname(const std::string_view input) {
    if (has_opaque_path) { return false; }
    path = "";
    return parse_path(input);
  }

  bool url::set_host_or_hostname(const std::string_view input, bool override_hostname) {
    if (has_opaque_path) { return false; }

    std::optional<std::string> previous_host = host;
    std::optional<uint16_t> previous_port = port;

    size_t host_end_pos = input.find('#');
    std::string _host(input.data(), host_end_pos != std::string_view::npos ? host_end_pos : input.size());
    helpers::remove_ascii_tab_or_newline(_host);
    std::string_view new_host(_host);

    // If url's scheme is "file", then set state to file host state, instead of host state.
    if (get_scheme_type() != ada::scheme::type::FILE) {
      std::string_view host_view(_host.data(), _host.length());
      auto [location,found_colon] = helpers::get_host_delimiter_location(is_special(), host_view);

      // Otherwise, if c is U+003A (:) and insideBrackets is false, then:
      // Note: the 'found_colon' value is true if and only if a colon was encountered
      // while not inside brackets.
      if (found_colon) {
        if (override_hostname) { return false; }
        std::string_view  buffer = new_host.substr(location+1);
        if (!buffer.empty()) { set_port(buffer); }
      }
      // If url is special and host_view is the empty string, validation error, return failure.
      // Otherwise, if state override is given, host_view is the empty string,
      // and either url includes credentials or url’s port is non-null, return.
      else if (host_view.empty() && (is_special() || includes_credentials() || port.has_value())) {
        return false;
      }

      // Let host be the result of host parsing host_view with url is not special.
      if (host_view.empty()) {
        host = "";
        return true;
      }

      bool succeeded = parse_host(host_view);
      if (!succeeded) {
        host = previous_host;
        port = previous_port;
      }
      return succeeded;
    }

    size_t location = new_host.find_first_of("/\\?");
    if (location != std::string_view::npos) { new_host.remove_suffix(new_host.length() - location); }

    if (new_host.empty()) {
      // Set url’s host to the empty string.
      host = "";
    }
    else {
      // Let host be the result of host parsing buffer with url is not special.
      if (!parse_host(new_host)) {
        host = previous_host;
        port = previous_port;
        return false;
      }

      // If host is "localhost", then set host to the empty string.
      if (host.has_value() && host.value() == "localhost") {
        host = "";
      }
    }
    return true;
  }

  bool url::set_host(const std::string_view input) {
    return set_host_or_hostname(input, false);
  }

  bool url::set_hostname(const std::string_view input) {
    return set_host_or_hostname(input, true);
  }

  bool url::set_protocol(const std::string_view input) {
    std::string view(input);
    helpers::remove_ascii_tab_or_newline(view);
    if (view.empty()) { return true; }

    // Schemes should start with alpha values.
    if (!checkers::is_alpha(view[0])) { return false; }

    view.append(":");

    std::string::iterator pointer = std::find_if_not(view.begin(), view.end(), unicode::is_alnum_plus);

    if (pointer != view.end() && *pointer == ':') {
      return parse_scheme<true>(std::string_view(view.data(), pointer - view.begin()));
    }
    return false;
  }

  bool url::set_href(const std::string_view input) {
    ada::result out = ada::parse(input);

    if (out) {
      username = out->username;
      password = out->password;
      host = out->host;
      port = out->port;
      path = out->path;
      query = out->query;
      fragment = out->fragment;
      type = out->type;
      non_special_scheme = out->non_special_scheme;
      has_opaque_path = out->has_opaque_path;
    }

    return out.has_value();
  }

} // namespace ada
/* end file src/url-setters.cpp */
// dofile: invoked with prepath=/Users/dlemire/CVS/github/ada/src, filename=parser.cpp
/* begin file src/parser.cpp */

#include <iostream>

#include <optional>
#include <string_view>

namespace ada::parser {

  url parse_url(std::string_view user_input,
                const ada::url* base_url,
                ada::encoding_type encoding) {
    ada_log("ada::parser::parse_url('", user_input,
     "' [", user_input.size()," bytes],", (base_url != nullptr ? base_url->to_string() : "null"),
     ",", ada::to_string(encoding), ")");

    ada::state state = ada::state::SCHEME_START;
    ada::url url = ada::url();

    // If we are provided with an invalid base, or the optional_url was invalid,
    // we must return.
    if(base_url != nullptr) { url.is_valid &= base_url->is_valid; }
    if(!url.is_valid) { return url; }

    std::string tmp_buffer;
    std::string_view internal_input;
    if(unicode::has_tabs_or_newline(user_input)) {
      tmp_buffer = user_input;
      // Optimization opportunity: Instead of copying and then pruning, we could just directly
      // build the string from user_input.
      helpers::remove_ascii_tab_or_newline(tmp_buffer);
      internal_input = tmp_buffer;
    } else {
      internal_input = user_input;
    }

    // Leading and trailing control characters are uncommon and easy to deal with (no performance concern).
    std::string_view url_data = internal_input;
    helpers::trim_c0_whitespace(url_data);

    // Optimization opportunity. Most websites do not have fragment.
    std::optional<std::string_view> fragment = helpers::prune_fragment(url_data);
    if(fragment.has_value()) {
      url.fragment = unicode::percent_encode(*fragment,
                                             ada::character_sets::FRAGMENT_PERCENT_ENCODE);
    }

    // Here url_data no longer has its fragment.
    // We are going to access the data from url_data (it is immutable).
    // At any given time, we are pointing at byte 'input_position' in url_data.
    // The input_position variable should range from 0 to input_size.
    // It is illegal to access url_data at input_size.
    size_t input_position = 0;
    const size_t input_size = url_data.size();
    // Keep running the following state machine by switching on state.
    // If after a run pointer points to the EOF code point, go to the next step.
    // Otherwise, increase pointer by 1 and continue with the state machine.
    // We never decrement input_position.
    while(input_position <= input_size) {
      switch (state) {
        case ada::state::SCHEME_START: {
          ada_log("SCHEME_START ", helpers::substring(url_data, input_position));
          // If c is an ASCII alpha, append c, lowercased, to buffer, and set state to scheme state.
          if ((input_position != input_size) && checkers::is_alpha(url_data[input_position])) {
            state = ada::state::SCHEME;
            input_position++;
          } else {
            // Otherwise, if state override is not given, set state to no scheme state and decrease pointer by 1.
            state = ada::state::NO_SCHEME;
          }
          break;
        }
        case ada::state::SCHEME: {
          ada_log("SCHEME ", helpers::substring(url_data, input_position));
          // If c is an ASCII alphanumeric, U+002B (+), U+002D (-), or U+002E (.), append c, lowercased, to buffer.
          while((input_position != input_size) && (ada::unicode::is_alnum_plus(url_data[input_position]))) {
            input_position++;
          }
          // Otherwise, if c is U+003A (:), then:
          if ((input_position != input_size) && (url_data[input_position] == ':')) {
            ada_log("SCHEME the scheme should be ", url_data.substr(0,input_position));
            if(!url.parse_scheme(url_data.substr(0,input_position))) { return url; }
            ada_log("SCHEME the scheme is ", url.get_scheme());

            // If url’s scheme is "file", then:
            if (url.get_scheme_type() == ada::scheme::type::FILE) {
              // Set state to file state.
              state = ada::state::FILE;
            }
            // Otherwise, if url is special, base is non-null, and base’s scheme is url’s scheme:
            // Note: Doing base_url->scheme is unsafe if base_url != nullptr is false.
            else if (url.is_special() && base_url != nullptr && base_url->get_scheme_type() == url.get_scheme_type()) {
              // Set state to special relative or authority state.
              state = ada::state::SPECIAL_RELATIVE_OR_AUTHORITY;
            }
            // Otherwise, if url is special, set state to special authority slashes state.
            else if (url.is_special()) {
              state = ada::state::SPECIAL_AUTHORITY_SLASHES;
            }
            // Otherwise, if remaining starts with an U+002F (/), set state to path or authority state
            // and increase pointer by 1.
            else if (input_position + 1 < input_size && url_data[input_position + 1] == '/') {
              state = ada::state::PATH_OR_AUTHORITY;
              input_position++;
            }
            // Otherwise, set url’s path to the empty string and set state to opaque path state.
            else {
              state = ada::state::OPAQUE_PATH;
            }
          }
          // Otherwise, if state override is not given, set buffer to the empty string, state to no scheme state,
          // and start over (from the first code point in input).
          else {
            state = ada::state::NO_SCHEME;
            input_position = 0;
            break;
          }
          input_position++;
          break;
        }
        case ada::state::NO_SCHEME: {
          ada_log("NO_SCHEME ", helpers::substring(url_data, input_position));
          // If base is null, or base has an opaque path and c is not U+0023 (#), validation error, return failure.
          // SCHEME state updates the state to NO_SCHEME and validates url_data is not empty.
          if (base_url == nullptr || (base_url->has_opaque_path && url_data[input_position] != '#')) {
            ada_log("NO_SCHEME validation error");
            url.is_valid = false;
            return url;
          }
          // Otherwise, if base has an opaque path and c is U+0023 (#),
          // set url’s scheme to base’s scheme, url’s path to base’s path, url’s query to base’s query,
          // url’s fragment to the empty string, and set state to fragment state.
          else if (base_url->has_opaque_path && url.fragment.has_value() && input_position == input_size) {
            ada_log("NO_SCHEME opaque base with fragment");
            url.copy_scheme(*base_url);
            url.path = base_url->path;
            url.has_opaque_path = base_url->has_opaque_path;
            url.query = base_url->query;
            return url;
          }
          // Otherwise, if base’s scheme is not "file", set state to relative state and decrease pointer by 1.
          else if (base_url->get_scheme_type() != ada::scheme::type::FILE) {
            ada_log("NO_SCHEME non-file relative path");
            state = ada::state::RELATIVE_SCHEME;
          }
          // Otherwise, set state to file state and decrease pointer by 1.
          else {
            ada_log("NO_SCHEME file base type");
            state = ada::state::FILE;
          }
          break;
        }
        case ada::state::AUTHORITY: {
          ada_log("AUTHORITY ", helpers::substring(url_data, input_position));
          // most URLs have no @. Having no @ tells us that we don't have to worry about AUTHORITY. Of course,
          // we could have @ and still not have to worry about AUTHORITY.
          // TODO: Instead of just collecting a bool, collect the location of the '@' and do something useful with it.
          // TODO: We could do various processing early on, using a single pass over the string to collect
          // information about it, e.g., telling us whether there is a @ and if so, where (or how many).
          const bool contains_ampersand = (url_data.find('@', input_position) != std::string_view::npos);

          if(!contains_ampersand) {
            state = ada::state::HOST;
            break;
          }
          bool at_sign_seen{false};
          bool password_token_seen{false};
          do {
            std::string_view view = helpers::substring(url_data, input_position);
            size_t location = url.is_special() ? helpers::find_authority_delimiter_special(view) : helpers::find_authority_delimiter(view);
            std::string_view authority_view(view.data(), location);
            size_t end_of_authority = input_position + authority_view.size();
            // If c is U+0040 (@), then:
            if ((end_of_authority != input_size) && (url_data[end_of_authority] == '@')) {
              // If atSignSeen is true, then prepend "%40" to buffer.
              if (at_sign_seen) {
                if (password_token_seen) {
                  url.password += "%40";
                } else {
                  url.username += "%40";
                }
              }

              at_sign_seen = true;

              if (!password_token_seen) {
                size_t password_token_location = authority_view.find(':');
                password_token_seen = password_token_location != std::string_view::npos;

                if (!password_token_seen) {
                  url.username += unicode::percent_encode(authority_view, character_sets::USERINFO_PERCENT_ENCODE);
                } else {
                  url.username += unicode::percent_encode(authority_view.substr(0,password_token_location), character_sets::USERINFO_PERCENT_ENCODE);
                  url.password += unicode::percent_encode(authority_view.substr(password_token_location+1), character_sets::USERINFO_PERCENT_ENCODE);
                }
              }
              else {
                url.password += unicode::percent_encode(authority_view, character_sets::USERINFO_PERCENT_ENCODE);
              }
            }
            // Otherwise, if one of the following is true:
            // - c is the EOF code point, U+002F (/), U+003F (?), or U+0023 (#)
            // - url is special and c is U+005C (\)
            else if (end_of_authority == input_size || url_data[end_of_authority] == '/' || url_data[end_of_authority] == '?' || (url.is_special() && url_data[end_of_authority] == '\\')) {
              // If atSignSeen is true and authority_view is the empty string, validation error, return failure.
              if (at_sign_seen && authority_view.empty()) {
                url.is_valid = false;
                return url;
              }
              state = ada::state::HOST;
              break;
            }
            if(end_of_authority == input_size) { return url; }
            input_position = end_of_authority + 1;
          } while(true);

          break;
        }
        case ada::state::SPECIAL_RELATIVE_OR_AUTHORITY: {
          ada_log("SPECIAL_RELATIVE_OR_AUTHORITY ", helpers::substring(url_data, input_position));

          // If c is U+002F (/) and remaining starts with U+002F (/),
          // then set state to special authority ignore slashes state and increase pointer by 1.
          std::string_view view  = helpers::substring(url_data, input_position);
          if (ada::checkers::begins_with(view, "//")) {
            state = ada::state::SPECIAL_AUTHORITY_IGNORE_SLASHES;
            input_position += 2;
          } else {
            // Otherwise, validation error, set state to relative state and decrease pointer by 1.
            state = ada::state::RELATIVE_SCHEME;
          }

          break;
        }
        case ada::state::PATH_OR_AUTHORITY: {
          ada_log("PATH_OR_AUTHORITY ", helpers::substring(url_data, input_position));

          // If c is U+002F (/), then set state to authority state.
          if ((input_position != input_size) && (url_data[input_position] == '/')) {
            state = ada::state::AUTHORITY;
            input_position++;
          } else {
            // Otherwise, set state to path state, and decrease pointer by 1.
            state = ada::state::PATH;
          }

          break;
        }
        case ada::state::RELATIVE_SCHEME: {
          ada_log("RELATIVE_SCHEME ", helpers::substring(url_data, input_position));

          // Set url’s scheme to base’s scheme.
          url.copy_scheme(*base_url);

          // If c is U+002F (/), then set state to relative slash state.
          if ((input_position != input_size) && (url_data[input_position] == '/')) {
            ada_log("RELATIVE_SCHEME if c is U+002F (/), then set state to relative slash state");
            state = ada::state::RELATIVE_SLASH;
          } else if (url.is_special() && (input_position != input_size) && (url_data[input_position] == '\\')) {
            // Otherwise, if url is special and c is U+005C (\), validation error, set state to relative slash state.
            ada_log("RELATIVE_SCHEME  if url is special and c is U+005C, validation error, set state to relative slash state");
            state = ada::state::RELATIVE_SLASH;
          } else {
            ada_log("RELATIVE_SCHEME otherwise");
            // Set url’s username to base’s username, url’s password to base’s password, url’s host to base’s host,
            // url’s port to base’s port, url’s path to a clone of base’s path, and url’s query to base’s query.
            url.username = base_url->username;
            url.password = base_url->password;
            url.host = base_url->host;
            url.port = base_url->port;
            url.path = base_url->path;
            url.has_opaque_path = base_url->has_opaque_path;
            url.query = base_url->query;

            // If c is U+003F (?), then set url’s query to the empty string, and state to query state.
            if ((input_position != input_size) && (url_data[input_position] == '?')) {
              state = ada::state::QUERY;
            }
            // Otherwise, if c is not the EOF code point:
            else if (input_position != input_size) {
              // Set url’s query to null.
              url.query = std::nullopt;

              // Shorten url’s path.
              helpers::shorten_path(url.path, url.get_scheme_type());

              // Set state to path state and decrease pointer by 1.
              state = ada::state::PATH;
              break;
            }
          }
          input_position++;
          break;
        }
        case ada::state::RELATIVE_SLASH: {
          ada_log("RELATIVE_SLASH ", helpers::substring(url_data, input_position));

          // If url is special and c is U+002F (/) or U+005C (\), then:
          if (url.is_special() && (input_position != input_size) && (url_data[input_position] == '/' || url_data[input_position] =='\\')) {
            // Set state to special authority ignore slashes state.
            state = ada::state::SPECIAL_AUTHORITY_IGNORE_SLASHES;
          }
          // Otherwise, if c is U+002F (/), then set state to authority state.
          else if ((input_position != input_size) && (url_data[input_position] == '/')) {
            state = ada::state::AUTHORITY;
          }
          // Otherwise, set
          // - url’s username to base’s username,
          // - url’s password to base’s password,
          // - url’s host to base’s host,
          // - url’s port to base’s port,
          // - state to path state, and then, decrease pointer by 1.
          else {
            url.username = base_url->username;
            url.password = base_url->password;
            url.host = base_url->host;
            url.port = base_url->port;
            state = ada::state::PATH;
            break;
          }

          input_position++;
          break;
        }
        case ada::state::SPECIAL_AUTHORITY_SLASHES: {
          ada_log("SPECIAL_AUTHORITY_SLASHES ", helpers::substring(url_data, input_position));

          // If c is U+002F (/) and remaining starts with U+002F (/),
          // then set state to special authority ignore slashes state and increase pointer by 1.
          state = ada::state::SPECIAL_AUTHORITY_IGNORE_SLASHES;
          std::string_view view = helpers::substring(url_data, input_position);
          if (ada::checkers::begins_with(view, "//")) {
            input_position += 2;
          }

          [[fallthrough]];
        }
        case ada::state::SPECIAL_AUTHORITY_IGNORE_SLASHES: {
          ada_log("SPECIAL_AUTHORITY_IGNORE_SLASHES ", helpers::substring(url_data, input_position));

          // If c is neither U+002F (/) nor U+005C (\), then set state to authority state and decrease pointer by 1.
          while ((input_position != input_size) && ((url_data[input_position] == '/') || (url_data[input_position] == '\\'))) {
            input_position++;
          }
          state = ada::state::AUTHORITY;

          break;
        }
        case ada::state::QUERY: {
          ada_log("QUERY ", helpers::substring(url_data, input_position));
          // If encoding is not UTF-8 and one of the following is true:
          // - url is not special
          // - url’s scheme is "ws" or "wss"
          if (encoding != ada::encoding_type::UTF8) {
            if (!url.is_special() || url.get_scheme_type() == ada::scheme::type::WS || url.get_scheme_type() == ada::scheme::type::WSS) {
              // then set encoding to UTF-8.
              encoding = ada::encoding_type::UTF8;
            }
          }
          // Let queryPercentEncodeSet be the special-query percent-encode set if url is special;
          // otherwise the query percent-encode set.
          auto query_percent_encode_set = url.is_special() ?
                                ada::character_sets::SPECIAL_QUERY_PERCENT_ENCODE :
                                ada::character_sets::QUERY_PERCENT_ENCODE;

          // Percent-encode after encoding, with encoding, buffer, and queryPercentEncodeSet,
          // and append the result to url’s query.
          url.query = ada::unicode::percent_encode(helpers::substring(url_data, input_position), query_percent_encode_set);

          return url;
        }
        case ada::state::HOST: {
          ada_log("HOST ", helpers::substring(url_data, input_position));

          std::string_view host_view = helpers::substring(url_data, input_position);
          auto [location, found_colon] = helpers::get_host_delimiter_location(url.is_special(), host_view);
          input_position = (location != std::string_view::npos) ? input_position + location : input_size;
          // Otherwise, if c is U+003A (:) and insideBrackets is false, then:
          // Note: the 'found_colon' value is true if and only if a colon was encountered
          // while not inside brackets.
          if (found_colon) {
            // If buffer is the empty string, validation error, return failure.
            // Let host be the result of host parsing buffer with url is not special.
            ada_log("HOST parsing ", host_view);
            if(!url.parse_host(host_view)) { return url; }
            ada_log("HOST parsing results in ", url.host.has_value() ? "none" : url.host.value());
            // Set url’s host to host, buffer to the empty string, and state to port state.
            state = ada::state::PORT;
            input_position++;
          }
          // Otherwise, if one of the following is true:
          // - c is the EOF code point, U+002F (/), U+003F (?), or U+0023 (#)
          // - url is special and c is U+005C (\)
          // The get_host_delimiter_location function either brings us to
          // the colon outside of the bracket, or to one of those characters.
          else {

            // If url is special and host_view is the empty string, validation error, return failure.
            if (url.is_special() && host_view.empty()) {
              url.is_valid = false;
              return url;
            }

            // Let host be the result of host parsing host_view with url is not special.
            if (host_view.empty()) {
              url.host = "";
            } else {
              if(!url.parse_host(host_view)) { return url; }
            }
            // Set url’s host to host, and state to path start state.
            state = ada::state::PATH_START;
          }

          break;
        }
        case ada::state::OPAQUE_PATH: {
          ada_log("OPAQUE_PATH ", helpers::substring(url_data, input_position));
          std::string_view view = helpers::substring(url_data, input_position);
          // If c is U+003F (?), then set url’s query to the empty string and state to query state.
          size_t location = view.find('?');
          if(location != std::string_view::npos) {
            view.remove_suffix(view.size() - location);
            state = ada::state::QUERY;
            input_position += location + 1;
          } else {
            input_position = input_size + 1;
          }
          url.has_opaque_path = true;
          url.path = unicode::percent_encode(view, character_sets::C0_CONTROL_PERCENT_ENCODE);
          break;
        }
        case ada::state::PORT: {
          ada_log("PORT ", helpers::substring(url_data, input_position));
          std::string_view port_view = helpers::substring(url_data, input_position);
          size_t consumed_bytes = url.parse_port(port_view, true);
          input_position += consumed_bytes;
          if(!url.is_valid) { return url; }
          state = state::PATH_START;
          [[fallthrough]];
        }
        case ada::state::PATH_START: {
          ada_log("PATH_START ", helpers::substring(url_data, input_position));

          // If url is special, then:
          if (url.is_special()) {
            // Set state to path state.
            state = ada::state::PATH;

            // Optimization: Avoiding going into PATH state improves the performance of urls ending with /.
            if (input_position == input_size) {
              url.path = "/";
              return url;
            }
            // If c is neither U+002F (/) nor U+005C (\), then decrease pointer by 1.
            // We know that (input_position == input_size) is impossible here, because of the previous if-check.
            if ((url_data[input_position] != '/') && (url_data[input_position] != '\\')) {
              break;
            }
          }
          // Otherwise, if state override is not given and c is U+003F (?),
          // set url’s query to the empty string and state to query state.
          else if ((input_position != input_size) && (url_data[input_position] == '?')) {
            state = ada::state::QUERY;
          }
          // Otherwise, if c is not the EOF code point:
          else if (input_position != input_size) {
            // Set state to path state.
            state = ada::state::PATH;

            // If c is not U+002F (/), then decrease pointer by 1.
            if (url_data[input_position] != '/') {
              break;
            }
          }

          input_position++;
          break;
        }
        case ada::state::PATH: {
          std::string_view view = helpers::substring(url_data, input_position);
          ada_log("PATH ", helpers::substring(url_data, input_position));

          // Most time, we do not need percent encoding.
          // Furthermore, we can immediately locate the '?'.
          size_t locofquestionmark = view.find('?');
          if(locofquestionmark != std::string_view::npos) {
            state = ada::state::QUERY;
            view.remove_suffix(view.size()-locofquestionmark);
            input_position += locofquestionmark + 1;
          } else {
            input_position = input_size + 1;
          }
          if(!helpers::parse_prepared_path(view, url.get_scheme_type(), url.path)) { return url; }
          break;
        }
        case ada::state::FILE_SLASH: {
          ada_log("FILE_SLASH ", helpers::substring(url_data, input_position));

          // If c is U+002F (/) or U+005C (\), then:
          if ((input_position != input_size) && (url_data[input_position] == '/' || url_data[input_position] == '\\')) {
            ada_log("FILE_SLASH c is U+002F or U+005C");
            // Set state to file host state.
            state = ada::state::FILE_HOST;
            input_position++;
          } else {
            ada_log("FILE_SLASH otherwise");
            // If base is non-null and base’s scheme is "file", then:
            // Note: it is unsafe to do base_url->scheme unless you know that
            // base_url_has_value() is true.
            if (base_url != nullptr && base_url->get_scheme_type() == ada::scheme::type::FILE) {
              // Set url’s host to base’s host.
              url.host = base_url->host;

              // If the code point substring from pointer to the end of input does not start with
              // a Windows drive letter and base’s path[0] is a normalized Windows drive letter,
              // then append base’s path[0] to url’s path.
              if (!base_url->path.empty()) {
                if (!checkers::is_windows_drive_letter(helpers::substring(url_data, input_position))) {
                  std::string_view first_base_url_path = base_url->path;
                  first_base_url_path.remove_prefix(1);
                  size_t loc = first_base_url_path.find('/');
                  if(loc != std::string_view::npos) {
                    first_base_url_path.remove_suffix(first_base_url_path.size() - loc);
                  }
                  if (checkers::is_normalized_windows_drive_letter(first_base_url_path)) {
                    url.path += '/';
                    url.path += first_base_url_path;
                  }
                }
              }
            }

            // Set state to path state, and decrease pointer by 1.
            state = ada::state::PATH;
          }

          break;
        }
        case ada::state::FILE_HOST: {
          std::string_view view = helpers::substring(url_data, input_position);
          ada_log("FILE_HOST ", helpers::substring(url_data, input_position));

          size_t location = view.find_first_of("/\\?");
          std::string_view file_host_buffer(view.data(), (location != std::string_view::npos) ? location : view.size());

          if (checkers::is_windows_drive_letter(file_host_buffer)) {
            state = ada::state::PATH;
          } else if (file_host_buffer.empty()) {
            // Set url’s host to the empty string.
            url.host = "";
            // Set state to path start state.
            state = ada::state::PATH_START;
          } else {
            size_t consumed_bytes = file_host_buffer.size();
            input_position += consumed_bytes;
            // Let host be the result of host parsing buffer with url is not special.
            if(!url.parse_host(file_host_buffer)) { return url; }

            // If host is "localhost", then set host to the empty string.
            if (url.host.has_value() && url.host.value() == "localhost") {
              url.host = "";
            }

            // Set buffer to the empty string and state to path start state.
            state = ada::state::PATH_START;
          }

          break;
        }
        case ada::state::FILE: {
          ada_log("FILE ", helpers::substring(url_data, input_position));
          std::string_view file_view = helpers::substring(url_data, input_position);

          // Set url’s scheme to "file".
          url.set_scheme("file");

          // Set url’s host to the empty string.
          url.host = "";

          // If c is U+002F (/) or U+005C (\), then:
          if (input_position != input_size && (url_data[input_position] == '/' || url_data[input_position] == '\\')) {
            ada_log("FILE c is U+002F or U+005C");
            // Set state to file slash state.
            state = ada::state::FILE_SLASH;
          }
          // Otherwise, if base is non-null and base’s scheme is "file":
          else if (base_url != nullptr && base_url->get_scheme_type() == ada::scheme::type::FILE) {
            // Set url’s host to base’s host, url’s path to a clone of base’s path, and url’s query to base’s query.
            ada_log("FILE base non-null");
            url.host = base_url->host;
            url.path = base_url->path;
            url.has_opaque_path = base_url->has_opaque_path;
            url.query = base_url->query;

            // If c is U+003F (?), then set url’s query to the empty string and state to query state.
            if (input_position != input_size && url_data[input_position] == '?') {
              state = ada::state::QUERY;
            }
            // Otherwise, if c is not the EOF code point:
            else if (input_position != input_size) {
              // Set url’s query to null.
              url.query = std::nullopt;

              // If the code point substring from pointer to the end of input does not start with a
              // Windows drive letter, then shorten url’s path.
              if (!checkers::is_windows_drive_letter(file_view)) {
                helpers::shorten_path(url.path, url.get_scheme_type());
              }
              // Otherwise:
              else {
                // Set url’s path to an empty list.
                url.path.clear();
                url.has_opaque_path = true;
              }

              // Set state to path state and decrease pointer by 1.
              state = ada::state::PATH;
              break;
            }
          }
          // Otherwise, set state to path state, and decrease pointer by 1.
          else {
            ada_log("FILE go to path");
            state = ada::state::PATH;
            break;
          }

          input_position++;
          break;
        }
        default:
          ada::unreachable();
      }
    }
    ada_log("returning ", url.to_string());
    return url;
  }

} // namespace ada::parser
/* end file src/parser.cpp */
/* end file src/ada.cpp */
