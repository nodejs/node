// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCOPE_INFO_H_
#define V8_OBJECTS_SCOPE_INFO_H_

#include "src/common/globals.h"
#include "src/objects/dependent-code.h"
#include "src/objects/fixed-array.h"
#include "src/objects/function-kind.h"
#include "src/objects/objects.h"
#include "src/objects/tagged-field.h"
#include "src/utils/utils.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// scope-info-tq.inc uses NameToIndexHashTable.
class NameToIndexHashTable;

#include "torque-generated/src/objects/scope-info-tq.inc"

class SourceTextModuleInfo;
class StringSet;
class Zone;

V8_EXPORT_PRIVATE const char* ToString(ScopeType type);

inline std::ostream& operator<<(std::ostream& os, ScopeType type) {
  return os << ToString(type);
}

struct VariableLookupResult {
  uint32_t context_index;
  int slot_index;
  // repl_mode flag is needed to disable inlining of 'const' variables in REPL
  // mode.
  bool is_repl_mode;
  IsStaticFlag is_static_flag;
  VariableMode mode;
  InitializationFlag init_flag;
  MaybeAssignedFlag maybe_assigned_flag;
  int initializer_position;
};

// ScopeInfo represents information about different scopes of a source
// program  and the allocation of the scope's variables. Scope information
// is stored in a compressed form in ScopeInfo objects and is used
// at runtime (stack dumps, deoptimization, etc.).

