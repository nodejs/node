// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/frames.h"

#include <memory>
#include <sstream>

#include "src/base/bits.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/register-configuration.h"
#include "src/safepoint-table.h"
#include "src/string-stream.h"
#include "src/vm-state-inl.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {

ReturnAddressLocationResolver
    StackFrame::return_address_location_resolver_ = NULL;


// Iterator that supports traversing the stack handlers of a
// particular frame. Needs to know the top of the handler chain.
class StackHandlerIterator BASE_EMBEDDED {
 public:
  StackHandlerIterator(const StackFrame* frame, StackHandler* handler)
      : limit_(frame->fp()), handler_(handler) {
    // Make sure the handler has already been unwound to this frame.
    DCHECK(frame->sp() <= handler->address());
  }

  StackHandler* handler() const { return handler_; }

  bool done() {
    return handler_ == NULL || handler_->address() > limit_;
  }
  void Advance() {
    DCHECK(!done());
    handler_ = handler_->next();
  }

 private:
  const Address limit_;
  StackHandler* handler_;
};


// -------------------------------------------------------------------------


#define INITIALIZE_SINGLETON(type, field) field##_(this),
StackFrameIteratorBase::StackFrameIteratorBase(Isolate* isolate,
                                               bool can_access_heap_objects)
    : isolate_(isolate),
      STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON)
      frame_(NULL), handler_(NULL),
      can_access_heap_objects_(can_access_heap_objects) {
}
#undef INITIALIZE_SINGLETON

StackFrameIterator::StackFrameIterator(Isolate* isolate)
    : StackFrameIterator(isolate, isolate->thread_local_top()) {}

StackFrameIterator::StackFrameIterator(Isolate* isolate, ThreadLocalTop* t)
    : StackFrameIteratorBase(isolate, true) {
  Reset(t);
}

void StackFrameIterator::Advance() {
  DCHECK(!done());
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
  DCHECK(!done() || handler_ == NULL);
}


void StackFrameIterator::Reset(ThreadLocalTop* top) {
  StackFrame::State state;
  StackFrame::Type type = ExitFrame::GetStateForFramePointer(
      Isolate::c_entry_fp(top), &state);
  handler_ = StackHandler::FromAddress(Isolate::handler(top));
  frame_ = SingletonFor(type, &state);
}


StackFrame* StackFrameIteratorBase::SingletonFor(StackFrame::Type type,
                                             StackFrame::State* state) {
  StackFrame* result = SingletonFor(type);
  DCHECK((!result) == (type == StackFrame::NONE));
  if (result) result->state_ = *state;
  return result;
}


StackFrame* StackFrameIteratorBase::SingletonFor(StackFrame::Type type) {
#define FRAME_TYPE_CASE(type, field) \
  case StackFrame::type:             \
    return &field##_;

  switch (type) {
    case StackFrame::NONE: return NULL;
    STACK_FRAME_TYPE_LIST(FRAME_TYPE_CASE)
    default: break;
  }
  return NULL;

#undef FRAME_TYPE_CASE
}

// -------------------------------------------------------------------------

JavaScriptFrameIterator::JavaScriptFrameIterator(Isolate* isolate,
                                                 StackFrame::Id id)
    : iterator_(isolate) {
  while (!done()) {
    Advance();
    if (frame()->id() == id) return;
  }
}


void JavaScriptFrameIterator::Advance() {
  do {
    iterator_.Advance();
  } while (!iterator_.done() && !iterator_.frame()->is_java_script());
}


void JavaScriptFrameIterator::AdvanceToArgumentsFrame() {
  if (!frame()->has_adapted_arguments()) return;
  iterator_.Advance();
  DCHECK(iterator_.frame()->is_arguments_adaptor());
}


// -------------------------------------------------------------------------

StackTraceFrameIterator::StackTraceFrameIterator(Isolate* isolate)
    : iterator_(isolate) {
  if (!done() && !IsValidFrame(iterator_.frame())) Advance();
}

StackTraceFrameIterator::StackTraceFrameIterator(Isolate* isolate,
                                                 StackFrame::Id id)
    : StackTraceFrameIterator(isolate) {
  while (!done() && frame()->id() != id) Advance();
}

void StackTraceFrameIterator::Advance() {
  do {
    iterator_.Advance();
  } while (!done() && !IsValidFrame(iterator_.frame()));
}

bool StackTraceFrameIterator::IsValidFrame(StackFrame* frame) const {
  if (frame->is_java_script()) {
    JavaScriptFrame* jsFrame = static_cast<JavaScriptFrame*>(frame);
    if (!jsFrame->function()->IsJSFunction()) return false;
    Object* script = jsFrame->function()->shared()->script();
    // Don't show functions from native scripts to user.
    return (script->IsScript() &&
            Script::TYPE_NATIVE != Script::cast(script)->type());
  }
  // apart from javascript, only wasm is valid
  return frame->is_wasm();
}

void StackTraceFrameIterator::AdvanceToArgumentsFrame() {
  if (!is_javascript() || !javascript_frame()->has_adapted_arguments()) return;
  iterator_.Advance();
  DCHECK(iterator_.frame()->is_arguments_adaptor());
}

// -------------------------------------------------------------------------


SafeStackFrameIterator::SafeStackFrameIterator(
    Isolate* isolate,
    Address fp, Address sp, Address js_entry_sp)
    : StackFrameIteratorBase(isolate, false),
      low_bound_(sp),
      high_bound_(js_entry_sp),
      top_frame_type_(StackFrame::NONE),
      external_callback_scope_(isolate->external_callback_scope()) {
  StackFrame::State state;
  StackFrame::Type type;
  ThreadLocalTop* top = isolate->thread_local_top();
  if (IsValidTop(top)) {
    type = ExitFrame::GetStateForFramePointer(Isolate::c_entry_fp(top), &state);
    top_frame_type_ = type;
  } else if (IsValidStackAddress(fp)) {
    DCHECK(fp != NULL);
    state.fp = fp;
    state.sp = sp;
    state.pc_address = StackFrame::ResolveReturnAddressLocation(
        reinterpret_cast<Address*>(StandardFrame::ComputePCAddress(fp)));
    // StackFrame::ComputeType will read both kContextOffset and kMarkerOffset,
    // we check only that kMarkerOffset is within the stack bounds and do
    // compile time check that kContextOffset slot is pushed on the stack before
    // kMarkerOffset.
    STATIC_ASSERT(StandardFrameConstants::kFunctionOffset <
                  StandardFrameConstants::kContextOffset);
    Address frame_marker = fp + StandardFrameConstants::kFunctionOffset;
    if (IsValidStackAddress(frame_marker)) {
      type = StackFrame::ComputeType(this, &state);
      top_frame_type_ = type;
    } else {
      // Mark the frame as JAVA_SCRIPT if we cannot determine its type.
      // The frame anyways will be skipped.
      type = StackFrame::JAVA_SCRIPT;
      // Top frame is incomplete so we cannot reliably determine its type.
      top_frame_type_ = StackFrame::NONE;
    }
  } else {
    return;
  }
  frame_ = SingletonFor(type, &state);
  if (frame_) Advance();
}


