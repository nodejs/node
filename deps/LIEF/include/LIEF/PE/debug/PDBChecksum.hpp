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
#ifndef LIEF_PE_PDBCHECKSUM_H
#define LIEF_PE_PDBCHECKSUM_H
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/PE/debug/Debug.hpp"

namespace LIEF {
namespace PE {

/// This class represents the PDB Checksum debug entry which is essentially
/// an array of bytes representing the checksum of the PDB content.
class LIEF_API PDBChecksum : public Debug {
  public:
  static std::unique_ptr<PDBChecksum>
    parse(const details::pe_debug& hdr, Section* section, span<uint8_t> payload);

  enum class HASH_ALGO : uint32_t {
    UNKNOWN = 0,
    SHA256,
  };

  PDBChecksum(HASH_ALGO algo, std::vector<uint8_t> hash) :
    Debug(Debug::TYPES::PDBCHECKSUM),
    algo_(algo),
    hash_(std::move(hash))
  {}

  PDBChecksum(const details::pe_debug& dbg, Section* sec,
              HASH_ALGO algo, std::vector<uint8_t> hash) :
    Debug(dbg, sec),
    algo_(algo),
    hash_(std::move(hash))
  {}

  PDBChecksum(const PDBChecksum& other) = default;
  PDBChecksum& operator=(const PDBChecksum& other) = default;

  PDBChecksum(PDBChecksum&&) = default;
  PDBChecksum& operator=(PDBChecksum&& other) = default;

  std::unique_ptr<Debug> clone() const override {
    return std::unique_ptr<Debug>(new PDBChecksum(*this));
  }

  /// Hash of the PDB content
  span<const uint8_t> hash() const {
    return hash_;
  }

  span<uint8_t> hash() {
    return hash_;
  }

  void hash(std::vector<uint8_t> h) {
    hash_ = std::move(h);
  }

  /// Algorithm used for hashing the PDB content
  HASH_ALGO algorithm() const {
    return algo_;
  }

  void algorithm(HASH_ALGO algo) {
    algo_ = algo;
  }

  static bool classof(const Debug* debug) {
    return debug->type() == Debug::TYPES::PDBCHECKSUM;
  }

  ~PDBChecksum() override = default;

  std::string to_string() const override;

  private:
  HASH_ALGO algo_ = HASH_ALGO::UNKNOWN;
  std::vector<uint8_t> hash_;
};

LIEF_API const char* to_string(PDBChecksum::HASH_ALGO e);

}
}

#endif
