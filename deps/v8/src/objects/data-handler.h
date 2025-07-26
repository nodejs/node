// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DATA_HANDLER_H_
#define V8_OBJECTS_DATA_HANDLER_H_

#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"
#include "src/objects/tagged-field.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/data-handler-tq.inc"

// DataHandler is a base class for load and store handlers that can't be
// encoded in one Smi. Kind of a handler can be deduced from instance type.
V8_OBJECT class DataHandler : public StructLayout {
 public:
  static constexpr int OffsetOf(int index);
  static constexpr int SizeFor(int count);

  // [smi_handler]: A Smi which encodes a handler or Code object
  // (we still use code handlers for accessing lexical environment variables,
  // but soon only smi handlers will remain). See LoadHandler and StoreHandler
  // for details about encoding.
  inline Tagged<UnionOf<Smi, Code>> smi_handler() const;
  inline void set_smi_handler(Tagged<Smi> value);
  inline void set_smi_handler(Tagged<Code> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [validity_cell]: A validity Cell that guards prototype chain modifications.
  inline Tagged<UnionOf<Smi, Cell>> validity_cell() const;
  inline void set_validity_cell(Tagged<UnionOf<Smi, Cell>> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Returns number of optional data fields available in the object.
  inline int data_field_count() const;

  // [data1-3]: These are optional general-purpose fields whose content and
  // presence depends on the handler kind.
  inline Tagged<MaybeObject> data1() const;
  inline void set_data1(Tagged<MaybeObject> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<MaybeObject> data2() const;
  inline void set_data2(Tagged<MaybeObject> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<MaybeObject> data3() const;
  inline void set_data3(Tagged<MaybeObject> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_VERIFIER(DataHandler)
  DECL_PRINTER(DataHandler)

  class BodyDescriptor;

 private:
  friend class AccessorAssembler;

  TaggedMember<UnionOf<Smi, Code>> smi_handler_;
  TaggedMember<UnionOf<Smi, Cell>> validity_cell_;
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<MaybeObject>, data);
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DATA_HANDLER_H_
