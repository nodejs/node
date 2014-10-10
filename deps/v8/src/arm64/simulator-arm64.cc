// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <cmath>
#include <cstdarg>
#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/decoder-arm64-inl.h"
#include "src/arm64/simulator-arm64.h"
#include "src/assembler.h"
#include "src/disasm.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

#if defined(USE_SIMULATOR)


// This macro provides a platform independent use of sscanf. The reason for
// SScanF not being implemented in a platform independent way through
// ::v8::internal::OS in the same way as SNPrintF is that the
// Windows C Run-Time Library does not provide vsscanf.
#define SScanF sscanf  // NOLINT


// Helpers for colors.
#define COLOUR(colour_code)       "\033[0;" colour_code "m"
#define COLOUR_BOLD(colour_code)  "\033[1;" colour_code "m"
#define NORMAL  ""
#define GREY    "30"
#define RED     "31"
#define GREEN   "32"
#define YELLOW  "33"
#define BLUE    "34"
#define MAGENTA "35"
#define CYAN    "36"
#define WHITE   "37"
typedef char const * const TEXT_COLOUR;
TEXT_COLOUR clr_normal         = FLAG_log_colour ? COLOUR(NORMAL)       : "";
TEXT_COLOUR clr_flag_name      = FLAG_log_colour ? COLOUR_BOLD(WHITE)   : "";
TEXT_COLOUR clr_flag_value     = FLAG_log_colour ? COLOUR(NORMAL)       : "";
TEXT_COLOUR clr_reg_name       = FLAG_log_colour ? COLOUR_BOLD(CYAN)    : "";
TEXT_COLOUR clr_reg_value      = FLAG_log_colour ? COLOUR(CYAN)         : "";
TEXT_COLOUR clr_fpreg_name     = FLAG_log_colour ? COLOUR_BOLD(MAGENTA) : "";
TEXT_COLOUR clr_fpreg_value    = FLAG_log_colour ? COLOUR(MAGENTA)      : "";
TEXT_COLOUR clr_memory_address = FLAG_log_colour ? COLOUR_BOLD(BLUE)    : "";
TEXT_COLOUR clr_debug_number   = FLAG_log_colour ? COLOUR_BOLD(YELLOW)  : "";
TEXT_COLOUR clr_debug_message  = FLAG_log_colour ? COLOUR(YELLOW)       : "";
TEXT_COLOUR clr_printf         = FLAG_log_colour ? COLOUR(GREEN)        : "";


// This is basically the same as PrintF, with a guard for FLAG_trace_sim.
void Simulator::TraceSim(const char* format, ...) {
  if (FLAG_trace_sim) {
    va_list arguments;
    va_start(arguments, format);
    base::OS::VFPrint(stream_, format, arguments);
    va_end(arguments);
  }
}


const Instruction* Simulator::kEndOfSimAddress = NULL;


void SimSystemRegister::SetBits(int msb, int lsb, uint32_t bits) {
  int width = msb - lsb + 1;
  DCHECK(is_uintn(bits, width) || is_intn(bits, width));

  bits <<= lsb;
  uint32_t mask = ((1 << width) - 1) << lsb;
  DCHECK((mask & write_ignore_mask_) == 0);

  value_ = (value_ & ~mask) | (bits & mask);
}


SimSystemRegister SimSystemRegister::DefaultValueFor(SystemRegister id) {
  switch (id) {
    case NZCV:
      return SimSystemRegister(0x00000000, NZCVWriteIgnoreMask);
    case FPCR:
      return SimSystemRegister(0x00000000, FPCRWriteIgnoreMask);
    default:
      UNREACHABLE();
      return SimSystemRegister();
  }
}


void Simulator::Initialize(Isolate* isolate) {
  if (isolate->simulator_initialized()) return;
  isolate->set_simulator_initialized(true);
  ExternalReference::set_redirector(isolate, &RedirectExternalReference);
}


// Get the active Simulator for the current thread.
Simulator* Simulator::current(Isolate* isolate) {
  Isolate::PerIsolateThreadData* isolate_data =
      isolate->FindOrAllocatePerThreadDataForThisThread();
  DCHECK(isolate_data != NULL);

  Simulator* sim = isolate_data->simulator();
  if (sim == NULL) {
    if (FLAG_trace_sim || FLAG_log_instruction_stats || FLAG_debug_sim) {
      sim = new Simulator(new Decoder<DispatchingDecoderVisitor>(), isolate);
    } else {
      sim = new Decoder<Simulator>();
      sim->isolate_ = isolate;
    }
    isolate_data->set_simulator(sim);
  }
  return sim;
}


void Simulator::CallVoid(byte* entry, CallArgument* args) {
  int index_x = 0;
  int index_d = 0;

  std::vector<int64_t> stack_args(0);
  for (int i = 0; !args[i].IsEnd(); i++) {
    CallArgument arg = args[i];
    if (arg.IsX() && (index_x < 8)) {
      set_xreg(index_x++, arg.bits());
    } else if (arg.IsD() && (index_d < 8)) {
      set_dreg_bits(index_d++, arg.bits());
    } else {
      DCHECK(arg.IsD() || arg.IsX());
      stack_args.push_back(arg.bits());
    }
  }

  // Process stack arguments, and make sure the stack is suitably aligned.
  uintptr_t original_stack = sp();
  uintptr_t entry_stack = original_stack -
                          stack_args.size() * sizeof(stack_args[0]);
  if (base::OS::ActivationFrameAlignment() != 0) {
    entry_stack &= -base::OS::ActivationFrameAlignment();
  }
  char * stack = reinterpret_cast<char*>(entry_stack);
  std::vector<int64_t>::const_iterator it;
  for (it = stack_args.begin(); it != stack_args.end(); it++) {
    memcpy(stack, &(*it), sizeof(*it));
    stack += sizeof(*it);
  }

  DCHECK(reinterpret_cast<uintptr_t>(stack) <= original_stack);
  set_sp(entry_stack);

  // Call the generated code.
  set_pc(entry);
  set_lr(kEndOfSimAddress);
  CheckPCSComplianceAndRun();

  set_sp(original_stack);
}


int64_t Simulator::CallInt64(byte* entry, CallArgument* args) {
  CallVoid(entry, args);
  return xreg(0);
}


double Simulator::CallDouble(byte* entry, CallArgument* args) {
  CallVoid(entry, args);
  return dreg(0);
}


int64_t Simulator::CallJS(byte* entry,
                          byte* function_entry,
                          JSFunction* func,
                          Object* revc,
                          int64_t argc,
                          Object*** argv) {
  CallArgument args[] = {
    CallArgument(function_entry),
    CallArgument(func),
    CallArgument(revc),
    CallArgument(argc),
    CallArgument(argv),
    CallArgument::End()
  };
  return CallInt64(entry, args);
}

int64_t Simulator::CallRegExp(byte* entry,
                              String* input,
                              int64_t start_offset,
                              const byte* input_start,
                              const byte* input_end,
                              int* output,
                              int64_t output_size,
                              Address stack_base,
                              int64_t direct_call,
                              void* return_address,
                              Isolate* isolate) {
  CallArgument args[] = {
    CallArgument(input),
    CallArgument(start_offset),
    CallArgument(input_start),
    CallArgument(input_end),
    CallArgument(output),
    CallArgument(output_size),
    CallArgument(stack_base),
    CallArgument(direct_call),
    CallArgument(return_address),
    CallArgument(isolate),
    CallArgument::End()
  };
  return CallInt64(entry, args);
}


void Simulator::CheckPCSComplianceAndRun() {
#ifdef DEBUG
  CHECK_EQ(kNumberOfCalleeSavedRegisters, kCalleeSaved.Count());
  CHECK_EQ(kNumberOfCalleeSavedFPRegisters, kCalleeSavedFP.Count());

  int64_t saved_registers[kNumberOfCalleeSavedRegisters];
  uint64_t saved_fpregisters[kNumberOfCalleeSavedFPRegisters];

  CPURegList register_list = kCalleeSaved;
  CPURegList fpregister_list = kCalleeSavedFP;

  for (int i = 0; i < kNumberOfCalleeSavedRegisters; i++) {
    // x31 is not a caller saved register, so no need to specify if we want
    // the stack or zero.
    saved_registers[i] = xreg(register_list.PopLowestIndex().code());
  }
  for (int i = 0; i < kNumberOfCalleeSavedFPRegisters; i++) {
    saved_fpregisters[i] =
        dreg_bits(fpregister_list.PopLowestIndex().code());
  }
  int64_t original_stack = sp();
#endif
  // Start the simulation!
  Run();
#ifdef DEBUG
  CHECK_EQ(original_stack, sp());
  // Check that callee-saved registers have been preserved.
  register_list = kCalleeSaved;
  fpregister_list = kCalleeSavedFP;
  for (int i = 0; i < kNumberOfCalleeSavedRegisters; i++) {
    CHECK_EQ(saved_registers[i], xreg(register_list.PopLowestIndex().code()));
  }
  for (int i = 0; i < kNumberOfCalleeSavedFPRegisters; i++) {
    DCHECK(saved_fpregisters[i] ==
           dreg_bits(fpregister_list.PopLowestIndex().code()));
  }

  // Corrupt caller saved register minus the return regiters.

  // In theory x0 to x7 can be used for return values, but V8 only uses x0, x1
  // for now .
  register_list = kCallerSaved;
  register_list.Remove(x0);
  register_list.Remove(x1);

  // In theory d0 to d7 can be used for return values, but V8 only uses d0
  // for now .
  fpregister_list = kCallerSavedFP;
  fpregister_list.Remove(d0);

  CorruptRegisters(&register_list, kCallerSavedRegisterCorruptionValue);
  CorruptRegisters(&fpregister_list, kCallerSavedFPRegisterCorruptionValue);
#endif
}


#ifdef DEBUG
// The least significant byte of the curruption value holds the corresponding
// register's code.
void Simulator::CorruptRegisters(CPURegList* list, uint64_t value) {
  if (list->type() == CPURegister::kRegister) {
    while (!list->IsEmpty()) {
      unsigned code = list->PopLowestIndex().code();
      set_xreg(code, value | code);
    }
  } else {
    DCHECK(list->type() == CPURegister::kFPRegister);
    while (!list->IsEmpty()) {
      unsigned code = list->PopLowestIndex().code();
      set_dreg_bits(code, value | code);
    }
  }
}


void Simulator::CorruptAllCallerSavedCPURegisters() {
  // Corrupt alters its parameter so copy them first.
  CPURegList register_list = kCallerSaved;
  CPURegList fpregister_list = kCallerSavedFP;

  CorruptRegisters(&register_list, kCallerSavedRegisterCorruptionValue);
  CorruptRegisters(&fpregister_list, kCallerSavedFPRegisterCorruptionValue);
}
#endif


// Extending the stack by 2 * 64 bits is required for stack alignment purposes.
uintptr_t Simulator::PushAddress(uintptr_t address) {
  DCHECK(sizeof(uintptr_t) < 2 * kXRegSize);
  intptr_t new_sp = sp() - 2 * kXRegSize;
  uintptr_t* alignment_slot =
    reinterpret_cast<uintptr_t*>(new_sp + kXRegSize);
  memcpy(alignment_slot, &kSlotsZapValue, kPointerSize);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(new_sp);
  memcpy(stack_slot, &address, kPointerSize);
  set_sp(new_sp);
  return new_sp;
}


uintptr_t Simulator::PopAddress() {
  intptr_t current_sp = sp();
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(current_sp);
  uintptr_t address = *stack_slot;
  DCHECK(sizeof(uintptr_t) < 2 * kXRegSize);
  set_sp(current_sp + 2 * kXRegSize);
  return address;
}


// Returns the limit of the stack area to enable checking for stack overflows.
uintptr_t Simulator::StackLimit() const {
  // Leave a safety margin of 1024 bytes to prevent overrunning the stack when
  // pushing values.
  return stack_limit_ + 1024;
}


Simulator::Simulator(Decoder<DispatchingDecoderVisitor>* decoder,
                     Isolate* isolate, FILE* stream)
    : decoder_(decoder),
      last_debugger_input_(NULL),
      log_parameters_(NO_PARAM),
      isolate_(isolate) {
  // Setup the decoder.
  decoder_->AppendVisitor(this);

  Init(stream);

  if (FLAG_trace_sim) {
    decoder_->InsertVisitorBefore(print_disasm_, this);
    log_parameters_ = LOG_ALL;
  }

  if (FLAG_log_instruction_stats) {
    instrument_ = new Instrument(FLAG_log_instruction_file,
                                 FLAG_log_instruction_period);
    decoder_->AppendVisitor(instrument_);
  }
}


Simulator::Simulator()
    : decoder_(NULL),
      last_debugger_input_(NULL),
      log_parameters_(NO_PARAM),
      isolate_(NULL) {
  Init(stdout);
  CHECK(!FLAG_trace_sim && !FLAG_log_instruction_stats);
}


void Simulator::Init(FILE* stream) {
  ResetState();

  // Allocate and setup the simulator stack.
  stack_size_ = (FLAG_sim_stack_size * KB) + (2 * stack_protection_size_);
  stack_ = reinterpret_cast<uintptr_t>(new byte[stack_size_]);
  stack_limit_ = stack_ + stack_protection_size_;
  uintptr_t tos = stack_ + stack_size_ - stack_protection_size_;
  // The stack pointer must be 16-byte aligned.
  set_sp(tos & ~0xfUL);

  stream_ = stream;
  print_disasm_ = new PrintDisassembler(stream_);

  // The debugger needs to disassemble code without the simulator executing an
  // instruction, so we create a dedicated decoder.
  disassembler_decoder_ = new Decoder<DispatchingDecoderVisitor>();
  disassembler_decoder_->AppendVisitor(print_disasm_);
}


void Simulator::ResetState() {
  // Reset the system registers.
  nzcv_ = SimSystemRegister::DefaultValueFor(NZCV);
  fpcr_ = SimSystemRegister::DefaultValueFor(FPCR);

  // Reset registers to 0.
  pc_ = NULL;
  for (unsigned i = 0; i < kNumberOfRegisters; i++) {
    set_xreg(i, 0xbadbeef);
  }
  for (unsigned i = 0; i < kNumberOfFPRegisters; i++) {
    // Set FP registers to a value that is NaN in both 32-bit and 64-bit FP.
    set_dreg_bits(i, 0x7ff000007f800001UL);
  }
  // Returning to address 0 exits the Simulator.
  set_lr(kEndOfSimAddress);

  // Reset debug helpers.
  breakpoints_.empty();
  break_on_next_= false;
}


Simulator::~Simulator() {
  delete[] reinterpret_cast<byte*>(stack_);
  if (FLAG_log_instruction_stats) {
    delete instrument_;
  }
  delete disassembler_decoder_;
  delete print_disasm_;
  DeleteArray(last_debugger_input_);
  delete decoder_;
}


void Simulator::Run() {
  pc_modified_ = false;
  while (pc_ != kEndOfSimAddress) {
    ExecuteInstruction();
  }
}


void Simulator::RunFrom(Instruction* start) {
  set_pc(start);
  Run();
}


// When the generated code calls an external reference we need to catch that in
// the simulator.  The external reference will be a function compiled for the
// host architecture.  We need to call that function instead of trying to
// execute it with the simulator.  We do that by redirecting the external
// reference to a svc (Supervisor Call) instruction that is handled by
// the simulator.  We write the original destination of the jump just at a known
// offset from the svc instruction so the simulator knows what to call.
class Redirection {
 public:
  Redirection(void* external_function, ExternalReference::Type type)
      : external_function_(external_function),
        type_(type),
        next_(NULL) {
    redirect_call_.SetInstructionBits(
        HLT | Assembler::ImmException(kImmExceptionIsRedirectedCall));
    Isolate* isolate = Isolate::Current();
    next_ = isolate->simulator_redirection();
    // TODO(all): Simulator flush I cache
    isolate->set_simulator_redirection(this);
  }

  void* address_of_redirect_call() {
    return reinterpret_cast<void*>(&redirect_call_);
  }

  template <typename T>
  T external_function() { return reinterpret_cast<T>(external_function_); }

  ExternalReference::Type type() { return type_; }

  static Redirection* Get(void* external_function,
                          ExternalReference::Type type) {
    Isolate* isolate = Isolate::Current();
    Redirection* current = isolate->simulator_redirection();
    for (; current != NULL; current = current->next_) {
      if (current->external_function_ == external_function) {
        DCHECK_EQ(current->type(), type);
        return current;
      }
    }
    return new Redirection(external_function, type);
  }

  static Redirection* FromHltInstruction(Instruction* redirect_call) {
    char* addr_of_hlt = reinterpret_cast<char*>(redirect_call);
    char* addr_of_redirection =
        addr_of_hlt - OFFSET_OF(Redirection, redirect_call_);
    return reinterpret_cast<Redirection*>(addr_of_redirection);
  }

  static void* ReverseRedirection(int64_t reg) {
    Redirection* redirection =
        FromHltInstruction(reinterpret_cast<Instruction*>(reg));
    return redirection->external_function<void*>();
  }

 private:
  void* external_function_;
  Instruction redirect_call_;
  ExternalReference::Type type_;
  Redirection* next_;
};


// Calls into the V8 runtime are based on this very simple interface.
// Note: To be able to return two values from some calls the code in runtime.cc
// uses the ObjectPair structure.
// The simulator assumes all runtime calls return two 64-bits values. If they
// don't, register x1 is clobbered. This is fine because x1 is caller-saved.
struct ObjectPair {
  int64_t res0;
  int64_t res1;
};


