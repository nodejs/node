// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/constant-expression-interface.h"

#include "src/base/overflowing-math.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-engine.h"
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
                                            const Simd128Immediate& imm,
                                            Value* result) {
  if (!generate_value()) return;
  result->runtime_value = WasmValue(imm.value, kWasmS128);
}

void ConstantExpressionInterface::UnOp(FullDecoder* decoder, WasmOpcode opcode,
                                       const Value& input, Value* result) {
  if (!generate_value()) return;
  switch (opcode) {
    case kExprExternConvertAny: {
      result->runtime_value =
          WasmValue(WasmToJSObject(isolate_, input.runtime_value.to_ref()),
                    CanonicalValueType::RefMaybeNull(kWasmExternRef,
                                                     input.type.nullability()));
      break;
    }
    case kExprAnyConvertExtern: {
      const char* error_message = nullptr;
      result->runtime_value =
          WasmValue(JSToWasmObject(isolate_, input.runtime_value.to_ref(),
                                   kWasmAnyRef, &error_message)
                        .ToHandleChecked(),
                    CanonicalValueType::RefMaybeNull(kWasmAnyRef,
                                                     input.type.nullability()));
      break;
    }
    default:
      UNREACHABLE();
  }
}

void ConstantExpressionInterface::BinOp(FullDecoder* decoder, WasmOpcode opcode,
                                        const Value& lhs, const Value& rhs,
                                        Value* result) {
  if (!generate_value()) return;
  switch (opcode) {
    case kExprI32Add:
      result->runtime_value = WasmValue(base::AddWithWraparound(
          lhs.runtime_value.to_i32(), rhs.runtime_value.to_i32()));
      break;
    case kExprI32Sub:
      result->runtime_value = WasmValue(base::SubWithWraparound(
          lhs.runtime_value.to_i32(), rhs.runtime_value.to_i32()));
      break;
    case kExprI32Mul:
      result->runtime_value = WasmValue(base::MulWithWraparound(
          lhs.runtime_value.to_i32(), rhs.runtime_value.to_i32()));
      break;
    case kExprI64Add:
      result->runtime_value = WasmValue(base::AddWithWraparound(
          lhs.runtime_value.to_i64(), rhs.runtime_value.to_i64()));
      break;
    case kExprI64Sub:
      result->runtime_value = WasmValue(base::SubWithWraparound(
          lhs.runtime_value.to_i64(), rhs.runtime_value.to_i64()));
      break;
    case kExprI64Mul:
      result->runtime_value = WasmValue(base::MulWithWraparound(
          lhs.runtime_value.to_i64(), rhs.runtime_value.to_i64()));
      break;
    default:
      UNREACHABLE();
  }
}

void ConstantExpressionInterface::RefNull(FullDecoder* decoder, ValueType type,
                                          Value* result) {
  if (!generate_value()) return;
  result->runtime_value = WasmValue(
      type.use_wasm_null() ? Cast<Object>(isolate_->factory()->wasm_null())
                           : Cast<Object>(isolate_->factory()->null_value()),
      decoder->module_->canonical_type(type));
}

void ConstantExpressionInterface::RefFunc(FullDecoder* decoder,
                                          uint32_t function_index,
                                          Value* result) {
  if (isolate_ == nullptr) {
    outer_module_->functions[function_index].declared = true;
    return;
  }
  if (!generate_value()) return;
  ModuleTypeIndex sig_index = module_->functions[function_index].sig_index;
  bool function_is_shared = module_->type(sig_index).is_shared;
  CanonicalValueType type =
      CanonicalValueType::Ref(module_->canonical_type_id(sig_index),
                              function_is_shared, RefTypeKind::kFunction);
  DirectHandle<WasmFuncRef> func_ref =
      WasmTrustedInstanceData::GetOrCreateFuncRef(
          isolate_,
          function_is_shared ? shared_trusted_instance_data_
                             : trusted_instance_data_,
          function_index);
  result->runtime_value = WasmValue(func_ref, type);
}

void ConstantExpressionInterface::GlobalGet(FullDecoder* decoder, Value* result,
                                            const GlobalIndexImmediate& imm) {
  if (!generate_value()) return;
  const WasmGlobal& global = module_->globals[imm.index];
  DCHECK(!global.mutability);
  DirectHandle<WasmTrustedInstanceData> data =
      global.shared ? shared_trusted_instance_data_ : trusted_instance_data_;
  CanonicalValueType type = module_->canonical_type(global.type);
  result->runtime_value =
      type.is_numeric()
          ? WasmValue(reinterpret_cast<uint8_t*>(
                          data->untagged_globals_buffer()->backing_store()) +
                          global.offset,
                      type)
          : WasmValue(
                direct_handle(data->tagged_globals_buffer()->get(global.offset),
                              isolate_),
                type);
}

