// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer.h"

#include <memory>

#include "src/accessors.h"
#include "src/assembler-inl.h"
#include "src/ast/prettyprinter.h"
#include "src/callable.h"
#include "src/disasm.h"
#include "src/frames-inl.h"
#include "src/global-handles.h"
#include "src/interpreter/interpreter.h"
#include "src/macro-assembler.h"
#include "src/objects/debug-objects-inl.h"
#include "src/tracing/trace-event.h"
#include "src/v8.h"


namespace v8 {
namespace internal {

DeoptimizerData::DeoptimizerData(Heap* heap) : heap_(heap), current_(nullptr) {
  for (int i = 0; i <= Deoptimizer::kLastBailoutType; ++i) {
    deopt_entry_code_[i] = nullptr;
  }
  Code** start = &deopt_entry_code_[0];
  Code** end = &deopt_entry_code_[Deoptimizer::kLastBailoutType + 1];
  heap_->RegisterStrongRoots(reinterpret_cast<Object**>(start),
                             reinterpret_cast<Object**>(end));
}


DeoptimizerData::~DeoptimizerData() {
  for (int i = 0; i <= Deoptimizer::kLastBailoutType; ++i) {
    deopt_entry_code_[i] = nullptr;
  }
  Code** start = &deopt_entry_code_[0];
  heap_->UnregisterStrongRoots(reinterpret_cast<Object**>(start));
}


Code* Deoptimizer::FindDeoptimizingCode(Address addr) {
  if (function_->IsHeapObject()) {
    // Search all deoptimizing code in the native context of the function.
    Isolate* isolate = function_->GetIsolate();
    Context* native_context = function_->context()->native_context();
    Object* element = native_context->DeoptimizedCodeListHead();
    while (!element->IsUndefined(isolate)) {
      Code* code = Code::cast(element);
      CHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
      if (code->contains(addr)) return code;
      element = code->next_code_link();
    }
  }
  return nullptr;
}


// We rely on this function not causing a GC.  It is called from generated code
// without having a real stack frame in place.
Deoptimizer* Deoptimizer::New(JSFunction* function,
                              BailoutType type,
                              unsigned bailout_id,
                              Address from,
                              int fp_to_sp_delta,
                              Isolate* isolate) {
  Deoptimizer* deoptimizer = new Deoptimizer(isolate, function, type,
                                             bailout_id, from, fp_to_sp_delta);
  CHECK_NULL(isolate->deoptimizer_data()->current_);
  isolate->deoptimizer_data()->current_ = deoptimizer;
  return deoptimizer;
}


Deoptimizer* Deoptimizer::Grab(Isolate* isolate) {
  Deoptimizer* result = isolate->deoptimizer_data()->current_;
  CHECK_NOT_NULL(result);
  result->DeleteFrameDescriptions();
  isolate->deoptimizer_data()->current_ = nullptr;
  return result;
}

DeoptimizedFrameInfo* Deoptimizer::DebuggerInspectableFrame(
    JavaScriptFrame* frame,
    int jsframe_index,
    Isolate* isolate) {
  CHECK(frame->is_optimized());

  TranslatedState translated_values(frame);
  translated_values.Prepare(frame->fp());

  TranslatedState::iterator frame_it = translated_values.end();
  int counter = jsframe_index;
  for (auto it = translated_values.begin(); it != translated_values.end();
       it++) {
    if (it->kind() == TranslatedFrame::kInterpretedFunction ||
        it->kind() == TranslatedFrame::kJavaScriptBuiltinContinuation) {
      if (counter == 0) {
        frame_it = it;
        break;
      }
      counter--;
    }
  }
  CHECK(frame_it != translated_values.end());
  // We only include kJavaScriptBuiltinContinuation frames above to get the
  // counting right.
  CHECK_EQ(frame_it->kind(), TranslatedFrame::kInterpretedFunction);

  DeoptimizedFrameInfo* info =
      new DeoptimizedFrameInfo(&translated_values, frame_it, isolate);

  return info;
}

void Deoptimizer::GenerateDeoptimizationEntries(MacroAssembler* masm,
                                                int count,
                                                BailoutType type) {
  TableEntryGenerator generator(masm, type, count);
  generator.Generate();
}

namespace {
class ActivationsFinder : public ThreadVisitor {
 public:
  explicit ActivationsFinder(std::set<Code*>* codes,
                             Code* topmost_optimized_code,
                             bool safe_to_deopt_topmost_optimized_code)
      : codes_(codes) {
#ifdef DEBUG
    topmost_ = topmost_optimized_code;
    safe_to_deopt_ = safe_to_deopt_topmost_optimized_code;
#endif
  }

  // Find the frames with activations of codes marked for deoptimization, search
  // for the trampoline to the deoptimizer call respective to each code, and use
  // it to replace the current pc on the stack.
  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    for (StackFrameIterator it(isolate, top); !it.done(); it.Advance()) {
      if (it.frame()->type() == StackFrame::OPTIMIZED) {
        Code* code = it.frame()->LookupCode();
        if (code->kind() == Code::OPTIMIZED_FUNCTION &&
            code->marked_for_deoptimization()) {
          codes_->erase(code);
          // Obtain the trampoline to the deoptimizer call.
          SafepointEntry safepoint = code->GetSafepointEntry(it.frame()->pc());
          int trampoline_pc = safepoint.trampoline_pc();
          DCHECK_IMPLIES(code == topmost_, safe_to_deopt_);
          // Replace the current pc on the stack with the trampoline.
          it.frame()->set_pc(code->instruction_start() + trampoline_pc);
        }
      }
    }
  }

 private:
  std::set<Code*>* codes_;

#ifdef DEBUG
  Code* topmost_;
  bool safe_to_deopt_;
#endif
};
}  // namespace

// Move marked code from the optimized code list to the deoptimized code list,
// and replace pc on the stack for codes marked for deoptimization.
void Deoptimizer::DeoptimizeMarkedCodeForContext(Context* context) {
  DisallowHeapAllocation no_allocation;

  Isolate* isolate = context->GetHeap()->isolate();
  Code* topmost_optimized_code = nullptr;
  bool safe_to_deopt_topmost_optimized_code = false;
#ifdef DEBUG
  // Make sure all activations of optimized code can deopt at their current PC.
  // The topmost optimized code has special handling because it cannot be
  // deoptimized due to weak object dependency.
  for (StackFrameIterator it(isolate, isolate->thread_local_top());
       !it.done(); it.Advance()) {
    StackFrame::Type type = it.frame()->type();
    if (type == StackFrame::OPTIMIZED) {
      Code* code = it.frame()->LookupCode();
      JSFunction* function =
          static_cast<OptimizedFrame*>(it.frame())->function();
      if (FLAG_trace_deopt) {
        CodeTracer::Scope scope(isolate->GetCodeTracer());
        PrintF(scope.file(), "[deoptimizer found activation of function: ");
        function->PrintName(scope.file());
        PrintF(scope.file(),
               " / %" V8PRIxPTR "]\n", reinterpret_cast<intptr_t>(function));
      }
      SafepointEntry safepoint = code->GetSafepointEntry(it.frame()->pc());
      int deopt_index = safepoint.deoptimization_index();

      // Turbofan deopt is checked when we are patching addresses on stack.
      bool safe_if_deopt_triggered =
          deopt_index != Safepoint::kNoDeoptimizationIndex;
      bool is_builtin_code = code->kind() == Code::BUILTIN;
      DCHECK(topmost_optimized_code == nullptr || safe_if_deopt_triggered ||
             is_builtin_code);
      if (topmost_optimized_code == nullptr) {
        topmost_optimized_code = code;
        safe_to_deopt_topmost_optimized_code = safe_if_deopt_triggered;
      }
    }
  }
#endif

  // We will use this set to mark those Code objects that are marked for
  // deoptimization and have not been found in stack frames.
  std::set<Code*> codes;

  // Move marked code from the optimized code list to the deoptimized code list.
  // Walk over all optimized code objects in this native context.
  Code* prev = nullptr;
  Object* element = context->OptimizedCodeListHead();
  while (!element->IsUndefined(isolate)) {
    Code* code = Code::cast(element);
    CHECK_EQ(code->kind(), Code::OPTIMIZED_FUNCTION);
    Object* next = code->next_code_link();

    if (code->marked_for_deoptimization()) {
      // Make sure that this object does not point to any garbage.
      isolate->heap()->InvalidateCodeEmbeddedObjects(code);
      codes.insert(code);

      if (prev != nullptr) {
        // Skip this code in the optimized code list.
        prev->set_next_code_link(next);
      } else {
        // There was no previous node, the next node is the new head.
        context->SetOptimizedCodeListHead(next);
      }

      // Move the code to the _deoptimized_ code list.
      code->set_next_code_link(context->DeoptimizedCodeListHead());
      context->SetDeoptimizedCodeListHead(code);
    } else {
      // Not marked; preserve this element.
      prev = code;
    }
    element = next;
  }

  ActivationsFinder visitor(&codes, topmost_optimized_code,
                            safe_to_deopt_topmost_optimized_code);
  // Iterate over the stack of this thread.
  visitor.VisitThread(isolate, isolate->thread_local_top());
  // In addition to iterate over the stack of this thread, we also
  // need to consider all the other threads as they may also use
  // the code currently beings deoptimized.
  isolate->thread_manager()->IterateArchivedThreads(&visitor);

  // If there's no activation of a code in any stack then we can remove its
  // deoptimization data. We do this to ensure that code objects that are
  // unlinked don't transitively keep objects alive unnecessarily.
  for (Code* code : codes) {
    isolate->heap()->InvalidateCodeDeoptimizationData(code);
  }
}


void Deoptimizer::DeoptimizeAll(Isolate* isolate) {
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::DeoptimizeCode);
  TimerEventScope<TimerEventDeoptimizeCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.DeoptimizeCode");
  if (FLAG_trace_deopt) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[deoptimize all code in all contexts]\n");
  }
  DisallowHeapAllocation no_allocation;
  // For all contexts, mark all code, then deoptimize.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined(isolate)) {
    Context* native_context = Context::cast(context);
    MarkAllCodeForContext(native_context);
    DeoptimizeMarkedCodeForContext(native_context);
    context = native_context->next_context_link();
  }
}


void Deoptimizer::DeoptimizeMarkedCode(Isolate* isolate) {
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::DeoptimizeCode);
  TimerEventScope<TimerEventDeoptimizeCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.DeoptimizeCode");
  if (FLAG_trace_deopt) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[deoptimize marked code in all contexts]\n");
  }
  DisallowHeapAllocation no_allocation;
  // For all contexts, deoptimize code already marked.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined(isolate)) {
    Context* native_context = Context::cast(context);
    DeoptimizeMarkedCodeForContext(native_context);
    context = native_context->next_context_link();
  }
}

void Deoptimizer::MarkAllCodeForContext(Context* context) {
  Object* element = context->OptimizedCodeListHead();
  Isolate* isolate = context->GetIsolate();
  while (!element->IsUndefined(isolate)) {
    Code* code = Code::cast(element);
    CHECK_EQ(code->kind(), Code::OPTIMIZED_FUNCTION);
    code->set_marked_for_deoptimization(true);
    element = code->next_code_link();
  }
}

void Deoptimizer::DeoptimizeFunction(JSFunction* function, Code* code) {
  Isolate* isolate = function->GetIsolate();
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::DeoptimizeCode);
  TimerEventScope<TimerEventDeoptimizeCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.DeoptimizeCode");
  if (code == nullptr) code = function->code();

  if (code->kind() == Code::OPTIMIZED_FUNCTION) {
    // Mark the code for deoptimization and unlink any functions that also
    // refer to that code. The code cannot be shared across native contexts,
    // so we only need to search one.
    code->set_marked_for_deoptimization(true);
    // The code in the function's optimized code feedback vector slot might
    // be different from the code on the function - evict it if necessary.
    function->feedback_vector()->EvictOptimizedCodeMarkedForDeoptimization(
        function->shared(), "unlinking code marked for deopt");
    if (!code->deopt_already_counted()) {
      function->feedback_vector()->increment_deopt_count();
      code->set_deopt_already_counted(true);
    }
    DeoptimizeMarkedCodeForContext(function->context()->native_context());
  }
}


void Deoptimizer::ComputeOutputFrames(Deoptimizer* deoptimizer) {
  deoptimizer->DoComputeOutputFrames();
}


const char* Deoptimizer::MessageFor(BailoutType type) {
  switch (type) {
    case EAGER: return "eager";
    case SOFT: return "soft";
    case LAZY: return "lazy";
  }
  FATAL("Unsupported deopt type");
  return nullptr;
}

namespace {

CodeEventListener::DeoptKind DeoptKindOfBailoutType(
    Deoptimizer::BailoutType bailout_type) {
  switch (bailout_type) {
    case Deoptimizer::EAGER:
      return CodeEventListener::kEager;
    case Deoptimizer::SOFT:
      return CodeEventListener::kSoft;
    case Deoptimizer::LAZY:
      return CodeEventListener::kLazy;
  }
  UNREACHABLE();
}

}  // namespace

Deoptimizer::Deoptimizer(Isolate* isolate, JSFunction* function,
                         BailoutType type, unsigned bailout_id, Address from,
                         int fp_to_sp_delta)
    : isolate_(isolate),
      function_(function),
      bailout_id_(bailout_id),
      bailout_type_(type),
      preserve_optimized_(false),
      from_(from),
      fp_to_sp_delta_(fp_to_sp_delta),
      deoptimizing_throw_(false),
      catch_handler_data_(-1),
      catch_handler_pc_offset_(-1),
      input_(nullptr),
      output_count_(0),
      jsframe_count_(0),
      output_(nullptr),
      caller_frame_top_(0),
      caller_fp_(0),
      caller_pc_(0),
      caller_constant_pool_(0),
      input_frame_context_(0),
      stack_fp_(0),
      trace_scope_(nullptr) {
  if (isolate->deoptimizer_lazy_throw()) {
    isolate->set_deoptimizer_lazy_throw(false);
    deoptimizing_throw_ = true;
  }

  DCHECK_NOT_NULL(from);
  compiled_code_ = FindOptimizedCode();
  DCHECK_NOT_NULL(compiled_code_);

  DCHECK(function->IsJSFunction());
  trace_scope_ = FLAG_trace_deopt
                     ? new CodeTracer::Scope(isolate->GetCodeTracer())
                     : nullptr;
#ifdef DEBUG
  DCHECK(AllowHeapAllocation::IsAllowed());
  disallow_heap_allocation_ = new DisallowHeapAllocation();
#endif  // DEBUG
  if (compiled_code_->kind() != Code::OPTIMIZED_FUNCTION ||
      !compiled_code_->deopt_already_counted()) {
    // If the function is optimized, and we haven't counted that deopt yet, then
    // increment the function's deopt count so that we can avoid optimising
    // functions that deopt too often.

    if (bailout_type_ == Deoptimizer::SOFT) {
      // Soft deopts shouldn't count against the overall deoptimization count
      // that can eventually lead to disabling optimization for a function.
      isolate->counters()->soft_deopts_executed()->Increment();
    } else if (function != nullptr) {
      function->feedback_vector()->increment_deopt_count();
    }
  }
  if (compiled_code_->kind() == Code::OPTIMIZED_FUNCTION) {
    compiled_code_->set_deopt_already_counted(true);
    PROFILE(isolate_,
            CodeDeoptEvent(compiled_code_, DeoptKindOfBailoutType(type), from_,
                           fp_to_sp_delta_));
  }
  unsigned size = ComputeInputFrameSize();
  int parameter_count =
      function->shared()->internal_formal_parameter_count() + 1;
  input_ = new (size) FrameDescription(size, parameter_count);
}

Code* Deoptimizer::FindOptimizedCode() {
  Code* compiled_code = FindDeoptimizingCode(from_);
  return (compiled_code == nullptr)
             ? static_cast<Code*>(isolate_->FindCodeObject(from_))
             : compiled_code;
}


void Deoptimizer::PrintFunctionName() {
  if (function_->IsHeapObject() && function_->IsJSFunction()) {
    function_->ShortPrint(trace_scope_->file());
  } else {
    PrintF(trace_scope_->file(),
           "%s", Code::Kind2String(compiled_code_->kind()));
  }
}

Handle<JSFunction> Deoptimizer::function() const {
  return Handle<JSFunction>(function_);
}
Handle<Code> Deoptimizer::compiled_code() const {
  return Handle<Code>(compiled_code_);
}

