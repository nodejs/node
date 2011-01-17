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

LargeObjectChunk* Deoptimizer::eager_deoptimization_entry_code_ = NULL;
LargeObjectChunk* Deoptimizer::lazy_deoptimization_entry_code_ = NULL;
Deoptimizer* Deoptimizer::current_ = NULL;
DeoptimizingCodeListNode* Deoptimizer::deoptimizing_code_list_ = NULL;


Deoptimizer* Deoptimizer::New(JSFunction* function,
                              BailoutType type,
                              unsigned bailout_id,
                              Address from,
                              int fp_to_sp_delta) {
  Deoptimizer* deoptimizer =
      new Deoptimizer(function, type, bailout_id, from, fp_to_sp_delta);
  ASSERT(current_ == NULL);
  current_ = deoptimizer;
  return deoptimizer;
}


Deoptimizer* Deoptimizer::Grab() {
  Deoptimizer* result = current_;
  ASSERT(result != NULL);
  result->DeleteFrameDescriptions();
  current_ = NULL;
  return result;
}


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
  Object* global = Heap::global_contexts_list();
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
  node = Deoptimizer::deoptimizing_code_list_;
  while (node != NULL) {
    ASSERT(node != reinterpret_cast<DeoptimizingCodeListNode*>(data));
    node = node->next();
  }
#endif
}


void Deoptimizer::ComputeOutputFrames(Deoptimizer* deoptimizer) {
  deoptimizer->DoComputeOutputFrames();
}


Deoptimizer::Deoptimizer(JSFunction* function,
                         BailoutType type,
                         unsigned bailout_id,
                         Address from,
                         int fp_to_sp_delta)
    : function_(function),
      bailout_id_(bailout_id),
      bailout_type_(type),
      from_(from),
      fp_to_sp_delta_(fp_to_sp_delta),
      output_count_(0),
      output_(NULL),
      integer32_values_(NULL),
      double_values_(NULL) {
  if (FLAG_trace_deopt && type != OSR) {
    PrintF("**** DEOPT: ");
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
  }
  ASSERT(Heap::allow_allocation(false));
  unsigned size = ComputeInputFrameSize();
  input_ = new(size) FrameDescription(size, function);
}


Deoptimizer::~Deoptimizer() {
  ASSERT(input_ == NULL && output_ == NULL);
  delete[] integer32_values_;
  delete[] double_values_;
}


void Deoptimizer::DeleteFrameDescriptions() {
  delete input_;
  for (int i = 0; i < output_count_; ++i) {
    if (output_[i] != input_) delete output_[i];
  }
  delete[] output_;
  input_ = NULL;
  output_ = NULL;
  ASSERT(!Heap::allow_allocation(true));
}


Address Deoptimizer::GetDeoptimizationEntry(int id, BailoutType type) {
  ASSERT(id >= 0);
  if (id >= kNumberOfEntries) return NULL;
  LargeObjectChunk* base = NULL;
  if (type == EAGER) {
    if (eager_deoptimization_entry_code_ == NULL) {
      eager_deoptimization_entry_code_ = CreateCode(type);
    }
    base = eager_deoptimization_entry_code_;
  } else {
    if (lazy_deoptimization_entry_code_ == NULL) {
      lazy_deoptimization_entry_code_ = CreateCode(type);
    }
    base = lazy_deoptimization_entry_code_;
  }
  return
      static_cast<Address>(base->GetStartAddress()) + (id * table_entry_size_);
}


