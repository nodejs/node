// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>
#include <cstdarg>
#include "v8.h"

#include "disasm.h"
#include "assembler.h"
#include "globals.h"    // Need the BitCast
#include "mips/constants-mips.h"
#include "mips/simulator-mips.h"

namespace v8i = v8::internal;

#if !defined(__mips)

// Only build the simulator if not compiling for real MIPS hardware.
namespace assembler {
namespace mips {

using ::v8::internal::Object;
using ::v8::internal::PrintF;
using ::v8::internal::OS;
using ::v8::internal::ReadLine;
using ::v8::internal::DeleteArray;

// Utils functions
bool HaveSameSign(int32_t a, int32_t b) {
  return ((a ^ b) > 0);
}


// This macro provides a platform independent use of sscanf. The reason for
// SScanF not being implemented in a platform independent was through
// ::v8::internal::OS in the same way as SNPrintF is that the Windows C Run-Time
// Library does not provide vsscanf.
#define SScanF sscanf  // NOLINT

// The Debugger class is used by the simulator while debugging simulated MIPS
// code.
class Debugger {
 public:
  explicit Debugger(Simulator* sim);
  ~Debugger();

  void Stop(Instruction* instr);
  void Debug();

 private:
  // We set the breakpoint code to 0xfffff to easily recognize it.
  static const Instr kBreakpointInstr = SPECIAL | BREAK | 0xfffff << 6;
  static const Instr kNopInstr =  0x0;

  Simulator* sim_;

  int32_t GetRegisterValue(int regnum);
  bool GetValue(const char* desc, int32_t* value);

  // Set or delete a breakpoint. Returns true if successful.
  bool SetBreakpoint(Instruction* breakpc);
  bool DeleteBreakpoint(Instruction* breakpc);

  // Undo and redo all breakpoints. This is needed to bracket disassembly and
  // execution to skip past breakpoints when run from the debugger.
  void UndoBreakpoints();
  void RedoBreakpoints();

  // Print all registers with a nice formatting.
  void PrintAllRegs();
};

Debugger::Debugger(Simulator* sim) {
  sim_ = sim;
}

Debugger::~Debugger() {
}

#ifdef GENERATED_CODE_COVERAGE
static FILE* coverage_log = NULL;


static void InitializeCoverage() {
  char* file_name = getenv("V8_GENERATED_CODE_COVERAGE_LOG");
  if (file_name != NULL) {
    coverage_log = fopen(file_name, "aw+");
  }
}


void Debugger::Stop(Instruction* instr) {
  UNIMPLEMENTED_MIPS();
  char* str = reinterpret_cast<char*>(instr->InstructionBits());
  if (strlen(str) > 0) {
    if (coverage_log != NULL) {
      fprintf(coverage_log, "%s\n", str);
      fflush(coverage_log);
    }
    instr->SetInstructionBits(0x0);  // Overwrite with nop.
  }
  sim_->set_pc(sim_->get_pc() + Instruction::kInstructionSize);
}

#else  // ndef GENERATED_CODE_COVERAGE

#define UNSUPPORTED() printf("Unsupported instruction.\n");

static void InitializeCoverage() {}


void Debugger::Stop(Instruction* instr) {
  const char* str = reinterpret_cast<char*>(instr->InstructionBits());
  PrintF("Simulator hit %s\n", str);
  sim_->set_pc(sim_->get_pc() + Instruction::kInstructionSize);
  Debug();
}
#endif  // GENERATED_CODE_COVERAGE


int32_t Debugger::GetRegisterValue(int regnum) {
  if (regnum == kNumSimuRegisters) {
    return sim_->get_pc();
  } else {
    return sim_->get_register(regnum);
  }
}


bool Debugger::GetValue(const char* desc, int32_t* value) {
  int regnum = Registers::Number(desc);
  if (regnum != kInvalidRegister) {
    *value = GetRegisterValue(regnum);
    return true;
  } else {
    return SScanF(desc, "%i", value) == 1;
  }
  return false;
}


bool Debugger::SetBreakpoint(Instruction* breakpc) {
  // Check if a breakpoint can be set. If not return without any side-effects.
  if (sim_->break_pc_ != NULL) {
    return false;
  }

  // Set the breakpoint.
  sim_->break_pc_ = breakpc;
  sim_->break_instr_ = breakpc->InstructionBits();
  // Not setting the breakpoint instruction in the code itself. It will be set
  // when the debugger shell continues.
  return true;
}


bool Debugger::DeleteBreakpoint(Instruction* breakpc) {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }

  sim_->break_pc_ = NULL;
  sim_->break_instr_ = 0;
  return true;
}


void Debugger::UndoBreakpoints() {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }
}


void Debugger::RedoBreakpoints() {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(kBreakpointInstr);
  }
}

void Debugger::PrintAllRegs() {
#define REG_INFO(n) Registers::Name(n), GetRegisterValue(n), GetRegisterValue(n)

  PrintF("\n");
  // at, v0, a0
  PrintF("%3s: 0x%08x %10d\t%3s: 0x%08x %10d\t%3s: 0x%08x %10d\n",
         REG_INFO(1), REG_INFO(2), REG_INFO(4));
  // v1, a1
  PrintF("%26s\t%3s: 0x%08x %10d\t%3s: 0x%08x %10d\n",
         "", REG_INFO(3), REG_INFO(5));
  // a2
  PrintF("%26s\t%26s\t%3s: 0x%08x %10d\n", "", "", REG_INFO(6));
  // a3
  PrintF("%26s\t%26s\t%3s: 0x%08x %10d\n", "", "", REG_INFO(7));
  PrintF("\n");
  // t0-t7, s0-s7
  for (int i = 0; i < 8; i++) {
    PrintF("%3s: 0x%08x %10d\t%3s: 0x%08x %10d\n",
           REG_INFO(8+i), REG_INFO(16+i));
  }
  PrintF("\n");
  // t8, k0, LO
  PrintF("%3s: 0x%08x %10d\t%3s: 0x%08x %10d\t%3s: 0x%08x %10d\n",
         REG_INFO(24), REG_INFO(26), REG_INFO(32));
  // t9, k1, HI
  PrintF("%3s: 0x%08x %10d\t%3s: 0x%08x %10d\t%3s: 0x%08x %10d\n",
         REG_INFO(25), REG_INFO(27), REG_INFO(33));
  // sp, fp, gp
  PrintF("%3s: 0x%08x %10d\t%3s: 0x%08x %10d\t%3s: 0x%08x %10d\n",
         REG_INFO(29), REG_INFO(30), REG_INFO(28));
  // pc
  PrintF("%3s: 0x%08x %10d\t%3s: 0x%08x %10d\n",
         REG_INFO(31), REG_INFO(34));
#undef REG_INFO
}

