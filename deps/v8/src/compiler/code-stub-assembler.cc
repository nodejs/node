// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-stub-assembler.h"

#include <ostream>

#include "src/code-factory.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/frames.h"
#include "src/interface-descriptors.h"
#include "src/interpreter/bytecodes.h"
#include "src/machine-type.h"
#include "src/macro-assembler.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

CodeStubAssembler::CodeStubAssembler(Isolate* isolate, Zone* zone,
                                     const CallInterfaceDescriptor& descriptor,
                                     Code::Flags flags, const char* name,
                                     size_t result_size)
    : CodeStubAssembler(
          isolate, zone,
          Linkage::GetStubCallDescriptor(
              isolate, zone, descriptor, descriptor.GetStackParameterCount(),
              CallDescriptor::kNoFlags, Operator::kNoProperties,
              MachineType::AnyTagged(), result_size),
          flags, name) {}

CodeStubAssembler::CodeStubAssembler(Isolate* isolate, Zone* zone,
                                     int parameter_count, Code::Flags flags,
                                     const char* name)
    : CodeStubAssembler(isolate, zone, Linkage::GetJSCallDescriptor(
                                           zone, false, parameter_count,
                                           CallDescriptor::kNoFlags),
                        flags, name) {}

CodeStubAssembler::CodeStubAssembler(Isolate* isolate, Zone* zone,
                                     CallDescriptor* call_descriptor,
                                     Code::Flags flags, const char* name)
    : raw_assembler_(new RawMachineAssembler(
          isolate, new (zone) Graph(zone), call_descriptor,
          MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags())),
      flags_(flags),
      name_(name),
      code_generated_(false),
      variables_(zone) {}

CodeStubAssembler::~CodeStubAssembler() {}

void CodeStubAssembler::CallPrologue() {}

void CodeStubAssembler::CallEpilogue() {}

Handle<Code> CodeStubAssembler::GenerateCode() {
  DCHECK(!code_generated_);

  Schedule* schedule = raw_assembler_->Export();
  Handle<Code> code = Pipeline::GenerateCodeForCodeStub(
      isolate(), raw_assembler_->call_descriptor(), graph(), schedule, flags_,
      name_);

  code_generated_ = true;
  return code;
}


Node* CodeStubAssembler::Int32Constant(int value) {
  return raw_assembler_->Int32Constant(value);
}


Node* CodeStubAssembler::IntPtrConstant(intptr_t value) {
  return raw_assembler_->IntPtrConstant(value);
}


Node* CodeStubAssembler::NumberConstant(double value) {
  return raw_assembler_->NumberConstant(value);
}

Node* CodeStubAssembler::SmiConstant(Smi* value) {
  return IntPtrConstant(bit_cast<intptr_t>(value));
}

Node* CodeStubAssembler::HeapConstant(Handle<HeapObject> object) {
  return raw_assembler_->HeapConstant(object);
}


Node* CodeStubAssembler::BooleanConstant(bool value) {
  return raw_assembler_->BooleanConstant(value);
}

Node* CodeStubAssembler::ExternalConstant(ExternalReference address) {
  return raw_assembler_->ExternalConstant(address);
}

Node* CodeStubAssembler::Float64Constant(double value) {
  return raw_assembler_->Float64Constant(value);
}

Node* CodeStubAssembler::BooleanMapConstant() {
  return HeapConstant(isolate()->factory()->boolean_map());
}

Node* CodeStubAssembler::HeapNumberMapConstant() {
  return HeapConstant(isolate()->factory()->heap_number_map());
}

Node* CodeStubAssembler::NullConstant() {
  return LoadRoot(Heap::kNullValueRootIndex);
}

Node* CodeStubAssembler::UndefinedConstant() {
  return LoadRoot(Heap::kUndefinedValueRootIndex);
}

Node* CodeStubAssembler::Parameter(int value) {
  return raw_assembler_->Parameter(value);
}

void CodeStubAssembler::Return(Node* value) {
  return raw_assembler_->Return(value);
}

void CodeStubAssembler::Bind(CodeStubAssembler::Label* label) {
  return label->Bind();
}

Node* CodeStubAssembler::LoadFramePointer() {
  return raw_assembler_->LoadFramePointer();
}

Node* CodeStubAssembler::LoadParentFramePointer() {
  return raw_assembler_->LoadParentFramePointer();
}

Node* CodeStubAssembler::LoadStackPointer() {
  return raw_assembler_->LoadStackPointer();
}

