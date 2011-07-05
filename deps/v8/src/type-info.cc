// Copyright 2010 the V8 project authors. All rights reserved.
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


STATIC_ASSERT(DEFAULT_STRING_STUB == Code::kNoExtraICState);


TypeFeedbackOracle::TypeFeedbackOracle(Handle<Code> code,
                                       Handle<Context> global_context) {
  global_context_ = global_context;
  Initialize(code);
}


void TypeFeedbackOracle::Initialize(Handle<Code> code) {
  ASSERT(map_.is_null());  // Only initialize once.
  map_ = Factory::NewJSObject(Top::object_function());
  PopulateMap(code);
}


bool TypeFeedbackOracle::LoadIsMonomorphic(Property* expr) {
  return GetElement(map_, expr->position())->IsMap();
}


bool TypeFeedbackOracle:: StoreIsMonomorphic(Assignment* expr) {
  return GetElement(map_, expr->position())->IsMap();
}


bool TypeFeedbackOracle::CallIsMonomorphic(Call* expr) {
  Handle<Object> value = GetElement(map_, expr->position());
  return value->IsMap() || value->IsSmi();
}


Handle<Map> TypeFeedbackOracle::LoadMonomorphicReceiverType(Property* expr) {
  ASSERT(LoadIsMonomorphic(expr));
  return Handle<Map>::cast(GetElement(map_, expr->position()));
}


Handle<Map> TypeFeedbackOracle::StoreMonomorphicReceiverType(Assignment* expr) {
  ASSERT(StoreIsMonomorphic(expr));
  return Handle<Map>::cast(GetElement(map_, expr->position()));
}


ZoneMapList* TypeFeedbackOracle::LoadReceiverTypes(Property* expr,
                                                   Handle<String> name) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, NORMAL);
  return CollectReceiverTypes(expr->position(), name, flags);
}


ZoneMapList* TypeFeedbackOracle::StoreReceiverTypes(Assignment* expr,
                                                    Handle<String> name) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::STORE_IC, NORMAL);
  return CollectReceiverTypes(expr->position(), name, flags);
}


ZoneMapList* TypeFeedbackOracle::CallReceiverTypes(Call* expr,
                                                   Handle<String> name) {
  int arity = expr->arguments()->length();
  // Note: these flags won't let us get maps from stubs with
  // non-default extra ic state in the megamorphic case. In the more
  // important monomorphic case the map is obtained directly, so it's
  // not a problem until we decide to emit more polymorphic code.
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::CALL_IC,
                                                    NORMAL,
                                                    Code::kNoExtraICState,
                                                    OWN_MAP,
                                                    NOT_IN_LOOP,
                                                    arity);
  return CollectReceiverTypes(expr->position(), name, flags);
}


CheckType TypeFeedbackOracle::GetCallCheckType(Call* expr) {
  Handle<Object> value = GetElement(map_, expr->position());
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


bool TypeFeedbackOracle::LoadIsBuiltin(Property* expr, Builtins::Name id) {
  Handle<Object> object = GetElement(map_, expr->position());
  return *object == Builtins::builtin(id);
}


TypeInfo TypeFeedbackOracle::CompareType(CompareOperation* expr) {
  Handle<Object> object = GetElement(map_, expr->position());
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
    case CompareIC::HEAP_NUMBERS:
      return TypeInfo::Number();
    case CompareIC::OBJECTS:
      // TODO(kasperl): We really need a type for JS objects here.
      return TypeInfo::NonPrimitive();
    case CompareIC::GENERIC:
    default:
      return unknown;
  }
}


TypeInfo TypeFeedbackOracle::BinaryType(BinaryOperation* expr) {
  Handle<Object> object = GetElement(map_, expr->position());
  TypeInfo unknown = TypeInfo::Unknown();
  if (!object->IsCode()) return unknown;
  Handle<Code> code = Handle<Code>::cast(object);
  if (code->is_binary_op_stub()) {
    BinaryOpIC::TypeInfo type = static_cast<BinaryOpIC::TypeInfo>(
        code->binary_op_type());
    switch (type) {
      case BinaryOpIC::UNINIT_OR_SMI:
        return TypeInfo::Smi();
      case BinaryOpIC::DEFAULT:
        return (expr->op() == Token::DIV || expr->op() == Token::MUL)
            ? TypeInfo::Double()
            : TypeInfo::Integer32();
      case BinaryOpIC::HEAP_NUMBERS:
        return TypeInfo::Double();
      default:
        return unknown;
    }
  } else if (code->is_type_recording_binary_op_stub()) {
    TRBinaryOpIC::TypeInfo type = static_cast<TRBinaryOpIC::TypeInfo>(
        code->type_recording_binary_op_type());
    TRBinaryOpIC::TypeInfo result_type = static_cast<TRBinaryOpIC::TypeInfo>(
        code->type_recording_binary_op_result_type());

    switch (type) {
      case TRBinaryOpIC::UNINITIALIZED:
        // Uninitialized means never executed.
        // TODO(fschneider): Introduce a separate value for never-executed ICs
        return unknown;
      case TRBinaryOpIC::SMI:
        switch (result_type) {
          case TRBinaryOpIC::UNINITIALIZED:
          case TRBinaryOpIC::SMI:
            return TypeInfo::Smi();
          case TRBinaryOpIC::INT32:
            return TypeInfo::Integer32();
          case TRBinaryOpIC::HEAP_NUMBER:
            return TypeInfo::Double();
          default:
            return unknown;
        }
      case TRBinaryOpIC::INT32:
        if (expr->op() == Token::DIV ||
            result_type == TRBinaryOpIC::HEAP_NUMBER) {
          return TypeInfo::Double();
        }
        return TypeInfo::Integer32();
      case TRBinaryOpIC::HEAP_NUMBER:
        return TypeInfo::Double();
      case TRBinaryOpIC::STRING:
      case TRBinaryOpIC::GENERIC:
        return unknown;
     default:
        return unknown;
    }
  }
  return unknown;
}


