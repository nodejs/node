// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Declares a Simulator for ARM instructions if we are not generating a native
// ARM binary. This Simulator allows us to run and debug ARM code generation on
// regular desktop machines.
// V8 calls into generated code by "calling" the CALL_GENERATED_CODE macro,
// which will start execution in the Simulator or forwards to the real entry
// on a ARM HW platform.

#ifndef V8_ARM_SIMULATOR_ARM_H_
#define V8_ARM_SIMULATOR_ARM_H_

#include "src/allocation.h"

#if !defined(USE_SIMULATOR)
// Running without a simulator on a native arm platform.

namespace v8 {
namespace internal {

// When running without a simulator we call the entry directly.
#define CALL_GENERATED_CODE(isolate, entry, p0, p1, p2, p3, p4) \
  (entry(p0, p1, p2, p3, p4))

typedef int (*arm_regexp_matcher)(String*, int, const byte*, const byte*,
                                  void*, int*, int, Address, int, Isolate*);


// Call the generated regexp code directly. The code at the entry address
// should act as a function matching the type arm_regexp_matcher.
// The fifth argument is a dummy that reserves the space used for
// the return address added by the ExitFrame in native calls.
#define CALL_GENERATED_REGEXP_CODE(isolate, entry, p0, p1, p2, p3, p4, p5, p6, \
                                   p7, p8)                                     \
  (FUNCTION_CAST<arm_regexp_matcher>(entry)(p0, p1, p2, p3, NULL, p4, p5, p6,  \
                                            p7, p8))

// The stack limit beyond which we will throw stack overflow errors in
// generated code. Because generated code on arm uses the C stack, we
// just use the C stack limit.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(v8::internal::Isolate* isolate,
                                            uintptr_t c_limit) {
    USE(isolate);
    return c_limit;
  }

  static inline uintptr_t RegisterCTryCatch(v8::internal::Isolate* isolate,
                                            uintptr_t try_catch_address) {
    USE(isolate);
    return try_catch_address;
  }

  static inline void UnregisterCTryCatch(v8::internal::Isolate* isolate) {
    USE(isolate);
  }
};

}  // namespace internal
}  // namespace v8

#else  // !defined(USE_SIMULATOR)
// Running with a simulator.

