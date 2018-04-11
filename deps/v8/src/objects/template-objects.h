// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATE_OBJECTS_H_
#define V8_OBJECTS_TEMPLATE_OBJECTS_H_

#include "src/objects.h"
#include "src/objects/hash-table.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// TemplateObjectDescription is a triple of hash, raw strings and cooked
// strings for tagged template literals. Used to communicate with the runtime
// for template object creation within the {Runtime_CreateTemplateObject}
// method.
class TemplateObjectDescription final : public Tuple2 {
 public:
  DECL_ACCESSORS(raw_strings, FixedArray)
  DECL_ACCESSORS(cooked_strings, FixedArray)

  static Handle<JSArray> CreateTemplateObject(
      Handle<TemplateObjectDescription> description);

  DECL_CAST(TemplateObjectDescription)

  static constexpr int kRawStringsOffset = kValue1Offset;
  static constexpr int kCookedStringsOffset = kValue2Offset;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateObjectDescription);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATE_OBJECTS_H_