bool SafeStackFrameIterator::IsValidTop(ThreadLocalTop* top) const {
  Address c_entry_fp = Isolate::c_entry_fp(top);
  if (!IsValidExitFrame(c_entry_fp)) return false;
  // There should be at least one JS_ENTRY stack handler.
  Address handler = Isolate::handler(top);
  if (handler == NULL) return false;
  // Check that there are no js frames on top of the native frames.
  return c_entry_fp < handler;
}


void SafeStackFrameIterator::AdvanceOneFrame() {
  DCHECK(!done());
  StackFrame* last_frame = frame_;
  Address last_sp = last_frame->sp(), last_fp = last_frame->fp();
  // Before advancing to the next stack frame, perform pointer validity tests.
  if (!IsValidFrame(last_frame) || !IsValidCaller(last_frame)) {
    frame_ = NULL;
    return;
  }

  // Advance to the previous frame.
  StackFrame::State state;
  StackFrame::Type type = frame_->GetCallerState(&state);
  frame_ = SingletonFor(type, &state);
  if (!frame_) return;

  // Check that we have actually moved to the previous frame in the stack.
  if (frame_->sp() < last_sp || frame_->fp() < last_fp) {
    frame_ = NULL;
  }
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
    if (!IsValidExitFrame(caller_fp)) return false;
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
      SingletonFor(frame->GetCallerState(&state)) != NULL;
}


bool SafeStackFrameIterator::IsValidExitFrame(Address fp) const {
  if (!IsValidStackAddress(fp)) return false;
  Address sp = ExitFrame::ComputeStackPointer(fp);
  if (!IsValidStackAddress(sp)) return false;
  StackFrame::State state;
  ExitFrame::FillState(fp, sp, &state);
  MSAN_MEMORY_IS_INITIALIZED(state.pc_address, sizeof(state.pc_address));
  return *state.pc_address != nullptr;
}


void SafeStackFrameIterator::Advance() {
  while (true) {
    AdvanceOneFrame();
    if (done()) break;
    ExternalCallbackScope* last_callback_scope = NULL;
    while (external_callback_scope_ != NULL &&
           external_callback_scope_->scope_address() < frame_->fp()) {
      // As long as the setup of a frame is not atomic, we may happen to be
      // in an interval where an ExternalCallbackScope is already created,
      // but the frame is not yet entered. So we are actually observing
      // the previous frame.
      // Skip all the ExternalCallbackScope's that are below the current fp.
      last_callback_scope = external_callback_scope_;
      external_callback_scope_ = external_callback_scope_->previous();
    }
    if (frame_->is_java_script()) break;
    if (frame_->is_exit() || frame_->is_builtin_exit()) {
      // Some of the EXIT frames may have ExternalCallbackScope allocated on
      // top of them. In that case the scope corresponds to the first EXIT
      // frame beneath it. There may be other EXIT frames on top of the
      // ExternalCallbackScope, just skip them as we cannot collect any useful
      // information about them.
      if (last_callback_scope) {
        frame_->state_.pc_address =
            last_callback_scope->callback_entrypoint_address();
      }
      break;
    }
  }
}


// -------------------------------------------------------------------------


Code* StackFrame::GetSafepointData(Isolate* isolate,
                                   Address inner_pointer,
                                   SafepointEntry* safepoint_entry,
                                   unsigned* stack_slots) {
  InnerPointerToCodeCache::InnerPointerToCodeCacheEntry* entry =
      isolate->inner_pointer_to_code_cache()->GetCacheEntry(inner_pointer);
  if (!entry->safepoint_entry.is_valid()) {
    entry->safepoint_entry = entry->code->GetSafepointEntry(inner_pointer);
    DCHECK(entry->safepoint_entry.is_valid());
  } else {
    DCHECK(entry->safepoint_entry.Equals(
        entry->code->GetSafepointEntry(inner_pointer)));
  }

  // Fill in the results and return the code.
  Code* code = entry->code;
  *safepoint_entry = entry->safepoint_entry;
  *stack_slots = code->stack_slots();
  return code;
}


#ifdef DEBUG
static bool GcSafeCodeContains(HeapObject* object, Address addr);
#endif


void StackFrame::IteratePc(ObjectVisitor* v, Address* pc_address,
                           Address* constant_pool_address, Code* holder) {
  Address pc = *pc_address;
  DCHECK(GcSafeCodeContains(holder, pc));
  unsigned pc_offset = static_cast<unsigned>(pc - holder->instruction_start());
  Object* code = holder;
  v->VisitPointer(&code);
  if (code != holder) {
    holder = reinterpret_cast<Code*>(code);
    pc = holder->instruction_start() + pc_offset;
    *pc_address = pc;
    if (FLAG_enable_embedded_constant_pool && constant_pool_address) {
      *constant_pool_address = holder->constant_pool();
    }
  }
}


void StackFrame::SetReturnAddressLocationResolver(
    ReturnAddressLocationResolver resolver) {
  DCHECK(return_address_location_resolver_ == NULL);
  return_address_location_resolver_ = resolver;
}

static bool IsInterpreterFramePc(Isolate* isolate, Address pc) {
  Code* interpreter_entry_trampoline =
      isolate->builtins()->builtin(Builtins::kInterpreterEntryTrampoline);
  Code* interpreter_bytecode_dispatch =
      isolate->builtins()->builtin(Builtins::kInterpreterEnterBytecodeDispatch);
  Code* interpreter_baseline_on_return =
      isolate->builtins()->builtin(Builtins::kInterpreterMarkBaselineOnReturn);

  return (pc >= interpreter_entry_trampoline->instruction_start() &&
          pc < interpreter_entry_trampoline->instruction_end()) ||
         (pc >= interpreter_bytecode_dispatch->instruction_start() &&
          pc < interpreter_bytecode_dispatch->instruction_end()) ||
         (pc >= interpreter_baseline_on_return->instruction_start() &&
          pc < interpreter_baseline_on_return->instruction_end());
}

