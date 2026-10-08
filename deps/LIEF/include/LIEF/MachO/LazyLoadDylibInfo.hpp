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
#ifndef LIEF_MACHO_LAZY_LOAD_DYLIB_INFO_COMMAND_H
#define LIEF_MACHO_LAZY_LOAD_DYLIB_INFO_COMMAND_H
#include <string>
#include <vector>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
class SpanStream;
class BinaryStream;
class vector_iostream;

namespace MachO {
class Binary;
class BinaryParser;
class Builder;
class LinkEdit;
class SegmentCommand;

namespace details {
struct linkedit_data_command;
}

/// Class representing the `LC_LAZY_LOAD_DYLIB_INFO` load command.
///
/// This command describes how to **lazily load a dylib**: instead of binding
/// the library and its symbols at launch time, `dyld` keeps the information
/// required to resolve the dylib on the first use of one of its symbols.
class LIEF_API LazyLoadDylibInfo : public LoadCommand {
  friend class BinaryParser;
  friend class Builder;
  friend class LinkEdit;

  public:
  /// A single lazy-binding fixup decoded from the chain referenced by
  /// chain_start_image_offset and decoded according to pointer_format
  class LIEF_API Fixup {
    friend class Binary;

    public:
    Fixup() = default;
    Fixup(uint64_t address, uint32_t ordinal, std::string symbol, bool is_auth) :
      address_(address),
      ordinal_(ordinal),
      symbol_(std::move(symbol)),
      is_auth_(is_auth) {}

    Fixup(const Fixup&) = default;
    Fixup& operator=(const Fixup&) = default;

    Fixup(Fixup&&) noexcept = default;
    Fixup& operator=(Fixup&&) noexcept = default;

    ~Fixup() = default;

    /// Virtual address of the slot bound by this fixup
    uint64_t address() const {
      return address_;
    }

    void address(uint64_t value) {
      address_ = value;
    }

    /// Index of the bound symbol in the symbols table of LazyLoadDylibInfo
    uint32_t ordinal() const {
      return ordinal_;
    }

    /// Name of the bound symbol, resolved from ordinal() (empty if the ordinal
    /// is out of the symbols() range)
    const std::string& symbol() const LIEF_LIFETIMEBOUND {
      return symbol_;
    }

    /// Whether the bound pointer is authenticated (`arm64e` PAC)
    bool is_auth() const {
      return is_auth_;
    }

    std::string to_string() const;

    LIEF_API friend std::ostream& operator<<(std::ostream& os,
                                             const Fixup& fixup) {
      os << fixup.to_string();
      return os;
    }

    private:
    uint64_t address_ = 0;
    uint32_t ordinal_ = 0;
    std::string symbol_;
    bool is_auth_ = false;
  };
  static constexpr auto MAYBE_MISSING_FLAG = 1;

  using fixups_t = std::vector<Fixup>;

  /// Iterator that outputs Fixup&
  using it_fixups = ref_iterator<fixups_t&>;

  /// Iterator that outputs const Fixup&
  using it_const_fixups = const_ref_iterator<const fixups_t&>;

  LazyLoadDylibInfo();
  LazyLoadDylibInfo(const details::linkedit_data_command& cmd);

  LazyLoadDylibInfo& operator=(const LazyLoadDylibInfo& copy) = default;
  LazyLoadDylibInfo(const LazyLoadDylibInfo& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<LazyLoadDylibInfo>(new LazyLoadDylibInfo(*this));
  }

  /// Offset in the `__LINKEDIT` segment where the payload starts
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
  /// data_offset and data_size.
  span<const uint8_t> content() const LIEF_LIFETIMEBOUND {
    return content_;
  }

  span<uint8_t> content() LIEF_LIFETIMEBOUND {
    return content_;
  }

  /// Load path of the dylib to bind lazily
  const std::string& load_path() const LIEF_LIFETIMEBOUND {
    return load_path_;
  }

  /// Change the load path of the dylib to bind lazily
  LazyLoadDylibInfo& load_path(std::string value) LIEF_LIFETIMEBOUND {
    load_path_ = std::move(value);
    return *this;
  }

