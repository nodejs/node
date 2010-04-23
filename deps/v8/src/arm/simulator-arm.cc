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
#include "arm/constants-arm.h"
#include "arm/simulator-arm.h"

#if !defined(__arm__)

// Only build the simulator if not compiling for real ARM hardware.
namespace assembler {
namespace arm {

using ::v8::internal::Object;
using ::v8::internal::PrintF;
using ::v8::internal::OS;
using ::v8::internal::ReadLine;
using ::v8::internal::DeleteArray;

// This macro provides a platform independent use of sscanf. The reason for
// SScanF not being implemented in a platform independent way through
// ::v8::internal::OS in the same way as SNPrintF is that the
// Windows C Run-Time Library does not provide vsscanf.
#define SScanF sscanf  // NOLINT

// The Debugger class is used by the simulator while debugging simulated ARM
// code.
class Debugger {
 public:
  explicit Debugger(Simulator* sim);
  ~Debugger();

  void Stop(Instr* instr);
  void Debug();

 private:
  static const instr_t kBreakpointInstr =
      ((AL << 28) | (7 << 25) | (1 << 24) | break_point);
  static const instr_t kNopInstr =
      ((AL << 28) | (13 << 21));

  Simulator* sim_;

  int32_t GetRegisterValue(int regnum);
  bool GetValue(const char* desc, int32_t* value);
  bool GetVFPSingleValue(const char* desc, float* value);
  bool GetVFPDoubleValue(const char* desc, double* value);

  // Set or delete a breakpoint. Returns true if successful.
  bool SetBreakpoint(Instr* breakpc);
  bool DeleteBreakpoint(Instr* breakpc);

