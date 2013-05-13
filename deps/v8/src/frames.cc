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

#include "ast.h"
#include "deoptimizer.h"
#include "frames-inl.h"
#include "full-codegen.h"
#include "lazy-instance.h"
#include "mark-compact.h"
#include "safepoint-table.h"
#include "scopeinfo.h"
#include "string-stream.h"

#include "allocation-inl.h"

namespace v8 {
namespace internal {


static ReturnAddressLocationResolver return_address_location_resolver = NULL;


// Resolves pc_address through the resolution address function if one is set.
static inline Address* ResolveReturnAddressLocation(Address* pc_address) {
  if (return_address_location_resolver == NULL) {
    return pc_address;
  } else {
    return reinterpret_cast<Address*>(
        return_address_location_resolver(
            reinterpret_cast<uintptr_t>(pc_address)));
  }
}


// Iterator that supports traversing the stack handlers of a
// particular frame. Needs to know the top of the handler chain.
class StackHandlerIterator BASE_EMBEDDED {
 public:
  StackHandlerIterator(const StackFrame* frame, StackHandler* handler)
      : limit_(frame->fp()), handler_(handler) {
    // Make sure the handler has already been unwound to this frame.
    ASSERT(frame->sp() <= handler->address());
  }

  StackHandler* handler() const { return handler_; }

  bool done() {
    return handler_ == NULL || handler_->address() > limit_;
  }
  void Advance() {
    ASSERT(!done());
    handler_ = handler_->next();
  }

 private:
  const Address limit_;
  StackHandler* handler_;
};


// -------------------------------------------------------------------------


#define INITIALIZE_SINGLETON(type, field) field##_(this),
StackFrameIterator::StackFrameIterator(Isolate* isolate)
    : isolate_(isolate),
      STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON)
      frame_(NULL), handler_(NULL),
      thread_(isolate_->thread_local_top()),
      fp_(NULL), sp_(NULL), advance_(&StackFrameIterator::AdvanceWithHandler) {
  Reset();
}
StackFrameIterator::StackFrameIterator(Isolate* isolate, ThreadLocalTop* t)
    : isolate_(isolate),
      STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON)
      frame_(NULL), handler_(NULL), thread_(t),
      fp_(NULL), sp_(NULL), advance_(&StackFrameIterator::AdvanceWithHandler) {
  Reset();
}
StackFrameIterator::StackFrameIterator(Isolate* isolate,
                                       bool use_top, Address fp, Address sp)
    : isolate_(isolate),
      STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON)
      frame_(NULL), handler_(NULL),
      thread_(use_top ? isolate_->thread_local_top() : NULL),
      fp_(use_top ? NULL : fp), sp_(sp),
      advance_(use_top ? &StackFrameIterator::AdvanceWithHandler :
               &StackFrameIterator::AdvanceWithoutHandler) {
  if (use_top || fp != NULL) {
    Reset();
  }
}

#undef INITIALIZE_SINGLETON


void StackFrameIterator::AdvanceWithHandler() {
  ASSERT(!done());
  // Compute the state of the calling frame before restoring
  // callee-saved registers and unwinding handlers. This allows the
  // frame code that computes the caller state to access the top
  // handler and the value of any callee-saved register if needed.
  StackFrame::State state;
  StackFrame::Type type = frame_->GetCallerState(&state);

  // Unwind handlers corresponding to the current frame.
  StackHandlerIterator it(frame_, handler_);
  while (!it.done()) it.Advance();
  handler_ = it.handler();

  // Advance to the calling frame.
  frame_ = SingletonFor(type, &state);

  // When we're done iterating over the stack frames, the handler
  // chain must have been completely unwound.
  ASSERT(!done() || handler_ == NULL);
}


void StackFrameIterator::AdvanceWithoutHandler() {
  // A simpler version of Advance which doesn't care about handler.
  ASSERT(!done());
  StackFrame::State state;
  StackFrame::Type type = frame_->GetCallerState(&state);
  frame_ = SingletonFor(type, &state);
}


void StackFrameIterator::Reset() {
  StackFrame::State state;
  StackFrame::Type type;
  if (thread_ != NULL) {
    type = ExitFrame::GetStateForFramePointer(
        Isolate::c_entry_fp(thread_), &state);
    handler_ = StackHandler::FromAddress(
        Isolate::handler(thread_));
  } else {
    ASSERT(fp_ != NULL);
    state.fp = fp_;
    state.sp = sp_;
    state.pc_address = ResolveReturnAddressLocation(
        reinterpret_cast<Address*>(StandardFrame::ComputePCAddress(fp_)));
    type = StackFrame::ComputeType(isolate(), &state);
  }
  if (SingletonFor(type) == NULL) return;
  frame_ = SingletonFor(type, &state);
}


StackFrame* StackFrameIterator::SingletonFor(StackFrame::Type type,
                                             StackFrame::State* state) {
  if (type == StackFrame::NONE) return NULL;
  StackFrame* result = SingletonFor(type);
  ASSERT(result != NULL);
  result->state_ = *state;
  return result;
}


StackFrame* StackFrameIterator::SingletonFor(StackFrame::Type type) {
#define FRAME_TYPE_CASE(type, field) \
  case StackFrame::type: result = &field##_; break;

  StackFrame* result = NULL;
  switch (type) {
    case StackFrame::NONE: return NULL;
    STACK_FRAME_TYPE_LIST(FRAME_TYPE_CASE)
    default: break;
  }
  return result;

#undef FRAME_TYPE_CASE
}


// -------------------------------------------------------------------------


StackTraceFrameIterator::StackTraceFrameIterator(Isolate* isolate)
    : JavaScriptFrameIterator(isolate) {
  if (!done() && !IsValidFrame()) Advance();
}


void StackTraceFrameIterator::Advance() {
  while (true) {
    JavaScriptFrameIterator::Advance();
    if (done()) return;
    if (IsValidFrame()) return;
  }
}

bool StackTraceFrameIterator::IsValidFrame() {
    if (!frame()->function()->IsJSFunction()) return false;
    Object* script = JSFunction::cast(frame()->function())->shared()->script();
    // Don't show functions from native scripts to user.
    return (script->IsScript() &&
            Script::TYPE_NATIVE != Script::cast(script)->type()->value());
}


// -------------------------------------------------------------------------


bool SafeStackFrameIterator::ExitFrameValidator::IsValidFP(Address fp) {
  if (!validator_.IsValid(fp)) return false;
  Address sp = ExitFrame::ComputeStackPointer(fp);
  if (!validator_.IsValid(sp)) return false;
  StackFrame::State state;
  ExitFrame::FillState(fp, sp, &state);
  if (!validator_.IsValid(reinterpret_cast<Address>(state.pc_address))) {
    return false;
  }
  return *state.pc_address != NULL;
}


