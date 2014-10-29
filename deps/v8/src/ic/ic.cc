// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/accessors.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/base/bits.h"
#include "src/codegen.h"
#include "src/conversions.h"
#include "src/execution.h"
#include "src/ic/call-optimization.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic-inl.h"
#include "src/ic/ic-compiler.h"
#include "src/ic/stub-cache.h"
#include "src/prototype.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

char IC::TransitionMarkFromState(IC::State state) {
  switch (state) {
    case UNINITIALIZED:
      return '0';
    case PREMONOMORPHIC:
      return '.';
    case MONOMORPHIC:
      return '1';
    case PROTOTYPE_FAILURE:
      return '^';
    case POLYMORPHIC:
      return 'P';
    case MEGAMORPHIC:
      return 'N';
    case GENERIC:
      return 'G';

    // We never see the debugger states here, because the state is
    // computed from the original code - not the patched code. Let
    // these cases fall through to the unreachable code below.
    case DEBUG_STUB:
      break;
    // Type-vector-based ICs resolve state to one of the above.
    case DEFAULT:
      break;
  }
  UNREACHABLE();
  return 0;
}


const char* GetTransitionMarkModifier(KeyedAccessStoreMode mode) {
  if (mode == STORE_NO_TRANSITION_HANDLE_COW) return ".COW";
  if (mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
    return ".IGNORE_OOB";
  }
  if (IsGrowStoreMode(mode)) return ".GROW";
  return "";
}


#ifdef DEBUG

#define TRACE_GENERIC_IC(isolate, type, reason)                \
  do {                                                         \
    if (FLAG_trace_ic) {                                       \
      PrintF("[%s patching generic stub in ", type);           \
      JavaScriptFrame::PrintTop(isolate, stdout, false, true); \
      PrintF(" (%s)]\n", reason);                              \
    }                                                          \
  } while (false)

#else

#define TRACE_GENERIC_IC(isolate, type, reason)      \
  do {                                               \
    if (FLAG_trace_ic) {                             \
      PrintF("[%s patching generic stub in ", type); \
      PrintF("(see below) (%s)]\n", reason);         \
    }                                                \
  } while (false)

#endif  // DEBUG


void IC::TraceIC(const char* type, Handle<Object> name) {
  if (FLAG_trace_ic) {
    Code* new_target = raw_target();
    State new_state = new_target->ic_state();
    TraceIC(type, name, state(), new_state);
  }
}


void IC::TraceIC(const char* type, Handle<Object> name, State old_state,
                 State new_state) {
  if (FLAG_trace_ic) {
    Code* new_target = raw_target();
    PrintF("[%s%s in ", new_target->is_keyed_stub() ? "Keyed" : "", type);

    // TODO(jkummerow): Add support for "apply". The logic is roughly:
    // marker = [fp_ + kMarkerOffset];
    // if marker is smi and marker.value == INTERNAL and
    //     the frame's code == builtin(Builtins::kFunctionApply):
    // then print "apply from" and advance one frame

    Object* maybe_function =
        Memory::Object_at(fp_ + JavaScriptFrameConstants::kFunctionOffset);
    if (maybe_function->IsJSFunction()) {
      JSFunction* function = JSFunction::cast(maybe_function);
      JavaScriptFrame::PrintFunctionAndOffset(function, function->code(), pc(),
                                              stdout, true);
    }

    ExtraICState extra_state = new_target->extra_ic_state();
    const char* modifier = "";
    if (new_target->kind() == Code::KEYED_STORE_IC) {
      modifier = GetTransitionMarkModifier(
          KeyedStoreIC::GetKeyedAccessStoreMode(extra_state));
    }
    PrintF(" (%c->%c%s)", TransitionMarkFromState(old_state),
           TransitionMarkFromState(new_state), modifier);
#ifdef OBJECT_PRINT
    OFStream os(stdout);
    name->Print(os);
#else
    name->ShortPrint(stdout);
#endif
    PrintF("]\n");
  }
}

#define TRACE_IC(type, name) TraceIC(type, name)
#define TRACE_VECTOR_IC(type, name, old_state, new_state) \
  TraceIC(type, name, old_state, new_state)

IC::IC(FrameDepth depth, Isolate* isolate)
    : isolate_(isolate), target_set_(false), target_maps_set_(false) {
  // To improve the performance of the (much used) IC code, we unfold a few
  // levels of the stack frame iteration code. This yields a ~35% speedup when
  // running DeltaBlue and a ~25% speedup of gbemu with the '--nouse-ic' flag.
  const Address entry = Isolate::c_entry_fp(isolate->thread_local_top());
  Address constant_pool = NULL;
  if (FLAG_enable_ool_constant_pool) {
    constant_pool =
        Memory::Address_at(entry + ExitFrameConstants::kConstantPoolOffset);
  }
  Address* pc_address =
      reinterpret_cast<Address*>(entry + ExitFrameConstants::kCallerPCOffset);
  Address fp = Memory::Address_at(entry + ExitFrameConstants::kCallerFPOffset);
  // If there's another JavaScript frame on the stack or a
  // StubFailureTrampoline, we need to look one frame further down the stack to
  // find the frame pointer and the return address stack slot.
  if (depth == EXTRA_CALL_FRAME) {
    if (FLAG_enable_ool_constant_pool) {
      constant_pool =
          Memory::Address_at(fp + StandardFrameConstants::kConstantPoolOffset);
    }
    const int kCallerPCOffset = StandardFrameConstants::kCallerPCOffset;
    pc_address = reinterpret_cast<Address*>(fp + kCallerPCOffset);
    fp = Memory::Address_at(fp + StandardFrameConstants::kCallerFPOffset);
  }
#ifdef DEBUG
  StackFrameIterator it(isolate);
  for (int i = 0; i < depth + 1; i++) it.Advance();
  StackFrame* frame = it.frame();
  DCHECK(fp == frame->fp() && pc_address == frame->pc_address());
#endif
  fp_ = fp;
  if (FLAG_enable_ool_constant_pool) {
    raw_constant_pool_ = handle(
        ConstantPoolArray::cast(reinterpret_cast<Object*>(constant_pool)),
        isolate);
  }
  pc_address_ = StackFrame::ResolveReturnAddressLocation(pc_address);
  target_ = handle(raw_target(), isolate);
  state_ = target_->ic_state();
  kind_ = target_->kind();
  extra_ic_state_ = target_->extra_ic_state();
}


SharedFunctionInfo* IC::GetSharedFunctionInfo() const {
  // Compute the JavaScript frame for the frame pointer of this IC
  // structure. We need this to be able to find the function
  // corresponding to the frame.
  StackFrameIterator it(isolate());
  while (it.frame()->fp() != this->fp()) it.Advance();
  JavaScriptFrame* frame = JavaScriptFrame::cast(it.frame());
  // Find the function on the stack and both the active code for the
  // function and the original code.
  JSFunction* function = frame->function();
  return function->shared();
}


Code* IC::GetCode() const {
  HandleScope scope(isolate());
  Handle<SharedFunctionInfo> shared(GetSharedFunctionInfo(), isolate());
  Code* code = shared->code();
  return code;
}


Code* IC::GetOriginalCode() const {
  HandleScope scope(isolate());
  Handle<SharedFunctionInfo> shared(GetSharedFunctionInfo(), isolate());
  DCHECK(Debug::HasDebugInfo(shared));
  Code* original_code = Debug::GetDebugInfo(shared)->original_code();
  DCHECK(original_code->IsCode());
  return original_code;
}


static void LookupForRead(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return;
      case LookupIterator::INTERCEPTOR: {
        // If there is a getter, return; otherwise loop to perform the lookup.
        Handle<JSObject> holder = it->GetHolder<JSObject>();
        if (!holder->GetNamedInterceptor()->getter()->IsUndefined()) {
          return;
        }
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        // PropertyHandlerCompiler::CheckPrototypes() knows how to emit
        // access checks for global proxies.
        if (it->GetHolder<JSObject>()->IsJSGlobalProxy() &&
            it->HasAccess(v8::ACCESS_GET)) {
          break;
        }
        return;
      case LookupIterator::ACCESSOR:
      case LookupIterator::DATA:
        return;
    }
  }
}


