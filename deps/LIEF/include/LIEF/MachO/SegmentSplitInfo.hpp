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
#ifndef LIEF_MACHO_SEGMENT_SPLIT_INFO_H
#define LIEF_MACHO_SEGMENT_SPLIT_INFO_H
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

/// Class that represents the LoadCommand::TYPE::SEGMENT_SPLIT_INFO command
class LIEF_API SegmentSplitInfo : public LoadCommand {
  friend class BinaryParser;
  friend class LinkEdit;
  public:
  SegmentSplitInfo() = default;
  SegmentSplitInfo(const details::linkedit_data_command& cmd);

  SegmentSplitInfo& operator=(const SegmentSplitInfo& copy) = default;
  SegmentSplitInfo(const SegmentSplitInfo& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<SegmentSplitInfo>(new SegmentSplitInfo(*this));
  }

  uint32_t data_offset() const {
    return data_offset_;
  }
  uint32_t data_size() const {
    return data_size_;
  }

  void data_offset(uint32_t offset) {
    data_offset_ = offset;
  }
  void data_size(uint32_t size) {
    data_size_ = size;
  }

  span<uint8_t> content() {
    return content_;
  }

  span<const uint8_t> content() const {
    return content_;
  }

  ~SegmentSplitInfo() override = default;

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::SEGMENT_SPLIT_INFO;
  }

  private:
  uint32_t data_offset_ = 0;
  uint32_t data_size_   = 0;
  span<uint8_t> content_;

};

}
}
#endif