StackFrame::Type StackFrame::ComputeType(const StackFrameIteratorBase* iterator,
                                         State* state) {
  DCHECK(state->fp != NULL);

  MSAN_MEMORY_IS_INITIALIZED(
      state->fp + CommonFrameConstants::kContextOrFrameTypeOffset,
      kPointerSize);
  Object* marker = Memory::Object_at(
      state->fp + CommonFrameConstants::kContextOrFrameTypeOffset);
  if (!iterator->can_access_heap_objects_) {
    // TODO(titzer): "can_access_heap_objects" is kind of bogus. It really
    // means that we are being called from the profiler, which can interrupt
    // the VM with a signal at any arbitrary instruction, with essentially
    // anything on the stack. So basically none of these checks are 100%
    // reliable.
    MSAN_MEMORY_IS_INITIALIZED(
        state->fp + StandardFrameConstants::kFunctionOffset, kPointerSize);
    Object* maybe_function =
        Memory::Object_at(state->fp + StandardFrameConstants::kFunctionOffset);
    if (!marker->IsSmi()) {
      if (maybe_function->IsSmi()) {
        return NONE;
      } else if (FLAG_ignition && IsInterpreterFramePc(iterator->isolate(),
                                                       *(state->pc_address))) {
        return INTERPRETED;
      } else {
        return JAVA_SCRIPT;
      }
    }
  } else {
    // Look up the code object to figure out the type of the stack frame.
    Code* code_obj =
        GetContainingCode(iterator->isolate(), *(state->pc_address));
    if (code_obj != nullptr) {
      switch (code_obj->kind()) {
        case Code::BUILTIN:
          if (marker->IsSmi()) break;
          if (code_obj->is_interpreter_trampoline_builtin()) {
            return INTERPRETED;
          }
          if (code_obj->is_turbofanned()) {
            // TODO(bmeurer): We treat frames for BUILTIN Code objects as
            // OptimizedFrame for now (all the builtins with JavaScript
            // linkage are actually generated with TurboFan currently, so
            // this is sound).
            return OPTIMIZED;
          }
          return BUILTIN;
        case Code::FUNCTION:
          return JAVA_SCRIPT;
        case Code::OPTIMIZED_FUNCTION:
          return OPTIMIZED;
        case Code::WASM_FUNCTION:
          return WASM;
        case Code::WASM_TO_JS_FUNCTION:
          return WASM_TO_JS;
        case Code::JS_TO_WASM_FUNCTION:
          return JS_TO_WASM;
        default:
          // All other types should have an explicit marker
          break;
      }
    } else {
      return NONE;
    }
  }

  DCHECK(marker->IsSmi());
  StackFrame::Type candidate =
      static_cast<StackFrame::Type>(Smi::cast(marker)->value());
  switch (candidate) {
    case ENTRY:
    case ENTRY_CONSTRUCT:
    case EXIT:
    case BUILTIN_EXIT:
    case STUB:
    case STUB_FAILURE_TRAMPOLINE:
    case INTERNAL:
    case CONSTRUCT:
    case ARGUMENTS_ADAPTOR:
    case WASM_TO_JS:
    case WASM:
      return candidate;
    case JS_TO_WASM:
    case JAVA_SCRIPT:
    case OPTIMIZED:
    case INTERPRETED:
    default:
      // Unoptimized and optimized JavaScript frames, including
      // interpreted frames, should never have a StackFrame::Type
      // marker. If we find one, we're likely being called from the
      // profiler in a bogus stack frame.
      return NONE;
  }
}


#ifdef DEBUG
bool StackFrame::can_access_heap_objects() const {
  return iterator_->can_access_heap_objects_;
}
#endif


StackFrame::Type StackFrame::GetCallerState(State* state) const {
  ComputeCallerState(state);
  return ComputeType(iterator_, state);
}


Address StackFrame::UnpaddedFP() const {
  return fp();
}


Code* EntryFrame::unchecked_code() const {
  return isolate()->heap()->js_entry_code();
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
  return isolate()->heap()->js_construct_entry_code();
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
  if (FLAG_enable_embedded_constant_pool) {
    state->constant_pool_address = reinterpret_cast<Address*>(
        fp() + ExitFrameConstants::kConstantPoolOffset);
  }
}


void ExitFrame::SetCallerFp(Address caller_fp) {
  Memory::Address_at(fp() + ExitFrameConstants::kCallerFPOffset) = caller_fp;
}


void ExitFrame::Iterate(ObjectVisitor* v) const {
  // The arguments are traversed as part of the expression stack of
  // the calling frame.
  IteratePc(v, pc_address(), constant_pool_address(), LookupCode());
  v->VisitPointer(&code_slot());
}


Address ExitFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPOffset;
}


StackFrame::Type ExitFrame::GetStateForFramePointer(Address fp, State* state) {
  if (fp == 0) return NONE;
  Address sp = ComputeStackPointer(fp);
  FillState(fp, sp, state);
  DCHECK(*state->pc_address != NULL);

  return ComputeFrameType(fp);
}

StackFrame::Type ExitFrame::ComputeFrameType(Address fp) {
  // Distinguish between between regular and builtin exit frames.
  // Default to EXIT in all hairy cases (e.g., when called from profiler).
  const int offset = ExitFrameConstants::kFrameTypeOffset;
  Object* marker = Memory::Object_at(fp + offset);

  if (!marker->IsSmi()) {
    return EXIT;
  }

  StackFrame::Type frame_type =
      static_cast<StackFrame::Type>(Smi::cast(marker)->value());
  if (frame_type == EXIT || frame_type == BUILTIN_EXIT) {
    return frame_type;
  }

  return EXIT;
}

Address ExitFrame::ComputeStackPointer(Address fp) {
  MSAN_MEMORY_IS_INITIALIZED(fp + ExitFrameConstants::kSPOffset, kPointerSize);
  return Memory::Address_at(fp + ExitFrameConstants::kSPOffset);
}

void ExitFrame::FillState(Address fp, Address sp, State* state) {
  state->sp = sp;
  state->fp = fp;
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(sp - 1 * kPCOnStackSize));
  // The constant pool recorded in the exit frame is not associated
  // with the pc in this state (the return address into a C entry
  // stub).  ComputeCallerState will retrieve the constant pool
  // together with the associated caller pc.
  state->constant_pool_address = NULL;
}

JSFunction* BuiltinExitFrame::function() const {
  return JSFunction::cast(target_slot_object());
}

Object* BuiltinExitFrame::receiver() const { return receiver_slot_object(); }

bool BuiltinExitFrame::IsConstructor() const {
  return !new_target_slot_object()->IsUndefined(isolate());
}

Object* BuiltinExitFrame::GetParameter(int i) const {
  DCHECK(i >= 0 && i < ComputeParametersCount());
  int offset = BuiltinExitFrameConstants::kArgcOffset + (i + 1) * kPointerSize;
  return Memory::Object_at(fp() + offset);
}

int BuiltinExitFrame::ComputeParametersCount() const {
  Object* argc_slot = argc_slot_object();
  DCHECK(argc_slot->IsSmi());
  // Argc also counts the receiver, target, new target, and argc itself as args,
  // therefore the real argument count is argc - 4.
  int argc = Smi::cast(argc_slot)->value() - 4;
  DCHECK(argc >= 0);
  return argc;
}

void BuiltinExitFrame::Print(StringStream* accumulator, PrintMode mode,
                             int index) const {
  DisallowHeapAllocation no_gc;
  Object* receiver = this->receiver();
  JSFunction* function = this->function();

  accumulator->PrintSecurityTokenIfChanged(function);
  PrintIndex(accumulator, mode, index);
  accumulator->Add("builtin exit frame: ");
  Code* code = NULL;
  if (IsConstructor()) accumulator->Add("new ");
  accumulator->PrintFunction(function, receiver, &code);

  accumulator->Add("(this=%o", receiver);

  // Print the parameters.
  int parameters_count = ComputeParametersCount();
  for (int i = 0; i < parameters_count; i++) {
    accumulator->Add(",%o", GetParameter(i));
  }

  accumulator->Add(")\n\n");
}

