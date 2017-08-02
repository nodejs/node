// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/type-info.h"

#include "src/assembler-inl.h"
#include "src/ast/ast.h"
#include "src/code-stubs.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/objects-inl.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

TypeFeedbackOracle::TypeFeedbackOracle(Isolate* isolate, Zone* zone,
                                       Handle<Code> code,
                                       Handle<FeedbackVector> feedback_vector,
                                       Handle<Context> native_context)
    : native_context_(native_context), isolate_(isolate), zone_(zone) {
  BuildDictionary(code);
  DCHECK(dictionary_->IsUnseededNumberDictionary());
  // We make a copy of the feedback vector because a GC could clear
  // the type feedback info contained therein.
  // TODO(mvstanton): revisit the decision to copy when we weakly
  // traverse the feedback vector at GC time.
  feedback_vector_ = FeedbackVector::Copy(isolate, feedback_vector);
}


static uint32_t IdToKey(TypeFeedbackId ast_id) {
  return static_cast<uint32_t>(ast_id.ToInt());
}


Handle<Object> TypeFeedbackOracle::GetInfo(TypeFeedbackId ast_id) {
  int entry = dictionary_->FindEntry(IdToKey(ast_id));
  if (entry != UnseededNumberDictionary::kNotFound) {
    Object* value = dictionary_->ValueAt(entry);
    if (value->IsCell()) {
      Cell* cell = Cell::cast(value);
      return Handle<Object>(cell->value(), isolate());
    } else {
      return Handle<Object>(value, isolate());
    }
  }
  return Handle<Object>::cast(isolate()->factory()->undefined_value());
}

Handle<Object> TypeFeedbackOracle::GetInfo(FeedbackSlot slot) {
  DCHECK(slot.ToInt() >= 0 && slot.ToInt() < feedback_vector_->length());
  Handle<Object> undefined =
      Handle<Object>::cast(isolate()->factory()->undefined_value());
  Object* obj = feedback_vector_->Get(slot);

  // Slots do not embed direct pointers to maps, functions. Instead
  // a WeakCell is always used.
  if (obj->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(obj);
    if (cell->cleared()) return undefined;
    obj = cell->value();
  }

  if (obj->IsJSFunction() || obj->IsAllocationSite() || obj->IsSymbol()) {
    return Handle<Object>(obj, isolate());
  }

  return undefined;
}

InlineCacheState TypeFeedbackOracle::LoadInlineCacheState(FeedbackSlot slot) {
  if (!slot.IsInvalid()) {
    FeedbackSlotKind kind = feedback_vector_->GetKind(slot);
    if (IsLoadICKind(kind)) {
      LoadICNexus nexus(feedback_vector_, slot);
      return nexus.StateFromFeedback();
    } else if (IsKeyedLoadICKind(kind)) {
      KeyedLoadICNexus nexus(feedback_vector_, slot);
      return nexus.StateFromFeedback();
    }
  }

  // If we can't find an IC, assume we've seen *something*, but we don't know
  // what. PREMONOMORPHIC roughly encodes this meaning.
  return PREMONOMORPHIC;
}

bool TypeFeedbackOracle::StoreIsUninitialized(FeedbackSlot slot) {
  if (!slot.IsInvalid()) {
    FeedbackSlotKind kind = feedback_vector_->GetKind(slot);
    if (IsStoreICKind(kind) || IsStoreGlobalICKind(kind)) {
      StoreICNexus nexus(feedback_vector_, slot);
      return nexus.StateFromFeedback() == UNINITIALIZED;
    } else if (IsKeyedStoreICKind(kind)) {
      KeyedStoreICNexus nexus(feedback_vector_, slot);
      return nexus.StateFromFeedback() == UNINITIALIZED;
    }
  }
  return true;
}

bool TypeFeedbackOracle::CallIsUninitialized(FeedbackSlot slot) {
  Handle<Object> value = GetInfo(slot);
  return value->IsUndefined(isolate()) ||
         value.is_identical_to(
             FeedbackVector::UninitializedSentinel(isolate()));
}

bool TypeFeedbackOracle::CallIsMonomorphic(FeedbackSlot slot) {
  Handle<Object> value = GetInfo(slot);
  return value->IsAllocationSite() || value->IsJSFunction();
}

