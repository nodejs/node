// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"

#include "accessors.h"
#include "codegen.h"
#include "deoptimizer.h"
#include "disasm.h"
#include "full-codegen.h"
#include "global-handles.h"
#include "macro-assembler.h"
#include "prettyprinter.h"


namespace v8 {
namespace internal {

static MemoryChunk* AllocateCodeChunk(MemoryAllocator* allocator) {
  return allocator->AllocateChunk(Deoptimizer::GetMaxDeoptTableSize(),
                                  OS::CommitPageSize(),
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
  int commit_page_size = static_cast<int>(OS::CommitPageSize());
  int page_count = ((kDeoptTableMaxEpilogueCodeSize + entries_size - 1) /
                    commit_page_size) + 1;
  return static_cast<size_t>(commit_page_size * page_count);
}


Deoptimizer* Deoptimizer::Grab(Isolate* isolate) {
  Deoptimizer* result = isolate->deoptimizer_data()->current_;
  CHECK_NE(result, NULL);
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
  unsigned fp_to_sp_delta = (stack_slots * kPointerSize) +
      StandardFrameConstants::kFixedFrameSizeFromFp;

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

  // Get the "simulated" top and size for the requested frame.
  FrameDescription* parameters_frame =
      deoptimizer->output_[
          has_arguments_adaptor ? (frame_index - 1) : frame_index];

  uint32_t parameters_size = (info->parameters_count() + 1) * kPointerSize;
  Address parameters_top = reinterpret_cast<Address>(
      parameters_frame->GetTop() + (parameters_frame->GetFrameSize() -
                                    parameters_size));

  uint32_t expressions_size = info->expression_count() * kPointerSize;
  Address expressions_top = reinterpret_cast<Address>(
      deoptimizer->output_[frame_index]->GetTop());

  // Done with the GC-unsafe frame descriptions. This re-enables allocation.
  deoptimizer->DeleteFrameDescriptions();

  // Allocate a heap number for the doubles belonging to this frame.
  deoptimizer->MaterializeHeapNumbersForDebuggerInspectableFrame(
      parameters_top, parameters_size, expressions_top, expressions_size, info);

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
        prev->set_next_function_link(next);
      } else {
        context->SetOptimizedFunctionsListHead(next);
      }
      // The visitor should not alter the link directly.
      CHECK_EQ(function->next_function_link(), next);
      // Set the next function link to undefined to indicate it is no longer
      // in the optimized functions list.
      function->set_next_function_link(context->GetHeap()->undefined_value());
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
      if (FLAG_trace_deopt) {
        JSFunction* function =
            static_cast<OptimizedFrame*>(it.frame())->function();
        CodeTracer::Scope scope(isolate->GetCodeTracer());
        PrintF(scope.file(), "[deoptimizer found activation of function: ");
        function->PrintName(scope.file());
        PrintF(scope.file(),
               " / %" V8PRIxPTR "]\n", reinterpret_cast<intptr_t>(function));
      }
      SafepointEntry safepoint = code->GetSafepointEntry(it.frame()->pc());
      int deopt_index = safepoint.deoptimization_index();
      bool safe_to_deopt = deopt_index != Safepoint::kNoDeoptimizationIndex;
      CHECK(topmost_optimized_code == NULL || safe_to_deopt);
      if (topmost_optimized_code == NULL) {
        topmost_optimized_code = code;
        safe_to_deopt_topmost_optimized_code = safe_to_deopt;
      }
    }
  }
#endif

  // Move marked code from the optimized code list to the deoptimized
  // code list, collecting them into a ZoneList.
  Zone zone(isolate);
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
      ASSERT(safe_to_deopt_topmost_optimized_code);
    }
#endif
    // It is finally time to die, code object.
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


