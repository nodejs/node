// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_DEOPTIMIZER_H_
#define V8_DEOPTIMIZER_DEOPTIMIZER_H_

#include <stack>
#include <vector>

#include "src/base/macros.h"
#include "src/codegen/label.h"
#include "src/codegen/register-arch.h"
#include "src/codegen/source-position.h"
#include "src/common/globals.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/isolate.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/shared-function-info.h"
#include "src/utils/allocation.h"
#include "src/utils/boxed-float.h"
#include "src/zone/zone-chunk-list.h"

namespace v8 {
namespace internal {

class FrameDescription;
class JavaScriptFrame;
class TranslationIterator;
class DeoptimizedFrameInfo;
class TranslatedFrame;
class TranslatedState;
class RegisterValues;
class MacroAssembler;

enum class BuiltinContinuationMode;

class TranslatedValue {
 public:
  // Allocation-less getter of the value.
  // Returns ReadOnlyRoots::arguments_marker() if allocation would be necessary
  // to get the value.
  Object GetRawValue() const;

  // Getter for the value, takes care of materializing the subgraph
  // reachable from this value.
  Handle<Object> GetValue();

  bool IsMaterializedObject() const;
  bool IsMaterializableByDebugger() const;

 private:
  friend class TranslatedState;
  friend class TranslatedFrame;

  enum Kind : uint8_t {
    kInvalid,
    kTagged,
    kInt32,
    kInt64,
    kUInt32,
    kBoolBit,
    kFloat,
    kDouble,
    kCapturedObject,   // Object captured by the escape analysis.
                       // The number of nested objects can be obtained
                       // with the DeferredObjectLength() method
                       // (the values of the nested objects follow
                       // this value in the depth-first order.)
    kDuplicatedObject  // Duplicated object of a deferred object.
  };

  enum MaterializationState : uint8_t {
    kUninitialized,
    kAllocated,  // Storage for the object has been allocated (or
                 // enqueued for allocation).
    kFinished,   // The object has been initialized (or enqueued for
                 // initialization).
  };

  TranslatedValue(TranslatedState* container, Kind kind)
      : kind_(kind), container_(container) {}
  Kind kind() const { return kind_; }
  MaterializationState materialization_state() const {
    return materialization_state_;
  }
  void Handlify();
  int GetChildrenCount() const;

  static TranslatedValue NewDeferredObject(TranslatedState* container,
                                           int length, int object_index);
  static TranslatedValue NewDuplicateObject(TranslatedState* container, int id);
  static TranslatedValue NewFloat(TranslatedState* container, Float32 value);
  static TranslatedValue NewDouble(TranslatedState* container, Float64 value);
  static TranslatedValue NewInt32(TranslatedState* container, int32_t value);
  static TranslatedValue NewInt64(TranslatedState* container, int64_t value);
  static TranslatedValue NewUInt32(TranslatedState* container, uint32_t value);
  static TranslatedValue NewBool(TranslatedState* container, uint32_t value);
  static TranslatedValue NewTagged(TranslatedState* container, Object literal);
  static TranslatedValue NewInvalid(TranslatedState* container);

  Isolate* isolate() const;
  void MaterializeSimple();

  void set_storage(Handle<HeapObject> storage) { storage_ = storage; }
  void set_initialized_storage(Handle<Object> storage);
  void mark_finished() { materialization_state_ = kFinished; }
  void mark_allocated() { materialization_state_ = kAllocated; }

  Handle<Object> GetStorage() {
    DCHECK_NE(kUninitialized, materialization_state());
    return storage_;
  }

  Kind kind_;
  MaterializationState materialization_state_ = kUninitialized;
  TranslatedState* container_;  // This is only needed for materialization of
                                // objects and constructing handles (to get
                                // to the isolate).

  Handle<Object> storage_;  // Contains the materialized value or the
                            // byte-array that will be later morphed into
                            // the materialized object.

  struct MaterializedObjectInfo {
    int id_;
    int length_;  // Applies only to kCapturedObject kinds.
  };

  union {
    // kind kTagged. After handlification it is always nullptr.
    Object raw_literal_;
    // kind is kUInt32 or kBoolBit.
    uint32_t uint32_value_;
    // kind is kInt32.
    int32_t int32_value_;
    // kind is kInt64.
    int64_t int64_value_;
    // kind is kFloat
    Float32 float_value_;
    // kind is kDouble
    Float64 double_value_;
    // kind is kDuplicatedObject or kCapturedObject.
    MaterializedObjectInfo materialization_info_;
  };

