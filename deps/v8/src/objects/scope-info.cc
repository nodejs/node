// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/objects/scope-info.h"

#include "src/ast/scopes.h"
#include "src/ast/variables.h"
#include "src/init/bootstrapper.h"

#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// An entry in ModuleVariableEntries consists of several slots:
enum ModuleVariableEntryOffset {
  kModuleVariableNameOffset,
  kModuleVariableIndexOffset,
  kModuleVariablePropertiesOffset,
  kModuleVariableEntryLength  // Sentinel value.
};

#ifdef DEBUG
bool ScopeInfo::Equals(ScopeInfo other) const {
  if (length() != other.length()) return false;
  for (int index = 0; index < length(); ++index) {
    Object entry = get(index);
    Object other_entry = other.get(index);
    if (entry.IsSmi()) {
      if (entry != other_entry) return false;
    } else {
      if (HeapObject::cast(entry).map().instance_type() !=
          HeapObject::cast(other_entry).map().instance_type()) {
        return false;
      }
      if (entry.IsString()) {
        if (!String::cast(entry).Equals(String::cast(other_entry))) {
          return false;
        }
      } else if (entry.IsScopeInfo()) {
        if (!ScopeInfo::cast(entry).Equals(ScopeInfo::cast(other_entry))) {
          return false;
        }
      } else if (entry.IsModuleInfo()) {
        if (!ModuleInfo::cast(entry).Equals(ModuleInfo::cast(other_entry))) {
          return false;
        }
      } else {
        UNREACHABLE();
      }
    }
  }
  return true;
}
#endif