typedef ObjectPair (*SimulatorRuntimeCall)(int64_t arg0,
                                           int64_t arg1,
                                           int64_t arg2,
                                           int64_t arg3,
                                           int64_t arg4,
                                           int64_t arg5,
                                           int64_t arg6,
                                           int64_t arg7);

typedef int64_t (*SimulatorRuntimeCompareCall)(double arg1, double arg2);
typedef double (*SimulatorRuntimeFPFPCall)(double arg1, double arg2);
typedef double (*SimulatorRuntimeFPCall)(double arg1);
typedef double (*SimulatorRuntimeFPIntCall)(double arg1, int32_t arg2);

// This signature supports direct call in to API function native callback
// (refer to InvocationCallback in v8.h).
typedef void (*SimulatorRuntimeDirectApiCall)(int64_t arg0);
typedef void (*SimulatorRuntimeProfilingApiCall)(int64_t arg0, void* arg1);

// This signature supports direct call to accessor getter callback.
typedef void (*SimulatorRuntimeDirectGetterCall)(int64_t arg0, int64_t arg1);
typedef void (*SimulatorRuntimeProfilingGetterCall)(int64_t arg0, int64_t arg1,
                                                    void* arg2);

void Simulator::DoRuntimeCall(Instruction* instr) {
  Redirection* redirection = Redirection::FromHltInstruction(instr);

  // The called C code might itself call simulated code, so any
  // caller-saved registers (including lr) could still be clobbered by a
  // redirected call.
  Instruction* return_address = lr();

  int64_t external = redirection->external_function<int64_t>();

  TraceSim("Call to host function at %p\n",
           redirection->external_function<void*>());

  // SP must be 16-byte-aligned at the call interface.
  bool stack_alignment_exception = ((sp() & 0xf) != 0);
  if (stack_alignment_exception) {
    TraceSim("  with unaligned stack 0x%016" PRIx64 ".\n", sp());
    FATAL("ALIGNMENT EXCEPTION");
  }

  switch (redirection->type()) {
    default:
      TraceSim("Type: Unknown.\n");
      UNREACHABLE();
      break;

    case ExternalReference::BUILTIN_CALL: {
      // Object* f(v8::internal::Arguments).
      TraceSim("Type: BUILTIN_CALL\n");
      SimulatorRuntimeCall target =
        reinterpret_cast<SimulatorRuntimeCall>(external);

      // We don't know how many arguments are being passed, but we can
      // pass 8 without touching the stack. They will be ignored by the
      // host function if they aren't used.
      TraceSim("Arguments: "
               "0x%016" PRIx64 ", 0x%016" PRIx64 ", "
               "0x%016" PRIx64 ", 0x%016" PRIx64 ", "
               "0x%016" PRIx64 ", 0x%016" PRIx64 ", "
               "0x%016" PRIx64 ", 0x%016" PRIx64,
               xreg(0), xreg(1), xreg(2), xreg(3),
               xreg(4), xreg(5), xreg(6), xreg(7));
      ObjectPair result = target(xreg(0), xreg(1), xreg(2), xreg(3),
                                 xreg(4), xreg(5), xreg(6), xreg(7));
      TraceSim("Returned: {0x%" PRIx64 ", 0x%" PRIx64 "}\n",
               result.res0, result.res1);
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      set_xreg(0, result.res0);
      set_xreg(1, result.res1);
      break;
    }

    case ExternalReference::DIRECT_API_CALL: {
      // void f(v8::FunctionCallbackInfo&)
      TraceSim("Type: DIRECT_API_CALL\n");
      SimulatorRuntimeDirectApiCall target =
        reinterpret_cast<SimulatorRuntimeDirectApiCall>(external);
      TraceSim("Arguments: 0x%016" PRIx64 "\n", xreg(0));
      target(xreg(0));
      TraceSim("No return value.");
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      break;
    }

    case ExternalReference::BUILTIN_COMPARE_CALL: {
      // int f(double, double)
      TraceSim("Type: BUILTIN_COMPARE_CALL\n");
      SimulatorRuntimeCompareCall target =
        reinterpret_cast<SimulatorRuntimeCompareCall>(external);
      TraceSim("Arguments: %f, %f\n", dreg(0), dreg(1));
      int64_t result = target(dreg(0), dreg(1));
      TraceSim("Returned: %" PRId64 "\n", result);
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      set_xreg(0, result);
      break;
    }

    case ExternalReference::BUILTIN_FP_CALL: {
      // double f(double)
      TraceSim("Type: BUILTIN_FP_CALL\n");
      SimulatorRuntimeFPCall target =
        reinterpret_cast<SimulatorRuntimeFPCall>(external);
      TraceSim("Argument: %f\n", dreg(0));
      double result = target(dreg(0));
      TraceSim("Returned: %f\n", result);
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      set_dreg(0, result);
      break;
    }

    case ExternalReference::BUILTIN_FP_FP_CALL: {
      // double f(double, double)
      TraceSim("Type: BUILTIN_FP_FP_CALL\n");
      SimulatorRuntimeFPFPCall target =
        reinterpret_cast<SimulatorRuntimeFPFPCall>(external);
      TraceSim("Arguments: %f, %f\n", dreg(0), dreg(1));
      double result = target(dreg(0), dreg(1));
      TraceSim("Returned: %f\n", result);
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      set_dreg(0, result);
      break;
    }

    case ExternalReference::BUILTIN_FP_INT_CALL: {
      // double f(double, int)
      TraceSim("Type: BUILTIN_FP_INT_CALL\n");
      SimulatorRuntimeFPIntCall target =
        reinterpret_cast<SimulatorRuntimeFPIntCall>(external);
      TraceSim("Arguments: %f, %d\n", dreg(0), wreg(0));
      double result = target(dreg(0), wreg(0));
      TraceSim("Returned: %f\n", result);
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      set_dreg(0, result);
      break;
    }

    case ExternalReference::DIRECT_GETTER_CALL: {
      // void f(Local<String> property, PropertyCallbackInfo& info)
      TraceSim("Type: DIRECT_GETTER_CALL\n");
      SimulatorRuntimeDirectGetterCall target =
        reinterpret_cast<SimulatorRuntimeDirectGetterCall>(external);
      TraceSim("Arguments: 0x%016" PRIx64 ", 0x%016" PRIx64 "\n",
               xreg(0), xreg(1));
      target(xreg(0), xreg(1));
      TraceSim("No return value.");
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      break;
    }

    case ExternalReference::PROFILING_API_CALL: {
      // void f(v8::FunctionCallbackInfo&, v8::FunctionCallback)
      TraceSim("Type: PROFILING_API_CALL\n");
      SimulatorRuntimeProfilingApiCall target =
        reinterpret_cast<SimulatorRuntimeProfilingApiCall>(external);
      void* arg1 = Redirection::ReverseRedirection(xreg(1));
      TraceSim("Arguments: 0x%016" PRIx64 ", %p\n", xreg(0), arg1);
      target(xreg(0), arg1);
      TraceSim("No return value.");
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      break;
    }

    case ExternalReference::PROFILING_GETTER_CALL: {
      // void f(Local<String> property, PropertyCallbackInfo& info,
      //        AccessorNameGetterCallback callback)
      TraceSim("Type: PROFILING_GETTER_CALL\n");
      SimulatorRuntimeProfilingGetterCall target =
        reinterpret_cast<SimulatorRuntimeProfilingGetterCall>(
            external);
      void* arg2 = Redirection::ReverseRedirection(xreg(2));
      TraceSim("Arguments: 0x%016" PRIx64 ", 0x%016" PRIx64 ", %p\n",
               xreg(0), xreg(1), arg2);
      target(xreg(0), xreg(1), arg2);
      TraceSim("No return value.");
#ifdef DEBUG
      CorruptAllCallerSavedCPURegisters();
#endif
      break;
    }
  }

  set_lr(return_address);
  set_pc(return_address);
}


void* Simulator::RedirectExternalReference(void* external_function,
                                           ExternalReference::Type type) {
  Redirection* redirection = Redirection::Get(external_function, type);
  return redirection->address_of_redirect_call();
}


const char* Simulator::xreg_names[] = {
"x0",  "x1",  "x2",  "x3",  "x4",   "x5",  "x6",  "x7",
"x8",  "x9",  "x10", "x11", "x12",  "x13", "x14", "x15",
"ip0", "ip1", "x18", "x19", "x20",  "x21", "x22", "x23",
"x24", "x25", "x26", "cp",  "jssp", "fp",  "lr",  "xzr", "csp"};

const char* Simulator::wreg_names[] = {
"w0",  "w1",  "w2",  "w3",  "w4",    "w5",  "w6",  "w7",
"w8",  "w9",  "w10", "w11", "w12",   "w13", "w14", "w15",
"w16", "w17", "w18", "w19", "w20",   "w21", "w22", "w23",
"w24", "w25", "w26", "wcp", "wjssp", "wfp", "wlr", "wzr", "wcsp"};

const char* Simulator::sreg_names[] = {
"s0",  "s1",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
"s8",  "s9",  "s10", "s11", "s12", "s13", "s14", "s15",
"s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",
"s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31"};

const char* Simulator::dreg_names[] = {
"d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",
"d8",  "d9",  "d10", "d11", "d12", "d13", "d14", "d15",
"d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
"d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"};

const char* Simulator::vreg_names[] = {
"v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
"v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
"v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
"v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"};


const char* Simulator::WRegNameForCode(unsigned code, Reg31Mode mode) {
  STATIC_ASSERT(arraysize(Simulator::wreg_names) == (kNumberOfRegisters + 1));
  DCHECK(code < kNumberOfRegisters);
  // The modulo operator has no effect here, but it silences a broken GCC
  // warning about out-of-bounds array accesses.
  code %= kNumberOfRegisters;

  // If the code represents the stack pointer, index the name after zr.
  if ((code == kZeroRegCode) && (mode == Reg31IsStackPointer)) {
    code = kZeroRegCode + 1;
  }
  return wreg_names[code];
}


const char* Simulator::XRegNameForCode(unsigned code, Reg31Mode mode) {
  STATIC_ASSERT(arraysize(Simulator::xreg_names) == (kNumberOfRegisters + 1));
  DCHECK(code < kNumberOfRegisters);
  code %= kNumberOfRegisters;

  // If the code represents the stack pointer, index the name after zr.
  if ((code == kZeroRegCode) && (mode == Reg31IsStackPointer)) {
    code = kZeroRegCode + 1;
  }
  return xreg_names[code];
}


const char* Simulator::SRegNameForCode(unsigned code) {
  STATIC_ASSERT(arraysize(Simulator::sreg_names) == kNumberOfFPRegisters);
  DCHECK(code < kNumberOfFPRegisters);
  return sreg_names[code % kNumberOfFPRegisters];
}


const char* Simulator::DRegNameForCode(unsigned code) {
  STATIC_ASSERT(arraysize(Simulator::dreg_names) == kNumberOfFPRegisters);
  DCHECK(code < kNumberOfFPRegisters);
  return dreg_names[code % kNumberOfFPRegisters];
}


const char* Simulator::VRegNameForCode(unsigned code) {
  STATIC_ASSERT(arraysize(Simulator::vreg_names) == kNumberOfFPRegisters);
  DCHECK(code < kNumberOfFPRegisters);
  return vreg_names[code % kNumberOfFPRegisters];
}


int Simulator::CodeFromName(const char* name) {
  for (unsigned i = 0; i < kNumberOfRegisters; i++) {
    if ((strcmp(xreg_names[i], name) == 0) ||
        (strcmp(wreg_names[i], name) == 0)) {
      return i;
    }
  }
  for (unsigned i = 0; i < kNumberOfFPRegisters; i++) {
    if ((strcmp(vreg_names[i], name) == 0) ||
        (strcmp(dreg_names[i], name) == 0) ||
        (strcmp(sreg_names[i], name) == 0)) {
      return i;
    }
  }
  if ((strcmp("csp", name) == 0) || (strcmp("wcsp", name) == 0)) {
    return kSPRegInternalCode;
  }
  return -1;
}


// Helpers ---------------------------------------------------------------------
template <typename T>
T Simulator::AddWithCarry(bool set_flags,
                          T src1,
                          T src2,
                          T carry_in) {
  typedef typename make_unsigned<T>::type unsignedT;
  DCHECK((carry_in == 0) || (carry_in == 1));

  T signed_sum = src1 + src2 + carry_in;
  T result = signed_sum;

  bool N, Z, C, V;

  // Compute the C flag
  unsignedT u1 = static_cast<unsignedT>(src1);
  unsignedT u2 = static_cast<unsignedT>(src2);
  unsignedT urest = std::numeric_limits<unsignedT>::max() - u1;
  C = (u2 > urest) || (carry_in && (((u2 + 1) > urest) || (u2 > (urest - 1))));

  // Overflow iff the sign bit is the same for the two inputs and different
  // for the result.
  V = ((src1 ^ src2) >= 0) && ((src1 ^ result) < 0);

  N = CalcNFlag(result);
  Z = CalcZFlag(result);

  if (set_flags) {
    nzcv().SetN(N);
    nzcv().SetZ(Z);
    nzcv().SetC(C);
    nzcv().SetV(V);
    LogSystemRegister(NZCV);
  }
  return result;
}


template<typename T>
void Simulator::AddSubWithCarry(Instruction* instr) {
  T op2 = reg<T>(instr->Rm());
  T new_val;

  if ((instr->Mask(AddSubOpMask) == SUB) || instr->Mask(AddSubOpMask) == SUBS) {
    op2 = ~op2;
  }

  new_val = AddWithCarry<T>(instr->FlagsUpdate(),
                            reg<T>(instr->Rn()),
                            op2,
                            nzcv().C());

  set_reg<T>(instr->Rd(), new_val);
}

template <typename T>
T Simulator::ShiftOperand(T value, Shift shift_type, unsigned amount) {
  typedef typename make_unsigned<T>::type unsignedT;

  if (amount == 0) {
    return value;
  }

  switch (shift_type) {
    case LSL:
      return value << amount;
    case LSR:
      return static_cast<unsignedT>(value) >> amount;
    case ASR:
      return value >> amount;
    case ROR:
      return (static_cast<unsignedT>(value) >> amount) |
              ((value & ((1L << amount) - 1L)) <<
                  (sizeof(unsignedT) * 8 - amount));
    default:
      UNIMPLEMENTED();
      return 0;
  }
}


template <typename T>
T Simulator::ExtendValue(T value, Extend extend_type, unsigned left_shift) {
  const unsigned kSignExtendBShift = (sizeof(T) - 1) * 8;
  const unsigned kSignExtendHShift = (sizeof(T) - 2) * 8;
  const unsigned kSignExtendWShift = (sizeof(T) - 4) * 8;

  switch (extend_type) {
    case UXTB:
      value &= kByteMask;
      break;
    case UXTH:
      value &= kHalfWordMask;
      break;
    case UXTW:
      value &= kWordMask;
      break;
    case SXTB:
      value = (value << kSignExtendBShift) >> kSignExtendBShift;
      break;
    case SXTH:
      value = (value << kSignExtendHShift) >> kSignExtendHShift;
      break;
    case SXTW:
      value = (value << kSignExtendWShift) >> kSignExtendWShift;
      break;
    case UXTX:
    case SXTX:
      break;
    default:
      UNREACHABLE();
  }
  return value << left_shift;
}


template <typename T>
void Simulator::Extract(Instruction* instr) {
  unsigned lsb = instr->ImmS();
  T op2 = reg<T>(instr->Rm());
  T result = op2;

  if (lsb) {
    T op1 = reg<T>(instr->Rn());
    result = op2 >> lsb | (op1 << ((sizeof(T) * 8) - lsb));
  }
  set_reg<T>(instr->Rd(), result);
}


template<> double Simulator::FPDefaultNaN<double>() const {
  return kFP64DefaultNaN;
}


template<> float Simulator::FPDefaultNaN<float>() const {
  return kFP32DefaultNaN;
}


void Simulator::FPCompare(double val0, double val1) {
  AssertSupportedFPCR();

  // TODO(jbramley): This assumes that the C++ implementation handles
  // comparisons in the way that we expect (as per AssertSupportedFPCR()).
  if ((std::isnan(val0) != 0) || (std::isnan(val1) != 0)) {
    nzcv().SetRawValue(FPUnorderedFlag);
  } else if (val0 < val1) {
    nzcv().SetRawValue(FPLessThanFlag);
  } else if (val0 > val1) {
    nzcv().SetRawValue(FPGreaterThanFlag);
  } else if (val0 == val1) {
    nzcv().SetRawValue(FPEqualFlag);
  } else {
    UNREACHABLE();
  }
  LogSystemRegister(NZCV);
}


void Simulator::SetBreakpoint(Instruction* location) {
  for (unsigned i = 0; i < breakpoints_.size(); i++) {
    if (breakpoints_.at(i).location == location) {
      PrintF(stream_,
             "Existing breakpoint at %p was %s\n",
             reinterpret_cast<void*>(location),
             breakpoints_.at(i).enabled ? "disabled" : "enabled");
      breakpoints_.at(i).enabled = !breakpoints_.at(i).enabled;
      return;
    }
  }
  Breakpoint new_breakpoint = {location, true};
  breakpoints_.push_back(new_breakpoint);
  PrintF(stream_,
         "Set a breakpoint at %p\n", reinterpret_cast<void*>(location));
}


