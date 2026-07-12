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
#ifndef LIEF_ELF_QNX_STACK_H
#define LIEF_ELF_QNX_STACK_H

#include <ostream>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/ELF/Note.hpp"

namespace LIEF {
namespace ELF {

/// Class representing the QNX `QNT_STACK` note
class LIEF_API QNXStack : public Note {
  public:
  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<QNXStack>(new QNXStack(*this));
  }

  /// Size of the stack
  uint32_t stack_size() const;

  /// Size of the stack pre-allocated (upfront)
  uint32_t stack_allocated() const;

  /// Whether the stack is executable
  bool is_executable() const;

  void stack_size(uint32_t value);
  void stack_allocated(uint32_t value);
  void set_is_executable(bool value);

  void dump(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::QNX_STACK;
  }

  ~QNXStack() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const QNXStack& note) {
    note.dump(os);
    return os;
  }
  protected:
  using Note::Note;
};


} // namepsace ELF
} // namespace LIEF

#endif
