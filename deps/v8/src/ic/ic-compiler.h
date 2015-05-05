// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_IC_COMPILER_H_
#define V8_IC_IC_COMPILER_H_

#include "src/ic/access-compiler.h"

namespace v8 {
namespace internal {


class PropertyICCompiler : public PropertyAccessCompiler {
 public:
  // Finds the Code object stored in the Heap::non_monomorphic_cache().
  static Code* FindPreMonomorphic(Isolate* isolate, Code::Kind kind,
                                  ExtraICState extra_ic_state);

  // Named
  static Handle<Code> ComputeLoad(Isolate* isolate, InlineCacheState ic_state,
                                  ExtraICState extra_state);
  static Handle<Code> ComputeStore(Isolate* isolate, InlineCacheState ic_state,
                                   ExtraICState extra_state);

  static Handle<Code> ComputeMonomorphic(Code::Kind kind, Handle<Name> name,
                                         Handle<Map> map, Handle<Code> handler,
                                         ExtraICState extra_ic_state);
  static Handle<Code> ComputePolymorphic(Code::Kind kind, MapHandleList* maps,
                                         CodeHandleList* handlers,
                                         int number_of_valid_maps,
                                         Handle<Name> name,
                                         ExtraICState extra_ic_state);

  // Keyed
  static Handle<Code> ComputeKeyedLoadMonomorphicHandler(
      Handle<Map> receiver_map);
  static Handle<Code> ComputeKeyedLoadMonomorphic(Handle<Map> receiver_map);

  static Handle<Code> ComputeKeyedStoreMonomorphic(
      Handle<Map> receiver_map, LanguageMode language_mode,
      KeyedAccessStoreMode store_mode);
  static Handle<Code> ComputeKeyedLoadPolymorphic(MapHandleList* receiver_maps);
  static Handle<Code> ComputeKeyedStorePolymorphic(
      MapHandleList* receiver_maps, KeyedAccessStoreMode store_mode,
      LanguageMode language_mode);

  // Compare nil
  static Handle<Code> ComputeCompareNil(Handle<Map> receiver_map,
                                        CompareNilICStub* stub);

  // Helpers
  // TODO(verwaest): Move all uses of these helpers to the PropertyICCompiler
  // and make the helpers private.
  static void GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         LanguageMode language_mode);


 private:
  PropertyICCompiler(Isolate* isolate, Code::Kind kind,
                     ExtraICState extra_ic_state = kNoExtraICState,
                     CacheHolderFlag cache_holder = kCacheOnReceiver)
      : PropertyAccessCompiler(isolate, kind, cache_holder),
        extra_ic_state_(extra_ic_state) {}

  static Handle<Code> Find(Handle<Name> name, Handle<Map> stub_holder_map,
                           Code::Kind kind,
                           ExtraICState extra_ic_state = kNoExtraICState,
                           CacheHolderFlag cache_holder = kCacheOnReceiver);

  Handle<Code> CompileLoadInitialize(Code::Flags flags);
  Handle<Code> CompileLoadPreMonomorphic(Code::Flags flags);
  Handle<Code> CompileLoadMegamorphic(Code::Flags flags);
  Handle<Code> CompileStoreInitialize(Code::Flags flags);
  Handle<Code> CompileStorePreMonomorphic(Code::Flags flags);
  Handle<Code> CompileStoreGeneric(Code::Flags flags);
  Handle<Code> CompileStoreMegamorphic(Code::Flags flags);

  Handle<Code> CompileMonomorphic(Handle<Map> map, Handle<Code> handler,
                                  Handle<Name> name, IcCheckType check);
  Handle<Code> CompilePolymorphic(MapHandleList* maps, CodeHandleList* handlers,
                                  Handle<Name> name, Code::StubType type,
                                  IcCheckType check);

  Handle<Code> CompileKeyedStoreMonomorphic(Handle<Map> receiver_map,
                                            KeyedAccessStoreMode store_mode);
  Handle<Code> CompileKeyedStorePolymorphic(MapHandleList* receiver_maps,
                                            KeyedAccessStoreMode store_mode);
  Handle<Code> CompileKeyedStorePolymorphic(MapHandleList* receiver_maps,
                                            CodeHandleList* handler_stubs,
                                            MapHandleList* transitioned_maps);

  bool IncludesNumberMap(MapHandleList* maps);

  Handle<Code> GetCode(Code::Kind kind, Code::StubType type, Handle<Name> name,
                       InlineCacheState state = MONOMORPHIC);

  Logger::LogEventsAndTags log_kind(Handle<Code> code) {
    if (kind() == Code::LOAD_IC) {
      return code->ic_state() == MONOMORPHIC ? Logger::LOAD_IC_TAG
                                             : Logger::LOAD_POLYMORPHIC_IC_TAG;
    } else if (kind() == Code::KEYED_LOAD_IC) {
      return code->ic_state() == MONOMORPHIC
                 ? Logger::KEYED_LOAD_IC_TAG
                 : Logger::KEYED_LOAD_POLYMORPHIC_IC_TAG;
    } else if (kind() == Code::STORE_IC) {
      return code->ic_state() == MONOMORPHIC ? Logger::STORE_IC_TAG
                                             : Logger::STORE_POLYMORPHIC_IC_TAG;
    } else {
      DCHECK_EQ(Code::KEYED_STORE_IC, kind());
      return code->ic_state() == MONOMORPHIC
                 ? Logger::KEYED_STORE_IC_TAG
                 : Logger::KEYED_STORE_POLYMORPHIC_IC_TAG;
    }
  }

  const ExtraICState extra_ic_state_;
};


}
}  // namespace v8::internal

#endif  // V8_IC_IC_COMPILER_H_
