// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/simulator-base.h"

#include "src/assembler.h"
#include "src/isolate.h"
#include "src/simulator.h"

#if defined(USE_SIMULATOR)

namespace v8 {
namespace internal {

// static
base::Mutex* SimulatorBase::redirection_mutex_ = nullptr;

// static
Redirection* SimulatorBase::redirection_ = nullptr;

// static
void SimulatorBase::InitializeOncePerProcess() {
  DCHECK_NULL(redirection_mutex_);
  redirection_mutex_ = new base::Mutex();
}

// static
void SimulatorBase::GlobalTearDown() {
  delete redirection_mutex_;
  redirection_mutex_ = nullptr;

  Redirection::DeleteChain(redirection_);
  redirection_ = nullptr;
}

// static
void SimulatorBase::Initialize(Isolate* isolate) {
  ExternalReference::set_redirector(isolate, &RedirectExternalReference);
}

// static
void SimulatorBase::TearDown(base::CustomMatcherHashMap* i_cache) {
  if (i_cache != nullptr) {
    for (base::HashMap::Entry* entry = i_cache->Start(); entry != nullptr;
         entry = i_cache->Next(entry)) {
      delete static_cast<CachePage*>(entry->value);
    }
    delete i_cache;
  }
}

// static
void* SimulatorBase::RedirectExternalReference(Isolate* isolate,
                                               void* external_function,
                                               ExternalReference::Type type) {
  base::LockGuard<base::Mutex> lock_guard(Simulator::redirection_mutex());
  Redirection* redirection = Redirection::Get(isolate, external_function, type);
  return redirection->address_of_instruction();
}

Redirection::Redirection(Isolate* isolate, void* external_function,
                         ExternalReference::Type type)
    : external_function_(external_function), type_(type), next_(nullptr) {
  next_ = Simulator::redirection();
  Simulator::SetRedirectInstruction(
      reinterpret_cast<Instruction*>(address_of_instruction()));
  Simulator::FlushICache(isolate->simulator_i_cache(),
                         reinterpret_cast<void*>(&instruction_),
                         sizeof(instruction_));
  Simulator::set_redirection(this);
#if ABI_USES_FUNCTION_DESCRIPTORS
  function_descriptor_[0] = reinterpret_cast<intptr_t>(&instruction_);
  function_descriptor_[1] = 0;
  function_descriptor_[2] = 0;
#endif
}

// static
Redirection* Redirection::Get(Isolate* isolate, void* external_function,
                              ExternalReference::Type type) {
  Redirection* current = Simulator::redirection();
  for (; current != nullptr; current = current->next_) {
    if (current->external_function_ == external_function &&
        current->type_ == type) {
      return current;
    }
  }
  return new Redirection(isolate, external_function, type);
}

}  // namespace internal
}  // namespace v8

#endif  // defined(USE_SIMULATOR)