Address StandardFrame::GetExpressionAddress(int n) const {
  const int offset = StandardFrameConstants::kExpressionsOffset;
  return fp() + offset - n * kPointerSize;
}

Address InterpretedFrame::GetExpressionAddress(int n) const {
  const int offset = InterpreterFrameConstants::kExpressionsOffset;
  return fp() + offset - n * kPointerSize;
}

Script* StandardFrame::script() const {
  // This should only be called on frames which override this method.
  DCHECK(false);
  return nullptr;
}

Object* StandardFrame::receiver() const {
  return isolate()->heap()->undefined_value();
}

Object* StandardFrame::context() const {
  return isolate()->heap()->undefined_value();
}

int StandardFrame::ComputeExpressionsCount() const {
  Address base = GetExpressionAddress(0);
  Address limit = sp() - kPointerSize;
  DCHECK(base >= limit);  // stack grows downwards
  // Include register-allocated locals in number of expressions.
  return static_cast<int>((base - limit) / kPointerSize);
}

Object* StandardFrame::GetParameter(int index) const {
  // StandardFrame does not define any parameters.
  UNREACHABLE();
  return nullptr;
}

int StandardFrame::ComputeParametersCount() const { return 0; }

void StandardFrame::ComputeCallerState(State* state) const {
  state->sp = caller_sp();
  state->fp = caller_fp();
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(ComputePCAddress(fp())));
  state->constant_pool_address =
      reinterpret_cast<Address*>(ComputeConstantPoolAddress(fp()));
}


void StandardFrame::SetCallerFp(Address caller_fp) {
  Memory::Address_at(fp() + StandardFrameConstants::kCallerFPOffset) =
      caller_fp;
}

bool StandardFrame::IsConstructor() const { return false; }

void StandardFrame::IterateCompiledFrame(ObjectVisitor* v) const {
  // Make sure that we're not doing "safe" stack frame iteration. We cannot
  // possibly find pointers in optimized frames in that state.
  DCHECK(can_access_heap_objects());

  // Compute the safepoint information.
  unsigned stack_slots = 0;
  SafepointEntry safepoint_entry;
  Code* code = StackFrame::GetSafepointData(
      isolate(), pc(), &safepoint_entry, &stack_slots);
  unsigned slot_space = stack_slots * kPointerSize;

  // Determine the fixed header and spill slot area size.
  int frame_header_size = StandardFrameConstants::kFixedFrameSizeFromFp;
  Object* marker =
      Memory::Object_at(fp() + CommonFrameConstants::kContextOrFrameTypeOffset);
  if (marker->IsSmi()) {
    StackFrame::Type candidate =
        static_cast<StackFrame::Type>(Smi::cast(marker)->value());
    switch (candidate) {
      case ENTRY:
      case ENTRY_CONSTRUCT:
      case EXIT:
      case BUILTIN_EXIT:
      case STUB_FAILURE_TRAMPOLINE:
      case ARGUMENTS_ADAPTOR:
      case STUB:
      case INTERNAL:
      case CONSTRUCT:
      case JS_TO_WASM:
      case WASM_TO_JS:
      case WASM:
        frame_header_size = TypedFrameConstants::kFixedFrameSizeFromFp;
        break;
      case JAVA_SCRIPT:
      case OPTIMIZED:
      case INTERPRETED:
      case BUILTIN:
        // These frame types have a context, but they are actually stored
        // in the place on the stack that one finds the frame type.
        UNREACHABLE();
        break;
      case NONE:
      case NUMBER_OF_TYPES:
      case MANUAL:
        UNREACHABLE();
        break;
    }
  }
  slot_space -=
      (frame_header_size + StandardFrameConstants::kFixedFrameSizeAboveFp);

  Object** frame_header_base = &Memory::Object_at(fp() - frame_header_size);
  Object** frame_header_limit =
      &Memory::Object_at(fp() - StandardFrameConstants::kCPSlotSize);
  Object** parameters_base = &Memory::Object_at(sp());
  Object** parameters_limit = frame_header_base - slot_space / kPointerSize;

  // Visit the parameters that may be on top of the saved registers.
  if (safepoint_entry.argument_count() > 0) {
    v->VisitPointers(parameters_base,
                     parameters_base + safepoint_entry.argument_count());
    parameters_base += safepoint_entry.argument_count();
  }

  // Skip saved double registers.
  if (safepoint_entry.has_doubles()) {
    // Number of doubles not known at snapshot time.
    DCHECK(!isolate()->serializer_enabled());
    parameters_base += RegisterConfiguration::Crankshaft()
                           ->num_allocatable_double_registers() *
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
  if (!is_js_to_wasm() && !is_wasm()) {
    // Non-WASM frames have tagged values as parameters.
    v->VisitPointers(parameters_base, parameters_limit);
  }

  // Visit pointer spill slots and locals.
  for (unsigned index = 0; index < stack_slots; index++) {
    int byte_index = index >> kBitsPerByteLog2;
    int bit_index = index & (kBitsPerByte - 1);
    if ((safepoint_bits[byte_index] & (1U << bit_index)) != 0) {
      v->VisitPointer(parameters_limit + index);
    }
  }

  // Visit the return address in the callee and incoming arguments.
  IteratePc(v, pc_address(), constant_pool_address(), code);

  if (!is_wasm() && !is_wasm_to_js()) {
    // Visit the context in stub frame and JavaScript frame.
    // Visit the function in JavaScript frame.
    v->VisitPointers(frame_header_base, frame_header_limit);
  }
}


void StubFrame::Iterate(ObjectVisitor* v) const {
  IterateCompiledFrame(v);
}


Code* StubFrame::unchecked_code() const {
  return static_cast<Code*>(isolate()->FindCodeObject(pc()));
}


Address StubFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPOffset;
}


int StubFrame::GetNumberOfIncomingArguments() const {
  return 0;
}


