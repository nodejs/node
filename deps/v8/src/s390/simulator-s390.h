// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declares a Simulator for S390 instructions if we are not generating a native
// S390 binary. This Simulator allows us to run and debug S390 code generation
// on regular desktop machines.
// V8 calls into generated code by "calling" the CALL_GENERATED_CODE macro,
// which will start execution in the Simulator or forwards to the real entry
// on a S390 hardware platform.

#ifndef V8_S390_SIMULATOR_S390_H_
#define V8_S390_SIMULATOR_S390_H_

#include "src/allocation.h"

#if !defined(USE_SIMULATOR)
// Running without a simulator on a native s390 platform.

namespace v8 {
namespace internal {

// When running without a simulator we call the entry directly.
#define CALL_GENERATED_CODE(isolate, entry, p0, p1, p2, p3, p4) \
  (entry(p0, p1, p2, p3, p4))

typedef int (*s390_regexp_matcher)(String*, int, const byte*, const byte*, int*,
                                   int, Address, int, Isolate*);

// Call the generated regexp code directly. The code at the entry address
// should act as a function matching the type ppc_regexp_matcher.
#define CALL_GENERATED_REGEXP_CODE(isolate, entry, p0, p1, p2, p3, p4, p5, p6, \
                                   p7, p8)                                     \
  (FUNCTION_CAST<s390_regexp_matcher>(entry)(p0, p1, p2, p3, p4, p5, p6, p7,   \
                                             p8))

// The stack limit beyond which we will throw stack overflow errors in
// generated code. Because generated code on s390 uses the C stack, we
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

#include "src/assembler.h"
#include "src/base/hashmap.h"
#include "src/s390/constants-s390.h"

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
  friend class S390Debugger;
  enum Register {
    no_reg = -1,
    r0 = 0,
    r1 = 1,
    r2 = 2,
    r3 = 3,
    r4 = 4,
    r5 = 5,
    r6 = 6,
    r7 = 7,
    r8 = 8,
    r9 = 9,
    r10 = 10,
    r11 = 11,
    r12 = 12,
    r13 = 13,
    r14 = 14,
    r15 = 15,
    fp = r11,
    ip = r12,
    cp = r13,
    ra = r14,
    sp = r15,  // name aliases
    kNumGPRs = 16,
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
    kNumFPRs = 16
  };

  explicit Simulator(Isolate* isolate);
  ~Simulator();

  // The currently executing Simulator instance. Potentially there can be one
  // for each native thread.
  static Simulator* current(v8::internal::Isolate* isolate);

  // Accessors for register state.
  void set_register(int reg, uint64_t value);
  uint64_t get_register(int reg) const;
  template <typename T>
  T get_low_register(int reg) const;
  template <typename T>
  T get_high_register(int reg) const;
  void set_low_register(int reg, uint32_t value);
  void set_high_register(int reg, uint32_t value);