bool IC::TryRemoveInvalidPrototypeDependentStub(Handle<Object> receiver,
                                                Handle<String> name) {
  if (!IsNameCompatibleWithPrototypeFailure(name)) return false;
  Handle<Map> receiver_map = TypeToMap(*receiver_type(), isolate());
  maybe_handler_ = target()->FindHandlerForMap(*receiver_map);

  // The current map wasn't handled yet. There's no reason to stay monomorphic,
  // *unless* we're moving from a deprecated map to its replacement, or
  // to a more general elements kind.
  // TODO(verwaest): Check if the current map is actually what the old map
  // would transition to.
  if (maybe_handler_.is_null()) {
    if (!receiver_map->IsJSObjectMap()) return false;
    Map* first_map = FirstTargetMap();
    if (first_map == NULL) return false;
    Handle<Map> old_map(first_map);
    if (old_map->is_deprecated()) return true;
    if (IsMoreGeneralElementsKindTransition(old_map->elements_kind(),
                                            receiver_map->elements_kind())) {
      return true;
    }
    return false;
  }

  CacheHolderFlag flag;
  Handle<Map> ic_holder_map(
      GetICCacheHolder(*receiver_type(), isolate(), &flag));

  DCHECK(flag != kCacheOnReceiver || receiver->IsJSObject());
  DCHECK(flag != kCacheOnPrototype || !receiver->IsJSReceiver());
  DCHECK(flag != kCacheOnPrototypeReceiverIsDictionary);

  if (state() == MONOMORPHIC) {
    int index = ic_holder_map->IndexInCodeCache(*name, *target());
    if (index >= 0) {
      ic_holder_map->RemoveFromCodeCache(*name, *target(), index);
    }
  }

  if (receiver->IsGlobalObject()) {
    Handle<GlobalObject> global = Handle<GlobalObject>::cast(receiver);
    LookupIterator it(global, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
    if (it.state() == LookupIterator::ACCESS_CHECK) return false;
    if (!it.IsFound()) return false;
    Handle<PropertyCell> cell = it.GetPropertyCell();
    return cell->type()->IsConstant();
  }

  return true;
}


bool IC::IsNameCompatibleWithPrototypeFailure(Handle<Object> name) {
  if (target()->is_keyed_stub()) {
    // Determine whether the failure is due to a name failure.
    if (!name->IsName()) return false;
    Name* stub_name = target()->FindFirstName();
    if (*name != stub_name) return false;
  }

  return true;
}


void IC::UpdateState(Handle<Object> receiver, Handle<Object> name) {
  update_receiver_type(receiver);
  if (!name->IsString()) return;
  if (state() != MONOMORPHIC && state() != POLYMORPHIC) return;
  if (receiver->IsUndefined() || receiver->IsNull()) return;

  // Remove the target from the code cache if it became invalid
  // because of changes in the prototype chain to avoid hitting it
  // again.
  if (TryRemoveInvalidPrototypeDependentStub(receiver,
                                             Handle<String>::cast(name))) {
    MarkPrototypeFailure(name);
    return;
  }

  // The builtins object is special.  It only changes when JavaScript
  // builtins are loaded lazily.  It is important to keep inline
  // caches for the builtins object monomorphic.  Therefore, if we get
  // an inline cache miss for the builtins object after lazily loading
  // JavaScript builtins, we return uninitialized as the state to
  // force the inline cache back to monomorphic state.
  if (receiver->IsJSBuiltinsObject()) state_ = UNINITIALIZED;
}


MaybeHandle<Object> IC::TypeError(const char* type, Handle<Object> object,
                                  Handle<Object> key) {
  HandleScope scope(isolate());
  Handle<Object> args[2] = {key, object};
  THROW_NEW_ERROR(isolate(), NewTypeError(type, HandleVector(args, 2)), Object);
}


MaybeHandle<Object> IC::ReferenceError(const char* type, Handle<Name> name) {
  HandleScope scope(isolate());
  THROW_NEW_ERROR(isolate(), NewReferenceError(type, HandleVector(&name, 1)),
                  Object);
}


static void ComputeTypeInfoCountDelta(IC::State old_state, IC::State new_state,
                                      int* polymorphic_delta,
                                      int* generic_delta) {
  switch (old_state) {
    case UNINITIALIZED:
    case PREMONOMORPHIC:
      if (new_state == UNINITIALIZED || new_state == PREMONOMORPHIC) break;
      if (new_state == MONOMORPHIC || new_state == POLYMORPHIC) {
        *polymorphic_delta = 1;
      } else if (new_state == MEGAMORPHIC || new_state == GENERIC) {
        *generic_delta = 1;
      }
      break;
    case MONOMORPHIC:
    case POLYMORPHIC:
      if (new_state == MONOMORPHIC || new_state == POLYMORPHIC) break;
      *polymorphic_delta = -1;
      if (new_state == MEGAMORPHIC || new_state == GENERIC) {
        *generic_delta = 1;
      }
      break;
    case MEGAMORPHIC:
    case GENERIC:
      if (new_state == MEGAMORPHIC || new_state == GENERIC) break;
      *generic_delta = -1;
      if (new_state == MONOMORPHIC || new_state == POLYMORPHIC) {
        *polymorphic_delta = 1;
      }
      break;
    case PROTOTYPE_FAILURE:
    case DEBUG_STUB:
    case DEFAULT:
      UNREACHABLE();
  }
}


void IC::OnTypeFeedbackChanged(Isolate* isolate, Address address,
                               State old_state, State new_state,
                               bool target_remains_ic_stub) {
  Code* host =
      isolate->inner_pointer_to_code_cache()->GetCacheEntry(address)->code;
  if (host->kind() != Code::FUNCTION) return;

  if (FLAG_type_info_threshold > 0 && target_remains_ic_stub &&
      // Not all Code objects have TypeFeedbackInfo.
      host->type_feedback_info()->IsTypeFeedbackInfo()) {
    int polymorphic_delta = 0;  // "Polymorphic" here includes monomorphic.
    int generic_delta = 0;      // "Generic" here includes megamorphic.
    ComputeTypeInfoCountDelta(old_state, new_state, &polymorphic_delta,
                              &generic_delta);
    TypeFeedbackInfo* info = TypeFeedbackInfo::cast(host->type_feedback_info());
    info->change_ic_with_type_info_count(polymorphic_delta);
    info->change_ic_generic_count(generic_delta);
  }
  if (host->type_feedback_info()->IsTypeFeedbackInfo()) {
    TypeFeedbackInfo* info = TypeFeedbackInfo::cast(host->type_feedback_info());
    info->change_own_type_change_checksum();
  }
  host->set_profiler_ticks(0);
  isolate->runtime_profiler()->NotifyICChanged();
  // TODO(2029): When an optimized function is patched, it would
  // be nice to propagate the corresponding type information to its
  // unoptimized version for the benefit of later inlining.
}


void IC::PostPatching(Address address, Code* target, Code* old_target) {
  // Type vector based ICs update these statistics at a different time because
  // they don't always patch on state change.
  if (target->kind() == Code::CALL_IC) return;

  Isolate* isolate = target->GetHeap()->isolate();
  State old_state = UNINITIALIZED;
  State new_state = UNINITIALIZED;
  bool target_remains_ic_stub = false;
  if (old_target->is_inline_cache_stub() && target->is_inline_cache_stub()) {
    old_state = old_target->ic_state();
    new_state = target->ic_state();
    target_remains_ic_stub = true;
  }

  OnTypeFeedbackChanged(isolate, address, old_state, new_state,
                        target_remains_ic_stub);
}


void IC::RegisterWeakMapDependency(Handle<Code> stub) {
  if (FLAG_collect_maps && FLAG_weak_embedded_maps_in_ic &&
      stub->CanBeWeakStub()) {
    DCHECK(!stub->is_weak_stub());
    MapHandleList maps;
    stub->FindAllMaps(&maps);
    if (maps.length() == 1 && stub->IsWeakObjectInIC(*maps.at(0))) {
      Map::AddDependentIC(maps.at(0), stub);
      stub->mark_as_weak_stub();
      if (FLAG_enable_ool_constant_pool) {
        stub->constant_pool()->set_weak_object_state(
            ConstantPoolArray::WEAK_OBJECTS_IN_IC);
      }
    }
  }
}


void IC::InvalidateMaps(Code* stub) {
  DCHECK(stub->is_weak_stub());
  stub->mark_as_invalidated_weak_stub();
  Isolate* isolate = stub->GetIsolate();
  Heap* heap = isolate->heap();
  Object* undefined = heap->undefined_value();
  int mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(stub, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT &&
        it.rinfo()->target_object()->IsMap()) {
      it.rinfo()->set_target_object(undefined, SKIP_WRITE_BARRIER);
    }
  }
  CpuFeatures::FlushICache(stub->instruction_start(), stub->instruction_size());
}


void IC::Clear(Isolate* isolate, Address address,
               ConstantPoolArray* constant_pool) {
  Code* target = GetTargetAtAddress(address, constant_pool);

  // Don't clear debug break inline cache as it will remove the break point.
  if (target->is_debug_stub()) return;

  switch (target->kind()) {
    case Code::LOAD_IC:
      return LoadIC::Clear(isolate, address, target, constant_pool);
    case Code::KEYED_LOAD_IC:
      return KeyedLoadIC::Clear(isolate, address, target, constant_pool);
    case Code::STORE_IC:
      return StoreIC::Clear(isolate, address, target, constant_pool);
    case Code::KEYED_STORE_IC:
      return KeyedStoreIC::Clear(isolate, address, target, constant_pool);
    case Code::CALL_IC:
      return CallIC::Clear(isolate, address, target, constant_pool);
    case Code::COMPARE_IC:
      return CompareIC::Clear(isolate, address, target, constant_pool);
    case Code::COMPARE_NIL_IC:
      return CompareNilIC::Clear(address, target, constant_pool);
    case Code::BINARY_OP_IC:
    case Code::TO_BOOLEAN_IC:
      // Clearing these is tricky and does not
      // make any performance difference.
      return;
    default:
      UNREACHABLE();
  }
}


void KeyedLoadIC::Clear(Isolate* isolate, Address address, Code* target,
                        ConstantPoolArray* constant_pool) {
  if (IsCleared(target)) return;
  // Make sure to also clear the map used in inline fast cases.  If we
  // do not clear these maps, cached code can keep objects alive
  // through the embedded maps.
  SetTargetAtAddress(address, *pre_monomorphic_stub(isolate), constant_pool);
}


void CallIC::Clear(Isolate* isolate, Address address, Code* target,
                   ConstantPoolArray* constant_pool) {
  // Currently, CallIC doesn't have state changes.
}


void LoadIC::Clear(Isolate* isolate, Address address, Code* target,
                   ConstantPoolArray* constant_pool) {
  if (IsCleared(target)) return;
  Code* code = PropertyICCompiler::FindPreMonomorphic(isolate, Code::LOAD_IC,
                                                      target->extra_ic_state());
  SetTargetAtAddress(address, code, constant_pool);
}


void StoreIC::Clear(Isolate* isolate, Address address, Code* target,
                    ConstantPoolArray* constant_pool) {
  if (IsCleared(target)) return;
  Code* code = PropertyICCompiler::FindPreMonomorphic(isolate, Code::STORE_IC,
                                                      target->extra_ic_state());
  SetTargetAtAddress(address, code, constant_pool);
}


void KeyedStoreIC::Clear(Isolate* isolate, Address address, Code* target,
                         ConstantPoolArray* constant_pool) {
  if (IsCleared(target)) return;
  SetTargetAtAddress(
      address, *pre_monomorphic_stub(
                   isolate, StoreIC::GetStrictMode(target->extra_ic_state())),
      constant_pool);
}


void CompareIC::Clear(Isolate* isolate, Address address, Code* target,
                      ConstantPoolArray* constant_pool) {
  DCHECK(CodeStub::GetMajorKey(target) == CodeStub::CompareIC);
  CompareICStub stub(target->stub_key(), isolate);
  // Only clear CompareICs that can retain objects.
  if (stub.state() != CompareICState::KNOWN_OBJECT) return;
  SetTargetAtAddress(address, GetRawUninitialized(isolate, stub.op()),
                     constant_pool);
  PatchInlinedSmiCode(address, DISABLE_INLINED_SMI_CHECK);
}


// static
Handle<Code> KeyedLoadIC::generic_stub(Isolate* isolate) {
  if (FLAG_compiled_keyed_generic_loads) {
    return KeyedLoadGenericStub(isolate).GetCode();
  } else {
    return isolate->builtins()->KeyedLoadIC_Generic();
  }
}


static bool MigrateDeprecated(Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  if (!receiver->map()->is_deprecated()) return false;
  JSObject::MigrateInstance(Handle<JSObject>::cast(object));
  return true;
}


MaybeHandle<Object> LoadIC::Load(Handle<Object> object, Handle<Name> name) {
  // If the object is undefined or null it's illegal to try to get any
  // of its properties; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_load", object, name);
  }

  // Check if the name is trivially convertible to an index and get
  // the element or char if so.
  uint32_t index;
  if (kind() == Code::KEYED_LOAD_IC && name->AsArrayIndex(&index)) {
    // Rewrite to the generic keyed load stub.
    if (FLAG_use_ic) {
      set_target(*KeyedLoadIC::generic_stub(isolate()));
      TRACE_IC("LoadIC", name);
      TRACE_GENERIC_IC(isolate(), "LoadIC", "name as array index");
    }
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Runtime::GetElementOrCharAt(isolate(), object, index), Object);
    return result;
  }

  bool use_ic = MigrateDeprecated(object) ? false : FLAG_use_ic;

  // Named lookup in the object.
  LookupIterator it(object, name);
  LookupForRead(&it);

  if (it.IsFound() || !IsUndeclaredGlobal(object)) {
    // Update inline cache and stub cache.
    if (use_ic) UpdateCaches(&it);

    // Get the property.
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), result, Object::GetProperty(&it),
                               Object);
    if (it.IsFound()) {
      return result;
    } else if (!IsUndeclaredGlobal(object)) {
      LOG(isolate(), SuspectReadEvent(*name, *object));
      return result;
    }
  }
  return ReferenceError("not_defined", name);
}