TypeInfo TypeFeedbackOracle::SwitchType(CaseClause* clause) {
  Handle<Object> object = GetElement(map_, clause->position());
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
    case CompareIC::HEAP_NUMBERS:
      return TypeInfo::Number();
    case CompareIC::OBJECTS:
      // TODO(kasperl): We really need a type for JS objects here.
      return TypeInfo::NonPrimitive();
    case CompareIC::GENERIC:
    default:
      return unknown;
  }
}


ZoneMapList* TypeFeedbackOracle::CollectReceiverTypes(int position,
                                                      Handle<String> name,
                                                      Code::Flags flags) {
  Handle<Object> object = GetElement(map_, position);
  if (object->IsUndefined() || object->IsSmi()) return NULL;

  if (*object == Builtins::builtin(Builtins::StoreIC_GlobalProxy)) {
    // TODO(fschneider): We could collect the maps and signal that
    // we need a generic store (or load) here.
    ASSERT(Handle<Code>::cast(object)->ic_state() == MEGAMORPHIC);
    return NULL;
  } else if (object->IsMap()) {
    ZoneMapList* types = new ZoneMapList(1);
    types->Add(Handle<Map>::cast(object));
    return types;
  } else if (Handle<Code>::cast(object)->ic_state() == MEGAMORPHIC) {
    ZoneMapList* types = new ZoneMapList(4);
    ASSERT(object->IsCode());
    StubCache::CollectMatchingMaps(types, *name, flags);
    return types->length() > 0 ? types : NULL;
  } else {
    return NULL;
  }
}


void TypeFeedbackOracle::PopulateMap(Handle<Code> code) {
  HandleScope scope;

  const int kInitialCapacity = 16;
  List<int> code_positions(kInitialCapacity);
  List<int> source_positions(kInitialCapacity);
  CollectPositions(*code, &code_positions, &source_positions);

  int length = code_positions.length();
  ASSERT(source_positions.length() == length);
  for (int i = 0; i < length; i++) {
    RelocInfo info(code->instruction_start() + code_positions[i],
                   RelocInfo::CODE_TARGET, 0);
    Handle<Code> target(Code::GetCodeFromTargetAddress(info.target_address()));
    int position = source_positions[i];
    InlineCacheState state = target->ic_state();
    Code::Kind kind = target->kind();
    if (kind == Code::BINARY_OP_IC ||
        kind == Code::TYPE_RECORDING_BINARY_OP_IC ||
        kind == Code::COMPARE_IC) {
      // TODO(kasperl): Avoid having multiple ICs with the same
      // position by making sure that we have position information
      // recorded for all binary ICs.
      if (GetElement(map_, position)->IsUndefined()) {
        SetElement(map_, position, target);
      }
    } else if (state == MONOMORPHIC) {
      if (target->kind() != Code::CALL_IC ||
          target->check_type() == RECEIVER_MAP_CHECK) {
        Handle<Map> map = Handle<Map>(target->FindFirstMap());
        if (*map == NULL) {
          SetElement(map_, position, target);
        } else {
          SetElement(map_, position, map);
        }
      } else {
        ASSERT(target->kind() == Code::CALL_IC);
        CheckType check = target->check_type();
        ASSERT(check != RECEIVER_MAP_CHECK);
        SetElement(map_, position, Handle<Object>(Smi::FromInt(check)));
        ASSERT(Smi::cast(*GetElement(map_, position))->value() == check);
      }
    } else if (state == MEGAMORPHIC) {
      SetElement(map_, position, target);
    }
  }
}


void TypeFeedbackOracle::CollectPositions(Code* code,
                                          List<int>* code_positions,
                                          List<int>* source_positions) {
  AssertNoAllocation no_allocation;
  int position = 0;
  // Because the ICs we use for global variables access in the full
  // code generator do not have any meaningful positions, we avoid
  // collecting those by filtering out contextual code targets.
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
      RelocInfo::kPositionMask;
  for (RelocIterator it(code, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    RelocInfo::Mode mode = info->rmode();
    if (RelocInfo::IsCodeTarget(mode)) {
      Code* target = Code::GetCodeFromTargetAddress(info->target_address());
      if (target->is_inline_cache_stub()) {
        InlineCacheState state = target->ic_state();
        Code::Kind kind = target->kind();
        if (kind == Code::BINARY_OP_IC) {
          if (target->binary_op_type() == BinaryOpIC::GENERIC) continue;
        } else if (kind == Code::TYPE_RECORDING_BINARY_OP_IC) {
          if (target->type_recording_binary_op_type() ==
              TRBinaryOpIC::GENERIC) {
            continue;
          }
        } else if (kind == Code::COMPARE_IC) {
          if (target->compare_state() == CompareIC::GENERIC) continue;
        } else {
          if (state != MONOMORPHIC && state != MEGAMORPHIC) continue;
        }
        code_positions->Add(
            static_cast<int>(info->pc() - code->instruction_start()));
        source_positions->Add(position);
      }
    } else {
      ASSERT(RelocInfo::IsPosition(mode));
      position = static_cast<int>(info->data());
    }
  }
}

} }  // namespace v8::internal
