// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_COMPILATION_UNIT_H_
#define V8_MAGLEV_MAGLEV_COMPILATION_UNIT_H_

#include <limits>
#include <optional>

#include "src/common/globals.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/heap-refs.h"

namespace v8 {
namespace internal {
namespace maglev {

enum class ValueRepresentation : uint8_t;
class MaglevCompilationInfo;
class MaglevGraphLabeller;
class Node;
class ValueNode;

// Bundles a static ScopeInfoRef with the distance (in Context::previous() hops)
// from this scope to MaglevCompilationUnit::specialization_context().
// - If specialization_context_distance() has a value, walking outward that many
//   steps reaches the unit's specialization context.
// - If specialization_context_distance() is std::nullopt (stored compactly as
//   kNoDistance to keep sizeof(ContextScopeInfo) == 16), either the unit has no
//   specialization context, or we have walked outward past the specialization
//   context.
class ContextScopeInfo {
 public:
  ContextScopeInfo() = default;
  ContextScopeInfo(std::nullopt_t) {}  // NOLINT(runtime/explicit)
  ContextScopeInfo(compiler::OptionalScopeInfoRef scope_info,
                   std::optional<size_t> specialization_context_distance =
                       std::nullopt)  // NOLINT(runtime/explicit)
      : scope_info_(scope_info),
        specialization_context_distance_(
            scope_info.has_value()
                ? specialization_context_distance.value_or(kNoDistance)
                : kNoDistance) {}

  bool has_value() const { return scope_info_.has_value(); }
  compiler::ScopeInfoRef value() const { return scope_info_.value(); }
  operator compiler::ScopeInfoRef() const { return value(); }
  compiler::OptionalScopeInfoRef scope_info() const { return scope_info_; }

  bool HasOuterScopeInfo() const { return scope_info_->HasOuterScopeInfo(); }
  bool HasContextExtensionSlot() const {
    return scope_info_->HasContextExtensionSlot();
  }
  ScopeType scope_type() const { return scope_info_->scope_type(); }

  std::optional<size_t> specialization_context_distance() const {
    if (!has_value() || specialization_context_distance_ == kNoDistance) {
      return std::nullopt;
    }
    return specialization_context_distance_;
  }

  V8_NODISCARD ContextScopeInfo Push(compiler::ScopeInfoRef inner_scope) const {
    std::optional<size_t> inner_dist;
    if (specialization_context_distance_ != kNoDistance) {
      inner_dist = specialization_context_distance_ + 1;
    }
    return ContextScopeInfo(inner_scope, inner_dist);
  }

  V8_NODISCARD ContextScopeInfo
  OuterScopeInfo(compiler::JSHeapBroker* broker) const {
    DCHECK(has_value());
    DCHECK(HasOuterScopeInfo());
    std::optional<size_t> outer_dist;
    if (specialization_context_distance_ != kNoDistance) {
      DCHECK_GT(specialization_context_distance_, 0);
      outer_dist = specialization_context_distance_ - 1;
    }
    return ContextScopeInfo(scope_info_->OuterScopeInfo(broker), outer_dist);
  }

  bool operator==(const ContextScopeInfo& other) const {
    DCHECK_IMPLIES(scope_info_.equals(other.scope_info_),
                   specialization_context_distance_ ==
                       other.specialization_context_distance_);
    return scope_info_.equals(other.scope_info_);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const ContextScopeInfo& info) {
    if (info.has_value()) {
      return os << info.value();
    }
    return os << "<empty>";
  }

 private:
  static constexpr size_t kNoDistance = std::numeric_limits<size_t>::max();

