/* Copyright 2022 - 2026 R. Thomas
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
#ifndef LIEF_DWARF_LEXICAL_BLOCK_H
#define LIEF_DWARF_LEXICAL_BLOCK_H

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/optional.hpp"
#include "LIEF/range.hpp"

#include <memory>
#include <string>
#include <cstdint>

namespace LIEF {
namespace dwarf {

namespace details {
class LexicalBlock;
class LexicalBlockIt;
}

/// This class represents a DWARF lexical block (`DW_TAG_lexical_block`)
class LIEF_API LexicalBlock {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  LexicalBlock, std::ptrdiff_t,
                                  const LexicalBlock*, const LexicalBlock&> {
    public:
    using implementation = details::LexicalBlockIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::LexicalBlockIt> impl);

    LIEF_API Iterator(const Iterator&);
    LIEF_API Iterator& operator=(const Iterator&);

    LIEF_API Iterator(Iterator&&) noexcept;
    LIEF_API Iterator& operator=(Iterator&&) noexcept;

    LIEF_API ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);
    friend bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator++();

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator--();

    LIEF_API const LexicalBlock& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const LexicalBlock* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the lexical block at the current position
    /// to the caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<LexicalBlock> yield();

    private:
    void load() const;

    std::unique_ptr<details::LexicalBlockIt> impl_;
    mutable std::unique_ptr<LexicalBlock> cached_;
  };

  using sub_blocks_it = iterator_range<Iterator>;

  LexicalBlock(std::unique_ptr<details::LexicalBlock> impl);
  LexicalBlock() = delete;
  LexicalBlock& operator=(const LexicalBlock&) = delete;
  LexicalBlock(const LexicalBlock&) = delete;

  /// Return the *name* associated with this lexical block or an
  /// empty string
  std::string name() const;

  /// Return the description associated with this lexical block or an
  /// empty string
  std::string description() const;

  /// Return an iterator over the sub-LexicalBlock owned by this block.
  sub_blocks_it sub_blocks() const LIEF_LIFETIMEBOUND;

  /// Return the start address of this block
  optional<uint64_t> addr() const;

  /// Return the size of this block as the difference of the highest address and
  /// the lowest address.
  uint64_t size() const;

  /// Return the lowest virtual address owned by this block.
  optional<uint64_t> low_pc() const;

  /// Return the highest virtual address owned by this block.
  optional<uint64_t> high_pc() const;

  /// Return a list of address ranges owned by this block.
  ///
  /// If the lexical block owns a contiguous range, it should return
  /// **a single** range.
  std::vector<range_t> ranges() const;

  ~LexicalBlock();

  LIEF_LOCAL static std::unique_ptr<LexicalBlock>
      create(std::unique_ptr<details::LexicalBlock> impl);

  protected:
  std::unique_ptr<details::LexicalBlock> impl_;
};

}
}
#endif
