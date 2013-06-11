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

#include "api.h"
#include "arguments.h"
#include "bootstrapper.h"
#include "code-stubs.h"
#include "codegen.h"
#include "compilation-cache.h"
#include "compiler.h"
#include "debug.h"
#include "deoptimizer.h"
#include "execution.h"
#include "full-codegen.h"
#include "global-handles.h"
#include "ic.h"
#include "ic-inl.h"
#include "isolate-inl.h"
#include "list.h"
#include "messages.h"
#include "natives.h"
#include "stub-cache.h"
#include "log.h"

#include "../include/v8-debug.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_DEBUGGER_SUPPORT


Debug::Debug(Isolate* isolate)
    : has_break_points_(false),
      script_cache_(NULL),
      debug_info_list_(NULL),
      disable_break_(false),
      break_on_exception_(false),
      break_on_uncaught_exception_(false),
      debug_break_return_(NULL),
      debug_break_slot_(NULL),
      isolate_(isolate) {
  memset(registers_, 0, sizeof(JSCallerSavedBuffer));
}


Debug::~Debug() {
}


static void PrintLn(v8::Local<v8::Value> value) {
  v8::Local<v8::String> s = value->ToString();
  ScopedVector<char> data(s->Utf8Length() + 1);
  if (data.start() == NULL) {
    V8::FatalProcessOutOfMemory("PrintLn");
    return;
  }
  s->WriteUtf8(data.start());
  PrintF("%s\n", data.start());
}


static Handle<Code> ComputeCallDebugPrepareStepIn(int argc, Code::Kind kind) {
  Isolate* isolate = Isolate::Current();
  return isolate->stub_cache()->ComputeCallDebugPrepareStepIn(argc, kind);
}


static v8::Handle<v8::Context> GetDebugEventContext(Isolate* isolate) {
  Handle<Context> context = isolate->debug()->debugger_entry()->GetContext();
  // Isolate::context() may have been NULL when "script collected" event
  // occured.
  if (context.is_null()) return v8::Local<v8::Context>();
  Handle<Context> native_context(context->native_context());
  return v8::Utils::ToLocal(native_context);
}


BreakLocationIterator::BreakLocationIterator(Handle<DebugInfo> debug_info,
                                             BreakLocatorType type) {
  debug_info_ = debug_info;
  type_ = type;
  reloc_iterator_ = NULL;
  reloc_iterator_original_ = NULL;
  Reset();  // Initialize the rest of the member variables.
}


BreakLocationIterator::~BreakLocationIterator() {
  ASSERT(reloc_iterator_ != NULL);
  ASSERT(reloc_iterator_original_ != NULL);
  delete reloc_iterator_;
  delete reloc_iterator_original_;
}


void BreakLocationIterator::Next() {
  DisallowHeapAllocation no_gc;
  ASSERT(!RinfoDone());

  // Iterate through reloc info for code and original code stopping at each
  // breakable code target.
  bool first = break_point_ == -1;
  while (!RinfoDone()) {
    if (!first) RinfoNext();
    first = false;
    if (RinfoDone()) return;

    // Whenever a statement position or (plain) position is passed update the
    // current value of these.
    if (RelocInfo::IsPosition(rmode())) {
      if (RelocInfo::IsStatementPosition(rmode())) {
        statement_position_ = static_cast<int>(
            rinfo()->data() - debug_info_->shared()->start_position());
      }
      // Always update the position as we don't want that to be before the
      // statement position.
      position_ = static_cast<int>(
          rinfo()->data() - debug_info_->shared()->start_position());
      ASSERT(position_ >= 0);
      ASSERT(statement_position_ >= 0);
    }

    if (IsDebugBreakSlot()) {
      // There is always a possible break point at a debug break slot.
      break_point_++;
      return;
    } else if (RelocInfo::IsCodeTarget(rmode())) {
      // Check for breakable code target. Look in the original code as setting
      // break points can cause the code targets in the running (debugged) code
      // to be of a different kind than in the original code.
      Address target = original_rinfo()->target_address();
      Code* code = Code::GetCodeFromTargetAddress(target);
      if ((code->is_inline_cache_stub() &&
           !code->is_binary_op_stub() &&
           !code->is_unary_op_stub() &&
           !code->is_compare_ic_stub() &&
           !code->is_to_boolean_ic_stub()) ||
          RelocInfo::IsConstructCall(rmode())) {
        break_point_++;
        return;
      }
      if (code->kind() == Code::STUB) {
        if (IsDebuggerStatement()) {
          break_point_++;
          return;
        }
        if (type_ == ALL_BREAK_LOCATIONS) {
          if (Debug::IsBreakStub(code)) {
            break_point_++;
            return;
          }
        } else {
          ASSERT(type_ == SOURCE_BREAK_LOCATIONS);
          if (Debug::IsSourceBreakStub(code)) {
            break_point_++;
            return;
          }
        }
      }
    }

    // Check for break at return.
    if (RelocInfo::IsJSReturn(rmode())) {
      // Set the positions to the end of the function.
      if (debug_info_->shared()->HasSourceCode()) {
        position_ = debug_info_->shared()->end_position() -
                    debug_info_->shared()->start_position() - 1;
      } else {
        position_ = 0;
      }
      statement_position_ = position_;
      break_point_++;
      return;
    }
  }
}


void BreakLocationIterator::Next(int count) {
  while (count > 0) {
    Next();
    count--;
  }
}


// Find the break point at the supplied address, or the closest one before
// the address.
void BreakLocationIterator::FindBreakLocationFromAddress(Address pc) {
  // Run through all break points to locate the one closest to the address.
  int closest_break_point = 0;
  int distance = kMaxInt;
  while (!Done()) {
    // Check if this break point is closer that what was previously found.
    if (this->pc() <= pc && pc - this->pc() < distance) {
      closest_break_point = break_point();
      distance = static_cast<int>(pc - this->pc());
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
    Next();
  }

  // Move to the break point found.
  Reset();
  Next(closest_break_point);
}


// Find the break point closest to the supplied source position.
void BreakLocationIterator::FindBreakLocationFromPosition(int position) {
  // Run through all break points to locate the one closest to the source
  // position.
  int closest_break_point = 0;
  int distance = kMaxInt;
  while (!Done()) {
    // Check if this break point is closer that what was previously found.
    if (position <= statement_position() &&
        statement_position() - position < distance) {
      closest_break_point = break_point();
      distance = statement_position() - position;
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
    Next();
  }

  // Move to the break point found.
  Reset();
  Next(closest_break_point);
}


void BreakLocationIterator::Reset() {
  // Create relocation iterators for the two code objects.
  if (reloc_iterator_ != NULL) delete reloc_iterator_;
  if (reloc_iterator_original_ != NULL) delete reloc_iterator_original_;
  reloc_iterator_ = new RelocIterator(
      debug_info_->code(),
      ~RelocInfo::ModeMask(RelocInfo::CODE_AGE_SEQUENCE));
  reloc_iterator_original_ = new RelocIterator(
      debug_info_->original_code(),
      ~RelocInfo::ModeMask(RelocInfo::CODE_AGE_SEQUENCE));

  // Position at the first break point.
  break_point_ = -1;
  position_ = 1;
  statement_position_ = 1;
  Next();
}


bool BreakLocationIterator::Done() const {
  return RinfoDone();
}


void BreakLocationIterator::SetBreakPoint(Handle<Object> break_point_object) {
  // If there is not already a real break point here patch code with debug
  // break.
  if (!HasBreakPoint()) {
    SetDebugBreak();
  }
  ASSERT(IsDebugBreak() || IsDebuggerStatement());
  // Set the break point information.
  DebugInfo::SetBreakPoint(debug_info_, code_position(),
                           position(), statement_position(),
                           break_point_object);
}


void BreakLocationIterator::ClearBreakPoint(Handle<Object> break_point_object) {
  // Clear the break point information.
  DebugInfo::ClearBreakPoint(debug_info_, code_position(), break_point_object);
  // If there are no more break points here remove the debug break.
  if (!HasBreakPoint()) {
    ClearDebugBreak();
    ASSERT(!IsDebugBreak());
  }
}


void BreakLocationIterator::SetOneShot() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) {
    return;
  }

  // If there is a real break point here no more to do.
  if (HasBreakPoint()) {
    ASSERT(IsDebugBreak());
    return;
  }

  // Patch code with debug break.
  SetDebugBreak();
}


void BreakLocationIterator::ClearOneShot() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) {
    return;
  }

  // If there is a real break point here no more to do.
  if (HasBreakPoint()) {
    ASSERT(IsDebugBreak());
    return;
  }

  // Patch code removing debug break.
  ClearDebugBreak();
  ASSERT(!IsDebugBreak());
}


void BreakLocationIterator::SetDebugBreak() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) {
    return;
  }

  // If there is already a break point here just return. This might happen if
  // the same code is flooded with break points twice. Flooding the same
  // function twice might happen when stepping in a function with an exception
  // handler as the handler and the function is the same.
  if (IsDebugBreak()) {
    return;
  }

  if (RelocInfo::IsJSReturn(rmode())) {
    // Patch the frame exit code with a break point.
    SetDebugBreakAtReturn();
  } else if (IsDebugBreakSlot()) {
    // Patch the code in the break slot.
    SetDebugBreakAtSlot();
  } else {
    // Patch the IC call.
    SetDebugBreakAtIC();
  }
  ASSERT(IsDebugBreak());
}


void BreakLocationIterator::ClearDebugBreak() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) {
    return;
  }

  if (RelocInfo::IsJSReturn(rmode())) {
    // Restore the frame exit code.
    ClearDebugBreakAtReturn();
  } else if (IsDebugBreakSlot()) {
    // Restore the code in the break slot.
    ClearDebugBreakAtSlot();
  } else {
    // Patch the IC call.
    ClearDebugBreakAtIC();
  }
  ASSERT(!IsDebugBreak());
}


void BreakLocationIterator::PrepareStepIn(Isolate* isolate) {
  HandleScope scope(isolate);

  // Step in can only be prepared if currently positioned on an IC call,
  // construct call or CallFunction stub call.
  Address target = rinfo()->target_address();
  Handle<Code> target_code(Code::GetCodeFromTargetAddress(target));
  if (target_code->is_call_stub() || target_code->is_keyed_call_stub()) {
    // Step in through IC call is handled by the runtime system. Therefore make
    // sure that the any current IC is cleared and the runtime system is
    // called. If the executing code has a debug break at the location change
    // the call in the original code as it is the code there that will be
    // executed in place of the debug break call.
    Handle<Code> stub = ComputeCallDebugPrepareStepIn(
        target_code->arguments_count(), target_code->kind());
    if (IsDebugBreak()) {
      original_rinfo()->set_target_address(stub->entry());
    } else {
      rinfo()->set_target_address(stub->entry());
    }
  } else {
#ifdef DEBUG
    // All the following stuff is needed only for assertion checks so the code
    // is wrapped in ifdef.
    Handle<Code> maybe_call_function_stub = target_code;
    if (IsDebugBreak()) {
      Address original_target = original_rinfo()->target_address();
      maybe_call_function_stub =
          Handle<Code>(Code::GetCodeFromTargetAddress(original_target));
    }
    bool is_call_function_stub =
        (maybe_call_function_stub->kind() == Code::STUB &&
         maybe_call_function_stub->major_key() == CodeStub::CallFunction);

    // Step in through construct call requires no changes to the running code.
    // Step in through getters/setters should already be prepared as well
    // because caller of this function (Debug::PrepareStep) is expected to
    // flood the top frame's function with one shot breakpoints.
    // Step in through CallFunction stub should also be prepared by caller of
    // this function (Debug::PrepareStep) which should flood target function
    // with breakpoints.
    ASSERT(RelocInfo::IsConstructCall(rmode()) ||
           target_code->is_inline_cache_stub() ||
           is_call_function_stub);
#endif
  }
}


// Check whether the break point is at a position which will exit the function.
bool BreakLocationIterator::IsExit() const {
  return (RelocInfo::IsJSReturn(rmode()));
}


bool BreakLocationIterator::HasBreakPoint() {
  return debug_info_->HasBreakPoint(code_position());
}


// Check whether there is a debug break at the current position.
bool BreakLocationIterator::IsDebugBreak() {
  if (RelocInfo::IsJSReturn(rmode())) {
    return IsDebugBreakAtReturn();
  } else if (IsDebugBreakSlot()) {
    return IsDebugBreakAtSlot();
  } else {
    return Debug::IsDebugBreak(rinfo()->target_address());
  }
}


void BreakLocationIterator::SetDebugBreakAtIC() {
  // Patch the original code with the current address as the current address
  // might have changed by the inline caching since the code was copied.
  original_rinfo()->set_target_address(rinfo()->target_address());

  RelocInfo::Mode mode = rmode();
  if (RelocInfo::IsCodeTarget(mode)) {
    Address target = rinfo()->target_address();
    Handle<Code> target_code(Code::GetCodeFromTargetAddress(target));

    // Patch the code to invoke the builtin debug break function matching the
    // calling convention used by the call site.
    Handle<Code> dbgbrk_code(Debug::FindDebugBreak(target_code, mode));
    rinfo()->set_target_address(dbgbrk_code->entry());
  }
}


void BreakLocationIterator::ClearDebugBreakAtIC() {
  // Patch the code to the original invoke.
  rinfo()->set_target_address(original_rinfo()->target_address());
}


bool BreakLocationIterator::IsDebuggerStatement() {
  return RelocInfo::DEBUG_BREAK == rmode();
}


bool BreakLocationIterator::IsDebugBreakSlot() {
  return RelocInfo::DEBUG_BREAK_SLOT == rmode();
}


Object* BreakLocationIterator::BreakPointObjects() {
  return debug_info_->GetBreakPointObjects(code_position());
}


// Clear out all the debug break code. This is ONLY supposed to be used when
// shutting down the debugger as it will leave the break point information in
// DebugInfo even though the code is patched back to the non break point state.
void BreakLocationIterator::ClearAllDebugBreak() {
  while (!Done()) {
    ClearDebugBreak();
    Next();
  }
}


bool BreakLocationIterator::RinfoDone() const {
  ASSERT(reloc_iterator_->done() == reloc_iterator_original_->done());
  return reloc_iterator_->done();
}


void BreakLocationIterator::RinfoNext() {
  reloc_iterator_->next();
  reloc_iterator_original_->next();
#ifdef DEBUG
  ASSERT(reloc_iterator_->done() == reloc_iterator_original_->done());
  if (!reloc_iterator_->done()) {
    ASSERT(rmode() == original_rmode());
  }
#endif
}


// Threading support.
void Debug::ThreadInit() {
  thread_local_.break_count_ = 0;
  thread_local_.break_id_ = 0;
  thread_local_.break_frame_id_ = StackFrame::NO_ID;
  thread_local_.last_step_action_ = StepNone;
  thread_local_.last_statement_position_ = RelocInfo::kNoPosition;
  thread_local_.step_count_ = 0;
  thread_local_.last_fp_ = 0;
  thread_local_.queued_step_count_ = 0;
  thread_local_.step_into_fp_ = 0;
  thread_local_.step_out_fp_ = 0;
  thread_local_.after_break_target_ = 0;
  // TODO(isolates): frames_are_dropped_?
  thread_local_.debugger_entry_ = NULL;
  thread_local_.pending_interrupts_ = 0;
  thread_local_.restarter_frame_function_pointer_ = NULL;
}


