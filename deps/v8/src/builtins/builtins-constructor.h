// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;
typedef compiler::CodeAssemblerState CodeAssemblerState;
typedef compiler::CodeAssemblerLabel CodeAssemblerLabel;

class ConstructorBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ConstructorBuiltinsAssembler(CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* EmitFastNewClosure(Node* shared_info, Node* feedback_vector, Node* slot,
                           Node* context);
  Node* EmitFastNewFunctionContext(Node* closure, Node* slots, Node* context,
                                   ScopeType scope_type);
  static int MaximumFunctionContextSlots();

  Node* EmitFastCloneRegExp(Node* closure, Node* literal_index, Node* pattern,
                            Node* flags, Node* context);
  Node* EmitFastCloneShallowArray(Node* closure, Node* literal_index,
                                  Node* context,
                                  CodeAssemblerLabel* call_runtime,
                                  AllocationSiteMode allocation_site_mode);

  // Maximum number of elements in copied array (chosen so that even an array
  // backed by a double backing store will fit into new-space).
  static const int kMaximumClonedShallowArrayElements =
      JSArray::kInitialMaxFastElementArray * kPointerSize / kDoubleSize;

  void CreateFastCloneShallowArrayBuiltin(
      AllocationSiteMode allocation_site_mode);

  // Maximum number of properties in copied objects.
  static const int kMaximumClonedShallowObjectProperties = 6;
  static int FastCloneShallowObjectPropertiesCount(int literal_length);
  Node* EmitFastCloneShallowObject(CodeAssemblerLabel* call_runtime,
                                   Node* closure, Node* literals_index,
                                   Node* properties_count);
  void CreateFastCloneShallowObjectBuiltin(int properties_count);

  Node* EmitFastNewObject(Node* context, Node* target, Node* new_target);

  Node* EmitFastNewObject(Node* context, Node* target, Node* new_target,
                          CodeAssemblerLabel* call_runtime);

 private:
  static const int kMaximumSlots = 0x8000;
  static const int kSmallMaximumSlots = 10;

  Node* NonEmptyShallowClone(Node* boilerplate, Node* boilerplate_map,
                             Node* boilerplate_elements, Node* allocation_site,
                             Node* capacity, ElementsKind kind);

  // FastNewFunctionContext can only allocate closures which fit in the
  // new space.
  STATIC_ASSERT(((kMaximumSlots + Context::MIN_CONTEXT_SLOTS) * kPointerSize +
                 FixedArray::kHeaderSize) < kMaxRegularHeapObjectSize);
};

}  // namespace internal
}  // namespace v8
