// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stdlib.h>
#include <cmath>

#if V8_TARGET_ARCH_PPC

#include "src/assembler.h"
#include "src/base/bits.h"
#include "src/codegen.h"
#include "src/disasm.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"
#include "src/ppc/constants-ppc.h"
#include "src/ppc/frame-constants-ppc.h"
#include "src/ppc/simulator-ppc.h"
#include "src/runtime/runtime-utils.h"

#if defined(USE_SIMULATOR)

// Only build the simulator if not compiling for real PPC hardware.
namespace v8 {
namespace internal {

const auto GetRegConfig = RegisterConfiguration::Default;

// static
base::LazyInstance<Simulator::GlobalMonitor>::type Simulator::global_monitor_ =
    LAZY_INSTANCE_INITIALIZER;

// This macro provides a platform independent use of sscanf. The reason for
// SScanF not being implemented in a platform independent way through
// ::v8::internal::OS in the same way as SNPrintF is that the
// Windows C Run-Time Library does not provide vsscanf.
#define SScanF sscanf  // NOLINT

// The PPCDebugger class is used by the simulator while debugging simulated
// PowerPC code.
class PPCDebugger {
 public:
  explicit PPCDebugger(Simulator* sim) : sim_(sim) {}

  void Stop(Instruction* instr);
  void Debug();

 private:
  static const Instr kBreakpointInstr = (TWI | 0x1F * B21);
  static const Instr kNopInstr = (ORI);  // ori, 0,0,0

  Simulator* sim_;

  intptr_t GetRegisterValue(int regnum);
  double GetRegisterPairDoubleValue(int regnum);
  double GetFPDoubleRegisterValue(int regnum);
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

void PPCDebugger::Stop(Instruction* instr) {
  // Get the stop code.
  // use of kStopCodeMask not right on PowerPC
  uint32_t code = instr->SvcValue() & kStopCodeMask;
  // Retrieve the encoded address, which comes just after this stop.
  char* msg =
      *reinterpret_cast<char**>(sim_->get_pc() + Instruction::kInstrSize);
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
  sim_->set_pc(sim_->get_pc() + Instruction::kInstrSize + kPointerSize);
  Debug();
}

intptr_t PPCDebugger::GetRegisterValue(int regnum) {
  return sim_->get_register(regnum);
}


double PPCDebugger::GetRegisterPairDoubleValue(int regnum) {
  return sim_->get_double_from_register_pair(regnum);
}


double PPCDebugger::GetFPDoubleRegisterValue(int regnum) {
  return sim_->get_double_from_d_register(regnum);
}


bool PPCDebugger::GetValue(const char* desc, intptr_t* value) {
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


bool PPCDebugger::GetFPDoubleValue(const char* desc, double* value) {
  int regnum = DoubleRegisters::Number(desc);
  if (regnum != kNoRegister) {
    *value = sim_->get_double_from_d_register(regnum);
    return true;
  }
  return false;
}


bool PPCDebugger::SetBreakpoint(Instruction* break_pc) {
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


bool PPCDebugger::DeleteBreakpoint(Instruction* break_pc) {
  if (sim_->break_pc_ != nullptr) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }

  sim_->break_pc_ = nullptr;
  sim_->break_instr_ = 0;
  return true;
}


void PPCDebugger::UndoBreakpoints() {
  if (sim_->break_pc_ != nullptr) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }
}


void PPCDebugger::RedoBreakpoints() {
  if (sim_->break_pc_ != nullptr) {
    sim_->break_pc_->SetInstructionBits(kBreakpointInstr);
  }
}


void PPCDebugger::Debug() {
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
                ->InstructionBits() == 0x7D821008) {
          sim_->set_pc(sim_->get_pc() + Instruction::kInstrSize);
        } else {
          sim_->ExecuteInstruction(
              reinterpret_cast<Instruction*>(sim_->get_pc()));
        }

