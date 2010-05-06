// Copyright 2010 the V8 project authors. All rights reserved.
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


#include "v8.h"

#include "liveedit.h"
#include "compiler.h"
#include "oprofile-agent.h"
#include "scopes.h"
#include "global-handles.h"
#include "debug.h"
#include "memory.h"

namespace v8 {
namespace internal {


#ifdef ENABLE_DEBUGGER_SUPPORT


// A simple implementation of dynamic programming algorithm. It solves
// the problem of finding the difference of 2 arrays. It uses a table of results
// of subproblems. Each cell contains a number together with 2-bit flag
// that helps building the chunk list.
class Differencer {
 public:
  explicit Differencer(Comparator::Input* input)
      : input_(input), len1_(input->getLength1()), len2_(input->getLength2()) {
    buffer_ = NewArray<int>(len1_ * len2_);
  }
  ~Differencer() {
    DeleteArray(buffer_);
  }

  void Initialize() {
    int array_size = len1_ * len2_;
    for (int i = 0; i < array_size; i++) {
      buffer_[i] = kEmptyCellValue;
    }
  }

  // Makes sure that result for the full problem is calculated and stored
  // in the table together with flags showing a path through subproblems.
  void FillTable() {
    CompareUpToTail(0, 0);
  }

  void SaveResult(Comparator::Output* chunk_writer) {
    ResultWriter writer(chunk_writer);

    int pos1 = 0;
    int pos2 = 0;
    while (true) {
      if (pos1 < len1_) {
        if (pos2 < len2_) {
          Direction dir = get_direction(pos1, pos2);
          switch (dir) {
            case EQ:
              writer.eq();
              pos1++;
              pos2++;
              break;
            case SKIP1:
              writer.skip1(1);
              pos1++;
              break;
            case SKIP2:
            case SKIP_ANY:
              writer.skip2(1);
              pos2++;
              break;
            default:
              UNREACHABLE();
          }
        } else {
          writer.skip1(len1_ - pos1);
          break;
        }
      } else {
        if (len2_ != pos2) {
          writer.skip2(len2_ - pos2);
        }
        break;
      }
    }
    writer.close();
  }

 private:
  Comparator::Input* input_;
  int* buffer_;
  int len1_;
  int len2_;

  enum Direction {
    EQ = 0,
    SKIP1,
    SKIP2,
    SKIP_ANY,

    MAX_DIRECTION_FLAG_VALUE = SKIP_ANY
  };

  // Computes result for a subtask and optionally caches it in the buffer table.
  // All results values are shifted to make space for flags in the lower bits.
  int CompareUpToTail(int pos1, int pos2) {
    if (pos1 < len1_) {
      if (pos2 < len2_) {
        int cached_res = get_value4(pos1, pos2);
        if (cached_res == kEmptyCellValue) {
          Direction dir;
          int res;
          if (input_->equals(pos1, pos2)) {
            res = CompareUpToTail(pos1 + 1, pos2 + 1);
            dir = EQ;
          } else {
            int res1 = CompareUpToTail(pos1 + 1, pos2) +
                (1 << kDirectionSizeBits);
            int res2 = CompareUpToTail(pos1, pos2 + 1) +
                (1 << kDirectionSizeBits);
            if (res1 == res2) {
              res = res1;
              dir = SKIP_ANY;
            } else if (res1 < res2) {
              res = res1;
              dir = SKIP1;
            } else {
              res = res2;
              dir = SKIP2;
            }
          }
          set_value4_and_dir(pos1, pos2, res, dir);
          cached_res = res;
        }
        return cached_res;
      } else {
        return (len1_ - pos1) << kDirectionSizeBits;
      }
    } else {
      return (len2_ - pos2) << kDirectionSizeBits;
    }
  }

  inline int& get_cell(int i1, int i2) {
    return buffer_[i1 + i2 * len1_];
  }

  // Each cell keeps a value plus direction. Value is multiplied by 4.
  void set_value4_and_dir(int i1, int i2, int value4, Direction dir) {
    ASSERT((value4 & kDirectionMask) == 0);
    get_cell(i1, i2) = value4 | dir;
  }

  int get_value4(int i1, int i2) {
    return get_cell(i1, i2) & (kMaxUInt32 ^ kDirectionMask);
  }
  Direction get_direction(int i1, int i2) {
    return static_cast<Direction>(get_cell(i1, i2) & kDirectionMask);
  }

  static const int kDirectionSizeBits = 2;
  static const int kDirectionMask = (1 << kDirectionSizeBits) - 1;
  static const int kEmptyCellValue = -1 << kDirectionSizeBits;

  // This method only holds static assert statement (unfortunately you cannot
  // place one in class scope).
  void StaticAssertHolder() {
    STATIC_ASSERT(MAX_DIRECTION_FLAG_VALUE < (1 << kDirectionSizeBits));
  }

  class ResultWriter {
   public:
    explicit ResultWriter(Comparator::Output* chunk_writer)
        : chunk_writer_(chunk_writer), pos1_(0), pos2_(0),
          pos1_begin_(-1), pos2_begin_(-1), has_open_chunk_(false) {
    }
    void eq() {
      FlushChunk();
      pos1_++;
      pos2_++;
    }
    void skip1(int len1) {
      StartChunk();
      pos1_ += len1;
    }
    void skip2(int len2) {
      StartChunk();
      pos2_ += len2;
    }
    void close() {
      FlushChunk();
    }

   private:
    Comparator::Output* chunk_writer_;
    int pos1_;
    int pos2_;
    int pos1_begin_;
    int pos2_begin_;
    bool has_open_chunk_;

    void StartChunk() {
      if (!has_open_chunk_) {
        pos1_begin_ = pos1_;
        pos2_begin_ = pos2_;
        has_open_chunk_ = true;
      }
    }