SafeStackFrameIterator::ActiveCountMaintainer::ActiveCountMaintainer(
    Isolate* isolate)
    : isolate_(isolate) {
  isolate_->set_safe_stack_iterator_counter(
      isolate_->safe_stack_iterator_counter() + 1);
}


SafeStackFrameIterator::ActiveCountMaintainer::~ActiveCountMaintainer() {
  isolate_->set_safe_stack_iterator_counter(
      isolate_->safe_stack_iterator_counter() - 1);
}


SafeStackFrameIterator::SafeStackFrameIterator(
    Isolate* isolate,
    Address fp, Address sp, Address low_bound, Address high_bound) :
    maintainer_(isolate),
    stack_validator_(low_bound, high_bound),
    is_valid_top_(IsValidTop(isolate, low_bound, high_bound)),
    is_valid_fp_(IsWithinBounds(low_bound, high_bound, fp)),
    is_working_iterator_(is_valid_top_ || is_valid_fp_),
    iteration_done_(!is_working_iterator_),
    iterator_(isolate, is_valid_top_, is_valid_fp_ ? fp : NULL, sp) {
}

bool SafeStackFrameIterator::is_active(Isolate* isolate) {
  return isolate->safe_stack_iterator_counter() > 0;
}


bool SafeStackFrameIterator::IsValidTop(Isolate* isolate,
                                        Address low_bound, Address high_bound) {
  ThreadLocalTop* top = isolate->thread_local_top();
  Address fp = Isolate::c_entry_fp(top);
  ExitFrameValidator validator(low_bound, high_bound);
  if (!validator.IsValidFP(fp)) return false;
  return Isolate::handler(top) != NULL;
}


void SafeStackFrameIterator::Advance() {
  ASSERT(is_working_iterator_);
  ASSERT(!done());
  StackFrame* last_frame = iterator_.frame();
  Address last_sp = last_frame->sp(), last_fp = last_frame->fp();
  // Before advancing to the next stack frame, perform pointer validity tests
  iteration_done_ = !IsValidFrame(last_frame) ||
      !CanIterateHandles(last_frame, iterator_.handler()) ||
      !IsValidCaller(last_frame);
  if (iteration_done_) return;

  iterator_.Advance();
  if (iterator_.done()) return;
  // Check that we have actually moved to the previous frame in the stack
  StackFrame* prev_frame = iterator_.frame();
  iteration_done_ = prev_frame->sp() < last_sp || prev_frame->fp() < last_fp;
}


bool SafeStackFrameIterator::CanIterateHandles(StackFrame* frame,
                                               StackHandler* handler) {
  // If StackIterator iterates over StackHandles, verify that
  // StackHandlerIterator can be instantiated (see StackHandlerIterator
  // constructor.)
  return !is_valid_top_ || (frame->sp() <= handler->address());
}


bool SafeStackFrameIterator::IsValidFrame(StackFrame* frame) const {
  return IsValidStackAddress(frame->sp()) && IsValidStackAddress(frame->fp());
}


bool SafeStackFrameIterator::IsValidCaller(StackFrame* frame) {
  StackFrame::State state;
  if (frame->is_entry() || frame->is_entry_construct()) {
    // See EntryFrame::GetCallerState. It computes the caller FP address
    // and calls ExitFrame::GetStateForFramePointer on it. We need to be
    // sure that caller FP address is valid.
    Address caller_fp = Memory::Address_at(
        frame->fp() + EntryFrameConstants::kCallerFPOffset);
    ExitFrameValidator validator(stack_validator_);
    if (!validator.IsValidFP(caller_fp)) return false;
  } else if (frame->is_arguments_adaptor()) {
    // See ArgumentsAdaptorFrame::GetCallerStackPointer. It assumes that
    // the number of arguments is stored on stack as Smi. We need to check
    // that it really an Smi.
    Object* number_of_args = reinterpret_cast<ArgumentsAdaptorFrame*>(frame)->
        GetExpression(0);
    if (!number_of_args->IsSmi()) {
      return false;
    }
  }
  frame->ComputeCallerState(&state);
  return IsValidStackAddress(state.sp) && IsValidStackAddress(state.fp) &&
      iterator_.SingletonFor(frame->GetCallerState(&state)) != NULL;
}


void SafeStackFrameIterator::Reset() {
  if (is_working_iterator_) {
    iterator_.Reset();
    iteration_done_ = false;
  }
}


// -------------------------------------------------------------------------


SafeStackTraceFrameIterator::SafeStackTraceFrameIterator(
    Isolate* isolate,
    Address fp, Address sp, Address low_bound, Address high_bound) :
    SafeJavaScriptFrameIterator(isolate, fp, sp, low_bound, high_bound) {
  if (!done() && !frame()->is_java_script()) Advance();
}


void SafeStackTraceFrameIterator::Advance() {
  while (true) {
    SafeJavaScriptFrameIterator::Advance();
    if (done()) return;
    if (frame()->is_java_script()) return;
  }
}


Code* StackFrame::GetSafepointData(Isolate* isolate,
                                   Address inner_pointer,
                                   SafepointEntry* safepoint_entry,
                                   unsigned* stack_slots) {
  InnerPointerToCodeCache::InnerPointerToCodeCacheEntry* entry =
      isolate->inner_pointer_to_code_cache()->GetCacheEntry(inner_pointer);
  if (!entry->safepoint_entry.is_valid()) {
    entry->safepoint_entry = entry->code->GetSafepointEntry(inner_pointer);
    ASSERT(entry->safepoint_entry.is_valid());
  } else {
    ASSERT(entry->safepoint_entry.Equals(
        entry->code->GetSafepointEntry(inner_pointer)));
  }

  // Fill in the results and return the code.
  Code* code = entry->code;
  *safepoint_entry = entry->safepoint_entry;
  *stack_slots = code->stack_slots();
  return code;
}


bool StackFrame::HasHandler() const {
  StackHandlerIterator it(this, top_handler());
  return !it.done();
}


#ifdef DEBUG
static bool GcSafeCodeContains(HeapObject* object, Address addr);
#endif


void StackFrame::IteratePc(ObjectVisitor* v,
                           Address* pc_address,
                           Code* holder) {
  Address pc = *pc_address;
  ASSERT(GcSafeCodeContains(holder, pc));
  unsigned pc_offset = static_cast<unsigned>(pc - holder->instruction_start());
  Object* code = holder;
  v->VisitPointer(&code);
  if (code != holder) {
    holder = reinterpret_cast<Code*>(code);
    pc = holder->instruction_start() + pc_offset;
    *pc_address = pc;
  }
}


void StackFrame::SetReturnAddressLocationResolver(
    ReturnAddressLocationResolver resolver) {
  ASSERT(return_address_location_resolver == NULL);
  return_address_location_resolver = resolver;
}