static bool AddOneReceiverMapIfMissing(MapHandleList* receiver_maps,
                                       Handle<Map> new_receiver_map) {
  DCHECK(!new_receiver_map.is_null());
  for (int current = 0; current < receiver_maps->length(); ++current) {
    if (!receiver_maps->at(current).is_null() &&
        receiver_maps->at(current).is_identical_to(new_receiver_map)) {
      return false;
    }
  }
  receiver_maps->Add(new_receiver_map);
  return true;
}


bool IC::UpdatePolymorphicIC(Handle<Name> name, Handle<Code> code) {
  if (!code->is_handler()) return false;
  if (target()->is_keyed_stub() && state() != PROTOTYPE_FAILURE) return false;
  Handle<HeapType> type = receiver_type();
  TypeHandleList types;
  CodeHandleList handlers;

  TargetTypes(&types);
  int number_of_types = types.length();
  int deprecated_types = 0;
  int handler_to_overwrite = -1;

  for (int i = 0; i < number_of_types; i++) {
    Handle<HeapType> current_type = types.at(i);
    if (current_type->IsClass() &&
        current_type->AsClass()->Map()->is_deprecated()) {
      // Filter out deprecated maps to ensure their instances get migrated.
      ++deprecated_types;
    } else if (type->NowIs(current_type)) {
      // If the receiver type is already in the polymorphic IC, this indicates
      // there was a prototoype chain failure. In that case, just overwrite the
      // handler.
      handler_to_overwrite = i;
    } else if (handler_to_overwrite == -1 && current_type->IsClass() &&
               type->IsClass() &&
               IsTransitionOfMonomorphicTarget(*current_type->AsClass()->Map(),
                                               *type->AsClass()->Map())) {
      handler_to_overwrite = i;
    }
  }

  int number_of_valid_types =
      number_of_types - deprecated_types - (handler_to_overwrite != -1);

  if (number_of_valid_types >= 4) return false;
  if (number_of_types == 0) return false;
  if (!target()->FindHandlers(&handlers, types.length())) return false;

  number_of_valid_types++;
  if (number_of_valid_types > 1 && target()->is_keyed_stub()) return false;
  Handle<Code> ic;
  if (number_of_valid_types == 1) {
    ic = PropertyICCompiler::ComputeMonomorphic(kind(), name, type, code,
                                                extra_ic_state());
  } else {
    if (handler_to_overwrite >= 0) {
      handlers.Set(handler_to_overwrite, code);
      if (!type->NowIs(types.at(handler_to_overwrite))) {
        types.Set(handler_to_overwrite, type);
      }
    } else {
      types.Add(type);
      handlers.Add(code);
    }
    ic = PropertyICCompiler::ComputePolymorphic(kind(), &types, &handlers,
                                                number_of_valid_types, name,
                                                extra_ic_state());
  }
  set_target(*ic);
  return true;
}


Handle<HeapType> IC::CurrentTypeOf(Handle<Object> object, Isolate* isolate) {
  return object->IsJSGlobalObject()
             ? HeapType::Constant(Handle<JSGlobalObject>::cast(object), isolate)
             : HeapType::NowOf(object, isolate);
}


Handle<Map> IC::TypeToMap(HeapType* type, Isolate* isolate) {
  if (type->Is(HeapType::Number()))
    return isolate->factory()->heap_number_map();
  if (type->Is(HeapType::Boolean())) return isolate->factory()->boolean_map();
  if (type->IsConstant()) {
    return handle(
        Handle<JSGlobalObject>::cast(type->AsConstant()->Value())->map());
  }
  DCHECK(type->IsClass());
  return type->AsClass()->Map();
}


template <class T>
typename T::TypeHandle IC::MapToType(Handle<Map> map,
                                     typename T::Region* region) {
  if (map->instance_type() == HEAP_NUMBER_TYPE) {
    return T::Number(region);
  } else if (map->instance_type() == ODDBALL_TYPE) {
    // The only oddballs that can be recorded in ICs are booleans.
    return T::Boolean(region);
  } else {
    return T::Class(map, region);
  }
}


template Type* IC::MapToType<Type>(Handle<Map> map, Zone* zone);


template Handle<HeapType> IC::MapToType<HeapType>(Handle<Map> map,
                                                  Isolate* region);


void IC::UpdateMonomorphicIC(Handle<Code> handler, Handle<Name> name) {
  DCHECK(handler->is_handler());
  Handle<Code> ic = PropertyICCompiler::ComputeMonomorphic(
      kind(), name, receiver_type(), handler, extra_ic_state());
  set_target(*ic);
}


void IC::CopyICToMegamorphicCache(Handle<Name> name) {
  TypeHandleList types;
  CodeHandleList handlers;
  TargetTypes(&types);
  if (!target()->FindHandlers(&handlers, types.length())) return;
  for (int i = 0; i < types.length(); i++) {
    UpdateMegamorphicCache(*types.at(i), *name, *handlers.at(i));
  }
}


bool IC::IsTransitionOfMonomorphicTarget(Map* source_map, Map* target_map) {
  if (source_map == NULL) return true;
  if (target_map == NULL) return false;
  ElementsKind target_elements_kind = target_map->elements_kind();
  bool more_general_transition = IsMoreGeneralElementsKindTransition(
      source_map->elements_kind(), target_elements_kind);
  Map* transitioned_map =
      more_general_transition
          ? source_map->LookupElementsTransitionMap(target_elements_kind)
          : NULL;

  return transitioned_map == target_map;
}


void IC::PatchCache(Handle<Name> name, Handle<Code> code) {
  switch (state()) {
    case UNINITIALIZED:
    case PREMONOMORPHIC:
      UpdateMonomorphicIC(code, name);
      break;
    case PROTOTYPE_FAILURE:
    case MONOMORPHIC:
    case POLYMORPHIC:
      if (!target()->is_keyed_stub() || state() == PROTOTYPE_FAILURE) {
        if (UpdatePolymorphicIC(name, code)) break;
        CopyICToMegamorphicCache(name);
      }
      set_target(*megamorphic_stub());
    // Fall through.
    case MEGAMORPHIC:
      UpdateMegamorphicCache(*receiver_type(), *name, *code);
      break;
    case DEBUG_STUB:
      break;
    case DEFAULT:
    case GENERIC:
      UNREACHABLE();
      break;
  }
}


Handle<Code> LoadIC::initialize_stub(Isolate* isolate,
                                     ExtraICState extra_state) {
  return PropertyICCompiler::ComputeLoad(isolate, UNINITIALIZED, extra_state);
}


Handle<Code> LoadIC::megamorphic_stub() {
  if (kind() == Code::LOAD_IC) {
    MegamorphicLoadStub stub(isolate(), LoadICState(extra_ic_state()));
    return stub.GetCode();
  } else {
    DCHECK_EQ(Code::KEYED_LOAD_IC, kind());
    return KeyedLoadIC::generic_stub(isolate());
  }
}


Handle<Code> LoadIC::pre_monomorphic_stub(Isolate* isolate,
                                          ExtraICState extra_state) {
  return PropertyICCompiler::ComputeLoad(isolate, PREMONOMORPHIC, extra_state);
}


Handle<Code> KeyedLoadIC::pre_monomorphic_stub(Isolate* isolate) {
  return isolate->builtins()->KeyedLoadIC_PreMonomorphic();
}


Handle<Code> LoadIC::pre_monomorphic_stub() const {
  if (kind() == Code::LOAD_IC) {
    return LoadIC::pre_monomorphic_stub(isolate(), extra_ic_state());
  } else {
    DCHECK_EQ(Code::KEYED_LOAD_IC, kind());
    return KeyedLoadIC::pre_monomorphic_stub(isolate());
  }
}


Handle<Code> LoadIC::SimpleFieldLoad(FieldIndex index) {
  LoadFieldStub stub(isolate(), index);
  return stub.GetCode();
}


void LoadIC::UpdateCaches(LookupIterator* lookup) {
  if (state() == UNINITIALIZED) {
    // This is the first time we execute this inline cache. Set the target to
    // the pre monomorphic stub to delay setting the monomorphic state.
    set_target(*pre_monomorphic_stub());
    TRACE_IC("LoadIC", lookup->name());
    return;
  }

  Handle<Code> code;
  if (lookup->state() == LookupIterator::JSPROXY ||
      lookup->state() == LookupIterator::ACCESS_CHECK) {
    code = slow_stub();
  } else if (!lookup->IsFound()) {
    if (kind() == Code::LOAD_IC) {
      code = NamedLoadHandlerCompiler::ComputeLoadNonexistent(lookup->name(),
                                                              receiver_type());
      // TODO(jkummerow/verwaest): Introduce a builtin that handles this case.
      if (code.is_null()) code = slow_stub();
    } else {
      code = slow_stub();
    }
  } else {
    code = ComputeHandler(lookup);
  }

  PatchCache(lookup->name(), code);
  TRACE_IC("LoadIC", lookup->name());
}


void IC::UpdateMegamorphicCache(HeapType* type, Name* name, Code* code) {
  if (kind() == Code::KEYED_LOAD_IC || kind() == Code::KEYED_STORE_IC) return;
  Map* map = *TypeToMap(type, isolate());
  isolate()->stub_cache()->Set(name, map, code);
}


Handle<Code> IC::ComputeHandler(LookupIterator* lookup, Handle<Object> value) {
  bool receiver_is_holder =
      lookup->GetReceiver().is_identical_to(lookup->GetHolder<JSObject>());
  CacheHolderFlag flag;
  Handle<Map> stub_holder_map = IC::GetHandlerCacheHolder(
      *receiver_type(), receiver_is_holder, isolate(), &flag);

  Handle<Code> code = PropertyHandlerCompiler::Find(
      lookup->name(), stub_holder_map, kind(), flag,
      lookup->is_dictionary_holder() ? Code::NORMAL : Code::FAST);
  // Use the cached value if it exists, and if it is different from the
  // handler that just missed.
  if (!code.is_null()) {
    if (!maybe_handler_.is_null() &&
        !maybe_handler_.ToHandleChecked().is_identical_to(code)) {
      return code;
    }
    if (maybe_handler_.is_null()) {
      // maybe_handler_ is only populated for MONOMORPHIC and POLYMORPHIC ICs.
      // In MEGAMORPHIC case, check if the handler in the megamorphic stub
      // cache (which just missed) is different from the cached handler.
      if (state() == MEGAMORPHIC && lookup->GetReceiver()->IsHeapObject()) {
        Map* map = Handle<HeapObject>::cast(lookup->GetReceiver())->map();
        Code* megamorphic_cached_code =
            isolate()->stub_cache()->Get(*lookup->name(), map, code->flags());
        if (megamorphic_cached_code != *code) return code;
      } else {
        return code;
      }
    }
  }

  code = CompileHandler(lookup, value, flag);
  DCHECK(code->is_handler());

  // TODO(mvstanton): we'd only like to cache code on the map when it's custom
  // code compiled for this map, otherwise it's already cached in the global
  // code
  // cache. We are also guarding against installing code with flags that don't
  // match the desired CacheHolderFlag computed above, which would lead to
  // invalid lookups later.
  if (code->type() != Code::NORMAL &&
      Code::ExtractCacheHolderFromFlags(code->flags()) == flag) {
    Map::UpdateCodeCache(stub_holder_map, lookup->name(), code);
  }

  return code;
}


