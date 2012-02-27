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


#include "v8.h"

#include "liveedit.h"

#include "compilation-cache.h"
#include "compiler.h"
#include "debug.h"
#include "deoptimizer.h"
#include "global-handles.h"
#include "parser.h"
#include "scopeinfo.h"
#include "scopes.h"
#include "v8memory.h"

namespace v8 {
namespace internal {


#ifdef ENABLE_DEBUGGER_SUPPORT


void SetElementNonStrict(Handle<JSObject> object,
                         uint32_t index,
                         Handle<Object> value) {
  // Ignore return value from SetElement. It can only be a failure if there
  // are element setters causing exceptions and the debugger context has none
  // of these.
  Handle<Object> no_failure =
      JSObject::SetElement(object, index, value, NONE, kNonStrictMode);
  ASSERT(!no_failure.is_null());
  USE(no_failure);
}

// A simple implementation of dynamic programming algorithm. It solves
// the problem of finding the difference of 2 arrays. It uses a table of results
// of subproblems. Each cell contains a number together with 2-bit flag
// that helps building the chunk list.
class Differencer {
 public:
  explicit Differencer(Comparator::Input* input)
      : input_(input), len1_(input->GetLength1()), len2_(input->GetLength2()) {
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
          if (input_->Equals(pos1, pos2)) {
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
  for (int i = 0; i < len; i++) {
    if (s1->Get(i + pos1) != s2->Get(i + pos2)) {
      return false;
    }
  }
  return true;
}


// Additional to Input interface. Lets switch Input range to subrange.
// More elegant way would be to wrap one Input as another Input object
// and translate positions there, but that would cost us additional virtual
// call per comparison.
class SubrangableInput : public Comparator::Input {
 public:
  virtual void SetSubrange1(int offset, int len) = 0;
  virtual void SetSubrange2(int offset, int len) = 0;
};


class SubrangableOutput : public Comparator::Output {
 public:
  virtual void SetSubrange1(int offset, int len) = 0;
  virtual void SetSubrange2(int offset, int len) = 0;
};


static int min(int a, int b) {
  return a < b ? a : b;
}


// Finds common prefix and suffix in input. This parts shouldn't take space in
// linear programming table. Enable subranging in input and output.
static void NarrowDownInput(SubrangableInput* input,
    SubrangableOutput* output) {
  const int len1 = input->GetLength1();
  const int len2 = input->GetLength2();

  int common_prefix_len;
  int common_suffix_len;

  {
    common_prefix_len = 0;
    int prefix_limit = min(len1, len2);
    while (common_prefix_len < prefix_limit &&
        input->Equals(common_prefix_len, common_prefix_len)) {
      common_prefix_len++;
    }

    common_suffix_len = 0;
    int suffix_limit = min(len1 - common_prefix_len, len2 - common_prefix_len);

    while (common_suffix_len < suffix_limit &&
        input->Equals(len1 - common_suffix_len - 1,
        len2 - common_suffix_len - 1)) {
      common_suffix_len++;
    }
  }

  if (common_prefix_len > 0 || common_suffix_len > 0) {
    int new_len1 = len1 - common_suffix_len - common_prefix_len;
    int new_len2 = len2 - common_suffix_len - common_prefix_len;

    input->SetSubrange1(common_prefix_len, new_len1);
    input->SetSubrange2(common_prefix_len, new_len2);

    output->SetSubrange1(common_prefix_len, new_len1);
    output->SetSubrange2(common_prefix_len, new_len2);
  }
}


// A helper class that writes chunk numbers into JSArray.
// Each chunk is stored as 3 array elements: (pos1_begin, pos1_end, pos2_end).
class CompareOutputArrayWriter {
 public:
  CompareOutputArrayWriter()
      : array_(FACTORY->NewJSArray(10)), current_size_(0) {}

  Handle<JSArray> GetResult() {
    return array_;
  }

  void WriteChunk(int char_pos1, int char_pos2, int char_len1, int char_len2) {
    SetElementNonStrict(array_,
                       current_size_,
                       Handle<Object>(Smi::FromInt(char_pos1)));
    SetElementNonStrict(array_,
                        current_size_ + 1,
                        Handle<Object>(Smi::FromInt(char_pos1 + char_len1)));
    SetElementNonStrict(array_,
                        current_size_ + 2,
                        Handle<Object>(Smi::FromInt(char_pos2 + char_len2)));
    current_size_ += 3;
  }

 private:
  Handle<JSArray> array_;
  int current_size_;
};


// Represents 2 strings as 2 arrays of tokens.
// TODO(LiveEdit): Currently it's actually an array of charactres.
//     Make array of tokens instead.
class TokensCompareInput : public Comparator::Input {
 public:
  TokensCompareInput(Handle<String> s1, int offset1, int len1,
                       Handle<String> s2, int offset2, int len2)
      : s1_(s1), offset1_(offset1), len1_(len1),
        s2_(s2), offset2_(offset2), len2_(len2) {
  }
  virtual int GetLength1() {
    return len1_;
  }
  virtual int GetLength2() {
    return len2_;
  }
  bool Equals(int index1, int index2) {
    return s1_->Get(offset1_ + index1) == s2_->Get(offset2_ + index2);
  }

 private:
  Handle<String> s1_;
  int offset1_;
  int len1_;
  Handle<String> s2_;
  int offset2_;
  int len2_;
};


// Stores compare result in JSArray. Converts substring positions
// to absolute positions.
class TokensCompareOutput : public Comparator::Output {
 public:
  TokensCompareOutput(CompareOutputArrayWriter* array_writer,
                      int offset1, int offset2)
        : array_writer_(array_writer), offset1_(offset1), offset2_(offset2) {
  }

  void AddChunk(int pos1, int pos2, int len1, int len2) {
    array_writer_->WriteChunk(pos1 + offset1_, pos2 + offset2_, len1, len2);
  }

 private:
  CompareOutputArrayWriter* array_writer_;
  int offset1_;
  int offset2_;
};


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
class LineArrayCompareInput : public SubrangableInput {
 public:
  LineArrayCompareInput(Handle<String> s1, Handle<String> s2,
                        LineEndsWrapper line_ends1, LineEndsWrapper line_ends2)
      : s1_(s1), s2_(s2), line_ends1_(line_ends1),
        line_ends2_(line_ends2),
        subrange_offset1_(0), subrange_offset2_(0),
        subrange_len1_(line_ends1_.length()),
        subrange_len2_(line_ends2_.length()) {
  }
  int GetLength1() {
    return subrange_len1_;
  }
  int GetLength2() {
    return subrange_len2_;
  }
  bool Equals(int index1, int index2) {
    index1 += subrange_offset1_;
    index2 += subrange_offset2_;

    int line_start1 = line_ends1_.GetLineStart(index1);
    int line_start2 = line_ends2_.GetLineStart(index2);
    int line_end1 = line_ends1_.GetLineEnd(index1);
    int line_end2 = line_ends2_.GetLineEnd(index2);
    int len1 = line_end1 - line_start1;
    int len2 = line_end2 - line_start2;
    if (len1 != len2) {
      return false;
    }
    return CompareSubstrings(s1_, line_start1, s2_, line_start2,
                             len1);
  }
  void SetSubrange1(int offset, int len) {
    subrange_offset1_ = offset;
    subrange_len1_ = len;
  }
  void SetSubrange2(int offset, int len) {
    subrange_offset2_ = offset;
    subrange_len2_ = len;
  }

 private:
  Handle<String> s1_;
  Handle<String> s2_;
  LineEndsWrapper line_ends1_;
  LineEndsWrapper line_ends2_;
  int subrange_offset1_;
  int subrange_offset2_;
  int subrange_len1_;
  int subrange_len2_;
};


// Stores compare result in JSArray. For each chunk tries to conduct
// a fine-grained nested diff token-wise.
class TokenizingLineArrayCompareOutput : public SubrangableOutput {
 public:
  TokenizingLineArrayCompareOutput(LineEndsWrapper line_ends1,
                                   LineEndsWrapper line_ends2,
                                   Handle<String> s1, Handle<String> s2)
      : line_ends1_(line_ends1), line_ends2_(line_ends2), s1_(s1), s2_(s2),
        subrange_offset1_(0), subrange_offset2_(0) {
  }

  void AddChunk(int line_pos1, int line_pos2, int line_len1, int line_len2) {
    line_pos1 += subrange_offset1_;
    line_pos2 += subrange_offset2_;

    int char_pos1 = line_ends1_.GetLineStart(line_pos1);
    int char_pos2 = line_ends2_.GetLineStart(line_pos2);
    int char_len1 = line_ends1_.GetLineStart(line_pos1 + line_len1) - char_pos1;
    int char_len2 = line_ends2_.GetLineStart(line_pos2 + line_len2) - char_pos2;

    if (char_len1 < CHUNK_LEN_LIMIT && char_len2 < CHUNK_LEN_LIMIT) {
      // Chunk is small enough to conduct a nested token-level diff.
      HandleScope subTaskScope;

      TokensCompareInput tokens_input(s1_, char_pos1, char_len1,
                                      s2_, char_pos2, char_len2);
      TokensCompareOutput tokens_output(&array_writer_, char_pos1,
                                          char_pos2);

      Comparator::CalculateDifference(&tokens_input, &tokens_output);
    } else {
      array_writer_.WriteChunk(char_pos1, char_pos2, char_len1, char_len2);
    }
  }
  void SetSubrange1(int offset, int len) {
    subrange_offset1_ = offset;
  }
  void SetSubrange2(int offset, int len) {
    subrange_offset2_ = offset;
  }

  Handle<JSArray> GetResult() {
    return array_writer_.GetResult();
  }

 private:
  static const int CHUNK_LEN_LIMIT = 800;

  CompareOutputArrayWriter array_writer_;
  LineEndsWrapper line_ends1_;
  LineEndsWrapper line_ends2_;
  Handle<String> s1_;
  Handle<String> s2_;
  int subrange_offset1_;
  int subrange_offset2_;
};


Handle<JSArray> LiveEdit::CompareStrings(Handle<String> s1,
                                         Handle<String> s2) {
  s1 = FlattenGetString(s1);
  s2 = FlattenGetString(s2);

  LineEndsWrapper line_ends1(s1);
  LineEndsWrapper line_ends2(s2);

  LineArrayCompareInput input(s1, s2, line_ends1, line_ends2);
  TokenizingLineArrayCompareOutput output(line_ends1, line_ends2, s1, s2);

  NarrowDownInput(&input, &output);

  Comparator::CalculateDifference(&input, &output);

  return output.GetResult();
}


static void CompileScriptForTracker(Isolate* isolate, Handle<Script> script) {
  // TODO(635): support extensions.
  PostponeInterruptsScope postpone(isolate);

  // Build AST.
  CompilationInfo info(script);
  info.MarkAsGlobal();
  // Parse and don't allow skipping lazy functions.
  if (ParserApi::Parse(&info, kNoParsingFlags)) {
    // Compile the code.
    LiveEditFunctionTracker tracker(info.isolate(), info.function());
    if (Compiler::MakeCodeForLiveEdit(&info)) {
      ASSERT(!info.code().is_null());
      tracker.RecordRootFunctionInfo(info.code());
    } else {
      info.isolate()->StackOverflow();
    }
  }
}


// Unwraps JSValue object, returning its field "value"
static Handle<Object> UnwrapJSValue(Handle<JSValue> jsValue) {
  return Handle<Object>(jsValue->value());
}


// Wraps any object into a OpaqueReference, that will hide the object
// from JavaScript.
static Handle<JSValue> WrapInJSValue(Handle<Object> object) {
  Handle<JSFunction> constructor =
      Isolate::Current()->opaque_reference_function();
  Handle<JSValue> result =
      Handle<JSValue>::cast(FACTORY->NewJSObject(constructor));
  result->set_value(*object);
  return result;
}


// Simple helper class that creates more or less typed structures over
// JSArray object. This is an adhoc method of passing structures from C++
// to JavaScript.
template<typename S>
class JSArrayBasedStruct {
 public:
  static S Create() {
    Handle<JSArray> array = FACTORY->NewJSArray(S::kSize_);
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
    SetElementNonStrict(array_, field_position, value);
  }
  void SetSmiValueField(int field_position, int value) {
    SetElementNonStrict(array_,
                        field_position,
                        Handle<Smi>(Smi::FromInt(value)));
  }
  Object* GetField(int field_position) {
    return array_->GetElementNoExceptionThrown(field_position);
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
  void SetFunctionCode(Handle<Code> function_code,
      Handle<Object> code_scope_info) {
    Handle<JSValue> code_wrapper = WrapInJSValue(function_code);
    this->SetField(kCodeOffset_, code_wrapper);

    Handle<JSValue> scope_wrapper = WrapInJSValue(code_scope_info);
    this->SetField(kCodeScopeInfoOffset_, scope_wrapper);
  }
  void SetOuterScopeInfo(Handle<Object> scope_info_array) {
    this->SetField(kOuterScopeInfoOffset_, scope_info_array);
  }
  void SetSharedFunctionInfo(Handle<SharedFunctionInfo> info) {
    Handle<JSValue> info_holder = WrapInJSValue(info);
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
  Handle<Object> GetCodeScopeInfo() {
    Handle<Object> raw_result = UnwrapJSValue(Handle<JSValue>(
        JSValue::cast(this->GetField(kCodeScopeInfoOffset_))));
    return raw_result;
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
  static const int kCodeScopeInfoOffset_ = 5;
  static const int kOuterScopeInfoOffset_ = 6;
  static const int kParentIndexOffset_ = 7;
  static const int kSharedFunctionInfoOffset_ = 8;
  static const int kSize_ = 9;

  friend class JSArrayBasedStruct<FunctionInfoWrapper>;
};


// Wraps SharedFunctionInfo along with some of its fields for passing it
// back to JavaScript. SharedFunctionInfo object itself is additionally
// wrapped into BlindReference for sanitizing reasons.
class SharedInfoWrapper : public JSArrayBasedStruct<SharedInfoWrapper> {
 public:
  static bool IsInstance(Handle<JSArray> array) {
    return array->length() == Smi::FromInt(kSize_) &&
        array->GetElementNoExceptionThrown(kSharedInfoOffset_)->IsJSValue();
  }

  explicit SharedInfoWrapper(Handle<JSArray> array)
      : JSArrayBasedStruct<SharedInfoWrapper>(array) {
  }

  void SetProperties(Handle<String> name, int start_position, int end_position,
                     Handle<SharedFunctionInfo> info) {
    HandleScope scope;
    this->SetField(kFunctionNameOffset_, name);
    Handle<JSValue> info_holder = WrapInJSValue(info);
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
    result_ = FACTORY->NewJSArray(10);
  }

  void FunctionStarted(FunctionLiteral* fun) {
    HandleScope scope;
    FunctionInfoWrapper info = FunctionInfoWrapper::Create();
    info.SetInitialProperties(fun->name(), fun->start_position(),
                              fun->end_position(), fun->parameter_count(),
                              current_parent_index_);
    current_parent_index_ = len_;
    SetElementNonStrict(result_, len_, info.GetJSArray());
    len_++;
  }

  void FunctionDone() {
    HandleScope scope;
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(
            result_->GetElementNoExceptionThrown(current_parent_index_));
    current_parent_index_ = info.GetParentIndex();
  }

  // Saves only function code, because for a script function we
  // may never create a SharedFunctionInfo object.
  void FunctionCode(Handle<Code> function_code) {
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(
            result_->GetElementNoExceptionThrown(current_parent_index_));
    info.SetFunctionCode(function_code, Handle<Object>(HEAP->null_value()));
  }

  // Saves full information about a function: its code, its scope info
  // and a SharedFunctionInfo object.
  void FunctionInfo(Handle<SharedFunctionInfo> shared, Scope* scope) {
    if (!shared->IsSharedFunctionInfo()) {
      return;
    }
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(
            result_->GetElementNoExceptionThrown(current_parent_index_));
    info.SetFunctionCode(Handle<Code>(shared->code()),
        Handle<Object>(shared->scope_info()));
    info.SetSharedFunctionInfo(shared);

    Handle<Object> scope_info_list(SerializeFunctionScope(scope));
    info.SetOuterScopeInfo(scope_info_list);
  }

  Handle<JSArray> GetResult() { return result_; }

 private:
  Object* SerializeFunctionScope(Scope* scope) {
    HandleScope handle_scope;

    Handle<JSArray> scope_info_list = FACTORY->NewJSArray(10);
    int scope_info_length = 0;

    // Saves some description of scope. It stores name and indexes of
    // variables in the whole scope chain. Null-named slots delimit
    // scopes of this chain.
    Scope* outer_scope = scope->outer_scope();
    if (outer_scope == NULL) {
      return HEAP->undefined_value();
    }
    do {
      ZoneList<Variable*> stack_list(outer_scope->StackLocalCount());
      ZoneList<Variable*> context_list(outer_scope->ContextLocalCount());
      outer_scope->CollectStackAndContextLocals(&stack_list, &context_list);
      context_list.Sort(&Variable::CompareIndex);

      for (int i = 0; i < context_list.length(); i++) {
        SetElementNonStrict(scope_info_list,
                            scope_info_length,
                            context_list[i]->name());
        scope_info_length++;
        SetElementNonStrict(
            scope_info_list,
            scope_info_length,
            Handle<Smi>(Smi::FromInt(context_list[i]->index())));
        scope_info_length++;
      }
      SetElementNonStrict(scope_info_list,
                          scope_info_length,
                          Handle<Object>(HEAP->null_value()));
      scope_info_length++;

      outer_scope = outer_scope->outer_scope();
    } while (outer_scope != NULL);

    return *scope_info_list;
  }

  Handle<JSArray> result_;
  int len_;
  int current_parent_index_;
};


JSArray* LiveEdit::GatherCompileInfo(Handle<Script> script,
                                     Handle<String> source) {
  Isolate* isolate = Isolate::Current();
  ZoneScope zone_scope(isolate, DELETE_ON_EXIT);

  FunctionInfoListener listener;
  Handle<Object> original_source = Handle<Object>(script->source());
  script->set_source(*source);
  isolate->set_active_function_info_listener(&listener);
  CompileScriptForTracker(isolate, script);
  isolate->set_active_function_info_listener(NULL);
  script->set_source(*original_source);

  return *(listener.GetResult());
}


void LiveEdit::WrapSharedFunctionInfos(Handle<JSArray> array) {
  HandleScope scope;
  int len = Smi::cast(array->length())->value();
  for (int i = 0; i < len; i++) {
    Handle<SharedFunctionInfo> info(
        SharedFunctionInfo::cast(array->GetElementNoExceptionThrown(i)));
    SharedInfoWrapper info_wrapper = SharedInfoWrapper::Create();
    Handle<String> name_handle(String::cast(info->name()));
    info_wrapper.SetProperties(name_handle, info->start_position(),
                               info->end_position(), info);
    SetElementNonStrict(array, i, info_wrapper.GetJSArray());
  }
}


// Visitor that collects all references to a particular code object,
// including "CODE_TARGET" references in other code objects.
// It works in context of ZoneScope.
class ReferenceCollectorVisitor : public ObjectVisitor {
 public:
  explicit ReferenceCollectorVisitor(Code* original)
    : original_(original), rvalues_(10), reloc_infos_(10), code_entries_(10) {
  }

  virtual void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) {
      if (*p == original_) {
        rvalues_.Add(p);
      }
    }
  }

  virtual void VisitCodeEntry(Address entry) {
    if (Code::GetObjectFromEntryAddress(entry) == original_) {
      code_entries_.Add(entry);
    }
  }

  virtual void VisitCodeTarget(RelocInfo* rinfo) {
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
    Address substitution_entry = substitution->instruction_start();
    for (int i = 0; i < reloc_infos_.length(); i++) {
      reloc_infos_[i].set_target_address(substitution_entry);
    }
    for (int i = 0; i < code_entries_.length(); i++) {
      Address entry = code_entries_[i];
      Memory::Address_at(entry) = substitution_entry;
    }
  }

 private:
  Code* original_;
  ZoneList<Object**> rvalues_;
  ZoneList<RelocInfo> reloc_infos_;
  ZoneList<Address> code_entries_;
};


// Finds all references to original and replaces them with substitution.
static void ReplaceCodeObject(Code* original, Code* substitution) {
  ASSERT(!HEAP->InNewSpace(substitution));

  HeapIterator iterator;
  AssertNoAllocation no_allocations_please;

  // A zone scope for ReferenceCollectorVisitor.
  ZoneScope scope(Isolate::Current(), DELETE_ON_EXIT);

  ReferenceCollectorVisitor visitor(original);

  // Iterate over all roots. Stack frames may have pointer into original code,
  // so temporary replace the pointers with offset numbers
  // in prologue/epilogue.
  {
    HEAP->IterateStrongRoots(&visitor, VISIT_ALL);
  }

  // Now iterate over all pointers of all objects, including code_target
  // implicit pointers.
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


// Returns true if an instance of candidate were inlined into function's code.
static bool IsInlined(JSFunction* function, SharedFunctionInfo* candidate) {
  AssertNoAllocation no_gc;

  if (function->code()->kind() != Code::OPTIMIZED_FUNCTION) return false;

  DeoptimizationInputData* data =
      DeoptimizationInputData::cast(function->code()->deoptimization_data());

  if (data == HEAP->empty_fixed_array()) return false;

  FixedArray* literals = data->LiteralArray();

  int inlined_count = data->InlinedFunctionCount()->value();
  for (int i = 0; i < inlined_count; ++i) {
    JSFunction* inlined = JSFunction::cast(literals->get(i));
    if (inlined->shared() == candidate) return true;
  }

  return false;
}


class DependentFunctionsDeoptimizingVisitor : public OptimizedFunctionVisitor {
 public:
  explicit DependentFunctionsDeoptimizingVisitor(
      SharedFunctionInfo* function_info)
      : function_info_(function_info) {}

  virtual void EnterContext(Context* context) {
  }

  virtual void VisitFunction(JSFunction* function) {
    if (function->shared() == function_info_ ||
        IsInlined(function, function_info_)) {
      Deoptimizer::DeoptimizeFunction(function);
    }
  }

  virtual void LeaveContext(Context* context) {
  }

 private:
  SharedFunctionInfo* function_info_;
};


static void DeoptimizeDependentFunctions(SharedFunctionInfo* function_info) {
  AssertNoAllocation no_allocation;

  DependentFunctionsDeoptimizingVisitor visitor(function_info);
  Deoptimizer::VisitAllOptimizedFunctions(&visitor);
}


MaybeObject* LiveEdit::ReplaceFunctionCode(
    Handle<JSArray> new_compile_info_array,
    Handle<JSArray> shared_info_array) {
  HandleScope scope;

  if (!SharedInfoWrapper::IsInstance(shared_info_array)) {
    return Isolate::Current()->ThrowIllegalOperation();
  }

  FunctionInfoWrapper compile_info_wrapper(new_compile_info_array);
  SharedInfoWrapper shared_info_wrapper(shared_info_array);

  Handle<SharedFunctionInfo> shared_info = shared_info_wrapper.GetInfo();

  HEAP->EnsureHeapIsIterable();

  if (IsJSFunctionCode(shared_info->code())) {
    Handle<Code> code = compile_info_wrapper.GetFunctionCode();
    ReplaceCodeObject(shared_info->code(), *code);
    Handle<Object> code_scope_info =  compile_info_wrapper.GetCodeScopeInfo();
    if (code_scope_info->IsFixedArray()) {
      shared_info->set_scope_info(ScopeInfo::cast(*code_scope_info));
    }
  }

  if (shared_info->debug_info()->IsDebugInfo()) {
    Handle<DebugInfo> debug_info(DebugInfo::cast(shared_info->debug_info()));
    Handle<Code> new_original_code =
        FACTORY->CopyCode(compile_info_wrapper.GetFunctionCode());
    debug_info->set_original_code(*new_original_code);
  }

  int start_position = compile_info_wrapper.GetStartPosition();
  int end_position = compile_info_wrapper.GetEndPosition();
  shared_info->set_start_position(start_position);
  shared_info->set_end_position(end_position);

  shared_info->set_construct_stub(
      Isolate::Current()->builtins()->builtin(
          Builtins::kJSConstructStubGeneric));

  DeoptimizeDependentFunctions(*shared_info);
  Isolate::Current()->compilation_cache()->Remove(shared_info);

  return HEAP->undefined_value();
}


MaybeObject* LiveEdit::FunctionSourceUpdated(
    Handle<JSArray> shared_info_array) {
  HandleScope scope;

  if (!SharedInfoWrapper::IsInstance(shared_info_array)) {
    return Isolate::Current()->ThrowIllegalOperation();
  }

  SharedInfoWrapper shared_info_wrapper(shared_info_array);
  Handle<SharedFunctionInfo> shared_info = shared_info_wrapper.GetInfo();

  DeoptimizeDependentFunctions(*shared_info);
  Isolate::Current()->compilation_cache()->Remove(shared_info);

  return HEAP->undefined_value();
}


void LiveEdit::SetFunctionScript(Handle<JSValue> function_wrapper,
                                 Handle<Object> script_handle) {
  Handle<SharedFunctionInfo> shared_info =
      Handle<SharedFunctionInfo>::cast(UnwrapJSValue(function_wrapper));
  shared_info->set_script(*script_handle);

  Isolate::Current()->compilation_cache()->Remove(shared_info);
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
    Object* element = position_change_array->GetElementNoExceptionThrown(i);
    int chunk_start = Smi::cast(element)->value();
    if (original_position < chunk_start) {
      break;
    }
    element = position_change_array->GetElementNoExceptionThrown(i + 1);
    int chunk_end = Smi::cast(element)->value();
    // Position mustn't be inside a chunk.
    ASSERT(original_position >= chunk_end);
    element = position_change_array->GetElementNoExceptionThrown(i + 2);
    int chunk_changed_end = Smi::cast(element)->value();
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

    // Set up new buffer.
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

  static const int kBufferGap = RelocInfoWriter::kMaxSize;
  static const int kMaximalBufferSize = 512*MB;
};

// Patch positions in code (changes relocation info section) and possibly
// returns new instance of code.
static Handle<Code> PatchPositionsInCode(
    Handle<Code> code,
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
          RelocInfo info_copy(rinfo->pc(), rinfo->rmode(), new_position, NULL);
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
    Handle<Code> result(FACTORY->CopyCode(code, buffer));
    return result;
  }
}


MaybeObject* LiveEdit::PatchFunctionPositions(
    Handle<JSArray> shared_info_array, Handle<JSArray> position_change_array) {

  if (!SharedInfoWrapper::IsInstance(shared_info_array)) {
    return Isolate::Current()->ThrowIllegalOperation();
  }

  SharedInfoWrapper shared_info_wrapper(shared_info_array);
  Handle<SharedFunctionInfo> info = shared_info_wrapper.GetInfo();

  int old_function_start = info->start_position();
  int new_function_start = TranslatePosition(old_function_start,
                                             position_change_array);
  int new_function_end = TranslatePosition(info->end_position(),
                                           position_change_array);
  int new_function_token_pos =
      TranslatePosition(info->function_token_position(), position_change_array);

  info->set_start_position(new_function_start);
  info->set_end_position(new_function_end);
  info->set_function_token_position(new_function_token_pos);

  HEAP->EnsureHeapIsIterable();

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

  return HEAP->undefined_value();
}


static Handle<Script> CreateScriptCopy(Handle<Script> original) {
  Handle<String> original_source(String::cast(original->source()));

  Handle<Script> copy = FACTORY->NewScript(original_source);

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
    Isolate::Current()->debugger()->OnAfterCompile(
        old_script, Debugger::SEND_WHEN_DEBUGGING);
  } else {
    old_script_object = Handle<Object>(HEAP->null_value());
  }

  original_script->set_source(*new_source);

  // Drop line ends so that they will be recalculated.
  original_script->set_line_ends(HEAP->undefined_value());

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
                            Handle<JSArray> result,
                            StackFrame* frame,
                            LiveEdit::FunctionPatchabilityStatus status) {
  if (!frame->is_java_script()) return false;

  Handle<JSFunction> function(
      JSFunction::cast(JavaScriptFrame::cast(frame)->function()));

  int len = Smi::cast(shared_info_array->length())->value();
  for (int i = 0; i < len; i++) {
    JSValue* wrapper =
        JSValue::cast(shared_info_array->GetElementNoExceptionThrown(i));
    Handle<SharedFunctionInfo> shared(
        SharedFunctionInfo::cast(wrapper->value()));

    if (function->shared() == *shared || IsInlined(*function, *shared)) {
      SetElementNonStrict(result, i, Handle<Smi>(Smi::FromInt(status)));
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
      &Memory::Address_at(Isolate::Current()->get_address_from_id(
          Isolate::kHandlerAddress));

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
                              int bottom_js_frame_index,
                              Debug::FrameDropMode* mode,
                              Object*** restarter_frame_function_pointer) {
  if (!Debug::kFrameDropperSupported) {
    return "Stack manipulations are not supported in this architecture.";
  }

  StackFrame* pre_top_frame = frames[top_frame_index - 1];
  StackFrame* top_frame = frames[top_frame_index];
  StackFrame* bottom_js_frame = frames[bottom_js_frame_index];

  ASSERT(bottom_js_frame->is_java_script());

  // Check the nature of the top frame.
  Isolate* isolate = Isolate::Current();
  Code* pre_top_frame_code = pre_top_frame->LookupCode();
  if (pre_top_frame_code->is_inline_cache_stub() &&
      pre_top_frame_code->ic_state() == DEBUG_BREAK) {
    // OK, we can drop inline cache calls.
    *mode = Debug::FRAME_DROPPED_IN_IC_CALL;
  } else if (pre_top_frame_code ==
             isolate->debug()->debug_break_slot()) {
    // OK, we can drop debug break slot.
    *mode = Debug::FRAME_DROPPED_IN_DEBUG_SLOT_CALL;
  } else if (pre_top_frame_code ==
      isolate->builtins()->builtin(
          Builtins::kFrameDropper_LiveEdit)) {
    // OK, we can drop our own code.
    *mode = Debug::FRAME_DROPPED_IN_DIRECT_CALL;
  } else if (pre_top_frame_code ==
      isolate->builtins()->builtin(Builtins::kReturn_DebugBreak)) {
    *mode = Debug::FRAME_DROPPED_IN_RETURN_CALL;
  } else if (pre_top_frame_code->kind() == Code::STUB &&
      pre_top_frame_code->major_key()) {
    // Entry from our unit tests, it's fine, we support this case.
    *mode = Debug::FRAME_DROPPED_IN_DIRECT_CALL;
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

  Handle<Code> code = Isolate::Current()->builtins()->FrameDropper_LiveEdit();
  top_frame->set_pc(code->entry());
  pre_top_frame->SetCallerFp(bottom_js_frame->fp());

  *restarter_frame_function_pointer =
      Debug::SetUpFrameDropperFrame(bottom_js_frame, code);

  ASSERT((**restarter_frame_function_pointer)->IsJSFunction());

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
  Isolate* isolate = Isolate::Current();
  Debug* debug = isolate->debug();
  ZoneScope scope(isolate, DELETE_ON_EXIT);
  Vector<StackFrame*> frames = CreateStackMap();

  int array_len = Smi::cast(shared_info_array->length())->value();

  int top_frame_index = -1;
  int frame_index = 0;
  for (; frame_index < frames.length(); frame_index++) {
    StackFrame* frame = frames[frame_index];
    if (frame->id() == debug->break_frame_id()) {
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

  Debug::FrameDropMode drop_mode = Debug::FRAMES_UNTOUCHED;
  Object** restarter_frame_function_pointer = NULL;
  const char* error_message = DropFrames(frames, top_frame_index,
                                         bottom_js_frame_index, &drop_mode,
                                         &restarter_frame_function_pointer);

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
  debug->FramesHaveBeenDropped(new_id, drop_mode,
                               restarter_frame_function_pointer);

  // Replace "blocked on active" with "replaced on active" status.
  for (int i = 0; i < array_len; i++) {
    if (result->GetElement(i) ==
        Smi::FromInt(LiveEdit::FUNCTION_BLOCKED_ON_ACTIVE_STACK)) {
      Handle<Object> replaced(
          Smi::FromInt(LiveEdit::FUNCTION_REPLACED_ON_ACTIVE_STACK));
      SetElementNonStrict(result, i, replaced);
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
  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    for (StackFrameIterator it(isolate, top); !it.done(); it.Advance()) {
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

  Handle<JSArray> result = FACTORY->NewJSArray(len);

  // Fill the default values.
  for (int i = 0; i < len; i++) {
    SetElementNonStrict(
        result,
        i,
        Handle<Smi>(Smi::FromInt(FUNCTION_AVAILABLE_FOR_PATCH)));
  }


  // First check inactive threads. Fail if some functions are blocked there.
  InactiveThreadActivationsChecker inactive_threads_checker(shared_info_array,
                                                            result);
  Isolate::Current()->thread_manager()->IterateArchivedThreads(
      &inactive_threads_checker);
  if (inactive_threads_checker.HasBlockedFunctions()) {
    return result;
  }

  // Try to drop activations from the current stack.
  const char* error_message =
      DropActivationsInActiveThread(shared_info_array, result, do_drop);
  if (error_message != NULL) {
    // Add error message as an array extra element.
    Vector<const char> vector_message(error_message, StrLength(error_message));
    Handle<String> str = FACTORY->NewStringFromAscii(vector_message);
    SetElementNonStrict(result, len, str);
  }
  return result;
}


LiveEditFunctionTracker::LiveEditFunctionTracker(Isolate* isolate,
                                                 FunctionLiteral* fun)
    : isolate_(isolate) {
  if (isolate_->active_function_info_listener() != NULL) {
    isolate_->active_function_info_listener()->FunctionStarted(fun);
  }
}


LiveEditFunctionTracker::~LiveEditFunctionTracker() {
  if (isolate_->active_function_info_listener() != NULL) {
    isolate_->active_function_info_listener()->FunctionDone();
  }
}


void LiveEditFunctionTracker::RecordFunctionInfo(
    Handle<SharedFunctionInfo> info, FunctionLiteral* lit) {
  if (isolate_->active_function_info_listener() != NULL) {
    isolate_->active_function_info_listener()->FunctionInfo(info, lit->scope());
  }
}


void LiveEditFunctionTracker::RecordRootFunctionInfo(Handle<Code> code) {
  isolate_->active_function_info_listener()->FunctionCode(code);
}


bool LiveEditFunctionTracker::IsActive(Isolate* isolate) {
  return isolate->active_function_info_listener() != NULL;
}


#else  // ENABLE_DEBUGGER_SUPPORT

// This ifdef-else-endif section provides working or stub implementation of
// LiveEditFunctionTracker.
LiveEditFunctionTracker::LiveEditFunctionTracker(Isolate* isolate,
                                                 FunctionLiteral* fun) {
}


LiveEditFunctionTracker::~LiveEditFunctionTracker() {
}


void LiveEditFunctionTracker::RecordFunctionInfo(
    Handle<SharedFunctionInfo> info, FunctionLiteral* lit) {
}


void LiveEditFunctionTracker::RecordRootFunctionInfo(Handle<Code> code) {
}


bool LiveEditFunctionTracker::IsActive(Isolate* isolate) {
  return false;
}

#endif  // ENABLE_DEBUGGER_SUPPORT



} }  // namespace v8::internal
