// Copyright 2011 the V8 project authors. All rights reserved.
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
    eager_deoptimization_entry_code_->Free(EXECUTABLE);
    eager_deoptimization_entry_code_ = NULL;
  }
  if (lazy_deoptimization_entry_code_ != NULL) {
    lazy_deoptimization_entry_code_->Free(EXECUTABLE);
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

#ifdef ENABLE_DEBUGGER_SUPPORT
DeoptimizedFrameInfo* Deoptimizer::DebuggerInspectableFrame(
    JavaScriptFrame* frame,
    int frame_index,
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
  ASSERT_LT(frame_index, deoptimizer->output_count());
  DeoptimizedFrameInfo* info =
      new DeoptimizedFrameInfo(deoptimizer, frame_index);
  isolate->deoptimizer_data()->deoptimized_frame_info_ = info;

  // Get the "simulated" top and size for the requested frame.
  Address top =
      reinterpret_cast<Address>(deoptimizer->output_[frame_index]->GetTop());
  uint32_t size = deoptimizer->output_[frame_index]->GetFrameSize();

  // Done with the GC-unsafe frame descriptions. This re-enables allocation.
  deoptimizer->DeleteFrameDescriptions();

  // Allocate a heap number for the doubles belonging to this frame.
  deoptimizer->MaterializeHeapNumbersForDebuggerInspectableFrame(
      top, size, info);

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
  AssertNoAllocation no_allocation;

  ASSERT(context->IsGlobalContext());

  visitor->EnterContext(context);
  // Run through the list of optimized functions and deoptimize them.
  Object* element = context->OptimizedFunctionsListHead();
  while (!element->IsUndefined()) {
    JSFunction* element_function = JSFunction::cast(element);
    // Get the next link before deoptimizing as deoptimizing will clear the
    // next link.
    element = element_function->next_function_link();
    visitor->VisitFunction(element_function);
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
        GlobalObject::cast(proto)->global_context(), visitor);
  } else if (object->IsGlobalObject()) {
    VisitAllOptimizedFunctionsForContext(
        GlobalObject::cast(object)->global_context(), visitor);
  }
}


