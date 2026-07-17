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
#include "LIEF/ELF/NoteDetails/QNXStack.hpp"
#include "LIEF/Visitor.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace ELF {

static constexpr auto stack_size_offset = 0;
static constexpr auto stack_allocated_offset = sizeof(uint32_t);
static constexpr auto is_stack_exec_offset = stack_allocated_offset + sizeof(uint32_t);

uint32_t QNXStack::stack_size() const {
  return read_at<uint32_t>(stack_size_offset).value_or(0);
}
uint32_t QNXStack::stack_allocated() const {
  return read_at<uint32_t>(stack_allocated_offset).value_or(0);
}

bool QNXStack::is_executable() const {
  return !(read_at<bool>(is_stack_exec_offset).value_or(true));
}

void QNXStack::stack_size(uint32_t value) {
  write_at(stack_size_offset, value);
}

void QNXStack::stack_allocated(uint32_t value) {
  write_at(stack_allocated_offset, value);
}

void QNXStack::set_is_executable(bool value) {
  write_at(is_stack_exec_offset, !value);
}

void QNXStack::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void QNXStack::dump(std::ostream& os) const {
  Note::dump(os);
  os << '\n';
  os << fmt::format("  Stack Size:      0x{:x}\n", stack_size())
     << fmt::format("  Stack allocated: 0x{:x}\n", stack_allocated())
     << fmt::format("  Executable:      {}\n", is_executable());
}

} // namespace ELF
} // namespace LIEF