DirectHandle<Map> ConstantExpressionInterface::GetRtt(
    DirectHandle<WasmTrustedInstanceData> data, ModuleTypeIndex index,
    const TypeDefinition& type, const Value& descriptor) {
  if (!type.has_descriptor()) {
    return direct_handle(
        Cast<Map>(data->managed_object_maps()->get(index.index)), isolate_);
  }

  DCHECK(type.has_descriptor());
  WasmValue desc = descriptor.runtime_value;
  DCHECK_EQ(desc.type().ref_index(),
            module_->canonical_type_id(type.descriptor));
  DirectHandle<Object> maybe_obj = desc.to_ref();
  if (!IsWasmStruct(*maybe_obj)) {
    DCHECK(IsNull(*maybe_obj));
    error_ = MessageTemplate::kWasmTrapNullDereference;
    return {};
  }
  return direct_handle(Cast<WasmStruct>(*maybe_obj)->get_described_rtt(),
                       isolate_);
}

void ConstantExpressionInterface::StructNew(FullDecoder* decoder,
                                            const StructIndexImmediate& imm,
                                            const Value& descriptor,
                                            const Value args[], Value* result) {
  if (!generate_value()) return;
  DirectHandle<WasmTrustedInstanceData> data =
      GetTrustedInstanceDataForTypeIndex(imm.index);
  const TypeDefinition& type = module_->type(imm.index);
  const StructType* struct_type = type.struct_type;
  DCHECK_EQ(struct_type, imm.struct_type);

  DirectHandle<Map> rtt = GetRtt(data, imm.index, type, descriptor);
  if (rtt.is_null()) return;  // Trap (descriptor was null).

  DirectHandle<WasmStruct> obj;
  if (type.is_descriptor()) {
    obj = WasmStruct::AllocateDescriptorUninitialized(isolate_, data, imm.index,
                                                      rtt);
  } else {
    obj = isolate_->factory()->NewWasmStructUninitialized(struct_type, rtt);
  }
  DisallowGarbageCollection no_gc;  // Must initialize fields first.

  for (uint32_t i = 0; i < struct_type->field_count(); i++) {
    int offset = struct_type->field_offset(i);
    if (struct_type->field(i).is_numeric()) {
      uint8_t* address =
          reinterpret_cast<uint8_t*>(obj->RawFieldAddress(offset));
      args[i].runtime_value.Packed(struct_type->field(i)).CopyTo(address);
    } else {
      TaggedField<Object, WasmStruct::kHeaderSize>::store(
          *obj, offset, *args[i].runtime_value.to_ref());
    }
  }
  result->runtime_value = WasmValue(
      obj, decoder->module_->canonical_type(
               ValueType::Ref(imm.heap_type()).AsExactIfProposalEnabled()));
}

void ConstantExpressionInterface::StringConst(FullDecoder* decoder,
                                              const StringConstImmediate& imm,
                                              Value* result) {
  if (!generate_value()) return;
  static_assert(base::IsInRange(kV8MaxWasmStringLiterals, 0, Smi::kMaxValue));

  DCHECK_LT(imm.index, module_->stringref_literals.size());

  const wasm::WasmStringRefLiteral& literal =
      module_->stringref_literals[imm.index];
  const base::Vector<const uint8_t> module_bytes =
      trusted_instance_data_->native_module()->wire_bytes();
  const base::Vector<const uint8_t> string_bytes = module_bytes.SubVector(
      literal.source.offset(), literal.source.end_offset());
  DirectHandle<String> string =
      isolate_->factory()
          ->NewStringFromUtf8(string_bytes, unibrow::Utf8Variant::kWtf8)
          .ToHandleChecked();
  result->runtime_value = WasmValue(string, kWasmRefString);
}

namespace {
WasmValue DefaultValueForType(ValueType type, Isolate* isolate,
                              const WasmModule* module) {
  switch (type.kind()) {
    case kI32:
    case kI8:
    case kI16:
      return WasmValue(0);
    case kI64:
      return WasmValue(int64_t{0});
    case kF16:
    case kF32:
      return WasmValue(0.0f);
    case kF64:
      return WasmValue(0.0);
    case kS128:
      return WasmValue(Simd128());
    case kRefNull:
      return WasmValue(type.use_wasm_null()
                           ? Cast<Object>(isolate->factory()->wasm_null())
                           : Cast<Object>(isolate->factory()->null_value()),
                       module->canonical_type(type));
    case kVoid:
    case kRef:
    case kTop:
    case kBottom:
      UNREACHABLE();
  }
}
}  // namespace

