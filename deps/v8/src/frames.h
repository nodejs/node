// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FRAMES_H_
#define V8_FRAMES_H_

#include "src/allocation.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/objects.h"
#include "src/objects/code.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {
namespace wasm {
class WasmCode;
}

class AbstractCode;
class Debug;
class ObjectVisitor;
class StringStream;

// Forward declarations.
class ExternalCallbackScope;
class Isolate;
class RootVisitor;
class StackFrameIteratorBase;
class ThreadLocalTop;
class WasmInstanceObject;

class InnerPointerToCodeCache {
 public:
  struct InnerPointerToCodeCacheEntry {
    Address inner_pointer;
    Code* code;
    SafepointEntry safepoint_entry;
  };

  explicit InnerPointerToCodeCache(Isolate* isolate) : isolate_(isolate) {
    Flush();
  }

  void Flush() {
    memset(&cache_[0], 0, sizeof(cache_));
  }

  InnerPointerToCodeCacheEntry* GetCacheEntry(Address inner_pointer);

 private:
  InnerPointerToCodeCacheEntry* cache(int index) { return &cache_[index]; }

  Isolate* isolate_;

  static const int kInnerPointerToCodeCacheSize = 1024;
  InnerPointerToCodeCacheEntry cache_[kInnerPointerToCodeCacheSize];

  DISALLOW_COPY_AND_ASSIGN(InnerPointerToCodeCache);
};


class StackHandlerConstants : public AllStatic {
 public:
  static const int kNextOffset = 0 * kPointerSize;
  static const int kPaddingOffset = 1 * kPointerSize;

  static const int kSize = kPaddingOffset + kPointerSize;
  static const int kSlotCount = kSize >> kPointerSizeLog2;
};


class StackHandler BASE_EMBEDDED {
 public:
  // Get the address of this stack handler.
  inline Address address() const;

  // Get the next stack handler in the chain.
  inline StackHandler* next() const;

  // Conversion support.
  static inline StackHandler* FromAddress(Address address);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StackHandler);
};

#define STACK_FRAME_TYPE_LIST(V)                                          \
  V(ENTRY, EntryFrame)                                                    \
  V(CONSTRUCT_ENTRY, ConstructEntryFrame)                                 \
  V(EXIT, ExitFrame)                                                      \
  V(OPTIMIZED, OptimizedFrame)                                            \
  V(WASM_COMPILED, WasmCompiledFrame)                                     \
  V(WASM_TO_JS, WasmToJsFrame)                                            \
  V(WASM_TO_WASM, WasmToWasmFrame)                                        \
  V(JS_TO_WASM, JsToWasmFrame)                                            \
  V(WASM_INTERPRETER_ENTRY, WasmInterpreterEntryFrame)                    \
  V(C_WASM_ENTRY, CWasmEntryFrame)                                        \
  V(INTERPRETED, InterpretedFrame)                                        \
  V(STUB, StubFrame)                                                      \
  V(BUILTIN_CONTINUATION, BuiltinContinuationFrame)                       \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION, JavaScriptBuiltinContinuationFrame) \
  V(INTERNAL, InternalFrame)                                              \
  V(CONSTRUCT, ConstructFrame)                                            \
  V(ARGUMENTS_ADAPTOR, ArgumentsAdaptorFrame)                             \
  V(BUILTIN, BuiltinFrame)                                                \
  V(BUILTIN_EXIT, BuiltinExitFrame)                                       \
  V(NATIVE, NativeFrame)

// Abstract base class for all stack frames.
class StackFrame BASE_EMBEDDED {
 public:
#define DECLARE_TYPE(type, ignore) type,
  enum Type {
    NONE = 0,
    STACK_FRAME_TYPE_LIST(DECLARE_TYPE)
    NUMBER_OF_TYPES,
    // Used by FrameScope to indicate that the stack frame is constructed
    // manually and the FrameScope does not need to emit code.
    MANUAL
  };
#undef DECLARE_TYPE

  // Opaque data type for identifying stack frames. Used extensively
  // by the debugger.
  // ID_MIN_VALUE and ID_MAX_VALUE are specified to ensure that enumeration type
  // has correct value range (see Issue 830 for more details).
  enum Id {
    ID_MIN_VALUE = kMinInt,
    ID_MAX_VALUE = kMaxInt,
    NO_ID = 0
  };

  // Used to mark the outermost JS entry frame.
  //
  // The mark is an opaque value that should be pushed onto the stack directly,
  // carefully crafted to not be interpreted as a tagged pointer.
  enum JsFrameMarker {
    INNER_JSENTRY_FRAME = (0 << kSmiTagSize) | kSmiTag,
    OUTERMOST_JSENTRY_FRAME = (1 << kSmiTagSize) | kSmiTag
  };
  STATIC_ASSERT((INNER_JSENTRY_FRAME & kHeapObjectTagMask) != kHeapObjectTag);
  STATIC_ASSERT((OUTERMOST_JSENTRY_FRAME & kHeapObjectTagMask) !=
                kHeapObjectTag);