        if (argc == 2 && last_pc != sim_->get_pc() && GetValue(arg1, &value)) {
          for (int i = 1; i < value; i++) {
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
                ->InstructionBits() == 0x7D821008) {
          sim_->set_pc(sim_->get_pc() + Instruction::kInstrSize);
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
            PrintF("  pc: %08" V8PRIxPTR "  lr: %08" V8PRIxPTR
                   "  "
                   "ctr: %08" V8PRIxPTR "  xer: %08x  cr: %08x\n",
                   sim_->special_reg_pc_, sim_->special_reg_lr_,
                   sim_->special_reg_ctr_, sim_->special_reg_xer_,
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
            PrintF("   pc: %08" V8PRIxPTR "  lr: %08" V8PRIxPTR
                   "  "
                   "ctr: %08" V8PRIxPTR "  xer: %08x  cr: %08x\n",
                   sim_->special_reg_pc_, sim_->special_reg_lr_,
                   sim_->special_reg_ctr_, sim_->special_reg_xer_,
                   sim_->condition_reg_);
          } else if (strcmp(arg1, "allf") == 0) {
            for (int i = 0; i < DoubleRegister::kNumRegisters; i++) {
              dvalue = GetFPDoubleRegisterValue(i);
              uint64_t as_words = bit_cast<uint64_t>(dvalue);
              PrintF("%3s: %f 0x%08x %08x\n",
                     GetRegConfig()->GetDoubleRegisterName(i), dvalue,
                     static_cast<uint32_t>(as_words >> 32),
                     static_cast<uint32_t>(as_words & 0xFFFFFFFF));
            }
          } else if (arg1[0] == 'r' &&
                     (arg1[1] >= '0' && arg1[1] <= '9' &&
                      (arg1[2] == '\0' || (arg1[2] >= '0' && arg1[2] <= '9' &&
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
                     static_cast<uint32_t>(as_words & 0xFFFFFFFF));
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
            PrintF(" (");
            if ((value & 1) == 0) {
              PrintF("smi %d", PlatformSmiTagging::SmiToInt(obj));
            } else {
              obj->ShortPrint();
            }
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
        byte* end = nullptr;

        if (argc == 1) {
          cur = reinterpret_cast<byte*>(sim_->get_pc());
          end = cur + (10 * Instruction::kInstrSize);
        } else if (argc == 2) {
          int regnum = Registers::Number(arg1);
          if (regnum != kNoRegister || strncmp(arg1, "0x", 2) == 0) {
            // The argument is an address or a register name.
            intptr_t value;
            if (GetValue(arg1, &value)) {
              cur = reinterpret_cast<byte*>(value);
              // Disassemble 10 instructions at <arg1>.
              end = cur + (10 * Instruction::kInstrSize);
            }
          } else {
            // The argument is the number of instructions.
            intptr_t value;
            if (GetValue(arg1, &value)) {
              cur = reinterpret_cast<byte*>(sim_->get_pc());
              // Disassemble <arg1> instructions.
              end = cur + (value * Instruction::kInstrSize);
            }
          }
        } else {
          intptr_t value1;
          intptr_t value2;
          if (GetValue(arg1, &value1) && GetValue(arg2, &value2)) {
            cur = reinterpret_cast<byte*>(value1);
            end = cur + (value2 * Instruction::kInstrSize);
          }
        }

        while (cur < end) {
          prev = cur;
          cur += dasm.InstructionDecode(buffer, cur);
          PrintF("  0x%08" V8PRIxPTR "  %s\n", reinterpret_cast<intptr_t>(prev),
                 buffer.start());
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
      } else if (strcmp(cmd, "lr") == 0) {
        PrintF("Link reg: %08" V8PRIxPTR "\n", sim_->special_reg_lr_);
      } else if (strcmp(cmd, "ctr") == 0) {
        PrintF("Ctr reg: %08" V8PRIxPTR "\n", sim_->special_reg_ctr_);
      } else if (strcmp(cmd, "xer") == 0) {
        PrintF("XER: %08x\n", sim_->special_reg_xer_);
      } else if (strcmp(cmd, "fpscr") == 0) {
        PrintF("FPSCR: %08x\n", sim_->fp_condition_reg_);
      } else if (strcmp(cmd, "stop") == 0) {
        intptr_t value;
        intptr_t stop_pc =
            sim_->get_pc() - (Instruction::kInstrSize + kPointerSize);
        Instruction* stop_instr = reinterpret_cast<Instruction*>(stop_pc);
        Instruction* msg_address =
            reinterpret_cast<Instruction*>(stop_pc + Instruction::kInstrSize);
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
        PrintF("lr\n");
        PrintF("  print link register\n");
        PrintF("ctr\n");
        PrintF("  print ctr register\n");
        PrintF("xer\n");
        PrintF("  print XER\n");
        PrintF("fpscr\n");
        PrintF("  print FPSCR\n");
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
        PrintF("    stop and and give control to the PPCDebugger.\n");
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

void Simulator::SetRedirectInstruction(Instruction* instruction) {
  instruction->SetInstructionBits(rtCallRedirInstr | kCallRtRedirected);
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
    CHECK_EQ(0,
             memcmp(reinterpret_cast<void*>(instr),
                    cache_page->CachedData(offset), Instruction::kInstrSize));
  } else {
    // Cache miss.  Load memory into the cache.
    memcpy(cached_line, line, CachePage::kLineLength);
    *cache_valid_byte = CachePage::LINE_VALID;
  }
}


Simulator::Simulator(Isolate* isolate) : isolate_(isolate) {
  i_cache_ = isolate_->simulator_i_cache();
  if (i_cache_ == nullptr) {
    i_cache_ = new base::CustomMatcherHashMap(&ICacheMatch);
    isolate_->set_simulator_i_cache(i_cache_);
  }
// Set up simulator support first. Some of this information is needed to
// setup the architecture state.
#if V8_TARGET_ARCH_PPC64
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

  // Set up architecture state.
  // All registers are initialized to zero to start with.
  for (int i = 0; i < kNumGPRs; i++) {
    registers_[i] = 0;
  }
  condition_reg_ = 0;
  fp_condition_reg_ = 0;
  special_reg_pc_ = 0;
  special_reg_lr_ = 0;
  special_reg_ctr_ = 0;

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

Simulator::~Simulator() {
  global_monitor_.Pointer()->RemoveProcessor(&global_monitor_processor_);
  free(stack_);
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
void Simulator::set_register(int reg, intptr_t value) {
  DCHECK((reg >= 0) && (reg < kNumGPRs));
  registers_[reg] = value;
}


// Get the register from the architecture state.
intptr_t Simulator::get_register(int reg) const {
  DCHECK((reg >= 0) && (reg < kNumGPRs));
  // Stupid code added to avoid bug in GCC.
  // See: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949
  if (reg >= kNumGPRs) return 0;
  // End stupid code.
  return registers_[reg];
}


double Simulator::get_double_from_register_pair(int reg) {
  DCHECK((reg >= 0) && (reg < kNumGPRs) && ((reg % 2) == 0));

  double dm_val = 0.0;
#if !V8_TARGET_ARCH_PPC64  // doesn't make sense in 64bit mode
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
// All are consructed here from d1, d2 and r3.
void Simulator::GetFpArgs(double* x, double* y, intptr_t* z) {
  *x = get_double_from_d_register(1);
  *y = get_double_from_d_register(2);
  *z = get_register(3);
}


// The return value is in d1.
void Simulator::SetFpResult(const double& result) {
  set_d_register_from_double(1, result);
}


void Simulator::TrashCallerSaveRegisters() {
// We don't trash the registers with the return value.
#if 0  // A good idea to trash volatile registers, needs to be done
  registers_[2] = 0x50BAD4U;
  registers_[3] = 0x50BAD4U;
  registers_[12] = 0x50BAD4U;
#endif
}


uint32_t Simulator::ReadWU(intptr_t addr, Instruction* instr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoad(addr);
  uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
  return *ptr;
}

uint32_t Simulator::ReadExWU(intptr_t addr, Instruction* instr) {
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoadExcl(addr, TransactionSize::Word);
  global_monitor_.Pointer()->NotifyLoadExcl_Locked(addr,
                                                   &global_monitor_processor_);
  uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
  return *ptr;
}

int32_t Simulator::ReadW(intptr_t addr, Instruction* instr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoad(addr);
  int32_t* ptr = reinterpret_cast<int32_t*>(addr);
  return *ptr;
}


void Simulator::WriteW(intptr_t addr, uint32_t value, Instruction* instr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyStore(addr);
  global_monitor_.Pointer()->NotifyStore_Locked(addr,
                                                &global_monitor_processor_);
  uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
  *ptr = value;
  return;
}

int Simulator::WriteExW(intptr_t addr, uint32_t value, Instruction* instr) {
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  if (local_monitor_.NotifyStoreExcl(addr, TransactionSize::Word) &&
      global_monitor_.Pointer()->NotifyStoreExcl_Locked(
          addr, &global_monitor_processor_)) {
    uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
    *ptr = value;
    return 0;
  } else {
    return 1;
  }
}

void Simulator::WriteW(intptr_t addr, int32_t value, Instruction* instr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyStore(addr);
  global_monitor_.Pointer()->NotifyStore_Locked(addr,
                                                &global_monitor_processor_);
  int32_t* ptr = reinterpret_cast<int32_t*>(addr);
  *ptr = value;
  return;
}

uint16_t Simulator::ReadHU(intptr_t addr, Instruction* instr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoad(addr);
  uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
  return *ptr;
}

uint16_t Simulator::ReadExHU(intptr_t addr, Instruction* instr) {
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoadExcl(addr, TransactionSize::HalfWord);
  global_monitor_.Pointer()->NotifyLoadExcl_Locked(addr,
                                                   &global_monitor_processor_);
  uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
  return *ptr;
}

int16_t Simulator::ReadH(intptr_t addr, Instruction* instr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoad(addr);
  int16_t* ptr = reinterpret_cast<int16_t*>(addr);
  return *ptr;
}


void Simulator::WriteH(intptr_t addr, uint16_t value, Instruction* instr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyStore(addr);
  global_monitor_.Pointer()->NotifyStore_Locked(addr,
                                                &global_monitor_processor_);
  uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
  *ptr = value;
  return;
}


void Simulator::WriteH(intptr_t addr, int16_t value, Instruction* instr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyStore(addr);
  global_monitor_.Pointer()->NotifyStore_Locked(addr,
                                                &global_monitor_processor_);
  int16_t* ptr = reinterpret_cast<int16_t*>(addr);
  *ptr = value;
  return;
}

int Simulator::WriteExH(intptr_t addr, uint16_t value, Instruction* instr) {
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  if (local_monitor_.NotifyStoreExcl(addr, TransactionSize::HalfWord) &&
      global_monitor_.Pointer()->NotifyStoreExcl_Locked(
          addr, &global_monitor_processor_)) {
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    *ptr = value;
    return 0;
  } else {
    return 1;
  }
}

uint8_t Simulator::ReadBU(intptr_t addr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoad(addr);
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  return *ptr;
}


int8_t Simulator::ReadB(intptr_t addr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoad(addr);
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  return *ptr;
}

uint8_t Simulator::ReadExBU(intptr_t addr) {
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoadExcl(addr, TransactionSize::Byte);
  global_monitor_.Pointer()->NotifyLoadExcl_Locked(addr,
                                                   &global_monitor_processor_);
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  return *ptr;
}

void Simulator::WriteB(intptr_t addr, uint8_t value) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyStore(addr);
  global_monitor_.Pointer()->NotifyStore_Locked(addr,
                                                &global_monitor_processor_);
  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  *ptr = value;
}


void Simulator::WriteB(intptr_t addr, int8_t value) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyStore(addr);
  global_monitor_.Pointer()->NotifyStore_Locked(addr,
                                                &global_monitor_processor_);
  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  *ptr = value;
}

int Simulator::WriteExB(intptr_t addr, uint8_t value) {
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  if (local_monitor_.NotifyStoreExcl(addr, TransactionSize::Byte) &&
      global_monitor_.Pointer()->NotifyStoreExcl_Locked(
          addr, &global_monitor_processor_)) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
    *ptr = value;
    return 0;
  } else {
    return 1;
  }
}

intptr_t* Simulator::ReadDW(intptr_t addr) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyLoad(addr);
  intptr_t* ptr = reinterpret_cast<intptr_t*>(addr);
  return ptr;
}


void Simulator::WriteDW(intptr_t addr, int64_t value) {
  // All supported PPC targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  base::LockGuard<base::Mutex> lock_guard(&global_monitor_.Pointer()->mutex);
  local_monitor_.NotifyStore(addr);
  global_monitor_.Pointer()->NotifyStore_Locked(addr,
                                                &global_monitor_processor_);
  int64_t* ptr = reinterpret_cast<int64_t*>(addr);
  *ptr = value;
  return;
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
  uint32_t urest = 0xFFFFFFFFU - uleft;

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
bool Simulator::OverflowFrom(int32_t alu_out, int32_t left, int32_t right,
                             bool addition) {
  bool overflow;
  if (addition) {
    // operands have the same sign
    overflow = ((left >= 0 && right >= 0) || (left < 0 && right < 0))
               // and operands and result have different sign
               &&
               ((left < 0 && alu_out >= 0) || (left >= 0 && alu_out < 0));
  } else {
    // operands have different signs
    overflow = ((left < 0 && right >= 0) || (left >= 0 && right < 0))
               // and first operand and result have different signs
               &&
               ((left < 0 && alu_out >= 0) || (left >= 0 && alu_out < 0));
  }
  return overflow;
}


#if V8_TARGET_ARCH_PPC64
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
      Redirection* redirection = Redirection::FromInstruction(instr);
      const int kArgCount = 9;
      const int kRegisterArgCount = 8;
      int arg0_regnum = 3;
      intptr_t result_buffer = 0;
      bool uses_result_buffer =
          (redirection->type() == ExternalReference::BUILTIN_CALL_PAIR &&
           !ABI_RETURNS_OBJECT_PAIRS_IN_REGS);
      if (uses_result_buffer) {
        result_buffer = get_register(r3);
        arg0_regnum++;
      }
      intptr_t arg[kArgCount];
      // First eight arguments in registers r3-r10.
      for (int i = 0; i < kRegisterArgCount; i++) {
        arg[i] = get_register(arg0_regnum + i);
      }
      intptr_t* stack_pointer = reinterpret_cast<intptr_t*>(get_register(sp));
      // Remaining argument on stack
      arg[kRegisterArgCount] = stack_pointer[kStackFrameExtraParamSlot];
      STATIC_ASSERT(kArgCount == kRegisterArgCount + 1);
      STATIC_ASSERT(kMaxCParameters == 9);
      bool fp_call =
          (redirection->type() == ExternalReference::BUILTIN_FP_FP_CALL) ||
          (redirection->type() == ExternalReference::BUILTIN_COMPARE_CALL) ||
          (redirection->type() == ExternalReference::BUILTIN_FP_CALL) ||
          (redirection->type() == ExternalReference::BUILTIN_FP_INT_CALL);
      // This is dodgy but it works because the C entry stubs are never moved.
      // See comment in codegen-arm.cc and bug 1242173.
      intptr_t saved_lr = special_reg_lr_;
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
                     static_cast<void*>(FUNCTION_ADDR(generic_target)),
                     dval0, dval1);
              break;
            case ExternalReference::BUILTIN_FP_CALL:
              PrintF("Call to host function at %p with arg %f",
                     static_cast<void*>(FUNCTION_ADDR(generic_target)),
                     dval0);
              break;
            case ExternalReference::BUILTIN_FP_INT_CALL:
              PrintF("Call to host function at %p with args %f, %" V8PRIdPTR,
                     static_cast<void*>(FUNCTION_ADDR(generic_target)),
                     dval0, ival);
              break;
            default:
              UNREACHABLE();
              break;
          }
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08" V8PRIxPTR "\n",
                   get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        switch (redirection->type()) {
          case ExternalReference::BUILTIN_COMPARE_CALL: {
            SimulatorRuntimeCompareCall target =
                reinterpret_cast<SimulatorRuntimeCompareCall>(external);
            iresult = target(dval0, dval1);
            set_register(r3, iresult);
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
                   get_register(sp));
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
                   get_register(sp));
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
                   get_register(sp));
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
                   get_register(sp));
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
                   get_register(sp));
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
          if (ABI_RETURNS_OBJECT_PAIRS_IN_REGS) {
            set_register(r3, x);
            set_register(r4, y);
          } else {
            memcpy(reinterpret_cast<void*>(result_buffer), &result,
                   sizeof(ObjectPair));
            set_register(r3, result_buffer);
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
          set_register(r3, result);
        }
      }
      set_pc(saved_lr);
      break;
    }
    case kBreakpoint: {
      PPCDebugger dbg(this);
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
          PPCDebugger dbg(this);
          dbg.Stop(instr);
        } else {
          set_pc(get_pc() + Instruction::kInstrSize + kPointerSize);
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
  if ((watched_stops_[code].count & ~(1 << 31)) == 0x7FFFFFFF) {
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


void Simulator::SetCR0(intptr_t result, bool setSO) {
  int bf = 0;
  if (result < 0) {
    bf |= 0x80000000;
  }
  if (result > 0) {
    bf |= 0x40000000;
  }
  if (result == 0) {
    bf |= 0x20000000;
  }
  if (setSO) {
    bf |= 0x10000000;
  }
  condition_reg_ = (condition_reg_ & ~0xF0000000) | bf;
}


void Simulator::ExecuteBranchConditional(Instruction* instr, BCType type) {
  int bo = instr->Bits(25, 21) << 21;
  int condition_bit = instr->Bits(20, 16);
  int condition_mask = 0x80000000 >> condition_bit;
  switch (bo) {
    case DCBNZF:  // Decrement CTR; branch if CTR != 0 and condition false
    case DCBEZF:  // Decrement CTR; branch if CTR == 0 and condition false
      UNIMPLEMENTED();
    case BF: {  // Branch if condition false
      if (condition_reg_ & condition_mask) return;
      break;
    }
    case DCBNZT:  // Decrement CTR; branch if CTR != 0 and condition true
    case DCBEZT:  // Decrement CTR; branch if CTR == 0 and condition true
      UNIMPLEMENTED();
    case BT: {  // Branch if condition true
      if (!(condition_reg_ & condition_mask)) return;
      break;
    }
    case DCBNZ:  // Decrement CTR; branch if CTR != 0
    case DCBEZ:  // Decrement CTR; branch if CTR == 0
      special_reg_ctr_ -= 1;
      if ((special_reg_ctr_ == 0) != (bo == DCBEZ)) return;
      break;
    case BA: {                   // Branch always
      break;
    }
    default:
      UNIMPLEMENTED();  // Invalid encoding
  }

  intptr_t old_pc = get_pc();

  switch (type) {
    case BC_OFFSET: {
      int offset = (instr->Bits(15, 2) << 18) >> 16;
      set_pc(old_pc + offset);
      break;
    }
    case BC_LINK_REG:
      set_pc(special_reg_lr_);
      break;
    case BC_CTR_REG:
      set_pc(special_reg_ctr_);
      break;
  }

  if (instr->Bit(0) == 1) {  // LK flag set
    special_reg_lr_ = old_pc + 4;
  }
}

void Simulator::ExecuteGeneric(Instruction* instr) {
  uint32_t opcode = instr->OpcodeBase();
  switch (opcode) {
    case SUBFIC: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      intptr_t ra_val = get_register(ra);
      int32_t im_val = instr->Bits(15, 0);
      im_val = SIGN_EXT_IMM16(im_val);
      intptr_t alu_out = im_val - ra_val;
      set_register(rt, alu_out);
      // todo - handle RC bit
      break;
    }
    case CMPLI: {
      int ra = instr->RAValue();
      uint32_t im_val = instr->Bits(15, 0);
      int cr = instr->Bits(25, 23);
      uint32_t bf = 0;
#if V8_TARGET_ARCH_PPC64
      int L = instr->Bit(21);
      if (L) {
#endif
        uintptr_t ra_val = get_register(ra);
        if (ra_val < im_val) {
          bf |= 0x80000000;
        }
        if (ra_val > im_val) {
          bf |= 0x40000000;
        }
        if (ra_val == im_val) {
          bf |= 0x20000000;
        }
#if V8_TARGET_ARCH_PPC64
      } else {
        uint32_t ra_val = get_register(ra);
        if (ra_val < im_val) {
          bf |= 0x80000000;
        }
        if (ra_val > im_val) {
          bf |= 0x40000000;
        }
        if (ra_val == im_val) {
          bf |= 0x20000000;
        }
      }
#endif
      uint32_t condition_mask = 0xF0000000U >> (cr * 4);
      uint32_t condition = bf >> (cr * 4);
      condition_reg_ = (condition_reg_ & ~condition_mask) | condition;
      break;
    }
    case CMPI: {
      int ra = instr->RAValue();
      int32_t im_val = instr->Bits(15, 0);
      im_val = SIGN_EXT_IMM16(im_val);
      int cr = instr->Bits(25, 23);
      uint32_t bf = 0;
#if V8_TARGET_ARCH_PPC64
      int L = instr->Bit(21);
      if (L) {
#endif
        intptr_t ra_val = get_register(ra);
        if (ra_val < im_val) {
          bf |= 0x80000000;
        }
        if (ra_val > im_val) {
          bf |= 0x40000000;
        }
        if (ra_val == im_val) {
          bf |= 0x20000000;
        }
#if V8_TARGET_ARCH_PPC64
      } else {
        int32_t ra_val = get_register(ra);
        if (ra_val < im_val) {
          bf |= 0x80000000;
        }
        if (ra_val > im_val) {
          bf |= 0x40000000;
        }
        if (ra_val == im_val) {
          bf |= 0x20000000;
        }
      }
#endif
      uint32_t condition_mask = 0xF0000000U >> (cr * 4);
      uint32_t condition = bf >> (cr * 4);
      condition_reg_ = (condition_reg_ & ~condition_mask) | condition;
      break;
    }
    case ADDIC: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      uintptr_t ra_val = get_register(ra);
      uintptr_t im_val = SIGN_EXT_IMM16(instr->Bits(15, 0));
      uintptr_t alu_out = ra_val + im_val;
      // Check overflow
      if (~ra_val < im_val) {
        special_reg_xer_ = (special_reg_xer_ & ~0xF0000000) | 0x20000000;
      } else {
        special_reg_xer_ &= ~0xF0000000;
      }
      set_register(rt, alu_out);
      break;
    }
    case ADDI: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int32_t im_val = SIGN_EXT_IMM16(instr->Bits(15, 0));
      intptr_t alu_out;
      if (ra == 0) {
        alu_out = im_val;
      } else {
        intptr_t ra_val = get_register(ra);
        alu_out = ra_val + im_val;
      }
      set_register(rt, alu_out);
      // todo - handle RC bit
      break;
    }
    case ADDIS: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int32_t im_val = (instr->Bits(15, 0) << 16);
      intptr_t alu_out;
      if (ra == 0) {  // treat r0 as zero
        alu_out = im_val;
      } else {
        intptr_t ra_val = get_register(ra);
        alu_out = ra_val + im_val;
      }
      set_register(rt, alu_out);
      break;
    }
    case BCX: {
      ExecuteBranchConditional(instr, BC_OFFSET);
      break;
    }
    case BX: {
      int offset = (instr->Bits(25, 2) << 8) >> 6;
      if (instr->Bit(0) == 1) {  // LK flag set
        special_reg_lr_ = get_pc() + 4;
      }
      set_pc(get_pc() + offset);
      // todo - AA flag
      break;
    }
    case MCRF:
      UNIMPLEMENTED();  // Not used by V8.
    case BCLRX:
      ExecuteBranchConditional(instr, BC_LINK_REG);
      break;
    case BCCTRX:
      ExecuteBranchConditional(instr, BC_CTR_REG);
      break;
    case CRNOR:
    case RFI:
    case CRANDC:
      UNIMPLEMENTED();
    case ISYNC: {
      // todo - simulate isync
      break;
    }
    case CRXOR: {
      int bt = instr->Bits(25, 21);
      int ba = instr->Bits(20, 16);
      int bb = instr->Bits(15, 11);
      int ba_val = ((0x80000000 >> ba) & condition_reg_) == 0 ? 0 : 1;
      int bb_val = ((0x80000000 >> bb) & condition_reg_) == 0 ? 0 : 1;
      int bt_val = ba_val ^ bb_val;
      bt_val = bt_val << (31 - bt);  // shift bit to correct destination
      condition_reg_ &= ~(0x80000000 >> bt);
      condition_reg_ |= bt_val;
      break;
    }
    case CREQV: {
      int bt = instr->Bits(25, 21);
      int ba = instr->Bits(20, 16);
      int bb = instr->Bits(15, 11);
      int ba_val = ((0x80000000 >> ba) & condition_reg_) == 0 ? 0 : 1;
      int bb_val = ((0x80000000 >> bb) & condition_reg_) == 0 ? 0 : 1;
      int bt_val = 1 - (ba_val ^ bb_val);
      bt_val = bt_val << (31 - bt);  // shift bit to correct destination
      condition_reg_ &= ~(0x80000000 >> bt);
      condition_reg_ |= bt_val;
      break;
    }
    case CRNAND:
    case CRAND:
    case CRORC:
    case CROR: {
      UNIMPLEMENTED();  // Not used by V8.
      break;
    }
    case RLWIMIX: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      uint32_t rs_val = get_register(rs);
      int32_t ra_val = get_register(ra);
      int sh = instr->Bits(15, 11);
      int mb = instr->Bits(10, 6);
      int me = instr->Bits(5, 1);
      uint32_t result = base::bits::RotateLeft32(rs_val, sh);
      int mask = 0;
      if (mb < me + 1) {
        int bit = 0x80000000 >> mb;
        for (; mb <= me; mb++) {
          mask |= bit;
          bit >>= 1;
        }
      } else if (mb == me + 1) {
        mask = 0xFFFFFFFF;
      } else {                             // mb > me+1
        int bit = 0x80000000 >> (me + 1);  // needs to be tested
        mask = 0xFFFFFFFF;
        for (; me < mb; me++) {
          mask ^= bit;
          bit >>= 1;
        }
      }
      result &= mask;
      ra_val &= ~mask;
      result |= ra_val;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
    case RLWINMX:
    case RLWNMX: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      uint32_t rs_val = get_register(rs);
      int sh = 0;
      if (opcode == RLWINMX) {
        sh = instr->Bits(15, 11);
      } else {
        int rb = instr->RBValue();
        uint32_t rb_val = get_register(rb);
        sh = (rb_val & 0x1F);
      }
      int mb = instr->Bits(10, 6);
      int me = instr->Bits(5, 1);
      uint32_t result = base::bits::RotateLeft32(rs_val, sh);
      int mask = 0;
      if (mb < me + 1) {
        int bit = 0x80000000 >> mb;
        for (; mb <= me; mb++) {
          mask |= bit;
          bit >>= 1;
        }
      } else if (mb == me + 1) {
        mask = 0xFFFFFFFF;
      } else {                             // mb > me+1
        int bit = 0x80000000 >> (me + 1);  // needs to be tested
        mask = 0xFFFFFFFF;
        for (; me < mb; me++) {
          mask ^= bit;
          bit >>= 1;
        }
      }
      result &= mask;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
    case ORI: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      intptr_t rs_val = get_register(rs);
      uint32_t im_val = instr->Bits(15, 0);
      intptr_t alu_out = rs_val | im_val;
      set_register(ra, alu_out);
      break;
    }
    case ORIS: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      intptr_t rs_val = get_register(rs);
      uint32_t im_val = instr->Bits(15, 0);
      intptr_t alu_out = rs_val | (im_val << 16);
      set_register(ra, alu_out);
      break;
    }
    case XORI: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      intptr_t rs_val = get_register(rs);
      uint32_t im_val = instr->Bits(15, 0);
      intptr_t alu_out = rs_val ^ im_val;
      set_register(ra, alu_out);
      // todo - set condition based SO bit
      break;
    }
    case XORIS: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      intptr_t rs_val = get_register(rs);
      uint32_t im_val = instr->Bits(15, 0);
      intptr_t alu_out = rs_val ^ (im_val << 16);
      set_register(ra, alu_out);
      break;
    }
    case ANDIx: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      intptr_t rs_val = get_register(rs);
      uint32_t im_val = instr->Bits(15, 0);
      intptr_t alu_out = rs_val & im_val;
      set_register(ra, alu_out);
      SetCR0(alu_out);
      break;
    }
    case ANDISx: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      intptr_t rs_val = get_register(rs);
      uint32_t im_val = instr->Bits(15, 0);
      intptr_t alu_out = rs_val & (im_val << 16);
      set_register(ra, alu_out);
      SetCR0(alu_out);
      break;
    }
    case SRWX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uint32_t rs_val = get_register(rs);
      uintptr_t rb_val = get_register(rb) & 0x3F;
      intptr_t result = (rb_val > 31) ? 0 : rs_val >> rb_val;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case SRDX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uintptr_t rs_val = get_register(rs);
      uintptr_t rb_val = get_register(rb) & 0x7F;
      intptr_t result = (rb_val > 63) ? 0 : rs_val >> rb_val;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
