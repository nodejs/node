// Copyright 2013 the V8 project authors. All rights reserved.
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

#ifndef V8_ARM64_SIMULATOR_ARM64_H_
#define V8_ARM64_SIMULATOR_ARM64_H_

#include <stdarg.h>
#include <vector>

#include "v8.h"

#include "globals.h"
#include "utils.h"
#include "allocation.h"
#include "assembler.h"
#include "arm64/assembler-arm64.h"
#include "arm64/decoder-arm64.h"
#include "arm64/disasm-arm64.h"
#include "arm64/instrument-arm64.h"

#define REGISTER_CODE_LIST(R)                                                  \
R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7)                                 \
R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15)                                \
R(16) R(17) R(18) R(19) R(20) R(21) R(22) R(23)                                \
R(24) R(25) R(26) R(27) R(28) R(29) R(30) R(31)

namespace v8 {
namespace internal {

#if !defined(USE_SIMULATOR)

// Running without a simulator on a native ARM64 platform.
// When running without a simulator we call the entry directly.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
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
#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7, p8) \
  (FUNCTION_CAST<arm64_regexp_matcher>(entry)(                                \
      p0, p1, p2, p3, p4, p5, p6, p7, NULL, p8))

#define TRY_CATCH_FROM_ADDRESS(try_catch_address) \
  reinterpret_cast<TryCatch*>(try_catch_address)

// Running without a simulator there is nothing to do.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static uintptr_t JsLimitFromCLimit(v8::internal::Isolate* isolate,
                                            uintptr_t c_limit) {
    USE(isolate);
    return c_limit;
  }

  static uintptr_t RegisterCTryCatch(uintptr_t try_catch_address) {
    return try_catch_address;
  }

  static void UnregisterCTryCatch() { }
};

#else  // !defined(USE_SIMULATOR)

enum ReverseByteMode {
  Reverse16 = 0,
  Reverse32 = 1,
  Reverse64 = 2
};


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
template<int kSizeInBytes>
class SimRegisterBase {
 public:
  template<typename T>
  void Set(T new_value, unsigned size = sizeof(T)) {
    ASSERT(size <= kSizeInBytes);
    ASSERT(size <= sizeof(new_value));
    // All AArch64 registers are zero-extending; Writing a W register clears the
    // top bits of the corresponding X register.
    memset(value_, 0, kSizeInBytes);
    memcpy(value_, &new_value, size);
  }

  // Copy 'size' bytes of the register to the result, and zero-extend to fill
  // the result.
  template<typename T>
  T Get(unsigned size = sizeof(T)) const {
    ASSERT(size <= kSizeInBytes);
    T result;
    memset(&result, 0, sizeof(result));
    memcpy(&result, value_, size);
    return result;
  }

 protected:
  uint8_t value_[kSizeInBytes];
};
typedef SimRegisterBase<kXRegSize> SimRegister;      // r0-r31
typedef SimRegisterBase<kDRegSize> SimFPRegister;    // v0-v31


class Simulator : public DecoderVisitor {
 public:
  explicit Simulator(Decoder<DispatchingDecoderVisitor>* decoder,
                     Isolate* isolate = NULL,
                     FILE* stream = stderr);
  Simulator();
  ~Simulator();

  // System functions.

  static void Initialize(Isolate* isolate);

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
                 byte* function_entry,
                 JSFunction* func,
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
      ASSERT(sizeof(argument) <= sizeof(bits_));
      memcpy(&bits_, &argument, sizeof(argument));
      type_ = X_ARG;
    }

    explicit CallArgument(double argument) {
      ASSERT(sizeof(argument) == sizeof(bits_));
      memcpy(&bits_, &argument, sizeof(argument));
      type_ = D_ARG;
    }

    explicit CallArgument(float argument) {
      // TODO(all): CallArgument(float) is untested, remove this check once
      //            tested.
      UNIMPLEMENTED();
      // Make the D register a NaN to try to trap errors if the callee expects a
      // double. If it expects a float, the callee should ignore the top word.
      ASSERT(sizeof(kFP64SignallingNaN) == sizeof(bits_));
      memcpy(&bits_, &kFP64SignallingNaN, sizeof(kFP64SignallingNaN));
      // Write the float payload to the S register.
      ASSERT(sizeof(argument) <= sizeof(bits_));
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
  uintptr_t StackLimit() const;

  void ResetState();

  // Runtime call support.
  static void* RedirectExternalReference(void* external_function,
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
    ASSERT(sizeof(T) == sizeof(pc_));
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
    ASSERT(IsAligned(reinterpret_cast<uintptr_t>(pc_), kInstructionSize));
    CheckBreakNext();
    Decode(pc_);
    LogProcessorState();
    increment_pc();
    CheckBreakpoints();
  }

  // Declare all Visitor functions.
  #define DECLARE(A)  void Visit##A(Instruction* instr);
  VISITOR_LIST(DECLARE)
  #undef DECLARE

  // Register accessors.

  // Return 'size' bits of the value of an integer register, as the specified
  // type. The value is zero-extended to fill the result.
  //
  // The only supported values of 'size' are kXRegSizeInBits and
  // kWRegSizeInBits.
  template<typename T>
  T reg(unsigned size, unsigned code,
        Reg31Mode r31mode = Reg31IsZeroRegister) const {
    unsigned size_in_bytes = size / 8;
    ASSERT(size_in_bytes <= sizeof(T));
    ASSERT((size == kXRegSizeInBits) || (size == kWRegSizeInBits));
    ASSERT(code < kNumberOfRegisters);

    if ((code == 31) && (r31mode == Reg31IsZeroRegister)) {
      T result;
      memset(&result, 0, sizeof(result));
      return result;
    }
    return registers_[code].Get<T>(size_in_bytes);
  }

  // Like reg(), but infer the access size from the template type.
  template<typename T>
  T reg(unsigned code, Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<T>(sizeof(T) * 8, code, r31mode);
  }

  // Common specialized accessors for the reg() template.
  int32_t wreg(unsigned code,
               Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<int32_t>(code, r31mode);
  }

  int64_t xreg(unsigned code,
               Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<int64_t>(code, r31mode);
  }

  int64_t reg(unsigned size, unsigned code,
              Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<int64_t>(size, code, r31mode);
  }

  // Write 'size' bits of 'value' into an integer register. The value is
  // zero-extended. This behaviour matches AArch64 register writes.
  //
  // The only supported values of 'size' are kXRegSizeInBits and
  // kWRegSizeInBits.
  template<typename T>
  void set_reg(unsigned size, unsigned code, T value,
               Reg31Mode r31mode = Reg31IsZeroRegister) {
    unsigned size_in_bytes = size / 8;
    ASSERT(size_in_bytes <= sizeof(T));
    ASSERT((size == kXRegSizeInBits) || (size == kWRegSizeInBits));
    ASSERT(code < kNumberOfRegisters);

    if ((code == 31) && (r31mode == Reg31IsZeroRegister)) {
      return;
    }
    return registers_[code].Set(value, size_in_bytes);
  }

  // Like set_reg(), but infer the access size from the template type.
  template<typename T>
  void set_reg(unsigned code, T value,
               Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg(sizeof(value) * 8, code, value, r31mode);
  }

  // Common specialized accessors for the set_reg() template.
  void set_wreg(unsigned code, int32_t value,
                Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg(kWRegSizeInBits, code, value, r31mode);
  }

  void set_xreg(unsigned code, int64_t value,
                Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg(kXRegSizeInBits, code, value, r31mode);
  }

  // Commonly-used special cases.
  template<typename T>
  void set_lr(T value) {
    ASSERT(sizeof(T) == kPointerSize);
    set_reg(kLinkRegCode, value);
  }

  template<typename T>
  void set_sp(T value) {
    ASSERT(sizeof(T) == kPointerSize);
    set_reg(31, value, Reg31IsStackPointer);
  }

  int64_t sp() { return xreg(31, Reg31IsStackPointer); }
  int64_t jssp() { return xreg(kJSSPCode, Reg31IsStackPointer); }
  int64_t fp() {
      return xreg(kFramePointerRegCode, Reg31IsStackPointer);
  }
  Instruction* lr() { return reg<Instruction*>(kLinkRegCode); }

  Address get_sp() { return reg<Address>(31, Reg31IsStackPointer); }

  // Return 'size' bits of the value of a floating-point register, as the
  // specified type. The value is zero-extended to fill the result.
  //
  // The only supported values of 'size' are kDRegSizeInBits and
  // kSRegSizeInBits.
  template<typename T>
  T fpreg(unsigned size, unsigned code) const {
    unsigned size_in_bytes = size / 8;
    ASSERT(size_in_bytes <= sizeof(T));
    ASSERT((size == kDRegSizeInBits) || (size == kSRegSizeInBits));
    ASSERT(code < kNumberOfFPRegisters);
    return fpregisters_[code].Get<T>(size_in_bytes);
  }

  // Like fpreg(), but infer the access size from the template type.
  template<typename T>
  T fpreg(unsigned code) const {
    return fpreg<T>(sizeof(T) * 8, code);
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
    ASSERT((sizeof(value) == kDRegSize) || (sizeof(value) == kSRegSize));
    ASSERT(code < kNumberOfFPRegisters);
    fpregisters_[code].Set(value, sizeof(value));
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

  void PrintSystemRegisters(bool print_all = false);
  void PrintRegisters(bool print_all_regs = false);
  void PrintFPRegisters(bool print_all_regs = false);
  void PrintProcessorState();
  void PrintWrite(uint8_t* address, uint64_t value, unsigned num_bytes);
  void LogSystemRegisters() {
    if (log_parameters_ & LOG_SYS_REGS) PrintSystemRegisters();
  }
  void LogRegisters() {
    if (log_parameters_ & LOG_REGS) PrintRegisters();
  }
  void LogFPRegisters() {
    if (log_parameters_ & LOG_FP_REGS) PrintFPRegisters();
  }
  void LogProcessorState() {
    LogSystemRegisters();
    LogRegisters();
    LogFPRegisters();
  }
  void LogWrite(uint8_t* address, uint64_t value, unsigned num_bytes) {
    if (log_parameters_ & LOG_WRITE) PrintWrite(address, value, num_bytes);
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

  void AddSubHelper(Instruction* instr, int64_t op2);
  int64_t AddWithCarry(unsigned reg_size,
                       bool set_flags,
                       int64_t src1,
                       int64_t src2,
                       int64_t carry_in = 0);
  void LogicalHelper(Instruction* instr, int64_t op2);
  void ConditionalCompareHelper(Instruction* instr, int64_t op2);
  void LoadStoreHelper(Instruction* instr,
                       int64_t offset,
                       AddrMode addrmode);
  void LoadStorePairHelper(Instruction* instr, AddrMode addrmode);
  uint8_t* LoadStoreAddress(unsigned addr_reg,
                            int64_t offset,
                            AddrMode addrmode);
  void LoadStoreWriteBack(unsigned addr_reg,
                          int64_t offset,
                          AddrMode addrmode);
  void CheckMemoryAccess(uint8_t* address, uint8_t* stack);

  uint64_t MemoryRead(uint8_t* address, unsigned num_bytes);
  uint8_t MemoryRead8(uint8_t* address);
  uint16_t MemoryRead16(uint8_t* address);
  uint32_t MemoryRead32(uint8_t* address);
  float MemoryReadFP32(uint8_t* address);
  uint64_t MemoryRead64(uint8_t* address);
  double MemoryReadFP64(uint8_t* address);

  void MemoryWrite(uint8_t* address, uint64_t value, unsigned num_bytes);
  void MemoryWrite32(uint8_t* address, uint32_t value);
  void MemoryWriteFP32(uint8_t* address, float value);
  void MemoryWrite64(uint8_t* address, uint64_t value);
  void MemoryWriteFP64(uint8_t* address, double value);

  int64_t ShiftOperand(unsigned reg_size,
                       int64_t value,
                       Shift shift_type,
                       unsigned amount);
  int64_t Rotate(unsigned reg_width,
                 int64_t value,
                 Shift shift_type,
                 unsigned amount);
  int64_t ExtendValue(unsigned reg_width,
                      int64_t value,
                      Extend extend_type,
                      unsigned left_shift = 0);

  uint64_t ReverseBits(uint64_t value, unsigned num_bits);
  uint64_t ReverseBytes(uint64_t value, ReverseByteMode mode);

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

  // Processor state ---------------------------------------

  // Output stream.
  FILE* stream_;
  PrintDisassembler* print_disasm_;

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
    ASSERT(fpcr().FZ() == 0);             // No flush-to-zero support.
    ASSERT(fpcr().RMode() == FPTieEven);  // Ties-to-even rounding only.

    // The simulator does not support half-precision operations so fpcr().AHP()
    // is irrelevant, and is not checked here.
  }

  static int CalcNFlag(uint64_t result, unsigned reg_size) {
    return (result >> (reg_size - 1)) & 1;
  }

  static int CalcZFlag(uint64_t result) {
    return result == 0;
  }

  static const uint32_t kConditionFlagsMask = 0xf0000000;

  // Stack
  byte* stack_;
  static const intptr_t stack_protection_size_ = KB;
  intptr_t stack_size_;
  byte* stack_limit_;

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
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  reinterpret_cast<Object*>(Simulator::current(Isolate::Current())->CallJS(    \
      FUNCTION_ADDR(entry),                                                    \
      p0, p1, p2, p3, p4))

#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7, p8)  \
  Simulator::current(Isolate::Current())->CallRegExp(                          \
      entry,                                                                   \
      p0, p1, p2, p3, p4, p5, p6, p7, NULL, p8)

#define TRY_CATCH_FROM_ADDRESS(try_catch_address)                              \
  try_catch_address == NULL ?                                                  \
      NULL : *(reinterpret_cast<TryCatch**>(try_catch_address))


// The simulator has its own stack. Thus it has a different stack limit from
// the C-based native code.
// See also 'class SimulatorStack' in arm/simulator-arm.h.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static uintptr_t JsLimitFromCLimit(v8::internal::Isolate* isolate,
                                            uintptr_t c_limit) {
    return Simulator::current(isolate)->StackLimit();
  }

  static uintptr_t RegisterCTryCatch(uintptr_t try_catch_address) {
    Simulator* sim = Simulator::current(Isolate::Current());
    return sim->PushAddress(try_catch_address);
  }

  static void UnregisterCTryCatch() {
    Simulator::current(Isolate::Current())->PopAddress();
  }
};

#endif  // !defined(USE_SIMULATOR)

} }  // namespace v8::internal

#endif  // V8_ARM64_SIMULATOR_ARM64_H_