  struct State {
    Address sp = nullptr;
    Address fp = nullptr;
    Address* pc_address = nullptr;
    Address* callee_pc_address = nullptr;
    Address* constant_pool_address = nullptr;
  };

  // Convert a stack frame type to a marker that can be stored on the stack.
  //
  // The marker is an opaque value, not intended to be interpreted in any way
  // except being checked by IsTypeMarker or converted by MarkerToType.
  // It has the same tagging as Smis, so any marker value that does not pass
  // IsTypeMarker can instead be interpreted as a tagged pointer.
  //
  // Note that the marker is not a Smi: Smis on 64-bit architectures are stored
  // in the top 32 bits of a 64-bit value, which in turn makes them expensive
  // (in terms of code/instruction size) to push as immediates onto the stack.
  static int32_t TypeToMarker(Type type) {
    DCHECK_GE(type, 0);
    return (type << kSmiTagSize) | kSmiTag;
  }

  // Convert a marker back to a stack frame type.
  //
  // Unlike the return value of TypeToMarker, this takes an intptr_t, as that is
  // the type of the value on the stack.
  static Type MarkerToType(intptr_t marker) {
    DCHECK(IsTypeMarker(marker));
    return static_cast<Type>(marker >> kSmiTagSize);
  }

  // Check if a marker is a stack frame type marker or a tagged pointer.
  //
  // Returns true if the given marker is tagged as a stack frame type marker,
  // and should be converted back to a stack frame type using MarkerToType.
  // Otherwise, the value is a tagged function pointer.
  static bool IsTypeMarker(intptr_t function_or_marker) {
    return (function_or_marker & kSmiTagMask) == kSmiTag;
  }

  // Copy constructor; it breaks the connection to host iterator
  // (as an iterator usually lives on stack).
  StackFrame(const StackFrame& original) {
    this->state_ = original.state_;
    this->iterator_ = nullptr;
    this->isolate_ = original.isolate_;
  }

  // Type testers.
  bool is_entry() const { return type() == ENTRY; }
  bool is_construct_entry() const { return type() == CONSTRUCT_ENTRY; }
  bool is_exit() const { return type() == EXIT; }
  bool is_optimized() const { return type() == OPTIMIZED; }
  bool is_interpreted() const { return type() == INTERPRETED; }
  bool is_wasm_compiled() const { return type() == WASM_COMPILED; }
  bool is_wasm_to_js() const { return type() == WASM_TO_JS; }
  bool is_js_to_wasm() const { return type() == JS_TO_WASM; }
  bool is_wasm_interpreter_entry() const {
    return type() == WASM_INTERPRETER_ENTRY;
  }
  bool is_arguments_adaptor() const { return type() == ARGUMENTS_ADAPTOR; }
  bool is_builtin() const { return type() == BUILTIN; }
  bool is_internal() const { return type() == INTERNAL; }
  bool is_builtin_continuation() const {
    return type() == BUILTIN_CONTINUATION;
  }
  bool is_java_script_builtin_continuation() const {
    return type() == JAVA_SCRIPT_BUILTIN_CONTINUATION;
  }
  bool is_construct() const { return type() == CONSTRUCT; }
  bool is_builtin_exit() const { return type() == BUILTIN_EXIT; }
  virtual bool is_standard() const { return false; }

  bool is_java_script() const {
    Type type = this->type();
    return (type == OPTIMIZED) || (type == INTERPRETED) || (type == BUILTIN) ||
           (type == JAVA_SCRIPT_BUILTIN_CONTINUATION);
  }
  bool is_wasm() const {
    Type type = this->type();
    return type == WASM_COMPILED || type == WASM_INTERPRETER_ENTRY;
  }

  // Accessors.
  Address sp() const { return state_.sp; }
  Address fp() const { return state_.fp; }
  Address callee_pc() const {
    return state_.callee_pc_address ? *state_.callee_pc_address : nullptr;
  }
  Address caller_sp() const { return GetCallerStackPointer(); }

  // If this frame is optimized and was dynamically aligned return its old
  // unaligned frame pointer.  When the frame is deoptimized its FP will shift
  // up one word and become unaligned.
  Address UnpaddedFP() const;

  Address pc() const { return *pc_address(); }
  void set_pc(Address pc) { *pc_address() = pc; }

  Address constant_pool() const { return *constant_pool_address(); }
  void set_constant_pool(Address constant_pool) {
    *constant_pool_address() = constant_pool;
  }

  Address* pc_address() const { return state_.pc_address; }

  Address* constant_pool_address() const {
    return state_.constant_pool_address;
  }

  // Get the id of this stack frame.
  Id id() const { return static_cast<Id>(OffsetFrom(caller_sp())); }

