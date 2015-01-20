// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "src/v8.h"

#include "src/liveedit.h"

#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/compiler.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/global-handles.h"
#include "src/messages.h"
#include "src/parser.h"
#include "src/scopeinfo.h"
#include "src/scopes.h"
#include "src/v8memory.h"

namespace v8 {
namespace internal {

void SetElementSloppy(Handle<JSObject> object,
                      uint32_t index,
                      Handle<Object> value) {
  // Ignore return value from SetElement. It can only be a failure if there
  // are element setters causing exceptions and the debugger context has none
  // of these.
  JSObject::SetElement(object, index, value, NONE, SLOPPY).Assert();
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
    DCHECK((value4 & kDirectionMask) == 0);
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
  static const int kEmptyCellValue = ~0u << kDirectionSizeBits;

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
  explicit CompareOutputArrayWriter(Isolate* isolate)
      : array_(isolate->factory()->NewJSArray(10)), current_size_(0) {}

  Handle<JSArray> GetResult() {
    return array_;
  }

  void WriteChunk(int char_pos1, int char_pos2, int char_len1, int char_len2) {
    Isolate* isolate = array_->GetIsolate();
    SetElementSloppy(array_,
                     current_size_,
                     Handle<Object>(Smi::FromInt(char_pos1), isolate));
    SetElementSloppy(array_,
                     current_size_ + 1,
                     Handle<Object>(Smi::FromInt(char_pos1 + char_len1),
                                    isolate));
    SetElementSloppy(array_,
                     current_size_ + 2,
                     Handle<Object>(Smi::FromInt(char_pos2 + char_len2),
                                    isolate));
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
      : ends_array_(String::CalculateLineEnds(string, false)),
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
      : array_writer_(s1->GetIsolate()),
        line_ends1_(line_ends1), line_ends2_(line_ends2), s1_(s1), s2_(s2),
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
      HandleScope subTaskScope(s1_->GetIsolate());

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
  s1 = String::Flatten(s1);
  s2 = String::Flatten(s2);

  LineEndsWrapper line_ends1(s1);
  LineEndsWrapper line_ends2(s2);

  LineArrayCompareInput input(s1, s2, line_ends1, line_ends2);
  TokenizingLineArrayCompareOutput output(line_ends1, line_ends2, s1, s2);

  NarrowDownInput(&input, &output);

  Comparator::CalculateDifference(&input, &output);

  return output.GetResult();
}


// Unwraps JSValue object, returning its field "value"
static Handle<Object> UnwrapJSValue(Handle<JSValue> jsValue) {
  return Handle<Object>(jsValue->value(), jsValue->GetIsolate());
}


// Wraps any object into a OpaqueReference, that will hide the object
// from JavaScript.
static Handle<JSValue> WrapInJSValue(Handle<HeapObject> object) {
  Isolate* isolate = object->GetIsolate();
  Handle<JSFunction> constructor = isolate->opaque_reference_function();
  Handle<JSValue> result =
      Handle<JSValue>::cast(isolate->factory()->NewJSObject(constructor));
  result->set_value(*object);
  return result;
}


static Handle<SharedFunctionInfo> UnwrapSharedFunctionInfoFromJSValue(
    Handle<JSValue> jsValue) {
  Object* shared = jsValue->value();
  CHECK(shared->IsSharedFunctionInfo());
  return Handle<SharedFunctionInfo>(SharedFunctionInfo::cast(shared));
}


static int GetArrayLength(Handle<JSArray> array) {
  Object* length = array->length();
  CHECK(length->IsSmi());
  return Smi::cast(length)->value();
}


void FunctionInfoWrapper::SetInitialProperties(Handle<String> name,
                                               int start_position,
                                               int end_position, int param_num,
                                               int literal_count,
                                               int parent_index) {
  HandleScope scope(isolate());
  this->SetField(kFunctionNameOffset_, name);
  this->SetSmiValueField(kStartPositionOffset_, start_position);
  this->SetSmiValueField(kEndPositionOffset_, end_position);
  this->SetSmiValueField(kParamNumOffset_, param_num);
  this->SetSmiValueField(kLiteralNumOffset_, literal_count);
  this->SetSmiValueField(kParentIndexOffset_, parent_index);
}


void FunctionInfoWrapper::SetFunctionCode(Handle<Code> function_code,
                                          Handle<HeapObject> code_scope_info) {
  Handle<JSValue> code_wrapper = WrapInJSValue(function_code);
  this->SetField(kCodeOffset_, code_wrapper);

  Handle<JSValue> scope_wrapper = WrapInJSValue(code_scope_info);
  this->SetField(kCodeScopeInfoOffset_, scope_wrapper);
}


void FunctionInfoWrapper::SetSharedFunctionInfo(
    Handle<SharedFunctionInfo> info) {
  Handle<JSValue> info_holder = WrapInJSValue(info);
  this->SetField(kSharedFunctionInfoOffset_, info_holder);
}


Handle<Code> FunctionInfoWrapper::GetFunctionCode() {
  Handle<Object> element = this->GetField(kCodeOffset_);
  Handle<JSValue> value_wrapper = Handle<JSValue>::cast(element);
  Handle<Object> raw_result = UnwrapJSValue(value_wrapper);
  CHECK(raw_result->IsCode());
  return Handle<Code>::cast(raw_result);
}


MaybeHandle<TypeFeedbackVector> FunctionInfoWrapper::GetFeedbackVector() {
  Handle<Object> element = this->GetField(kSharedFunctionInfoOffset_);
  if (element->IsJSValue()) {
    Handle<JSValue> value_wrapper = Handle<JSValue>::cast(element);
    Handle<Object> raw_result = UnwrapJSValue(value_wrapper);
    Handle<SharedFunctionInfo> shared =
        Handle<SharedFunctionInfo>::cast(raw_result);
    return Handle<TypeFeedbackVector>(shared->feedback_vector(), isolate());
  } else {
    // Scripts may never have a SharedFunctionInfo created.
    return MaybeHandle<TypeFeedbackVector>();
  }
}


Handle<Object> FunctionInfoWrapper::GetCodeScopeInfo() {
  Handle<Object> element = this->GetField(kCodeScopeInfoOffset_);
  return UnwrapJSValue(Handle<JSValue>::cast(element));
}


void SharedInfoWrapper::SetProperties(Handle<String> name,
                                      int start_position,
                                      int end_position,
                                      Handle<SharedFunctionInfo> info) {
  HandleScope scope(isolate());
  this->SetField(kFunctionNameOffset_, name);
  Handle<JSValue> info_holder = WrapInJSValue(info);
  this->SetField(kSharedInfoOffset_, info_holder);
  this->SetSmiValueField(kStartPositionOffset_, start_position);
  this->SetSmiValueField(kEndPositionOffset_, end_position);
}


Handle<SharedFunctionInfo> SharedInfoWrapper::GetInfo() {
  Handle<Object> element = this->GetField(kSharedInfoOffset_);
  Handle<JSValue> value_wrapper = Handle<JSValue>::cast(element);
  return UnwrapSharedFunctionInfoFromJSValue(value_wrapper);
}


class FunctionInfoListener {
 public:
  explicit FunctionInfoListener(Isolate* isolate) {
    current_parent_index_ = -1;
    len_ = 0;
    result_ = isolate->factory()->NewJSArray(10);
  }

  void FunctionStarted(FunctionLiteral* fun) {
    HandleScope scope(isolate());
    FunctionInfoWrapper info = FunctionInfoWrapper::Create(isolate());
    info.SetInitialProperties(fun->name(), fun->start_position(),
                              fun->end_position(), fun->parameter_count(),
                              fun->materialized_literal_count(),
                              current_parent_index_);
    current_parent_index_ = len_;
    SetElementSloppy(result_, len_, info.GetJSArray());
    len_++;
  }

  void FunctionDone() {
    HandleScope scope(isolate());
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(
            *Object::GetElement(
                isolate(), result_, current_parent_index_).ToHandleChecked());
    current_parent_index_ = info.GetParentIndex();
  }

  // Saves only function code, because for a script function we
  // may never create a SharedFunctionInfo object.
  void FunctionCode(Handle<Code> function_code) {
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(
            *Object::GetElement(
                isolate(), result_, current_parent_index_).ToHandleChecked());
    info.SetFunctionCode(function_code,
                         Handle<HeapObject>(isolate()->heap()->null_value()));
  }

  // Saves full information about a function: its code, its scope info
  // and a SharedFunctionInfo object.
  void FunctionInfo(Handle<SharedFunctionInfo> shared, Scope* scope,
                    Zone* zone) {
    if (!shared->IsSharedFunctionInfo()) {
      return;
    }
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(
            *Object::GetElement(
                isolate(), result_, current_parent_index_).ToHandleChecked());
    info.SetFunctionCode(Handle<Code>(shared->code()),
                         Handle<HeapObject>(shared->scope_info()));
    info.SetSharedFunctionInfo(shared);

    Handle<Object> scope_info_list = SerializeFunctionScope(scope, zone);
    info.SetFunctionScopeInfo(scope_info_list);
  }

  Handle<JSArray> GetResult() { return result_; }

 private:
  Isolate* isolate() const { return result_->GetIsolate(); }

  Handle<Object> SerializeFunctionScope(Scope* scope, Zone* zone) {
    Handle<JSArray> scope_info_list = isolate()->factory()->NewJSArray(10);
    int scope_info_length = 0;

    // Saves some description of scope. It stores name and indexes of
    // variables in the whole scope chain. Null-named slots delimit
    // scopes of this chain.
    Scope* current_scope = scope;
    while (current_scope != NULL) {
      HandleScope handle_scope(isolate());
      ZoneList<Variable*> stack_list(current_scope->StackLocalCount(), zone);
      ZoneList<Variable*> context_list(
          current_scope->ContextLocalCount(), zone);
      current_scope->CollectStackAndContextLocals(&stack_list, &context_list);
      context_list.Sort(&Variable::CompareIndex);

      for (int i = 0; i < context_list.length(); i++) {
        SetElementSloppy(scope_info_list,
                         scope_info_length,
                         context_list[i]->name());
        scope_info_length++;
        SetElementSloppy(
            scope_info_list,
            scope_info_length,
            Handle<Smi>(Smi::FromInt(context_list[i]->index()), isolate()));
        scope_info_length++;
      }
      SetElementSloppy(scope_info_list,
                       scope_info_length,
                       Handle<Object>(isolate()->heap()->null_value(),
                                      isolate()));
      scope_info_length++;

      current_scope = current_scope->outer_scope();
    }

    return scope_info_list;
  }

  Handle<JSArray> result_;
  int len_;
  int current_parent_index_;
};


void LiveEdit::InitializeThreadLocal(Debug* debug) {
  debug->thread_local_.frame_drop_mode_ = LiveEdit::FRAMES_UNTOUCHED;
}


bool LiveEdit::SetAfterBreakTarget(Debug* debug) {
  Code* code = NULL;
  Isolate* isolate = debug->isolate_;
  switch (debug->thread_local_.frame_drop_mode_) {
    case FRAMES_UNTOUCHED:
      return false;
    case FRAME_DROPPED_IN_IC_CALL:
      // We must have been calling IC stub. Do not go there anymore.
      code = isolate->builtins()->builtin(Builtins::kPlainReturn_LiveEdit);
      break;
    case FRAME_DROPPED_IN_DEBUG_SLOT_CALL:
      // Debug break slot stub does not return normally, instead it manually
      // cleans the stack and jumps. We should patch the jump address.
      code = isolate->builtins()->builtin(Builtins::kFrameDropper_LiveEdit);
      break;
    case FRAME_DROPPED_IN_DIRECT_CALL:
      // Nothing to do, after_break_target is not used here.
      return true;
    case FRAME_DROPPED_IN_RETURN_CALL:
      code = isolate->builtins()->builtin(Builtins::kFrameDropper_LiveEdit);
      break;
    case CURRENTLY_SET_MODE:
      UNREACHABLE();
      break;
  }
  debug->after_break_target_ = code->entry();
  return true;
}


MaybeHandle<JSArray> LiveEdit::GatherCompileInfo(Handle<Script> script,
                                                 Handle<String> source) {
  Isolate* isolate = script->GetIsolate();

  FunctionInfoListener listener(isolate);
  Handle<Object> original_source =
      Handle<Object>(script->source(), isolate);
  script->set_source(*source);
  isolate->set_active_function_info_listener(&listener);

  {
    // Creating verbose TryCatch from public API is currently the only way to
    // force code save location. We do not use this the object directly.
    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    // A logical 'try' section.
    Compiler::CompileForLiveEdit(script);
  }

  // A logical 'catch' section.
  Handle<JSObject> rethrow_exception;
  if (isolate->has_pending_exception()) {
    Handle<Object> exception(isolate->pending_exception(), isolate);
    MessageLocation message_location = isolate->GetMessageLocation();

    isolate->clear_pending_message();
    isolate->clear_pending_exception();

    // If possible, copy positions from message object to exception object.
    if (exception->IsJSObject() && !message_location.script().is_null()) {
      rethrow_exception = Handle<JSObject>::cast(exception);

      Factory* factory = isolate->factory();
      Handle<String> start_pos_key = factory->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("startPosition"));
      Handle<String> end_pos_key =
          factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("endPosition"));
      Handle<String> script_obj_key =
          factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("scriptObject"));
      Handle<Smi> start_pos(
          Smi::FromInt(message_location.start_pos()), isolate);
      Handle<Smi> end_pos(Smi::FromInt(message_location.end_pos()), isolate);
      Handle<JSObject> script_obj =
          Script::GetWrapper(message_location.script());
      Object::SetProperty(rethrow_exception, start_pos_key, start_pos, SLOPPY)
          .Assert();
      Object::SetProperty(rethrow_exception, end_pos_key, end_pos, SLOPPY)
          .Assert();
      Object::SetProperty(rethrow_exception, script_obj_key, script_obj, SLOPPY)
          .Assert();
    }
  }