void ConstantExpressionInterface::StructNewDefault(
    FullDecoder* decoder, const StructIndexImmediate& imm,
    const Value& descriptor, Value* result) {
  if (!generate_value()) return;
  DirectHandle<WasmTrustedInstanceData> data =
      GetTrustedInstanceDataForTypeIndex(imm.index);
  const TypeDefinition& type = module_->type(imm.index);
  const StructType* struct_type = type.struct_type;
  DCHECK_EQ(struct_type, imm.struct_type);

  DirectHandle<Map> rtt = GetRtt(data, imm.index, type, descriptor);
  if (rtt.is_null()) return;  // Trap (descriptor was null).

  DirectHandle<WasmStruct> obj;
  if (type.is_descriptor()) {
    obj = WasmStruct::AllocateDescriptorUninitialized(isolate_, data, imm.index,
                                                      rtt);
  } else {
    obj = isolate_->factory()->NewWasmStructUninitialized(struct_type, rtt);
  }
  DisallowGarbageCollection no_gc;  // Must initialize fields first.

  for (uint32_t i = 0; i < struct_type->field_count(); i++) {
    int offset = struct_type->field_offset(i);
    ValueType ftype = struct_type->field(i);
    if (ftype.is_numeric()) {
      uint8_t* address =
          reinterpret_cast<uint8_t*>(obj->RawFieldAddress(offset));
      DefaultValueForType(ftype, isolate_, module_)
          .Packed(ftype)
          .CopyTo(address);
    } else {
      TaggedField<Object, WasmStruct::kHeaderSize>::store(
          *obj, offset,
          *DefaultValueForType(ftype, isolate_, module_).to_ref());
    }
  }

  result->runtime_value = WasmValue(
      obj, decoder->module_->canonical_type(
               ValueType::Ref(imm.heap_type()).AsExactIfProposalEnabled()));
}

void ConstantExpressionInterface::ArrayNew(FullDecoder* decoder,
                                           const ArrayIndexImmediate& imm,
                                           const Value& length,
                                           const Value& initial_value,
                                           Value* result) {
  if (!generate_value()) return;
  DirectHandle<WasmTrustedInstanceData> data =
      GetTrustedInstanceDataForTypeIndex(imm.index);
  DirectHandle<Map> rtt{
      Cast<Map>(data->managed_object_maps()->get(imm.index.index)), isolate_};
  if (length.runtime_value.to_u32() >
      static_cast<uint32_t>(WasmArray::MaxLength(imm.array_type))) {
    error_ = MessageTemplate::kWasmTrapArrayTooLarge;
    return;
  }
  result->runtime_value = WasmValue(
      isolate_->factory()->NewWasmArray(imm.array_type->element_type(),
                                        length.runtime_value.to_u32(),
                                        initial_value.runtime_value, rtt),
      decoder->module_->canonical_type(ValueType::Ref(imm.heap_type())));
}

void ConstantExpressionInterface::ArrayNewDefault(
    FullDecoder* decoder, const ArrayIndexImmediate& imm, const Value& length,
    Value* result) {
  if (!generate_value()) return;
  Value initial_value(decoder->pc(), imm.array_type->element_type());
  initial_value.runtime_value = DefaultValueForType(
      imm.array_type->element_type(), isolate_, decoder->module_);
  return ArrayNew(decoder, imm, length, initial_value, result);
}

void ConstantExpressionInterface::ArrayNewFixed(
    FullDecoder* decoder, const ArrayIndexImmediate& array_imm,
    const IndexImmediate& length_imm, const Value elements[], Value* result) {
  if (!generate_value()) return;
  DirectHandle<WasmTrustedInstanceData> data =
      GetTrustedInstanceDataForTypeIndex(array_imm.index);
  DirectHandle<Map> rtt{
      Cast<Map>(data->managed_object_maps()->get(array_imm.index.index)),
      isolate_};
  base::Vector<WasmValue> element_values =
      decoder->zone_->AllocateVector<WasmValue>(length_imm.index);
  for (size_t i = 0; i < length_imm.index; i++) {
    element_values[i] = elements[i].runtime_value;
  }
  result->runtime_value = WasmValue(
      isolate_->factory()->NewWasmArrayFromElements(array_imm.array_type,
                                                    element_values, rtt),
      decoder->module_->canonical_type(ValueType::Ref(array_imm.heap_type())));
}