  // Get the top handler from the current stack iterator.
  inline StackHandler* top_handler() const;

  // Get the type of this frame.
  virtual Type type() const = 0;

  // Get the code associated with this frame.
  // This method could be called during marking phase of GC.
  virtual Code* unchecked_code() const = 0;

  // Search for the code associated with this frame.
  Code* LookupCode() const;

  virtual void Iterate(RootVisitor* v) const = 0;
  static void IteratePc(RootVisitor* v, Address* pc_address,
                        Address* constant_pool_address, Code* holder);

  // Sets a callback function for return-address rewriting profilers
  // to resolve the location of a return address to the location of the
  // profiler's stashed return address.
  static void SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver resolver);

  // Resolves pc_address through the resolution address function if one is set.
  static inline Address* ResolveReturnAddressLocation(Address* pc_address);

  // Printing support.
  enum PrintMode { OVERVIEW, DETAILS };
  virtual void Print(StringStream* accumulator,
                     PrintMode mode,
                     int index) const { }

  Isolate* isolate() const { return isolate_; }

  void operator=(const StackFrame& original) = delete;

 protected:
  inline explicit StackFrame(StackFrameIteratorBase* iterator);
  virtual ~StackFrame() { }

  // Compute the stack pointer for the calling frame.
  virtual Address GetCallerStackPointer() const = 0;

  // Compute the stack frame type for the given state.
  static Type ComputeType(const StackFrameIteratorBase* iterator, State* state);

#ifdef DEBUG
  bool can_access_heap_objects() const;
#endif

 private:
  const StackFrameIteratorBase* iterator_;
  Isolate* isolate_;
  State state_;

  static ReturnAddressLocationResolver return_address_location_resolver_;

  // Fill in the state of the calling frame.
  virtual void ComputeCallerState(State* state) const = 0;

  // Get the type and the state of the calling frame.
  virtual Type GetCallerState(State* state) const;

  static const intptr_t kIsolateTag = 1;

  friend class StackFrameIterator;
  friend class StackFrameIteratorBase;
  friend class StackHandlerIterator;
  friend class SafeStackFrameIterator;
};

class NativeFrame : public StackFrame {
 public:
  Type type() const override { return NATIVE; }

  Code* unchecked_code() const override { return nullptr; }

  // Garbage collection support.
  void Iterate(RootVisitor* v) const override {}

 protected:
  inline explicit NativeFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

 private:
  void ComputeCallerState(State* state) const override;

  friend class StackFrameIteratorBase;
};

// Entry frames are used to enter JavaScript execution from C.
class EntryFrame: public StackFrame {
 public:
  Type type() const override { return ENTRY; }

  Code* unchecked_code() const override;

  // Garbage collection support.
  void Iterate(RootVisitor* v) const override;

  static EntryFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_entry());
    return static_cast<EntryFrame*>(frame);
  }

 protected:
  inline explicit EntryFrame(StackFrameIteratorBase* iterator);

  // The caller stack pointer for entry frames is always zero. The
  // real information about the caller frame is available through the
  // link to the top exit frame.
  Address GetCallerStackPointer() const override { return 0; }

 private:
  void ComputeCallerState(State* state) const override;
  Type GetCallerState(State* state) const override;

  friend class StackFrameIteratorBase;
};

class ConstructEntryFrame : public EntryFrame {
 public:
  Type type() const override { return CONSTRUCT_ENTRY; }

  Code* unchecked_code() const override;

  static ConstructEntryFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_construct_entry());
    return static_cast<ConstructEntryFrame*>(frame);
  }

 protected:
  inline explicit ConstructEntryFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};


// Exit frames are used to exit JavaScript execution and go to C.
class ExitFrame: public StackFrame {
 public:
  Type type() const override { return EXIT; }

  Code* unchecked_code() const override;

  Object*& code_slot() const;

  // Garbage collection support.
  void Iterate(RootVisitor* v) const override;

  static ExitFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_exit());
    return static_cast<ExitFrame*>(frame);
  }

  // Compute the state and type of an exit frame given a frame
  // pointer. Used when constructing the first stack frame seen by an
  // iterator and the frames following entry frames.
  static Type GetStateForFramePointer(Address fp, State* state);
  static Address ComputeStackPointer(Address fp);
  static StackFrame::Type ComputeFrameType(Address fp);
  static void FillState(Address fp, Address sp, State* state);

 protected:
  inline explicit ExitFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

 private:
  void ComputeCallerState(State* state) const override;

  friend class StackFrameIteratorBase;
};

// Builtin exit frames are a special case of exit frames, which are used
// whenever C++ builtins (e.g., Math.acos) are called. Their main purpose is
// to allow such builtins to appear in stack traces.
class BuiltinExitFrame : public ExitFrame {
 public:
  Type type() const override { return BUILTIN_EXIT; }