    void FlushChunk() {
      if (has_open_chunk_) {
        chunk_writer_->AddChunk(pos1_begin_, pos2_begin_,
                                pos1_ - pos1_begin_, pos2_ - pos2_begin_);
        has_open_chunk_ = false;
      }
    }
  };
};


void Comparator::CalculateDifference(Comparator::Input* input,
                                     Comparator::Output* result_writer) {
  Differencer differencer(input);
  differencer.Initialize();
  differencer.FillTable();
  differencer.SaveResult(result_writer);
}


static bool CompareSubstrings(Handle<String> s1, int pos1,
                              Handle<String> s2, int pos2, int len) {
  static StringInputBuffer buf1;
  static StringInputBuffer buf2;
  buf1.Reset(*s1);
  buf1.Seek(pos1);
  buf2.Reset(*s2);
  buf2.Seek(pos2);
  for (int i = 0; i < len; i++) {
    ASSERT(buf1.has_more() && buf2.has_more());
    if (buf1.GetNext() != buf2.GetNext()) {
      return false;
    }
  }
  return true;
}


// Wraps raw n-elements line_ends array as a list of n+1 lines. The last line
// never has terminating new line character.
class LineEndsWrapper {
 public:
  explicit LineEndsWrapper(Handle<String> string)
      : ends_array_(CalculateLineEnds(string, false)),
        string_len_(string->length()) {
  }
  int length() {
    return ends_array_->length() + 1;
  }
  // Returns start for any line including start of the imaginary line after
  // the last line.
  int GetLineStart(int index) {
    if (index == 0) {
      return 0;
    } else {
      return GetLineEnd(index - 1);
    }
  }
  int GetLineEnd(int index) {
    if (index == ends_array_->length()) {
      // End of the last line is always an end of the whole string.
      // If the string ends with a new line character, the last line is an
      // empty string after this character.
      return string_len_;
    } else {
      return GetPosAfterNewLine(index);
    }
  }

 private:
  Handle<FixedArray> ends_array_;
  int string_len_;

  int GetPosAfterNewLine(int index) {
    return Smi::cast(ends_array_->get(index))->value() + 1;
  }
};


// Represents 2 strings as 2 arrays of lines.
class LineArrayCompareInput : public Comparator::Input {
 public:
  LineArrayCompareInput(Handle<String> s1, Handle<String> s2,
                        LineEndsWrapper line_ends1, LineEndsWrapper line_ends2)
      : s1_(s1), s2_(s2), line_ends1_(line_ends1), line_ends2_(line_ends2) {
  }
  int getLength1() {
    return line_ends1_.length();
  }
  int getLength2() {
    return line_ends2_.length();
  }
  bool equals(int index1, int index2) {
    int line_start1 = line_ends1_.GetLineStart(index1);
    int line_start2 = line_ends2_.GetLineStart(index2);
    int line_end1 = line_ends1_.GetLineEnd(index1);
    int line_end2 = line_ends2_.GetLineEnd(index2);
    int len1 = line_end1 - line_start1;
    int len2 = line_end2 - line_start2;
    if (len1 != len2) {
      return false;
    }
    return CompareSubstrings(s1_, line_start1, s2_, line_start2, len1);
  }

 private:
  Handle<String> s1_;
  Handle<String> s2_;
  LineEndsWrapper line_ends1_;
  LineEndsWrapper line_ends2_;
};


// Stores compare result in JSArray. Each chunk is stored as 3 array elements:
// (pos1_begin, pos1_end, pos2_end).
class LineArrayCompareOutput : public Comparator::Output {
 public:
  LineArrayCompareOutput(LineEndsWrapper line_ends1, LineEndsWrapper line_ends2)
      : array_(Factory::NewJSArray(10)), current_size_(0),
        line_ends1_(line_ends1), line_ends2_(line_ends2) {
  }

  void AddChunk(int line_pos1, int line_pos2, int line_len1, int line_len2) {
    int char_pos1 = line_ends1_.GetLineStart(line_pos1);
    int char_pos2 = line_ends2_.GetLineStart(line_pos2);
    int char_len1 = line_ends1_.GetLineStart(line_pos1 + line_len1) - char_pos1;
    int char_len2 = line_ends2_.GetLineStart(line_pos2 + line_len2) - char_pos2;

    SetElement(array_, current_size_, Handle<Object>(Smi::FromInt(char_pos1)));
    SetElement(array_, current_size_ + 1,
               Handle<Object>(Smi::FromInt(char_pos1 + char_len1)));
    SetElement(array_, current_size_ + 2,
               Handle<Object>(Smi::FromInt(char_pos2 + char_len2)));
    current_size_ += 3;
  }

  Handle<JSArray> GetResult() {
    return array_;
  }