  // Undo and redo all breakpoints. This is needed to bracket disassembly and
  // execution to skip past breakpoints when run from the debugger.
  void UndoBreakpoints();
  void RedoBreakpoints();
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


void Debugger::Stop(Instr* instr) {
  char* str = reinterpret_cast<char*>(instr->InstructionBits() & 0x0fffffff);
  if (strlen(str) > 0) {
    if (coverage_log != NULL) {
      fprintf(coverage_log, "%s\n", str);
      fflush(coverage_log);
    }
    instr->SetInstructionBits(0xe1a00000);  // Overwrite with nop.
  }
  sim_->set_pc(sim_->get_pc() + Instr::kInstrSize);
}

#else  // ndef GENERATED_CODE_COVERAGE

static void InitializeCoverage() {
}


void Debugger::Stop(Instr* instr) {
  const char* str = (const char*)(instr->InstructionBits() & 0x0fffffff);
  PrintF("Simulator hit %s\n", str);
  sim_->set_pc(sim_->get_pc() + Instr::kInstrSize);
  Debug();
}
#endif


int32_t Debugger::GetRegisterValue(int regnum) {
  if (regnum == kPCRegister) {
    return sim_->get_pc();
  } else {
    return sim_->get_register(regnum);
  }
}


bool Debugger::GetValue(const char* desc, int32_t* value) {
  int regnum = Registers::Number(desc);
  if (regnum != kNoRegister) {
    *value = GetRegisterValue(regnum);
    return true;
  } else {
    if (strncmp(desc, "0x", 2) == 0) {
      return SScanF(desc + 2, "%x", reinterpret_cast<uint32_t*>(value)) == 1;
    } else {
      return SScanF(desc, "%u", reinterpret_cast<uint32_t*>(value)) == 1;
    }
  }
  return false;
}


bool Debugger::GetVFPSingleValue(const char* desc, float* value) {
  bool is_double;
  int regnum = VFPRegisters::Number(desc, &is_double);
  if (regnum != kNoRegister && !is_double) {
    *value = sim_->get_float_from_s_register(regnum);
    return true;
  }
  return false;
}


bool Debugger::GetVFPDoubleValue(const char* desc, double* value) {
  bool is_double;
  int regnum = VFPRegisters::Number(desc, &is_double);
  if (regnum != kNoRegister && is_double) {
    *value = sim_->get_double_from_d_register(regnum);
    return true;
  }
  return false;
}


bool Debugger::SetBreakpoint(Instr* breakpc) {
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


bool Debugger::DeleteBreakpoint(Instr* breakpc) {
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
  char* argv[3] = { cmd, arg1, arg2 };

  // make sure to have a proper terminating character if reaching the limit
  cmd[COMMAND_SIZE] = 0;
  arg1[ARG_SIZE] = 0;
  arg2[ARG_SIZE] = 0;

  // Undo all set breakpoints while running in the debugger shell. This will
  // make them invisible to all commands.
  UndoBreakpoints();

  while (!done) {
    if (last_pc != sim_->get_pc()) {
      disasm::NameConverter converter;
      disasm::Disassembler dasm(converter);
      // use a reasonably large buffer
      v8::internal::EmbeddedVector<char, 256> buffer;
      dasm.InstructionDecode(buffer,
                             reinterpret_cast<byte*>(sim_->get_pc()));
      PrintF("  0x%08x  %s\n", sim_->get_pc(), buffer.start());
      last_pc = sim_->get_pc();
    }
    char* line = ReadLine("sim> ");
    if (line == NULL) {
      break;
    } else {
      // Use sscanf to parse the individual parts of the command line. At the
      // moment no command expects more than two parameters.
      int argc = SScanF(line,
                        "%" XSTR(COMMAND_SIZE) "s "
                        "%" XSTR(ARG_SIZE) "s "
                        "%" XSTR(ARG_SIZE) "s",
                        cmd, arg1, arg2);
      if ((strcmp(cmd, "si") == 0) || (strcmp(cmd, "stepi") == 0)) {
        sim_->InstructionDecode(reinterpret_cast<Instr*>(sim_->get_pc()));
      } else if ((strcmp(cmd, "c") == 0) || (strcmp(cmd, "cont") == 0)) {
        // Execute the one instruction we broke at with breakpoints disabled.
        sim_->InstructionDecode(reinterpret_cast<Instr*>(sim_->get_pc()));
        // Leave the debugger shell.
        done = true;
      } else if ((strcmp(cmd, "p") == 0) || (strcmp(cmd, "print") == 0)) {
        if (argc == 2) {
          int32_t value;
          float svalue;
          double dvalue;
          if (strcmp(arg1, "all") == 0) {
            for (int i = 0; i < kNumRegisters; i++) {
              value = GetRegisterValue(i);
              PrintF("%3s: 0x%08x %10d\n", Registers::Name(i), value, value);
            }
          } else {
            if (GetValue(arg1, &value)) {
              PrintF("%s: 0x%08x %d \n", arg1, value, value);
            } else if (GetVFPSingleValue(arg1, &svalue)) {
              PrintF("%s: %f \n", arg1, svalue);
            } else if (GetVFPDoubleValue(arg1, &dvalue)) {
              PrintF("%s: %lf \n", arg1, dvalue);
            } else {
              PrintF("%s unrecognized\n", arg1);
            }
          }
        } else {
          PrintF("print <register>\n");
        }
      } else if ((strcmp(cmd, "po") == 0)
                 || (strcmp(cmd, "printobject") == 0)) {
        if (argc == 2) {
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
      } else if (strcmp(cmd, "stack") == 0 || strcmp(cmd, "mem") == 0) {
        int32_t* cur = NULL;
        int32_t* end = NULL;
        int next_arg = 1;

        if (strcmp(cmd, "stack") == 0) {
          cur = reinterpret_cast<int32_t*>(sim_->get_register(Simulator::sp));
        } else {  // "mem"
          int32_t value;
          if (!GetValue(arg1, &value)) {
            PrintF("%s unrecognized\n", arg1);
            continue;
          }
          cur = reinterpret_cast<int32_t*>(value);
          next_arg++;
        }

        int32_t words;
        if (argc == next_arg) {
          words = 10;
        } else if (argc == next_arg + 1) {
          if (!GetValue(argv[next_arg], &words)) {
            words = 10;
          }
        }
        end = cur + words;

        while (cur < end) {
          PrintF("  0x%08x:  0x%08x %10d\n", cur, *cur, *cur);
          cur++;
        }
      } else if (strcmp(cmd, "disasm") == 0) {
        disasm::NameConverter converter;
        disasm::Disassembler dasm(converter);
        // use a reasonably large buffer
        v8::internal::EmbeddedVector<char, 256> buffer;

        byte* cur = NULL;
        byte* end = NULL;

        if (argc == 1) {
          cur = reinterpret_cast<byte*>(sim_->get_pc());
          end = cur + (10 * Instr::kInstrSize);
        } else if (argc == 2) {
          int32_t value;
          if (GetValue(arg1, &value)) {
            cur = reinterpret_cast<byte*>(value);
            // no length parameter passed, assume 10 instructions
            end = cur + (10 * Instr::kInstrSize);
          }
        } else {
          int32_t value1;
          int32_t value2;
          if (GetValue(arg1, &value1) && GetValue(arg2, &value2)) {
            cur = reinterpret_cast<byte*>(value1);
            end = cur + (value2 * Instr::kInstrSize);
          }
        }

        while (cur < end) {
          dasm.InstructionDecode(buffer, cur);
          PrintF("  0x%08x  %s\n", cur, buffer.start());
          cur += Instr::kInstrSize;
        }
      } else if (strcmp(cmd, "gdb") == 0) {
        PrintF("relinquishing control to gdb\n");
        v8::internal::OS::DebugBreak();
        PrintF("regaining control from gdb\n");
      } else if (strcmp(cmd, "break") == 0) {
        if (argc == 2) {
          int32_t value;
          if (GetValue(arg1, &value)) {
            if (!SetBreakpoint(reinterpret_cast<Instr*>(value))) {
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
        PrintF("N flag: %d; ", sim_->n_flag_);
        PrintF("Z flag: %d; ", sim_->z_flag_);
        PrintF("C flag: %d; ", sim_->c_flag_);
        PrintF("V flag: %d\n", sim_->v_flag_);
        PrintF("INVALID OP flag: %d; ", sim_->inv_op_vfp_flag_);
        PrintF("DIV BY ZERO flag: %d; ", sim_->div_zero_vfp_flag_);
        PrintF("OVERFLOW flag: %d; ", sim_->overflow_vfp_flag_);
        PrintF("UNDERFLOW flag: %d; ", sim_->underflow_vfp_flag_);
        PrintF("INEXACT flag: %d; ", sim_->inexact_vfp_flag_);
      } else if (strcmp(cmd, "unstop") == 0) {
        intptr_t stop_pc = sim_->get_pc() - Instr::kInstrSize;
        Instr* stop_instr = reinterpret_cast<Instr*>(stop_pc);
        if (stop_instr->ConditionField() == special_condition) {
          stop_instr->SetInstructionBits(kNopInstr);
        } else {
          PrintF("Not at debugger stop.");
        }
      } else if ((strcmp(cmd, "t") == 0) || strcmp(cmd, "trace") == 0) {
        ::v8::internal::FLAG_trace_sim = !::v8::internal::FLAG_trace_sim;
        PrintF("Trace of executed instructions is %s\n",
               ::v8::internal::FLAG_trace_sim ? "on" : "off");
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
        PrintF("stack [<words>]\n");
        PrintF("  dump stack content, default dump 10 words)\n");
        PrintF("mem <address> [<words>]\n");
        PrintF("  dump memory content, default dump 10 words)\n");
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
        PrintF("  from now on\n");
        PrintF("trace (alias 't')\n");
        PrintF("  toogle the tracing of all executed statements\n");
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
  for (int i = 0; i < num_registers; i++) {
    registers_[i] = 0;
  }
  n_flag_ = false;
  z_flag_ = false;
  c_flag_ = false;
  v_flag_ = false;

  // Initializing VFP registers.
  // All registers are initialized to zero to start with
  // even though s_registers_ & d_registers_ share the same
  // physical registers in the target.
  for (int i = 0; i < num_s_registers; i++) {
    vfp_register[i] = 0;
  }
  n_flag_FPSCR_ = false;
  z_flag_FPSCR_ = false;
  c_flag_FPSCR_ = false;
  v_flag_FPSCR_ = false;

  inv_op_vfp_flag_ = false;
  div_zero_vfp_flag_ = false;
  overflow_vfp_flag_ = false;
  underflow_vfp_flag_ = false;
  inexact_vfp_flag_ = false;

  // The sp is initialized to point to the bottom (high address) of the
  // allocated stack area. To be safe in potential stack underflows we leave
  // some buffer below.
  registers_[sp] = reinterpret_cast<int32_t>(stack_) + stack_size - 64;
  // The lr and pc are initialized to a known bad value that will cause an
  // access violation if the simulator ever tries to execute it.
  registers_[pc] = bad_lr;
  registers_[lr] = bad_lr;
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
        swi_instruction_((AL << 28) | (0xf << 24) | call_rt_redirected),
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

  static Redirection* FromSwiInstruction(Instr* swi_instruction) {
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
  ASSERT((reg >= 0) && (reg < num_registers));
  if (reg == pc) {
    pc_modified_ = true;
  }
  registers_[reg] = value;
}


// Get the register from the architecture state. This function does handle
// the special case of accessing the PC register.
int32_t Simulator::get_register(int reg) const {
  ASSERT((reg >= 0) && (reg < num_registers));
  return registers_[reg] + ((reg == pc) ? Instr::kPCReadOffset : 0);
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


// Getting from and setting into VFP registers.
void Simulator::set_s_register(int sreg, unsigned int value) {
  ASSERT((sreg >= 0) && (sreg < num_s_registers));
  vfp_register[sreg] = value;
}


unsigned int Simulator::get_s_register(int sreg) const {
  ASSERT((sreg >= 0) && (sreg < num_s_registers));
  return vfp_register[sreg];
}


void Simulator::set_s_register_from_float(int sreg, const float flt) {
  ASSERT((sreg >= 0) && (sreg < num_s_registers));
  // Read the bits from the single precision floating point value
  // into the unsigned integer element of vfp_register[] given by index=sreg.
  char buffer[sizeof(vfp_register[0])];
  memcpy(buffer, &flt, sizeof(vfp_register[0]));
  memcpy(&vfp_register[sreg], buffer, sizeof(vfp_register[0]));
}


void Simulator::set_s_register_from_sinteger(int sreg, const int sint) {
  ASSERT((sreg >= 0) && (sreg < num_s_registers));
  // Read the bits from the integer value into the unsigned integer element of
  // vfp_register[] given by index=sreg.
  char buffer[sizeof(vfp_register[0])];
  memcpy(buffer, &sint, sizeof(vfp_register[0]));
  memcpy(&vfp_register[sreg], buffer, sizeof(vfp_register[0]));
}


void Simulator::set_d_register_from_double(int dreg, const double& dbl) {
  ASSERT((dreg >= 0) && (dreg < num_d_registers));
  // Read the bits from the double precision floating point value into the two
  // consecutive unsigned integer elements of vfp_register[] given by index
  // 2*sreg and 2*sreg+1.
  char buffer[2 * sizeof(vfp_register[0])];
  memcpy(buffer, &dbl, 2 * sizeof(vfp_register[0]));
#ifndef BIG_ENDIAN_FLOATING_POINT
  memcpy(&vfp_register[dreg * 2], buffer, 2 * sizeof(vfp_register[0]));
#else
  memcpy(&vfp_register[dreg * 2], &buffer[4], sizeof(vfp_register[0]));
  memcpy(&vfp_register[dreg * 2 + 1], &buffer[0], sizeof(vfp_register[0]));
#endif
}


float Simulator::get_float_from_s_register(int sreg) {
  ASSERT((sreg >= 0) && (sreg < num_s_registers));

  float sm_val = 0.0;
  // Read the bits from the unsigned integer vfp_register[] array
  // into the single precision floating point value and return it.
  char buffer[sizeof(vfp_register[0])];
  memcpy(buffer, &vfp_register[sreg], sizeof(vfp_register[0]));
  memcpy(&sm_val, buffer, sizeof(vfp_register[0]));
  return(sm_val);
}


int Simulator::get_sinteger_from_s_register(int sreg) {
  ASSERT((sreg >= 0) && (sreg < num_s_registers));

  int sm_val = 0;
  // Read the bits from the unsigned integer vfp_register[] array
  // into the single precision floating point value and return it.
  char buffer[sizeof(vfp_register[0])];
  memcpy(buffer, &vfp_register[sreg], sizeof(vfp_register[0]));
  memcpy(&sm_val, buffer, sizeof(vfp_register[0]));
  return(sm_val);
}


double Simulator::get_double_from_d_register(int dreg) {
  ASSERT((dreg >= 0) && (dreg < num_d_registers));

  double dm_val = 0.0;
  // Read the bits from the unsigned integer vfp_register[] array
  // into the double precision floating point value and return it.
  char buffer[2 * sizeof(vfp_register[0])];
#ifdef BIG_ENDIAN_FLOATING_POINT
  memcpy(&buffer[0], &vfp_register[2 * dreg + 1], sizeof(vfp_register[0]));
  memcpy(&buffer[4], &vfp_register[2 * dreg], sizeof(vfp_register[0]));
#else
  memcpy(buffer, &vfp_register[2 * dreg], 2 * sizeof(vfp_register[0]));
#endif
  memcpy(&dm_val, buffer, 2 * sizeof(vfp_register[0]));
  return(dm_val);
}


// For use in calls that take two double values, constructed from r0, r1, r2
// and r3.
void Simulator::GetFpArgs(double* x, double* y) {
  // We use a char buffer to get around the strict-aliasing rules which
  // otherwise allow the compiler to optimize away the copy.
  char buffer[2 * sizeof(registers_[0])];
  // Registers 0 and 1 -> x.
  memcpy(buffer, registers_, sizeof(buffer));
  memcpy(x, buffer, sizeof(buffer));
  // Registers 2 and 3 -> y.
  memcpy(buffer, registers_ + 2, sizeof(buffer));
  memcpy(y, buffer, sizeof(buffer));
}


void Simulator::SetFpResult(const double& result) {
  char buffer[2 * sizeof(registers_[0])];
  memcpy(buffer, &result, sizeof(buffer));
  // result -> registers 0 and 1.
  memcpy(registers_, buffer, sizeof(buffer));
}


void Simulator::TrashCallerSaveRegisters() {
  // We don't trash the registers with the return value.
  registers_[2] = 0x50Bad4U;
  registers_[3] = 0x50Bad4U;
  registers_[12] = 0x50Bad4U;
}


// The ARM cannot do unaligned reads and writes.  On some ARM platforms an
// interrupt is caused.  On others it does a funky rotation thing.  For now we
// simply disallow unaligned reads, but at some point we may want to move to
// emulating the rotate behaviour.  Note that simulator runs have the runtime
// system running directly on the host system and only generated code is
// executed in the simulator.  Since the host is typically IA32 we will not
// get the correct ARM-like behaviour on unaligned accesses.

int Simulator::ReadW(int32_t addr, Instr* instr) {
  if ((addr & 3) == 0) {
    intptr_t* ptr = reinterpret_cast<intptr_t*>(addr);
    return *ptr;
  }
  PrintF("Unaligned read at 0x%08x\n", addr);
  UNIMPLEMENTED();
  return 0;
}


void Simulator::WriteW(int32_t addr, int value, Instr* instr) {
  if ((addr & 3) == 0) {
    intptr_t* ptr = reinterpret_cast<intptr_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned write at 0x%08x, pc=%p\n", addr, instr);
  UNIMPLEMENTED();
}


uint16_t Simulator::ReadHU(int32_t addr, Instr* instr) {
  if ((addr & 1) == 0) {
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    return *ptr;
  }
  PrintF("Unaligned unsigned halfword read at 0x%08x, pc=%p\n", addr, instr);
  UNIMPLEMENTED();
  return 0;
}


int16_t Simulator::ReadH(int32_t addr, Instr* instr) {
  if ((addr & 1) == 0) {
    int16_t* ptr = reinterpret_cast<int16_t*>(addr);
    return *ptr;
  }
  PrintF("Unaligned signed halfword read at 0x%08x\n", addr);
  UNIMPLEMENTED();
  return 0;
}


void Simulator::WriteH(int32_t addr, uint16_t value, Instr* instr) {
  if ((addr & 1) == 0) {
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned unsigned halfword write at 0x%08x, pc=%p\n", addr, instr);
  UNIMPLEMENTED();
}


void Simulator::WriteH(int32_t addr, int16_t value, Instr* instr) {
  if ((addr & 1) == 0) {
    int16_t* ptr = reinterpret_cast<int16_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned halfword write at 0x%08x, pc=%p\n", addr, instr);
  UNIMPLEMENTED();
}


uint8_t Simulator::ReadBU(int32_t addr) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  return *ptr;
}


int8_t Simulator::ReadB(int32_t addr) {
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  return *ptr;
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
void Simulator::Format(Instr* instr, const char* format) {
  PrintF("Simulator found unsupported instruction:\n 0x%08x: %s\n",
         instr, format);
  UNIMPLEMENTED();
}


// Checks if the current instruction should be executed based on its
// condition bits.
bool Simulator::ConditionallyExecute(Instr* instr) {
  switch (instr->ConditionField()) {
    case EQ: return z_flag_;
    case NE: return !z_flag_;
    case CS: return c_flag_;
    case CC: return !c_flag_;
    case MI: return n_flag_;
    case PL: return !n_flag_;
    case VS: return v_flag_;
    case VC: return !v_flag_;
    case HI: return c_flag_ && !z_flag_;
    case LS: return !c_flag_ || z_flag_;
    case GE: return n_flag_ == v_flag_;
    case LT: return n_flag_ != v_flag_;
    case GT: return !z_flag_ && (n_flag_ == v_flag_);
    case LE: return z_flag_ || (n_flag_ != v_flag_);
    case AL: return true;
    default: UNREACHABLE();
  }
  return false;
}


// Calculate and set the Negative and Zero flags.
void Simulator::SetNZFlags(int32_t val) {
  n_flag_ = (val < 0);
  z_flag_ = (val == 0);
}


// Set the Carry flag.
void Simulator::SetCFlag(bool val) {
  c_flag_ = val;
}


// Set the oVerflow flag.
void Simulator::SetVFlag(bool val) {
  v_flag_ = val;
}


// Calculate C flag value for additions.
bool Simulator::CarryFrom(int32_t left, int32_t right) {
  uint32_t uleft = static_cast<uint32_t>(left);
  uint32_t uright = static_cast<uint32_t>(right);
  uint32_t urest  = 0xffffffffU - uleft;

  return (uright > urest);
}


// Calculate C flag value for subtractions.
bool Simulator::BorrowFrom(int32_t left, int32_t right) {
  uint32_t uleft = static_cast<uint32_t>(left);
  uint32_t uright = static_cast<uint32_t>(right);

  return (uright > uleft);
}


// Calculate V flag value for additions and subtractions.
bool Simulator::OverflowFrom(int32_t alu_out,
                             int32_t left, int32_t right, bool addition) {
  bool overflow;
  if (addition) {
               // operands have the same sign
    overflow = ((left >= 0 && right >= 0) || (left < 0 && right < 0))
               // and operands and result have different sign
               && ((left < 0 && alu_out >= 0) || (left >= 0 && alu_out < 0));
  } else {
               // operands have different signs
    overflow = ((left < 0 && right >= 0) || (left >= 0 && right < 0))
               // and first operand and result have different signs
               && ((left < 0 && alu_out >= 0) || (left >= 0 && alu_out < 0));
  }
  return overflow;
}


// Support for VFP comparisons.
void Simulator::Compute_FPSCR_Flags(double val1, double val2) {
  if (isnan(val1) || isnan(val2)) {
    n_flag_FPSCR_ = false;
    z_flag_FPSCR_ = false;
    c_flag_FPSCR_ = true;
    v_flag_FPSCR_ = true;
  // All non-NaN cases.
  } else if (val1 == val2) {
    n_flag_FPSCR_ = false;
    z_flag_FPSCR_ = true;
    c_flag_FPSCR_ = true;
    v_flag_FPSCR_ = false;
  } else if (val1 < val2) {
    n_flag_FPSCR_ = true;
    z_flag_FPSCR_ = false;
    c_flag_FPSCR_ = false;
    v_flag_FPSCR_ = false;
  } else {
    // Case when (val1 > val2).
    n_flag_FPSCR_ = false;
    z_flag_FPSCR_ = false;
    c_flag_FPSCR_ = true;
    v_flag_FPSCR_ = false;
  }
}


void Simulator::Copy_FPSCR_to_APSR() {
  n_flag_ = n_flag_FPSCR_;
  z_flag_ = z_flag_FPSCR_;
  c_flag_ = c_flag_FPSCR_;
  v_flag_ = v_flag_FPSCR_;
}


// Addressing Mode 1 - Data-processing operands:
// Get the value based on the shifter_operand with register.
int32_t Simulator::GetShiftRm(Instr* instr, bool* carry_out) {
  Shift shift = instr->ShiftField();
  int shift_amount = instr->ShiftAmountField();
  int32_t result = get_register(instr->RmField());
  if (instr->Bit(4) == 0) {
    // by immediate
    if ((shift == ROR) && (shift_amount == 0)) {
      UNIMPLEMENTED();
      return result;
    } else if (((shift == LSR) || (shift == ASR)) && (shift_amount == 0)) {
      shift_amount = 32;
    }
    switch (shift) {
      case ASR: {
        if (shift_amount == 0) {
          if (result < 0) {
            result = 0xffffffff;
            *carry_out = true;
          } else {
            result = 0;
            *carry_out = false;
          }
        } else {
          result >>= (shift_amount - 1);
          *carry_out = (result & 1) == 1;
          result >>= 1;
        }
        break;
      }

      case LSL: {
        if (shift_amount == 0) {
          *carry_out = c_flag_;
        } else {
          result <<= (shift_amount - 1);
          *carry_out = (result < 0);
          result <<= 1;
        }
        break;
      }

      case LSR: {
        if (shift_amount == 0) {
          result = 0;
          *carry_out = c_flag_;
        } else {
          uint32_t uresult = static_cast<uint32_t>(result);
          uresult >>= (shift_amount - 1);
          *carry_out = (uresult & 1) == 1;
          uresult >>= 1;
          result = static_cast<int32_t>(uresult);
        }
        break;
      }

      case ROR: {
        UNIMPLEMENTED();
        break;
      }

      default: {
        UNREACHABLE();
        break;
      }
    }
  } else {
    // by register
    int rs = instr->RsField();
    shift_amount = get_register(rs) &0xff;
    switch (shift) {
      case ASR: {
        if (shift_amount == 0) {
          *carry_out = c_flag_;
        } else if (shift_amount < 32) {
          result >>= (shift_amount - 1);
          *carry_out = (result & 1) == 1;
          result >>= 1;
        } else {
          ASSERT(shift_amount >= 32);
          if (result < 0) {
            *carry_out = true;
            result = 0xffffffff;
          } else {
            *carry_out = false;
            result = 0;
          }
        }
        break;
      }

      case LSL: {
        if (shift_amount == 0) {
          *carry_out = c_flag_;
        } else if (shift_amount < 32) {
          result <<= (shift_amount - 1);
          *carry_out = (result < 0);
          result <<= 1;
        } else if (shift_amount == 32) {
          *carry_out = (result & 1) == 1;
          result = 0;
        } else {
          ASSERT(shift_amount > 32);
          *carry_out = false;
          result = 0;
        }
        break;
      }

      case LSR: {
        if (shift_amount == 0) {
          *carry_out = c_flag_;
        } else if (shift_amount < 32) {
          uint32_t uresult = static_cast<uint32_t>(result);
          uresult >>= (shift_amount - 1);
          *carry_out = (uresult & 1) == 1;
          uresult >>= 1;
          result = static_cast<int32_t>(uresult);
        } else if (shift_amount == 32) {
          *carry_out = (result < 0);
          result = 0;
        } else {
          *carry_out = false;
          result = 0;
        }
        break;
      }

      case ROR: {
        UNIMPLEMENTED();
        break;
      }

      default: {
        UNREACHABLE();
        break;
      }
    }
  }
  return result;
}


// Addressing Mode 1 - Data-processing operands:
// Get the value based on the shifter_operand with immediate.
int32_t Simulator::GetImm(Instr* instr, bool* carry_out) {
  int rotate = instr->RotateField() * 2;
  int immed8 = instr->Immed8Field();
  int imm = (immed8 >> rotate) | (immed8 << (32 - rotate));
  *carry_out = (rotate == 0) ? c_flag_ : (imm < 0);
  return imm;
}


static int count_bits(int bit_vector) {
  int count = 0;
  while (bit_vector != 0) {
    if ((bit_vector & 1) != 0) {
      count++;
    }
    bit_vector >>= 1;
  }
  return count;
}


// Addressing Mode 4 - Load and Store Multiple
void Simulator::HandleRList(Instr* instr, bool load) {
  int rn = instr->RnField();
  int32_t rn_val = get_register(rn);
  int rlist = instr->RlistField();
  int num_regs = count_bits(rlist);

  intptr_t start_address = 0;
  intptr_t end_address = 0;
  switch (instr->PUField()) {
    case 0: {
      // Print("da");
      UNIMPLEMENTED();
      break;
    }
    case 1: {
      // Print("ia");
      start_address = rn_val;
      end_address = rn_val + (num_regs * 4) - 4;
      rn_val = rn_val + (num_regs * 4);
      break;
    }
    case 2: {
      // Print("db");
      start_address = rn_val - (num_regs * 4);
      end_address = rn_val - 4;
      rn_val = start_address;
      break;
    }
    case 3: {
      // Print("ib");
      UNIMPLEMENTED();
      break;
    }
    default: {
      UNREACHABLE();
      break;
    }
  }
  if (instr->HasW()) {
    set_register(rn, rn_val);
  }
  intptr_t* address = reinterpret_cast<intptr_t*>(start_address);
  int reg = 0;
  while (rlist != 0) {
    if ((rlist & 1) != 0) {
      if (load) {
        set_register(reg, *address);
      } else {
        *address = get_register(reg);
      }
      address += 1;
    }
    reg++;
    rlist >>= 1;
  }
  ASSERT(end_address == ((intptr_t)address) - 4);
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
typedef double (*SimulatorRuntimeFPCall)(int32_t arg0,
                                         int32_t arg1,
                                         int32_t arg2,
                                         int32_t arg3);


// Software interrupt instructions are used by the simulator to call into the
// C-based V8 runtime.
void Simulator::SoftwareInterrupt(Instr* instr) {
  int swi = instr->SwiField();
  switch (swi) {
    case call_rt_redirected: {
      // Check if stack is aligned. Error if not aligned is reported below to
      // include information on the function called.
      bool stack_aligned =
          (get_register(sp)
           & (::v8::internal::FLAG_sim_stack_alignment - 1)) == 0;
      Redirection* redirection = Redirection::FromSwiInstruction(instr);
      int32_t arg0 = get_register(r0);
      int32_t arg1 = get_register(r1);
      int32_t arg2 = get_register(r2);
      int32_t arg3 = get_register(r3);
      // This is dodgy but it works because the C entry stubs are never moved.
      // See comment in codegen-arm.cc and bug 1242173.
      int32_t saved_lr = get_register(lr);
      if (redirection->fp_return()) {
        intptr_t external =
            reinterpret_cast<intptr_t>(redirection->external_function());
        SimulatorRuntimeFPCall target =
            reinterpret_cast<SimulatorRuntimeFPCall>(external);
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          double x, y;
          GetFpArgs(&x, &y);
          PrintF("Call to host function at %p with args %f, %f",
                 FUNCTION_ADDR(target), x, y);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        double result = target(arg0, arg1, arg2, arg3);
        SetFpResult(result);
      } else {
        intptr_t external =
            reinterpret_cast<int32_t>(redirection->external_function());
        SimulatorRuntimeCall target =
            reinterpret_cast<SimulatorRuntimeCall>(external);
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF(
              "Call to host function at %p with args %08x, %08x, %08x, %08x",
              FUNCTION_ADDR(target),
              arg0,
              arg1,
              arg2,
              arg3);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        int64_t result = target(arg0, arg1, arg2, arg3);
        int32_t lo_res = static_cast<int32_t>(result);
        int32_t hi_res = static_cast<int32_t>(result >> 32);
        if (::v8::internal::FLAG_trace_sim) {
          PrintF("Returned %08x\n", lo_res);
        }
        set_register(r0, lo_res);
        set_register(r1, hi_res);
      }
      set_register(lr, saved_lr);
      set_pc(get_register(lr));
      break;
    }
    case break_point: {
      Debugger dbg(this);
      dbg.Debug();
      break;
    }
    default: {
      UNREACHABLE();
      break;
    }
  }
}


// Handle execution based on instruction types.

// Instruction types 0 and 1 are both rolled into one function because they
// only differ in the handling of the shifter_operand.
void Simulator::DecodeType01(Instr* instr) {
  int type = instr->TypeField();
  if ((type == 0) && instr->IsSpecialType0()) {
    // multiply instruction or extra loads and stores
    if (instr->Bits(7, 4) == 9) {
      if (instr->Bit(24) == 0) {
        // Raw field decoding here. Multiply instructions have their Rd in
        // funny places.
        int rn = instr->RnField();
        int rm = instr->RmField();
        int rs = instr->RsField();
        int32_t rs_val = get_register(rs);
        int32_t rm_val = get_register(rm);
        if (instr->Bit(23) == 0) {
          if (instr->Bit(21) == 0) {
            // The MUL instruction description (A 4.1.33) refers to Rd as being
            // the destination for the operation, but it confusingly uses the
            // Rn field to encode it.
            // Format(instr, "mul'cond's 'rn, 'rm, 'rs");
            int rd = rn;  // Remap the rn field to the Rd register.
            int32_t alu_out = rm_val * rs_val;
            set_register(rd, alu_out);
            if (instr->HasS()) {
              SetNZFlags(alu_out);
            }
          } else {
            // The MLA instruction description (A 4.1.28) refers to the order
            // of registers as "Rd, Rm, Rs, Rn". But confusingly it uses the
            // Rn field to encode the Rd register and the Rd field to encode
            // the Rn register.
            Format(instr, "mla'cond's 'rn, 'rm, 'rs, 'rd");
          }
        } else {
          // The signed/long multiply instructions use the terms RdHi and RdLo
          // when referring to the target registers. They are mapped to the Rn
          // and Rd fields as follows:
          // RdLo == Rd
          // RdHi == Rn (This is confusingly stored in variable rd here
          //             because the mul instruction from above uses the
          //             Rn field to encode the Rd register. Good luck figuring
          //             this out without reading the ARM instruction manual
          //             at a very detailed level.)
          // Format(instr, "'um'al'cond's 'rd, 'rn, 'rs, 'rm");
          int rd_hi = rn;  // Remap the rn field to the RdHi register.
          int rd_lo = instr->RdField();
          int32_t hi_res = 0;
          int32_t lo_res = 0;
          if (instr->Bit(22) == 1) {
            int64_t left_op  = static_cast<int32_t>(rm_val);
            int64_t right_op = static_cast<int32_t>(rs_val);
            uint64_t result = left_op * right_op;
            hi_res = static_cast<int32_t>(result >> 32);
            lo_res = static_cast<int32_t>(result & 0xffffffff);
          } else {
            // unsigned multiply
            uint64_t left_op  = static_cast<uint32_t>(rm_val);
            uint64_t right_op = static_cast<uint32_t>(rs_val);
            uint64_t result = left_op * right_op;
            hi_res = static_cast<int32_t>(result >> 32);
            lo_res = static_cast<int32_t>(result & 0xffffffff);
          }
          set_register(rd_lo, lo_res);
          set_register(rd_hi, hi_res);
          if (instr->HasS()) {
            UNIMPLEMENTED();
          }
        }
      } else {
        UNIMPLEMENTED();  // Not used by V8.
      }
    } else {
      // extra load/store instructions
      int rd = instr->RdField();
      int rn = instr->RnField();
      int32_t rn_val = get_register(rn);
      int32_t addr = 0;
      if (instr->Bit(22) == 0) {
        int rm = instr->RmField();
        int32_t rm_val = get_register(rm);
        switch (instr->PUField()) {
          case 0: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn], -'rm");
            ASSERT(!instr->HasW());
            addr = rn_val;
            rn_val -= rm_val;
            set_register(rn, rn_val);
            break;
          }
          case 1: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn], +'rm");
            ASSERT(!instr->HasW());
            addr = rn_val;
            rn_val += rm_val;
            set_register(rn, rn_val);
            break;
          }
          case 2: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn, -'rm]'w");
            rn_val -= rm_val;
            addr = rn_val;
            if (instr->HasW()) {
              set_register(rn, rn_val);
            }
            break;
          }
          case 3: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn, +'rm]'w");
            rn_val += rm_val;
            addr = rn_val;
            if (instr->HasW()) {
              set_register(rn, rn_val);
            }
            break;
          }
          default: {
            // The PU field is a 2-bit field.
            UNREACHABLE();
            break;
          }
        }
      } else {
        int32_t imm_val = (instr->ImmedHField() << 4) | instr->ImmedLField();
        switch (instr->PUField()) {
          case 0: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn], #-'off8");
            ASSERT(!instr->HasW());
            addr = rn_val;
            rn_val -= imm_val;
            set_register(rn, rn_val);
            break;
          }
          case 1: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn], #+'off8");
            ASSERT(!instr->HasW());
            addr = rn_val;
            rn_val += imm_val;
            set_register(rn, rn_val);
            break;
          }
          case 2: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn, #-'off8]'w");
            rn_val -= imm_val;
            addr = rn_val;
            if (instr->HasW()) {
              set_register(rn, rn_val);
            }
            break;
          }
          case 3: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn, #+'off8]'w");
            rn_val += imm_val;
            addr = rn_val;
            if (instr->HasW()) {
              set_register(rn, rn_val);
            }
            break;
          }
          default: {
            // The PU field is a 2-bit field.
            UNREACHABLE();
            break;
          }
        }
      }
      if (instr->HasH()) {
        if (instr->HasSign()) {
          if (instr->HasL()) {
            int16_t val = ReadH(addr, instr);
            set_register(rd, val);
          } else {
            int16_t val = get_register(rd);
            WriteH(addr, val, instr);
          }
        } else {
          if (instr->HasL()) {
            uint16_t val = ReadHU(addr, instr);
            set_register(rd, val);
          } else {
            uint16_t val = get_register(rd);
            WriteH(addr, val, instr);
          }
        }
      } else {
        // signed byte loads
        ASSERT(instr->HasSign());
        ASSERT(instr->HasL());
        int8_t val = ReadB(addr);
        set_register(rd, val);
      }
      return;
    }
  } else if ((type == 0) && instr->IsMiscType0()) {
    if (instr->Bits(22, 21) == 1) {
      int rm = instr->RmField();
      switch (instr->Bits(7, 4)) {
        case BX:
          set_pc(get_register(rm));
          break;
        case BLX: {
          uint32_t old_pc = get_pc();
          set_pc(get_register(rm));
          set_register(lr, old_pc + Instr::kInstrSize);
          break;
        }
        case BKPT:
          v8::internal::OS::DebugBreak();
          break;
        default:
          UNIMPLEMENTED();
      }
    } else if (instr->Bits(22, 21) == 3) {
      int rm = instr->RmField();
      int rd = instr->RdField();
      switch (instr->Bits(7, 4)) {
        case CLZ: {
          uint32_t bits = get_register(rm);
          int leading_zeros = 0;
          if (bits == 0) {
            leading_zeros = 32;
          } else {
            while ((bits & 0x80000000u) == 0) {
              bits <<= 1;
              leading_zeros++;
            }
          }
          set_register(rd, leading_zeros);
          break;
        }
        default:
          UNIMPLEMENTED();
      }
    } else {
      PrintF("%08x\n", instr->InstructionBits());
      UNIMPLEMENTED();
    }
  } else {
    int rd = instr->RdField();
    int rn = instr->RnField();
    int32_t rn_val = get_register(rn);
    int32_t shifter_operand = 0;
    bool shifter_carry_out = 0;
    if (type == 0) {
      shifter_operand = GetShiftRm(instr, &shifter_carry_out);
    } else {
      ASSERT(instr->TypeField() == 1);
      shifter_operand = GetImm(instr, &shifter_carry_out);
    }
    int32_t alu_out;

    switch (instr->OpcodeField()) {
      case AND: {
        // Format(instr, "and'cond's 'rd, 'rn, 'shift_rm");
        // Format(instr, "and'cond's 'rd, 'rn, 'imm");
        alu_out = rn_val & shifter_operand;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(shifter_carry_out);
        }
        break;
      }

      case EOR: {
        // Format(instr, "eor'cond's 'rd, 'rn, 'shift_rm");
        // Format(instr, "eor'cond's 'rd, 'rn, 'imm");
        alu_out = rn_val ^ shifter_operand;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(shifter_carry_out);
        }
        break;
      }

      case SUB: {
        // Format(instr, "sub'cond's 'rd, 'rn, 'shift_rm");
        // Format(instr, "sub'cond's 'rd, 'rn, 'imm");
        alu_out = rn_val - shifter_operand;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(!BorrowFrom(rn_val, shifter_operand));
          SetVFlag(OverflowFrom(alu_out, rn_val, shifter_operand, false));
        }
        break;
      }

      case RSB: {
        // Format(instr, "rsb'cond's 'rd, 'rn, 'shift_rm");
        // Format(instr, "rsb'cond's 'rd, 'rn, 'imm");
        alu_out = shifter_operand - rn_val;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(!BorrowFrom(shifter_operand, rn_val));
          SetVFlag(OverflowFrom(alu_out, shifter_operand, rn_val, false));
        }
        break;
      }

      case ADD: {
        // Format(instr, "add'cond's 'rd, 'rn, 'shift_rm");
        // Format(instr, "add'cond's 'rd, 'rn, 'imm");
        alu_out = rn_val + shifter_operand;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(CarryFrom(rn_val, shifter_operand));
          SetVFlag(OverflowFrom(alu_out, rn_val, shifter_operand, true));
        }
        break;
      }

