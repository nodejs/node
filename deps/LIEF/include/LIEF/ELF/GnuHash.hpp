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
#ifndef LIEF_ELF_GNU_HASH_H
#define LIEF_ELF_GNU_HASH_H

#include <vector>
#include <ostream>
#include <cstdint>
#include <memory>

#include "LIEF/errors.hpp"
#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
class SpanStream;
namespace ELF {

class Parser;
class Builder;
class Binary;
class Segment;

/// Class which provides a view over the GNU Hash implementation.
/// Most of the fields are read-only since the values
/// are re-computed by the LIEF::ELF::Builder.
class LIEF_API GnuHash : public Object {

  friend class Parser;
  friend class Builder;
  friend class Binary;

  public:
  GnuHash() = default;
  GnuHash(uint32_t symbol_idx, uint32_t shift2,
          std::vector<uint64_t> bloom_filters, std::vector<uint32_t> buckets,
          std::vector<uint32_t> hash_values = {}) :
    symbol_index_{symbol_idx},
    shift2_{shift2},
    bloom_filters_{std::move(bloom_filters)},
    buckets_{std::move(buckets)},
    hash_values_{std::move(hash_values)}
  {}

  GnuHash& operator=(const GnuHash& copy) = default;
  GnuHash(const GnuHash& copy) = default;

  GnuHash(GnuHash&&) = default;
  GnuHash& operator=(GnuHash&&) = default;

  ~GnuHash() override = default;

  /// Return the number of buckets
  /// @see GnuHash::buckets
  uint32_t nb_buckets() const {
    return buckets_.size();
  }

  /// Index of the first symbol in the dynamic
  /// symbols table which accessible with the hash table
  uint32_t symbol_index() const {
    return symbol_index_;
  }

  /// Shift count used in the bloom filter
  uint32_t shift2() const {
    return shift2_;
  }

  /// Number of bloom filters used.
  /// It must be a power of 2
  uint32_t maskwords() const {
    return bloom_filters_.size();
  }

  /// Bloom filters
  const std::vector<uint64_t>& bloom_filters() const {
    return bloom_filters_;
  }

  /// Hash buckets
  const std::vector<uint32_t>& buckets() const {
    return buckets_;
  }

  /// Hash values
  const std::vector<uint32_t>& hash_values() const {
    return hash_values_;
  }

  /// Check if the given hash passes the bloom filter
  bool check_bloom_filter(uint32_t hash) const;

  /// Check if the given hash passes the bucket filter
  bool check_bucket(uint32_t hash) const {
    return buckets_[hash % nb_buckets()] > 0;
  }

  /// Check if the symbol *probably* exists. If
  /// the returned value is ``false`` you can assume at ``100%`` that
  /// the symbol with the given name doesn't exist. If ``true``, you can't
  /// do any assumption
  bool check(const std::string& symbol_name) const;

  /// Check if the symbol associated with the given hash *probably* exists. If
  /// the returned value is ``false`` you can assume at ``100%`` that
  /// the symbol doesn't exists. If ``true`` you can't
  /// do any assumption
  bool check(uint32_t hash) const;


  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const GnuHash& gnuhash);

  template<class ELF_T>
  static LIEF_LOCAL std::unique_ptr<GnuHash> parse(SpanStream& strm, uint64_t dynsymcount);

  template<class ELF_T>
  static LIEF_LOCAL result<uint32_t> nb_symbols(SpanStream& strm);

  LIEF_LOCAL uint64_t original_size() const {
    return original_size_;
  }

  private:
  uint32_t symbol_index_ = 0;
  uint32_t shift2_       = 0;

  std::vector<uint64_t> bloom_filters_;
  std::vector<uint32_t> buckets_;
  std::vector<uint32_t> hash_values_;

  size_t c_ = 0;
  uint64_t original_size_ = 0;
};


} // namepsace ELF
} // namespace LIEF

#endif
