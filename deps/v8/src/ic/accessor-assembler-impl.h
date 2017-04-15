// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SRC_IC_ACCESSOR_ASSEMBLER_IMPL_H_
#define V8_SRC_IC_ACCESSOR_ASSEMBLER_IMPL_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}

using compiler::Node;

#define ACCESSOR_ASSEMBLER_PUBLIC_INTERFACE(V) \
  V(LoadIC)                                    \
  V(LoadField)                                 \
  V(LoadICTrampoline)                          \
  V(KeyedLoadICTF)                             \
  V(KeyedLoadICTrampolineTF)                   \
  V(KeyedLoadICMegamorphic)                    \
  V(StoreIC)                                   \
  V(StoreICTrampoline)
// The other IC entry points need custom handling because of additional
// parameters like "typeof_mode" or "language_mode".

class AccessorAssemblerImpl : public CodeStubAssembler {
 public:
  explicit AccessorAssemblerImpl(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

#define DECLARE_PUBLIC_METHOD(Name) void Generate##Name();

  ACCESSOR_ASSEMBLER_PUBLIC_INTERFACE(DECLARE_PUBLIC_METHOD)
#undef DECLARE_PUBLIC_METHOD

  void GenerateLoadICProtoArray(bool throw_reference_error_if_nonexistent);

  void GenerateLoadGlobalIC(TypeofMode typeof_mode);
  void GenerateLoadGlobalICTrampoline(TypeofMode typeof_mode);

  void GenerateKeyedStoreICTF(LanguageMode language_mode);
  void GenerateKeyedStoreICTrampolineTF(LanguageMode language_mode);

  void TryProbeStubCache(StubCache* stub_cache, Node* receiver, Node* name,
                         Label* if_handler, Variable* var_handler,
                         Label* if_miss);

  Node* StubCachePrimaryOffsetForTesting(Node* name, Node* map) {
    return StubCachePrimaryOffset(name, map);
  }
  Node* StubCacheSecondaryOffsetForTesting(Node* name, Node* map) {
    return StubCacheSecondaryOffset(name, map);
  }

 protected:
  struct LoadICParameters {
    LoadICParameters(Node* context, Node* receiver, Node* name, Node* slot,
                     Node* vector)
        : context(context),
          receiver(receiver),
          name(name),
          slot(slot),
          vector(vector) {}

    Node* context;
    Node* receiver;
    Node* name;
    Node* slot;
    Node* vector;
  };

  struct StoreICParameters : public LoadICParameters {
    StoreICParameters(Node* context, Node* receiver, Node* name, Node* value,
                      Node* slot, Node* vector)
        : LoadICParameters(context, receiver, name, slot, vector),
          value(value) {}
    Node* value;
  };

  enum ElementSupport { kOnlyProperties, kSupportElements };
  void HandleStoreICHandlerCase(
      const StoreICParameters* p, Node* handler, Label* miss,
      ElementSupport support_elements = kOnlyProperties);

 private:
  // Stub generation entry points.

  void LoadIC(const LoadICParameters* p);
  void LoadICProtoArray(const LoadICParameters* p, Node* handler,
                        bool throw_reference_error_if_nonexistent);
  void LoadGlobalIC(const LoadICParameters* p, TypeofMode typeof_mode);
  void KeyedLoadIC(const LoadICParameters* p);
  void KeyedLoadICGeneric(const LoadICParameters* p);
  void StoreIC(const StoreICParameters* p);
  void KeyedStoreIC(const StoreICParameters* p, LanguageMode language_mode);

  // IC dispatcher behavior.

  // Checks monomorphic case. Returns {feedback} entry of the vector.
  Node* TryMonomorphicCase(Node* slot, Node* vector, Node* receiver_map,
                           Label* if_handler, Variable* var_handler,
                           Label* if_miss);
  void HandlePolymorphicCase(Node* receiver_map, Node* feedback,
                             Label* if_handler, Variable* var_handler,
                             Label* if_miss, int unroll_count);
  void HandleKeyedStorePolymorphicCase(Node* receiver_map, Node* feedback,
                                       Label* if_handler, Variable* var_handler,
                                       Label* if_transition_handler,
                                       Variable* var_transition_map_cell,
                                       Label* if_miss);

  // LoadIC implementation.

  void HandleLoadICHandlerCase(
      const LoadICParameters* p, Node* handler, Label* miss,
      ElementSupport support_elements = kOnlyProperties);

  void HandleLoadICSmiHandlerCase(const LoadICParameters* p, Node* holder,
                                  Node* smi_handler, Label* miss,
                                  ElementSupport support_elements);

  void HandleLoadICProtoHandlerCase(const LoadICParameters* p, Node* handler,
                                    Variable* var_holder,
                                    Variable* var_smi_handler,
                                    Label* if_smi_handler, Label* miss,
                                    bool throw_reference_error_if_nonexistent);

  Node* EmitLoadICProtoArrayCheck(const LoadICParameters* p, Node* handler,
                                  Node* handler_length, Node* handler_flags,
                                  Label* miss,
                                  bool throw_reference_error_if_nonexistent);

  // LoadGlobalIC implementation.

  void HandleLoadGlobalICHandlerCase(const LoadICParameters* p, Node* handler,
                                     Label* miss,
                                     bool throw_reference_error_if_nonexistent);

  // StoreIC implementation.

  void HandleStoreICElementHandlerCase(const StoreICParameters* p,
                                       Node* handler, Label* miss);

  void HandleStoreICProtoHandler(const StoreICParameters* p, Node* handler,
                                 Label* miss);
  // If |transition| is nullptr then the normal field store is generated or
  // transitioning store otherwise.
  void HandleStoreICSmiHandlerCase(Node* handler_word, Node* holder,
                                   Node* value, Node* transition, Label* miss);
  // If |transition| is nullptr then the normal field store is generated or
  // transitioning store otherwise.
  void HandleStoreFieldAndReturn(Node* handler_word, Node* holder,
                                 Representation representation, Node* value,
                                 Node* transition, Label* miss);

  // Low-level helpers.

  Node* PrepareValueForStore(Node* handler_word, Node* holder,
                             Representation representation, Node* transition,
                             Node* value, Label* bailout);

  // Extends properties backing store by JSObject::kFieldsAdded elements.
  void ExtendPropertiesBackingStore(Node* object);

  void StoreNamedField(Node* handler_word, Node* object, bool is_inobject,
                       Representation representation, Node* value,
                       bool transition_to_field);

  void EmitFastElementsBoundsCheck(Node* object, Node* elements,
                                   Node* intptr_index,
                                   Node* is_jsarray_condition, Label* miss);
  void EmitElementLoad(Node* object, Node* elements, Node* elements_kind,
                       Node* key, Node* is_jsarray_condition, Label* if_hole,
                       Label* rebox_double, Variable* var_double_value,
                       Label* unimplemented_elements_kind, Label* out_of_bounds,
                       Label* miss);
  void CheckPrototype(Node* prototype_cell, Node* name, Label* miss);
  void NameDictionaryNegativeLookup(Node* object, Node* name, Label* miss);

  // Stub cache access helpers.

  // This enum is used here as a replacement for StubCache::Table to avoid
  // including stub cache header.
  enum StubCacheTable : int;

  Node* StubCachePrimaryOffset(Node* name, Node* map);
  Node* StubCacheSecondaryOffset(Node* name, Node* seed);

  void TryProbeStubCacheTable(StubCache* stub_cache, StubCacheTable table_id,
                              Node* entry_offset, Node* name, Node* map,
                              Label* if_handler, Variable* var_handler,
                              Label* if_miss);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SRC_IC_ACCESSOR_ASSEMBLER_IMPL_H_
