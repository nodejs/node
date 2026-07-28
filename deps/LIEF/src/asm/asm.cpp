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
#include "LIEF/asm/Instruction.hpp"
#include "LIEF/asm/Engine.hpp"

#include "LIEF/asm/aarch64/Instruction.hpp"
#include "LIEF/asm/aarch64/Operand.hpp"
#include "LIEF/asm/aarch64/registers.hpp"

#include "LIEF/asm/aarch64/operands/Immediate.hpp"
#include "LIEF/asm/aarch64/operands/Register.hpp"
#include "LIEF/asm/aarch64/operands/PCRelative.hpp"
#include "LIEF/asm/aarch64/operands/Memory.hpp"

#include "LIEF/asm/x86/Instruction.hpp"
#include "LIEF/asm/x86/Operand.hpp"
#include "LIEF/asm/x86/registers.hpp"

#include "LIEF/asm/x86/operands/Immediate.hpp"
#include "LIEF/asm/x86/operands/Register.hpp"
#include "LIEF/asm/x86/operands/PCRelative.hpp"
#include "LIEF/asm/x86/operands/Memory.hpp"

#include "LIEF/asm/arm/Instruction.hpp"
#include "LIEF/asm/arm/registers.hpp"

#include "LIEF/asm/mips/Instruction.hpp"
#include "LIEF/asm/mips/registers.hpp"

#include "LIEF/asm/ebpf/Instruction.hpp"
#include "LIEF/asm/ebpf/registers.hpp"

#include "LIEF/asm/riscv/Instruction.hpp"
#include "LIEF/asm/riscv/registers.hpp"

#include "LIEF/asm/powerpc/Instruction.hpp"
#include "LIEF/asm/powerpc/registers.hpp"

#include "LIEF/Abstract/Binary.hpp"
#include "LIEF/COFF/Binary.hpp"

#include "internal_utils.hpp"
#include "messages.hpp"
#include "logging.hpp"

namespace LIEF {

// ----------------------------------------------------------------------------
// Abstract/Binary.hpp
// ----------------------------------------------------------------------------

Binary::instructions_it Binary::disassemble(uint64_t/*address*/, size_t/*size*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

Binary::instructions_it Binary::disassemble(uint64_t/*address*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

Binary::instructions_it Binary::disassemble(const std::string&/*function*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

Binary::instructions_it Binary::disassemble(const uint8_t*, size_t, uint64_t) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

std::vector<uint8_t> Binary::assemble(uint64_t/*address*/, const std::string&/*Asm*/,
    assembly::AssemblerConfig& /*config*/)
{
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}

std::vector<uint8_t> Binary::assemble(uint64_t/*address*/, const llvm::MCInst&/*inst*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}


std::vector<uint8_t> Binary::assemble(uint64_t/*address*/, const std::vector<llvm::MCInst>&/*inst*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}

assembly::Engine* Binary::get_engine(uint64_t) const {
  return nullptr;
}


namespace COFF {
assembly::Engine* Binary::get_engine(uint64_t) const {
  return nullptr;
}

Binary::instructions_it Binary::disassemble(const std::string& /*symbol*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

Binary::instructions_it Binary::disassemble(const Symbol& /*symbol*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

Binary::instructions_it Binary::disassemble(const uint8_t* /*buffer*/, size_t /*size*/,
                                            uint64_t /*address*/) const
{
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}
}

