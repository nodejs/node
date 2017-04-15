// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_FLAGS_H_
#define V8_INTERPRETER_BYTECODE_FLAGS_H_

#include "src/utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class CreateArrayLiteralFlags {
 public:
  class FlagsBits : public BitField8<int, 0, 3> {};
  class FastShallowCloneBit : public BitField8<bool, FlagsBits::kNext, 1> {};

  static uint8_t Encode(bool use_fast_shallow_clone, int runtime_flags);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CreateArrayLiteralFlags);
};

class CreateObjectLiteralFlags {
 public:
  class FlagsBits : public BitField8<int, 0, 3> {};
  class FastClonePropertiesCountBits
      : public BitField8<int, FlagsBits::kNext, 3> {};

  static uint8_t Encode(bool fast_clone_supported, int properties_count,
                        int runtime_flags);

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

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_FLAGS_H_