#endif
    case MODUW: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uint32_t ra_val = get_register(ra);
      uint32_t rb_val = get_register(rb);
      uint32_t alu_out = (rb_val == 0) ? -1 : ra_val % rb_val;
      set_register(rt, alu_out);
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case MODUD: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uint64_t ra_val = get_register(ra);
      uint64_t rb_val = get_register(rb);
      uint64_t alu_out = (rb_val == 0) ? -1 : ra_val % rb_val;
      set_register(rt, alu_out);
      break;
    }
#endif
    case MODSW: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int32_t ra_val = get_register(ra);
      int32_t rb_val = get_register(rb);
      bool overflow = (ra_val == kMinInt && rb_val == -1);
      // result is undefined if divisor is zero or if operation
      // is 0x80000000 / -1.
      int32_t alu_out = (rb_val == 0 || overflow) ? -1 : ra_val % rb_val;
      set_register(rt, alu_out);
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case MODSD: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int64_t ra_val = get_register(ra);
      int64_t rb_val = get_register(rb);
      int64_t one = 1;  // work-around gcc
      int64_t kMinLongLong = (one << 63);
      // result is undefined if divisor is zero or if operation
      // is 0x80000000_00000000 / -1.
      int64_t alu_out =
          (rb_val == 0 || (ra_val == kMinLongLong && rb_val == -1))
              ? -1
              : ra_val % rb_val;
      set_register(rt, alu_out);
      break;
    }
