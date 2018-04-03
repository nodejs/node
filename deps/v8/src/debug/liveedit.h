// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_LIVEEDIT_H_
#define V8_DEBUG_LIVEEDIT_H_


// Live Edit feature implementation.
// User should be able to change script on already running VM. This feature
// matches hot swap features in other frameworks.
//
// The basic use-case is when user spots some mistake in function body
// from debugger and wishes to change the algorithm without restart.
//
// A single change always has a form of a simple replacement (in pseudo-code):
//   script.source[positions, positions+length] = new_string;
// Implementation first determines, which function's body includes this
// change area. Then both old and new versions of script are fully compiled
// in order to analyze, whether the function changed its outer scope
// expectations (or number of parameters). If it didn't, function's code is
// patched with a newly compiled code. If it did change, enclosing function
// gets patched. All inner functions are left untouched, whatever happened
// to them in a new script version. However, new version of code will
// instantiate newly compiled functions.


#include "src/allocation.h"
#include "src/ast/ast-traversal-visitor.h"

namespace v8 {
namespace internal {

class JavaScriptFrame;

// This class collects some specific information on structure of functions
// in a particular script.
//
// The primary interest of the Tracker is to record function scope structures
// in order to analyze whether function code may be safely patched (with new
// code successfully reading existing data from function scopes). The Tracker
// also collects compiled function codes.
class LiveEditFunctionTracker
    : public AstTraversalVisitor<LiveEditFunctionTracker> {
 public:
  // Traverses the entire AST, and records information about all
  // FunctionLiterals for further use by LiveEdit code patching. The collected
  // information is returned as a serialized array.
  static Handle<JSArray> Collect(FunctionLiteral* node, Handle<Script> script,
                                 Zone* zone, Isolate* isolate);

 protected:
  friend AstTraversalVisitor<LiveEditFunctionTracker>;
  void VisitFunctionLiteral(FunctionLiteral* node);

 private:
  LiveEditFunctionTracker(Handle<Script> script, Zone* zone, Isolate* isolate);

  void FunctionStarted(FunctionLiteral* fun);
  void FunctionDone(Handle<SharedFunctionInfo> shared, Scope* scope);
  Handle<Object> SerializeFunctionScope(Scope* scope);

  Handle<Script> script_;
  Zone* zone_;
  Isolate* isolate_;

  Handle<JSArray> result_;
  int len_;
  int current_parent_index_;

  DISALLOW_COPY_AND_ASSIGN(LiveEditFunctionTracker);
};


class LiveEdit : AllStatic {
 public:
  static void InitializeThreadLocal(Debug* debug);

  MUST_USE_RESULT static MaybeHandle<JSArray> GatherCompileInfo(
      Handle<Script> script,
      Handle<String> source);

  static void ReplaceFunctionCode(Handle<JSArray> new_compile_info_array,
                                  Handle<JSArray> shared_info_array);

  static void FixupScript(Handle<Script> script, int max_function_literal_id);

  static void FunctionSourceUpdated(Handle<JSArray> shared_info_array,
                                    int new_function_literal_id);

  // Updates script field in FunctionSharedInfo.
  static void SetFunctionScript(Handle<JSValue> function_wrapper,
                                Handle<Object> script_handle);

  static void PatchFunctionPositions(Handle<JSArray> shared_info_array,
                                     Handle<JSArray> position_change_array);

  // For a script updates its source field. If old_script_name is provided
  // (i.e. is a String), also creates a copy of the script with its original
  // source and sends notification to debugger.
  static Handle<Object> ChangeScriptSource(Handle<Script> original_script,
                                           Handle<String> new_source,
                                           Handle<Object> old_script_name);

  // In a code of a parent function replaces original function as embedded
  // object with a substitution one.
  static void ReplaceRefToNestedFunction(Handle<JSValue> parent_function_shared,
                                         Handle<JSValue> orig_function_shared,
                                         Handle<JSValue> subst_function_shared);

  // Find open generator activations, and set corresponding "result" elements to
  // FUNCTION_BLOCKED_ACTIVE_GENERATOR.
  static bool FindActiveGenerators(Handle<FixedArray> shared_info_array,
                                   Handle<FixedArray> result, int len);

  // Checks listed functions on stack and return array with corresponding
  // FunctionPatchabilityStatus statuses; extra array element may
  // contain general error message. Modifies the current stack and
  // has restart the lowest found frames and drops all other frames above
  // if possible and if do_drop is true.
  static Handle<JSArray> CheckAndDropActivations(
      Handle<JSArray> old_shared_array, Handle<JSArray> new_shared_array,
      bool do_drop);

  // Restarts the call frame and completely drops all frames above it.
  // Return error message or nullptr.
  static const char* RestartFrame(JavaScriptFrame* frame);

  // A copy of this is in liveedit.js.
  enum FunctionPatchabilityStatus {
    FUNCTION_AVAILABLE_FOR_PATCH = 1,
    FUNCTION_BLOCKED_ON_ACTIVE_STACK = 2,
    FUNCTION_BLOCKED_ON_OTHER_STACK = 3,
    FUNCTION_BLOCKED_UNDER_NATIVE_CODE = 4,
    FUNCTION_REPLACED_ON_ACTIVE_STACK = 5,
    FUNCTION_BLOCKED_UNDER_GENERATOR = 6,
    FUNCTION_BLOCKED_ACTIVE_GENERATOR = 7,
    FUNCTION_BLOCKED_NO_NEW_TARGET_ON_RESTART = 8
  };

  // Compares 2 strings line-by-line, then token-wise and returns diff in form
  // of array of triplets (pos1, pos1_end, pos2_end) describing list
  // of diff chunks.
  static Handle<JSArray> CompareStrings(Handle<String> s1,
                                        Handle<String> s2);

