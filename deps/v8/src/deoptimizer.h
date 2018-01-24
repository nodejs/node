// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_H_
#define V8_DEOPTIMIZER_H_

#include <vector>

#include "src/allocation.h"
#include "src/base/macros.h"
#include "src/boxed-float.h"
#include "src/deoptimize-reason.h"
#include "src/frame-constants.h"
#include "src/macro-assembler.h"
#include "src/source-position.h"
#include "src/zone/zone-chunk-list.h"

namespace v8 {
namespace internal {

class FrameDescription;
class TranslationIterator;
class DeoptimizedFrameInfo;
class TranslatedState;
class RegisterValues;

class TranslatedValue {
 public:
  // Allocation-less getter of the value.
  // Returns heap()->arguments_marker() if allocation would be
  // necessary to get the value.
  Object* GetRawValue() const;
  Handle<Object> GetValue();

  bool IsMaterializedObject() const;
  bool IsMaterializableByDebugger() const;

 private:
  friend class TranslatedState;
  friend class TranslatedFrame;

  enum Kind {
    kInvalid,
    kTagged,
    kInt32,
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

  TranslatedValue(TranslatedState* container, Kind kind)
      : kind_(kind), container_(container) {}
  Kind kind() const { return kind_; }
  void Handlify();
  int GetChildrenCount() const;

  static TranslatedValue NewDeferredObject(TranslatedState* container,
                                           int length, int object_index);
  static TranslatedValue NewDuplicateObject(TranslatedState* container, int id);
  static TranslatedValue NewFloat(TranslatedState* container, Float32 value);
  static TranslatedValue NewDouble(TranslatedState* container, Float64 value);
  static TranslatedValue NewInt32(TranslatedState* container, int32_t value);
  static TranslatedValue NewUInt32(TranslatedState* container, uint32_t value);
  static TranslatedValue NewBool(TranslatedState* container, uint32_t value);
  static TranslatedValue NewTagged(TranslatedState* container, Object* literal);
  static TranslatedValue NewInvalid(TranslatedState* container);

  Isolate* isolate() const;
  void MaterializeSimple();

  Kind kind_;
  TranslatedState* container_;  // This is only needed for materialization of
                                // objects and constructing handles (to get
                                // to the isolate).

  MaybeHandle<Object> value_;  // Before handlification, this is always null,
                               // after materialization it is never null,
                               // in between it is only null if the value needs
                               // to be materialized.

  struct MaterializedObjectInfo {
    int id_;
    int length_;  // Applies only to kCapturedObject kinds.
  };

  union {
    // kind kTagged. After handlification it is always nullptr.
    Object* raw_literal_;
    // kind is kUInt32 or kBoolBit.
    uint32_t uint32_value_;
    // kind is kInt32.
    int32_t int32_value_;
    // kind is kFloat
    Float32 float_value_;
    // kind is kDouble
    Float64 double_value_;
    // kind is kDuplicatedObject or kCapturedObject.
    MaterializedObjectInfo materialization_info_;
  };

  // Checked accessors for the union members.
  Object* raw_literal() const;
  int32_t int32_value() const;
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
    kInvalid
  };

  int GetValueCount();

  Kind kind() const { return kind_; }
  BailoutId node_id() const { return node_id_; }
  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  int height() const { return height_; }

  SharedFunctionInfo* raw_shared_info() const {
    CHECK_NOT_NULL(raw_shared_info_);
    return raw_shared_info_;
  }

  class iterator {
   public:
    iterator& operator++() {
      AdvanceIterator(&position_);
      return *this;
    }

    iterator operator++(int) {
      iterator original(position_);
      AdvanceIterator(&position_);
      return original;
    }

    bool operator==(const iterator& other) const {
      return position_ == other.position_;
    }
    bool operator!=(const iterator& other) const { return !(*this == other); }

    TranslatedValue& operator*() { return (*position_); }
    TranslatedValue* operator->() { return &(*position_); }

   private:
    friend TranslatedFrame;

    explicit iterator(std::deque<TranslatedValue>::iterator position)
        : position_(position) {}

    std::deque<TranslatedValue>::iterator position_;
  };

  typedef TranslatedValue& reference;
  typedef TranslatedValue const& const_reference;

