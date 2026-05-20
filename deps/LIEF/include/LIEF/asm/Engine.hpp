/* Copyright 2022 - 2025 R. Thomas
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
#ifndef LIEF_ASM_ENGINE_H
#define LIEF_ASM_ENGINE_H
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

#include "LIEF/asm/Instruction.hpp"
#include "LIEF/asm/AssemblerConfig.hpp"

#include <memory>

namespace LIEF {
class Binary;

/// Namespace related to assembly/disassembly support
namespace assembly {

namespace details {
class Engine;
}

/// This class interfaces the assembler/disassembler support
class LIEF_API Engine {
  public:
  /// Disassembly instruction iterator
  using instructions_it = iterator_range<Instruction::Iterator>;

  Engine() = delete;
  Engine(std::unique_ptr<details::Engine> impl);

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  Engine(Engine&&) noexcept;
  Engine& operator=(Engine&&) noexcept;

  /// Disassemble the provided buffer with the address specified in the second
  /// parameter.
  instructions_it disassemble(const uint8_t* buffer, size_t size, uint64_t addr);

  /// Disassemble the given vector of bytes with the address specified in the second
  /// parameter.
  instructions_it disassemble(const std::vector<uint8_t>& bytes, uint64_t addr) {
    return disassemble(bytes.data(), bytes.size(), addr);
  }

  std::vector<uint8_t> assemble(uint64_t address, const std::string& Asm,
      AssemblerConfig& config = AssemblerConfig::default_config());

  std::vector<uint8_t> assemble(uint64_t address, const std::string& Asm,
      LIEF::Binary& bin, AssemblerConfig& config = AssemblerConfig::default_config());

  std::vector<uint8_t> assemble(uint64_t address, const llvm::MCInst& inst,
                                LIEF::Binary& bin);

  std::vector<uint8_t> assemble(
      uint64_t address, const std::vector<llvm::MCInst>& inst, LIEF::Binary& bin);

  ~Engine();

  /// \private
  LIEF_LOCAL const details::Engine& impl() const {
    assert(impl_ != nullptr);
    return *impl_;
  }

  /// \private
  LIEF_LOCAL details::Engine& impl() {
    assert(impl_ != nullptr);
    return *impl_;
  }

  private:
  std::unique_ptr<details::Engine> impl_;
};
}
}

#endif
