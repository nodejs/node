// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DATA_HANDLER_INL_H_
#define V8_OBJECTS_DATA_HANDLER_INL_H_

#include "src/objects/data-handler.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/map.h"
#include "src/objects/objects.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/data-handler-tq-inl.inc"

// static
constexpr int DataHandler::OffsetOf(int index) {
  return OFFSET_OF_DATA_START(DataHandler) +
         index * sizeof(DataHandler::FlexibleDataType);
}
// static
constexpr int DataHandler::SizeFor(int count) {
  return OFFSET_OF_DATA_START(DataHandler) +
         count * sizeof(DataHandler::FlexibleDataType);
}

inline Tagged<UnionOf<Smi, Code>> DataHandler::smi_handler() const {
  return smi_handler_.load();
}
inline void DataHandler::set_smi_handler(Tagged<Smi> value) {
  smi_handler_.store(this, value, SKIP_WRITE_BARRIER);
}
inline void DataHandler::set_smi_handler(Tagged<Code> value,
                                         WriteBarrierMode mode) {
  smi_handler_.store(this, value, UPDATE_WRITE_BARRIER);
}

inline Tagged<UnionOf<Smi, Cell>> DataHandler::validity_cell() const {
  return validity_cell_.load();
}
inline void DataHandler::set_validity_cell(Tagged<UnionOf<Smi, Cell>> value,
                                           WriteBarrierMode mode) {
  validity_cell_.store(this, value, mode);
}

inline int DataHandler::data_field_count() const {
  return (map()->instance_size() - OFFSET_OF_DATA_START(DataHandler)) /
         sizeof(data()[0]);
}

inline Tagged<MaybeObject> DataHandler::data1() const {
  DCHECK_GT(data_field_count(), 0);
  return data()[0].load();
}
inline void DataHandler::set_data1(Tagged<MaybeObject> value,
                                   WriteBarrierMode mode) {
  DCHECK_GT(data_field_count(), 0);
  data()[0].store(this, value, mode);
}

inline Tagged<MaybeObject> DataHandler::data2() const {
  DCHECK_GT(data_field_count(), 1);
  return data()[1].load();
}
inline void DataHandler::set_data2(Tagged<MaybeObject> value,
                                   WriteBarrierMode mode) {
  DCHECK_GT(data_field_count(), 1);
  data()[1].store(this, value, mode);
}

inline Tagged<MaybeObject> DataHandler::data3() const {
  DCHECK_GT(data_field_count(), 2);
  return data()[2].load();
}
inline void DataHandler::set_data3(Tagged<MaybeObject> value,
                                   WriteBarrierMode mode) {
  DCHECK_GT(data_field_count(), 2);
  data()[2].store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DATA_HANDLER_INL_H_