  iterator begin() { return iterator(values_.begin()); }
  iterator end() { return iterator(values_.end()); }

  reference front() { return values_.front(); }
  const_reference front() const { return values_.front(); }

 private:
  friend class TranslatedState;

  // Constructor static methods.
  static TranslatedFrame InterpretedFrame(BailoutId bytecode_offset,
                                          SharedFunctionInfo* shared_info,
                                          int height);
  static TranslatedFrame AccessorFrame(Kind kind,
                                       SharedFunctionInfo* shared_info);
  static TranslatedFrame ArgumentsAdaptorFrame(SharedFunctionInfo* shared_info,
                                               int height);
  static TranslatedFrame ConstructStubFrame(BailoutId bailout_id,
                                            SharedFunctionInfo* shared_info,
                                            int height);
  static TranslatedFrame BuiltinContinuationFrame(
      BailoutId bailout_id, SharedFunctionInfo* shared_info, int height);
  static TranslatedFrame JavaScriptBuiltinContinuationFrame(
      BailoutId bailout_id, SharedFunctionInfo* shared_info, int height);
  static TranslatedFrame InvalidFrame() {
    return TranslatedFrame(kInvalid, nullptr);
  }

  static void AdvanceIterator(std::deque<TranslatedValue>::iterator* iter);

  TranslatedFrame(Kind kind, SharedFunctionInfo* shared_info = nullptr,
                  int height = 0)
      : kind_(kind),
        node_id_(BailoutId::None()),
        raw_shared_info_(shared_info),
        height_(height) {}

  void Add(const TranslatedValue& value) { values_.push_back(value); }
  void Handlify();

  Kind kind_;
  BailoutId node_id_;
  SharedFunctionInfo* raw_shared_info_;
  Handle<SharedFunctionInfo> shared_info_;
  int height_;

  typedef std::deque<TranslatedValue> ValuesContainer;

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
  TranslatedState();
  explicit TranslatedState(const JavaScriptFrame* frame);

  void Prepare(Address stack_frame_pointer);

  // Store newly materialized values into the isolate.
  void StoreMaterializedValuesAndDeopt(JavaScriptFrame* frame);

  typedef std::vector<TranslatedFrame>::iterator iterator;
  iterator begin() { return frames_.begin(); }
  iterator end() { return frames_.end(); }

  typedef std::vector<TranslatedFrame>::const_iterator const_iterator;
  const_iterator begin() const { return frames_.begin(); }
  const_iterator end() const { return frames_.end(); }

  std::vector<TranslatedFrame>& frames() { return frames_; }

  TranslatedFrame* GetFrameFromJSFrameIndex(int jsframe_index);
  TranslatedFrame* GetArgumentsInfoFromJSFrameIndex(int jsframe_index,
                                                    int* arguments_count);

  Isolate* isolate() { return isolate_; }

  void Init(Address input_frame_pointer, TranslationIterator* iterator,
            FixedArray* literal_array, RegisterValues* registers,
            FILE* trace_file, int parameter_count);

 private:
  friend TranslatedValue;

  TranslatedFrame CreateNextTranslatedFrame(TranslationIterator* iterator,
                                            FixedArray* literal_array,
                                            Address fp,
                                            FILE* trace_file);
  int CreateNextTranslatedValue(int frame_index, TranslationIterator* iterator,
                                FixedArray* literal_array, Address fp,
                                RegisterValues* registers, FILE* trace_file);
  Address ComputeArgumentsPosition(Address input_frame_pointer,
                                   CreateArgumentsType type, int* length);
  void CreateArgumentsElementsTranslatedValues(int frame_index,
                                               Address input_frame_pointer,
                                               CreateArgumentsType type,
                                               FILE* trace_file);

  void UpdateFromPreviouslyMaterializedObjects();
  Handle<Object> MaterializeAt(int frame_index, int* value_index);
  Handle<Object> MaterializeObjectAt(int object_index);
  class CapturedObjectMaterializer;
  Handle<Object> MaterializeCapturedObjectAt(TranslatedValue* slot,
                                             int frame_index, int* value_index);

  static uint32_t GetUInt32Slot(Address fp, int slot_index);
  static Float32 GetFloatSlot(Address fp, int slot_index);
  static Float64 GetDoubleSlot(Address fp, int slot_index);

