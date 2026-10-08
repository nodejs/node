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
#ifndef LIEF_MACHO_FUNCTION_VARIANT_FIXUPS_COMMAND_H
#define LIEF_MACHO_FUNCTION_VARIANT_FIXUPS_COMMAND_H
#include <vector>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"
#include "LIEF/iterators.hpp"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
class SpanStream;

namespace MachO {
class BinaryParser;
class LinkEdit;
class SegmentCommand;

namespace details {
struct linkedit_data_command;

// clang-format off
//
// On-disk entry of a `LC_FUNCTION_VARIANT_FIXUPS` payload
// (mirror of dyld's `FunctionVariantFixups::InternalFixup`)
struct function_variant_fixup_t {
  uint32_t seg_offset;
  uint32_t seg_index     : 4,
           variant_index : 8,
           pac_auth      : 1,
           pac_address   : 1,
           pac_key       : 2,
           pac_diversity : 16;
};
// clang-format on
}

/// Class which represents the `LC_FUNCTION_VARIANT_FIXUPS` command.
///
/// This command contains the relocations that must be applied to the GOT-like
/// slots associated with a FunctionVariants table. At runtime, `dyld` resolves
/// each slot to the best implementation and (re-)signs it according to the
/// pointer-authentication information.
class LIEF_API FunctionVariantFixups : public LoadCommand {
  friend class BinaryParser;
  friend class LinkEdit;

  public:
  /// A single relocation associated with a function-variant. It mirrors the
  /// `FunctionVariantFixups::InternalFixup` structure used by `dyld` and
  /// describes a slot that must be fixed up to point to the variant referenced by
  /// variant_index().
  class LIEF_API Fixup {
    public:
    Fixup() = default;
    Fixup(uint32_t seg_offset, uint32_t seg_index, uint32_t variant_index,
          bool pac_auth, bool pac_address, uint8_t pac_key,
          uint16_t pac_diversity) :
      seg_offset_(seg_offset),
      seg_index_(seg_index),
      variant_index_(variant_index),
      pac_auth_(pac_auth),
      pac_address_(pac_address),
      pac_key_(pac_key),
      pac_diversity_(pac_diversity) {}

    Fixup(const details::function_variant_fixup_t& raw);

    Fixup(const Fixup&) = default;
    Fixup& operator=(const Fixup&) = default;

    Fixup(Fixup&&) noexcept = default;
    Fixup& operator=(Fixup&&) noexcept = default;

    ~Fixup() = default;

    /// Offset of the slot to fix up, relative to the segment designated by
    /// seg_index()
    uint32_t seg_offset() const {
      return seg_offset_;
    }

    /// Index of the segment that owns the slot to fix up
    uint32_t seg_index() const {
      return seg_index_;
    }

    /// Index of the FunctionVariants runtime table used to resolve the slot
    uint32_t variant_index() const {
      return variant_index_;
    }

    /// Whether the slot is signed with pointer authentication (arm64e)
    bool pac_auth() const {
      return pac_auth_;
    }

    /// Whether the pointer-authentication signature mixes the storage address
    /// (address diversity)
    bool pac_address() const {
      return pac_address_;
    }

    /// Pointer-authentication key used to sign the slot
    uint8_t pac_key() const {
      return pac_key_;
    }

    /// Pointer-authentication diversity (discriminator) of the slot
    uint16_t pac_diversity() const {
      return pac_diversity_;
    }

    /// SegmentCommand referenced by seg_index() if it could be resolved
    SegmentCommand* segment() {
      return segment_;
    }

    const SegmentCommand* segment() const {
      return segment_;
    }

    void seg_offset(uint32_t value) {
      seg_offset_ = value;
    }

    void seg_index(uint32_t value) {
      seg_index_ = value;
    }

    void variant_index(uint32_t value) {
      variant_index_ = value;
    }

    void pac_auth(bool value) {
      pac_auth_ = value;
    }

    void pac_address(bool value) {
      pac_address_ = value;
    }

    void pac_key(uint8_t value) {
      pac_key_ = value;
    }

    void pac_diversity(uint16_t value) {
      pac_diversity_ = value;
    }

    void segment(SegmentCommand& seg) {
      segment_ = &seg;
    }

    std::string to_string() const;

    LIEF_API friend std::ostream& operator<<(std::ostream& os,
                                             const Fixup& fixup) {
      os << fixup.to_string();
      return os;
    }

    private:
    uint32_t seg_offset_ = 0;
    uint32_t seg_index_ = 0;
    uint32_t variant_index_ = 0;
    bool pac_auth_ = false;
    bool pac_address_ = false;
    uint8_t pac_key_ = 0;
    uint16_t pac_diversity_ = 0;
    SegmentCommand* segment_ = nullptr;
  };

  using fixups_t = std::vector<Fixup>;

  /// Iterator that outputs Fixup&
  using it_fixups = ref_iterator<fixups_t&>;

  /// Iterator that outputs const Fixup&
  using it_const_fixups = const_ref_iterator<const fixups_t&>;

  FunctionVariantFixups() = default;

  FunctionVariantFixups(FunctionVariantFixups&&) noexcept = default;
  FunctionVariantFixups& operator=(FunctionVariantFixups&&) noexcept = default;

  FunctionVariantFixups(const details::linkedit_data_command& cmd);

  FunctionVariantFixups& operator=(const FunctionVariantFixups& copy) = default;
  FunctionVariantFixups(const FunctionVariantFixups& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<FunctionVariantFixups>(
        new FunctionVariantFixups(*this)
    );
  }

  /// Offset in the `__LINKEDIT` SegmentCommand where the payload starts
  uint32_t data_offset() const {
    return data_offset_;
  }

  /// Size of the payload
  uint32_t data_size() const {
    return data_size_;
  }

  void data_offset(uint32_t offset) {
    data_offset_ = offset;
  }

  void data_size(uint32_t size) {
    data_size_ = size;
  }

  /// Return the data slice in the `__LINKEDIT` segment referenced by
  /// data_offset and data_size
  span<const uint8_t> content() const LIEF_LIFETIMEBOUND {
    return content_;
  }

  span<uint8_t> content() LIEF_LIFETIMEBOUND {
    return content_;
  }

  /// Iterator over the different Fixup entries
  it_fixups fixups() LIEF_LIFETIMEBOUND {
    return fixups_;
  }

  it_const_fixups fixups() const LIEF_LIFETIMEBOUND {
    return fixups_;
  }

  /// Append a new Fixup
  FunctionVariantFixups& add(Fixup fixup) {
    fixups_.push_back(fixup);
    return *this;
  }

  ~FunctionVariantFixups() override = default;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::FUNCTION_VARIANT_FIXUPS;
  }

  LIEF_LOCAL static std::vector<Fixup> parse_payload(SpanStream& stream);

  private:
  uint32_t data_offset_ = 0;
  uint32_t data_size_ = 0;
  span<uint8_t> content_;
  std::vector<Fixup> fixups_;
};

}
}
#endif
