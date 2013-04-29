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


TypeInfo TypeInfo::TypeFromValue(Handle<Object> value) {
  TypeInfo info;
  if (value->IsSmi()) {
    info = TypeInfo::Smi();
  } else if (value->IsHeapNumber()) {
    info = TypeInfo::IsInt32Double(HeapNumber::cast(*value)->value())
        ? TypeInfo::Integer32()
        : TypeInfo::Double();
  } else if (value->IsString()) {
    info = TypeInfo::String();
  } else {
    info = TypeInfo::Unknown();
  }
  return info;
}


TypeFeedbackOracle::TypeFeedbackOracle(Handle<Code> code,
                                       Handle<Context> native_context,
                                       Isolate* isolate,
                                       Zone* zone)
    : native_context_(native_context),
      isolate_(isolate),
      zone_(zone) {
  BuildDictionary(code);
  ASSERT(reinterpret_cast<Address>(*dictionary_.location()) != kHandleZapValue);
}


static uint32_t IdToKey(TypeFeedbackId ast_id) {
  return static_cast<uint32_t>(ast_id.ToInt());
}


Handle<Object> TypeFeedbackOracle::GetInfo(TypeFeedbackId ast_id) {
  int entry = dictionary_->FindEntry(IdToKey(ast_id));
  return entry != UnseededNumberDictionary::kNotFound
      ? Handle<Object>(dictionary_->ValueAt(entry), isolate_)
      : Handle<Object>::cast(isolate_->factory()->undefined_value());
}


bool TypeFeedbackOracle::LoadIsUninitialized(Property* expr) {
  Handle<Object> map_or_code = GetInfo(expr->PropertyFeedbackId());
  if (map_or_code->IsMap()) return false;
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    return code->is_inline_cache_stub() && code->ic_state() == UNINITIALIZED;
  }
  return false;
}


bool TypeFeedbackOracle::LoadIsMonomorphicNormal(Property* expr) {
  Handle<Object> map_or_code = GetInfo(expr->PropertyFeedbackId());
  if (map_or_code->IsMap()) return true;
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    bool preliminary_checks = code->is_keyed_load_stub() &&
        code->ic_state() == MONOMORPHIC &&
        Code::ExtractTypeFromFlags(code->flags()) == Code::NORMAL;
    if (!preliminary_checks) return false;
    Map* map = code->FindFirstMap();
    return map != NULL && !CanRetainOtherContext(map, *native_context_);
  }
  return false;
}


bool TypeFeedbackOracle::LoadIsPolymorphic(Property* expr) {
  Handle<Object> map_or_code = GetInfo(expr->PropertyFeedbackId());
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    return code->is_keyed_load_stub() && code->ic_state() == POLYMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::StoreIsMonomorphicNormal(TypeFeedbackId ast_id) {
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsMap()) return true;
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    bool standard_store = FLAG_compiled_keyed_stores ||
        (Code::GetKeyedAccessStoreMode(code->extra_ic_state()) ==
         STANDARD_STORE);
    bool preliminary_checks =
        code->is_keyed_store_stub() &&
        standard_store &&
        code->ic_state() == MONOMORPHIC &&
        Code::ExtractTypeFromFlags(code->flags()) == Code::NORMAL;
    if (!preliminary_checks) return false;
    Map* map = code->FindFirstMap();
    return map != NULL && !CanRetainOtherContext(map, *native_context_);
  }
  return false;
}


