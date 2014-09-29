// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/api.h"
#include "src/bootstrapper.h"
#include "src/debug.h"
#include "src/execution.h"
#include "src/regexp-stack.h"
#include "src/v8threads.h"

namespace v8 {


// Track whether this V8 instance has ever called v8::Locker. This allows the
// API code to verify that the lock is always held when V8 is being entered.
bool Locker::active_ = false;


// Once the Locker is initialized, the current thread will be guaranteed to have
// the lock for a given isolate.
void Locker::Initialize(v8::Isolate* isolate) {
  DCHECK(isolate != NULL);
  has_lock_= false;
  top_level_ = true;
  isolate_ = reinterpret_cast<i::Isolate*>(isolate);
  // Record that the Locker has been used at least once.
  active_ = true;
  // Get the big lock if necessary.
  if (!isolate_->thread_manager()->IsLockedByCurrentThread()) {
    isolate_->thread_manager()->Lock();
    has_lock_ = true;

    // Make sure that V8 is initialized.  Archiving of threads interferes
    // with deserialization by adding additional root pointers, so we must
    // initialize here, before anyone can call ~Locker() or Unlocker().
    if (!isolate_->IsInitialized()) {
      isolate_->Enter();
      V8::Initialize();
      isolate_->Exit();
    }

    // This may be a locker within an unlocker in which case we have to
    // get the saved state for this thread and restore it.
    if (isolate_->thread_manager()->RestoreThread()) {
      top_level_ = false;
    } else {
      internal::ExecutionAccess access(isolate_);
      isolate_->stack_guard()->ClearThread(access);
      isolate_->stack_guard()->InitThread(access);
    }
  }
  DCHECK(isolate_->thread_manager()->IsLockedByCurrentThread());
}


bool Locker::IsLocked(v8::Isolate* isolate) {
  DCHECK(isolate != NULL);
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  return internal_isolate->thread_manager()->IsLockedByCurrentThread();
}


bool Locker::IsActive() {
  return active_;
}


Locker::~Locker() {
  DCHECK(isolate_->thread_manager()->IsLockedByCurrentThread());
  if (has_lock_) {
    if (top_level_) {
      isolate_->thread_manager()->FreeThreadResources();
    } else {
      isolate_->thread_manager()->ArchiveThread();
    }
    isolate_->thread_manager()->Unlock();
  }
}


void Unlocker::Initialize(v8::Isolate* isolate) {
  DCHECK(isolate != NULL);
  isolate_ = reinterpret_cast<i::Isolate*>(isolate);
  DCHECK(isolate_->thread_manager()->IsLockedByCurrentThread());
  isolate_->thread_manager()->ArchiveThread();
  isolate_->thread_manager()->Unlock();
}


Unlocker::~Unlocker() {
  DCHECK(!isolate_->thread_manager()->IsLockedByCurrentThread());
  isolate_->thread_manager()->Lock();
  isolate_->thread_manager()->RestoreThread();
}


namespace internal {


bool ThreadManager::RestoreThread() {
  DCHECK(IsLockedByCurrentThread());
  // First check whether the current thread has been 'lazily archived', i.e.
  // not archived at all.  If that is the case we put the state storage we
  // had prepared back in the free list, since we didn't need it after all.
  if (lazily_archived_thread_.Equals(ThreadId::Current())) {
    lazily_archived_thread_ = ThreadId::Invalid();
    Isolate::PerIsolateThreadData* per_thread =
        isolate_->FindPerThreadDataForThisThread();
    DCHECK(per_thread != NULL);
    DCHECK(per_thread->thread_state() == lazily_archived_thread_state_);
    lazily_archived_thread_state_->set_id(ThreadId::Invalid());
    lazily_archived_thread_state_->LinkInto(ThreadState::FREE_LIST);
    lazily_archived_thread_state_ = NULL;
    per_thread->set_thread_state(NULL);
    return true;
  }

  // Make sure that the preemption thread cannot modify the thread state while
  // it is being archived or restored.
  ExecutionAccess access(isolate_);

  // If there is another thread that was lazily archived then we have to really
  // archive it now.
  if (lazily_archived_thread_.IsValid()) {
    EagerlyArchiveThread();
  }
  Isolate::PerIsolateThreadData* per_thread =
      isolate_->FindPerThreadDataForThisThread();
  if (per_thread == NULL || per_thread->thread_state() == NULL) {
    // This is a new thread.
    isolate_->stack_guard()->InitThread(access);
    return false;
  }
  ThreadState* state = per_thread->thread_state();
  char* from = state->data();
  from = isolate_->handle_scope_implementer()->RestoreThread(from);
  from = isolate_->RestoreThread(from);
  from = Relocatable::RestoreState(isolate_, from);
  from = isolate_->debug()->RestoreDebug(from);
  from = isolate_->stack_guard()->RestoreStackGuard(from);
  from = isolate_->regexp_stack()->RestoreStack(from);
  from = isolate_->bootstrapper()->RestoreState(from);
  per_thread->set_thread_state(NULL);
  if (state->terminate_on_restore()) {
    isolate_->stack_guard()->RequestTerminateExecution();
    state->set_terminate_on_restore(false);
  }
  state->set_id(ThreadId::Invalid());
  state->Unlink();
  state->LinkInto(ThreadState::FREE_LIST);
  return true;
}


void ThreadManager::Lock() {
  mutex_.Lock();
  mutex_owner_ = ThreadId::Current();
  DCHECK(IsLockedByCurrentThread());
}


void ThreadManager::Unlock() {
  mutex_owner_ = ThreadId::Invalid();
  mutex_.Unlock();
}


static int ArchiveSpacePerThread() {
  return HandleScopeImplementer::ArchiveSpacePerThread() +
                        Isolate::ArchiveSpacePerThread() +
                          Debug::ArchiveSpacePerThread() +
                     StackGuard::ArchiveSpacePerThread() +
                    RegExpStack::ArchiveSpacePerThread() +
                   Bootstrapper::ArchiveSpacePerThread() +
                    Relocatable::ArchiveSpacePerThread();
}


ThreadState::ThreadState(ThreadManager* thread_manager)
    : id_(ThreadId::Invalid()),
      terminate_on_restore_(false),
      data_(NULL),
      next_(this),
      previous_(this),
      thread_manager_(thread_manager) {
}


ThreadState::~ThreadState() {
  DeleteArray<char>(data_);
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
      list == FREE_LIST ? thread_manager_->free_anchor_
                        : thread_manager_->in_use_anchor_;
  next_ = flying_anchor->next_;
  previous_ = flying_anchor;
  flying_anchor->next_ = this;
  next_->previous_ = this;
}


ThreadState* ThreadManager::GetFreeThreadState() {
  ThreadState* gotten = free_anchor_->next_;
  if (gotten == free_anchor_) {
    ThreadState* new_thread_state = new ThreadState(this);
    new_thread_state->AllocateSpace();
    return new_thread_state;
  }
  return gotten;
}


// Gets the first in the list of archived threads.
ThreadState* ThreadManager::FirstThreadStateInUse() {
  return in_use_anchor_->Next();
}


ThreadState* ThreadState::Next() {
  if (next_ == thread_manager_->in_use_anchor_) return NULL;
  return next_;
}


// Thread ids must start with 1, because in TLS having thread id 0 can't
// be distinguished from not having a thread id at all (since NULL is
// defined as 0.)
ThreadManager::ThreadManager()
    : mutex_owner_(ThreadId::Invalid()),
      lazily_archived_thread_(ThreadId::Invalid()),
      lazily_archived_thread_state_(NULL),
      free_anchor_(NULL),
      in_use_anchor_(NULL) {
  free_anchor_ = new ThreadState(this);
  in_use_anchor_ = new ThreadState(this);
}


ThreadManager::~ThreadManager() {
  DeleteThreadStateList(free_anchor_);
  DeleteThreadStateList(in_use_anchor_);
}


void ThreadManager::DeleteThreadStateList(ThreadState* anchor) {
  // The list starts and ends with the anchor.
  for (ThreadState* current = anchor->next_; current != anchor;) {
    ThreadState* next = current->next_;
    delete current;
    current = next;
  }
  delete anchor;
}


void ThreadManager::ArchiveThread() {
  DCHECK(lazily_archived_thread_.Equals(ThreadId::Invalid()));
  DCHECK(!IsArchived());
  DCHECK(IsLockedByCurrentThread());
  ThreadState* state = GetFreeThreadState();
  state->Unlink();
  Isolate::PerIsolateThreadData* per_thread =
      isolate_->FindOrAllocatePerThreadDataForThisThread();
  per_thread->set_thread_state(state);
  lazily_archived_thread_ = ThreadId::Current();
  lazily_archived_thread_state_ = state;
  DCHECK(state->id().Equals(ThreadId::Invalid()));
  state->set_id(CurrentId());
  DCHECK(!state->id().Equals(ThreadId::Invalid()));
}


void ThreadManager::EagerlyArchiveThread() {
  DCHECK(IsLockedByCurrentThread());
  ThreadState* state = lazily_archived_thread_state_;
  state->LinkInto(ThreadState::IN_USE_LIST);
  char* to = state->data();
  // Ensure that data containing GC roots are archived first, and handle them
  // in ThreadManager::Iterate(ObjectVisitor*).
  to = isolate_->handle_scope_implementer()->ArchiveThread(to);
  to = isolate_->ArchiveThread(to);
  to = Relocatable::ArchiveState(isolate_, to);
  to = isolate_->debug()->ArchiveDebug(to);
  to = isolate_->stack_guard()->ArchiveStackGuard(to);
  to = isolate_->regexp_stack()->ArchiveStack(to);
  to = isolate_->bootstrapper()->ArchiveState(to);
  lazily_archived_thread_ = ThreadId::Invalid();
  lazily_archived_thread_state_ = NULL;
}


void ThreadManager::FreeThreadResources() {
  isolate_->handle_scope_implementer()->FreeThreadResources();
  isolate_->FreeThreadResources();
  isolate_->debug()->FreeThreadResources();
  isolate_->stack_guard()->FreeThreadResources();
  isolate_->regexp_stack()->FreeThreadResources();
  isolate_->bootstrapper()->FreeThreadResources();
}


bool ThreadManager::IsArchived() {
  Isolate::PerIsolateThreadData* data =
      isolate_->FindPerThreadDataForThisThread();
  return data != NULL && data->thread_state() != NULL;
}


void ThreadManager::Iterate(ObjectVisitor* v) {
  // Expecting no threads during serialization/deserialization
  for (ThreadState* state = FirstThreadStateInUse();
       state != NULL;
       state = state->Next()) {
    char* data = state->data();
    data = HandleScopeImplementer::Iterate(v, data);
    data = isolate_->Iterate(v, data);
    data = Relocatable::Iterate(v, data);
  }
}


void ThreadManager::IterateArchivedThreads(ThreadVisitor* v) {
  for (ThreadState* state = FirstThreadStateInUse();
       state != NULL;
       state = state->Next()) {
    char* data = state->data();
    data += HandleScopeImplementer::ArchiveSpacePerThread();
    isolate_->IterateThread(v, data);
  }
}


ThreadId ThreadManager::CurrentId() {
  return ThreadId::Current();
}


void ThreadManager::TerminateExecution(ThreadId thread_id) {
  for (ThreadState* state = FirstThreadStateInUse();
       state != NULL;
       state = state->Next()) {
    if (thread_id.Equals(state->id())) {
      state->set_terminate_on_restore(true);
    }
  }
}


}  // namespace internal
}  // namespace v8
