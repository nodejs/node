// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_PROTOTYPE_ASSIGNMENT_SEQUENCE_BUILDER_H_
#define V8_INTERPRETER_PROTOTYPE_ASSIGNMENT_SEQUENCE_BUILDER_H_

#include "src/ast/ast.h"
#include "src/objects/literal-objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

using PrototypeAssignment = std::pair<const AstRawString*, Expression*>;

class ProtoAssignmentSeqBuilder final : public ZoneObject {
 public:
  explicit ProtoAssignmentSeqBuilder(
      ZoneVector<PrototypeAssignment>&& properties)
      : properties_(std::move(properties)) {}

  ~ProtoAssignmentSeqBuilder() = default;

  ProtoAssignmentSeqBuilder(const ProtoAssignmentSeqBuilder& other) = delete;
  ProtoAssignmentSeqBuilder(ProtoAssignmentSeqBuilder&& other)
      V8_NOEXCEPT = delete;
  ProtoAssignmentSeqBuilder& operator=(const ProtoAssignmentSeqBuilder& other)
      V8_NOEXCEPT = delete;
  ProtoAssignmentSeqBuilder& operator=(ProtoAssignmentSeqBuilder&& other)
      V8_NOEXCEPT = delete;

  // Populate the boilerplate description.
  template <typename IsolateT>
  void BuildBoilerplateDescription(IsolateT* isolate, Handle<Script> script);

  // Get the boilerplate description, populating it if necessary.
  template <typename IsolateT>
  Handle<ObjectBoilerplateDescription> GetOrBuildBoilerplateDescription(
      IsolateT* isolate, Handle<Script> script) {
    if (boilerplate_description_.is_null()) {
      BuildBoilerplateDescription(isolate, script);
    }
    return boilerplate_description_;
  }

  DirectHandle<ObjectBoilerplateDescription> boilerplate_description() const {
    DCHECK(!boilerplate_description_.is_null());
    return boilerplate_description_;
  }
  template <typename IsolateT>
  static DirectHandle<Object> GetBoilerplateValue(Expression* expression,
                                                  IsolateT* isolate,
                                                  Handle<Script> script);

  const ZoneVector<PrototypeAssignment>* properties() { return &properties_; }

 private:
  ZoneVector<PrototypeAssignment> properties_;
  IndirectHandle<ObjectBoilerplateDescription> boilerplate_description_;
};

}  // namespace internal
}  // namespace v8
#endif  // V8_INTERPRETER_PROTOTYPE_ASSIGNMENT_SEQUENCE_BUILDER_H_