void Simulator::ListBreakpoints() {
  PrintF(stream_, "Breakpoints:\n");
  for (unsigned i = 0; i < breakpoints_.size(); i++) {
    PrintF(stream_, "%p  : %s\n",
           reinterpret_cast<void*>(breakpoints_.at(i).location),
           breakpoints_.at(i).enabled ? "enabled" : "disabled");
  }
}


void Simulator::CheckBreakpoints() {
  bool hit_a_breakpoint = false;
  for (unsigned i = 0; i < breakpoints_.size(); i++) {
    if ((breakpoints_.at(i).location == pc_) &&
        breakpoints_.at(i).enabled) {
      hit_a_breakpoint = true;
      // Disable this breakpoint.
      breakpoints_.at(i).enabled = false;
    }
  }
  if (hit_a_breakpoint) {
    PrintF(stream_, "Hit and disabled a breakpoint at %p.\n",
           reinterpret_cast<void*>(pc_));
    Debug();
  }
}


void Simulator::CheckBreakNext() {
  // If the current instruction is a BL, insert a breakpoint just after it.
  if (break_on_next_ && pc_->IsBranchAndLinkToRegister()) {
    SetBreakpoint(pc_->following());
    break_on_next_ = false;
  }
}


void Simulator::PrintInstructionsAt(Instruction* start, uint64_t count) {
  Instruction* end = start->InstructionAtOffset(count * kInstructionSize);
  for (Instruction* pc = start; pc < end; pc = pc->following()) {
    disassembler_decoder_->Decode(pc);
  }
}


void Simulator::PrintSystemRegisters() {
  PrintSystemRegister(NZCV);
  PrintSystemRegister(FPCR);
}


void Simulator::PrintRegisters() {
  for (unsigned i = 0; i < kNumberOfRegisters; i++) {
    PrintRegister(i);
  }
}


void Simulator::PrintFPRegisters() {
  for (unsigned i = 0; i < kNumberOfFPRegisters; i++) {
    PrintFPRegister(i);
  }
}


void Simulator::PrintRegister(unsigned code, Reg31Mode r31mode) {
  // Don't print writes into xzr.
  if ((code == kZeroRegCode) && (r31mode == Reg31IsZeroRegister)) {
    return;
  }

  // The template is "# x<code>:value".
  fprintf(stream_, "# %s%5s: %s0x%016" PRIx64 "%s\n",
          clr_reg_name, XRegNameForCode(code, r31mode),
          clr_reg_value, reg<uint64_t>(code, r31mode), clr_normal);
}


void Simulator::PrintFPRegister(unsigned code, PrintFPRegisterSizes sizes) {
  // The template is "# v<code>:bits (d<code>:value, ...)".

  DCHECK(sizes != 0);
  DCHECK((sizes & kPrintAllFPRegValues) == sizes);

  // Print the raw bits.
  fprintf(stream_, "# %s%5s: %s0x%016" PRIx64 "%s (",
          clr_fpreg_name, VRegNameForCode(code),
          clr_fpreg_value, fpreg<uint64_t>(code), clr_normal);

  // Print all requested value interpretations.
  bool need_separator = false;
  if (sizes & kPrintDRegValue) {
    fprintf(stream_, "%s%s%s: %s%g%s",
            need_separator ? ", " : "",
            clr_fpreg_name, DRegNameForCode(code),
            clr_fpreg_value, fpreg<double>(code), clr_normal);
    need_separator = true;
  }

  if (sizes & kPrintSRegValue) {
    fprintf(stream_, "%s%s%s: %s%g%s",
            need_separator ? ", " : "",
            clr_fpreg_name, SRegNameForCode(code),
            clr_fpreg_value, fpreg<float>(code), clr_normal);
    need_separator = true;
  }

  // End the value list.
  fprintf(stream_, ")\n");
}


void Simulator::PrintSystemRegister(SystemRegister id) {
  switch (id) {
    case NZCV:
      fprintf(stream_, "# %sNZCV: %sN:%d Z:%d C:%d V:%d%s\n",
              clr_flag_name, clr_flag_value,
              nzcv().N(), nzcv().Z(), nzcv().C(), nzcv().V(),
              clr_normal);
      break;
    case FPCR: {
      static const char * rmode[] = {
        "0b00 (Round to Nearest)",
        "0b01 (Round towards Plus Infinity)",
        "0b10 (Round towards Minus Infinity)",
        "0b11 (Round towards Zero)"
      };
      DCHECK(fpcr().RMode() < arraysize(rmode));
      fprintf(stream_,
              "# %sFPCR: %sAHP:%d DN:%d FZ:%d RMode:%s%s\n",
              clr_flag_name, clr_flag_value,
              fpcr().AHP(), fpcr().DN(), fpcr().FZ(), rmode[fpcr().RMode()],
              clr_normal);
      break;
    }
    default:
      UNREACHABLE();
  }
}


void Simulator::PrintRead(uintptr_t address,
                          size_t size,
                          unsigned reg_code) {
  USE(size);  // Size is unused here.

  // The template is "# x<code>:value <- address".
  fprintf(stream_, "# %s%5s: %s0x%016" PRIx64 "%s",
          clr_reg_name, XRegNameForCode(reg_code),
          clr_reg_value, reg<uint64_t>(reg_code), clr_normal);

  fprintf(stream_, " <- %s0x%016" PRIxPTR "%s\n",
          clr_memory_address, address, clr_normal);
}


void Simulator::PrintReadFP(uintptr_t address,
                            size_t size,
                            unsigned reg_code) {
  // The template is "# reg:bits (reg:value) <- address".
  switch (size) {
    case kSRegSize:
      fprintf(stream_, "# %s%5s: %s0x%016" PRIx64 "%s (%s%s: %s%gf%s)",
              clr_fpreg_name, VRegNameForCode(reg_code),
              clr_fpreg_value, fpreg<uint64_t>(reg_code), clr_normal,
              clr_fpreg_name, SRegNameForCode(reg_code),
              clr_fpreg_value, fpreg<float>(reg_code), clr_normal);
      break;
    case kDRegSize:
      fprintf(stream_, "# %s%5s: %s0x%016" PRIx64 "%s (%s%s: %s%g%s)",
              clr_fpreg_name, VRegNameForCode(reg_code),
              clr_fpreg_value, fpreg<uint64_t>(reg_code), clr_normal,
              clr_fpreg_name, DRegNameForCode(reg_code),
              clr_fpreg_value, fpreg<double>(reg_code), clr_normal);
      break;
    default:
      UNREACHABLE();
  }

  fprintf(stream_, " <- %s0x%016" PRIxPTR "%s\n",
          clr_memory_address, address, clr_normal);
}


void Simulator::PrintWrite(uintptr_t address,
                           size_t size,
                           unsigned reg_code) {
  // The template is "# reg:value -> address". To keep the trace tidy and
  // readable, the value is aligned with the values in the register trace.
  switch (size) {
    case kByteSizeInBytes:
      fprintf(stream_, "# %s%5s<7:0>:          %s0x%02" PRIx8 "%s",
              clr_reg_name, WRegNameForCode(reg_code),
              clr_reg_value, reg<uint8_t>(reg_code), clr_normal);
      break;
    case kHalfWordSizeInBytes:
      fprintf(stream_, "# %s%5s<15:0>:       %s0x%04" PRIx16 "%s",
              clr_reg_name, WRegNameForCode(reg_code),
              clr_reg_value, reg<uint16_t>(reg_code), clr_normal);
      break;
    case kWRegSize:
      fprintf(stream_, "# %s%5s:         %s0x%08" PRIx32 "%s",
              clr_reg_name, WRegNameForCode(reg_code),
              clr_reg_value, reg<uint32_t>(reg_code), clr_normal);
      break;
    case kXRegSize:
      fprintf(stream_, "# %s%5s: %s0x%016" PRIx64 "%s",
              clr_reg_name, XRegNameForCode(reg_code),
              clr_reg_value, reg<uint64_t>(reg_code), clr_normal);
      break;
    default:
      UNREACHABLE();
  }

  fprintf(stream_, " -> %s0x%016" PRIxPTR "%s\n",
          clr_memory_address, address, clr_normal);
}


void Simulator::PrintWriteFP(uintptr_t address,
                             size_t size,
                             unsigned reg_code) {
  // The template is "# reg:bits (reg:value) -> address". To keep the trace tidy
  // and readable, the value is aligned with the values in the register trace.
  switch (size) {
    case kSRegSize:
      fprintf(stream_, "# %s%5s<31:0>:   %s0x%08" PRIx32 "%s (%s%s: %s%gf%s)",
              clr_fpreg_name, VRegNameForCode(reg_code),
              clr_fpreg_value, fpreg<uint32_t>(reg_code), clr_normal,
              clr_fpreg_name, SRegNameForCode(reg_code),
              clr_fpreg_value, fpreg<float>(reg_code), clr_normal);
      break;
    case kDRegSize:
      fprintf(stream_, "# %s%5s: %s0x%016" PRIx64 "%s (%s%s: %s%g%s)",
              clr_fpreg_name, VRegNameForCode(reg_code),
              clr_fpreg_value, fpreg<uint64_t>(reg_code), clr_normal,
              clr_fpreg_name, DRegNameForCode(reg_code),
              clr_fpreg_value, fpreg<double>(reg_code), clr_normal);
      break;
    default:
      UNREACHABLE();
  }

  fprintf(stream_, " -> %s0x%016" PRIxPTR "%s\n",
          clr_memory_address, address, clr_normal);
}


// Visitors---------------------------------------------------------------------

void Simulator::VisitUnimplemented(Instruction* instr) {
  fprintf(stream_, "Unimplemented instruction at %p: 0x%08" PRIx32 "\n",
          reinterpret_cast<void*>(instr), instr->InstructionBits());
  UNIMPLEMENTED();
}


void Simulator::VisitUnallocated(Instruction* instr) {
  fprintf(stream_, "Unallocated instruction at %p: 0x%08" PRIx32 "\n",
          reinterpret_cast<void*>(instr), instr->InstructionBits());
  UNIMPLEMENTED();
}


void Simulator::VisitPCRelAddressing(Instruction* instr) {
  switch (instr->Mask(PCRelAddressingMask)) {
    case ADR:
      set_reg(instr->Rd(), instr->ImmPCOffsetTarget());
      break;
    case ADRP:  // Not implemented in the assembler.
      UNIMPLEMENTED();
      break;
    default:
      UNREACHABLE();
      break;
  }
}


void Simulator::VisitUnconditionalBranch(Instruction* instr) {
  switch (instr->Mask(UnconditionalBranchMask)) {
    case BL:
      set_lr(instr->following());
      // Fall through.
    case B:
      set_pc(instr->ImmPCOffsetTarget());
      break;
    default:
      UNREACHABLE();
  }
}


void Simulator::VisitConditionalBranch(Instruction* instr) {
  DCHECK(instr->Mask(ConditionalBranchMask) == B_cond);
  if (ConditionPassed(static_cast<Condition>(instr->ConditionBranch()))) {
    set_pc(instr->ImmPCOffsetTarget());
  }
}


void Simulator::VisitUnconditionalBranchToRegister(Instruction* instr) {
  Instruction* target = reg<Instruction*>(instr->Rn());
  switch (instr->Mask(UnconditionalBranchToRegisterMask)) {
    case BLR: {
      set_lr(instr->following());
      if (instr->Rn() == 31) {
        // BLR XZR is used as a guard for the constant pool. We should never hit
        // this, but if we do trap to allow debugging.
        Debug();
      }
      // Fall through.
    }
    case BR:
    case RET: set_pc(target); break;
    default: UNIMPLEMENTED();
  }
}


void Simulator::VisitTestBranch(Instruction* instr) {
  unsigned bit_pos = (instr->ImmTestBranchBit5() << 5) |
                     instr->ImmTestBranchBit40();
  bool take_branch = ((xreg(instr->Rt()) & (1UL << bit_pos)) == 0);
  switch (instr->Mask(TestBranchMask)) {
    case TBZ: break;
    case TBNZ: take_branch = !take_branch; break;
    default: UNIMPLEMENTED();
  }
  if (take_branch) {
    set_pc(instr->ImmPCOffsetTarget());
  }
}


void Simulator::VisitCompareBranch(Instruction* instr) {
  unsigned rt = instr->Rt();
  bool take_branch = false;
  switch (instr->Mask(CompareBranchMask)) {
    case CBZ_w: take_branch = (wreg(rt) == 0); break;
    case CBZ_x: take_branch = (xreg(rt) == 0); break;
    case CBNZ_w: take_branch = (wreg(rt) != 0); break;
    case CBNZ_x: take_branch = (xreg(rt) != 0); break;
    default: UNIMPLEMENTED();
  }
  if (take_branch) {
    set_pc(instr->ImmPCOffsetTarget());
  }
}


template<typename T>
void Simulator::AddSubHelper(Instruction* instr, T op2) {
  bool set_flags = instr->FlagsUpdate();
  T new_val = 0;
  Instr operation = instr->Mask(AddSubOpMask);

  switch (operation) {
    case ADD:
    case ADDS: {
      new_val = AddWithCarry<T>(set_flags,
                                reg<T>(instr->Rn(), instr->RnMode()),
                                op2);
      break;
    }
    case SUB:
    case SUBS: {
      new_val = AddWithCarry<T>(set_flags,
                                reg<T>(instr->Rn(), instr->RnMode()),
                                ~op2,
                                1);
      break;
    }
    default: UNREACHABLE();
  }

  set_reg<T>(instr->Rd(), new_val, instr->RdMode());
}


void Simulator::VisitAddSubShifted(Instruction* instr) {
  Shift shift_type = static_cast<Shift>(instr->ShiftDP());
  unsigned shift_amount = instr->ImmDPShift();

  if (instr->SixtyFourBits()) {
    int64_t op2 = ShiftOperand(xreg(instr->Rm()), shift_type, shift_amount);
    AddSubHelper(instr, op2);
  } else {
    int32_t op2 = ShiftOperand(wreg(instr->Rm()), shift_type, shift_amount);
    AddSubHelper(instr, op2);
  }
}


void Simulator::VisitAddSubImmediate(Instruction* instr) {
  int64_t op2 = instr->ImmAddSub() << ((instr->ShiftAddSub() == 1) ? 12 : 0);
  if (instr->SixtyFourBits()) {
    AddSubHelper<int64_t>(instr, op2);
  } else {
    AddSubHelper<int32_t>(instr, op2);
  }
}


void Simulator::VisitAddSubExtended(Instruction* instr) {
  Extend ext = static_cast<Extend>(instr->ExtendMode());
  unsigned left_shift = instr->ImmExtendShift();
  if (instr->SixtyFourBits()) {
    int64_t op2 = ExtendValue(xreg(instr->Rm()), ext, left_shift);
    AddSubHelper(instr, op2);
  } else {
    int32_t op2 = ExtendValue(wreg(instr->Rm()), ext, left_shift);
    AddSubHelper(instr, op2);
  }
}


void Simulator::VisitAddSubWithCarry(Instruction* instr) {
  if (instr->SixtyFourBits()) {
    AddSubWithCarry<int64_t>(instr);
  } else {
    AddSubWithCarry<int32_t>(instr);
  }
}


void Simulator::VisitLogicalShifted(Instruction* instr) {
  Shift shift_type = static_cast<Shift>(instr->ShiftDP());
  unsigned shift_amount = instr->ImmDPShift();

  if (instr->SixtyFourBits()) {
    int64_t op2 = ShiftOperand(xreg(instr->Rm()), shift_type, shift_amount);
    op2 = (instr->Mask(NOT) == NOT) ? ~op2 : op2;
    LogicalHelper<int64_t>(instr, op2);
  } else {
    int32_t op2 = ShiftOperand(wreg(instr->Rm()), shift_type, shift_amount);
    op2 = (instr->Mask(NOT) == NOT) ? ~op2 : op2;
    LogicalHelper<int32_t>(instr, op2);
  }
}


void Simulator::VisitLogicalImmediate(Instruction* instr) {
  if (instr->SixtyFourBits()) {
    LogicalHelper<int64_t>(instr, instr->ImmLogical());
  } else {
    LogicalHelper<int32_t>(instr, instr->ImmLogical());
  }
}


template<typename T>
void Simulator::LogicalHelper(Instruction* instr, T op2) {
  T op1 = reg<T>(instr->Rn());
  T result = 0;
  bool update_flags = false;

  // Switch on the logical operation, stripping out the NOT bit, as it has a
  // different meaning for logical immediate instructions.
  switch (instr->Mask(LogicalOpMask & ~NOT)) {
    case ANDS: update_flags = true;  // Fall through.
    case AND: result = op1 & op2; break;
    case ORR: result = op1 | op2; break;
    case EOR: result = op1 ^ op2; break;
    default:
      UNIMPLEMENTED();
  }

  if (update_flags) {
    nzcv().SetN(CalcNFlag(result));
    nzcv().SetZ(CalcZFlag(result));
    nzcv().SetC(0);
    nzcv().SetV(0);
    LogSystemRegister(NZCV);
  }

  set_reg<T>(instr->Rd(), result, instr->RdMode());
}


void Simulator::VisitConditionalCompareRegister(Instruction* instr) {
  if (instr->SixtyFourBits()) {
    ConditionalCompareHelper(instr, xreg(instr->Rm()));
  } else {
    ConditionalCompareHelper(instr, wreg(instr->Rm()));
  }
}


