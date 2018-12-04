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

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// {FrameWriter} offers a stack writer abstraction for writing
// FrameDescriptions. The main service the class provides is managing
// {top_offset_}, i.e. the offset of the next slot to write to.
class FrameWriter {
 public:
  static const int NO_INPUT_INDEX = -1;
  FrameWriter(Deoptimizer* deoptimizer, FrameDescription* frame,
              CodeTracer::Scope* trace_scope)
      : deoptimizer_(deoptimizer),
        frame_(frame),
        trace_scope_(trace_scope),
        top_offset_(frame->GetFrameSize()) {}

  void PushRawValue(intptr_t value, const char* debug_hint) {
    PushValue(value);

    if (trace_scope_ != nullptr) {
      DebugPrintOutputValue(value, debug_hint);
    }
  }

  void PushRawObject(Object* obj, const char* debug_hint) {
    intptr_t value = reinterpret_cast<intptr_t>(obj);
    PushValue(value);
    if (trace_scope_ != nullptr) {
      DebugPrintOutputObject(obj, top_offset_, debug_hint);
    }
  }

  void PushCallerPc(intptr_t pc) {
    top_offset_ -= kPCOnStackSize;
    frame_->SetCallerPc(top_offset_, pc);
    DebugPrintOutputValue(pc, "caller's pc\n");
  }

  void PushCallerFp(intptr_t fp) {
    top_offset_ -= kFPOnStackSize;
    frame_->SetCallerFp(top_offset_, fp);
    DebugPrintOutputValue(fp, "caller's fp\n");
  }

  void PushCallerConstantPool(intptr_t cp) {
    top_offset_ -= kPointerSize;
    frame_->SetCallerConstantPool(top_offset_, cp);
    DebugPrintOutputValue(cp, "caller's constant_pool\n");
  }

  void PushTranslatedValue(const TranslatedFrame::iterator& iterator,
                           const char* debug_hint = "") {
    Object* obj = iterator->GetRawValue();

    PushRawObject(obj, debug_hint);

    if (trace_scope_) {
      PrintF(trace_scope_->file(), " (input #%d)\n", iterator.input_index());
    }

    deoptimizer_->QueueValueForMaterialization(output_address(top_offset_), obj,
                                               iterator);
  }

  unsigned top_offset() const { return top_offset_; }

 private:
  void PushValue(intptr_t value) {
    CHECK_GE(top_offset_, 0);
    top_offset_ -= kPointerSize;
    frame_->SetFrameSlot(top_offset_, value);
  }

  Address output_address(unsigned output_offset) {
    Address output_address =
        static_cast<Address>(frame_->GetTop()) + output_offset;
    return output_address;
  }

  void DebugPrintOutputValue(intptr_t value, const char* debug_hint = "") {
    if (trace_scope_ != nullptr) {
      PrintF(trace_scope_->file(),
             "    " V8PRIxPTR_FMT ": [top + %3d] <- " V8PRIxPTR_FMT " ;  %s",
             output_address(top_offset_), top_offset_, value, debug_hint);
    }
  }

  void DebugPrintOutputObject(Object* obj, unsigned output_offset,
                              const char* debug_hint = "") {
    if (trace_scope_ != nullptr) {
      PrintF(trace_scope_->file(), "    " V8PRIxPTR_FMT ": [top + %3d] <- ",
             output_address(output_offset), output_offset);
      if (obj->IsSmi()) {
        PrintF(V8PRIxPTR_FMT " <Smi %d>", reinterpret_cast<Address>(obj),
               Smi::cast(obj)->value());
      } else {
        obj->ShortPrint(trace_scope_->file());
      }
      PrintF(trace_scope_->file(), " ;  %s", debug_hint);
    }
  }

  Deoptimizer* deoptimizer_;
  FrameDescription* frame_;
  CodeTracer::Scope* trace_scope_;
  unsigned top_offset_;
};

DeoptimizerData::DeoptimizerData(Heap* heap) : heap_(heap), current_(nullptr) {
  for (int i = 0; i <= DeoptimizerData::kLastDeoptimizeKind; ++i) {
    deopt_entry_code_[i] = nullptr;
  }
  Code** start = &deopt_entry_code_[0];
  Code** end = &deopt_entry_code_[DeoptimizerData::kLastDeoptimizeKind + 1];
  heap_->RegisterStrongRoots(reinterpret_cast<Object**>(start),
                             reinterpret_cast<Object**>(end));
}


DeoptimizerData::~DeoptimizerData() {
  for (int i = 0; i <= DeoptimizerData::kLastDeoptimizeKind; ++i) {
    deopt_entry_code_[i] = nullptr;
  }
  Code** start = &deopt_entry_code_[0];
  heap_->UnregisterStrongRoots(reinterpret_cast<Object**>(start));
}

Code* DeoptimizerData::deopt_entry_code(DeoptimizeKind kind) {
  return deopt_entry_code_[static_cast<int>(kind)];
}

void DeoptimizerData::set_deopt_entry_code(DeoptimizeKind kind, Code* code) {
  deopt_entry_code_[static_cast<int>(kind)] = code;
}

Code* Deoptimizer::FindDeoptimizingCode(Address addr) {
  if (function_->IsHeapObject()) {
    // Search all deoptimizing code in the native context of the function.
    Isolate* isolate = isolate_;
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
Deoptimizer* Deoptimizer::New(JSFunction* function, DeoptimizeKind kind,
                              unsigned bailout_id, Address from,
                              int fp_to_sp_delta, Isolate* isolate) {
  Deoptimizer* deoptimizer = new Deoptimizer(isolate, function, kind,
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
        it->kind() == TranslatedFrame::kJavaScriptBuiltinContinuation ||
        it->kind() ==
            TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch) {
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

void Deoptimizer::GenerateDeoptimizationEntries(MacroAssembler* masm, int count,
                                                DeoptimizeKind kind) {
  NoRootArrayScope no_root_array(masm);
  TableEntryGenerator generator(masm, kind, count);
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
  void VisitThread(Isolate* isolate, ThreadLocalTop* top) override {
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
          it.frame()->set_pc(code->raw_instruction_start() + trampoline_pc);
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
                                     RuntimeCallCounterId::kDeoptimizeCode);
  TimerEventScope<TimerEventDeoptimizeCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.DeoptimizeCode");
  if (FLAG_trace_deopt) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[deoptimize all code in all contexts]\n");
  }
  isolate->AbortConcurrentOptimization(BlockingBehavior::kBlock);
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
                                     RuntimeCallCounterId::kDeoptimizeCode);
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
                                     RuntimeCallCounterId::kDeoptimizeCode);
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

const char* Deoptimizer::MessageFor(DeoptimizeKind kind) {
  switch (kind) {
    case DeoptimizeKind::kEager:
      return "eager";
    case DeoptimizeKind::kSoft:
      return "soft";
    case DeoptimizeKind::kLazy:
      return "lazy";
  }
  FATAL("Unsupported deopt kind");
  return nullptr;
}

Deoptimizer::Deoptimizer(Isolate* isolate, JSFunction* function,
                         DeoptimizeKind kind, unsigned bailout_id, Address from,
                         int fp_to_sp_delta)
    : isolate_(isolate),
      function_(function),
      bailout_id_(bailout_id),
      deopt_kind_(kind),
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

  DCHECK_NE(from, kNullAddress);
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

    if (deopt_kind_ == DeoptimizeKind::kSoft) {
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
            CodeDeoptEvent(compiled_code_, kind, from_, fp_to_sp_delta_));
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
  return Handle<JSFunction>(function_, isolate());
}
Handle<Code> Deoptimizer::compiled_code() const {
  return Handle<Code>(compiled_code_, isolate());
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
                                            DeoptimizeKind kind) {
  CHECK_GE(id, 0);
  if (id >= kMaxNumberOfEntries) return kNullAddress;
  DeoptimizerData* data = isolate->deoptimizer_data();
  CHECK_LE(kind, DeoptimizerData::kLastDeoptimizeKind);
  CHECK_NOT_NULL(data->deopt_entry_code(kind));
  Code* code = data->deopt_entry_code(kind);
  return code->raw_instruction_start() + (id * table_entry_size_);
}

int Deoptimizer::GetDeoptimizationId(Isolate* isolate, Address addr,
                                     DeoptimizeKind kind) {
  DeoptimizerData* data = isolate->deoptimizer_data();
  CHECK_LE(kind, DeoptimizerData::kLastDeoptimizeKind);
  DCHECK(IsInDeoptimizationTable(isolate, addr, kind));
  Code* code = data->deopt_entry_code(kind);
  Address start = code->raw_instruction_start();
  DCHECK_EQ(0,
            static_cast<int>(addr - start) % table_entry_size_);
  return static_cast<int>(addr - start) / table_entry_size_;
}

bool Deoptimizer::IsInDeoptimizationTable(Isolate* isolate, Address addr,
                                          DeoptimizeKind type) {
  DeoptimizerData* data = isolate->deoptimizer_data();
  CHECK_LE(type, DeoptimizerData::kLastDeoptimizeKind);
  Code* code = data->deopt_entry_code(type);
  if (code == nullptr) return false;
  Address start = code->raw_instruction_start();
  return ((table_entry_size_ == 0 && addr == start) ||
          (addr >= start &&
           addr < start + (kMaxNumberOfEntries * table_entry_size_)));
}

bool Deoptimizer::IsDeoptimizationEntry(Isolate* isolate, Address addr,
                                        DeoptimizeKind* type) {
  if (IsInDeoptimizationTable(isolate, addr, DeoptimizeKind::kEager)) {
    *type = DeoptimizeKind::kEager;
    return true;
  }
  if (IsInDeoptimizationTable(isolate, addr, DeoptimizeKind::kSoft)) {
    *type = DeoptimizeKind::kSoft;
    return true;
  }
  if (IsInDeoptimizationTable(isolate, addr, DeoptimizeKind::kLazy)) {
    *type = DeoptimizeKind::kLazy;
    return true;
  }
  return false;
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
      HandlerTable table(
          translated_frame->raw_shared_info()->GetBytecodeArray());
      return table.LookupRange(bytecode_offset, data_out, nullptr);
    }
    case TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch: {
      return 0;
    }
    default:
      break;
  }
  return -1;
}

