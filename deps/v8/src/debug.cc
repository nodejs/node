// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/api.h"
#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compiler.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/full-codegen.h"
#include "src/global-handles.h"
#include "src/isolate-inl.h"
#include "src/list.h"
#include "src/log.h"
#include "src/messages.h"
#include "src/natives.h"

#include "include/v8-debug.h"

namespace v8 {
namespace internal {

Debug::Debug(Isolate* isolate)
    : debug_context_(Handle<Context>()),
      event_listener_(Handle<Object>()),
      event_listener_data_(Handle<Object>()),
      message_handler_(NULL),
      command_received_(0),
      command_queue_(isolate->logger(), kQueueInitialSize),
      event_command_queue_(isolate->logger(), kQueueInitialSize),
      is_active_(false),
      is_suppressed_(false),
      live_edit_enabled_(true),  // TODO(yangguo): set to false by default.
      has_break_points_(false),
      break_disabled_(false),
      in_debug_event_listener_(false),
      break_on_exception_(false),
      break_on_uncaught_exception_(false),
      script_cache_(NULL),
      debug_info_list_(NULL),
      isolate_(isolate) {
  ThreadInit();
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
  DCHECK(reloc_iterator_ != NULL);
  DCHECK(reloc_iterator_original_ != NULL);
  delete reloc_iterator_;
  delete reloc_iterator_original_;
}


// Check whether a code stub with the specified major key is a possible break
// point location when looking for source break locations.
static bool IsSourceBreakStub(Code* code) {
  CodeStub::Major major_key = CodeStub::GetMajorKey(code);
  return major_key == CodeStub::CallFunction;
}


// Check whether a code stub with the specified major key is a possible break
// location.
static bool IsBreakStub(Code* code) {
  CodeStub::Major major_key = CodeStub::GetMajorKey(code);
  return major_key == CodeStub::CallFunction;
}


void BreakLocationIterator::Next() {
  DisallowHeapAllocation no_gc;
  DCHECK(!RinfoDone());

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
      DCHECK(position_ >= 0);
      DCHECK(statement_position_ >= 0);
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

    if (RelocInfo::IsCodeTarget(rmode())) {
      // Check for breakable code target. Look in the original code as setting
      // break points can cause the code targets in the running (debugged) code
      // to be of a different kind than in the original code.
      Address target = original_rinfo()->target_address();
      Code* code = Code::GetCodeFromTargetAddress(target);

      if (RelocInfo::IsConstructCall(rmode()) || code->is_call_stub()) {
        break_point_++;
        return;
      }

      // Skip below if we only want locations for calls and returns.
      if (type_ == CALLS_AND_RETURNS) continue;

      if ((code->is_inline_cache_stub() && !code->is_binary_op_stub() &&
           !code->is_compare_ic_stub() && !code->is_to_boolean_ic_stub())) {
        break_point_++;
        return;
      }
      if (code->kind() == Code::STUB) {
        if (IsDebuggerStatement()) {
          break_point_++;
          return;
        } else if (type_ == ALL_BREAK_LOCATIONS) {
          if (IsBreakStub(code)) {
            break_point_++;
            return;
          }
        } else {
          DCHECK(type_ == SOURCE_BREAK_LOCATIONS);
          if (IsSourceBreakStub(code)) {
            break_point_++;
            return;
          }
        }
      }
    }

    if (IsDebugBreakSlot() && type_ != CALLS_AND_RETURNS) {
      // There is always a possible break point at a debug break slot.
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
void BreakLocationIterator::FindBreakLocationFromPosition(int position,
    BreakPositionAlignment alignment) {
  // Run through all break points to locate the one closest to the source
  // position.
  int closest_break_point = 0;
  int distance = kMaxInt;

  while (!Done()) {
    int next_position;
    switch (alignment) {
    case STATEMENT_ALIGNED:
      next_position = this->statement_position();
      break;
    case BREAK_POSITION_ALIGNED:
      next_position = this->position();
      break;
    default:
      UNREACHABLE();
      next_position = this->statement_position();
    }
    // Check if this break point is closer that what was previously found.
    if (position <= next_position && next_position - position < distance) {
      closest_break_point = break_point();
      distance = next_position - position;
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
  if (!HasBreakPoint()) SetDebugBreak();
  DCHECK(IsDebugBreak() || IsDebuggerStatement());
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
    DCHECK(!IsDebugBreak());
  }
}


void BreakLocationIterator::SetOneShot() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) return;

  // If there is a real break point here no more to do.
  if (HasBreakPoint()) {
    DCHECK(IsDebugBreak());
    return;
  }

  // Patch code with debug break.
  SetDebugBreak();
}


void BreakLocationIterator::ClearOneShot() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) return;

  // If there is a real break point here no more to do.
  if (HasBreakPoint()) {
    DCHECK(IsDebugBreak());
    return;
  }

  // Patch code removing debug break.
  ClearDebugBreak();
  DCHECK(!IsDebugBreak());
}


void BreakLocationIterator::SetDebugBreak() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) return;

  // If there is already a break point here just return. This might happen if
  // the same code is flooded with break points twice. Flooding the same
  // function twice might happen when stepping in a function with an exception
  // handler as the handler and the function is the same.
  if (IsDebugBreak()) return;

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
  DCHECK(IsDebugBreak());
}


void BreakLocationIterator::ClearDebugBreak() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) return;

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
  DCHECK(!IsDebugBreak());
}


bool BreakLocationIterator::IsStepInLocation(Isolate* isolate) {
  if (RelocInfo::IsConstructCall(original_rmode())) {
    return true;
  } else if (RelocInfo::IsCodeTarget(rmode())) {
    HandleScope scope(debug_info_->GetIsolate());
    Address target = original_rinfo()->target_address();
    Handle<Code> target_code(Code::GetCodeFromTargetAddress(target));
    if (target_code->kind() == Code::STUB) {
      return CodeStub::GetMajorKey(*target_code) == CodeStub::CallFunction;
    }
    return target_code->is_call_stub();
  }
  return false;
}


void BreakLocationIterator::PrepareStepIn(Isolate* isolate) {
#ifdef DEBUG
  HandleScope scope(isolate);
  // Step in can only be prepared if currently positioned on an IC call,
  // construct call or CallFunction stub call.
  Address target = rinfo()->target_address();
  Handle<Code> target_code(Code::GetCodeFromTargetAddress(target));
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
       CodeStub::GetMajorKey(*maybe_call_function_stub) ==
           CodeStub::CallFunction);

  // Step in through construct call requires no changes to the running code.
  // Step in through getters/setters should already be prepared as well
  // because caller of this function (Debug::PrepareStep) is expected to
  // flood the top frame's function with one shot breakpoints.
  // Step in through CallFunction stub should also be prepared by caller of
  // this function (Debug::PrepareStep) which should flood target function
  // with breakpoints.
  DCHECK(RelocInfo::IsConstructCall(rmode()) ||
         target_code->is_inline_cache_stub() ||
         is_call_function_stub);
#endif
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


