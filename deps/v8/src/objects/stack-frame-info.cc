// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/stack-frame-info.h"

#include "src/objects/stack-frame-info-inl.h"
#include "src/strings/string-builder-inl.h"

namespace v8 {
namespace internal {

// static
int StackTraceFrame::GetLineNumber(Handle<StackTraceFrame> frame) {
  int line = GetFrameInfo(frame)->line_number();
  return line != StackFrameBase::kNone ? line : Message::kNoLineNumberInfo;
}

// static
int StackTraceFrame::GetOneBasedLineNumber(Handle<StackTraceFrame> frame) {
  // JavaScript line numbers are already 1-based. Wasm line numbers need
  // to be adjusted.
  int line = StackTraceFrame::GetLineNumber(frame);
  if (StackTraceFrame::IsWasm(frame) && line >= 0) line++;
  return line;
}

// static
int StackTraceFrame::GetColumnNumber(Handle<StackTraceFrame> frame) {
  int column = GetFrameInfo(frame)->column_number();
  return column != StackFrameBase::kNone ? column : Message::kNoColumnInfo;
}

// static
int StackTraceFrame::GetOneBasedColumnNumber(Handle<StackTraceFrame> frame) {
  // JavaScript colun numbers are already 1-based. Wasm column numbers need
  // to be adjusted.
  int column = StackTraceFrame::GetColumnNumber(frame);
  if (StackTraceFrame::IsWasm(frame) && column >= 0) column++;
  return column;
}

// static
int StackTraceFrame::GetScriptId(Handle<StackTraceFrame> frame) {
  int id = GetFrameInfo(frame)->script_id();
  return id != StackFrameBase::kNone ? id : Message::kNoScriptIdInfo;
}

// static
int StackTraceFrame::GetPromiseAllIndex(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->promise_all_index();
}

// static
int StackTraceFrame::GetFunctionOffset(Handle<StackTraceFrame> frame) {
  DCHECK(IsWasm(frame));
  return GetFrameInfo(frame)->function_offset();
}

// static
Handle<Object> StackTraceFrame::GetFileName(Handle<StackTraceFrame> frame) {
  auto name = GetFrameInfo(frame)->script_name();
  return handle(name, frame->GetIsolate());
}

// static
Handle<Object> StackTraceFrame::GetScriptNameOrSourceUrl(
    Handle<StackTraceFrame> frame) {
  auto name = GetFrameInfo(frame)->script_name_or_source_url();
  return handle(name, frame->GetIsolate());
}

// static
Handle<Object> StackTraceFrame::GetFunctionName(Handle<StackTraceFrame> frame) {
  auto name = GetFrameInfo(frame)->function_name();
  return handle(name, frame->GetIsolate());
}

// static
Handle<Object> StackTraceFrame::GetMethodName(Handle<StackTraceFrame> frame) {
  auto name = GetFrameInfo(frame)->method_name();
  return handle(name, frame->GetIsolate());
}

// static
Handle<Object> StackTraceFrame::GetTypeName(Handle<StackTraceFrame> frame) {
  auto name = GetFrameInfo(frame)->type_name();
  return handle(name, frame->GetIsolate());
}

// static
Handle<Object> StackTraceFrame::GetEvalOrigin(Handle<StackTraceFrame> frame) {
  auto origin = GetFrameInfo(frame)->eval_origin();
  return handle(origin, frame->GetIsolate());
}

// static
Handle<Object> StackTraceFrame::GetWasmModuleName(
    Handle<StackTraceFrame> frame) {
  auto module = GetFrameInfo(frame)->wasm_module_name();
  return handle(module, frame->GetIsolate());
}

// static
Handle<WasmInstanceObject> StackTraceFrame::GetWasmInstance(
    Handle<StackTraceFrame> frame) {
  Object instance = GetFrameInfo(frame)->wasm_instance();
  return handle(WasmInstanceObject::cast(instance), frame->GetIsolate());
}

// static
bool StackTraceFrame::IsEval(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->is_eval();
}

// static
bool StackTraceFrame::IsConstructor(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->is_constructor();
}

// static
bool StackTraceFrame::IsWasm(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->is_wasm();
}

// static
bool StackTraceFrame::IsAsmJsWasm(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->is_asmjs_wasm();
}

// static
bool StackTraceFrame::IsUserJavaScript(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->is_user_java_script();
}

// static
bool StackTraceFrame::IsToplevel(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->is_toplevel();
}

// static
bool StackTraceFrame::IsAsync(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->is_async();
}

// static
bool StackTraceFrame::IsPromiseAll(Handle<StackTraceFrame> frame) {
  return GetFrameInfo(frame)->is_promise_all();
}

// static
Handle<StackFrameInfo> StackTraceFrame::GetFrameInfo(
    Handle<StackTraceFrame> frame) {
  if (frame->frame_info().IsUndefined()) InitializeFrameInfo(frame);
  return handle(StackFrameInfo::cast(frame->frame_info()), frame->GetIsolate());
}

// static
void StackTraceFrame::InitializeFrameInfo(Handle<StackTraceFrame> frame) {
  Isolate* isolate = frame->GetIsolate();
  Handle<StackFrameInfo> frame_info = isolate->factory()->NewStackFrameInfo(
      handle(FrameArray::cast(frame->frame_array()), isolate),
      frame->frame_index());
  frame->set_frame_info(*frame_info);

  // After initializing, we no longer need to keep a reference
  // to the frame_array.
  frame->set_frame_array(ReadOnlyRoots(isolate).undefined_value());
  frame->set_frame_index(-1);
}

Handle<FrameArray> GetFrameArrayFromStackTrace(Isolate* isolate,
                                               Handle<FixedArray> stack_trace) {
  // For the empty case, a empty FrameArray needs to be allocated so the rest
  // of the code doesn't has to be special cased everywhere.
  if (stack_trace->length() == 0) {
    return isolate->factory()->NewFrameArray(0);
  }

  // Retrieve the FrameArray from the first StackTraceFrame.
  DCHECK_GT(stack_trace->length(), 0);
  Handle<StackTraceFrame> frame(StackTraceFrame::cast(stack_trace->get(0)),
                                isolate);
  return handle(FrameArray::cast(frame->frame_array()), isolate);
}

namespace {

bool IsNonEmptyString(Handle<Object> object) {
  return (object->IsString() && String::cast(*object).length() > 0);
}

void AppendFileLocation(Isolate* isolate, Handle<StackTraceFrame> frame,
                        IncrementalStringBuilder* builder) {
  Handle<Object> file_name = StackTraceFrame::GetScriptNameOrSourceUrl(frame);
  if (!file_name->IsString() && StackTraceFrame::IsEval(frame)) {
    Handle<Object> eval_origin = StackTraceFrame::GetEvalOrigin(frame);
    DCHECK(eval_origin->IsString());
    builder->AppendString(Handle<String>::cast(eval_origin));
    builder->AppendCString(", ");  // Expecting source position to follow.
  }

  if (IsNonEmptyString(file_name)) {
    builder->AppendString(Handle<String>::cast(file_name));
  } else {
    // Source code does not originate from a file and is not native, but we
    // can still get the source position inside the source string, e.g. in
    // an eval string.
    builder->AppendCString("<anonymous>");
  }

  int line_number = StackTraceFrame::GetLineNumber(frame);
  if (line_number != Message::kNoLineNumberInfo) {
    builder->AppendCharacter(':');
    builder->AppendInt(line_number);

    int column_number = StackTraceFrame::GetColumnNumber(frame);
    if (column_number != Message::kNoColumnInfo) {
      builder->AppendCharacter(':');
      builder->AppendInt(column_number);
    }
  }
}

int StringIndexOf(Isolate* isolate, Handle<String> subject,
                  Handle<String> pattern) {
  if (pattern->length() > subject->length()) return -1;
  return String::IndexOf(isolate, subject, pattern, 0);
}

// Returns true iff
// 1. the subject ends with '.' + pattern, or
// 2. subject == pattern.
bool StringEndsWithMethodName(Isolate* isolate, Handle<String> subject,
                              Handle<String> pattern) {
  if (String::Equals(isolate, subject, pattern)) return true;

  FlatStringReader subject_reader(isolate, String::Flatten(isolate, subject));
  FlatStringReader pattern_reader(isolate, String::Flatten(isolate, pattern));

  int pattern_index = pattern_reader.length() - 1;
  int subject_index = subject_reader.length() - 1;
  for (int i = 0; i <= pattern_reader.length(); i++) {  // Iterate over len + 1.
    if (subject_index < 0) {
      return false;
    }

    const uc32 subject_char = subject_reader.Get(subject_index);
    if (i == pattern_reader.length()) {
      if (subject_char != '.') return false;
    } else if (subject_char != pattern_reader.Get(pattern_index)) {
      return false;
    }

    pattern_index--;
    subject_index--;
  }

  return true;
}

void AppendMethodCall(Isolate* isolate, Handle<StackTraceFrame> frame,
                      IncrementalStringBuilder* builder) {
  Handle<Object> type_name = StackTraceFrame::GetTypeName(frame);
  Handle<Object> method_name = StackTraceFrame::GetMethodName(frame);
  Handle<Object> function_name = StackTraceFrame::GetFunctionName(frame);

  if (IsNonEmptyString(function_name)) {
    Handle<String> function_string = Handle<String>::cast(function_name);
    if (IsNonEmptyString(type_name)) {
      Handle<String> type_string = Handle<String>::cast(type_name);
      bool starts_with_type_name =
          (StringIndexOf(isolate, function_string, type_string) == 0);
      if (!starts_with_type_name) {
        builder->AppendString(type_string);
        builder->AppendCharacter('.');
      }
    }
    builder->AppendString(function_string);

    if (IsNonEmptyString(method_name)) {
      Handle<String> method_string = Handle<String>::cast(method_name);
      if (!StringEndsWithMethodName(isolate, function_string, method_string)) {
        builder->AppendCString(" [as ");
        builder->AppendString(method_string);
        builder->AppendCharacter(']');
      }
    }
  } else {
    if (IsNonEmptyString(type_name)) {
      builder->AppendString(Handle<String>::cast(type_name));
      builder->AppendCharacter('.');
    }
    if (IsNonEmptyString(method_name)) {
      builder->AppendString(Handle<String>::cast(method_name));
    } else {
      builder->AppendCString("<anonymous>");
    }
  }
}

void SerializeJSStackFrame(
    Isolate* isolate, Handle<StackTraceFrame> frame,
    IncrementalStringBuilder& builder  // NOLINT(runtime/references)
) {
  Handle<Object> function_name = StackTraceFrame::GetFunctionName(frame);

  const bool is_toplevel = StackTraceFrame::IsToplevel(frame);
  const bool is_async = StackTraceFrame::IsAsync(frame);
  const bool is_promise_all = StackTraceFrame::IsPromiseAll(frame);
  const bool is_constructor = StackTraceFrame::IsConstructor(frame);
  // Note: Keep the {is_method_call} predicate in sync with the corresponding
  //       predicate in factory.cc where the StackFrameInfo is created.
  //       Otherwise necessary fields for serialzing this frame might be
  //       missing.
  const bool is_method_call = !(is_toplevel || is_constructor);

  if (is_async) {
    builder.AppendCString("async ");
  }
  if (is_promise_all) {
    builder.AppendCString("Promise.all (index ");
    builder.AppendInt(StackTraceFrame::GetPromiseAllIndex(frame));
    builder.AppendCString(")");
    return;
  }
  if (is_method_call) {
    AppendMethodCall(isolate, frame, &builder);
  } else if (is_constructor) {
    builder.AppendCString("new ");
    if (IsNonEmptyString(function_name)) {
      builder.AppendString(Handle<String>::cast(function_name));
    } else {
      builder.AppendCString("<anonymous>");
    }
  } else if (IsNonEmptyString(function_name)) {
    builder.AppendString(Handle<String>::cast(function_name));
  } else {
    AppendFileLocation(isolate, frame, &builder);
    return;
  }

  builder.AppendCString(" (");
  AppendFileLocation(isolate, frame, &builder);
  builder.AppendCString(")");
}

void SerializeAsmJsWasmStackFrame(
    Isolate* isolate, Handle<StackTraceFrame> frame,
    IncrementalStringBuilder& builder  // NOLINT(runtime/references)
) {
  // The string should look exactly as the respective javascript frame string.
  // Keep this method in line to
  // JSStackFrame::ToString(IncrementalStringBuilder&).
  Handle<Object> function_name = StackTraceFrame::GetFunctionName(frame);

  if (IsNonEmptyString(function_name)) {
    builder.AppendString(Handle<String>::cast(function_name));
    builder.AppendCString(" (");
  }

  AppendFileLocation(isolate, frame, &builder);

  if (IsNonEmptyString(function_name)) builder.AppendCString(")");

  return;
}

void SerializeWasmStackFrame(
    Isolate* isolate, Handle<StackTraceFrame> frame,
    IncrementalStringBuilder& builder  // NOLINT(runtime/references)
) {
  Handle<Object> module_name = StackTraceFrame::GetWasmModuleName(frame);
  Handle<Object> function_name = StackTraceFrame::GetFunctionName(frame);
  const bool has_name = !module_name->IsNull() || !function_name->IsNull();
  if (has_name) {
    if (module_name->IsNull()) {
      builder.AppendString(Handle<String>::cast(function_name));
    } else {
      builder.AppendString(Handle<String>::cast(module_name));
      if (!function_name->IsNull()) {
        builder.AppendCString(".");
        builder.AppendString(Handle<String>::cast(function_name));
      }
    }
    builder.AppendCString(" (");
  }

  const int wasm_func_index = StackTraceFrame::GetLineNumber(frame);

  builder.AppendCString("wasm-function[");
  builder.AppendInt(wasm_func_index);
  builder.AppendCString("]:");

  char buffer[16];
  SNPrintF(ArrayVector(buffer), "0x%x",
           StackTraceFrame::GetColumnNumber(frame));
  builder.AppendCString(buffer);

  if (has_name) builder.AppendCString(")");
}

}  // namespace

void SerializeStackTraceFrame(
    Isolate* isolate, Handle<StackTraceFrame> frame,
    IncrementalStringBuilder& builder  // NOLINT(runtime/references)
) {
  // Ordering here is important, as AsmJs frames are also marked as Wasm.
  if (StackTraceFrame::IsAsmJsWasm(frame)) {
    SerializeAsmJsWasmStackFrame(isolate, frame, builder);
  } else if (StackTraceFrame::IsWasm(frame)) {
    SerializeWasmStackFrame(isolate, frame, builder);
  } else {
    SerializeJSStackFrame(isolate, frame, builder);
  }
}

MaybeHandle<String> SerializeStackTraceFrame(Isolate* isolate,
                                             Handle<StackTraceFrame> frame) {
  IncrementalStringBuilder builder(isolate);
  SerializeStackTraceFrame(isolate, frame, builder);
  return builder.Finish();
}

}  // namespace internal
}  // namespace v8