  /// Image offset of the global flag that is set once the dylib has been
  /// loaded by dyld
  uint32_t flag_image_offset() const {
    return flag_image_offset_;
  }

  LazyLoadDylibInfo& flag_image_offset(uint32_t value) LIEF_LIFETIMEBOUND {
    flag_image_offset_ = value;
    return *this;
  }

  /// Raw flags associated with this command
  uint16_t flags() const {
    return flags_;
  }

  LazyLoadDylibInfo& flags(uint16_t value) LIEF_LIFETIMEBOUND {
    flags_ = value;
    return *this;
  }

  /// Whether the dylib is allowed to be missing at runtime (i.e. "weak
  /// linked")
  bool may_be_missing() const {
    return (flags_ & MAYBE_MISSING_FLAG) != 0;
  }

  /// Set or clear the "may be missing" (weak linked) bit of flags().
  LazyLoadDylibInfo& may_be_missing(bool value) LIEF_LIFETIMEBOUND {
    if (value) {
      flags_ |= MAYBE_MISSING_FLAG;
    } else {
      flags_ &= ~static_cast<uint16_t>(MAYBE_MISSING_FLAG);
    }
    return *this;
  }

  /// Chained-fixups pointer format used by the binding chain
  /// (e.g. `DYLD_CHAINED_PTR_ARM64E_USERLAND`)
  uint16_t pointer_format() const {
    return pointer_format_;
  }

  LazyLoadDylibInfo& pointer_format(uint16_t value) LIEF_LIFETIMEBOUND {
    pointer_format_ = value;
    return *this;
  }

  /// Image offset of the fixup chain start used to bind the dylib's symbols
  uint32_t chain_start_image_offset() const {
    return chain_start_image_offset_;
  }

  LazyLoadDylibInfo& chain_start_image_offset(uint32_t value) LIEF_LIFETIMEBOUND {
    chain_start_image_offset_ = value;
    return *this;
  }

  /// List of the symbol names to bind lazily for this dylib
  const std::vector<std::string>& symbols() const LIEF_LIFETIMEBOUND {
    return symbols_;
  }

  /// Replace the list of the symbol names to bind lazily for this dylib.
  LazyLoadDylibInfo& symbols(std::vector<std::string> value) LIEF_LIFETIMEBOUND {
    symbols_ = std::move(value);
    return *this;
  }

  /// Append a symbol name to the list of symbols to bind lazily.
  LazyLoadDylibInfo& add_symbol(std::string value) LIEF_LIFETIMEBOUND {
    symbols_.push_back(std::move(value));
    return *this;
  }

  /// Remove all the symbol names to bind lazily.
  LazyLoadDylibInfo& clear_symbols() LIEF_LIFETIMEBOUND {
    symbols_.clear();
    return *this;
  }

  /// Iterator over the lazy-binding Fixup entries
  it_fixups fixups() LIEF_LIFETIMEBOUND {
    return fixups_;
  }

  it_const_fixups fixups() const LIEF_LIFETIMEBOUND {
    return fixups_;
  }

  ~LazyLoadDylibInfo() override = default;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::LAZY_LOAD_DYLIB_INFO;
  }

  /// \private
  LIEF_LOCAL ok_error_t parse_payload(BinaryStream& stream);

  /// \private
  LIEF_LOCAL ok_error_t walk_fixups(uint64_t chain_va, SegmentCommand& seg);

  /// \private
  LIEF_LOCAL ok_error_t serialize(vector_iostream& ios) const;


  private:
  uint32_t data_offset_ = 0;
  uint32_t data_size_ = 0;
  span<uint8_t> content_;

  std::string load_path_;
  uint32_t flag_image_offset_ = 0;
  uint16_t flags_ = 0;
  uint16_t pointer_format_ = 0;
  uint32_t chain_start_image_offset_ = 0;
  std::vector<std::string> symbols_;
  fixups_t fixups_;
};

}
}
#endif
