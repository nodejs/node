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
#ifndef LIEF_MACHO_FUNCTION_VARIANT_FIXUPS_COMMAND_H
#define LIEF_MACHO_FUNCTION_VARIANT_FIXUPS_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {
class BinaryParser;
class LinkEdit;

namespace details {
struct linkedit_data_command;
}

/// Class which represents the `LC_FUNCTION_VARIANT_FIXUPS` command
class LIEF_API FunctionVariantFixups : public LoadCommand {
  friend class BinaryParser;
  friend class LinkEdit;

  public:
  FunctionVariantFixups() = default;
  FunctionVariantFixups(const details::linkedit_data_command& cmd);

  FunctionVariantFixups& operator=(const FunctionVariantFixups& copy) = default;
  FunctionVariantFixups(const FunctionVariantFixups& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<FunctionVariantFixups>(new FunctionVariantFixups(*this));
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

  span<const uint8_t> content() const {
    return content_;
  }

  span<uint8_t> content() {
    return content_;
  }


  ~FunctionVariantFixups() override = default;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::FUNCTION_VARIANT_FIXUPS;
  }

  private:
  uint32_t data_offset_ = 0;
  uint32_t data_size_ = 0;
  span<uint8_t> content_;
};

}
}
#endif