      case ADC: {
        Format(instr, "adc'cond's 'rd, 'rn, 'shift_rm");
        Format(instr, "adc'cond's 'rd, 'rn, 'imm");
        break;
      }

      case SBC: {
        Format(instr, "sbc'cond's 'rd, 'rn, 'shift_rm");
        Format(instr, "sbc'cond's 'rd, 'rn, 'imm");
        break;
      }

      case RSC: {
        Format(instr, "rsc'cond's 'rd, 'rn, 'shift_rm");
        Format(instr, "rsc'cond's 'rd, 'rn, 'imm");
        break;
      }

      case TST: {
        if (instr->HasS()) {
          // Format(instr, "tst'cond 'rn, 'shift_rm");
          // Format(instr, "tst'cond 'rn, 'imm");
          alu_out = rn_val & shifter_operand;
          SetNZFlags(alu_out);
          SetCFlag(shifter_carry_out);
        } else {
          UNIMPLEMENTED();
        }
        break;
      }

      case TEQ: {
        if (instr->HasS()) {
          // Format(instr, "teq'cond 'rn, 'shift_rm");
          // Format(instr, "teq'cond 'rn, 'imm");
          alu_out = rn_val ^ shifter_operand;
          SetNZFlags(alu_out);
          SetCFlag(shifter_carry_out);
        } else {
          // Other instructions matching this pattern are handled in the
          // miscellaneous instructions part above.
          UNREACHABLE();
        }
        break;
      }