int Deoptimizer::GetDeoptimizationId(Address addr, BailoutType type) {
  LargeObjectChunk* base = NULL;
  if (type == EAGER) {
    base = eager_deoptimization_entry_code_;
  } else {
    base = lazy_deoptimization_entry_code_;
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


void Deoptimizer::Setup() {
  // Do nothing yet.
}


void Deoptimizer::TearDown() {
  if (eager_deoptimization_entry_code_ != NULL) {
    eager_deoptimization_entry_code_->Free(EXECUTABLE);
    eager_deoptimization_entry_code_ = NULL;
  }
  if (lazy_deoptimization_entry_code_ != NULL) {
    lazy_deoptimization_entry_code_->Free(EXECUTABLE);
    lazy_deoptimization_entry_code_ = NULL;
  }
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


int Deoptimizer::GetDeoptimizedCodeCount() {
  int length = 0;
  DeoptimizingCodeListNode* node = Deoptimizer::deoptimizing_code_list_;
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
  // Per-frame lists of untagged and unboxed int32 and double values.
  integer32_values_ = new List<ValueDescriptionInteger32>[count];
  double_values_ = new List<ValueDescriptionDouble>[count];
  for (int i = 0; i < count; ++i) {
    output_[i] = NULL;
    integer32_values_[i].Initialize(0);
    double_values_[i].Initialize(0);
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


void Deoptimizer::InsertHeapNumberValues(int index, JavaScriptFrame* frame) {
  // We need to adjust the stack index by one for the top-most frame.
  int extra_slot_count = (index == output_count() - 1) ? 1 : 0;
  List<ValueDescriptionInteger32>* ints = &integer32_values_[index];
  for (int i = 0; i < ints->length(); i++) {
    ValueDescriptionInteger32 value = ints->at(i);
    double val = static_cast<double>(value.int32_value());
    InsertHeapNumberValue(frame, value.stack_index(), val, extra_slot_count);
  }

  // Iterate over double values and convert them to a heap number.
  List<ValueDescriptionDouble>* doubles = &double_values_[index];
  for (int i = 0; i < doubles->length(); ++i) {
    ValueDescriptionDouble value = doubles->at(i);
    InsertHeapNumberValue(frame, value.stack_index(), value.double_value(),
                          extra_slot_count);
  }
}


void Deoptimizer::InsertHeapNumberValue(JavaScriptFrame* frame,
                                        int stack_index,
                                        double val,
                                        int extra_slot_count) {
  // Add one to the TOS index to take the 'state' pushed before jumping
  // to the stub that calls Runtime::NotifyDeoptimized into account.
  int tos_index = stack_index + extra_slot_count;
  int index = (frame->ComputeExpressionsCount() - 1) - tos_index;
  if (FLAG_trace_deopt) PrintF("Allocating a new heap number: %e\n", val);
  Handle<Object> num = Factory::NewNumber(val);
  frame->SetExpression(index, *num);
}


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
      unsigned output_index = output_offset / kPointerSize;
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
        AddInteger32Value(frame_index,
                          output_index,
                          static_cast<int32_t>(value));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::DOUBLE_REGISTER: {
      int input_reg = iterator->Next();
      double value = input_->GetDoubleRegister(input_reg);
      unsigned output_index = output_offset / kPointerSize;
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- %e ; %s\n",
               output_[frame_index]->GetTop() + output_offset,
               output_offset,
               value,
               DoubleRegister::AllocationIndexToString(input_reg));
      }
      // We save the untagged value on the side and store a GC-safe
      // temporary placeholder in the frame.
      AddDoubleValue(frame_index, output_index, value);
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
      unsigned output_index = output_offset / kPointerSize;
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
        AddInteger32Value(frame_index,
                          output_index,
                          static_cast<int32_t>(value));
        output_[frame_index]->SetFrameSlot(output_offset, kPlaceholder);
      }
      return;
    }

    case Translation::DOUBLE_STACK_SLOT: {
      int input_slot_index = iterator->Next();
      unsigned input_offset =
          input_->GetOffsetFromSlotIndex(this, input_slot_index);
      double value = input_->GetDoubleFrameSlot(input_offset);
      unsigned output_index = output_offset / kPointerSize;
      if (FLAG_trace_deopt) {
        PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- %e ; [esp + %d]\n",
               output_[frame_index]->GetTop() + output_offset,
               output_offset,
               value,
               input_offset);
      }
      // We save the untagged value on the side and store a GC-safe
      // temporary placeholder in the frame.
      AddDoubleValue(frame_index, output_index, value);
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
        Heap::arguments_marker()->ShortPrint();
        PrintF(" ; arguments object\n");
      }
      intptr_t value = reinterpret_cast<intptr_t>(Heap::arguments_marker());
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
         PrintF("    %s <- 0x%08" V8PRIxPTR " ; [esp + %d]\n",
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
        PrintF("    %s <- %d (int32) ; [esp + %d]\n",
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
        PrintF("    %s <- %g (double) ; [esp + %d]\n",
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
        PrintF("    [esp + %d] <- 0x%08" V8PRIxPTR " ; [esp + %d]\n",
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
        PrintF("    [esp + %d] <- %d (int32) ; [esp + %d]\n",
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
        PrintF("    [esp + %d] <- 0x%08x (upper bits of %g) ; [esp + %d]\n",
               output_offset + kUpperOffset,
               upper,
               double_value,
               *input_offset);
        PrintF("    [esp + %d] <- 0x%08x (lower bits of %g) ; [esp + %d]\n",
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


void Deoptimizer::AddInteger32Value(int frame_index,
                                    int slot_index,
                                    int32_t value) {
  ValueDescriptionInteger32 value_desc(slot_index, value);
  integer32_values_[frame_index].Add(value_desc);
}


void Deoptimizer::AddDoubleValue(int frame_index,
                                 int slot_index,
                                 double value) {
  ValueDescriptionDouble value_desc(slot_index, value);
  double_values_[frame_index].Add(value_desc);
}


LargeObjectChunk* Deoptimizer::CreateCode(BailoutType type) {
  // We cannot run this if the serializer is enabled because this will
  // cause us to emit relocation information for the external
  // references. This is fine because the deoptimizer's code section
  // isn't meant to be serialized at all.
  ASSERT(!Serializer::enabled());
  bool old_debug_code = FLAG_debug_code;
  FLAG_debug_code = false;

  MacroAssembler masm(NULL, 16 * KB);
  GenerateDeoptimizationEntries(&masm, kNumberOfEntries, type);
  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(desc.reloc_size == 0);

  LargeObjectChunk* chunk = LargeObjectChunk::New(desc.instr_size, EXECUTABLE);
  memcpy(chunk->GetStartAddress(), desc.buffer, desc.instr_size);
  CPU::FlushICache(chunk->GetStartAddress(), desc.instr_size);
  FLAG_debug_code = old_debug_code;
  return chunk;
}


Code* Deoptimizer::FindDeoptimizingCodeFromAddress(Address addr) {
  DeoptimizingCodeListNode* node = Deoptimizer::deoptimizing_code_list_;
  while (node != NULL) {
    if (node->code()->contains(addr)) return *node->code();
    node = node->next();
  }
  return NULL;
}


void Deoptimizer::RemoveDeoptimizingCode(Code* code) {
  ASSERT(deoptimizing_code_list_ != NULL);
  // Run through the code objects to find this one and remove it.
  DeoptimizingCodeListNode* prev = NULL;
  DeoptimizingCodeListNode* current = deoptimizing_code_list_;
  while (current != NULL) {
    if (*current->code() == code) {
      // Unlink from list. If prev is NULL we are looking at the first element.
      if (prev == NULL) {
        deoptimizing_code_list_ = current->next();
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
    unsigned base = static_cast<unsigned>(
        GetFrameSize() - deoptimizer->ComputeFixedSize(GetFunction()));
    return base - ((slot_index + 1) * kPointerSize);
  } else {
    // Incoming parameter.
    unsigned base = static_cast<unsigned>(GetFrameSize() -
        deoptimizer->ComputeIncomingArgumentSize(GetFunction()));
    return base - ((slot_index + 1) * kPointerSize);
  }
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
  ASSERT(HasNext());
  // Run through the bytes until we reach one with a least significant
  // bit of zero (marks the end).
  uint32_t bits = 0;
  for (int i = 0; true; i += 7) {
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
  Handle<ByteArray> result = Factory::NewByteArray(length, TENURED);
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


#ifdef OBJECT_PRINT

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
  // Globalize the code object and make it weak.
  code_ = Handle<Code>::cast((GlobalHandles::Create(code)));
  GlobalHandles::MakeWeak(reinterpret_cast<Object**>(code_.location()),
                          this,
                          Deoptimizer::HandleWeakDeoptimizedCode);
}


DeoptimizingCodeListNode::~DeoptimizingCodeListNode() {
  GlobalHandles::Destroy(reinterpret_cast<Object**>(code_.location()));
}


} }  // namespace v8::internal
