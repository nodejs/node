// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <cmath>

#if V8_TARGET_ARCH_MIPS64

#include "src/assembler.h"
#include "src/base/bits.h"
#include "src/codegen.h"
#include "src/disasm.h"
#include "src/mips64/constants-mips64.h"
#include "src/mips64/simulator-mips64.h"
#include "src/ostreams.h"
#include "src/runtime/runtime-utils.h"

// Only build the simulator if not compiling for real MIPS hardware.
#if defined(USE_SIMULATOR)

namespace v8 {
namespace internal {

// Util functions.
inline bool HaveSameSign(int64_t a, int64_t b) { return ((a ^ b) >= 0); }

uint32_t get_fcsr_condition_bit(uint32_t cc) {
  if (cc == 0) {
    return 23;
  } else {
    return 24 + cc;
  }
}


static int64_t MultiplyHighSigned(int64_t u, int64_t v) {
  uint64_t u0, v0, w0;
  int64_t u1, v1, w1, w2, t;

  u0 = u & 0xffffffffL;
  u1 = u >> 32;
  v0 = v & 0xffffffffL;
  v1 = v >> 32;

  w0 = u0 * v0;
  t = u1 * v0 + (w0 >> 32);
  w1 = t & 0xffffffffL;
  w2 = t >> 32;
  w1 = u0 * v1 + w1;

  return u1 * v1 + w2 + (w1 >> 32);
}


// This macro provides a platform independent use of sscanf. The reason for
// SScanF not being implemented in a platform independent was through
// ::v8::internal::OS in the same way as SNPrintF is that the Windows C Run-Time
// Library does not provide vsscanf.
#define SScanF sscanf  // NOLINT

// The MipsDebugger class is used by the simulator while debugging simulated
// code.
class MipsDebugger {
 public:
  explicit MipsDebugger(Simulator* sim) : sim_(sim) { }

  void Stop(Instruction* instr);
  void Debug();
  // Print all registers with a nice formatting.
  void PrintAllRegs();
  void PrintAllRegsIncludingFPU();

 private:
  // We set the breakpoint code to 0xfffff to easily recognize it.
  static const Instr kBreakpointInstr = SPECIAL | BREAK | 0xfffff << 6;
  static const Instr kNopInstr =  0x0;

  Simulator* sim_;

  int64_t GetRegisterValue(int regnum);
  int64_t GetFPURegisterValue(int regnum);
  float GetFPURegisterValueFloat(int regnum);
  double GetFPURegisterValueDouble(int regnum);
  bool GetValue(const char* desc, int64_t* value);

  // Set or delete a breakpoint. Returns true if successful.
  bool SetBreakpoint(Instruction* breakpc);
  bool DeleteBreakpoint(Instruction* breakpc);

  // Undo and redo all breakpoints. This is needed to bracket disassembly and
  // execution to skip past breakpoints when run from the debugger.
  void UndoBreakpoints();
  void RedoBreakpoints();
};

inline void UNSUPPORTED() { printf("Sim: Unsupported instruction.\n"); }

void MipsDebugger::Stop(Instruction* instr) {
  // Get the stop code.
  uint32_t code = instr->Bits(25, 6);
  PrintF("Simulator hit (%u)\n", code);
  // TODO(yuyin): 2 -> 3?
  sim_->set_pc(sim_->get_pc() + 3 * Instruction::kInstrSize);
  Debug();
}

int64_t MipsDebugger::GetRegisterValue(int regnum) {
  if (regnum == kNumSimuRegisters) {
    return sim_->get_pc();
  } else {
    return sim_->get_register(regnum);
  }
}


int64_t MipsDebugger::GetFPURegisterValue(int regnum) {
  if (regnum == kNumFPURegisters) {
    return sim_->get_pc();
  } else {
    return sim_->get_fpu_register(regnum);
  }
}


float MipsDebugger::GetFPURegisterValueFloat(int regnum) {
  if (regnum == kNumFPURegisters) {
    return sim_->get_pc();
  } else {
    return sim_->get_fpu_register_float(regnum);
  }
}


double MipsDebugger::GetFPURegisterValueDouble(int regnum) {
  if (regnum == kNumFPURegisters) {
    return sim_->get_pc();
  } else {
    return sim_->get_fpu_register_double(regnum);
  }
}


bool MipsDebugger::GetValue(const char* desc, int64_t* value) {
  int regnum = Registers::Number(desc);
  int fpuregnum = FPURegisters::Number(desc);

  if (regnum != kInvalidRegister) {
    *value = GetRegisterValue(regnum);
    return true;
  } else if (fpuregnum != kInvalidFPURegister) {
    *value = GetFPURegisterValue(fpuregnum);
    return true;
  } else if (strncmp(desc, "0x", 2) == 0) {
    return SScanF(desc + 2, "%" SCNx64,
                  reinterpret_cast<uint64_t*>(value)) == 1;
  } else {
    return SScanF(desc, "%" SCNu64, reinterpret_cast<uint64_t*>(value)) == 1;
  }
  return false;
}


bool MipsDebugger::SetBreakpoint(Instruction* breakpc) {
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


bool MipsDebugger::DeleteBreakpoint(Instruction* breakpc) {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }

  sim_->break_pc_ = NULL;
  sim_->break_instr_ = 0;
  return true;
}


void MipsDebugger::UndoBreakpoints() {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }
}


void MipsDebugger::RedoBreakpoints() {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(kBreakpointInstr);
  }
}


void MipsDebugger::PrintAllRegs() {
#define REG_INFO(n) Registers::Name(n), GetRegisterValue(n), GetRegisterValue(n)

  PrintF("\n");
  // at, v0, a0.
  PrintF("%3s: 0x%016" PRIx64 " %14" PRId64 "\t%3s: 0x%016" PRIx64 " %14" PRId64
         "\t%3s: 0x%016" PRIx64 " %14" PRId64 "\n",
         REG_INFO(1), REG_INFO(2), REG_INFO(4));
  // v1, a1.
  PrintF("%34s\t%3s: 0x%016" PRIx64 "  %14" PRId64 " \t%3s: 0x%016" PRIx64
         "  %14" PRId64 " \n",
         "", REG_INFO(3), REG_INFO(5));
  // a2.
  PrintF("%34s\t%34s\t%3s: 0x%016" PRIx64 "  %14" PRId64 " \n", "", "",
         REG_INFO(6));
  // a3.
  PrintF("%34s\t%34s\t%3s: 0x%016" PRIx64 "  %14" PRId64 " \n", "", "",
         REG_INFO(7));
  PrintF("\n");
  // a4-t3, s0-s7
  for (int i = 0; i < 8; i++) {
    PrintF("%3s: 0x%016" PRIx64 "  %14" PRId64 " \t%3s: 0x%016" PRIx64
           "  %14" PRId64 " \n",
           REG_INFO(8 + i), REG_INFO(16 + i));
  }
  PrintF("\n");
  // t8, k0, LO.
  PrintF("%3s: 0x%016" PRIx64 "  %14" PRId64 " \t%3s: 0x%016" PRIx64
         "  %14" PRId64 " \t%3s: 0x%016" PRIx64 "  %14" PRId64 " \n",
         REG_INFO(24), REG_INFO(26), REG_INFO(32));
  // t9, k1, HI.
  PrintF("%3s: 0x%016" PRIx64 "  %14" PRId64 " \t%3s: 0x%016" PRIx64
         "  %14" PRId64 " \t%3s: 0x%016" PRIx64 "  %14" PRId64 " \n",
         REG_INFO(25), REG_INFO(27), REG_INFO(33));
  // sp, fp, gp.
  PrintF("%3s: 0x%016" PRIx64 "  %14" PRId64 " \t%3s: 0x%016" PRIx64
         "  %14" PRId64 " \t%3s: 0x%016" PRIx64 "  %14" PRId64 " \n",
         REG_INFO(29), REG_INFO(30), REG_INFO(28));
  // pc.
  PrintF("%3s: 0x%016" PRIx64 "  %14" PRId64 " \t%3s: 0x%016" PRIx64
         "  %14" PRId64 " \n",
         REG_INFO(31), REG_INFO(34));

#undef REG_INFO
#undef FPU_REG_INFO
}


void MipsDebugger::PrintAllRegsIncludingFPU() {
#define FPU_REG_INFO(n) FPURegisters::Name(n), \
        GetFPURegisterValue(n), \
        GetFPURegisterValueDouble(n)

  PrintAllRegs();

  PrintF("\n\n");
  // f0, f1, f2, ... f31.
  // TODO(plind): consider printing 2 columns for space efficiency.
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(0));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(1));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(2));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(3));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(4));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(5));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(6));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(7));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(8));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(9));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(10));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(11));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(12));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(13));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(14));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(15));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(16));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(17));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(18));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(19));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(20));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(21));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(22));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(23));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(24));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(25));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(26));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(27));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(28));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(29));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(30));
  PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n", FPU_REG_INFO(31));

#undef REG_INFO
#undef FPU_REG_INFO
}


void MipsDebugger::Debug() {
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

  // Make sure to have a proper terminating character if reaching the limit.
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
      // Use a reasonably large buffer.
      v8::internal::EmbeddedVector<char, 256> buffer;
      dasm.InstructionDecode(buffer,
                             reinterpret_cast<byte*>(sim_->get_pc()));
      PrintF("  0x%016" PRIx64 "   %s\n", sim_->get_pc(), buffer.start());
      last_pc = sim_->get_pc();
    }
    char* line = ReadLine("sim> ");
    if (line == NULL) {
      break;
    } else {
      char* last_input = sim_->last_debugger_input();
      if (strcmp(line, "\n") == 0 && last_input != NULL) {
        line = last_input;
      } else {
        // Ownership is transferred to sim_;
        sim_->set_last_debugger_input(line);
      }
      // Use sscanf to parse the individual parts of the command line. At the
      // moment no command expects more than two parameters.
      int argc = SScanF(line,
                        "%" XSTR(COMMAND_SIZE) "s "
                        "%" XSTR(ARG_SIZE) "s "
                        "%" XSTR(ARG_SIZE) "s",
                        cmd, arg1, arg2);
      if ((strcmp(cmd, "si") == 0) || (strcmp(cmd, "stepi") == 0)) {
        Instruction* instr = reinterpret_cast<Instruction*>(sim_->get_pc());
        if (!(instr->IsTrap()) ||
            instr->InstructionBits() == rtCallRedirInstr) {
          sim_->InstructionDecode(
              reinterpret_cast<Instruction*>(sim_->get_pc()));
        } else {
          // Allow si to jump over generated breakpoints.
          PrintF("/!\\ Jumping over generated breakpoint.\n");
          sim_->set_pc(sim_->get_pc() + Instruction::kInstrSize);
        }
      } else if ((strcmp(cmd, "c") == 0) || (strcmp(cmd, "cont") == 0)) {
        // Execute the one instruction we broke at with breakpoints disabled.
        sim_->InstructionDecode(reinterpret_cast<Instruction*>(sim_->get_pc()));
        // Leave the debugger shell.
        done = true;
      } else if ((strcmp(cmd, "p") == 0) || (strcmp(cmd, "print") == 0)) {
        if (argc == 2) {
          int64_t value;
          double dvalue;
          if (strcmp(arg1, "all") == 0) {
            PrintAllRegs();
          } else if (strcmp(arg1, "allf") == 0) {
            PrintAllRegsIncludingFPU();
          } else {
            int regnum = Registers::Number(arg1);
            int fpuregnum = FPURegisters::Number(arg1);

            if (regnum != kInvalidRegister) {
              value = GetRegisterValue(regnum);
              PrintF("%s: 0x%08" PRIx64 "  %" PRId64 "  \n", arg1, value,
                     value);
            } else if (fpuregnum != kInvalidFPURegister) {
              value = GetFPURegisterValue(fpuregnum);
              dvalue = GetFPURegisterValueDouble(fpuregnum);
              PrintF("%3s: 0x%016" PRIx64 "  %16.4e\n",
                     FPURegisters::Name(fpuregnum), value, dvalue);
            } else {
              PrintF("%s unrecognized\n", arg1);
            }
          }
        } else {
          if (argc == 3) {
            if (strcmp(arg2, "single") == 0) {
              int64_t value;
              float fvalue;
              int fpuregnum = FPURegisters::Number(arg1);

              if (fpuregnum != kInvalidFPURegister) {
                value = GetFPURegisterValue(fpuregnum);
                value &= 0xffffffffUL;
                fvalue = GetFPURegisterValueFloat(fpuregnum);
                PrintF("%s: 0x%08" PRIx64 "  %11.4e\n", arg1, value, fvalue);
              } else {
                PrintF("%s unrecognized\n", arg1);
              }
            } else {
              PrintF("print <fpu register> single\n");
            }
          } else {
            PrintF("print <register> or print <fpu register> single\n");
          }
        }
      } else if ((strcmp(cmd, "po") == 0)
                 || (strcmp(cmd, "printobject") == 0)) {
        if (argc == 2) {
          int64_t value;
          OFStream os(stdout);
          if (GetValue(arg1, &value)) {
            Object* obj = reinterpret_cast<Object*>(value);
            os << arg1 << ": \n";
#ifdef DEBUG
            obj->Print(os);
            os << "\n";
#else
            os << Brief(obj) << "\n";
#endif
          } else {
            os << arg1 << " unrecognized\n";
          }
        } else {
          PrintF("printobject <value>\n");
        }
      } else if (strcmp(cmd, "stack") == 0 || strcmp(cmd, "mem") == 0) {
        int64_t* cur = NULL;
        int64_t* end = NULL;
        int next_arg = 1;

        if (strcmp(cmd, "stack") == 0) {
          cur = reinterpret_cast<int64_t*>(sim_->get_register(Simulator::sp));
        } else {  // Command "mem".
          int64_t value;
          if (!GetValue(arg1, &value)) {
            PrintF("%s unrecognized\n", arg1);
            continue;
          }
          cur = reinterpret_cast<int64_t*>(value);
          next_arg++;
        }

        int64_t words;
        if (argc == next_arg) {
          words = 10;
        } else {
          if (!GetValue(argv[next_arg], &words)) {
            words = 10;
          }
        }
        end = cur + words;

        while (cur < end) {
          PrintF("  0x%012" PRIxPTR " :  0x%016" PRIx64 "  %14" PRId64 " ",
                 reinterpret_cast<intptr_t>(cur), *cur, *cur);
          HeapObject* obj = reinterpret_cast<HeapObject*>(*cur);
          int64_t value = *cur;
          Heap* current_heap = sim_->isolate_->heap();
          if (((value & 1) == 0) ||
              current_heap->ContainsSlow(obj->address())) {
            PrintF(" (");
            if ((value & 1) == 0) {
              PrintF("smi %d", static_cast<int>(value >> 32));
            } else {
              obj->ShortPrint();
            }
            PrintF(")");
          }
          PrintF("\n");
          cur++;
        }

      } else if ((strcmp(cmd, "disasm") == 0) ||
                 (strcmp(cmd, "dpc") == 0) ||
                 (strcmp(cmd, "di") == 0)) {
        disasm::NameConverter converter;
        disasm::Disassembler dasm(converter);
        // Use a reasonably large buffer.
        v8::internal::EmbeddedVector<char, 256> buffer;

        byte* cur = NULL;
        byte* end = NULL;

        if (argc == 1) {
          cur = reinterpret_cast<byte*>(sim_->get_pc());
          end = cur + (10 * Instruction::kInstrSize);
        } else if (argc == 2) {
          int regnum = Registers::Number(arg1);
          if (regnum != kInvalidRegister || strncmp(arg1, "0x", 2) == 0) {
            // The argument is an address or a register name.
            int64_t value;
            if (GetValue(arg1, &value)) {
              cur = reinterpret_cast<byte*>(value);
              // Disassemble 10 instructions at <arg1>.
              end = cur + (10 * Instruction::kInstrSize);
            }
          } else {
            // The argument is the number of instructions.
            int64_t value;
            if (GetValue(arg1, &value)) {
              cur = reinterpret_cast<byte*>(sim_->get_pc());
              // Disassemble <arg1> instructions.
              end = cur + (value * Instruction::kInstrSize);
            }
          }
        } else {
          int64_t value1;
          int64_t value2;
          if (GetValue(arg1, &value1) && GetValue(arg2, &value2)) {
            cur = reinterpret_cast<byte*>(value1);
            end = cur + (value2 * Instruction::kInstrSize);
          }
        }

        while (cur < end) {
          dasm.InstructionDecode(buffer, cur);
          PrintF("  0x%08" PRIxPTR "   %s\n", reinterpret_cast<intptr_t>(cur),
                 buffer.start());
          cur += Instruction::kInstrSize;
        }
      } else if (strcmp(cmd, "gdb") == 0) {
        PrintF("relinquishing control to gdb\n");
        v8::base::OS::DebugBreak();
        PrintF("regaining control from gdb\n");
      } else if (strcmp(cmd, "break") == 0) {
        if (argc == 2) {
          int64_t value;
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
      } else if (strcmp(cmd, "stop") == 0) {
        int64_t value;
        intptr_t stop_pc = sim_->get_pc() -
            2 * Instruction::kInstrSize;
        Instruction* stop_instr = reinterpret_cast<Instruction*>(stop_pc);
        Instruction* msg_address =
          reinterpret_cast<Instruction*>(stop_pc +
              Instruction::kInstrSize);
        if ((argc == 2) && (strcmp(arg1, "unstop") == 0)) {
          // Remove the current stop.
          if (sim_->IsStopInstruction(stop_instr)) {
            stop_instr->SetInstructionBits(kNopInstr);
            msg_address->SetInstructionBits(kNopInstr);
          } else {
            PrintF("Not at debugger stop.\n");
          }
        } else if (argc == 3) {
          // Print information about all/the specified breakpoint(s).
          if (strcmp(arg1, "info") == 0) {
            if (strcmp(arg2, "all") == 0) {
              PrintF("Stop information:\n");
              for (uint32_t i = kMaxWatchpointCode + 1;
                   i <= kMaxStopCode;
                   i++) {
                sim_->PrintStopInfo(i);
              }
            } else if (GetValue(arg2, &value)) {
              sim_->PrintStopInfo(value);
            } else {
              PrintF("Unrecognized argument.\n");
            }
          } else if (strcmp(arg1, "enable") == 0) {
            // Enable all/the specified breakpoint(s).
            if (strcmp(arg2, "all") == 0) {
              for (uint32_t i = kMaxWatchpointCode + 1;
                   i <= kMaxStopCode;
                   i++) {
                sim_->EnableStop(i);
              }
            } else if (GetValue(arg2, &value)) {
              sim_->EnableStop(value);
            } else {
              PrintF("Unrecognized argument.\n");
            }
          } else if (strcmp(arg1, "disable") == 0) {
            // Disable all/the specified breakpoint(s).
            if (strcmp(arg2, "all") == 0) {
              for (uint32_t i = kMaxWatchpointCode + 1;
                   i <= kMaxStopCode;
                   i++) {
                sim_->DisableStop(i);
              }
            } else if (GetValue(arg2, &value)) {
              sim_->DisableStop(value);
            } else {
              PrintF("Unrecognized argument.\n");
            }
          }
        } else {
          PrintF("Wrong usage. Use help command for more information.\n");
        }
      } else if ((strcmp(cmd, "stat") == 0) || (strcmp(cmd, "st") == 0)) {
        // Print registers and disassemble.
        PrintAllRegs();
        PrintF("\n");

        disasm::NameConverter converter;
        disasm::Disassembler dasm(converter);
        // Use a reasonably large buffer.
        v8::internal::EmbeddedVector<char, 256> buffer;

        byte* cur = NULL;
        byte* end = NULL;

        if (argc == 1) {
          cur = reinterpret_cast<byte*>(sim_->get_pc());
          end = cur + (10 * Instruction::kInstrSize);
        } else if (argc == 2) {
          int64_t value;
          if (GetValue(arg1, &value)) {
            cur = reinterpret_cast<byte*>(value);
            // no length parameter passed, assume 10 instructions
            end = cur + (10 * Instruction::kInstrSize);
          }
        } else {
          int64_t value1;
          int64_t value2;
          if (GetValue(arg1, &value1) && GetValue(arg2, &value2)) {
            cur = reinterpret_cast<byte*>(value1);
            end = cur + (value2 * Instruction::kInstrSize);
          }
        }

        while (cur < end) {
          dasm.InstructionDecode(buffer, cur);
          PrintF("  0x%08" PRIxPTR "   %s\n", reinterpret_cast<intptr_t>(cur),
                 buffer.start());
          cur += Instruction::kInstrSize;
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
        PrintF("stack [<words>]\n");
        PrintF("  dump stack content, default dump 10 words)\n");
        PrintF("mem <address> [<words>]\n");
        PrintF("  dump memory content, default dump 10 words)\n");
        PrintF("flags\n");
        PrintF("  print flags\n");
        PrintF("disasm [<instructions>]\n");
        PrintF("disasm [<address/register>]\n");
        PrintF("disasm [[<address/register>] <instructions>]\n");
        PrintF("  disassemble code, default is 10 instructions\n");
        PrintF("  from pc (alias 'di')\n");
        PrintF("gdb\n");
        PrintF("  enter gdb\n");
        PrintF("break <address>\n");
        PrintF("  set a break point on the address\n");
        PrintF("del\n");
        PrintF("  delete the breakpoint\n");
        PrintF("stop feature:\n");
        PrintF("  Description:\n");
        PrintF("    Stops are debug instructions inserted by\n");
        PrintF("    the Assembler::stop() function.\n");
        PrintF("    When hitting a stop, the Simulator will\n");
        PrintF("    stop and and give control to the Debugger.\n");
        PrintF("    All stop codes are watched:\n");
        PrintF("    - They can be enabled / disabled: the Simulator\n");
        PrintF("       will / won't stop when hitting them.\n");
        PrintF("    - The Simulator keeps track of how many times they \n");
        PrintF("      are met. (See the info command.) Going over a\n");
        PrintF("      disabled stop still increases its counter. \n");
        PrintF("  Commands:\n");
        PrintF("    stop info all/<code> : print infos about number <code>\n");
        PrintF("      or all stop(s).\n");
        PrintF("    stop enable/disable all/<code> : enables / disables\n");
        PrintF("      all or number <code> stop(s)\n");
        PrintF("    stop unstop\n");
        PrintF("      ignore the stop instruction at the current location\n");
        PrintF("      from now on\n");
      } else {
        PrintF("Unknown command: %s\n", cmd);
      }
    }
  }

  // Add all the breakpoints back to stop execution and enter the debugger
  // shell when hit.
  RedoBreakpoints();

#undef COMMAND_SIZE
#undef ARG_SIZE

#undef STR
#undef XSTR
}


