// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_FUNCTION_H_
#define V8_OBJECTS_JS_FUNCTION_H_

#include "src/objects/code-kind.h"
#include "src/objects/js-objects.h"
#include "torque-generated/field-offsets.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-function-tq.inc"

// An abstract superclass for classes representing JavaScript function values.
// It doesn't carry any functionality but allows function classes to be
// identified in the type system.
class JSFunctionOrBoundFunction
    : public TorqueGeneratedJSFunctionOrBoundFunction<JSFunctionOrBoundFunction,
                                                      JSObject> {
 public:
  STATIC_ASSERT(kHeaderSize == JSObject::kHeaderSize);
  TQ_OBJECT_CONSTRUCTORS(JSFunctionOrBoundFunction)
};

// JSBoundFunction describes a bound function exotic object.
class JSBoundFunction
    : public TorqueGeneratedJSBoundFunction<JSBoundFunction,
                                            JSFunctionOrBoundFunction> {
 public:
  static MaybeHandle<String> GetName(Isolate* isolate,
                                     Handle<JSBoundFunction> function);
  static Maybe<int> GetLength(Isolate* isolate,
                              Handle<JSBoundFunction> function);
  static MaybeHandle<NativeContext> GetFunctionRealm(
      Handle<JSBoundFunction> function);

  // Dispatched behavior.
  DECL_PRINTER(JSBoundFunction)
  DECL_VERIFIER(JSBoundFunction)

  // The bound function's string representation implemented according
  // to ES6 section 19.2.3.5 Function.prototype.toString ( ).
  static Handle<String> ToString(Handle<JSBoundFunction> function);

  TQ_OBJECT_CONSTRUCTORS(JSBoundFunction)
};

// JSFunction describes JavaScript functions.
class JSFunction : public JSFunctionOrBoundFunction {
 public:
  // [prototype_or_initial_map]:
  DECL_ACCESSORS(prototype_or_initial_map, HeapObject)

  // [shared]: The information about the function that
  // can be shared by instances.
  DECL_ACCESSORS(shared, SharedFunctionInfo)

  static const int kLengthDescriptorIndex = 0;
  static const int kNameDescriptorIndex = 1;
  // Home object descriptor index when function has a [[HomeObject]] slot.
  static const int kMaybeHomeObjectDescriptorIndex = 2;
  // Fast binding requires length and name accessors.
  static const int kMinDescriptorsForFastBind = 2;

  // [context]: The context for this function.
  inline Context context();
  inline bool has_context() const;
  inline void set_context(HeapObject context);
  inline JSGlobalProxy global_proxy();
  inline NativeContext native_context();
  inline int length();

  static Handle<Object> GetName(Isolate* isolate, Handle<JSFunction> function);
  static Handle<NativeContext> GetFunctionRealm(Handle<JSFunction> function);

  // [code]: The generated code object for this function.  Executed
  // when the function is invoked, e.g. foo() or new foo(). See
  // [[Call]] and [[Construct]] description in ECMA-262, section
  // 8.6.2, page 27.
  inline Code code() const;
  inline void set_code(Code code);
  inline void set_code_no_write_barrier(Code code);

  // Get the abstract code associated with the function, which will either be
  // a Code object or a BytecodeArray.
  inline AbstractCode abstract_code();

  // The predicates for querying code kinds related to this function have
  // specific terminology:
  //
  // - Attached: all code kinds that are directly attached to this JSFunction
  //   object.
  // - Available: all code kinds that are either attached or available through
  //   indirect means such as the feedback vector's optimized code cache.
  // - Active: the single code kind that would be executed if this function
  //   were called in its current state. Note that there may not be an active
  //   code kind if the function is not compiled.
  //
  // Note: code objects that are marked_for_deoptimization are not part of the
  // attached/available/active sets. This is because the JSFunction might have
  // been already deoptimized but its code() still needs to be unlinked, which
  // will happen on its next activation.

  // True, iff any generated code kind is attached/available to this function.
  V8_EXPORT_PRIVATE bool HasAttachedOptimizedCode() const;
  bool HasAvailableOptimizedCode() const;

  bool HasAvailableCodeKind(CodeKind kind) const;

  V8_EXPORT_PRIVATE bool ActiveTierIsIgnition() const;
  bool ActiveTierIsTurbofan() const;
  bool ActiveTierIsNCI() const;
  bool ActiveTierIsMidtierTurboprop() const;
  bool ActiveTierIsToptierTurboprop() const;