void OptimizedFrame::Iterate(ObjectVisitor* v) const {
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


bool JavaScriptFrame::HasInlinedFrames() const {
  List<JSFunction*> functions(1);
  GetFunctions(&functions);
  return functions.length() > 1;
}


int JavaScriptFrame::GetArgumentsLength() const {
  // If there is an arguments adaptor frame get the arguments length from it.
  if (has_adapted_arguments()) {
    return ArgumentsAdaptorFrame::GetLength(caller_fp());
  } else {
    return GetNumberOfIncomingArguments();
  }
}


Code* JavaScriptFrame::unchecked_code() const {
  return function()->code();
}


int JavaScriptFrame::GetNumberOfIncomingArguments() const {
  DCHECK(can_access_heap_objects() &&
         isolate()->heap()->gc_state() == Heap::NOT_IN_GC);

  return function()->shared()->internal_formal_parameter_count();
}


Address JavaScriptFrame::GetCallerStackPointer() const {
  return fp() + StandardFrameConstants::kCallerSPOffset;
}


void JavaScriptFrame::GetFunctions(List<JSFunction*>* functions) const {
  DCHECK(functions->length() == 0);
  functions->Add(function());
}

void JavaScriptFrame::Summarize(List<FrameSummary>* functions,
                                FrameSummary::Mode mode) const {
  DCHECK(functions->length() == 0);
  Code* code = LookupCode();
  int offset = static_cast<int>(pc() - code->instruction_start());
  AbstractCode* abstract_code = AbstractCode::cast(code);
  FrameSummary summary(receiver(), function(), abstract_code, offset,
                       IsConstructor(), mode);
  functions->Add(summary);
}

JSFunction* JavaScriptFrame::function() const {
  return JSFunction::cast(function_slot_object());
}

Object* JavaScriptFrame::receiver() const { return GetParameter(-1); }

Script* JavaScriptFrame::script() const {
  return Script::cast(function()->shared()->script());
}

Object* JavaScriptFrame::context() const {
  const int offset = StandardFrameConstants::kContextOffset;
  Object* maybe_result = Memory::Object_at(fp() + offset);
  DCHECK(!maybe_result->IsSmi());
  return maybe_result;
}

int JavaScriptFrame::LookupExceptionHandlerInTable(
    int* stack_depth, HandlerTable::CatchPrediction* prediction) {
  Code* code = LookupCode();
  DCHECK(!code->is_optimized_code());
  int pc_offset = static_cast<int>(pc() - code->entry());
  return code->LookupRangeInHandlerTable(pc_offset, stack_depth, prediction);
}

void JavaScriptFrame::PrintFunctionAndOffset(JSFunction* function, Code* code,
                                             Address pc, FILE* file,
                                             bool print_line_number) {
  PrintF(file, "%s", function->IsOptimized() ? "*" : "~");
  function->PrintName(file);
  int code_offset = static_cast<int>(pc - code->instruction_start());
  PrintF(file, "+%d", code_offset);
  if (print_line_number) {
    SharedFunctionInfo* shared = function->shared();
    int source_pos = AbstractCode::cast(code)->SourcePosition(code_offset);
    Object* maybe_script = shared->script();
    if (maybe_script->IsScript()) {
      Script* script = Script::cast(maybe_script);
      int line = script->GetLineNumber(source_pos) + 1;
      Object* script_name_raw = script->name();
      if (script_name_raw->IsString()) {
        String* script_name = String::cast(script->name());
        std::unique_ptr<char[]> c_script_name =
            script_name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
        PrintF(file, " at %s:%d", c_script_name.get(), line);
      } else {
        PrintF(file, " at <unknown>:%d", line);
      }
    } else {
      PrintF(file, " at <unknown>:<unknown>");
    }
  }
}


void JavaScriptFrame::PrintTop(Isolate* isolate, FILE* file, bool print_args,
                               bool print_line_number) {
  // constructor calls
  DisallowHeapAllocation no_allocation;
  JavaScriptFrameIterator it(isolate);
  while (!it.done()) {
    if (it.frame()->is_java_script()) {
      JavaScriptFrame* frame = it.frame();
      if (frame->IsConstructor()) PrintF(file, "new ");
      PrintFunctionAndOffset(frame->function(), frame->unchecked_code(),
                             frame->pc(), file, print_line_number);
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


void JavaScriptFrame::SaveOperandStack(FixedArray* store) const {
  int operands_count = store->length();
  DCHECK_LE(operands_count, ComputeOperandsCount());
  for (int i = 0; i < operands_count; i++) {
    store->set(i, GetOperand(i));
  }
}

Object* JavaScriptFrame::GetParameter(int index) const {
  return Memory::Object_at(GetParameterSlot(index));
}

int JavaScriptFrame::ComputeParametersCount() const {
  return GetNumberOfIncomingArguments();
}

namespace {

bool CannotDeoptFromAsmCode(Code* code, JSFunction* function) {
  return code->is_turbofanned() && function->shared()->asm_function() &&
         !FLAG_turbo_asm_deoptimization;
}

}  // namespace

FrameSummary::FrameSummary(Object* receiver, JSFunction* function,
                           AbstractCode* abstract_code, int code_offset,
                           bool is_constructor, Mode mode)
    : receiver_(receiver, function->GetIsolate()),
      function_(function),
      abstract_code_(abstract_code),
      code_offset_(code_offset),
      is_constructor_(is_constructor) {
  DCHECK(abstract_code->IsBytecodeArray() ||
         Code::cast(abstract_code)->kind() != Code::OPTIMIZED_FUNCTION ||
         CannotDeoptFromAsmCode(Code::cast(abstract_code), function) ||
         mode == kApproximateSummary);
}

FrameSummary FrameSummary::GetFirst(JavaScriptFrame* frame) {
  List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
  frame->Summarize(&frames);
  return frames.first();
}

void FrameSummary::Print() {
  PrintF("receiver: ");
  receiver_->ShortPrint();
  PrintF("\nfunction: ");
  function_->shared()->DebugName()->ShortPrint();
  PrintF("\ncode: ");
  abstract_code_->ShortPrint();
  if (abstract_code_->IsCode()) {
    Code* code = abstract_code_->GetCode();
    if (code->kind() == Code::FUNCTION) PrintF(" UNOPT ");
    if (code->kind() == Code::OPTIMIZED_FUNCTION) {
      if (function()->shared()->asm_function()) {
        DCHECK(CannotDeoptFromAsmCode(code, *function()));
        PrintF(" ASM ");
      } else {
        PrintF(" OPT (approximate)");
      }
    }
  } else {
    PrintF(" BYTECODE ");
  }
  PrintF("\npc: %d\n", code_offset_);
}

void OptimizedFrame::Summarize(List<FrameSummary>* frames,
                               FrameSummary::Mode mode) const {
  DCHECK(frames->length() == 0);
  DCHECK(is_optimized());

  // Delegate to JS frame in absence of turbofan deoptimization.
  // TODO(turbofan): Revisit once we support deoptimization across the board.
  Code* code = LookupCode();
  if (code->kind() == Code::BUILTIN ||
      CannotDeoptFromAsmCode(code, function())) {
    return JavaScriptFrame::Summarize(frames);
  }

  DisallowHeapAllocation no_gc;
  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationInputData* const data = GetDeoptimizationData(&deopt_index);
  if (deopt_index == Safepoint::kNoDeoptimizationIndex) {
    DCHECK(data == nullptr);
    if (mode == FrameSummary::kApproximateSummary) {
      return JavaScriptFrame::Summarize(frames, mode);
    }
    FATAL("Missing deoptimization information for OptimizedFrame::Summarize.");
  }
  FixedArray* const literal_array = data->LiteralArray();

  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode frame_opcode =
      static_cast<Translation::Opcode>(it.Next());
  DCHECK_EQ(Translation::BEGIN, frame_opcode);
  it.Next();  // Drop frame count.
  int jsframe_count = it.Next();

  // We create the summary in reverse order because the frames
  // in the deoptimization translation are ordered bottom-to-top.
  bool is_constructor = IsConstructor();
  while (jsframe_count != 0) {
    frame_opcode = static_cast<Translation::Opcode>(it.Next());
    if (frame_opcode == Translation::JS_FRAME ||
        frame_opcode == Translation::INTERPRETED_FRAME) {
      jsframe_count--;
      BailoutId const bailout_id = BailoutId(it.Next());
      SharedFunctionInfo* const shared_info =
          SharedFunctionInfo::cast(literal_array->get(it.Next()));
      it.Next();  // Skip height.

      // The translation commands are ordered and the function is always
      // at the first position, and the receiver is next.
      Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());

      // Get the correct function in the optimized frame.
      JSFunction* function;
      if (opcode == Translation::LITERAL) {
        function = JSFunction::cast(literal_array->get(it.Next()));
      } else {
        CHECK_EQ(opcode, Translation::STACK_SLOT);
        function = JSFunction::cast(StackSlotAt(it.Next()));
      }
      DCHECK_EQ(shared_info, function->shared());

      // If we are at a call, the receiver is always in a stack slot.
      // Otherwise we are not guaranteed to get the receiver value.
      opcode = static_cast<Translation::Opcode>(it.Next());

      // Get the correct receiver in the optimized frame.
      Object* receiver;
      if (opcode == Translation::LITERAL) {
        receiver = literal_array->get(it.Next());
      } else if (opcode == Translation::STACK_SLOT) {
        receiver = StackSlotAt(it.Next());
      } else {
        // The receiver is not in a stack slot nor in a literal.  We give up.
        it.Skip(Translation::NumberOfOperandsFor(opcode));
        // TODO(3029): Materializing a captured object (or duplicated
        // object) is hard, we return undefined for now. This breaks the
        // produced stack trace, as constructor frames aren't marked as
        // such anymore.
        receiver = isolate()->heap()->undefined_value();
      }

      AbstractCode* abstract_code;

      unsigned code_offset;
      if (frame_opcode == Translation::JS_FRAME) {
        Code* code = shared_info->code();
        DeoptimizationOutputData* const output_data =
            DeoptimizationOutputData::cast(code->deoptimization_data());
        unsigned const entry =
            Deoptimizer::GetOutputInfo(output_data, bailout_id, shared_info);
        code_offset = FullCodeGenerator::PcField::decode(entry);
        abstract_code = AbstractCode::cast(code);
      } else {
        DCHECK_EQ(frame_opcode, Translation::INTERPRETED_FRAME);
        // BailoutId points to the next bytecode in the bytecode aray. Subtract
        // 1 to get the end of current bytecode.
        code_offset = bailout_id.ToInt() - 1;
        abstract_code = AbstractCode::cast(shared_info->bytecode_array());
      }
      FrameSummary summary(receiver, function, abstract_code, code_offset,
                           is_constructor);
      frames->Add(summary);
      is_constructor = false;
    } else if (frame_opcode == Translation::CONSTRUCT_STUB_FRAME) {
      // The next encountered JS_FRAME will be marked as a constructor call.
      it.Skip(Translation::NumberOfOperandsFor(frame_opcode));
      DCHECK(!is_constructor);
      is_constructor = true;
    } else {
      // Skip over operands to advance to the next opcode.
      it.Skip(Translation::NumberOfOperandsFor(frame_opcode));
    }
  }
  DCHECK(!is_constructor);
}


int OptimizedFrame::LookupExceptionHandlerInTable(
    int* stack_slots, HandlerTable::CatchPrediction* prediction) {
  // We cannot perform exception prediction on optimized code. Instead, we need
  // to use FrameSummary to find the corresponding code offset in unoptimized
  // code to perform prediction there.
  DCHECK_NULL(prediction);
  Code* code = LookupCode();
  HandlerTable* table = HandlerTable::cast(code->handler_table());
  int pc_offset = static_cast<int>(pc() - code->entry());
  if (stack_slots) *stack_slots = code->stack_slots();
  return table->LookupReturn(pc_offset);
}


DeoptimizationInputData* OptimizedFrame::GetDeoptimizationData(
    int* deopt_index) const {
  DCHECK(is_optimized());

  JSFunction* opt_function = function();
  Code* code = opt_function->code();

  // The code object may have been replaced by lazy deoptimization. Fall
  // back to a slow search in this case to find the original optimized
  // code object.
  if (!code->contains(pc())) {
    code = isolate()->inner_pointer_to_code_cache()->
        GcSafeFindCodeForInnerPointer(pc());
  }
  DCHECK(code != NULL);
  DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);

  SafepointEntry safepoint_entry = code->GetSafepointEntry(pc());
  *deopt_index = safepoint_entry.deoptimization_index();
  if (*deopt_index != Safepoint::kNoDeoptimizationIndex) {
    return DeoptimizationInputData::cast(code->deoptimization_data());
  }
  return nullptr;
}


void OptimizedFrame::GetFunctions(List<JSFunction*>* functions) const {
  DCHECK(functions->length() == 0);
  DCHECK(is_optimized());

  // Delegate to JS frame in absence of turbofan deoptimization.
  // TODO(turbofan): Revisit once we support deoptimization across the board.
  Code* code = LookupCode();
  if (code->kind() == Code::BUILTIN ||
      CannotDeoptFromAsmCode(code, function())) {
    return JavaScriptFrame::GetFunctions(functions);
  }

  DisallowHeapAllocation no_gc;
  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationInputData* const data = GetDeoptimizationData(&deopt_index);
  DCHECK_NOT_NULL(data);
  DCHECK_NE(Safepoint::kNoDeoptimizationIndex, deopt_index);
  FixedArray* const literal_array = data->LiteralArray();

  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  DCHECK_EQ(Translation::BEGIN, opcode);
  it.Next();  // Skip frame count.
  int jsframe_count = it.Next();

  // We insert the frames in reverse order because the frames
  // in the deoptimization translation are ordered bottom-to-top.
  while (jsframe_count != 0) {
    opcode = static_cast<Translation::Opcode>(it.Next());
    // Skip over operands to advance to the next opcode.
    it.Skip(Translation::NumberOfOperandsFor(opcode));
    if (opcode == Translation::JS_FRAME ||
        opcode == Translation::INTERPRETED_FRAME) {
      jsframe_count--;

      // The translation commands are ordered and the function is always at the
      // first position.
      opcode = static_cast<Translation::Opcode>(it.Next());

      // Get the correct function in the optimized frame.
      Object* function;
      if (opcode == Translation::LITERAL) {
        function = literal_array->get(it.Next());
      } else {
        CHECK_EQ(Translation::STACK_SLOT, opcode);
        function = StackSlotAt(it.Next());
      }
      functions->Add(JSFunction::cast(function));
    }
  }
}


int OptimizedFrame::StackSlotOffsetRelativeToFp(int slot_index) {
  return StandardFrameConstants::kCallerSPOffset -
         ((slot_index + 1) * kPointerSize);
}


Object* OptimizedFrame::StackSlotAt(int index) const {
  return Memory::Object_at(fp() + StackSlotOffsetRelativeToFp(index));
}

int InterpretedFrame::LookupExceptionHandlerInTable(
    int* context_register, HandlerTable::CatchPrediction* prediction) {
  BytecodeArray* bytecode = function()->shared()->bytecode_array();
  return bytecode->LookupRangeInHandlerTable(GetBytecodeOffset(),
                                             context_register, prediction);
}

int InterpretedFrame::GetBytecodeOffset() const {
  const int index = InterpreterFrameConstants::kBytecodeOffsetExpressionIndex;
  DCHECK_EQ(
      InterpreterFrameConstants::kBytecodeOffsetFromFp,
      InterpreterFrameConstants::kExpressionsOffset - index * kPointerSize);
  int raw_offset = Smi::cast(GetExpression(index))->value();
  return raw_offset - BytecodeArray::kHeaderSize + kHeapObjectTag;
}

void InterpretedFrame::PatchBytecodeOffset(int new_offset) {
  const int index = InterpreterFrameConstants::kBytecodeOffsetExpressionIndex;
  DCHECK_EQ(
      InterpreterFrameConstants::kBytecodeOffsetFromFp,
      InterpreterFrameConstants::kExpressionsOffset - index * kPointerSize);
  int raw_offset = new_offset + BytecodeArray::kHeaderSize - kHeapObjectTag;
  SetExpression(index, Smi::FromInt(raw_offset));
}

BytecodeArray* InterpretedFrame::GetBytecodeArray() const {
  const int index = InterpreterFrameConstants::kBytecodeArrayExpressionIndex;
  DCHECK_EQ(
      InterpreterFrameConstants::kBytecodeArrayFromFp,
      InterpreterFrameConstants::kExpressionsOffset - index * kPointerSize);
  return BytecodeArray::cast(GetExpression(index));
}

void InterpretedFrame::PatchBytecodeArray(BytecodeArray* bytecode_array) {
  const int index = InterpreterFrameConstants::kBytecodeArrayExpressionIndex;
  DCHECK_EQ(
      InterpreterFrameConstants::kBytecodeArrayFromFp,
      InterpreterFrameConstants::kExpressionsOffset - index * kPointerSize);
  SetExpression(index, bytecode_array);
}

Object* InterpretedFrame::ReadInterpreterRegister(int register_index) const {
  const int index = InterpreterFrameConstants::kRegisterFileExpressionIndex;
  DCHECK_EQ(
      InterpreterFrameConstants::kRegisterFileFromFp,
      InterpreterFrameConstants::kExpressionsOffset - index * kPointerSize);
  return GetExpression(index + register_index);
}

void InterpretedFrame::WriteInterpreterRegister(int register_index,
                                                Object* value) {
  const int index = InterpreterFrameConstants::kRegisterFileExpressionIndex;
  DCHECK_EQ(
      InterpreterFrameConstants::kRegisterFileFromFp,
      InterpreterFrameConstants::kExpressionsOffset - index * kPointerSize);
  return SetExpression(index + register_index, value);
}

void InterpretedFrame::Summarize(List<FrameSummary>* functions,
                                 FrameSummary::Mode mode) const {
  DCHECK(functions->length() == 0);
  AbstractCode* abstract_code =
      AbstractCode::cast(function()->shared()->bytecode_array());
  FrameSummary summary(receiver(), function(), abstract_code,
                       GetBytecodeOffset(), IsConstructor());
  functions->Add(summary);
}

int ArgumentsAdaptorFrame::GetNumberOfIncomingArguments() const {
  return Smi::cast(GetExpression(0))->value();
}

int ArgumentsAdaptorFrame::GetLength(Address fp) {
  const int offset = ArgumentsAdaptorFrameConstants::kLengthOffset;
  return Smi::cast(Memory::Object_at(fp + offset))->value();
}

Code* ArgumentsAdaptorFrame::unchecked_code() const {
  return isolate()->builtins()->builtin(
      Builtins::kArgumentsAdaptorTrampoline);
}

int BuiltinFrame::GetNumberOfIncomingArguments() const {
  return Smi::cast(GetExpression(0))->value();
}

void BuiltinFrame::PrintFrameKind(StringStream* accumulator) const {
  accumulator->Add("builtin frame: ");
}

Address InternalFrame::GetCallerStackPointer() const {
  // Internal frames have no arguments. The stack pointer of the
  // caller is at a fixed offset from the frame pointer.
  return fp() + StandardFrameConstants::kCallerSPOffset;
}

Code* InternalFrame::unchecked_code() const {
  const int offset = InternalFrameConstants::kCodeOffset;
  Object* code = Memory::Object_at(fp() + offset);
  DCHECK(code != NULL);
  return reinterpret_cast<Code*>(code);
}


void StackFrame::PrintIndex(StringStream* accumulator,
                            PrintMode mode,
                            int index) {
  accumulator->Add((mode == OVERVIEW) ? "%5d: " : "[%d]: ", index);
}

void WasmFrame::Print(StringStream* accumulator, PrintMode mode,
                      int index) const {
  accumulator->Add("wasm frame");
}

Code* WasmFrame::unchecked_code() const {
  return static_cast<Code*>(isolate()->FindCodeObject(pc()));
}

void WasmFrame::Iterate(ObjectVisitor* v) const { IterateCompiledFrame(v); }

Address WasmFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPOffset;
}

Object* WasmFrame::wasm_obj() const {
  Object* ret = wasm::GetOwningWasmInstance(LookupCode());
  if (ret == nullptr) ret = *(isolate()->factory()->undefined_value());
  return ret;
}

uint32_t WasmFrame::function_index() const {
  FixedArray* deopt_data = LookupCode()->deoptimization_data();
  DCHECK(deopt_data->length() == 2);
  return Smi::cast(deopt_data->get(1))->value();
}

Script* WasmFrame::script() const {
  Handle<JSObject> wasm(JSObject::cast(wasm_obj()), isolate());
  Handle<wasm::WasmDebugInfo> debug_info = wasm::GetDebugInfo(wasm);
  return wasm::WasmDebugInfo::GetFunctionScript(debug_info, function_index());
}

int WasmFrame::LookupExceptionHandlerInTable(int* stack_slots) {
  DCHECK_NOT_NULL(stack_slots);
  Code* code = LookupCode();
  HandlerTable* table = HandlerTable::cast(code->handler_table());
  int pc_offset = static_cast<int>(pc() - code->entry());
  *stack_slots = code->stack_slots();
  return table->LookupReturn(pc_offset);
}

namespace {


void PrintFunctionSource(StringStream* accumulator, SharedFunctionInfo* shared,
                         Code* code) {
  if (FLAG_max_stack_trace_source_length != 0 && code != NULL) {
    std::ostringstream os;
    os << "--------- s o u r c e   c o d e ---------\n"
       << SourceCodeOf(shared, FLAG_max_stack_trace_source_length)
       << "\n-----------------------------------------\n";
    accumulator->Add(os.str().c_str());
  }
}


}  // namespace


void JavaScriptFrame::Print(StringStream* accumulator,
                            PrintMode mode,
                            int index) const {
  DisallowHeapAllocation no_gc;
  Object* receiver = this->receiver();
  JSFunction* function = this->function();

  accumulator->PrintSecurityTokenIfChanged(function);
  PrintIndex(accumulator, mode, index);
  PrintFrameKind(accumulator);
  Code* code = NULL;
  if (IsConstructor()) accumulator->Add("new ");
  accumulator->PrintFunction(function, receiver, &code);

  // Get scope information for nicer output, if possible. If code is NULL, or
  // doesn't contain scope info, scope_info will return 0 for the number of
  // parameters, stack local variables, context local variables, stack slots,
  // or context slots.
  SharedFunctionInfo* shared = function->shared();
  ScopeInfo* scope_info = shared->scope_info();
  Object* script_obj = shared->script();
  if (script_obj->IsScript()) {
    Script* script = Script::cast(script_obj);
    accumulator->Add(" [");
    accumulator->PrintName(script->name());

    Address pc = this->pc();
    if (code != NULL && code->kind() == Code::FUNCTION &&
        pc >= code->instruction_start() && pc < code->instruction_end()) {
      int offset = static_cast<int>(pc - code->instruction_start());
      int source_pos = AbstractCode::cast(code)->SourcePosition(offset);
      int line = script->GetLineNumber(source_pos) + 1;
      accumulator->Add(":%d] [pc=%p]", line, pc);
    } else if (is_interpreted()) {
      const InterpretedFrame* iframe =
          reinterpret_cast<const InterpretedFrame*>(this);
      BytecodeArray* bytecodes = iframe->GetBytecodeArray();
      int offset = iframe->GetBytecodeOffset();
      int source_pos = AbstractCode::cast(bytecodes)->SourcePosition(offset);
      int line = script->GetLineNumber(source_pos) + 1;
      accumulator->Add(":%d] [bytecode=%p offset=%d]", line, bytecodes, offset);
    } else {
      int function_start_pos = shared->start_position();
      int line = script->GetLineNumber(function_start_pos) + 1;
      accumulator->Add(":~%d] [pc=%p]", line, pc);
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
    accumulator->Add(" {\n// optimized frame\n");
    PrintFunctionSource(accumulator, shared, code);
    accumulator->Add("}\n");
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
  while (context->IsWithContext()) {
    context = context->previous();
    DCHECK(context != NULL);
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
      int index = Context::MIN_CONTEXT_SLOTS + i;
      if (index < context->length()) {
        accumulator->Add("%o", context->get(index));
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
    accumulator->Add("  [%02d] : %o\n", i, GetExpression(i));
  }

  PrintFunctionSource(accumulator, shared, code);

  accumulator->Add("}\n\n");
}


void ArgumentsAdaptorFrame::Print(StringStream* accumulator,
                                  PrintMode mode,
                                  int index) const {
  int actual = ComputeParametersCount();
  int expected = -1;
  JSFunction* function = this->function();
  expected = function->shared()->internal_formal_parameter_count();

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
  IteratePc(v, pc_address(), constant_pool_address(), LookupCode());
}


void StandardFrame::IterateExpressions(ObjectVisitor* v) const {
  const int offset = StandardFrameConstants::kLastObjectOffset;
  Object** base = &Memory::Object_at(sp());
  Object** limit = &Memory::Object_at(fp() + offset) + 1;
  v->VisitPointers(base, limit);
}


void JavaScriptFrame::Iterate(ObjectVisitor* v) const {
  IterateExpressions(v);
  IteratePc(v, pc_address(), constant_pool_address(), LookupCode());
}

void InternalFrame::Iterate(ObjectVisitor* v) const {
  // Internal frames only have object pointers on the expression stack
  // as they never have any arguments.
  IterateExpressions(v);
  IteratePc(v, pc_address(), constant_pool_address(), LookupCode());
}


void StubFailureTrampolineFrame::Iterate(ObjectVisitor* v) const {
  Object** base = &Memory::Object_at(sp());
  Object** limit = &Memory::Object_at(
      fp() + StubFailureTrampolineFrameConstants::kFixedHeaderBottomOffset);
  v->VisitPointers(base, limit);
  base = &Memory::Object_at(fp() + StandardFrameConstants::kFunctionOffset);
  const int offset = StandardFrameConstants::kLastObjectOffset;
  limit = &Memory::Object_at(fp() + offset) + 1;
  v->VisitPointers(base, limit);
  IteratePc(v, pc_address(), constant_pool_address(), LookupCode());
}


Address StubFailureTrampolineFrame::GetCallerStackPointer() const {
  return fp() + StandardFrameConstants::kCallerSPOffset;
}


Code* StubFailureTrampolineFrame::unchecked_code() const {
  Code* trampoline;
  StubFailureTrampolineStub(isolate(), NOT_JS_FUNCTION_STUB_MODE).
      FindCodeInCache(&trampoline);
  if (trampoline->contains(pc())) {
    return trampoline;
  }

  StubFailureTrampolineStub(isolate(), JS_FUNCTION_STUB_MODE).
      FindCodeInCache(&trampoline);
  if (trampoline->contains(pc())) {
    return trampoline;
  }

  UNREACHABLE();
  return NULL;
}


// -------------------------------------------------------------------------


JavaScriptFrame* StackFrameLocator::FindJavaScriptFrame(int n) {
  DCHECK(n >= 0);
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
  DCHECK(map == code->GetHeap()->code_map());
  Address start = code->address();
  Address end = code->address() + code->SizeFromMap(map);
  return start <= addr && addr < end;
}
#endif


Code* InnerPointerToCodeCache::GcSafeCastToCode(HeapObject* object,
                                                Address inner_pointer) {
  Code* code = reinterpret_cast<Code*>(object);
  DCHECK(code != NULL && GcSafeCodeContains(code, inner_pointer));
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

  if (!heap->code_space()->Contains(inner_pointer)) {
    return nullptr;
  }

  // Iterate through the page until we reach the end or find an object starting
  // after the inner pointer.
  Page* page = Page::FromAddress(inner_pointer);

  DCHECK_EQ(page->owner(), heap->code_space());
  heap->mark_compact_collector()->sweeper().SweepOrWaitUntilSweepingCompleted(
      page);

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
  DCHECK(base::bits::IsPowerOfTwo32(kInnerPointerToCodeCacheSize));
  uint32_t hash = ComputeIntegerHash(ObjectAddressForHashing(inner_pointer),
                                     v8::internal::kZeroHashSeed);
  uint32_t index = hash & (kInnerPointerToCodeCacheSize - 1);
  InnerPointerToCodeCacheEntry* entry = cache(index);
  if (entry->inner_pointer == inner_pointer) {
    isolate_->counters()->pc_to_code_cached()->Increment();
    DCHECK(entry->code == GcSafeFindCodeForInnerPointer(inner_pointer));
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


int NumRegs(RegList reglist) { return base::bits::CountPopulation(reglist); }


struct JSCallerSavedCodeData {
  int reg_code[kNumJSCallerSaved];
};

JSCallerSavedCodeData caller_saved_code_data;

void SetUpJSCallerSavedCodeData() {
  int i = 0;
  for (int r = 0; r < kNumRegs; r++)
    if ((kJSCallerSaved & (1 << r)) != 0)
      caller_saved_code_data.reg_code[i++] = r;

  DCHECK(i == kNumJSCallerSaved);
}


int JSCallerSavedCode(int n) {
  DCHECK(0 <= n && n < kNumJSCallerSaved);
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


}  // namespace internal
}  // namespace v8
