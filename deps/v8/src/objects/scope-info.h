// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCOPE_INFO_H_
#define V8_OBJECTS_SCOPE_INFO_H_

#include "src/common/globals.h"
#include "src/objects/fixed-array.h"
#include "src/objects/function-kind.h"
#include "src/objects/objects.h"
#include "src/utils/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class Isolate;
template <typename T>
class MaybeHandle;
class SourceTextModuleInfo;
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
  DECL_CAST(ScopeInfo)
  DECL_PRINTER(ScopeInfo)

  // Return the type of this scope.
  ScopeType scope_type() const;

  // Return the language mode of this scope.
  LanguageMode language_mode() const;

  // True if this scope is a (var) declaration scope.
  bool is_declaration_scope() const;

  // True if this scope is a class scope.
  bool is_class_scope() const;

  // Does this scope make a sloppy eval call?
  bool SloppyEvalCanExtendVars() const;

  // Return the number of context slots for code if a context is allocated. This
  // number consists of three parts:
  //  1. Size of fixed header for every context: Context::MIN_CONTEXT_SLOTS
  //  2. One context slot per context allocated local.
  //  3. One context slot for the function name if it is context allocated.
  // Parameters allocated in the context count as context allocated locals. If
  // no contexts are allocated for this scope ContextLength returns 0.
  int ContextLength() const;

  // Does this scope declare a "this" binding?
  bool HasReceiver() const;

  // Does this scope declare a "this" binding, and the "this" binding is stack-
  // or context-allocated?
  bool HasAllocatedReceiver() const;

  // Does this scope has class brand (for private methods)?
  bool HasClassBrand() const;

  // Does this scope contains a saved class variable context local slot index
  // for checking receivers of static private methods?
  bool HasSavedClassVariableIndex() const;

  // Does this scope declare a "new.target" binding?
  bool HasNewTarget() const;

  // Is this scope the scope of a named function expression?
  V8_EXPORT_PRIVATE bool HasFunctionName() const;

  // See SharedFunctionInfo::HasSharedName.
  V8_EXPORT_PRIVATE bool HasSharedFunctionName() const;

  V8_EXPORT_PRIVATE bool HasInferredFunctionName() const;

  void SetFunctionName(Object name);
  void SetInferredFunctionName(String name);

  // Does this scope belong to a function?
  bool HasPositionInfo() const;

  // Return if contexts are allocated for this scope.
  bool HasContext() const;

  // Return if this is a function scope with "use asm".
  inline bool IsAsmModule() const;

  inline bool HasSimpleParameters() const;

  // Return the function_name if present.
  V8_EXPORT_PRIVATE Object FunctionName() const;

  // The function's name if it is non-empty, otherwise the inferred name or an
  // empty string.
  String FunctionDebugName() const;

  // Return the function's inferred name if present.
  // See SharedFunctionInfo::function_identifier.
  V8_EXPORT_PRIVATE Object InferredFunctionName() const;

  // Position information accessors.
  int StartPosition() const;
  int EndPosition() const;
  void SetPositionInfo(int start, int end);

  SourceTextModuleInfo ModuleDescriptorInfo() const;

  // Return the name of the given context local.
  String ContextLocalName(int var) const;

  // Return the mode of the given context local.
  VariableMode ContextLocalMode(int var) const;

  // Return whether the given context local variable is static.
  IsStaticFlag ContextLocalIsStaticFlag(int var) const;

  // Return the initialization flag of the given context local.
  InitializationFlag ContextLocalInitFlag(int var) const;

  bool ContextLocalIsParameter(int var) const;
  uint32_t ContextLocalParameterNumber(int var) const;

  // Return the initialization flag of the given context local.
  MaybeAssignedFlag ContextLocalMaybeAssignedFlag(int var) const;

  // Return true if this local was introduced by the compiler, and should not be
  // exposed to the user in a debugger.
  static bool VariableIsSynthetic(String name);

  // Lookup support for serialized scope info. Returns the local context slot
  // index for a given slot name if the slot is present; otherwise
  // returns a value < 0. The name must be an internalized string.
  // If the slot is present and mode != nullptr, sets *mode to the corresponding
  // mode for that variable.
  static int ContextSlotIndex(ScopeInfo scope_info, String name,
                              VariableMode* mode, InitializationFlag* init_flag,
                              MaybeAssignedFlag* maybe_assigned_flag,
                              IsStaticFlag* is_static_flag);

  // Lookup metadata of a MODULE-allocated variable.  Return 0 if there is no
  // module variable with the given name (the index value of a MODULE variable
  // is never 0).
  int ModuleIndex(String name, VariableMode* mode,
                  InitializationFlag* init_flag,
                  MaybeAssignedFlag* maybe_assigned_flag);

  // Lookup support for serialized scope info. Returns the function context
  // slot index if the function name is present and context-allocated (named
  // function expressions, only), otherwise returns a value < 0. The name
  // must be an internalized string.
  int FunctionContextSlotIndex(String name) const;

  // Lookup support for serialized scope info.  Returns the receiver context
  // slot index if scope has a "this" binding, and the binding is
  // context-allocated.  Otherwise returns a value < 0.
  int ReceiverContextSlotIndex() const;

  // Lookup support for serialized scope info.  Returns the index of the
  // saved class variable in context local slots if scope is a class scope
  // and it contains static private methods that may be accessed.
  // Otherwise returns a value < 0.
  int SavedClassVariableContextLocalIndex() const;

  FunctionKind function_kind() const;

  // Returns true if this ScopeInfo is linked to a outer ScopeInfo.
  bool HasOuterScopeInfo() const;

  // Returns true if this ScopeInfo was created for a debug-evaluate scope.
  bool IsDebugEvaluateScope() const;

  // Can be used to mark a ScopeInfo that looks like a with-scope as actually
  // being a debug-evaluate scope.
  void SetIsDebugEvaluateScope();

  // Return the outer ScopeInfo if present.
  ScopeInfo OuterScopeInfo() const;

  // Returns true if this ScopeInfo was created for a scope that skips the
  // closest outer class when resolving private names.
  bool PrivateNameLookupSkipsOuterClass() const;

