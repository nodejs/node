// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_SIMULATOR_ARM64_H_
#define V8_ARM64_SIMULATOR_ARM64_H_

#include <stdarg.h>
#include <vector>

#include "src/allocation.h"
#include "src/arm64/assembler-arm64.h"
#include "src/arm64/decoder-arm64.h"
#include "src/arm64/disasm-arm64.h"
#include "src/arm64/instrument-arm64.h"
#include "src/assembler.h"
#include "src/globals.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

#if !defined(USE_SIMULATOR)

// Running without a simulator on a native ARM64 platform.
// When running without a simulator we call the entry directly.
#define CALL_GENERATED_CODE(isolate, entry, p0, p1, p2, p3, p4) \
  (entry(p0, p1, p2, p3, p4))

typedef int (*arm64_regexp_matcher)(String* input,
                                    int64_t start_offset,
                                    const byte* input_start,
                                    const byte* input_end,
                                    int* output,
                                    int64_t output_size,
                                    Address stack_base,
                                    int64_t direct_call,
                                    void* return_address,
                                    Isolate* isolate);

// Call the generated regexp code directly. The code at the entry address
// should act as a function matching the type arm64_regexp_matcher.
// The ninth argument is a dummy that reserves the space used for
// the return address added by the ExitFrame in native calls.
#define CALL_GENERATED_REGEXP_CODE(isolate, entry, p0, p1, p2, p3, p4, p5, p6, \
                                   p7, p8)                                     \
  (FUNCTION_CAST<arm64_regexp_matcher>(entry)(p0, p1, p2, p3, p4, p5, p6, p7,  \
                                              NULL, p8))

// Running without a simulator there is nothing to do.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static uintptr_t JsLimitFromCLimit(v8::internal::Isolate* isolate,
                                     uintptr_t c_limit) {
    USE(isolate);
    return c_limit;
  }

  static uintptr_t RegisterCTryCatch(v8::internal::Isolate* isolate,
                                     uintptr_t try_catch_address) {
    USE(isolate);
    return try_catch_address;
  }

  static void UnregisterCTryCatch(v8::internal::Isolate* isolate) {
    USE(isolate);
  }
};

#else  // !defined(USE_SIMULATOR)


// The proper way to initialize a simulated system register (such as NZCV) is as
// follows:
//  SimSystemRegister nzcv = SimSystemRegister::DefaultValueFor(NZCV);
class SimSystemRegister {
 public:
  // The default constructor represents a register which has no writable bits.
  // It is not possible to set its value to anything other than 0.
  SimSystemRegister() : value_(0), write_ignore_mask_(0xffffffff) { }

  uint32_t RawValue() const {
    return value_;
  }

  void SetRawValue(uint32_t new_value) {
    value_ = (value_ & write_ignore_mask_) | (new_value & ~write_ignore_mask_);
  }

  uint32_t Bits(int msb, int lsb) const {
    return unsigned_bitextract_32(msb, lsb, value_);
  }

  int32_t SignedBits(int msb, int lsb) const {
    return signed_bitextract_32(msb, lsb, value_);
  }

  void SetBits(int msb, int lsb, uint32_t bits);

  // Default system register values.
  static SimSystemRegister DefaultValueFor(SystemRegister id);

#define DEFINE_GETTER(Name, HighBit, LowBit, Func, Type)                       \
  Type Name() const { return static_cast<Type>(Func(HighBit, LowBit)); }       \
  void Set##Name(Type bits) {                                                  \
    SetBits(HighBit, LowBit, static_cast<Type>(bits));                         \
  }
#define DEFINE_WRITE_IGNORE_MASK(Name, Mask)                                   \
  static const uint32_t Name##WriteIgnoreMask = ~static_cast<uint32_t>(Mask);
  SYSTEM_REGISTER_FIELDS_LIST(DEFINE_GETTER, DEFINE_WRITE_IGNORE_MASK)
#undef DEFINE_ZERO_BITS
#undef DEFINE_GETTER

 protected:
  // Most system registers only implement a few of the bits in the word. Other
  // bits are "read-as-zero, write-ignored". The write_ignore_mask argument
  // describes the bits which are not modifiable.
  SimSystemRegister(uint32_t value, uint32_t write_ignore_mask)
      : value_(value), write_ignore_mask_(write_ignore_mask) { }

  uint32_t value_;
  uint32_t write_ignore_mask_;
};