void Simulator::VisitConditionalCompareImmediate(Instruction* instr) {
  if (instr->SixtyFourBits()) {
    ConditionalCompareHelper<int64_t>(instr, instr->ImmCondCmp());
  } else {
    ConditionalCompareHelper<int32_t>(instr, instr->ImmCondCmp());
  }
}


template<typename T>
void Simulator::ConditionalCompareHelper(Instruction* instr, T op2) {
  T op1 = reg<T>(instr->Rn());

  if (ConditionPassed(static_cast<Condition>(instr->Condition()))) {
    // If the condition passes, set the status flags to the result of comparing
    // the operands.
    if (instr->Mask(ConditionalCompareMask) == CCMP) {
      AddWithCarry<T>(true, op1, ~op2, 1);
    } else {
      DCHECK(instr->Mask(ConditionalCompareMask) == CCMN);
      AddWithCarry<T>(true, op1, op2, 0);
    }
  } else {
    // If the condition fails, set the status flags to the nzcv immediate.
    nzcv().SetFlags(instr->Nzcv());
    LogSystemRegister(NZCV);
  }
}


void Simulator::VisitLoadStoreUnsignedOffset(Instruction* instr) {
  int offset = instr->ImmLSUnsigned() << instr->SizeLS();
  LoadStoreHelper(instr, offset, Offset);
}


void Simulator::VisitLoadStoreUnscaledOffset(Instruction* instr) {
  LoadStoreHelper(instr, instr->ImmLS(), Offset);
}


void Simulator::VisitLoadStorePreIndex(Instruction* instr) {
  LoadStoreHelper(instr, instr->ImmLS(), PreIndex);
}


void Simulator::VisitLoadStorePostIndex(Instruction* instr) {
  LoadStoreHelper(instr, instr->ImmLS(), PostIndex);
}


void Simulator::VisitLoadStoreRegisterOffset(Instruction* instr) {
  Extend ext = static_cast<Extend>(instr->ExtendMode());
  DCHECK((ext == UXTW) || (ext == UXTX) || (ext == SXTW) || (ext == SXTX));
  unsigned shift_amount = instr->ImmShiftLS() * instr->SizeLS();

  int64_t offset = ExtendValue(xreg(instr->Rm()), ext, shift_amount);
  LoadStoreHelper(instr, offset, Offset);
}


void Simulator::LoadStoreHelper(Instruction* instr,
                                int64_t offset,
                                AddrMode addrmode) {
  unsigned srcdst = instr->Rt();
  unsigned addr_reg = instr->Rn();
  uintptr_t address = LoadStoreAddress(addr_reg, offset, addrmode);
  uintptr_t stack = 0;

  // Handle the writeback for stores before the store. On a CPU the writeback
  // and the store are atomic, but when running on the simulator it is possible
  // to be interrupted in between. The simulator is not thread safe and V8 does
  // not require it to be to run JavaScript therefore the profiler may sample
  // the "simulated" CPU in the middle of load/store with writeback. The code
  // below ensures that push operations are safe even when interrupted: the
  // stack pointer will be decremented before adding an element to the stack.
  if (instr->IsStore()) {
    LoadStoreWriteBack(addr_reg, offset, addrmode);

    // For store the address post writeback is used to check access below the
    // stack.
    stack = sp();
  }

  LoadStoreOp op = static_cast<LoadStoreOp>(instr->Mask(LoadStoreOpMask));
  switch (op) {
    // Use _no_log variants to suppress the register trace (LOG_REGS,
    // LOG_FP_REGS). We will print a more detailed log.
    case LDRB_w:  set_wreg_no_log(srcdst, MemoryRead<uint8_t>(address)); break;
    case LDRH_w:  set_wreg_no_log(srcdst, MemoryRead<uint16_t>(address)); break;
    case LDR_w:   set_wreg_no_log(srcdst, MemoryRead<uint32_t>(address)); break;
    case LDR_x:   set_xreg_no_log(srcdst, MemoryRead<uint64_t>(address)); break;
    case LDRSB_w: set_wreg_no_log(srcdst, MemoryRead<int8_t>(address)); break;
    case LDRSH_w: set_wreg_no_log(srcdst, MemoryRead<int16_t>(address)); break;
    case LDRSB_x: set_xreg_no_log(srcdst, MemoryRead<int8_t>(address)); break;
    case LDRSH_x: set_xreg_no_log(srcdst, MemoryRead<int16_t>(address)); break;
    case LDRSW_x: set_xreg_no_log(srcdst, MemoryRead<int32_t>(address)); break;
    case LDR_s:   set_sreg_no_log(srcdst, MemoryRead<float>(address)); break;
    case LDR_d:   set_dreg_no_log(srcdst, MemoryRead<double>(address)); break;

    case STRB_w:  MemoryWrite<uint8_t>(address, wreg(srcdst)); break;
    case STRH_w:  MemoryWrite<uint16_t>(address, wreg(srcdst)); break;
    case STR_w:   MemoryWrite<uint32_t>(address, wreg(srcdst)); break;
    case STR_x:   MemoryWrite<uint64_t>(address, xreg(srcdst)); break;
    case STR_s:   MemoryWrite<float>(address, sreg(srcdst)); break;
    case STR_d:   MemoryWrite<double>(address, dreg(srcdst)); break;

    default: UNIMPLEMENTED();
  }

  // Print a detailed trace (including the memory address) instead of the basic
  // register:value trace generated by set_*reg().
  size_t access_size = 1 << instr->SizeLS();
  if (instr->IsLoad()) {
    if ((op == LDR_s) || (op == LDR_d)) {
      LogReadFP(address, access_size, srcdst);
    } else {
      LogRead(address, access_size, srcdst);
    }
  } else {
    if ((op == STR_s) || (op == STR_d)) {
      LogWriteFP(address, access_size, srcdst);
    } else {
      LogWrite(address, access_size, srcdst);
    }
  }

  // Handle the writeback for loads after the load to ensure safe pop
  // operation even when interrupted in the middle of it. The stack pointer
  // is only updated after the load so pop(fp) will never break the invariant
  // sp <= fp expected while walking the stack in the sampler.
  if (instr->IsLoad()) {
    // For loads the address pre writeback is used to check access below the
    // stack.
    stack = sp();

    LoadStoreWriteBack(addr_reg, offset, addrmode);
  }

  // Accesses below the stack pointer (but above the platform stack limit) are
  // not allowed in the ABI.
  CheckMemoryAccess(address, stack);
}


void Simulator::VisitLoadStorePairOffset(Instruction* instr) {
  LoadStorePairHelper(instr, Offset);
}


void Simulator::VisitLoadStorePairPreIndex(Instruction* instr) {
  LoadStorePairHelper(instr, PreIndex);
}


void Simulator::VisitLoadStorePairPostIndex(Instruction* instr) {
  LoadStorePairHelper(instr, PostIndex);
}


void Simulator::VisitLoadStorePairNonTemporal(Instruction* instr) {
  LoadStorePairHelper(instr, Offset);
}


void Simulator::LoadStorePairHelper(Instruction* instr,
                                    AddrMode addrmode) {
  unsigned rt = instr->Rt();
  unsigned rt2 = instr->Rt2();
  unsigned addr_reg = instr->Rn();
  size_t access_size = 1 << instr->SizeLSPair();
  int64_t offset = instr->ImmLSPair() * access_size;
  uintptr_t address = LoadStoreAddress(addr_reg, offset, addrmode);
  uintptr_t address2 = address + access_size;
  uintptr_t stack = 0;

  // Handle the writeback for stores before the store. On a CPU the writeback
  // and the store are atomic, but when running on the simulator it is possible
  // to be interrupted in between. The simulator is not thread safe and V8 does
  // not require it to be to run JavaScript therefore the profiler may sample
  // the "simulated" CPU in the middle of load/store with writeback. The code
  // below ensures that push operations are safe even when interrupted: the
  // stack pointer will be decremented before adding an element to the stack.
  if (instr->IsStore()) {
    LoadStoreWriteBack(addr_reg, offset, addrmode);

    // For store the address post writeback is used to check access below the
    // stack.
    stack = sp();
  }

  LoadStorePairOp op =
    static_cast<LoadStorePairOp>(instr->Mask(LoadStorePairMask));

  // 'rt' and 'rt2' can only be aliased for stores.
  DCHECK(((op & LoadStorePairLBit) == 0) || (rt != rt2));

  switch (op) {
    // Use _no_log variants to suppress the register trace (LOG_REGS,
    // LOG_FP_REGS). We will print a more detailed log.
    case LDP_w: {
      DCHECK(access_size == kWRegSize);
      set_wreg_no_log(rt, MemoryRead<uint32_t>(address));
      set_wreg_no_log(rt2, MemoryRead<uint32_t>(address2));
      break;
    }
    case LDP_s: {
      DCHECK(access_size == kSRegSize);
      set_sreg_no_log(rt, MemoryRead<float>(address));
      set_sreg_no_log(rt2, MemoryRead<float>(address2));
      break;
    }
    case LDP_x: {
      DCHECK(access_size == kXRegSize);
      set_xreg_no_log(rt, MemoryRead<uint64_t>(address));
      set_xreg_no_log(rt2, MemoryRead<uint64_t>(address2));
      break;
    }
    case LDP_d: {
      DCHECK(access_size == kDRegSize);
      set_dreg_no_log(rt, MemoryRead<double>(address));
      set_dreg_no_log(rt2, MemoryRead<double>(address2));
      break;
    }
    case LDPSW_x: {
      DCHECK(access_size == kWRegSize);
      set_xreg_no_log(rt, MemoryRead<int32_t>(address));
      set_xreg_no_log(rt2, MemoryRead<int32_t>(address2));
      break;
    }
    case STP_w: {
      DCHECK(access_size == kWRegSize);
      MemoryWrite<uint32_t>(address, wreg(rt));
      MemoryWrite<uint32_t>(address2, wreg(rt2));
      break;
    }
    case STP_s: {
      DCHECK(access_size == kSRegSize);
      MemoryWrite<float>(address, sreg(rt));
      MemoryWrite<float>(address2, sreg(rt2));
      break;
    }
    case STP_x: {
      DCHECK(access_size == kXRegSize);
      MemoryWrite<uint64_t>(address, xreg(rt));
      MemoryWrite<uint64_t>(address2, xreg(rt2));
      break;
    }
    case STP_d: {
      DCHECK(access_size == kDRegSize);
      MemoryWrite<double>(address, dreg(rt));
      MemoryWrite<double>(address2, dreg(rt2));
      break;
    }
    default: UNREACHABLE();
  }

  // Print a detailed trace (including the memory address) instead of the basic
  // register:value trace generated by set_*reg().
  if (instr->IsLoad()) {
    if ((op == LDP_s) || (op == LDP_d)) {
      LogReadFP(address, access_size, rt);
      LogReadFP(address2, access_size, rt2);
    } else {
      LogRead(address, access_size, rt);
      LogRead(address2, access_size, rt2);
    }
  } else {
    if ((op == STP_s) || (op == STP_d)) {
      LogWriteFP(address, access_size, rt);
      LogWriteFP(address2, access_size, rt2);
    } else {
      LogWrite(address, access_size, rt);
      LogWrite(address2, access_size, rt2);
    }
  }

  // Handle the writeback for loads after the load to ensure safe pop
  // operation even when interrupted in the middle of it. The stack pointer
  // is only updated after the load so pop(fp) will never break the invariant
  // sp <= fp expected while walking the stack in the sampler.
  if (instr->IsLoad()) {
    // For loads the address pre writeback is used to check access below the
    // stack.
    stack = sp();

    LoadStoreWriteBack(addr_reg, offset, addrmode);
  }

  // Accesses below the stack pointer (but above the platform stack limit) are
  // not allowed in the ABI.
  CheckMemoryAccess(address, stack);
}


void Simulator::VisitLoadLiteral(Instruction* instr) {
  uintptr_t address = instr->LiteralAddress();
  unsigned rt = instr->Rt();

  switch (instr->Mask(LoadLiteralMask)) {
    // Use _no_log variants to suppress the register trace (LOG_REGS,
    // LOG_FP_REGS), then print a more detailed log.
    case LDR_w_lit:
      set_wreg_no_log(rt, MemoryRead<uint32_t>(address));
      LogRead(address, kWRegSize, rt);
      break;
    case LDR_x_lit:
      set_xreg_no_log(rt, MemoryRead<uint64_t>(address));
      LogRead(address, kXRegSize, rt);
      break;
    case LDR_s_lit:
      set_sreg_no_log(rt, MemoryRead<float>(address));
      LogReadFP(address, kSRegSize, rt);
      break;
    case LDR_d_lit:
      set_dreg_no_log(rt, MemoryRead<double>(address));
      LogReadFP(address, kDRegSize, rt);
      break;
    default: UNREACHABLE();
  }
}


uintptr_t Simulator::LoadStoreAddress(unsigned addr_reg, int64_t offset,
                                      AddrMode addrmode) {
  const unsigned kSPRegCode = kSPRegInternalCode & kRegCodeMask;
  uint64_t address = xreg(addr_reg, Reg31IsStackPointer);
  if ((addr_reg == kSPRegCode) && ((address % 16) != 0)) {
    // When the base register is SP the stack pointer is required to be
    // quadword aligned prior to the address calculation and write-backs.
    // Misalignment will cause a stack alignment fault.
    FATAL("ALIGNMENT EXCEPTION");
  }

  if ((addrmode == Offset) || (addrmode == PreIndex)) {
    address += offset;
  }

  return address;
}


void Simulator::LoadStoreWriteBack(unsigned addr_reg,
                                   int64_t offset,
                                   AddrMode addrmode) {
  if ((addrmode == PreIndex) || (addrmode == PostIndex)) {
    DCHECK(offset != 0);
    uint64_t address = xreg(addr_reg, Reg31IsStackPointer);
    set_reg(addr_reg, address + offset, Reg31IsStackPointer);
  }
}


void Simulator::CheckMemoryAccess(uintptr_t address, uintptr_t stack) {
  if ((address >= stack_limit_) && (address < stack)) {
    fprintf(stream_, "ACCESS BELOW STACK POINTER:\n");
    fprintf(stream_, "  sp is here:          0x%016" PRIx64 "\n",
            static_cast<uint64_t>(stack));
    fprintf(stream_, "  access was here:     0x%016" PRIx64 "\n",
            static_cast<uint64_t>(address));
    fprintf(stream_, "  stack limit is here: 0x%016" PRIx64 "\n",
            static_cast<uint64_t>(stack_limit_));
    fprintf(stream_, "\n");
    FATAL("ACCESS BELOW STACK POINTER");
  }
}


void Simulator::VisitMoveWideImmediate(Instruction* instr) {
  MoveWideImmediateOp mov_op =
    static_cast<MoveWideImmediateOp>(instr->Mask(MoveWideImmediateMask));
  int64_t new_xn_val = 0;

  bool is_64_bits = instr->SixtyFourBits() == 1;
  // Shift is limited for W operations.
  DCHECK(is_64_bits || (instr->ShiftMoveWide() < 2));

  // Get the shifted immediate.
  int64_t shift = instr->ShiftMoveWide() * 16;
  int64_t shifted_imm16 = instr->ImmMoveWide() << shift;

  // Compute the new value.
  switch (mov_op) {
    case MOVN_w:
    case MOVN_x: {
        new_xn_val = ~shifted_imm16;
        if (!is_64_bits) new_xn_val &= kWRegMask;
      break;
    }
    case MOVK_w:
    case MOVK_x: {
        unsigned reg_code = instr->Rd();
        int64_t prev_xn_val = is_64_bits ? xreg(reg_code)
                                         : wreg(reg_code);
        new_xn_val = (prev_xn_val & ~(0xffffL << shift)) | shifted_imm16;
      break;
    }
    case MOVZ_w:
    case MOVZ_x: {
        new_xn_val = shifted_imm16;
      break;
    }
    default:
      UNREACHABLE();
  }

  // Update the destination register.
  set_xreg(instr->Rd(), new_xn_val);
}


void Simulator::VisitConditionalSelect(Instruction* instr) {
  if (ConditionFailed(static_cast<Condition>(instr->Condition()))) {
    uint64_t new_val = xreg(instr->Rm());
    switch (instr->Mask(ConditionalSelectMask)) {
      case CSEL_w: set_wreg(instr->Rd(), new_val); break;
      case CSEL_x: set_xreg(instr->Rd(), new_val); break;
      case CSINC_w: set_wreg(instr->Rd(), new_val + 1); break;
      case CSINC_x: set_xreg(instr->Rd(), new_val + 1); break;
      case CSINV_w: set_wreg(instr->Rd(), ~new_val); break;
      case CSINV_x: set_xreg(instr->Rd(), ~new_val); break;
      case CSNEG_w: set_wreg(instr->Rd(), -new_val); break;
      case CSNEG_x: set_xreg(instr->Rd(), -new_val); break;
      default: UNIMPLEMENTED();
    }
  } else {
    if (instr->SixtyFourBits()) {
      set_xreg(instr->Rd(), xreg(instr->Rn()));
    } else {
      set_wreg(instr->Rd(), wreg(instr->Rn()));
    }
  }
}