char* Debug::ArchiveDebug(char* storage) {
  char* to = storage;
  OS::MemCopy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
  to += sizeof(ThreadLocal);
  OS::MemCopy(to, reinterpret_cast<char*>(&registers_), sizeof(registers_));
  ThreadInit();
  ASSERT(to <= storage + ArchiveSpacePerThread());
  return storage + ArchiveSpacePerThread();
}


char* Debug::RestoreDebug(char* storage) {
  char* from = storage;
  OS::MemCopy(
      reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
  from += sizeof(ThreadLocal);
  OS::MemCopy(reinterpret_cast<char*>(&registers_), from, sizeof(registers_));
  ASSERT(from <= storage + ArchiveSpacePerThread());
  return storage + ArchiveSpacePerThread();
}


int Debug::ArchiveSpacePerThread() {
  return sizeof(ThreadLocal) + sizeof(JSCallerSavedBuffer);
}


// Frame structure (conforms InternalFrame structure):
//   -- code
//   -- SMI maker
//   -- function (slot is called "context")
//   -- frame base
Object** Debug::SetUpFrameDropperFrame(StackFrame* bottom_js_frame,
                                       Handle<Code> code) {
  ASSERT(bottom_js_frame->is_java_script());

  Address fp = bottom_js_frame->fp();

  // Move function pointer into "context" slot.
  Memory::Object_at(fp + StandardFrameConstants::kContextOffset) =
      Memory::Object_at(fp + JavaScriptFrameConstants::kFunctionOffset);

  Memory::Object_at(fp + InternalFrameConstants::kCodeOffset) = *code;
  Memory::Object_at(fp + StandardFrameConstants::kMarkerOffset) =
      Smi::FromInt(StackFrame::INTERNAL);

  return reinterpret_cast<Object**>(&Memory::Object_at(
      fp + StandardFrameConstants::kContextOffset));
}

const int Debug::kFrameDropperFrameSize = 4;


void ScriptCache::Add(Handle<Script> script) {
  GlobalHandles* global_handles = Isolate::Current()->global_handles();
  // Create an entry in the hash map for the script.
  int id = Smi::cast(script->id())->value();
  HashMap::Entry* entry =
      HashMap::Lookup(reinterpret_cast<void*>(id), Hash(id), true);
  if (entry->value != NULL) {
    ASSERT(*script == *reinterpret_cast<Script**>(entry->value));
    return;
  }
  // Globalize the script object, make it weak and use the location of the
  // global handle as the value in the hash map.
  Handle<Script> script_ =
      Handle<Script>::cast(
          (global_handles->Create(*script)));
  global_handles->MakeWeak(reinterpret_cast<Object**>(script_.location()),
                           this,
                           ScriptCache::HandleWeakScript);
  entry->value = script_.location();
}


Handle<FixedArray> ScriptCache::GetScripts() {
  Factory* factory = Isolate::Current()->factory();
  Handle<FixedArray> instances = factory->NewFixedArray(occupancy());
  int count = 0;
  for (HashMap::Entry* entry = Start(); entry != NULL; entry = Next(entry)) {
    ASSERT(entry->value != NULL);
    if (entry->value != NULL) {
      instances->set(count, *reinterpret_cast<Script**>(entry->value));
      count++;
    }
  }
  return instances;
}


void ScriptCache::ProcessCollectedScripts() {
  Debugger* debugger = Isolate::Current()->debugger();
  for (int i = 0; i < collected_scripts_.length(); i++) {
    debugger->OnScriptCollected(collected_scripts_[i]);
  }
  collected_scripts_.Clear();
}


void ScriptCache::Clear() {
  GlobalHandles* global_handles = Isolate::Current()->global_handles();
  // Iterate the script cache to get rid of all the weak handles.
  for (HashMap::Entry* entry = Start(); entry != NULL; entry = Next(entry)) {
    ASSERT(entry != NULL);
    Object** location = reinterpret_cast<Object**>(entry->value);
    ASSERT((*location)->IsScript());
    global_handles->ClearWeakness(location);
    global_handles->Destroy(location);
  }
  // Clear the content of the hash map.
  HashMap::Clear();
}


void ScriptCache::HandleWeakScript(v8::Isolate* isolate,
                                   v8::Persistent<v8::Value>* obj,
                                   void* data) {
  ScriptCache* script_cache = reinterpret_cast<ScriptCache*>(data);
  // Find the location of the global handle.
  Script** location =
      reinterpret_cast<Script**>(Utils::OpenHandle(**obj).location());
  ASSERT((*location)->IsScript());

  // Remove the entry from the cache.
  int id = Smi::cast((*location)->id())->value();
  script_cache->Remove(reinterpret_cast<void*>(id), Hash(id));
  script_cache->collected_scripts_.Add(id);

  // Clear the weak handle.
  obj->Dispose(isolate);
}


void Debug::SetUp(bool create_heap_objects) {
  ThreadInit();
  if (create_heap_objects) {
    // Get code to handle debug break on return.
    debug_break_return_ =
        isolate_->builtins()->builtin(Builtins::kReturn_DebugBreak);
    ASSERT(debug_break_return_->IsCode());
    // Get code to handle debug break in debug break slots.
    debug_break_slot_ =
        isolate_->builtins()->builtin(Builtins::kSlot_DebugBreak);
    ASSERT(debug_break_slot_->IsCode());
  }
}


void Debug::HandleWeakDebugInfo(v8::Isolate* isolate,
                                v8::Persistent<v8::Value>* obj,
                                void* data) {
  Debug* debug = reinterpret_cast<Isolate*>(isolate)->debug();
  DebugInfoListNode* node = reinterpret_cast<DebugInfoListNode*>(data);
  // We need to clear all breakpoints associated with the function to restore
  // original code and avoid patching the code twice later because
  // the function will live in the heap until next gc, and can be found by
  // Debug::FindSharedFunctionInfoInScript.
  BreakLocationIterator it(node->debug_info(), ALL_BREAK_LOCATIONS);
  it.ClearAllDebugBreak();
  debug->RemoveDebugInfo(node->debug_info());
#ifdef DEBUG
  node = debug->debug_info_list_;
  while (node != NULL) {
    ASSERT(node != reinterpret_cast<DebugInfoListNode*>(data));
    node = node->next();
  }
#endif
}


DebugInfoListNode::DebugInfoListNode(DebugInfo* debug_info): next_(NULL) {
  GlobalHandles* global_handles = Isolate::Current()->global_handles();
  // Globalize the request debug info object and make it weak.
  debug_info_ = Handle<DebugInfo>::cast(
      (global_handles->Create(debug_info)));
  global_handles->MakeWeak(reinterpret_cast<Object**>(debug_info_.location()),
                           this,
                           Debug::HandleWeakDebugInfo);
}


DebugInfoListNode::~DebugInfoListNode() {
  Isolate::Current()->global_handles()->Destroy(
      reinterpret_cast<Object**>(debug_info_.location()));
}


bool Debug::CompileDebuggerScript(int index) {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  // Bail out if the index is invalid.
  if (index == -1) {
    return false;
  }

  // Find source and name for the requested script.
  Handle<String> source_code =
      isolate->bootstrapper()->NativesSourceLookup(index);
  Vector<const char> name = Natives::GetScriptName(index);
  Handle<String> script_name = factory->NewStringFromAscii(name);
  Handle<Context> context = isolate->native_context();

  // Compile the script.
  Handle<SharedFunctionInfo> function_info;
  function_info = Compiler::Compile(source_code,
                                    script_name,
                                    0, 0,
                                    context,
                                    NULL, NULL,
                                    Handle<String>::null(),
                                    NATIVES_CODE);

  // Silently ignore stack overflows during compilation.
  if (function_info.is_null()) {
    ASSERT(isolate->has_pending_exception());
    isolate->clear_pending_exception();
    return false;
  }

  // Execute the shared function in the debugger context.
  bool caught_exception;
  Handle<JSFunction> function =
      factory->NewFunctionFromSharedFunctionInfo(function_info, context);

  Handle<Object> exception =
      Execution::TryCall(function,
                         Handle<Object>(context->global_object(), isolate),
                         0,
                         NULL,
                         &caught_exception);

  // Check for caught exceptions.
  if (caught_exception) {
    ASSERT(!isolate->has_pending_exception());
    MessageLocation computed_location;
    isolate->ComputeLocation(&computed_location);
    Handle<Object> message = MessageHandler::MakeMessageObject(
        isolate, "error_loading_debugger", &computed_location,
        Vector<Handle<Object> >::empty(), Handle<String>(), Handle<JSArray>());
    ASSERT(!isolate->has_pending_exception());
    if (!exception.is_null()) {
      isolate->set_pending_exception(*exception);
      MessageHandler::ReportMessage(Isolate::Current(), NULL, message);
      isolate->clear_pending_exception();
    }
    return false;
  }

  // Mark this script as native and return successfully.
  Handle<Script> script(Script::cast(function->shared()->script()));
  script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
  return true;
}


bool Debug::Load() {
  // Return if debugger is already loaded.
  if (IsLoaded()) return true;

  Debugger* debugger = isolate_->debugger();

  // Bail out if we're already in the process of compiling the native
  // JavaScript source code for the debugger.
  if (debugger->compiling_natives() ||
      debugger->is_loading_debugger())
    return false;
  debugger->set_loading_debugger(true);

  // Disable breakpoints and interrupts while compiling and running the
  // debugger scripts including the context creation code.
  DisableBreak disable(true);
  PostponeInterruptsScope postpone(isolate_);

  // Create the debugger context.
  HandleScope scope(isolate_);
  Handle<Context> context =
      isolate_->bootstrapper()->CreateEnvironment(
          Handle<Object>::null(),
          v8::Handle<ObjectTemplate>(),
          NULL);

  // Fail if no context could be created.
  if (context.is_null()) return false;

  // Use the debugger context.
  SaveContext save(isolate_);
  isolate_->set_context(*context);

  // Expose the builtins object in the debugger context.
  Handle<String> key = isolate_->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("builtins"));
  Handle<GlobalObject> global = Handle<GlobalObject>(context->global_object());
  RETURN_IF_EMPTY_HANDLE_VALUE(
      isolate_,
      JSReceiver::SetProperty(global,
                              key,
                              Handle<Object>(global->builtins(), isolate_),
                              NONE,
                              kNonStrictMode),
      false);

  // Compile the JavaScript for the debugger in the debugger context.
  debugger->set_compiling_natives(true);
  bool caught_exception =
      !CompileDebuggerScript(Natives::GetIndex("mirror")) ||
      !CompileDebuggerScript(Natives::GetIndex("debug"));

  if (FLAG_enable_liveedit) {
    caught_exception = caught_exception ||
        !CompileDebuggerScript(Natives::GetIndex("liveedit"));
  }

  debugger->set_compiling_natives(false);

  // Make sure we mark the debugger as not loading before we might
  // return.
  debugger->set_loading_debugger(false);

  // Check for caught exceptions.
  if (caught_exception) return false;

  // Debugger loaded, create debugger context global handle.
  debug_context_ = Handle<Context>::cast(
      isolate_->global_handles()->Create(*context));

  return true;
}


void Debug::Unload() {
  // Return debugger is not loaded.
  if (!IsLoaded()) {
    return;
  }

  // Clear the script cache.
  DestroyScriptCache();

  // Clear debugger context global handle.
  isolate_->global_handles()->Destroy(
      reinterpret_cast<Object**>(debug_context_.location()));
  debug_context_ = Handle<Context>();
}


// Set the flag indicating that preemption happened during debugging.
void Debug::PreemptionWhileInDebugger() {
  ASSERT(InDebugger());
  Debug::set_interrupts_pending(PREEMPT);
}


void Debug::Iterate(ObjectVisitor* v) {
  v->VisitPointer(BitCast<Object**>(&(debug_break_return_)));
  v->VisitPointer(BitCast<Object**>(&(debug_break_slot_)));
}