  // A logical 'finally' section.
  isolate->set_active_function_info_listener(NULL);
  script->set_source(*original_source);

  if (rethrow_exception.is_null()) {
    return listener.GetResult();
  } else {
    return isolate->Throw<JSArray>(rethrow_exception);
  }
}


void LiveEdit::WrapSharedFunctionInfos(Handle<JSArray> array) {
  Isolate* isolate = array->GetIsolate();
  HandleScope scope(isolate);
  int len = GetArrayLength(array);
  for (int i = 0; i < len; i++) {
    Handle<SharedFunctionInfo> info(
        SharedFunctionInfo::cast(
            *Object::GetElement(isolate, array, i).ToHandleChecked()));
    SharedInfoWrapper info_wrapper = SharedInfoWrapper::Create(isolate);
    Handle<String> name_handle(String::cast(info->name()));
    info_wrapper.SetProperties(name_handle, info->start_position(),
                               info->end_position(), info);
    SetElementSloppy(array, i, info_wrapper.GetJSArray());
  }
}


// Visitor that finds all references to a particular code object,
// including "CODE_TARGET" references in other code objects and replaces
// them on the fly.
class ReplacingVisitor : public ObjectVisitor {
 public:
  explicit ReplacingVisitor(Code* original, Code* substitution)
    : original_(original), substitution_(substitution) {
  }

