// Copyright 2011 the V8 project authors. All rights reserved.
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


// Declares a Simulator for MIPS instructions if we are not generating a native
// MIPS binary. This Simulator allows us to run and debug MIPS code generation
// on regular desktop machines.
// V8 calls into generated code by "calling" the CALL_GENERATED_CODE macro,
// which will start execution in the Simulator or forwards to the real entry
// on a MIPS HW platform.

#ifndef V8_MIPS_SIMULATOR_MIPS_H_
#define V8_MIPS_SIMULATOR_MIPS_H_

#include "allocation.h"
#include "constants-mips.h"

#if !defined(USE_SIMULATOR)
// Running without a simulator on a native mips platform.

namespace v8 {
namespace internal {

// When running without a simulator we call the entry directly.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  entry(p0, p1, p2, p3, p4)

typedef int (*mips_regexp_matcher)(String*, int, const byte*, const byte*,
                                   void*, int*, Address, int, Isolate*);


// Call the generated regexp code directly. The code at the entry address
// should act as a function matching the type arm_regexp_matcher.
// The fifth argument is a dummy that reserves the space used for
// the return address added by the ExitFrame in native calls.
#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7) \
  (FUNCTION_CAST<mips_regexp_matcher>(entry)( \
      p0, p1, p2, p3, NULL, p4, p5, p6, p7))

#define TRY_CATCH_FROM_ADDRESS(try_catch_address) \
  reinterpret_cast<TryCatch*>(try_catch_address)

// The stack limit beyond which we will throw stack overflow errors in
// generated code. Because generated code on mips uses the C stack, we
// just use the C stack limit.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(Isolate* isolate,
                                            uintptr_t c_limit) {
    return c_limit;
  }

  static inline uintptr_t RegisterCTryCatch(uintptr_t try_catch_address) {
    return try_catch_address;
  }

  static inline void UnregisterCTryCatch() { }
};

} }  // namespace v8::internal

// Calculated the stack limit beyond which we will throw stack overflow errors.
// This macro must be called from a C++ method. It relies on being able to take
// the address of "this" to get a value on the current execution stack and then
// calculates the stack limit based on that value.
// NOTE: The check for overflow is not safe as there is no guarantee that the
// running thread has its stack in all memory up to address 0x00000000.
#define GENERATED_CODE_STACK_LIMIT(limit) \
  (reinterpret_cast<uintptr_t>(this) >= limit ? \
      reinterpret_cast<uintptr_t>(this) - limit : 0)

#else  // !defined(USE_SIMULATOR)
// Running with a simulator.

#include "hashmap.h"
#include "assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Utility functions

class CachePage {
 public:
  static const int LINE_VALID = 0;
  static const int LINE_INVALID = 1;

  static const int kPageShift = 12;
  static const int kPageSize = 1 << kPageShift;
  static const int kPageMask = kPageSize - 1;
  static const int kLineShift = 2;  // The cache line is only 4 bytes right now.
  static const int kLineLength = 1 << kLineShift;
  static const int kLineMask = kLineLength - 1;

  CachePage() {
    memset(&validity_map_, LINE_INVALID, sizeof(validity_map_));
  }

  char* ValidityByte(int offset) {
    return &validity_map_[offset >> kLineShift];
  }

  char* CachedData(int offset) {
    return &data_[offset];
  }

 private:
  char data_[kPageSize];   // The cached data.
  static const int kValidityMapSize = kPageSize >> kLineShift;
  char validity_map_[kValidityMapSize];  // One byte per line.
};

class Simulator {
 public:
  friend class MipsDebugger;

  // Registers are declared in order. See SMRL chapter 2.
  enum Register {
    no_reg = -1,
    zero_reg = 0,
    at,
    v0, v1,
    a0, a1, a2, a3,
    t0, t1, t2, t3, t4, t5, t6, t7,
    s0, s1, s2, s3, s4, s5, s6, s7,
    t8, t9,
    k0, k1,
    gp,
    sp,
    s8,
    ra,
    // LO, HI, and pc.
    LO,
    HI,
    pc,   // pc must be the last register.
    kNumSimuRegisters,
    // aliases
    fp = s8
  };

  // Coprocessor registers.
  // Generated code will always use doubles. So we will only use even registers.
  enum FPURegister {
    f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11,
    f12, f13, f14, f15,   // f12 and f14 are arguments FPURegisters.
    f16, f17, f18, f19, f20, f21, f22, f23, f24, f25,
    f26, f27, f28, f29, f30, f31,
    kNumFPURegisters
  };

