// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_H_
#define V8_DEOPTIMIZER_H_

#include "src/v8.h"

#include "src/allocation.h"
#include "src/macro-assembler.h"


namespace v8 {
namespace internal {


static inline double read_double_value(Address p) {
  double d;
  memcpy(&d, p, sizeof(d));
  return d;
}


class FrameDescription;
class TranslationIterator;
class DeoptimizedFrameInfo;

template<typename T>
class HeapNumberMaterializationDescriptor BASE_EMBEDDED {
 public:
  HeapNumberMaterializationDescriptor(T destination, double value)
      : destination_(destination), value_(value) { }

  T destination() const { return destination_; }
  double value() const { return value_; }

 private:
  T destination_;
  double value_;
};


class ObjectMaterializationDescriptor BASE_EMBEDDED {
 public:
  ObjectMaterializationDescriptor(
      Address slot_address, int frame, int length, int duplicate, bool is_args)
      : slot_address_(slot_address),
        jsframe_index_(frame),
        object_length_(length),
        duplicate_object_(duplicate),
        is_arguments_(is_args) { }

  Address slot_address() const { return slot_address_; }
  int jsframe_index() const { return jsframe_index_; }
  int object_length() const { return object_length_; }
  int duplicate_object() const { return duplicate_object_; }
  bool is_arguments() const { return is_arguments_; }

  // Only used for allocated receivers in DoComputeConstructStubFrame.
  void patch_slot_address(intptr_t slot) {
    slot_address_ = reinterpret_cast<Address>(slot);
  }

 private:
  Address slot_address_;
  int jsframe_index_;
  int object_length_;
  int duplicate_object_;
  bool is_arguments_;
};


class OptimizedFunctionVisitor BASE_EMBEDDED {
 public:
  virtual ~OptimizedFunctionVisitor() {}

  // Function which is called before iteration of any optimized functions
  // from given native context.
  virtual void EnterContext(Context* context) = 0;

  virtual void VisitFunction(JSFunction* function) = 0;