  std::vector<TranslatedFrame> frames_;
  Isolate* isolate_;
  Address stack_frame_pointer_;
  int formal_parameter_count_;

  struct ObjectPosition {
    int frame_index_;
    int value_index_;
  };
  std::deque<ObjectPosition> object_positions_;
};


class OptimizedFunctionVisitor BASE_EMBEDDED {
 public:
  virtual ~OptimizedFunctionVisitor() {}
  virtual void VisitFunction(JSFunction* function) = 0;
};

class Deoptimizer : public Malloced {
 public:
  enum BailoutType { EAGER, LAZY, SOFT, kLastBailoutType = SOFT };

  struct DeoptInfo {
    DeoptInfo(SourcePosition position, DeoptimizeReason deopt_reason,
              int deopt_id)
        : position(position), deopt_reason(deopt_reason), deopt_id(deopt_id) {}

    SourcePosition position;
    DeoptimizeReason deopt_reason;
    int deopt_id;

    static const int kNoDeoptId = -1;
  };

  static DeoptInfo GetDeoptInfo(Code* code, byte* from);

  static int ComputeSourcePositionFromBytecodeArray(SharedFunctionInfo* shared,
                                                    BailoutId node_id);

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

  static const char* MessageFor(BailoutType type);

  int output_count() const { return output_count_; }

  Handle<JSFunction> function() const;
  Handle<Code> compiled_code() const;
  BailoutType bailout_type() const { return bailout_type_; }
  bool preserve_optimized() const { return preserve_optimized_; }

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

  // Deoptimize the function now. Its current optimized code will never be run
  // again and any activations of the optimized code will get deoptimized when
  // execution returns. If {code} is specified then the given code is targeted
  // instead of the function code (e.g. OSR code not installed on function).
  static void DeoptimizeFunction(JSFunction* function, Code* code = nullptr);

  // Deoptimize all code in the given isolate.
  static void DeoptimizeAll(Isolate* isolate);

  // Deoptimizes all optimized code that has been previously marked
  // (via code->set_marked_for_deoptimization) and unlinks all functions that
  // refer to that code.
  static void DeoptimizeMarkedCode(Isolate* isolate);

  ~Deoptimizer();

  void MaterializeHeapObjects();

  static void ComputeOutputFrames(Deoptimizer* deoptimizer);

  static Address GetDeoptimizationEntry(Isolate* isolate, int id,
                                        BailoutType type);
  static int GetDeoptimizationId(Isolate* isolate,
                                 Address addr,
                                 BailoutType type);

  // Code generation support.
  static int input_offset() { return OFFSET_OF(Deoptimizer, input_); }
  static int output_count_offset() {
    return OFFSET_OF(Deoptimizer, output_count_);
  }
  static int output_offset() { return OFFSET_OF(Deoptimizer, output_); }

