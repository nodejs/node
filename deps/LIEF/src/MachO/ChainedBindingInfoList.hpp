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
#ifndef LIEF_MACHO_CHAINED_BINDING_INFO_LIST_H
#define LIEF_MACHO_CHAINED_BINDING_INFO_LIST_H

#include <vector>
#include <memory>
#include "LIEF/MachO/ChainedBindingInfo.hpp"

namespace LIEF {
namespace MachO {
class BinaryParser;
class Builder;
class DyldChainedFixupsCreator;

class ChainedBindingInfoList : public ChainedBindingInfo {

  friend class BinaryParser;
  friend class Builder;
  friend class DyldChainedFixupsCreator;

  public:
  static std::unique_ptr<ChainedBindingInfoList>
    create(const ChainedBindingInfo& other);

  ChainedBindingInfoList() = delete;
  explicit ChainedBindingInfoList(DYLD_CHAINED_FORMAT fmt, bool is_weak) :
    ChainedBindingInfo(fmt, is_weak)
  {}


  ChainedBindingInfoList& operator=(ChainedBindingInfoList other) = delete;
  ChainedBindingInfoList(const ChainedBindingInfoList& other) = delete;

  ChainedBindingInfoList(ChainedBindingInfoList&&) noexcept = default;

  void swap(ChainedBindingInfoList& other) noexcept;

  ~ChainedBindingInfoList() override = default;

  static bool classof(const BindingInfo& info) {
    return info.type() == BindingInfo::TYPES::CHAINED_LIST;
  }

  private:
  std::vector<ChainedBindingInfo*> elements_;
};

}
}
#endif
