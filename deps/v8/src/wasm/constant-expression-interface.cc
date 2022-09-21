// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/constant-expression-interface.h"

#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/oddball.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

void ConstantExpressionInterface::I32Const(FullDecoder* decoder, Value* result,
                                           int32_t value) {
  if (generate_value()) result->runtime_value = WasmValue(value);
}

void ConstantExpressionInterface::I64Const(FullDecoder* decoder, Value* result,
                                           int64_t value) {
  if (generate_value()) result->runtime_value = WasmValue(value);
}

void ConstantExpressionInterface::F32Const(FullDecoder* decoder, Value* result,
                                           float value) {
  if (generate_value()) result->runtime_value = WasmValue(value);
}

void ConstantExpressionInterface::F64Const(FullDecoder* decoder, Value* result,
                                           double value) {
  if (generate_value()) result->runtime_value = WasmValue(value);
}

void ConstantExpressionInterface::S128Const(FullDecoder* decoder,
                                            Simd128Immediate<validate>& imm,
                                            Value* result) {
  if (!generate_value()) return;
  result->runtime_value = WasmValue(imm.value, kWasmS128);
}

void ConstantExpressionInterface::BinOp(FullDecoder* decoder, WasmOpcode opcode,
                                        const Value& lhs, const Value& rhs,
                                        Value* result) {
  if (!generate_value()) return;
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

void ConstantExpressionInterface::RefNull(FullDecoder* decoder, ValueType type,
                                          Value* result) {
  if (!generate_value()) return;
  result->runtime_value = WasmValue(isolate_->factory()->null_value(), type);
}

void ConstantExpressionInterface::RefFunc(FullDecoder* decoder,
                                          uint32_t function_index,
                                          Value* result) {
  if (isolate_ == nullptr) {
    outer_module_->functions[function_index].declared = true;
    return;
  }
  if (!generate_value()) return;
  ValueType type = ValueType::Ref(module_->functions[function_index].sig_index);
  Handle<WasmInternalFunction> internal =
      WasmInstanceObject::GetOrCreateWasmInternalFunction(isolate_, instance_,
                                                          function_index);
  result->runtime_value = WasmValue(internal, type);
}

void ConstantExpressionInterface::GlobalGet(
    FullDecoder* decoder, Value* result,
    const GlobalIndexImmediate<validate>& imm) {
  if (!generate_value()) return;
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

void ConstantExpressionInterface::StructNew(
    FullDecoder* decoder, const StructIndexImmediate<validate>& imm,
    const Value& rtt, const Value args[], Value* result) {
  if (!generate_value()) return;
  std::vector<WasmValue> field_values(imm.struct_type->field_count());
  for (size_t i = 0; i < field_values.size(); i++) {
    field_values[i] = args[i].runtime_value;
  }
  result->runtime_value =
      WasmValue(isolate_->factory()->NewWasmStruct(
                    imm.struct_type, field_values.data(),
                    Handle<Map>::cast(rtt.runtime_value.to_ref())),
                ValueType::Ref(HeapType(imm.index)));
}

void ConstantExpressionInterface::StringConst(
    FullDecoder* decoder, const StringConstImmediate<validate>& imm,
    Value* result) {
  if (!generate_value()) return;
  static_assert(base::IsInRange(kV8MaxWasmStringLiterals, 0, Smi::kMaxValue));

  DCHECK_LT(imm.index, module_->stringref_literals.size());

  const wasm::WasmStringRefLiteral& literal =
      module_->stringref_literals[imm.index];
  const base::Vector<const uint8_t> module_bytes =
      instance_->module_object().native_module()->wire_bytes();
  const base::Vector<const uint8_t> string_bytes =
      module_bytes.SubVector(literal.source.offset(),
                             literal.source.offset() + literal.source.length());
  Handle<String> string =
      isolate_->factory()
          ->NewStringFromUtf8(string_bytes, unibrow::Utf8Variant::kWtf8)
          .ToHandleChecked();
  result->runtime_value = WasmValue(string, kWasmStringRef.AsNonNull());
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
    case kRefNull:
      return WasmValue(isolate->factory()->null_value(), type);
    case kVoid:
    case kRtt:
    case kRef:
    case kBottom:
      UNREACHABLE();
  }
}
}  // namespace

void ConstantExpressionInterface::StructNewDefault(
    FullDecoder* decoder, const StructIndexImmediate<validate>& imm,
    const Value& rtt, Value* result) {
  if (!generate_value()) return;
  std::vector<WasmValue> field_values(imm.struct_type->field_count());
  for (uint32_t i = 0; i < field_values.size(); i++) {
    field_values[i] = DefaultValueForType(imm.struct_type->field(i), isolate_);
  }
  result->runtime_value =
      WasmValue(isolate_->factory()->NewWasmStruct(
                    imm.struct_type, field_values.data(),
                    Handle<Map>::cast(rtt.runtime_value.to_ref())),
                ValueType::Ref(imm.index));
}

