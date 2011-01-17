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

#ifndef V8_DEOPTIMIZER_H_
#define V8_DEOPTIMIZER_H_

#include "v8.h"

#include "macro-assembler.h"
#include "zone-inl.h"


namespace v8 {
namespace internal {

class FrameDescription;
class TranslationIterator;
class DeoptimizingCodeListNode;


class ValueDescription BASE_EMBEDDED {
 public:
  explicit ValueDescription(int index) : stack_index_(index) { }
  int stack_index() const { return stack_index_; }

 private:
  // Offset relative to the top of the stack.
  int stack_index_;
};


class ValueDescriptionInteger32: public ValueDescription {
 public:
  ValueDescriptionInteger32(int index, int32_t value)
      : ValueDescription(index), int32_value_(value) { }
  int32_t int32_value() const { return int32_value_; }

 private:
  // Raw value.
  int32_t int32_value_;
};


class ValueDescriptionDouble: public ValueDescription {
 public:
  ValueDescriptionDouble(int index, double value)
      : ValueDescription(index), double_value_(value) { }
  double double_value() const { return double_value_; }

 private:
  // Raw value.
  double double_value_;
};


class OptimizedFunctionVisitor BASE_EMBEDDED {
 public:
  virtual ~OptimizedFunctionVisitor() {}

  // Function which is called before iteration of any optimized functions
  // from given global context.
  virtual void EnterContext(Context* context) = 0;

  virtual void VisitFunction(JSFunction* function) = 0;

  // Function which is called after iteration of all optimized functions
  // from given global context.
  virtual void LeaveContext(Context* context) = 0;
};


class Deoptimizer : public Malloced {
 public:
  enum BailoutType {
    EAGER,
    LAZY,
    OSR
  };

  int output_count() const { return output_count_; }

  static Deoptimizer* New(JSFunction* function,
                          BailoutType type,
                          unsigned bailout_id,
                          Address from,
                          int fp_to_sp_delta);
  static Deoptimizer* Grab();

  // Deoptimize the function now. Its current optimized code will never be run
  // again and any activations of the optimized code will get deoptimized when
  // execution returns.
  static void DeoptimizeFunction(JSFunction* function);

  // Deoptimize all functions in the heap.
  static void DeoptimizeAll();

  static void DeoptimizeGlobalObject(JSObject* object);

  static void VisitAllOptimizedFunctionsForContext(
      Context* context, OptimizedFunctionVisitor* visitor);

  static void VisitAllOptimizedFunctionsForGlobalObject(
      JSObject* object, OptimizedFunctionVisitor* visitor);

  static void VisitAllOptimizedFunctions(OptimizedFunctionVisitor* visitor);

  // Given the relocation info of a call to the stack check stub, patch the
  // code so as to go unconditionally to the on-stack replacement builtin
  // instead.
  static void PatchStackCheckCode(RelocInfo* rinfo, Code* replacement_code);

  // Given the relocation info of a call to the on-stack replacement
  // builtin, patch the code back to the original stack check code.
  static void RevertStackCheckCode(RelocInfo* rinfo, Code* check_code);

  ~Deoptimizer();

  void InsertHeapNumberValues(int index, JavaScriptFrame* frame);

  static void ComputeOutputFrames(Deoptimizer* deoptimizer);

  static Address GetDeoptimizationEntry(int id, BailoutType type);
  static int GetDeoptimizationId(Address addr, BailoutType type);
  static int GetOutputInfo(DeoptimizationOutputData* data,
                           unsigned node_id,
                           SharedFunctionInfo* shared);

  static void Setup();
  static void TearDown();

  // Code generation support.
  static int input_offset() { return OFFSET_OF(Deoptimizer, input_); }
  static int output_count_offset() {
    return OFFSET_OF(Deoptimizer, output_count_);
  }
  static int output_offset() { return OFFSET_OF(Deoptimizer, output_); }

  static int GetDeoptimizedCodeCount();

  static const int kNotDeoptimizationEntry = -1;

  // Generators for the deoptimization entry code.
  class EntryGenerator BASE_EMBEDDED {
   public:
    EntryGenerator(MacroAssembler* masm, BailoutType type)
        : masm_(masm), type_(type) { }
    virtual ~EntryGenerator() { }

    void Generate();

   protected:
    MacroAssembler* masm() const { return masm_; }
    BailoutType type() const { return type_; }

    virtual void GeneratePrologue() { }

   private:
    MacroAssembler* masm_;
    Deoptimizer::BailoutType type_;
  };

