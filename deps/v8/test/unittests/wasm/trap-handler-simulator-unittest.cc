// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/trap-handler/trap-handler-simulator.h"

#include <cstdint>
#include <optional>

#include "include/v8-initialization.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/execution/simulator.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/allocation.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"

#ifdef V8_TRAP_HANDLER_VIA_SIMULATOR

namespace v8::internal::trap_handler {

constexpr uintptr_t kFakePc = 11;

class SimulatorTrapHandlerTest : public TestWithPlatform {
 public:
  SimulatorTrapHandlerTest()
      : array_buffer_allocator_(
            v8::ArrayBuffer::Allocator::NewDefaultAllocator()) {
    // Set up the trap handler before allocating an isolate. Isolate allocation
    // registers the `WasmNull` unaccessible range with the trap handler *only*
    // if the trap handler is enabled at this point.
    constexpr bool kUseDefaultHandler = true;
    CHECK(v8::V8::EnableWebAssemblyTrapHandler(kUseDefaultHandler));

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = array_buffer_allocator_;
    isolate_ = v8::Isolate::New(create_params);
    CHECK_NOT_NULL(isolate_);

    isolate_scope_.emplace(isolate_);
    handle_scope_.emplace(isolate_);
  }

  ~SimulatorTrapHandlerTest() override {
    if (inaccessible_memory_) {
      auto* page_allocator = GetArrayBufferPageAllocator();
      size_t page_size = page_allocator->AllocatePageSize();
      UnregisterCoveredMemory(reinterpret_cast<uintptr_t>(inaccessible_memory_),
                              page_size);
      CHECK(page_allocator->FreePages(inaccessible_memory_, page_size));
    }
    handle_scope_.reset();
    isolate_scope_.reset();
    isolate_->Dispose();
    delete array_buffer_allocator_;

    // Clean up the trap handler.
    RemoveTrapHandler();
  }

  v8::Isolate* isolate() const { return isolate_; }
  i::Isolate* i_isolate() const {
    return reinterpret_cast<i::Isolate*>(isolate_);
  }

  uintptr_t InaccessibleMemoryPtr() {
    if (!inaccessible_memory_) {
      auto* page_allocator = GetArrayBufferPageAllocator();
      size_t page_size = page_allocator->AllocatePageSize();
      inaccessible_memory_ =
          reinterpret_cast<uint8_t*>(page_allocator->AllocatePages(
              nullptr, /* size */ page_size, /* align */ page_size,
              PageAllocator::kNoAccess));
      CHECK_NOT_NULL(inaccessible_memory_);
      RegisterCoveredMemory(reinterpret_cast<uintptr_t>(inaccessible_memory_),
                            page_size);
    }
    return reinterpret_cast<uintptr_t>(inaccessible_memory_);
  }

 private:
  v8::ArrayBuffer::Allocator* array_buffer_allocator_;
  v8::Isolate* isolate_;
  std::optional<v8::Isolate::Scope> isolate_scope_;
  std::optional<v8::HandleScope> handle_scope_;
  uint8_t* inaccessible_memory_ = nullptr;
};

TEST_F(SimulatorTrapHandlerTest, ProbeMemorySuccess) {
  int x = 47;
  EXPECT_EQ(0u, ProbeMemory(reinterpret_cast<uintptr_t>(&x), kFakePc));
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryFailNullptr) {
  constexpr uintptr_t kNullAddress = 0;
  EXPECT_DEATH_IF_SUPPORTED(ProbeMemory(kNullAddress, kFakePc), "");
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryFailInaccessible) {
  EXPECT_DEATH_IF_SUPPORTED(ProbeMemory(InaccessibleMemoryPtr(), kFakePc), "");
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryFailWhileInWasm) {
  // Test that we still crash if the trap handler is set up, but the PC is not
  // registered as a trapping instruction.
  EXPECT_DEATH_IF_SUPPORTED(ProbeMemory(InaccessibleMemoryPtr(), kFakePc), "");
}

namespace {
uintptr_t v8_landing_pad() {
  return Builtins::EmbeddedEntryOf(Builtin::kWasmTrapHandlerLandingPad);
}
}  // namespace

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryWithTrapHandled) {
  TrappingInstructionData fake_protected_instruction{kFakePc};
  int handler_data_index =
      RegisterHandlerData(0, 128, 1, &fake_protected_instruction);

  EXPECT_EQ(v8_landing_pad(), ProbeMemory(InaccessibleMemoryPtr(), kFakePc));

  // Reset everything.
  ReleaseHandlerData(handler_data_index);
}

