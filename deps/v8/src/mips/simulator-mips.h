// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declares a Simulator for MIPS instructions if we are not generating a native
// MIPS binary. This Simulator allows us to run and debug MIPS code generation
// on regular desktop machines.
// V8 calls into generated code via the GeneratedCode wrapper,
// which will start execution in the Simulator or forwards to the real entry
// on a MIPS HW platform.

#ifndef V8_MIPS_SIMULATOR_MIPS_H_
#define V8_MIPS_SIMULATOR_MIPS_H_

#include "src/allocation.h"
#include "src/mips/constants-mips.h"

#if defined(USE_SIMULATOR)
// Running with a simulator.

#include "src/assembler.h"
#include "src/base/hashmap.h"
#include "src/simulator-base.h"

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

class SimInstructionBase : public InstructionBase {
 public:
  Type InstructionType() const { return type_; }
  inline Instruction* instr() const { return instr_; }
  inline int32_t operand() const { return operand_; }

 protected:
  SimInstructionBase() : operand_(-1), instr_(nullptr), type_(kUnsupported) {}
  explicit SimInstructionBase(Instruction* instr) {}

  int32_t operand_;
  Instruction* instr_;
  Type type_;

 private:
  DISALLOW_ASSIGN(SimInstructionBase);
};

class SimInstruction : public InstructionGetters<SimInstructionBase> {
 public:
  SimInstruction() {}

  explicit SimInstruction(Instruction* instr) { *this = instr; }

  SimInstruction& operator=(Instruction* instr) {
    operand_ = *reinterpret_cast<const int32_t*>(instr);
    instr_ = instr;
    type_ = InstructionBase::InstructionType();
    DCHECK(reinterpret_cast<void*>(&operand_) == this);
    return *this;
  }
};

class Simulator : public SimulatorBase {
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

  // MSA registers
  enum MSARegister {
    w0,
    w1,
    w2,
    w3,
    w4,
    w5,
    w6,
    w7,
    w8,
    w9,
    w10,
    w11,
    w12,
    w13,
    w14,
    w15,
    w16,
    w17,
    w18,
    w19,
    w20,
    w21,
    w22,
    w23,
    w24,
    w25,
    w26,
    w27,
    w28,
    w29,
    w30,
    w31,
    kNumMSARegisters
  };

  explicit Simulator(Isolate* isolate);
  ~Simulator();

  // The currently executing Simulator instance. Potentially there can be one
  // for each native thread.
  V8_EXPORT_PRIVATE static Simulator* current(v8::internal::Isolate* isolate);

  // Accessors for register state. Reading the pc value adheres to the MIPS
  // architecture specification and is off by a 8 from the currently executing
  // instruction.
  void set_register(int reg, int32_t value);
  void set_dw_register(int dreg, const int* dbl);
  int32_t get_register(int reg) const;
  double get_double_from_register_pair(int reg);
  // Same for FPURegisters.
  void set_fpu_register(int fpureg, int64_t value);
  void set_fpu_register_word(int fpureg, int32_t value);
  void set_fpu_register_hi_word(int fpureg, int32_t value);
  void set_fpu_register_float(int fpureg, float value);
  void set_fpu_register_double(int fpureg, double value);
  void set_fpu_register_invalid_result64(float original, float rounded);
  void set_fpu_register_invalid_result(float original, float rounded);
  void set_fpu_register_word_invalid_result(float original, float rounded);
  void set_fpu_register_invalid_result64(double original, double rounded);
  void set_fpu_register_invalid_result(double original, double rounded);
  void set_fpu_register_word_invalid_result(double original, double rounded);
  int64_t get_fpu_register(int fpureg) const;
  int32_t get_fpu_register_word(int fpureg) const;
  int32_t get_fpu_register_signed_word(int fpureg) const;
  int32_t get_fpu_register_hi_word(int fpureg) const;
  float get_fpu_register_float(int fpureg) const;
  double get_fpu_register_double(int fpureg) const;
  template <typename T>
  void get_msa_register(int wreg, T* value);
  template <typename T>
  void set_msa_register(int wreg, const T* value);
  void set_fcsr_bit(uint32_t cc, bool value);
  bool test_fcsr_bit(uint32_t cc);
  void set_fcsr_rounding_mode(FPURoundingMode mode);
  void set_msacsr_rounding_mode(FPURoundingMode mode);
  unsigned int get_fcsr_rounding_mode();
  unsigned int get_msacsr_rounding_mode();
  bool set_fcsr_round_error(double original, double rounded);
  bool set_fcsr_round_error(float original, float rounded);
  bool set_fcsr_round64_error(double original, double rounded);
  bool set_fcsr_round64_error(float original, float rounded);
  void round_according_to_fcsr(double toRound, double& rounded,
                               int32_t& rounded_int, double fs);
  void round_according_to_fcsr(float toRound, float& rounded,
                               int32_t& rounded_int, float fs);
  template <typename Tfp, typename Tint>
  void round_according_to_msacsr(Tfp toRound, Tfp& rounded, Tint& rounded_int);
  void round64_according_to_fcsr(double toRound, double& rounded,
                                 int64_t& rounded_int, double fs);
  void round64_according_to_fcsr(float toRound, float& rounded,
                                 int64_t& rounded_int, float fs);
  // Special case of set_register and get_register to access the raw PC value.
  void set_pc(int32_t value);
  int32_t get_pc() const;

