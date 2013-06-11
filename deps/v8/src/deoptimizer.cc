// Copyright 2013 the V8 project authors. All rights reserved.
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
                                  EXECUTABLE,
                                  NULL);
}


DeoptimizerData::DeoptimizerData(MemoryAllocator* allocator)
    : allocator_(allocator),
      current_(NULL),
#ifdef ENABLE_DEBUGGER_SUPPORT
      deoptimized_frame_info_(NULL),
#endif
      deoptimizing_code_list_(NULL) {
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

  DeoptimizingCodeListNode* current = deoptimizing_code_list_;
  while (current != NULL) {
    DeoptimizingCodeListNode* prev = current;
    current = current->next();
    delete prev;
  }
  deoptimizing_code_list_ = NULL;
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void DeoptimizerData::Iterate(ObjectVisitor* v) {
  if (deoptimized_frame_info_ != NULL) {
    deoptimized_frame_info_->Iterate(v);
  }
}
#endif


Code* DeoptimizerData::FindDeoptimizingCode(Address addr) {
  for (DeoptimizingCodeListNode* node = deoptimizing_code_list_;
       node != NULL;
       node = node->next()) {
    if (node->code()->contains(addr)) return *node->code();
  }
  return NULL;
}


void DeoptimizerData::RemoveDeoptimizingCode(Code* code) {
  for (DeoptimizingCodeListNode *prev = NULL, *cur = deoptimizing_code_list_;
       cur != NULL;
       prev = cur, cur = cur->next()) {
    if (*cur->code() == code) {
      if (prev == NULL) {
        deoptimizing_code_list_ = cur->next();
      } else {
        prev->set_next(cur->next());
      }
      delete cur;
      return;
    }
  }
  // Deoptimizing code is removed through weak callback. Each object is expected
  // to be removed once and only once.
  UNREACHABLE();
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
  ASSERT(isolate->deoptimizer_data()->current_ == NULL);
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
  ASSERT(result != NULL);
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


#ifdef ENABLE_DEBUGGER_SUPPORT
DeoptimizedFrameInfo* Deoptimizer::DebuggerInspectableFrame(
    JavaScriptFrame* frame,
    int jsframe_index,
    Isolate* isolate) {
  ASSERT(frame->is_optimized());
  ASSERT(isolate->deoptimizer_data()->deoptimized_frame_info_ == NULL);

  // Get the function and code from the frame.
  JSFunction* function = JSFunction::cast(frame->function());
  Code* code = frame->LookupCode();

  // Locate the deoptimization point in the code. As we are at a call the
  // return address must be at a place in the code with deoptimization support.
  SafepointEntry safepoint_entry = code->GetSafepointEntry(frame->pc());
  int deoptimization_index = safepoint_entry.deoptimization_index();
  ASSERT(deoptimization_index != Safepoint::kNoDeoptimizationIndex);

  // Always use the actual stack slots when calculating the fp to sp
  // delta adding two for the function and context.
  unsigned stack_slots = code->stack_slots();
  unsigned fp_to_sp_delta = ((stack_slots + 2) * kPointerSize);

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
  ASSERT_LT(jsframe_index, deoptimizer->jsframe_count());

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
  ASSERT(isolate->deoptimizer_data()->deoptimized_frame_info_ == info);
  delete info;
  isolate->deoptimizer_data()->deoptimized_frame_info_ = NULL;
}
#endif

void Deoptimizer::GenerateDeoptimizationEntries(MacroAssembler* masm,
                                                int count,
                                                BailoutType type) {
  TableEntryGenerator generator(masm, type, count);
  generator.Generate();
}


void Deoptimizer::VisitAllOptimizedFunctionsForContext(
    Context* context, OptimizedFunctionVisitor* visitor) {
  Isolate* isolate = context->GetIsolate();
  ZoneScope zone_scope(isolate->runtime_zone(), DELETE_ON_EXIT);
  DisallowHeapAllocation no_allocation;

  ASSERT(context->IsNativeContext());

  visitor->EnterContext(context);

  // Create a snapshot of the optimized functions list. This is needed because
  // visitors might remove more than one link from the list at once.
  ZoneList<JSFunction*> snapshot(1, isolate->runtime_zone());
  Object* element = context->OptimizedFunctionsListHead();
  while (!element->IsUndefined()) {
    JSFunction* element_function = JSFunction::cast(element);
    snapshot.Add(element_function, isolate->runtime_zone());
    element = element_function->next_function_link();
  }

  // Run through the snapshot of optimized functions and visit them.
  for (int i = 0; i < snapshot.length(); ++i) {
    visitor->VisitFunction(snapshot.at(i));
  }

  visitor->LeaveContext(context);
}


void Deoptimizer::VisitAllOptimizedFunctions(
    Isolate* isolate,
    OptimizedFunctionVisitor* visitor) {
  DisallowHeapAllocation no_allocation;

  // Run through the list of all native contexts and deoptimize.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined()) {
    VisitAllOptimizedFunctionsForContext(Context::cast(context), visitor);
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


// Removes the functions selected by the given filter from the optimized
// function list of the given context and partitions the removed functions
// into one or more lists such that all functions in a list share the same
// code. The head of each list is written in the deoptimizing_functions field
// of the corresponding code object.
// The found code objects are returned in the given zone list.
static void PartitionOptimizedFunctions(Context* context,
                                        OptimizedFunctionFilter* filter,
                                        ZoneList<Code*>* partitions,
                                        Zone* zone,
                                        Object* undefined) {
  DisallowHeapAllocation no_allocation;
  Object* current = context->get(Context::OPTIMIZED_FUNCTIONS_LIST);
  Object* remainder_head = undefined;
  Object* remainder_tail = undefined;
  ASSERT_EQ(0, partitions->length());
  while (current != undefined) {
    JSFunction* function = JSFunction::cast(current);
    current = function->next_function_link();
    if (filter->TakeFunction(function)) {
      Code* code = function->code();
      if (code->deoptimizing_functions() == undefined) {
        partitions->Add(code, zone);
      } else {
        ASSERT(partitions->Contains(code));
      }
      function->set_next_function_link(code->deoptimizing_functions());
      code->set_deoptimizing_functions(function);
    } else {
      if (remainder_head == undefined) {
        remainder_head = function;
      } else {
        JSFunction::cast(remainder_tail)->set_next_function_link(function);
      }
      remainder_tail = function;
    }
  }
  if (remainder_tail != undefined) {
    JSFunction::cast(remainder_tail)->set_next_function_link(undefined);
  }
  context->set(Context::OPTIMIZED_FUNCTIONS_LIST, remainder_head);
}


class DeoptimizeAllFilter : public OptimizedFunctionFilter {
 public:
  virtual bool TakeFunction(JSFunction* function) {
    return true;
  }
};


class DeoptimizeWithMatchingCodeFilter : public OptimizedFunctionFilter {
 public:
  explicit DeoptimizeWithMatchingCodeFilter(Code* code) : code_(code) {}
  virtual bool TakeFunction(JSFunction* function) {
    return function->code() == code_;
  }
 private:
  Code* code_;
};


void Deoptimizer::DeoptimizeAll(Isolate* isolate) {
  DisallowHeapAllocation no_allocation;

  if (FLAG_trace_deopt) {
    PrintF("[deoptimize all contexts]\n");
  }

  DeoptimizeAllFilter filter;
  DeoptimizeAllFunctionsWith(isolate, &filter);
}


void Deoptimizer::DeoptimizeGlobalObject(JSObject* object) {
  DisallowHeapAllocation no_allocation;
  DeoptimizeAllFilter filter;
  if (object->IsJSGlobalProxy()) {
    Object* proto = object->GetPrototype();
    ASSERT(proto->IsJSGlobalObject());
    DeoptimizeAllFunctionsForContext(
        GlobalObject::cast(proto)->native_context(), &filter);
  } else if (object->IsGlobalObject()) {
    DeoptimizeAllFunctionsForContext(
        GlobalObject::cast(object)->native_context(), &filter);
  }
}


void Deoptimizer::DeoptimizeFunction(JSFunction* function) {
  if (!function->IsOptimized()) return;
  Code* code = function->code();
  Context* context = function->context()->native_context();
  Isolate* isolate = context->GetIsolate();
  Object* undefined = isolate->heap()->undefined_value();
  Zone* zone = isolate->runtime_zone();
  ZoneScope zone_scope(zone, DELETE_ON_EXIT);
  ZoneList<Code*> codes(1, zone);
  DeoptimizeWithMatchingCodeFilter filter(code);
  PartitionOptimizedFunctions(context, &filter, &codes, zone, undefined);
  ASSERT_EQ(1, codes.length());
  DeoptimizeFunctionWithPreparedFunctionList(
      JSFunction::cast(codes.at(0)->deoptimizing_functions()));
  codes.at(0)->set_deoptimizing_functions(undefined);
}


void Deoptimizer::DeoptimizeAllFunctionsForContext(
    Context* context, OptimizedFunctionFilter* filter) {
  ASSERT(context->IsNativeContext());
  Isolate* isolate = context->GetIsolate();
  Object* undefined = isolate->heap()->undefined_value();
  Zone* zone = isolate->runtime_zone();
  ZoneScope zone_scope(zone, DELETE_ON_EXIT);
  ZoneList<Code*> codes(1, zone);
  PartitionOptimizedFunctions(context, filter, &codes, zone, undefined);
  for (int i = 0; i < codes.length(); ++i) {
    DeoptimizeFunctionWithPreparedFunctionList(
        JSFunction::cast(codes.at(i)->deoptimizing_functions()));
    codes.at(i)->set_deoptimizing_functions(undefined);
  }
}


void Deoptimizer::DeoptimizeAllFunctionsWith(Isolate* isolate,
                                             OptimizedFunctionFilter* filter) {
  DisallowHeapAllocation no_allocation;

  // Run through the list of all native contexts and deoptimize.
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined()) {
    DeoptimizeAllFunctionsForContext(Context::cast(context), filter);
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


void Deoptimizer::HandleWeakDeoptimizedCode(v8::Isolate* isolate,
                                            v8::Persistent<v8::Value>* obj,
                                            void* parameter) {
  DeoptimizingCodeListNode* node =
      reinterpret_cast<DeoptimizingCodeListNode*>(parameter);
  DeoptimizerData* data =
      reinterpret_cast<Isolate*>(isolate)->deoptimizer_data();
  data->RemoveDeoptimizingCode(*node->code());
#ifdef DEBUG
  for (DeoptimizingCodeListNode* current = data->deoptimizing_code_list_;
       current != NULL;
       current = current->next()) {
    ASSERT(current != node);
  }
#endif
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
    case OSR:
      return FLAG_trace_osr;
  }
  UNREACHABLE();
  return false;
}


const char* Deoptimizer::MessageFor(BailoutType type) {
  switch (type) {
    case EAGER: return "eager";
    case SOFT: return "soft";
    case LAZY: return "lazy";
    case DEBUGGER: return "debugger";
    case OSR: return "OSR";
  }
  UNREACHABLE();
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
      deferred_arguments_objects_values_(0),
      deferred_arguments_objects_(0),
      deferred_heap_numbers_(0),
      trace_(false) {
  // For COMPILED_STUBs called from builtins, the function pointer is a SMI
  // indicating an internal frame.
  if (function->IsSmi()) {
    function = NULL;
  }
  if (function != NULL && function->IsOptimized()) {
    function->shared()->increment_deopt_count();
    if (bailout_type_ == Deoptimizer::SOFT) {
      // Soft deopts shouldn't count against the overall re-optimization count
      // that can eventually lead to disabling optimization for a function.
      int opt_count = function->shared()->opt_count();
      if (opt_count > 0) opt_count--;
      function->shared()->set_opt_count(opt_count);
    }
  }
  compiled_code_ = FindOptimizedCode(function, optimized_code);
  StackFrame::Type frame_type = function == NULL
      ? StackFrame::STUB
      : StackFrame::JAVA_SCRIPT;
  trace_ = TraceEnabledFor(type, frame_type);
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
      ASSERT(from_ == NULL);
      return function->code();
    case Deoptimizer::LAZY: {
      Code* compiled_code =
          isolate_->deoptimizer_data()->FindDeoptimizingCode(from_);
      return (compiled_code == NULL)
          ? static_cast<Code*>(isolate_->heap()->FindCodeObject(from_))
          : compiled_code;
    }
    case Deoptimizer::OSR: {
      // The function has already been optimized and we're transitioning
      // from the unoptimized shared version to the optimized one in the
      // function. The return address (from_) points to unoptimized code.
      Code* compiled_code = function->code();
      ASSERT(compiled_code->kind() == Code::OPTIMIZED_FUNCTION);
      ASSERT(!compiled_code->contains(from_));
      return compiled_code;
    }
    case Deoptimizer::DEBUGGER:
      ASSERT(optimized_code->contains(from_));
      return optimized_code;
  }
  UNREACHABLE();
  return NULL;
}


void Deoptimizer::PrintFunctionName() {
  if (function_->IsJSFunction()) {
    function_->PrintName();
  } else {
    PrintF("%s", Code::Kind2String(compiled_code_->kind()));
  }
}


Deoptimizer::~Deoptimizer() {
  ASSERT(input_ == NULL && output_ == NULL);
  ASSERT(disallow_heap_allocation_ == NULL);
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
  ASSERT(id >= 0);
  if (id >= kMaxNumberOfEntries) return NULL;
  if (mode == ENSURE_ENTRY_CODE) {
    EnsureCodeForDeoptimizationEntry(isolate, type, id);
  } else {
    ASSERT(mode == CALCULATE_ENTRY_ADDRESS);
  }
  DeoptimizerData* data = isolate->deoptimizer_data();
  ASSERT(type < kBailoutTypesWithCodeEntry);
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
  PrintF("[couldn't find pc offset for node=%d]\n", id.ToInt());
  PrintF("[method: %s]\n", *shared->DebugName()->ToCString());
  // Print the source code if available.
  HeapStringAllocator string_allocator;
  StringStream stream(&string_allocator);
  shared->SourceCodePrint(&stream, -1);
  PrintF("[source:\n%s\n]", *stream.ToCString());

  FATAL("unable to find pc offset during deoptimization");
  return -1;
}


int Deoptimizer::GetDeoptimizedCodeCount(Isolate* isolate) {
  int length = 0;
  DeoptimizingCodeListNode* node =
      isolate->deoptimizer_data()->deoptimizing_code_list_;
  while (node != NULL) {
    length++;
    node = node->next();
  }
  return length;
}


// We rely on this function not causing a GC.  It is called from generated code
// without having a real stack frame in place.
void Deoptimizer::DoComputeOutputFrames() {
  if (bailout_type_ == OSR) {
    DoComputeOsrOutputFrame();
    return;
  }

  // Print some helpful diagnostic information.
  int64_t start = OS::Ticks();
  if (trace_) {
    PrintF("[deoptimizing (DEOPT %s): begin 0x%08" V8PRIxPTR " ",
           MessageFor(bailout_type_),
           reinterpret_cast<intptr_t>(function_));
    PrintFunctionName();
    PrintF(" @%d, FP to SP delta: %d]\n", bailout_id_, fp_to_sp_delta_);
    if (bailout_type_ == EAGER || bailout_type_ == SOFT) {
      compiled_code_->PrintDeoptLocation(bailout_id_);
    }
  }

  // Determine basic deoptimization information.  The optimized frame is
  // described by the input data.
  DeoptimizationInputData* input_data =
      DeoptimizationInputData::cast(compiled_code_->deoptimization_data());
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
      case Translation::DUPLICATE:
      default:
        UNREACHABLE();
        break;
    }
  }

  // Print some helpful diagnostic information.
  if (trace_) {
    double ms = static_cast<double>(OS::Ticks() - start) / 1000;
    int index = output_count_ - 1;  // Index of the topmost frame.
    JSFunction* function = output_[index]->GetFunction();
    PrintF("[deoptimizing (%s): end 0x%08" V8PRIxPTR " ",
           MessageFor(bailout_type_),
           reinterpret_cast<intptr_t>(function));
    PrintFunctionName();
    PrintF(" @%d => node=%d, pc=0x%08" V8PRIxPTR ", state=%s, alignment=%s,"
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
    ASSERT_EQ(Translation::kSelfLiteralId, closure_id);
    function = function_;
  }
  unsigned height = iterator->Next();
  unsigned height_in_bytes = height * kPointerSize;
  if (trace_) {
    PrintF("  translating ");
    function->PrintName();
    PrintF(" => node=%d, height=%d\n", node_id.ToInt(), height_in_bytes);
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
  ASSERT(frame_index >= 0 && frame_index < output_count_);
  ASSERT(output_[frame_index] == NULL);
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
    top_address = input_->GetRegister(fp_reg.code()) - (2 * kPointerSize) -
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
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  intptr_t value;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
  } else {
    value = output_[frame_index - 1]->GetPc();
  }
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR  " ; caller's pc\n",
           top_address + output_offset, output_offset, value);
  }

  // The caller's frame pointer for the bottommost output frame is the same
  // as in the input frame.  For all subsequent output frames, it can be
  // read from the previous one.  Also compute and set this frame's frame
  // pointer.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
  } else {
    value = output_[frame_index - 1]->GetFp();
  }
  output_frame->SetFrameSlot(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  ASSERT(!is_bottommost || (input_->GetRegister(fp_reg.code()) +
      has_alignment_padding_ * kPointerSize) == fp_value);
  output_frame->SetFp(fp_value);
  if (is_topmost) output_frame->SetRegister(fp_reg.code(), fp_value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           fp_value, output_offset, value);
  }
  ASSERT(!is_bottommost || !has_alignment_padding_ ||
         (fp_value & kPointerSize) != 0);

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
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
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
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR "; function\n",
           top_address + output_offset, output_offset, value);
  }

  // Translate the rest of the frame.
  for (unsigned i = 0; i < height; ++i) {
    output_offset -= kPointerSize;
    DoTranslateCommand(iterator, frame_index, output_offset);
  }
  ASSERT(0 == output_offset);

  // Compute this frame's PC, state, and continuation.
  Code* non_optimized_code = function->shared()->code();
  FixedArray* raw_data = non_optimized_code->deoptimization_data();
  DeoptimizationOutputData* data = DeoptimizationOutputData::cast(raw_data);
  Address start = non_optimized_code->instruction_start();
  unsigned pc_and_state = GetOutputInfo(data, node_id, function->shared());
  unsigned pc_offset = FullCodeGenerator::PcField::decode(pc_and_state);
  intptr_t pc_value = reinterpret_cast<intptr_t>(start + pc_offset);
  output_frame->SetPc(pc_value);

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
      ASSERT(bailout_type_ == EAGER);
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
  if (trace_) {
    PrintF("  translating arguments adaptor => height=%d\n", height_in_bytes);
  }

  unsigned fixed_frame_size = ArgumentsAdaptorFrameConstants::kFrameSize;
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, function);
  output_frame->SetFrameType(StackFrame::ARGUMENTS_ADAPTOR);

  // Arguments adaptor can not be topmost or bottommost.
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
    DoTranslateCommand(iterator, frame_index, output_offset);
  }

  // Read caller's PC from the previous frame.
  output_offset -= kPointerSize;
  intptr_t callers_pc = output_[frame_index - 1]->GetPc();
  output_frame->SetFrameSlot(output_offset, callers_pc);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's pc\n",
           top_address + output_offset, output_offset, callers_pc);
  }

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_offset -= kPointerSize;
  intptr_t value = output_[frame_index - 1]->GetFp();
  output_frame->SetFrameSlot(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           fp_value, output_offset, value);
  }

  // A marker value is used in place of the context.
  output_offset -= kPointerSize;
  intptr_t context = reinterpret_cast<intptr_t>(
      Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  output_frame->SetFrameSlot(output_offset, context);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; context (adaptor sentinel)\n",
           top_address + output_offset, output_offset, context);
  }

  // The function was mentioned explicitly in the ARGUMENTS_ADAPTOR_FRAME.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; function\n",
           top_address + output_offset, output_offset, value);
  }

  // Number of incoming arguments.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(height - 1));
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
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
}