Deoptimizer::~Deoptimizer() {
  DCHECK(input_ == nullptr && output_ == nullptr);
  DCHECK_NULL(disallow_heap_allocation_);
  delete trace_scope_;
}


void Deoptimizer::DeleteFrameDescriptions() {
  delete input_;
  for (int i = 0; i < output_count_; ++i) {
    if (output_[i] != input_) delete output_[i];
  }
  delete[] output_;
  input_ = nullptr;
  output_ = nullptr;
#ifdef DEBUG
  DCHECK(!AllowHeapAllocation::IsAllowed());
  DCHECK_NOT_NULL(disallow_heap_allocation_);
  delete disallow_heap_allocation_;
  disallow_heap_allocation_ = nullptr;
#endif  // DEBUG
}

Address Deoptimizer::GetDeoptimizationEntry(Isolate* isolate, int id,
                                            BailoutType type) {
  CHECK_GE(id, 0);
  if (id >= kMaxNumberOfEntries) return nullptr;
  DeoptimizerData* data = isolate->deoptimizer_data();
  CHECK_LE(type, kLastBailoutType);
  CHECK_NOT_NULL(data->deopt_entry_code_[type]);
  Code* code = data->deopt_entry_code_[type];
  return code->instruction_start() + (id * table_entry_size_);
}


int Deoptimizer::GetDeoptimizationId(Isolate* isolate,
                                     Address addr,
                                     BailoutType type) {
  DeoptimizerData* data = isolate->deoptimizer_data();
  CHECK_LE(type, kLastBailoutType);
  Code* code = data->deopt_entry_code_[type];
  if (code == nullptr) return kNotDeoptimizationEntry;
  Address start = code->instruction_start();
  if (addr < start ||
      addr >= start + (kMaxNumberOfEntries * table_entry_size_)) {
    return kNotDeoptimizationEntry;
  }
  DCHECK_EQ(0,
            static_cast<int>(addr - start) % table_entry_size_);
  return static_cast<int>(addr - start) / table_entry_size_;
}


int Deoptimizer::GetDeoptimizedCodeCount(Isolate* isolate) {
  int length = 0;
  // Count all entries in the deoptimizing code list of every context.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined(isolate)) {
    Context* native_context = Context::cast(context);
    Object* element = native_context->DeoptimizedCodeListHead();
    while (!element->IsUndefined(isolate)) {
      Code* code = Code::cast(element);
      DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
      if (!code->marked_for_deoptimization()) {
        length++;
      }
      element = code->next_code_link();
    }
    context = Context::cast(context)->next_context_link();
  }
  return length;
}

namespace {

int LookupCatchHandler(TranslatedFrame* translated_frame, int* data_out) {
  switch (translated_frame->kind()) {
    case TranslatedFrame::kInterpretedFunction: {
      int bytecode_offset = translated_frame->node_id().ToInt();
      BytecodeArray* bytecode =
          translated_frame->raw_shared_info()->bytecode_array();
      HandlerTable* table = HandlerTable::cast(bytecode->handler_table());
      return table->LookupRange(bytecode_offset, data_out, nullptr);
    }
    default:
      break;
  }
  return -1;
}

}  // namespace

// We rely on this function not causing a GC.  It is called from generated code
// without having a real stack frame in place.
void Deoptimizer::DoComputeOutputFrames() {
  base::ElapsedTimer timer;

  // Determine basic deoptimization information.  The optimized frame is
  // described by the input data.
  DeoptimizationData* input_data =
      DeoptimizationData::cast(compiled_code_->deoptimization_data());

  {
    // Read caller's PC, caller's FP and caller's constant pool values
    // from input frame. Compute caller's frame top address.

    Register fp_reg = JavaScriptFrame::fp_register();
    stack_fp_ = input_->GetRegister(fp_reg.code());

    caller_frame_top_ = stack_fp_ + ComputeInputFrameAboveFpFixedSize();

    Address fp_address = input_->GetFramePointerAddress();
    caller_fp_ = Memory::intptr_at(fp_address);
    caller_pc_ =
        Memory::intptr_at(fp_address + CommonFrameConstants::kCallerPCOffset);
    input_frame_context_ = Memory::intptr_at(
        fp_address + CommonFrameConstants::kContextOrFrameTypeOffset);

    if (FLAG_enable_embedded_constant_pool) {
      caller_constant_pool_ = Memory::intptr_at(
          fp_address + CommonFrameConstants::kConstantPoolOffset);
    }
  }

  if (trace_scope_ != nullptr) {
    timer.Start();
    PrintF(trace_scope_->file(), "[deoptimizing (DEOPT %s): begin ",
           MessageFor(bailout_type_));
    PrintFunctionName();
    PrintF(trace_scope_->file(),
           " (opt #%d) @%d, FP to SP delta: %d, caller sp: 0x%08" V8PRIxPTR
           "]\n",
           input_data->OptimizationId()->value(), bailout_id_, fp_to_sp_delta_,
           caller_frame_top_);
    if (bailout_type_ == EAGER || bailout_type_ == SOFT) {
      compiled_code_->PrintDeoptLocation(trace_scope_->file(), from_);
    }
  }

  BailoutId node_id = input_data->BytecodeOffset(bailout_id_);
  ByteArray* translations = input_data->TranslationByteArray();
  unsigned translation_index =
      input_data->TranslationIndex(bailout_id_)->value();

  TranslationIterator state_iterator(translations, translation_index);
  translated_state_.Init(
      input_->GetFramePointerAddress(), &state_iterator,
      input_data->LiteralArray(), input_->GetRegisterValues(),
      trace_scope_ == nullptr ? nullptr : trace_scope_->file(),
      function_->IsHeapObject()
          ? function_->shared()->internal_formal_parameter_count()
          : 0);

  // Do the input frame to output frame(s) translation.
  size_t count = translated_state_.frames().size();
  // If we are supposed to go to the catch handler, find the catching frame
  // for the catch and make sure we only deoptimize upto that frame.
  if (deoptimizing_throw_) {
    size_t catch_handler_frame_index = count;
    for (size_t i = count; i-- > 0;) {
      catch_handler_pc_offset_ = LookupCatchHandler(
          &(translated_state_.frames()[i]), &catch_handler_data_);
      if (catch_handler_pc_offset_ >= 0) {
        catch_handler_frame_index = i;
        break;
      }
    }
    CHECK_LT(catch_handler_frame_index, count);
    count = catch_handler_frame_index + 1;
  }

  DCHECK_NULL(output_);
  output_ = new FrameDescription*[count];
  for (size_t i = 0; i < count; ++i) {
    output_[i] = nullptr;
  }
  output_count_ = static_cast<int>(count);

  // Translate each output frame.
  int frame_index = 0;  // output_frame_index
  for (size_t i = 0; i < count; ++i, ++frame_index) {
    // Read the ast node id, function, and frame height for this output frame.
    TranslatedFrame* translated_frame = &(translated_state_.frames()[i]);
    switch (translated_frame->kind()) {
      case TranslatedFrame::kInterpretedFunction:
        DoComputeInterpretedFrame(translated_frame, frame_index,
                                  deoptimizing_throw_ && i == count - 1);
        jsframe_count_++;
        break;
      case TranslatedFrame::kArgumentsAdaptor:
        DoComputeArgumentsAdaptorFrame(translated_frame, frame_index);
        break;
      case TranslatedFrame::kConstructStub:
        DoComputeConstructStubFrame(translated_frame, frame_index);
        break;
      case TranslatedFrame::kBuiltinContinuation:
        DoComputeBuiltinContinuation(translated_frame, frame_index, false);
        break;
      case TranslatedFrame::kJavaScriptBuiltinContinuation:
        DoComputeBuiltinContinuation(translated_frame, frame_index, true);
        break;
      case TranslatedFrame::kInvalid:
        FATAL("invalid frame");
        break;
    }
  }

  // Print some helpful diagnostic information.
  if (trace_scope_ != nullptr) {
    double ms = timer.Elapsed().InMillisecondsF();
    int index = output_count_ - 1;  // Index of the topmost frame.
    PrintF(trace_scope_->file(), "[deoptimizing (%s): end ",
           MessageFor(bailout_type_));
    PrintFunctionName();
    PrintF(trace_scope_->file(),
           " @%d => node=%d, pc=0x%08" V8PRIxPTR ", caller sp=0x%08" V8PRIxPTR
           ", took %0.3f ms]\n",
           bailout_id_, node_id.ToInt(), output_[index]->GetPc(),
           caller_frame_top_, ms);
  }
}

void Deoptimizer::DoComputeInterpretedFrame(TranslatedFrame* translated_frame,
                                            int frame_index,
                                            bool goto_catch_handler) {
  SharedFunctionInfo* shared = translated_frame->raw_shared_info();

  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  bool is_bottommost = (0 == frame_index);
  bool is_topmost = (output_count_ - 1 == frame_index);
  int input_index = 0;

  int bytecode_offset = translated_frame->node_id().ToInt();
  int height = translated_frame->height();
  int register_count = height - 1;  // Exclude accumulator.
  int register_stack_slot_count =
      InterpreterFrameConstants::RegisterStackSlotCount(register_count);
  int height_in_bytes = register_stack_slot_count * kPointerSize;

  // The topmost frame will contain the accumulator.
  if (is_topmost) {
    height_in_bytes += kPointerSize;
    if (PadTopOfStackRegister()) height_in_bytes += kPointerSize;
  }

  TranslatedFrame::iterator function_iterator = value_iterator;
  Object* function = value_iterator->GetRawValue();
  value_iterator++;
  input_index++;
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(), "  translating interpreted frame ");
    std::unique_ptr<char[]> name = shared->DebugName()->ToCString();
    PrintF(trace_scope_->file(), "%s", name.get());
    PrintF(trace_scope_->file(), " => bytecode_offset=%d, height=%d%s\n",
           bytecode_offset, height_in_bytes,
           goto_catch_handler ? " (throw)" : "");
  }
  if (goto_catch_handler) {
    bytecode_offset = catch_handler_pc_offset_;
  }

  // The 'fixed' part of the frame consists of the incoming parameters and
  // the part described by InterpreterFrameConstants.
  unsigned fixed_frame_size = ComputeInterpretedFixedSize(shared);
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  int parameter_count = shared->internal_formal_parameter_count() + 1;
  FrameDescription* output_frame = new (output_frame_size)
      FrameDescription(output_frame_size, parameter_count);

  CHECK(frame_index >= 0 && frame_index < output_count_);
  CHECK_NULL(output_[frame_index]);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous frame's top and
  // this frame's size.
  intptr_t top_address;
  if (is_bottommost) {
    top_address = caller_frame_top_ - output_frame_size;
  } else {
    top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  }
  output_frame->SetTop(top_address);

  // Compute the incoming parameter translation.
  unsigned output_offset = output_frame_size;
  for (int i = 0; i < parameter_count; ++i) {
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
  }

  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(), "    -------------------------\n");
  }

  // There are no translation commands for the caller's pc and fp, the
  // context, the function and the bytecode offset.  Synthesize
  // their values and set them up
  // explicitly.
  //
  // The caller's pc for the bottommost output frame is the same as in the
  // input frame.  For all subsequent output frames, it can be read from the
  // previous one.  This frame's pc can be computed from the non-optimized
  // function code and AST id of the bailout.
  output_offset -= kPCOnStackSize;
  intptr_t value;
  if (is_bottommost) {
    value = caller_pc_;
  } else {
    value = output_[frame_index - 1]->GetPc();
  }
  output_frame->SetCallerPc(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's pc\n");

  // The caller's frame pointer for the bottommost output frame is the same
  // as in the input frame.  For all subsequent output frames, it can be
  // read from the previous one.  Also compute and set this frame's frame
  // pointer.
  output_offset -= kFPOnStackSize;
  if (is_bottommost) {
    value = caller_fp_;
  } else {
    value = output_[frame_index - 1]->GetFp();
  }
  output_frame->SetCallerFp(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  if (is_topmost) {
    Register fp_reg = InterpretedFrame::fp_register();
    output_frame->SetRegister(fp_reg.code(), fp_value);
  }
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's fp\n");

  if (FLAG_enable_embedded_constant_pool) {
    // For the bottommost output frame the constant pool pointer can be gotten
    // from the input frame. For subsequent output frames, it can be read from
    // the previous frame.
    output_offset -= kPointerSize;
    if (is_bottommost) {
      value = caller_constant_pool_;
    } else {
      value = output_[frame_index - 1]->GetConstantPool();
    }
    output_frame->SetCallerConstantPool(output_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_offset,
                         "caller's constant_pool\n");
  }

  // For the bottommost output frame the context can be gotten from the input
  // frame. For all subsequent output frames it can be gotten from the function
  // so long as we don't inline functions that need local contexts.
  output_offset -= kPointerSize;

  // When deoptimizing into a catch block, we need to take the context
  // from a register that was specified in the handler table.
  TranslatedFrame::iterator context_pos = value_iterator;
  int context_input_index = input_index;
  if (goto_catch_handler) {
    // Skip to the translated value of the register specified
    // in the handler table.
    for (int i = 0; i < catch_handler_data_ + 1; ++i) {
      context_pos++;
      context_input_index++;
    }
  }
  // Read the context from the translations.
  Object* context = context_pos->GetRawValue();
  value = reinterpret_cast<intptr_t>(context);
  output_frame->SetContext(value);
  WriteValueToOutput(context, context_input_index, frame_index, output_offset,
                     "context    ");
  if (context == isolate_->heap()->arguments_marker()) {
    Address output_address =
        reinterpret_cast<Address>(output_[frame_index]->GetTop()) +
        output_offset;
    values_to_materialize_.push_back({output_address, context_pos});
  }
  value_iterator++;
  input_index++;

  // The function was mentioned explicitly in the BEGIN_FRAME.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  WriteValueToOutput(function, 0, frame_index, output_offset, "function    ");
  if (function == isolate_->heap()->arguments_marker()) {
    Address output_address =
        reinterpret_cast<Address>(output_[frame_index]->GetTop()) +
        output_offset;
    values_to_materialize_.push_back({output_address, function_iterator});
  }

  // Set the bytecode array pointer.
  output_offset -= kPointerSize;
  Object* bytecode_array = shared->HasBreakInfo()
                               ? shared->GetDebugInfo()->DebugBytecodeArray()
                               : shared->bytecode_array();
  WriteValueToOutput(bytecode_array, 0, frame_index, output_offset,
                     "bytecode array ");

  // The bytecode offset was mentioned explicitly in the BEGIN_FRAME.
  output_offset -= kPointerSize;

  int raw_bytecode_offset =
      BytecodeArray::kHeaderSize - kHeapObjectTag + bytecode_offset;
  Smi* smi_bytecode_offset = Smi::FromInt(raw_bytecode_offset);
  output_[frame_index]->SetFrameSlot(
      output_offset, reinterpret_cast<intptr_t>(smi_bytecode_offset));

  if (trace_scope_ != nullptr) {
    DebugPrintOutputSlot(reinterpret_cast<intptr_t>(smi_bytecode_offset),
                         frame_index, output_offset, "bytecode offset @ ");
    PrintF(trace_scope_->file(), "%d\n", bytecode_offset);
    PrintF(trace_scope_->file(), "  (input #0)\n");
    PrintF(trace_scope_->file(), "    -------------------------\n");
  }

  // Translate the rest of the interpreter registers in the frame.
  for (int i = 0; i < register_count; ++i) {
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
  }

  int register_slots_written = register_count;
  DCHECK_LE(register_slots_written, register_stack_slot_count);
  // Some architectures must pad the stack frame with extra stack slots
  // to ensure the stack frame is aligned. Do this now.
  while (register_slots_written < register_stack_slot_count) {
    register_slots_written++;
    output_offset -= kPointerSize;
    WriteValueToOutput(isolate()->heap()->the_hole_value(), 0, frame_index,
                       output_offset, "padding ");
  }

  // Translate the accumulator register (depending on frame position).
  if (is_topmost) {
    if (PadTopOfStackRegister()) {
      output_offset -= kPointerSize;
      WriteValueToOutput(isolate()->heap()->the_hole_value(), 0, frame_index,
                         output_offset, "padding ");
    }
    // For topmost frame, put the accumulator on the stack. The
    // {NotifyDeoptimized} builtin pops it off the topmost frame (possibly
    // after materialization).
    output_offset -= kPointerSize;
    if (goto_catch_handler) {
      // If we are lazy deopting to a catch handler, we set the accumulator to
      // the exception (which lives in the result register).
      intptr_t accumulator_value =
          input_->GetRegister(kInterpreterAccumulatorRegister.code());
      WriteValueToOutput(reinterpret_cast<Object*>(accumulator_value), 0,
                         frame_index, output_offset, "accumulator ");
      value_iterator++;
    } else {
      WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                   output_offset, "accumulator ");
    }
  } else {
    // For non-topmost frames, skip the accumulator translation. For those
    // frames, the return value from the callee will become the accumulator.
    value_iterator++;
    input_index++;
  }
  CHECK_EQ(0u, output_offset);

  // Compute this frame's PC and state. The PC will be a special builtin that
  // continues the bytecode dispatch. Note that non-topmost and lazy-style
  // bailout handlers also advance the bytecode offset before dispatch, hence
  // simulating what normal handlers do upon completion of the operation.
  Builtins* builtins = isolate_->builtins();
  Code* dispatch_builtin =
      (!is_topmost || (bailout_type_ == LAZY)) && !goto_catch_handler
          ? builtins->builtin(Builtins::kInterpreterEnterBytecodeAdvance)
          : builtins->builtin(Builtins::kInterpreterEnterBytecodeDispatch);
  output_frame->SetPc(reinterpret_cast<intptr_t>(dispatch_builtin->entry()));

  // Update constant pool.
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(dispatch_builtin->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
    if (is_topmost) {
      Register constant_pool_reg =
          InterpretedFrame::constant_pool_pointer_register();
      output_frame->SetRegister(constant_pool_reg.code(), constant_pool_value);
    }
  }

  // Clear the context register. The context might be a de-materialized object
  // and will be materialized by {Runtime_NotifyDeoptimized}. For additional
  // safety we use Smi(0) instead of the potential {arguments_marker} here.
  if (is_topmost) {
    intptr_t context_value = reinterpret_cast<intptr_t>(Smi::kZero);
    Register context_reg = JavaScriptFrame::context_register();
    output_frame->SetRegister(context_reg.code(), context_value);
    // Set the continuation for the topmost frame.
    Code* continuation = builtins->builtin(Builtins::kNotifyDeoptimized);
    output_frame->SetContinuation(
        reinterpret_cast<intptr_t>(continuation->entry()));
  }
}