  virtual void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) {
      if (*p == original_) {
        *p = substitution_;
      }
    }
  }

  virtual void VisitCodeEntry(Address entry) {
    if (Code::GetObjectFromEntryAddress(entry) == original_) {
      Address substitution_entry = substitution_->instruction_start();
      Memory::Address_at(entry) = substitution_entry;
    }
  }

  virtual void VisitCodeTarget(RelocInfo* rinfo) {
    if (RelocInfo::IsCodeTarget(rinfo->rmode()) &&
        Code::GetCodeFromTargetAddress(rinfo->target_address()) == original_) {
      Address substitution_entry = substitution_->instruction_start();
      rinfo->set_target_address(substitution_entry);
    }
  }

  virtual void VisitDebugTarget(RelocInfo* rinfo) {
    VisitCodeTarget(rinfo);
  }

 private:
  Code* original_;
  Code* substitution_;
};


// Finds all references to original and replaces them with substitution.
static void ReplaceCodeObject(Handle<Code> original,
                              Handle<Code> substitution) {
  // Perform a full GC in order to ensure that we are not in the middle of an
  // incremental marking phase when we are replacing the code object.
  // Since we are not in an incremental marking phase we can write pointers
  // to code objects (that are never in new space) without worrying about
  // write barriers.
  Heap* heap = original->GetHeap();
  HeapIterator iterator(heap);

  DCHECK(!heap->InNewSpace(*substitution));

  ReplacingVisitor visitor(*original, *substitution);

  // Iterate over all roots. Stack frames may have pointer into original code,
  // so temporary replace the pointers with offset numbers
  // in prologue/epilogue.
  heap->IterateRoots(&visitor, VISIT_ALL);

  // Now iterate over all pointers of all objects, including code_target
  // implicit pointers.
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    obj->Iterate(&visitor);
  }
}


// Patch function literals.
// Name 'literals' is a misnomer. Rather it's a cache for complex object
// boilerplates and for a native context. We must clean cached values.
// Additionally we may need to allocate a new array if number of literals
// changed.
class LiteralFixer {
 public:
  static void PatchLiterals(FunctionInfoWrapper* compile_info_wrapper,
                            Handle<SharedFunctionInfo> shared_info,
                            Isolate* isolate) {
    int new_literal_count = compile_info_wrapper->GetLiteralCount();
    if (new_literal_count > 0) {
      new_literal_count += JSFunction::kLiteralsPrefixSize;
    }
    int old_literal_count = shared_info->num_literals();

    if (old_literal_count == new_literal_count) {
      // If literal count didn't change, simply go over all functions
      // and clear literal arrays.
      ClearValuesVisitor visitor;
      IterateJSFunctions(shared_info, &visitor);
    } else {
      // When literal count changes, we have to create new array instances.
      // Since we cannot create instances when iterating heap, we should first
      // collect all functions and fix their literal arrays.
      Handle<FixedArray> function_instances =
          CollectJSFunctions(shared_info, isolate);
      for (int i = 0; i < function_instances->length(); i++) {
        Handle<JSFunction> fun(JSFunction::cast(function_instances->get(i)));
        Handle<FixedArray> old_literals(fun->literals());
        Handle<FixedArray> new_literals =
            isolate->factory()->NewFixedArray(new_literal_count);
        if (new_literal_count > 0) {
          Handle<Context> native_context;
          if (old_literals->length() >
              JSFunction::kLiteralNativeContextIndex) {
            native_context = Handle<Context>(
                JSFunction::NativeContextFromLiterals(fun->literals()));
          } else {
            native_context = Handle<Context>(fun->context()->native_context());
          }
          new_literals->set(JSFunction::kLiteralNativeContextIndex,
              *native_context);
        }
        fun->set_literals(*new_literals);
      }

      shared_info->set_num_literals(new_literal_count);
    }
  }