  // Checked accessors for the union members.
  Object raw_literal() const;
  int32_t int32_value() const;
  int64_t int64_value() const;
  uint32_t uint32_value() const;
  Float32 float_value() const;
  Float64 double_value() const;
  int object_length() const;
  int object_index() const;
};

class TranslatedFrame {
 public:
  enum Kind {
    kInterpretedFunction,
    kArgumentsAdaptor,
    kConstructStub,
    kBuiltinContinuation,
    kJavaScriptBuiltinContinuation,
    kJavaScriptBuiltinContinuationWithCatch,
    kInvalid
  };

  int GetValueCount();

  Kind kind() const { return kind_; }
  BailoutId node_id() const { return node_id_; }
  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }

  // TODO(jgruber): Simplify/clarify the semantics of this field. The name
  // `height` is slightly misleading. Yes, this value is related to stack frame
  // height, but must undergo additional mutations to arrive at the real stack
  // frame height (e.g.: addition/subtraction of context, accumulator, fixed
  // frame sizes, padding).
  int height() const { return height_; }

  int return_value_offset() const { return return_value_offset_; }
  int return_value_count() const { return return_value_count_; }

  SharedFunctionInfo raw_shared_info() const {
    CHECK(!raw_shared_info_.is_null());
    return raw_shared_info_;
  }

  class iterator {
   public:
    iterator& operator++() {
      ++input_index_;
      AdvanceIterator(&position_);
      return *this;
    }

    iterator operator++(int) {
      iterator original(position_, input_index_);
      ++input_index_;
      AdvanceIterator(&position_);
      return original;
    }

    bool operator==(const iterator& other) const {
      // Ignore {input_index_} for equality.
      return position_ == other.position_;
    }
    bool operator!=(const iterator& other) const { return !(*this == other); }

    TranslatedValue& operator*() { return (*position_); }
    TranslatedValue* operator->() { return &(*position_); }
    const TranslatedValue& operator*() const { return (*position_); }
    const TranslatedValue* operator->() const { return &(*position_); }

    int input_index() const { return input_index_; }

   private:
    friend TranslatedFrame;

    explicit iterator(std::deque<TranslatedValue>::iterator position,
                      int input_index = 0)
        : position_(position), input_index_(input_index) {}

    std::deque<TranslatedValue>::iterator position_;
    int input_index_;
  };

  using reference = TranslatedValue&;
  using const_reference = TranslatedValue const&;

  iterator begin() { return iterator(values_.begin()); }
  iterator end() { return iterator(values_.end()); }

  reference front() { return values_.front(); }
  const_reference front() const { return values_.front(); }

 private:
  friend class TranslatedState;

  // Constructor static methods.
  static TranslatedFrame InterpretedFrame(BailoutId bytecode_offset,
                                          SharedFunctionInfo shared_info,
                                          int height, int return_value_offset,
                                          int return_value_count);
  static TranslatedFrame AccessorFrame(Kind kind,
                                       SharedFunctionInfo shared_info);
  static TranslatedFrame ArgumentsAdaptorFrame(SharedFunctionInfo shared_info,
                                               int height);
  static TranslatedFrame ConstructStubFrame(BailoutId bailout_id,
                                            SharedFunctionInfo shared_info,
                                            int height);
  static TranslatedFrame BuiltinContinuationFrame(
      BailoutId bailout_id, SharedFunctionInfo shared_info, int height);
  static TranslatedFrame JavaScriptBuiltinContinuationFrame(
      BailoutId bailout_id, SharedFunctionInfo shared_info, int height);
  static TranslatedFrame JavaScriptBuiltinContinuationWithCatchFrame(
      BailoutId bailout_id, SharedFunctionInfo shared_info, int height);
  static TranslatedFrame InvalidFrame() {
    return TranslatedFrame(kInvalid, SharedFunctionInfo());
  }

  static void AdvanceIterator(std::deque<TranslatedValue>::iterator* iter);

  TranslatedFrame(Kind kind,
                  SharedFunctionInfo shared_info = SharedFunctionInfo(),
                  int height = 0, int return_value_offset = 0,
                  int return_value_count = 0)
      : kind_(kind),
        node_id_(BailoutId::None()),
        raw_shared_info_(shared_info),
        height_(height),
        return_value_offset_(return_value_offset),
        return_value_count_(return_value_count) {}

