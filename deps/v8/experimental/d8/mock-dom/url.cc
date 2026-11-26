// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/url.h"

#include <cctype>

namespace v8 {
namespace mock_dom {

namespace {

bool IsSchemeChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '-' ||
         c == '.';
}

std::string Lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

bool IsSpecialScheme(std::string_view scheme) {
  return scheme == "http" || scheme == "https" || scheme == "file" ||
         scheme == "ws" || scheme == "wss" || scheme == "ftp";
}

std::string DefaultPort(std::string_view scheme) {
  if (scheme == "http" || scheme == "ws") return "80";
  if (scheme == "https" || scheme == "wss") return "443";
  if (scheme == "ftp") return "21";
  return "";
}

// Removes "." and ".." segments.
std::string NormalizePath(std::string_view path) {
  std::vector<std::string_view> segments;
  size_t i = 0;
  bool trailing_slash = false;
  while (i <= path.size()) {
    size_t slash = path.find('/', i);
    if (slash == std::string_view::npos) slash = path.size();
    std::string_view segment = path.substr(i, slash - i);
    bool last = slash == path.size();
    if (segment == "..") {
      if (!segments.empty()) segments.pop_back();
      trailing_slash = last;
    } else if (segment == ".") {
      trailing_slash = last;
    } else if (!segment.empty() || !last) {
      if (i != 0 || !segment.empty()) segments.push_back(segment);
      trailing_slash = false;
    } else {
      trailing_slash = true;
    }
    i = slash + 1;
  }
  std::string out;
  for (std::string_view segment : segments) {
    out.push_back('/');
    out.append(segment);
  }
  if (trailing_slash || out.empty()) out.push_back('/');
  return out;
}

void SplitPathQueryFragment(std::string_view rest, std::string* path,
                            std::string* query, std::string* fragment) {
  size_t hash = rest.find('#');
  if (hash != std::string_view::npos) {
    *fragment = std::string(rest.substr(hash + 1));
    rest = rest.substr(0, hash);
  } else {
    fragment->clear();
  }
  size_t question = rest.find('?');
  if (question != std::string_view::npos) {
    *query = std::string(rest.substr(question + 1));
    rest = rest.substr(0, question);
  } else {
    query->clear();
  }
  *path = std::string(rest);
}

std::string_view Trim(std::string_view s) {
  while (!s.empty() && static_cast<unsigned char>(s.front()) <= 0x20) {
    s.remove_prefix(1);
  }
  while (!s.empty() && static_cast<unsigned char>(s.back()) <= 0x20) {
    s.remove_suffix(1);
  }
  return s;
}

size_t SchemeLength(std::string_view input) {
  if (input.empty() || !std::isalpha(static_cast<unsigned char>(input[0]))) {
    return 0;
  }
  for (size_t i = 1; i < input.size(); ++i) {
    if (input[i] == ':') return i;
    if (!IsSchemeChar(input[i])) return 0;
  }
  return 0;
}

}  // namespace

ParsedURL ParsedURL::Parse(std::string_view input) {
  ParsedURL url;
  input = Trim(input);
  size_t scheme_length = SchemeLength(input);
  if (scheme_length == 0) return url;
  url.scheme = Lower(input.substr(0, scheme_length));
  std::string_view rest = input.substr(scheme_length + 1);
  if (rest.substr(0, 2) == "//") {
    url.has_authority = true;
    rest.remove_prefix(2);
    size_t end = rest.find_first_of("/?#");
    std::string_view authority = rest.substr(0, end);
    rest =
        end == std::string_view::npos ? std::string_view() : rest.substr(end);
    size_t at = authority.rfind('@');
    if (at != std::string_view::npos) {
      std::string_view userinfo = authority.substr(0, at);
      authority = authority.substr(at + 1);
      size_t colon = userinfo.find(':');
      url.username = std::string(userinfo.substr(0, colon));
      if (colon != std::string_view::npos) {
        url.password = std::string(userinfo.substr(colon + 1));
      }
    }
    size_t colon = authority.rfind(':');
    if (colon != std::string_view::npos &&
        authority.find(']', colon) == std::string_view::npos) {
      url.port = std::string(authority.substr(colon + 1));
      authority = authority.substr(0, colon);
      if (url.port == DefaultPort(url.scheme)) url.port.clear();
    }
    url.hostname = Lower(authority);
    std::string path;
    SplitPathQueryFragment(rest, &path, &url.query, &url.fragment);
    url.pathname = NormalizePath(path);
  } else if (IsSpecialScheme(url.scheme)) {
    // "http:foo" and friends: treat as a path on an empty host.
    url.has_authority = true;
    std::string path;
    SplitPathQueryFragment(rest, &path, &url.query, &url.fragment);
    url.pathname = NormalizePath(path);
  } else {
    // Opaque path (about:blank, data:, blob:, javascript:).
    SplitPathQueryFragment(rest, &url.pathname, &url.query, &url.fragment);
  }
  url.valid = true;
  return url;
}

ParsedURL ParsedURL::Resolve(std::string_view input, const ParsedURL& base) {
  input = Trim(input);
  if (SchemeLength(input) != 0) return Parse(input);
  if (!base.valid) return ParsedURL();
  ParsedURL url = base;
  if (input.empty()) {
    url.fragment.clear();
    return url;
  }
  if (input.substr(0, 2) == "//") {
    return Parse(base.scheme + ":" + std::string(input));
  }
  if (input[0] == '#') {
    url.fragment = std::string(input.substr(1));
    return url;
  }
  std::string path;
  if (input[0] == '?') {
    std::string ignored;
    SplitPathQueryFragment(input, &ignored, &url.query, &url.fragment);
    return url;
  }
  SplitPathQueryFragment(input, &path, &url.query, &url.fragment);
  if (!base.has_authority) {
    url.pathname = path;
    return url;
  }
  if (!path.empty() && path[0] == '/') {
    url.pathname = NormalizePath(path);
  } else {
    size_t last_slash = base.pathname.rfind('/');
    std::string directory = last_slash == std::string::npos
                                ? "/"
                                : base.pathname.substr(0, last_slash + 1);
    url.pathname = NormalizePath(directory + path);
  }
  return url;
}

std::string ParsedURL::Host() const {
  return port.empty() ? hostname : hostname + ":" + port;
}

std::string ParsedURL::Origin() const {
  if (scheme == "http" || scheme == "https" || scheme == "ws" ||
      scheme == "wss" || scheme == "ftp") {
    return scheme + "://" + Host();
  }
  return "null";
}

std::string ParsedURL::Href() const {
  if (!valid) return "";
  std::string out = scheme + ":";
  if (has_authority) {
    out += "//";
    if (!username.empty() || !password.empty()) {
      out += username;
      if (!password.empty()) out += ":" + password;
      out += "@";
    }
    out += Host();
  }
  out += pathname;
  if (!query.empty()) out += "?" + query;
  if (!fragment.empty()) out += "#" + fragment;
  return out;
}

std::string PercentDecode(std::string_view input, bool plus_as_space) {
  auto hex = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (c == '+' && plus_as_space) {
      out.push_back(' ');
    } else if (c == '%' && i + 2 < input.size() + 0 && hex(input[i + 1]) >= 0 &&
               hex(input[i + 2]) >= 0) {
      out.push_back(
          static_cast<char>(hex(input[i + 1]) * 16 + hex(input[i + 2])));
      i += 2;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::vector<std::pair<std::string, std::string>> ParseQuery(
    std::string_view query) {
  std::vector<std::pair<std::string, std::string>> pairs;
  if (!query.empty() && query[0] == '?') query.remove_prefix(1);
  size_t i = 0;
  while (i <= query.size() && !query.empty()) {
    size_t amp = query.find('&', i);
    if (amp == std::string_view::npos) amp = query.size();
    std::string_view part = query.substr(i, amp - i);
    if (!part.empty()) {
      size_t eq = part.find('=');
      std::string_view name = part.substr(0, eq);
      std::string_view value = eq == std::string_view::npos
                                   ? std::string_view()
                                   : part.substr(eq + 1);
      pairs.emplace_back(PercentDecode(name, true), PercentDecode(value, true));
    }
    i = amp + 1;
  }
  return pairs;
}

std::string SerializeQuery(
    const std::vector<std::pair<std::string, std::string>>& pairs) {
  auto encode = [](std::string_view s, std::string* out) {
    static const char kHex[] = "0123456789ABCDEF";
    for (char ch : s) {
      unsigned char c = static_cast<unsigned char>(ch);
      if (std::isalnum(c) || c == '*' || c == '-' || c == '.' || c == '_') {
        out->push_back(ch);
      } else if (c == ' ') {
        out->push_back('+');
      } else {
        out->push_back('%');
        out->push_back(kHex[c >> 4]);
        out->push_back(kHex[c & 15]);
      }
    }
  };
  std::string out;
  for (const auto& pair : pairs) {
    if (!out.empty()) out.push_back('&');
    encode(pair.first, &out);
    out.push_back('=');
    encode(pair.second, &out);
  }
  return out;
}

}  // namespace mock_dom
}  // namespace v8