bool TypeFeedbackOracle::StoreIsPolymorphic(TypeFeedbackId ast_id) {
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    bool standard_store = FLAG_compiled_keyed_stores ||
        (Code::GetKeyedAccessStoreMode(code->extra_ic_state()) ==
         STANDARD_STORE);
    return code->is_keyed_store_stub() && standard_store  &&
        code->ic_state() == POLYMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::CallIsMonomorphic(Call* expr) {
  Handle<Object> value = GetInfo(expr->CallFeedbackId());
  return value->IsMap() || value->IsSmi() || value->IsJSFunction();
}


bool TypeFeedbackOracle::CallNewIsMonomorphic(CallNew* expr) {
  Handle<Object> info = GetInfo(expr->CallNewFeedbackId());
  if (info->IsSmi()) {
    ASSERT(static_cast<ElementsKind>(Smi::cast(*info)->value()) <=
           LAST_FAST_ELEMENTS_KIND);
    return isolate_->global_context()->array_function();
  }
  return info->IsJSFunction();
}


bool TypeFeedbackOracle::ObjectLiteralStoreIsMonomorphic(
    ObjectLiteral::Property* prop) {
  Handle<Object> map_or_code = GetInfo(prop->key()->LiteralFeedbackId());
  return map_or_code->IsMap();
}


bool TypeFeedbackOracle::IsForInFastCase(ForInStatement* stmt) {
  Handle<Object> value = GetInfo(stmt->ForInFeedbackId());
  return value->IsSmi() &&
      Smi::cast(*value)->value() == TypeFeedbackCells::kForInFastCaseMarker;
}


Handle<Map> TypeFeedbackOracle::LoadMonomorphicReceiverType(Property* expr) {
  ASSERT(LoadIsMonomorphicNormal(expr));
  Handle<Object> map_or_code = GetInfo(expr->PropertyFeedbackId());
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    Map* first_map = code->FindFirstMap();
    ASSERT(first_map != NULL);
    return CanRetainOtherContext(first_map, *native_context_)
        ? Handle<Map>::null()
        : Handle<Map>(first_map);
  }
  return Handle<Map>::cast(map_or_code);
}


Handle<Map> TypeFeedbackOracle::StoreMonomorphicReceiverType(
    TypeFeedbackId ast_id) {
  ASSERT(StoreIsMonomorphicNormal(ast_id));
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    Map* first_map = code->FindFirstMap();
    ASSERT(first_map != NULL);
    return CanRetainOtherContext(first_map, *native_context_)
        ? Handle<Map>::null()
        : Handle<Map>(first_map);
  }
  return Handle<Map>::cast(map_or_code);
}


Handle<Map> TypeFeedbackOracle::CompareNilMonomorphicReceiverType(
    TypeFeedbackId id) {
  Handle<Object> maybe_code = GetInfo(id);
  if (maybe_code->IsCode()) {
    Map* first_map = Handle<Code>::cast(maybe_code)->FindFirstMap();
    if (first_map != NULL) return Handle<Map>(first_map);
  }
  return Handle<Map>();
}


KeyedAccessStoreMode TypeFeedbackOracle::GetStoreMode(
    TypeFeedbackId ast_id) {
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    if (code->kind() == Code::KEYED_STORE_IC) {
      return Code::GetKeyedAccessStoreMode(code->extra_ic_state());
    }
  }
  return STANDARD_STORE;
}


void TypeFeedbackOracle::LoadReceiverTypes(Property* expr,
                                           Handle<String> name,
                                           SmallMapList* types) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC);
  CollectReceiverTypes(expr->PropertyFeedbackId(), name, flags, types);
}


void TypeFeedbackOracle::StoreReceiverTypes(Assignment* expr,
                                            Handle<String> name,
                                            SmallMapList* types) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::STORE_IC);
  CollectReceiverTypes(expr->AssignmentFeedbackId(), name, flags, types);
}


void TypeFeedbackOracle::CallReceiverTypes(Call* expr,
                                           Handle<String> name,
                                           CallKind call_kind,
                                           SmallMapList* types) {
  int arity = expr->arguments()->length();

  // Note: Currently we do not take string extra ic data into account
  // here.
  Code::ExtraICState extra_ic_state =
      CallIC::Contextual::encode(call_kind == CALL_AS_FUNCTION);

  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::CALL_IC,
                                                    extra_ic_state,
                                                    Code::NORMAL,
                                                    arity,
                                                    OWN_MAP);
  CollectReceiverTypes(expr->CallFeedbackId(), name, flags, types);
}