Object* Debug::Break(Arguments args) {
  Heap* heap = isolate_->heap();
  HandleScope scope(isolate_);
  ASSERT(args.length() == 0);

  thread_local_.frame_drop_mode_ = FRAMES_UNTOUCHED;

  // Get the top-most JavaScript frame.
  JavaScriptFrameIterator it(isolate_);
  JavaScriptFrame* frame = it.frame();

  // Just continue if breaks are disabled or debugger cannot be loaded.
  if (disable_break() || !Load()) {
    SetAfterBreakTarget(frame);
    return heap->undefined_value();
  }

  // Enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) {
    return heap->undefined_value();
  }

  // Postpone interrupt during breakpoint processing.
  PostponeInterruptsScope postpone(isolate_);

  // Get the debug info (create it if it does not exist).
  Handle<SharedFunctionInfo> shared =
      Handle<SharedFunctionInfo>(JSFunction::cast(frame->function())->shared());
  Handle<DebugInfo> debug_info = GetDebugInfo(shared);

  // Find the break point where execution has stopped.
  BreakLocationIterator break_location_iterator(debug_info,
                                                ALL_BREAK_LOCATIONS);
  // pc points to the instruction after the current one, possibly a break
  // location as well. So the "- 1" to exclude it from the search.
  break_location_iterator.FindBreakLocationFromAddress(frame->pc() - 1);

  // Check whether step next reached a new statement.
  if (!StepNextContinue(&break_location_iterator, frame)) {
    // Decrease steps left if performing multiple steps.
    if (thread_local_.step_count_ > 0) {
      thread_local_.step_count_--;
    }
  }

  // If there is one or more real break points check whether any of these are
  // triggered.
  Handle<Object> break_points_hit(heap->undefined_value(), isolate_);
  if (break_location_iterator.HasBreakPoint()) {
    Handle<Object> break_point_objects =
        Handle<Object>(break_location_iterator.BreakPointObjects(), isolate_);
    break_points_hit = CheckBreakPoints(break_point_objects);
  }

  // If step out is active skip everything until the frame where we need to step
  // out to is reached, unless real breakpoint is hit.
  if (StepOutActive() && frame->fp() != step_out_fp() &&
      break_points_hit->IsUndefined() ) {
      // Step count should always be 0 for StepOut.
      ASSERT(thread_local_.step_count_ == 0);
  } else if (!break_points_hit->IsUndefined() ||
             (thread_local_.last_step_action_ != StepNone &&
              thread_local_.step_count_ == 0)) {
    // Notify debugger if a real break point is triggered or if performing
    // single stepping with no more steps to perform. Otherwise do another step.

    // Clear all current stepping setup.
    ClearStepping();

    if (thread_local_.queued_step_count_ > 0) {
      // Perform queued steps
      int step_count = thread_local_.queued_step_count_;

      // Clear queue
      thread_local_.queued_step_count_ = 0;

      PrepareStep(StepNext, step_count);
    } else {
      // Notify the debug event listeners.
      isolate_->debugger()->OnDebugBreak(break_points_hit, false);
    }
  } else if (thread_local_.last_step_action_ != StepNone) {
    // Hold on to last step action as it is cleared by the call to
    // ClearStepping.
    StepAction step_action = thread_local_.last_step_action_;
    int step_count = thread_local_.step_count_;

    // If StepNext goes deeper in code, StepOut until original frame
    // and keep step count queued up in the meantime.
    if (step_action == StepNext && frame->fp() < thread_local_.last_fp_) {
      // Count frames until target frame
      int count = 0;
      JavaScriptFrameIterator it(isolate_);
      while (!it.done() && it.frame()->fp() < thread_local_.last_fp_) {
        count++;
        it.Advance();
      }

      // Check that we indeed found the frame we are looking for.
      CHECK(!it.done() && (it.frame()->fp() == thread_local_.last_fp_));
      if (step_count > 1) {
        // Save old count and action to continue stepping after StepOut.
        thread_local_.queued_step_count_ = step_count - 1;
      }

      // Set up for StepOut to reach target frame.
      step_action = StepOut;
      step_count = count;
    }

    // Clear all current stepping setup.
    ClearStepping();

    // Set up for the remaining steps.
    PrepareStep(step_action, step_count);
  }

  if (thread_local_.frame_drop_mode_ == FRAMES_UNTOUCHED) {
    SetAfterBreakTarget(frame);
  } else if (thread_local_.frame_drop_mode_ ==
      FRAME_DROPPED_IN_IC_CALL) {
    // We must have been calling IC stub. Do not go there anymore.
    Code* plain_return = isolate_->builtins()->builtin(
        Builtins::kPlainReturn_LiveEdit);
    thread_local_.after_break_target_ = plain_return->entry();
  } else if (thread_local_.frame_drop_mode_ ==
      FRAME_DROPPED_IN_DEBUG_SLOT_CALL) {
    // Debug break slot stub does not return normally, instead it manually
    // cleans the stack and jumps. We should patch the jump address.
    Code* plain_return = isolate_->builtins()->builtin(
        Builtins::kFrameDropper_LiveEdit);
    thread_local_.after_break_target_ = plain_return->entry();
  } else if (thread_local_.frame_drop_mode_ ==
      FRAME_DROPPED_IN_DIRECT_CALL) {
    // Nothing to do, after_break_target is not used here.
  } else if (thread_local_.frame_drop_mode_ ==
      FRAME_DROPPED_IN_RETURN_CALL) {
    Code* plain_return = isolate_->builtins()->builtin(
        Builtins::kFrameDropper_LiveEdit);
    thread_local_.after_break_target_ = plain_return->entry();
  } else {
    UNREACHABLE();
  }

  return heap->undefined_value();
}


RUNTIME_FUNCTION(Object*, Debug_Break) {
  return isolate->debug()->Break(args);
}


// Check the break point objects for whether one or more are actually
// triggered. This function returns a JSArray with the break point objects
// which is triggered.
Handle<Object> Debug::CheckBreakPoints(Handle<Object> break_point_objects) {
  Factory* factory = isolate_->factory();

  // Count the number of break points hit. If there are multiple break points
  // they are in a FixedArray.
  Handle<FixedArray> break_points_hit;
  int break_points_hit_count = 0;
  ASSERT(!break_point_objects->IsUndefined());
  if (break_point_objects->IsFixedArray()) {
    Handle<FixedArray> array(FixedArray::cast(*break_point_objects));
    break_points_hit = factory->NewFixedArray(array->length());
    for (int i = 0; i < array->length(); i++) {
      Handle<Object> o(array->get(i), isolate_);
      if (CheckBreakPoint(o)) {
        break_points_hit->set(break_points_hit_count++, *o);
      }
    }
  } else {
    break_points_hit = factory->NewFixedArray(1);
    if (CheckBreakPoint(break_point_objects)) {
      break_points_hit->set(break_points_hit_count++, *break_point_objects);
    }
  }

  // Return undefined if no break points were triggered.
  if (break_points_hit_count == 0) {
    return factory->undefined_value();
  }
  // Return break points hit as a JSArray.
  Handle<JSArray> result = factory->NewJSArrayWithElements(break_points_hit);
  result->set_length(Smi::FromInt(break_points_hit_count));
  return result;
}


// Check whether a single break point object is triggered.
bool Debug::CheckBreakPoint(Handle<Object> break_point_object) {
  Factory* factory = isolate_->factory();
  HandleScope scope(isolate_);

  // Ignore check if break point object is not a JSObject.
  if (!break_point_object->IsJSObject()) return true;

  // Get the function IsBreakPointTriggered (defined in debug-debugger.js).
  Handle<String> is_break_point_triggered_string =
      factory->InternalizeOneByteString(
          STATIC_ASCII_VECTOR("IsBreakPointTriggered"));
  Handle<JSFunction> check_break_point =
    Handle<JSFunction>(JSFunction::cast(
        debug_context()->global_object()->GetPropertyNoExceptionThrown(
            *is_break_point_triggered_string)));

  // Get the break id as an object.
  Handle<Object> break_id = factory->NewNumberFromInt(Debug::break_id());

  // Call HandleBreakPointx.
  bool caught_exception;
  Handle<Object> argv[] = { break_id, break_point_object };
  Handle<Object> result = Execution::TryCall(check_break_point,
                                             isolate_->js_builtins_object(),
                                             ARRAY_SIZE(argv),
                                             argv,
                                             &caught_exception);

  // If exception or non boolean result handle as not triggered
  if (caught_exception || !result->IsBoolean()) {
    return false;
  }

  // Return whether the break point is triggered.
  ASSERT(!result.is_null());
  return (*result)->IsTrue();
}


// Check whether the function has debug information.
bool Debug::HasDebugInfo(Handle<SharedFunctionInfo> shared) {
  return !shared->debug_info()->IsUndefined();
}


// Return the debug info for this function. EnsureDebugInfo must be called
// prior to ensure the debug info has been generated for shared.
Handle<DebugInfo> Debug::GetDebugInfo(Handle<SharedFunctionInfo> shared) {
  ASSERT(HasDebugInfo(shared));
  return Handle<DebugInfo>(DebugInfo::cast(shared->debug_info()));
}


void Debug::SetBreakPoint(Handle<JSFunction> function,
                          Handle<Object> break_point_object,
                          int* source_position) {
  HandleScope scope(isolate_);

  PrepareForBreakPoints();

  // Make sure the function is compiled and has set up the debug info.
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if retrieving debug info failed.
    return;
  }

  Handle<DebugInfo> debug_info = GetDebugInfo(shared);
  // Source positions starts with zero.
  ASSERT(*source_position >= 0);

  // Find the break point and change it.
  BreakLocationIterator it(debug_info, SOURCE_BREAK_LOCATIONS);
  it.FindBreakLocationFromPosition(*source_position);
  it.SetBreakPoint(break_point_object);

  *source_position = it.position();

  // At least one active break point now.
  ASSERT(debug_info->GetBreakPointCount() > 0);
}


bool Debug::SetBreakPointForScript(Handle<Script> script,
                                   Handle<Object> break_point_object,
                                   int* source_position) {
  HandleScope scope(isolate_);

  PrepareForBreakPoints();

  // Obtain shared function info for the function.
  Object* result = FindSharedFunctionInfoInScript(script, *source_position);
  if (result->IsUndefined()) return false;

  // Make sure the function has set up the debug info.
  Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(result));
  if (!EnsureDebugInfo(shared, Handle<JSFunction>::null())) {
    // Return if retrieving debug info failed.
    return false;
  }

  // Find position within function. The script position might be before the
  // source position of the first function.
  int position;
  if (shared->start_position() > *source_position) {
    position = 0;
  } else {
    position = *source_position - shared->start_position();
  }

  Handle<DebugInfo> debug_info = GetDebugInfo(shared);
  // Source positions starts with zero.
  ASSERT(position >= 0);

  // Find the break point and change it.
  BreakLocationIterator it(debug_info, SOURCE_BREAK_LOCATIONS);
  it.FindBreakLocationFromPosition(position);
  it.SetBreakPoint(break_point_object);

  *source_position = it.position() + shared->start_position();

  // At least one active break point now.
  ASSERT(debug_info->GetBreakPointCount() > 0);
  return true;
}


void Debug::ClearBreakPoint(Handle<Object> break_point_object) {
  HandleScope scope(isolate_);

  DebugInfoListNode* node = debug_info_list_;
  while (node != NULL) {
    Object* result = DebugInfo::FindBreakPointInfo(node->debug_info(),
                                                   break_point_object);
    if (!result->IsUndefined()) {
      // Get information in the break point.
      BreakPointInfo* break_point_info = BreakPointInfo::cast(result);
      Handle<DebugInfo> debug_info = node->debug_info();

      // Find the break point and clear it.
      BreakLocationIterator it(debug_info, SOURCE_BREAK_LOCATIONS);
      it.FindBreakLocationFromAddress(debug_info->code()->entry() +
          break_point_info->code_position()->value());
      it.ClearBreakPoint(break_point_object);

      // If there are no more break points left remove the debug info for this
      // function.
      if (debug_info->GetBreakPointCount() == 0) {
        RemoveDebugInfo(debug_info);
      }

      return;
    }
    node = node->next();
  }
}


void Debug::ClearAllBreakPoints() {
  DebugInfoListNode* node = debug_info_list_;
  while (node != NULL) {
    // Remove all debug break code.
    BreakLocationIterator it(node->debug_info(), ALL_BREAK_LOCATIONS);
    it.ClearAllDebugBreak();
    node = node->next();
  }

  // Remove all debug info.
  while (debug_info_list_ != NULL) {
    RemoveDebugInfo(debug_info_list_->debug_info());
  }
}


void Debug::FloodWithOneShot(Handle<JSFunction> function) {
  PrepareForBreakPoints();

  // Make sure the function is compiled and has set up the debug info.
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if we failed to retrieve the debug info.
    return;
  }

  // Flood the function with break points.
  BreakLocationIterator it(GetDebugInfo(shared), ALL_BREAK_LOCATIONS);
  while (!it.Done()) {
    it.SetOneShot();
    it.Next();
  }
}


void Debug::FloodBoundFunctionWithOneShot(Handle<JSFunction> function) {
  Handle<FixedArray> new_bindings(function->function_bindings());
  Handle<Object> bindee(new_bindings->get(JSFunction::kBoundFunctionIndex),
                        isolate_);

  if (!bindee.is_null() && bindee->IsJSFunction() &&
      !JSFunction::cast(*bindee)->IsBuiltin()) {
    Handle<JSFunction> bindee_function(JSFunction::cast(*bindee));
    Debug::FloodWithOneShot(bindee_function);
  }
}


void Debug::FloodHandlerWithOneShot() {
  // Iterate through the JavaScript stack looking for handlers.
  StackFrame::Id id = break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there is no JavaScript stack don't do anything.
    return;
  }
  for (JavaScriptFrameIterator it(isolate_, id); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    if (frame->HasHandler()) {
      // Flood the function with the catch block with break points
      JSFunction* function = JSFunction::cast(frame->function());
      FloodWithOneShot(Handle<JSFunction>(function));
      return;
    }
  }
}


void Debug::ChangeBreakOnException(ExceptionBreakType type, bool enable) {
  if (type == BreakUncaughtException) {
    break_on_uncaught_exception_ = enable;
  } else {
    break_on_exception_ = enable;
  }
}


bool Debug::IsBreakOnException(ExceptionBreakType type) {
  if (type == BreakUncaughtException) {
    return break_on_uncaught_exception_;
  } else {
    return break_on_exception_;
  }
}