bool ShouldPadArguments(int arg_count) {
  return kPadArguments && (arg_count % 2 != 0);
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
    caller_fp_ = Memory<intptr_t>(fp_address);
    caller_pc_ =
        Memory<intptr_t>(fp_address + CommonFrameConstants::kCallerPCOffset);
    input_frame_context_ = Memory<intptr_t>(
        fp_address + CommonFrameConstants::kContextOrFrameTypeOffset);

    if (FLAG_enable_embedded_constant_pool) {
      caller_constant_pool_ = Memory<intptr_t>(
          fp_address + CommonFrameConstants::kConstantPoolOffset);
    }
  }

  if (trace_scope_ != nullptr) {
    timer.Start();
    PrintF(trace_scope_->file(), "[deoptimizing (DEOPT %s): begin ",
           MessageFor(deopt_kind_));
    PrintFunctionName();
    PrintF(trace_scope_->file(),
           " (opt #%d) @%d, FP to SP delta: %d, caller sp: " V8PRIxPTR_FMT
           "]\n",
           input_data->OptimizationId()->value(), bailout_id_, fp_to_sp_delta_,
           caller_frame_top_);
    if (deopt_kind_ == DeoptimizeKind::kEager ||
        deopt_kind_ == DeoptimizeKind::kSoft) {
      compiled_code_->PrintDeoptLocation(
          trace_scope_->file(), "            ;;; deoptimize at ", from_);
    }
  }

  BailoutId node_id = input_data->BytecodeOffset(bailout_id_);
  ByteArray* translations = input_data->TranslationByteArray();
  unsigned translation_index =
      input_data->TranslationIndex(bailout_id_)->value();

  TranslationIterator state_iterator(translations, translation_index);
  translated_state_.Init(
      isolate_, input_->GetFramePointerAddress(), &state_iterator,
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
    bool handle_exception = deoptimizing_throw_ && i == count - 1;
    switch (translated_frame->kind()) {
      case TranslatedFrame::kInterpretedFunction:
        DoComputeInterpretedFrame(translated_frame, frame_index,
                                  handle_exception);
        jsframe_count_++;
        break;
      case TranslatedFrame::kArgumentsAdaptor:
        DoComputeArgumentsAdaptorFrame(translated_frame, frame_index);
        break;
      case TranslatedFrame::kConstructStub:
        DoComputeConstructStubFrame(translated_frame, frame_index);
        break;
      case TranslatedFrame::kBuiltinContinuation:
        DoComputeBuiltinContinuation(translated_frame, frame_index,
                                     BuiltinContinuationMode::STUB);
        break;
      case TranslatedFrame::kJavaScriptBuiltinContinuation:
        DoComputeBuiltinContinuation(translated_frame, frame_index,
                                     BuiltinContinuationMode::JAVASCRIPT);
        break;
      case TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch:
        DoComputeBuiltinContinuation(
            translated_frame, frame_index,
            handle_exception
                ? BuiltinContinuationMode::JAVASCRIPT_HANDLE_EXCEPTION
                : BuiltinContinuationMode::JAVASCRIPT_WITH_CATCH);
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
           MessageFor(deopt_kind_));
    PrintFunctionName();
    PrintF(trace_scope_->file(),
           " @%d => node=%d, pc=" V8PRIxPTR_FMT ", caller sp=" V8PRIxPTR_FMT
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

  TranslatedFrame::iterator function_iterator = value_iterator++;
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
  // the part described by InterpreterFrameConstants. This will include
  // argument padding, when needed.
  unsigned fixed_frame_size = ComputeInterpretedFixedSize(shared);
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  int parameter_count = shared->internal_formal_parameter_count() + 1;
  FrameDescription* output_frame = new (output_frame_size)
      FrameDescription(output_frame_size, parameter_count);
  FrameWriter frame_writer(this, output_frame, trace_scope_);

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

  ReadOnlyRoots roots(isolate());
  if (ShouldPadArguments(parameter_count)) {
    frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
  }

  for (int i = 0; i < parameter_count; ++i, ++value_iterator) {
    frame_writer.PushTranslatedValue(value_iterator, "stack parameter");
  }

  DCHECK_EQ(output_frame->GetLastArgumentSlotOffset(),
            frame_writer.top_offset());
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
  const intptr_t caller_pc =
      is_bottommost ? caller_pc_ : output_[frame_index - 1]->GetPc();
  frame_writer.PushCallerPc(caller_pc);

  // The caller's frame pointer for the bottommost output frame is the same
  // as in the input frame.  For all subsequent output frames, it can be
  // read from the previous one.  Also compute and set this frame's frame
  // pointer.
  const intptr_t caller_fp =
      is_bottommost ? caller_fp_ : output_[frame_index - 1]->GetFp();
  frame_writer.PushCallerFp(caller_fp);

  intptr_t fp_value = top_address + frame_writer.top_offset();
  output_frame->SetFp(fp_value);
  if (is_topmost) {
    Register fp_reg = InterpretedFrame::fp_register();
    output_frame->SetRegister(fp_reg.code(), fp_value);
  }

  if (FLAG_enable_embedded_constant_pool) {
    // For the bottommost output frame the constant pool pointer can be gotten
    // from the input frame. For subsequent output frames, it can be read from
    // the previous frame.
    const intptr_t caller_cp =
        is_bottommost ? caller_constant_pool_
                      : output_[frame_index - 1]->GetConstantPool();
    frame_writer.PushCallerConstantPool(caller_cp);
  }

  // For the bottommost output frame the context can be gotten from the input
  // frame. For all subsequent output frames it can be gotten from the function
  // so long as we don't inline functions that need local contexts.

  // When deoptimizing into a catch block, we need to take the context
  // from a register that was specified in the handler table.
  TranslatedFrame::iterator context_pos = value_iterator++;
  if (goto_catch_handler) {
    // Skip to the translated value of the register specified
    // in the handler table.
    for (int i = 0; i < catch_handler_data_ + 1; ++i) {
      context_pos++;
    }
  }
  // Read the context from the translations.
  Object* context = context_pos->GetRawValue();
  output_frame->SetContext(reinterpret_cast<intptr_t>(context));
  frame_writer.PushTranslatedValue(context_pos, "context\n");

  // The function was mentioned explicitly in the BEGIN_FRAME.
  frame_writer.PushTranslatedValue(function_iterator, "function\n");

  // Set the bytecode array pointer.
  Object* bytecode_array = shared->HasBreakInfo()
                               ? shared->GetDebugInfo()->DebugBytecodeArray()
                               : shared->GetBytecodeArray();
  frame_writer.PushRawObject(bytecode_array, "bytecode array\n");

  // The bytecode offset was mentioned explicitly in the BEGIN_FRAME.
  int raw_bytecode_offset =
      BytecodeArray::kHeaderSize - kHeapObjectTag + bytecode_offset;
  Smi* smi_bytecode_offset = Smi::FromInt(raw_bytecode_offset);
  frame_writer.PushRawObject(smi_bytecode_offset, "bytecode offset\n");

  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(), "    -------------------------\n");
  }

  // Translate the rest of the interpreter registers in the frame.
  for (int i = 0; i < register_count; ++i, ++value_iterator) {
    frame_writer.PushTranslatedValue(value_iterator, "stack parameter");
  }

  int register_slots_written = register_count;
  DCHECK_LE(register_slots_written, register_stack_slot_count);
  // Some architectures must pad the stack frame with extra stack slots
  // to ensure the stack frame is aligned. Do this now.
  while (register_slots_written < register_stack_slot_count) {
    register_slots_written++;
    frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
  }

  // Translate the accumulator register (depending on frame position).
  if (is_topmost) {
    if (PadTopOfStackRegister()) {
      frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
    }
    // For topmost frame, put the accumulator on the stack. The
    // {NotifyDeoptimized} builtin pops it off the topmost frame (possibly
    // after materialization).
    if (goto_catch_handler) {
      // If we are lazy deopting to a catch handler, we set the accumulator to
      // the exception (which lives in the result register).
      intptr_t accumulator_value =
          input_->GetRegister(kInterpreterAccumulatorRegister.code());
      frame_writer.PushRawObject(reinterpret_cast<Object*>(accumulator_value),
                                 "accumulator\n");
      ++value_iterator;  // Skip the accumulator.
    } else {
      frame_writer.PushTranslatedValue(value_iterator++, "accumulator");
    }
  } else {
    // For non-topmost frames, skip the accumulator translation. For those
    // frames, the return value from the callee will become the accumulator.
    ++value_iterator;
  }
  CHECK_EQ(translated_frame->end(), value_iterator);
  CHECK_EQ(0u, frame_writer.top_offset());

  // Compute this frame's PC and state. The PC will be a special builtin that
  // continues the bytecode dispatch. Note that non-topmost and lazy-style
  // bailout handlers also advance the bytecode offset before dispatch, hence
  // simulating what normal handlers do upon completion of the operation.
  Builtins* builtins = isolate_->builtins();
  Code* dispatch_builtin =
      (!is_topmost || (deopt_kind_ == DeoptimizeKind::kLazy)) &&
              !goto_catch_handler
          ? builtins->builtin(Builtins::kInterpreterEnterBytecodeAdvance)
          : builtins->builtin(Builtins::kInterpreterEnterBytecodeDispatch);
  output_frame->SetPc(
      static_cast<intptr_t>(dispatch_builtin->InstructionStart()));

  // Update constant pool.
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        static_cast<intptr_t>(dispatch_builtin->constant_pool());
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
        static_cast<intptr_t>(continuation->InstructionStart()));
  }
}

