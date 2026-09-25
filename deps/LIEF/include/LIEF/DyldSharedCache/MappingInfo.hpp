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
#ifndef LIEF_DSC_MAPPING_INFO_H
#define LIEF_DSC_MAPPING_INFO_H
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

#include <memory>
#include <cstdint>

namespace LIEF {
namespace dsc {

namespace details {
class MappingInfo;
class MappingInfoIt;
}

/// This class represents a `dyld_cache_mapping_info` entry.
///
/// It provides information about the relationshiop between on-disk shared cache
/// and in-memory shared cache.
class LIEF_API MappingInfo {
  public:
  /// MappingInfo Iterator
  class LIEF_API Iterator :
    public iterator_facade_base<Iterator, std::random_access_iterator_tag,
                                std::unique_ptr<MappingInfo>, std::ptrdiff_t, MappingInfo*,
                                std::unique_ptr<MappingInfo>>
  {
    public:
    using implementation = details::MappingInfoIt;

    Iterator(std::unique_ptr<details::MappingInfoIt> impl);
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

    std::unique_ptr<MappingInfo> operator*() const;

    private:
    std::unique_ptr<details::MappingInfoIt> impl_;
  };

  MappingInfo(std::unique_ptr<details::MappingInfo> impl);
  ~MappingInfo();

  /// The in-memory address where this dyld shared cache region is mapped
  uint64_t address() const;

  /// Size of the region being mapped
  uint64_t size() const;

  /// End virtual address of the region
  uint64_t end_address() const {
    return address() + size();
  }

  /// On-disk file offset
  uint64_t file_offset() const;

  /// Max memory protection
  uint32_t max_prot() const;

  /// Initial memory protection
  uint32_t init_prot() const;

  private:
  std::unique_ptr<details::MappingInfo> impl_;
};

}
}
#endif