Handle<Code> LoadIC::CompileHandler(LookupIterator* lookup,
                                    Handle<Object> unused,
                                    CacheHolderFlag cache_holder) {
  Handle<Object> receiver = lookup->GetReceiver();
  if (receiver->IsString() &&
      Name::Equals(isolate()->factory()->length_string(), lookup->name())) {
    FieldIndex index = FieldIndex::ForInObjectOffset(String::kLengthOffset);
    return SimpleFieldLoad(index);
  }

  if (receiver->IsStringWrapper() &&
      Name::Equals(isolate()->factory()->length_string(), lookup->name())) {
    StringLengthStub string_length_stub(isolate());
    return string_length_stub.GetCode();
  }

  // Use specialized code for getting prototype of functions.
  if (receiver->IsJSFunction() &&
      Name::Equals(isolate()->factory()->prototype_string(), lookup->name()) &&
      Handle<JSFunction>::cast(receiver)->should_have_prototype() &&
      !Handle<JSFunction>::cast(receiver)
           ->map()
           ->has_non_instance_prototype()) {
    Handle<Code> stub;
    FunctionPrototypeStub function_prototype_stub(isolate());
    return function_prototype_stub.GetCode();
  }

  Handle<HeapType> type = receiver_type();
  Handle<JSObject> holder = lookup->GetHolder<JSObject>();
  bool receiver_is_holder = receiver.is_identical_to(holder);
  switch (lookup->state()) {
    case LookupIterator::INTERCEPTOR: {
      DCHECK(!holder->GetNamedInterceptor()->getter()->IsUndefined());
      NamedLoadHandlerCompiler compiler(isolate(), receiver_type(), holder,
                                        cache_holder);
      // Perform a lookup behind the interceptor. Copy the LookupIterator since
      // the original iterator will be used to fetch the value.
      LookupIterator it = *lookup;
      it.Next();
      LookupForRead(&it);
      return compiler.CompileLoadInterceptor(&it);
    }

    case LookupIterator::ACCESSOR: {
      // Use simple field loads for some well-known callback properties.
      if (receiver_is_holder) {
        DCHECK(receiver->IsJSObject());
        Handle<JSObject> js_receiver = Handle<JSObject>::cast(receiver);
        int object_offset;
        if (Accessors::IsJSObjectFieldAccessor<HeapType>(type, lookup->name(),
                                                         &object_offset)) {
          FieldIndex index =
              FieldIndex::ForInObjectOffset(object_offset, js_receiver->map());
          return SimpleFieldLoad(index);
        }
      }

      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(accessors);
        if (v8::ToCData<Address>(info->getter()) == 0) break;
        if (!ExecutableAccessorInfo::IsCompatibleReceiverType(isolate(), info,
                                                              type)) {
          break;
        }
        if (!holder->HasFastProperties()) break;
        NamedLoadHandlerCompiler compiler(isolate(), receiver_type(), holder,
                                          cache_holder);
        return compiler.CompileLoadCallback(lookup->name(), info);
      }
      if (accessors->IsAccessorPair()) {
        Handle<Object> getter(Handle<AccessorPair>::cast(accessors)->getter(),
                              isolate());
        if (!getter->IsJSFunction()) break;
        if (!holder->HasFastProperties()) break;
        Handle<JSFunction> function = Handle<JSFunction>::cast(getter);
        if (!receiver->IsJSObject() && !function->IsBuiltin() &&
            function->shared()->strict_mode() == SLOPPY) {
          // Calling sloppy non-builtins with a value as the receiver
          // requires boxing.
          break;
        }
        CallOptimization call_optimization(function);
        NamedLoadHandlerCompiler compiler(isolate(), receiver_type(), holder,
                                          cache_holder);
        if (call_optimization.is_simple_api_call() &&
            call_optimization.IsCompatibleReceiver(receiver, holder)) {
          return compiler.CompileLoadCallback(lookup->name(),
                                              call_optimization);
        }
        return compiler.CompileLoadViaGetter(lookup->name(), function);
      }
      // TODO(dcarney): Handle correctly.
      DCHECK(accessors->IsDeclaredAccessorInfo());
      break;
    }

    case LookupIterator::DATA: {
      if (lookup->is_dictionary_holder()) {
        if (kind() != Code::LOAD_IC) break;
        if (holder->IsGlobalObject()) {
          NamedLoadHandlerCompiler compiler(isolate(), receiver_type(), holder,
                                            cache_holder);
          Handle<PropertyCell> cell = lookup->GetPropertyCell();
          Handle<Code> code = compiler.CompileLoadGlobal(
              cell, lookup->name(), lookup->IsConfigurable());
          // TODO(verwaest): Move caching of these NORMAL stubs outside as well.
          CacheHolderFlag flag;
          Handle<Map> stub_holder_map = GetHandlerCacheHolder(
              *type, receiver_is_holder, isolate(), &flag);
          Map::UpdateCodeCache(stub_holder_map, lookup->name(), code);
          return code;
        }
        // There is only one shared stub for loading normalized
        // properties. It does not traverse the prototype chain, so the
        // property must be found in the object for the stub to be
        // applicable.
        if (!receiver_is_holder) break;
        return isolate()->builtins()->LoadIC_Normal();
      }

      // -------------- Fields --------------
      if (lookup->property_details().type() == FIELD) {
        FieldIndex field = lookup->GetFieldIndex();
        if (receiver_is_holder) {
          return SimpleFieldLoad(field);
        }
        NamedLoadHandlerCompiler compiler(isolate(), receiver_type(), holder,
                                          cache_holder);
        return compiler.CompileLoadField(lookup->name(), field);
      }

      // -------------- Constant properties --------------
      DCHECK(lookup->property_details().type() == CONSTANT);
      if (receiver_is_holder) {
        LoadConstantStub stub(isolate(), lookup->GetConstantIndex());
        return stub.GetCode();
      }
      NamedLoadHandlerCompiler compiler(isolate(), receiver_type(), holder,
                                        cache_holder);
      return compiler.CompileLoadConstant(lookup->name(),
                                          lookup->GetConstantIndex());
    }

    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::TRANSITION:
      UNREACHABLE();
  }

  return slow_stub();
}


static Handle<Object> TryConvertKey(Handle<Object> key, Isolate* isolate) {
  // This helper implements a few common fast cases for converting
  // non-smi keys of keyed loads/stores to a smi or a string.
  if (key->IsHeapNumber()) {
    double value = Handle<HeapNumber>::cast(key)->value();
    if (std::isnan(value)) {
      key = isolate->factory()->nan_string();
    } else {
      int int_value = FastD2I(value);
      if (value == int_value && Smi::IsValid(int_value)) {
        key = Handle<Smi>(Smi::FromInt(int_value), isolate);
      }
    }
  } else if (key->IsUndefined()) {
    key = isolate->factory()->undefined_string();
  }
  return key;
}


Handle<Code> KeyedLoadIC::LoadElementStub(Handle<JSObject> receiver) {
  Handle<Map> receiver_map(receiver->map(), isolate());
  MapHandleList target_receiver_maps;
  if (target().is_identical_to(string_stub())) {
    target_receiver_maps.Add(isolate()->factory()->string_map());
  } else {
    TargetMaps(&target_receiver_maps);
  }
  if (target_receiver_maps.length() == 0) {
    return PropertyICCompiler::ComputeKeyedLoadMonomorphic(receiver_map);
  }

  // The first time a receiver is seen that is a transitioned version of the
  // previous monomorphic receiver type, assume the new ElementsKind is the
  // monomorphic type. This benefits global arrays that only transition
  // once, and all call sites accessing them are faster if they remain
  // monomorphic. If this optimistic assumption is not true, the IC will
  // miss again and it will become polymorphic and support both the
  // untransitioned and transitioned maps.
  if (state() == MONOMORPHIC && IsMoreGeneralElementsKindTransition(
                                    target_receiver_maps.at(0)->elements_kind(),
                                    receiver->GetElementsKind())) {
    return PropertyICCompiler::ComputeKeyedLoadMonomorphic(receiver_map);
  }

  DCHECK(state() != GENERIC);

  // Determine the list of receiver maps that this call site has seen,
  // adding the map that was just encountered.
  if (!AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map)) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the generic stub.
    TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "same map added twice");
    return generic_stub();
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "max polymorph exceeded");
    return generic_stub();
  }

  return PropertyICCompiler::ComputeKeyedLoadPolymorphic(&target_receiver_maps);
}


MaybeHandle<Object> KeyedLoadIC::Load(Handle<Object> object,
                                      Handle<Object> key) {
  if (MigrateDeprecated(object)) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result, Runtime::GetObjectProperty(isolate(), object, key),
        Object);
    return result;
  }

  Handle<Object> load_handle;
  Handle<Code> stub = generic_stub();

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  if (key->IsInternalizedString() || key->IsSymbol()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), load_handle,
                               LoadIC::Load(object, Handle<Name>::cast(key)),
                               Object);
  } else if (FLAG_use_ic && !object->IsAccessCheckNeeded()) {
    if (object->IsString() && key->IsNumber()) {
      if (state() == UNINITIALIZED) stub = string_stub();
    } else if (object->IsJSObject()) {
      Handle<JSObject> receiver = Handle<JSObject>::cast(object);
      if (!Object::ToSmi(isolate(), key).is_null()) {
        stub = LoadElementStub(receiver);
      }
    }
  }

  if (!is_target_set()) {
    Code* generic = *generic_stub();
    if (*stub == generic) {
      TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "set generic");
    }
    set_target(*stub);
    TRACE_IC("LoadIC", key);
  }

  if (!load_handle.is_null()) return load_handle;
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                             Runtime::GetObjectProperty(isolate(), object, key),
                             Object);
  return result;
}


bool StoreIC::LookupForWrite(LookupIterator* it, Handle<Object> value,
                             JSReceiver::StoreFromKeyed store_mode) {
  // Disable ICs for non-JSObjects for now.
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSObject()) return false;
  DCHECK(!Handle<JSObject>::cast(receiver)->map()->is_deprecated());

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return false;
      case LookupIterator::INTERCEPTOR: {
        Handle<JSObject> holder = it->GetHolder<JSObject>();
        InterceptorInfo* info = holder->GetNamedInterceptor();
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          if (!info->setter()->IsUndefined()) return true;
        } else if (!info->getter()->IsUndefined() ||
                   !info->query()->IsUndefined()) {
          return false;
        }
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->GetHolder<JSObject>()->IsAccessCheckNeeded()) return false;
        break;
      case LookupIterator::ACCESSOR:
        return !it->IsReadOnly();
      case LookupIterator::DATA: {
        if (it->IsReadOnly()) return false;
        Handle<JSObject> holder = it->GetHolder<JSObject>();
        if (receiver.is_identical_to(holder)) {
          it->PrepareForDataProperty(value);
          // The previous receiver map might just have been deprecated,
          // so reload it.
          update_receiver_type(receiver);
          return true;
        }

        // Receiver != holder.
        PrototypeIterator iter(it->isolate(), receiver);
        if (receiver->IsJSGlobalProxy()) {
          return it->GetHolder<Object>().is_identical_to(
              PrototypeIterator::GetCurrent(iter));
        }

        it->PrepareTransitionToDataProperty(value, NONE, store_mode);
        return it->IsCacheableTransition();
      }
    }
  }

  it->PrepareTransitionToDataProperty(value, NONE, store_mode);
  return it->IsCacheableTransition();
}


