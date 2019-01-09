// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FEEDBACK_CELL_H_
#define V8_OBJECTS_FEEDBACK_CELL_H_

#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// This is a special cell used to maintain both the link between a
// closure and it's feedback vector, as well as a way to count the
// number of closures created for a certain function per native
// context. There's at most one FeedbackCell for each function in
// a native context.
class FeedbackCell : public Struct {
 public:
  // [value]: value of the cell.
  DECL_ACCESSORS2(value, HeapObject)

  DECL_CAST2(FeedbackCell)

  // Dispatched behavior.
  DECL_PRINTER(FeedbackCell)
  DECL_VERIFIER(FeedbackCell)

  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

  typedef FixedBodyDescriptor<kValueOffset, kValueOffset + kPointerSize, kSize>
      BodyDescriptor;

  OBJECT_CONSTRUCTORS(FeedbackCell, Struct);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FEEDBACK_CELL_H_