  // Function which is called after iteration of all optimized functions
  // from given native context.
  virtual void LeaveContext(Context* context) = 0;
};


#define DEOPT_MESSAGES_LIST(V)                                                 \
  V(kNoReason, "no reason")                                                    \
  V(kConstantGlobalVariableAssignment, "Constant global variable assignment")  \
  V(kConversionOverflow, "conversion overflow")                                \
  V(kDivisionByZero, "division by zero")                                       \
  V(kElementsKindUnhandledInKeyedLoadGenericStub,                              \
    "ElementsKind unhandled in KeyedLoadGenericStub")                          \
  V(kExpectedHeapNumber, "Expected heap number")                               \
  V(kExpectedSmi, "Expected smi")                                              \
  V(kForcedDeoptToRuntime, "Forced deopt to runtime")                          \
  V(kHole, "hole")                                                             \
  V(kHoleyArrayDespitePackedElements_kindFeedback,                             \
    "Holey array despite packed elements_kind feedback")                       \
  V(kInstanceMigrationFailed, "instance migration failed")                     \
  V(kInsufficientTypeFeedbackForCallWithArguments,                             \
    "Insufficient type feedback for call with arguments")                      \
  V(kInsufficientTypeFeedbackForCombinedTypeOfBinaryOperation,                 \
    "Insufficient type feedback for combined type of binary operation")        \
  V(kInsufficientTypeFeedbackForGenericNamedAccess,                            \
    "Insufficient type feedback for generic named access")                     \
  V(kInsufficientTypeFeedbackForKeyedLoad,                                     \
    "Insufficient type feedback for keyed load")                               \
  V(kInsufficientTypeFeedbackForKeyedStore,                                    \
    "Insufficient type feedback for keyed store")                              \
  V(kInsufficientTypeFeedbackForLHSOfBinaryOperation,                          \
    "Insufficient type feedback for LHS of binary operation")                  \
  V(kInsufficientTypeFeedbackForRHSOfBinaryOperation,                          \
    "Insufficient type feedback for RHS of binary operation")                  \
  V(kKeyIsNegative, "key is negative")                                         \
  V(kLostPrecision, "lost precision")                                          \
  V(kLostPrecisionOrNaN, "lost precision or NaN")                              \
  V(kMementoFound, "memento found")                                            \
  V(kMinusZero, "minus zero")                                                  \
  V(kNaN, "NaN")                                                               \
  V(kNegativeKeyEncountered, "Negative key encountered")                       \
  V(kNegativeValue, "negative value")                                          \
  V(kNoCache, "no cache")                                                      \
  V(kNonStrictElementsInKeyedLoadGenericStub,                                  \
    "non-strict elements in KeyedLoadGenericStub")                             \
  V(kNotADateObject, "not a date object")                                      \
  V(kNotAHeapNumber, "not a heap number")                                      \
  V(kNotAHeapNumberUndefinedBoolean, "not a heap number/undefined/true/false") \
  V(kNotAHeapNumberUndefined, "not a heap number/undefined")                   \
  V(kNotAJavaScriptObject, "not a JavaScript object")                          \
  V(kNotASmi, "not a Smi")                                                     \
  V(kNull, "null")                                                             \
  V(kOutOfBounds, "out of bounds")                                             \
  V(kOutsideOfRange, "Outside of range")                                       \
  V(kOverflow, "overflow")                                                     \
  V(kReceiverWasAGlobalObject, "receiver was a global object")                 \
  V(kSmi, "Smi")                                                               \
  V(kTooManyArguments, "too many arguments")                                   \
  V(kTooManyUndetectableTypes, "Too many undetectable types")                  \
  V(kTracingElementsTransitions, "Tracing elements transitions")               \
  V(kTypeMismatchBetweenFeedbackAndConstant,                                   \
    "Type mismatch between feedback and constant")                             \
  V(kUndefined, "undefined")                                                   \
  V(kUnexpectedCellContentsInConstantGlobalStore,                              \
    "Unexpected cell contents in constant global store")                       \
  V(kUnexpectedCellContentsInGlobalStore,                                      \
    "Unexpected cell contents in global store")                                \
  V(kUnexpectedObject, "unexpected object")                                    \
  V(kUnexpectedRHSOfBinaryOperation, "Unexpected RHS of binary operation")     \
  V(kUninitializedBoilerplateInFastClone,                                      \
    "Uninitialized boilerplate in fast clone")                                 \
  V(kUninitializedBoilerplateLiterals, "Uninitialized boilerplate literals")   \
  V(kUnknownMapInPolymorphicAccess, "Unknown map in polymorphic access")       \
  V(kUnknownMapInPolymorphicCall, "Unknown map in polymorphic call")           \
  V(kUnknownMapInPolymorphicElementAccess,                                     \
    "Unknown map in polymorphic element access")                               \
  V(kUnknownMap, "Unknown map")                                                \
  V(kValueMismatch, "value mismatch")                                          \
  V(kWrongInstanceType, "wrong instance type")                                 \
  V(kWrongMap, "wrong map")                                                    \
  V(kUndefinedOrNullInForIn, "null or undefined in for-in")


class Deoptimizer : public Malloced {
 public:
  enum BailoutType {
    EAGER,
    LAZY,
    SOFT,
    // This last bailout type is not really a bailout, but used by the
    // debugger to deoptimize stack frames to allow inspection.
    DEBUGGER,
    kBailoutTypesWithCodeEntry = SOFT + 1
  };

#define DEOPT_MESSAGES_CONSTANTS(C, T) C,
  enum DeoptReason {
    DEOPT_MESSAGES_LIST(DEOPT_MESSAGES_CONSTANTS) kLastDeoptReason
  };
#undef DEOPT_MESSAGES_CONSTANTS
  static const char* GetDeoptReason(DeoptReason deopt_reason);

  struct DeoptInfo {
    DeoptInfo(SourcePosition position, const char* m, DeoptReason d)
        : position(position), mnemonic(m), deopt_reason(d), inlining_id(0) {}

    SourcePosition position;
    const char* mnemonic;
    DeoptReason deopt_reason;
    int inlining_id;
  };

  static DeoptInfo GetDeoptInfo(Code* code, byte* from);

  struct JumpTableEntry : public ZoneObject {
    inline JumpTableEntry(Address entry, const DeoptInfo& deopt_info,
                          Deoptimizer::BailoutType type, bool frame)
        : label(),
          address(entry),
          deopt_info(deopt_info),
          bailout_type(type),
          needs_frame(frame) {}

    bool IsEquivalentTo(const JumpTableEntry& other) const {
      return address == other.address && bailout_type == other.bailout_type &&
             needs_frame == other.needs_frame;
    }

    Label label;
    Address address;
    DeoptInfo deopt_info;
    Deoptimizer::BailoutType bailout_type;
    bool needs_frame;
  };

  static bool TraceEnabledFor(BailoutType deopt_type,
                              StackFrame::Type frame_type);
  static const char* MessageFor(BailoutType type);

  int output_count() const { return output_count_; }

  Handle<JSFunction> function() const { return Handle<JSFunction>(function_); }
  Handle<Code> compiled_code() const { return Handle<Code>(compiled_code_); }
  BailoutType bailout_type() const { return bailout_type_; }

