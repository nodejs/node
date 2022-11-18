// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/safepoint.h"

#include <atomic>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/handles/local-handles.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/logging/counters-scopes.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

IsolateSafepoint::IsolateSafepoint(Heap* heap)
    : heap_(heap), local_heaps_head_(nullptr), active_safepoint_scopes_(0) {}

void IsolateSafepoint::EnterLocalSafepointScope() {
  // Safepoints need to be initiated on some main thread.
  DCHECK_NULL(LocalHeap::Current());
  DCHECK(AllowGarbageCollection::IsAllowed());

  LockMutex(isolate()->main_thread_local_heap());
  if (++active_safepoint_scopes_ > 1) return;

  // Local safepoint can only be initiated on the isolate's main thread.
  DCHECK_EQ(ThreadId::Current(), isolate()->thread_id());

  TimedHistogramScope timer(isolate()->counters()->gc_time_to_safepoint());
  TRACE_GC(heap_->tracer(), GCTracer::Scope::TIME_TO_SAFEPOINT);

  barrier_.Arm();
  size_t running = SetSafepointRequestedFlags(IncludeMainThread::kNo);
  barrier_.WaitUntilRunningThreadsInSafepoint(running);
}

class PerClientSafepointData final {
 public:
  explicit PerClientSafepointData(Isolate* isolate) : isolate_(isolate) {}

  void set_locked_and_running(size_t running) {
    locked_ = true;
    running_ = running;
  }

  IsolateSafepoint* safepoint() const { return heap()->safepoint(); }
  Heap* heap() const { return isolate_->heap(); }
  Isolate* isolate() const { return isolate_; }

  bool is_locked() const { return locked_; }
  size_t running() const { return running_; }

 private:
  Isolate* const isolate_;
  size_t running_ = 0;
  bool locked_ = false;
};

void IsolateSafepoint::InitiateGlobalSafepointScope(
    Isolate* initiator, PerClientSafepointData* client_data) {
  shared_heap_isolate()->global_safepoint()->AssertActive();
  IgnoreLocalGCRequests ignore_gc_requests(initiator->heap());
  LockMutex(initiator->main_thread_local_heap());
  InitiateGlobalSafepointScopeRaw(initiator, client_data);
}

void IsolateSafepoint::TryInitiateGlobalSafepointScope(
    Isolate* initiator, PerClientSafepointData* client_data) {
  shared_heap_isolate()->global_safepoint()->AssertActive();
  if (!local_heaps_mutex_.TryLock()) return;
  InitiateGlobalSafepointScopeRaw(initiator, client_data);
}

class GlobalSafepointInterruptTask : public CancelableTask {
 public:
  explicit GlobalSafepointInterruptTask(Heap* heap)
      : CancelableTask(heap->isolate()), heap_(heap) {}

  ~GlobalSafepointInterruptTask() override = default;
  GlobalSafepointInterruptTask(const GlobalSafepointInterruptTask&) = delete;
  GlobalSafepointInterruptTask& operator=(const GlobalSafepointInterruptTask&) =
      delete;

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override { heap_->main_thread_local_heap()->Safepoint(); }

  Heap* heap_;
};

void IsolateSafepoint::InitiateGlobalSafepointScopeRaw(
    Isolate* initiator, PerClientSafepointData* client_data) {
  CHECK_EQ(++active_safepoint_scopes_, 1);
  barrier_.Arm();

  size_t running =
      SetSafepointRequestedFlags(ShouldIncludeMainThread(initiator));
  client_data->set_locked_and_running(running);

  if (isolate() != initiator) {
    // An isolate might be waiting in the event loop. Post a task in order to
    // wake it up.
    V8::GetCurrentPlatform()
        ->GetForegroundTaskRunner(reinterpret_cast<v8::Isolate*>(isolate()))
        ->PostTask(std::make_unique<GlobalSafepointInterruptTask>(heap_));

    // Request an interrupt in case of long-running code.
    isolate()->stack_guard()->RequestGlobalSafepoint();
  }
}

IsolateSafepoint::IncludeMainThread IsolateSafepoint::ShouldIncludeMainThread(
    Isolate* initiator) {
  const bool is_initiator = isolate() == initiator;
  return is_initiator ? IncludeMainThread::kNo : IncludeMainThread::kYes;
}

size_t IsolateSafepoint::SetSafepointRequestedFlags(
    IncludeMainThread include_main_thread) {
  size_t running = 0;

  // There needs to be at least one LocalHeap for the main thread.
  DCHECK_NOT_NULL(local_heaps_head_);

  for (LocalHeap* local_heap = local_heaps_head_; local_heap;
       local_heap = local_heap->next_) {
    if (local_heap->is_main_thread() &&
        include_main_thread == IncludeMainThread::kNo) {
      continue;
    }

    const LocalHeap::ThreadState old_state =
        local_heap->state_.SetSafepointRequested();

    if (old_state.IsRunning()) running++;
    CHECK_IMPLIES(old_state.IsCollectionRequested(),
                  local_heap->is_main_thread());
    CHECK(!old_state.IsSafepointRequested());
  }

  return running;
}

