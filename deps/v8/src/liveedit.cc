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

namespace v8 {
namespace internal {


#ifdef ENABLE_DEBUGGER_SUPPORT

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
  Handle<Code> code = MakeCodeForLiveEdit(&info);

  // Check for stack-overflow exceptions.
  if (code.is_null()) {
    Top::StackOverflow();
    return;
  }
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
  void SetScopeInfo(Handle<JSArray> scope_info_array) {
    this->SetField(kScopeInfoOffset_, scope_info_array);
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
  static const int kSize_ = 7;

  friend class JSArrayBasedStruct<FunctionInfoWrapper>;
};

// Wraps SharedFunctionInfo along with some of its fields for passing it
// back to JavaScript. SharedFunctionInfo object itself is additionally
// wrapped into BlindReference for sanitizing reasons.
class SharedInfoWrapper : public JSArrayBasedStruct<SharedInfoWrapper> {
 public:
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

  void FunctionScope(Scope* scope) {
    HandleScope handle_scope;

    Handle<JSArray> scope_info_list = Factory::NewJSArray(10);
    int scope_info_length = 0;

    // Saves some description of scope. It stores name and indexes of
    // variables in the whole scope chain. Null-named slots delimit
    // scopes of this chain.
    Scope* outer_scope = scope->outer_scope();
    if (outer_scope == NULL) {
      return;
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

    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(result_->GetElement(current_parent_index_));
    info.SetScopeInfo(scope_info_list);
  }

  void FunctionCode(Handle<Code> function_code) {
    FunctionInfoWrapper info =
        FunctionInfoWrapper::cast(result_->GetElement(current_parent_index_));
    info.SetFunctionCode(function_code);
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
    ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
    if (Code::GetCodeFromTargetAddress(rinfo->target_address()) == original_) {
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


void LiveEdit::ReplaceFunctionCode(Handle<JSArray> new_compile_info_array,
                                   Handle<JSArray> shared_info_array) {
  HandleScope scope;

  FunctionInfoWrapper compile_info_wrapper(new_compile_info_array);
  SharedInfoWrapper shared_info_wrapper(shared_info_array);

  Handle<SharedFunctionInfo> shared_info = shared_info_wrapper.GetInfo();

  ReplaceCodeObject(shared_info->code(),
                       *(compile_info_wrapper.GetFunctionCode()));

  shared_info->set_start_position(compile_info_wrapper.GetStartPosition());
  shared_info->set_end_position(compile_info_wrapper.GetEndPosition());
  // update breakpoints, original code, constructor stub
}


void LiveEdit::RelinkFunctionToScript(Handle<JSArray> shared_info_array,
                                      Handle<Script> script_handle) {
  SharedInfoWrapper shared_info_wrapper(shared_info_array);
  Handle<SharedFunctionInfo> shared_info = shared_info_wrapper.GetInfo();

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
    return Vector<byte>(reloc_info_writer_.pos(),
                        buffer_ + buffer_size_ - reloc_info_writer_.pos());
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
    int curently_used_size = buffer_ + buffer_size_ - reloc_info_writer_.pos();
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


void LiveEdit::PatchFunctionPositions(Handle<JSArray> shared_info_array,
                                      Handle<JSArray> position_change_array) {
  SharedInfoWrapper shared_info_wrapper(shared_info_array);
  Handle<SharedFunctionInfo> info = shared_info_wrapper.GetInfo();

  info->set_start_position(TranslatePosition(info->start_position(),
                                             position_change_array));
  info->set_end_position(TranslatePosition(info->end_position(),
                                           position_change_array));

  info->set_function_token_position(
      TranslatePosition(info->function_token_position(),
      position_change_array));

  // Patch relocation info section of the code.
  Handle<Code> patched_code = PatchPositionsInCode(Handle<Code>(info->code()),
                                                   position_change_array);
  if (*patched_code != info->code()) {
    // Replace all references to the code across the heap. In particular,
    // some stubs may refer to this code and this code may be being executed
    // on stack (it is safe to substitute the code object on stack, because
    // we only change the structure of rinfo and leave instructions untouched).
    ReplaceCodeObject(info->code(), *patched_code);
  }

  if (info->debug_info()->IsDebugInfo()) {
    Handle<DebugInfo> debug_info(DebugInfo::cast(info->debug_info()));
    Handle<Code> patched_orig_code =
        PatchPositionsInCode(Handle<Code>(debug_info->original_code()),
                             position_change_array);
    if (*patched_orig_code != debug_info->original_code()) {
      // Do not use expensive ReplaceCodeObject for original_code, because we
      // do not expect any other references except this one.
      debug_info->set_original_code(*patched_orig_code);
    }

    Handle<FixedArray> break_point_infos(debug_info->break_points());
    for (int i = 0; i < break_point_infos->length(); i++) {
      if (!break_point_infos->get(i)->IsBreakPointInfo()) {
        continue;
      }
      Handle<BreakPointInfo> info(
          BreakPointInfo::cast(break_point_infos->get(i)));
      int new_position = TranslatePosition(info->source_position()->value(),
          position_change_array);
      info->set_source_position(Smi::FromInt(new_position));
    }
  }
  // TODO(635): Also patch breakpoint objects in JS.
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


void LiveEditFunctionTracker::RecordFunctionCode(Handle<Code> code) {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionCode(code);
  }
}


void LiveEditFunctionTracker::RecordFunctionScope(Scope* scope) {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionScope(scope);
  }
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


void LiveEditFunctionTracker::RecordFunctionCode(Handle<Code> code) {
}


void LiveEditFunctionTracker::RecordFunctionScope(Scope* scope) {
}


bool LiveEditFunctionTracker::IsActive() {
  return false;
}

#endif  // ENABLE_DEBUGGER_SUPPORT



} }  // namespace v8::internal