// static
Handle<ScopeInfo> ScopeInfo::Create(Isolate* isolate, Zone* zone, Scope* scope,
                                    MaybeHandle<ScopeInfo> outer_scope) {
  // Collect variables.
  int context_local_count = 0;
  int module_vars_count = 0;
  // Stack allocated block scope variables are allocated in the parent
  // declaration scope, but are recorded in the block scope's scope info. First
  // slot index indicates at which offset a particular scope starts in the
  // parent declaration scope.
  for (Variable* var : *scope->locals()) {
    switch (var->location()) {
      case VariableLocation::CONTEXT:
        context_local_count++;
        break;
      case VariableLocation::MODULE:
        module_vars_count++;
        break;
      default:
        break;
    }
  }
  // Determine use and location of the "this" binding if it is present.
  VariableAllocationInfo receiver_info;
  if (scope->is_declaration_scope() &&
      scope->AsDeclarationScope()->has_this_declaration()) {
    Variable* var = scope->AsDeclarationScope()->receiver();
    if (!var->is_used()) {
      receiver_info = UNUSED;
    } else if (var->IsContextSlot()) {
      receiver_info = CONTEXT;
      context_local_count++;
    } else {
      DCHECK(var->IsParameter());
      receiver_info = STACK;
    }
  } else {
    receiver_info = NONE;
  }

  DCHECK(module_vars_count == 0 || scope->is_module_scope());

  // Make sure we allocate the correct amount.
  DCHECK_EQ(scope->ContextLocalCount(), context_local_count);

  const bool has_new_target =
      scope->is_declaration_scope() &&
      scope->AsDeclarationScope()->new_target_var() != nullptr;
  // TODO(cbruni): Don't always waste a field for the inferred name.
  const bool has_inferred_function_name = scope->is_function_scope();

  // Determine use and location of the function variable if it is present.
  VariableAllocationInfo function_name_info;
  if (scope->is_function_scope()) {
    if (scope->AsDeclarationScope()->function_var() != nullptr) {
      Variable* var = scope->AsDeclarationScope()->function_var();
      if (!var->is_used()) {
        function_name_info = UNUSED;
      } else if (var->IsContextSlot()) {
        function_name_info = CONTEXT;
      } else {
        DCHECK(var->IsStackLocal());
        function_name_info = STACK;
      }
    } else {
      // Always reserve space for the debug name in the scope info.
      function_name_info = UNUSED;
    }
  } else if (scope->is_module_scope() || scope->is_script_scope() ||
             scope->is_eval_scope()) {
    // Always reserve space for the debug name in the scope info.
    function_name_info = UNUSED;
  } else {
    function_name_info = NONE;
  }

  const bool has_brand = scope->is_class_scope()
                             ? scope->AsClassScope()->brand() != nullptr
                             : false;
  const bool has_function_name = function_name_info != NONE;
  const bool has_position_info = NeedsPositionInfo(scope->scope_type());
  const bool has_receiver = receiver_info == STACK || receiver_info == CONTEXT;
  const int parameter_count =
      scope->is_declaration_scope()
          ? scope->AsDeclarationScope()->num_parameters()
          : 0;
  const bool has_outer_scope_info = !outer_scope.is_null();
  const int length = kVariablePartIndex + 2 * context_local_count +
                     (has_receiver ? 1 : 0) +
                     (has_function_name ? kFunctionNameEntries : 0) +
                     (has_inferred_function_name ? 1 : 0) +
                     (has_position_info ? kPositionInfoEntries : 0) +
                     (has_outer_scope_info ? 1 : 0) +
                     (scope->is_module_scope()
                          ? 2 + kModuleVariableEntryLength * module_vars_count
                          : 0);

  Handle<ScopeInfo> scope_info_handle =
      isolate->factory()->NewScopeInfo(length);
  int index = kVariablePartIndex;
  {
    DisallowHeapAllocation no_gc;
    ScopeInfo scope_info = *scope_info_handle;
    WriteBarrierMode mode = scope_info.GetWriteBarrierMode(no_gc);

    bool has_simple_parameters = false;
    bool is_asm_module = false;
    bool calls_sloppy_eval = false;
    if (scope->is_function_scope()) {
      DeclarationScope* function_scope = scope->AsDeclarationScope();
      has_simple_parameters = function_scope->has_simple_parameters();
      is_asm_module = function_scope->is_asm_module();
    }
    FunctionKind function_kind = kNormalFunction;
    if (scope->is_declaration_scope()) {
      function_kind = scope->AsDeclarationScope()->function_kind();
      calls_sloppy_eval = scope->AsDeclarationScope()->calls_sloppy_eval();
    }

    // Encode the flags.
    int flags =
        ScopeTypeField::encode(scope->scope_type()) |
        CallsSloppyEvalField::encode(calls_sloppy_eval) |
        LanguageModeField::encode(scope->language_mode()) |
        DeclarationScopeField::encode(scope->is_declaration_scope()) |
        ReceiverVariableField::encode(receiver_info) |
        HasClassBrandField::encode(has_brand) |
        HasNewTargetField::encode(has_new_target) |
        FunctionVariableField::encode(function_name_info) |
        HasInferredFunctionNameField::encode(has_inferred_function_name) |
        IsAsmModuleField::encode(is_asm_module) |
        HasSimpleParametersField::encode(has_simple_parameters) |
        FunctionKindField::encode(function_kind) |
        HasOuterScopeInfoField::encode(has_outer_scope_info) |
        IsDebugEvaluateScopeField::encode(scope->is_debug_evaluate_scope()) |
        ForceContextAllocationField::encode(
            scope->ForceContextForLanguageMode());
    scope_info.SetFlags(flags);

    scope_info.SetParameterCount(parameter_count);
    scope_info.SetContextLocalCount(context_local_count);

    // Add context locals' names and info, module variables' names and info.
    // Context locals are added using their index.
    int context_local_base = index;
    int context_local_info_base = context_local_base + context_local_count;
    int module_var_entry = scope_info.ModuleVariablesIndex();

    for (Variable* var : *scope->locals()) {
      switch (var->location()) {
        case VariableLocation::CONTEXT: {
          // Due to duplicate parameters, context locals aren't guaranteed to
          // come in order.
          int local_index = var->index() - Context::MIN_CONTEXT_SLOTS;
          DCHECK_LE(0, local_index);
          DCHECK_LT(local_index, context_local_count);
          uint32_t info =
              VariableModeField::encode(var->mode()) |
              InitFlagField::encode(var->initialization_flag()) |
              MaybeAssignedFlagField::encode(var->maybe_assigned()) |
              ParameterNumberField::encode(ParameterNumberField::kMax);
          scope_info.set(context_local_base + local_index, *var->name(), mode);
          scope_info.set(context_local_info_base + local_index,
                         Smi::FromInt(info));
          break;
        }
        case VariableLocation::MODULE: {
          scope_info.set(module_var_entry + kModuleVariableNameOffset,
                         *var->name(), mode);
          scope_info.set(module_var_entry + kModuleVariableIndexOffset,
                         Smi::FromInt(var->index()));
          uint32_t properties =
              VariableModeField::encode(var->mode()) |
              InitFlagField::encode(var->initialization_flag()) |
              MaybeAssignedFlagField::encode(var->maybe_assigned()) |
              ParameterNumberField::encode(ParameterNumberField::kMax);
          scope_info.set(module_var_entry + kModuleVariablePropertiesOffset,
                         Smi::FromInt(properties));
          module_var_entry += kModuleVariableEntryLength;
          break;
        }
        default:
          break;
      }
    }

    if (scope->is_declaration_scope()) {
      // Mark contexts slots with the parameter number they represent. We walk
      // the list of parameters. That can include duplicate entries if a
      // parameter name is repeated. By walking upwards, we'll automatically
      // mark the context slot with the highest parameter number that uses this
      // variable. That will be the parameter number that is represented by the
      // context slot. All lower parameters will only be available on the stack
      // through the arguments object.
      for (int i = 0; i < parameter_count; i++) {
        Variable* parameter = scope->AsDeclarationScope()->parameter(i);
        if (parameter->location() != VariableLocation::CONTEXT) continue;
        int index = parameter->index() - Context::MIN_CONTEXT_SLOTS;
        int info_index = context_local_info_base + index;
        int info = Smi::ToInt(scope_info.get(info_index));
        info = ParameterNumberField::update(info, i);
        scope_info.set(info_index, Smi::FromInt(info));
      }

      // TODO(verwaest): Remove this unnecessary entry.
      if (scope->AsDeclarationScope()->has_this_declaration()) {
        Variable* var = scope->AsDeclarationScope()->receiver();
        if (var->location() == VariableLocation::CONTEXT) {
          int local_index = var->index() - Context::MIN_CONTEXT_SLOTS;
          uint32_t info =
              VariableModeField::encode(var->mode()) |
              InitFlagField::encode(var->initialization_flag()) |
              MaybeAssignedFlagField::encode(var->maybe_assigned()) |
              ParameterNumberField::encode(ParameterNumberField::kMax);
          scope_info.set(context_local_base + local_index, *var->name(), mode);
          scope_info.set(context_local_info_base + local_index,
                         Smi::FromInt(info));
        }
      }
    }

    index += 2 * context_local_count;

    // If the receiver is allocated, add its index.
    DCHECK_EQ(index, scope_info.ReceiverInfoIndex());
    if (has_receiver) {
      int var_index = scope->AsDeclarationScope()->receiver()->index();
      scope_info.set(index++, Smi::FromInt(var_index));
      // ?? DCHECK(receiver_info != CONTEXT || var_index ==
      // scope_info->ContextLength() - 1);
    }

    // If present, add the function variable name and its index.
    DCHECK_EQ(index, scope_info.FunctionNameInfoIndex());
    if (has_function_name) {
      Variable* var = scope->AsDeclarationScope()->function_var();
      int var_index = -1;
      Object name = Smi::kZero;
      if (var != nullptr) {
        var_index = var->index();
        name = *var->name();
      }
      scope_info.set(index++, name, mode);
      scope_info.set(index++, Smi::FromInt(var_index));
      DCHECK(function_name_info != CONTEXT ||
             var_index == scope_info.ContextLength() - 1);
    }

    DCHECK_EQ(index, scope_info.InferredFunctionNameIndex());
    if (has_inferred_function_name) {
      // The inferred function name is taken from the SFI.
      index++;
    }

    DCHECK_EQ(index, scope_info.PositionInfoIndex());
    if (has_position_info) {
      scope_info.set(index++, Smi::FromInt(scope->start_position()));
      scope_info.set(index++, Smi::FromInt(scope->end_position()));
    }

    // If present, add the outer scope info.
    DCHECK(index == scope_info.OuterScopeInfoIndex());
    if (has_outer_scope_info) {
      scope_info.set(index++, *outer_scope.ToHandleChecked(), mode);
    }
  }

  // Module-specific information (only for module scopes).
  if (scope->is_module_scope()) {
    Handle<ModuleInfo> module_info =
        ModuleInfo::New(isolate, zone, scope->AsModuleScope()->module());
    DCHECK_EQ(index, scope_info_handle->ModuleInfoIndex());
    scope_info_handle->set(index++, *module_info);
    DCHECK_EQ(index, scope_info_handle->ModuleVariableCountIndex());
    scope_info_handle->set(index++, Smi::FromInt(module_vars_count));
    DCHECK_EQ(index, scope_info_handle->ModuleVariablesIndex());
    // The variable entries themselves have already been written above.
    index += kModuleVariableEntryLength * module_vars_count;
  }

  DCHECK_EQ(index, scope_info_handle->length());
  DCHECK_EQ(parameter_count, scope_info_handle->ParameterCount());
  DCHECK_EQ(scope->num_heap_slots(), scope_info_handle->ContextLength());
  return scope_info_handle;
}