// Represent a register (r0-r31, v0-v31).
class SimRegisterBase {
 public:
  template<typename T>
  void Set(T new_value) {
    value_ = 0;
    memcpy(&value_, &new_value, sizeof(T));
  }

  template<typename T>
  T Get() const {
    T result;
    memcpy(&result, &value_, sizeof(T));
    return result;
  }

 protected:
  int64_t value_;
};


typedef SimRegisterBase SimRegister;      // r0-r31
typedef SimRegisterBase SimFPRegister;    // v0-v31


class Simulator : public DecoderVisitor {
 public:
  static void FlushICache(v8::internal::HashMap* i_cache, void* start,
                          size_t size) {
    USE(i_cache);
    USE(start);
    USE(size);
  }

  explicit Simulator(Decoder<DispatchingDecoderVisitor>* decoder,
                     Isolate* isolate = NULL,
                     FILE* stream = stderr);
  Simulator();
  ~Simulator();

  // System functions.

  static void Initialize(Isolate* isolate);

  static void TearDown(HashMap* i_cache, Redirection* first);

  static Simulator* current(v8::internal::Isolate* isolate);

  class CallArgument;

  // Call an arbitrary function taking an arbitrary number of arguments. The
  // varargs list must be a set of arguments with type CallArgument, and
  // terminated by CallArgument::End().
  void CallVoid(byte* entry, CallArgument* args);

  // Like CallVoid, but expect a return value.
  int64_t CallInt64(byte* entry, CallArgument* args);
  double CallDouble(byte* entry, CallArgument* args);

  // V8 calls into generated JS code with 5 parameters and into
  // generated RegExp code with 10 parameters. These are convenience functions,
  // which set up the simulator state and grab the result on return.
  int64_t CallJS(byte* entry,
                 Object* new_target,
                 Object* target,
                 Object* revc,
                 int64_t argc,
                 Object*** argv);
  int64_t CallRegExp(byte* entry,
                     String* input,
                     int64_t start_offset,
                     const byte* input_start,
                     const byte* input_end,
                     int* output,
                     int64_t output_size,
                     Address stack_base,
                     int64_t direct_call,
                     void* return_address,
                     Isolate* isolate);

  // A wrapper class that stores an argument for one of the above Call
  // functions.
  //
  // Only arguments up to 64 bits in size are supported.
  class CallArgument {
   public:
    template<typename T>
    explicit CallArgument(T argument) {
      bits_ = 0;
      DCHECK(sizeof(argument) <= sizeof(bits_));
      memcpy(&bits_, &argument, sizeof(argument));
      type_ = X_ARG;
    }

    explicit CallArgument(double argument) {
      DCHECK(sizeof(argument) == sizeof(bits_));
      memcpy(&bits_, &argument, sizeof(argument));
      type_ = D_ARG;
    }

    explicit CallArgument(float argument) {
      // TODO(all): CallArgument(float) is untested, remove this check once
      //            tested.
      UNIMPLEMENTED();
      // Make the D register a NaN to try to trap errors if the callee expects a
      // double. If it expects a float, the callee should ignore the top word.
      DCHECK(sizeof(kFP64SignallingNaN) == sizeof(bits_));
      memcpy(&bits_, &kFP64SignallingNaN, sizeof(kFP64SignallingNaN));
      // Write the float payload to the S register.
      DCHECK(sizeof(argument) <= sizeof(bits_));
      memcpy(&bits_, &argument, sizeof(argument));
      type_ = D_ARG;
    }

    // This indicates the end of the arguments list, so that CallArgument
    // objects can be passed into varargs functions.
    static CallArgument End() { return CallArgument(); }

    int64_t bits() const { return bits_; }
    bool IsEnd() const { return type_ == NO_ARG; }
    bool IsX() const { return type_ == X_ARG; }
    bool IsD() const { return type_ == D_ARG; }

   private:
    enum CallArgumentType { X_ARG, D_ARG, NO_ARG };

    // All arguments are aligned to at least 64 bits and we don't support
    // passing bigger arguments, so the payload size can be fixed at 64 bits.
    int64_t bits_;
    CallArgumentType type_;

    CallArgument() { type_ = NO_ARG; }
  };


