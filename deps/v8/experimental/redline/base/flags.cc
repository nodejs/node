// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/base/flags.h"

#include <cerrno>
#include <cstdlib>

#include "experimental/redline/base/log.h"

namespace redline {

bool ParseInt64(std::string_view text, int64_t* out) {
  if (text.empty()) {
    return false;
  }
  std::string buffer(text);
  int64_t multiplier = 1;
  char last = buffer.back();
  if (last == 'k' || last == 'K') {
    multiplier = 1024;
  } else if (last == 'm' || last == 'M') {
    multiplier = static_cast<int64_t>(1024) * 1024;
  } else if (last == 'g' || last == 'G') {
    multiplier = static_cast<int64_t>(1024) * 1024 * 1024;
  }
  if (multiplier != 1) {
    buffer.pop_back();
  }
  if (buffer.empty()) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  long long value = strtoll(buffer.c_str(), &end, 0);
  if (errno != 0 || end == nullptr || *end != '\0') {
    return false;
  }
  *out = static_cast<int64_t>(value) * multiplier;
  return true;
}

// static
Flags Flags::Parse(int argc, const char* const* argv, int start_index) {
  Flags flags;
  bool only_positional = false;
  for (int i = start_index; i < argc; i++) {
    std::string_view arg(argv[i]);
    if (only_positional || !arg.starts_with("--")) {
      flags.positional_.emplace_back(arg);
      continue;
    }
    if (arg == "--") {
      only_positional = true;
      continue;
    }
    arg.remove_prefix(2);
    size_t eq = arg.find('=');
    if (eq != std::string_view::npos) {
      flags.values_[std::string(arg.substr(0, eq))] =
          std::string(arg.substr(eq + 1));
    } else if (arg.size() > 3 && arg.starts_with("no-")) {
      flags.values_[std::string(arg.substr(3))] = "false";
    } else {
      flags.values_[std::string(arg)] = "true";
    }
  }
  return flags;
}

bool Flags::Has(std::string_view name) const {
  queried_.emplace(name);
  return values_.find(name) != values_.end();
}

std::string Flags::GetString(std::string_view name,
                             std::string default_value) const {
  queried_.emplace(name);
  auto it = values_.find(name);
  if (it == values_.end()) {
    return default_value;
  }
  return it->second;
}

int64_t Flags::GetInt(std::string_view name, int64_t default_value) const {
  queried_.emplace(name);
  auto it = values_.find(name);
  if (it == values_.end()) {
    return default_value;
  }
  int64_t value;
  if (!ParseInt64(it->second, &value)) {
    Fatal("Flag --%s expects an integer, got '%s'", std::string(name).c_str(),
          it->second.c_str());
  }
  return value;
}

double Flags::GetDouble(std::string_view name, double default_value) const {
  queried_.emplace(name);
  auto it = values_.find(name);
  if (it == values_.end()) {
    return default_value;
  }
  char* end = nullptr;
  double value = strtod(it->second.c_str(), &end);
  if (end == nullptr || *end != '\0' || it->second.empty()) {
    Fatal("Flag --%s expects a number, got '%s'", std::string(name).c_str(),
          it->second.c_str());
  }
  return value;
}

bool Flags::GetBool(std::string_view name, bool default_value) const {
  queried_.emplace(name);
  auto it = values_.find(name);
  if (it == values_.end()) {
    return default_value;
  }
  const std::string& v = it->second;
  if (v == "true" || v == "1" || v == "yes" || v == "on") {
    return true;
  }
  if (v == "false" || v == "0" || v == "no" || v == "off") {
    return false;
  }
  Fatal("Flag --%s expects a boolean, got '%s'", std::string(name).c_str(),
        v.c_str());
}

std::vector<std::string> Flags::GetList(std::string_view name,
                                        char separator) const {
  std::vector<std::string> result;
  std::string value = GetString(name);
  size_t start = 0;
  while (start <= value.size()) {
    size_t end = value.find(separator, start);
    if (end == std::string::npos) {
      end = value.size();
    }
    if (end > start) {
      result.push_back(value.substr(start, end - start));
    }
    start = end + 1;
  }
  return result;
}

std::vector<std::string> Flags::UnusedFlags() const {
  std::vector<std::string> unused;
  for (const auto& [name, value] : values_) {
    if (queried_.find(name) == queried_.end()) {
      unused.push_back(name);
    }
  }
  return unused;
}

std::string Flags::ToString() const {
  std::string result;
  for (const auto& [name, value] : values_) {
    if (!result.empty()) {
      result += ' ';
    }
    result += "--";
    result += name;
    result += '=';
    result += value;
  }
  for (const std::string& p : positional_) {
    if (!result.empty()) {
      result += ' ';
    }
    result += p;
  }
  return result;
}

}  // namespace redline
