// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"

#include "scopeinfo.h"
#include "scopes.h"

#include "allocation-inl.h"

namespace v8 {
namespace internal {


Handle<ScopeInfo> ScopeInfo::Create(Scope* scope, Zone* zone) {
  // Collect stack and context locals.
  ZoneList<Variable*> stack_locals(scope->StackLocalCount(), zone);
  ZoneList<Variable*> context_locals(scope->ContextLocalCount(), zone);
  scope->CollectStackAndContextLocals(&stack_locals, &context_locals);
  const int stack_local_count = stack_locals.length();
  const int context_local_count = context_locals.length();
  // Make sure we allocate the correct amount.
  ASSERT(scope->StackLocalCount() == stack_local_count);
  ASSERT(scope->ContextLocalCount() == context_local_count);

  // Determine use and location of the function variable if it is present.
  FunctionVariableInfo function_name_info;
  VariableMode function_variable_mode;
  if (scope->is_function_scope() && scope->function() != NULL) {
    Variable* var = scope->function()->proxy()->var();
    if (!var->is_used()) {
      function_name_info = UNUSED;
    } else if (var->IsContextSlot()) {
      function_name_info = CONTEXT;
    } else {
      ASSERT(var->IsStackLocal());
      function_name_info = STACK;
    }
    function_variable_mode = var->mode();
  } else {
    function_name_info = NONE;
    function_variable_mode = VAR;
  }

  const bool has_function_name = function_name_info != NONE;
  const int parameter_count = scope->num_parameters();
  const int length = kVariablePartIndex
      + parameter_count + stack_local_count + 2 * context_local_count
      + (has_function_name ? 2 : 0);

  Handle<ScopeInfo> scope_info = FACTORY->NewScopeInfo(length);

  // Encode the flags.
  int flags = TypeField::encode(scope->type()) |
      CallsEvalField::encode(scope->calls_eval()) |
      LanguageModeField::encode(scope->language_mode()) |
      FunctionVariableField::encode(function_name_info) |
      FunctionVariableMode::encode(function_variable_mode);
  scope_info->SetFlags(flags);
  scope_info->SetParameterCount(parameter_count);
  scope_info->SetStackLocalCount(stack_local_count);
  scope_info->SetContextLocalCount(context_local_count);

  int index = kVariablePartIndex;
  // Add parameters.
  ASSERT(index == scope_info->ParameterEntriesIndex());
  for (int i = 0; i < parameter_count; ++i) {
    scope_info->set(index++, *scope->parameter(i)->name());
  }

  // Add stack locals' names. We are assuming that the stack locals'
  // slots are allocated in increasing order, so we can simply add
  // them to the ScopeInfo object.
  ASSERT(index == scope_info->StackLocalEntriesIndex());
  for (int i = 0; i < stack_local_count; ++i) {
    ASSERT(stack_locals[i]->index() == i);
    scope_info->set(index++, *stack_locals[i]->name());
  }

  // Due to usage analysis, context-allocated locals are not necessarily in
  // increasing order: Some of them may be parameters which are allocated before
  // the non-parameter locals. When the non-parameter locals are sorted
  // according to usage, the allocated slot indices may not be in increasing
  // order with the variable list anymore. Thus, we first need to sort them by
  // context slot index before adding them to the ScopeInfo object.
  context_locals.Sort(&Variable::CompareIndex);

  // Add context locals' names.
  ASSERT(index == scope_info->ContextLocalNameEntriesIndex());
  for (int i = 0; i < context_local_count; ++i) {
    scope_info->set(index++, *context_locals[i]->name());
  }

  // Add context locals' info.
  ASSERT(index == scope_info->ContextLocalInfoEntriesIndex());
  for (int i = 0; i < context_local_count; ++i) {
    Variable* var = context_locals[i];
    uint32_t value = ContextLocalMode::encode(var->mode()) |
        ContextLocalInitFlag::encode(var->initialization_flag());
    scope_info->set(index++, Smi::FromInt(value));
  }

  // If present, add the function variable name and its index.
  ASSERT(index == scope_info->FunctionNameEntryIndex());
  if (has_function_name) {
    int var_index = scope->function()->proxy()->var()->index();
    scope_info->set(index++, *scope->function()->proxy()->name());
    scope_info->set(index++, Smi::FromInt(var_index));
    ASSERT(function_name_info != STACK ||
           (var_index == scope_info->StackLocalCount() &&
            var_index == scope_info->StackSlotCount() - 1));
    ASSERT(function_name_info != CONTEXT ||
           var_index == scope_info->ContextLength() - 1);
  }

  ASSERT(index == scope_info->length());
  ASSERT(scope->num_parameters() == scope_info->ParameterCount());
  ASSERT(scope->num_stack_slots() == scope_info->StackSlotCount());
  ASSERT(scope->num_heap_slots() == scope_info->ContextLength() ||
         (scope->num_heap_slots() == kVariablePartIndex &&
          scope_info->ContextLength() == 0));
  return scope_info;
}


ScopeInfo* ScopeInfo::Empty(Isolate* isolate) {
  return reinterpret_cast<ScopeInfo*>(isolate->heap()->empty_fixed_array());
}


ScopeType ScopeInfo::Type() {
  ASSERT(length() > 0);
  return TypeField::decode(Flags());
}


bool ScopeInfo::CallsEval() {
  return length() > 0 && CallsEvalField::decode(Flags());
}


LanguageMode ScopeInfo::language_mode() {
  return length() > 0 ? LanguageModeField::decode(Flags()) : CLASSIC_MODE;
}


int ScopeInfo::LocalCount() {
  return StackLocalCount() + ContextLocalCount();
}


int ScopeInfo::StackSlotCount() {
  if (length() > 0) {
    bool function_name_stack_slot =
        FunctionVariableField::decode(Flags()) == STACK;
    return StackLocalCount() + (function_name_stack_slot ? 1 : 0);
  }
  return 0;
}


int ScopeInfo::ContextLength() {
  if (length() > 0) {
    int context_locals = ContextLocalCount();
    bool function_name_context_slot =
        FunctionVariableField::decode(Flags()) == CONTEXT;
    bool has_context = context_locals > 0 ||
        function_name_context_slot ||
        Type() == WITH_SCOPE ||
        (Type() == FUNCTION_SCOPE && CallsEval()) ||
        Type() == MODULE_SCOPE;
    if (has_context) {
      return Context::MIN_CONTEXT_SLOTS + context_locals +
          (function_name_context_slot ? 1 : 0);
    }
  }
  return 0;
}


bool ScopeInfo::HasFunctionName() {
  if (length() > 0) {
    return NONE != FunctionVariableField::decode(Flags());
  } else {
    return false;
  }
}


bool ScopeInfo::HasHeapAllocatedLocals() {
  if (length() > 0) {
    return ContextLocalCount() > 0;
  } else {
    return false;
  }
}


bool ScopeInfo::HasContext() {
  return ContextLength() > 0;
}


String* ScopeInfo::FunctionName() {
  ASSERT(HasFunctionName());
  return String::cast(get(FunctionNameEntryIndex()));
}


String* ScopeInfo::ParameterName(int var) {
  ASSERT(0 <= var && var < ParameterCount());
  int info_index = ParameterEntriesIndex() + var;
  return String::cast(get(info_index));
}


String* ScopeInfo::LocalName(int var) {
  ASSERT(0 <= var && var < LocalCount());
  ASSERT(StackLocalEntriesIndex() + StackLocalCount() ==
         ContextLocalNameEntriesIndex());
  int info_index = StackLocalEntriesIndex() + var;
  return String::cast(get(info_index));
}


String* ScopeInfo::StackLocalName(int var) {
  ASSERT(0 <= var && var < StackLocalCount());
  int info_index = StackLocalEntriesIndex() + var;
  return String::cast(get(info_index));
}


String* ScopeInfo::ContextLocalName(int var) {
  ASSERT(0 <= var && var < ContextLocalCount());
  int info_index = ContextLocalNameEntriesIndex() + var;
  return String::cast(get(info_index));
}


VariableMode ScopeInfo::ContextLocalMode(int var) {
  ASSERT(0 <= var && var < ContextLocalCount());
  int info_index = ContextLocalInfoEntriesIndex() + var;
  int value = Smi::cast(get(info_index))->value();
  return ContextLocalMode::decode(value);
}


InitializationFlag ScopeInfo::ContextLocalInitFlag(int var) {
  ASSERT(0 <= var && var < ContextLocalCount());
  int info_index = ContextLocalInfoEntriesIndex() + var;
  int value = Smi::cast(get(info_index))->value();
  return ContextLocalInitFlag::decode(value);
}


int ScopeInfo::StackSlotIndex(String* name) {
  ASSERT(name->IsInternalizedString());
  if (length() > 0) {
    int start = StackLocalEntriesIndex();
    int end = StackLocalEntriesIndex() + StackLocalCount();
    for (int i = start; i < end; ++i) {
      if (name == get(i)) {
        return i - start;
      }
    }
  }
  return -1;
}


int ScopeInfo::ContextSlotIndex(String* name,
                                VariableMode* mode,
                                InitializationFlag* init_flag) {
  ASSERT(name->IsInternalizedString());
  ASSERT(mode != NULL);
  ASSERT(init_flag != NULL);
  if (length() > 0) {
    ContextSlotCache* context_slot_cache = GetIsolate()->context_slot_cache();
    int result = context_slot_cache->Lookup(this, name, mode, init_flag);
    if (result != ContextSlotCache::kNotFound) {
      ASSERT(result < ContextLength());
      return result;
    }

    int start = ContextLocalNameEntriesIndex();
    int end = ContextLocalNameEntriesIndex() + ContextLocalCount();
    for (int i = start; i < end; ++i) {
      if (name == get(i)) {
        int var = i - start;
        *mode = ContextLocalMode(var);
        *init_flag = ContextLocalInitFlag(var);
        result = Context::MIN_CONTEXT_SLOTS + var;
        context_slot_cache->Update(this, name, *mode, *init_flag, result);
        ASSERT(result < ContextLength());
        return result;
      }
    }
    // Cache as not found. Mode and init flag don't matter.
    context_slot_cache->Update(this, name, INTERNAL, kNeedsInitialization, -1);
  }
  return -1;
}


int ScopeInfo::ParameterIndex(String* name) {
  ASSERT(name->IsInternalizedString());
  if (length() > 0) {
    // We must read parameters from the end since for
    // multiply declared parameters the value of the
    // last declaration of that parameter is used
    // inside a function (and thus we need to look
    // at the last index). Was bug# 1110337.
    int start = ParameterEntriesIndex();
    int end = ParameterEntriesIndex() + ParameterCount();
    for (int i = end - 1; i >= start; --i) {
      if (name == get(i)) {
        return i - start;
      }
    }
  }
  return -1;
}


int ScopeInfo::FunctionContextSlotIndex(String* name, VariableMode* mode) {
  ASSERT(name->IsInternalizedString());
  ASSERT(mode != NULL);
  if (length() > 0) {
    if (FunctionVariableField::decode(Flags()) == CONTEXT &&
        FunctionName() == name) {
      *mode = FunctionVariableMode::decode(Flags());
      return Smi::cast(get(FunctionNameEntryIndex() + 1))->value();
    }
  }
  return -1;
}


bool ScopeInfo::CopyContextLocalsToScopeObject(
    Isolate* isolate,
    Handle<Context> context,
    Handle<JSObject> scope_object) {
  int local_count = ContextLocalCount();
  if (local_count == 0) return true;
  // Fill all context locals to the context extension.
  int start = ContextLocalNameEntriesIndex();
  int end = start + local_count;
  for (int i = start; i < end; ++i) {
    int context_index = Context::MIN_CONTEXT_SLOTS + i - start;
    RETURN_IF_EMPTY_HANDLE_VALUE(
        isolate,
        SetProperty(isolate,
                    scope_object,
                    Handle<String>(String::cast(get(i))),
                    Handle<Object>(context->get(context_index), isolate),
                    ::NONE,
                    kNonStrictMode),
        false);
  }
  return true;
}


int ScopeInfo::ParameterEntriesIndex() {
  ASSERT(length() > 0);
  return kVariablePartIndex;
}


int ScopeInfo::StackLocalEntriesIndex() {
  return ParameterEntriesIndex() + ParameterCount();
}


int ScopeInfo::ContextLocalNameEntriesIndex() {
  return StackLocalEntriesIndex() + StackLocalCount();
}


int ScopeInfo::ContextLocalInfoEntriesIndex() {
  return ContextLocalNameEntriesIndex() + ContextLocalCount();
}


int ScopeInfo::FunctionNameEntryIndex() {
  return ContextLocalInfoEntriesIndex() + ContextLocalCount();
}


int ContextSlotCache::Hash(Object* data, String* name) {
  // Uses only lower 32 bits if pointers are larger.
  uintptr_t addr_hash =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data)) >> 2;
  return static_cast<int>((addr_hash ^ name->Hash()) % kLength);
}