void Deoptimizer::DoComputeArgumentsAdaptorFrame(
    TranslatedFrame* translated_frame, int frame_index) {
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  bool is_bottommost = (0 == frame_index);
  int input_index = 0;

  unsigned height = translated_frame->height();
  unsigned height_in_bytes = height * kPointerSize;
  TranslatedFrame::iterator function_iterator = value_iterator;
  Object* function = value_iterator->GetRawValue();
  value_iterator++;
  input_index++;
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(),
           "  translating arguments adaptor => height=%d\n", height_in_bytes);
  }

  unsigned fixed_frame_size = ArgumentsAdaptorFrameConstants::kFixedFrameSize;
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  int parameter_count = height;
  FrameDescription* output_frame = new (output_frame_size)
      FrameDescription(output_frame_size, parameter_count);

  // Arguments adaptor can not be topmost.
  CHECK(frame_index < output_count_ - 1);
  CHECK_NULL(output_[frame_index]);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous frame's top and
  // this frame's size.
  intptr_t top_address;
  if (is_bottommost) {
    top_address = caller_frame_top_ - output_frame_size;
  } else {
    top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  }
  output_frame->SetTop(top_address);

  // Compute the incoming parameter translation.
  unsigned output_offset = output_frame_size;
  for (int i = 0; i < parameter_count; ++i) {
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
  }

  // Read caller's PC from the previous frame.
  output_offset -= kPCOnStackSize;
  intptr_t value;
  if (is_bottommost) {
    value = caller_pc_;
  } else {
    value = output_[frame_index - 1]->GetPc();
  }
  output_frame->SetCallerPc(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's pc\n");

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_offset -= kFPOnStackSize;
  if (is_bottommost) {
    value = caller_fp_;
  } else {
    value = output_[frame_index - 1]->GetFp();
  }
  output_frame->SetCallerFp(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's fp\n");

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_offset -= kPointerSize;
    if (is_bottommost) {
      value = caller_constant_pool_;
    } else {
      value = output_[frame_index - 1]->GetConstantPool();
    }
    output_frame->SetCallerConstantPool(output_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_offset,
                         "caller's constant_pool\n");
  }

  // A marker value is used in place of the context.
  output_offset -= kPointerSize;
  intptr_t context = StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR);
  output_frame->SetFrameSlot(output_offset, context);
  DebugPrintOutputSlot(context, frame_index, output_offset,
                       "context (adaptor sentinel)\n");

  // The function was mentioned explicitly in the ARGUMENTS_ADAPTOR_FRAME.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  WriteValueToOutput(function, 0, frame_index, output_offset, "function    ");
  if (function == isolate_->heap()->arguments_marker()) {
    Address output_address =
        reinterpret_cast<Address>(output_[frame_index]->GetTop()) +
        output_offset;
    values_to_materialize_.push_back({output_address, function_iterator});
  }

  // Number of incoming arguments.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(height - 1));
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "argc ");
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(), "(%d)\n", height - 1);
  }

  DCHECK_EQ(0, output_offset);

  Builtins* builtins = isolate_->builtins();
  Code* adaptor_trampoline =
      builtins->builtin(Builtins::kArgumentsAdaptorTrampoline);
  intptr_t pc_value = reinterpret_cast<intptr_t>(
      adaptor_trampoline->instruction_start() +
      isolate_->heap()->arguments_adaptor_deopt_pc_offset()->value());
  output_frame->SetPc(pc_value);
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(adaptor_trampoline->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
  }
}

void Deoptimizer::DoComputeConstructStubFrame(TranslatedFrame* translated_frame,
                                              int frame_index) {
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  bool is_topmost = (output_count_ - 1 == frame_index);
  // The construct frame could become topmost only if we inlined a constructor
  // call which does a tail call (otherwise the tail callee's frame would be
  // the topmost one). So it could only be the LAZY case.
  CHECK(!is_topmost || bailout_type_ == LAZY);
  int input_index = 0;

  Builtins* builtins = isolate_->builtins();
  Code* construct_stub = builtins->builtin(
      FLAG_harmony_restrict_constructor_return
          ? Builtins::kJSConstructStubGenericRestrictedReturn
          : Builtins::kJSConstructStubGenericUnrestrictedReturn);
  BailoutId bailout_id = translated_frame->node_id();
  unsigned height = translated_frame->height();
  unsigned height_in_bytes = height * kPointerSize;

  // If the construct frame appears to be topmost we should ensure that the
  // value of result register is preserved during continuation execution.
  // We do this here by "pushing" the result of the constructor function to the
  // top of the reconstructed stack and popping it in
  // {Builtins::kNotifyDeoptimized}.
  if (is_topmost) {
    height_in_bytes += kPointerSize;
    if (PadTopOfStackRegister()) height_in_bytes += kPointerSize;
  }

  JSFunction* function = JSFunction::cast(value_iterator->GetRawValue());
  value_iterator++;
  input_index++;
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(),
           "  translating construct stub => bailout_id=%d (%s), height=%d\n",
           bailout_id.ToInt(),
           bailout_id == BailoutId::ConstructStubCreate() ? "create" : "invoke",
           height_in_bytes);
  }

  unsigned fixed_frame_size = ConstructFrameConstants::kFixedFrameSize;
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new (output_frame_size) FrameDescription(output_frame_size);

  // Construct stub can not be topmost.
  DCHECK(frame_index > 0 && frame_index < output_count_);
  DCHECK_NULL(output_[frame_index]);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous frame's top and
  // this frame's size.
  intptr_t top_address;
  top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  output_frame->SetTop(top_address);

  // Compute the incoming parameter translation.
  int parameter_count = height;
  unsigned output_offset = output_frame_size;
  for (int i = 0; i < parameter_count; ++i) {
    output_offset -= kPointerSize;
    // The allocated receiver of a construct stub frame is passed as the
    // receiver parameter through the translation. It might be encoding
    // a captured object, override the slot address for a captured object.
    WriteTranslatedValueToOutput(
        &value_iterator, &input_index, frame_index, output_offset, nullptr,
        (i == 0) ? reinterpret_cast<Address>(top_address) : nullptr);
  }

  // Read caller's PC from the previous frame.
  output_offset -= kPCOnStackSize;
  intptr_t callers_pc = output_[frame_index - 1]->GetPc();
  output_frame->SetCallerPc(output_offset, callers_pc);
  DebugPrintOutputSlot(callers_pc, frame_index, output_offset, "caller's pc\n");

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_offset -= kFPOnStackSize;
  intptr_t value = output_[frame_index - 1]->GetFp();
  output_frame->SetCallerFp(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  if (is_topmost) {
    Register fp_reg = JavaScriptFrame::fp_register();
    output_frame->SetRegister(fp_reg.code(), fp_value);
  }
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's fp\n");

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_offset -= kPointerSize;
    value = output_[frame_index - 1]->GetConstantPool();
    output_frame->SetCallerConstantPool(output_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_offset,
                         "caller's constant_pool\n");
  }

  // A marker value is used to mark the frame.
  output_offset -= kPointerSize;
  value = StackFrame::TypeToMarker(StackFrame::CONSTRUCT);
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset,
                       "typed frame marker\n");

  // The context can be gotten from the previous frame.
  output_offset -= kPointerSize;
  value = output_[frame_index - 1]->GetContext();
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "context\n");

  // Number of incoming arguments.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(height - 1));
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "argc ");
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(), "(%d)\n", height - 1);
  }

  // The constructor function was mentioned explicitly in the
  // CONSTRUCT_STUB_FRAME.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  WriteValueToOutput(function, 0, frame_index, output_offset,
                     "constructor function ");

  // The deopt info contains the implicit receiver or the new target at the
  // position of the receiver. Copy it to the top of stack.
  output_offset -= kPointerSize;
  value = output_frame->GetFrameSlot(output_frame_size - kPointerSize);
  output_frame->SetFrameSlot(output_offset, value);
  if (bailout_id == BailoutId::ConstructStubCreate()) {
    DebugPrintOutputSlot(value, frame_index, output_offset, "new target\n");
  } else {
    CHECK(bailout_id == BailoutId::ConstructStubInvoke());
    DebugPrintOutputSlot(value, frame_index, output_offset,
                         "allocated receiver\n");
  }

  if (is_topmost) {
    if (PadTopOfStackRegister()) {
      output_offset -= kPointerSize;
      WriteValueToOutput(isolate()->heap()->the_hole_value(), 0, frame_index,
                         output_offset, "padding ");
    }
    // Ensure the result is restored back when we return to the stub.
    output_offset -= kPointerSize;
    Register result_reg = kReturnRegister0;
    value = input_->GetRegister(result_reg.code());
    output_frame->SetFrameSlot(output_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_offset, "subcall result\n");
  }

  CHECK_EQ(0u, output_offset);

  // Compute this frame's PC.
  DCHECK(bailout_id.IsValidForConstructStub());
  Address start = construct_stub->instruction_start();
  int pc_offset =
      bailout_id == BailoutId::ConstructStubCreate()
          ? isolate_->heap()->construct_stub_create_deopt_pc_offset()->value()
          : isolate_->heap()->construct_stub_invoke_deopt_pc_offset()->value();
  intptr_t pc_value = reinterpret_cast<intptr_t>(start + pc_offset);
  output_frame->SetPc(pc_value);

  // Update constant pool.
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(construct_stub->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
    if (is_topmost) {
      Register constant_pool_reg =
          JavaScriptFrame::constant_pool_pointer_register();
      output_frame->SetRegister(constant_pool_reg.code(), fp_value);
    }
  }

  // Clear the context register. The context might be a de-materialized object
  // and will be materialized by {Runtime_NotifyDeoptimized}. For additional
  // safety we use Smi(0) instead of the potential {arguments_marker} here.
  if (is_topmost) {
    intptr_t context_value = reinterpret_cast<intptr_t>(Smi::kZero);
    Register context_reg = JavaScriptFrame::context_register();
    output_frame->SetRegister(context_reg.code(), context_value);
  }

  // Set the continuation for the topmost frame.
  if (is_topmost) {
    Builtins* builtins = isolate_->builtins();
    DCHECK_EQ(LAZY, bailout_type_);
    Code* continuation = builtins->builtin(Builtins::kNotifyDeoptimized);
    output_frame->SetContinuation(
        reinterpret_cast<intptr_t>(continuation->entry()));
  }
}

