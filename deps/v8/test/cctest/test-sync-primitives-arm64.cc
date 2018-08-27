// Copyright 2017 the V8 project authors. All rights reserved.
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

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/arm64/simulator-arm64.h"
#include "src/heap/factory.h"
#include "src/macro-assembler-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// These tests rely on the behaviour specific to the simulator so we cannot
// expect the same results on real hardware. The reason for this is that our
// simulation of synchronisation primitives is more conservative than the
// reality.
// For example:
//   ldxr x1, [x2] ; Load acquire at address x2; x2 is now marked as exclusive.
//   ldr x0, [x4]  ; This is a normal load, and at a different address.
//                 ; However, any memory accesses can potentially clear the
//                 ; exclusivity (See ARM DDI 0487B.a B2.9.5). This is unlikely
//                 ; on real hardware but to be conservative, the simulator
//                 ; always does it.
//   stxr w3, x1, [x2] ; As a result, this will always fail in the simulator but
//                     ; will likely succeed on hardware.
#if defined(USE_SIMULATOR)

#ifndef V8_TARGET_LITTLE_ENDIAN
#error Expected ARM to be little-endian
#endif

#define __ masm.

struct MemoryAccess {
  enum class Kind {
    None,
    Load,
    LoadExcl,
    Store,
    StoreExcl,
  };

  enum class Size {
    Byte,
    HalfWord,
    Word,
  };

  MemoryAccess() : kind(Kind::None) {}
  MemoryAccess(Kind kind, Size size, size_t offset, int value = 0)
      : kind(kind), size(size), offset(offset), value(value) {}

  Kind kind = Kind::None;
  Size size = Size::Byte;
  size_t offset = 0;
  int value = 0;
};

struct TestData {
  explicit TestData(int w) : w(w) {}

  union {
    int32_t w;
    int16_t h;
    int8_t b;
  };
  int dummy;
};

namespace {

void AssembleMemoryAccess(MacroAssembler* assembler, MemoryAccess access,
                          Register dest_reg, Register value_reg,
                          Register addr_reg) {
  MacroAssembler& masm = *assembler;
  __ Add(addr_reg, x0, Operand(access.offset));

  switch (access.kind) {
    case MemoryAccess::Kind::None:
      break;

    case MemoryAccess::Kind::Load:
      switch (access.size) {
        case MemoryAccess::Size::Byte:
          __ ldrb(value_reg, MemOperand(addr_reg));
          break;

        case MemoryAccess::Size::HalfWord:
          __ ldrh(value_reg, MemOperand(addr_reg));
          break;

        case MemoryAccess::Size::Word:
          __ ldr(value_reg, MemOperand(addr_reg));
          break;
      }
      break;

    case MemoryAccess::Kind::LoadExcl:
      switch (access.size) {
        case MemoryAccess::Size::Byte:
          __ ldaxrb(value_reg, addr_reg);
          break;

        case MemoryAccess::Size::HalfWord:
          __ ldaxrh(value_reg, addr_reg);
          break;

        case MemoryAccess::Size::Word:
          __ ldaxr(value_reg, addr_reg);
          break;
      }
      break;

    case MemoryAccess::Kind::Store:
      switch (access.size) {
        case MemoryAccess::Size::Byte:
          __ Mov(value_reg, Operand(access.value));
          __ strb(value_reg, MemOperand(addr_reg));
          break;

        case MemoryAccess::Size::HalfWord:
          __ Mov(value_reg, Operand(access.value));
          __ strh(value_reg, MemOperand(addr_reg));
          break;

        case MemoryAccess::Size::Word:
          __ Mov(value_reg, Operand(access.value));
          __ str(value_reg, MemOperand(addr_reg));
          break;
      }
      break;

    case MemoryAccess::Kind::StoreExcl:
      switch (access.size) {
        case MemoryAccess::Size::Byte:
          __ Mov(value_reg, Operand(access.value));
          __ stlxrb(dest_reg, value_reg, addr_reg);
          break;

        case MemoryAccess::Size::HalfWord:
          __ Mov(value_reg, Operand(access.value));
          __ stlxrh(dest_reg, value_reg, addr_reg);
          break;

        case MemoryAccess::Size::Word:
          __ Mov(value_reg, Operand(access.value));
          __ stlxr(dest_reg, value_reg, addr_reg);
          break;
      }
      break;
  }
}

void AssembleLoadExcl(MacroAssembler* assembler, MemoryAccess access,
                      Register value_reg, Register addr_reg) {
  DCHECK(access.kind == MemoryAccess::Kind::LoadExcl);
  AssembleMemoryAccess(assembler, access, no_reg, value_reg, addr_reg);
}

void AssembleStoreExcl(MacroAssembler* assembler, MemoryAccess access,
                       Register dest_reg, Register value_reg,
                       Register addr_reg) {
  DCHECK(access.kind == MemoryAccess::Kind::StoreExcl);
  AssembleMemoryAccess(assembler, access, dest_reg, value_reg, addr_reg);
}

void TestInvalidateExclusiveAccess(TestData initial_data, MemoryAccess access1,
                                   MemoryAccess access2, MemoryAccess access3,
                                   int expected_res, TestData expected_data) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler masm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);

  AssembleLoadExcl(&masm, access1, w1, x1);
  AssembleMemoryAccess(&masm, access2, w3, w2, x1);
  AssembleStoreExcl(&masm, access3, w0, w3, x1);
  __ br(lr);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  TestData t = initial_data;
  Simulator::current(isolate)->Call<void>(code->entry(), &t);
  int res = Simulator::current(isolate)->wreg(0);

  CHECK_EQ(expected_res, res);
  switch (access3.size) {
    case MemoryAccess::Size::Byte:
      CHECK_EQ(expected_data.b, t.b);
      break;

    case MemoryAccess::Size::HalfWord:
      CHECK_EQ(expected_data.h, t.h);
      break;

    case MemoryAccess::Size::Word:
      CHECK_EQ(expected_data.w, t.w);
      break;
  }
}

}  // namespace

