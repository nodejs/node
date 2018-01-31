// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stdlib.h>
#include <cmath>

#if V8_TARGET_ARCH_S390

#include "src/assembler.h"
#include "src/base/bits.h"
#include "src/base/once.h"
#include "src/codegen.h"
#include "src/disasm.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"
#include "src/runtime/runtime-utils.h"
#include "src/s390/constants-s390.h"
#include "src/s390/simulator-s390.h"
#if defined(USE_SIMULATOR)

// Only build the simulator if not compiling for real s390 hardware.
namespace v8 {
namespace internal {

const auto GetRegConfig = RegisterConfiguration::Default;

// This macro provides a platform independent use of sscanf. The reason for
// SScanF not being implemented in a platform independent way through
// ::v8::internal::OS in the same way as SNPrintF is that the
// Windows C Run-Time Library does not provide vsscanf.
#define SScanF sscanf  // NOLINT

// The S390Debugger class is used by the simulator while debugging simulated
// z/Architecture code.
class S390Debugger {
 public:
  explicit S390Debugger(Simulator* sim) : sim_(sim) {}

  void Stop(Instruction* instr);
  void Debug();

 private:
#if V8_TARGET_LITTLE_ENDIAN
  static const Instr kBreakpointInstr = (0x0000FFB2);  // TRAP4 0000
  static const Instr kNopInstr = (0x00160016);         // OR r0, r0 x2
#else
  static const Instr kBreakpointInstr = (0xB2FF0000);  // TRAP4 0000
  static const Instr kNopInstr = (0x16001600);         // OR r0, r0 x2
#endif

  Simulator* sim_;

  intptr_t GetRegisterValue(int regnum);
  double GetRegisterPairDoubleValue(int regnum);
  double GetFPDoubleRegisterValue(int regnum);
  float GetFPFloatRegisterValue(int regnum);
  bool GetValue(const char* desc, intptr_t* value);
  bool GetFPDoubleValue(const char* desc, double* value);

  // Set or delete a breakpoint. Returns true if successful.
  bool SetBreakpoint(Instruction* break_pc);
  bool DeleteBreakpoint(Instruction* break_pc);

  // Undo and redo all breakpoints. This is needed to bracket disassembly and
  // execution to skip past breakpoints when run from the debugger.
  void UndoBreakpoints();
  void RedoBreakpoints();
};

void S390Debugger::Stop(Instruction* instr) {
  // Get the stop code.
  // use of kStopCodeMask not right on PowerPC
  uint32_t code = instr->SvcValue() & kStopCodeMask;
  // Retrieve the encoded address, which comes just after this stop.
  char* msg = *reinterpret_cast<char**>(sim_->get_pc() + sizeof(FourByteInstr));
  // Update this stop description.
  if (sim_->isWatchedStop(code) && !sim_->watched_stops_[code].desc) {
    sim_->watched_stops_[code].desc = msg;
  }
  // Print the stop message and code if it is not the default code.
  if (code != kMaxStopCode) {
    PrintF("Simulator hit stop %u: %s\n", code, msg);
  } else {
    PrintF("Simulator hit %s\n", msg);
  }
  sim_->set_pc(sim_->get_pc() + sizeof(FourByteInstr) + kPointerSize);
  Debug();
}

intptr_t S390Debugger::GetRegisterValue(int regnum) {
  return sim_->get_register(regnum);
}

double S390Debugger::GetRegisterPairDoubleValue(int regnum) {
  return sim_->get_double_from_register_pair(regnum);
}

double S390Debugger::GetFPDoubleRegisterValue(int regnum) {
  return sim_->get_double_from_d_register(regnum);
}

float S390Debugger::GetFPFloatRegisterValue(int regnum) {
  return sim_->get_float32_from_d_register(regnum);
}

bool S390Debugger::GetValue(const char* desc, intptr_t* value) {
  int regnum = Registers::Number(desc);
  if (regnum != kNoRegister) {
    *value = GetRegisterValue(regnum);
    return true;
  } else {
    if (strncmp(desc, "0x", 2) == 0) {
      return SScanF(desc + 2, "%" V8PRIxPTR,
                    reinterpret_cast<uintptr_t*>(value)) == 1;
    } else {
      return SScanF(desc, "%" V8PRIuPTR, reinterpret_cast<uintptr_t*>(value)) ==
             1;
    }
  }
  return false;
}

bool S390Debugger::GetFPDoubleValue(const char* desc, double* value) {
  int regnum = DoubleRegisters::Number(desc);
  if (regnum != kNoRegister) {
    *value = sim_->get_double_from_d_register(regnum);
    return true;
  }
  return false;
}

bool S390Debugger::SetBreakpoint(Instruction* break_pc) {
  // Check if a breakpoint can be set. If not return without any side-effects.
  if (sim_->break_pc_ != nullptr) {
    return false;
  }

  // Set the breakpoint.
  sim_->break_pc_ = break_pc;
  sim_->break_instr_ = break_pc->InstructionBits();
  // Not setting the breakpoint instruction in the code itself. It will be set
  // when the debugger shell continues.
  return true;
}

bool S390Debugger::DeleteBreakpoint(Instruction* break_pc) {
  if (sim_->break_pc_ != nullptr) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }

  sim_->break_pc_ = nullptr;
  sim_->break_instr_ = 0;
  return true;
}

void S390Debugger::UndoBreakpoints() {
  if (sim_->break_pc_ != nullptr) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }
}

void S390Debugger::RedoBreakpoints() {
  if (sim_->break_pc_ != nullptr) {
    sim_->break_pc_->SetInstructionBits(kBreakpointInstr);
  }
}