void Deoptimizer::DeoptimizeGlobalObject(JSObject* object) {
  if (FLAG_trace_deopt) {
    CodeTracer::Scope scope(object->GetHeap()->isolate()->GetCodeTracer());
    PrintF(scope.file(), "[deoptimize global object @ 0x%08" V8PRIxPTR "]\n",
        reinterpret_cast<intptr_t>(object));
  }
  if (object->IsJSGlobalProxy()) {
    Object* proto = object->GetPrototype();
    CHECK(proto->IsJSGlobalObject());
    Context* native_context = GlobalObject::cast(proto)->native_context();
    MarkAllCodeForContext(native_context);
    DeoptimizeMarkedCodeForContext(native_context);
  } else if (object->IsGlobalObject()) {
    Context* native_context = GlobalObject::cast(object)->native_context();
    MarkAllCodeForContext(native_context);
    DeoptimizeMarkedCodeForContext(native_context);
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


Deoptimizer::Deoptimizer(Isolate* isolate,
                         JSFunction* function,
                         BailoutType type,
                         unsigned bailout_id,
                         Address from,
                         int fp_to_sp_delta,
                         Code* optimized_code)
    : isolate_(isolate),
      function_(function),
      bailout_id_(bailout_id),
      bailout_type_(type),
      from_(from),
      fp_to_sp_delta_(fp_to_sp_delta),
      has_alignment_padding_(0),
      input_(NULL),
      output_count_(0),
      jsframe_count_(0),
      output_(NULL),
      deferred_objects_tagged_values_(0),
      deferred_objects_double_values_(0),
      deferred_objects_(0),
      deferred_heap_numbers_(0),
      jsframe_functions_(0),
      jsframe_has_adapted_arguments_(0),
      materialized_values_(NULL),
      materialized_objects_(NULL),
      materialization_value_index_(0),
      materialization_object_index_(0),
      trace_scope_(NULL) {
  // For COMPILED_STUBs called from builtins, the function pointer is a SMI
  // indicating an internal frame.
  if (function->IsSmi()) {
    function = NULL;
  }
  ASSERT(from != NULL);
  if (function != NULL && function->IsOptimized()) {
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
  ASSERT(compiled_code_ != NULL);
  if (type == EAGER || type == SOFT || type == LAZY) {
    ASSERT(compiled_code_->kind() != Code::FUNCTION);
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
      ASSERT(optimized_code->contains(from_));
      return optimized_code;
  }
  FATAL("Could not find code for optimized function");
  return NULL;
}


void Deoptimizer::PrintFunctionName() {
  if (function_->IsJSFunction()) {
    function_->PrintName(trace_scope_->file());
  } else {
    PrintF(trace_scope_->file(),
           "%s", Code::Kind2String(compiled_code_->kind()));
  }
}


Deoptimizer::~Deoptimizer() {
  ASSERT(input_ == NULL && output_ == NULL);
  ASSERT(disallow_heap_allocation_ == NULL);
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
  if (base == NULL ||
      addr < start ||
      addr >= start + (kMaxNumberOfEntries * table_entry_size_)) {
    return kNotDeoptimizationEntry;
  }
  ASSERT_EQ(0,
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
  PrintF(stderr, "[couldn't find pc offset for node=%d]\n", id.ToInt());
  PrintF(stderr, "[method: %s]\n", shared->DebugName()->ToCString().get());
  // Print the source code if available.
  HeapStringAllocator string_allocator;
  StringStream stream(&string_allocator);
  shared->SourceCodePrint(&stream, -1);
  PrintF(stderr, "[source:\n%s\n]", stream.ToCString().get());

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
      ASSERT(code->kind() == Code::OPTIMIZED_FUNCTION);
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
  // Print some helpful diagnostic information.
  if (FLAG_log_timer_events &&
      compiled_code_->kind() == Code::OPTIMIZED_FUNCTION) {
    LOG(isolate(), CodeDeoptEvent(compiled_code_));
  }
  ElapsedTimer timer;

  // Determine basic deoptimization information.  The optimized frame is
  // described by the input data.
  DeoptimizationInputData* input_data =
      DeoptimizationInputData::cast(compiled_code_->deoptimization_data());

  if (trace_scope_ != NULL) {
    timer.Start();
    PrintF(trace_scope_->file(),
           "[deoptimizing (DEOPT %s): begin 0x%08" V8PRIxPTR " ",
           MessageFor(bailout_type_),
           reinterpret_cast<intptr_t>(function_));
    PrintFunctionName();
    PrintF(trace_scope_->file(),
           " (opt #%d) @%d, FP to SP delta: %d]\n",
           input_data->OptimizationId()->value(),
           bailout_id_,
           fp_to_sp_delta_);
    if (bailout_type_ == EAGER || bailout_type_ == SOFT) {
      compiled_code_->PrintDeoptLocation(trace_scope_->file(), bailout_id_);
    }
  }

  BailoutId node_id = input_data->AstId(bailout_id_);
  ByteArray* translations = input_data->TranslationByteArray();
  unsigned translation_index =
      input_data->TranslationIndex(bailout_id_)->value();

  // Do the input frame to output frame(s) translation.
  TranslationIterator iterator(translations, translation_index);
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator.Next());
  ASSERT(Translation::BEGIN == opcode);
  USE(opcode);
  // Read the number of output frames and allocate an array for their
  // descriptions.
  int count = iterator.Next();
  iterator.Next();  // Drop JS frames count.
  ASSERT(output_ == NULL);
  output_ = new FrameDescription*[count];
  for (int i = 0; i < count; ++i) {
    output_[i] = NULL;
  }
  output_count_ = count;

  Register fp_reg = JavaScriptFrame::fp_register();
  stack_fp_ = reinterpret_cast<Address>(
      input_->GetRegister(fp_reg.code()) +
          has_alignment_padding_ * kPointerSize);

  // Translate each output frame.
  for (int i = 0; i < count; ++i) {
    // Read the ast node id, function, and frame height for this output frame.
    Translation::Opcode opcode =
        static_cast<Translation::Opcode>(iterator.Next());
    switch (opcode) {
      case Translation::JS_FRAME:
        DoComputeJSFrame(&iterator, i);
        jsframe_count_++;
        break;
      case Translation::ARGUMENTS_ADAPTOR_FRAME:
        DoComputeArgumentsAdaptorFrame(&iterator, i);
        break;
      case Translation::CONSTRUCT_STUB_FRAME:
        DoComputeConstructStubFrame(&iterator, i);
        break;
      case Translation::GETTER_STUB_FRAME:
        DoComputeAccessorStubFrame(&iterator, i, false);
        break;
      case Translation::SETTER_STUB_FRAME:
        DoComputeAccessorStubFrame(&iterator, i, true);
        break;
      case Translation::COMPILED_STUB_FRAME:
        DoComputeCompiledStubFrame(&iterator, i);
        break;
      case Translation::BEGIN:
      case Translation::REGISTER:
      case Translation::INT32_REGISTER:
      case Translation::UINT32_REGISTER:
      case Translation::DOUBLE_REGISTER:
      case Translation::STACK_SLOT:
      case Translation::INT32_STACK_SLOT:
      case Translation::UINT32_STACK_SLOT:
      case Translation::DOUBLE_STACK_SLOT:
      case Translation::LITERAL:
      case Translation::ARGUMENTS_OBJECT:
      default:
        FATAL("Unsupported translation");
        break;
    }
  }

  // Print some helpful diagnostic information.
  if (trace_scope_ != NULL) {
    double ms = timer.Elapsed().InMillisecondsF();
    int index = output_count_ - 1;  // Index of the topmost frame.
    JSFunction* function = output_[index]->GetFunction();
    PrintF(trace_scope_->file(),
           "[deoptimizing (%s): end 0x%08" V8PRIxPTR " ",
           MessageFor(bailout_type_),
           reinterpret_cast<intptr_t>(function));
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


void Deoptimizer::DoComputeJSFrame(TranslationIterator* iterator,
                                   int frame_index) {
  BailoutId node_id = BailoutId(iterator->Next());
  JSFunction* function;
  if (frame_index != 0) {
    function = JSFunction::cast(ComputeLiteral(iterator->Next()));
  } else {
    int closure_id = iterator->Next();
    USE(closure_id);
    CHECK_EQ(Translation::kSelfLiteralId, closure_id);
    function = function_;
  }
  unsigned height = iterator->Next();
  unsigned height_in_bytes = height * kPointerSize;
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(), "  translating ");
    function->PrintName(trace_scope_->file());
    PrintF(trace_scope_->file(),
           " => node=%d, height=%d\n", node_id.ToInt(), height_in_bytes);
  }

  // The 'fixed' part of the frame consists of the incoming parameters and
  // the part described by JavaScriptFrameConstants.
  unsigned fixed_frame_size = ComputeFixedSize(function);
  unsigned input_frame_size = input_->GetFrameSize();
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, function);
  output_frame->SetFrameType(StackFrame::JAVA_SCRIPT);

  bool is_bottommost = (0 == frame_index);
  bool is_topmost = (output_count_ - 1 == frame_index);
  CHECK(frame_index >= 0 && frame_index < output_count_);
  CHECK_EQ(output_[frame_index], NULL);
  output_[frame_index] = output_frame;

  // The top address for the bottommost output frame can be computed from
  // the input frame pointer and the output frame's height.  For all
  // subsequent output frames, it can be computed from the previous one's
  // top address and the current frame's size.
  Register fp_reg = JavaScriptFrame::fp_register();
  intptr_t top_address;
  if (is_bottommost) {
    // Determine whether the input frame contains alignment padding.
    has_alignment_padding_ = HasAlignmentPadding(function) ? 1 : 0;
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
  int parameter_count = function->shared()->formal_parameter_count() + 1;
  unsigned output_offset = output_frame_size;
  unsigned input_offset = input_frame_size;
  for (int i = 0; i < parameter_count; ++i) {
    output_offset -= kPointerSize;
    DoTranslateCommand(iterator, frame_index, output_offset);
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
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR  " ; caller's pc\n",
           top_address + output_offset, output_offset, value);
  }

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
  ASSERT(!is_bottommost || (input_->GetRegister(fp_reg.code()) +
      has_alignment_padding_ * kPointerSize) == fp_value);
  output_frame->SetFp(fp_value);
  if (is_topmost) output_frame->SetRegister(fp_reg.code(), fp_value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           fp_value, output_offset, value);
  }
  ASSERT(!is_bottommost || !has_alignment_padding_ ||
         (fp_value & kPointerSize) != 0);

  if (FLAG_enable_ool_constant_pool) {
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
    if (trace_scope_) {
      PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR "; caller's constant_pool\n",
             top_address + output_offset, output_offset, value);
    }
  }

  // For the bottommost output frame the context can be gotten from the input
  // frame. For all subsequent output frames it can be gotten from the function
  // so long as we don't inline functions that need local contexts.
  Register context_reg = JavaScriptFrame::context_register();
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
  } else {
    value = reinterpret_cast<intptr_t>(function->context());
  }
  output_frame->SetFrameSlot(output_offset, value);
  output_frame->SetContext(value);
  if (is_topmost) output_frame->SetRegister(context_reg.code(), value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR "; context\n",
           top_address + output_offset, output_offset, value);
  }

  // The function was mentioned explicitly in the BEGIN_FRAME.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  // The function for the bottommost output frame should also agree with the
  // input frame.
  ASSERT(!is_bottommost || input_->GetFrameSlot(input_offset) == value);
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR "; function\n",
           top_address + output_offset, output_offset, value);
  }

  // Translate the rest of the frame.
  for (unsigned i = 0; i < height; ++i) {
    output_offset -= kPointerSize;
    DoTranslateCommand(iterator, frame_index, output_offset);
  }
  CHECK_EQ(0, output_offset);

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
  if (FLAG_enable_ool_constant_pool) {
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


void Deoptimizer::DoComputeArgumentsAdaptorFrame(TranslationIterator* iterator,
                                                 int frame_index) {
  JSFunction* function = JSFunction::cast(ComputeLiteral(iterator->Next()));
  unsigned height = iterator->Next();
  unsigned height_in_bytes = height * kPointerSize;
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
    DoTranslateCommand(iterator, frame_index, output_offset);
  }

  // Read caller's PC from the previous frame.
  output_offset -= kPCOnStackSize;
  intptr_t callers_pc = output_[frame_index - 1]->GetPc();
  output_frame->SetCallerPc(output_offset, callers_pc);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's pc\n",
           top_address + output_offset, output_offset, callers_pc);
  }

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_offset -= kFPOnStackSize;
  intptr_t value = output_[frame_index - 1]->GetFp();
  output_frame->SetCallerFp(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           fp_value, output_offset, value);
  }

  if (FLAG_enable_ool_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_offset -= kPointerSize;
    value = output_[frame_index - 1]->GetConstantPool();
    output_frame->SetCallerConstantPool(output_offset, value);
    if (trace_scope_) {
      PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR "; caller's constant_pool\n",
             top_address + output_offset, output_offset, value);
    }
  }

  // A marker value is used in place of the context.
  output_offset -= kPointerSize;
  intptr_t context = reinterpret_cast<intptr_t>(
      Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  output_frame->SetFrameSlot(output_offset, context);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; context (adaptor sentinel)\n",
           top_address + output_offset, output_offset, context);
  }

  // The function was mentioned explicitly in the ARGUMENTS_ADAPTOR_FRAME.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; function\n",
           top_address + output_offset, output_offset, value);
  }

  // Number of incoming arguments.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(height - 1));
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; argc (%d)\n",
           top_address + output_offset, output_offset, value, height - 1);
  }

  ASSERT(0 == output_offset);

  Builtins* builtins = isolate_->builtins();
  Code* adaptor_trampoline =
      builtins->builtin(Builtins::kArgumentsAdaptorTrampoline);
  intptr_t pc_value = reinterpret_cast<intptr_t>(
      adaptor_trampoline->instruction_start() +
      isolate_->heap()->arguments_adaptor_deopt_pc_offset()->value());
  output_frame->SetPc(pc_value);
  if (FLAG_enable_ool_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(adaptor_trampoline->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
  }
}


void Deoptimizer::DoComputeConstructStubFrame(TranslationIterator* iterator,
                                              int frame_index) {
  Builtins* builtins = isolate_->builtins();
  Code* construct_stub = builtins->builtin(Builtins::kJSConstructStubGeneric);
  JSFunction* function = JSFunction::cast(ComputeLiteral(iterator->Next()));
  unsigned height = iterator->Next();
  unsigned height_in_bytes = height * kPointerSize;
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
  ASSERT(frame_index > 0 && frame_index < output_count_ - 1);
  ASSERT(output_[frame_index] == NULL);
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
    int deferred_object_index = deferred_objects_.length();
    DoTranslateCommand(iterator, frame_index, output_offset);
    // The allocated receiver of a construct stub frame is passed as the
    // receiver parameter through the translation. It might be encoding
    // a captured object, patch the slot address for a captured object.
    if (i == 0 && deferred_objects_.length() > deferred_object_index) {
      CHECK(!deferred_objects_[deferred_object_index].is_arguments());
      deferred_objects_[deferred_object_index].patch_slot_address(top_address);
    }
  }

  // Read caller's PC from the previous frame.
  output_offset -= kPCOnStackSize;
  intptr_t callers_pc = output_[frame_index - 1]->GetPc();
  output_frame->SetCallerPc(output_offset, callers_pc);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's pc\n",
           top_address + output_offset, output_offset, callers_pc);
  }

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_offset -= kFPOnStackSize;
  intptr_t value = output_[frame_index - 1]->GetFp();
  output_frame->SetCallerFp(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           fp_value, output_offset, value);
  }

  if (FLAG_enable_ool_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_offset -= kPointerSize;
    value = output_[frame_index - 1]->GetConstantPool();
    output_frame->SetCallerConstantPool(output_offset, value);
    if (trace_scope_) {
      PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR " ; caller's constant pool\n",
             top_address + output_offset, output_offset, value);
    }
  }

  // The context can be gotten from the previous frame.
  output_offset -= kPointerSize;
  value = output_[frame_index - 1]->GetContext();
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; context\n",
           top_address + output_offset, output_offset, value);
  }

  // A marker value is used in place of the function.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(StackFrame::CONSTRUCT));
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; function (construct sentinel)\n",
           top_address + output_offset, output_offset, value);
  }

  // The output frame reflects a JSConstructStubGeneric frame.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(construct_stub);
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; code object\n",
           top_address + output_offset, output_offset, value);
  }

  // Number of incoming arguments.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(height - 1));
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; argc (%d)\n",
           top_address + output_offset, output_offset, value, height - 1);
  }

  // Constructor function being invoked by the stub (only present on some
  // architectures, indicated by kConstructorOffset).
  if (ConstructFrameConstants::kConstructorOffset != kMinInt) {
    output_offset -= kPointerSize;
    value = reinterpret_cast<intptr_t>(function);
    output_frame->SetFrameSlot(output_offset, value);
    if (trace_scope_ != NULL) {
      PrintF(trace_scope_->file(),
             "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR " ; constructor function\n",
             top_address + output_offset, output_offset, value);
    }
  }

  // The newly allocated object was passed as receiver in the artificial
  // constructor stub environment created by HEnvironment::CopyForInlining().
  output_offset -= kPointerSize;
  value = output_frame->GetFrameSlot(output_frame_size - kPointerSize);
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; allocated receiver\n",
           top_address + output_offset, output_offset, value);
  }

  CHECK_EQ(0, output_offset);

  intptr_t pc = reinterpret_cast<intptr_t>(
      construct_stub->instruction_start() +
      isolate_->heap()->construct_stub_deopt_pc_offset()->value());
  output_frame->SetPc(pc);
  if (FLAG_enable_ool_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(construct_stub->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
  }
}


