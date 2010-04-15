// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#include "api.h"
#include "arguments.h"
#include "ic-inl.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------
// StubCache implementation.


StubCache::Entry StubCache::primary_[StubCache::kPrimaryTableSize];
StubCache::Entry StubCache::secondary_[StubCache::kSecondaryTableSize];

void StubCache::Initialize(bool create_heap_objects) {
  ASSERT(IsPowerOf2(kPrimaryTableSize));
  ASSERT(IsPowerOf2(kSecondaryTableSize));
  if (create_heap_objects) {
    HandleScope scope;
    Clear();
  }
}


Code* StubCache::Set(String* name, Map* map, Code* code) {
  // Get the flags from the code.
  Code::Flags flags = Code::RemoveTypeFromFlags(code->flags());

  // Validate that the name does not move on scavenge, and that we
  // can use identity checks instead of string equality checks.
  ASSERT(!Heap::InNewSpace(name));
  ASSERT(name->IsSymbol());

  // The state bits are not important to the hash function because
  // the stub cache only contains monomorphic stubs. Make sure that
  // the bits are the least significant so they will be the ones
  // masked out.
  ASSERT(Code::ExtractICStateFromFlags(flags) == MONOMORPHIC);
  ASSERT(Code::kFlagsICStateShift == 0);

  // Make sure that the code type is not included in the hash.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Compute the primary entry.
  int primary_offset = PrimaryOffset(name, flags, map);
  Entry* primary = entry(primary_, primary_offset);
  Code* hit = primary->value;

  // If the primary entry has useful data in it, we retire it to the
  // secondary cache before overwriting it.
  if (hit != Builtins::builtin(Builtins::Illegal)) {
    Code::Flags primary_flags = Code::RemoveTypeFromFlags(hit->flags());
    int secondary_offset =
        SecondaryOffset(primary->key, primary_flags, primary_offset);
    Entry* secondary = entry(secondary_, secondary_offset);
    *secondary = *primary;
  }

  // Update primary cache.
  primary->key = name;
  primary->value = code;
  return code;
}


Object* StubCache::ComputeLoadField(String* name,
                                    JSObject* receiver,
                                    JSObject* holder,
                                    int field_index) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, FIELD);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    code = compiler.CompileLoadField(receiver, holder, field_index, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeLoadCallback(String* name,
                                       JSObject* receiver,
                                       JSObject* holder,
                                       AccessorInfo* callback) {
  ASSERT(v8::ToCData<Address>(callback->getter()) != 0);
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    code = compiler.CompileLoadCallback(name, receiver, holder, callback);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeLoadConstant(String* name,
                                       JSObject* receiver,
                                       JSObject* holder,
                                       Object* value) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::LOAD_IC, CONSTANT_FUNCTION);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    code = compiler.CompileLoadConstant(receiver, holder, value, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeLoadInterceptor(String* name,
                                          JSObject* receiver,
                                          JSObject* holder) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, INTERCEPTOR);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    code = compiler.CompileLoadInterceptor(receiver, holder, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeLoadNormal(String* name, JSObject* receiver) {
  Code* code = Builtins::builtin(Builtins::LoadIC_Normal);
  return Set(name, receiver->map(), code);
}


Object* StubCache::ComputeLoadGlobal(String* name,
                                     JSObject* receiver,
                                     GlobalObject* holder,
                                     JSGlobalPropertyCell* cell,
                                     bool is_dont_delete) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, NORMAL);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    code = compiler.CompileLoadGlobal(receiver,
                                      holder,
                                      cell,
                                      name,
                                      is_dont_delete);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeKeyedLoadField(String* name,
                                         JSObject* receiver,
                                         JSObject* holder,
                                         int field_index) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, FIELD);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    code = compiler.CompileLoadField(name, receiver, holder, field_index);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return code;
}


Object* StubCache::ComputeKeyedLoadConstant(String* name,
                                            JSObject* receiver,
                                            JSObject* holder,
                                            Object* value) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CONSTANT_FUNCTION);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    code = compiler.CompileLoadConstant(name, receiver, holder, value);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return code;
}


Object* StubCache::ComputeKeyedLoadInterceptor(String* name,
                                               JSObject* receiver,
                                               JSObject* holder) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, INTERCEPTOR);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    code = compiler.CompileLoadInterceptor(receiver, holder, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return code;
}