  static BuiltinExitFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_builtin_exit());
    return static_cast<BuiltinExitFrame*>(frame);
  }

  JSFunction* function() const;
  Object* receiver() const;

  bool IsConstructor() const;

  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

 protected:
  inline explicit BuiltinExitFrame(StackFrameIteratorBase* iterator);

 private:
  Object* GetParameter(int i) const;
  int ComputeParametersCount() const;

  inline Object* receiver_slot_object() const;
  inline Object* argc_slot_object() const;
  inline Object* target_slot_object() const;
  inline Object* new_target_slot_object() const;

  friend class StackFrameIteratorBase;
};

class StandardFrame;

class FrameSummary BASE_EMBEDDED {
 public:
// Subclasses for the different summary kinds:
#define FRAME_SUMMARY_VARIANTS(F)                                             \
  F(JAVA_SCRIPT, JavaScriptFrameSummary, java_script_summary_, JavaScript)    \
  F(WASM_COMPILED, WasmCompiledFrameSummary, wasm_compiled_summary_,          \
    WasmCompiled)                                                             \
  F(WASM_INTERPRETED, WasmInterpretedFrameSummary, wasm_interpreted_summary_, \
    WasmInterpreted)

#define FRAME_SUMMARY_KIND(kind, type, field, desc) kind,
  enum Kind { FRAME_SUMMARY_VARIANTS(FRAME_SUMMARY_KIND) };
#undef FRAME_SUMMARY_KIND

  class FrameSummaryBase {
   public:
    FrameSummaryBase(Isolate* isolate, Kind kind)
        : isolate_(isolate), kind_(kind) {}
    Isolate* isolate() const { return isolate_; }
    Kind kind() const { return kind_; }

   private:
    Isolate* isolate_;
    Kind kind_;
  };

  class JavaScriptFrameSummary : public FrameSummaryBase {
   public:
    JavaScriptFrameSummary(Isolate* isolate, Object* receiver,
                           JSFunction* function, AbstractCode* abstract_code,
                           int code_offset, bool is_constructor);

    Handle<Object> receiver() const { return receiver_; }
    Handle<JSFunction> function() const { return function_; }
    Handle<AbstractCode> abstract_code() const { return abstract_code_; }
    int code_offset() const { return code_offset_; }
    bool is_constructor() const { return is_constructor_; }
    bool is_subject_to_debugging() const;
    int SourcePosition() const;
    int SourceStatementPosition() const;
    Handle<Object> script() const;
    Handle<String> FunctionName() const;
    Handle<Context> native_context() const;

   private:
    Handle<Object> receiver_;
    Handle<JSFunction> function_;
    Handle<AbstractCode> abstract_code_;
    int code_offset_;
    bool is_constructor_;
  };

  class WasmFrameSummary : public FrameSummaryBase {
   protected:
    WasmFrameSummary(Isolate*, Kind, Handle<WasmInstanceObject>,
                     bool at_to_number_conversion);

   public:
    Handle<Object> receiver() const;
    uint32_t function_index() const;
    int byte_offset() const;
    bool is_constructor() const { return false; }
    bool is_subject_to_debugging() const { return true; }
    int SourcePosition() const;
    int SourceStatementPosition() const { return SourcePosition(); }
    Handle<Script> script() const;
    Handle<WasmInstanceObject> wasm_instance() const { return wasm_instance_; }
    Handle<String> FunctionName() const;
    Handle<Context> native_context() const;
    bool at_to_number_conversion() const { return at_to_number_conversion_; }

   private:
    Handle<WasmInstanceObject> wasm_instance_;
    bool at_to_number_conversion_;
  };

  class WasmCompiledFrameSummary : public WasmFrameSummary {
   public:
    WasmCompiledFrameSummary(Isolate*, Handle<WasmInstanceObject>,
                             WasmCodeWrapper, int code_offset,
                             bool at_to_number_conversion);
    uint32_t function_index() const;
    WasmCodeWrapper code() const { return code_; }
    int code_offset() const { return code_offset_; }
    int byte_offset() const;
    static int GetWasmSourcePosition(const wasm::WasmCode* code, int offset);

   private:
    WasmCodeWrapper const code_;
    int code_offset_;
  };

  class WasmInterpretedFrameSummary : public WasmFrameSummary {
   public:
    WasmInterpretedFrameSummary(Isolate*, Handle<WasmInstanceObject>,
                                uint32_t function_index, int byte_offset);
    uint32_t function_index() const { return function_index_; }
    int code_offset() const { return byte_offset_; }
    int byte_offset() const { return byte_offset_; }

   private:
    uint32_t function_index_;
    int byte_offset_;
  };

#undef FRAME_SUMMARY_FIELD
#define FRAME_SUMMARY_CONS(kind, type, field, desc) \
  FrameSummary(type summ) : field(summ) {}  // NOLINT
  FRAME_SUMMARY_VARIANTS(FRAME_SUMMARY_CONS)
#undef FRAME_SUMMARY_CONS

