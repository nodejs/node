/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
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
#include "internal_utils.hpp"
#include <ctime>
#include <chrono>

namespace LIEF {

std::string printable_string(const std::string& str) {
  std::string out;
  out.reserve(str.size());
  for (char c : str) {
    if (is_printable(c)) {
      out += c;
    }
  }
  return out;
}

template<class T>
std::string hex_dump_impl(T data, const std::string& sep) {
  std::vector<std::string> hexdigits;
  hexdigits.reserve(data.size());
  std::transform(data.begin(), data.end(), std::back_inserter(hexdigits),
                 [] (uint8_t x) { return fmt::format("{:02x}", x); });
  return fmt::to_string(fmt::join(hexdigits, sep));
}

std::string hex_dump(const std::vector<uint8_t>& data, const std::string& sep) {
  return hex_dump_impl(data, sep);
}

std::string hex_dump(span<const uint8_t> data, const std::string& sep) {
  return hex_dump_impl(data, sep);
}

inline std::string pretty_hex(char c) {
  if (is_printable(c)) {
    return std::string("") + c;
  }
  return ".";
}

std::vector<std::string> split(const std::string& input, char c = '\n') {
  // Not really efficient but does not aim to
  std::stringstream strm(input);
  std::vector<std::string> out;
  std::string element;
  while (std::getline(strm, element, c)) {
    out.push_back(element);
  }
  return out;
}

std::string indent(const std::string& input, size_t level) {
  if (input.empty()) {
    return "";
  }
  std::vector<std::string> lines = split(input);
  std::string output;
  output.reserve(input.size());
  for (const std::string& line : lines) {
    if (line.empty()) {
      output += '\n';
      continue;
    }
    output += std::string(level, ' ') + line + '\n';
  }
  if (output.empty()) {
    output.pop_back();
  }
  return output;
}


std::string ts_to_str(uint64_t timestamp) {
  using namespace fmt;
  using namespace std::chrono;
  system_clock::time_point tp = system_clock::time_point(std::chrono::seconds(timestamp));
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::string ts = std::ctime(&t);
  ts.resize(ts.size() - 1);
  return ts;
}

}