 private:
  Handle<JSArray> array_;
  int current_size_;
  LineEndsWrapper line_ends1_;
  LineEndsWrapper line_ends2_;
};


Handle<JSArray> LiveEdit::CompareStringsLinewise(Handle<String> s1,
                                                 Handle<String> s2) {
  LineEndsWrapper line_ends1(s1);
  LineEndsWrapper line_ends2(s2);

  LineArrayCompareInput input(s1, s2, line_ends1, line_ends2);
  LineArrayCompareOutput output(line_ends1, line_ends2);

  Comparator::CalculateDifference(&input, &output);

  return output.GetResult();
}


static void CompileScriptForTracker(Handle<Script> script) {
  const bool is_eval = false;
  const bool is_global = true;
  // TODO(635): support extensions.
  Extension* extension = NULL;

  PostponeInterruptsScope postpone;

  // Only allow non-global compiles for eval.
  ASSERT(is_eval || is_global);

  // Build AST.
  ScriptDataImpl* pre_data = NULL;
  FunctionLiteral* lit = MakeAST(is_global, script, extension, pre_data);

  // Check for parse errors.
  if (lit == NULL) {
    ASSERT(Top::has_pending_exception());
    return;
  }

  // Compile the code.
  CompilationInfo info(lit, script, is_eval);

  LiveEditFunctionTracker tracker(lit);
  Handle<Code> code = MakeCodeForLiveEdit(&info);

  // Check for stack-overflow exceptions.
  if (code.is_null()) {
    Top::StackOverflow();
    return;
  }
  tracker.RecordRootFunctionInfo(code);
}

// Unwraps JSValue object, returning its field "value"
static Handle<Object> UnwrapJSValue(Handle<JSValue> jsValue) {
  return Handle<Object>(jsValue->value());
}

// Wraps any object into a OpaqueReference, that will hide the object
// from JavaScript.
static Handle<JSValue> WrapInJSValue(Object* object) {
  Handle<JSFunction> constructor = Top::opaque_reference_function();
  Handle<JSValue> result =
      Handle<JSValue>::cast(Factory::NewJSObject(constructor));
  result->set_value(object);
  return result;
}

// Simple helper class that creates more or less typed structures over
// JSArray object. This is an adhoc method of passing structures from C++
// to JavaScript.
template<typename S>
class JSArrayBasedStruct {
 public:
  static S Create() {
    Handle<JSArray> array = Factory::NewJSArray(S::kSize_);
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
 protected:
  void SetField(int field_position, Handle<Object> value) {
    SetElement(array_, field_position, value);
  }
  void SetSmiValueField(int field_position, int value) {
    SetElement(array_, field_position, Handle<Smi>(Smi::FromInt(value)));
  }
  Object* GetField(int field_position) {
    return array_->GetElement(field_position);
  }
  int GetSmiValueField(int field_position) {
    Object* res = GetField(field_position);
    return Smi::cast(res)->value();
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
                            int end_position, int param_num, int parent_index) {
    HandleScope scope;
    this->SetField(kFunctionNameOffset_, name);
    this->SetSmiValueField(kStartPositionOffset_, start_position);
    this->SetSmiValueField(kEndPositionOffset_, end_position);
    this->SetSmiValueField(kParamNumOffset_, param_num);
    this->SetSmiValueField(kParentIndexOffset_, parent_index);
  }
  void SetFunctionCode(Handle<Code> function_code) {
    Handle<JSValue> wrapper = WrapInJSValue(*function_code);
    this->SetField(kCodeOffset_, wrapper);
  }
  void SetScopeInfo(Handle<Object> scope_info_array) {
    this->SetField(kScopeInfoOffset_, scope_info_array);
  }
  void SetSharedFunctionInfo(Handle<SharedFunctionInfo> info) {
    Handle<JSValue> info_holder = WrapInJSValue(*info);
    this->SetField(kSharedFunctionInfoOffset_, info_holder);
  }
  int GetParentIndex() {
    return this->GetSmiValueField(kParentIndexOffset_);
  }
  Handle<Code> GetFunctionCode() {
    Handle<Object> raw_result = UnwrapJSValue(Handle<JSValue>(
        JSValue::cast(this->GetField(kCodeOffset_))));
    return Handle<Code>::cast(raw_result);
  }
  int GetStartPosition() {
    return this->GetSmiValueField(kStartPositionOffset_);
  }
  int GetEndPosition() {
    return this->GetSmiValueField(kEndPositionOffset_);
  }

 private:
  static const int kFunctionNameOffset_ = 0;
  static const int kStartPositionOffset_ = 1;
  static const int kEndPositionOffset_ = 2;
  static const int kParamNumOffset_ = 3;
  static const int kCodeOffset_ = 4;
  static const int kScopeInfoOffset_ = 5;
  static const int kParentIndexOffset_ = 6;
  static const int kSharedFunctionInfoOffset_ = 7;
  static const int kSize_ = 8;

  friend class JSArrayBasedStruct<FunctionInfoWrapper>;
};

// Wraps SharedFunctionInfo along with some of its fields for passing it
// back to JavaScript. SharedFunctionInfo object itself is additionally
// wrapped into BlindReference for sanitizing reasons.
class SharedInfoWrapper : public JSArrayBasedStruct<SharedInfoWrapper> {
 public:
  static bool IsInstance(Handle<JSArray> array) {
    return array->length() == Smi::FromInt(kSize_) &&
        array->GetElement(kSharedInfoOffset_)->IsJSValue();
  }

  explicit SharedInfoWrapper(Handle<JSArray> array)
      : JSArrayBasedStruct<SharedInfoWrapper>(array) {
  }

  void SetProperties(Handle<String> name, int start_position, int end_position,
                     Handle<SharedFunctionInfo> info) {
    HandleScope scope;
    this->SetField(kFunctionNameOffset_, name);
    Handle<JSValue> info_holder = WrapInJSValue(*info);
    this->SetField(kSharedInfoOffset_, info_holder);
    this->SetSmiValueField(kStartPositionOffset_, start_position);
    this->SetSmiValueField(kEndPositionOffset_, end_position);
  }
  Handle<SharedFunctionInfo> GetInfo() {
    Object* element = this->GetField(kSharedInfoOffset_);
    Handle<JSValue> value_wrapper(JSValue::cast(element));
    Handle<Object> raw_result = UnwrapJSValue(value_wrapper);
    return Handle<SharedFunctionInfo>::cast(raw_result);
  }

 private:
  static const int kFunctionNameOffset_ = 0;
  static const int kStartPositionOffset_ = 1;
  static const int kEndPositionOffset_ = 2;
  static const int kSharedInfoOffset_ = 3;
  static const int kSize_ = 4;

  friend class JSArrayBasedStruct<SharedInfoWrapper>;
};

class FunctionInfoListener {
 public:
  FunctionInfoListener() {
    current_parent_index_ = -1;
    len_ = 0;
    result_ = Factory::NewJSArray(10);
  }

