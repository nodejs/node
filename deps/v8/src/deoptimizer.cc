// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer.h"

#include "src/accessors.h"
#include "src/ast/prettyprinter.h"
#include "src/codegen.h"
#include "src/disasm.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/global-handles.h"
#include "src/macro-assembler.h"
#include "src/profiler/cpu-profiler.h"
#include "src/v8.h"


namespace v8 {
namespace internal {

static MemoryChunk* AllocateCodeChunk(MemoryAllocator* allocator) {
  return allocator->AllocateChunk(Deoptimizer::GetMaxDeoptTableSize(),
                                  base::OS::CommitPageSize(),
#if defined(__native_client__)
  // The Native Client port of V8 uses an interpreter,
  // so code pages don't need PROT_EXEC.
                                  NOT_EXECUTABLE,
#else
                                  EXECUTABLE,
#endif
                                  NULL);
}


DeoptimizerData::DeoptimizerData(MemoryAllocator* allocator)
    : allocator_(allocator),
      deoptimized_frame_info_(NULL),
      current_(NULL) {
  for (int i = 0; i < Deoptimizer::kBailoutTypesWithCodeEntry; ++i) {
    deopt_entry_code_entries_[i] = -1;
    deopt_entry_code_[i] = AllocateCodeChunk(allocator);
  }
}


DeoptimizerData::~DeoptimizerData() {
  for (int i = 0; i < Deoptimizer::kBailoutTypesWithCodeEntry; ++i) {
    allocator_->Free(deopt_entry_code_[i]);
    deopt_entry_code_[i] = NULL;
  }
}


void DeoptimizerData::Iterate(ObjectVisitor* v) {
  if (deoptimized_frame_info_ != NULL) {
    deoptimized_frame_info_->Iterate(v);
  }
}


Code* Deoptimizer::FindDeoptimizingCode(Address addr) {
  if (function_->IsHeapObject()) {
    // Search all deoptimizing code in the native context of the function.
    Context* native_context = function_->context()->native_context();
    Object* element = native_context->DeoptimizedCodeListHead();
    while (!element->IsUndefined()) {
      Code* code = Code::cast(element);
      CHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
      if (code->contains(addr)) return code;
      element = code->next_code_link();
    }
  }
  return NULL;
}


// We rely on this function not causing a GC.  It is called from generated code
// without having a real stack frame in place.
Deoptimizer* Deoptimizer::New(JSFunction* function,
                              BailoutType type,
                              unsigned bailout_id,
                              Address from,
                              int fp_to_sp_delta,
                              Isolate* isolate) {
  Deoptimizer* deoptimizer = new Deoptimizer(isolate,
                                             function,
                                             type,
                                             bailout_id,
                                             from,
                                             fp_to_sp_delta,
                                             NULL);
  CHECK(isolate->deoptimizer_data()->current_ == NULL);
  isolate->deoptimizer_data()->current_ = deoptimizer;
  return deoptimizer;
}


// No larger than 2K on all platforms
static const int kDeoptTableMaxEpilogueCodeSize = 2 * KB;


size_t Deoptimizer::GetMaxDeoptTableSize() {
  int entries_size =
      Deoptimizer::kMaxNumberOfEntries * Deoptimizer::table_entry_size_;
  int commit_page_size = static_cast<int>(base::OS::CommitPageSize());
  int page_count = ((kDeoptTableMaxEpilogueCodeSize + entries_size - 1) /
                    commit_page_size) + 1;
  return static_cast<size_t>(commit_page_size * page_count);
}


Deoptimizer* Deoptimizer::Grab(Isolate* isolate) {
  Deoptimizer* result = isolate->deoptimizer_data()->current_;
  CHECK_NOT_NULL(result);
  result->DeleteFrameDescriptions();
  isolate->deoptimizer_data()->current_ = NULL;
  return result;
}


int Deoptimizer::ConvertJSFrameIndexToFrameIndex(int jsframe_index) {
  if (jsframe_index == 0) return 0;

  int frame_index = 0;
  while (jsframe_index >= 0) {
    FrameDescription* frame = output_[frame_index];
    if (frame->GetFrameType() == StackFrame::JAVA_SCRIPT) {
      jsframe_index--;
    }
    frame_index++;
  }

  return frame_index - 1;
}


DeoptimizedFrameInfo* Deoptimizer::DebuggerInspectableFrame(
    JavaScriptFrame* frame,
    int jsframe_index,
    Isolate* isolate) {
  CHECK(frame->is_optimized());
  CHECK(isolate->deoptimizer_data()->deoptimized_frame_info_ == NULL);

  // Get the function and code from the frame.
  JSFunction* function = frame->function();
  Code* code = frame->LookupCode();

  // Locate the deoptimization point in the code. As we are at a call the
  // return address must be at a place in the code with deoptimization support.
  SafepointEntry safepoint_entry = code->GetSafepointEntry(frame->pc());
  int deoptimization_index = safepoint_entry.deoptimization_index();
  CHECK_NE(deoptimization_index, Safepoint::kNoDeoptimizationIndex);

  // Always use the actual stack slots when calculating the fp to sp
  // delta adding two for the function and context.
  unsigned stack_slots = code->stack_slots();
  unsigned arguments_stack_height =
      Deoptimizer::ComputeOutgoingArgumentSize(code, deoptimization_index);
  unsigned fp_to_sp_delta = (stack_slots * kPointerSize) +
                            StandardFrameConstants::kFixedFrameSizeFromFp +
                            arguments_stack_height;

  Deoptimizer* deoptimizer = new Deoptimizer(isolate,
                                             function,
                                             Deoptimizer::DEBUGGER,
                                             deoptimization_index,
                                             frame->pc(),
                                             fp_to_sp_delta,
                                             code);
  Address tos = frame->fp() - fp_to_sp_delta;
  deoptimizer->FillInputFrame(tos, frame);

  // Calculate the output frames.
  Deoptimizer::ComputeOutputFrames(deoptimizer);

  // Create the GC safe output frame information and register it for GC
  // handling.
  CHECK_LT(jsframe_index, deoptimizer->jsframe_count());

  // Convert JS frame index into frame index.
  int frame_index = deoptimizer->ConvertJSFrameIndexToFrameIndex(jsframe_index);

  bool has_arguments_adaptor =
      frame_index > 0 &&
      deoptimizer->output_[frame_index - 1]->GetFrameType() ==
      StackFrame::ARGUMENTS_ADAPTOR;

  int construct_offset = has_arguments_adaptor ? 2 : 1;
  bool has_construct_stub =
      frame_index >= construct_offset &&
      deoptimizer->output_[frame_index - construct_offset]->GetFrameType() ==
      StackFrame::CONSTRUCT;

  DeoptimizedFrameInfo* info = new DeoptimizedFrameInfo(deoptimizer,
                                                        frame_index,
                                                        has_arguments_adaptor,
                                                        has_construct_stub);
  isolate->deoptimizer_data()->deoptimized_frame_info_ = info;

  // Done with the GC-unsafe frame descriptions. This re-enables allocation.
  deoptimizer->DeleteFrameDescriptions();

  // Allocate a heap number for the doubles belonging to this frame.
  deoptimizer->MaterializeHeapNumbersForDebuggerInspectableFrame(
      frame_index, info->parameters_count(), info->expression_count(), info);

  // Finished using the deoptimizer instance.
  delete deoptimizer;

  return info;
}


void Deoptimizer::DeleteDebuggerInspectableFrame(DeoptimizedFrameInfo* info,
                                                 Isolate* isolate) {
  CHECK_EQ(isolate->deoptimizer_data()->deoptimized_frame_info_, info);
  delete info;
  isolate->deoptimizer_data()->deoptimized_frame_info_ = NULL;
}


void Deoptimizer::GenerateDeoptimizationEntries(MacroAssembler* masm,
                                                int count,
                                                BailoutType type) {
  TableEntryGenerator generator(masm, type, count);
  generator.Generate();
}


void Deoptimizer::VisitAllOptimizedFunctionsForContext(
    Context* context, OptimizedFunctionVisitor* visitor) {
  DisallowHeapAllocation no_allocation;

  CHECK(context->IsNativeContext());

  visitor->EnterContext(context);

  // Visit the list of optimized functions, removing elements that
  // no longer refer to optimized code.
  JSFunction* prev = NULL;
  Object* element = context->OptimizedFunctionsListHead();
  while (!element->IsUndefined()) {
    JSFunction* function = JSFunction::cast(element);
    Object* next = function->next_function_link();
    if (function->code()->kind() != Code::OPTIMIZED_FUNCTION ||
        (visitor->VisitFunction(function),
         function->code()->kind() != Code::OPTIMIZED_FUNCTION)) {
      // The function no longer refers to optimized code, or the visitor
      // changed the code to which it refers to no longer be optimized code.
      // Remove the function from this list.
      if (prev != NULL) {
        prev->set_next_function_link(next, UPDATE_WEAK_WRITE_BARRIER);
      } else {
        context->SetOptimizedFunctionsListHead(next);
      }
      // The visitor should not alter the link directly.
      CHECK_EQ(function->next_function_link(), next);
      // Set the next function link to undefined to indicate it is no longer
      // in the optimized functions list.
      function->set_next_function_link(context->GetHeap()->undefined_value(),
                                       SKIP_WRITE_BARRIER);
    } else {
      // The visitor should not alter the link directly.
      CHECK_EQ(function->next_function_link(), next);
      // preserve this element.
      prev = function;
    }
    element = next;
  }

  visitor->LeaveContext(context);
}


void Deoptimizer::VisitAllOptimizedFunctions(
    Isolate* isolate,
    OptimizedFunctionVisitor* visitor) {
  DisallowHeapAllocation no_allocation;

  // Run through the list of all native contexts.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined()) {
    VisitAllOptimizedFunctionsForContext(Context::cast(context), visitor);
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


// Unlink functions referring to code marked for deoptimization, then move
// marked code from the optimized code list to the deoptimized code list,
// and patch code for lazy deopt.
void Deoptimizer::DeoptimizeMarkedCodeForContext(Context* context) {
  DisallowHeapAllocation no_allocation;

  // A "closure" that unlinks optimized code that is going to be
  // deoptimized from the functions that refer to it.
  class SelectedCodeUnlinker: public OptimizedFunctionVisitor {
   public:
    virtual void EnterContext(Context* context) { }  // Don't care.
    virtual void LeaveContext(Context* context)  { }  // Don't care.
    virtual void VisitFunction(JSFunction* function) {
      Code* code = function->code();
      if (!code->marked_for_deoptimization()) return;

      // Unlink this function and evict from optimized code map.
      SharedFunctionInfo* shared = function->shared();
      function->set_code(shared->code());

      if (FLAG_trace_deopt) {
        CodeTracer::Scope scope(code->GetHeap()->isolate()->GetCodeTracer());
        PrintF(scope.file(), "[deoptimizer unlinked: ");
        function->PrintName(scope.file());
        PrintF(scope.file(),
               " / %" V8PRIxPTR "]\n", reinterpret_cast<intptr_t>(function));
      }
    }
  };

  // Unlink all functions that refer to marked code.
  SelectedCodeUnlinker unlinker;
  VisitAllOptimizedFunctionsForContext(context, &unlinker);

  Isolate* isolate = context->GetHeap()->isolate();
#ifdef DEBUG
  Code* topmost_optimized_code = NULL;
  bool safe_to_deopt_topmost_optimized_code = false;
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
      bool turbofanned = code->is_turbofanned() &&
                         function->shared()->asm_function() &&
                         !FLAG_turbo_asm_deoptimization;
      bool safe_to_deopt =
          deopt_index != Safepoint::kNoDeoptimizationIndex || turbofanned;
      CHECK(topmost_optimized_code == NULL || safe_to_deopt || turbofanned);
      if (topmost_optimized_code == NULL) {
        topmost_optimized_code = code;
        safe_to_deopt_topmost_optimized_code = safe_to_deopt;
      }
    }
  }
#endif

  // Move marked code from the optimized code list to the deoptimized
  // code list, collecting them into a ZoneList.
  Zone zone;
  ZoneList<Code*> codes(10, &zone);

