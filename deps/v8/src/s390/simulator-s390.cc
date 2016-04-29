// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stdlib.h>
#include <cmath>

#if V8_TARGET_ARCH_S390

#include "src/assembler.h"
#include "src/base/bits.h"
#include "src/codegen.h"
#include "src/disasm.h"
#include "src/runtime/runtime-utils.h"
#include "src/s390/constants-s390.h"
#include "src/s390/frames-s390.h"
#include "src/s390/simulator-s390.h"
#if defined(USE_SIMULATOR)

// Only build the simulator if not compiling for real s390 hardware.
namespace v8 {
namespace internal {

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
  ~S390Debugger();

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

S390Debugger::~S390Debugger() {}

#ifdef GENERATED_CODE_COVERAGE
static FILE* coverage_log = NULL;

static void InitializeCoverage() {
  char* file_name = getenv("V8_GENERATED_CODE_COVERAGE_LOG");
  if (file_name != NULL) {
    coverage_log = fopen(file_name, "aw+");
  }
}

void S390Debugger::Stop(Instruction* instr) {
  // Get the stop code.
  uint32_t code = instr->SvcValue() & kStopCodeMask;
  // Retrieve the encoded address, which comes just after this stop.
  char** msg_address =
      reinterpret_cast<char**>(sim_->get_pc() + sizeof(FourByteInstr));
  char* msg = *msg_address;
  DCHECK(msg != NULL);

  // Update this stop description.
  if (isWatchedStop(code) && !watched_stops_[code].desc) {
    watched_stops_[code].desc = msg;
  }

  if (strlen(msg) > 0) {
    if (coverage_log != NULL) {
      fprintf(coverage_log, "%s\n", msg);
      fflush(coverage_log);
    }
    // Overwrite the instruction and address with nops.
    instr->SetInstructionBits(kNopInstr);
    reinterpret_cast<Instruction*>(msg_address)->SetInstructionBits(kNopInstr);
  }
  sim_->set_pc(sim_->get_pc() + sizeof(FourByteInstr) + kPointerSize);
}

#else  // ndef GENERATED_CODE_COVERAGE

static void InitializeCoverage() {}

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
#endif

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
  if (sim_->break_pc_ != NULL) {
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
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }

  sim_->break_pc_ = NULL;
  sim_->break_instr_ = 0;
  return true;
}

void S390Debugger::UndoBreakpoints() {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }
}