void IsolateSafepoint::LockMutex(LocalHeap* local_heap) {
  if (!local_heaps_mutex_.TryLock()) {
    ParkedScope parked_scope(local_heap);
    local_heaps_mutex_.Lock();
  }
}

void IsolateSafepoint::LeaveGlobalSafepointScope(Isolate* initiator) {
  local_heaps_mutex_.AssertHeld();
  CHECK_EQ(--active_safepoint_scopes_, 0);
  ClearSafepointRequestedFlags(ShouldIncludeMainThread(initiator));
  barrier_.Disarm();
  local_heaps_mutex_.Unlock();
}

void IsolateSafepoint::LeaveLocalSafepointScope() {
  local_heaps_mutex_.AssertHeld();
  DCHECK_GT(active_safepoint_scopes_, 0);

  if (--active_safepoint_scopes_ == 0) {
    ClearSafepointRequestedFlags(IncludeMainThread::kNo);
    barrier_.Disarm();
  }

  local_heaps_mutex_.Unlock();
}

void IsolateSafepoint::ClearSafepointRequestedFlags(
    IncludeMainThread include_main_thread) {
  for (LocalHeap* local_heap = local_heaps_head_; local_heap;
       local_heap = local_heap->next_) {
    if (local_heap->is_main_thread() &&
        include_main_thread == IncludeMainThread::kNo) {
      continue;
    }

    const LocalHeap::ThreadState old_state =
        local_heap->state_.ClearSafepointRequested();

    CHECK(old_state.IsParked());
    CHECK(old_state.IsSafepointRequested());
    CHECK_IMPLIES(old_state.IsCollectionRequested(),
                  local_heap->is_main_thread());
  }
}

void IsolateSafepoint::WaitInSafepoint() { barrier_.WaitInSafepoint(); }

void IsolateSafepoint::WaitInUnpark() { barrier_.WaitInUnpark(); }

void IsolateSafepoint::NotifyPark() { barrier_.NotifyPark(); }

void IsolateSafepoint::WaitUntilRunningThreadsInSafepoint(
    const PerClientSafepointData* client_data) {
  barrier_.WaitUntilRunningThreadsInSafepoint(client_data->running());
}

void IsolateSafepoint::Barrier::Arm() {
  base::MutexGuard guard(&mutex_);
  DCHECK(!IsArmed());
  armed_ = true;
  stopped_ = 0;
}

void IsolateSafepoint::Barrier::Disarm() {
  base::MutexGuard guard(&mutex_);
  DCHECK(IsArmed());
  armed_ = false;
  stopped_ = 0;
  cv_resume_.NotifyAll();
}

void IsolateSafepoint::Barrier::WaitUntilRunningThreadsInSafepoint(
    size_t running) {
  base::MutexGuard guard(&mutex_);
  DCHECK(IsArmed());
  while (stopped_ < running) {
    cv_stopped_.Wait(&mutex_);
  }
  DCHECK_EQ(stopped_, running);
}

void IsolateSafepoint::Barrier::NotifyPark() {
  base::MutexGuard guard(&mutex_);
  CHECK(IsArmed());
  stopped_++;
  cv_stopped_.NotifyOne();
}

void IsolateSafepoint::Barrier::WaitInSafepoint() {
  base::MutexGuard guard(&mutex_);
  CHECK(IsArmed());
  stopped_++;
  cv_stopped_.NotifyOne();

  while (IsArmed()) {
    cv_resume_.Wait(&mutex_);
  }
}

void IsolateSafepoint::Barrier::WaitInUnpark() {
  base::MutexGuard guard(&mutex_);

  while (IsArmed()) {
    cv_resume_.Wait(&mutex_);
  }
}

void IsolateSafepoint::Iterate(RootVisitor* visitor) {
  AssertActive();
  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    current->handles()->Iterate(visitor);
  }
}

void IsolateSafepoint::AssertMainThreadIsOnlyThread() {
  DCHECK_EQ(local_heaps_head_, heap_->main_thread_local_heap());
  DCHECK_NULL(heap_->main_thread_local_heap()->next_);
}

Isolate* IsolateSafepoint::isolate() const { return heap_->isolate(); }

Isolate* IsolateSafepoint::shared_heap_isolate() const {
  return isolate()->shared_heap_isolate();
}

SafepointScope::SafepointScope(Heap* heap) : safepoint_(heap->safepoint()) {
  safepoint_->EnterLocalSafepointScope();
}

SafepointScope::~SafepointScope() { safepoint_->LeaveLocalSafepointScope(); }

GlobalSafepoint::GlobalSafepoint(Isolate* isolate)
    : shared_heap_isolate_(isolate) {}

void GlobalSafepoint::AppendClient(Isolate* client) {
  clients_mutex_.AssertHeld();

  DCHECK_NULL(client->global_safepoint_prev_client_isolate_);
  DCHECK_NULL(client->global_safepoint_next_client_isolate_);
  DCHECK_NE(clients_head_, client);

  if (clients_head_) {
    clients_head_->global_safepoint_prev_client_isolate_ = client;
  }

  client->global_safepoint_prev_client_isolate_ = nullptr;
  client->global_safepoint_next_client_isolate_ = clients_head_;

  clients_head_ = client;
}