MaybeHandle<Object> StoreIC::Store(Handle<Object> object, Handle<Name> name,
                                   Handle<Object> value,
                                   JSReceiver::StoreFromKeyed store_mode) {
  // TODO(verwaest): Let SetProperty do the migration, since storing a property
  // might deprecate the current map again, if value does not fit.
  if (MigrateDeprecated(object) || object->IsJSProxy()) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Object::SetProperty(object, name, value, strict_mode()), Object);
    return result;
  }

  // If the object is undefined or null it's illegal to try to set any
  // properties on it; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_store", object, name);
  }

  // Check if the given name is an array index.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    // Ignore other stores where the receiver is not a JSObject.
    // TODO(1475): Must check prototype chains of object wrappers.
    if (!object->IsJSObject()) return value;
    Handle<JSObject> receiver = Handle<JSObject>::cast(object);

    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        JSObject::SetElement(receiver, index, value, NONE, strict_mode()),
        Object);
    return value;
  }

  // Observed objects are always modified through the runtime.
  if (object->IsHeapObject() &&
      Handle<HeapObject>::cast(object)->map()->is_observed()) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Object::SetProperty(object, name, value, strict_mode(), store_mode),
        Object);
    return result;
  }

  LookupIterator it(object, name);
  if (FLAG_use_ic) UpdateCaches(&it, value, store_mode);

  // Set the property.
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      Object::SetProperty(&it, value, strict_mode(), store_mode), Object);
  return result;
}


Handle<Code> CallIC::initialize_stub(Isolate* isolate, int argc,
                                     CallICState::CallType call_type) {
  CallICStub stub(isolate, CallICState(argc, call_type));
  Handle<Code> code = stub.GetCode();
  return code;
}


Handle<Code> StoreIC::initialize_stub(Isolate* isolate,
                                      StrictMode strict_mode) {
  ExtraICState extra_state = ComputeExtraICState(strict_mode);
  Handle<Code> ic =
      PropertyICCompiler::ComputeStore(isolate, UNINITIALIZED, extra_state);
  return ic;
}


Handle<Code> StoreIC::megamorphic_stub() {
  if (kind() == Code::STORE_IC) {
    return PropertyICCompiler::ComputeStore(isolate(), MEGAMORPHIC,
                                            extra_ic_state());
  } else {
    DCHECK(kind() == Code::KEYED_STORE_IC);
    if (strict_mode() == STRICT) {
      return isolate()->builtins()->KeyedStoreIC_Generic_Strict();
    } else {
      return isolate()->builtins()->KeyedStoreIC_Generic();
    }
  }
}


Handle<Code> StoreIC::generic_stub() const {
  if (kind() == Code::STORE_IC) {
    return PropertyICCompiler::ComputeStore(isolate(), GENERIC,
                                            extra_ic_state());
  } else {
    DCHECK(kind() == Code::KEYED_STORE_IC);
    if (strict_mode() == STRICT) {
      return isolate()->builtins()->KeyedStoreIC_Generic_Strict();
    } else {
      return isolate()->builtins()->KeyedStoreIC_Generic();
    }
  }
}


Handle<Code> StoreIC::slow_stub() const {
  if (kind() == Code::STORE_IC) {
    return isolate()->builtins()->StoreIC_Slow();
  } else {
    DCHECK(kind() == Code::KEYED_STORE_IC);
    return isolate()->builtins()->KeyedStoreIC_Slow();
  }
}


Handle<Code> StoreIC::pre_monomorphic_stub(Isolate* isolate,
                                           StrictMode strict_mode) {
  ExtraICState state = ComputeExtraICState(strict_mode);
  return PropertyICCompiler::ComputeStore(isolate, PREMONOMORPHIC, state);
}


void StoreIC::UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                           JSReceiver::StoreFromKeyed store_mode) {
  if (state() == UNINITIALIZED) {
    // This is the first time we execute this inline cache. Set the target to
    // the pre monomorphic stub to delay setting the monomorphic state.
    set_target(*pre_monomorphic_stub());
    TRACE_IC("StoreIC", lookup->name());
    return;
  }

  bool use_ic = LookupForWrite(lookup, value, store_mode);
  if (!use_ic) {
    TRACE_GENERIC_IC(isolate(), "StoreIC", "LookupForWrite said 'false'");
  }
  Handle<Code> code = use_ic ? ComputeHandler(lookup, value) : slow_stub();

  PatchCache(lookup->name(), code);
  TRACE_IC("StoreIC", lookup->name());
}


Handle<Code> StoreIC::CompileHandler(LookupIterator* lookup,
                                     Handle<Object> value,
                                     CacheHolderFlag cache_holder) {
  DCHECK_NE(LookupIterator::JSPROXY, lookup->state());

  // This is currently guaranteed by checks in StoreIC::Store.
  Handle<JSObject> receiver = Handle<JSObject>::cast(lookup->GetReceiver());
  Handle<JSObject> holder = lookup->GetHolder<JSObject>();
  DCHECK(!receiver->IsAccessCheckNeeded());

  switch (lookup->state()) {
    case LookupIterator::TRANSITION: {
      Handle<Map> transition = lookup->transition_map();
      // Currently not handled by CompileStoreTransition.
      if (!holder->HasFastProperties()) {
        TRACE_GENERIC_IC(isolate(), "StoreIC", "transition from slow");
        break;
      }

      DCHECK(lookup->IsCacheableTransition());
      NamedStoreHandlerCompiler compiler(isolate(), receiver_type(), holder);
      return compiler.CompileStoreTransition(transition, lookup->name());
    }

    case LookupIterator::INTERCEPTOR: {
      DCHECK(!holder->GetNamedInterceptor()->setter()->IsUndefined());
      NamedStoreHandlerCompiler compiler(isolate(), receiver_type(), holder);
      return compiler.CompileStoreInterceptor(lookup->name());
    }

    case LookupIterator::ACCESSOR: {
      if (!holder->HasFastProperties()) {
        TRACE_GENERIC_IC(isolate(), "StoreIC", "accessor on slow map");
        break;
      }
      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(accessors);
        if (v8::ToCData<Address>(info->setter()) == 0) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "setter == 0");
          break;
        }
        if (!ExecutableAccessorInfo::IsCompatibleReceiverType(
                isolate(), info, receiver_type())) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "incompatible receiver type");
          break;
        }
        NamedStoreHandlerCompiler compiler(isolate(), receiver_type(), holder);
        return compiler.CompileStoreCallback(receiver, lookup->name(), info);
      } else if (accessors->IsAccessorPair()) {
        Handle<Object> setter(Handle<AccessorPair>::cast(accessors)->setter(),
                              isolate());
        if (!setter->IsJSFunction()) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "setter not a function");
          break;
        }
        Handle<JSFunction> function = Handle<JSFunction>::cast(setter);
        CallOptimization call_optimization(function);
        NamedStoreHandlerCompiler compiler(isolate(), receiver_type(), holder);
        if (call_optimization.is_simple_api_call() &&
            call_optimization.IsCompatibleReceiver(receiver, holder)) {
          return compiler.CompileStoreCallback(receiver, lookup->name(),
                                               call_optimization);
        }
        return compiler.CompileStoreViaSetter(receiver, lookup->name(),
                                              Handle<JSFunction>::cast(setter));
      }
      // TODO(dcarney): Handle correctly.
      DCHECK(accessors->IsDeclaredAccessorInfo());
      TRACE_GENERIC_IC(isolate(), "StoreIC", "declared accessor info");
      break;
    }

    case LookupIterator::DATA: {
      if (lookup->is_dictionary_holder()) {
        if (holder->IsGlobalObject()) {
          Handle<PropertyCell> cell = lookup->GetPropertyCell();
          Handle<HeapType> union_type = PropertyCell::UpdatedType(cell, value);
          StoreGlobalStub stub(isolate(), union_type->IsConstant(),
                               receiver->IsJSGlobalProxy());
          Handle<Code> code = stub.GetCodeCopyFromTemplate(
              Handle<GlobalObject>::cast(holder), cell);
          // TODO(verwaest): Move caching of these NORMAL stubs outside as well.
          HeapObject::UpdateMapCodeCache(receiver, lookup->name(), code);
          return code;
        }
        DCHECK(holder.is_identical_to(receiver));
        return isolate()->builtins()->StoreIC_Normal();
      }

      // -------------- Fields --------------
      if (lookup->property_details().type() == FIELD) {
        bool use_stub = true;
        if (lookup->representation().IsHeapObject()) {
          // Only use a generic stub if no types need to be tracked.
          Handle<HeapType> field_type = lookup->GetFieldType();
          HeapType::Iterator<Map> it = field_type->Classes();
          use_stub = it.Done();
        }
        if (use_stub) {
          StoreFieldStub stub(isolate(), lookup->GetFieldIndex(),
                              lookup->representation());
          return stub.GetCode();
        }
        NamedStoreHandlerCompiler compiler(isolate(), receiver_type(), holder);
        return compiler.CompileStoreField(lookup);
      }

      // -------------- Constant properties --------------
      DCHECK(lookup->property_details().type() == CONSTANT);
      TRACE_GENERIC_IC(isolate(), "StoreIC", "constant property");
      break;
    }

    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
      UNREACHABLE();
  }
  return slow_stub();
}


