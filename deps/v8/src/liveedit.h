// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIVEEDIT_H_
#define V8_LIVEEDIT_H_



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
#include "src/compiler.h"

namespace v8 {
namespace internal {

// This class collects some specific information on structure of functions
// in a particular script. It gets called from compiler all the time, but
// actually records any data only when liveedit operation is in process;
// in any other time this class is very cheap.
//
// The primary interest of the Tracker is to record function scope structures
// in order to analyze whether function code maybe safely patched (with new
// code successfully reading existing data from function scopes). The Tracker
// also collects compiled function codes.
class LiveEditFunctionTracker {
 public:
  explicit LiveEditFunctionTracker(Isolate* isolate, FunctionLiteral* fun);
  ~LiveEditFunctionTracker();
  void RecordFunctionInfo(Handle<SharedFunctionInfo> info,
                          FunctionLiteral* lit, Zone* zone);
  void RecordRootFunctionInfo(Handle<Code> code);

  static bool IsActive(Isolate* isolate);

 private:
  Isolate* isolate_;
};


class LiveEdit : AllStatic {
 public:
  // Describes how exactly a frame has been dropped from stack.
  enum FrameDropMode {
    // No frame has been dropped.
    FRAMES_UNTOUCHED,
    // The top JS frame had been calling IC stub. IC stub mustn't be called now.
    FRAME_DROPPED_IN_IC_CALL,
    // The top JS frame had been calling debug break slot stub. Patch the
    // address this stub jumps to in the end.
    FRAME_DROPPED_IN_DEBUG_SLOT_CALL,
    // The top JS frame had been calling some C++ function. The return address
    // gets patched automatically.
    FRAME_DROPPED_IN_DIRECT_CALL,
    FRAME_DROPPED_IN_RETURN_CALL,
    CURRENTLY_SET_MODE
  };

  static void InitializeThreadLocal(Debug* debug);

  static bool SetAfterBreakTarget(Debug* debug);

  MUST_USE_RESULT static MaybeHandle<JSArray> GatherCompileInfo(
      Handle<Script> script,
      Handle<String> source);

  static void WrapSharedFunctionInfos(Handle<JSArray> array);

  static void ReplaceFunctionCode(Handle<JSArray> new_compile_info_array,
                                  Handle<JSArray> shared_info_array);

  static void FunctionSourceUpdated(Handle<JSArray> shared_info_array);

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
      Handle<JSArray> shared_info_array, bool do_drop);

  // Restarts the call frame and completely drops all frames above it.
  // Return error message or NULL.
  static const char* RestartFrame(JavaScriptFrame* frame);

  // A copy of this is in liveedit-debugger.js.
  enum FunctionPatchabilityStatus {
    FUNCTION_AVAILABLE_FOR_PATCH = 1,
    FUNCTION_BLOCKED_ON_ACTIVE_STACK = 2,
    FUNCTION_BLOCKED_ON_OTHER_STACK = 3,
    FUNCTION_BLOCKED_UNDER_NATIVE_CODE = 4,
    FUNCTION_REPLACED_ON_ACTIVE_STACK = 5,
    FUNCTION_BLOCKED_UNDER_GENERATOR = 6,
    FUNCTION_BLOCKED_ACTIVE_GENERATOR = 7
  };

  // Compares 2 strings line-by-line, then token-wise and returns diff in form
  // of array of triplets (pos1, pos1_end, pos2_end) describing list
  // of diff chunks.
  static Handle<JSArray> CompareStrings(Handle<String> s1,
                                        Handle<String> s2);

  // Architecture-specific constant.
  static const bool kFrameDropperSupported;

  /**
   * Defines layout of a stack frame that supports padding. This is a regular
   * internal frame that has a flexible stack structure. LiveEdit can shift
   * its lower part up the stack, taking up the 'padding' space when additional
   * stack memory is required.
   * Such frame is expected immediately above the topmost JavaScript frame.
   *
   * Stack Layout:
   *   --- Top
   *   LiveEdit routine frames
   *   ---
   *   C frames of debug handler
   *   ---
   *   ...
   *   ---
   *      An internal frame that has n padding words:
   *      - any number of words as needed by code -- upper part of frame
   *      - padding size: a Smi storing n -- current size of padding
   *      - padding: n words filled with kPaddingValue in form of Smi
   *      - 3 context/type words of a regular InternalFrame
   *      - fp
   *   ---
   *      Topmost JavaScript frame
   *   ---
   *   ...
   *   --- Bottom
   */
  // A size of frame base including fp. Padding words starts right above
  // the base.
  static const int kFrameDropperFrameSize = 4;
  // A number of words that should be reserved on stack for the LiveEdit use.
  // Stored on stack in form of Smi.
  static const int kFramePaddingInitialSize = 1;
  // A value that padding words are filled with (in form of Smi). Going
  // bottom-top, the first word not having this value is a counter word.
  static const int kFramePaddingValue = kFramePaddingInitialSize + 1;
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
    JSObject::SetElement(array_, field_position, value, NONE, SLOPPY).Assert();
  }

  void SetSmiValueField(int field_position, int value) {
    SetField(field_position, Handle<Smi>(Smi::FromInt(value), isolate()));
  }

  Handle<Object> GetField(int field_position) {
    return Object::GetElement(
        isolate(), array_, field_position).ToHandleChecked();
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
                            int end_position, int param_num, int literal_count,
                            int slot_count, int ic_slot_count,
                            int parent_index);

  void SetFunctionCode(Handle<Code> function_code,
                       Handle<HeapObject> code_scope_info);

  void SetFunctionScopeInfo(Handle<Object> scope_info_array) {
    this->SetField(kFunctionScopeInfoOffset_, scope_info_array);
  }

  void SetSharedFunctionInfo(Handle<SharedFunctionInfo> info);

  int GetLiteralCount() {
    return this->GetSmiValueField(kLiteralNumOffset_);
  }

  int GetParentIndex() {
    return this->GetSmiValueField(kParentIndexOffset_);
  }

  Handle<Code> GetFunctionCode();

  Handle<TypeFeedbackVector> GetFeedbackVector();

  Handle<Object> GetCodeScopeInfo();

  int GetStartPosition() {
    return this->GetSmiValueField(kStartPositionOffset_);
  }

  int GetEndPosition() { return this->GetSmiValueField(kEndPositionOffset_); }

  int GetSlotCount() {
    return this->GetSmiValueField(kSlotNumOffset_);
  }

  int GetICSlotCount() { return this->GetSmiValueField(kICSlotNumOffset_); }

 private:
  static const int kFunctionNameOffset_ = 0;
  static const int kStartPositionOffset_ = 1;
  static const int kEndPositionOffset_ = 2;
  static const int kParamNumOffset_ = 3;
  static const int kCodeOffset_ = 4;
  static const int kCodeScopeInfoOffset_ = 5;
  static const int kFunctionScopeInfoOffset_ = 6;
  static const int kParentIndexOffset_ = 7;
  static const int kSharedFunctionInfoOffset_ = 8;
  static const int kLiteralNumOffset_ = 9;
  static const int kSlotNumOffset_ = 10;
  static const int kICSlotNumOffset_ = 11;
  static const int kSize_ = 12;

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
        Object::GetElement(array->GetIsolate(),
                           array,
                           kSharedInfoOffset_).ToHandleChecked());
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

} }  // namespace v8::internal

#endif /* V*_LIVEEDIT_H_ */
