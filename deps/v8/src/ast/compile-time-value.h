// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_COMPILE_TIME_VALUE_H_
#define V8_AST_COMPILE_TIME_VALUE_H_

#include "src/allocation.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Expression;

// Support for handling complex values (array and object literals) that
// can be fully handled at compile time.
class CompileTimeValue : public AllStatic {
 public:
  // This is a special marker used to encode array literals. The value has to be
  // different from any value possibly returned by
  // ObjectLiteral::EncodeLiteralType.
  static const int kArrayLiteralFlag = -1;

  static bool IsCompileTimeValue(Expression* expression);

  // Get the value as a compile time value.
  static Handle<FixedArray> GetValue(Isolate* isolate, Expression* expression);

  // Get the encoded literal type. This can either be kArrayLiteralFlag or
  // encoded properties of an ObjectLiteral returned by
  // ObjectLiteral::EncodeLiteralType.
  static int GetLiteralTypeFlags(Handle<FixedArray> value);

  // Get the elements of a compile time value returned by GetValue().
  static Handle<HeapObject> GetElements(Handle<FixedArray> value);

 private:
  static const int kLiteralTypeSlot = 0;
  static const int kElementsSlot = 1;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_COMPILE_TIME_VALUE_H_