Node* CodeStubAssembler::SmiShiftBitsConstant() {
  return IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

Node* CodeStubAssembler::Float64Round(Node* x) {
  Node* one = Float64Constant(1.0);
  Node* one_half = Float64Constant(0.5);

  Variable var_x(this, MachineRepresentation::kFloat64);
  Label return_x(this);

  // Round up {x} towards Infinity.
  var_x.Bind(Float64Ceil(x));

  GotoIf(Float64LessThanOrEqual(Float64Sub(var_x.value(), one_half), x),
         &return_x);
  var_x.Bind(Float64Sub(var_x.value(), one));
  Goto(&return_x);

  Bind(&return_x);
  return var_x.value();
}

Node* CodeStubAssembler::Float64Ceil(Node* x) {
  if (raw_assembler_->machine()->Float64RoundUp().IsSupported()) {
    return raw_assembler_->Float64RoundUp(x);
  }

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

  Variable var_x(this, MachineRepresentation::kFloat64);
  Label return_x(this), return_minus_x(this);
  var_x.Bind(x);

  // Check if {x} is greater than zero.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  Bind(&if_xgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]0,2^52[.
    GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

    // Round positive {x} towards Infinity.
    var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
    GotoUnless(Float64LessThan(var_x.value(), x), &return_x);
    var_x.Bind(Float64Add(var_x.value(), one));
    Goto(&return_x);
  }

  Bind(&if_xnotgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]-2^52,0[
    GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
    GotoUnless(Float64LessThan(x, zero), &return_x);

    // Round negated {x} towards Infinity and return the result negated.
    Node* minus_x = Float64Neg(x);
    var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
    GotoUnless(Float64GreaterThan(var_x.value(), minus_x), &return_minus_x);
    var_x.Bind(Float64Sub(var_x.value(), one));
    Goto(&return_minus_x);
  }

  Bind(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  Bind(&return_x);
  return var_x.value();
}

Node* CodeStubAssembler::Float64Floor(Node* x) {
  if (raw_assembler_->machine()->Float64RoundDown().IsSupported()) {
    return raw_assembler_->Float64RoundDown(x);
  }

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

  Variable var_x(this, MachineRepresentation::kFloat64);
  Label return_x(this), return_minus_x(this);
  var_x.Bind(x);

  // Check if {x} is greater than zero.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  Bind(&if_xgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]0,2^52[.
    GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

    // Round positive {x} towards -Infinity.
    var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
    GotoUnless(Float64GreaterThan(var_x.value(), x), &return_x);
    var_x.Bind(Float64Sub(var_x.value(), one));
    Goto(&return_x);
  }

  Bind(&if_xnotgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]-2^52,0[
    GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
    GotoUnless(Float64LessThan(x, zero), &return_x);

    // Round negated {x} towards -Infinity and return the result negated.
    Node* minus_x = Float64Neg(x);
    var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
    GotoUnless(Float64LessThan(var_x.value(), minus_x), &return_minus_x);
    var_x.Bind(Float64Add(var_x.value(), one));
    Goto(&return_minus_x);
  }

  Bind(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  Bind(&return_x);
  return var_x.value();
}

Node* CodeStubAssembler::Float64Trunc(Node* x) {
  if (raw_assembler_->machine()->Float64RoundTruncate().IsSupported()) {
    return raw_assembler_->Float64RoundTruncate(x);
  }

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

  Variable var_x(this, MachineRepresentation::kFloat64);
  Label return_x(this), return_minus_x(this);
  var_x.Bind(x);

  // Check if {x} is greater than 0.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  Bind(&if_xgreaterthanzero);
  {
    if (raw_assembler_->machine()->Float64RoundDown().IsSupported()) {
      var_x.Bind(raw_assembler_->Float64RoundDown(x));
    } else {
      // Just return {x} unless it's in the range ]0,2^52[.
      GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

      // Round positive {x} towards -Infinity.
      var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
      GotoUnless(Float64GreaterThan(var_x.value(), x), &return_x);
      var_x.Bind(Float64Sub(var_x.value(), one));
    }
    Goto(&return_x);
  }

  Bind(&if_xnotgreaterthanzero);
  {
    if (raw_assembler_->machine()->Float64RoundUp().IsSupported()) {
      var_x.Bind(raw_assembler_->Float64RoundUp(x));
      Goto(&return_x);
    } else {
      // Just return {x} unless its in the range ]-2^52,0[.
      GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
      GotoUnless(Float64LessThan(x, zero), &return_x);

      // Round negated {x} towards -Infinity and return result negated.
      Node* minus_x = Float64Neg(x);
      var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
      GotoUnless(Float64GreaterThan(var_x.value(), minus_x), &return_minus_x);
      var_x.Bind(Float64Sub(var_x.value(), one));
      Goto(&return_minus_x);
    }
  }

  Bind(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  Bind(&return_x);
  return var_x.value();
}

Node* CodeStubAssembler::SmiTag(Node* value) {
  return raw_assembler_->WordShl(value, SmiShiftBitsConstant());
}

Node* CodeStubAssembler::SmiUntag(Node* value) {
  return raw_assembler_->WordSar(value, SmiShiftBitsConstant());
}

Node* CodeStubAssembler::SmiToWord32(Node* value) {
  Node* result = raw_assembler_->WordSar(value, SmiShiftBitsConstant());
  if (raw_assembler_->machine()->Is64()) {
    result = raw_assembler_->TruncateInt64ToInt32(result);
  }
  return result;
}

Node* CodeStubAssembler::SmiToFloat64(Node* value) {
  return ChangeInt32ToFloat64(SmiUntag(value));
}

Node* CodeStubAssembler::SmiAdd(Node* a, Node* b) { return IntPtrAdd(a, b); }

Node* CodeStubAssembler::SmiAddWithOverflow(Node* a, Node* b) {
  return IntPtrAddWithOverflow(a, b);
}

Node* CodeStubAssembler::SmiSub(Node* a, Node* b) { return IntPtrSub(a, b); }

Node* CodeStubAssembler::SmiSubWithOverflow(Node* a, Node* b) {
  return IntPtrSubWithOverflow(a, b);
}

Node* CodeStubAssembler::SmiEqual(Node* a, Node* b) { return WordEqual(a, b); }

Node* CodeStubAssembler::SmiLessThan(Node* a, Node* b) {
  return IntPtrLessThan(a, b);
}

Node* CodeStubAssembler::SmiLessThanOrEqual(Node* a, Node* b) {
  return IntPtrLessThanOrEqual(a, b);
}

Node* CodeStubAssembler::SmiMin(Node* a, Node* b) {
  // TODO(bmeurer): Consider using Select once available.
  Variable min(this, MachineRepresentation::kTagged);
  Label if_a(this), if_b(this), join(this);
  BranchIfSmiLessThan(a, b, &if_a, &if_b);
  Bind(&if_a);
  min.Bind(a);
  Goto(&join);
  Bind(&if_b);
  min.Bind(b);
  Goto(&join);
  Bind(&join);
  return min.value();
}

#define DEFINE_CODE_STUB_ASSEMBER_BINARY_OP(name)   \
  Node* CodeStubAssembler::name(Node* a, Node* b) { \
    return raw_assembler_->name(a, b);              \
  }
CODE_STUB_ASSEMBLER_BINARY_OP_LIST(DEFINE_CODE_STUB_ASSEMBER_BINARY_OP)
#undef DEFINE_CODE_STUB_ASSEMBER_BINARY_OP

Node* CodeStubAssembler::WordShl(Node* value, int shift) {
  return raw_assembler_->WordShl(value, IntPtrConstant(shift));
}

#define DEFINE_CODE_STUB_ASSEMBER_UNARY_OP(name) \
  Node* CodeStubAssembler::name(Node* a) { return raw_assembler_->name(a); }
CODE_STUB_ASSEMBLER_UNARY_OP_LIST(DEFINE_CODE_STUB_ASSEMBER_UNARY_OP)
#undef DEFINE_CODE_STUB_ASSEMBER_UNARY_OP

Node* CodeStubAssembler::WordIsSmi(Node* a) {
  return WordEqual(raw_assembler_->WordAnd(a, IntPtrConstant(kSmiTagMask)),
                   IntPtrConstant(0));
}

Node* CodeStubAssembler::WordIsPositiveSmi(Node* a) {
  return WordEqual(
      raw_assembler_->WordAnd(a, IntPtrConstant(kSmiTagMask | kSmiSignMask)),
      IntPtrConstant(0));
}

Node* CodeStubAssembler::LoadBufferObject(Node* buffer, int offset,
                                          MachineType rep) {
  return raw_assembler_->Load(rep, buffer, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadObjectField(Node* object, int offset,
                                         MachineType rep) {
  return raw_assembler_->Load(rep, object,
                              IntPtrConstant(offset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadHeapNumberValue(Node* object) {
  return Load(MachineType::Float64(), object,
              IntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag));
}

Node* CodeStubAssembler::StoreHeapNumberValue(Node* object, Node* value) {
  return StoreNoWriteBarrier(
      MachineRepresentation::kFloat64, object,
      IntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag), value);
}

Node* CodeStubAssembler::TruncateHeapNumberValueToWord32(Node* object) {
  Node* value = LoadHeapNumberValue(object);
  return raw_assembler_->TruncateFloat64ToInt32(TruncationMode::kJavaScript,
                                                value);
}

Node* CodeStubAssembler::LoadMapBitField(Node* map) {
  return Load(MachineType::Uint8(), map,
              IntPtrConstant(Map::kBitFieldOffset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadMapBitField2(Node* map) {
  return Load(MachineType::Uint8(), map,
              IntPtrConstant(Map::kBitField2Offset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadMapBitField3(Node* map) {
  return Load(MachineType::Uint32(), map,
              IntPtrConstant(Map::kBitField3Offset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadMapInstanceType(Node* map) {
  return Load(MachineType::Uint8(), map,
              IntPtrConstant(Map::kInstanceTypeOffset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadMapDescriptors(Node* map) {
  return LoadObjectField(map, Map::kDescriptorsOffset);
}

Node* CodeStubAssembler::LoadNameHash(Node* name) {
  return Load(MachineType::Uint32(), name,
              IntPtrConstant(Name::kHashFieldOffset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadFixedArrayElementInt32Index(
    Node* object, Node* int32_index, int additional_offset) {
  Node* header_size = IntPtrConstant(additional_offset +
                                     FixedArray::kHeaderSize - kHeapObjectTag);
  Node* scaled_index = WordShl(int32_index, IntPtrConstant(kPointerSizeLog2));
  Node* offset = IntPtrAdd(scaled_index, header_size);
  return Load(MachineType::AnyTagged(), object, offset);
}

Node* CodeStubAssembler::LoadFixedArrayElementSmiIndex(Node* object,
                                                       Node* smi_index,
                                                       int additional_offset) {
  int const kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
  Node* header_size = IntPtrConstant(additional_offset +
                                     FixedArray::kHeaderSize - kHeapObjectTag);
  Node* scaled_index =
      (kSmiShiftBits > kPointerSizeLog2)
          ? WordSar(smi_index, IntPtrConstant(kSmiShiftBits - kPointerSizeLog2))
          : WordShl(smi_index,
                    IntPtrConstant(kPointerSizeLog2 - kSmiShiftBits));
  Node* offset = IntPtrAdd(scaled_index, header_size);
  return Load(MachineType::AnyTagged(), object, offset);
}

Node* CodeStubAssembler::LoadFixedArrayElementConstantIndex(Node* object,
                                                            int index) {
  Node* offset = IntPtrConstant(FixedArray::kHeaderSize - kHeapObjectTag +
                                index * kPointerSize);
  return raw_assembler_->Load(MachineType::AnyTagged(), object, offset);
}

Node* CodeStubAssembler::StoreFixedArrayElementNoWriteBarrier(Node* object,
                                                              Node* index,
                                                              Node* value) {
  Node* offset =
      IntPtrAdd(WordShl(index, IntPtrConstant(kPointerSizeLog2)),
                IntPtrConstant(FixedArray::kHeaderSize - kHeapObjectTag));
  return StoreNoWriteBarrier(MachineRepresentation::kTagged, object, offset,
                             value);
}

Node* CodeStubAssembler::LoadRoot(Heap::RootListIndex root_index) {
  if (isolate()->heap()->RootCanBeTreatedAsConstant(root_index)) {
    Handle<Object> root = isolate()->heap()->root_handle(root_index);
    if (root->IsSmi()) {
      return SmiConstant(Smi::cast(*root));
    } else {
      return HeapConstant(Handle<HeapObject>::cast(root));
    }
  }

  compiler::Node* roots_array_start =
      ExternalConstant(ExternalReference::roots_array_start(isolate()));
  USE(roots_array_start);

  // TODO(danno): Implement thee root-access case where the root is not constant
  // and must be loaded from the root array.
  UNIMPLEMENTED();
  return nullptr;
}

Node* CodeStubAssembler::AllocateRawUnaligned(Node* size_in_bytes,
                                              AllocationFlags flags,
                                              Node* top_address,
                                              Node* limit_address) {
  Node* top = Load(MachineType::Pointer(), top_address);
  Node* limit = Load(MachineType::Pointer(), limit_address);

  // If there's not enough space, call the runtime.
  RawMachineLabel runtime_call(RawMachineLabel::kDeferred), no_runtime_call,
      merge_runtime;
  raw_assembler_->Branch(
      raw_assembler_->IntPtrLessThan(IntPtrSub(limit, top), size_in_bytes),
      &runtime_call, &no_runtime_call);

  raw_assembler_->Bind(&runtime_call);
  // AllocateInTargetSpace does not use the context.
  Node* context = IntPtrConstant(0);
  Node* runtime_flags = SmiTag(Int32Constant(
      AllocateDoubleAlignFlag::encode(false) |
      AllocateTargetSpace::encode(flags & kPretenured
                                      ? AllocationSpace::OLD_SPACE
                                      : AllocationSpace::NEW_SPACE)));
  Node* runtime_result = CallRuntime(Runtime::kAllocateInTargetSpace, context,
                                     SmiTag(size_in_bytes), runtime_flags);
  raw_assembler_->Goto(&merge_runtime);

  // When there is enough space, return `top' and bump it up.
  raw_assembler_->Bind(&no_runtime_call);
  Node* no_runtime_result = top;
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), top_address,
                      IntPtrAdd(top, size_in_bytes));
  no_runtime_result =
      IntPtrAdd(no_runtime_result, IntPtrConstant(kHeapObjectTag));
  raw_assembler_->Goto(&merge_runtime);

  raw_assembler_->Bind(&merge_runtime);
  return raw_assembler_->Phi(MachineType::PointerRepresentation(),
                             runtime_result, no_runtime_result);
}

Node* CodeStubAssembler::AllocateRawAligned(Node* size_in_bytes,
                                            AllocationFlags flags,
                                            Node* top_address,
                                            Node* limit_address) {
  Node* top = Load(MachineType::Pointer(), top_address);
  Node* limit = Load(MachineType::Pointer(), limit_address);
  Node* adjusted_size = size_in_bytes;
  if (flags & kDoubleAlignment) {
    // TODO(epertoso): Simd128 alignment.
    RawMachineLabel aligned, not_aligned, merge;
    raw_assembler_->Branch(WordAnd(top, IntPtrConstant(kDoubleAlignmentMask)),
                           &not_aligned, &aligned);

    raw_assembler_->Bind(&not_aligned);
    Node* not_aligned_size =
        IntPtrAdd(size_in_bytes, IntPtrConstant(kPointerSize));
    raw_assembler_->Goto(&merge);

    raw_assembler_->Bind(&aligned);
    raw_assembler_->Goto(&merge);

    raw_assembler_->Bind(&merge);
    adjusted_size = raw_assembler_->Phi(MachineType::PointerRepresentation(),
                                        not_aligned_size, adjusted_size);
  }

  Node* address = AllocateRawUnaligned(adjusted_size, kNone, top, limit);

  RawMachineLabel needs_filler, doesnt_need_filler, merge_address;
  raw_assembler_->Branch(
      raw_assembler_->IntPtrEqual(adjusted_size, size_in_bytes),
      &doesnt_need_filler, &needs_filler);

  raw_assembler_->Bind(&needs_filler);
  // Store a filler and increase the address by kPointerSize.
  // TODO(epertoso): this code assumes that we only align to kDoubleSize. Change
  // it when Simd128 alignment is supported.
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), top,
                      LoadRoot(Heap::kOnePointerFillerMapRootIndex));
  Node* address_with_filler = IntPtrAdd(address, IntPtrConstant(kPointerSize));
  raw_assembler_->Goto(&merge_address);

  raw_assembler_->Bind(&doesnt_need_filler);
  Node* address_without_filler = address;
  raw_assembler_->Goto(&merge_address);

  raw_assembler_->Bind(&merge_address);
  address = raw_assembler_->Phi(MachineType::PointerRepresentation(),
                                address_with_filler, address_without_filler);
  // Update the top.
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), top_address,
                      IntPtrAdd(top, adjusted_size));
  return address;
}

Node* CodeStubAssembler::Allocate(int size_in_bytes, AllocationFlags flags) {
  bool const new_space = !(flags & kPretenured);
  Node* top_address = ExternalConstant(
      new_space
          ? ExternalReference::new_space_allocation_top_address(isolate())
          : ExternalReference::old_space_allocation_top_address(isolate()));
  Node* limit_address = ExternalConstant(
      new_space
          ? ExternalReference::new_space_allocation_limit_address(isolate())
          : ExternalReference::old_space_allocation_limit_address(isolate()));

#ifdef V8_HOST_ARCH_32_BIT
  if (flags & kDoubleAlignment) {
    return AllocateRawAligned(IntPtrConstant(size_in_bytes), flags, top_address,
                              limit_address);
  }
#endif

  return AllocateRawUnaligned(IntPtrConstant(size_in_bytes), flags, top_address,
                              limit_address);
}

Node* CodeStubAssembler::AllocateHeapNumber() {
  Node* result = Allocate(HeapNumber::kSize, kNone);
  StoreMapNoWriteBarrier(result, HeapNumberMapConstant());
  return result;
}

Node* CodeStubAssembler::AllocateHeapNumberWithValue(Node* value) {
  Node* result = AllocateHeapNumber();
  StoreHeapNumberValue(result, value);
  return result;
}

Node* CodeStubAssembler::Load(MachineType rep, Node* base) {
  return raw_assembler_->Load(rep, base);
}

Node* CodeStubAssembler::Load(MachineType rep, Node* base, Node* index) {
  return raw_assembler_->Load(rep, base, index);
}

Node* CodeStubAssembler::Store(MachineRepresentation rep, Node* base,
                               Node* value) {
  return raw_assembler_->Store(rep, base, value, kFullWriteBarrier);
}

Node* CodeStubAssembler::Store(MachineRepresentation rep, Node* base,
                               Node* index, Node* value) {
  return raw_assembler_->Store(rep, base, index, value, kFullWriteBarrier);
}

Node* CodeStubAssembler::StoreNoWriteBarrier(MachineRepresentation rep,
                                             Node* base, Node* value) {
  return raw_assembler_->Store(rep, base, value, kNoWriteBarrier);
}

Node* CodeStubAssembler::StoreNoWriteBarrier(MachineRepresentation rep,
                                             Node* base, Node* index,
                                             Node* value) {
  return raw_assembler_->Store(rep, base, index, value, kNoWriteBarrier);
}

Node* CodeStubAssembler::Projection(int index, Node* value) {
  return raw_assembler_->Projection(index, value);
}

Node* CodeStubAssembler::LoadMap(Node* object) {
  return LoadObjectField(object, HeapObject::kMapOffset);
}

Node* CodeStubAssembler::StoreMapNoWriteBarrier(Node* object, Node* map) {
  return StoreNoWriteBarrier(
      MachineRepresentation::kTagged, object,
      IntPtrConstant(HeapNumber::kMapOffset - kHeapObjectTag), map);
}

Node* CodeStubAssembler::LoadInstanceType(Node* object) {
  return LoadMapInstanceType(LoadMap(object));
}

Node* CodeStubAssembler::LoadElements(Node* object) {
  return LoadObjectField(object, JSObject::kElementsOffset);
}

Node* CodeStubAssembler::LoadFixedArrayBaseLength(Node* array) {
  return LoadObjectField(array, FixedArrayBase::kLengthOffset);
}

Node* CodeStubAssembler::BitFieldDecode(Node* word32, uint32_t shift,
                                        uint32_t mask) {
  return raw_assembler_->Word32Shr(
      raw_assembler_->Word32And(word32, raw_assembler_->Int32Constant(mask)),
      raw_assembler_->Int32Constant(shift));
}

Node* CodeStubAssembler::ChangeFloat64ToTagged(Node* value) {
  Node* value32 = raw_assembler_->TruncateFloat64ToInt32(
      TruncationMode::kRoundToZero, value);
  Node* value64 = ChangeInt32ToFloat64(value32);

  Label if_valueisint32(this), if_valueisheapnumber(this), if_join(this);

  Label if_valueisequal(this), if_valueisnotequal(this);
  Branch(Float64Equal(value, value64), &if_valueisequal, &if_valueisnotequal);
  Bind(&if_valueisequal);
  {
    Label if_valueiszero(this), if_valueisnotzero(this);
    Branch(Float64Equal(value, Float64Constant(0.0)), &if_valueiszero,
           &if_valueisnotzero);

    Bind(&if_valueiszero);
    BranchIfInt32LessThan(raw_assembler_->Float64ExtractHighWord32(value),
                          Int32Constant(0), &if_valueisheapnumber,
                          &if_valueisint32);

    Bind(&if_valueisnotzero);
    Goto(&if_valueisint32);
  }
  Bind(&if_valueisnotequal);
  Goto(&if_valueisheapnumber);

  Variable var_result(this, MachineRepresentation::kTagged);
  Bind(&if_valueisint32);
  {
    if (raw_assembler_->machine()->Is64()) {
      Node* result = SmiTag(ChangeInt32ToInt64(value32));
      var_result.Bind(result);
      Goto(&if_join);
    } else {
      Node* pair = Int32AddWithOverflow(value32, value32);
      Node* overflow = Projection(1, pair);
      Label if_overflow(this, Label::kDeferred), if_notoverflow(this);
      Branch(overflow, &if_overflow, &if_notoverflow);
      Bind(&if_overflow);
      Goto(&if_valueisheapnumber);
      Bind(&if_notoverflow);
      {
        Node* result = Projection(0, pair);
        var_result.Bind(result);
        Goto(&if_join);
      }
    }
  }
  Bind(&if_valueisheapnumber);
  {
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&if_join);
  }
  Bind(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::ChangeInt32ToTagged(Node* value) {
  if (raw_assembler_->machine()->Is64()) {
    return SmiTag(ChangeInt32ToInt64(value));
  }
  Variable var_result(this, MachineRepresentation::kTagged);
  Node* pair = Int32AddWithOverflow(value, value);
  Node* overflow = Projection(1, pair);
  Label if_overflow(this, Label::kDeferred), if_notoverflow(this),
      if_join(this);
  Branch(overflow, &if_overflow, &if_notoverflow);
  Bind(&if_overflow);
  {
    Node* value64 = ChangeInt32ToFloat64(value);
    Node* result = AllocateHeapNumberWithValue(value64);
    var_result.Bind(result);
  }
  Goto(&if_join);
  Bind(&if_notoverflow);
  {
    Node* result = Projection(0, pair);
    var_result.Bind(result);
  }
  Goto(&if_join);
  Bind(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::TruncateTaggedToFloat64(Node* context, Node* value) {
  // We might need to loop once due to ToNumber conversion.
  Variable var_value(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kFloat64);
  Label loop(this, &var_value), done_loop(this, &var_result);
  var_value.Bind(value);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {value}.
    value = var_value.value();

    // Check if the {value} is a Smi or a HeapObject.
    Label if_valueissmi(this), if_valueisnotsmi(this);
    Branch(WordIsSmi(value), &if_valueissmi, &if_valueisnotsmi);

    Bind(&if_valueissmi);
    {
      // Convert the Smi {value}.
      var_result.Bind(SmiToFloat64(value));
      Goto(&done_loop);
    }

    Bind(&if_valueisnotsmi);
    {
      // Check if {value} is a HeapNumber.
      Label if_valueisheapnumber(this),
          if_valueisnotheapnumber(this, Label::kDeferred);
      Branch(WordEqual(LoadMap(value), HeapNumberMapConstant()),
             &if_valueisheapnumber, &if_valueisnotheapnumber);

      Bind(&if_valueisheapnumber);
      {
        // Load the floating point value.
        var_result.Bind(LoadHeapNumberValue(value));
        Goto(&done_loop);
      }

      Bind(&if_valueisnotheapnumber);
      {
        // Convert the {value} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_value.Bind(CallStub(callable, context, value));
        Goto(&loop);
      }
    }
  }
  Bind(&done_loop);
  return var_result.value();
}

Node* CodeStubAssembler::TruncateTaggedToWord32(Node* context, Node* value) {
  // We might need to loop once due to ToNumber conversion.
  Variable var_value(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kWord32);
  Label loop(this, &var_value), done_loop(this, &var_result);
  var_value.Bind(value);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {value}.
    value = var_value.value();

    // Check if the {value} is a Smi or a HeapObject.
    Label if_valueissmi(this), if_valueisnotsmi(this);
    Branch(WordIsSmi(value), &if_valueissmi, &if_valueisnotsmi);

    Bind(&if_valueissmi);
    {
      // Convert the Smi {value}.
      var_result.Bind(SmiToWord32(value));
      Goto(&done_loop);
    }

    Bind(&if_valueisnotsmi);
    {
      // Check if {value} is a HeapNumber.
      Label if_valueisheapnumber(this),
          if_valueisnotheapnumber(this, Label::kDeferred);
      Branch(WordEqual(LoadMap(value), HeapNumberMapConstant()),
             &if_valueisheapnumber, &if_valueisnotheapnumber);

      Bind(&if_valueisheapnumber);
      {
        // Truncate the floating point value.
        var_result.Bind(TruncateHeapNumberValueToWord32(value));
        Goto(&done_loop);
      }

      Bind(&if_valueisnotheapnumber);
      {
        // Convert the {value} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_value.Bind(CallStub(callable, context, value));
        Goto(&loop);
      }
    }
  }
  Bind(&done_loop);
  return var_result.value();
}

void CodeStubAssembler::BranchIf(Node* condition, Label* if_true,
                                 Label* if_false) {
  Label if_condition_is_true(this), if_condition_is_false(this);
  Branch(condition, &if_condition_is_true, &if_condition_is_false);
  Bind(&if_condition_is_true);
  Goto(if_true);
  Bind(&if_condition_is_false);
  Goto(if_false);
}

Node* CodeStubAssembler::CallN(CallDescriptor* descriptor, Node* code_target,
                               Node** args) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallN(descriptor, code_target, args);
  CallEpilogue();
  return return_value;
}


Node* CodeStubAssembler::TailCallN(CallDescriptor* descriptor,
                                   Node* code_target, Node** args) {
  return raw_assembler_->TailCallN(descriptor, code_target, args);
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime0(function_id, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime1(function_id, arg1, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2) {
  CallPrologue();
  Node* return_value =
      raw_assembler_->CallRuntime2(function_id, arg1, arg2, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2,
                                     Node* arg3) {
  CallPrologue();
  Node* return_value =
      raw_assembler_->CallRuntime3(function_id, arg1, arg2, arg3, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2,
                                     Node* arg3, Node* arg4) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime4(function_id, arg1, arg2,
                                                    arg3, arg4, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context) {
  return raw_assembler_->TailCallRuntime0(function_id, context);
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1) {
  return raw_assembler_->TailCallRuntime1(function_id, arg1, context);
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1,
                                         Node* arg2) {
  return raw_assembler_->TailCallRuntime2(function_id, arg1, arg2, context);
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1, Node* arg2,
                                         Node* arg3) {
  return raw_assembler_->TailCallRuntime3(function_id, arg1, arg2, arg3,
                                          context);
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1, Node* arg2,
                                         Node* arg3, Node* arg4) {
  return raw_assembler_->TailCallRuntime4(function_id, arg1, arg2, arg3, arg4,
                                          context);
}

Node* CodeStubAssembler::CallStub(Callable const& callable, Node* context,
                                  Node* arg1, size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return CallStub(callable.descriptor(), target, context, arg1, result_size);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(2);
  args[0] = arg1;
  args[1] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(3);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, Node* arg3, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(4);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, Node* arg3, Node* arg4,
                                  size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(5);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = arg4;
  args[4] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, Node* arg3, Node* arg4,
                                  Node* arg5, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(6);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = arg4;
  args[4] = arg5;
  args[5] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::TailCallStub(Callable const& callable, Node* context,
                                      Node* arg1, Node* arg2,
                                      size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return TailCallStub(callable.descriptor(), target, context, arg1, arg2,
                      result_size);
}

Node* CodeStubAssembler::TailCallStub(const CallInterfaceDescriptor& descriptor,
                                      Node* target, Node* context, Node* arg1,
                                      Node* arg2, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(3);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = context;

  return raw_assembler_->TailCallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::TailCall(
    const CallInterfaceDescriptor& interface_descriptor, Node* code_target,
    Node** args, size_t result_size) {
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), interface_descriptor,
      interface_descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);
  return raw_assembler_->TailCallN(descriptor, code_target, args);
}

void CodeStubAssembler::Goto(CodeStubAssembler::Label* label) {
  label->MergeVariables();
  raw_assembler_->Goto(label->label_);
}

void CodeStubAssembler::GotoIf(Node* condition, Label* true_label) {
  Label false_label(this);
  Branch(condition, true_label, &false_label);
  Bind(&false_label);
}

void CodeStubAssembler::GotoUnless(Node* condition, Label* false_label) {
  Label true_label(this);
  Branch(condition, &true_label, false_label);
  Bind(&true_label);
}

void CodeStubAssembler::Branch(Node* condition,
                               CodeStubAssembler::Label* true_label,
                               CodeStubAssembler::Label* false_label) {
  true_label->MergeVariables();
  false_label->MergeVariables();
  return raw_assembler_->Branch(condition, true_label->label_,
                                false_label->label_);
}

void CodeStubAssembler::Switch(Node* index, Label* default_label,
                               int32_t* case_values, Label** case_labels,
                               size_t case_count) {
  RawMachineLabel** labels =
      new (zone()->New(sizeof(RawMachineLabel*) * case_count))
          RawMachineLabel*[case_count];
  for (size_t i = 0; i < case_count; ++i) {
    labels[i] = case_labels[i]->label_;
    case_labels[i]->MergeVariables();
    default_label->MergeVariables();
  }
  return raw_assembler_->Switch(index, default_label->label_, case_values,
                                labels, case_count);
}

// RawMachineAssembler delegate helpers:
Isolate* CodeStubAssembler::isolate() const {
  return raw_assembler_->isolate();
}

Factory* CodeStubAssembler::factory() const { return isolate()->factory(); }

Graph* CodeStubAssembler::graph() const { return raw_assembler_->graph(); }

Zone* CodeStubAssembler::zone() const { return raw_assembler_->zone(); }

// The core implementation of Variable is stored through an indirection so
// that it can outlive the often block-scoped Variable declarations. This is
// needed to ensure that variable binding and merging through phis can
// properly be verified.
class CodeStubAssembler::Variable::Impl : public ZoneObject {
 public:
  explicit Impl(MachineRepresentation rep) : value_(nullptr), rep_(rep) {}
  Node* value_;
  MachineRepresentation rep_;
};

CodeStubAssembler::Variable::Variable(CodeStubAssembler* assembler,
                                      MachineRepresentation rep)
    : impl_(new (assembler->zone()) Impl(rep)) {
  assembler->variables_.push_back(impl_);
}

void CodeStubAssembler::Variable::Bind(Node* value) { impl_->value_ = value; }

Node* CodeStubAssembler::Variable::value() const {
  DCHECK_NOT_NULL(impl_->value_);
  return impl_->value_;
}

MachineRepresentation CodeStubAssembler::Variable::rep() const {
  return impl_->rep_;
}

bool CodeStubAssembler::Variable::IsBound() const {
  return impl_->value_ != nullptr;
}

CodeStubAssembler::Label::Label(CodeStubAssembler* assembler,
                                int merged_value_count,
                                CodeStubAssembler::Variable** merged_variables,
                                CodeStubAssembler::Label::Type type)
    : bound_(false), merge_count_(0), assembler_(assembler), label_(nullptr) {
  void* buffer = assembler->zone()->New(sizeof(RawMachineLabel));
  label_ = new (buffer)
      RawMachineLabel(type == kDeferred ? RawMachineLabel::kDeferred
                                        : RawMachineLabel::kNonDeferred);
  for (int i = 0; i < merged_value_count; ++i) {
    variable_phis_[merged_variables[i]->impl_] = nullptr;
  }
}

void CodeStubAssembler::Label::MergeVariables() {
  ++merge_count_;
  for (auto var : assembler_->variables_) {
    size_t count = 0;
    Node* node = var->value_;
    if (node != nullptr) {
      auto i = variable_merges_.find(var);
      if (i != variable_merges_.end()) {
        i->second.push_back(node);
        count = i->second.size();
      } else {
        count = 1;
        variable_merges_[var] = std::vector<Node*>(1, node);
      }
    }
    // If the following asserts, then you've jumped to a label without a bound
    // variable along that path that expects to merge its value into a phi.
    DCHECK(variable_phis_.find(var) == variable_phis_.end() ||
           count == merge_count_);
    USE(count);

    // If the label is already bound, we already know the set of variables to
    // merge and phi nodes have already been created.
    if (bound_) {
      auto phi = variable_phis_.find(var);
      if (phi != variable_phis_.end()) {
        DCHECK_NOT_NULL(phi->second);
        assembler_->raw_assembler_->AppendPhiInput(phi->second, node);
      } else {
        auto i = variable_merges_.find(var);
        if (i != variable_merges_.end()) {
          // If the following assert fires, then you've declared a variable that
          // has the same bound value along all paths up until the point you
          // bound this label, but then later merged a path with a new value for
          // the variable after the label bind (it's not possible to add phis to
          // the bound label after the fact, just make sure to list the variable
          // in the label's constructor's list of merged variables).
          DCHECK(find_if(i->second.begin(), i->second.end(),
                         [node](Node* e) -> bool { return node != e; }) ==
                 i->second.end());
        }
      }
    }
  }
}

void CodeStubAssembler::Label::Bind() {
  DCHECK(!bound_);
  assembler_->raw_assembler_->Bind(label_);

  // Make sure that all variables that have changed along any path up to this
  // point are marked as merge variables.
  for (auto var : assembler_->variables_) {
    Node* shared_value = nullptr;
    auto i = variable_merges_.find(var);
    if (i != variable_merges_.end()) {
      for (auto value : i->second) {
        DCHECK(value != nullptr);
        if (value != shared_value) {
          if (shared_value == nullptr) {
            shared_value = value;
          } else {
            variable_phis_[var] = nullptr;
          }
        }
      }
    }
  }

  for (auto var : variable_phis_) {
    CodeStubAssembler::Variable::Impl* var_impl = var.first;
    auto i = variable_merges_.find(var_impl);
    // If the following assert fires, then a variable that has been marked as
    // being merged at the label--either by explicitly marking it so in the
    // label constructor or by having seen different bound values at branches
    // into the label--doesn't have a bound value along all of the paths that
    // have been merged into the label up to this point.
    DCHECK(i != variable_merges_.end() && i->second.size() == merge_count_);
    Node* phi = assembler_->raw_assembler_->Phi(
        var.first->rep_, static_cast<int>(merge_count_), &(i->second[0]));
    variable_phis_[var_impl] = phi;
  }

  // Bind all variables to a merge phi, the common value along all paths or
  // null.
  for (auto var : assembler_->variables_) {
    auto i = variable_phis_.find(var);
    if (i != variable_phis_.end()) {
      var->value_ = i->second;
    } else {
      auto j = variable_merges_.find(var);
      if (j != variable_merges_.end() && j->second.size() == merge_count_) {
        var->value_ = j->second.back();
      } else {
        var->value_ = nullptr;
      }
    }
  }

  bound_ = true;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