 private:
  // Iterates all function instances in the HEAP that refers to the
  // provided shared_info.
  template<typename Visitor>
  static void IterateJSFunctions(Handle<SharedFunctionInfo> shared_info,
                                 Visitor* visitor) {
    HeapIterator iterator(shared_info->GetHeap());
    for (HeapObject* obj = iterator.next(); obj != NULL;
        obj = iterator.next()) {
      if (obj->IsJSFunction()) {
        JSFunction* function = JSFunction::cast(obj);
        if (function->shared() == *shared_info) {
          visitor->visit(function);
        }
      }
    }
  }

  // Finds all instances of JSFunction that refers to the provided shared_info
  // and returns array with them.
  static Handle<FixedArray> CollectJSFunctions(
      Handle<SharedFunctionInfo> shared_info, Isolate* isolate) {
    CountVisitor count_visitor;
    count_visitor.count = 0;
    IterateJSFunctions(shared_info, &count_visitor);
    int size = count_visitor.count;

    Handle<FixedArray> result = isolate->factory()->NewFixedArray(size);
    if (size > 0) {
      CollectVisitor collect_visitor(result);
      IterateJSFunctions(shared_info, &collect_visitor);
    }
    return result;
  }

  class ClearValuesVisitor {
   public:
    void visit(JSFunction* fun) {
      FixedArray* literals = fun->literals();
      int len = literals->length();
      for (int j = JSFunction::kLiteralsPrefixSize; j < len; j++) {
        literals->set_undefined(j);
      }
    }
  };

  class CountVisitor {
   public:
    void visit(JSFunction* fun) {
      count++;
    }
    int count;
  };

  class CollectVisitor {
   public:
    explicit CollectVisitor(Handle<FixedArray> output)
        : m_output(output), m_pos(0) {}

    void visit(JSFunction* fun) {
      m_output->set(m_pos, fun);
      m_pos++;
    }
   private:
    Handle<FixedArray> m_output;
    int m_pos;
  };
};


// Check whether the code is natural function code (not a lazy-compile stub
// code).
static bool IsJSFunctionCode(Code* code) {
  return code->kind() == Code::FUNCTION;
}


// Returns true if an instance of candidate were inlined into function's code.
static bool IsInlined(JSFunction* function, SharedFunctionInfo* candidate) {
  DisallowHeapAllocation no_gc;

  if (function->code()->kind() != Code::OPTIMIZED_FUNCTION) return false;

  DeoptimizationInputData* data =
      DeoptimizationInputData::cast(function->code()->deoptimization_data());

  if (data == function->GetIsolate()->heap()->empty_fixed_array()) {
    return false;
  }

  FixedArray* literals = data->LiteralArray();

  int inlined_count = data->InlinedFunctionCount()->value();
  for (int i = 0; i < inlined_count; ++i) {
    JSFunction* inlined = JSFunction::cast(literals->get(i));
    if (inlined->shared() == candidate) return true;
  }

  return false;
}


// Marks code that shares the same shared function info or has inlined
// code that shares the same function info.
class DependentFunctionMarker: public OptimizedFunctionVisitor {
 public:
  SharedFunctionInfo* shared_info_;
  bool found_;

  explicit DependentFunctionMarker(SharedFunctionInfo* shared_info)
    : shared_info_(shared_info), found_(false) { }

  virtual void EnterContext(Context* context) { }  // Don't care.
  virtual void LeaveContext(Context* context)  { }  // Don't care.
  virtual void VisitFunction(JSFunction* function) {
    // It should be guaranteed by the iterator that everything is optimized.
    DCHECK(function->code()->kind() == Code::OPTIMIZED_FUNCTION);
    if (shared_info_ == function->shared() ||
        IsInlined(function, shared_info_)) {
      // Mark the code for deoptimization.
      function->code()->set_marked_for_deoptimization(true);
      found_ = true;
    }
  }
};


static void DeoptimizeDependentFunctions(SharedFunctionInfo* function_info) {
  DisallowHeapAllocation no_allocation;
  DependentFunctionMarker marker(function_info);
  // TODO(titzer): need to traverse all optimized code to find OSR code here.
  Deoptimizer::VisitAllOptimizedFunctions(function_info->GetIsolate(), &marker);

  if (marker.found_) {
    // Only go through with the deoptimization if something was found.
    Deoptimizer::DeoptimizeMarkedCode(function_info->GetIsolate());
  }
}