static bool ICacheMatch(void* one, void* two) {
  DCHECK((reinterpret_cast<intptr_t>(one) & CachePage::kPageMask) == 0);
  DCHECK((reinterpret_cast<intptr_t>(two) & CachePage::kPageMask) == 0);
  return one == two;
}


static uint32_t ICacheHash(void* key) {
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(key)) >> 2;
}


static bool AllOnOnePage(uintptr_t start, size_t size) {
  intptr_t start_page = (start & ~CachePage::kPageMask);
  intptr_t end_page = ((start + size) & ~CachePage::kPageMask);
  return start_page == end_page;
}


void Simulator::set_last_debugger_input(char* input) {
  DeleteArray(last_debugger_input_);
  last_debugger_input_ = input;
}

void Simulator::FlushICache(base::CustomMatcherHashMap* i_cache,
                            void* start_addr, size_t size) {
  int64_t start = reinterpret_cast<int64_t>(start_addr);
  int64_t intra_line = (start & CachePage::kLineMask);
  start -= intra_line;
  size += intra_line;
  size = ((size - 1) | CachePage::kLineMask) + 1;
  int offset = (start & CachePage::kPageMask);
  while (!AllOnOnePage(start, size - 1)) {
    int bytes_to_flush = CachePage::kPageSize - offset;
    FlushOnePage(i_cache, start, bytes_to_flush);
    start += bytes_to_flush;
    size -= bytes_to_flush;
    DCHECK_EQ((int64_t)0, start & CachePage::kPageMask);
    offset = 0;
  }
  if (size != 0) {
    FlushOnePage(i_cache, start, size);
  }
}

CachePage* Simulator::GetCachePage(base::CustomMatcherHashMap* i_cache,
                                   void* page) {
  base::HashMap::Entry* entry = i_cache->LookupOrInsert(page, ICacheHash(page));
  if (entry->value == NULL) {
    CachePage* new_page = new CachePage();
    entry->value = new_page;
  }
  return reinterpret_cast<CachePage*>(entry->value);
}


// Flush from start up to and not including start + size.
void Simulator::FlushOnePage(base::CustomMatcherHashMap* i_cache,
                             intptr_t start, size_t size) {
  DCHECK(size <= CachePage::kPageSize);
  DCHECK(AllOnOnePage(start, size - 1));
  DCHECK((start & CachePage::kLineMask) == 0);
  DCHECK((size & CachePage::kLineMask) == 0);
  void* page = reinterpret_cast<void*>(start & (~CachePage::kPageMask));
  int offset = (start & CachePage::kPageMask);
  CachePage* cache_page = GetCachePage(i_cache, page);
  char* valid_bytemap = cache_page->ValidityByte(offset);
  memset(valid_bytemap, CachePage::LINE_INVALID, size >> CachePage::kLineShift);
}

void Simulator::CheckICache(base::CustomMatcherHashMap* i_cache,
                            Instruction* instr) {
  int64_t address = reinterpret_cast<int64_t>(instr);
  void* page = reinterpret_cast<void*>(address & (~CachePage::kPageMask));
  void* line = reinterpret_cast<void*>(address & (~CachePage::kLineMask));
  int offset = (address & CachePage::kPageMask);
  CachePage* cache_page = GetCachePage(i_cache, page);
  char* cache_valid_byte = cache_page->ValidityByte(offset);
  bool cache_hit = (*cache_valid_byte == CachePage::LINE_VALID);
  char* cached_line = cache_page->CachedData(offset & ~CachePage::kLineMask);
  if (cache_hit) {
    // Check that the data in memory matches the contents of the I-cache.
    CHECK_EQ(0, memcmp(reinterpret_cast<void*>(instr),
                       cache_page->CachedData(offset),
                       Instruction::kInstrSize));
  } else {
    // Cache miss.  Load memory into the cache.
    memcpy(cached_line, line, CachePage::kLineLength);
    *cache_valid_byte = CachePage::LINE_VALID;
  }
}


void Simulator::Initialize(Isolate* isolate) {
  if (isolate->simulator_initialized()) return;
  isolate->set_simulator_initialized(true);
  ::v8::internal::ExternalReference::set_redirector(isolate,
                                                    &RedirectExternalReference);
}


Simulator::Simulator(Isolate* isolate) : isolate_(isolate) {
  i_cache_ = isolate_->simulator_i_cache();
  if (i_cache_ == NULL) {
    i_cache_ = new base::CustomMatcherHashMap(&ICacheMatch);
    isolate_->set_simulator_i_cache(i_cache_);
  }
  Initialize(isolate);
  // Set up simulator support first. Some of this information is needed to
  // setup the architecture state.
  stack_size_ = FLAG_sim_stack_size * KB;
  stack_ = reinterpret_cast<char*>(malloc(stack_size_));
  pc_modified_ = false;
  icount_ = 0;
  break_count_ = 0;
  break_pc_ = NULL;
  break_instr_ = 0;

  // Set up architecture state.
  // All registers are initialized to zero to start with.
  for (int i = 0; i < kNumSimuRegisters; i++) {
    registers_[i] = 0;
  }
  for (int i = 0; i < kNumFPURegisters; i++) {
    FPUregisters_[i] = 0;
  }

  if (kArchVariant == kMips64r6) {
    FCSR_ = kFCSRNaN2008FlagMask;
  } else {
    FCSR_ = 0;
  }

  // The sp is initialized to point to the bottom (high address) of the
  // allocated stack area. To be safe in potential stack underflows we leave
  // some buffer below.
  registers_[sp] = reinterpret_cast<int64_t>(stack_) + stack_size_ - 64;
  // The ra and pc are initialized to a known bad value that will cause an
  // access violation if the simulator ever tries to execute it.
  registers_[pc] = bad_ra;
  registers_[ra] = bad_ra;

  last_debugger_input_ = NULL;
}


Simulator::~Simulator() { free(stack_); }


// When the generated code calls an external reference we need to catch that in
// the simulator.  The external reference will be a function compiled for the
// host architecture.  We need to call that function instead of trying to
// execute it with the simulator.  We do that by redirecting the external
// reference to a swi (software-interrupt) instruction that is handled by
// the simulator.  We write the original destination of the jump just at a known
// offset from the swi instruction so the simulator knows what to call.
class Redirection {
 public:
  Redirection(Isolate* isolate, void* external_function,
              ExternalReference::Type type)
      : external_function_(external_function),
        swi_instruction_(rtCallRedirInstr),
        type_(type),
        next_(NULL) {
    next_ = isolate->simulator_redirection();
    Simulator::current(isolate)->
        FlushICache(isolate->simulator_i_cache(),
                    reinterpret_cast<void*>(&swi_instruction_),
                    Instruction::kInstrSize);
    isolate->set_simulator_redirection(this);
  }

  void* address_of_swi_instruction() {
    return reinterpret_cast<void*>(&swi_instruction_);
  }

  void* external_function() { return external_function_; }
  ExternalReference::Type type() { return type_; }

  static Redirection* Get(Isolate* isolate, void* external_function,
                          ExternalReference::Type type) {
    Redirection* current = isolate->simulator_redirection();
    for (; current != NULL; current = current->next_) {
      if (current->external_function_ == external_function) return current;
    }
    return new Redirection(isolate, external_function, type);
  }

  static Redirection* FromSwiInstruction(Instruction* swi_instruction) {
    char* addr_of_swi = reinterpret_cast<char*>(swi_instruction);
    char* addr_of_redirection =
        addr_of_swi - offsetof(Redirection, swi_instruction_);
    return reinterpret_cast<Redirection*>(addr_of_redirection);
  }

  static void* ReverseRedirection(int64_t reg) {
    Redirection* redirection = FromSwiInstruction(
        reinterpret_cast<Instruction*>(reinterpret_cast<void*>(reg)));
    return redirection->external_function();
  }

  static void DeleteChain(Redirection* redirection) {
    while (redirection != nullptr) {
      Redirection* next = redirection->next_;
      delete redirection;
      redirection = next;
    }
  }

 private:
  void* external_function_;
  uint32_t swi_instruction_;
  ExternalReference::Type type_;
  Redirection* next_;
};


// static
void Simulator::TearDown(base::CustomMatcherHashMap* i_cache,
                         Redirection* first) {
  Redirection::DeleteChain(first);
  if (i_cache != nullptr) {
    for (base::HashMap::Entry* entry = i_cache->Start(); entry != nullptr;
         entry = i_cache->Next(entry)) {
      delete static_cast<CachePage*>(entry->value);
    }
    delete i_cache;
  }
}


void* Simulator::RedirectExternalReference(Isolate* isolate,
                                           void* external_function,
                                           ExternalReference::Type type) {
  Redirection* redirection = Redirection::Get(isolate, external_function, type);
  return redirection->address_of_swi_instruction();
}


// Get the active Simulator for the current thread.
Simulator* Simulator::current(Isolate* isolate) {
  v8::internal::Isolate::PerIsolateThreadData* isolate_data =
       isolate->FindOrAllocatePerThreadDataForThisThread();
  DCHECK(isolate_data != NULL);
  DCHECK(isolate_data != NULL);

  Simulator* sim = isolate_data->simulator();
  if (sim == NULL) {
    // TODO(146): delete the simulator object when a thread/isolate goes away.
    sim = new Simulator(isolate);
    isolate_data->set_simulator(sim);
  }
  return sim;
}


// Sets the register in the architecture state. It will also deal with updating
// Simulator internal state for special registers such as PC.
void Simulator::set_register(int reg, int64_t value) {
  DCHECK((reg >= 0) && (reg < kNumSimuRegisters));
  if (reg == pc) {
    pc_modified_ = true;
  }

  // Zero register always holds 0.
  registers_[reg] = (reg == 0) ? 0 : value;
}


void Simulator::set_dw_register(int reg, const int* dbl) {
  DCHECK((reg >= 0) && (reg < kNumSimuRegisters));
  registers_[reg] = dbl[1];
  registers_[reg] = registers_[reg] << 32;
  registers_[reg] += dbl[0];
}


void Simulator::set_fpu_register(int fpureg, int64_t value) {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  FPUregisters_[fpureg] = value;
}


void Simulator::set_fpu_register_word(int fpureg, int32_t value) {
  // Set ONLY lower 32-bits, leaving upper bits untouched.
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  int32_t* pword;
  if (kArchEndian == kLittle) {
    pword = reinterpret_cast<int32_t*>(&FPUregisters_[fpureg]);
  } else {
    pword = reinterpret_cast<int32_t*>(&FPUregisters_[fpureg]) + 1;
  }
  *pword = value;
}


void Simulator::set_fpu_register_hi_word(int fpureg, int32_t value) {
  // Set ONLY upper 32-bits, leaving lower bits untouched.
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  int32_t* phiword;
  if (kArchEndian == kLittle) {
    phiword = (reinterpret_cast<int32_t*>(&FPUregisters_[fpureg])) + 1;
  } else {
    phiword = reinterpret_cast<int32_t*>(&FPUregisters_[fpureg]);
  }
  *phiword = value;
}


void Simulator::set_fpu_register_float(int fpureg, float value) {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  *bit_cast<float*>(&FPUregisters_[fpureg]) = value;
}


void Simulator::set_fpu_register_double(int fpureg, double value) {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  *bit_cast<double*>(&FPUregisters_[fpureg]) = value;
}


// Get the register from the architecture state. This function does handle
// the special case of accessing the PC register.
int64_t Simulator::get_register(int reg) const {
  DCHECK((reg >= 0) && (reg < kNumSimuRegisters));
  if (reg == 0)
    return 0;
  else
    return registers_[reg] + ((reg == pc) ? Instruction::kPCReadOffset : 0);
}


double Simulator::get_double_from_register_pair(int reg) {
  // TODO(plind): bad ABI stuff, refactor or remove.
  DCHECK((reg >= 0) && (reg < kNumSimuRegisters) && ((reg % 2) == 0));

  double dm_val = 0.0;
  // Read the bits from the unsigned integer register_[] array
  // into the double precision floating point value and return it.
  char buffer[sizeof(registers_[0])];
  memcpy(buffer, &registers_[reg], sizeof(registers_[0]));
  memcpy(&dm_val, buffer, sizeof(registers_[0]));
  return(dm_val);
}


int64_t Simulator::get_fpu_register(int fpureg) const {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  return FPUregisters_[fpureg];
}


int32_t Simulator::get_fpu_register_word(int fpureg) const {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  return static_cast<int32_t>(FPUregisters_[fpureg] & 0xffffffff);
}


int32_t Simulator::get_fpu_register_signed_word(int fpureg) const {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  return static_cast<int32_t>(FPUregisters_[fpureg] & 0xffffffff);
}


int32_t Simulator::get_fpu_register_hi_word(int fpureg) const {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  return static_cast<int32_t>((FPUregisters_[fpureg] >> 32) & 0xffffffff);
}


float Simulator::get_fpu_register_float(int fpureg) const {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  return *bit_cast<float*>(const_cast<int64_t*>(&FPUregisters_[fpureg]));
}


double Simulator::get_fpu_register_double(int fpureg) const {
  DCHECK((fpureg >= 0) && (fpureg < kNumFPURegisters));
  return *bit_cast<double*>(&FPUregisters_[fpureg]);
}


// Runtime FP routines take up to two double arguments and zero
// or one integer arguments. All are constructed here,
// from a0-a3 or f12 and f13 (n64), or f14 (O32).
void Simulator::GetFpArgs(double* x, double* y, int32_t* z) {
  if (!IsMipsSoftFloatABI) {
    const int fparg2 = 13;
    *x = get_fpu_register_double(12);
    *y = get_fpu_register_double(fparg2);
    *z = static_cast<int32_t>(get_register(a2));
  } else {
  // TODO(plind): bad ABI stuff, refactor or remove.
    // We use a char buffer to get around the strict-aliasing rules which
    // otherwise allow the compiler to optimize away the copy.
    char buffer[sizeof(*x)];
    int32_t* reg_buffer = reinterpret_cast<int32_t*>(buffer);

    // Registers a0 and a1 -> x.
    reg_buffer[0] = get_register(a0);
    reg_buffer[1] = get_register(a1);
    memcpy(x, buffer, sizeof(buffer));
    // Registers a2 and a3 -> y.
    reg_buffer[0] = get_register(a2);
    reg_buffer[1] = get_register(a3);
    memcpy(y, buffer, sizeof(buffer));
    // Register 2 -> z.
    reg_buffer[0] = get_register(a2);
    memcpy(z, buffer, sizeof(*z));
  }
}


// The return value is either in v0/v1 or f0.
void Simulator::SetFpResult(const double& result) {
  if (!IsMipsSoftFloatABI) {
    set_fpu_register_double(0, result);
  } else {
    char buffer[2 * sizeof(registers_[0])];
    int64_t* reg_buffer = reinterpret_cast<int64_t*>(buffer);
    memcpy(buffer, &result, sizeof(buffer));
    // Copy result to v0 and v1.
    set_register(v0, reg_buffer[0]);
    set_register(v1, reg_buffer[1]);
  }
}


// Helper functions for setting and testing the FCSR register's bits.
void Simulator::set_fcsr_bit(uint32_t cc, bool value) {
  if (value) {
    FCSR_ |= (1 << cc);
  } else {
    FCSR_ &= ~(1 << cc);
  }
}


bool Simulator::test_fcsr_bit(uint32_t cc) {
  return FCSR_ & (1 << cc);
}


void Simulator::set_fcsr_rounding_mode(FPURoundingMode mode) {
  FCSR_ |= mode & kFPURoundingModeMask;
}


unsigned int Simulator::get_fcsr_rounding_mode() {
  return FCSR_ & kFPURoundingModeMask;
}


// Sets the rounding error codes in FCSR based on the result of the rounding.
// Returns true if the operation was invalid.
bool Simulator::set_fcsr_round_error(double original, double rounded) {
  bool ret = false;
  double max_int32 = std::numeric_limits<int32_t>::max();
  double min_int32 = std::numeric_limits<int32_t>::min();

  if (!std::isfinite(original) || !std::isfinite(rounded)) {
    set_fcsr_bit(kFCSRInvalidOpFlagBit, true);
    ret = true;
  }

  if (original != rounded) {
    set_fcsr_bit(kFCSRInexactFlagBit, true);
  }

  if (rounded < DBL_MIN && rounded > -DBL_MIN && rounded != 0) {
    set_fcsr_bit(kFCSRUnderflowFlagBit, true);
    ret = true;
  }

  if (rounded > max_int32 || rounded < min_int32) {
    set_fcsr_bit(kFCSROverflowFlagBit, true);
    // The reference is not really clear but it seems this is required:
    set_fcsr_bit(kFCSRInvalidOpFlagBit, true);
    ret = true;
  }

  return ret;
}


// Sets the rounding error codes in FCSR based on the result of the rounding.
// Returns true if the operation was invalid.
bool Simulator::set_fcsr_round64_error(double original, double rounded) {
  bool ret = false;
  // The value of INT64_MAX (2^63-1) can't be represented as double exactly,
  // loading the most accurate representation into max_int64, which is 2^63.
  double max_int64 = std::numeric_limits<int64_t>::max();
  double min_int64 = std::numeric_limits<int64_t>::min();

  if (!std::isfinite(original) || !std::isfinite(rounded)) {
    set_fcsr_bit(kFCSRInvalidOpFlagBit, true);
    ret = true;
  }

  if (original != rounded) {
    set_fcsr_bit(kFCSRInexactFlagBit, true);
  }

  if (rounded < DBL_MIN && rounded > -DBL_MIN && rounded != 0) {
    set_fcsr_bit(kFCSRUnderflowFlagBit, true);
    ret = true;
  }

  if (rounded >= max_int64 || rounded < min_int64) {
    set_fcsr_bit(kFCSROverflowFlagBit, true);
    // The reference is not really clear but it seems this is required:
    set_fcsr_bit(kFCSRInvalidOpFlagBit, true);
    ret = true;
  }

  return ret;
}


// Sets the rounding error codes in FCSR based on the result of the rounding.
// Returns true if the operation was invalid.
bool Simulator::set_fcsr_round_error(float original, float rounded) {
  bool ret = false;
  double max_int32 = std::numeric_limits<int32_t>::max();
  double min_int32 = std::numeric_limits<int32_t>::min();

  if (!std::isfinite(original) || !std::isfinite(rounded)) {
    set_fcsr_bit(kFCSRInvalidOpFlagBit, true);
    ret = true;
  }

  if (original != rounded) {
    set_fcsr_bit(kFCSRInexactFlagBit, true);
  }

  if (rounded < FLT_MIN && rounded > -FLT_MIN && rounded != 0) {
    set_fcsr_bit(kFCSRUnderflowFlagBit, true);
    ret = true;
  }

  if (rounded > max_int32 || rounded < min_int32) {
    set_fcsr_bit(kFCSROverflowFlagBit, true);
    // The reference is not really clear but it seems this is required:
    set_fcsr_bit(kFCSRInvalidOpFlagBit, true);
    ret = true;
  }

  return ret;
}

