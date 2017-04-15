// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This header contains a set of experimental V8 APIs. We hope these will
 * become a part of standard V8, but they may also be removed if we deem the
 * experiment to not be successul.
 */
#ifndef V8_INCLUDE_V8_EXPERIMENTAL_H_
#define V8_INCLUDE_V8_EXPERIMENTAL_H_

#include "v8.h"  // NOLINT(build/include)

namespace v8 {
namespace experimental {

// Allow the embedder to construct accessors that V8 can compile and use
// directly, without jumping into the runtime.
class V8_EXPORT FastAccessorBuilder {
 public:
  struct ValueId {
    size_t value_id;
  };
  struct LabelId {
    size_t label_id;
  };

  static FastAccessorBuilder* New(Isolate* isolate);

  ValueId IntegerConstant(int int_constant);
  ValueId GetReceiver();
  ValueId LoadInternalField(ValueId value_id, int field_no);
  ValueId LoadInternalFieldUnchecked(ValueId value_id, int field_no);
  ValueId LoadValue(ValueId value_id, int offset);
  ValueId LoadObject(ValueId value_id, int offset);
  ValueId ToSmi(ValueId value_id);

  void ReturnValue(ValueId value_id);
  void CheckFlagSetOrReturnNull(ValueId value_id, int mask);
  void CheckNotZeroOrReturnNull(ValueId value_id);
  LabelId MakeLabel();
  void SetLabel(LabelId label_id);
  void Goto(LabelId label_id);
  void CheckNotZeroOrJump(ValueId value_id, LabelId label_id);
  ValueId Call(v8::FunctionCallback callback, ValueId value_id);

 private:
  FastAccessorBuilder() = delete;
  FastAccessorBuilder(const FastAccessorBuilder&) = delete;
  ~FastAccessorBuilder() = delete;
  void operator=(const FastAccessorBuilder&) = delete;
};

}  // namespace experimental
}  // namespace v8

#endif  // V8_INCLUDE_V8_EXPERIMENTAL_H_