      case CMP: {
        if (instr->HasS()) {
          // Format(instr, "cmp'cond 'rn, 'shift_rm");
          // Format(instr, "cmp'cond 'rn, 'imm");
          alu_out = rn_val - shifter_operand;
          SetNZFlags(alu_out);
          SetCFlag(!BorrowFrom(rn_val, shifter_operand));
          SetVFlag(OverflowFrom(alu_out, rn_val, shifter_operand, false));
        } else {
          UNIMPLEMENTED();
        }
        break;
      }

      case CMN: {
        if (instr->HasS()) {
          // Format(instr, "cmn'cond 'rn, 'shift_rm");
          // Format(instr, "cmn'cond 'rn, 'imm");
          alu_out = rn_val + shifter_operand;
          SetNZFlags(alu_out);
          SetCFlag(!CarryFrom(rn_val, shifter_operand));
          SetVFlag(OverflowFrom(alu_out, rn_val, shifter_operand, true));
        } else {
          // Other instructions matching this pattern are handled in the
          // miscellaneous instructions part above.
          UNREACHABLE();
        }
        break;
      }

      case ORR: {
        // Format(instr, "orr'cond's 'rd, 'rn, 'shift_rm");
        // Format(instr, "orr'cond's 'rd, 'rn, 'imm");
        alu_out = rn_val | shifter_operand;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(shifter_carry_out);
        }
        break;
      }

