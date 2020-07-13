// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_H_
#define V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_H_

#include "src/objects/js-objects.h"
#include "torque-generated/bit-fields-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class JSRegExpStringIterator
    : public TorqueGeneratedJSRegExpStringIterator<JSRegExpStringIterator,
                                                   JSObject> {
 public:
  // [boolean]: The [[Done]] internal property.
  DECL_BOOLEAN_ACCESSORS(done)

  // [boolean]: The [[Global]] internal property.
  DECL_BOOLEAN_ACCESSORS(global)

  // [boolean]: The [[Unicode]] internal property.
  DECL_BOOLEAN_ACCESSORS(unicode)

  DECL_PRINTER(JSRegExpStringIterator)

  DEFINE_TORQUE_GENERATED_JS_REG_EXP_STRING_ITERATOR_FLAGS()

  TQ_OBJECT_CONSTRUCTORS(JSRegExpStringIterator)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_H_