void Deoptimizer::DoComputeAccessorStubFrame(TranslationIterator* iterator,
                                             int frame_index,
                                             bool is_setter_stub_frame) {
  JSFunction* accessor = JSFunction::cast(ComputeLiteral(iterator->Next()));
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
  // pool (if FLAG_enable_ool_constant_pool)- see MacroAssembler::EnterFrame).
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
  CHECK_EQ(output_[frame_index], NULL);
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
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; caller's pc\n",
           top_address + output_offset, output_offset, callers_pc);
  }

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_offset -= kFPOnStackSize;
  intptr_t value = output_[frame_index - 1]->GetFp();
  output_frame->SetCallerFp(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; caller's fp\n",
           fp_value, output_offset, value);
  }

  if (FLAG_enable_ool_constant_pool) {
    // Read the caller's constant pool from the previous frame.
    output_offset -= kPointerSize;
    value = output_[frame_index - 1]->GetConstantPool();
    output_frame->SetCallerConstantPool(output_offset, value);
    if (trace_scope_) {
      PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR " ; caller's constant pool\n",
             top_address + output_offset, output_offset, value);
    }
  }

  // The context can be gotten from the previous frame.
  output_offset -= kPointerSize;
  value = output_[frame_index - 1]->GetContext();
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; context\n",
           top_address + output_offset, output_offset, value);
  }

  // A marker value is used in place of the function.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(StackFrame::INTERNAL));
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; function (%s sentinel)\n",
           top_address + output_offset, output_offset, value, kind);
  }

  // Get Code object from accessor stub.
  output_offset -= kPointerSize;
  Builtins::Name name = is_setter_stub_frame ?
      Builtins::kStoreIC_Setter_ForDeopt :
      Builtins::kLoadIC_Getter_ForDeopt;
  Code* accessor_stub = isolate_->builtins()->builtin(name);
  value = reinterpret_cast<intptr_t>(accessor_stub);
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; code object\n",
           top_address + output_offset, output_offset, value);
  }

  // Skip receiver.
  DoTranslateObjectAndSkip(iterator);

  if (is_setter_stub_frame) {
    // The implicit return value was part of the artificial setter stub
    // environment.
    output_offset -= kPointerSize;
    DoTranslateCommand(iterator, frame_index, output_offset);
  }

  CHECK_EQ(output_offset, 0);

  Smi* offset = is_setter_stub_frame ?
      isolate_->heap()->setter_stub_deopt_pc_offset() :
      isolate_->heap()->getter_stub_deopt_pc_offset();
  intptr_t pc = reinterpret_cast<intptr_t>(
      accessor_stub->instruction_start() + offset->value());
  output_frame->SetPc(pc);
  if (FLAG_enable_ool_constant_pool) {
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(accessor_stub->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
  }
}