void Simulator::set_fpu_register_word_invalid_result(float original,
                                                     float rounded) {
  if (FCSR_ & kFCSRNaN2008FlagMask) {
    double max_int32 = std::numeric_limits<int32_t>::max();
    double min_int32 = std::numeric_limits<int32_t>::min();
    if (std::isnan(original)) {
      set_fpu_register_word(fd_reg(), 0);
    } else if (rounded > max_int32) {
      set_fpu_register_word(fd_reg(), kFPUInvalidResult);
    } else if (rounded < min_int32) {
      set_fpu_register_word(fd_reg(), kFPUInvalidResultNegative);
    } else {
      UNREACHABLE();
    }
  } else {
    set_fpu_register_word(fd_reg(), kFPUInvalidResult);
  }
}


void Simulator::set_fpu_register_invalid_result(float original, float rounded) {
  if (FCSR_ & kFCSRNaN2008FlagMask) {
    double max_int32 = std::numeric_limits<int32_t>::max();
    double min_int32 = std::numeric_limits<int32_t>::min();
    if (std::isnan(original)) {
      set_fpu_register(fd_reg(), 0);
    } else if (rounded > max_int32) {
      set_fpu_register(fd_reg(), kFPUInvalidResult);
    } else if (rounded < min_int32) {
      set_fpu_register(fd_reg(), kFPUInvalidResultNegative);
    } else {
      UNREACHABLE();
    }
  } else {
    set_fpu_register(fd_reg(), kFPUInvalidResult);
  }
}


void Simulator::set_fpu_register_invalid_result64(float original,
                                                  float rounded) {
  if (FCSR_ & kFCSRNaN2008FlagMask) {
    // The value of INT64_MAX (2^63-1) can't be represented as double exactly,
    // loading the most accurate representation into max_int64, which is 2^63.
    double max_int64 = std::numeric_limits<int64_t>::max();
    double min_int64 = std::numeric_limits<int64_t>::min();
    if (std::isnan(original)) {
      set_fpu_register(fd_reg(), 0);
    } else if (rounded >= max_int64) {
      set_fpu_register(fd_reg(), kFPU64InvalidResult);
    } else if (rounded < min_int64) {
      set_fpu_register(fd_reg(), kFPU64InvalidResultNegative);
    } else {
      UNREACHABLE();
    }
  } else {
    set_fpu_register(fd_reg(), kFPU64InvalidResult);
  }
}


void Simulator::set_fpu_register_word_invalid_result(double original,
                                                     double rounded) {
  if (FCSR_ & kFCSRNaN2008FlagMask) {
    double max_int32 = std::numeric_limits<int32_t>::max();
    double min_int32 = std::numeric_limits<int32_t>::min();
    if (std::isnan(original)) {
      set_fpu_register_word(fd_reg(), 0);
    } else if (rounded > max_int32) {
      set_fpu_register_word(fd_reg(), kFPUInvalidResult);
    } else if (rounded < min_int32) {
      set_fpu_register_word(fd_reg(), kFPUInvalidResultNegative);
    } else {
      UNREACHABLE();
    }
  } else {
    set_fpu_register_word(fd_reg(), kFPUInvalidResult);
  }
}


void Simulator::set_fpu_register_invalid_result(double original,
                                                double rounded) {
  if (FCSR_ & kFCSRNaN2008FlagMask) {
    double max_int32 = std::numeric_limits<int32_t>::max();
    double min_int32 = std::numeric_limits<int32_t>::min();
    if (std::isnan(original)) {
      set_fpu_register(fd_reg(), 0);
    } else if (rounded > max_int32) {
      set_fpu_register(fd_reg(), kFPUInvalidResult);
    } else if (rounded < min_int32) {
      set_fpu_register(fd_reg(), kFPUInvalidResultNegative);
    } else {
      UNREACHABLE();
    }
  } else {
    set_fpu_register(fd_reg(), kFPUInvalidResult);
  }
}


void Simulator::set_fpu_register_invalid_result64(double original,
                                                  double rounded) {
  if (FCSR_ & kFCSRNaN2008FlagMask) {
    // The value of INT64_MAX (2^63-1) can't be represented as double exactly,
    // loading the most accurate representation into max_int64, which is 2^63.
    double max_int64 = std::numeric_limits<int64_t>::max();
    double min_int64 = std::numeric_limits<int64_t>::min();
    if (std::isnan(original)) {
      set_fpu_register(fd_reg(), 0);
    } else if (rounded >= max_int64) {
      set_fpu_register(fd_reg(), kFPU64InvalidResult);
    } else if (rounded < min_int64) {
      set_fpu_register(fd_reg(), kFPU64InvalidResultNegative);
    } else {
      UNREACHABLE();
    }
  } else {
    set_fpu_register(fd_reg(), kFPU64InvalidResult);
  }
}


// Sets the rounding error codes in FCSR based on the result of the rounding.
// Returns true if the operation was invalid.
bool Simulator::set_fcsr_round64_error(float original, float rounded) {
  bool ret = false;
  // The value of INT64_MAX (2^63-1) can't be represented as double exactly,
  // loading the most accurate representation into max_int64, which is 2^63.
  double max_int64 = std::numeric_limits<int64_t>::max();
  double min_int64 = std::numeric_limits<int64_t>::min();

  if (!std::isfinite(original) || !std::isfinite(rounded)) {
    set_fcsr_bit(kFCSRInvalidOpFlagBit, true);
    ret = true;
  }

  if (original != rounded) {
    set_fcsr_bit(kFCSRInexactFlagBit, true);
  }

  if (rounded < FLT_MIN && rounded > -FLT_MIN && rounded != 0) {
    set_fcsr_bit(kFCSRUnderflowFlagBit, true);
    ret = true;
  }

  if (rounded >= max_int64 || rounded < min_int64) {
    set_fcsr_bit(kFCSROverflowFlagBit, true);
    // The reference is not really clear but it seems this is required:
    set_fcsr_bit(kFCSRInvalidOpFlagBit, true);
    ret = true;
  }

  return ret;
}


// For cvt instructions only
void Simulator::round_according_to_fcsr(double toRound, double& rounded,
                                        int32_t& rounded_int, double fs) {
  // 0 RN (round to nearest): Round a result to the nearest
  // representable value; if the result is exactly halfway between
  // two representable values, round to zero. Behave like round_w_d.

  // 1 RZ (round toward zero): Round a result to the closest
  // representable value whose absolute value is less than or
  // equal to the infinitely accurate result. Behave like trunc_w_d.

  // 2 RP (round up, or toward +infinity): Round a result to the
  // next representable value up. Behave like ceil_w_d.

  // 3 RN (round down, or toward −infinity): Round a result to
  // the next representable value down. Behave like floor_w_d.
  switch (FCSR_ & 3) {
    case kRoundToNearest:
      rounded = std::floor(fs + 0.5);
      rounded_int = static_cast<int32_t>(rounded);
      if ((rounded_int & 1) != 0 && rounded_int - fs == 0.5) {
        // If the number is halfway between two integers,
        // round to the even one.
        rounded_int--;
      }
      break;
    case kRoundToZero:
      rounded = trunc(fs);
      rounded_int = static_cast<int32_t>(rounded);
      break;
    case kRoundToPlusInf:
      rounded = std::ceil(fs);
      rounded_int = static_cast<int32_t>(rounded);
      break;
    case kRoundToMinusInf:
      rounded = std::floor(fs);
      rounded_int = static_cast<int32_t>(rounded);
      break;
  }
}


void Simulator::round64_according_to_fcsr(double toRound, double& rounded,
                                          int64_t& rounded_int, double fs) {
  // 0 RN (round to nearest): Round a result to the nearest
  // representable value; if the result is exactly halfway between
  // two representable values, round to zero. Behave like round_w_d.

  // 1 RZ (round toward zero): Round a result to the closest
  // representable value whose absolute value is less than or.
  // equal to the infinitely accurate result. Behave like trunc_w_d.

  // 2 RP (round up, or toward +infinity): Round a result to the
  // next representable value up. Behave like ceil_w_d.

  // 3 RN (round down, or toward −infinity): Round a result to
  // the next representable value down. Behave like floor_w_d.
  switch (FCSR_ & 3) {
    case kRoundToNearest:
      rounded = std::floor(fs + 0.5);
      rounded_int = static_cast<int64_t>(rounded);
      if ((rounded_int & 1) != 0 && rounded_int - fs == 0.5) {
        // If the number is halfway between two integers,
        // round to the even one.
        rounded_int--;
      }
      break;
    case kRoundToZero:
      rounded = trunc(fs);
      rounded_int = static_cast<int64_t>(rounded);
      break;
    case kRoundToPlusInf:
      rounded = std::ceil(fs);
      rounded_int = static_cast<int64_t>(rounded);
      break;
    case kRoundToMinusInf:
      rounded = std::floor(fs);
      rounded_int = static_cast<int64_t>(rounded);
      break;
  }
}


// for cvt instructions only
void Simulator::round_according_to_fcsr(float toRound, float& rounded,
                                        int32_t& rounded_int, float fs) {
  // 0 RN (round to nearest): Round a result to the nearest
  // representable value; if the result is exactly halfway between
  // two representable values, round to zero. Behave like round_w_d.

  // 1 RZ (round toward zero): Round a result to the closest
  // representable value whose absolute value is less than or
  // equal to the infinitely accurate result. Behave like trunc_w_d.

  // 2 RP (round up, or toward +infinity): Round a result to the
  // next representable value up. Behave like ceil_w_d.

  // 3 RN (round down, or toward −infinity): Round a result to
  // the next representable value down. Behave like floor_w_d.
  switch (FCSR_ & 3) {
    case kRoundToNearest:
      rounded = std::floor(fs + 0.5);
      rounded_int = static_cast<int32_t>(rounded);
      if ((rounded_int & 1) != 0 && rounded_int - fs == 0.5) {
        // If the number is halfway between two integers,
        // round to the even one.
        rounded_int--;
      }
      break;
    case kRoundToZero:
      rounded = trunc(fs);
      rounded_int = static_cast<int32_t>(rounded);
      break;
    case kRoundToPlusInf:
      rounded = std::ceil(fs);
      rounded_int = static_cast<int32_t>(rounded);
      break;
    case kRoundToMinusInf:
      rounded = std::floor(fs);
      rounded_int = static_cast<int32_t>(rounded);
      break;
  }
}


void Simulator::round64_according_to_fcsr(float toRound, float& rounded,
                                          int64_t& rounded_int, float fs) {
  // 0 RN (round to nearest): Round a result to the nearest
  // representable value; if the result is exactly halfway between
  // two representable values, round to zero. Behave like round_w_d.

  // 1 RZ (round toward zero): Round a result to the closest
  // representable value whose absolute value is less than or.
  // equal to the infinitely accurate result. Behave like trunc_w_d.

  // 2 RP (round up, or toward +infinity): Round a result to the
  // next representable value up. Behave like ceil_w_d.

  // 3 RN (round down, or toward −infinity): Round a result to
  // the next representable value down. Behave like floor_w_d.
  switch (FCSR_ & 3) {
    case kRoundToNearest:
      rounded = std::floor(fs + 0.5);
      rounded_int = static_cast<int64_t>(rounded);
      if ((rounded_int & 1) != 0 && rounded_int - fs == 0.5) {
        // If the number is halfway between two integers,
        // round to the even one.
        rounded_int--;
      }
      break;
    case kRoundToZero:
      rounded = trunc(fs);
      rounded_int = static_cast<int64_t>(rounded);
      break;
    case kRoundToPlusInf:
      rounded = std::ceil(fs);
      rounded_int = static_cast<int64_t>(rounded);
      break;
    case kRoundToMinusInf:
      rounded = std::floor(fs);
      rounded_int = static_cast<int64_t>(rounded);
      break;
  }
}


// Raw access to the PC register.
void Simulator::set_pc(int64_t value) {
  pc_modified_ = true;
  registers_[pc] = value;
}


bool Simulator::has_bad_pc() const {
  return ((registers_[pc] == bad_ra) || (registers_[pc] == end_sim_pc));
}


// Raw access to the PC register without the special adjustment when reading.
int64_t Simulator::get_pc() const {
  return registers_[pc];
}


// The MIPS cannot do unaligned reads and writes.  On some MIPS platforms an
// interrupt is caused.  On others it does a funky rotation thing.  For now we
// simply disallow unaligned reads, but at some point we may want to move to
// emulating the rotate behaviour.  Note that simulator runs have the runtime
// system running directly on the host system and only generated code is
// executed in the simulator.  Since the host is typically IA32 we will not
// get the correct MIPS-like behaviour on unaligned accesses.

// TODO(plind): refactor this messy debug code when we do unaligned access.
void Simulator::DieOrDebug() {
  if (1) {  // Flag for this was removed.
    MipsDebugger dbg(this);
    dbg.Debug();
  } else {
    base::OS::Abort();
  }
}


void Simulator::TraceRegWr(int64_t value) {
  if (::v8::internal::FLAG_trace_sim) {
    SNPrintF(trace_buf_, "%016" PRIx64 " ", value);
  }
}


// TODO(plind): consider making icount_ printing a flag option.
void Simulator::TraceMemRd(int64_t addr, int64_t value) {
  if (::v8::internal::FLAG_trace_sim) {
    SNPrintF(trace_buf_,
             "%016" PRIx64 "  <-- [%016" PRIx64 " ]    (%" PRId64 " )", value,
             addr, icount_);
  }
}


void Simulator::TraceMemWr(int64_t addr, int64_t value, TraceType t) {
  if (::v8::internal::FLAG_trace_sim) {
    switch (t) {
      case BYTE:
        SNPrintF(trace_buf_, "               %02x --> [%016" PRIx64 " ]",
                 static_cast<int8_t>(value), addr);
        break;
      case HALF:
        SNPrintF(trace_buf_, "            %04x --> [%016" PRIx64 " ]",
                 static_cast<int16_t>(value), addr);
        break;
      case WORD:
        SNPrintF(trace_buf_, "        %08x --> [%016" PRIx64 " ]",
                 static_cast<int32_t>(value), addr);
        break;
      case DWORD:
        SNPrintF(trace_buf_,
                 "%016" PRIx64 "  --> [%016" PRIx64 " ]    (%" PRId64 " )",
                 value, addr, icount_);
        break;
    }
  }
}