  // Walk over all optimized code objects in this native context.
  Code* prev = NULL;
  Object* element = context->OptimizedCodeListHead();
  while (!element->IsUndefined()) {
    Code* code = Code::cast(element);
    CHECK_EQ(code->kind(), Code::OPTIMIZED_FUNCTION);
    Object* next = code->next_code_link();

    if (code->marked_for_deoptimization()) {
      // Put the code into the list for later patching.
      codes.Add(code, &zone);

      if (prev != NULL) {
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

  // TODO(titzer): we need a handle scope only because of the macro assembler,
  // which is only used in EnsureCodeForDeoptimizationEntry.
  HandleScope scope(isolate);

  // Now patch all the codes for deoptimization.
  for (int i = 0; i < codes.length(); i++) {
#ifdef DEBUG
    if (codes[i] == topmost_optimized_code) {
      DCHECK(safe_to_deopt_topmost_optimized_code);
    }
#endif
    // It is finally time to die, code object.

    // Remove the code from optimized code map.
    DeoptimizationInputData* deopt_data =
        DeoptimizationInputData::cast(codes[i]->deoptimization_data());
    SharedFunctionInfo* shared =
        SharedFunctionInfo::cast(deopt_data->SharedFunctionInfo());
    shared->EvictFromOptimizedCodeMap(codes[i], "deoptimized code");

    // Do platform-specific patching to force any activations to lazy deopt.
    PatchCodeForDeoptimization(isolate, codes[i]);

    // We might be in the middle of incremental marking with compaction.
    // Tell collector to treat this code object in a special way and
    // ignore all slots that might have been recorded on it.
    isolate->heap()->mark_compact_collector()->InvalidateCode(codes[i]);
  }
}


void Deoptimizer::DeoptimizeAll(Isolate* isolate) {
  if (FLAG_trace_deopt) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[deoptimize all code in all contexts]\n");
  }
  DisallowHeapAllocation no_allocation;
  // For all contexts, mark all code, then deoptimize.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined()) {
    Context* native_context = Context::cast(context);
    MarkAllCodeForContext(native_context);
    DeoptimizeMarkedCodeForContext(native_context);
    context = native_context->get(Context::NEXT_CONTEXT_LINK);
  }
}


void Deoptimizer::DeoptimizeMarkedCode(Isolate* isolate) {
  if (FLAG_trace_deopt) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[deoptimize marked code in all contexts]\n");
  }
  DisallowHeapAllocation no_allocation;
  // For all contexts, deoptimize code already marked.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined()) {
    Context* native_context = Context::cast(context);
    DeoptimizeMarkedCodeForContext(native_context);
    context = native_context->get(Context::NEXT_CONTEXT_LINK);
  }
}


void Deoptimizer::MarkAllCodeForContext(Context* context) {
  Object* element = context->OptimizedCodeListHead();
  while (!element->IsUndefined()) {
    Code* code = Code::cast(element);
    CHECK_EQ(code->kind(), Code::OPTIMIZED_FUNCTION);
    code->set_marked_for_deoptimization(true);
    element = code->next_code_link();
  }
}


void Deoptimizer::DeoptimizeFunction(JSFunction* function) {
  Code* code = function->code();
  if (code->kind() == Code::OPTIMIZED_FUNCTION) {
    // Mark the code for deoptimization and unlink any functions that also
    // refer to that code. The code cannot be shared across native contexts,
    // so we only need to search one.
    code->set_marked_for_deoptimization(true);
    DeoptimizeMarkedCodeForContext(function->context()->native_context());
  }
}


void Deoptimizer::ComputeOutputFrames(Deoptimizer* deoptimizer) {
  deoptimizer->DoComputeOutputFrames();
}


bool Deoptimizer::TraceEnabledFor(BailoutType deopt_type,
                                  StackFrame::Type frame_type) {
  switch (deopt_type) {
    case EAGER:
    case SOFT:
    case LAZY:
    case DEBUGGER:
      return (frame_type == StackFrame::STUB)
          ? FLAG_trace_stub_failures
          : FLAG_trace_deopt;
  }
  FATAL("Unsupported deopt type");
  return false;
}


const char* Deoptimizer::MessageFor(BailoutType type) {
  switch (type) {
    case EAGER: return "eager";
    case SOFT: return "soft";
    case LAZY: return "lazy";
    case DEBUGGER: return "debugger";
  }
  FATAL("Unsupported deopt type");
  return NULL;
}


Deoptimizer::Deoptimizer(Isolate* isolate, JSFunction* function,
                         BailoutType type, unsigned bailout_id, Address from,
                         int fp_to_sp_delta, Code* optimized_code)
    : isolate_(isolate),
      function_(function),
      bailout_id_(bailout_id),
      bailout_type_(type),
      from_(from),
      fp_to_sp_delta_(fp_to_sp_delta),
      has_alignment_padding_(0),
      input_(nullptr),
      output_count_(0),
      jsframe_count_(0),
      output_(nullptr),
      trace_scope_(nullptr) {
  // For COMPILED_STUBs called from builtins, the function pointer is a SMI
  // indicating an internal frame.
  if (function->IsSmi()) {
    function = nullptr;
  }
  DCHECK(from != nullptr);
  if (function != nullptr && function->IsOptimized()) {
    function->shared()->increment_deopt_count();
    if (bailout_type_ == Deoptimizer::SOFT) {
      isolate->counters()->soft_deopts_executed()->Increment();
      // Soft deopts shouldn't count against the overall re-optimization count
      // that can eventually lead to disabling optimization for a function.
      int opt_count = function->shared()->opt_count();
      if (opt_count > 0) opt_count--;
      function->shared()->set_opt_count(opt_count);
    }
  }
  compiled_code_ = FindOptimizedCode(function, optimized_code);
#if DEBUG
  DCHECK(compiled_code_ != NULL);
  if (type == EAGER || type == SOFT || type == LAZY) {
    DCHECK(compiled_code_->kind() != Code::FUNCTION);
  }
#endif

  StackFrame::Type frame_type = function == NULL
      ? StackFrame::STUB
      : StackFrame::JAVA_SCRIPT;
  trace_scope_ = TraceEnabledFor(type, frame_type) ?
      new CodeTracer::Scope(isolate->GetCodeTracer()) : NULL;
#ifdef DEBUG
  CHECK(AllowHeapAllocation::IsAllowed());
  disallow_heap_allocation_ = new DisallowHeapAllocation();
#endif  // DEBUG
  if (compiled_code_->kind() == Code::OPTIMIZED_FUNCTION) {
    PROFILE(isolate_, CodeDeoptEvent(compiled_code_, from_, fp_to_sp_delta_));
  }
  unsigned size = ComputeInputFrameSize();
  input_ = new(size) FrameDescription(size, function);
  input_->SetFrameType(frame_type);
}


Code* Deoptimizer::FindOptimizedCode(JSFunction* function,
                                     Code* optimized_code) {
  switch (bailout_type_) {
    case Deoptimizer::SOFT:
    case Deoptimizer::EAGER:
    case Deoptimizer::LAZY: {
      Code* compiled_code = FindDeoptimizingCode(from_);
      return (compiled_code == NULL)
          ? static_cast<Code*>(isolate_->FindCodeObject(from_))
          : compiled_code;
    }
    case Deoptimizer::DEBUGGER:
      DCHECK(optimized_code->contains(from_));
      return optimized_code;
  }
  FATAL("Could not find code for optimized function");
  return NULL;
}


void Deoptimizer::PrintFunctionName() {
  if (function_->IsJSFunction()) {
    function_->ShortPrint(trace_scope_->file());
  } else {
    PrintF(trace_scope_->file(),
           "%s", Code::Kind2String(compiled_code_->kind()));
  }
}


Deoptimizer::~Deoptimizer() {
  DCHECK(input_ == NULL && output_ == NULL);
  DCHECK(disallow_heap_allocation_ == NULL);
  delete trace_scope_;
}


void Deoptimizer::DeleteFrameDescriptions() {
  delete input_;
  for (int i = 0; i < output_count_; ++i) {
    if (output_[i] != input_) delete output_[i];
  }
  delete[] output_;
  input_ = NULL;
  output_ = NULL;
#ifdef DEBUG
  CHECK(!AllowHeapAllocation::IsAllowed());
  CHECK(disallow_heap_allocation_ != NULL);
  delete disallow_heap_allocation_;
  disallow_heap_allocation_ = NULL;
#endif  // DEBUG
}


Address Deoptimizer::GetDeoptimizationEntry(Isolate* isolate,
                                            int id,
                                            BailoutType type,
                                            GetEntryMode mode) {
  CHECK_GE(id, 0);
  if (id >= kMaxNumberOfEntries) return NULL;
  if (mode == ENSURE_ENTRY_CODE) {
    EnsureCodeForDeoptimizationEntry(isolate, type, id);
  } else {
    CHECK_EQ(mode, CALCULATE_ENTRY_ADDRESS);
  }
  DeoptimizerData* data = isolate->deoptimizer_data();
  CHECK_LT(type, kBailoutTypesWithCodeEntry);
  MemoryChunk* base = data->deopt_entry_code_[type];
  return base->area_start() + (id * table_entry_size_);
}


int Deoptimizer::GetDeoptimizationId(Isolate* isolate,
                                     Address addr,
                                     BailoutType type) {
  DeoptimizerData* data = isolate->deoptimizer_data();
  MemoryChunk* base = data->deopt_entry_code_[type];
  Address start = base->area_start();
  if (addr < start ||
      addr >= start + (kMaxNumberOfEntries * table_entry_size_)) {
    return kNotDeoptimizationEntry;
  }
  DCHECK_EQ(0,
            static_cast<int>(addr - start) % table_entry_size_);
  return static_cast<int>(addr - start) / table_entry_size_;
}


int Deoptimizer::GetOutputInfo(DeoptimizationOutputData* data,
                               BailoutId id,
                               SharedFunctionInfo* shared) {
  // TODO(kasperl): For now, we do a simple linear search for the PC
  // offset associated with the given node id. This should probably be
  // changed to a binary search.
  int length = data->DeoptPoints();
  for (int i = 0; i < length; i++) {
    if (data->AstId(i) == id) {
      return data->PcAndState(i)->value();
    }
  }
  OFStream os(stderr);
  os << "[couldn't find pc offset for node=" << id.ToInt() << "]\n"
     << "[method: " << shared->DebugName()->ToCString().get() << "]\n"
     << "[source:\n" << SourceCodeOf(shared) << "\n]" << std::endl;

  shared->GetHeap()->isolate()->PushStackTraceAndDie(0xfefefefe, data, shared,
                                                     0xfefefeff);
  FATAL("unable to find pc offset during deoptimization");
  return -1;
}


int Deoptimizer::GetDeoptimizedCodeCount(Isolate* isolate) {
  int length = 0;
  // Count all entries in the deoptimizing code list of every context.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined()) {
    Context* native_context = Context::cast(context);
    Object* element = native_context->DeoptimizedCodeListHead();
    while (!element->IsUndefined()) {
      Code* code = Code::cast(element);
      DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
      length++;
      element = code->next_code_link();
    }
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
  return length;
}


// We rely on this function not causing a GC.  It is called from generated code
// without having a real stack frame in place.
void Deoptimizer::DoComputeOutputFrames() {
  base::ElapsedTimer timer;

  // Determine basic deoptimization information.  The optimized frame is
  // described by the input data.
  DeoptimizationInputData* input_data =
      DeoptimizationInputData::cast(compiled_code_->deoptimization_data());

  if (trace_scope_ != NULL) {
    timer.Start();
    PrintF(trace_scope_->file(), "[deoptimizing (DEOPT %s): begin ",
           MessageFor(bailout_type_));
    PrintFunctionName();
    PrintF(trace_scope_->file(),
           " (opt #%d) @%d, FP to SP delta: %d]\n",
           input_data->OptimizationId()->value(),
           bailout_id_,
           fp_to_sp_delta_);
    if (bailout_type_ == EAGER || bailout_type_ == SOFT ||
        (compiled_code_->is_hydrogen_stub())) {
      compiled_code_->PrintDeoptLocation(trace_scope_->file(), from_);
    }
  }

  BailoutId node_id = input_data->AstId(bailout_id_);
  ByteArray* translations = input_data->TranslationByteArray();
  unsigned translation_index =
      input_data->TranslationIndex(bailout_id_)->value();

  TranslationIterator state_iterator(translations, translation_index);
  translated_state_.Init(
      input_->GetFramePointerAddress(), &state_iterator,
      input_data->LiteralArray(), input_->GetRegisterValues(),
      trace_scope_ == nullptr ? nullptr : trace_scope_->file());

  // Do the input frame to output frame(s) translation.
  size_t count = translated_state_.frames().size();
  DCHECK(output_ == NULL);
  output_ = new FrameDescription*[count];
  for (size_t i = 0; i < count; ++i) {
    output_[i] = NULL;
  }
  output_count_ = static_cast<int>(count);

  Register fp_reg = JavaScriptFrame::fp_register();
  stack_fp_ = reinterpret_cast<Address>(
      input_->GetRegister(fp_reg.code()) +
          has_alignment_padding_ * kPointerSize);

  // Translate each output frame.
  for (size_t i = 0; i < count; ++i) {
    // Read the ast node id, function, and frame height for this output frame.
    int frame_index = static_cast<int>(i);
    switch (translated_state_.frames()[i].kind()) {
      case TranslatedFrame::kFunction:
        DoComputeJSFrame(frame_index);
        jsframe_count_++;
        break;
      case TranslatedFrame::kInterpretedFunction:
        DoComputeInterpretedFrame(frame_index);
        jsframe_count_++;
        break;
      case TranslatedFrame::kArgumentsAdaptor:
        DoComputeArgumentsAdaptorFrame(frame_index);
        break;
      case TranslatedFrame::kConstructStub:
        DoComputeConstructStubFrame(frame_index);
        break;
      case TranslatedFrame::kGetter:
        DoComputeAccessorStubFrame(frame_index, false);
        break;
      case TranslatedFrame::kSetter:
        DoComputeAccessorStubFrame(frame_index, true);
        break;
      case TranslatedFrame::kCompiledStub:
        DoComputeCompiledStubFrame(frame_index);
        break;
      case TranslatedFrame::kInvalid:
        FATAL("invalid frame");
        break;
    }
  }

  // Print some helpful diagnostic information.
  if (trace_scope_ != NULL) {
    double ms = timer.Elapsed().InMillisecondsF();
    int index = output_count_ - 1;  // Index of the topmost frame.
    PrintF(trace_scope_->file(), "[deoptimizing (%s): end ",
           MessageFor(bailout_type_));
    PrintFunctionName();
    PrintF(trace_scope_->file(),
           " @%d => node=%d, pc=0x%08" V8PRIxPTR ", state=%s, alignment=%s,"
           " took %0.3f ms]\n",
           bailout_id_,
           node_id.ToInt(),
           output_[index]->GetPc(),
           FullCodeGenerator::State2String(
               static_cast<FullCodeGenerator::State>(
                   output_[index]->GetState()->value())),
           has_alignment_padding_ ? "with padding" : "no padding",
           ms);
  }
}


