// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/frames.h"

#include <memory>
#include <sstream>

#include "src/base/bits.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/ic/ic-stats.h"
#include "src/register-configuration.h"
#include "src/safepoint-table.h"
#include "src/string-stream.h"
#include "src/visitors.h"
#include "src/vm-state-inl.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

ReturnAddressLocationResolver StackFrame::return_address_location_resolver_ =
    nullptr;

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

  bool done() { return handler_ == nullptr || handler_->address() > limit_; }
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
      STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON) frame_(nullptr),
      handler_(nullptr),
      can_access_heap_objects_(can_access_heap_objects) {}
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
  DCHECK(!done() || handler_ == nullptr);
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
    case StackFrame::NONE:
      return nullptr;
      STACK_FRAME_TYPE_LIST(FRAME_TYPE_CASE)
    default: break;
  }
  return nullptr;

#undef FRAME_TYPE_CASE
}

// -------------------------------------------------------------------------

void JavaScriptFrameIterator::Advance() {
  do {
    iterator_.Advance();
  } while (!iterator_.done() && !iterator_.frame()->is_java_script());
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
    return jsFrame->function()->shared()->IsSubjectToDebugging();
  }
  // apart from javascript, only wasm is valid
  return frame->is_wasm();
}

// -------------------------------------------------------------------------

namespace {

bool IsInterpreterFramePc(Isolate* isolate, Address pc,
                          StackFrame::State* state) {
  Code* interpreter_entry_trampoline =
      isolate->builtins()->builtin(Builtins::kInterpreterEntryTrampoline);
  Code* interpreter_bytecode_advance =
      isolate->builtins()->builtin(Builtins::kInterpreterEnterBytecodeAdvance);
  Code* interpreter_bytecode_dispatch =
      isolate->builtins()->builtin(Builtins::kInterpreterEnterBytecodeDispatch);

  if (interpreter_entry_trampoline->contains(pc) ||
      interpreter_bytecode_advance->contains(pc) ||
      interpreter_bytecode_dispatch->contains(pc)) {
    return true;
  } else if (FLAG_interpreted_frames_native_stack) {
    intptr_t marker = Memory::intptr_at(
        state->fp + CommonFrameConstants::kContextOrFrameTypeOffset);
    MSAN_MEMORY_IS_INITIALIZED(
        state->fp + StandardFrameConstants::kFunctionOffset, kPointerSize);
    Object* maybe_function =
        Memory::Object_at(state->fp + StandardFrameConstants::kFunctionOffset);
    // There's no need to run a full ContainsSlow if we know the frame can't be
    // an InterpretedFrame,  so we do these fast checks first
    if (StackFrame::IsTypeMarker(marker) || maybe_function->IsSmi()) {
      return false;
    } else if (!isolate->heap()->code_space()->ContainsSlow(pc)) {
      return false;
    }
    interpreter_entry_trampoline =
        isolate->heap()->GcSafeFindCodeForInnerPointer(pc);
    return interpreter_entry_trampoline->is_interpreter_trampoline_builtin();
  } else {
    return false;
  }
}

DISABLE_ASAN Address ReadMemoryAt(Address address) {
  return Memory::Address_at(address);
}

WasmInstanceObject* LookupWasmInstanceObjectFromStandardFrame(
    const StandardFrame* frame) {
  // TODO(titzer): WASM instances cannot be found from the code in the future.
  WasmInstanceObject* ret = WasmInstanceObject::GetOwningInstance(
      frame->isolate()->wasm_engine()->code_manager()->LookupCode(frame->pc()));
  // This is a live stack frame, there must be a live wasm instance available.
  DCHECK_NOT_NULL(ret);
  return ret;
}

}  // namespace

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
  bool advance_frame = true;
  if (IsValidTop(top)) {
    type = ExitFrame::GetStateForFramePointer(Isolate::c_entry_fp(top), &state);
    top_frame_type_ = type;
  } else if (IsValidStackAddress(fp)) {
    DCHECK_NOT_NULL(fp);
    state.fp = fp;
    state.sp = sp;
    state.pc_address = StackFrame::ResolveReturnAddressLocation(
        reinterpret_cast<Address*>(StandardFrame::ComputePCAddress(fp)));

    // If the top of stack is a return address to the interpreter trampoline,
    // then we are likely in a bytecode handler with elided frame. In that
    // case, set the PC properly and make sure we do not drop the frame.
    if (IsValidStackAddress(sp)) {
      MSAN_MEMORY_IS_INITIALIZED(sp, kPointerSize);
      Address tos = ReadMemoryAt(reinterpret_cast<Address>(sp));
      if (IsInterpreterFramePc(isolate, tos, &state)) {
        state.pc_address = reinterpret_cast<Address*>(sp);
        advance_frame = false;
      }
    }

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
      // We only keep the top frame if we believe it to be interpreted frame.
      if (type != StackFrame::INTERPRETED) {
        advance_frame = true;
      }
    } else {
      // Mark the frame as OPTIMIZED if we cannot determine its type.
      // We chose OPTIMIZED rather than INTERPRETED because it's closer to
      // the original value of StackFrame::JAVA_SCRIPT here, in that JAVA_SCRIPT
      // referred to full-codegen frames (now removed from the tree), and
      // OPTIMIZED refers to turbofan frames, both of which are generated
      // code. INTERPRETED frames refer to bytecode.
      // The frame anyways will be skipped.
      type = StackFrame::OPTIMIZED;
      // Top frame is incomplete so we cannot reliably determine its type.
      top_frame_type_ = StackFrame::NONE;
    }
  } else {
    return;
  }
  frame_ = SingletonFor(type, &state);
  if (advance_frame && frame_) Advance();
}


bool SafeStackFrameIterator::IsValidTop(ThreadLocalTop* top) const {
  Address c_entry_fp = Isolate::c_entry_fp(top);
  if (!IsValidExitFrame(c_entry_fp)) return false;
  // There should be at least one JS_ENTRY stack handler.
  Address handler = Isolate::handler(top);
  if (handler == nullptr) return false;
  // Check that there are no js frames on top of the native frames.
  return c_entry_fp < handler;
}


void SafeStackFrameIterator::AdvanceOneFrame() {
  DCHECK(!done());
  StackFrame* last_frame = frame_;
  Address last_sp = last_frame->sp(), last_fp = last_frame->fp();
  // Before advancing to the next stack frame, perform pointer validity tests.
  if (!IsValidFrame(last_frame) || !IsValidCaller(last_frame)) {
    frame_ = nullptr;
    return;
  }

  // Advance to the previous frame.
  StackFrame::State state;
  StackFrame::Type type = frame_->GetCallerState(&state);
  frame_ = SingletonFor(type, &state);
  if (!frame_) return;

  // Check that we have actually moved to the previous frame in the stack.
  if (frame_->sp() <= last_sp || frame_->fp() <= last_fp) {
    frame_ = nullptr;
  }
}


bool SafeStackFrameIterator::IsValidFrame(StackFrame* frame) const {
  return IsValidStackAddress(frame->sp()) && IsValidStackAddress(frame->fp());
}


