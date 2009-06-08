// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "frames-inl.h"
#include "mark-compact.h"
#include "scopeinfo.h"
#include "string-stream.h"
#include "top.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {

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

  bool done() { return handler_->address() > limit_; }
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
StackFrameIterator::StackFrameIterator()
    : STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON)
      frame_(NULL), handler_(NULL), thread_(Top::GetCurrentThread()),
      fp_(NULL), sp_(NULL), advance_(&StackFrameIterator::AdvanceWithHandler) {
  Reset();
}
StackFrameIterator::StackFrameIterator(ThreadLocalTop* t)
    : STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON)
      frame_(NULL), handler_(NULL), thread_(t),
      fp_(NULL), sp_(NULL), advance_(&StackFrameIterator::AdvanceWithHandler) {
  Reset();
}
StackFrameIterator::StackFrameIterator(bool use_top, Address fp, Address sp)
    : STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON)
      frame_(NULL), handler_(NULL),
      thread_(use_top ? Top::GetCurrentThread() : NULL),
      fp_(use_top ? NULL : fp), sp_(sp),
      advance_(use_top ? &StackFrameIterator::AdvanceWithHandler :
               &StackFrameIterator::AdvanceWithoutHandler) {
  if (use_top || fp != NULL) {
    Reset();
  }
  JavaScriptFrame_.DisableHeapAccess();
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
    type = ExitFrame::GetStateForFramePointer(Top::c_entry_fp(thread_), &state);
    handler_ = StackHandler::FromAddress(Top::handler(thread_));
  } else {
    ASSERT(fp_ != NULL);
    state.fp = fp_;
    state.sp = sp_;
    state.pc_address =
        reinterpret_cast<Address*>(StandardFrame::ComputePCAddress(fp_));
    type = StackFrame::ComputeType(&state);
    if (SingletonFor(type) == NULL) return;
  }
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


StackTraceFrameIterator::StackTraceFrameIterator() {
  if (!done() && !frame()->function()->IsJSFunction()) Advance();
}


void StackTraceFrameIterator::Advance() {
  while (true) {
    JavaScriptFrameIterator::Advance();
    if (done()) return;
    if (frame()->function()->IsJSFunction()) return;
  }
}


// -------------------------------------------------------------------------