Handle<Code> KeyedStoreIC::StoreElementStub(Handle<JSObject> receiver,
                                            KeyedAccessStoreMode store_mode) {
  // Don't handle megamorphic property accesses for INTERCEPTORS or CALLBACKS
  // via megamorphic stubs, since they don't have a map in their relocation info
  // and so the stubs can't be harvested for the object needed for a map check.
  if (target()->type() != Code::NORMAL) {
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "non-NORMAL target type");
    return generic_stub();
  }

  Handle<Map> receiver_map(receiver->map(), isolate());
  MapHandleList target_receiver_maps;
  TargetMaps(&target_receiver_maps);
  if (target_receiver_maps.length() == 0) {
    Handle<Map> monomorphic_map =
        ComputeTransitionedMap(receiver_map, store_mode);
    store_mode = GetNonTransitioningStoreMode(store_mode);
    return PropertyICCompiler::ComputeKeyedStoreMonomorphic(
        monomorphic_map, strict_mode(), store_mode);
  }

  // There are several special cases where an IC that is MONOMORPHIC can still
  // transition to a different GetNonTransitioningStoreMode IC that handles a
  // superset of the original IC. Handle those here if the receiver map hasn't
  // changed or it has transitioned to a more general kind.
  KeyedAccessStoreMode old_store_mode =
      KeyedStoreIC::GetKeyedAccessStoreMode(target()->extra_ic_state());
  Handle<Map> previous_receiver_map = target_receiver_maps.at(0);
  if (state() == MONOMORPHIC) {
    Handle<Map> transitioned_receiver_map = receiver_map;
    if (IsTransitionStoreMode(store_mode)) {
      transitioned_receiver_map =
          ComputeTransitionedMap(receiver_map, store_mode);
    }
    if ((receiver_map.is_identical_to(previous_receiver_map) &&
         IsTransitionStoreMode(store_mode)) ||
        IsTransitionOfMonomorphicTarget(*previous_receiver_map,
                                        *transitioned_receiver_map)) {
      // If the "old" and "new" maps are in the same elements map family, or
      // if they at least come from the same origin for a transitioning store,
      // stay MONOMORPHIC and use the map for the most generic ElementsKind.
      store_mode = GetNonTransitioningStoreMode(store_mode);
      return PropertyICCompiler::ComputeKeyedStoreMonomorphic(
          transitioned_receiver_map, strict_mode(), store_mode);
    } else if (*previous_receiver_map == receiver->map() &&
               old_store_mode == STANDARD_STORE &&
               (store_mode == STORE_AND_GROW_NO_TRANSITION ||
                store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
                store_mode == STORE_NO_TRANSITION_HANDLE_COW)) {
      // A "normal" IC that handles stores can switch to a version that can
      // grow at the end of the array, handle OOB accesses or copy COW arrays
      // and still stay MONOMORPHIC.
      return PropertyICCompiler::ComputeKeyedStoreMonomorphic(
          receiver_map, strict_mode(), store_mode);
    }
  }

  DCHECK(state() != GENERIC);

  bool map_added =
      AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map);

  if (IsTransitionStoreMode(store_mode)) {
    Handle<Map> transitioned_receiver_map =
        ComputeTransitionedMap(receiver_map, store_mode);
    map_added |= AddOneReceiverMapIfMissing(&target_receiver_maps,
                                            transitioned_receiver_map);
  }

  if (!map_added) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the generic stub.
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "same map added twice");
    return generic_stub();
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "max polymorph exceeded");
    return generic_stub();
  }

  // Make sure all polymorphic handlers have the same store mode, otherwise the
  // generic stub must be used.
  store_mode = GetNonTransitioningStoreMode(store_mode);
  if (old_store_mode != STANDARD_STORE) {
    if (store_mode == STANDARD_STORE) {
      store_mode = old_store_mode;
    } else if (store_mode != old_store_mode) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "store mode mismatch");
      return generic_stub();
    }
  }

  // If the store mode isn't the standard mode, make sure that all polymorphic
  // receivers are either external arrays, or all "normal" arrays. Otherwise,
  // use the generic stub.
  if (store_mode != STANDARD_STORE) {
    int external_arrays = 0;
    for (int i = 0; i < target_receiver_maps.length(); ++i) {
      if (target_receiver_maps[i]->has_external_array_elements() ||
          target_receiver_maps[i]->has_fixed_typed_array_elements()) {
        external_arrays++;
      }
    }
    if (external_arrays != 0 &&
        external_arrays != target_receiver_maps.length()) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC",
                       "unsupported combination of external and normal arrays");
      return generic_stub();
    }
  }

  return PropertyICCompiler::ComputeKeyedStorePolymorphic(
      &target_receiver_maps, store_mode, strict_mode());
}


Handle<Map> KeyedStoreIC::ComputeTransitionedMap(
    Handle<Map> map, KeyedAccessStoreMode store_mode) {
  switch (store_mode) {
    case STORE_TRANSITION_SMI_TO_OBJECT:
    case STORE_TRANSITION_DOUBLE_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_SMI_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_DOUBLE_TO_OBJECT:
      return Map::TransitionElementsTo(map, FAST_ELEMENTS);
    case STORE_TRANSITION_SMI_TO_DOUBLE:
    case STORE_AND_GROW_TRANSITION_SMI_TO_DOUBLE:
      return Map::TransitionElementsTo(map, FAST_DOUBLE_ELEMENTS);
    case STORE_TRANSITION_HOLEY_SMI_TO_OBJECT:
    case STORE_TRANSITION_HOLEY_DOUBLE_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_HOLEY_DOUBLE_TO_OBJECT:
      return Map::TransitionElementsTo(map, FAST_HOLEY_ELEMENTS);
    case STORE_TRANSITION_HOLEY_SMI_TO_DOUBLE:
    case STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_DOUBLE:
      return Map::TransitionElementsTo(map, FAST_HOLEY_DOUBLE_ELEMENTS);
    case STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS:
      DCHECK(map->has_external_array_elements());
    // Fall through
    case STORE_NO_TRANSITION_HANDLE_COW:
    case STANDARD_STORE:
    case STORE_AND_GROW_NO_TRANSITION:
      return map;
  }
  UNREACHABLE();
  return MaybeHandle<Map>().ToHandleChecked();
}


bool IsOutOfBoundsAccess(Handle<JSObject> receiver, int index) {
  if (receiver->IsJSArray()) {
    return JSArray::cast(*receiver)->length()->IsSmi() &&
           index >= Smi::cast(JSArray::cast(*receiver)->length())->value();
  }
  return index >= receiver->elements()->length();
}


KeyedAccessStoreMode KeyedStoreIC::GetStoreMode(Handle<JSObject> receiver,
                                                Handle<Object> key,
                                                Handle<Object> value) {
  Handle<Smi> smi_key = Object::ToSmi(isolate(), key).ToHandleChecked();
  int index = smi_key->value();
  bool oob_access = IsOutOfBoundsAccess(receiver, index);
  // Don't consider this a growing store if the store would send the receiver to
  // dictionary mode.
  bool allow_growth = receiver->IsJSArray() && oob_access &&
                      !receiver->WouldConvertToSlowElements(key);
  if (allow_growth) {
    // Handle growing array in stub if necessary.
    if (receiver->HasFastSmiElements()) {
      if (value->IsHeapNumber()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_DOUBLE;
        } else {
          return STORE_AND_GROW_TRANSITION_SMI_TO_DOUBLE;
        }
      }
      if (value->IsHeapObject()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_OBJECT;
        } else {
          return STORE_AND_GROW_TRANSITION_SMI_TO_OBJECT;
        }
      }
    } else if (receiver->HasFastDoubleElements()) {
      if (!value->IsSmi() && !value->IsHeapNumber()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_AND_GROW_TRANSITION_HOLEY_DOUBLE_TO_OBJECT;
        } else {
          return STORE_AND_GROW_TRANSITION_DOUBLE_TO_OBJECT;
        }
      }
    }
    return STORE_AND_GROW_NO_TRANSITION;
  } else {
    // Handle only in-bounds elements accesses.
    if (receiver->HasFastSmiElements()) {
      if (value->IsHeapNumber()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_TRANSITION_HOLEY_SMI_TO_DOUBLE;
        } else {
          return STORE_TRANSITION_SMI_TO_DOUBLE;
        }
      } else if (value->IsHeapObject()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_TRANSITION_HOLEY_SMI_TO_OBJECT;
        } else {
          return STORE_TRANSITION_SMI_TO_OBJECT;
        }
      }
    } else if (receiver->HasFastDoubleElements()) {
      if (!value->IsSmi() && !value->IsHeapNumber()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_TRANSITION_HOLEY_DOUBLE_TO_OBJECT;
        } else {
          return STORE_TRANSITION_DOUBLE_TO_OBJECT;
        }
      }
    }
    if (!FLAG_trace_external_array_abuse &&
        receiver->map()->has_external_array_elements() && oob_access) {
      return STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS;
    }
    Heap* heap = receiver->GetHeap();
    if (receiver->elements()->map() == heap->fixed_cow_array_map()) {
      return STORE_NO_TRANSITION_HANDLE_COW;
    } else {
      return STANDARD_STORE;
    }
  }
}


MaybeHandle<Object> KeyedStoreIC::Store(Handle<Object> object,
                                        Handle<Object> key,
                                        Handle<Object> value) {
  // TODO(verwaest): Let SetProperty do the migration, since storing a property
  // might deprecate the current map again, if value does not fit.
  if (MigrateDeprecated(object)) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result, Runtime::SetObjectProperty(isolate(), object, key,
                                                      value, strict_mode()),
        Object);
    return result;
  }

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  Handle<Object> store_handle;
  Handle<Code> stub = generic_stub();

  if (key->IsInternalizedString()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), store_handle,
        StoreIC::Store(object, Handle<String>::cast(key), value,
                       JSReceiver::MAY_BE_STORE_FROM_KEYED),
        Object);
    // TODO(jkummerow): Ideally we'd wrap this in "if (!is_target_set())",
    // but doing so causes Hydrogen crashes. Needs investigation.
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC",
                     "unhandled internalized string key");
    TRACE_IC("StoreIC", key);
    set_target(*stub);
    return store_handle;
  }

  bool use_ic =
      FLAG_use_ic && !object->IsStringWrapper() &&
      !object->IsAccessCheckNeeded() && !object->IsJSGlobalProxy() &&
      !(object->IsJSObject() && JSObject::cast(*object)->map()->is_observed());
  if (use_ic && !object->IsSmi()) {
    // Don't use ICs for maps of the objects in Array's prototype chain. We
    // expect to be able to trap element sets to objects with those maps in
    // the runtime to enable optimization of element hole access.
    Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
    if (heap_object->map()->IsMapInArrayPrototypeChain()) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "map in array prototype");
      use_ic = false;
    }
  }

  if (use_ic) {
    DCHECK(!object->IsAccessCheckNeeded());

    if (object->IsJSObject()) {
      Handle<JSObject> receiver = Handle<JSObject>::cast(object);
      bool key_is_smi_like = !Object::ToSmi(isolate(), key).is_null();
      if (receiver->elements()->map() ==
          isolate()->heap()->sloppy_arguments_elements_map()) {
        if (strict_mode() == SLOPPY) {
          stub = sloppy_arguments_stub();
        } else {
          TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "arguments receiver");
        }
      } else if (key_is_smi_like &&
                 !(target().is_identical_to(sloppy_arguments_stub()))) {
        // We should go generic if receiver isn't a dictionary, but our
        // prototype chain does have dictionary elements. This ensures that
        // other non-dictionary receivers in the polymorphic case benefit
        // from fast path keyed stores.
        if (!(receiver->map()->DictionaryElementsInPrototypeChainOnly())) {
          KeyedAccessStoreMode store_mode = GetStoreMode(receiver, key, value);
          stub = StoreElementStub(receiver, store_mode);
        } else {
          TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "dictionary prototype");
        }
      } else {
        TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "non-smi-like key");
      }
    } else {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "non-JSObject receiver");
    }
  }

  if (store_handle.is_null()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), store_handle,
        Runtime::SetObjectProperty(isolate(), object, key, value,
                                   strict_mode()),
        Object);
  }

  DCHECK(!is_target_set());
  Code* generic = *generic_stub();
  if (*stub == generic) {
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "set generic");
  }
  if (*stub == *slow_stub()) {
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "slow stub");
  }
  DCHECK(!stub.is_null());
  set_target(*stub);
  TRACE_IC("StoreIC", key);

  return store_handle;
}