  double get_double_from_register_pair(int reg);
  void set_d_register_from_double(int dreg, const double dbl) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    *bit_cast<double*>(&fp_registers_[dreg]) = dbl;
  }

  double get_double_from_d_register(int dreg) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    return *bit_cast<double*>(&fp_registers_[dreg]);
  }
  void set_d_register(int dreg, int64_t value) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    fp_registers_[dreg] = value;
  }
  int64_t get_d_register(int dreg) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    return fp_registers_[dreg];
  }

  void set_d_register_from_float32(int dreg, const float f) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);

    int32_t f_int = *bit_cast<int32_t*>(&f);
    int64_t finalval = static_cast<int64_t>(f_int) << 32;
    set_d_register(dreg, finalval);
  }

  float get_float32_from_d_register(int dreg) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);

    int64_t regval = get_d_register(dreg) >> 32;
    int32_t regval32 = static_cast<int32_t>(regval);
    return *bit_cast<float*>(&regval32);
  }

  // Special case of set_register and get_register to access the raw PC value.
  void set_pc(intptr_t value);
  intptr_t get_pc() const;

  Address get_sp() const {
    return reinterpret_cast<Address>(static_cast<intptr_t>(get_register(sp)));
  }

  // Accessor to the internal simulator stack area.
  uintptr_t StackLimit(uintptr_t c_limit) const;

  // Executes S390 instructions until the PC reaches end_sim_pc.
  void Execute();

  // Call on program start.
  static void Initialize(Isolate* isolate);

  static void TearDown(base::CustomMatcherHashMap* i_cache, Redirection* first);

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
  static void FlushICache(base::CustomMatcherHashMap* i_cache, void* start,
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
  template <typename T1>
  inline bool OverflowFromSigned(T1 alu_out, T1 left, T1 right, bool addition);

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

  // Byte Reverse
  inline int16_t ByteReverse(int16_t hword);
  inline int32_t ByteReverse(int32_t word);
  inline int64_t ByteReverse(int64_t dword);

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
  inline int64_t ReadW64(intptr_t addr, Instruction* instr);
  inline void WriteW(intptr_t addr, uint32_t value, Instruction* instr);
  inline void WriteW(intptr_t addr, int32_t value, Instruction* instr);

  inline int64_t ReadDW(intptr_t addr);
  inline double ReadDouble(intptr_t addr);
  inline float ReadFloat(intptr_t addr);
  inline void WriteDW(intptr_t addr, int64_t value);

  // S390
  void Trace(Instruction* instr);

  // Used by the CL**BR instructions.
  template <typename T1, typename T2>
  void SetS390RoundConditionCode(T1 r2_val, T2 max, T2 min) {
    condition_reg_ = 0;
    double r2_dval = static_cast<double>(r2_val);
    double dbl_min = static_cast<double>(min);
    double dbl_max = static_cast<double>(max);

    if (r2_dval == 0.0)
      condition_reg_ = 8;
    else if (r2_dval < 0.0 && r2_dval >= dbl_min && std::isfinite(r2_dval))
      condition_reg_ = 4;
    else if (r2_dval > 0.0 && r2_dval <= dbl_max && std::isfinite(r2_dval))
      condition_reg_ = 2;
    else
      condition_reg_ = 1;
  }

  template <typename T1>
  void SetS390RoundConditionCode(T1 r2_val, int64_t max, int64_t min) {
    condition_reg_ = 0;
    double r2_dval = static_cast<double>(r2_val);
    double dbl_min = static_cast<double>(min);
    double dbl_max = static_cast<double>(max);

    // Note that the IEEE 754 floating-point representations (both 32 and
    // 64 bit) cannot exactly represent INT64_MAX. The closest it can get
    // is INT64_max + 1. IEEE 754 FP can, though, represent INT64_MIN
    // exactly.

    // This is not an issue for INT32, as IEEE754 64-bit can represent
    // INT32_MAX and INT32_MIN with exact precision.

    if (r2_dval == 0.0)
      condition_reg_ = 8;
    else if (r2_dval < 0.0 && r2_dval >= dbl_min && std::isfinite(r2_dval))
      condition_reg_ = 4;
    else if (r2_dval > 0.0 && r2_dval < dbl_max && std::isfinite(r2_dval))
      condition_reg_ = 2;
    else
      condition_reg_ = 1;
  }

  // Used by the CL**BR instructions.
  template <typename T1, typename T2, typename T3>
  void SetS390ConvertConditionCode(T1 src, T2 dst, T3 max) {
    condition_reg_ = 0;
    if (src == static_cast<T1>(0.0)) {
      condition_reg_ |= 8;
    } else if (src < static_cast<T1>(0.0) && static_cast<T2>(src) == 0 &&
               std::isfinite(src)) {
      condition_reg_ |= 4;
    } else if (src > static_cast<T1>(0.0) && std::isfinite(src) &&
               src < static_cast<T1>(max)) {
      condition_reg_ |= 2;
    } else {
      condition_reg_ |= 1;
    }
  }

  template <typename T>
  void SetS390ConditionCode(T lhs, T rhs) {
    condition_reg_ = 0;
    if (lhs == rhs) {
      condition_reg_ |= CC_EQ;
    } else if (lhs < rhs) {
      condition_reg_ |= CC_LT;
    } else if (lhs > rhs) {
      condition_reg_ |= CC_GT;
    }

    // We get down here only for floating point
    // comparisons and the values are unordered
    // i.e. NaN
    if (condition_reg_ == 0) condition_reg_ = unordered;
  }

  // Used by arithmetic operations that use carry.
  template <typename T>
  void SetS390ConditionCodeCarry(T result, bool overflow) {
    condition_reg_ = 0;
    bool zero_result = (result == static_cast<T>(0));
    if (zero_result && !overflow) {
      condition_reg_ |= 8;
    } else if (!zero_result && !overflow) {
      condition_reg_ |= 4;
    } else if (zero_result && overflow) {
      condition_reg_ |= 2;
    } else if (!zero_result && overflow) {
      condition_reg_ |= 1;
    }
    if (condition_reg_ == 0) UNREACHABLE();
  }

  bool isNaN(double value) { return (value != value); }

  // Set the condition code for bitwise operations
  // CC0 is set if value == 0.
  // CC1 is set if value != 0.
  // CC2/CC3 are not set.
  template <typename T>
  void SetS390BitWiseConditionCode(T value) {
    condition_reg_ = 0;

    if (value == 0)
      condition_reg_ |= CC_EQ;
    else
      condition_reg_ |= CC_LT;
  }

  void SetS390OverflowCode(bool isOF) {
    if (isOF) condition_reg_ = CC_OF;
  }

  bool TestConditionCode(Condition mask) {
    // Check for unconditional branch
    if (mask == 0xf) return true;

    return (condition_reg_ & mask) != 0;
  }

  // Executes one instruction.
  void ExecuteInstruction(Instruction* instr, bool auto_incr_pc = true);

  // ICache.
  static void CheckICache(base::CustomMatcherHashMap* i_cache,
                          Instruction* instr);
  static void FlushOnePage(base::CustomMatcherHashMap* i_cache, intptr_t start,
                           int size);
  static CachePage* GetCachePage(base::CustomMatcherHashMap* i_cache,
                                 void* page);

  // Runtime call support. Uses the isolate in a thread-safe way.
  static void* RedirectExternalReference(
      Isolate* isolate, void* external_function,
      v8::internal::ExternalReference::Type type);

  // Handle arguments and return value for runtime FP functions.
  void GetFpArgs(double* x, double* y, intptr_t* z);
  void SetFpResult(const double& result);
  void TrashCallerSaveRegisters();

  void CallInternal(byte* entry, int reg_arg_count = 3);

  // Architecture state.
  // On z9 and higher and supported Linux on z Systems platforms, all registers
  // are 64-bit, even in 31-bit mode.
  uint64_t registers_[kNumGPRs];
  int64_t fp_registers_[kNumFPRs];

  // Condition Code register. In S390, the last 4 bits are used.
  int32_t condition_reg_;
  // Special register to track PC.
  intptr_t special_reg_pc_;

  // Simulator support.
  char* stack_;
  static const size_t stack_protection_size_ = 256 * kPointerSize;
  bool pc_modified_;
  int64_t icount_;

  // Debugger input.
  char* last_debugger_input_;

  // Icache simulation
  base::CustomMatcherHashMap* i_cache_;

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
  void DebugStart();

  int DecodeInstructionOriginal(Instruction* instr);
  int DecodeInstruction(Instruction* instr);
  int Evaluate_Unknown(Instruction* instr);
#define MAX_NUM_OPCODES (1 << 16)
  typedef int (Simulator::*EvaluateFuncType)(Instruction*);

  static EvaluateFuncType EvalTable[MAX_NUM_OPCODES];
  static void EvalTableInit();

#define EVALUATE(name) int Evaluate_##name(Instruction* instr)
#define EVALUATE_VRR_INSTRUCTIONS(name, op_name, op_value) EVALUATE(op_name);
  S390_VRR_C_OPCODE_LIST(EVALUATE_VRR_INSTRUCTIONS)
  S390_VRR_A_OPCODE_LIST(EVALUATE_VRR_INSTRUCTIONS)
#undef EVALUATE_VRR_INSTRUCTIONS

  EVALUATE(DUMY);
  EVALUATE(BKPT);
  EVALUATE(SPM);
  EVALUATE(BALR);
  EVALUATE(BCTR);
  EVALUATE(BCR);
  EVALUATE(SVC);
  EVALUATE(BSM);
  EVALUATE(BASSM);
  EVALUATE(BASR);
  EVALUATE(MVCL);
  EVALUATE(CLCL);
  EVALUATE(LPR);
  EVALUATE(LNR);
  EVALUATE(LTR);
  EVALUATE(LCR);
  EVALUATE(NR);
  EVALUATE(CLR);
  EVALUATE(OR);
  EVALUATE(XR);
  EVALUATE(LR);
  EVALUATE(CR);
  EVALUATE(AR);
  EVALUATE(SR);
  EVALUATE(MR);
  EVALUATE(DR);
  EVALUATE(ALR);
  EVALUATE(SLR);
  EVALUATE(LDR);
  EVALUATE(CDR);
  EVALUATE(LER);
  EVALUATE(STH);
  EVALUATE(LA);
  EVALUATE(STC);
  EVALUATE(IC_z);
  EVALUATE(EX);
  EVALUATE(BAL);
  EVALUATE(BCT);
  EVALUATE(BC);
  EVALUATE(LH);
  EVALUATE(CH);
  EVALUATE(AH);
  EVALUATE(SH);
  EVALUATE(MH);
  EVALUATE(BAS);
  EVALUATE(CVD);
  EVALUATE(CVB);
  EVALUATE(ST);
  EVALUATE(LAE);
  EVALUATE(N);
  EVALUATE(CL);
  EVALUATE(O);
  EVALUATE(X);
  EVALUATE(L);
  EVALUATE(C);
  EVALUATE(A);
  EVALUATE(S);
  EVALUATE(M);
  EVALUATE(D);
  EVALUATE(AL);
  EVALUATE(SL);
  EVALUATE(STD);
  EVALUATE(LD);
  EVALUATE(CD);
  EVALUATE(STE);
  EVALUATE(MS);
  EVALUATE(LE);
  EVALUATE(BRXH);
  EVALUATE(BRXLE);
  EVALUATE(BXH);
  EVALUATE(BXLE);
  EVALUATE(SRL);
  EVALUATE(SLL);
  EVALUATE(SRA);
  EVALUATE(SLA);
  EVALUATE(SRDL);
  EVALUATE(SLDL);
  EVALUATE(SRDA);
  EVALUATE(SLDA);
  EVALUATE(STM);
  EVALUATE(TM);
  EVALUATE(MVI);
  EVALUATE(TS);
  EVALUATE(NI);
  EVALUATE(CLI);
  EVALUATE(OI);
  EVALUATE(XI);
  EVALUATE(LM);
  EVALUATE(CS);
  EVALUATE(MVCLE);
  EVALUATE(CLCLE);
  EVALUATE(MC);
  EVALUATE(CDS);
  EVALUATE(STCM);
  EVALUATE(ICM);
  EVALUATE(BPRP);
  EVALUATE(BPP);
  EVALUATE(TRTR);
  EVALUATE(MVN);
  EVALUATE(MVC);
  EVALUATE(MVZ);
  EVALUATE(NC);
  EVALUATE(CLC);
  EVALUATE(OC);
  EVALUATE(XC);
  EVALUATE(MVCP);
  EVALUATE(TR);
  EVALUATE(TRT);
  EVALUATE(ED);
  EVALUATE(EDMK);
  EVALUATE(PKU);
  EVALUATE(UNPKU);
  EVALUATE(MVCIN);
  EVALUATE(PKA);
  EVALUATE(UNPKA);
  EVALUATE(PLO);
  EVALUATE(LMD);
  EVALUATE(SRP);
  EVALUATE(MVO);
  EVALUATE(PACK);
  EVALUATE(UNPK);
  EVALUATE(ZAP);
  EVALUATE(AP);
  EVALUATE(SP);
  EVALUATE(MP);
  EVALUATE(DP);
  EVALUATE(UPT);
  EVALUATE(PFPO);
  EVALUATE(IIHH);
  EVALUATE(IIHL);
  EVALUATE(IILH);
  EVALUATE(IILL);
  EVALUATE(NIHH);
  EVALUATE(NIHL);
  EVALUATE(NILH);
  EVALUATE(NILL);
  EVALUATE(OIHH);
  EVALUATE(OIHL);
  EVALUATE(OILH);
  EVALUATE(OILL);
  EVALUATE(LLIHH);
  EVALUATE(LLIHL);
  EVALUATE(LLILH);
  EVALUATE(LLILL);
  EVALUATE(TMLH);
  EVALUATE(TMLL);
  EVALUATE(TMHH);
  EVALUATE(TMHL);
  EVALUATE(BRC);
  EVALUATE(BRAS);
  EVALUATE(BRCT);
  EVALUATE(BRCTG);
  EVALUATE(LHI);
  EVALUATE(LGHI);
  EVALUATE(AHI);
  EVALUATE(AGHI);
  EVALUATE(MHI);
  EVALUATE(MGHI);
  EVALUATE(CHI);
  EVALUATE(CGHI);
  EVALUATE(LARL);
  EVALUATE(LGFI);
  EVALUATE(BRCL);
  EVALUATE(BRASL);
  EVALUATE(XIHF);
  EVALUATE(XILF);
  EVALUATE(IIHF);
  EVALUATE(IILF);
  EVALUATE(NIHF);
  EVALUATE(NILF);
  EVALUATE(OIHF);
  EVALUATE(OILF);
  EVALUATE(LLIHF);
  EVALUATE(LLILF);
  EVALUATE(MSGFI);
  EVALUATE(MSFI);
  EVALUATE(SLGFI);
  EVALUATE(SLFI);
  EVALUATE(AGFI);
  EVALUATE(AFI);
  EVALUATE(ALGFI);
  EVALUATE(ALFI);
  EVALUATE(CGFI);
  EVALUATE(CFI);
  EVALUATE(CLGFI);
  EVALUATE(CLFI);
  EVALUATE(LLHRL);
  EVALUATE(LGHRL);
  EVALUATE(LHRL);
  EVALUATE(LLGHRL);
  EVALUATE(STHRL);
  EVALUATE(LGRL);
  EVALUATE(STGRL);
  EVALUATE(LGFRL);
  EVALUATE(LRL);
  EVALUATE(LLGFRL);
  EVALUATE(STRL);
  EVALUATE(EXRL);
  EVALUATE(PFDRL);
  EVALUATE(CGHRL);
  EVALUATE(CHRL);
  EVALUATE(CGRL);
  EVALUATE(CGFRL);
  EVALUATE(ECTG);
  EVALUATE(CSST);
  EVALUATE(LPD);
  EVALUATE(LPDG);
  EVALUATE(BRCTH);
  EVALUATE(AIH);
  EVALUATE(ALSIH);
  EVALUATE(ALSIHN);
  EVALUATE(CIH);
  EVALUATE(CLIH);
  EVALUATE(STCK);
  EVALUATE(CFC);
  EVALUATE(IPM);
  EVALUATE(HSCH);
  EVALUATE(MSCH);
  EVALUATE(SSCH);
  EVALUATE(STSCH);
  EVALUATE(TSCH);
  EVALUATE(TPI);
  EVALUATE(SAL);
  EVALUATE(RSCH);
  EVALUATE(STCRW);
  EVALUATE(STCPS);
  EVALUATE(RCHP);
  EVALUATE(SCHM);
  EVALUATE(CKSM);
  EVALUATE(SAR);
  EVALUATE(EAR);
  EVALUATE(MSR);
  EVALUATE(MSRKC);
  EVALUATE(MVST);
  EVALUATE(CUSE);
  EVALUATE(SRST);
  EVALUATE(XSCH);
  EVALUATE(STCKE);
  EVALUATE(STCKF);
  EVALUATE(SRNM);
  EVALUATE(STFPC);
  EVALUATE(LFPC);
  EVALUATE(TRE);
  EVALUATE(CUUTF);
  EVALUATE(CUTFU);
  EVALUATE(STFLE);
  EVALUATE(SRNMB);
  EVALUATE(SRNMT);
  EVALUATE(LFAS);
  EVALUATE(PPA);
  EVALUATE(ETND);
  EVALUATE(TEND);
  EVALUATE(NIAI);
  EVALUATE(TABORT);
  EVALUATE(TRAP4);
  EVALUATE(LPEBR);
  EVALUATE(LNEBR);
  EVALUATE(LTEBR);
  EVALUATE(LCEBR);
  EVALUATE(LDEBR);
  EVALUATE(LXDBR);
  EVALUATE(LXEBR);
  EVALUATE(MXDBR);
  EVALUATE(KEBR);
  EVALUATE(CEBR);
  EVALUATE(AEBR);
  EVALUATE(SEBR);
  EVALUATE(MDEBR);
  EVALUATE(DEBR);
  EVALUATE(MAEBR);
  EVALUATE(MSEBR);
  EVALUATE(LPDBR);
  EVALUATE(LNDBR);
  EVALUATE(LTDBR);
  EVALUATE(LCDBR);
  EVALUATE(SQEBR);
  EVALUATE(SQDBR);
  EVALUATE(SQXBR);
  EVALUATE(MEEBR);
  EVALUATE(KDBR);
  EVALUATE(CDBR);
  EVALUATE(ADBR);
  EVALUATE(SDBR);
  EVALUATE(MDBR);
  EVALUATE(DDBR);
  EVALUATE(MADBR);
  EVALUATE(MSDBR);
  EVALUATE(LPXBR);
  EVALUATE(LNXBR);
  EVALUATE(LTXBR);
  EVALUATE(LCXBR);
  EVALUATE(LEDBRA);
  EVALUATE(LDXBRA);
  EVALUATE(LEXBRA);
  EVALUATE(FIXBRA);
  EVALUATE(KXBR);
  EVALUATE(CXBR);
  EVALUATE(AXBR);
  EVALUATE(SXBR);
  EVALUATE(MXBR);
  EVALUATE(DXBR);
  EVALUATE(TBEDR);
  EVALUATE(TBDR);
  EVALUATE(DIEBR);
  EVALUATE(FIEBRA);
  EVALUATE(THDER);
  EVALUATE(THDR);
  EVALUATE(DIDBR);
  EVALUATE(FIDBRA);
  EVALUATE(LXR);
  EVALUATE(LPDFR);
  EVALUATE(LNDFR);
  EVALUATE(LCDFR);
  EVALUATE(LZER);
  EVALUATE(LZDR);
  EVALUATE(LZXR);
  EVALUATE(SFPC);
  EVALUATE(SFASR);
  EVALUATE(EFPC);
  EVALUATE(CELFBR);
  EVALUATE(CDLFBR);
  EVALUATE(CXLFBR);
  EVALUATE(CEFBRA);
  EVALUATE(CDFBRA);
  EVALUATE(CXFBRA);
  EVALUATE(CFEBRA);
  EVALUATE(CFDBRA);
  EVALUATE(CFXBRA);
  EVALUATE(CLFEBR);
  EVALUATE(CLFDBR);
  EVALUATE(CLFXBR);
  EVALUATE(CELGBR);
  EVALUATE(CDLGBR);
  EVALUATE(CXLGBR);
  EVALUATE(CEGBRA);
  EVALUATE(CDGBRA);
  EVALUATE(CXGBRA);
  EVALUATE(CGEBRA);
  EVALUATE(CGDBRA);
  EVALUATE(CGXBRA);
  EVALUATE(CLGEBR);
  EVALUATE(CLGDBR);
  EVALUATE(CFER);
  EVALUATE(CFDR);
  EVALUATE(CFXR);
  EVALUATE(LDGR);
  EVALUATE(CGER);
  EVALUATE(CGDR);
  EVALUATE(CGXR);
  EVALUATE(LGDR);
  EVALUATE(MDTR);
  EVALUATE(MDTRA);
  EVALUATE(DDTRA);
  EVALUATE(ADTRA);
  EVALUATE(SDTRA);
  EVALUATE(LDETR);
  EVALUATE(LEDTR);
  EVALUATE(LTDTR);
  EVALUATE(FIDTR);
  EVALUATE(MXTRA);
  EVALUATE(DXTRA);
  EVALUATE(AXTRA);
  EVALUATE(SXTRA);
  EVALUATE(LXDTR);
  EVALUATE(LDXTR);
  EVALUATE(LTXTR);
  EVALUATE(FIXTR);
  EVALUATE(KDTR);
  EVALUATE(CGDTRA);
  EVALUATE(CUDTR);
  EVALUATE(CDTR);
  EVALUATE(EEDTR);
  EVALUATE(ESDTR);
  EVALUATE(KXTR);
  EVALUATE(CGXTRA);
  EVALUATE(CUXTR);
  EVALUATE(CSXTR);
  EVALUATE(CXTR);
  EVALUATE(EEXTR);
  EVALUATE(ESXTR);
  EVALUATE(CDGTRA);
  EVALUATE(CDUTR);
  EVALUATE(CDSTR);
  EVALUATE(CEDTR);
  EVALUATE(QADTR);
  EVALUATE(IEDTR);
  EVALUATE(RRDTR);
  EVALUATE(CXGTRA);
  EVALUATE(CXUTR);
  EVALUATE(CXSTR);
  EVALUATE(CEXTR);
  EVALUATE(QAXTR);
  EVALUATE(IEXTR);
  EVALUATE(RRXTR);
  EVALUATE(LPGR);
  EVALUATE(LNGR);
  EVALUATE(LTGR);
  EVALUATE(LCGR);
  EVALUATE(LGR);
  EVALUATE(LGBR);
  EVALUATE(LGHR);
  EVALUATE(AGR);
  EVALUATE(SGR);
  EVALUATE(ALGR);
  EVALUATE(SLGR);
  EVALUATE(MSGR);
  EVALUATE(MSGRKC);
  EVALUATE(DSGR);
  EVALUATE(LRVGR);
  EVALUATE(LPGFR);
  EVALUATE(LNGFR);
  EVALUATE(LTGFR);
  EVALUATE(LCGFR);
  EVALUATE(LGFR);
  EVALUATE(LLGFR);
  EVALUATE(LLGTR);
  EVALUATE(AGFR);
  EVALUATE(SGFR);
  EVALUATE(ALGFR);
  EVALUATE(SLGFR);
  EVALUATE(MSGFR);
  EVALUATE(DSGFR);
  EVALUATE(KMAC);
  EVALUATE(LRVR);
  EVALUATE(CGR);
  EVALUATE(CLGR);
  EVALUATE(LBR);
  EVALUATE(LHR);
  EVALUATE(KMF);
  EVALUATE(KMO);
  EVALUATE(PCC);
  EVALUATE(KMCTR);
  EVALUATE(KM);
  EVALUATE(KMC);
  EVALUATE(CGFR);
  EVALUATE(KIMD);
  EVALUATE(KLMD);
  EVALUATE(CFDTR);
  EVALUATE(CLGDTR);
  EVALUATE(CLFDTR);
  EVALUATE(BCTGR);
  EVALUATE(CFXTR);
  EVALUATE(CLFXTR);
  EVALUATE(CDFTR);
  EVALUATE(CDLGTR);
  EVALUATE(CDLFTR);
  EVALUATE(CXFTR);
  EVALUATE(CXLGTR);
  EVALUATE(CXLFTR);
  EVALUATE(CGRT);
  EVALUATE(NGR);
  EVALUATE(OGR);
  EVALUATE(XGR);
  EVALUATE(FLOGR);
  EVALUATE(LLGCR);
  EVALUATE(LLGHR);
  EVALUATE(MLGR);
  EVALUATE(DLGR);
  EVALUATE(ALCGR);
  EVALUATE(SLBGR);
  EVALUATE(EPSW);
  EVALUATE(TRTT);
  EVALUATE(TRTO);
  EVALUATE(TROT);
  EVALUATE(TROO);
  EVALUATE(LLCR);
  EVALUATE(LLHR);
  EVALUATE(MLR);
  EVALUATE(DLR);
  EVALUATE(ALCR);
  EVALUATE(SLBR);
  EVALUATE(CU14);
  EVALUATE(CU24);
  EVALUATE(CU41);
  EVALUATE(CU42);
  EVALUATE(TRTRE);
  EVALUATE(SRSTU);
  EVALUATE(TRTE);
  EVALUATE(AHHHR);
  EVALUATE(SHHHR);
  EVALUATE(ALHHHR);
  EVALUATE(SLHHHR);
  EVALUATE(CHHR);
  EVALUATE(AHHLR);
  EVALUATE(SHHLR);
  EVALUATE(ALHHLR);
  EVALUATE(SLHHLR);
  EVALUATE(CHLR);
  EVALUATE(POPCNT_Z);
  EVALUATE(LOCGR);
  EVALUATE(NGRK);
  EVALUATE(OGRK);
  EVALUATE(XGRK);
  EVALUATE(AGRK);
  EVALUATE(SGRK);
  EVALUATE(ALGRK);
  EVALUATE(SLGRK);
  EVALUATE(LOCR);
  EVALUATE(NRK);
  EVALUATE(ORK);
  EVALUATE(XRK);
  EVALUATE(ARK);
  EVALUATE(SRK);
  EVALUATE(ALRK);
  EVALUATE(SLRK);
  EVALUATE(LTG);
  EVALUATE(LG);
  EVALUATE(CVBY);
  EVALUATE(AG);
  EVALUATE(SG);
  EVALUATE(ALG);
  EVALUATE(SLG);
  EVALUATE(MSG);
  EVALUATE(DSG);
  EVALUATE(CVBG);
  EVALUATE(LRVG);
  EVALUATE(LT);
  EVALUATE(LGF);
  EVALUATE(LGH);
  EVALUATE(LLGF);
  EVALUATE(LLGT);
  EVALUATE(AGF);
  EVALUATE(SGF);
  EVALUATE(ALGF);
  EVALUATE(SLGF);
  EVALUATE(MSGF);
  EVALUATE(DSGF);
  EVALUATE(LRV);
  EVALUATE(LRVH);
  EVALUATE(CG);
  EVALUATE(CLG);
  EVALUATE(STG);
  EVALUATE(NTSTG);
  EVALUATE(CVDY);
  EVALUATE(CVDG);
  EVALUATE(STRVG);
  EVALUATE(CGF);
  EVALUATE(CLGF);
  EVALUATE(LTGF);
  EVALUATE(CGH);
  EVALUATE(PFD);
  EVALUATE(STRV);
  EVALUATE(STRVH);
  EVALUATE(BCTG);
  EVALUATE(STY);
  EVALUATE(MSY);
  EVALUATE(MSC);
  EVALUATE(NY);
  EVALUATE(CLY);
  EVALUATE(OY);
  EVALUATE(XY);
  EVALUATE(LY);
  EVALUATE(CY);
  EVALUATE(AY);
  EVALUATE(SY);
  EVALUATE(MFY);
  EVALUATE(ALY);
  EVALUATE(SLY);
  EVALUATE(STHY);
  EVALUATE(LAY);
  EVALUATE(STCY);
  EVALUATE(ICY);
  EVALUATE(LAEY);
  EVALUATE(LB);
  EVALUATE(LGB);
  EVALUATE(LHY);
  EVALUATE(CHY);
  EVALUATE(AHY);
  EVALUATE(SHY);
  EVALUATE(MHY);
  EVALUATE(NG);
  EVALUATE(OG);
  EVALUATE(XG);
  EVALUATE(LGAT);
  EVALUATE(MLG);
  EVALUATE(DLG);
  EVALUATE(ALCG);
  EVALUATE(SLBG);
  EVALUATE(STPQ);
  EVALUATE(LPQ);
  EVALUATE(LLGC);
  EVALUATE(LLGH);
  EVALUATE(LLC);
  EVALUATE(LLH);
  EVALUATE(ML);
  EVALUATE(DL);
  EVALUATE(ALC);
  EVALUATE(SLB);
  EVALUATE(LLGTAT);
  EVALUATE(LLGFAT);
  EVALUATE(LAT);
  EVALUATE(LBH);
  EVALUATE(LLCH);
  EVALUATE(STCH);
  EVALUATE(LHH);
  EVALUATE(LLHH);
  EVALUATE(STHH);
  EVALUATE(LFHAT);
  EVALUATE(LFH);
  EVALUATE(STFH);
  EVALUATE(CHF);
  EVALUATE(MVCDK);
  EVALUATE(MVHHI);
  EVALUATE(MVGHI);
  EVALUATE(MVHI);
  EVALUATE(CHHSI);
  EVALUATE(CGHSI);
  EVALUATE(CHSI);
  EVALUATE(CLFHSI);
  EVALUATE(TBEGIN);
  EVALUATE(TBEGINC);
  EVALUATE(LMG);
  EVALUATE(SRAG);
  EVALUATE(SLAG);
  EVALUATE(SRLG);
  EVALUATE(SLLG);
  EVALUATE(CSY);
  EVALUATE(CSG);
  EVALUATE(RLLG);
  EVALUATE(RLL);
  EVALUATE(STMG);
  EVALUATE(STMH);
  EVALUATE(STCMH);
  EVALUATE(STCMY);
  EVALUATE(CDSY);
  EVALUATE(CDSG);
  EVALUATE(BXHG);
  EVALUATE(BXLEG);
  EVALUATE(ECAG);
  EVALUATE(TMY);
  EVALUATE(MVIY);
  EVALUATE(NIY);
  EVALUATE(CLIY);
  EVALUATE(OIY);
  EVALUATE(XIY);
  EVALUATE(ASI);
  EVALUATE(ALSI);
  EVALUATE(AGSI);
  EVALUATE(ALGSI);
  EVALUATE(ICMH);
  EVALUATE(ICMY);
  EVALUATE(MVCLU);
  EVALUATE(CLCLU);
  EVALUATE(STMY);
  EVALUATE(LMH);
  EVALUATE(LMY);
  EVALUATE(TP);
  EVALUATE(SRAK);
  EVALUATE(SLAK);
  EVALUATE(SRLK);
  EVALUATE(SLLK);
  EVALUATE(LOCG);
  EVALUATE(STOCG);
  EVALUATE(LANG);
  EVALUATE(LAOG);
  EVALUATE(LAXG);
  EVALUATE(LAAG);
  EVALUATE(LAALG);
  EVALUATE(LOC);
  EVALUATE(STOC);
  EVALUATE(LAN);
  EVALUATE(LAO);
  EVALUATE(LAX);
  EVALUATE(LAA);
  EVALUATE(LAAL);
  EVALUATE(BRXHG);
  EVALUATE(BRXLG);
  EVALUATE(RISBLG);
  EVALUATE(RNSBG);
  EVALUATE(RISBG);
  EVALUATE(ROSBG);
  EVALUATE(RXSBG);
  EVALUATE(RISBGN);
  EVALUATE(RISBHG);
  EVALUATE(CGRJ);
  EVALUATE(CGIT);
  EVALUATE(CIT);
  EVALUATE(CLFIT);
  EVALUATE(CGIJ);
  EVALUATE(CIJ);
  EVALUATE(AHIK);
  EVALUATE(AGHIK);
  EVALUATE(ALHSIK);
  EVALUATE(ALGHSIK);
  EVALUATE(CGRB);
  EVALUATE(CGIB);
  EVALUATE(CIB);
  EVALUATE(LDEB);
  EVALUATE(LXDB);
  EVALUATE(LXEB);
  EVALUATE(MXDB);
  EVALUATE(KEB);
  EVALUATE(CEB);
  EVALUATE(AEB);
  EVALUATE(SEB);
  EVALUATE(MDEB);
  EVALUATE(DEB);
  EVALUATE(MAEB);
  EVALUATE(MSEB);
  EVALUATE(TCEB);
  EVALUATE(TCDB);
  EVALUATE(TCXB);
  EVALUATE(SQEB);
  EVALUATE(SQDB);
  EVALUATE(MEEB);
  EVALUATE(KDB);
  EVALUATE(CDB);
  EVALUATE(ADB);
  EVALUATE(SDB);
  EVALUATE(MDB);
  EVALUATE(DDB);
  EVALUATE(MADB);
  EVALUATE(MSDB);
  EVALUATE(SLDT);
  EVALUATE(SRDT);
  EVALUATE(SLXT);
  EVALUATE(SRXT);
  EVALUATE(TDCET);
  EVALUATE(TDGET);
  EVALUATE(TDCDT);
  EVALUATE(TDGDT);
  EVALUATE(TDCXT);
  EVALUATE(TDGXT);
  EVALUATE(LEY);
  EVALUATE(LDY);
  EVALUATE(STEY);
  EVALUATE(STDY);
  EVALUATE(CZDT);
  EVALUATE(CZXT);
  EVALUATE(CDZT);
  EVALUATE(CXZT);
#undef EVALUATE
};

// When running with the simulator transition into simulated execution at this
// point.
#define CALL_GENERATED_CODE(isolate, entry, p0, p1, p2, p3, p4)          \
  reinterpret_cast<Object*>(Simulator::current(isolate)->Call(           \
      FUNCTION_ADDR(entry), 5, (intptr_t)p0, (intptr_t)p1, (intptr_t)p2, \
      (intptr_t)p3, (intptr_t)p4))

#define CALL_GENERATED_REGEXP_CODE(isolate, entry, p0, p1, p2, p3, p4, p5, p6, \
                                   p7, p8)                                     \
  Simulator::current(isolate)->Call(                                           \
      entry, 9, (intptr_t)p0, (intptr_t)p1, (intptr_t)p2, (intptr_t)p3,        \
      (intptr_t)p4, (intptr_t)p5, (intptr_t)p6, (intptr_t)p7, (intptr_t)p8)

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
#endif  // V8_S390_SIMULATOR_S390_H_