void Deoptimizer::DoComputeArgumentsAdaptorFrame(
    TranslatedFrame* translated_frame, int frame_index) {
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  bool is_bottommost = (0 == frame_index);

  unsigned height = translated_frame->height();
  unsigned height_in_bytes = height * kPointerSize;
  int parameter_count = height;
  if (ShouldPadArguments(parameter_count)) height_in_bytes += kPointerSize;

  TranslatedFrame::iterator function_iterator = value_iterator++;
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(),
           "  translating arguments adaptor => height=%d\n", height_in_bytes);
  }

  unsigned fixed_frame_size = ArgumentsAdaptorFrameConstants::kFixedFrameSize;
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame = new (output_frame_size)
      FrameDescription(output_frame_size, parameter_count);
  FrameWriter frame_writer(this, output_frame, trace_scope_);

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

  ReadOnlyRoots roots(isolate());
  if (ShouldPadArguments(parameter_count)) {
    frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
  }

  // Compute the incoming parameter translation.
  for (int i = 0; i < parameter_count; ++i, ++value_iterator) {
    frame_writer.PushTranslatedValue(value_iterator, "stack parameter");
  }

  DCHECK_EQ(output_frame->GetLastArgumentSlotOffset(),
            frame_writer.top_offset());

  // Read caller's PC from the previous frame.
  const intptr_t caller_pc =
      is_bottommost ? caller_pc_ : output_[frame_index - 1]->GetPc();
  frame_writer.PushCallerPc(caller_pc);

  // Read caller's FP from the previous frame, and set this frame's FP.
  const intptr_t caller_fp =
      is_bottommost ? caller_fp_ : output_[frame_index - 1]->GetFp();
  frame_writer.PushCallerFp(caller_fp);

  intptr_t fp_value = top_address + frame_writer.top_offset();
  output_frame->SetFp(fp_value);

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    const intptr_t caller_cp =
        is_bottommost ? caller_constant_pool_
                      : output_[frame_index - 1]->GetConstantPool();
    frame_writer.PushCallerConstantPool(caller_cp);
  }

  // A marker value is used in place of the context.
  intptr_t marker = StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR);
  frame_writer.PushRawValue(marker, "context (adaptor sentinel)\n");

  // The function was mentioned explicitly in the ARGUMENTS_ADAPTOR_FRAME.
  frame_writer.PushTranslatedValue(function_iterator, "function\n");

  // Number of incoming arguments.
  frame_writer.PushRawObject(Smi::FromInt(height - 1), "argc\n");

  frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");

  CHECK_EQ(translated_frame->end(), value_iterator);
  DCHECK_EQ(0, frame_writer.top_offset());

  Builtins* builtins = isolate_->builtins();
  Code* adaptor_trampoline =
      builtins->builtin(Builtins::kArgumentsAdaptorTrampoline);
  intptr_t pc_value = static_cast<intptr_t>(
      adaptor_trampoline->InstructionStart() +
      isolate_->heap()->arguments_adaptor_deopt_pc_offset()->value());
  output_frame->SetPc(pc_value);
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        static_cast<intptr_t>(adaptor_trampoline->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
  }
}

void Deoptimizer::DoComputeConstructStubFrame(TranslatedFrame* translated_frame,
                                              int frame_index) {
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  bool is_topmost = (output_count_ - 1 == frame_index);
  // The construct frame could become topmost only if we inlined a constructor
  // call which does a tail call (otherwise the tail callee's frame would be
  // the topmost one). So it could only be the DeoptimizeKind::kLazy case.
  CHECK(!is_topmost || deopt_kind_ == DeoptimizeKind::kLazy);

  Builtins* builtins = isolate_->builtins();
  Code* construct_stub = builtins->builtin(Builtins::kJSConstructStubGeneric);
  BailoutId bailout_id = translated_frame->node_id();
  unsigned height = translated_frame->height();
  unsigned parameter_count = height - 1;  // Exclude the context.
  unsigned height_in_bytes = parameter_count * kPointerSize;

  // If the construct frame appears to be topmost we should ensure that the
  // value of result register is preserved during continuation execution.
  // We do this here by "pushing" the result of the constructor function to the
  // top of the reconstructed stack and popping it in
  // {Builtins::kNotifyDeoptimized}.
  if (is_topmost) {
    height_in_bytes += kPointerSize;
    if (PadTopOfStackRegister()) height_in_bytes += kPointerSize;
  }

  if (ShouldPadArguments(parameter_count)) height_in_bytes += kPointerSize;

  TranslatedFrame::iterator function_iterator = value_iterator++;
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
  FrameDescription* output_frame = new (output_frame_size)
      FrameDescription(output_frame_size, parameter_count);
  FrameWriter frame_writer(this, output_frame, trace_scope_);

  // Construct stub can not be topmost.
  DCHECK(frame_index > 0 && frame_index < output_count_);
  DCHECK_NULL(output_[frame_index]);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous frame's top and
  // this frame's size.
  intptr_t top_address;
  top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  output_frame->SetTop(top_address);

  ReadOnlyRoots roots(isolate());
  if (ShouldPadArguments(parameter_count)) {
    frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
  }

  // The allocated receiver of a construct stub frame is passed as the
  // receiver parameter through the translation. It might be encoding
  // a captured object, so we need save it for later.
  TranslatedFrame::iterator receiver_iterator = value_iterator;

  // Compute the incoming parameter translation.
  for (unsigned i = 0; i < parameter_count; ++i, ++value_iterator) {
    frame_writer.PushTranslatedValue(value_iterator, "stack parameter");
  }

  DCHECK_EQ(output_frame->GetLastArgumentSlotOffset(),
            frame_writer.top_offset());

  // Read caller's PC from the previous frame.
  const intptr_t caller_pc = output_[frame_index - 1]->GetPc();
  frame_writer.PushCallerPc(caller_pc);

  // Read caller's FP from the previous frame, and set this frame's FP.
  const intptr_t caller_fp = output_[frame_index - 1]->GetFp();
  frame_writer.PushCallerFp(caller_fp);

  intptr_t fp_value = top_address + frame_writer.top_offset();
  output_frame->SetFp(fp_value);
  if (is_topmost) {
    Register fp_reg = JavaScriptFrame::fp_register();
    output_frame->SetRegister(fp_reg.code(), fp_value);
  }

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    const intptr_t caller_cp = output_[frame_index - 1]->GetConstantPool();
    frame_writer.PushCallerConstantPool(caller_cp);
  }

  // A marker value is used to mark the frame.
  intptr_t marker = StackFrame::TypeToMarker(StackFrame::CONSTRUCT);
  frame_writer.PushRawValue(marker, "context (construct stub sentinel)\n");

  frame_writer.PushTranslatedValue(value_iterator++, "context\n");

  // Number of incoming arguments.
  frame_writer.PushRawObject(Smi::FromInt(parameter_count - 1), "argc\n");

  // The constructor function was mentioned explicitly in the
  // CONSTRUCT_STUB_FRAME.
  frame_writer.PushTranslatedValue(function_iterator, "constuctor function\n");

  // The deopt info contains the implicit receiver or the new target at the
  // position of the receiver. Copy it to the top of stack, with the hole value
  // as padding to maintain alignment.

  frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");

  CHECK(bailout_id == BailoutId::ConstructStubCreate() ||
        bailout_id == BailoutId::ConstructStubInvoke());
  const char* debug_hint = bailout_id == BailoutId::ConstructStubCreate()
                               ? "new target\n"
                               : "allocated receiver\n";
  frame_writer.PushTranslatedValue(receiver_iterator, debug_hint);

  if (is_topmost) {
    if (PadTopOfStackRegister()) {
      frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
    }
    // Ensure the result is restored back when we return to the stub.
    Register result_reg = kReturnRegister0;
    intptr_t result = input_->GetRegister(result_reg.code());
    frame_writer.PushRawValue(result, "subcall result\n");
  }

  CHECK_EQ(translated_frame->end(), value_iterator);
  CHECK_EQ(0u, frame_writer.top_offset());

  // Compute this frame's PC.
  DCHECK(bailout_id.IsValidForConstructStub());
  Address start = construct_stub->InstructionStart();
  int pc_offset =
      bailout_id == BailoutId::ConstructStubCreate()
          ? isolate_->heap()->construct_stub_create_deopt_pc_offset()->value()
          : isolate_->heap()->construct_stub_invoke_deopt_pc_offset()->value();
  intptr_t pc_value = static_cast<intptr_t>(start + pc_offset);
  output_frame->SetPc(pc_value);

  // Update constant pool.
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        static_cast<intptr_t>(construct_stub->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
    if (is_topmost) {
      Register constant_pool_reg =
          JavaScriptFrame::constant_pool_pointer_register();
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
  }

  // Set the continuation for the topmost frame.
  if (is_topmost) {
    Builtins* builtins = isolate_->builtins();
    DCHECK_EQ(DeoptimizeKind::kLazy, deopt_kind_);
    Code* continuation = builtins->builtin(Builtins::kNotifyDeoptimized);
    output_frame->SetContinuation(
        static_cast<intptr_t>(continuation->InstructionStart()));
  }
}

bool Deoptimizer::BuiltinContinuationModeIsJavaScript(
    BuiltinContinuationMode mode) {
  switch (mode) {
    case BuiltinContinuationMode::STUB:
      return false;
    case BuiltinContinuationMode::JAVASCRIPT:
    case BuiltinContinuationMode::JAVASCRIPT_WITH_CATCH:
    case BuiltinContinuationMode::JAVASCRIPT_HANDLE_EXCEPTION:
      return true;
  }
  UNREACHABLE();
}

bool Deoptimizer::BuiltinContinuationModeIsWithCatch(
    BuiltinContinuationMode mode) {
  switch (mode) {
    case BuiltinContinuationMode::STUB:
    case BuiltinContinuationMode::JAVASCRIPT:
      return false;
    case BuiltinContinuationMode::JAVASCRIPT_WITH_CATCH:
    case BuiltinContinuationMode::JAVASCRIPT_HANDLE_EXCEPTION:
      return true;
  }
  UNREACHABLE();
}

StackFrame::Type Deoptimizer::BuiltinContinuationModeToFrameType(
    BuiltinContinuationMode mode) {
  switch (mode) {
    case BuiltinContinuationMode::STUB:
      return StackFrame::BUILTIN_CONTINUATION;
    case BuiltinContinuationMode::JAVASCRIPT:
      return StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION;
    case BuiltinContinuationMode::JAVASCRIPT_WITH_CATCH:
      return StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH;
    case BuiltinContinuationMode::JAVASCRIPT_HANDLE_EXCEPTION:
      return StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH;
  }
  UNREACHABLE();
}