void LiveEdit::ReplaceFunctionCode(
    Handle<JSArray> new_compile_info_array,
    Handle<JSArray> shared_info_array) {
  Isolate* isolate = new_compile_info_array->GetIsolate();

  FunctionInfoWrapper compile_info_wrapper(new_compile_info_array);
  SharedInfoWrapper shared_info_wrapper(shared_info_array);

  Handle<SharedFunctionInfo> shared_info = shared_info_wrapper.GetInfo();

  if (IsJSFunctionCode(shared_info->code())) {
    Handle<Code> code = compile_info_wrapper.GetFunctionCode();
    ReplaceCodeObject(Handle<Code>(shared_info->code()), code);
    Handle<Object> code_scope_info = compile_info_wrapper.GetCodeScopeInfo();
    if (code_scope_info->IsFixedArray()) {
      shared_info->set_scope_info(ScopeInfo::cast(*code_scope_info));
    }
    shared_info->DisableOptimization(kLiveEdit);
    // Update the type feedback vector, if needed.
    MaybeHandle<TypeFeedbackVector> feedback_vector =
        compile_info_wrapper.GetFeedbackVector();
    if (!feedback_vector.is_null()) {
      shared_info->set_feedback_vector(*feedback_vector.ToHandleChecked());
    }
  }

  if (shared_info->debug_info()->IsDebugInfo()) {
    Handle<DebugInfo> debug_info(DebugInfo::cast(shared_info->debug_info()));
    Handle<Code> new_original_code =
        isolate->factory()->CopyCode(compile_info_wrapper.GetFunctionCode());
    debug_info->set_original_code(*new_original_code);
  }

  int start_position = compile_info_wrapper.GetStartPosition();
  int end_position = compile_info_wrapper.GetEndPosition();
  shared_info->set_start_position(start_position);
  shared_info->set_end_position(end_position);

  LiteralFixer::PatchLiterals(&compile_info_wrapper, shared_info, isolate);

  shared_info->set_construct_stub(
      isolate->builtins()->builtin(Builtins::kJSConstructStubGeneric));

  DeoptimizeDependentFunctions(*shared_info);
  isolate->compilation_cache()->Remove(shared_info);
}


void LiveEdit::FunctionSourceUpdated(Handle<JSArray> shared_info_array) {
  SharedInfoWrapper shared_info_wrapper(shared_info_array);
  Handle<SharedFunctionInfo> shared_info = shared_info_wrapper.GetInfo();

  DeoptimizeDependentFunctions(*shared_info);
  shared_info_array->GetIsolate()->compilation_cache()->Remove(shared_info);
}


void LiveEdit::SetFunctionScript(Handle<JSValue> function_wrapper,
                                 Handle<Object> script_handle) {
  Handle<SharedFunctionInfo> shared_info =
      UnwrapSharedFunctionInfoFromJSValue(function_wrapper);
  CHECK(script_handle->IsScript() || script_handle->IsUndefined());
  shared_info->set_script(*script_handle);

  function_wrapper->GetIsolate()->compilation_cache()->Remove(shared_info);
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
  int array_len = GetArrayLength(position_change_array);
  Isolate* isolate = position_change_array->GetIsolate();
  // TODO(635): binary search may be used here
  for (int i = 0; i < array_len; i += 3) {
    HandleScope scope(isolate);
    Handle<Object> element = Object::GetElement(
        isolate, position_change_array, i).ToHandleChecked();
    CHECK(element->IsSmi());
    int chunk_start = Handle<Smi>::cast(element)->value();
    if (original_position < chunk_start) {
      break;
    }
    element = Object::GetElement(
        isolate, position_change_array, i + 1).ToHandleChecked();
    CHECK(element->IsSmi());
    int chunk_end = Handle<Smi>::cast(element)->value();
    // Position mustn't be inside a chunk.
    DCHECK(original_position >= chunk_end);
    element = Object::GetElement(
        isolate, position_change_array, i + 2).ToHandleChecked();
    CHECK(element->IsSmi());
    int chunk_changed_end = Handle<Smi>::cast(element)->value();
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
    MemMove(new_buffer + new_buffer_size - curently_used_size,
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
  Isolate* isolate = code->GetIsolate();

  RelocInfoBuffer buffer_writer(code->relocation_size(),
                                code->instruction_start());

  {
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
      if (RelocInfo::IsRealRelocMode(rinfo->rmode())) {
        buffer_writer.Write(it.rinfo());
      }
    }
  }

  Vector<byte> buffer = buffer_writer.GetResult();

  if (buffer.length() == code->relocation_size()) {
    // Simply patch relocation area of code.
    MemCopy(code->relocation_start(), buffer.start(), buffer.length());
    return code;
  } else {
    // Relocation info section now has different size. We cannot simply
    // rewrite it inside code object. Instead we have to create a new
    // code object.
    Handle<Code> result(isolate->factory()->CopyCode(code, buffer));
    return result;
  }
}


void LiveEdit::PatchFunctionPositions(Handle<JSArray> shared_info_array,
                                      Handle<JSArray> position_change_array) {
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
      ReplaceCodeObject(Handle<Code>(info->code()), patched_code);
    }
  }
}


static Handle<Script> CreateScriptCopy(Handle<Script> original) {
  Isolate* isolate = original->GetIsolate();

  Handle<String> original_source(String::cast(original->source()));
  Handle<Script> copy = isolate->factory()->NewScript(original_source);

  copy->set_name(original->name());
  copy->set_line_offset(original->line_offset());
  copy->set_column_offset(original->column_offset());
  copy->set_type(original->type());
  copy->set_context_data(original->context_data());
  copy->set_eval_from_shared(original->eval_from_shared());
  copy->set_eval_from_instructions_offset(
      original->eval_from_instructions_offset());

  // Copy all the flags, but clear compilation state.
  copy->set_flags(original->flags());
  copy->set_compilation_state(Script::COMPILATION_STATE_INITIAL);

  return copy;
}


Handle<Object> LiveEdit::ChangeScriptSource(Handle<Script> original_script,
                                            Handle<String> new_source,
                                            Handle<Object> old_script_name) {
  Isolate* isolate = original_script->GetIsolate();
  Handle<Object> old_script_object;
  if (old_script_name->IsString()) {
    Handle<Script> old_script = CreateScriptCopy(original_script);
    old_script->set_name(String::cast(*old_script_name));
    old_script_object = old_script;
    isolate->debug()->OnAfterCompile(old_script);
  } else {
    old_script_object = isolate->factory()->null_value();
  }

  original_script->set_source(*new_source);

  // Drop line ends so that they will be recalculated.
  original_script->set_line_ends(isolate->heap()->undefined_value());

  return old_script_object;
}