Object* StubCache::ComputeKeyedLoadCallback(String* name,
                                            JSObject* receiver,
                                            JSObject* holder,
                                            AccessorInfo* callback) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    code = compiler.CompileLoadCallback(name, receiver, holder, callback);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return code;
}



Object* StubCache::ComputeKeyedLoadArrayLength(String* name,
                                               JSArray* receiver) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    code = compiler.CompileLoadArrayLength(name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return code;
}


Object* StubCache::ComputeKeyedLoadStringLength(String* name,
                                                String* receiver) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    code = compiler.CompileLoadStringLength(name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return code;
}


Object* StubCache::ComputeKeyedLoadFunctionPrototype(String* name,
                                                     JSFunction* receiver) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    code = compiler.CompileLoadFunctionPrototype(name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return code;
}


Object* StubCache::ComputeStoreField(String* name,
                                     JSObject* receiver,
                                     int field_index,
                                     Map* transition) {
  PropertyType type = (transition == NULL) ? FIELD : MAP_TRANSITION;
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::STORE_IC, type);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    StoreStubCompiler compiler;
    code = compiler.CompileStoreField(receiver, field_index, transition, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::STORE_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeStoreGlobal(String* name,
                                      GlobalObject* receiver,
                                      JSGlobalPropertyCell* cell) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::STORE_IC, NORMAL);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    StoreStubCompiler compiler;
    code = compiler.CompileStoreGlobal(receiver, cell, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeStoreCallback(String* name,
                                        JSObject* receiver,
                                        AccessorInfo* callback) {
  ASSERT(v8::ToCData<Address>(callback->setter()) != 0);
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::STORE_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    StoreStubCompiler compiler;
    code = compiler.CompileStoreCallback(receiver, callback, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::STORE_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeStoreInterceptor(String* name,
                                           JSObject* receiver) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::STORE_IC, INTERCEPTOR);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    StoreStubCompiler compiler;
    code = compiler.CompileStoreInterceptor(receiver, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(Logger::STORE_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeKeyedStoreField(String* name, JSObject* receiver,
                                          int field_index, Map* transition) {
  PropertyType type = (transition == NULL) ? FIELD : MAP_TRANSITION;
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::KEYED_STORE_IC, type);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedStoreStubCompiler compiler;
    code = compiler.CompileStoreField(receiver, field_index, transition, name);
    if (code->IsFailure()) return code;
    PROFILE(CodeCreateEvent(
        Logger::KEYED_STORE_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return code;
}


Object* StubCache::ComputeCallConstant(int argc,
                                       InLoopFlag in_loop,
                                       String* name,
                                       Object* object,
                                       JSObject* holder,
                                       JSFunction* function) {
  // Compute the check type and the map.
  Map* map = IC::GetCodeCacheMapForObject(object);

  // Compute check type based on receiver/holder.
  StubCompiler::CheckType check = StubCompiler::RECEIVER_MAP_CHECK;
  if (object->IsString()) {
    check = StubCompiler::STRING_CHECK;
  } else if (object->IsNumber()) {
    check = StubCompiler::NUMBER_CHECK;
  } else if (object->IsBoolean()) {
    check = StubCompiler::BOOLEAN_CHECK;
  }

  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::CALL_IC,
                                    CONSTANT_FUNCTION,
                                    in_loop,
                                    argc);
  Object* code = map->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    // If the function hasn't been compiled yet, we cannot do it now
    // because it may cause GC. To avoid this issue, we return an
    // internal error which will make sure we do not update any
    // caches.
    if (!function->is_compiled()) return Failure::InternalError();
    // Compile the stub - only create stubs for fully compiled functions.
    CallStubCompiler compiler(argc, in_loop);
    code = compiler.CompileCallConstant(object, holder, function, name, check);
    if (code->IsFailure()) return code;
    ASSERT_EQ(flags, Code::cast(code)->flags());
    PROFILE(CodeCreateEvent(Logger::CALL_IC_TAG, Code::cast(code), name));
    Object* result = map->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, map, Code::cast(code));
}


Object* StubCache::ComputeCallField(int argc,
                                    InLoopFlag in_loop,
                                    String* name,
                                    Object* object,
                                    JSObject* holder,
                                    int index) {
  // Compute the check type and the map.
  Map* map = IC::GetCodeCacheMapForObject(object);

  // TODO(1233596): We cannot do receiver map check for non-JS objects
  // because they may be represented as immediates without a
  // map. Instead, we check against the map in the holder.
  if (object->IsNumber() || object->IsBoolean() || object->IsString()) {
    object = holder;
  }

  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::CALL_IC,
                                                    FIELD,
                                                    in_loop,
                                                    argc);
  Object* code = map->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    CallStubCompiler compiler(argc, in_loop);
    code = compiler.CompileCallField(JSObject::cast(object),
                                     holder,
                                     index,
                                     name);
    if (code->IsFailure()) return code;
    ASSERT_EQ(flags, Code::cast(code)->flags());
    PROFILE(CodeCreateEvent(Logger::CALL_IC_TAG, Code::cast(code), name));
    Object* result = map->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, map, Code::cast(code));
}


Object* StubCache::ComputeCallInterceptor(int argc,
                                          String* name,
                                          Object* object,
                                          JSObject* holder) {
  // Compute the check type and the map.
  // If the object is a value, we use the prototype map for the cache.
  Map* map = IC::GetCodeCacheMapForObject(object);

  // TODO(1233596): We cannot do receiver map check for non-JS objects
  // because they may be represented as immediates without a
  // map. Instead, we check against the map in the holder.
  if (object->IsNumber() || object->IsBoolean() || object->IsString()) {
    object = holder;
  }

  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::CALL_IC,
                                    INTERCEPTOR,
                                    NOT_IN_LOOP,
                                    argc);
  Object* code = map->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    CallStubCompiler compiler(argc, NOT_IN_LOOP);
    code = compiler.CompileCallInterceptor(JSObject::cast(object),
                                           holder,
                                           name);
    if (code->IsFailure()) return code;
    ASSERT_EQ(flags, Code::cast(code)->flags());
    PROFILE(CodeCreateEvent(Logger::CALL_IC_TAG, Code::cast(code), name));
    Object* result = map->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, map, Code::cast(code));
}


