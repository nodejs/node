// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_URL_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_URL_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace v8 {
namespace mock_dom {

// A parsed hierarchical URL. This covers what pages and client-side routers
// do with URLs (http(s), file, about, data, blob), not the whole URL
// standard: no IDNA, no IPv6 normalisation, no percent-encode sets beyond the
// basics.
struct ParsedURL {
  bool valid = false;
  std::string scheme;  // without ':'
  std::string username;
  std::string password;
  std::string hostname;
  std::string port;
  std::string pathname;  // with leading '/', or opaque path
  std::string query;     // without '?'
  std::string fragment;  // without '#'
  bool has_authority = false;

  static ParsedURL Parse(std::string_view input);
  // Resolves |input| against |base| as the URL parser would.
  static ParsedURL Resolve(std::string_view input, const ParsedURL& base);

  std::string Href() const;
  std::string Origin() const;
  std::string Host() const;
  std::string Protocol() const { return scheme + ":"; }
  std::string Search() const { return query.empty() ? "" : "?" + query; }
  std::string Hash() const { return fragment.empty() ? "" : "#" + fragment; }
};

// application/x-www-form-urlencoded helpers for URLSearchParams.
std::vector<std::pair<std::string, std::string>> ParseQuery(
    std::string_view query);
std::string SerializeQuery(
    const std::vector<std::pair<std::string, std::string>>& pairs);
std::string PercentDecode(std::string_view input, bool plus_as_space);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_URL_H_