  // Number of created JS frames. Not all created frames are necessarily JS.
  int jsframe_count() const { return jsframe_count_; }

  static Deoptimizer* New(JSFunction* function,
                          BailoutType type,
                          unsigned bailout_id,
                          Address from,
                          int fp_to_sp_delta,
                          Isolate* isolate);
  static Deoptimizer* Grab(Isolate* isolate);

  // The returned object with information on the optimized frame needs to be
  // freed before another one can be generated.
  static DeoptimizedFrameInfo* DebuggerInspectableFrame(JavaScriptFrame* frame,
                                                        int jsframe_index,
                                                        Isolate* isolate);
  static void DeleteDebuggerInspectableFrame(DeoptimizedFrameInfo* info,
                                             Isolate* isolate);

  // Makes sure that there is enough room in the relocation
  // information of a code object to perform lazy deoptimization
  // patching. If there is not enough room a new relocation
  // information object is allocated and comments are added until it
  // is big enough.
  static void EnsureRelocSpaceForLazyDeoptimization(Handle<Code> code);

  // Deoptimize the function now. Its current optimized code will never be run
  // again and any activations of the optimized code will get deoptimized when
  // execution returns.
  static void DeoptimizeFunction(JSFunction* function);

  // Deoptimize all code in the given isolate.
  static void DeoptimizeAll(Isolate* isolate);

  // Deoptimize code associated with the given global object.
  static void DeoptimizeGlobalObject(JSObject* object);

  // Deoptimizes all optimized code that has been previously marked
  // (via code->set_marked_for_deoptimization) and unlinks all functions that
  // refer to that code.
  static void DeoptimizeMarkedCode(Isolate* isolate);

  // Visit all the known optimized functions in a given isolate.
  static void VisitAllOptimizedFunctions(
      Isolate* isolate, OptimizedFunctionVisitor* visitor);

  // The size in bytes of the code required at a lazy deopt patch site.
  static int patch_size();

  ~Deoptimizer();

  void MaterializeHeapObjects(JavaScriptFrameIterator* it);

  void MaterializeHeapNumbersForDebuggerInspectableFrame(
      Address parameters_top,
      uint32_t parameters_size,
      Address expressions_top,
      uint32_t expressions_size,
      DeoptimizedFrameInfo* info);

  static void ComputeOutputFrames(Deoptimizer* deoptimizer);


  enum GetEntryMode {
    CALCULATE_ENTRY_ADDRESS,
    ENSURE_ENTRY_CODE
  };


  static Address GetDeoptimizationEntry(
      Isolate* isolate,
      int id,
      BailoutType type,
      GetEntryMode mode = ENSURE_ENTRY_CODE);
  static int GetDeoptimizationId(Isolate* isolate,
                                 Address addr,
                                 BailoutType type);
  static int GetOutputInfo(DeoptimizationOutputData* data,
                           BailoutId node_id,
                           SharedFunctionInfo* shared);

  // Code generation support.
  static int input_offset() { return OFFSET_OF(Deoptimizer, input_); }
  static int output_count_offset() {
    return OFFSET_OF(Deoptimizer, output_count_);
  }
  static int output_offset() { return OFFSET_OF(Deoptimizer, output_); }

  static int has_alignment_padding_offset() {
    return OFFSET_OF(Deoptimizer, has_alignment_padding_);
  }

  static int GetDeoptimizedCodeCount(Isolate* isolate);

  static const int kNotDeoptimizationEntry = -1;

  // Generators for the deoptimization entry code.
  class TableEntryGenerator BASE_EMBEDDED {
   public:
    TableEntryGenerator(MacroAssembler* masm, BailoutType type, int count)
        : masm_(masm), type_(type), count_(count) {}

    void Generate();

   protected:
    MacroAssembler* masm() const { return masm_; }
    BailoutType type() const { return type_; }
    Isolate* isolate() const { return masm_->isolate(); }

    void GeneratePrologue();

   private:
    int count() const { return count_; }

    MacroAssembler* masm_;
    Deoptimizer::BailoutType type_;
    int count_;
  };

  int ConvertJSFrameIndexToFrameIndex(int jsframe_index);

  static size_t GetMaxDeoptTableSize();

  static void EnsureCodeForDeoptimizationEntry(Isolate* isolate,
                                               BailoutType type,
                                               int max_entry_id);

  Isolate* isolate() const { return isolate_; }

 private:
  static const int kMinNumberOfEntries = 64;
  static const int kMaxNumberOfEntries = 16384;