  Address get_sp() const {
    return reinterpret_cast<Address>(static_cast<intptr_t>(get_register(sp)));
  }

  // Accessor to the internal simulator stack area.
  uintptr_t StackLimit(uintptr_t c_limit) const;

  // Executes MIPS instructions until the PC reaches end_sim_pc.
  void Execute();

  template <typename Return, typename... Args>
  Return Call(byte* entry, Args... args) {
    return VariadicCall<Return>(this, &Simulator::CallImpl, entry, args...);
  }

  // Alternative: call a 2-argument double function.
  double CallFP(byte* entry, double d0, double d1);

  // Push an address onto the JS stack.
  uintptr_t PushAddress(uintptr_t address);

  // Pop an address from the JS stack.
  uintptr_t PopAddress();

  // Debugger input.
  void set_last_debugger_input(char* input);
  char* last_debugger_input() { return last_debugger_input_; }

  // Redirection support.
  static void SetRedirectInstruction(Instruction* instruction);

  // ICache checking.
  static bool ICacheMatch(void* one, void* two);
  static void FlushICache(base::CustomMatcherHashMap* i_cache, void* start,
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

  V8_EXPORT_PRIVATE intptr_t CallImpl(byte* entry, int argument_count,
                                      const intptr_t* arguments);

  // Unsupported instructions use Format to print an error and stop execution.
  void Format(Instruction* instr, const char* format);

  // Helpers for data value tracing.
  enum TraceType { BYTE, HALF, WORD, DWORD, FLOAT, DOUBLE, FLOAT_DOUBLE };

  // MSA Data Format
  enum MSADataFormat { MSA_VECT = 0, MSA_BYTE, MSA_HALF, MSA_WORD, MSA_DWORD };
  typedef union {
    int8_t b[kMSALanesByte];
    uint8_t ub[kMSALanesByte];
    int16_t h[kMSALanesHalf];
    uint16_t uh[kMSALanesHalf];
    int32_t w[kMSALanesWord];
    uint32_t uw[kMSALanesWord];
    int64_t d[kMSALanesDword];
    uint64_t ud[kMSALanesDword];
  } msa_reg_t;

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

  inline int ReadW(int32_t addr, Instruction* instr, TraceType t = WORD);
  inline void WriteW(int32_t addr, int value, Instruction* instr);

  inline double ReadD(int32_t addr, Instruction* instr);
  inline void WriteD(int32_t addr, double value, Instruction* instr);

  template <typename T>
  T ReadMem(int32_t addr, Instruction* instr);

  template <typename T>
  void WriteMem(int32_t addr, T value, Instruction* instr);

  void TraceRegWr(int32_t value, TraceType t = WORD);
  void TraceRegWr(int64_t value, TraceType t = DWORD);
  template <typename T>
  void TraceMSARegWr(T* value, TraceType t);
  template <typename T>
  void TraceMSARegWr(T* value);
  void TraceMemWr(int32_t addr, int32_t value, TraceType t = WORD);
  void TraceMemRd(int32_t addr, int32_t value, TraceType t = WORD);
  void TraceMemWr(int32_t addr, int64_t value, TraceType t = DWORD);
  void TraceMemRd(int32_t addr, int64_t value, TraceType t = DWORD);
  template <typename T>
  void TraceMemRd(int32_t addr, T value);
  template <typename T>
  void TraceMemWr(int32_t addr, T value);
  EmbeddedVector<char, 128> trace_buf_;

  // Operations depending on endianness.
  // Get Double Higher / Lower word.
  inline int32_t GetDoubleHIW(double* addr);
  inline int32_t GetDoubleLOW(double* addr);
  // Set Double Higher / Lower word.
  inline int32_t SetDoubleHIW(double* addr);
  inline int32_t SetDoubleLOW(double* addr);

  SimInstruction instr_;

  // Executing is handled based on the instruction type.
  void DecodeTypeRegister();

  // Functions called from DecodeTypeRegister.
  void DecodeTypeRegisterCOP1();

  void DecodeTypeRegisterCOP1X();

  void DecodeTypeRegisterSPECIAL();

  void DecodeTypeRegisterSPECIAL2();

  void DecodeTypeRegisterSPECIAL3();

  // Called from DecodeTypeRegisterCOP1.
  void DecodeTypeRegisterSRsType();

  void DecodeTypeRegisterDRsType();

  void DecodeTypeRegisterWRsType();

  void DecodeTypeRegisterLRsType();

  int DecodeMsaDataFormat();
  void DecodeTypeMsaI8();
  void DecodeTypeMsaI5();
  void DecodeTypeMsaI10();
  void DecodeTypeMsaELM();
  void DecodeTypeMsaBIT();
  void DecodeTypeMsaMI10();
  void DecodeTypeMsa3R();
  void DecodeTypeMsa3RF();
  void DecodeTypeMsaVec();
  void DecodeTypeMsa2R();
  void DecodeTypeMsa2RF();
  template <typename T>
  T MsaI5InstrHelper(uint32_t opcode, T ws, int32_t i5);
  template <typename T>
  T MsaBitInstrHelper(uint32_t opcode, T wd, T ws, int32_t m);
  template <typename T>
  T Msa3RInstrHelper(uint32_t opcode, T wd, T ws, T wt);

  inline int32_t rs_reg() const { return instr_.RsValue(); }
  inline int32_t rs() const { return get_register(rs_reg()); }
  inline uint32_t rs_u() const {
    return static_cast<uint32_t>(get_register(rs_reg()));
  }
  inline int32_t rt_reg() const { return instr_.RtValue(); }
  inline int32_t rt() const { return get_register(rt_reg()); }
  inline uint32_t rt_u() const {
    return static_cast<uint32_t>(get_register(rt_reg()));
  }
  inline int32_t rd_reg() const { return instr_.RdValue(); }
  inline int32_t fr_reg() const { return instr_.FrValue(); }
  inline int32_t fs_reg() const { return instr_.FsValue(); }
  inline int32_t ft_reg() const { return instr_.FtValue(); }
  inline int32_t fd_reg() const { return instr_.FdValue(); }
  inline int32_t sa() const { return instr_.SaValue(); }
  inline int32_t lsa_sa() const { return instr_.LsaSaValue(); }
  inline int32_t ws_reg() const { return instr_.WsValue(); }
  inline int32_t wt_reg() const { return instr_.WtValue(); }
  inline int32_t wd_reg() const { return instr_.WdValue(); }

  inline void SetResult(int32_t rd_reg, int32_t alu_out) {
    set_register(rd_reg, alu_out);
    TraceRegWr(alu_out);
  }

  inline void SetFPUWordResult(int32_t fd_reg, int32_t alu_out) {
    set_fpu_register_word(fd_reg, alu_out);
    TraceRegWr(get_fpu_register_word(fd_reg));
  }

  inline void SetFPUResult(int32_t fd_reg, int64_t alu_out) {
    set_fpu_register(fd_reg, alu_out);
    TraceRegWr(get_fpu_register(fd_reg));
  }

  inline void SetFPUFloatResult(int32_t fd_reg, float alu_out) {
    set_fpu_register_float(fd_reg, alu_out);
    TraceRegWr(get_fpu_register_word(fd_reg), FLOAT);
  }

  inline void SetFPUDoubleResult(int32_t fd_reg, double alu_out) {
    set_fpu_register_double(fd_reg, alu_out);
    TraceRegWr(get_fpu_register(fd_reg), DOUBLE);
  }

  void DecodeTypeImmediate();
  void DecodeTypeJump();

  // Used for breakpoints and traps.
  void SoftwareInterrupt();

  // Compact branch guard.
  void CheckForbiddenSlot(int32_t current_pc) {
    Instruction* instr_after_compact_branch =
        reinterpret_cast<Instruction*>(current_pc + Instruction::kInstrSize);
    if (instr_after_compact_branch->IsForbiddenAfterBranch()) {
      FATAL(
          "Error: Unexpected instruction 0x%08x immediately after a "
          "compact branch instruction.",
          *reinterpret_cast<uint32_t*>(instr_after_compact_branch));
    }
  }

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
    if (instr->InstructionBits() == nopInstr) {
      // Short-cut generic nop instructions. They are always valid and they
      // never change the simulator state.
      return;
    }

    if (instr->IsForbiddenInBranchDelay()) {
      FATAL("Eror:Unexpected %i opcode in a branch delay slot.",
            instr->OpcodeValue());
    }
    InstructionDecode(instr);
    SNPrintF(trace_buf_, " ");
  }