// This object provides quick access to scope info details for runtime
// routines.
V8_OBJECT class ScopeInfo : public HeapObject {
 public:
  DEFINE_TORQUE_GENERATED_SCOPE_FLAGS()

  DECL_PRINTER(ScopeInfo)
  DECL_VERIFIER(ScopeInfo)
  class BodyDescriptor;

  // Return the type of this scope.
  ScopeType scope_type() const;

  // The maximum distance from the start position that can be stored for a
  // variable's initializer position. High distance results in kMaxInt.
  static constexpr int kMaxVariablePositionDistance = 0xFFFF;

  // Return the language mode of this scope.
  LanguageMode language_mode() const;

  // True if this scope is a (var) declaration scope.
  bool is_declaration_scope() const;

  // Does this scope make a sloppy eval call?
  bool SloppyEvalCanExtendVars() const;

  // Return the number of context slots for code if a context is allocated. This
  // number consists of three parts:
  //  1. Size of header for every context.
  //  2. One context slot per context allocated local.
  //  3. One context slot for the function name if it is context allocated.
  // Parameters allocated in the context count as context allocated locals. If
  // no contexts are allocated for this scope ContextLength returns 0.
  int ContextLength() const;
  int ContextHeaderLength() const;

  // Returns true if the respective contexts have a context extension slot.
  V8_EXPORT_PRIVATE bool HasContextExtensionSlot() const;

  // Returns true if there is a context with created context extension
  // (meaningful only for contexts that call sloppy eval, see
  // SloppyEvalCanExtendVars()).
  bool SomeContextHasExtension() const;
  void mark_some_context_has_extension();

  // Does this scope declare a "this" binding?
  bool HasReceiver() const;

  // Does this scope declare a "this" binding, and the "this" binding is stack-
  // or context-allocated?
  bool HasAllocatedReceiver() const;

  // Does this scope has class brand (for private methods)? If it's a class
  // scope, this indicates whether the class has a private brand. If it's a
  // constructor scope, this indicates whether it needs to initialize the
  // brand.
  bool ClassScopeHasPrivateBrand() const;

  // Does this scope contain a saved class variable for checking receivers of
  // static private methods?
  bool HasSavedClassVariable() const;

  bool IsSloppyNormalJSFunction() const;

  // This (function) scope cannot access rest parameters or the arguments
  // exotic object.
  V8_EXPORT_PRIVATE bool CanOnlyAccessFixedFormalParameters() const;

  // Is this scope the scope of a named function expression?
  V8_EXPORT_PRIVATE bool HasFunctionName() const;

  bool HasContextAllocatedFunctionName() const;

  // See SharedFunctionInfo::HasSharedName.
  V8_EXPORT_PRIVATE bool HasSharedFunctionName() const;

  V8_EXPORT_PRIVATE bool HasInferredFunctionName() const;

  void SetFunctionName(Tagged<UnionOf<Smi, String>> name);
  void SetInferredFunctionName(Tagged<String> name);

  // Does this scope belong to a function?
  bool HasPositionInfo() const;

  bool IsWrappedFunctionScope() const;

  // Return if contexts are allocated for this scope.
  bool HasContext() const;

  // Return if this is a function scope with "use asm".
  inline bool IsAsmModule() const;

  inline bool HasSimpleParameters() const;

  inline bool HasContextCells() const;

  inline bool is_hoisted_in_context() const;

  // Return the function_name if present.
  V8_EXPORT_PRIVATE Tagged<UnionOf<Smi, String>> FunctionName() const;

  // The function's name if it is non-empty, otherwise the inferred name or an
  // empty string.
  Tagged<String> FunctionDebugName() const;

  // Return the function's inferred name if present.
  // See SharedFunctionInfo::function_identifier.
  V8_EXPORT_PRIVATE Tagged<Object> InferredFunctionName() const;

  // Position information accessors.
  int StartPosition() const;
  int EndPosition() const;
  void SetPositionInfo(int start, int end);

  int UniqueIdInScript() const;

  Tagged<SourceTextModuleInfo> ModuleDescriptorInfo() const;

  // Return true if the local names are inlined in the scope info object.
  inline bool HasInlinedLocalNames() const;

  template <typename ScopeInfoPtr>
  class LocalNamesRange;

  static inline LocalNamesRange<DirectHandle<ScopeInfo>> IterateLocalNames(
      DirectHandle<ScopeInfo> scope_info);

  static inline LocalNamesRange<Tagged<ScopeInfo>> IterateLocalNames(
      Tagged<ScopeInfo> scope_info, const DisallowGarbageCollection& no_gc);

  // Return the name of a given context local.
  // It should only be used if inlined local names.
  Tagged<String> ContextInlinedLocalName(int var) const;

  // Return the mode of the given context local.
  VariableMode ContextLocalMode(int var) const;

  // Return whether the given context local variable is static.
  IsStaticFlag ContextLocalIsStaticFlag(int var) const;

  // Return the initialization flag of the given context local.
  InitializationFlag ContextLocalInitFlag(int var) const;

  bool ContextLocalIsParameter(int var) const;
  uint32_t ContextLocalParameterNumber(int var) const;
  int ContextLocalInitializerPosition(int var) const;

  // Return the initialization flag of the given context local.
  MaybeAssignedFlag ContextLocalMaybeAssignedFlag(int var) const;

  // Return true if this local was introduced by the compiler, and should not be
  // exposed to the user in a debugger.
  static bool VariableIsSynthetic(Tagged<String> name);

  // Lookup support for serialized scope info. Returns the local context slot
  // index for a given slot name if the slot is present; otherwise
  // returns a value < 0. The name must be an internalized string.
  // If the slot is present and mode != nullptr, sets *mode to the corresponding
  // mode for that variable.
  int ContextSlotIndex(Tagged<String> name);
  int ContextSlotIndex(Tagged<String> name,
                       VariableLookupResult* lookup_result);

  // Lookup metadata of a MODULE-allocated variable.  Return 0 if there is no
  // module variable with the given name (the index value of a MODULE variable
  // is never 0).
  int ModuleIndex(Tagged<String> name, VariableMode* mode,
                  InitializationFlag* init_flag,
                  MaybeAssignedFlag* maybe_assigned_flag = nullptr,
                  int* initializer_position = nullptr);

  int ModuleVariableCount() const;

  // Lookup support for serialized scope info. Returns the function context
  // slot index if the function name is present and context-allocated (named
  // function expressions, only), otherwise returns a value < 0. The name
  // must be an internalized string.
  int FunctionContextSlotIndex(Tagged<String> name) const;
  // Same as above but works without knowing the name.
  int FunctionContextSlotIndex() const;

  // Lookup support for serialized scope info.  Returns the receiver context
  // slot index if scope has a "this" binding, and the binding is
  // context-allocated.  Otherwise returns a value < 0.
  int ReceiverContextSlotIndex() const;

  // Returns the first parameter context slot index.
  int ParametersStartIndex() const;

  // Lookup support for serialized scope info.  Returns the name and index of
  // the saved class variable in context local slots if scope is a class scope
  // and it contains static private methods that may be accessed.
  std::pair<Tagged<String>, int> SavedClassVariable() const;

  FunctionKind function_kind() const;

  // Returns true if this ScopeInfo is linked to an outer ScopeInfo.
  bool HasOuterScopeInfo() const;

  // Returns true if this ScopeInfo was created for a debug-evaluate scope.
  bool IsDebugEvaluateScope() const;

  // Can be used to mark a ScopeInfo that looks like a with-scope as actually
  // being a debug-evaluate scope.
  void SetIsDebugEvaluateScope();

  // Return the outer ScopeInfo if present.
  Tagged<ScopeInfo> OuterScopeInfo() const;

  bool is_script_scope() const;

  // Returns true if this ScopeInfo was created for a scope that skips the
  // closest outer class when resolving private names.
  bool PrivateNameLookupSkipsOuterClass() const;

  // REPL mode scopes allow re-declaraction of let and const variables. They
  // come from debug evaluate but are different to IsDebugEvaluateScope().
  bool IsReplModeScope() const;

  // For LiveEdit we ignore:
  //   - position info: "unchanged" functions are allowed to move in a script
  //   - module info: SourceTextModuleInfo::Equals compares exact FixedArray
  //     addresses which will never match for separate instances.
  //   - outer scope info: LiveEdit already analyses outer scopes of unchanged
  //     functions. Also checking it here will break in really subtle cases
  //     e.g. changing a let to a const in an outer function, which is fine.
  bool Equals(Tagged<ScopeInfo> other, bool is_live_edit_compare = false,
              int* out_last_checked_field = nullptr) const;

  template <typename IsolateT>
  static Handle<ScopeInfo> Create(IsolateT* isolate, Zone* zone, Scope* scope,
                                  MaybeDirectHandle<ScopeInfo> outer_scope,
                                  FunctionKind closure_function_kind);
  V8_EXPORT_PRIVATE static DirectHandle<ScopeInfo> CreateForWithScope(
      Isolate* isolate, MaybeDirectHandle<ScopeInfo> outer_scope);
  V8_EXPORT_PRIVATE static DirectHandle<ScopeInfo> CreateForEmptyFunction(
      Isolate* isolate);
  static DirectHandle<ScopeInfo> CreateForNativeContext(Isolate* isolate);
  static DirectHandle<ScopeInfo> CreateForShadowRealmNativeContext(
      Isolate* isolate);
  static DirectHandle<ScopeInfo> CreateGlobalThisBinding(Isolate* isolate);

  // Serializes empty scope info.
  V8_EXPORT_PRIVATE static Tagged<ScopeInfo> Empty(Isolate* isolate);

  inline uint32_t Flags() const;
  inline int ParameterCount() const;
  inline int ContextLocalCount() const;

  // Pre-port flat-index field identifiers, preserved for the crash-dump
  // `out_last_checked_field` encoding in Equals(). Indices 0..4 identify
  // fixed-header fields; values >= kVariablePartStart encode
  // (kVariablePartStart + tail_index).
  enum Fields {
    kFlags,
    kParameterCount,
    kContextLocalCount,
    kPositionInfoStart,
    kPositionInfoEnd,
    kVariablePartStart
  };

  static_assert(LanguageModeSize == 1 << LanguageModeBit::kSize);
  static_assert(FunctionKindBits::is_valid(FunctionKind::kLastFunctionKind));

  // Named field accessors.
  inline uint32_t flags(RelaxedLoadTag) const;
  inline void set_flags(uint32_t value, RelaxedStoreTag);

  inline int parameter_count() const;
  inline void set_parameter_count(int value);

  inline int context_local_count() const;
  inline void set_context_local_count(int value);

  inline int position_info_start() const;
  inline void set_position_info_start(int value);

  inline int position_info_end() const;
  inline void set_position_info_end(int value);

  inline int module_variable_count() const;
  inline void set_module_variable_count(int value);

  // Variable-length array accessors. Indices are bounds-checked in debug
  // builds against the corresponding slice length.
  inline Tagged<String> context_local_names(int i) const;
  inline void set_context_local_names(
      int i, Tagged<String> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<NameToIndexHashTable> context_local_names_hashtable() const;
  inline void set_context_local_names_hashtable(
      Tagged<NameToIndexHashTable> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int context_local_infos(int i) const;
  inline void set_context_local_infos(int i, int value);

  inline Tagged<Union<Name, Smi>> saved_class_variable_info() const;
  inline void set_saved_class_variable_info(
      Tagged<Union<Name, Smi>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Union<Smi, String>> function_variable_info_name() const;
  inline void set_function_variable_info_name(
      Tagged<Union<Smi, String>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int function_variable_info_context_or_stack_slot_index() const;
  inline void set_function_variable_info_context_or_stack_slot_index(int value);

  inline Tagged<Union<String, Undefined>> inferred_function_name() const;
  inline void set_inferred_function_name(
      Tagged<Union<String, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<ScopeInfo> outer_scope_info() const;
  inline void set_outer_scope_info(
      Tagged<ScopeInfo> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> module_info() const;
  inline void set_module_info(Tagged<FixedArray> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> module_variables_name(int i) const;
  inline void set_module_variables_name(
      int i, Tagged<String> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int module_variables_index(int i) const;
  inline void set_module_variables_index(int i, int value);

  inline int module_variables_properties(int i) const;
  inline void set_module_variables_properties(int i, int value);

  inline Tagged<DependentCode> dependent_code() const;
  inline void set_dependent_code(Tagged<DependentCode> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int unused_parameter_bits() const;
  inline void set_unused_parameter_bits(int value);

  // Conditional-slice offset accessors. Each returns the byte offset of
  // the corresponding variable-length field. Offsets chain off of each
  // other based on flags and preceding array lengths.
  inline int ModuleVariableCountOffset() const;
  inline int ContextLocalNamesOffset() const;
  inline int ContextLocalNamesHashtableOffset() const;
  inline int ContextLocalInfosOffset() const;
  inline int SavedClassVariableInfoOffset() const;
  inline int FunctionVariableInfoOffset() const;
  inline int InferredFunctionNameOffset() const;
  inline int OuterScopeInfoOffset() const;
  inline int ModuleInfoOffset() const;
  inline int ModuleVariablesOffset() const;
  inline int DependentCodeOffset() const;
  inline int UnusedParameterBitsOffset() const;

  // Total byte size of this object (depends on flags and local counts).
  inline int AllocatedSize() const;

  bool IsEmpty() const;

  // Returns the size in bytes for a ScopeInfo with |length| variable-part
  // (tail) slots. |length| is the count returned by length(). Defined
  // out-of-line because it depends on OFFSET_OF_DATA_START(ScopeInfo).
  static inline constexpr int SizeFor(int length);

  // Zero-initializes all GC-visible tagged slots (4 fixed Smi header fields
  // plus |tail_length| variable-part slots) with `value`. Safe to call only
  // immediately after allocation, before GC or any reader observes the
  // object; callers pass `tail_length` explicitly because the flags-derived
  // accessors (including length()) are not yet valid at this point.
  inline void InitializeTaggedMembers(Tagged<Object> value, int tail_length);

  // Hash based on position info and flags. Falls back to flags + local count.
  V8_EXPORT_PRIVATE uint32_t Hash();

  static const int kFlagsOffsetEnd;
  static const int kPositionInfoOffsetEnd;
  static const int kHeaderSize;
  static const int kModuleVariableCountOffset;

  // Offset of the first tagged slot. Everything from here to the end of
  // the object (fixed TaggedMember<Smi> header fields + variable-part
  // tail) is the GC-visible tagged region visited by BodyDescriptor.
  // Number of fixed TaggedMember<Smi> header slots
  // (parameter_count_..position_info_end_) that sit between the non-tagged
  // flags/padding and the variable-part tail (data[]).
  static const int kFixedTaggedHeaderSlotCount;

 private:
  int InlinedLocalNamesLookup(Tagged<String> name);

  int ContextLocalNamesIndex() const;
  int ContextLocalInfosIndex() const;
  int SavedClassVariableInfoIndex() const;
  int FunctionVariableInfoIndex() const;
  int InferredFunctionNameIndex() const;
  int OuterScopeInfoIndex() const;
  int ModuleInfoIndex() const;
  int ModuleVariableCountIndex() const;
  int ModuleVariablesIndex() const;
  int DependentCodeIndex() const;
  int UnusedParameterBitsIndex() const;

  // Raw access by slot index into the variable-part tail. `index` is
  // 0-based; valid range is [0, length()).
  V8_EXPORT_PRIVATE Tagged<Object> get(int index) const;
  // Setter that doesn't need write barrier.
  void set(int index, Tagged<Smi> value);
  // Setter with explicit barrier mode.
  void set(int index, Tagged<Object> value,
           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  void CopyElements(Isolate* isolate, int dst_index, Tagged<ScopeInfo> src,
                    int src_index, int len, WriteBarrierMode mode);
  ObjectSlot RawFieldOfElementAt(int index);
  // The number of tagged-pointer-sized slots in the variable-part tail.
  V8_EXPORT_PRIVATE int length() const;

  // Conversions between offset (bytes from the beginning of the object)
  // and index (0-based position in the variable-part tail). Defined
  // out-of-line below the class because they depend on
  // OFFSET_OF_DATA_START(ScopeInfo).
  static inline constexpr int OffsetOfElementAt(int index);
  static inline constexpr int ConvertOffsetToIndex(int offset);

  enum class BootstrappingType { kScript, kFunction, kNative, kShadowRealm };
  static DirectHandle<ScopeInfo> CreateForBootstrapping(Isolate* isolate,
                                                        BootstrappingType type);

  int Lookup(Handle<String> name, int start, int end, VariableMode* mode,
             VariableLocation* location, InitializationFlag* init_flag,
             MaybeAssignedFlag* maybe_assigned_flag);

  // Get metadata of i-th MODULE-allocated variable, where 0 <= i <
  // ModuleVariableCount.  The metadata is returned via out-arguments, which may
  // be nullptr if the corresponding information is not requested
  void ModuleVariable(int i, Tagged<String>* name, int* index,
                      VariableMode* mode = nullptr,
                      InitializationFlag* init_flag = nullptr,
                      MaybeAssignedFlag* maybe_assigned_flag = nullptr,
                      int* initializer_position = nullptr);

  static const int kFunctionNameEntries =
      TorqueGeneratedFunctionVariableInfoOffsets::kSize / kTaggedSize;
  static const int kModuleVariableEntryLength =
      TorqueGeneratedModuleVariableOffsets::kSize / kTaggedSize;

  // Properties of variables.
  DEFINE_TORQUE_GENERATED_VARIABLE_PROPERTIES()

  friend class ScopeIterator;
  friend class CodeStubAssembler;
  friend std::ostream& operator<<(std::ostream& os, VariableAllocationInfo var);

 public:
  // Relaxed-atomic flags word (ScopeFlags bit layout).
  std::atomic<uint32_t> flags_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif
  // TODO(jgruber): Consider plain uint32_t (or similar) for these.
  TaggedMember<Smi> parameter_count_;
  TaggedMember<Smi> context_local_count_;
  TaggedMember<Smi> position_info_start_;
  TaggedMember<Smi> position_info_end_;
  // Variable-length tagged tail. The presence and position of each
  // sub-section is determined by flags and by header counts; see the
  // Foo...Offset() accessors below.
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<Object>, data);
} V8_OBJECT_END;

inline constexpr int ScopeInfo::kHeaderSize = OFFSET_OF_DATA_START(ScopeInfo);
// ModuleVariableCount is the first slot of the variable tail (data[0]).
inline constexpr int ScopeInfo::kModuleVariableCountOffset =
    OFFSET_OF_DATA_START(ScopeInfo);

// Derived from the C++ layout so adding/removing a TaggedMember<Smi>
// header field keeps this in sync automatically.
inline constexpr int ScopeInfo::kFixedTaggedHeaderSlotCount =
    (OFFSET_OF_DATA_START(ScopeInfo) - offsetof(ScopeInfo, parameter_count_)) /
    kTaggedSize;

inline constexpr int ScopeInfo::SizeFor(int length) {
  return OffsetOfElementAt(length);
}

inline constexpr int ScopeInfo::OffsetOfElementAt(int index) {
  return OFFSET_OF_DATA_START(ScopeInfo) + index * kTaggedSize;
}

inline constexpr int ScopeInfo::ConvertOffsetToIndex(int offset) {
  int index = (offset - OFFSET_OF_DATA_START(ScopeInfo)) / kTaggedSize;
  DCHECK_EQ(OffsetOfElementAt(index), offset);
  return index;
}

std::ostream& operator<<(std::ostream& os, VariableAllocationInfo var);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCOPE_INFO_H_