void Deoptimizer::DoComputeCompiledStubFrame(TranslationIterator* iterator,
                                             int frame_index) {
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

  CHECK(compiled_code_->is_crankshafted() &&
        compiled_code_->kind() != Code::OPTIMIZED_FUNCTION);
  int major_key = compiled_code_->major_key();
  CodeStubInterfaceDescriptor* descriptor =
      isolate_->code_stub_interface_descriptor(major_key);

  // The output frame must have room for all pushed register parameters
  // and the standard stack frame slots.  Include space for an argument
  // object to the callee and optionally the space to pass the argument
  // object to the stub failure handler.
  CHECK_GE(descriptor->register_param_count_, 0);
  int height_in_bytes = kPointerSize * descriptor->register_param_count_ +
      sizeof(Arguments) + kPointerSize;
  int fixed_frame_size = StandardFrameConstants::kFixedFrameSize;
  int input_frame_size = input_->GetFrameSize();
  int output_frame_size = height_in_bytes + fixed_frame_size;
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "  translating %s => StubFailureTrampolineStub, height=%d\n",
           CodeStub::MajorName(static_cast<CodeStub::Major>(major_key), false),
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
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's pc\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  // Read caller's FP from the input frame, and set this frame's FP.
  input_frame_offset -= kFPOnStackSize;
  value = input_->GetFrameSlot(input_frame_offset);
  output_frame_offset -= kFPOnStackSize;
  output_frame->SetCallerFp(output_frame_offset, value);
  intptr_t frame_ptr = input_->GetRegister(fp_reg.code());
  output_frame->SetRegister(fp_reg.code(), frame_ptr);
  output_frame->SetFp(frame_ptr);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  if (FLAG_enable_ool_constant_pool) {
    // Read the caller's constant pool from the input frame.
    input_frame_offset -= kPointerSize;
    value = input_->GetFrameSlot(input_frame_offset);
    output_frame_offset -= kPointerSize;
    output_frame->SetCallerConstantPool(output_frame_offset, value);
    if (trace_scope_) {
      PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR " ; caller's constant_pool\n",
             top_address + output_frame_offset, output_frame_offset, value);
    }
  }

  // The context can be gotten from the input frame.
  Register context_reg = StubFailureTrampolineFrame::context_register();
  input_frame_offset -= kPointerSize;
  value = input_->GetFrameSlot(input_frame_offset);
  output_frame->SetRegister(context_reg.code(), value);
  output_frame_offset -= kPointerSize;
  output_frame->SetFrameSlot(output_frame_offset, value);
  CHECK(reinterpret_cast<Object*>(value)->IsContext());
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; context\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  // A marker value is used in place of the function.
  output_frame_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(
      Smi::FromInt(StackFrame::STUB_FAILURE_TRAMPOLINE));
  output_frame->SetFrameSlot(output_frame_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; function (stub failure sentinel)\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  intptr_t caller_arg_count = 0;
  bool arg_count_known = !descriptor->stack_parameter_count_.is_valid();

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
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; args.arguments %s\n",
           top_address + args_arguments_offset, args_arguments_offset, value,
           arg_count_known ? "" : "(the hole)");
  }

  output_frame_offset -= kPointerSize;
  int length_frame_offset = output_frame_offset;
  value = arg_count_known ? caller_arg_count : the_hole;
  output_frame->SetFrameSlot(length_frame_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; args.length %s\n",
           top_address + length_frame_offset, length_frame_offset, value,
           arg_count_known ? "" : "(the hole)");
  }

  output_frame_offset -= kPointerSize;
  value = frame_ptr + StandardFrameConstants::kCallerSPOffset -
      (output_frame_size - output_frame_offset) + kPointerSize;
  output_frame->SetFrameSlot(output_frame_offset, value);
  if (trace_scope_ != NULL) {
    PrintF(trace_scope_->file(),
           "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; args*\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  // Copy the register parameters to the failure frame.
  int arguments_length_offset = -1;
  for (int i = 0; i < descriptor->register_param_count_; ++i) {
    output_frame_offset -= kPointerSize;
    DoTranslateCommand(iterator, 0, output_frame_offset);

    if (!arg_count_known && descriptor->IsParameterCountRegister(i)) {
      arguments_length_offset = output_frame_offset;
    }
  }

  CHECK_EQ(output_frame_offset, 0);

  if (!arg_count_known) {
    CHECK_GE(arguments_length_offset, 0);
    // We know it's a smi because 1) the code stub guarantees the stack
    // parameter count is in smi range, and 2) the DoTranslateCommand in the
    // parameter loop above translated that to a tagged value.
    Smi* smi_caller_arg_count = reinterpret_cast<Smi*>(
        output_frame->GetFrameSlot(arguments_length_offset));
    caller_arg_count = smi_caller_arg_count->value();
    output_frame->SetFrameSlot(length_frame_offset, caller_arg_count);
    if (trace_scope_ != NULL) {
      PrintF(trace_scope_->file(),
             "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR " ; args.length\n",
             top_address + length_frame_offset, length_frame_offset,
             caller_arg_count);
    }
    value = frame_ptr + StandardFrameConstants::kCallerSPOffset +
        (caller_arg_count - 1) * kPointerSize;
    output_frame->SetFrameSlot(args_arguments_offset, value);
    if (trace_scope_ != NULL) {
      PrintF(trace_scope_->file(),
             "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR " ; args.arguments\n",
             top_address + args_arguments_offset, args_arguments_offset,
             value);
    }
  }

  // Copy the double registers from the input into the output frame.
  CopyDoubleRegisters(output_frame);

  // Fill registers containing handler and number of parameters.
  SetPlatformCompiledStubRegisters(output_frame, descriptor);

  // Compute this frame's PC, state, and continuation.
  Code* trampoline = NULL;
  StubFunctionMode function_mode = descriptor->function_mode_;
  StubFailureTrampolineStub(isolate_,
                            function_mode).FindCodeInCache(&trampoline);
  ASSERT(trampoline != NULL);
  output_frame->SetPc(reinterpret_cast<intptr_t>(
      trampoline->instruction_start()));
  if (FLAG_enable_ool_constant_pool) {
    Register constant_pool_reg =
        StubFailureTrampolineFrame::constant_pool_pointer_register();
    intptr_t constant_pool_value =
        reinterpret_cast<intptr_t>(trampoline->constant_pool());
    output_frame->SetConstantPool(constant_pool_value);
    output_frame->SetRegister(constant_pool_reg.code(), constant_pool_value);
  }
  output_frame->SetState(Smi::FromInt(FullCodeGenerator::NO_REGISTERS));
  Code* notify_failure = NotifyStubFailureBuiltin();
  output_frame->SetContinuation(
      reinterpret_cast<intptr_t>(notify_failure->entry()));
}


Handle<Object> Deoptimizer::MaterializeNextHeapObject() {
  int object_index = materialization_object_index_++;
  ObjectMaterializationDescriptor desc = deferred_objects_[object_index];
  const int length = desc.object_length();

  if (desc.duplicate_object() >= 0) {
    // Found a previously materialized object by de-duplication.
    object_index = desc.duplicate_object();
    materialized_objects_->Add(Handle<Object>());
  } else if (desc.is_arguments() && ArgumentsObjectIsAdapted(object_index)) {
    // Use the arguments adapter frame we just built to materialize the
    // arguments object. FunctionGetArguments can't throw an exception.
    Handle<JSFunction> function = ArgumentsObjectFunction(object_index);
    Handle<JSObject> arguments = Handle<JSObject>::cast(
        Accessors::FunctionGetArguments(function));
    materialized_objects_->Add(arguments);
    // To keep consistent object counters, we still materialize the
    // nested values (but we throw them away).
    for (int i = 0; i < length; ++i) {
      MaterializeNextValue();
    }
  } else if (desc.is_arguments()) {
    // Construct an arguments object and copy the parameters to a newly
    // allocated arguments object backing store.
    Handle<JSFunction> function = ArgumentsObjectFunction(object_index);
    Handle<JSObject> arguments =
        isolate_->factory()->NewArgumentsObject(function, length);
    Handle<FixedArray> array = isolate_->factory()->NewFixedArray(length);
    ASSERT_EQ(array->length(), length);
    arguments->set_elements(*array);
    materialized_objects_->Add(arguments);
    for (int i = 0; i < length; ++i) {
      Handle<Object> value = MaterializeNextValue();
      array->set(i, *value);
    }
  } else {
    // Dispatch on the instance type of the object to be materialized.
    // We also need to make sure that the representation of all fields
    // in the given object are general enough to hold a tagged value.
    Handle<Map> map = Map::GeneralizeAllFieldRepresentations(
        Handle<Map>::cast(MaterializeNextValue()));
    switch (map->instance_type()) {
      case HEAP_NUMBER_TYPE: {
        // Reuse the HeapNumber value directly as it is already properly
        // tagged and skip materializing the HeapNumber explicitly.
        Handle<Object> object = MaterializeNextValue();
        if (object_index < prev_materialized_count_) {
          materialized_objects_->Add(Handle<Object>(
              previously_materialized_objects_->get(object_index), isolate_));
        } else {
          materialized_objects_->Add(object);
        }
        materialization_value_index_ += kDoubleSize / kPointerSize - 1;
        break;
      }
      case JS_OBJECT_TYPE: {
        Handle<JSObject> object =
            isolate_->factory()->NewJSObjectFromMap(map, NOT_TENURED, false);
        if (object_index < prev_materialized_count_) {
          materialized_objects_->Add(Handle<Object>(
              previously_materialized_objects_->get(object_index), isolate_));
        } else {
          materialized_objects_->Add(object);
        }
        Handle<Object> properties = MaterializeNextValue();
        Handle<Object> elements = MaterializeNextValue();
        object->set_properties(FixedArray::cast(*properties));
        object->set_elements(FixedArrayBase::cast(*elements));
        for (int i = 0; i < length - 3; ++i) {
          Handle<Object> value = MaterializeNextValue();
          object->FastPropertyAtPut(i, *value);
        }
        break;
      }
      case JS_ARRAY_TYPE: {
        Handle<JSArray> object =
            isolate_->factory()->NewJSArray(0, map->elements_kind());
        if (object_index < prev_materialized_count_) {
          materialized_objects_->Add(Handle<Object>(
              previously_materialized_objects_->get(object_index), isolate_));
        } else {
          materialized_objects_->Add(object);
        }
        Handle<Object> properties = MaterializeNextValue();
        Handle<Object> elements = MaterializeNextValue();
        Handle<Object> length = MaterializeNextValue();
        object->set_properties(FixedArray::cast(*properties));
        object->set_elements(FixedArrayBase::cast(*elements));
        object->set_length(*length);
        break;
      }
      default:
        PrintF(stderr,
               "[couldn't handle instance type %d]\n", map->instance_type());
        FATAL("Unsupported instance type");
    }
  }

  return materialized_objects_->at(object_index);
}


Handle<Object> Deoptimizer::MaterializeNextValue() {
  int value_index = materialization_value_index_++;
  Handle<Object> value = materialized_values_->at(value_index);
  if (*value == isolate_->heap()->arguments_marker()) {
    value = MaterializeNextHeapObject();
  }
  return value;
}


void Deoptimizer::MaterializeHeapObjects(JavaScriptFrameIterator* it) {
  ASSERT_NE(DEBUGGER, bailout_type_);

  MaterializedObjectStore* materialized_store =
      isolate_->materialized_object_store();
  previously_materialized_objects_ = materialized_store->Get(stack_fp_);
  prev_materialized_count_ = previously_materialized_objects_.is_null() ?
      0 : previously_materialized_objects_->length();

  // Walk all JavaScript output frames with the given frame iterator.
  for (int frame_index = 0; frame_index < jsframe_count(); ++frame_index) {
    if (frame_index != 0) it->Advance();
    JavaScriptFrame* frame = it->frame();
    jsframe_functions_.Add(handle(frame->function(), isolate_));
    jsframe_has_adapted_arguments_.Add(frame->has_adapted_arguments());
  }

  // Handlify all tagged object values before triggering any allocation.
  List<Handle<Object> > values(deferred_objects_tagged_values_.length());
  for (int i = 0; i < deferred_objects_tagged_values_.length(); ++i) {
    values.Add(Handle<Object>(deferred_objects_tagged_values_[i], isolate_));
  }

  // Play it safe and clear all unhandlified values before we continue.
  deferred_objects_tagged_values_.Clear();

  // Materialize all heap numbers before looking at arguments because when the
  // output frames are used to materialize arguments objects later on they need
  // to already contain valid heap numbers.
  for (int i = 0; i < deferred_heap_numbers_.length(); i++) {
    HeapNumberMaterializationDescriptor<Address> d = deferred_heap_numbers_[i];
    Handle<Object> num = isolate_->factory()->NewNumber(d.value());
    if (trace_scope_ != NULL) {
      PrintF(trace_scope_->file(),
             "Materialized a new heap number %p [%e] in slot %p\n",
             reinterpret_cast<void*>(*num),
             d.value(),
             d.destination());
    }
    Memory::Object_at(d.destination()) = *num;
  }

  // Materialize all heap numbers required for arguments/captured objects.
  for (int i = 0; i < deferred_objects_double_values_.length(); i++) {
    HeapNumberMaterializationDescriptor<int> d =
        deferred_objects_double_values_[i];
    Handle<Object> num = isolate_->factory()->NewNumber(d.value());
    if (trace_scope_ != NULL) {
      PrintF(trace_scope_->file(),
             "Materialized a new heap number %p [%e] for object at %d\n",
             reinterpret_cast<void*>(*num),
             d.value(),
             d.destination());
    }
    ASSERT(values.at(d.destination())->IsTheHole());
    values.Set(d.destination(), num);
  }

  // Play it safe and clear all object double values before we continue.
  deferred_objects_double_values_.Clear();

  // Materialize arguments/captured objects.
  if (!deferred_objects_.is_empty()) {
    List<Handle<Object> > materialized_objects(deferred_objects_.length());
    materialized_objects_ = &materialized_objects;
    materialized_values_ = &values;

    while (materialization_object_index_ < deferred_objects_.length()) {
      int object_index = materialization_object_index_;
      ObjectMaterializationDescriptor descriptor =
          deferred_objects_.at(object_index);

      // Find a previously materialized object by de-duplication or
      // materialize a new instance of the object if necessary. Store
      // the materialized object into the frame slot.
      Handle<Object> object = MaterializeNextHeapObject();
      if (descriptor.slot_address() != NULL) {
        Memory::Object_at(descriptor.slot_address()) = *object;
      }
      if (trace_scope_ != NULL) {
        if (descriptor.is_arguments()) {
          PrintF(trace_scope_->file(),
                 "Materialized %sarguments object of length %d for %p: ",
                 ArgumentsObjectIsAdapted(object_index) ? "(adapted) " : "",
                 Handle<JSObject>::cast(object)->elements()->length(),
                 reinterpret_cast<void*>(descriptor.slot_address()));
        } else {
          PrintF(trace_scope_->file(),
                 "Materialized captured object of size %d for %p: ",
                 Handle<HeapObject>::cast(object)->Size(),
                 reinterpret_cast<void*>(descriptor.slot_address()));
        }
        object->ShortPrint(trace_scope_->file());
        PrintF(trace_scope_->file(), "\n");
      }
    }

    CHECK_EQ(materialization_object_index_, materialized_objects_->length());
    CHECK_EQ(materialization_value_index_, materialized_values_->length());
  }

  if (prev_materialized_count_ > 0) {
    materialized_store->Remove(stack_fp_);
  }
}


void Deoptimizer::MaterializeHeapNumbersForDebuggerInspectableFrame(
    Address parameters_top,
    uint32_t parameters_size,
    Address expressions_top,
    uint32_t expressions_size,
    DeoptimizedFrameInfo* info) {
  CHECK_EQ(DEBUGGER, bailout_type_);
  Address parameters_bottom = parameters_top + parameters_size;
  Address expressions_bottom = expressions_top + expressions_size;
  for (int i = 0; i < deferred_heap_numbers_.length(); i++) {
    HeapNumberMaterializationDescriptor<Address> d = deferred_heap_numbers_[i];

    // Check of the heap number to materialize actually belong to the frame
    // being extracted.
    Address slot = d.destination();
    if (parameters_top <= slot && slot < parameters_bottom) {
      Handle<Object> num = isolate_->factory()->NewNumber(d.value());

      int index = (info->parameters_count() - 1) -
          static_cast<int>(slot - parameters_top) / kPointerSize;

      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "Materializing a new heap number %p [%e] in slot %p"
               "for parameter slot #%d\n",
               reinterpret_cast<void*>(*num),
               d.value(),
               d.destination(),
               index);
      }

      info->SetParameter(index, *num);
    } else if (expressions_top <= slot && slot < expressions_bottom) {
      Handle<Object> num = isolate_->factory()->NewNumber(d.value());

      int index = info->expression_count() - 1 -
          static_cast<int>(slot - expressions_top) / kPointerSize;

      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "Materializing a new heap number %p [%e] in slot %p"
               "for expression slot #%d\n",
               reinterpret_cast<void*>(*num),
               d.value(),
               d.destination(),
               index);
      }

      info->SetExpression(index, *num);
    }
  }
}