  void Add(const TranslatedValue& value) { values_.push_back(value); }
  TranslatedValue* ValueAt(int index) { return &(values_[index]); }
  void Handlify();

  Kind kind_;
  BailoutId node_id_;
  SharedFunctionInfo raw_shared_info_;
  Handle<SharedFunctionInfo> shared_info_;
  int height_;
  int return_value_offset_;
  int return_value_count_;

  using ValuesContainer = std::deque<TranslatedValue>;

  ValuesContainer values_;
};

// Auxiliary class for translating deoptimization values.
// Typical usage sequence:
//
// 1. Construct the instance. This will involve reading out the translations
//    and resolving them to values using the supplied frame pointer and
//    machine state (registers). This phase is guaranteed not to allocate
//    and not to use any HandleScope. Any object pointers will be stored raw.
//
// 2. Handlify pointers. This will convert all the raw pointers to handles.
//
// 3. Reading out the frame values.
//
// Note: After the instance is constructed, it is possible to iterate over
// the values eagerly.

class TranslatedState {
 public:
  TranslatedState() = default;
  explicit TranslatedState(const JavaScriptFrame* frame);

  void Prepare(Address stack_frame_pointer);

  // Store newly materialized values into the isolate.
  void StoreMaterializedValuesAndDeopt(JavaScriptFrame* frame);

  using iterator = std::vector<TranslatedFrame>::iterator;
  iterator begin() { return frames_.begin(); }
  iterator end() { return frames_.end(); }

  using const_iterator = std::vector<TranslatedFrame>::const_iterator;
  const_iterator begin() const { return frames_.begin(); }
  const_iterator end() const { return frames_.end(); }

  std::vector<TranslatedFrame>& frames() { return frames_; }

  TranslatedFrame* GetFrameFromJSFrameIndex(int jsframe_index);
  TranslatedFrame* GetArgumentsInfoFromJSFrameIndex(int jsframe_index,
                                                    int* arguments_count);

  Isolate* isolate() { return isolate_; }

  void Init(Isolate* isolate, Address input_frame_pointer,
            TranslationIterator* iterator, FixedArray literal_array,
            RegisterValues* registers, FILE* trace_file, int parameter_count);

  void VerifyMaterializedObjects();
  bool DoUpdateFeedback();

 private:
  friend TranslatedValue;

  TranslatedFrame CreateNextTranslatedFrame(TranslationIterator* iterator,
                                            FixedArray literal_array,
                                            Address fp, FILE* trace_file);
  int CreateNextTranslatedValue(int frame_index, TranslationIterator* iterator,
                                FixedArray literal_array, Address fp,
                                RegisterValues* registers, FILE* trace_file);
  Address DecompressIfNeeded(intptr_t value);
  Address ComputeArgumentsPosition(Address input_frame_pointer,
                                   CreateArgumentsType type, int* length);
  void CreateArgumentsElementsTranslatedValues(int frame_index,
                                               Address input_frame_pointer,
                                               CreateArgumentsType type,
                                               FILE* trace_file);

  void UpdateFromPreviouslyMaterializedObjects();
  void MaterializeFixedDoubleArray(TranslatedFrame* frame, int* value_index,
                                   TranslatedValue* slot, Handle<Map> map);
  void MaterializeHeapNumber(TranslatedFrame* frame, int* value_index,
                             TranslatedValue* slot);

  void EnsureObjectAllocatedAt(TranslatedValue* slot);

  void SkipSlots(int slots_to_skip, TranslatedFrame* frame, int* value_index);

  Handle<ByteArray> AllocateStorageFor(TranslatedValue* slot);
  void EnsureJSObjectAllocated(TranslatedValue* slot, Handle<Map> map);
  void EnsurePropertiesAllocatedAndMarked(TranslatedValue* properties_slot,
                                          Handle<Map> map);
  void EnsureChildrenAllocated(int count, TranslatedFrame* frame,
                               int* value_index, std::stack<int>* worklist);
  void EnsureCapturedObjectAllocatedAt(int object_index,
                                       std::stack<int>* worklist);
  Handle<Object> InitializeObjectAt(TranslatedValue* slot);
  void InitializeCapturedObjectAt(int object_index, std::stack<int>* worklist,
                                  const DisallowHeapAllocation& no_allocation);
  void InitializeJSObjectAt(TranslatedFrame* frame, int* value_index,
                            TranslatedValue* slot, Handle<Map> map,
                            const DisallowHeapAllocation& no_allocation);
  void InitializeObjectWithTaggedFieldsAt(
      TranslatedFrame* frame, int* value_index, TranslatedValue* slot,
      Handle<Map> map, const DisallowHeapAllocation& no_allocation);

