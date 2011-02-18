// Copyright 2009 the V8 project authors. All rights reserved.
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


// Declares a Simulator for ARM instructions if we are not generating a native
// ARM binary. This Simulator allows us to run and debug ARM code generation on
// regular desktop machines.
// V8 calls into generated code by "calling" the CALL_GENERATED_CODE macro,
// which will start execution in the Simulator or forwards to the real entry
// on a ARM HW platform.

#ifndef V8_ARM_SIMULATOR_ARM_H_
#define V8_ARM_SIMULATOR_ARM_H_

#include "allocation.h"

#if !defined(USE_SIMULATOR)
// Running without a simulator on a native arm platform.

namespace v8 {
namespace internal {

// When running without a simulator we call the entry directly.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  (entry(p0, p1, p2, p3, p4))

typedef int (*arm_regexp_matcher)(String*, int, const byte*, const byte*,
                                  void*, int*, Address, int);


// Call the generated regexp code directly. The code at the entry address
// should act as a function matching the type arm_regexp_matcher.
// The fifth argument is a dummy that reserves the space used for
// the return address added by the ExitFrame in native calls.
#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6) \
  (FUNCTION_CAST<arm_regexp_matcher>(entry)(p0, p1, p2, p3, NULL, p4, p5, p6))

#define TRY_CATCH_FROM_ADDRESS(try_catch_address) \
  (reinterpret_cast<TryCatch*>(try_catch_address))

// The stack limit beyond which we will throw stack overflow errors in
// generated code. Because generated code on arm uses the C stack, we
// just use the C stack limit.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(uintptr_t c_limit) {
    return c_limit;
  }

  static inline uintptr_t RegisterCTryCatch(uintptr_t try_catch_address) {
    return try_catch_address;
  }

  static inline void UnregisterCTryCatch() { }
};

} }  // namespace v8::internal

#else  // !defined(USE_SIMULATOR)
// Running with a simulator.