// static
Handle<ScopeInfo> ScopeInfo::CreateForWithScope(
    Isolate* isolate, MaybeHandle<ScopeInfo> outer_scope) {
  const bool has_outer_scope_info = !outer_scope.is_null();
  const int length = kVariablePartIndex + (has_outer_scope_info ? 1 : 0);

  Factory* factory = isolate->factory();
  Handle<ScopeInfo> scope_info = factory->NewScopeInfo(length);

  // Encode the flags.
  int flags =
      ScopeTypeField::encode(WITH_SCOPE) | CallsSloppyEvalField::encode(false) |
      LanguageModeField::encode(LanguageMode::kSloppy) |
      DeclarationScopeField::encode(false) |
      ReceiverVariableField::encode(NONE) | HasClassBrandField::encode(false) |
      HasNewTargetField::encode(false) | FunctionVariableField::encode(NONE) |
      IsAsmModuleField::encode(false) | HasSimpleParametersField::encode(true) |
      FunctionKindField::encode(kNormalFunction) |
      HasOuterScopeInfoField::encode(has_outer_scope_info) |
      IsDebugEvaluateScopeField::encode(false);
  scope_info->SetFlags(flags);

  scope_info->SetParameterCount(0);
  scope_info->SetContextLocalCount(0);

  int index = kVariablePartIndex;
  DCHECK_EQ(index, scope_info->ReceiverInfoIndex());
  DCHECK_EQ(index, scope_info->FunctionNameInfoIndex());
  DCHECK_EQ(index, scope_info->InferredFunctionNameIndex());
  DCHECK_EQ(index, scope_info->PositionInfoIndex());
  DCHECK(index == scope_info->OuterScopeInfoIndex());
  if (has_outer_scope_info) {
    scope_info->set(index++, *outer_scope.ToHandleChecked());
  }
  DCHECK_EQ(index, scope_info->length());
  DCHECK_EQ(0, scope_info->ParameterCount());
  DCHECK_EQ(Context::MIN_CONTEXT_SLOTS, scope_info->ContextLength());
  return scope_info;
}