  // ICache.
  static void CheckICache(base::CustomMatcherHashMap* i_cache,
                          Instruction* instr);
  static void FlushOnePage(base::CustomMatcherHashMap* i_cache, intptr_t start,
                           int size);
  static CachePage* GetCachePage(base::CustomMatcherHashMap* i_cache,
                                 void* page);

  enum Exception {
    none,
    kIntegerOverflow,
    kIntegerUnderflow,
    kDivideByZero,
    kNumExceptions
  };

  // Exceptions.
  void SignalException(Exception e);

  // Handle arguments and return value for runtime FP functions.
  void GetFpArgs(double* x, double* y, int32_t* z);
  void SetFpResult(const double& result);

  void CallInternal(byte* entry);

  // Architecture state.
  // Registers.
  int32_t registers_[kNumSimuRegisters];
  // Coprocessor Registers.
  // Note: FP32 mode uses only the lower 32-bit part of each element,
  // the upper 32-bit is unpredictable.
  // Note: FPUregisters_[] array is increased to 64 * 8B = 32 * 16B in
  // order to support MSA registers
  int64_t FPUregisters_[kNumFPURegisters * 2];
  // FPU control register.
  uint32_t FCSR_;
  // MSA control register.
  uint32_t MSACSR_;

  // Simulator support.
  // Allocate 1MB for stack.
  static const size_t stack_size_ = 1 * 1024*1024;
  char* stack_;
  bool pc_modified_;
  uint64_t icount_;
  int break_count_;

  // Debugger input.
  char* last_debugger_input_;

  v8::internal::Isolate* isolate_;

  // Registered breakpoints.
  Instruction* break_pc_;
  Instr break_instr_;

  // Stop is disabled if bit 31 is set.
  static const uint32_t kStopDisabledBit = 1 << 31;

  // A stop is enabled, meaning the simulator will stop when meeting the
  // instruction, if bit 31 of watched_stops_[code].count is unset.
  // The value watched_stops_[code].count & ~(1 << 31) indicates how many times
  // the breakpoint was hit or gone through.
  struct StopCountAndDesc {
    uint32_t count;
    char* desc;
  };
  StopCountAndDesc watched_stops_[kMaxStopCode + 1];
};

}  // namespace internal
}  // namespace v8

#endif  // defined(USE_SIMULATOR)
#endif  // V8_MIPS_SIMULATOR_MIPS_H_
