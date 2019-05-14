// Copyright 2007-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DISASM_H_
#define V8_DISASM_H_

#include "src/vector.h"

namespace disasm {

typedef unsigned char byte;

// Interface and default implementation for converting addresses and
// register-numbers to text.  The default implementation is machine
// specific.
class NameConverter {
 public:
  virtual ~NameConverter() = default;
  virtual const char* NameOfCPURegister(int reg) const;
  virtual const char* NameOfByteCPURegister(int reg) const;
  virtual const char* NameOfXMMRegister(int reg) const;
  virtual const char* NameOfAddress(byte* addr) const;
  virtual const char* NameOfConstant(byte* addr) const;
  virtual const char* NameInCode(byte* addr) const;

  // Given a root-register-relative offset, returns either a name or nullptr if
  // none is found.
  // TODO(jgruber,v8:7989): This is a temporary solution until we can preserve
  // code comments through snapshotting.
  virtual const char* RootRelativeName(int offset) const { UNREACHABLE(); }

 protected:
  v8::internal::EmbeddedVector<char, 128> tmp_buffer_;
};


// A generic Disassembler interface
class Disassembler {
 public:
  enum UnimplementedOpcodeAction : int8_t {
    kContinueOnUnimplementedOpcode,
    kAbortOnUnimplementedOpcode
  };

  // Caller deallocates converter.
  explicit Disassembler(const NameConverter& converter,
                        UnimplementedOpcodeAction unimplemented_opcode_action =
                            kAbortOnUnimplementedOpcode)
      : converter_(converter),
        unimplemented_opcode_action_(unimplemented_opcode_action) {}

  UnimplementedOpcodeAction unimplemented_opcode_action() const {
    return unimplemented_opcode_action_;
  }

  // Writes one disassembled instruction into 'buffer' (0-terminated).
  // Returns the length of the disassembled machine instruction in bytes.
  int InstructionDecode(v8::internal::Vector<char> buffer, byte* instruction);

  // Returns -1 if instruction does not mark the beginning of a constant pool,
  // or the number of entries in the constant pool beginning here.
  int ConstantPoolSizeAt(byte* instruction);

  // Write disassembly into specified file 'f' using specified NameConverter
  // (see constructor).
  static void Disassemble(FILE* f, byte* begin, byte* end,
                          UnimplementedOpcodeAction unimplemented_action =
                              kAbortOnUnimplementedOpcode);

 private:
  const NameConverter& converter_;
  const UnimplementedOpcodeAction unimplemented_opcode_action_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Disassembler);
};

}  // namespace disasm

#endif  // V8_DISASM_H_