// Find the builtin to use for invoking the debug break
static Handle<Code> DebugBreakForIC(Handle<Code> code, RelocInfo::Mode mode) {
  Isolate* isolate = code->GetIsolate();

  // Find the builtin debug break function matching the calling convention
  // used by the call site.
  if (code->is_inline_cache_stub()) {
    switch (code->kind()) {
      case Code::CALL_IC:
        return isolate->builtins()->CallICStub_DebugBreak();

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
    DCHECK(CodeStub::GetMajorKey(*code) == CodeStub::CallFunction);
    return isolate->builtins()->CallFunctionStub_DebugBreak();
  }

  UNREACHABLE();
  return Handle<Code>::null();
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
    Handle<Code> dbgbrk_code = DebugBreakForIC(target_code, mode);
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
  DCHECK(reloc_iterator_->done() == reloc_iterator_original_->done());
  return reloc_iterator_->done();
}


void BreakLocationIterator::RinfoNext() {
  reloc_iterator_->next();
  reloc_iterator_original_->next();
#ifdef DEBUG
  DCHECK(reloc_iterator_->done() == reloc_iterator_original_->done());
  if (!reloc_iterator_->done()) {
    DCHECK(rmode() == original_rmode());
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
  // TODO(isolates): frames_are_dropped_?
  base::NoBarrier_Store(&thread_local_.current_debug_scope_,
                        static_cast<base::AtomicWord>(0));
  thread_local_.restarter_frame_function_pointer_ = NULL;
}


char* Debug::ArchiveDebug(char* storage) {
  char* to = storage;
  MemCopy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
  ThreadInit();
  return storage + ArchiveSpacePerThread();
}


char* Debug::RestoreDebug(char* storage) {
  char* from = storage;
  MemCopy(reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
  return storage + ArchiveSpacePerThread();
}


int Debug::ArchiveSpacePerThread() {
  return sizeof(ThreadLocal);
}


ScriptCache::ScriptCache(Isolate* isolate) : HashMap(HashMap::PointersMatch),
                                             isolate_(isolate) {
  Heap* heap = isolate_->heap();
  HandleScope scope(isolate_);

  // Perform a GC to get rid of all unreferenced scripts.
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask, "ScriptCache");

  // Scan heap for Script objects.
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_allocation;

  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (obj->IsScript() && Script::cast(obj)->HasValidSource()) {
      Add(Handle<Script>(Script::cast(obj)));
    }
  }
}


void ScriptCache::Add(Handle<Script> script) {
  GlobalHandles* global_handles = isolate_->global_handles();
  // Create an entry in the hash map for the script.
  int id = script->id()->value();
  HashMap::Entry* entry =
      HashMap::Lookup(reinterpret_cast<void*>(id), Hash(id), true);
  if (entry->value != NULL) {
#ifdef DEBUG
    // The code deserializer may introduce duplicate Script objects.
    // Assert that the Script objects with the same id have the same name.
    Handle<Script> found(reinterpret_cast<Script**>(entry->value));
    DCHECK(script->id() == found->id());
    DCHECK(!script->name()->IsString() ||
           String::cast(script->name())->Equals(String::cast(found->name())));
#endif
    return;
  }
  // Globalize the script object, make it weak and use the location of the
  // global handle as the value in the hash map.
  Handle<Script> script_ =
      Handle<Script>::cast(global_handles->Create(*script));
  GlobalHandles::MakeWeak(reinterpret_cast<Object**>(script_.location()),
                          this,
                          ScriptCache::HandleWeakScript);
  entry->value = script_.location();
}


Handle<FixedArray> ScriptCache::GetScripts() {
  Factory* factory = isolate_->factory();
  Handle<FixedArray> instances = factory->NewFixedArray(occupancy());
  int count = 0;
  for (HashMap::Entry* entry = Start(); entry != NULL; entry = Next(entry)) {
    DCHECK(entry->value != NULL);
    if (entry->value != NULL) {
      instances->set(count, *reinterpret_cast<Script**>(entry->value));
      count++;
    }
  }
  return instances;
}


void ScriptCache::Clear() {
  // Iterate the script cache to get rid of all the weak handles.
  for (HashMap::Entry* entry = Start(); entry != NULL; entry = Next(entry)) {
    DCHECK(entry != NULL);
    Object** location = reinterpret_cast<Object**>(entry->value);
    DCHECK((*location)->IsScript());
    GlobalHandles::ClearWeakness(location);
    GlobalHandles::Destroy(location);
  }
  // Clear the content of the hash map.
  HashMap::Clear();
}


void ScriptCache::HandleWeakScript(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  // Retrieve the script identifier.
  Handle<Object> object = Utils::OpenHandle(*data.GetValue());
  int id = Handle<Script>::cast(object)->id()->value();
  void* key = reinterpret_cast<void*>(id);
  uint32_t hash = Hash(id);

  // Remove the corresponding entry from the cache.
  ScriptCache* script_cache =
      reinterpret_cast<ScriptCache*>(data.GetParameter());
  HashMap::Entry* entry = script_cache->Lookup(key, hash, false);
  Object** location = reinterpret_cast<Object**>(entry->value);
  script_cache->Remove(key, hash);

  // Clear the weak handle.
  GlobalHandles::Destroy(location);
}


void Debug::HandlePhantomDebugInfo(
    const v8::PhantomCallbackData<DebugInfoListNode>& data) {
  Debug* debug = reinterpret_cast<Isolate*>(data.GetIsolate())->debug();
  DebugInfoListNode* node = data.GetParameter();
  debug->RemoveDebugInfo(node);
#ifdef DEBUG
  for (DebugInfoListNode* n = debug->debug_info_list_;
       n != NULL;
       n = n->next()) {
    DCHECK(n != node);
  }
#endif
}


DebugInfoListNode::DebugInfoListNode(DebugInfo* debug_info): next_(NULL) {
  // Globalize the request debug info object and make it weak.
  GlobalHandles* global_handles = debug_info->GetIsolate()->global_handles();
  debug_info_ = Handle<DebugInfo>::cast(global_handles->Create(debug_info));
  typedef PhantomCallbackData<void>::Callback Callback;
  GlobalHandles::MakePhantom(
      reinterpret_cast<Object**>(debug_info_.location()), this, 0,
      reinterpret_cast<Callback>(Debug::HandlePhantomDebugInfo));
}


DebugInfoListNode::~DebugInfoListNode() {
  GlobalHandles::Destroy(reinterpret_cast<Object**>(debug_info_.location()));
}


bool Debug::CompileDebuggerScript(Isolate* isolate, int index) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  // Bail out if the index is invalid.
  if (index == -1) return false;

  // Find source and name for the requested script.
  Handle<String> source_code =
      isolate->bootstrapper()->NativesSourceLookup(index);
  Vector<const char> name = Natives::GetScriptName(index);
  Handle<String> script_name =
      factory->NewStringFromAscii(name).ToHandleChecked();
  Handle<Context> context = isolate->native_context();

  // Compile the script.
  Handle<SharedFunctionInfo> function_info;
  function_info = Compiler::CompileScript(
      source_code, script_name, 0, 0, false, false, context, NULL, NULL,
      ScriptCompiler::kNoCompileOptions, NATIVES_CODE, false);

  // Silently ignore stack overflows during compilation.
  if (function_info.is_null()) {
    DCHECK(isolate->has_pending_exception());
    isolate->clear_pending_exception();
    return false;
  }

  // Execute the shared function in the debugger context.
  Handle<JSFunction> function =
      factory->NewFunctionFromSharedFunctionInfo(function_info, context);

  MaybeHandle<Object> maybe_exception;
  MaybeHandle<Object> result = Execution::TryCall(
      function, handle(context->global_proxy()), 0, NULL, &maybe_exception);

  // Check for caught exceptions.
  if (result.is_null()) {
    DCHECK(!isolate->has_pending_exception());
    MessageLocation computed_location;
    isolate->ComputeLocation(&computed_location);
    Handle<Object> message = MessageHandler::MakeMessageObject(
        isolate, "error_loading_debugger", &computed_location,
        Vector<Handle<Object> >::empty(), Handle<JSArray>());
    DCHECK(!isolate->has_pending_exception());
    Handle<Object> exception;
    if (maybe_exception.ToHandle(&exception)) {
      isolate->set_pending_exception(*exception);
      MessageHandler::ReportMessage(isolate, NULL, message);
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
  if (is_loaded()) return true;

  // Bail out if we're already in the process of compiling the native
  // JavaScript source code for the debugger.
  if (is_suppressed_) return false;
  SuppressDebug while_loading(this);

  // Disable breakpoints and interrupts while compiling and running the
  // debugger scripts including the context creation code.
  DisableBreak disable(this, true);
  PostponeInterruptsScope postpone(isolate_);

  // Create the debugger context.
  HandleScope scope(isolate_);
  ExtensionConfiguration no_extensions;
  Handle<Context> context =
      isolate_->bootstrapper()->CreateEnvironment(
          MaybeHandle<JSGlobalProxy>(),
          v8::Handle<ObjectTemplate>(),
          &no_extensions);

  // Fail if no context could be created.
  if (context.is_null()) return false;

  // Use the debugger context.
  SaveContext save(isolate_);
  isolate_->set_context(*context);

  // Expose the builtins object in the debugger context.
  Handle<String> key = isolate_->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("builtins"));
  Handle<GlobalObject> global =
      Handle<GlobalObject>(context->global_object(), isolate_);
  Handle<JSBuiltinsObject> builtin =
      Handle<JSBuiltinsObject>(global->builtins(), isolate_);
  RETURN_ON_EXCEPTION_VALUE(
      isolate_, Object::SetProperty(global, key, builtin, SLOPPY), false);

  // Compile the JavaScript for the debugger in the debugger context.
  bool caught_exception =
      !CompileDebuggerScript(isolate_, Natives::GetIndex("mirror")) ||
      !CompileDebuggerScript(isolate_, Natives::GetIndex("debug"));

  if (FLAG_enable_liveedit) {
    caught_exception = caught_exception ||
        !CompileDebuggerScript(isolate_, Natives::GetIndex("liveedit"));
  }
  // Check for caught exceptions.
  if (caught_exception) return false;

  debug_context_ = Handle<Context>::cast(
      isolate_->global_handles()->Create(*context));
  return true;
}


void Debug::Unload() {
  ClearAllBreakPoints();
  ClearStepping();

  // Return debugger is not loaded.
  if (!is_loaded()) return;

  // Clear the script cache.
  if (script_cache_ != NULL) {
    delete script_cache_;
    script_cache_ = NULL;
  }

  // Clear debugger context global handle.
  GlobalHandles::Destroy(Handle<Object>::cast(debug_context_).location());
  debug_context_ = Handle<Context>();
}


void Debug::Break(Arguments args, JavaScriptFrame* frame) {
  Heap* heap = isolate_->heap();
  HandleScope scope(isolate_);
  DCHECK(args.length() == 0);

  // Initialize LiveEdit.
  LiveEdit::InitializeThreadLocal(this);

  // Just continue if breaks are disabled or debugger cannot be loaded.
  if (break_disabled()) return;

  // Enter the debugger.
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Postpone interrupt during breakpoint processing.
  PostponeInterruptsScope postpone(isolate_);

  // Get the debug info (create it if it does not exist).
  Handle<SharedFunctionInfo> shared =
      Handle<SharedFunctionInfo>(frame->function()->shared());
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
  if (StepOutActive() &&
      frame->fp() != thread_local_.step_out_fp_ &&
      break_points_hit->IsUndefined() ) {
      // Step count should always be 0 for StepOut.
      DCHECK(thread_local_.step_count_ == 0);
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

      PrepareStep(StepNext, step_count, StackFrame::NO_ID);
    } else {
      // Notify the debug event listeners.
      OnDebugBreak(break_points_hit, false);
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
    PrepareStep(step_action, step_count, StackFrame::NO_ID);
  }
}


RUNTIME_FUNCTION(Debug_Break) {
  // Get the top-most JavaScript frame.
  JavaScriptFrameIterator it(isolate);
  isolate->debug()->Break(args, it.frame());
  isolate->debug()->SetAfterBreakTarget(it.frame());
  return isolate->heap()->undefined_value();
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
  DCHECK(!break_point_objects->IsUndefined());
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
          STATIC_CHAR_VECTOR("IsBreakPointTriggered"));
  Handle<GlobalObject> debug_global(debug_context()->global_object());
  Handle<JSFunction> check_break_point =
    Handle<JSFunction>::cast(Object::GetProperty(
        debug_global, is_break_point_triggered_string).ToHandleChecked());

  // Get the break id as an object.
  Handle<Object> break_id = factory->NewNumberFromInt(Debug::break_id());

  // Call HandleBreakPointx.
  Handle<Object> argv[] = { break_id, break_point_object };
  Handle<Object> result;
  if (!Execution::TryCall(check_break_point,
                          isolate_->js_builtins_object(),
                          arraysize(argv),
                          argv).ToHandle(&result)) {
    return false;
  }

  // Return whether the break point is triggered.
  return result->IsTrue();
}


