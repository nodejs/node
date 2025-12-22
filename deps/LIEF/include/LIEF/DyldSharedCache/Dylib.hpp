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
#ifndef LIEF_DSC_DYLIB_H
#define LIEF_DSC_DYLIB_H
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/errors.hpp"

#include <memory>
#include <string>

namespace LIEF {

namespace MachO {
class Binary;
}

namespace dsc {

namespace details {
class Dylib;
class DylibIt;
}

/// This class represents a library embedded in a dyld shared cache.
/// It mirrors the original `dyld_cache_image_info` structure.
class LIEF_API Dylib {
  public:
  /// Dylib Iterator
  class LIEF_API Iterator :
    public iterator_facade_base<Iterator, std::random_access_iterator_tag,
                                std::unique_ptr<Dylib>, std::ptrdiff_t, Dylib*,
                                std::unique_ptr<Dylib>>
  {
    public:
    using implementation = details::DylibIt;

    Iterator(std::unique_ptr<details::DylibIt> impl);
    Iterator(const Iterator&);
    Iterator& operator=(const Iterator&);

    Iterator(Iterator&&) noexcept;
    Iterator& operator=(Iterator&&) noexcept;

    ~Iterator();
    bool operator<(const Iterator& rhs) const;

    std::ptrdiff_t operator-(const Iterator& R) const;

    Iterator& operator+=(std::ptrdiff_t n);
    Iterator& operator-=(std::ptrdiff_t n);

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);

    friend LIEF_API bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    std::unique_ptr<Dylib> operator*() const;

    private:
    std::unique_ptr<details::DylibIt> impl_;
  };
  public:
  /// This structure is used to tweak the extraction process while calling
  /// Dylib::get. These options allow to deoptimize the dylib and get an
  /// accurate representation of the origin Mach-O binary.
  struct LIEF_API extract_opt_t {
    extract_opt_t();

    /// Whether the segment's offsets should be packed to avoid
    /// an in-memory size while writing back the binary.
    ///
    /// \note This option does not have an impact on the performances
    bool pack = true;

    /// Fix call instructions that target addresses outside the current dylib
    /// virtual space.
    ///
    /// \warning Enabling this option can have a significant impact on the
    ///          performances. Make sure to enable the internal cache mechanism:
    ///          LIEF::dsc::enable_cache or LIEF::dsc::DyldSharedCache::enable_caching
    bool fix_branches = false;

    /// Fix memory accesses performed outside the dylib's virtual space
    ///
    /// \warning Enabling this option can have a significant impact on the
    ///          performances. Make sure to enable the internal cache mechanism:
    ///          LIEF::dsc::enable_cache or LIEF::dsc::DyldSharedCache::enable_caching
    bool fix_memory = false;

    /// Recover and fix relocations
    ///
    /// \warning Enabling this option can have a significant impact on the
    ///          performances. Make sure to enable the internal cache mechanism:
    ///          LIEF::dsc::enable_cache or LIEF::dsc::DyldSharedCache::enable_caching
    bool fix_relocations = false;

    /// Fix Objective-C information
    bool fix_objc = false;

    /// Whether the `LC_DYLD_CHAINED_FIXUPS` command should be (re)created.
    ///
    /// If this value is not set, LIEF will add the command only if it's
    /// meaningful regarding the other options
    LIEF::result<bool> create_dyld_chained_fixup_cmd;
  };

  Dylib(std::unique_ptr<details::Dylib> impl);
  ~Dylib();

  /// Original path of the library (e.g. `/usr/lib/libcryptex.dylib`)
  std::string path() const;

  /// In-memory address of the library
  uint64_t address() const;

  /// Modification time of the library matching `stat.st_mtime`, or 0
  uint64_t modtime() const;

  /// File serial number matching `stat.st_ino` or 0
  ///
  /// Note that for shared cache targeting iOS, this value can hold a hash of
  /// the path (if modtime is set to 0)
  uint64_t inode() const;

  /// Padding alignment value (should be 0)
  uint64_t padding() const;

  /// Get a MachO::Binary representation for this Dylib.
  ///
  /// One can use this function to write back the Mach-O binary on the disk:
  ///
  /// ```cpp
  /// dyld_cache->libraries()[12]->get()->write("liblockdown.dylib");
  /// ```
  std::unique_ptr<LIEF::MachO::Binary> get(const extract_opt_t& opt = extract_opt_t()) const;

  private:
  std::unique_ptr<details::Dylib> impl_;
};

}
}
#endif