StackFrame::Type StackFrame::ComputeType(Isolate* isolate, State* state) {
  ASSERT(state->fp != NULL);
  if (StandardFrame::IsArgumentsAdaptorFrame(state->fp)) {
    return ARGUMENTS_ADAPTOR;
  }
  // The marker and function offsets overlap. If the marker isn't a
  // smi then the frame is a JavaScript frame -- and the marker is
  // really the function.
  const int offset = StandardFrameConstants::kMarkerOffset;
  Object* marker = Memory::Object_at(state->fp + offset);
  if (!marker->IsSmi()) {
    // If we're using a "safe" stack iterator, we treat optimized
    // frames as normal JavaScript frames to avoid having to look
    // into the heap to determine the state. This is safe as long
    // as nobody tries to GC...
    if (SafeStackFrameIterator::is_active(isolate)) return JAVA_SCRIPT;
    Code::Kind kind = GetContainingCode(isolate, *(state->pc_address))->kind();
    ASSERT(kind == Code::FUNCTION || kind == Code::OPTIMIZED_FUNCTION);
    return (kind == Code::OPTIMIZED_FUNCTION) ? OPTIMIZED : JAVA_SCRIPT;
  }
  return static_cast<StackFrame::Type>(Smi::cast(marker)->value());
}



StackFrame::Type StackFrame::GetCallerState(State* state) const {
  ComputeCallerState(state);
  return ComputeType(isolate(), state);
}


Address StackFrame::UnpaddedFP() const {
#if defined(V8_TARGET_ARCH_IA32)
  if (!is_optimized()) return fp();
  int32_t alignment_state = Memory::int32_at(
    fp() + JavaScriptFrameConstants::kDynamicAlignmentStateOffset);

  return (alignment_state == kAlignmentPaddingPushed) ?
    (fp() + kPointerSize) : fp();
#else
  return fp();
#endif
}


Code* EntryFrame::unchecked_code() const {
  return HEAP->js_entry_code();
}


void EntryFrame::ComputeCallerState(State* state) const {
  GetCallerState(state);
}


void EntryFrame::SetCallerFp(Address caller_fp) {
  const int offset = EntryFrameConstants::kCallerFPOffset;
  Memory::Address_at(this->fp() + offset) = caller_fp;
}


StackFrame::Type EntryFrame::GetCallerState(State* state) const {
  const int offset = EntryFrameConstants::kCallerFPOffset;
  Address fp = Memory::Address_at(this->fp() + offset);
  return ExitFrame::GetStateForFramePointer(fp, state);
}


Code* EntryConstructFrame::unchecked_code() const {
  return HEAP->js_construct_entry_code();
}


Object*& ExitFrame::code_slot() const {
  const int offset = ExitFrameConstants::kCodeOffset;
  return Memory::Object_at(fp() + offset);
}


Code* ExitFrame::unchecked_code() const {
  return reinterpret_cast<Code*>(code_slot());
}


void ExitFrame::ComputeCallerState(State* state) const {
  // Set up the caller state.
  state->sp = caller_sp();
  state->fp = Memory::Address_at(fp() + ExitFrameConstants::kCallerFPOffset);
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(fp() + ExitFrameConstants::kCallerPCOffset));
}


void ExitFrame::SetCallerFp(Address caller_fp) {
  Memory::Address_at(fp() + ExitFrameConstants::kCallerFPOffset) = caller_fp;
}


void ExitFrame::Iterate(ObjectVisitor* v) const {
  // The arguments are traversed as part of the expression stack of
  // the calling frame.
  IteratePc(v, pc_address(), LookupCode());
  v->VisitPointer(&code_slot());
}


Address ExitFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPDisplacement;
}


StackFrame::Type ExitFrame::GetStateForFramePointer(Address fp, State* state) {
  if (fp == 0) return NONE;
  Address sp = ComputeStackPointer(fp);
  FillState(fp, sp, state);
  ASSERT(*state->pc_address != NULL);
  return EXIT;
}


void ExitFrame::FillState(Address fp, Address sp, State* state) {
  state->sp = sp;
  state->fp = fp;
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(sp - 1 * kPointerSize));
}


Address StandardFrame::GetExpressionAddress(int n) const {
  const int offset = StandardFrameConstants::kExpressionsOffset;
  return fp() + offset - n * kPointerSize;
}


Object* StandardFrame::GetExpression(Address fp, int index) {
  return Memory::Object_at(GetExpressionAddress(fp, index));
}


Address StandardFrame::GetExpressionAddress(Address fp, int n) {
  const int offset = StandardFrameConstants::kExpressionsOffset;
  return fp + offset - n * kPointerSize;
}


int StandardFrame::ComputeExpressionsCount() const {
  const int offset =
      StandardFrameConstants::kExpressionsOffset + kPointerSize;
  Address base = fp() + offset;
  Address limit = sp();
  ASSERT(base >= limit);  // stack grows downwards
  // Include register-allocated locals in number of expressions.
  return static_cast<int>((base - limit) / kPointerSize);
}


void StandardFrame::ComputeCallerState(State* state) const {
  state->sp = caller_sp();
  state->fp = caller_fp();
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(ComputePCAddress(fp())));
}


void StandardFrame::SetCallerFp(Address caller_fp) {
  Memory::Address_at(fp() + StandardFrameConstants::kCallerFPOffset) =
      caller_fp;
}


bool StandardFrame::IsExpressionInsideHandler(int n) const {
  Address address = GetExpressionAddress(n);
  for (StackHandlerIterator it(this, top_handler()); !it.done(); it.Advance()) {
    if (it.handler()->includes(address)) return true;
  }
  return false;
}