void Deoptimizer::DoComputeJSFrame(int frame_index) {
  TranslatedFrame* translated_frame =
      &(translated_state_.frames()[frame_index]);
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  int input_index = 0;

  BailoutId node_id = translated_frame->node_id();
  unsigned height =
      translated_frame->height() - 1;  // Do not count the context.
  unsigned height_in_bytes = height * kPointerSize;
  JSFunction* function = JSFunction::cast(value_iterator->GetRawValue());
  value_iterator++;
  input_index++;
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(), "  translating frame ");
    function->PrintName(trace_scope_->file());
    PrintF(trace_scope_->file(),
           " => node=%d, height=%d\n", node_id.ToInt(), height_in_bytes);
  }

  // The 'fixed' part of the frame consists of the incoming parameters and
  // the part described by JavaScriptFrameConstants.
  unsigned fixed_frame_size = ComputeJavascriptFixedSize(function);
  unsigned input_frame_size = input_->GetFrameSize();
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, function);
  output_frame->SetFrameType(StackFrame::JAVA_SCRIPT);

  bool is_bottommost = (0 == frame_index);
  bool is_topmost = (output_count_ - 1 == frame_index);
  CHECK(frame_index >= 0 && frame_index < output_count_);
  CHECK_NULL(output_[frame_index]);
  output_[frame_index] = output_frame;

  // The top address for the bottommost output frame can be computed from
  // the input frame pointer and the output frame's height.  For all
  // subsequent output frames, it can be computed from the previous one's
  // top address and the current frame's size.
  Register fp_reg = JavaScriptFrame::fp_register();
  intptr_t top_address;
  if (is_bottommost) {
    // Determine whether the input frame contains alignment padding.
    has_alignment_padding_ =
        (!compiled_code_->is_turbofanned() && HasAlignmentPadding(function))
            ? 1
            : 0;
    // 2 = context and function in the frame.
    // If the optimized frame had alignment padding, adjust the frame pointer
    // to point to the new position of the old frame pointer after padding
    // is removed. Subtract 2 * kPointerSize for the context and function slots.
    top_address = input_->GetRegister(fp_reg.code()) -
        StandardFrameConstants::kFixedFrameSizeFromFp -
        height_in_bytes + has_alignment_padding_ * kPointerSize;
  } else {
    top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  }
  output_frame->SetTop(top_address);

  // Compute the incoming parameter translation.
  int parameter_count =
      function->shared()->internal_formal_parameter_count() + 1;
  unsigned output_offset = output_frame_size;
  unsigned input_offset = input_frame_size;
  for (int i = 0; i < parameter_count; ++i) {
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
  }
  input_offset -= (parameter_count * kPointerSize);

  // There are no translation commands for the caller's pc and fp, the
  // context, and the function.  Synthesize their values and set them up
  // explicitly.
  //
  // The caller's pc for the bottommost output frame is the same as in the
  // input frame.  For all subsequent output frames, it can be read from the
  // previous one.  This frame's pc can be computed from the non-optimized
  // function code and AST id of the bailout.
  output_offset -= kPCOnStackSize;
  input_offset -= kPCOnStackSize;
  intptr_t value;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
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
  input_offset -= kFPOnStackSize;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
  } else {
    value = output_[frame_index - 1]->GetFp();
  }
  output_frame->SetCallerFp(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  DCHECK(!is_bottommost || (input_->GetRegister(fp_reg.code()) +
      has_alignment_padding_ * kPointerSize) == fp_value);
  output_frame->SetFp(fp_value);
  if (is_topmost) output_frame->SetRegister(fp_reg.code(), fp_value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's fp\n");
  DCHECK(!is_bottommost || !has_alignment_padding_ ||
         (fp_value & kPointerSize) != 0);

  if (FLAG_enable_embedded_constant_pool) {
    // For the bottommost output frame the constant pool pointer can be gotten
    // from the input frame. For subsequent output frames, it can be read from
    // the previous frame.
    output_offset -= kPointerSize;
    input_offset -= kPointerSize;
    if (is_bottommost) {
      value = input_->GetFrameSlot(input_offset);
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
  Register context_reg = JavaScriptFrame::context_register();
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  // Read the context from the translations.
  Object* context = value_iterator->GetRawValue();
  if (context == isolate_->heap()->undefined_value()) {
    // If the context was optimized away, just use the context from
    // the activation. This should only apply to Crankshaft code.
    CHECK(!compiled_code_->is_turbofanned());
    context =
        is_bottommost
            ? reinterpret_cast<Object*>(input_->GetFrameSlot(input_offset))
            : function->context();
  }
  value = reinterpret_cast<intptr_t>(context);
  output_frame->SetContext(value);
  if (is_topmost) output_frame->SetRegister(context_reg.code(), value);
  WriteValueToOutput(context, input_index, frame_index, output_offset,
                     "context    ");
  if (context == isolate_->heap()->arguments_marker()) {
    Address output_address =
        reinterpret_cast<Address>(output_[frame_index]->GetTop()) +
        output_offset;
    values_to_materialize_.push_back({output_address, value_iterator});
  }
  value_iterator++;
  input_index++;

  // The function was mentioned explicitly in the BEGIN_FRAME.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  // The function for the bottommost output frame should also agree with the
  // input frame.
  DCHECK(!is_bottommost || input_->GetFrameSlot(input_offset) == value);
  WriteValueToOutput(function, 0, frame_index, output_offset, "function    ");

  // Translate the rest of the frame.
  for (unsigned i = 0; i < height; ++i) {
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
  }
  CHECK_EQ(0u, output_offset);

  // Compute this frame's PC, state, and continuation.
  Code* non_optimized_code = function->shared()->code();
  FixedArray* raw_data = non_optimized_code->deoptimization_data();
  DeoptimizationOutputData* data = DeoptimizationOutputData::cast(raw_data);
  Address start = non_optimized_code->instruction_start();
  unsigned pc_and_state = GetOutputInfo(data, node_id, function->shared());
  unsigned pc_offset = FullCodeGenerator::PcField::decode(pc_and_state);
  intptr_t pc_value = reinterpret_cast<intptr_t>(start + pc_offset);
  output_frame->SetPc(pc_value);

  // Update constant pool.
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(non_optimized_code->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
    if (is_topmost) {
      Register constant_pool_reg =
          JavaScriptFrame::constant_pool_pointer_register();
      output_frame->SetRegister(constant_pool_reg.code(), constant_pool_value);
    }
  }

  FullCodeGenerator::State state =
      FullCodeGenerator::StateField::decode(pc_and_state);
  output_frame->SetState(Smi::FromInt(state));

  // Set the continuation for the topmost frame.
  if (is_topmost && bailout_type_ != DEBUGGER) {
    Builtins* builtins = isolate_->builtins();
    Code* continuation = builtins->builtin(Builtins::kNotifyDeoptimized);
    if (bailout_type_ == LAZY) {
      continuation = builtins->builtin(Builtins::kNotifyLazyDeoptimized);
    } else if (bailout_type_ == SOFT) {
      continuation = builtins->builtin(Builtins::kNotifySoftDeoptimized);
    } else {
      CHECK_EQ(bailout_type_, EAGER);
    }
    output_frame->SetContinuation(
        reinterpret_cast<intptr_t>(continuation->entry()));
  }
}


void Deoptimizer::DoComputeInterpretedFrame(int frame_index) {
  TranslatedFrame* translated_frame =
      &(translated_state_.frames()[frame_index]);
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  int input_index = 0;

  BailoutId bytecode_offset = translated_frame->node_id();
  unsigned height = translated_frame->height();
  unsigned height_in_bytes = height * kPointerSize;
  JSFunction* function = JSFunction::cast(value_iterator->GetRawValue());
  value_iterator++;
  input_index++;
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(), "  translating interpreted frame ");
    function->PrintName(trace_scope_->file());
    PrintF(trace_scope_->file(), " => bytecode_offset=%d, height=%d\n",
           bytecode_offset.ToInt(), height_in_bytes);
  }

  // The 'fixed' part of the frame consists of the incoming parameters and
  // the part described by InterpreterFrameConstants.
  unsigned fixed_frame_size = ComputeInterpretedFixedSize(function);
  unsigned input_frame_size = input_->GetFrameSize();
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new (output_frame_size) FrameDescription(output_frame_size, function);
  output_frame->SetFrameType(StackFrame::INTERPRETED);

  bool is_bottommost = (0 == frame_index);
  bool is_topmost = (output_count_ - 1 == frame_index);
  CHECK(frame_index >= 0 && frame_index < output_count_);
  CHECK_NULL(output_[frame_index]);
  output_[frame_index] = output_frame;

  // The top address for the bottommost output frame can be computed from
  // the input frame pointer and the output frame's height.  For all
  // subsequent output frames, it can be computed from the previous one's
  // top address and the current frame's size.
  Register fp_reg = InterpretedFrame::fp_register();
  intptr_t top_address;
  if (is_bottommost) {
    // Subtract interpreter fixed frame size for the context function slots,
    // new,target and bytecode offset.
    top_address = input_->GetRegister(fp_reg.code()) -
                  InterpreterFrameConstants::kFixedFrameSizeFromFp -
                  height_in_bytes;
  } else {
    top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  }
  output_frame->SetTop(top_address);

  // Compute the incoming parameter translation.
  int parameter_count =
      function->shared()->internal_formal_parameter_count() + 1;
  unsigned output_offset = output_frame_size;
  unsigned input_offset = input_frame_size;
  for (int i = 0; i < parameter_count; ++i) {
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
  }
  input_offset -= (parameter_count * kPointerSize);

  // There are no translation commands for the caller's pc and fp, the
  // context, the function, new.target and the bytecode offset.  Synthesize
  // their values and set them up
  // explicitly.
  //
  // The caller's pc for the bottommost output frame is the same as in the
  // input frame.  For all subsequent output frames, it can be read from the
  // previous one.  This frame's pc can be computed from the non-optimized
  // function code and AST id of the bailout.
  output_offset -= kPCOnStackSize;
  input_offset -= kPCOnStackSize;
  intptr_t value;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
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
  input_offset -= kFPOnStackSize;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
  } else {
    value = output_[frame_index - 1]->GetFp();
  }
  output_frame->SetCallerFp(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  DCHECK(!is_bottommost ||
         (input_->GetRegister(fp_reg.code()) +
          has_alignment_padding_ * kPointerSize) == fp_value);
  output_frame->SetFp(fp_value);
  if (is_topmost) output_frame->SetRegister(fp_reg.code(), fp_value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's fp\n");
  DCHECK(!is_bottommost || !has_alignment_padding_ ||
         (fp_value & kPointerSize) != 0);

  if (FLAG_enable_embedded_constant_pool) {
    // For the bottommost output frame the constant pool pointer can be gotten
    // from the input frame. For subsequent output frames, it can be read from
    // the previous frame.
    output_offset -= kPointerSize;
    input_offset -= kPointerSize;
    if (is_bottommost) {
      value = input_->GetFrameSlot(input_offset);
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
  Register context_reg = InterpretedFrame::context_register();
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  // Read the context from the translations.
  Object* context = value_iterator->GetRawValue();
  // The context should not be a placeholder for a materialized object.
  CHECK(context != isolate_->heap()->arguments_marker());
  value = reinterpret_cast<intptr_t>(context);
  output_frame->SetContext(value);
  if (is_topmost) output_frame->SetRegister(context_reg.code(), value);
  WriteValueToOutput(context, input_index, frame_index, output_offset,
                     "context    ");
  value_iterator++;
  input_index++;

  // The function was mentioned explicitly in the BEGIN_FRAME.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  // The function for the bottommost output frame should also agree with the
  // input frame.
  DCHECK(!is_bottommost || input_->GetFrameSlot(input_offset) == value);
  WriteValueToOutput(function, 0, frame_index, output_offset, "function    ");

  // TODO(rmcilroy): Deal with new.target correctly - currently just set it to
  // undefined.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  Object* new_target = isolate_->heap()->undefined_value();
  WriteValueToOutput(new_target, 0, frame_index, output_offset, "new_target  ");

  // The bytecode offset was mentioned explicitly in the BEGIN_FRAME.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  int raw_bytecode_offset =
      BytecodeArray::kHeaderSize - kHeapObjectTag + bytecode_offset.ToInt();
  Smi* smi_bytecode_offset = Smi::FromInt(raw_bytecode_offset);
  WriteValueToOutput(smi_bytecode_offset, 0, frame_index, output_offset,
                     "bytecode offset ");

  // Translate the rest of the interpreter registers in the frame.
  for (unsigned i = 0; i < height; ++i) {
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
  }
  CHECK_EQ(0u, output_offset);

  // Set the accumulator register.
  output_frame->SetRegister(
      kInterpreterAccumulatorRegister.code(),
      reinterpret_cast<intptr_t>(value_iterator->GetRawValue()));
  value_iterator++;

  Builtins* builtins = isolate_->builtins();
  Code* trampoline = builtins->builtin(Builtins::kInterpreterEntryTrampoline);
  output_frame->SetPc(reinterpret_cast<intptr_t>(trampoline->entry()));
  output_frame->SetState(0);

  // Update constant pool.
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(trampoline->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
    if (is_topmost) {
      Register constant_pool_reg =
          InterpretedFrame::constant_pool_pointer_register();
      output_frame->SetRegister(constant_pool_reg.code(), constant_pool_value);
    }
  }

  // Set the continuation for the topmost frame.
  if (is_topmost && bailout_type_ != DEBUGGER) {
    Code* continuation =
        builtins->builtin(Builtins::kInterpreterNotifyDeoptimized);
    if (bailout_type_ == LAZY) {
      continuation =
          builtins->builtin(Builtins::kInterpreterNotifyLazyDeoptimized);
    } else if (bailout_type_ == SOFT) {
      continuation =
          builtins->builtin(Builtins::kInterpreterNotifySoftDeoptimized);
    } else {
      CHECK_EQ(bailout_type_, EAGER);
    }
    output_frame->SetContinuation(
        reinterpret_cast<intptr_t>(continuation->entry()));
  }
}


void Deoptimizer::DoComputeArgumentsAdaptorFrame(int frame_index) {
  TranslatedFrame* translated_frame =
      &(translated_state_.frames()[frame_index]);
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  int input_index = 0;

  unsigned height = translated_frame->height();
  unsigned height_in_bytes = height * kPointerSize;
  JSFunction* function = JSFunction::cast(value_iterator->GetRawValue());
  value_iterator++;
  input_index++;
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "  translating arguments adaptor => height=%d\n", height_in_bytes);
  }

  unsigned fixed_frame_size = ArgumentsAdaptorFrameConstants::kFrameSize;
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, function);
  output_frame->SetFrameType(StackFrame::ARGUMENTS_ADAPTOR);

  // Arguments adaptor can not be topmost or bottommost.
  CHECK(frame_index > 0 && frame_index < output_count_ - 1);
  CHECK(output_[frame_index] == NULL);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous
  // frame's top and this frame's size.
  intptr_t top_address;
  top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  output_frame->SetTop(top_address);

  // Compute the incoming parameter translation.
  int parameter_count = height;
  unsigned output_offset = output_frame_size;
  for (int i = 0; i < parameter_count; ++i) {
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
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
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's fp\n");

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_offset -= kPointerSize;
    value = output_[frame_index - 1]->GetConstantPool();
    output_frame->SetCallerConstantPool(output_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_offset,
                         "caller's constant_pool\n");
  }

  // A marker value is used in place of the context.
  output_offset -= kPointerSize;
  intptr_t context = reinterpret_cast<intptr_t>(
      Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  output_frame->SetFrameSlot(output_offset, context);
  DebugPrintOutputSlot(context, frame_index, output_offset,
                       "context (adaptor sentinel)\n");

  // The function was mentioned explicitly in the ARGUMENTS_ADAPTOR_FRAME.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  WriteValueToOutput(function, 0, frame_index, output_offset, "function    ");

  // Number of incoming arguments.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(height - 1));
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "argc ");
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(), "(%d)\n", height - 1);
  }

  DCHECK(0 == output_offset);

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