#endif
    case SRAW: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int32_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb) & 0x3F;
      intptr_t result = (rb_val > 31) ? rs_val >> 31 : rs_val >> rb_val;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case SRAD: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb) & 0x7F;
      intptr_t result = (rb_val > 63) ? rs_val >> 63 : rs_val >> rb_val;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
#endif
    case SRAWIX: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      int sh = instr->Bits(15, 11);
      int32_t rs_val = get_register(rs);
      intptr_t result = rs_val >> sh;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case EXTSW: {
      const int shift = kBitsPerPointer - 32;
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      intptr_t rs_val = get_register(rs);
      intptr_t ra_val = (rs_val << shift) >> shift;
      set_register(ra, ra_val);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(ra_val);
      }
      break;
    }
#endif
    case EXTSH: {
      const int shift = kBitsPerPointer - 16;
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      intptr_t rs_val = get_register(rs);
      intptr_t ra_val = (rs_val << shift) >> shift;
      set_register(ra, ra_val);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(ra_val);
      }
      break;
    }
    case EXTSB: {
      const int shift = kBitsPerPointer - 8;
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      intptr_t rs_val = get_register(rs);
      intptr_t ra_val = (rs_val << shift) >> shift;
      set_register(ra, ra_val);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(ra_val);
      }
      break;
    }
    case LFSUX:
    case LFSX: {
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      int32_t val = ReadW(ra_val + rb_val, instr);
      float* fptr = reinterpret_cast<float*>(&val);
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
      // Conversion using double changes sNan to qNan on ia32/x64
      if ((val & 0x7F800000) == 0x7F800000) {
        int64_t dval = static_cast<int64_t>(val);
        dval = ((dval & 0xC0000000) << 32) | ((dval & 0x40000000) << 31) |
               ((dval & 0x40000000) << 30) | ((dval & 0x7FFFFFFF) << 29) | 0x0;
        set_d_register(frt, dval);
      } else {
        set_d_register_from_double(frt, static_cast<double>(*fptr));
      }
#else
      set_d_register_from_double(frt, static_cast<double>(*fptr));
#endif
      if (opcode == LFSUX) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
    case LFDUX:
    case LFDX: {
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      int64_t* dptr = reinterpret_cast<int64_t*>(ReadDW(ra_val + rb_val));
      set_d_register(frt, *dptr);
      if (opcode == LFDUX) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
    case STFSUX: {
      case STFSX:
        int frs = instr->RSValue();
        int ra = instr->RAValue();
        int rb = instr->RBValue();
        intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
        intptr_t rb_val = get_register(rb);
        float frs_val = static_cast<float>(get_double_from_d_register(frs));
        int32_t* p = reinterpret_cast<int32_t*>(&frs_val);
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
        // Conversion using double changes sNan to qNan on ia32/x64
        int32_t sval = 0;
        int64_t dval = get_d_register(frs);
        if ((dval & 0x7FF0000000000000) == 0x7FF0000000000000) {
          sval = ((dval & 0xC000000000000000) >> 32) |
                 ((dval & 0x07FFFFFFE0000000) >> 29);
          p = &sval;
        } else {
          p = reinterpret_cast<int32_t*>(&frs_val);
        }
#else
        p = reinterpret_cast<int32_t*>(&frs_val);
#endif
        WriteW(ra_val + rb_val, *p, instr);
        if (opcode == STFSUX) {
          DCHECK_NE(ra, 0);
          set_register(ra, ra_val + rb_val);
        }
        break;
    }
    case STFDUX: {
      case STFDX:
        int frs = instr->RSValue();
        int ra = instr->RAValue();
        int rb = instr->RBValue();
        intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
        intptr_t rb_val = get_register(rb);
        int64_t frs_val = get_d_register(frs);
        WriteDW(ra_val + rb_val, frs_val);
        if (opcode == STFDUX) {
          DCHECK_NE(ra, 0);
          set_register(ra, ra_val + rb_val);
        }
        break;
    }
    case POPCNTW: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      uintptr_t rs_val = get_register(rs);
      uintptr_t count = 0;
      int n = 0;
      uintptr_t bit = 0x80000000;
      for (; n < 32; n++) {
        if (bit & rs_val) count++;
        bit >>= 1;
      }
      set_register(ra, count);
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case POPCNTD: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      uintptr_t rs_val = get_register(rs);
      uintptr_t count = 0;
      int n = 0;
      uintptr_t bit = 0x8000000000000000UL;
      for (; n < 64; n++) {
        if (bit & rs_val) count++;
        bit >>= 1;
      }
      set_register(ra, count);
      break;
    }
#endif
    case SYNC: {
      // todo - simulate sync
      break;
    }
    case ICBI: {
      // todo - simulate icbi
      break;
    }

    case LWZU:
    case LWZ: {
      int ra = instr->RAValue();
      int rt = instr->RTValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      set_register(rt, ReadWU(ra_val + offset, instr));
      if (opcode == LWZU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case LBZU:
    case LBZ: {
      int ra = instr->RAValue();
      int rt = instr->RTValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      set_register(rt, ReadB(ra_val + offset) & 0xFF);
      if (opcode == LBZU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case STWU:
    case STW: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int32_t rs_val = get_register(rs);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      WriteW(ra_val + offset, rs_val, instr);
      if (opcode == STWU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }
    case SRADIX: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      int sh = (instr->Bits(15, 11) | (instr->Bit(1) << 5));
      intptr_t rs_val = get_register(rs);
      intptr_t result = rs_val >> sh;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
    case STBCX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int8_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      SetCR0(WriteExB(ra_val + rb_val, rs_val));
      break;
    }
    case STHCX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int16_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      SetCR0(WriteExH(ra_val + rb_val, rs_val, instr));
      break;
    }
    case STWCX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int32_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      SetCR0(WriteExW(ra_val + rb_val, rs_val, instr));
      break;
    }
    case TW: {
      // used for call redirection in simulation mode
      SoftwareInterrupt(instr);
      break;
    }
    case CMP: {
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int cr = instr->Bits(25, 23);
      uint32_t bf = 0;
#if V8_TARGET_ARCH_PPC64
      int L = instr->Bit(21);
      if (L) {
#endif
        intptr_t ra_val = get_register(ra);
        intptr_t rb_val = get_register(rb);
        if (ra_val < rb_val) {
          bf |= 0x80000000;
        }
        if (ra_val > rb_val) {
          bf |= 0x40000000;
        }
        if (ra_val == rb_val) {
          bf |= 0x20000000;
        }
#if V8_TARGET_ARCH_PPC64
      } else {
        int32_t ra_val = get_register(ra);
        int32_t rb_val = get_register(rb);
        if (ra_val < rb_val) {
          bf |= 0x80000000;
        }
        if (ra_val > rb_val) {
          bf |= 0x40000000;
        }
        if (ra_val == rb_val) {
          bf |= 0x20000000;
        }
      }
#endif
      uint32_t condition_mask = 0xF0000000U >> (cr * 4);
      uint32_t condition = bf >> (cr * 4);
      condition_reg_ = (condition_reg_ & ~condition_mask) | condition;
      break;
    }
    case SUBFCX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      // int oe = instr->Bit(10);
      uintptr_t ra_val = get_register(ra);
      uintptr_t rb_val = get_register(rb);
      uintptr_t alu_out = ~ra_val + rb_val + 1;
      // Set carry
      if (ra_val <= rb_val) {
        special_reg_xer_ = (special_reg_xer_ & ~0xF0000000) | 0x20000000;
      } else {
        special_reg_xer_ &= ~0xF0000000;
      }
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      // todo - handle OE bit
      break;
    }
    case SUBFEX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      // int oe = instr->Bit(10);
      uintptr_t ra_val = get_register(ra);
      uintptr_t rb_val = get_register(rb);
      uintptr_t alu_out = ~ra_val + rb_val;
      if (special_reg_xer_ & 0x20000000) {
        alu_out += 1;
      }
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(static_cast<intptr_t>(alu_out));
      }
      // todo - handle OE bit
      break;
    }
    case ADDCX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      // int oe = instr->Bit(10);
      uintptr_t ra_val = get_register(ra);
      uintptr_t rb_val = get_register(rb);
      uintptr_t alu_out = ra_val + rb_val;
      // Set carry
      if (~ra_val < rb_val) {
        special_reg_xer_ = (special_reg_xer_ & ~0xF0000000) | 0x20000000;
      } else {
        special_reg_xer_ &= ~0xF0000000;
      }
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(static_cast<intptr_t>(alu_out));
      }
      // todo - handle OE bit
      break;
    }
    case ADDEX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      // int oe = instr->Bit(10);
      uintptr_t ra_val = get_register(ra);
      uintptr_t rb_val = get_register(rb);
      uintptr_t alu_out = ra_val + rb_val;
      if (special_reg_xer_ & 0x20000000) {
        alu_out += 1;
      }
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(static_cast<intptr_t>(alu_out));
      }
      // todo - handle OE bit
      break;
    }
    case MULHWX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int32_t ra_val = (get_register(ra) & 0xFFFFFFFF);
      int32_t rb_val = (get_register(rb) & 0xFFFFFFFF);
      int64_t alu_out = (int64_t)ra_val * (int64_t)rb_val;
      alu_out >>= 32;
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(static_cast<intptr_t>(alu_out));
      }
      break;
    }
    case MULHWUX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uint32_t ra_val = (get_register(ra) & 0xFFFFFFFF);
      uint32_t rb_val = (get_register(rb) & 0xFFFFFFFF);
      uint64_t alu_out = (uint64_t)ra_val * (uint64_t)rb_val;
      alu_out >>= 32;
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(static_cast<intptr_t>(alu_out));
      }
      break;
    }
    case NEGX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      intptr_t ra_val = get_register(ra);
      intptr_t alu_out = 1 + ~ra_val;
#if V8_TARGET_ARCH_PPC64
      intptr_t one = 1;  // work-around gcc
      intptr_t kOverflowVal = (one << 63);
#else
      intptr_t kOverflowVal = kMinInt;
