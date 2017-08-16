// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_FLAGS_H_
#define V8_INTERPRETER_BYTECODE_FLAGS_H_

#include "src/utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Literal;
class AstStringConstants;

namespace interpreter {

class CreateArrayLiteralFlags {
 public:
  class FlagsBits : public BitField8<int, 0, 4> {};
  class FastShallowCloneBit : public BitField8<bool, FlagsBits::kNext, 1> {};

  static uint8_t Encode(bool use_fast_shallow_clone, int runtime_flags);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CreateArrayLiteralFlags);
};

class CreateObjectLiteralFlags {
 public:
  class FlagsBits : public BitField8<int, 0, 4> {};
  class FastCloneSupportedBit : public BitField8<bool, FlagsBits::kNext, 1> {};

  static uint8_t Encode(int runtime_flags, bool fast_clone_supported);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CreateObjectLiteralFlags);
};

class CreateClosureFlags {
 public:
  class PretenuredBit : public BitField8<bool, 0, 1> {};
  class FastNewClosureBit : public BitField8<bool, PretenuredBit::kNext, 1> {};

  static uint8_t Encode(bool pretenure, bool is_function_scope);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CreateClosureFlags);
};

#define TYPEOF_LITERAL_LIST(V) \
  V(Number, number)            \
  V(String, string)            \
  V(Symbol, symbol)            \
  V(Boolean, boolean)          \
  V(Undefined, undefined)      \
  V(Function, function)        \
  V(Object, object)            \
  V(Other, other)

class TestTypeOfFlags {
 public:
  enum class LiteralFlag : uint8_t {
#define DECLARE_LITERAL_FLAG(name, _) k##name,
    TYPEOF_LITERAL_LIST(DECLARE_LITERAL_FLAG)
#undef DECLARE_LITERAL_FLAG
  };

  static LiteralFlag GetFlagForLiteral(const AstStringConstants* ast_constants,
                                       Literal* literal);
  static uint8_t Encode(LiteralFlag literal_flag);
  static LiteralFlag Decode(uint8_t raw_flag);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TestTypeOfFlags);
};

class SuspendGeneratorBytecodeFlags {
 public:
  class FlagsBits
      : public BitField8<SuspendFlags, 0,
                         static_cast<int>(SuspendFlags::kBitWidth)> {};

  static uint8_t Encode(SuspendFlags suspend_type);
  static SuspendFlags Decode(uint8_t flags);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SuspendGeneratorBytecodeFlags);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_FLAGS_H_