void Deoptimizer::DoComputeConstructStubFrame(int frame_index) {
  TranslatedFrame* translated_frame =
      &(translated_state_.frames()[frame_index]);
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  int input_index = 0;

  Builtins* builtins = isolate_->builtins();
  Code* construct_stub = builtins->builtin(Builtins::kJSConstructStubGeneric);
  unsigned height = translated_frame->height();
  unsigned height_in_bytes = height * kPointerSize;
  JSFunction* function = JSFunction::cast(value_iterator->GetRawValue());
  value_iterator++;
  input_index++;
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "  translating construct stub => height=%d\n", height_in_bytes);
  }

  unsigned fixed_frame_size = ConstructFrameConstants::kFrameSize;
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, function);
  output_frame->SetFrameType(StackFrame::CONSTRUCT);

  // Construct stub can not be topmost or bottommost.
  DCHECK(frame_index > 0 && frame_index < output_count_ - 1);
  DCHECK(output_[frame_index] == NULL);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous
  // frame's top and this frame's size.
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
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's fp\n");

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_offset -= kPointerSize;
    value = output_[frame_index - 1]->GetConstantPool();
    output_frame->SetCallerConstantPool(output_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_offset,
                         "caller's constant_pool\n");
  }

  // The context can be gotten from the previous frame.
  output_offset -= kPointerSize;
  value = output_[frame_index - 1]->GetContext();
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "context\n");

  // A marker value is used in place of the function.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(StackFrame::CONSTRUCT));
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset,
                       "function (construct sentinel)\n");

  // The output frame reflects a JSConstructStubGeneric frame.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(construct_stub);
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "code object\n");

  // The allocation site.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(isolate_->heap()->undefined_value());
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "allocation site\n");

  // Number of incoming arguments.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(height - 1));
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "argc ");
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(), "(%d)\n", height - 1);
  }

  // The newly allocated object was passed as receiver in the artificial
  // constructor stub environment created by HEnvironment::CopyForInlining().
  output_offset -= kPointerSize;
  value = output_frame->GetFrameSlot(output_frame_size - kPointerSize);
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset,
                       "allocated receiver\n");

  CHECK_EQ(0u, output_offset);

  intptr_t pc = reinterpret_cast<intptr_t>(
      construct_stub->instruction_start() +
      isolate_->heap()->construct_stub_deopt_pc_offset()->value());
  output_frame->SetPc(pc);
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(construct_stub->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
  }
}


void Deoptimizer::DoComputeAccessorStubFrame(int frame_index,
                                             bool is_setter_stub_frame) {
  TranslatedFrame* translated_frame =
      &(translated_state_.frames()[frame_index]);
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  int input_index = 0;

  JSFunction* accessor = JSFunction::cast(value_iterator->GetRawValue());
  value_iterator++;
  input_index++;
  // The receiver (and the implicit return value, if any) are expected in
  // registers by the LoadIC/StoreIC, so they don't belong to the output stack
  // frame. This means that we have to use a height of 0.
  unsigned height = 0;
  unsigned height_in_bytes = height * kPointerSize;
  const char* kind = is_setter_stub_frame ? "setter" : "getter";
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "  translating %s stub => height=%u\n", kind, height_in_bytes);
  }

  // We need 1 stack entry for the return address and enough entries for the
  // StackFrame::INTERNAL (FP, context, frame type, code object and constant
  // pool (if enabled)- see MacroAssembler::EnterFrame).
  // For a setter stub frame we need one additional entry for the implicit
  // return value, see StoreStubCompiler::CompileStoreViaSetter.
  unsigned fixed_frame_entries =
      (StandardFrameConstants::kFixedFrameSize / kPointerSize) + 1 +
      (is_setter_stub_frame ? 1 : 0);
  unsigned fixed_frame_size = fixed_frame_entries * kPointerSize;
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, accessor);
  output_frame->SetFrameType(StackFrame::INTERNAL);

  // A frame for an accessor stub can not be the topmost or bottommost one.
  CHECK(frame_index > 0 && frame_index < output_count_ - 1);
  CHECK_NULL(output_[frame_index]);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous frame's top and
  // this frame's size.
  intptr_t top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  output_frame->SetTop(top_address);

  unsigned output_offset = output_frame_size;

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
  DebugPrintOutputSlot(value, frame_index, output_offset, "caller's fp\n");

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_offset -= kPointerSize;
    value = output_[frame_index - 1]->GetConstantPool();
    output_frame->SetCallerConstantPool(output_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_offset,
                         "caller's constant_pool\n");
  }

  // The context can be gotten from the previous frame.
  output_offset -= kPointerSize;
  value = output_[frame_index - 1]->GetContext();
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "context\n");

  // A marker value is used in place of the function.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(StackFrame::INTERNAL));
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "function ");
  if (trace_scope_ != nullptr) {
    PrintF(trace_scope_->file(), "(%s sentinel)\n", kind);
  }

  // Get Code object from accessor stub.
  output_offset -= kPointerSize;
  Builtins::Name name = is_setter_stub_frame ?
      Builtins::kStoreIC_Setter_ForDeopt :
      Builtins::kLoadIC_Getter_ForDeopt;
  Code* accessor_stub = isolate_->builtins()->builtin(name);
  value = reinterpret_cast<intptr_t>(accessor_stub);
  output_frame->SetFrameSlot(output_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_offset, "code object\n");

  // Skip receiver.
  value_iterator++;
  input_index++;

  if (is_setter_stub_frame) {
    // The implicit return value was part of the artificial setter stub
    // environment.
    output_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, frame_index,
                                 output_offset);
  }

  CHECK_EQ(0u, output_offset);

  Smi* offset = is_setter_stub_frame ?
      isolate_->heap()->setter_stub_deopt_pc_offset() :
      isolate_->heap()->getter_stub_deopt_pc_offset();
  intptr_t pc = reinterpret_cast<intptr_t>(
      accessor_stub->instruction_start() + offset->value());
  output_frame->SetPc(pc);
  if (FLAG_enable_embedded_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(accessor_stub->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
  }
}