// BuiltinContinuationFrames capture the machine state that is expected as input
// to a builtin, including both input register values and stack parameters. When
// the frame is reactivated (i.e. the frame below it returns), a
// ContinueToBuiltin stub restores the register state from the frame and tail
// calls to the actual target builtin, making it appear that the stub had been
// directly called by the frame above it. The input values to populate the frame
// are taken from the deopt's FrameState.
//
// Frame translation happens in two modes, EAGER and LAZY. In EAGER mode, all of
// the parameters to the Builtin are explicitly specified in the TurboFan
// FrameState node. In LAZY mode, there is always one fewer parameters specified
// in the FrameState than expected by the Builtin. In that case, construction of
// BuiltinContinuationFrame adds the final missing parameter during
// deoptimization, and that parameter is always on the stack and contains the
// value returned from the callee of the call site triggering the LAZY deopt
// (e.g. rax on x64). This requires that continuation Builtins for LAZY deopts
// must have at least one stack parameter.
//
//                TO
//    |          ....           |
//    +-------------------------+
//    |     builtin param 0     |<- FrameState input value n becomes
//    +-------------------------+
//    |           ...           |
//    +-------------------------+
//    |     builtin param m     |<- FrameState input value n+m-1, or in
//    +-------------------------+   the LAZY case, return LAZY result value
//    | ContinueToBuiltin entry |
//    +-------------------------+
// |  |    saved frame (FP)     |
// |  +=========================+<- fpreg
// |  |constant pool (if ool_cp)|
// v  +-------------------------+
//    |BUILTIN_CONTINUATION mark|
//    +-------------------------+
//    |  JS Builtin code object |
//    +-------------------------+
//    | builtin input GPR reg0  |<- populated from deopt FrameState using
//    +-------------------------+   the builtin's CallInterfaceDescriptor
//    |          ...            |   to map a FrameState's 0..n-1 inputs to
//    +-------------------------+   the builtin's n input register params.
//    | builtin input GPR regn  |
//    |-------------------------|<- spreg
//
void Deoptimizer::DoComputeBuiltinContinuation(
    TranslatedFrame* translated_frame, int frame_index,
    bool java_script_builtin) {
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  int input_index = 0;

  // The output frame must have room for all of the parameters that need to be
  // passed to the builtin continuation.
  int height_in_words = translated_frame->height();

  BailoutId bailout_id = translated_frame->node_id();
  Builtins::Name builtin_name = Builtins::GetBuiltinFromBailoutId(bailout_id);
  DCHECK(!Builtins::IsLazy(builtin_name));
  Code* builtin = isolate()->builtins()->builtin(builtin_name);
  Callable continuation_callable =
      Builtins::CallableFor(isolate(), builtin_name);
  CallInterfaceDescriptor continuation_descriptor =
      continuation_callable.descriptor();

  bool is_bottommost = (0 == frame_index);
  bool is_topmost = (output_count_ - 1 == frame_index);
  bool must_handle_result = !is_topmost || bailout_type_ == LAZY;

  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  int padding_slot_count = BuiltinContinuationFrameConstants::PaddingSlotCount(
      allocatable_register_count);

  int register_parameter_count =
      continuation_descriptor.GetRegisterParameterCount();
  // Make sure to account for the context by removing it from the register
  // parameter count.
  int stack_param_count = height_in_words - register_parameter_count - 1;
  if (must_handle_result) stack_param_count++;
  int output_frame_size =
      kPointerSize * (stack_param_count + allocatable_register_count +
                      padding_slot_count) +
      BuiltinContinuationFrameConstants::kFixedFrameSize;

  // If the builtins frame appears to be topmost we should ensure that the
  // value of result register is preserved during continuation execution.
  // We do this here by "pushing" the result of callback function to the
  // top of the reconstructed stack and popping it in
  // {Builtins::kNotifyDeoptimized}.
  if (is_topmost) {
    output_frame_size += kPointerSize;
    if (PadTopOfStackRegister()) output_frame_size += kPointerSize;
  }

  // Validate types of parameters. They must all be tagged except for argc for
  // JS builtins.
  bool has_argc = false;
  for (int i = 0; i < register_parameter_count; ++i) {
    MachineType type = continuation_descriptor.GetParameterType(i);
    int code = continuation_descriptor.GetRegisterParameter(i).code();
    // Only tagged and int32 arguments are supported, and int32 only for the
    // arguments count on JavaScript builtins.
    if (type == MachineType::Int32()) {
      CHECK_EQ(code, kJavaScriptCallArgCountRegister.code());
      has_argc = true;
    } else {
      // Any other argument must be a tagged value.
      CHECK(IsAnyTagged(type.representation()));
    }
  }
  CHECK_EQ(java_script_builtin, has_argc);

  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(),
           "  translating BuiltinContinuation to %s,"
           " register param count %d,"
           " stack param count %d\n",
           Builtins::name(builtin_name), register_parameter_count,
           stack_param_count);
  }

  unsigned output_frame_offset = output_frame_size;
  FrameDescription* output_frame =
      new (output_frame_size) FrameDescription(output_frame_size);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous frame's top and
  // this frame's size.
  intptr_t top_address;
  if (is_bottommost) {
    top_address = caller_frame_top_ - output_frame_size;
  } else {
    top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  }
  output_frame->SetTop(top_address);

  // Get the possible JSFunction for the case that this is a
  // JavaScriptBuiltinContinuationFrame, which needs the JSFunction pointer
  // like a normal JavaScriptFrame.
  intptr_t maybe_function =
      reinterpret_cast<intptr_t>(value_iterator->GetRawValue());
  ++input_index;
  ++value_iterator;

  struct RegisterValue {
    Object* raw_value_;
    TranslatedFrame::iterator iterator_;
  };
  std::vector<RegisterValue> register_values;
  int total_registers = config->num_general_registers();
  register_values.resize(total_registers, {Smi::kZero, value_iterator});

  intptr_t value;

  int translated_stack_parameters =
      must_handle_result ? stack_param_count - 1 : stack_param_count;

  for (int i = 0; i < translated_stack_parameters; ++i) {
    output_frame_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_frame_offset);
  }

  if (must_handle_result) {
    output_frame_offset -= kPointerSize;
    WriteValueToOutput(isolate()->heap()->the_hole_value(), input_index,
                       frame_index, output_frame_offset,
                       "placeholder for return result on lazy deopt ");
  }

  for (int i = 0; i < register_parameter_count; ++i) {
    Object* object = value_iterator->GetRawValue();
    int code = continuation_descriptor.GetRegisterParameter(i).code();
    register_values[code] = {object, value_iterator};
    ++input_index;
    ++value_iterator;
  }

  // The context register is always implicit in the CallInterfaceDescriptor but
  // its register must be explicitly set when continuing to the builtin. Make
  // sure that it's harvested from the translation and copied into the register
  // set (it was automatically added at the end of the FrameState by the
  // instruction selector).
  Object* context = value_iterator->GetRawValue();
  value = reinterpret_cast<intptr_t>(context);
  register_values[kContextRegister.code()] = {context, value_iterator};
  output_frame->SetContext(value);
  output_frame->SetRegister(kContextRegister.code(), value);
  ++input_index;
  ++value_iterator;

  // Set caller's PC (JSFunction continuation).
  output_frame_offset -= kPCOnStackSize;
  if (is_bottommost) {
    value = caller_pc_;
  } else {
    value = output_[frame_index - 1]->GetPc();
  }
  output_frame->SetCallerPc(output_frame_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                       "caller's pc\n");

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_frame_offset -= kFPOnStackSize;
  if (is_bottommost) {
    value = caller_fp_;
  } else {
    value = output_[frame_index - 1]->GetFp();
  }
  output_frame->SetCallerFp(output_frame_offset, value);
  intptr_t fp_value = top_address + output_frame_offset;
  output_frame->SetFp(fp_value);
  DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                       "caller's fp\n");

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_frame_offset -= kPointerSize;
    if (is_bottommost) {
      value = caller_constant_pool_;
    } else {
      value = output_[frame_index - 1]->GetConstantPool();
    }
    output_frame->SetCallerConstantPool(output_frame_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                         "caller's constant_pool\n");
  }

  // A marker value is used in place of the context.
  output_frame_offset -= kPointerSize;
  intptr_t marker =
      java_script_builtin
          ? StackFrame::TypeToMarker(
                StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION)
          : StackFrame::TypeToMarker(StackFrame::BUILTIN_CONTINUATION);
  output_frame->SetFrameSlot(output_frame_offset, marker);
  DebugPrintOutputSlot(marker, frame_index, output_frame_offset,
                       "context (builtin continuation sentinel)\n");

  output_frame_offset -= kPointerSize;
  value = java_script_builtin ? maybe_function : 0;
  output_frame->SetFrameSlot(output_frame_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                       java_script_builtin ? "JSFunction\n" : "unused\n");

  // The builtin to continue to
  output_frame_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(builtin);
  output_frame->SetFrameSlot(output_frame_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                       "builtin address\n");

  for (int i = 0; i < allocatable_register_count; ++i) {
    output_frame_offset -= kPointerSize;
    int code = config->GetAllocatableGeneralCode(i);
    Object* object = register_values[code].raw_value_;
    value = reinterpret_cast<intptr_t>(object);
    output_frame->SetFrameSlot(output_frame_offset, value);
    if (trace_scope_ != nullptr) {
      ScopedVector<char> str(128);
      if (java_script_builtin &&
          code == kJavaScriptCallArgCountRegister.code()) {
        SNPrintF(
            str,
            "tagged argument count %s (will be untagged by continuation)\n",
            config->GetGeneralRegisterName(code));
      } else {
        SNPrintF(str, "builtin register argument %s\n",
                 config->GetGeneralRegisterName(code));
      }
      DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                           str.start());
    }
    if (object == isolate_->heap()->arguments_marker()) {
      Address output_address =
          reinterpret_cast<Address>(output_[frame_index]->GetTop()) +
          output_frame_offset;
      values_to_materialize_.push_back(
          {output_address, register_values[code].iterator_});
    }
  }

  // Some architectures must pad the stack frame with extra stack slots
  // to ensure the stack frame is aligned.
  for (int i = 0; i < padding_slot_count; ++i) {
    output_frame_offset -= kPointerSize;
    WriteValueToOutput(isolate()->heap()->the_hole_value(), 0, frame_index,
                       output_frame_offset, "padding ");
  }

  if (is_topmost) {
    if (PadTopOfStackRegister()) {
      output_frame_offset -= kPointerSize;
      WriteValueToOutput(isolate()->heap()->the_hole_value(), 0, frame_index,
                         output_frame_offset, "padding ");
    }
    // Ensure the result is restored back when we return to the stub.
    output_frame_offset -= kPointerSize;
    Register result_reg = kReturnRegister0;
    if (must_handle_result) {
      value = input_->GetRegister(result_reg.code());
    } else {
      value = reinterpret_cast<intptr_t>(isolate()->heap()->undefined_value());
    }
    output_frame->SetFrameSlot(output_frame_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                         "callback result\n");
  }

  CHECK_EQ(0u, output_frame_offset);

  // Clear the context register. The context might be a de-materialized object
  // and will be materialized by {Runtime_NotifyDeoptimized}. For additional
  // safety we use Smi(0) instead of the potential {arguments_marker} here.
  if (is_topmost) {
    intptr_t context_value = reinterpret_cast<intptr_t>(Smi::kZero);
    Register context_reg = JavaScriptFrame::context_register();
    output_frame->SetRegister(context_reg.code(), context_value);
  }

  // TODO(6898): For eager deopts within builtin stub frames we currently skip
  // marking the underlying function as deoptimized. This is to avoid deopt
  // loops where we would just generate the same optimized code all over again.
  if (is_topmost && bailout_type_ != LAZY) {
    preserve_optimized_ = true;
  }

  // Ensure the frame pointer register points to the callee's frame. The builtin
  // will build its own frame once we continue to it.
  Register fp_reg = JavaScriptFrame::fp_register();
  output_frame->SetRegister(fp_reg.code(), output_[frame_index - 1]->GetFp());

  Code* continue_to_builtin =
      java_script_builtin
          ? (must_handle_result
                 ? isolate()->builtins()->builtin(
                       Builtins::kContinueToJavaScriptBuiltinWithResult)
                 : isolate()->builtins()->builtin(
                       Builtins::kContinueToJavaScriptBuiltin))
          : (must_handle_result
                 ? isolate()->builtins()->builtin(
                       Builtins::kContinueToCodeStubBuiltinWithResult)
                 : isolate()->builtins()->builtin(
                       Builtins::kContinueToCodeStubBuiltin));
  output_frame->SetPc(
      reinterpret_cast<intptr_t>(continue_to_builtin->instruction_start()));

  Code* continuation =
      isolate()->builtins()->builtin(Builtins::kNotifyDeoptimized);
  output_frame->SetContinuation(
      reinterpret_cast<intptr_t>(continuation->entry()));
}

void Deoptimizer::MaterializeHeapObjects() {
  translated_state_.Prepare(reinterpret_cast<Address>(stack_fp_));

  for (auto& materialization : values_to_materialize_) {
    Handle<Object> value = materialization.value_->GetValue();

    if (trace_scope_ != nullptr) {
      PrintF("Materialization [0x%08" V8PRIxPTR "] <- 0x%08" V8PRIxPTR " ;  ",
             reinterpret_cast<intptr_t>(materialization.output_slot_address_),
             reinterpret_cast<intptr_t>(*value));
      value->ShortPrint(trace_scope_->file());
      PrintF(trace_scope_->file(), "\n");
    }

    *(reinterpret_cast<intptr_t*>(materialization.output_slot_address_)) =
        reinterpret_cast<intptr_t>(*value);
  }

  isolate_->materialized_object_store()->Remove(
      reinterpret_cast<Address>(stack_fp_));
}


void Deoptimizer::WriteTranslatedValueToOutput(
    TranslatedFrame::iterator* iterator, int* input_index, int frame_index,
    unsigned output_offset, const char* debug_hint_string,
    Address output_address_for_materialization) {
  Object* value = (*iterator)->GetRawValue();

  WriteValueToOutput(value, *input_index, frame_index, output_offset,
                     debug_hint_string);

  if (value == isolate_->heap()->arguments_marker()) {
    Address output_address =
        reinterpret_cast<Address>(output_[frame_index]->GetTop()) +
        output_offset;
    if (output_address_for_materialization == nullptr) {
      output_address_for_materialization = output_address;
    }
    values_to_materialize_.push_back(
        {output_address_for_materialization, *iterator});
  }

  (*iterator)++;
  (*input_index)++;
}


void Deoptimizer::WriteValueToOutput(Object* value, int input_index,
                                     int frame_index, unsigned output_offset,
                                     const char* debug_hint_string) {
  output_[frame_index]->SetFrameSlot(output_offset,
                                     reinterpret_cast<intptr_t>(value));

  if (trace_scope_ != nullptr) {
    DebugPrintOutputSlot(reinterpret_cast<intptr_t>(value), frame_index,
                         output_offset, debug_hint_string);
    value->ShortPrint(trace_scope_->file());
    PrintF(trace_scope_->file(), "  (input #%d)\n", input_index);
  }
}


void Deoptimizer::DebugPrintOutputSlot(intptr_t value, int frame_index,
                                       unsigned output_offset,
                                       const char* debug_hint_string) {
  if (trace_scope_ != nullptr) {
    Address output_address =
        reinterpret_cast<Address>(output_[frame_index]->GetTop()) +
        output_offset;
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08" V8PRIxPTR " ;  %s",
           reinterpret_cast<intptr_t>(output_address), output_offset, value,
           debug_hint_string == nullptr ? "" : debug_hint_string);
  }
}

unsigned Deoptimizer::ComputeInputFrameAboveFpFixedSize() const {
  unsigned fixed_size = CommonFrameConstants::kFixedFrameSizeAboveFp;
  if (!function_->IsSmi()) {
    fixed_size += ComputeIncomingArgumentSize(function_->shared());
  }
  return fixed_size;
}

unsigned Deoptimizer::ComputeInputFrameSize() const {
  // The fp-to-sp delta already takes the context, constant pool pointer and the
  // function into account so we have to avoid double counting them.
  unsigned fixed_size_above_fp = ComputeInputFrameAboveFpFixedSize();
  unsigned result = fixed_size_above_fp + fp_to_sp_delta_;
  if (compiled_code_->kind() == Code::OPTIMIZED_FUNCTION) {
    unsigned stack_slots = compiled_code_->stack_slots();
    unsigned outgoing_size = 0;
    //        ComputeOutgoingArgumentSize(compiled_code_, bailout_id_);
    CHECK_EQ(fixed_size_above_fp + (stack_slots * kPointerSize) -
                 CommonFrameConstants::kFixedFrameSizeAboveFp + outgoing_size,
             result);
  }
  return result;
}

// static
unsigned Deoptimizer::ComputeJavascriptFixedSize(SharedFunctionInfo* shared) {
  // The fixed part of the frame consists of the return address, frame
  // pointer, function, context, and all the incoming arguments.
  return ComputeIncomingArgumentSize(shared) +
         StandardFrameConstants::kFixedFrameSize;
}

// static
unsigned Deoptimizer::ComputeInterpretedFixedSize(SharedFunctionInfo* shared) {
  // The fixed part of the frame consists of the return address, frame
  // pointer, function, context, bytecode offset and all the incoming arguments.
  return ComputeIncomingArgumentSize(shared) +
         InterpreterFrameConstants::kFixedFrameSize;
}

// static
unsigned Deoptimizer::ComputeIncomingArgumentSize(SharedFunctionInfo* shared) {
  return (shared->internal_formal_parameter_count() + 1) * kPointerSize;
}

void Deoptimizer::EnsureCodeForDeoptimizationEntry(Isolate* isolate,
                                                   BailoutType type) {
  CHECK(type == EAGER || type == SOFT || type == LAZY);
  DeoptimizerData* data = isolate->deoptimizer_data();
  if (data->deopt_entry_code_[type] != nullptr) return;

  MacroAssembler masm(isolate, nullptr, 16 * KB, CodeObjectRequired::kYes);
  masm.set_emit_debug_code(false);
  GenerateDeoptimizationEntries(&masm, kMaxNumberOfEntries, type);
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  // Allocate the code as immovable since the entry addresses will be used
  // directly and there is no support for relocating them.
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::STUB, Handle<Object>(), Builtins::kNoBuiltinId,
      MaybeHandle<HandlerTable>(), MaybeHandle<ByteArray>(),
      MaybeHandle<DeoptimizationData>(), kImmovable);
  CHECK(Heap::IsImmovable(*code));

  CHECK_NULL(data->deopt_entry_code_[type]);
  data->deopt_entry_code_[type] = *code;
}

void Deoptimizer::EnsureCodeForMaxDeoptimizationEntries(Isolate* isolate) {
  EnsureCodeForDeoptimizationEntry(isolate, EAGER);
  EnsureCodeForDeoptimizationEntry(isolate, LAZY);
  EnsureCodeForDeoptimizationEntry(isolate, SOFT);
}

FrameDescription::FrameDescription(uint32_t frame_size, int parameter_count)
    : frame_size_(frame_size),
      parameter_count_(parameter_count),
      top_(kZapUint32),
      pc_(kZapUint32),
      fp_(kZapUint32),
      context_(kZapUint32),
      constant_pool_(kZapUint32) {
  // Zap all the registers.
  for (int r = 0; r < Register::kNumRegisters; r++) {
    // TODO(jbramley): It isn't safe to use kZapUint32 here. If the register
    // isn't used before the next safepoint, the GC will try to scan it as a
    // tagged value. kZapUint32 looks like a valid tagged pointer, but it isn't.
    SetRegister(r, kZapUint32);
  }

  // Zap all the slots.
  for (unsigned o = 0; o < frame_size; o += kPointerSize) {
    SetFrameSlot(o, kZapUint32);
  }
}

void TranslationBuffer::Add(int32_t value) {
  // This wouldn't handle kMinInt correctly if it ever encountered it.
  DCHECK_NE(value, kMinInt);
  // Encode the sign bit in the least significant bit.
  bool is_negative = (value < 0);
  uint32_t bits = ((is_negative ? -value : value) << 1) |
      static_cast<int32_t>(is_negative);
  // Encode the individual bytes using the least significant bit of
  // each byte to indicate whether or not more bytes follow.
  do {
    uint32_t next = bits >> 7;
    contents_.push_back(((bits << 1) & 0xFF) | (next != 0));
    bits = next;
  } while (bits != 0);
}

TranslationIterator::TranslationIterator(ByteArray* buffer, int index)
    : buffer_(buffer), index_(index) {
  DCHECK(index >= 0 && index < buffer->length());
}

