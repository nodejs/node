// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/init-expr-interface.h"

#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/oddball.h"
#include "src/roots/roots-inl.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

void InitExprInterface::I32Const(FullDecoder* decoder, Value* result,
                                 int32_t value) {
  if (generate_result()) result->runtime_value = WasmValue(value);
}

void InitExprInterface::I64Const(FullDecoder* decoder, Value* result,
                                 int64_t value) {
  if (generate_result()) result->runtime_value = WasmValue(value);
}

void InitExprInterface::F32Const(FullDecoder* decoder, Value* result,
                                 float value) {
  if (generate_result()) result->runtime_value = WasmValue(value);
}

void InitExprInterface::F64Const(FullDecoder* decoder, Value* result,
                                 double value) {
  if (generate_result()) result->runtime_value = WasmValue(value);
}

void InitExprInterface::S128Const(FullDecoder* decoder,
                                  Simd128Immediate<validate>& imm,
                                  Value* result) {
  if (!generate_result()) return;
  result->runtime_value = WasmValue(imm.value, kWasmS128);
}

void InitExprInterface::BinOp(FullDecoder* decoder, WasmOpcode opcode,
                              const Value& lhs, const Value& rhs,
                              Value* result) {
  if (!generate_result()) return;
  switch (opcode) {
    case kExprI32Add:
      result->runtime_value =
          WasmValue(lhs.runtime_value.to_i32() + rhs.runtime_value.to_i32());
      break;
    case kExprI32Sub:
      result->runtime_value =
          WasmValue(lhs.runtime_value.to_i32() - rhs.runtime_value.to_i32());
      break;
    case kExprI32Mul:
      result->runtime_value =
          WasmValue(lhs.runtime_value.to_i32() * rhs.runtime_value.to_i32());
      break;
    case kExprI64Add:
      result->runtime_value =
          WasmValue(lhs.runtime_value.to_i64() + rhs.runtime_value.to_i64());
      break;
    case kExprI64Sub:
      result->runtime_value =
          WasmValue(lhs.runtime_value.to_i64() - rhs.runtime_value.to_i64());
      break;
    case kExprI64Mul:
      result->runtime_value =
          WasmValue(lhs.runtime_value.to_i64() * rhs.runtime_value.to_i64());
      break;
    default:
      UNREACHABLE();
  }
}

void InitExprInterface::RefNull(FullDecoder* decoder, ValueType type,
                                Value* result) {
  if (!generate_result()) return;
  result->runtime_value = WasmValue(isolate_->factory()->null_value(), type);
}

void InitExprInterface::RefFunc(FullDecoder* decoder, uint32_t function_index,
                                Value* result) {
  if (isolate_ == nullptr) {
    outer_module_->functions[function_index].declared = true;
    return;
  }
  if (!generate_result()) return;
  ValueType type = ValueType::Ref(module_->functions[function_index].sig_index,
                                  kNonNullable);
  Handle<WasmInternalFunction> internal =
      WasmInstanceObject::GetOrCreateWasmInternalFunction(isolate_, instance_,
                                                          function_index);
  result->runtime_value = WasmValue(internal, type);
}

void InitExprInterface::GlobalGet(FullDecoder* decoder, Value* result,
                                  const GlobalIndexImmediate<validate>& imm) {
  if (!generate_result()) return;
  const WasmGlobal& global = module_->globals[imm.index];
  DCHECK(!global.mutability);
  result->runtime_value =
      global.type.is_numeric()
          ? WasmValue(
                reinterpret_cast<byte*>(
                    instance_->untagged_globals_buffer().backing_store()) +
                    global.offset,
                global.type)
          : WasmValue(
                handle(instance_->tagged_globals_buffer().get(global.offset),
                       isolate_),
                global.type);
}

void InitExprInterface::StructNewWithRtt(
    FullDecoder* decoder, const StructIndexImmediate<validate>& imm,
    const Value& rtt, const Value args[], Value* result) {
  if (!generate_result()) return;
  std::vector<WasmValue> field_values(imm.struct_type->field_count());
  for (size_t i = 0; i < field_values.size(); i++) {
    field_values[i] = args[i].runtime_value;
  }
  result->runtime_value =
      WasmValue(isolate_->factory()->NewWasmStruct(
                    imm.struct_type, field_values.data(),
                    Handle<Map>::cast(rtt.runtime_value.to_ref())),
                ValueType::Ref(HeapType(imm.index), kNonNullable));
}