void ConstantExpressionInterface::ArrayNew(
    FullDecoder* decoder, const ArrayIndexImmediate<validate>& imm,
    const Value& length, const Value& initial_value, const Value& rtt,
    Value* result) {
  if (!generate_value()) return;
  if (length.runtime_value.to_u32() >
      static_cast<uint32_t>(WasmArray::MaxLength(imm.array_type))) {
    error_ = MessageTemplate::kWasmTrapArrayTooLarge;
    return;
  }
  result->runtime_value =
      WasmValue(isolate_->factory()->NewWasmArray(
                    imm.array_type, length.runtime_value.to_u32(),
                    initial_value.runtime_value,
                    Handle<Map>::cast(rtt.runtime_value.to_ref())),
                ValueType::Ref(imm.index));
}

void ConstantExpressionInterface::ArrayNewDefault(
    FullDecoder* decoder, const ArrayIndexImmediate<validate>& imm,
    const Value& length, const Value& rtt, Value* result) {
  if (!generate_value()) return;
  Value initial_value(decoder->pc(), imm.array_type->element_type());
  initial_value.runtime_value =
      DefaultValueForType(imm.array_type->element_type(), isolate_);
  return ArrayNew(decoder, imm, length, initial_value, rtt, result);
}

void ConstantExpressionInterface::ArrayNewFixed(
    FullDecoder* decoder, const ArrayIndexImmediate<validate>& imm,
    const base::Vector<Value>& elements, const Value& rtt, Value* result) {
  if (!generate_value()) return;
  std::vector<WasmValue> element_values;
  for (Value elem : elements) element_values.push_back(elem.runtime_value);
  result->runtime_value =
      WasmValue(isolate_->factory()->NewWasmArrayFromElements(
                    imm.array_type, element_values,
                    Handle<Map>::cast(rtt.runtime_value.to_ref())),
                ValueType::Ref(HeapType(imm.index)));
}

void ConstantExpressionInterface::ArrayNewSegment(
    FullDecoder* decoder, const ArrayIndexImmediate<validate>& array_imm,
    const IndexImmediate<validate>& segment_imm, const Value& offset_value,
    const Value& length_value, const Value& rtt, Value* result) {
  if (!generate_value()) return;

  uint32_t length = length_value.runtime_value.to_u32();
  uint32_t offset = offset_value.runtime_value.to_u32();
  if (length >
      static_cast<uint32_t>(WasmArray::MaxLength(array_imm.array_type))) {
    error_ = MessageTemplate::kWasmTrapArrayTooLarge;
    return;
  }
  ValueType element_type = array_imm.array_type->element_type();
  ValueType result_type = ValueType::Ref(HeapType(array_imm.index));
  if (element_type.is_numeric()) {
    const WasmDataSegment& data_segment =
        module_->data_segments[segment_imm.index];
    uint32_t length_in_bytes =
        length * array_imm.array_type->element_type().value_kind_size();

    if (!base::IsInBounds<uint32_t>(offset, length_in_bytes,
                                    data_segment.source.length())) {
      error_ = MessageTemplate::kWasmTrapDataSegmentOutOfBounds;
      return;
    }

    Address source =
        instance_->data_segment_starts().get(segment_imm.index) + offset;
    Handle<WasmArray> array_value = isolate_->factory()->NewWasmArrayFromMemory(
        length, Handle<Map>::cast(rtt.runtime_value.to_ref()), source);
    result->runtime_value = WasmValue(array_value, result_type);
  } else {
    const wasm::WasmElemSegment* elem_segment =
        &decoder->module_->elem_segments[segment_imm.index];
    // A constant expression should not observe if a passive segment is dropped.
    // However, it should consider active and declarative segments as empty.
    if (!base::IsInBounds<size_t>(
            offset, length,
            elem_segment->status == WasmElemSegment::kStatusPassive
                ? elem_segment->entries.size()
                : 0)) {
      error_ = MessageTemplate::kWasmTrapElementSegmentOutOfBounds;
      return;
    }

    Handle<Object> array_object =
        isolate_->factory()->NewWasmArrayFromElementSegment(
            instance_, elem_segment, offset, length,
            Handle<Map>::cast(rtt.runtime_value.to_ref()));
    if (array_object->IsSmi()) {
      // A smi result stands for an error code.
      error_ = static_cast<MessageTemplate>(array_object->ToSmi().value());
    } else {
      result->runtime_value = WasmValue(array_object, result_type);
    }
  }
}

void ConstantExpressionInterface::RttCanon(FullDecoder* decoder,
                                           uint32_t type_index, Value* result) {
  if (!generate_value()) return;
  result->runtime_value = WasmValue(
      handle(instance_->managed_object_maps().get(type_index), isolate_),
      ValueType::Rtt(type_index));
}

void ConstantExpressionInterface::I31New(FullDecoder* decoder,
                                         const Value& input, Value* result) {
  if (!generate_value()) return;
  Address raw = static_cast<Address>(input.runtime_value.to_i32());
  // 33 = 1 (Smi tag) + 31 (Smi shift) + 1 (i31ref high-bit truncation).
  Address shifted = raw << (SmiValuesAre31Bits() ? 1 : 33);
  result->runtime_value =
      WasmValue(handle(Smi(shifted), isolate_), wasm::kWasmI31Ref.AsNonNull());
}

void ConstantExpressionInterface::DoReturn(FullDecoder* decoder,
                                           uint32_t /*drop_values*/) {
  end_found_ = true;
  // End decoding on "end".
  decoder->set_end(decoder->pc() + 1);
  if (generate_value()) {
    computed_value_ = decoder->stack_value(1)->runtime_value;
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