  ~FrameSummary();

  static FrameSummary GetTop(const StandardFrame* frame);
  static FrameSummary GetBottom(const StandardFrame* frame);
  static FrameSummary GetSingle(const StandardFrame* frame);
  static FrameSummary Get(const StandardFrame* frame, int index);

  // Dispatched accessors.
  Handle<Object> receiver() const;
  int code_offset() const;
  bool is_constructor() const;
  bool is_subject_to_debugging() const;
  Handle<Object> script() const;
  int SourcePosition() const;
  int SourceStatementPosition() const;
  Handle<String> FunctionName() const;
  Handle<Context> native_context() const;

#define FRAME_SUMMARY_CAST(kind_, type, field, desc)      \
  bool Is##desc() const { return base_.kind() == kind_; } \
  const type& As##desc() const {                          \
    DCHECK_EQ(base_.kind(), kind_);                       \
    return field;                                         \
  }
  FRAME_SUMMARY_VARIANTS(FRAME_SUMMARY_CAST)
#undef FRAME_SUMMARY_CAST

  bool IsWasm() const { return IsWasmCompiled() || IsWasmInterpreted(); }
  const WasmFrameSummary& AsWasm() const {
    if (IsWasmCompiled()) return AsWasmCompiled();
    return AsWasmInterpreted();
  }

 private:
#define FRAME_SUMMARY_FIELD(kind, type, field, desc) type field;
  union {
    FrameSummaryBase base_;
    FRAME_SUMMARY_VARIANTS(FRAME_SUMMARY_FIELD)
  };
};

class StandardFrame : public StackFrame {
 public:
  // Testers.
  bool is_standard() const override { return true; }

  // Accessors.
  virtual Object* receiver() const;
  virtual Script* script() const;
  virtual Object* context() const;
  virtual int position() const;

  // Access the expressions in the stack frame including locals.
  inline Object* GetExpression(int index) const;
  inline void SetExpression(int index, Object* value);
  int ComputeExpressionsCount() const;

  // Access the parameters.
  virtual Object* GetParameter(int index) const;
  virtual int ComputeParametersCount() const;

  // Check if this frame is a constructor frame invoked through 'new'.
  virtual bool IsConstructor() const;

  // Build a list with summaries for this frame including all inlined frames.
  // The functions are ordered bottom-to-top (i.e. summaries.last() is the
  // top-most activation; caller comes before callee).
  virtual void Summarize(std::vector<FrameSummary>* frames) const;

  static StandardFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_standard());
    return static_cast<StandardFrame*>(frame);
  }

 protected:
  inline explicit StandardFrame(StackFrameIteratorBase* iterator);

  void ComputeCallerState(State* state) const override;

  // Accessors.
  inline Address caller_fp() const;
  inline Address caller_pc() const;

  // Computes the address of the PC field in the standard frame given
  // by the provided frame pointer.
  static inline Address ComputePCAddress(Address fp);

  // Computes the address of the constant pool  field in the standard
  // frame given by the provided frame pointer.
  static inline Address ComputeConstantPoolAddress(Address fp);

  // Iterate over expression stack including stack handlers, locals,
  // and parts of the fixed part including context and code fields.
  void IterateExpressions(RootVisitor* v) const;

  // Returns the address of the n'th expression stack element.
  virtual Address GetExpressionAddress(int n) const;

  // Determines if the standard frame for the given frame pointer is
  // an arguments adaptor frame.
  static inline bool IsArgumentsAdaptorFrame(Address fp);

  // Determines if the standard frame for the given frame pointer is a
  // construct frame.
  static inline bool IsConstructFrame(Address fp);

  // Used by OptimizedFrames and StubFrames.
  void IterateCompiledFrame(RootVisitor* v) const;

 private:
  friend class StackFrame;
  friend class SafeStackFrameIterator;
};

class JavaScriptFrame : public StandardFrame {
 public:
  Type type() const override = 0;

  void Summarize(std::vector<FrameSummary>* frames) const override;

  // Accessors.
  virtual JSFunction* function() const;
  Object* unchecked_function() const;
  Object* receiver() const override;
  Object* context() const override;
  Script* script() const override;

  inline void set_receiver(Object* value);

  // Access the parameters.
  inline Address GetParameterSlot(int index) const;
  Object* GetParameter(int index) const override;
  int ComputeParametersCount() const override;

  // Debugger access.
  void SetParameterValue(int index, Object* value) const;

  // Check if this frame is a constructor frame invoked through 'new'.
  bool IsConstructor() const override;

  // Determines whether this frame includes inlined activations. To get details
  // about the inlined frames use {GetFunctions} and {Summarize}.
  bool HasInlinedFrames() const;

  // Check if this frame has "adapted" arguments in the sense that the
  // actual passed arguments are available in an arguments adaptor
  // frame below it on the stack.
  inline bool has_adapted_arguments() const;