class SimulatorTrapHandlerTestWithCodegen : public SimulatorTrapHandlerTest {
 protected:
  void GenerateAndExecuteCode() {
    EXPECT_EQ(0u, GetRecoveredTrapCount());
    CodeDesc desc;
    masm_.GetCode(static_cast<LocalIsolate*>(nullptr), &desc);

    TrappingInstructionData protected_instruction{crash_offset_};
    int handler_data_index =
        RegisterHandlerData(reinterpret_cast<Address>(desc.buffer),
                            desc.instr_size, 1, &protected_instruction);

    buffer_->MakeExecutable();
    GeneratedCode<void> code = GeneratedCode<void>::FromAddress(
        i_isolate(), reinterpret_cast<Address>(desc.buffer));

    SetLandingPad(reinterpret_cast<uintptr_t>(buffer_->start()) +
                  recovery_offset_);
    code.Call();

    ReleaseHandlerData(handler_data_index);
    SetLandingPad(0);

    EXPECT_EQ(1u, GetRecoveredTrapCount());
  }

  std::unique_ptr<TestingAssemblerBuffer> buffer_{AllocateAssemblerBuffer()};
  MacroAssembler masm_{i_isolate(), AssemblerOptions{}, CodeObjectRequired::kNo,
                       buffer_->CreateView()};
  uint32_t crash_offset_{0};
  uint32_t recovery_offset_{0};
};

#define __ masm_.

TEST_F(SimulatorTrapHandlerTestWithCodegen, ProbeMemoryWithLandingPad) {
#ifdef V8_TARGET_ARCH_ARM64
  constexpr Register scratch = x0;
  // Generate an illegal memory access.
  __ Mov(scratch, InaccessibleMemoryPtr());
  crash_offset_ = __ pc_offset();
  __ Str(scratch, MemOperand(scratch, 0));  // store to inaccessible memory.
  recovery_offset_ = __ pc_offset();
  __ Ret();

  GenerateAndExecuteCode();
#elif V8_TARGET_ARCH_LOONG64
  constexpr Register scratch = a0;
  // Generate an illegal memory access.
  __ li(scratch, static_cast<int64_t>(InaccessibleMemoryPtr()));
  crash_offset_ = __ pc_offset();
  __ St_d(scratch, MemOperand(scratch, 0));  // store to inaccessible memory.
  recovery_offset_ = __ pc_offset();
  __ Ret();

  GenerateAndExecuteCode();
#elif V8_TARGET_ARCH_RISCV64
  constexpr Register scratch = a0;
  // Generate an illegal memory access.
  __ li(scratch, static_cast<int64_t>(InaccessibleMemoryPtr()));
  crash_offset_ = __ pc_offset();
  __ StoreWord(scratch,
               MemOperand(scratch, 0));  // store to inaccessible memory.
  recovery_offset_ = __ pc_offset();
  __ Ret();

  GenerateAndExecuteCode();
#else
#error Unsupported platform
#endif
}

TEST_F(SimulatorTrapHandlerTestWithCodegen, ProbeMemory_MultiStruct) {
#ifdef V8_TARGET_ARCH_ARM64
  constexpr VRegister scratch = v0;
  constexpr Register addr = x0;
  // Generate an illegal memory access.
  __ Mov(addr, InaccessibleMemoryPtr());
  crash_offset_ = __ pc_offset();
  __ Ld1(scratch.V16B(), MemOperand(addr, 0));
  recovery_offset_ = __ pc_offset();
  __ Ret();

  GenerateAndExecuteCode();
#elif V8_TARGET_ARCH_LOONG64
  // TODO(loong64): Implement ProbeMemory_MultiStruct test when wasm-simd is
  // supported.
#elif V8_TARGET_ARCH_RISCV64
  constexpr Register addr = a0;
  constexpr VRegister scratch = v1;
  // Generate an illegal memory access.
  __ li(addr, InaccessibleMemoryPtr());
  __ VU.set(1, E16, m1);
  crash_offset_ = __ pc_offset();
  __ vl(scratch, addr, 0, VSew::E16);
  recovery_offset_ = __ pc_offset();
  __ Ret();

  GenerateAndExecuteCode();
#else
#error Unsupported platform
#endif
}

TEST_F(SimulatorTrapHandlerTestWithCodegen, ProbeMemory_LoadStorePair) {
#ifdef V8_TARGET_ARCH_ARM64
  constexpr Register scratch_0 = x0;
  constexpr Register scratch_1 = x1;
  constexpr Register addr = x2;
  // Generate an illegal memory access.
  __ Mov(addr, InaccessibleMemoryPtr());
  crash_offset_ = __ pc_offset();
  __ Ldp(scratch_0, scratch_1, MemOperand(addr, 0));
  recovery_offset_ = __ pc_offset();
  __ Ret();

  GenerateAndExecuteCode();
#elif V8_TARGET_ARCH_LOONG64
  // Loong64 doesn't have a LoadStorePair instruction.
#elif V8_TARGET_ARCH_RISCV64 || V8_TARGET_ARCH_RISCV32
  // RISCV64 and RISCV32 don't have a LoadStorePair instruction.
#else
#error Unsupported platform
#endif
}

#undef __

}  // namespace v8::internal::trap_handler

#endif  // V8_TRAP_HANDLER_VIA_SIMULATOR
