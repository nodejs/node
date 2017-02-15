// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stdlib.h>
#include <cmath>

#if V8_TARGET_ARCH_ARM

#include "src/arm/constants-arm.h"
#include "src/arm/simulator-arm.h"
#include "src/assembler.h"
#include "src/base/bits.h"
#include "src/codegen.h"
#include "src/disasm.h"
#include "src/runtime/runtime-utils.h"

#if defined(USE_SIMULATOR)

// Only build the simulator if not compiling for real ARM hardware.
namespace v8 {
namespace internal {

// This macro provides a platform independent use of sscanf. The reason for
// SScanF not being implemented in a platform independent way through
// ::v8::internal::OS in the same way as SNPrintF is that the
// Windows C Run-Time Library does not provide vsscanf.
#define SScanF sscanf  // NOLINT

// The ArmDebugger class is used by the simulator while debugging simulated ARM
// code.
class ArmDebugger {
 public:
  explicit ArmDebugger(Simulator* sim) : sim_(sim) { }

  void Stop(Instruction* instr);
  void Debug();

 private:
  static const Instr kBreakpointInstr =
      (al | (7*B25) | (1*B24) | kBreakpoint);
  static const Instr kNopInstr = (al | (13*B21));

  Simulator* sim_;

  int32_t GetRegisterValue(int regnum);
  double GetRegisterPairDoubleValue(int regnum);
  double GetVFPDoubleRegisterValue(int regnum);
  bool GetValue(const char* desc, int32_t* value);
  bool GetVFPSingleValue(const char* desc, float* value);
  bool GetVFPDoubleValue(const char* desc, double* value);

  // Set or delete a breakpoint. Returns true if successful.
  bool SetBreakpoint(Instruction* breakpc);
  bool DeleteBreakpoint(Instruction* breakpc);