Object* StubCache::ComputeCallNormal(int argc,
                                     InLoopFlag in_loop,
                                     String* name,
                                     JSObject* receiver) {
  Object* code = ComputeCallNormal(argc, in_loop);
  if (code->IsFailure()) return code;
  return Set(name, receiver->map(), Code::cast(code));
}


Object* StubCache::ComputeCallGlobal(int argc,
                                     InLoopFlag in_loop,
                                     String* name,
                                     JSObject* receiver,
                                     GlobalObject* holder,
                                     JSGlobalPropertyCell* cell,
                                     JSFunction* function) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::CALL_IC, NORMAL, in_loop, argc);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    // If the function hasn't been compiled yet, we cannot do it now
    // because it may cause GC. To avoid this issue, we return an
    // internal error which will make sure we do not update any
    // caches.
    if (!function->is_compiled()) return Failure::InternalError();
    CallStubCompiler compiler(argc, in_loop);
    code = compiler.CompileCallGlobal(receiver, holder, cell, function, name);
    if (code->IsFailure()) return code;
    ASSERT_EQ(flags, Code::cast(code)->flags());
    PROFILE(CodeCreateEvent(Logger::CALL_IC_TAG, Code::cast(code), name));
    Object* result = receiver->map()->UpdateCodeCache(name, Code::cast(code));
    if (result->IsFailure()) return result;
  }
  return Set(name, receiver->map(), Code::cast(code));
}


static Object* GetProbeValue(Code::Flags flags) {
  // Use raw_unchecked... so we don't get assert failures during GC.
  NumberDictionary* dictionary = Heap::raw_unchecked_non_monomorphic_cache();
  int entry = dictionary->FindEntry(flags);
  if (entry != -1) return dictionary->ValueAt(entry);
  return Heap::raw_unchecked_undefined_value();
}


static Object* ProbeCache(Code::Flags flags) {
  Object* probe = GetProbeValue(flags);
  if (probe != Heap::undefined_value()) return probe;
  // Seed the cache with an undefined value to make sure that any
  // generated code object can always be inserted into the cache
  // without causing  allocation failures.
  Object* result =
      Heap::non_monomorphic_cache()->AtNumberPut(flags,
                                                 Heap::undefined_value());
  if (result->IsFailure()) return result;
  Heap::public_set_non_monomorphic_cache(NumberDictionary::cast(result));
  return probe;
}


