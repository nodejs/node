#include "LIEF/runtime/Process.hpp"
#include "LIEF/runtime/disassembler.hpp"
#include "LIEF/asm/Instruction.hpp"

#include "internal_utils.hpp"
#include "messages.hpp"
#include "logging.hpp"

namespace LIEF::assembly::details {
class Instruction {};
class InstructionIt {};
}

namespace LIEF::runtime {
instructions_it disassemble(uintptr_t /*address*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return make_empty_iterator<assembly::Instruction>();
}

assembly::Engine* Process::default_engine() {
  return nullptr;
}

}
