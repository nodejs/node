// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_BASE_FLAGS_H_
#define V8_EXPERIMENTAL_REDLINE_BASE_FLAGS_H_

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace redline {

// A deliberately small command line parser. Every redline command accepts
// flags of the form "--name=value", "--name" (boolean true) and "--no-name"
// (boolean false). Everything else is a positional argument. "--" ends flag
// parsing. Flags are recorded as strings and converted on access, so a
// command can report unknown flags via UnusedFlags() after it has queried
// everything it understands.
class Flags {
 public:
  Flags() = default;

  // Parses argv[start_index..argc).
  static Flags Parse(int argc, const char* const* argv, int start_index);

  bool Has(std::string_view name) const;

  std::string GetString(std::string_view name,
                        std::string default_value = "") const;
  int64_t GetInt(std::string_view name, int64_t default_value) const;
  double GetDouble(std::string_view name, double default_value) const;
  bool GetBool(std::string_view name, bool default_value) const;
  // Splits a flag value on `separator`, dropping empty pieces.
  std::vector<std::string> GetList(std::string_view name,
                                   char separator = ',') const;

  const std::vector<std::string>& positional() const { return positional_; }

  // Flags that were given but never queried through one of the getters.
  std::vector<std::string> UnusedFlags() const;

  // Human readable rendering of all flags (for logs and event files).
  std::string ToString() const;

 private:
  std::map<std::string, std::string, std::less<>> values_;
  mutable std::set<std::string, std::less<>> queried_;
  std::vector<std::string> positional_;
};

// Parses an integer with an optional size suffix (k, m, g). Returns false
// on malformed input.
bool ParseInt64(std::string_view text, int64_t* out);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_BASE_FLAGS_H_