bool TypeFeedbackOracle::CallNewIsMonomorphic(FeedbackSlot slot) {
  Handle<Object> info = GetInfo(slot);
  return info->IsAllocationSite() || info->IsJSFunction();
}

byte TypeFeedbackOracle::ForInType(FeedbackSlot feedback_vector_slot) {
  Handle<Object> value = GetInfo(feedback_vector_slot);
  return value.is_identical_to(FeedbackVector::UninitializedSentinel(isolate()))
             ? ForInStatement::FAST_FOR_IN
             : ForInStatement::SLOW_FOR_IN;
}

void TypeFeedbackOracle::GetStoreModeAndKeyType(
    FeedbackSlot slot, KeyedAccessStoreMode* store_mode,
    IcCheckType* key_type) {
  if (!slot.IsInvalid() && feedback_vector_->IsKeyedStoreIC(slot)) {
    KeyedStoreICNexus nexus(feedback_vector_, slot);
    *store_mode = nexus.GetKeyedAccessStoreMode();
    *key_type = nexus.GetKeyType();
  } else {
    *store_mode = STANDARD_STORE;
    *key_type = ELEMENT;
  }
}

Handle<JSFunction> TypeFeedbackOracle::GetCallTarget(FeedbackSlot slot) {
  Handle<Object> info = GetInfo(slot);
  if (info->IsAllocationSite()) {
    return Handle<JSFunction>(isolate()->native_context()->array_function());
  }

  return Handle<JSFunction>::cast(info);
}

Handle<JSFunction> TypeFeedbackOracle::GetCallNewTarget(FeedbackSlot slot) {
  Handle<Object> info = GetInfo(slot);
  if (info->IsJSFunction()) {
    return Handle<JSFunction>::cast(info);
  }

  DCHECK(info->IsAllocationSite());
  return Handle<JSFunction>(isolate()->native_context()->array_function());
}

Handle<AllocationSite> TypeFeedbackOracle::GetCallAllocationSite(
    FeedbackSlot slot) {
  Handle<Object> info = GetInfo(slot);
  if (info->IsAllocationSite()) {
    return Handle<AllocationSite>::cast(info);
  }
  return Handle<AllocationSite>::null();
}

Handle<AllocationSite> TypeFeedbackOracle::GetCallNewAllocationSite(
    FeedbackSlot slot) {
  Handle<Object> info = GetInfo(slot);
  if (info->IsAllocationSite()) {
    return Handle<AllocationSite>::cast(info);
  }
  return Handle<AllocationSite>::null();
}

namespace {

AstType* CompareOpHintToType(CompareOperationHint hint) {
  switch (hint) {
    case CompareOperationHint::kNone:
      return AstType::None();
    case CompareOperationHint::kSignedSmall:
      return AstType::SignedSmall();
    case CompareOperationHint::kNumber:
      return AstType::Number();
    case CompareOperationHint::kNumberOrOddball:
      return AstType::NumberOrOddball();
    case CompareOperationHint::kInternalizedString:
      return AstType::InternalizedString();
    case CompareOperationHint::kString:
      return AstType::String();
    case CompareOperationHint::kReceiver:
      return AstType::Receiver();
    case CompareOperationHint::kAny:
      return AstType::Any();
  }
  UNREACHABLE();
  return AstType::None();
}

AstType* BinaryOpFeedbackToType(int hint) {
  switch (hint) {
    case BinaryOperationFeedback::kNone:
      return AstType::None();
    case BinaryOperationFeedback::kSignedSmall:
      return AstType::SignedSmall();
    case BinaryOperationFeedback::kNumber:
      return AstType::Number();
    case BinaryOperationFeedback::kString:
      return AstType::String();
    case BinaryOperationFeedback::kNumberOrOddball:
      return AstType::NumberOrOddball();
    case BinaryOperationFeedback::kAny:
    default:
      return AstType::Any();
  }
  UNREACHABLE();
  return AstType::None();
}

}  // end anonymous namespace