SafeStackFrameIterator::SafeStackFrameIterator(
    Address fp, Address sp, Address low_bound, Address high_bound) :
    low_bound_(low_bound), high_bound_(high_bound),
    is_valid_top_(
        IsWithinBounds(low_bound, high_bound,
                       Top::c_entry_fp(Top::GetCurrentThread())) &&
        Top::handler(Top::GetCurrentThread()) != NULL),
    is_valid_fp_(IsWithinBounds(low_bound, high_bound, fp)),
    is_working_iterator_(is_valid_top_ || is_valid_fp_),
    iteration_done_(!is_working_iterator_),
    iterator_(is_valid_top_, is_valid_fp_ ? fp : NULL, sp) {
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
    if (!IsValidStackAddress(caller_fp)) {
      return false;
    }
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


#ifdef ENABLE_LOGGING_AND_PROFILING
SafeStackTraceFrameIterator::SafeStackTraceFrameIterator(
    Address fp, Address sp, Address low_bound, Address high_bound) :
    SafeJavaScriptFrameIterator(fp, sp, low_bound, high_bound) {
  if (!done() && !frame()->is_java_script()) Advance();
}


void SafeStackTraceFrameIterator::Advance() {
  while (true) {
    SafeJavaScriptFrameIterator::Advance();
    if (done()) return;
    if (frame()->is_java_script()) return;
  }
}
#endif


// -------------------------------------------------------------------------


void StackHandler::Cook(Code* code) {
  ASSERT(MarkCompactCollector::IsCompacting());
  ASSERT(code->contains(pc()));
  set_pc(AddressFrom<Address>(pc() - code->instruction_start()));
}


void StackHandler::Uncook(Code* code) {
  ASSERT(MarkCompactCollector::IsCompacting());
  set_pc(code->instruction_start() + OffsetFrom(pc()));
  ASSERT(code->contains(pc()));
}


// -------------------------------------------------------------------------


bool StackFrame::HasHandler() const {
  StackHandlerIterator it(this, top_handler());
  return !it.done();
}


void StackFrame::CookFramesForThread(ThreadLocalTop* thread) {
  // Only cooking frames when the collector is compacting and thus moving code
  // around.
  ASSERT(MarkCompactCollector::IsCompacting());
  ASSERT(!thread->stack_is_cooked());
  for (StackFrameIterator it(thread); !it.done(); it.Advance()) {
    it.frame()->Cook();
  }
  thread->set_stack_is_cooked(true);
}


void StackFrame::UncookFramesForThread(ThreadLocalTop* thread) {
  // Only uncooking frames when the collector is compacting and thus moving code
  // around.
  ASSERT(MarkCompactCollector::IsCompacting());
  ASSERT(thread->stack_is_cooked());
  for (StackFrameIterator it(thread); !it.done(); it.Advance()) {
    it.frame()->Uncook();
  }
  thread->set_stack_is_cooked(false);
}


void StackFrame::Cook() {
  Code* code = this->code();
  for (StackHandlerIterator it(this, top_handler()); !it.done(); it.Advance()) {
    it.handler()->Cook(code);
  }
  ASSERT(code->contains(pc()));
  set_pc(AddressFrom<Address>(pc() - code->instruction_start()));
}


void StackFrame::Uncook() {
  Code* code = this->code();
  for (StackHandlerIterator it(this, top_handler()); !it.done(); it.Advance()) {
    it.handler()->Uncook(code);
  }
  set_pc(code->instruction_start() + OffsetFrom(pc()));
  ASSERT(code->contains(pc()));
}


StackFrame::Type StackFrame::GetCallerState(State* state) const {
  ComputeCallerState(state);
  return ComputeType(state);
}


Code* EntryFrame::code() const {
  return Heap::js_entry_code();
}


void EntryFrame::ComputeCallerState(State* state) const {
  GetCallerState(state);
}


StackFrame::Type EntryFrame::GetCallerState(State* state) const {
  const int offset = EntryFrameConstants::kCallerFPOffset;
  Address fp = Memory::Address_at(this->fp() + offset);
  return ExitFrame::GetStateForFramePointer(fp, state);
}


Code* EntryConstructFrame::code() const {
  return Heap::js_construct_entry_code();
}


Code* ExitFrame::code() const {
  return Heap::c_entry_code();
}


void ExitFrame::ComputeCallerState(State* state) const {
  // Setup the caller state.
  state->sp = pp();
  state->fp = Memory::Address_at(fp() + ExitFrameConstants::kCallerFPOffset);
  state->pc_address
      = reinterpret_cast<Address*>(fp() + ExitFrameConstants::kCallerPCOffset);
}


Address ExitFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kPPDisplacement;
}


Code* ExitDebugFrame::code() const {
  return Heap::c_entry_debug_break_code();
}


Address StandardFrame::GetExpressionAddress(int n) const {
  const int offset = StandardFrameConstants::kExpressionsOffset;
  return fp() + offset - n * kPointerSize;
}


int StandardFrame::ComputeExpressionsCount() const {
  const int offset =
      StandardFrameConstants::kExpressionsOffset + kPointerSize;
  Address base = fp() + offset;
  Address limit = sp();
  ASSERT(base >= limit);  // stack grows downwards
  // Include register-allocated locals in number of expressions.
  return (base - limit) / kPointerSize;
}


void StandardFrame::ComputeCallerState(State* state) const {
  state->sp = caller_sp();
  state->fp = caller_fp();
  state->pc_address = reinterpret_cast<Address*>(ComputePCAddress(fp()));
}


bool StandardFrame::IsExpressionInsideHandler(int n) const {
  Address address = GetExpressionAddress(n);
  for (StackHandlerIterator it(this, top_handler()); !it.done(); it.Advance()) {
    if (it.handler()->includes(address)) return true;
  }
  return false;
}


Object* JavaScriptFrame::GetParameter(int index) const {
  ASSERT(index >= 0 && index < ComputeParametersCount());
  const int offset = JavaScriptFrameConstants::kParam0Offset;
  return Memory::Object_at(pp() + offset - (index * kPointerSize));
}


int JavaScriptFrame::ComputeParametersCount() const {
  Address base  = pp() + JavaScriptFrameConstants::kReceiverOffset;
  Address limit = fp() + JavaScriptFrameConstants::kSavedRegistersOffset;
  return (base - limit) / kPointerSize;
}


bool JavaScriptFrame::IsConstructor() const {
  Address fp = caller_fp();
  if (has_adapted_arguments()) {
    // Skip the arguments adaptor frame and look at the real caller.
    fp = Memory::Address_at(fp + StandardFrameConstants::kCallerFPOffset);
  }
  return IsConstructFrame(fp);
}


Code* JavaScriptFrame::code() const {
  JSFunction* function = JSFunction::cast(this->function());
  return function->shared()->code();
}


Code* ArgumentsAdaptorFrame::code() const {
  return Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline);
}