// TODO(plind): sign-extend and zero-extend not implmented properly
// on all the ReadXX functions, I don't think re-interpret cast does it.
int32_t Simulator::ReadW(int64_t addr, Instruction* instr) {
  if (addr >=0 && addr < 0x400) {
    // This has to be a NULL-dereference, drop into debugger.
    PrintF("Memory read from bad address: 0x%08" PRIx64 " , pc=0x%08" PRIxPTR
           " \n",
           addr, reinterpret_cast<intptr_t>(instr));
    DieOrDebug();
  }
  if ((addr & 0x3) == 0 || kArchVariant == kMips64r6) {
    int32_t* ptr = reinterpret_cast<int32_t*>(addr);
    TraceMemRd(addr, static_cast<int64_t>(*ptr));
    return *ptr;
  }
  PrintF("Unaligned read at 0x%08" PRIx64 " , pc=0x%08" V8PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
  return 0;
}


uint32_t Simulator::ReadWU(int64_t addr, Instruction* instr) {
  if (addr >=0 && addr < 0x400) {
    // This has to be a NULL-dereference, drop into debugger.
    PrintF("Memory read from bad address: 0x%08" PRIx64 " , pc=0x%08" PRIxPTR
           " \n",
           addr, reinterpret_cast<intptr_t>(instr));
    DieOrDebug();
  }
  if ((addr & 0x3) == 0 || kArchVariant == kMips64r6) {
    uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
    TraceMemRd(addr, static_cast<int64_t>(*ptr));
    return *ptr;
  }
  PrintF("Unaligned read at 0x%08" PRIx64 " , pc=0x%08" V8PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
  return 0;
}


void Simulator::WriteW(int64_t addr, int32_t value, Instruction* instr) {
  if (addr >= 0 && addr < 0x400) {
    // This has to be a NULL-dereference, drop into debugger.
    PrintF("Memory write to bad address: 0x%08" PRIx64 " , pc=0x%08" PRIxPTR
           " \n",
           addr, reinterpret_cast<intptr_t>(instr));
    DieOrDebug();
  }
  if ((addr & 0x3) == 0 || kArchVariant == kMips64r6) {
    TraceMemWr(addr, value, WORD);
    int* ptr = reinterpret_cast<int*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned write at 0x%08" PRIx64 " , pc=0x%08" V8PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
}


int64_t Simulator::Read2W(int64_t addr, Instruction* instr) {
  if (addr >=0 && addr < 0x400) {
    // This has to be a NULL-dereference, drop into debugger.
    PrintF("Memory read from bad address: 0x%08" PRIx64 " , pc=0x%08" PRIxPTR
           " \n",
           addr, reinterpret_cast<intptr_t>(instr));
    DieOrDebug();
  }
  if ((addr & kPointerAlignmentMask) == 0 || kArchVariant == kMips64r6) {
    int64_t* ptr = reinterpret_cast<int64_t*>(addr);
    TraceMemRd(addr, *ptr);
    return *ptr;
  }
  PrintF("Unaligned read at 0x%08" PRIx64 " , pc=0x%08" V8PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
  return 0;
}


void Simulator::Write2W(int64_t addr, int64_t value, Instruction* instr) {
  if (addr >= 0 && addr < 0x400) {
    // This has to be a NULL-dereference, drop into debugger.
    PrintF("Memory write to bad address: 0x%08" PRIx64 " , pc=0x%08" PRIxPTR
           "\n",
           addr, reinterpret_cast<intptr_t>(instr));
    DieOrDebug();
  }
  if ((addr & kPointerAlignmentMask) == 0 || kArchVariant == kMips64r6) {
    TraceMemWr(addr, value, DWORD);
    int64_t* ptr = reinterpret_cast<int64_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned write at 0x%08" PRIx64 " , pc=0x%08" V8PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
}


double Simulator::ReadD(int64_t addr, Instruction* instr) {
  if ((addr & kDoubleAlignmentMask) == 0 || kArchVariant == kMips64r6) {
    double* ptr = reinterpret_cast<double*>(addr);
    return *ptr;
  }
  PrintF("Unaligned (double) read at 0x%08" PRIx64 " , pc=0x%08" V8PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  base::OS::Abort();
  return 0;
}


void Simulator::WriteD(int64_t addr, double value, Instruction* instr) {
  if ((addr & kDoubleAlignmentMask) == 0 || kArchVariant == kMips64r6) {
    double* ptr = reinterpret_cast<double*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned (double) write at 0x%08" PRIx64 " , pc=0x%08" V8PRIxPTR
         "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
}


uint16_t Simulator::ReadHU(int64_t addr, Instruction* instr) {
  if ((addr & 1) == 0 || kArchVariant == kMips64r6) {
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    TraceMemRd(addr, static_cast<int64_t>(*ptr));
    return *ptr;
  }
  PrintF("Unaligned unsigned halfword read at 0x%08" PRIx64
         " , pc=0x%08" V8PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
  return 0;
}


int16_t Simulator::ReadH(int64_t addr, Instruction* instr) {
  if ((addr & 1) == 0 || kArchVariant == kMips64r6) {
    int16_t* ptr = reinterpret_cast<int16_t*>(addr);
    TraceMemRd(addr, static_cast<int64_t>(*ptr));
    return *ptr;
  }
  PrintF("Unaligned signed halfword read at 0x%08" PRIx64
         " , pc=0x%08" V8PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
  return 0;
}


void Simulator::WriteH(int64_t addr, uint16_t value, Instruction* instr) {
  if ((addr & 1) == 0 || kArchVariant == kMips64r6) {
    TraceMemWr(addr, value, HALF);
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned unsigned halfword write at 0x%08" PRIx64
         " , pc=0x%08" V8PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
}


void Simulator::WriteH(int64_t addr, int16_t value, Instruction* instr) {
  if ((addr & 1) == 0 || kArchVariant == kMips64r6) {
    TraceMemWr(addr, value, HALF);
    int16_t* ptr = reinterpret_cast<int16_t*>(addr);
    *ptr = value;
    return;
  }
  PrintF("Unaligned halfword write at 0x%08" PRIx64 " , pc=0x%08" V8PRIxPTR
         "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  DieOrDebug();
}


uint32_t Simulator::ReadBU(int64_t addr) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  TraceMemRd(addr, static_cast<int64_t>(*ptr));
  return *ptr & 0xff;
}


int32_t Simulator::ReadB(int64_t addr) {
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  TraceMemRd(addr, static_cast<int64_t>(*ptr));
  return *ptr;
}


void Simulator::WriteB(int64_t addr, uint8_t value) {
  TraceMemWr(addr, value, BYTE);
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  *ptr = value;
}


void Simulator::WriteB(int64_t addr, int8_t value) {
  TraceMemWr(addr, value, BYTE);
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  *ptr = value;
}


// Returns the limit of the stack area to enable checking for stack overflows.
uintptr_t Simulator::StackLimit(uintptr_t c_limit) const {
  // The simulator uses a separate JS stack. If we have exhausted the C stack,
  // we also drop down the JS limit to reflect the exhaustion on the JS stack.
  if (GetCurrentStackPosition() < c_limit) {
    return reinterpret_cast<uintptr_t>(get_sp());
  }

  // Otherwise the limit is the JS stack. Leave a safety margin of 1024 bytes
  // to prevent overrunning the stack when pushing values.
  return reinterpret_cast<uintptr_t>(stack_) + 1024;
}


// Unsupported instructions use Format to print an error and stop execution.
void Simulator::Format(Instruction* instr, const char* format) {
  PrintF("Simulator found unsupported instruction:\n 0x%08" PRIxPTR " : %s\n",
         reinterpret_cast<intptr_t>(instr), format);
  UNIMPLEMENTED_MIPS();
}


// Calls into the V8 runtime are based on this very simple interface.
// Note: To be able to return two values from some calls the code in runtime.cc
// uses the ObjectPair which is essentially two 32-bit values stuffed into a
// 64-bit value. With the code below we assume that all runtime calls return
// 64 bits of result. If they don't, the v1 result register contains a bogus
// value, which is fine because it is caller-saved.

typedef ObjectPair (*SimulatorRuntimeCall)(int64_t arg0,
                                        int64_t arg1,
                                        int64_t arg2,
                                        int64_t arg3,
                                        int64_t arg4,
                                        int64_t arg5);

typedef ObjectTriple (*SimulatorRuntimeTripleCall)(int64_t arg0, int64_t arg1,
                                                   int64_t arg2, int64_t arg3,
                                                   int64_t arg4);

// These prototypes handle the four types of FP calls.
typedef int64_t (*SimulatorRuntimeCompareCall)(double darg0, double darg1);
typedef double (*SimulatorRuntimeFPFPCall)(double darg0, double darg1);
typedef double (*SimulatorRuntimeFPCall)(double darg0);
typedef double (*SimulatorRuntimeFPIntCall)(double darg0, int32_t arg0);

// This signature supports direct call in to API function native callback
// (refer to InvocationCallback in v8.h).
typedef void (*SimulatorRuntimeDirectApiCall)(int64_t arg0);
typedef void (*SimulatorRuntimeProfilingApiCall)(int64_t arg0, void* arg1);

// This signature supports direct call to accessor getter callback.
typedef void (*SimulatorRuntimeDirectGetterCall)(int64_t arg0, int64_t arg1);
typedef void (*SimulatorRuntimeProfilingGetterCall)(
    int64_t arg0, int64_t arg1, void* arg2);

// Software interrupt instructions are used by the simulator to call into the
// C-based V8 runtime. They are also used for debugging with simulator.
void Simulator::SoftwareInterrupt() {
  // There are several instructions that could get us here,
  // the break_ instruction, or several variants of traps. All
  // Are "SPECIAL" class opcode, and are distinuished by function.
  int32_t func = instr_.FunctionFieldRaw();
  uint32_t code = (func == BREAK) ? instr_.Bits(25, 6) : -1;
  // We first check if we met a call_rt_redirected.
  if (instr_.InstructionBits() == rtCallRedirInstr) {
    Redirection* redirection = Redirection::FromSwiInstruction(instr_.instr());
    int64_t arg0 = get_register(a0);
    int64_t arg1 = get_register(a1);
    int64_t arg2 = get_register(a2);
    int64_t arg3 = get_register(a3);
    int64_t arg4, arg5;

    arg4 = get_register(a4);  // Abi n64 register a4.
    arg5 = get_register(a5);  // Abi n64 register a5.

    bool fp_call =
         (redirection->type() == ExternalReference::BUILTIN_FP_FP_CALL) ||
         (redirection->type() == ExternalReference::BUILTIN_COMPARE_CALL) ||
         (redirection->type() == ExternalReference::BUILTIN_FP_CALL) ||
         (redirection->type() == ExternalReference::BUILTIN_FP_INT_CALL);

    if (!IsMipsSoftFloatABI) {
      // With the hard floating point calling convention, double
      // arguments are passed in FPU registers. Fetch the arguments
      // from there and call the builtin using soft floating point
      // convention.
      switch (redirection->type()) {
      case ExternalReference::BUILTIN_FP_FP_CALL:
      case ExternalReference::BUILTIN_COMPARE_CALL:
        arg0 = get_fpu_register(f12);
        arg1 = get_fpu_register(f13);
        arg2 = get_fpu_register(f14);
        arg3 = get_fpu_register(f15);
        break;
      case ExternalReference::BUILTIN_FP_CALL:
        arg0 = get_fpu_register(f12);
        arg1 = get_fpu_register(f13);
        break;
      case ExternalReference::BUILTIN_FP_INT_CALL:
        arg0 = get_fpu_register(f12);
        arg1 = get_fpu_register(f13);
        arg2 = get_register(a2);
        break;
      default:
        break;
      }
    }

    // This is dodgy but it works because the C entry stubs are never moved.
    // See comment in codegen-arm.cc and bug 1242173.
    int64_t saved_ra = get_register(ra);

    intptr_t external =
          reinterpret_cast<intptr_t>(redirection->external_function());

    // Based on CpuFeatures::IsSupported(FPU), Mips will use either hardware
    // FPU, or gcc soft-float routines. Hardware FPU is simulated in this
    // simulator. Soft-float has additional abstraction of ExternalReference,
    // to support serialization.
    if (fp_call) {
      double dval0, dval1;  // one or two double parameters
      int32_t ival;         // zero or one integer parameters
      int64_t iresult = 0;  // integer return value
      double dresult = 0;   // double return value
      GetFpArgs(&dval0, &dval1, &ival);
      SimulatorRuntimeCall generic_target =
          reinterpret_cast<SimulatorRuntimeCall>(external);
      if (::v8::internal::FLAG_trace_sim) {
        switch (redirection->type()) {
          case ExternalReference::BUILTIN_FP_FP_CALL:
          case ExternalReference::BUILTIN_COMPARE_CALL:
            PrintF("Call to host function at %p with args %f, %f",
                   static_cast<void*>(FUNCTION_ADDR(generic_target)), dval0,
                   dval1);
            break;
          case ExternalReference::BUILTIN_FP_CALL:
            PrintF("Call to host function at %p with arg %f",
                   static_cast<void*>(FUNCTION_ADDR(generic_target)), dval0);
            break;
          case ExternalReference::BUILTIN_FP_INT_CALL:
            PrintF("Call to host function at %p with args %f, %d",
                   static_cast<void*>(FUNCTION_ADDR(generic_target)), dval0,
                   ival);
            break;
          default:
            UNREACHABLE();
            break;
        }
      }
      switch (redirection->type()) {
      case ExternalReference::BUILTIN_COMPARE_CALL: {
        SimulatorRuntimeCompareCall target =
          reinterpret_cast<SimulatorRuntimeCompareCall>(external);
        iresult = target(dval0, dval1);
        set_register(v0, static_cast<int64_t>(iresult));
      //  set_register(v1, static_cast<int64_t>(iresult >> 32));
        break;
      }
      case ExternalReference::BUILTIN_FP_FP_CALL: {
        SimulatorRuntimeFPFPCall target =
          reinterpret_cast<SimulatorRuntimeFPFPCall>(external);
        dresult = target(dval0, dval1);
        SetFpResult(dresult);
        break;
      }
      case ExternalReference::BUILTIN_FP_CALL: {
        SimulatorRuntimeFPCall target =
          reinterpret_cast<SimulatorRuntimeFPCall>(external);
        dresult = target(dval0);
        SetFpResult(dresult);
        break;
      }
      case ExternalReference::BUILTIN_FP_INT_CALL: {
        SimulatorRuntimeFPIntCall target =
          reinterpret_cast<SimulatorRuntimeFPIntCall>(external);
        dresult = target(dval0, ival);
        SetFpResult(dresult);
        break;
      }
      default:
        UNREACHABLE();
        break;
      }
      if (::v8::internal::FLAG_trace_sim) {
        switch (redirection->type()) {
        case ExternalReference::BUILTIN_COMPARE_CALL:
          PrintF("Returned %08x\n", static_cast<int32_t>(iresult));
          break;
        case ExternalReference::BUILTIN_FP_FP_CALL:
        case ExternalReference::BUILTIN_FP_CALL:
        case ExternalReference::BUILTIN_FP_INT_CALL:
          PrintF("Returned %f\n", dresult);
          break;
        default:
          UNREACHABLE();
          break;
        }
      }
    } else if (redirection->type() == ExternalReference::DIRECT_API_CALL) {
      if (::v8::internal::FLAG_trace_sim) {
        PrintF("Call to host function at %p args %08" PRIx64 " \n",
               reinterpret_cast<void*>(external), arg0);
      }
      SimulatorRuntimeDirectApiCall target =
          reinterpret_cast<SimulatorRuntimeDirectApiCall>(external);
      target(arg0);
    } else if (
        redirection->type() == ExternalReference::PROFILING_API_CALL) {
      if (::v8::internal::FLAG_trace_sim) {
        PrintF("Call to host function at %p args %08" PRIx64 "  %08" PRIx64
               " \n",
               reinterpret_cast<void*>(external), arg0, arg1);
      }
      SimulatorRuntimeProfilingApiCall target =
          reinterpret_cast<SimulatorRuntimeProfilingApiCall>(external);
      target(arg0, Redirection::ReverseRedirection(arg1));
    } else if (
        redirection->type() == ExternalReference::DIRECT_GETTER_CALL) {
      if (::v8::internal::FLAG_trace_sim) {
        PrintF("Call to host function at %p args %08" PRIx64 "  %08" PRIx64
               " \n",
               reinterpret_cast<void*>(external), arg0, arg1);
      }
      SimulatorRuntimeDirectGetterCall target =
          reinterpret_cast<SimulatorRuntimeDirectGetterCall>(external);
      target(arg0, arg1);
    } else if (
        redirection->type() == ExternalReference::PROFILING_GETTER_CALL) {
      if (::v8::internal::FLAG_trace_sim) {
        PrintF("Call to host function at %p args %08" PRIx64 "  %08" PRIx64
               "  %08" PRIx64 " \n",
               reinterpret_cast<void*>(external), arg0, arg1, arg2);
      }
      SimulatorRuntimeProfilingGetterCall target =
          reinterpret_cast<SimulatorRuntimeProfilingGetterCall>(external);
      target(arg0, arg1, Redirection::ReverseRedirection(arg2));
    } else if (redirection->type() == ExternalReference::BUILTIN_CALL_TRIPLE) {
      // builtin call returning ObjectTriple.
      SimulatorRuntimeTripleCall target =
          reinterpret_cast<SimulatorRuntimeTripleCall>(external);
      if (::v8::internal::FLAG_trace_sim) {
        PrintF(
            "Call to host triple returning runtime function %p "
            "args %016" PRIx64 ", %016" PRIx64 ", %016" PRIx64 ", %016" PRIx64
            ", %016" PRIx64 "\n",
            static_cast<void*>(FUNCTION_ADDR(target)), arg1, arg2, arg3, arg4,
            arg5);
      }
      // arg0 is a hidden argument pointing to the return location, so don't
      // pass it to the target function.
      ObjectTriple result = target(arg1, arg2, arg3, arg4, arg5);
      if (::v8::internal::FLAG_trace_sim) {
        PrintF("Returned { %p, %p, %p }\n", static_cast<void*>(result.x),
               static_cast<void*>(result.y), static_cast<void*>(result.z));
      }
      // Return is passed back in address pointed to by hidden first argument.
      ObjectTriple* sim_result = reinterpret_cast<ObjectTriple*>(arg0);
      *sim_result = result;
      set_register(v0, arg0);
    } else {
      DCHECK(redirection->type() == ExternalReference::BUILTIN_CALL ||
             redirection->type() == ExternalReference::BUILTIN_CALL_PAIR);
      SimulatorRuntimeCall target =
                  reinterpret_cast<SimulatorRuntimeCall>(external);
      if (::v8::internal::FLAG_trace_sim) {
        PrintF(
            "Call to host function at %p "
            "args %08" PRIx64 " , %08" PRIx64 " , %08" PRIx64 " , %08" PRIx64
            " , %08" PRIx64 " , %08" PRIx64 " \n",
            static_cast<void*>(FUNCTION_ADDR(target)), arg0, arg1, arg2, arg3,
            arg4, arg5);
      }
      // int64_t result = target(arg0, arg1, arg2, arg3, arg4, arg5);
      // set_register(v0, static_cast<int32_t>(result));
      // set_register(v1, static_cast<int32_t>(result >> 32));
      ObjectPair result = target(arg0, arg1, arg2, arg3, arg4, arg5);
      set_register(v0, (int64_t)(result.x));
      set_register(v1, (int64_t)(result.y));
    }
     if (::v8::internal::FLAG_trace_sim) {
       PrintF("Returned %08" PRIx64 "  : %08" PRIx64 " \n", get_register(v1),
              get_register(v0));
    }
    set_register(ra, saved_ra);
    set_pc(get_register(ra));

  } else if (func == BREAK && code <= kMaxStopCode) {
    if (IsWatchpoint(code)) {
      PrintWatchpoint(code);
    } else {
      IncreaseStopCounter(code);
      HandleStop(code, instr_.instr());
    }
  } else {
    // All remaining break_ codes, and all traps are handled here.
    MipsDebugger dbg(this);
    dbg.Debug();
  }
}


// Stop helper functions.
bool Simulator::IsWatchpoint(uint64_t code) {
  return (code <= kMaxWatchpointCode);
}


void Simulator::PrintWatchpoint(uint64_t code) {
  MipsDebugger dbg(this);
  ++break_count_;
  PrintF("\n---- break %" PRId64 "  marker: %3d  (instr count: %8" PRId64
         " ) ----------"
         "----------------------------------",
         code, break_count_, icount_);
  dbg.PrintAllRegs();  // Print registers and continue running.
}


void Simulator::HandleStop(uint64_t code, Instruction* instr) {
  // Stop if it is enabled, otherwise go on jumping over the stop
  // and the message address.
  if (IsEnabledStop(code)) {
    MipsDebugger dbg(this);
    dbg.Stop(instr);
  } else {
    set_pc(get_pc() + 2 * Instruction::kInstrSize);
  }
}


bool Simulator::IsStopInstruction(Instruction* instr) {
  int32_t func = instr->FunctionFieldRaw();
  uint32_t code = static_cast<uint32_t>(instr->Bits(25, 6));
  return (func == BREAK) && code > kMaxWatchpointCode && code <= kMaxStopCode;
}


bool Simulator::IsEnabledStop(uint64_t code) {
  DCHECK(code <= kMaxStopCode);
  DCHECK(code > kMaxWatchpointCode);
  return !(watched_stops_[code].count & kStopDisabledBit);
}


void Simulator::EnableStop(uint64_t code) {
  if (!IsEnabledStop(code)) {
    watched_stops_[code].count &= ~kStopDisabledBit;
  }
}


void Simulator::DisableStop(uint64_t code) {
  if (IsEnabledStop(code)) {
    watched_stops_[code].count |= kStopDisabledBit;
  }
}


void Simulator::IncreaseStopCounter(uint64_t code) {
  DCHECK(code <= kMaxStopCode);
  if ((watched_stops_[code].count & ~(1 << 31)) == 0x7fffffff) {
    PrintF("Stop counter for code %" PRId64
           "  has overflowed.\n"
           "Enabling this code and reseting the counter to 0.\n",
           code);
    watched_stops_[code].count = 0;
    EnableStop(code);
  } else {
    watched_stops_[code].count++;
  }
}


// Print a stop status.
void Simulator::PrintStopInfo(uint64_t code) {
  if (code <= kMaxWatchpointCode) {
    PrintF("That is a watchpoint, not a stop.\n");
    return;
  } else if (code > kMaxStopCode) {
    PrintF("Code too large, only %u stops can be used\n", kMaxStopCode + 1);
    return;
  }
  const char* state = IsEnabledStop(code) ? "Enabled" : "Disabled";
  int32_t count = watched_stops_[code].count & ~kStopDisabledBit;
  // Don't print the state of unused breakpoints.
  if (count != 0) {
    if (watched_stops_[code].desc) {
      PrintF("stop %" PRId64 "  - 0x%" PRIx64 " : \t%s, \tcounter = %i, \t%s\n",
             code, code, state, count, watched_stops_[code].desc);
    } else {
      PrintF("stop %" PRId64 "  - 0x%" PRIx64 " : \t%s, \tcounter = %i\n", code,
             code, state, count);
    }
  }
}


void Simulator::SignalException(Exception e) {
  V8_Fatal(__FILE__, __LINE__, "Error: Exception %i raised.",
           static_cast<int>(e));
}

// Min/Max template functions for Double and Single arguments.

template <typename T>
static T FPAbs(T a);

template <>
double FPAbs<double>(double a) {
  return fabs(a);
}

template <>
float FPAbs<float>(float a) {
  return fabsf(a);
}

template <typename T>
static bool FPUProcessNaNsAndZeros(T a, T b, MaxMinKind kind, T& result) {
  if (std::isnan(a) && std::isnan(b)) {
    result = a;
  } else if (std::isnan(a)) {
    result = b;
  } else if (std::isnan(b)) {
    result = a;
  } else if (b == a) {
    // Handle -0.0 == 0.0 case.
    // std::signbit() returns int 0 or 1 so substracting MaxMinKind::kMax
    // negates the result.
    result = std::signbit(b) - static_cast<int>(kind) ? b : a;
  } else {
    return false;
  }
  return true;
}

template <typename T>
static T FPUMin(T a, T b) {
  T result;
  if (FPUProcessNaNsAndZeros(a, b, MaxMinKind::kMin, result)) {
    return result;
  } else {
    return b < a ? b : a;
  }
}

template <typename T>
static T FPUMax(T a, T b) {
  T result;
  if (FPUProcessNaNsAndZeros(a, b, MaxMinKind::kMax, result)) {
    return result;
  } else {
    return b > a ? b : a;
  }
}

template <typename T>
static T FPUMinA(T a, T b) {
  T result;
  if (!FPUProcessNaNsAndZeros(a, b, MaxMinKind::kMin, result)) {
    if (FPAbs(a) < FPAbs(b)) {
      result = a;
    } else if (FPAbs(b) < FPAbs(a)) {
      result = b;
    } else {
      result = a < b ? a : b;
    }
  }
  return result;
}

template <typename T>
static T FPUMaxA(T a, T b) {
  T result;
  if (!FPUProcessNaNsAndZeros(a, b, MaxMinKind::kMin, result)) {
    if (FPAbs(a) > FPAbs(b)) {
      result = a;
    } else if (FPAbs(b) > FPAbs(a)) {
      result = b;
    } else {
      result = a > b ? a : b;
    }
  }
  return result;
}

enum class KeepSign : bool { no = false, yes };

template <typename T, typename std::enable_if<std::is_floating_point<T>::value,
                                              int>::type = 0>
T FPUCanonalizeNaNArg(T result, T arg, KeepSign keepSign = KeepSign::no) {
  DCHECK(std::isnan(arg));
  T qNaN = std::numeric_limits<T>::quiet_NaN();
  if (keepSign == KeepSign::yes) {
    return std::copysign(qNaN, result);
  }
  return qNaN;
}

template <typename T>
T FPUCanonalizeNaNArgs(T result, KeepSign keepSign, T first) {
  if (std::isnan(first)) {
    return FPUCanonalizeNaNArg(result, first, keepSign);
  }
  return result;
}

template <typename T, typename... Args>
T FPUCanonalizeNaNArgs(T result, KeepSign keepSign, T first, Args... args) {
  if (std::isnan(first)) {
    return FPUCanonalizeNaNArg(result, first, keepSign);
  }
  return FPUCanonalizeNaNArgs(result, keepSign, args...);
}

template <typename Func, typename T, typename... Args>
T FPUCanonalizeOperation(Func f, T first, Args... args) {
  return FPUCanonalizeOperation(f, KeepSign::no, first, args...);
}

template <typename Func, typename T, typename... Args>
T FPUCanonalizeOperation(Func f, KeepSign keepSign, T first, Args... args) {
  T result = f(first, args...);
  if (std::isnan(result)) {
    result = FPUCanonalizeNaNArgs(result, keepSign, first, args...);
  }
  return result;
}

// Handle execution based on instruction types.

void Simulator::DecodeTypeRegisterSRsType() {
  float fs, ft, fd;
  fs = get_fpu_register_float(fs_reg());
  ft = get_fpu_register_float(ft_reg());
  fd = get_fpu_register_float(fd_reg());
  int32_t ft_int = bit_cast<int32_t>(ft);
  int32_t fd_int = bit_cast<int32_t>(fd);
  uint32_t cc, fcsr_cc;
  cc = instr_.FCccValue();
  fcsr_cc = get_fcsr_condition_bit(cc);
  switch (instr_.FunctionFieldRaw()) {
    case RINT: {
      DCHECK(kArchVariant == kMips64r6);
      float result, temp_result;
      double temp;
      float upper = std::ceil(fs);
      float lower = std::floor(fs);
      switch (get_fcsr_rounding_mode()) {
        case kRoundToNearest:
          if (upper - fs < fs - lower) {
            result = upper;
          } else if (upper - fs > fs - lower) {
            result = lower;
          } else {
            temp_result = upper / 2;
            float reminder = modf(temp_result, &temp);
            if (reminder == 0) {
              result = upper;
            } else {
              result = lower;
            }
          }
          break;
        case kRoundToZero:
          result = (fs > 0 ? lower : upper);
          break;
        case kRoundToPlusInf:
          result = upper;
          break;
        case kRoundToMinusInf:
          result = lower;
          break;
      }
      set_fpu_register_float(fd_reg(), result);
      if (result != fs) {
        set_fcsr_bit(kFCSRInexactFlagBit, true);
      }
      break;
    }
    case ADD_S:
      set_fpu_register_float(
          fd_reg(),
          FPUCanonalizeOperation([](float lhs, float rhs) { return lhs + rhs; },
                                 fs, ft));
      break;
    case SUB_S:
      set_fpu_register_float(
          fd_reg(),
          FPUCanonalizeOperation([](float lhs, float rhs) { return lhs - rhs; },
                                 fs, ft));
      break;
    case MADDF_S:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), fd + (fs * ft));
      break;
    case MSUBF_S:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), fd - (fs * ft));
      break;
    case MUL_S:
      set_fpu_register_float(
          fd_reg(),
          FPUCanonalizeOperation([](float lhs, float rhs) { return lhs * rhs; },
                                 fs, ft));
      break;
    case DIV_S:
      set_fpu_register_float(
          fd_reg(),
          FPUCanonalizeOperation([](float lhs, float rhs) { return lhs / rhs; },
                                 fs, ft));
      break;
    case ABS_S:
      set_fpu_register_float(
          fd_reg(),
          FPUCanonalizeOperation([](float fs) { return FPAbs(fs); }, fs));
      break;
    case MOV_S:
      set_fpu_register_float(fd_reg(), fs);
      break;
    case NEG_S:
      set_fpu_register_float(
          fd_reg(), FPUCanonalizeOperation([](float src) { return -src; },
                                           KeepSign::yes, fs));
      break;
    case SQRT_S:
      set_fpu_register_float(
          fd_reg(),
          FPUCanonalizeOperation([](float src) { return std::sqrt(src); }, fs));
      break;
    case RSQRT_S:
      set_fpu_register_float(
          fd_reg(), FPUCanonalizeOperation(
                        [](float src) { return 1.0 / std::sqrt(src); }, fs));
      break;
    case RECIP_S:
      set_fpu_register_float(
          fd_reg(),
          FPUCanonalizeOperation([](float src) { return 1.0 / src; }, fs));
      break;
    case C_F_D:
      set_fcsr_bit(fcsr_cc, false);
      break;
    case C_UN_D:
      set_fcsr_bit(fcsr_cc, std::isnan(fs) || std::isnan(ft));
      break;
    case C_EQ_D:
      set_fcsr_bit(fcsr_cc, (fs == ft));
      break;
    case C_UEQ_D:
      set_fcsr_bit(fcsr_cc, (fs == ft) || (std::isnan(fs) || std::isnan(ft)));
      break;
    case C_OLT_D:
      set_fcsr_bit(fcsr_cc, (fs < ft));
      break;
    case C_ULT_D:
      set_fcsr_bit(fcsr_cc, (fs < ft) || (std::isnan(fs) || std::isnan(ft)));
      break;
    case C_OLE_D:
      set_fcsr_bit(fcsr_cc, (fs <= ft));
      break;
    case C_ULE_D:
      set_fcsr_bit(fcsr_cc, (fs <= ft) || (std::isnan(fs) || std::isnan(ft)));
      break;
    case CVT_D_S:
      set_fpu_register_double(fd_reg(), static_cast<double>(fs));
      break;
    case CLASS_S: {  // Mips64r6 instruction
      // Convert float input to uint32_t for easier bit manipulation
      uint32_t classed = bit_cast<uint32_t>(fs);

      // Extracting sign, exponent and mantissa from the input float
      uint32_t sign = (classed >> 31) & 1;
      uint32_t exponent = (classed >> 23) & 0x000000ff;
      uint32_t mantissa = classed & 0x007fffff;
      uint32_t result;
      float fResult;

      // Setting flags if input float is negative infinity,
      // positive infinity, negative zero or positive zero
      bool negInf = (classed == 0xFF800000);
      bool posInf = (classed == 0x7F800000);
      bool negZero = (classed == 0x80000000);
      bool posZero = (classed == 0x00000000);

      bool signalingNan;
      bool quietNan;
      bool negSubnorm;
      bool posSubnorm;
      bool negNorm;
      bool posNorm;

      // Setting flags if float is NaN
      signalingNan = false;
      quietNan = false;
      if (!negInf && !posInf && (exponent == 0xff)) {
        quietNan = ((mantissa & 0x00200000) == 0) &&
                   ((mantissa & (0x00200000 - 1)) == 0);
        signalingNan = !quietNan;
      }

      // Setting flags if float is subnormal number
      posSubnorm = false;
      negSubnorm = false;
      if ((exponent == 0) && (mantissa != 0)) {
        DCHECK(sign == 0 || sign == 1);
        posSubnorm = (sign == 0);
        negSubnorm = (sign == 1);
      }

      // Setting flags if float is normal number
      posNorm = false;
      negNorm = false;
      if (!posSubnorm && !negSubnorm && !posInf && !negInf && !signalingNan &&
          !quietNan && !negZero && !posZero) {
        DCHECK(sign == 0 || sign == 1);
        posNorm = (sign == 0);
        negNorm = (sign == 1);
      }

      // Calculating result according to description of CLASS.S instruction
      result = (posZero << 9) | (posSubnorm << 8) | (posNorm << 7) |
               (posInf << 6) | (negZero << 5) | (negSubnorm << 4) |
               (negNorm << 3) | (negInf << 2) | (quietNan << 1) | signalingNan;

      DCHECK(result != 0);

      fResult = bit_cast<float>(result);
      set_fpu_register_float(fd_reg(), fResult);

      break;
    }
    case CVT_L_S: {
      float rounded;
      int64_t result;
      round64_according_to_fcsr(fs, rounded, result, fs);
      set_fpu_register(fd_reg(), result);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case CVT_W_S: {
      float rounded;
      int32_t result;
      round_according_to_fcsr(fs, rounded, result, fs);
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_word_invalid_result(fs, rounded);
      }
      break;
    }
    case TRUNC_W_S: {  // Truncate single to word (round towards 0).
      float rounded = trunc(fs);
      int32_t result = static_cast<int32_t>(rounded);
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_word_invalid_result(fs, rounded);
      }
    } break;
    case TRUNC_L_S: {  // Mips64r2 instruction.
      float rounded = trunc(fs);
      int64_t result = static_cast<int64_t>(rounded);
      set_fpu_register(fd_reg(), result);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case ROUND_W_S: {
      float rounded = std::floor(fs + 0.5);
      int32_t result = static_cast<int32_t>(rounded);
      if ((result & 1) != 0 && result - fs == 0.5) {
        // If the number is halfway between two integers,
        // round to the even one.
        result--;
      }
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_word_invalid_result(fs, rounded);
      }
      break;
    }
    case ROUND_L_S: {  // Mips64r2 instruction.
      float rounded = std::floor(fs + 0.5);
      int64_t result = static_cast<int64_t>(rounded);
      if ((result & 1) != 0 && result - fs == 0.5) {
        // If the number is halfway between two integers,
        // round to the even one.
        result--;
      }
      int64_t i64 = static_cast<int64_t>(result);
      set_fpu_register(fd_reg(), i64);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case FLOOR_L_S: {  // Mips64r2 instruction.
      float rounded = floor(fs);
      int64_t result = static_cast<int64_t>(rounded);
      set_fpu_register(fd_reg(), result);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case FLOOR_W_S:  // Round double to word towards negative infinity.
    {
      float rounded = std::floor(fs);
      int32_t result = static_cast<int32_t>(rounded);
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_word_invalid_result(fs, rounded);
      }
    } break;
    case CEIL_W_S:  // Round double to word towards positive infinity.
    {
      float rounded = std::ceil(fs);
      int32_t result = static_cast<int32_t>(rounded);
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_invalid_result(fs, rounded);
      }
    } break;
    case CEIL_L_S: {  // Mips64r2 instruction.
      float rounded = ceil(fs);
      int64_t result = static_cast<int64_t>(rounded);
      set_fpu_register(fd_reg(), result);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case MINA:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), FPUMinA(ft, fs));
      break;
    case MAXA:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), FPUMaxA(ft, fs));
      break;
    case MIN:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), FPUMin(ft, fs));
      break;
    case MAX:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), FPUMax(ft, fs));
      break;
    case SEL:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), (fd_int & 0x1) == 0 ? fs : ft);
      break;
    case SELEQZ_C:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), (ft_int & 0x1) == 0
                                           ? get_fpu_register_float(fs_reg())
                                           : 0.0);
      break;
    case SELNEZ_C:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_float(fd_reg(), (ft_int & 0x1) != 0
                                           ? get_fpu_register_float(fs_reg())
                                           : 0.0);
      break;
    case MOVZ_C: {
      DCHECK(kArchVariant == kMips64r2);
      if (rt() == 0) {
        set_fpu_register_float(fd_reg(), fs);
      }
      break;
    }
    case MOVN_C: {
      DCHECK(kArchVariant == kMips64r2);
      if (rt() != 0) {
        set_fpu_register_float(fd_reg(), fs);
      }
      break;
    }
    case MOVF: {
      // Same function field for MOVT.D and MOVF.D
      uint32_t ft_cc = (ft_reg() >> 2) & 0x7;
      ft_cc = get_fcsr_condition_bit(ft_cc);

      if (instr_.Bit(16)) {  // Read Tf bit.
        // MOVT.D
        if (test_fcsr_bit(ft_cc)) set_fpu_register_float(fd_reg(), fs);
      } else {
        // MOVF.D
        if (!test_fcsr_bit(ft_cc)) set_fpu_register_float(fd_reg(), fs);
      }
      break;
    }
    default:
      // TRUNC_W_S ROUND_W_S ROUND_L_S FLOOR_W_S FLOOR_L_S
      // CEIL_W_S CEIL_L_S CVT_PS_S are unimplemented.
      UNREACHABLE();
  }
}