  class TableEntryGenerator : public EntryGenerator {
   public:
    TableEntryGenerator(MacroAssembler* masm, BailoutType type,  int count)
        : EntryGenerator(masm, type), count_(count) { }

   protected:
    virtual void GeneratePrologue();

   private:
    int count() const { return count_; }

    int count_;
  };

 private:
  static const int kNumberOfEntries = 4096;

  Deoptimizer(JSFunction* function,
              BailoutType type,
              unsigned bailout_id,
              Address from,
              int fp_to_sp_delta);
  void DeleteFrameDescriptions();

  void DoComputeOutputFrames();
  void DoComputeOsrOutputFrame();
  void DoComputeFrame(TranslationIterator* iterator, int frame_index);
  void DoTranslateCommand(TranslationIterator* iterator,
                          int frame_index,
                          unsigned output_offset);
  // Translate a command for OSR.  Updates the input offset to be used for
  // the next command.  Returns false if translation of the command failed
  // (e.g., a number conversion failed) and may or may not have updated the
  // input offset.
  bool DoOsrTranslateCommand(TranslationIterator* iterator,
                             int* input_offset);

  unsigned ComputeInputFrameSize() const;
  unsigned ComputeFixedSize(JSFunction* function) const;

  unsigned ComputeIncomingArgumentSize(JSFunction* function) const;
  unsigned ComputeOutgoingArgumentSize() const;

  Object* ComputeLiteral(int index) const;

  void InsertHeapNumberValue(JavaScriptFrame* frame,
                             int stack_index,
                             double val,
                             int extra_slot_count);

  void AddInteger32Value(int frame_index, int slot_index, int32_t value);
  void AddDoubleValue(int frame_index, int slot_index, double value);

  static LargeObjectChunk* CreateCode(BailoutType type);
  static void GenerateDeoptimizationEntries(
      MacroAssembler* masm, int count, BailoutType type);

  // Weak handle callback for deoptimizing code objects.
  static void HandleWeakDeoptimizedCode(
      v8::Persistent<v8::Value> obj, void* data);
  static Code* FindDeoptimizingCodeFromAddress(Address addr);
  static void RemoveDeoptimizingCode(Code* code);

  static LargeObjectChunk* eager_deoptimization_entry_code_;
  static LargeObjectChunk* lazy_deoptimization_entry_code_;
  static Deoptimizer* current_;

  // List of deoptimized code which still have references from active stack
  // frames. These code objects are needed by the deoptimizer when deoptimizing
  // a frame for which the code object for the function function has been
  // changed from the code present when deoptimizing was done.
  static DeoptimizingCodeListNode* deoptimizing_code_list_;

  JSFunction* function_;
  Code* optimized_code_;
  unsigned bailout_id_;
  BailoutType bailout_type_;
  Address from_;
  int fp_to_sp_delta_;

  // Input frame description.
  FrameDescription* input_;
  // Number of output frames.
  int output_count_;
  // Array of output frame descriptions.
  FrameDescription** output_;

  List<ValueDescriptionInteger32>* integer32_values_;
  List<ValueDescriptionDouble>* double_values_;

  static int table_entry_size_;

  friend class FrameDescription;
  friend class DeoptimizingCodeListNode;
};


class FrameDescription {
 public:
  FrameDescription(uint32_t frame_size,
                   JSFunction* function);

  void* operator new(size_t size, uint32_t frame_size) {
    return malloc(size + frame_size);
  }

  void operator delete(void* description) {
    free(description);
  }

  intptr_t GetFrameSize() const { return frame_size_; }

  JSFunction* GetFunction() const { return function_; }

  unsigned GetOffsetFromSlotIndex(Deoptimizer* deoptimizer, int slot_index);

  intptr_t GetFrameSlot(unsigned offset) {
    return *GetFrameSlotPointer(offset);
  }

  double GetDoubleFrameSlot(unsigned offset) {
    return *reinterpret_cast<double*>(GetFrameSlotPointer(offset));
  }

  void SetFrameSlot(unsigned offset, intptr_t value) {
    *GetFrameSlotPointer(offset) = value;
  }

  intptr_t GetRegister(unsigned n) const {
    ASSERT(n < ARRAY_SIZE(registers_));
    return registers_[n];
  }

  double GetDoubleRegister(unsigned n) const {
    ASSERT(n < ARRAY_SIZE(double_registers_));
    return double_registers_[n];
  }

  void SetRegister(unsigned n, intptr_t value) {
    ASSERT(n < ARRAY_SIZE(registers_));
    registers_[n] = value;
  }

