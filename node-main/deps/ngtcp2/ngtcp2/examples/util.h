/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2012 nghttp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef UTIL_H
#define UTIL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <sys/socket.h>

#include <cassert>
#include <optional>
#include <string>
#include <random>
#include <unordered_map>
#include <string_view>
#include <span>

#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>

#include <urlparse.h>

#include "network.h"
#include "siphash.h"
#include "template.h"

namespace ngtcp2 {

namespace util {

inline nghttp3_nv make_nv(const std::string_view &name,
                          const std::string_view &value, uint8_t flags) {
  return nghttp3_nv{
    reinterpret_cast<uint8_t *>(const_cast<char *>(std::ranges::data(name))),
    reinterpret_cast<uint8_t *>(const_cast<char *>(std::ranges::data(value))),
    name.size(),
    value.size(),
    flags,
  };
}

inline nghttp3_nv make_nv_cc(const std::string_view &name,
                             const std::string_view &value) {
  return make_nv(name, value, NGHTTP3_NV_FLAG_NONE);
}

inline nghttp3_nv make_nv_nc(const std::string_view &name,
                             const std::string_view &value) {
  return make_nv(name, value, NGHTTP3_NV_FLAG_NO_COPY_NAME);
}

inline nghttp3_nv make_nv_nn(const std::string_view &name,
                             const std::string_view &value) {
  return make_nv(name, value,
                 NGHTTP3_NV_FLAG_NO_COPY_NAME | NGHTTP3_NV_FLAG_NO_COPY_VALUE);
}

constinit const auto hexdigits = []() {
  constexpr char LOWER_XDIGITS[] = "0123456789abcdef";

  std::array<char, 512> tbl;

  for (size_t i = 0; i < 256; ++i) {
    tbl[i * 2] = LOWER_XDIGITS[static_cast<size_t>(i >> 4)];
    tbl[i * 2 + 1] = LOWER_XDIGITS[static_cast<size_t>(i & 0xf)];
  }

  return tbl;
}();

// format_hex converts a range [|first|, |last|) in hex format, and
// stores the result in another range, beginning at |result|.  It
// returns an output iterator to the element past the last element
// stored.
template <std::input_iterator I, std::weakly_incrementable O>
requires(std::indirectly_writable<O, char> &&
         sizeof(std::iter_value_t<I>) == sizeof(uint8_t))
constexpr O format_hex(I first, I last, O result) {
  for (; first != last; ++first) {
    result = std::ranges::copy_n(
               hexdigits.data() + static_cast<uint8_t>(*first) * 2, 2, result)
               .out;
  }

  return result;
}

// format_hex converts a range [|first|, |first| + |n|) in hex format,
// and stores the result in another range, beginning at |result|.  It
// returns an output iterator to the element past the last element
// stored.
template <std::input_iterator I, std::weakly_incrementable O>
requires(std::indirectly_writable<O, char> &&
         sizeof(std::iter_value_t<I>) == sizeof(uint8_t))
constexpr O format_hex(I first, std::iter_difference_t<I> n, O result) {
  return format_hex(first, std::ranges::next(first, n), std::move(result));
}

// format_hex converts a range [|first|, |first| + |n|) in hex format,
// and returns it.
template <std::input_iterator I>
requires(sizeof(std::iter_value_t<I>) == sizeof(uint8_t))
constexpr std::string format_hex(I first, std::iter_difference_t<I> n) {
  if (n <= 0) {
    return {};
  }

  std::string res;

  res.resize(as_unsigned(n * 2));

  format_hex(std::move(first), std::move(n), std::ranges::begin(res));

  return res;
}

// format_hex converts |R| in hex format, and stores the result in
// another range, beginning at |result|.  It returns an output
// iterator to the element past the last element stored.
template <std::ranges::input_range R, std::weakly_incrementable O>
requires(std::indirectly_writable<O, char> &&
         !std::is_array_v<std::remove_cvref_t<R>> &&
         sizeof(std::ranges::range_value_t<R>) == sizeof(uint8_t))
constexpr O format_hex(R &&r, O result) {
  return format_hex(std::ranges::begin(r), std::ranges::end(r),
                    std::move(result));
}

// format_hex converts |R| in hex format, and returns the result.
template <std::ranges::input_range R>
requires(!std::is_array_v<std::remove_cvref_t<R>> &&
         sizeof(std::ranges::range_value_t<R>) == sizeof(uint8_t))
constexpr std::string format_hex(R &&r) {
  std::string res;

  res.resize(as_unsigned(std::ranges::distance(r) * 2));

  format_hex(std::forward<R>(r), std::ranges::begin(res));

  return res;
}

// format_hex converts |n| in hex format, and stores the result in
// another range, beginning at |result|.  It returns an output
// iterator to the element past the last element stored.
template <std::unsigned_integral T, std::weakly_incrementable O>
requires(std::indirectly_writable<O, char>)
constexpr O format_hex(T n, O result) {
  if constexpr (sizeof(n) == 1) {
    return std::ranges::copy_n(hexdigits.data() + n * 2, 2, result).out;
  }

  if constexpr (std::endian::native == std::endian::little) {
    auto end = reinterpret_cast<uint8_t *>(&n);
    auto p = end + sizeof(n);

    for (; p != end; --p) {
      result =
        std::ranges::copy_n(hexdigits.data() + *(p - 1) * 2, 2, result).out;
    }
  } else {
    auto p = reinterpret_cast<uint8_t *>(&n);
    auto end = p + sizeof(n);

    for (; p != end; ++p) {
      result = std::ranges::copy_n(hexdigits.data() + *p * 2, 2, result).out;
    }
  }

  return result;
}

// format_hex converts |n| in hex format, and returns it.
template <std::unsigned_integral T> constexpr std::string format_hex(T n) {
  std::string res;

  res.resize(sizeof(n) * 2);

  format_hex(std::move(n), std::ranges::begin(res));

  return res;
}

std::string decode_hex(const std::string_view &s);

// format_durationf formats |ns| in human readable manner.  |ns| must
// be nanoseconds resolution.  This function uses the largest unit so
// that the integral part is strictly more than zero, and the
// precision is at most 2 digits.  For example, 1234 is formatted as
// "1.23us".  The largest unit is seconds.
std::string format_durationf(uint64_t ns);

std::mt19937 make_mt19937();

ngtcp2_tstamp timestamp();

// system_clock_now returns the current timestamp of system clock.
ngtcp2_tstamp system_clock_now();

bool numeric_host(const char *hostname);

bool numeric_host(const char *hostname, int family);

// hexdump dumps |data| of length |datalen| in the format similar to
// hexdump(1) with -C option.  This function returns 0 if it succeeds,
// or -1.
int hexdump(FILE *out, const void *data, size_t datalen);

static constexpr uint8_t lowcase_tbl[] = {
  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
  60,  61,  62,  63,  64,  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
  'z', 91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104,
  105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
  120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
  135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
  150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
  165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
  180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
  195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
  210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
  225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
  240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
  255,
};

constexpr char lowcase(char c) noexcept {
  return as_signed(lowcase_tbl[static_cast<uint8_t>(c)]);
}

struct CaseCmp {
  constexpr bool operator()(char lhs, char rhs) const noexcept {
    return lowcase(lhs) == lowcase(rhs);
  }
};

// istarts_with returns true if |s| starts with |prefix|.  Comparison
// is performed in case-insensitive manner.
constexpr bool istarts_with(const std::string_view &s,
                            const std::string_view &prefix) {
  return s.size() >= prefix.size() &&
         std::ranges::equal(s.substr(0, prefix.size()), prefix, CaseCmp());
}

// make_cid_key returns the key for |cid|.
std::string_view make_cid_key(const ngtcp2_cid *cid);
ngtcp2_cid make_cid_key(std::span<const uint8_t> cid);

// straddr stringifies |sa| of length |salen| in a format "[IP]:PORT".
std::string straddr(const sockaddr *sa, socklen_t salen);

// port returns port from |su|.
uint16_t port(const sockaddr_union *su);

// prohibited_port returns true if |port| is prohibited as a client
// port.
bool prohibited_port(uint16_t port);

// strccalgo stringifies |cc_algo|.
std::string_view strccalgo(ngtcp2_cc_algo cc_algo);

// read_mime_types reads "MIME media types and the extensions" file
// denoted by |filename| and returns the mapping of extension to MIME
// media type.
std::optional<std::unordered_map<std::string, std::string>>
read_mime_types(const std::string_view &filename);

constinit const auto count_digit_tbl = []() {
  std::array<uint64_t, std::numeric_limits<uint64_t>::digits10> tbl;

  uint64_t x = 1;

  for (size_t i = 0; i < tbl.size(); ++i) {
    x *= 10;
    tbl[i] = x - 1;
  }

  return tbl;
}();

// count_digit returns the minimum number of digits to represent |x|
// in base 10.
//
// credit:
// https://lemire.me/blog/2025/01/07/counting-the-digits-of-64-bit-integers/
template <std::unsigned_integral T> constexpr size_t count_digit(T x) {
  auto y = static_cast<size_t>(19 * (std::numeric_limits<T>::digits - 1 -
                                     std::countl_zero(static_cast<T>(x | 1))) >>
                               6);

  y += x > count_digit_tbl[y];

  return y + 1;
}

constinit const auto utos_digits = []() {
  std::array<char, 200> a;

  for (size_t i = 0; i < 100; ++i) {
    a[i * 2] = '0' + static_cast<char>(i / 10);
    a[i * 2 + 1] = '0' + static_cast<char>(i % 10);
  }

  return a;
}();

struct UIntFormatter {
  template <std::unsigned_integral T, std::weakly_incrementable O>
  requires(std::indirectly_writable<O, char>)
  constexpr O operator()(T n, O result) {
    using result_type = std::iter_value_t<O>;

    if (n < 10) {
      *result++ = static_cast<result_type>('0' + static_cast<char>(n));
      return result;
    }

    if (n < 100) {
      return std::ranges::copy_n(utos_digits.data() + n * 2, 2, result).out;
    }

    std::ranges::advance(result, as_signed(count_digit(n)));

    auto p = result;

    for (; n >= 100; n /= 100) {
      std::ranges::advance(p, -2);
      std::ranges::copy_n(utos_digits.data() + (n % 100) * 2, 2, p);
    }

    if (n < 10) {
      *--p = static_cast<result_type>('0' + static_cast<char>(n));
      return result;
    }

    std::ranges::advance(p, -2);
    std::ranges::copy_n(utos_digits.data() + n * 2, 2, p);

    return result;
  }
};

template <std::unsigned_integral T, std::weakly_incrementable O>
requires(std::indirectly_writable<O, char>)
constexpr O utos(T n, O result) {
  return UIntFormatter{}(std::move(n), std::move(result));
}

// format_uint converts |n| into string.
template <std::unsigned_integral T> constexpr std::string format_uint(T n) {
  using namespace std::literals;

  if (n == 0) {
    return "0"s;
  }

  std::string res;

  res.resize(count_digit(n));

  utos(n, std::ranges::begin(res));

  return res;
}

// format_uint_iec converts |n| into string with the IEC unit (either
// "G", "M", or "K").  It chooses the largest unit which does not drop
// precision.
template <std::unsigned_integral T> std::string format_uint_iec(T n) {
  if (n >= (1 << 30) && (n & ((1 << 30) - 1)) == 0) {
    return format_uint(n / (1 << 30)) + 'G';
  }
  if (n >= (1 << 20) && (n & ((1 << 20) - 1)) == 0) {
    return format_uint(n / (1 << 20)) + 'M';
  }
  if (n >= (1 << 10) && (n & ((1 << 10) - 1)) == 0) {
    return format_uint(n / (1 << 10)) + 'K';
  }
  return format_uint(n);
}

// format_duration converts |n| into string with the unit in either
// "h" (hours), "m" (minutes), "s" (seconds), "ms" (milliseconds),
// "us" (microseconds) or "ns" (nanoseconds).  It chooses the largest
// unit which does not drop precision.  |n| is in nanosecond
// resolution.
std::string format_duration(ngtcp2_duration n);

// parse_uint parses |s| as 64-bit unsigned integer.  If it cannot
// parse |s|, the return value does not contain a value.
std::optional<uint64_t> parse_uint(const std::string_view &s);

// parse_uint_iec parses |s| as 64-bit unsigned integer.  It accepts
// IEC unit letter (either "G", "M", or "K") in |s|.  If it cannot
// parse |s|, the return value does not contain a value.
std::optional<uint64_t> parse_uint_iec(const std::string_view &s);

// parse_duration parses |s| as 64-bit unsigned integer.  It accepts a
// unit (either "h", "m", "s", "ms", "us", or "ns") in |s|.  If no
// unit is present, the unit "s" is assumed.  If it cannot parse |s|,
// the return value does not contain a value.
std::optional<uint64_t> parse_duration(const std::string_view &s);

// generate_secure_random generates a cryptographically secure pseudo
// random data of |data|.
int generate_secure_random(std::span<uint8_t> data);

// generate_secret generates secret and writes it to |secret|.
// Currently, |secret| must be 32 bytes long.
int generate_secret(std::span<uint8_t> secret);

// normalize_path removes ".." by consuming a previous path component.
// It also removes ".".  It assumes that |path| starts with "/".  If
// it cannot consume a previous path component, it just removes "..".
std::string normalize_path(const std::string_view &path);

template <std::predicate<size_t> Pred>
constexpr auto pred_tbl_gen256(Pred pred) {
  std::array<bool, 256> tbl;

  for (size_t i = 0; i < tbl.size(); ++i) {
    tbl[i] = pred(i);
  }

  return tbl;
}

constexpr auto digit_pred(size_t i) noexcept { return '0' <= i && i <= '9'; }

constinit const auto is_digit_tbl = pred_tbl_gen256(digit_pred);

constexpr bool is_digit(char c) noexcept {
  return is_digit_tbl[static_cast<uint8_t>(c)];
}

constinit const auto is_hex_digit_tbl = pred_tbl_gen256([](auto i) {
  return digit_pred(i) || ('A' <= i && i <= 'F') || ('a' <= i && i <= 'f');
});

constexpr bool is_hex_digit(char c) noexcept {
  return is_hex_digit_tbl[static_cast<uint8_t>(c)];
}

// is_hex_string returns true if the length of |s| is even, and |s|
// does not contain a character other than [0-9A-Fa-f].  It returns
// false otherwise.
template <std::ranges::input_range R>
requires(!std::is_array_v<std::remove_cvref_t<R>>)
constexpr bool is_hex_string(R &&r) {
  return !(std::ranges::size(r) & 1) && std::ranges::all_of(r, is_hex_digit);
}

constinit const auto hex_to_uint_tbl = []() {
  std::array<uint32_t, 256> tbl;

  std::ranges::fill(tbl, 256);

  for (char i = '0'; i <= '9'; ++i) {
    tbl[static_cast<uint8_t>(i)] = static_cast<uint32_t>(i - '0');
  }

  for (char i = 'A'; i <= 'F'; ++i) {
    tbl[static_cast<uint8_t>(i)] = static_cast<uint32_t>(i - 'A' + 10);
  }

  for (char i = 'a'; i <= 'f'; ++i) {
    tbl[static_cast<uint8_t>(i)] = static_cast<uint32_t>(i - 'a' + 10);
  }

  return tbl;
}();

// hex_to_uint returns integer corresponding to hex notation |c|.  If
// is_hex_digit(c) is false, it returns 256.
constexpr uint32_t hex_to_uint(char c) noexcept {
  return hex_to_uint_tbl[static_cast<uint8_t>(c)];
}

std::string percent_decode(const std::string_view &s);

int make_socket_nonblocking(int fd);

int create_nonblock_socket(int domain, int type, int protocol);

std::optional<std::vector<uint8_t>>
read_token(const std::string_view &filename);
int write_token(const std::string_view &filename,
                std::span<const uint8_t> token);

std::optional<std::vector<uint8_t>>
read_transport_params(const std::string_view &filename);
int write_transport_params(const std::string_view &filename,
                           std::span<const uint8_t> data);

const char *crypto_default_ciphers();

const char *crypto_default_groups();

// split_str parses delimited strings in |s| and returns substrings
// delimited by |delim|.  The any white spaces around substring are
// treated as a part of substring.
std::vector<std::string_view> split_str(const std::string_view &s,
                                        char delim = ',');

// parse_version parses |s| to get 4 byte QUIC version.  |s| must be a
// hex string and must start with "0x" (.e.g, 0x00000001).
std::optional<uint32_t> parse_version(const std::string_view &s);

// read_file reads a file denoted by |path| and returns its content.
std::optional<std::vector<uint8_t>> read_file(const std::string_view &path);

enum HPKEPrivateKeyType : uint16_t {
  HPKE_DHKEM_X25519_HKDF_SHA256 = 0x0020,
};

// HPKEPrivateKey contains HPKE private key.
struct HPKEPrivateKey {
  // type is HPKE private key type.
  HPKEPrivateKeyType type;
  // bytes contains raw private key.
  std::vector<uint8_t> bytes;
};

// ECHServerConfig is a server-side ECH configuration.
struct ECHServerConfig {
  // private_key contains a private key used for decrypting encrypted
  // Client Hello.
  HPKEPrivateKey private_key;
  // ech_config contains a serialized ECHConfig.
  std::vector<uint8_t> ech_config;
};

// read_ech_server_config reads server-side ECH configuration from a
// file denoted by |path|.
std::optional<ECHServerConfig>
read_ech_server_config(const std::string_view &path);

std::span<uint64_t, 2> generate_siphash_key();

// get_string returns a URL component specified by |f| of |uri|.  This
// function assumes that u.field_set & (1 << f) is nonzero.
constexpr std::string_view get_string(const std::string_view &uri,
                                      const urlparse_url &u,
                                      urlparse_url_fields f) {
  assert(u.field_set & (1 << f));

  auto p = &u.field_data[f];
  return {uri.data() + p->off, p->len};
}

} // namespace util

std::ostream &operator<<(std::ostream &os, const ngtcp2_cid &cid);

} // namespace ngtcp2

namespace std {
template <> struct hash<ngtcp2_cid> {
  hash() {
    std::ranges::copy(ngtcp2::util::generate_siphash_key(), key.begin());
  }

  std::size_t operator()(const ngtcp2_cid &cid) const noexcept {
    return static_cast<size_t>(siphash24(key, {cid.data, cid.datalen}));
  }

  std::array<uint64_t, 2> key;
};
} // namespace std

inline bool operator==(const ngtcp2_cid &lhs, const ngtcp2_cid &rhs) {
  return ngtcp2_cid_eq(&lhs, &rhs);
}

#endif // !defined(UTIL_H)