TEST(simulator_invalidate_exclusive_access) {
  using Kind = MemoryAccess::Kind;
  using Size = MemoryAccess::Size;

  MemoryAccess ldaxr_w(Kind::LoadExcl, Size::Word, offsetof(TestData, w));
  MemoryAccess stlxr_w(Kind::StoreExcl, Size::Word, offsetof(TestData, w), 7);

  // Address mismatch.
  TestInvalidateExclusiveAccess(
      TestData(1), ldaxr_w,
      MemoryAccess(Kind::LoadExcl, Size::Word, offsetof(TestData, dummy)),
      stlxr_w, 1, TestData(1));

  // Size mismatch.
  TestInvalidateExclusiveAccess(
      TestData(1), ldaxr_w, MemoryAccess(),
      MemoryAccess(Kind::StoreExcl, Size::HalfWord, offsetof(TestData, w), 7),
      1, TestData(1));

  // Load between ldaxr/stlxr.
  TestInvalidateExclusiveAccess(
      TestData(1), ldaxr_w,
      MemoryAccess(Kind::Load, Size::Word, offsetof(TestData, dummy)), stlxr_w,
      1, TestData(1));

  // Store between ldaxr/stlxr.
  TestInvalidateExclusiveAccess(
      TestData(1), ldaxr_w,
      MemoryAccess(Kind::Store, Size::Word, offsetof(TestData, dummy)), stlxr_w,
      1, TestData(1));

  // Match
  TestInvalidateExclusiveAccess(TestData(1), ldaxr_w, MemoryAccess(), stlxr_w,
                                0, TestData(7));
}

namespace {

int ExecuteMemoryAccess(Isolate* isolate, TestData* test_data,
                        MemoryAccess access) {
  HandleScope scope(isolate);
  MacroAssembler masm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);
  AssembleMemoryAccess(&masm, access, w0, w2, x1);
  __ br(lr);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
  Simulator::current(isolate)->Call<void>(code->entry(), test_data);
  return Simulator::current(isolate)->wreg(0);
}

}  // namespace

