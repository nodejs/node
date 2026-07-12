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
#ifndef LIEF_MACHO_THREAD_COMMAND_H
#define LIEF_MACHO_THREAD_COMMAND_H
#include <vector>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

#include "LIEF/MachO/LoadCommand.hpp"
#include "LIEF/MachO/Header.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;

namespace details {
struct thread_command;
}

/// Class that represents the LC_THREAD / LC_UNIXTHREAD commands and that
/// can be used to get the binary entrypoint when the LC_MAIN (MainCommand) is not present
///
/// Generally speaking, this command aims at defining the original state
/// of the main thread which includes the registers' values
class LIEF_API ThreadCommand : public LoadCommand {
  friend class BinaryParser;
  public:
  ThreadCommand() = default;
  ThreadCommand(const details::thread_command& cmd,
                Header::CPU_TYPE arch = Header::CPU_TYPE::ANY);
  ThreadCommand(uint32_t flavor, uint32_t count,
                Header::CPU_TYPE arch= Header::CPU_TYPE::ANY);

  ThreadCommand& operator=(const ThreadCommand& copy) = default;
  ThreadCommand(const ThreadCommand& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<ThreadCommand>(new ThreadCommand(*this));
  }

  ~ThreadCommand() override = default;

  /// Integer that defines a special *flavor* for the thread.
  ///
  /// The meaning of this value depends on the architecture(). The list of
  /// the values can be found in the XNU kernel files:
  /// - xnu/osfmk/mach/arm/thread_status.h  for the ARM/AArch64 architectures
  /// - xnu/osfmk/mach/i386/thread_status.h for the x86/x86-64 architectures
  uint32_t flavor() const {
    return flavor_;
  }

  /// Size of the thread state data with 32-bits alignment.
  ///
  /// This value should match state().size()
  uint32_t count() const {
    return count_;
  }

  /// The CPU architecture that is targeted by this ThreadCommand
  Header::CPU_TYPE architecture() const {
    return architecture_;
  }

  /// The actual thread state as a vector of bytes. Depending on the architecture(),
  /// these data can be casted into x86_thread_state_t, x86_thread_state64_t, ...
  span<const uint8_t> state() const {
    return  state_;
  }

  span<uint8_t> state() {
    return state_;
  }

  /// Return the initial Program Counter regardless of the underlying architecture.
  /// This value, when non null, can be used to determine the binary's entrypoint.
  ///
  /// Underneath, it works by looking for the PC register value in the state() data
  uint64_t pc() const;

  void state(std::vector<uint8_t> state) {
    state_ = std::move(state);
  }

  void flavor(uint32_t flavor) {
    flavor_ = flavor;
  }
  void count(uint32_t count) {
    count_ = count;
  }
  void architecture(Header::CPU_TYPE arch) {
    architecture_ = arch;
  }

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    const LoadCommand::TYPE type = cmd->command();
    return type == LoadCommand::TYPE::THREAD ||
           type == LoadCommand::TYPE::UNIXTHREAD;
  }

  private:
  uint32_t flavor_ = 0;
  uint32_t count_ = 0;
  Header::CPU_TYPE architecture_  = Header::CPU_TYPE::ANY;
  std::vector<uint8_t> state_;

};

}
}
#endif