// Check whether the function has debug information.
bool Debug::HasDebugInfo(Handle<SharedFunctionInfo> shared) {
  return !shared->debug_info()->IsUndefined();
}


// Return the debug info for this function. EnsureDebugInfo must be called
// prior to ensure the debug info has been generated for shared.
Handle<DebugInfo> Debug::GetDebugInfo(Handle<SharedFunctionInfo> shared) {
  DCHECK(HasDebugInfo(shared));
  return Handle<DebugInfo>(DebugInfo::cast(shared->debug_info()));
}


bool Debug::SetBreakPoint(Handle<JSFunction> function,
                          Handle<Object> break_point_object,
                          int* source_position) {
  HandleScope scope(isolate_);

  PrepareForBreakPoints();

  // Make sure the function is compiled and has set up the debug info.
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if retrieving debug info failed.
    return true;
  }

  Handle<DebugInfo> debug_info = GetDebugInfo(shared);
  // Source positions starts with zero.
  DCHECK(*source_position >= 0);

  // Find the break point and change it.
  BreakLocationIterator it(debug_info, SOURCE_BREAK_LOCATIONS);
  it.FindBreakLocationFromPosition(*source_position, STATEMENT_ALIGNED);
  it.SetBreakPoint(break_point_object);

  *source_position = it.statement_position();

  // At least one active break point now.
  return debug_info->GetBreakPointCount() > 0;
}


bool Debug::SetBreakPointForScript(Handle<Script> script,
                                   Handle<Object> break_point_object,
                                   int* source_position,
                                   BreakPositionAlignment alignment) {
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
  DCHECK(position >= 0);

  // Find the break point and change it.
  BreakLocationIterator it(debug_info, SOURCE_BREAK_LOCATIONS);
  it.FindBreakLocationFromPosition(position, alignment);
  it.SetBreakPoint(break_point_object);

  position = (alignment == STATEMENT_ALIGNED) ? it.statement_position()
                                              : it.position();

  *source_position = position + shared->start_position();

  // At least one active break point now.
  DCHECK(debug_info->GetBreakPointCount() > 0);
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
        RemoveDebugInfoAndClearFromShared(debug_info);
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
    RemoveDebugInfoAndClearFromShared(debug_info_list_->debug_info());
  }
}


void Debug::FloodWithOneShot(Handle<JSFunction> function,
                             BreakLocatorType type) {
  // Do not ever break in native functions.
  if (function->IsFromNativeScript()) return;

  PrepareForBreakPoints();

  // Make sure the function is compiled and has set up the debug info.
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if we failed to retrieve the debug info.
    return;
  }

  // Flood the function with break points.
  BreakLocationIterator it(GetDebugInfo(shared), type);
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
      !JSFunction::cast(*bindee)->IsFromNativeScript()) {
    Handle<JSFunction> bindee_function(JSFunction::cast(*bindee));
    FloodWithOneShotGeneric(bindee_function);
  }
}


void Debug::FloodDefaultConstructorWithOneShot(Handle<JSFunction> function) {
  DCHECK(function->shared()->is_default_constructor());
  // Instead of stepping into the function we directly step into the super class
  // constructor.
  Isolate* isolate = function->GetIsolate();
  PrototypeIterator iter(isolate, function);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSFunction()) return;  // Object.prototype
  Handle<JSFunction> function_proto = Handle<JSFunction>::cast(proto);
  FloodWithOneShotGeneric(function_proto);
}


void Debug::FloodWithOneShotGeneric(Handle<JSFunction> function,
                                    Handle<Object> holder) {
  if (function->shared()->bound()) {
    FloodBoundFunctionWithOneShot(function);
  } else if (function->shared()->is_default_constructor()) {
    FloodDefaultConstructorWithOneShot(function);
  } else {
    Isolate* isolate = function->GetIsolate();
    // Don't allow step into functions in the native context.
    if (function->shared()->code() ==
            isolate->builtins()->builtin(Builtins::kFunctionApply) ||
        function->shared()->code() ==
            isolate->builtins()->builtin(Builtins::kFunctionCall)) {
      // Handle function.apply and function.call separately to flood the
      // function to be called and not the code for Builtins::FunctionApply or
      // Builtins::FunctionCall. The receiver of call/apply is the target
      // function.
      if (!holder.is_null() && holder->IsJSFunction()) {
        Handle<JSFunction> js_function = Handle<JSFunction>::cast(holder);
        FloodWithOneShotGeneric(js_function);
      }
    } else {
      FloodWithOneShot(function);
    }
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
      FloodWithOneShot(Handle<JSFunction>(frame->function()));
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


void Debug::PrepareStep(StepAction step_action,
                        int step_count,
                        StackFrame::Id frame_id) {
  HandleScope scope(isolate_);

  PrepareForBreakPoints();

  DCHECK(in_debug_scope());

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
  if (frame_id != StackFrame::NO_ID) {
    id = frame_id;
  }
  JavaScriptFrameIterator frames_it(isolate_, id);
  JavaScriptFrame* frame = frames_it.frame();

  // First of all ensure there is one-shot break points in the top handler
  // if any.
  FloodHandlerWithOneShot();

  // If the function on the top frame is unresolved perform step out. This will
  // be the case when calling unknown function and having the debugger stopped
  // in an unhandled exception.
  if (!frame->function()->IsJSFunction()) {
    // Step out: Find the calling JavaScript frame and flood it with
    // breakpoints.
    frames_it.Advance();
    // Fill the function to return to with one-shot break points.
    JSFunction* function = frames_it.frame()->function();
    FloodWithOneShot(Handle<JSFunction>(function));
    return;
  }

  // Get the debug info (create it if it does not exist).
  Handle<JSFunction> function(frame->function());
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
      if (code->is_call_stub()) {
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
      if ((maybe_call_function_stub->kind() == Code::STUB &&
           CodeStub::GetMajorKey(maybe_call_function_stub) ==
               CodeStub::CallFunction) ||
          maybe_call_function_stub->is_call_stub()) {
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
      DCHECK(it.IsExit());
      frames_it.Advance();
    }
    // Skip builtin functions on the stack.
    while (!frames_it.done() &&
           frames_it.frame()->function()->IsFromNativeScript()) {
      frames_it.Advance();
    }
    // Step out: If there is a JavaScript caller frame, we need to
    // flood it with breakpoints.
    if (!frames_it.done()) {
      // Fill the function to return to with one-shot break points.
      JSFunction* function = frames_it.frame()->function();
      FloodWithOneShot(Handle<JSFunction>(function));
      // Set target frame pointer.
      ActivateStepOut(frames_it.frame());
    }
  } else if (!(is_inline_cache_stub || RelocInfo::IsConstructCall(it.rmode()) ||
               !call_function_stub.is_null() || is_at_restarted_function)
             || step_action == StepNext || step_action == StepMin) {
    // Step next or step min.

    // Fill the current function with one-shot break points.
    // If we are stepping into another frame, only fill calls and returns.
    FloodWithOneShot(function, step_action == StepFrame ? CALLS_AND_RETURNS
                                                        : ALL_BREAK_LOCATIONS);

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
      bool is_call_ic = call_function_stub->kind() == Code::CALL_IC;

      // Find out number of arguments from the stub minor key.
      uint32_t key = call_function_stub->stub_key();
      // Argc in the stub is the number of arguments passed - not the
      // expected arguments of the called function.
      int call_function_arg_count = is_call_ic
          ? CallICStub::ExtractArgcFromMinorKey(CodeStub::MinorKeyFromKey(key))
          : CallFunctionStub::ExtractArgcFromMinorKey(
              CodeStub::MinorKeyFromKey(key));

      DCHECK(is_call_ic ||
             CodeStub::GetMajorKey(*call_function_stub) ==
                 CodeStub::MajorKeyFromKey(key));

      // Find target function on the expression stack.
      // Expression stack looks like this (top to bottom):
      // argN
      // ...
      // arg0
      // Receiver
      // Function to call
      int expressions_count = frame->ComputeExpressionsCount();
      DCHECK(expressions_count - 2 - call_function_arg_count >= 0);
      Object* fun = frame->GetExpression(
          expressions_count - 2 - call_function_arg_count);

      // Flood the actual target of call/apply.
      if (fun->IsJSFunction()) {
        Isolate* isolate = JSFunction::cast(fun)->GetIsolate();
        Code* apply = isolate->builtins()->builtin(Builtins::kFunctionApply);
        Code* call = isolate->builtins()->builtin(Builtins::kFunctionCall);
        while (fun->IsJSFunction()) {
          Code* code = JSFunction::cast(fun)->shared()->code();
          if (code != apply && code != call) break;
          fun = frame->GetExpression(
              expressions_count - 1 - call_function_arg_count);
        }
      }

      if (fun->IsJSFunction()) {
        Handle<JSFunction> js_function(JSFunction::cast(fun));
        FloodWithOneShotGeneric(js_function);
      }
    }

    // Fill the current function with one-shot break points even for step in on
    // a call target as the function called might be a native function for
    // which step in will not stop. It also prepares for stepping in
    // getters/setters.
    // If we are stepping into another frame, only fill calls and returns.
    FloodWithOneShot(function, step_action == StepFrame ? CALLS_AND_RETURNS
                                                        : ALL_BREAK_LOCATIONS);

    if (is_load_or_store) {
      // Remember source position and frame to handle step in getter/setter. If
      // there is a custom getter/setter it will be handled in
      // Object::Get/SetPropertyWithAccessor, otherwise the step action will be
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
  StepAction step_action = thread_local_.last_step_action_;

  if (step_action == StepNext || step_action == StepOut) {
    if (frame->fp() < thread_local_.last_fp_) return true;
  }

  // We stepped into a new frame if the frame pointer changed.
  if (step_action == StepFrame) {
    return frame->UnpaddedFP() == thread_local_.last_fp_;
  }

  // If the step last action was step next or step in make sure that a new
  // statement is hit.
  if (step_action == StepNext || step_action == StepIn) {
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
  return code->is_debug_stub() && code->extra_ic_state() == DEBUG_BREAK;
}





// Simple function for returning the source positions for active break points.
Handle<Object> Debug::GetSourceBreakLocations(
    Handle<SharedFunctionInfo> shared,
    BreakPositionAlignment position_alignment) {
  Isolate* isolate = shared->GetIsolate();
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
        Smi* position = NULL;
        switch (position_alignment) {
          case STATEMENT_ALIGNED:
            position = break_point_info->statement_position();
            break;
          case BREAK_POSITION_ALIGNED:
            position = break_point_info->source_position();
            break;
        }

        locations->set(count++, position);
      }
    }
  }
  return locations;
}


// Handle stepping into a function.
void Debug::HandleStepIn(Handle<Object> function_obj, Handle<Object> holder,
                         Address fp, bool is_constructor) {
  // Flood getter/setter if we either step in or step to another frame.
  bool step_frame = thread_local_.last_step_action_ == StepFrame;
  if (!StepInActive() && !step_frame) return;
  if (!function_obj->IsJSFunction()) return;
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_obj);
  Isolate* isolate = function->GetIsolate();
  // If the frame pointer is not supplied by the caller find it.
  if (fp == 0) {
    StackFrameIterator it(isolate);
    it.Advance();
    // For constructor functions skip another frame.
    if (is_constructor) {
      DCHECK(it.frame()->is_construct());
      it.Advance();
    }
    fp = it.frame()->fp();
  }

  // Flood the function with one-shot break points if it is called from where
  // step into was requested, or when stepping into a new frame.
  if (fp == thread_local_.step_into_fp_ || step_frame) {
    FloodWithOneShotGeneric(function, holder);
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
  DCHECK(!StepOutActive());
  thread_local_.step_into_fp_ = frame->UnpaddedFP();
}


void Debug::ClearStepIn() {
  thread_local_.step_into_fp_ = 0;
}


void Debug::ActivateStepOut(StackFrame* frame) {
  DCHECK(!StepInActive());
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
      List<JSFunction*> functions(FLAG_max_inlining_levels + 1);
      frame->GetFunctions(&functions);
      for (int i = 0; i < functions.length(); i++) {
        JSFunction* function = functions[i];
        active_functions->Add(Handle<JSFunction>(function));
        function->shared()->code()->set_gc_metadata(active_code_marker);
      }
    } else if (frame->function()->IsJSFunction()) {
      JSFunction* function = frame->function();
      DCHECK(frame->LookupCode()->kind() == Code::FUNCTION);
      active_functions->Add(Handle<JSFunction>(function));
      function->shared()->code()->set_gc_metadata(active_code_marker);
    }
  }
}


