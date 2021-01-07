// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The infrastructure used for (localized) message reporting in V8.
//
// Note: there's a big unresolved issue about ownership of the data
// structures used by this framework.

#ifndef V8_EXECUTION_MESSAGES_H_
#define V8_EXECUTION_MESSAGES_H_

#include <memory>

#include "src/base/optional.h"
#include "src/common/message-template.h"
#include "src/handles/handles.h"

namespace v8 {
namespace internal {
namespace wasm {
class WasmCode;
}  // namespace wasm

// Forward declarations.
class AbstractCode;
class FrameArray;
class JSMessageObject;
class LookupIterator;
class PrimitiveHeapObject;
class SharedFunctionInfo;
class SourceInfo;
class WasmInstanceObject;

class V8_EXPORT_PRIVATE MessageLocation {
 public:
  // Constructors for when source positions are already known.
  // TODO(delphick): Collapse to a single constructor with a default parameter
  // when we stop using the GCC that requires this separation.
  MessageLocation(Handle<Script> script, int start_pos, int end_pos);
  MessageLocation(Handle<Script> script, int start_pos, int end_pos,
                  Handle<SharedFunctionInfo> shared);
  // Constructor for when source positions were not collected but which can be
  // reconstructed from the SharedFuncitonInfo and bytecode offset.
  MessageLocation(Handle<Script> script, Handle<SharedFunctionInfo> shared,
                  int bytecode_offset);
  MessageLocation();

  Handle<Script> script() const { return script_; }
  int start_pos() const { return start_pos_; }
  int end_pos() const { return end_pos_; }
  int bytecode_offset() const { return bytecode_offset_; }
  Handle<SharedFunctionInfo> shared() const { return shared_; }

 private:
  Handle<Script> script_;
  int start_pos_;
  int end_pos_;
  int bytecode_offset_;
  Handle<SharedFunctionInfo> shared_;
};

class StackFrameBase {
 public:
  virtual ~StackFrameBase() = default;

  virtual Handle<Object> GetReceiver() const = 0;
  virtual Handle<Object> GetFunction() const = 0;

  virtual Handle<Object> GetFileName() = 0;
  virtual Handle<PrimitiveHeapObject> GetFunctionName() = 0;
  virtual Handle<Object> GetScriptNameOrSourceUrl() = 0;
  virtual Handle<PrimitiveHeapObject> GetMethodName() = 0;
  virtual Handle<PrimitiveHeapObject> GetTypeName() = 0;
  virtual Handle<PrimitiveHeapObject> GetEvalOrigin();
  virtual Handle<PrimitiveHeapObject> GetWasmModuleName();
  virtual Handle<HeapObject> GetWasmInstance();

  // Returns the script ID if one is attached, -1 otherwise.
  int GetScriptId() const;

  virtual int GetPosition() const = 0;
  // Return 1-based line number, including line offset.
  virtual int GetLineNumber() = 0;
  // Return 1-based column number, including column offset if first line.
  virtual int GetColumnNumber() = 0;
  // Return 0-based Wasm function index. Returns -1 for non-Wasm frames.
  virtual int GetWasmFunctionIndex();

  virtual int GetEnclosingColumnNumber() = 0;
  virtual int GetEnclosingLineNumber() = 0;

  // Returns the index of the rejected promise in the Promise combinator input,
  // or -1 if this frame is not a Promise combinator frame.
  virtual int GetPromiseIndex() const = 0;

  virtual bool IsNative() = 0;
  virtual bool IsToplevel() = 0;
  virtual bool IsEval();
  virtual bool IsAsync() const = 0;
  virtual bool IsPromiseAll() const = 0;
  virtual bool IsPromiseAny() const = 0;
  virtual bool IsConstructor() = 0;
  virtual bool IsStrict() const = 0;

  // Used to signal that the requested field is unknown.
  static const int kNone = -1;

 protected:
  StackFrameBase() = default;
  explicit StackFrameBase(Isolate* isolate) : isolate_(isolate) {}
  Isolate* isolate_;

 private:
  virtual bool HasScript() const = 0;
  virtual Handle<Script> GetScript() const = 0;
};

class JSStackFrame : public StackFrameBase {
 public:
  JSStackFrame(Isolate* isolate, Handle<Object> receiver,
               Handle<JSFunction> function, Handle<AbstractCode> code,
               int offset);
  ~JSStackFrame() override = default;

  Handle<Object> GetReceiver() const override { return receiver_; }
  Handle<Object> GetFunction() const override;

