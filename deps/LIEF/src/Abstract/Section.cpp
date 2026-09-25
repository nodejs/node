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
#include <array>
#include <ostream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <utility>

#include "LIEF/Visitor.hpp"

#include "logging.hpp"
#include "LIEF/Abstract/hash.hpp"


#include "LIEF/Abstract/Section.hpp"

#include "Section.tcc"

namespace LIEF {

// Search functions
// ================
size_t Section::search(uint64_t integer, size_t pos, size_t size) const {
  if (size > sizeof(integer)) {
    return npos;
  }

  size_t minimal_size = size;
  if (size == 0) {
    if (integer < std::numeric_limits<uint8_t>::max()) {
      minimal_size = sizeof(uint8_t);
    }
    else if (integer < std::numeric_limits<uint16_t>::max()) {
      minimal_size = sizeof(uint16_t);
    }
    else if (integer < std::numeric_limits<uint32_t>::max()) {
      minimal_size = sizeof(uint32_t);
    }
    else if (integer < std::numeric_limits<uint64_t>::max()) {
      minimal_size = sizeof(uint64_t);
    } else {
      return npos;
    }
  }

  std::vector<uint8_t> pattern(minimal_size, 0);
  memcpy(pattern.data(), &integer, minimal_size);
  return search(pattern, pos);
}

size_t Section::search(const std::vector<uint8_t>& pattern, size_t pos) const {
  span<const uint8_t> content = this->content();

  const auto* it_found = std::search(
      std::begin(content) + pos, std::end(content),
      std::begin(pattern), std::end(pattern));

  if (it_found == std::end(content)) {
    return npos;
  }

  return std::distance(std::begin(content), it_found);
}

size_t Section::search(const std::string& pattern, size_t pos) const {
  std::vector<uint8_t> pattern_formated = {std::begin(pattern), std::end(pattern)};
  return search(pattern_formated, pos);
}

size_t Section::search(uint64_t integer, size_t pos) const {
  return search(integer, pos, 0);
}

// Search all functions
// ====================
std::vector<size_t> Section::search_all(uint64_t v, size_t size) const {
  std::vector<size_t> result;
  size_t pos = search(v, 0, size);

  if (pos == Section::npos) {
    return result;
  }

  do {
    result.push_back(pos);
    pos = search(v, pos + 1, size);
  } while(pos != Section::npos);

  return result;
}

std::vector<size_t> Section::search_all(uint64_t v) const {
  return search_all(v, 0);
}

std::vector<size_t> Section::search_all(const std::string& v) const {
  return search_all_<std::string>(v);
}


double Section::entropy() const {
  std::array<uint64_t, 256> frequencies = { {0} };
  span<const uint8_t> content = this->content();
  if (content.empty() || content.size() == 1) {
    return 0.;
  }
  for (uint8_t x : content) {
    frequencies[x]++;
  }

  double entropy = 0.0;
  for (uint64_t p : frequencies) {
    if (p > 0) {
      double freq = static_cast<double>(p) / static_cast<double>(content.size());
      entropy += freq * std::log2l(freq) ;
    }
  }
  return (-entropy);
}


void Section::accept(Visitor& visitor) const {
  visitor.visit(*this);
}




std::ostream& operator<<(std::ostream& os, const Section& entry) {
  os << std::hex;
  os << std::left
     << std::setw(30) << entry.name()
     << std::setw(10) << entry.virtual_address()
     << std::setw(10) << entry.size()
     << std::setw(10) << entry.offset()
     << std::setw(10) << entry.entropy();

  return os;
}

}