// Figure out how many bytes of "pc_offset" correspond to actual code by
// subtracting off the bytes that correspond to constant/veneer pools.  See
// Assembler::CheckConstPool() and Assembler::CheckVeneerPool(). Note that this
// is only useful for architectures using constant pools or veneer pools.
static int ComputeCodeOffsetFromPcOffset(Code *code, int pc_offset) {
  DCHECK_EQ(code->kind(), Code::FUNCTION);
  DCHECK(!code->has_debug_break_slots());
  DCHECK_LE(0, pc_offset);
  DCHECK_LT(pc_offset, code->instruction_end() - code->instruction_start());

  int mask = RelocInfo::ModeMask(RelocInfo::CONST_POOL) |
             RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  byte *pc = code->instruction_start() + pc_offset;
  int code_offset = pc_offset;
  for (RelocIterator it(code, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->pc() >= pc) break;
    DCHECK(RelocInfo::IsConstPool(info->rmode()));
    code_offset -= static_cast<int>(info->data());
    DCHECK_LE(0, code_offset);
  }

  return code_offset;
}


// The inverse of ComputeCodeOffsetFromPcOffset.
static int ComputePcOffsetFromCodeOffset(Code *code, int code_offset) {
  DCHECK_EQ(code->kind(), Code::FUNCTION);

  int mask = RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT) |
             RelocInfo::ModeMask(RelocInfo::CONST_POOL) |
             RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  int reloc = 0;
  for (RelocIterator it(code, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->pc() - code->instruction_start() - reloc >= code_offset) break;
    if (RelocInfo::IsDebugBreakSlot(info->rmode())) {
      reloc += Assembler::kDebugBreakSlotLength;
    } else {
      DCHECK(RelocInfo::IsConstPool(info->rmode()));
      reloc += static_cast<int>(info->data());
    }
  }

  int pc_offset = code_offset + reloc;

  DCHECK_LT(code->instruction_start() + pc_offset, code->instruction_end());

  return pc_offset;
}


