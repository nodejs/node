// Copyright 2012 the V8 project authors. All rights reserved.
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

DeoptimizerData::DeoptimizerData() {
  eager_deoptimization_entry_code_ = NULL;
  lazy_deoptimization_entry_code_ = NULL;
  current_ = NULL;
  deoptimizing_code_list_ = NULL;
#ifdef ENABLE_DEBUGGER_SUPPORT
  deoptimized_frame_info_ = NULL;
#endif
}


DeoptimizerData::~DeoptimizerData() {
  if (eager_deoptimization_entry_code_ != NULL) {
    Isolate::Current()->memory_allocator()->Free(
        eager_deoptimization_entry_code_);
    eager_deoptimization_entry_code_ = NULL;
  }
  if (lazy_deoptimization_entry_code_ != NULL) {
    Isolate::Current()->memory_allocator()->Free(
        lazy_deoptimization_entry_code_);
    lazy_deoptimization_entry_code_ = NULL;
  }
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void DeoptimizerData::Iterate(ObjectVisitor* v) {
  if (deoptimized_frame_info_ != NULL) {
    deoptimized_frame_info_->Iterate(v);
  }
}
#endif


// We rely on this function not causing a GC.  It is called from generated code
// without having a real stack frame in place.
Deoptimizer* Deoptimizer::New(JSFunction* function,
                              BailoutType type,
                              unsigned bailout_id,
                              Address from,
                              int fp_to_sp_delta,
                              Isolate* isolate) {
  ASSERT(isolate == Isolate::Current());
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


Deoptimizer* Deoptimizer::Grab(Isolate* isolate) {
  ASSERT(isolate == Isolate::Current());
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
  ASSERT(isolate == Isolate::Current());
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
  ASSERT(isolate == Isolate::Current());
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


class DeoptimizingVisitor : public OptimizedFunctionVisitor {
 public:
  virtual void EnterContext(Context* context) {
    if (FLAG_trace_deopt) {
      PrintF("[deoptimize context: %" V8PRIxPTR "]\n",
             reinterpret_cast<intptr_t>(context));
    }
  }

  virtual void VisitFunction(JSFunction* function) {
    Deoptimizer::DeoptimizeFunction(function);
  }

  virtual void LeaveContext(Context* context) {
    context->ClearOptimizedFunctions();
  }
};


void Deoptimizer::DeoptimizeAll() {
  AssertNoAllocation no_allocation;

  if (FLAG_trace_deopt) {
    PrintF("[deoptimize all contexts]\n");
  }

  DeoptimizingVisitor visitor;
  VisitAllOptimizedFunctions(&visitor);
}


void Deoptimizer::DeoptimizeGlobalObject(JSObject* object) {
  AssertNoAllocation no_allocation;

  DeoptimizingVisitor visitor;
  VisitAllOptimizedFunctionsForGlobalObject(object, &visitor);
}


void Deoptimizer::VisitAllOptimizedFunctionsForContext(
    Context* context, OptimizedFunctionVisitor* visitor) {
  Isolate* isolate = context->GetIsolate();
  ZoneScope zone_scope(isolate->runtime_zone(), DELETE_ON_EXIT);
  AssertNoAllocation no_allocation;

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


void Deoptimizer::VisitAllOptimizedFunctionsForGlobalObject(
    JSObject* object, OptimizedFunctionVisitor* visitor) {
  AssertNoAllocation no_allocation;

  if (object->IsJSGlobalProxy()) {
    Object* proto = object->GetPrototype();
    ASSERT(proto->IsJSGlobalObject());
    VisitAllOptimizedFunctionsForContext(
        GlobalObject::cast(proto)->native_context(), visitor);
  } else if (object->IsGlobalObject()) {
    VisitAllOptimizedFunctionsForContext(
        GlobalObject::cast(object)->native_context(), visitor);
  }
}


void Deoptimizer::VisitAllOptimizedFunctions(
    OptimizedFunctionVisitor* visitor) {
  AssertNoAllocation no_allocation;

  // Run through the list of all native contexts and deoptimize.
  Object* context = Isolate::Current()->heap()->native_contexts_list();
  while (!context->IsUndefined()) {
    // GC can happen when the context is not fully initialized,
    // so the global field of the context can be undefined.
    Object* global = Context::cast(context)->get(Context::GLOBAL_OBJECT_INDEX);
    if (!global->IsUndefined()) {
      VisitAllOptimizedFunctionsForGlobalObject(JSObject::cast(global),
                                                visitor);
    }
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


void Deoptimizer::HandleWeakDeoptimizedCode(
    v8::Persistent<v8::Value> obj, void* data) {
  DeoptimizingCodeListNode* node =
      reinterpret_cast<DeoptimizingCodeListNode*>(data);
  RemoveDeoptimizingCode(*node->code());
#ifdef DEBUG
  node = Isolate::Current()->deoptimizer_data()->deoptimizing_code_list_;
  while (node != NULL) {
    ASSERT(node != reinterpret_cast<DeoptimizingCodeListNode*>(data));
    node = node->next();
  }
#endif
}


void Deoptimizer::ComputeOutputFrames(Deoptimizer* deoptimizer) {
  deoptimizer->DoComputeOutputFrames();
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
      deferred_heap_numbers_(0) {
  if (FLAG_trace_deopt && type != OSR) {
    if (type == DEBUGGER) {
      PrintF("**** DEOPT FOR DEBUGGER: ");
    } else {
      PrintF("**** DEOPT: ");
    }
    function->PrintName();
    PrintF(" at bailout #%u, address 0x%" V8PRIxPTR ", frame size %d\n",
           bailout_id,
           reinterpret_cast<intptr_t>(from),
           fp_to_sp_delta - (2 * kPointerSize));
  } else if (FLAG_trace_osr && type == OSR) {
    PrintF("**** OSR: ");
    function->PrintName();
    PrintF(" at ast id #%u, address 0x%" V8PRIxPTR ", frame size %d\n",
           bailout_id,
           reinterpret_cast<intptr_t>(from),
           fp_to_sp_delta - (2 * kPointerSize));
  }
  function->shared()->increment_deopt_count();
  // Find the optimized code.
  if (type == EAGER) {
    ASSERT(from == NULL);
    optimized_code_ = function_->code();
    if (FLAG_trace_deopt && FLAG_code_comments) {
      // Print instruction associated with this bailout.
      const char* last_comment = NULL;
      int mask = RelocInfo::ModeMask(RelocInfo::COMMENT)
          | RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);
      for (RelocIterator it(optimized_code_, mask); !it.done(); it.next()) {
        RelocInfo* info = it.rinfo();
        if (info->rmode() == RelocInfo::COMMENT) {
          last_comment = reinterpret_cast<const char*>(info->data());
        }
        if (info->rmode() == RelocInfo::RUNTIME_ENTRY) {
          unsigned id = Deoptimizer::GetDeoptimizationId(
              info->target_address(), Deoptimizer::EAGER);
          if (id == bailout_id && last_comment != NULL) {
            PrintF("            %s\n", last_comment);
            break;
          }
        }
      }
    }
  } else if (type == LAZY) {
    optimized_code_ = FindDeoptimizingCodeFromAddress(from);
    ASSERT(optimized_code_ != NULL);
  } else if (type == OSR) {
    // The function has already been optimized and we're transitioning
    // from the unoptimized shared version to the optimized one in the
    // function. The return address (from) points to unoptimized code.
    optimized_code_ = function_->code();
    ASSERT(optimized_code_->kind() == Code::OPTIMIZED_FUNCTION);
    ASSERT(!optimized_code_->contains(from));
  } else if (type == DEBUGGER) {
    optimized_code_ = optimized_code;
    ASSERT(optimized_code_->contains(from));
  }
  ASSERT(HEAP->allow_allocation(false));
  unsigned size = ComputeInputFrameSize();
  input_ = new(size) FrameDescription(size, function);
  input_->SetFrameType(StackFrame::JAVA_SCRIPT);
}


Deoptimizer::~Deoptimizer() {
  ASSERT(input_ == NULL && output_ == NULL);
}


void Deoptimizer::DeleteFrameDescriptions() {
  delete input_;
  for (int i = 0; i < output_count_; ++i) {
    if (output_[i] != input_) delete output_[i];
  }
  delete[] output_;
  input_ = NULL;
  output_ = NULL;
  ASSERT(!HEAP->allow_allocation(true));
}


Address Deoptimizer::GetDeoptimizationEntry(int id, BailoutType type) {
  ASSERT(id >= 0);
  if (id >= kNumberOfEntries) return NULL;
  MemoryChunk* base = NULL;
  DeoptimizerData* data = Isolate::Current()->deoptimizer_data();
  if (type == EAGER) {
    if (data->eager_deoptimization_entry_code_ == NULL) {
      data->eager_deoptimization_entry_code_ = CreateCode(type);
    }
    base = data->eager_deoptimization_entry_code_;
  } else {
    if (data->lazy_deoptimization_entry_code_ == NULL) {
      data->lazy_deoptimization_entry_code_ = CreateCode(type);
    }
    base = data->lazy_deoptimization_entry_code_;
  }
  return
      static_cast<Address>(base->area_start()) + (id * table_entry_size_);
}


int Deoptimizer::GetDeoptimizationId(Address addr, BailoutType type) {
  MemoryChunk* base = NULL;
  DeoptimizerData* data = Isolate::Current()->deoptimizer_data();
  if (type == EAGER) {
    base = data->eager_deoptimization_entry_code_;
  } else {
    base = data->lazy_deoptimization_entry_code_;
  }
  if (base == NULL ||
      addr < base->area_start() ||
      addr >= base->area_start() +
          (kNumberOfEntries * table_entry_size_)) {
    return kNotDeoptimizationEntry;
  }
  ASSERT_EQ(0,
      static_cast<int>(addr - base->area_start()) % table_entry_size_);
  return static_cast<int>(addr - base->area_start()) / table_entry_size_;
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

  UNREACHABLE();
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
  if (FLAG_trace_deopt) {
    PrintF("[deoptimizing%s: begin 0x%08" V8PRIxPTR " ",
           (bailout_type_ == LAZY ? " (lazy)" : ""),
           reinterpret_cast<intptr_t>(function_));
    function_->PrintName();
    PrintF(" @%d]\n", bailout_id_);
  }

  // Determine basic deoptimization information.  The optimized frame is
  // described by the input data.
  DeoptimizationInputData* input_data =
      DeoptimizationInputData::cast(optimized_code_->deoptimization_data());
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
        UNREACHABLE();
        break;
    }
  }

  // Print some helpful diagnostic information.
  if (FLAG_trace_deopt) {
    double ms = static_cast<double>(OS::Ticks() - start) / 1000;
    int index = output_count_ - 1;  // Index of the topmost frame.
    JSFunction* function = output_[index]->GetFunction();
    PrintF("[deoptimizing: end 0x%08" V8PRIxPTR " ",
           reinterpret_cast<intptr_t>(function));
    function->PrintName();
    PrintF(" => node=%d, pc=0x%08" V8PRIxPTR ", state=%s, alignment=%s,"
           " took %0.3f ms]\n",
           node_id.ToInt(),
           output_[index]->GetPc(),
           FullCodeGenerator::State2String(
               static_cast<FullCodeGenerator::State>(
                   output_[index]->GetState()->value())),
           has_alignment_padding_ ? "with padding" : "no padding",
           ms);
  }
}


void Deoptimizer::MaterializeHeapObjects(JavaScriptFrameIterator* it) {
  ASSERT_NE(DEBUGGER, bailout_type_);

  // Handlify all argument object values before triggering any allocation.
  List<Handle<Object> > values(deferred_arguments_objects_values_.length());
  for (int i = 0; i < deferred_arguments_objects_values_.length(); ++i) {
    values.Add(Handle<Object>(deferred_arguments_objects_values_[i]));
  }

  // Play it safe and clear all unhandlified values before we continue.
  deferred_arguments_objects_values_.Clear();

  // Materialize all heap numbers before looking at arguments because when the
  // output frames are used to materialize arguments objects later on they need
  // to already contain valid heap numbers.
  for (int i = 0; i < deferred_heap_numbers_.length(); i++) {
    HeapNumberMaterializationDescriptor d = deferred_heap_numbers_[i];
    Handle<Object> num = isolate_->factory()->NewNumber(d.value());
    if (FLAG_trace_deopt) {
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
        if (FLAG_trace_deopt) {
          PrintF("Materializing %sarguments object for %p: ",
                 frame->has_adapted_arguments() ? "(adapted) " : "",
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

      if (FLAG_trace_deopt) {
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

      if (FLAG_trace_deopt) {
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


void Deoptimizer::DoTranslateCommand(TranslationIterator* iterator,
                                     int frame_index,
                                     unsigned output_offset) {
  disasm::NameConverter converter;
  // A GC-safe temporary placeholder that we can put in the output frame.
  const intptr_t kPlaceholder = reinterpret_cast<intptr_t>(Smi::FromInt(0));

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
    case Translation::DUPLICATE:
      UNREACHABLE();
      return;

    case Translation::REGISTER: {
      int input_reg = iterator->Next();
      intptr_t input_value = input_->GetRegister(input_reg);
      if (FLAG_trace_deopt) {
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
      bool is_smi = Smi::IsValid(value);
      if (FLAG_trace_deopt) {
        PrintF(
            "    0x%08" V8PRIxPTR ": [top + %d] <- %" V8PRIdPTR " ; %s (%s)\n",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            value,
            converter.NameOfCPURegister(input_reg),
            is_smi ? "smi" : "heap number");
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
      bool is_smi = (value <= static_cast<uintptr_t>(Smi::kMaxValue));
      if (FLAG_trace_deopt) {
        PrintF(
            "    0x%08" V8PRIxPTR ": [top + %d] <- %" V8PRIuPTR
            " ; uint %s (%s)\n",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            value,
            converter.NameOfCPURegister(input_reg),
            is_smi ? "smi" : "heap number");
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
      if (FLAG_trace_deopt) {
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
      if (FLAG_trace_deopt) {
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
      bool is_smi = Smi::IsValid(value);
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF("[top + %d] <- %" V8PRIdPTR " ; [sp + %d] (%s)\n",
               output_offset,
               value,
               input_offset,
               is_smi ? "smi" : "heap number");
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
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(input_slot_index);
      uintptr_t value =
          static_cast<uintptr_t>(input_->GetFrameSlot(input_offset));
      bool is_smi = (value <= static_cast<uintptr_t>(Smi::kMaxValue));
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF("[top + %d] <- %" V8PRIuPTR " ; [sp + %d] (uint32 %s)\n",
               output_offset,
               value,
               input_offset,
               is_smi ? "smi" : "heap number");
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
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(input_slot_index);
      double value = input_->GetDoubleFrameSlot(input_offset);
      if (FLAG_trace_deopt) {
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
      if (FLAG_trace_deopt) {
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
      int args_index = iterator->Next() + 1;  // Skip receiver.
      int args_length = iterator->Next() - 1;  // Skip receiver.
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- ",
               output_[frame_index]->GetTop() + output_offset,
               output_offset);
        isolate_->heap()->arguments_marker()->ShortPrint();
        PrintF(" ; arguments object\n");
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
        intptr_t input_value = input_->GetFrameSlot(input_offset);
        AddArgumentsObjectValue(input_value);
      }
      return;
    }
  }
}


static bool ObjectToInt32(Object* obj, int32_t* value) {
  if (obj->IsSmi()) {
    *value = Smi::cast(obj)->value();
    return true;
  }

  if (obj->IsHeapNumber()) {
    double num = HeapNumber::cast(obj)->value();
    if (FastI2D(FastD2I(num)) != num) {
      if (FLAG_trace_osr) {
        PrintF("**** %g could not be converted to int32 ****\n",
               HeapNumber::cast(obj)->value());
      }
      return false;
    }

    *value = FastD2I(num);
    return true;
  }

  return false;
}


static bool ObjectToUint32(Object* obj, uint32_t* value) {
  if (obj->IsSmi()) {
    if (Smi::cast(obj)->value() < 0) return false;

    *value = static_cast<uint32_t>(Smi::cast(obj)->value());
    return true;
  }

  if (obj->IsHeapNumber()) {
    double num = HeapNumber::cast(obj)->value();
    if ((num < 0) || (FastUI2D(FastD2UI(num)) != num)) {
      if (FLAG_trace_osr) {
        PrintF("**** %g could not be converted to uint32 ****\n",
               HeapNumber::cast(obj)->value());
      }
      return false;
    }

    *value = FastD2UI(num);
    return true;
  }

  return false;
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
      if (!ObjectToInt32(input_object, &int32_value)) return false;

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
      if (!ObjectToUint32(input_object, &uint32_value)) return false;

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
      if (!ObjectToInt32(input_object, &int32_value)) return false;

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
      if (!ObjectToUint32(input_object, &uint32_value)) return false;

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


void Deoptimizer::PatchStackCheckCode(Code* unoptimized_code,
                                      Code* check_code,
                                      Code* replacement_code) {
  // Iterate over the stack check table and patch every stack check
  // call to an unconditional call to the replacement code.
  ASSERT(unoptimized_code->kind() == Code::FUNCTION);
  Address stack_check_cursor = unoptimized_code->instruction_start() +
      unoptimized_code->stack_check_table_offset();
  uint32_t table_length = Memory::uint32_at(stack_check_cursor);
  stack_check_cursor += kIntSize;
  for (uint32_t i = 0; i < table_length; ++i) {
    uint32_t pc_offset = Memory::uint32_at(stack_check_cursor + kIntSize);
    Address pc_after = unoptimized_code->instruction_start() + pc_offset;
    PatchStackCheckCodeAt(unoptimized_code,
                          pc_after,
                          check_code,
                          replacement_code);
    stack_check_cursor += 2 * kIntSize;
  }
}


void Deoptimizer::RevertStackCheckCode(Code* unoptimized_code,
                                       Code* check_code,
                                       Code* replacement_code) {
  // Iterate over the stack check table and revert the patched
  // stack check calls.
  ASSERT(unoptimized_code->kind() == Code::FUNCTION);
  Address stack_check_cursor = unoptimized_code->instruction_start() +
      unoptimized_code->stack_check_table_offset();
  uint32_t table_length = Memory::uint32_at(stack_check_cursor);
  stack_check_cursor += kIntSize;
  for (uint32_t i = 0; i < table_length; ++i) {
    uint32_t pc_offset = Memory::uint32_at(stack_check_cursor + kIntSize);
    Address pc_after = unoptimized_code->instruction_start() + pc_offset;
    RevertStackCheckCodeAt(unoptimized_code,
                           pc_after,
                           check_code,
                           replacement_code);
    stack_check_cursor += 2 * kIntSize;
  }
}


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
  } else {
    unsigned stack_slots = optimized_code_->stack_slots();
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
  unsigned arguments = function->shared()->formal_parameter_count() + 1;
  return arguments * kPointerSize;
}


unsigned Deoptimizer::ComputeOutgoingArgumentSize() const {
  DeoptimizationInputData* data = DeoptimizationInputData::cast(
      optimized_code_->deoptimization_data());
  unsigned height = data->ArgumentsStackHeight(bailout_id_)->value();
  return height * kPointerSize;
}


Object* Deoptimizer::ComputeLiteral(int index) const {
  DeoptimizationInputData* data = DeoptimizationInputData::cast(
      optimized_code_->deoptimization_data());
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


MemoryChunk* Deoptimizer::CreateCode(BailoutType type) {
  // We cannot run this if the serializer is enabled because this will
  // cause us to emit relocation information for the external
  // references. This is fine because the deoptimizer's code section
  // isn't meant to be serialized at all.
  ASSERT(!Serializer::enabled());

  MacroAssembler masm(Isolate::Current(), NULL, 16 * KB);
  masm.set_emit_debug_code(false);
  GenerateDeoptimizationEntries(&masm, kNumberOfEntries, type);
  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(desc.reloc_size == 0);

  MemoryChunk* chunk =
      Isolate::Current()->memory_allocator()->AllocateChunk(desc.instr_size,
                                                            EXECUTABLE,
                                                            NULL);
  ASSERT(chunk->area_size() >= desc.instr_size);
  if (chunk == NULL) {
    V8::FatalProcessOutOfMemory("Not enough memory for deoptimization table");
  }
  memcpy(chunk->area_start(), desc.buffer, desc.instr_size);
  CPU::FlushICache(chunk->area_start(), desc.instr_size);
  return chunk;
}


Code* Deoptimizer::FindDeoptimizingCodeFromAddress(Address addr) {
  DeoptimizingCodeListNode* node =
      Isolate::Current()->deoptimizer_data()->deoptimizing_code_list_;
  while (node != NULL) {
    if (node->code()->contains(addr)) return *node->code();
    node = node->next();
  }
  return NULL;
}


void Deoptimizer::RemoveDeoptimizingCode(Code* code) {
  DeoptimizerData* data = Isolate::Current()->deoptimizer_data();
  ASSERT(data->deoptimizing_code_list_ != NULL);
  // Run through the code objects to find this one and remove it.
  DeoptimizingCodeListNode* prev = NULL;
  DeoptimizingCodeListNode* current = data->deoptimizing_code_list_;
  while (current != NULL) {
    if (*current->code() == code) {
      // Unlink from list. If prev is NULL we are looking at the first element.
      if (prev == NULL) {
        data->deoptimizing_code_list_ = current->next();
      } else {
        prev->set_next(current->next());
      }
      delete current;
      return;
    }
    // Move to next in list.
    prev = current;
    current = current->next();
  }
  // Deoptimizing code is removed through weak callback. Each object is expected
  // to be removed once and only once.
  UNREACHABLE();
}


static Object* CutOutRelatedFunctionsList(Context* context,
                                          Code* code,
                                          Object* undefined) {
  Object* result_list_head = undefined;
  Object* head;
  Object* current;
  current = head = context->get(Context::OPTIMIZED_FUNCTIONS_LIST);
  JSFunction* prev = NULL;
  while (current != undefined) {
    JSFunction* func = JSFunction::cast(current);
    current = func->next_function_link();
    if (func->code() == code) {
      func->set_next_function_link(result_list_head);
      result_list_head = func;
      if (prev) {
        prev->set_next_function_link(current);
      } else {
        head = current;
      }
    } else {
      prev = func;
    }
  }
  if (head != context->get(Context::OPTIMIZED_FUNCTIONS_LIST)) {
    context->set(Context::OPTIMIZED_FUNCTIONS_LIST, head);
  }
  return result_list_head;
}


void Deoptimizer::ReplaceCodeForRelatedFunctions(JSFunction* function,
                                                 Code* code) {
  Context* context = function->context()->native_context();

  SharedFunctionInfo* shared = function->shared();

  Object* undefined = Isolate::Current()->heap()->undefined_value();
  Object* current = CutOutRelatedFunctionsList(context, code, undefined);

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


Handle<ByteArray> TranslationBuffer::CreateByteArray() {
  int length = contents_.length();
  Handle<ByteArray> result =
      Isolate::Current()->factory()->NewByteArray(length, TENURED);
  memcpy(result->GetDataStartAddress(), contents_.ToVector().start(), length);
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


void Translation::StoreArgumentsObject(int args_index, int args_length) {
  buffer_->Add(ARGUMENTS_OBJECT, zone());
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
      return 1;
    case BEGIN:
    case ARGUMENTS_ADAPTOR_FRAME:
    case CONSTRUCT_STUB_FRAME:
    case ARGUMENTS_OBJECT:
      return 2;
    case JS_FRAME:
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
  GlobalHandles* global_handles = Isolate::Current()->global_handles();
  // Globalize the code object and make it weak.
  code_ = Handle<Code>::cast(global_handles->Create(code));
  global_handles->MakeWeak(reinterpret_cast<Object**>(code_.location()),
                           this,
                           Deoptimizer::HandleWeakDeoptimizedCode);
}


DeoptimizingCodeListNode::~DeoptimizingCodeListNode() {
  GlobalHandles* global_handles = Isolate::Current()->global_handles();
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
      return SlotRef(data->LiteralArray()->get(literal_index));
    }
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
  AssertNoAllocation no_gc;
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
  Code* code = Code::cast(Isolate::Current()->heap()->FindCodeObject(pc));
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
