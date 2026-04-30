// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_H_
#define V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_H_

#include "src/base/bit-field.h"
#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-regexp-string-iterator-tq.inc"

V8_OBJECT class JSRegExpStringIterator : public JSObject {
 public:
  inline Tagged<JSReceiver> iterating_reg_exp() const;
  inline void set_iterating_reg_exp(
      Tagged<JSReceiver> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> iterated_string() const;
  inline void set_iterated_string(Tagged<String> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int flags() const;
  inline void set_flags(int value);

  // [boolean]: The [[Done]] internal property.
  DECL_BOOLEAN_ACCESSORS(done)

  // [boolean]: The [[Global]] internal property.
  DECL_BOOLEAN_ACCESSORS(global)

  // [boolean]: The [[Unicode]] internal property.
  DECL_BOOLEAN_ACCESSORS(unicode)

  DECL_PRINTER(JSRegExpStringIterator)
  DECL_VERIFIER(JSRegExpStringIterator)

  // Bit layout for flags_.
  using DoneBit = base::BitField<bool, 0, 1>;
  using GlobalBit = DoneBit::Next<bool, 1>;
  using UnicodeBit = GlobalBit::Next<bool, 1>;

 public:
  TaggedMember<JSReceiver> iterating_reg_exp_;
  TaggedMember<String> iterated_string_;
  // SmiTagged<JSRegExpStringIteratorFlags>.
  TaggedMember<Smi> flags_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_H_