int ContextSlotCache::Lookup(Object* data,
                             String* name,
                             VariableMode* mode,
                             InitializationFlag* init_flag) {
  int index = Hash(data, name);
  Key& key = keys_[index];
  if ((key.data == data) && key.name->Equals(name)) {
    Value result(values_[index]);
    if (mode != NULL) *mode = result.mode();
    if (init_flag != NULL) *init_flag = result.initialization_flag();
    return result.index() + kNotFound;
  }
  return kNotFound;
}


void ContextSlotCache::Update(Object* data,
                              String* name,
                              VariableMode mode,
                              InitializationFlag init_flag,
                              int slot_index) {
  String* internalized_name;
  ASSERT(slot_index > kNotFound);
  if (HEAP->InternalizeStringIfExists(name, &internalized_name)) {
    int index = Hash(data, internalized_name);
    Key& key = keys_[index];
    key.data = data;
    key.name = internalized_name;
    // Please note value only takes a uint as index.
    values_[index] = Value(mode, init_flag, slot_index - kNotFound).raw();
#ifdef DEBUG
    ValidateEntry(data, name, mode, init_flag, slot_index);
#endif
  }
}


void ContextSlotCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].data = NULL;
}


#ifdef DEBUG

void ContextSlotCache::ValidateEntry(Object* data,
                                     String* name,
                                     VariableMode mode,
                                     InitializationFlag init_flag,
                                     int slot_index) {
  String* internalized_name;
  if (HEAP->InternalizeStringIfExists(name, &internalized_name)) {
    int index = Hash(data, name);
    Key& key = keys_[index];
    ASSERT(key.data == data);
    ASSERT(key.name->Equals(name));
    Value result(values_[index]);
    ASSERT(result.mode() == mode);
    ASSERT(result.initialization_flag() == init_flag);
    ASSERT(result.index() + kNotFound == slot_index);
  }
}