  Handle<Object> GetFileName() override;
  Handle<PrimitiveHeapObject> GetFunctionName() override;
  Handle<Object> GetScriptNameOrSourceUrl() override;
  Handle<PrimitiveHeapObject> GetMethodName() override;
  Handle<PrimitiveHeapObject> GetTypeName() override;

  int GetPosition() const override;
  int GetLineNumber() override;
  int GetColumnNumber() override;

  int GetEnclosingColumnNumber() override;
  int GetEnclosingLineNumber() override;

  int GetPromiseIndex() const override;

  bool IsNative() override;
  bool IsToplevel() override;
  bool IsAsync() const override { return is_async_; }
  bool IsPromiseAll() const override { return is_promise_all_; }
  bool IsPromiseAny() const override { return is_promise_any_; }
  bool IsConstructor() override { return is_constructor_; }
  bool IsStrict() const override { return is_strict_; }

 private:
  JSStackFrame() = default;
  void FromFrameArray(Isolate* isolate, Handle<FrameArray> array, int frame_ix);

  bool HasScript() const override;
  Handle<Script> GetScript() const override;

  Handle<Object> receiver_;
  Handle<JSFunction> function_;
  Handle<AbstractCode> code_;
  int offset_;
  mutable base::Optional<int> cached_position_;

  bool is_async_ : 1;
  bool is_constructor_ : 1;
  bool is_promise_all_ : 1;
  bool is_promise_any_ : 1;
  bool is_strict_ : 1;

  friend class FrameArrayIterator;
};

class WasmStackFrame : public StackFrameBase {
 public:
  ~WasmStackFrame() override = default;

  Handle<Object> GetReceiver() const override;
  Handle<Object> GetFunction() const override;

  Handle<Object> GetFileName() override;
  Handle<PrimitiveHeapObject> GetFunctionName() override;
  Handle<Object> GetScriptNameOrSourceUrl() override;
  Handle<PrimitiveHeapObject> GetMethodName() override { return Null(); }
  Handle<PrimitiveHeapObject> GetTypeName() override { return Null(); }
  Handle<PrimitiveHeapObject> GetWasmModuleName() override;
  Handle<HeapObject> GetWasmInstance() override;

  int GetPosition() const override;
  int GetLineNumber() override { return 0; }
  int GetColumnNumber() override;
  int GetEnclosingColumnNumber() override;
  int GetEnclosingLineNumber() override { return 0; }
  int GetWasmFunctionIndex() override { return wasm_func_index_; }

  int GetPromiseIndex() const override { return GetPosition(); }

  bool IsNative() override { return false; }
  bool IsToplevel() override { return false; }
  bool IsAsync() const override { return false; }
  bool IsPromiseAll() const override { return false; }
  bool IsPromiseAny() const override { return false; }
  bool IsConstructor() override { return false; }
  bool IsStrict() const override { return false; }
  bool IsInterpreted() const { return code_ == nullptr; }

 protected:
  Handle<PrimitiveHeapObject> Null() const;

  bool HasScript() const override;
  Handle<Script> GetScript() const override;

  Handle<WasmInstanceObject> wasm_instance_;
  uint32_t wasm_func_index_;
  wasm::WasmCode* code_;  // null for interpreted frames.
  int offset_;

 private:
  int GetModuleOffset() const;

  WasmStackFrame() = default;
  void FromFrameArray(Isolate* isolate, Handle<FrameArray> array, int frame_ix);

  friend class FrameArrayIterator;
  friend class AsmJsWasmStackFrame;
};

class AsmJsWasmStackFrame : public WasmStackFrame {
 public:
  ~AsmJsWasmStackFrame() override = default;

  Handle<Object> GetReceiver() const override;
  Handle<Object> GetFunction() const override;

  Handle<Object> GetFileName() override;
  Handle<Object> GetScriptNameOrSourceUrl() override;

  int GetPosition() const override;
  int GetLineNumber() override;
  int GetColumnNumber() override;

  int GetEnclosingColumnNumber() override;
  int GetEnclosingLineNumber() override;

 private:
  friend class FrameArrayIterator;
  AsmJsWasmStackFrame() = default;
  void FromFrameArray(Isolate* isolate, Handle<FrameArray> array, int frame_ix);

  bool is_at_number_conversion_;
};

class FrameArrayIterator {
 public:
  FrameArrayIterator(Isolate* isolate, Handle<FrameArray> array,
                     int frame_ix = 0);

  StackFrameBase* Frame();