void Simulator::VisitDataProcessing1Source(Instruction* instr) {
  unsigned dst = instr->Rd();
  unsigned src = instr->Rn();

  switch (instr->Mask(DataProcessing1SourceMask)) {
    case RBIT_w: set_wreg(dst, ReverseBits(wreg(src), kWRegSizeInBits)); break;
    case RBIT_x: set_xreg(dst, ReverseBits(xreg(src), kXRegSizeInBits)); break;
    case REV16_w: set_wreg(dst, ReverseBytes(wreg(src), Reverse16)); break;
    case REV16_x: set_xreg(dst, ReverseBytes(xreg(src), Reverse16)); break;
    case REV_w: set_wreg(dst, ReverseBytes(wreg(src), Reverse32)); break;
    case REV32_x: set_xreg(dst, ReverseBytes(xreg(src), Reverse32)); break;
    case REV_x: set_xreg(dst, ReverseBytes(xreg(src), Reverse64)); break;
    case CLZ_w: set_wreg(dst, CountLeadingZeros(wreg(src), kWRegSizeInBits));
                break;
    case CLZ_x: set_xreg(dst, CountLeadingZeros(xreg(src), kXRegSizeInBits));
                break;
    case CLS_w: {
      set_wreg(dst, CountLeadingSignBits(wreg(src), kWRegSizeInBits));
      break;
    }
    case CLS_x: {
      set_xreg(dst, CountLeadingSignBits(xreg(src), kXRegSizeInBits));
      break;
    }
    default: UNIMPLEMENTED();
  }
}


uint64_t Simulator::ReverseBits(uint64_t value, unsigned num_bits) {
  DCHECK((num_bits == kWRegSizeInBits) || (num_bits == kXRegSizeInBits));
  uint64_t result = 0;
  for (unsigned i = 0; i < num_bits; i++) {
    result = (result << 1) | (value & 1);
    value >>= 1;
  }
  return result;
}


uint64_t Simulator::ReverseBytes(uint64_t value, ReverseByteMode mode) {
  // Split the 64-bit value into an 8-bit array, where b[0] is the least
  // significant byte, and b[7] is the most significant.
  uint8_t bytes[8];
  uint64_t mask = 0xff00000000000000UL;
  for (int i = 7; i >= 0; i--) {
    bytes[i] = (value & mask) >> (i * 8);
    mask >>= 8;
  }

  // Permutation tables for REV instructions.
  //  permute_table[Reverse16] is used by REV16_x, REV16_w
  //  permute_table[Reverse32] is used by REV32_x, REV_w
  //  permute_table[Reverse64] is used by REV_x
  DCHECK((Reverse16 == 0) && (Reverse32 == 1) && (Reverse64 == 2));
  static const uint8_t permute_table[3][8] = { {6, 7, 4, 5, 2, 3, 0, 1},
                                               {4, 5, 6, 7, 0, 1, 2, 3},
                                               {0, 1, 2, 3, 4, 5, 6, 7} };
  uint64_t result = 0;
  for (int i = 0; i < 8; i++) {
    result <<= 8;
    result |= bytes[permute_table[mode][i]];
  }
  return result;
}


template <typename T>
void Simulator::DataProcessing2Source(Instruction* instr) {
  Shift shift_op = NO_SHIFT;
  T result = 0;
  switch (instr->Mask(DataProcessing2SourceMask)) {
    case SDIV_w:
    case SDIV_x: {
      T rn = reg<T>(instr->Rn());
      T rm = reg<T>(instr->Rm());
      if ((rn == std::numeric_limits<T>::min()) && (rm == -1)) {
        result = std::numeric_limits<T>::min();
      } else if (rm == 0) {
        // Division by zero can be trapped, but not on A-class processors.
        result = 0;
      } else {
        result = rn / rm;
      }
      break;
    }
    case UDIV_w:
    case UDIV_x: {
      typedef typename make_unsigned<T>::type unsignedT;
      unsignedT rn = static_cast<unsignedT>(reg<T>(instr->Rn()));
      unsignedT rm = static_cast<unsignedT>(reg<T>(instr->Rm()));
      if (rm == 0) {
        // Division by zero can be trapped, but not on A-class processors.
        result = 0;
      } else {
        result = rn / rm;
      }
      break;
    }
    case LSLV_w:
    case LSLV_x: shift_op = LSL; break;
    case LSRV_w:
    case LSRV_x: shift_op = LSR; break;
    case ASRV_w:
    case ASRV_x: shift_op = ASR; break;
    case RORV_w:
    case RORV_x: shift_op = ROR; break;
    default: UNIMPLEMENTED();
  }

  if (shift_op != NO_SHIFT) {
    // Shift distance encoded in the least-significant five/six bits of the
    // register.
    unsigned shift = wreg(instr->Rm());
    if (sizeof(T) == kWRegSize) {
      shift &= kShiftAmountWRegMask;
    } else {
      shift &= kShiftAmountXRegMask;
    }
    result = ShiftOperand(reg<T>(instr->Rn()), shift_op, shift);
  }
  set_reg<T>(instr->Rd(), result);
}


void Simulator::VisitDataProcessing2Source(Instruction* instr) {
  if (instr->SixtyFourBits()) {
    DataProcessing2Source<int64_t>(instr);
  } else {
    DataProcessing2Source<int32_t>(instr);
  }
}


// The algorithm used is described in section 8.2 of
//   Hacker's Delight, by Henry S. Warren, Jr.
// It assumes that a right shift on a signed integer is an arithmetic shift.
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


void Simulator::VisitDataProcessing3Source(Instruction* instr) {
  int64_t result = 0;
  // Extract and sign- or zero-extend 32-bit arguments for widening operations.
  uint64_t rn_u32 = reg<uint32_t>(instr->Rn());
  uint64_t rm_u32 = reg<uint32_t>(instr->Rm());
  int64_t rn_s32 = reg<int32_t>(instr->Rn());
  int64_t rm_s32 = reg<int32_t>(instr->Rm());
  switch (instr->Mask(DataProcessing3SourceMask)) {
    case MADD_w:
    case MADD_x:
      result = xreg(instr->Ra()) + (xreg(instr->Rn()) * xreg(instr->Rm()));
      break;
    case MSUB_w:
    case MSUB_x:
      result = xreg(instr->Ra()) - (xreg(instr->Rn()) * xreg(instr->Rm()));
      break;
    case SMADDL_x: result = xreg(instr->Ra()) + (rn_s32 * rm_s32); break;
    case SMSUBL_x: result = xreg(instr->Ra()) - (rn_s32 * rm_s32); break;
    case UMADDL_x: result = xreg(instr->Ra()) + (rn_u32 * rm_u32); break;
    case UMSUBL_x: result = xreg(instr->Ra()) - (rn_u32 * rm_u32); break;
    case SMULH_x:
      DCHECK(instr->Ra() == kZeroRegCode);
      result = MultiplyHighSigned(xreg(instr->Rn()), xreg(instr->Rm()));
      break;
    default: UNIMPLEMENTED();
  }

  if (instr->SixtyFourBits()) {
    set_xreg(instr->Rd(), result);
  } else {
    set_wreg(instr->Rd(), result);
  }
}


template <typename T>
void Simulator::BitfieldHelper(Instruction* instr) {
  typedef typename make_unsigned<T>::type unsignedT;
  T reg_size = sizeof(T) * 8;
  T R = instr->ImmR();
  T S = instr->ImmS();
  T diff = S - R;
  T mask;
  if (diff >= 0) {
    mask = diff < reg_size - 1 ? (static_cast<T>(1) << (diff + 1)) - 1
                               : static_cast<T>(-1);
  } else {
    mask = ((1L << (S + 1)) - 1);
    mask = (static_cast<uint64_t>(mask) >> R) | (mask << (reg_size - R));
    diff += reg_size;
  }

  // inzero indicates if the extracted bitfield is inserted into the
  // destination register value or in zero.
  // If extend is true, extend the sign of the extracted bitfield.
  bool inzero = false;
  bool extend = false;
  switch (instr->Mask(BitfieldMask)) {
    case BFM_x:
    case BFM_w:
      break;
    case SBFM_x:
    case SBFM_w:
      inzero = true;
      extend = true;
      break;
    case UBFM_x:
    case UBFM_w:
      inzero = true;
      break;
    default:
      UNIMPLEMENTED();
  }

  T dst = inzero ? 0 : reg<T>(instr->Rd());
  T src = reg<T>(instr->Rn());
  // Rotate source bitfield into place.
  T result = (static_cast<unsignedT>(src) >> R) | (src << (reg_size - R));
  // Determine the sign extension.
  T topbits_preshift = (static_cast<T>(1) << (reg_size - diff - 1)) - 1;
  T signbits = (extend && ((src >> S) & 1) ? topbits_preshift : 0)
               << (diff + 1);

  // Merge sign extension, dest/zero and bitfield.
  result = signbits | (result & mask) | (dst & ~mask);

  set_reg<T>(instr->Rd(), result);
}


void Simulator::VisitBitfield(Instruction* instr) {
  if (instr->SixtyFourBits()) {
    BitfieldHelper<int64_t>(instr);
  } else {
    BitfieldHelper<int32_t>(instr);
  }
}


void Simulator::VisitExtract(Instruction* instr) {
  if (instr->SixtyFourBits()) {
    Extract<uint64_t>(instr);
  } else {
    Extract<uint32_t>(instr);
  }
}


void Simulator::VisitFPImmediate(Instruction* instr) {
  AssertSupportedFPCR();

  unsigned dest = instr->Rd();
  switch (instr->Mask(FPImmediateMask)) {
    case FMOV_s_imm: set_sreg(dest, instr->ImmFP32()); break;
    case FMOV_d_imm: set_dreg(dest, instr->ImmFP64()); break;
    default: UNREACHABLE();
  }
}


void Simulator::VisitFPIntegerConvert(Instruction* instr) {
  AssertSupportedFPCR();

  unsigned dst = instr->Rd();
  unsigned src = instr->Rn();

  FPRounding round = fpcr().RMode();

  switch (instr->Mask(FPIntegerConvertMask)) {
    case FCVTAS_ws: set_wreg(dst, FPToInt32(sreg(src), FPTieAway)); break;
    case FCVTAS_xs: set_xreg(dst, FPToInt64(sreg(src), FPTieAway)); break;
    case FCVTAS_wd: set_wreg(dst, FPToInt32(dreg(src), FPTieAway)); break;
    case FCVTAS_xd: set_xreg(dst, FPToInt64(dreg(src), FPTieAway)); break;
    case FCVTAU_ws: set_wreg(dst, FPToUInt32(sreg(src), FPTieAway)); break;
    case FCVTAU_xs: set_xreg(dst, FPToUInt64(sreg(src), FPTieAway)); break;
    case FCVTAU_wd: set_wreg(dst, FPToUInt32(dreg(src), FPTieAway)); break;
    case FCVTAU_xd: set_xreg(dst, FPToUInt64(dreg(src), FPTieAway)); break;
    case FCVTMS_ws:
      set_wreg(dst, FPToInt32(sreg(src), FPNegativeInfinity));
      break;
    case FCVTMS_xs:
      set_xreg(dst, FPToInt64(sreg(src), FPNegativeInfinity));
      break;
    case FCVTMS_wd:
      set_wreg(dst, FPToInt32(dreg(src), FPNegativeInfinity));
      break;
    case FCVTMS_xd:
      set_xreg(dst, FPToInt64(dreg(src), FPNegativeInfinity));
      break;
    case FCVTMU_ws:
      set_wreg(dst, FPToUInt32(sreg(src), FPNegativeInfinity));
      break;
    case FCVTMU_xs:
      set_xreg(dst, FPToUInt64(sreg(src), FPNegativeInfinity));
      break;
    case FCVTMU_wd:
      set_wreg(dst, FPToUInt32(dreg(src), FPNegativeInfinity));
      break;
    case FCVTMU_xd:
      set_xreg(dst, FPToUInt64(dreg(src), FPNegativeInfinity));
      break;
    case FCVTNS_ws: set_wreg(dst, FPToInt32(sreg(src), FPTieEven)); break;
    case FCVTNS_xs: set_xreg(dst, FPToInt64(sreg(src), FPTieEven)); break;
    case FCVTNS_wd: set_wreg(dst, FPToInt32(dreg(src), FPTieEven)); break;
    case FCVTNS_xd: set_xreg(dst, FPToInt64(dreg(src), FPTieEven)); break;
    case FCVTNU_ws: set_wreg(dst, FPToUInt32(sreg(src), FPTieEven)); break;
    case FCVTNU_xs: set_xreg(dst, FPToUInt64(sreg(src), FPTieEven)); break;
    case FCVTNU_wd: set_wreg(dst, FPToUInt32(dreg(src), FPTieEven)); break;
    case FCVTNU_xd: set_xreg(dst, FPToUInt64(dreg(src), FPTieEven)); break;
    case FCVTZS_ws: set_wreg(dst, FPToInt32(sreg(src), FPZero)); break;
    case FCVTZS_xs: set_xreg(dst, FPToInt64(sreg(src), FPZero)); break;
    case FCVTZS_wd: set_wreg(dst, FPToInt32(dreg(src), FPZero)); break;
    case FCVTZS_xd: set_xreg(dst, FPToInt64(dreg(src), FPZero)); break;
    case FCVTZU_ws: set_wreg(dst, FPToUInt32(sreg(src), FPZero)); break;
    case FCVTZU_xs: set_xreg(dst, FPToUInt64(sreg(src), FPZero)); break;
    case FCVTZU_wd: set_wreg(dst, FPToUInt32(dreg(src), FPZero)); break;
    case FCVTZU_xd: set_xreg(dst, FPToUInt64(dreg(src), FPZero)); break;
    case FMOV_ws: set_wreg(dst, sreg_bits(src)); break;
    case FMOV_xd: set_xreg(dst, dreg_bits(src)); break;
    case FMOV_sw: set_sreg_bits(dst, wreg(src)); break;
    case FMOV_dx: set_dreg_bits(dst, xreg(src)); break;

    // A 32-bit input can be handled in the same way as a 64-bit input, since
    // the sign- or zero-extension will not affect the conversion.
    case SCVTF_dx: set_dreg(dst, FixedToDouble(xreg(src), 0, round)); break;
    case SCVTF_dw: set_dreg(dst, FixedToDouble(wreg(src), 0, round)); break;
    case UCVTF_dx: set_dreg(dst, UFixedToDouble(xreg(src), 0, round)); break;
    case UCVTF_dw: {
      set_dreg(dst, UFixedToDouble(reg<uint32_t>(src), 0, round));
      break;
    }
    case SCVTF_sx: set_sreg(dst, FixedToFloat(xreg(src), 0, round)); break;
    case SCVTF_sw: set_sreg(dst, FixedToFloat(wreg(src), 0, round)); break;
    case UCVTF_sx: set_sreg(dst, UFixedToFloat(xreg(src), 0, round)); break;
    case UCVTF_sw: {
      set_sreg(dst, UFixedToFloat(reg<uint32_t>(src), 0, round));
      break;
    }

    default: UNREACHABLE();
  }
}


void Simulator::VisitFPFixedPointConvert(Instruction* instr) {
  AssertSupportedFPCR();

  unsigned dst = instr->Rd();
  unsigned src = instr->Rn();
  int fbits = 64 - instr->FPScale();

  FPRounding round = fpcr().RMode();

  switch (instr->Mask(FPFixedPointConvertMask)) {
    // A 32-bit input can be handled in the same way as a 64-bit input, since
    // the sign- or zero-extension will not affect the conversion.
    case SCVTF_dx_fixed:
      set_dreg(dst, FixedToDouble(xreg(src), fbits, round));
      break;
    case SCVTF_dw_fixed:
      set_dreg(dst, FixedToDouble(wreg(src), fbits, round));
      break;
    case UCVTF_dx_fixed:
      set_dreg(dst, UFixedToDouble(xreg(src), fbits, round));
      break;
    case UCVTF_dw_fixed: {
      set_dreg(dst,
               UFixedToDouble(reg<uint32_t>(src), fbits, round));
      break;
    }
    case SCVTF_sx_fixed:
      set_sreg(dst, FixedToFloat(xreg(src), fbits, round));
      break;
    case SCVTF_sw_fixed:
      set_sreg(dst, FixedToFloat(wreg(src), fbits, round));
      break;
    case UCVTF_sx_fixed:
      set_sreg(dst, UFixedToFloat(xreg(src), fbits, round));
      break;
    case UCVTF_sw_fixed: {
      set_sreg(dst,
               UFixedToFloat(reg<uint32_t>(src), fbits, round));
      break;
    }
    default: UNREACHABLE();
  }
}


int32_t Simulator::FPToInt32(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  if (value >= kWMaxInt) {
    return kWMaxInt;
  } else if (value < kWMinInt) {
    return kWMinInt;
  }
  return std::isnan(value) ? 0 : static_cast<int32_t>(value);
}


int64_t Simulator::FPToInt64(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  if (value >= kXMaxInt) {
    return kXMaxInt;
  } else if (value < kXMinInt) {
    return kXMinInt;
  }
  return std::isnan(value) ? 0 : static_cast<int64_t>(value);
}


uint32_t Simulator::FPToUInt32(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  if (value >= kWMaxUInt) {
    return kWMaxUInt;
  } else if (value < 0.0) {
    return 0;
  }
  return std::isnan(value) ? 0 : static_cast<uint32_t>(value);
}


uint64_t Simulator::FPToUInt64(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  if (value >= kXMaxUInt) {
    return kXMaxUInt;
  } else if (value < 0.0) {
    return 0;
  }
  return std::isnan(value) ? 0 : static_cast<uint64_t>(value);
}