void StandardFrame::IterateCompiledFrame(ObjectVisitor* v) const {
  // Make sure that we're not doing "safe" stack frame iteration. We cannot
  // possibly find pointers in optimized frames in that state.
  ASSERT(!SafeStackFrameIterator::is_active(isolate()));

  // Compute the safepoint information.
  unsigned stack_slots = 0;
  SafepointEntry safepoint_entry;
  Code* code = StackFrame::GetSafepointData(
      isolate(), pc(), &safepoint_entry, &stack_slots);
  unsigned slot_space = stack_slots * kPointerSize;

  // Visit the outgoing parameters.
  Object** parameters_base = &Memory::Object_at(sp());
  Object** parameters_limit = &Memory::Object_at(
      fp() + JavaScriptFrameConstants::kFunctionOffset - slot_space);

  // Visit the parameters that may be on top of the saved registers.
  if (safepoint_entry.argument_count() > 0) {
    v->VisitPointers(parameters_base,
                     parameters_base + safepoint_entry.argument_count());
    parameters_base += safepoint_entry.argument_count();
  }

  // Skip saved double registers.
  if (safepoint_entry.has_doubles()) {
    // Number of doubles not known at snapshot time.
    ASSERT(!Serializer::enabled());
    parameters_base += DoubleRegister::NumAllocatableRegisters() *
        kDoubleSize / kPointerSize;
  }

  // Visit the registers that contain pointers if any.
  if (safepoint_entry.HasRegisters()) {
    for (int i = kNumSafepointRegisters - 1; i >=0; i--) {
      if (safepoint_entry.HasRegisterAt(i)) {
        int reg_stack_index = MacroAssembler::SafepointRegisterStackIndex(i);
        v->VisitPointer(parameters_base + reg_stack_index);
      }
    }
    // Skip the words containing the register values.
    parameters_base += kNumSafepointRegisters;
  }

  // We're done dealing with the register bits.
  uint8_t* safepoint_bits = safepoint_entry.bits();
  safepoint_bits += kNumSafepointRegisters >> kBitsPerByteLog2;

  // Visit the rest of the parameters.
  v->VisitPointers(parameters_base, parameters_limit);

  // Visit pointer spill slots and locals.
  for (unsigned index = 0; index < stack_slots; index++) {
    int byte_index = index >> kBitsPerByteLog2;
    int bit_index = index & (kBitsPerByte - 1);
    if ((safepoint_bits[byte_index] & (1U << bit_index)) != 0) {
      v->VisitPointer(parameters_limit + index);
    }
  }

  // Visit the return address in the callee and incoming arguments.
  IteratePc(v, pc_address(), code);

  // Visit the context in stub frame and JavaScript frame.
  // Visit the function in JavaScript frame.
  Object** fixed_base = &Memory::Object_at(
      fp() + StandardFrameConstants::kMarkerOffset);
  Object** fixed_limit = &Memory::Object_at(fp());
  v->VisitPointers(fixed_base, fixed_limit);
}


void StubFrame::Iterate(ObjectVisitor* v) const {
  IterateCompiledFrame(v);
}


Code* StubFrame::unchecked_code() const {
  return static_cast<Code*>(isolate()->heap()->FindCodeObject(pc()));
}


Address StubFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPDisplacement;
}


int StubFrame::GetNumberOfIncomingArguments() const {
  return 0;
}


void OptimizedFrame::Iterate(ObjectVisitor* v) const {
#ifdef DEBUG
  // Make sure that optimized frames do not contain any stack handlers.
  StackHandlerIterator it(this, top_handler());
  ASSERT(it.done());
#endif

  IterateCompiledFrame(v);
}


void JavaScriptFrame::SetParameterValue(int index, Object* value) const {
  Memory::Object_at(GetParameterSlot(index)) = value;
}


bool JavaScriptFrame::IsConstructor() const {
  Address fp = caller_fp();
  if (has_adapted_arguments()) {
    // Skip the arguments adaptor frame and look at the real caller.
    fp = Memory::Address_at(fp + StandardFrameConstants::kCallerFPOffset);
  }
  return IsConstructFrame(fp);
}


int JavaScriptFrame::GetArgumentsLength() const {
  // If there is an arguments adaptor frame get the arguments length from it.
  if (has_adapted_arguments()) {
    return Smi::cast(GetExpression(caller_fp(), 0))->value();
  } else {
    return GetNumberOfIncomingArguments();
  }
}


Code* JavaScriptFrame::unchecked_code() const {
  JSFunction* function = JSFunction::cast(this->function());
  return function->unchecked_code();
}


int JavaScriptFrame::GetNumberOfIncomingArguments() const {
  ASSERT(!SafeStackFrameIterator::is_active(isolate()) &&
         isolate()->heap()->gc_state() == Heap::NOT_IN_GC);

  JSFunction* function = JSFunction::cast(this->function());
  return function->shared()->formal_parameter_count();
}


Address JavaScriptFrame::GetCallerStackPointer() const {
  return fp() + StandardFrameConstants::kCallerSPOffset;
}


void JavaScriptFrame::GetFunctions(List<JSFunction*>* functions) {
  ASSERT(functions->length() == 0);
  functions->Add(JSFunction::cast(function()));
}


void JavaScriptFrame::Summarize(List<FrameSummary>* functions) {
  ASSERT(functions->length() == 0);
  Code* code_pointer = LookupCode();
  int offset = static_cast<int>(pc() - code_pointer->address());
  FrameSummary summary(receiver(),
                       JSFunction::cast(function()),
                       code_pointer,
                       offset,
                       IsConstructor());
  functions->Add(summary);
}


void JavaScriptFrame::PrintTop(Isolate* isolate,
                               FILE* file,
                               bool print_args,
                               bool print_line_number) {
  // constructor calls
  HandleScope scope(isolate);
  AssertNoAllocation no_allocation;
  JavaScriptFrameIterator it(isolate);
  while (!it.done()) {
    if (it.frame()->is_java_script()) {
      JavaScriptFrame* frame = it.frame();
      if (frame->IsConstructor()) PrintF(file, "new ");
      // function name
      Object* maybe_fun = frame->function();
      if (maybe_fun->IsJSFunction()) {
        JSFunction* fun = JSFunction::cast(maybe_fun);
        fun->PrintName();
        Code* js_code = frame->unchecked_code();
        Address pc = frame->pc();
        int code_offset =
            static_cast<int>(pc - js_code->instruction_start());
        PrintF("+%d", code_offset);
        SharedFunctionInfo* shared = fun->shared();
        if (print_line_number) {
          Code* code = Code::cast(
              v8::internal::Isolate::Current()->heap()->FindCodeObject(pc));
          int source_pos = code->SourcePosition(pc);
          Object* maybe_script = shared->script();
          if (maybe_script->IsScript()) {
            Handle<Script> script(Script::cast(maybe_script));
            int line = GetScriptLineNumberSafe(script, source_pos) + 1;
            Object* script_name_raw = script->name();
            if (script_name_raw->IsString()) {
              String* script_name = String::cast(script->name());
              SmartArrayPointer<char> c_script_name =
                  script_name->ToCString(DISALLOW_NULLS,
                                         ROBUST_STRING_TRAVERSAL);
              PrintF(file, " at %s:%d", *c_script_name, line);
            } else {
              PrintF(file, " at <unknown>:%d", line);
            }
          } else {
            PrintF(file, " at <unknown>:<unknown>");
          }
        }
      } else {
        PrintF("<unknown>");
      }

      if (print_args) {
        // function arguments
        // (we are intentionally only printing the actually
        // supplied parameters, not all parameters required)
        PrintF(file, "(this=");
        frame->receiver()->ShortPrint(file);
        const int length = frame->ComputeParametersCount();
        for (int i = 0; i < length; i++) {
          PrintF(file, ", ");
          frame->GetParameter(i)->ShortPrint(file);
        }
        PrintF(file, ")");
      }
      break;
    }
    it.Advance();
  }
}