  compiler::OptionalScopeInfoRef scope_info_;
  size_t specialization_context_distance_ = kNoDistance;
};

// Per-unit data, i.e. once per top-level function and once per inlined
// function.
class MaglevCompilationUnit : public ZoneObject {
 public:
  static MaglevCompilationUnit* New(Zone* zone, MaglevCompilationInfo* info,
                                    Handle<JSFunction> function) {
    return zone->New<MaglevCompilationUnit>(info, function);
  }
  static MaglevCompilationUnit* NewInner(
      Zone* zone, const MaglevCompilationUnit* caller,
      compiler::SharedFunctionInfoRef shared_function_info,
      compiler::FeedbackCellRef feedback_cell, ValueNode* context,
      ValueNode* function) {
    return zone->New<MaglevCompilationUnit>(caller->info(), caller,
                                            shared_function_info, feedback_cell,
                                            context, function);
  }
  static MaglevCompilationUnit* NewDummy(Zone* zone,
                                         const MaglevCompilationUnit* caller,
                                         int register_count,
                                         uint16_t parameter_count,
                                         uint16_t max_arguments) {
    return zone->New<MaglevCompilationUnit>(
        caller->info(), caller, register_count, parameter_count, max_arguments);
  }

  MaglevCompilationUnit(MaglevCompilationInfo* info,
                        DirectHandle<JSFunction> function);

  MaglevCompilationUnit(MaglevCompilationInfo* info,
                        const MaglevCompilationUnit* caller,
                        compiler::SharedFunctionInfoRef shared_function_info,
                        compiler::FeedbackCellRef feedback_cell,
                        ValueNode* context = nullptr,
                        ValueNode* function = nullptr);

  MaglevCompilationUnit(MaglevCompilationInfo* info,
                        const MaglevCompilationUnit* caller, int register_count,
                        uint16_t parameter_count, uint16_t max_arguments);

  MaglevCompilationInfo* info() const { return info_; }
  const MaglevCompilationUnit* caller() const { return caller_; }
  compiler::JSHeapBroker* broker() const;
  LocalIsolate* local_isolate() const;
  Zone* zone() const;
  int register_count() const { return register_count_; }
  uint16_t parameter_count() const { return parameter_count_; }
  uint16_t max_arguments() const { return max_arguments_; }
  bool is_osr() const;
  BytecodeOffset osr_offset() const;
  int inlining_depth() const { return inlining_depth_; }
  bool is_inline() const { return inlining_depth_ != 0; }
  bool has_graph_labeller() const;
  bool is_tracing_enabled() const;
  MaglevGraphLabeller* graph_labeller() const;
  compiler::SharedFunctionInfoRef shared_function_info() const {
    return shared_function_info_.value();
  }
  compiler::BytecodeArrayRef bytecode() const { return bytecode_.value(); }
  compiler::FeedbackCellRef feedback_cell() const {
    return feedback_cell_.value();
  }
  compiler::FeedbackVectorRef feedback() const {
    return feedback_cell().feedback_vector((broker())).value();
  }
  compiler::OptionalContextRef specialization_context() const {
    DCHECK(specialization_context_initialized_);
    return specialization_context_;
  }
  ContextScopeInfo incoming_context_scope_info() const {
    DCHECK(specialization_context_initialized_);
    return incoming_context_scope_info_;
  }
  void InitializeSpecializationContextForTopLevel();

  void RegisterNodeInGraphLabeller(const Node* node);
  const MaglevCompilationUnit* GetTopLevelCompilationUnit() const;

 private:
  void InitializeSpecializationContextForInlined(ValueNode* context,
                                                 ValueNode* function);
  void InitializeSpecializationContextFromKnownContext(
      compiler::ContextRef context, compiler::ScopeInfoRef scope_info,
      bool has_incoming_context_scope, bool specialize_to_function_context);

  MaglevCompilationInfo* const info_;
  const MaglevCompilationUnit* const caller_;
  const compiler::OptionalSharedFunctionInfoRef shared_function_info_;
  const compiler::OptionalBytecodeArrayRef bytecode_;
  const compiler::OptionalFeedbackCellRef feedback_cell_;
  compiler::OptionalContextRef specialization_context_;
  ContextScopeInfo incoming_context_scope_info_;
#ifdef DEBUG
  bool specialization_context_initialized_ = false;
#endif
  const int register_count_;
  const uint16_t parameter_count_;
  const uint16_t max_arguments_;
  const int inlining_depth_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_COMPILATION_UNIT_H_
