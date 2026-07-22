// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_KYTHE_DATA_H_
#define V8_TORQUE_KYTHE_DATA_H_

#include "src/base/contextual.h"
#include "src/torque/ast.h"
#include "src/torque/global-context.h"
#include "src/torque/implementation-visitor.h"

namespace v8 {
namespace internal {
namespace torque {

struct KythePosition {
  std::string file_path;
  uint64_t start_offset;
  uint64_t end_offset;
};

using kythe_entity_t = uint64_t;

class KytheConsumer {
 public:
  enum class Kind {
    Unspecified,
    Constant,
    Function,
    ClassField,
    Variable,
    Type,
  };

  virtual ~KytheConsumer() = 0;

  virtual kythe_entity_t AddDefinition(Kind kind, std::string name,
                                       KythePosition pos) = 0;

  virtual void AddUse(Kind kind, kythe_entity_t entity,
                      KythePosition use_pos) = 0;
  virtual void AddCall(Kind kind, kythe_entity_t caller_entity,
                       KythePosition call_pos,
                       kythe_entity_t callee_entity) = 0;
};
inline KytheConsumer::~KytheConsumer() = default;

class KytheData : public base::ContextualClass<KytheData> {
 public:
  KytheData() = default;

  static void SetConsumer(KytheConsumer* consumer) {
    Get().consumer_ = consumer;
  }

  // Constants
  V8_EXPORT_PRIVATE static kythe_entity_t AddConstantDefinition(
      const Value* constant);
  V8_EXPORT_PRIVATE static void AddConstantUse(SourcePosition use_position,
                                               const Value* constant);
  // Callables
  V8_EXPORT_PRIVATE static kythe_entity_t AddFunctionDefinition(
      Callable* callable);
  V8_EXPORT_PRIVATE static void AddCall(Callable* caller,
                                        SourcePosition call_position,
                                        Callable* callee);
  // Class fields
  V8_EXPORT_PRIVATE static kythe_entity_t AddClassFieldDefinition(
      const Field* field);
  V8_EXPORT_PRIVATE static void AddClassFieldUse(SourcePosition use_position,
                                                 const Field* field);
  // Bindings
  V8_EXPORT_PRIVATE static kythe_entity_t AddBindingDefinition(
      Binding<LocalValue>* binding);
  V8_EXPORT_PRIVATE static kythe_entity_t AddBindingDefinition(
      Binding<LocalLabel>* binding);
  V8_EXPORT_PRIVATE static void AddBindingUse(SourcePosition use_position,
                                              Binding<LocalValue>* binding);
  V8_EXPORT_PRIVATE static void AddBindingUse(SourcePosition use_position,
                                              Binding<LocalLabel>* binding);

  // Types
  V8_EXPORT_PRIVATE static kythe_entity_t AddTypeDefinition(
      const Declarable* type_decl);
  V8_EXPORT_PRIVATE static void AddTypeUse(SourcePosition use_position,
                                           const Declarable* type_decl);

 private:
  static kythe_entity_t AddBindingDefinitionImpl(
      uint64_t binding_index, const std::string& name,
      const SourcePosition& ident_pos);

  KytheConsumer* consumer_;
  std::unordered_map<const Value*, kythe_entity_t> constants_;
  std::unordered_map<Callable*, kythe_entity_t> callables_;

  std::unordered_map<const Field*, std::set<SourcePosition>> field_uses_;
  std::unordered_map<uint64_t, kythe_entity_t> local_bindings_;
  std::unordered_map<const Declarable*, kythe_entity_t> types_;
  std::unordered_map<const Field*, kythe_entity_t> class_fields_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_KYTHE_DATA_H_
