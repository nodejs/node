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
#ifndef LIEF_DSC_DYLD_SHARED_CACHE_H
#define LIEF_DSC_DYLD_SHARED_CACHE_H
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/DyldSharedCache/Dylib.hpp"
#include "LIEF/DyldSharedCache/MappingInfo.hpp"
#include "LIEF/DyldSharedCache/SubCache.hpp"

#include "LIEF/asm/Instruction.hpp"

#include <memory>
#include <string>
#include <vector>

namespace LIEF {
class FileStream;

/// Namespace related to the dyld shared cache support
namespace dsc {
namespace details {
class DyldSharedCache;
}

/// This class represents a dyld shared cache file.
class LIEF_API DyldSharedCache {
  public:

  /// This enum wraps the dyld's git tags for which the structure of
  /// dyld shared cache evolved
  enum class VERSION : uint32_t {
    UNKNOWN = 0,

    DYLD_95_3,    ///< dyld-95.3 (2007-10-30)
    DYLD_195_5,   ///< dyld-195.5 (2011-07-13)
    DYLD_239_3,   ///< dyld-239.3 (2013-10-29)
    DYLD_360_14,  ///< dyld-360.14 (2015-09-04)
    DYLD_421_1,   ///< dyld-421.1 (2016-09-22)
    DYLD_832_7_1, ///< dyld-832.7.1 (2020-11-19)
    DYLD_940,     ///< dyld-940 (2021-02-09)
    DYLD_1042_1,  ///< dyld-1042.1 (2022-10-19)
    DYLD_1231_3,  ///< dyld-1231.3 (2024-09-24)
    DYLD_1284_13, ///< dyld-1284.13 (2025-04-25)

    /// This value is used for versions of dyld not publicly released or not yet
    /// supported by LIEF
    UNRELEASED,
  };

  /// Platforms supported by the dyld shared cache
  enum class DYLD_TARGET_PLATFORM : uint32_t {
    UNKNOWN             = 0,
    MACOS               = 1,
    IOS                 = 2,
    TVOS                = 3,
    WATCHOS             = 4,
    BRIDGEOS            = 5,
    IOSMAC              = 6,
    IOS_SIMULATOR       = 7,
    TVOS_SIMULATOR      = 8,
    WATCHOS_SIMULATOR   = 9,
    DRIVERKIT           = 10,
    VISIONOS            = 11,
    VISIONOS_SIMULATOR  = 12,
    FIRMWARE            = 13,
    SEPOS               = 14,

    ANY                = 0xFFFFFFFF
  };

  /// Architecture supported by the dyld shared cache
  enum class DYLD_TARGET_ARCH {
    UNKNOWN = 0,

    I386,

    X86_64,
    X86_64H,

    ARMV5,
    ARMV6,
    ARMV7,

    ARM64,
    ARM64E,
  };

  /// Iterator over the libraries in the shared cache
  using dylib_iterator = iterator_range<Dylib::Iterator>;

  /// Iterator over the mapping info in the shared cache
  using mapping_info_iterator = iterator_range<MappingInfo::Iterator>;

  /// Iterator over the split/sub-cache in this **main** shared cache
  using subcache_iterator = iterator_range<SubCache::Iterator>;

  using instructions_iterator = iterator_range<assembly::Instruction::Iterator>;

  DyldSharedCache(std::unique_ptr<details::DyldSharedCache> impl);
  ~DyldSharedCache();

  /// See the \ref load functions for the details
  static std::unique_ptr<DyldSharedCache> from_path(const std::string& path,
                                                    const std::string& arch = "");

  /// See the \ref load functions for the details
  static std::unique_ptr<DyldSharedCache> from_files(const std::vector<std::string>& path);

  /// Filename of the dyld shared file associated with this object.
  ///
  /// For instance: `dyld_shared_cache_arm64e, dyld_shared_cache_arm64e.62.dyldlinkedit`
  std::string filename() const;

  /// Version of dyld used by this cache
  VERSION version() const;

  /// Full path to the original dyld shared cache file associated with object
  /// (e.g. `/home/lief/downloads/visionos/dyld_shared_cache_arm64e.42`)
  std::string filepath() const;

  /// Based address of this cache
  uint64_t load_address() const;

  /// Name of the architecture targeted by this cache (`x86_64h`)
  std::string arch_name() const;

  /// Platform targeted by this cache (e.g. vision-os)
  DYLD_TARGET_PLATFORM platform() const;

  /// Architecture targeted by this cache
  DYLD_TARGET_ARCH arch() const;

  /// Find the Dylib that encompasses the given virtual address.
  /// It returns a nullptr if a Dylib can't be found.
  std::unique_ptr<Dylib> find_lib_from_va(uint64_t va) const;

  /// Find the Dylib whose Dylib::path matches the provided path.
  std::unique_ptr<Dylib> find_lib_from_path(const std::string& path) const;

  /// Find the Dylib whose filename of Dylib::path matches the provided name.
  ///
  /// If multiple libraries have the same name (but with a different path),
  /// the **first one** matching the provided name is returned.
  std::unique_ptr<Dylib> find_lib_from_name(const std::string& name) const;

  /// True if the subcaches are associated with this cache
  bool has_subcaches() const;