#endif
      set_register(rt, alu_out);
      if (instr->Bit(10)) {  // OE bit set
        if (ra_val == kOverflowVal) {
          special_reg_xer_ |= 0xC0000000;  // set SO,OV
        } else {
          special_reg_xer_ &= ~0x40000000;  // clear OV
        }
      }
      if (instr->Bit(0)) {  // RC bit set
        bool setSO = (special_reg_xer_ & 0x80000000);
        SetCR0(alu_out, setSO);
      }
      break;
    }
    case SLWX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uint32_t rs_val = get_register(rs);
      uintptr_t rb_val = get_register(rb) & 0x3F;
      uint32_t result = (rb_val > 31) ? 0 : rs_val << rb_val;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case SLDX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uintptr_t rs_val = get_register(rs);
      uintptr_t rb_val = get_register(rb) & 0x7F;
      uintptr_t result = (rb_val > 63) ? 0 : rs_val << rb_val;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      break;
    }
    case MFVSRD: {
      DCHECK(!instr->Bit(0));
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      int64_t frt_val = get_d_register(frt);
      set_register(ra, frt_val);
      break;
    }
    case MFVSRWZ: {
      DCHECK(!instr->Bit(0));
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      int64_t frt_val = get_d_register(frt);
      set_register(ra, static_cast<uint32_t>(frt_val));
      break;
    }
    case MTVSRD: {
      DCHECK(!instr->Bit(0));
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      int64_t ra_val = get_register(ra);
      set_d_register(frt, ra_val);
      break;
    }
    case MTVSRWA: {
      DCHECK(!instr->Bit(0));
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      int64_t ra_val = static_cast<int32_t>(get_register(ra));
      set_d_register(frt, ra_val);
      break;
    }
    case MTVSRWZ: {
      DCHECK(!instr->Bit(0));
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      uint64_t ra_val = static_cast<uint32_t>(get_register(ra));
      set_d_register(frt, ra_val);
      break;
    }
#endif
    case CNTLZWX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      uintptr_t rs_val = get_register(rs);
      uintptr_t count = 0;
      int n = 0;
      uintptr_t bit = 0x80000000;
      for (; n < 32; n++) {
        if (bit & rs_val) break;
        count++;
        bit >>= 1;
      }
      set_register(ra, count);
      if (instr->Bit(0)) {  // RC Bit set
        int bf = 0;
        if (count > 0) {
          bf |= 0x40000000;
        }
        if (count == 0) {
          bf |= 0x20000000;
        }
        condition_reg_ = (condition_reg_ & ~0xF0000000) | bf;
      }
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case CNTLZDX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      uintptr_t rs_val = get_register(rs);
      uintptr_t count = 0;
      int n = 0;
      uintptr_t bit = 0x8000000000000000UL;
      for (; n < 64; n++) {
        if (bit & rs_val) break;
        count++;
        bit >>= 1;
      }
      set_register(ra, count);
      if (instr->Bit(0)) {  // RC Bit set
        int bf = 0;
        if (count > 0) {
          bf |= 0x40000000;
        }
        if (count == 0) {
          bf |= 0x20000000;
        }
        condition_reg_ = (condition_reg_ & ~0xF0000000) | bf;
      }
      break;
    }
#endif
    case ANDX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      intptr_t alu_out = rs_val & rb_val;
      set_register(ra, alu_out);
      if (instr->Bit(0)) {  // RC Bit set
        SetCR0(alu_out);
      }
      break;
    }
    case ANDCX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      intptr_t alu_out = rs_val & ~rb_val;
      set_register(ra, alu_out);
      if (instr->Bit(0)) {  // RC Bit set
        SetCR0(alu_out);
      }
      break;
    }
    case CMPL: {
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int cr = instr->Bits(25, 23);
      uint32_t bf = 0;
#if V8_TARGET_ARCH_PPC64
      int L = instr->Bit(21);
      if (L) {
#endif
        uintptr_t ra_val = get_register(ra);
        uintptr_t rb_val = get_register(rb);
        if (ra_val < rb_val) {
          bf |= 0x80000000;
        }
        if (ra_val > rb_val) {
          bf |= 0x40000000;
        }
        if (ra_val == rb_val) {
          bf |= 0x20000000;
        }
#if V8_TARGET_ARCH_PPC64
      } else {
        uint32_t ra_val = get_register(ra);
        uint32_t rb_val = get_register(rb);
        if (ra_val < rb_val) {
          bf |= 0x80000000;
        }
        if (ra_val > rb_val) {
          bf |= 0x40000000;
        }
        if (ra_val == rb_val) {
          bf |= 0x20000000;
        }
      }
#endif
      uint32_t condition_mask = 0xF0000000U >> (cr * 4);
      uint32_t condition = bf >> (cr * 4);
      condition_reg_ = (condition_reg_ & ~condition_mask) | condition;
      break;
    }
    case SUBFX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      // int oe = instr->Bit(10);
      intptr_t ra_val = get_register(ra);
      intptr_t rb_val = get_register(rb);
      intptr_t alu_out = rb_val - ra_val;
      // todo - figure out underflow
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC Bit set
        SetCR0(alu_out);
      }
      // todo - handle OE bit
      break;
    }
    case ADDZEX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      intptr_t ra_val = get_register(ra);
      if (special_reg_xer_ & 0x20000000) {
        ra_val += 1;
      }
      set_register(rt, ra_val);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(ra_val);
      }
      // todo - handle OE bit
      break;
    }
    case NORX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      intptr_t alu_out = ~(rs_val | rb_val);
      set_register(ra, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      break;
    }
    case MULLW: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int32_t ra_val = (get_register(ra) & 0xFFFFFFFF);
      int32_t rb_val = (get_register(rb) & 0xFFFFFFFF);
      int32_t alu_out = ra_val * rb_val;
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      // todo - handle OE bit
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case MULLD: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int64_t ra_val = get_register(ra);
      int64_t rb_val = get_register(rb);
      int64_t alu_out = ra_val * rb_val;
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      // todo - handle OE bit
      break;
    }
#endif
    case DIVW: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int32_t ra_val = get_register(ra);
      int32_t rb_val = get_register(rb);
      bool overflow = (ra_val == kMinInt && rb_val == -1);
      // result is undefined if divisor is zero or if operation
      // is 0x80000000 / -1.
      int32_t alu_out = (rb_val == 0 || overflow) ? -1 : ra_val / rb_val;
      set_register(rt, alu_out);
      if (instr->Bit(10)) {  // OE bit set
        if (overflow) {
          special_reg_xer_ |= 0xC0000000;  // set SO,OV
        } else {
          special_reg_xer_ &= ~0x40000000;  // clear OV
        }
      }
      if (instr->Bit(0)) {  // RC bit set
        bool setSO = (special_reg_xer_ & 0x80000000);
        SetCR0(alu_out, setSO);
      }
      break;
    }
    case DIVWU: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uint32_t ra_val = get_register(ra);
      uint32_t rb_val = get_register(rb);
      bool overflow = (rb_val == 0);
      // result is undefined if divisor is zero
      uint32_t alu_out = (overflow) ? -1 : ra_val / rb_val;
      set_register(rt, alu_out);
      if (instr->Bit(10)) {  // OE bit set
        if (overflow) {
          special_reg_xer_ |= 0xC0000000;  // set SO,OV
        } else {
          special_reg_xer_ &= ~0x40000000;  // clear OV
        }
      }
      if (instr->Bit(0)) {  // RC bit set
        bool setSO = (special_reg_xer_ & 0x80000000);
        SetCR0(alu_out, setSO);
      }
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case DIVD: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int64_t ra_val = get_register(ra);
      int64_t rb_val = get_register(rb);
      int64_t one = 1;  // work-around gcc
      int64_t kMinLongLong = (one << 63);
      // result is undefined if divisor is zero or if operation
      // is 0x80000000_00000000 / -1.
      int64_t alu_out =
          (rb_val == 0 || (ra_val == kMinLongLong && rb_val == -1))
              ? -1
              : ra_val / rb_val;
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      // todo - handle OE bit
      break;
    }
    case DIVDU: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      uint64_t ra_val = get_register(ra);
      uint64_t rb_val = get_register(rb);
      // result is undefined if divisor is zero
      uint64_t alu_out = (rb_val == 0) ? -1 : ra_val / rb_val;
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      // todo - handle OE bit
      break;
    }