Code* InternalFrame::code() const {
  const int offset = InternalFrameConstants::kCodeOffset;
  Object* code = Memory::Object_at(fp() + offset);
  ASSERT(code != NULL);
  return Code::cast(code);
}


void StackFrame::PrintIndex(StringStream* accumulator,
                            PrintMode mode,
                            int index) {
  accumulator->Add((mode == OVERVIEW) ? "%5d: " : "[%d]: ", index);
}


void JavaScriptFrame::Print(StringStream* accumulator,
                            PrintMode mode,
                            int index) const {
  HandleScope scope;
  Object* receiver = this->receiver();
  Object* function = this->function();

  accumulator->PrintSecurityTokenIfChanged(function);
  PrintIndex(accumulator, mode, index);
  Code* code = NULL;
  if (IsConstructor()) accumulator->Add("new ");
  accumulator->PrintFunction(function, receiver, &code);
  accumulator->Add("(this=%o", receiver);

  // Get scope information for nicer output, if possible. If code is
  // NULL, or doesn't contain scope info, info will return 0 for the
  // number of parameters, stack slots, or context slots.
  ScopeInfo<PreallocatedStorage> info(code);

  // Print the parameters.
  int parameters_count = ComputeParametersCount();
  for (int i = 0; i < parameters_count; i++) {
    accumulator->Add(",");
    // If we have a name for the parameter we print it. Nameless
    // parameters are either because we have more actual parameters
    // than formal parameters or because we have no scope information.
    if (i < info.number_of_parameters()) {
      accumulator->PrintName(*info.parameter_name(i));
      accumulator->Add("=");
    }
    accumulator->Add("%o", GetParameter(i));
  }

  accumulator->Add(")");
  if (mode == OVERVIEW) {
    accumulator->Add("\n");
    return;
  }
  accumulator->Add(" {\n");

  // Compute the number of locals and expression stack elements.
  int stack_locals_count = info.number_of_stack_slots();
  int heap_locals_count = info.number_of_context_slots();
  int expressions_count = ComputeExpressionsCount();

  // Print stack-allocated local variables.
  if (stack_locals_count > 0) {
    accumulator->Add("  // stack-allocated locals\n");
  }
  for (int i = 0; i < stack_locals_count; i++) {
    accumulator->Add("  var ");
    accumulator->PrintName(*info.stack_slot_name(i));
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
  if (heap_locals_count > Context::MIN_CONTEXT_SLOTS) {
    accumulator->Add("  // heap-allocated locals\n");
  }
  for (int i = Context::MIN_CONTEXT_SLOTS; i < heap_locals_count; i++) {
    accumulator->Add("  var ");
    accumulator->PrintName(*info.context_slot_name(i));
    accumulator->Add(" = ");
    if (context != NULL) {
      if (i < context->length()) {
        accumulator->Add("%o", context->get(i));
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
  ASSERT(handler->is_entry());
  handler->Iterate(v);
  // Make sure that there's the entry frame does not contain more than
  // one stack handler.
#ifdef DEBUG
  it.Advance();
  ASSERT(it.done());
#endif
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
    handler->Iterate(v);
  }
  v->VisitPointers(base, limit);
}


void JavaScriptFrame::Iterate(ObjectVisitor* v) const {
  IterateExpressions(v);

  // Traverse callee-saved registers, receiver, and parameters.
  const int kBaseOffset = JavaScriptFrameConstants::kSavedRegistersOffset;
  const int kLimitOffset = JavaScriptFrameConstants::kReceiverOffset;
  Object** base = &Memory::Object_at(fp() + kBaseOffset);
  Object** limit = &Memory::Object_at(pp() + kLimitOffset) + 1;
  v->VisitPointers(base, limit);
}


void InternalFrame::Iterate(ObjectVisitor* v) const {
  // Internal frames only have object pointers on the expression stack
  // as they never have any arguments.
  IterateExpressions(v);
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


int NumRegs(RegList reglist) {
  int n = 0;
  while (reglist != 0) {
    n++;
    reglist &= reglist - 1;  // clear one bit
  }
  return n;
}


int JSCallerSavedCode(int n) {
  static int reg_code[kNumJSCallerSaved];
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    int i = 0;
    for (int r = 0; r < kNumRegs; r++)
      if ((kJSCallerSaved & (1 << r)) != 0)
        reg_code[i++] = r;

    ASSERT(i == kNumJSCallerSaved);
  }
  ASSERT(0 <= n && n < kNumJSCallerSaved);
  return reg_code[n];
}


} }  // namespace v8::internal