#include "constants-arm.h"
#include "hashmap.h"
#include "assembler.h"

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
  friend class Debugger;
  enum Register {
    no_reg = -1,
    r0 = 0, r1, r2, r3, r4, r5, r6, r7,
    r8, r9, r10, r11, r12, r13, r14, r15,
    num_registers,
    sp = 13,
    lr = 14,
    pc = 15,
    s0 = 0, s1, s2, s3, s4, s5, s6, s7,
    s8, s9, s10, s11, s12, s13, s14, s15,
    s16, s17, s18, s19, s20, s21, s22, s23,
    s24, s25, s26, s27, s28, s29, s30, s31,
    num_s_registers = 32,
    d0 = 0, d1, d2, d3, d4, d5, d6, d7,
    d8, d9, d10, d11, d12, d13, d14, d15,
    num_d_registers = 16
  };

  Simulator();
  ~Simulator();

  // The currently executing Simulator instance. Potentially there can be one
  // for each native thread.
  static Simulator* current();

  // Accessors for register state. Reading the pc value adheres to the ARM
  // architecture specification and is off by a 8 from the currently executing
  // instruction.
  void set_register(int reg, int32_t value);
  int32_t get_register(int reg) const;
  void set_dw_register(int dreg, const int* dbl);

  // Support for VFP.
  void set_s_register(int reg, unsigned int value);
  unsigned int get_s_register(int reg) const;
  void set_d_register_from_double(int dreg, const double& dbl);
  double get_double_from_d_register(int dreg);
  void set_s_register_from_float(int sreg, const float dbl);
  float get_float_from_s_register(int sreg);
  void set_s_register_from_sinteger(int reg, const int value);
  int get_sinteger_from_s_register(int reg);

  // Special case of set_register and get_register to access the raw PC value.
  void set_pc(int32_t value);
  int32_t get_pc() const;

  // Accessor to the internal simulator stack area.
  uintptr_t StackLimit() const;

  // Executes ARM instructions until the PC reaches end_sim_pc.
  void Execute();

  // Call on program start.
  static void Initialize();

  // V8 generally calls into generated JS code with 5 parameters and into
  // generated RegExp code with 7 parameters. This is a convenience function,
  // which sets up the simulator state and grabs the result on return.
  int32_t Call(byte* entry, int argument_count, ...);

  // Push an address onto the JS stack.
  uintptr_t PushAddress(uintptr_t address);

  // Pop an address from the JS stack.
  uintptr_t PopAddress();

  // ICache checking.
  static void FlushICache(void* start, size_t size);

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

  // Checks if the current instruction should be executed based on its
  // condition bits.
  bool ConditionallyExecute(Instruction* instr);

  // Helper functions to set the conditional flags in the architecture state.
  void SetNZFlags(int32_t val);
  void SetCFlag(bool val);
  void SetVFlag(bool val);
  bool CarryFrom(int32_t left, int32_t right);
  bool BorrowFrom(int32_t left, int32_t right);
  bool OverflowFrom(int32_t alu_out,
                    int32_t left,
                    int32_t right,
                    bool addition);

  // Support for VFP.
  void Compute_FPSCR_Flags(double val1, double val2);
  void Copy_FPSCR_to_APSR();

  // Helper functions to decode common "addressing" modes
  int32_t GetShiftRm(Instruction* instr, bool* carry_out);
  int32_t GetImm(Instruction* instr, bool* carry_out);
  void HandleRList(Instruction* instr, bool load);
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
  inline uint8_t ReadBU(int32_t addr);
  inline int8_t ReadB(int32_t addr);
  inline void WriteB(int32_t addr, uint8_t value);
  inline void WriteB(int32_t addr, int8_t value);

  inline uint16_t ReadHU(int32_t addr, Instruction* instr);
  inline int16_t ReadH(int32_t addr, Instruction* instr);
  // Note: Overloaded on the sign of the value.
  inline void WriteH(int32_t addr, uint16_t value, Instruction* instr);
  inline void WriteH(int32_t addr, int16_t value, Instruction* instr);

  inline int ReadW(int32_t addr, Instruction* instr);
  inline void WriteW(int32_t addr, int value, Instruction* instr);

  int32_t* ReadDW(int32_t addr);
  void WriteDW(int32_t addr, int32_t value1, int32_t value2);

  // Executing is handled based on the instruction type.
  // Both type 0 and type 1 rolled into one.
  void DecodeType01(Instruction* instr);
  void DecodeType2(Instruction* instr);
  void DecodeType3(Instruction* instr);
  void DecodeType4(Instruction* instr);
  void DecodeType5(Instruction* instr);
  void DecodeType6(Instruction* instr);
  void DecodeType7(Instruction* instr);

  // Support for VFP.
  void DecodeTypeVFP(Instruction* instr);
  void DecodeType6CoprocessorIns(Instruction* instr);

  void DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(Instruction* instr);
  void DecodeVCMP(Instruction* instr);
  void DecodeVCVTBetweenDoubleAndSingle(Instruction* instr);
  void DecodeVCVTBetweenFloatingPointAndInteger(Instruction* instr);

  // Executes one instruction.
  void InstructionDecode(Instruction* instr);

  // ICache.
  static void CheckICache(Instruction* instr);
  static void FlushOnePage(intptr_t start, int size);
  static CachePage* GetCachePage(void* page);

  // Runtime call support.
  static void* RedirectExternalReference(
      void* external_function,
      v8::internal::ExternalReference::Type type);

  // For use in calls that take two double values, constructed from r0, r1, r2
  // and r3.
  void GetFpArgs(double* x, double* y);
  void SetFpResult(const double& result);
  void TrashCallerSaveRegisters();

  // Architecture state.
  // Saturating instructions require a Q flag to indicate saturation.
  // There is currently no way to read the CPSR directly, and thus read the Q
  // flag, so this is left unimplemented.
  int32_t registers_[16];
  bool n_flag_;
  bool z_flag_;
  bool c_flag_;
  bool v_flag_;

  // VFP architecture state.
  unsigned int vfp_register[num_s_registers];
  bool n_flag_FPSCR_;
  bool z_flag_FPSCR_;
  bool c_flag_FPSCR_;
  bool v_flag_FPSCR_;

  // VFP rounding mode. See ARM DDI 0406B Page A2-29.
  VFPRoundingMode FPSCR_rounding_mode_;

  // VFP FP exception flags architecture state.
  bool inv_op_vfp_flag_;
  bool div_zero_vfp_flag_;
  bool overflow_vfp_flag_;
  bool underflow_vfp_flag_;
  bool inexact_vfp_flag_;

  // Simulator support.
  char* stack_;
  bool pc_modified_;
  int icount_;
  static bool initialized_;

  // Icache simulation
  static v8::internal::HashMap* i_cache_;

  // Registered breakpoints.
  Instruction* break_pc_;
  Instr break_instr_;

  // A stop is watched if its code is less than kNumOfWatchedStops.
  // Only watched stops support enabling/disabling and the counter feature.
  static const uint32_t kNumOfWatchedStops = 256;

  // Breakpoint is disabled if bit 31 is set.
  static const uint32_t kStopDisabledBit = 1 << 31;

  // A stop is enabled, meaning the simulator will stop when meeting the
  // instruction, if bit 31 of watched_stops[code].count is unset.
  // The value watched_stops[code].count & ~(1 << 31) indicates how many times
  // the breakpoint was hit or gone through.
  struct StopCountAndDesc {
    uint32_t count;
    char* desc;
  };
  StopCountAndDesc watched_stops[kNumOfWatchedStops];
};


// When running with the simulator transition into simulated execution at this
// point.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  reinterpret_cast<Object*>(Simulator::current()->Call( \
      FUNCTION_ADDR(entry), 5, p0, p1, p2, p3, p4))

#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6) \
  Simulator::current()->Call(entry, 8, p0, p1, p2, p3, NULL, p4, p5, p6)

#define TRY_CATCH_FROM_ADDRESS(try_catch_address) \
  try_catch_address == \
      NULL ? NULL : *(reinterpret_cast<TryCatch**>(try_catch_address))


// The simulator has its own stack. Thus it has a different stack limit from
// the C-based native code.  Setting the c_limit to indicate a very small
// stack cause stack overflow errors, since the simulator ignores the input.
// This is unlikely to be an issue in practice, though it might cause testing
// trouble down the line.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(uintptr_t c_limit) {
    return Simulator::current()->StackLimit();
  }

  static inline uintptr_t RegisterCTryCatch(uintptr_t try_catch_address) {
    Simulator* sim = Simulator::current();
    return sim->PushAddress(try_catch_address);
  }

  static inline void UnregisterCTryCatch() {
    Simulator::current()->PopAddress();
  }
};

} }  // namespace v8::internal

#endif  // !defined(USE_SIMULATOR)
#endif  // V8_ARM_SIMULATOR_ARM_H_