void Simulator::VisitFPCompare(Instruction* instr) {
  AssertSupportedFPCR();

  unsigned reg_size = (instr->Mask(FP64) == FP64) ? kDRegSizeInBits
                                                  : kSRegSizeInBits;
  double fn_val = fpreg(reg_size, instr->Rn());

  switch (instr->Mask(FPCompareMask)) {
    case FCMP_s:
    case FCMP_d: FPCompare(fn_val, fpreg(reg_size, instr->Rm())); break;
    case FCMP_s_zero:
    case FCMP_d_zero: FPCompare(fn_val, 0.0); break;
    default: UNIMPLEMENTED();
  }
}


void Simulator::VisitFPConditionalCompare(Instruction* instr) {
  AssertSupportedFPCR();

  switch (instr->Mask(FPConditionalCompareMask)) {
    case FCCMP_s:
    case FCCMP_d: {
      if (ConditionPassed(static_cast<Condition>(instr->Condition()))) {
        // If the condition passes, set the status flags to the result of
        // comparing the operands.
        unsigned reg_size = (instr->Mask(FP64) == FP64) ? kDRegSizeInBits
                                                        : kSRegSizeInBits;
        FPCompare(fpreg(reg_size, instr->Rn()), fpreg(reg_size, instr->Rm()));
      } else {
        // If the condition fails, set the status flags to the nzcv immediate.
        nzcv().SetFlags(instr->Nzcv());
        LogSystemRegister(NZCV);
      }
      break;
    }
    default: UNIMPLEMENTED();
  }
}


void Simulator::VisitFPConditionalSelect(Instruction* instr) {
  AssertSupportedFPCR();

  Instr selected;
  if (ConditionPassed(static_cast<Condition>(instr->Condition()))) {
    selected = instr->Rn();
  } else {
    selected = instr->Rm();
  }

  switch (instr->Mask(FPConditionalSelectMask)) {
    case FCSEL_s: set_sreg(instr->Rd(), sreg(selected)); break;
    case FCSEL_d: set_dreg(instr->Rd(), dreg(selected)); break;
    default: UNIMPLEMENTED();
  }
}


void Simulator::VisitFPDataProcessing1Source(Instruction* instr) {
  AssertSupportedFPCR();

  unsigned fd = instr->Rd();
  unsigned fn = instr->Rn();

  switch (instr->Mask(FPDataProcessing1SourceMask)) {
    case FMOV_s: set_sreg(fd, sreg(fn)); break;
    case FMOV_d: set_dreg(fd, dreg(fn)); break;
    case FABS_s: set_sreg(fd, std::fabs(sreg(fn))); break;
    case FABS_d: set_dreg(fd, std::fabs(dreg(fn))); break;
    case FNEG_s: set_sreg(fd, -sreg(fn)); break;
    case FNEG_d: set_dreg(fd, -dreg(fn)); break;
    case FSQRT_s: set_sreg(fd, FPSqrt(sreg(fn))); break;
    case FSQRT_d: set_dreg(fd, FPSqrt(dreg(fn))); break;
    case FRINTA_s: set_sreg(fd, FPRoundInt(sreg(fn), FPTieAway)); break;
    case FRINTA_d: set_dreg(fd, FPRoundInt(dreg(fn), FPTieAway)); break;
    case FRINTM_s:
        set_sreg(fd, FPRoundInt(sreg(fn), FPNegativeInfinity)); break;
    case FRINTM_d:
        set_dreg(fd, FPRoundInt(dreg(fn), FPNegativeInfinity)); break;
    case FRINTN_s: set_sreg(fd, FPRoundInt(sreg(fn), FPTieEven)); break;
    case FRINTN_d: set_dreg(fd, FPRoundInt(dreg(fn), FPTieEven)); break;
    case FRINTZ_s: set_sreg(fd, FPRoundInt(sreg(fn), FPZero)); break;
    case FRINTZ_d: set_dreg(fd, FPRoundInt(dreg(fn), FPZero)); break;
    case FCVT_ds: set_dreg(fd, FPToDouble(sreg(fn))); break;
    case FCVT_sd: set_sreg(fd, FPToFloat(dreg(fn), FPTieEven)); break;
    default: UNIMPLEMENTED();
  }
}


// Assemble the specified IEEE-754 components into the target type and apply
// appropriate rounding.
//  sign:     0 = positive, 1 = negative
//  exponent: Unbiased IEEE-754 exponent.
//  mantissa: The mantissa of the input. The top bit (which is not encoded for
//            normal IEEE-754 values) must not be omitted. This bit has the
//            value 'pow(2, exponent)'.
//
// The input value is assumed to be a normalized value. That is, the input may
// not be infinity or NaN. If the source value is subnormal, it must be
// normalized before calling this function such that the highest set bit in the
// mantissa has the value 'pow(2, exponent)'.
//
// Callers should use FPRoundToFloat or FPRoundToDouble directly, rather than
// calling a templated FPRound.
template <class T, int ebits, int mbits>
static T FPRound(int64_t sign, int64_t exponent, uint64_t mantissa,
                 FPRounding round_mode) {
  DCHECK((sign == 0) || (sign == 1));

  // Only the FPTieEven rounding mode is implemented.
  DCHECK(round_mode == FPTieEven);
  USE(round_mode);

  // Rounding can promote subnormals to normals, and normals to infinities. For
  // example, a double with exponent 127 (FLT_MAX_EXP) would appear to be
  // encodable as a float, but rounding based on the low-order mantissa bits
  // could make it overflow. With ties-to-even rounding, this value would become
  // an infinity.

  // ---- Rounding Method ----
  //
  // The exponent is irrelevant in the rounding operation, so we treat the
  // lowest-order bit that will fit into the result ('onebit') as having
  // the value '1'. Similarly, the highest-order bit that won't fit into
  // the result ('halfbit') has the value '0.5'. The 'point' sits between
  // 'onebit' and 'halfbit':
  //
  //            These bits fit into the result.
  //               |---------------------|
  //  mantissa = 0bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
  //                                     ||
  //                                    / |
  //                                   /  halfbit
  //                               onebit
  //
  // For subnormal outputs, the range of representable bits is smaller and
  // the position of onebit and halfbit depends on the exponent of the
  // input, but the method is otherwise similar.
  //
  //   onebit(frac)
  //     |
  //     | halfbit(frac)          halfbit(adjusted)
  //     | /                      /
  //     | |                      |
  //  0b00.0 (exact)      -> 0b00.0 (exact)                    -> 0b00
  //  0b00.0...           -> 0b00.0...                         -> 0b00
  //  0b00.1 (exact)      -> 0b00.0111..111                    -> 0b00
  //  0b00.1...           -> 0b00.1...                         -> 0b01
  //  0b01.0 (exact)      -> 0b01.0 (exact)                    -> 0b01
  //  0b01.0...           -> 0b01.0...                         -> 0b01
  //  0b01.1 (exact)      -> 0b01.1 (exact)                    -> 0b10
  //  0b01.1...           -> 0b01.1...                         -> 0b10
  //  0b10.0 (exact)      -> 0b10.0 (exact)                    -> 0b10
  //  0b10.0...           -> 0b10.0...                         -> 0b10
  //  0b10.1 (exact)      -> 0b10.0111..111                    -> 0b10
  //  0b10.1...           -> 0b10.1...                         -> 0b11
  //  0b11.0 (exact)      -> 0b11.0 (exact)                    -> 0b11
  //  ...                   /             |                      /   |
  //                       /              |                     /    |
  //                                                           /     |
  // adjusted = frac - (halfbit(mantissa) & ~onebit(frac));   /      |
  //
  //                   mantissa = (mantissa >> shift) + halfbit(adjusted);

  static const int mantissa_offset = 0;
  static const int exponent_offset = mantissa_offset + mbits;
  static const int sign_offset = exponent_offset + ebits;
  STATIC_ASSERT(sign_offset == (sizeof(T) * kByteSize - 1));

  // Bail out early for zero inputs.
  if (mantissa == 0) {
    return sign << sign_offset;
  }

  // If all bits in the exponent are set, the value is infinite or NaN.
  // This is true for all binary IEEE-754 formats.
  static const int infinite_exponent = (1 << ebits) - 1;
  static const int max_normal_exponent = infinite_exponent - 1;

  // Apply the exponent bias to encode it for the result. Doing this early makes
  // it easy to detect values that will be infinite or subnormal.
  exponent += max_normal_exponent >> 1;

  if (exponent > max_normal_exponent) {
    // Overflow: The input is too large for the result type to represent. The
    // FPTieEven rounding mode handles overflows using infinities.
    exponent = infinite_exponent;
    mantissa = 0;
    return (sign << sign_offset) |
           (exponent << exponent_offset) |
           (mantissa << mantissa_offset);
  }

  // Calculate the shift required to move the top mantissa bit to the proper
  // place in the destination type.
  const int highest_significant_bit = 63 - CountLeadingZeros(mantissa, 64);
  int shift = highest_significant_bit - mbits;

  if (exponent <= 0) {
    // The output will be subnormal (before rounding).

    // For subnormal outputs, the shift must be adjusted by the exponent. The +1
    // is necessary because the exponent of a subnormal value (encoded as 0) is
    // the same as the exponent of the smallest normal value (encoded as 1).
    shift += -exponent + 1;

    // Handle inputs that would produce a zero output.
    //
    // Shifts higher than highest_significant_bit+1 will always produce a zero
    // result. A shift of exactly highest_significant_bit+1 might produce a
    // non-zero result after rounding.
    if (shift > (highest_significant_bit + 1)) {
      // The result will always be +/-0.0.
      return sign << sign_offset;
    }

    // Properly encode the exponent for a subnormal output.
    exponent = 0;
  } else {
    // Clear the topmost mantissa bit, since this is not encoded in IEEE-754
    // normal values.
    mantissa &= ~(1UL << highest_significant_bit);
  }

  if (shift > 0) {
    // We have to shift the mantissa to the right. Some precision is lost, so we
    // need to apply rounding.
    uint64_t onebit_mantissa = (mantissa >> (shift)) & 1;
    uint64_t halfbit_mantissa = (mantissa >> (shift-1)) & 1;
    uint64_t adjusted = mantissa - (halfbit_mantissa & ~onebit_mantissa);
    T halfbit_adjusted = (adjusted >> (shift-1)) & 1;

    T result = (sign << sign_offset) |
               (exponent << exponent_offset) |
               ((mantissa >> shift) << mantissa_offset);

    // A very large mantissa can overflow during rounding. If this happens, the
    // exponent should be incremented and the mantissa set to 1.0 (encoded as
    // 0). Applying halfbit_adjusted after assembling the float has the nice
    // side-effect that this case is handled for free.
    //
    // This also handles cases where a very large finite value overflows to
    // infinity, or where a very large subnormal value overflows to become
    // normal.
    return result + halfbit_adjusted;
  } else {
    // We have to shift the mantissa to the left (or not at all). The input
    // mantissa is exactly representable in the output mantissa, so apply no
    // rounding correction.
    return (sign << sign_offset) |
           (exponent << exponent_offset) |
           ((mantissa << -shift) << mantissa_offset);
  }
}


// See FPRound for a description of this function.
static inline double FPRoundToDouble(int64_t sign, int64_t exponent,
                                     uint64_t mantissa, FPRounding round_mode) {
  int64_t bits =
      FPRound<int64_t, kDoubleExponentBits, kDoubleMantissaBits>(sign,
                                                                 exponent,
                                                                 mantissa,
                                                                 round_mode);
  return rawbits_to_double(bits);
}


// See FPRound for a description of this function.
static inline float FPRoundToFloat(int64_t sign, int64_t exponent,
                                   uint64_t mantissa, FPRounding round_mode) {
  int32_t bits =
      FPRound<int32_t, kFloatExponentBits, kFloatMantissaBits>(sign,
                                                               exponent,
                                                               mantissa,
                                                               round_mode);
  return rawbits_to_float(bits);
}


double Simulator::FixedToDouble(int64_t src, int fbits, FPRounding round) {
  if (src >= 0) {
    return UFixedToDouble(src, fbits, round);
  } else {
    // This works for all negative values, including INT64_MIN.
    return -UFixedToDouble(-src, fbits, round);
  }
}


double Simulator::UFixedToDouble(uint64_t src, int fbits, FPRounding round) {
  // An input of 0 is a special case because the result is effectively
  // subnormal: The exponent is encoded as 0 and there is no implicit 1 bit.
  if (src == 0) {
    return 0.0;
  }

  // Calculate the exponent. The highest significant bit will have the value
  // 2^exponent.
  const int highest_significant_bit = 63 - CountLeadingZeros(src, 64);
  const int64_t exponent = highest_significant_bit - fbits;

  return FPRoundToDouble(0, exponent, src, round);
}


float Simulator::FixedToFloat(int64_t src, int fbits, FPRounding round) {
  if (src >= 0) {
    return UFixedToFloat(src, fbits, round);
  } else {
    // This works for all negative values, including INT64_MIN.
    return -UFixedToFloat(-src, fbits, round);
  }
}


float Simulator::UFixedToFloat(uint64_t src, int fbits, FPRounding round) {
  // An input of 0 is a special case because the result is effectively
  // subnormal: The exponent is encoded as 0 and there is no implicit 1 bit.
  if (src == 0) {
    return 0.0f;
  }

  // Calculate the exponent. The highest significant bit will have the value
  // 2^exponent.
  const int highest_significant_bit = 63 - CountLeadingZeros(src, 64);
  const int32_t exponent = highest_significant_bit - fbits;

  return FPRoundToFloat(0, exponent, src, round);
}


double Simulator::FPRoundInt(double value, FPRounding round_mode) {
  if ((value == 0.0) || (value == kFP64PositiveInfinity) ||
      (value == kFP64NegativeInfinity)) {
    return value;
  } else if (std::isnan(value)) {
    return FPProcessNaN(value);
  }

  double int_result = floor(value);
  double error = value - int_result;
  switch (round_mode) {
    case FPTieAway: {
      // Take care of correctly handling the range ]-0.5, -0.0], which must
      // yield -0.0.
      if ((-0.5 < value) && (value < 0.0)) {
        int_result = -0.0;

      } else if ((error > 0.5) || ((error == 0.5) && (int_result >= 0.0))) {
        // If the error is greater than 0.5, or is equal to 0.5 and the integer
        // result is positive, round up.
        int_result++;
      }
      break;
    }
    case FPTieEven: {
      // Take care of correctly handling the range [-0.5, -0.0], which must
      // yield -0.0.
      if ((-0.5 <= value) && (value < 0.0)) {
        int_result = -0.0;

      // If the error is greater than 0.5, or is equal to 0.5 and the integer
      // result is odd, round up.
      } else if ((error > 0.5) ||
          ((error == 0.5) && (fmod(int_result, 2) != 0))) {
        int_result++;
      }
      break;
    }
    case FPZero: {
      // If value > 0 then we take floor(value)
      // otherwise, ceil(value)
      if (value < 0) {
         int_result = ceil(value);
      }
      break;
    }
    case FPNegativeInfinity: {
      // We always use floor(value).
      break;
    }
    default: UNIMPLEMENTED();
  }
  return int_result;
}


double Simulator::FPToDouble(float value) {
  switch (std::fpclassify(value)) {
    case FP_NAN: {
      if (fpcr().DN()) return kFP64DefaultNaN;

      // Convert NaNs as the processor would:
      //  - The sign is propagated.
      //  - The payload (mantissa) is transferred entirely, except that the top
      //    bit is forced to '1', making the result a quiet NaN. The unused
      //    (low-order) payload bits are set to 0.
      uint32_t raw = float_to_rawbits(value);

      uint64_t sign = raw >> 31;
      uint64_t exponent = (1 << 11) - 1;
      uint64_t payload = unsigned_bitextract_64(21, 0, raw);
      payload <<= (52 - 23);  // The unused low-order bits should be 0.
      payload |= (1L << 51);  // Force a quiet NaN.

      return rawbits_to_double((sign << 63) | (exponent << 52) | payload);
    }

    case FP_ZERO:
    case FP_NORMAL:
    case FP_SUBNORMAL:
    case FP_INFINITE: {
      // All other inputs are preserved in a standard cast, because every value
      // representable using an IEEE-754 float is also representable using an
      // IEEE-754 double.
      return static_cast<double>(value);
    }
  }

  UNREACHABLE();
  return static_cast<double>(value);
}