void Deoptimizer::VisitAllOptimizedFunctions(
    OptimizedFunctionVisitor* visitor) {
  AssertNoAllocation no_allocation;

  // Run through the list of all global contexts and deoptimize.
  Object* global = Isolate::Current()->heap()->global_contexts_list();
  while (!global->IsUndefined()) {
    VisitAllOptimizedFunctionsForGlobalObject(Context::cast(global)->global(),
                                              visitor);
    global = Context::cast(global)->get(Context::NEXT_CONTEXT_LINK);
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
      input_(NULL),
      output_count_(0),
      output_(NULL),
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
  // Find the optimized code.
  if (type == EAGER) {
    ASSERT(from == NULL);
    optimized_code_ = function_->code();
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
#ifdef DEBUG
  input_->SetKind(Code::OPTIMIZED_FUNCTION);
#endif
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
  LargeObjectChunk* base = NULL;
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
      static_cast<Address>(base->GetStartAddress()) + (id * table_entry_size_);
}


int Deoptimizer::GetDeoptimizationId(Address addr, BailoutType type) {
  LargeObjectChunk* base = NULL;
  DeoptimizerData* data = Isolate::Current()->deoptimizer_data();
  if (type == EAGER) {
    base = data->eager_deoptimization_entry_code_;
  } else {
    base = data->lazy_deoptimization_entry_code_;
  }
  if (base == NULL ||
      addr < base->GetStartAddress() ||
      addr >= base->GetStartAddress() +
          (kNumberOfEntries * table_entry_size_)) {
    return kNotDeoptimizationEntry;
  }
  ASSERT_EQ(0,
      static_cast<int>(addr - base->GetStartAddress()) % table_entry_size_);
  return static_cast<int>(addr - base->GetStartAddress()) / table_entry_size_;
}


int Deoptimizer::GetOutputInfo(DeoptimizationOutputData* data,
                               unsigned id,
                               SharedFunctionInfo* shared) {
  // TODO(kasperl): For now, we do a simple linear search for the PC
  // offset associated with the given node id. This should probably be
  // changed to a binary search.
  int length = data->DeoptPoints();
  Smi* smi_id = Smi::FromInt(id);
  for (int i = 0; i < length; i++) {
    if (data->AstId(i) == smi_id) {
      return data->PcAndState(i)->value();
    }
  }
  PrintF("[couldn't find pc offset for node=%u]\n", id);
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
  unsigned node_id = input_data->AstId(bailout_id_)->value();
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
  ASSERT(output_ == NULL);
  output_ = new FrameDescription*[count];
  for (int i = 0; i < count; ++i) {
    output_[i] = NULL;
  }
  output_count_ = count;

  // Translate each output frame.
  for (int i = 0; i < count; ++i) {
    DoComputeFrame(&iterator, i);
  }

  // Print some helpful diagnostic information.
  if (FLAG_trace_deopt) {
    double ms = static_cast<double>(OS::Ticks() - start) / 1000;
    int index = output_count_ - 1;  // Index of the topmost frame.
    JSFunction* function = output_[index]->GetFunction();
    PrintF("[deoptimizing: end 0x%08" V8PRIxPTR " ",
           reinterpret_cast<intptr_t>(function));
    function->PrintName();
    PrintF(" => node=%u, pc=0x%08" V8PRIxPTR ", state=%s, took %0.3f ms]\n",
           node_id,
           output_[index]->GetPc(),
           FullCodeGenerator::State2String(
               static_cast<FullCodeGenerator::State>(
                   output_[index]->GetState()->value())),
           ms);
  }
}


void Deoptimizer::MaterializeHeapNumbers() {
  ASSERT_NE(DEBUGGER, bailout_type_);
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
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void Deoptimizer::MaterializeHeapNumbersForDebuggerInspectableFrame(
    Address top, uint32_t size, DeoptimizedFrameInfo* info) {
  ASSERT_EQ(DEBUGGER, bailout_type_);
  for (int i = 0; i < deferred_heap_numbers_.length(); i++) {
    HeapNumberMaterializationDescriptor d = deferred_heap_numbers_[i];

    // Check of the heap number to materialize actually belong to the frame
    // being extracted.
    Address slot = d.slot_address();
    if (top <= slot && slot < top + size) {
      Handle<Object> num = isolate_->factory()->NewNumber(d.value());
      // Calculate the index with the botton of the expression stack
      // at index 0, and the fixed part (including incoming arguments)
      // at negative indexes.
      int index = static_cast<int>(
          info->expression_count_ - (slot - top) / kPointerSize - 1);
      if (FLAG_trace_deopt) {
        PrintF("Materializing a new heap number %p [%e] in slot %p"
               "for stack index %d\n",
               reinterpret_cast<void*>(*num),
               d.value(),
               d.slot_address(),
               index);
      }
      if (index >=0) {
        info->SetExpression(index, *num);
      } else {
        // Calculate parameter index subtracting one for the receiver.
        int parameter_index =
            index +
            static_cast<int>(size) / kPointerSize -
            info->expression_count_ - 1;
        info->SetParameter(parameter_index, *num);
      }
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
    case Translation::FRAME:
    case Translation::DUPLICATE:
      UNREACHABLE();
      return;

    case Translation::REGISTER: {
      int input_reg = iterator->Next();
      intptr_t input_value = input_->GetRegister(input_reg);
      if (FLAG_trace_deopt) {
        PrintF(
            "    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08" V8PRIxPTR " ; %s\n",
            output_[frame_index]->GetTop() + output_offset,
            output_offset,
            input_value,
            converter.NameOfCPURegister(input_reg));
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
          input_->GetOffsetFromSlotIndex(this, input_slot_index);
      intptr_t input_value = input_->GetFrameSlot(input_offset);
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF("[top + %d] <- 0x%08" V8PRIxPTR " ; [esp + %d]\n",
               output_offset,
               input_value,
               input_offset);
      }
      output_[frame_index]->SetFrameSlot(output_offset, input_value);
      return;
    }

    case Translation::INT32_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(this, input_slot_index);
      intptr_t value = input_->GetFrameSlot(input_offset);
      bool is_smi = Smi::IsValid(value);
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": ",
               output_[frame_index]->GetTop() + output_offset);
        PrintF("[top + %d] <- %" V8PRIdPTR " ; [esp + %d] (%s)\n",
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

    case Translation::DOUBLE_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(this, input_slot_index);
      double value = input_->GetDoubleFrameSlot(input_offset);
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- %e ; [esp + %d]\n",
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
      // Use the arguments marker value as a sentinel and fill in the arguments
      // object after the deoptimized frame is built.
      ASSERT(frame_index == 0);  // Only supported for first frame.
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- ",
               output_[frame_index]->GetTop() + output_offset,
               output_offset);
        isolate_->heap()->arguments_marker()->ShortPrint();
        PrintF(" ; arguments object\n");
      }
      intptr_t value = reinterpret_cast<intptr_t>(
          isolate_->heap()->arguments_marker());
      output_[frame_index]->SetFrameSlot(output_offset, value);
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
    case Translation::FRAME:
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
      // Abort OSR if we don't have a number.
      if (!input_object->IsNumber()) return false;

      int output_reg = iterator->Next();
      int int32_value = input_object->IsSmi()
          ? Smi::cast(input_object)->value()
          : FastD2I(input_object->Number());
      // Abort the translation if the conversion lost information.
      if (!input_object->IsSmi() &&
          FastI2D(int32_value) != input_object->Number()) {
        if (FLAG_trace_osr) {
          PrintF("**** %g could not be converted to int32 ****\n",
                 input_object->Number());
        }
        return false;
      }
      if (FLAG_trace_osr) {
        PrintF("    %s <- %d (int32) ; [sp + %d]\n",
               converter.NameOfCPURegister(output_reg),
               int32_value,
               *input_offset);
      }
      output->SetRegister(output_reg, int32_value);
      break;
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
          output->GetOffsetFromSlotIndex(this, output_index);
      if (FLAG_trace_osr) {
        PrintF("    [sp + %d] <- 0x%08" V8PRIxPTR " ; [sp + %d]\n",
               output_offset,
               input_value,
               *input_offset);
      }
      output->SetFrameSlot(output_offset, input_value);
      break;
    }

    case Translation::INT32_STACK_SLOT: {
      // Abort OSR if we don't have a number.
      if (!input_object->IsNumber()) return false;

      int output_index = iterator->Next();
      unsigned output_offset =
          output->GetOffsetFromSlotIndex(this, output_index);
      int int32_value = input_object->IsSmi()
          ? Smi::cast(input_object)->value()
          : DoubleToInt32(input_object->Number());
      // Abort the translation if the conversion lost information.
      if (!input_object->IsSmi() &&
          FastI2D(int32_value) != input_object->Number()) {
        if (FLAG_trace_osr) {
          PrintF("**** %g could not be converted to int32 ****\n",
                 input_object->Number());
        }
        return false;
      }
      if (FLAG_trace_osr) {
        PrintF("    [sp + %d] <- %d (int32) ; [sp + %d]\n",
               output_offset,
               int32_value,
               *input_offset);
      }
      output->SetFrameSlot(output_offset, int32_value);
      break;
    }

    case Translation::DOUBLE_STACK_SLOT: {
      static const int kLowerOffset = 0 * kPointerSize;
      static const int kUpperOffset = 1 * kPointerSize;

      // Abort OSR if we don't have a number.
      if (!input_object->IsNumber()) return false;

      int output_index = iterator->Next();
      unsigned output_offset =
          output->GetOffsetFromSlotIndex(this, output_index);
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
    PatchStackCheckCodeAt(pc_after, check_code, replacement_code);
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
    RevertStackCheckCodeAt(pc_after, check_code, replacement_code);
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
  static const unsigned kFixedSlotSize = 4 * kPointerSize;
  return ComputeIncomingArgumentSize(function) + kFixedSlotSize;
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


void Deoptimizer::AddDoubleValue(intptr_t slot_address,
                                 double value) {
  HeapNumberMaterializationDescriptor value_desc(
      reinterpret_cast<Address>(slot_address), value);
  deferred_heap_numbers_.Add(value_desc);
}


LargeObjectChunk* Deoptimizer::CreateCode(BailoutType type) {
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

  LargeObjectChunk* chunk = LargeObjectChunk::New(desc.instr_size, EXECUTABLE);
  if (chunk == NULL) {
    V8::FatalProcessOutOfMemory("Not enough memory for deoptimization table");
  }
  memcpy(chunk->GetStartAddress(), desc.buffer, desc.instr_size);
  CPU::FlushICache(chunk->GetStartAddress(), desc.instr_size);
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


FrameDescription::FrameDescription(uint32_t frame_size,
                                   JSFunction* function)
    : frame_size_(frame_size),
      function_(function),
      top_(kZapUint32),
      pc_(kZapUint32),
      fp_(kZapUint32) {
  // Zap all the registers.
  for (int r = 0; r < Register::kNumRegisters; r++) {
    SetRegister(r, kZapUint32);
  }

  // Zap all the slots.
  for (unsigned o = 0; o < frame_size; o += kPointerSize) {
    SetFrameSlot(o, kZapUint32);
  }
}


unsigned FrameDescription::GetOffsetFromSlotIndex(Deoptimizer* deoptimizer,
                                                  int slot_index) {
  if (slot_index >= 0) {
    // Local or spill slots. Skip the fixed part of the frame
    // including all arguments.
    unsigned base =
        GetFrameSize() - deoptimizer->ComputeFixedSize(GetFunction());
    return base - ((slot_index + 1) * kPointerSize);
  } else {
    // Incoming parameter.
    unsigned base = GetFrameSize() -
        deoptimizer->ComputeIncomingArgumentSize(GetFunction());
    return base - ((slot_index + 1) * kPointerSize);
  }
}


int FrameDescription::ComputeParametersCount() {
  return function_->shared()->formal_parameter_count();
}


Object* FrameDescription::GetParameter(Deoptimizer* deoptimizer, int index) {
  ASSERT_EQ(Code::FUNCTION, kind_);
  ASSERT(index >= 0);
  ASSERT(index < ComputeParametersCount());
  // The slot indexes for incoming arguments are negative.
  unsigned offset = GetOffsetFromSlotIndex(deoptimizer,
                                           index - ComputeParametersCount());
  return reinterpret_cast<Object*>(*GetFrameSlotPointer(offset));
}


unsigned FrameDescription::GetExpressionCount(Deoptimizer* deoptimizer) {
  ASSERT_EQ(Code::FUNCTION, kind_);
  unsigned size = GetFrameSize() - deoptimizer->ComputeFixedSize(GetFunction());
  return size / kPointerSize;
}


Object* FrameDescription::GetExpression(Deoptimizer* deoptimizer, int index) {
  ASSERT_EQ(Code::FUNCTION, kind_);
  unsigned offset = GetOffsetFromSlotIndex(deoptimizer, index);
  return reinterpret_cast<Object*>(*GetFrameSlotPointer(offset));
}


void TranslationBuffer::Add(int32_t value) {
  // Encode the sign bit in the least significant bit.
  bool is_negative = (value < 0);
  uint32_t bits = ((is_negative ? -value : value) << 1) |
      static_cast<int32_t>(is_negative);
  // Encode the individual bytes using the least significant bit of
  // each byte to indicate whether or not more bytes follow.
  do {
    uint32_t next = bits >> 7;
    contents_.Add(((bits << 1) & 0xFF) | (next != 0));
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


void Translation::BeginFrame(int node_id, int literal_id, unsigned height) {
  buffer_->Add(FRAME);
  buffer_->Add(node_id);
  buffer_->Add(literal_id);
  buffer_->Add(height);
}


void Translation::StoreRegister(Register reg) {
  buffer_->Add(REGISTER);
  buffer_->Add(reg.code());
}


void Translation::StoreInt32Register(Register reg) {
  buffer_->Add(INT32_REGISTER);
  buffer_->Add(reg.code());
}


void Translation::StoreDoubleRegister(DoubleRegister reg) {
  buffer_->Add(DOUBLE_REGISTER);
  buffer_->Add(DoubleRegister::ToAllocationIndex(reg));
}


void Translation::StoreStackSlot(int index) {
  buffer_->Add(STACK_SLOT);
  buffer_->Add(index);
}


void Translation::StoreInt32StackSlot(int index) {
  buffer_->Add(INT32_STACK_SLOT);
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


void Translation::StoreArgumentsObject() {
  buffer_->Add(ARGUMENTS_OBJECT);
}


void Translation::MarkDuplicate() {
  buffer_->Add(DUPLICATE);
}


int Translation::NumberOfOperandsFor(Opcode opcode) {
  switch (opcode) {
    case ARGUMENTS_OBJECT:
    case DUPLICATE:
      return 0;
    case BEGIN:
    case REGISTER:
    case INT32_REGISTER:
    case DOUBLE_REGISTER:
    case STACK_SLOT:
    case INT32_STACK_SLOT:
    case DOUBLE_STACK_SLOT:
    case LITERAL:
      return 1;
    case FRAME:
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
    case FRAME:
      return "FRAME";
    case REGISTER:
      return "REGISTER";
    case INT32_REGISTER:
      return "INT32_REGISTER";
    case DOUBLE_REGISTER:
      return "DOUBLE_REGISTER";
    case STACK_SLOT:
      return "STACK_SLOT";
    case INT32_STACK_SLOT:
      return "INT32_STACK_SLOT";
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
    case Translation::FRAME:
      // Peeled off before getting here.
      break;

    case Translation::ARGUMENTS_OBJECT:
      // This can be only emitted for local slots not for argument slots.
      break;

    case Translation::REGISTER:
    case Translation::INT32_REGISTER:
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


void SlotRef::ComputeSlotMappingForArguments(JavaScriptFrame* frame,
                                             int inlined_frame_index,
                                             Vector<SlotRef>* args_slots) {
  AssertNoAllocation no_gc;
  int deopt_index = AstNode::kNoNumber;
  DeoptimizationInputData* data =
      static_cast<OptimizedFrame*>(frame)->GetDeoptimizationData(&deopt_index);
  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  ASSERT(opcode == Translation::BEGIN);
  int frame_count = it.Next();
  USE(frame_count);
  ASSERT(frame_count > inlined_frame_index);
  int frames_to_skip = inlined_frame_index;
  while (true) {
    opcode = static_cast<Translation::Opcode>(it.Next());
    // Skip over operands to advance to the next opcode.
    it.Skip(Translation::NumberOfOperandsFor(opcode));
    if (opcode == Translation::FRAME) {
      if (frames_to_skip == 0) {
        // We reached the frame corresponding to the inlined function
        // in question.  Process the translation commands for the
        // arguments.
        //
        // Skip the translation command for the receiver.
        it.Skip(Translation::NumberOfOperandsFor(
            static_cast<Translation::Opcode>(it.Next())));
        // Compute slots for arguments.
        for (int i = 0; i < args_slots->length(); ++i) {
          (*args_slots)[i] = ComputeSlotForNextArgument(&it, data, frame);
        }
        return;
      }
      frames_to_skip--;
    }
  }

  UNREACHABLE();
}

#ifdef ENABLE_DEBUGGER_SUPPORT

DeoptimizedFrameInfo::DeoptimizedFrameInfo(
    Deoptimizer* deoptimizer, int frame_index) {
  FrameDescription* output_frame = deoptimizer->output_[frame_index];
  SetFunction(output_frame->GetFunction());
  expression_count_ = output_frame->GetExpressionCount(deoptimizer);
  parameters_count_ = output_frame->ComputeParametersCount();
  parameters_ = new Object*[parameters_count_];
  for (int i = 0; i < parameters_count_; i++) {
    SetParameter(i, output_frame->GetParameter(deoptimizer, i));
  }
  expression_stack_ = new Object*[expression_count_];
  for (int i = 0; i < expression_count_; i++) {
    SetExpression(i, output_frame->GetExpression(deoptimizer, i));
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
