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
#ifndef LIEF_INTERNAL_UTILS_HEADER
#define LIEF_INTERNAL_UTILS_HEADER
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/ranges.h"

#include "LIEF/span.hpp"
#include "LIEF/optional.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"


namespace LIEF {
std::string printable_string(const std::string& str);

inline bool is_printable(char c) {
  return ' ' <= c && c <= '~';
}

inline bool is_printable(const std::string& str) {
  return std::all_of(std::begin(str), std::end(str),
    [] (char c) { return is_printable(c); }
  );
}

template<class T>
inline std::vector<T> as_vector(span<T> s) {
  return std::vector<T>(s.begin(), s.end());
}

template<class T>
inline std::vector<T> as_vector(span<const T> s) {
  return std::vector<T>(s.begin(), s.end());
}

template<class T>
inline const char* to_string_or(result<T> res, const char* defval = "???") {
  return res ? to_string(*res) : defval;
}

template<class T>
inline std::string to_string(const T& obj) {
  std::stringstream oss;
  oss << obj;
  return oss.str();
}


template<class T>
inline std::string to_hex(const T& container, size_t maxsize = 0) {
  if (container.empty()) {
    return "";
  }
  size_t count = maxsize;
  if (count == 0 || count > container.size()) {
    count = container.size();
  }
  std::string out;
  out.reserve(count * 2);
  for (size_t i = 0; i < count; ++i) {
    out += fmt::format("{:02x} ", container[i]);
  }
  if (count < container.size()) {
    out += "...";
  } else{
    out.pop_back(); // remove trailing ' '
  }
  return out;
}

template<typename HANDLER>
std::vector<std::string> optimize(const HANDLER& container,
                                  std::string(* getter)(const typename HANDLER::value_type&),
                                  size_t& offset_counter,
                                  std::unordered_map<std::string, size_t> *of_map_p = nullptr)
{
  if (container.empty()) {
    return {};
  }

  std::set<std::string> string_table;
  std::vector<std::string> string_table_optimized;
  string_table_optimized.reserve(container.size());

  // reverse all symbol names and sort them so we can merge them in the linear time:
  // aaa, aadd, aaaa, cca, ca -> aaaa, aaa, acc, ac ddaa
  std::transform(std::begin(container), std::end(container),
                 std::inserter(string_table, std::end(string_table)),
                 getter);

  for (const auto& val: string_table) {
    string_table_optimized.emplace_back(val);
    std::reverse(std::begin(string_table_optimized.back()), std::end(string_table_optimized.back()));
  }

  std::sort(std::begin(string_table_optimized), std::end(string_table_optimized),
      [] (const std::string& lhs, const std::string& rhs) {
          bool ret = false;
          if (lhs.size() > rhs.size()) {
            auto res = lhs.compare(0, rhs.size(), rhs);
            ret = (res <= 0);
          } else {
            auto res = rhs.compare(0, lhs.size(), lhs);
            ret = (res > 0);
          }
          return ret;
      }
  );

  // as all elements that can be merged are adjacent we can just go through the list once
  // and memorize ones we merged to calculate the offsets later
  std::unordered_map<std::string, std::string> merged_map;
  size_t to_set_idx = 0, cur_elm_idx = 1;
  for (; cur_elm_idx < string_table_optimized.size(); ++cur_elm_idx) {
      auto &cur_elm = string_table_optimized[cur_elm_idx];
      auto &to_set_elm = string_table_optimized[to_set_idx];
      if (to_set_elm.size() >= cur_elm.size()) {
          auto ret = to_set_elm.compare(0, cur_elm.size(), cur_elm);
          if (ret == 0) {
            // when memorizing reverse back symbol names
            std::string rev_cur_elm = cur_elm;
            std::string rev_to_set_elm = to_set_elm;
            std::reverse(std::begin(rev_cur_elm), std::end(rev_cur_elm));
            std::reverse(std::begin(rev_to_set_elm), std::end(rev_to_set_elm));
            merged_map[rev_cur_elm] = rev_to_set_elm;
            continue;
          }
      }
      ++to_set_idx;
      std::swap(string_table_optimized[to_set_idx], cur_elm);
  }
  // if the first one is empty
  if (string_table_optimized[0].empty()) {
    std::swap(string_table_optimized[0], string_table_optimized[to_set_idx]);
    --to_set_idx;
  }
  string_table_optimized.resize(to_set_idx + 1);

  //reverse symbols back and sort them again
  for (auto &val: string_table_optimized) {
    std::reverse(std::begin(val), std::end(val));
  }
  std::sort(std::begin(string_table_optimized), std::end(string_table_optimized));

  if (of_map_p != nullptr) {
    std::unordered_map<std::string, size_t>& offset_map = *of_map_p;
    offset_map[""] = 0;

    for (const auto &v : string_table_optimized) {
      if (!v.empty()) {
        offset_map[v] = offset_counter;
        offset_counter += v.size() + 1;
      }
    }
    for (const auto &kv : merged_map) {
      if (!kv.first.empty()) {
        offset_map[kv.first] = offset_map[kv.second] + (kv.second.size() - kv.first.size());
      }
    }
  }

  return string_table_optimized;
}

template<class T>
auto make_empty_iterator() {
  auto begin = std::make_unique<typename T::Iterator::implementation>();
  auto end = std::make_unique<typename  T::Iterator::implementation>();
  return make_range<typename T::Iterator>(
      typename T::Iterator(std::move(begin)),
      typename T::Iterator(std::move(end))
  );
}

inline bool is_hex_number(const std::string& str) {
  return std::all_of(std::begin(str), std::end(str), ::isxdigit);
}

inline std::string hex_str(uint8_t c) {
  return fmt::format("{:02x}", c);
}

std::string hex_dump(const std::vector<uint8_t>& data,
                     const std::string& sep = ":");

std::string hex_dump(span<const uint8_t> data,
                     const std::string& sep = ":");

std::string indent(const std::string& input, size_t level);

inline bool is_digit(char c) {
  return '0' <= c && c <= '9';
}

inline bool is_digit(const std::string& str) {
  return std::all_of(str.begin(), str.end(), (bool(*)(char))&is_digit);
}

inline bool is_digit(const char* str) {
  while (*str != 0) {
    if (!is_digit(*str)) {
      return false;
    }
    ++str;
  }
  return true;
}

std::string ts_to_str(uint64_t timestamp);

template <size_t N>
inline std::string uuid_to_str_impl(uint8_t (&uuid)[N]) {
  std::vector<std::string> hexstr;
  std::transform(std::begin(uuid), std::end(uuid), std::back_inserter(hexstr),
    [] (uint8_t x) { return fmt::format("{:02x}", x); }
  );
  return fmt::to_string(fmt::join(hexstr, ":"));
}

template <size_t N>
inline std::string uuid_to_str_impl(const std::array<uint8_t, N>& uuid) {
  std::vector<std::string> hexstr;
  std::transform(std::begin(uuid), std::end(uuid), std::back_inserter(hexstr),
    [] (uint8_t x) { return fmt::format("{:02x}", x); }
  );
  return fmt::to_string(fmt::join(hexstr, ":"));
}

inline bool endswith(const std::string& str, const std::string& suffix) {
  if (suffix.size() > str.size()) {
    return false;
  }
  return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

inline optional<std::string> libname(const std::string& path, char sep = '/') {
  size_t pos = path.rfind(sep);
  if (pos == std::string::npos) {
    return nullopt();
  }
  return path.substr(pos + 1);
}

}

#endif