float Simulator::FPToFloat(double value, FPRounding round_mode) {
  // Only the FPTieEven rounding mode is implemented.
  DCHECK(round_mode == FPTieEven);
  USE(round_mode);

  switch (std::fpclassify(value)) {
    case FP_NAN: {
      if (fpcr().DN()) return kFP32DefaultNaN;

      // Convert NaNs as the processor would:
      //  - The sign is propagated.
      //  - The payload (mantissa) is transferred as much as possible, except
      //    that the top bit is forced to '1', making the result a quiet NaN.
      uint64_t raw = double_to_rawbits(value);

      uint32_t sign = raw >> 63;
      uint32_t exponent = (1 << 8) - 1;
      uint32_t payload = unsigned_bitextract_64(50, 52 - 23, raw);
      payload |= (1 << 22);   // Force a quiet NaN.

      return rawbits_to_float((sign << 31) | (exponent << 23) | payload);
    }

    case FP_ZERO:
    case FP_INFINITE: {
      // In a C++ cast, any value representable in the target type will be
      // unchanged. This is always the case for +/-0.0 and infinities.
      return static_cast<float>(value);
    }

    case FP_NORMAL:
    case FP_SUBNORMAL: {
      // Convert double-to-float as the processor would, assuming that FPCR.FZ
      // (flush-to-zero) is not set.
      uint64_t raw = double_to_rawbits(value);
      // Extract the IEEE-754 double components.
      uint32_t sign = raw >> 63;
      // Extract the exponent and remove the IEEE-754 encoding bias.
      int32_t exponent = unsigned_bitextract_64(62, 52, raw) - 1023;
      // Extract the mantissa and add the implicit '1' bit.
      uint64_t mantissa = unsigned_bitextract_64(51, 0, raw);
      if (std::fpclassify(value) == FP_NORMAL) {
        mantissa |= (1UL << 52);
      }
      return FPRoundToFloat(sign, exponent, mantissa, round_mode);
    }
  }

  UNREACHABLE();
  return value;
}


void Simulator::VisitFPDataProcessing2Source(Instruction* instr) {
  AssertSupportedFPCR();

  unsigned fd = instr->Rd();
  unsigned fn = instr->Rn();
  unsigned fm = instr->Rm();

  // Fmaxnm and Fminnm have special NaN handling.
  switch (instr->Mask(FPDataProcessing2SourceMask)) {
    case FMAXNM_s: set_sreg(fd, FPMaxNM(sreg(fn), sreg(fm))); return;
    case FMAXNM_d: set_dreg(fd, FPMaxNM(dreg(fn), dreg(fm))); return;
    case FMINNM_s: set_sreg(fd, FPMinNM(sreg(fn), sreg(fm))); return;
    case FMINNM_d: set_dreg(fd, FPMinNM(dreg(fn), dreg(fm))); return;
    default:
      break;    // Fall through.
  }

  if (FPProcessNaNs(instr)) return;

  switch (instr->Mask(FPDataProcessing2SourceMask)) {
    case FADD_s: set_sreg(fd, FPAdd(sreg(fn), sreg(fm))); break;
    case FADD_d: set_dreg(fd, FPAdd(dreg(fn), dreg(fm))); break;
    case FSUB_s: set_sreg(fd, FPSub(sreg(fn), sreg(fm))); break;
    case FSUB_d: set_dreg(fd, FPSub(dreg(fn), dreg(fm))); break;
    case FMUL_s: set_sreg(fd, FPMul(sreg(fn), sreg(fm))); break;
    case FMUL_d: set_dreg(fd, FPMul(dreg(fn), dreg(fm))); break;
    case FDIV_s: set_sreg(fd, FPDiv(sreg(fn), sreg(fm))); break;
    case FDIV_d: set_dreg(fd, FPDiv(dreg(fn), dreg(fm))); break;
    case FMAX_s: set_sreg(fd, FPMax(sreg(fn), sreg(fm))); break;
    case FMAX_d: set_dreg(fd, FPMax(dreg(fn), dreg(fm))); break;
    case FMIN_s: set_sreg(fd, FPMin(sreg(fn), sreg(fm))); break;
    case FMIN_d: set_dreg(fd, FPMin(dreg(fn), dreg(fm))); break;
    case FMAXNM_s:
    case FMAXNM_d:
    case FMINNM_s:
    case FMINNM_d:
      // These were handled before the standard FPProcessNaNs() stage.
      UNREACHABLE();
    default: UNIMPLEMENTED();
  }
}


void Simulator::VisitFPDataProcessing3Source(Instruction* instr) {
  AssertSupportedFPCR();

  unsigned fd = instr->Rd();
  unsigned fn = instr->Rn();
  unsigned fm = instr->Rm();
  unsigned fa = instr->Ra();

  switch (instr->Mask(FPDataProcessing3SourceMask)) {
    // fd = fa +/- (fn * fm)
    case FMADD_s: set_sreg(fd, FPMulAdd(sreg(fa), sreg(fn), sreg(fm))); break;
    case FMSUB_s: set_sreg(fd, FPMulAdd(sreg(fa), -sreg(fn), sreg(fm))); break;
    case FMADD_d: set_dreg(fd, FPMulAdd(dreg(fa), dreg(fn), dreg(fm))); break;
    case FMSUB_d: set_dreg(fd, FPMulAdd(dreg(fa), -dreg(fn), dreg(fm))); break;
    // Negated variants of the above.
    case FNMADD_s:
      set_sreg(fd, FPMulAdd(-sreg(fa), -sreg(fn), sreg(fm)));
      break;
    case FNMSUB_s:
      set_sreg(fd, FPMulAdd(-sreg(fa), sreg(fn), sreg(fm)));
      break;
    case FNMADD_d:
      set_dreg(fd, FPMulAdd(-dreg(fa), -dreg(fn), dreg(fm)));
      break;
    case FNMSUB_d:
      set_dreg(fd, FPMulAdd(-dreg(fa), dreg(fn), dreg(fm)));
      break;
    default: UNIMPLEMENTED();
  }
}


template <typename T>
T Simulator::FPAdd(T op1, T op2) {
  // NaNs should be handled elsewhere.
  DCHECK(!std::isnan(op1) && !std::isnan(op2));

  if (std::isinf(op1) && std::isinf(op2) && (op1 != op2)) {
    // inf + -inf returns the default NaN.
    return FPDefaultNaN<T>();
  } else {
    // Other cases should be handled by standard arithmetic.
    return op1 + op2;
  }
}


template <typename T>
T Simulator::FPDiv(T op1, T op2) {
  // NaNs should be handled elsewhere.
  DCHECK(!std::isnan(op1) && !std::isnan(op2));

  if ((std::isinf(op1) && std::isinf(op2)) || ((op1 == 0.0) && (op2 == 0.0))) {
    // inf / inf and 0.0 / 0.0 return the default NaN.
    return FPDefaultNaN<T>();
  } else {
    // Other cases should be handled by standard arithmetic.
    return op1 / op2;
  }
}


template <typename T>
T Simulator::FPMax(T a, T b) {
  // NaNs should be handled elsewhere.
  DCHECK(!std::isnan(a) && !std::isnan(b));

  if ((a == 0.0) && (b == 0.0) &&
      (copysign(1.0, a) != copysign(1.0, b))) {
    // a and b are zero, and the sign differs: return +0.0.
    return 0.0;
  } else {
    return (a > b) ? a : b;
  }
}


template <typename T>
T Simulator::FPMaxNM(T a, T b) {
  if (IsQuietNaN(a) && !IsQuietNaN(b)) {
    a = kFP64NegativeInfinity;
  } else if (!IsQuietNaN(a) && IsQuietNaN(b)) {
    b = kFP64NegativeInfinity;
  }

  T result = FPProcessNaNs(a, b);
  return std::isnan(result) ? result : FPMax(a, b);
}

template <typename T>
T Simulator::FPMin(T a, T b) {
  // NaNs should be handled elsewhere.
  DCHECK(!std::isnan(a) && !std::isnan(b));

  if ((a == 0.0) && (b == 0.0) &&
      (copysign(1.0, a) != copysign(1.0, b))) {
    // a and b are zero, and the sign differs: return -0.0.
    return -0.0;
  } else {
    return (a < b) ? a : b;
  }
}


template <typename T>
T Simulator::FPMinNM(T a, T b) {
  if (IsQuietNaN(a) && !IsQuietNaN(b)) {
    a = kFP64PositiveInfinity;
  } else if (!IsQuietNaN(a) && IsQuietNaN(b)) {
    b = kFP64PositiveInfinity;
  }

  T result = FPProcessNaNs(a, b);
  return std::isnan(result) ? result : FPMin(a, b);
}


template <typename T>
T Simulator::FPMul(T op1, T op2) {
  // NaNs should be handled elsewhere.
  DCHECK(!std::isnan(op1) && !std::isnan(op2));

  if ((std::isinf(op1) && (op2 == 0.0)) || (std::isinf(op2) && (op1 == 0.0))) {
    // inf * 0.0 returns the default NaN.
    return FPDefaultNaN<T>();
  } else {
    // Other cases should be handled by standard arithmetic.
    return op1 * op2;
  }
}


template<typename T>
T Simulator::FPMulAdd(T a, T op1, T op2) {
  T result = FPProcessNaNs3(a, op1, op2);

  T sign_a = copysign(1.0, a);
  T sign_prod = copysign(1.0, op1) * copysign(1.0, op2);
  bool isinf_prod = std::isinf(op1) || std::isinf(op2);
  bool operation_generates_nan =
      (std::isinf(op1) && (op2 == 0.0)) ||                      // inf * 0.0
      (std::isinf(op2) && (op1 == 0.0)) ||                      // 0.0 * inf
      (std::isinf(a) && isinf_prod && (sign_a != sign_prod));   // inf - inf

  if (std::isnan(result)) {
    // Generated NaNs override quiet NaNs propagated from a.
    if (operation_generates_nan && IsQuietNaN(a)) {
      return FPDefaultNaN<T>();
    } else {
      return result;
    }
  }

  // If the operation would produce a NaN, return the default NaN.
  if (operation_generates_nan) {
    return FPDefaultNaN<T>();
  }

  // Work around broken fma implementations for exact zero results: The sign of
  // exact 0.0 results is positive unless both a and op1 * op2 are negative.
  if (((op1 == 0.0) || (op2 == 0.0)) && (a == 0.0)) {
    return ((sign_a < 0) && (sign_prod < 0)) ? -0.0 : 0.0;
  }

  result = FusedMultiplyAdd(op1, op2, a);
  DCHECK(!std::isnan(result));

  // Work around broken fma implementations for rounded zero results: If a is
  // 0.0, the sign of the result is the sign of op1 * op2 before rounding.
  if ((a == 0.0) && (result == 0.0)) {
    return copysign(0.0, sign_prod);
  }

  return result;
}


template <typename T>
T Simulator::FPSqrt(T op) {
  if (std::isnan(op)) {
    return FPProcessNaN(op);
  } else if (op < 0.0) {
    return FPDefaultNaN<T>();
  } else {
    return std::sqrt(op);
  }
}


template <typename T>
T Simulator::FPSub(T op1, T op2) {
  // NaNs should be handled elsewhere.
  DCHECK(!std::isnan(op1) && !std::isnan(op2));

  if (std::isinf(op1) && std::isinf(op2) && (op1 == op2)) {
    // inf - inf returns the default NaN.
    return FPDefaultNaN<T>();
  } else {
    // Other cases should be handled by standard arithmetic.
    return op1 - op2;
  }
}


template <typename T>
T Simulator::FPProcessNaN(T op) {
  DCHECK(std::isnan(op));
  return fpcr().DN() ? FPDefaultNaN<T>() : ToQuietNaN(op);
}


template <typename T>
T Simulator::FPProcessNaNs(T op1, T op2) {
  if (IsSignallingNaN(op1)) {
    return FPProcessNaN(op1);
  } else if (IsSignallingNaN(op2)) {
    return FPProcessNaN(op2);
  } else if (std::isnan(op1)) {
    DCHECK(IsQuietNaN(op1));
    return FPProcessNaN(op1);
  } else if (std::isnan(op2)) {
    DCHECK(IsQuietNaN(op2));
    return FPProcessNaN(op2);
  } else {
    return 0.0;
  }
}


template <typename T>
T Simulator::FPProcessNaNs3(T op1, T op2, T op3) {
  if (IsSignallingNaN(op1)) {
    return FPProcessNaN(op1);
  } else if (IsSignallingNaN(op2)) {
    return FPProcessNaN(op2);
  } else if (IsSignallingNaN(op3)) {
    return FPProcessNaN(op3);
  } else if (std::isnan(op1)) {
    DCHECK(IsQuietNaN(op1));
    return FPProcessNaN(op1);
  } else if (std::isnan(op2)) {
    DCHECK(IsQuietNaN(op2));
    return FPProcessNaN(op2);
  } else if (std::isnan(op3)) {
    DCHECK(IsQuietNaN(op3));
    return FPProcessNaN(op3);
  } else {
    return 0.0;
  }
}


bool Simulator::FPProcessNaNs(Instruction* instr) {
  unsigned fd = instr->Rd();
  unsigned fn = instr->Rn();
  unsigned fm = instr->Rm();
  bool done = false;

  if (instr->Mask(FP64) == FP64) {
    double result = FPProcessNaNs(dreg(fn), dreg(fm));
    if (std::isnan(result)) {
      set_dreg(fd, result);
      done = true;
    }
  } else {
    float result = FPProcessNaNs(sreg(fn), sreg(fm));
    if (std::isnan(result)) {
      set_sreg(fd, result);
      done = true;
    }
  }

  return done;
}


void Simulator::VisitSystem(Instruction* instr) {
  // Some system instructions hijack their Op and Cp fields to represent a
  // range of immediates instead of indicating a different instruction. This
  // makes the decoding tricky.
  if (instr->Mask(SystemSysRegFMask) == SystemSysRegFixed) {
    switch (instr->Mask(SystemSysRegMask)) {
      case MRS: {
        switch (instr->ImmSystemRegister()) {
          case NZCV: set_xreg(instr->Rt(), nzcv().RawValue()); break;
          case FPCR: set_xreg(instr->Rt(), fpcr().RawValue()); break;
          default: UNIMPLEMENTED();
        }
        break;
      }
      case MSR: {
        switch (instr->ImmSystemRegister()) {
          case NZCV:
            nzcv().SetRawValue(xreg(instr->Rt()));
            LogSystemRegister(NZCV);
            break;
          case FPCR:
            fpcr().SetRawValue(xreg(instr->Rt()));
            LogSystemRegister(FPCR);
            break;
          default: UNIMPLEMENTED();
        }
        break;
      }
    }
  } else if (instr->Mask(SystemHintFMask) == SystemHintFixed) {
    DCHECK(instr->Mask(SystemHintMask) == HINT);
    switch (instr->ImmHint()) {
      case NOP: break;
      default: UNIMPLEMENTED();
    }
  } else if (instr->Mask(MemBarrierFMask) == MemBarrierFixed) {
    __sync_synchronize();
  } else {
    UNIMPLEMENTED();
  }
}


bool Simulator::GetValue(const char* desc, int64_t* value) {
  int regnum = CodeFromName(desc);
  if (regnum >= 0) {
    unsigned code = regnum;
    if (code == kZeroRegCode) {
      // Catch the zero register and return 0.
      *value = 0;
      return true;
    } else if (code == kSPRegInternalCode) {
      // Translate the stack pointer code to 31, for Reg31IsStackPointer.
      code = 31;
    }
    if (desc[0] == 'w') {
      *value = wreg(code, Reg31IsStackPointer);
    } else {
      *value = xreg(code, Reg31IsStackPointer);
    }
    return true;
  } else if (strncmp(desc, "0x", 2) == 0) {
    return SScanF(desc + 2, "%" SCNx64,
                  reinterpret_cast<uint64_t*>(value)) == 1;
  } else {
    return SScanF(desc, "%" SCNu64,
                  reinterpret_cast<uint64_t*>(value)) == 1;
  }
}


bool Simulator::PrintValue(const char* desc) {
  if (strcmp(desc, "csp") == 0) {
    DCHECK(CodeFromName(desc) == static_cast<int>(kSPRegInternalCode));
    PrintF(stream_, "%s csp:%s 0x%016" PRIx64 "%s\n",
        clr_reg_name, clr_reg_value, xreg(31, Reg31IsStackPointer), clr_normal);
    return true;
  } else if (strcmp(desc, "wcsp") == 0) {
    DCHECK(CodeFromName(desc) == static_cast<int>(kSPRegInternalCode));
    PrintF(stream_, "%s wcsp:%s 0x%08" PRIx32 "%s\n",
        clr_reg_name, clr_reg_value, wreg(31, Reg31IsStackPointer), clr_normal);
    return true;
  }

  int i = CodeFromName(desc);
  STATIC_ASSERT(kNumberOfRegisters == kNumberOfFPRegisters);
  if (i < 0 || static_cast<unsigned>(i) >= kNumberOfFPRegisters) return false;

  if (desc[0] == 'v') {
    PrintF(stream_, "%s %s:%s 0x%016" PRIx64 "%s (%s%s:%s %g%s %s:%s %g%s)\n",
        clr_fpreg_name, VRegNameForCode(i),
        clr_fpreg_value, double_to_rawbits(dreg(i)),
        clr_normal,
        clr_fpreg_name, DRegNameForCode(i),
        clr_fpreg_value, dreg(i),
        clr_fpreg_name, SRegNameForCode(i),
        clr_fpreg_value, sreg(i),
        clr_normal);
    return true;
  } else if (desc[0] == 'd') {
    PrintF(stream_, "%s %s:%s %g%s\n",
        clr_fpreg_name, DRegNameForCode(i),
        clr_fpreg_value, dreg(i),
        clr_normal);
    return true;
  } else if (desc[0] == 's') {
    PrintF(stream_, "%s %s:%s %g%s\n",
        clr_fpreg_name, SRegNameForCode(i),
        clr_fpreg_value, sreg(i),
        clr_normal);
    return true;
  } else if (desc[0] == 'w') {
    PrintF(stream_, "%s %s:%s 0x%08" PRIx32 "%s\n",
        clr_reg_name, WRegNameForCode(i), clr_reg_value, wreg(i), clr_normal);
    return true;
  } else {
    // X register names have a wide variety of starting characters, but anything
    // else will be an X register.
    PrintF(stream_, "%s %s:%s 0x%016" PRIx64 "%s\n",
        clr_reg_name, XRegNameForCode(i), clr_reg_value, xreg(i), clr_normal);
    return true;
  }
}