  // Garbage collection support.
  void Iterate(RootVisitor* v) const override;

  // Printing support.
  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

  // Determine the code for the frame.
  Code* unchecked_code() const override;

  // Return a list with {SharedFunctionInfo} objects of this frame.
  virtual void GetFunctions(std::vector<SharedFunctionInfo*>* functions) const;

  void GetFunctions(std::vector<Handle<SharedFunctionInfo>>* functions) const;

  // Lookup exception handler for current {pc}, returns -1 if none found. Also
  // returns data associated with the handler site specific to the frame type:
  //  - OptimizedFrame  : Data is the stack slot count of the entire frame.
  //  - InterpretedFrame: Data is the register index holding the context.
  virtual int LookupExceptionHandlerInTable(
      int* data, HandlerTable::CatchPrediction* prediction);

  // Architecture-specific register description.
  static Register fp_register();
  static Register context_register();
  static Register constant_pool_pointer_register();

  static JavaScriptFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_java_script());
    return static_cast<JavaScriptFrame*>(frame);
  }

  static void PrintFunctionAndOffset(JSFunction* function, AbstractCode* code,
                                     int code_offset, FILE* file,
                                     bool print_line_number);

  static void PrintTop(Isolate* isolate, FILE* file, bool print_args,
                       bool print_line_number);

  static void CollectFunctionAndOffsetForICStats(JSFunction* function,
                                                 AbstractCode* code,
                                                 int code_offset);
  static void CollectTopFrameForICStats(Isolate* isolate);

 protected:
  inline explicit JavaScriptFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

  virtual int GetNumberOfIncomingArguments() const;

  // Garbage collection support. Iterates over incoming arguments,
  // receiver, and any callee-saved registers.
  void IterateArguments(RootVisitor* v) const;

  virtual void PrintFrameKind(StringStream* accumulator) const {}

 private:
  inline Object* function_slot_object() const;

  friend class StackFrameIteratorBase;
};


class StubFrame : public StandardFrame {
 public:
  Type type() const override { return STUB; }

  // GC support.
  void Iterate(RootVisitor* v) const override;

  // Determine the code for the frame.
  Code* unchecked_code() const override;

  // Lookup exception handler for current {pc}, returns -1 if none found. Only
  // TurboFan stub frames are supported. Also returns data associated with the
  // handler site:
  //  - TurboFan stub: Data is the stack slot count of the entire frame.
  int LookupExceptionHandlerInTable(int* data);

 protected:
  inline explicit StubFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

  virtual int GetNumberOfIncomingArguments() const;

  friend class StackFrameIteratorBase;
};


class OptimizedFrame : public JavaScriptFrame {
 public:
  Type type() const override { return OPTIMIZED; }

  // GC support.
  void Iterate(RootVisitor* v) const override;

  // Return a list with {SharedFunctionInfo} objects of this frame.
  // The functions are ordered bottom-to-top (i.e. functions.last()
  // is the top-most activation)
  void GetFunctions(std::vector<SharedFunctionInfo*>* functions) const override;

  void Summarize(std::vector<FrameSummary>* frames) const override;

  // Lookup exception handler for current {pc}, returns -1 if none found.
  int LookupExceptionHandlerInTable(
      int* data, HandlerTable::CatchPrediction* prediction) override;

  DeoptimizationData* GetDeoptimizationData(int* deopt_index) const;

  Object* receiver() const override;

  static int StackSlotOffsetRelativeToFp(int slot_index);

 protected:
  inline explicit OptimizedFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;

  Object* StackSlotAt(int index) const;
};


class InterpretedFrame : public JavaScriptFrame {
 public:
  Type type() const override { return INTERPRETED; }

  // Accessors.
  int position() const override;

  // Lookup exception handler for current {pc}, returns -1 if none found.
  int LookupExceptionHandlerInTable(
      int* data, HandlerTable::CatchPrediction* prediction) override;

  // Returns the current offset into the bytecode stream.
  int GetBytecodeOffset() const;

  // Updates the current offset into the bytecode stream, mainly used for stack
  // unwinding to continue execution at a different bytecode offset.
  void PatchBytecodeOffset(int new_offset);

  // Returns the frame's current bytecode array.
  BytecodeArray* GetBytecodeArray() const;

  // Updates the frame's BytecodeArray with |bytecode_array|. Used by the
  // debugger to swap execution onto a BytecodeArray patched with breakpoints.
  void PatchBytecodeArray(BytecodeArray* bytecode_array);

  // Access to the interpreter register file for this frame.
  Object* ReadInterpreterRegister(int register_index) const;
  void WriteInterpreterRegister(int register_index, Object* value);

  // Build a list with summaries for this frame including all inlined frames.
  void Summarize(std::vector<FrameSummary>* frames) const override;

  static int GetBytecodeOffset(Address fp);