  void ReadUpdateFeedback(TranslationIterator* iterator,
                          FixedArray literal_array, FILE* trace_file);

  TranslatedValue* ResolveCapturedObject(TranslatedValue* slot);
  TranslatedValue* GetValueByObjectIndex(int object_index);
  Handle<Object> GetValueAndAdvance(TranslatedFrame* frame, int* value_index);

  static uint32_t GetUInt32Slot(Address fp, int slot_index);
  static uint64_t GetUInt64Slot(Address fp, int slot_index);
  static Float32 GetFloatSlot(Address fp, int slot_index);
  static Float64 GetDoubleSlot(Address fp, int slot_index);

  std::vector<TranslatedFrame> frames_;
  Isolate* isolate_ = nullptr;
  Address stack_frame_pointer_ = kNullAddress;
  int formal_parameter_count_;

  struct ObjectPosition {
    int frame_index_;
    int value_index_;
  };
  std::deque<ObjectPosition> object_positions_;
  Handle<FeedbackVector> feedback_vector_handle_;
  FeedbackVector feedback_vector_;
  FeedbackSlot feedback_slot_;
};

class OptimizedFunctionVisitor {
 public:
  virtual ~OptimizedFunctionVisitor() = default;
  virtual void VisitFunction(JSFunction function) = 0;
};

class Deoptimizer : public Malloced {
 public:
  struct DeoptInfo {
    DeoptInfo(SourcePosition position, DeoptimizeReason deopt_reason,
              int deopt_id)
        : position(position), deopt_reason(deopt_reason), deopt_id(deopt_id) {}

    SourcePosition position;
    DeoptimizeReason deopt_reason;
    int deopt_id;

    static const int kNoDeoptId = -1;
  };

  static DeoptInfo GetDeoptInfo(Code code, Address from);

  static int ComputeSourcePositionFromBytecodeArray(SharedFunctionInfo shared,
                                                    BailoutId node_id);

  static const char* MessageFor(DeoptimizeKind kind);

  int output_count() const { return output_count_; }

  Handle<JSFunction> function() const;
  Handle<Code> compiled_code() const;
  DeoptimizeKind deopt_kind() const { return deopt_kind_; }

  // Number of created JS frames. Not all created frames are necessarily JS.
  int jsframe_count() const { return jsframe_count_; }

  static Deoptimizer* New(Address raw_function, DeoptimizeKind kind,
                          unsigned bailout_id, Address from, int fp_to_sp_delta,
                          Isolate* isolate);
  static Deoptimizer* Grab(Isolate* isolate);

  // The returned object with information on the optimized frame needs to be
  // freed before another one can be generated.
  static DeoptimizedFrameInfo* DebuggerInspectableFrame(JavaScriptFrame* frame,
                                                        int jsframe_index,
                                                        Isolate* isolate);

  // Deoptimize the function now. Its current optimized code will never be run
  // again and any activations of the optimized code will get deoptimized when
  // execution returns. If {code} is specified then the given code is targeted
  // instead of the function code (e.g. OSR code not installed on function).
  static void DeoptimizeFunction(JSFunction function, Code code = Code());

  // Deoptimize all code in the given isolate.
  V8_EXPORT_PRIVATE static void DeoptimizeAll(Isolate* isolate);

  // Deoptimizes all optimized code that has been previously marked
  // (via code->set_marked_for_deoptimization) and unlinks all functions that
  // refer to that code.
  static void DeoptimizeMarkedCode(Isolate* isolate);

  ~Deoptimizer();

  void MaterializeHeapObjects();

  static void ComputeOutputFrames(Deoptimizer* deoptimizer);

  static Address GetDeoptimizationEntry(Isolate* isolate, DeoptimizeKind kind);