#endif
    case ADDX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      // int oe = instr->Bit(10);
      intptr_t ra_val = get_register(ra);
      intptr_t rb_val = get_register(rb);
      intptr_t alu_out = ra_val + rb_val;
      set_register(rt, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      // todo - handle OE bit
      break;
    }
    case XORX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      intptr_t alu_out = rs_val ^ rb_val;
      set_register(ra, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      break;
    }
    case ORX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      intptr_t alu_out = rs_val | rb_val;
      set_register(ra, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      break;
    }
    case ORC: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      intptr_t alu_out = rs_val | ~rb_val;
      set_register(ra, alu_out);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(alu_out);
      }
      break;
    }
    case MFSPR: {
      int rt = instr->RTValue();
      int spr = instr->Bits(20, 11);
      if (spr != 256) {
        UNIMPLEMENTED();  // Only LRLR supported
      }
      set_register(rt, special_reg_lr_);
      break;
    }
    case MTSPR: {
      int rt = instr->RTValue();
      intptr_t rt_val = get_register(rt);
      int spr = instr->Bits(20, 11);
      if (spr == 256) {
        special_reg_lr_ = rt_val;
      } else if (spr == 288) {
        special_reg_ctr_ = rt_val;
      } else if (spr == 32) {
        special_reg_xer_ = rt_val;
      } else {
        UNIMPLEMENTED();  // Only LR supported
      }
      break;
    }
    case MFCR: {
      int rt = instr->RTValue();
      set_register(rt, condition_reg_);
      break;
    }
    case STWUX:
    case STWX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int32_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      WriteW(ra_val + rb_val, rs_val, instr);
      if (opcode == STWUX) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
    case STBUX:
    case STBX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int8_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      WriteB(ra_val + rb_val, rs_val);
      if (opcode == STBUX) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
    case STHUX:
    case STHX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int16_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      WriteH(ra_val + rb_val, rs_val, instr);
      if (opcode == STHUX) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
    case LWZX:
    case LWZUX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      set_register(rt, ReadWU(ra_val + rb_val, instr));
      if (opcode == LWZUX) {
        DCHECK(ra != 0 && ra != rt);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case LWAX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      set_register(rt, ReadW(ra_val + rb_val, instr));
      break;
    }
    case LDX:
    case LDUX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      intptr_t* result = ReadDW(ra_val + rb_val);
      set_register(rt, *result);
      if (opcode == LDUX) {
        DCHECK(ra != 0 && ra != rt);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
    case STDX:
    case STDUX: {
      int rs = instr->RSValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rs_val = get_register(rs);
      intptr_t rb_val = get_register(rb);
      WriteDW(ra_val + rb_val, rs_val);
      if (opcode == STDUX) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
#endif
    case LBZX:
    case LBZUX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      set_register(rt, ReadBU(ra_val + rb_val) & 0xFF);
      if (opcode == LBZUX) {
        DCHECK(ra != 0 && ra != rt);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
    case LHZX:
    case LHZUX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      set_register(rt, ReadHU(ra_val + rb_val, instr) & 0xFFFF);
      if (opcode == LHZUX) {
        DCHECK(ra != 0 && ra != rt);
        set_register(ra, ra_val + rb_val);
      }
      break;
    }
    case LHAX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      set_register(rt, ReadH(ra_val + rb_val, instr));
      break;
    }
    case LBARX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      set_register(rt, ReadExBU(ra_val + rb_val) & 0xFF);
      break;
    }
    case LHARX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      set_register(rt, ReadExHU(ra_val + rb_val, instr));
      break;
    }
    case LWARX: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      set_register(rt, ReadExWU(ra_val + rb_val, instr));
      break;
    }
    case DCBF: {
      // todo - simulate dcbf
      break;
    }
    case ISEL: {
      int rt = instr->RTValue();
      int ra = instr->RAValue();
      int rb = instr->RBValue();
      int condition_bit = instr->RCValue();
      int condition_mask = 0x80000000 >> condition_bit;
      intptr_t ra_val = (ra == 0) ? 0 : get_register(ra);
      intptr_t rb_val = get_register(rb);
      intptr_t value = (condition_reg_ & condition_mask) ? ra_val : rb_val;
      set_register(rt, value);
      break;
    }

    case STBU:
    case STB: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int8_t rs_val = get_register(rs);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      WriteB(ra_val + offset, rs_val);
      if (opcode == STBU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case LHZU:
    case LHZ: {
      int ra = instr->RAValue();
      int rt = instr->RTValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      uintptr_t result = ReadHU(ra_val + offset, instr) & 0xFFFF;
      set_register(rt, result);
      if (opcode == LHZU) {
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case LHA:
    case LHAU: {
      int ra = instr->RAValue();
      int rt = instr->RTValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      intptr_t result = ReadH(ra_val + offset, instr);
      set_register(rt, result);
      if (opcode == LHAU) {
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case STHU:
    case STH: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int16_t rs_val = get_register(rs);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      WriteH(ra_val + offset, rs_val, instr);
      if (opcode == STHU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case LMW:
    case STMW: {
      UNIMPLEMENTED();
      break;
    }

    case LFSU:
    case LFS: {
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      int32_t offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int32_t val = ReadW(ra_val + offset, instr);
      float* fptr = reinterpret_cast<float*>(&val);
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
      // Conversion using double changes sNan to qNan on ia32/x64
      if ((val & 0x7F800000) == 0x7F800000) {
        int64_t dval = static_cast<int64_t>(val);
        dval = ((dval & 0xC0000000) << 32) | ((dval & 0x40000000) << 31) |
               ((dval & 0x40000000) << 30) | ((dval & 0x7FFFFFFF) << 29) | 0x0;
        set_d_register(frt, dval);
      } else {
        set_d_register_from_double(frt, static_cast<double>(*fptr));
      }
#else
      set_d_register_from_double(frt, static_cast<double>(*fptr));
#endif
      if (opcode == LFSU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case LFDU:
    case LFD: {
      int frt = instr->RTValue();
      int ra = instr->RAValue();
      int32_t offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int64_t* dptr = reinterpret_cast<int64_t*>(ReadDW(ra_val + offset));
      set_d_register(frt, *dptr);
      if (opcode == LFDU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case STFSU: {
      case STFS:
        int frs = instr->RSValue();
        int ra = instr->RAValue();
        int32_t offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
        intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
        float frs_val = static_cast<float>(get_double_from_d_register(frs));
        int32_t* p;
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
        // Conversion using double changes sNan to qNan on ia32/x64
        int32_t sval = 0;
        int64_t dval = get_d_register(frs);
        if ((dval & 0x7FF0000000000000) == 0x7FF0000000000000) {
          sval = ((dval & 0xC000000000000000) >> 32) |
                 ((dval & 0x07FFFFFFE0000000) >> 29);
          p = &sval;
        } else {
          p = reinterpret_cast<int32_t*>(&frs_val);
        }
#else
        p = reinterpret_cast<int32_t*>(&frs_val);
#endif
        WriteW(ra_val + offset, *p, instr);
        if (opcode == STFSU) {
          DCHECK_NE(ra, 0);
          set_register(ra, ra_val + offset);
        }
        break;
    }

    case STFDU:
    case STFD: {
      int frs = instr->RSValue();
      int ra = instr->RAValue();
      int32_t offset = SIGN_EXT_IMM16(instr->Bits(15, 0));
      intptr_t ra_val = ra == 0 ? 0 : get_register(ra);
      int64_t frs_val = get_d_register(frs);
      WriteDW(ra_val + offset, frs_val);
      if (opcode == STFDU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }

    case FCFIDS: {
      // fcfids
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      int64_t frb_val = get_d_register(frb);
      double frt_val = static_cast<float>(frb_val);
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FCFIDUS: {
      // fcfidus
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      uint64_t frb_val = get_d_register(frb);
      double frt_val = static_cast<float>(frb_val);
      set_d_register_from_double(frt, frt_val);
      return;
    }

    case FDIV: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frt_val = fra_val / frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FSUB: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frt_val = fra_val - frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FADD: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frt_val = fra_val + frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FSQRT: {
      lazily_initialize_fast_sqrt(isolate_);
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      double frt_val = fast_sqrt(frb_val, isolate_);
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FSEL: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      int frc = instr->RCValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frc_val = get_double_from_d_register(frc);
      double frt_val = ((fra_val >= 0.0) ? frc_val : frb_val);
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FMUL: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frc = instr->RCValue();
      double fra_val = get_double_from_d_register(fra);
      double frc_val = get_double_from_d_register(frc);
      double frt_val = fra_val * frc_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FMSUB: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      int frc = instr->RCValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frc_val = get_double_from_d_register(frc);
      double frt_val = (fra_val * frc_val) - frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FMADD: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      int frc = instr->RCValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frc_val = get_double_from_d_register(frc);
      double frt_val = (fra_val * frc_val) + frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FCMPU: {
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      int cr = instr->Bits(25, 23);
      int bf = 0;
      if (fra_val < frb_val) {
        bf |= 0x80000000;
      }
      if (fra_val > frb_val) {
        bf |= 0x40000000;
      }
      if (fra_val == frb_val) {
        bf |= 0x20000000;
      }
      if (std::isunordered(fra_val, frb_val)) {
        bf |= 0x10000000;
      }
      int condition_mask = 0xF0000000 >> (cr * 4);
      int condition = bf >> (cr * 4);
      condition_reg_ = (condition_reg_ & ~condition_mask) | condition;
      return;
    }
    case FRIN: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      double frt_val = std::round(frb_val);
      set_d_register_from_double(frt, frt_val);
      if (instr->Bit(0)) {  // RC bit set
                            //  UNIMPLEMENTED();
      }
      return;
    }
    case FRIZ: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      double frt_val = std::trunc(frb_val);
      set_d_register_from_double(frt, frt_val);
      if (instr->Bit(0)) {  // RC bit set
                            //  UNIMPLEMENTED();
      }
      return;
    }
    case FRIP: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      double frt_val = std::ceil(frb_val);
      set_d_register_from_double(frt, frt_val);
      if (instr->Bit(0)) {  // RC bit set
                            //  UNIMPLEMENTED();
      }
      return;
    }
    case FRIM: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      double frt_val = std::floor(frb_val);
      set_d_register_from_double(frt, frt_val);
      if (instr->Bit(0)) {  // RC bit set
                            //  UNIMPLEMENTED();
      }
      return;
    }
    case FRSP: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      // frsp round 8-byte double-precision value to
      // single-precision value
      double frb_val = get_double_from_d_register(frb);
      double frt_val = static_cast<float>(frb_val);
      set_d_register_from_double(frt, frt_val);
      if (instr->Bit(0)) {  // RC bit set
                            //  UNIMPLEMENTED();
      }
      return;
    }
    case FCFID: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      int64_t frb_val = get_d_register(frb);
      double frt_val = static_cast<double>(frb_val);
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FCFIDU: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      uint64_t frb_val = get_d_register(frb);
      double frt_val = static_cast<double>(frb_val);
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FCTID:
    case FCTIDZ: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      int mode = (opcode == FCTIDZ) ? kRoundToZero
                                    : (fp_condition_reg_ & kFPRoundingModeMask);
      int64_t frt_val;
      int64_t one = 1;  // work-around gcc
      int64_t kMinVal = (one << 63);
      int64_t kMaxVal = kMinVal - 1;
      bool invalid_convert = false;

      if (std::isnan(frb_val)) {
        frt_val = kMinVal;
        invalid_convert = true;
      } else {
        switch (mode) {
          case kRoundToZero:
            frb_val = std::trunc(frb_val);
            break;
          case kRoundToPlusInf:
            frb_val = std::ceil(frb_val);
            break;
          case kRoundToMinusInf:
            frb_val = std::floor(frb_val);
            break;
          default:
            UNIMPLEMENTED();  // Not used by V8.
            break;
        }
        if (frb_val < static_cast<double>(kMinVal)) {
          frt_val = kMinVal;
          invalid_convert = true;
        } else if (frb_val >= static_cast<double>(kMaxVal)) {
          frt_val = kMaxVal;
          invalid_convert = true;
        } else {
          frt_val = (int64_t)frb_val;
        }
      }
      set_d_register(frt, frt_val);
      if (invalid_convert) SetFPSCR(VXCVI);
      return;
    }
    case FCTIDU:
    case FCTIDUZ: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      int mode = (opcode == FCTIDUZ)
                     ? kRoundToZero
                     : (fp_condition_reg_ & kFPRoundingModeMask);
      uint64_t frt_val;
      uint64_t kMinVal = 0;
      uint64_t kMaxVal = kMinVal - 1;
      bool invalid_convert = false;

      if (std::isnan(frb_val)) {
        frt_val = kMinVal;
        invalid_convert = true;
      } else {
        switch (mode) {
          case kRoundToZero:
            frb_val = std::trunc(frb_val);
            break;
          case kRoundToPlusInf:
            frb_val = std::ceil(frb_val);
            break;
          case kRoundToMinusInf:
            frb_val = std::floor(frb_val);
            break;
          default:
            UNIMPLEMENTED();  // Not used by V8.
            break;
        }
        if (frb_val < static_cast<double>(kMinVal)) {
          frt_val = kMinVal;
          invalid_convert = true;
        } else if (frb_val >= static_cast<double>(kMaxVal)) {
          frt_val = kMaxVal;
          invalid_convert = true;
        } else {
          frt_val = (uint64_t)frb_val;
        }
      }
      set_d_register(frt, frt_val);
      if (invalid_convert) SetFPSCR(VXCVI);
      return;
    }
    case FCTIW:
    case FCTIWZ: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      int mode = (opcode == FCTIWZ) ? kRoundToZero
                                    : (fp_condition_reg_ & kFPRoundingModeMask);
      int64_t frt_val;
      int64_t kMinVal = kMinInt;
      int64_t kMaxVal = kMaxInt;

      if (std::isnan(frb_val)) {
        frt_val = kMinVal;
      } else {
        switch (mode) {
          case kRoundToZero:
            frb_val = std::trunc(frb_val);
            break;
          case kRoundToPlusInf:
            frb_val = std::ceil(frb_val);
            break;
          case kRoundToMinusInf:
            frb_val = std::floor(frb_val);
            break;
          case kRoundToNearest: {
            double orig = frb_val;
            frb_val = lround(frb_val);
            // Round to even if exactly halfway.  (lround rounds up)
            if (std::fabs(frb_val - orig) == 0.5 && ((int64_t)frb_val % 2)) {
              frb_val += ((frb_val > 0) ? -1.0 : 1.0);
            }
            break;
          }
          default:
            UNIMPLEMENTED();  // Not used by V8.
            break;
        }
        if (frb_val < kMinVal) {
          frt_val = kMinVal;
        } else if (frb_val > kMaxVal) {
          frt_val = kMaxVal;
        } else {
          frt_val = (int64_t)frb_val;
        }
      }
      set_d_register(frt, frt_val);
      return;
    }
    case FNEG: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      double frt_val = -frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case FMR: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      int64_t frb_val = get_d_register(frb);
      set_d_register(frt, frb_val);
      return;
    }
    case MTFSFI: {
      int bf = instr->Bits(25, 23);
      int imm = instr->Bits(15, 12);
      int fp_condition_mask = 0xF0000000 >> (bf * 4);
      fp_condition_reg_ &= ~fp_condition_mask;
      fp_condition_reg_ |= (imm << (28 - (bf * 4)));
      if (instr->Bit(0)) {  // RC bit set
        condition_reg_ &= 0xF0FFFFFF;
        condition_reg_ |= (imm << 23);
      }
      return;
    }
    case MTFSF: {
      int frb = instr->RBValue();
      int64_t frb_dval = get_d_register(frb);
      int32_t frb_ival = static_cast<int32_t>((frb_dval)&0xFFFFFFFF);
      int l = instr->Bits(25, 25);
      if (l == 1) {
        fp_condition_reg_ = frb_ival;
      } else {
        UNIMPLEMENTED();
      }
      if (instr->Bit(0)) {  // RC bit set
        UNIMPLEMENTED();
        // int w = instr->Bits(16, 16);
        // int flm = instr->Bits(24, 17);
      }
      return;
    }
    case MFFS: {
      int frt = instr->RTValue();
      int64_t lval = static_cast<int64_t>(fp_condition_reg_);
      set_d_register(frt, lval);
      return;
    }
    case MCRFS: {
      int bf = instr->Bits(25, 23);
      int bfa = instr->Bits(20, 18);
      int cr_shift = (7 - bf) * CRWIDTH;
      int fp_shift = (7 - bfa) * CRWIDTH;
      int field_val = (fp_condition_reg_ >> fp_shift) & 0xF;
      condition_reg_ &= ~(0x0F << cr_shift);
      condition_reg_ |= (field_val << cr_shift);
      // Clear copied exception bits
      switch (bfa) {
        case 5:
          ClearFPSCR(VXSOFT);
          ClearFPSCR(VXSQRT);
          ClearFPSCR(VXCVI);
          break;
        default:
          UNIMPLEMENTED();
          break;
      }
      return;
    }
    case MTFSB0: {
      int bt = instr->Bits(25, 21);
      ClearFPSCR(bt);
      if (instr->Bit(0)) {  // RC bit set
        UNIMPLEMENTED();
      }
      return;
    }
    case MTFSB1: {
      int bt = instr->Bits(25, 21);
      SetFPSCR(bt);
      if (instr->Bit(0)) {  // RC bit set
        UNIMPLEMENTED();
      }
      return;
    }
    case FABS: {
      int frt = instr->RTValue();
      int frb = instr->RBValue();
      double frb_val = get_double_from_d_register(frb);
      double frt_val = std::fabs(frb_val);
      set_d_register_from_double(frt, frt_val);
      return;
    }


#if V8_TARGET_ARCH_PPC64
    case RLDICL: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      uintptr_t rs_val = get_register(rs);
      int sh = (instr->Bits(15, 11) | (instr->Bit(1) << 5));
      int mb = (instr->Bits(10, 6) | (instr->Bit(5) << 5));
      DCHECK(sh >= 0 && sh <= 63);
      DCHECK(mb >= 0 && mb <= 63);
      uintptr_t result = base::bits::RotateLeft64(rs_val, sh);
      uintptr_t mask = 0xFFFFFFFFFFFFFFFF >> mb;
      result &= mask;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      return;
    }
    case RLDICR: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      uintptr_t rs_val = get_register(rs);
      int sh = (instr->Bits(15, 11) | (instr->Bit(1) << 5));
      int me = (instr->Bits(10, 6) | (instr->Bit(5) << 5));
      DCHECK(sh >= 0 && sh <= 63);
      DCHECK(me >= 0 && me <= 63);
      uintptr_t result = base::bits::RotateLeft64(rs_val, sh);
      uintptr_t mask = 0xFFFFFFFFFFFFFFFF << (63 - me);
      result &= mask;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      return;
    }
    case RLDIC: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      uintptr_t rs_val = get_register(rs);
      int sh = (instr->Bits(15, 11) | (instr->Bit(1) << 5));
      int mb = (instr->Bits(10, 6) | (instr->Bit(5) << 5));
      DCHECK(sh >= 0 && sh <= 63);
      DCHECK(mb >= 0 && mb <= 63);
      uintptr_t result = base::bits::RotateLeft64(rs_val, sh);
      uintptr_t mask = (0xFFFFFFFFFFFFFFFF >> mb) & (0xFFFFFFFFFFFFFFFF << sh);
      result &= mask;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      return;
    }
    case RLDIMI: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      uintptr_t rs_val = get_register(rs);
      intptr_t ra_val = get_register(ra);
      int sh = (instr->Bits(15, 11) | (instr->Bit(1) << 5));
      int mb = (instr->Bits(10, 6) | (instr->Bit(5) << 5));
      int me = 63 - sh;
      uintptr_t result = base::bits::RotateLeft64(rs_val, sh);
      uintptr_t mask = 0;
      if (mb < me + 1) {
        uintptr_t bit = 0x8000000000000000 >> mb;
        for (; mb <= me; mb++) {
          mask |= bit;
          bit >>= 1;
        }
      } else if (mb == me + 1) {
        mask = 0xFFFFFFFFFFFFFFFF;
      } else {                                           // mb > me+1
        uintptr_t bit = 0x8000000000000000 >> (me + 1);  // needs to be tested
        mask = 0xFFFFFFFFFFFFFFFF;
        for (; me < mb; me++) {
          mask ^= bit;
          bit >>= 1;
        }
      }
      result &= mask;
      ra_val &= ~mask;
      result |= ra_val;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      return;
    }
    case RLDCL: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      int rb = instr->RBValue();
      uintptr_t rs_val = get_register(rs);
      uintptr_t rb_val = get_register(rb);
      int sh = (rb_val & 0x3F);
      int mb = (instr->Bits(10, 6) | (instr->Bit(5) << 5));
      DCHECK(sh >= 0 && sh <= 63);
      DCHECK(mb >= 0 && mb <= 63);
      uintptr_t result = base::bits::RotateLeft64(rs_val, sh);
      uintptr_t mask = 0xFFFFFFFFFFFFFFFF >> mb;
      result &= mask;
      set_register(ra, result);
      if (instr->Bit(0)) {  // RC bit set
        SetCR0(result);
      }
      return;
    }

    case LD:
    case LDU:
    case LWA: {
      int ra = instr->RAValue();
      int rt = instr->RTValue();
      int64_t ra_val = ra == 0 ? 0 : get_register(ra);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0) & ~3);
      switch (instr->Bits(1, 0)) {
        case 0: {  // ld
          intptr_t* result = ReadDW(ra_val + offset);
          set_register(rt, *result);
          break;
        }
        case 1: {  // ldu
          intptr_t* result = ReadDW(ra_val + offset);
          set_register(rt, *result);
          DCHECK_NE(ra, 0);
          set_register(ra, ra_val + offset);
          break;
        }
        case 2: {  // lwa
          intptr_t result = ReadW(ra_val + offset, instr);
          set_register(rt, result);
          break;
        }
      }
      break;
    }

    case STD:
    case STDU: {
      int ra = instr->RAValue();
      int rs = instr->RSValue();
      int64_t ra_val = ra == 0 ? 0 : get_register(ra);
      int64_t rs_val = get_register(rs);
      int offset = SIGN_EXT_IMM16(instr->Bits(15, 0) & ~3);
      WriteDW(ra_val + offset, rs_val);
      if (opcode == STDU) {
        DCHECK_NE(ra, 0);
        set_register(ra, ra_val + offset);
      }
      break;
    }
