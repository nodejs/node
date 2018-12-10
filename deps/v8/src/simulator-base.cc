// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/simulator-base.h"

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
base::Mutex* SimulatorBase::i_cache_mutex_ = nullptr;

// static
base::CustomMatcherHashMap* SimulatorBase::i_cache_ = nullptr;

// static
void SimulatorBase::InitializeOncePerProcess() {
  DCHECK_NULL(redirection_mutex_);
  redirection_mutex_ = new base::Mutex();

  DCHECK_NULL(i_cache_mutex_);
  i_cache_mutex_ = new base::Mutex();

  DCHECK_NULL(i_cache_);
  i_cache_ = new base::CustomMatcherHashMap(&Simulator::ICacheMatch);
}

// static
void SimulatorBase::GlobalTearDown() {
  delete redirection_mutex_;
  redirection_mutex_ = nullptr;

  Redirection::DeleteChain(redirection_);
  redirection_ = nullptr;

  delete i_cache_mutex_;
  i_cache_mutex_ = nullptr;

  if (i_cache_ != nullptr) {
    for (base::HashMap::Entry* entry = i_cache_->Start(); entry != nullptr;
         entry = i_cache_->Next(entry)) {
      delete static_cast<CachePage*>(entry->value);
    }
  }
  delete i_cache_;
  i_cache_ = nullptr;
}

// static
Address SimulatorBase::RedirectExternalReference(Address external_function,
                                                 ExternalReference::Type type) {
  base::MutexGuard lock_guard(Simulator::redirection_mutex());
  Redirection* redirection = Redirection::Get(external_function, type);
  return redirection->address_of_instruction();
}

Redirection::Redirection(Address external_function,
                         ExternalReference::Type type)
    : external_function_(external_function), type_(type), next_(nullptr) {
  next_ = Simulator::redirection();
  base::MutexGuard lock_guard(Simulator::i_cache_mutex());
  Simulator::SetRedirectInstruction(
      reinterpret_cast<Instruction*>(address_of_instruction()));
  Simulator::FlushICache(Simulator::i_cache(),
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
Redirection* Redirection::Get(Address external_function,
                              ExternalReference::Type type) {
  Redirection* current = Simulator::redirection();
  for (; current != nullptr; current = current->next_) {
    if (current->external_function_ == external_function &&
        current->type_ == type) {
      return current;
    }
  }
  return new Redirection(external_function, type);
}

}  // namespace internal
}  // namespace v8

#endif  // defined(USE_SIMULATOR)