bool SafeStackFrameIterator::IsValidCaller(StackFrame* frame) {
  StackFrame::State state;
  if (frame->is_entry() || frame->is_construct_entry()) {
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
         SingletonFor(frame->GetCallerState(&state)) != nullptr;
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
    ExternalCallbackScope* last_callback_scope = nullptr;
    while (external_callback_scope_ != nullptr &&
           external_callback_scope_->scope_address() < frame_->fp()) {
      // As long as the setup of a frame is not atomic, we may happen to be
      // in an interval where an ExternalCallbackScope is already created,
      // but the frame is not yet entered. So we are actually observing
      // the previous frame.
      // Skip all the ExternalCallbackScope's that are below the current fp.
      last_callback_scope = external_callback_scope_;
      external_callback_scope_ = external_callback_scope_->previous();
    }
    if (frame_->is_java_script() || frame_->is_wasm()) break;
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

namespace {
Code* GetContainingCode(Isolate* isolate, Address pc) {
  return isolate->inner_pointer_to_code_cache()->GetCacheEntry(pc)->code;
}
}  // namespace

Code* StackFrame::LookupCode() const {
  Code* result = GetContainingCode(isolate(), pc());
  DCHECK_GE(pc(), result->InstructionStart());
  DCHECK_LT(pc(), result->InstructionEnd());
  return result;
}

void StackFrame::IteratePc(RootVisitor* v, Address* pc_address,
                           Address* constant_pool_address, Code* holder) {
  Address pc = *pc_address;
  DCHECK(holder->GetHeap()->GcSafeCodeContains(holder, pc));
  unsigned pc_offset = static_cast<unsigned>(pc - holder->InstructionStart());
  Object* code = holder;
  v->VisitRootPointer(Root::kTop, nullptr, &code);
  if (code == holder) return;
  holder = reinterpret_cast<Code*>(code);
  pc = holder->InstructionStart() + pc_offset;
  *pc_address = pc;
  if (FLAG_enable_embedded_constant_pool && constant_pool_address) {
    *constant_pool_address = holder->constant_pool();
  }
}


void StackFrame::SetReturnAddressLocationResolver(
    ReturnAddressLocationResolver resolver) {
  DCHECK_NULL(return_address_location_resolver_);
  return_address_location_resolver_ = resolver;
}

StackFrame::Type StackFrame::ComputeType(const StackFrameIteratorBase* iterator,
                                         State* state) {
  DCHECK_NOT_NULL(state->fp);

  MSAN_MEMORY_IS_INITIALIZED(
      state->fp + CommonFrameConstants::kContextOrFrameTypeOffset,
      kPointerSize);
  intptr_t marker = Memory::intptr_at(
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
    if (!StackFrame::IsTypeMarker(marker)) {
      if (maybe_function->IsSmi()) {
        return NATIVE;
      } else if (IsInterpreterFramePc(iterator->isolate(), *(state->pc_address),
                                      state)) {
        return INTERPRETED;
      } else {
        return OPTIMIZED;
      }
    }
  } else {
    Address pc = *(state->pc_address);
    // If the {pc} does not point into WebAssembly code we can rely on the
    // returned {wasm_code} to be null and fall back to {GetContainingCode}.
    wasm::WasmCode* wasm_code =
        iterator->isolate()->wasm_engine()->code_manager()->LookupCode(pc);
    if (wasm_code != nullptr) {
      switch (wasm_code->kind()) {
        case wasm::WasmCode::kInterpreterStub:
          return WASM_INTERPRETER_ENTRY;
        case wasm::WasmCode::kFunction:
        case wasm::WasmCode::kCopiedStub:
          return WASM_COMPILED;
        case wasm::WasmCode::kLazyStub:
          if (StackFrame::IsTypeMarker(marker)) break;
          return BUILTIN;
        case wasm::WasmCode::kWasmToJsWrapper:
          return WASM_TO_JS;
        default:
          UNREACHABLE();
      }
    } else {
      // Look up the code object to figure out the type of the stack frame.
      Code* code_obj = GetContainingCode(iterator->isolate(), pc);
      if (code_obj != nullptr) {
        switch (code_obj->kind()) {
          case Code::BUILTIN:
            if (StackFrame::IsTypeMarker(marker)) break;
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
          case Code::OPTIMIZED_FUNCTION:
            return OPTIMIZED;
          case Code::WASM_FUNCTION:
            return WASM_COMPILED;
          case Code::WASM_TO_JS_FUNCTION:
            return WASM_TO_JS;
          case Code::JS_TO_WASM_FUNCTION:
            return JS_TO_WASM;
          case Code::WASM_INTERPRETER_ENTRY:
            return WASM_INTERPRETER_ENTRY;
          case Code::C_WASM_ENTRY:
            return C_WASM_ENTRY;
          default:
            // All other types should have an explicit marker
            break;
        }
      } else {
        return NATIVE;
      }
    }
  }
  DCHECK(StackFrame::IsTypeMarker(marker));
  StackFrame::Type candidate = StackFrame::MarkerToType(marker);
  switch (candidate) {
    case ENTRY:
    case CONSTRUCT_ENTRY:
    case EXIT:
    case BUILTIN_CONTINUATION:
    case JAVA_SCRIPT_BUILTIN_CONTINUATION:
    case JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH:
    case BUILTIN_EXIT:
    case STUB:
    case INTERNAL:
    case CONSTRUCT:
    case ARGUMENTS_ADAPTOR:
    case WASM_TO_JS:
    case WASM_COMPILED:
      return candidate;
    case JS_TO_WASM:
    case OPTIMIZED:
    case INTERPRETED:
    default:
      // Unoptimized and optimized JavaScript frames, including
      // interpreted frames, should never have a StackFrame::Type
      // marker. If we find one, we're likely being called from the
      // profiler in a bogus stack frame.
      return NATIVE;
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

void NativeFrame::ComputeCallerState(State* state) const {
  state->sp = caller_sp();
  state->fp = Memory::Address_at(fp() + CommonFrameConstants::kCallerFPOffset);
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(fp() + CommonFrameConstants::kCallerPCOffset));
  state->callee_pc_address = nullptr;
  state->constant_pool_address = nullptr;
}

Code* EntryFrame::unchecked_code() const {
  return isolate()->heap()->js_entry_code();
}


void EntryFrame::ComputeCallerState(State* state) const {
  GetCallerState(state);
}


StackFrame::Type EntryFrame::GetCallerState(State* state) const {
  const int offset = EntryFrameConstants::kCallerFPOffset;
  Address fp = Memory::Address_at(this->fp() + offset);
  return ExitFrame::GetStateForFramePointer(fp, state);
}

Code* ConstructEntryFrame::unchecked_code() const {
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
  state->callee_pc_address = nullptr;
  if (FLAG_enable_embedded_constant_pool) {
    state->constant_pool_address = reinterpret_cast<Address*>(
        fp() + ExitFrameConstants::kConstantPoolOffset);
  }
}


void ExitFrame::Iterate(RootVisitor* v) const {
  // The arguments are traversed as part of the expression stack of
  // the calling frame.
  IteratePc(v, pc_address(), constant_pool_address(), LookupCode());
  v->VisitRootPointer(Root::kTop, nullptr, &code_slot());
}


Address ExitFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPOffset;
}


StackFrame::Type ExitFrame::GetStateForFramePointer(Address fp, State* state) {
  if (fp == 0) return NONE;
  Address sp = ComputeStackPointer(fp);
  FillState(fp, sp, state);
  DCHECK_NOT_NULL(*state->pc_address);

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

  intptr_t marker_int = bit_cast<intptr_t>(marker);

  StackFrame::Type frame_type = static_cast<StackFrame::Type>(marker_int >> 1);
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
  state->callee_pc_address = nullptr;
  // The constant pool recorded in the exit frame is not associated
  // with the pc in this state (the return address into a C entry
  // stub).  ComputeCallerState will retrieve the constant pool
  // together with the associated caller pc.
  state->constant_pool_address = nullptr;
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
  int offset =
      BuiltinExitFrameConstants::kFirstArgumentOffset + i * kPointerSize;
  return Memory::Object_at(fp() + offset);
}

int BuiltinExitFrame::ComputeParametersCount() const {
  Object* argc_slot = argc_slot_object();
  DCHECK(argc_slot->IsSmi());
  // Argc also counts the receiver, target, new target, and argc itself as args,
  // therefore the real argument count is argc - 4.
  int argc = Smi::ToInt(argc_slot) - 4;
  DCHECK_GE(argc, 0);
  return argc;
}

namespace {
void PrintIndex(StringStream* accumulator, StackFrame::PrintMode mode,
                int index) {
  accumulator->Add((mode == StackFrame::OVERVIEW) ? "%5d: " : "[%d]: ", index);
}

const char* StringForStackFrameType(StackFrame::Type type) {
  switch (type) {
#define CASE(value, name) \
  case StackFrame::value: \
    return #name;
    STACK_FRAME_TYPE_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}
}  // namespace

void StackFrame::Print(StringStream* accumulator, PrintMode mode,
                       int index) const {
  DisallowHeapAllocation no_gc;
  PrintIndex(accumulator, mode, index);
  accumulator->Add(StringForStackFrameType(type()));
  accumulator->Add(" [pc: %p]\n", pc());
}

void BuiltinExitFrame::Print(StringStream* accumulator, PrintMode mode,
                             int index) const {
  DisallowHeapAllocation no_gc;
  Object* receiver = this->receiver();
  JSFunction* function = this->function();

  accumulator->PrintSecurityTokenIfChanged(function);
  PrintIndex(accumulator, mode, index);
  accumulator->Add("builtin exit frame: ");
  Code* code = nullptr;
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

int StandardFrame::position() const {
  AbstractCode* code = AbstractCode::cast(LookupCode());
  int code_offset = static_cast<int>(pc() - code->InstructionStart());
  return code->SourcePosition(code_offset);
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
}

int StandardFrame::ComputeParametersCount() const { return 0; }

void StandardFrame::ComputeCallerState(State* state) const {
  state->sp = caller_sp();
  state->fp = caller_fp();
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(ComputePCAddress(fp())));
  state->callee_pc_address = pc_address();
  state->constant_pool_address =
      reinterpret_cast<Address*>(ComputeConstantPoolAddress(fp()));
}


bool StandardFrame::IsConstructor() const { return false; }

void StandardFrame::Summarize(std::vector<FrameSummary>* functions) const {
  // This should only be called on frames which override this method.
  UNREACHABLE();
}

void StandardFrame::IterateCompiledFrame(RootVisitor* v) const {
  // Make sure that we're not doing "safe" stack frame iteration. We cannot
  // possibly find pointers in optimized frames in that state.
  DCHECK(can_access_heap_objects());

  // Find the code and compute the safepoint information.
  Address inner_pointer = pc();
  const wasm::WasmCode* wasm_code =
      isolate()->wasm_engine()->code_manager()->LookupCode(inner_pointer);
  SafepointEntry safepoint_entry;
  uint32_t stack_slots;
  Code* code = nullptr;
  bool has_tagged_params = false;
  if (wasm_code != nullptr) {
    SafepointTable table(wasm_code->instructions().start(),
                         wasm_code->safepoint_table_offset(),
                         wasm_code->stack_slots());
    safepoint_entry = table.FindEntry(inner_pointer);
    stack_slots = wasm_code->stack_slots();
    has_tagged_params = wasm_code->kind() != wasm::WasmCode::kFunction;
  } else {
    InnerPointerToCodeCache::InnerPointerToCodeCacheEntry* entry =
        isolate()->inner_pointer_to_code_cache()->GetCacheEntry(inner_pointer);
    if (!entry->safepoint_entry.is_valid()) {
      entry->safepoint_entry = entry->code->GetSafepointEntry(inner_pointer);
      DCHECK(entry->safepoint_entry.is_valid());
    } else {
      DCHECK(entry->safepoint_entry.Equals(
          entry->code->GetSafepointEntry(inner_pointer)));
    }

    code = entry->code;
    safepoint_entry = entry->safepoint_entry;
    stack_slots = code->stack_slots();
    has_tagged_params = code->has_tagged_params();
  }
  uint32_t slot_space = stack_slots * kPointerSize;

  // Determine the fixed header and spill slot area size.
  int frame_header_size = StandardFrameConstants::kFixedFrameSizeFromFp;
  intptr_t marker =
      Memory::intptr_at(fp() + CommonFrameConstants::kContextOrFrameTypeOffset);
  if (StackFrame::IsTypeMarker(marker)) {
    StackFrame::Type candidate = StackFrame::MarkerToType(marker);
    switch (candidate) {
      case ENTRY:
      case CONSTRUCT_ENTRY:
      case EXIT:
      case BUILTIN_CONTINUATION:
      case JAVA_SCRIPT_BUILTIN_CONTINUATION:
      case JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH:
      case BUILTIN_EXIT:
      case ARGUMENTS_ADAPTOR:
      case STUB:
      case INTERNAL:
      case CONSTRUCT:
      case JS_TO_WASM:
      case WASM_TO_JS:
      case WASM_COMPILED:
      case WASM_INTERPRETER_ENTRY:
      case C_WASM_ENTRY:
        frame_header_size = TypedFrameConstants::kFixedFrameSizeFromFp;
        break;
      case OPTIMIZED:
      case INTERPRETED:
      case BUILTIN:
        // These frame types have a context, but they are actually stored
        // in the place on the stack that one finds the frame type.
        UNREACHABLE();
        break;
      case NATIVE:
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
    v->VisitRootPointers(Root::kTop, nullptr, parameters_base,
                         parameters_base + safepoint_entry.argument_count());
    parameters_base += safepoint_entry.argument_count();
  }

  // Skip saved double registers.
  if (safepoint_entry.has_doubles()) {
    // Number of doubles not known at snapshot time.
    DCHECK(!isolate()->serializer_enabled());
    parameters_base +=
        RegisterConfiguration::Default()->num_allocatable_double_registers() *
        kDoubleSize / kPointerSize;
  }

  // Visit the registers that contain pointers if any.
  if (safepoint_entry.HasRegisters()) {
    for (int i = kNumSafepointRegisters - 1; i >=0; i--) {
      if (safepoint_entry.HasRegisterAt(i)) {
        int reg_stack_index = MacroAssembler::SafepointRegisterStackIndex(i);
        v->VisitRootPointer(Root::kTop, nullptr,
                            parameters_base + reg_stack_index);
      }
    }
    // Skip the words containing the register values.
    parameters_base += kNumSafepointRegisters;
  }

  // We're done dealing with the register bits.
  uint8_t* safepoint_bits = safepoint_entry.bits();
  safepoint_bits += kNumSafepointRegisters >> kBitsPerByteLog2;

  // Visit the rest of the parameters if they are tagged.
  if (has_tagged_params) {
    v->VisitRootPointers(Root::kTop, nullptr, parameters_base,
                         parameters_limit);
  }

  // Visit pointer spill slots and locals.
  for (unsigned index = 0; index < stack_slots; index++) {
    int byte_index = index >> kBitsPerByteLog2;
    int bit_index = index & (kBitsPerByte - 1);
    if ((safepoint_bits[byte_index] & (1U << bit_index)) != 0) {
      v->VisitRootPointer(Root::kTop, nullptr, parameters_limit + index);
    }
  }

  // For wasm-to-js cases, we can skip this.
  if (code != nullptr) {
    // Visit the return address in the callee and incoming arguments.
    IteratePc(v, pc_address(), constant_pool_address(), code);
  }

  if (!is_wasm() && !is_wasm_to_js()) {
    // If this frame has JavaScript ABI, visit the context (in stub and JS
    // frames) and the function (in JS frames).
    v->VisitRootPointers(Root::kTop, nullptr, frame_header_base,
                         frame_header_limit);
  }
}

void StubFrame::Iterate(RootVisitor* v) const { IterateCompiledFrame(v); }

Code* StubFrame::unchecked_code() const {
  return isolate()->FindCodeObject(pc());
}


Address StubFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPOffset;
}


int StubFrame::GetNumberOfIncomingArguments() const {
  return 0;
}

int StubFrame::LookupExceptionHandlerInTable(int* stack_slots) {
  Code* code = LookupCode();
  DCHECK(code->is_turbofanned());
  DCHECK_EQ(code->kind(), Code::BUILTIN);
  HandlerTable table(code);
  int pc_offset = static_cast<int>(pc() - code->InstructionStart());
  *stack_slots = code->stack_slots();
  return table.LookupReturn(pc_offset);
}

void OptimizedFrame::Iterate(RootVisitor* v) const { IterateCompiledFrame(v); }

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
  std::vector<SharedFunctionInfo*> functions;
  GetFunctions(&functions);
  return functions.size() > 1;
}


Code* JavaScriptFrame::unchecked_code() const {
  return function()->code();
}


int JavaScriptFrame::GetNumberOfIncomingArguments() const {
  DCHECK(can_access_heap_objects() &&
         isolate()->heap()->gc_state() == Heap::NOT_IN_GC);
  return function()->shared()->internal_formal_parameter_count();
}

int OptimizedFrame::GetNumberOfIncomingArguments() const {
  Code* code = LookupCode();
  if (code->kind() == Code::BUILTIN) {
    return static_cast<int>(
        Memory::intptr_at(fp() + OptimizedBuiltinFrameConstants::kArgCOffset));
  } else {
    return JavaScriptFrame::GetNumberOfIncomingArguments();
  }
}

Address JavaScriptFrame::GetCallerStackPointer() const {
  return fp() + StandardFrameConstants::kCallerSPOffset;
}

void JavaScriptFrame::GetFunctions(
    std::vector<SharedFunctionInfo*>* functions) const {
  DCHECK(functions->empty());
  functions->push_back(function()->shared());
}

void JavaScriptFrame::GetFunctions(
    std::vector<Handle<SharedFunctionInfo>>* functions) const {
  DCHECK(functions->empty());
  std::vector<SharedFunctionInfo*> raw_functions;
  GetFunctions(&raw_functions);
  for (const auto& raw_function : raw_functions) {
    functions->push_back(Handle<SharedFunctionInfo>(raw_function));
  }
}

void JavaScriptFrame::Summarize(std::vector<FrameSummary>* functions) const {
  DCHECK(functions->empty());
  Code* code = LookupCode();
  int offset = static_cast<int>(pc() - code->InstructionStart());
  AbstractCode* abstract_code = AbstractCode::cast(code);
  FrameSummary::JavaScriptFrameSummary summary(isolate(), receiver(),
                                               function(), abstract_code,
                                               offset, IsConstructor());
  functions->push_back(summary);
}

JSFunction* JavaScriptFrame::function() const {
  return JSFunction::cast(function_slot_object());
}

Object* JavaScriptFrame::unchecked_function() const {
  // During deoptimization of an optimized function, we may have yet to
  // materialize some closures on the stack. The arguments marker object
  // marks this case.
  DCHECK(function_slot_object()->IsJSFunction() ||
         isolate()->heap()->arguments_marker() == function_slot_object());
  return function_slot_object();
}

Object* JavaScriptFrame::receiver() const { return GetParameter(-1); }

Object* JavaScriptFrame::context() const {
  const int offset = StandardFrameConstants::kContextOffset;
  Object* maybe_result = Memory::Object_at(fp() + offset);
  DCHECK(!maybe_result->IsSmi());
  return maybe_result;
}

Script* JavaScriptFrame::script() const {
  return Script::cast(function()->shared()->script());
}

int JavaScriptFrame::LookupExceptionHandlerInTable(
    int* stack_depth, HandlerTable::CatchPrediction* prediction) {
  DCHECK_EQ(0, LookupCode()->handler_table_offset());
  DCHECK(!LookupCode()->is_optimized_code());
  return -1;
}

void JavaScriptFrame::PrintFunctionAndOffset(JSFunction* function,
                                             AbstractCode* code,
                                             int code_offset, FILE* file,
                                             bool print_line_number) {
  PrintF(file, "%s", function->IsOptimized() ? "*" : "~");
  function->PrintName(file);
  PrintF(file, "+%d", code_offset);
  if (print_line_number) {
    SharedFunctionInfo* shared = function->shared();
    int source_pos = code->SourcePosition(code_offset);
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
      JSFunction* function = frame->function();
      int code_offset = 0;
      if (frame->is_interpreted()) {
        InterpretedFrame* iframe = reinterpret_cast<InterpretedFrame*>(frame);
        code_offset = iframe->GetBytecodeOffset();
      } else {
        Code* code = frame->unchecked_code();
        code_offset = static_cast<int>(frame->pc() - code->InstructionStart());
      }
      PrintFunctionAndOffset(function, function->abstract_code(), code_offset,
                             file, print_line_number);
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

void JavaScriptFrame::CollectFunctionAndOffsetForICStats(JSFunction* function,
                                                         AbstractCode* code,
                                                         int code_offset) {
  auto ic_stats = ICStats::instance();
  ICInfo& ic_info = ic_stats->Current();
  SharedFunctionInfo* shared = function->shared();

  ic_info.function_name = ic_stats->GetOrCacheFunctionName(function);
  ic_info.script_offset = code_offset;

  int source_pos = code->SourcePosition(code_offset);
  Object* maybe_script = shared->script();
  if (maybe_script->IsScript()) {
    Script* script = Script::cast(maybe_script);
    ic_info.line_num = script->GetLineNumber(source_pos) + 1;
    ic_info.script_name = ic_stats->GetOrCacheScriptName(script);
  }
}

void JavaScriptFrame::CollectTopFrameForICStats(Isolate* isolate) {
  // constructor calls
  DisallowHeapAllocation no_allocation;
  JavaScriptFrameIterator it(isolate);
  ICInfo& ic_info = ICStats::instance()->Current();
  while (!it.done()) {
    if (it.frame()->is_java_script()) {
      JavaScriptFrame* frame = it.frame();
      if (frame->IsConstructor()) ic_info.is_constructor = true;
      JSFunction* function = frame->function();
      int code_offset = 0;
      if (frame->is_interpreted()) {
        InterpretedFrame* iframe = reinterpret_cast<InterpretedFrame*>(frame);
        code_offset = iframe->GetBytecodeOffset();
      } else {
        Code* code = frame->unchecked_code();
        code_offset = static_cast<int>(frame->pc() - code->InstructionStart());
      }
      CollectFunctionAndOffsetForICStats(function, function->abstract_code(),
                                         code_offset);
      return;
    }
    it.Advance();
  }
}

Object* JavaScriptFrame::GetParameter(int index) const {
  return Memory::Object_at(GetParameterSlot(index));
}

int JavaScriptFrame::ComputeParametersCount() const {
  return GetNumberOfIncomingArguments();
}

int JavaScriptBuiltinContinuationFrame::ComputeParametersCount() const {
  // Assert that the first allocatable register is also the argument count
  // register.
  DCHECK_EQ(RegisterConfiguration::Default()->GetAllocatableGeneralCode(0),
            kJavaScriptCallArgCountRegister.code());
  Object* argc_object =
      Memory::Object_at(fp() + BuiltinContinuationFrameConstants::kArgCOffset);
  return Smi::ToInt(argc_object);
}

intptr_t JavaScriptBuiltinContinuationFrame::GetSPToFPDelta() const {
  Address height_slot =
      fp() + BuiltinContinuationFrameConstants::kFrameSPtoFPDeltaAtDeoptimize;
  intptr_t height = Smi::ToInt(*reinterpret_cast<Smi**>(height_slot));
  return height;
}

Object* JavaScriptBuiltinContinuationFrame::context() const {
  return Memory::Object_at(
      fp() + BuiltinContinuationFrameConstants::kBuiltinContextOffset);
}

void JavaScriptBuiltinContinuationWithCatchFrame::SetException(
    Object* exception) {
  Address exception_argument_slot =
      fp() + JavaScriptFrameConstants::kLastParameterOffset +
      kPointerSize;  // Skip over return value slot.

  // Only allow setting exception if previous value was the hole.
  CHECK_EQ(isolate()->heap()->the_hole_value(),
           Memory::Object_at(exception_argument_slot));
  Memory::Object_at(exception_argument_slot) = exception;
}

FrameSummary::JavaScriptFrameSummary::JavaScriptFrameSummary(
    Isolate* isolate, Object* receiver, JSFunction* function,
    AbstractCode* abstract_code, int code_offset, bool is_constructor)
    : FrameSummaryBase(isolate, FrameSummary::JAVA_SCRIPT),
      receiver_(receiver, isolate),
      function_(function, isolate),
      abstract_code_(abstract_code, isolate),
      code_offset_(code_offset),
      is_constructor_(is_constructor) {
  DCHECK(abstract_code->IsBytecodeArray() ||
         Code::cast(abstract_code)->kind() != Code::OPTIMIZED_FUNCTION);
}

bool FrameSummary::JavaScriptFrameSummary::is_subject_to_debugging() const {
  return function()->shared()->IsSubjectToDebugging();
}

int FrameSummary::JavaScriptFrameSummary::SourcePosition() const {
  return abstract_code()->SourcePosition(code_offset());
}

int FrameSummary::JavaScriptFrameSummary::SourceStatementPosition() const {
  return abstract_code()->SourceStatementPosition(code_offset());
}

Handle<Object> FrameSummary::JavaScriptFrameSummary::script() const {
  return handle(function_->shared()->script(), isolate());
}

Handle<String> FrameSummary::JavaScriptFrameSummary::FunctionName() const {
  return JSFunction::GetDebugName(function_);
}

Handle<Context> FrameSummary::JavaScriptFrameSummary::native_context() const {
  return handle(function_->context()->native_context(), isolate());
}

FrameSummary::WasmFrameSummary::WasmFrameSummary(
    Isolate* isolate, FrameSummary::Kind kind,
    Handle<WasmInstanceObject> instance, bool at_to_number_conversion)
    : FrameSummaryBase(isolate, kind),
      wasm_instance_(instance),
      at_to_number_conversion_(at_to_number_conversion) {}

Handle<Object> FrameSummary::WasmFrameSummary::receiver() const {
  return wasm_instance_->GetIsolate()->global_proxy();
}

#define WASM_SUMMARY_DISPATCH(type, name)                                      \
  type FrameSummary::WasmFrameSummary::name() const {                          \
    DCHECK(kind() == Kind::WASM_COMPILED || kind() == Kind::WASM_INTERPRETED); \
    return kind() == Kind::WASM_COMPILED                                       \
               ? static_cast<const WasmCompiledFrameSummary*>(this)->name()    \
               : static_cast<const WasmInterpretedFrameSummary*>(this)         \
                     ->name();                                                 \
  }

WASM_SUMMARY_DISPATCH(uint32_t, function_index)
WASM_SUMMARY_DISPATCH(int, byte_offset)

#undef WASM_SUMMARY_DISPATCH

int FrameSummary::WasmFrameSummary::SourcePosition() const {
  Handle<WasmSharedModuleData> shared(
      wasm_instance()->compiled_module()->shared(), isolate());
  return WasmSharedModuleData::GetSourcePosition(
      shared, function_index(), byte_offset(), at_to_number_conversion());
}

Handle<Script> FrameSummary::WasmFrameSummary::script() const {
  return handle(wasm_instance()->compiled_module()->shared()->script());
}

Handle<String> FrameSummary::WasmFrameSummary::FunctionName() const {
  Handle<WasmSharedModuleData> shared(
      wasm_instance()->compiled_module()->shared(), isolate());
  return WasmSharedModuleData::GetFunctionName(isolate(), shared,
                                               function_index());
}

Handle<Context> FrameSummary::WasmFrameSummary::native_context() const {
  return handle(wasm_instance()->compiled_module()->native_context(),
                isolate());
}

FrameSummary::WasmCompiledFrameSummary::WasmCompiledFrameSummary(
    Isolate* isolate, Handle<WasmInstanceObject> instance, wasm::WasmCode* code,
    int code_offset, bool at_to_number_conversion)
    : WasmFrameSummary(isolate, WASM_COMPILED, instance,
                       at_to_number_conversion),
      code_(code),
      code_offset_(code_offset) {}

uint32_t FrameSummary::WasmCompiledFrameSummary::function_index() const {
  return code()->index();
}

int FrameSummary::WasmCompiledFrameSummary::GetWasmSourcePosition(
    const wasm::WasmCode* code, int offset) {
  int position = 0;
  // Subtract one because the current PC is one instruction after the call site.
  offset--;
  for (SourcePositionTableIterator iterator(code->source_positions());
       !iterator.done() && iterator.code_offset() <= offset;
       iterator.Advance()) {
    position = iterator.source_position().ScriptOffset();
  }
  return position;
}

int FrameSummary::WasmCompiledFrameSummary::byte_offset() const {
  return GetWasmSourcePosition(code_, code_offset());
}

FrameSummary::WasmInterpretedFrameSummary::WasmInterpretedFrameSummary(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    uint32_t function_index, int byte_offset)
    : WasmFrameSummary(isolate, WASM_INTERPRETED, instance, false),
      function_index_(function_index),
      byte_offset_(byte_offset) {}

FrameSummary::~FrameSummary() {
#define FRAME_SUMMARY_DESTR(kind, type, field, desc) \
  case kind:                                         \
    field.~type();                                   \
    break;
  switch (base_.kind()) {
    FRAME_SUMMARY_VARIANTS(FRAME_SUMMARY_DESTR)
    default:
      UNREACHABLE();
  }
#undef FRAME_SUMMARY_DESTR
}

FrameSummary FrameSummary::GetTop(const StandardFrame* frame) {
  std::vector<FrameSummary> frames;
  frame->Summarize(&frames);
  DCHECK_LT(0, frames.size());
  return frames.back();
}

FrameSummary FrameSummary::GetBottom(const StandardFrame* frame) {
  return Get(frame, 0);
}

FrameSummary FrameSummary::GetSingle(const StandardFrame* frame) {
  std::vector<FrameSummary> frames;
  frame->Summarize(&frames);
  DCHECK_EQ(1, frames.size());
  return frames.front();
}

FrameSummary FrameSummary::Get(const StandardFrame* frame, int index) {
  DCHECK_LE(0, index);
  std::vector<FrameSummary> frames;
  frame->Summarize(&frames);
  DCHECK_GT(frames.size(), index);
  return frames[index];
}

#define FRAME_SUMMARY_DISPATCH(ret, name)        \
  ret FrameSummary::name() const {               \
    switch (base_.kind()) {                      \
      case JAVA_SCRIPT:                          \
        return java_script_summary_.name();      \
      case WASM_COMPILED:                        \
        return wasm_compiled_summary_.name();    \
      case WASM_INTERPRETED:                     \
        return wasm_interpreted_summary_.name(); \
      default:                                   \
        UNREACHABLE();                           \
        return ret{};                            \
    }                                            \
  }

FRAME_SUMMARY_DISPATCH(Handle<Object>, receiver)
FRAME_SUMMARY_DISPATCH(int, code_offset)
FRAME_SUMMARY_DISPATCH(bool, is_constructor)
FRAME_SUMMARY_DISPATCH(bool, is_subject_to_debugging)
FRAME_SUMMARY_DISPATCH(Handle<Object>, script)
FRAME_SUMMARY_DISPATCH(int, SourcePosition)
FRAME_SUMMARY_DISPATCH(int, SourceStatementPosition)
FRAME_SUMMARY_DISPATCH(Handle<String>, FunctionName)
FRAME_SUMMARY_DISPATCH(Handle<Context>, native_context)

#undef FRAME_SUMMARY_DISPATCH

void OptimizedFrame::Summarize(std::vector<FrameSummary>* frames) const {
  DCHECK(frames->empty());
  DCHECK(is_optimized());

  // Delegate to JS frame in absence of turbofan deoptimization.
  // TODO(turbofan): Revisit once we support deoptimization across the board.
  Code* code = LookupCode();
  if (code->kind() == Code::BUILTIN) {
    return JavaScriptFrame::Summarize(frames);
  }

  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationData* const data = GetDeoptimizationData(&deopt_index);
  if (deopt_index == Safepoint::kNoDeoptimizationIndex) {
    CHECK_NULL(data);
    FATAL("Missing deoptimization information for OptimizedFrame::Summarize.");
  }

  // Prepare iteration over translation. Note that the below iteration might
  // materialize objects without storing them back to the Isolate, this will
  // lead to objects being re-materialized again for each summary.
  TranslatedState translated(this);
  translated.Prepare(fp());

  // We create the summary in reverse order because the frames
  // in the deoptimization translation are ordered bottom-to-top.
  bool is_constructor = IsConstructor();
  for (auto it = translated.begin(); it != translated.end(); it++) {
    if (it->kind() == TranslatedFrame::kInterpretedFunction ||
        it->kind() == TranslatedFrame::kJavaScriptBuiltinContinuation ||
        it->kind() ==
            TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch) {
      Handle<SharedFunctionInfo> shared_info = it->shared_info();

      // The translation commands are ordered and the function is always
      // at the first position, and the receiver is next.
      TranslatedFrame::iterator translated_values = it->begin();

      // Get or materialize the correct function in the optimized frame.
      Handle<JSFunction> function =
          Handle<JSFunction>::cast(translated_values->GetValue());
      translated_values++;

      // Get or materialize the correct receiver in the optimized frame.
      Handle<Object> receiver = translated_values->GetValue();
      translated_values++;

      // Determine the underlying code object and the position within it from
      // the translation corresponding to the frame type in question.
      Handle<AbstractCode> abstract_code;
      unsigned code_offset;
      if (it->kind() == TranslatedFrame::kJavaScriptBuiltinContinuation ||
          it->kind() ==
              TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch) {
        code_offset = 0;
        abstract_code =
            handle(AbstractCode::cast(isolate()->builtins()->builtin(
                       Builtins::GetBuiltinFromBailoutId(it->node_id()))),
                   isolate());
      } else {
        DCHECK_EQ(it->kind(), TranslatedFrame::kInterpretedFunction);
        code_offset = it->node_id().ToInt();  // Points to current bytecode.
        abstract_code = handle(shared_info->abstract_code(), isolate());
      }

      // Append full summary of the encountered JS frame.
      FrameSummary::JavaScriptFrameSummary summary(isolate(), *receiver,
                                                   *function, *abstract_code,
                                                   code_offset, is_constructor);
      frames->push_back(summary);
      is_constructor = false;
    } else if (it->kind() == TranslatedFrame::kConstructStub) {
      // The next encountered JS frame will be marked as a constructor call.
      DCHECK(!is_constructor);
      is_constructor = true;
    }
  }
}


int OptimizedFrame::LookupExceptionHandlerInTable(
    int* stack_slots, HandlerTable::CatchPrediction* prediction) {
  // We cannot perform exception prediction on optimized code. Instead, we need
  // to use FrameSummary to find the corresponding code offset in unoptimized
  // code to perform prediction there.
  DCHECK_NULL(prediction);
  Code* code = LookupCode();
  HandlerTable table(code);
  int pc_offset = static_cast<int>(pc() - code->InstructionStart());
  if (stack_slots) *stack_slots = code->stack_slots();

  // When the return pc has been replaced by a trampoline there won't be
  // a handler for this trampoline. Thus we need to use the return pc that
  // _used to be_ on the stack to get the right ExceptionHandler.
  if (code->kind() == Code::OPTIMIZED_FUNCTION &&
      code->marked_for_deoptimization()) {
    SafepointTable safepoints(code);
    pc_offset = safepoints.find_return_pc(pc_offset);
  }
  return table.LookupReturn(pc_offset);
}

DeoptimizationData* OptimizedFrame::GetDeoptimizationData(
    int* deopt_index) const {
  DCHECK(is_optimized());

  JSFunction* opt_function = function();
  Code* code = opt_function->code();

  // The code object may have been replaced by lazy deoptimization. Fall
  // back to a slow search in this case to find the original optimized
  // code object.
  if (!code->contains(pc())) {
    code = isolate()->heap()->GcSafeFindCodeForInnerPointer(pc());
  }
  DCHECK_NOT_NULL(code);
  DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);

  SafepointEntry safepoint_entry = code->GetSafepointEntry(pc());
  *deopt_index = safepoint_entry.deoptimization_index();
  if (*deopt_index != Safepoint::kNoDeoptimizationIndex) {
    return DeoptimizationData::cast(code->deoptimization_data());
  }
  return nullptr;
}

Object* OptimizedFrame::receiver() const {
  Code* code = LookupCode();
  if (code->kind() == Code::BUILTIN) {
    Address argc_ptr = fp() + OptimizedBuiltinFrameConstants::kArgCOffset;
    intptr_t argc = *reinterpret_cast<intptr_t*>(argc_ptr);
    intptr_t args_size =
        (StandardFrameConstants::kFixedSlotCountAboveFp + argc) * kPointerSize;
    Address receiver_ptr = fp() + args_size;
    return *reinterpret_cast<Object**>(receiver_ptr);
  } else {
    return JavaScriptFrame::receiver();
  }
}

void OptimizedFrame::GetFunctions(
    std::vector<SharedFunctionInfo*>* functions) const {
  DCHECK(functions->empty());
  DCHECK(is_optimized());

  // Delegate to JS frame in absence of turbofan deoptimization.
  // TODO(turbofan): Revisit once we support deoptimization across the board.
  Code* code = LookupCode();
  if (code->kind() == Code::BUILTIN) {
    return JavaScriptFrame::GetFunctions(functions);
  }

  DisallowHeapAllocation no_gc;
  int deopt_index = Safepoint::kNoDeoptimizationIndex;
  DeoptimizationData* const data = GetDeoptimizationData(&deopt_index);
  DCHECK_NOT_NULL(data);
  DCHECK_NE(Safepoint::kNoDeoptimizationIndex, deopt_index);
  FixedArray* const literal_array = data->LiteralArray();

  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());
  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  DCHECK_EQ(Translation::BEGIN, opcode);
  it.Next();  // Skip frame count.
  int jsframe_count = it.Next();
  it.Next();  // Skip update feedback count.

  // We insert the frames in reverse order because the frames
  // in the deoptimization translation are ordered bottom-to-top.
  while (jsframe_count != 0) {
    opcode = static_cast<Translation::Opcode>(it.Next());
    if (opcode == Translation::INTERPRETED_FRAME ||
        opcode == Translation::JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME ||
        opcode ==
            Translation::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME) {
      it.Next();  // Skip bailout id.
      jsframe_count--;

      // The second operand of the frame points to the function.
      Object* shared = literal_array->get(it.Next());
      functions->push_back(SharedFunctionInfo::cast(shared));

      // Skip over remaining operands to advance to the next opcode.
      it.Skip(Translation::NumberOfOperandsFor(opcode) - 2);
    } else {
      // Skip over operands to advance to the next opcode.
      it.Skip(Translation::NumberOfOperandsFor(opcode));
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

int InterpretedFrame::position() const {
  AbstractCode* code = AbstractCode::cast(GetBytecodeArray());
  int code_offset = GetBytecodeOffset();
  return code->SourcePosition(code_offset);
}

int InterpretedFrame::LookupExceptionHandlerInTable(
    int* context_register, HandlerTable::CatchPrediction* prediction) {
  HandlerTable table(function()->shared()->GetBytecodeArray());
  return table.LookupRange(GetBytecodeOffset(), context_register, prediction);
}

int InterpretedFrame::GetBytecodeOffset() const {
  const int index = InterpreterFrameConstants::kBytecodeOffsetExpressionIndex;
  DCHECK_EQ(
      InterpreterFrameConstants::kBytecodeOffsetFromFp,
      InterpreterFrameConstants::kExpressionsOffset - index * kPointerSize);
  int raw_offset = Smi::ToInt(GetExpression(index));
  return raw_offset - BytecodeArray::kHeaderSize + kHeapObjectTag;
}

int InterpretedFrame::GetBytecodeOffset(Address fp) {
  const int offset = InterpreterFrameConstants::kExpressionsOffset;
  const int index = InterpreterFrameConstants::kBytecodeOffsetExpressionIndex;
  DCHECK_EQ(
      InterpreterFrameConstants::kBytecodeOffsetFromFp,
      InterpreterFrameConstants::kExpressionsOffset - index * kPointerSize);
  Address expression_offset = fp + offset - index * kPointerSize;
  int raw_offset = Smi::ToInt(Memory::Object_at(expression_offset));
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

void InterpretedFrame::Summarize(std::vector<FrameSummary>* functions) const {
  DCHECK(functions->empty());
  AbstractCode* abstract_code =
      AbstractCode::cast(function()->shared()->GetBytecodeArray());
  FrameSummary::JavaScriptFrameSummary summary(
      isolate(), receiver(), function(), abstract_code, GetBytecodeOffset(),
      IsConstructor());
  functions->push_back(summary);
}

int ArgumentsAdaptorFrame::GetNumberOfIncomingArguments() const {
  return Smi::ToInt(GetExpression(0));
}

Code* ArgumentsAdaptorFrame::unchecked_code() const {
  return isolate()->builtins()->builtin(
      Builtins::kArgumentsAdaptorTrampoline);
}

int BuiltinFrame::GetNumberOfIncomingArguments() const {
  return Smi::ToInt(GetExpression(0));
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
  DCHECK_NOT_NULL(code);
  return reinterpret_cast<Code*>(code);
}


void WasmCompiledFrame::Print(StringStream* accumulator, PrintMode mode,
                              int index) const {
  PrintIndex(accumulator, mode, index);
  accumulator->Add("WASM [");
  Script* script = this->script();
  accumulator->PrintName(script->name());
  Address instruction_start = isolate()
                                  ->wasm_engine()
                                  ->code_manager()
                                  ->LookupCode(pc())
                                  ->instructions()
                                  .start();
  int pc = static_cast<int>(this->pc() - instruction_start);
  Vector<const uint8_t> raw_func_name =
      shared()->GetRawFunctionName(this->function_index());
  const int kMaxPrintedFunctionName = 64;
  char func_name[kMaxPrintedFunctionName + 1];
  int func_name_len = std::min(kMaxPrintedFunctionName, raw_func_name.length());
  memcpy(func_name, raw_func_name.start(), func_name_len);
  func_name[func_name_len] = '\0';
  accumulator->Add("], function #%u ('%s'), pc=%p, pos=%d\n",
                   this->function_index(), func_name, pc, this->position());
  if (mode != OVERVIEW) accumulator->Add("\n");
}

Code* WasmCompiledFrame::unchecked_code() const {
  return isolate()->FindCodeObject(pc());
}

void WasmCompiledFrame::Iterate(RootVisitor* v) const {
  IterateCompiledFrame(v);
}

Address WasmCompiledFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPOffset;
}

wasm::WasmCode* WasmCompiledFrame::wasm_code() const {
  return isolate()->wasm_engine()->code_manager()->LookupCode(pc());
}

WasmInstanceObject* WasmCompiledFrame::wasm_instance() const {
  return LookupWasmInstanceObjectFromStandardFrame(this);
}

WasmSharedModuleData* WasmCompiledFrame::shared() const {
  return LookupWasmInstanceObjectFromStandardFrame(this)
      ->compiled_module()
      ->shared();
}

WasmCompiledModule* WasmCompiledFrame::compiled_module() const {
  return LookupWasmInstanceObjectFromStandardFrame(this)->compiled_module();
}

uint32_t WasmCompiledFrame::function_index() const {
  return FrameSummary::GetSingle(this).AsWasmCompiled().function_index();
}

Script* WasmCompiledFrame::script() const { return shared()->script(); }

int WasmCompiledFrame::position() const {
  return FrameSummary::GetSingle(this).SourcePosition();
}

void WasmCompiledFrame::Summarize(std::vector<FrameSummary>* functions) const {
  DCHECK(functions->empty());
  wasm::WasmCode* code = wasm_code();
  int offset = static_cast<int>(pc() - code->instructions().start());
  Handle<WasmInstanceObject> instance(
      LookupWasmInstanceObjectFromStandardFrame(this), isolate());
  FrameSummary::WasmCompiledFrameSummary summary(
      isolate(), instance, code, offset, at_to_number_conversion());
  functions->push_back(summary);
}

bool WasmCompiledFrame::at_to_number_conversion() const {
  // Check whether our callee is a WASM_TO_JS frame, and this frame is at the
  // ToNumber conversion call.
  Address callee_pc = reinterpret_cast<Address>(this->callee_pc());
  wasm::WasmCode* code =
      callee_pc
          ? isolate()->wasm_engine()->code_manager()->LookupCode(callee_pc)
          : nullptr;
  if (!code || code->kind() != wasm::WasmCode::kWasmToJsWrapper) return false;
  int offset = static_cast<int>(callee_pc - code->instructions().start());
  int pos = FrameSummary::WasmCompiledFrameSummary::GetWasmSourcePosition(
      code, offset);
  DCHECK(pos == 0 || pos == 1);
  // The imported call has position 0, ToNumber has position 1.
  return !!pos;
}

int WasmCompiledFrame::LookupExceptionHandlerInTable(int* stack_slots) {
  DCHECK_NOT_NULL(stack_slots);
  wasm::WasmCode* code =
      isolate()->wasm_engine()->code_manager()->LookupCode(pc());
  if (!code->IsAnonymous() && code->handler_table_offset() > 0) {
    HandlerTable table(code->instructions().start(),
                       code->handler_table_offset());
    int pc_offset = static_cast<int>(pc() - code->instructions().start());
    *stack_slots = static_cast<int>(code->stack_slots());
    return table.LookupReturn(pc_offset);
  }
  return -1;
}

void WasmInterpreterEntryFrame::Iterate(RootVisitor* v) const {
  IterateCompiledFrame(v);
}

void WasmInterpreterEntryFrame::Print(StringStream* accumulator, PrintMode mode,
                                      int index) const {
  PrintIndex(accumulator, mode, index);
  accumulator->Add("WASM INTERPRETER ENTRY [");
  Script* script = this->script();
  accumulator->PrintName(script->name());
  accumulator->Add("]");
  if (mode != OVERVIEW) accumulator->Add("\n");
}

void WasmInterpreterEntryFrame::Summarize(
    std::vector<FrameSummary>* functions) const {
  Handle<WasmInstanceObject> instance(
      LookupWasmInstanceObjectFromStandardFrame(this), isolate());
  std::vector<std::pair<uint32_t, int>> interpreted_stack =
      instance->debug_info()->GetInterpretedStack(fp());

  for (auto& e : interpreted_stack) {
    FrameSummary::WasmInterpretedFrameSummary summary(isolate(), instance,
                                                      e.first, e.second);
    functions->push_back(summary);
  }
}

Code* WasmInterpreterEntryFrame::unchecked_code() const { UNREACHABLE(); }

// TODO(titzer): deprecate this method.
WasmInstanceObject* WasmInterpreterEntryFrame::wasm_instance() const {
  return LookupWasmInstanceObjectFromStandardFrame(this);
}

WasmDebugInfo* WasmInterpreterEntryFrame::debug_info() const {
  return LookupWasmInstanceObjectFromStandardFrame(this)->debug_info();
}

WasmSharedModuleData* WasmInterpreterEntryFrame::shared() const {
  return LookupWasmInstanceObjectFromStandardFrame(this)
      ->compiled_module()
      ->shared();
}

WasmCompiledModule* WasmInterpreterEntryFrame::compiled_module() const {
  return LookupWasmInstanceObjectFromStandardFrame(this)->compiled_module();
}

Script* WasmInterpreterEntryFrame::script() const { return shared()->script(); }

int WasmInterpreterEntryFrame::position() const {
  return FrameSummary::GetBottom(this).AsWasmInterpreted().SourcePosition();
}

Object* WasmInterpreterEntryFrame::context() const {
  return compiled_module()->native_context();
}

Address WasmInterpreterEntryFrame::GetCallerStackPointer() const {
  return fp() + ExitFrameConstants::kCallerSPOffset;
}

namespace {


void PrintFunctionSource(StringStream* accumulator, SharedFunctionInfo* shared,
                         Code* code) {
  if (FLAG_max_stack_trace_source_length != 0 && code != nullptr) {
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
  Code* code = nullptr;
  if (IsConstructor()) accumulator->Add("new ");
  accumulator->PrintFunction(function, receiver, &code);
  accumulator->Add(" [%p]", function);

  // Get scope information for nicer output, if possible. If code is nullptr, or
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
    if (is_interpreted()) {
      const InterpretedFrame* iframe =
          reinterpret_cast<const InterpretedFrame*>(this);
      BytecodeArray* bytecodes = iframe->GetBytecodeArray();
      int offset = iframe->GetBytecodeOffset();
      int source_pos = AbstractCode::cast(bytecodes)->SourcePosition(offset);
      int line = script->GetLineNumber(source_pos) + 1;
      accumulator->Add(":%d] [bytecode=%p offset=%d]", line, bytecodes, offset);
    } else {
      int function_start_pos = shared->StartPosition();
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
  Context* context = nullptr;
  if (this->context() != nullptr && this->context()->IsContext()) {
    context = Context::cast(this->context());
    while (context->IsWithContext()) {
      context = context->previous();
      DCHECK_NOT_NULL(context);
    }
  }

  // Print heap-allocated local variables.
  if (heap_locals_count > 0) {
    accumulator->Add("  // heap-allocated locals\n");
  }
  for (int i = 0; i < heap_locals_count; i++) {
    accumulator->Add("  var ");
    accumulator->PrintName(scope_info->ContextLocalName(i));
    accumulator->Add(" = ");
    if (context != nullptr) {
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

void EntryFrame::Iterate(RootVisitor* v) const {
  IteratePc(v, pc_address(), constant_pool_address(), LookupCode());
}

void StandardFrame::IterateExpressions(RootVisitor* v) const {
  const int offset = StandardFrameConstants::kLastObjectOffset;
  Object** base = &Memory::Object_at(sp());
  Object** limit = &Memory::Object_at(fp() + offset) + 1;
  v->VisitRootPointers(Root::kTop, nullptr, base, limit);
}

void JavaScriptFrame::Iterate(RootVisitor* v) const {
  IterateExpressions(v);
  IteratePc(v, pc_address(), constant_pool_address(), LookupCode());
}

void InternalFrame::Iterate(RootVisitor* v) const {
  wasm::WasmCode* wasm_code =
      isolate()->wasm_engine()->code_manager()->LookupCode(pc());
  if (wasm_code != nullptr) {
    DCHECK(wasm_code->kind() == wasm::WasmCode::kLazyStub);
  } else {
    Code* code = LookupCode();
    IteratePc(v, pc_address(), constant_pool_address(), code);
    // Internal frames typically do not receive any arguments, hence their stack
    // only contains tagged pointers.
    // We are misusing the has_tagged_params flag here to tell us whether
    // the full stack frame contains only tagged pointers or only raw values.
    // This is used for the WasmCompileLazy builtin, where we actually pass
    // untagged arguments and also store untagged values on the stack.
    if (code->has_tagged_params()) IterateExpressions(v);
  }
}

// -------------------------------------------------------------------------

InnerPointerToCodeCache::InnerPointerToCodeCacheEntry*
    InnerPointerToCodeCache::GetCacheEntry(Address inner_pointer) {
  isolate_->counters()->pc_to_code()->Increment();
  DCHECK(base::bits::IsPowerOfTwo(kInnerPointerToCodeCacheSize));
  uint32_t hash = ComputeIntegerHash(ObjectAddressForHashing(inner_pointer));
  uint32_t index = hash & (kInnerPointerToCodeCacheSize - 1);
  InnerPointerToCodeCacheEntry* entry = cache(index);
  if (entry->inner_pointer == inner_pointer) {
    isolate_->counters()->pc_to_code_cached()->Increment();
    DCHECK(entry->code ==
           isolate_->heap()->GcSafeFindCodeForInnerPointer(inner_pointer));
  } else {
    // Because this code may be interrupted by a profiling signal that
    // also queries the cache, we cannot update inner_pointer before the code
    // has been set. Otherwise, we risk trying to use a cache entry before
    // the code has been computed.
    entry->code =
        isolate_->heap()->GcSafeFindCodeForInnerPointer(inner_pointer);
    entry->safepoint_entry.Reset();
    entry->inner_pointer = inner_pointer;
  }
  return entry;
}


// -------------------------------------------------------------------------


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
  return nullptr;
}


Vector<StackFrame*> CreateStackMap(Isolate* isolate, Zone* zone) {
  ZoneVector<StackFrame*> frames(zone);
  for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
    StackFrame* frame = AllocateFrameCopy(it.frame(), zone);
    frames.push_back(frame);
  }
  return Vector<StackFrame*>(frames.data(), frames.size());
}


}  // namespace internal
}  // namespace v8