  // Undo and redo all breakpoints. This is needed to bracket disassembly and
  // execution to skip past breakpoints when run from the debugger.
  void UndoBreakpoints();
  void RedoBreakpoints();
};

void ArmDebugger::Stop(Instruction* instr) {
  // Get the stop code.
  uint32_t code = instr->SvcValue() & kStopCodeMask;
  // Print the stop message and code if it is not the default code.
  if (code != kMaxStopCode) {
    PrintF("Simulator hit stop %u\n", code);
  } else {
    PrintF("Simulator hit\n");
  }
  sim_->set_pc(sim_->get_pc() + 2 * Instruction::kInstrSize);
  Debug();
}

int32_t ArmDebugger::GetRegisterValue(int regnum) {
  if (regnum == kPCRegister) {
    return sim_->get_pc();
  } else {
    return sim_->get_register(regnum);
  }
}

double ArmDebugger::GetRegisterPairDoubleValue(int regnum) {
  return sim_->get_double_from_register_pair(regnum);
}


double ArmDebugger::GetVFPDoubleRegisterValue(int regnum) {
  return sim_->get_double_from_d_register(regnum);
}


bool ArmDebugger::GetValue(const char* desc, int32_t* value) {
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


bool ArmDebugger::GetVFPSingleValue(const char* desc, float* value) {
  bool is_double;
  int regnum = VFPRegisters::Number(desc, &is_double);
  if (regnum != kNoRegister && !is_double) {
    *value = sim_->get_float_from_s_register(regnum);
    return true;
  }
  return false;
}


bool ArmDebugger::GetVFPDoubleValue(const char* desc, double* value) {
  bool is_double;
  int regnum = VFPRegisters::Number(desc, &is_double);
  if (regnum != kNoRegister && is_double) {
    *value = sim_->get_double_from_d_register(regnum);
    return true;
  }
  return false;
}


bool ArmDebugger::SetBreakpoint(Instruction* breakpc) {
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


bool ArmDebugger::DeleteBreakpoint(Instruction* breakpc) {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }

  sim_->break_pc_ = NULL;
  sim_->break_instr_ = 0;
  return true;
}


void ArmDebugger::UndoBreakpoints() {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(sim_->break_instr_);
  }
}


void ArmDebugger::RedoBreakpoints() {
  if (sim_->break_pc_ != NULL) {
    sim_->break_pc_->SetInstructionBits(kBreakpointInstr);
  }
}


void ArmDebugger::Debug() {
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

  while (!done && !sim_->has_bad_pc()) {
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
        sim_->InstructionDecode(reinterpret_cast<Instruction*>(sim_->get_pc()));
      } else if ((strcmp(cmd, "c") == 0) || (strcmp(cmd, "cont") == 0)) {
        // Execute the one instruction we broke at with breakpoints disabled.
        sim_->InstructionDecode(reinterpret_cast<Instruction*>(sim_->get_pc()));
        // Leave the debugger shell.
        done = true;
      } else if ((strcmp(cmd, "p") == 0) || (strcmp(cmd, "print") == 0)) {
        if (argc == 2 || (argc == 3 && strcmp(arg2, "fp") == 0)) {
          int32_t value;
          float svalue;
          double dvalue;
          if (strcmp(arg1, "all") == 0) {
            for (int i = 0; i < kNumRegisters; i++) {
              value = GetRegisterValue(i);
              PrintF(
                  "%3s: 0x%08x %10d",
                  RegisterConfiguration::Crankshaft()->GetGeneralRegisterName(
                      i),
                  value, value);
              if ((argc == 3 && strcmp(arg2, "fp") == 0) &&
                  i < 8 &&
                  (i % 2) == 0) {
                dvalue = GetRegisterPairDoubleValue(i);
                PrintF(" (%f)\n", dvalue);
              } else {
                PrintF("\n");
              }
            }
            for (int i = 0; i < DwVfpRegister::NumRegisters(); i++) {
              dvalue = GetVFPDoubleRegisterValue(i);
              uint64_t as_words = bit_cast<uint64_t>(dvalue);
              PrintF("%3s: %f 0x%08x %08x\n",
                     VFPRegisters::Name(i, true),
                     dvalue,
                     static_cast<uint32_t>(as_words >> 32),
                     static_cast<uint32_t>(as_words & 0xffffffff));
            }
          } else {
            if (GetValue(arg1, &value)) {
              PrintF("%s: 0x%08x %d \n", arg1, value, value);
            } else if (GetVFPSingleValue(arg1, &svalue)) {
              uint32_t as_word = bit_cast<uint32_t>(svalue);
              PrintF("%s: %f 0x%08x\n", arg1, svalue, as_word);
            } else if (GetVFPDoubleValue(arg1, &dvalue)) {
              uint64_t as_words = bit_cast<uint64_t>(dvalue);
              PrintF("%s: %f 0x%08x %08x\n",
                     arg1,
                     dvalue,
                     static_cast<uint32_t>(as_words >> 32),
                     static_cast<uint32_t>(as_words & 0xffffffff));
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
        } else {
          if (!GetValue(argv[next_arg], &words)) {
            words = 10;
          }
        }
        end = cur + words;

        while (cur < end) {
          PrintF("  0x%08" V8PRIxPTR ":  0x%08x %10d",
                 reinterpret_cast<intptr_t>(cur), *cur, *cur);
          HeapObject* obj = reinterpret_cast<HeapObject*>(*cur);
          int value = *cur;
          Heap* current_heap = sim_->isolate_->heap();
          if (((value & 1) == 0) ||
              current_heap->ContainsSlow(obj->address())) {
            PrintF(" (");
            if ((value & 1) == 0) {
              PrintF("smi %d", value / 2);
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

        byte* prev = NULL;
        byte* cur = NULL;
        byte* end = NULL;

        if (argc == 1) {
          cur = reinterpret_cast<byte*>(sim_->get_pc());
          end = cur + (10 * Instruction::kInstrSize);
        } else if (argc == 2) {
          int regnum = Registers::Number(arg1);
          if (regnum != kNoRegister || strncmp(arg1, "0x", 2) == 0) {
            // The argument is an address or a register name.
            int32_t value;
            if (GetValue(arg1, &value)) {
              cur = reinterpret_cast<byte*>(value);
              // Disassemble 10 instructions at <arg1>.
              end = cur + (10 * Instruction::kInstrSize);
            }
          } else {
            // The argument is the number of instructions.
            int32_t value;
            if (GetValue(arg1, &value)) {
              cur = reinterpret_cast<byte*>(sim_->get_pc());
              // Disassemble <arg1> instructions.
              end = cur + (value * Instruction::kInstrSize);
            }
          }
        } else {
          int32_t value1;
          int32_t value2;
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
        PrintF("N flag: %d; ", sim_->n_flag_);
        PrintF("Z flag: %d; ", sim_->z_flag_);
        PrintF("C flag: %d; ", sim_->c_flag_);
        PrintF("V flag: %d\n", sim_->v_flag_);
        PrintF("INVALID OP flag: %d; ", sim_->inv_op_vfp_flag_);
        PrintF("DIV BY ZERO flag: %d; ", sim_->div_zero_vfp_flag_);
        PrintF("OVERFLOW flag: %d; ", sim_->overflow_vfp_flag_);
        PrintF("UNDERFLOW flag: %d; ", sim_->underflow_vfp_flag_);
        PrintF("INEXACT flag: %d;\n", sim_->inexact_vfp_flag_);
      } else if (strcmp(cmd, "stop") == 0) {
        int32_t value;
        intptr_t stop_pc = sim_->get_pc() - 2 * Instruction::kInstrSize;
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
        PrintF("stepi\n");
        PrintF("  step one instruction (alias 'si')\n");
        PrintF("print <register>\n");
        PrintF("  print register content (alias 'p')\n");
        PrintF("  use register name 'all' to print all registers\n");
        PrintF("  add argument 'fp' to print register pair double values\n");
        PrintF("printobject <register>\n");
        PrintF("  print an object from a register (alias 'po')\n");
        PrintF("flags\n");
        PrintF("  print flags\n");
        PrintF("stack [<words>]\n");
        PrintF("  dump stack content, default dump 10 words)\n");
        PrintF("mem <address> [<words>]\n");
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
        PrintF("    stop and and give control to the ArmDebugger.\n");
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
    DCHECK_EQ(0, start & CachePage::kPageMask);
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
                             intptr_t start, int size) {
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
  size_t stack_size = 1 * 1024*1024;  // allocate 1MB for stack
  stack_ = reinterpret_cast<char*>(malloc(stack_size));
  pc_modified_ = false;
  icount_ = 0;
  break_pc_ = NULL;
  break_instr_ = 0;

  // Set up architecture state.
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
  for (int i = 0; i < num_d_registers * 2; i++) {
    vfp_registers_[i] = 0;
  }
  n_flag_FPSCR_ = false;
  z_flag_FPSCR_ = false;
  c_flag_FPSCR_ = false;
  v_flag_FPSCR_ = false;
  FPSCR_rounding_mode_ = RN;
  FPSCR_default_NaN_mode_ = false;

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
        swi_instruction_(al | (0xf * B24) | kCallRtRedirected),
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

  static void* ReverseRedirection(int32_t reg) {
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
void Simulator::set_register(int reg, int32_t value) {
  DCHECK((reg >= 0) && (reg < num_registers));
  if (reg == pc) {
    pc_modified_ = true;
  }
  registers_[reg] = value;
}


// Get the register from the architecture state. This function does handle
// the special case of accessing the PC register.
int32_t Simulator::get_register(int reg) const {
  DCHECK((reg >= 0) && (reg < num_registers));
  // Stupid code added to avoid bug in GCC.
  // See: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949
  if (reg >= num_registers) return 0;
  // End stupid code.
  return registers_[reg] + ((reg == pc) ? Instruction::kPCReadOffset : 0);
}


double Simulator::get_double_from_register_pair(int reg) {
  DCHECK((reg >= 0) && (reg < num_registers) && ((reg % 2) == 0));

  double dm_val = 0.0;
  // Read the bits from the unsigned integer register_[] array
  // into the double precision floating point value and return it.
  char buffer[2 * sizeof(vfp_registers_[0])];
  memcpy(buffer, &registers_[reg], 2 * sizeof(registers_[0]));
  memcpy(&dm_val, buffer, 2 * sizeof(registers_[0]));
  return(dm_val);
}


void Simulator::set_register_pair_from_double(int reg, double* value) {
  DCHECK((reg >= 0) && (reg < num_registers) && ((reg % 2) == 0));
  memcpy(registers_ + reg, value, sizeof(*value));
}


void Simulator::set_dw_register(int dreg, const int* dbl) {
  DCHECK((dreg >= 0) && (dreg < num_d_registers));
  registers_[dreg] = dbl[0];
  registers_[dreg + 1] = dbl[1];
}


void Simulator::get_d_register(int dreg, uint64_t* value) {
  DCHECK((dreg >= 0) && (dreg < DwVfpRegister::NumRegisters()));
  memcpy(value, vfp_registers_ + dreg * 2, sizeof(*value));
}


void Simulator::set_d_register(int dreg, const uint64_t* value) {
  DCHECK((dreg >= 0) && (dreg < DwVfpRegister::NumRegisters()));
  memcpy(vfp_registers_ + dreg * 2, value, sizeof(*value));
}


void Simulator::get_d_register(int dreg, uint32_t* value) {
  DCHECK((dreg >= 0) && (dreg < DwVfpRegister::NumRegisters()));
  memcpy(value, vfp_registers_ + dreg * 2, sizeof(*value) * 2);
}


void Simulator::set_d_register(int dreg, const uint32_t* value) {
  DCHECK((dreg >= 0) && (dreg < DwVfpRegister::NumRegisters()));
  memcpy(vfp_registers_ + dreg * 2, value, sizeof(*value) * 2);
}


void Simulator::get_q_register(int qreg, uint64_t* value) {
  DCHECK((qreg >= 0) && (qreg < num_q_registers));
  memcpy(value, vfp_registers_ + qreg * 4, sizeof(*value) * 2);
}


void Simulator::set_q_register(int qreg, const uint64_t* value) {
  DCHECK((qreg >= 0) && (qreg < num_q_registers));
  memcpy(vfp_registers_ + qreg * 4, value, sizeof(*value) * 2);
}


void Simulator::get_q_register(int qreg, uint32_t* value) {
  DCHECK((qreg >= 0) && (qreg < num_q_registers));
  memcpy(value, vfp_registers_ + qreg * 4, sizeof(*value) * 4);
}


void Simulator::set_q_register(int qreg, const uint32_t* value) {
  DCHECK((qreg >= 0) && (qreg < num_q_registers));
  memcpy(vfp_registers_ + qreg * 4, value, sizeof(*value) * 4);
}


// Raw access to the PC register.
void Simulator::set_pc(int32_t value) {
  pc_modified_ = true;
  registers_[pc] = value;
}


bool Simulator::has_bad_pc() const {
  return ((registers_[pc] == bad_lr) || (registers_[pc] == end_sim_pc));
}


// Raw access to the PC register without the special adjustment when reading.
int32_t Simulator::get_pc() const {
  return registers_[pc];
}


// Getting from and setting into VFP registers.
void Simulator::set_s_register(int sreg, unsigned int value) {
  DCHECK((sreg >= 0) && (sreg < num_s_registers));
  vfp_registers_[sreg] = value;
}


unsigned int Simulator::get_s_register(int sreg) const {
  DCHECK((sreg >= 0) && (sreg < num_s_registers));
  return vfp_registers_[sreg];
}


template<class InputType, int register_size>
void Simulator::SetVFPRegister(int reg_index, const InputType& value) {
  DCHECK(reg_index >= 0);
  if (register_size == 1) DCHECK(reg_index < num_s_registers);
  if (register_size == 2) DCHECK(reg_index < DwVfpRegister::NumRegisters());

  char buffer[register_size * sizeof(vfp_registers_[0])];
  memcpy(buffer, &value, register_size * sizeof(vfp_registers_[0]));
  memcpy(&vfp_registers_[reg_index * register_size], buffer,
         register_size * sizeof(vfp_registers_[0]));
}


template<class ReturnType, int register_size>
ReturnType Simulator::GetFromVFPRegister(int reg_index) {
  DCHECK(reg_index >= 0);
  if (register_size == 1) DCHECK(reg_index < num_s_registers);
  if (register_size == 2) DCHECK(reg_index < DwVfpRegister::NumRegisters());

  ReturnType value = 0;
  char buffer[register_size * sizeof(vfp_registers_[0])];
  memcpy(buffer, &vfp_registers_[register_size * reg_index],
         register_size * sizeof(vfp_registers_[0]));
  memcpy(&value, buffer, register_size * sizeof(vfp_registers_[0]));
  return value;
}

void Simulator::SetSpecialRegister(SRegisterFieldMask reg_and_mask,
                                   uint32_t value) {
  // Only CPSR_f is implemented. Of that, only N, Z, C and V are implemented.
  if ((reg_and_mask == CPSR_f) && ((value & ~kSpecialCondition) == 0)) {
    n_flag_ = ((value & (1 << 31)) != 0);
    z_flag_ = ((value & (1 << 30)) != 0);
    c_flag_ = ((value & (1 << 29)) != 0);
    v_flag_ = ((value & (1 << 28)) != 0);
  } else {
    UNIMPLEMENTED();
  }
}

uint32_t Simulator::GetFromSpecialRegister(SRegister reg) {
  uint32_t result = 0;
  // Only CPSR_f is implemented.
  if (reg == CPSR) {
    if (n_flag_) result |= (1 << 31);
    if (z_flag_) result |= (1 << 30);
    if (c_flag_) result |= (1 << 29);
    if (v_flag_) result |= (1 << 28);
  } else {
    UNIMPLEMENTED();
  }
  return result;
}

// Runtime FP routines take:
// - two double arguments
// - one double argument and zero or one integer arguments.
// All are consructed here from r0-r3 or d0, d1 and r0.
void Simulator::GetFpArgs(double* x, double* y, int32_t* z) {
  if (use_eabi_hardfloat()) {
    *x = get_double_from_d_register(0);
    *y = get_double_from_d_register(1);
    *z = get_register(0);
  } else {
    // Registers 0 and 1 -> x.
    *x = get_double_from_register_pair(0);
    // Register 2 and 3 -> y.
    *y = get_double_from_register_pair(2);
    // Register 2 -> z
    *z = get_register(2);
  }
}


// The return value is either in r0/r1 or d0.
void Simulator::SetFpResult(const double& result) {
  if (use_eabi_hardfloat()) {
    char buffer[2 * sizeof(vfp_registers_[0])];
    memcpy(buffer, &result, sizeof(buffer));
    // Copy result to d0.
    memcpy(vfp_registers_, buffer, sizeof(buffer));
  } else {
    char buffer[2 * sizeof(registers_[0])];
    memcpy(buffer, &result, sizeof(buffer));
    // Copy result to r0 and r1.
    memcpy(registers_, buffer, sizeof(buffer));
  }
}


void Simulator::TrashCallerSaveRegisters() {
  // We don't trash the registers with the return value.
  registers_[2] = 0x50Bad4U;
  registers_[3] = 0x50Bad4U;
  registers_[12] = 0x50Bad4U;
}


int Simulator::ReadW(int32_t addr, Instruction* instr) {
  // All supported ARM targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  intptr_t* ptr = reinterpret_cast<intptr_t*>(addr);
  return *ptr;
}


void Simulator::WriteW(int32_t addr, int value, Instruction* instr) {
  // All supported ARM targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  intptr_t* ptr = reinterpret_cast<intptr_t*>(addr);
  *ptr = value;
}


uint16_t Simulator::ReadHU(int32_t addr, Instruction* instr) {
  // All supported ARM targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
  return *ptr;
}


int16_t Simulator::ReadH(int32_t addr, Instruction* instr) {
  // All supported ARM targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  int16_t* ptr = reinterpret_cast<int16_t*>(addr);
  return *ptr;
}


void Simulator::WriteH(int32_t addr, uint16_t value, Instruction* instr) {
  // All supported ARM targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
  *ptr = value;
}


void Simulator::WriteH(int32_t addr, int16_t value, Instruction* instr) {
  // All supported ARM targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  int16_t* ptr = reinterpret_cast<int16_t*>(addr);
  *ptr = value;
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


int32_t* Simulator::ReadDW(int32_t addr) {
  // All supported ARM targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  int32_t* ptr = reinterpret_cast<int32_t*>(addr);
  return ptr;
}


void Simulator::WriteDW(int32_t addr, int32_t value1, int32_t value2) {
  // All supported ARM targets allow unaligned accesses, so we don't need to
  // check the alignment here.
  int32_t* ptr = reinterpret_cast<int32_t*>(addr);
  *ptr++ = value1;
  *ptr = value2;
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
  PrintF("Simulator found unsupported instruction:\n 0x%08" V8PRIxPTR ": %s\n",
         reinterpret_cast<intptr_t>(instr), format);
  UNIMPLEMENTED();
}


// Checks if the current instruction should be executed based on its
// condition bits.
bool Simulator::ConditionallyExecute(Instruction* instr) {
  switch (instr->ConditionField()) {
    case eq: return z_flag_;
    case ne: return !z_flag_;
    case cs: return c_flag_;
    case cc: return !c_flag_;
    case mi: return n_flag_;
    case pl: return !n_flag_;
    case vs: return v_flag_;
    case vc: return !v_flag_;
    case hi: return c_flag_ && !z_flag_;
    case ls: return !c_flag_ || z_flag_;
    case ge: return n_flag_ == v_flag_;
    case lt: return n_flag_ != v_flag_;
    case gt: return !z_flag_ && (n_flag_ == v_flag_);
    case le: return z_flag_ || (n_flag_ != v_flag_);
    case al: return true;
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
bool Simulator::CarryFrom(int32_t left, int32_t right, int32_t carry) {
  uint32_t uleft = static_cast<uint32_t>(left);
  uint32_t uright = static_cast<uint32_t>(right);
  uint32_t urest  = 0xffffffffU - uleft;

  return (uright > urest) ||
         (carry && (((uright + 1) > urest) || (uright > (urest - 1))));
}


// Calculate C flag value for subtractions.
bool Simulator::BorrowFrom(int32_t left, int32_t right, int32_t carry) {
  uint32_t uleft = static_cast<uint32_t>(left);
  uint32_t uright = static_cast<uint32_t>(right);

  return (uright > uleft) ||
         (!carry && (((uright + 1) > uleft) || (uright > (uleft - 1))));
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
void Simulator::Compute_FPSCR_Flags(float val1, float val2) {
  if (std::isnan(val1) || std::isnan(val2)) {
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


void Simulator::Compute_FPSCR_Flags(double val1, double val2) {
  if (std::isnan(val1) || std::isnan(val2)) {
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
int32_t Simulator::GetShiftRm(Instruction* instr, bool* carry_out) {
  ShiftOp shift = instr->ShiftField();
  int shift_amount = instr->ShiftAmountValue();
  int32_t result = get_register(instr->RmValue());
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
        if (shift_amount == 0) {
          *carry_out = c_flag_;
        } else {
          uint32_t left = static_cast<uint32_t>(result) >> shift_amount;
          uint32_t right = static_cast<uint32_t>(result) << (32 - shift_amount);
          result = right | left;
          *carry_out = (static_cast<uint32_t>(result) >> 31) != 0;
        }
        break;
      }

      default: {
        UNREACHABLE();
        break;
      }
    }
  } else {
    // by register
    int rs = instr->RsValue();
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
          DCHECK(shift_amount >= 32);
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
          DCHECK(shift_amount > 32);
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
        if (shift_amount == 0) {
          *carry_out = c_flag_;
        } else {
          uint32_t left = static_cast<uint32_t>(result) >> shift_amount;
          uint32_t right = static_cast<uint32_t>(result) << (32 - shift_amount);
          result = right | left;
          *carry_out = (static_cast<uint32_t>(result) >> 31) != 0;
        }
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
int32_t Simulator::GetImm(Instruction* instr, bool* carry_out) {
  int rotate = instr->RotateValue() * 2;
  int immed8 = instr->Immed8Value();
  int imm = base::bits::RotateRight32(immed8, rotate);
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


int32_t Simulator::ProcessPU(Instruction* instr,
                             int num_regs,
                             int reg_size,
                             intptr_t* start_address,
                             intptr_t* end_address) {
  int rn = instr->RnValue();
  int32_t rn_val = get_register(rn);
  switch (instr->PUField()) {
    case da_x: {
      UNIMPLEMENTED();
      break;
    }
    case ia_x: {
      *start_address = rn_val;
      *end_address = rn_val + (num_regs * reg_size) - reg_size;
      rn_val = rn_val + (num_regs * reg_size);
      break;
    }
    case db_x: {
      *start_address = rn_val - (num_regs * reg_size);
      *end_address = rn_val - reg_size;
      rn_val = *start_address;
      break;
    }
    case ib_x: {
      *start_address = rn_val + reg_size;
      *end_address = rn_val + (num_regs * reg_size);
      rn_val = *end_address;
      break;
    }
    default: {
      UNREACHABLE();
      break;
    }
  }
  return rn_val;
}


// Addressing Mode 4 - Load and Store Multiple
void Simulator::HandleRList(Instruction* instr, bool load) {
  int rlist = instr->RlistValue();
  int num_regs = count_bits(rlist);

  intptr_t start_address = 0;
  intptr_t end_address = 0;
  int32_t rn_val =
      ProcessPU(instr, num_regs, kPointerSize, &start_address, &end_address);

  intptr_t* address = reinterpret_cast<intptr_t*>(start_address);
  // Catch null pointers a little earlier.
  DCHECK(start_address > 8191 || start_address < 0);
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
  DCHECK(end_address == ((intptr_t)address) - 4);
  if (instr->HasW()) {
    set_register(instr->RnValue(), rn_val);
  }
}


// Addressing Mode 6 - Load and Store Multiple Coprocessor registers.
void Simulator::HandleVList(Instruction* instr) {
  VFPRegPrecision precision =
      (instr->SzValue() == 0) ? kSinglePrecision : kDoublePrecision;
  int operand_size = (precision == kSinglePrecision) ? 4 : 8;

  bool load = (instr->VLValue() == 0x1);

  int vd;
  int num_regs;
  vd = instr->VFPDRegValue(precision);
  if (precision == kSinglePrecision) {
    num_regs = instr->Immed8Value();
  } else {
    num_regs = instr->Immed8Value() / 2;
  }

  intptr_t start_address = 0;
  intptr_t end_address = 0;
  int32_t rn_val =
      ProcessPU(instr, num_regs, operand_size, &start_address, &end_address);

  intptr_t* address = reinterpret_cast<intptr_t*>(start_address);
  for (int reg = vd; reg < vd + num_regs; reg++) {
    if (precision == kSinglePrecision) {
      if (load) {
        set_s_register_from_sinteger(
            reg, ReadW(reinterpret_cast<int32_t>(address), instr));
      } else {
        WriteW(reinterpret_cast<int32_t>(address),
               get_sinteger_from_s_register(reg), instr);
      }
      address += 1;
    } else {
      if (load) {
        int32_t data[] = {
          ReadW(reinterpret_cast<int32_t>(address), instr),
          ReadW(reinterpret_cast<int32_t>(address + 1), instr)
        };
        set_d_register(reg, reinterpret_cast<uint32_t*>(data));
      } else {
        uint32_t data[2];
        get_d_register(reg, data);
        WriteW(reinterpret_cast<int32_t>(address), data[0], instr);
        WriteW(reinterpret_cast<int32_t>(address + 1), data[1], instr);
      }
      address += 2;
    }
  }
  DCHECK(reinterpret_cast<intptr_t>(address) - operand_size == end_address);
  if (instr->HasW()) {
    set_register(instr->RnValue(), rn_val);
  }
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
                                        int32_t arg3,
                                        int32_t arg4,
                                        int32_t arg5);

typedef ObjectTriple (*SimulatorRuntimeTripleCall)(int32_t arg0, int32_t arg1,
                                                   int32_t arg2, int32_t arg3,
                                                   int32_t arg4);

// These prototypes handle the four types of FP calls.
typedef int64_t (*SimulatorRuntimeCompareCall)(double darg0, double darg1);
typedef double (*SimulatorRuntimeFPFPCall)(double darg0, double darg1);
typedef double (*SimulatorRuntimeFPCall)(double darg0);
typedef double (*SimulatorRuntimeFPIntCall)(double darg0, int32_t arg0);

// This signature supports direct call in to API function native callback
// (refer to InvocationCallback in v8.h).
typedef void (*SimulatorRuntimeDirectApiCall)(int32_t arg0);
typedef void (*SimulatorRuntimeProfilingApiCall)(int32_t arg0, void* arg1);

// This signature supports direct call to accessor getter callback.
typedef void (*SimulatorRuntimeDirectGetterCall)(int32_t arg0, int32_t arg1);
typedef void (*SimulatorRuntimeProfilingGetterCall)(
    int32_t arg0, int32_t arg1, void* arg2);

// Software interrupt instructions are used by the simulator to call into the
// C-based V8 runtime.
void Simulator::SoftwareInterrupt(Instruction* instr) {
  int svc = instr->SvcValue();
  switch (svc) {
    case kCallRtRedirected: {
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
      int32_t* stack_pointer = reinterpret_cast<int32_t*>(get_register(sp));
      int32_t arg4 = stack_pointer[0];
      int32_t arg5 = stack_pointer[1];
      bool fp_call =
         (redirection->type() == ExternalReference::BUILTIN_FP_FP_CALL) ||
         (redirection->type() == ExternalReference::BUILTIN_COMPARE_CALL) ||
         (redirection->type() == ExternalReference::BUILTIN_FP_CALL) ||
         (redirection->type() == ExternalReference::BUILTIN_FP_INT_CALL);
      // This is dodgy but it works because the C entry stubs are never moved.
      // See comment in codegen-arm.cc and bug 1242173.
      int32_t saved_lr = get_register(lr);
      intptr_t external =
          reinterpret_cast<intptr_t>(redirection->external_function());
      if (fp_call) {
        double dval0, dval1;  // one or two double parameters
        int32_t ival;         // zero or one integer parameters
        int64_t iresult = 0;  // integer return value
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
            PrintF("Call to host function at %p with args %f, %d",
                   static_cast<void*>(FUNCTION_ADDR(generic_target)), dval0,
                   ival);
            break;
          default:
            UNREACHABLE();
            break;
          }
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        switch (redirection->type()) {
        case ExternalReference::BUILTIN_COMPARE_CALL: {
          SimulatorRuntimeCompareCall target =
            reinterpret_cast<SimulatorRuntimeCompareCall>(external);
          iresult = target(dval0, dval1);
          set_register(r0, static_cast<int32_t>(iresult));
          set_register(r1, static_cast<int32_t>(iresult >> 32));
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
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF("Call to host function at %p args %08x",
              reinterpret_cast<void*>(external), arg0);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        SimulatorRuntimeDirectApiCall target =
            reinterpret_cast<SimulatorRuntimeDirectApiCall>(external);
        target(arg0);
      } else if (
          redirection->type() == ExternalReference::PROFILING_API_CALL) {
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF("Call to host function at %p args %08x %08x",
              reinterpret_cast<void*>(external), arg0, arg1);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        SimulatorRuntimeProfilingApiCall target =
            reinterpret_cast<SimulatorRuntimeProfilingApiCall>(external);
        target(arg0, Redirection::ReverseRedirection(arg1));
      } else if (
          redirection->type() == ExternalReference::DIRECT_GETTER_CALL) {
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF("Call to host function at %p args %08x %08x",
              reinterpret_cast<void*>(external), arg0, arg1);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        SimulatorRuntimeDirectGetterCall target =
            reinterpret_cast<SimulatorRuntimeDirectGetterCall>(external);
        target(arg0, arg1);
      } else if (
          redirection->type() == ExternalReference::PROFILING_GETTER_CALL) {
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF("Call to host function at %p args %08x %08x %08x",
              reinterpret_cast<void*>(external), arg0, arg1, arg2);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        SimulatorRuntimeProfilingGetterCall target =
            reinterpret_cast<SimulatorRuntimeProfilingGetterCall>(
                external);
        target(arg0, arg1, Redirection::ReverseRedirection(arg2));
      } else if (redirection->type() ==
                 ExternalReference::BUILTIN_CALL_TRIPLE) {
        // builtin call returning ObjectTriple.
        SimulatorRuntimeTripleCall target =
            reinterpret_cast<SimulatorRuntimeTripleCall>(external);
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF(
              "Call to host triple returning runtime function %p "
              "args %08x, %08x, %08x, %08x, %08x",
              static_cast<void*>(FUNCTION_ADDR(target)), arg1, arg2, arg3, arg4,
              arg5);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
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
        set_register(r0, arg0);
      } else {
        // builtin call.
        DCHECK(redirection->type() == ExternalReference::BUILTIN_CALL ||
               redirection->type() == ExternalReference::BUILTIN_CALL_PAIR);
        SimulatorRuntimeCall target =
            reinterpret_cast<SimulatorRuntimeCall>(external);
        if (::v8::internal::FLAG_trace_sim || !stack_aligned) {
          PrintF(
              "Call to host function at %p "
              "args %08x, %08x, %08x, %08x, %08x, %08x",
              static_cast<void*>(FUNCTION_ADDR(target)), arg0, arg1, arg2, arg3,
              arg4, arg5);
          if (!stack_aligned) {
            PrintF(" with unaligned stack %08x\n", get_register(sp));
          }
          PrintF("\n");
        }
        CHECK(stack_aligned);
        int64_t result = target(arg0, arg1, arg2, arg3, arg4, arg5);
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
    case kBreakpoint: {
      ArmDebugger dbg(this);
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
          ArmDebugger dbg(this);
          dbg.Stop(instr);
        } else {
          set_pc(get_pc() + 2 * Instruction::kInstrSize);
        }
      } else {
        // This is not a valid svc code.
        UNREACHABLE();
        break;
      }
    }
  }
}


float Simulator::canonicalizeNaN(float value) {
  // Default NaN value, see "NaN handling" in "IEEE 754 standard implementation
  // choices" of the ARM Reference Manual.
  const uint32_t kDefaultNaN = 0x7FC00000u;
  if (FPSCR_default_NaN_mode_ && std::isnan(value)) {
    value = bit_cast<float>(kDefaultNaN);
  }
  return value;
}


double Simulator::canonicalizeNaN(double value) {
  // Default NaN value, see "NaN handling" in "IEEE 754 standard implementation
  // choices" of the ARM Reference Manual.
  const uint64_t kDefaultNaN = V8_UINT64_C(0x7FF8000000000000);
  if (FPSCR_default_NaN_mode_ && std::isnan(value)) {
    value = bit_cast<double>(kDefaultNaN);
  }
  return value;
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
    PrintF("Stop counter for code %i has overflowed.\n"
           "Enabling this code and reseting the counter to 0.\n", code);
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
        PrintF("stop %i - 0x%x: \t%s, \tcounter = %i, \t%s\n",
               code, code, state, count, watched_stops_[code].desc);
      } else {
        PrintF("stop %i - 0x%x: \t%s, \tcounter = %i\n",
               code, code, state, count);
      }
    }
  }
}


// Handle execution based on instruction types.

// Instruction types 0 and 1 are both rolled into one function because they
// only differ in the handling of the shifter_operand.
void Simulator::DecodeType01(Instruction* instr) {
  int type = instr->TypeValue();
  if ((type == 0) && instr->IsSpecialType0()) {
    // multiply instruction or extra loads and stores
    if (instr->Bits(7, 4) == 9) {
      if (instr->Bit(24) == 0) {
        // Raw field decoding here. Multiply instructions have their Rd in
        // funny places.
        int rn = instr->RnValue();
        int rm = instr->RmValue();
        int rs = instr->RsValue();
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
            int rd = instr->RdValue();
            int32_t acc_value = get_register(rd);
            if (instr->Bit(22) == 0) {
              // The MLA instruction description (A 4.1.28) refers to the order
              // of registers as "Rd, Rm, Rs, Rn". But confusingly it uses the
              // Rn field to encode the Rd register and the Rd field to encode
              // the Rn register.
              // Format(instr, "mla'cond's 'rn, 'rm, 'rs, 'rd");
              int32_t mul_out = rm_val * rs_val;
              int32_t result = acc_value + mul_out;
              set_register(rn, result);
            } else {
              // Format(instr, "mls'cond's 'rn, 'rm, 'rs, 'rd");
              int32_t mul_out = rm_val * rs_val;
              int32_t result = acc_value - mul_out;
              set_register(rn, result);
            }
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
          int rd_lo = instr->RdValue();
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
      int rd = instr->RdValue();
      int rn = instr->RnValue();
      int32_t rn_val = get_register(rn);
      int32_t addr = 0;
      if (instr->Bit(22) == 0) {
        int rm = instr->RmValue();
        int32_t rm_val = get_register(rm);
        switch (instr->PUField()) {
          case da_x: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn], -'rm");
            DCHECK(!instr->HasW());
            addr = rn_val;
            rn_val -= rm_val;
            set_register(rn, rn_val);
            break;
          }
          case ia_x: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn], +'rm");
            DCHECK(!instr->HasW());
            addr = rn_val;
            rn_val += rm_val;
            set_register(rn, rn_val);
            break;
          }
          case db_x: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn, -'rm]'w");
            rn_val -= rm_val;
            addr = rn_val;
            if (instr->HasW()) {
              set_register(rn, rn_val);
            }
            break;
          }
          case ib_x: {
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
        int32_t imm_val = (instr->ImmedHValue() << 4) | instr->ImmedLValue();
        switch (instr->PUField()) {
          case da_x: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn], #-'off8");
            DCHECK(!instr->HasW());
            addr = rn_val;
            rn_val -= imm_val;
            set_register(rn, rn_val);
            break;
          }
          case ia_x: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn], #+'off8");
            DCHECK(!instr->HasW());
            addr = rn_val;
            rn_val += imm_val;
            set_register(rn, rn_val);
            break;
          }
          case db_x: {
            // Format(instr, "'memop'cond'sign'h 'rd, ['rn, #-'off8]'w");
            rn_val -= imm_val;
            addr = rn_val;
            if (instr->HasW()) {
              set_register(rn, rn_val);
            }
            break;
          }
          case ib_x: {
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
      if (((instr->Bits(7, 4) & 0xd) == 0xd) && (instr->Bit(20) == 0)) {
        DCHECK((rd % 2) == 0);
        if (instr->HasH()) {
          // The strd instruction.
          int32_t value1 = get_register(rd);
          int32_t value2 = get_register(rd+1);
          WriteDW(addr, value1, value2);
        } else {
          // The ldrd instruction.
          int* rn_data = ReadDW(addr);
          set_dw_register(rd, rn_data);
        }
      } else if (instr->HasH()) {
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
        DCHECK(instr->HasSign());
        DCHECK(instr->HasL());
        int8_t val = ReadB(addr);
        set_register(rd, val);
      }
      return;
    }
  } else if ((type == 0) && instr->IsMiscType0()) {
    if ((instr->Bits(27, 23) == 2) && (instr->Bits(21, 20) == 2) &&
        (instr->Bits(15, 4) == 0xf00)) {
      // MSR
      int rm = instr->RmValue();
      DCHECK_NE(pc, rm);  // UNPREDICTABLE
      SRegisterFieldMask sreg_and_mask =
          instr->BitField(22, 22) | instr->BitField(19, 16);
      SetSpecialRegister(sreg_and_mask, get_register(rm));
    } else if ((instr->Bits(27, 23) == 2) && (instr->Bits(21, 20) == 0) &&
               (instr->Bits(11, 0) == 0)) {
      // MRS
      int rd = instr->RdValue();
      DCHECK_NE(pc, rd);  // UNPREDICTABLE
      SRegister sreg = static_cast<SRegister>(instr->BitField(22, 22));
      set_register(rd, GetFromSpecialRegister(sreg));
    } else if (instr->Bits(22, 21) == 1) {
      int rm = instr->RmValue();
      switch (instr->BitField(7, 4)) {
        case BX:
          set_pc(get_register(rm));
          break;
        case BLX: {
          uint32_t old_pc = get_pc();
          set_pc(get_register(rm));
          set_register(lr, old_pc + Instruction::kInstrSize);
          break;
        }
        case BKPT: {
          ArmDebugger dbg(this);
          PrintF("Simulator hit BKPT.\n");
          dbg.Debug();
          break;
        }
        default:
          UNIMPLEMENTED();
      }
    } else if (instr->Bits(22, 21) == 3) {
      int rm = instr->RmValue();
      int rd = instr->RdValue();
      switch (instr->BitField(7, 4)) {
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
  } else if ((type == 1) && instr->IsNopType1()) {
    // NOP.
  } else {
    int rd = instr->RdValue();
    int rn = instr->RnValue();
    int32_t rn_val = get_register(rn);
    int32_t shifter_operand = 0;
    bool shifter_carry_out = 0;
    if (type == 0) {
      shifter_operand = GetShiftRm(instr, &shifter_carry_out);
    } else {
      DCHECK(instr->TypeValue() == 1);
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
        // Format(instr, "adc'cond's 'rd, 'rn, 'shift_rm");
        // Format(instr, "adc'cond's 'rd, 'rn, 'imm");
        alu_out = rn_val + shifter_operand + GetCarry();
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(CarryFrom(rn_val, shifter_operand, GetCarry()));
          SetVFlag(OverflowFrom(alu_out, rn_val, shifter_operand, true));
        }
        break;
      }

      case SBC: {
        //        Format(instr, "sbc'cond's 'rd, 'rn, 'shift_rm");
        //        Format(instr, "sbc'cond's 'rd, 'rn, 'imm");
        alu_out = (rn_val - shifter_operand) - (GetCarry() ? 0 : 1);
        set_register(rd, alu_out);
        if (instr->HasS()) {
          SetNZFlags(alu_out);
          SetCFlag(!BorrowFrom(rn_val, shifter_operand, GetCarry()));
          SetVFlag(OverflowFrom(alu_out, rn_val, shifter_operand, false));
        }
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
          // Format(instr, "movw'cond 'rd, 'imm").
          alu_out = instr->ImmedMovwMovtValue();
          set_register(rd, alu_out);
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
          // Format(instr, "movt'cond 'rd, 'imm").
          alu_out = (get_register(rd) & 0xffff) |
              (instr->ImmedMovwMovtValue() << 16);
          set_register(rd, alu_out);
        }
        break;
      }

      case CMN: {
        if (instr->HasS()) {
          // Format(instr, "cmn'cond 'rn, 'shift_rm");
          // Format(instr, "cmn'cond 'rn, 'imm");
          alu_out = rn_val + shifter_operand;
          SetNZFlags(alu_out);
          SetCFlag(CarryFrom(rn_val, shifter_operand));
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


void Simulator::DecodeType2(Instruction* instr) {
  int rd = instr->RdValue();
  int rn = instr->RnValue();
  int32_t rn_val = get_register(rn);
  int32_t im_val = instr->Offset12Value();
  int32_t addr = 0;
  switch (instr->PUField()) {
    case da_x: {
      // Format(instr, "'memop'cond'b 'rd, ['rn], #-'off12");
      DCHECK(!instr->HasW());
      addr = rn_val;
      rn_val -= im_val;
      set_register(rn, rn_val);
      break;
    }
    case ia_x: {
      // Format(instr, "'memop'cond'b 'rd, ['rn], #+'off12");
      DCHECK(!instr->HasW());
      addr = rn_val;
      rn_val += im_val;
      set_register(rn, rn_val);
      break;
    }
    case db_x: {
      // Format(instr, "'memop'cond'b 'rd, ['rn, #-'off12]'w");
      rn_val -= im_val;
      addr = rn_val;
      if (instr->HasW()) {
        set_register(rn, rn_val);
      }
      break;
    }
    case ib_x: {
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


void Simulator::DecodeType3(Instruction* instr) {
  int rd = instr->RdValue();
  int rn = instr->RnValue();
  int32_t rn_val = get_register(rn);
  bool shifter_carry_out = 0;
  int32_t shifter_operand = GetShiftRm(instr, &shifter_carry_out);
  int32_t addr = 0;
  switch (instr->PUField()) {
    case da_x: {
      DCHECK(!instr->HasW());
      Format(instr, "'memop'cond'b 'rd, ['rn], -'shift_rm");
      UNIMPLEMENTED();
      break;
    }
    case ia_x: {
      if (instr->Bit(4) == 0) {
        // Memop.
      } else {
        if (instr->Bit(5) == 0) {
          switch (instr->Bits(22, 21)) {
            case 0:
              if (instr->Bit(20) == 0) {
                if (instr->Bit(6) == 0) {
                  // Pkhbt.
                  uint32_t rn_val = get_register(rn);
                  uint32_t rm_val = get_register(instr->RmValue());
                  int32_t shift = instr->Bits(11, 7);
                  rm_val <<= shift;
                  set_register(rd, (rn_val & 0xFFFF) | (rm_val & 0xFFFF0000U));
                } else {
                  // Pkhtb.
                  uint32_t rn_val = get_register(rn);
                  int32_t rm_val = get_register(instr->RmValue());
                  int32_t shift = instr->Bits(11, 7);
                  if (shift == 0) {
                    shift = 32;
                  }
                  rm_val >>= shift;
                  set_register(rd, (rn_val & 0xFFFF0000U) | (rm_val & 0xFFFF));
                }
              } else {
                UNIMPLEMENTED();
              }
              break;
            case 1:
              UNIMPLEMENTED();
              break;
            case 2:
              UNIMPLEMENTED();
              break;
            case 3: {
              // Usat.
              int32_t sat_pos = instr->Bits(20, 16);
              int32_t sat_val = (1 << sat_pos) - 1;
              int32_t shift = instr->Bits(11, 7);
              int32_t shift_type = instr->Bit(6);
              int32_t rm_val = get_register(instr->RmValue());
              if (shift_type == 0) {  // LSL
                rm_val <<= shift;
              } else {  // ASR
                rm_val >>= shift;
              }
              // If saturation occurs, the Q flag should be set in the CPSR.
              // There is no Q flag yet, and no instruction (MRS) to read the
              // CPSR directly.
              if (rm_val > sat_val) {
                rm_val = sat_val;
              } else if (rm_val < 0) {
                rm_val = 0;
              }
              set_register(rd, rm_val);
              break;
            }
          }
        } else {
          switch (instr->Bits(22, 21)) {
            case 0:
              UNIMPLEMENTED();
              break;
            case 1:
              if (instr->Bits(9, 6) == 1) {
                if (instr->Bit(20) == 0) {
                  if (instr->Bits(19, 16) == 0xF) {
                    // Sxtb.
                    int32_t rm_val = get_register(instr->RmValue());
                    int32_t rotate = instr->Bits(11, 10);
                    switch (rotate) {
                      case 0:
                        break;
                      case 1:
                        rm_val = (rm_val >> 8) | (rm_val << 24);
                        break;
                      case 2:
                        rm_val = (rm_val >> 16) | (rm_val << 16);
                        break;
                      case 3:
                        rm_val = (rm_val >> 24) | (rm_val << 8);
                        break;
                    }
                    set_register(rd, static_cast<int8_t>(rm_val));
                  } else {
                    // Sxtab.
                    int32_t rn_val = get_register(rn);
                    int32_t rm_val = get_register(instr->RmValue());
                    int32_t rotate = instr->Bits(11, 10);
                    switch (rotate) {
                      case 0:
                        break;
                      case 1:
                        rm_val = (rm_val >> 8) | (rm_val << 24);
                        break;
                      case 2:
                        rm_val = (rm_val >> 16) | (rm_val << 16);
                        break;
                      case 3:
                        rm_val = (rm_val >> 24) | (rm_val << 8);
                        break;
                    }
                    set_register(rd, rn_val + static_cast<int8_t>(rm_val));
                  }
                } else {
                  if (instr->Bits(19, 16) == 0xF) {
                    // Sxth.
                    int32_t rm_val = get_register(instr->RmValue());
                    int32_t rotate = instr->Bits(11, 10);
                    switch (rotate) {
                      case 0:
                        break;
                      case 1:
                        rm_val = (rm_val >> 8) | (rm_val << 24);
                        break;
                      case 2:
                        rm_val = (rm_val >> 16) | (rm_val << 16);
                        break;
                      case 3:
                        rm_val = (rm_val >> 24) | (rm_val << 8);
                        break;
                    }
                    set_register(rd, static_cast<int16_t>(rm_val));
                  } else {
                    // Sxtah.
                    int32_t rn_val = get_register(rn);
                    int32_t rm_val = get_register(instr->RmValue());
                    int32_t rotate = instr->Bits(11, 10);
                    switch (rotate) {
                      case 0:
                        break;
                      case 1:
                        rm_val = (rm_val >> 8) | (rm_val << 24);
                        break;
                      case 2:
                        rm_val = (rm_val >> 16) | (rm_val << 16);
                        break;
                      case 3:
                        rm_val = (rm_val >> 24) | (rm_val << 8);
                        break;
                    }
                    set_register(rd, rn_val + static_cast<int16_t>(rm_val));
                  }
                }
              } else {
                UNREACHABLE();
              }
              break;
            case 2:
              if ((instr->Bit(20) == 0) && (instr->Bits(9, 6) == 1)) {
                if (instr->Bits(19, 16) == 0xF) {
                  // Uxtb16.
                  uint32_t rm_val = get_register(instr->RmValue());
                  int32_t rotate = instr->Bits(11, 10);
                  switch (rotate) {
                    case 0:
                      break;
                    case 1:
                      rm_val = (rm_val >> 8) | (rm_val << 24);
                      break;
                    case 2:
                      rm_val = (rm_val >> 16) | (rm_val << 16);
                      break;
                    case 3:
                      rm_val = (rm_val >> 24) | (rm_val << 8);
                      break;
                  }
                  set_register(rd, (rm_val & 0xFF) | (rm_val & 0xFF0000));
                } else {
                  UNIMPLEMENTED();
                }
              } else {
                UNIMPLEMENTED();
              }
              break;
            case 3:
              if ((instr->Bits(9, 6) == 1)) {
                if (instr->Bit(20) == 0) {
                  if (instr->Bits(19, 16) == 0xF) {
                    // Uxtb.
                    uint32_t rm_val = get_register(instr->RmValue());
                    int32_t rotate = instr->Bits(11, 10);
                    switch (rotate) {
                      case 0:
                        break;
                      case 1:
                        rm_val = (rm_val >> 8) | (rm_val << 24);
                        break;
                      case 2:
                        rm_val = (rm_val >> 16) | (rm_val << 16);
                        break;
                      case 3:
                        rm_val = (rm_val >> 24) | (rm_val << 8);
                        break;
                    }
                    set_register(rd, (rm_val & 0xFF));
                  } else {
                    // Uxtab.
                    uint32_t rn_val = get_register(rn);
                    uint32_t rm_val = get_register(instr->RmValue());
                    int32_t rotate = instr->Bits(11, 10);
                    switch (rotate) {
                      case 0:
                        break;
                      case 1:
                        rm_val = (rm_val >> 8) | (rm_val << 24);
                        break;
                      case 2:
                        rm_val = (rm_val >> 16) | (rm_val << 16);
                        break;
                      case 3:
                        rm_val = (rm_val >> 24) | (rm_val << 8);
                        break;
                    }
                    set_register(rd, rn_val + (rm_val & 0xFF));
                  }
                } else {
                  if (instr->Bits(19, 16) == 0xF) {
                    // Uxth.
                    uint32_t rm_val = get_register(instr->RmValue());
                    int32_t rotate = instr->Bits(11, 10);
                    switch (rotate) {
                      case 0:
                        break;
                      case 1:
                        rm_val = (rm_val >> 8) | (rm_val << 24);
                        break;
                      case 2:
                        rm_val = (rm_val >> 16) | (rm_val << 16);
                        break;
                      case 3:
                        rm_val = (rm_val >> 24) | (rm_val << 8);
                        break;
                    }
                    set_register(rd, (rm_val & 0xFFFF));
                  } else {
                    // Uxtah.
                    uint32_t rn_val = get_register(rn);
                    uint32_t rm_val = get_register(instr->RmValue());
                    int32_t rotate = instr->Bits(11, 10);
                    switch (rotate) {
                      case 0:
                        break;
                      case 1:
                        rm_val = (rm_val >> 8) | (rm_val << 24);
                        break;
                      case 2:
                        rm_val = (rm_val >> 16) | (rm_val << 16);
                        break;
                      case 3:
                        rm_val = (rm_val >> 24) | (rm_val << 8);
                        break;
                    }
                    set_register(rd, rn_val + (rm_val & 0xFFFF));
                  }
                }
              } else {
                // PU == 0b01, BW == 0b11, Bits(9, 6) != 0b0001
                if ((instr->Bits(20, 16) == 0x1f) &&
                    (instr->Bits(11, 4) == 0xf3)) {
                  // Rbit.
                  uint32_t rm_val = get_register(instr->RmValue());
                  set_register(rd, base::bits::ReverseBits(rm_val));
                } else {
                  UNIMPLEMENTED();
                }
              }
              break;
          }
        }
        return;
      }
      break;
    }
    case db_x: {
      if (instr->Bits(22, 20) == 0x5) {
        if (instr->Bits(7, 4) == 0x1) {
          int rm = instr->RmValue();
          int32_t rm_val = get_register(rm);
          int rs = instr->RsValue();
          int32_t rs_val = get_register(rs);
          if (instr->Bits(15, 12) == 0xF) {
            // SMMUL (in V8 notation matching ARM ISA format)
            // Format(instr, "smmul'cond 'rn, 'rm, 'rs");
            rn_val = base::bits::SignedMulHigh32(rm_val, rs_val);
          } else {
            // SMMLA (in V8 notation matching ARM ISA format)
            // Format(instr, "smmla'cond 'rn, 'rm, 'rs, 'rd");
            int rd = instr->RdValue();
            int32_t rd_val = get_register(rd);
            rn_val = base::bits::SignedMulHighAndAdd32(rm_val, rs_val, rd_val);
          }
          set_register(rn, rn_val);
          return;
        }
      }
      if (instr->Bits(5, 4) == 0x1) {
        if ((instr->Bit(22) == 0x0) && (instr->Bit(20) == 0x1)) {
          // (s/u)div (in V8 notation matching ARM ISA format) rn = rm/rs
          // Format(instr, "'(s/u)div'cond'b 'rn, 'rm, 'rs);
          int rm = instr->RmValue();
          int32_t rm_val = get_register(rm);
          int rs = instr->RsValue();
          int32_t rs_val = get_register(rs);
          int32_t ret_val = 0;
          // udiv
          if (instr->Bit(21) == 0x1) {
            ret_val = bit_cast<int32_t>(base::bits::UnsignedDiv32(
                bit_cast<uint32_t>(rm_val), bit_cast<uint32_t>(rs_val)));
          } else {
            ret_val = base::bits::SignedDiv32(rm_val, rs_val);
          }
          set_register(rn, ret_val);
          return;
        }
      }
      // Format(instr, "'memop'cond'b 'rd, ['rn, -'shift_rm]'w");
      addr = rn_val - shifter_operand;
      if (instr->HasW()) {
        set_register(rn, addr);
      }
      break;
    }
    case ib_x: {
      if (instr->HasW() && (instr->Bits(6, 4) == 0x5)) {
        uint32_t widthminus1 = static_cast<uint32_t>(instr->Bits(20, 16));
        uint32_t lsbit = static_cast<uint32_t>(instr->Bits(11, 7));
        uint32_t msbit = widthminus1 + lsbit;
        if (msbit <= 31) {
          if (instr->Bit(22)) {
            // ubfx - unsigned bitfield extract.
            uint32_t rm_val =
                static_cast<uint32_t>(get_register(instr->RmValue()));
            uint32_t extr_val = rm_val << (31 - msbit);
            extr_val = extr_val >> (31 - widthminus1);
            set_register(instr->RdValue(), extr_val);
          } else {
            // sbfx - signed bitfield extract.
            int32_t rm_val = get_register(instr->RmValue());
            int32_t extr_val = rm_val << (31 - msbit);
            extr_val = extr_val >> (31 - widthminus1);
            set_register(instr->RdValue(), extr_val);
          }
        } else {
          UNREACHABLE();
        }
        return;
      } else if (!instr->HasW() && (instr->Bits(6, 4) == 0x1)) {
        uint32_t lsbit = static_cast<uint32_t>(instr->Bits(11, 7));
        uint32_t msbit = static_cast<uint32_t>(instr->Bits(20, 16));
        if (msbit >= lsbit) {
          // bfc or bfi - bitfield clear/insert.
          uint32_t rd_val =
              static_cast<uint32_t>(get_register(instr->RdValue()));
          uint32_t bitcount = msbit - lsbit + 1;
          uint32_t mask = 0xffffffffu >> (32 - bitcount);
          rd_val &= ~(mask << lsbit);
          if (instr->RmValue() != 15) {
            // bfi - bitfield insert.
            uint32_t rm_val =
                static_cast<uint32_t>(get_register(instr->RmValue()));
            rm_val &= mask;
            rd_val |= rm_val << lsbit;
          }
          set_register(instr->RdValue(), rd_val);
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


void Simulator::DecodeType4(Instruction* instr) {
  DCHECK(instr->Bit(22) == 0);  // only allowed to be set in privileged mode
  if (instr->HasL()) {
    // Format(instr, "ldm'cond'pu 'rn'w, 'rlist");
    HandleRList(instr, true);
  } else {
    // Format(instr, "stm'cond'pu 'rn'w, 'rlist");
    HandleRList(instr, false);
  }
}


void Simulator::DecodeType5(Instruction* instr) {
  // Format(instr, "b'l'cond 'target");
  int off = (instr->SImmed24Value() << 2);
  intptr_t pc_address = get_pc();
  if (instr->HasLink()) {
    set_register(lr, pc_address + Instruction::kInstrSize);
  }
  int pc_reg = get_register(pc);
  set_pc(pc_reg + off);
}


void Simulator::DecodeType6(Instruction* instr) {
  DecodeType6CoprocessorIns(instr);
}


void Simulator::DecodeType7(Instruction* instr) {
  if (instr->Bit(24) == 1) {
    SoftwareInterrupt(instr);
  } else {
    switch (instr->CoprocessorValue()) {
      case 10:  // Fall through.
      case 11:
        DecodeTypeVFP(instr);
        break;
      case 15:
        DecodeTypeCP15(instr);
        break;
      default:
        UNIMPLEMENTED();
    }
  }
}


// void Simulator::DecodeTypeVFP(Instruction* instr)
// The Following ARMv7 VFPv instructions are currently supported.
// vmov :Sn = Rt
// vmov :Rt = Sn
// vcvt: Dd = Sm
// vcvt: Sd = Dm
// vcvt.f64.s32 Dd, Dd, #<fbits>
// Dd = vabs(Dm)
// Sd = vabs(Sm)
// Dd = vneg(Dm)
// Sd = vneg(Sm)
// Dd = vadd(Dn, Dm)
// Sd = vadd(Sn, Sm)
// Dd = vsub(Dn, Dm)
// Sd = vsub(Sn, Sm)
// Dd = vmul(Dn, Dm)
// Sd = vmul(Sn, Sm)
// Dd = vdiv(Dn, Dm)
// Sd = vdiv(Sn, Sm)
// vcmp(Dd, Dm)
// vcmp(Sd, Sm)
// Dd = vsqrt(Dm)
// Sd = vsqrt(Sm)
// vmrs
void Simulator::DecodeTypeVFP(Instruction* instr) {
  DCHECK((instr->TypeValue() == 7) && (instr->Bit(24) == 0x0) );
  DCHECK(instr->Bits(11, 9) == 0x5);

  // Obtain single precision register codes.
  int m = instr->VFPMRegValue(kSinglePrecision);
  int d = instr->VFPDRegValue(kSinglePrecision);
  int n = instr->VFPNRegValue(kSinglePrecision);
  // Obtain double precision register codes.
  int vm = instr->VFPMRegValue(kDoublePrecision);
  int vd = instr->VFPDRegValue(kDoublePrecision);
  int vn = instr->VFPNRegValue(kDoublePrecision);

  if (instr->Bit(4) == 0) {
    if (instr->Opc1Value() == 0x7) {
      // Other data processing instructions
      if ((instr->Opc2Value() == 0x0) && (instr->Opc3Value() == 0x1)) {
        // vmov register to register.
        if (instr->SzValue() == 0x1) {
          uint32_t data[2];
          get_d_register(vm, data);
          set_d_register(vd, data);
        } else {
          set_s_register(d, get_s_register(m));
        }
      } else if ((instr->Opc2Value() == 0x0) && (instr->Opc3Value() == 0x3)) {
        // vabs
        if (instr->SzValue() == 0x1) {
          double dm_value = get_double_from_d_register(vm);
          double dd_value = std::fabs(dm_value);
          dd_value = canonicalizeNaN(dd_value);
          set_d_register_from_double(vd, dd_value);
        } else {
          float sm_value = get_float_from_s_register(m);
          float sd_value = std::fabs(sm_value);
          sd_value = canonicalizeNaN(sd_value);
          set_s_register_from_float(d, sd_value);
        }
      } else if ((instr->Opc2Value() == 0x1) && (instr->Opc3Value() == 0x1)) {
        // vneg
        if (instr->SzValue() == 0x1) {
          double dm_value = get_double_from_d_register(vm);
          double dd_value = -dm_value;
          dd_value = canonicalizeNaN(dd_value);
          set_d_register_from_double(vd, dd_value);
        } else {
          float sm_value = get_float_from_s_register(m);
          float sd_value = -sm_value;
          sd_value = canonicalizeNaN(sd_value);
          set_s_register_from_float(d, sd_value);
        }
      } else if ((instr->Opc2Value() == 0x7) && (instr->Opc3Value() == 0x3)) {
        DecodeVCVTBetweenDoubleAndSingle(instr);
      } else if ((instr->Opc2Value() == 0x8) && (instr->Opc3Value() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if ((instr->Opc2Value() == 0xA) && (instr->Opc3Value() == 0x3) &&
                 (instr->Bit(8) == 1)) {
        // vcvt.f64.s32 Dd, Dd, #<fbits>
        int fraction_bits = 32 - ((instr->Bits(3, 0) << 1) | instr->Bit(5));
        int fixed_value = get_sinteger_from_s_register(vd * 2);
        double divide = 1 << fraction_bits;
        set_d_register_from_double(vd, fixed_value / divide);
      } else if (((instr->Opc2Value() >> 1) == 0x6) &&
                 (instr->Opc3Value() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if (((instr->Opc2Value() == 0x4) || (instr->Opc2Value() == 0x5)) &&
                 (instr->Opc3Value() & 0x1)) {
        DecodeVCMP(instr);
      } else if (((instr->Opc2Value() == 0x1)) && (instr->Opc3Value() == 0x3)) {
        // vsqrt
        lazily_initialize_fast_sqrt(isolate_);
        if (instr->SzValue() == 0x1) {
          double dm_value = get_double_from_d_register(vm);
          double dd_value = fast_sqrt(dm_value, isolate_);
          dd_value = canonicalizeNaN(dd_value);
          set_d_register_from_double(vd, dd_value);
        } else {
          float sm_value = get_float_from_s_register(m);
          float sd_value = fast_sqrt(sm_value, isolate_);
          sd_value = canonicalizeNaN(sd_value);
          set_s_register_from_float(d, sd_value);
        }
      } else if (instr->Opc3Value() == 0x0) {
        // vmov immediate.
        if (instr->SzValue() == 0x1) {
          set_d_register_from_double(vd, instr->DoubleImmedVmov());
        } else {
          set_s_register_from_float(d, instr->DoubleImmedVmov());
        }
      } else if (((instr->Opc2Value() == 0x6)) && (instr->Opc3Value() == 0x3)) {
        // vrintz - truncate
        if (instr->SzValue() == 0x1) {
          double dm_value = get_double_from_d_register(vm);
          double dd_value = trunc(dm_value);
          dd_value = canonicalizeNaN(dd_value);
          set_d_register_from_double(vd, dd_value);
        } else {
          float sm_value = get_float_from_s_register(m);
          float sd_value = truncf(sm_value);
          sd_value = canonicalizeNaN(sd_value);
          set_s_register_from_float(d, sd_value);
        }
      } else {
        UNREACHABLE();  // Not used by V8.
      }
    } else if (instr->Opc1Value() == 0x3) {
      if (instr->Opc3Value() & 0x1) {
        // vsub
        if (instr->SzValue() == 0x1) {
          double dn_value = get_double_from_d_register(vn);
          double dm_value = get_double_from_d_register(vm);
          double dd_value = dn_value - dm_value;
          dd_value = canonicalizeNaN(dd_value);
          set_d_register_from_double(vd, dd_value);
        } else {
          float sn_value = get_float_from_s_register(n);
          float sm_value = get_float_from_s_register(m);
          float sd_value = sn_value - sm_value;
          sd_value = canonicalizeNaN(sd_value);
          set_s_register_from_float(d, sd_value);
        }
      } else {
        // vadd
        if (instr->SzValue() == 0x1) {
          double dn_value = get_double_from_d_register(vn);
          double dm_value = get_double_from_d_register(vm);
          double dd_value = dn_value + dm_value;
          dd_value = canonicalizeNaN(dd_value);
          set_d_register_from_double(vd, dd_value);
        } else {
          float sn_value = get_float_from_s_register(n);
          float sm_value = get_float_from_s_register(m);
          float sd_value = sn_value + sm_value;
          sd_value = canonicalizeNaN(sd_value);
          set_s_register_from_float(d, sd_value);
        }
      }
    } else if ((instr->Opc1Value() == 0x2) && !(instr->Opc3Value() & 0x1)) {
      // vmul
      if (instr->SzValue() == 0x1) {
        double dn_value = get_double_from_d_register(vn);
        double dm_value = get_double_from_d_register(vm);
        double dd_value = dn_value * dm_value;
        dd_value = canonicalizeNaN(dd_value);
        set_d_register_from_double(vd, dd_value);
      } else {
        float sn_value = get_float_from_s_register(n);
        float sm_value = get_float_from_s_register(m);
        float sd_value = sn_value * sm_value;
        sd_value = canonicalizeNaN(sd_value);
        set_s_register_from_float(d, sd_value);
      }
    } else if ((instr->Opc1Value() == 0x0)) {
      // vmla, vmls
      const bool is_vmls = (instr->Opc3Value() & 0x1);
      if (instr->SzValue() == 0x1) {
        const double dd_val = get_double_from_d_register(vd);
        const double dn_val = get_double_from_d_register(vn);
        const double dm_val = get_double_from_d_register(vm);

        // Note: we do the mul and add/sub in separate steps to avoid getting a
        // result with too high precision.
        set_d_register_from_double(vd, dn_val * dm_val);
        if (is_vmls) {
          set_d_register_from_double(
              vd, canonicalizeNaN(dd_val - get_double_from_d_register(vd)));
        } else {
          set_d_register_from_double(
              vd, canonicalizeNaN(dd_val + get_double_from_d_register(vd)));
        }
      } else {
        const float sd_val = get_float_from_s_register(d);
        const float sn_val = get_float_from_s_register(n);
        const float sm_val = get_float_from_s_register(m);

        // Note: we do the mul and add/sub in separate steps to avoid getting a
        // result with too high precision.
        set_s_register_from_float(d, sn_val * sm_val);
        if (is_vmls) {
          set_s_register_from_float(
              d, canonicalizeNaN(sd_val - get_float_from_s_register(d)));
        } else {
          set_s_register_from_float(
              d, canonicalizeNaN(sd_val + get_float_from_s_register(d)));
        }
      }
    } else if ((instr->Opc1Value() == 0x4) && !(instr->Opc3Value() & 0x1)) {
      // vdiv
      if (instr->SzValue() == 0x1) {
        double dn_value = get_double_from_d_register(vn);
        double dm_value = get_double_from_d_register(vm);
        double dd_value = dn_value / dm_value;
        div_zero_vfp_flag_ = (dm_value == 0);
        dd_value = canonicalizeNaN(dd_value);
        set_d_register_from_double(vd, dd_value);
      } else {
        float sn_value = get_float_from_s_register(n);
        float sm_value = get_float_from_s_register(m);
        float sd_value = sn_value / sm_value;
        div_zero_vfp_flag_ = (sm_value == 0);
        sd_value = canonicalizeNaN(sd_value);
        set_s_register_from_float(d, sd_value);
      }
    } else {
      UNIMPLEMENTED();  // Not used by V8.
    }
  } else {
    if ((instr->VCValue() == 0x0) &&
        (instr->VAValue() == 0x0)) {
      DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(instr);
    } else if ((instr->VLValue() == 0x0) &&
               (instr->VCValue() == 0x1) &&
               (instr->Bit(23) == 0x0)) {
      // vmov (ARM core register to scalar)
      int vd = instr->Bits(19, 16) | (instr->Bit(7) << 4);
      uint32_t data[2];
      get_d_register(vd, data);
      data[instr->Bit(21)] = get_register(instr->RtValue());
      set_d_register(vd, data);
    } else if ((instr->VLValue() == 0x1) &&
               (instr->VCValue() == 0x1) &&
               (instr->Bit(23) == 0x0)) {
      // vmov (scalar to ARM core register)
      int vn = instr->Bits(19, 16) | (instr->Bit(7) << 4);
      double dn_value = get_double_from_d_register(vn);
      int32_t data[2];
      memcpy(data, &dn_value, 8);
      set_register(instr->RtValue(), data[instr->Bit(21)]);
    } else if ((instr->VLValue() == 0x1) &&
               (instr->VCValue() == 0x0) &&
               (instr->VAValue() == 0x7) &&
               (instr->Bits(19, 16) == 0x1)) {
      // vmrs
      uint32_t rt = instr->RtValue();
      if (rt == 0xF) {
        Copy_FPSCR_to_APSR();
      } else {
        // Emulate FPSCR from the Simulator flags.
        uint32_t fpscr = (n_flag_FPSCR_ << 31) |
                         (z_flag_FPSCR_ << 30) |
                         (c_flag_FPSCR_ << 29) |
                         (v_flag_FPSCR_ << 28) |
                         (FPSCR_default_NaN_mode_ << 25) |
                         (inexact_vfp_flag_ << 4) |
                         (underflow_vfp_flag_ << 3) |
                         (overflow_vfp_flag_ << 2) |
                         (div_zero_vfp_flag_ << 1) |
                         (inv_op_vfp_flag_ << 0) |
                         (FPSCR_rounding_mode_);
        set_register(rt, fpscr);
      }
    } else if ((instr->VLValue() == 0x0) &&
               (instr->VCValue() == 0x0) &&
               (instr->VAValue() == 0x7) &&
               (instr->Bits(19, 16) == 0x1)) {
      // vmsr
      uint32_t rt = instr->RtValue();
      if (rt == pc) {
        UNREACHABLE();
      } else {
        uint32_t rt_value = get_register(rt);
        n_flag_FPSCR_ = (rt_value >> 31) & 1;
        z_flag_FPSCR_ = (rt_value >> 30) & 1;
        c_flag_FPSCR_ = (rt_value >> 29) & 1;
        v_flag_FPSCR_ = (rt_value >> 28) & 1;
        FPSCR_default_NaN_mode_ = (rt_value >> 25) & 1;
        inexact_vfp_flag_ = (rt_value >> 4) & 1;
        underflow_vfp_flag_ = (rt_value >> 3) & 1;
        overflow_vfp_flag_ = (rt_value >> 2) & 1;
        div_zero_vfp_flag_ = (rt_value >> 1) & 1;
        inv_op_vfp_flag_ = (rt_value >> 0) & 1;
        FPSCR_rounding_mode_ =
            static_cast<VFPRoundingMode>((rt_value) & kVFPRoundingModeMask);
      }
    } else {
      UNIMPLEMENTED();  // Not used by V8.
    }
  }
}

void Simulator::DecodeTypeCP15(Instruction* instr) {
  DCHECK((instr->TypeValue() == 7) && (instr->Bit(24) == 0x0));
  DCHECK(instr->CoprocessorValue() == 15);

  if (instr->Bit(4) == 1) {
    // mcr
    int crn = instr->Bits(19, 16);
    int crm = instr->Bits(3, 0);
    int opc1 = instr->Bits(23, 21);
    int opc2 = instr->Bits(7, 5);
    if ((opc1 == 0) && (crn == 7)) {
      // ARMv6 memory barrier operations.
      // Details available in ARM DDI 0406C.b, B3-1750.
      if (((crm == 10) && (opc2 == 5)) ||  // CP15DMB
          ((crm == 10) && (opc2 == 4)) ||  // CP15DSB
          ((crm == 5) && (opc2 == 4))) {   // CP15ISB
        // These are ignored by the simulator for now.
      } else {
        UNIMPLEMENTED();
      }
    }
  } else {
    UNIMPLEMENTED();
  }
}

void Simulator::DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(
    Instruction* instr) {
  DCHECK((instr->Bit(4) == 1) && (instr->VCValue() == 0x0) &&
         (instr->VAValue() == 0x0));

  int t = instr->RtValue();
  int n = instr->VFPNRegValue(kSinglePrecision);
  bool to_arm_register = (instr->VLValue() == 0x1);

  if (to_arm_register) {
    int32_t int_value = get_sinteger_from_s_register(n);
    set_register(t, int_value);
  } else {
    int32_t rs_val = get_register(t);
    set_s_register_from_sinteger(n, rs_val);
  }
}


void Simulator::DecodeVCMP(Instruction* instr) {
  DCHECK((instr->Bit(4) == 0) && (instr->Opc1Value() == 0x7));
  DCHECK(((instr->Opc2Value() == 0x4) || (instr->Opc2Value() == 0x5)) &&
         (instr->Opc3Value() & 0x1));
  // Comparison.

  VFPRegPrecision precision = kSinglePrecision;
  if (instr->SzValue() == 0x1) {
    precision = kDoublePrecision;
  }

  int d = instr->VFPDRegValue(precision);
  int m = 0;
  if (instr->Opc2Value() == 0x4) {
    m = instr->VFPMRegValue(precision);
  }

  if (precision == kDoublePrecision) {
    double dd_value = get_double_from_d_register(d);
    double dm_value = 0.0;
    if (instr->Opc2Value() == 0x4) {
      dm_value = get_double_from_d_register(m);
    }

    // Raise exceptions for quiet NaNs if necessary.
    if (instr->Bit(7) == 1) {
      if (std::isnan(dd_value)) {
        inv_op_vfp_flag_ = true;
      }
    }

    Compute_FPSCR_Flags(dd_value, dm_value);
  } else {
    float sd_value = get_float_from_s_register(d);
    float sm_value = 0.0;
    if (instr->Opc2Value() == 0x4) {
      sm_value = get_float_from_s_register(m);
    }

    // Raise exceptions for quiet NaNs if necessary.
    if (instr->Bit(7) == 1) {
      if (std::isnan(sd_value)) {
        inv_op_vfp_flag_ = true;
      }
    }

    Compute_FPSCR_Flags(sd_value, sm_value);
  }
}


void Simulator::DecodeVCVTBetweenDoubleAndSingle(Instruction* instr) {
  DCHECK((instr->Bit(4) == 0) && (instr->Opc1Value() == 0x7));
  DCHECK((instr->Opc2Value() == 0x7) && (instr->Opc3Value() == 0x3));

  VFPRegPrecision dst_precision = kDoublePrecision;
  VFPRegPrecision src_precision = kSinglePrecision;
  if (instr->SzValue() == 1) {
    dst_precision = kSinglePrecision;
    src_precision = kDoublePrecision;
  }

  int dst = instr->VFPDRegValue(dst_precision);
  int src = instr->VFPMRegValue(src_precision);

  if (dst_precision == kSinglePrecision) {
    double val = get_double_from_d_register(src);
    set_s_register_from_float(dst, static_cast<float>(val));
  } else {
    float val = get_float_from_s_register(src);
    set_d_register_from_double(dst, static_cast<double>(val));
  }
}

bool get_inv_op_vfp_flag(VFPRoundingMode mode,
                         double val,
                         bool unsigned_) {
  DCHECK((mode == RN) || (mode == RM) || (mode == RZ));
  double max_uint = static_cast<double>(0xffffffffu);
  double max_int = static_cast<double>(kMaxInt);
  double min_int = static_cast<double>(kMinInt);

  // Check for NaN.
  if (val != val) {
    return true;
  }

  // Check for overflow. This code works because 32bit integers can be
  // exactly represented by ieee-754 64bit floating-point values.
  switch (mode) {
    case RN:
      return  unsigned_ ? (val >= (max_uint + 0.5)) ||
                          (val < -0.5)
                        : (val >= (max_int + 0.5)) ||
                          (val < (min_int - 0.5));

    case RM:
      return  unsigned_ ? (val >= (max_uint + 1.0)) ||
                          (val < 0)
                        : (val >= (max_int + 1.0)) ||
                          (val < min_int);

    case RZ:
      return  unsigned_ ? (val >= (max_uint + 1.0)) ||
                          (val <= -1)
                        : (val >= (max_int + 1.0)) ||
                          (val <= (min_int - 1.0));
    default:
      UNREACHABLE();
      return true;
  }
}


// We call this function only if we had a vfp invalid exception.
// It returns the correct saturated value.
int VFPConversionSaturate(double val, bool unsigned_res) {
  if (val != val) {
    return 0;
  } else {
    if (unsigned_res) {
      return (val < 0) ? 0 : 0xffffffffu;
    } else {
      return (val < 0) ? kMinInt : kMaxInt;
    }
  }
}


void Simulator::DecodeVCVTBetweenFloatingPointAndInteger(Instruction* instr) {
  DCHECK((instr->Bit(4) == 0) && (instr->Opc1Value() == 0x7) &&
         (instr->Bits(27, 23) == 0x1D));
  DCHECK(((instr->Opc2Value() == 0x8) && (instr->Opc3Value() & 0x1)) ||
         (((instr->Opc2Value() >> 1) == 0x6) && (instr->Opc3Value() & 0x1)));

  // Conversion between floating-point and integer.
  bool to_integer = (instr->Bit(18) == 1);

  VFPRegPrecision src_precision = (instr->SzValue() == 1) ? kDoublePrecision
                                                          : kSinglePrecision;

  if (to_integer) {
    // We are playing with code close to the C++ standard's limits below,
    // hence the very simple code and heavy checks.
    //
    // Note:
    // C++ defines default type casting from floating point to integer as
    // (close to) rounding toward zero ("fractional part discarded").

    int dst = instr->VFPDRegValue(kSinglePrecision);
    int src = instr->VFPMRegValue(src_precision);

    // Bit 7 in vcvt instructions indicates if we should use the FPSCR rounding
    // mode or the default Round to Zero mode.
    VFPRoundingMode mode = (instr->Bit(7) != 1) ? FPSCR_rounding_mode_
                                                : RZ;
    DCHECK((mode == RM) || (mode == RZ) || (mode == RN));

    bool unsigned_integer = (instr->Bit(16) == 0);
    bool double_precision = (src_precision == kDoublePrecision);

    double val = double_precision ? get_double_from_d_register(src)
                                  : get_float_from_s_register(src);

    int temp = unsigned_integer ? static_cast<uint32_t>(val)
                                : static_cast<int32_t>(val);

    inv_op_vfp_flag_ = get_inv_op_vfp_flag(mode, val, unsigned_integer);

    double abs_diff =
      unsigned_integer ? std::fabs(val - static_cast<uint32_t>(temp))
                       : std::fabs(val - temp);

    inexact_vfp_flag_ = (abs_diff != 0);

    if (inv_op_vfp_flag_) {
      temp = VFPConversionSaturate(val, unsigned_integer);
    } else {
      switch (mode) {
        case RN: {
          int val_sign = (val > 0) ? 1 : -1;
          if (abs_diff > 0.5) {
            temp += val_sign;
          } else if (abs_diff == 0.5) {
            // Round to even if exactly halfway.
            temp = ((temp % 2) == 0) ? temp : temp + val_sign;
          }
          break;
        }

        case RM:
          temp = temp > val ? temp - 1 : temp;
          break;

        case RZ:
          // Nothing to do.
          break;

        default:
          UNREACHABLE();
      }
    }

    // Update the destination register.
    set_s_register_from_sinteger(dst, temp);

  } else {
    bool unsigned_integer = (instr->Bit(7) == 0);

    int dst = instr->VFPDRegValue(src_precision);
    int src = instr->VFPMRegValue(kSinglePrecision);

    int val = get_sinteger_from_s_register(src);

    if (src_precision == kDoublePrecision) {
      if (unsigned_integer) {
        set_d_register_from_double(
            dst, static_cast<double>(static_cast<uint32_t>(val)));
      } else {
        set_d_register_from_double(dst, static_cast<double>(val));
      }
    } else {
      if (unsigned_integer) {
        set_s_register_from_float(
            dst, static_cast<float>(static_cast<uint32_t>(val)));
      } else {
        set_s_register_from_float(dst, static_cast<float>(val));
      }
    }
  }
}


// void Simulator::DecodeType6CoprocessorIns(Instruction* instr)
// Decode Type 6 coprocessor instructions.
// Dm = vmov(Rt, Rt2)
// <Rt, Rt2> = vmov(Dm)
// Ddst = MEM(Rbase + 4*offset).
// MEM(Rbase + 4*offset) = Dsrc.
void Simulator::DecodeType6CoprocessorIns(Instruction* instr) {
  DCHECK((instr->TypeValue() == 6));

  if (instr->CoprocessorValue() == 0xA) {
    switch (instr->OpcodeValue()) {
      case 0x8:
      case 0xA:
      case 0xC:
      case 0xE: {  // Load and store single precision float to memory.
        int rn = instr->RnValue();
        int vd = instr->VFPDRegValue(kSinglePrecision);
        int offset = instr->Immed8Value();
        if (!instr->HasU()) {
          offset = -offset;
        }

        int32_t address = get_register(rn) + 4 * offset;
        // Load and store address for singles must be at least four-byte
        // aligned.
        DCHECK((address % 4) == 0);
        if (instr->HasL()) {
          // Load single from memory: vldr.
          set_s_register_from_sinteger(vd, ReadW(address, instr));
        } else {
          // Store single to memory: vstr.
          WriteW(address, get_sinteger_from_s_register(vd), instr);
        }
        break;
      }
      case 0x4:
      case 0x5:
      case 0x6:
      case 0x7:
      case 0x9:
      case 0xB:
        // Load/store multiple single from memory: vldm/vstm.
        HandleVList(instr);
        break;
      default:
        UNIMPLEMENTED();  // Not used by V8.
    }
  } else if (instr->CoprocessorValue() == 0xB) {
    switch (instr->OpcodeValue()) {
      case 0x2:
        // Load and store double to two GP registers
        if (instr->Bits(7, 6) != 0 || instr->Bit(4) != 1) {
          UNIMPLEMENTED();  // Not used by V8.
        } else {
          int rt = instr->RtValue();
          int rn = instr->RnValue();
          int vm = instr->VFPMRegValue(kDoublePrecision);
          if (instr->HasL()) {
            uint32_t data[2];
            get_d_register(vm, data);
            set_register(rt, data[0]);
            set_register(rn, data[1]);
          } else {
            int32_t data[] = { get_register(rt), get_register(rn) };
            set_d_register(vm, reinterpret_cast<uint32_t*>(data));
          }
        }
        break;
      case 0x8:
      case 0xA:
      case 0xC:
      case 0xE: {  // Load and store double to memory.
        int rn = instr->RnValue();
        int vd = instr->VFPDRegValue(kDoublePrecision);
        int offset = instr->Immed8Value();
        if (!instr->HasU()) {
          offset = -offset;
        }
        int32_t address = get_register(rn) + 4 * offset;
        // Load and store address for doubles must be at least four-byte
        // aligned.
        DCHECK((address % 4) == 0);
        if (instr->HasL()) {
          // Load double from memory: vldr.
          int32_t data[] = {
            ReadW(address, instr),
            ReadW(address + 4, instr)
          };
          set_d_register(vd, reinterpret_cast<uint32_t*>(data));
        } else {
          // Store double to memory: vstr.
          uint32_t data[2];
          get_d_register(vd, data);
          WriteW(address, data[0], instr);
          WriteW(address + 4, data[1], instr);
        }
        break;
      }
      case 0x4:
      case 0x5:
      case 0x6:
      case 0x7:
      case 0x9:
      case 0xB:
        // Load/store multiple double from memory: vldm/vstm.
        HandleVList(instr);
        break;
      default:
        UNIMPLEMENTED();  // Not used by V8.
    }
  } else {
    UNIMPLEMENTED();  // Not used by V8.
  }
}


void Simulator::DecodeSpecialCondition(Instruction* instr) {
  switch (instr->SpecialValue()) {
    case 5:
      if ((instr->Bits(18, 16) == 0) && (instr->Bits(11, 6) == 0x28) &&
          (instr->Bit(4) == 1)) {
        // vmovl signed
        if ((instr->VdValue() & 1) != 0) UNIMPLEMENTED();
        int Vd = (instr->Bit(22) << 3) | (instr->VdValue() >> 1);
        int Vm = (instr->Bit(5) << 4) | instr->VmValue();
        int imm3 = instr->Bits(21, 19);
        if ((imm3 != 1) && (imm3 != 2) && (imm3 != 4)) UNIMPLEMENTED();
        int esize = 8 * imm3;
        int elements = 64 / esize;
        int8_t from[8];
        get_d_register(Vm, reinterpret_cast<uint64_t*>(from));
        int16_t to[8];
        int e = 0;
        while (e < elements) {
          to[e] = from[e];
          e++;
        }
        set_q_register(Vd, reinterpret_cast<uint64_t*>(to));
      } else {
        UNIMPLEMENTED();
      }
      break;
    case 7:
      if ((instr->Bits(18, 16) == 0) && (instr->Bits(11, 6) == 0x28) &&
          (instr->Bit(4) == 1)) {
        // vmovl unsigned
        if ((instr->VdValue() & 1) != 0) UNIMPLEMENTED();
        int Vd = (instr->Bit(22) << 3) | (instr->VdValue() >> 1);
        int Vm = (instr->Bit(5) << 4) | instr->VmValue();
        int imm3 = instr->Bits(21, 19);
        if ((imm3 != 1) && (imm3 != 2) && (imm3 != 4)) UNIMPLEMENTED();
        int esize = 8 * imm3;
        int elements = 64 / esize;
        uint8_t from[8];
        get_d_register(Vm, reinterpret_cast<uint64_t*>(from));
        uint16_t to[8];
        int e = 0;
        while (e < elements) {
          to[e] = from[e];
          e++;
        }
        set_q_register(Vd, reinterpret_cast<uint64_t*>(to));
      } else if ((instr->Bits(21, 16) == 0x32) && (instr->Bits(11, 7) == 0) &&
                 (instr->Bit(4) == 0)) {
        int vd = instr->VFPDRegValue(kDoublePrecision);
        int vm = instr->VFPMRegValue(kDoublePrecision);
        if (instr->Bit(6) == 0) {
          // vswp Dd, Dm.
          uint64_t dval, mval;
          get_d_register(vd, &dval);
          get_d_register(vm, &mval);
          set_d_register(vm, &dval);
          set_d_register(vd, &mval);
        } else {
          // Q register vswp unimplemented.
          UNIMPLEMENTED();
        }
      } else {
        UNIMPLEMENTED();
      }
      break;
    case 8:
      if (instr->Bits(21, 20) == 0) {
        // vst1
        int Vd = (instr->Bit(22) << 4) | instr->VdValue();
        int Rn = instr->VnValue();
        int type = instr->Bits(11, 8);
        int Rm = instr->VmValue();
        int32_t address = get_register(Rn);
        int regs = 0;
        switch (type) {
          case nlt_1:
            regs = 1;
            break;
          case nlt_2:
            regs = 2;
            break;
          case nlt_3:
            regs = 3;
            break;
          case nlt_4:
            regs = 4;
            break;
          default:
            UNIMPLEMENTED();
            break;
        }
        int r = 0;
        while (r < regs) {
          uint32_t data[2];
          get_d_register(Vd + r, data);
          WriteW(address, data[0], instr);
          WriteW(address + 4, data[1], instr);
          address += 8;
          r++;
        }
        if (Rm != 15) {
          if (Rm == 13) {
            set_register(Rn, address);
          } else {
            set_register(Rn, get_register(Rn) + get_register(Rm));
          }
        }
      } else if (instr->Bits(21, 20) == 2) {
        // vld1
        int Vd = (instr->Bit(22) << 4) | instr->VdValue();
        int Rn = instr->VnValue();
        int type = instr->Bits(11, 8);
        int Rm = instr->VmValue();
        int32_t address = get_register(Rn);
        int regs = 0;
        switch (type) {
          case nlt_1:
            regs = 1;
            break;
          case nlt_2:
            regs = 2;
            break;
          case nlt_3:
            regs = 3;
            break;
          case nlt_4:
            regs = 4;
            break;
          default:
            UNIMPLEMENTED();
            break;
        }
        int r = 0;
        while (r < regs) {
          uint32_t data[2];
          data[0] = ReadW(address, instr);
          data[1] = ReadW(address + 4, instr);
          set_d_register(Vd + r, data);
          address += 8;
          r++;
        }
        if (Rm != 15) {
          if (Rm == 13) {
            set_register(Rn, address);
          } else {
            set_register(Rn, get_register(Rn) + get_register(Rm));
          }
        }
      } else {
        UNIMPLEMENTED();
      }
      break;
    case 0xA:
    case 0xB:
      if ((instr->Bits(22, 20) == 5) && (instr->Bits(15, 12) == 0xf)) {
        // pld: ignore instruction.
      } else if (instr->SpecialValue() == 0xA && instr->Bits(22, 20) == 7) {
        // dsb, dmb, isb: ignore instruction for now.
        // TODO(binji): implement
        // Also refer to the ARMv6 CP15 equivalents in DecodeTypeCP15.
      } else {
        UNIMPLEMENTED();
      }
      break;
    case 0x1D:
      if (instr->Opc1Value() == 0x7 && instr->Opc3Value() == 0x1 &&
          instr->Bits(11, 9) == 0x5 && instr->Bits(19, 18) == 0x2) {
        if (instr->SzValue() == 0x1) {
          int vm = instr->VFPMRegValue(kDoublePrecision);
          int vd = instr->VFPDRegValue(kDoublePrecision);
          double dm_value = get_double_from_d_register(vm);
          double dd_value = 0.0;
          int rounding_mode = instr->Bits(17, 16);
          switch (rounding_mode) {
            case 0x0:  // vrinta - round with ties to away from zero
              dd_value = round(dm_value);
              break;
            case 0x1: {  // vrintn - round with ties to even
              dd_value = nearbyint(dm_value);
              break;
            }
            case 0x2:  // vrintp - ceil
              dd_value = ceil(dm_value);
              break;
            case 0x3:  // vrintm - floor
              dd_value = floor(dm_value);
              break;
            default:
              UNREACHABLE();  // Case analysis is exhaustive.
              break;
          }
          dd_value = canonicalizeNaN(dd_value);
          set_d_register_from_double(vd, dd_value);
        } else {
          int m = instr->VFPMRegValue(kSinglePrecision);
          int d = instr->VFPDRegValue(kSinglePrecision);
          float sm_value = get_float_from_s_register(m);
          float sd_value = 0.0;
          int rounding_mode = instr->Bits(17, 16);
          switch (rounding_mode) {
            case 0x0:  // vrinta - round with ties to away from zero
              sd_value = roundf(sm_value);
              break;
            case 0x1: {  // vrintn - round with ties to even
              sd_value = nearbyintf(sm_value);
              break;
            }
            case 0x2:  // vrintp - ceil
              sd_value = ceilf(sm_value);
              break;
            case 0x3:  // vrintm - floor
              sd_value = floorf(sm_value);
              break;
            default:
              UNREACHABLE();  // Case analysis is exhaustive.
              break;
          }
          sd_value = canonicalizeNaN(sd_value);
          set_s_register_from_float(d, sd_value);
        }
      } else if ((instr->Opc1Value() == 0x4) && (instr->Bits(11, 9) == 0x5) &&
                 (instr->Bit(4) == 0x0)) {
        if (instr->SzValue() == 0x1) {
          int m = instr->VFPMRegValue(kDoublePrecision);
          int n = instr->VFPNRegValue(kDoublePrecision);
          int d = instr->VFPDRegValue(kDoublePrecision);
          double dn_value = get_double_from_d_register(n);
          double dm_value = get_double_from_d_register(m);
          double dd_value;
          if (instr->Bit(6) == 0x1) {  // vminnm
            if ((dn_value < dm_value) || std::isnan(dm_value)) {
              dd_value = dn_value;
            } else if ((dm_value < dn_value) || std::isnan(dn_value)) {
              dd_value = dm_value;
            } else {
              DCHECK_EQ(dn_value, dm_value);
              // Make sure that we pick the most negative sign for +/-0.
              dd_value = std::signbit(dn_value) ? dn_value : dm_value;
            }
          } else {  // vmaxnm
            if ((dn_value > dm_value) || std::isnan(dm_value)) {
              dd_value = dn_value;
            } else if ((dm_value > dn_value) || std::isnan(dn_value)) {
              dd_value = dm_value;
            } else {
              DCHECK_EQ(dn_value, dm_value);
              // Make sure that we pick the most positive sign for +/-0.
              dd_value = std::signbit(dn_value) ? dm_value : dn_value;
            }
          }
          dd_value = canonicalizeNaN(dd_value);
          set_d_register_from_double(d, dd_value);
        } else {
          int m = instr->VFPMRegValue(kSinglePrecision);
          int n = instr->VFPNRegValue(kSinglePrecision);
          int d = instr->VFPDRegValue(kSinglePrecision);
          float sn_value = get_float_from_s_register(n);
          float sm_value = get_float_from_s_register(m);
          float sd_value;
          if (instr->Bit(6) == 0x1) {  // vminnm
            if ((sn_value < sm_value) || std::isnan(sm_value)) {
              sd_value = sn_value;
            } else if ((sm_value < sn_value) || std::isnan(sn_value)) {
              sd_value = sm_value;
            } else {
              DCHECK_EQ(sn_value, sm_value);
              // Make sure that we pick the most negative sign for +/-0.
              sd_value = std::signbit(sn_value) ? sn_value : sm_value;
            }
          } else {  // vmaxnm
            if ((sn_value > sm_value) || std::isnan(sm_value)) {
              sd_value = sn_value;
            } else if ((sm_value > sn_value) || std::isnan(sn_value)) {
              sd_value = sm_value;
            } else {
              DCHECK_EQ(sn_value, sm_value);
              // Make sure that we pick the most positive sign for +/-0.
              sd_value = std::signbit(sn_value) ? sm_value : sn_value;
            }
          }
          sd_value = canonicalizeNaN(sd_value);
          set_s_register_from_float(d, sd_value);
        }
      } else {
        UNIMPLEMENTED();
      }
      break;
    case 0x1C:
      if ((instr->Bits(11, 9) == 0x5) && (instr->Bit(6) == 0) &&
          (instr->Bit(4) == 0)) {
        // VSEL* (floating-point)
        bool condition_holds;
        switch (instr->Bits(21, 20)) {
          case 0x0:  // VSELEQ
            condition_holds = (z_flag_ == 1);
            break;
          case 0x1:  // VSELVS
            condition_holds = (v_flag_ == 1);
            break;
          case 0x2:  // VSELGE
            condition_holds = (n_flag_ == v_flag_);
            break;
          case 0x3:  // VSELGT
            condition_holds = ((z_flag_ == 0) && (n_flag_ == v_flag_));
            break;
          default:
            UNREACHABLE();  // Case analysis is exhaustive.
            break;
        }
        if (instr->SzValue() == 0x1) {
          int n = instr->VFPNRegValue(kDoublePrecision);
          int m = instr->VFPMRegValue(kDoublePrecision);
          int d = instr->VFPDRegValue(kDoublePrecision);
          double result = get_double_from_d_register(condition_holds ? n : m);
          set_d_register_from_double(d, result);
        } else {
          int n = instr->VFPNRegValue(kSinglePrecision);
          int m = instr->VFPMRegValue(kSinglePrecision);
          int d = instr->VFPDRegValue(kSinglePrecision);
          float result = get_float_from_s_register(condition_holds ? n : m);
          set_s_register_from_float(d, result);
        }
      } else {
        UNIMPLEMENTED();
      }
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
}


// Executes the current instruction.
void Simulator::InstructionDecode(Instruction* instr) {
  if (v8::internal::FLAG_check_icache) {
    CheckICache(isolate_->simulator_i_cache(), instr);
  }
  pc_modified_ = false;
  if (::v8::internal::FLAG_trace_sim) {
    disasm::NameConverter converter;
    disasm::Disassembler dasm(converter);
    // use a reasonably large buffer
    v8::internal::EmbeddedVector<char, 256> buffer;
    dasm.InstructionDecode(buffer,
                           reinterpret_cast<byte*>(instr));
    PrintF("  0x%08" V8PRIxPTR "  %s\n", reinterpret_cast<intptr_t>(instr),
           buffer.start());
  }
  if (instr->ConditionField() == kSpecialCondition) {
    DecodeSpecialCondition(instr);
  } else if (ConditionallyExecute(instr)) {
    switch (instr->TypeValue()) {
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
  // If the instruction is a non taken conditional stop, we need to skip the
  // inlined message address.
  } else if (instr->IsStop()) {
    set_pc(get_pc() + 2 * Instruction::kInstrSize);
  }
  if (!pc_modified_) {
    set_register(pc, reinterpret_cast<int32_t>(instr)
                         + Instruction::kInstrSize);
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
        ArmDebugger dbg(this);
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

  // Set up the callee-saved registers with a known value. To be able to check
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
}


int32_t Simulator::Call(byte* entry, int argument_count, ...) {
  va_list parameters;
  va_start(parameters, argument_count);
  // Set up arguments

  // First four arguments passed in registers.
  DCHECK(argument_count >= 4);
  set_register(r0, va_arg(parameters, int32_t));
  set_register(r1, va_arg(parameters, int32_t));
  set_register(r2, va_arg(parameters, int32_t));
  set_register(r3, va_arg(parameters, int32_t));

  // Remaining arguments passed on stack.
  int original_stack = get_register(sp);
  // Compute position of stack on entry to generated code.
  int entry_stack = (original_stack - (argument_count - 4) * sizeof(int32_t));
  if (base::OS::ActivationFrameAlignment() != 0) {
    entry_stack &= -base::OS::ActivationFrameAlignment();
  }
  // Store remaining arguments on stack, from low to high memory.
  intptr_t* stack_argument = reinterpret_cast<intptr_t*>(entry_stack);
  for (int i = 4; i < argument_count; i++) {
    stack_argument[i - 4] = va_arg(parameters, int32_t);
  }
  va_end(parameters);
  set_register(sp, entry_stack);

  CallInternal(entry);

  // Pop stack passed arguments.
  CHECK_EQ(entry_stack, get_register(sp));
  set_register(sp, original_stack);

  int32_t result = get_register(r0);
  return result;
}


void Simulator::CallFP(byte* entry, double d0, double d1) {
  if (use_eabi_hardfloat()) {
    set_d_register_from_double(0, d0);
    set_d_register_from_double(1, d1);
  } else {
    set_register_pair_from_double(0, &d0);
    set_register_pair_from_double(2, &d1);
  }
  CallInternal(entry);
}


int32_t Simulator::CallFPReturnsInt(byte* entry, double d0, double d1) {
  CallFP(entry, d0, d1);
  int32_t result = get_register(r0);
  return result;
}


double Simulator::CallFPReturnsDouble(byte* entry, double d0, double d1) {
  CallFP(entry, d0, d1);
  if (use_eabi_hardfloat()) {
    return get_double_from_d_register(0);
  } else {
    return get_double_from_register_pair(0);
  }
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

}  // namespace internal
}  // namespace v8

#endif  // USE_SIMULATOR

#endif  // V8_TARGET_ARCH_ARM
