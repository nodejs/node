// Copyright 2008 the V8 project authors. All rights reserved.
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
#include "bootstrapper.h"
#include "debug.h"
#include "execution.h"
#include "v8threads.h"
#include "regexp-stack.h"

namespace v8 {

static internal::Thread::LocalStorageKey thread_state_key =
    internal::Thread::CreateThreadLocalKey();
static internal::Thread::LocalStorageKey thread_id_key =
    internal::Thread::CreateThreadLocalKey();


// Track whether this V8 instance has ever called v8::Locker. This allows the
// API code to verify that the lock is always held when V8 is being entered.
bool Locker::active_ = false;


// Constructor for the Locker object.  Once the Locker is constructed the
// current thread will be guaranteed to have the big V8 lock.
Locker::Locker() : has_lock_(false), top_level_(true) {
  // Record that the Locker has been used at least once.
  active_ = true;
  // Get the big lock if necessary.
  if (!internal::ThreadManager::IsLockedByCurrentThread()) {
    internal::ThreadManager::Lock();
    has_lock_ = true;
    // This may be a locker within an unlocker in which case we have to
    // get the saved state for this thread and restore it.
    if (internal::ThreadManager::RestoreThread()) {
      top_level_ = false;
    }
  }
  ASSERT(internal::ThreadManager::IsLockedByCurrentThread());

  // Make sure this thread is assigned a thread id.
  internal::ThreadManager::AssignId();
}


bool Locker::IsLocked() {
  return internal::ThreadManager::IsLockedByCurrentThread();
}


Locker::~Locker() {
  ASSERT(internal::ThreadManager::IsLockedByCurrentThread());
  if (has_lock_) {
    if (!top_level_) {
      internal::ThreadManager::ArchiveThread();
    }
    internal::ThreadManager::Unlock();
  }
}


Unlocker::Unlocker() {
  ASSERT(internal::ThreadManager::IsLockedByCurrentThread());
  internal::ThreadManager::ArchiveThread();
  internal::ThreadManager::Unlock();
}


Unlocker::~Unlocker() {
  ASSERT(!internal::ThreadManager::IsLockedByCurrentThread());
  internal::ThreadManager::Lock();
  internal::ThreadManager::RestoreThread();
}


void Locker::StartPreemption(int every_n_ms) {
  v8::internal::ContextSwitcher::StartPreemption(every_n_ms);
}


void Locker::StopPreemption() {
  v8::internal::ContextSwitcher::StopPreemption();
}


namespace internal {


bool ThreadManager::RestoreThread() {
  // First check whether the current thread has been 'lazily archived', ie
  // not archived at all.  If that is the case we put the state storage we
  // had prepared back in the free list, since we didn't need it after all.
  if (lazily_archived_thread_.IsSelf()) {
    lazily_archived_thread_.Initialize(ThreadHandle::INVALID);
    ASSERT(Thread::GetThreadLocal(thread_state_key) ==
           lazily_archived_thread_state_);
    lazily_archived_thread_state_->set_id(kInvalidId);
    lazily_archived_thread_state_->LinkInto(ThreadState::FREE_LIST);
    lazily_archived_thread_state_ = NULL;
    Thread::SetThreadLocal(thread_state_key, NULL);
    return true;
  }

  // Make sure that the preemption thread cannot modify the thread state while
  // it is being archived or restored.
  ExecutionAccess access;

  // If there is another thread that was lazily archived then we have to really
  // archive it now.
  if (lazily_archived_thread_.IsValid()) {
    EagerlyArchiveThread();
  }
  ThreadState* state =
      reinterpret_cast<ThreadState*>(Thread::GetThreadLocal(thread_state_key));
  if (state == NULL) {
    return false;
  }
  char* from = state->data();
  from = HandleScopeImplementer::RestoreThread(from);
  from = Top::RestoreThread(from);
#ifdef ENABLE_DEBUGGER_SUPPORT
  from = Debug::RestoreDebug(from);
#endif
  from = StackGuard::RestoreStackGuard(from);
  from = RegExpStack::RestoreStack(from);
  from = Bootstrapper::RestoreState(from);
  Thread::SetThreadLocal(thread_state_key, NULL);
  if (state->terminate_on_restore()) {
    StackGuard::TerminateExecution();
    state->set_terminate_on_restore(false);
  }
  state->set_id(kInvalidId);
  state->Unlink();
  state->LinkInto(ThreadState::FREE_LIST);
  return true;
}


void ThreadManager::Lock() {
  mutex_->Lock();
  mutex_owner_.Initialize(ThreadHandle::SELF);
  ASSERT(IsLockedByCurrentThread());
}


void ThreadManager::Unlock() {
  mutex_owner_.Initialize(ThreadHandle::INVALID);
  mutex_->Unlock();
}


static int ArchiveSpacePerThread() {
  return HandleScopeImplementer::ArchiveSpacePerThread() +
                            Top::ArchiveSpacePerThread() +
#ifdef ENABLE_DEBUGGER_SUPPORT
                          Debug::ArchiveSpacePerThread() +
#endif
                     StackGuard::ArchiveSpacePerThread() +
                    RegExpStack::ArchiveSpacePerThread() +
                   Bootstrapper::ArchiveSpacePerThread();
}


ThreadState* ThreadState::free_anchor_ = new ThreadState();
ThreadState* ThreadState::in_use_anchor_ = new ThreadState();


ThreadState::ThreadState() : id_(ThreadManager::kInvalidId),
                             terminate_on_restore_(false),
                             next_(this), previous_(this) {
}


void ThreadState::AllocateSpace() {
  data_ = NewArray<char>(ArchiveSpacePerThread());
}


void ThreadState::Unlink() {
  next_->previous_ = previous_;
  previous_->next_ = next_;
}


void ThreadState::LinkInto(List list) {
  ThreadState* flying_anchor =
      list == FREE_LIST ? free_anchor_ : in_use_anchor_;
  next_ = flying_anchor->next_;
  previous_ = flying_anchor;
  flying_anchor->next_ = this;
  next_->previous_ = this;
}


ThreadState* ThreadState::GetFree() {
  ThreadState* gotten = free_anchor_->next_;
  if (gotten == free_anchor_) {
    ThreadState* new_thread_state = new ThreadState();
    new_thread_state->AllocateSpace();
    return new_thread_state;
  }
  return gotten;
}


// Gets the first in the list of archived threads.
ThreadState* ThreadState::FirstInUse() {
  return in_use_anchor_->Next();
}


ThreadState* ThreadState::Next() {
  if (next_ == in_use_anchor_) return NULL;
  return next_;
}


int ThreadManager::next_id_ = 0;
Mutex* ThreadManager::mutex_ = OS::CreateMutex();
ThreadHandle ThreadManager::mutex_owner_(ThreadHandle::INVALID);
ThreadHandle ThreadManager::lazily_archived_thread_(ThreadHandle::INVALID);
ThreadState* ThreadManager::lazily_archived_thread_state_ = NULL;


void ThreadManager::ArchiveThread() {
  ASSERT(!lazily_archived_thread_.IsValid());
  ASSERT(Thread::GetThreadLocal(thread_state_key) == NULL);
  ThreadState* state = ThreadState::GetFree();
  state->Unlink();
  Thread::SetThreadLocal(thread_state_key, reinterpret_cast<void*>(state));
  lazily_archived_thread_.Initialize(ThreadHandle::SELF);
  lazily_archived_thread_state_ = state;
  ASSERT(state->id() == kInvalidId);
  state->set_id(CurrentId());
  ASSERT(state->id() != kInvalidId);
}


void ThreadManager::EagerlyArchiveThread() {
  ThreadState* state = lazily_archived_thread_state_;
  state->LinkInto(ThreadState::IN_USE_LIST);
  char* to = state->data();
  // Ensure that data containing GC roots are archived first, and handle them
  // in ThreadManager::Iterate(ObjectVisitor*).
  to = HandleScopeImplementer::ArchiveThread(to);
  to = Top::ArchiveThread(to);
#ifdef ENABLE_DEBUGGER_SUPPORT
  to = Debug::ArchiveDebug(to);
#endif
  to = StackGuard::ArchiveStackGuard(to);
  to = RegExpStack::ArchiveStack(to);
  to = Bootstrapper::ArchiveState(to);
  lazily_archived_thread_.Initialize(ThreadHandle::INVALID);
  lazily_archived_thread_state_ = NULL;
}


void ThreadManager::Iterate(ObjectVisitor* v) {
  // Expecting no threads during serialization/deserialization
  for (ThreadState* state = ThreadState::FirstInUse();
       state != NULL;
       state = state->Next()) {
    char* data = state->data();
    data = HandleScopeImplementer::Iterate(v, data);
    data = Top::Iterate(v, data);
  }
}


void ThreadManager::MarkCompactPrologue(bool is_compacting) {
  for (ThreadState* state = ThreadState::FirstInUse();
       state != NULL;
       state = state->Next()) {
    char* data = state->data();
    data += HandleScopeImplementer::ArchiveSpacePerThread();
    Top::MarkCompactPrologue(is_compacting, data);
  }
}


void ThreadManager::MarkCompactEpilogue(bool is_compacting) {
  for (ThreadState* state = ThreadState::FirstInUse();
       state != NULL;
       state = state->Next()) {
    char* data = state->data();
    data += HandleScopeImplementer::ArchiveSpacePerThread();
    Top::MarkCompactEpilogue(is_compacting, data);
  }
}


int ThreadManager::CurrentId() {
  return Thread::GetThreadLocalInt(thread_id_key);
}


void ThreadManager::AssignId() {
  if (!Thread::HasThreadLocal(thread_id_key)) {
    ASSERT(Locker::IsLocked());
    int thread_id = next_id_++;
    Thread::SetThreadLocalInt(thread_id_key, thread_id);
    Top::set_thread_id(thread_id);
  }
}


void ThreadManager::TerminateExecution(int thread_id) {
  for (ThreadState* state = ThreadState::FirstInUse();
       state != NULL;
       state = state->Next()) {
    if (thread_id == state->id()) {
      state->set_terminate_on_restore(true);
    }
  }
}


// This is the ContextSwitcher singleton. There is at most a single thread
// running which delivers preemption events to V8 threads.
ContextSwitcher* ContextSwitcher::singleton_ = NULL;


ContextSwitcher::ContextSwitcher(int every_n_ms)
  : keep_going_(true),
    sleep_ms_(every_n_ms) {
}


// Set the scheduling interval of V8 threads. This function starts the
// ContextSwitcher thread if needed.
void ContextSwitcher::StartPreemption(int every_n_ms) {
  ASSERT(Locker::IsLocked());
  if (singleton_ == NULL) {
    // If the ContextSwitcher thread is not running at the moment start it now.
    singleton_ = new ContextSwitcher(every_n_ms);
    singleton_->Start();
  } else {
    // ContextSwitcher thread is already running, so we just change the
    // scheduling interval.
    singleton_->sleep_ms_ = every_n_ms;
  }
}


// Disable preemption of V8 threads. If multiple threads want to use V8 they
// must cooperatively schedule amongst them from this point on.
void ContextSwitcher::StopPreemption() {
  ASSERT(Locker::IsLocked());
  if (singleton_ != NULL) {
    // The ContextSwitcher thread is running. We need to stop it and release
    // its resources.
    singleton_->keep_going_ = false;
    singleton_->Join();  // Wait for the ContextSwitcher thread to exit.
    // Thread has exited, now we can delete it.
    delete(singleton_);
    singleton_ = NULL;
  }
}


// Main loop of the ContextSwitcher thread: Preempt the currently running V8
// thread at regular intervals.
void ContextSwitcher::Run() {
  while (keep_going_) {
    OS::Sleep(sleep_ms_);
    StackGuard::Preempt();
  }
}


// Acknowledge the preemption by the receiving thread.
void ContextSwitcher::PreemptionReceived() {
  ASSERT(Locker::IsLocked());
  // There is currently no accounting being done for this. But could be in the
  // future, which is why we leave this in.
}


}  // namespace internal
}  // namespace v8