  Deoptimizer(Isolate* isolate,
              JSFunction* function,
              BailoutType type,
              unsigned bailout_id,
              Address from,
              int fp_to_sp_delta,
              Code* optimized_code);
  Code* FindOptimizedCode(JSFunction* function, Code* optimized_code);
  void PrintFunctionName();
  void DeleteFrameDescriptions();

  void DoComputeOutputFrames();
  void DoComputeJSFrame(TranslationIterator* iterator, int frame_index);
  void DoComputeArgumentsAdaptorFrame(TranslationIterator* iterator,
                                      int frame_index);
  void DoComputeConstructStubFrame(TranslationIterator* iterator,
                                   int frame_index);
  void DoComputeAccessorStubFrame(TranslationIterator* iterator,
                                  int frame_index,
                                  bool is_setter_stub_frame);
  void DoComputeCompiledStubFrame(TranslationIterator* iterator,
                                  int frame_index);

  // Translate object, store the result into an auxiliary array
  // (deferred_objects_tagged_values_).
  void DoTranslateObject(TranslationIterator* iterator,
                         int object_index,
                         int field_index);

  // Translate value, store the result into the given frame slot.
  void DoTranslateCommand(TranslationIterator* iterator,
                          int frame_index,
                          unsigned output_offset);

  // Translate object, do not store the result anywhere (but do update
  // the deferred materialization array).
  void DoTranslateObjectAndSkip(TranslationIterator* iterator);

  unsigned ComputeInputFrameSize() const;
  unsigned ComputeFixedSize(JSFunction* function) const;

  unsigned ComputeIncomingArgumentSize(JSFunction* function) const;
  unsigned ComputeOutgoingArgumentSize() const;

  Object* ComputeLiteral(int index) const;

  void AddObjectStart(intptr_t slot_address, int argc, bool is_arguments);
  void AddObjectDuplication(intptr_t slot, int object_index);
  void AddObjectTaggedValue(intptr_t value);
  void AddObjectDoubleValue(double value);
  void AddDoubleValue(intptr_t slot_address, double value);

  bool ArgumentsObjectIsAdapted(int object_index) {
    ObjectMaterializationDescriptor desc = deferred_objects_.at(object_index);
    int reverse_jsframe_index = jsframe_count_ - desc.jsframe_index() - 1;
    return jsframe_has_adapted_arguments_[reverse_jsframe_index];
  }

  Handle<JSFunction> ArgumentsObjectFunction(int object_index) {
    ObjectMaterializationDescriptor desc = deferred_objects_.at(object_index);
    int reverse_jsframe_index = jsframe_count_ - desc.jsframe_index() - 1;
    return jsframe_functions_[reverse_jsframe_index];
  }

  // Helper function for heap object materialization.
  Handle<Object> MaterializeNextHeapObject();
  Handle<Object> MaterializeNextValue();

  static void GenerateDeoptimizationEntries(
      MacroAssembler* masm, int count, BailoutType type);

  // Marks all the code in the given context for deoptimization.
  static void MarkAllCodeForContext(Context* native_context);

  // Visit all the known optimized functions in a given context.
  static void VisitAllOptimizedFunctionsForContext(
      Context* context, OptimizedFunctionVisitor* visitor);

  // Deoptimizes all code marked in the given context.
  static void DeoptimizeMarkedCodeForContext(Context* native_context);

  // Patch the given code so that it will deoptimize itself.
  static void PatchCodeForDeoptimization(Isolate* isolate, Code* code);

  // Searches the list of known deoptimizing code for a Code object
  // containing the given address (which is supposedly faster than
  // searching all code objects).
  Code* FindDeoptimizingCode(Address addr);

  // Fill the input from from a JavaScript frame. This is used when
  // the debugger needs to inspect an optimized frame. For normal
  // deoptimizations the input frame is filled in generated code.
  void FillInputFrame(Address tos, JavaScriptFrame* frame);

  // Fill the given output frame's registers to contain the failure handler
  // address and the number of parameters for a stub failure trampoline.
  void SetPlatformCompiledStubRegisters(FrameDescription* output_frame,
                                        CodeStubDescriptor* desc);

  // Fill the given output frame's double registers with the original values
  // from the input frame's double registers.
  void CopyDoubleRegisters(FrameDescription* output_frame);

  // Determines whether the input frame contains alignment padding by looking
  // at the dynamic alignment state slot inside the frame.
  bool HasAlignmentPadding(JSFunction* function);

  Isolate* isolate_;
  JSFunction* function_;
  Code* compiled_code_;
  unsigned bailout_id_;
  BailoutType bailout_type_;
  Address from_;
  int fp_to_sp_delta_;
  int has_alignment_padding_;