#ifdef DEBUG
  bool Equals(ScopeInfo other) const;
#endif

  static Handle<ScopeInfo> Create(Isolate* isolate, Zone* zone, Scope* scope,
                                  MaybeHandle<ScopeInfo> outer_scope);
  static Handle<ScopeInfo> CreateForWithScope(
      Isolate* isolate, MaybeHandle<ScopeInfo> outer_scope);
  V8_EXPORT_PRIVATE static Handle<ScopeInfo> CreateForEmptyFunction(
      Isolate* isolate);
  static Handle<ScopeInfo> CreateGlobalThisBinding(Isolate* isolate);

  // Serializes empty scope info.
  V8_EXPORT_PRIVATE static ScopeInfo Empty(Isolate* isolate);

// The layout of the static part of a ScopeInfo is as follows. Each entry is
// numeric and occupies one array slot.
// 1. A set of properties of the scope.
// 2. The number of parameters. For non-function scopes this is 0.
// 3. The number of non-parameter and parameter variables allocated in the
//    context.
#define FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(V) \
  V(Flags)                                   \
  V(ParameterCount)                          \
  V(ContextLocalCount)

#define FIELD_ACCESSORS(name)       \
  inline void Set##name(int value); \
  inline int name() const;
  FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(FIELD_ACCESSORS)