void Debugger::Debug() {
  intptr_t last_pc = -1;
  bool done = false;

#define COMMAND_SIZE 63
#define ARG_SIZE 255

#define STR(a) #a
#define XSTR(a) STR(a)

  char cmd[COMMAND_SIZE + 1];
  char arg1[ARG_SIZE + 1];
  char arg2[ARG_SIZE + 1];

  // make sure to have a proper terminating character if reaching the limit
  cmd[COMMAND_SIZE] = 0;
  arg1[ARG_SIZE] = 0;
  arg2[ARG_SIZE] = 0;

  // Undo all set breakpoints while running in the debugger shell. This will
  // make them invisible to all commands.
  UndoBreakpoints();

  while (!done && (sim_->get_pc() != Simulator::end_sim_pc)) {
    if (last_pc != sim_->get_pc()) {
      disasm::NameConverter converter;
      disasm::Disassembler dasm(converter);
      // use a reasonably large buffer
      v8::internal::EmbeddedVector<char, 256> buffer;
      dasm.InstructionDecode(buffer,
                             reinterpret_cast<byte_*>(sim_->get_pc()));
      PrintF("  0x%08x  %s\n", sim_->get_pc(), buffer.start());
      last_pc = sim_->get_pc();
    }
    char* line = ReadLine("sim> ");
    if (line == NULL) {
      break;
    } else {
      // Use sscanf to parse the individual parts of the command line. At the
      // moment no command expects more than two parameters.
      int args = SScanF(line,
                        "%" XSTR(COMMAND_SIZE) "s "
                        "%" XSTR(ARG_SIZE) "s "
                        "%" XSTR(ARG_SIZE) "s",
                        cmd, arg1, arg2);
      if ((strcmp(cmd, "si") == 0) || (strcmp(cmd, "stepi") == 0)) {
        if (!(reinterpret_cast<Instruction*>(sim_->get_pc())->IsTrap())) {
          sim_->InstructionDecode(
                                reinterpret_cast<Instruction*>(sim_->get_pc()));
        } else {
          // Allow si to jump over generated breakpoints.
          PrintF("/!\\ Jumping over generated breakpoint.\n");
          sim_->set_pc(sim_->get_pc() + Instruction::kInstructionSize);
        }
      } else if ((strcmp(cmd, "c") == 0) || (strcmp(cmd, "cont") == 0)) {
        // Execute the one instruction we broke at with breakpoints disabled.
        sim_->InstructionDecode(reinterpret_cast<Instruction*>(sim_->get_pc()));
        // Leave the debugger shell.
        done = true;
      } else if ((strcmp(cmd, "p") == 0) || (strcmp(cmd, "print") == 0)) {
        if (args == 2) {
          int32_t value;
          if (strcmp(arg1, "all") == 0) {
            PrintAllRegs();
          } else {
            if (GetValue(arg1, &value)) {
              PrintF("%s: 0x%08x %d \n", arg1, value, value);
            } else {
              PrintF("%s unrecognized\n", arg1);
            }
          }
        } else {
          PrintF("print <register>\n");
        }
      } else if ((strcmp(cmd, "po") == 0)
                 || (strcmp(cmd, "printobject") == 0)) {
        if (args == 2) {
          int32_t value;
          if (GetValue(arg1, &value)) {
            Object* obj = reinterpret_cast<Object*>(value);
            PrintF("%s: \n", arg1);
#ifdef DEBUG
            obj->PrintLn();
#else
            obj->ShortPrint();
            PrintF("\n");
#endif
          } else {
            PrintF("%s unrecognized\n", arg1);
          }
        } else {
          PrintF("printobject <value>\n");
        }
      } else if ((strcmp(cmd, "disasm") == 0) || (strcmp(cmd, "dpc") == 0)) {
        disasm::NameConverter converter;
        disasm::Disassembler dasm(converter);
        // use a reasonably large buffer
        v8::internal::EmbeddedVector<char, 256> buffer;

        byte_* cur = NULL;
        byte_* end = NULL;

        if (args == 1) {
          cur = reinterpret_cast<byte_*>(sim_->get_pc());
          end = cur + (10 * Instruction::kInstructionSize);
        } else if (args == 2) {
          int32_t value;
          if (GetValue(arg1, &value)) {
            cur = reinterpret_cast<byte_*>(value);
            // no length parameter passed, assume 10 instructions
            end = cur + (10 * Instruction::kInstructionSize);
          }
        } else {
          int32_t value1;
          int32_t value2;
          if (GetValue(arg1, &value1) && GetValue(arg2, &value2)) {
            cur = reinterpret_cast<byte_*>(value1);
            end = cur + (value2 * Instruction::kInstructionSize);
          }
        }

        while (cur < end) {
          dasm.InstructionDecode(buffer, cur);
          PrintF("  0x%08x  %s\n", cur, buffer.start());
          cur += Instruction::kInstructionSize;
        }
      } else if (strcmp(cmd, "gdb") == 0) {
        PrintF("relinquishing control to gdb\n");
        v8::internal::OS::DebugBreak();
        PrintF("regaining control from gdb\n");
      } else if (strcmp(cmd, "break") == 0) {
        if (args == 2) {
          int32_t value;
          if (GetValue(arg1, &value)) {
            if (!SetBreakpoint(reinterpret_cast<Instruction*>(value))) {
              PrintF("setting breakpoint failed\n");
            }
          } else {
            PrintF("%s unrecognized\n", arg1);
          }
        } else {
          PrintF("break <address>\n");
        }
      } else if (strcmp(cmd, "del") == 0) {
        if (!DeleteBreakpoint(NULL)) {
          PrintF("deleting breakpoint failed\n");
        }
      } else if (strcmp(cmd, "flags") == 0) {
        PrintF("No flags on MIPS !\n");
      } else if (strcmp(cmd, "unstop") == 0) {
          PrintF("Unstop command not implemented on MIPS.");
      } else if ((strcmp(cmd, "stat") == 0) || (strcmp(cmd, "st") == 0)) {
        // Print registers and disassemble
        PrintAllRegs();
        PrintF("\n");

        disasm::NameConverter converter;
        disasm::Disassembler dasm(converter);
        // use a reasonably large buffer
        v8::internal::EmbeddedVector<char, 256> buffer;

        byte_* cur = NULL;
        byte_* end = NULL;

        if (args == 1) {
          cur = reinterpret_cast<byte_*>(sim_->get_pc());
          end = cur + (10 * Instruction::kInstructionSize);
        } else if (args == 2) {
          int32_t value;
          if (GetValue(arg1, &value)) {
            cur = reinterpret_cast<byte_*>(value);
            // no length parameter passed, assume 10 instructions
            end = cur + (10 * Instruction::kInstructionSize);
          }
        } else {
          int32_t value1;
          int32_t value2;
          if (GetValue(arg1, &value1) && GetValue(arg2, &value2)) {
            cur = reinterpret_cast<byte_*>(value1);
            end = cur + (value2 * Instruction::kInstructionSize);
          }
        }

        while (cur < end) {
          dasm.InstructionDecode(buffer, cur);
          PrintF("  0x%08x  %s\n", cur, buffer.start());
          cur += Instruction::kInstructionSize;
        }
      } else if ((strcmp(cmd, "h") == 0) || (strcmp(cmd, "help") == 0)) {
        PrintF("cont\n");
        PrintF("  continue execution (alias 'c')\n");
        PrintF("stepi\n");
        PrintF("  step one instruction (alias 'si')\n");
        PrintF("print <register>\n");
        PrintF("  print register content (alias 'p')\n");
        PrintF("  use register name 'all' to print all registers\n");
        PrintF("printobject <register>\n");
        PrintF("  print an object from a register (alias 'po')\n");
        PrintF("flags\n");
        PrintF("  print flags\n");
        PrintF("disasm [<instructions>]\n");
        PrintF("disasm [[<address>] <instructions>]\n");
        PrintF("  disassemble code, default is 10 instructions from pc\n");
        PrintF("gdb\n");
        PrintF("  enter gdb\n");
        PrintF("break <address>\n");
        PrintF("  set a break point on the address\n");
        PrintF("del\n");
        PrintF("  delete the breakpoint\n");
        PrintF("unstop\n");
        PrintF("  ignore the stop instruction at the current location");
        PrintF(" from now on\n");
      } else {
        PrintF("Unknown command: %s\n", cmd);
      }
    }
    DeleteArray(line);
  }

  // Add all the breakpoints back to stop execution and enter the debugger
  // shell when hit.
  RedoBreakpoints();

#undef COMMAND_SIZE
#undef ARG_SIZE

#undef STR
#undef XSTR
}