  // Input frame description.
  FrameDescription* input_;
  // Number of output frames.
  int output_count_;
  // Number of output js frames.
  int jsframe_count_;
  // Array of output frame descriptions.
  FrameDescription** output_;

  // Deferred values to be materialized.
  List<Object*> deferred_objects_tagged_values_;
  List<HeapNumberMaterializationDescriptor<int> >
      deferred_objects_double_values_;
  List<ObjectMaterializationDescriptor> deferred_objects_;
  List<HeapNumberMaterializationDescriptor<Address> > deferred_heap_numbers_;

  // Key for lookup of previously materialized objects
  Address stack_fp_;
  Handle<FixedArray> previously_materialized_objects_;
  int prev_materialized_count_;

  // Output frame information. Only used during heap object materialization.
  List<Handle<JSFunction> > jsframe_functions_;
  List<bool> jsframe_has_adapted_arguments_;

  // Materialized objects. Only used during heap object materialization.
  List<Handle<Object> >* materialized_values_;
  List<Handle<Object> >* materialized_objects_;
  int materialization_value_index_;
  int materialization_object_index_;

#ifdef DEBUG
  DisallowHeapAllocation* disallow_heap_allocation_;
#endif  // DEBUG

  CodeTracer::Scope* trace_scope_;

  static const int table_entry_size_;

  friend class FrameDescription;
  friend class DeoptimizedFrameInfo;
};


class FrameDescription {
 public:
  FrameDescription(uint32_t frame_size,
                   JSFunction* function);

  void* operator new(size_t size, uint32_t frame_size) {
    // Subtracts kPointerSize, as the member frame_content_ already supplies
    // the first element of the area to store the frame.
    return malloc(size + frame_size - kPointerSize);
  }

// Bug in VS2015 RC, reported fixed in RTM. Microsoft bug: 1153909.
#if !defined(_MSC_FULL_VER) || _MSC_FULL_VER != 190022816
  void operator delete(void* pointer, uint32_t frame_size) {
    free(pointer);
  }
#endif  // _MSC_FULL_VER

  void operator delete(void* description) {
    free(description);
  }

  uint32_t GetFrameSize() const {
    DCHECK(static_cast<uint32_t>(frame_size_) == frame_size_);
    return static_cast<uint32_t>(frame_size_);
  }

  JSFunction* GetFunction() const { return function_; }

  unsigned GetOffsetFromSlotIndex(int slot_index);

  intptr_t GetFrameSlot(unsigned offset) {
    return *GetFrameSlotPointer(offset);
  }

  double GetDoubleFrameSlot(unsigned offset) {
    intptr_t* ptr = GetFrameSlotPointer(offset);
    return read_double_value(reinterpret_cast<Address>(ptr));
  }

  void SetFrameSlot(unsigned offset, intptr_t value) {
    *GetFrameSlotPointer(offset) = value;
  }

  void SetCallerPc(unsigned offset, intptr_t value);

  void SetCallerFp(unsigned offset, intptr_t value);

  void SetCallerConstantPool(unsigned offset, intptr_t value);

  intptr_t GetRegister(unsigned n) const {
#if DEBUG
    // This convoluted DCHECK is needed to work around a gcc problem that
    // improperly detects an array bounds overflow in optimized debug builds
    // when using a plain DCHECK.
    if (n >= arraysize(registers_)) {
      DCHECK(false);
      return 0;
    }
#endif
    return registers_[n];
  }

  double GetDoubleRegister(unsigned n) const {
    DCHECK(n < arraysize(double_registers_));
    return double_registers_[n];
  }

  void SetRegister(unsigned n, intptr_t value) {
    DCHECK(n < arraysize(registers_));
    registers_[n] = value;
  }

  void SetDoubleRegister(unsigned n, double value) {
    DCHECK(n < arraysize(double_registers_));
    double_registers_[n] = value;
  }

  intptr_t GetTop() const { return top_; }
  void SetTop(intptr_t top) { top_ = top; }

  intptr_t GetPc() const { return pc_; }
  void SetPc(intptr_t pc) { pc_ = pc; }

  intptr_t GetFp() const { return fp_; }
  void SetFp(intptr_t fp) { fp_ = fp; }

  intptr_t GetContext() const { return context_; }
  void SetContext(intptr_t context) { context_ = context; }

  intptr_t GetConstantPool() const { return constant_pool_; }
  void SetConstantPool(intptr_t constant_pool) {
    constant_pool_ = constant_pool;
  }