#undef FIELD_ACCESSORS

  enum Fields {
#define DECL_INDEX(name) k##name,
    FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(DECL_INDEX)
#undef DECL_INDEX
        kVariablePartIndex
  };

  // Used for the function name variable for named function expressions, and for
  // the receiver.
  enum VariableAllocationInfo { NONE, STACK, CONTEXT, UNUSED };

  // Properties of scopes.
  using ScopeTypeField = BitField<ScopeType, 0, 4>;
  using SloppyEvalCanExtendVarsField = ScopeTypeField::Next<bool, 1>;
  STATIC_ASSERT(LanguageModeSize == 2);
  using LanguageModeField = SloppyEvalCanExtendVarsField::Next<LanguageMode, 1>;
  using DeclarationScopeField = LanguageModeField::Next<bool, 1>;
  using ReceiverVariableField =
      DeclarationScopeField::Next<VariableAllocationInfo, 2>;
  using HasClassBrandField = ReceiverVariableField::Next<bool, 1>;
  using HasSavedClassVariableIndexField = HasClassBrandField::Next<bool, 1>;
  using HasNewTargetField = HasSavedClassVariableIndexField::Next<bool, 1>;
  using FunctionVariableField =
      HasNewTargetField::Next<VariableAllocationInfo, 2>;
  // TODO(cbruni): Combine with function variable field when only storing the
  // function name.
  using HasInferredFunctionNameField = FunctionVariableField::Next<bool, 1>;
  using IsAsmModuleField = HasInferredFunctionNameField::Next<bool, 1>;
  using HasSimpleParametersField = IsAsmModuleField::Next<bool, 1>;
  using FunctionKindField = HasSimpleParametersField::Next<FunctionKind, 5>;
  using HasOuterScopeInfoField = FunctionKindField::Next<bool, 1>;
  using IsDebugEvaluateScopeField = HasOuterScopeInfoField::Next<bool, 1>;
  using ForceContextAllocationField = IsDebugEvaluateScopeField::Next<bool, 1>;
  using PrivateNameLookupSkipsOuterClassField =
      ForceContextAllocationField::Next<bool, 1>;

  STATIC_ASSERT(kLastFunctionKind <= FunctionKindField::kMax);

 private:
  // The layout of the variable part of a ScopeInfo is as follows:
  // 1. ContextLocalNames:
  //    Contains the names of local variables and parameters that are allocated
  //    in the context. They are stored in increasing order of the context slot
  //    index starting with Context::MIN_CONTEXT_SLOTS. One slot is used per
  //    context local, so in total this part occupies ContextLocalCount() slots
  //    in the array.
  // 2. ContextLocalInfos:
  //    Contains the variable modes and initialization flags corresponding to
  //    the context locals in ContextLocalNames. One slot is used per
  //    context local, so in total this part occupies ContextLocalCount()
  //    slots in the array.
  // 3. SavedClassVariableInfo:
  //    If the scope is a class scope and it has static private methods that
  //    may be accessed directly or through eval, one slot is reserved to hold
  //    the context slot index for the class variable.
  // 4. ReceiverInfo:
  //    If the scope binds a "this" value, one slot is reserved to hold the
  //    context or stack slot index for the variable.
  // 5. FunctionNameInfo:
  //    If the scope belongs to a named function expression this part contains
  //    information about the function variable. It always occupies two array
  //    slots:  a. The name of the function variable.
  //            b. The context or stack slot index for the variable.
  // 6. InferredFunctionName:
  //    Contains the function's inferred name.
  // 7. SourcePosition:
  //    Contains two slots with a) the startPosition and b) the endPosition if
  //    the scope belongs to a function or script.
  // 8. OuterScopeInfoIndex:
  //    The outer scope's ScopeInfo or the hole if there's none.
  // 9. SourceTextModuleInfo, ModuleVariableCount, and ModuleVariables:
  //    For a module scope, this part contains the SourceTextModuleInfo, the
  //    number of MODULE-allocated variables, and the metadata of those
  //    variables.  For non-module scopes it is empty.
  int ContextLocalNamesIndex() const;
  int ContextLocalInfosIndex() const;
  int SavedClassVariableInfoIndex() const;
  int ReceiverInfoIndex() const;
  int FunctionNameInfoIndex() const;
  int InferredFunctionNameIndex() const;
  int PositionInfoIndex() const;
  int OuterScopeInfoIndex() const;
  int ModuleInfoIndex() const;
  int ModuleVariableCountIndex() const;
  int ModuleVariablesIndex() const;

  static bool NeedsPositionInfo(ScopeType type);
  static Handle<ScopeInfo> CreateForBootstrapping(Isolate* isolate,
                                                  ScopeType type);

  int Lookup(Handle<String> name, int start, int end, VariableMode* mode,
             VariableLocation* location, InitializationFlag* init_flag,
             MaybeAssignedFlag* maybe_assigned_flag);

  // Get metadata of i-th MODULE-allocated variable, where 0 <= i <
  // ModuleVariableCount.  The metadata is returned via out-arguments, which may
  // be nullptr if the corresponding information is not requested
  void ModuleVariable(int i, String* name, int* index,
                      VariableMode* mode = nullptr,
                      InitializationFlag* init_flag = nullptr,
                      MaybeAssignedFlag* maybe_assigned_flag = nullptr);

  static const int kFunctionNameEntries = 2;
  static const int kPositionInfoEntries = 2;

  // Properties of variables.
  using VariableModeField = BitField<VariableMode, 0, 4>;
  using InitFlagField = VariableModeField::Next<InitializationFlag, 1>;
  using MaybeAssignedFlagField = InitFlagField::Next<MaybeAssignedFlag, 1>;
  using ParameterNumberField = MaybeAssignedFlagField::Next<uint32_t, 16>;
  using IsStaticFlagField = ParameterNumberField::Next<IsStaticFlag, 1>;

  friend class ScopeIterator;
  friend std::ostream& operator<<(std::ostream& os,
                                  ScopeInfo::VariableAllocationInfo var);

  OBJECT_CONSTRUCTORS(ScopeInfo, FixedArray);
};

std::ostream& operator<<(std::ostream& os,
                         ScopeInfo::VariableAllocationInfo var);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCOPE_INFO_H_