  // Returns true if {addr} is a deoptimization entry and stores its type in
  // {type}. Returns false if {addr} is not a deoptimization entry.
  static bool IsDeoptimizationEntry(Isolate* isolate, Address addr,
                                    DeoptimizeKind* type);

  // Code generation support.
  static int input_offset() { return offsetof(Deoptimizer, input_); }
  static int output_count_offset() {
    return offsetof(Deoptimizer, output_count_);
  }
  static int output_offset() { return offsetof(Deoptimizer, output_); }

  static int caller_frame_top_offset() {
    return offsetof(Deoptimizer, caller_frame_top_);
  }

  V8_EXPORT_PRIVATE static int GetDeoptimizedCodeCount(Isolate* isolate);

  static const int kNotDeoptimizationEntry = -1;

  static void EnsureCodeForDeoptimizationEntry(Isolate* isolate,
                                               DeoptimizeKind kind);
  static void EnsureCodeForDeoptimizationEntries(Isolate* isolate);

  Isolate* isolate() const { return isolate_; }

  static const int kMaxNumberOfEntries = 16384;

  // Set to true when the architecture supports deoptimization exit sequences
  // of a fixed size, that can be sorted so that the deoptimization index is
  // deduced from the address of the deoptimization exit.
  static const bool kSupportsFixedDeoptExitSize;

  // Size of deoptimization exit sequence. This is only meaningful when
  // kSupportsFixedDeoptExitSize is true.
  static const int kDeoptExitSize;

 private:
  friend class FrameWriter;
  void QueueValueForMaterialization(Address output_address, Object obj,
                                    const TranslatedFrame::iterator& iterator);

  Deoptimizer(Isolate* isolate, JSFunction function, DeoptimizeKind kind,
              unsigned bailout_id, Address from, int fp_to_sp_delta);
  Code FindOptimizedCode();
  void PrintFunctionName();
  void DeleteFrameDescriptions();

  static bool IsDeoptimizationEntry(Isolate* isolate, Address addr,
                                    DeoptimizeKind type);

  void DoComputeOutputFrames();
  void DoComputeInterpretedFrame(TranslatedFrame* translated_frame,
                                 int frame_index, bool goto_catch_handler);
  void DoComputeArgumentsAdaptorFrame(TranslatedFrame* translated_frame,
                                      int frame_index);
  void DoComputeConstructStubFrame(TranslatedFrame* translated_frame,
                                   int frame_index);

  static Builtins::Name TrampolineForBuiltinContinuation(
      BuiltinContinuationMode mode, bool must_handle_result);

  void DoComputeBuiltinContinuation(TranslatedFrame* translated_frame,
                                    int frame_index,
                                    BuiltinContinuationMode mode);

  unsigned ComputeInputFrameAboveFpFixedSize() const;
  unsigned ComputeInputFrameSize() const;

  static unsigned ComputeIncomingArgumentSize(SharedFunctionInfo shared);
  static unsigned ComputeOutgoingArgumentSize(Code code, unsigned bailout_id);

  static void GenerateDeoptimizationEntries(MacroAssembler* masm,
                                            Isolate* isolate,
                                            DeoptimizeKind kind);

  static void MarkAllCodeForContext(NativeContext native_context);
  static void DeoptimizeMarkedCodeForContext(NativeContext native_context);
  // Searches the list of known deoptimizing code for a Code object
  // containing the given address (which is supposedly faster than
  // searching all code objects).
  Code FindDeoptimizingCode(Address addr);

  Isolate* isolate_;
  JSFunction function_;
  Code compiled_code_;
  unsigned bailout_id_;
  DeoptimizeKind deopt_kind_;
  Address from_;
  int fp_to_sp_delta_;
  bool deoptimizing_throw_;
  int catch_handler_data_;
  int catch_handler_pc_offset_;

  // Input frame description.
  FrameDescription* input_;
  // Number of output frames.
  int output_count_;
  // Number of output js frames.
  int jsframe_count_;
  // Array of output frame descriptions.
  FrameDescription** output_;

  // Caller frame details computed from input frame.
  intptr_t caller_frame_top_;
  intptr_t caller_fp_;
  intptr_t caller_pc_;
  intptr_t caller_constant_pool_;
  intptr_t input_frame_context_;

  // Key for lookup of previously materialized objects
  intptr_t stack_fp_;

