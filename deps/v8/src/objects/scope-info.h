// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCOPE_INFO_H_
#define V8_OBJECTS_SCOPE_INFO_H_

#include "src/globals.h"
#include "src/objects.h"
#include "src/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class Isolate;
template <typename T>
class MaybeHandle;
class Scope;
class Zone;

// ScopeInfo represents information about different scopes of a source
// program  and the allocation of the scope's variables. Scope information
// is stored in a compressed form in ScopeInfo objects and is used
// at runtime (stack dumps, deoptimization, etc.).

// This object provides quick access to scope info details for runtime
// routines.
class ScopeInfo : public FixedArray {
 public:
  DECLARE_CAST(ScopeInfo)

  // Return the type of this scope.
  ScopeType scope_type();

  // Does this scope call eval?
  bool CallsEval();

  // Return the language mode of this scope.
  LanguageMode language_mode();

  // True if this scope is a (var) declaration scope.
  bool is_declaration_scope();

  // Does this scope make a sloppy eval call?
  bool CallsSloppyEval() { return CallsEval() && is_sloppy(language_mode()); }

  // Return the total number of locals allocated on the stack and in the
  // context. This includes the parameters that are allocated in the context.
  int LocalCount();

  // Return the number of stack slots for code. This number consists of two
  // parts:
  //  1. One stack slot per stack allocated local.
  //  2. One stack slot for the function name if it is stack allocated.
  int StackSlotCount();

  // Return the number of context slots for code if a context is allocated. This
  // number consists of three parts:
  //  1. Size of fixed header for every context: Context::MIN_CONTEXT_SLOTS
  //  2. One context slot per context allocated local.
  //  3. One context slot for the function name if it is context allocated.
  // Parameters allocated in the context count as context allocated locals. If
  // no contexts are allocated for this scope ContextLength returns 0.
  int ContextLength();

  // Does this scope declare a "this" binding?
  bool HasReceiver();

  // Does this scope declare a "this" binding, and the "this" binding is stack-
  // or context-allocated?
  bool HasAllocatedReceiver();

  // Does this scope declare a "new.target" binding?
  bool HasNewTarget();

  // Is this scope the scope of a named function expression?
  bool HasFunctionName();

  // Return if this has context allocated locals.
  bool HasHeapAllocatedLocals();

  // Return if contexts are allocated for this scope.
  bool HasContext();

  // Return if this is a function scope with "use asm".
  inline bool IsAsmModule() { return AsmModuleField::decode(Flags()); }

  // Return if this is a nested function within an asm module scope.
  inline bool IsAsmFunction() { return AsmFunctionField::decode(Flags()); }

  inline bool HasSimpleParameters() {
    return HasSimpleParametersField::decode(Flags());
  }

  // Return the function_name if present.
  String* FunctionName();

  ModuleInfo* ModuleDescriptorInfo();

  // Return the name of the given parameter.
  String* ParameterName(int var);

  // Return the name of the given local.
  String* LocalName(int var);

  // Return the name of the given stack local.
  String* StackLocalName(int var);

  // Return the name of the given stack local.
  int StackLocalIndex(int var);

  // Return the name of the given context local.
  String* ContextLocalName(int var);

  // Return the mode of the given context local.
  VariableMode ContextLocalMode(int var);

  // Return the initialization flag of the given context local.
  InitializationFlag ContextLocalInitFlag(int var);

  // Return the initialization flag of the given context local.
  MaybeAssignedFlag ContextLocalMaybeAssignedFlag(int var);

  // Return true if this local was introduced by the compiler, and should not be
  // exposed to the user in a debugger.
  static bool VariableIsSynthetic(String* name);

  // Lookup support for serialized scope info. Returns the
  // the stack slot index for a given slot name if the slot is
  // present; otherwise returns a value < 0. The name must be an internalized
  // string.
  int StackSlotIndex(String* name);

  // Lookup support for serialized scope info. Returns the local context slot
  // index for a given slot name if the slot is present; otherwise
  // returns a value < 0. The name must be an internalized string.
  // If the slot is present and mode != NULL, sets *mode to the corresponding
  // mode for that variable.
  static int ContextSlotIndex(Handle<ScopeInfo> scope_info, Handle<String> name,
                              VariableMode* mode, InitializationFlag* init_flag,
                              MaybeAssignedFlag* maybe_assigned_flag);