void S390Debugger::RedoBreakpoints() {
  if (sim_->break_pc_ != NULL) {
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
        intptr_t value;

        // If at a breakpoint, proceed past it.
        if ((reinterpret_cast<Instruction*>(sim_->get_pc()))
                ->InstructionBits() == 0x7d821008) {
          sim_->set_pc(sim_->get_pc() + sizeof(FourByteInstr));
        } else {
          sim_->ExecuteInstruction(
              reinterpret_cast<Instruction*>(sim_->get_pc()));
        }

        if (argc == 2 && last_pc != sim_->get_pc() && GetValue(arg1, &value)) {
          for (int i = 1; (!sim_->has_bad_pc()) && i < value; i++) {
            disasm::NameConverter converter;
            disasm::Disassembler dasm(converter);
            // use a reasonably large buffer
            v8::internal::EmbeddedVector<char, 256> buffer;
            dasm.InstructionDecode(buffer,
                                   reinterpret_cast<byte*>(sim_->get_pc()));
            PrintF("  0x%08" V8PRIxPTR "  %s\n", sim_->get_pc(),
                   buffer.start());
            sim_->ExecuteInstruction(
                reinterpret_cast<Instruction*>(sim_->get_pc()));
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
                     Register::from_code(i).ToString(), value);
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
                     Register::from_code(i).ToString(), value, value);
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
                     DoubleRegister::from_code(i).ToString(), fvalue, as_words);
            }
          } else if (strcmp(arg1, "alld") == 0) {
            for (int i = 0; i < DoubleRegister::kNumRegisters; i++) {
              dvalue = GetFPDoubleRegisterValue(i);
              uint64_t as_words = bit_cast<uint64_t>(dvalue);
              PrintF("%3s: %f 0x%08x %08x\n",
                     DoubleRegister::from_code(i).ToString(), dvalue,
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
        intptr_t* cur = NULL;
        intptr_t* end = NULL;
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

        byte* prev = NULL;
        byte* cur = NULL;
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
        if (!DeleteBreakpoint(NULL)) {
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
  DCHECK((reinterpret_cast<intptr_t>(one) & CachePage::kPageMask) == 0);
  DCHECK((reinterpret_cast<intptr_t>(two) & CachePage::kPageMask) == 0);
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

void Simulator::FlushICache(v8::internal::HashMap* i_cache, void* start_addr,
                            size_t size) {
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

CachePage* Simulator::GetCachePage(v8::internal::HashMap* i_cache, void* page) {
  v8::internal::HashMap::Entry* entry =
      i_cache->LookupOrInsert(page, ICacheHash(page));
  if (entry->value == NULL) {
    CachePage* new_page = new CachePage();
    entry->value = new_page;
  }
  return reinterpret_cast<CachePage*>(entry->value);
}

// Flush from start up to and not including start + size.
void Simulator::FlushOnePage(v8::internal::HashMap* i_cache, intptr_t start,
                             int size) {
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

void Simulator::CheckICache(v8::internal::HashMap* i_cache,
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
}

Simulator::Simulator(Isolate* isolate) : isolate_(isolate) {
  i_cache_ = isolate_->simulator_i_cache();
  if (i_cache_ == NULL) {
    i_cache_ = new v8::internal::HashMap(&ICacheMatch);
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
  break_pc_ = NULL;
  break_instr_ = 0;

// make sure our register type can hold exactly 4/8 bytes
#ifdef V8_TARGET_ARCH_S390X
  DCHECK(sizeof(intptr_t) == 8);
#else
  DCHECK(sizeof(intptr_t) == 4);
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
  InitializeCoverage();

  last_debugger_input_ = NULL;
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
        next_(NULL) {
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
    for (; current != NULL; current = current->next_) {
      if (current->external_function_ == external_function) {
        DCHECK_EQ(current->type(), type);
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
void Simulator::TearDown(HashMap* i_cache, Redirection* first) {
  Redirection::DeleteChain(first);
  if (i_cache != nullptr) {
    for (HashMap::Entry* entry = i_cache->Start(); entry != nullptr;
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
  return redirection->address();
}

// Get the active Simulator for the current thread.
Simulator* Simulator::current(Isolate* isolate) {
  v8::internal::Isolate::PerIsolateThreadData* isolate_data =
      isolate->FindOrAllocatePerThreadDataForThisThread();
  DCHECK(isolate_data != NULL);

  Simulator* sim = isolate_data->simulator();
  if (sim == NULL) {
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
                                         intptr_t arg4, intptr_t arg5);
typedef ObjectPair (*SimulatorRuntimePairCall)(intptr_t arg0, intptr_t arg1,
                                               intptr_t arg2, intptr_t arg3,
                                               intptr_t arg4, intptr_t arg5);
typedef ObjectTriple (*SimulatorRuntimeTripleCall)(intptr_t arg0, intptr_t arg1,
                                                   intptr_t arg2, intptr_t arg3,
                                                   intptr_t arg4,
                                                   intptr_t arg5);

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
      const int kArgCount = 6;
      int arg0_regnum = 2;
      intptr_t result_buffer = 0;
      bool uses_result_buffer =
          redirection->type() == ExternalReference::BUILTIN_CALL_TRIPLE ||
          (redirection->type() == ExternalReference::BUILTIN_CALL_PAIR &&
           !ABI_RETURNS_OBJECTPAIR_IN_REGS);
      if (uses_result_buffer) {
        result_buffer = get_register(r2);
        arg0_regnum++;
      }
      intptr_t arg[kArgCount];
      for (int i = 0; i < kArgCount - 1; i++) {
        arg[i] = get_register(arg0_regnum + i);
      }
      intptr_t* stack_pointer = reinterpret_cast<intptr_t*>(get_register(sp));
      arg[5] = stack_pointer[kCalleeRegisterSaveAreaSize / kPointerSize];
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
                     FUNCTION_ADDR(generic_target), dval0, dval1);
              break;
            case ExternalReference::BUILTIN_FP_CALL:
              PrintF("Call to host function at %p with arg %f",
                     FUNCTION_ADDR(generic_target), dval0);
              break;
            case ExternalReference::BUILTIN_FP_INT_CALL:
              PrintF("Call to host function at %p with args %f, %" V8PRIdPTR,
                     FUNCTION_ADDR(generic_target), dval0, ival);
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
              ", %08" V8PRIxPTR ", %08" V8PRIxPTR ", %08" V8PRIxPTR,
              FUNCTION_ADDR(target), arg[0], arg[1], arg[2], arg[3], arg[4],
              arg[5]);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08" V8PRIxPTR "\n",
                   static_cast<intptr_t>(get_register(sp)));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        if (redirection->type() == ExternalReference::BUILTIN_CALL_TRIPLE) {
          SimulatorRuntimeTripleCall target =
              reinterpret_cast<SimulatorRuntimeTripleCall>(external);
          ObjectTriple result =
              target(arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);
          if (::v8::internal::FLAG_trace_sim) {
            PrintF("Returned {%08" V8PRIxPTR ", %08" V8PRIxPTR ", %08" V8PRIxPTR
                   "}\n",
                   reinterpret_cast<intptr_t>(result.x),
                   reinterpret_cast<intptr_t>(result.y),
                   reinterpret_cast<intptr_t>(result.z));
          }
          memcpy(reinterpret_cast<void*>(result_buffer), &result,
                 sizeof(ObjectTriple));
          set_register(r2, result_buffer);
        } else {
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
            intptr_t result =
                target(arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);
            if (::v8::internal::FLAG_trace_sim) {
              PrintF("Returned %08" V8PRIxPTR "\n", result);
            }
            set_register(r2, result);
          }
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
  DCHECK(code <= kMaxStopCode);
  return code < kNumOfWatchedStops;
}

bool Simulator::isEnabledStop(uint32_t code) {
  DCHECK(code <= kMaxStopCode);
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
  DCHECK(code <= kMaxStopCode);
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
  DCHECK(code <= kMaxStopCode);
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

// Method for checking overflow on unsigned addtion
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

// S390 Decode and simulate helpers
bool Simulator::DecodeTwoByte(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();

  switch (op) {
    // RR format instructions
    case AR:
    case SR:
    case MR:
    case DR:
    case OR:
    case NR:
    case XR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      int32_t r1_val = get_low_register<int32_t>(r1);
      int32_t r2_val = get_low_register<int32_t>(r2);
      bool isOF = false;
      switch (op) {
        case AR:
          isOF = CheckOverflowForIntAdd(r1_val, r2_val, int32_t);
          r1_val += r2_val;
          SetS390ConditionCode<int32_t>(r1_val, 0);
          SetS390OverflowCode(isOF);
          break;
        case SR:
          isOF = CheckOverflowForIntSub(r1_val, r2_val, int32_t);
          r1_val -= r2_val;
          SetS390ConditionCode<int32_t>(r1_val, 0);
          SetS390OverflowCode(isOF);
          break;
        case OR:
          r1_val |= r2_val;
          SetS390BitWiseConditionCode<uint32_t>(r1_val);
          break;
        case NR:
          r1_val &= r2_val;
          SetS390BitWiseConditionCode<uint32_t>(r1_val);
          break;
        case XR:
          r1_val ^= r2_val;
          SetS390BitWiseConditionCode<uint32_t>(r1_val);
          break;
        case MR: {
          DCHECK(r1 % 2 == 0);
          r1_val = get_low_register<int32_t>(r1 + 1);
          int64_t product =
              static_cast<int64_t>(r1_val) * static_cast<int64_t>(r2_val);
          int32_t high_bits = product >> 32;
          r1_val = high_bits;
          int32_t low_bits = product & 0x00000000FFFFFFFF;
          set_low_register(r1, high_bits);
          set_low_register(r1 + 1, low_bits);
          break;
        }
        case DR: {
          // reg-reg pair should be even-odd pair, assert r1 is an even register
          DCHECK(r1 % 2 == 0);
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
          break;  // reg pair
        }
        default:
          UNREACHABLE();
          break;
      }
      set_low_register(r1, r1_val);
      break;
    }
    case LR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      set_low_register(r1, get_low_register<int32_t>(r2));
      break;
    }
    case LDR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      int64_t r2_val = get_d_register(r2);
      set_d_register(r1, r2_val);
      break;
    }
    case CR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      int32_t r1_val = get_low_register<int32_t>(r1);
      int32_t r2_val = get_low_register<int32_t>(r2);
      SetS390ConditionCode<int32_t>(r1_val, r2_val);
      break;
    }
    case CLR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      uint32_t r1_val = get_low_register<uint32_t>(r1);
      uint32_t r2_val = get_low_register<uint32_t>(r2);
      SetS390ConditionCode<uint32_t>(r1_val, r2_val);
      break;
    }
    case BCR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
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
      break;
    }
    case LTR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      int32_t r2_val = get_low_register<int32_t>(r2);
      SetS390ConditionCode<int32_t>(r2_val, 0);
      set_low_register(r1, r2_val);
      break;
    }
    case ALR:
    case SLR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      uint32_t r1_val = get_low_register<uint32_t>(r1);
      uint32_t r2_val = get_low_register<uint32_t>(r2);
      uint32_t alu_out = 0;
      bool isOF = false;
      if (ALR == op) {
        alu_out = r1_val + r2_val;
        isOF = CheckOverflowForUIntAdd(r1_val, r2_val);
      } else if (SLR == op) {
        alu_out = r1_val - r2_val;
        isOF = CheckOverflowForUIntSub(r1_val, r2_val);
      } else {
        UNREACHABLE();
      }
      set_low_register(r1, alu_out);
      SetS390ConditionCodeCarry<uint32_t>(alu_out, isOF);
      break;
    }
    case LNR: {
      // Load Negative (32)
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      int32_t r2_val = get_low_register<int32_t>(r2);
      r2_val = (r2_val >= 0) ? -r2_val : r2_val;  // If pos, then negate it.
      set_low_register(r1, r2_val);
      condition_reg_ = (r2_val == 0) ? CC_EQ : CC_LT;  // CC0 - result is zero
      // CC1 - result is negative
      break;
    }
    case BASR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
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
      break;
    }
    case LCR: {
      RRInstruction* rrinst = reinterpret_cast<RRInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
      int32_t r2_val = get_low_register<int32_t>(r2);
      int32_t original_r2_val = r2_val;
      r2_val = ~r2_val;
      r2_val = r2_val + 1;
      set_low_register(r1, r2_val);
      SetS390ConditionCode<int32_t>(r2_val, 0);
      // Checks for overflow where r2_val = -2147483648.
      // Cannot do int comparison due to GCC 4.8 bug on x86.
      // Detect INT_MIN alternatively, as it is the only value where both
      // original and result are negative due to overflow.
      if (r2_val < 0 && original_r2_val < 0) {
        SetS390OverflowCode(true);
      }
      break;
    }
    case BKPT: {
      set_pc(get_pc() + 2);
      S390Debugger dbg(this);
      dbg.Debug();
      break;
    }
    default:
      UNREACHABLE();
      return false;
      break;
  }
  return true;
}

// Decode routine for four-byte instructions
bool Simulator::DecodeFourByte(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();

  // Pre-cast instruction to various types
  RREInstruction* rreInst = reinterpret_cast<RREInstruction*>(instr);
  SIInstruction* siInstr = reinterpret_cast<SIInstruction*>(instr);

  switch (op) {
    case POPCNT_Z: {
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
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
      break;
    }
    case LLGFR: {
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      int32_t r2_val = get_low_register<int32_t>(r2);
      uint64_t r2_finalval =
          (static_cast<uint64_t>(r2_val) & 0x00000000ffffffff);
      set_register(r1, r2_finalval);
      break;
    }
    case EX: {
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int r1 = rxinst->R1Value();
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxinst->D2Value();
      int32_t r1_val = get_low_register<int32_t>(r1);

      SixByteInstr the_instr = Instruction::InstructionBits(
          reinterpret_cast<const byte*>(b2_val + x2_val + d2_val));
      int length = Instruction::InstructionLength(
          reinterpret_cast<const byte*>(b2_val + x2_val + d2_val));

      char new_instr_buf[8];
      char* addr = reinterpret_cast<char*>(&new_instr_buf[0]);
      the_instr |= static_cast<SixByteInstr>(r1_val & 0xff)
                   << (8 * length - 16);
      Instruction::SetInstructionBits<SixByteInstr>(
          reinterpret_cast<byte*>(addr), static_cast<SixByteInstr>(the_instr));
      ExecuteInstruction(reinterpret_cast<Instruction*>(addr), false);
      break;
    }
    case LGR: {
      // Load Register (64)
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      set_register(r1, get_register(r2));
      break;
    }
    case LDGR: {
      // Load FPR from GPR (L <- 64)
      uint64_t int_val = get_register(rreInst->R2Value());
      // double double_val = bit_cast<double, uint64_t>(int_val);
      // set_d_register_from_double(rreInst->R1Value(), double_val);
      set_d_register(rreInst->R1Value(), int_val);
      break;
    }
    case LGDR: {
      // Load GPR from FPR (64 <- L)
      int64_t double_val = get_d_register(rreInst->R2Value());
      set_register(rreInst->R1Value(), double_val);
      break;
    }
    case LTGR: {
      // Load Register (64)
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      int64_t r2_val = get_register(r2);
      SetS390ConditionCode<int64_t>(r2_val, 0);
      set_register(r1, get_register(r2));
      break;
    }
    case LZDR: {
      int r1 = rreInst->R1Value();
      set_d_register_from_double(r1, 0.0);
      break;
    }
    case LTEBR: {
      RREInstruction* rreinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreinst->R1Value();
      int r2 = rreinst->R2Value();
      int64_t r2_val = get_d_register(r2);
      float fr2_val = get_float32_from_d_register(r2);
      SetS390ConditionCode<float>(fr2_val, 0.0);
      set_d_register(r1, r2_val);
      break;
    }
    case LTDBR: {
      RREInstruction* rreinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreinst->R1Value();
      int r2 = rreinst->R2Value();
      int64_t r2_val = get_d_register(r2);
      SetS390ConditionCode<double>(bit_cast<double, int64_t>(r2_val), 0.0);
      set_d_register(r1, r2_val);
      break;
    }
    case CGR: {
      // Compare (64)
      int64_t r1_val = get_register(rreInst->R1Value());
      int64_t r2_val = get_register(rreInst->R2Value());
      SetS390ConditionCode<int64_t>(r1_val, r2_val);
      break;
    }
    case CLGR: {
      // Compare Logical (64)
      uint64_t r1_val = static_cast<uint64_t>(get_register(rreInst->R1Value()));
      uint64_t r2_val = static_cast<uint64_t>(get_register(rreInst->R2Value()));
      SetS390ConditionCode<uint64_t>(r1_val, r2_val);
      break;
    }
    case LH: {
      // Load Halfword
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int r1 = rxinst->R1Value();
      int x2 = rxinst->X2Value();
      int b2 = rxinst->B2Value();

      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      intptr_t d2_val = rxinst->D2Value();
      intptr_t mem_addr = x2_val + b2_val + d2_val;

      int32_t result = static_cast<int32_t>(ReadH(mem_addr, instr));
      set_low_register(r1, result);
      break;
    }
    case LHI: {
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int r1 = riinst->R1Value();
      int i = riinst->I2Value();
      set_low_register(r1, i);
      break;
    }
    case LGHI: {
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int r1 = riinst->R1Value();
      int64_t i = riinst->I2Value();
      set_register(r1, i);
      break;
    }
    case CHI: {
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int r1 = riinst->R1Value();
      int16_t i = riinst->I2Value();
      int32_t r1_val = get_low_register<int32_t>(r1);
      SetS390ConditionCode<int32_t>(r1_val, i);
      break;
    }
    case CGHI: {
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int r1 = riinst->R1Value();
      int64_t i = static_cast<int64_t>(riinst->I2Value());
      int64_t r1_val = get_register(r1);
      SetS390ConditionCode<int64_t>(r1_val, i);
      break;
    }
    case BRAS: {
      // Branch Relative and Save
      RILInstruction* rilInstr = reinterpret_cast<RILInstruction*>(instr);
      int r1 = rilInstr->R1Value();
      intptr_t d2 = rilInstr->I2Value();
      intptr_t pc = get_pc();
      // Set PC of next instruction to register
      set_register(r1, pc + sizeof(FourByteInstr));
      // Update PC to branch target
      set_pc(pc + d2 * 2);
      break;
    }
    case BRC: {
      // Branch Relative on Condition
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int m1 = riinst->M1Value();
      if (TestConditionCode((Condition)m1)) {
        intptr_t offset = riinst->I2Value() * 2;
        set_pc(get_pc() + offset);
      }
      break;
    }
    case BRCT:
    case BRCTG: {
      // Branch On Count (32/64).
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int r1 = riinst->R1Value();
      int64_t value =
          (op == BRCT) ? get_low_register<int32_t>(r1) : get_register(r1);
      if (BRCT == op)
        set_low_register(r1, --value);
      else
        set_register(r1, --value);
      // Branch if value != 0
      if (value != 0) {
        intptr_t offset = riinst->I2Value() * 2;
        set_pc(get_pc() + offset);
      }
      break;
    }
    case BXH: {
      RSInstruction* rsinst = reinterpret_cast<RSInstruction*>(instr);
      int r1 = rsinst->R1Value();
      int r3 = rsinst->R3Value();
      int b2 = rsinst->B2Value();
      int d2 = rsinst->D2Value();

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
      break;
    }
    case IIHH:
    case IIHL:
    case IILH:
    case IILL: {
      UNIMPLEMENTED();
      break;
    }
    case STM:
    case LM: {
      // Store Multiple 32-bits.
      RSInstruction* rsinstr = reinterpret_cast<RSInstruction*>(instr);
      int r1 = rsinstr->R1Value();
      int r3 = rsinstr->R3Value();
      int rb = rsinstr->B2Value();
      int offset = rsinstr->D2Value();

      // Regs roll around if r3 is less than r1.
      // Artifically increase r3 by 16 so we can calculate
      // the number of regs stored properly.
      if (r3 < r1) r3 += 16;

      int32_t rb_val = (rb == 0) ? 0 : get_low_register<int32_t>(rb);

      // Store each register in ascending order.
      for (int i = 0; i <= r3 - r1; i++) {
        if (op == STM) {
          int32_t value = get_low_register<int32_t>((r1 + i) % 16);
          WriteW(rb_val + offset + 4 * i, value, instr);
        } else if (op == LM) {
          int32_t value = ReadW(rb_val + offset + 4 * i, instr);
          set_low_register((r1 + i) % 16, value);
        }
      }
      break;
    }
    case SLL:
    case SRL: {
      RSInstruction* rsInstr = reinterpret_cast<RSInstruction*>(instr);
      int r1 = rsInstr->R1Value();
      int b2 = rsInstr->B2Value();
      intptr_t d2 = rsInstr->D2Value();
      // only takes rightmost 6bits
      int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
      int shiftBits = (b2_val + d2) & 0x3F;
      uint32_t r1_val = get_low_register<uint32_t>(r1);
      uint32_t alu_out = 0;
      if (SLL == op) {
        alu_out = r1_val << shiftBits;
      } else if (SRL == op) {
        alu_out = r1_val >> shiftBits;
      } else {
        UNREACHABLE();
      }
      set_low_register(r1, alu_out);
      break;
    }
    case SLDL: {
      RSInstruction* rsInstr = reinterpret_cast<RSInstruction*>(instr);
      int r1 = rsInstr->R1Value();
      int b2 = rsInstr->B2Value();
      intptr_t d2 = rsInstr->D2Value();
      // only takes rightmost 6bits
      int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
      int shiftBits = (b2_val + d2) & 0x3F;

      DCHECK(r1 % 2 == 0);
      uint32_t r1_val = get_low_register<uint32_t>(r1);
      uint32_t r1_next_val = get_low_register<uint32_t>(r1 + 1);
      uint64_t alu_out = (static_cast<uint64_t>(r1_val) << 32) |
                         (static_cast<uint64_t>(r1_next_val));
      alu_out <<= shiftBits;
      set_low_register(r1 + 1, static_cast<uint32_t>(alu_out));
      set_low_register(r1, static_cast<uint32_t>(alu_out >> 32));
      break;
    }
    case SLA:
    case SRA: {
      RSInstruction* rsInstr = reinterpret_cast<RSInstruction*>(instr);
      int r1 = rsInstr->R1Value();
      int b2 = rsInstr->B2Value();
      intptr_t d2 = rsInstr->D2Value();
      // only takes rightmost 6bits
      int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
      int shiftBits = (b2_val + d2) & 0x3F;
      int32_t r1_val = get_low_register<int32_t>(r1);
      int32_t alu_out = 0;
      bool isOF = false;
      if (op == SLA) {
        isOF = CheckOverflowForShiftLeft(r1_val, shiftBits);
        alu_out = r1_val << shiftBits;
      } else if (op == SRA) {
        alu_out = r1_val >> shiftBits;
      }
      set_low_register(r1, alu_out);
      SetS390ConditionCode<int32_t>(alu_out, 0);
      SetS390OverflowCode(isOF);
      break;
    }
    case LLHR: {
      UNIMPLEMENTED();
      break;
    }
    case LLGHR: {
      UNIMPLEMENTED();
      break;
    }
    case L:
    case LA:
    case LD:
    case LE: {
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      int32_t r1 = rxinst->R1Value();
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxinst->D2Value();
      intptr_t addr = b2_val + x2_val + d2_val;
      if (op == L) {
        int32_t mem_val = ReadW(addr, instr);
        set_low_register(r1, mem_val);
      } else if (op == LA) {
        set_register(r1, addr);
      } else if (op == LD) {
        int64_t dbl_val = *reinterpret_cast<int64_t*>(addr);
        set_d_register(r1, dbl_val);
      } else if (op == LE) {
        float float_val = *reinterpret_cast<float*>(addr);
        set_d_register_from_float32(r1, float_val);
      }
      break;
    }
    case C:
    case CL: {
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      int32_t r1_val = get_low_register<int32_t>(rxinst->R1Value());
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxinst->D2Value();
      intptr_t addr = b2_val + x2_val + d2_val;
      int32_t mem_val = ReadW(addr, instr);
      if (C == op)
        SetS390ConditionCode<int32_t>(r1_val, mem_val);
      else if (CL == op)
        SetS390ConditionCode<uint32_t>(r1_val, mem_val);
      break;
    }
    case CLI: {
      // Compare Immediate (Mem - Imm) (8)
      int b1 = siInstr->B1Value();
      int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
      intptr_t d1_val = siInstr->D1Value();
      intptr_t addr = b1_val + d1_val;
      uint8_t mem_val = ReadB(addr);
      uint8_t imm_val = siInstr->I2Value();
      SetS390ConditionCode<uint8_t>(mem_val, imm_val);
      break;
    }
    case TM: {
      // Test Under Mask (Mem - Imm) (8)
      int b1 = siInstr->B1Value();
      int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
      intptr_t d1_val = siInstr->D1Value();
      intptr_t addr = b1_val + d1_val;
      uint8_t mem_val = ReadB(addr);
      uint8_t imm_val = siInstr->I2Value();
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
      break;
    }
    case ST:
    case STE:
    case STD: {
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      int32_t r1_val = get_low_register<int32_t>(rxinst->R1Value());
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxinst->D2Value();
      intptr_t addr = b2_val + x2_val + d2_val;
      if (op == ST) {
        WriteW(addr, r1_val, instr);
      } else if (op == STD) {
        int64_t frs_val = get_d_register(rxinst->R1Value());
        WriteDW(addr, frs_val);
      } else if (op == STE) {
        int64_t frs_val = get_d_register(rxinst->R1Value()) >> 32;
        WriteW(addr, static_cast<int32_t>(frs_val), instr);
      }
      break;
    }
    case LTGFR:
    case LGFR: {
      // Load and Test Register (64 <- 32)  (Sign Extends 32-bit val)
      // Load Register (64 <- 32)  (Sign Extends 32-bit val)
      RREInstruction* rreInstr = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreInstr->R1Value();
      int r2 = rreInstr->R2Value();
      int32_t r2_val = get_low_register<int32_t>(r2);
      int64_t result = static_cast<int64_t>(r2_val);
      set_register(r1, result);

      if (LTGFR == op) SetS390ConditionCode<int64_t>(result, 0);
      break;
    }
    case LNGR: {
      // Load Negative (64)
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      int64_t r2_val = get_register(r2);
      r2_val = (r2_val >= 0) ? -r2_val : r2_val;  // If pos, then negate it.
      set_register(r1, r2_val);
      condition_reg_ = (r2_val == 0) ? CC_EQ : CC_LT;  // CC0 - result is zero
      // CC1 - result is negative
      break;
    }
    case TRAP4: {
      // whack the space of the caller allocated stack
      int64_t sp_addr = get_register(sp);
      for (int i = 0; i < kCalleeRegisterSaveAreaSize / kPointerSize; ++i) {
        // we dont want to whack the RA (r14)
        if (i != 14) (reinterpret_cast<intptr_t*>(sp_addr))[i] = 0xdeadbabe;
      }
      SoftwareInterrupt(instr);
      break;
    }
    case STC: {
      // Store Character/Byte
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      uint8_t r1_val = get_low_register<int32_t>(rxinst->R1Value());
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxinst->D2Value();
      intptr_t mem_addr = b2_val + x2_val + d2_val;
      WriteB(mem_addr, r1_val);
      break;
    }
    case STH: {
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      int16_t r1_val = get_low_register<int32_t>(rxinst->R1Value());
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxinst->D2Value();
      intptr_t mem_addr = b2_val + x2_val + d2_val;
      WriteH(mem_addr, r1_val, instr);
      break;
    }
#if V8_TARGET_ARCH_S390X
    case LCGR: {
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      int64_t r2_val = get_register(r2);
      r2_val = ~r2_val;
      r2_val = r2_val + 1;
      set_register(r1, r2_val);
      SetS390ConditionCode<int64_t>(r2_val, 0);
      // if the input is INT_MIN, loading its compliment would be overflowing
      if (r2_val < 0 && (r2_val + 1) > 0) {
        SetS390OverflowCode(true);
      }
      break;
    }
#endif
    case SRDA: {
      RSInstruction* rsInstr = reinterpret_cast<RSInstruction*>(instr);
      int r1 = rsInstr->R1Value();
      DCHECK(r1 % 2 == 0);  // must be a reg pair
      int b2 = rsInstr->B2Value();
      intptr_t d2 = rsInstr->D2Value();
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
      break;
    }
    case SRDL: {
      RSInstruction* rsInstr = reinterpret_cast<RSInstruction*>(instr);
      int r1 = rsInstr->R1Value();
      DCHECK(r1 % 2 == 0);  // must be a reg pair
      int b2 = rsInstr->B2Value();
      intptr_t d2 = rsInstr->D2Value();
      // only takes rightmost 6bits
      int64_t b2_val = b2 == 0 ? 0 : get_register(b2);
      int shiftBits = (b2_val + d2) & 0x3F;
      uint64_t opnd1 = static_cast<uint64_t>(get_low_register<uint32_t>(r1))
                       << 32;
      uint64_t opnd2 =
          static_cast<uint64_t>(get_low_register<uint32_t>(r1 + 1));
      uint64_t r1_val = opnd1 | opnd2;
      uint64_t alu_out = r1_val >> shiftBits;
      set_low_register(r1, alu_out >> 32);
      set_low_register(r1 + 1, alu_out & 0x00000000FFFFFFFF);
      SetS390ConditionCode<int32_t>(alu_out, 0);
      break;
    }
    default: { return DecodeFourByteArithmetic(instr); }
  }
  return true;
}

bool Simulator::DecodeFourByteArithmetic64Bit(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();

  RRFInstruction* rrfInst = reinterpret_cast<RRFInstruction*>(instr);
  RREInstruction* rreInst = reinterpret_cast<RREInstruction*>(instr);

  switch (op) {
    case AGR:
    case SGR:
    case OGR:
    case NGR:
    case XGR: {
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      int64_t r1_val = get_register(r1);
      int64_t r2_val = get_register(r2);
      bool isOF = false;
      switch (op) {
        case AGR:
          isOF = CheckOverflowForIntAdd(r1_val, r2_val, int64_t);
          r1_val += r2_val;
          SetS390ConditionCode<int64_t>(r1_val, 0);
          SetS390OverflowCode(isOF);
          break;
        case SGR:
          isOF = CheckOverflowForIntSub(r1_val, r2_val, int64_t);
          r1_val -= r2_val;
          SetS390ConditionCode<int64_t>(r1_val, 0);
          SetS390OverflowCode(isOF);
          break;
        case OGR:
          r1_val |= r2_val;
          SetS390BitWiseConditionCode<uint64_t>(r1_val);
          break;
        case NGR:
          r1_val &= r2_val;
          SetS390BitWiseConditionCode<uint64_t>(r1_val);
          break;
        case XGR:
          r1_val ^= r2_val;
          SetS390BitWiseConditionCode<uint64_t>(r1_val);
          break;
        default:
          UNREACHABLE();
          break;
      }
      set_register(r1, r1_val);
      break;
    }
    case AGFR: {
      // Add Register (64 <- 32)  (Sign Extends 32-bit val)
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      int64_t r1_val = get_register(r1);
      int64_t r2_val = static_cast<int64_t>(get_low_register<int32_t>(r2));
      bool isOF = CheckOverflowForIntAdd(r1_val, r2_val, int64_t);
      r1_val += r2_val;
      SetS390ConditionCode<int64_t>(r1_val, 0);
      SetS390OverflowCode(isOF);
      set_register(r1, r1_val);
      break;
    }
    case SGFR: {
      // Sub Reg (64 <- 32)
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      int64_t r1_val = get_register(r1);
      int64_t r2_val = static_cast<int64_t>(get_low_register<int32_t>(r2));
      bool isOF = false;
      isOF = CheckOverflowForIntSub(r1_val, r2_val, int64_t);
      r1_val -= r2_val;
      SetS390ConditionCode<int64_t>(r1_val, 0);
      SetS390OverflowCode(isOF);
      set_register(r1, r1_val);
      break;
    }
    case AGRK:
    case SGRK:
    case NGRK:
    case OGRK:
    case XGRK: {
      // 64-bit Non-clobbering arithmetics / bitwise ops.
      int r1 = rrfInst->R1Value();
      int r2 = rrfInst->R2Value();
      int r3 = rrfInst->R3Value();
      int64_t r2_val = get_register(r2);
      int64_t r3_val = get_register(r3);
      if (AGRK == op) {
        bool isOF = CheckOverflowForIntAdd(r2_val, r3_val, int64_t);
        SetS390ConditionCode<int64_t>(r2_val + r3_val, 0);
        SetS390OverflowCode(isOF);
        set_register(r1, r2_val + r3_val);
      } else if (SGRK == op) {
        bool isOF = CheckOverflowForIntSub(r2_val, r3_val, int64_t);
        SetS390ConditionCode<int64_t>(r2_val - r3_val, 0);
        SetS390OverflowCode(isOF);
        set_register(r1, r2_val - r3_val);
      } else {
        // Assume bitwise operation here
        uint64_t bitwise_result = 0;
        if (NGRK == op) {
          bitwise_result = r2_val & r3_val;
        } else if (OGRK == op) {
          bitwise_result = r2_val | r3_val;
        } else if (XGRK == op) {
          bitwise_result = r2_val ^ r3_val;
        }
        SetS390BitWiseConditionCode<uint64_t>(bitwise_result);
        set_register(r1, bitwise_result);
      }
      break;
    }
    case ALGRK:
    case SLGRK: {
      // 64-bit Non-clobbering unsigned arithmetics
      int r1 = rrfInst->R1Value();
      int r2 = rrfInst->R2Value();
      int r3 = rrfInst->R3Value();
      uint64_t r2_val = get_register(r2);
      uint64_t r3_val = get_register(r3);
      if (ALGRK == op) {
        bool isOF = CheckOverflowForUIntAdd(r2_val, r3_val);
        SetS390ConditionCode<uint64_t>(r2_val + r3_val, 0);
        SetS390OverflowCode(isOF);
        set_register(r1, r2_val + r3_val);
      } else if (SLGRK == op) {
        bool isOF = CheckOverflowForUIntSub(r2_val, r3_val);
        SetS390ConditionCode<uint64_t>(r2_val - r3_val, 0);
        SetS390OverflowCode(isOF);
        set_register(r1, r2_val - r3_val);
      }
    }
    case AGHI:
    case MGHI: {
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int32_t r1 = riinst->R1Value();
      int64_t i = static_cast<int64_t>(riinst->I2Value());
      int64_t r1_val = get_register(r1);
      bool isOF = false;
      switch (op) {
        case AGHI:
          isOF = CheckOverflowForIntAdd(r1_val, i, int64_t);
          r1_val += i;
          break;
        case MGHI:
          isOF = CheckOverflowForMul(r1_val, i);
          r1_val *= i;
          break;  // no overflow indication is given
        default:
          break;
      }
      set_register(r1, r1_val);
      SetS390ConditionCode<int32_t>(r1_val, 0);
      SetS390OverflowCode(isOF);
      break;
    }
    default:
      UNREACHABLE();
  }
  return true;
}

/**
 * Decodes and simulates four byte arithmetic instructions
 */
bool Simulator::DecodeFourByteArithmetic(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();

  // Pre-cast instruction to various types
  RRFInstruction* rrfInst = reinterpret_cast<RRFInstruction*>(instr);

  switch (op) {
    case AGR:
    case SGR:
    case OGR:
    case NGR:
    case XGR:
    case AGFR:
    case SGFR: {
      DecodeFourByteArithmetic64Bit(instr);
      break;
    }
    case ARK:
    case SRK:
    case NRK:
    case ORK:
    case XRK: {
      // 32-bit Non-clobbering arithmetics / bitwise ops
      int r1 = rrfInst->R1Value();
      int r2 = rrfInst->R2Value();
      int r3 = rrfInst->R3Value();
      int32_t r2_val = get_low_register<int32_t>(r2);
      int32_t r3_val = get_low_register<int32_t>(r3);
      if (ARK == op) {
        bool isOF = CheckOverflowForIntAdd(r2_val, r3_val, int32_t);
        SetS390ConditionCode<int32_t>(r2_val + r3_val, 0);
        SetS390OverflowCode(isOF);
        set_low_register(r1, r2_val + r3_val);
      } else if (SRK == op) {
        bool isOF = CheckOverflowForIntSub(r2_val, r3_val, int32_t);
        SetS390ConditionCode<int32_t>(r2_val - r3_val, 0);
        SetS390OverflowCode(isOF);
        set_low_register(r1, r2_val - r3_val);
      } else {
        // Assume bitwise operation here
        uint32_t bitwise_result = 0;
        if (NRK == op) {
          bitwise_result = r2_val & r3_val;
        } else if (ORK == op) {
          bitwise_result = r2_val | r3_val;
        } else if (XRK == op) {
          bitwise_result = r2_val ^ r3_val;
        }
        SetS390BitWiseConditionCode<uint32_t>(bitwise_result);
        set_low_register(r1, bitwise_result);
      }
      break;
    }
    case ALRK:
    case SLRK: {
      // 32-bit Non-clobbering unsigned arithmetics
      int r1 = rrfInst->R1Value();
      int r2 = rrfInst->R2Value();
      int r3 = rrfInst->R3Value();
      uint32_t r2_val = get_low_register<uint32_t>(r2);
      uint32_t r3_val = get_low_register<uint32_t>(r3);
      if (ALRK == op) {
        bool isOF = CheckOverflowForUIntAdd(r2_val, r3_val);
        SetS390ConditionCode<uint32_t>(r2_val + r3_val, 0);
        SetS390OverflowCode(isOF);
        set_low_register(r1, r2_val + r3_val);
      } else if (SLRK == op) {
        bool isOF = CheckOverflowForUIntSub(r2_val, r3_val);
        SetS390ConditionCode<uint32_t>(r2_val - r3_val, 0);
        SetS390OverflowCode(isOF);
        set_low_register(r1, r2_val - r3_val);
      }
      break;
    }
    case AGRK:
    case SGRK:
    case NGRK:
    case OGRK:
    case XGRK: {
      DecodeFourByteArithmetic64Bit(instr);
      break;
    }
    case ALGRK:
    case SLGRK: {
      DecodeFourByteArithmetic64Bit(instr);
      break;
    }
    case AHI:
    case MHI: {
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int32_t r1 = riinst->R1Value();
      int32_t i = riinst->I2Value();
      int32_t r1_val = get_low_register<int32_t>(r1);
      bool isOF = false;
      switch (op) {
        case AHI:
          isOF = CheckOverflowForIntAdd(r1_val, i, int32_t);
          r1_val += i;
          break;
        case MHI:
          isOF = CheckOverflowForMul(r1_val, i);
          r1_val *= i;
          break;  // no overflow indication is given
        default:
          break;
      }
      set_low_register(r1, r1_val);
      SetS390ConditionCode<int32_t>(r1_val, 0);
      SetS390OverflowCode(isOF);
      break;
    }
    case AGHI:
    case MGHI: {
      DecodeFourByteArithmetic64Bit(instr);
      break;
    }
    case MLR: {
      RREInstruction* rreinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreinst->R1Value();
      int r2 = rreinst->R2Value();
      DCHECK(r1 % 2 == 0);

      uint32_t r1_val = get_low_register<uint32_t>(r1 + 1);
      uint32_t r2_val = get_low_register<uint32_t>(r2);
      uint64_t product =
          static_cast<uint64_t>(r1_val) * static_cast<uint64_t>(r2_val);
      int32_t high_bits = product >> 32;
      int32_t low_bits = product & 0x00000000FFFFFFFF;
      set_low_register(r1, high_bits);
      set_low_register(r1 + 1, low_bits);
      break;
    }
    case DLGR: {
#ifdef V8_TARGET_ARCH_S390X
      RREInstruction* rreinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreinst->R1Value();
      int r2 = rreinst->R2Value();
      uint64_t r1_val = get_register(r1);
      uint64_t r2_val = get_register(r2);
      DCHECK(r1 % 2 == 0);
      unsigned __int128 dividend = static_cast<unsigned __int128>(r1_val) << 64;
      dividend += get_register(r1 + 1);
      uint64_t remainder = dividend % r2_val;
      uint64_t quotient = dividend / r2_val;
      r1_val = remainder;
      set_register(r1, remainder);
      set_register(r1 + 1, quotient);
#else
      UNREACHABLE();
#endif
      break;
    }
    case DLR: {
      RREInstruction* rreinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreinst->R1Value();
      int r2 = rreinst->R2Value();
      uint32_t r1_val = get_low_register<uint32_t>(r1);
      uint32_t r2_val = get_low_register<uint32_t>(r2);
      DCHECK(r1 % 2 == 0);
      uint64_t dividend = static_cast<uint64_t>(r1_val) << 32;
      dividend += get_low_register<uint32_t>(r1 + 1);
      uint32_t remainder = dividend % r2_val;
      uint32_t quotient = dividend / r2_val;
      r1_val = remainder;
      set_low_register(r1, remainder);
      set_low_register(r1 + 1, quotient);
      break;
    }
    case A:
    case S:
    case M:
    case D:
    case O:
    case N:
    case X: {
      // 32-bit Reg-Mem instructions
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      int32_t r1_val = get_low_register<int32_t>(rxinst->R1Value());
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxinst->D2Value();
      int32_t mem_val = ReadW(b2_val + x2_val + d2_val, instr);
      int32_t alu_out = 0;
      bool isOF = false;
      switch (op) {
        case A:
          isOF = CheckOverflowForIntAdd(r1_val, mem_val, int32_t);
          alu_out = r1_val + mem_val;
          SetS390ConditionCode<int32_t>(alu_out, 0);
          SetS390OverflowCode(isOF);
          break;
        case S:
          isOF = CheckOverflowForIntSub(r1_val, mem_val, int32_t);
          alu_out = r1_val - mem_val;
          SetS390ConditionCode<int32_t>(alu_out, 0);
          SetS390OverflowCode(isOF);
          break;
        case M:
        case D:
          UNIMPLEMENTED();
          break;
        case O:
          alu_out = r1_val | mem_val;
          SetS390BitWiseConditionCode<uint32_t>(alu_out);
          break;
        case N:
          alu_out = r1_val & mem_val;
          SetS390BitWiseConditionCode<uint32_t>(alu_out);
          break;
        case X:
          alu_out = r1_val ^ mem_val;
          SetS390BitWiseConditionCode<uint32_t>(alu_out);
          break;
        default:
          UNREACHABLE();
          break;
      }
      set_low_register(r1, alu_out);
      break;
    }
    case OILL:
    case OIHL: {
      RIInstruction* riInst = reinterpret_cast<RIInstruction*>(instr);
      int r1 = riInst->R1Value();
      int i = riInst->I2Value();
      int32_t r1_val = get_low_register<int32_t>(r1);
      if (OILL == op) {
        // CC is set based on the 16 bits that are AND'd
        SetS390BitWiseConditionCode<uint16_t>(r1_val | i);
      } else if (OILH == op) {
        // CC is set based on the 16 bits that are AND'd
        SetS390BitWiseConditionCode<uint16_t>((r1_val >> 16) | i);
        i = i << 16;
      } else {
        UNIMPLEMENTED();
      }
      set_low_register(r1, r1_val | i);
      break;
    }
    case NILL:
    case NILH: {
      RIInstruction* riInst = reinterpret_cast<RIInstruction*>(instr);
      int r1 = riInst->R1Value();
      int i = riInst->I2Value();
      int32_t r1_val = get_low_register<int32_t>(r1);
      if (NILL == op) {
        // CC is set based on the 16 bits that are AND'd
        SetS390BitWiseConditionCode<uint16_t>(r1_val & i);
        i |= 0xFFFF0000;
      } else if (NILH == op) {
        // CC is set based on the 16 bits that are AND'd
        SetS390BitWiseConditionCode<uint16_t>((r1_val >> 16) & i);
        i = (i << 16) | 0x0000FFFF;
      } else {
        UNIMPLEMENTED();
      }
      set_low_register(r1, r1_val & i);
      break;
    }
    case AH:
    case SH:
    case MH: {
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      int32_t r1_val = get_low_register<int32_t>(rxinst->R1Value());
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxinst->D2Value();
      intptr_t addr = b2_val + x2_val + d2_val;
      int32_t mem_val = static_cast<int32_t>(ReadH(addr, instr));
      int32_t alu_out = 0;
      bool isOF = false;
      if (AH == op) {
        isOF = CheckOverflowForIntAdd(r1_val, mem_val, int32_t);
        alu_out = r1_val + mem_val;
      } else if (SH == op) {
        isOF = CheckOverflowForIntSub(r1_val, mem_val, int32_t);
        alu_out = r1_val - mem_val;
      } else if (MH == op) {
        alu_out = r1_val * mem_val;
      } else {
        UNREACHABLE();
      }
      set_low_register(r1, alu_out);
      if (MH != op) {  // MH does not change condition code
        SetS390ConditionCode<int32_t>(alu_out, 0);
        SetS390OverflowCode(isOF);
      }
      break;
    }
    case DSGR: {
      RREInstruction* rreInst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();

      DCHECK(r1 % 2 == 0);

      int64_t dividend = get_register(r1 + 1);
      int64_t divisor = get_register(r2);
      set_register(r1, dividend % divisor);
      set_register(r1 + 1, dividend / divisor);

      break;
    }
    case FLOGR: {
      RREInstruction* rreInst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();

      DCHECK(r1 % 2 == 0);

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

      break;
    }
    case MSR:
    case MSGR: {  // they do not set overflow code
      RREInstruction* rreInst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      if (op == MSR) {
        int32_t r1_val = get_low_register<int32_t>(r1);
        int32_t r2_val = get_low_register<int32_t>(r2);
        set_low_register(r1, r1_val * r2_val);
      } else if (op == MSGR) {
        int64_t r1_val = get_register(r1);
        int64_t r2_val = get_register(r2);
        set_register(r1, r1_val * r2_val);
      } else {
        UNREACHABLE();
      }
      break;
    }
    case MS: {
      RXInstruction* rxinst = reinterpret_cast<RXInstruction*>(instr);
      int r1 = rxinst->R1Value();
      int b2 = rxinst->B2Value();
      int x2 = rxinst->X2Value();
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      intptr_t d2_val = rxinst->D2Value();
      int32_t mem_val = ReadW(b2_val + x2_val + d2_val, instr);
      int32_t r1_val = get_low_register<int32_t>(r1);
      set_low_register(r1, r1_val * mem_val);
      break;
    }
    case LGBR:
    case LBR: {
      RREInstruction* rrinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
#ifdef V8_TARGET_ARCH_S390X
      int64_t r2_val = get_low_register<int64_t>(r2);
      r2_val <<= 56;
      r2_val >>= 56;
      set_register(r1, r2_val);
#else
      int32_t r2_val = get_low_register<int32_t>(r2);
      r2_val <<= 24;
      r2_val >>= 24;
      set_low_register(r1, r2_val);
#endif
      break;
    }
    case LGHR:
    case LHR: {
      RREInstruction* rrinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
#ifdef V8_TARGET_ARCH_S390X
      int64_t r2_val = get_low_register<int64_t>(r2);
      r2_val <<= 48;
      r2_val >>= 48;
      set_register(r1, r2_val);
#else
      int32_t r2_val = get_low_register<int32_t>(r2);
      r2_val <<= 16;
      r2_val >>= 16;
      set_low_register(r1, r2_val);
#endif
      break;
    }
    case ALCR: {
      RREInstruction* rrinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
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
      break;
    }
    case SLBR: {
      RREInstruction* rrinst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rrinst->R1Value();
      int r2 = rrinst->R2Value();
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
      break;
    }
    default: { return DecodeFourByteFloatingPoint(instr); }
  }
  return true;
}

void Simulator::DecodeFourByteFloatingPointIntConversion(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();
  switch (op) {
    case CDLFBR:
    case CDLGBR:
    case CELGBR:
    case CLFDBR:
    case CLGDBR:
    case CELFBR:
    case CLGEBR:
    case CLFEBR: {
      RREInstruction* rreInstr = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreInstr->R1Value();
      int r2 = rreInstr->R2Value();
      if (op == CDLFBR) {
        uint32_t r2_val = get_low_register<uint32_t>(r2);
        double r1_val = static_cast<double>(r2_val);
        set_d_register_from_double(r1, r1_val);
      } else if (op == CELFBR) {
        uint32_t r2_val = get_low_register<uint32_t>(r2);
        float r1_val = static_cast<float>(r2_val);
        set_d_register_from_float32(r1, r1_val);
      } else if (op == CDLGBR) {
        uint64_t r2_val = get_register(r2);
        double r1_val = static_cast<double>(r2_val);
        set_d_register_from_double(r1, r1_val);
      } else if (op == CELGBR) {
        uint64_t r2_val = get_register(r2);
        float r1_val = static_cast<float>(r2_val);
        set_d_register_from_float32(r1, r1_val);
      } else if (op == CLFDBR) {
        double r2_val = get_double_from_d_register(r2);
        uint32_t r1_val = static_cast<uint32_t>(r2_val);
        set_low_register(r1, r1_val);
        SetS390ConvertConditionCode<double>(r2_val, r1_val, UINT32_MAX);
      } else if (op == CLFEBR) {
        float r2_val = get_float32_from_d_register(r2);
        uint32_t r1_val = static_cast<uint32_t>(r2_val);
        set_low_register(r1, r1_val);
        SetS390ConvertConditionCode<double>(r2_val, r1_val, UINT32_MAX);
      } else if (op == CLGDBR) {
        double r2_val = get_double_from_d_register(r2);
        uint64_t r1_val = static_cast<uint64_t>(r2_val);
        set_register(r1, r1_val);
        SetS390ConvertConditionCode<double>(r2_val, r1_val, UINT64_MAX);
      } else if (op == CLGEBR) {
        float r2_val = get_float32_from_d_register(r2);
        uint64_t r1_val = static_cast<uint64_t>(r2_val);
        set_register(r1, r1_val);
        SetS390ConvertConditionCode<double>(r2_val, r1_val, UINT64_MAX);
      }
      break;
    }
    default:
      UNREACHABLE();
  }
}

void Simulator::DecodeFourByteFloatingPointRound(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();
  RREInstruction* rreInstr = reinterpret_cast<RREInstruction*>(instr);
  int r1 = rreInstr->R1Value();
  int r2 = rreInstr->R2Value();
  double r2_val = get_double_from_d_register(r2);
  float r2_fval = get_float32_from_d_register(r2);

  switch (op) {
    case CFDBR: {
      int mask_val = rreInstr->M3Value();
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
      break;
    }
    case CGDBR: {
      int mask_val = rreInstr->M3Value();
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
          } else if (std::abs(r2_val - floor_val) <
                     std::abs(r2_val - ceil_val)) {
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
      break;
    }
    case CGEBR: {
      int mask_val = rreInstr->M3Value();
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
          } else if (std::abs(r2_fval - floor_val) <
                     std::abs(r2_fval - ceil_val)) {
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
      break;
    }
    case CFEBR: {
      int mask_val = rreInstr->M3Value();
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

      break;
    }
    default:
      UNREACHABLE();
  }
}

/**
 * Decodes and simulates four byte floating point instructions
 */
bool Simulator::DecodeFourByteFloatingPoint(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();

  switch (op) {
    case ADBR:
    case AEBR:
    case SDBR:
    case SEBR:
    case MDBR:
    case MEEBR:
    case MADBR:
    case DDBR:
    case DEBR:
    case CDBR:
    case CEBR:
    case CDFBR:
    case CDGBR:
    case CEGBR:
    case CGEBR:
    case CFDBR:
    case CGDBR:
    case SQDBR:
    case SQEBR:
    case CFEBR:
    case CEFBR:
    case LCDBR:
    case LPDBR:
    case LPEBR: {
      RREInstruction* rreInstr = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreInstr->R1Value();
      int r2 = rreInstr->R2Value();
      double r1_val = get_double_from_d_register(r1);
      double r2_val = get_double_from_d_register(r2);
      float fr1_val = get_float32_from_d_register(r1);
      float fr2_val = get_float32_from_d_register(r2);
      if (op == ADBR) {
        r1_val += r2_val;
        set_d_register_from_double(r1, r1_val);
        SetS390ConditionCode<double>(r1_val, 0);
      } else if (op == AEBR) {
        fr1_val += fr2_val;
        set_d_register_from_float32(r1, fr1_val);
        SetS390ConditionCode<float>(fr1_val, 0);
      } else if (op == SDBR) {
        r1_val -= r2_val;
        set_d_register_from_double(r1, r1_val);
        SetS390ConditionCode<double>(r1_val, 0);
      } else if (op == SEBR) {
        fr1_val -= fr2_val;
        set_d_register_from_float32(r1, fr1_val);
        SetS390ConditionCode<float>(fr1_val, 0);
      } else if (op == MDBR) {
        r1_val *= r2_val;
        set_d_register_from_double(r1, r1_val);
        SetS390ConditionCode<double>(r1_val, 0);
      } else if (op == MEEBR) {
        fr1_val *= fr2_val;
        set_d_register_from_float32(r1, fr1_val);
        SetS390ConditionCode<float>(fr1_val, 0);
      } else if (op == MADBR) {
        RRDInstruction* rrdInstr = reinterpret_cast<RRDInstruction*>(instr);
        int r1 = rrdInstr->R1Value();
        int r2 = rrdInstr->R2Value();
        int r3 = rrdInstr->R3Value();
        double r1_val = get_double_from_d_register(r1);
        double r2_val = get_double_from_d_register(r2);
        double r3_val = get_double_from_d_register(r3);
        r1_val += r2_val * r3_val;
        set_d_register_from_double(r1, r1_val);
        SetS390ConditionCode<double>(r1_val, 0);
      } else if (op == DDBR) {
        r1_val /= r2_val;
        set_d_register_from_double(r1, r1_val);
        SetS390ConditionCode<double>(r1_val, 0);
      } else if (op == DEBR) {
        fr1_val /= fr2_val;
        set_d_register_from_float32(r1, fr1_val);
        SetS390ConditionCode<float>(fr1_val, 0);
      } else if (op == CDBR) {
        if (isNaN(r1_val) || isNaN(r2_val)) {
          condition_reg_ = CC_OF;
        } else {
          SetS390ConditionCode<double>(r1_val, r2_val);
        }
      } else if (op == CEBR) {
        if (isNaN(fr1_val) || isNaN(fr2_val)) {
          condition_reg_ = CC_OF;
        } else {
          SetS390ConditionCode<float>(fr1_val, fr2_val);
        }
      } else if (op == CDGBR) {
        int64_t r2_val = get_register(r2);
        double r1_val = static_cast<double>(r2_val);
        set_d_register_from_double(r1, r1_val);
      } else if (op == CEGBR) {
        int64_t fr2_val = get_register(r2);
        float fr1_val = static_cast<float>(fr2_val);
        set_d_register_from_float32(r1, fr1_val);
      } else if (op == CDFBR) {
        int32_t r2_val = get_low_register<int32_t>(r2);
        double r1_val = static_cast<double>(r2_val);
        set_d_register_from_double(r1, r1_val);
      } else if (op == CEFBR) {
        int32_t fr2_val = get_low_register<int32_t>(r2);
        float fr1_val = static_cast<float>(fr2_val);
        set_d_register_from_float32(r1, fr1_val);
      } else if (op == CFDBR) {
        DecodeFourByteFloatingPointRound(instr);
      } else if (op == CGDBR) {
        DecodeFourByteFloatingPointRound(instr);
      } else if (op == CGEBR) {
        DecodeFourByteFloatingPointRound(instr);
      } else if (op == SQDBR) {
        r1_val = std::sqrt(r2_val);
        set_d_register_from_double(r1, r1_val);
      } else if (op == SQEBR) {
        fr1_val = std::sqrt(fr2_val);
        set_d_register_from_float32(r1, fr1_val);
      } else if (op == CFEBR) {
        DecodeFourByteFloatingPointRound(instr);
      } else if (op == LCDBR) {
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
      } else if (op == LPDBR) {
        r1_val = std::fabs(r2_val);
        set_d_register_from_double(r1, r1_val);
        if (r2_val != r2_val) {  // input is NaN
          condition_reg_ = CC_OF;
        } else if (r2_val == 0) {
          condition_reg_ = CC_EQ;
        } else {
          condition_reg_ = CC_GT;
        }
      } else if (op == LPEBR) {
        fr1_val = std::fabs(fr2_val);
        set_d_register_from_float32(r1, fr1_val);
        if (fr2_val != fr2_val) {  // input is NaN
          condition_reg_ = CC_OF;
        } else if (fr2_val == 0) {
          condition_reg_ = CC_EQ;
        } else {
          condition_reg_ = CC_GT;
        }
      } else {
        UNREACHABLE();
      }
      break;
    }
    case CDLFBR:
    case CDLGBR:
    case CELGBR:
    case CLFDBR:
    case CELFBR:
    case CLGDBR:
    case CLGEBR:
    case CLFEBR: {
      DecodeFourByteFloatingPointIntConversion(instr);
      break;
    }
    case TMLL: {
      RIInstruction* riinst = reinterpret_cast<RIInstruction*>(instr);
      int r1 = riinst->R1Value();
      int mask = riinst->I2Value() & 0x0000FFFF;
      if (mask == 0) {
        condition_reg_ = 0x0;
        break;
      }
      uint32_t r1_val = get_low_register<uint32_t>(r1);
      r1_val = r1_val & 0x0000FFFF;  // uses only the last 16bits

      // Test if all selected bits are Zero
      bool allSelectedBitsAreZeros = true;
      for (int i = 0; i < 15; i++) {
        if (mask & (1 << i)) {
          if (r1_val & (1 << i)) {
            allSelectedBitsAreZeros = false;
            break;
          }
        }
      }
      if (allSelectedBitsAreZeros) {
        condition_reg_ = 0x8;
        break;  // Done!
      }

      // Test if all selected bits are one
      bool allSelectedBitsAreOnes = true;
      for (int i = 0; i < 15; i++) {
        if (mask & (1 << i)) {
          if (!(r1_val & (1 << i))) {
            allSelectedBitsAreOnes = false;
            break;
          }
        }
      }
      if (allSelectedBitsAreOnes) {
        condition_reg_ = 0x1;
        break;  // Done!
      }

      // Now we know selected bits mixed zeros and ones
      // Test if the leftmost bit is zero or one
      for (int i = 14; i >= 0; i--) {
        if (mask & (1 << i)) {
          if (r1_val & (1 << i)) {
            // leftmost bit is one
            condition_reg_ = 0x2;
          } else {
            // leftmost bit is zero
            condition_reg_ = 0x4;
          }
          break;  // Done!
        }
      }
      break;
    }
    case LEDBR: {
      RREInstruction* rreInst = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreInst->R1Value();
      int r2 = rreInst->R2Value();
      double r2_val = get_double_from_d_register(r2);
      set_d_register_from_float32(r1, static_cast<float>(r2_val));
      break;
    }
    case FIDBRA: {
      RRFInstruction* rrfInst = reinterpret_cast<RRFInstruction*>(instr);
      int r1 = rrfInst->R1Value();
      int r2 = rrfInst->R2Value();
      int m3 = rrfInst->M3Value();
      double r2_val = get_double_from_d_register(r2);
      DCHECK(rrfInst->M4Value() == 0);
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
      break;
    }
    case FIEBRA: {
      RRFInstruction* rrfInst = reinterpret_cast<RRFInstruction*>(instr);
      int r1 = rrfInst->R1Value();
      int r2 = rrfInst->R2Value();
      int m3 = rrfInst->M3Value();
      float r2_val = get_float32_from_d_register(r2);
      DCHECK(rrfInst->M4Value() == 0);
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
      break;
    }
    case MSDBR: {
      UNIMPLEMENTED();
      break;
    }
    case LDEBR: {
      RREInstruction* rreInstr = reinterpret_cast<RREInstruction*>(instr);
      int r1 = rreInstr->R1Value();
      int r2 = rreInstr->R2Value();
      float fp_val = get_float32_from_d_register(r2);
      double db_val = static_cast<double>(fp_val);
      set_d_register_from_double(r1, db_val);
      break;
    }
    default: {
      UNREACHABLE();
      return false;
    }
  }
  return true;
}

// Decode routine for six-byte instructions
bool Simulator::DecodeSixByte(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();

  // Pre-cast instruction to various types
  RIEInstruction* rieInstr = reinterpret_cast<RIEInstruction*>(instr);
  RILInstruction* rilInstr = reinterpret_cast<RILInstruction*>(instr);
  RSYInstruction* rsyInstr = reinterpret_cast<RSYInstruction*>(instr);
  RXEInstruction* rxeInstr = reinterpret_cast<RXEInstruction*>(instr);
  RXYInstruction* rxyInstr = reinterpret_cast<RXYInstruction*>(instr);
  SIYInstruction* siyInstr = reinterpret_cast<SIYInstruction*>(instr);
  SILInstruction* silInstr = reinterpret_cast<SILInstruction*>(instr);
  SSInstruction* ssInstr = reinterpret_cast<SSInstruction*>(instr);

  switch (op) {
    case CLIY: {
      // Compare Immediate (Mem - Imm) (8)
      int b1 = siyInstr->B1Value();
      int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
      intptr_t d1_val = siyInstr->D1Value();
      intptr_t addr = b1_val + d1_val;
      uint8_t mem_val = ReadB(addr);
      uint8_t imm_val = siyInstr->I2Value();
      SetS390ConditionCode<uint8_t>(mem_val, imm_val);
      break;
    }
    case TMY: {
      // Test Under Mask (Mem - Imm) (8)
      int b1 = siyInstr->B1Value();
      int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
      intptr_t d1_val = siyInstr->D1Value();
      intptr_t addr = b1_val + d1_val;
      uint8_t mem_val = ReadB(addr);
      uint8_t imm_val = siyInstr->I2Value();
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
      break;
    }
    case LDEB: {
      // Load Float
      int r1 = rxeInstr->R1Value();
      int rb = rxeInstr->B2Value();
      int rx = rxeInstr->X2Value();
      int offset = rxeInstr->D2Value();
      int64_t rb_val = (rb == 0) ? 0 : get_register(rb);
      int64_t rx_val = (rx == 0) ? 0 : get_register(rx);
      double ret = static_cast<double>(
          *reinterpret_cast<float*>(rx_val + rb_val + offset));
      set_d_register_from_double(r1, ret);
      break;
    }
    case LAY: {
      // Load Address
      int r1 = rxyInstr->R1Value();
      int rb = rxyInstr->B2Value();
      int rx = rxyInstr->X2Value();
      int offset = rxyInstr->D2Value();
      int64_t rb_val = (rb == 0) ? 0 : get_register(rb);
      int64_t rx_val = (rx == 0) ? 0 : get_register(rx);
      set_register(r1, rx_val + rb_val + offset);
      break;
    }
    case LARL: {
      // Load Addresss Relative Long
      int r1 = rilInstr->R1Value();
      intptr_t offset = rilInstr->I2Value() * 2;
      set_register(r1, get_pc() + offset);
      break;
    }
    case LLILF: {
      // Load Logical into lower 32-bits (zero extend upper 32-bits)
      int r1 = rilInstr->R1Value();
      uint64_t imm = static_cast<uint64_t>(rilInstr->I2UnsignedValue());
      set_register(r1, imm);
      break;
    }
    case LLIHF: {
      // Load Logical Immediate into high word
      int r1 = rilInstr->R1Value();
      uint64_t imm = static_cast<uint64_t>(rilInstr->I2UnsignedValue());
      set_register(r1, imm << 32);
      break;
    }
    case OILF:
    case NILF:
    case IILF: {
      // Bitwise Op on lower 32-bits
      int r1 = rilInstr->R1Value();
      uint32_t imm = rilInstr->I2UnsignedValue();
      uint32_t alu_out = get_low_register<uint32_t>(r1);
      if (NILF == op) {
        alu_out &= imm;
        SetS390BitWiseConditionCode<uint32_t>(alu_out);
      } else if (OILF == op) {
        alu_out |= imm;
        SetS390BitWiseConditionCode<uint32_t>(alu_out);
      } else if (op == IILF) {
        alu_out = imm;
      } else {
        DCHECK(false);
      }
      set_low_register(r1, alu_out);
      break;
    }
    case OIHF:
    case NIHF:
    case IIHF: {
      // Bitwise Op on upper 32-bits
      int r1 = rilInstr->R1Value();
      uint32_t imm = rilInstr->I2Value();
      uint32_t alu_out = get_high_register<uint32_t>(r1);
      if (op == NIHF) {
        alu_out &= imm;
        SetS390BitWiseConditionCode<uint32_t>(alu_out);
      } else if (op == OIHF) {
        alu_out |= imm;
        SetS390BitWiseConditionCode<uint32_t>(alu_out);
      } else if (op == IIHF) {
        alu_out = imm;
      } else {
        DCHECK(false);
      }
      set_high_register(r1, alu_out);
      break;
    }
    case CLFI: {
      // Compare Logical with Immediate (32)
      int r1 = rilInstr->R1Value();
      uint32_t imm = rilInstr->I2UnsignedValue();
      SetS390ConditionCode<uint32_t>(get_low_register<uint32_t>(r1), imm);
      break;
    }
    case CFI: {
      // Compare with Immediate (32)
      int r1 = rilInstr->R1Value();
      int32_t imm = rilInstr->I2Value();
      SetS390ConditionCode<int32_t>(get_low_register<int32_t>(r1), imm);
      break;
    }
    case CLGFI: {
      // Compare Logical with Immediate (64)
      int r1 = rilInstr->R1Value();
      uint64_t imm = static_cast<uint64_t>(rilInstr->I2UnsignedValue());
      SetS390ConditionCode<uint64_t>(get_register(r1), imm);
      break;
    }
    case CGFI: {
      // Compare with Immediate (64)
      int r1 = rilInstr->R1Value();
      int64_t imm = static_cast<int64_t>(rilInstr->I2Value());
      SetS390ConditionCode<int64_t>(get_register(r1), imm);
      break;
    }
    case BRASL: {
      // Branch and Save Relative Long
      int r1 = rilInstr->R1Value();
      intptr_t d2 = rilInstr->I2Value();
      intptr_t pc = get_pc();
      set_register(r1, pc + 6);  // save next instruction to register
      set_pc(pc + d2 * 2);       // update register
      break;
    }
    case BRCL: {
      // Branch on Condition Relative Long
      Condition m1 = (Condition)rilInstr->R1Value();
      if (TestConditionCode((Condition)m1)) {
        intptr_t offset = rilInstr->I2Value() * 2;
        set_pc(get_pc() + offset);
      }
      break;
    }
    case LMG:
    case STMG: {
      // Store Multiple 64-bits.
      int r1 = rsyInstr->R1Value();
      int r3 = rsyInstr->R3Value();
      int rb = rsyInstr->B2Value();
      int offset = rsyInstr->D2Value();

      // Regs roll around if r3 is less than r1.
      // Artifically increase r3 by 16 so we can calculate
      // the number of regs stored properly.
      if (r3 < r1) r3 += 16;

      int64_t rb_val = (rb == 0) ? 0 : get_register(rb);

      // Store each register in ascending order.
      for (int i = 0; i <= r3 - r1; i++) {
        if (op == LMG) {
          int64_t value = ReadDW(rb_val + offset + 8 * i);
          set_register((r1 + i) % 16, value);
        } else if (op == STMG) {
          int64_t value = get_register((r1 + i) % 16);
          WriteDW(rb_val + offset + 8 * i, value);
        } else {
          DCHECK(false);
        }
      }
      break;
    }
    case SLLK:
    case RLL:
    case SRLK:
    case SLLG:
    case RLLG:
    case SRLG: {
      DecodeSixByteBitShift(instr);
      break;
    }
    case SLAK:
    case SRAK: {
      // 32-bit non-clobbering shift-left/right arithmetic
      int r1 = rsyInstr->R1Value();
      int r3 = rsyInstr->R3Value();
      int b2 = rsyInstr->B2Value();
      intptr_t d2 = rsyInstr->D2Value();
      // only takes rightmost 6 bits
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int shiftBits = (b2_val + d2) & 0x3F;
      int32_t r3_val = get_low_register<int32_t>(r3);
      int32_t alu_out = 0;
      bool isOF = false;
      if (op == SLAK) {
        isOF = CheckOverflowForShiftLeft(r3_val, shiftBits);
        alu_out = r3_val << shiftBits;
      } else if (op == SRAK) {
        alu_out = r3_val >> shiftBits;
      }
      set_low_register(r1, alu_out);
      SetS390ConditionCode<int32_t>(alu_out, 0);
      SetS390OverflowCode(isOF);
      break;
    }
    case SLAG:
    case SRAG: {
      // 64-bit non-clobbering shift-left/right arithmetic
      int r1 = rsyInstr->R1Value();
      int r3 = rsyInstr->R3Value();
      int b2 = rsyInstr->B2Value();
      intptr_t d2 = rsyInstr->D2Value();
      // only takes rightmost 6 bits
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int shiftBits = (b2_val + d2) & 0x3F;
      int64_t r3_val = get_register(r3);
      intptr_t alu_out = 0;
      bool isOF = false;
      if (op == SLAG) {
        isOF = CheckOverflowForShiftLeft(r3_val, shiftBits);
        alu_out = r3_val << shiftBits;
      } else if (op == SRAG) {
        alu_out = r3_val >> shiftBits;
      }
      set_register(r1, alu_out);
      SetS390ConditionCode<intptr_t>(alu_out, 0);
      SetS390OverflowCode(isOF);
      break;
    }
    case LMY:
    case STMY: {
      RSYInstruction* rsyInstr = reinterpret_cast<RSYInstruction*>(instr);
      // Load/Store Multiple (32)
      int r1 = rsyInstr->R1Value();
      int r3 = rsyInstr->R3Value();
      int b2 = rsyInstr->B2Value();
      int offset = rsyInstr->D2Value();

      // Regs roll around if r3 is less than r1.
      // Artifically increase r3 by 16 so we can calculate
      // the number of regs stored properly.
      if (r3 < r1) r3 += 16;

      int32_t b2_val = (b2 == 0) ? 0 : get_low_register<int32_t>(b2);

      // Store each register in ascending order.
      for (int i = 0; i <= r3 - r1; i++) {
        if (op == LMY) {
          int32_t value = ReadW(b2_val + offset + 4 * i, instr);
          set_low_register((r1 + i) % 16, value);
        } else {
          int32_t value = get_low_register<int32_t>((r1 + i) % 16);
          WriteW(b2_val + offset + 4 * i, value, instr);
        }
      }
      break;
    }
    case LT:
    case LTG: {
      // Load and Test (32/64)
      int r1 = rxyInstr->R1Value();
      int x2 = rxyInstr->X2Value();
      int b2 = rxyInstr->B2Value();
      int d2 = rxyInstr->D2Value();

      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      intptr_t addr = x2_val + b2_val + d2;

      if (op == LT) {
        int32_t value = ReadW(addr, instr);
        set_low_register(r1, value);
        SetS390ConditionCode<int32_t>(value, 0);
      } else if (op == LTG) {
        int64_t value = ReadDW(addr);
        set_register(r1, value);
        SetS390ConditionCode<int64_t>(value, 0);
      }
      break;
    }
    case LY:
    case LB:
    case LGB:
    case LG:
    case LGF:
    case LGH:
    case LLGF:
    case STG:
    case STY:
    case STCY:
    case STHY:
    case STEY:
    case LDY:
    case LHY:
    case STDY:
    case LEY: {
      // Miscellaneous Loads and Stores
      int r1 = rxyInstr->R1Value();
      int x2 = rxyInstr->X2Value();
      int b2 = rxyInstr->B2Value();
      int d2 = rxyInstr->D2Value();
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      intptr_t addr = x2_val + b2_val + d2;
      if (op == LY) {
        uint32_t mem_val = ReadWU(addr, instr);
        set_low_register(r1, mem_val);
      } else if (op == LB) {
        int32_t mem_val = ReadB(addr);
        set_low_register(r1, mem_val);
      } else if (op == LGB) {
        int64_t mem_val = ReadB(addr);
        set_register(r1, mem_val);
      } else if (op == LG) {
        int64_t mem_val = ReadDW(addr);
        set_register(r1, mem_val);
      } else if (op == LGF) {
        int64_t mem_val = static_cast<int64_t>(ReadW(addr, instr));
        set_register(r1, mem_val);
      } else if (op == LGH) {
        int64_t mem_val = static_cast<int64_t>(ReadH(addr, instr));
        set_register(r1, mem_val);
      } else if (op == LLGF) {
        //      int r1 = rreInst->R1Value();
        //      int r2 = rreInst->R2Value();
        //      int32_t r2_val = get_low_register<int32_t>(r2);
        //      uint64_t r2_finalval = (static_cast<uint64_t>(r2_val)
        //        & 0x00000000ffffffff);
        //      set_register(r1, r2_finalval);
        //      break;
        uint64_t mem_val = static_cast<uint64_t>(ReadWU(addr, instr));
        set_register(r1, mem_val);
      } else if (op == LDY) {
        uint64_t dbl_val = *reinterpret_cast<uint64_t*>(addr);
        set_d_register(r1, dbl_val);
      } else if (op == STEY) {
        int64_t frs_val = get_d_register(r1) >> 32;
        WriteW(addr, static_cast<int32_t>(frs_val), instr);
      } else if (op == LEY) {
        float float_val = *reinterpret_cast<float*>(addr);
        set_d_register_from_float32(r1, float_val);
      } else if (op == STY) {
        uint32_t value = get_low_register<uint32_t>(r1);
        WriteW(addr, value, instr);
      } else if (op == STG) {
        uint64_t value = get_register(r1);
        WriteDW(addr, value);
      } else if (op == STDY) {
        int64_t frs_val = get_d_register(r1);
        WriteDW(addr, frs_val);
      } else if (op == STCY) {
        uint8_t value = get_low_register<uint32_t>(r1);
        WriteB(addr, value);
      } else if (op == STHY) {
        uint16_t value = get_low_register<uint32_t>(r1);
        WriteH(addr, value, instr);
      } else if (op == LHY) {
        int32_t result = static_cast<int32_t>(ReadH(addr, instr));
        set_low_register(r1, result);
      }
      break;
    }
    case MVC: {
      // Move Character
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
      break;
    }
    case MVHI: {
      // Move Integer (32)
      int b1 = silInstr->B1Value();
      intptr_t d1 = silInstr->D1Value();
      int16_t i2 = silInstr->I2Value();
      int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
      intptr_t src_addr = b1_val + d1;
      WriteW(src_addr, i2, instr);
      break;
    }
    case MVGHI: {
      // Move Integer (64)
      int b1 = silInstr->B1Value();
      intptr_t d1 = silInstr->D1Value();
      int16_t i2 = silInstr->I2Value();
      int64_t b1_val = (b1 == 0) ? 0 : get_register(b1);
      intptr_t src_addr = b1_val + d1;
      WriteDW(src_addr, i2);
      break;
    }
    case LLH:
    case LLGH: {
      // Load Logical Halfworld
      int r1 = rxyInstr->R1Value();
      int b2 = rxyInstr->B2Value();
      int x2 = rxyInstr->X2Value();
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxyInstr->D2Value();
      uint16_t mem_val = ReadHU(b2_val + d2_val + x2_val, instr);
      if (op == LLH) {
        set_low_register(r1, mem_val);
      } else if (op == LLGH) {
        set_register(r1, mem_val);
      } else {
        UNREACHABLE();
      }
      break;
    }
    case LLC:
    case LLGC: {
      // Load Logical Character - loads a byte and zero extends.
      int r1 = rxyInstr->R1Value();
      int b2 = rxyInstr->B2Value();
      int x2 = rxyInstr->X2Value();
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxyInstr->D2Value();
      uint8_t mem_val = ReadBU(b2_val + d2_val + x2_val);
      if (op == LLC) {
        set_low_register(r1, static_cast<uint32_t>(mem_val));
      } else if (op == LLGC) {
        set_register(r1, static_cast<uint64_t>(mem_val));
      } else {
        UNREACHABLE();
      }
      break;
    }
    case XIHF:
    case XILF: {
      int r1 = rilInstr->R1Value();
      uint32_t imm = rilInstr->I2UnsignedValue();
      uint32_t alu_out = 0;
      if (op == XILF) {
        alu_out = get_low_register<uint32_t>(r1);
        alu_out = alu_out ^ imm;
        set_low_register(r1, alu_out);
      } else if (op == XIHF) {
        alu_out = get_high_register<uint32_t>(r1);
        alu_out = alu_out ^ imm;
        set_high_register(r1, alu_out);
      } else {
        UNREACHABLE();
      }
      SetS390BitWiseConditionCode<uint32_t>(alu_out);
      break;
    }
    case RISBG: {
      // Rotate then insert selected bits
      int r1 = rieInstr->R1Value();
      int r2 = rieInstr->R2Value();
      // Starting Bit Position is Bits 2-7 of I3 field
      uint32_t start_bit = rieInstr->I3Value() & 0x3F;
      // Ending Bit Position is Bits 2-7 of I4 field
      uint32_t end_bit = rieInstr->I4Value() & 0x3F;
      // Shift Amount is Bits 2-7 of I5 field
      uint32_t shift_amount = rieInstr->I5Value() & 0x3F;
      // Zero out Remaining (unslected) bits if Bit 0 of I4 is 1.
      bool zero_remaining = (0 != (rieInstr->I4Value() & 0x80));

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
        selected_val = (src_val & ~selection_mask) | selected_val;
      }

      // Condition code is set by treating result as 64-bit signed int
      SetS390ConditionCode<int64_t>(selected_val, 0);
      set_register(r1, selected_val);
      break;
    }
    default:
      return DecodeSixByteArithmetic(instr);
  }
  return true;
}

void Simulator::DecodeSixByteBitShift(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();

  // Pre-cast instruction to various types

  RSYInstruction* rsyInstr = reinterpret_cast<RSYInstruction*>(instr);

  switch (op) {
    case SLLK:
    case RLL:
    case SRLK: {
      // For SLLK/SRLL, the 32-bit third operand is shifted the number
      // of bits specified by the second-operand address, and the result is
      // placed at the first-operand location. Except for when the R1 and R3
      // fields designate the same register, the third operand remains
      // unchanged in general register R3.
      int r1 = rsyInstr->R1Value();
      int r3 = rsyInstr->R3Value();
      int b2 = rsyInstr->B2Value();
      intptr_t d2 = rsyInstr->D2Value();
      // only takes rightmost 6 bits
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int shiftBits = (b2_val + d2) & 0x3F;
      // unsigned
      uint32_t r3_val = get_low_register<uint32_t>(r3);
      uint32_t alu_out = 0;
      if (SLLK == op) {
        alu_out = r3_val << shiftBits;
      } else if (SRLK == op) {
        alu_out = r3_val >> shiftBits;
      } else if (RLL == op) {
        uint32_t rotateBits = r3_val >> (32 - shiftBits);
        alu_out = (r3_val << shiftBits) | (rotateBits);
      } else {
        UNREACHABLE();
      }
      set_low_register(r1, alu_out);
      break;
    }
    case SLLG:
    case RLLG:
    case SRLG: {
      // For SLLG/SRLG, the 64-bit third operand is shifted the number
      // of bits specified by the second-operand address, and the result is
      // placed at the first-operand location. Except for when the R1 and R3
      // fields designate the same register, the third operand remains
      // unchanged in general register R3.
      int r1 = rsyInstr->R1Value();
      int r3 = rsyInstr->R3Value();
      int b2 = rsyInstr->B2Value();
      intptr_t d2 = rsyInstr->D2Value();
      // only takes rightmost 6 bits
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int shiftBits = (b2_val + d2) & 0x3F;
      // unsigned
      uint64_t r3_val = get_register(r3);
      uint64_t alu_out = 0;
      if (op == SLLG) {
        alu_out = r3_val << shiftBits;
      } else if (op == SRLG) {
        alu_out = r3_val >> shiftBits;
      } else if (op == RLLG) {
        uint64_t rotateBits = r3_val >> (64 - shiftBits);
        alu_out = (r3_val << shiftBits) | (rotateBits);
      } else {
        UNREACHABLE();
      }
      set_register(r1, alu_out);
      break;
    }
    default:
      UNREACHABLE();
  }
}

/**
 * Decodes and simulates six byte arithmetic instructions
 */
bool Simulator::DecodeSixByteArithmetic(Instruction* instr) {
  Opcode op = instr->S390OpcodeValue();

  // Pre-cast instruction to various types
  SIYInstruction* siyInstr = reinterpret_cast<SIYInstruction*>(instr);

  switch (op) {
    case CDB:
    case ADB:
    case SDB:
    case MDB:
    case DDB:
    case SQDB: {
      RXEInstruction* rxeInstr = reinterpret_cast<RXEInstruction*>(instr);
      int b2 = rxeInstr->B2Value();
      int x2 = rxeInstr->X2Value();
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxeInstr->D2Value();
      double r1_val = get_double_from_d_register(rxeInstr->R1Value());
      double dbl_val = ReadDouble(b2_val + x2_val + d2_val);

      switch (op) {
        case CDB:
          SetS390ConditionCode<double>(r1_val, dbl_val);
          break;
        case ADB:
          r1_val += dbl_val;
          set_d_register_from_double(r1, r1_val);
          SetS390ConditionCode<double>(r1_val, 0);
          break;
        case SDB:
          r1_val -= dbl_val;
          set_d_register_from_double(r1, r1_val);
          SetS390ConditionCode<double>(r1_val, 0);
          break;
        case MDB:
          r1_val *= dbl_val;
          set_d_register_from_double(r1, r1_val);
          SetS390ConditionCode<double>(r1_val, 0);
          break;
        case DDB:
          r1_val /= dbl_val;
          set_d_register_from_double(r1, r1_val);
          SetS390ConditionCode<double>(r1_val, 0);
          break;
        case SQDB:
          r1_val = std::sqrt(dbl_val);
          set_d_register_from_double(r1, r1_val);
        default:
          UNREACHABLE();
          break;
      }
      break;
    }
    case LRV:
    case LRVH:
    case STRV:
    case STRVH: {
      RXYInstruction* rxyInstr = reinterpret_cast<RXYInstruction*>(instr);
      int r1 = rxyInstr->R1Value();
      int x2 = rxyInstr->X2Value();
      int b2 = rxyInstr->B2Value();
      int d2 = rxyInstr->D2Value();
      int32_t r1_val = get_low_register<int32_t>(r1);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      intptr_t mem_addr = b2_val + x2_val + d2;

      if (op == LRVH) {
        int16_t mem_val = ReadH(mem_addr, instr);
        int32_t result = ByteReverse(mem_val) & 0x0000ffff;
        result |= r1_val & 0xffff0000;
        set_low_register(r1, result);
      } else if (op == LRV) {
        int32_t mem_val = ReadW(mem_addr, instr);
        set_low_register(r1, ByteReverse(mem_val));
      } else if (op == STRVH) {
        int16_t result = static_cast<int16_t>(r1_val >> 16);
        WriteH(mem_addr, ByteReverse(result), instr);
      } else if (op == STRV) {
        WriteW(mem_addr, ByteReverse(r1_val), instr);
      }

      break;
    }
    case AHIK:
    case AGHIK: {
      // Non-clobbering Add Halfword Immediate
      RIEInstruction* rieInst = reinterpret_cast<RIEInstruction*>(instr);
      int r1 = rieInst->R1Value();
      int r2 = rieInst->R2Value();
      bool isOF = false;
      if (AHIK == op) {
        // 32-bit Add
        int32_t r2_val = get_low_register<int32_t>(r2);
        int32_t imm = rieInst->I6Value();
        isOF = CheckOverflowForIntAdd(r2_val, imm, int32_t);
        set_low_register(r1, r2_val + imm);
        SetS390ConditionCode<int32_t>(r2_val + imm, 0);
      } else if (AGHIK == op) {
        // 64-bit Add
        int64_t r2_val = get_register(r2);
        int64_t imm = static_cast<int64_t>(rieInst->I6Value());
        isOF = CheckOverflowForIntAdd(r2_val, imm, int64_t);
        set_register(r1, r2_val + imm);
        SetS390ConditionCode<int64_t>(r2_val + imm, 0);
      }
      SetS390OverflowCode(isOF);
      break;
    }
    case ALFI:
    case SLFI: {
      RILInstruction* rilInstr = reinterpret_cast<RILInstruction*>(instr);
      int r1 = rilInstr->R1Value();
      uint32_t imm = rilInstr->I2UnsignedValue();
      uint32_t alu_out = get_low_register<uint32_t>(r1);
      if (op == ALFI) {
        alu_out += imm;
      } else if (op == SLFI) {
        alu_out -= imm;
      }
      SetS390ConditionCode<uint32_t>(alu_out, 0);
      set_low_register(r1, alu_out);
      break;
    }
    case ML: {
      UNIMPLEMENTED();
      break;
    }
    case AY:
    case SY:
    case NY:
    case OY:
    case XY:
    case CY: {
      RXYInstruction* rxyInstr = reinterpret_cast<RXYInstruction*>(instr);
      int r1 = rxyInstr->R1Value();
      int x2 = rxyInstr->X2Value();
      int b2 = rxyInstr->B2Value();
      int d2 = rxyInstr->D2Value();
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int32_t alu_out = get_low_register<int32_t>(r1);
      int32_t mem_val = ReadW(b2_val + x2_val + d2, instr);
      bool isOF = false;
      if (op == AY) {
        isOF = CheckOverflowForIntAdd(alu_out, mem_val, int32_t);
        alu_out += mem_val;
        SetS390ConditionCode<int32_t>(alu_out, 0);
        SetS390OverflowCode(isOF);
      } else if (op == SY) {
        isOF = CheckOverflowForIntSub(alu_out, mem_val, int32_t);
        alu_out -= mem_val;
        SetS390ConditionCode<int32_t>(alu_out, 0);
        SetS390OverflowCode(isOF);
      } else if (op == NY) {
        alu_out &= mem_val;
        SetS390BitWiseConditionCode<uint32_t>(alu_out);
      } else if (op == OY) {
        alu_out |= mem_val;
        SetS390BitWiseConditionCode<uint32_t>(alu_out);
      } else if (op == XY) {
        alu_out ^= mem_val;
        SetS390BitWiseConditionCode<uint32_t>(alu_out);
      } else if (op == CY) {
        SetS390ConditionCode<int32_t>(alu_out, mem_val);
      }
      if (op != CY) {
        set_low_register(r1, alu_out);
      }
      break;
    }
    case AHY:
    case SHY: {
      RXYInstruction* rxyInstr = reinterpret_cast<RXYInstruction*>(instr);
      int32_t r1_val = get_low_register<int32_t>(rxyInstr->R1Value());
      int b2 = rxyInstr->B2Value();
      int x2 = rxyInstr->X2Value();
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxyInstr->D2Value();
      int32_t mem_val =
          static_cast<int32_t>(ReadH(b2_val + d2_val + x2_val, instr));
      int32_t alu_out = 0;
      bool isOF = false;
      switch (op) {
        case AHY:
          alu_out = r1_val + mem_val;
          isOF = CheckOverflowForIntAdd(r1_val, mem_val, int32_t);
          break;
        case SHY:
          alu_out = r1_val - mem_val;
          isOF = CheckOverflowForIntSub(r1_val, mem_val, int64_t);
          break;
        default:
          UNREACHABLE();
          break;
      }
      set_low_register(r1, alu_out);
      SetS390ConditionCode<int32_t>(alu_out, 0);
      SetS390OverflowCode(isOF);
      break;
    }
    case AG:
    case SG:
    case NG:
    case OG:
    case XG:
    case CG:
    case CLG: {
      RXYInstruction* rxyInstr = reinterpret_cast<RXYInstruction*>(instr);
      int r1 = rxyInstr->R1Value();
      int x2 = rxyInstr->X2Value();
      int b2 = rxyInstr->B2Value();
      int d2 = rxyInstr->D2Value();
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t alu_out = get_register(r1);
      int64_t mem_val = ReadDW(b2_val + x2_val + d2);

      switch (op) {
        case AG: {
          alu_out += mem_val;
          SetS390ConditionCode<int32_t>(alu_out, 0);
          break;
        }
        case SG: {
          alu_out -= mem_val;
          SetS390ConditionCode<int32_t>(alu_out, 0);
          break;
        }
        case NG: {
          alu_out &= mem_val;
          SetS390BitWiseConditionCode<uint32_t>(alu_out);
          break;
        }
        case OG: {
          alu_out |= mem_val;
          SetS390BitWiseConditionCode<uint32_t>(alu_out);
          break;
        }
        case XG: {
          alu_out ^= mem_val;
          SetS390BitWiseConditionCode<uint32_t>(alu_out);
          break;
        }
        case CG: {
          SetS390ConditionCode<int64_t>(alu_out, mem_val);
          break;
        }
        case CLG: {
          SetS390ConditionCode<uint64_t>(alu_out, mem_val);
          break;
        }
        default: {
          DCHECK(false);
          break;
        }
      }

      if (op != CG) {
        set_register(r1, alu_out);
      }
      break;
    }
    case ALY:
    case SLY:
    case CLY: {
      RXYInstruction* rxyInstr = reinterpret_cast<RXYInstruction*>(instr);
      int r1 = rxyInstr->R1Value();
      int x2 = rxyInstr->X2Value();
      int b2 = rxyInstr->B2Value();
      int d2 = rxyInstr->D2Value();
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      uint32_t alu_out = get_low_register<uint32_t>(r1);
      uint32_t mem_val = ReadWU(b2_val + x2_val + d2, instr);

      if (op == ALY) {
        alu_out += mem_val;
        set_low_register(r1, alu_out);
        SetS390ConditionCode<uint32_t>(alu_out, 0);
      } else if (op == SLY) {
        alu_out -= mem_val;
        set_low_register(r1, alu_out);
        SetS390ConditionCode<uint32_t>(alu_out, 0);
      } else if (op == CLY) {
        SetS390ConditionCode<uint32_t>(alu_out, mem_val);
      }
      break;
    }
    case AGFI:
    case AFI: {
      // Clobbering Add Word Immediate
      RILInstruction* rilInstr = reinterpret_cast<RILInstruction*>(instr);
      int32_t r1 = rilInstr->R1Value();
      bool isOF = false;
      if (AFI == op) {
        // 32-bit Add (Register + 32-bit Immediate)
        int32_t r1_val = get_low_register<int32_t>(r1);
        int32_t i2 = rilInstr->I2Value();
        isOF = CheckOverflowForIntAdd(r1_val, i2, int32_t);
        int32_t alu_out = r1_val + i2;
        set_low_register(r1, alu_out);
        SetS390ConditionCode<int32_t>(alu_out, 0);
      } else if (AGFI == op) {
        // 64-bit Add (Register + 32-bit Imm)
        int64_t r1_val = get_register(r1);
        int64_t i2 = static_cast<int64_t>(rilInstr->I2Value());
        isOF = CheckOverflowForIntAdd(r1_val, i2, int64_t);
        int64_t alu_out = r1_val + i2;
        set_register(r1, alu_out);
        SetS390ConditionCode<int64_t>(alu_out, 0);
      }
      SetS390OverflowCode(isOF);
      break;
    }
    case ASI: {
      // TODO(bcleung): Change all fooInstr->I2Value() to template functions.
      // The below static cast to 8 bit and then to 32 bit is necessary
      // because siyInstr->I2Value() returns a uint8_t, which a direct
      // cast to int32_t could incorrectly interpret.
      int8_t i2_8bit = static_cast<int8_t>(siyInstr->I2Value());
      int32_t i2 = static_cast<int32_t>(i2_8bit);
      int b1 = siyInstr->B1Value();
      intptr_t b1_val = (b1 == 0) ? 0 : get_register(b1);

      int d1_val = siyInstr->D1Value();
      intptr_t addr = b1_val + d1_val;

      int32_t mem_val = ReadW(addr, instr);
      bool isOF = CheckOverflowForIntAdd(mem_val, i2, int32_t);
      int32_t alu_out = mem_val + i2;
      SetS390ConditionCode<int32_t>(alu_out, 0);
      SetS390OverflowCode(isOF);
      WriteW(addr, alu_out, instr);
      break;
    }
    case AGSI: {
      // TODO(bcleung): Change all fooInstr->I2Value() to template functions.
      // The below static cast to 8 bit and then to 32 bit is necessary
      // because siyInstr->I2Value() returns a uint8_t, which a direct
      // cast to int32_t could incorrectly interpret.
      int8_t i2_8bit = static_cast<int8_t>(siyInstr->I2Value());
      int64_t i2 = static_cast<int64_t>(i2_8bit);
      int b1 = siyInstr->B1Value();
      intptr_t b1_val = (b1 == 0) ? 0 : get_register(b1);

      int d1_val = siyInstr->D1Value();
      intptr_t addr = b1_val + d1_val;

      int64_t mem_val = ReadDW(addr);
      int isOF = CheckOverflowForIntAdd(mem_val, i2, int64_t);
      int64_t alu_out = mem_val + i2;
      SetS390ConditionCode<uint64_t>(alu_out, 0);
      SetS390OverflowCode(isOF);
      WriteDW(addr, alu_out);
      break;
    }
    case AGF:
    case SGF:
    case ALG:
    case SLG: {
#ifndef V8_TARGET_ARCH_S390X
      DCHECK(false);
#endif
      RXYInstruction* rxyInstr = reinterpret_cast<RXYInstruction*>(instr);
      int r1 = rxyInstr->R1Value();
      uint64_t r1_val = get_register(rxyInstr->R1Value());
      int b2 = rxyInstr->B2Value();
      int x2 = rxyInstr->X2Value();
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxyInstr->D2Value();
      uint64_t alu_out = r1_val;
      if (op == ALG) {
        uint64_t mem_val =
            static_cast<uint64_t>(ReadDW(b2_val + d2_val + x2_val));
        alu_out += mem_val;
        SetS390ConditionCode<uint64_t>(alu_out, 0);
      } else if (op == SLG) {
        uint64_t mem_val =
            static_cast<uint64_t>(ReadDW(b2_val + d2_val + x2_val));
        alu_out -= mem_val;
        SetS390ConditionCode<uint64_t>(alu_out, 0);
      } else if (op == AGF) {
        uint32_t mem_val = ReadW(b2_val + d2_val + x2_val, instr);
        alu_out += mem_val;
        SetS390ConditionCode<int64_t>(alu_out, 0);
      } else if (op == SGF) {
        uint32_t mem_val = ReadW(b2_val + d2_val + x2_val, instr);
        alu_out -= mem_val;
        SetS390ConditionCode<int64_t>(alu_out, 0);
      } else {
        DCHECK(false);
      }
      set_register(r1, alu_out);
      break;
    }
    case ALGFI:
    case SLGFI: {
#ifndef V8_TARGET_ARCH_S390X
      // should only be called on 64bit
      DCHECK(false);
#endif
      RILInstruction* rilInstr = reinterpret_cast<RILInstruction*>(instr);
      int r1 = rilInstr->R1Value();
      uint32_t i2 = rilInstr->I2UnsignedValue();
      uint64_t r1_val = (uint64_t)(get_register(r1));
      uint64_t alu_out;
      if (op == ALGFI)
        alu_out = r1_val + i2;
      else
        alu_out = r1_val - i2;
      set_register(r1, (intptr_t)alu_out);
      SetS390ConditionCode<uint64_t>(alu_out, 0);
      break;
    }
    case MSY:
    case MSG: {
      RXYInstruction* rxyInstr = reinterpret_cast<RXYInstruction*>(instr);
      int r1 = rxyInstr->R1Value();
      int b2 = rxyInstr->B2Value();
      int x2 = rxyInstr->X2Value();
      int64_t b2_val = (b2 == 0) ? 0 : get_register(b2);
      int64_t x2_val = (x2 == 0) ? 0 : get_register(x2);
      intptr_t d2_val = rxyInstr->D2Value();
      if (op == MSY) {
        int32_t mem_val = ReadW(b2_val + d2_val + x2_val, instr);
        int32_t r1_val = get_low_register<int32_t>(r1);
        set_low_register(r1, mem_val * r1_val);
      } else if (op == MSG) {
        int64_t mem_val = ReadDW(b2_val + d2_val + x2_val);
        int64_t r1_val = get_register(r1);
        set_register(r1, mem_val * r1_val);
      } else {
        UNREACHABLE();
      }
      break;
    }
    case MSFI:
    case MSGFI: {
      RILInstruction* rilinst = reinterpret_cast<RILInstruction*>(instr);
      int r1 = rilinst->R1Value();
      int32_t i2 = rilinst->I2Value();
      if (op == MSFI) {
        int32_t alu_out = get_low_register<int32_t>(r1);
        alu_out = alu_out * i2;
        set_low_register(r1, alu_out);
      } else if (op == MSGFI) {
        int64_t alu_out = get_register(r1);
        alu_out = alu_out * i2;
        set_register(r1, alu_out);
      } else {
        UNREACHABLE();
      }
      break;
    }
    default:
      UNREACHABLE();
      return false;
  }
  return true;
}

int16_t Simulator::ByteReverse(int16_t hword) {
  return (hword << 8) | ((hword >> 8) & 0x00ff);
}

int32_t Simulator::ByteReverse(int32_t word) {
  int32_t result = word << 24;
  result |= (word << 8) & 0x00ff0000;
  result |= (word >> 8) & 0x0000ff00;
  result |= (word >> 24) & 0x00000ff;
  return result;
}

// Executes the current instruction.
void Simulator::ExecuteInstruction(Instruction* instr, bool auto_incr_pc) {
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
#ifdef V8_TARGET_ARCH_S390X
    PrintF("%05ld  %08" V8PRIxPTR "  %s\n", icount_,
           reinterpret_cast<intptr_t>(instr), buffer.start());
#else
    PrintF("%05lld  %08" V8PRIxPTR "  %s\n", icount_,
           reinterpret_cast<intptr_t>(instr), buffer.start());
#endif
    // Flush stdout to prevent incomplete file output during abnormal exits
    // This is caused by the output being buffered before being written to file
    fflush(stdout);
  }

  // Try to simulate as S390 Instruction first.
  bool processed = true;

  int instrLength = instr->InstructionLength();
  if (instrLength == 2)
    processed = DecodeTwoByte(instr);
  else if (instrLength == 4)
    processed = DecodeFourByte(instr);
  else if (instrLength == 6)
    processed = DecodeSixByte(instr);

  if (processed) {
    if (!pc_modified_ && auto_incr_pc) {
      set_pc(reinterpret_cast<intptr_t>(instr) + instrLength);
    }
    return;
  }
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
      icount_++;
      ExecuteInstruction(instr);
      program_counter = get_pc();
    }
  } else {
    // FLAG_stop_sim_at is at the non-default value. Stop in the debugger when
    // we reach the particular instuction count.
    while (program_counter != end_sim_pc) {
      Instruction* instr = reinterpret_cast<Instruction*>(program_counter);
      icount_++;
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
  intptr_t callee_saved_value = icount_;
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
    DCHECK_EQ(callee_saved_value + 6, get_low_register<int32_t>(r6));
  }
  DCHECK_EQ(callee_saved_value + 7, get_low_register<int32_t>(r7));
  DCHECK_EQ(callee_saved_value + 8, get_low_register<int32_t>(r8));
  DCHECK_EQ(callee_saved_value + 9, get_low_register<int32_t>(r9));
  DCHECK_EQ(callee_saved_value + 10, get_low_register<int32_t>(r10));
  DCHECK_EQ(callee_saved_value + 11, get_low_register<int32_t>(r11));
  DCHECK_EQ(callee_saved_value + 12, get_low_register<int32_t>(r12));
  DCHECK_EQ(callee_saved_value + 13, get_low_register<int32_t>(r13));
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
  intptr_t entry_stack =
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
  intptr_t callee_saved_value = icount_;
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
    DCHECK_EQ(callee_saved_value + 6, get_low_register<int32_t>(r6));
  }
  DCHECK_EQ(callee_saved_value + 7, get_low_register<int32_t>(r7));
  DCHECK_EQ(callee_saved_value + 8, get_low_register<int32_t>(r8));
  DCHECK_EQ(callee_saved_value + 9, get_low_register<int32_t>(r9));
  DCHECK_EQ(callee_saved_value + 10, get_low_register<int32_t>(r10));
  DCHECK_EQ(callee_saved_value + 11, get_low_register<int32_t>(r11));
  DCHECK_EQ(callee_saved_value + 12, get_low_register<int32_t>(r12));
  DCHECK_EQ(callee_saved_value + 13, get_low_register<int32_t>(r13));
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
  DCHECK_EQ(entry_stack, get_low_register<int32_t>(sp));
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

}  // namespace internal
}  // namespace v8

#endif  // USE_SIMULATOR
#endif  // V8_TARGET_ARCH_S390