Builtins::Name Deoptimizer::TrampolineForBuiltinContinuation(
    BuiltinContinuationMode mode, bool must_handle_result) {
  switch (mode) {
    case BuiltinContinuationMode::STUB:
      return must_handle_result ? Builtins::kContinueToCodeStubBuiltinWithResult
                                : Builtins::kContinueToCodeStubBuiltin;
    case BuiltinContinuationMode::JAVASCRIPT:
    case BuiltinContinuationMode::JAVASCRIPT_WITH_CATCH:
    case BuiltinContinuationMode::JAVASCRIPT_HANDLE_EXCEPTION:
      return must_handle_result
                 ? Builtins::kContinueToJavaScriptBuiltinWithResult
                 : Builtins::kContinueToJavaScriptBuiltin;
  }
  UNREACHABLE();
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
//    | arg padding (arch dept) |<- at most 1*kPointerSize
//    +-------------------------+
//    |     builtin param 0     |<- FrameState input value n becomes
//    +-------------------------+
//    |           ...           |
//    +-------------------------+
//    |     builtin param m     |<- FrameState input value n+m-1, or in
//    +-----needs-alignment-----+   the LAZY case, return LAZY result value
//    | ContinueToBuiltin entry |
//    +-------------------------+
// |  |    saved frame (FP)     |
// |  +=====needs=alignment=====+<- fpreg
// |  |constant pool (if ool_cp)|
// v  +-------------------------+
//    |BUILTIN_CONTINUATION mark|
//    +-------------------------+
//    |  JSFunction (or zero)   |<- only if JavaScript builtin
//    +-------------------------+
//    |  frame height above FP  |
//    +-------------------------+
//    |         context         |<- this non-standard context slot contains
//    +-------------------------+   the context, even for non-JS builtins.
//    |     builtin address     |
//    +-------------------------+
//    | builtin input GPR reg0  |<- populated from deopt FrameState using
//    +-------------------------+   the builtin's CallInterfaceDescriptor
//    |          ...            |   to map a FrameState's 0..n-1 inputs to
//    +-------------------------+   the builtin's n input register params.
//    | builtin input GPR regn  |
//    +-------------------------+
//    | reg padding (arch dept) |
//    +-----needs--alignment----+
//    | res padding (arch dept) |<- only if {is_topmost}; result is pop'd by
//    +-------------------------+<- kNotifyDeopt ASM stub and moved to acc
//    |      result  value      |<- reg, as ContinueToBuiltin stub expects.
//    +-----needs-alignment-----+<- spreg
//
void Deoptimizer::DoComputeBuiltinContinuation(
    TranslatedFrame* translated_frame, int frame_index,
    BuiltinContinuationMode mode) {
  TranslatedFrame::iterator value_iterator = translated_frame->begin();

  // The output frame must have room for all of the parameters that need to be
  // passed to the builtin continuation.
  const int height_in_words = translated_frame->height();

  BailoutId bailout_id = translated_frame->node_id();
  Builtins::Name builtin_name = Builtins::GetBuiltinFromBailoutId(bailout_id);
  CHECK(!Builtins::IsLazy(builtin_name));
  Code* builtin = isolate()->builtins()->builtin(builtin_name);
  Callable continuation_callable =
      Builtins::CallableFor(isolate(), builtin_name);
  CallInterfaceDescriptor continuation_descriptor =
      continuation_callable.descriptor();

  const bool is_bottommost = (0 == frame_index);
  const bool is_topmost = (output_count_ - 1 == frame_index);
  const bool must_handle_result =
      !is_topmost || deopt_kind_ == DeoptimizeKind::kLazy;

#if defined(V8_TARGET_ARCH_IA32) && defined(V8_EMBEDDED_BUILTINS)
  // TODO(v8:6666): Fold into Default config once root is fully supported.
  const RegisterConfiguration* config(
      RegisterConfiguration::PreserveRootIA32());
#else
  const RegisterConfiguration* config(RegisterConfiguration::Default());
#endif
  const int allocatable_register_count =
      config->num_allocatable_general_registers();
  const int padding_slot_count =
      BuiltinContinuationFrameConstants::PaddingSlotCount(
          allocatable_register_count);

  const int register_parameter_count =
      continuation_descriptor.GetRegisterParameterCount();
  // Make sure to account for the context by removing it from the register
  // parameter count.
  const int translated_stack_parameters =
      height_in_words - register_parameter_count - 1;
  const int stack_param_count =
      translated_stack_parameters + (must_handle_result ? 1 : 0) +
      (BuiltinContinuationModeIsWithCatch(mode) ? 1 : 0);
  const int stack_param_pad_count =
      ShouldPadArguments(stack_param_count) ? 1 : 0;

  // If the builtins frame appears to be topmost we should ensure that the
  // value of result register is preserved during continuation execution.
  // We do this here by "pushing" the result of callback function to the
  // top of the reconstructed stack and popping it in
  // {Builtins::kNotifyDeoptimized}.
  const int push_result_count =
      is_topmost ? (PadTopOfStackRegister() ? 2 : 1) : 0;

  const unsigned output_frame_size =
      kPointerSize * (stack_param_count + stack_param_pad_count +
                      allocatable_register_count + padding_slot_count +
                      push_result_count) +
      BuiltinContinuationFrameConstants::kFixedFrameSize;

  const unsigned output_frame_size_above_fp =
      kPointerSize * (allocatable_register_count + padding_slot_count +
                      push_result_count) +
      (BuiltinContinuationFrameConstants::kFixedFrameSize -
       BuiltinContinuationFrameConstants::kFixedFrameSizeAboveFp);

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
  CHECK_EQ(BuiltinContinuationModeIsJavaScript(mode), has_argc);

  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(),
           "  translating BuiltinContinuation to %s,"
           " register param count %d,"
           " stack param count %d\n",
           Builtins::name(builtin_name), register_parameter_count,
           stack_param_count);
  }

  FrameDescription* output_frame = new (output_frame_size)
      FrameDescription(output_frame_size, stack_param_count);
  output_[frame_index] = output_frame;
  FrameWriter frame_writer(this, output_frame, trace_scope_);

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
  const intptr_t maybe_function =
      reinterpret_cast<intptr_t>(value_iterator->GetRawValue());
  ++value_iterator;

  ReadOnlyRoots roots(isolate());
  if (ShouldPadArguments(stack_param_count)) {
    frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
  }

  for (int i = 0; i < translated_stack_parameters; ++i, ++value_iterator) {
    frame_writer.PushTranslatedValue(value_iterator, "stack parameter");
  }

  switch (mode) {
    case BuiltinContinuationMode::STUB:
      break;
    case BuiltinContinuationMode::JAVASCRIPT:
      break;
    case BuiltinContinuationMode::JAVASCRIPT_WITH_CATCH: {
      frame_writer.PushRawObject(roots.the_hole_value(),
                                 "placeholder for exception on lazy deopt\n");
    } break;
    case BuiltinContinuationMode::JAVASCRIPT_HANDLE_EXCEPTION: {
      intptr_t accumulator_value =
          input_->GetRegister(kInterpreterAccumulatorRegister.code());
      frame_writer.PushRawObject(reinterpret_cast<Object*>(accumulator_value),
                                 "exception (from accumulator)\n");
    } break;
  }

  if (must_handle_result) {
    frame_writer.PushRawObject(roots.the_hole_value(),
                               "placeholder for return result on lazy deopt\n");
  }

  DCHECK_EQ(output_frame->GetLastArgumentSlotOffset(),
            frame_writer.top_offset());

  std::vector<TranslatedFrame::iterator> register_values;
  int total_registers = config->num_general_registers();
  register_values.resize(total_registers, {value_iterator});

  for (int i = 0; i < register_parameter_count; ++i, ++value_iterator) {
    int code = continuation_descriptor.GetRegisterParameter(i).code();
    register_values[code] = value_iterator;
  }

  // The context register is always implicit in the CallInterfaceDescriptor but
  // its register must be explicitly set when continuing to the builtin. Make
  // sure that it's harvested from the translation and copied into the register
  // set (it was automatically added at the end of the FrameState by the
  // instruction selector).
  Object* context = value_iterator->GetRawValue();
  const intptr_t value = reinterpret_cast<intptr_t>(context);
  TranslatedFrame::iterator context_register_value = value_iterator++;
  register_values[kContextRegister.code()] = context_register_value;
  output_frame->SetContext(value);
  output_frame->SetRegister(kContextRegister.code(), value);

  // Set caller's PC (JSFunction continuation).
  const intptr_t caller_pc =
      is_bottommost ? caller_pc_ : output_[frame_index - 1]->GetPc();
  frame_writer.PushCallerPc(caller_pc);

  // Read caller's FP from the previous frame, and set this frame's FP.
  const intptr_t caller_fp =
      is_bottommost ? caller_fp_ : output_[frame_index - 1]->GetFp();
  frame_writer.PushCallerFp(caller_fp);

  const intptr_t fp_value = top_address + frame_writer.top_offset();
  output_frame->SetFp(fp_value);

  DCHECK_EQ(output_frame_size_above_fp, frame_writer.top_offset());

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    const intptr_t caller_cp =
        is_bottommost ? caller_constant_pool_
                      : output_[frame_index - 1]->GetConstantPool();
    frame_writer.PushCallerConstantPool(caller_cp);
  }

  // A marker value is used in place of the context.
  const intptr_t marker =
      StackFrame::TypeToMarker(BuiltinContinuationModeToFrameType(mode));
  frame_writer.PushRawValue(marker,
                            "context (builtin continuation sentinel)\n");

  if (BuiltinContinuationModeIsJavaScript(mode)) {
    frame_writer.PushRawValue(maybe_function, "JSFunction\n");
  } else {
    frame_writer.PushRawValue(0, "unused\n");
  }

  // The delta from the SP to the FP; used to reconstruct SP in
  // Isolate::UnwindAndFindHandler.
  frame_writer.PushRawObject(Smi::FromInt(output_frame_size_above_fp),
                             "frame height at deoptimization\n");

  // The context even if this is a stub contininuation frame. We can't use the
  // usual context slot, because we must store the frame marker there.
  frame_writer.PushTranslatedValue(context_register_value,
                                   "builtin JavaScript context\n");

  // The builtin to continue to.
  frame_writer.PushRawObject(builtin, "builtin address\n");

  for (int i = 0; i < allocatable_register_count; ++i) {
    int code = config->GetAllocatableGeneralCode(i);
    ScopedVector<char> str(128);
    if (trace_scope_ != nullptr) {
      if (BuiltinContinuationModeIsJavaScript(mode) &&
          code == kJavaScriptCallArgCountRegister.code()) {
        SNPrintF(
            str,
            "tagged argument count %s (will be untagged by continuation)\n",
            config->GetGeneralRegisterName(code));
      } else {
        SNPrintF(str, "builtin register argument %s\n",
                 config->GetGeneralRegisterName(code));
      }
    }
    frame_writer.PushTranslatedValue(
        register_values[code], trace_scope_ != nullptr ? str.start() : "");
  }

  // Some architectures must pad the stack frame with extra stack slots
  // to ensure the stack frame is aligned.
  for (int i = 0; i < padding_slot_count; ++i) {
    frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
  }

  if (is_topmost) {
    if (PadTopOfStackRegister()) {
      frame_writer.PushRawObject(roots.the_hole_value(), "padding\n");
    }
    // Ensure the result is restored back when we return to the stub.

    if (must_handle_result) {
      Register result_reg = kReturnRegister0;
      frame_writer.PushRawValue(input_->GetRegister(result_reg.code()),
                                "callback result\n");
    } else {
      frame_writer.PushRawObject(roots.undefined_value(), "callback result\n");
    }
  }

  CHECK_EQ(translated_frame->end(), value_iterator);
  CHECK_EQ(0u, frame_writer.top_offset());

  // Clear the context register. The context might be a de-materialized object
  // and will be materialized by {Runtime_NotifyDeoptimized}. For additional
  // safety we use Smi(0) instead of the potential {arguments_marker} here.
  if (is_topmost) {
    intptr_t context_value = reinterpret_cast<intptr_t>(Smi::kZero);
    Register context_reg = JavaScriptFrame::context_register();
    output_frame->SetRegister(context_reg.code(), context_value);
  }

  // Ensure the frame pointer register points to the callee's frame. The builtin
  // will build its own frame once we continue to it.
  Register fp_reg = JavaScriptFrame::fp_register();
  output_frame->SetRegister(fp_reg.code(), fp_value);

  Code* continue_to_builtin = isolate()->builtins()->builtin(
      TrampolineForBuiltinContinuation(mode, must_handle_result));
  output_frame->SetPc(
      static_cast<intptr_t>(continue_to_builtin->InstructionStart()));

  Code* continuation =
      isolate()->builtins()->builtin(Builtins::kNotifyDeoptimized);
  output_frame->SetContinuation(
      static_cast<intptr_t>(continuation->InstructionStart()));
}

