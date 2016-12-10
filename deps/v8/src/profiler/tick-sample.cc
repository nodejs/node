// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/tick-sample.h"

#include "include/v8-profiler.h"
#include "src/frames-inl.h"
#include "src/msan.h"
#include "src/simulator.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace {

bool IsSamePage(i::byte* ptr1, i::byte* ptr2) {
  const uint32_t kPageSize = 4096;
  uintptr_t mask = ~static_cast<uintptr_t>(kPageSize - 1);
  return (reinterpret_cast<uintptr_t>(ptr1) & mask) ==
         (reinterpret_cast<uintptr_t>(ptr2) & mask);
}

// Check if the code at specified address could potentially be a
// frame setup code.
bool IsNoFrameRegion(i::Address address) {
  struct Pattern {
    int bytes_count;
    i::byte bytes[8];
    int offsets[4];
  };
  i::byte* pc = reinterpret_cast<i::byte*>(address);
  static Pattern patterns[] = {
#if V8_HOST_ARCH_IA32
    // push %ebp
    // mov %esp,%ebp
    {3, {0x55, 0x89, 0xe5}, {0, 1, -1}},
    // pop %ebp
    // ret N
    {2, {0x5d, 0xc2}, {0, 1, -1}},
    // pop %ebp
    // ret
    {2, {0x5d, 0xc3}, {0, 1, -1}},
#elif V8_HOST_ARCH_X64
    // pushq %rbp
    // movq %rsp,%rbp
    {4, {0x55, 0x48, 0x89, 0xe5}, {0, 1, -1}},
    // popq %rbp
    // ret N
    {2, {0x5d, 0xc2}, {0, 1, -1}},
    // popq %rbp
    // ret
    {2, {0x5d, 0xc3}, {0, 1, -1}},
#endif
    {0, {}, {}}
  };
  for (Pattern* pattern = patterns; pattern->bytes_count; ++pattern) {
    for (int* offset_ptr = pattern->offsets; *offset_ptr != -1; ++offset_ptr) {
      int offset = *offset_ptr;
      if (!offset || IsSamePage(pc, pc - offset)) {
        MSAN_MEMORY_IS_INITIALIZED(pc - offset, pattern->bytes_count);
        if (!memcmp(pc - offset, pattern->bytes, pattern->bytes_count))
          return true;
      } else {
        // It is not safe to examine bytes on another page as it might not be
        // allocated thus causing a SEGFAULT.
        // Check the pattern part that's on the same page and
        // pessimistically assume it could be the entire pattern match.
        MSAN_MEMORY_IS_INITIALIZED(pc, pattern->bytes_count - offset);
        if (!memcmp(pc, pattern->bytes + offset, pattern->bytes_count - offset))
          return true;
      }
    }
  }
  return false;
}

}  // namespace

namespace internal {
namespace {

#if defined(USE_SIMULATOR)
class SimulatorHelper {
 public:
  // Returns true if register values were successfully retrieved
  // from the simulator, otherwise returns false.
  static bool FillRegisters(Isolate* isolate, v8::RegisterState* state);
};

bool SimulatorHelper::FillRegisters(Isolate* isolate,
                                    v8::RegisterState* state) {
  Simulator* simulator = isolate->thread_local_top()->simulator_;
  // Check if there is active simulator.
  if (simulator == NULL) return false;
#if V8_TARGET_ARCH_ARM
  if (!simulator->has_bad_pc()) {
    state->pc = reinterpret_cast<Address>(simulator->get_pc());
  }
  state->sp = reinterpret_cast<Address>(simulator->get_register(Simulator::sp));
  state->fp =
      reinterpret_cast<Address>(simulator->get_register(Simulator::r11));
#elif V8_TARGET_ARCH_ARM64
  state->pc = reinterpret_cast<Address>(simulator->pc());
  state->sp = reinterpret_cast<Address>(simulator->sp());
  state->fp = reinterpret_cast<Address>(simulator->fp());
#elif V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
  if (!simulator->has_bad_pc()) {
    state->pc = reinterpret_cast<Address>(simulator->get_pc());
  }
  state->sp = reinterpret_cast<Address>(simulator->get_register(Simulator::sp));
  state->fp = reinterpret_cast<Address>(simulator->get_register(Simulator::fp));
#elif V8_TARGET_ARCH_PPC
  if (!simulator->has_bad_pc()) {
    state->pc = reinterpret_cast<Address>(simulator->get_pc());
  }
  state->sp = reinterpret_cast<Address>(simulator->get_register(Simulator::sp));
  state->fp = reinterpret_cast<Address>(simulator->get_register(Simulator::fp));
#elif V8_TARGET_ARCH_S390
  if (!simulator->has_bad_pc()) {
    state->pc = reinterpret_cast<Address>(simulator->get_pc());
  }
  state->sp = reinterpret_cast<Address>(simulator->get_register(Simulator::sp));
  state->fp = reinterpret_cast<Address>(simulator->get_register(Simulator::fp));
#endif
  if (state->sp == 0 || state->fp == 0) {
    // It possible that the simulator is interrupted while it is updating
    // the sp or fp register. ARM64 simulator does this in two steps:
    // first setting it to zero and then setting it to the new value.
    // Bailout if sp/fp doesn't contain the new value.
    //
    // FIXME: The above doesn't really solve the issue.
    // If a 64-bit target is executed on a 32-bit host even the final
    // write is non-atomic, so it might obtain a half of the result.
    // Moreover as long as the register set code uses memcpy (as of now),
    // it is not guaranteed to be atomic even when both host and target
    // are of same bitness.
    return false;
  }
  return true;
}
#endif  // USE_SIMULATOR

}  // namespace
}  // namespace internal

//
// StackTracer implementation
//
DISABLE_ASAN void TickSample::Init(Isolate* v8_isolate,
                                   const RegisterState& reg_state,
                                   RecordCEntryFrame record_c_entry_frame,
                                   bool update_stats,
                                   bool use_simulator_reg_state) {
  this->update_stats = update_stats;
  SampleInfo info;
  RegisterState regs = reg_state;
  if (!GetStackSample(v8_isolate, &regs, record_c_entry_frame, stack,
                      kMaxFramesCount, &info, use_simulator_reg_state)) {
    // It is executing JS but failed to collect a stack trace.
    // Mark the sample as spoiled.
    pc = nullptr;
    return;
  }

  state = info.vm_state;
  pc = regs.pc;
  frames_count = static_cast<unsigned>(info.frames_count);
  has_external_callback = info.external_callback_entry != nullptr;
  if (has_external_callback) {
    external_callback_entry = info.external_callback_entry;
  } else if (frames_count) {
    // sp register may point at an arbitrary place in memory, make
    // sure MSAN doesn't complain about it.
    MSAN_MEMORY_IS_INITIALIZED(regs.sp, sizeof(void*));
    // Sample potential return address value for frameless invocation of
    // stubs (we'll figure out later, if this value makes sense).
    tos = i::Memory::Address_at(reinterpret_cast<i::Address>(regs.sp));
  } else {
    tos = nullptr;
  }
}

bool TickSample::GetStackSample(Isolate* v8_isolate, RegisterState* regs,
                                RecordCEntryFrame record_c_entry_frame,
                                void** frames, size_t frames_limit,
                                v8::SampleInfo* sample_info,
                                bool use_simulator_reg_state) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  sample_info->frames_count = 0;
  sample_info->vm_state = isolate->current_vm_state();
  sample_info->external_callback_entry = nullptr;
  if (sample_info->vm_state == GC) return true;

  i::Address js_entry_sp = isolate->js_entry_sp();
  if (js_entry_sp == nullptr) return true;  // Not executing JS now.

#if defined(USE_SIMULATOR)
  if (use_simulator_reg_state) {
    if (!i::SimulatorHelper::FillRegisters(isolate, regs)) return false;
  }
#else
  USE(use_simulator_reg_state);
#endif
  DCHECK(regs->sp);

  if (regs->pc && IsNoFrameRegion(static_cast<i::Address>(regs->pc))) {
    // The frame is not setup, so it'd be hard to iterate the stack. Bailout.
    return false;
  }

  i::ExternalCallbackScope* scope = isolate->external_callback_scope();
  i::Address handler = i::Isolate::handler(isolate->thread_local_top());
  // If there is a handler on top of the external callback scope then
  // we have already entrered JavaScript again and the external callback
  // is not the top function.
  if (scope && scope->scope_address() < handler) {
    i::Address* external_callback_entry_ptr =
        scope->callback_entrypoint_address();
    sample_info->external_callback_entry =
        external_callback_entry_ptr == nullptr ? nullptr
                                               : *external_callback_entry_ptr;
  }

  i::SafeStackFrameIterator it(isolate, reinterpret_cast<i::Address>(regs->fp),
                               reinterpret_cast<i::Address>(regs->sp),
                               js_entry_sp);

  // If at this point iterator does not see any frames,
  // is usually means something is wrong with the FP,
  // e.g. it is used as a general purpose register in the function.
  // Bailout.
  if (it.done()) return false;

  size_t i = 0;
  if (record_c_entry_frame == kIncludeCEntryFrame &&
      (it.top_frame_type() == internal::StackFrame::EXIT ||
       it.top_frame_type() == internal::StackFrame::BUILTIN_EXIT)) {
    frames[i++] = isolate->c_function();
  }
  for (; !it.done() && i < frames_limit; it.Advance()) {
    if (!it.frame()->is_interpreted()) {
      frames[i++] = it.frame()->pc();
      continue;
    }
    // For interpreted frames use the bytecode array pointer as the pc.
    i::InterpretedFrame* frame = static_cast<i::InterpretedFrame*>(it.frame());
    // Since the sampler can interrupt execution at any point the
    // bytecode_array might be garbage, so don't dereference it.
    i::Address bytecode_array =
        reinterpret_cast<i::Address>(frame->GetBytecodeArray()) -
        i::kHeapObjectTag;
    frames[i++] = bytecode_array + i::BytecodeArray::kHeaderSize +
                  frame->GetBytecodeOffset();
  }
  sample_info->frames_count = i;
  return true;
}

namespace internal {

void TickSample::Init(Isolate* isolate, const v8::RegisterState& state,
                      RecordCEntryFrame record_c_entry_frame, bool update_stats,
                      bool use_simulator_reg_state) {
  v8::TickSample::Init(reinterpret_cast<v8::Isolate*>(isolate), state,
                       record_c_entry_frame, update_stats,
                       use_simulator_reg_state);
  if (pc == nullptr) return;
  timestamp = base::TimeTicks::HighResolutionNow();
}

}  // namespace internal
}  // namespace v8