  static int caller_frame_top_offset() {
    return OFFSET_OF(Deoptimizer, caller_frame_top_);
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

  static void EnsureCodeForDeoptimizationEntry(Isolate* isolate,
                                               BailoutType type);
  static void EnsureCodeForMaxDeoptimizationEntries(Isolate* isolate);

  Isolate* isolate() const { return isolate_; }

 private:
  static const int kMinNumberOfEntries = 64;
  static const int kMaxNumberOfEntries = 16384;

  Deoptimizer(Isolate* isolate, JSFunction* function, BailoutType type,
              unsigned bailout_id, Address from, int fp_to_sp_delta);
  Code* FindOptimizedCode();
  void PrintFunctionName();
  void DeleteFrameDescriptions();

  void DoComputeOutputFrames();
  void DoComputeInterpretedFrame(TranslatedFrame* translated_frame,
                                 int frame_index, bool goto_catch_handler);
  void DoComputeArgumentsAdaptorFrame(TranslatedFrame* translated_frame,
                                      int frame_index);
  void DoComputeConstructStubFrame(TranslatedFrame* translated_frame,
                                   int frame_index);
  void DoComputeBuiltinContinuation(TranslatedFrame* translated_frame,
                                    int frame_index, bool java_script_frame);

  void WriteTranslatedValueToOutput(
      TranslatedFrame::iterator* iterator, int* input_index, int frame_index,
      unsigned output_offset, const char* debug_hint_string = nullptr,
      Address output_address_for_materialization = nullptr);
  void WriteValueToOutput(Object* value, int input_index, int frame_index,
                          unsigned output_offset,
                          const char* debug_hint_string);
  void DebugPrintOutputSlot(intptr_t value, int frame_index,
                            unsigned output_offset,
                            const char* debug_hint_string);

  unsigned ComputeInputFrameAboveFpFixedSize() const;
  unsigned ComputeInputFrameSize() const;
  static unsigned ComputeJavascriptFixedSize(SharedFunctionInfo* shared);
  static unsigned ComputeInterpretedFixedSize(SharedFunctionInfo* shared);

  static unsigned ComputeIncomingArgumentSize(SharedFunctionInfo* shared);
  static unsigned ComputeOutgoingArgumentSize(Code* code, unsigned bailout_id);

  static void GenerateDeoptimizationEntries(
      MacroAssembler* masm, int count, BailoutType type);

  // Marks all the code in the given context for deoptimization.
  static void MarkAllCodeForContext(Context* native_context);

  // Deoptimizes all code marked in the given context.
  static void DeoptimizeMarkedCodeForContext(Context* native_context);

  // Some architectures need to push padding together with the TOS register
  // in order to maintain stack alignment.
  static bool PadTopOfStackRegister();

  // Searches the list of known deoptimizing code for a Code object
  // containing the given address (which is supposedly faster than
  // searching all code objects).
  Code* FindDeoptimizingCode(Address addr);

  Isolate* isolate_;
  JSFunction* function_;
  Code* compiled_code_;
  unsigned bailout_id_;
  BailoutType bailout_type_;
  bool preserve_optimized_;
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

  Float32 GetFloatRegister(unsigned n) const {
    DCHECK(n < arraysize(float_registers_));
    return float_registers_[n];
  }

  Float64 GetDoubleRegister(unsigned n) const {
    DCHECK(n < arraysize(double_registers_));
    return double_registers_[n];
  }

  void SetRegister(unsigned n, intptr_t value) {
    DCHECK(n < arraysize(registers_));
    registers_[n] = value;
  }

  void SetFloatRegister(unsigned n, Float32 value) {
    DCHECK(n < arraysize(float_registers_));
    float_registers_[n] = value;
  }

  void SetDoubleRegister(unsigned n, Float64 value) {
    DCHECK(n < arraysize(double_registers_));
    double_registers_[n] = value;
  }

  // Generated code is writing directly into the below arrays, make sure their
  // element sizes fit what the machine instructions expect.
  static_assert(sizeof(Float32) == kFloatSize, "size mismatch");
  static_assert(sizeof(Float64) == kDoubleSize, "size mismatch");

  intptr_t registers_[Register::kNumRegisters];
  Float32 float_registers_[FloatRegister::kNumRegisters];
  Float64 double_registers_[DoubleRegister::kNumRegisters];
};


class FrameDescription {
 public:
  explicit FrameDescription(uint32_t frame_size, int parameter_count = 0);

  void* operator new(size_t size, uint32_t frame_size) {
    // Subtracts kPointerSize, as the member frame_content_ already supplies
    // the first element of the area to store the frame.
    return malloc(size + frame_size - kPointerSize);
  }

  void operator delete(void* pointer, uint32_t frame_size) {
    free(pointer);
  }

  void operator delete(void* description) {
    free(description);
  }

  uint32_t GetFrameSize() const {
    USE(frame_content_);
    DCHECK(static_cast<uint32_t>(frame_size_) == frame_size_);
    return static_cast<uint32_t>(frame_size_);
  }

  intptr_t GetFrameSlot(unsigned offset) {
    return *GetFrameSlotPointer(offset);
  }

  Address GetFramePointerAddress() {
    int fp_offset = GetFrameSize() - parameter_count() * kPointerSize -
                    StandardFrameConstants::kCallerSPOffset;
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

  void SetDoubleRegister(unsigned n, Float64 value) {
    register_values_.SetDoubleRegister(n, value);
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
    return OFFSET_OF(FrameDescription, register_values_.registers_);
  }

  static int double_registers_offset() {
    return OFFSET_OF(FrameDescription, register_values_.double_registers_);
  }

  static int float_registers_offset() {
    return OFFSET_OF(FrameDescription, register_values_.float_registers_);
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
    return reinterpret_cast<intptr_t*>(
        reinterpret_cast<Address>(this) + frame_content_offset() + offset);
  }
};


class DeoptimizerData {
 public:
  explicit DeoptimizerData(Heap* heap);
  ~DeoptimizerData();