  /// Return an interator over the libraries embedded in this dyld shared cache.
  ///
  /// This iterator implements the *random access* trait. Thus, one can use
  /// iterator_range::size, iterator_range::at, iterator_range::operator[] to a
  /// access Dylib at an arbitrary index:
  ///
  /// ```cpp
  /// auto libraries = cache.libaries();
  /// for (size_t i = 0; i < libraries.size(); ++i) {
  ///   std::string path = libaries[i]->path();
  /// }
  /// ```
  dylib_iterator libraries() const;

  /// Return an interator over the mapping information of this dyld shared cache.
  ///
  /// This iterator implements the *random access* trait. Thus, one can use
  /// iterator_range::size, iterator_range::at, iterator_range::operator[] to
  /// access a MappingInfo at an arbitrary index:
  ///
  /// ```cpp
  /// auto mapping = cache.mapping_info();
  /// for (size_t i = 0; i < mapping.size(); ++i) {
  ///   const uint64_t addr = mapping[i]->address();
  /// }
  /// ```
  mapping_info_iterator mapping_info() const;

  /// Return an interator over the subcaches associated with this (main) dyld shared
  /// cache.
  ///
  /// This iterator implements the *random access* trait. Thus, one can use
  /// iterator_range::size, iterator_range::at, iterator_range::operator[] to
  /// access a SubCache at an arbitrary index:
  ///
  /// ```cpp
  /// auto subcaches = cache.subcaches();
  /// for (size_t i = 0; i < subcaches.size(); ++i) {
  ///   std::unique_ptr<DyldSharedCache> impl = subcaches[i]->cache();
  /// }
  /// ```
  subcache_iterator subcaches() const;

  /// Disassemble instructions at the provided virtual address.
  /// This function returns an iterator over assembly::Instruction.
  instructions_iterator disassemble(uint64_t va) const;

  /// Return the content at the specified virtual address
  std::vector<uint8_t> get_content_from_va(uint64_t va, uint64_t size) const;

  /// Find the sub-DyldSharedCache that wraps the given virtual address
  std::unique_ptr<DyldSharedCache> cache_for_address(uint64_t va) const;

  /// Return the principal dyld shared cache in the case of multiple subcaches
  std::unique_ptr<DyldSharedCache> main_cache() const;

  /// Try to find the DyldSharedCache associated with the filename given
  /// in the first parameter.
  std::unique_ptr<DyldSharedCache> find_subcache(const std::string& filename) const;

  /// Convert the given virtual address into an offset.
  ///
  /// \warning If the shared cache contains multiple subcaches,
  ///          this function needs to be called on the targeted subcache.
  ///          See cache_for_address() to find the associated subcache.
  result<uint64_t> va_to_offset(uint64_t va) const;

  /// Return the stream associated with this dyld shared cache
  FileStream& stream() const;

  /// Return the stream associated with this dyld shared cache
  FileStream& stream();

  /// When enabled, this function allows to record and to keep in *cache*,
  /// dyld shared cache information that are costly to access.
  ///
  /// For instance, GOT symbols, rebases information, stub symbols, ...
  ///
  /// It is **highly** recommended to enable this function when processing
  /// a dyld shared cache several times or when extracting a large number of
  /// LIEF::dsc::Dylib with enhanced extraction options (e.g. Dylib::extract_opt_t::fix_branches)
  ///
  /// One can enable caching by calling this function:
  ///
  /// ```cpp
  /// auto dyld_cache = LIEF::dsc::load("macos-15.0.1/");
  /// dyld_cache->enable_caching("/home/user/.cache/lief-dsc");
  /// ```
  ///
  /// One can also enable this cache optimization **globally** using the
  /// function: LIEF::dsc::enable_cache or by setting the environment variable
  /// `DYLDSC_ENABLE_CACHE` to 1.
  void enable_caching(const std::string& target_cache_dir) const;

  /// Flush internal information into the on-disk cache (see: enable_caching)
  void flush_cache() const;

  private:
  std::unique_ptr<details::DyldSharedCache> impl_;
};

/// Load a shared cache from a single file or from a directory specified
/// by the \p path parameter.
///
/// In the case where multiple architectures are
/// available in the \p path directory, the \p arch parameter can be used to
/// define which architecture should be prefered.
///
/// **Example:**
///
/// ```cpp
/// // From a directory (split caches)
/// auto cache = LIEF::dsc::load("vision-pro-2.0/");
///
/// // From a single cache file
/// auto cache = LIEF::dsc::load("ios-14.2/dyld_shared_cache_arm64");
///
/// // From a directory with multiple architectures
/// auto cache = LIEF::dsc::load("macos-12.6/", /*arch=*/"x86_64h");
/// ```
inline std::unique_ptr<DyldSharedCache> load(const std::string& path,
                                             const std::string& arch = "")
{
  return DyldSharedCache::from_path(path, arch);
}

/// Load a shared cache from a list of files.
///
/// ```cpp
/// std::vector<std::string> files = {
///   "/tmp/dsc/dyld_shared_cache_arm64e",
///   "/tmp/dsc/dyld_shared_cache_arm64e.1"
/// };
/// auto cache = LIEF::dsc::load(files);
/// ```
inline std::unique_ptr<DyldSharedCache> load(const std::vector<std::string>& files)
{
  return DyldSharedCache::from_files(files);
}

}
}
#endif