void TypeFeedbackOracle::CompareType(TypeFeedbackId id, FeedbackSlot slot,
                                     AstType** left_type, AstType** right_type,
                                     AstType** combined_type) {
  Handle<Object> info = GetInfo(id);
  // A check for a valid slot is not sufficient here. InstanceOf collects
  // type feedback in a General slot.
  if (!info->IsCode()) {
    // For some comparisons we don't have type feedback, e.g.
    // LiteralCompareTypeof.
    *left_type = *right_type = *combined_type = AstType::None();
    return;
  }

  // Feedback from Ignition. The feedback slot will be allocated and initialized
  // to AstType::None() even when ignition is not enabled. So it is safe to get
  // feedback from the type feedback vector.
  DCHECK(!slot.IsInvalid());
  CompareICNexus nexus(feedback_vector_, slot);
  *left_type = *right_type = *combined_type =
      CompareOpHintToType(nexus.GetCompareOperationFeedback());

  // Merge the feedback from full-codegen if available.
  Handle<Code> code = Handle<Code>::cast(info);
  Handle<Map> map;
  Map* raw_map = code->FindFirstMap();
  if (raw_map != NULL) Map::TryUpdate(handle(raw_map)).ToHandle(&map);

  if (code->is_compare_ic_stub()) {
    CompareICStub stub(code->stub_key(), isolate());
    AstType* left_type_from_ic =
        CompareICState::StateToType(zone(), stub.left());
    AstType* right_type_from_ic =
        CompareICState::StateToType(zone(), stub.right());
    AstType* combined_type_from_ic =
        CompareICState::StateToType(zone(), stub.state(), map);
    // Full-codegen collects lhs and rhs feedback seperately and Crankshaft
    // could use this information to optimize better. So if combining the
    // feedback has made the feedback less precise, we should use the feedback
    // only from Full-codegen. If the union of the feedback from Full-codegen
    // is same as that of Ignition, there is no need to combine feedback from
    // from Ignition.
    AstType* combined_type_from_fcg = AstType::Union(
        left_type_from_ic,
        AstType::Union(right_type_from_ic, combined_type_from_ic, zone()),
        zone());
    if (combined_type_from_fcg == *left_type) {
      // Full-codegen collects information about lhs, rhs and result types
      // seperately. So just retain that information.
      *left_type = left_type_from_ic;
      *right_type = right_type_from_ic;
      *combined_type = combined_type_from_ic;
    } else {
      // Combine Ignition and Full-codegen feedbacks.
      *left_type = AstType::Union(*left_type, left_type_from_ic, zone());
      *right_type = AstType::Union(*right_type, right_type_from_ic, zone());
      *combined_type =
          AstType::Union(*combined_type, combined_type_from_ic, zone());
    }
  }
}

void TypeFeedbackOracle::BinaryType(TypeFeedbackId id, FeedbackSlot slot,
                                    AstType** left, AstType** right,
                                    AstType** result,
                                    Maybe<int>* fixed_right_arg,
                                    Handle<AllocationSite>* allocation_site,
                                    Token::Value op) {
  Handle<Object> object = GetInfo(id);
  if (slot.IsInvalid()) {
    // For some binary ops we don't have ICs or feedback slots,
    // e.g. Token::COMMA, but for the operations covered by the BinaryOpIC we
    // should always have them.
    DCHECK(!object->IsCode());
    DCHECK(op < BinaryOpICState::FIRST_TOKEN ||
           op > BinaryOpICState::LAST_TOKEN);
    *left = *right = *result = AstType::None();
    *fixed_right_arg = Nothing<int>();
    *allocation_site = Handle<AllocationSite>::null();
    return;
  }

  // Feedback from Ignition. The feedback slot will be allocated and initialized
  // to AstType::None() even when ignition is not enabled. So it is safe to get
  // feedback from the type feedback vector.
  DCHECK(!slot.IsInvalid());
  BinaryOpICNexus nexus(feedback_vector_, slot);
  *left = *right = *result =
      BinaryOpFeedbackToType(Smi::cast(nexus.GetFeedback())->value());
  *fixed_right_arg = Nothing<int>();
  *allocation_site = Handle<AllocationSite>::null();

  if (!object->IsCode()) return;

  // Merge the feedback from full-codegen if available.
  Handle<Code> code = Handle<Code>::cast(object);
  DCHECK_EQ(Code::BINARY_OP_IC, code->kind());
  BinaryOpICState state(isolate(), code->extra_ic_state());
  DCHECK_EQ(op, state.op());

  // Full-codegen collects lhs and rhs feedback seperately and Crankshaft
  // could use this information to optimize better. So if combining the
  // feedback has made the feedback less precise, we should use the feedback
  // only from Full-codegen. If the union of the feedback from Full-codegen
  // is same as that of Ignition, there is no need to combine feedback from
  // from Ignition.
  AstType* combined_type_from_fcg = AstType::Union(
      state.GetLeftType(),
      AstType::Union(state.GetRightType(), state.GetResultType(), zone()),
      zone());
  if (combined_type_from_fcg == *left) {
    // Full-codegen collects information about lhs, rhs and result types
    // seperately. So just retain that information.
    *left = state.GetLeftType();
    *right = state.GetRightType();
    *result = state.GetResultType();
  } else {
    // Combine Ignition and Full-codegen feedback.
    *left = AstType::Union(*left, state.GetLeftType(), zone());
    *right = AstType::Union(*right, state.GetRightType(), zone());
    *result = AstType::Union(*result, state.GetResultType(), zone());
  }
  // Ignition does not collect this feedback.
  *fixed_right_arg = state.fixed_right_arg();

  AllocationSite* first_allocation_site = code->FindFirstAllocationSite();
  if (first_allocation_site != NULL) {
    *allocation_site = handle(first_allocation_site);
  } else {
    *allocation_site = Handle<AllocationSite>::null();
  }
}