class MemoryAccessThread : public v8::base::Thread {
 public:
  MemoryAccessThread()
      : Thread(Options("MemoryAccessThread")),
        test_data_(nullptr),
        is_finished_(false),
        has_request_(false),
        did_request_(false),
        isolate_(nullptr) {}

  virtual void Run() {
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    isolate_ = v8::Isolate::New(create_params);
    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate_);
    {
      v8::Isolate::Scope scope(isolate_);
      v8::base::LockGuard<v8::base::Mutex> lock_guard(&mutex_);
      while (!is_finished_) {
        while (!(has_request_ || is_finished_)) {
          has_request_cv_.Wait(&mutex_);
        }

        if (is_finished_) {
          break;
        }

        ExecuteMemoryAccess(i_isolate, test_data_, access_);
        has_request_ = false;
        did_request_ = true;
        did_request_cv_.NotifyOne();
      }
    }
    isolate_->Dispose();
  }

  void NextAndWait(TestData* test_data, MemoryAccess access) {
    DCHECK(!has_request_);
    v8::base::LockGuard<v8::base::Mutex> lock_guard(&mutex_);
    test_data_ = test_data;
    access_ = access;
    has_request_ = true;
    has_request_cv_.NotifyOne();
    while (!did_request_) {
      did_request_cv_.Wait(&mutex_);
    }
    did_request_ = false;
  }

  void Finish() {
    v8::base::LockGuard<v8::base::Mutex> lock_guard(&mutex_);
    is_finished_ = true;
    has_request_cv_.NotifyOne();
  }

 private:
  TestData* test_data_;
  MemoryAccess access_;
  bool is_finished_;
  bool has_request_;
  bool did_request_;
  v8::base::Mutex mutex_;
  v8::base::ConditionVariable has_request_cv_;
  v8::base::ConditionVariable did_request_cv_;
  v8::Isolate* isolate_;
};

TEST(simulator_invalidate_exclusive_access_threaded) {
  using Kind = MemoryAccess::Kind;
  using Size = MemoryAccess::Size;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  TestData test_data(1);

  MemoryAccessThread thread;
  thread.Start();

  MemoryAccess ldaxr_w(Kind::LoadExcl, Size::Word, offsetof(TestData, w));
  MemoryAccess stlxr_w(Kind::StoreExcl, Size::Word, offsetof(TestData, w), 7);

  // Exclusive store completed by another thread first.
  test_data = TestData(1);
  thread.NextAndWait(&test_data, MemoryAccess(Kind::LoadExcl, Size::Word,
                                              offsetof(TestData, w)));
  ExecuteMemoryAccess(isolate, &test_data, ldaxr_w);
  thread.NextAndWait(&test_data, MemoryAccess(Kind::StoreExcl, Size::Word,
                                              offsetof(TestData, w), 5));
  CHECK_EQ(1, ExecuteMemoryAccess(isolate, &test_data, stlxr_w));
  CHECK_EQ(5, test_data.w);

  // Exclusive store completed by another thread; different address, but masked
  // to same
  test_data = TestData(1);
  ExecuteMemoryAccess(isolate, &test_data, ldaxr_w);
  thread.NextAndWait(&test_data, MemoryAccess(Kind::LoadExcl, Size::Word,
                                              offsetof(TestData, dummy)));
  thread.NextAndWait(&test_data, MemoryAccess(Kind::StoreExcl, Size::Word,
                                              offsetof(TestData, dummy), 5));
  CHECK_EQ(1, ExecuteMemoryAccess(isolate, &test_data, stlxr_w));
  CHECK_EQ(1, test_data.w);

  // Test failure when store between ldaxr/stlxr.
  test_data = TestData(1);
  ExecuteMemoryAccess(isolate, &test_data, ldaxr_w);
  thread.NextAndWait(&test_data, MemoryAccess(Kind::Store, Size::Word,
                                              offsetof(TestData, dummy)));
  CHECK_EQ(1, ExecuteMemoryAccess(isolate, &test_data, stlxr_w));
  CHECK_EQ(1, test_data.w);

  thread.Finish();
  thread.Join();
}

#undef __

#endif  // USE_SIMULATOR

}  // namespace internal
}  // namespace v8