static void RedirectActivationsToRecompiledCodeOnThread(
    Isolate* isolate,
    ThreadLocalTop* top) {
  for (JavaScriptFrameIterator it(isolate, top); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();

    if (frame->is_optimized() || !frame->function()->IsJSFunction()) continue;

    JSFunction* function = frame->function();

    DCHECK(frame->LookupCode()->kind() == Code::FUNCTION);

    Handle<Code> frame_code(frame->LookupCode());
    if (frame_code->has_debug_break_slots()) continue;

    Handle<Code> new_code(function->shared()->code());
    if (new_code->kind() != Code::FUNCTION ||
        !new_code->has_debug_break_slots()) {
      continue;
    }

    int old_pc_offset =
        static_cast<int>(frame->pc() - frame_code->instruction_start());
    int code_offset = ComputeCodeOffsetFromPcOffset(*frame_code, old_pc_offset);
    int new_pc_offset = ComputePcOffsetFromCodeOffset(*new_code, code_offset);

    // Compute the equivalent pc in the new code.
    byte* new_pc = new_code->instruction_start() + new_pc_offset;

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

    if (FLAG_enable_ool_constant_pool) {
      // Update constant pool pointer for new code.
      frame->set_constant_pool(new_code->constant_pool());
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


static void EnsureFunctionHasDebugBreakSlots(Handle<JSFunction> function) {
  if (function->code()->kind() == Code::FUNCTION &&
      function->code()->has_debug_break_slots()) {
    // Nothing to do. Function code already had debug break slots.
    return;
  }
  // Make sure that the shared full code is compiled with debug
  // break slots.
  if (!function->shared()->code()->has_debug_break_slots()) {
    MaybeHandle<Code> code = Compiler::GetDebugCode(function);
    // Recompilation can fail.  In that case leave the code as it was.
    if (!code.is_null()) function->ReplaceCode(*code.ToHandleChecked());
  } else {
    // Simply use shared code if it has debug break slots.
    function->ReplaceCode(function->shared()->code());
  }
}


static void RecompileAndRelocateSuspendedGenerators(
    const List<Handle<JSGeneratorObject> > &generators) {
  for (int i = 0; i < generators.length(); i++) {
    Handle<JSFunction> fun(generators[i]->function());

    EnsureFunctionHasDebugBreakSlots(fun);

    int code_offset = generators[i]->continuation();
    int pc_offset = ComputePcOffsetFromCodeOffset(fun->code(), code_offset);
    generators[i]->set_continuation(pc_offset);
  }
}


static bool SkipSharedFunctionInfo(SharedFunctionInfo* shared,
                                   Object* active_code_marker) {
  if (!shared->allows_lazy_compilation()) return true;
  if (!shared->script()->IsScript()) return true;
  Object* script = shared->script();
  if (!script->IsScript()) return true;
  if (Script::cast(script)->type()->value() == Script::TYPE_NATIVE) return true;
  Code* shared_code = shared->code();
  return shared_code->gc_metadata() == active_code_marker;
}


static inline bool HasDebugBreakSlots(Code* code) {
  return code->kind() == Code::FUNCTION && code->has_debug_break_slots();
}


void Debug::PrepareForBreakPoints() {
  // If preparing for the first break point make sure to deoptimize all
  // functions as debugging does not work with optimized code.
  if (!has_break_points_) {
    if (isolate_->concurrent_recompilation_enabled()) {
      isolate_->optimizing_compiler_thread()->Flush();
    }

    Deoptimizer::DeoptimizeAll(isolate_);

    Handle<Code> lazy_compile = isolate_->builtins()->CompileLazy();

    // There will be at least one break point when we are done.
    has_break_points_ = true;

    // Keep the list of activated functions in a handlified list as it
    // is used both in GC and non-GC code.
    List<Handle<JSFunction> > active_functions(100);

    // A list of all suspended generators.
    List<Handle<JSGeneratorObject> > suspended_generators;

    // A list of all generator functions.  We need to recompile all functions,
    // but we don't know until after visiting the whole heap which generator
    // functions have suspended activations and which do not.  As in the case of
    // functions with activations on the stack, we need to be careful with
    // generator functions with suspended activations because although they
    // should be recompiled, recompilation can fail, and we need to avoid
    // leaving the heap in an inconsistent state.
    //
    // We could perhaps avoid this list and instead re-use the GC metadata
    // links.
    List<Handle<JSFunction> > generator_functions;

    {
      // We are going to iterate heap to find all functions without
      // debug break slots.
      Heap* heap = isolate_->heap();
      heap->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                              "preparing for breakpoints");
      HeapIterator iterator(heap);

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
      HeapObject* obj = NULL;
      while (((obj = iterator.next()) != NULL)) {
        if (obj->IsJSFunction()) {
          JSFunction* function = JSFunction::cast(obj);
          SharedFunctionInfo* shared = function->shared();
          if (SkipSharedFunctionInfo(shared, active_code_marker)) continue;
          if (shared->is_generator()) {
            generator_functions.Add(Handle<JSFunction>(function, isolate_));
            continue;
          }
          if (HasDebugBreakSlots(function->code())) continue;
          Code* fallback = HasDebugBreakSlots(shared->code()) ? shared->code()
                                                              : *lazy_compile;
          Code::Kind kind = function->code()->kind();
          if (kind == Code::FUNCTION ||
              (kind == Code::BUILTIN &&  // Abort in-flight compilation.
               (function->IsInOptimizationQueue() ||
                function->IsMarkedForOptimization() ||
                function->IsMarkedForConcurrentOptimization()))) {
            function->ReplaceCode(fallback);
          }
          if (kind == Code::OPTIMIZED_FUNCTION) {
            // Optimized code can only get here if DeoptimizeAll did not
            // deoptimize turbo fan code.
            DCHECK(!FLAG_turbo_deoptimization);
            DCHECK(function->code()->is_turbofanned());
            function->ReplaceCode(fallback);
          }
        } else if (obj->IsJSGeneratorObject()) {
          JSGeneratorObject* gen = JSGeneratorObject::cast(obj);
          if (!gen->is_suspended()) continue;

          JSFunction* fun = gen->function();
          DCHECK_EQ(fun->code()->kind(), Code::FUNCTION);
          if (fun->code()->has_debug_break_slots()) continue;

          int pc_offset = gen->continuation();
          DCHECK_LT(0, pc_offset);

          int code_offset =
              ComputeCodeOffsetFromPcOffset(fun->code(), pc_offset);

          // This will be fixed after we recompile the functions.
          gen->set_continuation(code_offset);

          suspended_generators.Add(Handle<JSGeneratorObject>(gen, isolate_));
        } else if (obj->IsSharedFunctionInfo()) {
          SharedFunctionInfo* shared = SharedFunctionInfo::cast(obj);
          if (SkipSharedFunctionInfo(shared, active_code_marker)) continue;
          if (shared->is_generator()) continue;
          if (HasDebugBreakSlots(shared->code())) continue;
          shared->ReplaceCode(*lazy_compile);
        }
      }

      // Clear gc_metadata field.
      for (int i = 0; i < active_functions.length(); i++) {
        Handle<JSFunction> function = active_functions[i];
        function->shared()->code()->set_gc_metadata(Smi::FromInt(0));
      }
    }

    // Recompile generator functions that have suspended activations, and
    // relocate those activations.
    RecompileAndRelocateSuspendedGenerators(suspended_generators);

    // Mark generator functions that didn't have suspended activations for lazy
    // recompilation.  Note that this set does not include any active functions.
    for (int i = 0; i < generator_functions.length(); i++) {
      Handle<JSFunction> &function = generator_functions[i];
      if (function->code()->kind() != Code::FUNCTION) continue;
      if (function->code()->has_debug_break_slots()) continue;
      function->ReplaceCode(*lazy_compile);
      function->shared()->ReplaceCode(*lazy_compile);
    }

    // Now recompile all functions with activation frames and and
    // patch the return address to run in the new compiled code.  It could be
    // that some active functions were recompiled already by the suspended
    // generator recompilation pass above; a generator with suspended
    // activations could also have active activations.  That's fine.
    for (int i = 0; i < active_functions.length(); i++) {
      Handle<JSFunction> function = active_functions[i];
      Handle<SharedFunctionInfo> shared(function->shared());

      // If recompilation is not possible just skip it.
      if (shared->is_toplevel()) continue;
      if (!shared->allows_lazy_compilation()) continue;
      if (shared->code()->kind() == Code::BUILTIN) continue;

      EnsureFunctionHasDebugBreakSlots(function);
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
    { // Extra scope for iterator.
      // If lazy compilation is off, we won't have duplicate shared function
      // infos that need to be filtered.
      HeapIterator iterator(heap, FLAG_lazy ? HeapIterator::kNoFiltering
                                            : HeapIterator::kFilterUnreachable);
      for (HeapObject* obj = iterator.next();
           obj != NULL; obj = iterator.next()) {
        bool found_next_candidate = false;
        Handle<JSFunction> function;
        Handle<SharedFunctionInfo> shared;
        if (obj->IsJSFunction()) {
          function = Handle<JSFunction>(JSFunction::cast(obj));
          shared = Handle<SharedFunctionInfo>(function->shared());
          DCHECK(shared->allows_lazy_compilation() || shared->is_compiled());
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
      MaybeHandle<Code> maybe_result = target_function.is_null()
          ? Compiler::GetUnoptimizedCode(target)
          : Compiler::GetUnoptimizedCode(target_function);
      if (maybe_result.is_null()) return isolate_->heap()->undefined_value();
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
    DCHECK(shared->is_compiled());
    return true;
  }

  // There will be at least one break point when we are done.
  has_break_points_ = true;

  // Ensure function is compiled. Return false if this failed.
  if (!function.is_null() &&
      !Compiler::EnsureCompiled(function, CLEAR_EXCEPTION)) {
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


void Debug::RemoveDebugInfo(DebugInfoListNode* prev, DebugInfoListNode* node) {
  // Unlink from list. If prev is NULL we are looking at the first element.
  if (prev == NULL) {
    debug_info_list_ = node->next();
  } else {
    prev->set_next(node->next());
  }
  delete node;

  // If there are no more debug info objects there are not more break
  // points.
  has_break_points_ = debug_info_list_ != NULL;
}


void Debug::RemoveDebugInfo(DebugInfo** debug_info) {
  DCHECK(debug_info_list_ != NULL);
  // Run through the debug info objects to find this one and remove it.
  DebugInfoListNode* prev = NULL;
  DebugInfoListNode* current = debug_info_list_;
  while (current != NULL) {
    if (current->debug_info().location() == debug_info) {
      RemoveDebugInfo(prev, current);
      return;
    }
    // Move to next in list.
    prev = current;
    current = current->next();
  }
  UNREACHABLE();
}


void Debug::RemoveDebugInfo(DebugInfoListNode* node) {
  DCHECK(debug_info_list_ != NULL);
  // Run through the debug info objects to find this one and remove it.
  DebugInfoListNode* prev = NULL;
  DebugInfoListNode* current = debug_info_list_;
  while (current != NULL) {
    if (current == node) {
      RemoveDebugInfo(prev, node);
      return;
    }
    // Move to next in list.
    prev = current;
    current = current->next();
  }
  UNREACHABLE();
}


void Debug::RemoveDebugInfoAndClearFromShared(Handle<DebugInfo> debug_info) {
  HandleScope scope(isolate_);
  Handle<SharedFunctionInfo> shared(debug_info->shared());

  RemoveDebugInfo(debug_info.location());

  shared->set_debug_info(isolate_->heap()->undefined_value());
}


void Debug::SetAfterBreakTarget(JavaScriptFrame* frame) {
  after_break_target_ = NULL;

  if (LiveEdit::SetAfterBreakTarget(this)) return;  // LiveEdit did the job.

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
  DCHECK(frame_code.is_identical_to(code));
#endif

  // Find the call address in the running code. This address holds the call to
  // either a DebugBreakXXX or to the debug break return entry code if the
  // break point is still active after processing the break point.
  Address addr = Assembler::break_address_from_return_address(frame->pc());

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
    // If the break point at return is still active jump to the corresponding
    // place in the original code. If not the break point was removed during
    // break point processing.
    if (break_at_js_return_active) {
      addr += original_code->instruction_start() - code->instruction_start();
    }

    // Move back to where the call instruction sequence started.
    after_break_target_ = addr - Assembler::kPatchReturnSequenceAddressOffset;
  } else if (at_debug_break_slot) {
    // Address of where the debug break slot starts.
    addr = addr - Assembler::kPatchDebugBreakSlotAddressOffset;

    // Continue just after the slot.
    after_break_target_ = addr + Assembler::kDebugBreakSlotLength;
  } else {
    addr = Assembler::target_address_from_return_address(frame->pc());
    if (IsDebugBreak(Assembler::target_address_at(addr, *code))) {
      // We now know that there is still a debug break call at the target
      // address, so the break point is still there and the original code will
      // hold the address to jump to in order to complete the call which is
      // replaced by a call to DebugBreakXXX.

      // Find the corresponding address in the original code.
      addr += original_code->instruction_start() - code->instruction_start();

      // Install jump to the call address in the original code. This will be the
      // call which was overwritten by the call to DebugBreakXXX.
      after_break_target_ = Assembler::target_address_at(addr, *original_code);
    } else {
      // There is no longer a break point present. Don't try to look in the
      // original code as the running code will have the right address. This
      // takes care of the case where the last break point is removed from the
      // function and therefore no "original code" is available.
      after_break_target_ = Assembler::target_address_at(addr, *code);
    }
  }
}


bool Debug::IsBreakAtReturn(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);

  // If there are no break points this cannot be break at return, as
  // the debugger statement and stack guard debug break cannot be at
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
  DCHECK(frame_code.is_identical_to(code));
#endif

  // Find the call address in the running code.
  Address addr = Assembler::break_address_from_return_address(frame->pc());

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
                                  LiveEdit::FrameDropMode mode,
                                  Object** restarter_frame_function_pointer) {
  if (mode != LiveEdit::CURRENTLY_SET_MODE) {
    thread_local_.frame_drop_mode_ = mode;
  }
  thread_local_.break_frame_id_ = new_break_frame_id;
  thread_local_.restarter_frame_function_pointer_ =
      restarter_frame_function_pointer;
}


bool Debug::IsDebugGlobal(GlobalObject* global) {
  return is_loaded() && global == debug_context()->global_object();
}


void Debug::ClearMirrorCache() {
  PostponeInterruptsScope postpone(isolate_);
  HandleScope scope(isolate_);
  AssertDebugContext();
  Factory* factory = isolate_->factory();
  Handle<GlobalObject> global(isolate_->global_object());
  JSObject::SetProperty(global,
                        factory->NewStringFromAsciiChecked("next_handle_"),
                        handle(Smi::FromInt(0), isolate_), SLOPPY).Check();
  JSObject::SetProperty(global,
                        factory->NewStringFromAsciiChecked("mirror_cache_"),
                        factory->NewJSArray(0, FAST_ELEMENTS), SLOPPY).Check();
}


Handle<FixedArray> Debug::GetLoadedScripts() {
  // Create and fill the script cache when the loaded scripts is requested for
  // the first time.
  if (script_cache_ == NULL) script_cache_ = new ScriptCache(isolate_);

  // Perform GC to get unreferenced scripts evicted from the cache before
  // returning the content.
  isolate_->heap()->CollectAllGarbage(Heap::kNoGCFlags,
                                      "Debug::GetLoadedScripts");

  // Get the scripts from the cache.
  return script_cache_->GetScripts();
}


void Debug::RecordEvalCaller(Handle<Script> script) {
  script->set_compilation_type(Script::COMPILATION_TYPE_EVAL);
  // For eval scripts add information on the function from which eval was
  // called.
  StackTraceFrameIterator it(script->GetIsolate());
  if (!it.done()) {
    script->set_eval_from_shared(it.frame()->function()->shared());
    Code* code = it.frame()->LookupCode();
    int offset = static_cast<int>(
        it.frame()->pc() - code->instruction_start());
    script->set_eval_from_instructions_offset(Smi::FromInt(offset));
  }
}


MaybeHandle<Object> Debug::MakeJSObject(const char* constructor_name,
                                        int argc,
                                        Handle<Object> argv[]) {
  AssertDebugContext();
  // Create the execution state object.
  Handle<GlobalObject> global(isolate_->global_object());
  Handle<Object> constructor = Object::GetProperty(
      isolate_, global, constructor_name).ToHandleChecked();
  DCHECK(constructor->IsJSFunction());
  if (!constructor->IsJSFunction()) return MaybeHandle<Object>();
  // We do not handle interrupts here.  In particular, termination interrupts.
  PostponeInterruptsScope no_interrupts(isolate_);
  return Execution::TryCall(Handle<JSFunction>::cast(constructor),
                            handle(debug_context()->global_proxy()),
                            argc,
                            argv);
}


MaybeHandle<Object> Debug::MakeExecutionState() {
  // Create the execution state object.
  Handle<Object> argv[] = { isolate_->factory()->NewNumberFromInt(break_id()) };
  return MakeJSObject("MakeExecutionState", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakeBreakEvent(Handle<Object> break_points_hit) {
  // Create the new break event object.
  Handle<Object> argv[] = { isolate_->factory()->NewNumberFromInt(break_id()),
                            break_points_hit };
  return MakeJSObject("MakeBreakEvent", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakeExceptionEvent(Handle<Object> exception,
                                              bool uncaught,
                                              Handle<Object> promise) {
  // Create the new exception event object.
  Handle<Object> argv[] = { isolate_->factory()->NewNumberFromInt(break_id()),
                            exception,
                            isolate_->factory()->ToBoolean(uncaught),
                            promise };
  return MakeJSObject("MakeExceptionEvent", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakeCompileEvent(Handle<Script> script,
                                            v8::DebugEvent type) {
  // Create the compile event object.
  Handle<Object> script_wrapper = Script::GetWrapper(script);
  Handle<Object> argv[] = { script_wrapper,
                            isolate_->factory()->NewNumberFromInt(type) };
  return MakeJSObject("MakeCompileEvent", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakePromiseEvent(Handle<JSObject> event_data) {
  // Create the promise event object.
  Handle<Object> argv[] = { event_data };
  return MakeJSObject("MakePromiseEvent", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakeAsyncTaskEvent(Handle<JSObject> task_event) {
  // Create the async task event object.
  Handle<Object> argv[] = { task_event };
  return MakeJSObject("MakeAsyncTaskEvent", arraysize(argv), argv);
}


void Debug::OnThrow(Handle<Object> exception, bool uncaught) {
  if (in_debug_scope() || ignore_events()) return;
  // Temporarily clear any scheduled_exception to allow evaluating
  // JavaScript from the debug event handler.
  HandleScope scope(isolate_);
  Handle<Object> scheduled_exception;
  if (isolate_->has_scheduled_exception()) {
    scheduled_exception = handle(isolate_->scheduled_exception(), isolate_);
    isolate_->clear_scheduled_exception();
  }
  OnException(exception, uncaught, isolate_->GetPromiseOnStackOnThrow());
  if (!scheduled_exception.is_null()) {
    isolate_->thread_local_top()->scheduled_exception_ = *scheduled_exception;
  }
}


void Debug::OnPromiseReject(Handle<JSObject> promise, Handle<Object> value) {
  if (in_debug_scope() || ignore_events()) return;
  HandleScope scope(isolate_);
  // Check whether the promise has been marked as having triggered a message.
  Handle<Symbol> key = isolate_->factory()->promise_debug_marker_symbol();
  if (JSObject::GetDataProperty(promise, key)->IsUndefined()) {
    OnException(value, false, promise);
  }
}


MaybeHandle<Object> Debug::PromiseHasUserDefinedRejectHandler(
    Handle<JSObject> promise) {
  Handle<JSFunction> fun = Handle<JSFunction>::cast(
      JSObject::GetDataProperty(isolate_->js_builtins_object(),
                                isolate_->factory()->NewStringFromStaticChars(
                                    "PromiseHasUserDefinedRejectHandler")));
  return Execution::Call(isolate_, fun, promise, 0, NULL);
}


void Debug::OnException(Handle<Object> exception, bool uncaught,
                        Handle<Object> promise) {
  if (!uncaught && promise->IsJSObject()) {
    Handle<JSObject> jspromise = Handle<JSObject>::cast(promise);
    // Mark the promise as already having triggered a message.
    Handle<Symbol> key = isolate_->factory()->promise_debug_marker_symbol();
    JSObject::SetProperty(jspromise, key, key, STRICT).Assert();
    // Check whether the promise reject is considered an uncaught exception.
    Handle<Object> has_reject_handler;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, has_reject_handler,
        PromiseHasUserDefinedRejectHandler(jspromise), /* void */);
    uncaught = has_reject_handler->IsFalse();
  }
  // Bail out if exception breaks are not active
  if (uncaught) {
    // Uncaught exceptions are reported by either flags.
    if (!(break_on_uncaught_exception_ || break_on_exception_)) return;
  } else {
    // Caught exceptions are reported is activated.
    if (!break_on_exception_) return;
  }

  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Clear all current stepping setup.
  ClearStepping();

  // Create the event data object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeExceptionEvent(
          exception, uncaught, promise).ToHandle(&event_data)) {
    return;
  }

  // Process debug event.
  ProcessDebugEvent(v8::Exception, Handle<JSObject>::cast(event_data), false);
  // Return to continue execution from where the exception was thrown.
}


void Debug::OnCompileError(Handle<Script> script) {
  if (ignore_events()) return;

  if (in_debug_scope()) {
    ProcessCompileEventInDebugScope(v8::CompileError, script);
    return;
  }

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Create the compile state object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeCompileEvent(script, v8::CompileError).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::CompileError, Handle<JSObject>::cast(event_data), true);
}


void Debug::OnDebugBreak(Handle<Object> break_points_hit,
                            bool auto_continue) {
  // The caller provided for DebugScope.
  AssertDebugContext();
  // Bail out if there is no listener for this event
  if (ignore_events()) return;

  HandleScope scope(isolate_);
  // Create the event data object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeBreakEvent(break_points_hit).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::Break,
                    Handle<JSObject>::cast(event_data),
                    auto_continue);
}


void Debug::OnBeforeCompile(Handle<Script> script) {
  if (in_debug_scope() || ignore_events()) return;

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Create the event data object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeCompileEvent(script, v8::BeforeCompile).ToHandle(&event_data))
    return;

  // Process debug event.
  ProcessDebugEvent(v8::BeforeCompile,
                    Handle<JSObject>::cast(event_data),
                    true);
}


// Handle debugger actions when a new script is compiled.
void Debug::OnAfterCompile(Handle<Script> script) {
  // Add the newly compiled script to the script cache.
  if (script_cache_ != NULL) script_cache_->Add(script);

  if (ignore_events()) return;

  if (in_debug_scope()) {
    ProcessCompileEventInDebugScope(v8::AfterCompile, script);
    return;
  }

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // If debugging there might be script break points registered for this
  // script. Make sure that these break points are set.

  // Get the function UpdateScriptBreakPoints (defined in debug-debugger.js).
  Handle<String> update_script_break_points_string =
      isolate_->factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("UpdateScriptBreakPoints"));
  Handle<GlobalObject> debug_global(debug_context()->global_object());
  Handle<Object> update_script_break_points =
      Object::GetProperty(
          debug_global, update_script_break_points_string).ToHandleChecked();
  if (!update_script_break_points->IsJSFunction()) {
    return;
  }
  DCHECK(update_script_break_points->IsJSFunction());

  // Wrap the script object in a proper JS object before passing it
  // to JavaScript.
  Handle<Object> wrapper = Script::GetWrapper(script);

  // Call UpdateScriptBreakPoints expect no exceptions.
  Handle<Object> argv[] = { wrapper };
  if (Execution::TryCall(Handle<JSFunction>::cast(update_script_break_points),
                         isolate_->js_builtins_object(),
                         arraysize(argv),
                         argv).is_null()) {
    return;
  }

  // Create the compile state object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeCompileEvent(script, v8::AfterCompile).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::AfterCompile, Handle<JSObject>::cast(event_data), true);
}


void Debug::OnPromiseEvent(Handle<JSObject> data) {
  if (in_debug_scope() || ignore_events()) return;

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Create the script collected state object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakePromiseEvent(data).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::PromiseEvent,
                    Handle<JSObject>::cast(event_data),
                    true);
}


void Debug::OnAsyncTaskEvent(Handle<JSObject> data) {
  if (in_debug_scope() || ignore_events()) return;

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Create the script collected state object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeAsyncTaskEvent(data).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::AsyncTaskEvent,
                    Handle<JSObject>::cast(event_data),
                    true);
}


void Debug::ProcessDebugEvent(v8::DebugEvent event,
                              Handle<JSObject> event_data,
                              bool auto_continue) {
  HandleScope scope(isolate_);

  // Create the execution state.
  Handle<Object> exec_state;
  // Bail out and don't call debugger if exception.
  if (!MakeExecutionState().ToHandle(&exec_state)) return;

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


void Debug::CallEventCallback(v8::DebugEvent event,
                              Handle<Object> exec_state,
                              Handle<Object> event_data,
                              v8::Debug::ClientData* client_data) {
  bool previous = in_debug_event_listener_;
  in_debug_event_listener_ = true;
  if (event_listener_->IsForeign()) {
    // Invoke the C debug event listener.
    v8::Debug::EventCallback callback =
        FUNCTION_CAST<v8::Debug::EventCallback>(
            Handle<Foreign>::cast(event_listener_)->foreign_address());
    EventDetailsImpl event_details(event,
                                   Handle<JSObject>::cast(exec_state),
                                   Handle<JSObject>::cast(event_data),
                                   event_listener_data_,
                                   client_data);
    callback(event_details);
    DCHECK(!isolate_->has_scheduled_exception());
  } else {
    // Invoke the JavaScript debug event listener.
    DCHECK(event_listener_->IsJSFunction());
    Handle<Object> argv[] = { Handle<Object>(Smi::FromInt(event), isolate_),
                              exec_state,
                              event_data,
                              event_listener_data_ };
    Handle<JSReceiver> global(isolate_->global_proxy());
    Execution::TryCall(Handle<JSFunction>::cast(event_listener_),
                       global, arraysize(argv), argv);
  }
  in_debug_event_listener_ = previous;
}


void Debug::ProcessCompileEventInDebugScope(v8::DebugEvent event,
                                            Handle<Script> script) {
  if (event_listener_.is_null()) return;

  SuppressDebug while_processing(this);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeCompileEvent(script, event).ToHandle(&event_data)) return;

  // Create the execution state.
  Handle<Object> exec_state;
  // Bail out and don't call debugger if exception.
  if (!MakeExecutionState().ToHandle(&exec_state)) return;

  CallEventCallback(event, exec_state, event_data, NULL);
}


Handle<Context> Debug::GetDebugContext() {
  DebugScope debug_scope(this);
  // The global handle may be destroyed soon after.  Return it reboxed.
  return handle(*debug_context(), isolate_);
}


void Debug::NotifyMessageHandler(v8::DebugEvent event,
                                 Handle<JSObject> exec_state,
                                 Handle<JSObject> event_data,
                                 bool auto_continue) {
  // Prevent other interrupts from triggering, for example API callbacks,
  // while dispatching message handler callbacks.
  PostponeInterruptsScope no_interrupts(isolate_);
  DCHECK(is_active_);
  HandleScope scope(isolate_);
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
    case v8::NewFunction:
      break;
    default:
      UNREACHABLE();
  }

  // The debug command interrupt flag might have been set when the command was
  // added. It should be enough to clear the flag only once while we are in the
  // debugger.
  DCHECK(in_debug_scope());
  isolate_->stack_guard()->ClearDebugCommand();

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
  if (auto_continue && !has_commands()) return;

  // DebugCommandProcessor goes here.
  bool running = auto_continue;

  Handle<Object> cmd_processor_ctor = Object::GetProperty(
      isolate_, exec_state, "debugCommandProcessor").ToHandleChecked();
  Handle<Object> ctor_args[] = { isolate_->factory()->ToBoolean(running) };
  Handle<Object> cmd_processor = Execution::Call(
      isolate_, cmd_processor_ctor, exec_state, 1, ctor_args).ToHandleChecked();
  Handle<JSFunction> process_debug_request = Handle<JSFunction>::cast(
      Object::GetProperty(
          isolate_, cmd_processor, "processDebugRequest").ToHandleChecked());
  Handle<Object> is_running = Object::GetProperty(
      isolate_, cmd_processor, "isRunning").ToHandleChecked();

  // Process requests from the debugger.
  do {
    // Wait for new command in the queue.
    command_received_.Wait();

    // Get the command from the queue.
    CommandMessage command = command_queue_.Get();
    isolate_->logger()->DebugTag(
        "Got request from command queue, in interactive loop.");
    if (!is_active()) {
      // Delete command text and user data.
      command.Dispose();
      return;
    }

    Vector<const uc16> command_text(
        const_cast<const uc16*>(command.text().start()),
        command.text().length());
    Handle<String> request_text = isolate_->factory()->NewStringFromTwoByte(
        command_text).ToHandleChecked();
    Handle<Object> request_args[] = { request_text };
    Handle<Object> answer_value;
    Handle<String> answer;
    MaybeHandle<Object> maybe_exception;
    MaybeHandle<Object> maybe_result =
        Execution::TryCall(process_debug_request, cmd_processor, 1,
                           request_args, &maybe_exception);

    if (maybe_result.ToHandle(&answer_value)) {
      if (answer_value->IsUndefined()) {
        answer = isolate_->factory()->empty_string();
      } else {
        answer = Handle<String>::cast(answer_value);
      }

      // Log the JSON request/response.
      if (FLAG_trace_debug_json) {
        PrintF("%s\n", request_text->ToCString().get());
        PrintF("%s\n", answer->ToCString().get());
      }

      Handle<Object> is_running_args[] = { answer };
      maybe_result = Execution::Call(
          isolate_, is_running, cmd_processor, 1, is_running_args);
      Handle<Object> result;
      if (!maybe_result.ToHandle(&result)) break;
      running = result->IsTrue();
    } else {
      Handle<Object> exception;
      if (!maybe_exception.ToHandle(&exception)) break;
      Handle<Object> result;
      if (!Execution::ToString(isolate_, exception).ToHandle(&result)) break;
      answer = Handle<String>::cast(result);
    }

    // Return the result.
    MessageImpl message = MessageImpl::NewResponse(
        event, running, exec_state, event_data, answer, command.client_data());
    InvokeMessageHandler(message);
    command.Dispose();

    // Return from debug event processing if either the VM is put into the
    // running state (through a continue command) or auto continue is active
    // and there are no more commands queued.
  } while (!running || has_commands());
  command_queue_.Clear();
}


void Debug::SetEventListener(Handle<Object> callback,
                             Handle<Object> data) {
  GlobalHandles* global_handles = isolate_->global_handles();

  // Remove existing entry.
  GlobalHandles::Destroy(event_listener_.location());
  event_listener_ = Handle<Object>();
  GlobalHandles::Destroy(event_listener_data_.location());
  event_listener_data_ = Handle<Object>();

  // Set new entry.
  if (!callback->IsUndefined() && !callback->IsNull()) {
    event_listener_ = global_handles->Create(*callback);
    if (data.is_null()) data = isolate_->factory()->undefined_value();
    event_listener_data_ = global_handles->Create(*data);
  }

  UpdateState();
}


void Debug::SetMessageHandler(v8::Debug::MessageHandler handler) {
  message_handler_ = handler;
  UpdateState();
  if (handler == NULL && in_debug_scope()) {
    // Send an empty command to the debugger if in a break to make JavaScript
    // run again if the debugger is closed.
    EnqueueCommandMessage(Vector<const uint16_t>::empty());
  }
}



void Debug::UpdateState() {
  is_active_ = message_handler_ != NULL || !event_listener_.is_null();
  if (is_active_ || in_debug_scope()) {
    // Note that the debug context could have already been loaded to
    // bootstrap test cases.
    isolate_->compilation_cache()->Disable();
    is_active_ = Load();
  } else if (is_loaded()) {
    isolate_->compilation_cache()->Enable();
    Unload();
  }
}


// Calls the registered debug message handler. This callback is part of the
// public API.
void Debug::InvokeMessageHandler(MessageImpl message) {
  if (message_handler_ != NULL) message_handler_(message);
}


// Puts a command coming from the public API on the queue.  Creates
// a copy of the command string managed by the debugger.  Up to this
// point, the command data was managed by the API client.  Called
// by the API client thread.
void Debug::EnqueueCommandMessage(Vector<const uint16_t> command,
                                  v8::Debug::ClientData* client_data) {
  // Need to cast away const.
  CommandMessage message = CommandMessage::New(
      Vector<uint16_t>(const_cast<uint16_t*>(command.start()),
                       command.length()),
      client_data);
  isolate_->logger()->DebugTag("Put command on command_queue.");
  command_queue_.Put(message);
  command_received_.Signal();

  // Set the debug command break flag to have the command processed.
  if (!in_debug_scope()) isolate_->stack_guard()->RequestDebugCommand();
}


void Debug::EnqueueDebugCommand(v8::Debug::ClientData* client_data) {
  CommandMessage message = CommandMessage::New(Vector<uint16_t>(), client_data);
  event_command_queue_.Put(message);

  // Set the debug command break flag to have the command processed.
  if (!in_debug_scope()) isolate_->stack_guard()->RequestDebugCommand();
}


MaybeHandle<Object> Debug::Call(Handle<JSFunction> fun, Handle<Object> data) {
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return isolate_->factory()->undefined_value();

  // Create the execution state.
  Handle<Object> exec_state;
  if (!MakeExecutionState().ToHandle(&exec_state)) {
    return isolate_->factory()->undefined_value();
  }

  Handle<Object> argv[] = { exec_state, data };
  return Execution::Call(
      isolate_,
      fun,
      Handle<Object>(debug_context()->global_proxy(), isolate_),
      arraysize(argv),
      argv);
}


void Debug::HandleDebugBreak() {
  // Ignore debug break during bootstrapping.
  if (isolate_->bootstrapper()->IsActive()) return;
  // Just continue if breaks are disabled.
  if (break_disabled()) return;
  // Ignore debug break if debugger is not active.
  if (!is_active()) return;

  StackLimitCheck check(isolate_);
  if (check.HasOverflowed()) return;

  { JavaScriptFrameIterator it(isolate_);
    DCHECK(!it.done());
    Object* fun = it.frame()->function();
    if (fun && fun->IsJSFunction()) {
      // Don't stop in builtin functions.
      if (JSFunction::cast(fun)->IsBuiltin()) return;
      GlobalObject* global = JSFunction::cast(fun)->context()->global_object();
      // Don't stop in debugger functions.
      if (IsDebugGlobal(global)) return;
    }
  }

  // Collect the break state before clearing the flags.
  bool debug_command_only = isolate_->stack_guard()->CheckDebugCommand() &&
                            !isolate_->stack_guard()->CheckDebugBreak();

  isolate_->stack_guard()->ClearDebugBreak();

  ProcessDebugMessages(debug_command_only);
}


void Debug::ProcessDebugMessages(bool debug_command_only) {
  isolate_->stack_guard()->ClearDebugCommand();

  StackLimitCheck check(isolate_);
  if (check.HasOverflowed()) return;

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Notify the debug event listeners. Indicate auto continue if the break was
  // a debug command break.
  OnDebugBreak(isolate_->factory()->undefined_value(), debug_command_only);
}


DebugScope::DebugScope(Debug* debug)
    : debug_(debug),
      prev_(debug->debugger_entry()),
      save_(debug_->isolate_),
      no_termination_exceptons_(debug_->isolate_,
                                StackGuard::TERMINATE_EXECUTION) {
  // Link recursive debugger entry.
  base::NoBarrier_Store(&debug_->thread_local_.current_debug_scope_,
                        reinterpret_cast<base::AtomicWord>(this));

  // Store the previous break id and frame id.
  break_id_ = debug_->break_id();
  break_frame_id_ = debug_->break_frame_id();

  // Create the new break info. If there is no JavaScript frames there is no
  // break frame id.
  JavaScriptFrameIterator it(isolate());
  bool has_js_frames = !it.done();
  debug_->thread_local_.break_frame_id_ = has_js_frames ? it.frame()->id()
                                                        : StackFrame::NO_ID;
  debug_->SetNextBreakId();

  debug_->UpdateState();
  // Make sure that debugger is loaded and enter the debugger context.
  // The previous context is kept in save_.
  failed_ = !debug_->is_loaded();
  if (!failed_) isolate()->set_context(*debug->debug_context());
}



DebugScope::~DebugScope() {
  if (!failed_ && prev_ == NULL) {
    // Clear mirror cache when leaving the debugger. Skip this if there is a
    // pending exception as clearing the mirror cache calls back into
    // JavaScript. This can happen if the v8::Debug::Call is used in which
    // case the exception should end up in the calling code.
    if (!isolate()->has_pending_exception()) debug_->ClearMirrorCache();

    // If there are commands in the queue when leaving the debugger request
    // that these commands are processed.
    if (debug_->has_commands()) isolate()->stack_guard()->RequestDebugCommand();
  }

  // Leaving this debugger entry.
  base::NoBarrier_Store(&debug_->thread_local_.current_debug_scope_,
                        reinterpret_cast<base::AtomicWord>(prev_));

  // Restore to the previous break state.
  debug_->thread_local_.break_frame_id_ = break_frame_id_;
  debug_->thread_local_.break_id_ = break_id_;

  debug_->UpdateState();
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


v8::Isolate* MessageImpl::GetIsolate() const {
  return reinterpret_cast<v8::Isolate*>(exec_state_->GetIsolate());
}


v8::Handle<v8::Object> MessageImpl::GetEventData() const {
  return v8::Utils::ToLocal(event_data_);
}


v8::Handle<v8::String> MessageImpl::GetJSON() const {
  Isolate* isolate = event_data_->GetIsolate();
  v8::EscapableHandleScope scope(reinterpret_cast<v8::Isolate*>(isolate));

  if (IsEvent()) {
    // Call toJSONProtocol on the debug event object.
    Handle<Object> fun = Object::GetProperty(
        isolate, event_data_, "toJSONProtocol").ToHandleChecked();
    if (!fun->IsJSFunction()) {
      return v8::Handle<v8::String>();
    }

    MaybeHandle<Object> maybe_json =
        Execution::TryCall(Handle<JSFunction>::cast(fun), event_data_, 0, NULL);
    Handle<Object> json;
    if (!maybe_json.ToHandle(&json) || !json->IsString()) {
      return v8::Handle<v8::String>();
    }
    return scope.Escape(v8::Utils::ToLocal(Handle<String>::cast(json)));
  } else {
    return v8::Utils::ToLocal(response_json_);
  }
}


v8::Handle<v8::Context> MessageImpl::GetEventContext() const {
  Isolate* isolate = event_data_->GetIsolate();
  v8::Handle<v8::Context> context = GetDebugEventContext(isolate);
  // Isolate::context() may be NULL when "script collected" event occurs.
  DCHECK(!context.IsEmpty());
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
  return GetDebugEventContext(exec_state_->GetIsolate());
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
  while (!IsEmpty()) Get().Dispose();
  DeleteArray(messages_);
}


CommandMessage CommandMessageQueue::Get() {
  DCHECK(!IsEmpty());
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
    : logger_(logger), queue_(size) {}


bool LockingCommandMessageQueue::IsEmpty() const {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  return queue_.IsEmpty();
}


CommandMessage LockingCommandMessageQueue::Get() {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  CommandMessage result = queue_.Get();
  logger_->DebugEvent("Get", result.text());
  return result;
}


void LockingCommandMessageQueue::Put(const CommandMessage& message) {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  queue_.Put(message);
  logger_->DebugEvent("Put", message.text());
}


void LockingCommandMessageQueue::Clear() {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  queue_.Clear();
}

} }  // namespace v8::internal