void Deoptimizer::DoComputeCompiledStubFrame(int frame_index) {
  //
  //               FROM                                  TO
  //    |          ....           |          |          ....           |
  //    +-------------------------+          +-------------------------+
  //    | JSFunction continuation |          | JSFunction continuation |
  //    +-------------------------+          +-------------------------+
  // |  |    saved frame (FP)     |          |    saved frame (FP)     |
  // |  +=========================+<-fpreg   +=========================+<-fpreg
  // |  |constant pool (if ool_cp)|          |constant pool (if ool_cp)|
  // |  +-------------------------+          +-------------------------|
  // |  |   JSFunction context    |          |   JSFunction context    |
  // v  +-------------------------+          +-------------------------|
  //    |   COMPILED_STUB marker  |          |   STUB_FAILURE marker   |
  //    +-------------------------+          +-------------------------+
  //    |                         |          |  caller args.arguments_ |
  //    | ...                     |          +-------------------------+
  //    |                         |          |  caller args.length_    |
  //    |-------------------------|<-spreg   +-------------------------+
  //                                         |  caller args pointer    |
  //                                         +-------------------------+
  //                                         |  caller stack param 1   |
  //      parameters in registers            +-------------------------+
  //       and spilled to stack              |           ....          |
  //                                         +-------------------------+
  //                                         |  caller stack param n   |
  //                                         +-------------------------+<-spreg
  //                                         reg = number of parameters
  //                                         reg = failure handler address
  //                                         reg = saved frame
  //                                         reg = JSFunction context
  //
  // Caller stack params contain the register parameters to the stub first,
  // and then, if the descriptor specifies a constant number of stack
  // parameters, the stack parameters as well.

  TranslatedFrame* translated_frame =
      &(translated_state_.frames()[frame_index]);
  TranslatedFrame::iterator value_iterator = translated_frame->begin();
  int input_index = 0;

  CHECK(compiled_code_->is_hydrogen_stub());
  int major_key = CodeStub::GetMajorKey(compiled_code_);
  CodeStubDescriptor descriptor(isolate_, compiled_code_->stub_key());

  // The output frame must have room for all pushed register parameters
  // and the standard stack frame slots.  Include space for an argument
  // object to the callee and optionally the space to pass the argument
  // object to the stub failure handler.
  int param_count = descriptor.GetRegisterParameterCount();
  int stack_param_count = descriptor.GetStackParameterCount();
  CHECK_EQ(translated_frame->height(), param_count);
  CHECK_GE(param_count, 0);

  int height_in_bytes = kPointerSize * (param_count + stack_param_count) +
                        sizeof(Arguments) + kPointerSize;
  int fixed_frame_size = StandardFrameConstants::kFixedFrameSize;
  int input_frame_size = input_->GetFrameSize();
  int output_frame_size = height_in_bytes + fixed_frame_size;
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "  translating %s => StubFailureTrampolineStub, height=%d\n",
           CodeStub::MajorName(static_cast<CodeStub::Major>(major_key)),
           height_in_bytes);
  }

  // The stub failure trampoline is a single frame.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, NULL);
  output_frame->SetFrameType(StackFrame::STUB_FAILURE_TRAMPOLINE);
  CHECK_EQ(frame_index, 0);
  output_[frame_index] = output_frame;

  // The top address for the output frame can be computed from the input
  // frame pointer and the output frame's height. Subtract space for the
  // context and function slots.
  Register fp_reg = StubFailureTrampolineFrame::fp_register();
  intptr_t top_address = input_->GetRegister(fp_reg.code()) -
      StandardFrameConstants::kFixedFrameSizeFromFp - height_in_bytes;
  output_frame->SetTop(top_address);

  // Read caller's PC (JSFunction continuation) from the input frame.
  unsigned input_frame_offset = input_frame_size - kPCOnStackSize;
  unsigned output_frame_offset = output_frame_size - kFPOnStackSize;
  intptr_t value = input_->GetFrameSlot(input_frame_offset);
  output_frame->SetCallerPc(output_frame_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                       "caller's pc\n");

  // Read caller's FP from the input frame, and set this frame's FP.
  input_frame_offset -= kFPOnStackSize;
  value = input_->GetFrameSlot(input_frame_offset);
  output_frame_offset -= kFPOnStackSize;
  output_frame->SetCallerFp(output_frame_offset, value);
  intptr_t frame_ptr = input_->GetRegister(fp_reg.code());
  output_frame->SetRegister(fp_reg.code(), frame_ptr);
  output_frame->SetFp(frame_ptr);
  DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                       "caller's fp\n");

  if (FLAG_enable_embedded_constant_pool) {
    // Read the caller's constant pool from the input frame.
    input_frame_offset -= kPointerSize;
    value = input_->GetFrameSlot(input_frame_offset);
    output_frame_offset -= kPointerSize;
    output_frame->SetCallerConstantPool(output_frame_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                         "caller's constant_pool\n");
  }

  // The context can be gotten from the input frame.
  Register context_reg = StubFailureTrampolineFrame::context_register();
  input_frame_offset -= kPointerSize;
  value = input_->GetFrameSlot(input_frame_offset);
  output_frame->SetRegister(context_reg.code(), value);
  output_frame_offset -= kPointerSize;
  output_frame->SetFrameSlot(output_frame_offset, value);
  CHECK(reinterpret_cast<Object*>(value)->IsContext());
  DebugPrintOutputSlot(value, frame_index, output_frame_offset, "context\n");

  // A marker value is used in place of the function.
  output_frame_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(
      Smi::FromInt(StackFrame::STUB_FAILURE_TRAMPOLINE));
  output_frame->SetFrameSlot(output_frame_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                       "function (stub failure sentinel)\n");

  intptr_t caller_arg_count = stack_param_count;
  bool arg_count_known = !descriptor.stack_parameter_count().is_valid();

  // Build the Arguments object for the caller's parameters and a pointer to it.
  output_frame_offset -= kPointerSize;
  int args_arguments_offset = output_frame_offset;
  intptr_t the_hole = reinterpret_cast<intptr_t>(
      isolate_->heap()->the_hole_value());
  if (arg_count_known) {
    value = frame_ptr + StandardFrameConstants::kCallerSPOffset +
        (caller_arg_count - 1) * kPointerSize;
  } else {
    value = the_hole;
  }

  output_frame->SetFrameSlot(args_arguments_offset, value);
  DebugPrintOutputSlot(
      value, frame_index, args_arguments_offset,
      arg_count_known ? "args.arguments\n" : "args.arguments (the hole)\n");

  output_frame_offset -= kPointerSize;
  int length_frame_offset = output_frame_offset;
  value = arg_count_known ? caller_arg_count : the_hole;
  output_frame->SetFrameSlot(length_frame_offset, value);
  DebugPrintOutputSlot(
      value, frame_index, length_frame_offset,
      arg_count_known ? "args.length\n" : "args.length (the hole)\n");

  output_frame_offset -= kPointerSize;
  value = frame_ptr + StandardFrameConstants::kCallerSPOffset -
      (output_frame_size - output_frame_offset) + kPointerSize;
  output_frame->SetFrameSlot(output_frame_offset, value);
  DebugPrintOutputSlot(value, frame_index, output_frame_offset, "args*\n");

  // Copy the register parameters to the failure frame.
  int arguments_length_offset = -1;
  for (int i = 0; i < param_count; ++i) {
    output_frame_offset -= kPointerSize;
    WriteTranslatedValueToOutput(&value_iterator, &input_index, 0,
                                 output_frame_offset);

    if (!arg_count_known &&
        descriptor.GetRegisterParameter(i)
            .is(descriptor.stack_parameter_count())) {
      arguments_length_offset = output_frame_offset;
    }
  }

  // Copy constant stack parameters to the failure frame. If the number of stack
  // parameters is not known in the descriptor, the arguments object is the way
  // to access them.
  for (int i = 0; i < stack_param_count; i++) {
    output_frame_offset -= kPointerSize;
    Object** stack_parameter = reinterpret_cast<Object**>(
        frame_ptr + StandardFrameConstants::kCallerSPOffset +
        (stack_param_count - i - 1) * kPointerSize);
    value = reinterpret_cast<intptr_t>(*stack_parameter);
    output_frame->SetFrameSlot(output_frame_offset, value);
    DebugPrintOutputSlot(value, frame_index, output_frame_offset,
                         "stack parameter\n");
  }

  CHECK_EQ(0u, output_frame_offset);

  if (!arg_count_known) {
    CHECK_GE(arguments_length_offset, 0);
    // We know it's a smi because 1) the code stub guarantees the stack
    // parameter count is in smi range, and 2) the DoTranslateCommand in the
    // parameter loop above translated that to a tagged value.
    Smi* smi_caller_arg_count = reinterpret_cast<Smi*>(
        output_frame->GetFrameSlot(arguments_length_offset));
    caller_arg_count = smi_caller_arg_count->value();
    output_frame->SetFrameSlot(length_frame_offset, caller_arg_count);
    DebugPrintOutputSlot(caller_arg_count, frame_index, length_frame_offset,
                         "args.length\n");
    value = frame_ptr + StandardFrameConstants::kCallerSPOffset +
        (caller_arg_count - 1) * kPointerSize;
    output_frame->SetFrameSlot(args_arguments_offset, value);
    DebugPrintOutputSlot(value, frame_index, args_arguments_offset,
                         "args.arguments");
  }

  // Copy the double registers from the input into the output frame.
  CopyDoubleRegisters(output_frame);

  // Fill registers containing handler and number of parameters.
  SetPlatformCompiledStubRegisters(output_frame, &descriptor);

  // Compute this frame's PC, state, and continuation.
  Code* trampoline = NULL;
  StubFunctionMode function_mode = descriptor.function_mode();
  StubFailureTrampolineStub(isolate_, function_mode)
      .FindCodeInCache(&trampoline);
  DCHECK(trampoline != NULL);
  output_frame->SetPc(reinterpret_cast<intptr_t>(
      trampoline->instruction_start()));
  if (FLAG_enable_embedded_constant_pool) {
    Register constant_pool_reg =
        StubFailureTrampolineFrame::constant_pool_pointer_register();
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(trampoline->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
    output_frame->SetRegister(constant_pool_reg.code(), constant_pool_value);
  }
  output_frame->SetState(Smi::FromInt(FullCodeGenerator::NO_REGISTERS));
  Code* notify_failure =
      isolate_->builtins()->builtin(Builtins::kNotifyStubFailureSaveDoubles);
  output_frame->SetContinuation(
      reinterpret_cast<intptr_t>(notify_failure->entry()));
}


void Deoptimizer::MaterializeHeapObjects(JavaScriptFrameIterator* it) {
  DCHECK_NE(DEBUGGER, bailout_type_);

  // Walk to the last JavaScript output frame to find out if it has
  // adapted arguments.
  for (int frame_index = 0; frame_index < jsframe_count(); ++frame_index) {
    if (frame_index != 0) it->Advance();
  }
  translated_state_.Prepare(it->frame()->has_adapted_arguments(), stack_fp_);

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

  isolate_->materialized_object_store()->Remove(stack_fp_);
}


void Deoptimizer::MaterializeHeapNumbersForDebuggerInspectableFrame(
    int frame_index, int parameter_count, int expression_count,
    DeoptimizedFrameInfo* info) {
  CHECK_EQ(DEBUGGER, bailout_type_);

  translated_state_.Prepare(false, nullptr);

  TranslatedFrame* frame = &(translated_state_.frames()[frame_index]);
  CHECK(frame->kind() == TranslatedFrame::kFunction);
  int frame_arg_count = frame->shared_info()->internal_formal_parameter_count();

  // The height is #expressions + 1 for context.
  CHECK_EQ(expression_count + 1, frame->height());
  TranslatedFrame* argument_frame = frame;
  if (frame_index > 0) {
    TranslatedFrame* previous_frame =
        &(translated_state_.frames()[frame_index - 1]);
    if (previous_frame->kind() == TranslatedFrame::kArgumentsAdaptor) {
      argument_frame = previous_frame;
      CHECK_EQ(parameter_count, argument_frame->height() - 1);
    } else {
      CHECK_EQ(frame_arg_count, parameter_count);
    }
  } else {
    CHECK_EQ(frame_arg_count, parameter_count);
  }

  TranslatedFrame::iterator arg_iter = argument_frame->begin();
  arg_iter++;  // Skip the function.
  arg_iter++;  // Skip the receiver.
  for (int i = 0; i < parameter_count; i++, arg_iter++) {
    if (!arg_iter->IsMaterializedObject()) {
      info->SetParameter(i, *(arg_iter->GetValue()));
    }
  }

  TranslatedFrame::iterator iter = frame->begin();
  // Skip the function, receiver, context and arguments.
  for (int i = 0; i < frame_arg_count + 3; i++, iter++) {
  }

  for (int i = 0; i < expression_count; i++, iter++) {
    if (!iter->IsMaterializedObject()) {
      info->SetExpression(i, *(iter->GetValue()));
    }
  }
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


unsigned Deoptimizer::ComputeInputFrameSize() const {
  unsigned fixed_size = ComputeJavascriptFixedSize(function_);
  // The fp-to-sp delta already takes the context, constant pool pointer and the
  // function into account so we have to avoid double counting them.
  unsigned result = fixed_size + fp_to_sp_delta_ -
                    StandardFrameConstants::kFixedFrameSizeFromFp;
  if (compiled_code_->kind() == Code::OPTIMIZED_FUNCTION) {
    unsigned stack_slots = compiled_code_->stack_slots();
    unsigned outgoing_size =
        ComputeOutgoingArgumentSize(compiled_code_, bailout_id_);
    CHECK(result == fixed_size + (stack_slots * kPointerSize) + outgoing_size);
  }
  return result;
}


unsigned Deoptimizer::ComputeJavascriptFixedSize(JSFunction* function) const {
  // The fixed part of the frame consists of the return address, frame
  // pointer, function, context, and all the incoming arguments.
  return ComputeIncomingArgumentSize(function) +
         StandardFrameConstants::kFixedFrameSize;
}


unsigned Deoptimizer::ComputeInterpretedFixedSize(JSFunction* function) const {
  // The fixed part of the frame consists of the return address, frame
  // pointer, function, context, new.target, bytecode offset and all the
  // incoming arguments.
  return ComputeIncomingArgumentSize(function) +
         InterpreterFrameConstants::kFixedFrameSize;
}


unsigned Deoptimizer::ComputeIncomingArgumentSize(JSFunction* function) const {
  // The incoming arguments is the values for formal parameters and
  // the receiver. Every slot contains a pointer.
  if (function->IsSmi()) {
    CHECK_EQ(Smi::cast(function), Smi::FromInt(StackFrame::STUB));
    return 0;
  }
  unsigned arguments =
      function->shared()->internal_formal_parameter_count() + 1;
  return arguments * kPointerSize;
}


// static
unsigned Deoptimizer::ComputeOutgoingArgumentSize(Code* code,
                                                  unsigned bailout_id) {
  DeoptimizationInputData* data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  unsigned height = data->ArgumentsStackHeight(bailout_id)->value();
  return height * kPointerSize;
}


Object* Deoptimizer::ComputeLiteral(int index) const {
  DeoptimizationInputData* data =
      DeoptimizationInputData::cast(compiled_code_->deoptimization_data());
  FixedArray* literals = data->LiteralArray();
  return literals->get(index);
}


void Deoptimizer::EnsureCodeForDeoptimizationEntry(Isolate* isolate,
                                                   BailoutType type,
                                                   int max_entry_id) {
  // We cannot run this if the serializer is enabled because this will
  // cause us to emit relocation information for the external
  // references. This is fine because the deoptimizer's code section
  // isn't meant to be serialized at all.
  CHECK(type == EAGER || type == SOFT || type == LAZY);
  DeoptimizerData* data = isolate->deoptimizer_data();
  int entry_count = data->deopt_entry_code_entries_[type];
  if (max_entry_id < entry_count) return;
  entry_count = Max(entry_count, Deoptimizer::kMinNumberOfEntries);
  while (max_entry_id >= entry_count) entry_count *= 2;
  CHECK(entry_count <= Deoptimizer::kMaxNumberOfEntries);

  MacroAssembler masm(isolate, NULL, 16 * KB, CodeObjectRequired::kYes);
  masm.set_emit_debug_code(false);
  GenerateDeoptimizationEntries(&masm, entry_count, type);
  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  MemoryChunk* chunk = data->deopt_entry_code_[type];
  CHECK(static_cast<int>(Deoptimizer::GetMaxDeoptTableSize()) >=
        desc.instr_size);
  if (!chunk->CommitArea(desc.instr_size)) {
    V8::FatalProcessOutOfMemory(
        "Deoptimizer::EnsureCodeForDeoptimizationEntry");
  }
  CopyBytes(chunk->area_start(), desc.buffer,
            static_cast<size_t>(desc.instr_size));
  Assembler::FlushICache(isolate, chunk->area_start(), desc.instr_size);

  data->deopt_entry_code_entries_[type] = entry_count;
}


FrameDescription::FrameDescription(uint32_t frame_size,
                                   JSFunction* function)
    : frame_size_(frame_size),
      function_(function),
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


int FrameDescription::ComputeFixedSize() {
  if (type_ == StackFrame::INTERPRETED) {
    return InterpreterFrameConstants::kFixedFrameSize +
           (ComputeParametersCount() + 1) * kPointerSize;
  } else {
    return StandardFrameConstants::kFixedFrameSize +
           (ComputeParametersCount() + 1) * kPointerSize;
  }
}


unsigned FrameDescription::GetOffsetFromSlotIndex(int slot_index) {
  if (slot_index >= 0) {
    // Local or spill slots. Skip the fixed part of the frame
    // including all arguments.
    unsigned base = GetFrameSize() - ComputeFixedSize();
    return base - ((slot_index + 1) * kPointerSize);
  } else {
    // Incoming parameter.
    int arg_size = (ComputeParametersCount() + 1) * kPointerSize;
    unsigned base = GetFrameSize() - arg_size;
    return base - ((slot_index + 1) * kPointerSize);
  }
}


int FrameDescription::ComputeParametersCount() {
  switch (type_) {
    case StackFrame::JAVA_SCRIPT:
      return function_->shared()->internal_formal_parameter_count();
    case StackFrame::ARGUMENTS_ADAPTOR: {
      // Last slot contains number of incomming arguments as a smi.
      // Can't use GetExpression(0) because it would cause infinite recursion.
      return reinterpret_cast<Smi*>(*GetFrameSlotPointer(0))->value();
    }
    case StackFrame::STUB:
      return -1;  // Minus receiver.
    default:
      FATAL("Unexpected stack frame type");
      return 0;
  }
}


Object* FrameDescription::GetParameter(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, ComputeParametersCount());
  // The slot indexes for incoming arguments are negative.
  unsigned offset = GetOffsetFromSlotIndex(index - ComputeParametersCount());
  return reinterpret_cast<Object*>(*GetFrameSlotPointer(offset));
}