  CodeKind NextTier() const;

  // Similar to SharedFunctionInfo::CanDiscardCompiled. Returns true, if the
  // attached code can be recreated at a later point by replacing it with
  // CompileLazy.
  bool CanDiscardCompiled() const;

  // Tells whether or not this function checks its optimization marker in its
  // feedback vector.
  inline bool ChecksOptimizationMarker();

  // Tells whether or not this function has a (non-zero) optimization marker.
  inline bool HasOptimizationMarker();

  // Mark this function for lazy recompilation. The function will be recompiled
  // the next time it is executed.
  inline void MarkForOptimization(ConcurrencyMode mode);

  // Tells whether or not the function is already marked for lazy recompilation.
  inline bool IsMarkedForOptimization();
  inline bool IsMarkedForConcurrentOptimization();

  // Tells whether or not the function is on the concurrent recompilation queue.
  inline bool IsInOptimizationQueue();

  // Sets the optimization marker in the function's feedback vector.
  inline void SetOptimizationMarker(OptimizationMarker marker);

  // Clears the optimization marker in the function's feedback vector.
  inline void ClearOptimizationMarker();

  // If slack tracking is active, it computes instance size of the initial map
  // with minimum permissible object slack.  If it is not active, it simply
  // returns the initial map's instance size.
  int ComputeInstanceSizeWithMinSlack(Isolate* isolate);

  // Completes inobject slack tracking on initial map if it is active.
  inline void CompleteInobjectSlackTrackingIfActive();

  // [raw_feedback_cell]: Gives raw access to the FeedbackCell used to hold the
  /// FeedbackVector eventually. Generally this shouldn't be used to get the
  // feedback_vector, instead use feedback_vector() which correctly deals with
  // the JSFunction's bytecode being flushed.
  DECL_ACCESSORS(raw_feedback_cell, FeedbackCell)

  // Functions related to feedback vector. feedback_vector() can be used once
  // the function has feedback vectors allocated. feedback vectors may not be
  // available after compile when lazily allocating feedback vectors.
  inline FeedbackVector feedback_vector() const;
  inline bool has_feedback_vector() const;
  V8_EXPORT_PRIVATE static void EnsureFeedbackVector(
      Handle<JSFunction> function, IsCompiledScope* compiled_scope);

  // Functions related to closure feedback cell array that holds feedback cells
  // used to create closures from this function. We allocate closure feedback
  // cell arrays after compile, when we want to allocate feedback vectors
  // lazily.
  inline bool has_closure_feedback_cell_array() const;
  inline ClosureFeedbackCellArray closure_feedback_cell_array() const;
  static void EnsureClosureFeedbackCellArray(Handle<JSFunction> function);

  // Initializes the feedback cell of |function|. In lite mode, this would be
  // initialized to the closure feedback cell array that holds the feedback
  // cells for create closure calls from this function. In the regular mode,
  // this allocates feedback vector.
  static void InitializeFeedbackCell(Handle<JSFunction> function,
                                     IsCompiledScope* compiled_scope);

  // Unconditionally clear the type feedback vector.
  void ClearTypeFeedbackInfo();

  // Resets function to clear compiled data after bytecode has been flushed.
  inline bool NeedsResetDueToFlushedBytecode();
  inline void ResetIfBytecodeFlushed(
      base::Optional<std::function<void(HeapObject object, ObjectSlot slot,
                                        HeapObject target)>>
          gc_notify_updated_slot = base::nullopt);

  DECL_GETTER(has_prototype_slot, bool)

  // The initial map for an object created by this constructor.
  DECL_GETTER(initial_map, Map)

