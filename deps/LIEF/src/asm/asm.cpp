/* Copyright 2022 - 2026 R. Thomas
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
// NOLINTBEGIN

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
#include "LIEF/asm/mips/Operand.hpp"
#include "LIEF/asm/mips/registers.hpp"

#include "LIEF/asm/mips/operands/Immediate.hpp"
#include "LIEF/asm/mips/operands/Register.hpp"
#include "LIEF/asm/mips/operands/PCRelative.hpp"
#include "LIEF/asm/mips/operands/Memory.hpp"

#include "LIEF/asm/ebpf/Instruction.hpp"
#include "LIEF/asm/ebpf/Operand.hpp"
#include "LIEF/asm/ebpf/registers.hpp"

#include "LIEF/asm/ebpf/operands/Immediate.hpp"
#include "LIEF/asm/ebpf/operands/Register.hpp"
#include "LIEF/asm/ebpf/operands/PCRelative.hpp"
#include "LIEF/asm/ebpf/operands/Memory.hpp"

#include "LIEF/asm/riscv/Instruction.hpp"
#include "LIEF/asm/riscv/Operand.hpp"
#include "LIEF/asm/riscv/registers.hpp"

#include "LIEF/asm/riscv/operands/Immediate.hpp"
#include "LIEF/asm/riscv/operands/Register.hpp"
#include "LIEF/asm/riscv/operands/PCRelative.hpp"
#include "LIEF/asm/riscv/operands/Memory.hpp"

#include "LIEF/asm/powerpc/Instruction.hpp"
#include "LIEF/asm/powerpc/Operand.hpp"
#include "LIEF/asm/powerpc/registers.hpp"

#include "LIEF/asm/powerpc/operands/Immediate.hpp"
#include "LIEF/asm/powerpc/operands/Register.hpp"
#include "LIEF/asm/powerpc/operands/PCRelative.hpp"
#include "LIEF/asm/powerpc/operands/Memory.hpp"

#include "LIEF/Abstract/Binary.hpp"
#if defined(LIEF_COFF_SUPPORT)
  #include "LIEF/COFF/Binary.hpp"
#endif

#include "internal_utils.hpp"
#include "messages.hpp"
#include "logging.hpp"

namespace LIEF {

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

namespace mips::details {
class Operand {};
class OperandIt {};
}

namespace powerpc::details {
class Operand {};
class OperandIt {};
}

namespace ebpf::details {
class Operand {};
class OperandIt {};
}

namespace riscv::details {
class Operand {};
class OperandIt {};
}
}

// ----------------------------------------------------------------------------
// Abstract/Binary.hpp
// ----------------------------------------------------------------------------

Binary::instructions_it Binary::disassemble(uint64_t /*address*/,
                                            size_t /*size*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

Binary::instructions_it Binary::disassemble(uint64_t /*address*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

Binary::instructions_it
    Binary::disassemble(const std::string& /*function*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

Binary::instructions_it Binary::disassemble(const uint8_t*, size_t,
                                            uint64_t) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

std::vector<uint8_t> Binary::assemble(uint64_t /*address*/,
                                      const std::string& /*Asm*/,
                                      assembly::AssemblerConfig& /*config*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}

std::vector<uint8_t> Binary::assemble(uint64_t /*address*/,
                                      const llvm::MCInst& /*inst*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}


std::vector<uint8_t> Binary::assemble(uint64_t /*address*/,
                                      const std::vector<llvm::MCInst>& /*inst*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}

assembly::Engine* Binary::get_engine(uint64_t) const {
  return nullptr;
}


#if defined(LIEF_COFF_SUPPORT)
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

Binary::instructions_it Binary::disassemble(const uint8_t* /*buffer*/,
                                            size_t /*size*/,
                                            uint64_t /*address*/) const {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}
}
#endif