int32_t TranslationIterator::Next() {
  // Run through the bytes until we reach one with a least significant
  // bit of zero (marks the end).
  uint32_t bits = 0;
  for (int i = 0; true; i += 7) {
    DCHECK(HasNext());
    uint8_t next = buffer_->get(index_++);
    bits |= (next >> 1) << i;
    if ((next & 1) == 0) break;
  }
  // The bits encode the sign in the least significant bit.
  bool is_negative = (bits & 1) == 1;
  int32_t result = bits >> 1;
  return is_negative ? -result : result;
}

bool TranslationIterator::HasNext() const { return index_ < buffer_->length(); }

Handle<ByteArray> TranslationBuffer::CreateByteArray(Factory* factory) {
  Handle<ByteArray> result = factory->NewByteArray(CurrentIndex(), TENURED);
  contents_.CopyTo(result->GetDataStartAddress());
  return result;
}

void Translation::BeginBuiltinContinuationFrame(BailoutId bailout_id,
                                                int literal_id,
                                                unsigned height) {
  buffer_->Add(BUILTIN_CONTINUATION_FRAME);
  buffer_->Add(bailout_id.ToInt());
  buffer_->Add(literal_id);
  buffer_->Add(height);
}

void Translation::BeginJavaScriptBuiltinContinuationFrame(BailoutId bailout_id,
                                                          int literal_id,
                                                          unsigned height) {
  buffer_->Add(JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME);
  buffer_->Add(bailout_id.ToInt());
  buffer_->Add(literal_id);
  buffer_->Add(height);
}

void Translation::BeginConstructStubFrame(BailoutId bailout_id, int literal_id,
                                          unsigned height) {
  buffer_->Add(CONSTRUCT_STUB_FRAME);
  buffer_->Add(bailout_id.ToInt());
  buffer_->Add(literal_id);
  buffer_->Add(height);
}


void Translation::BeginArgumentsAdaptorFrame(int literal_id, unsigned height) {
  buffer_->Add(ARGUMENTS_ADAPTOR_FRAME);
  buffer_->Add(literal_id);
  buffer_->Add(height);
}

void Translation::BeginInterpretedFrame(BailoutId bytecode_offset,
                                        int literal_id, unsigned height) {
  buffer_->Add(INTERPRETED_FRAME);
  buffer_->Add(bytecode_offset.ToInt());
  buffer_->Add(literal_id);
  buffer_->Add(height);
}

void Translation::ArgumentsElements(CreateArgumentsType type) {
  buffer_->Add(ARGUMENTS_ELEMENTS);
  buffer_->Add(static_cast<uint8_t>(type));
}

void Translation::ArgumentsLength(CreateArgumentsType type) {
  buffer_->Add(ARGUMENTS_LENGTH);
  buffer_->Add(static_cast<uint8_t>(type));
}

void Translation::BeginCapturedObject(int length) {
  buffer_->Add(CAPTURED_OBJECT);
  buffer_->Add(length);
}


void Translation::DuplicateObject(int object_index) {
  buffer_->Add(DUPLICATED_OBJECT);
  buffer_->Add(object_index);
}


void Translation::StoreRegister(Register reg) {
  buffer_->Add(REGISTER);
  buffer_->Add(reg.code());
}


void Translation::StoreInt32Register(Register reg) {
  buffer_->Add(INT32_REGISTER);
  buffer_->Add(reg.code());
}


void Translation::StoreUint32Register(Register reg) {
  buffer_->Add(UINT32_REGISTER);
  buffer_->Add(reg.code());
}


void Translation::StoreBoolRegister(Register reg) {
  buffer_->Add(BOOL_REGISTER);
  buffer_->Add(reg.code());
}

void Translation::StoreFloatRegister(FloatRegister reg) {
  buffer_->Add(FLOAT_REGISTER);
  buffer_->Add(reg.code());
}

void Translation::StoreDoubleRegister(DoubleRegister reg) {
  buffer_->Add(DOUBLE_REGISTER);
  buffer_->Add(reg.code());
}


void Translation::StoreStackSlot(int index) {
  buffer_->Add(STACK_SLOT);
  buffer_->Add(index);
}


void Translation::StoreInt32StackSlot(int index) {
  buffer_->Add(INT32_STACK_SLOT);
  buffer_->Add(index);
}


void Translation::StoreUint32StackSlot(int index) {
  buffer_->Add(UINT32_STACK_SLOT);
  buffer_->Add(index);
}


void Translation::StoreBoolStackSlot(int index) {
  buffer_->Add(BOOL_STACK_SLOT);
  buffer_->Add(index);
}

void Translation::StoreFloatStackSlot(int index) {
  buffer_->Add(FLOAT_STACK_SLOT);
  buffer_->Add(index);
}

void Translation::StoreDoubleStackSlot(int index) {
  buffer_->Add(DOUBLE_STACK_SLOT);
  buffer_->Add(index);
}


void Translation::StoreLiteral(int literal_id) {
  buffer_->Add(LITERAL);
  buffer_->Add(literal_id);
}


void Translation::StoreJSFrameFunction() {
  StoreStackSlot((StandardFrameConstants::kCallerPCOffset -
                  StandardFrameConstants::kFunctionOffset) /
                 kPointerSize);
}

int Translation::NumberOfOperandsFor(Opcode opcode) {
  switch (opcode) {
    case DUPLICATED_OBJECT:
    case ARGUMENTS_ELEMENTS:
    case ARGUMENTS_LENGTH:
    case CAPTURED_OBJECT:
    case REGISTER:
    case INT32_REGISTER:
    case UINT32_REGISTER:
    case BOOL_REGISTER:
    case FLOAT_REGISTER:
    case DOUBLE_REGISTER:
    case STACK_SLOT:
    case INT32_STACK_SLOT:
    case UINT32_STACK_SLOT:
    case BOOL_STACK_SLOT:
    case FLOAT_STACK_SLOT:
    case DOUBLE_STACK_SLOT:
    case LITERAL:
      return 1;
    case BEGIN:
    case ARGUMENTS_ADAPTOR_FRAME:
      return 2;
    case INTERPRETED_FRAME:
    case CONSTRUCT_STUB_FRAME:
    case BUILTIN_CONTINUATION_FRAME:
    case JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME:
      return 3;
  }
  FATAL("Unexpected translation type");
  return -1;
}


#if defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)

const char* Translation::StringFor(Opcode opcode) {
#define TRANSLATION_OPCODE_CASE(item)   case item: return #item;
  switch (opcode) {
    TRANSLATION_OPCODE_LIST(TRANSLATION_OPCODE_CASE)
  }
#undef TRANSLATION_OPCODE_CASE
  UNREACHABLE();
}

#endif


Handle<FixedArray> MaterializedObjectStore::Get(Address fp) {
  int index = StackIdToIndex(fp);
  if (index == -1) {
    return Handle<FixedArray>::null();
  }
  Handle<FixedArray> array = GetStackEntries();
  CHECK_GT(array->length(), index);
  return Handle<FixedArray>::cast(Handle<Object>(array->get(index), isolate()));
}


void MaterializedObjectStore::Set(Address fp,
                                  Handle<FixedArray> materialized_objects) {
  int index = StackIdToIndex(fp);
  if (index == -1) {
    index = static_cast<int>(frame_fps_.size());
    frame_fps_.push_back(fp);
  }

  Handle<FixedArray> array = EnsureStackEntries(index + 1);
  array->set(index, *materialized_objects);
}


bool MaterializedObjectStore::Remove(Address fp) {
  auto it = std::find(frame_fps_.begin(), frame_fps_.end(), fp);
  if (it == frame_fps_.end()) return false;
  int index = static_cast<int>(std::distance(frame_fps_.begin(), it));

  frame_fps_.erase(it);
  FixedArray* array = isolate()->heap()->materialized_objects();

  CHECK_LT(index, array->length());
  int fps_size = static_cast<int>(frame_fps_.size());
  for (int i = index; i < fps_size; i++) {
    array->set(i, array->get(i + 1));
  }
  array->set(fps_size, isolate()->heap()->undefined_value());
  return true;
}


int MaterializedObjectStore::StackIdToIndex(Address fp) {
  auto it = std::find(frame_fps_.begin(), frame_fps_.end(), fp);
  return it == frame_fps_.end()
             ? -1
             : static_cast<int>(std::distance(frame_fps_.begin(), it));
}


Handle<FixedArray> MaterializedObjectStore::GetStackEntries() {
  return Handle<FixedArray>(isolate()->heap()->materialized_objects());
}


Handle<FixedArray> MaterializedObjectStore::EnsureStackEntries(int length) {
  Handle<FixedArray> array = GetStackEntries();
  if (array->length() >= length) {
    return array;
  }

  int new_length = length > 10 ? length : 10;
  if (new_length < 2 * array->length()) {
    new_length = 2 * array->length();
  }

  Handle<FixedArray> new_array =
      isolate()->factory()->NewFixedArray(new_length, TENURED);
  for (int i = 0; i < array->length(); i++) {
    new_array->set(i, array->get(i));
  }
  for (int i = array->length(); i < length; i++) {
    new_array->set(i, isolate()->heap()->undefined_value());
  }
  isolate()->heap()->SetRootMaterializedObjects(*new_array);
  return new_array;
}

namespace {

Handle<Object> GetValueForDebugger(TranslatedFrame::iterator it,
                                   Isolate* isolate) {
  if (it->GetRawValue() == isolate->heap()->arguments_marker()) {
    if (!it->IsMaterializableByDebugger()) {
      return isolate->factory()->optimized_out();
    }
  }
  return it->GetValue();
}

}  // namespace

DeoptimizedFrameInfo::DeoptimizedFrameInfo(TranslatedState* state,
                                           TranslatedState::iterator frame_it,
                                           Isolate* isolate) {
  // If the previous frame is an adaptor frame, we will take the parameters
  // from there.
  TranslatedState::iterator parameter_frame = frame_it;
  if (parameter_frame != state->begin()) {
    parameter_frame--;
  }
  int parameter_count;
  if (parameter_frame->kind() == TranslatedFrame::kArgumentsAdaptor) {
    parameter_count = parameter_frame->height() - 1;  // Ignore the receiver.
  } else {
    parameter_frame = frame_it;
    parameter_count =
        frame_it->shared_info()->internal_formal_parameter_count();
  }
  TranslatedFrame::iterator parameter_it = parameter_frame->begin();
  parameter_it++;  // Skip the function.
  parameter_it++;  // Skip the receiver.

  // Figure out whether there is a construct stub frame on top of
  // the parameter frame.
  has_construct_stub_ =
      parameter_frame != state->begin() &&
      (parameter_frame - 1)->kind() == TranslatedFrame::kConstructStub;

  DCHECK_EQ(TranslatedFrame::kInterpretedFunction, frame_it->kind());
  source_position_ = Deoptimizer::ComputeSourcePositionFromBytecodeArray(
      *frame_it->shared_info(), frame_it->node_id());

  TranslatedFrame::iterator value_it = frame_it->begin();
  // Get the function. Note that this might materialize the function.
  // In case the debugger mutates this value, we should deoptimize
  // the function and remember the value in the materialized value store.
  function_ = Handle<JSFunction>::cast(value_it->GetValue());

  parameters_.resize(static_cast<size_t>(parameter_count));
  for (int i = 0; i < parameter_count; i++) {
    Handle<Object> parameter = GetValueForDebugger(parameter_it, isolate);
    SetParameter(i, parameter);
    parameter_it++;
  }

  // Skip the function, the receiver and the arguments.
  int skip_count =
      frame_it->shared_info()->internal_formal_parameter_count() + 2;
  TranslatedFrame::iterator stack_it = frame_it->begin();
  for (int i = 0; i < skip_count; i++) {
    stack_it++;
  }

  // Get the context.
  context_ = GetValueForDebugger(stack_it, isolate);
  stack_it++;

  // Get the expression stack.
  int stack_height = frame_it->height();
  if (frame_it->kind() == TranslatedFrame::kInterpretedFunction) {
    // For interpreter frames, we should not count the accumulator.
    // TODO(jarin): Clean up the indexing in translated frames.
    stack_height--;
  }
  expression_stack_.resize(static_cast<size_t>(stack_height));
  for (int i = 0; i < stack_height; i++) {
    Handle<Object> expression = GetValueForDebugger(stack_it, isolate);
    SetExpression(i, expression);
    stack_it++;
  }

  // For interpreter frame, skip the accumulator.
  if (frame_it->kind() == TranslatedFrame::kInterpretedFunction) {
    stack_it++;
  }
  CHECK(stack_it == frame_it->end());
}


Deoptimizer::DeoptInfo Deoptimizer::GetDeoptInfo(Code* code, Address pc) {
  CHECK(code->instruction_start() <= pc && pc <= code->instruction_end());
  SourcePosition last_position = SourcePosition::Unknown();
  DeoptimizeReason last_reason = DeoptimizeReason::kNoReason;
  int last_deopt_id = kNoDeoptimizationId;
  int mask = RelocInfo::ModeMask(RelocInfo::DEOPT_REASON) |
             RelocInfo::ModeMask(RelocInfo::DEOPT_ID) |
             RelocInfo::ModeMask(RelocInfo::DEOPT_SCRIPT_OFFSET) |
             RelocInfo::ModeMask(RelocInfo::DEOPT_INLINING_ID);
  for (RelocIterator it(code, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->pc() >= pc) break;
    if (info->rmode() == RelocInfo::DEOPT_SCRIPT_OFFSET) {
      int script_offset = static_cast<int>(info->data());
      it.next();
      DCHECK(it.rinfo()->rmode() == RelocInfo::DEOPT_INLINING_ID);
      int inlining_id = static_cast<int>(it.rinfo()->data());
      last_position = SourcePosition(script_offset, inlining_id);
    } else if (info->rmode() == RelocInfo::DEOPT_ID) {
      last_deopt_id = static_cast<int>(info->data());
    } else if (info->rmode() == RelocInfo::DEOPT_REASON) {
      last_reason = static_cast<DeoptimizeReason>(info->data());
    }
  }
  return DeoptInfo(last_position, last_reason, last_deopt_id);
}


// static
int Deoptimizer::ComputeSourcePositionFromBytecodeArray(
    SharedFunctionInfo* shared, BailoutId node_id) {
  DCHECK(shared->HasBytecodeArray());
  return AbstractCode::cast(shared->bytecode_array())
      ->SourcePosition(node_id.ToInt());
}

// static
TranslatedValue TranslatedValue::NewDeferredObject(TranslatedState* container,
                                                   int length,
                                                   int object_index) {
  TranslatedValue slot(container, kCapturedObject);
  slot.materialization_info_ = {object_index, length};
  return slot;
}


// static
TranslatedValue TranslatedValue::NewDuplicateObject(TranslatedState* container,
                                                    int id) {
  TranslatedValue slot(container, kDuplicatedObject);
  slot.materialization_info_ = {id, -1};
  return slot;
}


// static
TranslatedValue TranslatedValue::NewFloat(TranslatedState* container,
                                          Float32 value) {
  TranslatedValue slot(container, kFloat);
  slot.float_value_ = value;
  return slot;
}

// static
TranslatedValue TranslatedValue::NewDouble(TranslatedState* container,
                                           Float64 value) {
  TranslatedValue slot(container, kDouble);
  slot.double_value_ = value;
  return slot;
}


// static
TranslatedValue TranslatedValue::NewInt32(TranslatedState* container,
                                          int32_t value) {
  TranslatedValue slot(container, kInt32);
  slot.int32_value_ = value;
  return slot;
}


// static
TranslatedValue TranslatedValue::NewUInt32(TranslatedState* container,
                                           uint32_t value) {
  TranslatedValue slot(container, kUInt32);
  slot.uint32_value_ = value;
  return slot;
}


// static
TranslatedValue TranslatedValue::NewBool(TranslatedState* container,
                                         uint32_t value) {
  TranslatedValue slot(container, kBoolBit);
  slot.uint32_value_ = value;
  return slot;
}


// static
TranslatedValue TranslatedValue::NewTagged(TranslatedState* container,
                                           Object* literal) {
  TranslatedValue slot(container, kTagged);
  slot.raw_literal_ = literal;
  return slot;
}


// static
TranslatedValue TranslatedValue::NewInvalid(TranslatedState* container) {
  return TranslatedValue(container, kInvalid);
}


Isolate* TranslatedValue::isolate() const { return container_->isolate(); }


Object* TranslatedValue::raw_literal() const {
  DCHECK_EQ(kTagged, kind());
  return raw_literal_;
}


int32_t TranslatedValue::int32_value() const {
  DCHECK_EQ(kInt32, kind());
  return int32_value_;
}