  static void SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                            Handle<HeapObject> prototype);
  DECL_GETTER(has_initial_map, bool)
  V8_EXPORT_PRIVATE static void EnsureHasInitialMap(
      Handle<JSFunction> function);

  // Creates a map that matches the constructor's initial map, but with
  // [[prototype]] being new.target.prototype. Because new.target can be a
  // JSProxy, this can call back into JavaScript.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Map> GetDerivedMap(
      Isolate* isolate, Handle<JSFunction> constructor,
      Handle<JSReceiver> new_target);

  // Get and set the prototype property on a JSFunction. If the
  // function has an initial map the prototype is set on the initial
  // map. Otherwise, the prototype is put in the initial map field
  // until an initial map is needed.
  DECL_GETTER(has_prototype, bool)
  DECL_GETTER(has_instance_prototype, bool)
  DECL_GETTER(prototype, Object)
  DECL_GETTER(instance_prototype, HeapObject)
  DECL_GETTER(has_prototype_property, bool)
  DECL_GETTER(PrototypeRequiresRuntimeLookup, bool)
  static void SetPrototype(Handle<JSFunction> function, Handle<Object> value);

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled() const;

  static int GetHeaderSize(bool function_has_prototype_slot) {
    return function_has_prototype_slot ? JSFunction::kSizeWithPrototype
                                       : JSFunction::kSizeWithoutPrototype;
  }

  // Prints the name of the function using PrintF.
  void PrintName(FILE* out = stdout);

  DECL_CAST(JSFunction)

  // Calculate the instance size and in-object properties count.
  // {CalculateExpectedNofProperties} can trigger compilation.
  static V8_WARN_UNUSED_RESULT int CalculateExpectedNofProperties(
      Isolate* isolate, Handle<JSFunction> function);
  static void CalculateInstanceSizeHelper(InstanceType instance_type,
                                          bool has_prototype_slot,
                                          int requested_embedder_fields,
                                          int requested_in_object_properties,
                                          int* instance_size,
                                          int* in_object_properties);

  // Dispatched behavior.
  DECL_PRINTER(JSFunction)
  DECL_VERIFIER(JSFunction)

  // The function's name if it is configured, otherwise shared function info
  // debug name.
  static Handle<String> GetName(Handle<JSFunction> function);

  // ES6 section 9.2.11 SetFunctionName
  // Because of the way this abstract operation is used in the spec,
  // it should never fail, but in practice it will fail if the generated
  // function name's length exceeds String::kMaxLength.
  static V8_WARN_UNUSED_RESULT bool SetName(Handle<JSFunction> function,
                                            Handle<Name> name,
                                            Handle<String> prefix);

  // The function's displayName if it is set, otherwise name if it is
  // configured, otherwise shared function info
  // debug name.
  static Handle<String> GetDebugName(Handle<JSFunction> function);

  // The function's string representation implemented according to
  // ES6 section 19.2.3.5 Function.prototype.toString ( ).
  static Handle<String> ToString(Handle<JSFunction> function);

  struct FieldOffsets {
    DEFINE_FIELD_OFFSET_CONSTANTS(JSFunctionOrBoundFunction::kHeaderSize,
                                  TORQUE_GENERATED_JS_FUNCTION_FIELDS)
  };
  static constexpr int kSharedFunctionInfoOffset =
      FieldOffsets::kSharedFunctionInfoOffset;
  static constexpr int kContextOffset = FieldOffsets::kContextOffset;
  static constexpr int kFeedbackCellOffset = FieldOffsets::kFeedbackCellOffset;
  static constexpr int kCodeOffset = FieldOffsets::kCodeOffset;
  static constexpr int kPrototypeOrInitialMapOffset =
      FieldOffsets::kPrototypeOrInitialMapOffset;

 private:
  // JSFunction doesn't have a fixed header size:
  // Hide JSFunctionOrBoundFunction::kHeaderSize to avoid confusion.
  static const int kHeaderSize;

  // Returns the set of code kinds of compilation artifacts (bytecode,
  // generated code) attached to this JSFunction.
  // Note that attached code objects that are marked_for_deoptimization are not
  // included in this set.
  // TODO(jgruber): Currently at most one code kind can be attached. Consider
  // adding a NOT_COMPILED kind and changing this function to simply return the
  // kind if this becomes more convenient in the future.
  CodeKinds GetAttachedCodeKinds() const;

  // As above, but also considers locations outside of this JSFunction. For
  // example the optimized code cache slot in the feedback vector, and the
  // shared function info.
  CodeKinds GetAvailableCodeKinds() const;

 public:
  static constexpr int kSizeWithoutPrototype = kPrototypeOrInitialMapOffset;
  static constexpr int kSizeWithPrototype = FieldOffsets::kHeaderSize;

  OBJECT_CONSTRUCTORS(JSFunction, JSFunctionOrBoundFunction);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_FUNCTION_H_