  TranslatedState translated_state_;
  struct ValueToMaterialize {
    Address output_slot_address_;
    TranslatedFrame::iterator value_;
  };
  std::vector<ValueToMaterialize> values_to_materialize_;

#ifdef DEBUG
  DisallowHeapAllocation* disallow_heap_allocation_;
#endif  // DEBUG

  CodeTracer::Scope* trace_scope_;

  static const int table_entry_size_;

  friend class FrameDescription;
  friend class DeoptimizedFrameInfo;
};

class RegisterValues {
 public:
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

  Float32 GetFloatRegister(unsigned n) const;

  Float64 GetDoubleRegister(unsigned n) const {
    DCHECK(n < arraysize(double_registers_));
    return double_registers_[n];
  }

  void SetRegister(unsigned n, intptr_t value) {
    DCHECK(n < arraysize(registers_));
    registers_[n] = value;
  }

  intptr_t registers_[Register::kNumRegisters];
  // Generated code writes directly into the following array, make sure the
  // element size matches what the machine instructions expect.
  static_assert(sizeof(Float64) == kDoubleSize, "size mismatch");
  Float64 double_registers_[DoubleRegister::kNumRegisters];
};

class FrameDescription {
 public:
  explicit FrameDescription(uint32_t frame_size, int parameter_count = 0);

  void* operator new(size_t size, uint32_t frame_size) {
    // Subtracts kSystemPointerSize, as the member frame_content_ already
    // supplies the first element of the area to store the frame.
    return malloc(size + frame_size - kSystemPointerSize);
  }

  void operator delete(void* pointer, uint32_t frame_size) { free(pointer); }

  void operator delete(void* description) { free(description); }

  uint32_t GetFrameSize() const {
    USE(frame_content_);
    DCHECK(static_cast<uint32_t>(frame_size_) == frame_size_);
    return static_cast<uint32_t>(frame_size_);
  }

  intptr_t GetFrameSlot(unsigned offset) {
    return *GetFrameSlotPointer(offset);
  }

  unsigned GetLastArgumentSlotOffset() {
    int parameter_slots = parameter_count();
    if (ShouldPadArguments(parameter_slots)) parameter_slots++;
    return GetFrameSize() - parameter_slots * kSystemPointerSize;
  }

  Address GetFramePointerAddress() {
    int fp_offset =
        GetLastArgumentSlotOffset() - StandardFrameConstants::kCallerSPOffset;
    return reinterpret_cast<Address>(GetFrameSlotPointer(fp_offset));
  }

  RegisterValues* GetRegisterValues() { return &register_values_; }

  void SetFrameSlot(unsigned offset, intptr_t value) {
    *GetFrameSlotPointer(offset) = value;
  }

  void SetCallerPc(unsigned offset, intptr_t value);

  void SetCallerFp(unsigned offset, intptr_t value);

  void SetCallerConstantPool(unsigned offset, intptr_t value);

  intptr_t GetRegister(unsigned n) const {
    return register_values_.GetRegister(n);
  }

  Float64 GetDoubleRegister(unsigned n) const {
    return register_values_.GetDoubleRegister(n);
  }

  void SetRegister(unsigned n, intptr_t value) {
    register_values_.SetRegister(n, value);
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

  void SetContinuation(intptr_t pc) { continuation_ = pc; }

  // Argument count, including receiver.
  int parameter_count() { return parameter_count_; }

  static int registers_offset() {
    return offsetof(FrameDescription, register_values_.registers_);
  }

  static int double_registers_offset() {
    return offsetof(FrameDescription, register_values_.double_registers_);
  }

  static int frame_size_offset() {
    return offsetof(FrameDescription, frame_size_);
  }

  static int pc_offset() { return offsetof(FrameDescription, pc_); }

  static int continuation_offset() {
    return offsetof(FrameDescription, continuation_);
  }

  static int frame_content_offset() {
    return offsetof(FrameDescription, frame_content_);
  }

 private:
  static const uint32_t kZapUint32 = 0xbeeddead;

  // Frame_size_ must hold a uint32_t value.  It is only a uintptr_t to
  // keep the variable-size array frame_content_ of type intptr_t at
  // the end of the structure aligned.
  uintptr_t frame_size_;  // Number of bytes.
  int parameter_count_;
  RegisterValues register_values_;
  intptr_t top_;
  intptr_t pc_;
  intptr_t fp_;
  intptr_t context_;
  intptr_t constant_pool_;

  // Continuation is the PC where the execution continues after
  // deoptimizing.
  intptr_t continuation_;

  // This must be at the end of the object as the object is allocated larger
  // than it's definition indicate to extend this array.
  intptr_t frame_content_[1];

  intptr_t* GetFrameSlotPointer(unsigned offset) {
    DCHECK(offset < frame_size_);
    return reinterpret_cast<intptr_t*>(reinterpret_cast<Address>(this) +
                                       frame_content_offset() + offset);
  }
};

class DeoptimizerData {
 public:
  explicit DeoptimizerData(Heap* heap);
  ~DeoptimizerData();

#ifdef DEBUG
  bool IsDeoptEntryCode(Code code) const {
    for (int i = 0; i < kLastDeoptimizeKind + 1; i++) {
      if (code == deopt_entry_code_[i]) return true;
    }
    return false;
  }
#endif  // DEBUG