// static
Handle<ScopeInfo> ScopeInfo::CreateGlobalThisBinding(Isolate* isolate) {
  return CreateForBootstrapping(isolate, SCRIPT_SCOPE);
}

// static
Handle<ScopeInfo> ScopeInfo::CreateForEmptyFunction(Isolate* isolate) {
  return CreateForBootstrapping(isolate, FUNCTION_SCOPE);
}

// static
Handle<ScopeInfo> ScopeInfo::CreateForBootstrapping(Isolate* isolate,
                                                    ScopeType type) {
  DCHECK(type == SCRIPT_SCOPE || type == FUNCTION_SCOPE);

  const int parameter_count = 0;
  const bool is_empty_function = type == FUNCTION_SCOPE;
  const int context_local_count = is_empty_function ? 0 : 1;
  const bool has_receiver = !is_empty_function;
  const bool has_inferred_function_name = is_empty_function;
  const bool has_position_info = true;
  const int length = kVariablePartIndex + 2 * context_local_count +
                     (has_receiver ? 1 : 0) +
                     (is_empty_function ? kFunctionNameEntries : 0) +
                     (has_inferred_function_name ? 1 : 0) +
                     (has_position_info ? kPositionInfoEntries : 0);

  Factory* factory = isolate->factory();
  Handle<ScopeInfo> scope_info = factory->NewScopeInfo(length);

  // Encode the flags.
  int flags =
      ScopeTypeField::encode(type) | CallsSloppyEvalField::encode(false) |
      LanguageModeField::encode(LanguageMode::kSloppy) |
      DeclarationScopeField::encode(true) |
      ReceiverVariableField::encode(is_empty_function ? UNUSED : CONTEXT) |
      HasClassBrandField::encode(false) | HasNewTargetField::encode(false) |
      FunctionVariableField::encode(is_empty_function ? UNUSED : NONE) |
      HasInferredFunctionNameField::encode(has_inferred_function_name) |
      IsAsmModuleField::encode(false) | HasSimpleParametersField::encode(true) |
      FunctionKindField::encode(FunctionKind::kNormalFunction) |
      HasOuterScopeInfoField::encode(false) |
      IsDebugEvaluateScopeField::encode(false);
  scope_info->SetFlags(flags);
  scope_info->SetParameterCount(parameter_count);
  scope_info->SetContextLocalCount(context_local_count);

  int index = kVariablePartIndex;

  // Here we add info for context-allocated "this".
  DCHECK_EQ(index, scope_info->ContextLocalNamesIndex());
  if (context_local_count) {
    scope_info->set(index++, ReadOnlyRoots(isolate).this_string());
  }
  DCHECK_EQ(index, scope_info->ContextLocalInfosIndex());
  if (context_local_count) {
    const uint32_t value =
        VariableModeField::encode(VariableMode::kConst) |
        InitFlagField::encode(kCreatedInitialized) |
        MaybeAssignedFlagField::encode(kNotAssigned) |
        ParameterNumberField::encode(ParameterNumberField::kMax);
    scope_info->set(index++, Smi::FromInt(value));
  }

  // And here we record that this scopeinfo binds a receiver.
  DCHECK_EQ(index, scope_info->ReceiverInfoIndex());
  const int receiver_index = Context::MIN_CONTEXT_SLOTS + 0;
  if (!is_empty_function) {
    scope_info->set(index++, Smi::FromInt(receiver_index));
  }

  DCHECK_EQ(index, scope_info->FunctionNameInfoIndex());
  if (is_empty_function) {
    scope_info->set(index++, *isolate->factory()->empty_string());
    scope_info->set(index++, Smi::kZero);
  }
  DCHECK_EQ(index, scope_info->InferredFunctionNameIndex());
  if (has_inferred_function_name) {
    scope_info->set(index++, *isolate->factory()->empty_string());
  }
  DCHECK_EQ(index, scope_info->PositionInfoIndex());
  // Store dummy position to be in sync with the {scope_type}.
  scope_info->set(index++, Smi::kZero);
  scope_info->set(index++, Smi::kZero);
  DCHECK_EQ(index, scope_info->OuterScopeInfoIndex());
  DCHECK_EQ(index, scope_info->length());
  DCHECK_EQ(scope_info->ParameterCount(), parameter_count);
  if (type == FUNCTION_SCOPE) {
    DCHECK_EQ(scope_info->ContextLength(), 0);
  } else {
    DCHECK_EQ(scope_info->ContextLength(), Context::MIN_CONTEXT_SLOTS + 1);
  }

  return scope_info;
}

