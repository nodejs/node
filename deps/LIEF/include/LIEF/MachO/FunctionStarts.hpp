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
#ifndef LIEF_MACHO_FUNCTION_STARTS_COMMAND_H
#define LIEF_MACHO_FUNCTION_STARTS_COMMAND_H
#include <vector>
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/span.hpp"
#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {
class BinaryParser;
class LinkEdit;

namespace details {
struct linkedit_data_command;
}


/// Class which represents the LC_FUNCTION_STARTS command
///
/// This command is an array of ULEB128 encoded values
class LIEF_API FunctionStarts : public LoadCommand {
  friend class BinaryParser;
  friend class LinkEdit;

  public:
  FunctionStarts() = default;
  FunctionStarts(const details::linkedit_data_command& cmd);

  FunctionStarts& operator=(const FunctionStarts& copy) = default;
  FunctionStarts(const FunctionStarts& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<FunctionStarts>(new FunctionStarts(*this));
  }

  /// Offset in the ``__LINKEDIT`` SegmentCommand where *start functions* are located
  uint32_t data_offset() const {
    return data_offset_;
  }

  /// Size of the functions list in the binary
  uint32_t data_size() const {
    return data_size_;
  }

  /// Addresses of every function entry point in the executable.
  ///
  /// This allows functions to exist for which there are no entries in the symbol table.
  ///
  /// @warning The address is relative to the ``__TEXT`` segment
  const std::vector<uint64_t>& functions() const {
    return functions_;
  }

  std::vector<uint64_t>& functions() {
    return functions_;
  }

  /// Add a new function
  void add_function(uint64_t address) {
    functions_.emplace_back(address);
  }

  void data_offset(uint32_t offset) {
    data_offset_ = offset;
  }
  void data_size(uint32_t size) {
    data_size_ = size;
  }

  void functions(std::vector<uint64_t> funcs) {
    functions_ = std::move(funcs);
  }

  span<const uint8_t> content() const {
    return content_;
  }

  span<uint8_t> content() {
    return content_;
  }

  ~FunctionStarts() override = default;

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::FUNCTION_STARTS;
  }
  private:
  uint32_t data_offset_ = 0;
  uint32_t data_size_ = 0;
  span<uint8_t> content_;
  std::vector<uint64_t> functions_;
};

}
}
#endif