AstType* TypeFeedbackOracle::CountType(TypeFeedbackId id, FeedbackSlot slot) {
  Handle<Object> object = GetInfo(id);
  if (slot.IsInvalid()) {
    DCHECK(!object->IsCode());
    return AstType::None();
  }

  DCHECK(!slot.IsInvalid());
  BinaryOpICNexus nexus(feedback_vector_, slot);
  AstType* type =
      BinaryOpFeedbackToType(Smi::cast(nexus.GetFeedback())->value());

  if (!object->IsCode()) return type;

  Handle<Code> code = Handle<Code>::cast(object);
  DCHECK_EQ(Code::BINARY_OP_IC, code->kind());
  BinaryOpICState state(isolate(), code->extra_ic_state());
  return AstType::Union(type, state.GetLeftType(), zone());
}


bool TypeFeedbackOracle::HasOnlyStringMaps(SmallMapList* receiver_types) {
  bool all_strings = receiver_types->length() > 0;
  for (int i = 0; i < receiver_types->length(); i++) {
    all_strings &= receiver_types->at(i)->IsStringMap();
  }
  return all_strings;
}

void TypeFeedbackOracle::PropertyReceiverTypes(FeedbackSlot slot,
                                               Handle<Name> name,
                                               SmallMapList* receiver_types) {
  receiver_types->Clear();
  if (!slot.IsInvalid()) {
    LoadICNexus nexus(feedback_vector_, slot);
    CollectReceiverTypes(&nexus, receiver_types);
  }
}

void TypeFeedbackOracle::KeyedPropertyReceiverTypes(
    FeedbackSlot slot, SmallMapList* receiver_types, bool* is_string,
    IcCheckType* key_type) {
  receiver_types->Clear();
  if (slot.IsInvalid()) {
    *is_string = false;
    *key_type = ELEMENT;
  } else {
    KeyedLoadICNexus nexus(feedback_vector_, slot);
    CollectReceiverTypes(&nexus, receiver_types);
    *is_string = HasOnlyStringMaps(receiver_types);
    *key_type = nexus.GetKeyType();
  }
}

void TypeFeedbackOracle::AssignmentReceiverTypes(FeedbackSlot slot,
                                                 Handle<Name> name,
                                                 SmallMapList* receiver_types) {
  receiver_types->Clear();
  StoreICNexus nexus(feedback_vector_, slot);
  CollectReceiverTypes(&nexus, receiver_types);
}

void TypeFeedbackOracle::KeyedAssignmentReceiverTypes(
    FeedbackSlot slot, SmallMapList* receiver_types,
    KeyedAccessStoreMode* store_mode, IcCheckType* key_type) {
  receiver_types->Clear();
  CollectReceiverTypes(slot, receiver_types);
  GetStoreModeAndKeyType(slot, store_mode, key_type);
}

void TypeFeedbackOracle::CountReceiverTypes(FeedbackSlot slot,
                                            SmallMapList* receiver_types) {
  receiver_types->Clear();
  if (!slot.IsInvalid()) CollectReceiverTypes(slot, receiver_types);
}

