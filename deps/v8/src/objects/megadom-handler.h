// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MEGADOM_HANDLER_H_
#define V8_OBJECTS_MEGADOM_HANDLER_H_

#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/megadom-handler-tq.inc"

V8_OBJECT class MegaDomHandler : public HeapObject {
 public:
  inline Tagged<MaybeObject> accessor() const;
  inline Tagged<MaybeObject> accessor(AcquireLoadTag) const;
  inline void set_accessor(
      Tagged<MaybeObject> value,
      WriteBarrierMode mode = WriteBarrierMode::UPDATE_WRITE_BARRIER);
  inline void set_accessor(
      Tagged<MaybeObject> value, ReleaseStoreTag,
      WriteBarrierMode mode = WriteBarrierMode::UPDATE_WRITE_BARRIER);

  inline Tagged<MaybeObject> context() const;
  inline void set_context(
      Tagged<MaybeObject> value,
      WriteBarrierMode mode = WriteBarrierMode::UPDATE_WRITE_BARRIER);

  void BriefPrintDetails(std::ostream& os);

  DECL_PRINTER(MegaDomHandler)
  DECL_VERIFIER(MegaDomHandler)

 public:
  TaggedMember<MaybeObject> accessor_;
  TaggedMember<MaybeObject> context_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<MegaDomHandler> {
  using BodyDescriptor =
      FixedWeakBodyDescriptor<offsetof(MegaDomHandler, accessor_),
                              sizeof(MegaDomHandler), sizeof(MegaDomHandler)>;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MEGADOM_HANDLER_H_
