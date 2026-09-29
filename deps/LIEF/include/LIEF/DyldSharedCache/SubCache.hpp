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
#ifndef LIEF_DSC_SUBCACHE_H
#define LIEF_DSC_SUBCACHE_H
#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/DyldSharedCache/uuid.hpp"

#include <memory>
#include <string>

namespace LIEF {
namespace dsc {
class DyldSharedCache;

namespace details {
class SubCache;
class SubCacheIt;
}

/// This class represents a subcache in the case of large/split dyld shared
/// cache.
///
/// It mirrors (and abstracts) the original `dyld_subcache_entry` /
/// `dyld_subcache_entry_v1`
class LIEF_API SubCache {
  public:
  /// SubCache Iterator
  class LIEF_API Iterator
    : public iterator_facade_base<Iterator, std::random_access_iterator_tag,
                                  SubCache, std::ptrdiff_t, const SubCache*,
                                  const SubCache&> {
    public:
    using implementation = details::SubCacheIt;

    Iterator();

    Iterator(std::unique_ptr<details::SubCacheIt> impl);
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

    const SubCache& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    const SubCache* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the subcache at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    std::unique_ptr<SubCache> yield();

    private:
    void load() const;

    std::unique_ptr<details::SubCacheIt> impl_;
    mutable std::unique_ptr<SubCache> cached_;
  };

  public:
  SubCache(std::unique_ptr<details::SubCache> impl);
  ~SubCache();

  /// The uuid of the subcache file
  sc_uuid_t uuid() const;

  /// The offset of this subcache from the main cache base address
  uint64_t vm_offset() const;

  /// The file name suffix of the subCache file (e.g. `.25.data`,
  /// `.03.development`)
  std::string suffix() const;

  /// The associated DyldSharedCache object for this subcache
  std::unique_ptr<const DyldSharedCache> cache() const LIEF_LIFETIMEBOUND;

  friend LIEF_API std::ostream& operator<<(std::ostream& os,
                                           const SubCache& subcache);

  private:
  std::unique_ptr<details::SubCache> impl_;
};

}
}
#endif
