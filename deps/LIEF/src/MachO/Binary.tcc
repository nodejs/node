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
#include "logging.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/LoadCommand.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/Relocation.hpp"
#include "LIEF/errors.hpp"
#include "internal_utils.hpp"

namespace LIEF {
namespace MachO {

template<class T>
bool Binary::has_command() const {
  return command<T>() != nullptr;
}

template<class T>
T* Binary::command() {
  static_assert(std::is_base_of<LoadCommand, T>::value, "Require inheritance of 'LoadCommand'");
  return const_cast<T*>(static_cast<const Binary*>(this)->command<T>());
}

template<class T>
const T* Binary::command() const {
  static_assert(std::is_base_of<LoadCommand, T>::value, "Require inheritance of 'LoadCommand'");
  const auto it_cmd = std::find_if(
      std::begin(commands_), std::end(commands_),
      [] (const std::unique_ptr<LoadCommand>& command) {
        return T::classof(command.get());
      });

  if (it_cmd == std::end(commands_)) {
    return nullptr;
  }

  return reinterpret_cast<const T*>(it_cmd->get());

}

template<class T>
size_t Binary::count_commands() const {
  static_assert(std::is_base_of<LoadCommand, T>::value, "Require inheritance of 'LoadCommand'");

  size_t nb_cmd = std::count_if(
      std::begin(commands_), std::end(commands_),
      [] (const std::unique_ptr<LoadCommand>& command) {
        return T::classof(command.get());
      });
  return nb_cmd;
}

template<class CMD, class Func>
Binary& Binary::for_commands(Func f) {
  static_assert(std::is_base_of<LoadCommand, CMD>::value, "Require inheritance of 'LoadCommand'");
  for (const std::unique_ptr<LoadCommand>& cmd : commands_) {
    if (!CMD::classof(cmd.get())) {
      continue;
    }
    f(*cmd->as<CMD>());
  }
  return *this;
}

template<class T>
ok_error_t Binary::patch_relocation(Relocation& relocation, uint64_t from, uint64_t shift) {

  SegmentCommand* segment = segment_from_virtual_address(relocation.address());
  if (segment == nullptr) {
    LIEF_DEBUG("Can't find the segment associated with the relocation: 0x{:x}",
               relocation.address());
    return make_error_code(lief_errors::not_found);
  }
  auto offset = virtual_address_to_offset(relocation.address());
  if (!offset) {
    return make_error_code(offset.error());
  }
  uint64_t relative_offset = *offset - segment->file_offset();
  span<uint8_t> segment_content = segment->writable_content();
  const size_t segment_size = segment_content.size();

  if (segment_size == 0) {
    LIEF_WARN("Segment is empty nothing to do");
    return ok();
  }

  if (relative_offset >= segment_size || (relative_offset + sizeof(T)) >= segment_size) {
    LIEF_DEBUG("Offset out of bound for relocation: {}", to_string(relocation));
    return make_error_code(lief_errors::read_out_of_bound);
  }

  auto* ptr_value = reinterpret_cast<T*>(segment_content.data() + relative_offset);
  if (*ptr_value >= from && is_valid_addr(*ptr_value)) {
    *ptr_value += shift;
  }
  return ok();
}


}
}