 private:
  Heap* heap_;
  static const int kLastDeoptimizeKind =
      static_cast<int>(DeoptimizeKind::kLastDeoptimizeKind);
  Code deopt_entry_code_[kLastDeoptimizeKind + 1];
  Code deopt_entry_code(DeoptimizeKind kind);
  void set_deopt_entry_code(DeoptimizeKind kind, Code code);

  Deoptimizer* current_;

  friend class Deoptimizer;

  DISALLOW_COPY_AND_ASSIGN(DeoptimizerData);
};

class TranslationBuffer {
 public:
  explicit TranslationBuffer(Zone* zone) : contents_(zone) {}

  int CurrentIndex() const { return static_cast<int>(contents_.size()); }
  void Add(int32_t value);

  Handle<ByteArray> CreateByteArray(Factory* factory);

 private:
  ZoneChunkList<uint8_t> contents_;
};

class TranslationIterator {
 public:
  TranslationIterator(ByteArray buffer, int index);

  int32_t Next();

  bool HasNext() const;

  void Skip(int n) {
    for (int i = 0; i < n; i++) Next();
  }

 private:
  ByteArray buffer_;
  int index_;
};

#define TRANSLATION_OPCODE_LIST(V)                     \
  V(BEGIN)                                             \
  V(INTERPRETED_FRAME)                                 \
  V(BUILTIN_CONTINUATION_FRAME)                        \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME)            \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME) \
  V(CONSTRUCT_STUB_FRAME)                              \
  V(ARGUMENTS_ADAPTOR_FRAME)                           \
  V(DUPLICATED_OBJECT)                                 \
  V(ARGUMENTS_ELEMENTS)                                \
  V(ARGUMENTS_LENGTH)                                  \
  V(CAPTURED_OBJECT)                                   \
  V(REGISTER)                                          \
  V(INT32_REGISTER)                                    \
  V(INT64_REGISTER)                                    \
  V(UINT32_REGISTER)                                   \
  V(BOOL_REGISTER)                                     \
  V(FLOAT_REGISTER)                                    \
  V(DOUBLE_REGISTER)                                   \
  V(STACK_SLOT)                                        \
  V(INT32_STACK_SLOT)                                  \
  V(INT64_STACK_SLOT)                                  \
  V(UINT32_STACK_SLOT)                                 \
  V(BOOL_STACK_SLOT)                                   \
  V(FLOAT_STACK_SLOT)                                  \
  V(DOUBLE_STACK_SLOT)                                 \
  V(LITERAL)                                           \
  V(UPDATE_FEEDBACK)

class Translation {
 public:
#define DECLARE_TRANSLATION_OPCODE_ENUM(item) item,
  enum Opcode {
    TRANSLATION_OPCODE_LIST(DECLARE_TRANSLATION_OPCODE_ENUM) LAST = LITERAL
  };
#undef DECLARE_TRANSLATION_OPCODE_ENUM

  Translation(TranslationBuffer* buffer, int frame_count, int jsframe_count,
              int update_feedback_count, Zone* zone)
      : buffer_(buffer), index_(buffer->CurrentIndex()), zone_(zone) {
    buffer_->Add(BEGIN);
    buffer_->Add(frame_count);
    buffer_->Add(jsframe_count);
    buffer_->Add(update_feedback_count);
  }

  int index() const { return index_; }

