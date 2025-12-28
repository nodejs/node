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
#ifndef LIEF_MACHO_FAT_BINARY_H
#define LIEF_MACHO_FAT_BINARY_H
#include <string>
#include <vector>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/MachO/Header.hpp"

namespace LIEF {
class Parser;
namespace MachO {

class Parser;
class Builder;
class Binary;

/// Class which represent a Mach-O (fat) binary
/// This object is also used for representing Mach-O binaries that are **NOT FAT**
class LIEF_API FatBinary {

  friend class LIEF::Parser;
  friend class Parser;
  friend class Builder;

  public:

  /// Internal containter used to store Binary objects within a Fat Mach-O
  using binaries_t = std::vector<std::unique_ptr<Binary>>;

  /// Iterator that outputs Binary&
  using it_binaries = ref_iterator<binaries_t&, Binary*>;

  /// Iterator that outputs const Binary&
  using it_const_binaries = const_ref_iterator<const binaries_t&, Binary*>;

  FatBinary(const FatBinary&) = delete;
  FatBinary& operator=(const FatBinary&) = delete;

  virtual ~FatBinary();

  /// Number of MachO::Binary wrapped by this object
  size_t size() const {
    return binaries_.size();
  }

  /// Checks whether this object contains MachO::Binary
  bool empty() const {
    return binaries_.empty();
  }

  it_binaries begin() {
    return binaries_;
  }
  it_const_binaries begin() const {
    return binaries_;
  }

  it_binaries end() {
    return it_binaries(binaries_).end();
  }

  it_const_binaries end() const {
    return it_const_binaries(binaries_).end();
  }

  void release_all_binaries();

  /// Get a pointer to the last MachO::Binary object presents in this Fat Binary.
  /// It returns a nullptr if no binary are present.
  std::unique_ptr<Binary> pop_back();

  /// Get a pointer to the MachO::Binary specified by the ``index``.
  /// It returns a nullptr if the binary does not exist at the given index.
  Binary*       at(size_t index);
  const Binary* at(size_t index) const;

  Binary*       back();
  const Binary* back() const;

  Binary*       front();
  const Binary* front() const;

  Binary* operator[](size_t index) {
    return at(index);
  }
  const Binary* operator[](size_t index) const {
    return at(index);
  }

  /// Extract a MachO::Binary object. Gives ownership to the caller, and
  /// remove it from this FatBinary object.
  ///
  /// @warning: this invalidates any previously hold iterator!
  std::unique_ptr<Binary> take(size_t index);

  /// Take the underlying MachO::Binary that matches the given architecture
  /// If no binary with the architecture can be found, return a nullptr
  std::unique_ptr<Binary> take(Header::CPU_TYPE cpu);

  /// Reconstruct the Fat binary object and write it in `filename`
  /// @param filename Path to write the reconstructed binary
  void write(const std::string& filename);

  /// Reconstruct the Fat binary object and return his content as bytes
  std::vector<uint8_t> raw();

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const FatBinary& fatbinary);

  private:
  LIEF_LOCAL FatBinary();
  LIEF_LOCAL FatBinary(binaries_t binaries);
  binaries_t binaries_;
};

} // namespace MachO
} // namespace LIEF
#endif
