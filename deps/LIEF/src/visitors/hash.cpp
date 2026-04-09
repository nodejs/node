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
#include <functional>
#include <numeric>

#include "mbedtls/sha256.h"

#include "LIEF/hash.hpp"


#if defined(LIEF_PE_SUPPORT)
#include "LIEF/PE/hash.hpp"
#endif

#if defined(LIEF_ELF_SUPPORT)
#include "LIEF/ELF/hash.hpp"
#endif

#if defined(LIEF_MACHO_SUPPORT)
#include "LIEF/MachO/hash.hpp"
#endif

#if defined(LIEF_OAT_SUPPORT)
#include "LIEF/OAT/hash.hpp"
#endif

#if defined(LIEF_ART_SUPPORT)
#include "LIEF/ART/hash.hpp"
#endif

#if defined(LIEF_DEX_SUPPORT)
#include "LIEF/DEX/hash.hpp"
#endif

#if defined(LIEF_VDEX_SUPPORT)
#include "LIEF/VDEX/hash.hpp"
#endif

namespace LIEF {

Hash::value_type hash(const Object& v) {
  Hash::value_type value = 0;

#if defined(LIEF_PE_SUPPORT)
  value = Hash::combine(value, Hash::hash<PE::Hash>(v));
#endif

#if defined(LIEF_ELF_SUPPORT)
  value = Hash::combine(value, Hash::hash<ELF::Hash>(v));
#endif

#if defined(LIEF_MACHO_SUPPORT)
  value = Hash::combine(value, Hash::hash<MachO::Hash>(v));
#endif

#if defined(LIEF_OAT_SUPPORT)
  value = Hash::combine(value, Hash::hash<OAT::Hash>(v));
#endif

#if defined(LIEF_ART_SUPPORT)
  value = Hash::combine(value, Hash::hash<ART::Hash>(v));
#endif

#if defined(LIEF_DEX_SUPPORT)
  value = Hash::combine(value, Hash::hash<DEX::Hash>(v));
#endif

#if defined(LIEF_VDEX_SUPPORT)
  value = Hash::combine(value, Hash::hash<VDEX::Hash>(v));
#endif

  return value;
}

Hash::value_type hash(const std::vector<uint8_t>& raw) {
  return Hash::hash(raw);
}

Hash::value_type hash(span<const uint8_t> raw) {
  return Hash::hash(raw);
}

Hash::~Hash() = default;
Hash::Hash() = default;

Hash::Hash(Hash::value_type init_value) :
  value_{init_value}
{}

Hash& Hash::process(const Object& obj) {
  value_ = combine(value_, LIEF::hash(obj));
  return *this;
}

Hash& Hash::process(size_t integer) {
  value_ = combine(value_, std::hash<Hash::value_type>{}(integer));
  return *this;
}

Hash& Hash::process(const std::string& str) {
  value_ = combine(value_, std::hash<std::string>{}(str));
  return *this;
}

Hash& Hash::process(const std::u16string& str) {
  value_ = combine(value_, std::hash<std::u16string>{}(str));
  return *this;
}

Hash& Hash::process(const std::vector<uint8_t>& raw) {
  value_ = combine(value_, Hash::hash(raw));
  return *this;
}

Hash& Hash::process(span<const uint8_t> raw) {
  value_ = combine(value_, Hash::hash(raw));
  return *this;
}

// Static methods
// ==============
Hash::value_type Hash::hash(const std::vector<uint8_t>& raw) {
  return hash(raw.data(), raw.size());
}


Hash::value_type Hash::hash(const void* raw, size_t size) {
  const auto* start = reinterpret_cast<const uint8_t*>(raw);

  std::vector<uint8_t> sha256(32, 0u);
  mbedtls_sha256(start, size, sha256.data(), 0);

  return std::accumulate(std::begin(sha256), std::end(sha256), size_t(0),
     [] (size_t v, uint8_t n) {
        return (v << sizeof(uint8_t) * 8u) | n;
     });
}

Hash::value_type Hash::hash(span<const uint8_t> raw) {
  return hash(raw.data(), raw.size());
}

}