 private:
  Heap* heap_;
  Code* deopt_entry_code_[Deoptimizer::kLastBailoutType + 1];

  Deoptimizer* current_;

  friend class Deoptimizer;

  DISALLOW_COPY_AND_ASSIGN(DeoptimizerData);
};


class TranslationBuffer BASE_EMBEDDED {
 public:
  explicit TranslationBuffer(Zone* zone) : contents_(zone) {}

  int CurrentIndex() const { return static_cast<int>(contents_.size()); }
  void Add(int32_t value);

  Handle<ByteArray> CreateByteArray(Factory* factory);

 private:
  ZoneChunkList<uint8_t> contents_;
};


class TranslationIterator BASE_EMBEDDED {
 public:
  TranslationIterator(ByteArray* buffer, int index);

  int32_t Next();

  bool HasNext() const;

  void Skip(int n) {
    for (int i = 0; i < n; i++) Next();
  }

 private:
  ByteArray* buffer_;
  int index_;
};

#define TRANSLATION_OPCODE_LIST(V)          \
  V(BEGIN)                                  \
  V(INTERPRETED_FRAME)                      \
  V(BUILTIN_CONTINUATION_FRAME)             \
  V(JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME) \
  V(CONSTRUCT_STUB_FRAME)                   \
  V(ARGUMENTS_ADAPTOR_FRAME)                \
  V(DUPLICATED_OBJECT)                      \
  V(ARGUMENTS_ELEMENTS)                     \
  V(ARGUMENTS_LENGTH)                       \
  V(CAPTURED_OBJECT)                        \
  V(REGISTER)                               \
  V(INT32_REGISTER)                         \
  V(UINT32_REGISTER)                        \
  V(BOOL_REGISTER)                          \
  V(FLOAT_REGISTER)                         \
  V(DOUBLE_REGISTER)                        \
  V(STACK_SLOT)                             \
  V(INT32_STACK_SLOT)                       \
  V(UINT32_STACK_SLOT)                      \
  V(BOOL_STACK_SLOT)                        \
  V(FLOAT_STACK_SLOT)                       \
  V(DOUBLE_STACK_SLOT)                      \
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
    buffer_->Add(BEGIN);
    buffer_->Add(frame_count);
    buffer_->Add(jsframe_count);
  }

  int index() const { return index_; }

  // Commands.
  void BeginInterpretedFrame(BailoutId bytecode_offset, int literal_id,
                             unsigned height);
  void BeginArgumentsAdaptorFrame(int literal_id, unsigned height);
  void BeginConstructStubFrame(BailoutId bailout_id, int literal_id,
                               unsigned height);
  void BeginBuiltinContinuationFrame(BailoutId bailout_id, int literal_id,
                                     unsigned height);
  void BeginJavaScriptBuiltinContinuationFrame(BailoutId bailout_id,
                                               int literal_id, unsigned height);
  void ArgumentsElements(CreateArgumentsType type);
  void ArgumentsLength(CreateArgumentsType type);
  void BeginCapturedObject(int length);
  void DuplicateObject(int object_index);
  void StoreRegister(Register reg);
  void StoreInt32Register(Register reg);
  void StoreUint32Register(Register reg);
  void StoreBoolRegister(Register reg);
  void StoreFloatRegister(FloatRegister reg);
  void StoreDoubleRegister(DoubleRegister reg);
  void StoreStackSlot(int index);
  void StoreInt32StackSlot(int index);
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
  explicit MaterializedObjectStore(Isolate* isolate) : isolate_(isolate) {
  }

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

  // Check if this frame is preceded by construct stub frame.  The bottom-most
  // inlined frame might still be called by an uninlined construct stub.
  bool HasConstructStub() {
    return has_construct_stub_;
  }

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

  int GetSourcePosition() {
    return source_position_;
  }

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
  bool has_construct_stub_;
  std::vector<Handle<Object> > parameters_;
  std::vector<Handle<Object> > expression_stack_;
  int source_position_;

  friend class Deoptimizer;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_H_