void Deoptimizer::MaterializeHeapObjects() {
  translated_state_.Prepare(static_cast<Address>(stack_fp_));
  if (FLAG_deopt_every_n_times > 0) {
    // Doing a GC here will find problems with the deoptimized frames.
    isolate_->heap()->CollectAllGarbage(Heap::kNoGCFlags,
                                        GarbageCollectionReason::kTesting);
  }

  for (auto& materialization : values_to_materialize_) {
    Handle<Object> value = materialization.value_->GetValue();

    if (trace_scope_ != nullptr) {
      PrintF("Materialization [" V8PRIxPTR_FMT "] <- " V8PRIxPTR_FMT " ;  ",
             static_cast<intptr_t>(materialization.output_slot_address_),
             reinterpret_cast<intptr_t>(*value));
      value->ShortPrint(trace_scope_->file());
      PrintF(trace_scope_->file(), "\n");
    }

    *(reinterpret_cast<intptr_t*>(materialization.output_slot_address_)) =
        reinterpret_cast<intptr_t>(*value);
  }

  translated_state_.VerifyMaterializedObjects();

  bool feedback_updated = translated_state_.DoUpdateFeedback();
  if (trace_scope_ != nullptr && feedback_updated) {
    PrintF(trace_scope_->file(), "Feedback updated");
    compiled_code_->PrintDeoptLocation(trace_scope_->file(),
                                       " from deoptimization at ", from_);
  }

  isolate_->materialized_object_store()->Remove(
      static_cast<Address>(stack_fp_));
}