  void FunctionStarted(FunctionLiteral* fun) {
    HandleScope scope;
    FunctionInfoWrapper info = FunctionInfoWrapper::Create();
    info.SetInitialProperties(fun->name(), fun->start_position(),
                              fun->end_position(), fun->num_parameters(),
                              current_parent_index_);
    current_parent_index_ = len_;
    SetElement(result_, len_, info.GetJSArray());
    len_++;
  }

  void FunctionDone() {
    HandleScope scope;
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(result_->GetElement(current_parent_index_));
    current_parent_index_ = info.GetParentIndex();
  }

// TODO(LiveEdit): Move private method below.
//     This private section was created here to avoid moving the function
//      to keep already complex diff simpler.
 private:
  Object* SerializeFunctionScope(Scope* scope) {
    HandleScope handle_scope;

    Handle<JSArray> scope_info_list = Factory::NewJSArray(10);
    int scope_info_length = 0;

    // Saves some description of scope. It stores name and indexes of
    // variables in the whole scope chain. Null-named slots delimit
    // scopes of this chain.
    Scope* outer_scope = scope->outer_scope();
    if (outer_scope == NULL) {
      return Heap::undefined_value();
    }
    do {
      ZoneList<Variable*> list(10);
      outer_scope->CollectUsedVariables(&list);
      int j = 0;
      for (int i = 0; i < list.length(); i++) {
        Variable* var1 = list[i];
        Slot* slot = var1->slot();
        if (slot != NULL && slot->type() == Slot::CONTEXT) {
          if (j != i) {
            list[j] = var1;
          }
          j++;
        }
      }

      // Sort it.
      for (int k = 1; k < j; k++) {
        int l = k;
        for (int m = k + 1; m < j; m++) {
          if (list[l]->slot()->index() > list[m]->slot()->index()) {
            l = m;
          }
        }
        list[k] = list[l];
      }
      for (int i = 0; i < j; i++) {
        SetElement(scope_info_list, scope_info_length, list[i]->name());
        scope_info_length++;
        SetElement(scope_info_list, scope_info_length,
                   Handle<Smi>(Smi::FromInt(list[i]->slot()->index())));
        scope_info_length++;
      }
      SetElement(scope_info_list, scope_info_length,
                 Handle<Object>(Heap::null_value()));
      scope_info_length++;

      outer_scope = outer_scope->outer_scope();
    } while (outer_scope != NULL);

    return *scope_info_list;
  }

 public:
  // Saves only function code, because for a script function we
  // may never create a SharedFunctionInfo object.
  void FunctionCode(Handle<Code> function_code) {
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(result_->GetElement(current_parent_index_));
    info.SetFunctionCode(function_code);
  }

  // Saves full information about a function: its code, its scope info
  // and a SharedFunctionInfo object.
  void FunctionInfo(Handle<SharedFunctionInfo> shared, Scope* scope) {
    if (!shared->IsSharedFunctionInfo()) {
      return;
    }
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(result_->GetElement(current_parent_index_));
    info.SetFunctionCode(Handle<Code>(shared->code()));
    info.SetSharedFunctionInfo(shared);

    Handle<Object> scope_info_list(SerializeFunctionScope(scope));
    info.SetScopeInfo(scope_info_list);
  }

  Handle<JSArray> GetResult() {
    return result_;
  }

 private:
  Handle<JSArray> result_;
  int len_;
  int current_parent_index_;
};

static FunctionInfoListener* active_function_info_listener = NULL;

JSArray* LiveEdit::GatherCompileInfo(Handle<Script> script,
                                     Handle<String> source) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  FunctionInfoListener listener;
  Handle<Object> original_source = Handle<Object>(script->source());
  script->set_source(*source);
  active_function_info_listener = &listener;
  CompileScriptForTracker(script);
  active_function_info_listener = NULL;
  script->set_source(*original_source);

  return *(listener.GetResult());
}


void LiveEdit::WrapSharedFunctionInfos(Handle<JSArray> array) {
  HandleScope scope;
  int len = Smi::cast(array->length())->value();
  for (int i = 0; i < len; i++) {
    Handle<SharedFunctionInfo> info(
        SharedFunctionInfo::cast(array->GetElement(i)));
    SharedInfoWrapper info_wrapper = SharedInfoWrapper::Create();
    Handle<String> name_handle(String::cast(info->name()));
    info_wrapper.SetProperties(name_handle, info->start_position(),
                               info->end_position(), info);
    array->SetElement(i, *(info_wrapper.GetJSArray()));
  }
}


// Visitor that collects all references to a particular code object,
// including "CODE_TARGET" references in other code objects.
// It works in context of ZoneScope.
class ReferenceCollectorVisitor : public ObjectVisitor {
 public:
  explicit ReferenceCollectorVisitor(Code* original)
      : original_(original), rvalues_(10), reloc_infos_(10) {
  }

  virtual void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) {
      if (*p == original_) {
        rvalues_.Add(p);
      }
    }
  }

  void VisitCodeTarget(RelocInfo* rinfo) {
    if (RelocInfo::IsCodeTarget(rinfo->rmode()) &&
        Code::GetCodeFromTargetAddress(rinfo->target_address()) == original_) {
      reloc_infos_.Add(*rinfo);
    }
  }

  virtual void VisitDebugTarget(RelocInfo* rinfo) {
    VisitCodeTarget(rinfo);
  }

  // Post-visiting method that iterates over all collected references and
  // modifies them.
  void Replace(Code* substitution) {
    for (int i = 0; i < rvalues_.length(); i++) {
      *(rvalues_[i]) = substitution;
    }
    for (int i = 0; i < reloc_infos_.length(); i++) {
      reloc_infos_[i].set_target_address(substitution->instruction_start());
    }
  }

 private:
  Code* original_;
  ZoneList<Object**> rvalues_;
  ZoneList<RelocInfo> reloc_infos_;
};