  // Start the debugging command line.
  void Debug();

  bool GetValue(const char* desc, int64_t* value);

  bool PrintValue(const char* desc);

  // Push an address onto the JS stack.
  uintptr_t PushAddress(uintptr_t address);

  // Pop an address from the JS stack.
  uintptr_t PopAddress();

  // Accessor to the internal simulator stack area.
  uintptr_t StackLimit(uintptr_t c_limit) const;

  void ResetState();

  // Runtime call support.
  static void* RedirectExternalReference(Isolate* isolate,
                                         void* external_function,
                                         ExternalReference::Type type);
  void DoRuntimeCall(Instruction* instr);

  // Run the simulator.
  static const Instruction* kEndOfSimAddress;
  void DecodeInstruction();
  void Run();
  void RunFrom(Instruction* start);

  // Simulation helpers.
  template <typename T>
  void set_pc(T new_pc) {
    DCHECK(sizeof(T) == sizeof(pc_));
    memcpy(&pc_, &new_pc, sizeof(T));
    pc_modified_ = true;
  }
  Instruction* pc() { return pc_; }

  void increment_pc() {
    if (!pc_modified_) {
      pc_ = pc_->following();
    }

    pc_modified_ = false;
  }

  virtual void Decode(Instruction* instr) {
    decoder_->Decode(instr);
  }

  void ExecuteInstruction() {
    DCHECK(IsAligned(reinterpret_cast<uintptr_t>(pc_), kInstructionSize));
    CheckBreakNext();
    Decode(pc_);
    increment_pc();
    CheckBreakpoints();
  }

  // Declare all Visitor functions.
  #define DECLARE(A)  void Visit##A(Instruction* instr);
  VISITOR_LIST(DECLARE)
  #undef DECLARE

  bool IsZeroRegister(unsigned code, Reg31Mode r31mode) const {
    return ((code == 31) && (r31mode == Reg31IsZeroRegister));
  }

  // Register accessors.
  // Return 'size' bits of the value of an integer register, as the specified
  // type. The value is zero-extended to fill the result.
  //
  template<typename T>
  T reg(unsigned code, Reg31Mode r31mode = Reg31IsZeroRegister) const {
    DCHECK(code < kNumberOfRegisters);
    if (IsZeroRegister(code, r31mode)) {
      return 0;
    }
    return registers_[code].Get<T>();
  }

  // Common specialized accessors for the reg() template.
  int32_t wreg(unsigned code, Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<int32_t>(code, r31mode);
  }

  int64_t xreg(unsigned code, Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<int64_t>(code, r31mode);
  }

  // Write 'value' into an integer register. The value is zero-extended. This
  // behaviour matches AArch64 register writes.
  template<typename T>
  void set_reg(unsigned code, T value,
               Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg_no_log(code, value, r31mode);
    LogRegister(code, r31mode);
  }

  // Common specialized accessors for the set_reg() template.
  void set_wreg(unsigned code, int32_t value,
                Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg(code, value, r31mode);
  }

  void set_xreg(unsigned code, int64_t value,
                Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg(code, value, r31mode);
  }

  // As above, but don't automatically log the register update.
  template <typename T>
  void set_reg_no_log(unsigned code, T value,
                      Reg31Mode r31mode = Reg31IsZeroRegister) {
    DCHECK(code < kNumberOfRegisters);
    if (!IsZeroRegister(code, r31mode)) {
      registers_[code].Set(value);
    }
  }

  void set_wreg_no_log(unsigned code, int32_t value,
                       Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg_no_log(code, value, r31mode);
  }

  void set_xreg_no_log(unsigned code, int64_t value,
                       Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg_no_log(code, value, r31mode);
  }

  // Commonly-used special cases.
  template<typename T>
  void set_lr(T value) {
    DCHECK(sizeof(T) == kPointerSize);
    set_reg(kLinkRegCode, value);
  }

  template<typename T>
  void set_sp(T value) {
    DCHECK(sizeof(T) == kPointerSize);
    set_reg(31, value, Reg31IsStackPointer);
  }

  int64_t sp() { return xreg(31, Reg31IsStackPointer); }
  int64_t jssp() { return xreg(kJSSPCode, Reg31IsStackPointer); }
  int64_t fp() {
      return xreg(kFramePointerRegCode, Reg31IsStackPointer);
  }
  Instruction* lr() { return reg<Instruction*>(kLinkRegCode); }