static Object* FillCache(Object* code) {
  if (code->IsCode()) {
    int entry =
        Heap::non_monomorphic_cache()->FindEntry(
            Code::cast(code)->flags());
    // The entry must be present see comment in ProbeCache.
    ASSERT(entry != -1);
    ASSERT(Heap::non_monomorphic_cache()->ValueAt(entry) ==
           Heap::undefined_value());
    Heap::non_monomorphic_cache()->ValueAtPut(entry, code);
    CHECK(GetProbeValue(Code::cast(code)->flags()) == code);
  }
  return code;
}


Code* StubCache::FindCallInitialize(int argc, InLoopFlag in_loop) {
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, in_loop, UNINITIALIZED, NORMAL, argc);
  Object* result = ProbeCache(flags);
  ASSERT(!result->IsUndefined());
  // This might be called during the marking phase of the collector
  // hence the unchecked cast.
  return reinterpret_cast<Code*>(result);
}


Object* StubCache::ComputeCallInitialize(int argc, InLoopFlag in_loop) {
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, in_loop, UNINITIALIZED, NORMAL, argc);
  Object* probe = ProbeCache(flags);
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(compiler.CompileCallInitialize(flags));
}


Object* StubCache::ComputeCallPreMonomorphic(int argc, InLoopFlag in_loop) {
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, in_loop, PREMONOMORPHIC, NORMAL, argc);
  Object* probe = ProbeCache(flags);
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(compiler.CompileCallPreMonomorphic(flags));
}


Object* StubCache::ComputeCallNormal(int argc, InLoopFlag in_loop) {
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, in_loop, MONOMORPHIC, NORMAL, argc);
  Object* probe = ProbeCache(flags);
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(compiler.CompileCallNormal(flags));
}


Object* StubCache::ComputeCallMegamorphic(int argc, InLoopFlag in_loop) {
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, in_loop, MEGAMORPHIC, NORMAL, argc);
  Object* probe = ProbeCache(flags);
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(compiler.CompileCallMegamorphic(flags));
}


Object* StubCache::ComputeCallMiss(int argc) {
  Code::Flags flags =
      Code::ComputeFlags(Code::STUB, NOT_IN_LOOP, MEGAMORPHIC, NORMAL, argc);
  Object* probe = ProbeCache(flags);
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(compiler.CompileCallMiss(flags));
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Object* StubCache::ComputeCallDebugBreak(int argc) {
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, NOT_IN_LOOP, DEBUG_BREAK, NORMAL, argc);
  Object* probe = ProbeCache(flags);
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(compiler.CompileCallDebugBreak(flags));
}


Object* StubCache::ComputeCallDebugPrepareStepIn(int argc) {
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC,
                         NOT_IN_LOOP,
                         DEBUG_PREPARE_STEP_IN,
                         NORMAL,
                         argc);
  Object* probe = ProbeCache(flags);
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(compiler.CompileCallDebugPrepareStepIn(flags));
}
#endif


Object* StubCache::ComputeLazyCompile(int argc) {
  Code::Flags flags =
      Code::ComputeFlags(Code::STUB, NOT_IN_LOOP, UNINITIALIZED, NORMAL, argc);
  Object* probe = ProbeCache(flags);
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  Object* result = FillCache(compiler.CompileLazyCompile(flags));
  if (result->IsCode()) {
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::LAZY_COMPILE_TAG,
                            code, code->arguments_count()));
  }
  return result;
}


void StubCache::Clear() {
  for (int i = 0; i < kPrimaryTableSize; i++) {
    primary_[i].key = Heap::empty_string();
    primary_[i].value = Builtins::builtin(Builtins::Illegal);
  }
  for (int j = 0; j < kSecondaryTableSize; j++) {
    secondary_[j].key = Heap::empty_string();
    secondary_[j].value = Builtins::builtin(Builtins::Illegal);
  }
}


// ------------------------------------------------------------------------
// StubCompiler implementation.


// Support function for computing call IC miss stubs.
Handle<Code> ComputeCallMiss(int argc) {
  CALL_HEAP_FUNCTION(StubCache::ComputeCallMiss(argc), Code);
}



