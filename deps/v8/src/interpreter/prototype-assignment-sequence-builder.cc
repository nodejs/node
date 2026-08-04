// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/prototype-assignment-sequence-builder.h"

#include "src/ast/ast.h"
#include "src/base/logging.h"
#include "src/codegen/compiler.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/literal-objects.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {
// Populate the boilerplate description.
template <typename IsolateT>
void ProtoAssignmentSeqBuilder::BuildBoilerplateDescription(
    IsolateT* isolate, Handle<Script> script) {
  if (!boilerplate_description_.is_null()) return;
  int prop_sz = static_cast<int>(properties_.size());
  Handle<ObjectBoilerplateDescription> boilerplate_description =
      isolate->factory()->NewObjectBoilerplateDescription(prop_sz, prop_sz, 0,
                                                          false);

  int position = 0;
  for (size_t i = 0; i < properties_.size(); i++) {
    PrototypeAssignment pair = properties_.at(i);
    const AstRawString* key_str = pair.first;

    DirectHandle<Object> key = Cast<Object>(key_str->string());
    DirectHandle<Object> value =
        GetBoilerplateValue(pair.second, isolate, script);
    boilerplate_description->set_key_value(position++, *key, *value);
  }

  boilerplate_description->set_flags(AggregateLiteral::kNoFlags);

  boilerplate_description_ = boilerplate_description;
}

template EXPORT_TEMPLATE_DEFINE(V8_BASE_EXPORT) void ProtoAssignmentSeqBuilder::
    BuildBoilerplateDescription(Isolate* isolate, Handle<Script> script);
template EXPORT_TEMPLATE_DEFINE(V8_BASE_EXPORT) void ProtoAssignmentSeqBuilder::
    BuildBoilerplateDescription(LocalIsolate* isolate, Handle<Script> script);

// static
template <typename IsolateT>
DirectHandle<Object> ProtoAssignmentSeqBuilder::GetBoilerplateValue(
    Expression* expression, IsolateT* isolate, Handle<Script> script) {
  if (expression->IsLiteral()) {
    return expression->AsLiteral()->BuildValue(isolate);
  }
  if (expression->IsFunctionLiteral()) {
    DirectHandle<SharedFunctionInfo> shared_info =
        Compiler::GetSharedFunctionInfo(expression->AsFunctionLiteral(), script,
                                        isolate);
    DCHECK(!shared_info.is_null());
    return shared_info;
  }
  UNREACHABLE();
}
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    DirectHandle<Object> ProtoAssignmentSeqBuilder::GetBoilerplateValue(
        Expression* expression, Isolate* isolate, Handle<Script> script);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    DirectHandle<Object> ProtoAssignmentSeqBuilder::GetBoilerplateValue(
        Expression* expression, LocalIsolate* isolate, Handle<Script> script);
}  // namespace internal
}  // namespace v8