  Address get_sp() const { return reg<Address>(31, Reg31IsStackPointer); }

  template<typename T>
  T fpreg(unsigned code) const {
    DCHECK(code < kNumberOfRegisters);
    return fpregisters_[code].Get<T>();
  }

  // Common specialized accessors for the fpreg() template.
  float sreg(unsigned code) const {
    return fpreg<float>(code);
  }

  uint32_t sreg_bits(unsigned code) const {
    return fpreg<uint32_t>(code);
  }

  double dreg(unsigned code) const {
    return fpreg<double>(code);
  }

  uint64_t dreg_bits(unsigned code) const {
    return fpreg<uint64_t>(code);
  }

  double fpreg(unsigned size, unsigned code) const {
    switch (size) {
      case kSRegSizeInBits: return sreg(code);
      case kDRegSizeInBits: return dreg(code);
      default:
        UNREACHABLE();
        return 0.0;
    }
  }

  // Write 'value' into a floating-point register. The value is zero-extended.
  // This behaviour matches AArch64 register writes.
  template<typename T>
  void set_fpreg(unsigned code, T value) {
    set_fpreg_no_log(code, value);

    if (sizeof(value) <= kSRegSize) {
      LogFPRegister(code, kPrintSRegValue);
    } else {
      LogFPRegister(code, kPrintDRegValue);
    }
  }

  // Common specialized accessors for the set_fpreg() template.
  void set_sreg(unsigned code, float value) {
    set_fpreg(code, value);
  }

  void set_sreg_bits(unsigned code, uint32_t value) {
    set_fpreg(code, value);
  }

  void set_dreg(unsigned code, double value) {
    set_fpreg(code, value);
  }

  void set_dreg_bits(unsigned code, uint64_t value) {
    set_fpreg(code, value);
  }

  // As above, but don't automatically log the register update.
  template <typename T>
  void set_fpreg_no_log(unsigned code, T value) {
    DCHECK((sizeof(value) == kDRegSize) || (sizeof(value) == kSRegSize));
    DCHECK(code < kNumberOfFPRegisters);
    fpregisters_[code].Set(value);
  }

  void set_sreg_no_log(unsigned code, float value) {
    set_fpreg_no_log(code, value);
  }

  void set_dreg_no_log(unsigned code, double value) {
    set_fpreg_no_log(code, value);
  }

  SimSystemRegister& nzcv() { return nzcv_; }
  SimSystemRegister& fpcr() { return fpcr_; }

  // Debug helpers

  // Simulator breakpoints.
  struct Breakpoint {
    Instruction* location;
    bool enabled;
  };
  std::vector<Breakpoint> breakpoints_;
  void SetBreakpoint(Instruction* breakpoint);
  void ListBreakpoints();
  void CheckBreakpoints();

  // Helpers for the 'next' command.
  // When this is set, the Simulator will insert a breakpoint after the next BL
  // instruction it meets.
  bool break_on_next_;
  // Check if the Simulator should insert a break after the current instruction
  // for the 'next' command.
  void CheckBreakNext();

  // Disassemble instruction at the given address.
  void PrintInstructionsAt(Instruction* pc, uint64_t count);

  // Print all registers of the specified types.
  void PrintRegisters();
  void PrintFPRegisters();
  void PrintSystemRegisters();

  // Like Print* (above), but respect log_parameters().
  void LogSystemRegisters() {
    if (log_parameters() & LOG_SYS_REGS) PrintSystemRegisters();
  }
  void LogRegisters() {
    if (log_parameters() & LOG_REGS) PrintRegisters();
  }
  void LogFPRegisters() {
    if (log_parameters() & LOG_FP_REGS) PrintFPRegisters();
  }

  // Specify relevant register sizes, for PrintFPRegister.
  //
  // These values are bit masks; they can be combined in case multiple views of
  // a machine register are interesting.
  enum PrintFPRegisterSizes {
    kPrintDRegValue = 1 << kDRegSize,
    kPrintSRegValue = 1 << kSRegSize,
    kPrintAllFPRegValues = kPrintDRegValue | kPrintSRegValue
  };

  // Print individual register values (after update).
  void PrintRegister(unsigned code, Reg31Mode r31mode = Reg31IsStackPointer);
  void PrintFPRegister(unsigned code,
                       PrintFPRegisterSizes sizes = kPrintAllFPRegValues);
  void PrintSystemRegister(SystemRegister id);

  // Like Print* (above), but respect log_parameters().
  void LogRegister(unsigned code, Reg31Mode r31mode = Reg31IsStackPointer) {
    if (log_parameters() & LOG_REGS) PrintRegister(code, r31mode);
  }
  void LogFPRegister(unsigned code,
                     PrintFPRegisterSizes sizes = kPrintAllFPRegValues) {
    if (log_parameters() & LOG_FP_REGS) PrintFPRegister(code, sizes);
  }
  void LogSystemRegister(SystemRegister id) {
    if (log_parameters() & LOG_SYS_REGS) PrintSystemRegister(id);
  }

  // Print memory accesses.
  void PrintRead(uintptr_t address, size_t size, unsigned reg_code);
  void PrintReadFP(uintptr_t address, size_t size, unsigned reg_code);
  void PrintWrite(uintptr_t address, size_t size, unsigned reg_code);
  void PrintWriteFP(uintptr_t address, size_t size, unsigned reg_code);

  // Like Print* (above), but respect log_parameters().
  void LogRead(uintptr_t address, size_t size, unsigned reg_code) {
    if (log_parameters() & LOG_REGS) PrintRead(address, size, reg_code);
  }
  void LogReadFP(uintptr_t address, size_t size, unsigned reg_code) {
    if (log_parameters() & LOG_FP_REGS) PrintReadFP(address, size, reg_code);
  }
  void LogWrite(uintptr_t address, size_t size, unsigned reg_code) {
    if (log_parameters() & LOG_WRITE) PrintWrite(address, size, reg_code);
  }
  void LogWriteFP(uintptr_t address, size_t size, unsigned reg_code) {
    if (log_parameters() & LOG_WRITE) PrintWriteFP(address, size, reg_code);
  }

  int log_parameters() { return log_parameters_; }
  void set_log_parameters(int new_parameters) {
    log_parameters_ = new_parameters;
    if (!decoder_) {
      if (new_parameters & LOG_DISASM) {
        PrintF("Run --debug-sim to dynamically turn on disassembler\n");
      }
      return;
    }
    if (new_parameters & LOG_DISASM) {
      decoder_->InsertVisitorBefore(print_disasm_, this);
    } else {
      decoder_->RemoveVisitor(print_disasm_);
    }
  }

  static inline const char* WRegNameForCode(unsigned code,
      Reg31Mode mode = Reg31IsZeroRegister);
  static inline const char* XRegNameForCode(unsigned code,
      Reg31Mode mode = Reg31IsZeroRegister);
  static inline const char* SRegNameForCode(unsigned code);
  static inline const char* DRegNameForCode(unsigned code);
  static inline const char* VRegNameForCode(unsigned code);
  static inline int CodeFromName(const char* name);

 protected:
  // Simulation helpers ------------------------------------
  bool ConditionPassed(Condition cond) {
    SimSystemRegister& flags = nzcv();
    switch (cond) {
      case eq:
        return flags.Z();
      case ne:
        return !flags.Z();
      case hs:
        return flags.C();
      case lo:
        return !flags.C();
      case mi:
        return flags.N();
      case pl:
        return !flags.N();
      case vs:
        return flags.V();
      case vc:
        return !flags.V();
      case hi:
        return flags.C() && !flags.Z();
      case ls:
        return !(flags.C() && !flags.Z());
      case ge:
        return flags.N() == flags.V();
      case lt:
        return flags.N() != flags.V();
      case gt:
        return !flags.Z() && (flags.N() == flags.V());
      case le:
        return !(!flags.Z() && (flags.N() == flags.V()));
      case nv:  // Fall through.
      case al:
        return true;
      default:
        UNREACHABLE();
        return false;
    }
  }

  bool ConditionFailed(Condition cond) {
    return !ConditionPassed(cond);
  }