ScopeInfo ScopeInfo::Empty(Isolate* isolate) {
  return ReadOnlyRoots(isolate).empty_scope_info();
}

ScopeType ScopeInfo::scope_type() const {
  DCHECK_LT(0, length());
  return ScopeTypeField::decode(Flags());
}

bool ScopeInfo::CallsSloppyEval() const {
  bool calls_sloppy_eval =
      length() > 0 && CallsSloppyEvalField::decode(Flags());
  DCHECK_IMPLIES(calls_sloppy_eval, is_sloppy(language_mode()));
  DCHECK_IMPLIES(calls_sloppy_eval, is_declaration_scope());
  return calls_sloppy_eval;
}

LanguageMode ScopeInfo::language_mode() const {
  return length() > 0 ? LanguageModeField::decode(Flags())
                      : LanguageMode::kSloppy;
}

bool ScopeInfo::is_declaration_scope() const {
  return DeclarationScopeField::decode(Flags());
}

int ScopeInfo::ContextLength() const {
  if (length() > 0) {
    int context_locals = ContextLocalCount();
    bool function_name_context_slot =
        FunctionVariableField::decode(Flags()) == CONTEXT;
    bool force_context = ForceContextAllocationField::decode(Flags());
    bool has_context =
        context_locals > 0 || force_context || function_name_context_slot ||
        scope_type() == WITH_SCOPE || scope_type() == CLASS_SCOPE ||
        (scope_type() == BLOCK_SCOPE && CallsSloppyEval() &&
         is_declaration_scope()) ||
        (scope_type() == FUNCTION_SCOPE && CallsSloppyEval()) ||
        (scope_type() == FUNCTION_SCOPE && IsAsmModule()) ||
        scope_type() == MODULE_SCOPE;

    if (has_context) {
      return Context::MIN_CONTEXT_SLOTS + context_locals +
             (function_name_context_slot ? 1 : 0);
    }
  }
  return 0;
}

bool ScopeInfo::HasReceiver() const {
  if (length() == 0) return false;
  return NONE != ReceiverVariableField::decode(Flags());
}

bool ScopeInfo::HasAllocatedReceiver() const {
  if (length() == 0) return false;
  VariableAllocationInfo allocation = ReceiverVariableField::decode(Flags());
  return allocation == STACK || allocation == CONTEXT;
}

bool ScopeInfo::HasClassBrand() const {
  return HasClassBrandField::decode(Flags());
}

bool ScopeInfo::HasNewTarget() const {
  return HasNewTargetField::decode(Flags());
}

bool ScopeInfo::HasFunctionName() const {
  if (length() == 0) return false;
  return NONE != FunctionVariableField::decode(Flags());
}

bool ScopeInfo::HasInferredFunctionName() const {
  if (length() == 0) return false;
  return HasInferredFunctionNameField::decode(Flags());
}

bool ScopeInfo::HasPositionInfo() const {
  if (length() == 0) return false;
  return NeedsPositionInfo(scope_type());
}

// static
bool ScopeInfo::NeedsPositionInfo(ScopeType type) {
  return type == FUNCTION_SCOPE || type == SCRIPT_SCOPE || type == EVAL_SCOPE ||
         type == MODULE_SCOPE;
}

bool ScopeInfo::HasSharedFunctionName() const {
  return FunctionName() != SharedFunctionInfo::kNoSharedNameSentinel;
}

void ScopeInfo::SetFunctionName(Object name) {
  DCHECK(HasFunctionName());
  DCHECK(name.IsString() || name == SharedFunctionInfo::kNoSharedNameSentinel);
  set(FunctionNameInfoIndex(), name);
}

void ScopeInfo::SetInferredFunctionName(String name) {
  DCHECK(HasInferredFunctionName());
  set(InferredFunctionNameIndex(), name);
}

bool ScopeInfo::HasOuterScopeInfo() const {
  if (length() == 0) return false;
  return HasOuterScopeInfoField::decode(Flags());
}

bool ScopeInfo::IsDebugEvaluateScope() const {
  if (length() == 0) return false;
  return IsDebugEvaluateScopeField::decode(Flags());
}