void JavaScriptFrame::SaveOperandStack(FixedArray* store,
                                       int* stack_handler_index) const {
  int operands_count = store->length();
  ASSERT_LE(operands_count, ComputeOperandsCount());

  // Visit the stack in LIFO order, saving operands and stack handlers into the
  // array.  The saved stack handlers store a link to the next stack handler,
  // which will allow RestoreOperandStack to rewind the handlers.
  StackHandlerIterator it(this, top_handler());
  int i = operands_count - 1;
  *stack_handler_index = -1;
  for (; !it.done(); it.Advance()) {
    StackHandler* handler = it.handler();
    // Save operands pushed after the handler was pushed.
    for (; GetOperandSlot(i) < handler->address(); i--) {
      store->set(i, GetOperand(i));
    }
    ASSERT_GE(i + 1, StackHandlerConstants::kSlotCount);
    ASSERT_EQ(handler->address(), GetOperandSlot(i));
    int next_stack_handler_index = i + 1 - StackHandlerConstants::kSlotCount;
    handler->Unwind(isolate(), store, next_stack_handler_index,
                    *stack_handler_index);
    *stack_handler_index = next_stack_handler_index;
    i -= StackHandlerConstants::kSlotCount;
  }

  // Save any remaining operands.
  for (; i >= 0; i--) {
    store->set(i, GetOperand(i));
  }
}


void JavaScriptFrame::RestoreOperandStack(FixedArray* store,
                                          int stack_handler_index) {
  int operands_count = store->length();
  ASSERT_LE(operands_count, ComputeOperandsCount());
  int i = 0;
  while (i <= stack_handler_index) {
    if (i < stack_handler_index) {
      // An operand.
      ASSERT_EQ(GetOperand(i), isolate()->heap()->the_hole_value());
      Memory::Object_at(GetOperandSlot(i)) = store->get(i);
      i++;
    } else {
      // A stack handler.
      ASSERT_EQ(i, stack_handler_index);
      // The FixedArray store grows up.  The stack grows down.  So the operand
      // slot for i actually points to the bottom of the top word in the
      // handler.  The base of the StackHandler* is the address of the bottom
      // word, which will be the last slot that is in the handler.
      int handler_slot_index = i + StackHandlerConstants::kSlotCount - 1;
      StackHandler *handler =
          StackHandler::FromAddress(GetOperandSlot(handler_slot_index));
      stack_handler_index = handler->Rewind(isolate(), store, i, fp());
      i += StackHandlerConstants::kSlotCount;
    }
  }

  for (; i < operands_count; i++) {
    ASSERT_EQ(GetOperand(i), isolate()->heap()->the_hole_value());
    Memory::Object_at(GetOperandSlot(i)) = store->get(i);
  }
}


void FrameSummary::Print() {
  PrintF("receiver: ");
  receiver_->ShortPrint();
  PrintF("\nfunction: ");
  function_->shared()->DebugName()->ShortPrint();
  PrintF("\ncode: ");
  code_->ShortPrint();
  if (code_->kind() == Code::FUNCTION) PrintF(" NON-OPT");
  if (code_->kind() == Code::OPTIMIZED_FUNCTION) PrintF(" OPT");
  PrintF("\npc: %d\n", offset_);
}


JSFunction* OptimizedFrame::LiteralAt(FixedArray* literal_array,
                                      int literal_id) {
  if (literal_id == Translation::kSelfLiteralId) {
    return JSFunction::cast(function());
  }

  return JSFunction::cast(literal_array->get(literal_id));
}


void OptimizedFrame::Summarize(List<FrameSummary>* frames) {
  ASSERT(frames->length() == 0);
  ASSERT(is_optimized());

  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationInputData* data = GetDeoptimizationData(&deopt_index);
  FixedArray* literal_array = data->LiteralArray();

  // BUG(3243555): Since we don't have a lazy-deopt registered at
  // throw-statements, we can't use the translation at the call-site of
  // throw. An entry with no deoptimization index indicates a call-site
  // without a lazy-deopt. As a consequence we are not allowed to inline
  // functions containing throw.
  if (deopt_index == Safepoint::kNoDeoptimizationIndex) {
    JavaScriptFrame::Summarize(frames);
    return;
  }

  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  ASSERT(opcode == Translation::BEGIN);
  it.Next();  // Drop frame count.
  int jsframe_count = it.Next();

  // We create the summary in reverse order because the frames
  // in the deoptimization translation are ordered bottom-to-top.
  bool is_constructor = IsConstructor();
  int i = jsframe_count;
  while (i > 0) {
    opcode = static_cast<Translation::Opcode>(it.Next());
    if (opcode == Translation::JS_FRAME) {
      i--;
      BailoutId ast_id = BailoutId(it.Next());
      JSFunction* function = LiteralAt(literal_array, it.Next());
      it.Next();  // Skip height.

      // The translation commands are ordered and the receiver is always
      // at the first position. Since we are always at a call when we need
      // to construct a stack trace, the receiver is always in a stack slot.
      opcode = static_cast<Translation::Opcode>(it.Next());
      ASSERT(opcode == Translation::STACK_SLOT ||
             opcode == Translation::LITERAL);
      int index = it.Next();

      // Get the correct receiver in the optimized frame.
      Object* receiver = NULL;
      if (opcode == Translation::LITERAL) {
        receiver = data->LiteralArray()->get(index);
      } else {
        // Positive index means the value is spilled to the locals
        // area. Negative means it is stored in the incoming parameter
        // area.
        if (index >= 0) {
          receiver = GetExpression(index);
        } else {
          // Index -1 overlaps with last parameter, -n with the first parameter,
          // (-n - 1) with the receiver with n being the number of parameters
          // of the outermost, optimized frame.
          int parameter_count = ComputeParametersCount();
          int parameter_index = index + parameter_count;
          receiver = (parameter_index == -1)
              ? this->receiver()
              : this->GetParameter(parameter_index);
        }
      }

      Code* code = function->shared()->code();
      DeoptimizationOutputData* output_data =
          DeoptimizationOutputData::cast(code->deoptimization_data());
      unsigned entry = Deoptimizer::GetOutputInfo(output_data,
                                                  ast_id,
                                                  function->shared());
      unsigned pc_offset =
          FullCodeGenerator::PcField::decode(entry) + Code::kHeaderSize;
      ASSERT(pc_offset > 0);

      FrameSummary summary(receiver, function, code, pc_offset, is_constructor);
      frames->Add(summary);
      is_constructor = false;
    } else if (opcode == Translation::CONSTRUCT_STUB_FRAME) {
      // The next encountered JS_FRAME will be marked as a constructor call.
      it.Skip(Translation::NumberOfOperandsFor(opcode));
      ASSERT(!is_constructor);
      is_constructor = true;
    } else {
      // Skip over operands to advance to the next opcode.
      it.Skip(Translation::NumberOfOperandsFor(opcode));
    }
  }
  ASSERT(!is_constructor);
}