static void PrintList(const char* list_name,
                      int nof_internal_slots,
                      int start,
                      int end,
                      ScopeInfo* scope_info) {
  if (start < end) {
    PrintF("\n  // %s\n", list_name);
    if (nof_internal_slots > 0) {
      PrintF("  %2d - %2d [internal slots]\n", 0 , nof_internal_slots - 1);
    }
    for (int i = nof_internal_slots; start < end; ++i, ++start) {
      PrintF("  %2d ", i);
      String::cast(scope_info->get(start))->ShortPrint();
      PrintF("\n");
    }
  }
}


void ScopeInfo::Print() {
  PrintF("ScopeInfo ");
  if (HasFunctionName()) {
    FunctionName()->ShortPrint();
  } else {
    PrintF("/* no function name */");
  }
  PrintF("{");

  PrintList("parameters", 0,
            ParameterEntriesIndex(),
            ParameterEntriesIndex() + ParameterCount(),
            this);
  PrintList("stack slots", 0,
            StackLocalEntriesIndex(),
            StackLocalEntriesIndex() + StackLocalCount(),
            this);
  PrintList("context slots",
            Context::MIN_CONTEXT_SLOTS,
            ContextLocalNameEntriesIndex(),
            ContextLocalNameEntriesIndex() + ContextLocalCount(),
            this);

  PrintF("}\n");
}
#endif  // DEBUG


//---------------------------------------------------------------------------
// ModuleInfo.

Handle<ModuleInfo> ModuleInfo::Create(
    Isolate* isolate, Interface* interface, Scope* scope) {
  Handle<ModuleInfo> info = Allocate(isolate, interface->Length());
  info->set_host_index(interface->Index());
  int i = 0;
  for (Interface::Iterator it = interface->iterator();
       !it.done(); it.Advance(), ++i) {
    Variable* var = scope->LocalLookup(it.name());
    info->set_name(i, *it.name());
    info->set_mode(i, var->mode());
    ASSERT((var->mode() == MODULE) == (it.interface()->IsModule()));
    if (var->mode() == MODULE) {
      ASSERT(it.interface()->IsFrozen());
      ASSERT(it.interface()->Index() >= 0);
      info->set_index(i, it.interface()->Index());
    } else {
      ASSERT(var->index() >= 0);
      info->set_index(i, var->index());
    }
  }
  ASSERT(i == info->length());
  return info;
}

} }  // namespace v8::internal
