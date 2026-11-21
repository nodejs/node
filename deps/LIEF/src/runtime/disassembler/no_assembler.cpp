#include "LIEF/asm/AssemblerConfig.hpp"
#include "LIEF/asm/Instruction.hpp"
#include "LIEF/runtime/assembler.hpp"

#include "messages.hpp"
#include "logging.hpp"

namespace LIEF::runtime {
std::vector<uint8_t> assemble(uint64_t /*addr*/, const std::string& /*Asm*/,
                              assembly::AssemblerConfig& /*config*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}

std::vector<uint8_t> assemble(uint64_t /*address*/, const llvm::MCInst& /*inst*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}

std::vector<uint8_t> assemble(uint64_t /*address*/,
                              const std::vector<llvm::MCInst>& /*insts*/) {
  LIEF_ERR(ASSEMBLY_NOT_SUPPORTED);
  return {};
}

}