DeoptimizationInputData* OptimizedFrame::GetDeoptimizationData(
    int* deopt_index) {
  ASSERT(is_optimized());

  JSFunction* opt_function = JSFunction::cast(function());
  Code* code = opt_function->code();

  // The code object may have been replaced by lazy deoptimization. Fall
  // back to a slow search in this case to find the original optimized
  // code object.
  if (!code->contains(pc())) {
    code = isolate()->inner_pointer_to_code_cache()->
        GcSafeFindCodeForInnerPointer(pc());
  }
  ASSERT(code != NULL);
  ASSERT(code->kind() == Code::OPTIMIZED_FUNCTION);

  SafepointEntry safepoint_entry = code->GetSafepointEntry(pc());
  *deopt_index = safepoint_entry.deoptimization_index();
  ASSERT(*deopt_index != Safepoint::kNoDeoptimizationIndex);

  return DeoptimizationInputData::cast(code->deoptimization_data());
}


int OptimizedFrame::GetInlineCount() {
  ASSERT(is_optimized());

  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationInputData* data = GetDeoptimizationData(&deopt_index);

  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  ASSERT(opcode == Translation::BEGIN);
  USE(opcode);
  it.Next();  // Drop frame count.
  int jsframe_count = it.Next();
  return jsframe_count;
}


void OptimizedFrame::GetFunctions(List<JSFunction*>* functions) {
  ASSERT(functions->length() == 0);
  ASSERT(is_optimized());

  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationInputData* data = GetDeoptimizationData(&deopt_index);
  FixedArray* literal_array = data->LiteralArray();

  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  ASSERT(opcode == Translation::BEGIN);
  it.Next();  // Drop frame count.
  int jsframe_count = it.Next();

  // We insert the frames in reverse order because the frames
  // in the deoptimization translation are ordered bottom-to-top.
  while (jsframe_count > 0) {
    opcode = static_cast<Translation::Opcode>(it.Next());
    if (opcode == Translation::JS_FRAME) {
      jsframe_count--;
      it.Next();  // Skip ast id.
      JSFunction* function = LiteralAt(literal_array, it.Next());
      it.Next();  // Skip height.
      functions->Add(function);
    } else {
      // Skip over operands to advance to the next opcode.
      it.Skip(Translation::NumberOfOperandsFor(opcode));
    }
  }
}


int ArgumentsAdaptorFrame::GetNumberOfIncomingArguments() const {
  return Smi::cast(GetExpression(0))->value();
}


Address ArgumentsAdaptorFrame::GetCallerStackPointer() const {
  return fp() + StandardFrameConstants::kCallerSPOffset;
}


Address InternalFrame::GetCallerStackPointer() const {
  // Internal frames have no arguments. The stack pointer of the
  // caller is at a fixed offset from the frame pointer.
  return fp() + StandardFrameConstants::kCallerSPOffset;
}


Code* ArgumentsAdaptorFrame::unchecked_code() const {
  return isolate()->builtins()->builtin(
      Builtins::kArgumentsAdaptorTrampoline);
}


Code* InternalFrame::unchecked_code() const {
  const int offset = InternalFrameConstants::kCodeOffset;
  Object* code = Memory::Object_at(fp() + offset);
  ASSERT(code != NULL);
  return reinterpret_cast<Code*>(code);
}


void StackFrame::PrintIndex(StringStream* accumulator,
                            PrintMode mode,
                            int index) {
  accumulator->Add((mode == OVERVIEW) ? "%5d: " : "[%d]: ", index);
}


void JavaScriptFrame::Print(StringStream* accumulator,
                            PrintMode mode,
                            int index) const {
  HandleScope scope(isolate());
  Object* receiver = this->receiver();
  Object* function = this->function();

  accumulator->PrintSecurityTokenIfChanged(function);
  PrintIndex(accumulator, mode, index);
  Code* code = NULL;
  if (IsConstructor()) accumulator->Add("new ");
  accumulator->PrintFunction(function, receiver, &code);

  // Get scope information for nicer output, if possible. If code is NULL, or
  // doesn't contain scope info, scope_info will return 0 for the number of
  // parameters, stack local variables, context local variables, stack slots,
  // or context slots.
  Handle<ScopeInfo> scope_info(ScopeInfo::Empty(isolate()));

  if (function->IsJSFunction()) {
    Handle<SharedFunctionInfo> shared(JSFunction::cast(function)->shared());
    scope_info = Handle<ScopeInfo>(shared->scope_info());
    Object* script_obj = shared->script();
    if (script_obj->IsScript()) {
      Handle<Script> script(Script::cast(script_obj));
      accumulator->Add(" [");
      accumulator->PrintName(script->name());

      Address pc = this->pc();
      if (code != NULL && code->kind() == Code::FUNCTION &&
          pc >= code->instruction_start() && pc < code->instruction_end()) {
        int source_pos = code->SourcePosition(pc);
        int line = GetScriptLineNumberSafe(script, source_pos) + 1;
        accumulator->Add(":%d", line);
      } else {
        int function_start_pos = shared->start_position();
        int line = GetScriptLineNumberSafe(script, function_start_pos) + 1;
        accumulator->Add(":~%d", line);
      }

      accumulator->Add("] ");
    }
  }

  accumulator->Add("(this=%o", receiver);

  // Print the parameters.
  int parameters_count = ComputeParametersCount();
  for (int i = 0; i < parameters_count; i++) {
    accumulator->Add(",");
    // If we have a name for the parameter we print it. Nameless
    // parameters are either because we have more actual parameters
    // than formal parameters or because we have no scope information.
    if (i < scope_info->ParameterCount()) {
      accumulator->PrintName(scope_info->ParameterName(i));
      accumulator->Add("=");
    }
    accumulator->Add("%o", GetParameter(i));
  }

  accumulator->Add(")");
  if (mode == OVERVIEW) {
    accumulator->Add("\n");
    return;
  }
  if (is_optimized()) {
    accumulator->Add(" {\n// optimized frame\n}\n");
    return;
  }
  accumulator->Add(" {\n");

  // Compute the number of locals and expression stack elements.
  int stack_locals_count = scope_info->StackLocalCount();
  int heap_locals_count = scope_info->ContextLocalCount();
  int expressions_count = ComputeExpressionsCount();

  // Print stack-allocated local variables.
  if (stack_locals_count > 0) {
    accumulator->Add("  // stack-allocated locals\n");
  }
  for (int i = 0; i < stack_locals_count; i++) {
    accumulator->Add("  var ");
    accumulator->PrintName(scope_info->StackLocalName(i));
    accumulator->Add(" = ");
    if (i < expressions_count) {
      accumulator->Add("%o", GetExpression(i));
    } else {
      accumulator->Add("// no expression found - inconsistent frame?");
    }
    accumulator->Add("\n");
  }

  // Try to get hold of the context of this frame.
  Context* context = NULL;
  if (this->context() != NULL && this->context()->IsContext()) {
    context = Context::cast(this->context());
  }

  // Print heap-allocated local variables.
  if (heap_locals_count > 0) {
    accumulator->Add("  // heap-allocated locals\n");
  }
  for (int i = 0; i < heap_locals_count; i++) {
    accumulator->Add("  var ");
    accumulator->PrintName(scope_info->ContextLocalName(i));
    accumulator->Add(" = ");
    if (context != NULL) {
      if (i < context->length()) {
        accumulator->Add("%o", context->get(Context::MIN_CONTEXT_SLOTS + i));
      } else {
        accumulator->Add(
            "// warning: missing context slot - inconsistent frame?");
      }
    } else {
      accumulator->Add("// warning: no context found - inconsistent frame?");
    }
    accumulator->Add("\n");
  }

  // Print the expression stack.
  int expressions_start = stack_locals_count;
  if (expressions_start < expressions_count) {
    accumulator->Add("  // expression stack (top to bottom)\n");
  }
  for (int i = expressions_count - 1; i >= expressions_start; i--) {
    if (IsExpressionInsideHandler(i)) continue;
    accumulator->Add("  [%02d] : %o\n", i, GetExpression(i));
  }

  // Print details about the function.
  if (FLAG_max_stack_trace_source_length != 0 && code != NULL) {
    SharedFunctionInfo* shared = JSFunction::cast(function)->shared();
    accumulator->Add("--------- s o u r c e   c o d e ---------\n");
    shared->SourceCodePrint(accumulator, FLAG_max_stack_trace_source_length);
    accumulator->Add("\n-----------------------------------------\n");
  }

  accumulator->Add("}\n\n");
}