CheckType TypeFeedbackOracle::GetCallCheckType(Call* expr) {
  Handle<Object> value = GetInfo(expr->CallFeedbackId());
  if (!value->IsSmi()) return RECEIVER_MAP_CHECK;
  CheckType check = static_cast<CheckType>(Smi::cast(*value)->value());
  ASSERT(check != RECEIVER_MAP_CHECK);
  return check;
}


Handle<JSObject> TypeFeedbackOracle::GetPrototypeForPrimitiveCheck(
    CheckType check) {
  JSFunction* function = NULL;
  switch (check) {
    case RECEIVER_MAP_CHECK:
      UNREACHABLE();
      break;
    case STRING_CHECK:
      function = native_context_->string_function();
      break;
    case SYMBOL_CHECK:
      function = native_context_->symbol_function();
      break;
    case NUMBER_CHECK:
      function = native_context_->number_function();
      break;
    case BOOLEAN_CHECK:
      function = native_context_->boolean_function();
      break;
  }
  ASSERT(function != NULL);
  return Handle<JSObject>(JSObject::cast(function->instance_prototype()));
}


Handle<JSFunction> TypeFeedbackOracle::GetCallTarget(Call* expr) {
  return Handle<JSFunction>::cast(GetInfo(expr->CallFeedbackId()));
}


Handle<JSFunction> TypeFeedbackOracle::GetCallNewTarget(CallNew* expr) {
  Handle<Object> info = GetInfo(expr->CallNewFeedbackId());
  if (info->IsSmi()) {
    ASSERT(static_cast<ElementsKind>(Smi::cast(*info)->value()) <=
           LAST_FAST_ELEMENTS_KIND);
    return Handle<JSFunction>(isolate_->global_context()->array_function());
  } else {
    return Handle<JSFunction>::cast(info);
  }
}


ElementsKind TypeFeedbackOracle::GetCallNewElementsKind(CallNew* expr) {
  Handle<Object> info = GetInfo(expr->CallNewFeedbackId());
  if (info->IsSmi()) {
    return static_cast<ElementsKind>(Smi::cast(*info)->value());
  } else {
    // TODO(mvstanton): avoided calling GetInitialFastElementsKind() for perf
    // reasons. Is there a better fix?
    if (FLAG_packed_arrays) {
      return FAST_SMI_ELEMENTS;
    } else {
      return FAST_HOLEY_SMI_ELEMENTS;
    }
  }
}

Handle<Map> TypeFeedbackOracle::GetObjectLiteralStoreMap(
    ObjectLiteral::Property* prop) {
  ASSERT(ObjectLiteralStoreIsMonomorphic(prop));
  return Handle<Map>::cast(GetInfo(prop->key()->LiteralFeedbackId()));
}


bool TypeFeedbackOracle::LoadIsBuiltin(Property* expr, Builtins::Name id) {
  return *GetInfo(expr->PropertyFeedbackId()) ==
      isolate_->builtins()->builtin(id);
}


bool TypeFeedbackOracle::LoadIsStub(Property* expr, ICStub* stub) {
  Handle<Object> object = GetInfo(expr->PropertyFeedbackId());
  if (!object->IsCode()) return false;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_load_stub()) return false;
  if (code->ic_state() != MONOMORPHIC) return false;
  return stub->Describes(*code);
}


static TypeInfo TypeFromCompareType(CompareIC::State state) {
  switch (state) {
    case CompareIC::UNINITIALIZED:
      // Uninitialized means never executed.
      return TypeInfo::Uninitialized();
    case CompareIC::SMI:
      return TypeInfo::Smi();
    case CompareIC::NUMBER:
      return TypeInfo::Number();
    case CompareIC::INTERNALIZED_STRING:
      return TypeInfo::InternalizedString();
    case CompareIC::STRING:
      return TypeInfo::String();
    case CompareIC::OBJECT:
    case CompareIC::KNOWN_OBJECT:
      // TODO(kasperl): We really need a type for JS objects here.
      return TypeInfo::NonPrimitive();
    case CompareIC::GENERIC:
    default:
      return TypeInfo::Unknown();
  }
}


void TypeFeedbackOracle::CompareType(CompareOperation* expr,
                                     TypeInfo* left_type,
                                     TypeInfo* right_type,
                                     TypeInfo* overall_type) {
  Handle<Object> object = GetInfo(expr->CompareOperationFeedbackId());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) {
    *left_type = *right_type = *overall_type = unknown;
    return;
  }
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_compare_ic_stub()) {
    *left_type = *right_type = *overall_type = unknown;
    return;
  }

  int stub_minor_key = code->stub_info();
  CompareIC::State left_state, right_state, handler_state;
  ICCompareStub::DecodeMinorKey(stub_minor_key, &left_state, &right_state,
                                &handler_state, NULL);
  *left_type = TypeFromCompareType(left_state);
  *right_type = TypeFromCompareType(right_state);
  *overall_type = TypeFromCompareType(handler_state);
}


Handle<Map> TypeFeedbackOracle::GetCompareMap(CompareOperation* expr) {
  Handle<Object> object = GetInfo(expr->CompareOperationFeedbackId());
  if (!object->IsCode()) return Handle<Map>::null();
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_compare_ic_stub()) return Handle<Map>::null();
  CompareIC::State state = ICCompareStub::CompareState(code->stub_info());
  if (state != CompareIC::KNOWN_OBJECT) {
    return Handle<Map>::null();
  }
  Map* first_map = code->FindFirstMap();
  ASSERT(first_map != NULL);
  return CanRetainOtherContext(first_map, *native_context_)
      ? Handle<Map>::null()
      : Handle<Map>(first_map);
}


TypeInfo TypeFeedbackOracle::UnaryType(UnaryOperation* expr) {
  Handle<Object> object = GetInfo(expr->UnaryOperationFeedbackId());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  ASSERT(code->is_unary_op_stub());
  UnaryOpIC::TypeInfo type = static_cast<UnaryOpIC::TypeInfo>(
      code->unary_op_type());
  switch (type) {
    case UnaryOpIC::SMI:
      return TypeInfo::Smi();
    case UnaryOpIC::NUMBER:
      return TypeInfo::Double();
    default:
      return unknown;
  }
}


static TypeInfo TypeFromBinaryOpType(BinaryOpIC::TypeInfo binary_type) {
  switch (binary_type) {
    // Uninitialized means never executed.
    case BinaryOpIC::UNINITIALIZED:  return TypeInfo::Uninitialized();
    case BinaryOpIC::SMI:            return TypeInfo::Smi();
    case BinaryOpIC::INT32:          return TypeInfo::Integer32();
    case BinaryOpIC::NUMBER:         return TypeInfo::Double();
    case BinaryOpIC::ODDBALL:        return TypeInfo::Unknown();
    case BinaryOpIC::STRING:         return TypeInfo::String();
    case BinaryOpIC::GENERIC:        return TypeInfo::Unknown();
  }
  UNREACHABLE();
  return TypeInfo::Unknown();
}


void TypeFeedbackOracle::BinaryType(BinaryOperation* expr,
                                    TypeInfo* left,
                                    TypeInfo* right,
                                    TypeInfo* result) {
  Handle<Object> object = GetInfo(expr->BinaryOperationFeedbackId());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) {
    *left = *right = *result = unknown;
    return;
  }
  Handle<Code> code = Handle<Code>::cast(object);
  if (code->is_binary_op_stub()) {
    BinaryOpIC::TypeInfo left_type, right_type, result_type;
    BinaryOpStub::decode_types_from_minor_key(code->stub_info(), &left_type,
                                              &right_type, &result_type);
    *left = TypeFromBinaryOpType(left_type);
    *right = TypeFromBinaryOpType(right_type);
    *result = TypeFromBinaryOpType(result_type);
    return;
  }
  // Not a binary op stub.
  *left = *right = *result = unknown;
}


TypeInfo TypeFeedbackOracle::SwitchType(CaseClause* clause) {
  Handle<Object> object = GetInfo(clause->CompareId());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_compare_ic_stub()) return unknown;

  CompareIC::State state = ICCompareStub::CompareState(code->stub_info());
  return TypeFromCompareType(state);
}


TypeInfo TypeFeedbackOracle::IncrementType(CountOperation* expr) {
  Handle<Object> object = GetInfo(expr->CountBinOpFeedbackId());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_binary_op_stub()) return unknown;

  BinaryOpIC::TypeInfo left_type, right_type, unused_result_type;
  BinaryOpStub::decode_types_from_minor_key(code->stub_info(), &left_type,
                                            &right_type, &unused_result_type);
  // CountOperations should always have +1 or -1 as their right input.
  ASSERT(right_type == BinaryOpIC::SMI ||
         right_type == BinaryOpIC::UNINITIALIZED);

  switch (left_type) {
    case BinaryOpIC::UNINITIALIZED:
    case BinaryOpIC::SMI:
      return TypeInfo::Smi();
    case BinaryOpIC::INT32:
      return TypeInfo::Integer32();
    case BinaryOpIC::NUMBER:
      return TypeInfo::Double();
    case BinaryOpIC::STRING:
    case BinaryOpIC::GENERIC:
      return unknown;
    default:
      return unknown;
  }
  UNREACHABLE();
  return unknown;
}


static void AddMapIfMissing(Handle<Map> map, SmallMapList* list,
                            Zone* zone) {
  for (int i = 0; i < list->length(); ++i) {
    if (list->at(i).is_identical_to(map)) return;
  }
  list->Add(map, zone);
}


void TypeFeedbackOracle::CollectPolymorphicMaps(Handle<Code> code,
                                                SmallMapList* types) {
  MapHandleList maps;
  code->FindAllMaps(&maps);
  types->Reserve(maps.length(), zone());
  for (int i = 0; i < maps.length(); i++) {
    Handle<Map> map(maps.at(i));
    if (!CanRetainOtherContext(*map, *native_context_)) {
      AddMapIfMissing(map, types, zone());
    }
  }
}


void TypeFeedbackOracle::CollectReceiverTypes(TypeFeedbackId ast_id,
                                              Handle<String> name,
                                              Code::Flags flags,
                                              SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (object->IsUndefined() || object->IsSmi()) return;

  if (object.is_identical_to(isolate_->builtins()->StoreIC_GlobalProxy())) {
    // TODO(fschneider): We could collect the maps and signal that
    // we need a generic store (or load) here.
    ASSERT(Handle<Code>::cast(object)->ic_state() == GENERIC);
  } else if (object->IsMap()) {
    types->Add(Handle<Map>::cast(object), zone());
  } else if (Handle<Code>::cast(object)->ic_state() == POLYMORPHIC) {
    CollectPolymorphicMaps(Handle<Code>::cast(object), types);
  } else if (FLAG_collect_megamorphic_maps_from_stub_cache &&
      Handle<Code>::cast(object)->ic_state() == MEGAMORPHIC) {
    types->Reserve(4, zone());
    ASSERT(object->IsCode());
    isolate_->stub_cache()->CollectMatchingMaps(types,
                                                *name,
                                                flags,
                                                native_context_,
                                                zone());
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


void TypeFeedbackOracle::CollectKeyedReceiverTypes(TypeFeedbackId ast_id,
                                                   SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (!object->IsCode()) return;
  Handle<Code> code = Handle<Code>::cast(object);
  if (code->kind() == Code::KEYED_LOAD_IC ||
      code->kind() == Code::KEYED_STORE_IC) {
    CollectPolymorphicMaps(code, types);
  }
}


byte TypeFeedbackOracle::ToBooleanTypes(TypeFeedbackId id) {
  Handle<Object> object = GetInfo(id);
  return object->IsCode() ? Handle<Code>::cast(object)->to_boolean_state() : 0;
}


byte TypeFeedbackOracle::CompareNilTypes(TypeFeedbackId id) {
  Handle<Object> object = GetInfo(id);
  if (object->IsCode() &&
      Handle<Code>::cast(object)->is_compare_nil_ic_stub()) {
    return Handle<Code>::cast(object)->compare_nil_state();
  } else {
    return CompareNilICStub::kFullCompare;
  }
}


// Things are a bit tricky here: The iterator for the RelocInfos and the infos
// themselves are not GC-safe, so we first get all infos, then we create the
// dictionary (possibly triggering GC), and finally we relocate the collected
// infos before we process them.
void TypeFeedbackOracle::BuildDictionary(Handle<Code> code) {
  AssertNoAllocation no_allocation;
  ZoneList<RelocInfo> infos(16, zone());
  HandleScope scope(isolate_);
  GetRelocInfos(code, &infos);
  CreateDictionary(code, &infos);
  ProcessRelocInfos(&infos);
  ProcessTypeFeedbackCells(code);
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
  DisableAssertNoAllocation allocation_allowed;
  int cell_count = code->type_feedback_info()->IsTypeFeedbackInfo()
      ? TypeFeedbackInfo::cast(code->type_feedback_info())->
          type_feedback_cells()->CellCount()
      : 0;
  int length = infos->length() + cell_count;
  byte* old_start = code->instruction_start();
  dictionary_ = FACTORY->NewUnseededNumberDictionary(length);
  byte* new_start = code->instruction_start();
  RelocateRelocInfos(infos, old_start, new_start);
}


void TypeFeedbackOracle::RelocateRelocInfos(ZoneList<RelocInfo>* infos,
                                            byte* old_start,
                                            byte* new_start) {
  for (int i = 0; i < infos->length(); i++) {
    RelocInfo* info = &(*infos)[i];
    info->set_pc(new_start + (info->pc() - old_start));
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
      case Code::CALL_IC:
      case Code::KEYED_CALL_IC:
        if (target->ic_state() == MONOMORPHIC) {
          if (target->kind() == Code::CALL_IC &&
              target->check_type() != RECEIVER_MAP_CHECK) {
            SetInfo(ast_id, Smi::FromInt(target->check_type()));
          } else {
            Object* map = target->FindFirstMap();
            if (map == NULL) {
              SetInfo(ast_id, static_cast<Object*>(target));
            } else if (!CanRetainOtherContext(Map::cast(map),
                                              *native_context_)) {
              SetInfo(ast_id, map);
            }
          }
        } else {
          SetInfo(ast_id, target);
        }
        break;

      case Code::KEYED_LOAD_IC:
      case Code::KEYED_STORE_IC:
        if (target->ic_state() == MONOMORPHIC ||
            target->ic_state() == POLYMORPHIC) {
          SetInfo(ast_id, target);
        }
        break;

      case Code::UNARY_OP_IC:
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


void TypeFeedbackOracle::ProcessTypeFeedbackCells(Handle<Code> code) {
  Object* raw_info = code->type_feedback_info();
  if (!raw_info->IsTypeFeedbackInfo()) return;
  Handle<TypeFeedbackCells> cache(
      TypeFeedbackInfo::cast(raw_info)->type_feedback_cells());
  for (int i = 0; i < cache->CellCount(); i++) {
    TypeFeedbackId ast_id = cache->AstId(i);
    Object* value = cache->Cell(i)->value();
    if (value->IsSmi() ||
        (value->IsJSFunction() &&
         !CanRetainOtherContext(JSFunction::cast(value),
                                *native_context_))) {
      SetInfo(ast_id, value);
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
