// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/kythe-data.h"

namespace v8 {
namespace internal {
namespace torque {

namespace {

KythePosition MakeKythePosition(const SourcePosition& pos) {
  KythePosition p;
  if (pos.source.IsValid()) {
    p.file_path = SourceFileMap::PathFromV8Root(pos.source);
  } else {
    p.file_path = "UNKNOWN";
  }
  p.start_offset = pos.start.offset;
  p.end_offset = pos.end.offset;
  return p;
}

}  // namespace

// Constants
kythe_entity_t KytheData::AddConstantDefinition(const Value* constant) {
  DCHECK(constant->IsNamespaceConstant() || constant->IsExternConstant());
  KytheData* that = &KytheData::Get();
  // Check if we know the constant already.
  auto it = that->constants_.find(constant);
  if (it != that->constants_.end()) return it->second;

  // Register this constant.
  KythePosition pos = MakeKythePosition(constant->name()->pos);
  kythe_entity_t constant_id = that->consumer_->AddDefinition(
      KytheConsumer::Kind::Constant, constant->name()->value, pos);
  that->constants_.insert(it, std::make_pair(constant, constant_id));
  return constant_id;
}

void KytheData::AddConstantUse(SourcePosition use_position,
                               const Value* constant) {
  DCHECK(constant->IsNamespaceConstant() || constant->IsExternConstant());
  KytheData* that = &Get();
  kythe_entity_t constant_id = AddConstantDefinition(constant);
  KythePosition use_pos = MakeKythePosition(use_position);
  that->consumer_->AddUse(KytheConsumer::Kind::Constant, constant_id, use_pos);
}

// Callables
kythe_entity_t KytheData::AddFunctionDefinition(Callable* callable) {
  KytheData* that = &KytheData::Get();
  // Check if we know the caller already.
  auto it = that->callables_.find(callable);
  if (it != that->callables_.end()) return it->second;

  // Register this callable.
  auto ident_pos = callable->IdentifierPosition();
  kythe_entity_t callable_id = that->consumer_->AddDefinition(
      KytheConsumer::Kind::Function, callable->ExternalName(),
      MakeKythePosition(ident_pos));
  that->callables_.insert(it, std::make_pair(callable, callable_id));
  return callable_id;
}

void KytheData::AddCall(Callable* caller, SourcePosition call_position,
                        Callable* callee) {
  if (!caller) return;  // Ignore those for now.
  DCHECK_NOT_NULL(caller);
  DCHECK_NOT_NULL(callee);
  KytheData* that = &Get();
  if (call_position.source.IsValid()) {
    kythe_entity_t caller_id = AddFunctionDefinition(caller);
    kythe_entity_t callee_id = AddFunctionDefinition(callee);

    KythePosition call_pos = MakeKythePosition(call_position);
    that->consumer_->AddCall(KytheConsumer::Kind::Function, caller_id, call_pos,
                             callee_id);
  }
}

// Class fields
kythe_entity_t KytheData::AddClassFieldDefinition(const Field* field) {
  DCHECK(field);
  KytheData* that = &KytheData::Get();
  // Check if we know that field already.
  auto it = that->class_fields_.find(field);
  if (it != that->class_fields_.end()) return it->second;
  // Register this field.
  KythePosition pos = MakeKythePosition(field->pos);
  kythe_entity_t field_id = that->consumer_->AddDefinition(
      KytheConsumer::Kind::ClassField, field->name_and_type.name, pos);
  that->class_fields_.insert(it, std::make_pair(field, field_id));
  return field_id;
}

void KytheData::AddClassFieldUse(SourcePosition use_position,
                                 const Field* field) {
  DCHECK(field);
  KytheData* that = &KytheData::Get();
  kythe_entity_t field_id = AddClassFieldDefinition(field);

  KythePosition use_pos = MakeKythePosition(use_position);
  that->consumer_->AddUse(KytheConsumer::Kind::ClassField, field_id, use_pos);
}

// Bindings
kythe_entity_t KytheData::AddBindingDefinition(Binding<LocalValue>* binding) {
  CHECK(binding);
  const uint64_t binding_index = binding->unique_index();
  return AddBindingDefinitionImpl(binding_index, binding->name(),
                                  binding->declaration_position());
}

kythe_entity_t KytheData::AddBindingDefinition(Binding<LocalLabel>* binding) {
  CHECK(binding);
  const uint64_t binding_index = binding->unique_index();
  return AddBindingDefinitionImpl(binding_index, binding->name(),
                                  binding->declaration_position());
}

kythe_entity_t KytheData::AddBindingDefinitionImpl(
    uint64_t binding_index, const std::string& name,
    const SourcePosition& ident_pos) {
  KytheData* that = &KytheData::Get();
  // Check if we know the binding already.
  auto it = that->local_bindings_.find(binding_index);
  if (it != that->local_bindings_.end()) return it->second;
  // Register this binding.
  kythe_entity_t binding_id = that->consumer_->AddDefinition(
      KytheConsumer::Kind::Variable, name, MakeKythePosition(ident_pos));
  that->local_bindings_.insert(it, std::make_pair(binding_index, binding_id));
  return binding_id;
}

void KytheData::AddBindingUse(SourcePosition use_position,
                              Binding<LocalValue>* binding) {
  CHECK(binding);
  KytheData* that = &KytheData::Get();
  kythe_entity_t binding_id = AddBindingDefinition(binding);

  KythePosition use_pos = MakeKythePosition(use_position);
  that->consumer_->AddUse(KytheConsumer::Kind::Variable, binding_id, use_pos);
}

void KytheData::AddBindingUse(SourcePosition use_position,
                              Binding<LocalLabel>* binding) {
  CHECK(binding);
  KytheData* that = &KytheData::Get();
  kythe_entity_t binding_id = AddBindingDefinition(binding);

  KythePosition use_pos = MakeKythePosition(use_position);
  that->consumer_->AddUse(KytheConsumer::Kind::Variable, binding_id, use_pos);
}

// Types
kythe_entity_t KytheData::AddTypeDefinition(const Declarable* type_decl) {
  CHECK(type_decl);
  KytheData* that = &KytheData::Get();
  // Check if we know that type already.
  auto it = that->types_.find(type_decl);
  if (it != that->types_.end()) return it->second;
  // Register this type.
  KythePosition pos = MakeKythePosition(type_decl->IdentifierPosition());
  kythe_entity_t type_id = that->consumer_->AddDefinition(
      KytheConsumer::Kind::Type, type_decl->type_name(), pos);
  that->types_.insert(it, std::make_pair(type_decl, type_id));
  return type_id;
}

void KytheData::AddTypeUse(SourcePosition use_position,
                           const Declarable* type_decl) {
  CHECK(type_decl);
  KytheData* that = &KytheData::Get();
  kythe_entity_t type_id = AddTypeDefinition(type_decl);

  KythePosition use_pos = MakeKythePosition(use_position);
  that->consumer_->AddUse(KytheConsumer::Kind::Type, type_id, use_pos);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