unsigned FrameDescription::GetExpressionCount() {
  CHECK_EQ(StackFrame::JAVA_SCRIPT, type_);
  unsigned size = GetFrameSize() - ComputeFixedSize();
  return size / kPointerSize;
}


Object* FrameDescription::GetExpression(int index) {
  DCHECK_EQ(StackFrame::JAVA_SCRIPT, type_);
  unsigned offset = GetOffsetFromSlotIndex(index);
  return reinterpret_cast<Object*>(*GetFrameSlotPointer(offset));
}


void TranslationBuffer::Add(int32_t value, Zone* zone) {
  // This wouldn't handle kMinInt correctly if it ever encountered it.
  DCHECK(value != kMinInt);
  // Encode the sign bit in the least significant bit.
  bool is_negative = (value < 0);
  uint32_t bits = ((is_negative ? -value : value) << 1) |
      static_cast<int32_t>(is_negative);
  // Encode the individual bytes using the least significant bit of
  // each byte to indicate whether or not more bytes follow.
  do {
    uint32_t next = bits >> 7;
    contents_.Add(((bits << 1) & 0xFF) | (next != 0), zone);
    bits = next;
  } while (bits != 0);
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


Handle<ByteArray> TranslationBuffer::CreateByteArray(Factory* factory) {
  int length = contents_.length();
  Handle<ByteArray> result = factory->NewByteArray(length, TENURED);
  MemCopy(result->GetDataStartAddress(), contents_.ToVector().start(), length);
  return result;
}


void Translation::BeginConstructStubFrame(int literal_id, unsigned height) {
  buffer_->Add(CONSTRUCT_STUB_FRAME, zone());
  buffer_->Add(literal_id, zone());
  buffer_->Add(height, zone());
}


void Translation::BeginGetterStubFrame(int literal_id) {
  buffer_->Add(GETTER_STUB_FRAME, zone());
  buffer_->Add(literal_id, zone());
}


void Translation::BeginSetterStubFrame(int literal_id) {
  buffer_->Add(SETTER_STUB_FRAME, zone());
  buffer_->Add(literal_id, zone());
}


void Translation::BeginArgumentsAdaptorFrame(int literal_id, unsigned height) {
  buffer_->Add(ARGUMENTS_ADAPTOR_FRAME, zone());
  buffer_->Add(literal_id, zone());
  buffer_->Add(height, zone());
}


void Translation::BeginJSFrame(BailoutId node_id,
                               int literal_id,
                               unsigned height) {
  buffer_->Add(JS_FRAME, zone());
  buffer_->Add(node_id.ToInt(), zone());
  buffer_->Add(literal_id, zone());
  buffer_->Add(height, zone());
}


void Translation::BeginInterpretedFrame(BailoutId bytecode_offset,
                                        int literal_id, unsigned height) {
  buffer_->Add(INTERPRETED_FRAME, zone());
  buffer_->Add(bytecode_offset.ToInt(), zone());
  buffer_->Add(literal_id, zone());
  buffer_->Add(height, zone());
}


void Translation::BeginCompiledStubFrame(int height) {
  buffer_->Add(COMPILED_STUB_FRAME, zone());
  buffer_->Add(height, zone());
}


void Translation::BeginArgumentsObject(int args_length) {
  buffer_->Add(ARGUMENTS_OBJECT, zone());
  buffer_->Add(args_length, zone());
}


void Translation::BeginCapturedObject(int length) {
  buffer_->Add(CAPTURED_OBJECT, zone());
  buffer_->Add(length, zone());
}


void Translation::DuplicateObject(int object_index) {
  buffer_->Add(DUPLICATED_OBJECT, zone());
  buffer_->Add(object_index, zone());
}


void Translation::StoreRegister(Register reg) {
  buffer_->Add(REGISTER, zone());
  buffer_->Add(reg.code(), zone());
}


void Translation::StoreInt32Register(Register reg) {
  buffer_->Add(INT32_REGISTER, zone());
  buffer_->Add(reg.code(), zone());
}


void Translation::StoreUint32Register(Register reg) {
  buffer_->Add(UINT32_REGISTER, zone());
  buffer_->Add(reg.code(), zone());
}


void Translation::StoreBoolRegister(Register reg) {
  buffer_->Add(BOOL_REGISTER, zone());
  buffer_->Add(reg.code(), zone());
}


void Translation::StoreDoubleRegister(DoubleRegister reg) {
  buffer_->Add(DOUBLE_REGISTER, zone());
  buffer_->Add(reg.code(), zone());
}


void Translation::StoreStackSlot(int index) {
  buffer_->Add(STACK_SLOT, zone());
  buffer_->Add(index, zone());
}


void Translation::StoreInt32StackSlot(int index) {
  buffer_->Add(INT32_STACK_SLOT, zone());
  buffer_->Add(index, zone());
}


void Translation::StoreUint32StackSlot(int index) {
  buffer_->Add(UINT32_STACK_SLOT, zone());
  buffer_->Add(index, zone());
}


void Translation::StoreBoolStackSlot(int index) {
  buffer_->Add(BOOL_STACK_SLOT, zone());
  buffer_->Add(index, zone());
}


void Translation::StoreDoubleStackSlot(int index) {
  buffer_->Add(DOUBLE_STACK_SLOT, zone());
  buffer_->Add(index, zone());
}


void Translation::StoreLiteral(int literal_id) {
  buffer_->Add(LITERAL, zone());
  buffer_->Add(literal_id, zone());
}


void Translation::StoreArgumentsObject(bool args_known,
                                       int args_index,
                                       int args_length) {
  buffer_->Add(ARGUMENTS_OBJECT, zone());
  buffer_->Add(args_known, zone());
  buffer_->Add(args_index, zone());
  buffer_->Add(args_length, zone());
}


void Translation::StoreJSFrameFunction() {
  buffer_->Add(JS_FRAME_FUNCTION, zone());
}


int Translation::NumberOfOperandsFor(Opcode opcode) {
  switch (opcode) {
    case JS_FRAME_FUNCTION:
      return 0;
    case GETTER_STUB_FRAME:
    case SETTER_STUB_FRAME:
    case DUPLICATED_OBJECT:
    case ARGUMENTS_OBJECT:
    case CAPTURED_OBJECT:
    case REGISTER:
    case INT32_REGISTER:
    case UINT32_REGISTER:
    case BOOL_REGISTER:
    case DOUBLE_REGISTER:
    case STACK_SLOT:
    case INT32_STACK_SLOT:
    case UINT32_STACK_SLOT:
    case BOOL_STACK_SLOT:
    case DOUBLE_STACK_SLOT:
    case LITERAL:
    case COMPILED_STUB_FRAME:
      return 1;
    case BEGIN:
    case ARGUMENTS_ADAPTOR_FRAME:
    case CONSTRUCT_STUB_FRAME:
      return 2;
    case JS_FRAME:
    case INTERPRETED_FRAME:
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
  return "";
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
    index = frame_fps_.length();
    frame_fps_.Add(fp);
  }

  Handle<FixedArray> array = EnsureStackEntries(index + 1);
  array->set(index, *materialized_objects);
}


bool MaterializedObjectStore::Remove(Address fp) {
  int index = StackIdToIndex(fp);
  if (index == -1) {
    return false;
  }
  CHECK_GE(index, 0);

  frame_fps_.Remove(index);
  FixedArray* array = isolate()->heap()->materialized_objects();
  CHECK_LT(index, array->length());
  for (int i = index; i < frame_fps_.length(); i++) {
    array->set(i, array->get(i + 1));
  }
  array->set(frame_fps_.length(), isolate()->heap()->undefined_value());
  return true;
}


int MaterializedObjectStore::StackIdToIndex(Address fp) {
  for (int i = 0; i < frame_fps_.length(); i++) {
    if (frame_fps_[i] == fp) {
      return i;
    }
  }
  return -1;
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


DeoptimizedFrameInfo::DeoptimizedFrameInfo(Deoptimizer* deoptimizer,
                                           int frame_index,
                                           bool has_arguments_adaptor,
                                           bool has_construct_stub) {
  FrameDescription* output_frame = deoptimizer->output_[frame_index];
  function_ = output_frame->GetFunction();
  context_ = reinterpret_cast<Object*>(output_frame->GetContext());
  has_construct_stub_ = has_construct_stub;
  expression_count_ = output_frame->GetExpressionCount();
  expression_stack_ = new Object* [expression_count_];
  // Get the source position using the unoptimized code.
  Address pc = reinterpret_cast<Address>(output_frame->GetPc());
  Code* code = Code::cast(deoptimizer->isolate()->FindCodeObject(pc));
  source_position_ = code->SourcePosition(pc);

  for (int i = 0; i < expression_count_; i++) {
    Object* value = output_frame->GetExpression(i);
    // Replace materialization markers with the undefined value.
    if (value == deoptimizer->isolate()->heap()->arguments_marker()) {
      value = deoptimizer->isolate()->heap()->undefined_value();
    }
    SetExpression(i, value);
  }

  if (has_arguments_adaptor) {
    output_frame = deoptimizer->output_[frame_index - 1];
    CHECK_EQ(output_frame->GetFrameType(), StackFrame::ARGUMENTS_ADAPTOR);
  }

  parameters_count_ = output_frame->ComputeParametersCount();
  parameters_ = new Object* [parameters_count_];
  for (int i = 0; i < parameters_count_; i++) {
    Object* value = output_frame->GetParameter(i);
    // Replace materialization markers with the undefined value.
    if (value == deoptimizer->isolate()->heap()->arguments_marker()) {
      value = deoptimizer->isolate()->heap()->undefined_value();
    }
    SetParameter(i, value);
  }
}


DeoptimizedFrameInfo::~DeoptimizedFrameInfo() {
  delete[] expression_stack_;
  delete[] parameters_;
}


void DeoptimizedFrameInfo::Iterate(ObjectVisitor* v) {
  v->VisitPointer(bit_cast<Object**>(&function_));
  v->VisitPointer(&context_);
  v->VisitPointers(parameters_, parameters_ + parameters_count_);
  v->VisitPointers(expression_stack_, expression_stack_ + expression_count_);
}


const char* Deoptimizer::GetDeoptReason(DeoptReason deopt_reason) {
  DCHECK(deopt_reason < kLastDeoptReason);
#define DEOPT_MESSAGES_TEXTS(C, T) T,
  static const char* deopt_messages_[] = {
      DEOPT_MESSAGES_LIST(DEOPT_MESSAGES_TEXTS)};
#undef DEOPT_MESSAGES_TEXTS
  return deopt_messages_[deopt_reason];
}


Deoptimizer::DeoptInfo Deoptimizer::GetDeoptInfo(Code* code, Address pc) {
  SourcePosition last_position = SourcePosition::Unknown();
  Deoptimizer::DeoptReason last_reason = Deoptimizer::kNoReason;
  int mask = RelocInfo::ModeMask(RelocInfo::DEOPT_REASON) |
             RelocInfo::ModeMask(RelocInfo::POSITION);
  for (RelocIterator it(code, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->pc() >= pc) return DeoptInfo(last_position, NULL, last_reason);
    if (info->rmode() == RelocInfo::POSITION) {
      int raw_position = static_cast<int>(info->data());
      last_position = raw_position ? SourcePosition::FromRaw(raw_position)
                                   : SourcePosition::Unknown();
    } else if (info->rmode() == RelocInfo::DEOPT_REASON) {
      last_reason = static_cast<Deoptimizer::DeoptReason>(info->data());
    }
  }
  return DeoptInfo(SourcePosition::Unknown(), NULL, Deoptimizer::kNoReason);
}


// static
TranslatedValue TranslatedValue::NewArgumentsObject(TranslatedState* container,
                                                    int length,
                                                    int object_index) {
  TranslatedValue slot(container, kArgumentsObject);
  slot.materialization_info_ = {object_index, length};
  return slot;
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
TranslatedValue TranslatedValue::NewDouble(TranslatedState* container,
                                           double value) {
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


double TranslatedValue::double_value() const {
  DCHECK_EQ(kDouble, kind());
  return double_value_;
}


int TranslatedValue::object_length() const {
  DCHECK(kind() == kArgumentsObject || kind() == kCapturedObject);
  return materialization_info_.length_;
}


int TranslatedValue::object_index() const {
  DCHECK(kind() == kArgumentsObject || kind() == kCapturedObject ||
         kind() == kDuplicatedObject);
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
    case TranslatedValue::kDouble: {
      MaterializeSimple();
      return value_.ToHandleChecked();
    }

    case TranslatedValue::kArgumentsObject:
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
    case kInt32: {
      value_ = Handle<Object>(isolate()->factory()->NewNumber(int32_value()));
      return;
    }

    case kUInt32:
      value_ = Handle<Object>(isolate()->factory()->NewNumber(uint32_value()));
      return;

    case kDouble:
      value_ = Handle<Object>(isolate()->factory()->NewNumber(double_value()));
      return;

    case kCapturedObject:
    case kDuplicatedObject:
    case kArgumentsObject:
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
    case kArgumentsObject:
      return true;
    default:
      return false;
  }
}


int TranslatedValue::GetChildrenCount() const {
  if (kind() == kCapturedObject || kind() == kArgumentsObject) {
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


void TranslatedValue::Handlify() {
  if (kind() == kTagged) {
    value_ = Handle<Object>(raw_literal(), isolate());
    raw_literal_ = nullptr;
  }
}


TranslatedFrame TranslatedFrame::JSFrame(BailoutId node_id,
                                         SharedFunctionInfo* shared_info,
                                         int height) {
  TranslatedFrame frame(kFunction, shared_info->GetIsolate(), shared_info,
                        height);
  frame.node_id_ = node_id;
  return frame;
}


TranslatedFrame TranslatedFrame::InterpretedFrame(
    BailoutId bytecode_offset, SharedFunctionInfo* shared_info, int height) {
  TranslatedFrame frame(kInterpretedFunction, shared_info->GetIsolate(),
                        shared_info, height);
  frame.node_id_ = bytecode_offset;
  return frame;
}


TranslatedFrame TranslatedFrame::AccessorFrame(
    Kind kind, SharedFunctionInfo* shared_info) {
  DCHECK(kind == kSetter || kind == kGetter);
  return TranslatedFrame(kind, shared_info->GetIsolate(), shared_info);
}


TranslatedFrame TranslatedFrame::ArgumentsAdaptorFrame(
    SharedFunctionInfo* shared_info, int height) {
  return TranslatedFrame(kArgumentsAdaptor, shared_info->GetIsolate(),
                         shared_info, height);
}


TranslatedFrame TranslatedFrame::ConstructStubFrame(
    SharedFunctionInfo* shared_info, int height) {
  return TranslatedFrame(kConstructStub, shared_info->GetIsolate(), shared_info,
                         height);
}


int TranslatedFrame::GetValueCount() {
  switch (kind()) {
    case kFunction: {
      int parameter_count =
          raw_shared_info_->internal_formal_parameter_count() + 1;
      // + 1 for function.
      return height_ + parameter_count + 1;
    }

    case kInterpretedFunction: {
      int parameter_count =
          raw_shared_info_->internal_formal_parameter_count() + 1;
      // + 3 for function, context and accumulator.
      return height_ + parameter_count + 3;
    }

    case kGetter:
      return 2;  // Function and receiver.

    case kSetter:
      return 3;  // Function, receiver and the value to set.

    case kArgumentsAdaptor:
    case kConstructStub:
      return 1 + height_;

    case kCompiledStub:
      return height_;

    case kInvalid:
      UNREACHABLE();
      break;
  }
  UNREACHABLE();
  return -1;
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
    case Translation::JS_FRAME: {
      BailoutId node_id = BailoutId(iterator->Next());
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        base::SmartArrayPointer<char> name =
            shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading input frame %s", name.get());
        int arg_count = shared_info->internal_formal_parameter_count() + 1;
        PrintF(trace_file, " => node=%d, args=%d, height=%d; inputs:\n",
               node_id.ToInt(), arg_count, height);
      }
      return TranslatedFrame::JSFrame(node_id, shared_info, height);
    }

    case Translation::INTERPRETED_FRAME: {
      BailoutId bytecode_offset = BailoutId(iterator->Next());
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        base::SmartArrayPointer<char> name =
            shared_info->DebugName()->ToCString();
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
        base::SmartArrayPointer<char> name =
            shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading arguments adaptor frame %s", name.get());
        PrintF(trace_file, " => height=%d; inputs:\n", height);
      }
      return TranslatedFrame::ArgumentsAdaptorFrame(shared_info, height);
    }

    case Translation::CONSTRUCT_STUB_FRAME: {
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      int height = iterator->Next();
      if (trace_file != nullptr) {
        base::SmartArrayPointer<char> name =
            shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading construct stub frame %s", name.get());
        PrintF(trace_file, " => height=%d; inputs:\n", height);
      }
      return TranslatedFrame::ConstructStubFrame(shared_info, height);
    }

    case Translation::GETTER_STUB_FRAME: {
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      if (trace_file != nullptr) {
        base::SmartArrayPointer<char> name =
            shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading getter frame %s; inputs:\n", name.get());
      }
      return TranslatedFrame::AccessorFrame(TranslatedFrame::kGetter,
                                            shared_info);
    }

    case Translation::SETTER_STUB_FRAME: {
      SharedFunctionInfo* shared_info =
          SharedFunctionInfo::cast(literal_array->get(iterator->Next()));
      if (trace_file != nullptr) {
        base::SmartArrayPointer<char> name =
            shared_info->DebugName()->ToCString();
        PrintF(trace_file, "  reading setter frame %s; inputs:\n", name.get());
      }
      return TranslatedFrame::AccessorFrame(TranslatedFrame::kSetter,
                                            shared_info);
    }

    case Translation::COMPILED_STUB_FRAME: {
      int height = iterator->Next();
      if (trace_file != nullptr) {
        PrintF(trace_file,
               "  reading compiler stub frame => height=%d; inputs:\n", height);
      }
      return TranslatedFrame::CompiledStubFrame(height,
                                                literal_array->GetIsolate());
    }

    case Translation::BEGIN:
    case Translation::DUPLICATED_OBJECT:
    case Translation::ARGUMENTS_OBJECT:
    case Translation::CAPTURED_OBJECT:
    case Translation::REGISTER:
    case Translation::INT32_REGISTER:
    case Translation::UINT32_REGISTER:
    case Translation::BOOL_REGISTER:
    case Translation::DOUBLE_REGISTER:
    case Translation::STACK_SLOT:
    case Translation::INT32_STACK_SLOT:
    case Translation::UINT32_STACK_SLOT:
    case Translation::BOOL_STACK_SLOT:
    case Translation::DOUBLE_STACK_SLOT:
    case Translation::LITERAL:
    case Translation::JS_FRAME_FUNCTION:
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


// We can't intermix stack decoding and allocations because
// deoptimization infrastracture is not GC safe.
// Thus we build a temporary structure in malloced space.
TranslatedValue TranslatedState::CreateNextTranslatedValue(
    int frame_index, int value_index, TranslationIterator* iterator,
    FixedArray* literal_array, Address fp, RegisterValues* registers,
    FILE* trace_file) {
  disasm::NameConverter converter;

  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  switch (opcode) {
    case Translation::BEGIN:
    case Translation::JS_FRAME:
    case Translation::INTERPRETED_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::GETTER_STUB_FRAME:
    case Translation::SETTER_STUB_FRAME:
    case Translation::COMPILED_STUB_FRAME:
      // Peeled off before getting here.
      break;

    case Translation::DUPLICATED_OBJECT: {
      int object_id = iterator->Next();
      if (trace_file != nullptr) {
        PrintF(trace_file, "duplicated object #%d", object_id);
      }
      object_positions_.push_back(object_positions_[object_id]);
      return TranslatedValue::NewDuplicateObject(this, object_id);
    }

    case Translation::ARGUMENTS_OBJECT: {
      int arg_count = iterator->Next();
      int object_index = static_cast<int>(object_positions_.size());
      if (trace_file != nullptr) {
        PrintF(trace_file, "argumets object #%d (length = %d)", object_index,
               arg_count);
      }
      object_positions_.push_back({frame_index, value_index});
      return TranslatedValue::NewArgumentsObject(this, arg_count, object_index);
    }

    case Translation::CAPTURED_OBJECT: {
      int field_count = iterator->Next();
      int object_index = static_cast<int>(object_positions_.size());
      if (trace_file != nullptr) {
        PrintF(trace_file, "captured object #%d (length = %d)", object_index,
               field_count);
      }
      object_positions_.push_back({frame_index, value_index});
      return TranslatedValue::NewDeferredObject(this, field_count,
                                                object_index);
    }

    case Translation::REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) return TranslatedValue::NewInvalid(this);
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "0x%08" V8PRIxPTR " ; %s ", value,
               converter.NameOfCPURegister(input_reg));
        reinterpret_cast<Object*>(value)->ShortPrint(trace_file);
      }
      return TranslatedValue::NewTagged(this, reinterpret_cast<Object*>(value));
    }

    case Translation::INT32_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) return TranslatedValue::NewInvalid(this);
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%" V8PRIdPTR " ; %s ", value,
               converter.NameOfCPURegister(input_reg));
      }
      return TranslatedValue::NewInt32(this, static_cast<int32_t>(value));
    }

    case Translation::UINT32_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) return TranslatedValue::NewInvalid(this);
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%" V8PRIuPTR " ; %s (uint)", value,
               converter.NameOfCPURegister(input_reg));
        reinterpret_cast<Object*>(value)->ShortPrint(trace_file);
      }
      return TranslatedValue::NewUInt32(this, static_cast<uint32_t>(value));
    }

    case Translation::BOOL_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) return TranslatedValue::NewInvalid(this);
      intptr_t value = registers->GetRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%" V8PRIdPTR " ; %s (bool)", value,
               converter.NameOfCPURegister(input_reg));
      }
      return TranslatedValue::NewBool(this, static_cast<uint32_t>(value));
    }

    case Translation::DOUBLE_REGISTER: {
      int input_reg = iterator->Next();
      if (registers == nullptr) return TranslatedValue::NewInvalid(this);
      double value = registers->GetDoubleRegister(input_reg);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%e ; %s (bool)", value,
               DoubleRegister::from_code(input_reg).ToString());
      }
      return TranslatedValue::NewDouble(this, value);
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
      return TranslatedValue::NewTagged(this, reinterpret_cast<Object*>(value));
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
      return TranslatedValue::NewInt32(this, value);
    }

    case Translation::UINT32_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      uint32_t value = GetUInt32Slot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%u ; (uint) [fp %c %d] ", value,
               slot_offset < 0 ? '-' : '+', std::abs(slot_offset));
      }
      return TranslatedValue::NewUInt32(this, value);
    }

    case Translation::BOOL_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      uint32_t value = GetUInt32Slot(fp, slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%u ; (bool) [fp %c %d] ", value,
               slot_offset < 0 ? '-' : '+', std::abs(slot_offset));
      }
      return TranslatedValue::NewBool(this, value);
    }

    case Translation::DOUBLE_STACK_SLOT: {
      int slot_offset =
          OptimizedFrame::StackSlotOffsetRelativeToFp(iterator->Next());
      double value = ReadDoubleValue(fp + slot_offset);
      if (trace_file != nullptr) {
        PrintF(trace_file, "%e ; (double) [fp %c %d] ", value,
               slot_offset < 0 ? '-' : '+', std::abs(slot_offset));
      }
      return TranslatedValue::NewDouble(this, value);
    }

    case Translation::LITERAL: {
      int literal_index = iterator->Next();
      Object* value = literal_array->get(literal_index);
      if (trace_file != nullptr) {
        PrintF(trace_file, "0x%08" V8PRIxPTR " ; (literal %d) ",
               reinterpret_cast<intptr_t>(value), literal_index);
        reinterpret_cast<Object*>(value)->ShortPrint(trace_file);
      }

      return TranslatedValue::NewTagged(this, value);
    }

    case Translation::JS_FRAME_FUNCTION: {
      int slot_offset = JavaScriptFrameConstants::kFunctionOffset;
      intptr_t value = *(reinterpret_cast<intptr_t*>(fp + slot_offset));
      if (trace_file != nullptr) {
        PrintF(trace_file, "0x%08" V8PRIxPTR " ; (frame function) ", value);
        reinterpret_cast<Object*>(value)->ShortPrint(trace_file);
      }
      return TranslatedValue::NewTagged(this, reinterpret_cast<Object*>(value));
    }
  }

  FATAL("We should never get here - unexpected deopt info.");
  return TranslatedValue(nullptr, TranslatedValue::kInvalid);
}