namespace assembly {
namespace details {
class Instruction {};
class InstructionIt {};

class Engine {};
}

namespace x86::details {
class Operand {};
class OperandIt {};
}

namespace aarch64::details {
class Operand {};
class OperandIt {};
}

// ----------------------------------------------------------------------------
// asm/Instruction.hpp
// ----------------------------------------------------------------------------

Instruction::Iterator::Iterator() :
  impl_(nullptr)
{}

Instruction::Iterator::Iterator(std::unique_ptr<details::InstructionIt>) :
  impl_(nullptr)
{}

Instruction::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

Instruction::Iterator& Instruction::Iterator::operator=(const Iterator&) {
  return *this;
}

Instruction::Iterator::Iterator(Iterator&&) noexcept = default;
Instruction::Iterator& Instruction::Iterator::operator=(Iterator&&) noexcept = default;

Instruction::Iterator& Instruction::Iterator::operator++() {
  return *this;
}

std::unique_ptr<Instruction> Instruction::Iterator::operator*() const {
  return nullptr;
}

bool operator==(const Instruction::Iterator&, const Instruction::Iterator&) {
  return true;
}

Instruction::Iterator::~Iterator() = default;

Instruction::Instruction(std::unique_ptr<details::Instruction>) :
  impl_(nullptr)
{}

std::unique_ptr<Instruction> Instruction::create(std::unique_ptr<details::Instruction>) {
  return nullptr;
}

Instruction::~Instruction() = default;

uint64_t Instruction::address() const {
  return 0;
}

size_t Instruction::size() const {
  return 0;
}

const std::vector<uint8_t>& Instruction::raw() const {
  static std::vector<uint8_t> empty;
  return empty;
}

std::string Instruction::mnemonic() const {
  return "";
}

std::string Instruction::to_string(bool/*with_address*/) const {
  return "";
}

bool Instruction::is_branch() const {
  return false;
}

bool Instruction::is_terminator() const {
  return false;
}

bool Instruction::is_call() const {
  return false;
}

bool Instruction::is_syscall() const {
  return false;
}

const llvm::MCInst& Instruction::mcinst() const {
  static uintptr_t FAKE = 0;
  return *reinterpret_cast<llvm::MCInst*>(&FAKE);
}

bool Instruction::is_memory_access() const { return false; }
bool Instruction::is_move_reg() const { return false; }
bool Instruction::is_add() const { return false; }
bool Instruction::is_trap() const { return false; }
bool Instruction::is_barrier() const { return false; }
bool Instruction::is_return() const { return false; }
bool Instruction::is_indirect_branch() const { return false; }
bool Instruction::is_conditional_branch() const { return false; }
bool Instruction::is_unconditional_branch() const { return false; }
bool Instruction::is_compare() const { return false; }
bool Instruction::is_move_immediate() const { return false; }
bool Instruction::is_bitcast() const { return false; }
Instruction::MemoryAccess Instruction::memory_access() const { return MemoryAccess::NONE; }

result<uint64_t> Instruction::branch_target() const {
  return make_error_code(lief_errors::not_implemented);
}

// ----------------------------------------------------------------------------
// asm/Engine.hpp
// ----------------------------------------------------------------------------
Engine::Engine(std::unique_ptr<details::Engine>) :
  impl_(nullptr)
{}

Engine::Engine(Engine&&) noexcept {

}

Engine& Engine::operator=(Engine&&) noexcept {
  return *this;
}

Engine::instructions_it Engine::disassemble(const uint8_t*, size_t, uint64_t) {
  return make_empty_iterator<assembly::Instruction>();
}

std::vector<uint8_t> Engine::assemble(uint64_t/*address*/, const std::string&/*Asm*/,
                                      AssemblerConfig& /*config*/)
{
  return {};
}

std::vector<uint8_t> Engine::assemble(uint64_t/*address*/, const std::string&/*Asm*/,
                                      LIEF::Binary&/*bin*/, AssemblerConfig& /*config*/)
{
  return {};
}

Engine::~Engine() = default;

// ----------------------------------------------------------------------------
// asm/aarch64/Instruction.hpp
// ----------------------------------------------------------------------------
aarch64::OPCODE aarch64::Instruction::opcode() const {
  return aarch64::OPCODE::INSTRUCTION_LIST_END;
}

aarch64::Instruction::operands_it aarch64::Instruction::operands() const {
  return make_empty_iterator<aarch64::Operand>();
}

bool aarch64::Instruction::classof(const assembly::Instruction*) {
  return false;
}

const char* aarch64::get_register_name(aarch64::REG) {
  return "";
}

const char* aarch64::get_register_name(aarch64::SYSREG) {
  return "";
}

// ----------------------------------------------------------------------------
// asm/x86/Instruction.hpp
// ----------------------------------------------------------------------------
x86::OPCODE x86::Instruction::opcode() const {
  return x86::OPCODE::INSTRUCTION_LIST_END;
}

bool x86::Instruction::classof(const assembly::Instruction*) {
  return false;
}

const char* x86::get_register_name(REG) {
  return "";
}

x86::Instruction::operands_it x86::Instruction::operands() const {
  return make_empty_iterator<x86::Operand>();
}

// ----------------------------------------------------------------------------
// asm/arm/Instruction.hpp
// ----------------------------------------------------------------------------
arm::OPCODE arm::Instruction::opcode() const {
  return arm::OPCODE::INSTRUCTION_LIST_END;
}

bool arm::Instruction::classof(const assembly::Instruction*) {
  return false;
}

const char* arm::get_register_name(REG) {
  return "";
}

// ----------------------------------------------------------------------------
// asm/mips/Instruction.hpp
// ----------------------------------------------------------------------------
mips::OPCODE mips::Instruction::opcode() const {
  return mips::OPCODE::INSTRUCTION_LIST_END;
}

bool mips::Instruction::classof(const assembly::Instruction*) {
  return false;
}

const char* mips::get_register_name(REG) {
  return "";
}

// ----------------------------------------------------------------------------
// asm/ebpf/Instruction.hpp
// ----------------------------------------------------------------------------
ebpf::OPCODE ebpf::Instruction::opcode() const {
  return ebpf::OPCODE::INSTRUCTION_LIST_END;
}

bool ebpf::Instruction::classof(const assembly::Instruction*) {
  return false;
}

const char* ebpf::get_register_name(REG) {
  return "";
}

// ----------------------------------------------------------------------------
// asm/riscv/Instruction.hpp
// ----------------------------------------------------------------------------
riscv::OPCODE riscv::Instruction::opcode() const {
  return riscv::OPCODE::INSTRUCTION_LIST_END;
}

bool riscv::Instruction::classof(const assembly::Instruction*) {
  return false;
}

const char* riscv::get_register_name(REG) {
  return "";
}

// ----------------------------------------------------------------------------
// asm/powerpc/Instruction.hpp
// ----------------------------------------------------------------------------
powerpc::OPCODE powerpc::Instruction::opcode() const {
  return powerpc::OPCODE::INSTRUCTION_LIST_END;
}

bool powerpc::Instruction::classof(const assembly::Instruction*) {
  return false;
}

const char* powerpc::get_register_name(REG) {
  return "";
}

// ----------------------------------------------------------------------------
// asm/x86/Operand.hpp
// ----------------------------------------------------------------------------
x86::Operand::Iterator::Iterator() :
  impl_(nullptr)
{}

x86::Operand::Iterator::Iterator(std::unique_ptr<details::OperandIt>) :
  impl_(nullptr)
{}

x86::Operand::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

x86::Operand::Iterator&  x86::Operand::Iterator::operator=(const Iterator&) {
  return *this;
}

x86::Operand::Iterator::Iterator(Iterator&&) noexcept = default;
x86::Operand::Iterator& x86::Operand::Iterator::operator=(Iterator&&) noexcept = default;

x86::Operand::Iterator& x86::Operand::Iterator::operator++() {
  return *this;
}

std::unique_ptr<x86::Operand> x86::Operand::Iterator::operator*() const {
  return nullptr;
}

namespace x86 {
bool operator==(const x86::Operand::Iterator&, const x86::Operand::Iterator&) {
  return true;
}
}

x86::Operand::Iterator::~Iterator() = default;

x86::Operand::~Operand() = default;
x86::Operand::Operand(std::unique_ptr<details::Operand>/*impl*/) :
  impl_(nullptr)
{}

std::string x86::Operand::to_string() const {
  return "";
}

std::unique_ptr<x86::Operand> x86::Operand::create(std::unique_ptr<details::Operand> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// asm/x86/Memory.hpp
// ----------------------------------------------------------------------------
bool x86::operands::Memory::classof(const Operand*) { return false; }

x86::REG x86::operands::Memory::base() const { return x86::REG::NoRegister; }

x86::REG x86::operands::Memory::scaled_register() const {
  return x86::REG::NoRegister;
}

x86::REG x86::operands::Memory::segment_register() const {
  return x86::REG::NoRegister;
}

uint64_t x86::operands::Memory::scale() const { return 0; }

int64_t x86::operands::Memory::displacement() const { return 0; }

// ----------------------------------------------------------------------------
// asm/x86/PCRelative.hpp
// ----------------------------------------------------------------------------
bool x86::operands::PCRelative::classof(const Operand*) { return false; }

int64_t x86::operands::PCRelative::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/x86/Register.hpp
// ----------------------------------------------------------------------------
bool x86::operands::Register::classof(const Operand*) { return false; }

x86::REG x86::operands::Register::value() const {
  return REG::NoRegister;
}

// ----------------------------------------------------------------------------
// asm/x86/Immediate.hpp
// ----------------------------------------------------------------------------
bool x86::operands::Immediate::classof(const Operand*) { return false; }

int64_t x86::operands::Immediate::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/aarch64/Operand.hpp
// ----------------------------------------------------------------------------
aarch64::Operand::Iterator::Iterator() :
  impl_(nullptr)
{}

aarch64::Operand::Iterator::Iterator(std::unique_ptr<details::OperandIt>) :
  impl_(nullptr)
{}

aarch64::Operand::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

aarch64::Operand::Iterator&  aarch64::Operand::Iterator::operator=(const Iterator&) {
  return *this;
}

aarch64::Operand::Iterator::Iterator(Iterator&&) noexcept = default;
aarch64::Operand::Iterator& aarch64::Operand::Iterator::operator=(Iterator&&) noexcept = default;

aarch64::Operand::Iterator& aarch64::Operand::Iterator::operator++() {
  return *this;
}

std::unique_ptr<aarch64::Operand> aarch64::Operand::Iterator::operator*() const {
  return nullptr;
}

namespace aarch64 {
bool operator==(const aarch64::Operand::Iterator&, const aarch64::Operand::Iterator&) {
  return true;
}
}

aarch64::Operand::Iterator::~Iterator() = default;

aarch64::Operand::~Operand() = default;
aarch64::Operand::Operand(std::unique_ptr<details::Operand>/*impl*/) :
  impl_(nullptr)
{}

std::string aarch64::Operand::to_string() const {
  return "";
}

std::unique_ptr<aarch64::Operand> aarch64::Operand::create(std::unique_ptr<details::Operand> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// asm/aarch64/Memory.hpp
// ----------------------------------------------------------------------------
bool aarch64::operands::Memory::classof(const Operand*) { return false; }

aarch64::REG aarch64::operands::Memory::base() const {
  return REG::NoRegister;
}

aarch64::operands::Memory::offset_t aarch64::operands::Memory::offset() const {
  return {};
}

aarch64::operands::Memory::shift_info_t aarch64::operands::Memory::shift() const {
  return {};
}

// ----------------------------------------------------------------------------
// asm/aarch64/PCRelative.hpp
// ----------------------------------------------------------------------------
bool aarch64::operands::PCRelative::classof(const Operand*) { return false; }

int64_t aarch64::operands::PCRelative::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/aarch64/Register.hpp
// ----------------------------------------------------------------------------
bool aarch64::operands::Register::classof(const Operand*) { return false; }

aarch64::operands::Register::reg_t aarch64::operands::Register::value() const {
  return {};
}

// ----------------------------------------------------------------------------
// asm/aarch64/Immediate.hpp
// ----------------------------------------------------------------------------
bool aarch64::operands::Immediate::classof(const Operand*) { return false; }

int64_t aarch64::operands::Immediate::value() const {
  return 0;
}

}
}