  Smi* GetState() const { return state_; }
  void SetState(Smi* state) { state_ = state; }

  void SetContinuation(intptr_t pc) { continuation_ = pc; }

  StackFrame::Type GetFrameType() const { return type_; }
  void SetFrameType(StackFrame::Type type) { type_ = type; }

  // Get the incoming arguments count.
  int ComputeParametersCount();

  // Get a parameter value for an unoptimized frame.
  Object* GetParameter(int index);

  // Get the expression stack height for a unoptimized frame.
  unsigned GetExpressionCount();

  // Get the expression stack value for an unoptimized frame.
  Object* GetExpression(int index);

  static int registers_offset() {
    return OFFSET_OF(FrameDescription, registers_);
  }

  static int double_registers_offset() {
    return OFFSET_OF(FrameDescription, double_registers_);
  }

  static int frame_size_offset() {
    return OFFSET_OF(FrameDescription, frame_size_);
  }

  static int pc_offset() {
    return OFFSET_OF(FrameDescription, pc_);
  }

  static int state_offset() {
    return OFFSET_OF(FrameDescription, state_);
  }

  static int continuation_offset() {
    return OFFSET_OF(FrameDescription, continuation_);
  }

  static int frame_content_offset() {
    return OFFSET_OF(FrameDescription, frame_content_);
  }

 private:
  static const uint32_t kZapUint32 = 0xbeeddead;

  // Frame_size_ must hold a uint32_t value.  It is only a uintptr_t to
  // keep the variable-size array frame_content_ of type intptr_t at
  // the end of the structure aligned.
  uintptr_t frame_size_;  // Number of bytes.
  JSFunction* function_;
  intptr_t registers_[Register::kNumRegisters];
  double double_registers_[DoubleRegister::kMaxNumRegisters];
  intptr_t top_;
  intptr_t pc_;
  intptr_t fp_;
  intptr_t context_;
  intptr_t constant_pool_;
  StackFrame::Type type_;
  Smi* state_;

  // Continuation is the PC where the execution continues after
  // deoptimizing.
  intptr_t continuation_;

  // This must be at the end of the object as the object is allocated larger
  // than it's definition indicate to extend this array.
  intptr_t frame_content_[1];

  intptr_t* GetFrameSlotPointer(unsigned offset) {
    DCHECK(offset < frame_size_);
    return reinterpret_cast<intptr_t*>(
        reinterpret_cast<Address>(this) + frame_content_offset() + offset);
  }

  int ComputeFixedSize();
};


class DeoptimizerData {
 public:
  explicit DeoptimizerData(MemoryAllocator* allocator);
  ~DeoptimizerData();

  void Iterate(ObjectVisitor* v);

 private:
  MemoryAllocator* allocator_;
  int deopt_entry_code_entries_[Deoptimizer::kBailoutTypesWithCodeEntry];
  MemoryChunk* deopt_entry_code_[Deoptimizer::kBailoutTypesWithCodeEntry];

  DeoptimizedFrameInfo* deoptimized_frame_info_;

  Deoptimizer* current_;

  friend class Deoptimizer;

  DISALLOW_COPY_AND_ASSIGN(DeoptimizerData);
};


class TranslationBuffer BASE_EMBEDDED {
 public:
  explicit TranslationBuffer(Zone* zone) : contents_(256, zone) { }

  int CurrentIndex() const { return contents_.length(); }
  void Add(int32_t value, Zone* zone);

  Handle<ByteArray> CreateByteArray(Factory* factory);

 private:
  ZoneList<uint8_t> contents_;
};


class TranslationIterator BASE_EMBEDDED {
 public:
  TranslationIterator(ByteArray* buffer, int index)
      : buffer_(buffer), index_(index) {
    DCHECK(index >= 0 && index < buffer->length());
  }

  int32_t Next();

  bool HasNext() const { return index_ < buffer_->length(); }

  void Skip(int n) {
    for (int i = 0; i < n; i++) Next();
  }