namespace {
WasmValue DefaultValueForType(ValueType type, Isolate* isolate) {
  switch (type.kind()) {
    case kI32:
    case kI8:
    case kI16:
      return WasmValue(0);
    case kI64:
      return WasmValue(int64_t{0});
    case kF32:
      return WasmValue(0.0f);
    case kF64:
      return WasmValue(0.0);
    case kS128:
      return WasmValue(Simd128());
    case kOptRef:
      return WasmValue(isolate->factory()->null_value(), type);
    case kVoid:
    case kRtt:
    case kRef:
    case kBottom:
      UNREACHABLE();
  }
}
}  // namespace

void InitExprInterface::StructNewDefault(
    FullDecoder* decoder, const StructIndexImmediate<validate>& imm,
    const Value& rtt, Value* result) {
  if (!generate_result()) return;
  std::vector<WasmValue> field_values(imm.struct_type->field_count());
  for (uint32_t i = 0; i < field_values.size(); i++) {
    field_values[i] = DefaultValueForType(imm.struct_type->field(i), isolate_);
  }
  result->runtime_value =
      WasmValue(isolate_->factory()->NewWasmStruct(
                    imm.struct_type, field_values.data(),
                    Handle<Map>::cast(rtt.runtime_value.to_ref())),
                ValueType::Ref(HeapType(imm.index), kNonNullable));
}

void InitExprInterface::ArrayInit(FullDecoder* decoder,
                                  const ArrayIndexImmediate<validate>& imm,
                                  const base::Vector<Value>& elements,
                                  const Value& rtt, Value* result) {
  if (!generate_result()) return;
  std::vector<WasmValue> element_values;
  for (Value elem : elements) element_values.push_back(elem.runtime_value);
  result->runtime_value =
      WasmValue(isolate_->factory()->NewWasmArrayFromElements(
                    imm.array_type, element_values,
                    Handle<Map>::cast(rtt.runtime_value.to_ref())),
                ValueType::Ref(HeapType(imm.index), kNonNullable));
}

void InitExprInterface::ArrayInitFromData(
    FullDecoder* decoder, const ArrayIndexImmediate<validate>& array_imm,
    const IndexImmediate<validate>& data_segment_imm, const Value& offset_value,
    const Value& length_value, const Value& rtt, Value* result) {
  if (!generate_result()) return;

  uint32_t length = length_value.runtime_value.to_u32();
  uint32_t offset = offset_value.runtime_value.to_u32();
  const WasmDataSegment& data_segment =
      module_->data_segments[data_segment_imm.index];
  uint32_t length_in_bytes =
      length * array_imm.array_type->element_type().value_kind_size();

  // Error handling.
  if (length >
      static_cast<uint32_t>(WasmArray::MaxLength(array_imm.array_type))) {
    error_ = "length for array.init_from_data too large";
    return;
  }
  if (!base::IsInBounds<uint32_t>(offset, length_in_bytes,
                                  data_segment.source.length())) {
    error_ = "data segment is out of bounds";
    return;
  }

  Address source =
      instance_->data_segment_starts()[data_segment_imm.index] + offset;
  Handle<WasmArray> array_value = isolate_->factory()->NewWasmArrayFromMemory(
      length, Handle<Map>::cast(rtt.runtime_value.to_ref()), source);
  result->runtime_value = WasmValue(
      array_value, ValueType::Ref(HeapType(array_imm.index), kNonNullable));
}

void InitExprInterface::RttCanon(FullDecoder* decoder, uint32_t type_index,
                                 Value* result) {
  if (!generate_result()) return;
  result->runtime_value = WasmValue(
      handle(instance_->managed_object_maps().get(type_index), isolate_),
      ValueType::Rtt(type_index));
}

void InitExprInterface::DoReturn(FullDecoder* decoder,
                                 uint32_t /*drop_values*/) {
  end_found_ = true;
  // End decoding on "end".
  decoder->set_end(decoder->pc() + 1);
  if (generate_result()) result_ = decoder->stack_value(1)->runtime_value;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