  explicit Simulator(Isolate* isolate);
  ~Simulator();

  // The currently executing Simulator instance. Potentially there can be one
  // for each native thread.
  static Simulator* current(v8::internal::Isolate* isolate);

  // Accessors for register state. Reading the pc value adheres to the MIPS
  // architecture specification and is off by a 8 from the currently executing
  // instruction.
  void set_register(int reg, int32_t value);
  int32_t get_register(int reg) const;
  // Same for FPURegisters.
  void set_fpu_register(int fpureg, int32_t value);
  void set_fpu_register_float(int fpureg, float value);
  void set_fpu_register_double(int fpureg, double value);
  int32_t get_fpu_register(int fpureg) const;
  int64_t get_fpu_register_long(int fpureg) const;
  float get_fpu_register_float(int fpureg) const;
  double get_fpu_register_double(int fpureg) const;
  void set_fcsr_bit(uint32_t cc, bool value);
  bool test_fcsr_bit(uint32_t cc);
  bool set_fcsr_round_error(double original, double rounded);

  // Special case of set_register and get_register to access the raw PC value.
  void set_pc(int32_t value);
  int32_t get_pc() const;

  // Accessor to the internal simulator stack area.
  uintptr_t StackLimit() const;

  // Executes MIPS instructions until the PC reaches end_sim_pc.
  void Execute();

  // Call on program start.
  static void Initialize(Isolate* isolate);

  // V8 generally calls into generated JS code with 5 parameters and into
  // generated RegExp code with 7 parameters. This is a convenience function,
  // which sets up the simulator state and grabs the result on return.
  int32_t Call(byte* entry, int argument_count, ...);

  // Push an address onto the JS stack.
  uintptr_t PushAddress(uintptr_t address);

  // Pop an address from the JS stack.
  uintptr_t PopAddress();

  // ICache checking.
  static void FlushICache(v8::internal::HashMap* i_cache, void* start,
                          size_t size);

  // Returns true if pc register contains one of the 'special_values' defined
  // below (bad_ra, end_sim_pc).
  bool has_bad_pc() const;

 private:
  enum special_values {
    // Known bad pc value to ensure that the simulator does not execute
    // without being properly setup.
    bad_ra = -1,
    // A pc value used to signal the simulator to stop execution.  Generally
    // the ra is set to this value on transition from native C code to
    // simulated execution, so that the simulator can "return" to the native
    // C code.
    end_sim_pc = -2,
    // Unpredictable value.
    Unpredictable = 0xbadbeaf
  };

  // Unsupported instructions use Format to print an error and stop execution.
  void Format(Instruction* instr, const char* format);

  // Read and write memory.
  inline uint32_t ReadBU(int32_t addr);
  inline int32_t ReadB(int32_t addr);
  inline void WriteB(int32_t addr, uint8_t value);
  inline void WriteB(int32_t addr, int8_t value);

  inline uint16_t ReadHU(int32_t addr, Instruction* instr);
  inline int16_t ReadH(int32_t addr, Instruction* instr);
  // Note: Overloaded on the sign of the value.
  inline void WriteH(int32_t addr, uint16_t value, Instruction* instr);
  inline void WriteH(int32_t addr, int16_t value, Instruction* instr);

  inline int ReadW(int32_t addr, Instruction* instr);
  inline void WriteW(int32_t addr, int value, Instruction* instr);

  inline double ReadD(int32_t addr, Instruction* instr);
  inline void WriteD(int32_t addr, double value, Instruction* instr);

  // Operations depending on endianness.
  // Get Double Higher / Lower word.
  inline int32_t GetDoubleHIW(double* addr);
  inline int32_t GetDoubleLOW(double* addr);
  // Set Double Higher / Lower word.
  inline int32_t SetDoubleHIW(double* addr);
  inline int32_t SetDoubleLOW(double* addr);

  // Executing is handled based on the instruction type.
  void DecodeTypeRegister(Instruction* instr);

  // Helper function for DecodeTypeRegister.
  void ConfigureTypeRegister(Instruction* instr,
                             int32_t& alu_out,
                             int64_t& i64hilo,
                             uint64_t& u64hilo,
                             int32_t& next_pc,
                             bool& do_interrupt);

  void DecodeTypeImmediate(Instruction* instr);
  void DecodeTypeJump(Instruction* instr);

  // Used for breakpoints and traps.
  void SoftwareInterrupt(Instruction* instr);