void Debug::PrepareStep(StepAction step_action, int step_count) {
  HandleScope scope(isolate_);

  PrepareForBreakPoints();

  ASSERT(Debug::InDebugger());

  // Remember this step action and count.
  thread_local_.last_step_action_ = step_action;
  if (step_action == StepOut) {
    // For step out target frame will be found on the stack so there is no need
    // to set step counter for it. It's expected to always be 0 for StepOut.
    thread_local_.step_count_ = 0;
  } else {
    thread_local_.step_count_ = step_count;
  }

  // Get the frame where the execution has stopped and skip the debug frame if
  // any. The debug frame will only be present if execution was stopped due to
  // hitting a break point. In other situations (e.g. unhandled exception) the
  // debug frame is not present.
  StackFrame::Id id = break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there is no JavaScript stack don't do anything.
    return;
  }
  JavaScriptFrameIterator frames_it(isolate_, id);
  JavaScriptFrame* frame = frames_it.frame();

  // First of all ensure there is one-shot break points in the top handler
  // if any.
  FloodHandlerWithOneShot();

  // If the function on the top frame is unresolved perform step out. This will
  // be the case when calling unknown functions and having the debugger stopped
  // in an unhandled exception.
  if (!frame->function()->IsJSFunction()) {
    // Step out: Find the calling JavaScript frame and flood it with
    // breakpoints.
    frames_it.Advance();
    // Fill the function to return to with one-shot break points.
    JSFunction* function = JSFunction::cast(frames_it.frame()->function());
    FloodWithOneShot(Handle<JSFunction>(function));
    return;
  }

  // Get the debug info (create it if it does not exist).
  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if ensuring debug info failed.
    return;
  }
  Handle<DebugInfo> debug_info = GetDebugInfo(shared);

  // Find the break location where execution has stopped.
  BreakLocationIterator it(debug_info, ALL_BREAK_LOCATIONS);
  // pc points to the instruction after the current one, possibly a break
  // location as well. So the "- 1" to exclude it from the search.
  it.FindBreakLocationFromAddress(frame->pc() - 1);

  // Compute whether or not the target is a call target.
  bool is_load_or_store = false;
  bool is_inline_cache_stub = false;
  bool is_at_restarted_function = false;
  Handle<Code> call_function_stub;

  if (thread_local_.restarter_frame_function_pointer_ == NULL) {
    if (RelocInfo::IsCodeTarget(it.rinfo()->rmode())) {
      bool is_call_target = false;
      Address target = it.rinfo()->target_address();
      Code* code = Code::GetCodeFromTargetAddress(target);
      if (code->is_call_stub() || code->is_keyed_call_stub()) {
        is_call_target = true;
      }
      if (code->is_inline_cache_stub()) {
        is_inline_cache_stub = true;
        is_load_or_store = !is_call_target;
      }

      // Check if target code is CallFunction stub.
      Code* maybe_call_function_stub = code;
      // If there is a breakpoint at this line look at the original code to
      // check if it is a CallFunction stub.
      if (it.IsDebugBreak()) {
        Address original_target = it.original_rinfo()->target_address();
        maybe_call_function_stub =
            Code::GetCodeFromTargetAddress(original_target);
      }
      if (maybe_call_function_stub->kind() == Code::STUB &&
          maybe_call_function_stub->major_key() == CodeStub::CallFunction) {
        // Save reference to the code as we may need it to find out arguments
        // count for 'step in' later.
        call_function_stub = Handle<Code>(maybe_call_function_stub);
      }
    }
  } else {
    is_at_restarted_function = true;
  }

  // If this is the last break code target step out is the only possibility.
  if (it.IsExit() || step_action == StepOut) {
    if (step_action == StepOut) {
      // Skip step_count frames starting with the current one.
      while (step_count-- > 0 && !frames_it.done()) {
        frames_it.Advance();
      }
    } else {
      ASSERT(it.IsExit());
      frames_it.Advance();
    }
    // Skip builtin functions on the stack.
    while (!frames_it.done() &&
           JSFunction::cast(frames_it.frame()->function())->IsBuiltin()) {
      frames_it.Advance();
    }
    // Step out: If there is a JavaScript caller frame, we need to
    // flood it with breakpoints.
    if (!frames_it.done()) {
      // Fill the function to return to with one-shot break points.
      JSFunction* function = JSFunction::cast(frames_it.frame()->function());
      FloodWithOneShot(Handle<JSFunction>(function));
      // Set target frame pointer.
      ActivateStepOut(frames_it.frame());
    }
  } else if (!(is_inline_cache_stub || RelocInfo::IsConstructCall(it.rmode()) ||
               !call_function_stub.is_null() || is_at_restarted_function)
             || step_action == StepNext || step_action == StepMin) {
    // Step next or step min.

    // Fill the current function with one-shot break points.
    FloodWithOneShot(function);

    // Remember source position and frame to handle step next.
    thread_local_.last_statement_position_ =
        debug_info->code()->SourceStatementPosition(frame->pc());
    thread_local_.last_fp_ = frame->UnpaddedFP();
  } else {
    // If there's restarter frame on top of the stack, just get the pointer
    // to function which is going to be restarted.
    if (is_at_restarted_function) {
      Handle<JSFunction> restarted_function(
          JSFunction::cast(*thread_local_.restarter_frame_function_pointer_));
      FloodWithOneShot(restarted_function);
    } else if (!call_function_stub.is_null()) {
      // If it's CallFunction stub ensure target function is compiled and flood
      // it with one shot breakpoints.

      // Find out number of arguments from the stub minor key.
      // Reverse lookup required as the minor key cannot be retrieved
      // from the code object.
      Handle<Object> obj(
          isolate_->heap()->code_stubs()->SlowReverseLookup(
              *call_function_stub),
          isolate_);
      ASSERT(!obj.is_null());
      ASSERT(!(*obj)->IsUndefined());
      ASSERT(obj->IsSmi());
      // Get the STUB key and extract major and minor key.
      uint32_t key = Smi::cast(*obj)->value();
      // Argc in the stub is the number of arguments passed - not the
      // expected arguments of the called function.
      int call_function_arg_count =
          CallFunctionStub::ExtractArgcFromMinorKey(
              CodeStub::MinorKeyFromKey(key));
      ASSERT(call_function_stub->major_key() ==
             CodeStub::MajorKeyFromKey(key));

      // Find target function on the expression stack.
      // Expression stack looks like this (top to bottom):
      // argN
      // ...
      // arg0
      // Receiver
      // Function to call
      int expressions_count = frame->ComputeExpressionsCount();
      ASSERT(expressions_count - 2 - call_function_arg_count >= 0);
      Object* fun = frame->GetExpression(
          expressions_count - 2 - call_function_arg_count);
      if (fun->IsJSFunction()) {
        Handle<JSFunction> js_function(JSFunction::cast(fun));
        if (js_function->shared()->bound()) {
          Debug::FloodBoundFunctionWithOneShot(js_function);
        } else if (!js_function->IsBuiltin()) {
          // Don't step into builtins.
          // It will also compile target function if it's not compiled yet.
          FloodWithOneShot(js_function);
        }
      }
    }

    // Fill the current function with one-shot break points even for step in on
    // a call target as the function called might be a native function for
    // which step in will not stop. It also prepares for stepping in
    // getters/setters.
    FloodWithOneShot(function);

    if (is_load_or_store) {
      // Remember source position and frame to handle step in getter/setter. If
      // there is a custom getter/setter it will be handled in
      // Object::Get/SetPropertyWithCallback, otherwise the step action will be
      // propagated on the next Debug::Break.
      thread_local_.last_statement_position_ =
          debug_info->code()->SourceStatementPosition(frame->pc());
      thread_local_.last_fp_ = frame->UnpaddedFP();
    }

    // Step in or Step in min
    it.PrepareStepIn(isolate_);
    ActivateStepIn(frame);
  }
}


// Check whether the current debug break should be reported to the debugger. It
// is used to have step next and step in only report break back to the debugger
// if on a different frame or in a different statement. In some situations
// there will be several break points in the same statement when the code is
// flooded with one-shot break points. This function helps to perform several
// steps before reporting break back to the debugger.
bool Debug::StepNextContinue(BreakLocationIterator* break_location_iterator,
                             JavaScriptFrame* frame) {
  // StepNext and StepOut shouldn't bring us deeper in code, so last frame
  // shouldn't be a parent of current frame.
  if (thread_local_.last_step_action_ == StepNext ||
      thread_local_.last_step_action_ == StepOut) {
    if (frame->fp() < thread_local_.last_fp_) return true;
  }

  // If the step last action was step next or step in make sure that a new
  // statement is hit.
  if (thread_local_.last_step_action_ == StepNext ||
      thread_local_.last_step_action_ == StepIn) {
    // Never continue if returning from function.
    if (break_location_iterator->IsExit()) return false;

    // Continue if we are still on the same frame and in the same statement.
    int current_statement_position =
        break_location_iterator->code()->SourceStatementPosition(frame->pc());
    return thread_local_.last_fp_ == frame->UnpaddedFP() &&
        thread_local_.last_statement_position_ == current_statement_position;
  }

  // No step next action - don't continue.
  return false;
}


// Check whether the code object at the specified address is a debug break code
// object.
bool Debug::IsDebugBreak(Address addr) {
  Code* code = Code::GetCodeFromTargetAddress(addr);
  return code->is_debug_break();
}


// Check whether a code stub with the specified major key is a possible break
// point location when looking for source break locations.
bool Debug::IsSourceBreakStub(Code* code) {
  CodeStub::Major major_key = CodeStub::GetMajorKey(code);
  return major_key == CodeStub::CallFunction;
}


// Check whether a code stub with the specified major key is a possible break
// location.
bool Debug::IsBreakStub(Code* code) {
  CodeStub::Major major_key = CodeStub::GetMajorKey(code);
  return major_key == CodeStub::CallFunction;
}


// Find the builtin to use for invoking the debug break
Handle<Code> Debug::FindDebugBreak(Handle<Code> code, RelocInfo::Mode mode) {
  Isolate* isolate = Isolate::Current();

  // Find the builtin debug break function matching the calling convention
  // used by the call site.
  if (code->is_inline_cache_stub()) {
    switch (code->kind()) {
      case Code::CALL_IC:
      case Code::KEYED_CALL_IC:
        return isolate->stub_cache()->ComputeCallDebugBreak(
            code->arguments_count(), code->kind());

      case Code::LOAD_IC:
        return isolate->builtins()->LoadIC_DebugBreak();

      case Code::STORE_IC:
        return isolate->builtins()->StoreIC_DebugBreak();

      case Code::KEYED_LOAD_IC:
        return isolate->builtins()->KeyedLoadIC_DebugBreak();

      case Code::KEYED_STORE_IC:
        return isolate->builtins()->KeyedStoreIC_DebugBreak();

      case Code::COMPARE_NIL_IC:
        return isolate->builtins()->CompareNilIC_DebugBreak();

      default:
        UNREACHABLE();
    }
  }
  if (RelocInfo::IsConstructCall(mode)) {
    if (code->has_function_cache()) {
      return isolate->builtins()->CallConstructStub_Recording_DebugBreak();
    } else {
      return isolate->builtins()->CallConstructStub_DebugBreak();
    }
  }
  if (code->kind() == Code::STUB) {
    ASSERT(code->major_key() == CodeStub::CallFunction);
    if (code->has_function_cache()) {
      return isolate->builtins()->CallFunctionStub_Recording_DebugBreak();
    } else {
      return isolate->builtins()->CallFunctionStub_DebugBreak();
    }
  }

  UNREACHABLE();
  return Handle<Code>::null();
}


// Simple function for returning the source positions for active break points.
Handle<Object> Debug::GetSourceBreakLocations(
    Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = Isolate::Current();
  Heap* heap = isolate->heap();
  if (!HasDebugInfo(shared)) {
    return Handle<Object>(heap->undefined_value(), isolate);
  }
  Handle<DebugInfo> debug_info = GetDebugInfo(shared);
  if (debug_info->GetBreakPointCount() == 0) {
    return Handle<Object>(heap->undefined_value(), isolate);
  }
  Handle<FixedArray> locations =
      isolate->factory()->NewFixedArray(debug_info->GetBreakPointCount());
  int count = 0;
  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (!debug_info->break_points()->get(i)->IsUndefined()) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(debug_info->break_points()->get(i));
      if (break_point_info->GetBreakPointCount() > 0) {
        locations->set(count++, break_point_info->statement_position());
      }
    }
  }
  return locations;
}


void Debug::NewBreak(StackFrame::Id break_frame_id) {
  thread_local_.break_frame_id_ = break_frame_id;
  thread_local_.break_id_ = ++thread_local_.break_count_;
}


void Debug::SetBreak(StackFrame::Id break_frame_id, int break_id) {
  thread_local_.break_frame_id_ = break_frame_id;
  thread_local_.break_id_ = break_id;
}


// Handle stepping into a function.
void Debug::HandleStepIn(Handle<JSFunction> function,
                         Handle<Object> holder,
                         Address fp,
                         bool is_constructor) {
  Isolate* isolate = function->GetIsolate();
  // If the frame pointer is not supplied by the caller find it.
  if (fp == 0) {
    StackFrameIterator it(isolate);
    it.Advance();
    // For constructor functions skip another frame.
    if (is_constructor) {
      ASSERT(it.frame()->is_construct());
      it.Advance();
    }
    fp = it.frame()->fp();
  }

  // Flood the function with one-shot break points if it is called from where
  // step into was requested.
  if (fp == step_in_fp()) {
    if (function->shared()->bound()) {
      // Handle Function.prototype.bind
      Debug::FloodBoundFunctionWithOneShot(function);
    } else if (!function->IsBuiltin()) {
      // Don't allow step into functions in the native context.
      if (function->shared()->code() ==
          isolate->builtins()->builtin(Builtins::kFunctionApply) ||
          function->shared()->code() ==
          isolate->builtins()->builtin(Builtins::kFunctionCall)) {
        // Handle function.apply and function.call separately to flood the
        // function to be called and not the code for Builtins::FunctionApply or
        // Builtins::FunctionCall. The receiver of call/apply is the target
        // function.
        if (!holder.is_null() && holder->IsJSFunction() &&
            !JSFunction::cast(*holder)->IsBuiltin()) {
          Handle<JSFunction> js_function = Handle<JSFunction>::cast(holder);
          Debug::FloodWithOneShot(js_function);
        }
      } else {
        Debug::FloodWithOneShot(function);
      }
    }
  }
}


void Debug::ClearStepping() {
  // Clear the various stepping setup.
  ClearOneShot();
  ClearStepIn();
  ClearStepOut();
  ClearStepNext();

  // Clear multiple step counter.
  thread_local_.step_count_ = 0;
}

// Clears all the one-shot break points that are currently set. Normally this
// function is called each time a break point is hit as one shot break points
// are used to support stepping.
void Debug::ClearOneShot() {
  // The current implementation just runs through all the breakpoints. When the
  // last break point for a function is removed that function is automatically
  // removed from the list.

  DebugInfoListNode* node = debug_info_list_;
  while (node != NULL) {
    BreakLocationIterator it(node->debug_info(), ALL_BREAK_LOCATIONS);
    while (!it.Done()) {
      it.ClearOneShot();
      it.Next();
    }
    node = node->next();
  }
}


void Debug::ActivateStepIn(StackFrame* frame) {
  ASSERT(!StepOutActive());
  thread_local_.step_into_fp_ = frame->UnpaddedFP();
}


void Debug::ClearStepIn() {
  thread_local_.step_into_fp_ = 0;
}


void Debug::ActivateStepOut(StackFrame* frame) {
  ASSERT(!StepInActive());
  thread_local_.step_out_fp_ = frame->UnpaddedFP();
}


void Debug::ClearStepOut() {
  thread_local_.step_out_fp_ = 0;
}


void Debug::ClearStepNext() {
  thread_local_.last_step_action_ = StepNone;
  thread_local_.last_statement_position_ = RelocInfo::kNoPosition;
  thread_local_.last_fp_ = 0;
}


// Helper function to compile full code for debugging. This code will
// have debug break slots and deoptimization information. Deoptimization
// information is required in case that an optimized version of this
// function is still activated on the stack. It will also make sure that
// the full code is compiled with the same flags as the previous version,
// that is flags which can change the code generated. The current method
// of mapping from already compiled full code without debug break slots
// to full code with debug break slots depends on the generated code is
// otherwise exactly the same.
static bool CompileFullCodeForDebugging(Handle<JSFunction> function,
                                        Handle<Code> current_code) {
  ASSERT(!current_code->has_debug_break_slots());

  CompilationInfoWithZone info(function);
  info.MarkCompilingForDebugging(current_code);
  ASSERT(!info.shared_info()->is_compiled());
  ASSERT(!info.isolate()->has_pending_exception());

  // Use compile lazy which will end up compiling the full code in the
  // configuration configured above.
  bool result = Compiler::CompileLazy(&info);
  ASSERT(result != Isolate::Current()->has_pending_exception());
  info.isolate()->clear_pending_exception();
#if DEBUG
  if (result) {
    Handle<Code> new_code(function->shared()->code());
    ASSERT(new_code->has_debug_break_slots());
    ASSERT(current_code->is_compiled_optimizable() ==
           new_code->is_compiled_optimizable());
  }
#endif
  return result;
}


static void CollectActiveFunctionsFromThread(
    Isolate* isolate,
    ThreadLocalTop* top,
    List<Handle<JSFunction> >* active_functions,
    Object* active_code_marker) {
  // Find all non-optimized code functions with activation frames
  // on the stack. This includes functions which have optimized
  // activations (including inlined functions) on the stack as the
  // non-optimized code is needed for the lazy deoptimization.
  for (JavaScriptFrameIterator it(isolate, top); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    if (frame->is_optimized()) {
      List<JSFunction*> functions(Compiler::kMaxInliningLevels + 1);
      frame->GetFunctions(&functions);
      for (int i = 0; i < functions.length(); i++) {
        JSFunction* function = functions[i];
        active_functions->Add(Handle<JSFunction>(function));
        function->shared()->code()->set_gc_metadata(active_code_marker);
      }
    } else if (frame->function()->IsJSFunction()) {
      JSFunction* function = JSFunction::cast(frame->function());
      ASSERT(frame->LookupCode()->kind() == Code::FUNCTION);
      active_functions->Add(Handle<JSFunction>(function));
      function->shared()->code()->set_gc_metadata(active_code_marker);
    }
  }
}