class FrameCookingThreadVisitor : public ThreadVisitor {
 public:
  void VisitThread(ThreadLocalTop* top) {
    StackFrame::CookFramesForThread(top);
  }
};

class FrameUncookingThreadVisitor : public ThreadVisitor {
 public:
  void VisitThread(ThreadLocalTop* top) {
    StackFrame::UncookFramesForThread(top);
  }
};

static void IterateAllThreads(ThreadVisitor* visitor) {
  Top::IterateThread(visitor);
  ThreadManager::IterateThreads(visitor);
}

// Finds all references to original and replaces them with substitution.
static void ReplaceCodeObject(Code* original, Code* substitution) {
  ASSERT(!Heap::InNewSpace(substitution));

  AssertNoAllocation no_allocations_please;

  // A zone scope for ReferenceCollectorVisitor.
  ZoneScope scope(DELETE_ON_EXIT);

  ReferenceCollectorVisitor visitor(original);

  // Iterate over all roots. Stack frames may have pointer into original code,
  // so temporary replace the pointers with offset numbers
  // in prologue/epilogue.
  {
    FrameCookingThreadVisitor cooking_visitor;
    IterateAllThreads(&cooking_visitor);

    Heap::IterateStrongRoots(&visitor, VISIT_ALL);

    FrameUncookingThreadVisitor uncooking_visitor;
    IterateAllThreads(&uncooking_visitor);
  }

  // Now iterate over all pointers of all objects, including code_target
  // implicit pointers.
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    obj->Iterate(&visitor);
  }

  visitor.Replace(substitution);
}


// Check whether the code is natural function code (not a lazy-compile stub
// code).
static bool IsJSFunctionCode(Code* code) {
  return code->kind() == Code::FUNCTION;
}


Object* LiveEdit::ReplaceFunctionCode(Handle<JSArray> new_compile_info_array,
                                      Handle<JSArray> shared_info_array) {
  HandleScope scope;

  if (!SharedInfoWrapper::IsInstance(shared_info_array)) {
    return Top::ThrowIllegalOperation();
  }

  FunctionInfoWrapper compile_info_wrapper(new_compile_info_array);
  SharedInfoWrapper shared_info_wrapper(shared_info_array);

  Handle<SharedFunctionInfo> shared_info = shared_info_wrapper.GetInfo();

  if (IsJSFunctionCode(shared_info->code())) {
    ReplaceCodeObject(shared_info->code(),
                      *(compile_info_wrapper.GetFunctionCode()));
  }

  if (shared_info->debug_info()->IsDebugInfo()) {
    Handle<DebugInfo> debug_info(DebugInfo::cast(shared_info->debug_info()));
    Handle<Code> new_original_code =
        Factory::CopyCode(compile_info_wrapper.GetFunctionCode());
    debug_info->set_original_code(*new_original_code);
  }

  shared_info->set_start_position(compile_info_wrapper.GetStartPosition());
  shared_info->set_end_position(compile_info_wrapper.GetEndPosition());

  shared_info->set_construct_stub(
      Builtins::builtin(Builtins::JSConstructStubGeneric));

  return Heap::undefined_value();
}


// TODO(635): Eval caches its scripts (same text -- same compiled info).
// Make sure we clear such caches.
void LiveEdit::SetFunctionScript(Handle<JSValue> function_wrapper,
                                 Handle<Object> script_handle) {
  Handle<SharedFunctionInfo> shared_info =
      Handle<SharedFunctionInfo>::cast(UnwrapJSValue(function_wrapper));
  shared_info->set_script(*script_handle);
}


// For a script text change (defined as position_change_array), translates
// position in unchanged text to position in changed text.
// Text change is a set of non-overlapping regions in text, that have changed
// their contents and length. It is specified as array of groups of 3 numbers:
// (change_begin, change_end, change_end_new_position).
// Each group describes a change in text; groups are sorted by change_begin.
// Only position in text beyond any changes may be successfully translated.
// If a positions is inside some region that changed, result is currently
// undefined.
static int TranslatePosition(int original_position,
                             Handle<JSArray> position_change_array) {
  int position_diff = 0;
  int array_len = Smi::cast(position_change_array->length())->value();
  // TODO(635): binary search may be used here
  for (int i = 0; i < array_len; i += 3) {
    int chunk_start =
        Smi::cast(position_change_array->GetElement(i))->value();
    if (original_position < chunk_start) {
      break;
    }
    int chunk_end =
        Smi::cast(position_change_array->GetElement(i + 1))->value();
    // Position mustn't be inside a chunk.
    ASSERT(original_position >= chunk_end);
    int chunk_changed_end =
        Smi::cast(position_change_array->GetElement(i + 2))->value();
    position_diff = chunk_changed_end - chunk_end;
  }

  return original_position + position_diff;
}


// Auto-growing buffer for writing relocation info code section. This buffer
// is a simplified version of buffer from Assembler. Unlike Assembler, this
// class is platform-independent and it works without dealing with instructions.
// As specified by RelocInfo format, the buffer is filled in reversed order:
// from upper to lower addresses.
// It uses NewArray/DeleteArray for memory management.
class RelocInfoBuffer {
 public:
  RelocInfoBuffer(int buffer_initial_capicity, byte* pc) {
    buffer_size_ = buffer_initial_capicity + kBufferGap;
    buffer_ = NewArray<byte>(buffer_size_);

    reloc_info_writer_.Reposition(buffer_ + buffer_size_, pc);
  }
  ~RelocInfoBuffer() {
    DeleteArray(buffer_);
  }

  // As specified by RelocInfo format, the buffer is filled in reversed order:
  // from upper to lower addresses.
  void Write(const RelocInfo* rinfo) {
    if (buffer_ + kBufferGap >= reloc_info_writer_.pos()) {
      Grow();
    }
    reloc_info_writer_.Write(rinfo);
  }