static const char* TraceValueType(bool is_smi) {
  if (is_smi) {
    return "smi";
  }

  return "heap number";
}


void Deoptimizer::DoTranslateObjectAndSkip(TranslationIterator* iterator) {
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());

  switch (opcode) {
    case Translation::BEGIN:
    case Translation::JS_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::GETTER_STUB_FRAME:
    case Translation::SETTER_STUB_FRAME:
    case Translation::COMPILED_STUB_FRAME: {
      FATAL("Unexpected frame start translation opcode");
      return;
    }

    case Translation::REGISTER:
    case Translation::INT32_REGISTER:
    case Translation::UINT32_REGISTER:
    case Translation::DOUBLE_REGISTER:
    case Translation::STACK_SLOT:
    case Translation::INT32_STACK_SLOT:
    case Translation::UINT32_STACK_SLOT:
    case Translation::DOUBLE_STACK_SLOT:
    case Translation::LITERAL: {
      // The value is not part of any materialized object, so we can ignore it.
      iterator->Skip(Translation::NumberOfOperandsFor(opcode));
      return;
    }

    case Translation::DUPLICATED_OBJECT: {
      int object_index = iterator->Next();
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(), "      skipping object ");
        PrintF(trace_scope_->file(),
               " ; duplicate of object #%d\n", object_index);
      }
      AddObjectDuplication(0, object_index);
      return;
    }

    case Translation::ARGUMENTS_OBJECT:
    case Translation::CAPTURED_OBJECT: {
      int length = iterator->Next();
      bool is_args = opcode == Translation::ARGUMENTS_OBJECT;
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(), "    skipping object ");
        PrintF(trace_scope_->file(),
               " ; object (length = %d, is_args = %d)\n", length, is_args);
      }

      AddObjectStart(0, length, is_args);

      // We save the object values on the side and materialize the actual
      // object after the deoptimized frame is built.
      int object_index = deferred_objects_.length() - 1;
      for (int i = 0; i < length; i++) {
        DoTranslateObject(iterator, object_index, i);
      }
      return;
    }
  }

  FATAL("Unexpected translation opcode");
}


void Deoptimizer::DoTranslateObject(TranslationIterator* iterator,
                                    int object_index,
                                    int field_index) {
  disasm::NameConverter converter;
  Address object_slot = deferred_objects_[object_index].slot_address();

  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());

  switch (opcode) {
    case Translation::BEGIN:
    case Translation::JS_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::GETTER_STUB_FRAME:
    case Translation::SETTER_STUB_FRAME:
    case Translation::COMPILED_STUB_FRAME:
      FATAL("Unexpected frame start translation opcode");
      return;

    case Translation::REGISTER: {
      int input_reg = iterator->Next();
      intptr_t input_value = input_->GetRegister(input_reg);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        PrintF(trace_scope_->file(),
               "0x%08" V8PRIxPTR " ; %s ", input_value,
               converter.NameOfCPURegister(input_reg));
        reinterpret_cast<Object*>(input_value)->ShortPrint(
            trace_scope_->file());
        PrintF(trace_scope_->file(),
               "\n");
      }
      AddObjectTaggedValue(input_value);
      return;
    }

    case Translation::INT32_REGISTER: {
      int input_reg = iterator->Next();
      intptr_t value = input_->GetRegister(input_reg);
      bool is_smi = Smi::IsValid(value);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        PrintF(trace_scope_->file(),
               "%" V8PRIdPTR " ; %s (%s)\n", value,
               converter.NameOfCPURegister(input_reg),
               TraceValueType(is_smi));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        AddObjectTaggedValue(tagged_value);
      } else {
        double double_value = static_cast<double>(static_cast<int32_t>(value));
        AddObjectDoubleValue(double_value);
      }
      return;
    }

    case Translation::UINT32_REGISTER: {
      int input_reg = iterator->Next();
      uintptr_t value = static_cast<uintptr_t>(input_->GetRegister(input_reg));
      bool is_smi = (value <= static_cast<uintptr_t>(Smi::kMaxValue));
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        PrintF(trace_scope_->file(),
               "%" V8PRIdPTR " ; uint %s (%s)\n", value,
               converter.NameOfCPURegister(input_reg),
               TraceValueType(is_smi));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        AddObjectTaggedValue(tagged_value);
      } else {
        double double_value = static_cast<double>(static_cast<uint32_t>(value));
        AddObjectDoubleValue(double_value);
      }
      return;
    }

    case Translation::DOUBLE_REGISTER: {
      int input_reg = iterator->Next();
      double value = input_->GetDoubleRegister(input_reg);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        PrintF(trace_scope_->file(),
               "%e ; %s\n", value,
               DoubleRegister::AllocationIndexToString(input_reg));
      }
      AddObjectDoubleValue(value);
      return;
    }

    case Translation::STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset = input_->GetOffsetFromSlotIndex(input_slot_index);
      intptr_t input_value = input_->GetFrameSlot(input_offset);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        PrintF(trace_scope_->file(),
               "0x%08" V8PRIxPTR " ; [sp + %d] ", input_value, input_offset);
        reinterpret_cast<Object*>(input_value)->ShortPrint(
            trace_scope_->file());
        PrintF(trace_scope_->file(),
               "\n");
      }
      AddObjectTaggedValue(input_value);
      return;
    }

    case Translation::INT32_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset = input_->GetOffsetFromSlotIndex(input_slot_index);
      intptr_t value = input_->GetFrameSlot(input_offset);
      bool is_smi = Smi::IsValid(value);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        PrintF(trace_scope_->file(),
               "%" V8PRIdPTR " ; [sp + %d] (%s)\n",
               value, input_offset, TraceValueType(is_smi));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        AddObjectTaggedValue(tagged_value);
      } else {
        double double_value = static_cast<double>(static_cast<int32_t>(value));
        AddObjectDoubleValue(double_value);
      }
      return;
    }

    case Translation::UINT32_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset = input_->GetOffsetFromSlotIndex(input_slot_index);
      uintptr_t value =
          static_cast<uintptr_t>(input_->GetFrameSlot(input_offset));
      bool is_smi = (value <= static_cast<uintptr_t>(Smi::kMaxValue));
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        PrintF(trace_scope_->file(),
               "%" V8PRIdPTR " ; [sp + %d] (uint %s)\n",
               value, input_offset, TraceValueType(is_smi));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        AddObjectTaggedValue(tagged_value);
      } else {
        double double_value = static_cast<double>(static_cast<uint32_t>(value));
        AddObjectDoubleValue(double_value);
      }
      return;
    }

    case Translation::DOUBLE_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset = input_->GetOffsetFromSlotIndex(input_slot_index);
      double value = input_->GetDoubleFrameSlot(input_offset);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        PrintF(trace_scope_->file(),
               "%e ; [sp + %d]\n", value, input_offset);
      }
      AddObjectDoubleValue(value);
      return;
    }

    case Translation::LITERAL: {
      Object* literal = ComputeLiteral(iterator->Next());
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      object @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        literal->ShortPrint(trace_scope_->file());
        PrintF(trace_scope_->file(),
               " ; literal\n");
      }
      intptr_t value = reinterpret_cast<intptr_t>(literal);
      AddObjectTaggedValue(value);
      return;
    }

    case Translation::DUPLICATED_OBJECT: {
      int object_index = iterator->Next();
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      nested @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        isolate_->heap()->arguments_marker()->ShortPrint(trace_scope_->file());
        PrintF(trace_scope_->file(),
               " ; duplicate of object #%d\n", object_index);
      }
      // Use the materialization marker value as a sentinel and fill in
      // the object after the deoptimized frame is built.
      intptr_t value = reinterpret_cast<intptr_t>(
          isolate_->heap()->arguments_marker());
      AddObjectDuplication(0, object_index);
      AddObjectTaggedValue(value);
      return;
    }

    case Translation::ARGUMENTS_OBJECT:
    case Translation::CAPTURED_OBJECT: {
      int length = iterator->Next();
      bool is_args = opcode == Translation::ARGUMENTS_OBJECT;
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "      nested @0x%08" V8PRIxPTR ": [field #%d] <- ",
               reinterpret_cast<intptr_t>(object_slot),
               field_index);
        isolate_->heap()->arguments_marker()->ShortPrint(trace_scope_->file());
        PrintF(trace_scope_->file(),
               " ; object (length = %d, is_args = %d)\n", length, is_args);
      }
      // Use the materialization marker value as a sentinel and fill in
      // the object after the deoptimized frame is built.
      intptr_t value = reinterpret_cast<intptr_t>(
          isolate_->heap()->arguments_marker());
      AddObjectStart(0, length, is_args);
      AddObjectTaggedValue(value);
      // We save the object values on the side and materialize the actual
      // object after the deoptimized frame is built.
      int object_index = deferred_objects_.length() - 1;
      for (int i = 0; i < length; i++) {
        DoTranslateObject(iterator, object_index, i);
      }
      return;
    }
  }

  FATAL("Unexpected translation opcode");
}