void Simulator::Debug() {
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

  bool done = false;
  bool cleared_log_disasm_bit = false;

  while (!done) {
    // Disassemble the next instruction to execute before doing anything else.
    PrintInstructionsAt(pc_, 1);
    // Read the command line.
    char* line = ReadLine("sim> ");
    if (line == NULL) {
      break;
    } else {
      // Repeat last command by default.
      char* last_input = last_debugger_input();
      if (strcmp(line, "\n") == 0 && (last_input != NULL)) {
        DeleteArray(line);
        line = last_input;
      } else {
        // Update the latest command ran
        set_last_debugger_input(line);
      }

      // Use sscanf to parse the individual parts of the command line. At the
      // moment no command expects more than two parameters.
      int argc = SScanF(line,
                        "%" XSTR(COMMAND_SIZE) "s "
                        "%" XSTR(ARG_SIZE) "s "
                        "%" XSTR(ARG_SIZE) "s",
                        cmd, arg1, arg2);

      // stepi / si ------------------------------------------------------------
      if ((strcmp(cmd, "si") == 0) || (strcmp(cmd, "stepi") == 0)) {
        // We are about to execute instructions, after which by default we
        // should increment the pc_. If it was set when reaching this debug
        // instruction, it has not been cleared because this instruction has not
        // completed yet. So clear it manually.
        pc_modified_ = false;

        if (argc == 1) {
          ExecuteInstruction();
        } else {
          int64_t number_of_instructions_to_execute = 1;
          GetValue(arg1, &number_of_instructions_to_execute);

          set_log_parameters(log_parameters() | LOG_DISASM);
          while (number_of_instructions_to_execute-- > 0) {
            ExecuteInstruction();
          }
          set_log_parameters(log_parameters() & ~LOG_DISASM);
          PrintF("\n");
        }

        // If it was necessary, the pc has already been updated or incremented
        // when executing the instruction. So we do not want it to be updated
        // again. It will be cleared when exiting.
        pc_modified_ = true;

      // next / n --------------------------------------------------------------
      } else if ((strcmp(cmd, "next") == 0) || (strcmp(cmd, "n") == 0)) {
        // Tell the simulator to break after the next executed BL.
        break_on_next_ = true;
        // Continue.
        done = true;

      // continue / cont / c ---------------------------------------------------
      } else if ((strcmp(cmd, "continue") == 0) ||
                 (strcmp(cmd, "cont") == 0) ||
                 (strcmp(cmd, "c") == 0)) {
        // Leave the debugger shell.
        done = true;

      // disassemble / disasm / di ---------------------------------------------
      } else if (strcmp(cmd, "disassemble") == 0 ||
                 strcmp(cmd, "disasm") == 0 ||
                 strcmp(cmd, "di") == 0) {
        int64_t n_of_instrs_to_disasm = 10;  // default value.
        int64_t address = reinterpret_cast<int64_t>(pc_);  // default value.
        if (argc >= 2) {  // disasm <n of instrs>
          GetValue(arg1, &n_of_instrs_to_disasm);
        }
        if (argc >= 3) {  // disasm <n of instrs> <address>
          GetValue(arg2, &address);
        }

        // Disassemble.
        PrintInstructionsAt(reinterpret_cast<Instruction*>(address),
                            n_of_instrs_to_disasm);
        PrintF("\n");

      // print / p -------------------------------------------------------------
      } else if ((strcmp(cmd, "print") == 0) || (strcmp(cmd, "p") == 0)) {
        if (argc == 2) {
          if (strcmp(arg1, "all") == 0) {
            PrintRegisters();
            PrintFPRegisters();
          } else {
            if (!PrintValue(arg1)) {
              PrintF("%s unrecognized\n", arg1);
            }
          }
        } else {
          PrintF(
            "print <register>\n"
            "    Print the content of a register. (alias 'p')\n"
            "    'print all' will print all registers.\n"
            "    Use 'printobject' to get more details about the value.\n");
        }

      // printobject / po ------------------------------------------------------
      } else if ((strcmp(cmd, "printobject") == 0) ||
                 (strcmp(cmd, "po") == 0)) {
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
          PrintF("printobject <value>\n"
                 "printobject <register>\n"
                 "    Print details about the value. (alias 'po')\n");
        }

      // stack / mem ----------------------------------------------------------
      } else if (strcmp(cmd, "stack") == 0 || strcmp(cmd, "mem") == 0) {
        int64_t* cur = NULL;
        int64_t* end = NULL;
        int next_arg = 1;

        if (strcmp(cmd, "stack") == 0) {
          cur = reinterpret_cast<int64_t*>(jssp());

        } else {  // "mem"
          int64_t value;
          if (!GetValue(arg1, &value)) {
            PrintF("%s unrecognized\n", arg1);
            continue;
          }
          cur = reinterpret_cast<int64_t*>(value);
          next_arg++;
        }

        int64_t words = 0;
        if (argc == next_arg) {
          words = 10;
        } else if (argc == next_arg + 1) {
          if (!GetValue(argv[next_arg], &words)) {
            PrintF("%s unrecognized\n", argv[next_arg]);
            PrintF("Printing 10 double words by default");
            words = 10;
          }
        } else {
          UNREACHABLE();
        }
        end = cur + words;

        while (cur < end) {
          PrintF("  0x%016" PRIx64 ":  0x%016" PRIx64 " %10" PRId64,
                 reinterpret_cast<uint64_t>(cur), *cur, *cur);
          HeapObject* obj = reinterpret_cast<HeapObject*>(*cur);
          int64_t value = *cur;
          Heap* current_heap = v8::internal::Isolate::Current()->heap();
          if (((value & 1) == 0) || current_heap->Contains(obj)) {
            PrintF(" (");
            if ((value & kSmiTagMask) == 0) {
              STATIC_ASSERT(kSmiValueSize == 32);
              int32_t untagged = (value >> kSmiShift) & 0xffffffff;
              PrintF("smi %" PRId32, untagged);
            } else {
              obj->ShortPrint();
            }
            PrintF(")");
          }
          PrintF("\n");
          cur++;
        }

      // trace / t -------------------------------------------------------------
      } else if (strcmp(cmd, "trace") == 0 || strcmp(cmd, "t") == 0) {
        if ((log_parameters() & (LOG_DISASM | LOG_REGS)) !=
            (LOG_DISASM | LOG_REGS)) {
          PrintF("Enabling disassembly and registers tracing\n");
          set_log_parameters(log_parameters() | LOG_DISASM | LOG_REGS);
        } else {
          PrintF("Disabling disassembly and registers tracing\n");
          set_log_parameters(log_parameters() & ~(LOG_DISASM | LOG_REGS));
        }

      // break / b -------------------------------------------------------------
      } else if (strcmp(cmd, "break") == 0 || strcmp(cmd, "b") == 0) {
        if (argc == 2) {
          int64_t value;
          if (GetValue(arg1, &value)) {
            SetBreakpoint(reinterpret_cast<Instruction*>(value));
          } else {
            PrintF("%s unrecognized\n", arg1);
          }
        } else {
          ListBreakpoints();
          PrintF("Use `break <address>` to set or disable a breakpoint\n");
        }

      // gdb -------------------------------------------------------------------
      } else if (strcmp(cmd, "gdb") == 0) {
        PrintF("Relinquishing control to gdb.\n");
        base::OS::DebugBreak();
        PrintF("Regaining control from gdb.\n");

      // sysregs ---------------------------------------------------------------
      } else if (strcmp(cmd, "sysregs") == 0) {
        PrintSystemRegisters();

      // help / h --------------------------------------------------------------
      } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
        PrintF(
          "stepi / si\n"
          "    stepi <n>\n"
          "    Step <n> instructions.\n"
          "next / n\n"
          "    Continue execution until a BL instruction is reached.\n"
          "    At this point a breakpoint is set just after this BL.\n"
          "    Then execution is resumed. It will probably later hit the\n"
          "    breakpoint just set.\n"
          "continue / cont / c\n"
          "    Continue execution from here.\n"
          "disassemble / disasm / di\n"
          "    disassemble <n> <address>\n"
          "    Disassemble <n> instructions from current <address>.\n"
          "    By default <n> is 20 and <address> is the current pc.\n"
          "print / p\n"
          "    print <register>\n"
          "    Print the content of a register.\n"
          "    'print all' will print all registers.\n"
          "    Use 'printobject' to get more details about the value.\n"
          "printobject / po\n"
          "    printobject <value>\n"
          "    printobject <register>\n"
          "    Print details about the value.\n"
          "stack\n"
          "    stack [<words>]\n"
          "    Dump stack content, default dump 10 words\n"
          "mem\n"
          "    mem <address> [<words>]\n"
          "    Dump memory content, default dump 10 words\n"
          "trace / t\n"
          "    Toggle disassembly and register tracing\n"
          "break / b\n"
          "    break : list all breakpoints\n"
          "    break <address> : set / enable / disable a breakpoint.\n"
          "gdb\n"
          "    Enter gdb.\n"
          "sysregs\n"
          "    Print all system registers (including NZCV).\n");
      } else {
        PrintF("Unknown command: %s\n", cmd);
        PrintF("Use 'help' for more information.\n");
      }
    }
    if (cleared_log_disasm_bit == true) {
      set_log_parameters(log_parameters_ | LOG_DISASM);
    }
  }
}


void Simulator::VisitException(Instruction* instr) {
  switch (instr->Mask(ExceptionMask)) {
    case HLT: {
      if (instr->ImmException() == kImmExceptionIsDebug) {
        // Read the arguments encoded inline in the instruction stream.
        uint32_t code;
        uint32_t parameters;

        memcpy(&code,
               pc_->InstructionAtOffset(kDebugCodeOffset),
               sizeof(code));
        memcpy(&parameters,
               pc_->InstructionAtOffset(kDebugParamsOffset),
               sizeof(parameters));
        char const *message =
            reinterpret_cast<char const*>(
                pc_->InstructionAtOffset(kDebugMessageOffset));

        // Always print something when we hit a debug point that breaks.
        // We are going to break, so printing something is not an issue in
        // terms of speed.
        if (FLAG_trace_sim_messages || FLAG_trace_sim || (parameters & BREAK)) {
          if (message != NULL) {
            PrintF(stream_,
                   "# %sDebugger hit %d: %s%s%s\n",
                   clr_debug_number,
                   code,
                   clr_debug_message,
                   message,
                   clr_normal);
          } else {
            PrintF(stream_,
                   "# %sDebugger hit %d.%s\n",
                   clr_debug_number,
                   code,
                   clr_normal);
          }
        }

        // Other options.
        switch (parameters & kDebuggerTracingDirectivesMask) {
          case TRACE_ENABLE:
            set_log_parameters(log_parameters() | parameters);
            if (parameters & LOG_SYS_REGS) { PrintSystemRegisters(); }
            if (parameters & LOG_REGS) { PrintRegisters(); }
            if (parameters & LOG_FP_REGS) { PrintFPRegisters(); }
            break;
          case TRACE_DISABLE:
            set_log_parameters(log_parameters() & ~parameters);
            break;
          case TRACE_OVERRIDE:
            set_log_parameters(parameters);
            break;
          default:
            // We don't support a one-shot LOG_DISASM.
            DCHECK((parameters & LOG_DISASM) == 0);
            // Don't print information that is already being traced.
            parameters &= ~log_parameters();
            // Print the requested information.
            if (parameters & LOG_SYS_REGS) PrintSystemRegisters();
            if (parameters & LOG_REGS) PrintRegisters();
            if (parameters & LOG_FP_REGS) PrintFPRegisters();
        }

        // The stop parameters are inlined in the code. Skip them:
        //  - Skip to the end of the message string.
        size_t size = kDebugMessageOffset + strlen(message) + 1;
        pc_ = pc_->InstructionAtOffset(RoundUp(size, kInstructionSize));
        //  - Verify that the unreachable marker is present.
        DCHECK(pc_->Mask(ExceptionMask) == HLT);
        DCHECK(pc_->ImmException() ==  kImmExceptionIsUnreachable);
        //  - Skip past the unreachable marker.
        set_pc(pc_->following());

        // Check if the debugger should break.
        if (parameters & BREAK) Debug();

      } else if (instr->ImmException() == kImmExceptionIsRedirectedCall) {
        DoRuntimeCall(instr);
      } else if (instr->ImmException() == kImmExceptionIsPrintf) {
        DoPrintf(instr);

      } else if (instr->ImmException() == kImmExceptionIsUnreachable) {
        fprintf(stream_, "Hit UNREACHABLE marker at PC=%p.\n",
                reinterpret_cast<void*>(pc_));
        abort();

      } else {
        base::OS::DebugBreak();
      }
      break;
    }

    default:
      UNIMPLEMENTED();
  }
}


void Simulator::DoPrintf(Instruction* instr) {
  DCHECK((instr->Mask(ExceptionMask) == HLT) &&
              (instr->ImmException() == kImmExceptionIsPrintf));

  // Read the arguments encoded inline in the instruction stream.
  uint32_t arg_count;
  uint32_t arg_pattern_list;
  STATIC_ASSERT(sizeof(*instr) == 1);
  memcpy(&arg_count,
         instr + kPrintfArgCountOffset,
         sizeof(arg_count));
  memcpy(&arg_pattern_list,
         instr + kPrintfArgPatternListOffset,
         sizeof(arg_pattern_list));

  DCHECK(arg_count <= kPrintfMaxArgCount);
  DCHECK((arg_pattern_list >> (kPrintfArgPatternBits * arg_count)) == 0);

  // We need to call the host printf function with a set of arguments defined by
  // arg_pattern_list. Because we don't know the types and sizes of the
  // arguments, this is very difficult to do in a robust and portable way. To
  // work around the problem, we pick apart the format string, and print one
  // format placeholder at a time.

  // Allocate space for the format string. We take a copy, so we can modify it.
  // Leave enough space for one extra character per expected argument (plus the
  // '\0' termination).
  const char * format_base = reg<const char *>(0);
  DCHECK(format_base != NULL);
  size_t length = strlen(format_base) + 1;
  char * const format = new char[length + arg_count];

  // A list of chunks, each with exactly one format placeholder.
  const char * chunks[kPrintfMaxArgCount];

  // Copy the format string and search for format placeholders.
  uint32_t placeholder_count = 0;
  char * format_scratch = format;
  for (size_t i = 0; i < length; i++) {
    if (format_base[i] != '%') {
      *format_scratch++ = format_base[i];
    } else {
      if (format_base[i + 1] == '%') {
        // Ignore explicit "%%" sequences.
        *format_scratch++ = format_base[i];

        if (placeholder_count == 0) {
          // The first chunk is passed to printf using "%s", so we need to
          // unescape "%%" sequences in this chunk. (Just skip the next '%'.)
          i++;
        } else {
          // Otherwise, pass through "%%" unchanged.
          *format_scratch++ = format_base[++i];
        }
      } else {
        CHECK(placeholder_count < arg_count);
        // Insert '\0' before placeholders, and store their locations.
        *format_scratch++ = '\0';
        chunks[placeholder_count++] = format_scratch;
        *format_scratch++ = format_base[i];
      }
    }
  }
  DCHECK(format_scratch <= (format + length + arg_count));
  CHECK(placeholder_count == arg_count);

  // Finally, call printf with each chunk, passing the appropriate register
  // argument. Normally, printf returns the number of bytes transmitted, so we
  // can emulate a single printf call by adding the result from each chunk. If
  // any call returns a negative (error) value, though, just return that value.

  fprintf(stream_, "%s", clr_printf);

  // Because '\0' is inserted before each placeholder, the first string in
  // 'format' contains no format placeholders and should be printed literally.
  int result = fprintf(stream_, "%s", format);
  int pcs_r = 1;      // Start at x1. x0 holds the format string.
  int pcs_f = 0;      // Start at d0.
  if (result >= 0) {
    for (uint32_t i = 0; i < placeholder_count; i++) {
      int part_result = -1;

      uint32_t arg_pattern = arg_pattern_list >> (i * kPrintfArgPatternBits);
      arg_pattern &= (1 << kPrintfArgPatternBits) - 1;
      switch (arg_pattern) {
        case kPrintfArgW:
          part_result = fprintf(stream_, chunks[i], wreg(pcs_r++));
          break;
        case kPrintfArgX:
          part_result = fprintf(stream_, chunks[i], xreg(pcs_r++));
          break;
        case kPrintfArgD:
          part_result = fprintf(stream_, chunks[i], dreg(pcs_f++));
          break;
        default: UNREACHABLE();
      }

      if (part_result < 0) {
        // Handle error values.
        result = part_result;
        break;
      }

      result += part_result;
    }
  }

  fprintf(stream_, "%s", clr_normal);

#ifdef DEBUG
  CorruptAllCallerSavedCPURegisters();
#endif

  // Printf returns its result in x0 (just like the C library's printf).
  set_xreg(0, result);

  // The printf parameters are inlined in the code, so skip them.
  set_pc(instr->InstructionAtOffset(kPrintfLength));

  // Set LR as if we'd just called a native printf function.
  set_lr(pc());

  delete[] format;
}


#endif  // USE_SIMULATOR

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