bool CallIC::DoCustomHandler(Handle<Object> receiver, Handle<Object> function,
                             Handle<TypeFeedbackVector> vector,
                             Handle<Smi> slot, const CallICState& state) {
  DCHECK(FLAG_use_ic && function->IsJSFunction());

  // Are we the array function?
  Handle<JSFunction> array_function =
      Handle<JSFunction>(isolate()->native_context()->array_function());
  if (array_function.is_identical_to(Handle<JSFunction>::cast(function))) {
    // Alter the slot.
    IC::State old_state = FeedbackToState(vector, slot);
    Object* feedback = vector->get(slot->value());
    if (!feedback->IsAllocationSite()) {
      Handle<AllocationSite> new_site =
          isolate()->factory()->NewAllocationSite();
      vector->set(slot->value(), *new_site);
    }

    CallIC_ArrayStub stub(isolate(), state);
    set_target(*stub.GetCode());
    Handle<String> name;
    if (array_function->shared()->name()->IsString()) {
      name = Handle<String>(String::cast(array_function->shared()->name()),
                            isolate());
    }

    IC::State new_state = FeedbackToState(vector, slot);
    OnTypeFeedbackChanged(isolate(), address(), old_state, new_state, true);
    TRACE_VECTOR_IC("CallIC (custom handler)", name, old_state, new_state);
    return true;
  }
  return false;
}


void CallIC::PatchMegamorphic(Handle<Object> function,
                              Handle<TypeFeedbackVector> vector,
                              Handle<Smi> slot) {
  CallICState state(target()->extra_ic_state());
  IC::State old_state = FeedbackToState(vector, slot);

  // We are going generic.
  vector->set(slot->value(),
              *TypeFeedbackVector::MegamorphicSentinel(isolate()),
              SKIP_WRITE_BARRIER);

  CallICStub stub(isolate(), state);
  Handle<Code> code = stub.GetCode();
  set_target(*code);

  Handle<Object> name = isolate()->factory()->empty_string();
  if (function->IsJSFunction()) {
    Handle<JSFunction> js_function = Handle<JSFunction>::cast(function);
    name = handle(js_function->shared()->name(), isolate());
  }

  IC::State new_state = FeedbackToState(vector, slot);
  OnTypeFeedbackChanged(isolate(), address(), old_state, new_state, true);
  TRACE_VECTOR_IC("CallIC", name, old_state, new_state);
}


void CallIC::HandleMiss(Handle<Object> receiver, Handle<Object> function,
                        Handle<TypeFeedbackVector> vector, Handle<Smi> slot) {
  CallICState state(target()->extra_ic_state());
  IC::State old_state = FeedbackToState(vector, slot);
  Handle<Object> name = isolate()->factory()->empty_string();
  Object* feedback = vector->get(slot->value());

  // Hand-coded MISS handling is easier if CallIC slots don't contain smis.
  DCHECK(!feedback->IsSmi());

  if (feedback->IsJSFunction() || !function->IsJSFunction()) {
    // We are going generic.
    vector->set(slot->value(),
                *TypeFeedbackVector::MegamorphicSentinel(isolate()),
                SKIP_WRITE_BARRIER);
  } else {
    // The feedback is either uninitialized or an allocation site.
    // It might be an allocation site because if we re-compile the full code
    // to add deoptimization support, we call with the default call-ic, and
    // merely need to patch the target to match the feedback.
    // TODO(mvstanton): the better approach is to dispense with patching
    // altogether, which is in progress.
    DCHECK(feedback == *TypeFeedbackVector::UninitializedSentinel(isolate()) ||
           feedback->IsAllocationSite());

    // Do we want to install a custom handler?
    if (FLAG_use_ic &&
        DoCustomHandler(receiver, function, vector, slot, state)) {
      return;
    }

    vector->set(slot->value(), *function);
  }

  if (function->IsJSFunction()) {
    Handle<JSFunction> js_function = Handle<JSFunction>::cast(function);
    name = handle(js_function->shared()->name(), isolate());
  }

  IC::State new_state = FeedbackToState(vector, slot);
  OnTypeFeedbackChanged(isolate(), address(), old_state, new_state, true);
  TRACE_VECTOR_IC("CallIC", name, old_state, new_state);
}


#undef TRACE_IC


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(CallIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CallIC ic(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> function = args.at<Object>(1);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(2);
  Handle<Smi> slot = args.at<Smi>(3);
  ic.HandleMiss(receiver, function, vector, slot);
  return *function;
}


RUNTIME_FUNCTION(CallIC_Customization_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  // A miss on a custom call ic always results in going megamorphic.
  CallIC ic(isolate);
  Handle<Object> function = args.at<Object>(1);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(2);
  Handle<Smi> slot = args.at<Smi>(3);
  ic.PatchMegamorphic(function, vector, slot);
  return *function;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(LoadIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  LoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Name> key = args.at<Name>(1);
  ic.UpdateState(receiver, key);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));
  return *result;
}


// Used from ic-<arch>.cc
RUNTIME_FUNCTION(KeyedLoadIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));
  return *result;
}


RUNTIME_FUNCTION(KeyedLoadIC_MissFromStubFailure) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  KeyedLoadIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));
  return *result;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(StoreIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<String> key = args.at<String>(1);
  ic.UpdateState(receiver, key);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, ic.Store(receiver, key, args.at<Object>(2)));
  return *result;
}


RUNTIME_FUNCTION(StoreIC_MissFromStubFailure) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  StoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<String> key = args.at<String>(1);
  ic.UpdateState(receiver, key);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, ic.Store(receiver, key, args.at<Object>(2)));
  return *result;
}


// Extend storage is called in a store inline cache when
// it is necessary to extend the properties array of a
// JSObject.
RUNTIME_FUNCTION(SharedStoreIC_ExtendStorage) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope shs(isolate);
  DCHECK(args.length() == 3);

  // Convert the parameters
  Handle<JSObject> object = args.at<JSObject>(0);
  Handle<Map> transition = args.at<Map>(1);
  Handle<Object> value = args.at<Object>(2);

  // Check the object has run out out property space.
  DCHECK(object->HasFastProperties());
  DCHECK(object->map()->unused_property_fields() == 0);

  JSObject::MigrateToNewProperty(object, transition, value);

  // Return the stored value.
  return *value;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(KeyedStoreIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, ic.Store(receiver, key, args.at<Object>(2)));
  return *result;
}


RUNTIME_FUNCTION(KeyedStoreIC_MissFromStubFailure) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, ic.Store(receiver, key, args.at<Object>(2)));
  return *result;
}


RUNTIME_FUNCTION(StoreIC_Slow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictMode strict_mode = ic.strict_mode();
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(isolate, object, key, value, strict_mode));
  return *result;
}


RUNTIME_FUNCTION(KeyedStoreIC_Slow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictMode strict_mode = ic.strict_mode();
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(isolate, object, key, value, strict_mode));
  return *result;
}


RUNTIME_FUNCTION(ElementsTransitionAndStoreIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> value = args.at<Object>(0);
  Handle<Map> map = args.at<Map>(1);
  Handle<Object> key = args.at<Object>(2);
  Handle<Object> object = args.at<Object>(3);
  StrictMode strict_mode = ic.strict_mode();
  if (object->IsJSObject()) {
    JSObject::TransitionElementsKind(Handle<JSObject>::cast(object),
                                     map->elements_kind());
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(isolate, object, key, value, strict_mode));
  return *result;
}


MaybeHandle<Object> BinaryOpIC::Transition(
    Handle<AllocationSite> allocation_site, Handle<Object> left,
    Handle<Object> right) {
  BinaryOpICState state(isolate(), target()->extra_ic_state());

  // Compute the actual result using the builtin for the binary operation.
  Object* builtin = isolate()->js_builtins_object()->javascript_builtin(
      TokenToJSBuiltin(state.op()));
  Handle<JSFunction> function = handle(JSFunction::cast(builtin), isolate());
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result, Execution::Call(isolate(), function, left, 1, &right),
      Object);

  // Execution::Call can execute arbitrary JavaScript, hence potentially
  // update the state of this very IC, so we must update the stored state.
  UpdateTarget();
  // Compute the new state.
  BinaryOpICState old_state(isolate(), target()->extra_ic_state());
  state.Update(left, right, result);

  // Check if we have a string operation here.
  Handle<Code> target;
  if (!allocation_site.is_null() || state.ShouldCreateAllocationMementos()) {
    // Setup the allocation site on-demand.
    if (allocation_site.is_null()) {
      allocation_site = isolate()->factory()->NewAllocationSite();
    }

    // Install the stub with an allocation site.
    BinaryOpICWithAllocationSiteStub stub(isolate(), state);
    target = stub.GetCodeCopyFromTemplate(allocation_site);

    // Sanity check the trampoline stub.
    DCHECK_EQ(*allocation_site, target->FindFirstAllocationSite());
  } else {
    // Install the generic stub.
    BinaryOpICStub stub(isolate(), state);
    target = stub.GetCode();

    // Sanity check the generic stub.
    DCHECK_EQ(NULL, target->FindFirstAllocationSite());
  }
  set_target(*target);

  if (FLAG_trace_ic) {
    OFStream os(stdout);
    os << "[BinaryOpIC" << old_state << " => " << state << " @ "
       << static_cast<void*>(*target) << " <- ";
    JavaScriptFrame::PrintTop(isolate(), stdout, false, true);
    if (!allocation_site.is_null()) {
      os << " using allocation site " << static_cast<void*>(*allocation_site);
    }
    os << "]" << endl;
  }

  // Patch the inlined smi code as necessary.
  if (!old_state.UseInlinedSmiCode() && state.UseInlinedSmiCode()) {
    PatchInlinedSmiCode(address(), ENABLE_INLINED_SMI_CHECK);
  } else if (old_state.UseInlinedSmiCode() && !state.UseInlinedSmiCode()) {
    PatchInlinedSmiCode(address(), DISABLE_INLINED_SMI_CHECK);
  }

  return result;
}


RUNTIME_FUNCTION(BinaryOpIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> left = args.at<Object>(BinaryOpICStub::kLeft);
  Handle<Object> right = args.at<Object>(BinaryOpICStub::kRight);
  BinaryOpIC ic(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      ic.Transition(Handle<AllocationSite>::null(), left, right));
  return *result;
}


RUNTIME_FUNCTION(BinaryOpIC_MissWithAllocationSite) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<AllocationSite> allocation_site =
      args.at<AllocationSite>(BinaryOpWithAllocationSiteStub::kAllocationSite);
  Handle<Object> left = args.at<Object>(BinaryOpWithAllocationSiteStub::kLeft);
  Handle<Object> right =
      args.at<Object>(BinaryOpWithAllocationSiteStub::kRight);
  BinaryOpIC ic(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, ic.Transition(allocation_site, left, right));
  return *result;
}


Code* CompareIC::GetRawUninitialized(Isolate* isolate, Token::Value op) {
  CompareICStub stub(isolate, op, CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED);
  Code* code = NULL;
  CHECK(stub.FindCodeInCache(&code));
  return code;
}


Handle<Code> CompareIC::GetUninitialized(Isolate* isolate, Token::Value op) {
  CompareICStub stub(isolate, op, CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED);
  return stub.GetCode();
}