void ArgumentsAdaptorFrame::Print(StringStream* accumulator,
                                  PrintMode mode,
                                  int index) const {
  int actual = ComputeParametersCount();
  int expected = -1;
  Object* function = this->function();
  if (function->IsJSFunction()) {
    expected = JSFunction::cast(function)->shared()->formal_parameter_count();
  }

  PrintIndex(accumulator, mode, index);
  accumulator->Add("arguments adaptor frame: %d->%d", actual, expected);
  if (mode == OVERVIEW) {
    accumulator->Add("\n");
    return;
  }
  accumulator->Add(" {\n");

  // Print actual arguments.
  if (actual > 0) accumulator->Add("  // actual arguments\n");
  for (int i = 0; i < actual; i++) {
    accumulator->Add("  [%02d] : %o", i, GetParameter(i));
    if (expected != -1 && i >= expected) {
      accumulator->Add("  // not passed to callee");
    }
    accumulator->Add("\n");
  }

  accumulator->Add("}\n\n");
}


void EntryFrame::Iterate(ObjectVisitor* v) const {
  StackHandlerIterator it(this, top_handler());
  ASSERT(!it.done());
  StackHandler* handler = it.handler();
  ASSERT(handler->is_js_entry());
  handler->Iterate(v, LookupCode());
#ifdef DEBUG
  // Make sure that the entry frame does not contain more than one
  // stack handler.
  it.Advance();
  ASSERT(it.done());
#endif
  IteratePc(v, pc_address(), LookupCode());
}


void StandardFrame::IterateExpressions(ObjectVisitor* v) const {
  const int offset = StandardFrameConstants::kContextOffset;
  Object** base = &Memory::Object_at(sp());
  Object** limit = &Memory::Object_at(fp() + offset) + 1;
  for (StackHandlerIterator it(this, top_handler()); !it.done(); it.Advance()) {
    StackHandler* handler = it.handler();
    // Traverse pointers down to - but not including - the next
    // handler in the handler chain. Update the base to skip the
    // handler and allow the handler to traverse its own pointers.
    const Address address = handler->address();
    v->VisitPointers(base, reinterpret_cast<Object**>(address));
    base = reinterpret_cast<Object**>(address + StackHandlerConstants::kSize);
    // Traverse the pointers in the handler itself.
    handler->Iterate(v, LookupCode());
  }
  v->VisitPointers(base, limit);
}


void JavaScriptFrame::Iterate(ObjectVisitor* v) const {
  IterateExpressions(v);
  IteratePc(v, pc_address(), LookupCode());
}


void InternalFrame::Iterate(ObjectVisitor* v) const {
  // Internal frames only have object pointers on the expression stack
  // as they never have any arguments.
  IterateExpressions(v);
  IteratePc(v, pc_address(), LookupCode());
}


void StubFailureTrampolineFrame::Iterate(ObjectVisitor* v) const {
  Object** base = &Memory::Object_at(sp());
  Object** limit = &Memory::Object_at(fp() +
                                      kFirstRegisterParameterFrameOffset);
  v->VisitPointers(base, limit);
  base = &Memory::Object_at(fp() + StandardFrameConstants::kMarkerOffset);
  const int offset = StandardFrameConstants::kContextOffset;
  limit = &Memory::Object_at(fp() + offset) + 1;
  v->VisitPointers(base, limit);
  IteratePc(v, pc_address(), LookupCode());
}


Address StubFailureTrampolineFrame::GetCallerStackPointer() const {
  return fp() + StandardFrameConstants::kCallerSPOffset;
}


Code* StubFailureTrampolineFrame::unchecked_code() const {
  Code* trampoline;
  StubFailureTrampolineStub(NOT_JS_FUNCTION_STUB_MODE).
      FindCodeInCache(&trampoline, isolate());
  if (trampoline->contains(pc())) {
    return trampoline;
  }

  StubFailureTrampolineStub(JS_FUNCTION_STUB_MODE).
      FindCodeInCache(&trampoline, isolate());
  if (trampoline->contains(pc())) {
    return trampoline;
  }

  UNREACHABLE();
  return NULL;
}


// -------------------------------------------------------------------------


JavaScriptFrame* StackFrameLocator::FindJavaScriptFrame(int n) {
  ASSERT(n >= 0);
  for (int i = 0; i <= n; i++) {
    while (!iterator_.frame()->is_java_script()) iterator_.Advance();
    if (i == n) return JavaScriptFrame::cast(iterator_.frame());
    iterator_.Advance();
  }
  UNREACHABLE();
  return NULL;
}


// -------------------------------------------------------------------------


static Map* GcSafeMapOfCodeSpaceObject(HeapObject* object) {
  MapWord map_word = object->map_word();
  return map_word.IsForwardingAddress() ?
      map_word.ToForwardingAddress()->map() : map_word.ToMap();
}


static int GcSafeSizeOfCodeSpaceObject(HeapObject* object) {
  return object->SizeFromMap(GcSafeMapOfCodeSpaceObject(object));
}


#ifdef DEBUG
static bool GcSafeCodeContains(HeapObject* code, Address addr) {
  Map* map = GcSafeMapOfCodeSpaceObject(code);
  ASSERT(map == code->GetHeap()->code_map());
  Address start = code->address();
  Address end = code->address() + code->SizeFromMap(map);
  return start <= addr && addr < end;
}
#endif