#endif

    case XSADDDP: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frt_val = fra_val + frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case XSSUBDP: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frt_val = fra_val - frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case XSMULDP: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frt_val = fra_val * frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }
    case XSDIVDP: {
      int frt = instr->RTValue();
      int fra = instr->RAValue();
      int frb = instr->RBValue();
      double fra_val = get_double_from_d_register(fra);
      double frb_val = get_double_from_d_register(frb);
      double frt_val = fra_val / frb_val;
      set_d_register_from_double(frt, frt_val);
      return;
    }

    default: {
      UNIMPLEMENTED();
      break;
    }
  }
}  // NOLINT


void Simulator::Trace(Instruction* instr) {
  disasm::NameConverter converter;
  disasm::Disassembler dasm(converter);
  // use a reasonably large buffer
  v8::internal::EmbeddedVector<char, 256> buffer;
  dasm.InstructionDecode(buffer, reinterpret_cast<byte*>(instr));
  PrintF("%05d  %08" V8PRIxPTR "  %s\n", icount_,
         reinterpret_cast<intptr_t>(instr), buffer.start());
}


// Executes the current instruction.
void Simulator::ExecuteInstruction(Instruction* instr) {
  if (v8::internal::FLAG_check_icache) {
    CheckICache(isolate_->simulator_i_cache(), instr);
  }
  pc_modified_ = false;
  if (::v8::internal::FLAG_trace_sim) {
    Trace(instr);
  }
  uint32_t opcode = instr->OpcodeField();
  if (opcode == TWI) {
    SoftwareInterrupt(instr);
  } else {
    ExecuteGeneric(instr);
  }
  if (!pc_modified_) {
    set_pc(reinterpret_cast<intptr_t>(instr) + Instruction::kInstrSize);
  }
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
    // we reach the particular instruction count.
    while (program_counter != end_sim_pc) {
      Instruction* instr = reinterpret_cast<Instruction*>(program_counter);
      icount_++;
      if (icount_ == ::v8::internal::FLAG_stop_sim_at) {
        PPCDebugger dbg(this);
        dbg.Debug();
      } else {
        ExecuteInstruction(instr);
      }
      program_counter = get_pc();
    }
  }
}