static void RedirectActivationsToRecompiledCodeOnThread(
    Isolate* isolate,
    ThreadLocalTop* top) {
  for (JavaScriptFrameIterator it(isolate, top); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();

    if (frame->is_optimized() || !frame->function()->IsJSFunction()) continue;

    JSFunction* function = JSFunction::cast(frame->function());

    ASSERT(frame->LookupCode()->kind() == Code::FUNCTION);

    Handle<Code> frame_code(frame->LookupCode());
    if (frame_code->has_debug_break_slots()) continue;

    Handle<Code> new_code(function->shared()->code());
    if (new_code->kind() != Code::FUNCTION ||
        !new_code->has_debug_break_slots()) {
      continue;
    }

    // Iterate over the RelocInfo in the original code to compute the sum of the
    // constant pools sizes. (See Assembler::CheckConstPool())
    // Note that this is only useful for architectures using constant pools.
    int constpool_mask = RelocInfo::ModeMask(RelocInfo::CONST_POOL);
    int frame_const_pool_size = 0;
    for (RelocIterator it(*frame_code, constpool_mask); !it.done(); it.next()) {
      RelocInfo* info = it.rinfo();
      if (info->pc() >= frame->pc()) break;
      frame_const_pool_size += static_cast<int>(info->data());
    }
    intptr_t frame_offset =
      frame->pc() - frame_code->instruction_start() - frame_const_pool_size;

    // Iterate over the RelocInfo for new code to find the number of bytes
    // generated for debug slots and constant pools.
    int debug_break_slot_bytes = 0;
    int new_code_const_pool_size = 0;
    int mask = RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT) |
               RelocInfo::ModeMask(RelocInfo::CONST_POOL);
    for (RelocIterator it(*new_code, mask); !it.done(); it.next()) {
      // Check if the pc in the new code with debug break
      // slots is before this slot.
      RelocInfo* info = it.rinfo();
      intptr_t new_offset = info->pc() - new_code->instruction_start() -
                            new_code_const_pool_size - debug_break_slot_bytes;
      if (new_offset >= frame_offset) {
        break;
      }

      if (RelocInfo::IsDebugBreakSlot(info->rmode())) {
        debug_break_slot_bytes += Assembler::kDebugBreakSlotLength;
      } else {
        ASSERT(RelocInfo::IsConstPool(info->rmode()));
        // The size of the constant pool is encoded in the data.
        new_code_const_pool_size += static_cast<int>(info->data());
      }
    }

    // Compute the equivalent pc in the new code.
    byte* new_pc = new_code->instruction_start() + frame_offset +
                   debug_break_slot_bytes + new_code_const_pool_size;

    if (FLAG_trace_deopt) {
      PrintF("Replacing code %08" V8PRIxPTR " - %08" V8PRIxPTR " (%d) "
             "with %08" V8PRIxPTR " - %08" V8PRIxPTR " (%d) "
             "for debugging, "
             "changing pc from %08" V8PRIxPTR " to %08" V8PRIxPTR "\n",
             reinterpret_cast<intptr_t>(
                 frame_code->instruction_start()),
             reinterpret_cast<intptr_t>(
                 frame_code->instruction_start()) +
             frame_code->instruction_size(),
             frame_code->instruction_size(),
             reinterpret_cast<intptr_t>(new_code->instruction_start()),
             reinterpret_cast<intptr_t>(new_code->instruction_start()) +
             new_code->instruction_size(),
             new_code->instruction_size(),
             reinterpret_cast<intptr_t>(frame->pc()),
             reinterpret_cast<intptr_t>(new_pc));
    }

    // Patch the return address to return into the code with
    // debug break slots.
    frame->set_pc(new_pc);
  }
}


class ActiveFunctionsCollector : public ThreadVisitor {
 public:
  explicit ActiveFunctionsCollector(List<Handle<JSFunction> >* active_functions,
                                    Object* active_code_marker)
      : active_functions_(active_functions),
        active_code_marker_(active_code_marker) { }

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    CollectActiveFunctionsFromThread(isolate,
                                     top,
                                     active_functions_,
                                     active_code_marker_);
  }

 private:
  List<Handle<JSFunction> >* active_functions_;
  Object* active_code_marker_;
};


class ActiveFunctionsRedirector : public ThreadVisitor {
 public:
  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    RedirectActivationsToRecompiledCodeOnThread(isolate, top);
  }
};


void Debug::PrepareForBreakPoints() {
  // If preparing for the first break point make sure to deoptimize all
  // functions as debugging does not work with optimized code.
  if (!has_break_points_) {
    Deoptimizer::DeoptimizeAll(isolate_);

    Handle<Code> lazy_compile =
        Handle<Code>(isolate_->builtins()->builtin(Builtins::kLazyCompile));

    // There will be at least one break point when we are done.
    has_break_points_ = true;

    // Keep the list of activated functions in a handlified list as it
    // is used both in GC and non-GC code.
    List<Handle<JSFunction> > active_functions(100);

    {
      // We are going to iterate heap to find all functions without
      // debug break slots.
      Heap* heap = isolate_->heap();
      heap->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                              "preparing for breakpoints");

      // Ensure no GC in this scope as we are going to use gc_metadata
      // field in the Code object to mark active functions.
      DisallowHeapAllocation no_allocation;

      Object* active_code_marker = heap->the_hole_value();

      CollectActiveFunctionsFromThread(isolate_,
                                       isolate_->thread_local_top(),
                                       &active_functions,
                                       active_code_marker);
      ActiveFunctionsCollector active_functions_collector(&active_functions,
                                                          active_code_marker);
      isolate_->thread_manager()->IterateArchivedThreads(
          &active_functions_collector);

      // Scan the heap for all non-optimized functions which have no
      // debug break slots and are not active or inlined into an active
      // function and mark them for lazy compilation.
      HeapIterator iterator(heap);
      HeapObject* obj = NULL;
      while (((obj = iterator.next()) != NULL)) {
        if (obj->IsJSFunction()) {
          JSFunction* function = JSFunction::cast(obj);
          SharedFunctionInfo* shared = function->shared();
          if (shared->allows_lazy_compilation() &&
              shared->script()->IsScript() &&
              function->code()->kind() == Code::FUNCTION &&
              !function->code()->has_debug_break_slots() &&
              shared->code()->gc_metadata() != active_code_marker) {
            function->set_code(*lazy_compile);
            function->shared()->set_code(*lazy_compile);
          }
        }
      }

      // Clear gc_metadata field.
      for (int i = 0; i < active_functions.length(); i++) {
        Handle<JSFunction> function = active_functions[i];
        function->shared()->code()->set_gc_metadata(Smi::FromInt(0));
      }
    }

    // Now recompile all functions with activation frames and and
    // patch the return address to run in the new compiled code.
    for (int i = 0; i < active_functions.length(); i++) {
      Handle<JSFunction> function = active_functions[i];
      Handle<SharedFunctionInfo> shared(function->shared());

      if (function->code()->kind() == Code::FUNCTION &&
          function->code()->has_debug_break_slots()) {
        // Nothing to do. Function code already had debug break slots.
        continue;
      }

      // If recompilation is not possible just skip it.
      if (shared->is_toplevel() ||
          !shared->allows_lazy_compilation() ||
          shared->code()->kind() == Code::BUILTIN) {
        continue;
      }

      // Make sure that the shared full code is compiled with debug
      // break slots.
      if (!shared->code()->has_debug_break_slots()) {
        // Try to compile the full code with debug break slots. If it
        // fails just keep the current code.
        Handle<Code> current_code(function->shared()->code());
        shared->set_code(*lazy_compile);
        bool prev_force_debugger_active =
            isolate_->debugger()->force_debugger_active();
        isolate_->debugger()->set_force_debugger_active(true);
        ASSERT(current_code->kind() == Code::FUNCTION);
        CompileFullCodeForDebugging(function, current_code);
        isolate_->debugger()->set_force_debugger_active(
            prev_force_debugger_active);
        if (!shared->is_compiled()) {
          shared->set_code(*current_code);
          continue;
        }
      }

      // Keep function code in sync with shared function info.
      function->set_code(shared->code());
    }

    RedirectActivationsToRecompiledCodeOnThread(isolate_,
                                                isolate_->thread_local_top());

    ActiveFunctionsRedirector active_functions_redirector;
    isolate_->thread_manager()->IterateArchivedThreads(
          &active_functions_redirector);
  }
}


Object* Debug::FindSharedFunctionInfoInScript(Handle<Script> script,
                                              int position) {
  // Iterate the heap looking for SharedFunctionInfo generated from the
  // script. The inner most SharedFunctionInfo containing the source position
  // for the requested break point is found.
  // NOTE: This might require several heap iterations. If the SharedFunctionInfo
  // which is found is not compiled it is compiled and the heap is iterated
  // again as the compilation might create inner functions from the newly
  // compiled function and the actual requested break point might be in one of
  // these functions.
  // NOTE: The below fix-point iteration depends on all functions that cannot be
  // compiled lazily without a context to not be compiled at all. Compilation
  // will be triggered at points where we do not need a context.
  bool done = false;
  // The current candidate for the source position:
  int target_start_position = RelocInfo::kNoPosition;
  Handle<JSFunction> target_function;
  Handle<SharedFunctionInfo> target;
  Heap* heap = isolate_->heap();
  while (!done) {
    { // Extra scope for iterator and no-allocation.
      heap->EnsureHeapIsIterable();
      DisallowHeapAllocation no_alloc_during_heap_iteration;
      HeapIterator iterator(heap);
      for (HeapObject* obj = iterator.next();
           obj != NULL; obj = iterator.next()) {
        bool found_next_candidate = false;
        Handle<JSFunction> function;
        Handle<SharedFunctionInfo> shared;
        if (obj->IsJSFunction()) {
          function = Handle<JSFunction>(JSFunction::cast(obj));
          shared = Handle<SharedFunctionInfo>(function->shared());
          ASSERT(shared->allows_lazy_compilation() || shared->is_compiled());
          found_next_candidate = true;
        } else if (obj->IsSharedFunctionInfo()) {
          shared = Handle<SharedFunctionInfo>(SharedFunctionInfo::cast(obj));
          // Skip functions that we cannot compile lazily without a context,
          // which is not available here, because there is no closure.
          found_next_candidate = shared->is_compiled() ||
              shared->allows_lazy_compilation_without_context();
        }
        if (!found_next_candidate) continue;
        if (shared->script() == *script) {
          // If the SharedFunctionInfo found has the requested script data and
          // contains the source position it is a candidate.
          int start_position = shared->function_token_position();
          if (start_position == RelocInfo::kNoPosition) {
            start_position = shared->start_position();
          }
          if (start_position <= position &&
              position <= shared->end_position()) {
            // If there is no candidate or this function is within the current
            // candidate this is the new candidate.
            if (target.is_null()) {
              target_start_position = start_position;
              target_function = function;
              target = shared;
            } else {
              if (target_start_position == start_position &&
                  shared->end_position() == target->end_position()) {
                // If a top-level function contains only one function
                // declaration the source for the top-level and the function
                // is the same. In that case prefer the non top-level function.
                if (!shared->is_toplevel()) {
                  target_start_position = start_position;
                  target_function = function;
                  target = shared;
                }
              } else if (target_start_position <= start_position &&
                         shared->end_position() <= target->end_position()) {
                // This containment check includes equality as a function
                // inside a top-level function can share either start or end
                // position with the top-level function.
                target_start_position = start_position;
                target_function = function;
                target = shared;
              }
            }
          }
        }
      }  // End for loop.
    }  // End no-allocation scope.

    if (target.is_null()) return heap->undefined_value();

    // There will be at least one break point when we are done.
    has_break_points_ = true;

    // If the candidate found is compiled we are done.
    done = target->is_compiled();
    if (!done) {
      // If the candidate is not compiled, compile it to reveal any inner
      // functions which might contain the requested source position. This
      // will compile all inner functions that cannot be compiled without a
      // context, because Compiler::BuildFunctionInfo checks whether the
      // debugger is active.
      if (target_function.is_null()) {
        SharedFunctionInfo::CompileLazy(target, KEEP_EXCEPTION);
      } else {
        JSFunction::CompileLazy(target_function, KEEP_EXCEPTION);
      }
    }
  }  // End while loop.

  return *target;
}


// Ensures the debug information is present for shared.
bool Debug::EnsureDebugInfo(Handle<SharedFunctionInfo> shared,
                            Handle<JSFunction> function) {
  Isolate* isolate = shared->GetIsolate();

  // Return if we already have the debug info for shared.
  if (HasDebugInfo(shared)) {
    ASSERT(shared->is_compiled());
    return true;
  }

  // There will be at least one break point when we are done.
  has_break_points_ = true;

  // Ensure function is compiled. Return false if this failed.
  if (!function.is_null() &&
      !JSFunction::EnsureCompiled(function, CLEAR_EXCEPTION)) {
    return false;
  }

  // Create the debug info object.
  Handle<DebugInfo> debug_info = isolate->factory()->NewDebugInfo(shared);

  // Add debug info to the list.
  DebugInfoListNode* node = new DebugInfoListNode(*debug_info);
  node->set_next(debug_info_list_);
  debug_info_list_ = node;

  return true;
}


void Debug::RemoveDebugInfo(Handle<DebugInfo> debug_info) {
  ASSERT(debug_info_list_ != NULL);
  // Run through the debug info objects to find this one and remove it.
  DebugInfoListNode* prev = NULL;
  DebugInfoListNode* current = debug_info_list_;
  while (current != NULL) {
    if (*current->debug_info() == *debug_info) {
      // Unlink from list. If prev is NULL we are looking at the first element.
      if (prev == NULL) {
        debug_info_list_ = current->next();
      } else {
        prev->set_next(current->next());
      }
      current->debug_info()->shared()->set_debug_info(
              isolate_->heap()->undefined_value());
      delete current;

      // If there are no more debug info objects there are not more break
      // points.
      has_break_points_ = debug_info_list_ != NULL;

      return;
    }
    // Move to next in list.
    prev = current;
    current = current->next();
  }
  UNREACHABLE();
}


void Debug::SetAfterBreakTarget(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);

  PrepareForBreakPoints();

  // Get the executing function in which the debug break occurred.
  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if we failed to retrieve the debug info.
    return;
  }
  Handle<DebugInfo> debug_info = GetDebugInfo(shared);
  Handle<Code> code(debug_info->code());
  Handle<Code> original_code(debug_info->original_code());
#ifdef DEBUG
  // Get the code which is actually executing.
  Handle<Code> frame_code(frame->LookupCode());
  ASSERT(frame_code.is_identical_to(code));
