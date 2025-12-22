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
#ifndef LIEF_MACHO_LOAD_COMMAND_H
#define LIEF_MACHO_LOAD_COMMAND_H

#include <memory>
#include <vector>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

namespace LIEF {
namespace MachO {
class Builder;
class BinaryParser;
class Binary;

namespace details {
struct load_command;
}

/// Based class for the Mach-O load commands
class LIEF_API LoadCommand : public Object {
  friend class Builder;
  friend class BinaryParser;
  friend class Binary;
  public:
  using raw_t = std::vector<uint8_t>;

  enum class TYPE: uint64_t {
    UNKNOWN                  = 0,
    SEGMENT                  = 0x00000001u,
    SYMTAB                   = 0x00000002u,
    SYMSEG                   = 0x00000003u,
    THREAD                   = 0x00000004u,
    UNIXTHREAD               = 0x00000005u,
    LOADFVMLIB               = 0x00000006u,
    IDFVMLIB                 = 0x00000007u,
    IDENT                    = 0x00000008u,
    FVMFILE                  = 0x00000009u,
    PREPAGE                  = 0x0000000Au,
    DYSYMTAB                 = 0x0000000Bu,
    LOAD_DYLIB               = 0x0000000Cu,
    ID_DYLIB                 = 0x0000000Du,
    LOAD_DYLINKER            = 0x0000000Eu,
    ID_DYLINKER              = 0x0000000Fu,
    PREBOUND_DYLIB           = 0x00000010u,
    ROUTINES                 = 0x00000011u,
    SUB_FRAMEWORK            = 0x00000012u,
    SUB_UMBRELLA             = 0x00000013u,
    SUB_CLIENT               = 0x00000014u,
    SUB_LIBRARY              = 0x00000015u,
    TWOLEVEL_HINTS           = 0x00000016u,
    PREBIND_CKSUM            = 0x00000017u,
    LOAD_WEAK_DYLIB          = 0x80000018u,
    SEGMENT_64               = 0x00000019u,
    ROUTINES_64              = 0x0000001Au,
    UUID                     = 0x0000001Bu,
    RPATH                    = 0x8000001Cu,
    CODE_SIGNATURE           = 0x0000001Du,
    SEGMENT_SPLIT_INFO       = 0x0000001Eu,
    REEXPORT_DYLIB           = 0x8000001Fu,
    LAZY_LOAD_DYLIB          = 0x00000020u,
    ENCRYPTION_INFO          = 0x00000021u,
    DYLD_INFO                = 0x00000022u,
    DYLD_INFO_ONLY           = 0x80000022u,
    LOAD_UPWARD_DYLIB        = 0x80000023u,
    VERSION_MIN_MACOSX       = 0x00000024u,
    VERSION_MIN_IPHONEOS     = 0x00000025u,
    FUNCTION_STARTS          = 0x00000026u,
    DYLD_ENVIRONMENT         = 0x00000027u,
    MAIN                     = 0x80000028u,
    DATA_IN_CODE             = 0x00000029u,
    SOURCE_VERSION           = 0x0000002Au,
    DYLIB_CODE_SIGN_DRS      = 0x0000002Bu,
    ENCRYPTION_INFO_64       = 0x0000002Cu,
    LINKER_OPTION            = 0x0000002Du,
    LINKER_OPTIMIZATION_HINT = 0x0000002Eu,
    VERSION_MIN_TVOS         = 0x0000002Fu,
    VERSION_MIN_WATCHOS      = 0x00000030u,
    NOTE                     = 0x00000031u,
    BUILD_VERSION            = 0x00000032u,
    DYLD_EXPORTS_TRIE        = 0x80000033u,
    DYLD_CHAINED_FIXUPS      = 0x80000034u,
    FILESET_ENTRY            = 0x80000035u,
    ATOM_INFO                = 0x00000036u,
    FUNCTION_VARIANTS        = 0x00000037u,
    FUNCTION_VARIANT_FIXUPS  = 0x00000038u,
    TARGET_TRIPLE            = 0x00000039u,

    LIEF_UNKNOWN             = 0xffee0001u
  };

  public:
  LoadCommand() = default;
  LoadCommand(const details::load_command& command);
  LoadCommand(LoadCommand::TYPE type, uint32_t size) :
    command_(type),
    size_(size)
  {}

  LoadCommand& operator=(const LoadCommand& copy) = default;
  LoadCommand(const LoadCommand& copy) = default;

  void swap(LoadCommand& other) noexcept;

  virtual std::unique_ptr<LoadCommand> clone() const {
    return std::unique_ptr<LoadCommand>(new LoadCommand(*this));
  }

  ~LoadCommand() override = default;

  /// Command type
  LoadCommand::TYPE command() const {
    return command_;
  }

  /// Size of the command (should be greather than ``sizeof(load_command)``)
  uint32_t size() const {
    return size_;
  }

  /// Raw command
  span<const uint8_t> data() const {
    return original_data_;
  }

  /// Offset of the command within the *Load Command Table*
  uint64_t command_offset() const {
    return command_offset_;
  }

  void data(raw_t data) {
    original_data_ = std::move(data);
  }

  void command(LoadCommand::TYPE command) {
    command_ = command;
  }

  void size(uint32_t size) {
    size_ = size;
  }

  void command_offset(uint64_t offset) {
    command_offset_ = offset;
  }

  virtual std::ostream& print(std::ostream& os) const;

  void accept(Visitor& visitor) const override;

  static bool is_linkedit_data(const LoadCommand& cmd);

  template<class T>
  const T* cast() const {
    static_assert(std::is_base_of<LoadCommand, T>::value,
                  "Require LoadCommand inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* cast() {
    return const_cast<T*>(static_cast<const LoadCommand*>(this)->cast<T>());
  }


  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const LoadCommand& cmd) {
    return cmd.print(os);
  }

  protected:
  raw_t original_data_;
  LoadCommand::TYPE command_ = LoadCommand::TYPE::UNKNOWN;
  uint32_t size_ = 0;
  uint64_t command_offset_ = 0;
};

LIEF_API const char* to_string(LoadCommand::TYPE type);

}
}
#endif