Code* InnerPointerToCodeCache::GcSafeCastToCode(HeapObject* object,
                                                Address inner_pointer) {
  Code* code = reinterpret_cast<Code*>(object);
  ASSERT(code != NULL && GcSafeCodeContains(code, inner_pointer));
  return code;
}


Code* InnerPointerToCodeCache::GcSafeFindCodeForInnerPointer(
    Address inner_pointer) {
  Heap* heap = isolate_->heap();
  // Check if the inner pointer points into a large object chunk.
  LargePage* large_page = heap->lo_space()->FindPage(inner_pointer);
  if (large_page != NULL) {
    return GcSafeCastToCode(large_page->GetObject(), inner_pointer);
  }

  // Iterate through the page until we reach the end or find an object starting
  // after the inner pointer.
  Page* page = Page::FromAddress(inner_pointer);

  Address addr = page->skip_list()->StartFor(inner_pointer);

  Address top = heap->code_space()->top();
  Address limit = heap->code_space()->limit();

  while (true) {
    if (addr == top && addr != limit) {
      addr = limit;
      continue;
    }

    HeapObject* obj = HeapObject::FromAddress(addr);
    int obj_size = GcSafeSizeOfCodeSpaceObject(obj);
    Address next_addr = addr + obj_size;
    if (next_addr > inner_pointer) return GcSafeCastToCode(obj, inner_pointer);
    addr = next_addr;
  }
}


InnerPointerToCodeCache::InnerPointerToCodeCacheEntry*
    InnerPointerToCodeCache::GetCacheEntry(Address inner_pointer) {
  isolate_->counters()->pc_to_code()->Increment();
  ASSERT(IsPowerOf2(kInnerPointerToCodeCacheSize));
  uint32_t hash = ComputeIntegerHash(
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(inner_pointer)),
      v8::internal::kZeroHashSeed);
  uint32_t index = hash & (kInnerPointerToCodeCacheSize - 1);
  InnerPointerToCodeCacheEntry* entry = cache(index);
  if (entry->inner_pointer == inner_pointer) {
    isolate_->counters()->pc_to_code_cached()->Increment();
    ASSERT(entry->code == GcSafeFindCodeForInnerPointer(inner_pointer));
  } else {
    // Because this code may be interrupted by a profiling signal that
    // also queries the cache, we cannot update inner_pointer before the code
    // has been set. Otherwise, we risk trying to use a cache entry before
    // the code has been computed.
    entry->code = GcSafeFindCodeForInnerPointer(inner_pointer);
    entry->safepoint_entry.Reset();
    entry->inner_pointer = inner_pointer;
  }
  return entry;
}


// -------------------------------------------------------------------------


void StackHandler::Unwind(Isolate* isolate,
                          FixedArray* array,
                          int offset,
                          int previous_handler_offset) const {
  STATIC_ASSERT(StackHandlerConstants::kSlotCount == 5);
  ASSERT_LE(0, offset);
  ASSERT_GE(array->length(), offset + 5);
  // Unwinding a stack handler into an array chains it in the opposite
  // direction, re-using the "next" slot as a "previous" link, so that stack
  // handlers can be later re-wound in the correct order.  Decode the "state"
  // slot into "index" and "kind" and store them separately, using the fp slot.
  array->set(offset, Smi::FromInt(previous_handler_offset));        // next
  array->set(offset + 1, *code_address());                          // code
  array->set(offset + 2, Smi::FromInt(static_cast<int>(index())));  // state
  array->set(offset + 3, *context_address());                       // context
  array->set(offset + 4, Smi::FromInt(static_cast<int>(kind())));   // fp

  *isolate->handler_address() = next()->address();
}


int StackHandler::Rewind(Isolate* isolate,
                         FixedArray* array,
                         int offset,
                         Address fp) {
  STATIC_ASSERT(StackHandlerConstants::kSlotCount == 5);
  ASSERT_LE(0, offset);
  ASSERT_GE(array->length(), offset + 5);
  Smi* prev_handler_offset = Smi::cast(array->get(offset));
  Code* code = Code::cast(array->get(offset + 1));
  Smi* smi_index = Smi::cast(array->get(offset + 2));
  Object* context = array->get(offset + 3);
  Smi* smi_kind = Smi::cast(array->get(offset + 4));

  unsigned state = KindField::encode(static_cast<Kind>(smi_kind->value())) |
      IndexField::encode(static_cast<unsigned>(smi_index->value()));

  Memory::Address_at(address() + StackHandlerConstants::kNextOffset) =
      *isolate->handler_address();
  Memory::Object_at(address() + StackHandlerConstants::kCodeOffset) = code;
  Memory::uintptr_at(address() + StackHandlerConstants::kStateOffset) = state;
  Memory::Object_at(address() + StackHandlerConstants::kContextOffset) =
      context;
  Memory::Address_at(address() + StackHandlerConstants::kFPOffset) = fp;

  *isolate->handler_address() = address();

  return prev_handler_offset->value();
}


// -------------------------------------------------------------------------

int NumRegs(RegList reglist) {
  return CompilerIntrinsics::CountSetBits(reglist);
}


struct JSCallerSavedCodeData {
  int reg_code[kNumJSCallerSaved];
};

JSCallerSavedCodeData caller_saved_code_data;

void SetUpJSCallerSavedCodeData() {
  int i = 0;
  for (int r = 0; r < kNumRegs; r++)
    if ((kJSCallerSaved & (1 << r)) != 0)
      caller_saved_code_data.reg_code[i++] = r;

  ASSERT(i == kNumJSCallerSaved);
}

int JSCallerSavedCode(int n) {
  ASSERT(0 <= n && n < kNumJSCallerSaved);
  return caller_saved_code_data.reg_code[n];
}


#define DEFINE_WRAPPER(type, field)                              \
class field##_Wrapper : public ZoneObject {                      \
 public:  /* NOLINT */                                           \
  field##_Wrapper(const field& original) : frame_(original) {    \
  }                                                              \
  field frame_;                                                  \
};
STACK_FRAME_TYPE_LIST(DEFINE_WRAPPER)
#undef DEFINE_WRAPPER

static StackFrame* AllocateFrameCopy(StackFrame* frame, Zone* zone) {
#define FRAME_TYPE_CASE(type, field) \
  case StackFrame::type: { \
    field##_Wrapper* wrapper = \
        new(zone) field##_Wrapper(*(reinterpret_cast<field*>(frame))); \
    return &wrapper->frame_; \
  }

  switch (frame->type()) {
    STACK_FRAME_TYPE_LIST(FRAME_TYPE_CASE)
    default: UNREACHABLE();
  }
#undef FRAME_TYPE_CASE
  return NULL;
}

Vector<StackFrame*> CreateStackMap(Isolate* isolate, Zone* zone) {
  ZoneList<StackFrame*> list(10, zone);
  for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
    StackFrame* frame = AllocateFrameCopy(it.frame(), zone);
    list.Add(frame, zone);
  }
  return list.ToVector();
}


} }  // namespace v8::internal
