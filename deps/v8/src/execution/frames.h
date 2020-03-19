// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_FRAMES_H_
#define V8_EXECUTION_FRAMES_H_

#include "src/codegen/safepoint-table.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/code.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {
namespace wasm {
class WasmCode;
}

// Forward declarations.
class AbstractCode;
class Debug;
class ExternalCallbackScope;
class InnerPointerToCodeCache;
class Isolate;
class ObjectVisitor;
class Register;
class RootVisitor;
class StackFrameIteratorBase;
class StringStream;
class ThreadLocalTop;
class WasmDebugInfo;
class WasmInstanceObject;
class WasmModuleObject;

class StackHandlerConstants : public AllStatic {
 public:
  static const int kNextOffset = 0 * kSystemPointerSize;
  static const int kPaddingOffset = 1 * kSystemPointerSize;

  static const int kSize = kPaddingOffset + kSystemPointerSize;
  static const int kSlotCount = kSize >> kSystemPointerSizeLog2;
};

class StackHandler {
 public:
  // Get the address of this stack handler.
  inline Address address() const;

  // Get the next stack handler in the chain.
  inline StackHandler* next() const;

  // Get the next stack handler, as an Address. This is safe to use even
  // when the next handler is null.
  inline Address next_address() const;

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
  V(JS_TO_WASM, JsToWasmFrame)                                            \
  V(WASM_INTERPRETER_ENTRY, WasmInterpreterEntryFrame)                    \
  V(WASM_DEBUG_BREAK, WasmDebugBreakFrame)                                \
  V(C_WASM_ENTRY, CWasmEntryFrame)                                        \
  V(WASM_EXIT, WasmExitFrame)                                             \
  V(WASM_COMPILE_LAZY, WasmCompileLazyFrame)                              \
  V(INTERPRETED, InterpretedFrame)                                        \
  V(STUB, StubFrame)                                                      \
  V(BUILTIN_CONTINUATION, BuiltinContinuationFrame)                       \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION, JavaScriptBuiltinContinuationFrame) \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH,                          \
    JavaScriptBuiltinContinuationWithCatchFrame)                          \
  V(INTERNAL, InternalFrame)                                              \
  V(CONSTRUCT, ConstructFrame)                                            \
  V(ARGUMENTS_ADAPTOR, ArgumentsAdaptorFrame)                             \
  V(BUILTIN, BuiltinFrame)                                                \
  V(BUILTIN_EXIT, BuiltinExitFrame)                                       \
  V(NATIVE, NativeFrame)

// Abstract base class for all stack frames.
class StackFrame {
 public:
#define DECLARE_TYPE(type, ignore) type,
  enum Type {
    NONE = 0,
    STACK_FRAME_TYPE_LIST(DECLARE_TYPE) NUMBER_OF_TYPES,
    // Used by FrameScope to indicate that the stack frame is constructed
    // manually and the FrameScope does not need to emit code.
    MANUAL
  };
#undef DECLARE_TYPE

  // Used to mark the outermost JS entry frame.
  //
  // The mark is an opaque value that should be pushed onto the stack directly,
  // carefully crafted to not be interpreted as a tagged pointer.
  enum JsFrameMarker {
    INNER_JSENTRY_FRAME = (0 << kSmiTagSize) | kSmiTag,
    OUTERMOST_JSENTRY_FRAME = (1 << kSmiTagSize) | kSmiTag
  };
  // NOLINTNEXTLINE(runtime/references) (false positive)
  STATIC_ASSERT((INNER_JSENTRY_FRAME & kHeapObjectTagMask) != kHeapObjectTag);
  // NOLINTNEXTLINE(runtime/references) (false positive)
  STATIC_ASSERT((OUTERMOST_JSENTRY_FRAME & kHeapObjectTagMask) !=
                kHeapObjectTag);