      case MOV: {
        // Format(instr, "mov'cond's 'rd, 'shift_rm");
        // Format(instr, "mov'cond's 'rd, 'imm");
        alu_out = shifter_operand;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(shifter_carry_out);
        }
        break;
      }

      case BIC: {
        // Format(instr, "bic'cond's 'rd, 'rn, 'shift_rm");
        // Format(instr, "bic'cond's 'rd, 'rn, 'imm");
        alu_out = rn_val & ~shifter_operand;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(shifter_carry_out);
        }
        break;
      }

      case MVN: {
        // Format(instr, "mvn'cond's 'rd, 'shift_rm");
        // Format(instr, "mvn'cond's 'rd, 'imm");
        alu_out = ~shifter_operand;
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(shifter_carry_out);
        }
        break;
      }

      default: {
        UNREACHABLE();
        break;
      }
    }
  }
}


void Simulator::DecodeType2(Instr* instr) {
  int rd = instr->RdField();
  int rn = instr->RnField();
  int32_t rn_val = get_register(rn);
  int32_t im_val = instr->Offset12Field();
  int32_t addr = 0;
  switch (instr->PUField()) {
    case 0: {
      // Format(instr, "'memop'cond'b 'rd, ['rn], #-'off12");
      ASSERT(!instr->HasW());
      addr = rn_val;
      rn_val -= im_val;
      set_register(rn, rn_val);
      break;
    }
    case 1: {
      // Format(instr, "'memop'cond'b 'rd, ['rn], #+'off12");
      ASSERT(!instr->HasW());
      addr = rn_val;
      rn_val += im_val;
      set_register(rn, rn_val);
      break;
    }
    case 2: {
      // Format(instr, "'memop'cond'b 'rd, ['rn, #-'off12]'w");
      rn_val -= im_val;
      addr = rn_val;
      if (instr->HasW()) {
        set_register(rn, rn_val);
      }
      break;
    }
    case 3: {
      // Format(instr, "'memop'cond'b 'rd, ['rn, #+'off12]'w");
      rn_val += im_val;
      addr = rn_val;
      if (instr->HasW()) {
        set_register(rn, rn_val);
      }
      break;
    }
    default: {
      UNREACHABLE();
      break;
    }
  }
  if (instr->HasB()) {
    if (instr->HasL()) {
      byte val = ReadBU(addr);
      set_register(rd, val);
    } else {
      byte val = get_register(rd);
      WriteB(addr, val);
    }
  } else {
    if (instr->HasL()) {
      set_register(rd, ReadW(addr, instr));
    } else {
      WriteW(addr, get_register(rd), instr);
    }
  }
}