void ScopeInfo::SetIsDebugEvaluateScope() {
  if (length() > 0) {
    DCHECK_EQ(scope_type(), WITH_SCOPE);
    SetFlags(Flags() | IsDebugEvaluateScopeField::encode(true));
  } else {
    UNREACHABLE();
  }
}

bool ScopeInfo::HasContext() const { return ContextLength() > 0; }

Object ScopeInfo::FunctionName() const {
  DCHECK(HasFunctionName());
  return get(FunctionNameInfoIndex());
}

Object ScopeInfo::InferredFunctionName() const {
  DCHECK(HasInferredFunctionName());
  return get(InferredFunctionNameIndex());
}

String ScopeInfo::FunctionDebugName() const {
  Object name = FunctionName();
  if (name.IsString() && String::cast(name).length() > 0) {
    return String::cast(name);
  }
  if (HasInferredFunctionName()) {
    name = InferredFunctionName();
    if (name.IsString()) return String::cast(name);
  }
  return GetReadOnlyRoots().empty_string();
}

int ScopeInfo::StartPosition() const {
  DCHECK(HasPositionInfo());
  return Smi::ToInt(get(PositionInfoIndex()));
}

int ScopeInfo::EndPosition() const {
  DCHECK(HasPositionInfo());
  return Smi::ToInt(get(PositionInfoIndex() + 1));
}

void ScopeInfo::SetPositionInfo(int start, int end) {
  DCHECK(HasPositionInfo());
  DCHECK_LE(start, end);
  set(PositionInfoIndex(), Smi::FromInt(start));
  set(PositionInfoIndex() + 1, Smi::FromInt(end));
}

ScopeInfo ScopeInfo::OuterScopeInfo() const {
  DCHECK(HasOuterScopeInfo());
  return ScopeInfo::cast(get(OuterScopeInfoIndex()));
}

ModuleInfo ScopeInfo::ModuleDescriptorInfo() const {
  DCHECK(scope_type() == MODULE_SCOPE);
  return ModuleInfo::cast(get(ModuleInfoIndex()));
}

String ScopeInfo::ContextLocalName(int var) const {
  DCHECK_LE(0, var);
  DCHECK_LT(var, ContextLocalCount());
  int info_index = ContextLocalNamesIndex() + var;
  return String::cast(get(info_index));
}

VariableMode ScopeInfo::ContextLocalMode(int var) const {
  DCHECK_LE(0, var);
  DCHECK_LT(var, ContextLocalCount());
  int info_index = ContextLocalInfosIndex() + var;
  int value = Smi::ToInt(get(info_index));
  return VariableModeField::decode(value);
}

InitializationFlag ScopeInfo::ContextLocalInitFlag(int var) const {
  DCHECK_LE(0, var);
  DCHECK_LT(var, ContextLocalCount());
  int info_index = ContextLocalInfosIndex() + var;
  int value = Smi::ToInt(get(info_index));
  return InitFlagField::decode(value);
}

bool ScopeInfo::ContextLocalIsParameter(int var) const {
  DCHECK_LE(0, var);
  DCHECK_LT(var, ContextLocalCount());
  int info_index = ContextLocalInfosIndex() + var;
  int value = Smi::ToInt(get(info_index));
  return ParameterNumberField::decode(value) != ParameterNumberField::kMax;
}

uint32_t ScopeInfo::ContextLocalParameterNumber(int var) const {
  DCHECK(ContextLocalIsParameter(var));
  int info_index = ContextLocalInfosIndex() + var;
  int value = Smi::ToInt(get(info_index));
  return ParameterNumberField::decode(value);
}

MaybeAssignedFlag ScopeInfo::ContextLocalMaybeAssignedFlag(int var) const {
  DCHECK_LE(0, var);
  DCHECK_LT(var, ContextLocalCount());
  int info_index = ContextLocalInfosIndex() + var;
  int value = Smi::ToInt(get(info_index));
  return MaybeAssignedFlagField::decode(value);
}

// static
bool ScopeInfo::VariableIsSynthetic(String name) {
  // There's currently no flag stored on the ScopeInfo to indicate that a
  // variable is a compiler-introduced temporary. However, to avoid conflict
  // with user declarations, the current temporaries like .generator_object and
  // .result start with a dot, so we can use that as a flag. It's a hack!
  return name.length() == 0 || name.Get(0) == '.' ||
         name.Equals(name.GetReadOnlyRoots().this_string());
}