void Deoptimizer::DoComputeConstructStubFrame(TranslationIterator* iterator,
                                              int frame_index) {
  Builtins* builtins = isolate_->builtins();
  Code* construct_stub = builtins->builtin(Builtins::kJSConstructStubGeneric);
  JSFunction* function = JSFunction::cast(ComputeLiteral(iterator->Next()));
  unsigned height = iterator->Next();
  unsigned height_in_bytes = height * kPointerSize;
  if (trace_) {
    PrintF("  translating construct stub => height=%d\n", height_in_bytes);
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
    DoTranslateCommand(iterator, frame_index, output_offset);
  }

  // Read caller's PC from the previous frame.
  output_offset -= kPointerSize;
  intptr_t callers_pc = output_[frame_index - 1]->GetPc();
  output_frame->SetFrameSlot(output_offset, callers_pc);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's pc\n",
           top_address + output_offset, output_offset, callers_pc);
  }

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_offset -= kPointerSize;
  intptr_t value = output_[frame_index - 1]->GetFp();
  output_frame->SetFrameSlot(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           fp_value, output_offset, value);
  }

  // The context can be gotten from the previous frame.
  output_offset -= kPointerSize;
  value = output_[frame_index - 1]->GetContext();
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; context\n",
           top_address + output_offset, output_offset, value);
  }

  // A marker value is used in place of the function.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(StackFrame::CONSTRUCT));
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; function (construct sentinel)\n",
           top_address + output_offset, output_offset, value);
  }

  // The output frame reflects a JSConstructStubGeneric frame.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(construct_stub);
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; code object\n",
           top_address + output_offset, output_offset, value);
  }

  // Number of incoming arguments.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(height - 1));
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; argc (%d)\n",
           top_address + output_offset, output_offset, value, height - 1);
  }

  // Constructor function being invoked by the stub (only present on some
  // architectures, indicated by kConstructorOffset).
  if (ConstructFrameConstants::kConstructorOffset != kMinInt) {
    output_offset -= kPointerSize;
    value = reinterpret_cast<intptr_t>(function);
    output_frame->SetFrameSlot(output_offset, value);
    if (trace_) {
      PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR " ; constructor function\n",
             top_address + output_offset, output_offset, value);
    }
  }

  // The newly allocated object was passed as receiver in the artificial
  // constructor stub environment created by HEnvironment::CopyForInlining().
  output_offset -= kPointerSize;
  value = output_frame->GetFrameSlot(output_frame_size - kPointerSize);
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; allocated receiver\n",
           top_address + output_offset, output_offset, value);
  }

  ASSERT(0 == output_offset);

  intptr_t pc = reinterpret_cast<intptr_t>(
      construct_stub->instruction_start() +
      isolate_->heap()->construct_stub_deopt_pc_offset()->value());
  output_frame->SetPc(pc);
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
  if (trace_) {
    PrintF("  translating %s stub => height=%u\n", kind, height_in_bytes);
  }

  // We need 1 stack entry for the return address + 4 stack entries from
  // StackFrame::INTERNAL (FP, context, frame type, code object, see
  // MacroAssembler::EnterFrame). For a setter stub frame we need one additional
  // entry for the implicit return value, see
  // StoreStubCompiler::CompileStoreViaSetter.
  unsigned fixed_frame_entries = 1 + 4 + (is_setter_stub_frame ? 1 : 0);
  unsigned fixed_frame_size = fixed_frame_entries * kPointerSize;
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, accessor);
  output_frame->SetFrameType(StackFrame::INTERNAL);

  // A frame for an accessor stub can not be the topmost or bottommost one.
  ASSERT(frame_index > 0 && frame_index < output_count_ - 1);
  ASSERT(output_[frame_index] == NULL);
  output_[frame_index] = output_frame;

  // The top address of the frame is computed from the previous frame's top and
  // this frame's size.
  intptr_t top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  output_frame->SetTop(top_address);

  unsigned output_offset = output_frame_size;

  // Read caller's PC from the previous frame.
  output_offset -= kPointerSize;
  intptr_t callers_pc = output_[frame_index - 1]->GetPc();
  output_frame->SetFrameSlot(output_offset, callers_pc);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; caller's pc\n",
           top_address + output_offset, output_offset, callers_pc);
  }

  // Read caller's FP from the previous frame, and set this frame's FP.
  output_offset -= kPointerSize;
  intptr_t value = output_[frame_index - 1]->GetFp();
  output_frame->SetFrameSlot(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  output_frame->SetFp(fp_value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; caller's fp\n",
           fp_value, output_offset, value);
  }

  // The context can be gotten from the previous frame.
  output_offset -= kPointerSize;
  value = output_[frame_index - 1]->GetContext();
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; context\n",
           top_address + output_offset, output_offset, value);
  }

  // A marker value is used in place of the function.
  output_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(Smi::FromInt(StackFrame::INTERNAL));
  output_frame->SetFrameSlot(output_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
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
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %u] <- 0x%08" V8PRIxPTR
           " ; code object\n",
           top_address + output_offset, output_offset, value);
  }

  // Skip receiver.
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  iterator->Skip(Translation::NumberOfOperandsFor(opcode));

  if (is_setter_stub_frame) {
    // The implicit return value was part of the artificial setter stub
    // environment.
    output_offset -= kPointerSize;
    DoTranslateCommand(iterator, frame_index, output_offset);
  }

  ASSERT(0 == output_offset);

  Smi* offset = is_setter_stub_frame ?
      isolate_->heap()->setter_stub_deopt_pc_offset() :
      isolate_->heap()->getter_stub_deopt_pc_offset();
  intptr_t pc = reinterpret_cast<intptr_t>(
      accessor_stub->instruction_start() + offset->value());
  output_frame->SetPc(pc);
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

  ASSERT(compiled_code_->is_crankshafted() &&
         compiled_code_->kind() != Code::OPTIMIZED_FUNCTION);
  int major_key = compiled_code_->major_key();
  CodeStubInterfaceDescriptor* descriptor =
      isolate_->code_stub_interface_descriptor(major_key);

  // The output frame must have room for all pushed register parameters
  // and the standard stack frame slots.  Include space for an argument
  // object to the callee and optionally the space to pass the argument
  // object to the stub failure handler.
  ASSERT(descriptor->register_param_count_ >= 0);
  int height_in_bytes = kPointerSize * descriptor->register_param_count_ +
      sizeof(Arguments) + kPointerSize;
  int fixed_frame_size = StandardFrameConstants::kFixedFrameSize;
  int input_frame_size = input_->GetFrameSize();
  int output_frame_size = height_in_bytes + fixed_frame_size;
  if (trace_) {
    PrintF("  translating %s => StubFailureTrampolineStub, height=%d\n",
           CodeStub::MajorName(static_cast<CodeStub::Major>(major_key), false),
           height_in_bytes);
  }

  // The stub failure trampoline is a single frame.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, NULL);
  output_frame->SetFrameType(StackFrame::STUB_FAILURE_TRAMPOLINE);
  ASSERT(frame_index == 0);
  output_[frame_index] = output_frame;

  // The top address for the output frame can be computed from the input
  // frame pointer and the output frame's height. Subtract space for the
  // context and function slots.
  Register fp_reg = StubFailureTrampolineFrame::fp_register();
  intptr_t top_address = input_->GetRegister(fp_reg.code()) -
      (2 * kPointerSize) - height_in_bytes;
  output_frame->SetTop(top_address);

  // Read caller's PC (JSFunction continuation) from the input frame.
  unsigned input_frame_offset = input_frame_size - kPointerSize;
  unsigned output_frame_offset = output_frame_size - kPointerSize;
  intptr_t value = input_->GetFrameSlot(input_frame_offset);
  output_frame->SetFrameSlot(output_frame_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's pc\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  // Read caller's FP from the input frame, and set this frame's FP.
  input_frame_offset -= kPointerSize;
  value = input_->GetFrameSlot(input_frame_offset);
  output_frame_offset -= kPointerSize;
  output_frame->SetFrameSlot(output_frame_offset, value);
  intptr_t frame_ptr = input_->GetRegister(fp_reg.code());
  output_frame->SetRegister(fp_reg.code(), frame_ptr);
  output_frame->SetFp(frame_ptr);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  // The context can be gotten from the input frame.
  Register context_reg = StubFailureTrampolineFrame::context_register();
  input_frame_offset -= kPointerSize;
  value = input_->GetFrameSlot(input_frame_offset);
  output_frame->SetRegister(context_reg.code(), value);
  output_frame_offset -= kPointerSize;
  output_frame->SetFrameSlot(output_frame_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; context\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  // A marker value is used in place of the function.
  output_frame_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(
      Smi::FromInt(StackFrame::STUB_FAILURE_TRAMPOLINE));
  output_frame->SetFrameSlot(output_frame_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; function (stub failure sentinel)\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  intptr_t caller_arg_count = 0;
  bool arg_count_known = descriptor->stack_parameter_count_ == NULL;

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
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; args.arguments %s\n",
           top_address + args_arguments_offset, args_arguments_offset, value,
           arg_count_known ? "" : "(the hole)");
  }

  output_frame_offset -= kPointerSize;
  int length_frame_offset = output_frame_offset;
  value = arg_count_known ? caller_arg_count : the_hole;
  output_frame->SetFrameSlot(length_frame_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; args.length %s\n",
           top_address + length_frame_offset, length_frame_offset, value,
           arg_count_known ? "" : "(the hole)");
  }

  output_frame_offset -= kPointerSize;
  value = frame_ptr - (output_frame_size - output_frame_offset) -
      StandardFrameConstants::kMarkerOffset + kPointerSize;
  output_frame->SetFrameSlot(output_frame_offset, value);
  if (trace_) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; args*\n",
           top_address + output_frame_offset, output_frame_offset, value);
  }

  // Copy the register parameters to the failure frame.
  for (int i = 0; i < descriptor->register_param_count_; ++i) {
    output_frame_offset -= kPointerSize;
    DoTranslateCommand(iterator, 0, output_frame_offset);
  }

  if (!arg_count_known) {
    DoTranslateCommand(iterator, 0, length_frame_offset,
                       TRANSLATED_VALUE_IS_NATIVE);
    caller_arg_count = output_frame->GetFrameSlot(length_frame_offset);
    value = frame_ptr + StandardFrameConstants::kCallerSPOffset +
        (caller_arg_count - 1) * kPointerSize;
    output_frame->SetFrameSlot(args_arguments_offset, value);
    if (trace_) {
      PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
             V8PRIxPTR " ; args.arguments\n",
             top_address + args_arguments_offset, args_arguments_offset, value);
    }
  }

  ASSERT(0 == output_frame_offset);

  // Copy the double registers from the input into the output frame.
  CopyDoubleRegisters(output_frame);

  // Fill registers containing handler and number of parameters.
  SetPlatformCompiledStubRegisters(output_frame, descriptor);

  // Compute this frame's PC, state, and continuation.
  Code* trampoline = NULL;
  StubFunctionMode function_mode = descriptor->function_mode_;
  StubFailureTrampolineStub(function_mode).FindCodeInCache(&trampoline,
                                                           isolate_);
  ASSERT(trampoline != NULL);
  output_frame->SetPc(reinterpret_cast<intptr_t>(
      trampoline->instruction_start()));
  output_frame->SetState(Smi::FromInt(FullCodeGenerator::NO_REGISTERS));
  Code* notify_failure =
      isolate_->builtins()->builtin(Builtins::kNotifyStubFailure);
  output_frame->SetContinuation(
      reinterpret_cast<intptr_t>(notify_failure->entry()));
}


