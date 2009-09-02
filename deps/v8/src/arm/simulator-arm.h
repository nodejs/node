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

#if defined(__arm__)

// When running without a simulator we call the entry directly.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  (entry(p0, p1, p2, p3, p4))

// Calculated the stack limit beyond which we will throw stack overflow errors.
// This macro must be called from a C++ method. It relies on being able to take
// the address of "this" to get a value on the current execution stack and then
// calculates the stack limit based on that value.
#define GENERATED_CODE_STACK_LIMIT(limit) \
  (reinterpret_cast<uintptr_t>(this) - limit)


// Call the generated regexp code directly. The entry function pointer should
// expect seven int/pointer sized arguments and return an int.
#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6) \
  entry(p0, p1, p2, p3, p4, p5, p6)

#else  // defined(__arm__)

// When running with the simulator transition into simulated execution at this
// point.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  reinterpret_cast<Object*>( \
      assembler::arm::Simulator::current()->Call(FUNCTION_ADDR(entry), 5, \
                                                 p0, p1, p2, p3, p4))

// The simulator has its own stack. Thus it has a different stack limit from
// the C-based native code.
#define GENERATED_CODE_STACK_LIMIT(limit) \
  (assembler::arm::Simulator::current()->StackLimit())


#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6) \
  assembler::arm::Simulator::current()->Call( \
    FUNCTION_ADDR(entry), 7, p0, p1, p2, p3, p4, p5, p6)

#include "constants-arm.h"


namespace assembler {
namespace arm {

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
    pc = 15
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
  void Format(Instr* instr, const char* format);

  // Checks if the current instruction should be executed based on its
  // condition bits.
  bool ConditionallyExecute(Instr* instr);

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

  // Helper functions to decode common "addressing" modes
  int32_t GetShiftRm(Instr* instr, bool* carry_out);
  int32_t GetImm(Instr* instr, bool* carry_out);
  void HandleRList(Instr* instr, bool load);
  void SoftwareInterrupt(Instr* instr);

  // Read and write memory.
  inline uint8_t ReadBU(int32_t addr);
  inline int8_t ReadB(int32_t addr);
  inline void WriteB(int32_t addr, uint8_t value);
  inline void WriteB(int32_t addr, int8_t value);

  inline uint16_t ReadHU(int32_t addr, Instr* instr);
  inline int16_t ReadH(int32_t addr, Instr* instr);
  // Note: Overloaded on the sign of the value.
  inline void WriteH(int32_t addr, uint16_t value, Instr* instr);
  inline void WriteH(int32_t addr, int16_t value, Instr* instr);

  inline int ReadW(int32_t addr, Instr* instr);
  inline void WriteW(int32_t addr, int value, Instr* instr);

  // Executing is handled based on the instruction type.
  void DecodeType01(Instr* instr);  // both type 0 and type 1 rolled into one
  void DecodeType2(Instr* instr);
  void DecodeType3(Instr* instr);
  void DecodeType4(Instr* instr);
  void DecodeType5(Instr* instr);
  void DecodeType6(Instr* instr);
  void DecodeType7(Instr* instr);
  void DecodeUnconditional(Instr* instr);

  // Executes one instruction.
  void InstructionDecode(Instr* instr);

  // Runtime call support.
  static void* RedirectExternalReference(void* external_function,
                                         bool fp_return);

  // For use in calls that take two double values, constructed from r0, r1, r2
  // and r3.
  void GetFpArgs(double* x, double* y);
  void SetFpResult(const double& result);
  void TrashCallerSaveRegisters();

  // architecture state
  int32_t registers_[16];
  bool n_flag_;
  bool z_flag_;
  bool c_flag_;
  bool v_flag_;

  // simulator support
  char* stack_;
  bool pc_modified_;
  int icount_;
  static bool initialized_;

  // registered breakpoints
  Instr* break_pc_;
  instr_t break_instr_;
};

} }  // namespace assembler::arm

#endif  // defined(__arm__)

#endif  // V8_ARM_SIMULATOR_ARM_H_