void Simulator::DecodeType3(Instr* instr) {
  ASSERT(instr->Bits(6, 4) == 0x5 || instr->Bit(4) == 0);
  int rd = instr->RdField();
  int rn = instr->RnField();
  int32_t rn_val = get_register(rn);
  bool shifter_carry_out = 0;
  int32_t shifter_operand = GetShiftRm(instr, &shifter_carry_out);
  int32_t addr = 0;
  switch (instr->PUField()) {
    case 0: {
      ASSERT(!instr->HasW());
      Format(instr, "'memop'cond'b 'rd, ['rn], -'shift_rm");
      break;
    }
    case 1: {
      ASSERT(!instr->HasW());
      Format(instr, "'memop'cond'b 'rd, ['rn], +'shift_rm");
      break;
    }
    case 2: {
      // Format(instr, "'memop'cond'b 'rd, ['rn, -'shift_rm]'w");
      addr = rn_val - shifter_operand;
      if (instr->HasW()) {
        set_register(rn, addr);
      }
      break;
    }
    case 3: {
      // UBFX.
      if (instr->HasW() && (instr->Bits(6, 4) == 0x5)) {
        uint32_t widthminus1 = static_cast<uint32_t>(instr->Bits(20, 16));
        uint32_t lsbit = static_cast<uint32_t>(instr->ShiftAmountField());
        uint32_t msbit = widthminus1 + lsbit;
        if (msbit <= 31) {
          uint32_t rm_val =
              static_cast<uint32_t>(get_register(instr->RmField()));
          uint32_t extr_val = rm_val << (31 - msbit);
          extr_val = extr_val >> (31 - widthminus1);
          set_register(instr->RdField(), extr_val);
        } else {
          UNREACHABLE();
        }
        return;
      } else {
        // Format(instr, "'memop'cond'b 'rd, ['rn, +'shift_rm]'w");
        addr = rn_val + shifter_operand;
        if (instr->HasW()) {
          set_register(rn, addr);
        }
      }
      break;
    }
    default: {
      UNREACHABLE();
      break;
    }
  }
  if (instr->HasB()) {
    if (instr->HasL()) {
      uint8_t byte = ReadB(addr);
      set_register(rd, byte);
    } else {
      uint8_t byte = get_register(rd);
      WriteB(addr, byte);
    }
  } else {
    if (instr->HasL()) {
      set_register(rd, ReadW(addr, instr));
    } else {
      WriteW(addr, get_register(rd), instr);
    }
  }
}


void Simulator::DecodeType4(Instr* instr) {
  ASSERT(instr->Bit(22) == 0);  // only allowed to be set in privileged mode
  if (instr->HasL()) {
    // Format(instr, "ldm'cond'pu 'rn'w, 'rlist");
    HandleRList(instr, true);
  } else {
    // Format(instr, "stm'cond'pu 'rn'w, 'rlist");
    HandleRList(instr, false);
  }
}


void Simulator::DecodeType5(Instr* instr) {
  // Format(instr, "b'l'cond 'target");
  int off = (instr->SImmed24Field() << 2);
  intptr_t pc_address = get_pc();
  if (instr->HasLink()) {
    set_register(lr, pc_address + Instr::kInstrSize);
  }
  int pc_reg = get_register(pc);
  set_pc(pc_reg + off);
}


void Simulator::DecodeType6(Instr* instr) {
  DecodeType6CoprocessorIns(instr);
}


void Simulator::DecodeType7(Instr* instr) {
  if (instr->Bit(24) == 1) {
    SoftwareInterrupt(instr);
  } else {
    DecodeTypeVFP(instr);
  }
}


void Simulator::DecodeUnconditional(Instr* instr) {
  if (instr->Bits(7, 4) == 0x0B && instr->Bits(27, 25) == 0 && instr->HasL()) {
    // Load halfword instruction, either register or immediate offset.
    int rd = instr->RdField();
    int rn = instr->RnField();
    int32_t rn_val = get_register(rn);
    int32_t addr = 0;
    int32_t offset;
    if (instr->Bit(22) == 0) {
      // Register offset.
      int rm = instr->RmField();
      offset = get_register(rm);
    } else {
      // Immediate offset
      offset = instr->Bits(3, 0) + (instr->Bits(11, 8) << 4);
    }
    switch (instr->PUField()) {
      case 0: {
        // Post index, negative.
        ASSERT(!instr->HasW());
        addr = rn_val;
        rn_val -= offset;
        set_register(rn, rn_val);
        break;
      }
      case 1: {
        // Post index, positive.
        ASSERT(!instr->HasW());
        addr = rn_val;
        rn_val += offset;
        set_register(rn, rn_val);
        break;
      }
      case 2: {
        // Pre index or offset, negative.
        rn_val -= offset;
        addr = rn_val;
        if (instr->HasW()) {
          set_register(rn, rn_val);
        }
        break;
      }
      case 3: {
        // Pre index or offset, positive.
        rn_val += offset;
        addr = rn_val;
        if (instr->HasW()) {
          set_register(rn, rn_val);
        }
        break;
      }
      default: {
        // The PU field is a 2-bit field.
        UNREACHABLE();
        break;
      }
    }
    // Not sign extending, so load as unsigned.
    uint16_t halfword = ReadH(addr, instr);
    set_register(rd, halfword);
  } else {
    Debugger dbg(this);
    dbg.Stop(instr);
  }
}