TranslatedState::TranslatedState(JavaScriptFrame* frame)
    : isolate_(nullptr),
      stack_frame_pointer_(nullptr),
      has_adapted_arguments_(false) {
  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationInputData* data =
      static_cast<OptimizedFrame*>(frame)->GetDeoptimizationData(&deopt_index);
  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Init(frame->fp(), &it, data->LiteralArray(), nullptr /* registers */,
       nullptr /* trace file */);
}


TranslatedState::TranslatedState()
    : isolate_(nullptr),
      stack_frame_pointer_(nullptr),
      has_adapted_arguments_(false) {}


void TranslatedState::Init(Address input_frame_pointer,
                           TranslationIterator* iterator,
                           FixedArray* literal_array, RegisterValues* registers,
                           FILE* trace_file) {
  DCHECK(frames_.empty());

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
  for (int i = 0; i < count; i++) {
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

      TranslatedValue value = CreateNextTranslatedValue(
          i, static_cast<int>(frame.values_.size()), iterator, literal_array,
          input_frame_pointer, registers, trace_file);
      frame.Add(value);

      if (trace_file != nullptr) {
        PrintF(trace_file, "\n");
      }

      // Update the value count and resolve the nesting.
      values_to_process--;
      int children_count = value.GetChildrenCount();
      if (children_count > 0) {
        nested_counts.push(values_to_process);
        values_to_process = children_count;
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


void TranslatedState::Prepare(bool has_adapted_arguments,
                              Address stack_frame_pointer) {
  for (auto& frame : frames_) frame.Handlify();

  stack_frame_pointer_ = stack_frame_pointer;
  has_adapted_arguments_ = has_adapted_arguments;

  UpdateFromPreviouslyMaterializedObjects();
}


Handle<Object> TranslatedState::MaterializeAt(int frame_index,
                                              int* value_index) {
  TranslatedFrame* frame = &(frames_[frame_index]);
  DCHECK(static_cast<size_t>(*value_index) < frame->values_.size());

  TranslatedValue* slot = &(frame->values_[*value_index]);
  (*value_index)++;

  switch (slot->kind()) {
    case TranslatedValue::kTagged:
    case TranslatedValue::kInt32:
    case TranslatedValue::kUInt32:
    case TranslatedValue::kBoolBit:
    case TranslatedValue::kDouble: {
      slot->MaterializeSimple();
      Handle<Object> value = slot->GetValue();
      if (value->IsMutableHeapNumber()) {
        HeapNumber::cast(*value)->set_map(isolate()->heap()->heap_number_map());
      }
      return value;
    }

    case TranslatedValue::kArgumentsObject: {
      int length = slot->GetChildrenCount();
      Handle<JSObject> arguments;
      if (GetAdaptedArguments(&arguments, frame_index)) {
        // Store the materialized object and consume the nested values.
        for (int i = 0; i < length; ++i) {
          MaterializeAt(frame_index, value_index);
        }
      } else {
        Handle<JSFunction> function =
            Handle<JSFunction>::cast(frame->front().GetValue());
        arguments = isolate_->factory()->NewArgumentsObject(function, length);
        Handle<FixedArray> array = isolate_->factory()->NewFixedArray(length);
        DCHECK_EQ(array->length(), length);
        arguments->set_elements(*array);
        for (int i = 0; i < length; ++i) {
          Handle<Object> value = MaterializeAt(frame_index, value_index);
          array->set(i, *value);
        }
      }
      slot->value_ = arguments;
      return arguments;
    }
    case TranslatedValue::kCapturedObject: {
      int length = slot->GetChildrenCount();

      // The map must be a tagged object.
      CHECK(frame->values_[*value_index].kind() == TranslatedValue::kTagged);

      Handle<Object> result;
      if (slot->value_.ToHandle(&result)) {
        // This has been previously materialized, return the previous value.
        // We still need to skip all the nested objects.
        for (int i = 0; i < length; i++) {
          MaterializeAt(frame_index, value_index);
        }

        return result;
      }

      Handle<Object> map_object = MaterializeAt(frame_index, value_index);
      Handle<Map> map =
          Map::GeneralizeAllFieldRepresentations(Handle<Map>::cast(map_object));
      switch (map->instance_type()) {
        case MUTABLE_HEAP_NUMBER_TYPE:
        case HEAP_NUMBER_TYPE: {
          // Reuse the HeapNumber value directly as it is already properly
          // tagged and skip materializing the HeapNumber explicitly.
          Handle<Object> object = MaterializeAt(frame_index, value_index);
          slot->value_ = object;
          // On 32-bit architectures, there is an extra slot there because
          // the escape analysis calculates the number of slots as
          // object-size/pointer-size. To account for this, we read out
          // any extra slots.
          for (int i = 0; i < length - 2; i++) {
            MaterializeAt(frame_index, value_index);
          }
          return object;
        }
        case JS_OBJECT_TYPE: {
          Handle<JSObject> object =
              isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED);
          slot->value_ = object;
          Handle<Object> properties = MaterializeAt(frame_index, value_index);
          Handle<Object> elements = MaterializeAt(frame_index, value_index);
          object->set_properties(FixedArray::cast(*properties));
          object->set_elements(FixedArrayBase::cast(*elements));
          for (int i = 0; i < length - 3; ++i) {
            Handle<Object> value = MaterializeAt(frame_index, value_index);
            FieldIndex index = FieldIndex::ForPropertyIndex(object->map(), i);
            object->FastPropertyAtPut(index, *value);
          }
          return object;
        }
        case JS_ARRAY_TYPE: {
          Handle<JSArray> object =
              isolate_->factory()->NewJSArray(0, map->elements_kind());
          slot->value_ = object;
          Handle<Object> properties = MaterializeAt(frame_index, value_index);
          Handle<Object> elements = MaterializeAt(frame_index, value_index);
          Handle<Object> length = MaterializeAt(frame_index, value_index);
          object->set_properties(FixedArray::cast(*properties));
          object->set_elements(FixedArrayBase::cast(*elements));
          object->set_length(*length);
          return object;
        }
        case FIXED_ARRAY_TYPE: {
          Handle<Object> lengthObject = MaterializeAt(frame_index, value_index);
          int32_t length = 0;
          CHECK(lengthObject->ToInt32(&length));
          Handle<FixedArray> object =
              isolate_->factory()->NewFixedArray(length);
          // We need to set the map, because the fixed array we are
          // materializing could be a context or an arguments object,
          // in which case we must retain that information.
          object->set_map(*map);
          slot->value_ = object;
          for (int i = 0; i < length; ++i) {
            Handle<Object> value = MaterializeAt(frame_index, value_index);
            object->set(i, *value);
          }
          return object;
        }
        case FIXED_DOUBLE_ARRAY_TYPE: {
          DCHECK_EQ(*map, isolate_->heap()->fixed_double_array_map());
          Handle<Object> lengthObject = MaterializeAt(frame_index, value_index);
          int32_t length = 0;
          CHECK(lengthObject->ToInt32(&length));
          Handle<FixedArrayBase> object =
              isolate_->factory()->NewFixedDoubleArray(length);
          slot->value_ = object;
          if (length > 0) {
            Handle<FixedDoubleArray> double_array =
                Handle<FixedDoubleArray>::cast(object);
            for (int i = 0; i < length; ++i) {
              Handle<Object> value = MaterializeAt(frame_index, value_index);
              CHECK(value->IsNumber());
              double_array->set(i, value->Number());
            }
          }
          return object;
        }
        default:
          PrintF(stderr, "[couldn't handle instance type %d]\n",
                 map->instance_type());
          FATAL("unreachable");
          return Handle<Object>::null();
      }
      UNREACHABLE();
      break;
    }

    case TranslatedValue::kDuplicatedObject: {
      int object_index = slot->object_index();
      TranslatedState::ObjectPosition pos = object_positions_[object_index];

      // Make sure the duplicate is refering to a previous object.
      DCHECK(pos.frame_index_ < frame_index ||
             (pos.frame_index_ == frame_index &&
              pos.value_index_ < *value_index - 1));

      Handle<Object> object =
          frames_[pos.frame_index_].values_[pos.value_index_].GetValue();

      // The object should have a (non-sentinel) value.
      DCHECK(!object.is_null() &&
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
  TranslatedState::ObjectPosition pos = object_positions_[object_index];
  return MaterializeAt(pos.frame_index_, &(pos.value_index_));
}


bool TranslatedState::GetAdaptedArguments(Handle<JSObject>* result,
                                          int frame_index) {
  if (frame_index == 0) {
    // Top level frame -> we need to go to the parent frame on the stack.
    if (!has_adapted_arguments_) return false;

    // This is top level frame, so we need to go to the stack to get
    // this function's argument. (Note that this relies on not inlining
    // recursive functions!)
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(frames_[frame_index].front().GetValue());
    *result = Handle<JSObject>::cast(Accessors::FunctionGetArguments(function));
    return true;
  } else {
    TranslatedFrame* previous_frame = &(frames_[frame_index]);
    if (previous_frame->kind() != TranslatedFrame::kArgumentsAdaptor) {
      return false;
    }
    // We get the adapted arguments from the parent translation.
    int length = previous_frame->height();
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(previous_frame->front().GetValue());
    Handle<JSObject> arguments =
        isolate_->factory()->NewArgumentsObject(function, length);
    Handle<FixedArray> array = isolate_->factory()->NewFixedArray(length);
    arguments->set_elements(*array);
    TranslatedFrame::iterator arg_iterator = previous_frame->begin();
    arg_iterator++;  // Skip function.
    for (int i = 0; i < length; ++i) {
      Handle<Object> value = arg_iterator->GetValue();
      array->set(i, *value);
      arg_iterator++;
    }
    CHECK(arg_iterator == previous_frame->end());
    *result = arguments;
    return true;
  }
}


TranslatedFrame* TranslatedState::GetArgumentsInfoFromJSFrameIndex(
    int jsframe_index, int* args_count) {
  for (size_t i = 0; i < frames_.size(); i++) {
    if (frames_[i].kind() == TranslatedFrame::kFunction) {
      if (jsframe_index > 0) {
        jsframe_index--;
      } else {
        // We have the JS function frame, now check if it has arguments adaptor.
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


void TranslatedState::StoreMaterializedValuesAndDeopt() {
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

  DCHECK_EQ(length, previously_materialized_objects->length());

  bool value_changed = false;
  for (int i = 0; i < length; i++) {
    TranslatedState::ObjectPosition pos = object_positions_[i];
    TranslatedValue* value_info =
        &(frames_[pos.frame_index_].values_[pos.value_index_]);

    DCHECK(value_info->IsMaterializedObject());

    Handle<Object> value(value_info->GetRawValue(), isolate_);

    if (!value.is_identical_to(marker)) {
      if (previously_materialized_objects->get(i) == *marker) {
        previously_materialized_objects->set(i, *value);
        value_changed = true;
      } else {
        DCHECK(previously_materialized_objects->get(i) == *value);
      }
    }
  }
  if (new_store && value_changed) {
    materialized_store->Set(stack_frame_pointer_,
                            previously_materialized_objects);
    DCHECK_EQ(TranslatedFrame::kFunction, frames_[0].kind());
    Object* const function = frames_[0].front().GetRawValue();
    Deoptimizer::DeoptimizeFunction(JSFunction::cast(function));
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
  DCHECK_EQ(length, previously_materialized_objects->length());

  for (int i = 0; i < length; i++) {
    // For a previously materialized objects, inject their value into the
    // translated values.
    if (previously_materialized_objects->get(i) != *marker) {
      TranslatedState::ObjectPosition pos = object_positions_[i];
      TranslatedValue* value_info =
          &(frames_[pos.frame_index_].values_[pos.value_index_]);
      DCHECK(value_info->IsMaterializedObject());

      value_info->value_ =
          Handle<Object>(previously_materialized_objects->get(i), isolate_);
    }
  }
}

}  // namespace internal
}  // namespace v8