 protected:
  inline explicit InterpretedFrame(StackFrameIteratorBase* iterator);

  Address GetExpressionAddress(int n) const override;

 private:
  friend class StackFrameIteratorBase;
};


// Arguments adaptor frames are automatically inserted below
// JavaScript frames when the actual number of parameters does not
// match the formal number of parameters.
class ArgumentsAdaptorFrame: public JavaScriptFrame {
 public:
  Type type() const override { return ARGUMENTS_ADAPTOR; }

  // Determine the code for the frame.
  Code* unchecked_code() const override;

  static ArgumentsAdaptorFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_arguments_adaptor());
    return static_cast<ArgumentsAdaptorFrame*>(frame);
  }

  // Printing support.
  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

 protected:
  inline explicit ArgumentsAdaptorFrame(StackFrameIteratorBase* iterator);

  int GetNumberOfIncomingArguments() const override;

 private:
  friend class StackFrameIteratorBase;
};

// Builtin frames are built for builtins with JavaScript linkage, such as
// various standard library functions (i.e. Math.asin, Math.floor, etc.).
class BuiltinFrame final : public JavaScriptFrame {
 public:
  Type type() const final { return BUILTIN; }

  static BuiltinFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_builtin());
    return static_cast<BuiltinFrame*>(frame);
  }

 protected:
  inline explicit BuiltinFrame(StackFrameIteratorBase* iterator);

  int GetNumberOfIncomingArguments() const final;
  void PrintFrameKind(StringStream* accumulator) const override;

 private:
  friend class StackFrameIteratorBase;
};

class WasmCompiledFrame final : public StandardFrame {
 public:
  Type type() const override { return WASM_COMPILED; }

  // GC support.
  void Iterate(RootVisitor* v) const override;

  // Printing support.
  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

  // Lookup exception handler for current {pc}, returns -1 if none found. Also
  // returns the stack slot count of the entire frame.
  int LookupExceptionHandlerInTable(int* data);

  // Determine the code for the frame.
  Code* unchecked_code() const override;

  // Accessors.
  WasmInstanceObject* wasm_instance() const;
  WasmCodeWrapper wasm_code() const;
  uint32_t function_index() const;
  Script* script() const override;
  int position() const override;
  bool at_to_number_conversion() const;

  void Summarize(std::vector<FrameSummary>* frames) const override;

  static WasmCompiledFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_wasm_compiled());
    return static_cast<WasmCompiledFrame*>(frame);
  }

 protected:
  inline explicit WasmCompiledFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

 private:
  friend class StackFrameIteratorBase;
};

class WasmInterpreterEntryFrame final : public StandardFrame {
 public:
  Type type() const override { return WASM_INTERPRETER_ENTRY; }

  // GC support.
  void Iterate(RootVisitor* v) const override;

  // Printing support.
  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

  void Summarize(std::vector<FrameSummary>* frames) const override;

  // Determine the code for the frame.
  Code* unchecked_code() const override;

  // Accessors.
  WasmInstanceObject* wasm_instance() const;
  Script* script() const override;
  int position() const override;
  Object* context() const override;

  static WasmInterpreterEntryFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_wasm_interpreter_entry());
    return static_cast<WasmInterpreterEntryFrame*>(frame);
  }

 protected:
  inline explicit WasmInterpreterEntryFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

 private:
  friend class StackFrameIteratorBase;
};

class WasmToJsFrame : public StubFrame {
 public:
  Type type() const override { return WASM_TO_JS; }

 protected:
  inline explicit WasmToJsFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class JsToWasmFrame : public StubFrame {
 public:
  Type type() const override { return JS_TO_WASM; }

 protected:
  inline explicit JsToWasmFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class WasmToWasmFrame : public StubFrame {
 public:
  Type type() const override { return WASM_TO_WASM; }

 protected:
  inline explicit WasmToWasmFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class CWasmEntryFrame : public StubFrame {
 public:
  Type type() const override { return C_WASM_ENTRY; }

 protected:
  inline explicit CWasmEntryFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class InternalFrame: public StandardFrame {
 public:
  Type type() const override { return INTERNAL; }

  // Garbage collection support.
  void Iterate(RootVisitor* v) const override;

  // Determine the code for the frame.
  Code* unchecked_code() const override;

  static InternalFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_internal());
    return static_cast<InternalFrame*>(frame);
  }

 protected:
  inline explicit InternalFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

 private:
  friend class StackFrameIteratorBase;
};


// Construct frames are special trampoline frames introduced to handle
// function invocations through 'new'.
class ConstructFrame: public InternalFrame {
 public:
  Type type() const override { return CONSTRUCT; }

  static ConstructFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_construct());
    return static_cast<ConstructFrame*>(frame);
  }