  // Lookup metadata of a MODULE-allocated variable.  Return 0 if there is no
  // module variable with the given name (the index value of a MODULE variable
  // is never 0).
  int ModuleIndex(Handle<String> name, VariableMode* mode,
                  InitializationFlag* init_flag,
                  MaybeAssignedFlag* maybe_assigned_flag);

  // Lookup the name of a certain context slot by its index.
  String* ContextSlotName(int slot_index);

  // Lookup support for serialized scope info. Returns the
  // parameter index for a given parameter name if the parameter is present;
  // otherwise returns a value < 0. The name must be an internalized string.
  int ParameterIndex(String* name);

  // Lookup support for serialized scope info. Returns the function context
  // slot index if the function name is present and context-allocated (named
  // function expressions, only), otherwise returns a value < 0. The name
  // must be an internalized string.
  int FunctionContextSlotIndex(String* name);

  // Lookup support for serialized scope info.  Returns the receiver context
  // slot index if scope has a "this" binding, and the binding is
  // context-allocated.  Otherwise returns a value < 0.
  int ReceiverContextSlotIndex();

  FunctionKind function_kind();

  // Returns true if this ScopeInfo is linked to a outer ScopeInfo.
  bool HasOuterScopeInfo();

  // Returns true if this ScopeInfo was created for a debug-evaluate scope.
  bool IsDebugEvaluateScope();

  // Can be used to mark a ScopeInfo that looks like a with-scope as actually
  // being a debug-evaluate scope.
  void SetIsDebugEvaluateScope();

  // Return the outer ScopeInfo if present.
  ScopeInfo* OuterScopeInfo();

#ifdef DEBUG
  bool Equals(ScopeInfo* other) const;
#endif

  static Handle<ScopeInfo> Create(Isolate* isolate, Zone* zone, Scope* scope,
                                  MaybeHandle<ScopeInfo> outer_scope);
  static Handle<ScopeInfo> CreateForWithScope(
      Isolate* isolate, MaybeHandle<ScopeInfo> outer_scope);
  static Handle<ScopeInfo> CreateGlobalThisBinding(Isolate* isolate);