#endif

  // Find the call address in the running code. This address holds the call to
  // either a DebugBreakXXX or to the debug break return entry code if the
  // break point is still active after processing the break point.
  Address addr = frame->pc() - Assembler::kPatchDebugBreakSlotReturnOffset;

  // Check if the location is at JS exit or debug break slot.
  bool at_js_return = false;
  bool break_at_js_return_active = false;
  bool at_debug_break_slot = false;
  RelocIterator it(debug_info->code());
  while (!it.done() && !at_js_return && !at_debug_break_slot) {
    if (RelocInfo::IsJSReturn(it.rinfo()->rmode())) {
      at_js_return = (it.rinfo()->pc() ==
          addr - Assembler::kPatchReturnSequenceAddressOffset);
      break_at_js_return_active = it.rinfo()->IsPatchedReturnSequence();
    }
    if (RelocInfo::IsDebugBreakSlot(it.rinfo()->rmode())) {
      at_debug_break_slot = (it.rinfo()->pc() ==
          addr - Assembler::kPatchDebugBreakSlotAddressOffset);
    }
    it.next();
  }

  // Handle the jump to continue execution after break point depending on the
  // break location.
  if (at_js_return) {
    // If the break point as return is still active jump to the corresponding
    // place in the original code. If not the break point was removed during
    // break point processing.
    if (break_at_js_return_active) {
      addr +=  original_code->instruction_start() - code->instruction_start();
    }

    // Move back to where the call instruction sequence started.
    thread_local_.after_break_target_ =
        addr - Assembler::kPatchReturnSequenceAddressOffset;
  } else if (at_debug_break_slot) {
    // Address of where the debug break slot starts.
    addr = addr - Assembler::kPatchDebugBreakSlotAddressOffset;

    // Continue just after the slot.
    thread_local_.after_break_target_ = addr + Assembler::kDebugBreakSlotLength;
  } else if (IsDebugBreak(Assembler::target_address_at(addr))) {
    // We now know that there is still a debug break call at the target address,
    // so the break point is still there and the original code will hold the
    // address to jump to in order to complete the call which is replaced by a
    // call to DebugBreakXXX.

    // Find the corresponding address in the original code.
    addr += original_code->instruction_start() - code->instruction_start();

    // Install jump to the call address in the original code. This will be the
    // call which was overwritten by the call to DebugBreakXXX.
    thread_local_.after_break_target_ = Assembler::target_address_at(addr);
  } else {
    // There is no longer a break point present. Don't try to look in the
    // original code as the running code will have the right address. This takes
    // care of the case where the last break point is removed from the function
    // and therefore no "original code" is available.
    thread_local_.after_break_target_ = Assembler::target_address_at(addr);
  }
}


bool Debug::IsBreakAtReturn(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);

  // If there are no break points this cannot be break at return, as
  // the debugger statement and stack guard bebug break cannot be at
  // return.
  if (!has_break_points_) {
    return false;
  }

  PrepareForBreakPoints();

  // Get the executing function in which the debug break occurred.
  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if we failed to retrieve the debug info.
    return false;
  }
  Handle<DebugInfo> debug_info = GetDebugInfo(shared);
  Handle<Code> code(debug_info->code());
#ifdef DEBUG
  // Get the code which is actually executing.
  Handle<Code> frame_code(frame->LookupCode());
  ASSERT(frame_code.is_identical_to(code));
#endif

  // Find the call address in the running code.
  Address addr = frame->pc() - Assembler::kPatchDebugBreakSlotReturnOffset;

  // Check if the location is at JS return.
  RelocIterator it(debug_info->code());
  while (!it.done()) {
    if (RelocInfo::IsJSReturn(it.rinfo()->rmode())) {
      return (it.rinfo()->pc() ==
          addr - Assembler::kPatchReturnSequenceAddressOffset);
    }
    it.next();
  }
  return false;
}


void Debug::FramesHaveBeenDropped(StackFrame::Id new_break_frame_id,
                                  FrameDropMode mode,
                                  Object** restarter_frame_function_pointer) {
  if (mode != CURRENTLY_SET_MODE) {
    thread_local_.frame_drop_mode_ = mode;
  }
  thread_local_.break_frame_id_ = new_break_frame_id;
  thread_local_.restarter_frame_function_pointer_ =
      restarter_frame_function_pointer;
}


const int Debug::FramePaddingLayout::kInitialSize = 1;


// Any even value bigger than kInitialSize as needed for stack scanning.
const int Debug::FramePaddingLayout::kPaddingValue = kInitialSize + 1;


bool Debug::IsDebugGlobal(GlobalObject* global) {
  return IsLoaded() && global == debug_context()->global_object();
}


void Debug::ClearMirrorCache() {
  PostponeInterruptsScope postpone(isolate_);
  HandleScope scope(isolate_);
  ASSERT(isolate_->context() == *Debug::debug_context());

  // Clear the mirror cache.
  Handle<String> function_name = isolate_->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("ClearMirrorCache"));
  Handle<Object> fun(
      isolate_->global_object()->GetPropertyNoExceptionThrown(*function_name),
      isolate_);
  ASSERT(fun->IsJSFunction());
  bool caught_exception;
  Execution::TryCall(Handle<JSFunction>::cast(fun),
      Handle<JSObject>(Debug::debug_context()->global_object()),
      0, NULL, &caught_exception);
}


void Debug::CreateScriptCache() {
  Heap* heap = isolate_->heap();
  HandleScope scope(isolate_);

  // Perform two GCs to get rid of all unreferenced scripts. The first GC gets
  // rid of all the cached script wrappers and the second gets rid of the
  // scripts which are no longer referenced.  The second also sweeps precisely,
  // which saves us doing yet another GC to make the heap iterable.
  heap->CollectAllGarbage(Heap::kNoGCFlags, "Debug::CreateScriptCache");
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                          "Debug::CreateScriptCache");

  ASSERT(script_cache_ == NULL);
  script_cache_ = new ScriptCache();

  // Scan heap for Script objects.
  int count = 0;
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_allocation;

  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (obj->IsScript() && Script::cast(obj)->HasValidSource()) {
      script_cache_->Add(Handle<Script>(Script::cast(obj)));
      count++;
    }
  }
}


void Debug::DestroyScriptCache() {
  // Get rid of the script cache if it was created.
  if (script_cache_ != NULL) {
    delete script_cache_;
    script_cache_ = NULL;
  }
}


void Debug::AddScriptToScriptCache(Handle<Script> script) {
  if (script_cache_ != NULL) {
    script_cache_->Add(script);
  }
}


Handle<FixedArray> Debug::GetLoadedScripts() {
  // Create and fill the script cache when the loaded scripts is requested for
  // the first time.
  if (script_cache_ == NULL) {
    CreateScriptCache();
  }

  // If the script cache is not active just return an empty array.
  ASSERT(script_cache_ != NULL);
  if (script_cache_ == NULL) {
    isolate_->factory()->NewFixedArray(0);
  }

  // Perform GC to get unreferenced scripts evicted from the cache before
  // returning the content.
  isolate_->heap()->CollectAllGarbage(Heap::kNoGCFlags,
                                      "Debug::GetLoadedScripts");

  // Get the scripts from the cache.
  return script_cache_->GetScripts();
}


void Debug::AfterGarbageCollection() {
  // Generate events for collected scripts.
  if (script_cache_ != NULL) {
    script_cache_->ProcessCollectedScripts();
  }
}


Debugger::Debugger(Isolate* isolate)
    : debugger_access_(isolate->debugger_access()),
      event_listener_(Handle<Object>()),
      event_listener_data_(Handle<Object>()),
      compiling_natives_(false),
      is_loading_debugger_(false),
      live_edit_enabled_(true),
      never_unload_debugger_(false),
      force_debugger_active_(false),
      message_handler_(NULL),
      debugger_unload_pending_(false),
      host_dispatch_handler_(NULL),
      dispatch_handler_access_(OS::CreateMutex()),
      debug_message_dispatch_handler_(NULL),
      message_dispatch_helper_thread_(NULL),
      host_dispatch_micros_(100 * 1000),
      agent_(NULL),
      command_queue_(isolate->logger(), kQueueInitialSize),
      command_received_(OS::CreateSemaphore(0)),
      event_command_queue_(isolate->logger(), kQueueInitialSize),
      isolate_(isolate) {
}


Debugger::~Debugger() {
  delete dispatch_handler_access_;
  dispatch_handler_access_ = 0;
  delete command_received_;
  command_received_ = 0;
}


Handle<Object> Debugger::MakeJSObject(Vector<const char> constructor_name,
                                      int argc,
                                      Handle<Object> argv[],
                                      bool* caught_exception) {
  ASSERT(isolate_->context() == *isolate_->debug()->debug_context());

  // Create the execution state object.
  Handle<String> constructor_str =
      isolate_->factory()->InternalizeUtf8String(constructor_name);
  Handle<Object> constructor(
      isolate_->global_object()->GetPropertyNoExceptionThrown(*constructor_str),
      isolate_);
  ASSERT(constructor->IsJSFunction());
  if (!constructor->IsJSFunction()) {
    *caught_exception = true;
    return isolate_->factory()->undefined_value();
  }
  Handle<Object> js_object = Execution::TryCall(
      Handle<JSFunction>::cast(constructor),
      Handle<JSObject>(isolate_->debug()->debug_context()->global_object()),
      argc,
      argv,
      caught_exception);
  return js_object;
}


Handle<Object> Debugger::MakeExecutionState(bool* caught_exception) {
  // Create the execution state object.
  Handle<Object> break_id = isolate_->factory()->NewNumberFromInt(
      isolate_->debug()->break_id());
  Handle<Object> argv[] = { break_id };
  return MakeJSObject(CStrVector("MakeExecutionState"),
                      ARRAY_SIZE(argv),
                      argv,
                      caught_exception);
}


Handle<Object> Debugger::MakeBreakEvent(Handle<Object> exec_state,
                                        Handle<Object> break_points_hit,
                                        bool* caught_exception) {
  // Create the new break event object.
  Handle<Object> argv[] = { exec_state, break_points_hit };
  return MakeJSObject(CStrVector("MakeBreakEvent"),
                      ARRAY_SIZE(argv),
                      argv,
                      caught_exception);
}


Handle<Object> Debugger::MakeExceptionEvent(Handle<Object> exec_state,
                                            Handle<Object> exception,
                                            bool uncaught,
                                            bool* caught_exception) {
  Factory* factory = isolate_->factory();
  // Create the new exception event object.
  Handle<Object> argv[] = { exec_state,
                            exception,
                            factory->ToBoolean(uncaught) };
  return MakeJSObject(CStrVector("MakeExceptionEvent"),
                      ARRAY_SIZE(argv),
                      argv,
                      caught_exception);
}


Handle<Object> Debugger::MakeNewFunctionEvent(Handle<Object> function,
                                              bool* caught_exception) {
  // Create the new function event object.
  Handle<Object> argv[] = { function };
  return MakeJSObject(CStrVector("MakeNewFunctionEvent"),
                      ARRAY_SIZE(argv),
                      argv,
                      caught_exception);
}


Handle<Object> Debugger::MakeCompileEvent(Handle<Script> script,
                                          bool before,
                                          bool* caught_exception) {
  Factory* factory = isolate_->factory();
  // Create the compile event object.
  Handle<Object> exec_state = MakeExecutionState(caught_exception);
  Handle<Object> script_wrapper = GetScriptWrapper(script);
  Handle<Object> argv[] = { exec_state,
                            script_wrapper,
                            factory->ToBoolean(before) };
  return MakeJSObject(CStrVector("MakeCompileEvent"),
                      ARRAY_SIZE(argv),
                      argv,
                      caught_exception);
}


Handle<Object> Debugger::MakeScriptCollectedEvent(int id,
                                                  bool* caught_exception) {
  // Create the script collected event object.
  Handle<Object> exec_state = MakeExecutionState(caught_exception);
  Handle<Object> id_object = Handle<Smi>(Smi::FromInt(id), isolate_);
  Handle<Object> argv[] = { exec_state, id_object };

  return MakeJSObject(CStrVector("MakeScriptCollectedEvent"),
                      ARRAY_SIZE(argv),
                      argv,
                      caught_exception);
}


void Debugger::OnException(Handle<Object> exception, bool uncaught) {
  HandleScope scope(isolate_);
  Debug* debug = isolate_->debug();

  // Bail out based on state or if there is no listener for this event
  if (debug->InDebugger()) return;
  if (!Debugger::EventActive(v8::Exception)) return;

  // Bail out if exception breaks are not active
  if (uncaught) {
    // Uncaught exceptions are reported by either flags.
    if (!(debug->break_on_uncaught_exception() ||
          debug->break_on_exception())) return;
  } else {
    // Caught exceptions are reported is activated.
    if (!debug->break_on_exception()) return;
  }

  // Enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) return;

  // Clear all current stepping setup.
  debug->ClearStepping();
  // Create the event data object.
  bool caught_exception = false;
  Handle<Object> exec_state = MakeExecutionState(&caught_exception);
  Handle<Object> event_data;
  if (!caught_exception) {
    event_data = MakeExceptionEvent(exec_state, exception, uncaught,
                                    &caught_exception);
  }
  // Bail out and don't call debugger if exception.
  if (caught_exception) {
    return;
  }

  // Process debug event.
  ProcessDebugEvent(v8::Exception, Handle<JSObject>::cast(event_data), false);
  // Return to continue execution from where the exception was thrown.
}


void Debugger::OnDebugBreak(Handle<Object> break_points_hit,
                            bool auto_continue) {
  HandleScope scope(isolate_);

  // Debugger has already been entered by caller.
  ASSERT(isolate_->context() == *isolate_->debug()->debug_context());

  // Bail out if there is no listener for this event
  if (!Debugger::EventActive(v8::Break)) return;

  // Debugger must be entered in advance.
  ASSERT(isolate_->context() == *isolate_->debug()->debug_context());

  // Create the event data object.
  bool caught_exception = false;
  Handle<Object> exec_state = MakeExecutionState(&caught_exception);
  Handle<Object> event_data;
  if (!caught_exception) {
    event_data = MakeBreakEvent(exec_state, break_points_hit,
                                &caught_exception);
  }
  // Bail out and don't call debugger if exception.
  if (caught_exception) {
    return;
  }

  // Process debug event.
  ProcessDebugEvent(v8::Break,
                    Handle<JSObject>::cast(event_data),
                    auto_continue);
}


void Debugger::OnBeforeCompile(Handle<Script> script) {
  HandleScope scope(isolate_);

  // Bail out based on state or if there is no listener for this event
  if (isolate_->debug()->InDebugger()) return;
  if (compiling_natives()) return;
  if (!EventActive(v8::BeforeCompile)) return;

  // Enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) return;

  // Create the event data object.
  bool caught_exception = false;
  Handle<Object> event_data = MakeCompileEvent(script, true, &caught_exception);
  // Bail out and don't call debugger if exception.
  if (caught_exception) {
    return;
  }

  // Process debug event.
  ProcessDebugEvent(v8::BeforeCompile,
                    Handle<JSObject>::cast(event_data),
                    true);
}