  void SetDoubleRegister(unsigned n, double value) {
    ASSERT(n < ARRAY_SIZE(double_registers_));
    double_registers_[n] = value;
  }

  intptr_t GetTop() const { return top_; }
  void SetTop(intptr_t top) { top_ = top; }

  intptr_t GetPc() const { return pc_; }
  void SetPc(intptr_t pc) { pc_ = pc; }

  intptr_t GetFp() const { return fp_; }
  void SetFp(intptr_t fp) { fp_ = fp; }

  Smi* GetState() const { return state_; }
  void SetState(Smi* state) { state_ = state; }

  void SetContinuation(intptr_t pc) { continuation_ = pc; }

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
    return sizeof(FrameDescription);
  }

 private:
  static const uint32_t kZapUint32 = 0xbeeddead;

  uintptr_t frame_size_;  // Number of bytes.
  JSFunction* function_;
  intptr_t registers_[Register::kNumRegisters];
  double double_registers_[DoubleRegister::kNumAllocatableRegisters];
  intptr_t top_;
  intptr_t pc_;
  intptr_t fp_;
  Smi* state_;

  // Continuation is the PC where the execution continues after
  // deoptimizing.
  intptr_t continuation_;

  intptr_t* GetFrameSlotPointer(unsigned offset) {
    ASSERT(offset < frame_size_);
    return reinterpret_cast<intptr_t*>(
        reinterpret_cast<Address>(this) + frame_content_offset() + offset);
  }
};


class TranslationBuffer BASE_EMBEDDED {
 public:
  TranslationBuffer() : contents_(256) { }

  int CurrentIndex() const { return contents_.length(); }
  void Add(int32_t value);

  Handle<ByteArray> CreateByteArray();

 private:
  ZoneList<uint8_t> contents_;
};


class TranslationIterator BASE_EMBEDDED {
 public:
  TranslationIterator(ByteArray* buffer, int index)
      : buffer_(buffer), index_(index) {
    ASSERT(index >= 0 && index < buffer->length());
  }

  int32_t Next();

  bool HasNext() const { return index_ >= 0; }

  void Done() { index_ = -1; }

  void Skip(int n) {
    for (int i = 0; i < n; i++) Next();
  }

 private:
  ByteArray* buffer_;
  int index_;
};


class Translation BASE_EMBEDDED {
 public:
  enum Opcode {
    BEGIN,
    FRAME,
    REGISTER,
    INT32_REGISTER,
    DOUBLE_REGISTER,
    STACK_SLOT,
    INT32_STACK_SLOT,
    DOUBLE_STACK_SLOT,
    LITERAL,
    ARGUMENTS_OBJECT,

    // A prefix indicating that the next command is a duplicate of the one
    // that follows it.
    DUPLICATE
  };

  Translation(TranslationBuffer* buffer, int frame_count)
      : buffer_(buffer),
        index_(buffer->CurrentIndex()) {
    buffer_->Add(BEGIN);
    buffer_->Add(frame_count);
  }

  int index() const { return index_; }

  // Commands.
  void BeginFrame(int node_id, int literal_id, unsigned height);
  void StoreRegister(Register reg);
  void StoreInt32Register(Register reg);
  void StoreDoubleRegister(DoubleRegister reg);
  void StoreStackSlot(int index);
  void StoreInt32StackSlot(int index);
  void StoreDoubleStackSlot(int index);
  void StoreLiteral(int literal_id);
  void StoreArgumentsObject();
  void MarkDuplicate();

  static int NumberOfOperandsFor(Opcode opcode);

#ifdef OBJECT_PRINT
  static const char* StringFor(Opcode opcode);
#endif

 private:
  TranslationBuffer* buffer_;
  int index_;
};


// Linked list holding deoptimizing code objects. The deoptimizing code objects
// are kept as weak handles until they are no longer activated on the stack.
class DeoptimizingCodeListNode : public Malloced {
 public:
  explicit DeoptimizingCodeListNode(Code* code);
  ~DeoptimizingCodeListNode();

  DeoptimizingCodeListNode* next() const { return next_; }
  void set_next(DeoptimizingCodeListNode* next) { next_ = next; }
  Handle<Code> code() const { return code_; }

 private:
  // Global (weak) handle to the deoptimizing code object.
  Handle<Code> code_;

  // Next pointer for linked list.
  DeoptimizingCodeListNode* next_;
};


} }  // namespace v8::internal

#endif  // V8_DEOPTIMIZER_H_