int ScopeInfo::ModuleIndex(String name, VariableMode* mode,
                           InitializationFlag* init_flag,
                           MaybeAssignedFlag* maybe_assigned_flag) {
  DisallowHeapAllocation no_gc;
  DCHECK(name.IsInternalizedString());
  DCHECK_EQ(scope_type(), MODULE_SCOPE);
  DCHECK_NOT_NULL(mode);
  DCHECK_NOT_NULL(init_flag);
  DCHECK_NOT_NULL(maybe_assigned_flag);

  int module_vars_count = Smi::ToInt(get(ModuleVariableCountIndex()));
  int entry = ModuleVariablesIndex();
  for (int i = 0; i < module_vars_count; ++i) {
    String var_name = String::cast(get(entry + kModuleVariableNameOffset));
    if (name.Equals(var_name)) {
      int index;
      ModuleVariable(i, nullptr, &index, mode, init_flag, maybe_assigned_flag);
      return index;
    }
    entry += kModuleVariableEntryLength;
  }

  return 0;
}

// static
int ScopeInfo::ContextSlotIndex(ScopeInfo scope_info, String name,
                                VariableMode* mode,
                                InitializationFlag* init_flag,
                                MaybeAssignedFlag* maybe_assigned_flag) {
  DisallowHeapAllocation no_gc;
  DCHECK(name.IsInternalizedString());
  DCHECK_NOT_NULL(mode);
  DCHECK_NOT_NULL(init_flag);
  DCHECK_NOT_NULL(maybe_assigned_flag);

  if (scope_info.length() == 0) return -1;

  int start = scope_info.ContextLocalNamesIndex();
  int end = start + scope_info.ContextLocalCount();
  for (int i = start; i < end; ++i) {
    if (name != scope_info.get(i)) continue;
    int var = i - start;
    *mode = scope_info.ContextLocalMode(var);
    *init_flag = scope_info.ContextLocalInitFlag(var);
    *maybe_assigned_flag = scope_info.ContextLocalMaybeAssignedFlag(var);
    int result = Context::MIN_CONTEXT_SLOTS + var;

    DCHECK_LT(result, scope_info.ContextLength());
    return result;
  }

  return -1;
}

int ScopeInfo::ReceiverContextSlotIndex() const {
  if (length() > 0 && ReceiverVariableField::decode(Flags()) == CONTEXT) {
    return Smi::ToInt(get(ReceiverInfoIndex()));
  }
  return -1;
}

int ScopeInfo::FunctionContextSlotIndex(String name) const {
  DCHECK(name.IsInternalizedString());
  if (length() > 0) {
    if (FunctionVariableField::decode(Flags()) == CONTEXT &&
        FunctionName() == name) {
      return Smi::ToInt(get(FunctionNameInfoIndex() + 1));
    }
  }
  return -1;
}

FunctionKind ScopeInfo::function_kind() const {
  return FunctionKindField::decode(Flags());
}

int ScopeInfo::ContextLocalNamesIndex() const {
  DCHECK_LT(0, length());
  return kVariablePartIndex;
}

int ScopeInfo::ContextLocalInfosIndex() const {
  return ContextLocalNamesIndex() + ContextLocalCount();
}

int ScopeInfo::ReceiverInfoIndex() const {
  return ContextLocalInfosIndex() + ContextLocalCount();
}

int ScopeInfo::FunctionNameInfoIndex() const {
  return ReceiverInfoIndex() + (HasAllocatedReceiver() ? 1 : 0);
}

int ScopeInfo::InferredFunctionNameIndex() const {
  return FunctionNameInfoIndex() +
         (HasFunctionName() ? kFunctionNameEntries : 0);
}

int ScopeInfo::PositionInfoIndex() const {
  return InferredFunctionNameIndex() + (HasInferredFunctionName() ? 1 : 0);
}

int ScopeInfo::OuterScopeInfoIndex() const {
  return PositionInfoIndex() + (HasPositionInfo() ? kPositionInfoEntries : 0);
}

int ScopeInfo::ModuleInfoIndex() const {
  return OuterScopeInfoIndex() + (HasOuterScopeInfo() ? 1 : 0);
}

int ScopeInfo::ModuleVariableCountIndex() const {
  return ModuleInfoIndex() + 1;
}

int ScopeInfo::ModuleVariablesIndex() const {
  return ModuleVariableCountIndex() + 1;
}

void ScopeInfo::ModuleVariable(int i, String* name, int* index,
                               VariableMode* mode,
                               InitializationFlag* init_flag,
                               MaybeAssignedFlag* maybe_assigned_flag) {
  DCHECK_LE(0, i);
  DCHECK_LT(i, Smi::ToInt(get(ModuleVariableCountIndex())));

  int entry = ModuleVariablesIndex() + i * kModuleVariableEntryLength;
  int properties = Smi::ToInt(get(entry + kModuleVariablePropertiesOffset));

  if (name != nullptr) {
    *name = String::cast(get(entry + kModuleVariableNameOffset));
  }
  if (index != nullptr) {
    *index = Smi::ToInt(get(entry + kModuleVariableIndexOffset));
    DCHECK_NE(*index, 0);
  }
  if (mode != nullptr) {
    *mode = VariableModeField::decode(properties);
  }
  if (init_flag != nullptr) {
    *init_flag = InitFlagField::decode(properties);
  }
  if (maybe_assigned_flag != nullptr) {
    *maybe_assigned_flag = MaybeAssignedFlagField::decode(properties);
  }
}