void Deoptimizer::DoTranslateCommand(TranslationIterator* iterator,
                                     int frame_index,
                                     unsigned output_offset) {
  disasm::NameConverter converter;
  // A GC-safe temporary placeholder that we can put in the output frame.
  const intptr_t kPlaceholder = reinterpret_cast<intptr_t>(Smi::FromInt(0));

  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());

  switch (opcode) {
    case Translation::BEGIN:
    case Translation::JS_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::GETTER_STUB_FRAME:
    case Translation::SETTER_STUB_FRAME:
    case Translation::COMPILED_STUB_FRAME:
      FATAL("Unexpected translation opcode");
      return;

    case Translation::REGISTER: {
      int input_reg = iterator->Next();
      intptr_t input_value = input_->GetRegister(input_reg);
      if (trace_scope_ != NULL) {
        PrintF(
            trace_scope_->file(),
            "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08" V8PRIxPTR " ; %s ",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            input_value,
            converter.NameOfCPURegister(input_reg));
        reinterpret_cast<Object*>(input_value)->ShortPrint(
            trace_scope_->file());
        PrintF(trace_scope_->file(), "\n");
      }
      output_[frame_index]->SetFrameSlot(output_offset, input_value);
      return;
    }

    case Translation::INT32_REGISTER: {
      int input_reg = iterator->Next();
      intptr_t value = input_->GetRegister(input_reg);
      bool is_smi = Smi::IsValid(value);
      if (trace_scope_ != NULL) {
        PrintF(
            trace_scope_->file(),
            "    0x%08" V8PRIxPTR ": [top + %d] <- %" V8PRIdPTR " ; %s (%s)\n",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            value,
            converter.NameOfCPURegister(input_reg),
            TraceValueType(is_smi));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, tagged_value);
      } else {
        // We save the untagged value on the side and store a GC-safe
        // temporary placeholder in the frame.
        AddDoubleValue(output_[frame_index]->GetTop() + output_offset,
                       static_cast<double>(static_cast<int32_t>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::UINT32_REGISTER: {
      int input_reg = iterator->Next();
      uintptr_t value = static_cast<uintptr_t>(input_->GetRegister(input_reg));
      bool is_smi = value <= static_cast<uintptr_t>(Smi::kMaxValue);
      if (trace_scope_ != NULL) {
        PrintF(
            trace_scope_->file(),
            "    0x%08" V8PRIxPTR ": [top + %d] <- %" V8PRIuPTR
            " ; uint %s (%s)\n",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            value,
            converter.NameOfCPURegister(input_reg),
            TraceValueType(is_smi));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, tagged_value);
      } else {
        // We save the untagged value on the side and store a GC-safe
        // temporary placeholder in the frame.
        AddDoubleValue(output_[frame_index]->GetTop() + output_offset,
                       static_cast<double>(static_cast<uint32_t>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::DOUBLE_REGISTER: {
      int input_reg = iterator->Next();
      double value = input_->GetDoubleRegister(input_reg);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "    0x%08" V8PRIxPTR ": [top + %d] <- %e ; %s\n",
               output_[frame_index]->GetTop() + output_offset,
               output_offset,
               value,
               DoubleRegister::AllocationIndexToString(input_reg));
      }
      // We save the untagged value on the side and store a GC-safe
      // temporary placeholder in the frame.
      AddDoubleValue(output_[frame_index]->GetTop() + output_offset, value);
      output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      return;
    }

    case Translation::STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset = input_->GetOffsetFromSlotIndex(input_slot_index);
      intptr_t input_value = input_->GetFrameSlot(input_offset);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF(trace_scope_->file(),
               "[top + %d] <- 0x%08" V8PRIxPTR " ; [sp + %d] ",
               output_offset,
               input_value,
               input_offset);
        reinterpret_cast<Object*>(input_value)->ShortPrint(
            trace_scope_->file());
        PrintF(trace_scope_->file(), "\n");
      }
      output_[frame_index]->SetFrameSlot(output_offset, input_value);
      return;
    }

    case Translation::INT32_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset = input_->GetOffsetFromSlotIndex(input_slot_index);
      intptr_t value = input_->GetFrameSlot(input_offset);
      bool is_smi = Smi::IsValid(value);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF(trace_scope_->file(),
               "[top + %d] <- %" V8PRIdPTR " ; [sp + %d] (%s)\n",
               output_offset,
               value,
               input_offset,
               TraceValueType(is_smi));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, tagged_value);
      } else {
        // We save the untagged value on the side and store a GC-safe
        // temporary placeholder in the frame.
        AddDoubleValue(output_[frame_index]->GetTop() + output_offset,
                       static_cast<double>(static_cast<int32_t>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::UINT32_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset = input_->GetOffsetFromSlotIndex(input_slot_index);
      uintptr_t value =
          static_cast<uintptr_t>(input_->GetFrameSlot(input_offset));
      bool is_smi = value <= static_cast<uintptr_t>(Smi::kMaxValue);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF(trace_scope_->file(),
               "[top + %d] <- %" V8PRIuPTR " ; [sp + %d] (uint32 %s)\n",
               output_offset,
               value,
               input_offset,
               TraceValueType(is_smi));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, tagged_value);
      } else {
        // We save the untagged value on the side and store a GC-safe
        // temporary placeholder in the frame.
        AddDoubleValue(output_[frame_index]->GetTop() + output_offset,
                       static_cast<double>(static_cast<uint32_t>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::DOUBLE_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset = input_->GetOffsetFromSlotIndex(input_slot_index);
      double value = input_->GetDoubleFrameSlot(input_offset);
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "    0x%08" V8PRIxPTR ": [top + %d] <- %e ; [sp + %d]\n",
               output_[frame_index]->GetTop() + output_offset,
               output_offset,
               value,
               input_offset);
      }
      // We save the untagged value on the side and store a GC-safe
      // temporary placeholder in the frame.
      AddDoubleValue(output_[frame_index]->GetTop() + output_offset, value);
      output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      return;
    }

    case Translation::LITERAL: {
      Object* literal = ComputeLiteral(iterator->Next());
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "    0x%08" V8PRIxPTR ": [top + %d] <- ",
               output_[frame_index]->GetTop() + output_offset,
               output_offset);
        literal->ShortPrint(trace_scope_->file());
        PrintF(trace_scope_->file(), " ; literal\n");
      }
      intptr_t value = reinterpret_cast<intptr_t>(literal);
      output_[frame_index]->SetFrameSlot(output_offset, value);
      return;
    }

    case Translation::DUPLICATED_OBJECT: {
      int object_index = iterator->Next();
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "    0x%08" V8PRIxPTR ": [top + %d] <- ",
               output_[frame_index]->GetTop() + output_offset,
               output_offset);
        isolate_->heap()->arguments_marker()->ShortPrint(trace_scope_->file());
        PrintF(trace_scope_->file(),
               " ; duplicate of object #%d\n", object_index);
      }
      // Use the materialization marker value as a sentinel and fill in
      // the object after the deoptimized frame is built.
      intptr_t value = reinterpret_cast<intptr_t>(
          isolate_->heap()->arguments_marker());
      AddObjectDuplication(output_[frame_index]->GetTop() + output_offset,
                           object_index);
      output_[frame_index]->SetFrameSlot(output_offset, value);
      return;
    }

    case Translation::ARGUMENTS_OBJECT:
    case Translation::CAPTURED_OBJECT: {
      int length = iterator->Next();
      bool is_args = opcode == Translation::ARGUMENTS_OBJECT;
      if (trace_scope_ != NULL) {
        PrintF(trace_scope_->file(),
               "    0x%08" V8PRIxPTR ": [top + %d] <- ",
               output_[frame_index]->GetTop() + output_offset,
               output_offset);
        isolate_->heap()->arguments_marker()->ShortPrint(trace_scope_->file());
        PrintF(trace_scope_->file(),
               " ; object (length = %d, is_args = %d)\n", length, is_args);
      }
      // Use the materialization marker value as a sentinel and fill in
      // the object after the deoptimized frame is built.
      intptr_t value = reinterpret_cast<intptr_t>(
          isolate_->heap()->arguments_marker());
      AddObjectStart(output_[frame_index]->GetTop() + output_offset,
                     length, is_args);
      output_[frame_index]->SetFrameSlot(output_offset, value);
      // We save the object values on the side and materialize the actual
      // object after the deoptimized frame is built.
      int object_index = deferred_objects_.length() - 1;
      for (int i = 0; i < length; i++) {
        DoTranslateObject(iterator, object_index, i);
      }
      return;
    }
  }
}


unsigned Deoptimizer::ComputeInputFrameSize() const {
  unsigned fixed_size = ComputeFixedSize(function_);
  // The fp-to-sp delta already takes the context, constant pool pointer and the
  // function into account so we have to avoid double counting them.
  unsigned result = fixed_size + fp_to_sp_delta_ -
      StandardFrameConstants::kFixedFrameSizeFromFp;
  if (compiled_code_->kind() == Code::OPTIMIZED_FUNCTION) {
    unsigned stack_slots = compiled_code_->stack_slots();
    unsigned outgoing_size = ComputeOutgoingArgumentSize();
    CHECK(result == fixed_size + (stack_slots * kPointerSize) + outgoing_size);
  }
  return result;
}


unsigned Deoptimizer::ComputeFixedSize(JSFunction* function) const {
  // The fixed part of the frame consists of the return address, frame
  // pointer, function, context, and all the incoming arguments.
  return ComputeIncomingArgumentSize(function) +
      StandardFrameConstants::kFixedFrameSize;
}


unsigned Deoptimizer::ComputeIncomingArgumentSize(JSFunction* function) const {
  // The incoming arguments is the values for formal parameters and
  // the receiver. Every slot contains a pointer.
  if (function->IsSmi()) {
    CHECK_EQ(Smi::cast(function), Smi::FromInt(StackFrame::STUB));
    return 0;
  }
  unsigned arguments = function->shared()->formal_parameter_count() + 1;
  return arguments * kPointerSize;
}


unsigned Deoptimizer::ComputeOutgoingArgumentSize() const {
  DeoptimizationInputData* data = DeoptimizationInputData::cast(
      compiled_code_->deoptimization_data());
  unsigned height = data->ArgumentsStackHeight(bailout_id_)->value();
  return height * kPointerSize;
}