  struct State {
    Address sp = kNullAddress;
    Address fp = kNullAddress;
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
    intptr_t type = marker >> kSmiTagSize;
    // TODO(petermarshall): There is a bug in the arm simulators that causes
    // invalid frame markers.
#if defined(USE_SIMULATOR) && (V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_ARM)
    if (static_cast<uintptr_t>(type) >= Type::NUMBER_OF_TYPES) {
      // Appease UBSan.
      return Type::NUMBER_OF_TYPES;
    }
#else
    DCHECK_LT(static_cast<uintptr_t>(type), Type::NUMBER_OF_TYPES);
#endif
    return static_cast<Type>(type);
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
  StackFrame(const StackFrame& original) V8_NOEXCEPT {
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
  bool is_wasm_compile_lazy() const { return type() == WASM_COMPILE_LAZY; }
  bool is_wasm_debug_break() const { return type() == WASM_DEBUG_BREAK; }
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
  bool is_java_script_builtin_with_catch_continuation() const {
    return type() == JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH;
  }
  bool is_construct() const { return type() == CONSTRUCT; }
  bool is_builtin_exit() const { return type() == BUILTIN_EXIT; }
  virtual bool is_standard() const { return false; }

  bool is_java_script() const {
    Type type = this->type();
    return (type == OPTIMIZED) || (type == INTERPRETED) || (type == BUILTIN) ||
           (type == JAVA_SCRIPT_BUILTIN_CONTINUATION) ||
           (type == JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH);
  }
  bool is_wasm() const {
    Type type = this->type();
    return type == WASM_COMPILED || type == WASM_INTERPRETER_ENTRY;
  }
  bool is_wasm_to_js() const { return type() == WASM_TO_JS; }

  // Accessors.
  Address sp() const { return state_.sp; }
  Address fp() const { return state_.fp; }
  inline Address callee_pc() const;
  Address caller_sp() const { return GetCallerStackPointer(); }

  // If this frame is optimized and was dynamically aligned return its old
  // unaligned frame pointer.  When the frame is deoptimized its FP will shift
  // up one word and become unaligned.
  Address UnpaddedFP() const;

  inline Address pc() const;

  Address constant_pool() const { return *constant_pool_address(); }
  void set_constant_pool(Address constant_pool) {
    *constant_pool_address() = constant_pool;
  }

  Address* pc_address() const { return state_.pc_address; }

  Address* constant_pool_address() const {
    return state_.constant_pool_address;
  }

  // Get the id of this stack frame.
  StackFrameId id() const { return static_cast<StackFrameId>(caller_sp()); }

  // Get the top handler from the current stack iterator.
  inline StackHandler* top_handler() const;

  // Get the type of this frame.
  virtual Type type() const = 0;

  // Get the code associated with this frame.
  // This method could be called during marking phase of GC.
  virtual Code unchecked_code() const = 0;

  // Search for the code associated with this frame.
  V8_EXPORT_PRIVATE Code LookupCode() const;

  virtual void Iterate(RootVisitor* v) const = 0;
  static void IteratePc(RootVisitor* v, Address* pc_address,
                        Address* constant_pool_address, Code holder);

  // Sets a callback function for return-address rewriting profilers
  // to resolve the location of a return address to the location of the
  // profiler's stashed return address.
  static void SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver resolver);

  static inline Address ReadPC(Address* pc_address);

  // Resolves pc_address through the resolution address function if one is set.
  static inline Address* ResolveReturnAddressLocation(Address* pc_address);

  // Printing support.
  enum PrintMode { OVERVIEW, DETAILS };
  virtual void Print(StringStream* accumulator, PrintMode mode,
                     int index) const;

  Isolate* isolate() const { return isolate_; }

  void operator=(const StackFrame& original) = delete;

 protected:
  inline explicit StackFrame(StackFrameIteratorBase* iterator);
  virtual ~StackFrame() = default;

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

  Code unchecked_code() const override;

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
class EntryFrame : public StackFrame {
 public:
  Type type() const override { return ENTRY; }

  Code unchecked_code() const override;

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

  Code unchecked_code() const override;

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
class ExitFrame : public StackFrame {
 public:
  Type type() const override { return EXIT; }

  Code unchecked_code() const override;

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

  JSFunction function() const;
  Object receiver() const;

  bool IsConstructor() const;

  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

 protected:
  inline explicit BuiltinExitFrame(StackFrameIteratorBase* iterator);

 private:
  Object GetParameter(int i) const;
  int ComputeParametersCount() const;