void Simulator::DecodeTypeRegisterDRsType() {
  double ft, fs, fd;
  uint32_t cc, fcsr_cc;
  fs = get_fpu_register_double(fs_reg());
  ft = (instr_.FunctionFieldRaw() != MOVF) ? get_fpu_register_double(ft_reg())
                                           : 0.0;
  fd = get_fpu_register_double(fd_reg());
  cc = instr_.FCccValue();
  fcsr_cc = get_fcsr_condition_bit(cc);
  int64_t ft_int = bit_cast<int64_t>(ft);
  int64_t fd_int = bit_cast<int64_t>(fd);
  switch (instr_.FunctionFieldRaw()) {
    case RINT: {
      DCHECK(kArchVariant == kMips64r6);
      double result, temp, temp_result;
      double upper = std::ceil(fs);
      double lower = std::floor(fs);
      switch (get_fcsr_rounding_mode()) {
        case kRoundToNearest:
          if (upper - fs < fs - lower) {
            result = upper;
          } else if (upper - fs > fs - lower) {
            result = lower;
          } else {
            temp_result = upper / 2;
            double reminder = modf(temp_result, &temp);
            if (reminder == 0) {
              result = upper;
            } else {
              result = lower;
            }
          }
          break;
        case kRoundToZero:
          result = (fs > 0 ? lower : upper);
          break;
        case kRoundToPlusInf:
          result = upper;
          break;
        case kRoundToMinusInf:
          result = lower;
          break;
      }
      set_fpu_register_double(fd_reg(), result);
      if (result != fs) {
        set_fcsr_bit(kFCSRInexactFlagBit, true);
      }
      break;
    }
    case SEL:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), (fd_int & 0x1) == 0 ? fs : ft);
      break;
    case SELEQZ_C:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), (ft_int & 0x1) == 0 ? fs : 0.0);
      break;
    case SELNEZ_C:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), (ft_int & 0x1) != 0 ? fs : 0.0);
      break;
    case MOVZ_C: {
      DCHECK(kArchVariant == kMips64r2);
      if (rt() == 0) {
        set_fpu_register_double(fd_reg(), fs);
      }
      break;
    }
    case MOVN_C: {
      DCHECK(kArchVariant == kMips64r2);
      if (rt() != 0) {
        set_fpu_register_double(fd_reg(), fs);
      }
      break;
    }
    case MOVF: {
      // Same function field for MOVT.D and MOVF.D
      uint32_t ft_cc = (ft_reg() >> 2) & 0x7;
      ft_cc = get_fcsr_condition_bit(ft_cc);
      if (instr_.Bit(16)) {  // Read Tf bit.
        // MOVT.D
        if (test_fcsr_bit(ft_cc)) set_fpu_register_double(fd_reg(), fs);
      } else {
        // MOVF.D
        if (!test_fcsr_bit(ft_cc)) set_fpu_register_double(fd_reg(), fs);
      }
      break;
    }
    case MINA:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), FPUMinA(ft, fs));
      break;
    case MAXA:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), FPUMaxA(ft, fs));
      break;
    case MIN:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), FPUMin(ft, fs));
      break;
    case MAX:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), FPUMax(ft, fs));
      break;
    case ADD_D:
      set_fpu_register_double(
          fd_reg(),
          FPUCanonalizeOperation(
              [](double lhs, double rhs) { return lhs + rhs; }, fs, ft));
      break;
    case SUB_D:
      set_fpu_register_double(
          fd_reg(),
          FPUCanonalizeOperation(
              [](double lhs, double rhs) { return lhs - rhs; }, fs, ft));
      break;
    case MADDF_D:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), fd + (fs * ft));
      break;
    case MSUBF_D:
      DCHECK(kArchVariant == kMips64r6);
      set_fpu_register_double(fd_reg(), fd - (fs * ft));
      break;
    case MUL_D:
      set_fpu_register_double(
          fd_reg(),
          FPUCanonalizeOperation(
              [](double lhs, double rhs) { return lhs * rhs; }, fs, ft));
      break;
    case DIV_D:
      set_fpu_register_double(
          fd_reg(),
          FPUCanonalizeOperation(
              [](double lhs, double rhs) { return lhs / rhs; }, fs, ft));
      break;
    case ABS_D:
      set_fpu_register_double(
          fd_reg(),
          FPUCanonalizeOperation([](double fs) { return FPAbs(fs); }, fs));
      break;
    case MOV_D:
      set_fpu_register_double(fd_reg(), fs);
      break;
    case NEG_D:
      set_fpu_register_double(
          fd_reg(), FPUCanonalizeOperation([](double src) { return -src; },
                                           KeepSign::yes, fs));
      break;
    case SQRT_D:
      set_fpu_register_double(
          fd_reg(),
          FPUCanonalizeOperation([](double fs) { return std::sqrt(fs); }, fs));
      break;
    case RSQRT_D:
      set_fpu_register_double(
          fd_reg(), FPUCanonalizeOperation(
                        [](double fs) { return 1.0 / std::sqrt(fs); }, fs));
      break;
    case RECIP_D:
      set_fpu_register_double(
          fd_reg(),
          FPUCanonalizeOperation([](double fs) { return 1.0 / fs; }, fs));
      break;
    case C_UN_D:
      set_fcsr_bit(fcsr_cc, std::isnan(fs) || std::isnan(ft));
      break;
    case C_EQ_D:
      set_fcsr_bit(fcsr_cc, (fs == ft));
      break;
    case C_UEQ_D:
      set_fcsr_bit(fcsr_cc, (fs == ft) || (std::isnan(fs) || std::isnan(ft)));
      break;
    case C_OLT_D:
      set_fcsr_bit(fcsr_cc, (fs < ft));
      break;
    case C_ULT_D:
      set_fcsr_bit(fcsr_cc, (fs < ft) || (std::isnan(fs) || std::isnan(ft)));
      break;
    case C_OLE_D:
      set_fcsr_bit(fcsr_cc, (fs <= ft));
      break;
    case C_ULE_D:
      set_fcsr_bit(fcsr_cc, (fs <= ft) || (std::isnan(fs) || std::isnan(ft)));
      break;
    case CVT_W_D: {  // Convert double to word.
      double rounded;
      int32_t result;
      round_according_to_fcsr(fs, rounded, result, fs);
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_word_invalid_result(fs, rounded);
      }
      break;
    }
    case ROUND_W_D:  // Round double to word (round half to even).
    {
      double rounded = std::floor(fs + 0.5);
      int32_t result = static_cast<int32_t>(rounded);
      if ((result & 1) != 0 && result - fs == 0.5) {
        // If the number is halfway between two integers,
        // round to the even one.
        result--;
      }
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_invalid_result(fs, rounded);
      }
    } break;
    case TRUNC_W_D:  // Truncate double to word (round towards 0).
    {
      double rounded = trunc(fs);
      int32_t result = static_cast<int32_t>(rounded);
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_invalid_result(fs, rounded);
      }
    } break;
    case FLOOR_W_D:  // Round double to word towards negative infinity.
    {
      double rounded = std::floor(fs);
      int32_t result = static_cast<int32_t>(rounded);
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_invalid_result(fs, rounded);
      }
    } break;
    case CEIL_W_D:  // Round double to word towards positive infinity.
    {
      double rounded = std::ceil(fs);
      int32_t result = static_cast<int32_t>(rounded);
      set_fpu_register_word(fd_reg(), result);
      if (set_fcsr_round_error(fs, rounded)) {
        set_fpu_register_invalid_result(fs, rounded);
      }
    } break;
    case CVT_S_D:  // Convert double to float (single).
      set_fpu_register_float(fd_reg(), static_cast<float>(fs));
      break;
    case CVT_L_D: {  // Mips64r2: Truncate double to 64-bit long-word.
      double rounded;
      int64_t result;
      round64_according_to_fcsr(fs, rounded, result, fs);
      set_fpu_register(fd_reg(), result);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case ROUND_L_D: {  // Mips64r2 instruction.
      double rounded = std::floor(fs + 0.5);
      int64_t result = static_cast<int64_t>(rounded);
      if ((result & 1) != 0 && result - fs == 0.5) {
        // If the number is halfway between two integers,
        // round to the even one.
        result--;
      }
      int64_t i64 = static_cast<int64_t>(result);
      set_fpu_register(fd_reg(), i64);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case TRUNC_L_D: {  // Mips64r2 instruction.
      double rounded = trunc(fs);
      int64_t result = static_cast<int64_t>(rounded);
      set_fpu_register(fd_reg(), result);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case FLOOR_L_D: {  // Mips64r2 instruction.
      double rounded = floor(fs);
      int64_t result = static_cast<int64_t>(rounded);
      set_fpu_register(fd_reg(), result);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case CEIL_L_D: {  // Mips64r2 instruction.
      double rounded = ceil(fs);
      int64_t result = static_cast<int64_t>(rounded);
      set_fpu_register(fd_reg(), result);
      if (set_fcsr_round64_error(fs, rounded)) {
        set_fpu_register_invalid_result64(fs, rounded);
      }
      break;
    }
    case CLASS_D: {  // Mips64r6 instruction
      // Convert double input to uint64_t for easier bit manipulation
      uint64_t classed = bit_cast<uint64_t>(fs);

      // Extracting sign, exponent and mantissa from the input double
      uint32_t sign = (classed >> 63) & 1;
      uint32_t exponent = (classed >> 52) & 0x00000000000007ff;
      uint64_t mantissa = classed & 0x000fffffffffffff;
      uint64_t result;
      double dResult;

      // Setting flags if input double is negative infinity,
      // positive infinity, negative zero or positive zero
      bool negInf = (classed == 0xFFF0000000000000);
      bool posInf = (classed == 0x7FF0000000000000);
      bool negZero = (classed == 0x8000000000000000);
      bool posZero = (classed == 0x0000000000000000);

      bool signalingNan;
      bool quietNan;
      bool negSubnorm;
      bool posSubnorm;
      bool negNorm;
      bool posNorm;

      // Setting flags if double is NaN
      signalingNan = false;
      quietNan = false;
      if (!negInf && !posInf && exponent == 0x7ff) {
        quietNan = ((mantissa & 0x0008000000000000) != 0) &&
                   ((mantissa & (0x0008000000000000 - 1)) == 0);
        signalingNan = !quietNan;
      }

      // Setting flags if double is subnormal number
      posSubnorm = false;
      negSubnorm = false;
      if ((exponent == 0) && (mantissa != 0)) {
        DCHECK(sign == 0 || sign == 1);
        posSubnorm = (sign == 0);
        negSubnorm = (sign == 1);
      }

      // Setting flags if double is normal number
      posNorm = false;
      negNorm = false;
      if (!posSubnorm && !negSubnorm && !posInf && !negInf && !signalingNan &&
          !quietNan && !negZero && !posZero) {
        DCHECK(sign == 0 || sign == 1);
        posNorm = (sign == 0);
        negNorm = (sign == 1);
      }

      // Calculating result according to description of CLASS.D instruction
      result = (posZero << 9) | (posSubnorm << 8) | (posNorm << 7) |
               (posInf << 6) | (negZero << 5) | (negSubnorm << 4) |
               (negNorm << 3) | (negInf << 2) | (quietNan << 1) | signalingNan;

      DCHECK(result != 0);

      dResult = bit_cast<double>(result);
      set_fpu_register_double(fd_reg(), dResult);

      break;
    }
    case C_F_D: {
      set_fcsr_bit(fcsr_cc, false);
      break;
    }
    default:
      UNREACHABLE();
  }
}


void Simulator::DecodeTypeRegisterWRsType() {
  float fs = get_fpu_register_float(fs_reg());
  float ft = get_fpu_register_float(ft_reg());
  int64_t alu_out = 0x12345678;
  switch (instr_.FunctionFieldRaw()) {
    case CVT_S_W:  // Convert word to float (single).
      alu_out = get_fpu_register_signed_word(fs_reg());
      set_fpu_register_float(fd_reg(), static_cast<float>(alu_out));
      break;
    case CVT_D_W:  // Convert word to double.
      alu_out = get_fpu_register_signed_word(fs_reg());
      set_fpu_register_double(fd_reg(), static_cast<double>(alu_out));
      break;
    case CMP_AF:
      set_fpu_register_word(fd_reg(), 0);
      break;
    case CMP_UN:
      if (std::isnan(fs) || std::isnan(ft)) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_EQ:
      if (fs == ft) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_UEQ:
      if ((fs == ft) || (std::isnan(fs) || std::isnan(ft))) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_LT:
      if (fs < ft) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_ULT:
      if ((fs < ft) || (std::isnan(fs) || std::isnan(ft))) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_LE:
      if (fs <= ft) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_ULE:
      if ((fs <= ft) || (std::isnan(fs) || std::isnan(ft))) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_OR:
      if (!std::isnan(fs) && !std::isnan(ft)) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_UNE:
      if ((fs != ft) || (std::isnan(fs) || std::isnan(ft))) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    case CMP_NE:
      if (fs != ft) {
        set_fpu_register_word(fd_reg(), -1);
      } else {
        set_fpu_register_word(fd_reg(), 0);
      }
      break;
    default:
      UNREACHABLE();
  }
}


void Simulator::DecodeTypeRegisterLRsType() {
  double fs = get_fpu_register_double(fs_reg());
  double ft = get_fpu_register_double(ft_reg());
  int64_t i64;
  switch (instr_.FunctionFieldRaw()) {
    case CVT_D_L:  // Mips32r2 instruction.
      i64 = get_fpu_register(fs_reg());
      set_fpu_register_double(fd_reg(), static_cast<double>(i64));
      break;
    case CVT_S_L:
      i64 = get_fpu_register(fs_reg());
      set_fpu_register_float(fd_reg(), static_cast<float>(i64));
      break;
    case CMP_AF:
      set_fpu_register(fd_reg(), 0);
      break;
    case CMP_UN:
      if (std::isnan(fs) || std::isnan(ft)) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_EQ:
      if (fs == ft) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_UEQ:
      if ((fs == ft) || (std::isnan(fs) || std::isnan(ft))) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_LT:
      if (fs < ft) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_ULT:
      if ((fs < ft) || (std::isnan(fs) || std::isnan(ft))) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_LE:
      if (fs <= ft) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_ULE:
      if ((fs <= ft) || (std::isnan(fs) || std::isnan(ft))) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_OR:
      if (!std::isnan(fs) && !std::isnan(ft)) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_UNE:
      if ((fs != ft) || (std::isnan(fs) || std::isnan(ft))) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    case CMP_NE:
      if (fs != ft && (!std::isnan(fs) && !std::isnan(ft))) {
        set_fpu_register(fd_reg(), -1);
      } else {
        set_fpu_register(fd_reg(), 0);
      }
      break;
    default:
      UNREACHABLE();
  }
}


void Simulator::DecodeTypeRegisterCOP1() {
  switch (instr_.RsFieldRaw()) {
    case BC1:  // Branch on coprocessor condition.
    case BC1EQZ:
    case BC1NEZ:
      UNREACHABLE();
      break;
    case CFC1:
      // At the moment only FCSR is supported.
      DCHECK(fs_reg() == kFCSRRegister);
      set_register(rt_reg(), FCSR_);
      break;
    case MFC1:
      set_register(rt_reg(),
                   static_cast<int64_t>(get_fpu_register_word(fs_reg())));
      break;
    case DMFC1:
      set_register(rt_reg(), get_fpu_register(fs_reg()));
      break;
    case MFHC1:
      set_register(rt_reg(), get_fpu_register_hi_word(fs_reg()));
      break;
    case CTC1: {
      // At the moment only FCSR is supported.
      DCHECK(fs_reg() == kFCSRRegister);
      uint32_t reg = static_cast<uint32_t>(rt());
      if (kArchVariant == kMips64r6) {
        FCSR_ = reg | kFCSRNaN2008FlagMask;
      } else {
        DCHECK(kArchVariant == kMips64r2);
        FCSR_ = reg & ~kFCSRNaN2008FlagMask;
      }
      break;
    }
    case MTC1:
      // Hardware writes upper 32-bits to zero on mtc1.
      set_fpu_register_hi_word(fs_reg(), 0);
      set_fpu_register_word(fs_reg(), static_cast<int32_t>(rt()));
      break;
    case DMTC1:
      set_fpu_register(fs_reg(), rt());
      break;
    case MTHC1:
      set_fpu_register_hi_word(fs_reg(), static_cast<int32_t>(rt()));
      break;
    case S:
      DecodeTypeRegisterSRsType();
      break;
    case D:
      DecodeTypeRegisterDRsType();
      break;
    case W:
      DecodeTypeRegisterWRsType();
      break;
    case L:
      DecodeTypeRegisterLRsType();
      break;
    default:
      UNREACHABLE();
  }
}


void Simulator::DecodeTypeRegisterCOP1X() {
  switch (instr_.FunctionFieldRaw()) {
    case MADD_S: {
      DCHECK(kArchVariant == kMips64r2);
      float fr, ft, fs;
      fr = get_fpu_register_float(fr_reg());
      fs = get_fpu_register_float(fs_reg());
      ft = get_fpu_register_float(ft_reg());
      set_fpu_register_float(fd_reg(), fs * ft + fr);
      break;
    }
    case MSUB_S: {
      DCHECK(kArchVariant == kMips64r2);
      float fr, ft, fs;
      fr = get_fpu_register_float(fr_reg());
      fs = get_fpu_register_float(fs_reg());
      ft = get_fpu_register_float(ft_reg());
      set_fpu_register_float(fd_reg(), fs * ft - fr);
      break;
    }
    case MADD_D: {
      DCHECK(kArchVariant == kMips64r2);
      double fr, ft, fs;
      fr = get_fpu_register_double(fr_reg());
      fs = get_fpu_register_double(fs_reg());
      ft = get_fpu_register_double(ft_reg());
      set_fpu_register_double(fd_reg(), fs * ft + fr);
      break;
    }
    case MSUB_D: {
      DCHECK(kArchVariant == kMips64r2);
      double fr, ft, fs;
      fr = get_fpu_register_double(fr_reg());
      fs = get_fpu_register_double(fs_reg());
      ft = get_fpu_register_double(ft_reg());
      set_fpu_register_double(fd_reg(), fs * ft - fr);
      break;
    }
    default:
      UNREACHABLE();
  }
}


void Simulator::DecodeTypeRegisterSPECIAL() {
  int64_t i64hilo;
  uint64_t u64hilo;
  int64_t alu_out;
  bool do_interrupt = false;

  switch (instr_.FunctionFieldRaw()) {
    case SELEQZ_S:
      DCHECK(kArchVariant == kMips64r6);
      set_register(rd_reg(), rt() == 0 ? rs() : 0);
      break;
    case SELNEZ_S:
      DCHECK(kArchVariant == kMips64r6);
      set_register(rd_reg(), rt() != 0 ? rs() : 0);
      break;
    case JR: {
      int64_t next_pc = rs();
      int64_t current_pc = get_pc();
      Instruction* branch_delay_instr =
          reinterpret_cast<Instruction*>(current_pc + Instruction::kInstrSize);
      BranchDelayInstructionDecode(branch_delay_instr);
      set_pc(next_pc);
      pc_modified_ = true;
      break;
    }
    case JALR: {
      int64_t next_pc = rs();
      int64_t current_pc = get_pc();
      int32_t return_addr_reg = rd_reg();
      Instruction* branch_delay_instr =
          reinterpret_cast<Instruction*>(current_pc + Instruction::kInstrSize);
      BranchDelayInstructionDecode(branch_delay_instr);
      set_register(return_addr_reg, current_pc + 2 * Instruction::kInstrSize);
      set_pc(next_pc);
      pc_modified_ = true;
      break;
    }
    case SLL:
      SetResult(rd_reg(), static_cast<int32_t>(rt()) << sa());
      break;
    case DSLL:
      SetResult(rd_reg(), rt() << sa());
      break;
    case DSLL32:
      SetResult(rd_reg(), rt() << sa() << 32);
      break;
    case SRL:
      if (rs_reg() == 0) {
        // Regular logical right shift of a word by a fixed number of
        // bits instruction. RS field is always equal to 0.
        // Sign-extend the 32-bit result.
        alu_out = static_cast<int32_t>(static_cast<uint32_t>(rt_u()) >> sa());
      } else if (rs_reg() == 1) {
        // Logical right-rotate of a word by a fixed number of bits. This
        // is special case of SRL instruction, added in MIPS32 Release 2.
        // RS field is equal to 00001.
        alu_out = static_cast<int32_t>(
            base::bits::RotateRight32(static_cast<const uint32_t>(rt_u()),
                                      static_cast<const uint32_t>(sa())));
      } else {
        UNREACHABLE();
      }
      SetResult(rd_reg(), alu_out);
      break;
    case DSRL:
      if (rs_reg() == 0) {
        // Regular logical right shift of a word by a fixed number of
        // bits instruction. RS field is always equal to 0.
        // Sign-extend the 64-bit result.
        alu_out = static_cast<int64_t>(rt_u() >> sa());
      } else if (rs_reg() == 1) {
        // Logical right-rotate of a word by a fixed number of bits. This
        // is special case of SRL instruction, added in MIPS32 Release 2.
        // RS field is equal to 00001.
        alu_out = static_cast<int64_t>(base::bits::RotateRight64(rt_u(), sa()));
      } else {
        UNREACHABLE();
      }
      SetResult(rd_reg(), alu_out);
      break;
    case DSRL32:
      if (rs_reg() == 0) {
        // Regular logical right shift of a word by a fixed number of
        // bits instruction. RS field is always equal to 0.
        // Sign-extend the 64-bit result.
        alu_out = static_cast<int64_t>(rt_u() >> sa() >> 32);
      } else if (rs_reg() == 1) {
        // Logical right-rotate of a word by a fixed number of bits. This
        // is special case of SRL instruction, added in MIPS32 Release 2.
        // RS field is equal to 00001.
        alu_out =
            static_cast<int64_t>(base::bits::RotateRight64(rt_u(), sa() + 32));
      } else {
        UNREACHABLE();
      }
      SetResult(rd_reg(), alu_out);
      break;
    case SRA:
      SetResult(rd_reg(), (int32_t)rt() >> sa());
      break;
    case DSRA:
      SetResult(rd_reg(), rt() >> sa());
      break;
    case DSRA32:
      SetResult(rd_reg(), rt() >> sa() >> 32);
      break;
    case SLLV:
      SetResult(rd_reg(), (int32_t)rt() << rs());
      break;
    case DSLLV:
      SetResult(rd_reg(), rt() << rs());
      break;
    case SRLV:
      if (sa() == 0) {
        // Regular logical right-shift of a word by a variable number of
        // bits instruction. SA field is always equal to 0.
        alu_out = static_cast<int32_t>((uint32_t)rt_u() >> rs());
      } else {
        // Logical right-rotate of a word by a variable number of bits.
        // This is special case od SRLV instruction, added in MIPS32
        // Release 2. SA field is equal to 00001.
        alu_out = static_cast<int32_t>(
            base::bits::RotateRight32(static_cast<const uint32_t>(rt_u()),
                                      static_cast<const uint32_t>(rs_u())));
      }
      SetResult(rd_reg(), alu_out);
      break;
    case DSRLV:
      if (sa() == 0) {
        // Regular logical right-shift of a word by a variable number of
        // bits instruction. SA field is always equal to 0.
        alu_out = static_cast<int64_t>(rt_u() >> rs());
      } else {
        // Logical right-rotate of a word by a variable number of bits.
        // This is special case od SRLV instruction, added in MIPS32
        // Release 2. SA field is equal to 00001.
        alu_out =
            static_cast<int64_t>(base::bits::RotateRight64(rt_u(), rs_u()));
      }
      SetResult(rd_reg(), alu_out);
      break;
    case SRAV:
      SetResult(rd_reg(), (int32_t)rt() >> rs());
      break;
    case DSRAV:
      SetResult(rd_reg(), rt() >> rs());
      break;
    case LSA: {
      DCHECK(kArchVariant == kMips64r6);
      int8_t sa = lsa_sa() + 1;
      int32_t _rt = static_cast<int32_t>(rt());
      int32_t _rs = static_cast<int32_t>(rs());
      int32_t res = _rs << sa;
      res += _rt;
      SetResult(rd_reg(), static_cast<int64_t>(res));
      break;
    }
    case DLSA:
      DCHECK(kArchVariant == kMips64r6);
      SetResult(rd_reg(), (rs() << (lsa_sa() + 1)) + rt());
      break;
    case MFHI:  // MFHI == CLZ on R6.
      if (kArchVariant != kMips64r6) {
        DCHECK(sa() == 0);
        alu_out = get_register(HI);
      } else {
        // MIPS spec: If no bits were set in GPR rs(), the result written to
        // GPR rd() is 32.
        DCHECK(sa() == 1);
        alu_out = base::bits::CountLeadingZeros32(static_cast<int32_t>(rs_u()));
      }
      SetResult(rd_reg(), alu_out);
      break;
    case MFLO:  // MFLO == DCLZ on R6.
      if (kArchVariant != kMips64r6) {
        DCHECK(sa() == 0);
        alu_out = get_register(LO);
      } else {
        // MIPS spec: If no bits were set in GPR rs(), the result written to
        // GPR rd() is 64.
        DCHECK(sa() == 1);
        alu_out = base::bits::CountLeadingZeros64(static_cast<int64_t>(rs_u()));
      }
      SetResult(rd_reg(), alu_out);
      break;
    // Instructions using HI and LO registers.
    case MULT: {  // MULT == D_MUL_MUH.
      int32_t rs_lo = static_cast<int32_t>(rs());
      int32_t rt_lo = static_cast<int32_t>(rt());
      i64hilo = static_cast<int64_t>(rs_lo) * static_cast<int64_t>(rt_lo);
      if (kArchVariant != kMips64r6) {
        set_register(LO, static_cast<int32_t>(i64hilo & 0xffffffff));
        set_register(HI, static_cast<int32_t>(i64hilo >> 32));
      } else {
        switch (sa()) {
          case MUL_OP:
            set_register(rd_reg(), static_cast<int32_t>(i64hilo & 0xffffffff));
            break;
          case MUH_OP:
            set_register(rd_reg(), static_cast<int32_t>(i64hilo >> 32));
            break;
          default:
            UNIMPLEMENTED_MIPS();
            break;
        }
      }
      break;
    }
    case MULTU:
      u64hilo = static_cast<uint64_t>(rs_u() & 0xffffffff) *
                static_cast<uint64_t>(rt_u() & 0xffffffff);
      if (kArchVariant != kMips64r6) {
        set_register(LO, static_cast<int32_t>(u64hilo & 0xffffffff));
        set_register(HI, static_cast<int32_t>(u64hilo >> 32));
      } else {
        switch (sa()) {
          case MUL_OP:
            set_register(rd_reg(), static_cast<int32_t>(u64hilo & 0xffffffff));
            break;
          case MUH_OP:
            set_register(rd_reg(), static_cast<int32_t>(u64hilo >> 32));
            break;
          default:
            UNIMPLEMENTED_MIPS();
            break;
        }
      }
      break;
    case DMULT:  // DMULT == D_MUL_MUH.
      if (kArchVariant != kMips64r6) {
        set_register(LO, rs() * rt());
        set_register(HI, MultiplyHighSigned(rs(), rt()));
      } else {
        switch (sa()) {
          case MUL_OP:
            set_register(rd_reg(), rs() * rt());
            break;
          case MUH_OP:
            set_register(rd_reg(), MultiplyHighSigned(rs(), rt()));
            break;
          default:
            UNIMPLEMENTED_MIPS();
            break;
        }
      }
      break;
    case DMULTU:
      UNIMPLEMENTED_MIPS();
      break;
    case DIV:
    case DDIV: {
      const int64_t int_min_value =
          instr_.FunctionFieldRaw() == DIV ? INT_MIN : LONG_MIN;
      switch (kArchVariant) {
        case kMips64r2:
          // Divide by zero and overflow was not checked in the
          // configuration step - div and divu do not raise exceptions. On
          // division by 0 the result will be UNPREDICTABLE. On overflow
          // (INT_MIN/-1), return INT_MIN which is what the hardware does.
          if (rs() == int_min_value && rt() == -1) {
            set_register(LO, int_min_value);
            set_register(HI, 0);
          } else if (rt() != 0) {
            set_register(LO, rs() / rt());
            set_register(HI, rs() % rt());
          }
          break;
        case kMips64r6:
          switch (sa()) {
            case DIV_OP:
              if (rs() == int_min_value && rt() == -1) {
                set_register(rd_reg(), int_min_value);
              } else if (rt() != 0) {
                set_register(rd_reg(), rs() / rt());
              }
              break;
            case MOD_OP:
              if (rs() == int_min_value && rt() == -1) {
                set_register(rd_reg(), 0);
              } else if (rt() != 0) {
                set_register(rd_reg(), rs() % rt());
              }
              break;
            default:
              UNIMPLEMENTED_MIPS();
              break;
          }
          break;
        default:
          break;
      }
      break;
    }
    case DIVU:
      switch (kArchVariant) {
        case kMips64r6: {
          uint32_t rt_u_32 = static_cast<uint32_t>(rt_u());
          uint32_t rs_u_32 = static_cast<uint32_t>(rs_u());
          switch (sa()) {
            case DIV_OP:
              if (rt_u_32 != 0) {
                set_register(rd_reg(), rs_u_32 / rt_u_32);
              }
              break;
            case MOD_OP:
              if (rt_u() != 0) {
                set_register(rd_reg(), rs_u_32 % rt_u_32);
              }
              break;
            default:
              UNIMPLEMENTED_MIPS();
              break;
          }
        } break;
        default: {
          if (rt_u() != 0) {
            uint32_t rt_u_32 = static_cast<uint32_t>(rt_u());
            uint32_t rs_u_32 = static_cast<uint32_t>(rs_u());
            set_register(LO, rs_u_32 / rt_u_32);
            set_register(HI, rs_u_32 % rt_u_32);
          }
        }
      }
      break;
    case DDIVU:
      switch (kArchVariant) {
        case kMips64r6: {
          switch (instr_.SaValue()) {
            case DIV_OP:
              if (rt_u() != 0) {
                set_register(rd_reg(), rs_u() / rt_u());
              }
              break;
            case MOD_OP:
              if (rt_u() != 0) {
                set_register(rd_reg(), rs_u() % rt_u());
              }
              break;
            default:
              UNIMPLEMENTED_MIPS();
              break;
          }
        } break;
        default: {
          if (rt_u() != 0) {
            set_register(LO, rs_u() / rt_u());
            set_register(HI, rs_u() % rt_u());
          }
        }
      }
      break;
    case ADD:
    case DADD:
      if (HaveSameSign(rs(), rt())) {
        if (rs() > 0) {
          if (rs() > (Registers::kMaxValue - rt())) {
            SignalException(kIntegerOverflow);
          }
        } else if (rs() < 0) {
          if (rs() < (Registers::kMinValue - rt())) {
            SignalException(kIntegerUnderflow);
          }
        }
      }
      SetResult(rd_reg(), rs() + rt());
      break;
    case ADDU: {
      int32_t alu32_out = static_cast<int32_t>(rs() + rt());
      // Sign-extend result of 32bit operation into 64bit register.
      SetResult(rd_reg(), static_cast<int64_t>(alu32_out));
      break;
    }
    case DADDU:
      SetResult(rd_reg(), rs() + rt());
      break;
    case SUB:
    case DSUB:
      if (!HaveSameSign(rs(), rt())) {
        if (rs() > 0) {
          if (rs() > (Registers::kMaxValue + rt())) {
            SignalException(kIntegerOverflow);
          }
        } else if (rs() < 0) {
          if (rs() < (Registers::kMinValue + rt())) {
            SignalException(kIntegerUnderflow);
          }
        }
      }
      SetResult(rd_reg(), rs() - rt());
      break;
    case SUBU: {
      int32_t alu32_out = static_cast<int32_t>(rs() - rt());
      // Sign-extend result of 32bit operation into 64bit register.
      SetResult(rd_reg(), static_cast<int64_t>(alu32_out));
      break;
    }
    case DSUBU:
      SetResult(rd_reg(), rs() - rt());
      break;
    case AND:
      SetResult(rd_reg(), rs() & rt());
      break;
    case OR:
      SetResult(rd_reg(), rs() | rt());
      break;
    case XOR:
      SetResult(rd_reg(), rs() ^ rt());
      break;
    case NOR:
      SetResult(rd_reg(), ~(rs() | rt()));
      break;
    case SLT:
      SetResult(rd_reg(), rs() < rt() ? 1 : 0);
      break;
    case SLTU:
      SetResult(rd_reg(), rs_u() < rt_u() ? 1 : 0);
      break;
    // Break and trap instructions.
    case BREAK:
      do_interrupt = true;
      break;
    case TGE:
      do_interrupt = rs() >= rt();
      break;
    case TGEU:
      do_interrupt = rs_u() >= rt_u();
      break;
    case TLT:
      do_interrupt = rs() < rt();
      break;
    case TLTU:
      do_interrupt = rs_u() < rt_u();
      break;
    case TEQ:
      do_interrupt = rs() == rt();
      break;
    case TNE:
      do_interrupt = rs() != rt();
      break;
    case SYNC:
      // TODO(palfia): Ignore sync instruction for now.
      break;
    // Conditional moves.
    case MOVN:
      if (rt()) {
        SetResult(rd_reg(), rs());
      }
      break;
    case MOVCI: {
      uint32_t cc = instr_.FBccValue();
      uint32_t fcsr_cc = get_fcsr_condition_bit(cc);
      if (instr_.Bit(16)) {  // Read Tf bit.
        if (test_fcsr_bit(fcsr_cc)) set_register(rd_reg(), rs());
      } else {
        if (!test_fcsr_bit(fcsr_cc)) set_register(rd_reg(), rs());
      }
      break;
    }
    case MOVZ:
      if (!rt()) {
        SetResult(rd_reg(), rs());
      }
      break;
    default:
      UNREACHABLE();
  }
  if (do_interrupt) {
    SoftwareInterrupt();
  }
}


void Simulator::DecodeTypeRegisterSPECIAL2() {
  int64_t alu_out;
  switch (instr_.FunctionFieldRaw()) {
    case MUL:
      alu_out = static_cast<int32_t>(rs_u()) * static_cast<int32_t>(rt_u());
      SetResult(rd_reg(), alu_out);
      // HI and LO are UNPREDICTABLE after the operation.
      set_register(LO, Unpredictable);
      set_register(HI, Unpredictable);
      break;
    case CLZ:
      // MIPS32 spec: If no bits were set in GPR rs(), the result written to
      // GPR rd is 32.
      alu_out = base::bits::CountLeadingZeros32(static_cast<uint32_t>(rs_u()));
      SetResult(rd_reg(), alu_out);
      break;
    case DCLZ:
      // MIPS64 spec: If no bits were set in GPR rs(), the result written to
      // GPR rd is 64.
      alu_out = base::bits::CountLeadingZeros64(static_cast<uint64_t>(rs_u()));
      SetResult(rd_reg(), alu_out);
      break;
    default:
      alu_out = 0x12345678;
      UNREACHABLE();
  }
}


void Simulator::DecodeTypeRegisterSPECIAL3() {
  int64_t alu_out;
  switch (instr_.FunctionFieldRaw()) {
    case INS: {  // Mips64r2 instruction.
      // Interpret rd field as 5-bit msb of insert.
      uint16_t msb = rd_reg();
      // Interpret sa field as 5-bit lsb of insert.
      uint16_t lsb = sa();
      uint16_t size = msb - lsb + 1;
      uint64_t mask = (1ULL << size) - 1;
      alu_out = static_cast<int32_t>((rt_u() & ~(mask << lsb)) |
                                     ((rs_u() & mask) << lsb));
      SetResult(rt_reg(), alu_out);
      break;
    }
    case DINS: {  // Mips64r2 instruction.
      // Interpret rd field as 5-bit msb of insert.
      uint16_t msb = rd_reg();
      // Interpret sa field as 5-bit lsb of insert.
      uint16_t lsb = sa();
      uint16_t size = msb - lsb + 1;
      uint64_t mask = (1ULL << size) - 1;
      alu_out = (rt_u() & ~(mask << lsb)) | ((rs_u() & mask) << lsb);
      SetResult(rt_reg(), alu_out);
      break;
    }
    case EXT: {  // Mips64r2 instruction.
      // Interpret rd field as 5-bit msb of extract.
      uint16_t msb = rd_reg();
      // Interpret sa field as 5-bit lsb of extract.
      uint16_t lsb = sa();
      uint16_t size = msb + 1;
      uint64_t mask = (1ULL << size) - 1;
      alu_out = static_cast<int32_t>((rs_u() & (mask << lsb)) >> lsb);
      SetResult(rt_reg(), alu_out);
      break;
    }
    case DEXT: {  // Mips64r2 instruction.
      // Interpret rd field as 5-bit msb of extract.
      uint16_t msb = rd_reg();
      // Interpret sa field as 5-bit lsb of extract.
      uint16_t lsb = sa();
      uint16_t size = msb + 1;
      uint64_t mask = (1ULL << size) - 1;
      alu_out = static_cast<int64_t>((rs_u() & (mask << lsb)) >> lsb);
      SetResult(rt_reg(), alu_out);
      break;
    }
    case DEXTM: {
      // Interpret rd field as 5-bit msb of extract.
      uint16_t msb = rd_reg();
      // Interpret sa field as 5-bit lsb of extract.
      uint16_t lsb = sa();
      uint16_t size = msb + 33;
      uint64_t mask = (1ULL << size) - 1;
      alu_out = static_cast<int64_t>((rs_u() & (mask << lsb)) >> lsb);
      SetResult(rt_reg(), alu_out);
      break;
    }
    case DEXTU: {
      // Interpret rd field as 5-bit msb of extract.
      uint16_t msb = rd_reg();
      // Interpret sa field as 5-bit lsb of extract.
      uint16_t lsb = sa() + 32;
      uint16_t size = msb + 1;
      uint64_t mask = (1ULL << size) - 1;
      alu_out = static_cast<int64_t>((rs_u() & (mask << lsb)) >> lsb);
      SetResult(rt_reg(), alu_out);
      break;
    }
    case BSHFL: {
      int32_t sa = instr_.SaFieldRaw() >> kSaShift;
      switch (sa) {
        case BITSWAP: {
          uint32_t input = static_cast<uint32_t>(rt());
          uint32_t output = 0;
          uint8_t i_byte, o_byte;

          // Reverse the bit in byte for each individual byte
          for (int i = 0; i < 4; i++) {
            output = output >> 8;
            i_byte = input & 0xff;

            // Fast way to reverse bits in byte
            // Devised by Sean Anderson, July 13, 2001
            o_byte = static_cast<uint8_t>(((i_byte * 0x0802LU & 0x22110LU) |
                                           (i_byte * 0x8020LU & 0x88440LU)) *
                                              0x10101LU >>
                                          16);

            output = output | (static_cast<uint32_t>(o_byte << 24));
            input = input >> 8;
          }

          alu_out = static_cast<int64_t>(static_cast<int32_t>(output));
          break;
        }
        case SEB: {
          uint8_t input = static_cast<uint8_t>(rt());
          uint32_t output = input;
          uint32_t mask = 0x00000080;

          // Extending sign
          if (mask & input) {
            output |= 0xFFFFFF00;
          }

          alu_out = static_cast<int32_t>(output);
          break;
        }
        case SEH: {
          uint16_t input = static_cast<uint16_t>(rt());
          uint32_t output = input;
          uint32_t mask = 0x00008000;

          // Extending sign
          if (mask & input) {
            output |= 0xFFFF0000;
          }

          alu_out = static_cast<int32_t>(output);
          break;
        }
        case WSBH: {
          uint32_t input = static_cast<uint32_t>(rt());
          uint64_t output = 0;

          uint32_t mask = 0xFF000000;
          for (int i = 0; i < 4; i++) {
            uint32_t tmp = mask & input;
            if (i % 2 == 0) {
              tmp = tmp >> 8;
            } else {
              tmp = tmp << 8;
            }
            output = output | tmp;
            mask = mask >> 8;
          }
          mask = 0x80000000;

          // Extending sign
          if (mask & output) {
            output |= 0xFFFFFFFF00000000;
          }

          alu_out = static_cast<int64_t>(output);
          break;
        }
        default: {
          const uint8_t bp2 = instr_.Bp2Value();
          sa >>= kBp2Bits;
          switch (sa) {
            case ALIGN: {
              if (bp2 == 0) {
                alu_out = static_cast<int32_t>(rt());
              } else {
                uint64_t rt_hi = rt() << (8 * bp2);
                uint64_t rs_lo = rs() >> (8 * (4 - bp2));
                alu_out = static_cast<int32_t>(rt_hi | rs_lo);
              }
              break;
            }
            default:
              alu_out = 0x12345678;
              UNREACHABLE();
              break;
          }
          break;
        }
      }
      SetResult(rd_reg(), alu_out);
      break;
    }
    case DBSHFL: {
      int32_t sa = instr_.SaFieldRaw() >> kSaShift;
      switch (sa) {
        case DBITSWAP: {
          switch (sa) {
            case DBITSWAP_SA: {  // Mips64r6
              uint64_t input = static_cast<uint64_t>(rt());
              uint64_t output = 0;
              uint8_t i_byte, o_byte;

              // Reverse the bit in byte for each individual byte
              for (int i = 0; i < 8; i++) {
                output = output >> 8;
                i_byte = input & 0xff;

                // Fast way to reverse bits in byte
                // Devised by Sean Anderson, July 13, 2001
                o_byte =
                    static_cast<uint8_t>(((i_byte * 0x0802LU & 0x22110LU) |
                                          (i_byte * 0x8020LU & 0x88440LU)) *
                                             0x10101LU >>
                                         16);

                output = output | ((static_cast<uint64_t>(o_byte) << 56));
                input = input >> 8;
              }

              alu_out = static_cast<int64_t>(output);
              break;
            }
          }
          break;
        }
        case DSBH: {
          uint64_t input = static_cast<uint64_t>(rt());
          uint64_t output = 0;

          uint64_t mask = 0xFF00000000000000;
          for (int i = 0; i < 8; i++) {
            uint64_t tmp = mask & input;
            if (i % 2 == 0)
              tmp = tmp >> 8;
            else
              tmp = tmp << 8;

            output = output | tmp;
            mask = mask >> 8;
          }

          alu_out = static_cast<int64_t>(output);
          break;
        }
        case DSHD: {
          uint64_t input = static_cast<uint64_t>(rt());
          uint64_t output = 0;

          uint64_t mask = 0xFFFF000000000000;
          for (int i = 0; i < 4; i++) {
            uint64_t tmp = mask & input;
            if (i == 0)
              tmp = tmp >> 48;
            else if (i == 1)
              tmp = tmp >> 16;
            else if (i == 2)
              tmp = tmp << 16;
            else
              tmp = tmp << 48;
            output = output | tmp;
            mask = mask >> 16;
          }

          alu_out = static_cast<int64_t>(output);
          break;
        }
        default: {
          const uint8_t bp3 = instr_.Bp3Value();
          sa >>= kBp3Bits;
          switch (sa) {
            case DALIGN: {
              if (bp3 == 0) {
                alu_out = static_cast<int64_t>(rt());
              } else {
                uint64_t rt_hi = rt() << (8 * bp3);
                uint64_t rs_lo = rs() >> (8 * (8 - bp3));
                alu_out = static_cast<int64_t>(rt_hi | rs_lo);
              }
              break;
            }
            default:
              alu_out = 0x12345678;
              UNREACHABLE();
              break;
          }
          break;
        }
      }
      SetResult(rd_reg(), alu_out);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void Simulator::DecodeTypeRegister() {
  // ---------- Execution.
  switch (instr_.OpcodeFieldRaw()) {
    case COP1:
      DecodeTypeRegisterCOP1();
      break;
    case COP1X:
      DecodeTypeRegisterCOP1X();
      break;
    case SPECIAL:
      DecodeTypeRegisterSPECIAL();
      break;
    case SPECIAL2:
      DecodeTypeRegisterSPECIAL2();
      break;
    case SPECIAL3:
      DecodeTypeRegisterSPECIAL3();
      break;
    // Unimplemented opcodes raised an error in the configuration step before,
    // so we can use the default here to set the destination register in common
    // cases.
    default:
      UNREACHABLE();
  }
}


// Type 2: instructions using a 16, 21 or 26 bits immediate. (e.g. beq, beqc).
void Simulator::DecodeTypeImmediate() {
  // Instruction fields.
  Opcode op = instr_.OpcodeFieldRaw();
  int32_t rs_reg = instr_.RsValue();
  int64_t rs = get_register(instr_.RsValue());
  uint64_t rs_u = static_cast<uint64_t>(rs);
  int32_t rt_reg = instr_.RtValue();  // Destination register.
  int64_t rt = get_register(rt_reg);
  int16_t imm16 = instr_.Imm16Value();
  int32_t imm18 = instr_.Imm18Value();

  int32_t ft_reg = instr_.FtValue();  // Destination register.

  // Zero extended immediate.
  uint64_t oe_imm16 = 0xffff & imm16;
  // Sign extended immediate.
  int64_t se_imm16 = imm16;
  int64_t se_imm18 = imm18 | ((imm18 & 0x20000) ? 0xfffffffffffc0000 : 0);

  // Next pc.
  int64_t next_pc = bad_ra;

  // Used for conditional branch instructions.
  bool execute_branch_delay_instruction = false;

  // Used for arithmetic instructions.
  int64_t alu_out = 0;

  // Used for memory instructions.
  int64_t addr = 0x0;
  // Alignment for 32-bit integers used in LWL, LWR, etc.
  const int kInt32AlignmentMask = sizeof(uint32_t) - 1;
  // Alignment for 64-bit integers used in LDL, LDR, etc.
  const int kInt64AlignmentMask = sizeof(uint64_t) - 1;

  // Branch instructions common part.
  auto BranchAndLinkHelper =
      [this, &next_pc, &execute_branch_delay_instruction](bool do_branch) {
        execute_branch_delay_instruction = true;
        int64_t current_pc = get_pc();
        if (do_branch) {
          int16_t imm16 = instr_.Imm16Value();
          next_pc = current_pc + (imm16 << 2) + Instruction::kInstrSize;
          set_register(31, current_pc + 2 * Instruction::kInstrSize);
        } else {
          next_pc = current_pc + 2 * Instruction::kInstrSize;
        }
      };

  auto BranchHelper = [this, &next_pc,
                       &execute_branch_delay_instruction](bool do_branch) {
    execute_branch_delay_instruction = true;
    int64_t current_pc = get_pc();
    if (do_branch) {
      int16_t imm16 = instr_.Imm16Value();
      next_pc = current_pc + (imm16 << 2) + Instruction::kInstrSize;
    } else {
      next_pc = current_pc + 2 * Instruction::kInstrSize;
    }
  };

  auto BranchAndLinkCompactHelper = [this, &next_pc](bool do_branch, int bits) {
    int64_t current_pc = get_pc();
    CheckForbiddenSlot(current_pc);
    if (do_branch) {
      int32_t imm = instr_.ImmValue(bits);
      imm <<= 32 - bits;
      imm >>= 32 - bits;
      next_pc = current_pc + (imm << 2) + Instruction::kInstrSize;
      set_register(31, current_pc + Instruction::kInstrSize);
    }
  };

  auto BranchCompactHelper = [this, &next_pc](bool do_branch, int bits) {
    int64_t current_pc = get_pc();
    CheckForbiddenSlot(current_pc);
    if (do_branch) {
      int32_t imm = instr_.ImmValue(bits);
      imm <<= 32 - bits;
      imm >>= 32 - bits;
      next_pc = get_pc() + (imm << 2) + Instruction::kInstrSize;
    }
  };

  switch (op) {
    // ------------- COP1. Coprocessor instructions.
    case COP1:
      switch (instr_.RsFieldRaw()) {
        case BC1: {  // Branch on coprocessor condition.
          uint32_t cc = instr_.FBccValue();
          uint32_t fcsr_cc = get_fcsr_condition_bit(cc);
          uint32_t cc_value = test_fcsr_bit(fcsr_cc);
          bool do_branch = (instr_.FBtrueValue()) ? cc_value : !cc_value;
          BranchHelper(do_branch);
          break;
        }
        case BC1EQZ:
          BranchHelper(!(get_fpu_register(ft_reg) & 0x1));
          break;
        case BC1NEZ:
          BranchHelper(get_fpu_register(ft_reg) & 0x1);
          break;
        default:
          UNREACHABLE();
      }
      break;
    // ------------- REGIMM class.
    case REGIMM:
      switch (instr_.RtFieldRaw()) {
        case BLTZ:
          BranchHelper(rs < 0);
          break;
        case BGEZ:
          BranchHelper(rs >= 0);
          break;
        case BLTZAL:
          BranchAndLinkHelper(rs < 0);
          break;
        case BGEZAL:
          BranchAndLinkHelper(rs >= 0);
          break;
        case DAHI:
          SetResult(rs_reg, rs + (se_imm16 << 32));
          break;
        case DATI:
          SetResult(rs_reg, rs + (se_imm16 << 48));
          break;
        default:
          UNREACHABLE();
      }
      break;  // case REGIMM.
    // ------------- Branch instructions.
    // When comparing to zero, the encoding of rt field is always 0, so we don't
    // need to replace rt with zero.
    case BEQ:
      BranchHelper(rs == rt);
      break;
    case BNE:
      BranchHelper(rs != rt);
      break;
    case POP06:  // BLEZALC, BGEZALC, BGEUC, BLEZ (pre-r6)
      if (kArchVariant == kMips64r6) {
        if (rt_reg != 0) {
          if (rs_reg == 0) {  // BLEZALC
            BranchAndLinkCompactHelper(rt <= 0, 16);
          } else {
            if (rs_reg == rt_reg) {  // BGEZALC
              BranchAndLinkCompactHelper(rt >= 0, 16);
            } else {  // BGEUC
              BranchCompactHelper(
                  static_cast<uint64_t>(rs) >= static_cast<uint64_t>(rt), 16);
            }
          }
        } else {  // BLEZ
          BranchHelper(rs <= 0);
        }
      } else {  // BLEZ
        BranchHelper(rs <= 0);
      }
      break;
    case POP07:  // BGTZALC, BLTZALC, BLTUC, BGTZ (pre-r6)
      if (kArchVariant == kMips64r6) {
        if (rt_reg != 0) {
          if (rs_reg == 0) {  // BGTZALC
            BranchAndLinkCompactHelper(rt > 0, 16);
          } else {
            if (rt_reg == rs_reg) {  // BLTZALC
              BranchAndLinkCompactHelper(rt < 0, 16);
            } else {  // BLTUC
              BranchCompactHelper(
                  static_cast<uint64_t>(rs) < static_cast<uint64_t>(rt), 16);
            }
          }
        } else {  // BGTZ
          BranchHelper(rs > 0);
        }
      } else {  // BGTZ
        BranchHelper(rs > 0);
      }
      break;
    case POP26:  // BLEZC, BGEZC, BGEC/BLEC / BLEZL (pre-r6)
      if (kArchVariant == kMips64r6) {
        if (rt_reg != 0) {
          if (rs_reg == 0) {  // BLEZC
            BranchCompactHelper(rt <= 0, 16);
          } else {
            if (rs_reg == rt_reg) {  // BGEZC
              BranchCompactHelper(rt >= 0, 16);
            } else {  // BGEC/BLEC
              BranchCompactHelper(rs >= rt, 16);
            }
          }
        }
      } else {  // BLEZL
        BranchAndLinkHelper(rs <= 0);
      }
      break;
    case POP27:  // BGTZC, BLTZC, BLTC/BGTC / BGTZL (pre-r6)
      if (kArchVariant == kMips64r6) {
        if (rt_reg != 0) {
          if (rs_reg == 0) {  // BGTZC
            BranchCompactHelper(rt > 0, 16);
          } else {
            if (rs_reg == rt_reg) {  // BLTZC
              BranchCompactHelper(rt < 0, 16);
            } else {  // BLTC/BGTC
              BranchCompactHelper(rs < rt, 16);
            }
          }
        }
      } else {  // BGTZL
        BranchAndLinkHelper(rs > 0);
      }
      break;
    case POP66:           // BEQZC, JIC
      if (rs_reg != 0) {  // BEQZC
        BranchCompactHelper(rs == 0, 21);
      } else {  // JIC
        next_pc = rt + imm16;
      }
      break;
    case POP76:           // BNEZC, JIALC
      if (rs_reg != 0) {  // BNEZC
        BranchCompactHelper(rs != 0, 21);
      } else {  // JIALC
        int64_t current_pc = get_pc();
        set_register(31, current_pc + Instruction::kInstrSize);
        next_pc = rt + imm16;
      }
      break;
    case BC:
      BranchCompactHelper(true, 26);
      break;
    case BALC:
      BranchAndLinkCompactHelper(true, 26);
      break;
    case POP10:  // BOVC, BEQZALC, BEQC / ADDI (pre-r6)
      if (kArchVariant == kMips64r6) {
        if (rs_reg >= rt_reg) {  // BOVC
          bool condition = !is_int32(rs) || !is_int32(rt) || !is_int32(rs + rt);
          BranchCompactHelper(condition, 16);
        } else {
          if (rs_reg == 0) {  // BEQZALC
            BranchAndLinkCompactHelper(rt == 0, 16);
          } else {  // BEQC
            BranchCompactHelper(rt == rs, 16);
          }
        }
      } else {  // ADDI
        if (HaveSameSign(rs, se_imm16)) {
          if (rs > 0) {
            if (rs <= Registers::kMaxValue - se_imm16) {
              SignalException(kIntegerOverflow);
            }
          } else if (rs < 0) {
            if (rs >= Registers::kMinValue - se_imm16) {
              SignalException(kIntegerUnderflow);
            }
          }
        }
        SetResult(rt_reg, rs + se_imm16);
      }
      break;
    case POP30:  // BNVC, BNEZALC, BNEC / DADDI (pre-r6)
      if (kArchVariant == kMips64r6) {
        if (rs_reg >= rt_reg) {  // BNVC
          bool condition = is_int32(rs) && is_int32(rt) && is_int32(rs + rt);
          BranchCompactHelper(condition, 16);
        } else {
          if (rs_reg == 0) {  // BNEZALC
            BranchAndLinkCompactHelper(rt != 0, 16);
          } else {  // BNEC
            BranchCompactHelper(rt != rs, 16);
          }
        }
      }
      break;
    // ------------- Arithmetic instructions.
    case ADDIU: {
      DCHECK(is_int32(rs));
      int32_t alu32_out = static_cast<int32_t>(rs + se_imm16);
      // Sign-extend result of 32bit operation into 64bit register.
      SetResult(rt_reg, static_cast<int64_t>(alu32_out));
      break;
    }
    case DADDIU:
      SetResult(rt_reg, rs + se_imm16);
      break;
    case SLTI:
      SetResult(rt_reg, rs < se_imm16 ? 1 : 0);
      break;
    case SLTIU:
      SetResult(rt_reg, rs_u < static_cast<uint64_t>(se_imm16) ? 1 : 0);
      break;
    case ANDI:
      SetResult(rt_reg, rs & oe_imm16);
      break;
    case ORI:
      SetResult(rt_reg, rs | oe_imm16);
      break;
    case XORI:
      SetResult(rt_reg, rs ^ oe_imm16);
      break;
    case LUI:
      if (rs_reg != 0) {
        // AUI instruction.
        DCHECK(kArchVariant == kMips64r6);
        int32_t alu32_out = static_cast<int32_t>(rs + (se_imm16 << 16));
        SetResult(rt_reg, static_cast<int64_t>(alu32_out));
      } else {
        // LUI instruction.
        int32_t alu32_out = static_cast<int32_t>(oe_imm16 << 16);
        // Sign-extend result of 32bit operation into 64bit register.
        SetResult(rt_reg, static_cast<int64_t>(alu32_out));
      }
      break;
    case DAUI:
      DCHECK(kArchVariant == kMips64r6);
      DCHECK(rs_reg != 0);
      SetResult(rt_reg, rs + (se_imm16 << 16));
      break;
    // ------------- Memory instructions.
    case LB:
      set_register(rt_reg, ReadB(rs + se_imm16));
      break;
    case LH:
      set_register(rt_reg, ReadH(rs + se_imm16, instr_.instr()));
      break;
    case LWL: {
      // al_offset is offset of the effective address within an aligned word.
      uint8_t al_offset = (rs + se_imm16) & kInt32AlignmentMask;
      uint8_t byte_shift = kInt32AlignmentMask - al_offset;
      uint32_t mask = (1 << byte_shift * 8) - 1;
      addr = rs + se_imm16 - al_offset;
      int32_t val = ReadW(addr, instr_.instr());
      val <<= byte_shift * 8;
      val |= rt & mask;
      set_register(rt_reg, static_cast<int64_t>(val));
      break;
    }
    case LW:
      set_register(rt_reg, ReadW(rs + se_imm16, instr_.instr()));
      break;
    case LWU:
      set_register(rt_reg, ReadWU(rs + se_imm16, instr_.instr()));
      break;
    case LD:
      set_register(rt_reg, Read2W(rs + se_imm16, instr_.instr()));
      break;
    case LBU:
      set_register(rt_reg, ReadBU(rs + se_imm16));
      break;
    case LHU:
      set_register(rt_reg, ReadHU(rs + se_imm16, instr_.instr()));
      break;
    case LWR: {
      // al_offset is offset of the effective address within an aligned word.
      uint8_t al_offset = (rs + se_imm16) & kInt32AlignmentMask;
      uint8_t byte_shift = kInt32AlignmentMask - al_offset;
      uint32_t mask = al_offset ? (~0 << (byte_shift + 1) * 8) : 0;
      addr = rs + se_imm16 - al_offset;
      alu_out = ReadW(addr, instr_.instr());
      alu_out = static_cast<uint32_t> (alu_out) >> al_offset * 8;
      alu_out |= rt & mask;
      set_register(rt_reg, alu_out);
      break;
    }
    case LDL: {
      // al_offset is offset of the effective address within an aligned word.
      uint8_t al_offset = (rs + se_imm16) & kInt64AlignmentMask;
      uint8_t byte_shift = kInt64AlignmentMask - al_offset;
      uint64_t mask = (1UL << byte_shift * 8) - 1;
      addr = rs + se_imm16 - al_offset;
      alu_out = Read2W(addr, instr_.instr());
      alu_out <<= byte_shift * 8;
      alu_out |= rt & mask;
      set_register(rt_reg, alu_out);
      break;
    }
    case LDR: {
      // al_offset is offset of the effective address within an aligned word.
      uint8_t al_offset = (rs + se_imm16) & kInt64AlignmentMask;
      uint8_t byte_shift = kInt64AlignmentMask - al_offset;
      uint64_t mask = al_offset ? (~0UL << (byte_shift + 1) * 8) : 0UL;
      addr = rs + se_imm16 - al_offset;
      alu_out = Read2W(addr, instr_.instr());
      alu_out = alu_out >> al_offset * 8;
      alu_out |= rt & mask;
      set_register(rt_reg, alu_out);
      break;
    }
    case SB:
      WriteB(rs + se_imm16, static_cast<int8_t>(rt));
      break;
    case SH:
      WriteH(rs + se_imm16, static_cast<uint16_t>(rt), instr_.instr());
      break;
    case SWL: {
      uint8_t al_offset = (rs + se_imm16) & kInt32AlignmentMask;
      uint8_t byte_shift = kInt32AlignmentMask - al_offset;
      uint32_t mask = byte_shift ? (~0 << (al_offset + 1) * 8) : 0;
      addr = rs + se_imm16 - al_offset;
      uint64_t mem_value = ReadW(addr, instr_.instr()) & mask;
      mem_value |= static_cast<uint32_t>(rt) >> byte_shift * 8;
      WriteW(addr, static_cast<int32_t>(mem_value), instr_.instr());
      break;
    }
    case SW:
      WriteW(rs + se_imm16, static_cast<int32_t>(rt), instr_.instr());
      break;
    case SD:
      Write2W(rs + se_imm16, rt, instr_.instr());
      break;
    case SWR: {
      uint8_t al_offset = (rs + se_imm16) & kInt32AlignmentMask;
      uint32_t mask = (1 << al_offset * 8) - 1;
      addr = rs + se_imm16 - al_offset;
      uint64_t mem_value = ReadW(addr, instr_.instr());
      mem_value = (rt << al_offset * 8) | (mem_value & mask);
      WriteW(addr, static_cast<int32_t>(mem_value), instr_.instr());
      break;
    }
    case SDL: {
      uint8_t al_offset = (rs + se_imm16) & kInt64AlignmentMask;
      uint8_t byte_shift = kInt64AlignmentMask - al_offset;
      uint64_t mask = byte_shift ? (~0UL << (al_offset + 1) * 8) : 0;
      addr = rs + se_imm16 - al_offset;
      uint64_t mem_value = Read2W(addr, instr_.instr()) & mask;
      mem_value |= rt >> byte_shift * 8;
      Write2W(addr, mem_value, instr_.instr());
      break;
    }
    case SDR: {
      uint8_t al_offset = (rs + se_imm16) & kInt64AlignmentMask;
      uint64_t mask = (1UL << al_offset * 8) - 1;
      addr = rs + se_imm16 - al_offset;
      uint64_t mem_value = Read2W(addr, instr_.instr());
      mem_value = (rt << al_offset * 8) | (mem_value & mask);
      Write2W(addr, mem_value, instr_.instr());
      break;
    }
    case LWC1:
      set_fpu_register(ft_reg, kFPUInvalidResult);  // Trash upper 32 bits.
      set_fpu_register_word(ft_reg, ReadW(rs + se_imm16, instr_.instr()));
      break;
    case LDC1:
      set_fpu_register_double(ft_reg, ReadD(rs + se_imm16, instr_.instr()));
      break;
    case SWC1: {
      int32_t alu_out_32 = static_cast<int32_t>(get_fpu_register(ft_reg));
      WriteW(rs + se_imm16, alu_out_32, instr_.instr());
      break;
    }
    case SDC1:
      WriteD(rs + se_imm16, get_fpu_register_double(ft_reg), instr_.instr());
      break;
    // ------------- PC-Relative instructions.
    case PCREL: {
      // rt field: checking 5-bits.
      int32_t imm21 = instr_.Imm21Value();
      int64_t current_pc = get_pc();
      uint8_t rt = (imm21 >> kImm16Bits);
      switch (rt) {
        case ALUIPC:
          addr = current_pc + (se_imm16 << 16);
          alu_out = static_cast<int64_t>(~0x0FFFF) & addr;
          break;
        case AUIPC:
          alu_out = current_pc + (se_imm16 << 16);
          break;
        default: {
          int32_t imm19 = instr_.Imm19Value();
          // rt field: checking the most significant 3-bits.
          rt = (imm21 >> kImm18Bits);
          switch (rt) {
            case LDPC:
              addr =
                  (current_pc & static_cast<int64_t>(~0x7)) + (se_imm18 << 3);
              alu_out = Read2W(addr, instr_.instr());
              break;
            default: {
              // rt field: checking the most significant 2-bits.
              rt = (imm21 >> kImm19Bits);
              switch (rt) {
                case LWUPC: {
                  // Set sign.
                  imm19 <<= (kOpcodeBits + kRsBits + 2);
                  imm19 >>= (kOpcodeBits + kRsBits + 2);
                  addr = current_pc + (imm19 << 2);
                  uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
                  alu_out = *ptr;
                  break;
                }
                case LWPC: {
                  // Set sign.
                  imm19 <<= (kOpcodeBits + kRsBits + 2);
                  imm19 >>= (kOpcodeBits + kRsBits + 2);
                  addr = current_pc + (imm19 << 2);
                  int32_t* ptr = reinterpret_cast<int32_t*>(addr);
                  alu_out = *ptr;
                  break;
                }
                case ADDIUPC: {
                  int64_t se_imm19 =
                      imm19 | ((imm19 & 0x40000) ? 0xfffffffffff80000 : 0);
                  alu_out = current_pc + (se_imm19 << 2);
                  break;
                }
                default:
                  UNREACHABLE();
                  break;
              }
              break;
            }
          }
          break;
        }
      }
      set_register(rs_reg, alu_out);
      break;
    }
    default:
      UNREACHABLE();
  }

  if (execute_branch_delay_instruction) {
    // Execute branch delay slot
    // We don't check for end_sim_pc. First it should not be met as the current
    // pc is valid. Secondly a jump should always execute its branch delay slot.
    Instruction* branch_delay_instr =
        reinterpret_cast<Instruction*>(get_pc() + Instruction::kInstrSize);
    BranchDelayInstructionDecode(branch_delay_instr);
  }

  // If needed update pc after the branch delay execution.
  if (next_pc != bad_ra) {
    set_pc(next_pc);
  }
}


// Type 3: instructions using a 26 bytes immediate. (e.g. j, jal).
void Simulator::DecodeTypeJump() {
  SimInstruction simInstr = instr_;
  // Get current pc.
  int64_t current_pc = get_pc();
  // Get unchanged bits of pc.
  int64_t pc_high_bits = current_pc & 0xfffffffff0000000;
  // Next pc.
  int64_t next_pc = pc_high_bits | (simInstr.Imm26Value() << 2);

  // Execute branch delay slot.
  // We don't check for end_sim_pc. First it should not be met as the current pc
  // is valid. Secondly a jump should always execute its branch delay slot.
  Instruction* branch_delay_instr =
      reinterpret_cast<Instruction*>(current_pc + Instruction::kInstrSize);
  BranchDelayInstructionDecode(branch_delay_instr);

  // Update pc and ra if necessary.
  // Do this after the branch delay execution.
  if (simInstr.IsLinkingInstruction()) {
    set_register(31, current_pc + 2 * Instruction::kInstrSize);
  }
  set_pc(next_pc);
  pc_modified_ = true;
}


// Executes the current instruction.
void Simulator::InstructionDecode(Instruction* instr) {
  if (v8::internal::FLAG_check_icache) {
    CheckICache(isolate_->simulator_i_cache(), instr);
  }
  pc_modified_ = false;

  v8::internal::EmbeddedVector<char, 256> buffer;

  if (::v8::internal::FLAG_trace_sim) {
    SNPrintF(trace_buf_, " ");
    disasm::NameConverter converter;
    disasm::Disassembler dasm(converter);
    // Use a reasonably large buffer.
    dasm.InstructionDecode(buffer, reinterpret_cast<byte*>(instr));
  }

  instr_ = instr;
  switch (instr_.InstructionType()) {
    case Instruction::kRegisterType:
      DecodeTypeRegister();
      break;
    case Instruction::kImmediateType:
      DecodeTypeImmediate();
      break;
    case Instruction::kJumpType:
      DecodeTypeJump();
      break;
    default:
      UNSUPPORTED();
  }

  if (::v8::internal::FLAG_trace_sim) {
    PrintF("  0x%08" PRIxPTR "   %-44s   %s\n",
           reinterpret_cast<intptr_t>(instr), buffer.start(),
           trace_buf_.start());
  }

  if (!pc_modified_) {
    set_register(pc, reinterpret_cast<int64_t>(instr) +
                 Instruction::kInstrSize);
  }
}



void Simulator::Execute() {
  // Get the PC to simulate. Cannot use the accessor here as we need the
  // raw PC value and not the one used as input to arithmetic instructions.
  int64_t program_counter = get_pc();
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
      if (icount_ == static_cast<uint64_t>(::v8::internal::FLAG_stop_sim_at)) {
        MipsDebugger dbg(this);
        dbg.Debug();
      } else {
        InstructionDecode(instr);
      }
      program_counter = get_pc();
    }
  }
}


void Simulator::CallInternal(byte* entry) {
  // Adjust JS-based stack limit to C-based stack limit.
  isolate_->stack_guard()->AdjustStackLimitForSimulator();

  // Prepare to execute the code at entry.
  set_register(pc, reinterpret_cast<int64_t>(entry));
  // Put down marker for end of simulation. The simulator will stop simulation
  // when the PC reaches this value. By saving the "end simulation" value into
  // the LR the simulation stops when returning to this call point.
  set_register(ra, end_sim_pc);

  // Remember the values of callee-saved registers.
  // The code below assumes that r9 is not used as sb (static base) in
  // simulator code and therefore is regarded as a callee-saved register.
  int64_t s0_val = get_register(s0);
  int64_t s1_val = get_register(s1);
  int64_t s2_val = get_register(s2);
  int64_t s3_val = get_register(s3);
  int64_t s4_val = get_register(s4);
  int64_t s5_val = get_register(s5);
  int64_t s6_val = get_register(s6);
  int64_t s7_val = get_register(s7);
  int64_t gp_val = get_register(gp);
  int64_t sp_val = get_register(sp);
  int64_t fp_val = get_register(fp);

  // Set up the callee-saved registers with a known value. To be able to check
  // that they are preserved properly across JS execution.
  int64_t callee_saved_value = icount_;
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

  // Start the simulation.
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
}


int64_t Simulator::Call(byte* entry, int argument_count, ...) {
  const int kRegisterPassedArguments = 8;
  va_list parameters;
  va_start(parameters, argument_count);
  // Set up arguments.

  // First four arguments passed in registers in both ABI's.
  DCHECK(argument_count >= 4);
  set_register(a0, va_arg(parameters, int64_t));
  set_register(a1, va_arg(parameters, int64_t));
  set_register(a2, va_arg(parameters, int64_t));
  set_register(a3, va_arg(parameters, int64_t));

  // Up to eight arguments passed in registers in N64 ABI.
  // TODO(plind): N64 ABI calls these regs a4 - a7. Clarify this.
  if (argument_count >= 5) set_register(a4, va_arg(parameters, int64_t));
  if (argument_count >= 6) set_register(a5, va_arg(parameters, int64_t));
  if (argument_count >= 7) set_register(a6, va_arg(parameters, int64_t));
  if (argument_count >= 8) set_register(a7, va_arg(parameters, int64_t));

  // Remaining arguments passed on stack.
  int64_t original_stack = get_register(sp);
  // Compute position of stack on entry to generated code.
  int stack_args_count = (argument_count > kRegisterPassedArguments) ?
                         (argument_count - kRegisterPassedArguments) : 0;
  int stack_args_size = stack_args_count * sizeof(int64_t) + kCArgsSlotsSize;
  int64_t entry_stack = original_stack - stack_args_size;

  if (base::OS::ActivationFrameAlignment() != 0) {
    entry_stack &= -base::OS::ActivationFrameAlignment();
  }
  // Store remaining arguments on stack, from low to high memory.
  intptr_t* stack_argument = reinterpret_cast<intptr_t*>(entry_stack);
  for (int i = kRegisterPassedArguments; i < argument_count; i++) {
    int stack_index = i - kRegisterPassedArguments + kCArgSlotCount;
    stack_argument[stack_index] = va_arg(parameters, int64_t);
  }
  va_end(parameters);
  set_register(sp, entry_stack);

  CallInternal(entry);

  // Pop stack passed arguments.
  CHECK_EQ(entry_stack, get_register(sp));
  set_register(sp, original_stack);

  int64_t result = get_register(v0);
  return result;
}


double Simulator::CallFP(byte* entry, double d0, double d1) {
  if (!IsMipsSoftFloatABI) {
    const FPURegister fparg2 = f13;
    set_fpu_register_double(f12, d0);
    set_fpu_register_double(fparg2, d1);
  } else {
    int buffer[2];
    DCHECK(sizeof(buffer[0]) * 2 == sizeof(d0));
    memcpy(buffer, &d0, sizeof(d0));
    set_dw_register(a0, buffer);
    memcpy(buffer, &d1, sizeof(d1));
    set_dw_register(a2, buffer);
  }
  CallInternal(entry);
  if (!IsMipsSoftFloatABI) {
    return get_fpu_register_double(f0);
  } else {
    return get_double_from_register_pair(v0);
  }
}


uintptr_t Simulator::PushAddress(uintptr_t address) {
  int64_t new_sp = get_register(sp) - sizeof(uintptr_t);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(new_sp);
  *stack_slot = address;
  set_register(sp, new_sp);
  return new_sp;
}


uintptr_t Simulator::PopAddress() {
  int64_t current_sp = get_register(sp);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(current_sp);
  uintptr_t address = *stack_slot;
  set_register(sp, current_sp + sizeof(uintptr_t));
  return address;
}


#undef UNSUPPORTED
}  // namespace internal
}  // namespace v8

#endif  // USE_SIMULATOR

#endif  // V8_TARGET_ARCH_MIPS64