#include "src/arm/constants-arm.h"
#include "src/assembler.h"
#include "src/hashmap.h"

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
  friend class ArmDebugger;
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
    d16, d17, d18, d19, d20, d21, d22, d23,
    d24, d25, d26, d27, d28, d29, d30, d31,
    num_d_registers = 32,
    q0 = 0, q1, q2, q3, q4, q5, q6, q7,
    q8, q9, q10, q11, q12, q13, q14, q15,
    num_q_registers = 16
  };

  explicit Simulator(Isolate* isolate);
  ~Simulator();

  // The currently executing Simulator instance. Potentially there can be one
  // for each native thread.
  static Simulator* current(v8::internal::Isolate* isolate);

  // Accessors for register state. Reading the pc value adheres to the ARM
  // architecture specification and is off by a 8 from the currently executing
  // instruction.
  void set_register(int reg, int32_t value);
  int32_t get_register(int reg) const;
  double get_double_from_register_pair(int reg);
  void set_register_pair_from_double(int reg, double* value);
  void set_dw_register(int dreg, const int* dbl);

  // Support for VFP.
  void get_d_register(int dreg, uint64_t* value);
  void set_d_register(int dreg, const uint64_t* value);
  void get_d_register(int dreg, uint32_t* value);
  void set_d_register(int dreg, const uint32_t* value);
  void get_q_register(int qreg, uint64_t* value);
  void set_q_register(int qreg, const uint64_t* value);
  void get_q_register(int qreg, uint32_t* value);
  void set_q_register(int qreg, const uint32_t* value);

  void set_s_register(int reg, unsigned int value);
  unsigned int get_s_register(int reg) const;

  void set_d_register_from_double(int dreg, const double& dbl) {
    SetVFPRegister<double, 2>(dreg, dbl);
  }

  double get_double_from_d_register(int dreg) {
    return GetFromVFPRegister<double, 2>(dreg);
  }

  void set_s_register_from_float(int sreg, const float flt) {
    SetVFPRegister<float, 1>(sreg, flt);
  }

  float get_float_from_s_register(int sreg) {
    return GetFromVFPRegister<float, 1>(sreg);
  }

  void set_s_register_from_sinteger(int sreg, const int sint) {
    SetVFPRegister<int, 1>(sreg, sint);
  }

  int get_sinteger_from_s_register(int sreg) {
    return GetFromVFPRegister<int, 1>(sreg);
  }

  // Special case of set_register and get_register to access the raw PC value.
  void set_pc(int32_t value);
  int32_t get_pc() const;

  Address get_sp() const {
    return reinterpret_cast<Address>(static_cast<intptr_t>(get_register(sp)));
  }

  // Accessor to the internal simulator stack area.
  uintptr_t StackLimit(uintptr_t c_limit) const;

  // Executes ARM instructions until the PC reaches end_sim_pc.
  void Execute();

  // Call on program start.
  static void Initialize(Isolate* isolate);

  static void TearDown(HashMap* i_cache, Redirection* first);

  // V8 generally calls into generated JS code with 5 parameters and into
  // generated RegExp code with 7 parameters. This is a convenience function,
  // which sets up the simulator state and grabs the result on return.
  int32_t Call(byte* entry, int argument_count, ...);
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

  // EABI variant for double arguments in use.
  bool use_eabi_hardfloat() {
#if USE_EABI_HARDFLOAT
    return true;
#else
    return false;
#endif
  }

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
  inline bool ConditionallyExecute(Instruction* instr);

  // Helper functions to set the conditional flags in the architecture state.
  void SetNZFlags(int32_t val);
  void SetCFlag(bool val);
  void SetVFlag(bool val);
  bool CarryFrom(int32_t left, int32_t right, int32_t carry = 0);
  bool BorrowFrom(int32_t left, int32_t right);
  bool OverflowFrom(int32_t alu_out,
                    int32_t left,
                    int32_t right,
                    bool addition);

  inline int GetCarry() {
    return c_flag_ ? 1 : 0;
  }

  // Support for VFP.
  void Compute_FPSCR_Flags(float val1, float val2);
  void Compute_FPSCR_Flags(double val1, double val2);
  void Copy_FPSCR_to_APSR();
  inline float canonicalizeNaN(float value);
  inline double canonicalizeNaN(double value);

  // Helper functions to decode common "addressing" modes
  int32_t GetShiftRm(Instruction* instr, bool* carry_out);
  int32_t GetImm(Instruction* instr, bool* carry_out);
  int32_t ProcessPU(Instruction* instr,
                    int num_regs,
                    int operand_size,
                    intptr_t* start_address,
                    intptr_t* end_address);
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
  void DecodeSpecialCondition(Instruction* instr);

  void DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(Instruction* instr);
  void DecodeVCMP(Instruction* instr);
  void DecodeVCVTBetweenDoubleAndSingle(Instruction* instr);
  void DecodeVCVTBetweenFloatingPointAndInteger(Instruction* instr);

  // Executes one instruction.
  void InstructionDecode(Instruction* instr);

  // ICache.
  static void CheckICache(v8::internal::HashMap* i_cache, Instruction* instr);
  static void FlushOnePage(v8::internal::HashMap* i_cache, intptr_t start,
                           int size);
  static CachePage* GetCachePage(v8::internal::HashMap* i_cache, void* page);

  // Runtime call support.
  static void* RedirectExternalReference(
      Isolate* isolate, void* external_function,
      v8::internal::ExternalReference::Type type);

  // Handle arguments and return value for runtime FP functions.
  void GetFpArgs(double* x, double* y, int32_t* z);
  void SetFpResult(const double& result);
  void TrashCallerSaveRegisters();

  template<class ReturnType, int register_size>
      ReturnType GetFromVFPRegister(int reg_index);

  template<class InputType, int register_size>
      void SetVFPRegister(int reg_index, const InputType& value);

  void CallInternal(byte* entry);

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
  unsigned int vfp_registers_[num_d_registers * 2];
  bool n_flag_FPSCR_;
  bool z_flag_FPSCR_;
  bool c_flag_FPSCR_;
  bool v_flag_FPSCR_;

  // VFP rounding mode. See ARM DDI 0406B Page A2-29.
  VFPRoundingMode FPSCR_rounding_mode_;
  bool FPSCR_default_NaN_mode_;

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
#define CALL_GENERATED_CODE(isolate, entry, p0, p1, p2, p3, p4) \
  reinterpret_cast<Object*>(Simulator::current(isolate)->Call(  \
      FUNCTION_ADDR(entry), 5, p0, p1, p2, p3, p4))

#define CALL_GENERATED_FP_INT(isolate, entry, p0, p1) \
  Simulator::current(isolate)->CallFPReturnsInt(FUNCTION_ADDR(entry), p0, p1)

#define CALL_GENERATED_REGEXP_CODE(isolate, entry, p0, p1, p2, p3, p4, p5, p6, \
                                   p7, p8)                                     \
  Simulator::current(isolate)                                                  \
      ->Call(entry, 10, p0, p1, p2, p3, NULL, p4, p5, p6, p7, p8)


// The simulator has its own stack. Thus it has a different stack limit from
// the C-based native code.  The JS-based limit normally points near the end of
// the simulator stack.  When the C-based limit is exhausted we reflect that by
// lowering the JS-based limit as well, to make stack checks trigger.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(v8::internal::Isolate* isolate,
                                            uintptr_t c_limit) {
    return Simulator::current(isolate)->StackLimit(c_limit);
  }

  static inline uintptr_t RegisterCTryCatch(v8::internal::Isolate* isolate,
                                            uintptr_t try_catch_address) {
    Simulator* sim = Simulator::current(isolate);
    return sim->PushAddress(try_catch_address);
  }

  static inline void UnregisterCTryCatch(v8::internal::Isolate* isolate) {
    Simulator::current(isolate)->PopAddress();
  }
};

}  // namespace internal
}  // namespace v8

#endif  // !defined(USE_SIMULATOR)
#endif  // V8_ARM_SIMULATOR_ARM_H_