std::ostream& operator<<(std::ostream& os,
                         ScopeInfo::VariableAllocationInfo var_info) {
  switch (var_info) {
    case ScopeInfo::VariableAllocationInfo::NONE:
      return os << "NONE";
    case ScopeInfo::VariableAllocationInfo::STACK:
      return os << "STACK";
    case ScopeInfo::VariableAllocationInfo::CONTEXT:
      return os << "CONTEXT";
    case ScopeInfo::VariableAllocationInfo::UNUSED:
      return os << "UNUSED";
  }
  UNREACHABLE();
  return os;
}

Handle<ModuleInfoEntry> ModuleInfoEntry::New(Isolate* isolate,
                                             Handle<Object> export_name,
                                             Handle<Object> local_name,
                                             Handle<Object> import_name,
                                             int module_request, int cell_index,
                                             int beg_pos, int end_pos) {
  Handle<ModuleInfoEntry> result =
      Handle<ModuleInfoEntry>::cast(isolate->factory()->NewStruct(
          MODULE_INFO_ENTRY_TYPE, AllocationType::kOld));
  result->set_export_name(*export_name);
  result->set_local_name(*local_name);
  result->set_import_name(*import_name);
  result->set_module_request(module_request);
  result->set_cell_index(cell_index);
  result->set_beg_pos(beg_pos);
  result->set_end_pos(end_pos);
  return result;
}

Handle<ModuleInfo> ModuleInfo::New(Isolate* isolate, Zone* zone,
                                   ModuleDescriptor* descr) {
  // Serialize module requests.
  int size = static_cast<int>(descr->module_requests().size());
  Handle<FixedArray> module_requests = isolate->factory()->NewFixedArray(size);
  Handle<FixedArray> module_request_positions =
      isolate->factory()->NewFixedArray(size);
  for (const auto& elem : descr->module_requests()) {
    module_requests->set(elem.second.index, *elem.first->string());
    module_request_positions->set(elem.second.index,
                                  Smi::FromInt(elem.second.position));
  }

  // Serialize special exports.
  Handle<FixedArray> special_exports = isolate->factory()->NewFixedArray(
      static_cast<int>(descr->special_exports().size()));
  {
    int i = 0;
    for (auto entry : descr->special_exports()) {
      Handle<ModuleInfoEntry> serialized_entry = entry->Serialize(isolate);
      special_exports->set(i++, *serialized_entry);
    }
  }

  // Serialize namespace imports.
  Handle<FixedArray> namespace_imports = isolate->factory()->NewFixedArray(
      static_cast<int>(descr->namespace_imports().size()));
  {
    int i = 0;
    for (auto entry : descr->namespace_imports()) {
      Handle<ModuleInfoEntry> serialized_entry = entry->Serialize(isolate);
      namespace_imports->set(i++, *serialized_entry);
    }
  }

  // Serialize regular exports.
  Handle<FixedArray> regular_exports =
      descr->SerializeRegularExports(isolate, zone);

  // Serialize regular imports.
  Handle<FixedArray> regular_imports = isolate->factory()->NewFixedArray(
      static_cast<int>(descr->regular_imports().size()));
  {
    int i = 0;
    for (const auto& elem : descr->regular_imports()) {
      Handle<ModuleInfoEntry> serialized_entry =
          elem.second->Serialize(isolate);
      regular_imports->set(i++, *serialized_entry);
    }
  }

  Handle<ModuleInfo> result = isolate->factory()->NewModuleInfo();
  result->set(kModuleRequestsIndex, *module_requests);
  result->set(kSpecialExportsIndex, *special_exports);
  result->set(kRegularExportsIndex, *regular_exports);
  result->set(kNamespaceImportsIndex, *namespace_imports);
  result->set(kRegularImportsIndex, *regular_imports);
  result->set(kModuleRequestPositionsIndex, *module_request_positions);
  return result;
}

int ModuleInfo::RegularExportCount() const {
  DCHECK_EQ(regular_exports().length() % kRegularExportLength, 0);
  return regular_exports().length() / kRegularExportLength;
}

String ModuleInfo::RegularExportLocalName(int i) const {
  return String::cast(regular_exports().get(i * kRegularExportLength +
                                            kRegularExportLocalNameOffset));
}

int ModuleInfo::RegularExportCellIndex(int i) const {
  return Smi::ToInt(regular_exports().get(i * kRegularExportLength +
                                          kRegularExportCellIndexOffset));
}

FixedArray ModuleInfo::RegularExportExportNames(int i) const {
  return FixedArray::cast(regular_exports().get(
      i * kRegularExportLength + kRegularExportExportNamesOffset));
}

}  // namespace internal
}  // namespace v8