  inline Object receiver_slot_object() const;
  inline Object argc_slot_object() const;
  inline Object target_slot_object() const;
  inline Object new_target_slot_object() const;

  friend class StackFrameIteratorBase;
  friend class FrameArrayBuilder;
};

class StandardFrame;

class V8_EXPORT_PRIVATE FrameSummary {
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
    JavaScriptFrameSummary(Isolate* isolate, Object receiver,
                           JSFunction function, AbstractCode abstract_code,
                           int code_offset, bool is_constructor,
                           FixedArray parameters);

    void EnsureSourcePositionsAvailable();
    bool AreSourcePositionsAvailable() const;

    Handle<Object> receiver() const { return receiver_; }
    Handle<JSFunction> function() const { return function_; }
    Handle<AbstractCode> abstract_code() const { return abstract_code_; }
    int code_offset() const { return code_offset_; }
    bool is_constructor() const { return is_constructor_; }
    Handle<FixedArray> parameters() const { return parameters_; }
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
    Handle<FixedArray> parameters_;
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
                             wasm::WasmCode*, int code_offset,
                             bool at_to_number_conversion);
    uint32_t function_index() const;
    wasm::WasmCode* code() const { return code_; }
    int code_offset() const { return code_offset_; }
    int byte_offset() const;
    static int GetWasmSourcePosition(const wasm::WasmCode* code, int offset);

   private:
    wasm::WasmCode* const code_;
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

#define FRAME_SUMMARY_CONS(kind, type, field, desc) \
  FrameSummary(type summ) : field(summ) {}  // NOLINT
  FRAME_SUMMARY_VARIANTS(FRAME_SUMMARY_CONS)
#undef FRAME_SUMMARY_CONS

  ~FrameSummary();

  static FrameSummary GetTop(const StandardFrame* frame);
  static FrameSummary GetBottom(const StandardFrame* frame);
  static FrameSummary GetSingle(const StandardFrame* frame);
  static FrameSummary Get(const StandardFrame* frame, int index);

  void EnsureSourcePositionsAvailable();
  bool AreSourcePositionsAvailable() const;

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
#undef FRAME_SUMMARY_FIELD
};

class StandardFrame : public StackFrame {
 public:
  // Testers.
  bool is_standard() const override { return true; }

  // Accessors.
  virtual Object receiver() const;
  virtual Script script() const;
  virtual Object context() const;
  virtual int position() const;

  // Access the expressions in the stack frame including locals.
  inline Object GetExpression(int index) const;
  inline void SetExpression(int index, Object value);
  int ComputeExpressionsCount() const;

  // Access the parameters.
  virtual Object GetParameter(int index) const;
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
  virtual JSFunction function() const;
  Object unchecked_function() const;
  Object receiver() const override;
  Object context() const override;
  Script script() const override;

  inline void set_receiver(Object value);

  // Access the parameters.
  inline Address GetParameterSlot(int index) const;
  Object GetParameter(int index) const override;
  int ComputeParametersCount() const override;
  Handle<FixedArray> GetParameters() const;

  // Debugger access.
  void SetParameterValue(int index, Object value) const;

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
  Code unchecked_code() const override;

  // Return a list with {SharedFunctionInfo} objects of this frame.
  virtual void GetFunctions(std::vector<SharedFunctionInfo>* functions) const;

  void GetFunctions(std::vector<Handle<SharedFunctionInfo>>* functions) const;

  // Lookup exception handler for current {pc}, returns -1 if none found. Also
  // returns data associated with the handler site specific to the frame type:
  //  - OptimizedFrame  : Data is not used and will not return a value.
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

  static void PrintFunctionAndOffset(JSFunction function, AbstractCode code,
                                     int code_offset, FILE* file,
                                     bool print_line_number);

  static void PrintTop(Isolate* isolate, FILE* file, bool print_args,
                       bool print_line_number);

  static void CollectFunctionAndOffsetForICStats(JSFunction function,
                                                 AbstractCode code,
                                                 int code_offset);

 protected:
  inline explicit JavaScriptFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

  virtual void PrintFrameKind(StringStream* accumulator) const {}

 private:
  inline Object function_slot_object() const;