  Vector<byte> GetResult() {
    // Return the bytes from pos up to end of buffer.
    int result_size =
        static_cast<int>((buffer_ + buffer_size_) - reloc_info_writer_.pos());
    return Vector<byte>(reloc_info_writer_.pos(), result_size);
  }

 private:
  void Grow() {
    // Compute new buffer size.
    int new_buffer_size;
    if (buffer_size_ < 2 * KB) {
      new_buffer_size = 4 * KB;
    } else {
      new_buffer_size = 2 * buffer_size_;
    }
    // Some internal data structures overflow for very large buffers,
    // they must ensure that kMaximalBufferSize is not too large.
    if (new_buffer_size > kMaximalBufferSize) {
      V8::FatalProcessOutOfMemory("RelocInfoBuffer::GrowBuffer");
    }

    // Setup new buffer.
    byte* new_buffer = NewArray<byte>(new_buffer_size);

    // Copy the data.
    int curently_used_size =
        static_cast<int>(buffer_ + buffer_size_ - reloc_info_writer_.pos());
    memmove(new_buffer + new_buffer_size - curently_used_size,
            reloc_info_writer_.pos(), curently_used_size);

    reloc_info_writer_.Reposition(
        new_buffer + new_buffer_size - curently_used_size,
        reloc_info_writer_.last_pc());

    DeleteArray(buffer_);
    buffer_ = new_buffer;
    buffer_size_ = new_buffer_size;
  }

  RelocInfoWriter reloc_info_writer_;
  byte* buffer_;
  int buffer_size_;

  static const int kBufferGap = 8;
  static const int kMaximalBufferSize = 512*MB;
};

// Patch positions in code (changes relocation info section) and possibly
// returns new instance of code.
static Handle<Code> PatchPositionsInCode(Handle<Code> code,
    Handle<JSArray> position_change_array) {

  RelocInfoBuffer buffer_writer(code->relocation_size(),
                                code->instruction_start());

  {
    AssertNoAllocation no_allocations_please;
    for (RelocIterator it(*code); !it.done(); it.next()) {
      RelocInfo* rinfo = it.rinfo();
      if (RelocInfo::IsPosition(rinfo->rmode())) {
        int position = static_cast<int>(rinfo->data());
        int new_position = TranslatePosition(position,
                                             position_change_array);
        if (position != new_position) {
          RelocInfo info_copy(rinfo->pc(), rinfo->rmode(), new_position);
          buffer_writer.Write(&info_copy);
          continue;
        }
      }
      buffer_writer.Write(it.rinfo());
    }
  }

  Vector<byte> buffer = buffer_writer.GetResult();

  if (buffer.length() == code->relocation_size()) {
    // Simply patch relocation area of code.
    memcpy(code->relocation_start(), buffer.start(), buffer.length());
    return code;
  } else {
    // Relocation info section now has different size. We cannot simply
    // rewrite it inside code object. Instead we have to create a new
    // code object.
    Handle<Code> result(Factory::CopyCode(code, buffer));
    return result;
  }
}


Object* LiveEdit::PatchFunctionPositions(
    Handle<JSArray> shared_info_array, Handle<JSArray> position_change_array) {

  if (!SharedInfoWrapper::IsInstance(shared_info_array)) {
    return Top::ThrowIllegalOperation();
  }

  SharedInfoWrapper shared_info_wrapper(shared_info_array);
  Handle<SharedFunctionInfo> info = shared_info_wrapper.GetInfo();

  int old_function_start = info->start_position();
  int new_function_start = TranslatePosition(old_function_start,
                                             position_change_array);
  info->set_start_position(new_function_start);
  info->set_end_position(TranslatePosition(info->end_position(),
                                           position_change_array));

  info->set_function_token_position(
      TranslatePosition(info->function_token_position(),
      position_change_array));

  if (IsJSFunctionCode(info->code())) {
    // Patch relocation info section of the code.
    Handle<Code> patched_code = PatchPositionsInCode(Handle<Code>(info->code()),
                                                     position_change_array);
    if (*patched_code != info->code()) {
      // Replace all references to the code across the heap. In particular,
      // some stubs may refer to this code and this code may be being executed
      // on stack (it is safe to substitute the code object on stack, because
      // we only change the structure of rinfo and leave instructions
      // untouched).
      ReplaceCodeObject(info->code(), *patched_code);
    }
  }

  return Heap::undefined_value();
}


static Handle<Script> CreateScriptCopy(Handle<Script> original) {
  Handle<String> original_source(String::cast(original->source()));

  Handle<Script> copy = Factory::NewScript(original_source);

  copy->set_name(original->name());
  copy->set_line_offset(original->line_offset());
  copy->set_column_offset(original->column_offset());
  copy->set_data(original->data());
  copy->set_type(original->type());
  copy->set_context_data(original->context_data());
  copy->set_compilation_type(original->compilation_type());
  copy->set_eval_from_shared(original->eval_from_shared());
  copy->set_eval_from_instructions_offset(
      original->eval_from_instructions_offset());

  return copy;
}


Object* LiveEdit::ChangeScriptSource(Handle<Script> original_script,
                                     Handle<String> new_source,
                                     Handle<Object> old_script_name) {
  Handle<Object> old_script_object;
  if (old_script_name->IsString()) {
    Handle<Script> old_script = CreateScriptCopy(original_script);
    old_script->set_name(String::cast(*old_script_name));
    old_script_object = old_script;
    Debugger::OnAfterCompile(old_script, Debugger::SEND_WHEN_DEBUGGING);
  } else {
    old_script_object = Handle<Object>(Heap::null_value());
  }

  original_script->set_source(*new_source);

  // Drop line ends so that they will be recalculated.
  original_script->set_line_ends(Heap::undefined_value());

  return *old_script_object;
}



