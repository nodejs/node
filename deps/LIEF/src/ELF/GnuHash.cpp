/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#include <spdlog/fmt/fmt.h>

#include "LIEF/Visitor.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/utils.hpp"
#include "LIEF/ELF/GnuHash.hpp"
#include "LIEF/ELF/Segment.hpp"

#include "logging.hpp"
#include "ELF/Structures.hpp"


namespace LIEF::ELF {

bool GnuHash::check_bloom_filter(uint32_t hash) const {
  if (maskwords() == 0) {
    return false;
  }
  const size_t C = c_;
  const uint32_t h1 = hash;
  const uint32_t h2 = hash >> shift2();

  const uint32_t n1 = (h1 / C) % maskwords();

  const uint32_t b1 = h1 % C;
  const uint32_t b2 = h2 % C;
  const uint64_t filter = bloom_filters()[n1];
  return ((filter >> b1) & (filter >> b2) & 1) != 0u;
}

bool GnuHash::check(const std::string& symbol_name) const {
  uint32_t hash = dl_new_hash(symbol_name.c_str());
  return check(hash);
}


bool GnuHash::check(uint32_t hash) const {
  if (!check_bloom_filter(hash)) { // Bloom filter not passed
    return false;
  }

  if (!check_bucket(hash)) { // hash buck not passed
    return false;
  }
  return true;
}


void GnuHash::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

template<typename ELF_T>
std::unique_ptr<GnuHash> GnuHash::parse(SpanStream& strm, uint64_t dynsymcount) {
  // See: https://github.com/lattera/glibc/blob/master/elf/dl-lookup.c#L860
  // and  https://github.com/lattera/glibc/blob/master/elf/dl-lookup.c#L226
  using uint__ = typename ELF_T::uint;
  LIEF_DEBUG("Parsing GNU hash");
  const uint64_t opos = strm.pos();

  auto gnuhash = std::make_unique<GnuHash>();

  gnuhash->c_ = sizeof(uint__) * 8;

  auto nbuckets = strm.read<uint32_t>();
  if (!nbuckets) {
    LIEF_ERR("Failed to read bucket count");
    return nullptr;
  }

  auto symidx = strm.read<uint32_t>();
  if (!symidx) {
    LIEF_ERR("Failed to read symndx");
    return nullptr;
  }

  gnuhash->symbol_index_ = *symidx;

  auto maskwords = strm.read<uint32_t>();
  if (!maskwords) {
    LIEF_ERR("Failed to read maskwords");
    return nullptr;
  }

  auto shift2 = strm.read<uint32_t>();
  if (!shift2) {
    LIEF_ERR("Failed to read shift2");
    return nullptr;
  }
  gnuhash->shift2_ = *shift2;

  if (*maskwords & (*maskwords - 1)) {
    LIEF_WARN("maskwords is not a power of 2");
  }

  std::vector<uint__> bloom_filters;
  if (strm.read_objects(bloom_filters, *maskwords)) {
    std::copy(bloom_filters.begin(), bloom_filters.end(),
              std::back_inserter(gnuhash->bloom_filters_));
  } else {
    LIEF_ERR("GNU hash maskwords corrupted");
  }

  if (!strm.read_objects(gnuhash->buckets_, *nbuckets)) {
    LIEF_ERR("GNU hash buckets corrupted");
  }

  if (dynsymcount > 0 && dynsymcount >= gnuhash->symbol_index_) {
    const uint32_t nb_hash = dynsymcount - gnuhash->symbol_index_;
    if (!strm.read_objects(gnuhash->hash_values_, nb_hash)) {
      LIEF_ERR("Failed to read hash values (count={})", nb_hash);
    }
  }

  gnuhash->original_size_ = strm.pos() - opos;
  return gnuhash;
}


template<class ELF_T>
result<uint32_t> GnuHash::nb_symbols(SpanStream& strm) {
  using uint__ = typename ELF_T::uint;
  const auto res_nbuckets = strm.read<uint32_t>();
  if (!res_nbuckets) {
    return 0;
  }

  const auto res_symndx = strm.read<uint32_t>();
  if (!res_symndx) {
    return 0;
  }

  const auto res_maskwords = strm.read<uint32_t>();
  if (!res_maskwords) {
    return 0;
  }

  const auto nbuckets = *res_nbuckets;
  const auto symndx = *res_symndx;
  const auto maskwords = *res_maskwords;

  // skip shift2, unused as we don't need the bloom filter to count syms.
  strm.increment_pos(sizeof(uint32_t));

  if (maskwords & (maskwords - 1)) {
    LIEF_WARN("maskwords is not a power of 2");
    return 0;
  }

  // skip bloom filter mask words
  strm.increment_pos(sizeof(uint__) * (maskwords));

  uint32_t max_bucket = 0;
  for (size_t i = 0; i < nbuckets; ++i) {
    auto bucket = strm.read<uint32_t>();
    if (!bucket) {
      break;
    }
    if (*bucket > max_bucket) {
      max_bucket = *bucket;
    }
  }

  if (max_bucket == 0) {
    return 0;
  }

  if (max_bucket < symndx) {
    LIEF_WARN("GNU hash: symbol index ({}) is larger than the max bucket ({})",
              symndx, max_bucket);
    return 0;
  }

  // Skip to the contents of the bucket with the largest symbol index
  strm.increment_pos(sizeof(uint32_t) * (max_bucket - symndx));

  // Count values in the bucket
  uint32_t hash_value = 0;
  size_t nsyms = 0;
  do {
    if (!strm.can_read<uint32_t>()) {
      return 0;
    }
    hash_value = *strm.read<uint32_t>();

    nsyms++;
  } while ((hash_value & 1) == 0); // "It is set to 1 when a symbol is the last
                                   // symbol in a given hash bucket"

  return max_bucket + nsyms;
}

std::ostream& operator<<(std::ostream& os, const GnuHash& gnuhash) {
  os << fmt::format("Number of buckets:           {}\n", gnuhash.nb_buckets())
     << fmt::format("First symbol index:          {}\n", gnuhash.symbol_index())
     << fmt::format("Shift Count:                 {}\n", gnuhash.shift2())
     << fmt::format("Bloom filters:               [{:#x}]\n",
                    fmt::join(gnuhash.bloom_filters(), ", "))
     << fmt::format("Buckets:                     [{}]\n",
                    fmt::join(gnuhash.buckets(), ", "))
     << fmt::format("Hash values:                 [{:#x}]\n",
                    fmt::join(gnuhash.hash_values(), ", "));
  return os;
}

template std::unique_ptr<GnuHash> GnuHash::parse<details::ELF64>(SpanStream&,
                                                                 uint64_t);
template std::unique_ptr<GnuHash> GnuHash::parse<details::ELF32>(SpanStream&,
                                                                 uint64_t);
template std::unique_ptr<GnuHash> GnuHash::parse<details::ELF32_x32>(SpanStream&,
                                                                     uint64_t);
template std::unique_ptr<GnuHash> GnuHash::parse<details::ELF32_arm64>(SpanStream&,
                                                                       uint64_t);

template result<uint32_t> GnuHash::nb_symbols<details::ELF64>(SpanStream&);
template result<uint32_t> GnuHash::nb_symbols<details::ELF32>(SpanStream&);
template result<uint32_t> GnuHash::nb_symbols<details::ELF32_x32>(SpanStream&);
template result<uint32_t> GnuHash::nb_symbols<details::ELF32_arm64>(SpanStream&);


} // namespace LIEF::ELF
