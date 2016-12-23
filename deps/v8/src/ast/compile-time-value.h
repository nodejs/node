// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_COMPILE_TIME_VALUE
#define V8_AST_COMPILE_TIME_VALUE

#include "src/allocation.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Expression;

// Support for handling complex values (array and object literals) that
// can be fully handled at compile time.
class CompileTimeValue : public AllStatic {
 public:
  enum LiteralType {
    OBJECT_LITERAL_FAST_ELEMENTS,
    OBJECT_LITERAL_SLOW_ELEMENTS,
    ARRAY_LITERAL
  };

  static bool IsCompileTimeValue(Expression* expression);

  // Get the value as a compile time value.
  static Handle<FixedArray> GetValue(Isolate* isolate, Expression* expression);

  // Get the type of a compile time value returned by GetValue().
  static LiteralType GetLiteralType(Handle<FixedArray> value);

  // Get the elements array of a compile time value returned by GetValue().
  static Handle<FixedArray> GetElements(Handle<FixedArray> value);

 private:
  static const int kLiteralTypeSlot = 0;
  static const int kElementsSlot = 1;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_COMPILE_TIME_VALUE