// TODO(14034): These expressions are non-constant for now. There are plans to
// make them constant in the future, so we retain the required infrastructure
// here.
void ConstantExpressionInterface::ArrayNewSegment(
    FullDecoder* decoder, const ArrayIndexImmediate& array_imm,
    const IndexImmediate& segment_imm, const Value& offset_value,
    const Value& length_value, Value* result) {
  if (!generate_value()) return;

  DirectHandle<WasmTrustedInstanceData> data =
      GetTrustedInstanceDataForTypeIndex(array_imm.index);

  DirectHandle<Map> rtt{
      Cast<Map>(data->managed_object_maps()->get(array_imm.index.index)),
      isolate_};
  DCHECK_EQ(rtt->wasm_type_info()->type_index(),
            decoder->module_->canonical_type_id(array_imm.index));

  uint32_t length = length_value.runtime_value.to_u32();
  uint32_t offset = offset_value.runtime_value.to_u32();
  if (length >
      static_cast<uint32_t>(WasmArray::MaxLength(array_imm.array_type))) {
    error_ = MessageTemplate::kWasmTrapArrayTooLarge;
    return;
  }
  CanonicalValueType element_type = rtt->wasm_type_info()->element_type();
  CanonicalValueType result_type = rtt->wasm_type_info()->type();
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
        data->data_segment_starts()->get(segment_imm.index) + offset;
    DirectHandle<WasmArray> array_value =
        isolate_->factory()->NewWasmArrayFromMemory(length, rtt, element_type,
                                                    source);
    result->runtime_value = WasmValue(array_value, result_type);
  } else {
    const wasm::WasmElemSegment* elem_segment =
        &decoder->module_->elem_segments[segment_imm.index];
    // A constant expression should not observe if a passive segment is dropped.
    // However, it should consider active and declarative segments as empty.
    if (!base::IsInBounds<size_t>(
            offset, length,
            elem_segment->status == WasmElemSegment::kStatusPassive
                ? elem_segment->element_count
                : 0)) {
      error_ = MessageTemplate::kWasmTrapElementSegmentOutOfBounds;
      return;
    }

    DirectHandle<Object> array_object =
        isolate_->factory()->NewWasmArrayFromElementSegment(
            trusted_instance_data_, shared_trusted_instance_data_,
            segment_imm.index, offset, length, rtt, element_type);
    if (IsSmi(*array_object)) {
      // A smi result stands for an error code.
      error_ = static_cast<MessageTemplate>(Cast<Smi>(*array_object).value());
    } else {
      result->runtime_value = WasmValue(array_object, result_type);
    }
  }
}

void ConstantExpressionInterface::RefI31(FullDecoder* decoder,
                                         const Value& input, Value* result) {
  if (!generate_value()) return;
  Address raw = input.runtime_value.to_i32();
  // We have to craft the Smi manually because we accept out-of-bounds inputs.
  // For 32-bit Smi builds, set the topmost bit to sign-extend the second bit.
  // This way, interpretation in JS (if this value escapes there) will be the
  // same as i31.get_s.
  static_assert((SmiValuesAre31Bits() ^ SmiValuesAre32Bits()) == 1);
  intptr_t shifted;
  if constexpr (SmiValuesAre31Bits()) {
    shifted = raw << (kSmiTagSize + kSmiShiftSize);
  } else {
    shifted =
        static_cast<intptr_t>(raw << (kSmiTagSize + kSmiShiftSize + 1)) >> 1;
  }
  result->runtime_value =
      WasmValue(direct_handle(Tagged<Smi>(shifted), isolate_), kWasmRefI31);
}

void ConstantExpressionInterface::DoReturn(FullDecoder* decoder,
                                           uint32_t /*drop_values*/) {
  end_found_ = true;
  // End decoding on "end". Note: We need this because we do not know the length
  // of a constant expression while decoding it.
  decoder->set_end(decoder->pc() + 1);
  if (generate_value()) {
    computed_value_ = decoder->stack_value(1)->runtime_value;
  }
}

DirectHandle<WasmTrustedInstanceData>
ConstantExpressionInterface::GetTrustedInstanceDataForTypeIndex(
    ModuleTypeIndex index) {
  bool type_is_shared = module_->type(index).is_shared;
  return type_is_shared ? shared_trusted_instance_data_
                        : trusted_instance_data_;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