void LiveEdit::ReplaceRefToNestedFunction(
    Handle<JSValue> parent_function_wrapper,
    Handle<JSValue> orig_function_wrapper,
    Handle<JSValue> subst_function_wrapper) {

  Handle<SharedFunctionInfo> parent_shared =
      Handle<SharedFunctionInfo>::cast(UnwrapJSValue(parent_function_wrapper));
  Handle<SharedFunctionInfo> orig_shared =
      Handle<SharedFunctionInfo>::cast(UnwrapJSValue(orig_function_wrapper));
  Handle<SharedFunctionInfo> subst_shared =
      Handle<SharedFunctionInfo>::cast(UnwrapJSValue(subst_function_wrapper));

  for (RelocIterator it(parent_shared->code()); !it.done(); it.next()) {
    if (it.rinfo()->rmode() == RelocInfo::EMBEDDED_OBJECT) {
      if (it.rinfo()->target_object() == *orig_shared) {
        it.rinfo()->set_target_object(*subst_shared);
      }
    }
  }
}


// Check an activation against list of functions. If there is a function
// that matches, its status in result array is changed to status argument value.
static bool CheckActivation(Handle<JSArray> shared_info_array,
                            Handle<JSArray> result, StackFrame* frame,
                            LiveEdit::FunctionPatchabilityStatus status) {
  if (!frame->is_java_script()) {
    return false;
  }
  int len = Smi::cast(shared_info_array->length())->value();
  for (int i = 0; i < len; i++) {
    JSValue* wrapper = JSValue::cast(shared_info_array->GetElement(i));
    Handle<SharedFunctionInfo> shared(
        SharedFunctionInfo::cast(wrapper->value()));

    if (frame->code() == shared->code()) {
      SetElement(result, i, Handle<Smi>(Smi::FromInt(status)));
      return true;
    }
  }
  return false;
}


// Iterates over handler chain and removes all elements that are inside
// frames being dropped.
static bool FixTryCatchHandler(StackFrame* top_frame,
                               StackFrame* bottom_frame) {
  Address* pointer_address =
      &Memory::Address_at(Top::get_address_from_id(Top::k_handler_address));

  while (*pointer_address < top_frame->sp()) {
    pointer_address = &Memory::Address_at(*pointer_address);
  }
  Address* above_frame_address = pointer_address;
  while (*pointer_address < bottom_frame->fp()) {
    pointer_address = &Memory::Address_at(*pointer_address);
  }
  bool change = *above_frame_address != *pointer_address;
  *above_frame_address = *pointer_address;
  return change;
}


// Removes specified range of frames from stack. There may be 1 or more
// frames in range. Anyway the bottom frame is restarted rather than dropped,
// and therefore has to be a JavaScript frame.
// Returns error message or NULL.
static const char* DropFrames(Vector<StackFrame*> frames,
                              int top_frame_index,
                              int bottom_js_frame_index) {
  StackFrame* pre_top_frame = frames[top_frame_index - 1];
  StackFrame* top_frame = frames[top_frame_index];
  StackFrame* bottom_js_frame = frames[bottom_js_frame_index];

  ASSERT(bottom_js_frame->is_java_script());

  // Check the nature of the top frame.
  if (pre_top_frame->code()->is_inline_cache_stub() &&
      pre_top_frame->code()->ic_state() == DEBUG_BREAK) {
    // OK, we can drop inline cache calls.
  } else if (pre_top_frame->code() ==
      Builtins::builtin(Builtins::FrameDropper_LiveEdit)) {
    // OK, we can drop our own code.
  } else if (pre_top_frame->code()->kind() == Code::STUB &&
      pre_top_frame->code()->major_key()) {
    // Unit Test entry, it's fine, we support this case.
  } else {
    return "Unknown structure of stack above changing function";
  }

  Address unused_stack_top = top_frame->sp();
  Address unused_stack_bottom = bottom_js_frame->fp()
      - Debug::kFrameDropperFrameSize * kPointerSize  // Size of the new frame.
      + kPointerSize;  // Bigger address end is exclusive.

  if (unused_stack_top > unused_stack_bottom) {
    return "Not enough space for frame dropper frame";
  }

  // Committing now. After this point we should return only NULL value.

  FixTryCatchHandler(pre_top_frame, bottom_js_frame);
  // Make sure FixTryCatchHandler is idempotent.
  ASSERT(!FixTryCatchHandler(pre_top_frame, bottom_js_frame));

  Handle<Code> code(Builtins::builtin(Builtins::FrameDropper_LiveEdit));
  top_frame->set_pc(code->entry());
  pre_top_frame->SetCallerFp(bottom_js_frame->fp());

  Debug::SetUpFrameDropperFrame(bottom_js_frame, code);

  for (Address a = unused_stack_top;
      a < unused_stack_bottom;
      a += kPointerSize) {
    Memory::Object_at(a) = Smi::FromInt(0);
  }

  return NULL;
}


static bool IsDropableFrame(StackFrame* frame) {
  return !frame->is_exit();
}