Code* CompareIC::UpdateCaches(Handle<Object> x, Handle<Object> y) {
  HandleScope scope(isolate());
  CompareICStub old_stub(target()->stub_key(), isolate());
  CompareICState::State new_left =
      CompareICState::NewInputState(old_stub.left(), x);
  CompareICState::State new_right =
      CompareICState::NewInputState(old_stub.right(), y);
  CompareICState::State state = CompareICState::TargetState(
      old_stub.state(), old_stub.left(), old_stub.right(), op_,
      HasInlinedSmiCode(address()), x, y);
  CompareICStub stub(isolate(), op_, new_left, new_right, state);
  if (state == CompareICState::KNOWN_OBJECT) {
    stub.set_known_map(
        Handle<Map>(Handle<JSObject>::cast(x)->map(), isolate()));
  }
  Handle<Code> new_target = stub.GetCode();
  set_target(*new_target);

  if (FLAG_trace_ic) {
    PrintF("[CompareIC in ");
    JavaScriptFrame::PrintTop(isolate(), stdout, false, true);
    PrintF(" ((%s+%s=%s)->(%s+%s=%s))#%s @ %p]\n",
           CompareICState::GetStateName(old_stub.left()),
           CompareICState::GetStateName(old_stub.right()),
           CompareICState::GetStateName(old_stub.state()),
           CompareICState::GetStateName(new_left),
           CompareICState::GetStateName(new_right),
           CompareICState::GetStateName(state), Token::Name(op_),
           static_cast<void*>(*stub.GetCode()));
  }

  // Activate inlined smi code.
  if (old_stub.state() == CompareICState::UNINITIALIZED) {
    PatchInlinedSmiCode(address(), ENABLE_INLINED_SMI_CHECK);
  }

  return *new_target;
}


// Used from CompareICStub::GenerateMiss in code-stubs-<arch>.cc.
RUNTIME_FUNCTION(CompareIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CompareIC ic(isolate, static_cast<Token::Value>(args.smi_at(2)));
  return ic.UpdateCaches(args.at<Object>(0), args.at<Object>(1));
}


void CompareNilIC::Clear(Address address, Code* target,
                         ConstantPoolArray* constant_pool) {
  if (IsCleared(target)) return;
  ExtraICState state = target->extra_ic_state();

  CompareNilICStub stub(target->GetIsolate(), state,
                        HydrogenCodeStub::UNINITIALIZED);
  stub.ClearState();

  Code* code = NULL;
  CHECK(stub.FindCodeInCache(&code));

  SetTargetAtAddress(address, code, constant_pool);
}


Handle<Object> CompareNilIC::DoCompareNilSlow(Isolate* isolate, NilValue nil,
                                              Handle<Object> object) {
  if (object->IsNull() || object->IsUndefined()) {
    return handle(Smi::FromInt(true), isolate);
  }
  return handle(Smi::FromInt(object->IsUndetectableObject()), isolate);
}


Handle<Object> CompareNilIC::CompareNil(Handle<Object> object) {
  ExtraICState extra_ic_state = target()->extra_ic_state();

  CompareNilICStub stub(isolate(), extra_ic_state);

  // Extract the current supported types from the patched IC and calculate what
  // types must be supported as a result of the miss.
  bool already_monomorphic = stub.IsMonomorphic();

  stub.UpdateStatus(object);

  NilValue nil = stub.nil_value();

  // Find or create the specialized stub to support the new set of types.
  Handle<Code> code;
  if (stub.IsMonomorphic()) {
    Handle<Map> monomorphic_map(already_monomorphic && FirstTargetMap() != NULL
                                    ? FirstTargetMap()
                                    : HeapObject::cast(*object)->map());
    code = PropertyICCompiler::ComputeCompareNil(monomorphic_map, &stub);
  } else {
    code = stub.GetCode();
  }
  set_target(*code);
  return DoCompareNilSlow(isolate(), nil, object);
}


RUNTIME_FUNCTION(CompareNilIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  CompareNilIC ic(isolate);
  return *ic.CompareNil(object);
}


RUNTIME_FUNCTION(Unreachable) {
  UNREACHABLE();
  CHECK(false);
  return isolate->heap()->undefined_value();
}


Builtins::JavaScript BinaryOpIC::TokenToJSBuiltin(Token::Value op) {
  switch (op) {
    default:
      UNREACHABLE();
    case Token::ADD:
      return Builtins::ADD;
      break;
    case Token::SUB:
      return Builtins::SUB;
      break;
    case Token::MUL:
      return Builtins::MUL;
      break;
    case Token::DIV:
      return Builtins::DIV;
      break;
    case Token::MOD:
      return Builtins::MOD;
      break;
    case Token::BIT_OR:
      return Builtins::BIT_OR;
      break;
    case Token::BIT_AND:
      return Builtins::BIT_AND;
      break;
    case Token::BIT_XOR:
      return Builtins::BIT_XOR;
      break;
    case Token::SAR:
      return Builtins::SAR;
      break;
    case Token::SHR:
      return Builtins::SHR;
      break;
    case Token::SHL:
      return Builtins::SHL;
      break;
  }
}


Handle<Object> ToBooleanIC::ToBoolean(Handle<Object> object) {
  ToBooleanStub stub(isolate(), target()->extra_ic_state());
  bool to_boolean_value = stub.UpdateStatus(object);
  Handle<Code> code = stub.GetCode();
  set_target(*code);
  return handle(Smi::FromInt(to_boolean_value ? 1 : 0), isolate());
}


RUNTIME_FUNCTION(ToBooleanIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  ToBooleanIC ic(isolate);
  return *ic.ToBoolean(object);
}


RUNTIME_FUNCTION(StoreCallbackProperty) {
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<JSObject> holder = args.at<JSObject>(1);
  Handle<ExecutableAccessorInfo> callback = args.at<ExecutableAccessorInfo>(2);
  Handle<Name> name = args.at<Name>(3);
  Handle<Object> value = args.at<Object>(4);
  HandleScope scope(isolate);

  DCHECK(callback->IsCompatibleReceiver(*receiver));

  Address setter_address = v8::ToCData<Address>(callback->setter());
  v8::AccessorNameSetterCallback fun =
      FUNCTION_CAST<v8::AccessorNameSetterCallback>(setter_address);
  DCHECK(fun != NULL);

  LOG(isolate, ApiNamedPropertyAccess("store", *receiver, *name));
  PropertyCallbackArguments custom_args(isolate, callback->data(), *receiver,
                                        *holder);
  custom_args.Call(fun, v8::Utils::ToLocal(name), v8::Utils::ToLocal(value));
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return *value;
}


/**
 * Attempts to load a property with an interceptor (which must be present),
 * but doesn't search the prototype chain.
 *
 * Returns |Heap::no_interceptor_result_sentinel()| if interceptor doesn't
 * provide any value for the given name.
 */
RUNTIME_FUNCTION(LoadPropertyWithInterceptorOnly) {
  DCHECK(args.length() == NamedLoadHandlerCompiler::kInterceptorArgsLength);
  Handle<Name> name_handle =
      args.at<Name>(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex);
  Handle<InterceptorInfo> interceptor_info = args.at<InterceptorInfo>(
      NamedLoadHandlerCompiler::kInterceptorArgsInfoIndex);

  // TODO(rossberg): Support symbols in the API.
  if (name_handle->IsSymbol())
    return isolate->heap()->no_interceptor_result_sentinel();
  Handle<String> name = Handle<String>::cast(name_handle);

  Address getter_address = v8::ToCData<Address>(interceptor_info->getter());
  v8::NamedPropertyGetterCallback getter =
      FUNCTION_CAST<v8::NamedPropertyGetterCallback>(getter_address);
  DCHECK(getter != NULL);

  Handle<JSObject> receiver =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex);
  Handle<JSObject> holder =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex);
  PropertyCallbackArguments callback_args(isolate, interceptor_info->data(),
                                          *receiver, *holder);
  {
    // Use the interceptor getter.
    HandleScope scope(isolate);
    v8::Handle<v8::Value> r =
        callback_args.Call(getter, v8::Utils::ToLocal(name));
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
    if (!r.IsEmpty()) {
      Handle<Object> result = v8::Utils::OpenHandle(*r);
      result->VerifyApiCallResultType();
      return *v8::Utils::OpenHandle(*r);
    }
  }

  return isolate->heap()->no_interceptor_result_sentinel();
}


static Object* ThrowReferenceError(Isolate* isolate, Name* name) {
  // If the load is non-contextual, just return the undefined result.
  // Note that both keyed and non-keyed loads may end up here.
  HandleScope scope(isolate);
  LoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  if (ic.contextual_mode() != CONTEXTUAL) {
    return isolate->heap()->undefined_value();
  }

  // Throw a reference error.
  Handle<Name> name_handle(name);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError("not_defined", HandleVector(&name_handle, 1)));
}


/**
 * Loads a property with an interceptor performing post interceptor
 * lookup if interceptor failed.
 */
RUNTIME_FUNCTION(LoadPropertyWithInterceptor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == NamedLoadHandlerCompiler::kInterceptorArgsLength);
  Handle<Name> name =
      args.at<Name>(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex);
  Handle<JSObject> receiver =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex);
  Handle<JSObject> holder =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex);

  Handle<Object> result;
  LookupIterator it(receiver, name, holder);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::GetProperty(&it));

  if (it.IsFound()) return *result;

  return ThrowReferenceError(isolate, Name::cast(args[0]));
}


RUNTIME_FUNCTION(StorePropertyWithInterceptor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<Name> name = args.at<Name>(1);
  Handle<Object> value = args.at<Object>(2);
#ifdef DEBUG
  PrototypeIterator iter(isolate, receiver,
                         PrototypeIterator::START_AT_RECEIVER);
  bool found = false;
  while (!iter.IsAtEnd(PrototypeIterator::END_AT_NON_HIDDEN)) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (current->IsJSObject() &&
        Handle<JSObject>::cast(current)->HasNamedInterceptor()) {
      found = true;
      break;
    }
  }
  DCHECK(found);
#endif
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::SetProperty(receiver, name, value, ic.strict_mode()));
  return *result;
}


RUNTIME_FUNCTION(LoadElementWithInterceptor) {
  HandleScope scope(isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  DCHECK(args.smi_at(1) >= 0);
  uint32_t index = args.smi_at(1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::GetElementWithInterceptor(receiver, receiver, index));
  return *result;
}


RUNTIME_FUNCTION(VectorLoadIC_MissFromStubFailure) {
  // TODO(mvstanton): To be enabled when ICs can accept a vector and slot
  return NULL;
}


RUNTIME_FUNCTION(VectorKeyedLoadIC_MissFromStubFailure) {
  // TODO(mvstanton): To be enabled when ICs can accept a vector and slot
  return NULL;
}


static const Address IC_utilities[] = {
#define ADDR(name) FUNCTION_ADDR(name),
    IC_UTIL_LIST(ADDR) NULL
#undef ADDR
};


Address IC::AddressFromUtilityId(IC::UtilityId id) { return IC_utilities[id]; }
}
}  // namespace v8::internal