Object* LoadCallbackProperty(Arguments args) {
  ASSERT(args[0]->IsJSObject());
  ASSERT(args[1]->IsJSObject());
  AccessorInfo* callback = AccessorInfo::cast(args[2]);
  Address getter_address = v8::ToCData<Address>(callback->getter());
  v8::AccessorGetter fun = FUNCTION_CAST<v8::AccessorGetter>(getter_address);
  ASSERT(fun != NULL);
  CustomArguments custom_args(callback->data(),
                              JSObject::cast(args[0]),
                              JSObject::cast(args[1]));
  v8::AccessorInfo info(custom_args.end());
  HandleScope scope;
  v8::Handle<v8::Value> result;
  {
    // Leaving JavaScript.
    VMState state(EXTERNAL);
#ifdef ENABLE_LOGGING_AND_PROFILING
    state.set_external_callback(getter_address);
#endif
    result = fun(v8::Utils::ToLocal(args.at<String>(4)), info);
  }
  RETURN_IF_SCHEDULED_EXCEPTION();
  if (result.IsEmpty()) return Heap::undefined_value();
  return *v8::Utils::OpenHandle(*result);
}


Object* StoreCallbackProperty(Arguments args) {
  JSObject* recv = JSObject::cast(args[0]);
  AccessorInfo* callback = AccessorInfo::cast(args[1]);
  Address setter_address = v8::ToCData<Address>(callback->setter());
  v8::AccessorSetter fun = FUNCTION_CAST<v8::AccessorSetter>(setter_address);
  ASSERT(fun != NULL);
  Handle<String> name = args.at<String>(2);
  Handle<Object> value = args.at<Object>(3);
  HandleScope scope;
  LOG(ApiNamedPropertyAccess("store", recv, *name));
  CustomArguments custom_args(callback->data(), recv, recv);
  v8::AccessorInfo info(custom_args.end());
  {
    // Leaving JavaScript.
    VMState state(EXTERNAL);
#ifdef ENABLE_LOGGING_AND_PROFILING
    state.set_external_callback(setter_address);
#endif
    fun(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), info);
  }
  RETURN_IF_SCHEDULED_EXCEPTION();
  return *value;
}


static const int kAccessorInfoOffsetInInterceptorArgs = 2;


/**
 * Attempts to load a property with an interceptor (which must be present),
 * but doesn't search the prototype chain.
 *
 * Returns |Heap::no_interceptor_result_sentinel()| if interceptor doesn't
 * provide any value for the given name.
 */