// Create one simulator per thread and keep it in thread local storage.
static v8::internal::Thread::LocalStorageKey simulator_key;


bool Simulator::initialized_ = false;


void Simulator::Initialize() {
  if (initialized_) return;
  simulator_key = v8::internal::Thread::CreateThreadLocalKey();
  initialized_ = true;
  ::v8::internal::ExternalReference::set_redirector(&RedirectExternalReference);
}


Simulator::Simulator() {
  Initialize();
  // Setup simulator support first. Some of this information is needed to
  // setup the architecture state.
  size_t stack_size = 1 * 1024*1024;  // allocate 1MB for stack
  stack_ = reinterpret_cast<char*>(malloc(stack_size));
  pc_modified_ = false;
  icount_ = 0;
  break_pc_ = NULL;
  break_instr_ = 0;

  // Setup architecture state.
  // All registers are initialized to zero to start with.
  for (int i = 0; i < kNumSimuRegisters; i++) {
    registers_[i] = 0;
  }

  // The sp is initialized to point to the bottom (high address) of the
  // allocated stack area. To be safe in potential stack underflows we leave
  // some buffer below.
  registers_[sp] = reinterpret_cast<int32_t>(stack_) + stack_size - 64;
  // The ra and pc are initialized to a known bad value that will cause an
  // access violation if the simulator ever tries to execute it.
  registers_[pc] = bad_ra;
  registers_[ra] = bad_ra;
  InitializeCoverage();
}


// When the generated code calls an external reference we need to catch that in
// the simulator.  The external reference will be a function compiled for the
// host architecture.  We need to call that function instead of trying to
// execute it with the simulator.  We do that by redirecting the external
// reference to a swi (software-interrupt) instruction that is handled by
// the simulator.  We write the original destination of the jump just at a known
// offset from the swi instruction so the simulator knows what to call.
class Redirection {
 public:
  Redirection(void* external_function, bool fp_return)
      : external_function_(external_function),
        swi_instruction_(rtCallRedirInstr),
        fp_return_(fp_return),
        next_(list_) {
    list_ = this;
  }

  void* address_of_swi_instruction() {
    return reinterpret_cast<void*>(&swi_instruction_);
  }

  void* external_function() { return external_function_; }
  bool fp_return() { return fp_return_; }

  static Redirection* Get(void* external_function, bool fp_return) {
    Redirection* current;
    for (current = list_; current != NULL; current = current->next_) {
      if (current->external_function_ == external_function) return current;
    }
    return new Redirection(external_function, fp_return);
  }

  static Redirection* FromSwiInstruction(Instruction* swi_instruction) {
    char* addr_of_swi = reinterpret_cast<char*>(swi_instruction);
    char* addr_of_redirection =
        addr_of_swi - OFFSET_OF(Redirection, swi_instruction_);
    return reinterpret_cast<Redirection*>(addr_of_redirection);
  }

 private:
  void* external_function_;
  uint32_t swi_instruction_;
  bool fp_return_;
  Redirection* next_;
  static Redirection* list_;
};


Redirection* Redirection::list_ = NULL;


void* Simulator::RedirectExternalReference(void* external_function,
                                           bool fp_return) {
  Redirection* redirection = Redirection::Get(external_function, fp_return);
  return redirection->address_of_swi_instruction();
}


// Get the active Simulator for the current thread.
Simulator* Simulator::current() {
  Initialize();
  Simulator* sim = reinterpret_cast<Simulator*>(
      v8::internal::Thread::GetThreadLocal(simulator_key));
  if (sim == NULL) {
    // TODO(146): delete the simulator object when a thread goes away.
    sim = new Simulator();
    v8::internal::Thread::SetThreadLocal(simulator_key, sim);
  }
  return sim;
}


// Sets the register in the architecture state. It will also deal with updating
// Simulator internal state for special registers such as PC.
void Simulator::set_register(int reg, int32_t value) {
  ASSERT((reg >= 0) && (reg < kNumSimuRegisters));
  if (reg == pc) {
    pc_modified_ = true;
  }

  // zero register always hold 0.
  registers_[reg] = (reg == 0) ? 0 : value;
}