  // Serializes empty scope info.
  V8_EXPORT_PRIVATE static ScopeInfo* Empty(Isolate* isolate);

#ifdef DEBUG
  void Print();
#endif

// The layout of the static part of a ScopeInfo is as follows. Each entry is
// numeric and occupies one array slot.
// 1. A set of properties of the scope.
// 2. The number of parameters. For non-function scopes this is 0.
// 3. The number of non-parameter variables allocated on the stack.
// 4. The number of non-parameter and parameter variables allocated in the
//    context.
#define FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(V) \
  V(Flags)                                   \
  V(ParameterCount)                          \
  V(StackLocalCount)                         \
  V(ContextLocalCount)

#define FIELD_ACCESSORS(name)                                             \
  inline void Set##name(int value) { set(k##name, Smi::FromInt(value)); } \
  inline int name() {                                                     \
    if (length() > 0) {                                                   \
      return Smi::cast(get(k##name))->value();                            \
    } else {                                                              \
      return 0;                                                           \
    }                                                                     \
  }

  FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(FIELD_ACCESSORS)
#undef FIELD_ACCESSORS

  enum {
#define DECL_INDEX(name) k##name,
    FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(DECL_INDEX)
#undef DECL_INDEX
        kVariablePartIndex
  };

 private:
  // The layout of the variable part of a ScopeInfo is as follows:
  // 1. ParameterNames:
  //    This part stores the names of the parameters for function scopes. One
  //    slot is used per parameter, so in total this part occupies
  //    ParameterCount() slots in the array. For other scopes than function
  //    scopes ParameterCount() is 0.
  // 2. StackLocalFirstSlot:
  //    Index of a first stack slot for stack local. Stack locals belonging to
  //    this scope are located on a stack at slots starting from this index.
  // 3. StackLocalNames:
  //    Contains the names of local variables that are allocated on the stack,
  //    in increasing order of the stack slot index. First local variable has a
  //    stack slot index defined in StackLocalFirstSlot (point 2 above).
  //    One slot is used per stack local, so in total this part occupies
  //    StackLocalCount() slots in the array.
  // 4. ContextLocalNames:
  //    Contains the names of local variables and parameters that are allocated
  //    in the context. They are stored in increasing order of the context slot
  //    index starting with Context::MIN_CONTEXT_SLOTS. One slot is used per
  //    context local, so in total this part occupies ContextLocalCount() slots
  //    in the array.
  // 5. ContextLocalInfos:
  //    Contains the variable modes and initialization flags corresponding to
  //    the context locals in ContextLocalNames. One slot is used per
  //    context local, so in total this part occupies ContextLocalCount()
  //    slots in the array.
  // 6. ReceiverInfo:
  //    If the scope binds a "this" value, one slot is reserved to hold the
  //    context or stack slot index for the variable.
  // 7. FunctionNameInfo:
  //    If the scope belongs to a named function expression this part contains
  //    information about the function variable. It always occupies two array
  //    slots:  a. The name of the function variable.
  //            b. The context or stack slot index for the variable.
  // 8. OuterScopeInfoIndex:
  //    The outer scope's ScopeInfo or the hole if there's none.
  // 9. ModuleInfo, ModuleVariableCount, and ModuleVariables:
  //    For a module scope, this part contains the ModuleInfo, the number of
  //    MODULE-allocated variables, and the metadata of those variables.  For
  //    non-module scopes it is empty.
  int ParameterNamesIndex();
  int StackLocalFirstSlotIndex();
  int StackLocalNamesIndex();
  int ContextLocalNamesIndex();
  int ContextLocalInfosIndex();
  int ReceiverInfoIndex();
  int FunctionNameInfoIndex();
  int OuterScopeInfoIndex();
  int ModuleInfoIndex();
  int ModuleVariableCountIndex();
  int ModuleVariablesIndex();

  int Lookup(Handle<String> name, int start, int end, VariableMode* mode,
             VariableLocation* location, InitializationFlag* init_flag,
             MaybeAssignedFlag* maybe_assigned_flag);

  // Get metadata of i-th MODULE-allocated variable, where 0 <= i <
  // ModuleVariableCount.  The metadata is returned via out-arguments, which may
  // be nullptr if the corresponding information is not requested
  void ModuleVariable(int i, String** name, int* index,
                      VariableMode* mode = nullptr,
                      InitializationFlag* init_flag = nullptr,
                      MaybeAssignedFlag* maybe_assigned_flag = nullptr);

  // Used for the function name variable for named function expressions, and for
  // the receiver.
  enum VariableAllocationInfo { NONE, STACK, CONTEXT, UNUSED };

  // Properties of scopes.
  class ScopeTypeField : public BitField<ScopeType, 0, 4> {};
  class CallsEvalField : public BitField<bool, ScopeTypeField::kNext, 1> {};
  STATIC_ASSERT(LANGUAGE_END == 2);
  class LanguageModeField
      : public BitField<LanguageMode, CallsEvalField::kNext, 1> {};
  class DeclarationScopeField
      : public BitField<bool, LanguageModeField::kNext, 1> {};
  class ReceiverVariableField
      : public BitField<VariableAllocationInfo, DeclarationScopeField::kNext,
                        2> {};
  class HasNewTargetField
      : public BitField<bool, ReceiverVariableField::kNext, 1> {};
  class FunctionVariableField
      : public BitField<VariableAllocationInfo, HasNewTargetField::kNext, 2> {};
  class AsmModuleField
      : public BitField<bool, FunctionVariableField::kNext, 1> {};
  class AsmFunctionField : public BitField<bool, AsmModuleField::kNext, 1> {};
  class HasSimpleParametersField
      : public BitField<bool, AsmFunctionField::kNext, 1> {};
  class FunctionKindField
      : public BitField<FunctionKind, HasSimpleParametersField::kNext, 10> {};
  class HasOuterScopeInfoField
      : public BitField<bool, FunctionKindField::kNext, 1> {};
  class IsDebugEvaluateScopeField
      : public BitField<bool, HasOuterScopeInfoField::kNext, 1> {};

  // Properties of variables.
  class VariableModeField : public BitField<VariableMode, 0, 3> {};
  class InitFlagField : public BitField<InitializationFlag, 3, 1> {};
  class MaybeAssignedFlagField : public BitField<MaybeAssignedFlag, 4, 1> {};

  friend class ScopeIterator;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCOPE_INFO_H_
