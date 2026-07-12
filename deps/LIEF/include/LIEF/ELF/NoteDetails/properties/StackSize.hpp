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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_STACK_SIZE_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_STACK_SIZE_H

#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace ELF {
/// This class provides an interface over the `GNU_PROPERTY_STACK_SIZE` property
///
/// This property can be used by the loader to raise the stack limit.
class LIEF_API StackSize : public NoteGnuProperty::Property {
  public:
  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::STACK_SIZE;
  }

  static std::unique_ptr<StackSize> create(uint64_t stack_size) {
    return std::unique_ptr<StackSize>(new StackSize(stack_size));
  }

  /// The indicated stack size
  uint64_t stack_size() const {
    return stack_size_;
  }

  void dump(std::ostream &os) const override;

  ~StackSize() override = default;

  protected:
  StackSize(uint64_t stack_size) :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::STACK_SIZE),
    stack_size_(stack_size)
  {}
  uint64_t stack_size_ = 0;
};
}
}

#endif