 protected:
  inline explicit ConstructFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class BuiltinContinuationFrame : public InternalFrame {
 public:
  Type type() const override { return BUILTIN_CONTINUATION; }

  static BuiltinContinuationFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_builtin_continuation());
    return static_cast<BuiltinContinuationFrame*>(frame);
  }

 protected:
  inline explicit BuiltinContinuationFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class JavaScriptBuiltinContinuationFrame : public JavaScriptFrame {
 public:
  Type type() const override { return JAVA_SCRIPT_BUILTIN_CONTINUATION; }

  static JavaScriptBuiltinContinuationFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_java_script_builtin_continuation());
    return static_cast<JavaScriptBuiltinContinuationFrame*>(frame);
  }

  int ComputeParametersCount() const override;

 protected:
  inline explicit JavaScriptBuiltinContinuationFrame(
      StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class StackFrameIteratorBase BASE_EMBEDDED {
 public:
  Isolate* isolate() const { return isolate_; }

  bool done() const { return frame_ == nullptr; }

 protected:
  // An iterator that iterates over a given thread's stack.
  StackFrameIteratorBase(Isolate* isolate, bool can_access_heap_objects);

  Isolate* isolate_;
#define DECLARE_SINGLETON(ignore, type) type type##_;
  STACK_FRAME_TYPE_LIST(DECLARE_SINGLETON)
#undef DECLARE_SINGLETON
  StackFrame* frame_;
  StackHandler* handler_;
  const bool can_access_heap_objects_;

  StackHandler* handler() const {
    DCHECK(!done());
    return handler_;
  }

  // Get the type-specific frame singleton in a given state.
  StackFrame* SingletonFor(StackFrame::Type type, StackFrame::State* state);
  // A helper function, can return a nullptr pointer.
  StackFrame* SingletonFor(StackFrame::Type type);

 private:
  friend class StackFrame;
  DISALLOW_COPY_AND_ASSIGN(StackFrameIteratorBase);
};


class StackFrameIterator: public StackFrameIteratorBase {
 public:
  // An iterator that iterates over the isolate's current thread's stack,
  explicit StackFrameIterator(Isolate* isolate);
  // An iterator that iterates over a given thread's stack.
  StackFrameIterator(Isolate* isolate, ThreadLocalTop* t);

  StackFrame* frame() const {
    DCHECK(!done());
    return frame_;
  }
  void Advance();

 private:
  // Go back to the first frame.
  void Reset(ThreadLocalTop* top);

  DISALLOW_COPY_AND_ASSIGN(StackFrameIterator);
};

// Iterator that supports iterating through all JavaScript frames.
class JavaScriptFrameIterator BASE_EMBEDDED {
 public:
  inline explicit JavaScriptFrameIterator(Isolate* isolate);
  inline JavaScriptFrameIterator(Isolate* isolate, ThreadLocalTop* top);

  inline JavaScriptFrame* frame() const;

  bool done() const { return iterator_.done(); }
  void Advance();
  void AdvanceOneFrame() { iterator_.Advance(); }

 private:
  StackFrameIterator iterator_;
};

// NOTE: The stack trace frame iterator is an iterator that only traverse proper
// JavaScript frames that have proper JavaScript functions and WebAssembly
// frames.
class StackTraceFrameIterator BASE_EMBEDDED {
 public:
  explicit StackTraceFrameIterator(Isolate* isolate);
  // Skip frames until the frame with the given id is reached.
  StackTraceFrameIterator(Isolate* isolate, StackFrame::Id id);
  bool done() const { return iterator_.done(); }
  void Advance();
  void AdvanceOneFrame() { iterator_.Advance(); }

  inline StandardFrame* frame() const;

  inline bool is_javascript() const;
  inline bool is_wasm() const;
  inline JavaScriptFrame* javascript_frame() const;

 private:
  StackFrameIterator iterator_;
  bool IsValidFrame(StackFrame* frame) const;
};


class SafeStackFrameIterator: public StackFrameIteratorBase {
 public:
  SafeStackFrameIterator(Isolate* isolate,
                         Address fp, Address sp,
                         Address js_entry_sp);

  inline StackFrame* frame() const;
  void Advance();

  StackFrame::Type top_frame_type() const { return top_frame_type_; }

 private:
  void AdvanceOneFrame();

  bool IsValidStackAddress(Address addr) const {
    return low_bound_ <= addr && addr <= high_bound_;
  }
  bool IsValidFrame(StackFrame* frame) const;
  bool IsValidCaller(StackFrame* frame);
  bool IsValidExitFrame(Address fp) const;
  bool IsValidTop(ThreadLocalTop* top) const;

  const Address low_bound_;
  const Address high_bound_;
  StackFrame::Type top_frame_type_;
  ExternalCallbackScope* external_callback_scope_;
};

// Reads all frames on the current stack and copies them into the current
// zone memory.
Vector<StackFrame*> CreateStackMap(Isolate* isolate, Zone* zone);

}  // namespace internal
}  // namespace v8

#endif  // V8_FRAMES_H_