void Deoptimizer::MaterializeHeapObjects(JavaScriptFrameIterator* it) {
  ASSERT_NE(DEBUGGER, bailout_type_);

  // Handlify all argument object values before triggering any allocation.
  List<Handle<Object> > values(deferred_arguments_objects_values_.length());
  for (int i = 0; i < deferred_arguments_objects_values_.length(); ++i) {
    values.Add(Handle<Object>(deferred_arguments_objects_values_[i],
                              isolate_));
  }

  // Play it safe and clear all unhandlified values before we continue.
  deferred_arguments_objects_values_.Clear();

  // Materialize all heap numbers before looking at arguments because when the
  // output frames are used to materialize arguments objects later on they need
  // to already contain valid heap numbers.
  for (int i = 0; i < deferred_heap_numbers_.length(); i++) {
    HeapNumberMaterializationDescriptor d = deferred_heap_numbers_[i];
    Handle<Object> num = isolate_->factory()->NewNumber(d.value());
    if (trace_) {
      PrintF("Materializing a new heap number %p [%e] in slot %p\n",
             reinterpret_cast<void*>(*num),
             d.value(),
             d.slot_address());
    }
    Memory::Object_at(d.slot_address()) = *num;
  }

  // Materialize arguments objects one frame at a time.
  for (int frame_index = 0; frame_index < jsframe_count(); ++frame_index) {
    if (frame_index != 0) it->Advance();
    JavaScriptFrame* frame = it->frame();
    Handle<JSFunction> function(JSFunction::cast(frame->function()), isolate_);
    Handle<JSObject> arguments;
    for (int i = frame->ComputeExpressionsCount() - 1; i >= 0; --i) {
      if (frame->GetExpression(i) == isolate_->heap()->arguments_marker()) {
        ArgumentsObjectMaterializationDescriptor descriptor =
            deferred_arguments_objects_.RemoveLast();
        const int length = descriptor.arguments_length();
        if (arguments.is_null()) {
          if (frame->has_adapted_arguments()) {
            // Use the arguments adapter frame we just built to materialize the
            // arguments object. FunctionGetArguments can't throw an exception,
            // so cast away the doubt with an assert.
            arguments = Handle<JSObject>(JSObject::cast(
                Accessors::FunctionGetArguments(*function,
                                                NULL)->ToObjectUnchecked()));
            values.RewindBy(length);
          } else {
            // Construct an arguments object and copy the parameters to a newly
            // allocated arguments object backing store.
            arguments =
                isolate_->factory()->NewArgumentsObject(function, length);
            Handle<FixedArray> array =
                isolate_->factory()->NewFixedArray(length);
            ASSERT(array->length() == length);
            for (int i = length - 1; i >= 0 ; --i) {
              array->set(i, *values.RemoveLast());
            }
            arguments->set_elements(*array);
          }
        }
        frame->SetExpression(i, *arguments);
        ASSERT_EQ(Memory::Object_at(descriptor.slot_address()), *arguments);
        if (trace_) {
          PrintF("Materializing %sarguments object of length %d for %p: ",
                 frame->has_adapted_arguments() ? "(adapted) " : "",
                 arguments->elements()->length(),
                 reinterpret_cast<void*>(descriptor.slot_address()));
          arguments->ShortPrint();
          PrintF("\n");
        }
      }
    }
  }
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void Deoptimizer::MaterializeHeapNumbersForDebuggerInspectableFrame(
    Address parameters_top,
    uint32_t parameters_size,
    Address expressions_top,
    uint32_t expressions_size,
    DeoptimizedFrameInfo* info) {
  ASSERT_EQ(DEBUGGER, bailout_type_);
  Address parameters_bottom = parameters_top + parameters_size;
  Address expressions_bottom = expressions_top + expressions_size;
  for (int i = 0; i < deferred_heap_numbers_.length(); i++) {
    HeapNumberMaterializationDescriptor d = deferred_heap_numbers_[i];

    // Check of the heap number to materialize actually belong to the frame
    // being extracted.
    Address slot = d.slot_address();
    if (parameters_top <= slot && slot < parameters_bottom) {
      Handle<Object> num = isolate_->factory()->NewNumber(d.value());

      int index = (info->parameters_count() - 1) -
          static_cast<int>(slot - parameters_top) / kPointerSize;

      if (trace_) {
        PrintF("Materializing a new heap number %p [%e] in slot %p"
               "for parameter slot #%d\n",
               reinterpret_cast<void*>(*num),
               d.value(),
               d.slot_address(),
               index);
      }

      info->SetParameter(index, *num);
    } else if (expressions_top <= slot && slot < expressions_bottom) {
      Handle<Object> num = isolate_->factory()->NewNumber(d.value());

      int index = info->expression_count() - 1 -
          static_cast<int>(slot - expressions_top) / kPointerSize;

      if (trace_) {
        PrintF("Materializing a new heap number %p [%e] in slot %p"
               "for expression slot #%d\n",
               reinterpret_cast<void*>(*num),
               d.value(),
               d.slot_address(),
               index);
      }

      info->SetExpression(index, *num);
    }
  }
}
#endif


static const char* TraceValueType(bool is_smi, bool is_native) {
  if (is_native) {
    return "native";
  } else if (is_smi) {
    return "smi";
  }

  return "heap number";
}


void Deoptimizer::DoTranslateCommand(TranslationIterator* iterator,
    int frame_index,
    unsigned output_offset,
    DeoptimizerTranslatedValueType value_type) {
  disasm::NameConverter converter;
  // A GC-safe temporary placeholder that we can put in the output frame.
  const intptr_t kPlaceholder = reinterpret_cast<intptr_t>(Smi::FromInt(0));
  bool is_native = value_type == TRANSLATED_VALUE_IS_NATIVE;

  // Ignore commands marked as duplicate and act on the first non-duplicate.
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  while (opcode == Translation::DUPLICATE) {
    opcode = static_cast<Translation::Opcode>(iterator->Next());
    iterator->Skip(Translation::NumberOfOperandsFor(opcode));
    opcode = static_cast<Translation::Opcode>(iterator->Next());
  }

  switch (opcode) {
    case Translation::BEGIN:
    case Translation::JS_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::GETTER_STUB_FRAME:
    case Translation::SETTER_STUB_FRAME:
    case Translation::COMPILED_STUB_FRAME:
    case Translation::DUPLICATE:
      UNREACHABLE();
      return;

    case Translation::REGISTER: {
      int input_reg = iterator->Next();
      intptr_t input_value = input_->GetRegister(input_reg);
      if (trace_) {
        PrintF(
            "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08" V8PRIxPTR " ; %s ",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            input_value,
            converter.NameOfCPURegister(input_reg));
        reinterpret_cast<Object*>(input_value)->ShortPrint();
        PrintF("\n");
      }
      output_[frame_index]->SetFrameSlot(output_offset, input_value);
      return;
    }

    case Translation::INT32_REGISTER: {
      int input_reg = iterator->Next();
      intptr_t value = input_->GetRegister(input_reg);
      bool is_smi = (value_type == TRANSLATED_VALUE_IS_TAGGED) &&
          Smi::IsValid(value);

      if (trace_) {
        PrintF(
            "    0x%08" V8PRIxPTR ": [top + %d] <- %" V8PRIdPTR " ; %s (%s)\n",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            value,
            converter.NameOfCPURegister(input_reg),
            TraceValueType(is_smi, is_native));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, tagged_value);
      } else if (value_type == TRANSLATED_VALUE_IS_NATIVE) {
        output_[frame_index]->SetFrameSlot(output_offset, value);
      } else {
        // We save the untagged value on the side and store a GC-safe
        // temporary placeholder in the frame.
        ASSERT(value_type == TRANSLATED_VALUE_IS_TAGGED);
        AddDoubleValue(output_[frame_index]->GetTop() + output_offset,
                       static_cast<double>(static_cast<int32_t>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::UINT32_REGISTER: {
      int input_reg = iterator->Next();
      uintptr_t value = static_cast<uintptr_t>(input_->GetRegister(input_reg));
      bool is_smi = (value_type == TRANSLATED_VALUE_IS_TAGGED) &&
          (value <= static_cast<uintptr_t>(Smi::kMaxValue));
      if (trace_) {
        PrintF(
            "    0x%08" V8PRIxPTR ": [top + %d] <- %" V8PRIuPTR
            " ; uint %s (%s)\n",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            value,
            converter.NameOfCPURegister(input_reg),
            TraceValueType(is_smi, is_native));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, tagged_value);
      } else if (value_type == TRANSLATED_VALUE_IS_NATIVE) {
        output_[frame_index]->SetFrameSlot(output_offset, value);
      } else {
        // We save the untagged value on the side and store a GC-safe
        // temporary placeholder in the frame.
        ASSERT(value_type == TRANSLATED_VALUE_IS_TAGGED);
        AddDoubleValue(output_[frame_index]->GetTop() + output_offset,
                       static_cast<double>(static_cast<uint32_t>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::DOUBLE_REGISTER: {
      int input_reg = iterator->Next();
      double value = input_->GetDoubleRegister(input_reg);
      if (trace_) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- %e ; %s\n",
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
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(input_slot_index);
      intptr_t input_value = input_->GetFrameSlot(input_offset);
      if (trace_) {
        PrintF("    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF("[top + %d] <- 0x%08" V8PRIxPTR " ; [sp + %d] ",
               output_offset,
               input_value,
               input_offset);
        reinterpret_cast<Object*>(input_value)->ShortPrint();
        PrintF("\n");
      }
      output_[frame_index]->SetFrameSlot(output_offset, input_value);
      return;
    }

    case Translation::INT32_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(input_slot_index);
      intptr_t value = input_->GetFrameSlot(input_offset);
      bool is_smi = (value_type == TRANSLATED_VALUE_IS_TAGGED) &&
          Smi::IsValid(value);
      if (trace_) {
        PrintF("    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF("[top + %d] <- %" V8PRIdPTR " ; [sp + %d] (%s)\n",
               output_offset,
               value,
               input_offset,
               TraceValueType(is_smi, is_native));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, tagged_value);
      } else if (value_type == TRANSLATED_VALUE_IS_NATIVE) {
        output_[frame_index]->SetFrameSlot(output_offset, value);
      } else {
        // We save the untagged value on the side and store a GC-safe
        // temporary placeholder in the frame.
        ASSERT(value_type == TRANSLATED_VALUE_IS_TAGGED);
        AddDoubleValue(output_[frame_index]->GetTop() + output_offset,
                       static_cast<double>(static_cast<int32_t>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::UINT32_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(input_slot_index);
      uintptr_t value =
          static_cast<uintptr_t>(input_->GetFrameSlot(input_offset));
      bool is_smi = (value_type == TRANSLATED_VALUE_IS_TAGGED) &&
          (value <= static_cast<uintptr_t>(Smi::kMaxValue));
      if (trace_) {
        PrintF("    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF("[top + %d] <- %" V8PRIuPTR " ; [sp + %d] (uint32 %s)\n",
               output_offset,
               value,
               input_offset,
               TraceValueType(is_smi, is_native));
      }
      if (is_smi) {
        intptr_t tagged_value =
            reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, tagged_value);
      } else if (value_type == TRANSLATED_VALUE_IS_NATIVE) {
        output_[frame_index]->SetFrameSlot(output_offset, value);
      } else {
        // We save the untagged value on the side and store a GC-safe
        // temporary placeholder in the frame.
        ASSERT(value_type == TRANSLATED_VALUE_IS_TAGGED);
        AddDoubleValue(output_[frame_index]->GetTop() + output_offset,
                       static_cast<double>(static_cast<uint32_t>(value)));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::DOUBLE_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(input_slot_index);
      double value = input_->GetDoubleFrameSlot(input_offset);
      if (trace_) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- %e ; [sp + %d]\n",
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
      if (trace_) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- ",
               output_[frame_index]->GetTop() + output_offset,
               output_offset);
        literal->ShortPrint();
        PrintF(" ; literal\n");
      }
      intptr_t value = reinterpret_cast<intptr_t>(literal);
      output_[frame_index]->SetFrameSlot(output_offset, value);
      return;
    }

    case Translation::ARGUMENTS_OBJECT: {
      bool args_known = iterator->Next();
      int args_index = iterator->Next() + 1;  // Skip receiver.
      int args_length = iterator->Next() - 1;  // Skip receiver.
      if (trace_) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- ",
               output_[frame_index]->GetTop() + output_offset,
               output_offset);
        isolate_->heap()->arguments_marker()->ShortPrint();
        PrintF(" ; %sarguments object\n", args_known ? "" : "dummy ");
      }
      // Use the arguments marker value as a sentinel and fill in the arguments
      // object after the deoptimized frame is built.
      intptr_t value = reinterpret_cast<intptr_t>(
          isolate_->heap()->arguments_marker());
      AddArgumentsObject(
          output_[frame_index]->GetTop() + output_offset, args_length);
      output_[frame_index]->SetFrameSlot(output_offset, value);
      // We save the tagged argument values on the side and materialize the
      // actual arguments object after the deoptimized frame is built.
      for (int i = 0; i < args_length; i++) {
        unsigned input_offset = input_->GetOffsetFromSlotIndex(args_index + i);
        intptr_t input_value = args_known
            ? input_->GetFrameSlot(input_offset)
            : reinterpret_cast<intptr_t>(isolate_->heap()->the_hole_value());
        AddArgumentsObjectValue(input_value);
      }
      return;
    }
  }
}


bool Deoptimizer::DoOsrTranslateCommand(TranslationIterator* iterator,
                                        int* input_offset) {
  disasm::NameConverter converter;
  FrameDescription* output = output_[0];

  // The input values are all part of the unoptimized frame so they
  // are all tagged pointers.
  uintptr_t input_value = input_->GetFrameSlot(*input_offset);
  Object* input_object = reinterpret_cast<Object*>(input_value);

  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  bool duplicate = (opcode == Translation::DUPLICATE);
  if (duplicate) {
    opcode = static_cast<Translation::Opcode>(iterator->Next());
  }

  switch (opcode) {
    case Translation::BEGIN:
    case Translation::JS_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::GETTER_STUB_FRAME:
    case Translation::SETTER_STUB_FRAME:
    case Translation::COMPILED_STUB_FRAME:
    case Translation::DUPLICATE:
      UNREACHABLE();  // Malformed input.
       return false;

     case Translation::REGISTER: {
       int output_reg = iterator->Next();
       if (FLAG_trace_osr) {
         PrintF("    %s <- 0x%08" V8PRIxPTR " ; [sp + %d]\n",
                converter.NameOfCPURegister(output_reg),
                input_value,
                *input_offset);
       }
       output->SetRegister(output_reg, input_value);
       break;
     }

    case Translation::INT32_REGISTER: {
      int32_t int32_value = 0;
      if (!input_object->ToInt32(&int32_value)) return false;

      int output_reg = iterator->Next();
      if (FLAG_trace_osr) {
        PrintF("    %s <- %d (int32) ; [sp + %d]\n",
               converter.NameOfCPURegister(output_reg),
               int32_value,
               *input_offset);
      }
      output->SetRegister(output_reg, int32_value);
      break;
    }

    case Translation::UINT32_REGISTER: {
      uint32_t uint32_value = 0;
      if (!input_object->ToUint32(&uint32_value)) return false;

      int output_reg = iterator->Next();
      if (FLAG_trace_osr) {
        PrintF("    %s <- %u (uint32) ; [sp + %d]\n",
               converter.NameOfCPURegister(output_reg),
               uint32_value,
               *input_offset);
      }
      output->SetRegister(output_reg, static_cast<int32_t>(uint32_value));
    }


    case Translation::DOUBLE_REGISTER: {
      // Abort OSR if we don't have a number.
      if (!input_object->IsNumber()) return false;

      int output_reg = iterator->Next();
      double double_value = input_object->Number();
      if (FLAG_trace_osr) {
        PrintF("    %s <- %g (double) ; [sp + %d]\n",
               DoubleRegister::AllocationIndexToString(output_reg),
               double_value,
               *input_offset);
      }
      output->SetDoubleRegister(output_reg, double_value);
      break;
    }

    case Translation::STACK_SLOT: {
      int output_index = iterator->Next();
      unsigned output_offset =
          output->GetOffsetFromSlotIndex(output_index);
      if (FLAG_trace_osr) {
        PrintF("    [sp + %d] <- 0x%08" V8PRIxPTR " ; [sp + %d] ",
               output_offset,
               input_value,
               *input_offset);
        reinterpret_cast<Object*>(input_value)->ShortPrint();
        PrintF("\n");
      }
      output->SetFrameSlot(output_offset, input_value);
      break;
    }

    case Translation::INT32_STACK_SLOT: {
      int32_t int32_value = 0;
      if (!input_object->ToInt32(&int32_value)) return false;

      int output_index = iterator->Next();
      unsigned output_offset =
          output->GetOffsetFromSlotIndex(output_index);
      if (FLAG_trace_osr) {
        PrintF("    [sp + %d] <- %d (int32) ; [sp + %d]\n",
               output_offset,
               int32_value,
               *input_offset);
      }
      output->SetFrameSlot(output_offset, int32_value);
      break;
    }

    case Translation::UINT32_STACK_SLOT: {
      uint32_t uint32_value = 0;
      if (!input_object->ToUint32(&uint32_value)) return false;

      int output_index = iterator->Next();
      unsigned output_offset =
          output->GetOffsetFromSlotIndex(output_index);
      if (FLAG_trace_osr) {
        PrintF("    [sp + %d] <- %u (uint32) ; [sp + %d]\n",
               output_offset,
               uint32_value,
               *input_offset);
      }
      output->SetFrameSlot(output_offset, static_cast<int32_t>(uint32_value));
      break;
    }

    case Translation::DOUBLE_STACK_SLOT: {
      static const int kLowerOffset = 0 * kPointerSize;
      static const int kUpperOffset = 1 * kPointerSize;

      // Abort OSR if we don't have a number.
      if (!input_object->IsNumber()) return false;

      int output_index = iterator->Next();
      unsigned output_offset =
          output->GetOffsetFromSlotIndex(output_index);
      double double_value = input_object->Number();
      uint64_t int_value = BitCast<uint64_t, double>(double_value);
      int32_t lower = static_cast<int32_t>(int_value);
      int32_t upper = static_cast<int32_t>(int_value >> kBitsPerInt);
      if (FLAG_trace_osr) {
        PrintF("    [sp + %d] <- 0x%08x (upper bits of %g) ; [sp + %d]\n",
               output_offset + kUpperOffset,
               upper,
               double_value,
               *input_offset);
        PrintF("    [sp + %d] <- 0x%08x (lower bits of %g) ; [sp + %d]\n",
               output_offset + kLowerOffset,
               lower,
               double_value,
               *input_offset);
      }
      output->SetFrameSlot(output_offset + kLowerOffset, lower);
      output->SetFrameSlot(output_offset + kUpperOffset, upper);
      break;
    }

    case Translation::LITERAL: {
      // Just ignore non-materialized literals.
      iterator->Next();
      break;
    }

    case Translation::ARGUMENTS_OBJECT: {
      // Optimized code assumes that the argument object has not been
      // materialized and so bypasses it when doing arguments access.
      // We should have bailed out before starting the frame
      // translation.
      UNREACHABLE();
      return false;
    }
  }

  if (!duplicate) *input_offset -= kPointerSize;
  return true;
}


void Deoptimizer::PatchInterruptCode(Code* unoptimized_code,
                                     Code* interrupt_code,
                                     Code* replacement_code) {
  // Iterate over the back edge table and patch every interrupt
  // call to an unconditional call to the replacement code.
  ASSERT(unoptimized_code->kind() == Code::FUNCTION);
  int loop_nesting_level = unoptimized_code->allow_osr_at_loop_nesting_level();
  Address back_edge_cursor = unoptimized_code->instruction_start() +
      unoptimized_code->back_edge_table_offset();
  uint32_t table_length = Memory::uint32_at(back_edge_cursor);
  back_edge_cursor += kIntSize;
  for (uint32_t i = 0; i < table_length; ++i) {
    uint8_t loop_depth = Memory::uint8_at(back_edge_cursor + 2 * kIntSize);
    if (loop_depth == loop_nesting_level) {
      // Loop back edge has the loop depth that we want to patch.
      uint32_t pc_offset = Memory::uint32_at(back_edge_cursor + kIntSize);
      Address pc_after = unoptimized_code->instruction_start() + pc_offset;
      PatchInterruptCodeAt(unoptimized_code,
                           pc_after,
                           interrupt_code,
                           replacement_code);
    }
    back_edge_cursor += FullCodeGenerator::kBackEdgeEntrySize;
  }
  unoptimized_code->set_back_edges_patched_for_osr(true);
#ifdef DEBUG
  Deoptimizer::VerifyInterruptCode(
      unoptimized_code, interrupt_code, replacement_code, loop_nesting_level);
#endif  // DEBUG
}


void Deoptimizer::RevertInterruptCode(Code* unoptimized_code,
                                      Code* interrupt_code,
                                      Code* replacement_code) {
  // Iterate over the back edge table and revert the patched interrupt calls.
  ASSERT(unoptimized_code->kind() == Code::FUNCTION);
  ASSERT(unoptimized_code->back_edges_patched_for_osr());
  int loop_nesting_level = unoptimized_code->allow_osr_at_loop_nesting_level();
  Address back_edge_cursor = unoptimized_code->instruction_start() +
      unoptimized_code->back_edge_table_offset();
  uint32_t table_length = Memory::uint32_at(back_edge_cursor);
  back_edge_cursor += kIntSize;
  for (uint32_t i = 0; i < table_length; ++i) {
    uint8_t loop_depth = Memory::uint8_at(back_edge_cursor + 2 * kIntSize);
    if (loop_depth <= loop_nesting_level) {
      uint32_t pc_offset = Memory::uint32_at(back_edge_cursor + kIntSize);
      Address pc_after = unoptimized_code->instruction_start() + pc_offset;
      RevertInterruptCodeAt(unoptimized_code,
                            pc_after,
                            interrupt_code,
                            replacement_code);
    }
    back_edge_cursor += FullCodeGenerator::kBackEdgeEntrySize;
  }
  unoptimized_code->set_back_edges_patched_for_osr(false);
#ifdef DEBUG
  // Assert that none of the back edges are patched anymore.
  Deoptimizer::VerifyInterruptCode(
      unoptimized_code, interrupt_code, replacement_code, -1);
#endif  // DEBUG
}


#ifdef DEBUG
void Deoptimizer::VerifyInterruptCode(Code* unoptimized_code,
                                      Code* interrupt_code,
                                      Code* replacement_code,
                                      int loop_nesting_level) {
  CHECK(unoptimized_code->kind() == Code::FUNCTION);
  Address back_edge_cursor = unoptimized_code->instruction_start() +
      unoptimized_code->back_edge_table_offset();
  uint32_t table_length = Memory::uint32_at(back_edge_cursor);
  back_edge_cursor += kIntSize;
  for (uint32_t i = 0; i < table_length; ++i) {
    uint8_t loop_depth = Memory::uint8_at(back_edge_cursor + 2 * kIntSize);
    CHECK_LE(loop_depth, Code::kMaxLoopNestingMarker);
    // Assert that all back edges for shallower loops (and only those)
    // have already been patched.
    uint32_t pc_offset = Memory::uint32_at(back_edge_cursor + kIntSize);
    Address pc_after = unoptimized_code->instruction_start() + pc_offset;
    CHECK_EQ((loop_depth <= loop_nesting_level),
             InterruptCodeIsPatched(unoptimized_code,
                                    pc_after,
                                    interrupt_code,
                                    replacement_code));
    back_edge_cursor += FullCodeGenerator::kBackEdgeEntrySize;
  }
}
#endif  // DEBUG


unsigned Deoptimizer::ComputeInputFrameSize() const {
  unsigned fixed_size = ComputeFixedSize(function_);
  // The fp-to-sp delta already takes the context and the function
  // into account so we have to avoid double counting them (-2).
  unsigned result = fixed_size + fp_to_sp_delta_ - (2 * kPointerSize);
#ifdef DEBUG
  if (bailout_type_ == OSR) {
    // TODO(kasperl): It would be nice if we could verify that the
    // size matches with the stack height we can compute based on the
    // environment at the OSR entry. The code for that his built into
    // the DoComputeOsrOutputFrame function for now.
  } else if (compiled_code_->kind() == Code::OPTIMIZED_FUNCTION) {
    unsigned stack_slots = compiled_code_->stack_slots();
    unsigned outgoing_size = ComputeOutgoingArgumentSize();
    ASSERT(result == fixed_size + (stack_slots * kPointerSize) + outgoing_size);
  }
#endif
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
    ASSERT(Smi::cast(function) == Smi::FromInt(StackFrame::STUB));
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


void Deoptimizer::AddArgumentsObject(intptr_t slot_address, int argc) {
  ArgumentsObjectMaterializationDescriptor object_desc(
      reinterpret_cast<Address>(slot_address), argc);
  deferred_arguments_objects_.Add(object_desc);
}


void Deoptimizer::AddArgumentsObjectValue(intptr_t value) {
  deferred_arguments_objects_values_.Add(reinterpret_cast<Object*>(value));
}


void Deoptimizer::AddDoubleValue(intptr_t slot_address, double value) {
  HeapNumberMaterializationDescriptor value_desc(
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
  ASSERT(type == EAGER || type == SOFT || type == LAZY);
  DeoptimizerData* data = isolate->deoptimizer_data();
  int entry_count = data->deopt_entry_code_entries_[type];
  if (max_entry_id < entry_count) return;
  entry_count = Max(entry_count, Deoptimizer::kMinNumberOfEntries);
  while (max_entry_id >= entry_count) entry_count *= 2;
  ASSERT(entry_count <= Deoptimizer::kMaxNumberOfEntries);

  MacroAssembler masm(isolate, NULL, 16 * KB);
  masm.set_emit_debug_code(false);
  GenerateDeoptimizationEntries(&masm, entry_count, type);
  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  MemoryChunk* chunk = data->deopt_entry_code_[type];
  ASSERT(static_cast<int>(Deoptimizer::GetMaxDeoptTableSize()) >=
         desc.instr_size);
  chunk->CommitArea(desc.instr_size);
  CopyBytes(chunk->area_start(), desc.buffer,
      static_cast<size_t>(desc.instr_size));
  CPU::FlushICache(chunk->area_start(), desc.instr_size);

  data->deopt_entry_code_entries_[type] = entry_count;
}


void Deoptimizer::ReplaceCodeForRelatedFunctions(JSFunction* function,
                                                 Code* code) {
  SharedFunctionInfo* shared = function->shared();
  Object* undefined = function->GetHeap()->undefined_value();
  Object* current = function;

  while (current != undefined) {
    JSFunction* func = JSFunction::cast(current);
    current = func->next_function_link();
    func->set_code(shared->code());
    func->set_next_function_link(undefined);
  }
}


FrameDescription::FrameDescription(uint32_t frame_size,
                                   JSFunction* function)
    : frame_size_(frame_size),
      function_(function),
      top_(kZapUint32),
      pc_(kZapUint32),
      fp_(kZapUint32),
      context_(kZapUint32) {
  // Zap all the registers.
  for (int r = 0; r < Register::kNumRegisters; r++) {
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
      UNREACHABLE();
      return 0;
  }
}


Object* FrameDescription::GetParameter(int index) {
  ASSERT(index >= 0);
  ASSERT(index < ComputeParametersCount());
  // The slot indexes for incoming arguments are negative.
  unsigned offset = GetOffsetFromSlotIndex(index - ComputeParametersCount());
  return reinterpret_cast<Object*>(*GetFrameSlotPointer(offset));
}


unsigned FrameDescription::GetExpressionCount() {
  ASSERT_EQ(StackFrame::JAVA_SCRIPT, type_);
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


void Translation::MarkDuplicate() {
  buffer_->Add(DUPLICATE, zone());
}


int Translation::NumberOfOperandsFor(Opcode opcode) {
  switch (opcode) {
    case DUPLICATE:
      return 0;
    case GETTER_STUB_FRAME:
    case SETTER_STUB_FRAME:
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
    case ARGUMENTS_OBJECT:
      return 3;
  }
  UNREACHABLE();
  return -1;
}


#if defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)

const char* Translation::StringFor(Opcode opcode) {
  switch (opcode) {
    case BEGIN:
      return "BEGIN";
    case JS_FRAME:
      return "JS_FRAME";
    case ARGUMENTS_ADAPTOR_FRAME:
      return "ARGUMENTS_ADAPTOR_FRAME";
    case CONSTRUCT_STUB_FRAME:
      return "CONSTRUCT_STUB_FRAME";
    case GETTER_STUB_FRAME:
      return "GETTER_STUB_FRAME";
    case SETTER_STUB_FRAME:
      return "SETTER_STUB_FRAME";
    case COMPILED_STUB_FRAME:
      return "COMPILED_STUB_FRAME";
    case REGISTER:
      return "REGISTER";
    case INT32_REGISTER:
      return "INT32_REGISTER";
    case UINT32_REGISTER:
      return "UINT32_REGISTER";
    case DOUBLE_REGISTER:
      return "DOUBLE_REGISTER";
    case STACK_SLOT:
      return "STACK_SLOT";
    case INT32_STACK_SLOT:
      return "INT32_STACK_SLOT";
    case UINT32_STACK_SLOT:
      return "UINT32_STACK_SLOT";
    case DOUBLE_STACK_SLOT:
      return "DOUBLE_STACK_SLOT";
    case LITERAL:
      return "LITERAL";
    case ARGUMENTS_OBJECT:
      return "ARGUMENTS_OBJECT";
    case DUPLICATE:
      return "DUPLICATE";
  }
  UNREACHABLE();
  return "";
}

#endif


DeoptimizingCodeListNode::DeoptimizingCodeListNode(Code* code): next_(NULL) {
  GlobalHandles* global_handles = code->GetIsolate()->global_handles();
  // Globalize the code object and make it weak.
  code_ = Handle<Code>::cast(global_handles->Create(code));
  global_handles->MakeWeak(reinterpret_cast<Object**>(code_.location()),
                           this,
                           Deoptimizer::HandleWeakDeoptimizedCode);
}


DeoptimizingCodeListNode::~DeoptimizingCodeListNode() {
  GlobalHandles* global_handles = code_->GetIsolate()->global_handles();
  global_handles->Destroy(reinterpret_cast<Object**>(code_.location()));
}


// We can't intermix stack decoding and allocations because
// deoptimization infrastracture is not GC safe.
// Thus we build a temporary structure in malloced space.
SlotRef SlotRef::ComputeSlotForNextArgument(TranslationIterator* iterator,
                                            DeoptimizationInputData* data,
                                            JavaScriptFrame* frame) {
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());

  switch (opcode) {
    case Translation::BEGIN:
    case Translation::JS_FRAME:
    case Translation::ARGUMENTS_ADAPTOR_FRAME:
    case Translation::CONSTRUCT_STUB_FRAME:
    case Translation::GETTER_STUB_FRAME:
    case Translation::SETTER_STUB_FRAME:
      // Peeled off before getting here.
      break;

    case Translation::ARGUMENTS_OBJECT:
      // This can be only emitted for local slots not for argument slots.
      break;

    case Translation::REGISTER:
    case Translation::INT32_REGISTER:
    case Translation::UINT32_REGISTER:
    case Translation::DOUBLE_REGISTER:
    case Translation::DUPLICATE:
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

  UNREACHABLE();
  return SlotRef();
}


void SlotRef::ComputeSlotsForArguments(Vector<SlotRef>* args_slots,
                                       TranslationIterator* it,
                                       DeoptimizationInputData* data,
                                       JavaScriptFrame* frame) {
  // Process the translation commands for the arguments.

  // Skip the translation command for the receiver.
  it->Skip(Translation::NumberOfOperandsFor(
      static_cast<Translation::Opcode>(it->Next())));

  // Compute slots for arguments.
  for (int i = 0; i < args_slots->length(); ++i) {
    (*args_slots)[i] = ComputeSlotForNextArgument(it, data, frame);
  }
}


Vector<SlotRef> SlotRef::ComputeSlotMappingForArguments(
    JavaScriptFrame* frame,
    int inlined_jsframe_index,
    int formal_parameter_count) {
  DisallowHeapAllocation no_gc;
  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationInputData* data =
      static_cast<OptimizedFrame*>(frame)->GetDeoptimizationData(&deopt_index);
  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  ASSERT(opcode == Translation::BEGIN);
  it.Next();  // Drop frame count.
  int jsframe_count = it.Next();
  USE(jsframe_count);
  ASSERT(jsframe_count > inlined_jsframe_index);
  int jsframes_to_skip = inlined_jsframe_index;
  while (true) {
    opcode = static_cast<Translation::Opcode>(it.Next());
    if (opcode == Translation::ARGUMENTS_ADAPTOR_FRAME) {
      if (jsframes_to_skip == 0) {
        ASSERT(Translation::NumberOfOperandsFor(opcode) == 2);

        it.Skip(1);  // literal id
        int height = it.Next();

        // We reached the arguments adaptor frame corresponding to the
        // inlined function in question.  Number of arguments is height - 1.
        Vector<SlotRef> args_slots =
            Vector<SlotRef>::New(height - 1);  // Minus receiver.
        ComputeSlotsForArguments(&args_slots, &it, data, frame);
        return args_slots;
      }
    } else if (opcode == Translation::JS_FRAME) {
      if (jsframes_to_skip == 0) {
        // Skip over operands to advance to the next opcode.
        it.Skip(Translation::NumberOfOperandsFor(opcode));

        // We reached the frame corresponding to the inlined function
        // in question.  Process the translation commands for the
        // arguments.  Number of arguments is equal to the number of
        // format parameter count.
        Vector<SlotRef> args_slots =
            Vector<SlotRef>::New(formal_parameter_count);
        ComputeSlotsForArguments(&args_slots, &it, data, frame);
        return args_slots;
      }
      jsframes_to_skip--;
    }

    // Skip over operands to advance to the next opcode.
    it.Skip(Translation::NumberOfOperandsFor(opcode));
  }

  UNREACHABLE();
  return Vector<SlotRef>();
}

#ifdef ENABLE_DEBUGGER_SUPPORT

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
  Code* code = Code::cast(deoptimizer->isolate()->heap()->FindCodeObject(pc));
  source_position_ = code->SourcePosition(pc);

  for (int i = 0; i < expression_count_; i++) {
    SetExpression(i, output_frame->GetExpression(i));
  }

  if (has_arguments_adaptor) {
    output_frame = deoptimizer->output_[frame_index - 1];
    ASSERT(output_frame->GetFrameType() == StackFrame::ARGUMENTS_ADAPTOR);
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

#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal
