// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_SIMULATOR_HELPER_H_
#define V8_TEST_COMMON_SIMULATOR_HELPER_H_

#include "include/v8-isolate.h"
#include "include/v8-unwinder.h"
#include "src/execution/isolate.h"
#include "src/execution/simulator.h"

#if defined(USE_SIMULATOR)
class SimulatorHelper {
 public:
  inline bool Init(v8::Isolate* isolate) {
    simulator_ = reinterpret_cast<v8::internal::Isolate*>(isolate)
                     ->thread_local_top()
                     ->simulator_;
    // Check if there is active simulator.
    return simulator_ != nullptr;
  }

  inline void FillRegisters(v8::RegisterState* state) {
#if V8_TARGET_ARCH_ARM
    state->pc = reinterpret_cast<void*>(simulator_->get_pc());
    state->sp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::sp));
    state->fp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::r11));
    state->lr = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::lr));
#elif V8_TARGET_ARCH_ARM64
    if (simulator_->sp() == 0 || simulator_->fp() == 0) {
      // It's possible that the simulator is interrupted while it is updating
      // the sp or fp register. ARM64 simulator does this in two steps:
      // first setting it to zero and then setting it to a new value.
      // Bailout if sp/fp doesn't contain the new value.
      return;
    }
    state->pc = reinterpret_cast<void*>(simulator_->pc());
    state->sp = reinterpret_cast<void*>(simulator_->sp());
    state->fp = reinterpret_cast<void*>(simulator_->fp());
    state->lr = reinterpret_cast<void*>(simulator_->lr());
#elif V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_LOONG64
    state->pc = reinterpret_cast<void*>(simulator_->get_pc());
    state->sp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::sp));
    state->fp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::fp));
#elif V8_TARGET_ARCH_RISCV64 || V8_TARGET_ARCH_RISCV32
    state->pc = reinterpret_cast<void*>(simulator_->get_pc());
    state->sp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::sp));
    state->fp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::fp));
    state->lr = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::ra));
#elif V8_TARGET_ARCH_PPC64
    state->pc = reinterpret_cast<void*>(simulator_->get_pc());
    state->sp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::sp));
    state->fp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::fp));
    state->lr = reinterpret_cast<void*>(simulator_->get_lr());
#elif V8_TARGET_ARCH_S390X
    state->pc = reinterpret_cast<void*>(simulator_->get_pc());
    state->sp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::sp));
    state->fp = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::fp));
    state->lr = reinterpret_cast<void*>(
        simulator_->get_register(v8::internal::Simulator::ra));
#endif
  }

 private:
  v8::internal::Simulator* simulator_;
};
#endif  // USE_SIMULATOR

#endif  // V8_TEST_COMMON_SIMULATOR_HELPER_H_