void Simulator::CallInternal(byte* entry) {
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

  if (ABI_CALL_VIA_IP) {
    // Put target address in ip (for JS prologue).
    set_register(r12, get_pc());
  }

  // Put down marker for end of simulation. The simulator will stop simulation
  // when the PC reaches this value. By saving the "end simulation" value into
  // the LR the simulation stops when returning to this call point.
  special_reg_lr_ = end_sim_pc;

  // Remember the values of non-volatile registers.
  intptr_t r2_val = get_register(r2);
  intptr_t r13_val = get_register(r13);
  intptr_t r14_val = get_register(r14);
  intptr_t r15_val = get_register(r15);
  intptr_t r16_val = get_register(r16);
  intptr_t r17_val = get_register(r17);
  intptr_t r18_val = get_register(r18);
  intptr_t r19_val = get_register(r19);
  intptr_t r20_val = get_register(r20);
  intptr_t r21_val = get_register(r21);
  intptr_t r22_val = get_register(r22);
  intptr_t r23_val = get_register(r23);
  intptr_t r24_val = get_register(r24);
  intptr_t r25_val = get_register(r25);
  intptr_t r26_val = get_register(r26);
  intptr_t r27_val = get_register(r27);
  intptr_t r28_val = get_register(r28);
  intptr_t r29_val = get_register(r29);
  intptr_t r30_val = get_register(r30);
  intptr_t r31_val = get_register(fp);

  // Set up the non-volatile registers with a known value. To be able to check
  // that they are preserved properly across JS execution.
  intptr_t callee_saved_value = icount_;
  set_register(r2, callee_saved_value);
  set_register(r13, callee_saved_value);
  set_register(r14, callee_saved_value);
  set_register(r15, callee_saved_value);
  set_register(r16, callee_saved_value);
  set_register(r17, callee_saved_value);
  set_register(r18, callee_saved_value);
  set_register(r19, callee_saved_value);
  set_register(r20, callee_saved_value);
  set_register(r21, callee_saved_value);
  set_register(r22, callee_saved_value);
  set_register(r23, callee_saved_value);
  set_register(r24, callee_saved_value);
  set_register(r25, callee_saved_value);
  set_register(r26, callee_saved_value);
  set_register(r27, callee_saved_value);
  set_register(r28, callee_saved_value);
  set_register(r29, callee_saved_value);
  set_register(r30, callee_saved_value);
  set_register(fp, callee_saved_value);

  // Start the simulation
  Execute();

  // Check that the non-volatile registers have been preserved.
  if (ABI_TOC_REGISTER != 2) {
    CHECK_EQ(callee_saved_value, get_register(r2));
  }
  if (ABI_TOC_REGISTER != 13) {
    CHECK_EQ(callee_saved_value, get_register(r13));
  }
  CHECK_EQ(callee_saved_value, get_register(r14));
  CHECK_EQ(callee_saved_value, get_register(r15));
  CHECK_EQ(callee_saved_value, get_register(r16));
  CHECK_EQ(callee_saved_value, get_register(r17));
  CHECK_EQ(callee_saved_value, get_register(r18));
  CHECK_EQ(callee_saved_value, get_register(r19));
  CHECK_EQ(callee_saved_value, get_register(r20));
  CHECK_EQ(callee_saved_value, get_register(r21));
  CHECK_EQ(callee_saved_value, get_register(r22));
  CHECK_EQ(callee_saved_value, get_register(r23));
  CHECK_EQ(callee_saved_value, get_register(r24));
  CHECK_EQ(callee_saved_value, get_register(r25));
  CHECK_EQ(callee_saved_value, get_register(r26));
  CHECK_EQ(callee_saved_value, get_register(r27));
  CHECK_EQ(callee_saved_value, get_register(r28));
  CHECK_EQ(callee_saved_value, get_register(r29));
  CHECK_EQ(callee_saved_value, get_register(r30));
  CHECK_EQ(callee_saved_value, get_register(fp));

  // Restore non-volatile registers with the original value.
  set_register(r2, r2_val);
  set_register(r13, r13_val);
  set_register(r14, r14_val);
  set_register(r15, r15_val);
  set_register(r16, r16_val);
  set_register(r17, r17_val);
  set_register(r18, r18_val);
  set_register(r19, r19_val);
  set_register(r20, r20_val);
  set_register(r21, r21_val);
  set_register(r22, r22_val);
  set_register(r23, r23_val);
  set_register(r24, r24_val);
  set_register(r25, r25_val);
  set_register(r26, r26_val);
  set_register(r27, r27_val);
  set_register(r28, r28_val);
  set_register(r29, r29_val);
  set_register(r30, r30_val);
  set_register(fp, r31_val);
}

intptr_t Simulator::CallImpl(byte* entry, int argument_count,
                             const intptr_t* arguments) {
  // Set up arguments

  // First eight arguments passed in registers r3-r10.
  int reg_arg_count = std::min(8, argument_count);
  int stack_arg_count = argument_count - reg_arg_count;
  for (int i = 0; i < reg_arg_count; i++) {
    set_register(i + 3, arguments[i]);
  }

  // Remaining arguments passed on stack.
  intptr_t original_stack = get_register(sp);
  // Compute position of stack on entry to generated code.
  intptr_t entry_stack =
      (original_stack -
       (kNumRequiredStackFrameSlots + stack_arg_count) * sizeof(intptr_t));
  if (base::OS::ActivationFrameAlignment() != 0) {
    entry_stack &= -base::OS::ActivationFrameAlignment();
  }
  // Store remaining arguments on stack, from low to high memory.
  // +2 is a hack for the LR slot + old SP on PPC
  intptr_t* stack_argument =
      reinterpret_cast<intptr_t*>(entry_stack) + kStackFrameExtraParamSlot;
  memcpy(stack_argument, arguments + reg_arg_count,
         stack_arg_count * sizeof(*arguments));
  set_register(sp, entry_stack);

  CallInternal(entry);

  // Pop stack passed arguments.
  CHECK_EQ(entry_stack, get_register(sp));
  set_register(sp, original_stack);

  return get_register(r3);
}


void Simulator::CallFP(byte* entry, double d0, double d1) {
  set_d_register_from_double(1, d0);
  set_d_register_from_double(2, d1);
  CallInternal(entry);
}


int32_t Simulator::CallFPReturnsInt(byte* entry, double d0, double d1) {
  CallFP(entry, d0, d1);
  int32_t result = get_register(r3);
  return result;
}


double Simulator::CallFPReturnsDouble(byte* entry, double d0, double d1) {
  CallFP(entry, d0, d1);
  return get_double_from_d_register(1);
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

Simulator::LocalMonitor::LocalMonitor()
    : access_state_(MonitorAccess::Open),
      tagged_addr_(0),
      size_(TransactionSize::None) {}

void Simulator::LocalMonitor::Clear() {
  access_state_ = MonitorAccess::Open;
  tagged_addr_ = 0;
  size_ = TransactionSize::None;
}

void Simulator::LocalMonitor::NotifyLoad(int32_t addr) {
  if (access_state_ == MonitorAccess::Exclusive) {
    // A load could cause a cache eviction which will affect the monitor. As a
    // result, it's most strict to unconditionally clear the local monitor on
    // load.
    Clear();
  }
}

void Simulator::LocalMonitor::NotifyLoadExcl(int32_t addr,
                                             TransactionSize size) {
  access_state_ = MonitorAccess::Exclusive;
  tagged_addr_ = addr;
  size_ = size;
}

void Simulator::LocalMonitor::NotifyStore(int32_t addr) {
  if (access_state_ == MonitorAccess::Exclusive) {
    // A store could cause a cache eviction which will affect the
    // monitor. As a result, it's most strict to unconditionally clear the
    // local monitor on store.
    Clear();
  }
}

bool Simulator::LocalMonitor::NotifyStoreExcl(int32_t addr,
                                              TransactionSize size) {
  if (access_state_ == MonitorAccess::Exclusive) {
    if (addr == tagged_addr_ && size_ == size) {
      Clear();
      return true;
    } else {
      Clear();
      return false;
    }
  } else {
    DCHECK(access_state_ == MonitorAccess::Open);
    return false;
  }
}

Simulator::GlobalMonitor::Processor::Processor()
    : access_state_(MonitorAccess::Open),
      tagged_addr_(0),
      next_(nullptr),
      prev_(nullptr) {}

void Simulator::GlobalMonitor::Processor::Clear_Locked() {
  access_state_ = MonitorAccess::Open;
  tagged_addr_ = 0;
}
void Simulator::GlobalMonitor::Processor::NotifyLoadExcl_Locked(int32_t addr) {
  access_state_ = MonitorAccess::Exclusive;
  tagged_addr_ = addr;
}

void Simulator::GlobalMonitor::Processor::NotifyStore_Locked(
    int32_t addr, bool is_requesting_processor) {
  if (access_state_ == MonitorAccess::Exclusive) {
    // It is possible that a store caused a cache eviction,
    // which can affect the montior, so conservatively,
    // we always clear the monitor.
    Clear_Locked();
  }
}

bool Simulator::GlobalMonitor::Processor::NotifyStoreExcl_Locked(
    int32_t addr, bool is_requesting_processor) {
  if (access_state_ == MonitorAccess::Exclusive) {
    if (is_requesting_processor) {
      if (addr == tagged_addr_) {
        Clear_Locked();
        return true;
      }
    } else if (addr == tagged_addr_) {
      Clear_Locked();
      return false;
    }
  }
  return false;
}

Simulator::GlobalMonitor::GlobalMonitor() : head_(nullptr) {}

void Simulator::GlobalMonitor::NotifyLoadExcl_Locked(int32_t addr,
                                                     Processor* processor) {
  processor->NotifyLoadExcl_Locked(addr);
  PrependProcessor_Locked(processor);
}

void Simulator::GlobalMonitor::NotifyStore_Locked(int32_t addr,
                                                  Processor* processor) {
  // Notify each processor of the store operation.
  for (Processor* iter = head_; iter; iter = iter->next_) {
    bool is_requesting_processor = iter == processor;
    iter->NotifyStore_Locked(addr, is_requesting_processor);
  }
}

bool Simulator::GlobalMonitor::NotifyStoreExcl_Locked(int32_t addr,
                                                      Processor* processor) {
  DCHECK(IsProcessorInLinkedList_Locked(processor));
  if (processor->NotifyStoreExcl_Locked(addr, true)) {
    // Notify the other processors that this StoreExcl succeeded.
    for (Processor* iter = head_; iter; iter = iter->next_) {
      if (iter != processor) {
        iter->NotifyStoreExcl_Locked(addr, false);
      }
    }
    return true;
  } else {
    return false;
  }
}

bool Simulator::GlobalMonitor::IsProcessorInLinkedList_Locked(
    Processor* processor) const {
  return head_ == processor || processor->next_ || processor->prev_;
}

void Simulator::GlobalMonitor::PrependProcessor_Locked(Processor* processor) {
  if (IsProcessorInLinkedList_Locked(processor)) {
    return;
  }

  if (head_) {
    head_->prev_ = processor;
  }
  processor->prev_ = nullptr;
  processor->next_ = head_;
  head_ = processor;
}

void Simulator::GlobalMonitor::RemoveProcessor(Processor* processor) {
  base::LockGuard<base::Mutex> lock_guard(&mutex);
  if (!IsProcessorInLinkedList_Locked(processor)) {
    return;
  }

  if (processor->prev_) {
    processor->prev_->next_ = processor->next_;
  } else {
    head_ = processor->next_;
  }
  if (processor->next_) {
    processor->next_->prev_ = processor->prev_;
  }
  processor->prev_ = nullptr;
  processor->next_ = nullptr;
}

}  // namespace internal
}  // namespace v8

#endif  // USE_SIMULATOR
#endif  // V8_TARGET_ARCH_PPC