void TypeFeedbackOracle::CollectReceiverTypes(FeedbackSlot slot,
                                              SmallMapList* types) {
  FeedbackSlotKind kind = feedback_vector_->GetKind(slot);
  if (IsStoreICKind(kind) || IsStoreOwnICKind(kind) ||
      IsStoreGlobalICKind(kind)) {
    StoreICNexus nexus(feedback_vector_, slot);
    CollectReceiverTypes(&nexus, types);
  } else {
    DCHECK(IsKeyedStoreICKind(kind));
    KeyedStoreICNexus nexus(feedback_vector_, slot);
    CollectReceiverTypes(&nexus, types);
  }
}

void TypeFeedbackOracle::CollectReceiverTypes(FeedbackNexus* nexus,
                                              SmallMapList* types) {
  MapHandles maps;
  if (nexus->ExtractMaps(&maps) == 0) {
    return;
  }

  types->Reserve(static_cast<int>(maps.size()), zone());
  for (Handle<Map> map : maps) {
    types->AddMapIfMissing(map, zone());
  }
}


uint16_t TypeFeedbackOracle::ToBooleanTypes(TypeFeedbackId id) {
  Handle<Object> object = GetInfo(id);
  return object->IsCode() ? Handle<Code>::cast(object)->to_boolean_state() : 0;
}


// Things are a bit tricky here: The iterator for the RelocInfos and the infos
// themselves are not GC-safe, so we first get all infos, then we create the
// dictionary (possibly triggering GC), and finally we relocate the collected
// infos before we process them.
void TypeFeedbackOracle::BuildDictionary(Handle<Code> code) {
  DisallowHeapAllocation no_allocation;
  ZoneList<RelocInfo> infos(16, zone());
  HandleScope scope(isolate());
  GetRelocInfos(code, &infos);
  CreateDictionary(code, &infos);
  ProcessRelocInfos(&infos);
  // Allocate handle in the parent scope.
  dictionary_ = scope.CloseAndEscape(dictionary_);
}


void TypeFeedbackOracle::GetRelocInfos(Handle<Code> code,
                                       ZoneList<RelocInfo>* infos) {
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID);
  for (RelocIterator it(*code, mask); !it.done(); it.next()) {
    infos->Add(*it.rinfo(), zone());
  }
}


void TypeFeedbackOracle::CreateDictionary(Handle<Code> code,
                                          ZoneList<RelocInfo>* infos) {
  AllowHeapAllocation allocation_allowed;
  Code* old_code = *code;
  dictionary_ = UnseededNumberDictionary::New(isolate(), infos->length());
  RelocateRelocInfos(infos, old_code, *code);
}


void TypeFeedbackOracle::RelocateRelocInfos(ZoneList<RelocInfo>* infos,
                                            Code* old_code,
                                            Code* new_code) {
  for (int i = 0; i < infos->length(); i++) {
    RelocInfo* info = &(*infos)[i];
    info->set_host(new_code);
    info->set_pc(new_code->instruction_start() +
                 (info->pc() - old_code->instruction_start()));
  }
}


void TypeFeedbackOracle::ProcessRelocInfos(ZoneList<RelocInfo>* infos) {
  for (int i = 0; i < infos->length(); i++) {
    RelocInfo reloc_entry = (*infos)[i];
    Address target_address = reloc_entry.target_address();
    TypeFeedbackId ast_id =
        TypeFeedbackId(static_cast<unsigned>((*infos)[i].data()));
    Code* target = Code::GetCodeFromTargetAddress(target_address);
    switch (target->kind()) {
      case Code::LOAD_IC:
      case Code::STORE_IC:
      case Code::KEYED_LOAD_IC:
      case Code::KEYED_STORE_IC:
      case Code::BINARY_OP_IC:
      case Code::COMPARE_IC:
      case Code::TO_BOOLEAN_IC:
        SetInfo(ast_id, target);
        break;

      default:
        break;
    }
  }
}


void TypeFeedbackOracle::SetInfo(TypeFeedbackId ast_id, Object* target) {
  DCHECK(dictionary_->FindEntry(IdToKey(ast_id)) ==
         UnseededNumberDictionary::kNotFound);
  // Dictionary has been allocated with sufficient size for all elements.
  DisallowHeapAllocation no_need_to_resize_dictionary;
  HandleScope scope(isolate());
  USE(UnseededNumberDictionary::AtNumberPut(
      dictionary_, IdToKey(ast_id), handle(target, isolate())));
}


}  // namespace internal
}  // namespace v8