  // Commands.
  void BeginInterpretedFrame(BailoutId bytecode_offset, int literal_id,
                             unsigned height, int return_value_offset,
                             int return_value_count);
  void BeginArgumentsAdaptorFrame(int literal_id, unsigned height);
  void BeginConstructStubFrame(BailoutId bailout_id, int literal_id,
                               unsigned height);
  void BeginBuiltinContinuationFrame(BailoutId bailout_id, int literal_id,
                                     unsigned height);
  void BeginJavaScriptBuiltinContinuationFrame(BailoutId bailout_id,
                                               int literal_id, unsigned height);
  void BeginJavaScriptBuiltinContinuationWithCatchFrame(BailoutId bailout_id,
                                                        int literal_id,
                                                        unsigned height);
  void ArgumentsElements(CreateArgumentsType type);
  void ArgumentsLength(CreateArgumentsType type);
  void BeginCapturedObject(int length);
  void AddUpdateFeedback(int vector_literal, int slot);
  void DuplicateObject(int object_index);
  void StoreRegister(Register reg);
  void StoreInt32Register(Register reg);
  void StoreInt64Register(Register reg);
  void StoreUint32Register(Register reg);
  void StoreBoolRegister(Register reg);
  void StoreFloatRegister(FloatRegister reg);
  void StoreDoubleRegister(DoubleRegister reg);
  void StoreStackSlot(int index);
  void StoreInt32StackSlot(int index);
  void StoreInt64StackSlot(int index);
  void StoreUint32StackSlot(int index);
  void StoreBoolStackSlot(int index);
  void StoreFloatStackSlot(int index);
  void StoreDoubleStackSlot(int index);
  void StoreLiteral(int literal_id);
  void StoreJSFrameFunction();

  Zone* zone() const { return zone_; }

  static int NumberOfOperandsFor(Opcode opcode);

#if defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)
  static const char* StringFor(Opcode opcode);
#endif

 private:
  TranslationBuffer* buffer_;
  int index_;
  Zone* zone_;
};

class MaterializedObjectStore {
 public:
  explicit MaterializedObjectStore(Isolate* isolate) : isolate_(isolate) {}

  Handle<FixedArray> Get(Address fp);
  void Set(Address fp, Handle<FixedArray> materialized_objects);
  bool Remove(Address fp);

 private:
  Isolate* isolate() const { return isolate_; }
  Handle<FixedArray> GetStackEntries();
  Handle<FixedArray> EnsureStackEntries(int size);

  int StackIdToIndex(Address fp);

  Isolate* isolate_;
  std::vector<Address> frame_fps_;
};

// Class used to represent an unoptimized frame when the debugger
// needs to inspect a frame that is part of an optimized frame. The
// internally used FrameDescription objects are not GC safe so for use
// by the debugger frame information is copied to an object of this type.
// Represents parameters in unadapted form so their number might mismatch
// formal parameter count.
class DeoptimizedFrameInfo : public Malloced {
 public:
  DeoptimizedFrameInfo(TranslatedState* state,
                       TranslatedState::iterator frame_it, Isolate* isolate);

  // Return the number of incoming arguments.
  int parameters_count() { return static_cast<int>(parameters_.size()); }

  // Return the height of the expression stack.
  int expression_count() { return static_cast<int>(expression_stack_.size()); }

  // Get the frame function.
  Handle<JSFunction> GetFunction() { return function_; }

  // Get the frame context.
  Handle<Object> GetContext() { return context_; }

  // Get an incoming argument.
  Handle<Object> GetParameter(int index) {
    DCHECK(0 <= index && index < parameters_count());
    return parameters_[index];
  }

  // Get an expression from the expression stack.
  Handle<Object> GetExpression(int index) {
    DCHECK(0 <= index && index < expression_count());
    return expression_stack_[index];
  }

  int GetSourcePosition() { return source_position_; }

 private:
  // Set an incoming argument.
  void SetParameter(int index, Handle<Object> obj) {
    DCHECK(0 <= index && index < parameters_count());
    parameters_[index] = obj;
  }

  // Set an expression on the expression stack.
  void SetExpression(int index, Handle<Object> obj) {
    DCHECK(0 <= index && index < expression_count());
    expression_stack_[index] = obj;
  }

  Handle<JSFunction> function_;
  Handle<Object> context_;
  std::vector<Handle<Object> > parameters_;
  std::vector<Handle<Object> > expression_stack_;
  int source_position_;

  friend class Deoptimizer;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_DEOPTIMIZER_H_