void GlobalSafepoint::RemoveClient(Isolate* client) {
  DCHECK_EQ(client->heap()->gc_state(), Heap::TEAR_DOWN);

  // A shared heap may have already acquired the client mutex to perform a
  // shared GC. We need to park the Isolate here to allow for a shared GC.
  IgnoreLocalGCRequests ignore_gc_requests(client->heap());
  ParkedMutexGuard guard(client->main_thread_local_heap(), &clients_mutex_);

  if (client->global_safepoint_next_client_isolate_) {
    client->global_safepoint_next_client_isolate_
        ->global_safepoint_prev_client_isolate_ =
        client->global_safepoint_prev_client_isolate_;
  }

  if (client->global_safepoint_prev_client_isolate_) {
    client->global_safepoint_prev_client_isolate_
        ->global_safepoint_next_client_isolate_ =
        client->global_safepoint_next_client_isolate_;
  } else {
    DCHECK_EQ(clients_head_, client);
    clients_head_ = client->global_safepoint_next_client_isolate_;
  }

  client->shared_isolate_ = nullptr;
}

void GlobalSafepoint::AssertNoClientsOnTearDown() {
  DCHECK_WITH_MSG(
      clients_head_ == nullptr,
      "Shared heap must not have clients at teardown. The first isolate that "
      "is created (in a process that has no isolates) owns the lifetime of the "
      "shared heap and is considered the main isolate. The main isolate must "
      "outlive all other isolates.");
}

void GlobalSafepoint::EnterGlobalSafepointScope(Isolate* initiator) {
  // Safepoints need to be initiated on some main thread.
  DCHECK_NULL(LocalHeap::Current());

  if (!clients_mutex_.TryLock()) {
    IgnoreLocalGCRequests ignore_gc_requests(initiator->heap());
    ParkedScope parked_scope(initiator->main_thread_local_heap());
    clients_mutex_.Lock();
  }

  TimedHistogramScope timer(
      initiator->counters()->gc_time_to_global_safepoint());
  TRACE_GC(initiator->heap()->tracer(),
           GCTracer::Scope::TIME_TO_GLOBAL_SAFEPOINT);

  std::vector<PerClientSafepointData> clients;

  // Try to initiate safepoint for all clients. Fail immediately when the
  // local_heaps_mutex_ can't be locked without blocking.
  IterateClientIsolates([&clients, initiator](Isolate* client) {
    clients.emplace_back(client);
    client->heap()->safepoint()->TryInitiateGlobalSafepointScope(
        initiator, &clients.back());
  });

  if (shared_heap_isolate_->is_shared()) {
    // Make it possible to use AssertActive() on shared isolates.
    CHECK(shared_heap_isolate_->heap()
              ->safepoint()
              ->local_heaps_mutex_.TryLock());

    // Shared isolates should never have multiple threads.
    shared_heap_isolate_->heap()->safepoint()->AssertMainThreadIsOnlyThread();
  }

  // Iterate all clients again to initiate the safepoint for all of them - even
  // if that means blocking.
  for (PerClientSafepointData& client : clients) {
    if (client.is_locked()) continue;
    client.safepoint()->InitiateGlobalSafepointScope(initiator, &client);
  }

#if DEBUG
  for (const PerClientSafepointData& client : clients) {
    DCHECK_EQ(client.isolate()->shared_heap_isolate(), shared_heap_isolate_);
    DCHECK(client.heap()->deserialization_complete());
  }
#endif  // DEBUG

  // Now that safepoints were initiated for all clients, wait until all threads
  // of all clients reached a safepoint.
  for (const PerClientSafepointData& client : clients) {
    DCHECK(client.is_locked());
    client.safepoint()->WaitUntilRunningThreadsInSafepoint(&client);
  }
}

void GlobalSafepoint::LeaveGlobalSafepointScope(Isolate* initiator) {
  if (shared_heap_isolate_->is_shared()) {
    shared_heap_isolate_->heap()->safepoint()->local_heaps_mutex_.Unlock();
  }

  IterateClientIsolates([initiator](Isolate* client) {
    Heap* client_heap = client->heap();
    client_heap->safepoint()->LeaveGlobalSafepointScope(initiator);
  });

  clients_mutex_.Unlock();
}

GlobalSafepointScope::GlobalSafepointScope(Isolate* initiator)
    : initiator_(initiator),
      shared_heap_isolate_(initiator->has_shared_heap()
                               ? initiator->shared_heap_isolate()
                               : nullptr) {
  if (shared_heap_isolate_) {
    shared_heap_isolate_->global_safepoint()->EnterGlobalSafepointScope(
        initiator_);
  } else {
    initiator_->heap()->safepoint()->EnterLocalSafepointScope();
  }
}

GlobalSafepointScope::~GlobalSafepointScope() {
  if (shared_heap_isolate_) {
    shared_heap_isolate_->global_safepoint()->LeaveGlobalSafepointScope(
        initiator_);
  } else {
    initiator_->heap()->safepoint()->LeaveLocalSafepointScope();
  }
}

}  // namespace internal
}  // namespace v8