 private:
  ByteArray* buffer_;
  int index_;
};


#define TRANSLATION_OPCODE_LIST(V) \
  V(BEGIN)                         \
  V(JS_FRAME)                      \
  V(CONSTRUCT_STUB_FRAME)          \
  V(GETTER_STUB_FRAME)             \
  V(SETTER_STUB_FRAME)             \
  V(ARGUMENTS_ADAPTOR_FRAME)       \
  V(COMPILED_STUB_FRAME)           \
  V(DUPLICATED_OBJECT)             \
  V(ARGUMENTS_OBJECT)              \
  V(CAPTURED_OBJECT)               \
  V(REGISTER)                      \
  V(INT32_REGISTER)                \
  V(UINT32_REGISTER)               \
  V(BOOL_REGISTER)                 \
  V(DOUBLE_REGISTER)               \
  V(STACK_SLOT)                    \
  V(INT32_STACK_SLOT)              \
  V(UINT32_STACK_SLOT)             \
  V(BOOL_STACK_SLOT)               \
  V(DOUBLE_STACK_SLOT)             \
  V(LITERAL)


class Translation BASE_EMBEDDED {
 public:
#define DECLARE_TRANSLATION_OPCODE_ENUM(item) item,
  enum Opcode {
    TRANSLATION_OPCODE_LIST(DECLARE_TRANSLATION_OPCODE_ENUM)
    LAST = LITERAL
  };
#undef DECLARE_TRANSLATION_OPCODE_ENUM

  Translation(TranslationBuffer* buffer, int frame_count, int jsframe_count,
              Zone* zone)
      : buffer_(buffer),
        index_(buffer->CurrentIndex()),
        zone_(zone) {
    buffer_->Add(BEGIN, zone);
    buffer_->Add(frame_count, zone);
    buffer_->Add(jsframe_count, zone);
  }

  int index() const { return index_; }

  // Commands.
  void BeginJSFrame(BailoutId node_id, int literal_id, unsigned height);
  void BeginCompiledStubFrame();
  void BeginArgumentsAdaptorFrame(int literal_id, unsigned height);
  void BeginConstructStubFrame(int literal_id, unsigned height);
  void BeginGetterStubFrame(int literal_id);
  void BeginSetterStubFrame(int literal_id);
  void BeginArgumentsObject(int args_length);
  void BeginCapturedObject(int length);
  void DuplicateObject(int object_index);
  void StoreRegister(Register reg);
  void StoreInt32Register(Register reg);
  void StoreUint32Register(Register reg);
  void StoreBoolRegister(Register reg);
  void StoreDoubleRegister(DoubleRegister reg);
  void StoreStackSlot(int index);
  void StoreInt32StackSlot(int index);
  void StoreUint32StackSlot(int index);
  void StoreBoolStackSlot(int index);
  void StoreDoubleStackSlot(int index);
  void StoreLiteral(int literal_id);
  void StoreArgumentsObject(bool args_known, int args_index, int args_length);

  Zone* zone() const { return zone_; }

  static int NumberOfOperandsFor(Opcode opcode);

#if defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)
  static const char* StringFor(Opcode opcode);
#endif

  // A literal id which refers to the JSFunction itself.
  static const int kSelfLiteralId = -239;

 private:
  TranslationBuffer* buffer_;
  int index_;
  Zone* zone_;
};


class SlotRef BASE_EMBEDDED {
 public:
  enum SlotRepresentation {
    UNKNOWN,
    TAGGED,
    INT32,
    UINT32,
    BOOLBIT,
    DOUBLE,
    LITERAL,
    DEFERRED_OBJECT,   // Object captured by the escape analysis.
                       // The number of nested objects can be obtained
                       // with the DeferredObjectLength() method
                       // (the SlotRefs of the nested objects follow
                       // this SlotRef in the depth-first order.)
    DUPLICATE_OBJECT,  // Duplicated object of a deferred object.
    ARGUMENTS_OBJECT   // Arguments object - only used to keep indexing
                       // in sync, it should not be materialized.
  };

  SlotRef()
      : addr_(NULL), representation_(UNKNOWN) { }

  SlotRef(Address addr, SlotRepresentation representation)
      : addr_(addr), representation_(representation) { }

  SlotRef(Isolate* isolate, Object* literal)
      : literal_(literal, isolate), representation_(LITERAL) { }

  static SlotRef NewArgumentsObject(int length) {
    SlotRef slot;
    slot.representation_ = ARGUMENTS_OBJECT;
    slot.deferred_object_length_ = length;
    return slot;
  }

  static SlotRef NewDeferredObject(int length) {
    SlotRef slot;
    slot.representation_ = DEFERRED_OBJECT;
    slot.deferred_object_length_ = length;
    return slot;
  }

  SlotRepresentation Representation() { return representation_; }

  static SlotRef NewDuplicateObject(int id) {
    SlotRef slot;
    slot.representation_ = DUPLICATE_OBJECT;
    slot.duplicate_object_id_ = id;
    return slot;
  }

  int GetChildrenCount() {
    if (representation_ == DEFERRED_OBJECT ||
        representation_ == ARGUMENTS_OBJECT) {
      return deferred_object_length_;
    } else {
      return 0;
    }
  }

  int DuplicateObjectId() { return duplicate_object_id_; }