// Depending on value of last_bit flag glue register code from vm and m values
// (where m is expected to be a single bit).
static int GlueRegCode(bool last_bit, int vm, int m) {
  return last_bit ? ((vm << 1) | m) : ((m << 4) | vm);
}


// void Simulator::DecodeTypeVFP(Instr* instr)
// The Following ARMv7 VFPv instructions are currently supported.
// vmov :Sn = Rt
// vmov :Rt = Sn
// vcvt: Dd = Sm
// vcvt: Sd = Dm
// Dd = vadd(Dn, Dm)
// Dd = vsub(Dn, Dm)
// Dd = vmul(Dn, Dm)
// Dd = vdiv(Dn, Dm)
// vcmp(Dd, Dm)
// VMRS
void Simulator::DecodeTypeVFP(Instr* instr) {
  ASSERT((instr->TypeField() == 7) && (instr->Bit(24) == 0x0) );
  ASSERT(instr->Bits(11, 9) == 0x5);

  int vm = instr->VmField();
  int vd = instr->VdField();
  int vn = instr->VnField();

  if (instr->Bit(4) == 0) {
    if (instr->Opc1Field() == 0x7) {
      // Other data processing instructions
      if ((instr->Opc2Field() == 0x7) && (instr->Opc3Field() == 0x3)) {
        DecodeVCVTBetweenDoubleAndSingle(instr);
      } else if ((instr->Opc2Field() == 0x8) && (instr->Opc3Field() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if (((instr->Opc2Field() >> 1) == 0x6) &&
                 (instr->Opc3Field() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if (((instr->Opc2Field() == 0x4) || (instr->Opc2Field() == 0x5)) &&
                 (instr->Opc3Field() & 0x1)) {
        DecodeVCMP(instr);
      } else {
        UNREACHABLE();  // Not used by V8.
      }
    } else if (instr->Opc1Field() == 0x3) {
      if (instr->SzField() != 0x1) {
        UNREACHABLE();  // Not used by V8.
      }

      if (instr->Opc3Field() & 0x1) {
        // vsub
        double dn_value = get_double_from_d_register(vn);
        double dm_value = get_double_from_d_register(vm);
        double dd_value = dn_value - dm_value;
        set_d_register_from_double(vd, dd_value);
      } else {
        // vadd
        double dn_value = get_double_from_d_register(vn);
        double dm_value = get_double_from_d_register(vm);
        double dd_value = dn_value + dm_value;
        set_d_register_from_double(vd, dd_value);
      }
    } else if ((instr->Opc1Field() == 0x2) && !(instr->Opc3Field() & 0x1)) {
      // vmul
      if (instr->SzField() != 0x1) {
        UNREACHABLE();  // Not used by V8.
      }

      double dn_value = get_double_from_d_register(vn);
      double dm_value = get_double_from_d_register(vm);
      double dd_value = dn_value * dm_value;
      set_d_register_from_double(vd, dd_value);
    } else if ((instr->Opc1Field() == 0x4) && !(instr->Opc3Field() & 0x1)) {
      // vdiv
      if (instr->SzField() != 0x1) {
        UNREACHABLE();  // Not used by V8.
      }

      double dn_value = get_double_from_d_register(vn);
      double dm_value = get_double_from_d_register(vm);
      double dd_value = dn_value / dm_value;
      set_d_register_from_double(vd, dd_value);
    } else {
      UNIMPLEMENTED();  // Not used by V8.
    }
  } else {
    if ((instr->VCField() == 0x0) &&
        (instr->VAField() == 0x0)) {
      DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(instr);
    } else if ((instr->VLField() == 0x1) &&
               (instr->VCField() == 0x0) &&
               (instr->VAField() == 0x7) &&
               (instr->Bits(19, 16) == 0x1)) {
      // vmrs
      if (instr->RtField() == 0xF)
        Copy_FPSCR_to_APSR();
      else
        UNIMPLEMENTED();  // Not used by V8.
    } else {
      UNIMPLEMENTED();  // Not used by V8.
    }
  }
}


void Simulator::DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(Instr* instr) {
  ASSERT((instr->Bit(4) == 1) && (instr->VCField() == 0x0) &&
         (instr->VAField() == 0x0));

  int t = instr->RtField();
  int n  = GlueRegCode(true, instr->VnField(), instr->NField());
  bool to_arm_register = (instr->VLField() == 0x1);

  if (to_arm_register) {
    int32_t int_value = get_sinteger_from_s_register(n);
    set_register(t, int_value);
  } else {
    int32_t rs_val = get_register(t);
    set_s_register_from_sinteger(n, rs_val);
  }
}


void Simulator::DecodeVCMP(Instr* instr) {
  ASSERT((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
  ASSERT(((instr->Opc2Field() == 0x4) || (instr->Opc2Field() == 0x5)) &&
         (instr->Opc3Field() & 0x1));

  // Comparison.
  bool dp_operation = (instr->SzField() == 1);

  if (instr->Bit(7) != 0) {
    // Raising exceptions for quiet NaNs are not supported.
    UNIMPLEMENTED();  // Not used by V8.
  }

  int d = GlueRegCode(!dp_operation, instr->VdField(), instr->DField());
  int m = GlueRegCode(!dp_operation, instr->VmField(), instr->MField());

  if (dp_operation) {
    double dd_value = get_double_from_d_register(d);
    double dm_value = get_double_from_d_register(m);

    Compute_FPSCR_Flags(dd_value, dm_value);
  } else {
    UNIMPLEMENTED();  // Not used by V8.
  }
}


void Simulator::DecodeVCVTBetweenDoubleAndSingle(Instr* instr) {
  ASSERT((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
  ASSERT((instr->Opc2Field() == 0x7) && (instr->Opc3Field() == 0x3));

  bool double_to_single = (instr->SzField() == 1);
  int dst = GlueRegCode(double_to_single, instr->VdField(), instr->DField());
  int src = GlueRegCode(!double_to_single, instr->VmField(), instr->MField());

  if (double_to_single) {
    double val = get_double_from_d_register(src);
    set_s_register_from_float(dst, static_cast<float>(val));
  } else {
    float val = get_float_from_s_register(src);
    set_d_register_from_double(dst, static_cast<double>(val));
  }
}


void Simulator::DecodeVCVTBetweenFloatingPointAndInteger(Instr* instr) {
  ASSERT((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
  ASSERT(((instr->Opc2Field() == 0x8) && (instr->Opc3Field() & 0x1)) ||
         (((instr->Opc2Field() >> 1) == 0x6) && (instr->Opc3Field() & 0x1)));

  // Conversion between floating-point and integer.
  int vd = instr->VdField();
  int d = instr->DField();
  int vm = instr->VmField();
  int m = instr->MField();

  bool to_integer = (instr->Bit(18) == 1);
  bool dp_operation = (instr->SzField() == 1);
  if (to_integer) {
    bool unsigned_integer = (instr->Bit(16) == 0);
    if (instr->Bit(7) != 1) {
      // Only rounding towards zero supported.
      UNIMPLEMENTED();  // Not used by V8.
    }

    int dst = GlueRegCode(true, vd, d);
    int src = GlueRegCode(!dp_operation, vm, m);

    if (dp_operation) {
      double val = get_double_from_d_register(src);

      int sint = unsigned_integer ? static_cast<uint32_t>(val) :
                                    static_cast<int32_t>(val);

      set_s_register_from_sinteger(dst, sint);
    } else {
      float val = get_float_from_s_register(src);

      int sint = unsigned_integer ? static_cast<uint32_t>(val) :
                                      static_cast<int32_t>(val);

      set_s_register_from_sinteger(dst, sint);
    }
  } else {
    bool unsigned_integer = (instr->Bit(7) == 0);

    int dst = GlueRegCode(!dp_operation, vd, d);
    int src = GlueRegCode(true, vm, m);

    int val = get_sinteger_from_s_register(src);

    if (dp_operation) {
      if (unsigned_integer) {
        set_d_register_from_double(dst,
                                   static_cast<double>((uint32_t)val));
      } else {
        set_d_register_from_double(dst, static_cast<double>(val));
      }
    } else {
      if (unsigned_integer) {
        set_s_register_from_float(dst,
                                  static_cast<float>((uint32_t)val));
      } else {
        set_s_register_from_float(dst, static_cast<float>(val));
      }
    }
  }
}


// void Simulator::DecodeType6CoprocessorIns(Instr* instr)
// Decode Type 6 coprocessor instructions.
// Dm = vmov(Rt, Rt2)
// <Rt, Rt2> = vmov(Dm)
// Ddst = MEM(Rbase + 4*offset).
// MEM(Rbase + 4*offset) = Dsrc.
void Simulator::DecodeType6CoprocessorIns(Instr* instr) {
  ASSERT((instr->TypeField() == 6));

  if (instr->CoprocessorField() == 0xA) {
    switch (instr->OpcodeField()) {
      case 0x8:
      case 0xC: {  // Load and store float to memory.
        int rn = instr->RnField();
        int vd = instr->VdField();
        int offset = instr->Immed8Field();
        if (!instr->HasU()) {
          offset = -offset;
        }

        int32_t address = get_register(rn) + 4 * offset;
        if (instr->HasL()) {
          // Load double from memory: vldr.
          set_s_register_from_sinteger(vd, ReadW(address, instr));
        } else {
          // Store double to memory: vstr.
          WriteW(address, get_sinteger_from_s_register(vd), instr);
        }
        break;
      }
      default:
        UNIMPLEMENTED();  // Not used by V8.
        break;
    }
  } else if (instr->CoprocessorField() == 0xB) {
    switch (instr->OpcodeField()) {
      case 0x2:
        // Load and store double to two GP registers
        if (instr->Bits(7, 4) != 0x1) {
          UNIMPLEMENTED();  // Not used by V8.
        } else {
          int rt = instr->RtField();
          int rn = instr->RnField();
          int vm = instr->VmField();
          if (instr->HasL()) {
            int32_t rt_int_value = get_sinteger_from_s_register(2*vm);
            int32_t rn_int_value = get_sinteger_from_s_register(2*vm+1);

            set_register(rt, rt_int_value);
            set_register(rn, rn_int_value);
          } else {
            int32_t rs_val = get_register(rt);
            int32_t rn_val = get_register(rn);

            set_s_register_from_sinteger(2*vm, rs_val);
            set_s_register_from_sinteger((2*vm+1), rn_val);
          }
        }
        break;
      case 0x8:
      case 0xC: {  // Load and store double to memory.
        int rn = instr->RnField();
        int vd = instr->VdField();
        int offset = instr->Immed8Field();
        if (!instr->HasU()) {
          offset = -offset;
        }
        int32_t address = get_register(rn) + 4 * offset;
        if (instr->HasL()) {
          // Load double from memory: vldr.
          set_s_register_from_sinteger(2*vd, ReadW(address, instr));
          set_s_register_from_sinteger(2*vd + 1, ReadW(address + 4, instr));
        } else {
          // Store double to memory: vstr.
          WriteW(address, get_sinteger_from_s_register(2*vd), instr);
          WriteW(address + 4, get_sinteger_from_s_register(2*vd + 1), instr);
        }
        break;
      }
      default:
        UNIMPLEMENTED();  // Not used by V8.
        break;
    }
  } else {
    UNIMPLEMENTED();  // Not used by V8.
  }
}


// Executes the current instruction.
void Simulator::InstructionDecode(Instr* instr) {
  pc_modified_ = false;
  if (::v8::internal::FLAG_trace_sim) {
    disasm::NameConverter converter;
    disasm::Disassembler dasm(converter);
    // use a reasonably large buffer
    v8::internal::EmbeddedVector<char, 256> buffer;
    dasm.InstructionDecode(buffer,
                           reinterpret_cast<byte*>(instr));
    PrintF("  0x%08x  %s\n", instr, buffer.start());
  }
  if (instr->ConditionField() == special_condition) {
    DecodeUnconditional(instr);
  } else if (ConditionallyExecute(instr)) {
    switch (instr->TypeField()) {
      case 0:
      case 1: {
        DecodeType01(instr);
        break;
      }
      case 2: {
        DecodeType2(instr);
        break;
      }
      case 3: {
        DecodeType3(instr);
        break;
      }
      case 4: {
        DecodeType4(instr);
        break;
      }
      case 5: {
        DecodeType5(instr);
        break;
      }
      case 6: {
        DecodeType6(instr);
        break;
      }
      case 7: {
        DecodeType7(instr);
        break;
      }
      default: {
        UNIMPLEMENTED();
        break;
      }
    }
  }
  if (!pc_modified_) {
    set_register(pc, reinterpret_cast<int32_t>(instr) + Instr::kInstrSize);
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
      Instr* instr = reinterpret_cast<Instr*>(program_counter);
      icount_++;
      InstructionDecode(instr);
      program_counter = get_pc();
    }
  } else {
    // FLAG_stop_sim_at is at the non-default value. Stop in the debugger when
    // we reach the particular instuction count.
    while (program_counter != end_sim_pc) {
      Instr* instr = reinterpret_cast<Instr*>(program_counter);
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


int32_t Simulator::Call(byte* entry, int argument_count, ...) {
  va_list parameters;
  va_start(parameters, argument_count);
  // Setup arguments

  // First four arguments passed in registers.
  ASSERT(argument_count >= 4);
  set_register(r0, va_arg(parameters, int32_t));
  set_register(r1, va_arg(parameters, int32_t));
  set_register(r2, va_arg(parameters, int32_t));
  set_register(r3, va_arg(parameters, int32_t));

  // Remaining arguments passed on stack.
  int original_stack = get_register(sp);
  // Compute position of stack on entry to generated code.
  int entry_stack = (original_stack - (argument_count - 4) * sizeof(int32_t));
  if (OS::ActivationFrameAlignment() != 0) {
    entry_stack &= -OS::ActivationFrameAlignment();
  }
  // Store remaining arguments on stack, from low to high memory.
  intptr_t* stack_argument = reinterpret_cast<intptr_t*>(entry_stack);
  for (int i = 4; i < argument_count; i++) {
    stack_argument[i - 4] = va_arg(parameters, int32_t);
  }
  va_end(parameters);
  set_register(sp, entry_stack);

  // Prepare to execute the code at entry
  set_register(pc, reinterpret_cast<int32_t>(entry));
  // Put down marker for end of simulation. The simulator will stop simulation
  // when the PC reaches this value. By saving the "end simulation" value into
  // the LR the simulation stops when returning to this call point.
  set_register(lr, end_sim_pc);

  // Remember the values of callee-saved registers.
  // The code below assumes that r9 is not used as sb (static base) in
  // simulator code and therefore is regarded as a callee-saved register.
  int32_t r4_val = get_register(r4);
  int32_t r5_val = get_register(r5);
  int32_t r6_val = get_register(r6);
  int32_t r7_val = get_register(r7);
  int32_t r8_val = get_register(r8);
  int32_t r9_val = get_register(r9);
  int32_t r10_val = get_register(r10);
  int32_t r11_val = get_register(r11);

  // Setup the callee-saved registers with a known value. To be able to check
  // that they are preserved properly across JS execution.
  int32_t callee_saved_value = icount_;
  set_register(r4, callee_saved_value);
  set_register(r5, callee_saved_value);
  set_register(r6, callee_saved_value);
  set_register(r7, callee_saved_value);
  set_register(r8, callee_saved_value);
  set_register(r9, callee_saved_value);
  set_register(r10, callee_saved_value);
  set_register(r11, callee_saved_value);

  // Start the simulation
  Execute();

  // Check that the callee-saved registers have been preserved.
  CHECK_EQ(callee_saved_value, get_register(r4));
  CHECK_EQ(callee_saved_value, get_register(r5));
  CHECK_EQ(callee_saved_value, get_register(r6));
  CHECK_EQ(callee_saved_value, get_register(r7));
  CHECK_EQ(callee_saved_value, get_register(r8));
  CHECK_EQ(callee_saved_value, get_register(r9));
  CHECK_EQ(callee_saved_value, get_register(r10));
  CHECK_EQ(callee_saved_value, get_register(r11));

  // Restore callee-saved registers with the original value.
  set_register(r4, r4_val);
  set_register(r5, r5_val);
  set_register(r6, r6_val);
  set_register(r7, r7_val);
  set_register(r8, r8_val);
  set_register(r9, r9_val);
  set_register(r10, r10_val);
  set_register(r11, r11_val);

  // Pop stack passed arguments.
  CHECK_EQ(entry_stack, get_register(sp));
  set_register(sp, original_stack);

  int32_t result = get_register(r0);
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


} }  // namespace assembler::arm

#endif  // __arm__