  template<typename T>
  void AddSubHelper(Instruction* instr, T op2);
  template<typename T>
  T AddWithCarry(bool set_flags,
                 T src1,
                 T src2,
                 T carry_in = 0);
  template<typename T>
  void AddSubWithCarry(Instruction* instr);
  template<typename T>
  void LogicalHelper(Instruction* instr, T op2);
  template<typename T>
  void ConditionalCompareHelper(Instruction* instr, T op2);
  void LoadStoreHelper(Instruction* instr,
                       int64_t offset,
                       AddrMode addrmode);
  void LoadStorePairHelper(Instruction* instr, AddrMode addrmode);
  uintptr_t LoadStoreAddress(unsigned addr_reg, int64_t offset,
                             AddrMode addrmode);
  void LoadStoreWriteBack(unsigned addr_reg,
                          int64_t offset,
                          AddrMode addrmode);
  void CheckMemoryAccess(uintptr_t address, uintptr_t stack);

  // Memory read helpers.
  template <typename T, typename A>
  T MemoryRead(A address) {
    T value;
    STATIC_ASSERT((sizeof(value) == 1) || (sizeof(value) == 2) ||
                  (sizeof(value) == 4) || (sizeof(value) == 8));
    memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
  }

  // Memory write helpers.
  template <typename T, typename A>
  void MemoryWrite(A address, T value) {
    STATIC_ASSERT((sizeof(value) == 1) || (sizeof(value) == 2) ||
                  (sizeof(value) == 4) || (sizeof(value) == 8));
    memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
  }

  template <typename T>
  T ShiftOperand(T value,
                 Shift shift_type,
                 unsigned amount);
  template <typename T>
  T ExtendValue(T value,
                Extend extend_type,
                unsigned left_shift = 0);
  template <typename T>
  void Extract(Instruction* instr);
  template <typename T>
  void DataProcessing2Source(Instruction* instr);
  template <typename T>
  void BitfieldHelper(Instruction* instr);

  template <typename T>
  T FPDefaultNaN() const;

  void FPCompare(double val0, double val1);
  double FPRoundInt(double value, FPRounding round_mode);
  double FPToDouble(float value);
  float FPToFloat(double value, FPRounding round_mode);
  double FixedToDouble(int64_t src, int fbits, FPRounding round_mode);
  double UFixedToDouble(uint64_t src, int fbits, FPRounding round_mode);
  float FixedToFloat(int64_t src, int fbits, FPRounding round_mode);
  float UFixedToFloat(uint64_t src, int fbits, FPRounding round_mode);
  int32_t FPToInt32(double value, FPRounding rmode);
  int64_t FPToInt64(double value, FPRounding rmode);
  uint32_t FPToUInt32(double value, FPRounding rmode);
  uint64_t FPToUInt64(double value, FPRounding rmode);

  template <typename T>
  T FPAdd(T op1, T op2);

  template <typename T>
  T FPDiv(T op1, T op2);

  template <typename T>
  T FPMax(T a, T b);

  template <typename T>
  T FPMaxNM(T a, T b);

  template <typename T>
  T FPMin(T a, T b);

  template <typename T>
  T FPMinNM(T a, T b);

  template <typename T>
  T FPMul(T op1, T op2);

  template <typename T>
  T FPMulAdd(T a, T op1, T op2);

  template <typename T>
  T FPSqrt(T op);

  template <typename T>
  T FPSub(T op1, T op2);

  // Standard NaN processing.
  template <typename T>
  T FPProcessNaN(T op);

  bool FPProcessNaNs(Instruction* instr);

  template <typename T>
  T FPProcessNaNs(T op1, T op2);

  template <typename T>
  T FPProcessNaNs3(T op1, T op2, T op3);

  void CheckStackAlignment();

  inline void CheckPCSComplianceAndRun();

#ifdef DEBUG
  // Corruption values should have their least significant byte cleared to
  // allow the code of the register being corrupted to be inserted.
  static const uint64_t kCallerSavedRegisterCorruptionValue =
      0xca11edc0de000000UL;
  // This value is a NaN in both 32-bit and 64-bit FP.
  static const uint64_t kCallerSavedFPRegisterCorruptionValue =
      0x7ff000007f801000UL;
  // This value is a mix of 32/64-bits NaN and "verbose" immediate.
  static const uint64_t kDefaultCPURegisterCorruptionValue =
      0x7ffbad007f8bad00UL;

  void CorruptRegisters(CPURegList* list,
                        uint64_t value = kDefaultCPURegisterCorruptionValue);
  void CorruptAllCallerSavedCPURegisters();
#endif