namespace assembly {

// ----------------------------------------------------------------------------
// asm/Instruction.hpp
// ----------------------------------------------------------------------------

Instruction::Iterator::Iterator() :
  impl_(nullptr) {}

Instruction::Iterator::Iterator(std::unique_ptr<details::InstructionIt>) :
  impl_(nullptr) {}

Instruction::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Instruction::Iterator& Instruction::Iterator::operator=(const Iterator&) {
  return *this;
}

Instruction::Iterator::Iterator(Iterator&&) noexcept = default;
Instruction::Iterator&
    Instruction::Iterator::operator=(Iterator&&) noexcept = default;

Instruction::Iterator& Instruction::Iterator::operator++() {
  return *this;
}

void Instruction::Iterator::load() const {}

const Instruction& Instruction::Iterator::operator*() const {
  return *cached_;
}

const Instruction* Instruction::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Instruction> Instruction::Iterator::yield() {
  return nullptr;
}

bool operator==(const Instruction::Iterator&, const Instruction::Iterator&) {
  return true;
}

Instruction::Iterator::~Iterator() = default;

Instruction::Instruction(std::unique_ptr<details::Instruction>) :
  impl_(nullptr) {}

std::unique_ptr<Instruction>
    Instruction::create(std::unique_ptr<details::Instruction>) {
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

std::string Instruction::to_string(bool /*with_address*/) const {
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

bool Instruction::is_memory_access() const {
  return false;
}
bool Instruction::is_move_reg() const {
  return false;
}
bool Instruction::is_add() const {
  return false;
}
bool Instruction::is_trap() const {
  return false;
}
bool Instruction::is_barrier() const {
  return false;
}
bool Instruction::is_return() const {
  return false;
}
bool Instruction::is_indirect_branch() const {
  return false;
}
bool Instruction::is_conditional_branch() const {
  return false;
}
bool Instruction::is_unconditional_branch() const {
  return false;
}
bool Instruction::is_compare() const {
  return false;
}
bool Instruction::is_move_immediate() const {
  return false;
}
bool Instruction::is_bitcast() const {
  return false;
}
Instruction::MemoryAccess Instruction::memory_access() const {
  return MemoryAccess::NONE;
}

result<uint64_t> Instruction::branch_target() const {
  return make_error_code(lief_errors::not_implemented);
}

// ----------------------------------------------------------------------------
// asm/Engine.hpp
// ----------------------------------------------------------------------------
Engine::Engine(std::unique_ptr<details::Engine>) :
  impl_(nullptr) {}

Engine::Engine(Engine&&) noexcept {}

Engine& Engine::operator=(Engine&&) noexcept {
  return *this;
}

Engine::instructions_it Engine::disassemble(const uint8_t*, size_t, uint64_t) {
  return make_empty_iterator<assembly::Instruction>();
}

std::vector<uint8_t> Engine::assemble(uint64_t /*address*/,
                                      const std::string& /*Asm*/,
                                      AssemblerConfig& /*config*/) {
  return {};
}

std::vector<uint8_t> Engine::assemble(uint64_t /*address*/,
                                      const std::string& /*Asm*/,
                                      LIEF::Binary& /*bin*/,
                                      AssemblerConfig& /*config*/) {
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

mips::Instruction::operands_it mips::Instruction::operands() const {
  return make_empty_iterator<mips::Operand>();
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

ebpf::Instruction::operands_it ebpf::Instruction::operands() const {
  return make_empty_iterator<ebpf::Operand>();
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

riscv::Instruction::operands_it riscv::Instruction::operands() const {
  return make_empty_iterator<riscv::Operand>();
}

bool riscv::Instruction::classof(const assembly::Instruction*) {
  return false;
}

const char* riscv::get_register_name(REG) {
  return "";
}

const char* riscv::get_register_name(SYSREG) {
  return "";
}

// ----------------------------------------------------------------------------
// asm/powerpc/Instruction.hpp
// ----------------------------------------------------------------------------
powerpc::OPCODE powerpc::Instruction::opcode() const {
  return powerpc::OPCODE::INSTRUCTION_LIST_END;
}

powerpc::Instruction::operands_it powerpc::Instruction::operands() const {
  return make_empty_iterator<powerpc::Operand>();
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
  impl_(nullptr) {}

x86::Operand::Iterator::Iterator(std::unique_ptr<details::OperandIt>) :
  impl_(nullptr) {}

x86::Operand::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

x86::Operand::Iterator& x86::Operand::Iterator::operator=(const Iterator&) {
  return *this;
}

x86::Operand::Iterator::Iterator(Iterator&&) noexcept = default;
x86::Operand::Iterator&
    x86::Operand::Iterator::operator=(Iterator&&) noexcept = default;

x86::Operand::Iterator& x86::Operand::Iterator::operator++() {
  return *this;
}

void x86::Operand::Iterator::load() const {}

const x86::Operand& x86::Operand::Iterator::operator*() const {
  return *cached_;
}

const x86::Operand* x86::Operand::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<x86::Operand> x86::Operand::Iterator::yield() {
  return nullptr;
}

namespace x86 {
bool operator==(const x86::Operand::Iterator&, const x86::Operand::Iterator&) {
  return true;
}
}

x86::Operand::Iterator::~Iterator() = default;

x86::Operand::~Operand() = default;
x86::Operand::Operand(std::unique_ptr<details::Operand> /*impl*/) :
  impl_(nullptr) {}

std::string x86::Operand::to_string() const {
  return "";
}

std::unique_ptr<x86::Operand>
    x86::Operand::create(std::unique_ptr<details::Operand> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// asm/x86/Memory.hpp
// ----------------------------------------------------------------------------
bool x86::operands::Memory::classof(const Operand*) {
  return false;
}

x86::REG x86::operands::Memory::base() const {
  return x86::REG::NoRegister;
}

x86::REG x86::operands::Memory::scaled_register() const {
  return x86::REG::NoRegister;
}

x86::REG x86::operands::Memory::segment_register() const {
  return x86::REG::NoRegister;
}

uint64_t x86::operands::Memory::scale() const {
  return 0;
}

int64_t x86::operands::Memory::displacement() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/x86/PCRelative.hpp
// ----------------------------------------------------------------------------
bool x86::operands::PCRelative::classof(const Operand*) {
  return false;
}

int64_t x86::operands::PCRelative::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/x86/Register.hpp
// ----------------------------------------------------------------------------
bool x86::operands::Register::classof(const Operand*) {
  return false;
}

x86::REG x86::operands::Register::value() const {
  return REG::NoRegister;
}

// ----------------------------------------------------------------------------
// asm/x86/Immediate.hpp
// ----------------------------------------------------------------------------
bool x86::operands::Immediate::classof(const Operand*) {
  return false;
}

int64_t x86::operands::Immediate::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/aarch64/Operand.hpp
// ----------------------------------------------------------------------------
aarch64::Operand::Iterator::Iterator() :
  impl_(nullptr) {}

aarch64::Operand::Iterator::Iterator(std::unique_ptr<details::OperandIt>) :
  impl_(nullptr) {}

aarch64::Operand::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

aarch64::Operand::Iterator&
    aarch64::Operand::Iterator::operator=(const Iterator&) {
  return *this;
}

aarch64::Operand::Iterator::Iterator(Iterator&&) noexcept = default;
aarch64::Operand::Iterator&
    aarch64::Operand::Iterator::operator=(Iterator&&) noexcept = default;

aarch64::Operand::Iterator& aarch64::Operand::Iterator::operator++() {
  return *this;
}

void aarch64::Operand::Iterator::load() const {}

const aarch64::Operand& aarch64::Operand::Iterator::operator*() const {
  return *cached_;
}

const aarch64::Operand* aarch64::Operand::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<aarch64::Operand> aarch64::Operand::Iterator::yield() {
  return nullptr;
}

namespace aarch64 {
bool operator==(const aarch64::Operand::Iterator&,
                const aarch64::Operand::Iterator&) {
  return true;
}
}

aarch64::Operand::Iterator::~Iterator() = default;

aarch64::Operand::~Operand() = default;
aarch64::Operand::Operand(std::unique_ptr<details::Operand> /*impl*/) :
  impl_(nullptr) {}

std::string aarch64::Operand::to_string() const {
  return "";
}

std::unique_ptr<aarch64::Operand>
    aarch64::Operand::create(std::unique_ptr<details::Operand> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// asm/aarch64/Memory.hpp
// ----------------------------------------------------------------------------
bool aarch64::operands::Memory::classof(const Operand*) {
  return false;
}

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
bool aarch64::operands::PCRelative::classof(const Operand*) {
  return false;
}

int64_t aarch64::operands::PCRelative::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/aarch64/Register.hpp
// ----------------------------------------------------------------------------
bool aarch64::operands::Register::classof(const Operand*) {
  return false;
}

aarch64::operands::Register::reg_t aarch64::operands::Register::value() const {
  return {};
}

// ----------------------------------------------------------------------------
// asm/aarch64/Immediate.hpp
// ----------------------------------------------------------------------------
bool aarch64::operands::Immediate::classof(const Operand*) {
  return false;
}

int64_t aarch64::operands::Immediate::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/mips/Operand.hpp
// ----------------------------------------------------------------------------
mips::Operand::Iterator::Iterator() :
  impl_(nullptr) {}

mips::Operand::Iterator::Iterator(std::unique_ptr<details::OperandIt>) :
  impl_(nullptr) {}

mips::Operand::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

mips::Operand::Iterator& mips::Operand::Iterator::operator=(const Iterator&) {
  return *this;
}

mips::Operand::Iterator::Iterator(Iterator&&) noexcept = default;
mips::Operand::Iterator&
    mips::Operand::Iterator::operator=(Iterator&&) noexcept = default;

mips::Operand::Iterator& mips::Operand::Iterator::operator++() {
  return *this;
}

void mips::Operand::Iterator::load() const {}

const mips::Operand& mips::Operand::Iterator::operator*() const {
  return *cached_;
}

const mips::Operand* mips::Operand::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<mips::Operand> mips::Operand::Iterator::yield() {
  return nullptr;
}

namespace mips {
bool operator==(const mips::Operand::Iterator&, const mips::Operand::Iterator&) {
  return true;
}
}

mips::Operand::Iterator::~Iterator() = default;

mips::Operand::~Operand() = default;
mips::Operand::Operand(std::unique_ptr<details::Operand> /*impl*/) :
  impl_(nullptr) {}

std::string mips::Operand::to_string() const {
  return "";
}

std::unique_ptr<mips::Operand>
    mips::Operand::create(std::unique_ptr<details::Operand> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// asm/mips/Memory.hpp
// ----------------------------------------------------------------------------
bool mips::operands::Memory::classof(const Operand*) {
  return false;
}

mips::REG mips::operands::Memory::base() const {
  return REG::NoRegister;
}

mips::operands::Memory::offset_t mips::operands::Memory::offset() const {
  return {};
}

// ----------------------------------------------------------------------------
// asm/mips/PCRelative.hpp
// ----------------------------------------------------------------------------
bool mips::operands::PCRelative::classof(const Operand*) {
  return false;
}

int64_t mips::operands::PCRelative::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/mips/Register.hpp
// ----------------------------------------------------------------------------
bool mips::operands::Register::classof(const Operand*) {
  return false;
}

mips::REG mips::operands::Register::value() const {
  return REG::NoRegister;
}

// ----------------------------------------------------------------------------
// asm/mips/Immediate.hpp
// ----------------------------------------------------------------------------
bool mips::operands::Immediate::classof(const Operand*) {
  return false;
}

int64_t mips::operands::Immediate::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/powerpc/Operand.hpp
// ----------------------------------------------------------------------------
powerpc::Operand::Iterator::Iterator() :
  impl_(nullptr) {}

powerpc::Operand::Iterator::Iterator(std::unique_ptr<details::OperandIt>) :
  impl_(nullptr) {}

powerpc::Operand::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

powerpc::Operand::Iterator&
    powerpc::Operand::Iterator::operator=(const Iterator&) {
  return *this;
}

powerpc::Operand::Iterator::Iterator(Iterator&&) noexcept = default;
powerpc::Operand::Iterator&
    powerpc::Operand::Iterator::operator=(Iterator&&) noexcept = default;

powerpc::Operand::Iterator& powerpc::Operand::Iterator::operator++() {
  return *this;
}

void powerpc::Operand::Iterator::load() const {}

const powerpc::Operand& powerpc::Operand::Iterator::operator*() const {
  return *cached_;
}

const powerpc::Operand* powerpc::Operand::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<powerpc::Operand> powerpc::Operand::Iterator::yield() {
  return nullptr;
}

namespace powerpc {
bool operator==(const powerpc::Operand::Iterator&,
                const powerpc::Operand::Iterator&) {
  return true;
}
}

powerpc::Operand::Iterator::~Iterator() = default;

powerpc::Operand::~Operand() = default;
powerpc::Operand::Operand(std::unique_ptr<details::Operand> /*impl*/) :
  impl_(nullptr) {}

std::string powerpc::Operand::to_string() const {
  return "";
}

std::unique_ptr<powerpc::Operand>
    powerpc::Operand::create(std::unique_ptr<details::Operand> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// asm/powerpc/Memory.hpp
// ----------------------------------------------------------------------------
bool powerpc::operands::Memory::classof(const Operand*) {
  return false;
}

powerpc::REG powerpc::operands::Memory::base() const {
  return REG::NoRegister;
}

powerpc::operands::Memory::offset_t powerpc::operands::Memory::offset() const {
  return {};
}

bool powerpc::operands::Memory::has_index() const {
  return false;
}

// ----------------------------------------------------------------------------
// asm/powerpc/PCRelative.hpp
// ----------------------------------------------------------------------------
bool powerpc::operands::PCRelative::classof(const Operand*) {
  return false;
}

int64_t powerpc::operands::PCRelative::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/powerpc/Register.hpp
// ----------------------------------------------------------------------------
bool powerpc::operands::Register::classof(const Operand*) {
  return false;
}

powerpc::REG powerpc::operands::Register::value() const {
  return REG::NoRegister;
}

// ----------------------------------------------------------------------------
// asm/powerpc/Immediate.hpp
// ----------------------------------------------------------------------------
bool powerpc::operands::Immediate::classof(const Operand*) {
  return false;
}

int64_t powerpc::operands::Immediate::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/ebpf/Operand.hpp
// ----------------------------------------------------------------------------
ebpf::Operand::Iterator::Iterator() :
  impl_(nullptr) {}

ebpf::Operand::Iterator::Iterator(std::unique_ptr<details::OperandIt>) :
  impl_(nullptr) {}

ebpf::Operand::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

ebpf::Operand::Iterator& ebpf::Operand::Iterator::operator=(const Iterator&) {
  return *this;
}

ebpf::Operand::Iterator::Iterator(Iterator&&) noexcept = default;
ebpf::Operand::Iterator&
    ebpf::Operand::Iterator::operator=(Iterator&&) noexcept = default;

ebpf::Operand::Iterator& ebpf::Operand::Iterator::operator++() {
  return *this;
}

void ebpf::Operand::Iterator::load() const {}

const ebpf::Operand& ebpf::Operand::Iterator::operator*() const {
  return *cached_;
}

const ebpf::Operand* ebpf::Operand::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<ebpf::Operand> ebpf::Operand::Iterator::yield() {
  return nullptr;
}

namespace ebpf {
bool operator==(const ebpf::Operand::Iterator&, const ebpf::Operand::Iterator&) {
  return true;
}
}

ebpf::Operand::Iterator::~Iterator() = default;

ebpf::Operand::~Operand() = default;
ebpf::Operand::Operand(std::unique_ptr<details::Operand> /*impl*/) :
  impl_(nullptr) {}

std::string ebpf::Operand::to_string() const {
  return "";
}

std::unique_ptr<ebpf::Operand>
    ebpf::Operand::create(std::unique_ptr<details::Operand> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// asm/ebpf/Memory.hpp
// ----------------------------------------------------------------------------
bool ebpf::operands::Memory::classof(const Operand*) {
  return false;
}

ebpf::REG ebpf::operands::Memory::base() const {
  return REG::NoRegister;
}

int64_t ebpf::operands::Memory::displacement() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/ebpf/PCRelative.hpp
// ----------------------------------------------------------------------------
bool ebpf::operands::PCRelative::classof(const Operand*) {
  return false;
}

int64_t ebpf::operands::PCRelative::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/ebpf/Register.hpp
// ----------------------------------------------------------------------------
bool ebpf::operands::Register::classof(const Operand*) {
  return false;
}

ebpf::REG ebpf::operands::Register::value() const {
  return REG::NoRegister;
}

// ----------------------------------------------------------------------------
// asm/ebpf/Immediate.hpp
// ----------------------------------------------------------------------------
bool ebpf::operands::Immediate::classof(const Operand*) {
  return false;
}

int64_t ebpf::operands::Immediate::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/riscv/Operand.hpp
// ----------------------------------------------------------------------------
riscv::Operand::Iterator::Iterator() :
  impl_(nullptr) {}

riscv::Operand::Iterator::Iterator(std::unique_ptr<details::OperandIt>) :
  impl_(nullptr) {}

riscv::Operand::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

riscv::Operand::Iterator& riscv::Operand::Iterator::operator=(const Iterator&) {
  return *this;
}

riscv::Operand::Iterator::Iterator(Iterator&&) noexcept = default;
riscv::Operand::Iterator&
    riscv::Operand::Iterator::operator=(Iterator&&) noexcept = default;

riscv::Operand::Iterator& riscv::Operand::Iterator::operator++() {
  return *this;
}

void riscv::Operand::Iterator::load() const {}

const riscv::Operand& riscv::Operand::Iterator::operator*() const {
  return *cached_;
}

const riscv::Operand* riscv::Operand::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<riscv::Operand> riscv::Operand::Iterator::yield() {
  return nullptr;
}

namespace riscv {
bool operator==(const riscv::Operand::Iterator&, const riscv::Operand::Iterator&) {
  return true;
}
}

riscv::Operand::Iterator::~Iterator() = default;

riscv::Operand::~Operand() = default;
riscv::Operand::Operand(std::unique_ptr<details::Operand> /*impl*/) :
  impl_(nullptr) {}

std::string riscv::Operand::to_string() const {
  return "";
}

std::unique_ptr<riscv::Operand>
    riscv::Operand::create(std::unique_ptr<details::Operand> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// asm/riscv/Memory.hpp
// ----------------------------------------------------------------------------
bool riscv::operands::Memory::classof(const Operand*) {
  return false;
}

riscv::REG riscv::operands::Memory::base() const {
  return REG::NoRegister;
}

int64_t riscv::operands::Memory::displacement() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/riscv/PCRelative.hpp
// ----------------------------------------------------------------------------
bool riscv::operands::PCRelative::classof(const Operand*) {
  return false;
}

int64_t riscv::operands::PCRelative::value() const {
  return 0;
}

// ----------------------------------------------------------------------------
// asm/riscv/Register.hpp
// ----------------------------------------------------------------------------
bool riscv::operands::Register::classof(const Operand*) {
  return false;
}

riscv::operands::Register::reg_t riscv::operands::Register::value() const {
  return {};
}

// ----------------------------------------------------------------------------
// asm/riscv/Immediate.hpp
// ----------------------------------------------------------------------------
bool riscv::operands::Immediate::classof(const Operand*) {
  return false;
}

int64_t riscv::operands::Immediate::value() const {
  return 0;
}

}

// NOLINTEND

}