Object* LoadPropertyWithInterceptorOnly(Arguments args) {
  Handle<String> name_handle = args.at<String>(0);
  Handle<InterceptorInfo> interceptor_info = args.at<InterceptorInfo>(1);
  ASSERT(kAccessorInfoOffsetInInterceptorArgs == 2);
  ASSERT(args[2]->IsJSObject());  // Receiver.
  ASSERT(args[3]->IsJSObject());  // Holder.
  ASSERT(args.length() == 5);  // Last arg is data object.

  Address getter_address = v8::ToCData<Address>(interceptor_info->getter());
  v8::NamedPropertyGetter getter =
      FUNCTION_CAST<v8::NamedPropertyGetter>(getter_address);
  ASSERT(getter != NULL);

  {
    // Use the interceptor getter.
    v8::AccessorInfo info(args.arguments() -
                          kAccessorInfoOffsetInInterceptorArgs);
    HandleScope scope;
    v8::Handle<v8::Value> r;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      r = getter(v8::Utils::ToLocal(name_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (!r.IsEmpty()) {
      return *v8::Utils::OpenHandle(*r);
    }
  }

  return Heap::no_interceptor_result_sentinel();
}


static Object* ThrowReferenceError(String* name) {
  // If the load is non-contextual, just return the undefined result.
  // Note that both keyed and non-keyed loads may end up here, so we
  // can't use either LoadIC or KeyedLoadIC constructors.
  IC ic(IC::NO_EXTRA_FRAME);
  ASSERT(ic.target()->is_load_stub() || ic.target()->is_keyed_load_stub());
  if (!ic.SlowIsContextual()) return Heap::undefined_value();

  // Throw a reference error.
  HandleScope scope;
  Handle<String> name_handle(name);
  Handle<Object> error =
      Factory::NewReferenceError("not_defined",
                                  HandleVector(&name_handle, 1));
  return Top::Throw(*error);
}


static Object* LoadWithInterceptor(Arguments* args,
                                   PropertyAttributes* attrs) {
  Handle<String> name_handle = args->at<String>(0);
  Handle<InterceptorInfo> interceptor_info = args->at<InterceptorInfo>(1);
  ASSERT(kAccessorInfoOffsetInInterceptorArgs == 2);
  Handle<JSObject> receiver_handle = args->at<JSObject>(2);
  Handle<JSObject> holder_handle = args->at<JSObject>(3);
  ASSERT(args->length() == 5);  // Last arg is data object.

  Address getter_address = v8::ToCData<Address>(interceptor_info->getter());
  v8::NamedPropertyGetter getter =
      FUNCTION_CAST<v8::NamedPropertyGetter>(getter_address);
  ASSERT(getter != NULL);

  {
    // Use the interceptor getter.
    v8::AccessorInfo info(args->arguments() -
                          kAccessorInfoOffsetInInterceptorArgs);
    HandleScope scope;
    v8::Handle<v8::Value> r;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      r = getter(v8::Utils::ToLocal(name_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (!r.IsEmpty()) {
      *attrs = NONE;
      return *v8::Utils::OpenHandle(*r);
    }
  }

  Object* result = holder_handle->GetPropertyPostInterceptor(
      *receiver_handle,
      *name_handle,
      attrs);
  RETURN_IF_SCHEDULED_EXCEPTION();
  return result;
}


/**
 * Loads a property with an interceptor performing post interceptor
 * lookup if interceptor failed.
 */
Object* LoadPropertyWithInterceptorForLoad(Arguments args) {
  PropertyAttributes attr = NONE;
  Object* result = LoadWithInterceptor(&args, &attr);
  if (result->IsFailure()) return result;

  // If the property is present, return it.
  if (attr != ABSENT) return result;
  return ThrowReferenceError(String::cast(args[0]));
}


Object* LoadPropertyWithInterceptorForCall(Arguments args) {
  PropertyAttributes attr;
  Object* result = LoadWithInterceptor(&args, &attr);
  RETURN_IF_SCHEDULED_EXCEPTION();
  // This is call IC. In this case, we simply return the undefined result which
  // will lead to an exception when trying to invoke the result as a
  // function.
  return result;
}


Object* StoreInterceptorProperty(Arguments args) {
  JSObject* recv = JSObject::cast(args[0]);
  String* name = String::cast(args[1]);
  Object* value = args[2];
  ASSERT(recv->HasNamedInterceptor());
  PropertyAttributes attr = NONE;
  Object* result = recv->SetPropertyWithInterceptor(name, value, attr);
  return result;
}


Object* KeyedLoadPropertyWithInterceptor(Arguments args) {
  JSObject* receiver = JSObject::cast(args[0]);
  uint32_t index = Smi::cast(args[1])->value();
  return receiver->GetElementWithInterceptor(receiver, index);
}


Object* StubCompiler::CompileCallInitialize(Code::Flags flags) {
  HandleScope scope;
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  CallIC::GenerateInitialize(masm(), argc);
  Object* result = GetCodeWithFlags(flags, "CompileCallInitialize");
  if (!result->IsFailure()) {
    Counters::call_initialize_stubs.Increment();
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::CALL_INITIALIZE_TAG,
                            code, code->arguments_count()));
  }
  return result;
}


Object* StubCompiler::CompileCallPreMonomorphic(Code::Flags flags) {
  HandleScope scope;
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  // The code of the PreMonomorphic stub is the same as the code
  // of the Initialized stub.  They just differ on the code object flags.
  CallIC::GenerateInitialize(masm(), argc);
  Object* result = GetCodeWithFlags(flags, "CompileCallPreMonomorphic");
  if (!result->IsFailure()) {
    Counters::call_premonomorphic_stubs.Increment();
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::CALL_PRE_MONOMORPHIC_TAG,
                            code, code->arguments_count()));
  }
  return result;
}


Object* StubCompiler::CompileCallNormal(Code::Flags flags) {
  HandleScope scope;
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  CallIC::GenerateNormal(masm(), argc);
  Object* result = GetCodeWithFlags(flags, "CompileCallNormal");
  if (!result->IsFailure()) {
    Counters::call_normal_stubs.Increment();
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::CALL_NORMAL_TAG,
                            code, code->arguments_count()));
  }
  return result;
}


