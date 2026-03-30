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
#ifndef LIEF_ELF_CORE_SIGINFO_H
#define LIEF_ELF_CORE_SIGINFO_H

#include <ostream>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/ELF/Note.hpp"
#include "LIEF/errors.hpp"

namespace LIEF {
namespace ELF {

/// Class representing a core siginfo object
class LIEF_API CoreSigInfo : public Note {
  public:
  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<CoreSigInfo>(new CoreSigInfo(*this));
  }

  /// Signal number of an error if it can't be resolved
  result<int32_t> signo() const;
  /// Signal code of an error if it can't be resolved
  result<int32_t> sigcode() const;

  /// Signal error number of an error if it can't be resolved
  result<int32_t> sigerrno() const;

  void signo(uint32_t value);
  void sigcode(uint32_t value);
  void sigerrno(uint32_t value);

  void dump(std::ostream& os) const override;
  void accept(Visitor& visitor) const override;

  ~CoreSigInfo() override = default;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::CORE_SIGINFO;
  }

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const CoreSigInfo& note) {
    note.dump(os);
    return os;
  }
  protected:
  using Note::Note;
};
} // namepsace ELF
} // namespace LIEF

#endif