  Handle<Object> GetValue(Isolate* isolate);

 private:
  Address addr_;
  Handle<Object> literal_;
  SlotRepresentation representation_;
  int deferred_object_length_;
  int duplicate_object_id_;
};

class SlotRefValueBuilder BASE_EMBEDDED {
 public:
  SlotRefValueBuilder(
      JavaScriptFrame* frame,
      int inlined_frame_index,
      int formal_parameter_count);

  void Prepare(Isolate* isolate);
  Handle<Object> GetNext(Isolate* isolate, int level);
  void Finish(Isolate* isolate);

  int args_length() { return args_length_; }

 private:
  List<Handle<Object> > materialized_objects_;
  Handle<FixedArray> previously_materialized_objects_;
  int prev_materialized_count_;
  Address stack_frame_id_;
  List<SlotRef> slot_refs_;
  int current_slot_;
  int args_length_;
  int first_slot_index_;
  bool should_deoptimize_;

  static SlotRef ComputeSlotForNextArgument(
      Translation::Opcode opcode,
      TranslationIterator* iterator,
      DeoptimizationInputData* data,
      JavaScriptFrame* frame);

  Handle<Object> GetPreviouslyMaterialized(Isolate* isolate, int length);

  static Address SlotAddress(JavaScriptFrame* frame, int slot_index) {
    if (slot_index >= 0) {
      const int offset = JavaScriptFrameConstants::kLocal0Offset;
      return frame->fp() + offset - (slot_index * kPointerSize);
    } else {
      const int offset = JavaScriptFrameConstants::kLastParameterOffset;
      return frame->fp() + offset - ((slot_index + 1) * kPointerSize);
    }
  }

  Handle<Object> GetDeferredObject(Isolate* isolate);
};

class MaterializedObjectStore {
 public:
  explicit MaterializedObjectStore(Isolate* isolate) : isolate_(isolate) {
  }

  Handle<FixedArray> Get(Address fp);
  void Set(Address fp, Handle<FixedArray> materialized_objects);
  bool Remove(Address fp);

 private:
  Isolate* isolate() { return isolate_; }
  Handle<FixedArray> GetStackEntries();
  Handle<FixedArray> EnsureStackEntries(int size);

  int StackIdToIndex(Address fp);

  Isolate* isolate_;
  List<Address> frame_fps_;
};


// Class used to represent an unoptimized frame when the debugger
// needs to inspect a frame that is part of an optimized frame. The
// internally used FrameDescription objects are not GC safe so for use
// by the debugger frame information is copied to an object of this type.
// Represents parameters in unadapted form so their number might mismatch
// formal parameter count.
class DeoptimizedFrameInfo : public Malloced {
 public:
  DeoptimizedFrameInfo(Deoptimizer* deoptimizer,
                       int frame_index,
                       bool has_arguments_adaptor,
                       bool has_construct_stub);
  virtual ~DeoptimizedFrameInfo();

  // GC support.
  void Iterate(ObjectVisitor* v);

  // Return the number of incoming arguments.
  int parameters_count() { return parameters_count_; }

  // Return the height of the expression stack.
  int expression_count() { return expression_count_; }

  // Get the frame function.
  JSFunction* GetFunction() {
    return function_;
  }

  // Get the frame context.
  Object* GetContext() { return context_; }

  // Check if this frame is preceded by construct stub frame.  The bottom-most
  // inlined frame might still be called by an uninlined construct stub.
  bool HasConstructStub() {
    return has_construct_stub_;
  }

  // Get an incoming argument.
  Object* GetParameter(int index) {
    DCHECK(0 <= index && index < parameters_count());
    return parameters_[index];
  }

  // Get an expression from the expression stack.
  Object* GetExpression(int index) {
    DCHECK(0 <= index && index < expression_count());
    return expression_stack_[index];
  }

  int GetSourcePosition() {
    return source_position_;
  }

 private:
  // Set an incoming argument.
  void SetParameter(int index, Object* obj) {
    DCHECK(0 <= index && index < parameters_count());
    parameters_[index] = obj;
  }

  // Set an expression on the expression stack.
  void SetExpression(int index, Object* obj) {
    DCHECK(0 <= index && index < expression_count());
    expression_stack_[index] = obj;
  }

  JSFunction* function_;
  Object* context_;
  bool has_construct_stub_;
  int parameters_count_;
  int expression_count_;
  Object** parameters_;
  Object** expression_stack_;
  int source_position_;

  friend class Deoptimizer;
};

} }  // namespace v8::internal

#endif  // V8_DEOPTIMIZER_H_
