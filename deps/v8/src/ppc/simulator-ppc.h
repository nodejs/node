// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Declares a Simulator for PPC instructions if we are not generating a native
// PPC binary. This Simulator allows us to run and debug PPC code generation on
// regular desktop machines.
// V8 calls into generated code by "calling" the CALL_GENERATED_CODE macro,
// which will start execution in the Simulator or forwards to the real entry
// on a PPC HW platform.

#ifndef V8_PPC_SIMULATOR_PPC_H_
#define V8_PPC_SIMULATOR_PPC_H_

#include "src/allocation.h"

#if !defined(USE_SIMULATOR)
// Running without a simulator on a native ppc platform.

namespace v8 {
namespace internal {

// When running without a simulator we call the entry directly.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  (entry(p0, p1, p2, p3, p4))

typedef int (*ppc_regexp_matcher)(String*, int, const byte*, const byte*, int*,
                                  int, Address, int, void*, Isolate*);


// Call the generated regexp code directly. The code at the entry address
// should act as a function matching the type ppc_regexp_matcher.
// The ninth argument is a dummy that reserves the space used for
// the return address added by the ExitFrame in native calls.
#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7, p8) \
  (FUNCTION_CAST<ppc_regexp_matcher>(entry)(p0, p1, p2, p3, p4, p5, p6, p7,   \
                                            NULL, p8))

// The stack limit beyond which we will throw stack overflow errors in
// generated code. Because generated code on ppc uses the C stack, we
// just use the C stack limit.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(v8::internal::Isolate* isolate,
                                            uintptr_t c_limit) {
    USE(isolate);
    return c_limit;
  }

  static inline uintptr_t RegisterCTryCatch(uintptr_t try_catch_address) {
    return try_catch_address;
  }

  static inline void UnregisterCTryCatch() {}
};
}
}  // namespace v8::internal

#else  // !defined(USE_SIMULATOR)
// Running with a simulator.

#include "src/assembler.h"
#include "src/hashmap.h"
#include "src/ppc/constants-ppc.h"

namespace v8 {
namespace internal {

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

  CachePage() { memset(&validity_map_, LINE_INVALID, sizeof(validity_map_)); }

  char* ValidityByte(int offset) {
    return &validity_map_[offset >> kLineShift];
  }

  char* CachedData(int offset) { return &data_[offset]; }

 private:
  char data_[kPageSize];  // The cached data.
  static const int kValidityMapSize = kPageSize >> kLineShift;
  char validity_map_[kValidityMapSize];  // One byte per line.
};


class Simulator {
 public:
  friend class PPCDebugger;
  enum Register {
    no_reg = -1,
    r0 = 0,
    sp,
    r2,
    r3,
    r4,
    r5,
    r6,
    r7,
    r8,
    r9,
    r10,
    r11,
    r12,
    r13,
    r14,
    r15,
    r16,
    r17,
    r18,
    r19,
    r20,
    r21,
    r22,
    r23,
    r24,
    r25,
    r26,
    r27,
    r28,
    r29,
    r30,
    fp,
    kNumGPRs = 32,
    d0 = 0,
    d1,
    d2,
    d3,
    d4,
    d5,
    d6,
    d7,
    d8,
    d9,
    d10,
    d11,
    d12,
    d13,
    d14,
    d15,
    d16,
    d17,
    d18,
    d19,
    d20,
    d21,
    d22,
    d23,
    d24,
    d25,
    d26,
    d27,
    d28,
    d29,
    d30,
    d31,
    kNumFPRs = 32
  };

  explicit Simulator(Isolate* isolate);
  ~Simulator();

  // The currently executing Simulator instance. Potentially there can be one
  // for each native thread.
  static Simulator* current(v8::internal::Isolate* isolate);

  // Accessors for register state.
  void set_register(int reg, intptr_t value);
  intptr_t get_register(int reg) const;
  double get_double_from_register_pair(int reg);
  void set_d_register_from_double(int dreg, const double dbl) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    fp_registers_[dreg] = dbl;
  }
  double get_double_from_d_register(int dreg) { return fp_registers_[dreg]; }

  // Special case of set_register and get_register to access the raw PC value.
  void set_pc(intptr_t value);
  intptr_t get_pc() const;

