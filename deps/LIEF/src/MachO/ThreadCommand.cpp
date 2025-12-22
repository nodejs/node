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
#include <iomanip>

#include "logging.hpp"
#include "LIEF/Visitor.hpp"

#include "LIEF/MachO/ThreadCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

ThreadCommand::ThreadCommand(const details::thread_command& cmd, Header::CPU_TYPE arch) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  flavor_{cmd.flavor},
  count_{cmd.count},
  architecture_{arch}
{}

ThreadCommand::ThreadCommand(uint32_t flavor, uint32_t count, Header::CPU_TYPE arch) :
  LoadCommand::LoadCommand{LoadCommand::TYPE::UNIXTHREAD,
                           static_cast<uint32_t>(sizeof(details::thread_command) + count * sizeof(uint32_t))},
  flavor_{flavor},
  count_{count},
  architecture_{arch},
  state_(size() - sizeof(details::thread_command), 0)
{
}

uint64_t ThreadCommand::pc() const {
  uint64_t entry = 0;
  switch(architecture_) {
    case Header::CPU_TYPE::X86:
      {
        if (state_.size() < sizeof(details::x86_thread_state_t)) {
          return entry;
        }
        entry = reinterpret_cast<const details::x86_thread_state_t*>(state_.data())->eip;
        break;
      }

    case Header::CPU_TYPE::X86_64:
      {
        if (state_.size() < sizeof(details::x86_thread_state64_t)) {
          return entry;
        }
        entry = reinterpret_cast<const details::x86_thread_state64_t*>(state_.data())->rip;
        break;
      }

    case Header::CPU_TYPE::ARM:
      {
        if (state_.size() < sizeof(details::arm_thread_state_t)) {
          return entry;
        }
        entry = reinterpret_cast<const details::arm_thread_state_t*>(state_.data())->r15;
        break;
      }

    case Header::CPU_TYPE::ARM64:
      {
        if (state_.size() < sizeof(details::arm_thread_state64_t)) {
          return entry;
        }
        entry = reinterpret_cast<const details::arm_thread_state64_t*>(state_.data())->pc;
        break;
      }

    case Header::CPU_TYPE::POWERPC:
      {
        if (state_.size() < sizeof(details::ppc_thread_state_t)) {
          return entry;
        }
        entry = reinterpret_cast<const details::ppc_thread_state_t*>(state_.data())->srr0;
        break;
      }

    case Header::CPU_TYPE::POWERPC64:
      {
        if (state_.size() < sizeof(details::ppc_thread_state64_t)) {
          return entry;
        }
        entry = reinterpret_cast<const details::ppc_thread_state64_t*>(state_.data())->srr0;
        break;
      }

    default:
      {
        LIEF_ERR("Unknown architecture");
      }
  }
  return entry;
}

void ThreadCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& ThreadCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("flavor=0x{:x}, count=0x{:x}, pc=0x{:06x}",
                    flavor(), count(), pc());
  return os;
}


}
}