void Simulator::set_fpu_register(int fpureg, int32_t value) {
  ASSERT((fpureg >= 0) && (fpureg < kNumFPURegisters));
  FPUregisters_[fpureg] = value;
}

void Simulator::set_fpu_register_double(int fpureg, double value) {
  ASSERT((fpureg >= 0) && (fpureg < kNumFPURegisters) && ((fpureg % 2) == 0));
  *v8i::BitCast<double*, int32_t*>(&FPUregisters_[fpureg]) = value;
}


// Get the register from the architecture state. This function does handle
// the special case of accessing the PC register.
int32_t Simulator::get_register(int reg) const {
  ASSERT((reg >= 0) && (reg < kNumSimuRegisters));
  if (reg == 0)
    return 0;
  else
    return registers_[reg] + ((reg == pc) ? Instruction::kPCReadOffset : 0);
}

int32_t Simulator::get_fpu_register(int fpureg) const {
  ASSERT((fpureg >= 0) && (fpureg < kNumFPURegisters));
  return FPUregisters_[fpureg];
}

double Simulator::get_fpu_register_double(int fpureg) const {
  ASSERT((fpureg >= 0) && (fpureg < kNumFPURegisters) && ((fpureg % 2) == 0));
  return *v8i::BitCast<double*, int32_t*>(
      const_cast<int32_t*>(&FPUregisters_[fpureg]));
}

// Raw access to the PC register.
void Simulator::set_pc(int32_t value) {
  pc_modified_ = true;
  registers_[pc] = value;
}

// Raw access to the PC register without the special adjustment when reading.
int32_t Simulator::get_pc() const {
  return registers_[pc];
}


// The MIPS cannot do unaligned reads and writes.  On some MIPS platforms an
// interrupt is caused.  On others it does a funky rotation thing.  For now we
// simply disallow unaligned reads, but at some point we may want to move to
// emulating the rotate behaviour.  Note that simulator runs have the runtime
// system running directly on the host system and only generated code is
// executed in the simulator.  Since the host is typically IA32 we will not
// get the correct MIPS-like behaviour on unaligned accesses.

int Simulator::ReadW(int32_t addr, Instruction* instr) {
  if ((addr & v8i::kPointerAlignmentMask) == 0) {
    intptr_t* ptr = reinterpret_cast<intptr_t*>(addr);
    return *ptr;
  }
  PrintF("Unaligned read at 0x%08x, pc=%p\n", addr, instr);
  OS::Abort();
  return 0;
}