void Deoptimizer::QueueValueForMaterialization(
    Address output_address, Object* obj,
    const TranslatedFrame::iterator& iterator) {
  if (obj == ReadOnlyRoots(isolate_).arguments_marker()) {
    values_to_materialize_.push_back({output_address, iterator});
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
unsigned Deoptimizer::ComputeInterpretedFixedSize(SharedFunctionInfo* shared) {
  // The fixed part of the frame consists of the return address, frame
  // pointer, function, context, bytecode offset and all the incoming arguments.
  return ComputeIncomingArgumentSize(shared) +
         InterpreterFrameConstants::kFixedFrameSize;
}

// static
unsigned Deoptimizer::ComputeIncomingArgumentSize(SharedFunctionInfo* shared) {
  int parameter_slots = shared->internal_formal_parameter_count() + 1;
  if (kPadArguments) parameter_slots = RoundUp(parameter_slots, 2);
  return parameter_slots * kPointerSize;
}

void Deoptimizer::EnsureCodeForDeoptimizationEntry(Isolate* isolate,
                                                   DeoptimizeKind kind) {
  CHECK(kind == DeoptimizeKind::kEager || kind == DeoptimizeKind::kSoft ||
        kind == DeoptimizeKind::kLazy);
  DeoptimizerData* data = isolate->deoptimizer_data();
  if (data->deopt_entry_code(kind) != nullptr) return;

  MacroAssembler masm(isolate, nullptr, 16 * KB, CodeObjectRequired::kYes);
  masm.set_emit_debug_code(false);
  GenerateDeoptimizationEntries(&masm, kMaxNumberOfEntries, kind);
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(!RelocInfo::RequiresRelocationAfterCodegen(desc));

  // Allocate the code as immovable since the entry addresses will be used
  // directly and there is no support for relocating them.
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::STUB, Handle<Object>(), Builtins::kNoBuiltinId,
      MaybeHandle<ByteArray>(), MaybeHandle<DeoptimizationData>(), kImmovable);
  CHECK(Heap::IsImmovable(*code));

  CHECK_NULL(data->deopt_entry_code(kind));
  data->set_deopt_entry_code(kind, *code);
}

void Deoptimizer::EnsureCodeForMaxDeoptimizationEntries(Isolate* isolate) {
  EnsureCodeForDeoptimizationEntry(isolate, DeoptimizeKind::kEager);
  EnsureCodeForDeoptimizationEntry(isolate, DeoptimizeKind::kLazy);
  EnsureCodeForDeoptimizationEntry(isolate, DeoptimizeKind::kSoft);
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
  uint32_t bits = (static_cast<uint32_t>(is_negative ? -value : value) << 1) |
                  static_cast<uint32_t>(is_negative);
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

void Translation::BeginJavaScriptBuiltinContinuationWithCatchFrame(
    BailoutId bailout_id, int literal_id, unsigned height) {
  buffer_->Add(JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME);
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

void Translation::StoreInt64Register(Register reg) {
  buffer_->Add(INT64_REGISTER);
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

void Translation::StoreInt64StackSlot(int index) {
  buffer_->Add(INT64_STACK_SLOT);
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

void Translation::AddUpdateFeedback(int vector_literal, int slot) {
  buffer_->Add(UPDATE_FEEDBACK);
  buffer_->Add(vector_literal);
  buffer_->Add(slot);
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
    case INT64_REGISTER:
    case UINT32_REGISTER:
    case BOOL_REGISTER:
    case FLOAT_REGISTER:
    case DOUBLE_REGISTER:
    case STACK_SLOT:
    case INT32_STACK_SLOT:
    case INT64_STACK_SLOT:
    case UINT32_STACK_SLOT:
    case BOOL_STACK_SLOT:
    case FLOAT_STACK_SLOT:
    case DOUBLE_STACK_SLOT:
    case LITERAL:
      return 1;
    case ARGUMENTS_ADAPTOR_FRAME:
    case UPDATE_FEEDBACK:
      return 2;
    case BEGIN:
    case INTERPRETED_FRAME:
    case CONSTRUCT_STUB_FRAME:
    case BUILTIN_CONTINUATION_FRAME:
    case JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME:
    case JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME:
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
  array->set(fps_size, ReadOnlyRoots(isolate()).undefined_value());
  return true;
}


int MaterializedObjectStore::StackIdToIndex(Address fp) {
  auto it = std::find(frame_fps_.begin(), frame_fps_.end(), fp);
  return it == frame_fps_.end()
             ? -1
             : static_cast<int>(std::distance(frame_fps_.begin(), it));
}


Handle<FixedArray> MaterializedObjectStore::GetStackEntries() {
  return Handle<FixedArray>(isolate()->heap()->materialized_objects(),
                            isolate());
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
  HeapObject* undefined_value = ReadOnlyRoots(isolate()).undefined_value();
  for (int i = array->length(); i < length; i++) {
    new_array->set(i, undefined_value);
  }
  isolate()->heap()->SetRootMaterializedObjects(*new_array);
  return new_array;
}

namespace {

Handle<Object> GetValueForDebugger(TranslatedFrame::iterator it,
                                   Isolate* isolate) {
  if (it->GetRawValue() == ReadOnlyRoots(isolate).arguments_marker()) {
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
  int parameter_count =
      frame_it->shared_info()->internal_formal_parameter_count();
  TranslatedFrame::iterator stack_it = frame_it->begin();

  // Get the function. Note that this might materialize the function.
  // In case the debugger mutates this value, we should deoptimize
  // the function and remember the value in the materialized value store.
  function_ = Handle<JSFunction>::cast(stack_it->GetValue());
  stack_it++;  // Skip the function.
  stack_it++;  // Skip the receiver.

  DCHECK_EQ(TranslatedFrame::kInterpretedFunction, frame_it->kind());
  source_position_ = Deoptimizer::ComputeSourcePositionFromBytecodeArray(
      *frame_it->shared_info(), frame_it->node_id());

  DCHECK_EQ(parameter_count,
            function_->shared()->internal_formal_parameter_count());

  parameters_.resize(static_cast<size_t>(parameter_count));
  for (int i = 0; i < parameter_count; i++) {
    Handle<Object> parameter = GetValueForDebugger(stack_it, isolate);
    SetParameter(i, parameter);
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
  CHECK(code->InstructionStart() <= pc && pc <= code->InstructionEnd());
  SourcePosition last_position = SourcePosition::Unknown();
  DeoptimizeReason last_reason = DeoptimizeReason::kUnknown;
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
  return AbstractCode::cast(shared->GetBytecodeArray())
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
TranslatedValue TranslatedValue::NewInt64(TranslatedState* container,
                                          int64_t value) {
  TranslatedValue slot(container, kInt64);
  slot.int64_value_ = value;
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

int64_t TranslatedValue::int64_value() const {
  DCHECK_EQ(kInt64, kind());
  return int64_value_;
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
  if (materialization_state() == kFinished) {
    return *storage_;
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

    case kInt64: {
      bool is_smi = (int64_value() >= static_cast<int64_t>(Smi::kMinValue) &&
                     int64_value() <= static_cast<int64_t>(Smi::kMaxValue));
      if (is_smi) {
        return Smi::FromIntptr(static_cast<intptr_t>(int64_value()));
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
        return ReadOnlyRoots(isolate()).false_value();
      } else {
        CHECK_EQ(1U, uint32_value());
        return ReadOnlyRoots(isolate()).true_value();
      }
    }

    default:
      break;
  }

  // If we could not get the value without allocation, return the arguments
  // marker.
  return ReadOnlyRoots(isolate()).arguments_marker();
}

void TranslatedValue::set_initialized_storage(Handle<Object> storage) {
  DCHECK_EQ(kUninitialized, materialization_state());
  storage_ = storage;
  materialization_state_ = kFinished;
}

Handle<Object> TranslatedValue::GetValue() {
  // If we already have a value, then get it.
  if (materialization_state() == kFinished) return storage_;

  // Otherwise we have to materialize.
  switch (kind()) {
    case TranslatedValue::kTagged:
    case TranslatedValue::kInt32:
    case TranslatedValue::kInt64:
    case TranslatedValue::kUInt32:
    case TranslatedValue::kBoolBit:
    case TranslatedValue::kFloat:
    case TranslatedValue::kDouble: {
      MaterializeSimple();
      return storage_;
    }

    case TranslatedValue::kCapturedObject:
    case TranslatedValue::kDuplicatedObject: {
      // We need to materialize the object (or possibly even object graphs).
      // To make the object verifier happy, we materialize in two steps.

      // 1. Allocate storage for reachable objects. This makes sure that for
      //    each object we have allocated space on heap. The space will be
      //    a byte array that will be later initialized, or a fully
      //    initialized object if it is safe to allocate one that will
      //    pass the verifier.
      container_->EnsureObjectAllocatedAt(this);

      // 2. Initialize the objects. If we have allocated only byte arrays
      //    for some objects, we now overwrite the byte arrays with the
      //    correct object fields. Note that this phase does not allocate
      //    any new objects, so it does not trigger the object verifier.
      return container_->InitializeObjectAt(this);
    }

    case TranslatedValue::kInvalid:
      FATAL("unexpected case");
      return Handle<Object>::null();
  }

  FATAL("internal error: value missing");
  return Handle<Object>::null();
}

void TranslatedValue::MaterializeSimple() {
  // If we already have materialized, return.
  if (materialization_state() == kFinished) return;

  Object* raw_value = GetRawValue();
  if (raw_value != ReadOnlyRoots(isolate()).arguments_marker()) {
    // We can get the value without allocation, just return it here.
    set_initialized_storage(Handle<Object>(raw_value, isolate()));
    return;
  }

  switch (kind()) {
    case kInt32:
      set_initialized_storage(
          Handle<Object>(isolate()->factory()->NewNumber(int32_value())));
      return;

    case kInt64:
      set_initialized_storage(Handle<Object>(
          isolate()->factory()->NewNumber(static_cast<double>(int64_value()))));
      return;

    case kUInt32:
      set_initialized_storage(
          Handle<Object>(isolate()->factory()->NewNumber(uint32_value())));
      return;

    case kFloat: {
      double scalar_value = float_value().get_scalar();
      set_initialized_storage(
          Handle<Object>(isolate()->factory()->NewNumber(scalar_value)));
      return;
    }

    case kDouble: {
      double scalar_value = double_value().get_scalar();
      set_initialized_storage(
          Handle<Object>(isolate()->factory()->NewNumber(scalar_value)));
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

uint64_t TranslatedState::GetUInt64Slot(Address fp, int slot_offset) {
  return Memory<uint64_t>(fp + slot_offset);
}

uint32_t TranslatedState::GetUInt32Slot(Address fp, int slot_offset) {
  Address address = fp + slot_offset;
#if V8_TARGET_BIG_ENDIAN && V8_HOST_ARCH_64_BIT
  return Memory<uint32_t>(address + kIntSize);
#else
  return Memory<uint32_t>(address);
#endif
}

Float32 TranslatedState::GetFloatSlot(Address fp, int slot_offset) {
#if !V8_TARGET_ARCH_S390X && !V8_TARGET_ARCH_PPC64
  return Float32::FromBits(GetUInt32Slot(fp, slot_offset));
#else
  return Float32::FromBits(Memory<uint32_t>(fp + slot_offset));
#endif
}

Float64 TranslatedState::GetDoubleSlot(Address fp, int slot_offset) {
  return Float64::FromBits(GetUInt64Slot(fp, slot_offset));
}

void TranslatedValue::Handlify() {
  if (kind() == kTagged) {
    set_initialized_storage(Handle<Object>(raw_literal(), isolate()));
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

TranslatedFrame TranslatedFrame::JavaScriptBuiltinContinuationWithCatchFrame(
    BailoutId bailout_id, SharedFunctionInfo* shared_info, int height) {
  TranslatedFrame frame(kJavaScriptBuiltinContinuationWithCatch, shared_info,
                        height);
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
    case kJavaScriptBuiltinContinuationWithCatch:
      return 1 + height_;

    case kInvalid:
      UNREACHABLE();
      break;
  }
  UNREACHABLE();
}


void TranslatedFrame::Handlify() {
  if (raw_shared_info_ != nullptr) {
    shared_info_ = Handle<SharedFunctionInfo>(raw_shared_info_,
                                              raw_shared_info_->GetIsolate());
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
    case Translation::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME: {
      BailoutId bailout_id = BailoutId(iterator->Next());
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        std::unique_ptr<char[]> name = shared_info->DebugName()->ToCString();
        PrintF(trace_file,
               "  reading JavaScript builtin continuation frame with catch %s",
               name.get());
        PrintF(trace_file, " => bailout_id=%d, height=%d; inputs:\n",
               bailout_id.ToInt(), height);
      }
      // Add one to the height to account for the context which was implicitly
      // added to the translation during code generation.
      int height_with_context = height + 1;
      return TranslatedFrame::JavaScriptBuiltinContinuationWithCatchFrame(
          bailout_id, shared_info, height_with_context);
    }
    case Translation::UPDATE_FEEDBACK:
    case Translation::BEGIN:
    case Translation::DUPLICATED_OBJECT:
    case Translation::ARGUMENTS_ELEMENTS:
    case Translation::ARGUMENTS_LENGTH:
    case Translation::CAPTURED_OBJECT:
    case Translation::REGISTER:
    case Translation::INT32_REGISTER:
    case Translation::INT64_REGISTER:
    case Translation::UINT32_REGISTER:
    case Translation::BOOL_REGISTER:
    case Translation::FLOAT_REGISTER:
    case Translation::DOUBLE_REGISTER:
    case Translation::STACK_SLOT:
    case Translation::INT32_STACK_SLOT:
    case Translation::INT64_STACK_SLOT:
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
  intptr_t parent_frame_type = Memory<intptr_t>(
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

  ReadOnlyRoots roots(isolate_);
  frame.Add(TranslatedValue::NewTagged(this, roots.fixed_array_map()));
  frame.Add(TranslatedValue::NewInt32(this, length));

  int number_of_holes = 0;
  if (type == CreateArgumentsType::kMappedArguments) {
    // If the actual number of arguments is less than the number of formal
    // parameters, we have fewer holes to fill to not overshoot the length.
    number_of_holes = Min(formal_parameter_count_, length);
  }
  for (int i = 0; i < number_of_holes; ++i) {
    frame.Add(TranslatedValue::NewTagged(this, roots.the_hole_value()));
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
    case Translation::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME:
    case Translation::BUILTIN_CONTINUATION_FRAME:
    case Translation::UPDATE_FEEDBACK:
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
        PrintF(trace_file, V8PRIxPTR_FMT " ; %s ", value,
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
        PrintF(trace_file, "%" V8PRIdPTR " ; %s (int32)", value,
               converter.NameOfCPURegister(input_reg));
      }
      TranslatedValue translated_value =
          TranslatedValue::NewInt32(this, static_cast<int32_t>(value));
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::INT64_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) {
        TranslatedValue translated_value = TranslatedValue::NewInvalid(this);
        frame.Add(translated_value);
        return translated_value.GetChildrenCount();
      }
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%" V8PRIdPTR " ; %s (int64)", value,
               converter.NameOfCPURegister(input_reg));
      }
      TranslatedValue translated_value =
          TranslatedValue::NewInt64(this, static_cast<int64_t>(value));
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
        PrintF(trace_file, "%" V8PRIuPTR " ; %s (uint32)", value,
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
        PrintF(trace_file, V8PRIxPTR_FMT " ;  [fp %c %3d]  ", value,
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
        PrintF(trace_file, "%d ; (int32) [fp %c %3d] ",
               static_cast<int32_t>(value), slot_offset < 0 ? '-' : '+',
               std::abs(slot_offset));
      }
      TranslatedValue translated_value = TranslatedValue::NewInt32(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::INT64_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      uint64_t value = GetUInt64Slot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%" V8PRIdPTR " ; (int64) [fp %c %3d] ",
               static_cast<intptr_t>(value), slot_offset < 0 ? '-' : '+',
               std::abs(slot_offset));
      }
      TranslatedValue translated_value = TranslatedValue::NewInt64(this, value);
      frame.Add(translated_value);
      return translated_value.GetChildrenCount();
    }

    case Translation::UINT32_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      uint32_t value = GetUInt32Slot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%u ; (uint32) [fp %c %3d] ", value,
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
        PrintF(trace_file, "%u ; (bool) [fp %c %3d] ", value,
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
        PrintF(trace_file, "%e ; (float) [fp %c %3d] ", value.get_scalar(),
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
        PrintF(trace_file, V8PRIxPTR_FMT " ; (literal %2d) ",
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

TranslatedState::TranslatedState(const JavaScriptFrame* frame) {
  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationData* data =
      static_cast<const OptimizedFrame*>(frame)->GetDeoptimizationData(
          &deopt_index);
  DCHECK(data != nullptr && deopt_index != Safepoint::kNoDeoptimizationIndex);
  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Init(frame->isolate(), frame->fp(), &it, data->LiteralArray(),
       nullptr /* registers */, nullptr /* trace file */,
       frame->function()->shared()->internal_formal_parameter_count());
}

void TranslatedState::Init(Isolate* isolate, Address input_frame_pointer,
                           TranslationIterator* iterator,
                           FixedArray* literal_array, RegisterValues* registers,
                           FILE* trace_file, int formal_parameter_count) {
  DCHECK(frames_.empty());

  formal_parameter_count_ = formal_parameter_count;
  isolate_ = isolate;

  // Read out the 'header' translation.
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  CHECK(opcode == Translation::BEGIN);

  int count = iterator->Next();
  frames_.reserve(count);
  iterator->Next();  // Drop JS frames count.
  int update_feedback_count = iterator->Next();
  CHECK_GE(update_feedback_count, 0);
  CHECK_LE(update_feedback_count, 1);

  if (update_feedback_count == 1) {
    ReadUpdateFeedback(iterator, literal_array, trace_file);
  }

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

  if (feedback_vector_ != nullptr) {
    feedback_vector_handle_ =
        Handle<FeedbackVector>(feedback_vector_, isolate());
    feedback_vector_ = nullptr;
  }
  stack_frame_pointer_ = stack_frame_pointer;

  UpdateFromPreviouslyMaterializedObjects();
}

TranslatedValue* TranslatedState::GetValueByObjectIndex(int object_index) {
  CHECK_LT(static_cast<size_t>(object_index), object_positions_.size());
  TranslatedState::ObjectPosition pos = object_positions_[object_index];
  return &(frames_[pos.frame_index_].values_[pos.value_index_]);
}

Handle<Object> TranslatedState::InitializeObjectAt(TranslatedValue* slot) {
  slot = ResolveCapturedObject(slot);

  DisallowHeapAllocation no_allocation;
  if (slot->materialization_state() != TranslatedValue::kFinished) {
    std::stack<int> worklist;
    worklist.push(slot->object_index());
    slot->mark_finished();

    while (!worklist.empty()) {
      int index = worklist.top();
      worklist.pop();
      InitializeCapturedObjectAt(index, &worklist, no_allocation);
    }
  }
  return slot->GetStorage();
}

void TranslatedState::InitializeCapturedObjectAt(
    int object_index, std::stack<int>* worklist,
    const DisallowHeapAllocation& no_allocation) {
  CHECK_LT(static_cast<size_t>(object_index), object_positions_.size());
  TranslatedState::ObjectPosition pos = object_positions_[object_index];
  int value_index = pos.value_index_;

  TranslatedFrame* frame = &(frames_[pos.frame_index_]);
  TranslatedValue* slot = &(frame->values_[value_index]);
  value_index++;

  CHECK_EQ(TranslatedValue::kFinished, slot->materialization_state());
  CHECK_EQ(TranslatedValue::kCapturedObject, slot->kind());

  // Ensure all fields are initialized.
  int children_init_index = value_index;
  for (int i = 0; i < slot->GetChildrenCount(); i++) {
    // If the field is an object that has not been initialized yet, queue it
    // for initialization (and mark it as such).
    TranslatedValue* child_slot = frame->ValueAt(children_init_index);
    if (child_slot->kind() == TranslatedValue::kCapturedObject ||
        child_slot->kind() == TranslatedValue::kDuplicatedObject) {
      child_slot = ResolveCapturedObject(child_slot);
      if (child_slot->materialization_state() != TranslatedValue::kFinished) {
        DCHECK_EQ(TranslatedValue::kAllocated,
                  child_slot->materialization_state());
        worklist->push(child_slot->object_index());
        child_slot->mark_finished();
      }
    }
    SkipSlots(1, frame, &children_init_index);
  }

  // Read the map.
  // The map should never be materialized, so let us check we already have
  // an existing object here.
  CHECK_EQ(frame->values_[value_index].kind(), TranslatedValue::kTagged);
  Handle<Map> map = Handle<Map>::cast(frame->values_[value_index].GetValue());
  CHECK(map->IsMap());
  value_index++;

  // Handle the special cases.
  switch (map->instance_type()) {
    case MUTABLE_HEAP_NUMBER_TYPE:
    case FIXED_DOUBLE_ARRAY_TYPE:
      return;

    case FIXED_ARRAY_TYPE:
    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case NATIVE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
    case PROPERTY_ARRAY_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
      InitializeObjectWithTaggedFieldsAt(frame, &value_index, slot, map,
                                         no_allocation);
      break;

    default:
      CHECK(map->IsJSObjectMap());
      InitializeJSObjectAt(frame, &value_index, slot, map, no_allocation);
      break;
  }
  CHECK_EQ(value_index, children_init_index);
}

void TranslatedState::EnsureObjectAllocatedAt(TranslatedValue* slot) {
  slot = ResolveCapturedObject(slot);

  if (slot->materialization_state() == TranslatedValue::kUninitialized) {
    std::stack<int> worklist;
    worklist.push(slot->object_index());
    slot->mark_allocated();

    while (!worklist.empty()) {
      int index = worklist.top();
      worklist.pop();
      EnsureCapturedObjectAllocatedAt(index, &worklist);
    }
  }
}

void TranslatedState::MaterializeFixedDoubleArray(TranslatedFrame* frame,
                                                  int* value_index,
                                                  TranslatedValue* slot,
                                                  Handle<Map> map) {
  int length = Smi::cast(frame->values_[*value_index].GetRawValue())->value();
  (*value_index)++;
  Handle<FixedDoubleArray> array = Handle<FixedDoubleArray>::cast(
      isolate()->factory()->NewFixedDoubleArray(length));
  CHECK_GT(length, 0);
  for (int i = 0; i < length; i++) {
    CHECK_NE(TranslatedValue::kCapturedObject,
             frame->values_[*value_index].kind());
    Handle<Object> value = frame->values_[*value_index].GetValue();
    if (value->IsNumber()) {
      array->set(i, value->Number());
    } else {
      CHECK(value.is_identical_to(isolate()->factory()->the_hole_value()));
      array->set_the_hole(isolate(), i);
    }
    (*value_index)++;
  }
  slot->set_storage(array);
}

void TranslatedState::MaterializeMutableHeapNumber(TranslatedFrame* frame,
                                                   int* value_index,
                                                   TranslatedValue* slot) {
  CHECK_NE(TranslatedValue::kCapturedObject,
           frame->values_[*value_index].kind());
  Handle<Object> value = frame->values_[*value_index].GetValue();
  CHECK(value->IsNumber());
  Handle<MutableHeapNumber> box =
      isolate()->factory()->NewMutableHeapNumber(value->Number());
  (*value_index)++;
  slot->set_storage(box);
}

namespace {

enum DoubleStorageKind : uint8_t {
  kStoreTagged,
  kStoreUnboxedDouble,
  kStoreMutableHeapNumber,
};

}  // namespace

void TranslatedState::SkipSlots(int slots_to_skip, TranslatedFrame* frame,
                                int* value_index) {
  while (slots_to_skip > 0) {
    TranslatedValue* slot = &(frame->values_[*value_index]);
    (*value_index)++;
    slots_to_skip--;

    if (slot->kind() == TranslatedValue::kCapturedObject) {
      slots_to_skip += slot->GetChildrenCount();
    }
  }
}

void TranslatedState::EnsureCapturedObjectAllocatedAt(
    int object_index, std::stack<int>* worklist) {
  CHECK_LT(static_cast<size_t>(object_index), object_positions_.size());
  TranslatedState::ObjectPosition pos = object_positions_[object_index];
  int value_index = pos.value_index_;

  TranslatedFrame* frame = &(frames_[pos.frame_index_]);
  TranslatedValue* slot = &(frame->values_[value_index]);
  value_index++;

  CHECK_EQ(TranslatedValue::kAllocated, slot->materialization_state());
  CHECK_EQ(TranslatedValue::kCapturedObject, slot->kind());

  // Read the map.
  // The map should never be materialized, so let us check we already have
  // an existing object here.
  CHECK_EQ(frame->values_[value_index].kind(), TranslatedValue::kTagged);
  Handle<Map> map = Handle<Map>::cast(frame->values_[value_index].GetValue());
  CHECK(map->IsMap());
  value_index++;

  // Handle the special cases.
  switch (map->instance_type()) {
    case FIXED_DOUBLE_ARRAY_TYPE:
      // Materialize (i.e. allocate&initialize) the array and return since
      // there is no need to process the children.
      return MaterializeFixedDoubleArray(frame, &value_index, slot, map);

    case MUTABLE_HEAP_NUMBER_TYPE:
      // Materialize (i.e. allocate&initialize) the heap number and return.
      // There is no need to process the children.
      return MaterializeMutableHeapNumber(frame, &value_index, slot);

    case FIXED_ARRAY_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case NATIVE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE: {
      // Check we have the right size.
      int array_length =
          Smi::cast(frame->values_[value_index].GetRawValue())->value();

      int instance_size = FixedArray::SizeFor(array_length);
      CHECK_EQ(instance_size, slot->GetChildrenCount() * kPointerSize);

      // Canonicalize empty fixed array.
      if (*map == ReadOnlyRoots(isolate()).empty_fixed_array()->map() &&
          array_length == 0) {
        slot->set_storage(isolate()->factory()->empty_fixed_array());
      } else {
        slot->set_storage(AllocateStorageFor(slot));
      }

      // Make sure all the remaining children (after the map) are allocated.
      return EnsureChildrenAllocated(slot->GetChildrenCount() - 1, frame,
                                     &value_index, worklist);
    }

    case PROPERTY_ARRAY_TYPE: {
      // Check we have the right size.
      int length_or_hash =
          Smi::cast(frame->values_[value_index].GetRawValue())->value();
      int array_length = PropertyArray::LengthField::decode(length_or_hash);
      int instance_size = PropertyArray::SizeFor(array_length);
      CHECK_EQ(instance_size, slot->GetChildrenCount() * kPointerSize);

      slot->set_storage(AllocateStorageFor(slot));
      // Make sure all the remaining children (after the map) are allocated.
      return EnsureChildrenAllocated(slot->GetChildrenCount() - 1, frame,
                                     &value_index, worklist);
    }

    default:
      CHECK(map->IsJSObjectMap());
      EnsureJSObjectAllocated(slot, map);
      TranslatedValue* properties_slot = &(frame->values_[value_index]);
      value_index++;
      if (properties_slot->kind() == TranslatedValue::kCapturedObject) {
        // If we are materializing the property array, make sure we put
        // the mutable heap numbers at the right places.
        EnsurePropertiesAllocatedAndMarked(properties_slot, map);
        EnsureChildrenAllocated(properties_slot->GetChildrenCount(), frame,
                                &value_index, worklist);
      }
      // Make sure all the remaining children (after the map and properties) are
      // allocated.
      return EnsureChildrenAllocated(slot->GetChildrenCount() - 2, frame,
                                     &value_index, worklist);
  }
  UNREACHABLE();
}

void TranslatedState::EnsureChildrenAllocated(int count, TranslatedFrame* frame,
                                              int* value_index,
                                              std::stack<int>* worklist) {
  // Ensure all children are allocated.
  for (int i = 0; i < count; i++) {
    // If the field is an object that has not been allocated yet, queue it
    // for initialization (and mark it as such).
    TranslatedValue* child_slot = frame->ValueAt(*value_index);
    if (child_slot->kind() == TranslatedValue::kCapturedObject ||
        child_slot->kind() == TranslatedValue::kDuplicatedObject) {
      child_slot = ResolveCapturedObject(child_slot);
      if (child_slot->materialization_state() ==
          TranslatedValue::kUninitialized) {
        worklist->push(child_slot->object_index());
        child_slot->mark_allocated();
      }
    } else {
      // Make sure the simple values (heap numbers, etc.) are properly
      // initialized.
      child_slot->MaterializeSimple();
    }
    SkipSlots(1, frame, value_index);
  }
}

void TranslatedState::EnsurePropertiesAllocatedAndMarked(
    TranslatedValue* properties_slot, Handle<Map> map) {
  CHECK_EQ(TranslatedValue::kUninitialized,
           properties_slot->materialization_state());

  Handle<ByteArray> object_storage = AllocateStorageFor(properties_slot);
  properties_slot->mark_allocated();
  properties_slot->set_storage(object_storage);

  // Set markers for the double properties.
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
  int field_count = map->NumberOfOwnDescriptors();
  for (int i = 0; i < field_count; i++) {
    FieldIndex index = FieldIndex::ForDescriptor(*map, i);
    if (descriptors->GetDetails(i).representation().IsDouble() &&
        !index.is_inobject()) {
      CHECK(!map->IsUnboxedDoubleField(index));
      int outobject_index = index.outobject_array_index();
      int array_index = outobject_index * kPointerSize;
      object_storage->set(array_index, kStoreMutableHeapNumber);
    }
  }
}

Handle<ByteArray> TranslatedState::AllocateStorageFor(TranslatedValue* slot) {
  int allocate_size =
      ByteArray::LengthFor(slot->GetChildrenCount() * kPointerSize);
  // It is important to allocate all the objects tenured so that the marker
  // does not visit them.
  Handle<ByteArray> object_storage =
      isolate()->factory()->NewByteArray(allocate_size, TENURED);
  for (int i = 0; i < object_storage->length(); i++) {
    object_storage->set(i, kStoreTagged);
  }
  return object_storage;
}

void TranslatedState::EnsureJSObjectAllocated(TranslatedValue* slot,
                                              Handle<Map> map) {
  CHECK_EQ(map->instance_size(), slot->GetChildrenCount() * kPointerSize);

  Handle<ByteArray> object_storage = AllocateStorageFor(slot);
  // Now we handle the interesting (JSObject) case.
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
  int field_count = map->NumberOfOwnDescriptors();

  // Set markers for the double properties.
  for (int i = 0; i < field_count; i++) {
    FieldIndex index = FieldIndex::ForDescriptor(*map, i);
    if (descriptors->GetDetails(i).representation().IsDouble() &&
        index.is_inobject()) {
      CHECK_GE(index.index(), FixedArray::kHeaderSize / kPointerSize);
      int array_index = index.index() * kPointerSize - FixedArray::kHeaderSize;
      uint8_t marker = map->IsUnboxedDoubleField(index)
                           ? kStoreUnboxedDouble
                           : kStoreMutableHeapNumber;
      object_storage->set(array_index, marker);
    }
  }
  slot->set_storage(object_storage);
}

Handle<Object> TranslatedState::GetValueAndAdvance(TranslatedFrame* frame,
                                                   int* value_index) {
  TranslatedValue* slot = frame->ValueAt(*value_index);
  SkipSlots(1, frame, value_index);
  if (slot->kind() == TranslatedValue::kDuplicatedObject) {
    slot = ResolveCapturedObject(slot);
  }
  CHECK_NE(TranslatedValue::kUninitialized, slot->materialization_state());
  return slot->GetStorage();
}

void TranslatedState::InitializeJSObjectAt(
    TranslatedFrame* frame, int* value_index, TranslatedValue* slot,
    Handle<Map> map, const DisallowHeapAllocation& no_allocation) {
  Handle<HeapObject> object_storage = Handle<HeapObject>::cast(slot->storage_);
  DCHECK_EQ(TranslatedValue::kCapturedObject, slot->kind());

  // The object should have at least a map and some payload.
  CHECK_GE(slot->GetChildrenCount(), 2);

  // Notify the concurrent marker about the layout change.
  isolate()->heap()->NotifyObjectLayoutChange(
      *object_storage, slot->GetChildrenCount() * kPointerSize, no_allocation);

  // Fill the property array field.
  {
    Handle<Object> properties = GetValueAndAdvance(frame, value_index);
    WRITE_FIELD(*object_storage, JSObject::kPropertiesOrHashOffset,
                *properties);
    WRITE_BARRIER(*object_storage, JSObject::kPropertiesOrHashOffset,
                  *properties);
  }

  // For all the other fields we first look at the fixed array and check the
  // marker to see if we store an unboxed double.
  DCHECK_EQ(kPointerSize, JSObject::kPropertiesOrHashOffset);
  for (int i = 2; i < slot->GetChildrenCount(); i++) {
    // Initialize and extract the value from its slot.
    Handle<Object> field_value = GetValueAndAdvance(frame, value_index);

    // Read out the marker and ensure the field is consistent with
    // what the markers in the storage say (note that all heap numbers
    // should be fully initialized by now).
    int offset = i * kPointerSize;
    uint8_t marker = READ_UINT8_FIELD(*object_storage, offset);
    if (marker == kStoreUnboxedDouble) {
      double double_field_value;
      if (field_value->IsSmi()) {
        double_field_value = Smi::cast(*field_value)->value();
      } else {
        CHECK(field_value->IsHeapNumber());
        double_field_value = HeapNumber::cast(*field_value)->value();
      }
      WRITE_DOUBLE_FIELD(*object_storage, offset, double_field_value);
    } else if (marker == kStoreMutableHeapNumber) {
      CHECK(field_value->IsMutableHeapNumber());
      WRITE_FIELD(*object_storage, offset, *field_value);
      WRITE_BARRIER(*object_storage, offset, *field_value);
    } else {
      CHECK_EQ(kStoreTagged, marker);
      WRITE_FIELD(*object_storage, offset, *field_value);
      WRITE_BARRIER(*object_storage, offset, *field_value);
    }
  }
  object_storage->synchronized_set_map(*map);
}

void TranslatedState::InitializeObjectWithTaggedFieldsAt(
    TranslatedFrame* frame, int* value_index, TranslatedValue* slot,
    Handle<Map> map, const DisallowHeapAllocation& no_allocation) {
  Handle<HeapObject> object_storage = Handle<HeapObject>::cast(slot->storage_);

  // Skip the writes if we already have the canonical empty fixed array.
  if (*object_storage == ReadOnlyRoots(isolate()).empty_fixed_array()) {
    CHECK_EQ(2, slot->GetChildrenCount());
    Handle<Object> length_value = GetValueAndAdvance(frame, value_index);
    CHECK_EQ(*length_value, Smi::FromInt(0));
    return;
  }

  // Notify the concurrent marker about the layout change.
  isolate()->heap()->NotifyObjectLayoutChange(
      *object_storage, slot->GetChildrenCount() * kPointerSize, no_allocation);

  // Write the fields to the object.
  for (int i = 1; i < slot->GetChildrenCount(); i++) {
    Handle<Object> field_value = GetValueAndAdvance(frame, value_index);
    int offset = i * kPointerSize;
    uint8_t marker = READ_UINT8_FIELD(*object_storage, offset);
    if (i > 1 && marker == kStoreMutableHeapNumber) {
      CHECK(field_value->IsMutableHeapNumber());
    } else {
      CHECK(marker == kStoreTagged || i == 1);
      CHECK(!field_value->IsMutableHeapNumber());
    }

    WRITE_FIELD(*object_storage, offset, *field_value);
    WRITE_BARRIER(*object_storage, offset, *field_value);
  }

  object_storage->synchronized_set_map(*map);
}

TranslatedValue* TranslatedState::ResolveCapturedObject(TranslatedValue* slot) {
  while (slot->kind() == TranslatedValue::kDuplicatedObject) {
    slot = GetValueByObjectIndex(slot->object_index());
  }
  CHECK_EQ(TranslatedValue::kCapturedObject, slot->kind());
  return slot;
}

TranslatedFrame* TranslatedState::GetFrameFromJSFrameIndex(int jsframe_index) {
  for (size_t i = 0; i < frames_.size(); i++) {
    if (frames_[i].kind() == TranslatedFrame::kInterpretedFunction ||
        frames_[i].kind() == TranslatedFrame::kJavaScriptBuiltinContinuation ||
        frames_[i].kind() ==
            TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch) {
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
        frames_[i].kind() == TranslatedFrame::kJavaScriptBuiltinContinuation ||
        frames_[i].kind() ==
            TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch) {
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
        isolate_->factory()->NewFixedArray(length, TENURED);
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

    // Skip duplicate objects (i.e., those that point to some
    // other object id).
    if (value_info->object_index() != i) continue;

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

      if (value_info->kind() == TranslatedValue::kCapturedObject) {
        value_info->set_initialized_storage(
            Handle<Object>(previously_materialized_objects->get(i), isolate_));
      }
    }
  }
}

void TranslatedState::VerifyMaterializedObjects() {
#if VERIFY_HEAP
  int length = static_cast<int>(object_positions_.size());
  for (int i = 0; i < length; i++) {
    TranslatedValue* slot = GetValueByObjectIndex(i);
    if (slot->kind() == TranslatedValue::kCapturedObject) {
      CHECK_EQ(slot, GetValueByObjectIndex(slot->object_index()));
      if (slot->materialization_state() == TranslatedValue::kFinished) {
        slot->GetStorage()->ObjectVerify(isolate());
      } else {
        CHECK_EQ(slot->materialization_state(),
                 TranslatedValue::kUninitialized);
      }
    }
  }
#endif
}

bool TranslatedState::DoUpdateFeedback() {
  if (!feedback_vector_handle_.is_null()) {
    CHECK(!feedback_slot_.IsInvalid());
    isolate()->CountUsage(v8::Isolate::kDeoptimizerDisableSpeculation);
    FeedbackNexus nexus(feedback_vector_handle_, feedback_slot_);
    nexus.SetSpeculationMode(SpeculationMode::kDisallowSpeculation);
    return true;
  }
  return false;
}

void TranslatedState::ReadUpdateFeedback(TranslationIterator* iterator,
                                         FixedArray* literal_array,
                                         FILE* trace_file) {
  CHECK_EQ(Translation::UPDATE_FEEDBACK, iterator->Next());
  feedback_vector_ = FeedbackVector::cast(literal_array->get(iterator->Next()));
  feedback_slot_ = FeedbackSlot(iterator->Next());
  if (trace_file != nullptr) {
    PrintF(trace_file, "  reading FeedbackVector (slot %d)\n",
           feedback_slot_.ToInt());
  }
}

}  // namespace internal
}  // namespace v8

// Undefine the heap manipulation macros.
#include "src/objects/object-macros-undef.h"
