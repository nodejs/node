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
#ifndef LIEF_MACHO_ENCRYPTION_INFO_COMMAND_H
#define LIEF_MACHO_ENCRYPTION_INFO_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct encryption_info_command;
}

/// Class that represents the LC_ENCRYPTION_INFO / LC_ENCRYPTION_INFO_64 commands
///
/// The encryption info is usually present in Mach-O executables that
/// target iOS to encrypt some sections of the binary
class LIEF_API EncryptionInfo : public LoadCommand {
  public:
  EncryptionInfo() = default;
  EncryptionInfo(const details::encryption_info_command& cmd);

  EncryptionInfo& operator=(const EncryptionInfo& copy) = default;
  EncryptionInfo(const EncryptionInfo& copy) = default;

  ~EncryptionInfo() override = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<EncryptionInfo>(new EncryptionInfo(*this));
  }

  /// The beginning of the encrypted area
  uint32_t crypt_offset() const {
    return coff_;
  }

  /// The size of the encrypted area
  uint32_t crypt_size() const {
    return csize_;
  }

  /// The encryption system. 0 means no encrypted
  uint32_t crypt_id() const {
    return cid_;
  }

  void crypt_offset(uint32_t offset) {
    coff_ = offset;
  }

  void crypt_size(uint32_t size) {
    csize_ = size;
  }
  void crypt_id(uint32_t id) {
    cid_ = id;
  }

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    const LoadCommand::TYPE type = cmd->command();
    return type == LoadCommand::TYPE::ENCRYPTION_INFO ||
           type == LoadCommand::TYPE::ENCRYPTION_INFO_64;
  }

  private:
  uint32_t coff_ = 0;
  uint32_t csize_ = 0;
  uint32_t cid_ = 0;
};

}
}
#endif