// Fills result array with statuses of functions. Modifies the stack
// removing all listed function if possible and if do_drop is true.
static const char* DropActivationsInActiveThread(
    Handle<JSArray> shared_info_array, Handle<JSArray> result, bool do_drop) {

  ZoneScope scope(DELETE_ON_EXIT);
  Vector<StackFrame*> frames = CreateStackMap();

  int array_len = Smi::cast(shared_info_array->length())->value();

  int top_frame_index = -1;
  int frame_index = 0;
  for (; frame_index < frames.length(); frame_index++) {
    StackFrame* frame = frames[frame_index];
    if (frame->id() == Debug::break_frame_id()) {
      top_frame_index = frame_index;
      break;
    }
    if (CheckActivation(shared_info_array, result, frame,
                        LiveEdit::FUNCTION_BLOCKED_UNDER_NATIVE_CODE)) {
      // We are still above break_frame. It is not a target frame,
      // it is a problem.
      return "Debugger mark-up on stack is not found";
    }
  }

  if (top_frame_index == -1) {
    // We haven't found break frame, but no function is blocking us anyway.
    return NULL;
  }

  bool target_frame_found = false;
  int bottom_js_frame_index = top_frame_index;
  bool c_code_found = false;

  for (; frame_index < frames.length(); frame_index++) {
    StackFrame* frame = frames[frame_index];
    if (!IsDropableFrame(frame)) {
      c_code_found = true;
      break;
    }
    if (CheckActivation(shared_info_array, result, frame,
                        LiveEdit::FUNCTION_BLOCKED_ON_ACTIVE_STACK)) {
      target_frame_found = true;
      bottom_js_frame_index = frame_index;
    }
  }

  if (c_code_found) {
    // There is a C frames on stack. Check that there are no target frames
    // below them.
    for (; frame_index < frames.length(); frame_index++) {
      StackFrame* frame = frames[frame_index];
      if (frame->is_java_script()) {
        if (CheckActivation(shared_info_array, result, frame,
                            LiveEdit::FUNCTION_BLOCKED_UNDER_NATIVE_CODE)) {
          // Cannot drop frame under C frames.
          return NULL;
        }
      }
    }
  }

  if (!do_drop) {
    // We are in check-only mode.
    return NULL;
  }

  if (!target_frame_found) {
    // Nothing to drop.
    return NULL;
  }

  const char* error_message = DropFrames(frames, top_frame_index,
                                         bottom_js_frame_index);

  if (error_message != NULL) {
    return error_message;
  }

  // Adjust break_frame after some frames has been dropped.
  StackFrame::Id new_id = StackFrame::NO_ID;
  for (int i = bottom_js_frame_index + 1; i < frames.length(); i++) {
    if (frames[i]->type() == StackFrame::JAVA_SCRIPT) {
      new_id = frames[i]->id();
      break;
    }
  }
  Debug::FramesHaveBeenDropped(new_id);

  // Replace "blocked on active" with "replaced on active" status.
  for (int i = 0; i < array_len; i++) {
    if (result->GetElement(i) ==
        Smi::FromInt(LiveEdit::FUNCTION_BLOCKED_ON_ACTIVE_STACK)) {
      result->SetElement(i, Smi::FromInt(
          LiveEdit::FUNCTION_REPLACED_ON_ACTIVE_STACK));
    }
  }
  return NULL;
}


class InactiveThreadActivationsChecker : public ThreadVisitor {
 public:
  InactiveThreadActivationsChecker(Handle<JSArray> shared_info_array,
                                   Handle<JSArray> result)
      : shared_info_array_(shared_info_array), result_(result),
        has_blocked_functions_(false) {
  }
  void VisitThread(ThreadLocalTop* top) {
    for (StackFrameIterator it(top); !it.done(); it.Advance()) {
      has_blocked_functions_ |= CheckActivation(
          shared_info_array_, result_, it.frame(),
          LiveEdit::FUNCTION_BLOCKED_ON_OTHER_STACK);
    }
  }
  bool HasBlockedFunctions() {
    return has_blocked_functions_;
  }

 private:
  Handle<JSArray> shared_info_array_;
  Handle<JSArray> result_;
  bool has_blocked_functions_;
};


Handle<JSArray> LiveEdit::CheckAndDropActivations(
    Handle<JSArray> shared_info_array, bool do_drop) {
  int len = Smi::cast(shared_info_array->length())->value();

  Handle<JSArray> result = Factory::NewJSArray(len);

  // Fill the default values.
  for (int i = 0; i < len; i++) {
    SetElement(result, i,
               Handle<Smi>(Smi::FromInt(FUNCTION_AVAILABLE_FOR_PATCH)));
  }


  // First check inactive threads. Fail if some functions are blocked there.
  InactiveThreadActivationsChecker inactive_threads_checker(shared_info_array,
                                                            result);
  ThreadManager::IterateThreads(&inactive_threads_checker);
  if (inactive_threads_checker.HasBlockedFunctions()) {
    return result;
  }

  // Try to drop activations from the current stack.
  const char* error_message =
      DropActivationsInActiveThread(shared_info_array, result, do_drop);
  if (error_message != NULL) {
    // Add error message as an array extra element.
    Vector<const char> vector_message(error_message, StrLength(error_message));
    Handle<String> str = Factory::NewStringFromAscii(vector_message);
    SetElement(result, len, str);
  }
  return result;
}


LiveEditFunctionTracker::LiveEditFunctionTracker(FunctionLiteral* fun) {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionStarted(fun);
  }
}


LiveEditFunctionTracker::~LiveEditFunctionTracker() {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionDone();
  }
}


void LiveEditFunctionTracker::RecordFunctionInfo(
    Handle<SharedFunctionInfo> info, FunctionLiteral* lit) {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionInfo(info, lit->scope());
  }
}


void LiveEditFunctionTracker::RecordRootFunctionInfo(Handle<Code> code) {
  active_function_info_listener->FunctionCode(code);
}


bool LiveEditFunctionTracker::IsActive() {
  return active_function_info_listener != NULL;
}


#else  // ENABLE_DEBUGGER_SUPPORT

// This ifdef-else-endif section provides working or stub implementation of
// LiveEditFunctionTracker.
LiveEditFunctionTracker::LiveEditFunctionTracker(FunctionLiteral* fun) {
}


LiveEditFunctionTracker::~LiveEditFunctionTracker() {
}


void LiveEditFunctionTracker::RecordFunctionInfo(
    Handle<SharedFunctionInfo> info, FunctionLiteral* lit) {
}


void LiveEditFunctionTracker::RecordRootFunctionInfo(Handle<Code> code) {
}


bool LiveEditFunctionTracker::IsActive() {
  return false;
}

#endif  // ENABLE_DEBUGGER_SUPPORT



} }  // namespace v8::internal