  bool HasFrame() const;
  void Advance();

 private:
  Isolate* isolate_;

  Handle<FrameArray> array_;
  int frame_ix_;

  WasmStackFrame wasm_frame_;
  AsmJsWasmStackFrame asm_wasm_frame_;
  JSStackFrame js_frame_;
};

// Determines how stack trace collection skips frames.
enum FrameSkipMode {
  // Unconditionally skips the first frame. Used e.g. when the Error constructor
  // is called, in which case the first frame is always a BUILTIN_EXIT frame.
  SKIP_FIRST,
  // Skip all frames until a specified caller function is seen.
  SKIP_UNTIL_SEEN,
  SKIP_NONE,
};

class ErrorUtils : public AllStatic {
 public:
  // |kNone| is useful when you don't need the stack information at all, for
  // example when creating a deserialized error.
  enum class StackTraceCollection { kDetailed, kSimple, kNone };
  static MaybeHandle<JSObject> Construct(Isolate* isolate,
                                         Handle<JSFunction> target,
                                         Handle<Object> new_target,
                                         Handle<Object> message);
  static MaybeHandle<JSObject> Construct(
      Isolate* isolate, Handle<JSFunction> target, Handle<Object> new_target,
      Handle<Object> message, FrameSkipMode mode, Handle<Object> caller,
      StackTraceCollection stack_trace_collection);

  static MaybeHandle<String> ToString(Isolate* isolate, Handle<Object> recv);

  static Handle<JSObject> MakeGenericError(
      Isolate* isolate, Handle<JSFunction> constructor, MessageTemplate index,
      Handle<Object> arg0, Handle<Object> arg1, Handle<Object> arg2,
      FrameSkipMode mode);

  // Formats a textual stack trace from the given structured stack trace.
  // Note that this can call arbitrary JS code through Error.prepareStackTrace.
  static MaybeHandle<Object> FormatStackTrace(Isolate* isolate,
                                              Handle<JSObject> error,
                                              Handle<Object> stack_trace);

  static Handle<JSObject> NewIteratorError(Isolate* isolate,
                                           Handle<Object> source);
  static Handle<JSObject> NewCalledNonCallableError(Isolate* isolate,
                                                    Handle<Object> source);
  static Handle<JSObject> NewConstructedNonConstructable(Isolate* isolate,
                                                         Handle<Object> source);
  // Returns the Exception sentinel.
  static Object ThrowSpreadArgError(Isolate* isolate, MessageTemplate id,
                                    Handle<Object> object);
  // Returns the Exception sentinel.
  static Object ThrowLoadFromNullOrUndefined(Isolate* isolate,
                                             Handle<Object> object,
                                             MaybeHandle<Object> key);
};

class MessageFormatter {
 public:
  V8_EXPORT_PRIVATE static const char* TemplateString(MessageTemplate index);

  V8_EXPORT_PRIVATE static MaybeHandle<String> Format(Isolate* isolate,
                                                      MessageTemplate index,
                                                      Handle<String> arg0,
                                                      Handle<String> arg1,
                                                      Handle<String> arg2);

  static Handle<String> Format(Isolate* isolate, MessageTemplate index,
                               Handle<Object> arg0,
                               Handle<Object> arg1 = Handle<Object>(),
                               Handle<Object> arg2 = Handle<Object>());
};

// A message handler is a convenience interface for accessing the list
// of message listeners registered in an environment
class MessageHandler {
 public:
  // Returns a message object for the API to use.
  V8_EXPORT_PRIVATE static Handle<JSMessageObject> MakeMessageObject(
      Isolate* isolate, MessageTemplate type, const MessageLocation* location,
      Handle<Object> argument, Handle<FixedArray> stack_frames);

  // Report a formatted message (needs JS allocation).
  V8_EXPORT_PRIVATE static void ReportMessage(Isolate* isolate,
                                              const MessageLocation* loc,
                                              Handle<JSMessageObject> message);

  static void DefaultMessageReport(Isolate* isolate, const MessageLocation* loc,
                                   Handle<Object> message_obj);
  static Handle<String> GetMessage(Isolate* isolate, Handle<Object> data);
  static std::unique_ptr<char[]> GetLocalizedMessage(Isolate* isolate,
                                                     Handle<Object> data);

 private:
  static void ReportMessageNoExceptions(Isolate* isolate,
                                        const MessageLocation* loc,
                                        Handle<Object> message_obj,
                                        v8::Local<v8::Value> api_exception_obj);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_MESSAGES_H_