uint32_t TranslatedValue::uint32_value() const {
  DCHECK(kind() == kUInt32 || kind() == kBoolBit);
  return uint32_value_;
}

Float32 TranslatedValue::float_value() const {
  DCHECK_EQ(kFloat, kind());
  return float_value_;
}

Float64 TranslatedValue::double_value() const {
  DCHECK_EQ(kDouble, kind());
  return double_value_;
}


int TranslatedValue::object_length() const {
  DCHECK_EQ(kind(), kCapturedObject);
  return materialization_info_.length_;
}


int TranslatedValue::object_index() const {
  DCHECK(kind() == kCapturedObject || kind() == kDuplicatedObject);
  return materialization_info_.id_;
}


Object* TranslatedValue::GetRawValue() const {
  // If we have a value, return it.
  Handle<Object> result_handle;
  if (value_.ToHandle(&result_handle)) {
    return *result_handle;
  }

  // Otherwise, do a best effort to get the value without allocation.
  switch (kind()) {
    case kTagged:
      return raw_literal();

    case kInt32: {
      bool is_smi = Smi::IsValid(int32_value());
      if (is_smi) {
        return Smi::FromInt(int32_value());
      }
      break;
    }

    case kUInt32: {
      bool is_smi = (uint32_value() <= static_cast<uintptr_t>(Smi::kMaxValue));
      if (is_smi) {
        return Smi::FromInt(static_cast<int32_t>(uint32_value()));
      }
      break;
    }

    case kBoolBit: {
      if (uint32_value() == 0) {
        return isolate()->heap()->false_value();
      } else {
        CHECK_EQ(1U, uint32_value());
        return isolate()->heap()->true_value();
      }
    }

    default:
      break;
  }

  // If we could not get the value without allocation, return the arguments
  // marker.
  return isolate()->heap()->arguments_marker();
}


Handle<Object> TranslatedValue::GetValue() {
  Handle<Object> result;
  // If we already have a value, then get it.
  if (value_.ToHandle(&result)) return result;

  // Otherwise we have to materialize.
  switch (kind()) {
    case TranslatedValue::kTagged:
    case TranslatedValue::kInt32:
    case TranslatedValue::kUInt32:
    case TranslatedValue::kBoolBit:
    case TranslatedValue::kFloat:
    case TranslatedValue::kDouble: {
      MaterializeSimple();
      return value_.ToHandleChecked();
    }

    case TranslatedValue::kCapturedObject:
    case TranslatedValue::kDuplicatedObject:
      return container_->MaterializeObjectAt(object_index());

    case TranslatedValue::kInvalid:
      FATAL("unexpected case");
      return Handle<Object>::null();
  }

  FATAL("internal error: value missing");
  return Handle<Object>::null();
}


void TranslatedValue::MaterializeSimple() {
  // If we already have materialized, return.
  if (!value_.is_null()) return;

  Object* raw_value = GetRawValue();
  if (raw_value != isolate()->heap()->arguments_marker()) {
    // We can get the value without allocation, just return it here.
    value_ = Handle<Object>(raw_value, isolate());
    return;
  }

  switch (kind()) {
    case kInt32:
      value_ = Handle<Object>(isolate()->factory()->NewNumber(int32_value()));
      return;

    case kUInt32:
      value_ = Handle<Object>(isolate()->factory()->NewNumber(uint32_value()));
      return;

    case kFloat: {
      double scalar_value = float_value().get_scalar();
      value_ = Handle<Object>(isolate()->factory()->NewNumber(scalar_value));
      return;
    }

    case kDouble: {
      double scalar_value = double_value().get_scalar();
      value_ = Handle<Object>(isolate()->factory()->NewNumber(scalar_value));
      return;
    }

    case kCapturedObject:
    case kDuplicatedObject:
    case kInvalid:
    case kTagged:
    case kBoolBit:
      FATAL("internal error: unexpected materialization.");
      break;
  }
}


bool TranslatedValue::IsMaterializedObject() const {
  switch (kind()) {
    case kCapturedObject:
    case kDuplicatedObject:
      return true;
    default:
      return false;
  }
}

bool TranslatedValue::IsMaterializableByDebugger() const {
  // At the moment, we only allow materialization of doubles.
  return (kind() == kDouble);
}

int TranslatedValue::GetChildrenCount() const {
  if (kind() == kCapturedObject) {
    return object_length();
  } else {
    return 0;
  }
}


uint32_t TranslatedState::GetUInt32Slot(Address fp, int slot_offset) {
  Address address = fp + slot_offset;
#if V8_TARGET_BIG_ENDIAN && V8_HOST_ARCH_64_BIT
  return Memory::uint32_at(address + kIntSize);
#else
  return Memory::uint32_at(address);
#endif
}

Float32 TranslatedState::GetFloatSlot(Address fp, int slot_offset) {
#if !V8_TARGET_ARCH_S390X && !V8_TARGET_ARCH_PPC64
  return Float32::FromBits(GetUInt32Slot(fp, slot_offset));
#else
  return Float32::FromBits(Memory::uint32_at(fp + slot_offset));
#endif
}

Float64 TranslatedState::GetDoubleSlot(Address fp, int slot_offset) {
  return Float64::FromBits(Memory::uint64_at(fp + slot_offset));
}

void TranslatedValue::Handlify() {
  if (kind() == kTagged) {
    value_ = Handle<Object>(raw_literal(), isolate());
    raw_literal_ = nullptr;
  }
}


TranslatedFrame TranslatedFrame::InterpretedFrame(
    BailoutId bytecode_offset, SharedFunctionInfo* shared_info, int height) {
  TranslatedFrame frame(kInterpretedFunction, shared_info, height);
  frame.node_id_ = bytecode_offset;
  return frame;
}


TranslatedFrame TranslatedFrame::ArgumentsAdaptorFrame(
    SharedFunctionInfo* shared_info, int height) {
  return TranslatedFrame(kArgumentsAdaptor, shared_info, height);
}

TranslatedFrame TranslatedFrame::ConstructStubFrame(
    BailoutId bailout_id, SharedFunctionInfo* shared_info, int height) {
  TranslatedFrame frame(kConstructStub, shared_info, height);
  frame.node_id_ = bailout_id;
  return frame;
}

TranslatedFrame TranslatedFrame::BuiltinContinuationFrame(
    BailoutId bailout_id, SharedFunctionInfo* shared_info, int height) {
  TranslatedFrame frame(kBuiltinContinuation, shared_info, height);
  frame.node_id_ = bailout_id;
  return frame;
}

TranslatedFrame TranslatedFrame::JavaScriptBuiltinContinuationFrame(
    BailoutId bailout_id, SharedFunctionInfo* shared_info, int height) {
  TranslatedFrame frame(kJavaScriptBuiltinContinuation, shared_info, height);
  frame.node_id_ = bailout_id;
  return frame;
}

int TranslatedFrame::GetValueCount() {
  switch (kind()) {
    case kInterpretedFunction: {
      int parameter_count =
          raw_shared_info_->internal_formal_parameter_count() + 1;
      // + 2 for function and context.
      return height_ + parameter_count + 2;
    }

    case kArgumentsAdaptor:
    case kConstructStub:
    case kBuiltinContinuation:
    case kJavaScriptBuiltinContinuation:
      return 1 + height_;

    case kInvalid:
      UNREACHABLE();
      break;
  }
  UNREACHABLE();
}


void TranslatedFrame::Handlify() {
  if (raw_shared_info_ != nullptr) {
    shared_info_ = Handle<SharedFunctionInfo>(raw_shared_info_);
    raw_shared_info_ = nullptr;
  }
  for (auto& value : values_) {
    value.Handlify();
  }
}


TranslatedFrame TranslatedState::CreateNextTranslatedFrame(
    TranslationIterator* iterator, FixedArray* literal_array, Address fp,
    FILE* trace_file) {
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  switch (opcode) {
    case Translation::INTERPRETED_FRAME: {
      BailoutId bytecode_offset = BailoutId(iterator->Next());
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        std::unique_ptr<char[]> name = shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading input frame %s", name.get());
        int arg_count = shared_info->internal_formal_parameter_count() + 1;
        PrintF(trace_file,
               " => bytecode_offset=%d, args=%d, height=%d; inputs:\n",
               bytecode_offset.ToInt(), arg_count, height);
      }
      return TranslatedFrame::InterpretedFrame(bytecode_offset, shared_info,
                                               height);
    }

    case Translation::ARGUMENTS_ADAPTOR_FRAME: {
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        std::unique_ptr<char[]> name = shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading arguments adaptor frame %s", name.get());
        PrintF(trace_file, " => height=%d; inputs:\n", height);
      }
      return TranslatedFrame::ArgumentsAdaptorFrame(shared_info, height);
    }

    case Translation::CONSTRUCT_STUB_FRAME: {
      BailoutId bailout_id = BailoutId(iterator->Next());
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        std::unique_ptr<char[]> name = shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading construct stub frame %s", name.get());
        PrintF(trace_file, " => bailout_id=%d, height=%d; inputs:\n",
               bailout_id.ToInt(), height);
      }
      return TranslatedFrame::ConstructStubFrame(bailout_id, shared_info,
                                                 height);
    }

    case Translation::BUILTIN_CONTINUATION_FRAME: {
      BailoutId bailout_id = BailoutId(iterator->Next());
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        std::unique_ptr<char[]> name = shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading builtin continuation frame %s",
               name.get());
        PrintF(trace_file, " => bailout_id=%d, height=%d; inputs:\n",
               bailout_id.ToInt(), height);
      }
      // Add one to the height to account for the context which was implicitly
      // added to the translation during code generation.
      int height_with_context = height + 1;
      return TranslatedFrame::BuiltinContinuationFrame(bailout_id, shared_info,
                                                       height_with_context);
    }

    case Translation::JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME: {
      BailoutId bailout_id = BailoutId(iterator->Next());
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        std::unique_ptr<char[]> name = shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading JavaScript builtin continuation frame %s",
               name.get());
        PrintF(trace_file, " => bailout_id=%d, height=%d; inputs:\n",
               bailout_id.ToInt(), height);
      }
      // Add one to the height to account for the context which was implicitly
      // added to the translation during code generation.
      int height_with_context = height + 1;
      return TranslatedFrame::JavaScriptBuiltinContinuationFrame(
          bailout_id, shared_info, height_with_context);
    }

    case Translation::BEGIN:
    case Translation::DUPLICATED_OBJECT:
    case Translation::ARGUMENTS_ELEMENTS:
    case Translation::ARGUMENTS_LENGTH:
    case Translation::CAPTURED_OBJECT:
    case Translation::REGISTER:
    case Translation::INT32_REGISTER:
    case Translation::UINT32_REGISTER:
    case Translation::BOOL_REGISTER:
    case Translation::FLOAT_REGISTER:
    case Translation::DOUBLE_REGISTER:
    case Translation::STACK_SLOT:
    case Translation::INT32_STACK_SLOT:
    case Translation::UINT32_STACK_SLOT:
    case Translation::BOOL_STACK_SLOT:
    case Translation::FLOAT_STACK_SLOT:
    case Translation::DOUBLE_STACK_SLOT:
    case Translation::LITERAL:
      break;
  }
  FATAL("We should never get here - unexpected deopt info.");
  return TranslatedFrame::InvalidFrame();
}


// static
void TranslatedFrame::AdvanceIterator(
    std::deque<TranslatedValue>::iterator* iter) {
  int values_to_skip = 1;
  while (values_to_skip > 0) {
    // Consume the current element.
    values_to_skip--;
    // Add all the children.
    values_to_skip += (*iter)->GetChildrenCount();

    (*iter)++;
  }
}

Address TranslatedState::ComputeArgumentsPosition(Address input_frame_pointer,
                                                  CreateArgumentsType type,
                                                  int* length) {
  Address parent_frame_pointer = *reinterpret_cast<Address*>(
      input_frame_pointer + StandardFrameConstants::kCallerFPOffset);
  intptr_t parent_frame_type = Memory::intptr_at(
      parent_frame_pointer + CommonFrameConstants::kContextOrFrameTypeOffset);

  Address arguments_frame;
  if (parent_frame_type ==
      StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)) {
    if (length)
      *length = Smi::cast(*reinterpret_cast<Object**>(
                              parent_frame_pointer +
                              ArgumentsAdaptorFrameConstants::kLengthOffset))
                    ->value();
    arguments_frame = parent_frame_pointer;
  } else {
    if (length) *length = formal_parameter_count_;
    arguments_frame = input_frame_pointer;
  }

  if (type == CreateArgumentsType::kRestParameter) {
    // If the actual number of arguments is less than the number of formal
    // parameters, we have zero rest parameters.
    if (length) *length = std::max(0, *length - formal_parameter_count_);
  }

  return arguments_frame;
}

// Creates translated values for an arguments backing store, or the backing
// store for rest parameters depending on the given {type}. The TranslatedValue
// objects for the fields are not read from the TranslationIterator, but instead
// created on-the-fly based on dynamic information in the optimized frame.
void TranslatedState::CreateArgumentsElementsTranslatedValues(
    int frame_index, Address input_frame_pointer, CreateArgumentsType type,
    FILE* trace_file) {
  TranslatedFrame& frame = frames_[frame_index];

  int length;
  Address arguments_frame =
      ComputeArgumentsPosition(input_frame_pointer, type, &length);

  int object_index = static_cast<int>(object_positions_.size());
  int value_index = static_cast<int>(frame.values_.size());
  if (trace_file != nullptr) {
    PrintF(trace_file, "arguments elements object #%d (type = %d, length = %d)",
           object_index, static_cast<uint8_t>(type), length);
  }
  object_positions_.push_back({frame_index, value_index});
  frame.Add(TranslatedValue::NewDeferredObject(
      this, length + FixedArray::kHeaderSize / kPointerSize, object_index));

  frame.Add(
      TranslatedValue::NewTagged(this, isolate_->heap()->fixed_array_map()));
  frame.Add(TranslatedValue::NewInt32(this, length));

  int number_of_holes = 0;
  if (type == CreateArgumentsType::kMappedArguments) {
    // If the actual number of arguments is less than the number of formal
    // parameters, we have fewer holes to fill to not overshoot the length.
    number_of_holes = Min(formal_parameter_count_, length);
  }
  for (int i = 0; i < number_of_holes; ++i) {
    frame.Add(
        TranslatedValue::NewTagged(this, isolate_->heap()->the_hole_value()));
  }
  for (int i = length - number_of_holes - 1; i >= 0; --i) {
    Address argument_slot = arguments_frame +
                            CommonFrameConstants::kFixedFrameSizeAboveFp +
                            i * kPointerSize;
    frame.Add(TranslatedValue::NewTagged(
        this, *reinterpret_cast<Object**>(argument_slot)));
  }
}

