// Copyright 2011 the V8 project authors. All rights reserved.
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
#include "ast.h"
#include "code-stubs.h"
#include "gdb-jit.h"
#include "ic-inl.h"
#include "stub-cache.h"
#include "vm-state-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------
// StubCache implementation.


StubCache::StubCache(Isolate* isolate) : isolate_(isolate) {
  ASSERT(isolate == Isolate::Current());
  memset(primary_, 0, sizeof(primary_[0]) * StubCache::kPrimaryTableSize);
  memset(secondary_, 0, sizeof(secondary_[0]) * StubCache::kSecondaryTableSize);
}


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
  ASSERT(!heap()->InNewSpace(name));
  ASSERT(name->IsSymbol());

  // The state bits are not important to the hash function because
  // the stub cache only contains monomorphic stubs. Make sure that
  // the bits are the least significant so they will be the ones
  // masked out.
  ASSERT(Code::ExtractICStateFromFlags(flags) == MONOMORPHIC);
  STATIC_ASSERT((Code::ICStateField::kMask & 1) == 1);

  // Make sure that the code type is not included in the hash.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Compute the primary entry.
  int primary_offset = PrimaryOffset(name, flags, map);
  Entry* primary = entry(primary_, primary_offset);
  Code* hit = primary->value;

  // If the primary entry has useful data in it, we retire it to the
  // secondary cache before overwriting it.
  if (hit != isolate_->builtins()->builtin(Builtins::kIllegal)) {
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


MaybeObject* StubCache::ComputeLoadNonexistent(String* name,
                                               JSObject* receiver) {
  ASSERT(receiver->IsGlobalObject() || receiver->HasFastProperties());
  // If no global objects are present in the prototype chain, the load
  // nonexistent IC stub can be shared for all names for a given map
  // and we use the empty string for the map cache in that case.  If
  // there are global objects involved, we need to check global
  // property cells in the stub and therefore the stub will be
  // specific to the name.
  String* cache_name = heap()->empty_string();
  if (receiver->IsGlobalObject()) cache_name = name;
  JSObject* last = receiver;
  while (last->GetPrototype() != heap()->null_value()) {
    last = JSObject::cast(last->GetPrototype());
    if (last->IsGlobalObject()) cache_name = name;
  }
  // Compile the stub that is either shared for all names or
  // name specific if there are global objects involved.
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::LOAD_IC, NONEXISTENT);
  Object* code = receiver->map()->FindInCodeCache(cache_name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadNonexistent(cache_name, receiver, last);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), cache_name));
    GDBJIT(AddCode(GDBJITInterface::LOAD_IC, cache_name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(cache_name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeLoadField(String* name,
                                         JSObject* receiver,
                                         JSObject* holder,
                                         int field_index) {
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, FIELD);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadField(receiver, holder, field_index, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeLoadCallback(String* name,
                                            JSObject* receiver,
                                            JSObject* holder,
                                            AccessorInfo* callback) {
  ASSERT(v8::ToCData<Address>(callback->getter()) != 0);
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadCallback(name, receiver, holder, callback);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeLoadConstant(String* name,
                                            JSObject* receiver,
                                            JSObject* holder,
                                            Object* value) {
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::LOAD_IC, CONSTANT_FUNCTION);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadConstant(receiver, holder, value, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeLoadInterceptor(String* name,
                                               JSObject* receiver,
                                               JSObject* holder) {
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, INTERCEPTOR);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadInterceptor(receiver, holder, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeLoadNormal() {
  return isolate_->builtins()->builtin(Builtins::kLoadIC_Normal);
}


MaybeObject* StubCache::ComputeLoadGlobal(String* name,
                                          JSObject* receiver,
                                          GlobalObject* holder,
                                          JSGlobalPropertyCell* cell,
                                          bool is_dont_delete) {
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, NORMAL);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    LoadStubCompiler compiler;
    { MaybeObject* maybe_code = compiler.CompileLoadGlobal(receiver,
                                                           holder,
                                                           cell,
                                                           name,
                                                           is_dont_delete);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeKeyedLoadField(String* name,
                                              JSObject* receiver,
                                              JSObject* holder,
                                              int field_index) {
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, FIELD);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadField(name, receiver, holder, field_index);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeKeyedLoadConstant(String* name,
                                                 JSObject* receiver,
                                                 JSObject* holder,
                                                 Object* value) {
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CONSTANT_FUNCTION);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadConstant(name, receiver, holder, value);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeKeyedLoadInterceptor(String* name,
                                                    JSObject* receiver,
                                                    JSObject* holder) {
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, INTERCEPTOR);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadInterceptor(receiver, holder, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeKeyedLoadCallback(String* name,
                                                 JSObject* receiver,
                                                 JSObject* holder,
                                                 AccessorInfo* callback) {
  ASSERT(IC::GetCodeCacheForObject(receiver, holder) == OWN_MAP);
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    { MaybeObject* maybe_code =
          compiler.CompileLoadCallback(name, receiver, holder, callback);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}



MaybeObject* StubCache::ComputeKeyedLoadArrayLength(String* name,
                                                    JSArray* receiver) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CALLBACKS);
  ASSERT(receiver->IsJSObject());
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    { MaybeObject* maybe_code = compiler.CompileLoadArrayLength(name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeKeyedLoadStringLength(String* name,
                                                     String* receiver) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CALLBACKS);
  Map* map = receiver->map();
  Object* code = map->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    { MaybeObject* maybe_code = compiler.CompileLoadStringLength(name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result = map->UpdateCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeKeyedLoadFunctionPrototype(
    String* name,
    JSFunction* receiver) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC, CALLBACKS);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedLoadStubCompiler compiler;
    { MaybeObject* maybe_code = compiler.CompileLoadFunctionPrototype(name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_LOAD_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeStoreField(String* name,
                                          JSObject* receiver,
                                          int field_index,
                                          Map* transition,
                                          StrictModeFlag strict_mode) {
  PropertyType type = (transition == NULL) ? FIELD : MAP_TRANSITION;
  Code::Flags flags = Code::ComputeMonomorphicFlags(
      Code::STORE_IC, type, strict_mode);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    StoreStubCompiler compiler(strict_mode);
    { MaybeObject* maybe_code =
          compiler.CompileStoreField(receiver, field_index, transition, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::STORE_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::STORE_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeKeyedLoadOrStoreElement(
    JSObject* receiver,
    bool is_store,
    StrictModeFlag strict_mode) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(
          is_store ? Code::KEYED_STORE_IC :
                     Code::KEYED_LOAD_IC,
          NORMAL,
          strict_mode);
  String* name = is_store
      ? isolate()->heap()->KeyedStoreElementMonomorphic_symbol()
      : isolate()->heap()->KeyedLoadElementMonomorphic_symbol();
  Object* maybe_code = receiver->map()->FindInCodeCache(name, flags);
  if (!maybe_code->IsUndefined()) return Code::cast(maybe_code);

  MaybeObject* maybe_new_code = NULL;
  Map* receiver_map = receiver->map();
  if (is_store) {
    KeyedStoreStubCompiler compiler(strict_mode);
    maybe_new_code = compiler.CompileStoreElement(receiver_map);
  } else {
    KeyedLoadStubCompiler compiler;
    maybe_new_code = compiler.CompileLoadElement(receiver_map);
  }
  Code* code;
  if (!maybe_new_code->To(&code)) return maybe_new_code;
  if (is_store) {
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_STORE_IC_TAG,
                            Code::cast(code), 0));
  } else {
    PROFILE(isolate_,
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG,
                            Code::cast(code), 0));
  }
  ASSERT(code->IsCode());
  Object* result;
  { MaybeObject* maybe_result =
        receiver->UpdateMapCodeCache(name, Code::cast(code));
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  return code;
}


MaybeObject* StubCache::ComputeStoreNormal(StrictModeFlag strict_mode) {
  return isolate_->builtins()->builtin((strict_mode == kStrictMode)
                            ? Builtins::kStoreIC_Normal_Strict
                            : Builtins::kStoreIC_Normal);
}


MaybeObject* StubCache::ComputeStoreGlobal(String* name,
                                           GlobalObject* receiver,
                                           JSGlobalPropertyCell* cell,
                                           StrictModeFlag strict_mode) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(
      Code::STORE_IC, NORMAL, strict_mode);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    StoreStubCompiler compiler(strict_mode);
    { MaybeObject* maybe_code =
          compiler.CompileStoreGlobal(receiver, cell, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::STORE_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::STORE_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeStoreCallback(
    String* name,
    JSObject* receiver,
    AccessorInfo* callback,
    StrictModeFlag strict_mode) {
  ASSERT(v8::ToCData<Address>(callback->setter()) != 0);
  Code::Flags flags = Code::ComputeMonomorphicFlags(
      Code::STORE_IC, CALLBACKS, strict_mode);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    StoreStubCompiler compiler(strict_mode);
    { MaybeObject* maybe_code =
          compiler.CompileStoreCallback(receiver, callback, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::STORE_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::STORE_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeStoreInterceptor(
    String* name,
    JSObject* receiver,
    StrictModeFlag strict_mode) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(
      Code::STORE_IC, INTERCEPTOR, strict_mode);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    StoreStubCompiler compiler(strict_mode);
    { MaybeObject* maybe_code =
          compiler.CompileStoreInterceptor(receiver, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate_,
            CodeCreateEvent(Logger::STORE_IC_TAG, Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::STORE_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeKeyedStoreField(String* name,
                                               JSObject* receiver,
                                               int field_index,
                                               Map* transition,
                                               StrictModeFlag strict_mode) {
  PropertyType type = (transition == NULL) ? FIELD : MAP_TRANSITION;
  Code::Flags flags = Code::ComputeMonomorphicFlags(
      Code::KEYED_STORE_IC, type, strict_mode);
  Object* code = receiver->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    KeyedStoreStubCompiler compiler(strict_mode);
    { MaybeObject* maybe_code =
          compiler.CompileStoreField(receiver, field_index, transition, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    PROFILE(isolate(),
            CodeCreateEvent(Logger::KEYED_STORE_IC_TAG,
                            Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_STORE_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          receiver->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}

#define CALL_LOGGER_TAG(kind, type) \
    (kind == Code::CALL_IC ? Logger::type : Logger::KEYED_##type)

MaybeObject* StubCache::ComputeCallConstant(int argc,
                                            Code::Kind kind,
                                            Code::ExtraICState extra_ic_state,
                                            String* name,
                                            Object* object,
                                            JSObject* holder,
                                            JSFunction* function) {
  // Compute the check type and the map.
  InlineCacheHolderFlag cache_holder =
      IC::GetCodeCacheForObject(object, holder);
  JSObject* map_holder = IC::GetCodeCacheHolder(object, cache_holder);

  // Compute check type based on receiver/holder.
  CheckType check = RECEIVER_MAP_CHECK;
  if (object->IsString()) {
    check = STRING_CHECK;
  } else if (object->IsNumber()) {
    check = NUMBER_CHECK;
  } else if (object->IsBoolean()) {
    check = BOOLEAN_CHECK;
  }

  Code::Flags flags = Code::ComputeMonomorphicFlags(kind,
                                                    CONSTANT_FUNCTION,
                                                    extra_ic_state,
                                                    cache_holder,
                                                    argc);
  Object* code = map_holder->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    // If the function hasn't been compiled yet, we cannot do it now
    // because it may cause GC. To avoid this issue, we return an
    // internal error which will make sure we do not update any
    // caches.
    if (!function->is_compiled()) return Failure::InternalError();
    // Compile the stub - only create stubs for fully compiled functions.
    CallStubCompiler compiler(argc, kind, extra_ic_state, cache_holder);
    { MaybeObject* maybe_code =
          compiler.CompileCallConstant(object, holder, function, name, check);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    Code::cast(code)->set_check_type(check);
    ASSERT_EQ(flags, Code::cast(code)->flags());
    PROFILE(isolate_,
            CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_IC_TAG),
                            Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::CALL_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          map_holder->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeCallField(int argc,
                                         Code::Kind kind,
                                         Code::ExtraICState extra_ic_state,
                                         String* name,
                                         Object* object,
                                         JSObject* holder,
                                         int index) {
  // Compute the check type and the map.
  InlineCacheHolderFlag cache_holder =
      IC::GetCodeCacheForObject(object, holder);
  JSObject* map_holder = IC::GetCodeCacheHolder(object, cache_holder);

  // TODO(1233596): We cannot do receiver map check for non-JS objects
  // because they may be represented as immediates without a
  // map. Instead, we check against the map in the holder.
  if (object->IsNumber() || object->IsBoolean() || object->IsString()) {
    object = holder;
  }

  Code::Flags flags = Code::ComputeMonomorphicFlags(kind,
                                                    FIELD,
                                                    extra_ic_state,
                                                    cache_holder,
                                                    argc);
  Object* code = map_holder->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    CallStubCompiler compiler(argc, kind, extra_ic_state, cache_holder);
    { MaybeObject* maybe_code =
          compiler.CompileCallField(JSObject::cast(object),
                                    holder,
                                    index,
                                    name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    ASSERT_EQ(flags, Code::cast(code)->flags());
    PROFILE(isolate_,
            CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_IC_TAG),
                            Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::CALL_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          map_holder->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeCallInterceptor(
    int argc,
    Code::Kind kind,
    Code::ExtraICState extra_ic_state,
    String* name,
    Object* object,
    JSObject* holder) {
  // Compute the check type and the map.
  InlineCacheHolderFlag cache_holder =
      IC::GetCodeCacheForObject(object, holder);
  JSObject* map_holder = IC::GetCodeCacheHolder(object, cache_holder);

  // TODO(1233596): We cannot do receiver map check for non-JS objects
  // because they may be represented as immediates without a
  // map. Instead, we check against the map in the holder.
  if (object->IsNumber() || object->IsBoolean() || object->IsString()) {
    object = holder;
  }

  Code::Flags flags = Code::ComputeMonomorphicFlags(kind,
                                                    INTERCEPTOR,
                                                    extra_ic_state,
                                                    cache_holder,
                                                    argc);
  Object* code = map_holder->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    CallStubCompiler compiler(argc, kind, extra_ic_state, cache_holder);
    { MaybeObject* maybe_code =
          compiler.CompileCallInterceptor(JSObject::cast(object), holder, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    ASSERT_EQ(flags, Code::cast(code)->flags());
    PROFILE(isolate(),
            CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_IC_TAG),
                            Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::CALL_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          map_holder->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


MaybeObject* StubCache::ComputeCallNormal(int argc,
                                          Code::Kind kind,
                                          Code::ExtraICState extra_ic_state,
                                          String* name,
                                          JSObject* receiver) {
  Object* code;
  { MaybeObject* maybe_code = ComputeCallNormal(argc, kind, extra_ic_state);
    if (!maybe_code->ToObject(&code)) return maybe_code;
  }
  return code;
}


MaybeObject* StubCache::ComputeCallGlobal(int argc,
                                          Code::Kind kind,
                                          Code::ExtraICState extra_ic_state,
                                          String* name,
                                          JSObject* receiver,
                                          GlobalObject* holder,
                                          JSGlobalPropertyCell* cell,
                                          JSFunction* function) {
  InlineCacheHolderFlag cache_holder =
      IC::GetCodeCacheForObject(receiver, holder);
  JSObject* map_holder = IC::GetCodeCacheHolder(receiver, cache_holder);
  Code::Flags flags = Code::ComputeMonomorphicFlags(kind,
                                                    NORMAL,
                                                    extra_ic_state,
                                                    cache_holder,
                                                    argc);
  Object* code = map_holder->map()->FindInCodeCache(name, flags);
  if (code->IsUndefined()) {
    // If the function hasn't been compiled yet, we cannot do it now
    // because it may cause GC. To avoid this issue, we return an
    // internal error which will make sure we do not update any
    // caches.
    if (!function->is_compiled()) return Failure::InternalError();
    CallStubCompiler compiler(argc, kind, extra_ic_state, cache_holder);
    { MaybeObject* maybe_code =
          compiler.CompileCallGlobal(receiver, holder, cell, function, name);
      if (!maybe_code->ToObject(&code)) return maybe_code;
    }
    ASSERT_EQ(flags, Code::cast(code)->flags());
    PROFILE(isolate(),
            CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_IC_TAG),
                            Code::cast(code), name));
    GDBJIT(AddCode(GDBJITInterface::CALL_IC, name, Code::cast(code)));
    Object* result;
    { MaybeObject* maybe_result =
          map_holder->UpdateMapCodeCache(name, Code::cast(code));
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return code;
}


static Object* GetProbeValue(Isolate* isolate, Code::Flags flags) {
  // Use raw_unchecked... so we don't get assert failures during GC.
  UnseededNumberDictionary* dictionary =
      isolate->heap()->raw_unchecked_non_monomorphic_cache();
  int entry = dictionary->FindEntry(isolate, flags);
  if (entry != -1) return dictionary->ValueAt(entry);
  return isolate->heap()->raw_unchecked_undefined_value();
}


MUST_USE_RESULT static MaybeObject* ProbeCache(Isolate* isolate,
                                               Code::Flags flags) {
  Heap* heap = isolate->heap();
  Object* probe = GetProbeValue(isolate, flags);
  if (probe != heap->undefined_value()) return probe;
  // Seed the cache with an undefined value to make sure that any
  // generated code object can always be inserted into the cache
  // without causing  allocation failures.
  Object* result;
  { MaybeObject* maybe_result =
        heap->non_monomorphic_cache()->AtNumberPut(flags,
                                                   heap->undefined_value());
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  heap->public_set_non_monomorphic_cache(
      UnseededNumberDictionary::cast(result));
  return probe;
}


static MaybeObject* FillCache(Isolate* isolate, MaybeObject* maybe_code) {
  Object* code;
  if (maybe_code->ToObject(&code)) {
    if (code->IsCode()) {
      Heap* heap = isolate->heap();
      int entry = heap->non_monomorphic_cache()->FindEntry(
          Code::cast(code)->flags());
      // The entry must be present see comment in ProbeCache.
      ASSERT(entry != -1);
      ASSERT(heap->non_monomorphic_cache()->ValueAt(entry) ==
             heap->undefined_value());
      heap->non_monomorphic_cache()->ValueAtPut(entry, code);
      CHECK(GetProbeValue(isolate, Code::cast(code)->flags()) == code);
    }
  }
  return maybe_code;
}


Code* StubCache::FindCallInitialize(int argc,
                                    RelocInfo::Mode mode,
                                    Code::Kind kind) {
  Code::ExtraICState extra_state =
      CallICBase::StringStubState::encode(DEFAULT_STRING_STUB) |
      CallICBase::Contextual::encode(mode == RelocInfo::CODE_TARGET_CONTEXT);
  Code::Flags flags = Code::ComputeFlags(kind,
                                         UNINITIALIZED,
                                         extra_state,
                                         NORMAL,
                                         argc);
  Object* result = ProbeCache(isolate(), flags)->ToObjectUnchecked();
  ASSERT(result != heap()->undefined_value());
  // This might be called during the marking phase of the collector
  // hence the unchecked cast.
  return reinterpret_cast<Code*>(result);
}


MaybeObject* StubCache::ComputeCallInitialize(int argc,
                                              RelocInfo::Mode mode,
                                              Code::Kind kind) {
  Code::ExtraICState extra_state =
      CallICBase::StringStubState::encode(DEFAULT_STRING_STUB) |
      CallICBase::Contextual::encode(mode == RelocInfo::CODE_TARGET_CONTEXT);
  Code::Flags flags = Code::ComputeFlags(kind,
                                         UNINITIALIZED,
                                         extra_state,
                                         NORMAL,
                                         argc);
  Object* probe;
  { MaybeObject* maybe_probe = ProbeCache(isolate_, flags);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(isolate_, compiler.CompileCallInitialize(flags));
}


Handle<Code> StubCache::ComputeCallInitialize(int argc,
                                              RelocInfo::Mode mode) {
  CALL_HEAP_FUNCTION(isolate_,
                     ComputeCallInitialize(argc, mode, Code::CALL_IC),
                     Code);
}


Handle<Code> StubCache::ComputeKeyedCallInitialize(int argc) {
  CALL_HEAP_FUNCTION(
      isolate_,
      ComputeCallInitialize(argc, RelocInfo::CODE_TARGET, Code::KEYED_CALL_IC),
      Code);
}


MaybeObject* StubCache::ComputeCallPreMonomorphic(
    int argc,
    Code::Kind kind,
    Code::ExtraICState extra_ic_state) {
  Code::Flags flags = Code::ComputeFlags(kind,
                                         PREMONOMORPHIC,
                                         extra_ic_state,
                                         NORMAL,
                                         argc);
  Object* probe;
  { MaybeObject* maybe_probe = ProbeCache(isolate_, flags);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(isolate_, compiler.CompileCallPreMonomorphic(flags));
}


MaybeObject* StubCache::ComputeCallNormal(int argc,
                                          Code::Kind kind,
                                          Code::ExtraICState extra_ic_state) {
  Code::Flags flags = Code::ComputeFlags(kind,
                                         MONOMORPHIC,
                                         extra_ic_state,
                                         NORMAL,
                                         argc);
  Object* probe;
  { MaybeObject* maybe_probe = ProbeCache(isolate_, flags);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(isolate_, compiler.CompileCallNormal(flags));
}


MaybeObject* StubCache::ComputeCallArguments(int argc, Code::Kind kind) {
  ASSERT(kind == Code::KEYED_CALL_IC);
  Code::Flags flags = Code::ComputeFlags(kind,
                                         MEGAMORPHIC,
                                         Code::kNoExtraICState,
                                         NORMAL,
                                         argc);
  Object* probe;
  { MaybeObject* maybe_probe = ProbeCache(isolate_, flags);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(isolate_, compiler.CompileCallArguments(flags));
}


MaybeObject* StubCache::ComputeCallMegamorphic(
    int argc,
    Code::Kind kind,
    Code::ExtraICState extra_ic_state) {
  Code::Flags flags = Code::ComputeFlags(kind,
                                         MEGAMORPHIC,
                                         extra_ic_state,
                                         NORMAL,
                                         argc);
  Object* probe;
  { MaybeObject* maybe_probe = ProbeCache(isolate_, flags);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(isolate_, compiler.CompileCallMegamorphic(flags));
}


MaybeObject* StubCache::ComputeCallMiss(int argc,
                                        Code::Kind kind,
                                        Code::ExtraICState extra_ic_state) {
  // MONOMORPHIC_PROTOTYPE_FAILURE state is used to make sure that miss stubs
  // and monomorphic stubs are not mixed up together in the stub cache.
  Code::Flags flags = Code::ComputeFlags(kind,
                                         MONOMORPHIC_PROTOTYPE_FAILURE,
                                         extra_ic_state,
                                         NORMAL,
                                         argc,
                                         OWN_MAP);
  Object* probe;
  { MaybeObject* maybe_probe = ProbeCache(isolate_, flags);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(isolate_, compiler.CompileCallMiss(flags));
}


#ifdef ENABLE_DEBUGGER_SUPPORT
MaybeObject* StubCache::ComputeCallDebugBreak(
    int argc,
    Code::Kind kind) {
  // Extra IC state is irrelevant for debug break ICs. They jump to
  // the actual call ic to carry out the work.
  Code::Flags flags = Code::ComputeFlags(kind,
                                         DEBUG_BREAK,
                                         Code::kNoExtraICState,
                                         NORMAL,
                                         argc);
  Object* probe;
  { MaybeObject* maybe_probe = ProbeCache(isolate_, flags);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(isolate_, compiler.CompileCallDebugBreak(flags));
}


MaybeObject* StubCache::ComputeCallDebugPrepareStepIn(
    int argc,
    Code::Kind kind) {
  // Extra IC state is irrelevant for debug break ICs. They jump to
  // the actual call ic to carry out the work.
  Code::Flags flags = Code::ComputeFlags(kind,
                                         DEBUG_PREPARE_STEP_IN,
                                         Code::kNoExtraICState,
                                         NORMAL,
                                         argc);
  Object* probe;
  { MaybeObject* maybe_probe = ProbeCache(isolate_, flags);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  if (!probe->IsUndefined()) return probe;
  StubCompiler compiler;
  return FillCache(isolate_, compiler.CompileCallDebugPrepareStepIn(flags));
}
#endif


void StubCache::Clear() {
  for (int i = 0; i < kPrimaryTableSize; i++) {
    primary_[i].key = heap()->empty_string();
    primary_[i].value = isolate_->builtins()->builtin(
        Builtins::kIllegal);
  }
  for (int j = 0; j < kSecondaryTableSize; j++) {
    secondary_[j].key = heap()->empty_string();
    secondary_[j].value = isolate_->builtins()->builtin(
        Builtins::kIllegal);
  }
}


void StubCache::CollectMatchingMaps(SmallMapList* types,
                                    String* name,
                                    Code::Flags flags) {
  for (int i = 0; i < kPrimaryTableSize; i++) {
    if (primary_[i].key == name) {
      Map* map = primary_[i].value->FindFirstMap();
      // Map can be NULL, if the stub is constant function call
      // with a primitive receiver.
      if (map == NULL) continue;

      int offset = PrimaryOffset(name, flags, map);
      if (entry(primary_, offset) == &primary_[i]) {
        types->Add(Handle<Map>(map));
      }
    }
  }

  for (int i = 0; i < kSecondaryTableSize; i++) {
    if (secondary_[i].key == name) {
      Map* map = secondary_[i].value->FindFirstMap();
      // Map can be NULL, if the stub is constant function call
      // with a primitive receiver.
      if (map == NULL) continue;

      // Lookup in primary table and skip duplicates.
      int primary_offset = PrimaryOffset(name, flags, map);
      Entry* primary_entry = entry(primary_, primary_offset);
      if (primary_entry->key == name) {
        Map* primary_map = primary_entry->value->FindFirstMap();
        if (map == primary_map) continue;
      }

      // Lookup in secondary table and add matches.
      int offset = SecondaryOffset(name, flags, primary_offset);
      if (entry(secondary_, offset) == &secondary_[i]) {
        types->Add(Handle<Map>(map));
      }
    }
  }
}


// ------------------------------------------------------------------------
// StubCompiler implementation.


RUNTIME_FUNCTION(MaybeObject*, LoadCallbackProperty) {
  ASSERT(args[0]->IsJSObject());
  ASSERT(args[1]->IsJSObject());
  AccessorInfo* callback = AccessorInfo::cast(args[3]);
  Address getter_address = v8::ToCData<Address>(callback->getter());
  v8::AccessorGetter fun = FUNCTION_CAST<v8::AccessorGetter>(getter_address);
  ASSERT(fun != NULL);
  v8::AccessorInfo info(&args[0]);
  HandleScope scope(isolate);
  v8::Handle<v8::Value> result;
  {
    // Leaving JavaScript.
    VMState state(isolate, EXTERNAL);
    ExternalCallbackScope call_scope(isolate, getter_address);
    result = fun(v8::Utils::ToLocal(args.at<String>(4)), info);
  }
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  if (result.IsEmpty()) return HEAP->undefined_value();
  return *v8::Utils::OpenHandle(*result);
}


RUNTIME_FUNCTION(MaybeObject*, StoreCallbackProperty) {
  JSObject* recv = JSObject::cast(args[0]);
  AccessorInfo* callback = AccessorInfo::cast(args[1]);
  Address setter_address = v8::ToCData<Address>(callback->setter());
  v8::AccessorSetter fun = FUNCTION_CAST<v8::AccessorSetter>(setter_address);
  ASSERT(fun != NULL);
  Handle<String> name = args.at<String>(2);
  Handle<Object> value = args.at<Object>(3);
  HandleScope scope(isolate);
  LOG(isolate, ApiNamedPropertyAccess("store", recv, *name));
  CustomArguments custom_args(isolate, callback->data(), recv, recv);
  v8::AccessorInfo info(custom_args.end());
  {
    // Leaving JavaScript.
    VMState state(isolate, EXTERNAL);
    ExternalCallbackScope call_scope(isolate, setter_address);
    fun(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), info);
  }
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
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
RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorOnly) {
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
    HandleScope scope(isolate);
    v8::Handle<v8::Value> r;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      r = getter(v8::Utils::ToLocal(name_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!r.IsEmpty()) {
      return *v8::Utils::OpenHandle(*r);
    }
  }

  return isolate->heap()->no_interceptor_result_sentinel();
}


static MaybeObject* ThrowReferenceError(String* name) {
  // If the load is non-contextual, just return the undefined result.
  // Note that both keyed and non-keyed loads may end up here, so we
  // can't use either LoadIC or KeyedLoadIC constructors.
  IC ic(IC::NO_EXTRA_FRAME, Isolate::Current());
  ASSERT(ic.target()->is_load_stub() || ic.target()->is_keyed_load_stub());
  if (!ic.SlowIsContextual()) return HEAP->undefined_value();

  // Throw a reference error.
  HandleScope scope;
  Handle<String> name_handle(name);
  Handle<Object> error =
      FACTORY->NewReferenceError("not_defined",
                                  HandleVector(&name_handle, 1));
  return Isolate::Current()->Throw(*error);
}


static MaybeObject* LoadWithInterceptor(Arguments* args,
                                        PropertyAttributes* attrs) {
  Handle<String> name_handle = args->at<String>(0);
  Handle<InterceptorInfo> interceptor_info = args->at<InterceptorInfo>(1);
  ASSERT(kAccessorInfoOffsetInInterceptorArgs == 2);
  Handle<JSObject> receiver_handle = args->at<JSObject>(2);
  Handle<JSObject> holder_handle = args->at<JSObject>(3);
  ASSERT(args->length() == 5);  // Last arg is data object.

  Isolate* isolate = receiver_handle->GetIsolate();

  Address getter_address = v8::ToCData<Address>(interceptor_info->getter());
  v8::NamedPropertyGetter getter =
      FUNCTION_CAST<v8::NamedPropertyGetter>(getter_address);
  ASSERT(getter != NULL);

  {
    // Use the interceptor getter.
    v8::AccessorInfo info(args->arguments() -
                          kAccessorInfoOffsetInInterceptorArgs);
    HandleScope scope(isolate);
    v8::Handle<v8::Value> r;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      r = getter(v8::Utils::ToLocal(name_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!r.IsEmpty()) {
      *attrs = NONE;
      return *v8::Utils::OpenHandle(*r);
    }
  }

  MaybeObject* result = holder_handle->GetPropertyPostInterceptor(
      *receiver_handle,
      *name_handle,
      attrs);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return result;
}


/**
 * Loads a property with an interceptor performing post interceptor
 * lookup if interceptor failed.
 */
RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorForLoad) {
  PropertyAttributes attr = NONE;
  Object* result;
  { MaybeObject* maybe_result = LoadWithInterceptor(&args, &attr);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // If the property is present, return it.
  if (attr != ABSENT) return result;
  return ThrowReferenceError(String::cast(args[0]));
}


RUNTIME_FUNCTION(MaybeObject*, LoadPropertyWithInterceptorForCall) {
  PropertyAttributes attr;
  MaybeObject* result = LoadWithInterceptor(&args, &attr);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  // This is call IC. In this case, we simply return the undefined result which
  // will lead to an exception when trying to invoke the result as a
  // function.
  return result;
}


RUNTIME_FUNCTION(MaybeObject*, StoreInterceptorProperty) {
  ASSERT(args.length() == 4);
  JSObject* recv = JSObject::cast(args[0]);
  String* name = String::cast(args[1]);
  Object* value = args[2];
  StrictModeFlag strict_mode = static_cast<StrictModeFlag>(args.smi_at(3));
  ASSERT(strict_mode == kStrictMode || strict_mode == kNonStrictMode);
  ASSERT(recv->HasNamedInterceptor());
  PropertyAttributes attr = NONE;
  MaybeObject* result = recv->SetPropertyWithInterceptor(
      name, value, attr, strict_mode);
  return result;
}


RUNTIME_FUNCTION(MaybeObject*, KeyedLoadPropertyWithInterceptor) {
  JSObject* receiver = JSObject::cast(args[0]);
  ASSERT(args.smi_at(1) >= 0);
  uint32_t index = args.smi_at(1);
  return receiver->GetElementWithInterceptor(receiver, index);
}


MaybeObject* StubCompiler::CompileCallInitialize(Code::Flags flags) {
  HandleScope scope(isolate());
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  Code::Kind kind = Code::ExtractKindFromFlags(flags);
  Code::ExtraICState extra_ic_state = Code::ExtractExtraICStateFromFlags(flags);
  if (kind == Code::CALL_IC) {
    CallIC::GenerateInitialize(masm(), argc, extra_ic_state);
  } else {
    KeyedCallIC::GenerateInitialize(masm(), argc);
  }
  Object* result;
  { MaybeObject* maybe_result =
        GetCodeWithFlags(flags, "CompileCallInitialize");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  isolate()->counters()->call_initialize_stubs()->Increment();
  Code* code = Code::cast(result);
  USE(code);
  PROFILE(isolate(),
          CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_INITIALIZE_TAG),
                          code, code->arguments_count()));
  GDBJIT(AddCode(GDBJITInterface::CALL_INITIALIZE, Code::cast(code)));
  return result;
}


MaybeObject* StubCompiler::CompileCallPreMonomorphic(Code::Flags flags) {
  HandleScope scope(isolate());
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  // The code of the PreMonomorphic stub is the same as the code
  // of the Initialized stub.  They just differ on the code object flags.
  Code::Kind kind = Code::ExtractKindFromFlags(flags);
  Code::ExtraICState extra_ic_state = Code::ExtractExtraICStateFromFlags(flags);
  if (kind == Code::CALL_IC) {
    CallIC::GenerateInitialize(masm(), argc, extra_ic_state);
  } else {
    KeyedCallIC::GenerateInitialize(masm(), argc);
  }
  Object* result;
  { MaybeObject* maybe_result =
        GetCodeWithFlags(flags, "CompileCallPreMonomorphic");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  isolate()->counters()->call_premonomorphic_stubs()->Increment();
  Code* code = Code::cast(result);
  USE(code);
  PROFILE(isolate(),
          CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_PRE_MONOMORPHIC_TAG),
                          code, code->arguments_count()));
  GDBJIT(AddCode(GDBJITInterface::CALL_PRE_MONOMORPHIC, Code::cast(code)));
  return result;
}


MaybeObject* StubCompiler::CompileCallNormal(Code::Flags flags) {
  HandleScope scope(isolate());
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  Code::Kind kind = Code::ExtractKindFromFlags(flags);
  if (kind == Code::CALL_IC) {
    // Call normal is always with a explict receiver.
    ASSERT(!CallIC::Contextual::decode(
        Code::ExtractExtraICStateFromFlags(flags)));
    CallIC::GenerateNormal(masm(), argc);
  } else {
    KeyedCallIC::GenerateNormal(masm(), argc);
  }
  Object* result;
  { MaybeObject* maybe_result = GetCodeWithFlags(flags, "CompileCallNormal");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  isolate()->counters()->call_normal_stubs()->Increment();
  Code* code = Code::cast(result);
  USE(code);
  PROFILE(isolate(),
          CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_NORMAL_TAG),
                          code, code->arguments_count()));
  GDBJIT(AddCode(GDBJITInterface::CALL_NORMAL, Code::cast(code)));
  return result;
}


MaybeObject* StubCompiler::CompileCallMegamorphic(Code::Flags flags) {
  HandleScope scope(isolate());
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  Code::Kind kind = Code::ExtractKindFromFlags(flags);
  Code::ExtraICState extra_ic_state = Code::ExtractExtraICStateFromFlags(flags);
  if (kind == Code::CALL_IC) {
    CallIC::GenerateMegamorphic(masm(), argc, extra_ic_state);
  } else {
    KeyedCallIC::GenerateMegamorphic(masm(), argc);
  }
  Object* result;
  { MaybeObject* maybe_result =
        GetCodeWithFlags(flags, "CompileCallMegamorphic");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  isolate()->counters()->call_megamorphic_stubs()->Increment();
  Code* code = Code::cast(result);
  USE(code);
  PROFILE(isolate(),
          CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_MEGAMORPHIC_TAG),
                          code, code->arguments_count()));
  GDBJIT(AddCode(GDBJITInterface::CALL_MEGAMORPHIC, Code::cast(code)));
  return result;
}


MaybeObject* StubCompiler::CompileCallArguments(Code::Flags flags) {
  HandleScope scope(isolate());
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  KeyedCallIC::GenerateNonStrictArguments(masm(), argc);
  Code::Kind kind = Code::ExtractKindFromFlags(flags);
  Object* result;
  { MaybeObject* maybe_result =
        GetCodeWithFlags(flags, "CompileCallArguments");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Code* code = Code::cast(result);
  USE(code);
  PROFILE(isolate(),
          CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_MEGAMORPHIC_TAG),
                          code, code->arguments_count()));
  GDBJIT(AddCode(GDBJITInterface::CALL_MEGAMORPHIC, Code::cast(code)));
  return result;
}


MaybeObject* StubCompiler::CompileCallMiss(Code::Flags flags) {
  HandleScope scope(isolate());
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  Code::Kind kind = Code::ExtractKindFromFlags(flags);
  Code::ExtraICState extra_ic_state = Code::ExtractExtraICStateFromFlags(flags);
  if (kind == Code::CALL_IC) {
    CallIC::GenerateMiss(masm(), argc, extra_ic_state);
  } else {
    KeyedCallIC::GenerateMiss(masm(), argc);
  }
  Object* result;
  { MaybeObject* maybe_result = GetCodeWithFlags(flags, "CompileCallMiss");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  isolate()->counters()->call_megamorphic_stubs()->Increment();
  Code* code = Code::cast(result);
  USE(code);
  PROFILE(isolate(),
          CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_MISS_TAG),
                          code, code->arguments_count()));
  GDBJIT(AddCode(GDBJITInterface::CALL_MISS, Code::cast(code)));
  return result;
}


#ifdef ENABLE_DEBUGGER_SUPPORT
MaybeObject* StubCompiler::CompileCallDebugBreak(Code::Flags flags) {
  HandleScope scope(isolate());
  Debug::GenerateCallICDebugBreak(masm());
  Object* result;
  { MaybeObject* maybe_result =
        GetCodeWithFlags(flags, "CompileCallDebugBreak");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Code* code = Code::cast(result);
  USE(code);
  Code::Kind kind = Code::ExtractKindFromFlags(flags);
  USE(kind);
  PROFILE(isolate(),
          CodeCreateEvent(CALL_LOGGER_TAG(kind, CALL_DEBUG_BREAK_TAG),
                          code, code->arguments_count()));
  return result;
}


MaybeObject* StubCompiler::CompileCallDebugPrepareStepIn(Code::Flags flags) {
  HandleScope scope(isolate());
  // Use the same code for the the step in preparations as we do for
  // the miss case.
  int argc = Code::ExtractArgumentsCountFromFlags(flags);
  Code::Kind kind = Code::ExtractKindFromFlags(flags);
  if (kind == Code::CALL_IC) {
    // For the debugger extra ic state is irrelevant.
    CallIC::GenerateMiss(masm(), argc, Code::kNoExtraICState);
  } else {
    KeyedCallIC::GenerateMiss(masm(), argc);
  }
  Object* result;
  { MaybeObject* maybe_result =
        GetCodeWithFlags(flags, "CompileCallDebugPrepareStepIn");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Code* code = Code::cast(result);
  USE(code);
  PROFILE(isolate(),
          CodeCreateEvent(
              CALL_LOGGER_TAG(kind, CALL_DEBUG_PREPARE_STEP_IN_TAG),
              code,
              code->arguments_count()));
  return result;
}
#endif

#undef CALL_LOGGER_TAG

MaybeObject* StubCompiler::GetCodeWithFlags(Code::Flags flags,
                                            const char* name) {
  // Check for allocation failures during stub compilation.
  if (failure_->IsFailure()) return failure_;

  // Create code object in the heap.
  CodeDesc desc;
  masm_.GetCode(&desc);
  MaybeObject* result = heap()->CreateCode(desc, flags, masm_.CodeObject());
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code_stubs && !result->IsFailure()) {
    Code::cast(result->ToObjectUnchecked())->Disassemble(name);
  }
#endif
  return result;
}


MaybeObject* StubCompiler::GetCodeWithFlags(Code::Flags flags, String* name) {
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
    if (!proto->IsNull()) {
      proto->Lookup(name, lookup);
    }
  }
}



MaybeObject* LoadStubCompiler::GetCode(PropertyType type, String* name) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::LOAD_IC, type);
  MaybeObject* result = GetCodeWithFlags(flags, name);
  if (!result->IsFailure()) {
    PROFILE(isolate(),
            CodeCreateEvent(Logger::LOAD_IC_TAG,
                            Code::cast(result->ToObjectUnchecked()),
                            name));
    GDBJIT(AddCode(GDBJITInterface::LOAD_IC,
                   name,
                   Code::cast(result->ToObjectUnchecked())));
  }
  return result;
}


MaybeObject* KeyedLoadStubCompiler::GetCode(PropertyType type,
                                            String* name,
                                            InlineCacheState state) {
  Code::Flags flags = Code::ComputeFlags(
      Code::KEYED_LOAD_IC, state, Code::kNoExtraICState, type);
  MaybeObject* result = GetCodeWithFlags(flags, name);
  if (!result->IsFailure()) {
    PROFILE(isolate(),
            CodeCreateEvent(Logger::KEYED_LOAD_IC_TAG,
                            Code::cast(result->ToObjectUnchecked()),
                            name));
    GDBJIT(AddCode(GDBJITInterface::LOAD_IC,
                   name,
                   Code::cast(result->ToObjectUnchecked())));
  }
  return result;
}


MaybeObject* StoreStubCompiler::GetCode(PropertyType type, String* name) {
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::STORE_IC, type, strict_mode_);
  MaybeObject* result = GetCodeWithFlags(flags, name);
  if (!result->IsFailure()) {
    PROFILE(isolate(),
            CodeCreateEvent(Logger::STORE_IC_TAG,
                            Code::cast(result->ToObjectUnchecked()),
                            name));
    GDBJIT(AddCode(GDBJITInterface::STORE_IC,
                   name,
                   Code::cast(result->ToObjectUnchecked())));
  }
  return result;
}


MaybeObject* KeyedStoreStubCompiler::GetCode(PropertyType type,
                                             String* name,
                                             InlineCacheState state) {
  Code::Flags flags =
      Code::ComputeFlags(Code::KEYED_STORE_IC, state, strict_mode_, type);
  MaybeObject* result = GetCodeWithFlags(flags, name);
  if (!result->IsFailure()) {
    PROFILE(isolate(),
            CodeCreateEvent(Logger::KEYED_STORE_IC_TAG,
                            Code::cast(result->ToObjectUnchecked()),
                            name));
    GDBJIT(AddCode(GDBJITInterface::KEYED_STORE_IC,
                   name,
                   Code::cast(result->ToObjectUnchecked())));
  }
  return result;
}


void KeyedStoreStubCompiler::GenerateStoreDictionaryElement(
    MacroAssembler* masm) {
  KeyedStoreIC::GenerateSlow(masm);
}


CallStubCompiler::CallStubCompiler(int argc,
                                   Code::Kind kind,
                                   Code::ExtraICState extra_ic_state,
                                   InlineCacheHolderFlag cache_holder)
    : arguments_(argc),
      kind_(kind),
      extra_ic_state_(extra_ic_state),
      cache_holder_(cache_holder) {
}


bool CallStubCompiler::HasCustomCallGenerator(JSFunction* function) {
  SharedFunctionInfo* info = function->shared();
  if (info->HasBuiltinFunctionId()) {
    BuiltinFunctionId id = info->builtin_function_id();
#define CALL_GENERATOR_CASE(name) if (id == k##name) return true;
    CUSTOM_CALL_IC_GENERATORS(CALL_GENERATOR_CASE)
#undef CALL_GENERATOR_CASE
  }
  CallOptimization optimization(function);
  if (optimization.is_simple_api_call()) {
    return true;
  }
  return false;
}


MaybeObject* CallStubCompiler::CompileCustomCall(Object* object,
                                                 JSObject* holder,
                                                 JSGlobalPropertyCell* cell,
                                                 JSFunction* function,
                                                 String* fname) {
  ASSERT(HasCustomCallGenerator(function));

  SharedFunctionInfo* info = function->shared();
  if (info->HasBuiltinFunctionId()) {
    BuiltinFunctionId id = info->builtin_function_id();
#define CALL_GENERATOR_CASE(name)                           \
    if (id == k##name) {                                    \
      return CallStubCompiler::Compile##name##Call(object,  \
                                                  holder,   \
                                                  cell,     \
                                                  function, \
                                                  fname);   \
    }
    CUSTOM_CALL_IC_GENERATORS(CALL_GENERATOR_CASE)
#undef CALL_GENERATOR_CASE
  }
  CallOptimization optimization(function);
  ASSERT(optimization.is_simple_api_call());
  return CompileFastApiCall(optimization,
                            object,
                            holder,
                            cell,
                            function,
                            fname);
}


MaybeObject* CallStubCompiler::GetCode(PropertyType type, String* name) {
  int argc = arguments_.immediate();
  Code::Flags flags = Code::ComputeMonomorphicFlags(kind_,
                                                    type,
                                                    extra_ic_state_,
                                                    cache_holder_,
                                                    argc);
  return GetCodeWithFlags(flags, name);
}


MaybeObject* CallStubCompiler::GetCode(JSFunction* function) {
  String* function_name = NULL;
  if (function->shared()->name()->IsString()) {
    function_name = String::cast(function->shared()->name());
  }
  return GetCode(CONSTANT_FUNCTION, function_name);
}


MaybeObject* ConstructStubCompiler::GetCode() {
  Code::Flags flags = Code::ComputeFlags(Code::STUB);
  Object* result;
  { MaybeObject* maybe_result = GetCodeWithFlags(flags, "ConstructStub");
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Code* code = Code::cast(result);
  USE(code);
  PROFILE(isolate(), CodeCreateEvent(Logger::STUB_TAG, code, "ConstructStub"));
  GDBJIT(AddCode(GDBJITInterface::STUB, "ConstructStub", Code::cast(code)));
  return result;
}


CallOptimization::CallOptimization(LookupResult* lookup) {
  if (!lookup->IsProperty() || !lookup->IsCacheable() ||
      lookup->type() != CONSTANT_FUNCTION) {
    Initialize(NULL);
  } else {
    // We only optimize constant function calls.
    Initialize(lookup->GetConstantFunction());
  }
}

CallOptimization::CallOptimization(JSFunction* function) {
  Initialize(function);
}


int CallOptimization::GetPrototypeDepthOfExpectedType(JSObject* object,
                                                      JSObject* holder) const {
  ASSERT(is_simple_api_call_);
  if (expected_receiver_type_ == NULL) return 0;
  int depth = 0;
  while (object != holder) {
    if (object->IsInstanceOf(expected_receiver_type_)) return depth;
    object = JSObject::cast(object->GetPrototype());
    ++depth;
  }
  if (holder->IsInstanceOf(expected_receiver_type_)) return depth;
  return kInvalidProtoDepth;
}


void CallOptimization::Initialize(JSFunction* function) {
  constant_function_ = NULL;
  is_simple_api_call_ = false;
  expected_receiver_type_ = NULL;
  api_call_info_ = NULL;

  if (function == NULL || !function->is_compiled()) return;

  constant_function_ = function;
  AnalyzePossibleApiFunction(function);
}


void CallOptimization::AnalyzePossibleApiFunction(JSFunction* function) {
  SharedFunctionInfo* sfi = function->shared();
  if (!sfi->IsApiFunction()) return;
  FunctionTemplateInfo* info = sfi->get_api_func_data();

  // Require a C++ callback.
  if (info->call_code()->IsUndefined()) return;
  api_call_info_ = CallHandlerInfo::cast(info->call_code());

  // Accept signatures that either have no restrictions at all or
  // only have restrictions on the receiver.
  if (!info->signature()->IsUndefined()) {
    SignatureInfo* signature = SignatureInfo::cast(info->signature());
    if (!signature->args()->IsUndefined()) return;
    if (!signature->receiver()->IsUndefined()) {
      expected_receiver_type_ =
          FunctionTemplateInfo::cast(signature->receiver());
    }
  }

  is_simple_api_call_ = true;
}


} }  // namespace v8::internal