  friend class StackFrameIteratorBase;
};

class StubFrame : public StandardFrame {
 public:
  Type type() const override { return STUB; }

  // GC support.
  void Iterate(RootVisitor* v) const override;

  // Determine the code for the frame.
  Code unchecked_code() const override;

  // Lookup exception handler for current {pc}, returns -1 if none found. Only
  // TurboFan stub frames are supported.
  int LookupExceptionHandlerInTable();

 protected:
  inline explicit StubFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

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
  void GetFunctions(std::vector<SharedFunctionInfo>* functions) const override;

  void Summarize(std::vector<FrameSummary>* frames) const override;

  // Lookup exception handler for current {pc}, returns -1 if none found.
  int LookupExceptionHandlerInTable(
      int* data, HandlerTable::CatchPrediction* prediction) override;

  DeoptimizationData GetDeoptimizationData(int* deopt_index) const;

#ifndef V8_REVERSE_JSARGS
  // When the arguments are reversed in the stack, receiver() is
  // inherited from JavaScriptFrame.
  Object receiver() const override;
#endif
  int ComputeParametersCount() const override;

  static int StackSlotOffsetRelativeToFp(int slot_index);

 protected:
  inline explicit OptimizedFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;

  Object StackSlotAt(int index) const;
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
  BytecodeArray GetBytecodeArray() const;

  // Updates the frame's BytecodeArray with |bytecode_array|. Used by the
  // debugger to swap execution onto a BytecodeArray patched with breakpoints.
  void PatchBytecodeArray(BytecodeArray bytecode_array);

  // Access to the interpreter register file for this frame.
  Object ReadInterpreterRegister(int register_index) const;
  void WriteInterpreterRegister(int register_index, Object value);

  // Build a list with summaries for this frame including all inlined frames.
  void Summarize(std::vector<FrameSummary>* frames) const override;

  static int GetBytecodeOffset(Address fp);

  static InterpretedFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_interpreted());
    return static_cast<InterpretedFrame*>(frame);
  }

 protected:
  inline explicit InterpretedFrame(StackFrameIteratorBase* iterator);

  Address GetExpressionAddress(int n) const override;

 private:
  friend class StackFrameIteratorBase;
};

// Arguments adaptor frames are automatically inserted below
// JavaScript frames when the actual number of parameters does not
// match the formal number of parameters.
class ArgumentsAdaptorFrame : public JavaScriptFrame {
 public:
  Type type() const override { return ARGUMENTS_ADAPTOR; }

  // Determine the code for the frame.
  Code unchecked_code() const override;

  static ArgumentsAdaptorFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_arguments_adaptor());
    return static_cast<ArgumentsAdaptorFrame*>(frame);
  }

  int ComputeParametersCount() const override;

  // Printing support.
  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

 protected:
  inline explicit ArgumentsAdaptorFrame(StackFrameIteratorBase* iterator);

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
  int ComputeParametersCount() const final;

 protected:
  inline explicit BuiltinFrame(StackFrameIteratorBase* iterator);

  void PrintFrameKind(StringStream* accumulator) const override;

 private:
  friend class StackFrameIteratorBase;
};

class WasmCompiledFrame : public StandardFrame {
 public:
  Type type() const override { return WASM_COMPILED; }

  // GC support.
  void Iterate(RootVisitor* v) const override;

  // Printing support.
  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

  // Lookup exception handler for current {pc}, returns -1 if none found.
  int LookupExceptionHandlerInTable();

  // Determine the code for the frame.
  Code unchecked_code() const override;

  // Accessors.
  WasmInstanceObject wasm_instance() const;
  wasm::NativeModule* native_module() const;
  wasm::WasmCode* wasm_code() const;
  uint32_t function_index() const;
  Script script() const override;
  int position() const override;
  Object context() const override;
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
  WasmModuleObject module_object() const;
};

class WasmExitFrame : public WasmCompiledFrame {
 public:
  Type type() const override { return WASM_EXIT; }
  static Address ComputeStackPointer(Address fp);

 protected:
  inline explicit WasmExitFrame(StackFrameIteratorBase* iterator);

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
  Code unchecked_code() const override;

  // Accessors.
  int NumberOfActiveFrames() const;
  WasmDebugInfo debug_info() const;
  WasmInstanceObject wasm_instance() const;