Object* Deoptimizer::ComputeLiteral(int index) const {
  DeoptimizationInputData* data = DeoptimizationInputData::cast(
      compiled_code_->deoptimization_data());
  FixedArray* literals = data->LiteralArray();
  return literals->get(index);
}


void Deoptimizer::AddObjectStart(intptr_t slot, int length, bool is_args) {
  ObjectMaterializationDescriptor object_desc(
      reinterpret_cast<Address>(slot), jsframe_count_, length, -1, is_args);
  deferred_objects_.Add(object_desc);
}


void Deoptimizer::AddObjectDuplication(intptr_t slot, int object_index) {
  ObjectMaterializationDescriptor object_desc(
      reinterpret_cast<Address>(slot), jsframe_count_, -1, object_index, false);
  deferred_objects_.Add(object_desc);
}


void Deoptimizer::AddObjectTaggedValue(intptr_t value) {
  deferred_objects_tagged_values_.Add(reinterpret_cast<Object*>(value));
}


void Deoptimizer::AddObjectDoubleValue(double value) {
  deferred_objects_tagged_values_.Add(isolate()->heap()->the_hole_value());
  HeapNumberMaterializationDescriptor<int> value_desc(
      deferred_objects_tagged_values_.length() - 1, value);
  deferred_objects_double_values_.Add(value_desc);
}


void Deoptimizer::AddDoubleValue(intptr_t slot_address, double value) {
  HeapNumberMaterializationDescriptor<Address> value_desc(
      reinterpret_cast<Address>(slot_address), value);
  deferred_heap_numbers_.Add(value_desc);
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

  MacroAssembler masm(isolate, NULL, 16 * KB);
  masm.set_emit_debug_code(false);
  GenerateDeoptimizationEntries(&masm, entry_count, type);
  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  MemoryChunk* chunk = data->deopt_entry_code_[type];
  CHECK(static_cast<int>(Deoptimizer::GetMaxDeoptTableSize()) >=
        desc.instr_size);
  chunk->CommitArea(desc.instr_size);
  CopyBytes(chunk->area_start(), desc.buffer,
      static_cast<size_t>(desc.instr_size));
  CPU::FlushICache(chunk->area_start(), desc.instr_size);

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
  return StandardFrameConstants::kFixedFrameSize +
      (ComputeParametersCount() + 1) * kPointerSize;
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
      return function_->shared()->formal_parameter_count();
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
  ASSERT_EQ(StackFrame::JAVA_SCRIPT, type_);
  unsigned offset = GetOffsetFromSlotIndex(index);
  return reinterpret_cast<Object*>(*GetFrameSlotPointer(offset));
}