  Address get_sp() {
    return reinterpret_cast<Address>(static_cast<intptr_t>(get_register(sp)));
  }

  // Accessor to the internal simulator stack area.
  uintptr_t StackLimit() const;

  // Executes PPC instructions until the PC reaches end_sim_pc.
  void Execute();

  // Call on program start.
  static void Initialize(Isolate* isolate);

  // V8 generally calls into generated JS code with 5 parameters and into
  // generated RegExp code with 7 parameters. This is a convenience function,
  // which sets up the simulator state and grabs the result on return.
  intptr_t Call(byte* entry, int argument_count, ...);
  // Alternative: call a 2-argument double function.
  void CallFP(byte* entry, double d0, double d1);
  int32_t CallFPReturnsInt(byte* entry, double d0, double d1);
  double CallFPReturnsDouble(byte* entry, double d0, double d1);

  // Push an address onto the JS stack.
  uintptr_t PushAddress(uintptr_t address);

  // Pop an address from the JS stack.
  uintptr_t PopAddress();

  // Debugger input.
  void set_last_debugger_input(char* input);
  char* last_debugger_input() { return last_debugger_input_; }

  // ICache checking.
  static void FlushICache(v8::internal::HashMap* i_cache, void* start,
                          size_t size);

  // Returns true if pc register contains one of the 'special_values' defined
  // below (bad_lr, end_sim_pc).
  bool has_bad_pc() const;

 private:
  enum special_values {
    // Known bad pc value to ensure that the simulator does not execute
    // without being properly setup.
    bad_lr = -1,
    // A pc value used to signal the simulator to stop execution.  Generally
    // the lr is set to this value on transition from native C code to
    // simulated execution, so that the simulator can "return" to the native
    // C code.
    end_sim_pc = -2
  };

  // Unsupported instructions use Format to print an error and stop execution.
  void Format(Instruction* instr, const char* format);

  // Helper functions to set the conditional flags in the architecture state.
  bool CarryFrom(int32_t left, int32_t right, int32_t carry = 0);
  bool BorrowFrom(int32_t left, int32_t right);
  bool OverflowFrom(int32_t alu_out, int32_t left, int32_t right,
                    bool addition);

  // Helper functions to decode common "addressing" modes
  int32_t GetShiftRm(Instruction* instr, bool* carry_out);
  int32_t GetImm(Instruction* instr, bool* carry_out);
  void ProcessPUW(Instruction* instr, int num_regs, int operand_size,
                  intptr_t* start_address, intptr_t* end_address);
  void HandleRList(Instruction* instr, bool load);
  void HandleVList(Instruction* inst);
  void SoftwareInterrupt(Instruction* instr);

  // Stop helper functions.
  inline bool isStopInstruction(Instruction* instr);
  inline bool isWatchedStop(uint32_t bkpt_code);
  inline bool isEnabledStop(uint32_t bkpt_code);
  inline void EnableStop(uint32_t bkpt_code);
  inline void DisableStop(uint32_t bkpt_code);
  inline void IncreaseStopCounter(uint32_t bkpt_code);
  void PrintStopInfo(uint32_t code);

  // Read and write memory.
  inline uint8_t ReadBU(intptr_t addr);
  inline int8_t ReadB(intptr_t addr);
  inline void WriteB(intptr_t addr, uint8_t value);
  inline void WriteB(intptr_t addr, int8_t value);

  inline uint16_t ReadHU(intptr_t addr, Instruction* instr);
  inline int16_t ReadH(intptr_t addr, Instruction* instr);
  // Note: Overloaded on the sign of the value.
  inline void WriteH(intptr_t addr, uint16_t value, Instruction* instr);
  inline void WriteH(intptr_t addr, int16_t value, Instruction* instr);

  inline uint32_t ReadWU(intptr_t addr, Instruction* instr);
  inline int32_t ReadW(intptr_t addr, Instruction* instr);
  inline void WriteW(intptr_t addr, uint32_t value, Instruction* instr);
  inline void WriteW(intptr_t addr, int32_t value, Instruction* instr);

  intptr_t* ReadDW(intptr_t addr);
  void WriteDW(intptr_t addr, int64_t value);

  void Trace(Instruction* instr);
  void SetCR0(intptr_t result, bool setSO = false);
  void ExecuteBranchConditional(Instruction* instr);
  void ExecuteExt1(Instruction* instr);
  bool ExecuteExt2_10bit(Instruction* instr);
  bool ExecuteExt2_9bit_part1(Instruction* instr);
  void ExecuteExt2_9bit_part2(Instruction* instr);
  void ExecuteExt2(Instruction* instr);
  void ExecuteExt4(Instruction* instr);
#if V8_TARGET_ARCH_PPC64
  void ExecuteExt5(Instruction* instr);
#endif
  void ExecuteGeneric(Instruction* instr);

  // Executes one instruction.
  void ExecuteInstruction(Instruction* instr);

  // ICache.
  static void CheckICache(v8::internal::HashMap* i_cache, Instruction* instr);
  static void FlushOnePage(v8::internal::HashMap* i_cache, intptr_t start,
                           int size);
  static CachePage* GetCachePage(v8::internal::HashMap* i_cache, void* page);

  // Runtime call support.
  static void* RedirectExternalReference(
      void* external_function, v8::internal::ExternalReference::Type type);

  // Handle arguments and return value for runtime FP functions.
  void GetFpArgs(double* x, double* y, intptr_t* z);
  void SetFpResult(const double& result);
  void TrashCallerSaveRegisters();

  void CallInternal(byte* entry);

  // Architecture state.
  // Saturating instructions require a Q flag to indicate saturation.
  // There is currently no way to read the CPSR directly, and thus read the Q
  // flag, so this is left unimplemented.
  intptr_t registers_[kNumGPRs];
  int32_t condition_reg_;
  int32_t fp_condition_reg_;
  intptr_t special_reg_lr_;
  intptr_t special_reg_pc_;
  intptr_t special_reg_ctr_;
  int32_t special_reg_xer_;

  double fp_registers_[kNumFPRs];

  // Simulator support.
  char* stack_;
  bool pc_modified_;
  int icount_;

  // Debugger input.
  char* last_debugger_input_;

  // Icache simulation
  v8::internal::HashMap* i_cache_;

  // Registered breakpoints.
  Instruction* break_pc_;
  Instr break_instr_;

  v8::internal::Isolate* isolate_;

  // A stop is watched if its code is less than kNumOfWatchedStops.
  // Only watched stops support enabling/disabling and the counter feature.
  static const uint32_t kNumOfWatchedStops = 256;

  // Breakpoint is disabled if bit 31 is set.
  static const uint32_t kStopDisabledBit = 1 << 31;

  // A stop is enabled, meaning the simulator will stop when meeting the
  // instruction, if bit 31 of watched_stops_[code].count is unset.
  // The value watched_stops_[code].count & ~(1 << 31) indicates how many times
  // the breakpoint was hit or gone through.
  struct StopCountAndDesc {
    uint32_t count;
    char* desc;
  };
  StopCountAndDesc watched_stops_[kNumOfWatchedStops];
};


// When running with the simulator transition into simulated execution at this
// point.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4)                    \
  reinterpret_cast<Object*>(Simulator::current(Isolate::Current())->Call( \
      FUNCTION_ADDR(entry), 5, (intptr_t)p0, (intptr_t)p1, (intptr_t)p2,  \
      (intptr_t)p3, (intptr_t)p4))

#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7, p8) \
  Simulator::current(Isolate::Current())                                      \
      ->Call(entry, 10, (intptr_t)p0, (intptr_t)p1, (intptr_t)p2,             \
             (intptr_t)p3, (intptr_t)p4, (intptr_t)p5, (intptr_t)p6,          \
             (intptr_t)p7, (intptr_t)NULL, (intptr_t)p8)


// The simulator has its own stack. Thus it has a different stack limit from
// the C-based native code.  Setting the c_limit to indicate a very small
// stack cause stack overflow errors, since the simulator ignores the input.
// This is unlikely to be an issue in practice, though it might cause testing
// trouble down the line.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(v8::internal::Isolate* isolate,
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
}
}  // namespace v8::internal

#endif  // !defined(USE_SIMULATOR)
#endif  // V8_PPC_SIMULATOR_PPC_H_