Object* StubCompiler::CompileCallMegamorphic(Code::Flags flags) {
  HandleScope scope;
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  CallIC::GenerateMegamorphic(masm(), argc);
  Object* result = GetCodeWithFlags(flags, "CompileCallMegamorphic");
  if (!result->IsFailure()) {
    Counters::call_megamorphic_stubs.Increment();
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::CALL_MEGAMORPHIC_TAG,
                            code, code->arguments_count()));
  }
  return result;
}


Object* StubCompiler::CompileCallMiss(Code::Flags flags) {
  HandleScope scope;
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  CallIC::GenerateMiss(masm(), argc);
  Object* result = GetCodeWithFlags(flags, "CompileCallMiss");
  if (!result->IsFailure()) {
    Counters::call_megamorphic_stubs.Increment();
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::CALL_MISS_TAG,
                            code, code->arguments_count()));
  }
  return result;
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Object* StubCompiler::CompileCallDebugBreak(Code::Flags flags) {
  HandleScope scope;
  Debug::GenerateCallICDebugBreak(masm());
  Object* result = GetCodeWithFlags(flags, "CompileCallDebugBreak");
  if (!result->IsFailure()) {
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::CALL_DEBUG_BREAK_TAG,
                            code, code->arguments_count()));
  }
  return result;
}


Object* StubCompiler::CompileCallDebugPrepareStepIn(Code::Flags flags) {
  HandleScope scope;
  // Use the same code for the the step in preparations as we do for
  // the miss case.
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  CallIC::GenerateMiss(masm(), argc);
  Object* result = GetCodeWithFlags(flags, "CompileCallDebugPrepareStepIn");
  if (!result->IsFailure()) {
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::CALL_DEBUG_PREPARE_STEP_IN_TAG,
                            code, code->arguments_count()));
  }
  return result;
}
#endif


Object* StubCompiler::GetCodeWithFlags(Code::Flags flags, const char* name) {
  // Check for allocation failures during stub compilation.
  if (failure_->IsFailure()) return failure_;

  // Create code object in the heap.
  CodeDesc desc;
  masm_.GetCode(&desc);
  Object* result = Heap::CreateCode(desc, NULL, flags, masm_.CodeObject());
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code_stubs && !result->IsFailure()) {
    Code::cast(result)->Disassemble(name);
  }
#endif
  return result;
}


Object* StubCompiler::GetCodeWithFlags(Code::Flags flags, String* name) {
  if (FLAG_print_code_stubs && (name != NULL)) {
    return GetCodeWithFlags(flags, *name->ToCString());
  }
  return GetCodeWithFlags(flags, reinterpret_cast<char*>(NULL));
}


void StubCompiler::LookupPostInterceptor(JSObject* holder,
                                         String* name,
                                         LookupResult* lookup) {
  holder->LocalLookupRealNamedProperty(name, lookup);
  if (!lookup->IsProperty()) {
    lookup->NotFound();
    Object* proto = holder->GetPrototype();
    if (proto != Heap::null_value()) {
      proto->Lookup(name, lookup);
    }
  }
}



Object* LoadStubCompiler::GetCode(PropertyType type, String* name) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, type);
  return GetCodeWithFlags(flags, name);
}


Object* KeyedLoadStubCompiler::GetCode(PropertyType type, String* name) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, type);
  return GetCodeWithFlags(flags, name);
}


Object* StoreStubCompiler::GetCode(PropertyType type, String* name) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::STORE_IC, type);
  return GetCodeWithFlags(flags, name);
}


Object* KeyedStoreStubCompiler::GetCode(PropertyType type, String* name) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::KEYED_STORE_IC, type);
  return GetCodeWithFlags(flags, name);
}


Object* CallStubCompiler::GetCode(PropertyType type, String* name) {
  int argc = arguments_.immediate();
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::CALL_IC,
                                                    type,
                                                    in_loop_,
                                                    argc);
  return GetCodeWithFlags(flags, name);
}


Object* ConstructStubCompiler::GetCode() {
  Code::Flags flags = Code::ComputeFlags(Code::STUB);
  Object* result = GetCodeWithFlags(flags, "ConstructStub");
  if (!result->IsFailure()) {
    Code* code = Code::cast(result);
    USE(code);
    PROFILE(CodeCreateEvent(Logger::STUB_TAG, code, "ConstructStub"));
  }
  return result;
}


} }  // namespace v8::internal