  // Architecture-specific constant.
  static const bool kFrameDropperSupported;
};


// A general-purpose comparator between 2 arrays.
class Comparator {
 public:
  // Holds 2 arrays of some elements allowing to compare any pair of
  // element from the first array and element from the second array.
  class Input {
   public:
    virtual int GetLength1() = 0;
    virtual int GetLength2() = 0;
    virtual bool Equals(int index1, int index2) = 0;

   protected:
    virtual ~Input() {}
  };

  // Receives compare result as a series of chunks.
  class Output {
   public:
    // Puts another chunk in result list. Note that technically speaking
    // only 3 arguments actually needed with 4th being derivable.
    virtual void AddChunk(int pos1, int pos2, int len1, int len2) = 0;

   protected:
    virtual ~Output() {}
  };

  // Finds the difference between 2 arrays of elements.
  static void CalculateDifference(Input* input,
                                  Output* result_writer);
};



// Simple helper class that creates more or less typed structures over
// JSArray object. This is an adhoc method of passing structures from C++
// to JavaScript.
template<typename S>
class JSArrayBasedStruct {
 public:
  static S Create(Isolate* isolate) {
    Factory* factory = isolate->factory();
    Handle<JSArray> array = factory->NewJSArray(S::kSize_);
    return S(array);
  }

  static S cast(Object* object) {
    JSArray* array = JSArray::cast(object);
    Handle<JSArray> array_handle(array);
    return S(array_handle);
  }

  explicit JSArrayBasedStruct(Handle<JSArray> array) : array_(array) {
  }

  Handle<JSArray> GetJSArray() {
    return array_;
  }

  Isolate* isolate() const {
    return array_->GetIsolate();
  }

 protected:
  void SetField(int field_position, Handle<Object> value) {
    Object::SetElement(isolate(), array_, field_position, value,
                       LanguageMode::kSloppy)
        .Assert();
  }

  void SetSmiValueField(int field_position, int value) {
    SetField(field_position, Handle<Smi>(Smi::FromInt(value), isolate()));
  }

  Handle<Object> GetField(int field_position) {
    return JSReceiver::GetElement(isolate(), array_, field_position)
        .ToHandleChecked();
  }

  int GetSmiValueField(int field_position) {
    Handle<Object> res = GetField(field_position);
    return Handle<Smi>::cast(res)->value();
  }

 private:
  Handle<JSArray> array_;
};


// Represents some function compilation details. This structure will be used
// from JavaScript. It contains Code object, which is kept wrapped
// into a BlindReference for sanitizing reasons.
class FunctionInfoWrapper : public JSArrayBasedStruct<FunctionInfoWrapper> {
 public:
  explicit FunctionInfoWrapper(Handle<JSArray> array)
      : JSArrayBasedStruct<FunctionInfoWrapper>(array) {
  }

  void SetInitialProperties(Handle<String> name, int start_position,
                            int end_position, int param_num, int parent_index,
                            int function_literal_id);

  void SetFunctionScopeInfo(Handle<Object> scope_info_array) {
    this->SetField(kFunctionScopeInfoOffset_, scope_info_array);
  }

  void SetSharedFunctionInfo(Handle<SharedFunctionInfo> info);

  Handle<SharedFunctionInfo> GetSharedFunctionInfo();

  int GetParentIndex() {
    return this->GetSmiValueField(kParentIndexOffset_);
  }

  int GetStartPosition() {
    return this->GetSmiValueField(kStartPositionOffset_);
  }

  int GetEndPosition() { return this->GetSmiValueField(kEndPositionOffset_); }

 private:
  static const int kFunctionNameOffset_ = 0;
  static const int kStartPositionOffset_ = 1;
  static const int kEndPositionOffset_ = 2;
  static const int kParamNumOffset_ = 3;
  static const int kFunctionScopeInfoOffset_ = 4;
  static const int kParentIndexOffset_ = 5;
  static const int kSharedFunctionInfoOffset_ = 6;
  static const int kFunctionLiteralIdOffset_ = 7;
  static const int kSize_ = 8;

  friend class JSArrayBasedStruct<FunctionInfoWrapper>;
};


// Wraps SharedFunctionInfo along with some of its fields for passing it
// back to JavaScript. SharedFunctionInfo object itself is additionally
// wrapped into BlindReference for sanitizing reasons.
class SharedInfoWrapper : public JSArrayBasedStruct<SharedInfoWrapper> {
 public:
  static bool IsInstance(Handle<JSArray> array) {
    if (array->length() != Smi::FromInt(kSize_)) return false;
    Handle<Object> element(
        JSReceiver::GetElement(array->GetIsolate(), array, kSharedInfoOffset_)
            .ToHandleChecked());
    if (!element->IsJSValue()) return false;
    return Handle<JSValue>::cast(element)->value()->IsSharedFunctionInfo();
  }

  explicit SharedInfoWrapper(Handle<JSArray> array)
      : JSArrayBasedStruct<SharedInfoWrapper>(array) {
  }

  void SetProperties(Handle<String> name,
                     int start_position,
                     int end_position,
                     Handle<SharedFunctionInfo> info);

  Handle<SharedFunctionInfo> GetInfo();

 private:
  static const int kFunctionNameOffset_ = 0;
  static const int kStartPositionOffset_ = 1;
  static const int kEndPositionOffset_ = 2;
  static const int kSharedInfoOffset_ = 3;
  static const int kSize_ = 4;

  friend class JSArrayBasedStruct<SharedInfoWrapper>;
};

}  // namespace internal
}  // namespace v8

#endif /* V8_DEBUG_LIVEEDIT_H_ */