void TranslationBuffer::Add(int32_t value, Zone* zone) {
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
    ASSERT(HasNext());
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
  OS::MemCopy(
      result->GetDataStartAddress(), contents_.ToVector().start(), length);
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


void Translation::BeginCompiledStubFrame() {
  buffer_->Add(COMPILED_STUB_FRAME, zone());
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


void Translation::StoreDoubleRegister(DoubleRegister reg) {
  buffer_->Add(DOUBLE_REGISTER, zone());
  buffer_->Add(DoubleRegister::ToAllocationIndex(reg), zone());
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


int Translation::NumberOfOperandsFor(Opcode opcode) {
  switch (opcode) {
    case GETTER_STUB_FRAME:
    case SETTER_STUB_FRAME:
    case DUPLICATED_OBJECT:
    case ARGUMENTS_OBJECT:
    case CAPTURED_OBJECT:
    case REGISTER:
    case INT32_REGISTER:
    case UINT32_REGISTER:
    case DOUBLE_REGISTER:
    case STACK_SLOT:
    case INT32_STACK_SLOT:
    case UINT32_STACK_SLOT:
    case DOUBLE_STACK_SLOT:
    case LITERAL:
    case COMPILED_STUB_FRAME:
      return 1;
    case BEGIN:
    case ARGUMENTS_ADAPTOR_FRAME:
    case CONSTRUCT_STUB_FRAME:
      return 2;
    case JS_FRAME:
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


// We can't intermix stack decoding and allocations because
// deoptimization infrastracture is not GC safe.
// Thus we build a temporary structure in malloced space.
SlotRef SlotRefValueBuilder::ComputeSlotForNextArgument(
    Translation::Opcode opcode,
    TranslationIterator* iterator,
    DeoptimizationInputData* data,
    JavaScriptFrame* frame) {
  switch (opcode) {
    case Translation::BEGIN:
    case Translation::JS_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::GETTER_STUB_FRAME:
    case Translation::SETTER_STUB_FRAME:
      // Peeled off before getting here.
      break;

    case Translation::DUPLICATED_OBJECT: {
      return SlotRef::NewDuplicateObject(iterator->Next());
    }

    case Translation::ARGUMENTS_OBJECT:
      return SlotRef::NewArgumentsObject(iterator->Next());

    case Translation::CAPTURED_OBJECT: {
      return SlotRef::NewDeferredObject(iterator->Next());
    }

    case Translation::REGISTER:
    case Translation::INT32_REGISTER:
    case Translation::UINT32_REGISTER:
    case Translation::DOUBLE_REGISTER:
      // We are at safepoint which corresponds to call.  All registers are
      // saved by caller so there would be no live registers at this
      // point. Thus these translation commands should not be used.
      break;

    case Translation::STACK_SLOT: {
      int slot_index = iterator->Next();
      Address slot_addr = SlotAddress(frame, slot_index);
      return SlotRef(slot_addr, SlotRef::TAGGED);
    }

    case Translation::INT32_STACK_SLOT: {
      int slot_index = iterator->Next();
      Address slot_addr = SlotAddress(frame, slot_index);
      return SlotRef(slot_addr, SlotRef::INT32);
    }

    case Translation::UINT32_STACK_SLOT: {
      int slot_index = iterator->Next();
      Address slot_addr = SlotAddress(frame, slot_index);
      return SlotRef(slot_addr, SlotRef::UINT32);
    }

    case Translation::DOUBLE_STACK_SLOT: {
      int slot_index = iterator->Next();
      Address slot_addr = SlotAddress(frame, slot_index);
      return SlotRef(slot_addr, SlotRef::DOUBLE);
    }

    case Translation::LITERAL: {
      int literal_index = iterator->Next();
      return SlotRef(data->GetIsolate(),
                     data->LiteralArray()->get(literal_index));
    }

    case Translation::COMPILED_STUB_FRAME:
      UNREACHABLE();
      break;
  }

  FATAL("We should never get here - unexpected deopt info.");
  return SlotRef();
}


SlotRefValueBuilder::SlotRefValueBuilder(JavaScriptFrame* frame,
                                         int inlined_jsframe_index,
                                         int formal_parameter_count)
    : current_slot_(0), args_length_(-1), first_slot_index_(-1) {
  DisallowHeapAllocation no_gc;

  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationInputData* data =
      static_cast<OptimizedFrame*>(frame)->GetDeoptimizationData(&deopt_index);
  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  CHECK_EQ(opcode, Translation::BEGIN);
  it.Next();  // Drop frame count.

  stack_frame_id_ = frame->fp();

  int jsframe_count = it.Next();
  CHECK_GT(jsframe_count, inlined_jsframe_index);
  int jsframes_to_skip = inlined_jsframe_index;
  int number_of_slots = -1;  // Number of slots inside our frame (yet unknown)
  bool should_deopt = false;
  while (number_of_slots != 0) {
    opcode = static_cast<Translation::Opcode>(it.Next());
    bool processed = false;
    if (opcode == Translation::ARGUMENTS_ADAPTOR_FRAME) {
      if (jsframes_to_skip == 0) {
        CHECK_EQ(Translation::NumberOfOperandsFor(opcode), 2);

        it.Skip(1);  // literal id
        int height = it.Next();

        // Skip the translation command for the receiver.
        it.Skip(Translation::NumberOfOperandsFor(
            static_cast<Translation::Opcode>(it.Next())));

        // We reached the arguments adaptor frame corresponding to the
        // inlined function in question.  Number of arguments is height - 1.
        first_slot_index_ = slot_refs_.length();
        args_length_ = height - 1;
        number_of_slots = height - 1;
        processed = true;
      }
    } else if (opcode == Translation::JS_FRAME) {
      if (jsframes_to_skip == 0) {
        // Skip over operands to advance to the next opcode.
        it.Skip(Translation::NumberOfOperandsFor(opcode));

        // Skip the translation command for the receiver.
        it.Skip(Translation::NumberOfOperandsFor(
            static_cast<Translation::Opcode>(it.Next())));

        // We reached the frame corresponding to the inlined function
        // in question.  Process the translation commands for the
        // arguments.  Number of arguments is equal to the number of
        // format parameter count.
        first_slot_index_ = slot_refs_.length();
        args_length_ = formal_parameter_count;
        number_of_slots = formal_parameter_count;
        processed = true;
      }
      jsframes_to_skip--;
    } else if (opcode != Translation::BEGIN &&
               opcode != Translation::CONSTRUCT_STUB_FRAME &&
               opcode != Translation::GETTER_STUB_FRAME &&
               opcode != Translation::SETTER_STUB_FRAME &&
               opcode != Translation::COMPILED_STUB_FRAME) {
      slot_refs_.Add(ComputeSlotForNextArgument(opcode, &it, data, frame));

      if (first_slot_index_ >= 0) {
        // We have found the beginning of our frame -> make sure we count
        // the nested slots of captured objects
        number_of_slots--;
        SlotRef& slot = slot_refs_.last();
        CHECK_NE(slot.Representation(), SlotRef::ARGUMENTS_OBJECT);
        number_of_slots += slot.GetChildrenCount();
        if (slot.Representation() == SlotRef::DEFERRED_OBJECT ||
            slot.Representation() == SlotRef::DUPLICATE_OBJECT) {
          should_deopt = true;
        }
      }

      processed = true;
    }
    if (!processed) {
      // Skip over operands to advance to the next opcode.
      it.Skip(Translation::NumberOfOperandsFor(opcode));
    }
  }
  if (should_deopt) {
    List<JSFunction*> functions(2);
    frame->GetFunctions(&functions);
    Deoptimizer::DeoptimizeFunction(functions[0]);
  }
}


Handle<Object> SlotRef::GetValue(Isolate* isolate) {
  switch (representation_) {
    case TAGGED:
      return Handle<Object>(Memory::Object_at(addr_), isolate);

    case INT32: {
      int value = Memory::int32_at(addr_);
      if (Smi::IsValid(value)) {
        return Handle<Object>(Smi::FromInt(value), isolate);
      } else {
        return isolate->factory()->NewNumberFromInt(value);
      }
    }

    case UINT32: {
      uint32_t value = Memory::uint32_at(addr_);
      if (value <= static_cast<uint32_t>(Smi::kMaxValue)) {
        return Handle<Object>(Smi::FromInt(static_cast<int>(value)), isolate);
      } else {
        return isolate->factory()->NewNumber(static_cast<double>(value));
      }
    }

    case DOUBLE: {
      double value = read_double_value(addr_);
      return isolate->factory()->NewNumber(value);
    }

    case LITERAL:
      return literal_;

    default:
      FATAL("We should never get here - unexpected deopt info.");
      return Handle<Object>::null();
  }
}


void SlotRefValueBuilder::Prepare(Isolate* isolate) {
  MaterializedObjectStore* materialized_store =
      isolate->materialized_object_store();
  previously_materialized_objects_ = materialized_store->Get(stack_frame_id_);
  prev_materialized_count_ = previously_materialized_objects_.is_null()
      ? 0 : previously_materialized_objects_->length();

  // Skip any materialized objects of the inlined "parent" frames.
  // (Note that we still need to materialize them because they might be
  // referred to as duplicated objects.)
  while (current_slot_ < first_slot_index_) {
    GetNext(isolate, 0);
  }
  CHECK_EQ(current_slot_, first_slot_index_);
}


Handle<Object> SlotRefValueBuilder::GetPreviouslyMaterialized(
    Isolate* isolate, int length) {
  int object_index = materialized_objects_.length();
  Handle<Object> return_value = Handle<Object>(
      previously_materialized_objects_->get(object_index), isolate);
  materialized_objects_.Add(return_value);

  // Now need to skip all the nested objects (and possibly read them from
  // the materialization store, too).
  for (int i = 0; i < length; i++) {
    SlotRef& slot = slot_refs_[current_slot_];
    current_slot_++;

    // We need to read all the nested objects - add them to the
    // number of objects we need to process.
    length += slot.GetChildrenCount();

    // Put the nested deferred/duplicate objects into our materialization
    // array.
    if (slot.Representation() == SlotRef::DEFERRED_OBJECT ||
        slot.Representation() == SlotRef::DUPLICATE_OBJECT) {
      int nested_object_index = materialized_objects_.length();
      Handle<Object> nested_object = Handle<Object>(
          previously_materialized_objects_->get(nested_object_index),
          isolate);
      materialized_objects_.Add(nested_object);
    }
  }

  return return_value;
}


Handle<Object> SlotRefValueBuilder::GetNext(Isolate* isolate, int lvl) {
  SlotRef& slot = slot_refs_[current_slot_];
  current_slot_++;
  switch (slot.Representation()) {
    case SlotRef::TAGGED:
    case SlotRef::INT32:
    case SlotRef::UINT32:
    case SlotRef::DOUBLE:
    case SlotRef::LITERAL: {
      return slot.GetValue(isolate);
    }
    case SlotRef::ARGUMENTS_OBJECT: {
      // We should never need to materialize an arguments object,
      // but we still need to put something into the array
      // so that the indexing is consistent.
      materialized_objects_.Add(isolate->factory()->undefined_value());
      int length = slot.GetChildrenCount();
      for (int i = 0; i < length; ++i) {
        // We don't need the argument, just ignore it
        GetNext(isolate, lvl + 1);
      }
      return isolate->factory()->undefined_value();
    }
    case SlotRef::DEFERRED_OBJECT: {
      int length = slot.GetChildrenCount();
      CHECK(slot_refs_[current_slot_].Representation() == SlotRef::LITERAL ||
            slot_refs_[current_slot_].Representation() == SlotRef::TAGGED);

      int object_index = materialized_objects_.length();
      if (object_index <  prev_materialized_count_) {
        return GetPreviouslyMaterialized(isolate, length);
      }

      Handle<Object> map_object = slot_refs_[current_slot_].GetValue(isolate);
      Handle<Map> map = Map::GeneralizeAllFieldRepresentations(
          Handle<Map>::cast(map_object));
      current_slot_++;
      // TODO(jarin) this should be unified with the code in
      // Deoptimizer::MaterializeNextHeapObject()
      switch (map->instance_type()) {
        case HEAP_NUMBER_TYPE: {
          // Reuse the HeapNumber value directly as it is already properly
          // tagged and skip materializing the HeapNumber explicitly.
          Handle<Object> object = GetNext(isolate, lvl + 1);
          materialized_objects_.Add(object);
          // On 32-bit architectures, there is an extra slot there because
          // the escape analysis calculates the number of slots as
          // object-size/pointer-size. To account for this, we read out
          // any extra slots.
          for (int i = 0; i < length - 2; i++) {
            GetNext(isolate, lvl + 1);
          }
          return object;
        }
        case JS_OBJECT_TYPE: {
          Handle<JSObject> object =
              isolate->factory()->NewJSObjectFromMap(map, NOT_TENURED, false);
          materialized_objects_.Add(object);
          Handle<Object> properties = GetNext(isolate, lvl + 1);
          Handle<Object> elements = GetNext(isolate, lvl + 1);
          object->set_properties(FixedArray::cast(*properties));
          object->set_elements(FixedArrayBase::cast(*elements));
          for (int i = 0; i < length - 3; ++i) {
            Handle<Object> value = GetNext(isolate, lvl + 1);
            object->FastPropertyAtPut(i, *value);
          }
          return object;
        }
        case JS_ARRAY_TYPE: {
          Handle<JSArray> object =
              isolate->factory()->NewJSArray(0, map->elements_kind());
          materialized_objects_.Add(object);
          Handle<Object> properties = GetNext(isolate, lvl + 1);
          Handle<Object> elements = GetNext(isolate, lvl + 1);
          Handle<Object> length = GetNext(isolate, lvl + 1);
          object->set_properties(FixedArray::cast(*properties));
          object->set_elements(FixedArrayBase::cast(*elements));
          object->set_length(*length);
          return object;
        }
        default:
          PrintF(stderr,
                 "[couldn't handle instance type %d]\n", map->instance_type());
          UNREACHABLE();
          break;
      }
      UNREACHABLE();
      break;
    }

    case SlotRef::DUPLICATE_OBJECT: {
      int object_index = slot.DuplicateObjectId();
      Handle<Object> object = materialized_objects_[object_index];
      materialized_objects_.Add(object);
      return object;
    }
    default:
      UNREACHABLE();
      break;
  }

  FATAL("We should never get here - unexpected deopt slot kind.");
  return Handle<Object>::null();
}


void SlotRefValueBuilder::Finish(Isolate* isolate) {
  // We should have processed all the slots
  CHECK_EQ(slot_refs_.length(), current_slot_);

  if (materialized_objects_.length() > prev_materialized_count_) {
    // We have materialized some new objects, so we have to store them
    // to prevent duplicate materialization
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(
        materialized_objects_.length());
    for (int i = 0; i < materialized_objects_.length(); i++) {
      array->set(i, *(materialized_objects_.at(i)));
    }
    isolate->materialized_object_store()->Set(stack_frame_id_, array);
  }
}


Handle<FixedArray> MaterializedObjectStore::Get(Address fp) {
  int index = StackIdToIndex(fp);
  if (index == -1) {
    return Handle<FixedArray>::null();
  }
  Handle<FixedArray> array = GetStackEntries();
  CHECK_GT(array->length(), index);
  return Handle<FixedArray>::cast(Handle<Object>(array->get(index),
                                                 isolate()));
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


void MaterializedObjectStore::Remove(Address fp) {
  int index = StackIdToIndex(fp);
  CHECK_GE(index, 0);

  frame_fps_.Remove(index);
  Handle<FixedArray> array = GetStackEntries();
  CHECK_LT(index, array->length());
  for (int i = index; i < frame_fps_.length(); i++) {
    array->set(i, array->get(i + 1));
  }
  array->set(frame_fps_.length(), isolate()->heap()->undefined_value());
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
  isolate()->heap()->public_set_materialized_objects(*new_array);
  return new_array;
}


DeoptimizedFrameInfo::DeoptimizedFrameInfo(Deoptimizer* deoptimizer,
                                           int frame_index,
                                           bool has_arguments_adaptor,
                                           bool has_construct_stub) {
  FrameDescription* output_frame = deoptimizer->output_[frame_index];
  function_ = output_frame->GetFunction();
  has_construct_stub_ = has_construct_stub;
  expression_count_ = output_frame->GetExpressionCount();
  expression_stack_ = new Object*[expression_count_];
  // Get the source position using the unoptimized code.
  Address pc = reinterpret_cast<Address>(output_frame->GetPc());
  Code* code = Code::cast(deoptimizer->isolate()->FindCodeObject(pc));
  source_position_ = code->SourcePosition(pc);

  for (int i = 0; i < expression_count_; i++) {
    SetExpression(i, output_frame->GetExpression(i));
  }

  if (has_arguments_adaptor) {
    output_frame = deoptimizer->output_[frame_index - 1];
    CHECK_EQ(output_frame->GetFrameType(), StackFrame::ARGUMENTS_ADAPTOR);
  }

  parameters_count_ = output_frame->ComputeParametersCount();
  parameters_ = new Object*[parameters_count_];
  for (int i = 0; i < parameters_count_; i++) {
    SetParameter(i, output_frame->GetParameter(i));
  }
}


DeoptimizedFrameInfo::~DeoptimizedFrameInfo() {
  delete[] expression_stack_;
  delete[] parameters_;
}


void DeoptimizedFrameInfo::Iterate(ObjectVisitor* v) {
  v->VisitPointer(BitCast<Object**>(&function_));
  v->VisitPointers(parameters_, parameters_ + parameters_count_);
  v->VisitPointers(expression_stack_, expression_stack_ + expression_count_);
}

} }  // namespace v8::internal