// We can't intermix stack decoding and allocations because the deoptimization
// infrastracture is not GC safe.
// Thus we build a temporary structure in malloced space.
// The TranslatedValue objects created correspond to the static translation
// instructions from the TranslationIterator, except for
// Translation::ARGUMENTS_ELEMENTS, where the number and values of the
// FixedArray elements depend on dynamic information from the optimized frame.
// Returns the number of expected nested translations from the
// TranslationIterator.
int TranslatedState::CreateNextTranslatedValue(
    int frame_index, TranslationIterator* iterator, FixedArray* literal_array,
    Address fp, RegisterValues* registers, FILE* trace_file) {
  disasm::NameConverter converter;

  TranslatedFrame& frame = frames_[frame_index];
  int value_index = static_cast<int>(frame.values_.size());

  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  switch (opcode) {
    case Translation::BEGIN:
    case Translation::INTERPRETED_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME:
    case Translation::BUILTIN_CONTINUATION_FRAME:
      // Peeled off before getting here.
      break;

    case Translation::DUPLICATED_OBJECT: {
      int object_id = iterator->Next();
      if (trace_file != nullptr) {
        PrintF(trace_file, "duplicated object #%d", object_id);
      }
      object_positions_.push_back(object_positions_[object_id]);
      TranslatedValue translated_value =
          TranslatedValue::NewDuplicateObject(this, object_id);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::ARGUMENTS_ELEMENTS: {
      CreateArgumentsType arguments_type =
          static_cast<CreateArgumentsType>(iterator->Next());
      CreateArgumentsElementsTranslatedValues(frame_index, fp, arguments_type,
                                              trace_file);
      return 0;
    }

    case Translation::ARGUMENTS_LENGTH: {
      CreateArgumentsType arguments_type =
          static_cast<CreateArgumentsType>(iterator->Next());
      int length;
      ComputeArgumentsPosition(fp, arguments_type, &length);
      if (trace_file != nullptr) {
        PrintF(trace_file, "arguments length field (type = %d, length = %d)",
               static_cast<uint8_t>(arguments_type), length);
      }
      frame.Add(TranslatedValue::NewInt32(this, length));
      return 0;
    }

    case Translation::CAPTURED_OBJECT: {
      int field_count = iterator->Next();
      int object_index = static_cast<int>(object_positions_.size());
      if (trace_file != nullptr) {
        PrintF(trace_file, "captured object #%d (length = %d)", object_index,
               field_count);
      }
      object_positions_.push_back({frame_index, value_index});
      TranslatedValue translated_value =
          TranslatedValue::NewDeferredObject(this, field_count, object_index);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) {
        TranslatedValue translated_value = TranslatedValue::NewInvalid(this);
        frame.Add(translated_value);
        return translated_value.GetChildrenCount();
      }
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "0x%08" V8PRIxPTR " ; %s ", value,
               converter.NameOfCPURegister(input_reg));
        reinterpret_cast<Object*>(value)->ShortPrint(trace_file);
      }
      TranslatedValue translated_value =
          TranslatedValue::NewTagged(this, reinterpret_cast<Object*>(value));
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::INT32_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) {
        TranslatedValue translated_value = TranslatedValue::NewInvalid(this);
        frame.Add(translated_value);
        return translated_value.GetChildrenCount();
      }
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%" V8PRIdPTR " ; %s ", value,
               converter.NameOfCPURegister(input_reg));
      }
      TranslatedValue translated_value =
          TranslatedValue::NewInt32(this, static_cast<int32_t>(value));
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::UINT32_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) {
        TranslatedValue translated_value = TranslatedValue::NewInvalid(this);
        frame.Add(translated_value);
        return translated_value.GetChildrenCount();
      }
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%" V8PRIuPTR " ; %s (uint)", value,
               converter.NameOfCPURegister(input_reg));
      }
      TranslatedValue translated_value =
          TranslatedValue::NewUInt32(this, static_cast<uint32_t>(value));
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::BOOL_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) {
        TranslatedValue translated_value = TranslatedValue::NewInvalid(this);
        frame.Add(translated_value);
        return translated_value.GetChildrenCount();
      }
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%" V8PRIdPTR " ; %s (bool)", value,
               converter.NameOfCPURegister(input_reg));
      }
      TranslatedValue translated_value =
          TranslatedValue::NewBool(this, static_cast<uint32_t>(value));
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::FLOAT_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) {
        TranslatedValue translated_value = TranslatedValue::NewInvalid(this);
        frame.Add(translated_value);
        return translated_value.GetChildrenCount();
      }
      Float32 value = registers->GetFloatRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(
            trace_file, "%e ; %s (float)", value.get_scalar(),
            RegisterConfiguration::Default()->GetFloatRegisterName(input_reg));
      }
      TranslatedValue translated_value = TranslatedValue::NewFloat(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::DOUBLE_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) {
        TranslatedValue translated_value = TranslatedValue::NewInvalid(this);
        frame.Add(translated_value);
        return translated_value.GetChildrenCount();
      }
      Float64 value = registers->GetDoubleRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(
            trace_file, "%e ; %s (double)", value.get_scalar(),
            RegisterConfiguration::Default()->GetDoubleRegisterName(input_reg));
      }
      TranslatedValue translated_value =
          TranslatedValue::NewDouble(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      intptr_t value = *(reinterpret_cast<intptr_t*>(fp + slot_offset));
      if (trace_file != nullptr) {
        PrintF(trace_file, "0x%08" V8PRIxPTR " ; [fp %c %d] ", value,
               slot_offset < 0 ? '-' : '+', std::abs(slot_offset));
        reinterpret_cast<Object*>(value)->ShortPrint(trace_file);
      }
      TranslatedValue translated_value =
          TranslatedValue::NewTagged(this, reinterpret_cast<Object*>(value));
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::INT32_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      uint32_t value = GetUInt32Slot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%d ; (int) [fp %c %d] ",
               static_cast<int32_t>(value), slot_offset < 0 ? '-' : '+',
               std::abs(slot_offset));
      }
      TranslatedValue translated_value = TranslatedValue::NewInt32(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::UINT32_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      uint32_t value = GetUInt32Slot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%u ; (uint) [fp %c %d] ", value,
               slot_offset < 0 ? '-' : '+', std::abs(slot_offset));
      }
      TranslatedValue translated_value =
          TranslatedValue::NewUInt32(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::BOOL_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      uint32_t value = GetUInt32Slot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%u ; (bool) [fp %c %d] ", value,
               slot_offset < 0 ? '-' : '+', std::abs(slot_offset));
      }
      TranslatedValue translated_value = TranslatedValue::NewBool(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::FLOAT_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      Float32 value = GetFloatSlot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%e ; (float) [fp %c %d] ", value.get_scalar(),
               slot_offset < 0 ? '-' : '+', std::abs(slot_offset));
      }
      TranslatedValue translated_value = TranslatedValue::NewFloat(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::DOUBLE_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      Float64 value = GetDoubleSlot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%e ; (double) [fp %c %d] ", value.get_scalar(),
               slot_offset < 0 ? '-' : '+', std::abs(slot_offset));
      }
      TranslatedValue translated_value =
          TranslatedValue::NewDouble(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::LITERAL: {
      int literal_index = iterator->Next();
      Object* value = literal_array->get(literal_index);
      if (trace_file != nullptr) {
        PrintF(trace_file, "0x%08" V8PRIxPTR " ; (literal %d) ",
               reinterpret_cast<intptr_t>(value), literal_index);
        reinterpret_cast<Object*>(value)->ShortPrint(trace_file);
      }

      TranslatedValue translated_value =
          TranslatedValue::NewTagged(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }
  }

  FATAL("We should never get here - unexpected deopt info.");
}

TranslatedState::TranslatedState(const JavaScriptFrame* frame)
    : isolate_(nullptr), stack_frame_pointer_(nullptr) {
  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationData* data =
      static_cast<const OptimizedFrame*>(frame)->GetDeoptimizationData(
          &deopt_index);
  DCHECK(data != nullptr && deopt_index != Safepoint::kNoDeoptimizationIndex);
  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Init(frame->fp(), &it, data->LiteralArray(), nullptr /* registers */,
       nullptr /* trace file */,
       frame->function()->shared()->internal_formal_parameter_count());
}

TranslatedState::TranslatedState()
    : isolate_(nullptr), stack_frame_pointer_(nullptr) {}

void TranslatedState::Init(Address input_frame_pointer,
                           TranslationIterator* iterator,
                           FixedArray* literal_array, RegisterValues* registers,
                           FILE* trace_file, int formal_parameter_count) {
  DCHECK(frames_.empty());

  formal_parameter_count_ = formal_parameter_count;

  isolate_ = literal_array->GetIsolate();
  // Read out the 'header' translation.
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  CHECK(opcode == Translation::BEGIN);

  int count = iterator->Next();
  iterator->Next();  // Drop JS frames count.

  frames_.reserve(count);

  std::stack<int> nested_counts;

  // Read the frames
  for (int frame_index = 0; frame_index < count; frame_index++) {
    // Read the frame descriptor.
    frames_.push_back(CreateNextTranslatedFrame(
        iterator, literal_array, input_frame_pointer, trace_file));
    TranslatedFrame& frame = frames_.back();

    // Read the values.
    int values_to_process = frame.GetValueCount();
    while (values_to_process > 0 || !nested_counts.empty()) {
      if (trace_file != nullptr) {
        if (nested_counts.empty()) {
          // For top level values, print the value number.
          PrintF(trace_file, "    %3i: ",
                 frame.GetValueCount() - values_to_process);
        } else {
          // Take care of indenting for nested values.
          PrintF(trace_file, "         ");
          for (size_t j = 0; j < nested_counts.size(); j++) {
            PrintF(trace_file, "  ");
          }
        }
      }

      int nested_count =
          CreateNextTranslatedValue(frame_index, iterator, literal_array,
                                    input_frame_pointer, registers, trace_file);

      if (trace_file != nullptr) {
        PrintF(trace_file, "\n");
      }

      // Update the value count and resolve the nesting.
      values_to_process--;
      if (nested_count > 0) {
        nested_counts.push(values_to_process);
        values_to_process = nested_count;
      } else {
        while (values_to_process == 0 && !nested_counts.empty()) {
          values_to_process = nested_counts.top();
          nested_counts.pop();
        }
      }
    }
  }

  CHECK(!iterator->HasNext() ||
        static_cast<Translation::Opcode>(iterator->Next()) ==
            Translation::BEGIN);
}

void TranslatedState::Prepare(Address stack_frame_pointer) {
  for (auto& frame : frames_) frame.Handlify();

  stack_frame_pointer_ = stack_frame_pointer;

  UpdateFromPreviouslyMaterializedObjects();
}

class TranslatedState::CapturedObjectMaterializer {
 public:
  CapturedObjectMaterializer(TranslatedState* state, int frame_index,
                             int field_count)
      : state_(state), frame_index_(frame_index), field_count_(field_count) {}

  // Ensure the properties never contain mutable heap numbers. This is necessary
  // because the deoptimizer generalizes all maps to tagged representation
  // fields (so mutable heap numbers are not allowed).
  static void EnsurePropertiesGeneralized(Handle<Object> properties_or_hash) {
    if (properties_or_hash->IsPropertyArray()) {
      Handle<PropertyArray> properties =
          Handle<PropertyArray>::cast(properties_or_hash);
      int length = properties->length();
      for (int i = 0; i < length; i++) {
        if (properties->get(i)->IsMutableHeapNumber()) {
          Handle<HeapObject> box(HeapObject::cast(properties->get(i)));
          box->set_map(properties->GetIsolate()->heap()->heap_number_map());
        }
      }
    }
  }

  Handle<Object> FieldAt(int* value_index) {
    CHECK_GT(field_count_, 0);
    --field_count_;
    Handle<Object> object = state_->MaterializeAt(frame_index_, value_index);
    // This is a big hammer to make sure that the materialized objects do not
    // have property arrays with mutable heap numbers (mutable heap numbers are
    // bad because we generalize maps for all materialized objects).
    EnsurePropertiesGeneralized(object);
    return object;
  }

  ~CapturedObjectMaterializer() { CHECK_EQ(0, field_count_); }

 private:
  TranslatedState* state_;
  int frame_index_;
  int field_count_;
};