void Simulator::WriteW(int32_t addr, int value, Instruction* instr) {
  if ((addr & v8i::kPointerAlignmentMask) == 0) {
    intptr_t* ptr = reinterpret_cast<intptr_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned write at 0x%08x, pc=%p\n", addr, instr);
  OS::Abort();
}


double Simulator::ReadD(int32_t addr, Instruction* instr) {
  if ((addr & kDoubleAlignmentMask) == 0) {
    double* ptr = reinterpret_cast<double*>(addr);
    return *ptr;
  }
  PrintF("Unaligned read at 0x%08x, pc=%p\n", addr, instr);
  OS::Abort();
  return 0;
}


void Simulator::WriteD(int32_t addr, double value, Instruction* instr) {
  if ((addr & kDoubleAlignmentMask) == 0) {
    double* ptr = reinterpret_cast<double*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned write at 0x%08x, pc=%p\n", addr, instr);
  OS::Abort();
}


uint16_t Simulator::ReadHU(int32_t addr, Instruction* instr) {
  if ((addr & 1) == 0) {
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    return *ptr;
  }
  PrintF("Unaligned unsigned halfword read at 0x%08x, pc=%p\n", addr, instr);
  OS::Abort();
  return 0;
}


int16_t Simulator::ReadH(int32_t addr, Instruction* instr) {
  if ((addr & 1) == 0) {
    int16_t* ptr = reinterpret_cast<int16_t*>(addr);
    return *ptr;
  }
  PrintF("Unaligned signed halfword read at 0x%08x, pc=%p\n", addr, instr);
  OS::Abort();
  return 0;
}


void Simulator::WriteH(int32_t addr, uint16_t value, Instruction* instr) {
  if ((addr & 1) == 0) {
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned unsigned halfword write at 0x%08x, pc=%p\n", addr, instr);
  OS::Abort();
}


void Simulator::WriteH(int32_t addr, int16_t value, Instruction* instr) {
  if ((addr & 1) == 0) {
    int16_t* ptr = reinterpret_cast<int16_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned halfword write at 0x%08x, pc=%p\n", addr, instr);
  OS::Abort();
}


uint32_t Simulator::ReadBU(int32_t addr) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  return *ptr & 0xff;
}


int32_t Simulator::ReadB(int32_t addr) {
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  return ((*ptr << 24) >> 24) & 0xff;
}


void Simulator::WriteB(int32_t addr, uint8_t value) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  *ptr = value;
}


void Simulator::WriteB(int32_t addr, int8_t value) {
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  *ptr = value;
}


// Returns the limit of the stack area to enable checking for stack overflows.
uintptr_t Simulator::StackLimit() const {
  // Leave a safety margin of 256 bytes to prevent overrunning the stack when
  // pushing values.
  return reinterpret_cast<uintptr_t>(stack_) + 256;
}


// Unsupported instructions use Format to print an error and stop execution.
void Simulator::Format(Instruction* instr, const char* format) {
  PrintF("Simulator found unsupported instruction:\n 0x%08x: %s\n",
         instr, format);
  UNIMPLEMENTED_MIPS();
}


// Calls into the V8 runtime are based on this very simple interface.
// Note: To be able to return two values from some calls the code in runtime.cc
// uses the ObjectPair which is essentially two 32-bit values stuffed into a
// 64-bit value. With the code below we assume that all runtime calls return
// 64 bits of result. If they don't, the r1 result register contains a bogus
// value, which is fine because it is caller-saved.
typedef int64_t (*SimulatorRuntimeCall)(int32_t arg0,
                                        int32_t arg1,
                                        int32_t arg2,
                                        int32_t arg3);
typedef double (*SimulatorRuntimeFPCall)(double fparg0,
                                         double fparg1);


// Software interrupt instructions are used by the simulator to call into the
// C-based V8 runtime.
void Simulator::SoftwareInterrupt(Instruction* instr) {
  // We first check if we met a call_rt_redirected.
  if (instr->InstructionBits() == rtCallRedirInstr) {
    Redirection* redirection = Redirection::FromSwiInstruction(instr);
    int32_t arg0 = get_register(a0);
    int32_t arg1 = get_register(a1);
    int32_t arg2 = get_register(a2);
    int32_t arg3 = get_register(a3);
    // fp args are (not always) in f12 and f14.
    // See MIPS conventions for more details.
    double fparg0 = get_fpu_register_double(f12);
    double fparg1 = get_fpu_register_double(f14);
    // This is dodgy but it works because the C entry stubs are never moved.
    // See comment in codegen-arm.cc and bug 1242173.
    int32_t saved_ra = get_register(ra);
    if (redirection->fp_return()) {
      intptr_t external =
          reinterpret_cast<intptr_t>(redirection->external_function());
      SimulatorRuntimeFPCall target =
          reinterpret_cast<SimulatorRuntimeFPCall>(external);
      if (::v8::internal::FLAG_trace_sim) {
        PrintF("Call to host function at %p with args %f, %f\n",
               FUNCTION_ADDR(target), fparg0, fparg1);
      }
      double result = target(fparg0, fparg1);
      set_fpu_register_double(f0, result);
    } else {
      intptr_t external =
          reinterpret_cast<int32_t>(redirection->external_function());
      SimulatorRuntimeCall target =
          reinterpret_cast<SimulatorRuntimeCall>(external);
      if (::v8::internal::FLAG_trace_sim) {
        PrintF(
            "Call to host function at %p with args %08x, %08x, %08x, %08x\n",
            FUNCTION_ADDR(target),
            arg0,
            arg1,
            arg2,
            arg3);
      }
      int64_t result = target(arg0, arg1, arg2, arg3);
      int32_t lo_res = static_cast<int32_t>(result);
      int32_t hi_res = static_cast<int32_t>(result >> 32);
      if (::v8::internal::FLAG_trace_sim) {
        PrintF("Returned %08x\n", lo_res);
      }
      set_register(v0, lo_res);
      set_register(v1, hi_res);
    }
    set_register(ra, saved_ra);
    set_pc(get_register(ra));
  } else {
    Debugger dbg(this);
    dbg.Debug();
  }
}

void Simulator::SignalExceptions() {
  for (int i = 1; i < kNumExceptions; i++) {
    if (exceptions[i] != 0) {
      V8_Fatal(__FILE__, __LINE__, "Error: Exception %i raised.", i);
    }
  }
}

// Handle execution based on instruction types.
void Simulator::DecodeTypeRegister(Instruction* instr) {
  // Instruction fields
  Opcode   op     = instr->OpcodeFieldRaw();
  int32_t  rs_reg = instr->RsField();
  int32_t  rs     = get_register(rs_reg);
  uint32_t rs_u   = static_cast<uint32_t>(rs);
  int32_t  rt_reg = instr->RtField();
  int32_t  rt     = get_register(rt_reg);
  uint32_t rt_u   = static_cast<uint32_t>(rt);
  int32_t  rd_reg = instr->RdField();
  uint32_t sa     = instr->SaField();

  int32_t fs_reg= instr->FsField();

  // ALU output
  // It should not be used as is. Instructions using it should always initialize
  // it first.
  int32_t alu_out = 0x12345678;
  // Output or temporary for floating point.
  double fp_out = 0.0;

  // For break and trap instructions.
  bool do_interrupt = false;

  // For jr and jalr
  // Get current pc.
  int32_t current_pc = get_pc();
  // Next pc
  int32_t next_pc = 0;

  // ---------- Configuration
  switch (op) {
    case COP1:    // Coprocessor instructions
      switch (instr->RsFieldRaw()) {
        case BC1:   // branch on coprocessor condition
          UNREACHABLE();
          break;
        case MFC1:
          alu_out = get_fpu_register(fs_reg);
          break;
        case MFHC1:
          fp_out = get_fpu_register_double(fs_reg);
          alu_out = *v8i::BitCast<int32_t*, double*>(&fp_out);
          break;
        case MTC1:
        case MTHC1:
          // Do the store in the execution step.
          break;
        case S:
        case D:
        case W:
        case L:
        case PS:
          // Do everything in the execution step.
          break;
        default:
          UNIMPLEMENTED_MIPS();
      };
      break;
    case SPECIAL:
      switch (instr->FunctionFieldRaw()) {
        case JR:
        case JALR:
          next_pc = get_register(instr->RsField());
          break;
        case SLL:
          alu_out = rt << sa;
          break;
        case SRL:
          alu_out = rt_u >> sa;
          break;
        case SRA:
          alu_out = rt >> sa;
          break;
        case SLLV:
          alu_out = rt << rs;
          break;
        case SRLV:
          alu_out = rt_u >> rs;
          break;
        case SRAV:
          alu_out = rt >> rs;
          break;
        case MFHI:
          alu_out = get_register(HI);
          break;
        case MFLO:
          alu_out = get_register(LO);
          break;
        case MULT:
          UNIMPLEMENTED_MIPS();
          break;
        case MULTU:
          UNIMPLEMENTED_MIPS();
          break;
        case DIV:
        case DIVU:
            exceptions[kDivideByZero] = rt == 0;
          break;
        case ADD:
          if (HaveSameSign(rs, rt)) {
            if (rs > 0) {
              exceptions[kIntegerOverflow] = rs > (Registers::kMaxValue - rt);
            } else if (rs < 0) {
              exceptions[kIntegerUnderflow] = rs < (Registers::kMinValue - rt);
            }
          }
          alu_out = rs + rt;
          break;
        case ADDU:
          alu_out = rs + rt;
          break;
        case SUB:
          if (!HaveSameSign(rs, rt)) {
            if (rs > 0) {
              exceptions[kIntegerOverflow] = rs > (Registers::kMaxValue + rt);
            } else if (rs < 0) {
              exceptions[kIntegerUnderflow] = rs < (Registers::kMinValue + rt);
            }
          }
          alu_out = rs - rt;
          break;
        case SUBU:
          alu_out = rs - rt;
          break;
        case AND:
          alu_out = rs & rt;
          break;
        case OR:
          alu_out = rs | rt;
          break;
        case XOR:
          alu_out = rs ^ rt;
          break;
        case NOR:
          alu_out = ~(rs | rt);
          break;
        case SLT:
          alu_out = rs < rt ? 1 : 0;
          break;
        case SLTU:
          alu_out = rs_u < rt_u ? 1 : 0;
          break;
        // Break and trap instructions
        case BREAK:
          do_interrupt = true;
          break;
        case TGE:
          do_interrupt = rs >= rt;
          break;
        case TGEU:
          do_interrupt = rs_u >= rt_u;
          break;
        case TLT:
          do_interrupt = rs < rt;
          break;
        case TLTU:
          do_interrupt = rs_u < rt_u;
          break;
        case TEQ:
          do_interrupt = rs == rt;
          break;
        case TNE:
          do_interrupt = rs != rt;
          break;
        default:
          UNREACHABLE();
      };
      break;
    case SPECIAL2:
      switch (instr->FunctionFieldRaw()) {
        case MUL:
          alu_out = rs_u * rt_u;  // Only the lower 32 bits are kept.
          break;
        default:
          UNREACHABLE();
      }
      break;
    default:
      UNREACHABLE();
  };

  // ---------- Raise exceptions triggered.
  SignalExceptions();

  // ---------- Execution
  switch (op) {
    case COP1:
      switch (instr->RsFieldRaw()) {
        case BC1:   // branch on coprocessor condition
          UNREACHABLE();
          break;
        case MFC1:
        case MFHC1:
          set_register(rt_reg, alu_out);
          break;
        case MTC1:
          // We don't need to set the higher bits to 0, because MIPS ISA says
          // they are in an unpredictable state after executing MTC1.
          FPUregisters_[fs_reg] = registers_[rt_reg];
          FPUregisters_[fs_reg+1] = Unpredictable;
          break;
        case MTHC1:
          // Here we need to keep the lower bits unchanged.
          FPUregisters_[fs_reg+1] = registers_[rt_reg];
          break;
        case S:
          switch (instr->FunctionFieldRaw()) {
            case CVT_D_S:
            case CVT_W_S:
            case CVT_L_S:
            case CVT_PS_S:
              UNIMPLEMENTED_MIPS();
              break;
            default:
              UNREACHABLE();
          }
          break;
        case D:
          switch (instr->FunctionFieldRaw()) {
            case CVT_S_D:
            case CVT_W_D:
            case CVT_L_D:
              UNIMPLEMENTED_MIPS();
              break;
            default:
              UNREACHABLE();
          }
          break;
        case W:
          switch (instr->FunctionFieldRaw()) {
            case CVT_S_W:
              UNIMPLEMENTED_MIPS();
              break;
            case CVT_D_W:   // Convert word to double.
              set_fpu_register(rd_reg, static_cast<double>(rs));
              break;
            default:
              UNREACHABLE();
          };
          break;
        case L:
          switch (instr->FunctionFieldRaw()) {
            case CVT_S_L:
            case CVT_D_L:
              UNIMPLEMENTED_MIPS();
              break;
            default:
              UNREACHABLE();
          }
          break;
        case PS:
          break;
        default:
          UNREACHABLE();
      };
      break;
    case SPECIAL:
      switch (instr->FunctionFieldRaw()) {
        case JR: {
          Instruction* branch_delay_instr = reinterpret_cast<Instruction*>(
              current_pc+Instruction::kInstructionSize);
          BranchDelayInstructionDecode(branch_delay_instr);
          set_pc(next_pc);
          pc_modified_ = true;
          break;
        }
        case JALR: {
          Instruction* branch_delay_instr = reinterpret_cast<Instruction*>(
              current_pc+Instruction::kInstructionSize);
          BranchDelayInstructionDecode(branch_delay_instr);
          set_register(31, current_pc + 2* Instruction::kInstructionSize);
          set_pc(next_pc);
          pc_modified_ = true;
          break;
        }
        // Instructions using HI and LO registers.
        case MULT:
        case MULTU:
          break;
        case DIV:
          // Divide by zero was checked in the configuration step.
          set_register(LO, rs / rt);
          set_register(HI, rs % rt);
          break;
        case DIVU:
          set_register(LO, rs_u / rt_u);
          set_register(HI, rs_u % rt_u);
          break;
        // Break and trap instructions
        case BREAK:
        case TGE:
        case TGEU:
        case TLT:
        case TLTU:
        case TEQ:
        case TNE:
          if (do_interrupt) {
            SoftwareInterrupt(instr);
          }
          break;
        default:  // For other special opcodes we do the default operation.
          set_register(rd_reg, alu_out);
      };
      break;
    case SPECIAL2:
      switch (instr->FunctionFieldRaw()) {
        case MUL:
          set_register(rd_reg, alu_out);
          // HI and LO are UNPREDICTABLE after the operation.
          set_register(LO, Unpredictable);
          set_register(HI, Unpredictable);
          break;
        default:
          UNREACHABLE();
      }
      break;
    // Unimplemented opcodes raised an error in the configuration step before,
    // so we can use the default here to set the destination register in common
    // cases.
    default:
      set_register(rd_reg, alu_out);
  };
}

// Type 2: instructions using a 16 bytes immediate. (eg: addi, beq)
void Simulator::DecodeTypeImmediate(Instruction* instr) {
  // Instruction fields
  Opcode   op     = instr->OpcodeFieldRaw();
  int32_t  rs     = get_register(instr->RsField());
  uint32_t rs_u   = static_cast<uint32_t>(rs);
  int32_t  rt_reg = instr->RtField();  // destination register
  int32_t  rt     = get_register(rt_reg);
  int16_t  imm16  = instr->Imm16Field();

  int32_t  ft_reg = instr->FtField();  // destination register
  int32_t  ft     = get_register(ft_reg);

  // zero extended immediate
  uint32_t  oe_imm16 = 0xffff & imm16;
  // sign extended immediate
  int32_t   se_imm16 = imm16;

  // Get current pc.
  int32_t current_pc = get_pc();
  // Next pc.
  int32_t next_pc = bad_ra;

  // Used for conditional branch instructions
  bool do_branch = false;
  bool execute_branch_delay_instruction = false;

  // Used for arithmetic instructions
  int32_t alu_out = 0;
  // Floating point
  double fp_out = 0.0;

  // Used for memory instructions
  int32_t addr = 0x0;

  // ---------- Configuration (and execution for REGIMM)
  switch (op) {
    // ------------- COP1. Coprocessor instructions
    case COP1:
      switch (instr->RsFieldRaw()) {
        case BC1:   // branch on coprocessor condition
          UNIMPLEMENTED_MIPS();
          break;
        default:
          UNREACHABLE();
      };
      break;
    // ------------- REGIMM class
    case REGIMM:
      switch (instr->RtFieldRaw()) {
        case BLTZ:
          do_branch = (rs  < 0);
          break;
        case BLTZAL:
          do_branch = rs  < 0;
          break;
        case BGEZ:
          do_branch = rs >= 0;
          break;
        case BGEZAL:
          do_branch = rs >= 0;
          break;
        default:
          UNREACHABLE();
      };
      switch (instr->RtFieldRaw()) {
        case BLTZ:
        case BLTZAL:
        case BGEZ:
        case BGEZAL:
          // Branch instructions common part.
          execute_branch_delay_instruction = true;
          // Set next_pc
          if (do_branch) {
            next_pc = current_pc + (imm16 << 2) + Instruction::kInstructionSize;
            if (instr->IsLinkingInstruction()) {
              set_register(31, current_pc + kBranchReturnOffset);
            }
          } else {
            next_pc = current_pc + kBranchReturnOffset;
          }
        default:
          break;
        };
    break;  // case REGIMM
    // ------------- Branch instructions
    // When comparing to zero, the encoding of rt field is always 0, so we don't
    // need to replace rt with zero.
    case BEQ:
      do_branch = (rs == rt);
      break;
    case BNE:
      do_branch = rs != rt;
      break;
    case BLEZ:
      do_branch = rs <= 0;
      break;
    case BGTZ:
      do_branch = rs  > 0;
      break;
    // ------------- Arithmetic instructions
    case ADDI:
      if (HaveSameSign(rs, se_imm16)) {
        if (rs > 0) {
          exceptions[kIntegerOverflow] = rs > (Registers::kMaxValue - se_imm16);
        } else if (rs < 0) {
          exceptions[kIntegerUnderflow] =
              rs < (Registers::kMinValue - se_imm16);
        }
      }
      alu_out = rs + se_imm16;
      break;
    case ADDIU:
      alu_out = rs + se_imm16;
      break;
    case SLTI:
      alu_out = (rs < se_imm16) ? 1 : 0;
      break;
    case SLTIU:
      alu_out = (rs_u < static_cast<uint32_t>(se_imm16)) ? 1 : 0;
      break;
    case ANDI:
        alu_out = rs & oe_imm16;
      break;
    case ORI:
        alu_out = rs | oe_imm16;
      break;
    case XORI:
        alu_out = rs ^ oe_imm16;
      break;
    case LUI:
        alu_out = (oe_imm16 << 16);
      break;
    // ------------- Memory instructions
    case LB:
      addr = rs + se_imm16;
      alu_out = ReadB(addr);
      break;
    case LW:
      addr = rs + se_imm16;
      alu_out = ReadW(addr, instr);
      break;
    case LBU:
      addr = rs + se_imm16;
      alu_out = ReadBU(addr);
      break;
    case SB:
      addr = rs + se_imm16;
      break;
    case SW:
      addr = rs + se_imm16;
      break;
    case LWC1:
      addr = rs + se_imm16;
      alu_out = ReadW(addr, instr);
      break;
    case LDC1:
      addr = rs + se_imm16;
      fp_out = ReadD(addr, instr);
      break;
    case SWC1:
    case SDC1:
      addr = rs + se_imm16;
      break;
    default:
      UNREACHABLE();
  };

  // ---------- Raise exceptions triggered.
  SignalExceptions();

  // ---------- Execution
  switch (op) {
    // ------------- Branch instructions
    case BEQ:
    case BNE:
    case BLEZ:
    case BGTZ:
      // Branch instructions common part.
      execute_branch_delay_instruction = true;
      // Set next_pc
      if (do_branch) {
        next_pc = current_pc + (imm16 << 2) + Instruction::kInstructionSize;
        if (instr->IsLinkingInstruction()) {
          set_register(31, current_pc + 2* Instruction::kInstructionSize);
        }
      } else {
        next_pc = current_pc + 2 * Instruction::kInstructionSize;
      }
      break;
    // ------------- Arithmetic instructions
    case ADDI:
    case ADDIU:
    case SLTI:
    case SLTIU:
    case ANDI:
    case ORI:
    case XORI:
    case LUI:
      set_register(rt_reg, alu_out);
      break;
    // ------------- Memory instructions
    case LB:
    case LW:
    case LBU:
      set_register(rt_reg, alu_out);
      break;
    case SB:
      WriteB(addr, static_cast<int8_t>(rt));
      break;
    case SW:
      WriteW(addr, rt, instr);
      break;
    case LWC1:
      set_fpu_register(ft_reg, alu_out);
      break;
    case LDC1:
      set_fpu_register_double(ft_reg, fp_out);
      break;
    case SWC1:
      addr = rs + se_imm16;
      WriteW(addr, get_fpu_register(ft_reg), instr);
      break;
    case SDC1:
      addr = rs + se_imm16;
      WriteD(addr, ft, instr);
      break;
    default:
      break;
  };


  if (execute_branch_delay_instruction) {
    // Execute branch delay slot
    // We don't check for end_sim_pc. First it should not be met as the current
    // pc is valid. Secondly a jump should always execute its branch delay slot.
    Instruction* branch_delay_instr =
      reinterpret_cast<Instruction*>(current_pc+Instruction::kInstructionSize);
    BranchDelayInstructionDecode(branch_delay_instr);
  }

  // If needed update pc after the branch delay execution.
  if (next_pc != bad_ra) {
    set_pc(next_pc);
  }
}

// Type 3: instructions using a 26 bytes immediate. (eg: j, jal)
void Simulator::DecodeTypeJump(Instruction* instr) {
  // Get current pc.
  int32_t current_pc = get_pc();
  // Get unchanged bits of pc.
  int32_t pc_high_bits = current_pc & 0xf0000000;
  // Next pc
  int32_t next_pc = pc_high_bits | (instr->Imm26Field() << 2);

  // Execute branch delay slot
  // We don't check for end_sim_pc. First it should not be met as the current pc
  // is valid. Secondly a jump should always execute its branch delay slot.
  Instruction* branch_delay_instr =
    reinterpret_cast<Instruction*>(current_pc+Instruction::kInstructionSize);
  BranchDelayInstructionDecode(branch_delay_instr);

  // Update pc and ra if necessary.
  // Do this after the branch delay execution.
  if (instr->IsLinkingInstruction()) {
    set_register(31, current_pc + 2* Instruction::kInstructionSize);
  }
  set_pc(next_pc);
  pc_modified_ = true;
}

// Executes the current instruction.
void Simulator::InstructionDecode(Instruction* instr) {
  pc_modified_ = false;
  if (::v8::internal::FLAG_trace_sim) {
    disasm::NameConverter converter;
    disasm::Disassembler dasm(converter);
    // use a reasonably large buffer
    v8::internal::EmbeddedVector<char, 256> buffer;
    dasm.InstructionDecode(buffer,
                           reinterpret_cast<byte_*>(instr));
    PrintF("  0x%08x  %s\n", instr, buffer.start());
  }

  switch (instr->InstructionType()) {
    case Instruction::kRegisterType:
      DecodeTypeRegister(instr);
      break;
    case Instruction::kImmediateType:
      DecodeTypeImmediate(instr);
      break;
    case Instruction::kJumpType:
      DecodeTypeJump(instr);
      break;
    default:
      UNSUPPORTED();
  }
  if (!pc_modified_) {
    set_register(pc, reinterpret_cast<int32_t>(instr) +
                 Instruction::kInstructionSize);
  }
}



void Simulator::Execute() {
  // Get the PC to simulate. Cannot use the accessor here as we need the
  // raw PC value and not the one used as input to arithmetic instructions.
  int program_counter = get_pc();
  if (::v8::internal::FLAG_stop_sim_at == 0) {
    // Fast version of the dispatch loop without checking whether the simulator
    // should be stopping at a particular executed instruction.
    while (program_counter != end_sim_pc) {
      Instruction* instr = reinterpret_cast<Instruction*>(program_counter);
      icount_++;
      InstructionDecode(instr);
      program_counter = get_pc();
    }
  } else {
    // FLAG_stop_sim_at is at the non-default value. Stop in the debugger when
    // we reach the particular instuction count.
    while (program_counter != end_sim_pc) {
      Instruction* instr = reinterpret_cast<Instruction*>(program_counter);
      icount_++;
      if (icount_ == ::v8::internal::FLAG_stop_sim_at) {
        Debugger dbg(this);
        dbg.Debug();
      } else {
        InstructionDecode(instr);
      }
      program_counter = get_pc();
    }
  }
}


int32_t Simulator::Call(byte_* entry, int argument_count, ...) {
  va_list parameters;
  va_start(parameters, argument_count);
  // Setup arguments

  // First four arguments passed in registers.
  ASSERT(argument_count >= 4);
  set_register(a0, va_arg(parameters, int32_t));
  set_register(a1, va_arg(parameters, int32_t));
  set_register(a2, va_arg(parameters, int32_t));
  set_register(a3, va_arg(parameters, int32_t));

  // Remaining arguments passed on stack.
  int original_stack = get_register(sp);
  // Compute position of stack on entry to generated code.
  int entry_stack = (original_stack - (argument_count - 4) * sizeof(int32_t)
                                    - kArgsSlotsSize);
  if (OS::ActivationFrameAlignment() != 0) {
    entry_stack &= -OS::ActivationFrameAlignment();
  }
  // Store remaining arguments on stack, from low to high memory.
  intptr_t* stack_argument = reinterpret_cast<intptr_t*>(entry_stack);
  for (int i = 4; i < argument_count; i++) {
    stack_argument[i - 4 + kArgsSlotsNum] = va_arg(parameters, int32_t);
  }
  va_end(parameters);
  set_register(sp, entry_stack);

  // Prepare to execute the code at entry
  set_register(pc, reinterpret_cast<int32_t>(entry));
  // Put down marker for end of simulation. The simulator will stop simulation
  // when the PC reaches this value. By saving the "end simulation" value into
  // the LR the simulation stops when returning to this call point.
  set_register(ra, end_sim_pc);

  // Remember the values of callee-saved registers.
  // The code below assumes that r9 is not used as sb (static base) in
  // simulator code and therefore is regarded as a callee-saved register.
  int32_t s0_val = get_register(s0);
  int32_t s1_val = get_register(s1);
  int32_t s2_val = get_register(s2);
  int32_t s3_val = get_register(s3);
  int32_t s4_val = get_register(s4);
  int32_t s5_val = get_register(s5);
  int32_t s6_val = get_register(s6);
  int32_t s7_val = get_register(s7);
  int32_t gp_val = get_register(gp);
  int32_t sp_val = get_register(sp);
  int32_t fp_val = get_register(fp);

  // Setup the callee-saved registers with a known value. To be able to check
  // that they are preserved properly across JS execution.
  int32_t callee_saved_value = icount_;
  set_register(s0, callee_saved_value);
  set_register(s1, callee_saved_value);
  set_register(s2, callee_saved_value);
  set_register(s3, callee_saved_value);
  set_register(s4, callee_saved_value);
  set_register(s5, callee_saved_value);
  set_register(s6, callee_saved_value);
  set_register(s7, callee_saved_value);
  set_register(gp, callee_saved_value);
  set_register(fp, callee_saved_value);

  // Start the simulation
  Execute();

  // Check that the callee-saved registers have been preserved.
  CHECK_EQ(callee_saved_value, get_register(s0));
  CHECK_EQ(callee_saved_value, get_register(s1));
  CHECK_EQ(callee_saved_value, get_register(s2));
  CHECK_EQ(callee_saved_value, get_register(s3));
  CHECK_EQ(callee_saved_value, get_register(s4));
  CHECK_EQ(callee_saved_value, get_register(s5));
  CHECK_EQ(callee_saved_value, get_register(s6));
  CHECK_EQ(callee_saved_value, get_register(s7));
  CHECK_EQ(callee_saved_value, get_register(gp));
  CHECK_EQ(callee_saved_value, get_register(fp));

  // Restore callee-saved registers with the original value.
  set_register(s0, s0_val);
  set_register(s1, s1_val);
  set_register(s2, s2_val);
  set_register(s3, s3_val);
  set_register(s4, s4_val);
  set_register(s5, s5_val);
  set_register(s6, s6_val);
  set_register(s7, s7_val);
  set_register(gp, gp_val);
  set_register(sp, sp_val);
  set_register(fp, fp_val);

  // Pop stack passed arguments.
  CHECK_EQ(entry_stack, get_register(sp));
  set_register(sp, original_stack);

  int32_t result = get_register(v0);
  return result;
}


uintptr_t Simulator::PushAddress(uintptr_t address) {
  int new_sp = get_register(sp) - sizeof(uintptr_t);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(new_sp);
  *stack_slot = address;
  set_register(sp, new_sp);
  return new_sp;
}


uintptr_t Simulator::PopAddress() {
  int current_sp = get_register(sp);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(current_sp);
  uintptr_t address = *stack_slot;
  set_register(sp, current_sp + sizeof(uintptr_t));
  return address;
}


#undef UNSUPPORTED

} }  // namespace assembler::mips

#endif  // __mips