  Script script() const override;
  int position() const override;
  Object context() const override;

  static WasmInterpreterEntryFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_wasm_interpreter_entry());
    return static_cast<WasmInterpreterEntryFrame*>(frame);
  }

 protected:
  inline explicit WasmInterpreterEntryFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

 private:
  friend class StackFrameIteratorBase;
  WasmModuleObject module_object() const;
};

class WasmDebugBreakFrame final : public StandardFrame {
 public:
  Type type() const override { return WASM_DEBUG_BREAK; }

  // GC support.
  void Iterate(RootVisitor* v) const override;

  Code unchecked_code() const override;

  void Print(StringStream* accumulator, PrintMode mode,
             int index) const override;

  static WasmDebugBreakFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_wasm_debug_break());
    return static_cast<WasmDebugBreakFrame*>(frame);
  }

 protected:
  inline explicit WasmDebugBreakFrame(StackFrameIteratorBase*);

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

class CWasmEntryFrame : public StubFrame {
 public:
  Type type() const override { return C_WASM_ENTRY; }

 protected:
  inline explicit CWasmEntryFrame(StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
  Type GetCallerState(State* state) const override;
};

class WasmCompileLazyFrame : public StandardFrame {
 public:
  Type type() const override { return WASM_COMPILE_LAZY; }

  Code unchecked_code() const override;
  WasmInstanceObject wasm_instance() const;
  FullObjectSlot wasm_instance_slot() const;

  // Garbage collection support.
  void Iterate(RootVisitor* v) const override;

  static WasmCompileLazyFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_wasm_compile_lazy());
    return static_cast<WasmCompileLazyFrame*>(frame);
  }

 protected:
  inline explicit WasmCompileLazyFrame(StackFrameIteratorBase* iterator);

  Address GetCallerStackPointer() const override;

 private:
  friend class StackFrameIteratorBase;
};

class InternalFrame : public StandardFrame {
 public:
  Type type() const override { return INTERNAL; }

  // Garbage collection support.
  void Iterate(RootVisitor* v) const override;

  // Determine the code for the frame.
  Code unchecked_code() const override;

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
class ConstructFrame : public InternalFrame {
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
  intptr_t GetSPToFPDelta() const;

  Object context() const override;

