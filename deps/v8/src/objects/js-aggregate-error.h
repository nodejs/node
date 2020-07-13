// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_AGGREGATE_ERROR_H_
#define V8_OBJECTS_JS_AGGREGATE_ERROR_H_

#include "src/objects/js-objects.h"
#include "torque-generated/builtin-definitions-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class JSAggregateError
    : public TorqueGeneratedJSAggregateError<JSAggregateError, JSObject> {
 public:
  DECL_PRINTER(JSAggregateError)
  TQ_OBJECT_CONSTRUCTORS(JSAggregateError)
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_JS_AGGREGATE_ERROR_H_