// Handle debugger actions when a new script is compiled.
void Debugger::OnAfterCompile(Handle<Script> script,
                              AfterCompileFlags after_compile_flags) {
  HandleScope scope(isolate_);
  Debug* debug = isolate_->debug();

  // Add the newly compiled script to the script cache.
  debug->AddScriptToScriptCache(script);

  // No more to do if not debugging.
  if (!IsDebuggerActive()) return;

  // No compile events while compiling natives.
  if (compiling_natives()) return;

  // Store whether in debugger before entering debugger.
  bool in_debugger = debug->InDebugger();

  // Enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) return;

  // If debugging there might be script break points registered for this
  // script. Make sure that these break points are set.

  // Get the function UpdateScriptBreakPoints (defined in debug-debugger.js).
  Handle<String> update_script_break_points_string =
      isolate_->factory()->InternalizeOneByteString(
          STATIC_ASCII_VECTOR("UpdateScriptBreakPoints"));
  Handle<Object> update_script_break_points =
      Handle<Object>(
          debug->debug_context()->global_object()->GetPropertyNoExceptionThrown(
              *update_script_break_points_string),
          isolate_);
  if (!update_script_break_points->IsJSFunction()) {
    return;
  }
  ASSERT(update_script_break_points->IsJSFunction());

  // Wrap the script object in a proper JS object before passing it
  // to JavaScript.
  Handle<JSValue> wrapper = GetScriptWrapper(script);

  // Call UpdateScriptBreakPoints expect no exceptions.
  bool caught_exception;
  Handle<Object> argv[] = { wrapper };
  Execution::TryCall(Handle<JSFunction>::cast(update_script_break_points),
                     Isolate::Current()->js_builtins_object(),
                     ARRAY_SIZE(argv),
                     argv,
                     &caught_exception);
  if (caught_exception) {
    return;
  }
  // Bail out based on state or if there is no listener for this event
  if (in_debugger && (after_compile_flags & SEND_WHEN_DEBUGGING) == 0) return;
  if (!Debugger::EventActive(v8::AfterCompile)) return;

  // Create the compile state object.
  Handle<Object> event_data = MakeCompileEvent(script,
                                               false,
                                               &caught_exception);
  // Bail out and don't call debugger if exception.
  if (caught_exception) {
    return;
  }
  // Process debug event.
  ProcessDebugEvent(v8::AfterCompile,
                    Handle<JSObject>::cast(event_data),
                    true);
}


void Debugger::OnScriptCollected(int id) {
  HandleScope scope(isolate_);

  // No more to do if not debugging.
  if (isolate_->debug()->InDebugger()) return;
  if (!IsDebuggerActive()) return;
  if (!Debugger::EventActive(v8::ScriptCollected)) return;

  // Enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) return;

  // Create the script collected state object.
  bool caught_exception = false;
  Handle<Object> event_data = MakeScriptCollectedEvent(id,
                                                       &caught_exception);
  // Bail out and don't call debugger if exception.
  if (caught_exception) {
    return;
  }

  // Process debug event.
  ProcessDebugEvent(v8::ScriptCollected,
                    Handle<JSObject>::cast(event_data),
                    true);
}


void Debugger::ProcessDebugEvent(v8::DebugEvent event,
                                 Handle<JSObject> event_data,
                                 bool auto_continue) {
  HandleScope scope(isolate_);

  // Clear any pending debug break if this is a real break.
  if (!auto_continue) {
    isolate_->debug()->clear_interrupt_pending(DEBUGBREAK);
  }

  // Create the execution state.
  bool caught_exception = false;
  Handle<Object> exec_state = MakeExecutionState(&caught_exception);
  if (caught_exception) {
    return;
  }
  // First notify the message handler if any.
  if (message_handler_ != NULL) {
    NotifyMessageHandler(event,
                         Handle<JSObject>::cast(exec_state),
                         event_data,
                         auto_continue);
  }
  // Notify registered debug event listener. This can be either a C or
  // a JavaScript function. Don't call event listener for v8::Break
  // here, if it's only a debug command -- they will be processed later.
  if ((event != v8::Break || !auto_continue) && !event_listener_.is_null()) {
    CallEventCallback(event, exec_state, event_data, NULL);
  }
  // Process pending debug commands.
  if (event == v8::Break) {
    while (!event_command_queue_.IsEmpty()) {
      CommandMessage command = event_command_queue_.Get();
      if (!event_listener_.is_null()) {
        CallEventCallback(v8::BreakForCommand,
                          exec_state,
                          event_data,
                          command.client_data());
      }
      command.Dispose();
    }
  }
}


void Debugger::CallEventCallback(v8::DebugEvent event,
                                 Handle<Object> exec_state,
                                 Handle<Object> event_data,
                                 v8::Debug::ClientData* client_data) {
  if (event_listener_->IsForeign()) {
    CallCEventCallback(event, exec_state, event_data, client_data);
  } else {
    CallJSEventCallback(event, exec_state, event_data);
  }
}


void Debugger::CallCEventCallback(v8::DebugEvent event,
                                  Handle<Object> exec_state,
                                  Handle<Object> event_data,
                                  v8::Debug::ClientData* client_data) {
  Handle<Foreign> callback_obj(Handle<Foreign>::cast(event_listener_));
  v8::Debug::EventCallback2 callback =
      FUNCTION_CAST<v8::Debug::EventCallback2>(
          callback_obj->foreign_address());
  EventDetailsImpl event_details(
      event,
      Handle<JSObject>::cast(exec_state),
      Handle<JSObject>::cast(event_data),
      event_listener_data_,
      client_data);
  callback(event_details);
}


void Debugger::CallJSEventCallback(v8::DebugEvent event,
                                   Handle<Object> exec_state,
                                   Handle<Object> event_data) {
  ASSERT(event_listener_->IsJSFunction());
  Handle<JSFunction> fun(Handle<JSFunction>::cast(event_listener_));

  // Invoke the JavaScript debug event listener.
  Handle<Object> argv[] = { Handle<Object>(Smi::FromInt(event), isolate_),
                            exec_state,
                            event_data,
                            event_listener_data_ };
  bool caught_exception;
  Execution::TryCall(fun,
                     isolate_->global_object(),
                     ARRAY_SIZE(argv),
                     argv,
                     &caught_exception);
  // Silently ignore exceptions from debug event listeners.
}


Handle<Context> Debugger::GetDebugContext() {
  never_unload_debugger_ = true;
  EnterDebugger debugger;
  return isolate_->debug()->debug_context();
}


void Debugger::UnloadDebugger() {
  Debug* debug = isolate_->debug();

  // Make sure that there are no breakpoints left.
  debug->ClearAllBreakPoints();

  // Unload the debugger if feasible.
  if (!never_unload_debugger_) {
    debug->Unload();
  }

  // Clear the flag indicating that the debugger should be unloaded.
  debugger_unload_pending_ = false;
}


void Debugger::NotifyMessageHandler(v8::DebugEvent event,
                                    Handle<JSObject> exec_state,
                                    Handle<JSObject> event_data,
                                    bool auto_continue) {
  HandleScope scope(isolate_);

  if (!isolate_->debug()->Load()) return;

  // Process the individual events.
  bool sendEventMessage = false;
  switch (event) {
    case v8::Break:
    case v8::BreakForCommand:
      sendEventMessage = !auto_continue;
      break;
    case v8::Exception:
      sendEventMessage = true;
      break;
    case v8::BeforeCompile:
      break;
    case v8::AfterCompile:
      sendEventMessage = true;
      break;
    case v8::ScriptCollected:
      sendEventMessage = true;
      break;
    case v8::NewFunction:
      break;
    default:
      UNREACHABLE();
  }

  // The debug command interrupt flag might have been set when the command was
  // added. It should be enough to clear the flag only once while we are in the
  // debugger.
  ASSERT(isolate_->debug()->InDebugger());
  isolate_->stack_guard()->Continue(DEBUGCOMMAND);

  // Notify the debugger that a debug event has occurred unless auto continue is
  // active in which case no event is send.
  if (sendEventMessage) {
    MessageImpl message = MessageImpl::NewEvent(
        event,
        auto_continue,
        Handle<JSObject>::cast(exec_state),
        Handle<JSObject>::cast(event_data));
    InvokeMessageHandler(message);
  }

  // If auto continue don't make the event cause a break, but process messages
  // in the queue if any. For script collected events don't even process
  // messages in the queue as the execution state might not be what is expected
  // by the client.
  if ((auto_continue && !HasCommands()) || event == v8::ScriptCollected) {
    return;
  }

  v8::TryCatch try_catch;

  // DebugCommandProcessor goes here.
  v8::Local<v8::Object> cmd_processor;
  {
    v8::Local<v8::Object> api_exec_state =
        v8::Utils::ToLocal(Handle<JSObject>::cast(exec_state));
    v8::Local<v8::String> fun_name =
        v8::String::New("debugCommandProcessor");
    v8::Local<v8::Function> fun =
        v8::Function::Cast(*api_exec_state->Get(fun_name));

    v8::Handle<v8::Boolean> running =
        auto_continue ? v8::True() : v8::False();
    static const int kArgc = 1;
    v8::Handle<Value> argv[kArgc] = { running };
    cmd_processor = v8::Object::Cast(*fun->Call(api_exec_state, kArgc, argv));
    if (try_catch.HasCaught()) {
      PrintLn(try_catch.Exception());
      return;
    }
  }

  bool running = auto_continue;

  // Process requests from the debugger.
  while (true) {
    // Wait for new command in the queue.
    if (Debugger::host_dispatch_handler_) {
      // In case there is a host dispatch - do periodic dispatches.
      if (!command_received_->Wait(host_dispatch_micros_)) {
        // Timout expired, do the dispatch.
        Debugger::host_dispatch_handler_();
        continue;
      }
    } else {
      // In case there is no host dispatch - just wait.
      command_received_->Wait();
    }

    // Get the command from the queue.
    CommandMessage command = command_queue_.Get();
    isolate_->logger()->DebugTag(
        "Got request from command queue, in interactive loop.");
    if (!Debugger::IsDebuggerActive()) {
      // Delete command text and user data.
      command.Dispose();
      return;
    }

    // Invoke JavaScript to process the debug request.
    v8::Local<v8::String> fun_name;
    v8::Local<v8::Function> fun;
    v8::Local<v8::Value> request;
    v8::TryCatch try_catch;
    fun_name = v8::String::New("processDebugRequest");
    fun = v8::Function::Cast(*cmd_processor->Get(fun_name));

    request = v8::String::New(command.text().start(),
                              command.text().length());
    static const int kArgc = 1;
    v8::Handle<Value> argv[kArgc] = { request };
    v8::Local<v8::Value> response_val = fun->Call(cmd_processor, kArgc, argv);

    // Get the response.
    v8::Local<v8::String> response;
    if (!try_catch.HasCaught()) {
      // Get response string.
      if (!response_val->IsUndefined()) {
        response = v8::String::Cast(*response_val);
      } else {
        response = v8::String::New("");
      }

      // Log the JSON request/response.
      if (FLAG_trace_debug_json) {
        PrintLn(request);
        PrintLn(response);
      }

      // Get the running state.
      fun_name = v8::String::New("isRunning");
      fun = v8::Function::Cast(*cmd_processor->Get(fun_name));
      static const int kArgc = 1;
      v8::Handle<Value> argv[kArgc] = { response };
      v8::Local<v8::Value> running_val = fun->Call(cmd_processor, kArgc, argv);
      if (!try_catch.HasCaught()) {
        running = running_val->ToBoolean()->Value();
      }
    } else {
      // In case of failure the result text is the exception text.
      response = try_catch.Exception()->ToString();
    }

    // Return the result.
    MessageImpl message = MessageImpl::NewResponse(
        event,
        running,
        Handle<JSObject>::cast(exec_state),
        Handle<JSObject>::cast(event_data),
        Handle<String>(Utils::OpenHandle(*response)),
        command.client_data());
    InvokeMessageHandler(message);
    command.Dispose();

    // Return from debug event processing if either the VM is put into the
    // running state (through a continue command) or auto continue is active
    // and there are no more commands queued.
    if (running && !HasCommands()) {
      return;
    }
  }
}


void Debugger::SetEventListener(Handle<Object> callback,
                                Handle<Object> data) {
  HandleScope scope(isolate_);
  GlobalHandles* global_handles = isolate_->global_handles();

  // Clear the global handles for the event listener and the event listener data
  // object.
  if (!event_listener_.is_null()) {
    global_handles->Destroy(
        reinterpret_cast<Object**>(event_listener_.location()));
    event_listener_ = Handle<Object>();
  }
  if (!event_listener_data_.is_null()) {
    global_handles->Destroy(
        reinterpret_cast<Object**>(event_listener_data_.location()));
    event_listener_data_ = Handle<Object>();
  }

  // If there is a new debug event listener register it together with its data
  // object.
  if (!callback->IsUndefined() && !callback->IsNull()) {
    event_listener_ = Handle<Object>::cast(
        global_handles->Create(*callback));
    if (data.is_null()) {
      data = isolate_->factory()->undefined_value();
    }
    event_listener_data_ = Handle<Object>::cast(
        global_handles->Create(*data));
  }

  ListenersChanged();
}


void Debugger::SetMessageHandler(v8::Debug::MessageHandler2 handler) {
  ScopedLock with(debugger_access_);

  message_handler_ = handler;
  ListenersChanged();
  if (handler == NULL) {
    // Send an empty command to the debugger if in a break to make JavaScript
    // run again if the debugger is closed.
    if (isolate_->debug()->InDebugger()) {
      ProcessCommand(Vector<const uint16_t>::empty());
    }
  }
}


void Debugger::ListenersChanged() {
  if (IsDebuggerActive()) {
    // Disable the compilation cache when the debugger is active.
    isolate_->compilation_cache()->Disable();
    debugger_unload_pending_ = false;
  } else {
    isolate_->compilation_cache()->Enable();
    // Unload the debugger if event listener and message handler cleared.
    // Schedule this for later, because we may be in non-V8 thread.
    debugger_unload_pending_ = true;
  }
}


void Debugger::SetHostDispatchHandler(v8::Debug::HostDispatchHandler handler,
                                      int period) {
  host_dispatch_handler_ = handler;
  host_dispatch_micros_ = period * 1000;
}


void Debugger::SetDebugMessageDispatchHandler(
    v8::Debug::DebugMessageDispatchHandler handler, bool provide_locker) {
  ScopedLock with(dispatch_handler_access_);
  debug_message_dispatch_handler_ = handler;

  if (provide_locker && message_dispatch_helper_thread_ == NULL) {
    message_dispatch_helper_thread_ = new MessageDispatchHelperThread(isolate_);
    message_dispatch_helper_thread_->Start();
  }
}


// Calls the registered debug message handler. This callback is part of the
// public API.
void Debugger::InvokeMessageHandler(MessageImpl message) {
  ScopedLock with(debugger_access_);

  if (message_handler_ != NULL) {
    message_handler_(message);
  }
}


