#include "gtest/gtest.h"

#ifdef HAVE_FFI_FASTCALL

#include <vector>

#include "ffi/fastcall/jit_memory.h"
#include "ffi/fastcall/stub_emitter.h"
#include "node_test_fixture.h"

using node::ffi::fastcall::ArgClass;
using node::ffi::fastcall::EmitForwarder;
using node::ffi::fastcall::JitMemory;
using node::ffi::fastcall::ResultClass;

class StubEmitterTest : public NodeTestFixture {
 protected:
  void TearDown() override {
    JitMemory::Get(isolate_)->ResetForTesting();
    NodeTestFixture::TearDown();
  }
};

// Test target functions. Plain static so the optimizer doesn't merge
// them with anything else.
namespace {
int target_zero_args() { return 1234; }
int target_one_int(int a) { return a + 100; }
int target_two_int(int a, int b) { return a - b; }
int target_six_int(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}
int target_seven_int(int a, int b, int c, int d, int e, int f, int g) {
  return a + b + c + d + e + f + g;
}
double target_one_double(double a) { return a * 2.0; }
int target_int_double(int a, double b) {
  return a + static_cast<int>(b);
}
[[maybe_unused]] int target_fp_int(double a, int b) { return static_cast<int>(a) + b; }
[[maybe_unused]] int target_three_int(int a, int b, int c) { return a + b + c; }
[[maybe_unused]] int target_four_int(int a, int b, int c, int d) {
  return a + b + c + d;
}
}  // namespace

#if defined(__aarch64__) || defined(_M_ARM64) || \
    defined(__x86_64__) || defined(_M_X64)

TEST_F(StubEmitterTest, ZeroArgs) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_zero_args),
                            {},
                            ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(1234, fn(reinterpret_cast<void*>(0xdeadbeefULL)));
  JitMemory::Get(isolate_)->Free(*stub);
}

TEST_F(StubEmitterTest, OneIntArg) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_one_int),
                            {ArgClass::kGP},
                            ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver, int);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(142, fn(nullptr, 42));
  JitMemory::Get(isolate_)->Free(*stub);
}

TEST_F(StubEmitterTest, TwoIntArgs) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_two_int),
                            {ArgClass::kGP, ArgClass::kGP},
                            ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver, int, int);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(7, fn(nullptr, 10, 3));
  JitMemory::Get(isolate_)->Free(*stub);
}

TEST_F(StubEmitterTest, OneDoubleArg) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_one_double),
                            {ArgClass::kFP},
                            ResultClass::kFP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = double (*)(void* receiver, double);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_DOUBLE_EQ(6.28, fn(nullptr, 3.14));
  JitMemory::Get(isolate_)->Free(*stub);
}

TEST_F(StubEmitterTest, MixedIntDouble) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_int_double),
                            {ArgClass::kGP, ArgClass::kFP},
                            ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver, int, double);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(13, fn(nullptr, 10, 3.7));
  JitMemory::Get(isolate_)->Free(*stub);
}

// SixIntArgs: passes on AArch64 (cap=7) and SysV x86_64 (cap=6), but fails
// on Win64 (combined GP+FP cap=3). Gate it out on Win64.
#if defined(__aarch64__) || defined(_M_ARM64) || \
    (defined(__x86_64__) && !defined(_WIN32))
TEST_F(StubEmitterTest, SixIntArgs) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_six_int),
                            std::vector<ArgClass>(6, ArgClass::kGP),
                            ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver, int, int, int, int, int, int);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(21, fn(nullptr, 1, 2, 3, 4, 5, 6));
  JitMemory::Get(isolate_)->Free(*stub);
}
#endif  // AArch64 || (x86_64 && !Win64)

// SevenIntArgs: only AArch64 supports 7 GP args (cap=7). SysV cap is 6,
// Win64 cap is 3 combined — both would reject 7 GP args.
#if defined(__aarch64__) || defined(_M_ARM64)
TEST_F(StubEmitterTest, SevenIntArgs) {
  // AArch64 supports up to 7 GP args after the receiver (kMaxGPArgs=7).
  auto stub = EmitForwarder(
      isolate_, reinterpret_cast<void*>(target_seven_int),
      std::vector<ArgClass>(7, ArgClass::kGP), ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver, int, int, int, int, int, int, int);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(28, fn(nullptr, 1, 2, 3, 4, 5, 6, 7));
  JitMemory::Get(isolate_)->Free(*stub);
}
#endif  // AArch64

// SysV (Linux/macOS/FreeBSD x86_64) cap is exactly 6 GP args. 7 must fail.
#if defined(__x86_64__) && \
    (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__))
TEST_F(StubEmitterTest, SevenGPArgsRejectedSysV) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_seven_int),
                            std::vector<ArgClass>(7, ArgClass::kGP),
                            ResultClass::kGP);
  EXPECT_FALSE(stub.has_value());
}
#endif  // SysV x86_64

TEST_F(StubEmitterTest, EightIntArgsRejected) {
  // 8 GP args would require stack handling not supported in v1.
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_seven_int),
                            std::vector<ArgClass>(8, ArgClass::kGP),
                            ResultClass::kGP);
  EXPECT_FALSE(stub.has_value());
}

TEST_F(StubEmitterTest, GPPairRejected) {
  // kGPPair is for AArch32 only; reject on AArch64/x86_64.
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_one_int),
                            {ArgClass::kGPPair},
                            ResultClass::kGP);
  EXPECT_FALSE(stub.has_value());
}

#if defined(__aarch64__) || defined(_M_ARM64)
TEST_F(StubEmitterTest, NineFPArgsRejected) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_one_double),
                            std::vector<ArgClass>(9, ArgClass::kFP),
                            ResultClass::kGP);
  EXPECT_FALSE(stub.has_value());
}
#endif  // __aarch64__

#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
TEST_F(StubEmitterTest, Win64MixedGPThenFP) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_int_double),
                            {ArgClass::kGP, ArgClass::kFP},
                            ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver, int, double);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(13, fn(nullptr, 10, 3.7));
  JitMemory::Get(isolate_)->Free(*stub);
}

TEST_F(StubEmitterTest, Win64FPThenGP) {
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_fp_int),
                            {ArgClass::kFP, ArgClass::kGP},
                            ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver, double, int);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(13, fn(nullptr, 3.7, 10));
  JitMemory::Get(isolate_)->Free(*stub);
}

TEST_F(StubEmitterTest, Win64ThreeArgsAccepted) {
  // 3 args is the Win64 cap (4 register slots minus the receiver).
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_three_int),
                            std::vector<ArgClass>(3, ArgClass::kGP),
                            ResultClass::kGP);
  ASSERT_TRUE(stub.has_value());
  using FnPtr = int (*)(void* receiver, int, int, int);
  FnPtr fn = reinterpret_cast<FnPtr>(stub->entry);
  EXPECT_EQ(6, fn(nullptr, 1, 2, 3));
  JitMemory::Get(isolate_)->Free(*stub);
}

TEST_F(StubEmitterTest, Win64FourArgsRejected) {
  // 4 args exceeds the Win64 register-slot cap.
  auto stub = EmitForwarder(isolate_,
                            reinterpret_cast<void*>(target_four_int),
                            std::vector<ArgClass>(4, ArgClass::kGP),
                            ResultClass::kGP);
  EXPECT_FALSE(stub.has_value());
}
#endif  // _WIN32 && x86_64

#endif  // __aarch64__ || x86_64

#endif  // HAVE_FFI_FASTCALL