void LiveEdit::ReplaceRefToNestedFunction(
    Handle<JSValue> parent_function_wrapper,
    Handle<JSValue> orig_function_wrapper,
    Handle<JSValue> subst_function_wrapper) {

  Handle<SharedFunctionInfo> parent_shared =
      UnwrapSharedFunctionInfoFromJSValue(parent_function_wrapper);
  Handle<SharedFunctionInfo> orig_shared =
      UnwrapSharedFunctionInfoFromJSValue(orig_function_wrapper);
  Handle<SharedFunctionInfo> subst_shared =
      UnwrapSharedFunctionInfoFromJSValue(subst_function_wrapper);

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

  Handle<JSFunction> function(JavaScriptFrame::cast(frame)->function());

  Isolate* isolate = shared_info_array->GetIsolate();
  int len = GetArrayLength(shared_info_array);
  for (int i = 0; i < len; i++) {
    HandleScope scope(isolate);
    Handle<Object> element =
        Object::GetElement(isolate, shared_info_array, i).ToHandleChecked();
    Handle<JSValue> jsvalue = Handle<JSValue>::cast(element);
    Handle<SharedFunctionInfo> shared =
        UnwrapSharedFunctionInfoFromJSValue(jsvalue);

    if (function->shared() == *shared || IsInlined(*function, *shared)) {
      SetElementSloppy(result, i, Handle<Smi>(Smi::FromInt(status), isolate));
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
      &Memory::Address_at(top_frame->isolate()->get_address_from_id(
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


// Initializes an artificial stack frame. The data it contains is used for:
//  a. successful work of frame dropper code which eventually gets control,
//  b. being compatible with regular stack structure for various stack
//     iterators.
// Returns address of stack allocated pointer to restarted function,
// the value that is called 'restarter_frame_function_pointer'. The value
// at this address (possibly updated by GC) may be used later when preparing
// 'step in' operation.
// Frame structure (conforms InternalFrame structure):
//   -- code
//   -- SMI maker
//   -- function (slot is called "context")
//   -- frame base
static Object** SetUpFrameDropperFrame(StackFrame* bottom_js_frame,
                                       Handle<Code> code) {
  DCHECK(bottom_js_frame->is_java_script());

  Address fp = bottom_js_frame->fp();

  // Move function pointer into "context" slot.
  Memory::Object_at(fp + StandardFrameConstants::kContextOffset) =
      Memory::Object_at(fp + JavaScriptFrameConstants::kFunctionOffset);

  Memory::Object_at(fp + InternalFrameConstants::kCodeOffset) = *code;
  Memory::Object_at(fp + StandardFrameConstants::kMarkerOffset) =
      Smi::FromInt(StackFrame::INTERNAL);

  return reinterpret_cast<Object**>(&Memory::Object_at(
      fp + StandardFrameConstants::kContextOffset));
}


// Removes specified range of frames from stack. There may be 1 or more
// frames in range. Anyway the bottom frame is restarted rather than dropped,
// and therefore has to be a JavaScript frame.
// Returns error message or NULL.
static const char* DropFrames(Vector<StackFrame*> frames,
                              int top_frame_index,
                              int bottom_js_frame_index,
                              LiveEdit::FrameDropMode* mode,
                              Object*** restarter_frame_function_pointer) {
  if (!LiveEdit::kFrameDropperSupported) {
    return "Stack manipulations are not supported in this architecture.";
  }

  StackFrame* pre_top_frame = frames[top_frame_index - 1];
  StackFrame* top_frame = frames[top_frame_index];
  StackFrame* bottom_js_frame = frames[bottom_js_frame_index];

  DCHECK(bottom_js_frame->is_java_script());

  // Check the nature of the top frame.
  Isolate* isolate = bottom_js_frame->isolate();
  Code* pre_top_frame_code = pre_top_frame->LookupCode();
  bool frame_has_padding = true;
  if (pre_top_frame_code->is_inline_cache_stub() &&
      pre_top_frame_code->is_debug_stub()) {
    // OK, we can drop inline cache calls.
    *mode = LiveEdit::FRAME_DROPPED_IN_IC_CALL;
  } else if (pre_top_frame_code ==
             isolate->builtins()->builtin(Builtins::kSlot_DebugBreak)) {
    // OK, we can drop debug break slot.
    *mode = LiveEdit::FRAME_DROPPED_IN_DEBUG_SLOT_CALL;
  } else if (pre_top_frame_code ==
             isolate->builtins()->builtin(Builtins::kFrameDropper_LiveEdit)) {
    // OK, we can drop our own code.
    pre_top_frame = frames[top_frame_index - 2];
    top_frame = frames[top_frame_index - 1];
    *mode = LiveEdit::CURRENTLY_SET_MODE;
    frame_has_padding = false;
  } else if (pre_top_frame_code ==
             isolate->builtins()->builtin(Builtins::kReturn_DebugBreak)) {
    *mode = LiveEdit::FRAME_DROPPED_IN_RETURN_CALL;
  } else if (pre_top_frame_code->kind() == Code::STUB &&
             CodeStub::GetMajorKey(pre_top_frame_code) == CodeStub::CEntry) {
    // Entry from our unit tests on 'debugger' statement.
    // It's fine, we support this case.
    *mode = LiveEdit::FRAME_DROPPED_IN_DIRECT_CALL;
    // We don't have a padding from 'debugger' statement call.
    // Here the stub is CEntry, it's not debug-only and can't be padded.
    // If anyone would complain, a proxy padded stub could be added.
    frame_has_padding = false;
  } else if (pre_top_frame->type() == StackFrame::ARGUMENTS_ADAPTOR) {
    // This must be adaptor that remain from the frame dropping that
    // is still on stack. A frame dropper frame must be above it.
    DCHECK(frames[top_frame_index - 2]->LookupCode() ==
           isolate->builtins()->builtin(Builtins::kFrameDropper_LiveEdit));
    pre_top_frame = frames[top_frame_index - 3];
    top_frame = frames[top_frame_index - 2];
    *mode = LiveEdit::CURRENTLY_SET_MODE;
    frame_has_padding = false;
  } else {
    return "Unknown structure of stack above changing function";
  }

  Address unused_stack_top = top_frame->sp();
  int new_frame_size = LiveEdit::kFrameDropperFrameSize * kPointerSize;
  Address unused_stack_bottom = bottom_js_frame->fp()
      - new_frame_size + kPointerSize;  // Bigger address end is exclusive.

  Address* top_frame_pc_address = top_frame->pc_address();

  // top_frame may be damaged below this point. Do not used it.
  DCHECK(!(top_frame = NULL));

  if (unused_stack_top > unused_stack_bottom) {
    if (frame_has_padding) {
      int shortage_bytes =
          static_cast<int>(unused_stack_top - unused_stack_bottom);

      Address padding_start = pre_top_frame->fp() -
          LiveEdit::kFrameDropperFrameSize * kPointerSize;

      Address padding_pointer = padding_start;
      Smi* padding_object = Smi::FromInt(LiveEdit::kFramePaddingValue);
      while (Memory::Object_at(padding_pointer) == padding_object) {
        padding_pointer -= kPointerSize;
      }
      int padding_counter =
          Smi::cast(Memory::Object_at(padding_pointer))->value();
      if (padding_counter * kPointerSize < shortage_bytes) {
        return "Not enough space for frame dropper frame "
            "(even with padding frame)";
      }
      Memory::Object_at(padding_pointer) =
          Smi::FromInt(padding_counter - shortage_bytes / kPointerSize);

      StackFrame* pre_pre_frame = frames[top_frame_index - 2];

      MemMove(padding_start + kPointerSize - shortage_bytes,
              padding_start + kPointerSize,
              LiveEdit::kFrameDropperFrameSize * kPointerSize);

      pre_top_frame->UpdateFp(pre_top_frame->fp() - shortage_bytes);
      pre_pre_frame->SetCallerFp(pre_top_frame->fp());
      unused_stack_top -= shortage_bytes;

      STATIC_ASSERT(sizeof(Address) == kPointerSize);
      top_frame_pc_address -= shortage_bytes / kPointerSize;
    } else {
      return "Not enough space for frame dropper frame";
    }
  }

  // Committing now. After this point we should return only NULL value.

  FixTryCatchHandler(pre_top_frame, bottom_js_frame);
  // Make sure FixTryCatchHandler is idempotent.
  DCHECK(!FixTryCatchHandler(pre_top_frame, bottom_js_frame));

  Handle<Code> code = isolate->builtins()->FrameDropper_LiveEdit();
  *top_frame_pc_address = code->entry();
  pre_top_frame->SetCallerFp(bottom_js_frame->fp());

  *restarter_frame_function_pointer =
      SetUpFrameDropperFrame(bottom_js_frame, code);

  DCHECK((**restarter_frame_function_pointer)->IsJSFunction());

  for (Address a = unused_stack_top;
      a < unused_stack_bottom;
      a += kPointerSize) {
    Memory::Object_at(a) = Smi::FromInt(0);
  }

  return NULL;
}


// Describes a set of call frames that execute any of listed functions.
// Finding no such frames does not mean error.
class MultipleFunctionTarget {
 public:
  MultipleFunctionTarget(Handle<JSArray> shared_info_array,
      Handle<JSArray> result)
      : m_shared_info_array(shared_info_array),
        m_result(result) {}
  bool MatchActivation(StackFrame* frame,
      LiveEdit::FunctionPatchabilityStatus status) {
    return CheckActivation(m_shared_info_array, m_result, frame, status);
  }
  const char* GetNotFoundMessage() const {
    return NULL;
  }
 private:
  Handle<JSArray> m_shared_info_array;
  Handle<JSArray> m_result;
};


// Drops all call frame matched by target and all frames above them.
template<typename TARGET>
static const char* DropActivationsInActiveThreadImpl(
    Isolate* isolate,
    TARGET& target,  // NOLINT
    bool do_drop) {
  Debug* debug = isolate->debug();
  Zone zone(isolate);
  Vector<StackFrame*> frames = CreateStackMap(isolate, &zone);


  int top_frame_index = -1;
  int frame_index = 0;
  for (; frame_index < frames.length(); frame_index++) {
    StackFrame* frame = frames[frame_index];
    if (frame->id() == debug->break_frame_id()) {
      top_frame_index = frame_index;
      break;
    }
    if (target.MatchActivation(
            frame, LiveEdit::FUNCTION_BLOCKED_UNDER_NATIVE_CODE)) {
      // We are still above break_frame. It is not a target frame,
      // it is a problem.
      return "Debugger mark-up on stack is not found";
    }
  }

  if (top_frame_index == -1) {
    // We haven't found break frame, but no function is blocking us anyway.
    return target.GetNotFoundMessage();
  }

  bool target_frame_found = false;
  int bottom_js_frame_index = top_frame_index;
  bool non_droppable_frame_found = false;
  LiveEdit::FunctionPatchabilityStatus non_droppable_reason;

  for (; frame_index < frames.length(); frame_index++) {
    StackFrame* frame = frames[frame_index];
    if (frame->is_exit()) {
      non_droppable_frame_found = true;
      non_droppable_reason = LiveEdit::FUNCTION_BLOCKED_UNDER_NATIVE_CODE;
      break;
    }
    if (frame->is_java_script() &&
        JavaScriptFrame::cast(frame)->function()->shared()->is_generator()) {
      non_droppable_frame_found = true;
      non_droppable_reason = LiveEdit::FUNCTION_BLOCKED_UNDER_GENERATOR;
      break;
    }
    if (target.MatchActivation(
            frame, LiveEdit::FUNCTION_BLOCKED_ON_ACTIVE_STACK)) {
      target_frame_found = true;
      bottom_js_frame_index = frame_index;
    }
  }

  if (non_droppable_frame_found) {
    // There is a C or generator frame on stack.  We can't drop C frames, and we
    // can't restart generators.  Check that there are no target frames below
    // them.
    for (; frame_index < frames.length(); frame_index++) {
      StackFrame* frame = frames[frame_index];
      if (frame->is_java_script()) {
        if (target.MatchActivation(frame, non_droppable_reason)) {
          // Fail.
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
    return target.GetNotFoundMessage();
  }

  LiveEdit::FrameDropMode drop_mode = LiveEdit::FRAMES_UNTOUCHED;
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
  debug->FramesHaveBeenDropped(
      new_id, drop_mode, restarter_frame_function_pointer);
  return NULL;
}


// Fills result array with statuses of functions. Modifies the stack
// removing all listed function if possible and if do_drop is true.
static const char* DropActivationsInActiveThread(
    Handle<JSArray> shared_info_array, Handle<JSArray> result, bool do_drop) {
  MultipleFunctionTarget target(shared_info_array, result);

  const char* message = DropActivationsInActiveThreadImpl(
      shared_info_array->GetIsolate(), target, do_drop);
  if (message) {
    return message;
  }

  Isolate* isolate = shared_info_array->GetIsolate();
  int array_len = GetArrayLength(shared_info_array);

  // Replace "blocked on active" with "replaced on active" status.
  for (int i = 0; i < array_len; i++) {
    Handle<Object> obj =
        Object::GetElement(isolate, result, i).ToHandleChecked();
    if (*obj == Smi::FromInt(LiveEdit::FUNCTION_BLOCKED_ON_ACTIVE_STACK)) {
      Handle<Object> replaced(
          Smi::FromInt(LiveEdit::FUNCTION_REPLACED_ON_ACTIVE_STACK), isolate);
      SetElementSloppy(result, i, replaced);
    }
  }
  return NULL;
}


bool LiveEdit::FindActiveGenerators(Handle<FixedArray> shared_info_array,
                                    Handle<FixedArray> result,
                                    int len) {
  Isolate* isolate = shared_info_array->GetIsolate();
  bool found_suspended_activations = false;

  DCHECK_LE(len, result->length());

  FunctionPatchabilityStatus active = FUNCTION_BLOCKED_ACTIVE_GENERATOR;

  Heap* heap = isolate->heap();
  HeapIterator iterator(heap);
  HeapObject* obj = NULL;
  while ((obj = iterator.next()) != NULL) {
    if (!obj->IsJSGeneratorObject()) continue;

    JSGeneratorObject* gen = JSGeneratorObject::cast(obj);
    if (gen->is_closed()) continue;

    HandleScope scope(isolate);

    for (int i = 0; i < len; i++) {
      Handle<JSValue> jsvalue =
          Handle<JSValue>::cast(FixedArray::get(shared_info_array, i));
      Handle<SharedFunctionInfo> shared =
          UnwrapSharedFunctionInfoFromJSValue(jsvalue);

      if (gen->function()->shared() == *shared) {
        result->set(i, Smi::FromInt(active));
        found_suspended_activations = true;
      }
    }
  }

  return found_suspended_activations;
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
  Isolate* isolate = shared_info_array->GetIsolate();
  int len = GetArrayLength(shared_info_array);

  DCHECK(shared_info_array->HasFastElements());
  Handle<FixedArray> shared_info_array_elements(
      FixedArray::cast(shared_info_array->elements()));

  Handle<JSArray> result = isolate->factory()->NewJSArray(len);
  Handle<FixedArray> result_elements =
      JSObject::EnsureWritableFastElements(result);

  // Fill the default values.
  for (int i = 0; i < len; i++) {
    FunctionPatchabilityStatus status = FUNCTION_AVAILABLE_FOR_PATCH;
    result_elements->set(i, Smi::FromInt(status));
  }

  // Scan the heap for active generators -- those that are either currently
  // running (as we wouldn't want to restart them, because we don't know where
  // to restart them from) or suspended.  Fail if any one corresponds to the set
  // of functions being edited.
  if (FindActiveGenerators(shared_info_array_elements, result_elements, len)) {
    return result;
  }

  // Check inactive threads. Fail if some functions are blocked there.
  InactiveThreadActivationsChecker inactive_threads_checker(shared_info_array,
                                                            result);
  isolate->thread_manager()->IterateArchivedThreads(
      &inactive_threads_checker);
  if (inactive_threads_checker.HasBlockedFunctions()) {
    return result;
  }

  // Try to drop activations from the current stack.
  const char* error_message =
      DropActivationsInActiveThread(shared_info_array, result, do_drop);
  if (error_message != NULL) {
    // Add error message as an array extra element.
    Handle<String> str =
        isolate->factory()->NewStringFromAsciiChecked(error_message);
    SetElementSloppy(result, len, str);
  }
  return result;
}


// Describes a single callframe a target. Not finding this frame
// means an error.
class SingleFrameTarget {
 public:
  explicit SingleFrameTarget(JavaScriptFrame* frame)
      : m_frame(frame),
        m_saved_status(LiveEdit::FUNCTION_AVAILABLE_FOR_PATCH) {}

  bool MatchActivation(StackFrame* frame,
      LiveEdit::FunctionPatchabilityStatus status) {
    if (frame->fp() == m_frame->fp()) {
      m_saved_status = status;
      return true;
    }
    return false;
  }
  const char* GetNotFoundMessage() const {
    return "Failed to found requested frame";
  }
  LiveEdit::FunctionPatchabilityStatus saved_status() {
    return m_saved_status;
  }
 private:
  JavaScriptFrame* m_frame;
  LiveEdit::FunctionPatchabilityStatus m_saved_status;
};


// Finds a drops required frame and all frames above.
// Returns error message or NULL.
const char* LiveEdit::RestartFrame(JavaScriptFrame* frame) {
  SingleFrameTarget target(frame);

  const char* result = DropActivationsInActiveThreadImpl(
      frame->isolate(), target, true);
  if (result != NULL) {
    return result;
  }
  if (target.saved_status() == LiveEdit::FUNCTION_BLOCKED_UNDER_NATIVE_CODE) {
    return "Function is blocked under native code";
  }
  if (target.saved_status() == LiveEdit::FUNCTION_BLOCKED_UNDER_GENERATOR) {
    return "Function is blocked under a generator activation";
  }
  return NULL;
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
    Handle<SharedFunctionInfo> info, FunctionLiteral* lit,
    Zone* zone) {
  if (isolate_->active_function_info_listener() != NULL) {
    isolate_->active_function_info_listener()->FunctionInfo(info, lit->scope(),
                                                            zone);
  }
}


void LiveEditFunctionTracker::RecordRootFunctionInfo(Handle<Code> code) {
  isolate_->active_function_info_listener()->FunctionCode(code);
}


bool LiveEditFunctionTracker::IsActive(Isolate* isolate) {
  return isolate->active_function_info_listener() != NULL;
}

} }  // namespace v8::internal