 protected:
  inline explicit JavaScriptBuiltinContinuationFrame(
      StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class JavaScriptBuiltinContinuationWithCatchFrame
    : public JavaScriptBuiltinContinuationFrame {
 public:
  Type type() const override {
    return JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH;
  }

  static JavaScriptBuiltinContinuationWithCatchFrame* cast(StackFrame* frame) {
    DCHECK(frame->is_java_script_builtin_with_catch_continuation());
    return static_cast<JavaScriptBuiltinContinuationWithCatchFrame*>(frame);
  }

  // Patch in the exception object at the appropriate location into the stack
  // frame.
  void SetException(Object exception);

 protected:
  inline explicit JavaScriptBuiltinContinuationWithCatchFrame(
      StackFrameIteratorBase* iterator);

 private:
  friend class StackFrameIteratorBase;
};

class StackFrameIteratorBase {
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

class StackFrameIterator : public StackFrameIteratorBase {
 public:
  // An iterator that iterates over the isolate's current thread's stack,
  V8_EXPORT_PRIVATE explicit StackFrameIterator(Isolate* isolate);
  // An iterator that iterates over a given thread's stack.
  V8_EXPORT_PRIVATE StackFrameIterator(Isolate* isolate, ThreadLocalTop* t);

  StackFrame* frame() const {
    DCHECK(!done());
    return frame_;
  }
  V8_EXPORT_PRIVATE void Advance();

 private:
  // Go back to the first frame.
  void Reset(ThreadLocalTop* top);

  DISALLOW_COPY_AND_ASSIGN(StackFrameIterator);
};

// Iterator that supports iterating through all JavaScript frames.
class JavaScriptFrameIterator {
 public:
  inline explicit JavaScriptFrameIterator(Isolate* isolate);
  inline JavaScriptFrameIterator(Isolate* isolate, ThreadLocalTop* top);

  inline JavaScriptFrame* frame() const;

  bool done() const { return iterator_.done(); }
  V8_EXPORT_PRIVATE void Advance();
  void AdvanceOneFrame() { iterator_.Advance(); }

 private:
  StackFrameIterator iterator_;
};

// NOTE: The stack trace frame iterator is an iterator that only traverse proper
// JavaScript frames that have proper JavaScript functions and WebAssembly
// frames.
class V8_EXPORT_PRIVATE StackTraceFrameIterator {
 public:
  explicit StackTraceFrameIterator(Isolate* isolate);
  // Skip frames until the frame with the given id is reached.
  StackTraceFrameIterator(Isolate* isolate, StackFrameId id);
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

class SafeStackFrameIterator : public StackFrameIteratorBase {
 public:
  SafeStackFrameIterator(Isolate* isolate, Address pc, Address fp, Address sp,
                         Address lr, Address js_entry_sp);

  inline StackFrame* frame() const;
  void Advance();

  StackFrame::Type top_frame_type() const { return top_frame_type_; }
  Address top_context_address() const { return top_context_address_; }

 private:
  void AdvanceOneFrame();

  bool IsValidStackAddress(Address addr) const {
    return low_bound_ <= addr && addr <= high_bound_;
  }
  bool IsValidFrame(StackFrame* frame) const;
  bool IsValidCaller(StackFrame* frame);
  bool IsValidExitFrame(Address fp) const;
  bool IsValidTop(ThreadLocalTop* top) const;

  // Returns true if the pc points to a bytecode handler and the frame pointer
  // doesn't seem to be a bytecode handler's frame, which implies that the
  // bytecode handler has an elided frame. This is not precise and might give
  // false negatives since it relies on checks to the frame's type marker,
  // which might be uninitialized.
  bool IsNoFrameBytecodeHandlerPc(Isolate* isolate, Address pc,
                                  Address fp) const;

  const Address low_bound_;
  const Address high_bound_;
  StackFrame::Type top_frame_type_;
  Address top_context_address_;
  ExternalCallbackScope* external_callback_scope_;
  Address top_link_register_;
};

// Frame layout helper classes. Used by the deoptimizer and instruction
// selector.
// -------------------------------------------------------------------------

// How to calculate the frame layout information. Precise, when all information
// is available during deoptimization. Conservative, when an overapproximation
// is fine.
// TODO(jgruber): Investigate whether the conservative kind can be removed. It
// seems possible: 1. is_topmost should be known through the outer_state chain
// of FrameStateDescriptor; 2. the deopt_kind may be a property of the bailout
// id; 3. for continuation_mode, we only care whether it is a mode with catch,
// and that is likewise known at compile-time.
// There is nothing specific blocking this, the investigation just requires time
// and it is not that important to get the exact frame height at compile-time.
enum class FrameInfoKind {
  kPrecise,
  kConservative,
};

// Used by the deoptimizer. Corresponds to frame kinds:
enum class BuiltinContinuationMode {
  STUB,                        // BuiltinContinuationFrame
  JAVASCRIPT,                  // JavaScriptBuiltinContinuationFrame
  JAVASCRIPT_WITH_CATCH,       // JavaScriptBuiltinContinuationWithCatchFrame
  JAVASCRIPT_HANDLE_EXCEPTION  // JavaScriptBuiltinContinuationWithCatchFrame
};

class InterpretedFrameInfo {
 public:
  static InterpretedFrameInfo Precise(int parameters_count_with_receiver,
                                      int translation_height, bool is_topmost) {
    return {parameters_count_with_receiver, translation_height, is_topmost,
            FrameInfoKind::kPrecise};
  }

  static InterpretedFrameInfo Conservative(int parameters_count_with_receiver,
                                           int locals_count) {
    return {parameters_count_with_receiver, locals_count, false,
            FrameInfoKind::kConservative};
  }

  uint32_t register_stack_slot_count() const {
    return register_stack_slot_count_;
  }
  uint32_t frame_size_in_bytes_without_fixed() const {
    return frame_size_in_bytes_without_fixed_;
  }
  uint32_t frame_size_in_bytes() const { return frame_size_in_bytes_; }

 private:
  InterpretedFrameInfo(int parameters_count_with_receiver,
                       int translation_height, bool is_topmost,
                       FrameInfoKind frame_info_kind);

  uint32_t register_stack_slot_count_;
  uint32_t frame_size_in_bytes_without_fixed_;
  uint32_t frame_size_in_bytes_;
};

class ArgumentsAdaptorFrameInfo {
 public:
  static ArgumentsAdaptorFrameInfo Precise(int translation_height) {
    return ArgumentsAdaptorFrameInfo{translation_height};
  }

  static ArgumentsAdaptorFrameInfo Conservative(int parameters_count) {
    return ArgumentsAdaptorFrameInfo{parameters_count};
  }

  uint32_t frame_size_in_bytes_without_fixed() const {
    return frame_size_in_bytes_without_fixed_;
  }
  uint32_t frame_size_in_bytes() const { return frame_size_in_bytes_; }

 private:
  explicit ArgumentsAdaptorFrameInfo(int translation_height);

  uint32_t frame_size_in_bytes_without_fixed_;
  uint32_t frame_size_in_bytes_;
};

class ConstructStubFrameInfo {
 public:
  static ConstructStubFrameInfo Precise(int translation_height,
                                        bool is_topmost) {
    return {translation_height, is_topmost, FrameInfoKind::kPrecise};
  }

  static ConstructStubFrameInfo Conservative(int parameters_count) {
    return {parameters_count, false, FrameInfoKind::kConservative};
  }

  uint32_t frame_size_in_bytes_without_fixed() const {
    return frame_size_in_bytes_without_fixed_;
  }
  uint32_t frame_size_in_bytes() const { return frame_size_in_bytes_; }

 private:
  ConstructStubFrameInfo(int translation_height, bool is_topmost,
                         FrameInfoKind frame_info_kind);

  uint32_t frame_size_in_bytes_without_fixed_;
  uint32_t frame_size_in_bytes_;
};

// Used by BuiltinContinuationFrameInfo.
class CallInterfaceDescriptor;
class RegisterConfiguration;

class BuiltinContinuationFrameInfo {
 public:
  static BuiltinContinuationFrameInfo Precise(
      int translation_height,
      const CallInterfaceDescriptor& continuation_descriptor,
      const RegisterConfiguration* register_config, bool is_topmost,
      DeoptimizeKind deopt_kind, BuiltinContinuationMode continuation_mode) {
    return {translation_height,
            continuation_descriptor,
            register_config,
            is_topmost,
            deopt_kind,
            continuation_mode,
            FrameInfoKind::kPrecise};
  }

  static BuiltinContinuationFrameInfo Conservative(
      int parameters_count,
      const CallInterfaceDescriptor& continuation_descriptor,
      const RegisterConfiguration* register_config) {
    // It doesn't matter what we pass as is_topmost, deopt_kind and
    // continuation_mode; these values are ignored in conservative mode.
    return {parameters_count,
            continuation_descriptor,
            register_config,
            false,
            DeoptimizeKind::kEager,
            BuiltinContinuationMode::STUB,
            FrameInfoKind::kConservative};
  }

  bool frame_has_result_stack_slot() const {
    return frame_has_result_stack_slot_;
  }
  uint32_t translated_stack_parameter_count() const {
    return translated_stack_parameter_count_;
  }
  uint32_t stack_parameter_count() const { return stack_parameter_count_; }
  uint32_t frame_size_in_bytes() const { return frame_size_in_bytes_; }
  uint32_t frame_size_in_bytes_above_fp() const {
    return frame_size_in_bytes_above_fp_;
  }

 private:
  BuiltinContinuationFrameInfo(
      int translation_height,
      const CallInterfaceDescriptor& continuation_descriptor,
      const RegisterConfiguration* register_config, bool is_topmost,
      DeoptimizeKind deopt_kind, BuiltinContinuationMode continuation_mode,
      FrameInfoKind frame_info_kind);

  bool frame_has_result_stack_slot_;
  uint32_t translated_stack_parameter_count_;
  uint32_t stack_parameter_count_;
  uint32_t frame_size_in_bytes_;
  uint32_t frame_size_in_bytes_above_fp_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_FRAMES_H_