  // Stop helper functions.
  bool IsWatchpoint(uint32_t code);
  void PrintWatchpoint(uint32_t code);
  void HandleStop(uint32_t code, Instruction* instr);
  bool IsStopInstruction(Instruction* instr);
  bool IsEnabledStop(uint32_t code);
  void EnableStop(uint32_t code);
  void DisableStop(uint32_t code);
  void IncreaseStopCounter(uint32_t code);
  void PrintStopInfo(uint32_t code);


  // Executes one instruction.
  void InstructionDecode(Instruction* instr);
  // Execute one instruction placed in a branch delay slot.
  void BranchDelayInstructionDecode(Instruction* instr) {
    if (instr->IsForbiddenInBranchDelay()) {
      V8_Fatal(__FILE__, __LINE__,
               "Eror:Unexpected %i opcode in a branch delay slot.",
               instr->OpcodeValue());
    }
    InstructionDecode(instr);
  }

  // ICache.
  static void CheckICache(v8::internal::HashMap* i_cache, Instruction* instr);
  static void FlushOnePage(v8::internal::HashMap* i_cache, intptr_t start,
                           int size);
  static CachePage* GetCachePage(v8::internal::HashMap* i_cache, void* page);

  enum Exception {
    none,
    kIntegerOverflow,
    kIntegerUnderflow,
    kDivideByZero,
    kNumExceptions
  };
  int16_t exceptions[kNumExceptions];

  // Exceptions.
  void SignalExceptions();

  // Runtime call support.
  static void* RedirectExternalReference(void* external_function,
                                         ExternalReference::Type type);

  // For use in calls that take double value arguments.
  void GetFpArgs(double* x, double* y);
  void GetFpArgs(double* x);
  void GetFpArgs(double* x, int32_t* y);
  void SetFpResult(const double& result);


  // Architecture state.
  // Registers.
  int32_t registers_[kNumSimuRegisters];
  // Coprocessor Registers.
  int32_t FPUregisters_[kNumFPURegisters];
  // FPU control register.
  uint32_t FCSR_;

  // Simulator support.
  // Allocate 1MB for stack.
  static const size_t stack_size_ = 1 * 1024*1024;
  char* stack_;
  bool pc_modified_;
  int icount_;
  int break_count_;

  // Icache simulation.
  v8::internal::HashMap* i_cache_;

  v8::internal::Isolate* isolate_;

  // Registered breakpoints.
  Instruction* break_pc_;
  Instr break_instr_;

  // Stop is disabled if bit 31 is set.
  static const uint32_t kStopDisabledBit = 1 << 31;

  // A stop is enabled, meaning the simulator will stop when meeting the
  // instruction, if bit 31 of watched_stops[code].count is unset.
  // The value watched_stops[code].count & ~(1 << 31) indicates how many times
  // the breakpoint was hit or gone through.
  struct StopCountAndDesc {
    uint32_t count;
    char* desc;
  };
  StopCountAndDesc watched_stops[kMaxStopCode + 1];
};


// When running with the simulator transition into simulated execution at this
// point.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
    reinterpret_cast<Object*>(Simulator::current(Isolate::Current())->Call( \
      FUNCTION_ADDR(entry), 5, p0, p1, p2, p3, p4))

#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7) \
    Simulator::current(Isolate::Current())->Call( \
        entry, 9, p0, p1, p2, p3, NULL, p4, p5, p6, p7)

#define TRY_CATCH_FROM_ADDRESS(try_catch_address)                              \
  try_catch_address == NULL ?                                                  \
      NULL : *(reinterpret_cast<TryCatch**>(try_catch_address))


// The simulator has its own stack. Thus it has a different stack limit from
// the C-based native code.  Setting the c_limit to indicate a very small
// stack cause stack overflow errors, since the simulator ignores the input.
// This is unlikely to be an issue in practice, though it might cause testing
// trouble down the line.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(Isolate* isolate,
                                            uintptr_t c_limit) {
    return Simulator::current(isolate)->StackLimit();
  }

  static inline uintptr_t RegisterCTryCatch(uintptr_t try_catch_address) {
    Simulator* sim = Simulator::current(Isolate::Current());
    return sim->PushAddress(try_catch_address);
  }

  static inline void UnregisterCTryCatch() {
    Simulator::current(Isolate::Current())->PopAddress();
  }
};

} }  // namespace v8::internal

#endif  // !defined(USE_SIMULATOR)
#endif  // V8_MIPS_SIMULATOR_MIPS_H_
