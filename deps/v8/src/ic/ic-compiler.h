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
  // Keyed
  static Handle<Code> ComputeKeyedLoadMonomorphicHandler(
      Handle<Map> receiver_map, ExtraICState extra_ic_state);

  static Handle<Code> ComputeKeyedStoreMonomorphicHandler(
      Handle<Map> receiver_map, KeyedAccessStoreMode store_mode);
  static void ComputeKeyedStorePolymorphicHandlers(
      MapHandleList* receiver_maps, MapHandleList* transitioned_maps,
      CodeHandleList* handlers, KeyedAccessStoreMode store_mode);

  // Helpers
  // TODO(verwaest): Move all uses of these helpers to the PropertyICCompiler
  // and make the helpers private.
  static void GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         LanguageMode language_mode);


 private:
  explicit PropertyICCompiler(Isolate* isolate)
      : PropertyAccessCompiler(isolate, Code::KEYED_STORE_IC,
                               kCacheOnReceiver) {}

  Handle<Code> CompileKeyedStoreMonomorphicHandler(
      Handle<Map> receiver_map, KeyedAccessStoreMode store_mode);
  void CompileKeyedStorePolymorphicHandlers(MapHandleList* receiver_maps,
                                            MapHandleList* transitioned_maps,
                                            CodeHandleList* handlers,
                                            KeyedAccessStoreMode store_mode);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_IC_IC_COMPILER_H_