  // Pseudo Printf instruction
  void DoPrintf(Instruction* instr);

  // Processor state ---------------------------------------

  // Output stream.
  FILE* stream_;
  PrintDisassembler* print_disasm_;
  void PRINTF_METHOD_CHECKING TraceSim(const char* format, ...);

  // Instrumentation.
  Instrument* instrument_;

  // General purpose registers. Register 31 is the stack pointer.
  SimRegister registers_[kNumberOfRegisters];

  // Floating point registers
  SimFPRegister fpregisters_[kNumberOfFPRegisters];

  // Processor state
  // bits[31, 27]: Condition flags N, Z, C, and V.
  //               (Negative, Zero, Carry, Overflow)
  SimSystemRegister nzcv_;

  // Floating-Point Control Register
  SimSystemRegister fpcr_;

  // Only a subset of FPCR features are supported by the simulator. This helper
  // checks that the FPCR settings are supported.
  //
  // This is checked when floating-point instructions are executed, not when
  // FPCR is set. This allows generated code to modify FPCR for external
  // functions, or to save and restore it when entering and leaving generated
  // code.
  void AssertSupportedFPCR() {
    DCHECK(fpcr().FZ() == 0);             // No flush-to-zero support.
    DCHECK(fpcr().RMode() == FPTieEven);  // Ties-to-even rounding only.

    // The simulator does not support half-precision operations so fpcr().AHP()
    // is irrelevant, and is not checked here.
  }

  template <typename T>
  static int CalcNFlag(T result) {
    return (result >> (sizeof(T) * 8 - 1)) & 1;
  }

  static int CalcZFlag(uint64_t result) {
    return result == 0;
  }

  static const uint32_t kConditionFlagsMask = 0xf0000000;

  // Stack
  uintptr_t stack_;
  static const size_t stack_protection_size_ = KB;
  size_t stack_size_;
  uintptr_t stack_limit_;

  Decoder<DispatchingDecoderVisitor>* decoder_;
  Decoder<DispatchingDecoderVisitor>* disassembler_decoder_;

  // Indicates if the pc has been modified by the instruction and should not be
  // automatically incremented.
  bool pc_modified_;
  Instruction* pc_;

  static const char* xreg_names[];
  static const char* wreg_names[];
  static const char* sreg_names[];
  static const char* dreg_names[];
  static const char* vreg_names[];

  // Debugger input.
  void set_last_debugger_input(char* input) {
    DeleteArray(last_debugger_input_);
    last_debugger_input_ = input;
  }
  char* last_debugger_input() { return last_debugger_input_; }
  char* last_debugger_input_;

 private:
  void Init(FILE* stream);

  int  log_parameters_;
  Isolate* isolate_;
};


// When running with the simulator transition into simulated execution at this
// point.
#define CALL_GENERATED_CODE(isolate, entry, p0, p1, p2, p3, p4)  \
  reinterpret_cast<Object*>(Simulator::current(isolate)->CallJS( \
      FUNCTION_ADDR(entry), p0, p1, p2, p3, p4))

#define CALL_GENERATED_REGEXP_CODE(isolate, entry, p0, p1, p2, p3, p4, p5, p6, \
                                   p7, p8)                                     \
  static_cast<int>(Simulator::current(isolate)->CallRegExp(                    \
      entry, p0, p1, p2, p3, p4, p5, p6, p7, NULL, p8))


// The simulator has its own stack. Thus it has a different stack limit from
// the C-based native code.  The JS-based limit normally points near the end of
// the simulator stack.  When the C-based limit is exhausted we reflect that by
// lowering the JS-based limit as well, to make stack checks trigger.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static uintptr_t JsLimitFromCLimit(v8::internal::Isolate* isolate,
                                            uintptr_t c_limit) {
    return Simulator::current(isolate)->StackLimit(c_limit);
  }

  static uintptr_t RegisterCTryCatch(v8::internal::Isolate* isolate,
                                     uintptr_t try_catch_address) {
    Simulator* sim = Simulator::current(isolate);
    return sim->PushAddress(try_catch_address);
  }

  static void UnregisterCTryCatch(v8::internal::Isolate* isolate) {
    Simulator::current(isolate)->PopAddress();
  }
};

#endif  // !defined(USE_SIMULATOR)

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_SIMULATOR_ARM64_H_