void S390Debugger::Debug() {
  intptr_t last_pc = -1;
  bool done = false;

#define COMMAND_SIZE 63
#define ARG_SIZE 255

#define STR(a) #a
#define XSTR(a) STR(a)

  char cmd[COMMAND_SIZE + 1];
  char arg1[ARG_SIZE + 1];
  char arg2[ARG_SIZE + 1];
  char* argv[3] = {cmd, arg1, arg2};

  // make sure to have a proper terminating character if reaching the limit
  cmd[COMMAND_SIZE] = 0;
  arg1[ARG_SIZE] = 0;
  arg2[ARG_SIZE] = 0;

  // Undo all set breakpoints while running in the debugger shell. This will
  // make them invisible to all commands.
  UndoBreakpoints();
  // Disable tracing while simulating
  bool trace = ::v8::internal::FLAG_trace_sim;
  ::v8::internal::FLAG_trace_sim = false;

  while (!done && !sim_->has_bad_pc()) {
    if (last_pc != sim_->get_pc()) {
      disasm::NameConverter converter;
      disasm::Disassembler dasm(converter);
      // use a reasonably large buffer
      v8::internal::EmbeddedVector<char, 256> buffer;
      dasm.InstructionDecode(buffer, reinterpret_cast<byte*>(sim_->get_pc()));
      PrintF("  0x%08" V8PRIxPTR "  %s\n", sim_->get_pc(), buffer.start());
      last_pc = sim_->get_pc();
    }
    char* line = ReadLine("sim> ");
    if (line == nullptr) {
      break;
    } else {
      char* last_input = sim_->last_debugger_input();
      if (strcmp(line, "\n") == 0 && last_input != nullptr) {
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
        intptr_t value;

        // If at a breakpoint, proceed past it.
        if ((reinterpret_cast<Instruction*>(sim_->get_pc()))
                ->InstructionBits() == 0x7d821008) {
          sim_->set_pc(sim_->get_pc() + sizeof(FourByteInstr));
        } else {
          sim_->ExecuteInstruction(
              reinterpret_cast<Instruction*>(sim_->get_pc()));
        }

        if (argc == 2 && last_pc != sim_->get_pc()) {
          disasm::NameConverter converter;
          disasm::Disassembler dasm(converter);
          // use a reasonably large buffer
          v8::internal::EmbeddedVector<char, 256> buffer;

          if (GetValue(arg1, &value)) {
            // Interpret a numeric argument as the number of instructions to
            // step past.
            for (int i = 1; (!sim_->has_bad_pc()) &&  i < value; i++) {
              dasm.InstructionDecode(buffer,
                                    reinterpret_cast<byte*>(sim_->get_pc()));
              PrintF("  0x%08" V8PRIxPTR "  %s\n", sim_->get_pc(),
                    buffer.start());
              sim_->ExecuteInstruction(
                      reinterpret_cast<Instruction*>(sim_->get_pc()));
            }
          } else {
            // Otherwise treat it as the mnemonic of the opcode to stop at.
            char mnemonic[256];
            while (!sim_->has_bad_pc()) {
              dasm.InstructionDecode(buffer,
                                    reinterpret_cast<byte*>(sim_->get_pc()));
              char* mnemonicStart = buffer.start();
              while (*mnemonicStart != 0 && *mnemonicStart != ' ')
                mnemonicStart++;
              SScanF(mnemonicStart, "%s", mnemonic);
              if (!strcmp(arg1, mnemonic)) break;

              PrintF("  0x%08" V8PRIxPTR "  %s\n", sim_->get_pc(),
                    buffer.start());
              sim_->ExecuteInstruction(
                      reinterpret_cast<Instruction*>(sim_->get_pc()));
            }
          }
        }
      } else if ((strcmp(cmd, "c") == 0) || (strcmp(cmd, "cont") == 0)) {
        // If at a breakpoint, proceed past it.
        if ((reinterpret_cast<Instruction*>(sim_->get_pc()))
                ->InstructionBits() == 0x7d821008) {
          sim_->set_pc(sim_->get_pc() + sizeof(FourByteInstr));
        } else {
          // Execute the one instruction we broke at with breakpoints disabled.
          sim_->ExecuteInstruction(
              reinterpret_cast<Instruction*>(sim_->get_pc()));
        }
        // Leave the debugger shell.
        done = true;
      } else if ((strcmp(cmd, "p") == 0) || (strcmp(cmd, "print") == 0)) {
        if (argc == 2 || (argc == 3 && strcmp(arg2, "fp") == 0)) {
          intptr_t value;
          double dvalue;
          if (strcmp(arg1, "all") == 0) {
            for (int i = 0; i < kNumRegisters; i++) {
              value = GetRegisterValue(i);
              PrintF("    %3s: %08" V8PRIxPTR,
                     GetRegConfig()->GetGeneralRegisterName(i), value);
              if ((argc == 3 && strcmp(arg2, "fp") == 0) && i < 8 &&
                  (i % 2) == 0) {
                dvalue = GetRegisterPairDoubleValue(i);
                PrintF(" (%f)\n", dvalue);
              } else if (i != 0 && !((i + 1) & 3)) {
                PrintF("\n");
              }
            }
            PrintF("  pc: %08" V8PRIxPTR "  cr: %08x\n", sim_->special_reg_pc_,
                   sim_->condition_reg_);
          } else if (strcmp(arg1, "alld") == 0) {
            for (int i = 0; i < kNumRegisters; i++) {
              value = GetRegisterValue(i);
              PrintF("     %3s: %08" V8PRIxPTR " %11" V8PRIdPTR,
                     GetRegConfig()->GetGeneralRegisterName(i), value, value);
              if ((argc == 3 && strcmp(arg2, "fp") == 0) && i < 8 &&
                  (i % 2) == 0) {
                dvalue = GetRegisterPairDoubleValue(i);
                PrintF(" (%f)\n", dvalue);
              } else if (!((i + 1) % 2)) {
                PrintF("\n");
              }
            }
            PrintF("   pc: %08" V8PRIxPTR "  cr: %08x\n", sim_->special_reg_pc_,
                   sim_->condition_reg_);
          } else if (strcmp(arg1, "allf") == 0) {
            for (int i = 0; i < DoubleRegister::kNumRegisters; i++) {
              float fvalue = GetFPFloatRegisterValue(i);
              uint32_t as_words = bit_cast<uint32_t>(fvalue);
              PrintF("%3s: %f 0x%08x\n",
                     GetRegConfig()->GetDoubleRegisterName(i), fvalue,
                     as_words);
            }
          } else if (strcmp(arg1, "alld") == 0) {
            for (int i = 0; i < DoubleRegister::kNumRegisters; i++) {
              dvalue = GetFPDoubleRegisterValue(i);
              uint64_t as_words = bit_cast<uint64_t>(dvalue);
              PrintF("%3s: %f 0x%08x %08x\n",
                     GetRegConfig()->GetDoubleRegisterName(i), dvalue,
                     static_cast<uint32_t>(as_words >> 32),
                     static_cast<uint32_t>(as_words & 0xffffffff));
            }
          } else if (arg1[0] == 'r' &&
                     (arg1[1] >= '0' && arg1[1] <= '2' &&
                      (arg1[2] == '\0' || (arg1[2] >= '0' && arg1[2] <= '5' &&
                                           arg1[3] == '\0')))) {
            int regnum = strtoul(&arg1[1], 0, 10);
            if (regnum != kNoRegister) {
              value = GetRegisterValue(regnum);
              PrintF("%s: 0x%08" V8PRIxPTR " %" V8PRIdPTR "\n", arg1, value,
                     value);
            } else {
              PrintF("%s unrecognized\n", arg1);
            }
          } else {
            if (GetValue(arg1, &value)) {
              PrintF("%s: 0x%08" V8PRIxPTR " %" V8PRIdPTR "\n", arg1, value,
                     value);
            } else if (GetFPDoubleValue(arg1, &dvalue)) {
              uint64_t as_words = bit_cast<uint64_t>(dvalue);
              PrintF("%s: %f 0x%08x %08x\n", arg1, dvalue,
                     static_cast<uint32_t>(as_words >> 32),
                     static_cast<uint32_t>(as_words & 0xffffffff));
            } else {
              PrintF("%s unrecognized\n", arg1);
            }
          }
        } else {
          PrintF("print <register>\n");
        }
      } else if ((strcmp(cmd, "po") == 0) ||
                 (strcmp(cmd, "printobject") == 0)) {
        if (argc == 2) {
          intptr_t value;
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
      } else if (strcmp(cmd, "setpc") == 0) {
        intptr_t value;

        if (!GetValue(arg1, &value)) {
          PrintF("%s unrecognized\n", arg1);
          continue;
        }
        sim_->set_pc(value);
      } else if (strcmp(cmd, "stack") == 0 || strcmp(cmd, "mem") == 0) {
        intptr_t* cur = nullptr;
        intptr_t* end = nullptr;
        int next_arg = 1;

        if (strcmp(cmd, "stack") == 0) {
          cur = reinterpret_cast<intptr_t*>(sim_->get_register(Simulator::sp));
        } else {  // "mem"
          intptr_t value;
          if (!GetValue(arg1, &value)) {
            PrintF("%s unrecognized\n", arg1);
            continue;
          }
          cur = reinterpret_cast<intptr_t*>(value);
          next_arg++;
        }

        intptr_t words;  // likely inaccurate variable name for 64bit
        if (argc == next_arg) {
          words = 10;
        } else {
          if (!GetValue(argv[next_arg], &words)) {
            words = 10;
          }
        }
        end = cur + words;

        while (cur < end) {
          PrintF("  0x%08" V8PRIxPTR ":  0x%08" V8PRIxPTR " %10" V8PRIdPTR,
                 reinterpret_cast<intptr_t>(cur), *cur, *cur);
          HeapObject* obj = reinterpret_cast<HeapObject*>(*cur);
          intptr_t value = *cur;
          Heap* current_heap = sim_->isolate_->heap();
          if (((value & 1) == 0) ||
              current_heap->ContainsSlow(obj->address())) {
            PrintF("(smi %d)", PlatformSmiTagging::SmiToInt(obj));
          } else if (current_heap->Contains(obj)) {
            PrintF(" (");
            obj->ShortPrint();
            PrintF(")");
          }
          PrintF("\n");
          cur++;
        }
      } else if (strcmp(cmd, "disasm") == 0 || strcmp(cmd, "di") == 0) {
        disasm::NameConverter converter;
        disasm::Disassembler dasm(converter);
        // use a reasonably large buffer
        v8::internal::EmbeddedVector<char, 256> buffer;

        byte* prev = nullptr;
        byte* cur = nullptr;
        // Default number of instructions to disassemble.
        int32_t numInstructions = 10;

        if (argc == 1) {
          cur = reinterpret_cast<byte*>(sim_->get_pc());
        } else if (argc == 2) {
          int regnum = Registers::Number(arg1);
          if (regnum != kNoRegister || strncmp(arg1, "0x", 2) == 0) {
            // The argument is an address or a register name.
            intptr_t value;
            if (GetValue(arg1, &value)) {
              cur = reinterpret_cast<byte*>(value);
            }
          } else {
            // The argument is the number of instructions.
            intptr_t value;
            if (GetValue(arg1, &value)) {
              cur = reinterpret_cast<byte*>(sim_->get_pc());
              // Disassemble <arg1> instructions.
              numInstructions = static_cast<int32_t>(value);
            }
          }
        } else {
          intptr_t value1;
          intptr_t value2;
          if (GetValue(arg1, &value1) && GetValue(arg2, &value2)) {
            cur = reinterpret_cast<byte*>(value1);
            // Disassemble <arg2> instructions.
            numInstructions = static_cast<int32_t>(value2);
          }
        }

        while (numInstructions > 0) {
          prev = cur;
          cur += dasm.InstructionDecode(buffer, cur);
          PrintF("  0x%08" V8PRIxPTR "  %s\n", reinterpret_cast<intptr_t>(prev),
                 buffer.start());
          numInstructions--;
        }
      } else if (strcmp(cmd, "gdb") == 0) {
        PrintF("relinquishing control to gdb\n");
        v8::base::OS::DebugBreak();
        PrintF("regaining control from gdb\n");
      } else if (strcmp(cmd, "break") == 0) {
        if (argc == 2) {
          intptr_t value;
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
        if (!DeleteBreakpoint(nullptr)) {
          PrintF("deleting breakpoint failed\n");
        }
      } else if (strcmp(cmd, "cr") == 0) {
        PrintF("Condition reg: %08x\n", sim_->condition_reg_);
      } else if (strcmp(cmd, "stop") == 0) {
        intptr_t value;
        intptr_t stop_pc =
            sim_->get_pc() - (sizeof(FourByteInstr) + kPointerSize);
        Instruction* stop_instr = reinterpret_cast<Instruction*>(stop_pc);
        Instruction* msg_address =
            reinterpret_cast<Instruction*>(stop_pc + sizeof(FourByteInstr));
        if ((argc == 2) && (strcmp(arg1, "unstop") == 0)) {
          // Remove the current stop.
          if (sim_->isStopInstruction(stop_instr)) {
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
              for (uint32_t i = 0; i < sim_->kNumOfWatchedStops; i++) {
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
              for (uint32_t i = 0; i < sim_->kNumOfWatchedStops; i++) {
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
              for (uint32_t i = 0; i < sim_->kNumOfWatchedStops; i++) {
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
      } else if (strcmp(cmd, "icount") == 0) {
        PrintF("%05" PRId64 "\n", sim_->icount_);
      } else if ((strcmp(cmd, "t") == 0) || strcmp(cmd, "trace") == 0) {
        ::v8::internal::FLAG_trace_sim = !::v8::internal::FLAG_trace_sim;
        PrintF("Trace of executed instructions is %s\n",
               ::v8::internal::FLAG_trace_sim ? "on" : "off");
      } else if ((strcmp(cmd, "h") == 0) || (strcmp(cmd, "help") == 0)) {
        PrintF("cont\n");
        PrintF("  continue execution (alias 'c')\n");
        PrintF("stepi [num instructions]\n");
        PrintF("  step one/num instruction(s) (alias 'si')\n");
        PrintF("print <register>\n");
        PrintF("  print register content (alias 'p')\n");
        PrintF("  use register name 'all' to display all integer registers\n");
        PrintF(
            "  use register name 'alld' to display integer registers "
            "with decimal values\n");
        PrintF("  use register name 'rN' to display register number 'N'\n");
        PrintF("  add argument 'fp' to print register pair double values\n");
        PrintF(
            "  use register name 'allf' to display floating-point "
            "registers\n");
        PrintF("printobject <register>\n");
        PrintF("  print an object from a register (alias 'po')\n");
        PrintF("cr\n");
        PrintF("  print condition register\n");
        PrintF("stack [<num words>]\n");
        PrintF("  dump stack content, default dump 10 words)\n");
        PrintF("mem <address> [<num words>]\n");
        PrintF("  dump memory content, default dump 10 words)\n");
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
        PrintF("trace (alias 't')\n");
        PrintF("  toogle the tracing of all executed statements\n");
        PrintF("stop feature:\n");
        PrintF("  Description:\n");
        PrintF("    Stops are debug instructions inserted by\n");
        PrintF("    the Assembler::stop() function.\n");
        PrintF("    When hitting a stop, the Simulator will\n");
        PrintF("    stop and and give control to the S390Debugger.\n");
        PrintF("    The first %d stop codes are watched:\n",
               Simulator::kNumOfWatchedStops);
        PrintF("    - They can be enabled / disabled: the Simulator\n");
        PrintF("      will / won't stop when hitting them.\n");
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
  // Restore tracing
  ::v8::internal::FLAG_trace_sim = trace;

#undef COMMAND_SIZE
#undef ARG_SIZE

#undef STR
#undef XSTR
}

static bool ICacheMatch(void* one, void* two) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(one) & CachePage::kPageMask, 0);
  DCHECK_EQ(reinterpret_cast<intptr_t>(two) & CachePage::kPageMask, 0);
  return one == two;
}

static uint32_t ICacheHash(void* key) {
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(key)) >> 2;
}

static bool AllOnOnePage(uintptr_t start, int size) {
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
  intptr_t start = reinterpret_cast<intptr_t>(start_addr);
  int intra_line = (start & CachePage::kLineMask);
  start -= intra_line;
  size += intra_line;
  size = ((size - 1) | CachePage::kLineMask) + 1;
  int offset = (start & CachePage::kPageMask);
  while (!AllOnOnePage(start, size - 1)) {
    int bytes_to_flush = CachePage::kPageSize - offset;
    FlushOnePage(i_cache, start, bytes_to_flush);
    start += bytes_to_flush;
    size -= bytes_to_flush;
    DCHECK_EQ(0, static_cast<int>(start & CachePage::kPageMask));
    offset = 0;
  }
  if (size != 0) {
    FlushOnePage(i_cache, start, size);
  }
}

CachePage* Simulator::GetCachePage(base::CustomMatcherHashMap* i_cache,
                                   void* page) {
  base::HashMap::Entry* entry = i_cache->LookupOrInsert(page, ICacheHash(page));
  if (entry->value == nullptr) {
    CachePage* new_page = new CachePage();
    entry->value = new_page;
  }
  return reinterpret_cast<CachePage*>(entry->value);
}

// Flush from start up to and not including start + size.
void Simulator::FlushOnePage(base::CustomMatcherHashMap* i_cache,
                             intptr_t start, int size) {
  DCHECK_LE(size, CachePage::kPageSize);
  DCHECK(AllOnOnePage(start, size - 1));
  DCHECK_EQ(start & CachePage::kLineMask, 0);
  DCHECK_EQ(size & CachePage::kLineMask, 0);
  void* page = reinterpret_cast<void*>(start & (~CachePage::kPageMask));
  int offset = (start & CachePage::kPageMask);
  CachePage* cache_page = GetCachePage(i_cache, page);
  char* valid_bytemap = cache_page->ValidityByte(offset);
  memset(valid_bytemap, CachePage::LINE_INVALID, size >> CachePage::kLineShift);
}

void Simulator::CheckICache(base::CustomMatcherHashMap* i_cache,
                            Instruction* instr) {
  intptr_t address = reinterpret_cast<intptr_t>(instr);
  void* page = reinterpret_cast<void*>(address & (~CachePage::kPageMask));
  void* line = reinterpret_cast<void*>(address & (~CachePage::kLineMask));
  int offset = (address & CachePage::kPageMask);
  CachePage* cache_page = GetCachePage(i_cache, page);
  char* cache_valid_byte = cache_page->ValidityByte(offset);
  bool cache_hit = (*cache_valid_byte == CachePage::LINE_VALID);
  char* cached_line = cache_page->CachedData(offset & ~CachePage::kLineMask);
  if (cache_hit) {
    // Check that the data in memory matches the contents of the I-cache.
    CHECK_EQ(memcmp(reinterpret_cast<void*>(instr),
                    cache_page->CachedData(offset), sizeof(FourByteInstr)),
             0);
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
  static base::OnceType once = V8_ONCE_INIT;
  base::CallOnce(&once, &Simulator::EvalTableInit);
}

Simulator::EvaluateFuncType Simulator::EvalTable[] = {nullptr};

void Simulator::EvalTableInit() {
  for (int i = 0; i < MAX_NUM_OPCODES; i++) {
    EvalTable[i] = &Simulator::Evaluate_Unknown;
  }

#define S390_SUPPORTED_VECTOR_OPCODE_LIST(V)                 \
  V(vfs, VFS, 0xE7E2) /* type = VRR_C VECTOR FP SUBTRACT  */ \
  V(vfa, VFA, 0xE7E3) /* type = VRR_C VECTOR FP ADD  */      \
  V(vfd, VFD, 0xE7E5) /* type = VRR_C VECTOR FP DIVIDE  */   \
  V(vfm, VFM, 0xE7E7) /* type = VRR_C VECTOR FP MULTIPLY  */

#define CREATE_EVALUATE_TABLE(name, op_name, op_value) \
  EvalTable[op_name] = &Simulator::Evaluate_##op_name;
  S390_SUPPORTED_VECTOR_OPCODE_LIST(CREATE_EVALUATE_TABLE);
#undef CREATE_EVALUATE_TABLE

  EvalTable[DUMY] = &Simulator::Evaluate_DUMY;
  EvalTable[BKPT] = &Simulator::Evaluate_BKPT;
  EvalTable[SPM] = &Simulator::Evaluate_SPM;
  EvalTable[BALR] = &Simulator::Evaluate_BALR;
  EvalTable[BCTR] = &Simulator::Evaluate_BCTR;
  EvalTable[BCR] = &Simulator::Evaluate_BCR;
  EvalTable[SVC] = &Simulator::Evaluate_SVC;
  EvalTable[BSM] = &Simulator::Evaluate_BSM;
  EvalTable[BASSM] = &Simulator::Evaluate_BASSM;
  EvalTable[BASR] = &Simulator::Evaluate_BASR;
  EvalTable[MVCL] = &Simulator::Evaluate_MVCL;
  EvalTable[CLCL] = &Simulator::Evaluate_CLCL;
  EvalTable[LPR] = &Simulator::Evaluate_LPR;
  EvalTable[LNR] = &Simulator::Evaluate_LNR;
  EvalTable[LTR] = &Simulator::Evaluate_LTR;
  EvalTable[LCR] = &Simulator::Evaluate_LCR;
  EvalTable[NR] = &Simulator::Evaluate_NR;
  EvalTable[CLR] = &Simulator::Evaluate_CLR;
  EvalTable[OR] = &Simulator::Evaluate_OR;
  EvalTable[XR] = &Simulator::Evaluate_XR;
  EvalTable[LR] = &Simulator::Evaluate_LR;
  EvalTable[CR] = &Simulator::Evaluate_CR;
  EvalTable[AR] = &Simulator::Evaluate_AR;
  EvalTable[SR] = &Simulator::Evaluate_SR;
  EvalTable[MR] = &Simulator::Evaluate_MR;
  EvalTable[DR] = &Simulator::Evaluate_DR;
  EvalTable[ALR] = &Simulator::Evaluate_ALR;
  EvalTable[SLR] = &Simulator::Evaluate_SLR;
  EvalTable[LDR] = &Simulator::Evaluate_LDR;
  EvalTable[CDR] = &Simulator::Evaluate_CDR;
  EvalTable[LER] = &Simulator::Evaluate_LER;
  EvalTable[STH] = &Simulator::Evaluate_STH;
  EvalTable[LA] = &Simulator::Evaluate_LA;
  EvalTable[STC] = &Simulator::Evaluate_STC;
  EvalTable[IC_z] = &Simulator::Evaluate_IC_z;
  EvalTable[EX] = &Simulator::Evaluate_EX;
  EvalTable[BAL] = &Simulator::Evaluate_BAL;
  EvalTable[BCT] = &Simulator::Evaluate_BCT;
  EvalTable[BC] = &Simulator::Evaluate_BC;
  EvalTable[LH] = &Simulator::Evaluate_LH;
  EvalTable[CH] = &Simulator::Evaluate_CH;
  EvalTable[AH] = &Simulator::Evaluate_AH;
  EvalTable[SH] = &Simulator::Evaluate_SH;
  EvalTable[MH] = &Simulator::Evaluate_MH;
  EvalTable[BAS] = &Simulator::Evaluate_BAS;
  EvalTable[CVD] = &Simulator::Evaluate_CVD;
  EvalTable[CVB] = &Simulator::Evaluate_CVB;
  EvalTable[ST] = &Simulator::Evaluate_ST;
  EvalTable[LAE] = &Simulator::Evaluate_LAE;
  EvalTable[N] = &Simulator::Evaluate_N;
  EvalTable[CL] = &Simulator::Evaluate_CL;
  EvalTable[O] = &Simulator::Evaluate_O;
  EvalTable[X] = &Simulator::Evaluate_X;
  EvalTable[L] = &Simulator::Evaluate_L;
  EvalTable[C] = &Simulator::Evaluate_C;
  EvalTable[A] = &Simulator::Evaluate_A;
  EvalTable[S] = &Simulator::Evaluate_S;
  EvalTable[M] = &Simulator::Evaluate_M;
  EvalTable[D] = &Simulator::Evaluate_D;
  EvalTable[AL] = &Simulator::Evaluate_AL;
  EvalTable[SL] = &Simulator::Evaluate_SL;
  EvalTable[STD] = &Simulator::Evaluate_STD;
  EvalTable[LD] = &Simulator::Evaluate_LD;
  EvalTable[CD] = &Simulator::Evaluate_CD;
  EvalTable[STE] = &Simulator::Evaluate_STE;
  EvalTable[MS] = &Simulator::Evaluate_MS;
  EvalTable[LE] = &Simulator::Evaluate_LE;
  EvalTable[BRXH] = &Simulator::Evaluate_BRXH;
  EvalTable[BRXLE] = &Simulator::Evaluate_BRXLE;
  EvalTable[BXH] = &Simulator::Evaluate_BXH;
  EvalTable[BXLE] = &Simulator::Evaluate_BXLE;
  EvalTable[SRL] = &Simulator::Evaluate_SRL;
  EvalTable[SLL] = &Simulator::Evaluate_SLL;
  EvalTable[SRA] = &Simulator::Evaluate_SRA;
  EvalTable[SLA] = &Simulator::Evaluate_SLA;
  EvalTable[SRDL] = &Simulator::Evaluate_SRDL;
  EvalTable[SLDL] = &Simulator::Evaluate_SLDL;
  EvalTable[SRDA] = &Simulator::Evaluate_SRDA;
  EvalTable[SLDA] = &Simulator::Evaluate_SLDA;
  EvalTable[STM] = &Simulator::Evaluate_STM;
  EvalTable[TM] = &Simulator::Evaluate_TM;
  EvalTable[MVI] = &Simulator::Evaluate_MVI;
  EvalTable[TS] = &Simulator::Evaluate_TS;
  EvalTable[NI] = &Simulator::Evaluate_NI;
  EvalTable[CLI] = &Simulator::Evaluate_CLI;
  EvalTable[OI] = &Simulator::Evaluate_OI;
  EvalTable[XI] = &Simulator::Evaluate_XI;
  EvalTable[LM] = &Simulator::Evaluate_LM;
  EvalTable[CS] = &Simulator::Evaluate_CS;
  EvalTable[MVCLE] = &Simulator::Evaluate_MVCLE;
  EvalTable[CLCLE] = &Simulator::Evaluate_CLCLE;
  EvalTable[MC] = &Simulator::Evaluate_MC;
  EvalTable[CDS] = &Simulator::Evaluate_CDS;
  EvalTable[STCM] = &Simulator::Evaluate_STCM;
  EvalTable[ICM] = &Simulator::Evaluate_ICM;
  EvalTable[BPRP] = &Simulator::Evaluate_BPRP;
  EvalTable[BPP] = &Simulator::Evaluate_BPP;
  EvalTable[TRTR] = &Simulator::Evaluate_TRTR;
  EvalTable[MVN] = &Simulator::Evaluate_MVN;
  EvalTable[MVC] = &Simulator::Evaluate_MVC;
  EvalTable[MVZ] = &Simulator::Evaluate_MVZ;
  EvalTable[NC] = &Simulator::Evaluate_NC;
  EvalTable[CLC] = &Simulator::Evaluate_CLC;
  EvalTable[OC] = &Simulator::Evaluate_OC;
  EvalTable[XC] = &Simulator::Evaluate_XC;
  EvalTable[MVCP] = &Simulator::Evaluate_MVCP;
  EvalTable[TR] = &Simulator::Evaluate_TR;
  EvalTable[TRT] = &Simulator::Evaluate_TRT;
  EvalTable[ED] = &Simulator::Evaluate_ED;
  EvalTable[EDMK] = &Simulator::Evaluate_EDMK;
  EvalTable[PKU] = &Simulator::Evaluate_PKU;
  EvalTable[UNPKU] = &Simulator::Evaluate_UNPKU;
  EvalTable[MVCIN] = &Simulator::Evaluate_MVCIN;
  EvalTable[PKA] = &Simulator::Evaluate_PKA;
  EvalTable[UNPKA] = &Simulator::Evaluate_UNPKA;
  EvalTable[PLO] = &Simulator::Evaluate_PLO;
  EvalTable[LMD] = &Simulator::Evaluate_LMD;
  EvalTable[SRP] = &Simulator::Evaluate_SRP;
  EvalTable[MVO] = &Simulator::Evaluate_MVO;
  EvalTable[PACK] = &Simulator::Evaluate_PACK;
  EvalTable[UNPK] = &Simulator::Evaluate_UNPK;
  EvalTable[ZAP] = &Simulator::Evaluate_ZAP;
  EvalTable[AP] = &Simulator::Evaluate_AP;
  EvalTable[SP] = &Simulator::Evaluate_SP;
  EvalTable[MP] = &Simulator::Evaluate_MP;
  EvalTable[DP] = &Simulator::Evaluate_DP;
  EvalTable[UPT] = &Simulator::Evaluate_UPT;
  EvalTable[PFPO] = &Simulator::Evaluate_PFPO;
  EvalTable[IIHH] = &Simulator::Evaluate_IIHH;
  EvalTable[IIHL] = &Simulator::Evaluate_IIHL;
  EvalTable[IILH] = &Simulator::Evaluate_IILH;
  EvalTable[IILL] = &Simulator::Evaluate_IILL;
  EvalTable[NIHH] = &Simulator::Evaluate_NIHH;
  EvalTable[NIHL] = &Simulator::Evaluate_NIHL;
  EvalTable[NILH] = &Simulator::Evaluate_NILH;
  EvalTable[NILL] = &Simulator::Evaluate_NILL;
  EvalTable[OIHH] = &Simulator::Evaluate_OIHH;
  EvalTable[OIHL] = &Simulator::Evaluate_OIHL;
  EvalTable[OILH] = &Simulator::Evaluate_OILH;
  EvalTable[OILL] = &Simulator::Evaluate_OILL;
  EvalTable[LLIHH] = &Simulator::Evaluate_LLIHH;
  EvalTable[LLIHL] = &Simulator::Evaluate_LLIHL;
  EvalTable[LLILH] = &Simulator::Evaluate_LLILH;
  EvalTable[LLILL] = &Simulator::Evaluate_LLILL;
  EvalTable[TMLH] = &Simulator::Evaluate_TMLH;
  EvalTable[TMLL] = &Simulator::Evaluate_TMLL;
  EvalTable[TMHH] = &Simulator::Evaluate_TMHH;
  EvalTable[TMHL] = &Simulator::Evaluate_TMHL;
  EvalTable[BRC] = &Simulator::Evaluate_BRC;
  EvalTable[BRAS] = &Simulator::Evaluate_BRAS;
  EvalTable[BRCT] = &Simulator::Evaluate_BRCT;
  EvalTable[BRCTG] = &Simulator::Evaluate_BRCTG;
  EvalTable[LHI] = &Simulator::Evaluate_LHI;
  EvalTable[LGHI] = &Simulator::Evaluate_LGHI;
  EvalTable[AHI] = &Simulator::Evaluate_AHI;
  EvalTable[AGHI] = &Simulator::Evaluate_AGHI;
  EvalTable[MHI] = &Simulator::Evaluate_MHI;
  EvalTable[MGHI] = &Simulator::Evaluate_MGHI;
  EvalTable[CHI] = &Simulator::Evaluate_CHI;
  EvalTable[CGHI] = &Simulator::Evaluate_CGHI;
  EvalTable[LARL] = &Simulator::Evaluate_LARL;
  EvalTable[LGFI] = &Simulator::Evaluate_LGFI;
  EvalTable[BRCL] = &Simulator::Evaluate_BRCL;
  EvalTable[BRASL] = &Simulator::Evaluate_BRASL;
  EvalTable[XIHF] = &Simulator::Evaluate_XIHF;
  EvalTable[XILF] = &Simulator::Evaluate_XILF;
  EvalTable[IIHF] = &Simulator::Evaluate_IIHF;
  EvalTable[IILF] = &Simulator::Evaluate_IILF;
  EvalTable[NIHF] = &Simulator::Evaluate_NIHF;
  EvalTable[NILF] = &Simulator::Evaluate_NILF;
  EvalTable[OIHF] = &Simulator::Evaluate_OIHF;
  EvalTable[OILF] = &Simulator::Evaluate_OILF;
  EvalTable[LLIHF] = &Simulator::Evaluate_LLIHF;
  EvalTable[LLILF] = &Simulator::Evaluate_LLILF;
  EvalTable[MSGFI] = &Simulator::Evaluate_MSGFI;
  EvalTable[MSFI] = &Simulator::Evaluate_MSFI;
  EvalTable[SLGFI] = &Simulator::Evaluate_SLGFI;
  EvalTable[SLFI] = &Simulator::Evaluate_SLFI;
  EvalTable[AGFI] = &Simulator::Evaluate_AGFI;
  EvalTable[AFI] = &Simulator::Evaluate_AFI;
  EvalTable[ALGFI] = &Simulator::Evaluate_ALGFI;
  EvalTable[ALFI] = &Simulator::Evaluate_ALFI;
  EvalTable[CGFI] = &Simulator::Evaluate_CGFI;
  EvalTable[CFI] = &Simulator::Evaluate_CFI;
  EvalTable[CLGFI] = &Simulator::Evaluate_CLGFI;
  EvalTable[CLFI] = &Simulator::Evaluate_CLFI;
  EvalTable[LLHRL] = &Simulator::Evaluate_LLHRL;
  EvalTable[LGHRL] = &Simulator::Evaluate_LGHRL;
  EvalTable[LHRL] = &Simulator::Evaluate_LHRL;
  EvalTable[LLGHRL] = &Simulator::Evaluate_LLGHRL;
  EvalTable[STHRL] = &Simulator::Evaluate_STHRL;
  EvalTable[LGRL] = &Simulator::Evaluate_LGRL;
  EvalTable[STGRL] = &Simulator::Evaluate_STGRL;
  EvalTable[LGFRL] = &Simulator::Evaluate_LGFRL;
  EvalTable[LRL] = &Simulator::Evaluate_LRL;
  EvalTable[LLGFRL] = &Simulator::Evaluate_LLGFRL;
  EvalTable[STRL] = &Simulator::Evaluate_STRL;
  EvalTable[EXRL] = &Simulator::Evaluate_EXRL;
  EvalTable[PFDRL] = &Simulator::Evaluate_PFDRL;
  EvalTable[CGHRL] = &Simulator::Evaluate_CGHRL;
  EvalTable[CHRL] = &Simulator::Evaluate_CHRL;
  EvalTable[CGRL] = &Simulator::Evaluate_CGRL;
  EvalTable[CGFRL] = &Simulator::Evaluate_CGFRL;
  EvalTable[ECTG] = &Simulator::Evaluate_ECTG;
  EvalTable[CSST] = &Simulator::Evaluate_CSST;
  EvalTable[LPD] = &Simulator::Evaluate_LPD;
  EvalTable[LPDG] = &Simulator::Evaluate_LPDG;
  EvalTable[BRCTH] = &Simulator::Evaluate_BRCTH;
  EvalTable[AIH] = &Simulator::Evaluate_AIH;
  EvalTable[ALSIH] = &Simulator::Evaluate_ALSIH;
  EvalTable[ALSIHN] = &Simulator::Evaluate_ALSIHN;
  EvalTable[CIH] = &Simulator::Evaluate_CIH;
  EvalTable[CLIH] = &Simulator::Evaluate_CLIH;
  EvalTable[STCK] = &Simulator::Evaluate_STCK;
  EvalTable[CFC] = &Simulator::Evaluate_CFC;
  EvalTable[IPM] = &Simulator::Evaluate_IPM;
  EvalTable[HSCH] = &Simulator::Evaluate_HSCH;
  EvalTable[MSCH] = &Simulator::Evaluate_MSCH;
  EvalTable[SSCH] = &Simulator::Evaluate_SSCH;
  EvalTable[STSCH] = &Simulator::Evaluate_STSCH;
  EvalTable[TSCH] = &Simulator::Evaluate_TSCH;
  EvalTable[TPI] = &Simulator::Evaluate_TPI;
  EvalTable[SAL] = &Simulator::Evaluate_SAL;
  EvalTable[RSCH] = &Simulator::Evaluate_RSCH;
  EvalTable[STCRW] = &Simulator::Evaluate_STCRW;
  EvalTable[STCPS] = &Simulator::Evaluate_STCPS;
  EvalTable[RCHP] = &Simulator::Evaluate_RCHP;
  EvalTable[SCHM] = &Simulator::Evaluate_SCHM;
  EvalTable[CKSM] = &Simulator::Evaluate_CKSM;
  EvalTable[SAR] = &Simulator::Evaluate_SAR;
  EvalTable[EAR] = &Simulator::Evaluate_EAR;
  EvalTable[MSR] = &Simulator::Evaluate_MSR;
  EvalTable[MSRKC] = &Simulator::Evaluate_MSRKC;
  EvalTable[MVST] = &Simulator::Evaluate_MVST;
  EvalTable[CUSE] = &Simulator::Evaluate_CUSE;
  EvalTable[SRST] = &Simulator::Evaluate_SRST;
  EvalTable[XSCH] = &Simulator::Evaluate_XSCH;
  EvalTable[STCKE] = &Simulator::Evaluate_STCKE;
  EvalTable[STCKF] = &Simulator::Evaluate_STCKF;
  EvalTable[SRNM] = &Simulator::Evaluate_SRNM;
  EvalTable[STFPC] = &Simulator::Evaluate_STFPC;
  EvalTable[LFPC] = &Simulator::Evaluate_LFPC;
  EvalTable[TRE] = &Simulator::Evaluate_TRE;
  EvalTable[CUUTF] = &Simulator::Evaluate_CUUTF;
  EvalTable[CUTFU] = &Simulator::Evaluate_CUTFU;
  EvalTable[STFLE] = &Simulator::Evaluate_STFLE;
  EvalTable[SRNMB] = &Simulator::Evaluate_SRNMB;
  EvalTable[SRNMT] = &Simulator::Evaluate_SRNMT;
  EvalTable[LFAS] = &Simulator::Evaluate_LFAS;
  EvalTable[PPA] = &Simulator::Evaluate_PPA;
  EvalTable[ETND] = &Simulator::Evaluate_ETND;
  EvalTable[TEND] = &Simulator::Evaluate_TEND;
  EvalTable[NIAI] = &Simulator::Evaluate_NIAI;
  EvalTable[TABORT] = &Simulator::Evaluate_TABORT;
  EvalTable[TRAP4] = &Simulator::Evaluate_TRAP4;
  EvalTable[LPEBR] = &Simulator::Evaluate_LPEBR;
  EvalTable[LNEBR] = &Simulator::Evaluate_LNEBR;
  EvalTable[LTEBR] = &Simulator::Evaluate_LTEBR;
  EvalTable[LCEBR] = &Simulator::Evaluate_LCEBR;
  EvalTable[LDEBR] = &Simulator::Evaluate_LDEBR;
  EvalTable[LXDBR] = &Simulator::Evaluate_LXDBR;
  EvalTable[LXEBR] = &Simulator::Evaluate_LXEBR;
  EvalTable[MXDBR] = &Simulator::Evaluate_MXDBR;
  EvalTable[KEBR] = &Simulator::Evaluate_KEBR;
  EvalTable[CEBR] = &Simulator::Evaluate_CEBR;
  EvalTable[AEBR] = &Simulator::Evaluate_AEBR;
  EvalTable[SEBR] = &Simulator::Evaluate_SEBR;
  EvalTable[MDEBR] = &Simulator::Evaluate_MDEBR;
  EvalTable[DEBR] = &Simulator::Evaluate_DEBR;
  EvalTable[MAEBR] = &Simulator::Evaluate_MAEBR;
  EvalTable[MSEBR] = &Simulator::Evaluate_MSEBR;
  EvalTable[LPDBR] = &Simulator::Evaluate_LPDBR;
  EvalTable[LNDBR] = &Simulator::Evaluate_LNDBR;
  EvalTable[LTDBR] = &Simulator::Evaluate_LTDBR;
  EvalTable[LCDBR] = &Simulator::Evaluate_LCDBR;
  EvalTable[SQEBR] = &Simulator::Evaluate_SQEBR;
  EvalTable[SQDBR] = &Simulator::Evaluate_SQDBR;
  EvalTable[SQXBR] = &Simulator::Evaluate_SQXBR;
  EvalTable[MEEBR] = &Simulator::Evaluate_MEEBR;
  EvalTable[KDBR] = &Simulator::Evaluate_KDBR;
  EvalTable[CDBR] = &Simulator::Evaluate_CDBR;
  EvalTable[ADBR] = &Simulator::Evaluate_ADBR;
  EvalTable[SDBR] = &Simulator::Evaluate_SDBR;
  EvalTable[MDBR] = &Simulator::Evaluate_MDBR;
  EvalTable[DDBR] = &Simulator::Evaluate_DDBR;
  EvalTable[MADBR] = &Simulator::Evaluate_MADBR;
  EvalTable[MSDBR] = &Simulator::Evaluate_MSDBR;
  EvalTable[LPXBR] = &Simulator::Evaluate_LPXBR;
  EvalTable[LNXBR] = &Simulator::Evaluate_LNXBR;
  EvalTable[LTXBR] = &Simulator::Evaluate_LTXBR;
  EvalTable[LCXBR] = &Simulator::Evaluate_LCXBR;
  EvalTable[LEDBRA] = &Simulator::Evaluate_LEDBRA;
  EvalTable[LDXBRA] = &Simulator::Evaluate_LDXBRA;
  EvalTable[LEXBRA] = &Simulator::Evaluate_LEXBRA;
  EvalTable[FIXBRA] = &Simulator::Evaluate_FIXBRA;
  EvalTable[KXBR] = &Simulator::Evaluate_KXBR;
  EvalTable[CXBR] = &Simulator::Evaluate_CXBR;
  EvalTable[AXBR] = &Simulator::Evaluate_AXBR;
  EvalTable[SXBR] = &Simulator::Evaluate_SXBR;
  EvalTable[MXBR] = &Simulator::Evaluate_MXBR;
  EvalTable[DXBR] = &Simulator::Evaluate_DXBR;
  EvalTable[TBEDR] = &Simulator::Evaluate_TBEDR;
  EvalTable[TBDR] = &Simulator::Evaluate_TBDR;
  EvalTable[DIEBR] = &Simulator::Evaluate_DIEBR;
  EvalTable[FIEBRA] = &Simulator::Evaluate_FIEBRA;
  EvalTable[THDER] = &Simulator::Evaluate_THDER;
  EvalTable[THDR] = &Simulator::Evaluate_THDR;
  EvalTable[DIDBR] = &Simulator::Evaluate_DIDBR;
  EvalTable[FIDBRA] = &Simulator::Evaluate_FIDBRA;
  EvalTable[LXR] = &Simulator::Evaluate_LXR;
  EvalTable[LPDFR] = &Simulator::Evaluate_LPDFR;
  EvalTable[LNDFR] = &Simulator::Evaluate_LNDFR;
  EvalTable[LCDFR] = &Simulator::Evaluate_LCDFR;
  EvalTable[LZER] = &Simulator::Evaluate_LZER;
  EvalTable[LZDR] = &Simulator::Evaluate_LZDR;
  EvalTable[LZXR] = &Simulator::Evaluate_LZXR;
  EvalTable[SFPC] = &Simulator::Evaluate_SFPC;
  EvalTable[SFASR] = &Simulator::Evaluate_SFASR;
  EvalTable[EFPC] = &Simulator::Evaluate_EFPC;
  EvalTable[CELFBR] = &Simulator::Evaluate_CELFBR;
  EvalTable[CDLFBR] = &Simulator::Evaluate_CDLFBR;
  EvalTable[CXLFBR] = &Simulator::Evaluate_CXLFBR;
  EvalTable[CEFBRA] = &Simulator::Evaluate_CEFBRA;
  EvalTable[CDFBRA] = &Simulator::Evaluate_CDFBRA;
  EvalTable[CXFBRA] = &Simulator::Evaluate_CXFBRA;
  EvalTable[CFEBRA] = &Simulator::Evaluate_CFEBRA;
  EvalTable[CFDBRA] = &Simulator::Evaluate_CFDBRA;
  EvalTable[CFXBRA] = &Simulator::Evaluate_CFXBRA;
  EvalTable[CLFEBR] = &Simulator::Evaluate_CLFEBR;
  EvalTable[CLFDBR] = &Simulator::Evaluate_CLFDBR;
  EvalTable[CLFXBR] = &Simulator::Evaluate_CLFXBR;
  EvalTable[CELGBR] = &Simulator::Evaluate_CELGBR;
  EvalTable[CDLGBR] = &Simulator::Evaluate_CDLGBR;
  EvalTable[CXLGBR] = &Simulator::Evaluate_CXLGBR;
  EvalTable[CEGBRA] = &Simulator::Evaluate_CEGBRA;
  EvalTable[CDGBRA] = &Simulator::Evaluate_CDGBRA;
  EvalTable[CXGBRA] = &Simulator::Evaluate_CXGBRA;
  EvalTable[CGEBRA] = &Simulator::Evaluate_CGEBRA;
  EvalTable[CGDBRA] = &Simulator::Evaluate_CGDBRA;
  EvalTable[CGXBRA] = &Simulator::Evaluate_CGXBRA;
  EvalTable[CLGEBR] = &Simulator::Evaluate_CLGEBR;
  EvalTable[CLGDBR] = &Simulator::Evaluate_CLGDBR;
  EvalTable[CFER] = &Simulator::Evaluate_CFER;
  EvalTable[CFDR] = &Simulator::Evaluate_CFDR;
  EvalTable[CFXR] = &Simulator::Evaluate_CFXR;
  EvalTable[LDGR] = &Simulator::Evaluate_LDGR;
  EvalTable[CGER] = &Simulator::Evaluate_CGER;
  EvalTable[CGDR] = &Simulator::Evaluate_CGDR;
  EvalTable[CGXR] = &Simulator::Evaluate_CGXR;
  EvalTable[LGDR] = &Simulator::Evaluate_LGDR;
  EvalTable[MDTR] = &Simulator::Evaluate_MDTR;
  EvalTable[MDTRA] = &Simulator::Evaluate_MDTRA;
  EvalTable[DDTRA] = &Simulator::Evaluate_DDTRA;
  EvalTable[ADTRA] = &Simulator::Evaluate_ADTRA;
  EvalTable[SDTRA] = &Simulator::Evaluate_SDTRA;
  EvalTable[LDETR] = &Simulator::Evaluate_LDETR;
  EvalTable[LEDTR] = &Simulator::Evaluate_LEDTR;
  EvalTable[LTDTR] = &Simulator::Evaluate_LTDTR;
  EvalTable[FIDTR] = &Simulator::Evaluate_FIDTR;
  EvalTable[MXTRA] = &Simulator::Evaluate_MXTRA;
  EvalTable[DXTRA] = &Simulator::Evaluate_DXTRA;
  EvalTable[AXTRA] = &Simulator::Evaluate_AXTRA;
  EvalTable[SXTRA] = &Simulator::Evaluate_SXTRA;
  EvalTable[LXDTR] = &Simulator::Evaluate_LXDTR;
  EvalTable[LDXTR] = &Simulator::Evaluate_LDXTR;
  EvalTable[LTXTR] = &Simulator::Evaluate_LTXTR;
  EvalTable[FIXTR] = &Simulator::Evaluate_FIXTR;
  EvalTable[KDTR] = &Simulator::Evaluate_KDTR;
  EvalTable[CGDTRA] = &Simulator::Evaluate_CGDTRA;
  EvalTable[CUDTR] = &Simulator::Evaluate_CUDTR;
  EvalTable[CDTR] = &Simulator::Evaluate_CDTR;
  EvalTable[EEDTR] = &Simulator::Evaluate_EEDTR;
  EvalTable[ESDTR] = &Simulator::Evaluate_ESDTR;
  EvalTable[KXTR] = &Simulator::Evaluate_KXTR;
  EvalTable[CGXTRA] = &Simulator::Evaluate_CGXTRA;
  EvalTable[CUXTR] = &Simulator::Evaluate_CUXTR;
  EvalTable[CSXTR] = &Simulator::Evaluate_CSXTR;
  EvalTable[CXTR] = &Simulator::Evaluate_CXTR;
  EvalTable[EEXTR] = &Simulator::Evaluate_EEXTR;
  EvalTable[ESXTR] = &Simulator::Evaluate_ESXTR;
  EvalTable[CDGTRA] = &Simulator::Evaluate_CDGTRA;
  EvalTable[CDUTR] = &Simulator::Evaluate_CDUTR;
  EvalTable[CDSTR] = &Simulator::Evaluate_CDSTR;
  EvalTable[CEDTR] = &Simulator::Evaluate_CEDTR;
  EvalTable[QADTR] = &Simulator::Evaluate_QADTR;
  EvalTable[IEDTR] = &Simulator::Evaluate_IEDTR;
  EvalTable[RRDTR] = &Simulator::Evaluate_RRDTR;
  EvalTable[CXGTRA] = &Simulator::Evaluate_CXGTRA;
  EvalTable[CXUTR] = &Simulator::Evaluate_CXUTR;
  EvalTable[CXSTR] = &Simulator::Evaluate_CXSTR;
  EvalTable[CEXTR] = &Simulator::Evaluate_CEXTR;
  EvalTable[QAXTR] = &Simulator::Evaluate_QAXTR;
  EvalTable[IEXTR] = &Simulator::Evaluate_IEXTR;
  EvalTable[RRXTR] = &Simulator::Evaluate_RRXTR;
  EvalTable[LPGR] = &Simulator::Evaluate_LPGR;
  EvalTable[LNGR] = &Simulator::Evaluate_LNGR;
  EvalTable[LTGR] = &Simulator::Evaluate_LTGR;
  EvalTable[LCGR] = &Simulator::Evaluate_LCGR;
  EvalTable[LGR] = &Simulator::Evaluate_LGR;
  EvalTable[LGBR] = &Simulator::Evaluate_LGBR;
  EvalTable[LGHR] = &Simulator::Evaluate_LGHR;
  EvalTable[AGR] = &Simulator::Evaluate_AGR;
  EvalTable[SGR] = &Simulator::Evaluate_SGR;
  EvalTable[ALGR] = &Simulator::Evaluate_ALGR;
  EvalTable[SLGR] = &Simulator::Evaluate_SLGR;
  EvalTable[MSGR] = &Simulator::Evaluate_MSGR;
  EvalTable[MSGRKC] = &Simulator::Evaluate_MSGRKC;
  EvalTable[DSGR] = &Simulator::Evaluate_DSGR;
  EvalTable[LRVGR] = &Simulator::Evaluate_LRVGR;
  EvalTable[LPGFR] = &Simulator::Evaluate_LPGFR;
  EvalTable[LNGFR] = &Simulator::Evaluate_LNGFR;
  EvalTable[LTGFR] = &Simulator::Evaluate_LTGFR;
  EvalTable[LCGFR] = &Simulator::Evaluate_LCGFR;
  EvalTable[LGFR] = &Simulator::Evaluate_LGFR;
  EvalTable[LLGFR] = &Simulator::Evaluate_LLGFR;
  EvalTable[LLGTR] = &Simulator::Evaluate_LLGTR;
  EvalTable[AGFR] = &Simulator::Evaluate_AGFR;
  EvalTable[SGFR] = &Simulator::Evaluate_SGFR;
  EvalTable[ALGFR] = &Simulator::Evaluate_ALGFR;
  EvalTable[SLGFR] = &Simulator::Evaluate_SLGFR;
  EvalTable[MSGFR] = &Simulator::Evaluate_MSGFR;
  EvalTable[DSGFR] = &Simulator::Evaluate_DSGFR;
  EvalTable[KMAC] = &Simulator::Evaluate_KMAC;
  EvalTable[LRVR] = &Simulator::Evaluate_LRVR;
  EvalTable[CGR] = &Simulator::Evaluate_CGR;
  EvalTable[CLGR] = &Simulator::Evaluate_CLGR;
  EvalTable[LBR] = &Simulator::Evaluate_LBR;
  EvalTable[LHR] = &Simulator::Evaluate_LHR;
  EvalTable[KMF] = &Simulator::Evaluate_KMF;
  EvalTable[KMO] = &Simulator::Evaluate_KMO;
  EvalTable[PCC] = &Simulator::Evaluate_PCC;
  EvalTable[KMCTR] = &Simulator::Evaluate_KMCTR;
  EvalTable[KM] = &Simulator::Evaluate_KM;
  EvalTable[KMC] = &Simulator::Evaluate_KMC;
  EvalTable[CGFR] = &Simulator::Evaluate_CGFR;
  EvalTable[KIMD] = &Simulator::Evaluate_KIMD;
  EvalTable[KLMD] = &Simulator::Evaluate_KLMD;
  EvalTable[CFDTR] = &Simulator::Evaluate_CFDTR;
  EvalTable[CLGDTR] = &Simulator::Evaluate_CLGDTR;
  EvalTable[CLFDTR] = &Simulator::Evaluate_CLFDTR;
  EvalTable[BCTGR] = &Simulator::Evaluate_BCTGR;
  EvalTable[CFXTR] = &Simulator::Evaluate_CFXTR;
  EvalTable[CLFXTR] = &Simulator::Evaluate_CLFXTR;
  EvalTable[CDFTR] = &Simulator::Evaluate_CDFTR;
  EvalTable[CDLGTR] = &Simulator::Evaluate_CDLGTR;
  EvalTable[CDLFTR] = &Simulator::Evaluate_CDLFTR;
  EvalTable[CXFTR] = &Simulator::Evaluate_CXFTR;
  EvalTable[CXLGTR] = &Simulator::Evaluate_CXLGTR;
  EvalTable[CXLFTR] = &Simulator::Evaluate_CXLFTR;
  EvalTable[CGRT] = &Simulator::Evaluate_CGRT;
  EvalTable[NGR] = &Simulator::Evaluate_NGR;
  EvalTable[OGR] = &Simulator::Evaluate_OGR;
  EvalTable[XGR] = &Simulator::Evaluate_XGR;
  EvalTable[FLOGR] = &Simulator::Evaluate_FLOGR;
  EvalTable[LLGCR] = &Simulator::Evaluate_LLGCR;
  EvalTable[LLGHR] = &Simulator::Evaluate_LLGHR;
  EvalTable[MLGR] = &Simulator::Evaluate_MLGR;
  EvalTable[DLGR] = &Simulator::Evaluate_DLGR;
  EvalTable[ALCGR] = &Simulator::Evaluate_ALCGR;
  EvalTable[SLBGR] = &Simulator::Evaluate_SLBGR;
  EvalTable[EPSW] = &Simulator::Evaluate_EPSW;
  EvalTable[TRTT] = &Simulator::Evaluate_TRTT;
  EvalTable[TRTO] = &Simulator::Evaluate_TRTO;
  EvalTable[TROT] = &Simulator::Evaluate_TROT;
  EvalTable[TROO] = &Simulator::Evaluate_TROO;
  EvalTable[LLCR] = &Simulator::Evaluate_LLCR;
  EvalTable[LLHR] = &Simulator::Evaluate_LLHR;
  EvalTable[MLR] = &Simulator::Evaluate_MLR;
  EvalTable[DLR] = &Simulator::Evaluate_DLR;
  EvalTable[ALCR] = &Simulator::Evaluate_ALCR;
  EvalTable[SLBR] = &Simulator::Evaluate_SLBR;
  EvalTable[CU14] = &Simulator::Evaluate_CU14;
  EvalTable[CU24] = &Simulator::Evaluate_CU24;
  EvalTable[CU41] = &Simulator::Evaluate_CU41;
  EvalTable[CU42] = &Simulator::Evaluate_CU42;
  EvalTable[TRTRE] = &Simulator::Evaluate_TRTRE;
  EvalTable[SRSTU] = &Simulator::Evaluate_SRSTU;
  EvalTable[TRTE] = &Simulator::Evaluate_TRTE;
  EvalTable[AHHHR] = &Simulator::Evaluate_AHHHR;
  EvalTable[SHHHR] = &Simulator::Evaluate_SHHHR;
  EvalTable[ALHHHR] = &Simulator::Evaluate_ALHHHR;
  EvalTable[SLHHHR] = &Simulator::Evaluate_SLHHHR;
  EvalTable[CHHR] = &Simulator::Evaluate_CHHR;
  EvalTable[AHHLR] = &Simulator::Evaluate_AHHLR;
  EvalTable[SHHLR] = &Simulator::Evaluate_SHHLR;
  EvalTable[ALHHLR] = &Simulator::Evaluate_ALHHLR;
  EvalTable[SLHHLR] = &Simulator::Evaluate_SLHHLR;
  EvalTable[CHLR] = &Simulator::Evaluate_CHLR;
  EvalTable[POPCNT_Z] = &Simulator::Evaluate_POPCNT_Z;
  EvalTable[LOCGR] = &Simulator::Evaluate_LOCGR;
  EvalTable[NGRK] = &Simulator::Evaluate_NGRK;
  EvalTable[OGRK] = &Simulator::Evaluate_OGRK;
  EvalTable[XGRK] = &Simulator::Evaluate_XGRK;
  EvalTable[AGRK] = &Simulator::Evaluate_AGRK;
  EvalTable[SGRK] = &Simulator::Evaluate_SGRK;
  EvalTable[ALGRK] = &Simulator::Evaluate_ALGRK;
  EvalTable[SLGRK] = &Simulator::Evaluate_SLGRK;
  EvalTable[LOCR] = &Simulator::Evaluate_LOCR;
  EvalTable[NRK] = &Simulator::Evaluate_NRK;
  EvalTable[ORK] = &Simulator::Evaluate_ORK;
  EvalTable[XRK] = &Simulator::Evaluate_XRK;
  EvalTable[ARK] = &Simulator::Evaluate_ARK;
  EvalTable[SRK] = &Simulator::Evaluate_SRK;
  EvalTable[ALRK] = &Simulator::Evaluate_ALRK;
  EvalTable[SLRK] = &Simulator::Evaluate_SLRK;
  EvalTable[LTG] = &Simulator::Evaluate_LTG;
  EvalTable[LG] = &Simulator::Evaluate_LG;
  EvalTable[CVBY] = &Simulator::Evaluate_CVBY;
  EvalTable[AG] = &Simulator::Evaluate_AG;
  EvalTable[SG] = &Simulator::Evaluate_SG;
  EvalTable[ALG] = &Simulator::Evaluate_ALG;
  EvalTable[SLG] = &Simulator::Evaluate_SLG;
  EvalTable[MSG] = &Simulator::Evaluate_MSG;
  EvalTable[DSG] = &Simulator::Evaluate_DSG;
  EvalTable[CVBG] = &Simulator::Evaluate_CVBG;
  EvalTable[LRVG] = &Simulator::Evaluate_LRVG;
  EvalTable[LT] = &Simulator::Evaluate_LT;
  EvalTable[LGF] = &Simulator::Evaluate_LGF;
  EvalTable[LGH] = &Simulator::Evaluate_LGH;
  EvalTable[LLGF] = &Simulator::Evaluate_LLGF;
  EvalTable[LLGT] = &Simulator::Evaluate_LLGT;
  EvalTable[AGF] = &Simulator::Evaluate_AGF;
  EvalTable[SGF] = &Simulator::Evaluate_SGF;
  EvalTable[ALGF] = &Simulator::Evaluate_ALGF;
  EvalTable[SLGF] = &Simulator::Evaluate_SLGF;
  EvalTable[MSGF] = &Simulator::Evaluate_MSGF;
  EvalTable[DSGF] = &Simulator::Evaluate_DSGF;
  EvalTable[LRV] = &Simulator::Evaluate_LRV;
  EvalTable[LRVH] = &Simulator::Evaluate_LRVH;
  EvalTable[CG] = &Simulator::Evaluate_CG;
  EvalTable[CLG] = &Simulator::Evaluate_CLG;
  EvalTable[STG] = &Simulator::Evaluate_STG;
  EvalTable[NTSTG] = &Simulator::Evaluate_NTSTG;
  EvalTable[CVDY] = &Simulator::Evaluate_CVDY;
  EvalTable[CVDG] = &Simulator::Evaluate_CVDG;
  EvalTable[STRVG] = &Simulator::Evaluate_STRVG;
  EvalTable[CGF] = &Simulator::Evaluate_CGF;
  EvalTable[CLGF] = &Simulator::Evaluate_CLGF;
  EvalTable[LTGF] = &Simulator::Evaluate_LTGF;
  EvalTable[CGH] = &Simulator::Evaluate_CGH;
  EvalTable[PFD] = &Simulator::Evaluate_PFD;
  EvalTable[STRV] = &Simulator::Evaluate_STRV;
  EvalTable[STRVH] = &Simulator::Evaluate_STRVH;
  EvalTable[BCTG] = &Simulator::Evaluate_BCTG;
  EvalTable[STY] = &Simulator::Evaluate_STY;
  EvalTable[MSY] = &Simulator::Evaluate_MSY;
  EvalTable[MSC] = &Simulator::Evaluate_MSC;
  EvalTable[NY] = &Simulator::Evaluate_NY;
  EvalTable[CLY] = &Simulator::Evaluate_CLY;
  EvalTable[OY] = &Simulator::Evaluate_OY;
  EvalTable[XY] = &Simulator::Evaluate_XY;
  EvalTable[LY] = &Simulator::Evaluate_LY;
  EvalTable[CY] = &Simulator::Evaluate_CY;
  EvalTable[AY] = &Simulator::Evaluate_AY;
  EvalTable[SY] = &Simulator::Evaluate_SY;
  EvalTable[MFY] = &Simulator::Evaluate_MFY;
  EvalTable[ALY] = &Simulator::Evaluate_ALY;
  EvalTable[SLY] = &Simulator::Evaluate_SLY;
  EvalTable[STHY] = &Simulator::Evaluate_STHY;
  EvalTable[LAY] = &Simulator::Evaluate_LAY;
  EvalTable[STCY] = &Simulator::Evaluate_STCY;
  EvalTable[ICY] = &Simulator::Evaluate_ICY;
  EvalTable[LAEY] = &Simulator::Evaluate_LAEY;
  EvalTable[LB] = &Simulator::Evaluate_LB;
  EvalTable[LGB] = &Simulator::Evaluate_LGB;
  EvalTable[LHY] = &Simulator::Evaluate_LHY;
  EvalTable[CHY] = &Simulator::Evaluate_CHY;
  EvalTable[AHY] = &Simulator::Evaluate_AHY;
  EvalTable[SHY] = &Simulator::Evaluate_SHY;
  EvalTable[MHY] = &Simulator::Evaluate_MHY;
  EvalTable[NG] = &Simulator::Evaluate_NG;
  EvalTable[OG] = &Simulator::Evaluate_OG;
  EvalTable[XG] = &Simulator::Evaluate_XG;
  EvalTable[LGAT] = &Simulator::Evaluate_LGAT;
  EvalTable[MLG] = &Simulator::Evaluate_MLG;
  EvalTable[DLG] = &Simulator::Evaluate_DLG;
  EvalTable[ALCG] = &Simulator::Evaluate_ALCG;
  EvalTable[SLBG] = &Simulator::Evaluate_SLBG;
  EvalTable[STPQ] = &Simulator::Evaluate_STPQ;
  EvalTable[LPQ] = &Simulator::Evaluate_LPQ;
  EvalTable[LLGC] = &Simulator::Evaluate_LLGC;
  EvalTable[LLGH] = &Simulator::Evaluate_LLGH;
  EvalTable[LLC] = &Simulator::Evaluate_LLC;
  EvalTable[LLH] = &Simulator::Evaluate_LLH;
  EvalTable[ML] = &Simulator::Evaluate_ML;
  EvalTable[DL] = &Simulator::Evaluate_DL;
  EvalTable[ALC] = &Simulator::Evaluate_ALC;
  EvalTable[SLB] = &Simulator::Evaluate_SLB;
  EvalTable[LLGTAT] = &Simulator::Evaluate_LLGTAT;
  EvalTable[LLGFAT] = &Simulator::Evaluate_LLGFAT;
  EvalTable[LAT] = &Simulator::Evaluate_LAT;
  EvalTable[LBH] = &Simulator::Evaluate_LBH;
  EvalTable[LLCH] = &Simulator::Evaluate_LLCH;
  EvalTable[STCH] = &Simulator::Evaluate_STCH;
  EvalTable[LHH] = &Simulator::Evaluate_LHH;
  EvalTable[LLHH] = &Simulator::Evaluate_LLHH;
  EvalTable[STHH] = &Simulator::Evaluate_STHH;
  EvalTable[LFHAT] = &Simulator::Evaluate_LFHAT;
  EvalTable[LFH] = &Simulator::Evaluate_LFH;
  EvalTable[STFH] = &Simulator::Evaluate_STFH;
  EvalTable[CHF] = &Simulator::Evaluate_CHF;
  EvalTable[MVCDK] = &Simulator::Evaluate_MVCDK;
  EvalTable[MVHHI] = &Simulator::Evaluate_MVHHI;
  EvalTable[MVGHI] = &Simulator::Evaluate_MVGHI;
  EvalTable[MVHI] = &Simulator::Evaluate_MVHI;
  EvalTable[CHHSI] = &Simulator::Evaluate_CHHSI;
  EvalTable[CGHSI] = &Simulator::Evaluate_CGHSI;
  EvalTable[CHSI] = &Simulator::Evaluate_CHSI;
  EvalTable[CLFHSI] = &Simulator::Evaluate_CLFHSI;
  EvalTable[TBEGIN] = &Simulator::Evaluate_TBEGIN;
  EvalTable[TBEGINC] = &Simulator::Evaluate_TBEGINC;
  EvalTable[LMG] = &Simulator::Evaluate_LMG;
  EvalTable[SRAG] = &Simulator::Evaluate_SRAG;
  EvalTable[SLAG] = &Simulator::Evaluate_SLAG;
  EvalTable[SRLG] = &Simulator::Evaluate_SRLG;
  EvalTable[SLLG] = &Simulator::Evaluate_SLLG;
  EvalTable[CSY] = &Simulator::Evaluate_CSY;
  EvalTable[RLLG] = &Simulator::Evaluate_RLLG;
  EvalTable[RLL] = &Simulator::Evaluate_RLL;
  EvalTable[STMG] = &Simulator::Evaluate_STMG;
  EvalTable[STMH] = &Simulator::Evaluate_STMH;
  EvalTable[STCMH] = &Simulator::Evaluate_STCMH;
  EvalTable[STCMY] = &Simulator::Evaluate_STCMY;
  EvalTable[CDSY] = &Simulator::Evaluate_CDSY;
  EvalTable[CDSG] = &Simulator::Evaluate_CDSG;
  EvalTable[BXHG] = &Simulator::Evaluate_BXHG;
  EvalTable[BXLEG] = &Simulator::Evaluate_BXLEG;
  EvalTable[ECAG] = &Simulator::Evaluate_ECAG;
  EvalTable[TMY] = &Simulator::Evaluate_TMY;
  EvalTable[MVIY] = &Simulator::Evaluate_MVIY;
  EvalTable[NIY] = &Simulator::Evaluate_NIY;
  EvalTable[CLIY] = &Simulator::Evaluate_CLIY;
  EvalTable[OIY] = &Simulator::Evaluate_OIY;
  EvalTable[XIY] = &Simulator::Evaluate_XIY;
  EvalTable[ASI] = &Simulator::Evaluate_ASI;
  EvalTable[ALSI] = &Simulator::Evaluate_ALSI;
  EvalTable[AGSI] = &Simulator::Evaluate_AGSI;
  EvalTable[ALGSI] = &Simulator::Evaluate_ALGSI;
  EvalTable[ICMH] = &Simulator::Evaluate_ICMH;
  EvalTable[ICMY] = &Simulator::Evaluate_ICMY;
  EvalTable[MVCLU] = &Simulator::Evaluate_MVCLU;
  EvalTable[CLCLU] = &Simulator::Evaluate_CLCLU;
  EvalTable[STMY] = &Simulator::Evaluate_STMY;
  EvalTable[LMH] = &Simulator::Evaluate_LMH;
  EvalTable[LMY] = &Simulator::Evaluate_LMY;
  EvalTable[TP] = &Simulator::Evaluate_TP;
  EvalTable[SRAK] = &Simulator::Evaluate_SRAK;
  EvalTable[SLAK] = &Simulator::Evaluate_SLAK;
  EvalTable[SRLK] = &Simulator::Evaluate_SRLK;
  EvalTable[SLLK] = &Simulator::Evaluate_SLLK;
  EvalTable[LOCG] = &Simulator::Evaluate_LOCG;
  EvalTable[STOCG] = &Simulator::Evaluate_STOCG;
  EvalTable[LANG] = &Simulator::Evaluate_LANG;
  EvalTable[LAOG] = &Simulator::Evaluate_LAOG;
  EvalTable[LAXG] = &Simulator::Evaluate_LAXG;
  EvalTable[LAAG] = &Simulator::Evaluate_LAAG;
  EvalTable[LAALG] = &Simulator::Evaluate_LAALG;
  EvalTable[LOC] = &Simulator::Evaluate_LOC;
  EvalTable[STOC] = &Simulator::Evaluate_STOC;
  EvalTable[LAN] = &Simulator::Evaluate_LAN;
  EvalTable[LAO] = &Simulator::Evaluate_LAO;
  EvalTable[LAX] = &Simulator::Evaluate_LAX;
  EvalTable[LAA] = &Simulator::Evaluate_LAA;
  EvalTable[LAAL] = &Simulator::Evaluate_LAAL;
  EvalTable[BRXHG] = &Simulator::Evaluate_BRXHG;
  EvalTable[BRXLG] = &Simulator::Evaluate_BRXLG;
  EvalTable[RISBLG] = &Simulator::Evaluate_RISBLG;
  EvalTable[RNSBG] = &Simulator::Evaluate_RNSBG;
  EvalTable[RISBG] = &Simulator::Evaluate_RISBG;
  EvalTable[ROSBG] = &Simulator::Evaluate_ROSBG;
  EvalTable[RXSBG] = &Simulator::Evaluate_RXSBG;
  EvalTable[RISBGN] = &Simulator::Evaluate_RISBGN;
  EvalTable[RISBHG] = &Simulator::Evaluate_RISBHG;
  EvalTable[CGRJ] = &Simulator::Evaluate_CGRJ;
  EvalTable[CGIT] = &Simulator::Evaluate_CGIT;
  EvalTable[CIT] = &Simulator::Evaluate_CIT;
  EvalTable[CLFIT] = &Simulator::Evaluate_CLFIT;
  EvalTable[CGIJ] = &Simulator::Evaluate_CGIJ;
  EvalTable[CIJ] = &Simulator::Evaluate_CIJ;
  EvalTable[AHIK] = &Simulator::Evaluate_AHIK;
  EvalTable[AGHIK] = &Simulator::Evaluate_AGHIK;
  EvalTable[ALHSIK] = &Simulator::Evaluate_ALHSIK;
  EvalTable[ALGHSIK] = &Simulator::Evaluate_ALGHSIK;
  EvalTable[CGRB] = &Simulator::Evaluate_CGRB;
  EvalTable[CGIB] = &Simulator::Evaluate_CGIB;
  EvalTable[CIB] = &Simulator::Evaluate_CIB;
  EvalTable[LDEB] = &Simulator::Evaluate_LDEB;
  EvalTable[LXDB] = &Simulator::Evaluate_LXDB;
  EvalTable[LXEB] = &Simulator::Evaluate_LXEB;
  EvalTable[MXDB] = &Simulator::Evaluate_MXDB;
  EvalTable[KEB] = &Simulator::Evaluate_KEB;
  EvalTable[CEB] = &Simulator::Evaluate_CEB;
  EvalTable[AEB] = &Simulator::Evaluate_AEB;
  EvalTable[SEB] = &Simulator::Evaluate_SEB;
  EvalTable[MDEB] = &Simulator::Evaluate_MDEB;
  EvalTable[DEB] = &Simulator::Evaluate_DEB;
  EvalTable[MAEB] = &Simulator::Evaluate_MAEB;
  EvalTable[MSEB] = &Simulator::Evaluate_MSEB;
  EvalTable[TCEB] = &Simulator::Evaluate_TCEB;
  EvalTable[TCDB] = &Simulator::Evaluate_TCDB;
  EvalTable[TCXB] = &Simulator::Evaluate_TCXB;
  EvalTable[SQEB] = &Simulator::Evaluate_SQEB;
  EvalTable[SQDB] = &Simulator::Evaluate_SQDB;
  EvalTable[MEEB] = &Simulator::Evaluate_MEEB;
  EvalTable[KDB] = &Simulator::Evaluate_KDB;
  EvalTable[CDB] = &Simulator::Evaluate_CDB;
  EvalTable[ADB] = &Simulator::Evaluate_ADB;
  EvalTable[SDB] = &Simulator::Evaluate_SDB;
  EvalTable[MDB] = &Simulator::Evaluate_MDB;
  EvalTable[DDB] = &Simulator::Evaluate_DDB;
  EvalTable[MADB] = &Simulator::Evaluate_MADB;
  EvalTable[MSDB] = &Simulator::Evaluate_MSDB;
  EvalTable[SLDT] = &Simulator::Evaluate_SLDT;
  EvalTable[SRDT] = &Simulator::Evaluate_SRDT;
  EvalTable[SLXT] = &Simulator::Evaluate_SLXT;
  EvalTable[SRXT] = &Simulator::Evaluate_SRXT;
  EvalTable[TDCET] = &Simulator::Evaluate_TDCET;
  EvalTable[TDGET] = &Simulator::Evaluate_TDGET;
  EvalTable[TDCDT] = &Simulator::Evaluate_TDCDT;
  EvalTable[TDGDT] = &Simulator::Evaluate_TDGDT;
  EvalTable[TDCXT] = &Simulator::Evaluate_TDCXT;
  EvalTable[TDGXT] = &Simulator::Evaluate_TDGXT;
  EvalTable[LEY] = &Simulator::Evaluate_LEY;
  EvalTable[LDY] = &Simulator::Evaluate_LDY;
  EvalTable[STEY] = &Simulator::Evaluate_STEY;
  EvalTable[STDY] = &Simulator::Evaluate_STDY;
  EvalTable[CZDT] = &Simulator::Evaluate_CZDT;
  EvalTable[CZXT] = &Simulator::Evaluate_CZXT;
  EvalTable[CDZT] = &Simulator::Evaluate_CDZT;
  EvalTable[CXZT] = &Simulator::Evaluate_CXZT;
}  // NOLINT

Simulator::Simulator(Isolate* isolate) : isolate_(isolate) {
  i_cache_ = isolate_->simulator_i_cache();
  if (i_cache_ == nullptr) {
    i_cache_ = new base::CustomMatcherHashMap(&ICacheMatch);
    isolate_->set_simulator_i_cache(i_cache_);
  }
  Initialize(isolate);
// Set up simulator support first. Some of this information is needed to
// setup the architecture state.
#if V8_TARGET_ARCH_S390X
  size_t stack_size = FLAG_sim_stack_size * KB;
#else
  size_t stack_size = MB;  // allocate 1MB for stack
#endif
  stack_size += 2 * stack_protection_size_;
  stack_ = reinterpret_cast<char*>(malloc(stack_size));
  pc_modified_ = false;
  icount_ = 0;
  break_pc_ = nullptr;
  break_instr_ = 0;

// make sure our register type can hold exactly 4/8 bytes
#ifdef V8_TARGET_ARCH_S390X
  DCHECK_EQ(sizeof(intptr_t), 8);
#else
  DCHECK_EQ(sizeof(intptr_t), 4);
#endif
  // Set up architecture state.
  // All registers are initialized to zero to start with.
  for (int i = 0; i < kNumGPRs; i++) {
    registers_[i] = 0;
  }
  condition_reg_ = 0;
  special_reg_pc_ = 0;

  // Initializing FP registers.
  for (int i = 0; i < kNumFPRs; i++) {
    fp_registers_[i] = 0.0;
  }

  // The sp is initialized to point to the bottom (high address) of the
  // allocated stack area. To be safe in potential stack underflows we leave
  // some buffer below.
  registers_[sp] =
      reinterpret_cast<intptr_t>(stack_) + stack_size - stack_protection_size_;

  last_debugger_input_ = nullptr;
}

Simulator::~Simulator() { free(stack_); }

// When the generated code calls an external reference we need to catch that in
// the simulator.  The external reference will be a function compiled for the
// host architecture.  We need to call that function instead of trying to
// execute it with the simulator.  We do that by redirecting the external
// reference to a svc (Supervisor Call) instruction that is handled by
// the simulator.  We write the original destination of the jump just at a known
// offset from the svc instruction so the simulator knows what to call.
class Redirection {
 public:
  Redirection(Isolate* isolate, void* external_function,
              ExternalReference::Type type)
      : external_function_(external_function),
// we use TRAP4 here (0xBF22)
#if V8_TARGET_LITTLE_ENDIAN
        swi_instruction_(0x1000FFB2),
#else
        swi_instruction_(0xB2FF0000 | kCallRtRedirected),
#endif
        type_(type),
        next_(nullptr) {
    next_ = isolate->simulator_redirection();
    Simulator::current(isolate)->FlushICache(
        isolate->simulator_i_cache(),
        reinterpret_cast<void*>(&swi_instruction_), sizeof(FourByteInstr));
    isolate->set_simulator_redirection(this);
    if (ABI_USES_FUNCTION_DESCRIPTORS) {
      function_descriptor_[0] = reinterpret_cast<intptr_t>(&swi_instruction_);
      function_descriptor_[1] = 0;
      function_descriptor_[2] = 0;
    }
  }

  void* address() {
    if (ABI_USES_FUNCTION_DESCRIPTORS) {
      return reinterpret_cast<void*>(function_descriptor_);
    } else {
      return reinterpret_cast<void*>(&swi_instruction_);
    }
  }

  void* external_function() { return external_function_; }
  ExternalReference::Type type() { return type_; }

  static Redirection* Get(Isolate* isolate, void* external_function,
                          ExternalReference::Type type) {
    Redirection* current = isolate->simulator_redirection();
    for (; current != nullptr; current = current->next_) {
      if (current->external_function_ == external_function &&
          current->type_ == type) {
        return current;
      }
    }
    return new Redirection(isolate, external_function, type);
  }

  static Redirection* FromSwiInstruction(Instruction* swi_instruction) {
    char* addr_of_swi = reinterpret_cast<char*>(swi_instruction);
    char* addr_of_redirection =
        addr_of_swi - offsetof(Redirection, swi_instruction_);
    return reinterpret_cast<Redirection*>(addr_of_redirection);
  }

  static Redirection* FromAddress(void* address) {
    int delta = ABI_USES_FUNCTION_DESCRIPTORS
                    ? offsetof(Redirection, function_descriptor_)
                    : offsetof(Redirection, swi_instruction_);
    char* addr_of_redirection = reinterpret_cast<char*>(address) - delta;
    return reinterpret_cast<Redirection*>(addr_of_redirection);
  }

  static void* ReverseRedirection(intptr_t reg) {
    Redirection* redirection = FromAddress(reinterpret_cast<void*>(reg));
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
  intptr_t function_descriptor_[3];
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
  base::LockGuard<base::Mutex> lock_guard(
      isolate->simulator_redirection_mutex());
  Redirection* redirection = Redirection::Get(isolate, external_function, type);
  return redirection->address();
}

// Get the active Simulator for the current thread.
Simulator* Simulator::current(Isolate* isolate) {
  v8::internal::Isolate::PerIsolateThreadData* isolate_data =
      isolate->FindOrAllocatePerThreadDataForThisThread();
  DCHECK_NOT_NULL(isolate_data);

  Simulator* sim = isolate_data->simulator();
  if (sim == nullptr) {
    // TODO(146): delete the simulator object when a thread/isolate goes away.
    sim = new Simulator(isolate);
    isolate_data->set_simulator(sim);
  }
  return sim;
}

// Sets the register in the architecture state.
void Simulator::set_register(int reg, uint64_t value) {
  DCHECK((reg >= 0) && (reg < kNumGPRs));
  registers_[reg] = value;
}

// Get the register from the architecture state.
uint64_t Simulator::get_register(int reg) const {
  DCHECK((reg >= 0) && (reg < kNumGPRs));
  // Stupid code added to avoid bug in GCC.
  // See: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949
  if (reg >= kNumGPRs) return 0;
  // End stupid code.
  return registers_[reg];
}

template <typename T>
T Simulator::get_low_register(int reg) const {
  DCHECK((reg >= 0) && (reg < kNumGPRs));
  // Stupid code added to avoid bug in GCC.
  // See: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949
  if (reg >= kNumGPRs) return 0;
  // End stupid code.
  return static_cast<T>(registers_[reg] & 0xFFFFFFFF);
}

template <typename T>
T Simulator::get_high_register(int reg) const {
  DCHECK((reg >= 0) && (reg < kNumGPRs));
  // Stupid code added to avoid bug in GCC.
  // See: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949
  if (reg >= kNumGPRs) return 0;
  // End stupid code.
  return static_cast<T>(registers_[reg] >> 32);
}

void Simulator::set_low_register(int reg, uint32_t value) {
  uint64_t shifted_val = static_cast<uint64_t>(value);
  uint64_t orig_val = static_cast<uint64_t>(registers_[reg]);
  uint64_t result = (orig_val >> 32 << 32) | shifted_val;
  registers_[reg] = result;
}

void Simulator::set_high_register(int reg, uint32_t value) {
  uint64_t shifted_val = static_cast<uint64_t>(value) << 32;
  uint64_t orig_val = static_cast<uint64_t>(registers_[reg]);
  uint64_t result = (orig_val & 0xFFFFFFFF) | shifted_val;
  registers_[reg] = result;
}

double Simulator::get_double_from_register_pair(int reg) {
  DCHECK((reg >= 0) && (reg < kNumGPRs) && ((reg % 2) == 0));

  double dm_val = 0.0;
#if 0 && !V8_TARGET_ARCH_S390X  // doesn't make sense in 64bit mode
  // Read the bits from the unsigned integer register_[] array
  // into the double precision floating point value and return it.
  char buffer[sizeof(fp_registers_[0])];
  memcpy(buffer, &registers_[reg], 2 * sizeof(registers_[0]));
  memcpy(&dm_val, buffer, 2 * sizeof(registers_[0]));
#endif
  return (dm_val);
}

// Raw access to the PC register.
void Simulator::set_pc(intptr_t value) {
  pc_modified_ = true;
  special_reg_pc_ = value;
}

bool Simulator::has_bad_pc() const {
  return ((special_reg_pc_ == bad_lr) || (special_reg_pc_ == end_sim_pc));
}

// Raw access to the PC register without the special adjustment when reading.
intptr_t Simulator::get_pc() const { return special_reg_pc_; }

// Runtime FP routines take:
// - two double arguments
// - one double argument and zero or one integer arguments.
// All are consructed here from d1, d2 and r2.
void Simulator::GetFpArgs(double* x, double* y, intptr_t* z) {
  *x = get_double_from_d_register(0);
  *y = get_double_from_d_register(2);
  *z = get_register(2);
}

// The return value is in d0.
void Simulator::SetFpResult(const double& result) {
  set_d_register_from_double(0, result);
}

void Simulator::TrashCallerSaveRegisters() {
// We don't trash the registers with the return value.
#if 0  // A good idea to trash volatile registers, needs to be done
  registers_[2] = 0x50Bad4U;
  registers_[3] = 0x50Bad4U;
  registers_[12] = 0x50Bad4U;
#endif
}

uint32_t Simulator::ReadWU(intptr_t addr, Instruction* instr) {
  uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
  return *ptr;
}

int64_t Simulator::ReadW64(intptr_t addr, Instruction* instr) {
  int64_t* ptr = reinterpret_cast<int64_t*>(addr);
  return *ptr;
}

int32_t Simulator::ReadW(intptr_t addr, Instruction* instr) {
  int32_t* ptr = reinterpret_cast<int32_t*>(addr);
  return *ptr;
}

void Simulator::WriteW(intptr_t addr, uint32_t value, Instruction* instr) {
  uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
  *ptr = value;
  return;
}

void Simulator::WriteW(intptr_t addr, int32_t value, Instruction* instr) {
  int32_t* ptr = reinterpret_cast<int32_t*>(addr);
  *ptr = value;
  return;
}

uint16_t Simulator::ReadHU(intptr_t addr, Instruction* instr) {
  uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
  return *ptr;
}

int16_t Simulator::ReadH(intptr_t addr, Instruction* instr) {
  int16_t* ptr = reinterpret_cast<int16_t*>(addr);
  return *ptr;
}

void Simulator::WriteH(intptr_t addr, uint16_t value, Instruction* instr) {
  uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
  *ptr = value;
  return;
}

void Simulator::WriteH(intptr_t addr, int16_t value, Instruction* instr) {
  int16_t* ptr = reinterpret_cast<int16_t*>(addr);
  *ptr = value;
  return;
}

uint8_t Simulator::ReadBU(intptr_t addr) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  return *ptr;
}

int8_t Simulator::ReadB(intptr_t addr) {
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  return *ptr;
}

void Simulator::WriteB(intptr_t addr, uint8_t value) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  *ptr = value;
}

void Simulator::WriteB(intptr_t addr, int8_t value) {
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  *ptr = value;
}

int64_t Simulator::ReadDW(intptr_t addr) {
  int64_t* ptr = reinterpret_cast<int64_t*>(addr);
  return *ptr;
}

void Simulator::WriteDW(intptr_t addr, int64_t value) {
  int64_t* ptr = reinterpret_cast<int64_t*>(addr);
  *ptr = value;
  return;
}

/**
 * Reads a double value from memory at given address.
 */
double Simulator::ReadDouble(intptr_t addr) {
  double* ptr = reinterpret_cast<double*>(addr);
  return *ptr;
}

float Simulator::ReadFloat(intptr_t addr) {
  float* ptr = reinterpret_cast<float*>(addr);
  return *ptr;
}

// Returns the limit of the stack area to enable checking for stack overflows.
uintptr_t Simulator::StackLimit(uintptr_t c_limit) const {
  // The simulator uses a separate JS stack. If we have exhausted the C stack,
  // we also drop down the JS limit to reflect the exhaustion on the JS stack.
  if (GetCurrentStackPosition() < c_limit) {
    return reinterpret_cast<uintptr_t>(get_sp());
  }

  // Otherwise the limit is the JS stack. Leave a safety margin to prevent
  // overrunning the stack when pushing values.
  return reinterpret_cast<uintptr_t>(stack_) + stack_protection_size_;
}

// Unsupported instructions use Format to print an error and stop execution.
void Simulator::Format(Instruction* instr, const char* format) {
  PrintF("Simulator found unsupported instruction:\n 0x%08" V8PRIxPTR ": %s\n",
         reinterpret_cast<intptr_t>(instr), format);
  UNIMPLEMENTED();
}

// Calculate C flag value for additions.
bool Simulator::CarryFrom(int32_t left, int32_t right, int32_t carry) {
  uint32_t uleft = static_cast<uint32_t>(left);
  uint32_t uright = static_cast<uint32_t>(right);
  uint32_t urest = 0xffffffffU - uleft;

  return (uright > urest) ||
         (carry && (((uright + 1) > urest) || (uright > (urest - 1))));
}

// Calculate C flag value for subtractions.
bool Simulator::BorrowFrom(int32_t left, int32_t right) {
  uint32_t uleft = static_cast<uint32_t>(left);
  uint32_t uright = static_cast<uint32_t>(right);

  return (uright > uleft);
}

// Calculate V flag value for additions and subtractions.
template <typename T1>
bool Simulator::OverflowFromSigned(T1 alu_out, T1 left, T1 right,
                                   bool addition) {
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

#if V8_TARGET_ARCH_S390X
static void decodeObjectPair(ObjectPair* pair, intptr_t* x, intptr_t* y) {
  *x = reinterpret_cast<intptr_t>(pair->x);
  *y = reinterpret_cast<intptr_t>(pair->y);
}
#else
static void decodeObjectPair(ObjectPair* pair, intptr_t* x, intptr_t* y) {
#if V8_TARGET_BIG_ENDIAN
  *x = static_cast<int32_t>(*pair >> 32);
  *y = static_cast<int32_t>(*pair);
#else
  *x = static_cast<int32_t>(*pair);
  *y = static_cast<int32_t>(*pair >> 32);
#endif
}
#endif

// Calls into the V8 runtime.
typedef intptr_t (*SimulatorRuntimeCall)(intptr_t arg0, intptr_t arg1,
                                         intptr_t arg2, intptr_t arg3,
                                         intptr_t arg4, intptr_t arg5,
                                         intptr_t arg6, intptr_t arg7,
                                         intptr_t arg8);
typedef ObjectPair (*SimulatorRuntimePairCall)(intptr_t arg0, intptr_t arg1,
                                               intptr_t arg2, intptr_t arg3,
                                               intptr_t arg4, intptr_t arg5);

// These prototypes handle the four types of FP calls.
typedef int (*SimulatorRuntimeCompareCall)(double darg0, double darg1);
typedef double (*SimulatorRuntimeFPFPCall)(double darg0, double darg1);
typedef double (*SimulatorRuntimeFPCall)(double darg0);
typedef double (*SimulatorRuntimeFPIntCall)(double darg0, intptr_t arg0);

// This signature supports direct call in to API function native callback
// (refer to InvocationCallback in v8.h).
typedef void (*SimulatorRuntimeDirectApiCall)(intptr_t arg0);
typedef void (*SimulatorRuntimeProfilingApiCall)(intptr_t arg0, void* arg1);

// This signature supports direct call to accessor getter callback.
typedef void (*SimulatorRuntimeDirectGetterCall)(intptr_t arg0, intptr_t arg1);
typedef void (*SimulatorRuntimeProfilingGetterCall)(intptr_t arg0,
                                                    intptr_t arg1, void* arg2);

// Software interrupt instructions are used by the simulator to call into the
// C-based V8 runtime.
void Simulator::SoftwareInterrupt(Instruction* instr) {
  int svc = instr->SvcValue();
  switch (svc) {
    case kCallRtRedirected: {
      // Check if stack is aligned. Error if not aligned is reported below to
      // include information on the function called.
      bool stack_aligned =
          (get_register(sp) & (::v8::internal::FLAG_sim_stack_alignment - 1)) ==
          0;
      Redirection* redirection = Redirection::FromSwiInstruction(instr);
      const int kArgCount = 9;
      const int kRegisterArgCount = 5;
      int arg0_regnum = 2;
      intptr_t result_buffer = 0;
      bool uses_result_buffer =
          redirection->type() == ExternalReference::BUILTIN_CALL_PAIR &&
          !ABI_RETURNS_OBJECTPAIR_IN_REGS;
      if (uses_result_buffer) {
        result_buffer = get_register(r2);
        arg0_regnum++;
      }
      intptr_t arg[kArgCount];
      // First 5 arguments in registers r2-r6.
      for (int i = 0; i < kRegisterArgCount; i++) {
        arg[i] = get_register(arg0_regnum + i);
      }
      // Remaining arguments on stack
      intptr_t* stack_pointer = reinterpret_cast<intptr_t*>(get_register(sp));
      for (int i = kRegisterArgCount; i < kArgCount; i++) {
        arg[i] = stack_pointer[(kCalleeRegisterSaveAreaSize / kPointerSize) +
                               (i - kRegisterArgCount)];
      }
      STATIC_ASSERT(kArgCount == kRegisterArgCount + 4);
      STATIC_ASSERT(kMaxCParameters == 9);
      bool fp_call =
          (redirection->type() == ExternalReference::BUILTIN_FP_FP_CALL) ||
          (redirection->type() == ExternalReference::BUILTIN_COMPARE_CALL) ||
          (redirection->type() == ExternalReference::BUILTIN_FP_CALL) ||
          (redirection->type() == ExternalReference::BUILTIN_FP_INT_CALL);

      // Place the return address on the stack, making the call GC safe.
      *reinterpret_cast<intptr_t*>(get_register(sp) +
                                   kStackFrameRASlot * kPointerSize) =
          get_register(r14);

      intptr_t external =
          reinterpret_cast<intptr_t>(redirection->external_function());
      if (fp_call) {
        double dval0, dval1;  // one or two double parameters
        intptr_t ival;        // zero or one integer parameters
        int iresult = 0;      // integer return value
        double dresult = 0;   // double return value
        GetFpArgs(&dval0, &dval1, &ival);
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          SimulatorRuntimeCall generic_target =
              reinterpret_cast<SimulatorRuntimeCall>(external);
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
              PrintF("Call to host function at %p with args %f, %" V8PRIdPTR,
                     static_cast<void*>(FUNCTION_ADDR(generic_target)), dval0,
                     ival);
              break;
            default:
              UNREACHABLE();
              break;
          }
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08" V8PRIxPTR "\n",
                   static_cast<intptr_t>(get_register(sp)));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        switch (redirection->type()) {
          case ExternalReference::BUILTIN_COMPARE_CALL: {
            SimulatorRuntimeCompareCall target =
                reinterpret_cast<SimulatorRuntimeCompareCall>(external);
            iresult = target(dval0, dval1);
            set_register(r2, iresult);
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
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          switch (redirection->type()) {
            case ExternalReference::BUILTIN_COMPARE_CALL:
              PrintF("Returned %08x\n", iresult);
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
        // See callers of MacroAssembler::CallApiFunctionAndReturn for
        // explanation of register usage.
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF("Call to host function at %p args %08" V8PRIxPTR,
                 reinterpret_cast<void*>(external), arg[0]);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08" V8PRIxPTR "\n",
                   static_cast<intptr_t>(get_register(sp)));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        SimulatorRuntimeDirectApiCall target =
            reinterpret_cast<SimulatorRuntimeDirectApiCall>(external);
        target(arg[0]);
      } else if (redirection->type() == ExternalReference::PROFILING_API_CALL) {
        // See callers of MacroAssembler::CallApiFunctionAndReturn for
        // explanation of register usage.
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF("Call to host function at %p args %08" V8PRIxPTR
                 " %08" V8PRIxPTR,
                 reinterpret_cast<void*>(external), arg[0], arg[1]);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08" V8PRIxPTR "\n",
                   static_cast<intptr_t>(get_register(sp)));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        SimulatorRuntimeProfilingApiCall target =
            reinterpret_cast<SimulatorRuntimeProfilingApiCall>(external);
        target(arg[0], Redirection::ReverseRedirection(arg[1]));
      } else if (redirection->type() == ExternalReference::DIRECT_GETTER_CALL) {
        // See callers of MacroAssembler::CallApiFunctionAndReturn for
        // explanation of register usage.
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF("Call to host function at %p args %08" V8PRIxPTR
                 " %08" V8PRIxPTR,
                 reinterpret_cast<void*>(external), arg[0], arg[1]);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08" V8PRIxPTR "\n",
                   static_cast<intptr_t>(get_register(sp)));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        SimulatorRuntimeDirectGetterCall target =
            reinterpret_cast<SimulatorRuntimeDirectGetterCall>(external);
        if (!ABI_PASSES_HANDLES_IN_REGS) {
          arg[0] = *(reinterpret_cast<intptr_t*>(arg[0]));
        }
        target(arg[0], arg[1]);
      } else if (redirection->type() ==
                 ExternalReference::PROFILING_GETTER_CALL) {
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF("Call to host function at %p args %08" V8PRIxPTR
                 " %08" V8PRIxPTR " %08" V8PRIxPTR,
                 reinterpret_cast<void*>(external), arg[0], arg[1], arg[2]);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08" V8PRIxPTR "\n",
                   static_cast<intptr_t>(get_register(sp)));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        SimulatorRuntimeProfilingGetterCall target =
            reinterpret_cast<SimulatorRuntimeProfilingGetterCall>(external);
        if (!ABI_PASSES_HANDLES_IN_REGS) {
          arg[0] = *(reinterpret_cast<intptr_t*>(arg[0]));
        }
        target(arg[0], arg[1], Redirection::ReverseRedirection(arg[2]));
      } else {
        // builtin call.
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          SimulatorRuntimeCall target =
              reinterpret_cast<SimulatorRuntimeCall>(external);
          PrintF(
              "Call to host function at %p,\n"
              "\t\t\t\targs %08" V8PRIxPTR ", %08" V8PRIxPTR ", %08" V8PRIxPTR
              ", %08" V8PRIxPTR ", %08" V8PRIxPTR ", %08" V8PRIxPTR
              ", %08" V8PRIxPTR ", %08" V8PRIxPTR ", %08" V8PRIxPTR,
              static_cast<void*>(FUNCTION_ADDR(target)), arg[0], arg[1], arg[2],
              arg[3], arg[4], arg[5], arg[6], arg[7], arg[8]);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08" V8PRIxPTR "\n",
                   static_cast<intptr_t>(get_register(sp)));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        if (redirection->type() == ExternalReference::BUILTIN_CALL_PAIR) {
          SimulatorRuntimePairCall target =
              reinterpret_cast<SimulatorRuntimePairCall>(external);
          ObjectPair result =
              target(arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);
          intptr_t x;
          intptr_t y;
          decodeObjectPair(&result, &x, &y);
          if (::v8::internal::FLAG_trace_sim) {
            PrintF("Returned {%08" V8PRIxPTR ", %08" V8PRIxPTR "}\n", x, y);
          }
          if (ABI_RETURNS_OBJECTPAIR_IN_REGS) {
            set_register(r2, x);
            set_register(r3, y);
          } else {
            memcpy(reinterpret_cast<void*>(result_buffer), &result,
                   sizeof(ObjectPair));
            set_register(r2, result_buffer);
          }
        } else {
          DCHECK(redirection->type() == ExternalReference::BUILTIN_CALL);
          SimulatorRuntimeCall target =
              reinterpret_cast<SimulatorRuntimeCall>(external);
          intptr_t result = target(arg[0], arg[1], arg[2], arg[3], arg[4],
                                   arg[5], arg[6], arg[7], arg[8]);
          if (::v8::internal::FLAG_trace_sim) {
            PrintF("Returned %08" V8PRIxPTR "\n", result);
          }
          set_register(r2, result);
        }
        // #if !V8_TARGET_ARCH_S390X
        //         DCHECK(redirection->type() ==
        //         ExternalReference::BUILTIN_CALL);
        //         SimulatorRuntimeCall target =
        //             reinterpret_cast<SimulatorRuntimeCall>(external);
        //         int64_t result = target(arg[0], arg[1], arg[2], arg[3],
        //         arg[4],
        //                                 arg[5]);
        //         int32_t lo_res = static_cast<int32_t>(result);
        //         int32_t hi_res = static_cast<int32_t>(result >> 32);
        // #if !V8_TARGET_LITTLE_ENDIAN
        //         if (::v8::internal::FLAG_trace_sim) {
        //           PrintF("Returned %08x\n", hi_res);
        //         }
        //         set_register(r2, hi_res);
        //         set_register(r3, lo_res);
        // #else
        //         if (::v8::internal::FLAG_trace_sim) {
        //           PrintF("Returned %08x\n", lo_res);
        //         }
        //         set_register(r2, lo_res);
        //         set_register(r3, hi_res);
        // #endif
        // #else
        //         if (redirection->type() == ExternalReference::BUILTIN_CALL) {
        //           SimulatorRuntimeCall target =
        //             reinterpret_cast<SimulatorRuntimeCall>(external);
        //           intptr_t result = target(arg[0], arg[1], arg[2], arg[3],
        //           arg[4],
        //               arg[5]);
        //           if (::v8::internal::FLAG_trace_sim) {
        //             PrintF("Returned %08" V8PRIxPTR "\n", result);
        //           }
        //           set_register(r2, result);
        //         } else {
        //           DCHECK(redirection->type() ==
        //               ExternalReference::BUILTIN_CALL_PAIR);
        //           SimulatorRuntimePairCall target =
        //             reinterpret_cast<SimulatorRuntimePairCall>(external);
        //           ObjectPair result = target(arg[0], arg[1], arg[2], arg[3],
        //               arg[4], arg[5]);
        //           if (::v8::internal::FLAG_trace_sim) {
        //             PrintF("Returned %08" V8PRIxPTR ", %08" V8PRIxPTR "\n",
        //                 result.x, result.y);
        //           }
        // #if ABI_RETURNS_OBJECTPAIR_IN_REGS
        //           set_register(r2, result.x);
        //           set_register(r3, result.y);
        // #else
        //            memcpy(reinterpret_cast<void *>(result_buffer), &result,
        //                sizeof(ObjectPair));
        // #endif
        //         }
        // #endif
      }
      int64_t saved_lr = *reinterpret_cast<intptr_t*>(
          get_register(sp) + kStackFrameRASlot * kPointerSize);
#if (!V8_TARGET_ARCH_S390X && V8_HOST_ARCH_S390)
      // On zLinux-31, the saved_lr might be tagged with a high bit of 1.
      // Cleanse it before proceeding with simulation.
      saved_lr &= 0x7FFFFFFF;
#endif
      set_pc(saved_lr);
      break;
    }
    case kBreakpoint: {
      S390Debugger dbg(this);
      dbg.Debug();
      break;
    }
    // stop uses all codes greater than 1 << 23.
    default: {
      if (svc >= (1 << 23)) {
        uint32_t code = svc & kStopCodeMask;
        if (isWatchedStop(code)) {
          IncreaseStopCounter(code);
        }
        // Stop if it is enabled, otherwise go on jumping over the stop
        // and the message address.
        if (isEnabledStop(code)) {
          S390Debugger dbg(this);
          dbg.Stop(instr);
        } else {
          set_pc(get_pc() + sizeof(FourByteInstr) + kPointerSize);
        }
      } else {
        // This is not a valid svc code.
        UNREACHABLE();
        break;
      }
    }
  }
}

// Stop helper functions.
bool Simulator::isStopInstruction(Instruction* instr) {
  return (instr->Bits(27, 24) == 0xF) && (instr->SvcValue() >= kStopCode);
}

bool Simulator::isWatchedStop(uint32_t code) {
  DCHECK_LE(code, kMaxStopCode);
  return code < kNumOfWatchedStops;
}

bool Simulator::isEnabledStop(uint32_t code) {
  DCHECK_LE(code, kMaxStopCode);
  // Unwatched stops are always enabled.
  return !isWatchedStop(code) ||
         !(watched_stops_[code].count & kStopDisabledBit);
}

void Simulator::EnableStop(uint32_t code) {
  DCHECK(isWatchedStop(code));
  if (!isEnabledStop(code)) {
    watched_stops_[code].count &= ~kStopDisabledBit;
  }
}

void Simulator::DisableStop(uint32_t code) {
  DCHECK(isWatchedStop(code));
  if (isEnabledStop(code)) {
    watched_stops_[code].count |= kStopDisabledBit;
  }
}

void Simulator::IncreaseStopCounter(uint32_t code) {
  DCHECK_LE(code, kMaxStopCode);
  DCHECK(isWatchedStop(code));
  if ((watched_stops_[code].count & ~(1 << 31)) == 0x7fffffff) {
    PrintF(
        "Stop counter for code %i has overflowed.\n"
        "Enabling this code and reseting the counter to 0.\n",
        code);
    watched_stops_[code].count = 0;
    EnableStop(code);
  } else {
    watched_stops_[code].count++;
  }
}

// Print a stop status.
void Simulator::PrintStopInfo(uint32_t code) {
  DCHECK_LE(code, kMaxStopCode);
  if (!isWatchedStop(code)) {
    PrintF("Stop not watched.");
  } else {
    const char* state = isEnabledStop(code) ? "Enabled" : "Disabled";
    int32_t count = watched_stops_[code].count & ~kStopDisabledBit;
    // Don't print the state of unused breakpoints.
    if (count != 0) {
      if (watched_stops_[code].desc) {
        PrintF("stop %i - 0x%x: \t%s, \tcounter = %i, \t%s\n", code, code,
               state, count, watched_stops_[code].desc);
      } else {
        PrintF("stop %i - 0x%x: \t%s, \tcounter = %i\n", code, code, state,
               count);
      }
    }
  }
}

// Method for checking overflow on signed addition:
//   Test src1 and src2 have opposite sign,
//   (1) No overflow if they have opposite sign
//   (2) Test the result and one of the operands have opposite sign
//      (a) No overflow if they don't have opposite sign
//      (b) Overflow if opposite
#define CheckOverflowForIntAdd(src1, src2, type) \
  OverflowFromSigned<type>(src1 + src2, src1, src2, true);

#define CheckOverflowForIntSub(src1, src2, type) \
  OverflowFromSigned<type>(src1 - src2, src1, src2, false);

// Method for checking overflow on unsigned addition
#define CheckOverflowForUIntAdd(src1, src2) \
  ((src1) + (src2) < (src1) || (src1) + (src2) < (src2))

// Method for checking overflow on unsigned subtraction
#define CheckOverflowForUIntSub(src1, src2) ((src1) - (src2) > (src1))

// Method for checking overflow on multiplication
#define CheckOverflowForMul(src1, src2) (((src1) * (src2)) / (src2) != (src1))

// Method for checking overflow on shift right
#define CheckOverflowForShiftRight(src1, src2) \
  (((src1) >> (src2)) << (src2) != (src1))

// Method for checking overflow on shift left
#define CheckOverflowForShiftLeft(src1, src2) \
  (((src1) << (src2)) >> (src2) != (src1))

int16_t Simulator::ByteReverse(int16_t hword) {
#if defined(__GNUC__)
  return __builtin_bswap16(hword);
#else
  return (hword << 8) | ((hword >> 8) & 0x00ff);
#endif
}

int32_t Simulator::ByteReverse(int32_t word) {
#if defined(__GNUC__)
  return __builtin_bswap32(word);
#else
  int32_t result = word << 24;
  result |= (word << 8) & 0x00ff0000;
  result |= (word >> 8) & 0x0000ff00;
  result |= (word >> 24) & 0x00000ff;
  return result;
#endif
}

int64_t Simulator::ByteReverse(int64_t dword) {
#if defined(__GNUC__)
  return __builtin_bswap64(dword);
#else
#error unsupport __builtin_bswap64
#endif
}

int Simulator::DecodeInstruction(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();
  DCHECK_NOT_NULL(EvalTable[op]);
  return (this->*EvalTable[op])(instr);
}

// Executes the current instruction.
void Simulator::ExecuteInstruction(Instruction* instr, bool auto_incr_pc) {
  icount_++;

  if (v8::internal::FLAG_check_icache) {
    CheckICache(isolate_->simulator_i_cache(), instr);
  }

  pc_modified_ = false;

  if (::v8::internal::FLAG_trace_sim) {
    disasm::NameConverter converter;
    disasm::Disassembler dasm(converter);
    // use a reasonably large buffer
    v8::internal::EmbeddedVector<char, 256> buffer;
    dasm.InstructionDecode(buffer, reinterpret_cast<byte*>(instr));
    PrintF("%05" PRId64 "  %08" V8PRIxPTR "  %s\n", icount_,
           reinterpret_cast<intptr_t>(instr), buffer.start());

    // Flush stdout to prevent incomplete file output during abnormal exits
    // This is caused by the output being buffered before being written to file
    fflush(stdout);
  }

  // Try to simulate as S390 Instruction first.
  int length = DecodeInstruction(instr);

  if (!pc_modified_ && auto_incr_pc) {
    DCHECK(length == instr->InstructionLength());
    set_pc(reinterpret_cast<intptr_t>(instr) + length);
  }
  return;
}

void Simulator::DebugStart() {
  S390Debugger dbg(this);
  dbg.Debug();
}

void Simulator::Execute() {
  // Get the PC to simulate. Cannot use the accessor here as we need the
  // raw PC value and not the one used as input to arithmetic instructions.
  intptr_t program_counter = get_pc();

  if (::v8::internal::FLAG_stop_sim_at == 0) {
    // Fast version of the dispatch loop without checking whether the simulator
    // should be stopping at a particular executed instruction.
    while (program_counter != end_sim_pc) {
      Instruction* instr = reinterpret_cast<Instruction*>(program_counter);
      ExecuteInstruction(instr);
      program_counter = get_pc();
    }
  } else {
    // FLAG_stop_sim_at is at the non-default value. Stop in the debugger when
    // we reach the particular instruction count.
    while (program_counter != end_sim_pc) {
      Instruction* instr = reinterpret_cast<Instruction*>(program_counter);
      if (icount_ == ::v8::internal::FLAG_stop_sim_at) {
        S390Debugger dbg(this);
        dbg.Debug();
      } else {
        ExecuteInstruction(instr);
      }
      program_counter = get_pc();
    }
  }
}

void Simulator::CallInternal(byte* entry, int reg_arg_count) {
  // Adjust JS-based stack limit to C-based stack limit.
  isolate_->stack_guard()->AdjustStackLimitForSimulator();

  // Prepare to execute the code at entry
  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    // entry is the function descriptor
    set_pc(*(reinterpret_cast<intptr_t*>(entry)));
  } else {
    // entry is the instruction address
    set_pc(reinterpret_cast<intptr_t>(entry));
  }
  // Remember the values of non-volatile registers.
  int64_t r6_val = get_register(r6);
  int64_t r7_val = get_register(r7);
  int64_t r8_val = get_register(r8);
  int64_t r9_val = get_register(r9);
  int64_t r10_val = get_register(r10);
  int64_t r11_val = get_register(r11);
  int64_t r12_val = get_register(r12);
  int64_t r13_val = get_register(r13);

  if (ABI_CALL_VIA_IP) {
    // Put target address in ip (for JS prologue).
    set_register(ip, get_pc());
  }

  // Put down marker for end of simulation. The simulator will stop simulation
  // when the PC reaches this value. By saving the "end simulation" value into
  // the LR the simulation stops when returning to this call point.
  registers_[14] = end_sim_pc;

  // Set up the non-volatile registers with a known value. To be able to check
  // that they are preserved properly across JS execution.
  uintptr_t callee_saved_value = icount_;
  if (reg_arg_count < 5) {
    set_register(r6, callee_saved_value + 6);
  }
  set_register(r7, callee_saved_value + 7);
  set_register(r8, callee_saved_value + 8);
  set_register(r9, callee_saved_value + 9);
  set_register(r10, callee_saved_value + 10);
  set_register(r11, callee_saved_value + 11);
  set_register(r12, callee_saved_value + 12);
  set_register(r13, callee_saved_value + 13);

  // Start the simulation
  Execute();

// Check that the non-volatile registers have been preserved.
#ifndef V8_TARGET_ARCH_S390X
  if (reg_arg_count < 5) {
    DCHECK_EQ(callee_saved_value + 6, get_low_register<uint32_t>(r6));
  }
  DCHECK_EQ(callee_saved_value + 7, get_low_register<uint32_t>(r7));
  DCHECK_EQ(callee_saved_value + 8, get_low_register<uint32_t>(r8));
  DCHECK_EQ(callee_saved_value + 9, get_low_register<uint32_t>(r9));
  DCHECK_EQ(callee_saved_value + 10, get_low_register<uint32_t>(r10));
  DCHECK_EQ(callee_saved_value + 11, get_low_register<uint32_t>(r11));
  DCHECK_EQ(callee_saved_value + 12, get_low_register<uint32_t>(r12));
  DCHECK_EQ(callee_saved_value + 13, get_low_register<uint32_t>(r13));
#else
  if (reg_arg_count < 5) {
    DCHECK_EQ(callee_saved_value + 6, get_register(r6));
  }
  DCHECK_EQ(callee_saved_value + 7, get_register(r7));
  DCHECK_EQ(callee_saved_value + 8, get_register(r8));
  DCHECK_EQ(callee_saved_value + 9, get_register(r9));
  DCHECK_EQ(callee_saved_value + 10, get_register(r10));
  DCHECK_EQ(callee_saved_value + 11, get_register(r11));
  DCHECK_EQ(callee_saved_value + 12, get_register(r12));
  DCHECK_EQ(callee_saved_value + 13, get_register(r13));
#endif

  // Restore non-volatile registers with the original value.
  set_register(r6, r6_val);
  set_register(r7, r7_val);
  set_register(r8, r8_val);
  set_register(r9, r9_val);
  set_register(r10, r10_val);
  set_register(r11, r11_val);
  set_register(r12, r12_val);
  set_register(r13, r13_val);
}

intptr_t Simulator::Call(byte* entry, int argument_count, ...) {
  // Adjust JS-based stack limit to C-based stack limit.
  isolate_->stack_guard()->AdjustStackLimitForSimulator();

  // Remember the values of non-volatile registers.
  int64_t r6_val = get_register(r6);
  int64_t r7_val = get_register(r7);
  int64_t r8_val = get_register(r8);
  int64_t r9_val = get_register(r9);
  int64_t r10_val = get_register(r10);
  int64_t r11_val = get_register(r11);
  int64_t r12_val = get_register(r12);
  int64_t r13_val = get_register(r13);

  va_list parameters;
  va_start(parameters, argument_count);
  // Set up arguments

  // First 5 arguments passed in registers r2-r6.
  int reg_arg_count = (argument_count > 5) ? 5 : argument_count;
  int stack_arg_count = argument_count - reg_arg_count;
  for (int i = 0; i < reg_arg_count; i++) {
    intptr_t value = va_arg(parameters, intptr_t);
    set_register(i + 2, value);
  }

  // Remaining arguments passed on stack.
  int64_t original_stack = get_register(sp);
  // Compute position of stack on entry to generated code.
  uintptr_t entry_stack =
      (original_stack -
       (kCalleeRegisterSaveAreaSize + stack_arg_count * sizeof(intptr_t)));
  if (base::OS::ActivationFrameAlignment() != 0) {
    entry_stack &= -base::OS::ActivationFrameAlignment();
  }

  // Store remaining arguments on stack, from low to high memory.
  intptr_t* stack_argument =
      reinterpret_cast<intptr_t*>(entry_stack + kCalleeRegisterSaveAreaSize);
  for (int i = 0; i < stack_arg_count; i++) {
    intptr_t value = va_arg(parameters, intptr_t);
    stack_argument[i] = value;
  }
  va_end(parameters);
  set_register(sp, entry_stack);

// Prepare to execute the code at entry
#if ABI_USES_FUNCTION_DESCRIPTORS
  // entry is the function descriptor
  set_pc(*(reinterpret_cast<intptr_t*>(entry)));
#else
  // entry is the instruction address
  set_pc(reinterpret_cast<intptr_t>(entry));
#endif

  // Put target address in ip (for JS prologue).
  set_register(r12, get_pc());

  // Put down marker for end of simulation. The simulator will stop simulation
  // when the PC reaches this value. By saving the "end simulation" value into
  // the LR the simulation stops when returning to this call point.
  registers_[14] = end_sim_pc;

  // Set up the non-volatile registers with a known value. To be able to check
  // that they are preserved properly across JS execution.
  uintptr_t callee_saved_value = icount_;
  if (reg_arg_count < 5) {
    set_register(r6, callee_saved_value + 6);
  }
  set_register(r7, callee_saved_value + 7);
  set_register(r8, callee_saved_value + 8);
  set_register(r9, callee_saved_value + 9);
  set_register(r10, callee_saved_value + 10);
  set_register(r11, callee_saved_value + 11);
  set_register(r12, callee_saved_value + 12);
  set_register(r13, callee_saved_value + 13);

  // Start the simulation
  Execute();

// Check that the non-volatile registers have been preserved.
#ifndef V8_TARGET_ARCH_S390X
  if (reg_arg_count < 5) {
    DCHECK_EQ(callee_saved_value + 6, get_low_register<uint32_t>(r6));
  }
  DCHECK_EQ(callee_saved_value + 7, get_low_register<uint32_t>(r7));
  DCHECK_EQ(callee_saved_value + 8, get_low_register<uint32_t>(r8));
  DCHECK_EQ(callee_saved_value + 9, get_low_register<uint32_t>(r9));
  DCHECK_EQ(callee_saved_value + 10, get_low_register<uint32_t>(r10));
  DCHECK_EQ(callee_saved_value + 11, get_low_register<uint32_t>(r11));
  DCHECK_EQ(callee_saved_value + 12, get_low_register<uint32_t>(r12));
  DCHECK_EQ(callee_saved_value + 13, get_low_register<uint32_t>(r13));
#else
  if (reg_arg_count < 5) {
    DCHECK_EQ(callee_saved_value + 6, get_register(r6));
  }
  DCHECK_EQ(callee_saved_value + 7, get_register(r7));
  DCHECK_EQ(callee_saved_value + 8, get_register(r8));
  DCHECK_EQ(callee_saved_value + 9, get_register(r9));
  DCHECK_EQ(callee_saved_value + 10, get_register(r10));
  DCHECK_EQ(callee_saved_value + 11, get_register(r11));
  DCHECK_EQ(callee_saved_value + 12, get_register(r12));
  DCHECK_EQ(callee_saved_value + 13, get_register(r13));
#endif

  // Restore non-volatile registers with the original value.
  set_register(r6, r6_val);
  set_register(r7, r7_val);
  set_register(r8, r8_val);
  set_register(r9, r9_val);
  set_register(r10, r10_val);
  set_register(r11, r11_val);
  set_register(r12, r12_val);
  set_register(r13, r13_val);
// Pop stack passed arguments.

#ifndef V8_TARGET_ARCH_S390X
  DCHECK_EQ(entry_stack, get_low_register<uint32_t>(sp));
#else
  DCHECK_EQ(entry_stack, get_register(sp));
#endif
  set_register(sp, original_stack);

  // Return value register
  intptr_t result = get_register(r2);
  return result;
}

void Simulator::CallFP(byte* entry, double d0, double d1) {
  set_d_register_from_double(0, d0);
  set_d_register_from_double(1, d1);
  CallInternal(entry);
}

int32_t Simulator::CallFPReturnsInt(byte* entry, double d0, double d1) {
  CallFP(entry, d0, d1);
  int32_t result = get_register(r2);
  return result;
}

double Simulator::CallFPReturnsDouble(byte* entry, double d0, double d1) {
  CallFP(entry, d0, d1);
  return get_double_from_d_register(0);
}

uintptr_t Simulator::PushAddress(uintptr_t address) {
  uintptr_t new_sp = get_register(sp) - sizeof(uintptr_t);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(new_sp);
  *stack_slot = address;
  set_register(sp, new_sp);
  return new_sp;
}

uintptr_t Simulator::PopAddress() {
  uintptr_t current_sp = get_register(sp);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(current_sp);
  uintptr_t address = *stack_slot;
  set_register(sp, current_sp + sizeof(uintptr_t));
  return address;
}

#define EVALUATE(name) \
  int Simulator::Evaluate_##name(Instruction* instr)

#define DCHECK_OPCODE(op) DCHECK(instr->S390OpcodeValue() == op)

#define AS(type) reinterpret_cast<type*>(instr)

#define DECODE_RIL_A_INSTRUCTION(r1, i2)               \
  int r1 = AS(RILInstruction)->R1Value();              \
  uint32_t i2 = AS(RILInstruction)->I2UnsignedValue(); \
  int length = 6;

#define DECODE_RIL_B_INSTRUCTION(r1, i2)      \
  int r1 = AS(RILInstruction)->R1Value();     \
  int32_t i2 = AS(RILInstruction)->I2Value(); \
  int length = 6;

#define DECODE_RIL_C_INSTRUCTION(m1, ri2)                               \
  Condition m1 = static_cast<Condition>(AS(RILInstruction)->R1Value()); \
  uint64_t ri2 = AS(RILInstruction)->I2Value();                         \
  int length = 6;

#define DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2) \
  int r1 = AS(RXYInstruction)->R1Value();        \
  int x2 = AS(RXYInstruction)->X2Value();        \
  int b2 = AS(RXYInstruction)->B2Value();        \
  int d2 = AS(RXYInstruction)->D2Value();        \
  int length = 6;

#define DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val) \
  int x2 = AS(RXInstruction)->X2Value();            \
  int b2 = AS(RXInstruction)->B2Value();            \
  int r1 = AS(RXInstruction)->R1Value();            \
  intptr_t d2_val = AS(RXInstruction)->D2Value();   \
  int length = 4;

#define DECODE_RS_A_INSTRUCTION(r1, r3, b2, d2) \
  int r3 = AS(RSInstruction)->R3Value();        \
  int b2 = AS(RSInstruction)->B2Value();        \
  int r1 = AS(RSInstruction)->R1Value();        \
  intptr_t d2 = AS(RSInstruction)->D2Value();   \
  int length = 4;

#define DECODE_RS_A_INSTRUCTION_NO_R3(r1, b2, d2) \
  int b2 = AS(RSInstruction)->B2Value();          \
  int r1 = AS(RSInstruction)->R1Value();          \
  int d2 = AS(RSInstruction)->D2Value();          \
  int length = 4;

#define DECODE_SI_INSTRUCTION_I_UINT8(b1, d1_val, imm_val) \
  int b1 = AS(SIInstruction)->B1Value();                   \
  intptr_t d1_val = AS(SIInstruction)->D1Value();          \
  uint8_t imm_val = AS(SIInstruction)->I2Value();          \
  int length = 4;

#define DECODE_SIL_INSTRUCTION(b1, d1, i2)     \
  int b1 = AS(SILInstruction)->B1Value();      \
  intptr_t d1 = AS(SILInstruction)->D1Value(); \
  int16_t i2 = AS(SILInstruction)->I2Value();  \
  int length = 6;

#define DECODE_SIY_INSTRUCTION(b1, d1, i2)     \
  int b1 = AS(SIYInstruction)->B1Value();      \
  intptr_t d1 = AS(SIYInstruction)->D1Value(); \
  uint8_t i2 = AS(SIYInstruction)->I2Value();  \
  int length = 6;

#define DECODE_RRE_INSTRUCTION(r1, r2)    \
  int r1 = AS(RREInstruction)->R1Value(); \
  int r2 = AS(RREInstruction)->R2Value(); \
  int length = 4;

#define DECODE_RRE_INSTRUCTION_M3(r1, r2, m3) \
  int r1 = AS(RREInstruction)->R1Value();     \
  int r2 = AS(RREInstruction)->R2Value();     \
  int m3 = AS(RREInstruction)->M3Value();     \
  int length = 4;

#define DECODE_RRE_INSTRUCTION_NO_R2(r1)  \
  int r1 = AS(RREInstruction)->R1Value(); \
  int length = 4;

#define DECODE_RRD_INSTRUCTION(r1, r2, r3) \
  int r1 = AS(RRDInstruction)->R1Value();  \
  int r2 = AS(RRDInstruction)->R2Value();  \
  int r3 = AS(RRDInstruction)->R3Value();  \
  int length = 4;

#define DECODE_RRF_E_INSTRUCTION(r1, r2, m3, m4) \
  int r1 = AS(RRFInstruction)->R1Value();        \
  int r2 = AS(RRFInstruction)->R2Value();        \
  int m3 = AS(RRFInstruction)->M3Value();        \
  int m4 = AS(RRFInstruction)->M4Value();        \
  int length = 4;

#define DECODE_RRF_A_INSTRUCTION(r1, r2, r3) \
  int r1 = AS(RRFInstruction)->R1Value();    \
  int r2 = AS(RRFInstruction)->R2Value();    \
  int r3 = AS(RRFInstruction)->R3Value();    \
  int length = 4;

#define DECODE_RRF_C_INSTRUCTION(r1, r2, m3)                            \
  int r1 = AS(RRFInstruction)->R1Value();                               \
  int r2 = AS(RRFInstruction)->R2Value();                               \
  Condition m3 = static_cast<Condition>(AS(RRFInstruction)->M3Value()); \
  int length = 4;

#define DECODE_RR_INSTRUCTION(r1, r2)    \
  int r1 = AS(RRInstruction)->R1Value(); \
  int r2 = AS(RRInstruction)->R2Value(); \
  int length = 2;

#define DECODE_RIE_D_INSTRUCTION(r1, r2, i2)  \
  int r1 = AS(RIEInstruction)->R1Value();     \
  int r2 = AS(RIEInstruction)->R2Value();     \
  int32_t i2 = AS(RIEInstruction)->I6Value(); \
  int length = 6;

#define DECODE_RIE_F_INSTRUCTION(r1, r2, i3, i4, i5) \
  int r1 = AS(RIEInstruction)->R1Value();            \
  int r2 = AS(RIEInstruction)->R2Value();            \
  uint32_t i3 = AS(RIEInstruction)->I3Value();       \
  uint32_t i4 = AS(RIEInstruction)->I4Value();       \
  uint32_t i5 = AS(RIEInstruction)->I5Value();       \
  int length = 6;

#define DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2) \
  int r1 = AS(RSYInstruction)->R1Value();        \
  int r3 = AS(RSYInstruction)->R3Value();        \
  int b2 = AS(RSYInstruction)->B2Value();        \
  intptr_t d2 = AS(RSYInstruction)->D2Value();   \
  int length = 6;

#define DECODE_RI_A_INSTRUCTION(instr, r1, i2) \
  int32_t r1 = AS(RIInstruction)->R1Value();   \
  int16_t i2 = AS(RIInstruction)->I2Value();   \
  int length = 4;

#define DECODE_RI_B_INSTRUCTION(instr, r1, i2) \
  int32_t r1 = AS(RILInstruction)->R1Value();  \
  int16_t i2 = AS(RILInstruction)->I2Value();  \
  int length = 4;

#define DECODE_RI_C_INSTRUCTION(instr, m1, i2)                         \
  Condition m1 = static_cast<Condition>(AS(RIInstruction)->R1Value()); \
  int16_t i2 = AS(RIInstruction)->I2Value();                           \
  int length = 4;

#define DECODE_RXE_INSTRUCTION(r1, b2, x2, d2) \
  int r1 = AS(RXEInstruction)->R1Value();      \
  int b2 = AS(RXEInstruction)->B2Value();      \
  int x2 = AS(RXEInstruction)->X2Value();      \
  int d2 = AS(RXEInstruction)->D2Value();      \
  int length = 6;

#define DECODE_VRR_C_INSTRUCTION(r1, r2, r3, m6, m5, m4) \
  int r1 = AS(VRR_C_Instruction)->R1Value();             \
  int r2 = AS(VRR_C_Instruction)->R2Value();             \
  int r3 = AS(VRR_C_Instruction)->R3Value();             \
  int m6 = AS(VRR_C_Instruction)->M6Value();             \
  int m5 = AS(VRR_C_Instruction)->M5Value();             \
  int m4 = AS(VRR_C_Instruction)->M4Value();             \
  int length = 6;

#define GET_ADDRESS(index_reg, base_reg, offset)       \
  (((index_reg) == 0) ? 0 : get_register(index_reg)) + \
      (((base_reg) == 0) ? 0 : get_register(base_reg)) + offset

int Simulator::Evaluate_Unknown(Instruction* instr) {
  UNREACHABLE();
}

EVALUATE(VFA) {
  DCHECK_OPCODE(VFA);
  DECODE_VRR_C_INSTRUCTION(r1, r2, r3, m6, m5, m4);
  USE(m6);
  USE(m5);
  USE(m4);
  DCHECK_EQ(m5, 8);
  DCHECK_EQ(m4, 3);
  double r2_val = get_double_from_d_register(r2);
  double r3_val = get_double_from_d_register(r3);
  double r1_val = r2_val + r3_val;
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(VFS) {
  DCHECK_OPCODE(VFS);
  DECODE_VRR_C_INSTRUCTION(r1, r2, r3, m6, m5, m4);
  USE(m6);
  USE(m5);
  USE(m4);
  DCHECK_EQ(m5, 8);
  DCHECK_EQ(m4, 3);
  double r2_val = get_double_from_d_register(r2);
  double r3_val = get_double_from_d_register(r3);
  double r1_val = r2_val - r3_val;
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(VFM) {
  DCHECK_OPCODE(VFM);
  DECODE_VRR_C_INSTRUCTION(r1, r2, r3, m6, m5, m4);
  USE(m6);
  USE(m5);
  USE(m4);
  DCHECK_EQ(m5, 8);
  DCHECK_EQ(m4, 3);
  double r2_val = get_double_from_d_register(r2);
  double r3_val = get_double_from_d_register(r3);
  double r1_val = r2_val * r3_val;
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(VFD) {
  DCHECK_OPCODE(VFD);
  DECODE_VRR_C_INSTRUCTION(r1, r2, r3, m6, m5, m4);
  USE(m6);
  USE(m5);
  USE(m4);
  DCHECK_EQ(m5, 8);
  DCHECK_EQ(m4, 3);
  double r2_val = get_double_from_d_register(r2);
  double r3_val = get_double_from_d_register(r3);
  double r1_val = r2_val / r3_val;
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(DUMY) {
  DCHECK_OPCODE(DUMY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  USE(r1);
  USE(x2);
  USE(b2);
  USE(d2);
  // dummy instruction does nothing.
  return length;
}

EVALUATE(CLR) {
  DCHECK_OPCODE(CLR);
  DECODE_RR_INSTRUCTION(r1, r2);
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  SetS390ConditionCode<uint32_t>(r1_val, r2_val);
  return length;
}

EVALUATE(LR) {
  DCHECK_OPCODE(LR);
  DECODE_RR_INSTRUCTION(r1, r2);
  set_low_register(r1, get_low_register<int32_t>(r2));
  return length;
}

EVALUATE(AR) {
  DCHECK_OPCODE(AR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  bool isOF = CheckOverflowForIntAdd(r1_val, r2_val, int32_t);
  r1_val += r2_val;
  SetS390ConditionCode<int32_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(L) {
  DCHECK_OPCODE(L);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int32_t mem_val = ReadW(addr, instr);
  set_low_register(r1, mem_val);
  return length;
}

EVALUATE(BRC) {
  DCHECK_OPCODE(BRC);
  DECODE_RI_C_INSTRUCTION(instr, m1, i2);

  if (TestConditionCode(m1)) {
    intptr_t offset = 2 * i2;
    set_pc(get_pc() + offset);
  }
  return length;
}

EVALUATE(AHI) {
  DCHECK_OPCODE(AHI);
  DECODE_RI_A_INSTRUCTION(instr, r1, i2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  bool isOF = CheckOverflowForIntAdd(r1_val, i2, int32_t);
  r1_val += i2;
  set_low_register(r1, r1_val);
  SetS390ConditionCode<int32_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(AGHI) {
  DCHECK_OPCODE(AGHI);
  DECODE_RI_A_INSTRUCTION(instr, r1, i2);
  int64_t r1_val = get_register(r1);
  bool isOF = false;
  isOF = CheckOverflowForIntAdd(r1_val, i2, int64_t);
  r1_val += i2;
  set_register(r1, r1_val);
  SetS390ConditionCode<int64_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(BRCL) {
  DCHECK_OPCODE(BRCL);
  DECODE_RIL_C_INSTRUCTION(m1, ri2);

  if (TestConditionCode(m1)) {
    intptr_t offset = 2 * ri2;
    set_pc(get_pc() + offset);
  }
  return length;
}

EVALUATE(IIHF) {
  DCHECK_OPCODE(IIHF);
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  set_high_register(r1, imm);
  return length;
}

EVALUATE(IILF) {
  DCHECK_OPCODE(IILF);
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  set_low_register(r1, imm);
  return length;
}

EVALUATE(LGR) {
  DCHECK_OPCODE(LGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  set_register(r1, get_register(r2));
  return length;
}

EVALUATE(LG) {
  DCHECK_OPCODE(LG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  intptr_t addr = GET_ADDRESS(x2, b2, d2);
  int64_t mem_val = ReadDW(addr);
  set_register(r1, mem_val);
  return length;
}

EVALUATE(AGR) {
  DCHECK_OPCODE(AGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r1_val = get_register(r1);
  int64_t r2_val = get_register(r2);
  bool isOF = CheckOverflowForIntAdd(r1_val, r2_val, int64_t);
  r1_val += r2_val;
  set_register(r1, r1_val);
  SetS390ConditionCode<int64_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(LGFR) {
  DCHECK_OPCODE(LGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  int64_t result = static_cast<int64_t>(r2_val);
  set_register(r1, result);

  return length;
}

EVALUATE(LBR) {
  DCHECK_OPCODE(LBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  r2_val <<= 24;
  r2_val >>= 24;
  set_low_register(r1, r2_val);
  return length;
}

EVALUATE(LGBR) {
  DCHECK_OPCODE(LGBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_low_register<int64_t>(r2);
  r2_val <<= 56;
  r2_val >>= 56;
  set_register(r1, r2_val);
  return length;
}

EVALUATE(LHR) {
  DCHECK_OPCODE(LHR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  r2_val <<= 16;
  r2_val >>= 16;
  set_low_register(r1, r2_val);
  return length;
}

EVALUATE(LGHR) {
  DCHECK_OPCODE(LGHR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_low_register<int64_t>(r2);
  r2_val <<= 48;
  r2_val >>= 48;
  set_register(r1, r2_val);
  return length;
}

EVALUATE(LGF) {
  DCHECK_OPCODE(LGF);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  intptr_t addr = GET_ADDRESS(x2, b2, d2);
  int64_t mem_val = static_cast<int64_t>(ReadW(addr, instr));
  set_register(r1, mem_val);
  return length;
}

EVALUATE(ST) {
  DCHECK_OPCODE(ST);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  WriteW(addr, r1_val, instr);
  return length;
}

EVALUATE(STG) {
  DCHECK_OPCODE(STG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  intptr_t addr = GET_ADDRESS(x2, b2, d2);
  uint64_t value = get_register(r1);
  WriteDW(addr, value);
  return length;
}

EVALUATE(STY) {
  DCHECK_OPCODE(STY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  intptr_t addr = GET_ADDRESS(x2, b2, d2);
  uint32_t value = get_low_register<uint32_t>(r1);
  WriteW(addr, value, instr);
  return length;
}

EVALUATE(LY) {
  DCHECK_OPCODE(LY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  intptr_t addr = GET_ADDRESS(x2, b2, d2);
  uint32_t mem_val = ReadWU(addr, instr);
  set_low_register(r1, mem_val);
  return length;
}

EVALUATE(LLGC) {
  DCHECK_OPCODE(LLGC);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  uint8_t mem_val = ReadBU(GET_ADDRESS(x2, b2, d2));
  set_register(r1, static_cast<uint64_t>(mem_val));
  return length;
}

EVALUATE(LLC) {
  DCHECK_OPCODE(LLC);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  uint8_t mem_val = ReadBU(GET_ADDRESS(x2, b2, d2));
  set_low_register(r1, static_cast<uint32_t>(mem_val));
  return length;
}

EVALUATE(RLL) {
  DCHECK_OPCODE(RLL);
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // only takes rightmost 6 bits
  int shiftBits = GET_ADDRESS(0, b2, d2) & 0x3F;
  // unsigned
  uint32_t r3_val = get_low_register<uint32_t>(r3);
  uint32_t alu_out = 0;
  uint32_t rotateBits = r3_val >> (32 - shiftBits);
  alu_out = (r3_val << shiftBits) | (rotateBits);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(RISBG) {
  DCHECK_OPCODE(RISBG);
  DECODE_RIE_F_INSTRUCTION(r1, r2, i3, i4, i5);
  // Starting Bit Position is Bits 2-7 of I3 field
  uint32_t start_bit = i3 & 0x3F;
  // Ending Bit Position is Bits 2-7 of I4 field
  uint32_t end_bit = i4 & 0x3F;
  // Shift Amount is Bits 2-7 of I5 field
  uint32_t shift_amount = i5 & 0x3F;
  // Zero out Remaining (unslected) bits if Bit 0 of I4 is 1.
  bool zero_remaining = (0 != (i4 & 0x80));

  uint64_t src_val = get_register(r2);

  // Rotate Left by Shift Amount first
  uint64_t rotated_val =
      (src_val << shift_amount) | (src_val >> (64 - shift_amount));
  int32_t width = end_bit - start_bit + 1;

  uint64_t selection_mask = 0;
  if (width < 64) {
    selection_mask = (static_cast<uint64_t>(1) << width) - 1;
  } else {
    selection_mask = static_cast<uint64_t>(static_cast<int64_t>(-1));
  }
  selection_mask = selection_mask << (63 - end_bit);

  uint64_t selected_val = rotated_val & selection_mask;

  if (!zero_remaining) {
    // Merged the unselected bits from the original value
    selected_val = (get_register(r1) & ~selection_mask) | selected_val;
  }

  // Condition code is set by treating result as 64-bit signed int
  SetS390ConditionCode<int64_t>(selected_val, 0);
  set_register(r1, selected_val);
  return length;
}

EVALUATE(AHIK) {
  DCHECK_OPCODE(AHIK);
  DECODE_RIE_D_INSTRUCTION(r1, r2, i2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t imm = static_cast<int32_t>(i2);
  bool isOF = CheckOverflowForIntAdd(r2_val, imm, int32_t);
  set_low_register(r1, r2_val + imm);
  SetS390ConditionCode<int32_t>(r2_val + imm, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(AGHIK) {
  // 64-bit Add
  DCHECK_OPCODE(AGHIK);
  DECODE_RIE_D_INSTRUCTION(r1, r2, i2);
  int64_t r2_val = get_register(r2);
  int64_t imm = static_cast<int64_t>(i2);
  bool isOF = CheckOverflowForIntAdd(r2_val, imm, int64_t);
  set_register(r1, r2_val + imm);
  SetS390ConditionCode<int64_t>(r2_val + imm, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(BKPT) {
  DCHECK_OPCODE(BKPT);
  set_pc(get_pc() + 2);
  S390Debugger dbg(this);
  dbg.Debug();
  int length = 2;
  return length;
}

EVALUATE(SPM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BALR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BCTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BCR) {
  DCHECK_OPCODE(BCR);
  DECODE_RR_INSTRUCTION(r1, r2);
  if (TestConditionCode(Condition(r1))) {
    intptr_t r2_val = get_register(r2);
#if (!V8_TARGET_ARCH_S390X && V8_HOST_ARCH_S390)
    // On 31-bit, the top most bit may be 0 or 1, but is ignored by the
    // hardware.  Cleanse the top bit before jumping to it, unless it's one
    // of the special PCs
    if (r2_val != bad_lr && r2_val != end_sim_pc) r2_val &= 0x7FFFFFFF;
#endif
    set_pc(r2_val);
  }

  return length;
}

EVALUATE(SVC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BSM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BASSM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BASR) {
  DCHECK_OPCODE(BASR);
  DECODE_RR_INSTRUCTION(r1, r2);
  intptr_t link_addr = get_pc() + 2;
  // If R2 is zero, the BASR does not branch.
  int64_t r2_val = (r2 == 0) ? link_addr : get_register(r2);
#if (!V8_TARGET_ARCH_S390X && V8_HOST_ARCH_S390)
  // On 31-bit, the top most bit may be 0 or 1, which can cause issues
  // for stackwalker.  The top bit should either be cleanse before being
  // pushed onto the stack, or during stack walking when dereferenced.
  // For simulator, we'll take the worst case scenario and always tag
  // the high bit, to flush out more problems.
  link_addr |= 0x80000000;
#endif
  set_register(r1, link_addr);
  set_pc(r2_val);
  return length;
}

EVALUATE(MVCL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLCL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LPR) {
  DCHECK_OPCODE(LPR);
  // Load Positive (32)
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  // If negative, then negate it.
  r2_val = (r2_val < 0) ? -r2_val : r2_val;
  set_low_register(r1, r2_val);
  SetS390ConditionCode<int32_t>(r2_val, 0);
  if (r2_val == (static_cast<int32_t>(1) << 31)) {
    SetS390OverflowCode(true);
  }
  return length;
}

EVALUATE(LNR) {
  DCHECK_OPCODE(LNR);
  // Load Negative (32)
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  r2_val = (r2_val >= 0) ? -r2_val : r2_val;  // If pos, then negate it.
  set_low_register(r1, r2_val);
  condition_reg_ = (r2_val == 0) ? CC_EQ : CC_LT;  // CC0 - result is zero
  // CC1 - result is negative
  return length;
}

EVALUATE(LTR) {
  DCHECK_OPCODE(LTR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  SetS390ConditionCode<int32_t>(r2_val, 0);
  set_low_register(r1, r2_val);
  return length;
}

EVALUATE(LCR) {
  DCHECK_OPCODE(LCR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t result = 0;
  bool isOF = false;
  isOF = __builtin_ssub_overflow(0, r2_val, &result);
  set_low_register(r1, result);
  SetS390ConditionCode<int32_t>(r2_val, 0);
  // Checks for overflow where r2_val = -2147483648.
  // Cannot do int comparison due to GCC 4.8 bug on x86.
  // Detect INT_MIN alternatively, as it is the only value where both
  // original and result are negative due to overflow.
  if (isOF) {
    SetS390OverflowCode(true);
  }
  return length;
}

EVALUATE(NR) {
  DCHECK_OPCODE(NR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  r1_val &= r2_val;
  SetS390BitWiseConditionCode<uint32_t>(r1_val);
  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(OR) {
  DCHECK_OPCODE(OR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  r1_val |= r2_val;
  SetS390BitWiseConditionCode<uint32_t>(r1_val);
  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(XR) {
  DCHECK_OPCODE(XR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  r1_val ^= r2_val;
  SetS390BitWiseConditionCode<uint32_t>(r1_val);
  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(CR) {
  DCHECK_OPCODE(CR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  SetS390ConditionCode<int32_t>(r1_val, r2_val);
  return length;
}

EVALUATE(SR) {
  DCHECK_OPCODE(SR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  bool isOF = false;
  isOF = CheckOverflowForIntSub(r1_val, r2_val, int32_t);
  r1_val -= r2_val;
  SetS390ConditionCode<int32_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(MR) {
  DCHECK_OPCODE(MR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  DCHECK_EQ(r1 % 2, 0);
  r1_val = get_low_register<int32_t>(r1 + 1);
  int64_t product = static_cast<int64_t>(r1_val) * static_cast<int64_t>(r2_val);
  int32_t high_bits = product >> 32;
  r1_val = high_bits;
  int32_t low_bits = product & 0x00000000FFFFFFFF;
  set_low_register(r1, high_bits);
  set_low_register(r1 + 1, low_bits);
  return length;
}

EVALUATE(DR) {
  DCHECK_OPCODE(DR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  // reg-reg pair should be even-odd pair, assert r1 is an even register
  DCHECK_EQ(r1 % 2, 0);
  // leftmost 32 bits of the dividend are in r1
  // rightmost 32 bits of the dividend are in r1+1
  // get the signed value from r1
  int64_t dividend = static_cast<int64_t>(r1_val) << 32;
  // get unsigned value from r1+1
  // avoid addition with sign-extended r1+1 value
  dividend += get_low_register<uint32_t>(r1 + 1);
  int32_t remainder = dividend % r2_val;
  int32_t quotient = dividend / r2_val;
  r1_val = remainder;
  set_low_register(r1, remainder);
  set_low_register(r1 + 1, quotient);
  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(ALR) {
  DCHECK_OPCODE(ALR);
  DECODE_RR_INSTRUCTION(r1, r2);
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  uint32_t alu_out = 0;
  bool isOF = false;
  alu_out = r1_val + r2_val;
  isOF = CheckOverflowForUIntAdd(r1_val, r2_val);
  set_low_register(r1, alu_out);
  SetS390ConditionCodeCarry<uint32_t>(alu_out, isOF);
  return length;
}

EVALUATE(SLR) {
  DCHECK_OPCODE(SLR);
  DECODE_RR_INSTRUCTION(r1, r2);
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  uint32_t alu_out = 0;
  bool isOF = false;
  alu_out = r1_val - r2_val;
  isOF = CheckOverflowForUIntSub(r1_val, r2_val);
  set_low_register(r1, alu_out);
  SetS390ConditionCodeCarry<uint32_t>(alu_out, isOF);
  return length;
}

EVALUATE(LDR) {
  DCHECK_OPCODE(LDR);
  DECODE_RR_INSTRUCTION(r1, r2);
  int64_t r2_val = get_d_register(r2);
  set_d_register(r1, r2_val);
  return length;
}

EVALUATE(CDR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LER) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STH) {
  DCHECK_OPCODE(STH);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int16_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t mem_addr = b2_val + x2_val + d2_val;
  WriteH(mem_addr, r1_val, instr);

  return length;
}

EVALUATE(LA) {
  DCHECK_OPCODE(LA);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  set_register(r1, addr);
  return length;
}

EVALUATE(STC) {
  DCHECK_OPCODE(STC);
  // Store Character/Byte
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  uint8_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t mem_addr = b2_val + x2_val + d2_val;
  WriteB(mem_addr, r1_val);
  return length;
}

EVALUATE(IC_z) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(EX) {
  DCHECK_OPCODE(EX);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int32_t r1_val = get_low_register<int32_t>(r1);

  SixByteInstr the_instr = Instruction::InstructionBits(
      reinterpret_cast<const byte*>(b2_val + x2_val + d2_val));
  int inst_length = Instruction::InstructionLength(
      reinterpret_cast<const byte*>(b2_val + x2_val + d2_val));

  char new_instr_buf[8];
  char* addr = reinterpret_cast<char*>(&new_instr_buf[0]);
  the_instr |= static_cast<SixByteInstr>(r1_val & 0xff)
               << (8 * inst_length - 16);
  Instruction::SetInstructionBits<SixByteInstr>(
      reinterpret_cast<byte*>(addr), static_cast<SixByteInstr>(the_instr));
  ExecuteInstruction(reinterpret_cast<Instruction*>(addr), false);
  return length;
}

EVALUATE(BAL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BCT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LH) {
  DCHECK_OPCODE(LH);
  // Load Halfword
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);

  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t mem_addr = x2_val + b2_val + d2_val;

  int32_t result = static_cast<int32_t>(ReadH(mem_addr, instr));
  set_low_register(r1, result);
  return length;
}

EVALUATE(CH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AH) {
  DCHECK_OPCODE(AH);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int32_t mem_val = static_cast<int32_t>(ReadH(addr, instr));
  int32_t alu_out = 0;
  bool isOF = false;
  isOF = CheckOverflowForIntAdd(r1_val, mem_val, int32_t);
  alu_out = r1_val + mem_val;
  set_low_register(r1, alu_out);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);

  return length;
}

EVALUATE(SH) {
  DCHECK_OPCODE(SH);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int32_t mem_val = static_cast<int32_t>(ReadH(addr, instr));
  int32_t alu_out = 0;
  bool isOF = false;
  isOF = CheckOverflowForIntSub(r1_val, mem_val, int32_t);
  alu_out = r1_val - mem_val;
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);

  return length;
}

EVALUATE(MH) {
  DCHECK_OPCODE(MH);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int32_t mem_val = static_cast<int32_t>(ReadH(addr, instr));
  int32_t alu_out = 0;
  alu_out = r1_val * mem_val;
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(BAS) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CVD) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CVB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(N) {
  DCHECK_OPCODE(N);
  // 32-bit Reg-Mem instructions
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int32_t mem_val = ReadW(b2_val + x2_val + d2_val, instr);
  int32_t alu_out = 0;
  alu_out = r1_val & mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(CL) {
  DCHECK_OPCODE(CL);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int32_t mem_val = ReadW(addr, instr);
  SetS390ConditionCode<uint32_t>(r1_val, mem_val);
  return length;
}

EVALUATE(O) {
  DCHECK_OPCODE(O);
  // 32-bit Reg-Mem instructions
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int32_t mem_val = ReadW(b2_val + x2_val + d2_val, instr);
  int32_t alu_out = 0;
  alu_out = r1_val | mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(X) {
  DCHECK_OPCODE(X);
  // 32-bit Reg-Mem instructions
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int32_t mem_val = ReadW(b2_val + x2_val + d2_val, instr);
  int32_t alu_out = 0;
  alu_out = r1_val ^ mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(C) {
  DCHECK_OPCODE(C);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int32_t mem_val = ReadW(addr, instr);
  SetS390ConditionCode<int32_t>(r1_val, mem_val);
  return length;
}

EVALUATE(A) {
  DCHECK_OPCODE(A);
  // 32-bit Reg-Mem instructions
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int32_t mem_val = ReadW(b2_val + x2_val + d2_val, instr);
  int32_t alu_out = 0;
  bool isOF = false;
  isOF = CheckOverflowForIntAdd(r1_val, mem_val, int32_t);
  alu_out = r1_val + mem_val;
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(S) {
  DCHECK_OPCODE(S);
  // 32-bit Reg-Mem instructions
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int32_t mem_val = ReadW(b2_val + x2_val + d2_val, instr);
  int32_t alu_out = 0;
  bool isOF = false;
  isOF = CheckOverflowForIntSub(r1_val, mem_val, int32_t);
  alu_out = r1_val - mem_val;
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(M) {
  DCHECK_OPCODE(M);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  DCHECK_EQ(r1 % 2, 0);
  int32_t mem_val = ReadW(addr, instr);
  int32_t r1_val = get_low_register<int32_t>(r1 + 1);
  int64_t product =
      static_cast<int64_t>(r1_val) * static_cast<int64_t>(mem_val);
  int32_t high_bits = product >> 32;
  r1_val = high_bits;
  int32_t low_bits = product & 0x00000000FFFFFFFF;
  set_low_register(r1, high_bits);
  set_low_register(r1 + 1, low_bits);
  return length;
}

EVALUATE(D) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STD) {
  DCHECK_OPCODE(STD);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int64_t frs_val = get_d_register(r1);
  WriteDW(addr, frs_val);
  return length;
}

EVALUATE(LD) {
  DCHECK_OPCODE(LD);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int64_t dbl_val = *reinterpret_cast<int64_t*>(addr);
  set_d_register(r1, dbl_val);
  return length;
}

EVALUATE(CD) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STE) {
  DCHECK_OPCODE(STE);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  int64_t frs_val = get_d_register(r1) >> 32;
  WriteW(addr, static_cast<int32_t>(frs_val), instr);
  return length;
}

EVALUATE(MS) {
  DCHECK_OPCODE(MS);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int32_t mem_val = ReadW(b2_val + x2_val + d2_val, instr);
  int32_t r1_val = get_low_register<int32_t>(r1);
  set_low_register(r1, r1_val * mem_val);
  return length;
}

EVALUATE(LE) {
  DCHECK_OPCODE(LE);
  DECODE_RX_A_INSTRUCTION(x2, b2, r1, d2_val);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t addr = b2_val + x2_val + d2_val;
  float float_val = *reinterpret_cast<float*>(addr);
  set_d_register_from_float32(r1, float_val);
  return length;
}

EVALUATE(BRXH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BRXLE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BXH) {
  DCHECK_OPCODE(BXH);
  DECODE_RS_A_INSTRUCTION(r1, r3, b2, d2);

  // r1_val is the first operand, r3_val is the increment
  int32_t r1_val = r1 == 0 ? 0 : get_register(r1);
  int32_t r3_val = r2 == 0 ? 0 : get_register(r3);
  intptr_t b2_val = b2 == 0 ? 0 : get_register(b2);
  intptr_t branch_address = b2_val + d2;
  // increment r1_val
  r1_val += r3_val;

  // if the increment is even, then it designates a pair of registers
  // and the contents of the even and odd registers of the pair are used as
  // the increment and compare value respectively. If the increment is odd,
  // the increment itself is used as both the increment and compare value
  int32_t compare_val = r3 % 2 == 0 ? get_register(r3 + 1) : r3_val;
  if (r1_val > compare_val) {
    // branch to address if r1_val is greater than compare value
    set_pc(branch_address);
  }

  // update contents of register in r1 with the new incremented value
  set_register(r1, r1_val);

  return length;
}

EVALUATE(BXLE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRL) {
  DCHECK_OPCODE(SRL);
  DECODE_RS_A_INSTRUCTION_NO_R3(r1, b2, d2);
  // only takes rightmost 6bits
  int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t alu_out = 0;
  alu_out = r1_val >> shiftBits;
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(SLL) {
  DCHECK_OPCODE(SLL);
  DECODE_RS_A_INSTRUCTION_NO_R3(r1, b2, d2)
  // only takes rightmost 6bits
  int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t alu_out = 0;
  alu_out = r1_val << shiftBits;
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(SRA) {
  DCHECK_OPCODE(SRA);
  DECODE_RS_A_INSTRUCTION_NO_R3(r1, b2, d2);
  // only takes rightmost 6bits
  int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t alu_out = 0;
  bool isOF = false;
  alu_out = r1_val >> shiftBits;
  set_low_register(r1, alu_out);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(SLA) {
  DCHECK_OPCODE(SLA);
  DECODE_RS_A_INSTRUCTION_NO_R3(r1, b2, d2);
  // only takes rightmost 6bits
  int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t alu_out = 0;
  bool isOF = false;
  isOF = CheckOverflowForShiftLeft(r1_val, shiftBits);
  alu_out = r1_val << shiftBits;
  set_low_register(r1, alu_out);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(SRDL) {
  DCHECK_OPCODE(SRDL);
  DECODE_RS_A_INSTRUCTION_NO_R3(r1, b2, d2);
  DCHECK_EQ(r1 % 2, 0);  // must be a reg pair
  // only takes rightmost 6bits
  int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  uint64_t opnd1 = static_cast<uint64_t>(get_low_register<uint32_t>(r1)) << 32;
  uint64_t opnd2 = static_cast<uint64_t>(get_low_register<uint32_t>(r1 + 1));
  uint64_t r1_val = opnd1 | opnd2;
  uint64_t alu_out = r1_val >> shiftBits;
  set_low_register(r1, alu_out >> 32);
  set_low_register(r1 + 1, alu_out & 0x00000000FFFFFFFF);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  return length;
}

EVALUATE(SLDL) {
  DCHECK_OPCODE(SLDL);
  DECODE_RS_A_INSTRUCTION_NO_R3(r1, b2, d2);
  // only takes rightmost 6bits
  int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;

  DCHECK_EQ(r1 % 2, 0);
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t r1_next_val = get_low_register<uint32_t>(r1 + 1);
  uint64_t alu_out = (static_cast<uint64_t>(r1_val) << 32) |
                     (static_cast<uint64_t>(r1_next_val));
  alu_out <<= shiftBits;
  set_low_register(r1 + 1, static_cast<uint32_t>(alu_out));
  set_low_register(r1, static_cast<uint32_t>(alu_out >> 32));
  return length;
}

EVALUATE(SRDA) {
  DCHECK_OPCODE(SRDA);
  DECODE_RS_A_INSTRUCTION_NO_R3(r1, b2, d2);
  DCHECK_EQ(r1 % 2, 0);  // must be a reg pair
  // only takes rightmost 6bits
  int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  int64_t opnd1 = static_cast<int64_t>(get_low_register<int32_t>(r1)) << 32;
  int64_t opnd2 = static_cast<uint64_t>(get_low_register<uint32_t>(r1 + 1));
  int64_t r1_val = opnd1 + opnd2;
  int64_t alu_out = r1_val >> shiftBits;
  set_low_register(r1, alu_out >> 32);
  set_low_register(r1 + 1, alu_out & 0x00000000FFFFFFFF);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  return length;
}

EVALUATE(SLDA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STM) {
  DCHECK_OPCODE(STM);
  DECODE_RS_A_INSTRUCTION(r1, r3, rb, d2);
  // Store Multiple 32-bits.
  int offset = d2;
  // Regs roll around if r3 is less than r1.
  // Artificially increase r3 by 16 so we can calculate
  // the number of regs stored properly.
  if (r3 < r1) r3 += 16;

  int32_t rb_val = (rb == 0) ? 0 : get_low_register<int32_t>(rb);

  // Store each register in ascending order.
  for (int i = 0; i <= r3 - r1; i++) {
    int32_t value = get_low_register<int32_t>((r1 + i) % 16);
    WriteW(rb_val + offset + 4 * i, value, instr);
  }
  return length;
}

EVALUATE(TM) {
  DCHECK_OPCODE(TM);
  // Test Under Mask (Mem - Imm) (8)
  DECODE_SI_INSTRUCTION_I_UINT8(b1, d1_val, imm_val)
  int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
  intptr_t addr = b1_val + d1_val;
  uint8_t mem_val = ReadB(addr);
  uint8_t selected_bits = mem_val & imm_val;
  // CC0: Selected bits are zero
  // CC1: Selected bits mixed zeros and ones
  // CC3: Selected bits all ones
  if (0 == selected_bits) {
    condition_reg_ = CC_EQ;  // CC0
  } else if (selected_bits == imm_val) {
    condition_reg_ = 0x1;  // CC3
  } else {
    condition_reg_ = 0x4;  // CC1
  }
  return length;
}

EVALUATE(MVI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TS) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLI) {
  DCHECK_OPCODE(CLI);
  // Compare Immediate (Mem - Imm) (8)
  DECODE_SI_INSTRUCTION_I_UINT8(b1, d1_val, imm_val)
  int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
  intptr_t addr = b1_val + d1_val;
  uint8_t mem_val = ReadB(addr);
  SetS390ConditionCode<uint8_t>(mem_val, imm_val);
  return length;
}

EVALUATE(OI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(XI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LM) {
  DCHECK_OPCODE(LM);
  DECODE_RS_A_INSTRUCTION(r1, r3, rb, d2);
  // Store Multiple 32-bits.
  int offset = d2;
  // Regs roll around if r3 is less than r1.
  // Artificially increase r3 by 16 so we can calculate
  // the number of regs stored properly.
  if (r3 < r1) r3 += 16;

  int32_t rb_val = (rb == 0) ? 0 : get_low_register<int32_t>(rb);

  // Store each register in ascending order.
  for (int i = 0; i <= r3 - r1; i++) {
    int32_t value = ReadW(rb_val + offset + 4 * i, instr);
    set_low_register((r1 + i) % 16, value);
  }
  return length;
}

EVALUATE(MVCLE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLCLE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDS) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STCM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ICM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BPRP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BPP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TRTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVN) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVC) {
  DCHECK_OPCODE(MVC);
  // Move Character
  SSInstruction* ssInstr = reinterpret_cast<SSInstruction*>(instr);
  int b1 = ssInstr->B1Value();
  intptr_t d1 = ssInstr->D1Value();
  int b2 = ssInstr->B2Value();
  intptr_t d2 = ssInstr->D2Value();
  int length = ssInstr->Length();
  int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t src_addr = b2_val + d2;
  intptr_t dst_addr = b1_val + d1;
  // remember that the length is the actual length - 1
  for (int i = 0; i < length + 1; ++i) {
    WriteB(dst_addr++, ReadB(src_addr++));
  }
  length = 6;
  return length;
}

EVALUATE(MVZ) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(OC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(XC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVCP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TRT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ED) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(EDMK) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PKU) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(UNPKU) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVCIN) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PKA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(UNPKA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PLO) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LMD) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVO) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PACK) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(UNPK) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ZAP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(UPT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PFPO) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(IIHH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(IIHL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(IILH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(IILL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NIHH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NIHL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NILH) {
  DCHECK_OPCODE(NILH);
  DECODE_RI_A_INSTRUCTION(instr, r1, i);
  int32_t r1_val = get_low_register<int32_t>(r1);
  // CC is set based on the 16 bits that are AND'd
  SetS390BitWiseConditionCode<uint16_t>((r1_val >> 16) & i);
  i = (i << 16) | 0x0000FFFF;
  set_low_register(r1, r1_val & i);
  return length;
}

EVALUATE(NILL) {
  DCHECK_OPCODE(NILL);
  DECODE_RI_A_INSTRUCTION(instr, r1, i);
  int32_t r1_val = get_low_register<int32_t>(r1);
  // CC is set based on the 16 bits that are AND'd
  SetS390BitWiseConditionCode<uint16_t>(r1_val & i);
  i |= 0xFFFF0000;
  set_low_register(r1, r1_val & i);
  return length;
}

EVALUATE(OIHH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(OIHL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(OILH) {
  DCHECK_OPCODE(OILH);
  DECODE_RI_A_INSTRUCTION(instr, r1, i);
  int32_t r1_val = get_low_register<int32_t>(r1);
  // CC is set based on the 16 bits that are AND'd
  SetS390BitWiseConditionCode<uint16_t>((r1_val >> 16) | i);
  i = i << 16;
  set_low_register(r1, r1_val | i);
  return length;
}

EVALUATE(OILL) {
  DCHECK_OPCODE(OILL);
  DECODE_RI_A_INSTRUCTION(instr, r1, i);
  int32_t r1_val = get_low_register<int32_t>(r1);
  // CC is set based on the 16 bits that are AND'd
  SetS390BitWiseConditionCode<uint16_t>(r1_val | i);
  set_low_register(r1, r1_val | i);
  return length;
}

EVALUATE(LLIHH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLIHL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLILH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLILL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TMLH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TMLL) {
  DCHECK_OPCODE(TMLL);
  DECODE_RI_A_INSTRUCTION(instr, r1, i2);
  uint32_t mask = i2 & 0x0000FFFF;
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  r1_val = r1_val & 0x0000FFFF;  // uses only the last 16bits

  // Test if all selected bits are zeros or mask is zero
  if (0 == (mask & r1_val)) {
    condition_reg_ = 0x8;
    return length;  // Done!
  }

  DCHECK_NE(mask, 0);
  // Test if all selected bits are one
  if (mask == (mask & r1_val)) {
    condition_reg_ = 0x1;
    return length;  // Done!
  }

  // Now we know selected bits mixed zeros and ones
  // Test if the leftmost bit is zero or one
#if defined(__GNUC__)
  int leadingZeros = __builtin_clz(mask);
  mask = 0x80000000u >> leadingZeros;
  if (mask & r1_val) {
    // leftmost bit is one
    condition_reg_ = 0x2;
  } else {
    // leftmost bit is zero
    condition_reg_ = 0x4;
  }
  return length;  // Done!
#else
  for (int i = 15; i >= 0; i--) {
    if (mask & (1 << i)) {
      if (r1_val & (1 << i)) {
        // leftmost bit is one
        condition_reg_ = 0x2;
      } else {
        // leftmost bit is zero
        condition_reg_ = 0x4;
      }
      return length;  // Done!
    }
  }
#endif
  UNREACHABLE();
}

EVALUATE(TMHH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TMHL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BRAS) {
  DCHECK_OPCODE(BRAS);
  // Branch Relative and Save
  DECODE_RI_B_INSTRUCTION(instr, r1, d2)
  intptr_t pc = get_pc();
  // Set PC of next instruction to register
  set_register(r1, pc + sizeof(FourByteInstr));
  // Update PC to branch target
  set_pc(pc + d2 * 2);
  return length;
}

EVALUATE(BRCT) {
  DCHECK_OPCODE(BRCT);
  // Branch On Count (32/64).
  DECODE_RI_A_INSTRUCTION(instr, r1, i2);
  int64_t value = get_low_register<int32_t>(r1);
  set_low_register(r1, --value);
  // Branch if value != 0
  if (value != 0) {
    intptr_t offset = i2 * 2;
    set_pc(get_pc() + offset);
  }
  return length;
}

EVALUATE(BRCTG) {
  DCHECK_OPCODE(BRCTG);
  // Branch On Count (32/64).
  DECODE_RI_A_INSTRUCTION(instr, r1, i2);
  int64_t value = get_register(r1);
  set_register(r1, --value);
  // Branch if value != 0
  if (value != 0) {
    intptr_t offset = i2 * 2;
    set_pc(get_pc() + offset);
  }
  return length;
}

EVALUATE(LHI) {
  DCHECK_OPCODE(LHI);
  DECODE_RI_A_INSTRUCTION(instr, r1, i);
  set_low_register(r1, i);
  return length;
}

EVALUATE(LGHI) {
  DCHECK_OPCODE(LGHI);
  DECODE_RI_A_INSTRUCTION(instr, r1, i2);
  int64_t i = static_cast<int64_t>(i2);
  set_register(r1, i);
  return length;
}

EVALUATE(MHI) {
  DCHECK_OPCODE(MHI);
  DECODE_RI_A_INSTRUCTION(instr, r1, i);
  int32_t r1_val = get_low_register<int32_t>(r1);
  bool isOF = false;
  isOF = CheckOverflowForMul(r1_val, i);
  r1_val *= i;
  set_low_register(r1, r1_val);
  SetS390ConditionCode<int32_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(MGHI) {
  DCHECK_OPCODE(MGHI);
  DECODE_RI_A_INSTRUCTION(instr, r1, i2);
  int64_t i = static_cast<int64_t>(i2);
  int64_t r1_val = get_register(r1);
  bool isOF = false;
  isOF = CheckOverflowForMul(r1_val, i);
  r1_val *= i;
  set_register(r1, r1_val);
  SetS390ConditionCode<int32_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(CHI) {
  DCHECK_OPCODE(CHI);
  DECODE_RI_A_INSTRUCTION(instr, r1, i);
  int32_t r1_val = get_low_register<int32_t>(r1);
  SetS390ConditionCode<int32_t>(r1_val, i);
  return length;
}

EVALUATE(CGHI) {
  DCHECK_OPCODE(CGHI);
  DECODE_RI_A_INSTRUCTION(instr, r1, i2);
  int64_t i = static_cast<int64_t>(i2);
  int64_t r1_val = get_register(r1);
  SetS390ConditionCode<int64_t>(r1_val, i);
  return length;
}

EVALUATE(LARL) {
  DCHECK_OPCODE(LARL);
  DECODE_RIL_B_INSTRUCTION(r1, i2);
  intptr_t offset = i2 * 2;
  set_register(r1, get_pc() + offset);
  return length;
}

EVALUATE(LGFI) {
  DCHECK_OPCODE(LGFI);
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  set_register(r1, static_cast<int64_t>(static_cast<int32_t>(imm)));
  return length;
}

EVALUATE(BRASL) {
  DCHECK_OPCODE(BRASL);
  // Branch and Save Relative Long
  DECODE_RIL_B_INSTRUCTION(r1, i2);
  intptr_t d2 = i2;
  intptr_t pc = get_pc();
  set_register(r1, pc + 6);  // save next instruction to register
  set_pc(pc + d2 * 2);       // update register
  return length;
}

EVALUATE(XIHF) {
  DCHECK_OPCODE(XIHF);
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  uint32_t alu_out = 0;
  alu_out = get_high_register<uint32_t>(r1);
  alu_out = alu_out ^ imm;
  set_high_register(r1, alu_out);
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  return length;
}

EVALUATE(XILF) {
  DCHECK_OPCODE(XILF);
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  uint32_t alu_out = 0;
  alu_out = get_low_register<uint32_t>(r1);
  alu_out = alu_out ^ imm;
  set_low_register(r1, alu_out);
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  return length;
}

EVALUATE(NIHF) {
  DCHECK_OPCODE(NIHF);
  // Bitwise Op on upper 32-bits
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  uint32_t alu_out = get_high_register<uint32_t>(r1);
  alu_out &= imm;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_high_register(r1, alu_out);
  return length;
}

EVALUATE(NILF) {
  DCHECK_OPCODE(NILF);
  // Bitwise Op on lower 32-bits
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  uint32_t alu_out = get_low_register<uint32_t>(r1);
  alu_out &= imm;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(OIHF) {
  DCHECK_OPCODE(OIHF);
  // Bitwise Op on upper 32-bits
  DECODE_RIL_B_INSTRUCTION(r1, imm);
  uint32_t alu_out = get_high_register<uint32_t>(r1);
  alu_out |= imm;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_high_register(r1, alu_out);
  return length;
}

EVALUATE(OILF) {
  DCHECK_OPCODE(OILF);
  // Bitwise Op on lower 32-bits
  DECODE_RIL_B_INSTRUCTION(r1, imm);
  uint32_t alu_out = get_low_register<uint32_t>(r1);
  alu_out |= imm;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(LLIHF) {
  DCHECK_OPCODE(LLIHF);
  // Load Logical Immediate into high word
  DECODE_RIL_A_INSTRUCTION(r1, i2);
  uint64_t imm = static_cast<uint64_t>(i2);
  set_register(r1, imm << 32);
  return length;
}

EVALUATE(LLILF) {
  DCHECK_OPCODE(LLILF);
  // Load Logical into lower 32-bits (zero extend upper 32-bits)
  DECODE_RIL_A_INSTRUCTION(r1, i2);
  uint64_t imm = static_cast<uint64_t>(i2);
  set_register(r1, imm);
  return length;
}

EVALUATE(MSGFI) {
  DCHECK_OPCODE(MSGFI);
  DECODE_RIL_B_INSTRUCTION(r1, i2);
  int64_t alu_out = get_register(r1);
  alu_out = alu_out * i2;
  set_register(r1, alu_out);
  return length;
}

EVALUATE(MSFI) {
  DCHECK_OPCODE(MSFI);
  DECODE_RIL_B_INSTRUCTION(r1, i2);
  int32_t alu_out = get_low_register<int32_t>(r1);
  alu_out = alu_out * i2;
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(SLGFI) {
  DCHECK_OPCODE(SLGFI);
#ifndef V8_TARGET_ARCH_S390X
  // should only be called on 64bit
  DCHECK(false);
#endif
  DECODE_RIL_A_INSTRUCTION(r1, i2);
  uint64_t r1_val = (uint64_t)(get_register(r1));
  uint64_t alu_out;
  alu_out = r1_val - i2;
  set_register(r1, (intptr_t)alu_out);
  SetS390ConditionCode<uint64_t>(alu_out, 0);
  return length;
}

EVALUATE(SLFI) {
  DCHECK_OPCODE(SLFI);
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  uint32_t alu_out = get_low_register<uint32_t>(r1);
  alu_out -= imm;
  SetS390ConditionCode<uint32_t>(alu_out, 0);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(AGFI) {
  DCHECK_OPCODE(AGFI);
  // Clobbering Add Word Immediate
  DECODE_RIL_B_INSTRUCTION(r1, i2_val);
  bool isOF = false;
  // 64-bit Add (Register + 32-bit Imm)
  int64_t r1_val = get_register(r1);
  int64_t i2 = static_cast<int64_t>(i2_val);
  isOF = CheckOverflowForIntAdd(r1_val, i2, int64_t);
  int64_t alu_out = r1_val + i2;
  set_register(r1, alu_out);
  SetS390ConditionCode<int64_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(AFI) {
  DCHECK_OPCODE(AFI);
  // Clobbering Add Word Immediate
  DECODE_RIL_B_INSTRUCTION(r1, i2);
  bool isOF = false;
  // 32-bit Add (Register + 32-bit Immediate)
  int32_t r1_val = get_low_register<int32_t>(r1);
  isOF = CheckOverflowForIntAdd(r1_val, i2, int32_t);
  int32_t alu_out = r1_val + i2;
  set_low_register(r1, alu_out);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(ALGFI) {
  DCHECK_OPCODE(ALGFI);
#ifndef V8_TARGET_ARCH_S390X
  // should only be called on 64bit
  DCHECK(false);
#endif
  DECODE_RIL_A_INSTRUCTION(r1, i2);
  uint64_t r1_val = (uint64_t)(get_register(r1));
  uint64_t alu_out;
  alu_out = r1_val + i2;
  set_register(r1, (intptr_t)alu_out);
  SetS390ConditionCode<uint64_t>(alu_out, 0);

  return length;
}

EVALUATE(ALFI) {
  DCHECK_OPCODE(ALFI);
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  uint32_t alu_out = get_low_register<uint32_t>(r1);
  alu_out += imm;
  SetS390ConditionCode<uint32_t>(alu_out, 0);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(CGFI) {
  DCHECK_OPCODE(CGFI);
  // Compare with Immediate (64)
  DECODE_RIL_B_INSTRUCTION(r1, i2);
  int64_t imm = static_cast<int64_t>(i2);
  SetS390ConditionCode<int64_t>(get_register(r1), imm);
  return length;
}

EVALUATE(CFI) {
  DCHECK_OPCODE(CFI);
  // Compare with Immediate (32)
  DECODE_RIL_B_INSTRUCTION(r1, imm);
  SetS390ConditionCode<int32_t>(get_low_register<int32_t>(r1), imm);
  return length;
}

EVALUATE(CLGFI) {
  DCHECK_OPCODE(CLGFI);
  // Compare Logical with Immediate (64)
  DECODE_RIL_A_INSTRUCTION(r1, i2);
  uint64_t imm = static_cast<uint64_t>(i2);
  SetS390ConditionCode<uint64_t>(get_register(r1), imm);
  return length;
}

EVALUATE(CLFI) {
  DCHECK_OPCODE(CLFI);
  // Compare Logical with Immediate (32)
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  SetS390ConditionCode<uint32_t>(get_low_register<uint32_t>(r1), imm);
  return length;
}

EVALUATE(LLHRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LGHRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LHRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLGHRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STHRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LGRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STGRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LGFRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLGFRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(EXRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PFDRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGHRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CHRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGFRL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ECTG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CSST) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LPD) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LPDG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BRCTH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AIH) {
  DCHECK_OPCODE(AIH);
  DECODE_RIL_A_INSTRUCTION(r1, i2);
  int32_t r1_val = get_high_register<int32_t>(r1);
  bool isOF = CheckOverflowForIntAdd(r1_val, static_cast<int32_t>(i2), int32_t);
  r1_val += static_cast<int32_t>(i2);
  set_high_register(r1, r1_val);
  SetS390ConditionCode<int32_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(ALSIH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ALSIHN) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CIH) {
  DCHECK_OPCODE(CIH);
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  int32_t r1_val = get_high_register<int32_t>(r1);
  SetS390ConditionCode<int32_t>(r1_val, static_cast<int32_t>(imm));
  return length;
}

EVALUATE(CLIH) {
  DCHECK_OPCODE(CLIH);
  // Compare Logical with Immediate (32)
  DECODE_RIL_A_INSTRUCTION(r1, imm);
  SetS390ConditionCode<uint32_t>(get_high_register<uint32_t>(r1), imm);
  return length;
}

EVALUATE(STCK) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CFC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(IPM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(HSCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SSCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STSCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TSCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TPI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SAL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RSCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STCRW) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STCPS) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RCHP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SCHM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CKSM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SAR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(EAR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSR) {
  DCHECK_OPCODE(MSR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r2_val = get_low_register<int32_t>(r2);
  set_low_register(r1, r1_val * r2_val);
  return length;
}

EVALUATE(MSRKC) {
  DCHECK_OPCODE(MSRKC);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t r3_val = get_low_register<int32_t>(r3);
  int64_t result64 =
      static_cast<int64_t>(r2_val) * static_cast<int64_t>(r3_val);
  int32_t result32 = static_cast<int32_t>(result64);
  bool isOF = (static_cast<int64_t>(result32) != result64);
  SetS390ConditionCode<int32_t>(result32, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, result32);
  return length;
}

EVALUATE(MVST) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CUSE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRST) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(XSCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STCKE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STCKF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRNM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STFPC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LFPC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TRE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CUUTF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CUTFU) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STFLE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRNMB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRNMT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LFAS) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PPA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ETND) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TEND) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NIAI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TABORT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TRAP4) {
  DCHECK_OPCODE(TRAP4);
  int length = 4;
  // whack the space of the caller allocated stack
  int64_t sp_addr = get_register(sp);
  for (int i = 0; i < kCalleeRegisterSaveAreaSize / kPointerSize; ++i) {
    // we dont want to whack the RA (r14)
    if (i != 14) (reinterpret_cast<intptr_t*>(sp_addr))[i] = 0xdeadbabe;
  }
  SoftwareInterrupt(instr);
  return length;
}

EVALUATE(LPEBR) {
  DCHECK_OPCODE(LPEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fr1_val = get_float32_from_d_register(r1);
  float fr2_val = get_float32_from_d_register(r2);
  fr1_val = std::fabs(fr2_val);
  set_d_register_from_float32(r1, fr1_val);
  if (fr2_val != fr2_val) {  // input is NaN
    condition_reg_ = CC_OF;
  } else if (fr2_val == 0) {
    condition_reg_ = CC_EQ;
  } else {
    condition_reg_ = CC_GT;
  }

  return length;
}

EVALUATE(LNEBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LTEBR) {
  DCHECK_OPCODE(LTEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_d_register(r2);
  float fr2_val = get_float32_from_d_register(r2);
  SetS390ConditionCode<float>(fr2_val, 0.0);
  set_d_register(r1, r2_val);
  return length;
}

EVALUATE(LCEBR) {
  DCHECK_OPCODE(LCEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fr1_val = get_float32_from_d_register(r1);
  float fr2_val = get_float32_from_d_register(r2);
  fr1_val = -fr2_val;
  set_d_register_from_float32(r1, fr1_val);
  if (fr2_val != fr2_val) {  // input is NaN
    condition_reg_ = CC_OF;
  } else if (fr2_val == 0) {
    condition_reg_ = CC_EQ;
  } else if (fr2_val < 0) {
    condition_reg_ = CC_LT;
  } else if (fr2_val > 0) {
    condition_reg_ = CC_GT;
  }
  return length;
}

EVALUATE(LDEBR) {
  DCHECK_OPCODE(LDEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fp_val = get_float32_from_d_register(r2);
  double db_val = static_cast<double>(fp_val);
  set_d_register_from_double(r1, db_val);
  return length;
}

EVALUATE(LXDBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LXEBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MXDBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KEBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CEBR) {
  DCHECK_OPCODE(CEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fr1_val = get_float32_from_d_register(r1);
  float fr2_val = get_float32_from_d_register(r2);
  if (isNaN(fr1_val) || isNaN(fr2_val)) {
    condition_reg_ = CC_OF;
  } else {
    SetS390ConditionCode<float>(fr1_val, fr2_val);
  }

  return length;
}

EVALUATE(AEBR) {
  DCHECK_OPCODE(AEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fr1_val = get_float32_from_d_register(r1);
  float fr2_val = get_float32_from_d_register(r2);
  fr1_val += fr2_val;
  set_d_register_from_float32(r1, fr1_val);
  SetS390ConditionCode<float>(fr1_val, 0);

  return length;
}

EVALUATE(SEBR) {
  DCHECK_OPCODE(SEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fr1_val = get_float32_from_d_register(r1);
  float fr2_val = get_float32_from_d_register(r2);
  fr1_val -= fr2_val;
  set_d_register_from_float32(r1, fr1_val);
  SetS390ConditionCode<float>(fr1_val, 0);

  return length;
}

EVALUATE(MDEBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DEBR) {
  DCHECK_OPCODE(DEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fr1_val = get_float32_from_d_register(r1);
  float fr2_val = get_float32_from_d_register(r2);
  fr1_val /= fr2_val;
  set_d_register_from_float32(r1, fr1_val);
  return length;
}

EVALUATE(MAEBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSEBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LPDBR) {
  DCHECK_OPCODE(LPDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  r1_val = std::fabs(r2_val);
  set_d_register_from_double(r1, r1_val);
  if (r2_val != r2_val) {  // input is NaN
    condition_reg_ = CC_OF;
  } else if (r2_val == 0) {
    condition_reg_ = CC_EQ;
  } else {
    condition_reg_ = CC_GT;
  }
  return length;
}

EVALUATE(LNDBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LTDBR) {
  DCHECK_OPCODE(LTDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_d_register(r2);
  SetS390ConditionCode<double>(bit_cast<double, int64_t>(r2_val), 0.0);
  set_d_register(r1, r2_val);
  return length;
}

EVALUATE(LCDBR) {
  DCHECK_OPCODE(LCDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  r1_val = -r2_val;
  set_d_register_from_double(r1, r1_val);
  if (r2_val != r2_val) {  // input is NaN
    condition_reg_ = CC_OF;
  } else if (r2_val == 0) {
    condition_reg_ = CC_EQ;
  } else if (r2_val < 0) {
    condition_reg_ = CC_LT;
  } else if (r2_val > 0) {
    condition_reg_ = CC_GT;
  }
  return length;
}

EVALUATE(SQEBR) {
  DCHECK_OPCODE(SQEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fr1_val = get_float32_from_d_register(r1);
  float fr2_val = get_float32_from_d_register(r2);
  fr1_val = std::sqrt(fr2_val);
  set_d_register_from_float32(r1, fr1_val);
  return length;
}

EVALUATE(SQDBR) {
  DCHECK_OPCODE(SQDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  r1_val = std::sqrt(r2_val);
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(SQXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MEEBR) {
  DCHECK_OPCODE(MEEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float fr1_val = get_float32_from_d_register(r1);
  float fr2_val = get_float32_from_d_register(r2);
  fr1_val *= fr2_val;
  set_d_register_from_float32(r1, fr1_val);
  return length;
}

EVALUATE(KDBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDBR) {
  DCHECK_OPCODE(CDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  if (isNaN(r1_val) || isNaN(r2_val)) {
    condition_reg_ = CC_OF;
  } else {
    SetS390ConditionCode<double>(r1_val, r2_val);
  }
  return length;
}

EVALUATE(ADBR) {
  DCHECK_OPCODE(ADBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  r1_val += r2_val;
  set_d_register_from_double(r1, r1_val);
  SetS390ConditionCode<double>(r1_val, 0);
  return length;
}

EVALUATE(SDBR) {
  DCHECK_OPCODE(SDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  r1_val -= r2_val;
  set_d_register_from_double(r1, r1_val);
  SetS390ConditionCode<double>(r1_val, 0);
  return length;
}

EVALUATE(MDBR) {
  DCHECK_OPCODE(MDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  r1_val *= r2_val;
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(DDBR) {
  DCHECK_OPCODE(DDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  r1_val /= r2_val;
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(MADBR) {
  DCHECK_OPCODE(MADBR);
  DECODE_RRD_INSTRUCTION(r1, r2, r3);
  double r1_val = get_double_from_d_register(r1);
  double r2_val = get_double_from_d_register(r2);
  double r3_val = get_double_from_d_register(r3);
  r1_val += r2_val * r3_val;
  set_d_register_from_double(r1, r1_val);
  SetS390ConditionCode<double>(r1_val, 0);
  return length;
}

EVALUATE(MSDBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LPXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LNXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LTXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LCXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LEDBRA) {
  DCHECK_OPCODE(LEDBRA);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r2_val = get_double_from_d_register(r2);
  set_d_register_from_float32(r1, static_cast<float>(r2_val));
  return length;
}

EVALUATE(LDXBRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LEXBRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(FIXBRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TBEDR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TBDR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DIEBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(FIEBRA) {
  DCHECK_OPCODE(FIEBRA);
  DECODE_RRF_E_INSTRUCTION(r1, r2, m3, m4);
  float r2_val = get_float32_from_d_register(r2);
  CHECK_EQ(m4, 0);
  switch (m3) {
    case Assembler::FIDBRA_ROUND_TO_NEAREST_AWAY_FROM_0:
      set_d_register_from_float32(r1, round(r2_val));
      break;
    case Assembler::FIDBRA_ROUND_TOWARD_0:
      set_d_register_from_float32(r1, trunc(r2_val));
      break;
    case Assembler::FIDBRA_ROUND_TOWARD_POS_INF:
      set_d_register_from_float32(r1, std::ceil(r2_val));
      break;
    case Assembler::FIDBRA_ROUND_TOWARD_NEG_INF:
      set_d_register_from_float32(r1, std::floor(r2_val));
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  return length;
}

EVALUATE(THDER) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(THDR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DIDBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(FIDBRA) {
  DCHECK_OPCODE(FIDBRA);
  DECODE_RRF_E_INSTRUCTION(r1, r2, m3, m4);
  double r2_val = get_double_from_d_register(r2);
  CHECK_EQ(m4, 0);
  switch (m3) {
    case Assembler::FIDBRA_ROUND_TO_NEAREST_AWAY_FROM_0:
      set_d_register_from_double(r1, round(r2_val));
      break;
    case Assembler::FIDBRA_ROUND_TOWARD_0:
      set_d_register_from_double(r1, trunc(r2_val));
      break;
    case Assembler::FIDBRA_ROUND_TOWARD_POS_INF:
      set_d_register_from_double(r1, std::ceil(r2_val));
      break;
    case Assembler::FIDBRA_ROUND_TOWARD_NEG_INF:
      set_d_register_from_double(r1, std::floor(r2_val));
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  return length;
}

EVALUATE(LXR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LPDFR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LNDFR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LCDFR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LZER) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LZDR) {
  DCHECK_OPCODE(LZDR);
  DECODE_RRE_INSTRUCTION_NO_R2(r1);
  set_d_register_from_double(r1, 0.0);
  return length;
}

EVALUATE(LZXR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SFPC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SFASR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(EFPC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CELFBR) {
  DCHECK_OPCODE(CELFBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  float r1_val = static_cast<float>(r2_val);
  set_d_register_from_float32(r1, r1_val);
  return length;
}

EVALUATE(CDLFBR) {
  DCHECK_OPCODE(CDLFBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  double r1_val = static_cast<double>(r2_val);
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(CXLFBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CEFBRA) {
  DCHECK_OPCODE(CEFBRA);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t fr2_val = get_low_register<int32_t>(r2);
  float fr1_val = static_cast<float>(fr2_val);
  set_d_register_from_float32(r1, fr1_val);
  return length;
}

EVALUATE(CDFBRA) {
  DCHECK_OPCODE(CDFBRA);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  double r1_val = static_cast<double>(r2_val);
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(CXFBRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CFEBRA) {
  DCHECK_OPCODE(CFEBRA);
  DECODE_RRE_INSTRUCTION_M3(r1, r2, mask_val);
  float r2_fval = get_float32_from_d_register(r2);
  int32_t r1_val = 0;

  SetS390RoundConditionCode(r2_fval, INT32_MAX, INT32_MIN);

  switch (mask_val) {
    case CURRENT_ROUNDING_MODE:
    case ROUND_TO_PREPARE_FOR_SHORTER_PRECISION: {
      r1_val = static_cast<int32_t>(r2_fval);
      break;
    }
    case ROUND_TO_NEAREST_WITH_TIES_AWAY_FROM_0: {
      float ceil_val = std::ceil(r2_fval);
      float floor_val = std::floor(r2_fval);
      float sub_val1 = std::fabs(r2_fval - floor_val);
      float sub_val2 = std::fabs(r2_fval - ceil_val);
      if (sub_val1 > sub_val2) {
        r1_val = static_cast<int32_t>(ceil_val);
      } else if (sub_val1 < sub_val2) {
        r1_val = static_cast<int32_t>(floor_val);
      } else {  // round away from zero:
        if (r2_fval > 0.0) {
          r1_val = static_cast<int32_t>(ceil_val);
        } else {
          r1_val = static_cast<int32_t>(floor_val);
        }
      }
      break;
    }
    case ROUND_TO_NEAREST_WITH_TIES_TO_EVEN: {
      float ceil_val = std::ceil(r2_fval);
      float floor_val = std::floor(r2_fval);
      float sub_val1 = std::fabs(r2_fval - floor_val);
      float sub_val2 = std::fabs(r2_fval - ceil_val);
      if (sub_val1 > sub_val2) {
        r1_val = static_cast<int32_t>(ceil_val);
      } else if (sub_val1 < sub_val2) {
        r1_val = static_cast<int32_t>(floor_val);
      } else {  // check which one is even:
        int32_t c_v = static_cast<int32_t>(ceil_val);
        int32_t f_v = static_cast<int32_t>(floor_val);
        if (f_v % 2 == 0)
          r1_val = f_v;
        else
          r1_val = c_v;
      }
      break;
    }
    case ROUND_TOWARD_0: {
      // check for overflow, cast r2_fval to 64bit integer
      // then check value within the range of INT_MIN and INT_MAX
      // and set condition code accordingly
      int64_t temp = static_cast<int64_t>(r2_fval);
      if (temp < INT_MIN || temp > INT_MAX) {
        condition_reg_ = CC_OF;
      }
      r1_val = static_cast<int32_t>(r2_fval);
      break;
    }
    case ROUND_TOWARD_PLUS_INFINITE: {
      r1_val = static_cast<int32_t>(std::ceil(r2_fval));
      break;
    }
    case ROUND_TOWARD_MINUS_INFINITE: {
      // check for overflow, cast r2_fval to 64bit integer
      // then check value within the range of INT_MIN and INT_MAX
      // and set condition code accordingly
      int64_t temp = static_cast<int64_t>(std::floor(r2_fval));
      if (temp < INT_MIN || temp > INT_MAX) {
        condition_reg_ = CC_OF;
      }
      r1_val = static_cast<int32_t>(std::floor(r2_fval));
      break;
    }
    default:
      UNREACHABLE();
  }
  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(CFDBRA) {
  DCHECK_OPCODE(CFDBRA);
  DECODE_RRE_INSTRUCTION_M3(r1, r2, mask_val);
  double r2_val = get_double_from_d_register(r2);
  int32_t r1_val = 0;

  SetS390RoundConditionCode(r2_val, INT32_MAX, INT32_MIN);

  switch (mask_val) {
    case CURRENT_ROUNDING_MODE:
    case ROUND_TO_PREPARE_FOR_SHORTER_PRECISION: {
      r1_val = static_cast<int32_t>(r2_val);
      break;
    }
    case ROUND_TO_NEAREST_WITH_TIES_AWAY_FROM_0: {
      double ceil_val = std::ceil(r2_val);
      double floor_val = std::floor(r2_val);
      double sub_val1 = std::fabs(r2_val - floor_val);
      double sub_val2 = std::fabs(r2_val - ceil_val);
      if (sub_val1 > sub_val2) {
        r1_val = static_cast<int32_t>(ceil_val);
      } else if (sub_val1 < sub_val2) {
        r1_val = static_cast<int32_t>(floor_val);
      } else {  // round away from zero:
        if (r2_val > 0.0) {
          r1_val = static_cast<int32_t>(ceil_val);
        } else {
          r1_val = static_cast<int32_t>(floor_val);
        }
      }
      break;
    }
    case ROUND_TO_NEAREST_WITH_TIES_TO_EVEN: {
      double ceil_val = std::ceil(r2_val);
      double floor_val = std::floor(r2_val);
      double sub_val1 = std::fabs(r2_val - floor_val);
      double sub_val2 = std::fabs(r2_val - ceil_val);
      if (sub_val1 > sub_val2) {
        r1_val = static_cast<int32_t>(ceil_val);
      } else if (sub_val1 < sub_val2) {
        r1_val = static_cast<int32_t>(floor_val);
      } else {  // check which one is even:
        int32_t c_v = static_cast<int32_t>(ceil_val);
        int32_t f_v = static_cast<int32_t>(floor_val);
        if (f_v % 2 == 0)
          r1_val = f_v;
        else
          r1_val = c_v;
      }
      break;
    }
    case ROUND_TOWARD_0: {
      // check for overflow, cast r2_val to 64bit integer
      // then check value within the range of INT_MIN and INT_MAX
      // and set condition code accordingly
      int64_t temp = static_cast<int64_t>(r2_val);
      if (temp < INT_MIN || temp > INT_MAX) {
        condition_reg_ = CC_OF;
      }
      r1_val = static_cast<int32_t>(r2_val);
      break;
    }
    case ROUND_TOWARD_PLUS_INFINITE: {
      r1_val = static_cast<int32_t>(std::ceil(r2_val));
      break;
    }
    case ROUND_TOWARD_MINUS_INFINITE: {
      // check for overflow, cast r2_val to 64bit integer
      // then check value within the range of INT_MIN and INT_MAX
      // and set condition code accordingly
      int64_t temp = static_cast<int64_t>(std::floor(r2_val));
      if (temp < INT_MIN || temp > INT_MAX) {
        condition_reg_ = CC_OF;
      }
      r1_val = static_cast<int32_t>(std::floor(r2_val));
      break;
    }
    default:
      UNREACHABLE();
  }
  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(CFXBRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLFEBR) {
  DCHECK_OPCODE(CLFEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float r2_val = get_float32_from_d_register(r2);
  uint32_t r1_val = static_cast<uint32_t>(r2_val);
  set_low_register(r1, r1_val);
  SetS390ConvertConditionCode<double>(r2_val, r1_val, UINT32_MAX);
  return length;
}

EVALUATE(CLFDBR) {
  DCHECK_OPCODE(CLFDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double a = get_double_from_d_register(r2);
  double n = std::round(a);
  uint32_t r1_val = static_cast<uint32_t>(n);
  set_low_register(r1, r1_val);
  if (std::isfinite(a) && a < 0.0) {
    DCHECK(n <= 0.0 && std::isfinite(n));
    condition_reg_ = (n < 0.0) ? 0x1 : 0x4;
  } else if (a == 0.0) {
    condition_reg_ = 0x8;
  } else if (std::isfinite(a) && a > 0.0) {
    DCHECK(n >= 0.0 && std::isfinite(n));
    condition_reg_ = (n <= static_cast<double>(UINT32_MAX)) ? 0x2 : 0x1;
  } else {
    condition_reg_ = 0x1;
  }
  return length;
}

EVALUATE(CLFXBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CELGBR) {
  DCHECK_OPCODE(CELGBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint64_t r2_val = get_register(r2);
  float r1_val = static_cast<float>(r2_val);
  set_d_register_from_float32(r1, r1_val);
  return length;
}

EVALUATE(CDLGBR) {
  DCHECK_OPCODE(CDLGBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint64_t r2_val = get_register(r2);
  double r1_val = static_cast<double>(r2_val);
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(CXLGBR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CEGBRA) {
  DCHECK_OPCODE(CEGBRA);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t fr2_val = get_register(r2);
  float fr1_val = static_cast<float>(fr2_val);
  set_d_register_from_float32(r1, fr1_val);
  return length;
}

EVALUATE(CDGBRA) {
  DCHECK_OPCODE(CDGBRA);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_register(r2);
  double r1_val = static_cast<double>(r2_val);
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(CXGBRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGEBRA) {
  DCHECK_OPCODE(CGEBRA);
  DECODE_RRE_INSTRUCTION_M3(r1, r2, mask_val);
  float r2_fval = get_float32_from_d_register(r2);
  int64_t r1_val = 0;

  SetS390RoundConditionCode(r2_fval, INT64_MAX, INT64_MIN);

  switch (mask_val) {
    case CURRENT_ROUNDING_MODE:
    case ROUND_TO_NEAREST_WITH_TIES_AWAY_FROM_0:
    case ROUND_TO_PREPARE_FOR_SHORTER_PRECISION: {
      UNIMPLEMENTED();
      break;
    }
    case ROUND_TO_NEAREST_WITH_TIES_TO_EVEN: {
      float ceil_val = std::ceil(r2_fval);
      float floor_val = std::floor(r2_fval);
      if (std::abs(r2_fval - floor_val) > std::abs(r2_fval - ceil_val)) {
        r1_val = static_cast<int64_t>(ceil_val);
      } else if (std::abs(r2_fval - floor_val) < std::abs(r2_fval - ceil_val)) {
        r1_val = static_cast<int64_t>(floor_val);
      } else {  // check which one is even:
        int64_t c_v = static_cast<int64_t>(ceil_val);
        int64_t f_v = static_cast<int64_t>(floor_val);
        if (f_v % 2 == 0)
          r1_val = f_v;
        else
          r1_val = c_v;
      }
      break;
    }
    case ROUND_TOWARD_0: {
      r1_val = static_cast<int64_t>(r2_fval);
      break;
    }
    case ROUND_TOWARD_PLUS_INFINITE: {
      r1_val = static_cast<int64_t>(std::ceil(r2_fval));
      break;
    }
    case ROUND_TOWARD_MINUS_INFINITE: {
      r1_val = static_cast<int64_t>(std::floor(r2_fval));
      break;
    }
    default:
      UNREACHABLE();
  }
  set_register(r1, r1_val);
  return length;
}

EVALUATE(CGDBRA) {
  DCHECK_OPCODE(CGDBRA);
  DECODE_RRE_INSTRUCTION_M3(r1, r2, mask_val);
  double r2_val = get_double_from_d_register(r2);
  int64_t r1_val = 0;

  SetS390RoundConditionCode(r2_val, INT64_MAX, INT64_MIN);

  switch (mask_val) {
    case CURRENT_ROUNDING_MODE:
    case ROUND_TO_NEAREST_WITH_TIES_AWAY_FROM_0:
    case ROUND_TO_PREPARE_FOR_SHORTER_PRECISION: {
      UNIMPLEMENTED();
      break;
    }
    case ROUND_TO_NEAREST_WITH_TIES_TO_EVEN: {
      double ceil_val = std::ceil(r2_val);
      double floor_val = std::floor(r2_val);
      if (std::abs(r2_val - floor_val) > std::abs(r2_val - ceil_val)) {
        r1_val = static_cast<int64_t>(ceil_val);
      } else if (std::abs(r2_val - floor_val) < std::abs(r2_val - ceil_val)) {
        r1_val = static_cast<int64_t>(floor_val);
      } else {  // check which one is even:
        int64_t c_v = static_cast<int64_t>(ceil_val);
        int64_t f_v = static_cast<int64_t>(floor_val);
        if (f_v % 2 == 0)
          r1_val = f_v;
        else
          r1_val = c_v;
      }
      break;
    }
    case ROUND_TOWARD_0: {
      r1_val = static_cast<int64_t>(r2_val);
      break;
    }
    case ROUND_TOWARD_PLUS_INFINITE: {
      r1_val = static_cast<int64_t>(std::ceil(r2_val));
      break;
    }
    case ROUND_TOWARD_MINUS_INFINITE: {
      r1_val = static_cast<int64_t>(std::floor(r2_val));
      break;
    }
    default:
      UNREACHABLE();
  }
  set_register(r1, r1_val);
  return length;
}

EVALUATE(CGXBRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLGEBR) {
  DCHECK_OPCODE(CLGEBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  float r2_val = get_float32_from_d_register(r2);
  uint64_t r1_val = static_cast<uint64_t>(r2_val);
  set_register(r1, r1_val);
  SetS390ConvertConditionCode<double>(r2_val, r1_val, UINT64_MAX);
  return length;
}

EVALUATE(CLGDBR) {
  DCHECK_OPCODE(CLGDBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  double r2_val = get_double_from_d_register(r2);
  uint64_t r1_val = static_cast<uint64_t>(r2_val);
  set_register(r1, r1_val);
  SetS390ConvertConditionCode<double>(r2_val, r1_val, UINT64_MAX);
  return length;
}

EVALUATE(CFER) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CFDR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CFXR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LDGR) {
  DCHECK_OPCODE(LDGR);
  // Load FPR from GPR (L <- 64)
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint64_t int_val = get_register(r2);
  // double double_val = bit_cast<double, uint64_t>(int_val);
  // set_d_register_from_double(rreInst->R1Value(), double_val);
  set_d_register(r1, int_val);
  return length;
}

EVALUATE(CGER) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGDR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGXR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LGDR) {
  DCHECK_OPCODE(LGDR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  // Load GPR from FPR (64 <- L)
  int64_t double_val = get_d_register(r2);
  set_register(r1, double_val);
  return length;
}

EVALUATE(MDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MDTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DDTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ADTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SDTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LDETR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LEDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LTDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(FIDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MXTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DXTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AXTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SXTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LXDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LDXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LTXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(FIXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGDTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CUDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(EEDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ESDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGXTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CUXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CSXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(EEXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ESXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDGTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDUTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDSTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CEDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(QADTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(IEDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RRDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXGTRA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXUTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXSTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CEXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(QAXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(IEXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RRXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LPGR) {
  DCHECK_OPCODE(LPGR);
  // Load Positive (32)
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_register(r2);
  r2_val = (r2_val < 0) ? -r2_val : r2_val;  // If negative, then negate it.
  set_register(r1, r2_val);
  SetS390ConditionCode<int64_t>(r2_val, 0);
  if (r2_val == (static_cast<int64_t>(1) << 63)) {
    SetS390OverflowCode(true);
  }
  return length;
}

EVALUATE(LNGR) {
  DCHECK_OPCODE(LNGR);
  // Load Negative (64)
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_register(r2);
  r2_val = (r2_val >= 0) ? -r2_val : r2_val;  // If pos, then negate it.
  set_register(r1, r2_val);
  condition_reg_ = (r2_val == 0) ? CC_EQ : CC_LT;  // CC0 - result is zero
  // CC1 - result is negative
  return length;
}

EVALUATE(LTGR) {
  DCHECK_OPCODE(LTGR);
  // Load Register (64)
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_register(r2);
  SetS390ConditionCode<int64_t>(r2_val, 0);
  set_register(r1, get_register(r2));
  return length;
}

EVALUATE(LCGR) {
  DCHECK_OPCODE(LCGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_register(r2);
  int64_t result = 0;
  bool isOF = false;
#ifdef V8_TARGET_ARCH_S390X
  isOF = __builtin_ssubl_overflow(0L, r2_val, &result);
#else
  isOF = __builtin_ssubll_overflow(0L, r2_val, &result);
#endif
  set_register(r1, result);
  SetS390ConditionCode<int64_t>(result, 0);
  if (isOF) {
    SetS390OverflowCode(true);
  }
  return length;
}

EVALUATE(SGR) {
  DCHECK_OPCODE(SGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r1_val = get_register(r1);
  int64_t r2_val = get_register(r2);
  bool isOF = false;
  isOF = CheckOverflowForIntSub(r1_val, r2_val, int64_t);
  r1_val -= r2_val;
  SetS390ConditionCode<int64_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, r1_val);
  return length;
}

EVALUATE(ALGR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLGR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSGR) {
  DCHECK_OPCODE(MSGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r1_val = get_register(r1);
  int64_t r2_val = get_register(r2);
  set_register(r1, r1_val * r2_val);
  return length;
}

EVALUATE(MSGRKC) {
  DCHECK_OPCODE(MSGRKC);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  int64_t r2_val = get_register(r2);
  int64_t r3_val = get_register(r3);
  volatile int64_t result64 = r2_val * r3_val;
  bool isOF = ((r2_val == -1 && result64 == (static_cast<int64_t>(1L) << 63)) ||
               (r2_val != 0 && result64 / r2_val != r3_val));
  SetS390ConditionCode<int64_t>(result64, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, result64);
  return length;
}

EVALUATE(DSGR) {
  DCHECK_OPCODE(DSGR);
  DECODE_RRE_INSTRUCTION(r1, r2);

  DCHECK_EQ(r1 % 2, 0);

  int64_t dividend = get_register(r1 + 1);
  int64_t divisor = get_register(r2);
  set_register(r1, dividend % divisor);
  set_register(r1 + 1, dividend / divisor);
  return length;
}

EVALUATE(LRVGR) {
  DCHECK_OPCODE(LRVGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_register(r2);
  int64_t r1_val = ByteReverse(r2_val);

  set_register(r1, r1_val);
  return length;
}

EVALUATE(LPGFR) {
  DCHECK_OPCODE(LPGFR);
  // Load Positive (32)
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  // If negative, then negate it.
  int64_t r1_val = static_cast<int64_t>((r2_val < 0) ? -r2_val : r2_val);
  set_register(r1, r1_val);
  SetS390ConditionCode<int64_t>(r1_val, 0);
  return length;
}

EVALUATE(LNGFR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LTGFR) {
  DCHECK_OPCODE(LTGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  // Load and Test Register (64 <- 32)  (Sign Extends 32-bit val)
  // Load Register (64 <- 32)  (Sign Extends 32-bit val)
  int32_t r2_val = get_low_register<int32_t>(r2);
  int64_t result = static_cast<int64_t>(r2_val);
  set_register(r1, result);
  SetS390ConditionCode<int64_t>(result, 0);
  return length;
}

EVALUATE(LCGFR) {
  DCHECK_OPCODE(LCGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  // Load and Test Register (64 <- 32)  (Sign Extends 32-bit val)
  // Load Register (64 <- 32)  (Sign Extends 32-bit val)
  int32_t r2_val = get_low_register<int32_t>(r2);
  int64_t result = static_cast<int64_t>(r2_val);
  set_register(r1, result);
  return length;
}

EVALUATE(LLGFR) {
  DCHECK_OPCODE(LLGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  uint64_t r2_finalval = (static_cast<uint64_t>(r2_val) & 0x00000000ffffffff);
  set_register(r1, r2_finalval);
  return length;
}

EVALUATE(LLGTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AGFR) {
  DCHECK_OPCODE(AGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  // Add Register (64 <- 32)  (Sign Extends 32-bit val)
  int64_t r1_val = get_register(r1);
  int64_t r2_val = static_cast<int64_t>(get_low_register<int32_t>(r2));
  bool isOF = CheckOverflowForIntAdd(r1_val, r2_val, int64_t);
  r1_val += r2_val;
  SetS390ConditionCode<int64_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, r1_val);
  return length;
}

EVALUATE(SGFR) {
  DCHECK_OPCODE(SGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  // Sub Reg (64 <- 32)
  int64_t r1_val = get_register(r1);
  int64_t r2_val = static_cast<int64_t>(get_low_register<int32_t>(r2));
  bool isOF = false;
  isOF = CheckOverflowForIntSub(r1_val, r2_val, int64_t);
  r1_val -= r2_val;
  SetS390ConditionCode<int64_t>(r1_val, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, r1_val);
  return length;
}

EVALUATE(ALGFR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLGFR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSGFR) {
  DCHECK_OPCODE(MSGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r1_val = get_register(r1);
  int64_t r2_val = static_cast<int64_t>(get_low_register<int32_t>(r2));
  int64_t product = r1_val * r2_val;
  set_register(r1, product);
  return length;
}

EVALUATE(DSGFR) {
  DCHECK_OPCODE(DSGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  DCHECK_EQ(r1 % 2, 0);
  int64_t r1_val = get_register(r1 + 1);
  int64_t r2_val = static_cast<int64_t>(get_low_register<int32_t>(r2));
  int64_t quotient = r1_val / r2_val;
  int64_t remainder = r1_val % r2_val;
  set_register(r1, remainder);
  set_register(r1 + 1, quotient);
  return length;
}

EVALUATE(KMAC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LRVR) {
  DCHECK_OPCODE(LRVR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t r1_val = ByteReverse(r2_val);

  set_low_register(r1, r1_val);
  return length;
}

EVALUATE(CGR) {
  DCHECK_OPCODE(CGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  // Compare (64)
  int64_t r1_val = get_register(r1);
  int64_t r2_val = get_register(r2);
  SetS390ConditionCode<int64_t>(r1_val, r2_val);
  return length;
}

EVALUATE(CLGR) {
  DCHECK_OPCODE(CLGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  // Compare Logical (64)
  uint64_t r1_val = static_cast<uint64_t>(get_register(r1));
  uint64_t r2_val = static_cast<uint64_t>(get_register(r2));
  SetS390ConditionCode<uint64_t>(r1_val, r2_val);
  return length;
}

EVALUATE(KMF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KMO) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PCC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KMCTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KM) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KMC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGFR) {
  DCHECK_OPCODE(CGFR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  // Compare (64)
  int64_t r1_val = get_register(r1);
  int64_t r2_val = static_cast<int64_t>(get_low_register<int32_t>(r2));
  SetS390ConditionCode<int64_t>(r1_val, r2_val);
  return length;
}

EVALUATE(KIMD) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KLMD) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CFDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLGDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLFDTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BCTGR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CFXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLFXTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDFTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDLGTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDLFTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXFTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXLGTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXLFTR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGRT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NGR) {
  DCHECK_OPCODE(NGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r1_val = get_register(r1);
  int64_t r2_val = get_register(r2);
  r1_val &= r2_val;
  SetS390BitWiseConditionCode<uint64_t>(r1_val);
  set_register(r1, r1_val);
  return length;
}

EVALUATE(OGR) {
  DCHECK_OPCODE(OGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r1_val = get_register(r1);
  int64_t r2_val = get_register(r2);
  r1_val |= r2_val;
  SetS390BitWiseConditionCode<uint64_t>(r1_val);
  set_register(r1, r1_val);
  return length;
}

EVALUATE(XGR) {
  DCHECK_OPCODE(XGR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r1_val = get_register(r1);
  int64_t r2_val = get_register(r2);
  r1_val ^= r2_val;
  SetS390BitWiseConditionCode<uint64_t>(r1_val);
  set_register(r1, r1_val);
  return length;
}

EVALUATE(FLOGR) {
  DCHECK_OPCODE(FLOGR);
  DECODE_RRE_INSTRUCTION(r1, r2);

  DCHECK_EQ(r1 % 2, 0);

  int64_t r2_val = get_register(r2);

  int i = 0;
  for (; i < 64; i++) {
    if (r2_val < 0) break;
    r2_val <<= 1;
  }

  r2_val = get_register(r2);

  int64_t mask = ~(1 << (63 - i));
  set_register(r1, i);
  set_register(r1 + 1, r2_val & mask);
  return length;
}

EVALUATE(LLGCR) {
  DCHECK_OPCODE(LLGCR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint64_t r2_val = get_low_register<uint64_t>(r2);
  r2_val <<= 56;
  r2_val >>= 56;
  set_register(r1, r2_val);
  return length;
}

EVALUATE(LLGHR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MLGR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DLGR) {
  DCHECK_OPCODE(DLGR);
#ifdef V8_TARGET_ARCH_S390X
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint64_t r1_val = get_register(r1);
  uint64_t r2_val = get_register(r2);
  DCHECK_EQ(r1 % 2, 0);
  unsigned __int128 dividend = static_cast<unsigned __int128>(r1_val) << 64;
  dividend += get_register(r1 + 1);
  uint64_t remainder = dividend % r2_val;
  uint64_t quotient = dividend / r2_val;
  set_register(r1, remainder);
  set_register(r1 + 1, quotient);
  return length;
#else
  // 32 bit arch doesn't support __int128 type
  USE(instr);
  UNREACHABLE();
#endif
}

EVALUATE(ALCGR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLBGR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(EPSW) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TRTT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TRTO) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TROT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TROO) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLCR) {
  DCHECK_OPCODE(LLCR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  r2_val <<= 24;
  r2_val >>= 24;
  set_low_register(r1, r2_val);
  return length;
}

EVALUATE(LLHR) {
  DCHECK_OPCODE(LLHR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  r2_val <<= 16;
  r2_val >>= 16;
  set_low_register(r1, r2_val);
  return length;
}

EVALUATE(MLR) {
  DCHECK_OPCODE(MLR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  DCHECK_EQ(r1 % 2, 0);

  uint32_t r1_val = get_low_register<uint32_t>(r1 + 1);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  uint64_t product =
      static_cast<uint64_t>(r1_val) * static_cast<uint64_t>(r2_val);
  int32_t high_bits = product >> 32;
  int32_t low_bits = product & 0x00000000FFFFFFFF;
  set_low_register(r1, high_bits);
  set_low_register(r1 + 1, low_bits);
  return length;
}

EVALUATE(DLR) {
  DCHECK_OPCODE(DLR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  DCHECK_EQ(r1 % 2, 0);
  uint64_t dividend = static_cast<uint64_t>(r1_val) << 32;
  dividend += get_low_register<uint32_t>(r1 + 1);
  uint32_t remainder = dividend % r2_val;
  uint32_t quotient = dividend / r2_val;
  r1_val = remainder;
  set_low_register(r1, remainder);
  set_low_register(r1 + 1, quotient);
  return length;
}

EVALUATE(ALCR) {
  DCHECK_OPCODE(ALCR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  uint32_t alu_out = 0;
  bool isOF = false;

  alu_out = r1_val + r2_val;
  bool isOF_original = CheckOverflowForUIntAdd(r1_val, r2_val);
  if (TestConditionCode((Condition)2) || TestConditionCode((Condition)3)) {
    alu_out = alu_out + 1;
    isOF = isOF_original || CheckOverflowForUIntAdd(alu_out, 1);
  } else {
    isOF = isOF_original;
  }
  set_low_register(r1, alu_out);
  SetS390ConditionCodeCarry<uint32_t>(alu_out, isOF);
  return length;
}

EVALUATE(SLBR) {
  DCHECK_OPCODE(SLBR);
  DECODE_RRE_INSTRUCTION(r1, r2);
  uint32_t r1_val = get_low_register<uint32_t>(r1);
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  uint32_t alu_out = 0;
  bool isOF = false;

  alu_out = r1_val - r2_val;
  bool isOF_original = CheckOverflowForUIntSub(r1_val, r2_val);
  if (TestConditionCode((Condition)2) || TestConditionCode((Condition)3)) {
    alu_out = alu_out - 1;
    isOF = isOF_original || CheckOverflowForUIntSub(alu_out, 1);
  } else {
    isOF = isOF_original;
  }
  set_low_register(r1, alu_out);
  SetS390ConditionCodeCarry<uint32_t>(alu_out, isOF);
  return length;
}

EVALUATE(CU14) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CU24) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CU41) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CU42) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TRTRE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRSTU) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TRTE) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AHHHR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SHHHR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ALHHHR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLHHHR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CHHR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AHHLR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SHHLR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ALHHLR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLHHLR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CHLR) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(POPCNT_Z) {
  DCHECK_OPCODE(POPCNT_Z);
  DECODE_RRE_INSTRUCTION(r1, r2);
  int64_t r2_val = get_register(r2);
  int64_t r1_val = 0;

  uint8_t* r2_val_ptr = reinterpret_cast<uint8_t*>(&r2_val);
  uint8_t* r1_val_ptr = reinterpret_cast<uint8_t*>(&r1_val);
  for (int i = 0; i < 8; i++) {
    uint32_t x = static_cast<uint32_t>(r2_val_ptr[i]);
#if defined(__GNUC__)
    r1_val_ptr[i] = __builtin_popcount(x);
#else
#error unsupport __builtin_popcount
#endif
  }
  set_register(r1, static_cast<uint64_t>(r1_val));
  return length;
}

EVALUATE(LOCGR) {
  DCHECK_OPCODE(LOCGR);
  DECODE_RRF_C_INSTRUCTION(r1, r2, m3);
  if (TestConditionCode(m3)) {
    set_register(r1, get_register(r2));
  }
  return length;
}

EVALUATE(NGRK) {
  DCHECK_OPCODE(NGRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 64-bit Non-clobbering arithmetics / bitwise ops.
  int64_t r2_val = get_register(r2);
  int64_t r3_val = get_register(r3);
  uint64_t bitwise_result = 0;
  bitwise_result = r2_val & r3_val;
  SetS390BitWiseConditionCode<uint64_t>(bitwise_result);
  set_register(r1, bitwise_result);
  return length;
}

EVALUATE(OGRK) {
  DCHECK_OPCODE(OGRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 64-bit Non-clobbering arithmetics / bitwise ops.
  int64_t r2_val = get_register(r2);
  int64_t r3_val = get_register(r3);
  uint64_t bitwise_result = 0;
  bitwise_result = r2_val | r3_val;
  SetS390BitWiseConditionCode<uint64_t>(bitwise_result);
  set_register(r1, bitwise_result);
  return length;
}

EVALUATE(XGRK) {
  DCHECK_OPCODE(XGRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 64-bit Non-clobbering arithmetics / bitwise ops.
  int64_t r2_val = get_register(r2);
  int64_t r3_val = get_register(r3);
  uint64_t bitwise_result = 0;
  bitwise_result = r2_val ^ r3_val;
  SetS390BitWiseConditionCode<uint64_t>(bitwise_result);
  set_register(r1, bitwise_result);
  return length;
}

EVALUATE(AGRK) {
  DCHECK_OPCODE(AGRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 64-bit Non-clobbering arithmetics / bitwise ops.
  int64_t r2_val = get_register(r2);
  int64_t r3_val = get_register(r3);
  bool isOF = CheckOverflowForIntAdd(r2_val, r3_val, int64_t);
  SetS390ConditionCode<int64_t>(r2_val + r3_val, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, r2_val + r3_val);
  return length;
}

EVALUATE(SGRK) {
  DCHECK_OPCODE(SGRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 64-bit Non-clobbering arithmetics / bitwise ops.
  int64_t r2_val = get_register(r2);
  int64_t r3_val = get_register(r3);
  bool isOF = CheckOverflowForIntSub(r2_val, r3_val, int64_t);
  SetS390ConditionCode<int64_t>(r2_val - r3_val, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, r2_val - r3_val);
  return length;
}

EVALUATE(ALGRK) {
  DCHECK_OPCODE(ALGRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 64-bit Non-clobbering unsigned arithmetics
  uint64_t r2_val = get_register(r2);
  uint64_t r3_val = get_register(r3);
  bool isOF = CheckOverflowForUIntAdd(r2_val, r3_val);
  SetS390ConditionCode<uint64_t>(r2_val + r3_val, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, r2_val + r3_val);
  return length;
}

EVALUATE(SLGRK) {
  DCHECK_OPCODE(SLGRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 64-bit Non-clobbering unsigned arithmetics
  uint64_t r2_val = get_register(r2);
  uint64_t r3_val = get_register(r3);
  bool isOF = CheckOverflowForUIntSub(r2_val, r3_val);
  SetS390ConditionCode<uint64_t>(r2_val - r3_val, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, r2_val - r3_val);
  return length;
}

EVALUATE(LOCR) {
  DCHECK_OPCODE(LOCR);
  DECODE_RRF_C_INSTRUCTION(r1, r2, m3);
  if (TestConditionCode(m3)) {
    set_low_register(r1, get_low_register<int32_t>(r2));
  }
  return length;
}

EVALUATE(NRK) {
  DCHECK_OPCODE(NRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 32-bit Non-clobbering arithmetics / bitwise ops
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t r3_val = get_low_register<int32_t>(r3);
  // Assume bitwise operation here
  uint32_t bitwise_result = 0;
  bitwise_result = r2_val & r3_val;
  SetS390BitWiseConditionCode<uint32_t>(bitwise_result);
  set_low_register(r1, bitwise_result);
  return length;
}

EVALUATE(ORK) {
  DCHECK_OPCODE(ORK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 32-bit Non-clobbering arithmetics / bitwise ops
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t r3_val = get_low_register<int32_t>(r3);
  // Assume bitwise operation here
  uint32_t bitwise_result = 0;
  bitwise_result = r2_val | r3_val;
  SetS390BitWiseConditionCode<uint32_t>(bitwise_result);
  set_low_register(r1, bitwise_result);
  return length;
}

EVALUATE(XRK) {
  DCHECK_OPCODE(XRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 32-bit Non-clobbering arithmetics / bitwise ops
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t r3_val = get_low_register<int32_t>(r3);
  // Assume bitwise operation here
  uint32_t bitwise_result = 0;
  bitwise_result = r2_val ^ r3_val;
  SetS390BitWiseConditionCode<uint32_t>(bitwise_result);
  set_low_register(r1, bitwise_result);
  return length;
}

EVALUATE(ARK) {
  DCHECK_OPCODE(ARK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 32-bit Non-clobbering arithmetics / bitwise ops
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t r3_val = get_low_register<int32_t>(r3);
  bool isOF = CheckOverflowForIntAdd(r2_val, r3_val, int32_t);
  SetS390ConditionCode<int32_t>(r2_val + r3_val, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, r2_val + r3_val);
  return length;
}

EVALUATE(SRK) {
  DCHECK_OPCODE(SRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 32-bit Non-clobbering arithmetics / bitwise ops
  int32_t r2_val = get_low_register<int32_t>(r2);
  int32_t r3_val = get_low_register<int32_t>(r3);
  bool isOF = CheckOverflowForIntSub(r2_val, r3_val, int32_t);
  SetS390ConditionCode<int32_t>(r2_val - r3_val, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, r2_val - r3_val);
  return length;
}

EVALUATE(ALRK) {
  DCHECK_OPCODE(ALRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 32-bit Non-clobbering unsigned arithmetics
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  uint32_t r3_val = get_low_register<uint32_t>(r3);
  bool isOF = CheckOverflowForUIntAdd(r2_val, r3_val);
  SetS390ConditionCode<uint32_t>(r2_val + r3_val, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, r2_val + r3_val);
  return length;
}

EVALUATE(SLRK) {
  DCHECK_OPCODE(SLRK);
  DECODE_RRF_A_INSTRUCTION(r1, r2, r3);
  // 32-bit Non-clobbering unsigned arithmetics
  uint32_t r2_val = get_low_register<uint32_t>(r2);
  uint32_t r3_val = get_low_register<uint32_t>(r3);
  bool isOF = CheckOverflowForUIntSub(r2_val, r3_val);
  SetS390ConditionCode<uint32_t>(r2_val - r3_val, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, r2_val - r3_val);
  return length;
}

EVALUATE(LTG) {
  DCHECK_OPCODE(LTG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  int64_t value = ReadDW(addr);
  set_register(r1, value);
  SetS390ConditionCode<int64_t>(value, 0);
  return length;
}

EVALUATE(CVBY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AG) {
  DCHECK_OPCODE(AG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t alu_out = get_register(r1);
  int64_t mem_val = ReadDW(b2_val + x2_val + d2);
  bool isOF = CheckOverflowForIntAdd(alu_out, mem_val, int64_t);
  alu_out += mem_val;
  SetS390ConditionCode<int64_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(SG) {
  DCHECK_OPCODE(SG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t alu_out = get_register(r1);
  int64_t mem_val = ReadDW(b2_val + x2_val + d2);
  bool isOF = CheckOverflowForIntSub(alu_out, mem_val, int64_t);
  alu_out -= mem_val;
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(ALG) {
  DCHECK_OPCODE(ALG);
#ifndef V8_TARGET_ARCH_S390X
  DCHECK(false);
#endif
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  uint64_t r1_val = get_register(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  uint64_t alu_out = r1_val;
  uint64_t mem_val = static_cast<uint64_t>(ReadDW(b2_val + d2_val + x2_val));
  alu_out += mem_val;
  SetS390ConditionCode<uint64_t>(alu_out, 0);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(SLG) {
  DCHECK_OPCODE(SLG);
#ifndef V8_TARGET_ARCH_S390X
  DCHECK(false);
#endif
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  uint64_t r1_val = get_register(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  uint64_t alu_out = r1_val;
  uint64_t mem_val = static_cast<uint64_t>(ReadDW(b2_val + d2_val + x2_val));
  alu_out -= mem_val;
  SetS390ConditionCode<uint64_t>(alu_out, 0);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(MSG) {
  DCHECK_OPCODE(MSG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  int64_t mem_val = ReadDW(b2_val + d2_val + x2_val);
  int64_t r1_val = get_register(r1);
  set_register(r1, mem_val * r1_val);
  return length;
}

EVALUATE(DSG) {
  DCHECK_OPCODE(DSG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  DCHECK_EQ(r1 % 2, 0);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  int64_t mem_val = ReadDW(b2_val + d2_val + x2_val);
  int64_t r1_val = get_register(r1 + 1);
  int64_t quotient = r1_val / mem_val;
  int64_t remainder = r1_val % mem_val;
  set_register(r1, remainder);
  set_register(r1 + 1, quotient);
  return length;
}

EVALUATE(CVBG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LT) {
  DCHECK_OPCODE(LT);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  int32_t value = ReadW(addr, instr);
  set_low_register(r1, value);
  SetS390ConditionCode<int32_t>(value, 0);
  return length;
}

EVALUATE(LGH) {
  DCHECK_OPCODE(LGH);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  int64_t mem_val = static_cast<int64_t>(ReadH(addr, instr));
  set_register(r1, mem_val);
  return length;
}

EVALUATE(LLGF) {
  DCHECK_OPCODE(LLGF);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  uint64_t mem_val = static_cast<uint64_t>(ReadWU(addr, instr));
  set_register(r1, mem_val);
  return length;
}

EVALUATE(LLGT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AGF) {
  DCHECK_OPCODE(AGF);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  uint64_t r1_val = get_register(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  uint64_t alu_out = r1_val;
  uint32_t mem_val = ReadW(b2_val + d2_val + x2_val, instr);
  alu_out += mem_val;
  SetS390ConditionCode<int64_t>(alu_out, 0);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(SGF) {
  DCHECK_OPCODE(SGF);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  uint64_t r1_val = get_register(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  uint64_t alu_out = r1_val;
  uint32_t mem_val = ReadW(b2_val + d2_val + x2_val, instr);
  alu_out -= mem_val;
  SetS390ConditionCode<int64_t>(alu_out, 0);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(ALGF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLGF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSGF) {
  DCHECK_OPCODE(MSGF);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  int64_t mem_val =
      static_cast<int64_t>(ReadW(b2_val + d2_val + x2_val, instr));
  int64_t r1_val = get_register(r1);
  int64_t product = r1_val * mem_val;
  set_register(r1, product);
  return length;
}

EVALUATE(DSGF) {
  DCHECK_OPCODE(DSGF);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  DCHECK_EQ(r1 % 2, 0);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  int64_t mem_val =
      static_cast<int64_t>(ReadW(b2_val + d2_val + x2_val, instr));
  int64_t r1_val = get_register(r1 + 1);
  int64_t quotient = r1_val / mem_val;
  int64_t remainder = r1_val % mem_val;
  set_register(r1, remainder);
  set_register(r1 + 1, quotient);
  return length;
}

EVALUATE(LRVG) {
  DCHECK_OPCODE(LRVG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t mem_addr = b2_val + x2_val + d2;
  int64_t mem_val = ReadW64(mem_addr, instr);
  set_register(r1, ByteReverse(mem_val));
  return length;
}

EVALUATE(LRV) {
  DCHECK_OPCODE(LRV);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t mem_addr = b2_val + x2_val + d2;
  int32_t mem_val = ReadW(mem_addr, instr);
  set_low_register(r1, ByteReverse(mem_val));
  return length;
}

EVALUATE(LRVH) {
  DCHECK_OPCODE(LRVH);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t mem_addr = b2_val + x2_val + d2;
  int16_t mem_val = ReadH(mem_addr, instr);
  int32_t result = ByteReverse(mem_val) & 0x0000ffff;
  result |= r1_val & 0xffff0000;
  set_low_register(r1, result);
  return length;
}

EVALUATE(CG) {
  DCHECK_OPCODE(CG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t alu_out = get_register(r1);
  int64_t mem_val = ReadDW(b2_val + x2_val + d2);
  SetS390ConditionCode<int64_t>(alu_out, mem_val);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(CLG) {
  DCHECK_OPCODE(CLG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t alu_out = get_register(r1);
  int64_t mem_val = ReadDW(b2_val + x2_val + d2);
  SetS390ConditionCode<uint64_t>(alu_out, mem_val);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(NTSTG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CVDY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CVDG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLGF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LTGF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(PFD) {
  DCHECK_OPCODE(PFD);
  USE(instr);
  return 6;
}

EVALUATE(STRV) {
  DCHECK_OPCODE(STRV);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t mem_addr = b2_val + x2_val + d2;
  WriteW(mem_addr, ByteReverse(r1_val), instr);
  return length;
}

EVALUATE(STRVG) {
  DCHECK_OPCODE(STRVG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t r1_val = get_register(r1);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t mem_addr = b2_val + x2_val + d2;
  WriteDW(mem_addr, ByteReverse(r1_val));
  return length;
}

EVALUATE(STRVH) {
  DCHECK_OPCODE(STRVH);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t mem_addr = b2_val + x2_val + d2;
  int16_t result = static_cast<int16_t>(r1_val >> 16);
  WriteH(mem_addr, ByteReverse(result), instr);
  return length;
}

EVALUATE(BCTG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSY) {
  DCHECK_OPCODE(MSY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  int32_t mem_val = ReadW(b2_val + d2_val + x2_val, instr);
  int32_t r1_val = get_low_register<int32_t>(r1);
  set_low_register(r1, mem_val * r1_val);
  return length;
}

EVALUATE(MSC) {
  DCHECK_OPCODE(MSC);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  int32_t mem_val = ReadW(b2_val + d2_val + x2_val, instr);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t result64 =
      static_cast<int64_t>(r1_val) * static_cast<int64_t>(mem_val);
  int32_t result32 = static_cast<int32_t>(result64);
  bool isOF = (static_cast<int64_t>(result32) != result64);
  SetS390ConditionCode<int32_t>(result32, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, result32);
  set_low_register(r1, mem_val * r1_val);
  return length;
}

EVALUATE(NY) {
  DCHECK_OPCODE(NY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int32_t alu_out = get_low_register<int32_t>(r1);
  int32_t mem_val = ReadW(b2_val + x2_val + d2, instr);
  alu_out &= mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(CLY) {
  DCHECK_OPCODE(CLY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  uint32_t alu_out = get_low_register<uint32_t>(r1);
  uint32_t mem_val = ReadWU(b2_val + x2_val + d2, instr);
  SetS390ConditionCode<uint32_t>(alu_out, mem_val);
  return length;
}

EVALUATE(OY) {
  DCHECK_OPCODE(OY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int32_t alu_out = get_low_register<int32_t>(r1);
  int32_t mem_val = ReadW(b2_val + x2_val + d2, instr);
  alu_out |= mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(XY) {
  DCHECK_OPCODE(XY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int32_t alu_out = get_low_register<int32_t>(r1);
  int32_t mem_val = ReadW(b2_val + x2_val + d2, instr);
  alu_out ^= mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(CY) {
  DCHECK_OPCODE(CY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int32_t alu_out = get_low_register<int32_t>(r1);
  int32_t mem_val = ReadW(b2_val + x2_val + d2, instr);
  SetS390ConditionCode<int32_t>(alu_out, mem_val);
  return length;
}

EVALUATE(AY) {
  DCHECK_OPCODE(AY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int32_t alu_out = get_low_register<int32_t>(r1);
  int32_t mem_val = ReadW(b2_val + x2_val + d2, instr);
  bool isOF = false;
  isOF = CheckOverflowForIntAdd(alu_out, mem_val, int32_t);
  alu_out += mem_val;
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(SY) {
  DCHECK_OPCODE(SY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int32_t alu_out = get_low_register<int32_t>(r1);
  int32_t mem_val = ReadW(b2_val + x2_val + d2, instr);
  bool isOF = false;
  isOF = CheckOverflowForIntSub(alu_out, mem_val, int32_t);
  alu_out -= mem_val;
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(MFY) {
  DCHECK_OPCODE(MFY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  DCHECK_EQ(r1 % 2, 0);
  int32_t mem_val = ReadW(b2_val + x2_val + d2, instr);
  int32_t r1_val = get_low_register<int32_t>(r1 + 1);
  int64_t product =
      static_cast<int64_t>(r1_val) * static_cast<int64_t>(mem_val);
  int32_t high_bits = product >> 32;
  r1_val = high_bits;
  int32_t low_bits = product & 0x00000000FFFFFFFF;
  set_low_register(r1, high_bits);
  set_low_register(r1 + 1, low_bits);
  return length;
}

EVALUATE(ALY) {
  DCHECK_OPCODE(ALY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  uint32_t alu_out = get_low_register<uint32_t>(r1);
  uint32_t mem_val = ReadWU(b2_val + x2_val + d2, instr);
  alu_out += mem_val;
  set_low_register(r1, alu_out);
  SetS390ConditionCode<uint32_t>(alu_out, 0);
  return length;
}

EVALUATE(SLY) {
  DCHECK_OPCODE(SLY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  uint32_t alu_out = get_low_register<uint32_t>(r1);
  uint32_t mem_val = ReadWU(b2_val + x2_val + d2, instr);
  alu_out -= mem_val;
  set_low_register(r1, alu_out);
  SetS390ConditionCode<uint32_t>(alu_out, 0);
  return length;
}

EVALUATE(STHY) {
  DCHECK_OPCODE(STHY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  uint16_t value = get_low_register<uint32_t>(r1);
  WriteH(addr, value, instr);
  return length;
}

EVALUATE(LAY) {
  DCHECK_OPCODE(LAY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Load Address
  int rb = b2;
  int rx = x2;
  int offset = d2;
  int64_t rb_val = (rb == 0) ? 0 : get_register(rb);
  int64_t rx_val = (rx == 0) ? 0 : get_register(rx);
  set_register(r1, rx_val + rb_val + offset);
  return length;
}

EVALUATE(STCY) {
  DCHECK_OPCODE(STCY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  uint8_t value = get_low_register<uint32_t>(r1);
  WriteB(addr, value);
  return length;
}

EVALUATE(ICY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAEY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LB) {
  DCHECK_OPCODE(LB);
  // Miscellaneous Loads and Stores
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  int32_t mem_val = ReadB(addr);
  set_low_register(r1, mem_val);
  return length;
}

EVALUATE(LGB) {
  DCHECK_OPCODE(LGB);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  int64_t mem_val = ReadB(addr);
  set_register(r1, mem_val);
  return length;
}

EVALUATE(LHY) {
  DCHECK_OPCODE(LHY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  int32_t result = static_cast<int32_t>(ReadH(addr, instr));
  set_low_register(r1, result);
  return length;
}

EVALUATE(CHY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AHY) {
  DCHECK_OPCODE(AHY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  int32_t mem_val =
      static_cast<int32_t>(ReadH(b2_val + d2_val + x2_val, instr));
  int32_t alu_out = 0;
  bool isOF = false;
  alu_out = r1_val + mem_val;
  isOF = CheckOverflowForIntAdd(r1_val, mem_val, int32_t);
  set_low_register(r1, alu_out);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(SHY) {
  DCHECK_OPCODE(SHY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int32_t r1_val = get_low_register<int32_t>(r1);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  int32_t mem_val =
      static_cast<int32_t>(ReadH(b2_val + d2_val + x2_val, instr));
  int32_t alu_out = 0;
  bool isOF = false;
  alu_out = r1_val - mem_val;
  isOF = CheckOverflowForIntSub(r1_val, mem_val, int64_t);
  set_low_register(r1, alu_out);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(MHY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NG) {
  DCHECK_OPCODE(NG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t alu_out = get_register(r1);
  int64_t mem_val = ReadDW(b2_val + x2_val + d2);
  alu_out &= mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(OG) {
  DCHECK_OPCODE(OG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t alu_out = get_register(r1);
  int64_t mem_val = ReadDW(b2_val + x2_val + d2);
  alu_out |= mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(XG) {
  DCHECK_OPCODE(XG);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t alu_out = get_register(r1);
  int64_t mem_val = ReadDW(b2_val + x2_val + d2);
  alu_out ^= mem_val;
  SetS390BitWiseConditionCode<uint32_t>(alu_out);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(LGAT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MLG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DLG) {
  DCHECK_OPCODE(DLG);
#ifdef V8_TARGET_ARCH_S390X
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  uint64_t r1_val = get_register(r1);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  DCHECK_EQ(r1 % 2, 0);
  unsigned __int128 dividend = static_cast<unsigned __int128>(r1_val) << 64;
  dividend += get_register(r1 + 1);
  int64_t mem_val = ReadDW(b2_val + x2_val + d2);
  uint64_t remainder = dividend % mem_val;
  uint64_t quotient = dividend / mem_val;
  set_register(r1, remainder);
  set_register(r1 + 1, quotient);
  return length;
#else
  // 32 bit arch doesn't support __int128 type
  USE(instr);
  UNREACHABLE();
#endif
}

EVALUATE(ALCG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLBG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STPQ) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LPQ) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLGH) {
  DCHECK_OPCODE(LLGH);
  // Load Logical Halfword
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  uint16_t mem_val = ReadHU(b2_val + d2_val + x2_val, instr);
  set_register(r1, mem_val);
  return length;
}

EVALUATE(LLH) {
  DCHECK_OPCODE(LLH);
  // Load Logical Halfword
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  uint16_t mem_val = ReadHU(b2_val + d2_val + x2_val, instr);
  set_low_register(r1, mem_val);
  return length;
}

EVALUATE(ML) {
  DCHECK_OPCODE(ML);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  DCHECK_EQ(r1 % 2, 0);
  uint32_t mem_val = ReadWU(b2_val + x2_val + d2, instr);
  uint32_t r1_val = get_low_register<uint32_t>(r1 + 1);
  uint64_t product =
      static_cast<uint64_t>(r1_val) * static_cast<uint64_t>(mem_val);
  uint32_t high_bits = product >> 32;
  r1_val = high_bits;
  uint32_t low_bits = product & 0x00000000FFFFFFFF;
  set_low_register(r1, high_bits);
  set_low_register(r1 + 1, low_bits);
  return length;
}

EVALUATE(DL) {
  DCHECK_OPCODE(DL);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  DCHECK_EQ(r1 % 2, 0);
  uint32_t mem_val = ReadWU(b2_val + x2_val + d2, instr);
  uint32_t r1_val = get_low_register<uint32_t>(r1 + 1);
  uint64_t quotient =
      static_cast<uint64_t>(r1_val) / static_cast<uint64_t>(mem_val);
  uint64_t remainder =
      static_cast<uint64_t>(r1_val) % static_cast<uint64_t>(mem_val);
  set_low_register(r1, remainder);
  set_low_register(r1 + 1, quotient);
  return length;
}

EVALUATE(ALC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLGTAT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLGFAT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LBH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STCH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LHH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LLHH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STHH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LFHAT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LFH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STFH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CHF) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVCDK) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVHHI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVGHI) {
  DCHECK_OPCODE(MVGHI);
  // Move Integer (64)
  DECODE_SIL_INSTRUCTION(b1, d1, i2);
  int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
  intptr_t src_addr = b1_val + d1;
  WriteDW(src_addr, i2);
  return length;
}

EVALUATE(MVHI) {
  DCHECK_OPCODE(MVHI);
  // Move Integer (32)
  DECODE_SIL_INSTRUCTION(b1, d1, i2);
  int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
  intptr_t src_addr = b1_val + d1;
  WriteW(src_addr, i2, instr);
  return length;
}

EVALUATE(CHHSI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGHSI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CHSI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLFHSI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TBEGIN) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TBEGINC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LMG) {
  DCHECK_OPCODE(LMG);
  // Store Multiple 64-bits.
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  int rb = b2;
  int offset = d2;

  // Regs roll around if r3 is less than r1.
  // Artificially increase r3 by 16 so we can calculate
  // the number of regs stored properly.
  if (r3 < r1) r3 += 16;

  int64_t rb_val = (rb == 0) ? 0 : get_register(rb);

  // Store each register in ascending order.
  for (int i = 0; i <= r3 - r1; i++) {
    int64_t value = ReadDW(rb_val + offset + 8 * i);
    set_register((r1 + i) % 16, value);
  }
  return length;
}

EVALUATE(SRAG) {
  DCHECK_OPCODE(SRAG);
  // 64-bit non-clobbering shift-left/right arithmetic
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  int64_t r3_val = get_register(r3);
  intptr_t alu_out = 0;
  bool isOF = false;
  alu_out = r3_val >> shiftBits;
  set_register(r1, alu_out);
  SetS390ConditionCode<intptr_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(SLAG) {
  DCHECK_OPCODE(SLAG);
  // 64-bit non-clobbering shift-left/right arithmetic
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  int64_t r3_val = get_register(r3);
  intptr_t alu_out = 0;
  bool isOF = false;
  isOF = CheckOverflowForShiftLeft(r3_val, shiftBits);
  alu_out = r3_val << shiftBits;
  set_register(r1, alu_out);
  SetS390ConditionCode<intptr_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(SRLG) {
  DCHECK_OPCODE(SRLG);
  // For SLLG/SRLG, the 64-bit third operand is shifted the number
  // of bits specified by the second-operand address, and the result is
  // placed at the first-operand location. Except for when the R1 and R3
  // fields designate the same register, the third operand remains
  // unchanged in general register R3.
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  // unsigned
  uint64_t r3_val = get_register(r3);
  uint64_t alu_out = 0;
  alu_out = r3_val >> shiftBits;
  set_register(r1, alu_out);
  return length;
}

EVALUATE(SLLG) {
  DCHECK_OPCODE(SLLG);
  // For SLLG/SRLG, the 64-bit third operand is shifted the number
  // of bits specified by the second-operand address, and the result is
  // placed at the first-operand location. Except for when the R1 and R3
  // fields designate the same register, the third operand remains
  // unchanged in general register R3.
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  // unsigned
  uint64_t r3_val = get_register(r3);
  uint64_t alu_out = 0;
  alu_out = r3_val << shiftBits;
  set_register(r1, alu_out);
  return length;
}

EVALUATE(CS) {
  DCHECK_OPCODE(CS);
  DECODE_RS_A_INSTRUCTION(r1, r3, rb, d2);
  int32_t offset = d2;
  int64_t rb_val = (rb == 0) ? 0 : get_register(rb);
  intptr_t target_addr = static_cast<intptr_t>(rb_val) + offset;

  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r3_val = get_low_register<int32_t>(r3);

  DCHECK_EQ(target_addr & 0x3, 0);
  bool is_success = __atomic_compare_exchange_n(
      reinterpret_cast<int32_t*>(target_addr), &r1_val, r3_val, true,
      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  if (!is_success) {
    set_low_register(r1, r1_val);
    condition_reg_ = 0x4;
  } else {
    condition_reg_ = 0x8;
  }
  return length;
}

EVALUATE(CSY) {
  DCHECK_OPCODE(CSY);
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  int32_t offset = d2;
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t target_addr = static_cast<intptr_t>(b2_val) + offset;

  int32_t r1_val = get_low_register<int32_t>(r1);
  int32_t r3_val = get_low_register<int32_t>(r3);

  DCHECK_EQ(target_addr & 0x3, 0);
  bool is_success = __atomic_compare_exchange_n(
      reinterpret_cast<int32_t*>(target_addr), &r1_val, r3_val, true,
      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  if (!is_success) {
    set_low_register(r1, r1_val);
    condition_reg_ = 0x4;
  } else {
    condition_reg_ = 0x8;
  }
  return length;
}

EVALUATE(CSG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RLLG) {
  DCHECK_OPCODE(RLLG);
  // For SLLG/SRLG, the 64-bit third operand is shifted the number
  // of bits specified by the second-operand address, and the result is
  // placed at the first-operand location. Except for when the R1 and R3
  // fields designate the same register, the third operand remains
  // unchanged in general register R3.
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  // unsigned
  uint64_t r3_val = get_register(r3);
  uint64_t alu_out = 0;
  uint64_t rotateBits = r3_val >> (64 - shiftBits);
  alu_out = (r3_val << shiftBits) | (rotateBits);
  set_register(r1, alu_out);
  return length;
}

EVALUATE(STMG) {
  DCHECK_OPCODE(STMG);
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  int rb = b2;
  int offset = d2;

  // Regs roll around if r3 is less than r1.
  // Artificially increase r3 by 16 so we can calculate
  // the number of regs stored properly.
  if (r3 < r1) r3 += 16;

  int64_t rb_val = (rb == 0) ? 0 : get_register(rb);

  // Store each register in ascending order.
  for (int i = 0; i <= r3 - r1; i++) {
    int64_t value = get_register((r1 + i) % 16);
    WriteDW(rb_val + offset + 8 * i, value);
  }
  return length;
}

EVALUATE(STMH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STCMH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STCMY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDSY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDSG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BXHG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BXLEG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ECAG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TMY) {
  DCHECK_OPCODE(TMY);
  // Test Under Mask (Mem - Imm) (8)
  DECODE_SIY_INSTRUCTION(b1, d1, i2);
  int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
  intptr_t d1_val = d1;
  intptr_t addr = b1_val + d1_val;
  uint8_t mem_val = ReadB(addr);
  uint8_t imm_val = i2;
  uint8_t selected_bits = mem_val & imm_val;
  // CC0: Selected bits are zero
  // CC1: Selected bits mixed zeros and ones
  // CC3: Selected bits all ones
  if (0 == selected_bits) {
    condition_reg_ = CC_EQ;  // CC0
  } else if (selected_bits == imm_val) {
    condition_reg_ = 0x1;  // CC3
  } else {
    condition_reg_ = 0x4;  // CC1
  }
  return length;
}

EVALUATE(MVIY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(NIY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLIY) {
  DCHECK_OPCODE(CLIY);
  DECODE_SIY_INSTRUCTION(b1, d1, i2);
  // Compare Immediate (Mem - Imm) (8)
  int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
  intptr_t d1_val = d1;
  intptr_t addr = b1_val + d1_val;
  uint8_t mem_val = ReadB(addr);
  uint8_t imm_val = i2;
  SetS390ConditionCode<uint8_t>(mem_val, imm_val);
  return length;
}

EVALUATE(OIY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(XIY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ASI) {
  DCHECK_OPCODE(ASI);
  // TODO(bcleung): Change all fooInstr->I2Value() to template functions.
  // The below static cast to 8 bit and then to 32 bit is necessary
  // because siyInstr->I2Value() returns a uint8_t, which a direct
  // cast to int32_t could incorrectly interpret.
  DECODE_SIY_INSTRUCTION(b1, d1, i2_unsigned);
  int8_t i2_8bit = static_cast<int8_t>(i2_unsigned);
  int32_t i2 = static_cast<int32_t>(i2_8bit);
  intptr_t b1_val = (b1 == 0) ? 0 : get_register(b1);

  int d1_val = d1;
  intptr_t addr = b1_val + d1_val;

  int32_t mem_val = ReadW(addr, instr);
  bool isOF = CheckOverflowForIntAdd(mem_val, i2, int32_t);
  int32_t alu_out = mem_val + i2;
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  WriteW(addr, alu_out, instr);
  return length;
}

EVALUATE(ALSI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(AGSI) {
  DCHECK_OPCODE(AGSI);
  // TODO(bcleung): Change all fooInstr->I2Value() to template functions.
  // The below static cast to 8 bit and then to 32 bit is necessary
  // because siyInstr->I2Value() returns a uint8_t, which a direct
  // cast to int32_t could incorrectly interpret.
  DECODE_SIY_INSTRUCTION(b1, d1, i2_unsigned);
  int8_t i2_8bit = static_cast<int8_t>(i2_unsigned);
  int64_t i2 = static_cast<int64_t>(i2_8bit);
  intptr_t b1_val = (b1 == 0) ? 0 : get_register(b1);

  int d1_val = d1;
  intptr_t addr = b1_val + d1_val;

  int64_t mem_val = ReadDW(addr);
  int isOF = CheckOverflowForIntAdd(mem_val, i2, int64_t);
  int64_t alu_out = mem_val + i2;
  SetS390ConditionCode<uint64_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  WriteDW(addr, alu_out);
  return length;
}

EVALUATE(ALGSI) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ICMH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ICMY) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MVCLU) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLCLU) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STMY) {
  DCHECK_OPCODE(STMY);
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // Load/Store Multiple (32)
  int offset = d2;

  // Regs roll around if r3 is less than r1.
  // Artificially increase r3 by 16 so we can calculate
  // the number of regs stored properly.
  if (r3 < r1) r3 += 16;

  int32_t b2_val = (b2 == 0) ? 0 : get_low_register<int32_t>(b2);

  // Store each register in ascending order.
  for (int i = 0; i <= r3 - r1; i++) {
    int32_t value = get_low_register<int32_t>((r1 + i) % 16);
    WriteW(b2_val + offset + 4 * i, value, instr);
  }
  return length;
}

EVALUATE(LMH) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LMY) {
  DCHECK_OPCODE(LMY);
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // Load/Store Multiple (32)
  int offset = d2;

  // Regs roll around if r3 is less than r1.
  // Artificially increase r3 by 16 so we can calculate
  // the number of regs stored properly.
  if (r3 < r1) r3 += 16;

  int32_t b2_val = (b2 == 0) ? 0 : get_low_register<int32_t>(b2);

  // Store each register in ascending order.
  for (int i = 0; i <= r3 - r1; i++) {
    int32_t value = ReadW(b2_val + offset + 4 * i, instr);
    set_low_register((r1 + i) % 16, value);
  }
  return length;
}

EVALUATE(TP) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRAK) {
  DCHECK_OPCODE(SRAK);
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // 32-bit non-clobbering shift-left/right arithmetic
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  int32_t r3_val = get_low_register<int32_t>(r3);
  int32_t alu_out = 0;
  bool isOF = false;
  alu_out = r3_val >> shiftBits;
  set_low_register(r1, alu_out);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(SLAK) {
  DCHECK_OPCODE(SLAK);
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // 32-bit non-clobbering shift-left/right arithmetic
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  int32_t r3_val = get_low_register<int32_t>(r3);
  int32_t alu_out = 0;
  bool isOF = false;
  isOF = CheckOverflowForShiftLeft(r3_val, shiftBits);
  alu_out = r3_val << shiftBits;
  set_low_register(r1, alu_out);
  SetS390ConditionCode<int32_t>(alu_out, 0);
  SetS390OverflowCode(isOF);
  return length;
}

EVALUATE(SRLK) {
  DCHECK_OPCODE(SRLK);
  // For SLLK/SRLL, the 32-bit third operand is shifted the number
  // of bits specified by the second-operand address, and the result is
  // placed at the first-operand location. Except for when the R1 and R3
  // fields designate the same register, the third operand remains
  // unchanged in general register R3.
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  // unsigned
  uint32_t r3_val = get_low_register<uint32_t>(r3);
  uint32_t alu_out = 0;
  alu_out = r3_val >> shiftBits;
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(SLLK) {
  DCHECK_OPCODE(SLLK);
  // For SLLK/SRLL, the 32-bit third operand is shifted the number
  // of bits specified by the second-operand address, and the result is
  // placed at the first-operand location. Except for when the R1 and R3
  // fields designate the same register, the third operand remains
  // unchanged in general register R3.
  DECODE_RSY_A_INSTRUCTION(r1, r3, b2, d2);
  // only takes rightmost 6 bits
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int shiftBits = (b2_val + d2) & 0x3F;
  // unsigned
  uint32_t r3_val = get_low_register<uint32_t>(r3);
  uint32_t alu_out = 0;
  alu_out = r3_val << shiftBits;
  set_low_register(r1, alu_out);
  return length;
}

EVALUATE(LOCG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STOCG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LANG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAOG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAXG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAAG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAALG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LOC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(STOC) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAN) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAO) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAX) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAA) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LAAL) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BRXHG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(BRXLG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RISBLG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RNSBG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ROSBG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RXSBG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RISBGN) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(RISBHG) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGRJ) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGIT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CIT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CLFIT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGIJ) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CIJ) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ALHSIK) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(ALGHSIK) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGRB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CGIB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CIB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LDEB) {
  DCHECK_OPCODE(LDEB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int rb = b2;
  int rx = x2;
  int offset = d2;
  int64_t rb_val = (rb == 0) ? 0 : get_register(rb);
  int64_t rx_val = (rx == 0) ? 0 : get_register(rx);
  float fval = ReadFloat(rx_val + rb_val + offset);
  set_d_register_from_double(r1, static_cast<double>(fval));
  return length;
}

EVALUATE(LXDB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LXEB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MXDB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(KEB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CEB) {
  DCHECK_OPCODE(CEB);

  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  float r1_val = get_float32_from_d_register(r1);
  float fval = ReadFloat(b2_val + x2_val + d2_val);
  SetS390ConditionCode<float>(r1_val, fval);
  return length;
}

EVALUATE(AEB) {
  DCHECK_OPCODE(AEB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  float r1_val = get_float32_from_d_register(r1);
  float fval = ReadFloat(b2_val + x2_val + d2_val);
  r1_val += fval;
  set_d_register_from_float32(r1, r1_val);
  SetS390ConditionCode<float>(r1_val, 0);
  return length;
}

EVALUATE(SEB) {
  DCHECK_OPCODE(SEB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  float r1_val = get_float32_from_d_register(r1);
  float fval = ReadFloat(b2_val + x2_val + d2_val);
  r1_val -= fval;
  set_d_register_from_float32(r1, r1_val);
  SetS390ConditionCode<float>(r1_val, 0);
  return length;
}

EVALUATE(MDEB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(DEB) {
  DCHECK_OPCODE(DEB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  float r1_val = get_float32_from_d_register(r1);
  float fval = ReadFloat(b2_val + x2_val + d2_val);
  r1_val /= fval;
  set_d_register_from_float32(r1, r1_val);
  return length;
}

EVALUATE(MAEB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSEB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TCEB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TCDB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TCXB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SQEB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SQDB) {
  DCHECK_OPCODE(SQDB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  double r1_val = get_double_from_d_register(r1);
  double dbl_val = ReadDouble(b2_val + x2_val + d2_val);
  r1_val = std::sqrt(dbl_val);
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(MEEB) {
  DCHECK_OPCODE(MEEB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  float r1_val = get_float32_from_d_register(r1);
  float fval = ReadFloat(b2_val + x2_val + d2_val);
  r1_val *= fval;
  set_d_register_from_float32(r1, r1_val);
  return length;
}

EVALUATE(KDB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDB) {
  DCHECK_OPCODE(CDB);

  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  double r1_val = get_double_from_d_register(r1);
  double dbl_val = ReadDouble(b2_val + x2_val + d2_val);
  SetS390ConditionCode<double>(r1_val, dbl_val);
  return length;
}

EVALUATE(ADB) {
  DCHECK_OPCODE(ADB);

  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  double r1_val = get_double_from_d_register(r1);
  double dbl_val = ReadDouble(b2_val + x2_val + d2_val);
  r1_val += dbl_val;
  set_d_register_from_double(r1, r1_val);
  SetS390ConditionCode<double>(r1_val, 0);
  return length;
}

EVALUATE(SDB) {
  DCHECK_OPCODE(SDB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  double r1_val = get_double_from_d_register(r1);
  double dbl_val = ReadDouble(b2_val + x2_val + d2_val);
  r1_val -= dbl_val;
  set_d_register_from_double(r1, r1_val);
  SetS390ConditionCode<double>(r1_val, 0);
  return length;
}

EVALUATE(MDB) {
  DCHECK_OPCODE(MDB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  double r1_val = get_double_from_d_register(r1);
  double dbl_val = ReadDouble(b2_val + x2_val + d2_val);
  r1_val *= dbl_val;
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(DDB) {
  DCHECK_OPCODE(DDB);
  DECODE_RXE_INSTRUCTION(r1, b2, x2, d2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  intptr_t d2_val = d2;
  double r1_val = get_double_from_d_register(r1);
  double dbl_val = ReadDouble(b2_val + x2_val + d2_val);
  r1_val /= dbl_val;
  set_d_register_from_double(r1, r1_val);
  return length;
}

EVALUATE(MADB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(MSDB) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLDT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRDT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SLXT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(SRXT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TDCET) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TDGET) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TDCDT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TDGDT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TDCXT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(TDGXT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(LEY) {
  DCHECK_OPCODE(LEY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  float float_val = *reinterpret_cast<float*>(addr);
  set_d_register_from_float32(r1, float_val);
  return length;
}

EVALUATE(LDY) {
  DCHECK_OPCODE(LDY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  uint64_t dbl_val = *reinterpret_cast<uint64_t*>(addr);
  set_d_register(r1, dbl_val);
  return length;
}

EVALUATE(STEY) {
  DCHECK_OPCODE(STEY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  int64_t frs_val = get_d_register(r1) >> 32;
  WriteW(addr, static_cast<int32_t>(frs_val), instr);
  return length;
}

EVALUATE(STDY) {
  DCHECK_OPCODE(STDY);
  DECODE_RXY_A_INSTRUCTION(r1, x2, b2, d2);
  // Miscellaneous Loads and Stores
  int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
  int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
  intptr_t addr = x2_val + b2_val + d2;
  int64_t frs_val = get_d_register(r1);
  WriteDW(addr, frs_val);
  return length;
}

EVALUATE(CZDT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CZXT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CDZT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

EVALUATE(CXZT) {
  UNIMPLEMENTED();
  USE(instr);
  return 0;
}

#undef EVALUATE

}  // namespace internal
}  // namespace v8

#endif  // USE_SIMULATOR
#endif  // V8_TARGET_ARCH_S390