// Puts a command coming from the public API on the queue.  Creates
// a copy of the command string managed by the debugger.  Up to this
// point, the command data was managed by the API client.  Called
// by the API client thread.
void Debugger::ProcessCommand(Vector<const uint16_t> command,
                              v8::Debug::ClientData* client_data) {
  // Need to cast away const.
  CommandMessage message = CommandMessage::New(
      Vector<uint16_t>(const_cast<uint16_t*>(command.start()),
                       command.length()),
      client_data);
  isolate_->logger()->DebugTag("Put command on command_queue.");
  command_queue_.Put(message);
  command_received_->Signal();

  // Set the debug command break flag to have the command processed.
  if (!isolate_->debug()->InDebugger()) {
    isolate_->stack_guard()->DebugCommand();
  }

  MessageDispatchHelperThread* dispatch_thread;
  {
    ScopedLock with(dispatch_handler_access_);
    dispatch_thread = message_dispatch_helper_thread_;
  }

  if (dispatch_thread == NULL) {
    CallMessageDispatchHandler();
  } else {
    dispatch_thread->Schedule();
  }
}


bool Debugger::HasCommands() {
  return !command_queue_.IsEmpty();
}


void Debugger::EnqueueDebugCommand(v8::Debug::ClientData* client_data) {
  CommandMessage message = CommandMessage::New(Vector<uint16_t>(), client_data);
  event_command_queue_.Put(message);

  // Set the debug command break flag to have the command processed.
  if (!isolate_->debug()->InDebugger()) {
    isolate_->stack_guard()->DebugCommand();
  }
}


bool Debugger::IsDebuggerActive() {
  ScopedLock with(debugger_access_);

  return message_handler_ != NULL ||
      !event_listener_.is_null() ||
      force_debugger_active_;
}


Handle<Object> Debugger::Call(Handle<JSFunction> fun,
                              Handle<Object> data,
                              bool* pending_exception) {
  // When calling functions in the debugger prevent it from beeing unloaded.
  Debugger::never_unload_debugger_ = true;

  // Enter the debugger.
  EnterDebugger debugger;
  if (debugger.FailedToEnter()) {
    return isolate_->factory()->undefined_value();
  }

  // Create the execution state.
  bool caught_exception = false;
  Handle<Object> exec_state = MakeExecutionState(&caught_exception);
  if (caught_exception) {
    return isolate_->factory()->undefined_value();
  }

  Handle<Object> argv[] = { exec_state, data };
  Handle<Object> result = Execution::Call(
      fun,
      Handle<Object>(isolate_->debug()->debug_context_->global_proxy(),
                     isolate_),
      ARRAY_SIZE(argv),
      argv,
      pending_exception);
  return result;
}


static void StubMessageHandler2(const v8::Debug::Message& message) {
  // Simply ignore message.
}


bool Debugger::StartAgent(const char* name, int port,
                          bool wait_for_connection) {
  ASSERT(Isolate::Current() == isolate_);
  if (wait_for_connection) {
    // Suspend V8 if it is already running or set V8 to suspend whenever
    // it starts.
    // Provide stub message handler; V8 auto-continues each suspend
    // when there is no message handler; we doesn't need it.
    // Once become suspended, V8 will stay so indefinitely long, until remote
    // debugger connects and issues "continue" command.
    Debugger::message_handler_ = StubMessageHandler2;
    v8::Debug::DebugBreak();
  }

  if (Socket::SetUp()) {
    if (agent_ == NULL) {
      agent_ = new DebuggerAgent(name, port);
      agent_->Start();
    }
    return true;
  }

  return false;
}


void Debugger::StopAgent() {
  ASSERT(Isolate::Current() == isolate_);
  if (agent_ != NULL) {
    agent_->Shutdown();
    agent_->Join();
    delete agent_;
    agent_ = NULL;
  }
}


void Debugger::WaitForAgent() {
  ASSERT(Isolate::Current() == isolate_);
  if (agent_ != NULL)
    agent_->WaitUntilListening();
}


void Debugger::CallMessageDispatchHandler() {
  v8::Debug::DebugMessageDispatchHandler handler;
  {
    ScopedLock with(dispatch_handler_access_);
    handler = Debugger::debug_message_dispatch_handler_;
  }
  if (handler != NULL) {
    handler();
  }
}


EnterDebugger::EnterDebugger()
    : isolate_(Isolate::Current()),
      prev_(isolate_->debug()->debugger_entry()),
      it_(isolate_),
      has_js_frames_(!it_.done()),
      save_(isolate_) {
  Debug* debug = isolate_->debug();
  ASSERT(prev_ != NULL || !debug->is_interrupt_pending(PREEMPT));
  ASSERT(prev_ != NULL || !debug->is_interrupt_pending(DEBUGBREAK));

  // Link recursive debugger entry.
  debug->set_debugger_entry(this);

  // Store the previous break id and frame id.
  break_id_ = debug->break_id();
  break_frame_id_ = debug->break_frame_id();

  // Create the new break info. If there is no JavaScript frames there is no
  // break frame id.
  if (has_js_frames_) {
    debug->NewBreak(it_.frame()->id());
  } else {
    debug->NewBreak(StackFrame::NO_ID);
  }

  // Make sure that debugger is loaded and enter the debugger context.
  load_failed_ = !debug->Load();
  if (!load_failed_) {
    // NOTE the member variable save which saves the previous context before
    // this change.
    isolate_->set_context(*debug->debug_context());
  }
}


EnterDebugger::~EnterDebugger() {
  ASSERT(Isolate::Current() == isolate_);
  Debug* debug = isolate_->debug();

  // Restore to the previous break state.
  debug->SetBreak(break_frame_id_, break_id_);

  // Check for leaving the debugger.
  if (!load_failed_ && prev_ == NULL) {
    // Clear mirror cache when leaving the debugger. Skip this if there is a
    // pending exception as clearing the mirror cache calls back into
    // JavaScript. This can happen if the v8::Debug::Call is used in which
    // case the exception should end up in the calling code.
    if (!isolate_->has_pending_exception()) {
      // Try to avoid any pending debug break breaking in the clear mirror
      // cache JavaScript code.
      if (isolate_->stack_guard()->IsDebugBreak()) {
        debug->set_interrupts_pending(DEBUGBREAK);
        isolate_->stack_guard()->Continue(DEBUGBREAK);
      }
      debug->ClearMirrorCache();
    }

    // Request preemption and debug break when leaving the last debugger entry
    // if any of these where recorded while debugging.
    if (debug->is_interrupt_pending(PREEMPT)) {
      // This re-scheduling of preemption is to avoid starvation in some
      // debugging scenarios.
      debug->clear_interrupt_pending(PREEMPT);
      isolate_->stack_guard()->Preempt();
    }
    if (debug->is_interrupt_pending(DEBUGBREAK)) {
      debug->clear_interrupt_pending(DEBUGBREAK);
      isolate_->stack_guard()->DebugBreak();
    }

    // If there are commands in the queue when leaving the debugger request
    // that these commands are processed.
    if (isolate_->debugger()->HasCommands()) {
      isolate_->stack_guard()->DebugCommand();
    }

    // If leaving the debugger with the debugger no longer active unload it.
    if (!isolate_->debugger()->IsDebuggerActive()) {
      isolate_->debugger()->UnloadDebugger();
    }
  }

  // Leaving this debugger entry.
  debug->set_debugger_entry(prev_);
}


MessageImpl MessageImpl::NewEvent(DebugEvent event,
                                  bool running,
                                  Handle<JSObject> exec_state,
                                  Handle<JSObject> event_data) {
  MessageImpl message(true, event, running,
                      exec_state, event_data, Handle<String>(), NULL);
  return message;
}


MessageImpl MessageImpl::NewResponse(DebugEvent event,
                                     bool running,
                                     Handle<JSObject> exec_state,
                                     Handle<JSObject> event_data,
                                     Handle<String> response_json,
                                     v8::Debug::ClientData* client_data) {
  MessageImpl message(false, event, running,
                      exec_state, event_data, response_json, client_data);
  return message;
}


MessageImpl::MessageImpl(bool is_event,
                         DebugEvent event,
                         bool running,
                         Handle<JSObject> exec_state,
                         Handle<JSObject> event_data,
                         Handle<String> response_json,
                         v8::Debug::ClientData* client_data)
    : is_event_(is_event),
      event_(event),
      running_(running),
      exec_state_(exec_state),
      event_data_(event_data),
      response_json_(response_json),
      client_data_(client_data) {}


bool MessageImpl::IsEvent() const {
  return is_event_;
}


bool MessageImpl::IsResponse() const {
  return !is_event_;
}


DebugEvent MessageImpl::GetEvent() const {
  return event_;
}


bool MessageImpl::WillStartRunning() const {
  return running_;
}


v8::Handle<v8::Object> MessageImpl::GetExecutionState() const {
  return v8::Utils::ToLocal(exec_state_);
}


v8::Handle<v8::Object> MessageImpl::GetEventData() const {
  return v8::Utils::ToLocal(event_data_);
}


v8::Handle<v8::String> MessageImpl::GetJSON() const {
  v8::HandleScope scope(
      reinterpret_cast<v8::Isolate*>(event_data_->GetIsolate()));

  if (IsEvent()) {
    // Call toJSONProtocol on the debug event object.
    Handle<Object> fun = GetProperty(event_data_, "toJSONProtocol");
    if (!fun->IsJSFunction()) {
      return v8::Handle<v8::String>();
    }
    bool caught_exception;
    Handle<Object> json = Execution::TryCall(Handle<JSFunction>::cast(fun),
                                             event_data_,
                                             0, NULL, &caught_exception);
    if (caught_exception || !json->IsString()) {
      return v8::Handle<v8::String>();
    }
    return scope.Close(v8::Utils::ToLocal(Handle<String>::cast(json)));
  } else {
    return v8::Utils::ToLocal(response_json_);
  }
}


v8::Handle<v8::Context> MessageImpl::GetEventContext() const {
  Isolate* isolate = Isolate::Current();
  v8::Handle<v8::Context> context = GetDebugEventContext(isolate);
  // Isolate::context() may be NULL when "script collected" event occures.
  ASSERT(!context.IsEmpty() || event_ == v8::ScriptCollected);
  return context;
}


v8::Debug::ClientData* MessageImpl::GetClientData() const {
  return client_data_;
}


EventDetailsImpl::EventDetailsImpl(DebugEvent event,
                                   Handle<JSObject> exec_state,
                                   Handle<JSObject> event_data,
                                   Handle<Object> callback_data,
                                   v8::Debug::ClientData* client_data)
    : event_(event),
      exec_state_(exec_state),
      event_data_(event_data),
      callback_data_(callback_data),
      client_data_(client_data) {}


DebugEvent EventDetailsImpl::GetEvent() const {
  return event_;
}


v8::Handle<v8::Object> EventDetailsImpl::GetExecutionState() const {
  return v8::Utils::ToLocal(exec_state_);
}


v8::Handle<v8::Object> EventDetailsImpl::GetEventData() const {
  return v8::Utils::ToLocal(event_data_);
}


v8::Handle<v8::Context> EventDetailsImpl::GetEventContext() const {
  return GetDebugEventContext(Isolate::Current());
}


v8::Handle<v8::Value> EventDetailsImpl::GetCallbackData() const {
  return v8::Utils::ToLocal(callback_data_);
}


v8::Debug::ClientData* EventDetailsImpl::GetClientData() const {
  return client_data_;
}


CommandMessage::CommandMessage() : text_(Vector<uint16_t>::empty()),
                                   client_data_(NULL) {
}


CommandMessage::CommandMessage(const Vector<uint16_t>& text,
                               v8::Debug::ClientData* data)
    : text_(text),
      client_data_(data) {
}


CommandMessage::~CommandMessage() {
}


void CommandMessage::Dispose() {
  text_.Dispose();
  delete client_data_;
  client_data_ = NULL;
}


CommandMessage CommandMessage::New(const Vector<uint16_t>& command,
                                   v8::Debug::ClientData* data) {
  return CommandMessage(command.Clone(), data);
}


CommandMessageQueue::CommandMessageQueue(int size) : start_(0), end_(0),
                                                     size_(size) {
  messages_ = NewArray<CommandMessage>(size);
}


CommandMessageQueue::~CommandMessageQueue() {
  while (!IsEmpty()) {
    CommandMessage m = Get();
    m.Dispose();
  }
  DeleteArray(messages_);
}


CommandMessage CommandMessageQueue::Get() {
  ASSERT(!IsEmpty());
  int result = start_;
  start_ = (start_ + 1) % size_;
  return messages_[result];
}


void CommandMessageQueue::Put(const CommandMessage& message) {
  if ((end_ + 1) % size_ == start_) {
    Expand();
  }
  messages_[end_] = message;
  end_ = (end_ + 1) % size_;
}


void CommandMessageQueue::Expand() {
  CommandMessageQueue new_queue(size_ * 2);
  while (!IsEmpty()) {
    new_queue.Put(Get());
  }
  CommandMessage* array_to_free = messages_;
  *this = new_queue;
  new_queue.messages_ = array_to_free;
  // Make the new_queue empty so that it doesn't call Dispose on any messages.
  new_queue.start_ = new_queue.end_;
  // Automatic destructor called on new_queue, freeing array_to_free.
}


LockingCommandMessageQueue::LockingCommandMessageQueue(Logger* logger, int size)
    : logger_(logger), queue_(size) {
  lock_ = OS::CreateMutex();
}


LockingCommandMessageQueue::~LockingCommandMessageQueue() {
  delete lock_;
}


bool LockingCommandMessageQueue::IsEmpty() const {
  ScopedLock sl(lock_);
  return queue_.IsEmpty();
}


CommandMessage LockingCommandMessageQueue::Get() {
  ScopedLock sl(lock_);
  CommandMessage result = queue_.Get();
  logger_->DebugEvent("Get", result.text());
  return result;
}


void LockingCommandMessageQueue::Put(const CommandMessage& message) {
  ScopedLock sl(lock_);
  queue_.Put(message);
  logger_->DebugEvent("Put", message.text());
}


void LockingCommandMessageQueue::Clear() {
  ScopedLock sl(lock_);
  queue_.Clear();
}


MessageDispatchHelperThread::MessageDispatchHelperThread(Isolate* isolate)
    : Thread("v8:MsgDispHelpr"),
      isolate_(isolate), sem_(OS::CreateSemaphore(0)),
      mutex_(OS::CreateMutex()), already_signalled_(false) {
}


MessageDispatchHelperThread::~MessageDispatchHelperThread() {
  delete mutex_;
  delete sem_;
}


void MessageDispatchHelperThread::Schedule() {
  {
    ScopedLock lock(mutex_);
    if (already_signalled_) {
      return;
    }
    already_signalled_ = true;
  }
  sem_->Signal();
}


void MessageDispatchHelperThread::Run() {
  while (true) {
    sem_->Wait();
    {
      ScopedLock lock(mutex_);
      already_signalled_ = false;
    }
    {
      Locker locker(reinterpret_cast<v8::Isolate*>(isolate_));
      isolate_->debugger()->CallMessageDispatchHandler();
    }
  }
}

#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal
