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
                                       Handle<Context> global_context,
                                       Isolate* isolate) {
  global_context_ = global_context;
  isolate_ = isolate;
  BuildDictionary(code);
  ASSERT(reinterpret_cast<Address>(*dictionary_.location()) != kHandleZapValue);
}


Handle<Object> TypeFeedbackOracle::GetInfo(unsigned ast_id) {
  int entry = dictionary_->FindEntry(ast_id);
  return entry != UnseededNumberDictionary::kNotFound
      ? Handle<Object>(dictionary_->ValueAt(entry))
      : Handle<Object>::cast(isolate_->factory()->undefined_value());
}


bool TypeFeedbackOracle::LoadIsMonomorphicNormal(Property* expr) {
  Handle<Object> map_or_code = GetInfo(expr->id());
  if (map_or_code->IsMap()) return true;
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    return code->is_keyed_load_stub() &&
        code->ic_state() == MONOMORPHIC &&
        Code::ExtractTypeFromFlags(code->flags()) == NORMAL &&
        code->FindFirstMap() != NULL &&
        !CanRetainOtherContext(code->FindFirstMap(), *global_context_);
  }
  return false;
}


bool TypeFeedbackOracle::LoadIsMegamorphicWithTypeInfo(Property* expr) {
  Handle<Object> map_or_code = GetInfo(expr->id());
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    Builtins* builtins = isolate_->builtins();
    return code->is_keyed_load_stub() &&
        *code != builtins->builtin(Builtins::kKeyedLoadIC_Generic) &&
        code->ic_state() == MEGAMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::StoreIsMonomorphicNormal(Expression* expr) {
  Handle<Object> map_or_code = GetInfo(expr->id());
  if (map_or_code->IsMap()) return true;
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    bool allow_growth =
        Code::GetKeyedAccessGrowMode(code->extra_ic_state()) ==
        ALLOW_JSARRAY_GROWTH;
    return code->is_keyed_store_stub() &&
        !allow_growth &&
        code->ic_state() == MONOMORPHIC &&
        Code::ExtractTypeFromFlags(code->flags()) == NORMAL &&
        code->FindFirstMap() != NULL &&
        !CanRetainOtherContext(code->FindFirstMap(), *global_context_);
  }
  return false;
}


bool TypeFeedbackOracle::StoreIsMegamorphicWithTypeInfo(Expression* expr) {
  Handle<Object> map_or_code = GetInfo(expr->id());
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    Builtins* builtins = isolate_->builtins();
    bool allow_growth =
        Code::GetKeyedAccessGrowMode(code->extra_ic_state()) ==
        ALLOW_JSARRAY_GROWTH;
    return code->is_keyed_store_stub() &&
        !allow_growth &&
        *code != builtins->builtin(Builtins::kKeyedStoreIC_Generic) &&
        *code != builtins->builtin(Builtins::kKeyedStoreIC_Generic_Strict) &&
        code->ic_state() == MEGAMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::CallIsMonomorphic(Call* expr) {
  Handle<Object> value = GetInfo(expr->id());
  return value->IsMap() || value->IsSmi() || value->IsJSFunction();
}


bool TypeFeedbackOracle::CallNewIsMonomorphic(CallNew* expr) {
  Handle<Object> value = GetInfo(expr->id());
  return value->IsJSFunction();
}


Handle<Map> TypeFeedbackOracle::LoadMonomorphicReceiverType(Property* expr) {
  ASSERT(LoadIsMonomorphicNormal(expr));
  Handle<Object> map_or_code = GetInfo(expr->id());
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    Map* first_map = code->FindFirstMap();
    ASSERT(first_map != NULL);
    return CanRetainOtherContext(first_map, *global_context_)
        ? Handle<Map>::null()
        : Handle<Map>(first_map);
  }
  return Handle<Map>::cast(map_or_code);
}


Handle<Map> TypeFeedbackOracle::StoreMonomorphicReceiverType(Expression* expr) {
  ASSERT(StoreIsMonomorphicNormal(expr));
  Handle<Object> map_or_code = GetInfo(expr->id());
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    Map* first_map = code->FindFirstMap();
    ASSERT(first_map != NULL);
    return CanRetainOtherContext(first_map, *global_context_)
        ? Handle<Map>::null()
        : Handle<Map>(first_map);
  }
  return Handle<Map>::cast(map_or_code);
}


void TypeFeedbackOracle::LoadReceiverTypes(Property* expr,
                                           Handle<String> name,
                                           SmallMapList* types) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, NORMAL);
  CollectReceiverTypes(expr->id(), name, flags, types);
}


void TypeFeedbackOracle::StoreReceiverTypes(Assignment* expr,
                                            Handle<String> name,
                                            SmallMapList* types) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::STORE_IC, NORMAL);
  CollectReceiverTypes(expr->id(), name, flags, types);
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
                                                    NORMAL,
                                                    extra_ic_state,
                                                    OWN_MAP,
                                                    arity);
  CollectReceiverTypes(expr->id(), name, flags, types);
}


CheckType TypeFeedbackOracle::GetCallCheckType(Call* expr) {
  Handle<Object> value = GetInfo(expr->id());
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
      function = global_context_->string_function();
      break;
    case NUMBER_CHECK:
      function = global_context_->number_function();
      break;
    case BOOLEAN_CHECK:
      function = global_context_->boolean_function();
      break;
  }
  ASSERT(function != NULL);
  return Handle<JSObject>(JSObject::cast(function->instance_prototype()));
}


Handle<JSFunction> TypeFeedbackOracle::GetCallTarget(Call* expr) {
  return Handle<JSFunction>::cast(GetInfo(expr->id()));
}


bool TypeFeedbackOracle::LoadIsBuiltin(Property* expr, Builtins::Name id) {
  return *GetInfo(expr->id()) ==
      isolate_->builtins()->builtin(id);
}


TypeInfo TypeFeedbackOracle::CompareType(CompareOperation* expr) {
  Handle<Object> object = GetInfo(expr->id());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_compare_ic_stub()) return unknown;

  CompareIC::State state = static_cast<CompareIC::State>(code->compare_state());
  switch (state) {
    case CompareIC::UNINITIALIZED:
      // Uninitialized means never executed.
      return TypeInfo::Uninitialized();
    case CompareIC::SMIS:
      return TypeInfo::Smi();
    case CompareIC::HEAP_NUMBERS:
      return TypeInfo::Number();
    case CompareIC::SYMBOLS:
    case CompareIC::STRINGS:
      return TypeInfo::String();
    case CompareIC::OBJECTS:
    case CompareIC::KNOWN_OBJECTS:
      // TODO(kasperl): We really need a type for JS objects here.
      return TypeInfo::NonPrimitive();
    case CompareIC::GENERIC:
    default:
      return unknown;
  }
}


bool TypeFeedbackOracle::IsSymbolCompare(CompareOperation* expr) {
  Handle<Object> object = GetInfo(expr->id());
  if (!object->IsCode()) return false;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_compare_ic_stub()) return false;
  CompareIC::State state = static_cast<CompareIC::State>(code->compare_state());
  return state == CompareIC::SYMBOLS;
}


Handle<Map> TypeFeedbackOracle::GetCompareMap(CompareOperation* expr) {
  Handle<Object> object = GetInfo(expr->id());
  if (!object->IsCode()) return Handle<Map>::null();
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_compare_ic_stub()) return Handle<Map>::null();
  CompareIC::State state = static_cast<CompareIC::State>(code->compare_state());
  if (state != CompareIC::KNOWN_OBJECTS) {
    return Handle<Map>::null();
  }
  Map* first_map = code->FindFirstMap();
  ASSERT(first_map != NULL);
  return CanRetainOtherContext(first_map, *global_context_)
      ? Handle<Map>::null()
      : Handle<Map>(first_map);
}


TypeInfo TypeFeedbackOracle::UnaryType(UnaryOperation* expr) {
  Handle<Object> object = GetInfo(expr->id());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  ASSERT(code->is_unary_op_stub());
  UnaryOpIC::TypeInfo type = static_cast<UnaryOpIC::TypeInfo>(
      code->unary_op_type());
  switch (type) {
    case UnaryOpIC::SMI:
      return TypeInfo::Smi();
    case UnaryOpIC::HEAP_NUMBER:
      return TypeInfo::Double();
    default:
      return unknown;
  }
}


TypeInfo TypeFeedbackOracle::BinaryType(BinaryOperation* expr) {
  Handle<Object> object = GetInfo(expr->id());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  if (code->is_binary_op_stub()) {
    BinaryOpIC::TypeInfo type = static_cast<BinaryOpIC::TypeInfo>(
        code->binary_op_type());
    BinaryOpIC::TypeInfo result_type = static_cast<BinaryOpIC::TypeInfo>(
        code->binary_op_result_type());

    switch (type) {
      case BinaryOpIC::UNINITIALIZED:
        // Uninitialized means never executed.
        return TypeInfo::Uninitialized();
      case BinaryOpIC::SMI:
        switch (result_type) {
          case BinaryOpIC::UNINITIALIZED:
          case BinaryOpIC::SMI:
            return TypeInfo::Smi();
          case BinaryOpIC::INT32:
            return TypeInfo::Integer32();
          case BinaryOpIC::HEAP_NUMBER:
            return TypeInfo::Double();
          default:
            return unknown;
        }
      case BinaryOpIC::INT32:
        if (expr->op() == Token::DIV ||
            result_type == BinaryOpIC::HEAP_NUMBER) {
          return TypeInfo::Double();
        }
        return TypeInfo::Integer32();
      case BinaryOpIC::HEAP_NUMBER:
        return TypeInfo::Double();
      case BinaryOpIC::BOTH_STRING:
        return TypeInfo::String();
      case BinaryOpIC::STRING:
      case BinaryOpIC::GENERIC:
        return unknown;
     default:
        return unknown;
    }
  }
  return unknown;
}


TypeInfo TypeFeedbackOracle::SwitchType(CaseClause* clause) {
  Handle<Object> object = GetInfo(clause->CompareId());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_compare_ic_stub()) return unknown;

  CompareIC::State state = static_cast<CompareIC::State>(code->compare_state());
  switch (state) {
    case CompareIC::UNINITIALIZED:
      // Uninitialized means never executed.
      // TODO(fschneider): Introduce a separate value for never-executed ICs.
      return unknown;
    case CompareIC::SMIS:
      return TypeInfo::Smi();
    case CompareIC::STRINGS:
      return TypeInfo::String();
    case CompareIC::SYMBOLS:
      return TypeInfo::Symbol();
    case CompareIC::HEAP_NUMBERS:
      return TypeInfo::Number();
    case CompareIC::OBJECTS:
    case CompareIC::KNOWN_OBJECTS:
      // TODO(kasperl): We really need a type for JS objects here.
      return TypeInfo::NonPrimitive();
    case CompareIC::GENERIC:
    default:
      return unknown;
  }
}


TypeInfo TypeFeedbackOracle::IncrementType(CountOperation* expr) {
  Handle<Object> object = GetInfo(expr->CountId());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_binary_op_stub()) return unknown;

  BinaryOpIC::TypeInfo type = static_cast<BinaryOpIC::TypeInfo>(
      code->binary_op_type());
  switch (type) {
    case BinaryOpIC::UNINITIALIZED:
    case BinaryOpIC::SMI:
      return TypeInfo::Smi();
    case BinaryOpIC::INT32:
      return TypeInfo::Integer32();
    case BinaryOpIC::HEAP_NUMBER:
      return TypeInfo::Double();
    case BinaryOpIC::BOTH_STRING:
    case BinaryOpIC::STRING:
    case BinaryOpIC::GENERIC:
      return unknown;
    default:
      return unknown;
  }
  UNREACHABLE();
  return unknown;
}


void TypeFeedbackOracle::CollectReceiverTypes(unsigned ast_id,
                                              Handle<String> name,
                                              Code::Flags flags,
                                              SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (object->IsUndefined() || object->IsSmi()) return;

  if (*object ==
      isolate_->builtins()->builtin(Builtins::kStoreIC_GlobalProxy)) {
    // TODO(fschneider): We could collect the maps and signal that
    // we need a generic store (or load) here.
    ASSERT(Handle<Code>::cast(object)->ic_state() == MEGAMORPHIC);
  } else if (object->IsMap()) {
    types->Add(Handle<Map>::cast(object));
  } else if (FLAG_collect_megamorphic_maps_from_stub_cache &&
      Handle<Code>::cast(object)->ic_state() == MEGAMORPHIC) {
    types->Reserve(4);
    ASSERT(object->IsCode());
    isolate_->stub_cache()->CollectMatchingMaps(types,
                                                *name,
                                                flags,
                                                global_context_);
  }
}


// Check if a map originates from a given global context. We use this
// information to filter out maps from different context to avoid
// retaining objects from different tabs in Chrome via optimized code.
bool TypeFeedbackOracle::CanRetainOtherContext(Map* map,
                                               Context* global_context) {
  Object* constructor = NULL;
  while (!map->prototype()->IsNull()) {
    constructor = map->constructor();
    if (!constructor->IsNull()) {
      // If the constructor is not null or a JSFunction, we have to
      // conservatively assume that it may retain a global context.
      if (!constructor->IsJSFunction()) return true;
      // Check if the constructor directly references a foreign context.
      if (CanRetainOtherContext(JSFunction::cast(constructor),
                                global_context)) {
        return true;
      }
    }
    map = HeapObject::cast(map->prototype())->map();
  }
  constructor = map->constructor();
  if (constructor->IsNull()) return false;
  JSFunction* function = JSFunction::cast(constructor);
  return CanRetainOtherContext(function, global_context);
}


bool TypeFeedbackOracle::CanRetainOtherContext(JSFunction* function,
                                               Context* global_context) {
  return function->context()->global() != global_context->global()
      && function->context()->global() != global_context->builtins();
}


static void AddMapIfMissing(Handle<Map> map, SmallMapList* list) {
  for (int i = 0; i < list->length(); ++i) {
    if (list->at(i).is_identical_to(map)) return;
  }
  list->Add(map);
}


void TypeFeedbackOracle::CollectKeyedReceiverTypes(unsigned ast_id,
                                                   SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (!object->IsCode()) return;
  Handle<Code> code = Handle<Code>::cast(object);
  if (code->kind() == Code::KEYED_LOAD_IC ||
      code->kind() == Code::KEYED_STORE_IC) {
    AssertNoAllocation no_allocation;
    int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
    for (RelocIterator it(*code, mask); !it.done(); it.next()) {
      RelocInfo* info = it.rinfo();
      Object* object = info->target_object();
      if (object->IsMap()) {
        Map* map = Map::cast(object);
        if (!CanRetainOtherContext(map, *global_context_)) {
          AddMapIfMissing(Handle<Map>(map), types);
        }
      }
    }
  }
}


byte TypeFeedbackOracle::ToBooleanTypes(unsigned ast_id) {
  Handle<Object> object = GetInfo(ast_id);
  return object->IsCode() ? Handle<Code>::cast(object)->to_boolean_state() : 0;
}


// Things are a bit tricky here: The iterator for the RelocInfos and the infos
// themselves are not GC-safe, so we first get all infos, then we create the
// dictionary (possibly triggering GC), and finally we relocate the collected
// infos before we process them.
void TypeFeedbackOracle::BuildDictionary(Handle<Code> code) {
  AssertNoAllocation no_allocation;
  ZoneList<RelocInfo> infos(16);
  HandleScope scope;
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
    infos->Add(*it.rinfo());
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
    unsigned ast_id = static_cast<unsigned>((*infos)[i].data());
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
                                              *global_context_)) {
              SetInfo(ast_id, map);
            }
          }
        } else if (target->ic_state() == MEGAMORPHIC) {
          SetInfo(ast_id, target);
        }
        break;

      case Code::KEYED_LOAD_IC:
      case Code::KEYED_STORE_IC:
        if (target->ic_state() == MONOMORPHIC ||
            target->ic_state() == MEGAMORPHIC) {
          SetInfo(ast_id, target);
        }
        break;

      case Code::UNARY_OP_IC:
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


void TypeFeedbackOracle::ProcessTypeFeedbackCells(Handle<Code> code) {
  Object* raw_info = code->type_feedback_info();
  if (!raw_info->IsTypeFeedbackInfo()) return;
  Handle<TypeFeedbackCells> cache(
      TypeFeedbackInfo::cast(raw_info)->type_feedback_cells());
  for (int i = 0; i < cache->CellCount(); i++) {
    unsigned ast_id = cache->AstId(i)->value();
    Object* value = cache->Cell(i)->value();
    if (value->IsJSFunction() &&
        !CanRetainOtherContext(JSFunction::cast(value),
                               *global_context_)) {
      SetInfo(ast_id, value);
    }
  }
}


void TypeFeedbackOracle::SetInfo(unsigned ast_id, Object* target) {
  ASSERT(dictionary_->FindEntry(ast_id) == UnseededNumberDictionary::kNotFound);
  MaybeObject* maybe_result = dictionary_->AtNumberPut(ast_id, target);
  USE(maybe_result);
#ifdef DEBUG
  Object* result = NULL;
  // Dictionary has been allocated with sufficient size for all elements.
  ASSERT(maybe_result->ToObject(&result));
  ASSERT(*dictionary_ == result);
#endif
}

} }  // namespace v8::internal
