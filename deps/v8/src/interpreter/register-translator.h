// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_REGISTER_TRANSLATOR_H_
#define V8_INTERPRETER_REGISTER_TRANSLATOR_H_

#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace interpreter {

class RegisterMover;

// A class that enables bytecodes having only byte sized register operands
// to access all registers in the two byte space. Most bytecode uses few
// registers so space can be saved if most bytecodes with register operands
// just take byte operands.
//
// To reach the wider register space, a translation window is reserved in
// the byte addressable space specifically for copying registers into and
// out of before a bytecode is emitted. The translation window occupies
// the last register slots at the top of the byte addressable range.
//
// Because of the translation window any registers which naturally lie
// at above the translation window have to have their register index
// incremented by the window width before they are emitted.
//
// This class does not support moving ranges of registers to and from
// the translation window. It would be straightforward to add support
// for constrained ranges, e.g. kRegPair8, kRegTriple8 operands, but
// these would have two negative effects. The translation window would
// need to be wider, further limiting the space for byte operands. And
// every register in a range would need to be moved consuming more
// space in the bytecode array.
class RegisterTranslator final {
 public:
  explicit RegisterTranslator(RegisterMover* mover);

  // Translate and re-write the register operands that are inputs
  // to |bytecode| when it is about to be emitted.
  void TranslateInputRegisters(Bytecode bytecode, uint32_t* raw_operands,
                               int raw_operand_count);

  // Translate and re-write the register operands that are outputs
  // from |bytecode| when it has just been output.
  void TranslateOutputRegisters();

  // Returns true if |reg| is in the translation window.
  static bool InTranslationWindow(Register reg);

  // Return register value as if it had been translated.
  static Register UntranslateRegister(Register reg);

  // Returns the distance in registers between the translation window
  // start and |reg|. The result is negative when |reg| is above the
  // start of the translation window.
  static int DistanceToTranslationWindow(Register reg);

  // Returns true if |reg| can be represented as an 8-bit operand
  // after translation.
  static bool FitsInReg8Operand(Register reg);

  // Returns true if |reg| can be represented as an 16-bit operand
  // after translation.
  static bool FitsInReg16Operand(Register reg);

  // Returns the increment to the register count necessary if the
  // value indicates the translation window is required.
  static int RegisterCountAdjustment(int register_count, int parameter_count);

 private:
  static const int kTranslationWindowLength = 4;
  static const int kTranslationWindowLimit = -kMinInt8;
  static const int kTranslationWindowStart =
      kTranslationWindowLimit - kTranslationWindowLength + 1;

  Register TranslateAndMove(Bytecode bytecode, int operand_index, Register reg);
  static bool RegisterIsMovableToWindow(Bytecode bytecode, int operand_index);

  static Register Translate(Register reg);

  RegisterMover* mover() const { return mover_; }

  // Entity to perform register moves necessary to translate registers
  // and ensure reachability.
  RegisterMover* mover_;

  // Flag to avoid re-entrancy when emitting move bytecodes for
  // translation.
  bool emitting_moves_;

  // Number of window registers in use.
  int window_registers_count_;

  // State for restoring register moves emitted by TranslateOutputRegisters.
  std::pair<Register, Register> output_moves_[kTranslationWindowLength];
  int output_moves_count_;
};

// Interface for RegisterTranslator helper class that will emit
// register move bytecodes at the translator's behest.
class RegisterMover {
 public:
  virtual ~RegisterMover() {}

  // Move register |from| to register |to| with no translation.
  // returns false if either register operand is invalid. Implementations
  // of this method must be aware that register moves with bad
  // register values are a security hole.
  virtual void MoveRegisterUntranslated(Register from, Register to) = 0;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_REGISTER_TRANSLATOR_H_
