// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "ast.h"
#include "code-stubs.h"
#include "compiler.h"
#include "ic.h"
#include "macro-assembler.h"
#include "stub-cache.h"
#include "type-info.h"

#include "ic-inl.h"
#include "objects-inl.h"

namespace v8 {
namespace internal {


TypeFeedbackOracle::TypeFeedbackOracle(Handle<Code> code,
                                       Handle<Context> native_context,
                                       Zone* zone)
    : native_context_(native_context),
      zone_(zone) {
  Object* raw_info = code->type_feedback_info();
  if (raw_info->IsTypeFeedbackInfo()) {
    feedback_vector_ = Handle<FixedArray>(TypeFeedbackInfo::cast(raw_info)->
                                          feedback_vector());
  }

  BuildDictionary(code);
  ASSERT(dictionary_->IsDictionary());
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


Handle<Object> TypeFeedbackOracle::GetInfo(int slot) {
  ASSERT(slot >= 0 && slot < feedback_vector_->length());
  Object* obj = feedback_vector_->get(slot);
  if (!obj->IsJSFunction() ||
      !CanRetainOtherContext(JSFunction::cast(obj), *native_context_)) {
    return Handle<Object>(obj, isolate());
  }
  return Handle<Object>::cast(isolate()->factory()->undefined_value());
}


bool TypeFeedbackOracle::LoadIsUninitialized(TypeFeedbackId id) {
  Handle<Object> maybe_code = GetInfo(id);
  if (maybe_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(maybe_code);
    return code->is_inline_cache_stub() && code->ic_state() == UNINITIALIZED;
  }
  return false;
}


bool TypeFeedbackOracle::StoreIsUninitialized(TypeFeedbackId ast_id) {
  Handle<Object> maybe_code = GetInfo(ast_id);
  if (!maybe_code->IsCode()) return false;
  Handle<Code> code = Handle<Code>::cast(maybe_code);
  return code->ic_state() == UNINITIALIZED;
}


bool TypeFeedbackOracle::StoreIsKeyedPolymorphic(TypeFeedbackId ast_id) {
  Handle<Object> maybe_code = GetInfo(ast_id);
  if (maybe_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(maybe_code);
    return code->is_keyed_store_stub() &&
        code->ic_state() == POLYMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::CallIsMonomorphic(int slot) {
  Handle<Object> value = GetInfo(slot);
  return FLAG_pretenuring_call_new
      ? value->IsJSFunction()
      : value->IsAllocationSite() || value->IsJSFunction();
}


bool TypeFeedbackOracle::CallNewIsMonomorphic(int slot) {
  Handle<Object> info = GetInfo(slot);
  return FLAG_pretenuring_call_new
      ? info->IsJSFunction()
      : info->IsAllocationSite() || info->IsJSFunction();
}


byte TypeFeedbackOracle::ForInType(int feedback_vector_slot) {
  Handle<Object> value = GetInfo(feedback_vector_slot);
  return value->IsSmi() &&
      Smi::cast(*value)->value() == TypeFeedbackInfo::kForInFastCaseMarker
          ? ForInStatement::FAST_FOR_IN : ForInStatement::SLOW_FOR_IN;
}


KeyedAccessStoreMode TypeFeedbackOracle::GetStoreMode(
    TypeFeedbackId ast_id) {
  Handle<Object> maybe_code = GetInfo(ast_id);
  if (maybe_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(maybe_code);
    if (code->kind() == Code::KEYED_STORE_IC) {
      return KeyedStoreIC::GetKeyedAccessStoreMode(code->extra_ic_state());
    }
  }
  return STANDARD_STORE;
}


Handle<JSFunction> TypeFeedbackOracle::GetCallTarget(int slot) {
  Handle<Object> info = GetInfo(slot);
  if (FLAG_pretenuring_call_new || info->IsJSFunction()) {
    return Handle<JSFunction>::cast(info);
  }

  ASSERT(info->IsAllocationSite());
  return Handle<JSFunction>(isolate()->native_context()->array_function());
}


Handle<JSFunction> TypeFeedbackOracle::GetCallNewTarget(int slot) {
  Handle<Object> info = GetInfo(slot);
  if (FLAG_pretenuring_call_new || info->IsJSFunction()) {
    return Handle<JSFunction>::cast(info);
  }

  ASSERT(info->IsAllocationSite());
  return Handle<JSFunction>(isolate()->native_context()->array_function());
}


Handle<AllocationSite> TypeFeedbackOracle::GetCallNewAllocationSite(int slot) {
  Handle<Object> info = GetInfo(slot);
  if (FLAG_pretenuring_call_new || info->IsAllocationSite()) {
    return Handle<AllocationSite>::cast(info);
  }
  return Handle<AllocationSite>::null();
}


bool TypeFeedbackOracle::LoadIsBuiltin(
    TypeFeedbackId id, Builtins::Name builtin) {
  return *GetInfo(id) == isolate()->builtins()->builtin(builtin);
}


bool TypeFeedbackOracle::LoadIsStub(TypeFeedbackId id, ICStub* stub) {
  Handle<Object> object = GetInfo(id);
  if (!object->IsCode()) return false;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_load_stub()) return false;
  if (code->ic_state() != MONOMORPHIC) return false;
  return stub->Describes(*code);
}


void TypeFeedbackOracle::CompareType(TypeFeedbackId id,
                                     Type** left_type,
                                     Type** right_type,
                                     Type** combined_type) {
  Handle<Object> info = GetInfo(id);
  if (!info->IsCode()) {
    // For some comparisons we don't have ICs, e.g. LiteralCompareTypeof.
    *left_type = *right_type = *combined_type = Type::None(zone());
    return;
  }
  Handle<Code> code = Handle<Code>::cast(info);

  Handle<Map> map;
  Map* raw_map = code->FindFirstMap();
  if (raw_map != NULL) {
    map = Map::CurrentMapForDeprecated(handle(raw_map));
    if (!map.is_null() && CanRetainOtherContext(*map, *native_context_)) {
      map = Handle<Map>::null();
    }
  }

  if (code->is_compare_ic_stub()) {
    int stub_minor_key = code->stub_info();
    CompareIC::StubInfoToType(
        stub_minor_key, left_type, right_type, combined_type, map, zone());
  } else if (code->is_compare_nil_ic_stub()) {
    CompareNilICStub stub(code->extra_ic_state());
    *combined_type = stub.GetType(zone(), map);
    *left_type = *right_type = stub.GetInputType(zone(), map);
  }
}


void TypeFeedbackOracle::BinaryType(TypeFeedbackId id,
                                    Type** left,
                                    Type** right,
                                    Type** result,
                                    Maybe<int>* fixed_right_arg,
                                    Handle<AllocationSite>* allocation_site,
                                    Token::Value op) {
  Handle<Object> object = GetInfo(id);
  if (!object->IsCode()) {
    // For some binary ops we don't have ICs, e.g. Token::COMMA, but for the
    // operations covered by the BinaryOpIC we should always have them.
    ASSERT(op < BinaryOpIC::State::FIRST_TOKEN ||
           op > BinaryOpIC::State::LAST_TOKEN);
    *left = *right = *result = Type::None(zone());
    *fixed_right_arg = Maybe<int>();
    *allocation_site = Handle<AllocationSite>::null();
    return;
  }
  Handle<Code> code = Handle<Code>::cast(object);
  ASSERT_EQ(Code::BINARY_OP_IC, code->kind());
  BinaryOpIC::State state(code->extra_ic_state());
  ASSERT_EQ(op, state.op());

  *left = state.GetLeftType(zone());
  *right = state.GetRightType(zone());
  *result = state.GetResultType(zone());
  *fixed_right_arg = state.fixed_right_arg();

  AllocationSite* first_allocation_site = code->FindFirstAllocationSite();
  if (first_allocation_site != NULL) {
    *allocation_site = handle(first_allocation_site);
  } else {
    *allocation_site = Handle<AllocationSite>::null();
  }
}


Type* TypeFeedbackOracle::CountType(TypeFeedbackId id) {
  Handle<Object> object = GetInfo(id);
  if (!object->IsCode()) return Type::None(zone());
  Handle<Code> code = Handle<Code>::cast(object);
  ASSERT_EQ(Code::BINARY_OP_IC, code->kind());
  BinaryOpIC::State state(code->extra_ic_state());
  return state.GetLeftType(zone());
}


void TypeFeedbackOracle::PropertyReceiverTypes(
    TypeFeedbackId id, Handle<String> name,
    SmallMapList* receiver_types, bool* is_prototype) {
  receiver_types->Clear();
  FunctionPrototypeStub proto_stub(Code::LOAD_IC);
  *is_prototype = LoadIsStub(id, &proto_stub);
  if (!*is_prototype) {
    Code::Flags flags = Code::ComputeHandlerFlags(Code::LOAD_IC);
    CollectReceiverTypes(id, name, flags, receiver_types);
  }
}


void TypeFeedbackOracle::KeyedPropertyReceiverTypes(
    TypeFeedbackId id, SmallMapList* receiver_types, bool* is_string) {
  receiver_types->Clear();
  *is_string = false;
  if (LoadIsBuiltin(id, Builtins::kKeyedLoadIC_String)) {
    *is_string = true;
  } else {
    CollectReceiverTypes(id, receiver_types);
  }
}


void TypeFeedbackOracle::AssignmentReceiverTypes(
    TypeFeedbackId id, Handle<String> name, SmallMapList* receiver_types) {
  receiver_types->Clear();
  Code::Flags flags = Code::ComputeHandlerFlags(Code::STORE_IC);
  CollectReceiverTypes(id, name, flags, receiver_types);
}


void TypeFeedbackOracle::KeyedAssignmentReceiverTypes(
    TypeFeedbackId id, SmallMapList* receiver_types,
    KeyedAccessStoreMode* store_mode) {
  receiver_types->Clear();
  CollectReceiverTypes(id, receiver_types);
  *store_mode = GetStoreMode(id);
}


void TypeFeedbackOracle::CountReceiverTypes(TypeFeedbackId id,
                                            SmallMapList* receiver_types) {
  receiver_types->Clear();
  CollectReceiverTypes(id, receiver_types);
}


void TypeFeedbackOracle::CollectReceiverTypes(TypeFeedbackId ast_id,
                                              Handle<String> name,
                                              Code::Flags flags,
                                              SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (object->IsUndefined() || object->IsSmi()) return;

  ASSERT(object->IsCode());
  Handle<Code> code(Handle<Code>::cast(object));

  if (FLAG_collect_megamorphic_maps_from_stub_cache &&
      code->ic_state() == MEGAMORPHIC) {
    types->Reserve(4, zone());
    isolate()->stub_cache()->CollectMatchingMaps(
        types, name, flags, native_context_, zone());
  } else {
    CollectReceiverTypes(ast_id, types);
  }
}


// Check if a map originates from a given native context. We use this
// information to filter out maps from different context to avoid
// retaining objects from different tabs in Chrome via optimized code.
bool TypeFeedbackOracle::CanRetainOtherContext(Map* map,
                                               Context* native_context) {
  Object* constructor = NULL;
  while (!map->prototype()->IsNull()) {
    constructor = map->constructor();
    if (!constructor->IsNull()) {
      // If the constructor is not null or a JSFunction, we have to
      // conservatively assume that it may retain a native context.
      if (!constructor->IsJSFunction()) return true;
      // Check if the constructor directly references a foreign context.
      if (CanRetainOtherContext(JSFunction::cast(constructor),
                                native_context)) {
        return true;
      }
    }
    map = HeapObject::cast(map->prototype())->map();
  }
  constructor = map->constructor();
  if (constructor->IsNull()) return false;
  JSFunction* function = JSFunction::cast(constructor);
  return CanRetainOtherContext(function, native_context);
}


bool TypeFeedbackOracle::CanRetainOtherContext(JSFunction* function,
                                               Context* native_context) {
  return function->context()->global_object() != native_context->global_object()
      && function->context()->global_object() != native_context->builtins();
}


void TypeFeedbackOracle::CollectReceiverTypes(TypeFeedbackId ast_id,
                                              SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (!object->IsCode()) return;
  Handle<Code> code = Handle<Code>::cast(object);
  MapHandleList maps;
  if (code->ic_state() == MONOMORPHIC) {
    Map* map = code->FindFirstMap();
    if (map != NULL) maps.Add(handle(map));
  } else if (code->ic_state() == POLYMORPHIC) {
    code->FindAllMaps(&maps);
  } else {
    return;
  }
  types->Reserve(maps.length(), zone());
  for (int i = 0; i < maps.length(); i++) {
    Handle<Map> map(maps.at(i));
    if (!CanRetainOtherContext(*map, *native_context_)) {
      types->AddMapIfMissing(map, zone());
    }
  }
}


byte TypeFeedbackOracle::ToBooleanTypes(TypeFeedbackId id) {
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
  dictionary_ =
      isolate()->factory()->NewUnseededNumberDictionary(infos->length());
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
      case Code::COMPARE_NIL_IC:
        SetInfo(ast_id, target);
        break;

      default:
        break;
    }
  }
}


void TypeFeedbackOracle::SetInfo(TypeFeedbackId ast_id, Object* target) {
  ASSERT(dictionary_->FindEntry(IdToKey(ast_id)) ==
         UnseededNumberDictionary::kNotFound);
  MaybeObject* maybe_result = dictionary_->AtNumberPut(IdToKey(ast_id), target);
  USE(maybe_result);
#ifdef DEBUG
  Object* result = NULL;
  // Dictionary has been allocated with sufficient size for all elements.
  ASSERT(maybe_result->ToObject(&result));
  ASSERT(*dictionary_ == result);
#endif
}


} }  // namespace v8::internal