Handle<Object> TranslatedState::MaterializeCapturedObjectAt(
    TranslatedValue* slot, int frame_index, int* value_index) {
  int length = slot->GetChildrenCount();

  CapturedObjectMaterializer materializer(this, frame_index, length);

  Handle<Object> result;
  if (slot->value_.ToHandle(&result)) {
    // This has been previously materialized, return the previous value.
    // We still need to skip all the nested objects.
    for (int i = 0; i < length; i++) {
      materializer.FieldAt(value_index);
    }

    return result;
  }

  Handle<Object> map_object = materializer.FieldAt(value_index);
  Handle<Map> map = Map::GeneralizeAllFields(Handle<Map>::cast(map_object));
  switch (map->instance_type()) {
    case MUTABLE_HEAP_NUMBER_TYPE:
    case HEAP_NUMBER_TYPE: {
      // Reuse the HeapNumber value directly as it is already properly
      // tagged and skip materializing the HeapNumber explicitly.
      Handle<Object> object = materializer.FieldAt(value_index);
      slot->value_ = object;
      // On 32-bit architectures, there is an extra slot there because
      // the escape analysis calculates the number of slots as
      // object-size/pointer-size. To account for this, we read out
      // any extra slots.
      for (int i = 0; i < length - 2; i++) {
        materializer.FieldAt(value_index);
      }
      return object;
    }
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_ARGUMENTS_TYPE: {
      Handle<JSObject> object =
          isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED);
      slot->value_ = object;
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      int in_object_properties = map->GetInObjectProperties();
      for (int i = 0; i < in_object_properties; ++i) {
        Handle<Object> value = materializer.FieldAt(value_index);
        FieldIndex index = FieldIndex::ForPropertyIndex(object->map(), i);
        object->FastPropertyAtPut(index, *value);
      }
      return object;
    }
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE: {
      Handle<JSSetIterator> object = Handle<JSSetIterator>::cast(
          isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
      slot->value_ = object;
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> table = materializer.FieldAt(value_index);
      Handle<Object> index = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      object->set_table(*table);
      object->set_index(*index);
      return object;
    }
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE: {
      Handle<JSMapIterator> object = Handle<JSMapIterator>::cast(
          isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
      slot->value_ = object;
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> table = materializer.FieldAt(value_index);
      Handle<Object> index = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      object->set_table(*table);
      object->set_index(*index);
      return object;
    }
#define ARRAY_ITERATOR_CASE(type) case type:
      ARRAY_ITERATOR_TYPE_LIST(ARRAY_ITERATOR_CASE)
#undef ARRAY_ITERATOR_CASE
      {
        Handle<JSArrayIterator> object = Handle<JSArrayIterator>::cast(
            isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
        slot->value_ = object;
        // Initialize the index to zero to make the heap verifier happy.
        object->set_index(Smi::FromInt(0));
        Handle<Object> properties = materializer.FieldAt(value_index);
        Handle<Object> elements = materializer.FieldAt(value_index);
        Handle<Object> iterated_object = materializer.FieldAt(value_index);
        Handle<Object> next_index = materializer.FieldAt(value_index);
        Handle<Object> iterated_object_map = materializer.FieldAt(value_index);
        object->set_raw_properties_or_hash(*properties);
        object->set_elements(FixedArrayBase::cast(*elements));
        object->set_object(*iterated_object);
        object->set_index(*next_index);
        object->set_object_map(*iterated_object_map);
        return object;
      }
    case JS_STRING_ITERATOR_TYPE: {
      Handle<JSStringIterator> object = Handle<JSStringIterator>::cast(
          isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
      slot->value_ = object;
      // Initialize the index to zero to make the heap verifier happy.
      object->set_index(0);
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> iterated_string = materializer.FieldAt(value_index);
      Handle<Object> next_index = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      CHECK(iterated_string->IsString());
      object->set_string(String::cast(*iterated_string));
      CHECK(next_index->IsSmi());
      object->set_index(Smi::ToInt(*next_index));
      return object;
    }
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE: {
      Handle<JSAsyncFromSyncIterator> object =
          Handle<JSAsyncFromSyncIterator>::cast(
              isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
      slot->value_ = object;
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> sync_iterator = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      object->set_sync_iterator(JSReceiver::cast(*sync_iterator));
      return object;
    }
    case JS_ARRAY_TYPE: {
      Handle<JSArray> object = Handle<JSArray>::cast(
          isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
      slot->value_ = object;
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> array_length = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      object->set_length(*array_length);
      int in_object_properties = map->GetInObjectProperties();
      for (int i = 0; i < in_object_properties; ++i) {
        Handle<Object> value = materializer.FieldAt(value_index);
        FieldIndex index = FieldIndex::ForPropertyIndex(object->map(), i);
        object->FastPropertyAtPut(index, *value);
      }
      return object;
    }
    case JS_BOUND_FUNCTION_TYPE: {
      Handle<JSBoundFunction> object = Handle<JSBoundFunction>::cast(
          isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
      slot->value_ = object;
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> bound_target_function = materializer.FieldAt(value_index);
      Handle<Object> bound_this = materializer.FieldAt(value_index);
      Handle<Object> bound_arguments = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      object->set_bound_target_function(
          JSReceiver::cast(*bound_target_function));
      object->set_bound_this(*bound_this);
      object->set_bound_arguments(FixedArray::cast(*bound_arguments));
      return object;
    }
    case JS_FUNCTION_TYPE: {
      Handle<JSFunction> object = isolate_->factory()->NewFunction(
          map, handle(isolate_->object_function()->shared()),
          handle(isolate_->context()), NOT_TENURED);
      slot->value_ = object;
      // We temporarily allocated a JSFunction for the {Object} function
      // within the current context, to break cycles in the object graph.
      // The correct function and context will be set below once available.
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> shared = materializer.FieldAt(value_index);
      Handle<Object> context = materializer.FieldAt(value_index);
      Handle<Object> vector_cell = materializer.FieldAt(value_index);
      Handle<Object> code = materializer.FieldAt(value_index);
      bool has_prototype_slot = map->has_prototype_slot();
      Handle<Object> prototype;
      if (has_prototype_slot) {
        prototype = materializer.FieldAt(value_index);
      }
      object->set_map(*map);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      object->set_shared(SharedFunctionInfo::cast(*shared));
      object->set_context(Context::cast(*context));
      object->set_feedback_vector_cell(Cell::cast(*vector_cell));
      object->set_code(Code::cast(*code));
      if (has_prototype_slot) {
        object->set_prototype_or_initial_map(*prototype);
      }
      int in_object_properties = map->GetInObjectProperties();
      for (int i = 0; i < in_object_properties; ++i) {
        Handle<Object> value = materializer.FieldAt(value_index);
        FieldIndex index = FieldIndex::ForPropertyIndex(object->map(), i);
        object->FastPropertyAtPut(index, *value);
      }
      return object;
    }
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE: {
      Handle<JSGeneratorObject> object = Handle<JSGeneratorObject>::cast(
          isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
      slot->value_ = object;
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> function = materializer.FieldAt(value_index);
      Handle<Object> context = materializer.FieldAt(value_index);
      Handle<Object> receiver = materializer.FieldAt(value_index);
      Handle<Object> input_or_debug_pos = materializer.FieldAt(value_index);
      Handle<Object> resume_mode = materializer.FieldAt(value_index);
      Handle<Object> continuation_offset = materializer.FieldAt(value_index);
      Handle<Object> register_file = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      object->set_function(JSFunction::cast(*function));
      object->set_context(Context::cast(*context));
      object->set_receiver(*receiver);
      object->set_input_or_debug_pos(*input_or_debug_pos);
      object->set_resume_mode(Smi::ToInt(*resume_mode));
      object->set_continuation(Smi::ToInt(*continuation_offset));
      object->set_register_file(FixedArray::cast(*register_file));

      if (object->IsJSAsyncGeneratorObject()) {
        auto generator = Handle<JSAsyncGeneratorObject>::cast(object);
        Handle<Object> queue = materializer.FieldAt(value_index);
        Handle<Object> awaited_promise = materializer.FieldAt(value_index);
        generator->set_queue(HeapObject::cast(*queue));
        generator->set_awaited_promise(HeapObject::cast(*awaited_promise));
      }

      int in_object_properties = map->GetInObjectProperties();
      for (int i = 0; i < in_object_properties; ++i) {
        Handle<Object> value = materializer.FieldAt(value_index);
        FieldIndex index = FieldIndex::ForPropertyIndex(object->map(), i);
        object->FastPropertyAtPut(index, *value);
      }
      return object;
    }
    case CONS_STRING_TYPE: {
      Handle<ConsString> object = Handle<ConsString>::cast(
          isolate_->factory()
              ->NewConsString(isolate_->factory()->undefined_string(),
                              isolate_->factory()->undefined_string())
              .ToHandleChecked());
      slot->value_ = object;
      Handle<Object> hash = materializer.FieldAt(value_index);
      Handle<Object> string_length = materializer.FieldAt(value_index);
      Handle<Object> first = materializer.FieldAt(value_index);
      Handle<Object> second = materializer.FieldAt(value_index);
      object->set_map(*map);
      object->set_length(Smi::ToInt(*string_length));
      object->set_first(String::cast(*first));
      object->set_second(String::cast(*second));
      CHECK(hash->IsNumber());  // The {Name::kEmptyHashField} value.
      return object;
    }
    case CONTEXT_EXTENSION_TYPE: {
      Handle<ContextExtension> object =
          isolate_->factory()->NewContextExtension(
              isolate_->factory()->NewScopeInfo(1),
              isolate_->factory()->undefined_value());
      slot->value_ = object;
      Handle<Object> scope_info = materializer.FieldAt(value_index);
      Handle<Object> extension = materializer.FieldAt(value_index);
      object->set_scope_info(ScopeInfo::cast(*scope_info));
      object->set_extension(*extension);
      return object;
    }
    case HASH_TABLE_TYPE:
    case FIXED_ARRAY_TYPE: {
      Handle<Object> lengthObject = materializer.FieldAt(value_index);
      int32_t array_length = 0;
      CHECK(lengthObject->ToInt32(&array_length));
      Handle<FixedArray> object =
          isolate_->factory()->NewFixedArray(array_length);
      // We need to set the map, because the fixed array we are
      // materializing could be a context or an arguments object,
      // in which case we must retain that information.
      object->set_map(*map);
      slot->value_ = object;
      for (int i = 0; i < array_length; ++i) {
        Handle<Object> value = materializer.FieldAt(value_index);
        object->set(i, *value);
      }
      return object;
    }
    case PROPERTY_ARRAY_TYPE: {
      DCHECK_EQ(*map, isolate_->heap()->property_array_map());
      Handle<Object> lengthObject = materializer.FieldAt(value_index);
      int32_t array_length = 0;
      CHECK(lengthObject->ToInt32(&array_length));
      Handle<PropertyArray> object =
          isolate_->factory()->NewPropertyArray(array_length);
      slot->value_ = object;
      for (int i = 0; i < array_length; ++i) {
        Handle<Object> value = materializer.FieldAt(value_index);
        object->set(i, *value);
      }
      return object;
    }
    case FIXED_DOUBLE_ARRAY_TYPE: {
      DCHECK_EQ(*map, isolate_->heap()->fixed_double_array_map());
      Handle<Object> lengthObject = materializer.FieldAt(value_index);
      int32_t array_length = 0;
      CHECK(lengthObject->ToInt32(&array_length));
      Handle<FixedArrayBase> object =
          isolate_->factory()->NewFixedDoubleArray(array_length);
      slot->value_ = object;
      if (array_length > 0) {
        Handle<FixedDoubleArray> double_array =
            Handle<FixedDoubleArray>::cast(object);
        for (int i = 0; i < array_length; ++i) {
          Handle<Object> value = materializer.FieldAt(value_index);
          if (value.is_identical_to(isolate_->factory()->the_hole_value())) {
            double_array->set_the_hole(isolate_, i);
          } else {
            CHECK(value->IsNumber());
            double_array->set(i, value->Number());
          }
        }
      }
      return object;
    }
    case JS_REGEXP_TYPE: {
      Handle<JSRegExp> object = Handle<JSRegExp>::cast(
          isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED));
      slot->value_ = object;
      Handle<Object> properties = materializer.FieldAt(value_index);
      Handle<Object> elements = materializer.FieldAt(value_index);
      Handle<Object> data = materializer.FieldAt(value_index);
      Handle<Object> source = materializer.FieldAt(value_index);
      Handle<Object> flags = materializer.FieldAt(value_index);
      Handle<Object> last_index = materializer.FieldAt(value_index);
      object->set_raw_properties_or_hash(*properties);
      object->set_elements(FixedArrayBase::cast(*elements));
      object->set_data(*data);
      object->set_source(*source);
      object->set_flags(*flags);
      object->set_last_index(*last_index);
      return object;
    }
    case STRING_TYPE:
    case ONE_BYTE_STRING_TYPE:
    case CONS_ONE_BYTE_STRING_TYPE:
    case SLICED_STRING_TYPE:
    case SLICED_ONE_BYTE_STRING_TYPE:
    case EXTERNAL_STRING_TYPE:
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
    case EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_STRING_TYPE:
    case SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE:
    case SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case THIN_STRING_TYPE:
    case THIN_ONE_BYTE_STRING_TYPE:
    case INTERNALIZED_STRING_TYPE:
    case ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SYMBOL_TYPE:
    case ODDBALL_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_VALUE_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_DATE_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_MODULE_NAMESPACE_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
    case JS_PROMISE_TYPE:
    case JS_PROXY_TYPE:
    case MAP_TYPE:
    case ALLOCATION_SITE_TYPE:
    case ACCESSOR_INFO_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:
    case FUNCTION_TEMPLATE_INFO_TYPE:
    case ACCESSOR_PAIR_TYPE:
    case BYTE_ARRAY_TYPE:
    case BYTECODE_ARRAY_TYPE:
    case DESCRIPTOR_ARRAY_TYPE:
    case TRANSITION_ARRAY_TYPE:
    case FEEDBACK_VECTOR_TYPE:
    case FOREIGN_TYPE:
    case SCRIPT_TYPE:
    case CODE_TYPE:
    case PROPERTY_CELL_TYPE:
    case BIGINT_TYPE:
    case MODULE_TYPE:
    case MODULE_INFO_ENTRY_TYPE:
    case FREE_SPACE_TYPE:
#define FIXED_TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:
      TYPED_ARRAYS(FIXED_TYPED_ARRAY_CASE)
#undef FIXED_TYPED_ARRAY_CASE
    case FILLER_TYPE:
    case ACCESS_CHECK_INFO_TYPE:
    case INTERCEPTOR_INFO_TYPE:
    case OBJECT_TEMPLATE_INFO_TYPE:
    case ALLOCATION_MEMENTO_TYPE:
    case ALIASED_ARGUMENTS_ENTRY_TYPE:
    case PROMISE_RESOLVE_THENABLE_JOB_INFO_TYPE:
    case PROMISE_REACTION_JOB_INFO_TYPE:
    case DEBUG_INFO_TYPE:
    case STACK_FRAME_INFO_TYPE:
    case CELL_TYPE:
    case WEAK_CELL_TYPE:
    case SMALL_ORDERED_HASH_MAP_TYPE:
    case SMALL_ORDERED_HASH_SET_TYPE:
    case CODE_DATA_CONTAINER_TYPE:
    case PROTOTYPE_INFO_TYPE:
    case TUPLE2_TYPE:
    case TUPLE3_TYPE:
    case ASYNC_GENERATOR_REQUEST_TYPE:
    case WASM_MODULE_TYPE:
    case WASM_INSTANCE_TYPE:
    case WASM_MEMORY_TYPE:
    case WASM_TABLE_TYPE:
      OFStream os(stderr);
      os << "[couldn't handle instance type " << map->instance_type() << "]"
         << std::endl;
      UNREACHABLE();
      break;
  }
  UNREACHABLE();
}

Handle<Object> TranslatedState::MaterializeAt(int frame_index,
                                              int* value_index) {
  CHECK_LT(static_cast<size_t>(frame_index), frames().size());
  TranslatedFrame* frame = &(frames_[frame_index]);
  CHECK_LT(static_cast<size_t>(*value_index), frame->values_.size());

  TranslatedValue* slot = &(frame->values_[*value_index]);
  (*value_index)++;

  switch (slot->kind()) {
    case TranslatedValue::kTagged:
    case TranslatedValue::kInt32:
    case TranslatedValue::kUInt32:
    case TranslatedValue::kBoolBit:
    case TranslatedValue::kFloat:
    case TranslatedValue::kDouble: {
      slot->MaterializeSimple();
      Handle<Object> value = slot->GetValue();
      if (value->IsMutableHeapNumber()) {
        HeapNumber::cast(*value)->set_map(isolate()->heap()->heap_number_map());
      }
      return value;
    }

    case TranslatedValue::kCapturedObject: {
      // The map must be a tagged object.
      CHECK_EQ(frame->values_[*value_index].kind(), TranslatedValue::kTagged);
      CHECK(frame->values_[*value_index].GetValue()->IsMap());
      return MaterializeCapturedObjectAt(slot, frame_index, value_index);
    }
    case TranslatedValue::kDuplicatedObject: {
      int object_index = slot->object_index();
      TranslatedState::ObjectPosition pos = object_positions_[object_index];

      // Make sure the duplicate is referring to a previous object.
      CHECK(pos.frame_index_ < frame_index ||
            (pos.frame_index_ == frame_index &&
             pos.value_index_ < *value_index - 1));

      Handle<Object> object =
          frames_[pos.frame_index_].values_[pos.value_index_].GetValue();

      // The object should have a (non-sentinel) value.
      CHECK(!object.is_null() &&
            !object.is_identical_to(isolate_->factory()->arguments_marker()));

      slot->value_ = object;
      return object;
    }

    case TranslatedValue::kInvalid:
      UNREACHABLE();
      break;
  }

  FATAL("We should never get here - unexpected deopt slot kind.");
  return Handle<Object>::null();
}

Handle<Object> TranslatedState::MaterializeObjectAt(int object_index) {
  CHECK_LT(static_cast<size_t>(object_index), object_positions_.size());
  TranslatedState::ObjectPosition pos = object_positions_[object_index];
  return MaterializeAt(pos.frame_index_, &(pos.value_index_));
}

TranslatedFrame* TranslatedState::GetFrameFromJSFrameIndex(int jsframe_index) {
  for (size_t i = 0; i < frames_.size(); i++) {
    if (frames_[i].kind() == TranslatedFrame::kInterpretedFunction ||
        frames_[i].kind() == TranslatedFrame::kJavaScriptBuiltinContinuation) {
      if (jsframe_index > 0) {
        jsframe_index--;
      } else {
        return &(frames_[i]);
      }
    }
  }
  return nullptr;
}

TranslatedFrame* TranslatedState::GetArgumentsInfoFromJSFrameIndex(
    int jsframe_index, int* args_count) {
  for (size_t i = 0; i < frames_.size(); i++) {
    if (frames_[i].kind() == TranslatedFrame::kInterpretedFunction ||
        frames_[i].kind() == TranslatedFrame::kJavaScriptBuiltinContinuation) {
      if (jsframe_index > 0) {
        jsframe_index--;
      } else {
        // We have the JS function frame, now check if it has arguments
        // adaptor.
        if (i > 0 &&
            frames_[i - 1].kind() == TranslatedFrame::kArgumentsAdaptor) {
          *args_count = frames_[i - 1].height();
          return &(frames_[i - 1]);
        }
        *args_count =
            frames_[i].shared_info()->internal_formal_parameter_count() + 1;
        return &(frames_[i]);
      }
    }
  }
  return nullptr;
}

void TranslatedState::StoreMaterializedValuesAndDeopt(JavaScriptFrame* frame) {
  MaterializedObjectStore* materialized_store =
      isolate_->materialized_object_store();
  Handle<FixedArray> previously_materialized_objects =
      materialized_store->Get(stack_frame_pointer_);

  Handle<Object> marker = isolate_->factory()->arguments_marker();

  int length = static_cast<int>(object_positions_.size());
  bool new_store = false;
  if (previously_materialized_objects.is_null()) {
    previously_materialized_objects =
        isolate_->factory()->NewFixedArray(length);
    for (int i = 0; i < length; i++) {
      previously_materialized_objects->set(i, *marker);
    }
    new_store = true;
  }

  CHECK_EQ(length, previously_materialized_objects->length());

  bool value_changed = false;
  for (int i = 0; i < length; i++) {
    TranslatedState::ObjectPosition pos = object_positions_[i];
    TranslatedValue* value_info =
        &(frames_[pos.frame_index_].values_[pos.value_index_]);

    CHECK(value_info->IsMaterializedObject());

    Handle<Object> value(value_info->GetRawValue(), isolate_);

    if (!value.is_identical_to(marker)) {
      if (previously_materialized_objects->get(i) == *marker) {
        previously_materialized_objects->set(i, *value);
        value_changed = true;
      } else {
        CHECK(previously_materialized_objects->get(i) == *value);
      }
    }
  }
  if (new_store && value_changed) {
    materialized_store->Set(stack_frame_pointer_,
                            previously_materialized_objects);
    CHECK_EQ(frames_[0].kind(), TranslatedFrame::kInterpretedFunction);
    CHECK_EQ(frame->function(), frames_[0].front().GetRawValue());
    Deoptimizer::DeoptimizeFunction(frame->function(), frame->LookupCode());
  }
}

void TranslatedState::UpdateFromPreviouslyMaterializedObjects() {
  MaterializedObjectStore* materialized_store =
      isolate_->materialized_object_store();
  Handle<FixedArray> previously_materialized_objects =
      materialized_store->Get(stack_frame_pointer_);

  // If we have no previously materialized objects, there is nothing to do.
  if (previously_materialized_objects.is_null()) return;

  Handle<Object> marker = isolate_->factory()->arguments_marker();

  int length = static_cast<int>(object_positions_.size());
  CHECK_EQ(length, previously_materialized_objects->length());

  for (int i = 0; i < length; i++) {
    // For a previously materialized objects, inject their value into the
    // translated values.
    if (previously_materialized_objects->get(i) != *marker) {
      TranslatedState::ObjectPosition pos = object_positions_[i];
      TranslatedValue* value_info =
          &(frames_[pos.frame_index_].values_[pos.value_index_]);
      CHECK(value_info->IsMaterializedObject());

      value_info->value_ =
          Handle<Object>(previously_materialized_objects->get(i), isolate_);
    }
  }
}

}  // namespace internal
}  // namespace v8
